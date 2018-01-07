#ifdef MASTER

/*
 * MASTER SERIAL PROTOCOL
 * Requests:
 * Requests are events occuring on the node that are signalen to the host machine where the node
 * is connected to.
 * These requests have to be machine readable. In order to mark them, they are prefixed with ###.
 * Defined requests:
 * ACK<NODE_ID>-<ACK_CODE>
 *   The last operation has finished
 *   <ACK_CODE> specifies a command specific code, nonzero values indicate erros
 * STATUS<NODE_ID>-<STATUS>
 *   Node sends status update
 * RAW<NODE_ID>-<COUNT>
 *   Begin of a block with <COUNT> raw frame values from <NODE_ID>
 *   The actual values will be printed linewise afterwards (prefixed with a '*')
 * TIMEOUT<NODE_ID>
 *   Retransmission limit reached
 * ERR
 *   Some error with the last command.
 *
 * Commands:
 * Commands can be sent by the user or automatically.
 *
 * Defined commands:
 * connect <NODE> <FIRST_HOP> <TIMEOUT>
 *   Connect to a node
 * retransmit <NODE>
 *   Re-send last packet
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

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <xtimer.h>
#include <thread.h>
#include <stdlib.h>
#include "meshnw.h"
#include "master_sensorconnection.h"
#include "messagetypes.h"
#include "utils.h"

#define MAX_ACTIVE_SENSORS 32

#define MASTER_NODE ((nodeid_t)0)

#define MESSAGE_LOOP_DELAY 1000000 // 1s

static char message_thd_stack[THREAD_STACKSIZE_DEFAULT];
static kernel_pid_t message_thd_pid;

struct
{
	sensor_connection_t nodes[MAX_ACTIVE_SENSORS];
} master;


static sensor_connection_t *find_node(nodeid_t node)
{
	for (uint8_t i = 0; i < ARRAYSIZE(master.nodes); i++)
	{
		if (sensor_connection_node(&master.nodes[i]) == node)
		{
			return &master.nodes[i];
		}
	}

	return NULL;
}


static sensor_connection_t *find_or_init_node(nodeid_t node)
{
	sensor_connection_t *n = find_node(node);
	if (n != NULL)
	{
		return n;
	}

	for (uint8_t i = 0; i < ARRAYSIZE(master.nodes); i++)
	{
		if (sensor_connection_node(&master.nodes[i]) == 0)
		{
			return &master.nodes[i];
		}
	}

	return NULL;
}


static void dispatch_packet(nodeid_t src, uint8_t *data, uint32_t len)
{
	sensor_connection_t *con = find_node(src);
	if (!con)
	{
		printf("Got packet from unknown node %u\n", src);
		return;
	}

	sensor_connection_handle_packet(con, data, len);
}


/*
 * connect <NODE> <FIRST_HOP> <TIMEOUT>
 *   Connect to a node
 */
int master_node_cmd_connect(int argc, char **argv)
{
	if (argc != 4)
	{
		puts("USAGE: connect <NODE> <FIRST_HOP> <TIMEOUT>\n");
		puts("NODE      The address of the node");
		puts("FIRST_HOP First hop in the answer path of the node");
		puts("TIMEOUT   Timeout for this connection");
		return 1;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 1);
	if (dst == MESHNW_INVALID_NODE)
	{
		return 1;
	}

	nodeid_t hop = utils_parse_nodeid(argv[2], 0);
	if (hop == MESHNW_INVALID_NODE)
	{
		return 1;
	}


	sensor_connection_t *con = find_or_init_node(dst);
	if (!con)
	{
		puts("Connection limit reached!");
		puts("###ERR");
		return 1;
	}

	uint16_t timeout = atoi(argv[3]);

	int res = sensor_connection_init(con, dst, hop, MASTER_NODE, timeout);
	if (res != 0)
	{
		printf("Connection init for node %u failed with error %i\n", dst, res);
		puts("###ERR");
		return 1;
	}
	return 0;
}


/*
 * retransmit <NODE>
 *   Re-send last packet
 */
int master_node_cmd_retransmit(int argc, char **argv)
{
	if (argc != 2)
	{
		puts("USAGE: retransmit <NODE>\n");
		puts("NODE      The address of the node");
		puts("This command can only be used if a TIMEOUT occured for this node!");
		return 1;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 1);
	if (dst == MESHNW_INVALID_NODE)
	{
		return 1;
	}

	sensor_connection_t *con = find_node(dst);
	if (!con)
	{
		puts("Not connected!");
		puts("###ERR");
		return 1;
	}

	int res = sensor_connection_retransmit(con);
	if (res != 0)
	{
		printf("Retransmission to node %u failed with error %i\n", dst, res);
		puts("###ERR");
		return 1;
	}
	return 0;
}


/*
 * reset_routes <NODE> <DST1>,<HOP1> <DST2,HOP2> ...
 *   Reset node and push new routes
 * set_routes <NODE> <DST1>,<HOP1> <DST2,HOP2> ...
 *   Set the specified routes
 */
int master_node_cmd_node_routes(int argc, char **argv)
{
	if (argc != 3)
	{
		puts("USAGE: reset_routes <NODE> <DST1>:<HOP1>,<DST2><HOP2>,...");
		puts("       set_routes   <NODE> <DST1>:<HOP1>,<DST2><HOP2>,...\n");

		puts("NODE      Address of the node");
		puts("DSTn:HOPn Packets with destination address DSTn will be sent to HOPn\n");
		puts("The reset_routes command will reset the node and than add new routes while the");
		puts("set_routes command updates the current routes");
		return 1;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 1);
	if (dst == MESHNW_INVALID_NODE)
	{
		return 1;
	}

	sensor_connection_t *con = find_node(dst);
	if (!con)
	{
		puts("Not connected!");
		puts("###ERR");
		return 1;
	}

	uint8_t reset = 0;
	if (strcmp(argv[0], "reset_routes") == 0)
	{
		reset = 1;
	}

	int res = sensor_connection_set_routes(con, reset, argv[2]);
	if (res != 0)
	{
		printf("Route request for node %u failed with error %i\n", dst, res);
		puts("###ERR");
		return 1;
	}
	return 0;
}


/*
 * configure_sensor <NODE> <CHANNEL> <IF> <MAT> <WND> <RF>
 *   Configure sensor on node
 */
int master_node_cmd_configure_sensor(int argc, char **argv)
{
	if (argc != 7)
	{
		puts("USAGE: configure_sensor <NODE> <CHANNEL> <IF> <MAT> <WND> <RF>\n");

		puts("NODE      Address of the node");
		puts("CHANNEL   Channel index");
		puts("IF        Input filter parameters");
		puts("          <MID_ADJ_SPEED>,<LOWPASS_WEIGTH>,<NUM_SAMPLES>");
		puts("MAT       State transition matrix");
		puts("          12 signed 16 bit values");
		puts("WND       Window sizes in different states");
		puts("          4 values, max 1536");
		puts("RF        Reject filter parameters");
		puts("          <REJECT_THRESHOLD>,<REJECT_CONSEC>");
		puts("See the documentation for details on the parameters.");

		return 1;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 1);
	if (dst == MESHNW_INVALID_NODE)
	{
		return 1;
	}

	sensor_connection_t *con = find_node(dst);
	if (!con)
	{
		puts("Not connected!");
		puts("###ERR");
		return 1;
	}

	uint8_t channel_id = atoi(argv[2]);

	int res = sensor_connection_configure_sensor(con, channel_id, argv[3], argv[4], argv[5], argv[6]);
	if (res != 0)
	{
		printf("Sensor config request for node %u (channel %u) failed with error %i\n", dst, channel_id, res);
		puts("###ERR");
		return 1;
	}
	return 0;
}


/*
 * enable_sensor <NODE> <MASK> <SPS>
 *   Set active sensors
 */
int master_node_cmd_enable_sensor(int argc, char **argv)
{
	if (argc != 4)
	{
		puts("USAGE: enable_sensor <NODE> <CHANNELS> <SPS>\n");

		puts("NODE      Address of the node");
		puts("CHANNELS  Active channels");
		puts("SPS       Samples per second");

		return 1;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 1);
	if (dst == MESHNW_INVALID_NODE)
	{
		return 1;
	}

	sensor_connection_t *con = find_node(dst);
	if (!con)
	{
		puts("Not connected!");
		puts("###ERR");
		return 1;
	}

	uint16_t channels = atoi(argv[2]);
	uint16_t sps = atoi(argv[3]);
	if (sps == 0)
	{
		sps = 1;
	}

	int res = sensor_connection_enable_sensors(con, channels, sps);
	if (res != 0)
	{
		printf("Sensor enable request for node %u failed with error %i\n", dst, res);
		puts("###ERR");
		return 1;
	}
	return 0;
}


/*
 * raw_frames <NODE> <CHANNEL> <COUNT>
 *   Request raw frame form node
 */

int master_node_cmd_raw_frames(int argc, char **argv)
{
	if (argc != 4)
	{
		puts("USAGE: raw_frames <NODE> <CHANNEL> <COUNT>\n");

		puts("NODE      Address of the node");
		puts("CHANNEL   Channel to measure");
		puts("COUNT     Number of frames to send");

		return 1;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 1);
	if (dst == MESHNW_INVALID_NODE)
	{
		return 1;
	}

	sensor_connection_t *con = find_node(dst);
	if (!con)
	{
		puts("Not connected!");
		puts("###ERR");
		return 1;
	}

	uint8_t channel = atoi(argv[2]);
	uint16_t frames = atoi(argv[3]);

	int res = sensor_connection_get_raw_data(con, channel, frames);
	if (res != 0)
	{
		printf("Raw data request for node %u failed with error %i\n", dst, res);
		puts("###ERR");
		return 1;
	}
	return 0;
}


/*
 * raw_status <node_id>
 *   Gets the raw status of the node including sensor and stateestimation values
 *   This command works in a similar way as raw_frames, meaning that the actual
 *   requested data is sent independent of the command without auth.
 */
int master_node_cmd_raw_status(int argc, char **argv)
{
	if (argc != 2)
	{
		puts("USAGE: raw_status <NODE>\n");
		puts("NODE      Address of the destination node");
		return 1;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 1);
	if (dst == MESHNW_INVALID_NODE)
	{
		return 1;
	}

	sensor_connection_t *con = find_node(dst);
	if (!con)
	{
		puts("Not connected!");
		puts("###ERR");
		return 1;
	}

	int res = sensor_connection_get_raw_status(con);
	if (res != 0)
	{
		printf("Send raw status request to node %u failed with error %i\n", dst, res);
		puts("###ERR");
		return 1;
	}
	return 0;
}


/*
 * authping <node_id>
 *   Check if connected node is still alive
 */
int master_node_cmd_authping(int argc, char **argv)
{
	if (argc != 2)
	{
		puts("USAGE: authping <NODE>\n");
		puts("NODE      Address of the destination node");
		return 1;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 1);
	if (dst == MESHNW_INVALID_NODE)
	{
		return 1;
	}

	sensor_connection_t *con = find_node(dst);
	if (!con)
	{
		puts("Not connected!");
		puts("###ERR");
		return 1;
	}

	int res = sensor_connection_authping(con);
	if (res != 0)
	{
		printf("Send authping to node %u failed with error %i\n", dst, res);
		puts("###ERR");
		return 1;
	}
	return 0;
}


int master_node_cmd_led(int argc, char **argv)
{
	if (argc < 3)
	{
		puts("USAGE: led <NODE> <LED1> ... <LEDn>.\n");
		puts("NODE      Address of the destination node");
		puts("LEDx      Color mode of the LED.");
		puts("          See the color table of the node for details.");
		return 1;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 1);
	if (dst == MESHNW_INVALID_NODE)
	{
		return 1;
	}

	sensor_connection_t *con = find_node(dst);
	if (!con)
	{
		puts("Not connected!");
		puts("###ERR");
		return 1;
	}

	int res = sensor_connection_led(con, argc - 2, argv + 2);
	if (res != 0)
	{
		printf("Send led request to node %u failed with error %i\n", dst, res);
		puts("###ERR");
		return 1;
	}
	return 0;
}


static void *message_thread(void *arg)
{
	(void) arg;
    xtimer_ticks32_t last = xtimer_now();

	while(1)
	{
        xtimer_periodic_wakeup(&last, MESSAGE_LOOP_DELAY);

		// update all connections
		for (uint8_t i = 0; i < ARRAYSIZE(master.nodes); i++)
		{
			if (sensor_connection_node(&master.nodes[i]) != 0)
			{
				sensor_connection_update(&master.nodes[i]);
			}
		}
	}

	return NULL;
}


static void message_callback(nodeid_t src, void *data, uint8_t len)
{
	if (len == 0)
	{
		return;
	}

	msg_union_t *msg = (msg_union_t *)data;
	if (msg->type == MSG_TYPE_ECHO_REPLY)
	{
		printf("Got echo reply from %u\n", src);
		return;
	}
	else if (msg->type == MSG_TYPE_ECHO_REQUEST)
	{
		msg_echo_request_t ping;
		ping.type = MSG_TYPE_ECHO_REPLY;

		meshnw_send(src, &ping, sizeof(ping));
		return;
	}

	dispatch_packet(src, data, len);
}


int master_node_init(void)
{
	printf("Start node in MASTER mode, id = %u\n", MASTER_NODE);
	memset(master.nodes, 0, sizeof(master.nodes));

	static const meshnw_rf_config_t rf_config = { 433500000, 10 };

	meshnw_init(MASTER_NODE, &rf_config, message_callback);

	message_thd_pid = thread_create(message_thd_stack, sizeof(message_thd_stack), THREAD_PRIORITY_MAIN - 1,
	                          THREAD_CREATE_STACKTEST, message_thread, NULL,
	                          "node_message_thread");

	if (message_thd_pid <= KERNEL_PID_UNDEF)
	{
		puts("Creation of mssage thread failed");
		return 1;
	}

	return 0;
}

#endif
