/*
 * RFM_config.h
 * Configuration for the RFM26 library
 */

// SPI interface
#define RFM_SPI           SPI1
#define RFM_SPI_PRESCALER SPI_CR1_BAUDRATE_FPCLK_DIV_256
#define RFM_SPI_PORT      GPIOA
#define RFM_SPI_PIN_MISO  GPIO6
#define RFM_SPI_PIN_MOSI  GPIO7
#define RFM_SPI_PIN_SCK   GPIO5

// This assumes, that all used GPIOs are on the same port
#define RFM_RCC_GPIO      RCC_GPIOA
#define RFM_RCC_SPI       RCC_SPI1

// Antenna switch pins (GPIOs on the RFM module)
#define RFM_ANTSW_TX 2  // GPIO needs to be connected to TX switch input
#define RFM_ANTSW_RX 3  // GPIO needs to be connected to RX switch input

// CTS of RFM module (Default GPIO 1 on the module) needs to be connected to this pin
#define RFM_CTS_PORT GPIOA
#define RFM_CTS_PIN GPIO3

// Interrupt pin (XIRQ) of the RFM module needs to be connected to this pin
#define RFM_INT_PORT GPIOA
#define RFM_INT_PIN GPIO2

// Slave select (NSEL / XSEL) pin of the RFM module needs to be connected to this pin
#define RFM_NSEL_PORT GPIOA
#define RFM_NSEL_PIN GPIO4

// Shutdown pin of the RFM module needs to be connected to this pin
#define RFM_SHDN_PORT GPIOA
#define RFM_SHDN_PIN GPIO1

// Timeout in ticks for the cts signal
#define RFM_CTS_TIMEOUT 500

// Number of idle loop cycles for the short delay.
// This is used e.g. before deasserting the NSEL pin of the module.
// A too small value will result in an unstable communication becuase the STM32 seems to be too fst for the RFM module
#define RFM_SHORT_DELAY_CYCLES 100

// Fixed size of a packet
// This value should be equal to the packet size of the packet handler
#define RFM_PACKET_SIZE 7

// Timeout for sending a packet
// This value must be larger than the time it takes to send a packet
#define RFM_TX_TIMEOUT 2000

// Length of the shutdown signal during module reset
#define RFM_RESET_TIME 100
