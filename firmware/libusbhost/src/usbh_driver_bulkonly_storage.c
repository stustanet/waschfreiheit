/*
 * Copyright (C) 2015 Amir Hammad <amir.hammad@hotmail.com>
 * Copyright (C) 2018 Daniel Frejek <daniel.frejek@stusta.net>
 *
 *
 * libusbhost is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include "usart_helpers.h"
#include "usbh_driver_bulkonly_storage.h"
#include "driver/usbh_device_driver.h"
#include "scsi_structures.h"

#include <stdint.h>
#include <string.h>

enum STATES {
	STATE_INACTIVE,     // This device is not connected
	STATE_INITIALIZED,  // Got all required information
	STATE_BEGIN_INIT,   // Begin initializing the device
	STATE_IDLE,         // Idle -> ready to process commands
	STATE_EXPECT_CSW,   // Expecting the CSW as next packet
	STATE_READ_DATA,    // Read operation in progress
	STATE_WRITE_DATA,   // Write operation in progress
	STATE_WAIT_CRTL,
	STATE_ERROR,
	STATE_RESET_MSR,
	STATE_RESET_CFH_IN,
	STATE_RESET_CFH_OUT
};

enum INIT_STATE
{
	INIT_STATE_DONE,               // Init done
	INIT_STATE_START,              // Begin init (no pending request)
	INIT_STATE_TEST_UNIT_READY,    // Check, if the unit is ready
	INIT_STATE_CAPACITY10,         // Read the capacity of the device using the 32 bit command
	INIT_STATE_CAPACITY16,         // Read the capacity of the device using the 64 bit command

    // A SENSE request needs to be sent after an error occured.
	// This can happen during init or during a normal data transfer.
	// If an error happens during the initialization, the init is restarted after the SENSE.
	INIT_STATE_SENSE_TXERR,        // SENSE after TXERR
	INIT_STATE_SENSE_INITERR       // SENSE after error during init
};

typedef struct _bulkonly_storage_device {
	usbh_device_t *usbh_device;

	// Buffer for CBW and CSW messages
	uint8_t cmd_buffer[USBH_BULKONLY_STORAGE_CMD_BUFFER];

	// Buffer for SCSI command responses (only used during initialization)
	uint8_t data_buffer[USBH_BULKONLY_STORAGE_DATA_BUFFER];
	uint64_t sector_count;
	void *data_ptr;
	const void *last_tx_data_ptr;
	uint32_t last_tx_data_size;
	uint32_t data_remaining_bytes;
	uint32_t sector_size;
	uint32_t last_tag;
	uint16_t endpoint_in_maxpacketsize;
	uint16_t endpoint_out_maxpacketsize;
	uint8_t endpoint_in_address;
	uint8_t endpoint_out_address;
	uint8_t endpoint_in_toggle;
	uint8_t endpoint_out_toggle;
	uint8_t state;
	uint8_t init_state; // Status of the SCSI initialization (ignored, if state is INACTIVE or INITIALIZED)
	uint8_t error_counter;

	uint8_t device_id;
	uint8_t configuration_value;
} bulkonly_storage_device_t;

static bulkonly_storage_device_t devices[USBH_BULKONLY_STORAGE_MAX_DEVICES];

typedef struct _bulkonly_cbw
{
	uint32_t signature;
	uint32_t tag;
	uint32_t dataTransferLength;
	uint8_t flags;
	uint8_t lun;
	uint8_t cbLength;
	uint8_t command[16];
} __attribute__((packed)) bulkonly_cbw_t;

typedef struct _bulkonly_csw
{
	uint32_t signature;
	uint32_t tag;
	uint32_t dataResidue;
	uint8_t status;
} __attribute__((packed)) bulkonly_csw_t;

#define BULKONLY_CBW_SIGNATURE          0x43425355
#define BULKONLY_CSW_SIGNATURE          0x53425355
#define BULKONLY_CBW_FLAG_DIRECTION           0x80
#define BULKONLY_CSW_STATUS_GOOD                 0
#define BULKONLY_CSW_STATUS_FAILED               1
#define BULKONLY_CSW_STATUS_PHASE_ERROR          2

_Static_assert(sizeof(bulkonly_cbw_t) == 31, "CBW must be exactly 31 bytes long");
_Static_assert(sizeof(bulkonly_csw_t) == 13, "CSW must be exactly 13 bytes long");


static bulkonly_storage_callbacks_t callbacks = {};

#define ASSERT(x) if (!(x)) assertFailed(#x)
static void assertFailed(const char *expr)
{
#ifndef LUSBH_USART_DEBUG
	(void)expr;
#endif
	LOG_PRINTF("ASSERT FAILED: \"%s\"", expr);
	(*((volatile int *)~0)) = 0;
}

static void num_to_unaligned_be(uint32_t num, void *ptr, uint8_t bytes)
{
	uint8_t *data = (uint8_t*)ptr;
	for (uint8_t i = 0; i < bytes; i++)
	{
		data[bytes - i - 1] = (uint8_t)num;
		num = num >> 8;
	}
}

static uint32_t unaligned_be_to_num(const void *ptr, uint8_t bytes)
{
	const uint8_t *data = (const uint8_t*)ptr;
	uint32_t num = 0;
	for (uint8_t i = 0; i < bytes; i++)
	{
		num = (num << 8) | data[i];
	}
	return num;
}

#if USBH_BULKONLY_STORAGE_SUPPORT_64_ADDRESS
static void num_to_unaligned_be64(uint64_t num, void *ptr)
{
	uint8_t *data = (uint8_t*)ptr;
	for (uint8_t i = 0; i < 8; i++)
	{
		data[7 - i] = (uint8_t)num;
		num = num >> 8;
	}
}

static uint64_t unaligned_be_to_num64(const void *ptr)
{
	const uint8_t *data = (const uint8_t*)ptr;
	uint64_t num = 0;
	for (uint8_t i = 0; i < 8; i++)
	{
		num = (num << 8) | data[i];
	}
	return num;
}
#endif

static void send_scsi_command(bulkonly_storage_device_t *dev, const void *cmd, uint8_t cmd_len, void *data_ptr, uint32_t data_length, uint8_t dir_write);
static void recv_packet(bulkonly_storage_device_t *dev, void *data, uint32_t len);
static void send_packet(bulkonly_storage_device_t *dev, const void *data, uint32_t len);
static void reset_recovery_step(bulkonly_storage_device_t *dev);
static void begin_reset_recovery(bulkonly_storage_device_t *dev);
static void receive_csw(bulkonly_storage_device_t *dev);
static void handle_csw(bulkonly_storage_device_t *dev, uint32_t len);

#ifdef LUSBH_USART_DEBUG
static void hexdump(const void *ptr, uint32_t len)
{
	for (uint32_t i = 0; i < len; i++)
	{
		LOG_PRINTF("%02x ", ((const uint8_t *)ptr)[i]);
		if (i % 16 == 15)
		{
			LOG_PRINTF("\n");
		}
		else if (i % 4 == 3)
		{
			LOG_PRINTF(" ");
		}
	}
	LOG_PRINTF("\n");
}
#endif


void bulkonly_storage_driver_init(const bulkonly_storage_callbacks_t *cb)
{
	LOG_PRINTF("Initialize bulkonly driver\n");
	callbacks = *cb;
	uint32_t i;
	for (i = 0; i < USBH_BULKONLY_STORAGE_MAX_DEVICES; i++) {
		devices[i].state = STATE_INACTIVE;
	}
}


static void send_read_cmd6(bulkonly_storage_device_t *dev, uint32_t sector_first, uint8_t sector_count, void *buffer)
{
	scsi_command_read_6_t cmd = {
		.opcode = SCSI_COMMAND_READ6_OPCODE,
		.length = sector_count
	};

	num_to_unaligned_be(sector_first, cmd.lba, 3);
	send_scsi_command(dev, &cmd, sizeof(cmd), buffer, sector_count * dev->sector_size, 0);
}


static void send_read_cmd10(bulkonly_storage_device_t *dev, uint32_t sector_first, uint32_t sector_count, void *buffer)
{
	scsi_command_read_10_t cmd = {
		.opcode = SCSI_COMMAND_READ10_OPCODE,
	};

	num_to_unaligned_be(sector_first, cmd.lba, 4);
	num_to_unaligned_be(sector_count, cmd.length, 2);
	send_scsi_command(dev, &cmd, sizeof(cmd), buffer, sector_count * dev->sector_size, 0);
}

#if USBH_BULKONLY_STORAGE_SUPPORT_64_ADDRESS
static void send_read_cmd16(bulkonly_storage_device_t *dev, uint64_t sector_first, uint32_t sector_count, void *buffer)
{
	scsi_command_read_16_t cmd = {
		.opcode = SCSI_COMMAND_READ16_OPCODE,
	};

	num_to_unaligned_be64(sector_first, cmd.lba);
	num_to_unaligned_be(sector_count, cmd.length, 4);
	send_scsi_command(dev, &cmd, sizeof(cmd), buffer, sector_count * dev->sector_size, 0);
}
#endif

#if !USBH_BULKONLY_STORAGE_READONLY
static void send_write_cmd6(bulkonly_storage_device_t *dev, uint32_t sector_first, uint8_t sector_count, const void *buffer)
{
	scsi_command_write_6_t cmd = {
		.opcode = SCSI_COMMAND_WRITE6_OPCODE,
		.length = sector_count
	};

	num_to_unaligned_be(sector_first, cmd.lba, 3);
	send_scsi_command(dev, &cmd, sizeof(cmd), (void *)buffer, sector_count * dev->sector_size, 1);
}


static void send_write_cmd10(bulkonly_storage_device_t *dev, uint32_t sector_first, uint32_t sector_count, const void *buffer)
{
	scsi_command_write_10_t cmd = {
		.opcode = SCSI_COMMAND_WRITE10_OPCODE,
	};

	num_to_unaligned_be(sector_first, cmd.lba, 4);
	num_to_unaligned_be(sector_count, cmd.length, 2);
	send_scsi_command(dev, &cmd, sizeof(cmd), (void *)buffer, sector_count * dev->sector_size, 1);
}

#if USBH_BULKONLY_STORAGE_SUPPORT_64_ADDRESS
static void send_write_cmd16(bulkonly_storage_device_t *dev, uint64_t sector_first, uint32_t sector_count, const void *buffer)
{
	scsi_command_write_16_t cmd = {
		.opcode = SCSI_COMMAND_WRITE16_OPCODE,
	};

	num_to_unaligned_be64(sector_first, cmd.lba);
	num_to_unaligned_be(sector_count, cmd.length, 4);
	send_scsi_command(dev, &cmd, sizeof(cmd), (void *)buffer, sector_count * dev->sector_size, 1);
}
#endif

#endif


bool bulkonly_storage_read(uint8_t device_id, uint64_t sector_first, uint32_t sector_count, void *buffer)
{
	if (device_id >= USBH_BULKONLY_STORAGE_MAX_DEVICES)
	{
		LOG_PRINTF("Invalid device ID\n");
		return false;
	}

	bulkonly_storage_device_t *dev = &devices[device_id];

	if (dev->state != STATE_IDLE || dev->init_state != INIT_STATE_DONE)
	{
		LOG_PRINTF("Can't send the read command while device is in state %u (init: %u)\n", dev->state, dev->init_state);
		return false;
	}

	if (sector_first >= dev->sector_count || sector_first + sector_count > dev->sector_count)
	{
		LOG_PRINTF("Read address out of bounds\n");
		return false;
	}

	// Send SCSI read
	if (dev->sector_count < (1 << 20) && sector_count < (1 << 8))
	{
		send_read_cmd6(dev, sector_first, sector_count, buffer);
	}
	else if (dev->sector_count < (1ULL << 32) && sector_count < (1 << 16))
	{
		send_read_cmd10(dev, sector_first, sector_count, buffer);
	}
	else
	{
#if USBH_BULKONLY_STORAGE_SUPPORT_64_ADDRESS
		send_read_cmd16(dev, sector_first, sector_count, buffer);
#else
		return false;
#endif
	}
	return true;
}


#if !USBH_BULKONLY_STORAGE_READONLY
bool bulkonly_storage_write(uint8_t device_id, uint64_t sector_first, uint32_t sector_count, const void *buffer)
{
	if (device_id >= USBH_BULKONLY_STORAGE_MAX_DEVICES)
	{
		LOG_PRINTF("Invalid device ID\n");
		return false;
	}

	bulkonly_storage_device_t *dev = &devices[device_id];

	if (dev->state != STATE_IDLE || dev->init_state != INIT_STATE_DONE)
	{
		LOG_PRINTF("Can't send write command while device is in state %u (init: %u)\n", dev->state, dev->init_state);
		return false;
	}

	if (sector_first >= dev->sector_count || sector_first + sector_count > dev->sector_count)
	{
		LOG_PRINTF("Write address out of bounds\n");
		return false;
	}

	// Send SCSI read
	if (dev->sector_count < (1 << 20) && sector_count < (1 << 8))
	{
		send_write_cmd6(dev, sector_first, sector_count, buffer);
	}
	else if (dev->sector_count < (1ULL << 32) && sector_count < (1 << 16))
	{
		send_write_cmd10(dev, sector_first, sector_count, buffer);
	}
	else
	{
#if USBH_BULKONLY_STORAGE_SUPPORT_64_ADDRESS
		send_write_cmd16(dev, sector_first, sector_count, buffer);
#else
		return false;
#endif
	}
	return true;
}
#endif

/**
 *
 *
 */
static void *init(usbh_device_t *usbh_dev)
{
	uint32_t i;
	bulkonly_storage_device_t *drvdata = 0;

	for (i = 0; i < USBH_BULKONLY_STORAGE_MAX_DEVICES; i++)
	{
		if (devices[i].state == STATE_INACTIVE)
		{
			drvdata = &devices[i];
			drvdata->device_id = i;
			drvdata->endpoint_in_address = 0;
			drvdata->endpoint_out_address = 0;
			drvdata->endpoint_in_toggle = 0;
			drvdata->endpoint_out_toggle = 0;
			drvdata->error_counter = 0;
			drvdata->data_ptr = NULL;
			drvdata->last_tx_data_ptr = NULL;
			drvdata->usbh_device = usbh_dev;
			break;
		}
	}

	return drvdata;
}

/**
 * Returns true if all needed data are parsed
 */
static bool analyze_descriptor(void *drvdata, void *descriptor)
{
	bulkonly_storage_device_t *dev = (bulkonly_storage_device_t *)drvdata;
	uint8_t desc_type = ((uint8_t *)descriptor)[1];
	switch (desc_type) {
	case USB_DT_CONFIGURATION:
		{
			struct usb_config_descriptor *cfg = (struct usb_config_descriptor*)descriptor;
			dev->configuration_value = cfg->bConfigurationValue;
		}
		break;
	case USB_DT_DEVICE:
		break;
	case USB_DT_INTERFACE:
		break;
	case USB_DT_ENDPOINT:
		{
			struct usb_endpoint_descriptor *ep = (struct usb_endpoint_descriptor*)descriptor;
			if ((ep->bmAttributes & 0x03) == USB_ENDPOINT_ATTR_BULK) {
				uint8_t epaddr = ep->bEndpointAddress;
				if (epaddr & (1<<7)) {
					dev->endpoint_in_address = epaddr&0x7f;
					dev->endpoint_in_maxpacketsize = ep->wMaxPacketSize;
				}
				else
				{
					dev->endpoint_out_address = epaddr;
					dev->endpoint_out_maxpacketsize = ep->wMaxPacketSize;
				}

				if (dev->endpoint_in_address && dev->endpoint_out_address)
				{
					dev->state = STATE_INITIALIZED;
					return true;
				}
			}
		}
		break;
	// TODO Class Specific descriptors
	default:
		break;
	}
	return false;
}


static void event(usbh_device_t *dev, usbh_packet_callback_data_t cb_data)
{
	bulkonly_storage_device_t *d = (bulkonly_storage_device_t *)dev->drvdata;

	LOG_PRINTF("\nUSB event: status=%u, tx=%u, drvstate=%u\n", cb_data.status, cb_data.transferred_length, d->state);

	if (cb_data.status == USBH_PACKET_CALLBACK_STATUS_EAGAIN)
	{
		if (d->last_tx_data_ptr != NULL && d->last_tx_data_size)
		{
			LOG_PRINTF("Resending last packet with %u bytes\n", d->last_tx_data_size);
			send_packet(d, d->last_tx_data_ptr, d->last_tx_data_size);
			return;
		}
	}
	d->last_tx_data_ptr = NULL;

	if (cb_data.status != USBH_PACKET_CALLBACK_STATUS_OK &&
		cb_data.status != USBH_PACKET_CALLBACK_STATUS_ERRSIZ)
	{
		LOG_PRINTF("TRANSFER ERROR!\n");
		begin_reset_recovery(d);
		return;
	}

	switch (d->state)
	{
		case STATE_INACTIVE:
		case STATE_INITIALIZED:
		case STATE_IDLE:
		LOG_PRINTF("\nUNEXPECTED STATE FOR EVENT IN BOS DRIVER: %u\n");
			break;

		case STATE_WAIT_CRTL:
			d->state = STATE_IDLE;
			break;

		case STATE_EXPECT_CSW:
		{
			// Check, if the received CSW is valid
			LOG_PRINTF("\nCheck CSW\n");
			handle_csw(d, cb_data.transferred_length);
			break;
		}

		case STATE_READ_DATA:
		{
			// Prepare a read for the data (or read the CSW, if the data transmission is complete)
			if (d->data_remaining_bytes == 0)
			{
				// No data left to read -> prepare to read the csw
				receive_csw(d);
				break;
			}

			// Read more data
			uint32_t next = d->data_remaining_bytes;
			if (next > d->endpoint_in_maxpacketsize)
			{
				next = d->endpoint_in_maxpacketsize;
			}
			LOG_PRINTF("receive data: total=%u; chunk=%u\n", d->data_remaining_bytes, next);

			recv_packet(d, d->data_ptr, next);

			// Advance the pointer and reduce the remaining size
			d->data_ptr = ((uint8_t *)(d->data_ptr)) + next;
			d->data_remaining_bytes -= next;
			break;
		}

		case STATE_WRITE_DATA:
		{
			// Prepare to write a data packet (or read the CSW, if the data transmission is complete)
			if (d->data_remaining_bytes == 0)
			{
				// No data left to send -> prepare to read the csw
				receive_csw(d);
				break;
			}

			// Write more data
			uint32_t next = d->data_remaining_bytes;
			if (next > d->endpoint_out_maxpacketsize)
			{
				next = d->endpoint_out_maxpacketsize;
			}

			LOG_PRINTF("send data: total=%u; chunk=%u\n", d->data_remaining_bytes, next);
			send_packet(d, d->data_ptr, next);

			// Advance the pointer and reduce the remaining size
			d->data_ptr = ((uint8_t *)(d->data_ptr)) + next;
			d->data_remaining_bytes -= next;
			break;
		}

		case STATE_RESET_MSR:
		case STATE_RESET_CFH_IN:
		case STATE_RESET_CFH_OUT:
		{
			reset_recovery_step(d);
			break;
		}
		default:
		{
			LOG_PRINTF("Unknown state\n");
			break;
		}
	}
}


static void recv_packet(bulkonly_storage_device_t *dev, void *data, uint32_t len)
{
	usbh_packet_t packet;

	packet.address = dev->usbh_device->address;
	packet.data.in = data;
	packet.datalen = len;
	packet.endpoint_address = dev->endpoint_in_address;
	packet.endpoint_size_max = dev->endpoint_in_maxpacketsize;
	packet.endpoint_type = USBH_ENDPOINT_TYPE_BULK;
	packet.speed = dev->usbh_device->speed;
	packet.callback = event;
	packet.callback_arg = dev->usbh_device;
	packet.toggle = &dev->endpoint_in_toggle;

	dev->last_tx_data_ptr = NULL;

	usbh_read(dev->usbh_device, &packet);
}


/*
 * Sends some data to the device.
 * The caller has to unsure the len does not exceed the maximum allowed size of the driver.
 */
static void send_packet(bulkonly_storage_device_t *dev, const void *data, uint32_t len)
{
	usbh_packet_t packet;

	packet.address = dev->usbh_device->address;
	packet.data.out = data;
	packet.datalen = len;
	packet.endpoint_address = dev->endpoint_out_address;
	packet.endpoint_size_max = dev->endpoint_out_maxpacketsize;
	packet.endpoint_type = USBH_ENDPOINT_TYPE_BULK;
	packet.speed = dev->usbh_device->speed;
	packet.callback = event;
	packet.callback_arg = dev->usbh_device;
	packet.toggle = &dev->endpoint_out_toggle;

	dev->last_tx_data_ptr = data;
	dev->last_tx_data_size = len;

	usbh_write(dev->usbh_device, &packet);
}


/*
 * Performs a reset recovery procedure.
 * This should be called in the event handler when the state is a reset state.
 * The return value indicates whether the reset is complete (true) or not (false)
 */
static void reset_recovery_step(bulkonly_storage_device_t *dev)
{
	LOG_PRINTF("reset recovery step, state=%u\n", dev->state);
	switch (dev->state)
	{
		case STATE_RESET_MSR:
		{
			// We have completed the MSR reset -> reset input HALT
			struct usb_setup_data cmd = {
				.bmRequestType = USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_ENDPOINT,
				.bRequest = USB_REQ_CLEAR_FEATURE,
				.wValue = USB_FEAT_ENDPOINT_HALT,
				.wIndex = dev->endpoint_in_address | 0x80,
				.wLength = 0
			};
			dev->state = STATE_RESET_CFH_IN;
			device_control(dev->usbh_device, event, &cmd, NULL);
			return;
		}

		case STATE_RESET_CFH_IN:
		{
			// We have completed the input HALT reset  -> reset output HALT
			struct usb_setup_data cmd = {
				.bmRequestType = USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_ENDPOINT,
				.bRequest = USB_REQ_CLEAR_FEATURE,
				.wValue = USB_FEAT_ENDPOINT_HALT,
				.wIndex = dev->endpoint_out_address,
				.wLength = 0

			};
			dev->state = STATE_RESET_CFH_OUT;
			device_control(dev->usbh_device, event, &cmd, NULL);
			return;
		}

		case STATE_RESET_CFH_OUT:
		{
			// reset output HALT feature complete -> we are done with the reset!
			LOG_PRINTF("RESET complete -> reinitialize\n");
			dev->state = STATE_BEGIN_INIT;

			LOG_PRINTF("to=%u, ti=%u\n", dev->endpoint_out_toggle, dev->endpoint_in_toggle);

			/*
			 * Reset the toggle bits.
			 *
			 * NOTE:
			 * This contradicts the bulkonly storage spec!
			 * It explicitly says:
			 * "The device shall preserve the value of its bulk data toggle bits and
			 *  endpoint STALL conditions despite the Bulk-Only Mass Storage Reset"
			 * However, NOT resetting the toggle bits here results is a blocked command.
			 * (This may also be needed because of some error in the USB stack though...)
			 */
			dev->endpoint_out_toggle = 0;
			dev->endpoint_in_toggle = 0;
			return;
		}

		// In (initial) error state
		// -> send MSR request
		case STATE_ERROR:
		{
			struct usb_setup_data cmd = {
				.bmRequestType = USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
				.bRequest = 0xff,
				.wValue = 0,
				.wIndex = 0,
				.wLength = 0
			};
			dev->state = STATE_RESET_MSR;
			device_control(dev->usbh_device, event, &cmd, NULL);
			return;
		}

		default:
		{
			LOG_PRINTF("Bad state for reset_recovery_step: %u\n", dev->state);
		}
	}
}

static void begin_reset_recovery(bulkonly_storage_device_t *dev)
{
	LOG_PRINTF("Perform reset recovery, state=%u\n", dev->state);
	dev->state = STATE_ERROR;

	if (dev->error_counter != 0xff)
	{
		dev->error_counter++;
	}

	if (callbacks.error)
	{
		callbacks.error(dev->device_id, dev->error_counter);
	}
	return;
}


static void receive_csw(bulkonly_storage_device_t *dev)
{
	_Static_assert(sizeof(bulkonly_csw_t) <= sizeof(dev->cmd_buffer),
				   "CSW must fit into the command buffer");

	LOG_PRINTF("receive csw\n");
	dev->state = STATE_EXPECT_CSW;
	memset(dev->cmd_buffer, 0, 13);
	recv_packet(dev, dev->cmd_buffer, sizeof(bulkonly_csw_t));
}


static bool check_csw(bulkonly_storage_device_t *dev, uint32_t len)
{
	if (len != sizeof(bulkonly_csw_t))
	{
		LOG_PRINTF("BAD CSW SIZE: %u\n", len);
		return false;
	}

	// The CSW will be in the shared buffer
	bulkonly_csw_t *csw = (bulkonly_csw_t *)dev->cmd_buffer;

	if (csw->signature != BULKONLY_CSW_SIGNATURE)
	{
		LOG_PRINTF("BAD CSW SIGNATURE: %u\n", csw->signature);
		return false;
	}

	if (csw->tag != dev->last_tag)
	{
		LOG_PRINTF("BAD CSW TAG: %u, EXPTECTED: %u\n", csw->tag, dev->last_tag);
		return false;
	}

	return true;
}


static void handle_csw(bulkonly_storage_device_t *dev, uint32_t len)
{
	_Static_assert(sizeof(bulkonly_csw_t) <= sizeof(dev->cmd_buffer),
				   "CSW must fit into the command buffer");

	bulkonly_csw_t *csw = (bulkonly_csw_t *)dev->cmd_buffer;

	if (!check_csw(dev, len) ||
		(csw->status != BULKONLY_CSW_STATUS_GOOD &&
		 csw->status != BULKONLY_CSW_STATUS_FAILED))
	{
		if (dev->init_state != INIT_STATE_DONE)
		{
			/*
			 * NOTE: This is non-standard, however I found some devices that sometimes reply to the first TEST_UNIT_READY after a MSR with
			 *       the beginning of the CBW (instead of the expected CSW).
			 *       Just ignoring this and restarting the init sequence solves this problem.
			 */
			LOG_PRINTF("Invalid CSW in INIT, try to restart init procedure\n");
			dev->init_state = INIT_STATE_START;
			dev->state = STATE_IDLE;
		}
		else
		{
			LOG_PRINTF("Invalid CSW -> do reset\n");
			begin_reset_recovery(dev);
		}

		if (callbacks.error)
		{
			callbacks.error(dev->device_id, dev->error_counter);
		}
		return;
	}


	LOG_PRINTF("CSW RESIDUAL: %u\n", csw->dataResidue);
	LOG_PRINTF("CSW STATUS: %u\n", csw->status);

	if (csw->status == BULKONLY_CSW_STATUS_FAILED)
	{
		/*
		 * If some command fails during init, we just do a SENSE request and begin with the init sequence again.
		 * This is required for some devices that won't report the unit as ready util the sense data is read.
		 */

		LOG_PRINTF("Command FAILED in init_state %u -> do SENSE request\n", dev->init_state);
		dev->state = STATE_IDLE;
		if (dev->init_state == INIT_STATE_SENSE_TXERR ||
			dev->init_state == INIT_STATE_SENSE_INITERR)
		{
			LOG_PRINTF("SENSE request failed!\n");
			begin_reset_recovery(dev);
		}
		else
		{
			scsi_command_request_sense_t cmd = {
				.opcode = SCSI_COMMAND_REQUEST_SENSE_OPCODE,
				.length = sizeof(dev->data_buffer) <= 0xff ? sizeof(dev->data_buffer) : 0xff};

			send_scsi_command(dev, &cmd, sizeof(cmd), dev->data_buffer, cmd.length, 0);

			if (dev->init_state != INIT_STATE_DONE)
			{
				dev->init_state = INIT_STATE_SENSE_INITERR;
			}
			else
			{
				dev->init_state = INIT_STATE_SENSE_TXERR;
			}
		}
		return;
	}

	if (callbacks.tx_done && dev->init_state == INIT_STATE_DONE)
	{
		callbacks.tx_done(dev->device_id, BULKONLY_STORAGE_TX_RESULT_OK);
	}
	dev->state = STATE_IDLE;
}


static void send_scsi_command(bulkonly_storage_device_t *dev, const void *cmd, uint8_t cmd_len, void *data_ptr, uint32_t data_length, uint8_t dir_write)
{
	_Static_assert(sizeof(bulkonly_cbw_t) <= sizeof(dev->cmd_buffer),
				   "CBW must fit into the command buffer");


    ASSERT(dev->state == STATE_IDLE);

	bulkonly_cbw_t *cbw = (bulkonly_cbw_t *)dev->cmd_buffer;

	cbw->signature = BULKONLY_CBW_SIGNATURE;
	cbw->tag = ++(dev->last_tag);
	cbw->dataTransferLength = data_length;
	if (dir_write)
	{
		cbw->flags = 0;
	}
	else
	{
		cbw->flags = BULKONLY_CBW_FLAG_DIRECTION;
	}
	cbw->lun = 0;
	cbw->cbLength = cmd_len;

    ASSERT(cmd_len <= sizeof(cbw->command));

	memcpy(cbw->command, cmd, cmd_len);

	dev->data_ptr = data_ptr;
	dev->data_remaining_bytes = data_length;

	if (dir_write)
	{
		dev->state = STATE_WRITE_DATA;
	}
	else
	{
		dev->state = STATE_READ_DATA;
	}

	LOG_PRINTF("send packet with size: %u, SCSI_CMD: %u\n", sizeof(*cbw), cmd_len);

	send_packet(dev, cbw, sizeof(*cbw));
}


static void init_step(bulkonly_storage_device_t *dev)
{
	ASSERT(dev->state == STATE_IDLE);
	switch (dev->init_state)
	{
		case INIT_STATE_START:
		{

			scsi_command_test_unit_ready_t cmd = {
				.opcode = SCSI_COMMAND_TEST_UNIT_READY_OPCODE};

			LOG_PRINTF("Doing TEST_UNIT_READY request\n");
			send_scsi_command(dev, &cmd, sizeof(cmd), NULL, 0, 0);
			dev->init_state = INIT_STATE_TEST_UNIT_READY;
			break;
		}
		case INIT_STATE_TEST_UNIT_READY:
		{
			scsi_command_read_capacity10_t cmd = {
				.opcode = SCSI_COMMAND_READ_CAPACITY10_OPCODE
			};
			_Static_assert(sizeof(dev->data_buffer) >=
						   sizeof(scsi_command_read_capacity10_response_t),
						   "Data buffer must be larger than the read capacity (10) response");

			LOG_PRINTF("Send READ_CAPACITY(10) command\n");
			send_scsi_command(dev,
							  &cmd,
							  sizeof(cmd),
							  dev->data_buffer,
							  sizeof(scsi_command_read_capacity10_response_t),
							  0);
			dev->init_state = INIT_STATE_CAPACITY10;
			break;
		}
		case INIT_STATE_CAPACITY10:
		{
			scsi_command_read_capacity10_response_t *res =
				(scsi_command_read_capacity10_response_t *)(dev->data_buffer);

#ifdef LUSBH_USART_DEBUG
			LOG_PRINTF("Dumping capacity data\n");
			hexdump(res, sizeof(*res));
#endif

			uint32_t blockcount = unaligned_be_to_num(res->lba, 4);
			uint32_t blocksize = unaligned_be_to_num(res->blocksize, 4);

#if USBH_BULKONLY_STORAGE_SUPPORT_64_ADDRESS
			if (blockcount != 0xffffffffUL)
			{
#endif
				dev->sector_count = blockcount;
				dev->sector_size = blocksize;

				dev->error_counter = 0;

				LOG_PRINTF("Connected storage with 0x%08x blocks a %u bytes\n", blockcount, blocksize);

				LOG_PRINTF("Size: %uMB\n", (uint32_t)((blocksize * ((uint64_t)(blockcount))) >> 20));


				LOG_PRINTF("Mass storage init done\n");
				dev->init_state = INIT_STATE_DONE;
				if (callbacks.ready)
				{
					callbacks.ready(dev->device_id, dev->sector_count, dev->sector_size);
				}

				break;
#if USBH_BULKONLY_STORAGE_SUPPORT_64_ADDRESS
			}

			LOG_PRINTF("Large medium detected -> retry with 64 bit command\n");
			scsi_command_read_capacity16_t cmd = {
				.opcode = SCSI_COMMAND_READ_CAPACITY16_OPCODE,
				.action = SCSI_COMMAND_READ_CAPACITY16_ACTION,
				.allocation_length = {0, 0, 0, sizeof(scsi_command_read_capacity16_response_t)}
			};

			_Static_assert(sizeof(dev->data_buffer) >=
						   sizeof(scsi_command_read_capacity16_response_t),
						   "Data buffer must be larger than the read capacity (16) response");

			LOG_PRINTF("Send READ_CAPACITY(16) command\n");
			send_scsi_command(dev,
							  &cmd,
							  sizeof(cmd),
							  dev->data_buffer,
							  sizeof(scsi_command_read_capacity16_response_t),
							  0);
			dev->init_state = INIT_STATE_CAPACITY16;
			break;
#endif
		}
#if USBH_BULKONLY_STORAGE_SUPPORT_64_ADDRESS
		case INIT_STATE_CAPACITY16:
		{
			scsi_command_read_capacity16_response_t *res =
				(scsi_command_read_capacity16_response_t *)(dev->data_buffer);

#ifdef LUSBH_USART_DEBUG
			LOG_PRINTF("Dumping capacity (16) data\n");
			hexdump(res, sizeof(*res));
#endif

			uint64_t blockcount = unaligned_be_to_num64(res->lba);
			uint32_t blocksize = unaligned_be_to_num(res->blocksize, 4);

			dev->sector_count = blockcount;
			dev->sector_size = blocksize;

			dev->error_counter = 0;

			LOG_PRINTF("Connected storage with 0x%08x%08x blocks a %u bytes\n", (uint32_t)(blockcount >> 32), (uint32_t)blockcount, blocksize);

			LOG_PRINTF("Size: %uMB\n", (uint32_t)((blocksize * blockcount) >> 20));


			LOG_PRINTF("Mass storage init done\n");
			dev->init_state = INIT_STATE_DONE;
			if (callbacks.ready)
			{
				callbacks.ready(dev->device_id, dev->sector_count, dev->sector_size);
			}

			break;
		}
#endif


		case INIT_STATE_SENSE_INITERR:
		{
			dev->init_state = INIT_STATE_START;
			break;
		}
		case INIT_STATE_SENSE_TXERR:
		{
			dev->init_state = INIT_STATE_DONE;
			if (callbacks.tx_done)
			{
				callbacks.tx_done(dev->device_id, BULKONLY_STORAGE_TX_RESULT_FAILED);
			}
			break;
		}
	}
}


/**
 * \param time_curr_us - monotically rising time (see usbh_hubbed.h)
 *		unit is microseconds
 */
static void poll(void *drvdata, uint32_t time_curr_us)
{
	(void)time_curr_us;

	bulkonly_storage_device_t *dev = (bulkonly_storage_device_t *)drvdata;


	switch (dev->state)
	{
		case STATE_INITIALIZED:
		{
			LOG_PRINTF("\nUSB mass storage connected -> Begin init\n");
			if (callbacks.connect)
			{
			    callbacks.connect(dev->device_id);
			}
			begin_reset_recovery(dev);
			break;
		}
		case STATE_BEGIN_INIT:
		{
			dev->state = STATE_IDLE;
			dev->init_state = INIT_STATE_START;
			init_step(dev);
			break;
		}
		case STATE_IDLE:
		{
			if (dev->init_state != INIT_STATE_DONE)
			{
				init_step(dev);
				break;
			}
			break;
		}

		case STATE_ERROR:
		{
			reset_recovery_step(dev);
			break;
		}

		// All other states: Nothing to do!
		// Maybe we could add a timeout for reading and writing...
		default:
		{
			return;
		}
		break;
	}
}

static void disconnect(void *drvdata)
{
	LOG_PRINTF("\nUSB mass storage removed\n");
	bulkonly_storage_device_t *dev = (bulkonly_storage_device_t *)drvdata;
	dev->state = STATE_INACTIVE;
	if (callbacks.disconnect)
	{
		callbacks.disconnect(dev->device_id);
	}
}

static const usbh_dev_driver_info_t driver_info = {
	.deviceClass = -1,
	.deviceSubClass = -1,
	.deviceProtocol = -1,
	.idVendor = -1,
	.idProduct = -1,
	.ifaceClass = 0x08,
	.ifaceSubClass = 0x06,
	.ifaceProtocol = 0x50
};

const usbh_dev_driver_t usbh_bulkonly_storage_driver = {
	.init = init,
	.analyze_descriptor = analyze_descriptor,
	.poll = poll,
	.remove = disconnect,
	.info = &driver_info
};
