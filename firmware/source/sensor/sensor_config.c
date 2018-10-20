/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


#ifndef MASTER

// For now, hard-code the used page.
// Obviously, this block MUST NOT CONTAIN any program data

#define FLASH_START 0x08000000
#define CONFIG_MAGIC 0xDEADBEEF

#ifdef WASCHV2
// Second page for new boards
#define CONFIG_FLASH_PAGE 1
#define CONFIG_FLASH_ADDR (FLASH_START + 16384)
#else

// Last page for legacy boards
#define CONFIG_FLASH_PAGE 63
#define CONFIG_FLASH_ADDR (FLASH_START + 1024 * CONFIG_FLASH_PAGE)
#endif

#include "sensor_config.h"
#include <string.h>
#include <stdlib.h>
#include <libopencm3/stm32/flash.h>
#include "sensor_config_defaults.h"
#include "sx127x.h"
#include "sx127x_config.h"
#include "utils.h"
#include "tinyprintf.h"

static const uint32_t CONFIG_NETWORK    = 0x00000001;
static const uint32_t CONFIG_COLORTABLE = 0x00000002;
static const uint32_t CONFIG_RF         = 0x00000004;
static const uint32_t CONFIG_MISC       = 0x00000008;

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
	sx127x_rf_config_t rf;
	misc_config_t misc;
} config_with_magic_t;


const sensor_configuration_t *sensor_config_get(void)
{
	config_with_magic_t *cfg = (config_with_magic_t *)CONFIG_FLASH_ADDR;
	if (cfg->magic != CONFIG_MAGIC)
	{
		printf("WRONG CONFIG MAGIC!\n");
		return NULL;
	}

	if ((cfg->config_set & CONFIG_NETWORK) == 0)
	{
		printf("NETWORK NOT CONFIGURED!\n");
		return NULL;
	}
	return &cfg->network;

}


const color_table_t *sensor_config_color_table(void)
{
	static const color_table_t defaultTable = DefaultColorMap;
	config_with_magic_t *cfg = (config_with_magic_t *)CONFIG_FLASH_ADDR;
	if (cfg->magic != CONFIG_MAGIC)
	{
		printf("Node not configured -> Use default colormap!\n");
		return &defaultTable;
	}

	if ((cfg->config_set & CONFIG_COLORTABLE) == 0)
	{
		printf("Colortable not configured -> Use default colormap!\n");
		return &defaultTable;
	}
	return &cfg->colortable;
}


const sx127x_rf_config_t *sensor_config_rf_settings(void)
{
	static const sx127x_rf_config_t defaultConfig = DefaultRFSettings;
	config_with_magic_t *cfg = (config_with_magic_t *)CONFIG_FLASH_ADDR;
	if (cfg->magic != CONFIG_MAGIC)
	{
		printf("Node not configured -> Use default rf config!\n");
		return &defaultConfig;
	}

	if ((cfg->config_set & CONFIG_RF) == 0)
	{
		printf("RF parameters not configured -> Use default!\n");
		return &defaultConfig;
	}
	return &cfg->rf;
}


const misc_config_t *sensor_config_misc_settings(void)
{
	static const misc_config_t defaultMisc = DefaultMiscSettings;
	config_with_magic_t *cfg = (config_with_magic_t *)CONFIG_FLASH_ADDR;
	if (cfg->magic != CONFIG_MAGIC)
	{
		printf("Node not configured -> Use default misc settings!\n");
		return &defaultMisc;
	}

	if ((cfg->config_set & CONFIG_MISC) == 0)
	{
		printf("Misc settings not configured -> Use default!\n");
		return &defaultMisc;
	}
	return &cfg->misc;
}


static int network_config(int argc, char **argv, config_with_magic_t *cfg)
{
	if (argc != 5)
	{
		printf("USAGE: config network <node_id> <key_status> <key_config>\n\n"
			   "node_id     The id of this node (decimal)\n"
			   "key_status  Key used for status messages (outgoing traffic)\n"
			   "key_config  Key used for config messages (incoming traffic)\n"
			   "Both keys must be exactly 128 bit long and encoded in hex format.\n");
		return 1;
	}

	if (strlen(argv[3]) != AUTH_KEY_LEN * 2 || !utils_hex_decode(argv[3], AUTH_KEY_LEN * 2, cfg->network.key_status))
	{
		printf("Invalid status key, expected status key to be 32 hex chars (128 bit)!\n");
		return 1;
	}

	if (strlen(argv[4]) != AUTH_KEY_LEN * 2 || !utils_hex_decode(argv[4], AUTH_KEY_LEN * 2, cfg->network.key_config))
	{
		printf("Invalid config key, expected status key to be 32 hex chars (128 bit)!\n");
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
	if (argc == 3 && strcmp(argv[2], "get") == 0)
	{
		const color_table_t *current = sensor_config_color_table();
		printf("Current color table:\n");
		for (int i = 0; i < 16; i++)
		{
			printf("%u,%u,%u ", (*current)[i].r, (*current)[i].g, (*current)[i].b);
		}
		printf("\n");
		return 2;
	}
	else if (argc != 18)
	{
		printf("USAGE: config color get\n"
			   "USAGE: config color r0,g0,b0 r1,g1,b2 ... r15,g15,b15\n\n"
			   "All 16 RBG values are set at once.\n"
			   "Each RGB value is composed of 3 8-bit decimal numbers seperated by a comma.\n");
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
	if (argc == 3 && strcmp(argv[2], "get") == 0)
	{
		const sx127x_rf_config_t *current = sensor_config_rf_settings();
		printf("Current rf configuration:\n%lu %u %u %u ",
		       current->frequency,
		       current->tx_power,
		       current->lora_spread_factor,
		       current->lora_coderate);
		switch (current->lora_bandwidth)
		{
			case 7: printf("125\n"); break;
			case 8: printf("250\n"); break;
			case 9: printf("500\n"); break;
		}
		return 2;
	}
	else if (argc != 7)
	{
		printf("USAGE: config rf get\n"
			   "USAGE: config rf <frequency> <tx_power> <spread_factor> <coderate> <bandwidth>\n\n"
			   "frequency     Carrier frequency of the LoRa modem\n"
			   "              Valid range: "
			   TOSTRING(SX127X_CONFIG_LORA_FREQUENCY_MIN)
			   " - "
			   TOSTRING(SX127X_CONFIG_LORA_FREQUENCY_MAX)
			   "\n"
			   "tx_power      LoRa modem tx power in dB\n"
			   "              Max value: " TOSTRING(SX127X_CONFIG_LORA_POWER_MAX) "\n"
			   "spread_factor LoRa spreading factor\n"
			   "              Valid range: "
			   TOSTRING(SX127X_CONFIG_LORA_SPREAD_MIN)
			   " - "
			   TOSTRING(SX127X_CONFIG_LORA_SPREAD_MAX)
			   "\n"
			   "coderate      LoRa coderate\n"
			   "              Max value: " TOSTRING(SX127X_CONFIG_LORA_CODERATE_MAX) "\n"
			   "bandwidth     Signal bandwith\n"
			   "              Allowed values: 125, 250, 500\n");
		return 1;
	}

	uint32_t f = strtoul(argv[2], NULL, 10);
	if (f < SX127X_CONFIG_LORA_FREQUENCY_MIN ||
	    f > SX127X_CONFIG_LORA_FREQUENCY_MAX)
	{
		printf("Frequency out of range!\n");
		return 1;
	}

	uint32_t p = strtoul(argv[3], NULL, 10);
	if (p > SX127X_CONFIG_LORA_POWER_MAX)
	{
		printf("Tx power out of range!\n");
		return 1;
	}

	uint32_t sf = strtoul(argv[4], NULL, 10);
	if (sf > SX127X_CONFIG_LORA_SPREAD_MAX ||
	    sf < SX127X_CONFIG_LORA_SPREAD_MIN)
	{
		printf("Spreading factor out of range!\n");
		return 1;
	}

	uint32_t cr = strtoul(argv[5], NULL, 10);
	if (cr > SX127X_CONFIG_LORA_CODERATE_MAX)
	{
		printf("Invalid coderate!\n");
		return 1;
	}

	uint32_t bw = strtoul(argv[6], NULL, 10);
	switch (bw)
	{
		case 125: cfg->rf.lora_bandwidth = 7; break;
		case 250: cfg->rf.lora_bandwidth = 8; break;
		case 500: cfg->rf.lora_bandwidth = 9; break;
		default:
			printf("Invalid LoRa bandwitth\n");
			return 1;
	}

	cfg->rf.frequency = f;
	cfg->rf.tx_power = (uint8_t)p;
	cfg->rf.lora_spread_factor = (uint8_t)sf;
	cfg->rf.lora_coderate = (uint8_t)cr;

	// set "rf configured" bit
	cfg->config_set |= CONFIG_RF;

	return 0;
}


static int misc_config(int argc, char **argv, config_with_magic_t *cfg)
{
	if (argc == 3 && strcmp(argv[2], "get") == 0)
	{
		const misc_config_t *current = sensor_config_misc_settings();
		printf("Current misc settings:\n%lu %lu %lu %lu ",
		       current->network_timeout,
		       current->max_status_retransmissions,
		       current->rt_delay_random,
		       current->rt_delay_lin_div);
		return 2;
	}
	else if (argc != 6)
	{
		printf("USAGE: config misc get\n"
			   "USAGE: config misc <timeout> <max_retransmissions> <rt_delay_random> <rt_delay_lin>\n\n"
			   "timeout              The timeout (in seconds) for the network before the node reboots.\n"
			   "                     The timer is reset when authenticated message arrives\n"
			   "max_retransmissions  Max number of consecutive status retransmissions\n"
			   "                     before the node reboots.\n"
			   "rt_delay_random      Factor for the random part in the retransmission delay.\n"
			   "rt_delay_lin         Divider for the number of retransmissions when calculating the delay.\n\n"
			   "Retransmission delay calculation:\n"
			   "delay = base_delay + random(0, rt_delay_random * (1 + num_retransmissions / re_delay_lin)\n"
			   "base_delay is the \"timeout\" specified on the master when connection to the node\n"
			   "num_retransmissions is the number of consecutive retransmissions.\n");
		return 1;
	}

	uint32_t f = strtoul(argv[2], NULL, 10);

	if (f < 10)
	{
		// Setting the timeout to 0 sec would soft-brick the node.
		printf("Timeout must be > 10s\n");
		return 1;
	}

	cfg->misc.network_timeout = f;
	cfg->misc.max_status_retransmissions = strtoul(argv[3], NULL, 10);
	cfg->misc.rt_delay_random = strtoul(argv[4], NULL, 10);
	cfg->misc.rt_delay_lin_div = strtoul(argv[5], NULL, 10);

	// set "misc configured" bit
	cfg->config_set |= CONFIG_MISC;

	return 0;
}

int sensor_config_set_cmd(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("USAGE: config <type> ...\n\n"
			   "Configures this node.\n"
			   "Config types:\n"
			   "    network  Network settings\n"
			   "    color    Colortable for the status LEDs\n"
			   "    rf       Radio configuration\n"
			   "    misc     Misc settings\n"
			   "    reset    Resets the whole config\n");
		return 1;
	}

	config_with_magic_t newCfg;
	config_with_magic_t *cfg = (config_with_magic_t *)CONFIG_FLASH_ADDR;

	// copy old flash page into buffer
	memcpy(&newCfg, cfg, sizeof(newCfg));
	if (newCfg.magic != CONFIG_MAGIC)
	{
		// invalid magic -> reset all "configured" bits
		newCfg.config_set = 0;
	}

	int res;

	if (strcmp(argv[1], "network") == 0)
	{
		res = network_config(argc, argv, &newCfg);
	}
	else if (strcmp(argv[1], "color") == 0)
	{
		res = color_config(argc, argv, &newCfg);
	}
	else if (strcmp(argv[1], "rf") == 0)
	{
		res = rf_config(argc, argv, &newCfg);
	}
	else if (strcmp(argv[1], "misc") == 0)
	{
		res = misc_config(argc, argv, &newCfg);
	}
	else if (strcmp(argv[1], "reset") == 0)
	{
		printf("Reset config!");
		// zero the whole config
		memset(&newCfg, 0, sizeof(newCfg));

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
	newCfg.magic = CONFIG_MAGIC;

	flash_unlock();
	flash_erase_sector(CONFIG_FLASH_PAGE, FLASH_CR_PROGRAM_X32);
	flash_program(CONFIG_FLASH_ADDR, (uint8_t*)&newCfg, sizeof(newCfg));
	flash_lock();

	printf("OK Config updated, some changes have no effect until restart!\n");
	return 0;

}

#endif
