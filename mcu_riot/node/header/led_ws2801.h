/*
 * Minimalistic driver for a WS2801 RGB LED strip
 */

#include <periph/gpio.h>
#include <stdint.h>
#include "rgbcolor.h"


void led_ws2801_set(gpio_t clk, gpio_t data, const rgb_data_t *rgb, uint32_t count);
