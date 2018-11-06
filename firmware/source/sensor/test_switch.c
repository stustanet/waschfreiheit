/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#include "test_switch.h"

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <stdint.h>
#include <stdlib.h>

static const uint16_t TEST_SWITCH_PIN[TEST_SWITCH_NUMOF] = {GPIO10, GPIO12};
#define TEST_SWITCH_PORT GPIOB
#define TEST_SWITCH_RCC RCC_GPIOB

void test_switch_init(void)
{
	rcc_periph_clock_enable(TEST_SWITCH_RCC);

	for (size_t i = 0; i < TEST_SWITCH_NUMOF; i++)
	{
		gpio_mode_setup(TEST_SWITCH_PORT,
						GPIO_MODE_INPUT,
						GPIO_PUPD_PULLUP,
						TEST_SWITCH_PIN[i]);
	}
}


bool test_switch_pressed(enum TEST_SWITCH sw)
{
	if (sw >= TEST_SWITCH_NUMOF)
	{
		return false;
	}
	return !gpio_get(TEST_SWITCH_PORT, TEST_SWITCH_PIN[sw]);
}
