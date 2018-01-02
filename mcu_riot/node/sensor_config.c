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
#include "sensor_config_defaults.h"
#include "utils.h"

static const uint32_t CONFIG_NETWORK    = 1;
static const uint32_t CONFIG_COLORTABLE = 2;

/*
 * The config data as stored in the flash.
 * This contains a magic number, if the config is read and the magic number does not match,
 * the node is unconfigured.
 */
typedef struct
{
	uint32_t magic;
	uint32_t config_set;
	sensor_configuration_t network;
	color_table_t colortable;
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

	if ((cfg->config_set & CONFIG_NETWORK) == 0)
	{
		puts("NETWORK NOT CONFIGURED!\n");
		return NULL;
	}
	return &cfg->network;

}


const color_table_t *sensor_config_color_table(void)
{
	static const color_table_t defaultTable = DefaultColorMap;
	config_with_magic_t *cfg = flashpage_addr(CONFIG_FLASH_PAGE);
	if (cfg->magic != CONFIG_MAGIC)
	{
		puts("Node not configured -> Use default colormap!\n");
		return &defaultTable;
	}

	if ((cfg->config_set & CONFIG_COLORTABLE) == 0)
	{
		puts("Colortable not configured -> Use default colormap!\n");
		return &defaultTable;
	}
	return &cfg->colortable;
}

static int network_config(int argc, char **argv, config_with_magic_t *cfg)
{
	if (argc != 5)
	{
		puts("USAGE: config network <node_id> <key_status> <key_config>\n");
		puts("node_id     The id of this node (decimal)");
		puts("key_status  Key used for status messages (outgoing traffic)");
		puts("key_config  Key used for config messages (incoming traffic)");
		puts("Both keys must be exactly 128 bit long and encoded in hex format.");
		return 1;
	}

	if (strlen(argv[3]) != AUTH_KEY_LEN * 2 || !utils_hex_decode(argv[3], AUTH_KEY_LEN * 2, cfg->network.key_status))
	{
		puts("Invalid status key, expected status key to be 32 hex chars (128 bit)!");
		return 1;
	}

	if (strlen(argv[4]) != AUTH_KEY_LEN * 2 || !utils_hex_decode(argv[4], AUTH_KEY_LEN * 2, cfg->network.key_config))
	{
		puts("Invalid config key, expected status key to be 32 hex chars (128 bit)!");
		return 1;
	}

	cfg->network.my_id = atoi(argv[2]);

	// set "network configured" bit
	cfg->config_set |= CONFIG_NETWORK;

	return 0;
}

static int color_config(int argc, char **argv, config_with_magic_t *cfg)
{
	_Static_assert(sizeof(cfg->colortable) == sizeof(cfg->colortable[0]) * 16, "Colortable must have exactly 16 elements");
	if (argc != 18)
	{
		puts("USAGE: config color <r0,g0,b0> <r1,g1,b1> ... <r15,g15,b15>\n");
		puts("All 16 RBG values are set at once.");
		puts("Each RGB value is composed of 3 8-bit decimal numbers seperated by a comma.");
		return 1;
	}

	for (int i = 0; i < 16; i++)
	{
		if (!utils_parse_rgb(argv[i + 2], &cfg->colortable[i]))
		{
			printf("Invalid rgb value[%i]: \"%s\"\n", i, argv[i + 2]);
			return 1;
		}
	}

	// set "color configured" bit
	cfg->config_set |= CONFIG_COLORTABLE;
	return 0;
}


static int rf_config(int argc, char **argv, config_with_magic_t *cfg)
{
	puts("Not implemented yet!");
	return 1;
}


int sensor_config_set_cmd(int argc, char **argv)
{
	/*
	 * This is the buffer for read-write-modify operations on the config page.
	 */
	static on_flash_config_t config_buffer;
	_Static_assert(sizeof(config_buffer) == FLASHPAGE_SIZE, "Config buffer must have flash page size");

	if (argc < 2)
	{
		puts("USAGE: config <type> ...\n");
		puts("Configures this node.");
		puts("Config types:");
		puts("    network  Network settings");
		puts("    color    Colortable for the status LEDs");
		puts("    rf       Radio configuration");
		puts("    reset    Resets the whole config");
		return 1;
	}


	// copy old flash page into buffer
	memcpy(&config_buffer, flashpage_addr(CONFIG_FLASH_PAGE), sizeof(config_buffer));
	if (config_buffer.cfg.magic != CONFIG_MAGIC)
	{
		// invalid magic -> reset all "configured" bits
		config_buffer.cfg.config_set = 0;
	}

	int res;

	if (strcmp(argv[1], "network") == 0)
	{
		res = network_config(argc, argv, &config_buffer.cfg);
	}
	else if (strcmp(argv[1], "color") == 0)
	{
		res = color_config(argc, argv, &config_buffer.cfg);
	}
	else if (strcmp(argv[1], "rf") == 0)
	{
		res = rf_config(argc, argv, &config_buffer.cfg);
	}
	else if (strcmp(argv[1], "reset") == 0)
	{
		puts("Reset config!");
		// zero the whole config
		memset(&config_buffer, 0, sizeof(config_buffer));
		
		res = 0;
	}
	else
	{
		printf("Unknown config type: \"%s\"\n", argv[1]);
		return 1;
	}

	if (res != 0)
	{
		return res;
	}

	// set valid magic
	config_buffer.cfg.magic = CONFIG_MAGIC;

	if (flashpage_write_and_verify(CONFIG_FLASH_PAGE, &config_buffer) != FLASHPAGE_OK)
	{
		puts("Flash verification failed!");
		return 1;
	}

	puts("OK Config updated, changes have no effect until restart!");
	return 0;

}

#endif
