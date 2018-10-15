/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


/*
 * This is a minimal getchar() implementation using DMA
 * This only works on STM32F1
 */

#include <periph/dev_enums.h>
#include <periph/uart.h>
#include "utils.h"
#include "board.h"

#define GETCHAR_USART USART1
#define GETCHAR_USART_DMA_CHANNEL DMA1_Channel5

#define GETCHAR_DMA_BUFFERSIZE 32

static uint8_t dma_buffer[GETCHAR_DMA_BUFFERSIZE];
static uint16_t last_read_index;


void serial_getchar_dma_init(void)
{
	if (uart_config[UART_STDIO_DEV].dev != GETCHAR_USART)
	{
		puts("STDIO USART is not the same as dma getchar");
		return;
	}
		
	// Enable DMA controller
	RCC->AHBENR |= RCC_AHBENR_DMA1EN;

	// Init the DMA for the USART1
	GETCHAR_USART_DMA_CHANNEL->CPAR = (uint32_t)&(GETCHAR_USART->DR);
	GETCHAR_USART_DMA_CHANNEL->CMAR = (uint32_t)dma_buffer;
	GETCHAR_USART_DMA_CHANNEL->CNDTR = sizeof(dma_buffer);

	last_read_index = 0;

	// No Mem2Mem, Low Prio, 8bit mem, 8bit peri, Memory Increment, circular, read from periph, no interrupts, enable channel
	GETCHAR_USART_DMA_CHANNEL->CCR = DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_EN;

	// Enable RX DMA for USART
	GETCHAR_USART->CR3 |= USART_CR3_DMAR;
}


int getchar(void)
{
	for (;;)
	{
		uint16_t current = sizeof(dma_buffer) - (uint16_t)(GETCHAR_USART_DMA_CHANNEL->CNDTR);
		if (current != last_read_index)
		{
			int r = dma_buffer[last_read_index];
			last_read_index++;
			if (last_read_index >= sizeof(dma_buffer))
			{
				last_read_index = 0;
			}
			return r;
		}
	}
	return -1;
}
