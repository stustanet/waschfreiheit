/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#include "storage_manager.h"

#include <string.h>
#include <stdlib.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "tinyprintf.h"
#include "led_status.h"
#include "usb_storage_helper.h"
#include "debug_command_queue.h"
#include "debug_file_logger.h"
#include "ff.h"


// Stack size of the LED thread (in words)
#define MOUNT_THD_STACK_SIZE 512

static StaticTask_t mount_thd_buffer;;
static StackType_t mount_thd_stack[MOUNT_THD_STACK_SIZE];


SemaphoreHandle_t global_fs_mutex;
static StaticSemaphore_t global_fs_mutex_buffer;

// This is the global instance of the fatfs
static FATFS usb_fat_fs;
static volatile bool usb_storage_mounted = false;
static volatile bool usb_storage_disable = false;


static void mount_thread(void *arg)
{
	(void)arg;

	const uint32_t DELAY_IDLE = 1000;

    TickType_t last = xTaskGetTickCount();
	while (1)
	{
		if (!usb_storage_ok())
		{
			usb_storage_disable = false;
		}

		if (!usb_storage_disable && (usb_storage_mounted != usb_storage_ok()))
		{
			xSemaphoreTake(global_fs_mutex, portMAX_DELAY);
			if (usb_storage_mounted)
			{
				// Device removed without unmount
				printf("Removed USB storage without unmount!\n");
				f_mount(NULL, "", 0);
				usb_storage_mounted = false;
			}
			else
			{
				FRESULT res = f_mount(&usb_fat_fs, "", 1);
				if (res != FR_OK)
				{
					printf("Mounting the USB storage failed with error %i\n", res);
					usb_storage_disable = true;
					led_status_system(LED_STATUS_SYSTEM_USB_MOUNT_ERR);
				}
				else
				{
					usb_storage_mounted = true;
				}
			}
			xSemaphoreGive(global_fs_mutex);

			if (usb_storage_mounted)
			{
				printf("Mounted USB storage\n");

				led_status_system(LED_STATUS_SYSTEM_USB_MOUNT_OK);
				if (debug_command_queue_load(COMMAND_QUEUE_AUTOEXEC))
				{
					led_status_system(LED_STATUS_SYSTEM_USB_AUTOEXEC);
				}
			}
		}
		else if (usb_storage_disable && usb_storage_mounted)
		{
			printf("Unmounting USB device...");
			debug_file_logger_set_file(NULL, false);

			xSemaphoreTake(global_fs_mutex, portMAX_DELAY);
			f_mount(NULL, "", 0);
			usb_storage_mounted = false;
			xSemaphoreGive(global_fs_mutex);
			printf("Unmounted!\n");
			led_status_system(LED_STATUS_SYSTEM_USB_DISCON);
		}

		vTaskDelayUntil(&last, DELAY_IDLE);
	}
}


void storage_manager_init(void)
{
	debug_file_logger_init();
	usb_storage_init();

	global_fs_mutex = xSemaphoreCreateMutexStatic(&global_fs_mutex_buffer);

	xTaskCreateStatic(
		&mount_thread,
		"USB_MOUNT",
		MOUNT_THD_STACK_SIZE,
		NULL,
		tskIDLE_PRIORITY,
		mount_thd_stack,
		&mount_thd_buffer);
}


void storage_manager_cmd_configure_logger(int argc, char **argv)
{
	if (argc == 1)
	{
		printf("Configures the USB storage logging.\n"
			   "USAGE: logger open_file PATH MODE\n"
			   "       logger set_opt   TYPE OPTIONS\n"
			   "       logger mark      TEXT\n"
			   "       logger close\n\n"
			   "open_file  Opens the file with the specified <PATH>\n"
			   "           This will fail if there is no USB storage with a FAT\n"
			   "           filesystem connected.\n"
			   "           <MODE> must be one of the following:\n"
			   "             \"new\"    Overwrite if existing\n"
			   "             \"append\" Append if existing\n\n"
			   "set_opt    Configure / enable a specific logger\n"
			   "           See the table below for available TYPEs and OPTIONS\n"
			   "           OPTIONS is an unsigned hexadecimal number.\n"
			   "mark       Put some string mark into the log\n\n"
			   "close      Close the logfile\n\n"
			   "Logging types:\n"
			   "ADC  ADC raw data logging\n"
			   "     OPTIONS is a bitfield to specify the logged channels\n"
			   "SE   State estimation logging\n"
			   "     OPTIONS is a bitfield to specify the logged channels\n"
			   "NW   Network message logging\n"
			   "     OPTIONS is a bitfield with the following values:\n"
			   "        1 Log incoming messages\n"
			   "        2 Log outgoing messages\n");
		return;
	}

	if (strcmp(argv[1], "open_file") == 0)
	{
		if (argc != 4)
		{
			printf("Invalid number of arguments!\n");
			return;
		}
		if (!usb_storage_mounted)
		{
			printf("No USB storage connected or not mounted.\n");
			return;
		}

		bool append;
		if (strcmp(argv[3], "new") == 0)
		{
			append = false;
		}
		else if (strcmp(argv[3], "append") == 0)
		{
			append = true;
		}
		else
		{
			printf("Invalid file open mode \"%s\".\n", argv[3]);
			return;
		}

		printf("Set logfile to \"%s\", append=%u.\n", argv[2], append);
		debug_file_logger_set_file(argv[2], append);
	}
	else if (strcmp(argv[1], "set_opt") == 0)
	{
		if (argc != 4)
		{
			printf("Invalid number of arguments!\n");
			return;
		}
		enum DEBUG_LOG_TYPE type;
		if (strcmp(argv[2], "ADC") == 0)
		{
			type = DEBUG_LOG_TYPE_ADC;
		}
		else if (strcmp(argv[2], "SE") == 0)
		{
			type = DEBUG_LOG_TYPE_STATEEST;
		}
		else if (strcmp(argv[2], "NW") == 0)
		{
			type = DEBUG_LOG_TYPE_NETWORK;
		}
		else
		{
			printf("Invalid logger type \"%s\".\n", argv[2]);
			return;
		}

		uint32_t option = strtoul(argv[3], NULL, 16);

		printf("Set logging opt \"%s\" to %lu.\n", argv[2], option);
		debug_file_logger_configure(type, option);
	}
	else if (strcmp(argv[1], "mark") == 0)
	{
		if (argc != 3)
		{
			printf("Invalid number of arguments!\n");
			return;
		}
		printf("Set log marker \"%s\".\n", argv[2]);
		debug_file_logger_log_marker(argv[2]);
	}
	else if (strcmp(argv[1], "close") == 0)
	{
		printf("Closing logfile\n");
		debug_file_logger_set_file(NULL, false);
	}
	else
	{
		printf("Invalid option \"%s\"!\n", argv[1]);
	}
}


void storage_manager_cmd_umount(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	if (!usb_storage_mounted)
	{
		printf("No USB storage connected or not mounted.\n");
		return;
	}

	printf("Requesting unmount...\n");
	usb_storage_disable = true;
}
