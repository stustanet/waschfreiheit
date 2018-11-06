/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#include "usb_storage_helper.h"

#include "usart_helpers.h"
#include "usbh_core.h"
#include "usbh_lld_stm32f4.h"

#include "usbh_driver_bulkonly_storage.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

enum USB_MSC_COMMAND
{
	USB_MSC_COMMAND_NONE,
	USB_MSC_COMMAND_READ
};


enum USB_MSC_STATUS
{
	USB_MSC_STATUS_CONNECTED = 1,
	USB_MSC_STATUS_READY = 2
};

enum USB_MSC_STATUS_CMD
{
	USB_MSC_STATUS_CMD_TXDONE = 1,
	USB_MSC_STATUS_CMD_ERR = 2
};

static struct
{
	uint64_t startcluster;
	void *buffer;
	uint32_t count;
	enum USB_MSC_COMMAND cmd;
} volatile usb_msc_command;

static struct
{
	uint64_t num_of_sectors;
	uint32_t bytes_per_sector;
	uint8_t error_cnt;
	uint8_t flags;
	uint8_t cmd;
} volatile usb_msc_status;

static volatile uint32_t ticks_ms = 0;

static const usbh_dev_driver_t *device_drivers[] = {
	&usbh_bulkonly_storage_driver,
	NULL
};


static const usbh_low_level_driver_t * const lld_drivers[] = {
	&usbh_lld_stm32f4_driver_fs,
	NULL
};


static void usb_msc_connect(uint8_t device_id)
{
    (void)device_id;
	LOG_PRINTF("USB MSC CONNECT %u\n", device_id);
	usb_msc_status.flags |= USB_MSC_STATUS_CONNECTED;
}

static void usb_msc_disconnect(uint8_t device_id)
{
	LOG_PRINTF("USB MSC DISCONNECT %u\n", device_id);
	usb_msc_status.flags &= ~(USB_MSC_STATUS_CONNECTED | USB_MSC_STATUS_READY);
}

static void usb_msc_error(uint8_t device_id, uint8_t error_cnt)
{
	LOG_PRINTF("USB MSC ERROR %u\n", device_id);
	usb_msc_status.error_cnt = error_cnt;
	usb_msc_status.flags &= ~USB_MSC_STATUS_READY;
}

static void usb_msc_ready(uint8_t device_id, uint64_t num_of_sectors, uint32_t bytes_per_sector)
{
	LOG_PRINTF("USB MSC READY %u\n", device_id);
	usb_msc_status.num_of_sectors = num_of_sectors;
	usb_msc_status.bytes_per_sector = bytes_per_sector;
	usb_msc_status.flags |= USB_MSC_STATUS_READY;
}


static void usb_msc_tx_done(uint8_t device_id, uint8_t result)
{
	LOG_PRINTF("USB MSC TX DONE %u -> %u\n", device_id, result);
	if (result == BULKONLY_STORAGE_TX_RESULT_OK)
	{
		usb_msc_status.cmd |= USB_MSC_STATUS_CMD_TXDONE;
	}
	else
	{
		usb_msc_status.cmd |= USB_MSC_STATUS_CMD_ERR;
	}
}


enum USB_READ_RESULT usb_storage_read_wait(uint64_t start, uint32_t size, void *ptr)
{
	if (!usb_storage_ok())
	{
		return USB_READ_FATAL;
	}
	if (start + size > usb_msc_status.num_of_sectors)
	{
		return USB_READ_OUT_OF_BOUNDS;
	}

	// clear cmd status
	usb_msc_status.cmd = 0;

	usb_msc_command.startcluster = start;
	usb_msc_command.count = size;
	usb_msc_command.buffer = ptr;
	usb_msc_command.cmd = USB_MSC_COMMAND_READ;

	// now wait for the finished event
	while (1)
	{
		if ((usb_msc_status.flags & USB_MSC_STATUS_READY) == 0)
		{
			// dev no longer ready -> BIG PROBLEM
			return USB_READ_FATAL;
		}

		if ((usb_msc_status.cmd & USB_MSC_STATUS_CMD_ERR) != 0)
		{
			// command error -> minor problem
			return USB_READ_ERROR;
		}

		if ((usb_msc_status.cmd & USB_MSC_STATUS_CMD_TXDONE) != 0)
		{
			// done
			return USB_READ_OK;
		}
	}
}


uint32_t usb_storage_sector_size(void)
{
	return usb_msc_status.bytes_per_sector;
}

uint64_t usb_storage_sector_count(void)
{
	return usb_msc_status.num_of_sectors;
}


bool usb_storage_wait(uint32_t timeout_ms)
{
	uint32_t start = ticks_ms;
	while (((ticks_ms - start) < timeout_ms) && !usb_storage_ok());
	return usb_storage_ok();
}


bool usb_storage_ok(void)
{
	return (usb_msc_status.flags & USB_MSC_STATUS_READY) != 0;
}


void usb_storage_init(void)
{
	rcc_periph_clock_enable(RCC_GPIOA); // OTG_FS
	rcc_periph_clock_enable(RCC_OTGFS); // OTG_FS

	// OTG_FS
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO11 | GPIO12);
	gpio_set_af(GPIOA, GPIO_AF10, GPIO11 | GPIO12);


	memset((void *)&usb_msc_status, 0, sizeof(usb_msc_status));
	memset((void *)&usb_msc_command, 0, sizeof(usb_msc_command));

	bulkonly_storage_callbacks_t boscb = {
		.connect = usb_msc_connect,
		.disconnect = usb_msc_disconnect,
		.error = usb_msc_error,
		.ready = usb_msc_ready,
		.tx_done = usb_msc_tx_done
	};
	bulkonly_storage_driver_init(&boscb);

	usbh_init(lld_drivers, device_drivers);
}


void usb_storage_poll(void)
{
	if (usb_msc_command.cmd == USB_MSC_COMMAND_READ)
	{
		if (!bulkonly_storage_read(0, usb_msc_command.startcluster, usb_msc_command.count, usb_msc_command.buffer))
		{
			usb_msc_status.cmd |= USB_MSC_STATUS_CMD_ERR;
		}
		usb_msc_command.cmd = USB_MSC_COMMAND_NONE;
	}

	ticks_ms++;
	usbh_poll(ticks_ms * 1000);
	LOG_FLUSH();
}
