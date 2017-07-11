/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2010 Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This library is free software: you can redistribute it and/or modify
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
 */

/*
 * Original file taken from libopencm3 example "usb_cdcacm"
 * Modified by Daniel Frejek
 * 2017-07-09
 */

#include "usbacm.h"
#include <stdlib.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/cdc.h>

#include "delay.h"

#define USB_XFER_BUFFER_SIZE 64
#define USB_TX_TIMEOUT_TICKS 50

// Need this to be static for interrupts
static usbd_device *acm_usb_device;

static const struct usb_device_descriptor dev = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = USB_CLASS_CDC,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = USB_XFER_BUFFER_SIZE,
	.idVendor = 0x0483,
	.idProduct = 0x5740,
	.bcdDevice = 0x0200,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1,
};

/*
 * This notification endpoint isn't implemented. According to CDC spec its
 * optional, but its absence causes a NULL pointer dereference in Linux
 * cdc_acm driver.
 */
static const struct usb_endpoint_descriptor comm_endp[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x83,
	.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
	.wMaxPacketSize = 16,
	.bInterval = 255,
}};

static const struct usb_endpoint_descriptor data_endp[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x01,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = USB_XFER_BUFFER_SIZE,
	.bInterval = 1,
}, {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x82,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = USB_XFER_BUFFER_SIZE,
	.bInterval = 1,
}};

static const struct {
	struct usb_cdc_header_descriptor header;
	struct usb_cdc_call_management_descriptor call_mgmt;
	struct usb_cdc_acm_descriptor acm;
	struct usb_cdc_union_descriptor cdc_union;
} __attribute__((packed)) cdcacm_functional_descriptors = {
	.header = {
		.bFunctionLength = sizeof(struct usb_cdc_header_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_HEADER,
		.bcdCDC = 0x0110,
	},
	.call_mgmt = {
		.bFunctionLength =
			sizeof(struct usb_cdc_call_management_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
		.bmCapabilities = 0,
		.bDataInterface = 1,
	},
	.acm = {
		.bFunctionLength = sizeof(struct usb_cdc_acm_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_ACM,
		.bmCapabilities = 0,
	},
	.cdc_union = {
		.bFunctionLength = sizeof(struct usb_cdc_union_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_UNION,
		.bControlInterface = 0,
		.bSubordinateInterface0 = 1,
	 },
};

static const struct usb_interface_descriptor comm_iface[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 1,
	.bInterfaceClass = USB_CLASS_CDC,
	.bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
	.bInterfaceProtocol = USB_CDC_PROTOCOL_AT,
	.iInterface = 0,

	.endpoint = comm_endp,

	.extra = &cdcacm_functional_descriptors,
	.extralen = sizeof(cdcacm_functional_descriptors),
}};

static const struct usb_interface_descriptor data_iface[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_DATA,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 0,

	.endpoint = data_endp,
}};

static const struct usb_interface ifaces[] = {{
	.num_altsetting = 1,
	.altsetting = comm_iface,
}, {
	.num_altsetting = 1,
	.altsetting = data_iface,
}};

static const struct usb_config_descriptor config = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
	.bNumInterfaces = 2,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80,
	.bMaxPower = 0x32,

	.interface = ifaces,
};

static const char *usb_strings[] = {
	"Black Sphere Technologies",
	"CDC-ACM Demo",
	"DEMO",
};

/* Buffer to be used for control requests. */
static uint8_t usbd_control_buffer[128];


static void usb_ints_enable(void)
{
	nvic_enable_irq(NVIC_USB_WAKEUP_IRQ);
	nvic_enable_irq(NVIC_USB_HP_CAN_TX_IRQ);
	nvic_enable_irq(NVIC_USB_LP_CAN_RX0_IRQ);
}


static void usb_ints_disable(void)
{
	nvic_disable_irq(NVIC_USB_WAKEUP_IRQ);
	nvic_disable_irq(NVIC_USB_HP_CAN_TX_IRQ);
	nvic_disable_irq(NVIC_USB_LP_CAN_RX0_IRQ);
}



static int cdcacm_control_request(usbd_device *usbd_dev, struct usb_setup_data *req, uint8_t **buf,
		uint16_t *len, void (**complete)(usbd_device *usbd_dev, struct usb_setup_data *req))
{
	(void)complete;
	(void)buf;
	(void)usbd_dev;

	switch (req->bRequest) {
	case USB_CDC_REQ_SET_CONTROL_LINE_STATE: {
		/*
		 * This Linux cdc_acm driver requires this to be implemented
		 * even though it's optional in the CDC spec, and we don't
		 * advertise it in the ACM functional descriptor.
		 */
		char local_buf[10];
		struct usb_cdc_notification *notif = (void *)local_buf;

		/* We echo signals back to host as notification. */
		notif->bmRequestType = 0xA1;
		notif->bNotification = USB_CDC_NOTIFY_SERIAL_STATE;
		notif->wValue = 0;
		notif->wIndex = 0;
		notif->wLength = 2;
		local_buf[8] = req->wValue & 3;
		local_buf[9] = 0;
		// usbd_ep_write_packet(0x83, buf, 10);
		return 1;
		}
	case USB_CDC_REQ_SET_LINE_CODING:
		if (*len < sizeof(struct usb_cdc_line_coding))
			return 0;
		return 1;
	}
	return 0;
}

static void cdcacm_data_rx_cb(usbd_device *usbd_dev, uint8_t ep)
{
	(void)ep;
	(void)usbd_dev;

	// avoid that this function is invoked more than once by an interrupt
	usb_ints_disable();

	char buf[USB_XFER_BUFFER_SIZE];
	int len = usbd_ep_read_packet(usbd_dev, 0x01, buf, USB_XFER_BUFFER_SIZE);

	if (len) {
		usbacm_recv_handler(buf, len);
		//usbd_ep_write_packet(usbd_dev, 0x82, buf, len);
	}

	usb_ints_enable();
}

static void cdcacm_set_config(usbd_device *usbd_dev, uint16_t wValue)
{
	(void)wValue;
	(void)usbd_dev;

	usbd_ep_setup(usbd_dev, 0x01, USB_ENDPOINT_ATTR_BULK, USB_XFER_BUFFER_SIZE, cdcacm_data_rx_cb);
	usbd_ep_setup(usbd_dev, 0x82, USB_ENDPOINT_ATTR_BULK, USB_XFER_BUFFER_SIZE, NULL);
	usbd_ep_setup(usbd_dev, 0x83, USB_ENDPOINT_ATTR_INTERRUPT, 16, NULL);

	usbd_register_control_callback(
				usbd_dev,
				USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
				USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
				cdcacm_control_request);
}


static void usb_trigger_rescan(void)
{
	// Trigger rescan by forcing usb data pin to low for some time
	rcc_periph_clock_enable(RCC_GPIOA);
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO12);
	gpio_clear(GPIOA, GPIO12);
	
	delay_ticks(2);

	gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO12);
}



void usbacm_init(void)
{
	usbd_device *usbd_dev;

	usb_trigger_rescan();


	usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev, &config, usb_strings, 3, usbd_control_buffer, sizeof(usbd_control_buffer));

	acm_usb_device = usbd_dev;

	usbd_register_set_config_callback(usbd_dev, cdcacm_set_config);

	// Medium prio for usb interrupts
	nvic_set_priority(NVIC_USB_WAKEUP_IRQ, USBACM_INT_PRIO);
	nvic_set_priority(NVIC_USB_HP_CAN_TX_IRQ, USBACM_INT_PRIO);
	nvic_set_priority(NVIC_USB_LP_CAN_RX0_IRQ, USBACM_INT_PRIO);

	usb_ints_enable();
}


void usbacm_send(const void *data, uint32_t len)
{
	usb_ints_disable();

	static uint8_t usb_send_was_timeout = 0;

	while (len)
	{
		uint32_t cur = len;
		if (cur > USB_XFER_BUFFER_SIZE)
		{
			cur = USB_XFER_BUFFER_SIZE;
		}

		uint8_t timeout = 0;
		volatile uint32_t usb_tx_start_tick = tickcount();
		while (usbd_ep_write_packet(acm_usb_device, 0x82, data, cur) == 0)
		{
			// Immediately timeout, if the last send timed out
			if (usb_send_was_timeout || tickcount() - usb_tx_start_tick > USB_TX_TIMEOUT_TICKS )
			{
				timeout = 1;
				break;
			}

			// Handle interrupts
			usb_ints_enable();
			usb_ints_disable();
		}

		usb_send_was_timeout = timeout;

		if (timeout)
		{
			break;
		}

		len -= cur;
		data += cur;
	}

	usb_ints_enable();
}


void usb_hp_can_tx_isr(void)
{
	usbd_poll(acm_usb_device);
}

void usb_lp_can_rx0_isr(void)
{
	usbd_poll(acm_usb_device);
}

void usb_wakeup_isr(void)
{
	usbd_poll(acm_usb_device);
}