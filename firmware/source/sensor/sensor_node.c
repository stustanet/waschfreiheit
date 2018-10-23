/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#include "sensor_node.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/rcc.h>

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "meshnw.h"
#include "state_estimation.h"
#include "sensor_config.h"
#include "auth.h"
#include "messagetypes.h"
#include "utils.h"
#include "led_ws2801.h"
#include "watchdog.h"
#include "flasher.h"
#include "tinyprintf.h"
#include "sensor_adc.h"
#include "debug_file_logger.h"
#include "led_status.h"

#ifdef WASCHV2

#endif

/*
 * Max time for the watchdog observing the ADC thread.
 * This is a pure software watchdog, but it is guarded by a low-level harware watchdog.
 */
#define ADC_THREAD_WATCHDOG_TIMEOUT 10

/*
 * Number of LEDs connected.
 */
#define NUM_OF_LED 5

// Bits in the status field
// Basic initialization complete, node is now active and can process messages
#define STATUS_INIT_CPLT          0x00000001

/*
 * Route initialization complete, node has valid routes
 * This also indicates a fully completed config handshake.
 */
#define STATUS_INIT_ROUTES        0x00000002

// Status auth initialization complete, node can send status update
#define STATUS_INIT_AUTH_STA      0x00000004

// Config auth initialization complete, node can now receive config messages
#define STATUS_INIT_AUTH_CFG      0x00000008

// Stauts auth init pending, hs1 was sent to the master node, waiting for hs2
#define STATUS_INIT_AUTH_STA_PEND 0x00000010

// Sensors have been started
#define STATUS_SENSORS_ACTIVE     0x000000020

// Serial debugging active (raw data dump)
#define STATUS_SERIALDEBUG        0x000000040

// Print state estimation frame values
#define STATUS_PRINTFRAMES        0x000000080

// LEDs have been set by the user or network command (stop all animations)
#define STATUS_LED_SET            0x000000100

// Send the status even if the master status is the same
#define STATUS_FORCE_UPDATE       0x000000200

// Disable automatic update of the LEDs
// This is set when the 'led' (test-)command is used.
// This is cleared when a LED message arrives.
#define STATUS_NO_LED_UPDATE      0x000000400

// If this bit is set, all sensor configure requests are rejected.
// Also, no status updates are sent.
// While this mode is active, the network timeout is NOT reset, therefore the node will reset itself after the specified timeout.
// This can only be cleared by a restart of the node or by the reset request
#define STATUS_SENSOR_TEST        0x000000800


#ifndef BOARD_WASCH_V1
static void init_channel_test_mode(void);
#endif


/*
 * This struct contains all the runtime-data for the node logic
 */
struct
{
	/*
	 * State estimation data.
	 * This can be huge!
	 */
	state_estimation_data_t sensors[NUM_OF_SENSORS];

	// Auth context for sending status information
	auth_context_t auth_status;

	// Auth context for receiving config commands
	auth_context_t auth_config;

	// Mutex for accessing context values
	SemaphoreHandle_t mutex;
	StaticSemaphore_t mutexBuffer;

	struct
	{
		uint8_t led;
		uint8_t color;
	} status_change_indicators[NUM_OF_SENSORS];

	/*
	 * Color table for the LEDs
	 */
	const color_table_t *led_color_table;

	/*
	 * Various config values.
	 */
	const misc_config_t *misc_config;

	/*
	 * Status information for the node (STATUS_ bits)
	 * Describes the initialization status of the individual components.
	 */
	uint32_t status;

	/*
	 * Delay in us for the adc loop.
	 */
	uint32_t sensor_loop_delay_ms;

	/*
	 * Incremented every second in the mesage thread, reset when an authenticated message is received.
	 * If this value exceeds CONFIG_CHANNEL_TIMEOUT, the CPU is reset.
	 */
	uint32_t config_channel_timeout_timer;

	/*
	 * Current value of the random number generator for backoff times.
	 */
	uint32_t random_current;

	/*
	 * Delay value for the raw mode.
	 * Set this to 0 for normal operation.
	 */
	uint32_t debug_raw_mode_delay_ms;

	/*
	 * Defines which LEDs are blinking.
	 */
	uint32_t led_blink_mask;

	/*
	 * The bits specify the active / enabled sensor channels.
	 * If a channel is active, the adc is sampled and status updates are sent.
	 */
	uint16_t active_sensor_channels;

	/*
	 * The current status of the sensors.
	 * Also as bit-field: A set bit indicates, that the machine connected to this channel is used.
	 */
	uint16_t current_sensor_status;

	/*
	 * Number of raw frames to send.
	 * This is always decremented. When the counter is zero no more raw frames are sent.
	 */
	uint16_t debug_raw_frame_transmission_counter;
	uint8_t debug_raw_frame_channel;

	/*
	 * Set to base_delay when the raw status has been requested by the master.
	 * The status is sent in the message thread. (this is also reset there)
	 * Set this to ~0 to print the status on the serial console.
	 */
	uint8_t debug_raw_status_requested;

	/*
	 * Set to nonzero if a valid ack message was receiced on the status channel,
	 * Reset to 0 again when a new status message is sent.
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
	 * Buffer for the led color values
	 * This is required because the LEDs tend to randomly change their color.
	 * Therefore I store the values here and send them to the LEDs every second.
	 */
	uint8_t led_colors[(NUM_OF_LED - 1) / 2 + 1];

	/*
	 * Watchdog counter for the adc thread.
	 * This counter is incremented in the message thread and reset in the adc thread.
	 * Should this counter ever reach 10, the adc thread has not been run in 10 sec.
	 * In this case, the CPU is reset.
	 */
	volatile uint8_t adc_thread_watchdog;

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

// Stuff for the threads
// Stack size of the adc thread (in words)
#define ADC_THD_STACK_SIZE 256

// Stack size of the message thread (in words)
#define MESSAGE_THD_STACK_SIZE 512

StaticTask_t adc_thd_buffer;;
StackType_t adc_thd_stack[ADC_THD_STACK_SIZE];

StaticTask_t message_thd_buffer;;
StackType_t message_thd_stack[MESSAGE_THD_STACK_SIZE];


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
 * Returns a random number between 0 and max
 * Every time this function is called a new random value is generated.
 */
static uint32_t get_random_delay(uint32_t max)
{
	// make next rand
	ctx.random_current = ctx.random_current * 1103515245 + 12345;
	return ctx.random_current % max;
}

/*
 * Returns the retransmission delay for a packet on the status channel based on the number
 * of retransmissions and a random value.
 * Every time this function is called a new random value is generated.
 */
static uint32_t calculate_retransmission_delay(uint32_t rt_counter)
{
	return get_random_delay(ctx.misc_config->rt_delay_random * (1 + rt_counter / ctx.misc_config->rt_delay_lin_div))
			+ ctx.status_retransmission_base_delay;
}


/*
 * Calculate sps (samples per second) form current loop speed
 */
static uint16_t calculate_adc_sps(void)
{
	uint16_t sps;

	if (ctx.sensor_loop_delay_ms == 0)
	{
		// 0 us delay, this is bad, simply default to 1
		sps = 1;
	}
	else
	{
		sps = 1000 / ctx.sensor_loop_delay_ms;

		if (sps == 0)
		{
			// Also, I don't want a 0 as sps
			sps = 1;
		}
	}
	return sps;
}

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
	ctx.sensor_loop_delay_ms = 1000;

	ctx.status |= STATUS_FORCE_UPDATE;

	led_status_system(LED_STATUS_SYSTEM_CONNECTED);
}


/*
 * -- Config channel --
 * Handles the incoming hs1 from the master to open the config channel.
 * The hs1 is the first auth message a node receives from the master.
 */
static void handle_auth_slave_handshake(nodeid_t src, void *data, uint8_t len)
{
	printf("Dump hs1: ");
	for (uint32_t i = 0; i < len; i++)
	{
		printf("%02x", ((uint8_t *)data)[i]);
	}
	printf("\n");

	// Buffer for the hs2 reply
	uint8_t out_buffer[MESHNW_MAX_PACKET_SIZE];
	uint32_t rep_msg_len = sizeof(out_buffer);

	/*
	 * Both auth handshake messages.
	 * HS1 is the message received from the master,
	 * HS2 is the reply, this is only generated and sent, if the hs1 is valid.
	 */
	msg_auth_hs_1_t *hs1 = (msg_auth_hs_1_t *)data;
	msg_auth_hs_2_t *hs2 = (msg_auth_hs_2_t *)out_buffer;

	/*
	 * Set the message type so that the master will known what to do with this message.
	 * Need to set this before passing the buffer to the auth module because the
	 * generated tag will include the message type.
	 */
	hs2->type = MSG_TYPE_AUTH_HS_2;

	/*
	 * Some basic status information is packed into the handshake reply.
	 * This information will be signed so it should be secure (tm)
	 */

	hs2->status = 0;
	if (ctx.status & STATUS_INIT_ROUTES)
	{
		hs2->status |= MSG_HS_2_STATUS_ROUTES;
	}

	if ((ctx.status & STATUS_SENSORS_ACTIVE) && !(ctx.status & STATUS_SENSOR_TEST))
	{
		hs2->status |= MSG_HS_2_STATUS_SENSOR;
	}

	hs2->channels = ctx.current_sensor_status;

	/*
	 * Give the hs1 to the auth module.
	 * On success the auth module will generate the hs2.
	 * The auth module also does length checking.
	 */
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
	printf("\n");


	/*
	 * Handshake ok. Now i need to send the hs2 to the master to complete his handshake as well.
	 * For this side the handshake itself is already completed. I still can't be sure that I'm actually talking
	 * to the master, this is verified as soon as the first authenticated message is received.
	 * This also implies that I can't trust any data from the hs1 message.
	 */

	if ((ctx.status & STATUS_INIT_ROUTES) == 0)
	{
		/*
		 * As said above, i must not trust the data from the message, so I only accept the
		 * reply route if I didn't get any routes yet.
		 *
		 * Also all previous routes are reset so i can't abuse this to add unverified routes.
		 */
		meshnw_clear_routes();
		meshnw_set_route(src, hs1->reply_route);

		printf("Add temp route %u:%u\n", src, hs1->reply_route);

		// also, store the source address as master address, the route request is expected from the same address.
		ctx.master_node = src;
	}

	// Finally send the hs2 to the master.
	if (!meshnw_send(src, out_buffer, rep_msg_len))
	{
		printf("sending slave handshake reply failed\n");
		return;
	}

	// Auth context for config channel is now init -> Now i can receive authenticated messages.
	ctx.status |= STATUS_INIT_AUTH_CFG;

	led_status_system(LED_STATUS_SYSTEM_CONNECTED);
}


/*
 * -- Status channel --
 * Initiates the buildup of the status channel.
 */
static void init_status_auth(void)
{
	// Need routes for this
	if ((ctx.status & STATUS_INIT_ROUTES) == 0)
	{
		printf("Called init_status_auth() befrore route setup\n");
		return;
	}

	// Buffer for the hs1
	uint8_t out_buffer[MESHNW_MAX_PACKET_SIZE];
	uint32_t rep_msg_len = sizeof(out_buffer);

	msg_auth_hs_1_t *hs1 = (msg_auth_hs_1_t *)out_buffer;

	// Set the message type before giving it to the auth module.
	hs1->type = MSG_TYPE_AUTH_HS_1;

	// No need to tell the master node a route
	hs1->reply_route = MESHNW_INVALID_NODE;

	/*
	 * Request auth module the make the hs1 for status channel..
	 */
	int res = auth_master_make_handshake(&ctx.auth_status, out_buffer, sizeof(*hs1), &rep_msg_len);

	if (res != 0)
	{
		printf("auth_master_make_handshake failed with error %i\n", res);
		return;
	}

	// Send the hs1 to the master.
	if (!meshnw_send(ctx.master_node, out_buffer, rep_msg_len))
	{
		printf("sending master handshake failed\n");
		return;
	}

	/*
	 * New handshake send => Any older handshakes are now invaild
	 * and i wait for the hs2 answer.
	 */
	ctx.status &= ~STATUS_INIT_AUTH_STA;
	ctx.status |= STATUS_INIT_AUTH_STA_PEND;
}


/*
 * -- Status channel --
 * Processes the hs2 response for the status channel.
 */
static void handle_auth_master_handshake(nodeid_t src, void *data, uint8_t len)
{
	if ((ctx.status & STATUS_INIT_ROUTES) == 0)
	{
		printf("Received hs2 before route setup\n");
		return;
	}

	// Only valid if I'm actually waiting for the hs2.
	if ((ctx.status & STATUS_INIT_AUTH_STA_PEND) == 0)
	{
		printf("Received hs2 but handshake is not pending\n");
		return;
	}

	// Only the master may send the hs2.
	if (src != ctx.master_node)
	{
		printf("Got hs2 from a node that is not the current master\n");
		return;
	}

	msg_auth_hs_2_t *hs2 = (msg_auth_hs_2_t *)data;

	// For now the status data from the hs2 is ignored.

	/*
	 * Give hs2 to auth. It will check, if the packet contains the signed challenge.
	 * If so, the master has proven freshness and i can accept this channel as valid.
	 */
	int res = auth_master_process_handshake(&ctx.auth_status, data, sizeof(*hs2), len);
	if (res != 0)
	{
		printf("auth_master_process_handshake failed with error %i\n", res);
		return;
	}

	// No longer pending, valid now.
	ctx.status &= ~STATUS_INIT_AUTH_STA_PEND;
	ctx.status |= STATUS_INIT_AUTH_STA;

	led_status_system(LED_STATUS_SYSTEM_SCH_BUILT);
}


/*
 * -- Config channel --
 * Sends an ACK for a command on the config channel.
 * This function will also set the last_ack_result to
 * the ack_result passed as parameter.
 */
static void send_ack(uint8_t ack_result)
{
	// Need config auth to ack anything
	if ((ctx.status & STATUS_INIT_AUTH_CFG) == 0)
	{
		printf("Called send_ack() but auth is not init\n");
		return;
	}

	// Buffer for the ack message.
	uint8_t out_buffer[MESHNW_MAX_PACKET_SIZE];
	uint32_t rep_msg_len = sizeof(out_buffer);

	msg_auth_ack_t *ack = (msg_auth_ack_t *)out_buffer;

	/*
	 * Set the message type and data before giving it to the auth.
	 * The tag will then include both of these values.
	 */
	ack->type = MSG_TYPE_AUTH_ACK;
	ack->result_code = ack_result;

	/*
	 * Request auth to make an ACK packet with the given payload.
	 */
	int res = auth_slave_make_ack(&ctx.auth_config, out_buffer, sizeof(*ack), &rep_msg_len);

	if (res != 0)
	{
		printf("auth_slave_make_ack failed with error %i\n", res);
		return;
	}

	// Send the ack to the master.
	if (!meshnw_send(ctx.master_node, out_buffer, rep_msg_len))
	{
		printf("sending slave ack reply failed\n");
	}

	// Store ack result for the case i need to do a retransmission.
	ctx.last_ack_result = ack_result;
}

/*
 * -- Status channel --
 * Check an ACK from the master for receiving a status message.
 */
static void handle_auth_master_ack(nodeid_t src, void *data, uint8_t len)
{
	// Need status auth for this
	if ((ctx.status & STATUS_INIT_AUTH_STA) == 0)
	{
		printf("Received ack before status auth init\n");
		return;
	}

	// Must only come from master.
	if (src != ctx.master_node)
	{
		printf("Got ack from a node that is not the current master\n");
		return;
	}

	msg_auth_ack_t *ack = (msg_auth_ack_t *)data;

	/*
	 * Give packet to the auth module to check it.
	 */
	int res = auth_master_check_ack(&ctx.auth_status, data, sizeof(*ack), len);
	if (res != 0)
	{
		printf("auth_master_check_ack failed with error %i\n", res);
		return;
	}

	printf("got ack from master with code %u\n", ack->result_code);

	// Check if I was expecting an ack
	if (ctx.last_status_msg_was_acked)
	{
		printf("unexpected status ack! there is no outstanding packet!\n");
	}
	else
	{
		// The last message has been ack'ed.
		// This means i may send the next status message now.
		ctx.last_status_msg_was_acked = 1;

		led_status_system(LED_STATUS_SYSTEM_SCH_BUILT);
	}
}


/*
 * -- Config channel --
 *  Checks the Auth tag on a config message.
 *  src   Source address
 *  data  Message data
 *  len   On call:    The length of the full message
 *        On success: The length of the data part of the message (message without auth stuff)
 */
static int check_auth_message(nodeid_t src, void *data, uint32_t *len)
{
	// Can't check anything without config auth
	if ((ctx.status & STATUS_INIT_AUTH_CFG) == 0)
	{
		printf("Received authenticated message but cfg auth not initialized\n");
		return -1;
	}

	// Config messages may only come from master node.
	if (src != ctx.master_node)
	{
		printf("Received config message from unexpected source %u", src);
		return -1;
	}


	// I incldue this data as addidional data in the MAC.
	// This data is used for MAC calculation but is not sent over the network.
	struct
	{
		nodeid_t src;
		nodeid_t dst;
	} add_data;
	add_data.src = src;
	add_data.dst = ctx.current_node;

	/*
	 * Check integrity of the packet.
	 */
	int res = auth_slave_verify(&ctx.auth_config, data, len, &add_data, sizeof(add_data));

	if (res == AUTH_OLD_NONCE)
	{
		/*
		 * The nonce in the packet is the nonce of the last packet.
		 * This happens when an ACK has been lost and the master retransmitts the packet.
		 * In this case the last ACK is sent again with the re-ack bit set.
		 */
		printf("Received message with old nonce -> re-ack\n");

		// set re-ack bit and ack
		send_ack(ctx.last_ack_result | 0x80);

		return res;
	}
	else if (res != 0)
	{
		printf("auth_slave_verify failed with error %i\n", res);
		return res;
	}

	if (!(ctx.status & STATUS_SENSOR_TEST))
	{
		/*
		 * Reset the timeout for receiving config messages.
		 */
		ctx.config_channel_timeout_timer = 0;
	}
	return 0;
}


/*
 * -- Config channel --
 * Proceses a route request (set_routes or reset_routes)
 */
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
		/*
		 * Message must at least contain one route
		 */
		printf("Received too small route message with size %lu\n", msglen);

		// still ack it so the master gets the error
		send_ack(1);
		return;
	}

	if (route_msg->type == MSG_TYPE_ROUTE_RESET)
	{
		/*
		 * Reset the node when a ROUTE_RESET command is received
		 */
		reset();
	}

	/*
	 * Load routes from message
	 * The number of routes is only defined by the message length.
	 * NOTE: There are no alignment problems here because the route elements are only 8 bits.
	 */
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


/*
 * -- Config channel --
 * Proceses a sensor configuration request.
 */
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
		// Wrong size
		printf("Received sensor config message with wrong size %lu\n", msglen);

		// Send an ack with code 1 to indicate an error
		send_ack(1);
		return;
	}

	// Check if channel id is in range
	if (cfg_msg->channel_id >= NUM_OF_SENSORS)
	{
		printf("Attempt to configure sensor with invalid index %u\n", cfg_msg->channel_id);

		// Send an ack with code 2 to indicate an error
		send_ack(2);
		return;
	}

	if (ctx.status & STATUS_SENSOR_TEST)
	{
		printf("Rejecting sensor configure request while in SENSOR_TEST mode\n");
		send_ack(3);
		return;
	}

	// Get the current samples per second
	uint16_t sps = calculate_adc_sps();

	/*
	 * Configure the state estimation engine
	 */
	int init_res = stateest_init(&ctx.sensors[cfg_msg->channel_id], &(cfg_msg->params), sps);

	if (init_res != 0)
	{
		printf("Failed to initialize state estimation, error %i\n", init_res);
		send_ack(3);
		return;
	}

	// finally ack it
	send_ack(0);
}


/*
 * -- Config channel --
 * Proceses a sensor start / enable request.
 */
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
		// Wrong size
		printf("Received sensor start message with wrong size %lu\n", msglen);
		send_ack(1);
		return;
	}


	if (ctx.status & STATUS_SENSOR_TEST)
	{
		printf("Rejecting sensor start request while in SENSOR_TEST mode\n");
		send_ack(3);
		return;
	}

	/*
	 * Simply copy the values from the message to my context.
	 */
	ASSERT_ALIGNED(msg_start_sensor_t, active_sensors);
	ASSERT_ALIGNED(msg_start_sensor_t, status_retransmission_delay);
	ctx.active_sensor_channels = start_msg->active_sensors;
	ctx.status_retransmission_base_delay = start_msg->status_retransmission_delay;


	// Calculate delay from samples per second
	if (start_msg->adc_samples_per_sec == 0)
	{
		// 1 sec delay is max
		ctx.sensor_loop_delay_ms = 1000;
	}
	else
	{
		ctx.sensor_loop_delay_ms = 1000 / start_msg->adc_samples_per_sec;
	}

	// Update the sample rate for all channels
	uint16_t sps = calculate_adc_sps();
	for (uint8_t i = 0; i < NUM_OF_SENSORS; i++)
	{
		stateest_set_adc_sps(&ctx.sensors[i], sps);
	}

	/*
	 * The adc thread is always running,
	 * (At 1 sec delay per sample with no configured channels by deafult)
	 * so just changing the delay and the channel config values here is enough.
	 */

	// finally ack it
	send_ack(0);

	// Set sensors active status
	ctx.status |= STATUS_SENSORS_ACTIVE;
}


/*
 * -- Config channel --
 * Proceses a raw data request.
 */
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
		// Wrong size
		printf("Received raw value request message with wrong size %lu\n", msglen);
		send_ack(1);
		return;
	}

	// set values in ctx
	ctx.debug_raw_frame_channel = rf_msg->channel;
	ctx.debug_raw_frame_transmission_counter = u16_from_unaligned(&rf_msg->num_of_frames);

	// finally ack it
	send_ack(0);
}


/*
 * -- Config channel --
 * Proceses a raw status (debug) request.
 */
static void handle_raw_status_request(nodeid_t src, void *data, uint8_t len)
{
	uint32_t msglen = len;
	if (check_auth_message(src, data, &msglen) != 0)
	{
		// something is wrong with the auth, can't proceed
		return;
	}

	// The status message is sent later by the message thread.
	ctx.debug_raw_status_requested = ctx.status_retransmission_base_delay + 1;

	// finally ack it
	send_ack(0);
}


/*
 * -- Config channel --
 * Proceses a nop request (authping).
 */
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


/*
 * -- Config channel --
 * Proceses a LED request.
 */
static void handle_led_request(nodeid_t src, void *data, uint8_t len)
{
	uint32_t msglen = len;
	if (check_auth_message(src, data, &msglen) != 0)
	{
		// something is wrong with the auth, can't proceed
		return;
	}

	msg_led_t *led_msg = (msg_led_t *)data;

	uint8_t color_bytes = msglen - sizeof(*led_msg);

	static const uint32_t MAX_LED_BYTES = sizeof(ctx.led_colors) / sizeof(ctx.led_colors[0]);

	if (color_bytes > MAX_LED_BYTES)
	{
		color_bytes = MAX_LED_BYTES;
	}

	memcpy(ctx.led_colors, led_msg->data, color_bytes);

	// Set undefined color values to 0
	while (color_bytes < MAX_LED_BYTES)
	{
		ctx.led_colors[color_bytes] = 0;
		color_bytes++;
	}

	// Reset LEDs to non-blinking
	ctx.led_blink_mask = 0;

	ctx.status |= STATUS_LED_SET;
	ctx.status &= ~STATUS_NO_LED_UPDATE;

	send_ack(0);
}

/*
 * -- Config channel --
 * Proceses a rebuild status channel request.
 */
static void handle_rebuild_status_channel_request(nodeid_t src, void *data, uint8_t len)
{
	uint32_t msglen = len;
	if (check_auth_message(src, data, &msglen) != 0)
	{
		// something is wrong with the auth, can't proceed
		return;
	}

	send_ack(0);

	// Only thing I need to do is to reset the INIT bit.
	// This causes the auth to be reinitialized on demand.
	ctx.status &= ~STATUS_INIT_AUTH_STA;

	// Just to be sure: Re-send the status
	ctx.status |= STATUS_FORCE_UPDATE;
}


/*
 * -- Config channel --
 * Configure the 'blink on status change' mode for one ore more channels.
 */
static void handle_configure_status_change_indicator_request(nodeid_t src, void *data, uint8_t len)
{
	uint32_t msglen = len;
	if (check_auth_message(src, data, &msglen) != 0)
	{
		// something is wrong with the auth, can't proceed
		return;
	}

	msg_configure_status_change_indicator_t *csci =
		(msg_configure_status_change_indicator_t *)data;

	// Num of channels is defined by the message size
	uint8_t channels = (msglen - sizeof(*csci)) / sizeof(csci->data[0]);

	for (uint8_t i = 0; i < channels; i++)
	{
		uint8_t ch = csci->data[i].channel;
		if (ch >= NUM_OF_SENSORS)
		{
			send_ack(1);
			return;
		}
		if (csci->data[i].led >= NUM_OF_LED)
		{
			send_ack(2);
			return;
		}

		ctx.status_change_indicators[ch].color = csci->data[i].color;
		ctx.status_change_indicators[ch].led = csci->data[i].led;
	}

	send_ack(0);
}


/*
 * -- Generic --
 * Proceses a echo request.
 */
static void handle_echo_request(nodeid_t src, void *data, uint8_t len)
{
	(void) data;
	(void) len;
	// Non-authenticated echo request -> just send a non-authenticated reply

	printf("Got echo request from %u\n", src);

	msg_echo_reply_t echo_rep_msg;
	echo_rep_msg.type = MSG_TYPE_ECHO_REPLY;

	if (!meshnw_send(src, &echo_rep_msg, sizeof(echo_rep_msg)))
	{
		printf("Failed to send echo reply\n");
	}
}


/*
 * Global callback for receiving data from the mesh network.
 */
static void mesh_message_received(nodeid_t id, void *data, uint8_t len)
{
	_Static_assert(sizeof(msg_union_t) + 16 <= MESHNW_MAX_PACKET_SIZE, "Message union is too large");
	if ((ctx.status & STATUS_INIT_CPLT) == 0)
	{
		// Discard any mesage before the basic init is done.
		return;
	}

	/*
	 * Determine the message ttype and pass it to the handler function
	 */
	msg_union_t *msg = (msg_union_t *)data;

	if (len < sizeof(msg->type))
	{
		printf("received too small message in layer 4\n");
		return;
	}

	xSemaphoreTake(ctx.mutex, portMAX_DELAY);
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
		case MSG_TYPE_GET_RAW_STATUS:
			handle_raw_status_request(id, data, len);
			break;
		case MSG_TYPE_NOP:
			handle_nop_request(id, data, len);
			break;
		case MSG_TYPE_LED:
			handle_led_request(id, data, len);
			break;
		case MSG_TYPE_REBUILD_STATUS_CHANNEL:
			handle_rebuild_status_channel_request(id, data, len);
			break;
		case MSG_TYPE_CONFIGURE_STATUS_CHANGE_INDICATOR:
			handle_configure_status_change_indicator_request(id, data, len);
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
	xSemaphoreGive(ctx.mutex);
	printf("Message handled\n");
}


/*
 * Thread function for the ADC thread.
 * This threads samples the sensor channels and updates the state estimation engines.
 * It then updates the sensor status values in the context so that the message thread
 * will know when something has changes and can send a message in this case.
 * It will also send raw data messages if raw data has been requested.
 */
static void adc_thread(void *arg)
{
	(void) arg;

	// Init the channels
	uint16_t dma_buffer[NUM_OF_SENSORS] = {};
	sensor_adc_init_dma(dma_buffer);

	// buffer for sending raw frame values
	uint8_t rf_buffer[MESHNW_MAX_PACKET_SIZE];
	msg_raw_frame_data_t *raw_frame_vals = (msg_raw_frame_data_t*)rf_buffer;
	raw_frame_vals->type = MSG_TYPE_RAW_FRAME_VALUES;
	static const uint8_t VALUES_PER_MESSAGE = (sizeof(rf_buffer) - sizeof(*raw_frame_vals)) / sizeof(raw_frame_vals->values[0]);
	uint8_t raw_values_in_msg = 0;

#ifdef DEBUG_FILE_LOGGER_AVAILABLE
	uint16_t adc_raw_value_buffer[NUM_OF_SENSORS];
	uint16_t adc_filtered_value_buffer[NUM_OF_SENSORS];
#endif

    TickType_t last = xTaskGetTickCount();

	while (1)
	{
		ctx.adc_thread_watchdog = 0;

#ifdef DEBUG_FILE_LOGGER_AVAILABLE
		uint8_t is_frame = 0;
#endif

		// Loop over all channels
		for (uint8_t adc = 0; adc < NUM_OF_SENSORS; adc++)
		{
			if (ctx.debug_raw_mode_delay_ms != 0)
			{
				// raw mode: print data for all channels
				uint16_t val = dma_buffer[adc];
				printf("*%u=%u ", adc, val);
				continue;
			}

			if ((ctx.status & STATUS_SENSORS_ACTIVE) == 0)
			{
				// sensors not configured -> stop here
				break;
			}

			if ((ctx.active_sensor_channels & (1 << adc)) == 0)
			{
				// this channel is not enabled
				continue;
			}

			// Sample the channel
			uint16_t val = dma_buffer[adc];
#ifdef DEBUG_FILE_LOGGER_AVAILABLE
			adc_raw_value_buffer[adc] = val;
#endif

			// Update satte estimation
			state_update_result_t res = stateest_update(&ctx.sensors[adc], val);

			if (res != state_update_unchanged)
			{
				// Status change on this channel.
				printf("Channel %u on state change %i\n", adc, res);
#ifdef DEBUG_FILE_LOGGER_AVAILABLE
				debug_file_logger_log_stateest(adc, stateest_get_current_state(&ctx.sensors[adc]));
#endif
			}

			// just change the status bits, the message loop will check for changes and notify the master
			if (stateest_is_on(&ctx.sensors[adc]))
			{
				ctx.current_sensor_status |= (1 << adc);
			}
			else
			{
				ctx.current_sensor_status &= ~(1 << adc);
			}

			if (stateest_get_frame(&ctx.sensors[adc]) != 0xffffffff)
			{
				// It's a frame
				uint16_t frame = (uint16_t)stateest_get_frame(&ctx.sensors[adc]);
#ifdef DEBUG_FILE_LOGGER_AVAILABLE
				adc_filtered_value_buffer[adc] = frame;
				is_frame = 1;
#endif

				if (ctx.status & STATUS_PRINTFRAMES)
				{
					// Print frame values
					printf("%u: %u\t%u\t%u\n",
					       adc,
					       frame,
					       stateest_get_current_rf_value(&ctx.sensors[adc]),
					       stateest_get_current_state(&ctx.sensors[adc]));
				}
			}

			if (adc == ctx.debug_raw_frame_channel && ctx.debug_raw_frame_transmission_counter > 0)
			{
				// Raw frame values have been requested for this channel

				if (stateest_get_frame(&ctx.sensors[adc]) != 0xffffffff)
				{
					// This is a frame of the input filter
					uint16_t v = (uint16_t) stateest_get_frame(&ctx.sensors[adc]);
					printf("Raw value: %u\n", v);

					u16_to_unaligned(&raw_frame_vals->values[raw_values_in_msg], v);

					raw_values_in_msg++;
					ctx.debug_raw_frame_transmission_counter--;

					if (raw_values_in_msg >= VALUES_PER_MESSAGE || ctx.debug_raw_frame_transmission_counter == 0)
					{
						// Message full or no more data to send => send the message
						uint8_t len = raw_values_in_msg * sizeof(raw_frame_vals->values[0]) + sizeof(*raw_frame_vals);
						if (!meshnw_send(ctx.master_node, rf_buffer, len))
						{
							printf("Failed to send raw frame values.\n");
						}

						raw_values_in_msg = 0;
					}
				}
			}
		}

#ifdef DEBUG_FILE_LOGGER_AVAILABLE
		debug_file_logger_log_raw_adc(adc_raw_value_buffer, NUM_OF_SENSORS);
		if (is_frame)
		{
			debug_file_logger_log_filtered_adc(adc_filtered_value_buffer, NUM_OF_SENSORS);
		}
#endif

		// Wait for next cycle
		if (ctx.debug_raw_mode_delay_ms != 0)
		{
			// Line feed at end of row
			printf("\n");
			// Use raw mode delay
			vTaskDelayUntil(&last, ctx.debug_raw_mode_delay_ms);
		}
		else
		{
			// Use normal delay
			vTaskDelayUntil(&last, ctx.sensor_loop_delay_ms);
		}
	}
}


/*
 * Send a status update message to the master with the specified status.
 */
static void send_status_update_message(uint16_t status)
{
	// Needs status auth
	if ((ctx.status & STATUS_INIT_AUTH_STA) == 0)
	{
		printf("Called send_status_update_message() before status channel has been built!\n");
		return;
	}

	// Buffer for the message
	uint8_t out_buffer[MESHNW_MAX_PACKET_SIZE];
	uint32_t status_msg_len = sizeof(out_buffer);

	msg_status_update_t *sta = (msg_status_update_t *)out_buffer;

	/*
	 * Set the message type and data before calling sign so that the MAC can
	 * incldue this data.
	 */
	sta->type = MSG_TYPE_STATUS_UPDATE;

	u16_to_unaligned(&sta->status, status);

	/*
	 * This data is included in the MAC but is not sent over the network.
	 */
	struct
	{
		nodeid_t src;
		nodeid_t dst;
	} add_data;
	add_data.src = ctx.current_node;
	add_data.dst = ctx.master_node;


	// Sign the packet...
	int res = auth_master_sign(&ctx.auth_status, out_buffer, sizeof(*sta), &status_msg_len, &add_data, sizeof(add_data));

	if (res != 0)
	{
		printf("auth_master_sign failed with error %i\n", res);
		return;
	}

	// ... and send it.
	if (!meshnw_send(ctx.master_node, out_buffer, status_msg_len))
	{
		printf("sending status update failed.\n");
	}
}


/*
 * Sends a message containing the raw status of the node.
 */
static void send_raw_status_message(uint32_t rt_counter, uint32_t uptime)
{
	msg_raw_status_t *rs;

	// Buffer for the message
	uint8_t out_buffer[(sizeof(*rs) + sizeof(rs->channels[0]) * NUM_OF_SENSORS)];
	_Static_assert(MESHNW_MAX_PACKET_SIZE > sizeof(out_buffer), "Too much sensors to fit into one message");

	rs = (msg_raw_status_t *)out_buffer;

	rs->type = MSG_TYPE_RAW_STATUS;

	u32_to_unaligned(&rs->node_status, ctx.status);
	u32_to_unaligned(&rs->sensor_loop_delay, ctx.sensor_loop_delay_ms);
	u32_to_unaligned(&rs->retransmission_counter, rt_counter);
	u32_to_unaligned(&rs->uptime, uptime);
	u16_to_unaligned(&rs->channel_status, ctx.current_sensor_status);
	u16_to_unaligned(&rs->channel_enabled, ctx.active_sensor_channels);
	rs->rt_base_delay = ctx.status_retransmission_base_delay;

	// now append channel data
	for (uint8_t i = 0; i < NUM_OF_SENSORS; i++)
	{
		// I read the values directly from the se context.
		u16_to_unaligned(&rs->channels[i].if_current, ctx.sensors[i].input_filter.current >> 2);
		u16_to_unaligned(&rs->channels[i].rf_current, stateest_get_current_rf_value(&ctx.sensors[i]));
		rs->channels[i].current_status = stateest_get_current_state(&ctx.sensors[i]);
	}

	if (!meshnw_send(ctx.master_node, out_buffer, sizeof(out_buffer)))
	{
		printf("sending raw status failed.\n");
	}
}

/*
 * Prints the current status on the serial console.
 */
static void print_raw_status(uint32_t rt_counter, uint32_t uptime, uint32_t rt_timer, uint16_t master_sensor_status)
{
	printf("Node status\n");
	printf("Node id:               %4u\n", ctx.current_node);
	printf("Master node:           %4u\n", ctx.master_node);
	printf("Status:          0x%08lX\n", ctx.status);
	printf("Channel status:      0x%04X\n", ctx.current_sensor_status);
	printf("Last sent status:    0x%04X\n", master_sensor_status);
	printf("Channel enabled:     0x%04X\n", ctx.active_sensor_channels);
	printf("Retransmissions:   %8lu\n", rt_counter);
	printf("Uptime:            %8lu\n", uptime);
	printf("ADC loop delay:    %8lu\n", ctx.sensor_loop_delay_ms);
	printf("RT base delay:         %4u\n", ctx.status_retransmission_base_delay);
	printf("RT timer:          %8lu\n", rt_timer);
	printf("Last message:      %8lu\n", ctx.config_channel_timeout_timer);

	// now append channel data
	for (uint8_t i = 0; i < NUM_OF_SENSORS; i++)
	{
		// I read the values directly from the se context.
		printf("  Channel %u\n", i);
		printf("    Input:    %5u\n", (uint16_t)(ctx.sensors[i].input_filter.current >> 2));
		printf("    Filtered: %5u\n", stateest_get_current_rf_value(&ctx.sensors[i]) >> 1);
		printf("    Status:   %5u\n", stateest_get_current_state(&ctx.sensors[i]));
	}
}

static void do_led_animation(uint32_t ticks)
{
	// Show init progress with LEDs

	const rgb_data_t WHITE = {.r = 0xff, .g = 0xff, .b = 0xff};
	const rgb_data_t BLACK = {.r = 0x00, .g = 0x00, .b = 0x00};
	const rgb_data_t RED   = {.r = 0xff, .g = 0x00, .b = 0x00};
	const rgb_data_t BLUE  = {.r = 0x00, .g = 0x00, .b = 0xff};

	if ((ctx.status & STATUS_INIT_CPLT) == 0)
	{
		if (ticks > 5)
		{
			if (ticks & 0x01)
			{
				// not yet initialized after 5 sec, this indicates an error during init
				// blink the first led red
				led_status_external(0, RED);
			}
			else
			{
				led_status_external(0, BLACK);
			}
		}
	}
	else if (ticks < 5)
	{
		// Show id as binary

		led_status_external(0, (ctx.current_node & 0x01) ? WHITE : BLACK);
		led_status_external(1, (ctx.current_node & 0x02) ? WHITE : BLACK);
		led_status_external(2, (ctx.current_node & 0x04) ? WHITE : BLACK);
		led_status_external(3, (ctx.current_node & 0x08) ? WHITE : BLACK);
		led_status_external(4, (ctx.current_node & 0x10) ? WHITE : BLACK);
		return;
	}
	else if (ticks & 0x01)
	{
	    // 0 will blink blue
		led_status_external(0, BLUE);
	}
	else
	{
	    // 0 will blink blue
		led_status_external(0, BLACK);
	}

	rgb_data_t color = {};

	// config channel init -> 1 blue
	if (ctx.status & STATUS_INIT_AUTH_CFG)
	{
		color.b = 160;
	}

	// routes init -> 1 +red
	if (ctx.status & STATUS_INIT_ROUTES)
	{
		color.r = 255;
	}

	led_status_external(1, color);
	color.r = color.b = 0;

	// status handshake pending -> 2 blue
	if (ctx.status & STATUS_INIT_AUTH_STA_PEND)
	{
		color.b = 160;
	}

	// status handshake done -> 2 blue+red
	if (ctx.status & STATUS_INIT_AUTH_STA)
	{
		color.b = 160;
		color.r = 255;
	}

	led_status_external(2, color);
	color.r = color.b = 0;

	// sensors active -> 3 blue
	if (ctx.status & STATUS_SENSORS_ACTIVE)
	{
		color.b = 160;
	}

	led_status_external(3, color);
	color.r = color.b = 0;

	// serial debug -> 4 blue
	if (ctx.status & STATUS_SERIALDEBUG)
	{
		color.b = 160;
	}

	led_status_external(4, color);
}


static void update_leds(uint32_t ticks)
{
	if (ctx.status & STATUS_NO_LED_UPDATE)
	{
		return;
	}

	_Static_assert(sizeof(*ctx.led_color_table) / sizeof((*ctx.led_color_table)[0]) == 16, "Wrong ColorMap size");

	for (uint8_t i = 0; i < NUM_OF_LED; i++)
	{
		uint8_t color;
		if (((ticks & 0x01) != 0) && (ctx.led_blink_mask & (1 << i)) != 0)
		{
			// Odd tick count and this LED should blink
			// -> Set it to black
			color = 0;
		}
		else if (i & 1)
		{
			// odd -> use second nibble
			color = ctx.led_colors[i >> 1] & 0x0f;
		}
		else
		{
			// even -> use first nibble
			color = ctx.led_colors[i >> 1] >> 4;
		}

		led_status_external(i, (*ctx.led_color_table)[color]);
	}
}


static void check_status_change_indicators(uint16_t changed)
{
	for (uint8_t ch = 0; ch < NUM_OF_SENSORS; ch++)
	{
		if (changed & (1 << ch))
		{
			// Status change on channel <ch>
			if (ctx.status_change_indicators[ch].led != 0xff)
			{
				// We have configured a status change indicator for this channel -> set color and start blinking
				uint8_t led = ctx.status_change_indicators[ch].led;
				if (led & 0x01)
				{
					// Odd -> color is defined in the lower nibble
					ctx.led_colors[led >> 1] =
						(ctx.led_colors[led >> 1] & 0xF0) |
						ctx.status_change_indicators[ch].color;
				}
				else
				{
					// Even -> color is defined in upper nibble
					ctx.led_colors[led >> 1] =
						(ctx.led_colors[led >> 1] & 0x0F) |
						(ctx.status_change_indicators[ch].color << 4);
				}

				ctx.led_blink_mask |= (1 << led);
			}
		}
	}
}


/*
 * This is the message thread.
 * It handles the status channel and the timeouts, this includes:
 *   - Buidling of the status channel
 *   - Sending status updates when a change is detected
 *   - Resetting the MCU if the retransmission limit or the config timeout is reached.
 */
static void message_thread(void *arg)
{
	(void) arg;
	// The message loop runs once per second.
	static const uint32_t MESSAGE_LOOP_DELAY_MS = 1000;


	/*
	 * The sensor status of the last status update message to the host.
	 * If I sent a message once, i must re-send it until i get an ack for this message.
	 * Only then i may send an other message with a different content.
	 *
	 * When a new status is sent, ctx.last_status_msg_was_acked is reset (to 0)
	 * I must not change this until last_status_msg_was_acked is nonzero again.
	 */
	uint16_t last_sent_sensor_status = 0;

	/*
	 * This counts the number of status message retransmissions.
	 * This is used for the timeout and for calculating the time before a retransmission.
	 */
	uint32_t retransmission_counter = 0;

	/*
	 * Current time to wait before a retransmission.
	 */
	uint32_t retransmission_timer = 0;

	uint32_t total_retransmissions = 0;
	uint32_t total_ticks = 0;


    TickType_t last = xTaskGetTickCount();


	while(1)
	{
		WATCHDOG_FEED();
		vTaskDelayUntil(&last, MESSAGE_LOOP_DELAY_MS);
		total_ticks++;

		/*
		 * Increment timeout timer for config messages.
		 */
		ctx.config_channel_timeout_timer++;

		if (retransmission_counter > ctx.misc_config->max_status_retransmissions ||
			ctx.config_channel_timeout_timer > ctx.misc_config->network_timeout)
		{
			printf("NETWORK TIMEOUT! Rebooting...\n");
			while(1); // Waait for the wdt reset
		}

		ctx.adc_thread_watchdog++;
		if (ctx.adc_thread_watchdog > ADC_THREAD_WATCHDOG_TIMEOUT)
		{
			printf("ADC THREAD DIED! Rebooting...\n");
			while(1); // Waait for the wdt reset
		}

		if ((ctx.status & STATUS_LED_SET) == 0)
		{
			// LEDs not yet set -> do some animations
			do_led_animation(total_ticks);
		}
		else
		{
			// Set LEDs to defined colors
			update_leds(total_ticks);
		}

		led_status_channels(ctx.active_sensor_channels, ctx.current_sensor_status);

		if (ctx.debug_raw_status_requested)
		{
			if (ctx.debug_raw_status_requested == 255)
			{
				print_raw_status(total_retransmissions, total_ticks, retransmission_timer, last_sent_sensor_status);
				ctx.debug_raw_status_requested = 0;
			}
			else
			{
				ctx.debug_raw_status_requested--;

				if (ctx.debug_raw_status_requested == 0)
				{
					send_raw_status_message(total_retransmissions, total_ticks);
				}
			}
		}


		if (ctx.status & STATUS_SENSOR_TEST)
		{
			// No status updates if in test mode
			continue;
		}

		xSemaphoreTake(ctx.mutex, portMAX_DELAY);

		if ((ctx.status & STATUS_INIT_AUTH_STA) == 0)
		{
			// Status channel not built -> check if sensors are active (if so, i'm supposed to build the channel)

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
					retransmission_timer = calculate_retransmission_delay(retransmission_counter);
					retransmission_counter++;
					total_retransmissions++;
				}
			}

			xSemaphoreGive(ctx.mutex);

			// can't do more without status channel
			continue;
		}

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


		// This is set to nonzero, if an update message should be sent.
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
				total_retransmissions++;
			}
		}
		else if ((current_status & ctx.active_sensor_channels) != (last_sent_sensor_status & ctx.active_sensor_channels) ||
		          (ctx.status & STATUS_FORCE_UPDATE) != 0)
		{
			// II -> Send new status

			// Check, if we need to set some LEDs to blinking
			check_status_change_indicators(last_sent_sensor_status ^ current_status);

			// Now i know that last_status_msg_was_acked is nonzero, therefore i may change the last status
			last_sent_sensor_status = current_status;

			// reset acked status and counter
			ctx.last_status_msg_was_acked = 0;
			retransmission_counter = 0;

			// Send message
			send_update_message = 1;

			ctx.status &= ~STATUS_FORCE_UPDATE;
			led_status_system(LED_STATUS_SYSTEM_SCHNG_PEND);
		}
		// else do nothing


		if (send_update_message)
		{
			// need to send the message
			send_status_update_message(last_sent_sensor_status);

			// finally calculate the new retransmission timer
			retransmission_timer = calculate_retransmission_delay(retransmission_counter);
			retransmission_counter++;
			printf("Status message sent, rt_t=%lu, rt_c=%lu\n", retransmission_timer, retransmission_counter);
		}

		xSemaphoreGive(ctx.mutex);

	}
}


/*
 * This init functiuon should be called from the main.
 * It initializes all components in sensor mode and starts the threads.
 */
int sensor_node_init(void)
{
	// Begin with only zeros
	memset(&ctx, 0, sizeof(ctx));

	ctx.led_color_table = sensor_config_color_table();
	ctx.misc_config = sensor_config_misc_settings();
	for (uint8_t ch = 0; ch < NUM_OF_SENSORS; ch++)
	{
		ctx.status_change_indicators[ch].led = 0xff;
	}

	led_status_init();
	led_status_system(LED_STATUS_SYSTEM_INIT);

	// This resets the LEDs
	do_led_animation(0);

	// Start sample loop at 1 per sec.
	// At initial state this loop will do nothing, because the channels are configured later.
	ctx.sensor_loop_delay_ms = 1000;

	// The initial state is 0 for both sides -> Don't expect an ack for this.
	ctx.last_status_msg_was_acked = 1;

	// Start the threads

	ctx.mutex = xSemaphoreCreateMutexStatic(&ctx.mutexBuffer);

	/*
	 * Need to run this thread with maximum priority due to bug in xtimer_periodic_wakeup
	 */

	xTaskCreateStatic(
		&adc_thread,
		"ADC",
	    ADC_THD_STACK_SIZE,
		NULL,
		tskIDLE_PRIORITY + 4,
		adc_thd_stack,
		&adc_thd_buffer);

	xTaskCreateStatic(
		&message_thread,
		"MESSAGE",
	    MESSAGE_THD_STACK_SIZE,
		NULL,
		tskIDLE_PRIORITY + 3,
		message_thd_stack,
		&message_thd_buffer);

	// Init the network
	const sensor_configuration_t *cfg = sensor_config_get();
	if (!cfg)
	{
		printf("Network not configured, init aborted!\n");
		led_status_system(LED_STATUS_SYSTEM_ERROR);
		return SN_ERROR_NOT_CONFIGURED;
	}

	printf("Starting SENSOR node, id = %u\n", cfg->my_id);

	ctx.current_node = cfg->my_id;

	// Need to init RF network before crypto because i need random numbers for crypto init
	if (!meshnw_init(cfg->my_id, sensor_config_rf_settings(), &mesh_message_received))
	{
		led_status_system(LED_STATUS_SYSTEM_ERROR);
		return SN_ERROR_MESHNW_INIT;
	}

	// Random numbers for crypto
	uint64_t cha = meshnw_get_random();
	uint64_t nonce = meshnw_get_random();

	printf("Random numbers for auth: cha=%08lx%08lx nonce=%08lx%08lx\n", (uint32_t)(cha >> 32), (uint32_t)cha, (uint32_t)(nonce>>32), (uint32_t)nonce);

	// Init crypto context
	auth_master_init(&ctx.auth_status, cfg->key_status, cha);
	auth_slave_init(&ctx.auth_config, cfg->key_config, nonce);

	// Initialize random value with (part of) nonce;
	ctx.random_current = (uint32_t)nonce;

	// state estimation is initialized later

	ctx.status |= STATUS_INIT_CPLT;

	return 0;
}


void sensor_node_cmd_raw(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("USAGE: raw <delay>\n"
			   "       Enables / disables raw value printing on the serial terminal.\n"
			   "       Raw values for all channels are printed every <delay> us.\n"
			   "       Set <delay> to zero to disbale raw mode.\n"
			   "       While the node is in raw mode all status updates are disabled.\n"
			   "       NOTE: If the delay is too small the data can't be processed fast\n"
			   "             enough and the serial terminal gets stuck.");
		return;
	}

	ctx.debug_raw_mode_delay_ms = strtoul(argv[1], NULL, 10);
	if (ctx.debug_raw_mode_delay_ms != 0)
	{
		ctx.status |= STATUS_SERIALDEBUG;
		printf("Enter raw mode with delay: %lu\n", ctx.debug_raw_mode_delay_ms);
	}
	else
	{
		ctx.status &= ~STATUS_SERIALDEBUG;
		printf("Raw mode disabled\n");
	}
	return;
}


void sensor_node_cmd_led(int argc, char **argv)
{
	if (argc < 2 || argc > 1 + NUM_OF_LED)
	{
		printf("USAGE: led <r,g,b> ...\n"
			   "       Set the LED RGB colors");
		return;
	}

	rgb_data_t tmp;
	for (int i = 0; i < argc - 1; i++)
	{
		char *end;
		tmp.r = strtoul(argv[i + 1], &end, 10);
		if (!end[0])
		{
			printf("Invalid RGB!\n");
			return;
		}
		tmp.g = strtoul(end + 1, &end, 10);
		if (!end[0])
		{
			printf("Invalid RGB!\n");
			return;
		}
		tmp.b = strtoul(end + 1, NULL, 10);

		led_status_external(i, tmp);
	}

	ctx.status |= STATUS_LED_SET;
	ctx.status |= STATUS_NO_LED_UPDATE;
	return;
}


void sensor_node_cmd_print_frames(int argc, char **argv)
{
	if (argc != 2)
	{
		printf("USAGE: print_frames TRUE|FALSE\n"
			   "Enables / Disables printing of the sensor state.\n"
			   "If enbaled, the state estimation state is printed every frame.");
		return;
	}

	if (strcasecmp(argv[1], "true") == 0 ||
	    strcmp(argv[1], "1") == 0)
	{
		ctx.status |= STATUS_PRINTFRAMES;
	}
	else
	{
		ctx.status &= ~STATUS_PRINTFRAMES;
	}
	return;
}


void sensor_node_cmd_print_status(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	printf("Requested status data\n");
	ctx.debug_raw_status_requested = ~0;
	return;
}


#ifndef WASCHV2
void sensor_node_cmd_firmware_upgrade(int argc, char **argv)
{
	if (argc != 3 || strcmp(argv[1], "--start") != 0)
	{
		printf("USAGE: firmware_upgrade --start <baudrate>\n"
			   "Flash the MCU.\n"
			   "This command is NOT intended for interactive use. (Use flash util)\n"
			   "This way of flashing is UNSAFE! Only it if you know how to flash the controller with a STLink or through the serial bootloader!");
		return;
	}

	uint32_t br = strtoul(argv[2], NULL, 10);

	flasher_start(br);
	return;
}

#else


void sensor_node_cmd_channel_test(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	init_channel_test_mode();
}


/*
 * Configures and enables all channels with a test config
 */
static void init_channel_test_mode(void)
{
	static const state_estimation_params_t TEST_PARAMS =
	{
		.input_filter =
		{
			.mid_value_adjustment_speed = 65535,
			.lowpass_weight = 10,
			.num_samples = 250
		},
		.state_filter =
		{
			.transition_matrix = {0, 1100, 0, 0, 0, 0, -900, 0, 0, 0, 0, 0}, // in 0 above 1100 to 2; in 2 below 900 to 0
			.window_sizes = {3, 3, 3, 3},
			.reject_threshold = 0,
			.reject_consec_count = 0
		}
	};


	printf("STARTING CHANNEL TEST MODE\n");

	for (uint8_t i = 0; i < NUM_OF_SENSORS; i++)
	{
		stateest_init(&ctx.sensors[i], &TEST_PARAMS, 500);
		ctx.active_sensor_channels |= 1 << i;
	}
	ctx.sensor_loop_delay_ms = 1000 / 500;

	xSemaphoreTake(ctx.mutex, portMAX_DELAY);
	ctx.status |= STATUS_SENSOR_TEST | STATUS_SENSORS_ACTIVE;
	xSemaphoreGive(ctx.mutex);
}

#endif
