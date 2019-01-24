/*
 * Copyright 2019 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#include "host_watchdog.h"

#include <FreeRTOS.h>
#include <task.h>
#include <stdint.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include "tinyprintf.h"

#include "watchdog.h"

// Timeout in seconds for the host watchdog
static const uint32_t HOST_WATCHDOG_LIMIT_SEC = 600;

// Send a warning, if the watchdog has not been fed for the specified number of seconds
static const uint32_t HOST_WATCHDOG_WARN_SEC =  550;

// This GPIO to which the reset line of the host controller is connected.
// On timeout, this pin will go high for a second.
#define HOST_WATCHDOG_RESET_GPIO_RCC    RCC_GPIOB
#define HOST_WATCHDOG_RESET_GPIO_PORT   GPIOB
#define HOST_WATCHDOG_RESET_GPIO_PIN    GPIO13

// Delay in ticks for in the watchdog thread.
// This can be rather high, but this thread is guarded by the hardware watchdog therefore it must be below 4 sec.
static const uint32_t HOST_WATCHDOG_TICKDELAY = 3000;

static const uint32_t HOST_WATCHDOG_THRESHOLD = (HOST_WATCHDOG_LIMIT_SEC * 1000) / HOST_WATCHDOG_TICKDELAY;
static const uint32_t HOST_WATCHDOG_WARN_THRESHOLD = (HOST_WATCHDOG_WARN_SEC * 1000) / HOST_WATCHDOG_TICKDELAY;

// The counter for the host watchdog
static volatile uint32_t host_watchdog_counter;


// Stack size of the watchdog thread (in words)
#define HWD_THD_STACK_SIZE configMINIMAL_STACK_SIZE

static StaticTask_t hwd_thd_buffer;;
static StackType_t hwd_thd_stack[HWD_THD_STACK_SIZE];

/*
 * This is the host watchdog thread.
 * The host watchdog needs to be regulary fed by the host through the 'feed_dog' command.
 * If it is not fed, it will emit a pulse on a GPIO pin which should be connected to the reset line of the
 * host controller.
 */
static void host_watchdog_thread(void *arg)
{
	(void) arg;
    TickType_t last = xTaskGetTickCount();

	host_watchdog_counter = 0;

	rcc_periph_clock_enable(HOST_WATCHDOG_RESET_GPIO_RCC);

	gpio_set(HOST_WATCHDOG_RESET_GPIO_PORT, HOST_WATCHDOG_RESET_GPIO_PIN);

#if defined(WASCHV1)
	gpio_set_mode(HOST_WATCHDOG_RESET_GPIO_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, HOST_WATCHDOG_RESET_GPIO_PIN);
#else
	gpio_mode_setup(HOST_WATCHDOG_RESET_GPIO_PORT,
					GPIO_MODE_OUTPUT,
					GPIO_PUPD_NONE,
					HOST_WATCHDOG_RESET_GPIO_PIN);
#endif

	while(1)
	{
		WATCHDOG_FEED();
		vTaskDelayUntil(&last, HOST_WATCHDOG_TICKDELAY);

		host_watchdog_counter++;

		if (host_watchdog_counter > HOST_WATCHDOG_THRESHOLD)
		{
			printf("\nHOST WATCHGOD TRIGGERED, RESETTING HOST NOW!\n");
			gpio_clear(HOST_WATCHDOG_RESET_GPIO_PORT, HOST_WATCHDOG_RESET_GPIO_PIN);
			vTaskDelay(1000);
			gpio_set(HOST_WATCHDOG_RESET_GPIO_PORT, HOST_WATCHDOG_RESET_GPIO_PIN);
			host_watchdog_counter = 0;
		}
		else if (host_watchdog_counter > HOST_WATCHDOG_WARN_THRESHOLD)
		{
			printf("\nWARNING: HOST WATCHDOG ABOUT TO TRIGGER\n");
		}
	}
}


void host_watchdog_init(void)
{
	xTaskCreateStatic(
		&host_watchdog_thread,
		"HWD",
	    HWD_THD_STACK_SIZE,
		NULL,
		tskIDLE_PRIORITY,
		hwd_thd_stack,
		&hwd_thd_buffer);
}


void host_watchdog_feed_cmd(int argc, char **argv)
{
    (void)argc;
	(void)argv;
	printf("Dog is fed, old counter value was: %lu\n", host_watchdog_counter);
	host_watchdog_counter = 0;
}
