#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/cm3/scb.h>
#include <string.h>

#include "delay.h"
#include "usbacm.h"
#include "util.h"
#include "RFM_26.h"
#include "RFM_26_config.h"


// command handling
static uint8_t command_valid = 0;
static uint32_t command_buffer_used = 0;
static uint8_t command_buffer[128];

static uint32_t get_next_comamnd_data(uint8_t *buffer);
static void print_str(const char *str);
static void print_hex(const uint8_t *data, uint32_t size);
static void print_help(void);

int main(void)
{
	rcc_clock_setup_in_hse_8mhz_out_72mhz();

	// need to manually set to usb prescaler to 1.5 (USB needs 48Mhz)
	rcc_set_usbpre(RCC_CFGR_USBPRE_PLL_CLK_DIV1_5);

	
	delay_init();


	rcc_periph_clock_enable(RCC_GPIOC);

	gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);

	usbacm_init();


	// Switch on error LED
	// There is not actual error now, this is just to see that something
	// happens during boot / init
	// Also, this will stay active as long as the mudule is unitialized
	gpio_clear(GPIOC, GPIO13);
	
	uint8_t rfm_result = RFM_driver_init();

	while(rfm_result != RFM_RES_OK)
	{

		print_str("failed to initialize the RFM module!\nerrorcode: ");
		print_hex(&rfm_result, 1);
		print_str("\nretry in 1 sec...\n");

		delay_ticks(1000);
		rfm_result = RFM_module_reset();

		// test
		break;
	}


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
						rfm_result = RFM_module_reset();
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
	if(!command_valid)
	{
		return 0;
	}

	memcpy(buffer, command_buffer, command_buffer_used);

	uint32_t res_size = command_buffer_used;

	// reset buffer
	command_buffer_used = 0;
	command_valid = 0;

	return res_size;

}


static void print_str(const char *str)
{
	usbacm_send(str, strlen(str));
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

		usbacm_send(tmp, cur * 2);

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
        RT resets the rf tranceiver\n\
        RM resets the MCU (and thus also the transceiver)\n\
";
	usbacm_send(help_string, sizeof(help_string));
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
		len = sizeof(command_buffer);
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
