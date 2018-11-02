/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


#include "master_node.h"
#include "master_config.h"
#include "cli.h"
#include "commands_common.h"
#include "tinyprintf.h"


/*
 * connect <NODE> <FIRST_HOP>
 *   Connect to a node
 * reset_routes <NODE> <DST1>,<HOP1> <DST2,HOP2> ...
 *   Reset node and push new routes
 * set_routes <NODE> <DST1>,<HOP1> <DST2,HOP2> ...
 *   Set the specified routes
 * configure_sensor <NODE> <CHANNEL> <PARAMS>
 *   Configure sensor on node
 * enable_sensor <NODE> <MASK> <SPS>
 *   Set active sensors
 * raw_frames <NODE> <CHANNEL> <COUNT>
 *   Request raw frame form node
 * ping <node_id>
 *   Debug ping to any node
 * authping <node_id>
 *   Check if connected node is still alive
 * routes <DST1>:<HOP1>,<DST2>:<HOP2>,...
 *   Set the master routes
 */
const cli_command_t cli_commands[] = {
    { "config",        "Node configuration",                    master_config_set_cmd },
    { "connect",       "Connect to a node",                     master_node_cmd_connect },
    { "retransmit",    "Re-sent timeouted packet",              master_node_cmd_retransmit },
    { "reset_routes",  "Reset node and set routes",             master_node_cmd_node_routes },
    { "set_routes",    "Set routes on a node",                  master_node_cmd_node_routes },
    { "cfg_sensor",    "Sets the sensor configuration",         master_node_cmd_configure_sensor },
    { "cfg_freq_chn",  "Configures a frequency sensor channel", master_node_cmd_configure_freq_sensor },
    { "enable_sensor", "Sets the active sensor channels",       master_node_cmd_enable_sensor },
    { "raw_frames",    "[DEBUG ONLY] Request raw sensor data from a node",  master_node_cmd_raw_frames },
    { "raw_status",    "[DEBUG ONLY] Request raw node status",  master_node_cmd_raw_status },
    { "ping",          "Sends an echo reuest",                  cmd_ping },
    { "authping",      "Sends a conneted node is still alive",  master_node_cmd_authping },
    { "led",           "Set the LEDs of a node",                master_node_cmd_led },
    { "rebuild_status_channel", "Request a node to rebuild the status channel", master_node_cmd_rebuild_status_channel },
    { "cfg_status_change_indicator", "Configure the status change indicator LEDs.", master_node_cmd_configure_status_change_indicator },
    { "routes",        "Sets the routes for the master node",  cmd_routes },
    { "reboot",        "Reboots (resets) the MCU",  cmd_reboot },
    { NULL, NULL, NULL }
};


// Defined in the common main
void node_init(void)
{
	int mni = master_node_init();
	if (mni != 0)
	{
		printf("Master initialization failed with error %i\n", mni);
	}
}
