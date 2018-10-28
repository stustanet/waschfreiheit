/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#include "sensor_adc.h"

#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/dma.h>

#include <FreeRTOS.h>
#include <task.h>

void sensor_adc_init_dma(uint16_t dest[NUM_OF_WASCH_CHANNELS])
{
	rcc_periph_clock_enable(ADC_GPIO_RCC);
	rcc_periph_clock_enable(RCC_ADC1);
	rcc_periph_clock_enable(RCC_DMA2);

#ifdef WASCHV2
	gpio_mode_setup(ADC_GPIO_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, ADC_CHANNEL_GPIO_LIST);
#else
	gpio_set_mode(ADC_GPIO_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_ANALOG, ADC_CHANNEL_GPIO_LIST);
#endif

	adc_power_on(ADC1);
	vTaskDelay(10);

	adc_enable_scan_mode(ADC1);
	adc_set_continuous_conversion_mode(ADC1);
	adc_disable_external_trigger_regular(ADC1);
	adc_set_right_aligned(ADC1);
#ifdef WASCHV2
	adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_480CYC);
#else
	adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_239DOT5CYC);
#endif


	uint8_t channel_array[NUM_OF_WASCH_CHANNELS] = ADC_CHANNEL_MUX_LIST;
	adc_set_regular_sequence(ADC1, NUM_OF_WASCH_CHANNELS, channel_array);

	// Set-up the DMA
#ifdef WASCHV2

	dma_stream_reset(DMA2, 0);
	dma_enable_memory_increment_mode(DMA2, 0);
	dma_set_transfer_mode(DMA2, 0, DMA_SxCR_DIR_PERIPHERAL_TO_MEM);
	dma_set_peripheral_size(DMA2, 0, DMA_SxCR_PSIZE_16BIT);
	dma_set_memory_size(DMA2, 0, DMA_SxCR_MSIZE_16BIT);

	dma_channel_select(DMA2, 0, DMA_SxCR_CHSEL_0);

	dma_set_number_of_data(DMA2, 0, NUM_OF_WASCH_CHANNELS * sizeof(dest[0]));

	dma_set_peripheral_address(DMA2, 0, (uint32_t)&ADC_DR(ADC1));
	dma_set_memory_address(DMA2, 0, (uint32_t)dest);

	dma_enable_circular_mode(DMA2, 0);
	dma_enable_stream(DMA2, 0);

	adc_set_dma_continue(ADC1);
#else

	dma_channel_reset(DMA1, DMA_CHANNEL1);
	dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL1);
	dma_set_read_from_peripheral(DMA1, DMA_CHANNEL1);
	dma_set_peripheral_size(DMA1, DMA_CHANNEL1, DMA_CCR_PSIZE_16BIT);
	dma_set_memory_size(DMA1, DMA_CHANNEL1, DMA_CCR_MSIZE_16BIT);

	dma_set_number_of_data(DMA1, DMA_CHANNEL1, NUM_OF_WASCH_CHANNELS * sizeof(dest[0]));

	dma_set_peripheral_address(DMA1, DMA_CHANNEL1, (uint32_t)&ADC_DR(ADC1));
	dma_set_memory_address(DMA1, DMA_CHANNEL1, (uint32_t)dest);

	dma_enable_circular_mode(DMA1, DMA_CHANNEL1);
	dma_enable_channel(DMA1, DMA_CHANNEL1);

#endif

	adc_enable_dma(ADC1);
	adc_start_conversion_regular(ADC1);
}
