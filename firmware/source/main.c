/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/cm3/cortex.h>

#include <FreeRTOS.h>
#include <task.h>

#include "tinyprintf.h"
#include "serial_getchar_dma.h"
#include "cli.h"
#include "commands_common.h"
#include "meshnw.h"
#include "isrsafe_printf.h"

#if !defined(MASTER) && defined(WASCHV2)
#include "debug_command_queue.h"
#endif

#ifdef WASCHV2
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
#endif

// Stack size in words for the CLI task
#define CLI_TASK_STACK_SIZE 512

// Number of bytes in the command buffer for the CLI
// This resides on the stack of the CLI task so keep the stack size in mind when changing this!
#define CLI_CMD_BUFFER_LENGTH 200

#define USART_CLI USART1
#define USART_CLI_RCC RCC_USART1

StaticTask_t cliTaskBuffer;
StackType_t cliTaskStack[CLI_TASK_STACK_SIZE];

void node_init(void);
extern cli_command_t cli_commands [];

static void tpf_putcf(void *ptr, char c)
{
	(void)ptr;
	usart_send_blocking(USART_CLI, c);
}


static void init_usart(void)
{
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(USART_CLI_RCC);

#ifdef WASCHV2
	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9 | GPIO10);
	gpio_set_af(GPIOA, GPIO_AF7, GPIO9 | GPIO10);
#else
	gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_USART1_TX);

	// On the V1 borads, the status LED of the bluepill board is connected to this pin
	// => Switch on this LED
	rcc_periph_clock_enable(RCC_GPIOC);
	gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
	gpio_clear(GPIOC, GPIO13);
#endif

	usart_set_baudrate(USART_CLI, 115200);
	usart_set_mode(USART_CLI, USART_MODE_TX | USART_MODE_RX);
	usart_enable(USART_CLI);
}


static void cliTask(void *arg)
{
	(void) arg;

	char buffer[CLI_CMD_BUFFER_LENGTH];
	size_t buffer_pos = 0;

	cli_set_commandlist(cli_commands);

	while (1)
	{

#if !defined(MASTER) && defined(WASCHV2)
		if (debug_command_queue_running())
		{
			char *next = debug_command_queue_next();

			if (next)
			{
				cli_evaluate(next);
				continue;
			}
		}
#endif
		int16_t c = serial_getchar();

		if (c == INT16_MIN)
		{
			taskYIELD();
			continue;
		}

		if (c == '\r' || c == '\n' || buffer_pos >= (CLI_CMD_BUFFER_LENGTH - 1))
		{
			if (buffer_pos)
			{
				buffer[buffer_pos] = 0;
				cli_evaluate(buffer);
				buffer_pos = 0;
#ifdef CLI_PROMPT
				ISRSAFE_PRINTF(CLI_PROMPT);
#endif
			}
		}
		else
		{
			buffer[buffer_pos] = c;
			buffer_pos++;
		}
	}
}


int main(void)
{
	cm_disable_interrupts();
#ifdef WASCHV2
	rcc_clock_setup_hse_3v3(&CLOCK_SETTINGS);
#else
	rcc_clock_setup_in_hse_8mhz_out_72mhz();
#endif

	init_usart();
	serial_getchar_dma_init();
	init_printf(NULL, &tpf_putcf);

	printf("Wasch node starting...\n");
	node_init();

	xTaskCreateStatic(
		&cliTask,
		"CLI",
		CLI_TASK_STACK_SIZE,
		NULL,
		tskIDLE_PRIORITY,
		cliTaskStack,
		&cliTaskBuffer);


	vTaskStartScheduler();

	return 0;
}


// From https://www.freertos.org/a00110.html
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{
	static StaticTask_t xIdleTaskTCB;
	static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
