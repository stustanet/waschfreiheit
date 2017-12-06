#pragma once

// RF Transceiver config
// The SPI interface the SX127X is connected to.
#define SX127X_PARAM_SPI                    (SPI_DEV(0))

// Speed of the SPI
#define SX127X_PARAM_SPI_SPEED              (SPI_CLK_1MHZ)

// SPI mode
#define SX127X_PARAM_SPI_MODE               (SPI_MODE_0)

// NSS is connected to this GPIO
#define SX127X_PARAM_SPI_NSS                GPIO_PIN(0, 4)       /* A4 */

// RESET is connected to this GPIO
#define SX127X_PARAM_RESET                  GPIO_PIN(2, 15)      /* C15 */

// Digital IO pins of the SX127X are connected to the following GPIOs
#define SX127X_PARAM_DIO0                   GPIO_PIN(0, 3)       /* A3 */
#define SX127X_PARAM_DIO1                   GPIO_PIN(0, 2)       /* A2 */
#define SX127X_PARAM_DIO2                   GPIO_PIN(0, 1)       /* A1 */
#define SX127X_PARAM_DIO3                   GPIO_PIN(0, 0)       /* A0 */


// RF configuration
// See the SX127X datasheet for details.
#define SX127X_CONFIG_LORA_BW        SX127X_BW_125_KHZ
#define SX127X_CONFIG_LORA_SPREAD    12
#define SX127X_CONFIG_LORA_CODERATE  6

// TX Power in db
#define SX127X_CONFIG_LORA_POWER     10

// Freqency
#define SX127X_CONFIG_LORA_FREQUENCY 433500000
