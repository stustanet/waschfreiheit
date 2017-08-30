#define SX127X_PARAM_SPI                    (SPI_DEV(0))
#define SX127X_PARAM_SPI_SPEED              (SPI_CLK_1MHZ)
#define SX127X_PARAM_SPI_MODE               (SPI_MODE_0)
#define SX127X_PARAM_SPI_NSS                GPIO_PIN(0, 4)       /* A4 */
#define SX127X_PARAM_RESET                  GPIO_PIN(2, 15)      /* C15 */
#define SX127X_PARAM_DIO0                   GPIO_PIN(0, 3)       /* A3 */
#define SX127X_PARAM_DIO1                   GPIO_PIN(0, 2)       /* A2 */
#define SX127X_PARAM_DIO2                   GPIO_PIN(0, 1)       /* A1 */
#define SX127X_PARAM_DIO3                   GPIO_PIN(0, 0)       /* A0 */

#define SX127X_CONFIG_LORA_BW        SX127X_BW_125_KHZ
#define SX127X_CONFIG_LORA_SPREAD    12
#define SX127X_CONFIG_LORA_CODERATE  6

#define SX127X_CONFIG_LORA_FREQUENCY 433500000
