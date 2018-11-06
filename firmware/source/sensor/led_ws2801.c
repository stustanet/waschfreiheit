/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


#include "led_ws2801.h"

#include <libopencm3/stm32/gpio.h>
#include "utils.h"


void led_ws2801_set(uint32_t port, uint16_t clk, uint16_t data, const rgb_data_t *rgb, uint32_t count)
{
	uint8_t *raw = (uint8_t *)rgb;
	count *= sizeof(rgb[0]) * 8;

	for (uint32_t i = 0; i < count; i++)
	{
		// Set data lane
		if (utils_bit_is_set(raw, i))
		{
			gpio_set(port, data);
		}
		else
		{
			gpio_clear(port, data);
		}

		// Now set clock to high
		gpio_set(port, clk);

		// Set clock to low again
		gpio_clear(port, clk);
	}
}
