//#include <string.h>

#include "RFM_26.h"

#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#include "delay.h"

#include "RFM_26_config.h"
#include "si4463_cmd.h"
#include "radio_config_Si4463_slow.h"


#define SLAVE_SEL()   gpio_clear(RFM_NSEL_PORT, RFM_NSEL_PIN)
#define SLAVE_DESEL() gpio_set(RFM_NSEL_PORT, RFM_NSEL_PIN)
#define CTS_IS_LOW()  (!gpio_get(RFM_CTS_PORT, RFM_CTS_PIN))
#define IRQ_IS_HIGH()  (gpio_get(RFM_INT_PORT, RFM_INT_PIN))

#define RFM_MODULE_DISABLE() gpio_set(RFM_SHDN_PORT, RFM_SHDN_PIN)
#define RFM_MODULE_ENABLE() gpio_clear(RFM_SHDN_PORT, RFM_SHDN_PIN)

#ifndef NULL
#define NULL ((void *)0)
#endif

static void rfm_short_delay(void)
{
	for(volatile uint32_t i = 0; i < RFM_SHORT_DELAY_CYCLES; i++);
}

/*
 * This is a helper function for releasing the slave select pin
 * It releases the pin and waits for a short time.
 * This is required to ensure that the RFM module actually gets the deselect signal
 */
static void slave_desel_and_wait(void)
{
	SLAVE_DESEL();
	rfm_short_delay();
}


static inline void rfm_spi_write_byte(const uint8_t data)
{
	spi_send(RFM_SPI, data);
}


static void rfm_spi_write(const uint8_t *data, uint32_t size)
{
	while (size--)
	{
		rfm_spi_write_byte(*(data++));
	}
}


static void rfm_spi_read(uint8_t *data, uint32_t size)
{
	while (size--)
	{
		(*data++) = spi_read(RFM_SPI);
	}
}

/**
 * Performs a fast resonse (or result) read operation
 * @param cmd        Query command (this sould be a command that does NOT require a wait for CTS)
 * @param result     Buffer for the result
 * @param ressize    Size of the result buffer in bytes (length of the expected result)
 * @param expect_cts If this is set, the first byte of the response is expected to be 0xFF (CTS byte)
 *                     If eypectCTS and the first byte is CTS, it is discarded and the normal receive begins
 *                     Otherwise, if expectCTS and the first byte is NOT CTS, the functions returns with 0
 *
 * @return A Value from RMF26_RES
 */
static uint8_t rfm_get_data(uint8_t cmd, uint8_t *result, uint32_t ressize, uint8_t expect_cts)
{
	// activate slave
	SLAVE_SEL();

	// send command
	
	rfm_spi_write_byte(cmd);
	
	if(expect_cts)
	{
		uint8_t tmp;

		rfm_spi_read(&tmp, sizeof(tmp));

		if(tmp != 0xFF)
		{
			slave_desel_and_wait();
			return RFM_RES_CTS_MISSING;
		}
	}

	rfm_spi_read(result, ressize);

	// disable slave
	slave_desel_and_wait();

	return RFM_RES_OK;
}


/**
 * Send a comamnd to the connected RFM module
 * @param cmd        Command data (binary)
 * @param cmdsize    Size of the command in bytes
 * @param result     Buffer for the result (this can be NULL, if ressize is 0)
 * @param ressize    Size of the result buffer in bytes (length of the expected result)
 * @return A value from RMF26_RES
 */
static uint8_t rfm_command(const uint8_t *cmd, uint32_t cmdsize, uint8_t *result, uint32_t ressize)
{
	// set ss to low -> activate slave
	SLAVE_SEL();

	rfm_spi_write(cmd, cmdsize);

	// deactivate SPI of slave
	slave_desel_and_wait();

	// and now wait for CTS signal
	uint32_t systick_old = tickcount();
	while (CTS_IS_LOW())
	{
		if (tickcount() - systick_old > RFM_CTS_TIMEOUT)
		{
			// failed, no cts after timeout
			return RFM_RES_NOT_RESPONDING;
		}
	}

	if (ressize > 0)
	{
		// Need a small delay before reading the result, RMF26 is wired :(
		return rfm_get_data(RFM_CMD_READ_BUF, result, ressize, 1);
	}

	return RFM_RES_OK;
}

/*
 * Reads a given amount of data from the rx fifo
 */
static uint8_t rfm_read_rx_fifo(uint8_t size, void *data)
{
	return rfm_get_data(RFM_CMD_READ_RX_FIFO, data, size, 0);
}


/*
 * Writes data to the tx fifo buffer
 * The WRITE_TX_FIFO command is a bit special, so i can't use rfm_command here
 */
static uint8_t rfm_write_tx_fifo(uint8_t size, const void *data)
{
	// set ss to low -> activate slave
	SLAVE_SEL();

	rfm_spi_write_byte(RFM_CMD_WRITE_TX_FIFO);
	rfm_spi_write(data, size);

	// deactivate SPI of slave
	slave_desel_and_wait();


	return RFM_RES_OK;
}

#if 0
/*
 * Reads a fast response register value
 * The <read_cmd> parameter is the command used to read this register, it should be one of the following:
 * RFM_CMD_FAST_RESPONSE_A
 * RFM_CMD_FAST_RESPONSE_B
 * RFM_CMD_FAST_RESPONSE_C
 * RFM_CMD_FAST_RESPONSE_D
 */
static inline uint8_t rfm_read_frr(uint8_t read_cmd, uint8_t *val)
{
	return rfm_get_data(read_cmd, val, 1, 0);
}
#endif


/*
 * Reads the RFM's interrupt status
 * reset specify the interrupt bits to reset
 * result contains the pending interrupts
 */
static uint8_t rfm_get_int_status(uint32_t reset, uint32_t *result)
{

	uint8_t int_res[8];
	uint8_t int_cmd[4];

	int_cmd[0] = RFM_CMD_GET_INT_STATUS;
	int_cmd[1] = ~((reset >> 16) & 0xFF);
	int_cmd[2] = ~((reset >> 8) & 0xFF);
	int_cmd[3] = ~(reset & 0xFF);

	uint8_t r = rfm_command(int_cmd, sizeof(int_cmd), int_res, sizeof(int_res));

	if(r != RFM_RES_OK)
	{
		return r;
	}

	if(result != NULL)
	{
		*result =  ((uint32_t)int_res[0]) << 24;
		*result |= ((uint32_t)int_res[2]) << 16;
		*result |= ((uint32_t)int_res[4]) << 8;
		*result |= ((uint32_t)int_res[6]);
	}

	return RFM_RES_OK;
}


/*
 * Reads the devices status from first fast response register
 */
static inline uint8_t rfm_get_state(uint8_t *s)
{
	//return rfm_read_frr(RFM_CMD_FAST_RESPONSE_A, s);
	
	uint8_t status_cmd = RFM_CMD_REQUEST_DEVICE_STATE;
	uint8_t status_res[2];

	uint8_t cmd_res = rfm_command(&status_cmd, sizeof(status_cmd), status_res, sizeof(status_res));
	if (cmd_res != RFM_RES_OK)
	{
		return cmd_res;
	}

	*s = status_res[0] & 0x0F;
	return RFM_RES_OK;
}


/*
 * Retrives the number of used bytes in the rx fifo and the number of free bytes in the rx buffer
 */
static uint8_t rfm_get_fifo_usage(uint8_t *rx_used, uint8_t *tx_free)
{
	uint8_t fifo_res[2];
	uint8_t fifo_cmd[] = {RFM_CMD_FIFO_INFO, 0};

	uint8_t r = rfm_command(fifo_cmd, sizeof(fifo_cmd), fifo_res, sizeof(fifo_res));
	if(r != RFM_RES_OK)
	{
		return r;
	}

	if(rx_used != NULL)
	{
		*rx_used = fifo_res[0];
	}

	if(tx_free != NULL)
	{
		*tx_free = fifo_res[1];
	}

	return RFM_RES_OK;
}


/*
 * Reset the fifo buffers
 */
static uint8_t rfm_reset_fifo(uint8_t reset_rx, uint8_t reset_tx)
{
	uint8_t fifo_cmd[] = {RFM_CMD_FIFO_INFO, 0};

	if(reset_rx)
	{
		fifo_cmd[1] |= 0x02;
	}

	if(reset_tx)
	{
		fifo_cmd[1] |= 0x01;
	}

	uint8_t r = rfm_command(fifo_cmd, sizeof(fifo_cmd), NULL, 0);
	if(r != RFM_RES_OK)
	{
		return r;
	}

	return RFM_RES_OK;
}


static uint8_t rfm_run_config(void)
{
	static const uint8_t CONFIG_DATA[] = RADIO_CONFIGURATION_DATA_ARRAY; 

	const uint8_t *ptr = CONFIG_DATA;

	while(ptr[0])
	{
		uint8_t cmd_length = ptr[0];
		ptr++;

		uint8_t r = rfm_command(ptr, cmd_length, NULL, 0);
		if (r != RFM_RES_OK)
		{
			return r;
		}

		ptr += cmd_length;

	}
	return RFM_RES_OK;
}

static uint8_t rfm_init(void)
{
	uint8_t cmdbuf[32];
	uint8_t resbuf[32];

	cmdbuf[0] = RFM_CMD_PART_INFO;

	uint8_t r = rfm_command(cmdbuf, 1, resbuf, 8);
	if (r != RFM_RES_OK)
	{
		return r;
	}
	else
	{
		if(resbuf[1] != 0x44 || resbuf[2] != 0x63) // Expect a 4463
		{
			return RFM_RES_UNEXPECTED_RESPONSE;
		}
	}

	return rfm_run_config();
}


static uint8_t rfm_set_antenna_switch(uint8_t tx)
{
	// Configure GPIOs of RFM module

	uint8_t gpio_cmd[] = {RFM_CMD_GPIO_PIN_CFG,
	                      RFM_GPIO_DONTCHANGE,  // GPIO0
	                      RFM_GPIO_DONTCHANGE,  // GPIO1
	                      RFM_GPIO_DONTCHANGE,  // GPIO2
	                      RFM_GPIO_DONTCHANGE,  // GPIO3
	                      RFM_GPIO_DONTCHANGE,  // NIRQ
	                      RFM_GPIO_DONTCHANGE,  // SDO
	                      0};                   // High output driver strength

	// Set switch state (switch is low active)
	if(tx)
	{
		gpio_cmd[RFM_ANTSW_TX + 1] = RFM_GPIO_LOW;
		gpio_cmd[RFM_ANTSW_RX + 1] = RFM_GPIO_HIGH;
	}
	else
	{
		gpio_cmd[RFM_ANTSW_TX + 1] = RFM_GPIO_HIGH;
		gpio_cmd[RFM_ANTSW_RX + 1] = RFM_GPIO_LOW;
	}

	return rfm_command(gpio_cmd, sizeof(gpio_cmd), NULL, 0);
}


static uint8_t rfm_enter_rx_mode(void)
{	
	uint8_t r = rfm_set_antenna_switch(0);

	if(r != RFM_RES_OK)
	{
		return r;
	}

	uint8_t rx_cmd[] = {
		RFM_CMD_START_RX,
		0x00,  // Channel 0
		0x00,  // Start immediately
		0x00,  // Use length from packet handler
		0x00,  // Also length
		RFM_STATE_RX,    // Re-enter rx on timeout
		RFM_STATE_READY, // Enter ready state when valid packet received 
		RFM_STATE_READY, // Enter ready state when invalid packet received
	};

	return rfm_command(rx_cmd, sizeof(rx_cmd), 0, 0);
}


uint8_t RFM_driver_init(void)
{
	// Hardware initialization

	// Clock
	rcc_periph_clock_enable(RFM_RCC_GPIO);
	rcc_periph_clock_enable(RFM_RCC_SPI);


	// GPIO
	gpio_set_mode(RFM_SPI_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, RFM_SPI_PIN_MOSI);
	gpio_set_mode(RFM_SPI_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, RFM_SPI_PIN_SCK);
	gpio_set_mode(RFM_SPI_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_INPUT_FLOAT, RFM_SPI_PIN_MISO);

	gpio_set_mode(RFM_NSEL_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, RFM_NSEL_PIN);
	gpio_set_mode(RFM_SHDN_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, RFM_SHDN_PIN);
	gpio_set_mode(RFM_CTS_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_INPUT_FLOAT, RFM_CTS_PIN);
	gpio_set_mode(RFM_INT_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_INPUT_FLOAT, RFM_INT_PIN);


	// SPI
	spi_init_master(RFM_SPI,
	                RFM_SPI_PRESCALER,
	                SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE,
	                SPI_CR1_CPHA_CLK_TRANSITION_1,
	                SPI_CR1_DFF_8BIT,
	                SPI_CR1_MSBFIRST);


	SLAVE_DESEL();
	spi_enable(RFM_SPI);
	

	return RFM_module_reset();
}


uint8_t RFM_module_reset(void)
{
	// reset the module
	RFM_MODULE_DISABLE();
	delay_ticks(RFM_RESET_TIME);
	RFM_MODULE_ENABLE();
	delay_ticks(RFM_RESET_TIME);

	return rfm_init();
}


uint8_t RFM_rx_packet(void *packet, uint32_t timeout, uint8_t reset_fifo)
{
	uint8_t r;

	uint32_t start_time = tickcount();

	if(reset_fifo)
	{
		r = rfm_reset_fifo(1, 0);
		if(r != RFM_RES_OK)
		{
			return r;
		}

		// clear int bits
		r = rfm_get_int_status(RFM_INT_PH_CRC_ERROR | RFM_INT_PH_RX, NULL);
		if(r != RFM_RES_OK)
		{
			return r;
		}
	}

	
	while(tickcount() - start_time < timeout)
	{

		// Check int status
		
		uint32_t int_status;
		r = rfm_get_int_status(RFM_INT_PH_CRC_ERROR | RFM_INT_PH_RX, &int_status);
		if(r != RFM_RES_OK)
		{
			return r;
		}

		if(int_status & RFM_INT_PH_CRC_ERROR)
		{
			// crc error -> reset fifo
			r = rfm_reset_fifo(1, 0);
			if(r != RFM_RES_OK)
			{
				return r;
			}

			continue;
		}

		if(int_status & RFM_INT_PH_RX)
		{
			// yay, i got data -> check received length, this must be equal to configured packet length
			
			uint8_t data_avail;
			r = rfm_get_fifo_usage(&data_avail, NULL);

			if(r != RFM_RES_OK)
			{
				return r;
			}

			if(data_avail != RFM_PACKET_SIZE)
			{
				// reset the fifo
				// NOTE: I don't check the return value here because i want to return WRONGSIZE anyway.
				rfm_reset_fifo(1, 0);
				return RFM_RES_RX_WRONGSIZE;
			}

			rfm_read_rx_fifo(RFM_PACKET_SIZE, packet);

			return RFM_RES_OK;
		}

		// no data -> wait for more data
		
		// check device state
		uint8_t status;
		r = rfm_get_state(&status);
		if(r != RFM_RES_OK)
		{
			return r;
		}

		if(status != RFM_STATE_RX)
		{
			// rfm not in rx state -> start rx
			r = rfm_enter_rx_mode();

			if(r != RFM_RES_OK)
			{
				return r;
			}
		}

		// Wait for IRQ line
		while(IRQ_IS_HIGH())
		{
			if(tickcount() - start_time > timeout)
			{
				// timeout
				return RFM_RES_RX_TIMEOUT;
			}
		}
	}
	
	return RFM_RES_RX_TIMEOUT;
}


uint8_t RFM_tx_packet(const void *packet)
{
	uint8_t r;

	uint8_t oldstatus;
	r = rfm_get_state(&oldstatus);
	if(r != RFM_RES_OK)
	{
		return r;
	}

	if(oldstatus == RFM_STATE_RX)
	{
		// rfm is in rx state -> check if fifo empty
		
		uint8_t data_avail;
		r = rfm_get_fifo_usage(&data_avail, NULL);

		if(r != RFM_RES_OK)
		{
			return r;
		}
		
		if(data_avail != 0)
		{
			// currently receiving -> cancel
			return RFM_RES_TX_BUSY;
		}
	}

	// enable tx antenna
	r = rfm_set_antenna_switch(1);
	if(r != RFM_RES_OK)
	{
		return r;
	}

	r = rfm_write_tx_fifo(RFM_PACKET_SIZE, packet);
	if(r != RFM_RES_OK)
	{
		return r;
	}

	static const uint8_t start_tx_cmd[] = {
		RFM_CMD_START_TX,
		0,                    // Channel 0
		RFM_STATE_READY << 4, // Enter ready state after send
		0,                    // Length (0 -> from PH)
		0                     // Second length filed
	};

	r = rfm_command(start_tx_cmd, sizeof(start_tx_cmd), NULL, 0);
	if(r != RFM_RES_OK)
	{
		return r;
	}

	// ok, now the tx command is issued -> wait for tx complete
	uint32_t start_time = tickcount();

	uint8_t success_return = RFM_RES_TX_TIMEOUT;

	while(tickcount() - start_time < RFM_TX_TIMEOUT)
	{
		// Wait for IRQ line
		if(IRQ_IS_HIGH())
		{
			continue;
		}

		// Check int status
		
		uint32_t int_status;
		r = rfm_get_int_status(RFM_INT_PH_SENT, &int_status);
		if(r != RFM_RES_OK)
		{
			return r;
		}

		if(!(int_status & RFM_INT_PH_SENT))
		{
			// not finished yet
			continue;
		}

		// packet sent successfull
		success_return = RFM_RES_OK;
		break;
	}

	if(oldstatus == RFM_STATE_RX || oldstatus == RFM_STATE_RX_TUNE)
	{
		// was in rx mode -> return to rx mode
		r = rfm_enter_rx_mode();
		if(r != RFM_RES_OK)
		{
			return r;
		}
	}
	
	return success_return;
}


uint8_t RFM_raw_cmd(const void *cmd, uint32_t cmd_size, void *result, uint32_t result_size)
{
	return rfm_command(cmd, cmd_size, result, result_size);
}

#if 0
// TEST!!!
//
#include <string.h>
static inline void configureUSART()
{
	
	// Initialize registers with zero
	USART1->CR1 = 0;
	USART1->CR2 = 0;
	USART1->CR3 = 0;

	// Begin configuration
	// Enable USART
	USART1->CR1 |= USART_CR1_UE;

	// Configure baud rate
	USART1->BRR = 0x0010; // 1 (4.5Mbs @ 72Mhz)

	// Enable DMA
	USART1->CR3 |= USART_CR3_DMAT;

	// Enable Transmitter
	USART1->CR1 |= USART_CR1_TE;

	// Enable Receiver
	USART1->CR1 |= USART_CR1_RE;
}

static void sendraw(const void *data, size_t len)
{
	for(size_t i = 0; i < len; i++)
	{
		// Wait for TX buffer empty
		while((USART1->SR & USART_SR_TXE) == 0)
		{
			continue;
		}
		// Write data to data register
		// This automatically clears the TXE bit
		USART1->DR = ((unsigned char *)data)[i];
	}
}

static void sendstr(const char *str)
{
	sendraw(str, strlen(str));
}

static void rx_wait(char *buffer, uint32_t size)
{
	char c;
	while(size > 1)
	{
		// Wait for data
		while((USART1->SR & USART_SR_RXNE) == 0);

		// Read data, this clears the RXNE bit
		c = USART1->DR;

		if(c == '\n') break;

		(*(buffer++)) = c;
		size--;
	}

	(*buffer) = 0;
}

void hexdump(const void *data, size_t len)
{
	uint8_t hexBuffer[128];
	size_t bufPos = 0;
	while(len--)
	{
		uint8_t d = *((uint8_t *)data);
		uint8_t a = d >> 4;
		uint8_t b = d & 0x0F;
		if(a >= 10) a += 'a' - 10;
		else a += '0';
		if(b >= 10) b += 'a' - 10;
		else b += '0';

		hexBuffer[bufPos] = a;
		hexBuffer[bufPos + 1] = b;

		data++;
		bufPos += 2;
		if(bufPos >= sizeof(hexBuffer) - 1)
		{
			sendraw(hexBuffer, sizeof(hexBuffer));
			bufPos = 0;
		}

	}
	if(bufPos != 0)
	{
		sendraw(hexBuffer, bufPos);
	}
}


uint8_t RFM_test()
{
	RCC->APB2ENR |= RCC_APB2ENR_USART1EN | RCC_APB2ENR_IOPAEN;
	GPIOA->CRH &= ~(GPIO_CRH_MODE9 | GPIO_CRH_CNF9);
	GPIOA->CRH |= GPIO_CRH_MODE9 | GPIO_CRH_CNF9_1;
	configureUSART();

	uint8_t ir = RFM_init();
	sendstr("init: ");
	hexdump(&ir, 1);
	sendstr("\n");
	



	uint8_t r;

	while(1)
	{
		uint8_t buf[128];
		rx_wait((char *)buf, sizeof(buf) / 2);
		sendstr("CMD: ");
		sendstr((char *)buf);

		if(buf[0] == 'H')
		{

			sendstr("\nRESET\n");
			r = RFM_module_reset();
			if(r == RFM_RES_OK)
			{
				sendstr("\nINIT OK\n");
			}
			else
			{
				sendstr("\nINIT FAILED: ");
				hexdump(&r, 1);
				sendstr("\n");
			}
			continue;
		}
		else if(buf[0] == 's' || buf[0] == 'S')
		{

			sendstr("\nManual ant sw test\n");
			r = rfm_set_antenna_switch(buf[0] == 'S');
			if(r == RFM_RES_OK)
			{
				sendstr("\nOK\n");
			}
			else
			{
				sendstr("\nFAILED: ");
				hexdump(&r, 1);
				sendstr("\n");
			}
			continue;
		}
		else if(buf[0] == 'i' || buf[0] == 'I')
		{
			if(HAL_GPIO_ReadPin(RFM_INT_PORT, RFM_INT_PIN))
			{
				sendstr("\nHIGH\n");
			}
			else
			{
				sendstr("\nLOW\n");
			}

			uint32_t intRes = 0;
			if(buf[0] == 'i')
			{
				r = rfm_get_int_status(0, &intRes);
			}
			else
			{
				r = rfm_get_int_status(~0, &intRes);
			}

			if(r != RFM_RES_OK)
			{
				sendstr("\nFailed to get interrupt status");
				hexdump(&r, 1);
				sendstr("\n");
				continue;
			}
			sendstr("int status: ");
			hexdump(&intRes, sizeof(intRes));

			continue;
		}
		else if(buf[0] == 'r' || buf[0] == 'R')
		{
			while(USART1->DR != 'x')
			{
				uint8_t rb[RFM_PACKET_SIZE];
				r = RFM_rx_packet(rb, 1000, buf[1] != 0);

				sendstr("\nrx result: ");
				hexdump(&r, 1);
				sendstr("\n");

				if(r == RFM_RES_OK)
				{
					sendstr("-> RX OK -> DUMP DATA\n");
					hexdump(rb, sizeof(rb));
					sendstr("\n");
				}

				if(buf[0] == 'r')
				{
					break;
				}

				if(buf[1] != 'F')
				{
					buf[1] = 0;
				}
			}
			continue;
		}
		else if(buf[0] == 't' || buf[0] == 'T')
		{
			sendstr("\ntx data:\n");
			hexdump(buf + 1, RFM_PACKET_SIZE);
			sendstr("\n");

			r = RFM_tx_packet(buf + 1);
			sendstr("-> TX complete, result: \n");
			hexdump(&r, 1);
			sendstr("\n");

			
			continue;
		}
		

		// De-hex buffer
		uint8_t size = 0;
		while(buf[size * 2] && buf[size * 2 + 1])
		{
			if(buf[size * 2] > '9') buf[size * 2] -= ('a' - 10);
			else buf[size * 2] -= '0';

			if(buf[size * 2 + 1] > '9') buf[size * 2 + 1] -= ('a' - 10);
			else buf[size * 2 + 1] -= '0';

			buf[size] = (buf[size * 2] << 4) | buf[size * 2 + 1];
			size++;
		}

		char resbuf[128];

		uint8_t res = 0;

		if(size == 2 && buf[0] == 0xff)
		{
			sendstr("\nFR request\n");
			res = rfm_get_data(buf[1], (uint8_t*)resbuf, 128, 0);
		}
		else
		{
			res = rfm_command(buf, size, (uint8_t*)resbuf, 128);
		}

		sendstr("\nfunc_res: ");
		hexdump(&res, 1);

		sendstr("\nresult\n");
		hexdump(resbuf, sizeof(resbuf));
		sendstr("\n");

	}
}
#endif
