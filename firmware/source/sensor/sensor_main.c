/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


#include "cli.h"

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "commands_common.h"
#include "tinyprintf.h"
#include "sensor_node.h"
#include "sensor_config.h"
#include "led_status.h"
#include "watchdog.h"

#ifdef WASCHV2
#include "storage_manager.h"
#endif

// Stack size of the LED thread (in words)
#define LED_THD_STACK_SIZE 128

static StaticTask_t led_thd_buffer;;
static StackType_t led_thd_stack[LED_THD_STACK_SIZE];

const cli_command_t cli_commands[] = {
    { "config",           "Node configuration",                      sensor_config_set_cmd },
    { "ping",             "Sends an echo reuest",                    cmd_ping },
    { "routes",           "Sets the routes for the node",            cmd_routes },
    { "raw",              "Enables / Disables raw data printing",    sensor_node_cmd_raw },
    { "led",              "RGB LED test",                            sensor_node_cmd_led },
    { "print_frames",     "Enbales / Disables frame value printing", sensor_node_cmd_print_frames },
    { "status",           "Prints node status",                      sensor_node_cmd_print_status },
    { "sx127x",           "RF modem debug",                          sx127x_test_cmd },
#ifdef WASCHV1
    { "firmware_upgrade", "Firmware upgrade",                        sensor_node_cmd_firmware_upgrade },
#else
    { "channel_test",     "Enter channel testing mode",              sensor_node_cmd_channel_test },
    { "logger",           "Configure the logger",                    storage_manager_cmd_configure_logger },
    { "umount",           "Unmount USB storage",                    storage_manager_cmd_umount },
#endif
    { NULL, NULL, NULL }
};


static void led_thread(void *arg)
{
	(void) arg;
	static const uint32_t LOOP_DELAY_MS = 100;

    TickType_t last = xTaskGetTickCount();

	while(1)
	{
		vTaskDelayUntil(&last, LOOP_DELAY_MS);
		led_status_update();
	}
}

void node_init(void)
{
	// Start the watchdog, this needs to be fed min every 4 sec
	watchdog_init();

#ifdef WASCHV2
	storage_manager_init();
#endif

	int sni = sensor_node_init();
	if (sni != 0)
	{
		printf("Sensor node initialization failed with error %i\n", sni);
	}
	else
	{
		printf("Sensor node initialized\n");
	}

	xTaskCreateStatic(
		&led_thread,
		"LED",
	    LED_THD_STACK_SIZE,
		NULL,
		tskIDLE_PRIORITY,
		led_thd_stack,
		&led_thd_buffer);
}
