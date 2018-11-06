/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

enum USB_RESULT
{
	USB_RESULT_OK,
	USB_RESULT_OUT_OF_BOUNDS,
	USB_RESULT_ERROR,
	USB_RESULT_FATAL
};

#define USB_TIMEOUT_MS 30000
#define USB_MAX_ERROR_CNT 5

enum USB_RESULT usb_storage_read_wait(uint64_t start, uint32_t size, void *ptr);
enum USB_RESULT usb_storage_write_wait(uint64_t start, uint32_t size, const void *ptr);
uint32_t usb_storage_sector_size(void);
uint64_t usb_storage_sector_count(void);

bool usb_storage_ok(void);

// initializes the USB stack
void usb_storage_init(void);
