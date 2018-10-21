/*
 * I2S_RBG: A driver for single-wire RGB LEDs (WS2812 and SK6812) through the I2S interface.
 */

#include <stdint.h>
#include "i2s_rgb_config.h"

void i2s_rgb_init(void);
void i2s_rgb_set(const uint8_t *data);
