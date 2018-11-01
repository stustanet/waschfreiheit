/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#include "usb_storage_helper.h"
#include "pff.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/f4/nvic.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/scb.h>

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "status.h"


// Clock for the STM32F401
const struct rcc_clock_scale hse_8_84mhz =
{
	.pllm = 4,     // PLL input is 2MHz
	.plln = 168,   // PLL output is 336 MHz
	.pllp = 4,     // CPU clock 84 MHz
	.pllq = 7,     // USB clock 48MHz
	.hpre = RCC_CFGR_HPRE_DIV_NONE,
	.ppre1 = RCC_CFGR_PPRE_DIV_2,
	.ppre2 = RCC_CFGR_PPRE_DIV_NONE,
	.flash_config = FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_2WS,
	.ahb_frequency  = 84000000,
	.apb1_frequency = 42000000,
	.apb2_frequency = 84000000,
};

#define CLOCK_SETTINGS hse_8_84mhz

#define FLASH_SIZE  (1 << 18)  /* 256 KB */
#define APP_OFFSET  0x00008000
static const uint8_t SECTOR_ERASE_LIST[] = {2, 3, 4, 5};


#define FLASH_START 0x08000000
#define APP_START ((uint32_t)(FLASH_START + APP_OFFSET))
#define MAX_IMAGE_SIZE (FLASH_SIZE - APP_OFFSET)

#define DEBUG_USART USART1

#define USB_PWRSW_GPIO_RCC  RCC_GPIOB
#define USB_PWRSW_GPIO_PORT GPIOB
#define USB_PWRSW_GPIO_PIN  GPIO0


static void delay(void)
{
	for (volatile uint32_t i = 0; i < 2000000; i++);
}


static void print_ch(char c)
{
	usart_send_blocking(DEBUG_USART, c);
}

#ifdef LUSBH_USART_DEBUG
static void tpf_putcf(void *ptr, char c)
{
	(void)ptr;
	print_ch(c);
}
#endif


static void print_str(const char *str)
{
	while (*str)
	{
		print_ch(*str);
		str++;
	}
}

static void print_num(uint32_t num)
{
	char buffer[11];
	itoa(num, buffer, 10);
	print_str(buffer);
}

static void print_num_hex(uint32_t num)
{
	char buffer[9];
	itoa(num, buffer, 16);
	print_str(buffer);
}


/*
static void hexdump(const void *ptr, uint32_t len)
{
	for (uint32_t i = 0; i < len; i++)
	{
		char buffer[3];
		itoa(((const uint8_t *)ptr)[i], buffer, 16);
		if (!buffer[1])
		{
			print_ch('0');
		}
		print_str(buffer);
		print_ch(' ');
		if (i % 16 == 15)
		{
			print_ch('\n');
		}
		else if (i % 4 == 3)
		{
			print_ch(' ');
		}
	}
	print_ch('\n');
}
*/


static void print_text_and_num(const char *text, uint32_t num)
{
	print_str(text);
	print_num(num);
	print_ch('\n');
}

#define PRINT_NUML(text, num) print_text_and_num(text, num);
#define PRINT_NUM(num) print_num(num);
#define PRINT_NUM_HEX(num) print_num_hex(num);
#define PRINT(text) print_str(text);

/*
 * CRC32 function adapted from
 * https://www.mikrocontroller.net/topic/155115
 *
 * NOTE: I don't use the hardware CRC because it can only handle words and not bytes
 */
static uint32_t crc32(uint32_t crc, unsigned char byte)
{
	int i;
	const uint32_t polynom = 0xEDB88320;

    for (i = 0; i < 8; ++i)
	{
        if ((crc & 1) != (byte & 1))
		{
			crc = (crc>>1)^polynom;
		}
        else
		{
			crc >>= 1;
		}
		byte >>= 1;
	}
	return crc;
}


static void init_usart(void)
{
	rcc_periph_clock_enable(RCC_GPIOA);
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9 | GPIO10);
	gpio_set_af(GPIOA, GPIO_AF7, GPIO9 | GPIO10);


	rcc_periph_clock_enable(RCC_USART1);
	usart_set_baudrate(DEBUG_USART, 115200);
	usart_set_mode(DEBUG_USART, USART_MODE_TX | USART_MODE_RX);
	usart_enable(DEBUG_USART);
}


static void poll_timer_setup(void)
{
	rcc_periph_clock_enable(RCC_TIM5);
	rcc_periph_reset_pulse(RST_TIM5);
	timer_set_prescaler(TIM5, 81); // Prescaler 82 -> 1MHz
	timer_set_period(TIM5, 1000); // Overflow in 1ms
	timer_enable_counter(TIM5);
	timer_enable_irq(TIM5, TIM_DIER_UIE);
	nvic_enable_irq(NVIC_TIM5_IRQ);
}


void tim5_isr(void)
{
	timer_clear_flag(TIM5, TIM_SR_UIF);
	usb_storage_poll();
}


static void mount_and_process(void)
{
	status_update(STATUS_WAIT_USB);

	PRINT("Waiting for USB device\n");
	bool storage_ok = usb_storage_wait(5000);

	if (!storage_ok)
	{
		status_update(STATUS_USB_TIMEOUT);
		PRINT("No USB device connected\n");
		return;
	}

	status_update(STATUS_USB_OK);
	PRINT("Detected USB mass storage device\n");

	FATFS fs = {0};
	FRESULT res = pf_mount(&fs);

	if (res != FR_OK)
	{
		status_update(STATUS_MOUNT_ERROR);
		PRINT_NUML("mount failed with error ", res);
		return;
	}

	res = pf_open("CHECKSUM.CRC");
	if (res != FR_OK)
	{
		status_update(STATUS_NO_CHECKSUM);
		PRINT_NUML("No CHECKSUM.CRC file, err=", res);
		return;
	}


	UINT br;
	char checksum[9];
	res = pf_read(checksum, 8, &br);
	if (res != FR_OK || br == 0 || br > 8)
	{
		status_update(STATUS_NO_CHECKSUM);
		PRINT_NUML("Failed to read checksum, err=", res);
		PRINT_NUML("br=", br);
		return;
	}

	checksum[br] = 0;

	res = pf_open("FIRMWARE.BIN");
	if (res != FR_OK)
	{
		status_update(STATUS_NO_FIRMWARE);
		PRINT_NUML("No FIRMWARE.BIN file, err=", res);
		return;
	}

	uint32_t size = fs.fsize;

	PRINT_NUML("Found firmware image with size ", size);
	if (size > MAX_IMAGE_SIZE)
	{
		status_update(STATUS_IMAGE_TOO_LARGE);
		PRINT("Firmware image size exceeds flash size!\n");
		return;
	}

	// read file and compare to flash
	uint32_t processed = 0;
	uint8_t buffer[512];
	uint8_t *const flash_ptr = (uint8_t *)APP_START;
	uint32_t crc = 0xffffffffUL;

	bool equal_to_flash = true;

	PRINT("Reading firmware file...\n");
	for (;;)
	{
		res = pf_read(buffer, sizeof(buffer), &br);
		if (res != FR_OK)
		{
			status_update(STATUS_STORAGE_ERROR);
			PRINT_NUML("Error while reading the firmware file, err=", res);
			return;
		}


		// compare to flash
		if (equal_to_flash &&
			memcmp(buffer, flash_ptr + processed, br))
		{
			equal_to_flash = false;
		}

		processed += br;

		for (uint32_t i = 0; i < br; i++)
		{
			crc = crc32(crc, buffer[i]);
		}

		int percent = (processed * 100) / size;
		status_progress(PROGRESS_READ, percent);
		PRINT_NUM(percent);
		PRINT(" %\n");

		if (br != sizeof(buffer))
		{
			break;
		}
	}

	if (processed != size)
	{
		status_update(STATUS_STORAGE_ERROR);
		PRINT_NUML("Bytes read differ from file size! read=", processed);
		return;
	}

	crc = ~crc;


	uint32_t crcfile = strtoul(checksum, NULL, 16);
	if (crc != crcfile)
	{
		status_update(STATUS_WRONG_CHECKSUM);
		PRINT("Checksum of image does not match checksum of file!\nCHECKSUM.CRC: ");
		PRINT_NUM_HEX(crcfile);
		PRINT("\nFIRMWARE.BIN: ");
		PRINT_NUM_HEX(crc);
		return;
	}


	if (equal_to_flash)
	{
		status_update(STATUS_SAME_FIRMWARE);
		PRINT("New image is equal to current image!\n");
		return;
	}

	// Re-open the file to set the file ptr to the start
	res = pf_open("FIRMWARE.BIN");
	if (res != FR_OK)
	{
		status_update(STATUS_STORAGE_ERROR);
		PRINT_NUML("Failed to re-open firmware image, err=", res);
		return;
	}


	PRINT("Erasing flash...\n");
	flash_unlock();

	for (uint8_t s = 0; s < sizeof(SECTOR_ERASE_LIST); s++)
	{
		status_progress(PROGRESS_ERASE, (100 * s) / sizeof(SECTOR_ERASE_LIST));
		PRINT_NUML("Sector ", SECTOR_ERASE_LIST[s]);
		flash_erase_sector(SECTOR_ERASE_LIST[s], FLASH_CR_PROGRAM_X32);
	}

	PRINT("Writing new image...\n");
	processed = 0;
	for (;;)
	{
		res = pf_read(buffer, sizeof(buffer), &br);
		if (res != FR_OK)
		{
			status_update(STATUS_STORAGE_ERROR);
			PRINT_NUML("Error while reading the firmware file, err=", res);
			return;
		}

		flash_program(APP_START + processed, buffer, br);
		processed += br;

		int percent = (processed * 100) / size;
		status_progress(PROGRESS_WRITE, percent);
		PRINT_NUM(percent);
		PRINT(" %\n");

		if (br != sizeof(buffer))
		{
			break;
		}
	}

	flash_lock();

	if (processed != size)
	{
		PRINT_NUML("Different file size in second read: ", processed);
		return;
	}

	PRINT("Verifying...\n");
	res = pf_open("FIRMWARE.BIN");
	if (res != FR_OK)
	{
		status_update(STATUS_STORAGE_ERROR);
		PRINT_NUML("Failed to re-open firmware image, err=", res);
		return;
	}

	processed = 0;
	for (;;)
	{
		res = pf_read(buffer, sizeof(buffer), &br);
		if (res != FR_OK)
		{
			status_update(STATUS_STORAGE_ERROR);
			PRINT_NUML("Error while reading the firmware file, err=", res);
			return;
		}

		int n = memcmp(flash_ptr + processed, buffer, br);

		if (n != 0)
		{
			if (n < 0)
			{
				n = -n;
			}

			status_update(STATUS_VERIFICATION_ERROR);
			PRINT_NUML("VERIFICATION ERROR AT ", processed + n);
			return;
		}

		processed += br;

		int percent = (processed * 100) / size;
		status_progress(PROGRESS_VERIFY, percent);
		PRINT_NUM(percent);
		PRINT(" %\n");

		if (br != sizeof(buffer))
		{
			break;
		}
	}


	status_update(STATUS_FLASH_SUCCESS);
	PRINT("NEW IMAGE FLASHED\n");
}


static void jump_to_app(void)
{
	nvic_disable_irq(NVIC_TIM5_IRQ);
	cm_disable_interrupts();

	volatile uint32_t stackPtr = *(volatile uint32_t*)(APP_START);
	volatile uint32_t jumpAddress = *(volatile uint32_t*) (APP_START + 4);

	// Set the interrupt vector to the start of the real image
	SCB_VTOR = APP_START;


	// reset all core registers
	__asm volatile ("eor r0, r0\n"
					"mov r1, %0\n"
					"mov r2, %1\n"
					"msr psp, r0\n"
					"msr msp, r2\n"
					"msr ipsr, r0\n"
					"msr primask, r0\n"
					"msr faultmask, r0\n"
					"msr basepri, r0\n"
					"msr control, r0\n"
					"eor lr, lr\n"
					"sub lr, #1\n"
					"mov pc, r1\n"
					:
					: "r" (jumpAddress), "r" (stackPtr));

}


int main(void)
{
	rcc_clock_setup_hse_3v3(&CLOCK_SETTINGS);

	status_init();

	rcc_periph_clock_enable(USB_PWRSW_GPIO_RCC);
	gpio_mode_setup(USB_PWRSW_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, USB_PWRSW_GPIO_PIN);

	init_usart();

#ifdef LUSBH_USART_DEBUG
	init_printf(NULL, &tpf_putcf);
#endif

	PRINT("\n\nStarted USB bootloader\n");

	// Turn off USB power
	gpio_clear(USB_PWRSW_GPIO_PORT, USB_PWRSW_GPIO_PIN);

	usb_storage_init();

	PRINT("USB init complete\n");
	poll_timer_setup();

	delay();
	// Power up the USB port
	gpio_set(USB_PWRSW_GPIO_PORT, USB_PWRSW_GPIO_PIN);

	PRINT("Wait for device\n");

	mount_and_process();

	delay();

	status_update(STATUS_JUMP_TO_APPLICATION);
	PRINT("\nJumping into user programm\n\n\n");

	delay();

	jump_to_app();

	return 0;
}
