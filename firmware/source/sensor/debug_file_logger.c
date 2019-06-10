/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#include "debug_file_logger.h"

#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <FreeRTOS.h>
#include <semphr.h>

#include "tinyprintf.h"
#include "ff.h"
#include "storage_manager.h"

#define LOG_BUFFER_SIZE 4096

static bool     logfile_is_open = false;
static FIL      logfile;
static uint8_t  log_buffer[LOG_BUFFER_SIZE];
static size_t   log_buffer_used = 0;
static uint32_t log_options[DEBUG_LOG_TYPE_NUMOF] = { 0 };
static SemaphoreHandle_t log_mutex = 0;
static StaticSemaphore_t log_mutex_buffer;
static uint32_t log_number = 0;


static void flush_buffer(void)
{
	if (!logfile_is_open)
	{
		log_buffer_used = 0;
		return;
	}

	UINT bw;
	FRESULT res = f_write(&logfile, log_buffer, log_buffer_used, &bw);


	if (res != FR_OK || bw != log_buffer_used)
	{
		printf("Failed to flush log buffer, result=%i; bw=%u; used=%u\n", res, bw, log_buffer_used);
		logfile_is_open = false;
	}

	f_sync(&logfile);

	log_buffer_used = 0;
}


static void write_buffered(const uint8_t *data, size_t len)
{
	while (len)
	{
		// copy data to log buffer
		size_t current = len;
		if (log_buffer_used + len > sizeof(log_buffer))
		{
			current = sizeof(log_buffer) - log_buffer_used;
		}

		memcpy(log_buffer + log_buffer_used, data, current);
		log_buffer_used += current;
		len -= current;
		data += current;

		if (log_buffer_used == sizeof(log_buffer))
		{
			xSemaphoreTake(global_fs_mutex, portMAX_DELAY);
			flush_buffer();
			xSemaphoreGive(global_fs_mutex);
		}
	}
}


static bool check_log_bitmask_nonempty(uint32_t mask, uint8_t cnt)
{
	if (!logfile_is_open || mask == 0)
	{
		return false;
	}

	// check if we have a val to log
	for (uint8_t i = 0; i < cnt; i++)
	{
		if (mask & (1 << i))
		{
			return true;
		}
	}
	return false;
}


static void write_log_entry_begin(char id)
{
	char buffer[16];
	int n = sprintf(buffer,
					"\n%lu %c ",  // max 15 bytes
					log_number,
					id);

	write_buffered((uint8_t *)buffer, n);

	log_number++;
}


static void write_uint16_kv(const uint16_t *vals, uint8_t cnt, uint32_t mask)
{
	for (uint8_t i = 0; i < cnt; i++)
	{
		if (mask & (1 << i))
		{
			char buffer[16];

			int n = sprintf(buffer,
							" %u=%u;",  // max 11 bytes
							i,
							vals[i]);

			write_buffered((uint8_t *)buffer, n);
		}
	}
}


static void write_hexdump(const uint8_t *data, size_t len)
{
	char buffer[8];
	buffer[0] = ' ';
	buffer[1] = ' ';
	for (size_t i = 0; i < len; i++)
	{
		sprintf(buffer,
				"  %02X",  // always 5 bytes
				data[i]);
		if (i && i % 4 == 0)
		{
			// full string
			write_buffered((uint8_t *)buffer, 4);
		}
		else
		{
			// skip the first space
			write_buffered((uint8_t *)buffer + 1, 3);
		}
	}
}


void debug_file_logger_init(void)
{
	log_mutex = xSemaphoreCreateMutexStatic(&log_mutex_buffer);
}


bool debug_file_logger_set_file(const char *file, bool append)
{
	xSemaphoreTake(global_fs_mutex, portMAX_DELAY);
	if (logfile_is_open)
	{
		flush_buffer();
		f_close(&logfile);
		logfile_is_open = false;
	}

	if (!file || !file[0])
	{
		xSemaphoreGive(global_fs_mutex);
		return true;
	}

	FRESULT res = f_open(&logfile, file, FA_WRITE | (append ? FA_OPEN_APPEND : FA_CREATE_ALWAYS));

	if (res != FR_OK)
	{
		printf("Failed to create log file: result=%i\n", res);
	}
	else
	{
		logfile_is_open = true;
	}

	xSemaphoreGive(global_fs_mutex);

	xSemaphoreTake(log_mutex, portMAX_DELAY);

	write_log_entry_begin('*');
	write_buffered((const uint8_t *)"====== LOG BEGIN ======", 23);

	xSemaphoreGive(log_mutex);

	return res == FR_OK;
}


void debug_file_logger_configure(enum DEBUG_LOG_TYPE what, uint32_t options)
{
	if (what >= DEBUG_LOG_TYPE_NUMOF)
	{
		return;
	}

	log_options[what] = options;
}


void debug_file_logger_log_raw_adc(const uint16_t *vals, uint8_t cnt)
{
	if (!check_log_bitmask_nonempty(log_options[DEBUG_LOG_TYPE_ADC], cnt))
	{
		return;
	}

	xSemaphoreTake(log_mutex, portMAX_DELAY);

	write_log_entry_begin('A');
	write_uint16_kv(vals, cnt, log_options[DEBUG_LOG_TYPE_ADC]);

	xSemaphoreGive(log_mutex);
}


void debug_file_logger_log_filtered_adc(const uint16_t *vals, uint8_t cnt)
{
	if (!check_log_bitmask_nonempty(log_options[DEBUG_LOG_TYPE_ADC] >> 16, cnt))
	{
		return;
	}

	xSemaphoreTake(log_mutex, portMAX_DELAY);

	write_log_entry_begin('F');
	write_uint16_kv(vals, cnt, log_options[DEBUG_LOG_TYPE_ADC] >> 16);

	xSemaphoreGive(log_mutex);
}


void debug_file_logger_log_stateest(uint8_t channel, uint8_t new_state)
{
	if (!(logfile_is_open &&
		  (log_options[DEBUG_LOG_TYPE_STATEEST] & (1 << channel))))
	{
		return;
	}

	xSemaphoreTake(log_mutex, portMAX_DELAY);

	write_log_entry_begin('S');

	char buffer[16];
	int n = sprintf(buffer,
					"%u: %u",  // max 8 bytes
					channel,
					new_state);

	write_buffered((const uint8_t *)buffer, n);

	xSemaphoreGive(log_mutex);
}


void debug_file_logger_log_network_packet(const void *data, uint8_t len, bool tx, uint8_t rssi, int8_t snr)
{
	if (!(logfile_is_open &&
		  (((log_options[DEBUG_LOG_TYPE_NETWORK] & DEBUG_FILE_LOGGER_OPTION_NW_RX) && !tx) ||
		   ((log_options[DEBUG_LOG_TYPE_NETWORK] & DEBUG_FILE_LOGGER_OPTION_NW_TX) && tx))))
	{
		return;
	}

	xSemaphoreTake(log_mutex, portMAX_DELAY);

	if (tx)
	{
		write_log_entry_begin('T');
	}
	else
	{
		write_log_entry_begin('R');

		char buffer[32];
		int n = sprintf(buffer,
						"RSSI=%u; SNR=%i\n",  // max 19 bytes
						rssi,
						snr);
		write_buffered((const uint8_t *)buffer, n);
	}


	write_hexdump((const uint8_t *)data, len);
	xSemaphoreGive(log_mutex);
}


void debug_file_logger_log_marker(const char *str)
{
	if (!logfile_is_open)
	{
		return;
	}

	size_t len = strlen(str);

	xSemaphoreTake(log_mutex, portMAX_DELAY);

	write_log_entry_begin('M');
	write_buffered((const uint8_t *)str, len);

	xSemaphoreGive(log_mutex);
}


bool debug_file_logger_is_open(void)
{
	return logfile_is_open;
}


uint32_t debug_file_logger_get_opt(enum DEBUG_LOG_TYPE what)
{
	if (what >= DEBUG_LOG_TYPE_NUMOF)
	{
		return 0;
	}

	return log_options[what];
}
