#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <thread.h>
#include <shell.h>
#include <shell_commands.h>

#include <board.h>
#include <periph/rtc.h>

#include "utils.h"
#include "messagetypes.h"

#include "watchdog.h"


/*
 * master_routes <DST1>:<HOP1>,<DST2>:<HOP2>,...
 *   Set the master routes
 */
static int cmd_routes(int argc, char **argv)
{
	if (argc != 2)
	{
		puts("USAGE: routes <DST1>:<HOP1>,<DST2><HOP2>,...\n\n");

		puts("DSTn:HOPn Packets with destination address DSTn will be sent to HOPn\n\n");
		return 1;
	}

	const char *routes = argv[1];

	while(routes[0] != 0)
	{
		nodeid_t dst;
		nodeid_t hop;
		if (utils_parse_route(&routes, &dst, &hop) != 0)
		{
			return 1;
		}

		meshnw_set_route(dst, hop);
		printf("Add route %u:%u\n", dst, hop);

		if (routes[0] != 0 && routes[0] != ',')
		{
			printf("Unexpected route delim: %i(%c)\n", routes[0], routes[0]);
			return 1;
		}

		if (routes[0] == ',')
		{
			routes++;
		}
	}

	return 0;
}


/*
 * ping <node_id>
 *   Debug ping to any node
 */
int cmd_ping(int argc, char **argv)
{
	if (argc != 2)
	{
		puts("USAGE: ping <NODE>\n");
		puts("NODE      Address of the destination node\n");
		return 1;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 0);
	if (dst == MESHNW_INVALID_NODE)
	{
		return 1;
	}

	// send ping
	msg_echo_request_t ping;
	ping.type = MSG_TYPE_ECHO_REQUEST;

	int res = meshnw_send(dst, &ping, sizeof(ping));
	if (res != 0)
	{
		printf("Send ping request to node %u failed with error %i\n", dst, res);
		return 1;
	}
	return 0;
}

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
    { "retransmit",    "Re-sent timeouted packet",             master_node_cmd_retransmit },
    { "reset_routes",  "Reset node and set routes",            master_node_cmd_node_routes },
    { "set_routes",    "Set routes on a node",                 master_node_cmd_node_routes },
    { "cfg_sensor",    "Sets the sensor configuration",        master_node_cmd_configure_sensor },
    { "enable_sensor", "Sets the active sensor channels",      master_node_cmd_enable_sensor },
    { "raw_frames",    "[DEBUG ONLY] Request raw sensor data from a node",  master_node_cmd_raw_frames },
    { "raw_status",    "[DEBUG ONLY] Request raw node status",  master_node_cmd_raw_status },
    { "ping",          "Sends an echo reuest",                 cmd_ping },
    { "authping",      "Sends a conneted node is still alive", master_node_cmd_authping },
    { "led",           "Set the LEDs of a node",               master_node_cmd_led },
    { "routes",        "Sets the routes for the master node",  cmd_routes },
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
    { "config",  "Node configuration",                   sensor_config_set_cmd },
    { "ping",    "Sends an echo reuest",                 cmd_ping },
    { "routes",  "Sets the routes for the node",         cmd_routes },
    { "raw",     "Enables / Disables raw data printing", sensor_node_cmd_raw },
    { "led",     "RGB LED test",                         sensor_node_cmd_led },
    { NULL, NULL, NULL }
};

static void init(void)
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
		puts("Sensor node initialized");
	}
}

#endif

int main(void)
{
	init();

    char line_buf[256];
    shell_run(shell_commands, line_buf, sizeof(line_buf));

    return 0;
}
