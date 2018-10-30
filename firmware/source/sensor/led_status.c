#include "led_status.h"

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#include <string.h>

#include <FreeRTOS.h>
#include <semphr.h>

#ifdef WASCHV1

#include "led_ws2801.h"

#define WS2801_GPIO_PORT GPIOB
#define WS2801_GPIO_PORT_RCC RCC_GPIOB
#define WS2801_GPIO_DATA GPIO10
#define WS2801_GPIO_CLK GPIO11

// WS2801: Normal RGB order
#define LED_COLOR_STRUCT rgb_data_t

#else

// WS2812: GRB color order
typedef struct _grb_data
{
	uint8_t g;
	uint8_t r;
	uint8_t b;
} grb_data_t;

#define LED_COLOR_STRUCT grb_data_t
#include "i2s_rgb.h"
#endif

#define COPY_CONVERT_RGB(dst, src) dst.r = (src.r >> 5); dst.g = (src.g >> 5); dst.b = (src.b >> 5)

#if NUM_SYSTEM_LEDS != 0
static struct
{
	uint8_t led;
	rgb_data_t color;
} status_color_table[LED_STATUS_COUNT] =
{
	{.led = 0, .color = {.r = 0x00, .g = 0x00, .b = 0xff}}, // LED_STATUS_SYSTEM_INIT
	{.led = 0, .color = {.r = 0xff, .g = 0x00, .b = 0x00}}, // LED_STATUS_SYSTEM_ERROR
	{.led = 0, .color = {.r = 0xff, .g = 0x00, .b = 0xff}}, // LED_STATUS_SYSTEM_CONNECTED
	{.led = 0, .color = {.r = 0xff, .g = 0xff, .b = 0x00}}, // LED_STATUS_SYSTEM_SENSORS
	{.led = 0, .color = {.r = 0x00, .g = 0xff, .b = 0x00}}, // LED_STATUS_SYSTEM_SCH_BUILT
	{.led = 0, .color = {.r = 0x00, .g = 0xff, .b = 0xff}}, // LED_STATUS_SYSTEM_SCHNG_PEND
	{.led = 1, .color = {.r = 0xff, .g = 0xff, .b = 0x00}}, // LED_STATUS_SYSTEM_USB_CON
	{.led = 1, .color = {.r = 0x00, .g = 0xff, .b = 0xff}}, // LED_STATUS_SYSTEM_USB_OK
	{.led = 1, .color = {.r = 0x00, .g = 0xff, .b = 0x00}}, // LED_STATUS_SYSTEM_USB_MOUNT_OK
	{.led = 1, .color = {.r = 0xff, .g = 0x00, .b = 0x80}}, // LED_STATUS_SYSTEM_USB_MOUNT_ERR
	{.led = 1, .color = {.r = 0xff, .g = 0x00, .b = 0x00}}, // LED_STATUS_SYSTEM_USB_ERR
	{.led = 1, .color = {.r = 0x00, .g = 0x00, .b = 0x00}}, // LED_STATUS_SYSTEM_USB_DISCON
	{.led = 1, .color = {.r = 0xff, .g = 0xff, .b = 0xff}}, // LED_STATUS_SYSTEM_USB_AUTOEXEC
	{.led = 0, .color = {.r = 0xff, .g = 0xff, .b = 0xff}}, // LED_STATUS_SYSTEM_RX
	{.led = 0, .color = {.r = 0xff, .g = 0x00, .b = 0x00}}, // LED_STATUS_SYSTEM_TX
	{.led = 1, .color = {.r = 0x00, .g = 0x00, .b = 0xff}}  // LED_STATUS_SYSTEM_USB_RW
};
#endif


#if NUM_CHANNEL_LEDS != 0
static const rgb_data_t CHANNEL_LED_ON      = {.r = 0xff, .g = 0x00, .b = 0x00};
static const rgb_data_t CHANNEL_LED_OFF     = {.r = 0x00, .g = 0xff, .b = 0x00};
static const rgb_data_t CHANNEL_LED_DISALED = {.r = 0x00, .g = 0x00, .b = 0x00};
#endif

static LED_COLOR_STRUCT led_buffer[NUM_OF_LEDS];


#if NUM_SYSTEM_LEDS != 0
static LED_COLOR_STRUCT system_led_original_colors[NUM_SYSTEM_LEDS];
static uint8_t system_led_temp_status = 0;

// Locking for the 'event' colors
SemaphoreHandle_t led_buffer_mutex;
StaticSemaphore_t led_buffer_mutexStorage;
#endif


static void set_leds(void)
{
#ifdef WASCHV1
	led_ws2801_set(WS2801_GPIO_PORT, WS2801_GPIO_CLK, WS2801_GPIO_DATA, led_buffer, NUM_OF_LEDS);
#else
	i2s_rgb_set((const uint8_t *)led_buffer);
#endif
}


void led_status_system(enum LED_STATUS_SYSTEM status)
{
#if NUM_SYSTEM_LEDS != 0
	if (status >= LED_STATUS_COUNT)
	{
		return;
	}


	xSemaphoreTake(led_buffer_mutex, portMAX_DELAY);

	uint8_t led = status_color_table[status].led;
	if (status > LED_STATUS_EVENT)
	{
		if ((system_led_temp_status & (1 << led)) == 0)
		{
			// Save the old color (if not already saved)
			system_led_temp_status |= 1 << led;
			system_led_original_colors[led] = led_buffer[led];
		}

		COPY_CONVERT_RGB(led_buffer[led], status_color_table[status].color);
	}
	else
	{
		if ((system_led_temp_status & (1 << led)) == 0)
		{
			// No event color -> just set the new color
			COPY_CONVERT_RGB(led_buffer[led], status_color_table[status].color);
		}
		else
		{
			// Event color -> set the original color
			COPY_CONVERT_RGB(system_led_original_colors[led], status_color_table[status].color);
		}
	}


	xSemaphoreGive(led_buffer_mutex);
#endif
}


void led_status_channels(uint16_t enable, uint16_t status)
{
#if NUM_CHANNEL_LEDS != 0


	for (uint8_t i = 0; i < NUM_CHANNEL_LEDS; i++)
	{
		if ((enable & (1 << i)) == 0)
		{
			COPY_CONVERT_RGB(led_buffer[NUM_SYSTEM_LEDS + i], CHANNEL_LED_DISALED);
		}
		else if ((status & (1 << i)) == 0)
		{
			COPY_CONVERT_RGB(led_buffer[NUM_SYSTEM_LEDS + i], CHANNEL_LED_OFF);
		}
		else
		{
			COPY_CONVERT_RGB(led_buffer[NUM_SYSTEM_LEDS + i], CHANNEL_LED_ON);
		}
	}

#endif
}


void led_status_external(uint8_t led, rgb_data_t color)
{
	if (led >= NUM_EXTERNAL_LEDS)
	{
		return;
	}
	COPY_CONVERT_RGB(led_buffer[NUM_SYSTEM_LEDS + NUM_CHANNEL_LEDS + led], color);
}


void led_status_init(void)
{
#if NUM_SYSTEM_LEDS != 0
	led_buffer_mutex = xSemaphoreCreateMutexStatic(&led_buffer_mutexStorage);
#endif
#ifdef WASCHV1
	// GPIOs for LEDs
	rcc_periph_clock_enable(WS2801_GPIO_PORT_RCC);
	gpio_set_mode(WS2801_GPIO_PORT, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, WS2801_GPIO_CLK | WS2801_GPIO_DATA);
#else
	i2s_rgb_init();
#endif

	// Turn all LEDs on
	memset(led_buffer, 0xff, sizeof(led_buffer));
	set_leds();

	// Will be switched off in the first update
	memset(led_buffer, 0, sizeof(led_buffer));
}


void led_status_update(void)
{
	set_leds();

#if NUM_SYSTEM_LEDS != 0
	xSemaphoreTake(led_buffer_mutex, portMAX_DELAY);

	// Reset the tmp status
	for (uint8_t i = 0; i < NUM_SYSTEM_LEDS; i++)
	{
		if (system_led_temp_status  & (1 << i))
		{
			led_buffer[i] = system_led_original_colors[i];
		}
	}

	system_led_temp_status = 0;

	xSemaphoreGive(led_buffer_mutex);
#endif
}
