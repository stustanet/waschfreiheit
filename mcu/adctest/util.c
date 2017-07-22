#include "util.h"

#include <stdint.h>

/*
 * Parses a single hex char and returs the number value
 * If the passed char is not a valid hex char (NOT 0-9, a-f, A-F)
 * 0xFF is returned
 */
static inline uint8_t dehex_nibble(uint8_t n)
{
	if(n >= '0' && n <= '9')
	{
		return n - '0';
	}
	else if(n >= 'A' && n <= 'F')
	{
		return (n - 'A') + 10;
	}
	else if(n >= 'a' && n <= 'f')
	{
		return (n - 'a') + 10;
	}

	// no valid hex
	return 0xFF;
}


uint8_t hex_decode(const char *hex_buffer, uint32_t hex_len, uint8_t *bin_buffer)
{
	// check if length is devideable by two
	if(hex_len & 0x01)
	{
		// invalid length
		return 0;
	}

	while(hex_len)
	{
		uint8_t h = dehex_nibble(*(hex_buffer++));
		uint8_t l = dehex_nibble(*(hex_buffer++));

		if(h == 0xFF || l == 0xFF)
		{
			// something invalid
			return 0;
		}

		*(bin_buffer++) = (h << 4) | l;
		hex_len -= 2;
	}

	return 1;
}


void hex_encode(const uint8_t *bin_buffer, uint32_t bin_len, char *hex_buffer)
{
	static const char HEX_TABLE[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

	// need to do this back to front to allow bin_buffer == hex_buffer
	while(bin_len--)
	{
		hex_buffer[bin_len * 2 + 1] = HEX_TABLE[bin_buffer[bin_len] & 0x0F];
		hex_buffer[bin_len * 2    ] = HEX_TABLE[bin_buffer[bin_len] >> 4];
	}
}
