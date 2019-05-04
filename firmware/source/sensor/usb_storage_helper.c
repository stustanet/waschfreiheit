/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#include "usb_storage_helper.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "usbh_core.h"
#include "usbh_lld_stm32f4.h"
#include "usbh_driver_bulkonly_storage.h"
#include "tinyprintf.h"
#include "led_status.h"

#define USB_FS_GPIO_PIN_DM GPIO11
#define USB_FS_GPIO_PIN_DP GPIO12
#define USB_FS_GPIO_AF GPIO_AF10
#define USB_FS_GPIO_PORT GPIOA
#define USB_FS_GPIO_RCC RCC_GPIOA

#define USB_POWERSW_GPIO_PIN_EN GPIO0
#define USB_POWERSW_GPIO_PORT GPIOB
#define USB_POWERSW_GPIO_RCC  RCC_GPIOB

// Stack size of the usb poll thread (in words)
#define USB_POLL_THD_STACK_SIZE 512

static StaticTask_t usb_poll_thd_buffer;
static StackType_t usb_poll_thd_stack[USB_POLL_THD_STACK_SIZE];

static SemaphoreHandle_t usb_poll_mutex;
static SemaphoreHandle_t usb_reentry_mutex;
static SemaphoreHandle_t usb_tx_semaphore;
static StaticSemaphore_t usb_poll_mutex_buffer;
static StaticSemaphore_t usb_reentry_mutex_buffer;
static StaticSemaphore_t usb_tx_semaphore_buffer;

static struct
{
	uint64_t num_of_sectors;
	uint32_t bytes_per_sector;
	volatile uint32_t timeout;
	enum USB_RESULT result;
	bool connected;
	bool tx_pending;
	bool busy;
	uint8_t error_cnt;
} usb_msc_status = {};


static void init_usb_driver(void);

#if defined(USE_STM32F4_USBH_DRIVER_HS)
#error USB HighSpeed is not supported
#endif


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
	usb_msc_status.busy = true;
	printf("USB CONNECTED");
	led_status_system(LED_STATUS_SYSTEM_USB_CON);
}

static void usb_msc_disconnect(uint8_t device_id)
{
    (void)device_id;
	printf("USB DISCONNECTED");
	usb_msc_status.connected = false;
	usb_msc_status.busy = false;
	if (usb_msc_status.tx_pending)
	{
		usb_msc_status.tx_pending = false;
		usb_msc_status.result = USB_RESULT_FATAL;
		xSemaphoreGive(usb_tx_semaphore);
	}

	led_status_system(LED_STATUS_SYSTEM_USB_DISCON);
}

static void usb_msc_error(uint8_t device_id, uint8_t error_cnt)
{
    (void)device_id;
	printf("USB ERROR");
	usb_msc_status.connected = false;
	usb_msc_status.busy = false;
	usb_msc_status.error_cnt = error_cnt;
	if (usb_msc_status.tx_pending)
	{
		usb_msc_status.tx_pending = false;
		usb_msc_status.result = USB_RESULT_FATAL;
		led_status_system(LED_STATUS_SYSTEM_USB_ERR);
		xSemaphoreGive(usb_tx_semaphore);
	}
}

static void usb_msc_ready(uint8_t device_id, uint64_t num_of_sectors, uint32_t bytes_per_sector)
{
    (void)device_id;
	usb_msc_status.connected = true;
	usb_msc_status.busy = false;
	usb_msc_status.num_of_sectors = num_of_sectors;
	usb_msc_status.bytes_per_sector = bytes_per_sector;
	led_status_system(LED_STATUS_SYSTEM_USB_OK);
}

static void usb_msc_tx_done(uint8_t device_id, uint8_t result)
{
    (void)device_id;
	usb_msc_status.busy = false;
	if (result == BULKONLY_STORAGE_TX_RESULT_OK)
	{
		usb_msc_status.result = USB_RESULT_OK;
	}
	else
	{
		usb_msc_status.result = USB_RESULT_ERROR;
	}

	if (usb_msc_status.tx_pending)
	{
		usb_msc_status.tx_pending = false;
		xSemaphoreGive(usb_tx_semaphore);
	}
}


static void usb_poll_thread(void *arg)
{
	(void)arg;

	while(1)
	{
		xSemaphoreTake(usb_poll_mutex, portMAX_DELAY);

		TickType_t now = xTaskGetTickCount();

		usbh_poll(now * (1000000 / configTICK_RATE_HZ));

		if (usb_msc_status.connected &&
		    !usb_msc_status.tx_pending)
		{
			// Nothing to wait for -> reset timeout
			usb_msc_status.timeout = now;
		}

		if ((now - usb_msc_status.timeout) > USB_TIMEOUT_MS ||
			usb_msc_status.error_cnt > USB_MAX_ERROR_CNT)
		{
			printf("Reset USB: timeout=%lu, err=%u\n", usb_msc_status.timeout, usb_msc_status.error_cnt);

			usb_msc_error(0, usb_msc_status.error_cnt);
			led_status_system(LED_STATUS_SYSTEM_USB_DISCON);

			// MAYBE power-cycle the USB?
			init_usb_driver();
		}

		xSemaphoreGive(usb_poll_mutex);

		if (usb_msc_status.busy)
		{
			// Pending operation -> only yield
			taskYIELD();
		}
		else
		{
			// No pending op -> poll every 1 ms
			vTaskDelay(1);
		}
	}
}


static void init_usb_driver(void)
{
	gpio_clear(USB_POWERSW_GPIO_PORT, USB_POWERSW_GPIO_PIN_EN);

	bulkonly_storage_callbacks_t boscb = {
		.connect = usb_msc_connect,
		.disconnect = usb_msc_disconnect,
		.error = usb_msc_error,
		.ready = usb_msc_ready,
		.tx_done = usb_msc_tx_done
	};
	bulkonly_storage_driver_init(&boscb);

	usbh_init(lld_drivers, device_drivers);

	usb_msc_status.timeout = xTaskGetTickCount();
	usb_msc_status.error_cnt = 0;

	vTaskDelay(100);
	gpio_set(USB_POWERSW_GPIO_PORT, USB_POWERSW_GPIO_PIN_EN);
}


static enum USB_RESULT usb_storage_tx(uint64_t start, uint32_t size, void *ptr, bool write)
{
	xSemaphoreTake(usb_poll_mutex, portMAX_DELAY);
	if (!usb_storage_ok())
	{
		xSemaphoreGive(usb_poll_mutex);
		return USB_RESULT_FATAL;
	}

	if (start + size < start ||
		start + size > usb_msc_status.num_of_sectors)
	{
		xSemaphoreGive(usb_poll_mutex);
		return USB_RESULT_OUT_OF_BOUNDS;
	}

	led_status_system(LED_STATUS_SYSTEM_USB_RW);

	xSemaphoreTake(usb_reentry_mutex, portMAX_DELAY);


	bool txres;
	if (write)
	{
		//printf("USB write: sector=%lu, size=%lu\n", (uint32_t)start, size);
		txres = bulkonly_storage_write(0, start, size, ptr);
	}
	else
	{
		//printf("USB read: sector=%lu, size=%lu\n", (uint32_t)start, size);
		txres = bulkonly_storage_read(0, start, size, ptr);
	}

	if (!txres)
	{
		printf("USB TX ERROR!\n");
		usb_msc_status.tx_pending = false;
		xSemaphoreGive(usb_poll_mutex);
		xSemaphoreGive(usb_reentry_mutex);

		return USB_RESULT_ERROR;
	}

	usb_msc_status.busy = true;
	usb_msc_status.tx_pending = true;

	xSemaphoreGive(usb_poll_mutex);

	// Wait for the semaphore
	// The timeout is handled by the poll thd
	xSemaphoreTake(usb_tx_semaphore, portMAX_DELAY);

	//printf("USB TX DONE\n");

	usb_msc_status.timeout = xTaskGetTickCount();

	enum USB_RESULT res = usb_msc_status.result;

	xSemaphoreGive(usb_reentry_mutex);
	return res;
}


enum USB_RESULT usb_storage_read_wait(uint64_t start, uint32_t size, void *ptr)
{
	return usb_storage_tx(start, size, ptr, false);
}


enum USB_RESULT usb_storage_write_wait(uint64_t start, uint32_t size, const void *ptr)
{
	return usb_storage_tx(start, size, (void *)ptr, true);
}


uint32_t usb_storage_sector_size(void)
{
	return usb_msc_status.bytes_per_sector;
}

uint64_t usb_storage_sector_count(void)
{
	return usb_msc_status.num_of_sectors;
}


bool usb_storage_ok(void)
{
	return usb_msc_status.connected;
}


void usb_storage_init(void)
{
	printf("Initializing USB host\n");

	rcc_periph_clock_enable(RCC_OTGFS);
	rcc_periph_clock_enable(USB_FS_GPIO_RCC);

	rcc_periph_clock_enable(USB_POWERSW_GPIO_RCC);

	gpio_mode_setup(USB_FS_GPIO_PORT,
					GPIO_MODE_AF,
					GPIO_PUPD_NONE,
					USB_FS_GPIO_PIN_DM | USB_FS_GPIO_PIN_DP);

	gpio_set_af(USB_FS_GPIO_PORT,
				USB_FS_GPIO_AF,
				USB_FS_GPIO_PIN_DM | USB_FS_GPIO_PIN_DP);

	gpio_mode_setup(USB_POWERSW_GPIO_PORT,
					GPIO_MODE_OUTPUT,
					GPIO_PUPD_NONE,
					USB_POWERSW_GPIO_PIN_EN);

	usb_poll_mutex = xSemaphoreCreateMutexStatic(&usb_poll_mutex_buffer);
	usb_reentry_mutex = xSemaphoreCreateMutexStatic(&usb_reentry_mutex_buffer);
	usb_tx_semaphore = xSemaphoreCreateBinaryStatic(&usb_tx_semaphore_buffer);

	init_usb_driver();

	xTaskCreateStatic(
		&usb_poll_thread,
		"USB",
	    USB_POLL_THD_STACK_SIZE,
		NULL,
		tskIDLE_PRIORITY,
		usb_poll_thd_stack,
		&usb_poll_thd_buffer);
}


void usbh_critical_error(void)
{
	printf("USBH CRITICAL ERROR!\n");
	usb_msc_status.error_cnt = ~0;
}
