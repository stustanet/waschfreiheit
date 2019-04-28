/*
 * The PLL and clock dividers for the I2S has to be configured so that the resulting bit clock is 3 times (WS2812) or 4 times (SK6812)
 * the bit clock for the LEDs.
 */

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/spi.h>
#include "led_status.h"

// The PLL has an input frequency of 1 - 2 MHz (Usually 2MHz).
// With this multiplier, the PLL Output frequency has to be set to a value
// in the range of 100 - 432 MHz.
// Valid range for this value is [50 - 432]
#define I2RGB_I2S_PLL_MUL 125

// This is the after-pll divider, it has to ensure, that the output frequency is < 192MHz.
// Valid range for this value is [2 - 7]
#define I2RGB_I2S_PLL_DIV 2

// This is the 'final' divider inside the I2S peripheral
// It defines the bitclock for the output signal
// Valid range for this value is [4 - 511]
// NOTE: Internally, the value is split into a 8-bit value (which defines 2 * the divider)
//       and an 'odd' bit, which defines the lowest bit.
//       For this value both fields are combined in a single value.
#define I2RGB_I2S_CLK_DIV 50


#define I2RGB_SPI SPI2
#define I2RGB_SPI_RCC RCC_SPI2
#define I2RGB_SPI_GPIO_RCC RCC_GPIOC
#define I2RGB_SPI_GPIO_PORT GPIOC
#define I2RGB_SPI_GPIO_PIN  GPIO3
#define I2RGB_SPI_GPIO_PIN_AF GPIO_AF5

#define I2RGB_DMA DMA1
#define I2RGB_DMA_RCC RCC_DMA1
// Channel (request) for SPI
#define I2RGB_DMA_CHANNEL DMA_SxCR_CHSEL_0
// Stream for the SPI
#define I2RGB_DMA_STREAM 4

#define WS2812 0
#define SK6812 1

// Timing mode
#define I2RGB_TIMING WS2812

// Num of LEDs
#define I2RGB_NUM_OF_LEDS NUM_OF_LEDS

// Bytes per LED
#define I2RGB_BYTES_PER_LED 3

// Number of bits for the break in input bits
#define I2RGB_NUM_BREAK_BITS 80

#define I2RGB_DMA_MODE_NO_DMA 0
#define I2RGB_DMA_MODE_SINGLE 1
#define I2RGB_DMA_MODE_CONTINOUS 2

#define I2RGB_DMA_MODE I2RGB_DMA_MODE_SINGLE
