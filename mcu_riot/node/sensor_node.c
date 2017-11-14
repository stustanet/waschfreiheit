#ifndef MASTER

#include "sensor_node.h"
#include <string.h>
#include <stdio.h>
#include <xtimer.h>
#include <thread.h>
#include <periph/pm.h>
#include <periph/adc.h>

#include "meshnw.h"
#include "state_estimation.h"
#include "sensor_config.h"
#include "auth.h"
#include "messagetypes.h"

#define NUM_OF_SENSORS 2

/*
 * Max number of retransmissions before the node declaers the network dead and shuts down.
 */
#define MAX_STATUS_RETRANSMISSIONS 100

#define CON_RETRANSMISSION_BASE_DELAY      6

/*
 * Actual retransmission delay is
 * (retransmission_delay * (1 + num_of_retries / RETRANSMISSION_LINEAR_BACKOFF_DIVIDER)
 */
#define RETRANSMISSION_LINEAR_BACKOFF_DIVIDER 3

// Basic initialization complete, node is now active and can process messages
#define STATUS_INIT_CPLT           1

// Route initialization complete, node has valid routes
#define STATUS_INIT_ROUTES         2

// Status auth initialization complete, node can send status update
#define STATUS_INIT_AUTH_STA       4

// Config auth initialization complete, node can now receive config messages
#define STATUS_INIT_AUTH_CFG       8

// Stauts auth init pending, hs1 was sent to the master node, waiting for hs2
#define STATUS_INIT_AUTH_STA_PEND 16

// Sensors have been started
#define STATUS_SENSORS_ACTIVE     32

struct
{
	state_estimation_data_t sensors[NUM_OF_SENSORS];

	// auth context for sending status information
	auth_context_t auth_status;

	// auth context for receiving config commands
	auth_context_t auth_config;

	/*
	 * Status information for the node (STATUS_ bits)
	 * Describes the initialization status of the individual components.
	 */
	uint32_t status;

	/*
	 * Delay in us for the adc loop.
	 */
	uint32_t sensor_loop_delay_us;

	/*
	 * The bits specify the active / enabled sensor channels.
	 * If a channel is active, the adc is sampled and status updates are sent.
	 */
	uint16_t active_sensor_channels;

	/*
	 * The current status of the sensors.
	 */
	uint16_t current_sensor_status;

	/*
	 * Number of raw frames to send.
	 * This is always decremented. When the counter is zero no more raw frames are sent.
	 */
	uint16_t debug_raw_frame_transmission_counter;
	uint8_t debug_raw_frame_channel;

	/*
	 * Set to true, if a valid ack message was receiced on the status channel,
	 * This is set to 0 again when a new status message is sent.
	 */
	uint8_t last_status_msg_was_acked;

	/*
	 * The ack result of the last received control message.
	 * This is used as the result if a message is re-acked.
	 */
	uint8_t last_ack_result;

	/*
	 * Base retransmission delay for status update messages
	 */
	uint8_t status_retransmission_base_delay;

	/*
	 * The address of this node.
	 */
	nodeid_t current_node;

	/*
	 * The address of the master node.
	 * The address of the first valid route request is used as master address.
	 * Changing the master address afterwards is currently impossible (requires a restart).
	 */
	nodeid_t master_node;
	
} ctx;

static char adc_thd_stack[512 * 3];
static kernel_pid_t adc_thd_pid;

static char message_thd_stack[512 * 3];
static kernel_pid_t message_thd_pid;


/*
 * Usual initialization process when master starts
 * M is the master node, N is the current node
 *
 * M                              N
 * |------------- HS1 ----------->|
 * |<------------ HS2 ------------|
 * |                              | <- Config channel valid
 * |------------ ROUTES --------->|
 * |<------------ ACK ------------|
 * |                              | <- Routes valid, node enables packet relay
 * |                              |
 *  ++++++++++++++++++++++++++++++  <- Master inits other nodes
 * |                              |
 * |-------- CFG SENSOR 0 ------->| <- Master configures sensor parameters
 * |<------------ ACK ------------|
 * |-------- CFG SENSOR 1 ------->|
 * |<------------ ACK ------------|
 * |                              |
 *  ++++++++++++++++++++++++++++++  <- Master configures sensors on other nodes
 * |                              |
 * |-------- START_SENSOR ------->| <- Master tells node to start measuring and report status
 * |<------------ ACK ------------|
 * |                              |
 * |<------------ HS1 ------------| <- Node builds status channel
 * |------------- HS2 ----------->|
 * |                              |
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  <- Node waits for status change
 * |                              |
 * |<------- STATUS UPDATE -------| <- Status update message
 * |------------- ACK ----------->|
 */


/*
 * Resets the sensor node to initial state. Everything except the config auth is reset.
 */
static void reset(void)
{
	// Clear all bits except for INIT_CPLT and INIT_AUTH_CFG
	ctx.status &= STATUS_INIT_CPLT | STATUS_INIT_AUTH_CFG;

	// Clear routes
	meshnw_clear_routes();

	// reset ack outstanding status
	ctx.last_status_msg_was_acked = 1;

	// disable raw frame transmission
	ctx.debug_raw_frame_transmission_counter = 0;

	// disable all sensor channels
	ctx.active_sensor_channels = 0;

	// reset sensor loop delay to 1s
	ctx.sensor_loop_delay_us = 1000000;
}


/*
 * Handles the incoming hs1 from the master to open the config channel.
 */
static void handle_auth_slave_handshake(nodeid_t src, void *data, uint8_t len)
{
	printf("Dump hs1: ");
	for (uint32_t i = 0; i < len; i++)
	{
		printf("%02x", ((uint8_t *)data)[i]);
	}
	puts("");

	uint8_t out_buffer[MESHNW_MAX_PACKET_SIZE];
	uint32_t rep_msg_len = sizeof(out_buffer);

	msg_auth_hs_1_t *hs1 = (msg_auth_hs_1_t *)data;
	msg_auth_hs_2_t *hs2 = (msg_auth_hs_2_t *)out_buffer;

	hs2->type = MSG_TYPE_AUTH_HS_2;

	// this also does length checking
	int res = auth_slave_handshake(&ctx.auth_config, data, sizeof(*hs1), len, out_buffer, sizeof(*hs2), &rep_msg_len);

	if (res != 0)
	{
		printf("Slave handshake failed with error %i\n", res);
		return;
	}

	printf("Dump hs2: ");
	for (uint32_t i = 0; i < rep_msg_len; i++)
	{
		printf("%02x", out_buffer[i]);
	}
	puts("");


	// handshake ok
	
	if ((ctx.status & STATUS_INIT_ROUTES) == 0)
	{
		// no routes yet -> set temp route
		meshnw_set_route(src, hs1->reply_route);

		printf("Add temp route %u:%u\n", src, hs1->reply_route);

		// also, store the source address as master address, the route request is expected from the same address.
		ctx.master_node = src;
	}
	
	// reply with hs2
	res = meshnw_send(src, out_buffer, rep_msg_len);
	
	if (res != 0)
	{
		printf("sending slave handshake reply failed with error %i\n", res);
		return;
	}
	
	ctx.status |= STATUS_INIT_AUTH_CFG;
}


static void init_status_auth(void)
{
	if ((ctx.status & STATUS_INIT_ROUTES) == 0)
	{
		puts("Called init_status_auth() befrore route setup");
		return;
	}

	uint8_t out_buffer[MESHNW_MAX_PACKET_SIZE];
	uint32_t rep_msg_len = sizeof(out_buffer);

	msg_auth_hs_1_t *hs1 = (msg_auth_hs_1_t *)out_buffer;

	hs1->type = MSG_TYPE_AUTH_HS_1;

	// No need to tell the master node a route
	hs1->reply_route = MESHNW_INVALID_NODE;

	int res = auth_master_make_handshake(&ctx.auth_status, out_buffer, sizeof(*hs1), &rep_msg_len);

	if (res != 0)
	{
		printf("auth_master_make_handshake failed with error %i\n", res);
		return;
	}

	res = meshnw_send(ctx.master_node, out_buffer, rep_msg_len);
	
	if (res != 0)
	{
		printf("sending master handshake failed with error %i\n", res);
		return;
	}

	ctx.status &= ~STATUS_INIT_AUTH_STA;
	ctx.status |= STATUS_INIT_AUTH_STA_PEND;
}


static void handle_auth_master_handshake(nodeid_t src, void *data, uint8_t len)
{
	if ((ctx.status & STATUS_INIT_ROUTES) == 0)
	{
		puts("Received hs2 before route setup");
		return;
	}

	if ((ctx.status & STATUS_INIT_AUTH_STA_PEND) == 0)
	{
		puts("Received hs2 but handshake is not pending");
		return;
	}

	if (src != ctx.master_node)
	{
		puts("Got hs2 from a node that is not the current master");
		return;
	}

	msg_auth_hs_2_t *hs2 = (msg_auth_hs_2_t *)data;
	int res = auth_master_process_handshake(&ctx.auth_status, data, sizeof(*hs2), len);
	if (res != 0)
	{
		printf("auth_master_process_handshake failed with error %i\n", res);
		return;
	}

	ctx.status &= ~STATUS_INIT_AUTH_STA_PEND;
	ctx.status |= STATUS_INIT_AUTH_STA;
}


static void send_ack(uint8_t ack_result)
{
	if ((ctx.status & STATUS_INIT_AUTH_CFG) == 0)
	{
		puts("Called send_ack() but auth is not init");
		return;
	}

	uint8_t out_buffer[MESHNW_MAX_PACKET_SIZE];
	uint32_t rep_msg_len = sizeof(out_buffer);
	
	msg_auth_ack_t *ack = (msg_auth_ack_t *)out_buffer;
	ack->type = MSG_TYPE_AUTH_ACK;
	ack->result_code = ack_result;

	int res = auth_slave_make_ack(&ctx.auth_config, out_buffer, sizeof(*ack), &rep_msg_len);

	if (res != 0)
	{
		printf("auth_slave_make_ack failed with error %i\n", res);
		return;
	}

	res = meshnw_send(ctx.master_node, out_buffer, rep_msg_len);
	
	if (res != 0)
	{
		printf("sending slave ack reply failed with error %i\n", res);
	}

	ctx.last_ack_result = ack_result;
}


static void handle_auth_master_ack(nodeid_t src, void *data, uint8_t len)
{
	if ((ctx.status & STATUS_INIT_AUTH_STA) == 0)
	{
		puts("Received ack before status auth init");
		return;
	}

	if (src != ctx.master_node)
	{
		puts("Got ack from a node that is not the current master");
		return;
	}

	msg_auth_ack_t *ack = (msg_auth_ack_t *)data;
	int res = auth_master_check_ack(&ctx.auth_status, data, sizeof(*ack), len);
	if (res != 0)
	{
		printf("auth_master_check_ack failed with error %i\n", res);
		return;
	}

	printf("got ack from master with code %u\n", ack->result_code);

	if (ctx.last_status_msg_was_acked)
	{
		printf("unexpected status ack! there is no outstanding packet!");
	}
	else
	{
		ctx.last_status_msg_was_acked = 1;
	}
}


static int check_auth_message(nodeid_t src, void *data, uint32_t *len)
{
	if ((ctx.status & STATUS_INIT_AUTH_CFG) == 0)
	{
		puts("Received authenticated message but cfg auth not initialized");
		return -1;
	}

	if (src != ctx.master_node)
	{
		printf("Received config message from unexpected source %u", src);
		return -1;
	}

	struct
	{
		nodeid_t src;
		nodeid_t dst;
	} add_data;
	add_data.src = src;
	add_data.dst = ctx.current_node;

	int res = auth_slave_verify(&ctx.auth_config, data, len, &add_data, sizeof(add_data));

	if (res == AUTH_OLD_NONCE)
	{
		// old nonce -> re-ack
		puts("Received message with old nonce -> re-ack");

		// set re-ack bit and ack
		send_ack(ctx.last_ack_result | 0x80);
	}
	else if (res != 0)
	{
		printf("auth_slave_verify failed with error %i\n", res);
		return res;
	}

	return res;
}


static void handle_route_request(nodeid_t src, void *data, uint8_t len)
{
	uint32_t msglen = len;

	if (check_auth_message(src, data, &msglen) != 0)
	{
		// something is wrong with the auth, cant proceed
		return;
	}

	msg_route_data_t *route_msg = (msg_route_data_t *)data;
	if (msglen < sizeof(*route_msg))
	{
		// too small
		printf("Received too small route message with size %lu\n", msglen);

		// still ack it so the master gets the error
		send_ack(1);
		return;
	}

	if (route_msg->type == MSG_TYPE_ROUTE_RESET)
	{
		reset();
	}

	uint32_t num_routes = (msglen - sizeof(*route_msg)) / sizeof(route_msg->r) + 1;

	for (uint32_t r = 0; r < num_routes; r++)
	{
		meshnw_set_route(route_msg->r[r].dst, route_msg->r[r].next);
	}

	// got routes now -> activate forwarding
	meshnw_enable_forwarding();
	ctx.status |= STATUS_INIT_ROUTES;

	// finally ack it
	send_ack(0);
}


static void handle_sensor_cfg_request(nodeid_t src, void *data, uint8_t len)
{
	uint32_t msglen = len;
	if (check_auth_message(src, data, &msglen) != 0)
	{
		// something is wrong with the auth, can't proceed
		return;
	}

	msg_configure_sensor_t *cfg_msg = (msg_configure_sensor_t *)data;
	if (msglen != sizeof(*cfg_msg))
	{
		// too small
		printf("Received sensor config message with wrong size %lu\n", msglen);
		send_ack(1);
		return;
	}

	if (cfg_msg->channel_id >= NUM_OF_SENSORS)
	{
		printf("Attempt to configure sensor with invalid index %u\n", cfg_msg->channel_id);
		send_ack(1);
		return;
	}

	uint16_t sps;
	if (ctx.sensor_loop_delay_us == 0)
	{
		sps = 1;
	}
	else
	{
		sps = 1000000 / ctx.sensor_loop_delay_us;
	}

	if (sps == 0)
	{
		sps = 1;
	}

	int init_res = stateest_init(&ctx.sensors[cfg_msg->channel_id], &(cfg_msg->params), sps);

	if (init_res != 0)
	{
		printf("Failed to initialize state estimation, error %i\n", init_res);
		send_ack(2);
		return;
	}

	// finally ack it
	send_ack(0);
}


static void handle_sensor_start_request(nodeid_t src, void *data, uint8_t len)
{
	uint32_t msglen = len;
	if (check_auth_message(src, data, &msglen) != 0)
	{
		// something is wrong with the auth, can't proceed
		return;
	}

	msg_start_sensor_t *start_msg = (msg_start_sensor_t *)data;
	if (msglen != sizeof*(start_msg))
	{
		// too small
		printf("Received sensor start message with wrong size %lu\n", msglen);
		send_ack(1);
		return;
	}

	ctx.active_sensor_channels = start_msg->active_sensors;
	ctx.status_retransmission_base_delay = start_msg->status_retransmission_delay;
	
	if (start_msg->adc_samples_per_sec == 0)
	{
		// 1 sec delay is max
		ctx.sensor_loop_delay_us = 1000000;
	}
	else
	{
		ctx.sensor_loop_delay_us = 1000000 / start_msg->adc_samples_per_sec;
	}

	uint16_t sps;
	if (ctx.sensor_loop_delay_us == 0)
	{
		sps = 1;
	}
	else
	{
		sps = 1000000 / ctx.sensor_loop_delay_us;
	}

	if (sps == 0)
	{
		sps = 1;
	}

	// set sps for all channels
	for (uint8_t i = 0; i < NUM_OF_SENSORS; i++)
	{
		stateest_set_adc_sps(&ctx.sensors[i], sps);
	}

	// The thread is already running at 1 sec delay per sample with no configured channels,
	// so just changing the values here is enough.

	// finally ack it
	send_ack(0);

	ctx.status |= STATUS_SENSORS_ACTIVE;
}


static void handle_raw_value_request(nodeid_t src, void *data, uint8_t len)
{
	uint32_t msglen = len;
	if (check_auth_message(src, data, &msglen) != 0)
	{
		// something is wrong with the auth, can't proceed
		return;
	}

	msg_begin_send_raw_frames_t *rf_msg = (msg_begin_send_raw_frames_t *)data;
	if (msglen != sizeof(*rf_msg))
	{
		// too small
		printf("Received rew value request message with wrong size %lu\n", msglen);
		send_ack(1);
		return;
	}

	// set values in ctx
	ctx.debug_raw_frame_channel = rf_msg->channel;
	memcpy(&ctx.debug_raw_frame_transmission_counter, &rf_msg->num_of_frames, sizeof(ctx.debug_raw_frame_transmission_counter));

	// finally ack it
	send_ack(0);
}


static void handle_nop_request(nodeid_t src, void *data, uint8_t len)
{
	// NOP request -> just check auth and ack it
	
	uint32_t msglen = len;
	if (check_auth_message(src, data, &msglen) != 0)
	{
		// something is wrong with the auth, can't proceed
		return;
	}

	send_ack(0);
}


static void handle_echo_request(nodeid_t src, void *data, uint8_t len)
{
	// Non-authenticated echo request -> just send a non-authenticated reply
	
	printf("Got echo request from %u\n", src);

	msg_echo_reply_t echo_rep_msg;
	echo_rep_msg.type = MSG_TYPE_ECHO_REPLY;

	int res = meshnw_send(src, &echo_rep_msg, sizeof(echo_rep_msg));
	if (res != 0)
	{
		printf("Failed to send echo reply, error: %i\n", res);
	}
}


static void mesh_message_received(nodeid_t id, void *data, uint8_t len)
{
	_Static_assert(sizeof(msg_union_t) + 16 <= MESHNW_MAX_PACKET_SIZE, "Message union is too large");
	if ((ctx.status & STATUS_INIT_CPLT) == 0)
	{
		return;
	}

	msg_union_t *msg = (msg_union_t *)data;

	if (len < sizeof(msg->type))
	{
		puts("received too small message in layer 4\n");
		return;
	}

	printf("Handle message with type %u\n", msg->type);
	switch (msg->type)
	{
		case MSG_TYPE_AUTH_HS_1:
			handle_auth_slave_handshake(id, data, len);
			break;
		case MSG_TYPE_AUTH_HS_2:
			handle_auth_master_handshake(id, data, len);
			break;
		case MSG_TYPE_AUTH_ACK:
			handle_auth_master_ack(id, data, len);
			break;
		case MSG_TYPE_ROUTE_RESET:
		case MSG_TYPE_ROUTE_APPEND:
			handle_route_request(id, data, len);
			break;
		case MSG_TYPE_CONFIGURE_SENSOR_CHANNEL:
			handle_sensor_cfg_request(id, data, len);
			break;
		case MSG_TYPE_START_SENSOR:
			handle_sensor_start_request(id, data, len);
			break;
		case MSG_TYPE_BEGIN_SEND_RAW_FRAMES:
			handle_raw_value_request(id, data, len);
			break;
		case MSG_TYPE_NOP:
			handle_nop_request(id, data, len);
			break;
		case MSG_TYPE_ECHO_REQUEST:
			handle_echo_request(id, data, len);
			break;
		case MSG_TYPE_ECHO_REPLY:
			printf("Got echo reply from %u\n", id);
			break;
		default:
			printf("Received message with unexpected type %u\n", msg->type);
	}
	puts("Message handled");
}


static void *adc_thread(void *arg)
{
	// init adc
	for (uint8_t adc = 0; adc < NUM_OF_SENSORS; adc++)
	{
		adc_init(adc);
	}

	// buffer for sending raw frame values
	uint8_t rf_buffer[MESHNW_MAX_PACKET_SIZE];
	msg_raw_frame_data_t *raw_frame_vals = (msg_raw_frame_data_t*)rf_buffer;
	raw_frame_vals->type = MSG_TYPE_RAW_FRAME_VALUES;
	static const uint8_t VALUES_PER_MESSAGE = (sizeof(rf_buffer) - sizeof(*raw_frame_vals)) / sizeof(raw_frame_vals->values[0]);
	uint8_t raw_values_in_msg = 0;
	

    xtimer_ticks32_t last = xtimer_now();
    while (1)
	{
		for (uint8_t adc = 0; adc < NUM_OF_SENSORS; adc++)
		{
			if ((ctx.active_sensor_channels & (1 << adc)) == 0)
			{
				// this channel is not enabled
				continue;
			}
			uint16_t val = adc_sample(adc, ADC_RES_12BIT);

			state_update_result_t res = stateest_update(&ctx.sensors[adc], val);

			if (res != state_update_unchanged)
			{
				printf("Channel %u on state change %i\n", adc, res);

				// just change the status bits, the message loop will chek for changes and notify the master 
				if (res == state_update_changed_to_on)
				{
					ctx.current_sensor_status |= (1 << adc);
				}
				else
				{
					ctx.current_sensor_status &= ~(1 << adc);
				}
			}

			if (adc == ctx.debug_raw_frame_channel && ctx.debug_raw_frame_transmission_counter > 0)
			{
				if (stateest_get_frame(&ctx.sensors[adc]) != 0xffffffff)
				{
					uint16_t val = (uint16_t) stateest_get_frame(&ctx.sensors[adc]);
					printf("Raw value: %u\n", val);
					memcpy(&raw_frame_vals->values[raw_values_in_msg], &val, sizeof(val));

					raw_values_in_msg++;
					ctx.debug_raw_frame_transmission_counter--;

					if (raw_values_in_msg >= VALUES_PER_MESSAGE || ctx.debug_raw_frame_transmission_counter == 0)
					{
						// send message
						uint8_t len = raw_values_in_msg * sizeof(raw_frame_vals->values[0]) + sizeof(*raw_frame_vals);
						int res = meshnw_send(ctx.master_node,
						                      rf_buffer,
											  len);

						if (res != 0)
						{
							printf("Failed to send raw frame values with error %i\n", res);
						}

						raw_values_in_msg = 0;
					}
				}
			}
		}

        xtimer_periodic_wakeup(&last, ctx.sensor_loop_delay_us);
    }

	return NULL;
}


static void send_status_update_message(uint16_t status)
{
	if ((ctx.status & STATUS_INIT_AUTH_STA) == 0)
	{
		puts("Called send_status_update_message() before status channel has been built!");
		return;
	}

	uint8_t out_buffer[MESHNW_MAX_PACKET_SIZE];
	uint32_t status_msg_len = sizeof(out_buffer);

	msg_status_update_t *sta = (msg_status_update_t *)out_buffer;
	sta->type = MSG_TYPE_STATUS_UPDATE;
	memcpy(&sta->status, &status, sizeof(sta->status));

	struct
	{
		nodeid_t src;
		nodeid_t dst;
	} add_data;
	add_data.src = ctx.current_node;
	add_data.dst = ctx.master_node;

	int res = auth_master_sign(&ctx.auth_status, out_buffer, sizeof(*sta), &status_msg_len, &add_data, sizeof(add_data));

	if (res != 0)
	{
		printf("auth_master_sign failed with error %i\n", res);
		return;
	}

	res = meshnw_send(ctx.master_node, out_buffer, status_msg_len);
	
	if (res != 0)
	{
		printf("sending status update failed with error %i\n", res);
	}
}


static void *message_thread(void *arg)
{
	// The message loop runs once per second.
	static const uint32_t MESSAGE_LOOP_DELAY_US = 1000000;


	/*
	 * The sensor status of the last status update message to the host.
	 * If I sent a message once, i must re-send it until i get an ack for this message.
	 * Only then i may send an other message with a different content.
	 *
	 * When a new status is sent, ctx.last_status_msg_was_acked is reset (to 0)
	 * I must not change this until last_status_msg_was_acked is nonzero again.
	 */
	uint16_t last_sent_sensor_status = 0;


	uint32_t retransmission_counter = 0;
	uint32_t retransmission_timer = 0;

    xtimer_ticks32_t last = xtimer_now();

	while(1)
	{
		/*
		 * There are three possibilities:
		 * I    last_status_msg_was_acked is 0:
		 *      -> There is still a message that was sent but not confirmed
		 *      => Retransmisstion (check / update timer)
		 * II   current_sensor_status != last_sent_sensor_status
		 *      -> There is a new change
		 *      => Sent new status
		 * III  otherwise
		 *      -> There is nothing to do, no unacked message and no status change
		 */

        xtimer_periodic_wakeup(&last, MESSAGE_LOOP_DELAY_US);

		if (retransmission_counter > MAX_STATUS_RETRANSMISSIONS)
		{
			puts("MESH NETWORK IS NOT RESPONDING! Rebooting...");
			pm_reboot();
		}

		if ((ctx.status & STATUS_INIT_AUTH_STA) == 0)
		{
			// Status chanel not built -> check if sensors are active (if so, i'm supposed to build the channel)
		

			if ((ctx.status & STATUS_SENSORS_ACTIVE) != 0)
			{
				// OK, time to build the status channel
				if (retransmission_timer > 0)
				{
					retransmission_timer--;
				}
				else
				{
					init_status_auth();
					// calculate / update rt timer / counter
					retransmission_timer = ((uint32_t)ctx.status_retransmission_base_delay) *
						(1 + retransmission_counter / RETRANSMISSION_LINEAR_BACKOFF_DIVIDER);
					retransmission_timer += CON_RETRANSMISSION_BASE_DELAY;
					retransmission_counter++;
				}
			}

			// can't do more without status channel
			continue;
		}

		// This is set to nonzero, if an update message hould be sent.
		uint8_t send_update_message = 0;

		// just copy this for thread safety
		uint16_t current_status = ctx.current_sensor_status;

		if (!ctx.last_status_msg_was_acked)
		{
			// I -> Check / do retransmission
			
			if (retransmission_timer > 0)
			{
				// Wait for timer
				retransmission_timer--;
			}
			else
			{
				// Timer is 0 -> send now
				send_update_message = 1;
			}
		}
		else if ((current_status & ctx.active_sensor_channels) != (last_sent_sensor_status & ctx.active_sensor_channels))
		{
			// II -> Send new status
			
			// Now i know that last_status_msg_was_acked is nonzero, therefore i may change the last status
			last_sent_sensor_status = current_status;

			// reset acked status and counter
			ctx.last_status_msg_was_acked = 0;
			retransmission_counter = 0;
		}
		// else do nothing
		

		if (send_update_message)
		{
			// need to send the message
			send_status_update_message(last_sent_sensor_status);

			// finally calculate the new retransmission timer
			retransmission_timer = CON_RETRANSMISSION_BASE_DELAY + ((uint32_t)ctx.status_retransmission_base_delay) *
				(1 + retransmission_counter / RETRANSMISSION_LINEAR_BACKOFF_DIVIDER);
			retransmission_counter++;
		}
	}

	puts("exited thread");
	return NULL;
}


int sensor_node_init(void)
{
	memset(&ctx, 0, sizeof(ctx));

	const sensor_configuration_t *cfg = sensor_config_get();
	if (!cfg)
	{
		return SN_ERROR_NOT_CONFIGURED;
	}

	ctx.current_node = cfg->my_id;

	// need to init mesh nw before crypto bcause i need random numbers for xrypto init
	meshnw_init(cfg->my_id, &mesh_message_received);

	uint64_t cha = meshnw_get_random();
	uint64_t nonce = meshnw_get_random();

	printf("Random numbers for auth: cha=%08lx%08lx nonce=%08lx%08lx\n", (uint32_t)(cha >> 32), (uint32_t)cha, (uint32_t)(nonce>>32), (uint32_t)nonce);

	// init crypto context
	auth_master_init(&ctx.auth_status, cfg->key_status, cha);
	auth_slave_init(&ctx.auth_config, cfg->key_config, nonce);

	// start sample loop at 1 per sec
	// at initial state this loop will do nothing, the channels are configured later
	ctx.sensor_loop_delay_us = 1000000;

	// The initial state is 0 for both sides, not this is acked.
	ctx.last_status_msg_was_acked = 1;

	// Start the threads
	
	adc_thd_pid = thread_create(adc_thd_stack, sizeof(adc_thd_stack), THREAD_PRIORITY_MAIN - 1,
	                          THREAD_CREATE_STACKTEST, adc_thread, NULL,
	                          "node_adc_thread");

	if (adc_thd_pid <= KERNEL_PID_UNDEF)
	{
		puts("Creation of adc thread failed");
		return SN_ERROR_THREAD_START;
	}

	message_thd_pid = thread_create(message_thd_stack, sizeof(message_thd_stack), THREAD_PRIORITY_MAIN - 1,
	                          THREAD_CREATE_STACKTEST, message_thread, NULL,
	                          "node_message_thread");

	if (message_thd_pid <= KERNEL_PID_UNDEF)
	{
		puts("Creation of mssage thread failed");
		return SN_ERROR_THREAD_START;
	}
	// state estimation is initialized later
	
	ctx.status |= STATUS_INIT_CPLT;

	return 0;
}

#endif
