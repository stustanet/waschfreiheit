#include "utils.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

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


uint8_t utils_hex_decode(const char *hex_buffer, uint32_t hex_len, uint8_t *bin_buffer)
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


int utils_parse_route(const char **route, nodeid_t *dst, nodeid_t *hop)
{
	unsigned long d = strtoul(*route, (char **)route, 10);
	if (*route[0] != ':')
	{
		printf("Unexpected char in route: %i(%c)\n", *route[0], *route[0]);
		return 1;
	}

	(*route)++;

	unsigned long h = strtoul(*route, (char **)route, 10);

	if (d > MESHNW_MAX_NODEID || h > MESHNW_MAX_NODEID)
	{
		printf("Invalid route, node id out of range, dst=%lu hop=%lu\n", d, h);
		return 1;
	}

	(*dst) = d;
	(*hop) = h;
	return 0;
}


nodeid_t utils_parse_nodeid(const char *str, nodeid_t min)
{
	char *end;
	unsigned long l = strtoul(str, &end, 10);
	if (end[0] != 0 || l < min || l > MESHNW_MAX_NODEID)
	{
		printf("Invalid node id \"%s\"!. Expected decimal number bewteen %u and %u\n", str, min, MESHNW_MAX_NODEID);
		return MESHNW_INVALID_NODE;
	}

	return (nodeid_t)l;
}


uint8_t utils_parse_rgb(const char *str, rgb_data_t *rgb)
{
	char *next;
	unsigned long r = strtoul(str, &next, 10);
	if (r > 0xff || next == str || next[0] != ',')
	{
		return 0;
	}

	str = next + 1;
	unsigned long g = strtoul(str, &next, 10);
	if (g > 0xff || next == str || next[0] != ',')
	{
		return 0;
	}

	str = next + 1;
	unsigned long b = strtoul(str, &next, 10);
	if (b > 0xff || next == str || next[0] != 0)
	{
		return 0;
	}

	rgb->r = (uint8_t)r;
	rgb->g = (uint8_t)g;
	rgb->b = (uint8_t)b;

	return 1;
}


void u32_to_unaligned(uint32_t *dst, uint32_t src)
{
	(*((uint8_t *)dst)) = (uint8_t)src;
	(*(((uint8_t *)dst) + 1)) = (uint8_t)(src >>  8);
	(*(((uint8_t *)dst) + 2)) = (uint8_t)(src >> 16);
	(*(((uint8_t *)dst) + 3)) = (uint8_t)(src >> 24);
}

uint32_t u32_from_unaligned(const uint32_t *src)
{
	return (*((uint8_t *)src)) +
		((*(((uint8_t *)src) + 1)) <<  8) +
		((*(((uint8_t *)src) + 2)) << 16) +
		((*(((uint8_t *)src) + 3)) << 24);
}
