/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


#include "commands_common.h"

#include "tinyprintf.h"
#include "meshnw.h"
#include "messagetypes.h"
#include "utils.h"

/*
 * master_routes <DST1>:<HOP1>,<DST2>:<HOP2>,...
 *   Set the master routes
 */
void cmd_routes(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("USAGE: routes <DST1>:<HOP1>,<DST2><HOP2>,...\n\n"
			   "DSTn:HOPn Packets with destination address DSTn will be sent to HOPn\n\n");
		return;
	}

	const char *routes = argv[1];

	while(routes[0] != 0)
	{
		nodeid_t dst;
		nodeid_t hop;
		if (utils_parse_route(&routes, &dst, &hop) != 0)
		{
			return;
		}

		meshnw_set_route(dst, hop);
		printf("Add route %u:%u\n", dst, hop);

		if (routes[0] != 0 && routes[0] != ',')
		{
			printf("Unexpected route delim: %i(%c)\n", routes[0], routes[0]);
			return;
		}

		if (routes[0] == ',')
		{
			routes++;
		}
	}

#ifndef MASTER
	meshnw_enable_forwarding();
#endif
}


/*
 * ping <node_id>
 *   Debug ping to any node
 */
void cmd_ping(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("USAGE: ping <NODE>\n"
			   "NODE      Address of the destination node\n");
		return;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 0);
	if (dst == MESHNW_INVALID_NODE)
	{
		return;
	}

	// send ping
	msg_echo_request_t ping;
	ping.type = MSG_TYPE_ECHO_REQUEST;

	int res = meshnw_send(dst, &ping, sizeof(ping));
	if (res != 0)
	{
		printf("Send ping request to node %u failed with error %i\n", dst, res);
		return;
	}
}
