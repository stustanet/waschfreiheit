/*
 * Collection of functions used in various places
 */


#pragma once

#include <stdint.h>
#include "meshnw.h"
#include "rgbcolor.h"

/*
 * Convenience macro to get the number of elements in an array.
 */
#define ARRAYSIZE(x) (sizeof((x)) / sizeof((x)[0]))

#ifndef offsetof
/*
 * Gets the offset of member v in struct S
 */
#define offsetof(S, v) ((size_t) &(((S *)0)->v))
#endif

/*
 * Checks if the value is aligned wrt. to the structs start.
 */
#define ASSERT_ALIGNED(S, v) _Static_assert(((offsetof(S, v) % sizeof(((S *)0)->v)) == 0), "Member " #v " of struct " #S " is not aligned!");

/**
 * Convert hexadecimal data to binary data
 *
 * This function parses data in hexadecimal reprensentation into binary data.
 * The result buffer may be the same as the input buffer.
 * (But the buffers MUST NOT overlap in any other way)
 * The hex chars may be lowercase or caps. If an error occures (non-hex char)
 * the function aborts and returns 0. On success the return value is 1.
 *
 * @param hex_buffer Hex data to be parsed
 * @param hex_len    Number of hex chars in the buffer
 *                   The output will have half the size of the input
 * @param bin_buffer Output buffer
 *                   This buffer must be at least hex_len / 2 bytes large
 *                   This may be the same as hex_buffer
 * @return On Success: 1
 *         On Error: 0
 */
uint8_t utils_hex_decode(const char *hex_buffer, uint32_t hex_len, uint8_t *bin_buffer);

/*
 * Parses a route from a string like 12:34,56:78, ...
 * the route pointer is set to the next element behind the next hop
 */
int utils_parse_route(const char **route, nodeid_t *dst, nodeid_t *hop);

/*
 * Parses a node id and does sanity checks.
 */
nodeid_t utils_parse_nodeid(const char *str, nodeid_t min);

/*
 * Parses a rgb value.
 *
 * Input format: 0-Terminated string with three 8-bit numbers seperated by a comma.
 * Returns nonzero on success.
 * This functions does not print an error message on error.
 */
uint8_t utils_parse_rgb(const char *str, rgb_data_t *rgb);


// Helper functions for unaligned access to 16 and 32 bit numbers
inline void u16_to_unaligned(uint16_t *dst, uint16_t src)
{
	(*((uint8_t *)dst)) = (uint8_t)src;
	(*(((uint8_t *)dst) + 1)) = (uint8_t)(src >> 8);
}

inline uint16_t u16_from_unaligned(const uint16_t *src)
{
	return (*((uint8_t *)src)) + ((*(((uint8_t *)src) + 1)) << 8);
}


void u32_to_unaligned(uint32_t *dst, uint32_t src);
uint32_t u32_from_unaligned(const uint32_t *src);

/*
 * Returns nonzero if the <bit_num> bit is set in the buffer <buf>
 */
static inline uint8_t utils_bit_is_set(uint8_t *buf, uint32_t bit_num)
{
	return buf[bit_num >> 3] & (0x80 >> (bit_num & 0x07));
}


/*
 * Sets the <bit_num> bit in <buf> to 1.
 */
static inline void utils_set_bit(uint8_t *buf, uint32_t bit_num)
{
	buf[bit_num >> 3] |= (0x80 >> (bit_num & 0x07));
}
