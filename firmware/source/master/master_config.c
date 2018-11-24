/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


#include "master_config.h"
#include <libopencm3/stm32/flash.h>
#include <string.h>
#include <stdlib.h>
#include "utils.h"

#include "tinyprintf.h"

#define FLASH_START 0x08000000
#define SIZE_OF_ENTRY (sizeof(node_auth_keys_t))

#ifdef WASCHV2
// The last page is 128k :(
// This is HALF of the total flash
#define FLASHPAGE_SIZE 0x20000
_Static_assert((SIZE_OF_ENTRY * MASTER_CONFIG_NUM_NODES <= FLASHPAGE_SIZE), "All config must fit into one page");
#define CONFIG_FLASH_PAGE_START 5
#define CONFIG_FLASH_ADDR (FLASH_START + 0x20000)
#define CONFIG_ENTRIES_PER_PAGE MASTER_CONFIG_NUM_NODES
#else

#define FLASHPAGE_SIZE 1024

// Number of node entries in a flash page
#define CONFIG_ENTRIES_PER_PAGE (FLASHPAGE_SIZE / SIZE_OF_ENTRY)

// Total number of flash pages used by the config
#define CONFIG_FLASH_PAGES ((((MASTER_CONFIG_NUM_NODES) - 1) / CONFIG_ENTRIES_PER_PAGE) + 1)

// The config is written to the end of the flash.
// Obviously, this space must not be used by program code!
#define CONFIG_FLASH_PAGE_START (64 - (CONFIG_FLASH_PAGES))

#define CONFIG_FLASH_ADDR (FLASH_START + CONFIG_FLASH_PAGE_START * FLASHPAGE_SIZE)
#endif

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
static const config_page_t *get_config_entry(uint8_t id, uint8_t *entry)
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
	return (const config_page_t *)(CONFIG_FLASH_ADDR + (id / CONFIG_ENTRIES_PER_PAGE) * FLASHPAGE_SIZE);
}


const node_auth_keys_t *master_config_get_keys(uint8_t id)
{
	uint8_t entry_idx;
	const config_page_t *page = get_config_entry(id, &entry_idx);
	if (!page)
	{
		// Invalid id
		return NULL;
	}

	// return the pointer to the entry
	return &page->keys[entry_idx];
}


void master_config_set_cmd(int argc, char **argv)
{
	// Buffer for read-modify-write the config entries
	// This is up to a full page, so I definitely don't want this on the stack!
	static config_page_t config_buffer;


	if (argc != 4)
	{
		printf("USAGE: config <node_id> <key_status> <key_config>\n\n"
			   "node_id     The id of the node to configure (decimal)\n"
			   "key_status  Key used for status messages (incoming traffic)\n"
			   "key_config  Key used for config messages (outgoing traffic)\n"
			   "Both keys must be exactly 128 bit long and encoded in hex format.\n");
		return;
	}

	int node_id = atoi(argv[1]);

	if (node_id < 0 || node_id > MASTER_CONFIG_NUM_NODES)
	{
		printf("Node id out of range, expected to be between 0 and %u!\n", MASTER_CONFIG_NUM_NODES);
		return;
	}

	uint8_t entry_idx;
	const config_page_t *current = get_config_entry(node_id, &entry_idx);

	if (current == NULL)
	{
		printf("Failed to get page for node\n");
		return;
	}

	// now copy data into buffer
	memcpy(&config_buffer, current, sizeof(config_buffer));


	// modify entry
	if (strlen(argv[2]) != AUTH_KEY_LEN * 2 || !utils_hex_decode(argv[2], AUTH_KEY_LEN * 2, config_buffer.keys[entry_idx].key_status))
	{
		printf("Invalid status key, expected status key to be 32 hex chars (128 bit)!\n");
		return;
	}

	if (strlen(argv[3]) != AUTH_KEY_LEN * 2 || !utils_hex_decode(argv[3], AUTH_KEY_LEN * 2, config_buffer.keys[entry_idx].key_config))
	{
		printf("Invalid config key, expected status key to be 32 hex chars (128 bit)!\n");
		return;
	}

	uint8_t page = node_id / CONFIG_ENTRIES_PER_PAGE;

	flash_unlock();

#ifdef WASCHV2
	flash_erase_sector(CONFIG_FLASH_PAGE_START + page, FLASH_CR_PROGRAM_X32);
#else
	flash_erase_page(CONFIG_FLASH_ADDR + page * FLASHPAGE_SIZE);
#endif

	flash_program(CONFIG_FLASH_ADDR + page * FLASHPAGE_SIZE, (uint8_t*)&config_buffer, sizeof(config_buffer));
	flash_lock();

	// Everything done
	printf("OK, updated config for node %u\n", node_id);
	return;

}
