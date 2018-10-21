/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#pragma once

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>

#if defined(WASCHV1)

#define SX127X_SPI               SPI1
#define SX127X_SPI_RCC           RCC_SPI1
#define SX127X_SPI_GPIO_RCC      RCC_GPIOA
#define SX127X_SPI_GPIO_PORT     GPIOA
#define SX127X_SPI_GPIO_PIN_MOSI GPIO7
#define SX127X_SPI_GPIO_PIN_MISO GPIO6
#define SX127X_SPI_GPIO_PIN_SCK  GPIO5
#define SX127X_SPI_GPIO_PIN_NSS  GPIO4

#define SX127X_RESET_GPIO_RCC    RCC_GPIOC
#define SX127X_RESET_GPIO_PORT   GPIOC
#define SX127X_RESET_GPIO_PIN    GPIO15

#else

#define SX127X_SPI               SPI3
#define SX127X_SPI_RCC           RCC_SPI3
#define SX127X_SPI_GPIO_RCC      RCC_GPIOC
#define SX127X_SPI_GPIO_PORT     GPIOC
#define SX127X_SPI_GPIO_PIN_MOSI GPIO12
#define SX127X_SPI_GPIO_PIN_MISO GPIO11
#define SX127X_SPI_GPIO_PIN_SCK  GPIO10
#define SX127X_SPI_GPIO_PIN_AF   GPIO_AF6
#define SX127X_SPI_GPIO_PIN_NSS  GPIO9

#define SX127X_RESET_GPIO_RCC    RCC_GPIOC
#define SX127X_RESET_GPIO_PORT   GPIOC
#define SX127X_RESET_GPIO_PIN    GPIO8

#endif

// SPI3 is on the APB1 bus which is clocked at 42MHz
// The max allowed frequency is 10 MHz so we need a 8 divider (SPI has an internal 2 divider)
#define SX127X_SPI_BAUDRATE      SPI_CR1_BAUDRATE_FPCLK_DIV_8

// Module configuration
// 32 MHz
#define SX127X_CONFIG_FXOSC 32000000

// Set to 1 if the antenna is connected to the boost pin
#define SX127X_USE_PA_BOOST_PIN 1

// Allowed paramenter ranges
// See the SX127X datasheet for details.
#define SX127X_CONFIG_LORA_SPREAD_MAX    12
#define SX127X_CONFIG_LORA_SPREAD_MIN    7
#define SX127X_CONFIG_LORA_CODERATE_MAX  3
#define SX127X_CONFIG_LORA_BW_MAX  9


// TX Power in db (regulatory limit for Germany: max 10db (10 mW))
#define SX127X_CONFIG_LORA_POWER_MAX     10


// Freqency limitations to stay within German regulatury limitations
#define SX127X_CONFIG_LORA_FREQUENCY_MIN 433200000
#define SX127X_CONFIG_LORA_FREQUENCY_MAX 434600000
