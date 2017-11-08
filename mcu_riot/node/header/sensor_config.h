/*
 * Persistent configuration storage of the sensor node
 */

#pragma once

#include <auth.h>
#include <meshnw.h>

typedef struct
{
	uint8_t key_status[AUTH_KEY_LEN];
	uint8_t key_config[AUTH_KEY_LEN];
	nodeid_t my_id;
	
} sensor_configuration_t;

const sensor_configuration_t *sensor_config_get(void);
int sensor_config_set_cmd(int argc, char **argv);
