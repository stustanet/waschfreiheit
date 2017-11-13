/*
 * Persistent configuration storage of the master
 */

#pragma once

#include <auth.h>
#include <meshnw.h>

#define MASTER_CONFIG_NUM_NODES 256

typedef struct
{
	uint8_t key_status[AUTH_KEY_LEN];
	uint8_t key_config[AUTH_KEY_LEN];
} node_auth_keys_t;

const node_auth_keys_t *master_config_get_keys(uint8_t id);
int master_config_set_cmd(int argc, char **argv);
