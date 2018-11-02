/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#include "frequency_sensor.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/gpio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

typedef struct
{
	uint32_t timer;
	enum tim_ic_id channel;
	enum rcc_periph_clken timer_rcc;
	enum rcc_periph_rst timer_rst;
	enum rcc_periph_clken gpio_rcc;
	uint32_t gpio_port;
	uint16_t gpio_pin;
	uint8_t gpio_af;
} frequency_sensor_channel_t;


typedef struct
{
	// Buffer for the old samples
	// A bit is 1, if the sample was negative
	uint64_t samples;
	uint16_t threshold;
	uint16_t last_frame;
	uint8_t num_samples;
	uint8_t negative_theshold;
	uint8_t negative_counter;
} frequency_sensor_context_t;

static frequency_sensor_context_t channel_ctx[FREQUENCY_SENSOR_MAX_SAMPLE_COUNT];
static const frequency_sensor_channel_t channel_def[FREQUENCY_SENSOR_NUM_OF_CHANNELS] =
{
	{
		.timer = TIM3,
		.channel =  TIM_IC2,
		.timer_rcc = RCC_TIM3,
		.timer_rst = RST_TIM3,
		.gpio_rcc = RCC_GPIOB,
		.gpio_port = GPIOB,
		.gpio_pin = GPIO5,
		.gpio_af = GPIO_AF2
	}
};

static bool initialized = false;
static volatile bool has_new_frame = false;

// Stack size of the sensor thread
#define FS_THD_STACK_SIZE configMINIMAL_STACK_SIZE

static StaticTask_t fs_thd_buffer;
static StackType_t fs_thd_stack[FS_THD_STACK_SIZE];

static SemaphoreHandle_t mutex;
static StaticSemaphore_t mutexBuffer;


static void init_timer(const frequency_sensor_channel_t *t)
{
	// Setup the GPIO
	rcc_periph_clock_enable(t->gpio_rcc);

	gpio_mode_setup(t->gpio_port,
					GPIO_MODE_AF,
					GPIO_PUPD_PULLUP,
					t->gpio_pin);

	gpio_set_af(t->gpio_port,
				t->gpio_af,
				t->gpio_pin);

	// Setup the timer
	rcc_periph_clock_enable(t->timer_rcc);
	rcc_periph_reset_pulse(t->timer_rst);

	timer_ic_set_input(t->timer, t->channel, TIM_IC_IN_TI2);

	// Slowest possible filter
	timer_ic_set_filter(t->timer, t->channel, TIM_IC_DTF_DIV_32_N_8);

	timer_ic_set_polarity(t->timer, t->channel, TIM_IC_RISING);

	timer_slave_set_mode(t->timer, TIM_SMCR_SMS_ECM1);

	timer_slave_set_trigger(t->timer, TIM_SMCR_TS_TI2FP2);
}


static void reset_timer(const frequency_sensor_channel_t *t)
{
	timer_set_counter(t->timer, 0);
	timer_clear_flag(t->timer, TIM_SR_UIF);
	timer_enable_counter(t->timer);
}

static uint16_t get_timer(const frequency_sensor_channel_t *t)
{
	timer_disable_counter(t->timer);
	if (timer_get_flag(t->timer, TIM_SR_UIF))
	{
		return UINT16_MAX;
	}
	return timer_get_counter(t->timer);
}

static void frequency_sensor_thread(void *arg)
{
	(void) arg;
    TickType_t last = xTaskGetTickCount();

	while(1)
	{
		xSemaphoreTake(mutex, portMAX_DELAY);
		for (size_t i = 0; i < FREQUENCY_SENSOR_NUM_OF_CHANNELS; i++)
		{
			if (channel_ctx[i].num_samples == 0)
			{
				continue;
			}

			_Static_assert(FREQUENCY_SENSOR_MAX_SAMPLE_COUNT <= 64, "FREQUENCY_SENSOR_MAX_SAMPLE_COUNT must be <= 64");

			bool wasNeg = channel_ctx[i].negative_counter >= channel_ctx[i].negative_theshold;

			if (channel_ctx[i].samples & (1 << (channel_ctx[i].num_samples - 1)))
			{
				// About to shift out a '1' => decrement the counter
				channel_ctx[i].negative_counter--;
			}

			// Shift left by one bit
			channel_ctx[i].samples <<= 1;

			uint16_t current = get_timer(&channel_def[i]);
			channel_ctx[i].last_frame = current;
			if (current < channel_ctx[1].threshold)
			{
				// Negative => Set lsb and inc counter
				channel_ctx[i].samples |= 0x1;
				channel_ctx[i].negative_counter++;
			}

			bool isNeg = channel_ctx[i].negative_counter >= channel_ctx[i].negative_theshold;

			if (isNeg != wasNeg)
			{
				// state changed -> (re-)set all bits to avoid flapping
				if (isNeg)
				{
					// Set all bits: Bits above num_samples are ignored anyway
					channel_ctx[i].samples = UINT64_MAX;

					// Set neg counter to max
					channel_ctx[i].negative_counter = channel_ctx[i].num_samples;
				}
				else
				{
					// Clear all bits
					channel_ctx[i].samples = 0;
					// Set neg counter to 0
					channel_ctx[i].negative_counter = 0;
				}
			}

			reset_timer(&channel_def[i]);
		}

		has_new_frame = true;

		xSemaphoreGive(mutex);

		vTaskDelayUntil(&last, FREQUENCY_SENSOR_SAMPLE_TIME);
	}
}

static void initialize()
{
	if (initialized)
	{
		return;
	}

	for (size_t i = 0; i < FREQUENCY_SENSOR_NUM_OF_CHANNELS; i++)
	{
		channel_ctx[i].num_samples = 0;
		init_timer(&channel_def[i]);
	}

	mutex = xSemaphoreCreateMutexStatic(&mutexBuffer);

	xTaskCreateStatic(
		&frequency_sensor_thread,
		"FREQ_SENS",
	    FS_THD_STACK_SIZE,
		NULL,
		tskIDLE_PRIORITY + 4,
		fs_thd_stack,
		&fs_thd_buffer);

	initialized = true;
}

bool frequency_sensor_init(uint8_t channel, uint16_t threshold, uint8_t sample_count, uint8_t negative_sample_threshold)
{
	initialize();

	if (channel >= FREQUENCY_SENSOR_NUM_OF_CHANNELS)
	{
		return false;
	}

	if (sample_count > FREQUENCY_SENSOR_MAX_SAMPLE_COUNT)
	{
		return false;
	}

	if (negative_sample_threshold > sample_count)
	{
		return false;
	}

	xSemaphoreTake(mutex, portMAX_DELAY);
	channel_ctx[channel].threshold = threshold;
	channel_ctx[channel].num_samples = sample_count;
	channel_ctx[channel].negative_theshold = negative_sample_threshold;
	channel_ctx[channel].samples = 0;
	channel_ctx[channel].negative_counter = 0;
	xSemaphoreGive(mutex);
	return true;
}


bool frequency_sensor_get_status(uint8_t channel)
{
	if (!initialized)
	{
		return false;
	}
	if (channel >= FREQUENCY_SENSOR_NUM_OF_CHANNELS)
	{
		return false;
	}

	xSemaphoreTake(mutex, portMAX_DELAY);
	bool ret = channel_ctx[channel].negative_counter < channel_ctx[channel].negative_theshold;
	xSemaphoreGive(mutex);

	return ret;
}


uint8_t frequency_sensor_get_negative_counter(uint8_t channel)
{
	if (!initialized)
	{
		return false;
	}

	if (channel >= FREQUENCY_SENSOR_NUM_OF_CHANNELS)
	{
		return false;
	}

	xSemaphoreTake(mutex, portMAX_DELAY);
	uint8_t ret = channel_ctx[channel].negative_counter;
	xSemaphoreGive(mutex);

	return ret;
}

uint16_t frequency_sensor_get_last_counter(uint8_t channel)
{
	if (channel >= FREQUENCY_SENSOR_NUM_OF_CHANNELS)
	{
		return 0;
	}

	return channel_ctx[channel].last_frame;
}

bool frequency_sensor_had_new_sample(void)
{
	if (has_new_frame)
	{
		has_new_frame = false;
		return true;
	}

	return false;
}
