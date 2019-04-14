#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <stdlib.h>
#include <stdint.h>


#ifndef F_CPU
#error "F_CPU is not defined"
#endif


// At 11.0592 MHz, this is exactly 115200 baud
#define BAUDRATE_DIVIDER_L 5
#define BAUDRATE_DIVIDER_H 0

// 5 Min
#define WATCHDOG_TIMEOUT_SEC (300UL)

// 30 sec before the reset, we assert a 'prefail' signal
// This allows the host to send / log, that the reset was caused by the watchdog.
#define PREFAIL_THRESHOLD_SEC (270UL)

#define RESET_PULSE_LENGTH_MS (3000)


// 256 from the prescaler * 256 from the counter
#define WATCHDOG_DIVIDER (256UL * 256UL)

#define WATCHDOG_COUNTER_MAX ((WATCHDOG_TIMEOUT_SEC * F_CPU) / WATCHDOG_DIVIDER)
#define WATCHDOG_COUNTER_PREFAIL ((PREFAIL_THRESHOLD_SEC * F_CPU) / WATCHDOG_DIVIDER)

#define RESET_OUTPUT_PORT PORTB
#define RESET_OUTPUT_DDR DDRB
#define RESET_OUTPUT_PIN PB0

#define PREFAIL_OUTPUT_PORT PORTD
#define PREFAIL_OUTPUT_DDR DDRD
#define PREFAIL_OUTPUT_PIN PD6

volatile uint32_t watchdog_counter;


void init_uart()
{
	// RX only, 8N1
	UBRRH = BAUDRATE_DIVIDER_H;
	UBRRL = BAUDRATE_DIVIDER_L;
	UCSRB = (1 << RXEN) | (1 << TXEN);
	UCSRC = (1 << UCSZ1) | (1 << UCSZ0);
}


void init_timer()
{
	TCCR0A = 0;
	TCCR0B = (1 << CS02); // Prescaler 256
	TIMSK = (1 << TOIE0);
}


// Reset the host device
// NOTE: The AVRs watchdog needs to be reset while this is running!
void send_reset_pulse()
{
	RESET_OUTPUT_DDR |= (1 << RESET_OUTPUT_PIN);
	RESET_OUTPUT_PORT |= (1 << RESET_OUTPUT_PIN);
	_delay_ms(RESET_PULSE_LENGTH_MS);
	RESET_OUTPUT_PORT &= ~(1 << RESET_OUTPUT_PIN);
}


void putch(char c)
{
	while (!(UCSRA & (1 << UDRE)));
    UDR = c;
}


void putstr(const char *s)
{
	while (*s)
	{
		putch(*s);
		s++;
	}
}


void wait_for_food()
{
	static const uint8_t *FEED_STRING = "wdt_feed";

	const uint8_t *cmp_ptr = FEED_STRING;

	while (*cmp_ptr)
	{
		while (!(UCSRA & (1 << RXC)));
		uint8_t ch = UDR;

		if (ch == *cmp_ptr)
		{
			// OK -> next char
			cmp_ptr++;
		}
		else
		{
			// Wrong -> reset
			cmp_ptr = FEED_STRING;
		}
	}
}


int main (void)
{
	wdt_reset();
	watchdog_counter = 0;
	init_uart();
	init_timer();
	sei();
	wdt_reset();
	putstr("Watchdog started -- sending reset pulse\n");

	PREFAIL_OUTPUT_DDR |= (1 << PREFAIL_OUTPUT_PIN);

	// Begin with a reset pulse
	send_reset_pulse();

	while (1)
	{
		wait_for_food();
		putstr("OK\n");
		watchdog_counter = 0;
	}
}



ISR (TIMER0_OVF_vect)
{
	if (watchdog_counter > WATCHDOG_COUNTER_MAX)
	{
		putch('!');
	}
	else
	{
		wdt_reset();
		watchdog_counter++;
	}

	if (watchdog_counter > WATCHDOG_COUNTER_PREFAIL)
	{
		PREFAIL_OUTPUT_PORT |= (1 << PREFAIL_OUTPUT_PIN);
	}
	else
	{
		PREFAIL_OUTPUT_PORT &= ~(1 << PREFAIL_OUTPUT_PIN);
	}
}
