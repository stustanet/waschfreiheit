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

#ifndef USBH_DRIVER_BULKONLY_STORAGE_
#define USBH_DRIVER_BULKONLY_STORAGE_

#include "usbh_core.h"

#include <stdint.h>

BEGIN_DECLS

#define BULKONLY_STORAGE_TX_RESULT_OK 0
#define BULKONLY_STORAGE_TX_RESULT_FAILED 1

typedef struct _bulkonly_storage_callbacks {
	void (*connect)(uint8_t device_id);     // New device is connected (No commands shall be sent utili the device is ready)
	void (*disconnect)(uint8_t device_id);  // Device is disconnected
	void (*error)(uint8_t device_id, uint8_t error_cnt); // Transfer error for the device. No new commands shall be sent until the device is ready again.
	void (*ready)(uint8_t device_id, uint64_t num_of_sectors, uint32_t bytes_per_sector); // The device is ready now.
	void (*tx_done)(uint8_t device_id, uint8_t result); // A transmission is completed.
} bulkonly_storage_callbacks_t;


void bulkonly_storage_driver_init(const bulkonly_storage_callbacks_t *callbacks);


/*
 * Read / write a number of sectors.
 * If the return value is false, the call has failed and neither the tx_done callback nor the error callback will be called.
 */
bool bulkonly_storage_read(uint8_t device_id, uint64_t sector_first, uint32_t sector_count, void *buffer);
bool bulkonly_storage_write(uint8_t device_id, uint64_t sector_first, uint32_t sector_count, const void *buffer);

extern const usbh_dev_driver_t usbh_bulkonly_storage_driver;

END_DECLS

#endif
