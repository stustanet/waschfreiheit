#pragma once

#include <stdint.h>

void delay_init(void);

/*
 * The delay and the tickcount function should MUST NOT be called in an interrupt
 * with a priority higher than the systick interrupts prio
 * */
void delay_ticks(uint32_t ticks);
uint32_t tickcount(void);

