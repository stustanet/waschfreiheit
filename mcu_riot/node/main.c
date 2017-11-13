#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "thread.h"
#include "xtimer.h"
#include "shell.h"
#include "shell_commands.h"

#include "board.h"
#include "periph/rtc.h"

#ifdef MASTER

#include "master_node.h"
#include "master_config.h"
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
static const shell_command_t shell_commands[] = {
    { "config",        "Node configuration",                   master_config_set_cmd },
    { "connect",       "Connect to a node",                    master_node_cmd_connect },
    { "reset_routes",  "Reset node and set routes",            master_node_cmd_node_routes },
    { "set_routes",    "Set routes on a node",                 master_node_cmd_node_routes },
    { "cfg_sensor",    "Sets the sensor configuration",        master_node_cmd_configure_sensor },
    { "enable_sensor", "Sets the active sensor channels",      master_node_cmd_enable_sensor },
    { "raw_frames",    "Request raw sensor data from a node",  master_node_cmd_raw_frames },
    { "ping",          "Sends an echo reuest",                 master_node_cmd_ping },
    { "authping",      "Sends a conneted node is still alive", master_node_cmd_authping },
    { "master_routes", "Sets the routes for the master node",  master_node_cmd_master_routes },
    { NULL, NULL, NULL }
};

static void init(void)
{
	int mni = master_node_init();
	if (mni != 0)
	{
		printf("Master initialization failed with error %i\n", mni);
	}
}

#else

#include "sensor_node.h"
#include "sensor_config.h"

static const shell_command_t shell_commands[] = {
    { "cfg",     "Node configuration",                 sensor_config_set_cmd },
    { NULL, NULL, NULL }
};

static void init(void)
{
	int sni = sensor_node_init();
	if (sni != 0)
	{
		printf("Sensor node initialization failed with error %i\n", sni);
	}
}

#endif

int main(void)
{
	init();

    /* start the shell */
    puts("Initialization successful - starting the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
