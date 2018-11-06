/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


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
