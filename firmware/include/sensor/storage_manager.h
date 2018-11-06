/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#include <FreeRTOS.h>
#include <semphr.h>

/*
 * Global mutex for filesystem access on the usb storage.
 * I use this instead of the built-in sync funtions of the FatFS because
 * the integrated sync does not sync the mount/unmout operations.
 * Also with the built-in sync, the buffer for LFNs must be on the stack (or the heap).
 */
extern SemaphoreHandle_t global_fs_mutex;

void storage_manager_init(void);

void storage_manager_cmd_configure_logger(int argc, char **argv);
void storage_manager_cmd_umount(int argc, char **argv);
