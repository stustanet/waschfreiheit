#include "led_ws2801.h"
#include "utils.h"
#include <stdio.h>

static void micro_delay(void)
{
	for (volatile uint32_t i = 0; i < 30; i++)
	{
	}
}


void led_ws2801_set(gpio_t clk, gpio_t data, rgb_data_t *rgb, uint32_t count)
{
	uint8_t *raw = (uint8_t *)rgb;
	count *= sizeof(rgb[0]) * 8;

	for (uint32_t i = 0; i < count; i++)
	{
		// Set data lane
		if (utils_bit_is_set(raw, i))
		{
			gpio_set(data);
		}
		else
		{
			gpio_clear(data);
		}
		micro_delay();

		// Now set clock to high
		gpio_set(clk);
		micro_delay();

		// Set clock to low again
		gpio_clear(clk);
		micro_delay();
	}
}
