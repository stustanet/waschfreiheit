#pragma once

#include <stdint.h>
#include "rgbcolor.h"

/*
 * The system has three different types of LEDs:
 * I    System status LEDs
 *      This are LEDs indication the overall status of the node.
 *
 * II   Channel LEDs
 *      One LED per channel used to show the channel status.
 *
 * III  External LEDs
 *      USed to display some information (like a global status) to the users.
 */

#ifdef WASCHV1

#define NUM_SYSTEM_LEDS 0
#define NUM_CHANNEL_LEDS 0
#define NUM_EXTERNAL_LEDS 5

#else

#define NUM_SYSTEM_LEDS 2
#define NUM_CHANNEL_LEDS 4
#define NUM_EXTERNAL_LEDS 5

#endif

#define NUM_OF_LEDS (NUM_SYSTEM_LEDS + NUM_CHANNEL_LEDS + NUM_EXTERNAL_LEDS)


enum LED_STATUS_SYSTEM
{
	LED_STATUS_SYSTEM_INIT,
	LED_STATUS_SYSTEM_ERROR,
	LED_STATUS_SYSTEM_CONNECTED,
	LED_STATUS_SYSTEM_SENSORS,
	LED_STATUS_SYSTEM_SCH_BUILT,
	LED_STATUS_SYSTEM_SCHNG_PEND,
	LED_STATUS_SYSTEM_USB_CON,
	LED_STATUS_SYSTEM_USB_OK,
	LED_STATUS_SYSTEM_USB_MOUNT_OK,
	LED_STATUS_SYSTEM_USB_MOUNT_ERR,
	LED_STATUS_SYSTEM_USB_ERR,
	LED_STATUS_SYSTEM_USB_DISCON,
	LED_STATUS_SYSTEM_USB_AUTOEXEC,

	LED_STATUS_EVENT = LED_STATUS_SYSTEM_USB_AUTOEXEC,  // Values above are event status
	LED_STATUS_SYSTEM_RX,
	LED_STATUS_SYSTEM_TX,
	LED_STATUS_SYSTEM_USB_RW,
	LED_STATUS_COUNT
};

/*
 * Updates the system status LEDs.
 * Some values of LED_SYSTEM_STATUS change the internal state while others only cause a temporary change in color.
 */
void led_status_system(enum LED_STATUS_SYSTEM status);

void led_status_channels(uint16_t enable, uint16_t status);

void led_status_external(uint8_t led, rgb_data_t color);

void led_status_init(void);
void led_status_update(void);
