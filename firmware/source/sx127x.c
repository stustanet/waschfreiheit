/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#include "sx127x.h"

#include <string.h>

#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/gpio.h>
#include "tinyprintf.h"
#include "sx127x_config.h"
#include "sx127x_reg_lora.h"


static const uint32_t LORA_BANDWIDTH_TABLE[] = {7800, 10400, 15600, 20800, 31200, 41700, 62500, 125000, 250000, 500000};

// 16ms: Above this symbol time, the LowDataRateOptimize mode should be set
#define LORA_LOW_DR_OPT_THRESHOLD_US 16000

static void sx127x_read(uint8_t addr, uint8_t *data, size_t len)
{
	addr &= ~SX127x_WriteReg;
	gpio_clear(SX127X_SPI_GPIO_PORT, SX127X_SPI_GPIO_PIN_NSS);

	spi_xfer(SX127X_SPI, addr);

	for (size_t i = 0; i < len; ++i)
	{
		data[i] = spi_xfer(SX127X_SPI, 0);
	}

	gpio_set(SX127X_SPI_GPIO_PORT, SX127X_SPI_GPIO_PIN_NSS);
}

static void sx127x_write(uint8_t addr, const uint8_t *data, size_t len)
{
	addr |= SX127x_WriteReg;
	gpio_clear(SX127X_SPI_GPIO_PORT, SX127X_SPI_GPIO_PIN_NSS);

	spi_xfer(SX127X_SPI, addr);

	for (size_t i = 0; i < len; ++i)
	{
		spi_xfer(SX127X_SPI, data[i]);
	}

	gpio_set(SX127X_SPI_GPIO_PORT, SX127X_SPI_GPIO_PIN_NSS);
}

static uint8_t sx127x_get_reg(uint8_t reg)
{
	uint8_t buffer;
	sx127x_read(reg, &buffer, 1);
	return buffer;
}

static void sx127x_set_reg(uint8_t reg, uint8_t val)
{
	sx127x_write(reg, &val, 1);
}

static void init_io(void)
{
	rcc_periph_clock_enable(SX127X_SPI_GPIO_RCC);
	rcc_periph_clock_enable(SX127X_SPI_RCC);

#if defined(WASCHV1)
	gpio_set_mode(SX127X_SPI_GPIO_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, SX127X_SPI_GPIO_PIN_MOSI);
	gpio_set_mode(SX127X_SPI_GPIO_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, SX127X_SPI_GPIO_PIN_SCK);
	gpio_set_mode(SX127X_SPI_GPIO_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, SX127X_SPI_GPIO_PIN_MISO);

	gpio_set_mode(SX127X_SPI_GPIO_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, SX127X_SPI_GPIO_PIN_NSS);
	gpio_set_mode(SX127X_RESET_GPIO_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, SX127X_RESET_GPIO_PIN);
#else
	gpio_mode_setup(SX127X_SPI_GPIO_PORT,
					GPIO_MODE_AF,
					GPIO_PUPD_NONE,
					SX127X_SPI_GPIO_PIN_MISO | SX127X_SPI_GPIO_PIN_MOSI | SX127X_SPI_GPIO_PIN_SCK);

	gpio_set_af(SX127X_SPI_GPIO_PORT,
				SX127X_SPI_GPIO_PIN_AF,
				SX127X_SPI_GPIO_PIN_MISO | SX127X_SPI_GPIO_PIN_MOSI | SX127X_SPI_GPIO_PIN_SCK);

	gpio_mode_setup(SX127X_SPI_GPIO_PORT,
					GPIO_MODE_OUTPUT,
					GPIO_PUPD_NONE,
					SX127X_SPI_GPIO_PIN_NSS);

	gpio_mode_setup(SX127X_RESET_GPIO_PORT,
					GPIO_MODE_OUTPUT,
					GPIO_PUPD_NONE,
					SX127X_RESET_GPIO_PIN);

#endif

	spi_init_master(SX127X_SPI,
					SX127X_SPI_BAUDRATE,
					SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE,
					SPI_CR1_CPHA_CLK_TRANSITION_1,
					SPI_CR1_DFF_8BIT,
					SPI_CR1_MSBFIRST);

	//spi_disable_ss_output(SX127X_SPI);
	//spi_enable_software_slave_management(SX127X_SPI);
	spi_enable(SX127X_SPI);
}


static bool config_sanity_check(const sx127x_rf_config_t *config)
{
	if (config->lora_spread_factor > SX127X_CONFIG_LORA_SPREAD_MAX ||
	    config->lora_spread_factor < SX127X_CONFIG_LORA_SPREAD_MIN)
	{
		printf("Invalid LoRa spread factor: %u\n", config->lora_spread_factor);
		return false;
	}

	if (config->lora_coderate > SX127X_CONFIG_LORA_CODERATE_MAX)
	{
		printf("Invalid LoRa code rate: %u\n", config->lora_coderate);
		return false;
	}

	if (config->tx_power > SX127X_CONFIG_LORA_POWER_MAX)
	{
		printf("RF tx power (%u) too large, max: %u\n", config->tx_power, SX127X_CONFIG_LORA_POWER_MAX);
		return false;
	}

	if (config->lora_bandwidth > SX127X_CONFIG_LORA_BW_MAX)
	{
		printf("Invalid LoRa bandwith value: %u\n", config->lora_bandwidth);
		return false;
	}

	if (config->frequency > SX127X_CONFIG_LORA_FREQUENCY_MAX ||
	    config->frequency < SX127X_CONFIG_LORA_FREQUENCY_MIN)
	{
		printf("RF frequency (%lu) out of allowed range (%u - %u)\n",
		       config->frequency,
		       SX127X_CONFIG_LORA_FREQUENCY_MIN,
		       SX127X_CONFIG_LORA_FREQUENCY_MAX);
		return false;
	}

	return true;
}


static bool init_rfm(const sx127x_rf_config_t *config)
{
	if (!config_sanity_check(config))
	{
		return false;
	}

	// Set to LoRa mode
	// -> Need to set device to sleep mode first
	sx127x_set_reg(SX127x_RegOpMode, SX127x_RegOpMode_LongRangeMode);
	// -> Now switch back to the STANDY mode
	sx127x_set_reg(SX127x_RegOpMode, SX127x_RegOpMode_LongRangeMode | SX127x_RegOpMode_Mode_STDBY);

	// Read back the config register
	uint8_t read_back = sx127x_get_reg(SX127x_RegOpMode);
	if (read_back != (SX127x_RegOpMode_LongRangeMode | SX127x_RegOpMode_Mode_STDBY))
	{
		printf("Unexpected value read back from Sx127x config register! Expected 0x81 but got: 0x%02x\nCheck the connection to the RF module!", read_back);
		return false;
	}

	// Set up the frequency
	//    freq = (FXOSC * frf) / 2^19
	// => frf  = (freq * 2^19) / FXOSC
	uint32_t frf = (uint32_t)((((uint64_t)config->frequency) * (1 << 19)) / SX127X_CONFIG_FXOSC);
	sx127x_set_reg(SX127x_RegFrfMsb, frf >> 16);
	sx127x_set_reg(SX127x_RegFrfMid, frf >> 8);
	sx127x_set_reg(SX127x_RegFrfLsb, frf);

	// Set tx power
	uint8_t pa_config = 0;
#if SX127X_USE_PA_BOOST_PIN == 1
	pa_config |= SX127x_RegPaConfig_PaSelect;

	uint8_t pwr = 0;
	if (config->tx_power > 2)
	{
		pwr = config->tx_power - 2;
	}

	pa_config |= pwr << SX127x_RegPaConfig_OutputPower_Pos;
#else
	// pwr = pmax - (15 - OutputPower)
	// We set pmax to 0 (10.8dB) so setting OutputPower to tx_power + 5 results in approx the right power
	uint8_t pwr = config->tx_power + 5;
	if (pwr > 15)
	{
		pwr = 15;
	}
	pa_config |= (pwr + 5) << SX127x_RegPaConfig_OutputPower_Pos;
#endif
	sx127x_set_reg(SX127x_RegPaConfig, pa_config);

	sx127x_set_reg(SX127x_RegModemConfig1,
				   (config->lora_bandwidth << SX127x_RegModemConfig1_Bw_Pos) |
				   ((config->lora_coderate + 1) << SX127x_RegModemConfig1_CodingRate_Pos));

	// Set spread factor
	// Set timeout to max
	sx127x_set_reg(SX127x_RegModemConfig2,
				   (config->lora_spread_factor << SX127x_RegModemConfig2_SpreadingFactor_Pos) |
				   SX127x_RegModemConfig2_SymbTimeout98_Mask);
	sx127x_set_reg(SX127x_RegSymbTimeoutLsb, 0xff);

	// Check, if we need to set the LowDataRateOptimize bit
	// This is required if the symbol length exceeds 16ms
	// The symbol length (sl) is calculated as follows: 1 / Rs with Rs = BW / 2^SF
	// ==> sl = 2^SF / BW
	// Max value for sf is 12 so 2^sf * 1000000 will never cause an uint32_t overflow.
	_Static_assert(SX127X_CONFIG_LORA_BW_MAX <= sizeof(LORA_BANDWIDTH_TABLE) / sizeof(LORA_BANDWIDTH_TABLE[0]), "Bad value for SX127X_CONFIG_LORA_BW_MAX");
	uint32_t sl_us = ((1UL << config->lora_spread_factor) * 1000000) / LORA_BANDWIDTH_TABLE[config->lora_bandwidth];
	if (sl_us > LORA_LOW_DR_OPT_THRESHOLD_US)
	{
		sx127x_set_reg(SX127x_RegModemConfig3, SX127x_RegModemConfig3_LowDataRateOptimize);
	}

	return true;
}


bool sx127x_init(const sx127x_rf_config_t *cfg)
{
	init_io();

	// Reset the module
	gpio_clear(SX127X_RESET_GPIO_PORT, SX127X_RESET_GPIO_PIN);
	for (volatile uint32_t i = 0; i < 100; i++);
	gpio_set(SX127X_RESET_GPIO_PORT, SX127X_RESET_GPIO_PIN);

	return init_rfm(cfg);
}


uint8_t sx127x_recv(uint8_t *buffer, uint8_t max)
{
	uint8_t modereg = sx127x_get_reg(SX127x_RegOpMode);
	uint8_t mode = modereg & SX127x_RegOpMode_Mode_Mask;

	if (mode == SX127x_RegOpMode_Mode_TX ||
		mode == SX127x_RegOpMode_Mode_FSTX)
	{
		// Currently in tx mode
		return 0;
	}

	if (mode != SX127x_RegOpMode_Mode_RXSINGLE)
	{
		// Not in rx mode -> Check, if we got data
		uint8_t irqflags = sx127x_get_reg(SX127x_RegIrqFlags);
		if (irqflags & SX127x_RegIrqFlags_RxDone)
		{
			// Yay, we got a packet
			uint8_t size = sx127x_get_reg(SX127x_RegRxNbBytes);
			uint8_t readsize = size;
			if (readsize > max)
			{
				readsize = max;
			}

			// Set ptr to begin
			sx127x_set_reg(SX127x_RegFifoAddrPtr, 0);

			// Read the data
			sx127x_read(SX127x_RegFifo, buffer, readsize);

			// Reset the irq flags
			sx127x_set_reg(SX127x_RegIrqFlags, 0xff);

			return size;
		}

		// Not in rx mode and no RxDone Interrupt -> re-enter rx mode
		mode = (modereg & ~SX127x_RegOpMode_Mode_Mask) | SX127x_RegOpMode_Mode_RXSINGLE;
		//printf("Set mode to %02x for RX\n", mode);
		sx127x_set_reg(SX127x_RegOpMode, mode);
	}

	return 0;
}


bool sx127x_send(const uint8_t *data, uint8_t len)
{
	if (sx127x_is_busy())
	{
		printf("Send packet failed because the modem is busy\n");
		return false;
	}

	// Re-set the tx fifo addr and the fifo ptr to 0
	sx127x_set_reg(SX127x_RegFifoTxBaseAddr, 0);
	sx127x_set_reg(SX127x_RegFifoAddrPtr, 0);

	// Set the tx len
	sx127x_set_reg(SX127x_RegPayloadLength, len);

	// Clear the interrupt flags
	sx127x_set_reg(SX127x_RegIrqFlags, 0xff);

	// Write data into TX FIFO
	sx127x_write(SX127x_RegFifo, data, len);

	uint8_t mode = sx127x_get_reg(SX127x_RegOpMode);
	mode = (mode & ~SX127x_RegOpMode_Mode_Mask) | SX127x_RegOpMode_Mode_TX;
	sx127x_set_reg(SX127x_RegOpMode, mode);

	// The device will now send the data

	return true;
}


bool sx127x_is_busy(void)
{
	uint8_t mode = sx127x_get_reg(SX127x_RegOpMode) & SX127x_RegOpMode_Mode_Mask;
	if (mode == SX127x_RegOpMode_Mode_FSTX ||
		mode == SX127x_RegOpMode_Mode_TX)
	{
		return true;
	}

	uint8_t state = sx127x_get_reg(SX127x_RegModemStat);
	if (state & (SX127x_RegModemStat_ModemStatus_SignalDetected |
			     SX127x_RegModemStat_ModemStatus_SignalSynchronized))
	{
		return true;
	}

	uint8_t irqflags = sx127x_get_reg(SX127x_RegIrqFlags);
	if (irqflags & SX127x_RegIrqFlags_RxDone)
	{
		return true;
	}

	return false;
}


uint64_t sx127x_get_random(void)
{
	// Set to continuous mode so the receiver will not enter sleep mode
	uint8_t mode = sx127x_get_reg(SX127x_RegOpMode);
	mode = (mode & ~SX127x_RegOpMode_Mode_Mask) | SX127x_RegOpMode_Mode_RXCONTINUOUS;
	sx127x_set_reg(SX127x_RegOpMode, mode);

	uint64_t res = 0;
	for (size_t i = 0; i < 65536; i++)
	{
	    res ^= ((uint64_t)sx127x_get_reg(SX127x_RegRssiWideband)) << (i % 64);
	}

	return res;
}

void sx127x_get_last_pkt_stats(uint8_t *rssi, int8_t *snr)
{
	*rssi = sx127x_get_reg(SX127x_RegPktRssiValue);
	*snr = (int8_t)sx127x_get_reg(SX127x_RegPktSnrValue);
}


void sx127x_test_cmd(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("USAGE: sx127x <CMD>\n");
		return;
	}

	if (strcmp(argv[1], "reg") == 0)
	{
		if (argc != 3 && argc != 4)
		{
			printf("USAGE: sx127x reg ADDR [new value]\n");
			return;
		}

		uint8_t addr = strtoul(argv[2], NULL, 16) & 0x7f;

		if (argc == 4)
		{
			uint8_t val = strtoul(argv[3], NULL, 16) & 0xff;
			printf("0x%x <- 0x%x\n", addr, val);
			sx127x_set_reg(addr, val);
		}

		uint8_t v = sx127x_get_reg(addr);
		printf("0x%x = 0x%x\n", addr, v);
	}
	else if (strcmp(argv[1], "recv") == 0)
	{
		uint8_t buffer[255];
		uint8_t res = sx127x_recv(buffer, sizeof(buffer));
		if (res == 0)
		{
			printf("No data\n");
		}
		else
		{
			printf("Received packet with size %u\n", res);
		}
	}
	else if (strcmp(argv[1], "send") == 0)
	{
		if (argc != 3)
		{
			printf("USAGE: sx127x send size\n");
			return;
		}

		uint8_t buffer[255];
		uint8_t size = strtoul(argv[2], NULL, 16) & 0xff;

		for (uint8_t i = 0; i < size; i++)
		{
			buffer[i] = i;
		}
		bool res = sx127x_send(buffer, size);
		if (res)
		{
			printf("Tx OK\n");
		}
		else
		{
			printf("Tx error\n");
		}
	}
	else if (strcmp(argv[1], "random") == 0)
	{
		uint64_t rnd = sx127x_get_random();
		printf("Random: 0x%08lx%08lx\n", (uint32_t)(rnd >> 32), (uint32_t)rnd);
	}
	else
	{
		printf("Unknown sx127x command: \"%s\"\n", argv[1]);
	}
}
