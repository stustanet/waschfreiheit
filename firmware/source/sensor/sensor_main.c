/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


#include "cli.h"
#include "commands_common.h"
#include "tinyprintf.h"

#include "sensor_node.h"
#include "sensor_config.h"
#include "watchdog.h"

const cli_command_t cli_commands[] = {
    { "config",           "Node configuration",                      sensor_config_set_cmd },
    { "ping",             "Sends an echo reuest",                    cmd_ping },
    { "routes",           "Sets the routes for the node",            cmd_routes },
    { "raw",              "Enables / Disables raw data printing",    sensor_node_cmd_raw },
    { "led",              "RGB LED test",                            sensor_node_cmd_led },
    { "print_frames",     "Enbales / Disables frame value printing", sensor_node_cmd_print_frames },
    { "status",           "Prints node status",                      sensor_node_cmd_print_status },
#ifdef WASCHV1
    { "firmware_upgrade", "Firmware upgrade",                        sensor_node_cmd_firmware_upgrade },
#endif
    { NULL, NULL, NULL }
};

void node_init(void)
{
	// Start the watchdog, this needs to be fed min every 4 sec
	watchdog_init();
	int sni = sensor_node_init();
	if (sni != 0)
	{
		printf("Sensor node initialization failed with error %i\n", sni);
	}
	else
	{
		printf("Sensor node initialized\n");
	}
}
