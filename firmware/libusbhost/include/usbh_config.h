/*
 * This file is part of the libusbhost library
 * hosted at http://github.com/libusbhost/libusbhost
 *
 * Copyright (C) 2015 Amir Hammad <amir.hammad@hotmail.com>
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

#ifndef USBH_CONFIG_
#define USBH_CONFIG_



// Max devices per hub
#define USBH_HUB_MAX_DEVICES	(1)

// Max number of hub instancies
#define USBH_MAX_HUBS		(0)

// Max devices
#define USBH_MAX_DEVICES		1

// Min: 128
// Set this wisely
#define BUFFER_ONE_BYTES	(2048)

// Bulkonly Storage
#define USBH_BULKONLY_STORAGE_MAX_DEVICES 1

// A command block is *ALWAYS* 31 bytes long, so there is no need to make this any larger
#define USBH_BULKONLY_STORAGE_CMD_BUFFER 31
// The data buffer is only used during the initialization so I want to keep it reasonably small
// (I also want the size of both buffers to be 4 byte aligned to not waste any space due to alignment)
#define USBH_BULKONLY_STORAGE_DATA_BUFFER 57

#define USBH_BULKONLY_STORAGE_SUPPORT_64_ADDRESS 0
#define USBH_BULKONLY_STORAGE_READONLY 0

/* Sanity checks */
#if (USBH_MAX_DEVICES > 127)
#error USBH_MAX_DEVICES > 127
#endif

#endif
