#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/nvic.h>
#include <string.h>

#include "delay.h"
#include "usbacm.h"
#include "util.h"
#include "Si4463.h"
#include "Si4463_config.h"


#define ENABLE_DEBUG_USART


// command handling
static uint8_t command_valid = 0;
static uint32_t command_buffer_used = 0;
static uint8_t command_buffer[128];

static uint32_t get_next_comamnd_data(uint8_t *buffer);
static void print_raw(const void *data, uint32_t len);
static void print_str(const char *str);
static void print_hex(const uint8_t *data, uint32_t size);
static void print_help(void);

#ifdef ENABLE_DEBUG_USART
static void debug_usart_init(void);
#endif


static void rxtest(void)
{
	print_str("Start RX test\n");
	while (!get_next_comamnd_data(NULL))
	{
		print_str("Requesting packet from RFM driver...\n");

		uint8_t packet_buffer[RFM_PACKET_SIZE];

		uint8_t rfm_result = RFM_rx_packet(packet_buffer, 3000, 0);
		if(rfm_result == RFM_RES_OK)
		{
			print_str("OK, packet data:\n");
			print_hex(packet_buffer, sizeof(packet_buffer));
			print_str("\n");

			if (packet_buffer[0])
			{
				gpio_clear(GPIOC, GPIO13);
			}
			else
			{
				gpio_set(GPIOC, GPIO13);
			}
		}
		else
		{
			print_str("ERROR: ");
			print_hex(&rfm_result, 1);
			print_str("\n");
		}
	}
}


static void echotest_svr(void)
{
	print_str("Start echo server\n");
	while (!get_next_comamnd_data(NULL))
	{
		print_str("Requesting packet from RFM driver...\n");

		uint8_t packet_buffer[RFM_PACKET_SIZE];

		uint8_t rfm_result = RFM_rx_packet(packet_buffer, 3000, 0);
		if(rfm_result == RFM_RES_OK)
		{
			print_str("OK, packet data:\n");
			print_hex(packet_buffer, sizeof(packet_buffer));
			print_str("\n");

			rfm_result = RFM_tx_packet(packet_buffer);
			if(rfm_result == RFM_RES_OK)
			{
				print_str("OK, echo sent:\n");
			}
			else
			{
				print_str("TX ERROR: ");
				print_hex(&rfm_result, 1);
				print_str("\n");
			}
		}
		else
		{
			print_str("RX ERROR: ");
			print_hex(&rfm_result, 1);
			print_str("\n");
		}
	}
}


static void echotest_client(void)
{
	print_str("Start echo client\n");

	int counter = 0;
	uint8_t req_fresh = 0;

	while (!get_next_comamnd_data(NULL))
	{

		uint8_t tx_buffer[RFM_PACKET_SIZE];

		for (size_t i = 0; i < sizeof(tx_buffer) / sizeof(tx_buffer[0]); i++)
		{
			tx_buffer[i] = counter++;
		}

		gpio_clear(GPIOC, GPIO13);
		delay_ticks(100);
		gpio_set(GPIOC, GPIO13);

		print_str("Send packet:\n");
		print_hex(tx_buffer, sizeof(tx_buffer));
		print_str("\n");

		uint8_t rfm_result = RFM_tx_packet(tx_buffer);
		if(rfm_result == RFM_RES_OK)
		{
			uint8_t rx_buffer[RFM_PACKET_SIZE];
			print_str("waiting for response...\n");
			rfm_result = RFM_rx_packet(rx_buffer, 5000, req_fresh);
			req_fresh = 0;
			if(rfm_result == RFM_RES_OK)
			{
				print_str("OK, received:\n");
				print_hex(rx_buffer, sizeof(rx_buffer));
				print_str("\n");

				if (memcmp(rx_buffer, tx_buffer, sizeof(rx_buffer)))
				{
					gpio_clear(GPIOC, GPIO13);
					delay_ticks(3000);
					gpio_set(GPIOC, GPIO13);
					delay_ticks(500);
					print_str("TX != RX!!!\n");
					req_fresh = 1;
				}
				else
				{
					print_str("--- OK ---\n");

					for (uint32_t i = 0; i < 3; i++)
					{
						gpio_clear(GPIOC, GPIO13);
						delay_ticks(100);
						gpio_set(GPIOC, GPIO13);
						delay_ticks(100);
					}
				}
			}
			else
			{
				print_str("RX ERROR: ");
				print_hex(&rfm_result, 1);
				print_str("\n");

				gpio_clear(GPIOC, GPIO13);
				delay_ticks(1000);
				gpio_set(GPIOC, GPIO13);
				delay_ticks(500);

				req_fresh = 1;
			}
		}
		else
		{
			print_str("TX ERROR: ");
			print_hex(&rfm_result, 1);
			print_str("\n");
			for (uint32_t i = 0; i < 15; i++)
			{
				gpio_clear(GPIOC, GPIO13);
				delay_ticks(20);
				gpio_set(GPIOC, GPIO13);
				delay_ticks(20);
			}
		}
	}
}


int main(void)
{
	rcc_clock_setup_in_hse_8mhz_out_72mhz();

	// need to manually set to usb prescaler to 1.5 (USB needs 48Mhz)
	rcc_set_usbpre(RCC_CFGR_USBPRE_PLL_CLK_DIV1_5);

	delay_init();

	rcc_periph_clock_enable(RCC_GPIOC);

	gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);

	usbacm_init();

#ifdef ENABLE_DEBUG_USART
	debug_usart_init();
#endif


	// Switch on error LED
	// There is not actual error now, this is just to see that something
	// happens during boot / init
	// Also, this will stay active as long as the mudule is unitialized
	gpio_clear(GPIOC, GPIO13);
	
	print_str("initializing transceiver module...\n");
	
	uint8_t rfm_result = RFM_driver_init();

	while(rfm_result != RFM_RES_OK)
	{

		print_str("failed to initialize the RF module!\nerrorcode: ");
		print_hex(&rfm_result, 1);
		print_str("\nretry in 1 sec...\n");

		delay_ticks(1000);
		rfm_result = RFM_reset();
	}

	print_str("transceiver initialized\n");


	//rxtest();
	echotest_client();


	while (1) 
	{
		uint8_t cmd[sizeof(command_buffer)];

		uint32_t cmd_len = get_next_comamnd_data(cmd);
		if(cmd_len != 0)
		{
			switch(cmd[0])
			{
				case 'H':
				case 'h':
					print_help();
					break;

				case 'r':
				{
					print_str("Requesting packet from RFM driver...\n");

					uint8_t packet_buffer[RFM_PACKET_SIZE];

					rfm_result = RFM_rx_packet(packet_buffer, 100, 0);
					if(rfm_result == RFM_RES_OK)
					{
						print_str("OK, packet data:\n");
						print_hex(packet_buffer, sizeof(packet_buffer));
						print_str("\n");
					}
					else
					{
						print_str("\nERROR: ");
						print_hex(&rfm_result, 1);
						print_str("\n");
					}
					break;
				}
				case 't':
					if(cmd_len != RFM_PACKET_SIZE * 2 + 1)
					{
						print_str("Argument length must be equal to packet size\n");
						break;
					}

					if(!hex_decode((const char *)(cmd + 1), cmd_len - 1, cmd + 1))
					{
						print_str("Failed to parse packet data (must be HEX encoded)\n");
						break;
					}


					print_str("Sending packet with data: \n");
					print_hex(cmd + 1, RFM_PACKET_SIZE);

					rfm_result = RFM_tx_packet(cmd + 1);
					if(rfm_result == RFM_RES_OK)
					{
						print_str("\nOK\n");
					}
					else
					{
						print_str("\nERROR: ");
						print_hex(&rfm_result, 1);
						print_str("\n");
					}

					break;

				case 'T':
				{
					if(cmd_len != 2 || (cmd[1] != '0' && cmd[1] != '1'))
					{
						print_str("USAGE: T0 / T1\n");
						break;
					}


					uint8_t packet_buffer[RFM_PACKET_SIZE];
					memset(packet_buffer, 0, RFM_PACKET_SIZE);

					if (cmd[1] == '1')
					{
						packet_buffer[0] = 1;
					}

					print_str("Sending packet with data: \n");
					print_hex(packet_buffer, RFM_PACKET_SIZE);

					rfm_result = RFM_tx_packet(packet_buffer);
					if(rfm_result == RFM_RES_OK)
					{
						print_str("\nOK\n");
					}
					else
					{
						print_str("\nERROR: ");
						print_hex(&rfm_result, 1);
						print_str("\n");
					}

					break;
				}

				case 'x':
					if(!hex_decode((const char *)(cmd + 1), cmd_len - 1, cmd + 1))
					{
						print_str("Failed to parse command data (must be HEX encoded)\n");
						break;
					}

					uint32_t raw_data_len = (cmd_len - 1) / 2;
					uint8_t result_buffer[16];

					print_str("Sending raw command: \n");
					print_hex(cmd + 1, raw_data_len);

					rfm_result = RFM_raw_cmd(cmd + 1, raw_data_len, result_buffer, sizeof(result_buffer));
					if(rfm_result == RFM_RES_OK)
					{
						print_str("\nOK, result:\n");
						print_hex(result_buffer, sizeof(result_buffer));
						print_str("\n");
					}
					else
					{
						print_str("\nERROR: ");
						print_hex(&rfm_result, 1);
						print_str("\n");
					}

					break;

				case 'R':
					if(cmd_len == 1)
					{
						print_str("Missing argument for reset command\n");
						break;
					}

					if(cmd[1] == 'T')
					{
						print_str("Resetting transceiver...\n");
						rfm_result = RFM_reset();
						if(rfm_result == RFM_RES_OK)
						{
							print_str("OK\n");
						}
						else
						{
							print_str("ERROR: ");
							print_hex(&rfm_result, 1);
							print_str("\n");
						}
					}
					else if(cmd[1] == 'M')
					{
						print_str("Resetting MCU...\n");
						delay_ticks(200); // wait 200ms to allow USB to finish transmission
						scb_reset_system();
					}
					else
					{
						print_str("Invalid argument for reset command\n");
					}

					break;

				case 'e':
					echotest_client();
					break;
				case 'E':
					echotest_svr();
					break;
			}
		}

		delay_ticks(100);
		gpio_toggle(GPIOC, GPIO13);
	}
}


/*
 * Returns the next comamnd
 * The buffer must be as large as the command_buffer
 */
static uint32_t get_next_comamnd_data(uint8_t *buffer)
{
	if (!command_valid)
	{
		return 0;
	}

	if (!buffer)
	{
		return 1;
	}

	memcpy(buffer, command_buffer, command_buffer_used);

	uint32_t res_size = command_buffer_used;

	// reset buffer
	command_buffer_used = 0;
	command_valid = 0;

	return res_size;

}


static inline void print_raw(const void *data, uint32_t len)
{
	usbacm_send(data, len);

#ifdef ENABLE_DEBUG_USART

	const unsigned char *d = data;
	while(len--)
	{
		usart_send_blocking(USART1, (d++)[0]);
	}
#endif
}


static void print_str(const char *str)
{
	print_raw(str, strlen(str));
}


static void print_hex(const uint8_t *data, uint32_t size)
{
	while(size > 0)
	{
		char tmp[64];
		uint32_t cur = size;
		if(cur > sizeof(tmp) / 2)
		{
			cur = sizeof(tmp) / 2;
		}

		hex_encode(data, cur, tmp);

		print_raw(tmp, cur * 2);

		data += cur;
		size -= cur;
	}
}


static void print_help(void)
{
	static const char help_string [] = "\
== SERIAL INTERFACE USAGE ==\n\
A COMMAND is a line (line is ended by \\n\n\
Each COMMAND is one char instruction followed by the parameters.\n\n\
VALID INSTRUCTIONS:\n\
h/H     show help\n\
r       receive a packet\n\
t       transmit a packet\n\
        t must be followed by exactly the number of bytes as specified in the packet size\n\
        this parameter is hexadecimal encoded\n\
x       Send a raw command to the transceiver module\n\
        The (hex encoded) command is specified as parameter\n\
        This function is intended for testing / debugging! only\n\
R       reset\n\
        RT resets the rf transceiver\n\
        RM resets the MCU (and thus also the transceiver)\n\
";
	print_raw(help_string, sizeof(help_string));
}


// callback from virtual com port
void usbacm_recv_handler(void *data, uint32_t len)
{
	if(command_valid)
	{
		// there is a valid (unhandled) command -> discard new data
		return;
	}

	if(command_buffer_used + len > sizeof(command_buffer))
	{
		len = sizeof(command_buffer) - command_buffer_used;
	}

	// bytewise copy data to command buffer
	// (check for \n while copying)
	
	const uint8_t *data_ptr = (const uint8_t *)data;
	while (len--)
	{
		uint8_t x = *(data_ptr++);

		if(x == '\n')
		{
			command_valid = 1;
			break;
		}

		command_buffer[command_buffer_used++] = x;
	}
}


static inline void debug_usart_init(void)
{
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_USART1);

	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_USART1_TX);
	usart_set_baudrate(USART1, 4500000);
	usart_set_databits(USART1, 8);
	usart_set_stopbits(USART1, USART_STOPBITS_1);
	usart_set_mode(USART1, USART_MODE_TX_RX);
	usart_set_parity(USART1, USART_PARITY_NONE);
	usart_set_flow_control(USART1, USART_FLOWCONTROL_NONE);

	usart_enable_rx_interrupt(USART1);

	// Set USART int prio to same as USB so that they won't interrupt each other
	nvic_set_priority(NVIC_USART1_IRQ, USBACM_INT_PRIO);
	nvic_enable_irq(NVIC_USART1_IRQ);

	usart_enable(USART1);
}


void usart1_isr(void)
{
	uint8_t d = usart_recv(USART1);
	usbacm_recv_handler(&d, 1);
}
