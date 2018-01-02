/*
 * Persistent configuration storage of the sensor node
 */

#pragma once

#include "auth.h"
#include "meshnw.h"
#include "rgbcolor.h"

typedef struct
{
	uint8_t key_status[AUTH_KEY_LEN];
	uint8_t key_config[AUTH_KEY_LEN];
	nodeid_t my_id;
	
} sensor_configuration_t;

/*
 * Gets the current configuration of this node.
 * If the node is not yet configured, NULL is returned.
 */
const sensor_configuration_t *sensor_config_get(void);

/*
 * Gets the current color table.
 * If the color table is not configured, a default value is returned.
 */
const color_table_t *sensor_config_color_table(void);

/*
 * Command function for an interactive command to set the config.
 * Add this as the "config" command to the serial console.
 */
int sensor_config_set_cmd(int argc, char **argv);
