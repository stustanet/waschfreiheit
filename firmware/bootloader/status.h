/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#include <stdint.h>

enum STATUS_TYPE
{
	STATUS_INIT,
	STATUS_WAIT_USB,
	STATUS_USB_TIMEOUT,
	STATUS_USB_OK,
	STATUS_MOUNT_ERROR,
	STATUS_NO_CHECKSUM,
	STATUS_NO_FIRMWARE,
	STATUS_IMAGE_TOO_LARGE,
	STATUS_WRONG_CHECKSUM,
	STATUS_SAME_FIRMWARE,
	STATUS_STORAGE_ERROR,
	STATUS_VERIFICATION_ERROR,
	STATUS_FLASH_SUCCESS,
	STATUS_JUMP_TO_APPLICATION
};

enum PROGRESS_TYPE
{
	PROGRESS_READ,
	PROGRESS_ERASE,
	PROGRESS_WRITE,
	PROGRESS_VERIFY
};


void status_init(void);
void status_update(enum STATUS_TYPE status);
void status_progress(enum PROGRESS_TYPE type, int percent);
