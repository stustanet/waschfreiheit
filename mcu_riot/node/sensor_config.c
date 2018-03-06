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
#include "meshnw_config.h"
#include "utils.h"

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
	meshnw_rf_config_t rf;
	misc_config_t misc;
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


const meshnw_rf_config_t *sensor_config_rf_settings(void)
{
	static const meshnw_rf_config_t defaultConfig = DefaultRFSettings;
	config_with_magic_t *cfg = flashpage_addr(CONFIG_FLASH_PAGE);
	if (cfg->magic != CONFIG_MAGIC)
	{
		puts("Node not configured -> Use default rf config!\n");
		return &defaultConfig;
	}

	if ((cfg->config_set & CONFIG_RF) == 0)
	{
		puts("RF parameters not configured -> Use default!\n");
		return &defaultConfig;
	}
	return &cfg->rf;
}


const misc_config_t *sensor_config_misc_settings(void)
{
	static const misc_config_t defaultMisc = DefaultMiscSettings;
	config_with_magic_t *cfg = flashpage_addr(CONFIG_FLASH_PAGE);
	if (cfg->magic != CONFIG_MAGIC)
	{
		puts("Node not configured -> Use default misc settings!\n");
		return &defaultMisc;
	}

	if ((cfg->config_set & CONFIG_MISC) == 0)
	{
		puts("Misc settings not configured -> Use default!\n");
		return &defaultMisc;
	}
	return &cfg->misc;
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
	if (argc == 3 && strcmp(argv[2], "get") == 0)
	{
		const color_table_t *current = sensor_config_color_table();
		puts("Current color table:");
		for (int i = 0; i < 16; i++)
		{
			printf("%u,%u,%u ", (*current)[i].r, (*current)[i].g, (*current)[i].b);
		}
		puts("");
		return 2;
	}
	else if (argc != 18)
	{
		puts("USAGE: config color get");
		puts("USAGE: config color r0,g0,b0 r1,g1,b2 ... r15,g15,b15\n");
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
	if (argc == 3 && strcmp(argv[2], "get") == 0)
	{
		const meshnw_rf_config_t *current = sensor_config_rf_settings();
		printf("Current rf configuration:\n%lu %u %u %u ",
		       current->frequency,
		       current->tx_power,
		       current->lora_spread_factor,
		       current->lora_coderate);
		switch (current->lora_bandwidth)
		{
			case LORA_BW_125_KHZ: puts("125"); break;
			case LORA_BW_250_KHZ: puts("250"); break;
			case LORA_BW_500_KHZ: puts("500"); break;
		}
		return 2;
	}
	else if (argc != 7)
	{
		puts("USAGE: config rf get");
		puts("USAGE: config rf <frequency> <tx_power> <spread_factor> <coderate> <bandwidth>\n");
		puts("frequency     Carrier frequency of the LoRa modem");
		puts("              Valid range: "
		                                   TOSTRING(SX127X_CONFIG_LORA_FREQUENCY_MIN)
		                                   " - "
		                                   TOSTRING(SX127X_CONFIG_LORA_FREQUENCY_MAX));
		puts("tx_power      LoRa modem tx power in dB");
		puts("              Max value: " TOSTRING(SX127X_CONFIG_LORA_POWER_MAX));
		puts("spread_factor LoRa spreading factor");
		puts("              Valid range: "
		                                   TOSTRING(SX127X_CONFIG_LORA_SPREAD_MIN)
		                                   " - "
		                                   TOSTRING(SX127X_CONFIG_LORA_SPREAD_MAX));
		puts("coderate      LoRa coderate");
		puts("              Max value: " TOSTRING(SX127X_CONFIG_LORA_CODERATE_MAX));
		puts("bandwidth     Signal bandwith");
		puts("              Allowed values: 125, 250, 500");
		return 1;
	}

	uint32_t f = strtoul(argv[2], NULL, 10);
	if (f < SX127X_CONFIG_LORA_FREQUENCY_MIN ||
	    f > SX127X_CONFIG_LORA_FREQUENCY_MAX)
	{
		puts("Frequency out of range!");
		return 1;
	}

	uint32_t p = strtoul(argv[3], NULL, 10);
	if (p > SX127X_CONFIG_LORA_POWER_MAX)
	{
		puts("Tx power out of range!");
		return 1;
	}

	uint32_t sf = strtoul(argv[4], NULL, 10);
	if (sf > SX127X_CONFIG_LORA_SPREAD_MAX ||
	    sf < SX127X_CONFIG_LORA_SPREAD_MIN)
	{
		puts("Spreading factor out of range!");
		return 1;
	}

	uint32_t cr = strtoul(argv[5], NULL, 10);
	if (cr > SX127X_CONFIG_LORA_CODERATE_MAX)
	{
		puts("Invalid coderate!");
		return 1;
	}

	uint32_t bw = strtoul(argv[6], NULL, 10);
	switch (bw)
	{
		case 125: cfg->rf.lora_bandwidth = LORA_BW_125_KHZ; break;
		case 250: cfg->rf.lora_bandwidth = LORA_BW_250_KHZ; break;
		case 500: cfg->rf.lora_bandwidth = LORA_BW_500_KHZ; break;
		default:
			puts("Invalid LoRa bandwitth");
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
		puts("USAGE: config misc get");
		puts("USAGE: config misc <timeout> <max_retransmissions> <rt_delay_random> <rt_delay_lin>\n");
		puts("timeout              The timeout (in seconds) for the network before the node reboots.");
		puts("                     The timer is reset when authenticated message arrives");
		puts("max_retransmissions  Max number of consecutive status retransmissions");
		puts("                     before the node reboots.");
		puts("rt_delay_random      Factor for the random part in the retransmission delay.");
		puts("rt_delay_lin         Divider for the number of retransmissions when calculating the delay.\n");
		puts("Retransmission delay calculation:");
		puts("delay = base_delay + random(0, rt_delay_random * (1 + num_retransmissions / re_delay_lin)");
		puts("base_delay is the \"timeout\" specified on the master when connection to the node");
		puts("num_retransmissions is the number of consecutive retransmissions.");
		return 1;
	}

	uint32_t f = strtoul(argv[2], NULL, 10);

	if (f < 10)
	{
		// Setting the timeout to 0 sec would soft-brick the node.
		puts("Timeout must be > 10s");
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
		puts("    misc     Misc settings");
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
	else if (strcmp(argv[1], "misc") == 0)
	{
		res = misc_config(argc, argv, &config_buffer.cfg);
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
