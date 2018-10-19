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
#include <libopencm3/cm3/cortex.h>

#include <FreeRTOS.h>
#include <task.h>

#include "tinyprintf.h"
#include "serial_getchar_dma.h"
#include "cli.h"
#include "sx127x.h"

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

// Stack size in words for the CLI task
#define CLI_TASK_STACK_SIZE 200

// Number of bytes in the command buffer for the CLI
// This resides on the stack of the CLI task so keep the stack size in mind when changing this!
#define CLI_CMD_BUFFER_LENGTH 200

#define USART_CLI USART1
#define USART_CLI_RCC RCC_USART1

StaticTask_t cliTaskBuffer;
StackType_t cliTaskStack[CLI_TASK_STACK_SIZE];

static void tpf_putcf(void *ptr, char c)
{
	(void)ptr;
	usart_send_blocking(USART_CLI, c);
}


static void init_usart(void)
{
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(USART_CLI_RCC);

	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9 | GPIO10);

	gpio_set_af(GPIOA, GPIO_AF7, GPIO9 | GPIO10);

	usart_set_baudrate(USART_CLI, 115200);
	usart_set_mode(USART_CLI, USART_MODE_TX | USART_MODE_RX);
	usart_enable(USART_CLI);
}


static void test_func(int argc, char **argv)
{
	printf("Called test command with the following %i arguments:\n", argc);
	for (int i = 0; i < argc; i++)
	{
		printf("%s\n", argv[i]);
	}
	printf("---------\n");
}


static cli_command_t cli_commands [] =
{
	{"test", "test command", test_func},
	{"sx127x", "RF module test command", sx127x_test_cmd},
	{}
};


static void cliTask(void *arg)
{
	(void) arg;

	char buffer[CLI_CMD_BUFFER_LENGTH];
	size_t buffer_pos = 0;

	while (1)
	{
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
	rcc_clock_setup_hse_3v3(&CLOCK_SETTINGS);

	init_usart();
	serial_getchar_dma_init();
	init_printf(NULL, &tpf_putcf);

	const sx127x_rf_config_t lora_cfg = {
		.frequency = 433500000,
		.tx_power = 10,
		.lora_spread_factor = 10,
		.lora_coderate = 2,
		.lora_bandwidth = 7
	};

	sx127x_init(&lora_cfg);

	cli_set_commandlist(cli_commands);


	printf("Hello world");
	//init();

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
