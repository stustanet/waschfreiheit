/*
 * Collection of functions used in various places
 */


#pragma once

#include <stdint.h>

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
