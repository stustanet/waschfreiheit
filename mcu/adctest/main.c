#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/cm3/nvic.h>
#include <string.h>

#include "delay.h"
#include "usbacm.h"
#include "util.h"


#define ENABLE_DEBUG_USART


static void print_raw(const void *data, uint32_t len);
static void print_str(const char *str);
static void print_hex(const uint8_t *data, uint32_t size);

static void adc_init(void);

#ifdef ENABLE_DEBUG_USART
static void debug_usart_init(void);
#endif


int main(void)
{
	rcc_clock_setup_in_hse_8mhz_out_72mhz();

	// need to manually set to usb prescaler to 1.5 (USB needs 48Mhz)
	rcc_set_usbpre(RCC_CFGR_USBPRE_PLL_CLK_DIV1_5);

	delay_init();

	usbacm_init();

	rcc_periph_clock_enable(RCC_GPIOC);

	gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);


	uint32_t mid = 0x0800;
	uint32_t avg_delta = 0;

#ifdef ENABLE_DEBUG_USART
	debug_usart_init();
#endif

	print_str("INIT\n");
	adc_init();

	while(1)
	{
		adc_start_conversion_direct(ADC1);
		while (!(ADC_SR(ADC1) & ADC_SR_EOC));
		uint16_t res = ADC_DR(ADC1);
/*
		uint8_t d[2]  = {res >> 8, res};

		print_str("v = ");
		print_hex(d, 2);
		print_str("\n");
*/

		uint32_t val = res * 20;
		if (val > mid)
		{
			mid++;
		}
		else
		{
			mid--;
		}

		int32_t delta = val - mid;

		if (delta < 0)
		{
			delta = -delta;
		}

		avg_delta = ((avg_delta * 99) + delta) / 100;

		uint8_t ad[4]  = {avg_delta >> 24, avg_delta >> 16, avg_delta >> 8, avg_delta};

		print_hex(ad, 4);
		print_str("\n");

		if (avg_delta > 200)
		{
			gpio_clear(GPIOC, GPIO13);
		}
		else
		{
			gpio_set(GPIOC, GPIO13);
		}
		
	}




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


// callback from virtual com port
void usbacm_recv_handler(void *data, uint32_t len)
{
	(void)data;
	(void)len;
	return;
}

void adc_init()
{
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_ADC1);

	gpio_set_mode(GPIOB, GPIO_MODE_INPUT, GPIO_CNF_INPUT_ANALOG, GPIO0);

	adc_power_off(ADC1);
	adc_disable_scan_mode(ADC1);
	adc_set_single_conversion_mode(ADC1);
	adc_disable_external_trigger_regular(ADC1);
	adc_set_right_aligned(ADC1);
	adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_239DOT5CYC);

	adc_power_on(ADC1);

	/* Wait for ADC starting up. */
	delay_ticks(1000);

	adc_reset_calibration(ADC1);
	adc_calibrate(ADC1);

	uint8_t channel_array[16];
	channel_array[0] = 8;
	adc_set_regular_sequence(ADC1, 1, channel_array);


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
