/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

/*
 * MASTER SERIAL PROTOCOL
 * Requests:
 * Requests are events occuring on the node that are signaled to the host machine where the node
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

#include "master_node.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "meshnw.h"
#include "master_sensorconnection.h"
#include "messagetypes.h"
#include "utils.h"
#include "tinyprintf.h"
#include "isrsafe_printf.h"

#define MAX_ACTIVE_SENSORS 32

#define MASTER_NODE ((nodeid_t)0)

#define MESSAGE_LOOP_DELAY 1000 // 1s

// Stack size of the maaaster thread (in words)
#define MESSAGE_THD_STACK_SIZE 512

StaticTask_t message_thd_buffer;;
StackType_t message_thd_stack[MESSAGE_THD_STACK_SIZE];

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

static void print_err_text(void)
{
	ISRSAFE_PRINTF("###ERR\n");
}


/*
 * connect <NODE> <FIRST_HOP> <TIMEOUT>
 *   Connect to a node
 */
void master_node_cmd_connect(int argc, char **argv)
{
	if (argc != 4)
	{
		printf("USAGE: connect <NODE> <FIRST_HOP> <TIMEOUT>\n\n"
			   "NODE      The address of the node\n"
			   "FIRST_HOP First hop in the answer path of the node\n"
			   "TIMEOUT   Timeout for this connection\n");
		return;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 1);
	if (dst == MESHNW_INVALID_NODE)
	{
		return;
	}

	nodeid_t hop = utils_parse_nodeid(argv[2], 0);
	if (hop == MESHNW_INVALID_NODE)
	{
		return;
	}


	sensor_connection_t *con = find_or_init_node(dst);
	if (!con)
	{
		printf("Connection limit reached!\n");
		print_err_text();
		return;
	}

	uint16_t timeout = atoi(argv[3]);

	int res = sensor_connection_init(con, dst, hop, MASTER_NODE, timeout);
	if (res != 0)
	{
		printf("Connection init for node %u failed with error %i\n", dst, res);
		print_err_text();
		return;
	}
}


/*
 * retransmit <NODE>
 *   Re-send last packet
 */
void master_node_cmd_retransmit(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("USAGE: retransmit <NODE>\n\n"
			   "NODE      The address of the node\n"
			   "This command can only be used if a TIMEOUT occured for this node!\n");
		return;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 1);
	if (dst == MESHNW_INVALID_NODE)
	{
		return;
	}

	sensor_connection_t *con = find_node(dst);
	if (!con)
	{
		printf("Not connected!\n");
		print_err_text();
		return;
	}

	int res = sensor_connection_retransmit(con);
	if (res != 0)
	{
		printf("Retransmission to node %u failed with error %i\n", dst, res);
		print_err_text();
		return;
	}
	return;
}


/*
 * reset_routes <NODE> <DST1>,<HOP1> <DST2,HOP2> ...
 *   Reset node and push new routes
 * set_routes <NODE> <DST1>,<HOP1> <DST2,HOP2> ...
 *   Set the specified routes
 */
void master_node_cmd_node_routes(int argc, char **argv)
{
	if (argc != 3)
	{
		printf("USAGE: reset_routes <NODE> <DST1>:<HOP1>,<DST2><HOP2>,...\n"
			   "       set_routes   <NODE> <DST1>:<HOP1>,<DST2><HOP2>,...\n\n"

			   "NODE      Address of the node\n"
			   "DSTn:HOPn Packets with destination address DSTn will be sent to HOPn\n\n"
			   "The reset_routes command will reset the node and than add new routes while the\n"
			   "set_routes command updates the current routes\n");
		return;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 1);
	if (dst == MESHNW_INVALID_NODE)
	{
		return;
	}

	sensor_connection_t *con = find_node(dst);
	if (!con)
	{
		printf("Not connected!\n");
		print_err_text();
		return;
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
		print_err_text();
		return;
	}
	return;
}


/*
 * configure_sensor <NODE> <CHANNEL> <IF> <MAT> <WND> <RF>
 *   Configure sensor on node
 */
void master_node_cmd_configure_sensor(int argc, char **argv)
{
	if (argc != 7)
	{
		printf("USAGE: cfg_sensor <NODE> <CHANNEL> <IF> <MAT> <WND> <RF>\n\n"
			   "NODE      Address of the node\n"
			   "CHANNEL   Channel index\n"
			   "IF        Input filter parameters\n"
			   "          <MID_ADJ_SPEED>,<LOWPASS_WEIGTH>,<NUM_SAMPLES>\n"
			   "MAT       State transition matrix\n"
			   "          12 signed 16 bit values\n"
			   "WND       Window sizes in different states\n"
			   "          4 values, max 1536\n"
			   "RF        Reject filter parameters\n"
			   "          <REJECT_THRESHOLD>,<REJECT_CONSEC>\n"
			   "See the documentation for details on the parameters.\n");

		return;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 1);
	if (dst == MESHNW_INVALID_NODE)
	{
		return;
	}

	sensor_connection_t *con = find_node(dst);
	if (!con)
	{
		printf("Not connected!\n");
		print_err_text();
		return;
	}

	uint8_t channel_id = atoi(argv[2]);

	int res = sensor_connection_configure_sensor(con, channel_id, argv[3], argv[4], argv[5], argv[6]);
	if (res != 0)
	{
		printf("Sensor config request for node %u (channel %u) failed with error %i\n", dst, channel_id, res);
		print_err_text();
		return;
	}
	return;
}


/*
 * enable_sensor <NODE> <MASK> <SPS>
 *   Set active sensors
 */
void master_node_cmd_enable_sensor(int argc, char **argv)
{
	if (argc != 4)
	{
		printf("USAGE: enable_sensor <NODE> <CHANNELS> <SPS>\n\n"
			   "NODE      Address of the node\n"
			   "CHANNELS  Active channels\n"
			   "SPS       Samples per second\n");

		return;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 1);
	if (dst == MESHNW_INVALID_NODE)
	{
		return;
	}

	sensor_connection_t *con = find_node(dst);
	if (!con)
	{
		printf("Not connected!\n");
		print_err_text();
		return;
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
		print_err_text();
		return;
	}
	return;
}


/*
 * raw_frames <NODE> <CHANNEL> <COUNT>
 *   Request raw frame form node
 */

void master_node_cmd_raw_frames(int argc, char **argv)
{
	if (argc != 4)
	{
		printf("USAGE: raw_frames <NODE> <CHANNEL> <COUNT>\n\n"
			   "NODE      Address of the node\n"
			   "CHANNEL   Channel to measure\n"
			   "COUNT     Number of frames to send\n");

		return;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 1);
	if (dst == MESHNW_INVALID_NODE)
	{
		return;
	}

	sensor_connection_t *con = find_node(dst);
	if (!con)
	{
		printf("Not connected!\n");
		print_err_text();
		return;
	}

	uint8_t channel = atoi(argv[2]);
	uint16_t frames = atoi(argv[3]);

	int res = sensor_connection_get_raw_data(con, channel, frames);
	if (res != 0)
	{
		printf("Raw data request for node %u failed with error %i\n", dst, res);
		print_err_text();
		return;
	}
	return;
}


/*
 * raw_status <node_id>
 *   Gets the raw status of the node including sensor and stateestimation values
 *   This command works in a similar way as raw_frames, meaning that the actual
 *   requested data is sent independent of the command without auth.
 */
void master_node_cmd_raw_status(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("USAGE: raw_status <NODE>\n\n"
			   "NODE      Address of the destination node\n");
		return;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 1);
	if (dst == MESHNW_INVALID_NODE)
	{
		return;
	}

	sensor_connection_t *con = find_node(dst);
	if (!con)
	{
		printf("Not connected!\n");
		print_err_text();
		return;
	}

	int res = sensor_connection_get_raw_status(con);
	if (res != 0)
	{
		printf("Send raw status request to node %u failed with error %i\n", dst, res);
		print_err_text();
		return;
	}
	return;
}


/*
 * authping <node_id>
 *   Check if connected node is still alive
 */
void master_node_cmd_authping(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("USAGE: authping <NODE>\n\n"
			   "NODE      Address of the destination node\n");
		return;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 1);
	if (dst == MESHNW_INVALID_NODE)
	{
		return;
	}

	sensor_connection_t *con = find_node(dst);
	if (!con)
	{
		printf("Not connected!\n");
		print_err_text();
		return;
	}

	int res = sensor_connection_authping(con);
	if (res != 0)
	{
		printf("Send authping to node %u failed with error %i\n", dst, res);
		print_err_text();
		return;
	}
	return;
}


void master_node_cmd_led(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("USAGE: led <NODE> <LED1> ... <LEDn>.\n\n"
			   "NODE      Address of the destination node\n"
			   "LEDx      Color mode of the LED.\n"
			   "          See the color table of the node for details.\n");
		return;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 1);
	if (dst == MESHNW_INVALID_NODE)
	{
		return;
	}

	sensor_connection_t *con = find_node(dst);
	if (!con)
	{
		printf("Not connected!\n");
		print_err_text();
		return;
	}

	int res = sensor_connection_led(con, argc - 2, argv + 2);
	if (res != 0)
	{
		printf("Send led request to node %u failed with error %i\n", dst, res);
		print_err_text();
		return;
	}
	return;
}

/*
 * rebuild_status_channel <node_id>
 *   Tell a node to rebuild its status channel.
 */
void master_node_cmd_rebuild_status_channel(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("USAGE: rebuild_status_channel <NODE>\n\n"
			   "NODE      Address of the destination node\n\n"
			   "This needs to be called if it is not reset after reconnecting.\n");
        return;
    }

    nodeid_t dst = utils_parse_nodeid(argv[1], 1);
    if (dst == MESHNW_INVALID_NODE)
    {
        return;
    }

    sensor_connection_t *con = find_node(dst);
    if (!con)
    {
        printf("Not connected!\n");
        print_err_text();
        return;
    }

    int res = sensor_connection_rebuild_status_channel(con);
    if (res != 0)
    {
        printf("Send rebuild_status_channel to node %u failed with error %i\n", dst, res);
        print_err_text();
        return;
    }
    return;
}


void master_node_cmd_configure_status_change_indicator(int argc, char **argv)
{
	if (argc < 3)
	{
		printf("USAGE: cfg_status_change_indicator <NODE> <CHANNEL0> ... <CHANNELn>.\n\n"
			   "NODE        Address of the destination node\n"
			   "<CHANNELx>  Configuration for a single channel in the format:\n"
			   "            <CHANNEL>,<LED>,<COLOR>\n"
			   "            When a status change on the CHANNEL is detected, the\n"
			   "            LED is set to blink in the specified COLOR.\n");
		return;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 1);
	if (dst == MESHNW_INVALID_NODE)
	{
		return;
	}

	sensor_connection_t *con = find_node(dst);
	if (!con)
	{
		printf("Not connected!\n");
		print_err_text();
		return;
	}

	int res = sensor_connection_configure_status_change_indicator(con, argc - 2, argv + 2);
	if (res != 0)
	{
		printf("Failed to configure status indicator on node %u with error %i\n", dst, res);
		print_err_text();
		return;
	}
}


void master_node_cmd_configure_freq_sensor(int argc, char **argv)
{
	if (argc != 6)
	{
		printf("USAGE: cfg_freq_chn <NODE> <CHANNEL> <THRESHOLD> <SAMPLECOUNT> <NEG_THRESHOLD>.\n\n"
			   "NODE           Address of the destination node\n"
			   "CHANNEL        Channel to configure\n"
			   "THRESHOLD      Number of positive edges per sample to accept the sample as positive\n"
			   "               Each sample is 10 sec long.\n"
			   "SAMPLECOUNT    Number of samples in the window\n"
			   "NEG_THRESHOLD  If more than this number of samples in the window are negative,\n"
			   "               the channel is considered negative.\n");
		return;
	}

	nodeid_t dst = utils_parse_nodeid(argv[1], 1);
	if (dst == MESHNW_INVALID_NODE)
	{
		return;
	}

	sensor_connection_t *con = find_node(dst);
	if (!con)
	{
		printf("Not connected!\n");
		print_err_text();
		return;
	}

	uint8_t chn = atoi(argv[2]);
	uint16_t thd = atoi(argv[3]);
	uint8_t sc = atoi(argv[4]);
	uint8_t nt = atoi(argv[5]);

	int res = sensor_connection_configure_freq_channel(con, chn, thd, sc, nt);
	if (res != 0)
	{
		printf("Failed to configure freq channel on node %u with error %i\n", dst, res);
		print_err_text();
		return;
	}
}


static void message_thread(void *arg)
{
	(void) arg;
    TickType_t last = xTaskGetTickCount();

	while(1)
	{
		vTaskDelayUntil(&last, MESSAGE_LOOP_DELAY);

		// update all connections
		for (uint8_t i = 0; i < ARRAYSIZE(master.nodes); i++)
		{
			if (sensor_connection_node(&master.nodes[i]) != 0)
			{
				sensor_connection_update(&master.nodes[i]);
			}
		}
	}
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

	static const sx127x_rf_config_t rf_config = { 433500000, 10 , 10, 2, 7 };

	meshnw_init(MASTER_NODE, &rf_config, message_callback);

	xTaskCreateStatic(
		&message_thread,
		"MESSAGE",
	    MESSAGE_THD_STACK_SIZE,
		NULL,
		tskIDLE_PRIORITY + 3,
		message_thd_stack,
		&message_thd_buffer);

	return 0;
}
