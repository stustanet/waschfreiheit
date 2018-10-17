/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#include "serial_getchar_dma.h"

#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/rcc.h>


#if defined(STM32F1)
// FIXME
#define SERIAL_GETCHAR_DMA DMA1
#define SERIAL_GETCHAR_DMA_RCC RCC_DMA1
#define SERIAL_GETCHAR_DMA_CHANNEL DMA1_Channel5
#elif defined(STM32F4)
#define SERIAL_GETCHAR_DMA DMA2
#define SERIAL_GETCHAR_DMA_RCC RCC_DMA2
#define SERIAL_GETCHAR_DMA_STREAM 2
#define SERIAL_GETCHAR_DMA_CHANNEL DMA_SxCR_CHSEL_4
#else
#error "Unknown CPU"
#endif

#define SERIAL_GETCHAR_USART USART1
#define SERIAL_GETCHAR_DMA_BUFFERSIZE 32

static uint8_t dma_buffer[SERIAL_GETCHAR_DMA_BUFFERSIZE];
static uint16_t last_read_index;


void serial_getchar_dma_init(void)
{
	// Enable DMA controller
	rcc_periph_clock_enable(SERIAL_GETCHAR_DMA_RCC);

	last_read_index = 0;

	// Init the DMA for the USART1
#if defined(STM32F1)
#error TODO
#else
	dma_stream_reset(SERIAL_GETCHAR_DMA, SERIAL_GETCHAR_DMA_STREAM);
	dma_enable_memory_increment_mode(SERIAL_GETCHAR_DMA, SERIAL_GETCHAR_DMA_STREAM);
	dma_set_transfer_mode(SERIAL_GETCHAR_DMA, SERIAL_GETCHAR_DMA_STREAM, DMA_SxCR_DIR_PERIPHERAL_TO_MEM);
	dma_set_peripheral_size(SERIAL_GETCHAR_DMA, SERIAL_GETCHAR_DMA_STREAM, DMA_SxCR_PSIZE_8BIT);
	dma_set_memory_size(SERIAL_GETCHAR_DMA, SERIAL_GETCHAR_DMA_STREAM, DMA_SxCR_MSIZE_8BIT);

	dma_channel_select(SERIAL_GETCHAR_DMA, SERIAL_GETCHAR_DMA_STREAM, SERIAL_GETCHAR_DMA_CHANNEL);

	dma_set_number_of_data(SERIAL_GETCHAR_DMA, SERIAL_GETCHAR_DMA_STREAM, sizeof(dma_buffer));

	dma_set_peripheral_address(SERIAL_GETCHAR_DMA, SERIAL_GETCHAR_DMA_STREAM, (uint32_t)&USART_DR(SERIAL_GETCHAR_USART));
	dma_set_memory_address(SERIAL_GETCHAR_DMA, SERIAL_GETCHAR_DMA_STREAM, (uint32_t)dma_buffer);

	dma_enable_circular_mode(SERIAL_GETCHAR_DMA, SERIAL_GETCHAR_DMA_STREAM);
	dma_enable_stream(SERIAL_GETCHAR_DMA, SERIAL_GETCHAR_DMA_STREAM);
#endif

	// Enable RX DMA for USART
	usart_enable_rx_dma(SERIAL_GETCHAR_USART);
}


int16_t serial_getchar(void)
{
	uint16_t current = sizeof(dma_buffer) - (uint16_t)(dma_get_number_of_data(SERIAL_GETCHAR_DMA, SERIAL_GETCHAR_DMA_STREAM));
	if (current != last_read_index)
	{
		uint8_t r = dma_buffer[last_read_index];
		last_read_index++;
		if (last_read_index >= sizeof(dma_buffer))
		{
			last_read_index = 0;
		}
		return r;
	}
	return INT16_MIN;
}
