#include "delay.h"

#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/cm3/nvic.h>

static volatile uint32_t tickcnt;

void delay_init(void)
{
	// Init systick freq
	
	tickcnt = 0;
	systick_set_frequency(1000, rcc_ahb_frequency);
	systick_counter_enable();
	systick_interrupt_enable();
}


void delay_ticks(volatile uint32_t ticks)
{
	volatile uint32_t start = tickcnt;

	while (tickcnt - start < ticks)
	{
		__asm__("nop");
	}
}


uint32_t tickcount(void)
{
	return tickcnt;
}


// Systick interrupt impl
void sys_tick_handler(void)
{
	tickcnt++;
}
