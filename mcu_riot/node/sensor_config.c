#ifndef MASTER

// For now, hard-code the sued page, this is the last one (one page is 1kb)
// Obviously, this block MUST NOT CONTAIN any program data
#define CONFIG_FLASH_PAGE 63
#define CONFIG_MAGIC 0xDEADBEEF

#include "sensor_config.h"
#include <periph/flashpage.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utils.h"

/*
 * The config data as stored in the flash.
 * This contains a magic number, if the config is read and the magic number does not match,
 * the node is unconfigured.
 */
typedef struct
{
	uint32_t magic;
	sensor_configuration_t data;
} config_with_magic_t;

/*
 * The data needs to be padded so that is fills an entire flash page.
 * Otherwise i can't write the flash because the write funtion expects full pages.
 */
typedef struct
{
	config_with_magic_t cfg;
	uint8_t filler[FLASHPAGE_SIZE -  sizeof(config_with_magic_t)];
} on_flash_config_t;


const sensor_configuration_t *sensor_config_get(void)
{
	config_with_magic_t *cfg = flashpage_addr(CONFIG_FLASH_PAGE);
	if (cfg->magic != CONFIG_MAGIC)
	{
		puts("WRONG CONFIG MAGIC!\n");
		return NULL;
	}

	return &cfg->data;

}


int sensor_config_set_cmd(int argc, char **argv)
{
	/*
	 * Sadly the current version of the RIOT flash api only lets me write the whole page. (expects the full page data)
	 * So i need a buffer that can contain the whole page. I don't want this large buffer on the stack of the caller so i have as
	 * static buffer.
	 */
	static on_flash_config_t config_buffer;
	_Static_assert(sizeof(config_buffer) == FLASHPAGE_SIZE, "Config buffer must have flash page size");


	if (argc != 4)
	{
		puts("USAGE: config <node_id> <key_status> <key_config>\n");
		puts("node_id     The id of this node (decimal)");
		puts("key_status  Key used for status messages (outgoing traffic)");
		puts("key_config  Key used for config messages (incoming traffic)");
		puts("Both keys must be exactly 128 bit long and encoded in hex format.");
		return 1;
	}

	if (strlen(argv[2]) != AUTH_KEY_LEN * 2 || !utils_hex_decode(argv[2], AUTH_KEY_LEN * 2, config_buffer.cfg.data.key_status))
	{
		puts("Invalid status key, expected status key to be 32 hex chars (128 bit)!");
		return 1;
	}

	if (strlen(argv[3]) != AUTH_KEY_LEN * 2 || !utils_hex_decode(argv[3], AUTH_KEY_LEN * 2, config_buffer.cfg.data.key_config))
	{
		puts("Invalid config key, expected status key to be 32 hex chars (128 bit)!");
		return 1;
	}

	config_buffer.cfg.data.my_id = atoi(argv[1]);

	config_buffer.cfg.magic = CONFIG_MAGIC;

	if (flashpage_write_and_verify(CONFIG_FLASH_PAGE, &config_buffer) != FLASHPAGE_OK)
	{
		puts("Flash verification failed!");
		return 1;
	}

	printf("OK, Config updated, node id is now: %u!\n", config_buffer.cfg.data.my_id);
	return 0;

}

#endif
