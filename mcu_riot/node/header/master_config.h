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

/*
 * Gets the keys for the specified node.
 * Returns NULL, if the node id is not valid.
 * NOTE:
 * If a node is not configured yet, this will still return a non-NULL value.
 * Aftern an chip erase, all keys are 0xff.
 */
const node_auth_keys_t *master_config_get_keys(uint8_t id);

/*
 * Command function for an interactive command to set the config.
 * Add this as the "config" command to the serial console.
 */
int master_config_set_cmd(int argc, char **argv);
