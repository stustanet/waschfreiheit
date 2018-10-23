/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#include "debug_command_queue.h"
#include <string.h>

#include <FreeRTOS.h>
#include <semphr.h>

#include "tinyprintf.h"
#include "ff.h"
#include "storage_manager.h"

#define COMMAND_BUFFER_LENGTH 256

static bool file_open = false;
static FIL current_file;

size_t command_buffer_offset;
size_t command_buffer_end;
static char command_buffer[COMMAND_BUFFER_LENGTH];

const char * QUEUE_TO_FILE[] =
{
	"AUTOEXEC.TXT",
	"USER.TXT"
};


bool debug_command_queue_load(enum COMMAND_QUEUE queue)
{
	if (queue >= sizeof(QUEUE_TO_FILE) / sizeof(QUEUE_TO_FILE[0]))
	{
		return false;
	}

	xSemaphoreTake(global_fs_mutex, portMAX_DELAY);
	if (file_open)
	{
		f_close(&current_file);
	}

	file_open = false;

	FRESULT res = f_open(&current_file, QUEUE_TO_FILE[queue], FA_READ | FA_OPEN_EXISTING);

	if (res != FR_OK)
	{
		xSemaphoreGive(global_fs_mutex);
		printf("Failed to open command file \"%s\"\n", QUEUE_TO_FILE[queue]);
		return false;
	}

	command_buffer_offset = 0;
	command_buffer_end = 0;
	file_open = true;

	xSemaphoreGive(global_fs_mutex);

	printf("Opened command file \"%s\"\n", QUEUE_TO_FILE[queue]);

	return true;
}


bool debug_command_queue_running(void)
{
	return file_open;
}


char *debug_command_queue_next(void)
{
	xSemaphoreTake(global_fs_mutex, portMAX_DELAY);

	if (!file_open)
	{
		xSemaphoreGive(global_fs_mutex);
		return NULL;
	}

	if (command_buffer_end != 0)
	{
		// rotate the command buffer so that the old data starts at the beginning
		command_buffer_end -= command_buffer_offset;

		if (command_buffer_end != 0)
		{
			memmove(command_buffer, command_buffer + command_buffer_offset, command_buffer_end);
		}
	}


	// re-fill the command buffer with new data
	UINT br;
	FRESULT res = f_read(&current_file, command_buffer + command_buffer_end, (sizeof(command_buffer) - 1) - command_buffer_end, &br);

	command_buffer_end += br;

	if (res != FR_OK || command_buffer_end == 0)
	{
		f_close(&current_file);
		file_open = false;
		command_buffer_offset = 0;
		command_buffer_end = 0;
		xSemaphoreGive(global_fs_mutex);

		if (res != FR_OK)
		{
			printf("Failed to read command file\n");
		}
		return NULL;
	}


	for (command_buffer_offset = 0; command_buffer_offset < command_buffer_end; command_buffer_offset++)
	{
		if (command_buffer[command_buffer_offset] == '\r' ||
			command_buffer[command_buffer_offset] == '\n')
		{
			break;
		}
	}

	command_buffer[command_buffer_offset] = '\0';
	command_buffer_offset++;
	xSemaphoreGive(global_fs_mutex);
	return command_buffer;
}
