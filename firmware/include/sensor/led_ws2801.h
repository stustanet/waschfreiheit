/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#pragma once

/*
 * Minimalistic driver for a WS2801 RGB LED strip
 */

#include <stdint.h>
#include "rgbcolor.h"


void led_ws2801_set(uint32_t port, uint16_t clk, uint16_t data, const rgb_data_t *rgb, uint32_t count);
