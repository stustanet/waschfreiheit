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

#ifdef WASCHV2
#include "storage_manager.h"
#include "test_switch.h"
#endif

// Stack size of the LED thread (in words)
#define LED_THD_STACK_SIZE 128

static StaticTask_t led_thd_buffer;;
static StackType_t led_thd_stack[LED_THD_STACK_SIZE];
static bool disable_led_updates = false;


static void cmd_led_test(int argc, char **argv);


const cli_command_t cli_commands[] = {
    { "config",           "Node configuration",                      sensor_config_set_cmd },
    { "ping",             "Sends an echo reuest",                    cmd_ping },
    { "routes",           "Sets the routes for the node",            cmd_routes },
    { "raw",              "Enables / Disables raw data printing",    sensor_node_cmd_raw },
    { "led",              "RGB LED test",                            sensor_node_cmd_led },
    { "print_frames",     "Enbales / Disables frame value printing", sensor_node_cmd_print_frames },
    { "status",           "Prints node status",                      sensor_node_cmd_print_status },
    { "sx127x",           "RF modem debug",                          sx127x_test_cmd },
    { "reboot",           "Reboots (resets) the MCU",                cmd_reboot },
#ifdef WASCHV1
    { "firmware_upgrade", "Firmware upgrade",                        sensor_node_cmd_firmware_upgrade },
#else
    { "channel_test",     "Enter channel testing mode",              sensor_node_cmd_channel_test },
    { "led_test",         "Show LED test pattern",                   cmd_led_test },
    { "logger",           "Configure the logger",                    storage_manager_cmd_configure_logger },
    { "umount",           "Unmount USB storage",                    storage_manager_cmd_umount },
#endif
    { NULL, NULL, NULL }
};


static void led_thread(void *arg)
{
	(void) arg;
	static const uint32_t LOOP_DELAY_MS = 100;

	led_status_test(true);

    TickType_t last = xTaskGetTickCount();

	while(1)
	{
		vTaskDelayUntil(&last, LOOP_DELAY_MS);
		if (!disable_led_updates)
		{
			led_status_update();
		}
	}
}


static void cmd_led_test(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	printf("Start led test sequence\n");
	disable_led_updates = true;
	led_status_test(false);
	disable_led_updates = false;
	printf("Test sequence done\n");
}

void node_init(void)
{

	led_status_init();

#ifdef WASCHV2
	test_switch_init();
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
