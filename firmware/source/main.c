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


static void tpf_putcf(void *ptr, char c)
{
	(void)ptr;
	usart_send_blocking(USART1, c);
}


static void init_usart(void)
{
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_USART1);

	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9 | GPIO10);

	gpio_set_af(GPIOA, GPIO_AF7, GPIO9 | GPIO10);

	usart_set_baudrate(USART1, 115200);
	usart_set_mode(USART1, USART_MODE_TX | USART_MODE_RX);
	usart_enable(USART1);
}

int main(void)
{
	cm_disable_interrupts();
	rcc_clock_setup_hse_3v3(&CLOCK_SETTINGS);

	init_usart();
	init_printf(NULL, &tpf_putcf);


	printf("Hello world");
	//init();

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
