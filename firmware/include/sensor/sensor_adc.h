/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#pragma once

/*
 * The max number of sensors channels.
 * This should be equal to the number of channles as supported by the hardware.
 * The ADC channel used for every sensor channel is equal th the channels' number.
 *
 * Every sensor channel uses a lot of RAM for the state estimation context so this
 * limits the max number of channels.
 */

#include <stdint.h>
#include <libopencm3/stm32/gpio.h>

#ifdef WASCHV2
#define NUM_OF_SENSORS 4
#define ADC_CHANNEL_MUX_LIST {0, 1, 4, 6}
#define ADC_CHANNEL_GPIO_LIST (GPIO0 | GPIO1 | GPIO4 | GPIO6)
#define ADC_GPIO_PORT GPIOA
#define ADC_GPIO_RCC RCC_GPIOA
#else
#define NUM_OF_SENSORS 2
#define ADC_CHANNEL_MUX_LIST {8, 9}
#define ADC_CHANNEL_GPIO_LIST (GPIO0 | GPIO1)
#define ADC_GPIO_PORT GPIOB
#define ADC_GPIO_RCC RCC_GPIOB
#endif


void sensor_adc_init_dma(uint16_t dest[NUM_OF_SENSORS]);
