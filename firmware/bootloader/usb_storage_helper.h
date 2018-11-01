/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

enum USB_READ_RESULT
{
	USB_READ_OK,
	USB_READ_OUT_OF_BOUNDS,
	USB_READ_ERROR,
	USB_READ_FATAL
};

enum USB_READ_RESULT usb_storage_read_wait(uint64_t start, uint32_t size, void *ptr);
uint32_t usb_storage_sector_size(void);
uint64_t usb_storage_sector_count(void);

bool usb_storage_wait(uint32_t timeout_ms);
bool usb_storage_ok(void);

// initialiyes the USB stack
void usb_storage_init(void);

// Should be called every ms form an interrupt
void usb_storage_poll(void);
