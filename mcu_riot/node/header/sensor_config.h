/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


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


typedef struct
{
	/*
	 * Timeout for the config channel in seconds
	 * If no authenticated message is received for more than the specified time, the node reboots.
	 * This is primary intended to avoid the network getting stuck if the routes are invalid.
	 */
	uint32_t network_timeout;

	/*
	 * Max number of retransmissions before the node declares the network dead and reboots.
	 */
	uint32_t max_status_retransmissions;

	/*
	 * Max random delay for status retransmissions.
	 * Actual retransmission delay is
	 * base delay + random((rt_delay_random * (1 + num_of_retries / rt_delay_lin_div)
	 */
	uint32_t rt_delay_random;
	uint32_t rt_delay_lin_div;
} misc_config_t;

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
 * Gets the current RF configuration.
 * If the rf config is not set, default values are returned,
 */
const meshnw_rf_config_t *sensor_config_rf_settings(void);

/*
 * Gets the current configuration for various parameters.
 * If the misc config is not set, default values are returned,
 */
const misc_config_t *sensor_config_misc_settings(void);

/*
 * Command function for an interactive command to set the config.
 * Add this as the "config" command to the serial console.
 */
int sensor_config_set_cmd(int argc, char **argv);
