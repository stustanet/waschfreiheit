#ifdef MASTER

#include "master_config.h"
#include <periph/flashpage.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utils.h"


// Number of node entries in a flash page
#define CONFIG_ENTRIES_PER_PAGE (FLASHPAGE_SIZE / sizeof(node_auth_keys_t))

// Total number of flash pages useb dy the config
#define CONFIG_FLASH_PAGES ((((MASTER_CONFIG_NUM_NODES) - 1) / CONFIG_ENTRIES_PER_PAGE) + 1)

// The config is written to the end of the flash.
// Obviously, this space must not be used by program code!
#define CONFIG_FLASH_PAGE_START (FLASHPAGE_NUMOF - (CONFIG_FLASH_PAGES))

/*
 * Layout of a flash page.
 * NOTE: This must be exactly as large as a flash page.
 */
typedef struct
{
	node_auth_keys_t keys[CONFIG_ENTRIES_PER_PAGE];
} config_page_t;


/*
 * Returns the pointer to the page.
 * Entry is set to the key pair's index in the page.
 */
static config_page_t *get_config_entry(uint8_t id, uint8_t *entry)
{

	// Need #if here to avoid warnings
#if MASTER_CONFIG_NUM_NODES < 0xff
	if (id > MASTER_CONFIG_NUM_NODES)
	{
		return NULL;
	}
#endif

	(*entry) = id % CONFIG_ENTRIES_PER_PAGE;

	// Because the flash is memory-mapped, i can simply return a pointer to the page.
	return flashpage_addr(CONFIG_FLASH_PAGE_START + id / CONFIG_ENTRIES_PER_PAGE);
}


const node_auth_keys_t *master_config_get_keys(uint8_t id)
{
	uint8_t entry_idx;
	config_page_t *page = get_config_entry(id, &entry_idx);
	if (!page)
	{
		// Invalid id
		return NULL;
	}

	// return the pointer to the entry
	return &page->keys[entry_idx];
}


int master_config_set_cmd(int argc, char **argv)
{
	// Buffer for read-modify-write the config entries
	// This is 1k, so I definitely don't want this on the stack!
	static config_page_t config_buffer;
	_Static_assert(sizeof(config_buffer) == FLASHPAGE_SIZE, "Config buffer must have flash page size");


	if (argc != 4)
	{
		puts("USAGE: config <node_id> <key_status> <key_config>\n");
		puts("node_id     The id of the node to configure (decimal)");
		puts("key_status  Key used for status messages (incoming traffic)");
		puts("key_config  Key used for config messages (outgoing traffic)");
		puts("Both keys must be exactly 128 bit long and encoded in hex format.");
		return 1;
	}

	int node_id = atoi(argv[1]);

	if (node_id < 0 || node_id > MASTER_CONFIG_NUM_NODES)
	{
		printf("Node id out of range, expected to be between 0 and %u!\n", MASTER_CONFIG_NUM_NODES);
		return 1;
	}

	uint8_t entry_idx;
	config_page_t *current = get_config_entry(node_id, &entry_idx);

	if (current == NULL)
	{
		puts("Failed to get page for node");
		return 1;
	}

	// now copy data into buffer
	memcpy(&config_buffer, current, sizeof(config_buffer));


	// modify entry
	if (strlen(argv[2]) != AUTH_KEY_LEN * 2 || !utils_hex_decode(argv[2], AUTH_KEY_LEN * 2, config_buffer.keys[entry_idx].key_status))
	{
		puts("Invalid status key, expected status key to be 32 hex chars (128 bit)!");
		return 1;
	}

	if (strlen(argv[3]) != AUTH_KEY_LEN * 2 || !utils_hex_decode(argv[3], AUTH_KEY_LEN * 2, config_buffer.keys[entry_idx].key_config))
	{
		puts("Invalid config key, expected status key to be 32 hex chars (128 bit)!");
		return 1;
	}

	// Finally write it back.
	if (flashpage_write_and_verify(flashpage_page(current) , &config_buffer) != FLASHPAGE_OK)
	{
		puts("Flash verification failed!");
		return 1;
	}

	// Everything done
	printf("OK, updated config for node %u\n", node_id);
	return 0;

}

#endif
