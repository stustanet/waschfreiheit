/*
 * Structure definition for a RGB value and a 16-Element color table.
 */

#pragma once

#include <stdint.h>

typedef struct
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
} rgb_data_t;

typedef rgb_data_t color_table_t[16];
