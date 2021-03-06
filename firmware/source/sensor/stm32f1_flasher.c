/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


/*
 * STM32 runtime bootloader-like programming interface.
 * The flash procedure overwrites itself so it will destroy itself while programming.
 * If anything goes wrong during flashing, this will 'brick' the device so when using this you MUST have a backup plan for recovery.
 *
 * NOTE:
 *
 * You are now leaving the realm of standard c.
 * The following code might not work in future compiler versions!
 */

#include "flasher.h"
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/cortex.h>

#include <stdint.h>
#include <string.h>
#include "utils.h"
#include "watchdog.h"
#include "tinyprintf.h"

// Write everything but the config page
#define NUM_PAGES_TO_FLASH 63

// 128 16-bit per block
#define FLASHER_BLOCK_SIZE 128

#define FLASH_KEY1 0x45670123
#define FLASH_KEY2 0xCDEF89AB

/*
 * Need to use macros instead of function calls to avoid addressing problems
 */
#define READCHAR()   ({ while ((USART_SR(USART1) & USART_SR_RXNE) == 0); USART_DR(USART1); })
#define READCHAR_NORST()   ({ while ((USART_SR(USART1) & USART_SR_RXNE) == 0) WATCHDOG_FEED(); USART_DR(USART1); })
#define WRITECHAR(C) while ((USART_SR(USART1) & USART_SR_TXE) == 0); USART_DR(USART1) = C
#define FLASH_WAIT() while ((FLASH_SR & FLASH_SR_BSY) != 0) WATCHDOG_FEED()


/*
 * The total number of loop cycles.
 * In each cycle FLASHER_BLOCK_SIZE * 2 bytes are written
 */
#define NUM_FLASH_LOOP_CYCLES ((NUM_PAGES_TO_FLASH * 1024) / FLASHER_BLOCK_SIZE / 2)

#define FLASH_START 0x08000000

static void flasher_function(void)
{
	/*
	 * This is the flasher function, it is copied to the RAM before executing.
	 * DO NOT call any API function form here!!!
	 */

	__asm volatile ("nop");
	__asm volatile ("nop");
	__asm volatile ("nop");
	__asm volatile ("nop");

	// Read init sequence

	WRITECHAR('$');
	if (READCHAR() != 'A' ||
		READCHAR() != 'B' ||
		READCHAR() != 'C')
	{
		WRITECHAR('N');
		// Fail: Invalid int sequence -> reset the device by waiting in an infinite loop for the wd
		for(;;);
	}

	WRITECHAR('U');


	// Unlock the flash controller
	FLASH_KEYR = FLASH_KEY1;
	FLASH_KEYR = FLASH_KEY2;

	uint16_t buffer[FLASHER_BLOCK_SIZE];

	while (1)
	{
		uint8_t cmd = READCHAR_NORST();

		if (cmd == 'e')
		{
			FLASH_WAIT();
			FLASH_CR |= FLASH_CR_MER;
			FLASH_CR |= FLASH_CR_STRT;
			FLASH_WAIT();
			FLASH_CR &= ~FLASH_CR_MER;
			WRITECHAR('E');
		}
		else if (cmd == 'w')
		{
			uint16_t *flash = (uint16_t *)FLASH_START;
			uint32_t counter = NUM_FLASH_LOOP_CYCLES;

			// Activate write mode
			FLASH_CR |= FLASH_CR_PG;

			while (counter--)
			{
				WRITECHAR('.');

				for (uint32_t i = 0; i < FLASHER_BLOCK_SIZE; i++)
				{
					uint8_t a = READCHAR_NORST();
					uint8_t b = READCHAR_NORST();
					buffer[i] = (((uint16_t)b) << 8) + a;
				}

				for (uint32_t i = 0; i < FLASHER_BLOCK_SIZE; i++)
				{
					// Write flash
					(*flash) = buffer[i];
					// Wait for write done
					FLASH_WAIT();
					flash++;
				}
			}
			// Disable write mode
			FLASH_CR &= ~FLASH_CR_PG;
			WRITECHAR('W');
		}
		else if (cmd == 'v')
		{
			uint16_t *flash = (uint16_t *)FLASH_START;
			uint32_t counter = NUM_FLASH_LOOP_CYCLES;

			uint8_t failed = 0;

			while (counter--)
			{
				WRITECHAR('.');

				for (uint32_t i = 0; i < FLASHER_BLOCK_SIZE; i++)
				{
					uint8_t a = READCHAR_NORST();
					uint8_t b = READCHAR_NORST();
					buffer[i] = (((uint16_t)b) << 8) + a;
				}

				for (uint32_t i = 0; i < FLASHER_BLOCK_SIZE; i++)
				{
					// Verify the data
					if ((*flash) != buffer[i])
					{
						// FAIL: Restart flash loop
						failed = 1;
						break;
					}
					flash++;
				}

				if (failed)
				{
					break;
				}
			}

			if (failed)
			{
				// Verify failed
				WRITECHAR('!');
			}
			else
			{
				WRITECHAR('V');
			}
		}
		else if (cmd == 'x')
		{
			WRITECHAR('X');
			// Wait for WDT reset
			for(;;);
		}
		else
		{
			WRITECHAR('~');
		}
	}
}

/*
 * The end of the flasher function.
 * Normally the compiler won't reorder this so it may work as intended.
 */
static void flasher_function_end(void) {}


/*
 * I need a dummy rx callback for changing the usart baudrate.
 */
static void dummy_rx_cb(void *arg, uint8_t data)
{
	(void)arg;
	(void)data;
}

void flasher_start(uint32_t baudrate)
{
	/*
	 * This function initializes flashing procedure.
	 * The actual flasher logic is a function that is executed from the ram.
	 * When the flasher is started, all interrupts are disabled to avoid interference from the normal program.
	 */

	if (((size_t)&flasher_function_end) < ((size_t)&flasher_function))
	{
		printf("Wrong function order for flasher function!\n1");
		return;
	}


	printf("About to change the baudrate for flashing to %lu.\nSee you on the other side.\n", baudrate);
	// We dont want any interrupt to interfere with our flasher
	cm_disable_interrupts();

	// Stop the USART DMA
	USART1_CR3 &= ~USART_CR3_DMAR;

	// Turn on the LSI, this is needed for flashing
	RCC_CR |= (RCC_CR_HSION);
	while (!(RCC_CR & RCC_CR_HSIRDY)) {}

	// Change the baudrate for flashing
	usart_set_baudrate(USART1, baudrate);

	// Wait some time to allow host to change baudrate
	for (uint32_t volatile i = 0; i < 5000000; i++);

	// Don't forget to feed the dog
	WATCHDOG_FEED();

	uint8_t function_buffer[(flasher_function_end - flasher_function) + 15];

	// The function must have the same alignment in the RAM
	uint32_t align = ((uint32_t)flasher_function) % 16;
	uint32_t align2 = ((uint32_t)function_buffer) % 16;

	align = (align + align2) % 16;
	printf("copy funtion with size %lx from %lx to %lx, align=%lx\n", (uint32_t)(flasher_function_end - flasher_function), (uint32_t)flasher_function, (uint32_t)function_buffer, align);
	memcpy(function_buffer + align, &flasher_function, flasher_function_end - flasher_function);

	// Round addres to next 4 byte boundary, this seems to work reliable
	uint32_t addr = ((((uint32_t)function_buffer + align) - 1) | 0x03) + 1;
	printf("About to set pc to %lx!", addr);

	__asm volatile ("mov pc, %0" : : "r" (addr));
}
