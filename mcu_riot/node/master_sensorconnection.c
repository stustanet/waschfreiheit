#ifdef MASTER

#include "master_sensorconnection.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "master_config.h"
#include "meshnw.h"
#include "messagetypes.h"
#include "utils.h"


/*
 * -- Config channel --
 * Sends the packet in the last packet buffer to the node and resets the timeout
 */
static int send_last_packet(sensor_connection_t *con)
{
	int res = meshnw_send(con->node_id, con->last_sent_message, con->last_sent_message_len);

	if (res != 0)
	{
		printf("Failed to send message to node %u with error %i\n", con->node_id, res);
	}

	con->timeout_counter= 0;

	return res;
}


/*
 * -- Status channel --
 * Handles incoming hs1 requests from the sensor node.
 * This is the initial packet on the status channel, sent by the sensor as soon as the the sensor is activated.
 */
static void handle_hs1(sensor_connection_t *con, uint8_t *message, uint8_t len)
{
	// Need to use my own buffer here to not interfere with the config channels retransmission buffer
	uint8_t rep_buffer[sizeof(msg_auth_hs_2_t) + 24];
	uint32_t rep_len = sizeof(rep_buffer);

	msg_auth_hs_2_t *rep_hs = (msg_auth_hs_2_t*)rep_buffer;
	rep_hs->type = MSG_TYPE_AUTH_HS_2;

	// Process hs1 form sensor to generate hs2 from it.
	int res = auth_slave_handshake(&con->auth_status, message, sizeof(msg_auth_hs_1_t), len, rep_buffer, sizeof(*rep_hs), &rep_len);
	if (res != 0)
	{
		printf("Failed to process slave hs1 form node %u. Error: %i\n", con->node_id, res);
		return;
	}

	// send hs2 directly (not through the retransmission buffer)
	
	res = meshnw_send(con->node_id, rep_buffer, rep_len);

	if (res != 0)
	{
		printf("Failed to send hs2 message to node %u with error %i\n", con->node_id, res);
	}
}


/*
 * -- Config channel --
 * Handles incoming hs2 messages.
 * A hs2 is sent by the sensor as soon as it receives the hs1 from the master.
 */
static void handle_hs2(sensor_connection_t *con, uint8_t *message, uint8_t len)
{
	if (!con->ack_outstanding)
	{
		// The hs2 is treated like an ack, so ack_outstanding must be set.
		// Otherwise this means we got a hs2 without asking for it.
		puts("Received unexpected hs2");
		return;
	}


	// Process hs2 from sensor to finalize config channel buildup
	int res = auth_master_process_handshake(&con->auth_config, message, sizeof(msg_auth_hs_2_t), len);

	if (res != 0)
	{
		printf("Failed to process hs2 from %u, error: %i\n", con->node_id, res);
		return;
	}

	// Handsahke OK => Channel built!
	con->ack_outstanding = 0;
	printf("###ACK%u-0\n", con->node_id);
	printf("Auth handshake complete for %u\n", con->node_id);
}


/*
 * -- Config channel --
 * Handles acks from the sensor.
 * An ack confirms that the last command was received by the sensor.
 */
static void handle_ack(sensor_connection_t *con, uint8_t *message, uint8_t len)
{
	if (!con->ack_outstanding)
	{
		// Was not waiting for an ACK
		puts("Received unexpected ack");
		return;
	}

	msg_auth_ack_t *ack = (msg_auth_ack_t *)message;

	// Give the ACK to the auth module.
	// If it accepts the ack, it will update its internal state.
	int res = auth_master_check_ack(&con->auth_config, message, sizeof(*ack), len);

	if (res != 0)
	{
		printf("Invalid ACK from %u, error: %i\n", con->node_id, res);
		return;
	}

	// ACK ok -> notify the interface
	con->ack_outstanding = 0;
	printf("###ACK%u-%u\n", con->node_id, ack->result_code);
}


/*
 * -- Status channel --
 * This function generates and sends an ACK packet on the status channel.
 * This is used to notify the sensor, that the status update has been received.
 */
static void ack_status_message(sensor_connection_t *con)
{
	// Need to use my own buffer here to not interfere with the config channels retransmission buffer
	uint8_t ack_buffer[sizeof(msg_auth_ack_t) + 16];
	uint32_t ack_len = sizeof(ack_buffer);

	msg_auth_ack_t *ack = (msg_auth_ack_t*)ack_buffer;
	ack->type = MSG_TYPE_AUTH_ACK;


	// Can't fail on this side so the ack code is always 0
	ack->result_code = 0;

	// Request ack packet from auth...
	int res = auth_slave_make_ack(&con->auth_status, ack_buffer, sizeof(*ack), &ack_len);
	if (res != 0)
	{
		printf("Failed to make ack for node %u. Error: %i\n", con->node_id, res);
		return;
	}

	// ... and send it directly.
	res = meshnw_send(con->node_id, ack_buffer, ack_len);

	if (res != 0)
	{
		printf("Failed to send ack to node %u with error %i\n", con->node_id, res);
	}
}


/*
 * -- Status channel --
 * Handles the status update message sent from the sensor through the status channel
 * and notifies the interface about status changes.
 */
static void handle_status_update(sensor_connection_t *con, uint8_t *message, uint8_t len)
{
	uint32_t contents_len = len;

	// Check the MAC
	int res = auth_slave_verify(&con->auth_status, message, &contents_len, con->auth_add_data_sta, sizeof(con->auth_add_data_sta));

	if (res != 0)
	{
		// Something is wrong

		if (res == AUTH_OLD_NONCE)
		{
			// Received an old MAC -> This is a retransmission
			// => Res-send the ack.
			printf("Re-send status ack for: %u\n", con->node_id);
			ack_status_message(con);
			return;
		}

		// Something else went wrong, can't do much about it.
		printf("Status message auth fail: %i (node: %u)\n", res, con->node_id);
		return;
	}

	msg_status_update_t *su = (msg_status_update_t *)message;
	if (contents_len != sizeof(*su))
	{
		printf("Received status message with wrong length: %lu\n", contents_len);
		return;
	}

	// copy status to my local variable and ack the message

	con->current_status = u16_from_unaligned(&su->status);

	// Notify interface about status change
	printf("###STATUS%u-%u\n", con->node_id, con->current_status);

	// Send ACK
	ack_status_message(con);
	return;
}


/*
 * -- Un-authenticated data --
 *  Processes raw sensor data sent be the sensor.
 */
static void handle_raw_frames(sensor_connection_t *con, uint8_t *message, uint8_t len)
{
	msg_raw_frame_data_t *raw = (msg_raw_frame_data_t *)message;

	// Number of values is defined by the message size
	uint8_t count = (len - sizeof(*raw)) / sizeof(raw->values[0]);

	if (count == 0)
	{
		return;
	}

	printf("RAW%u-%u\n", con->node_id, count);
	for (uint8_t i = 0; i < count; i++)
	{
		// Need to be carefull about the alignment
		printf("*%u\n", u16_from_unaligned(&raw->values[i]));
	}
}


/*
 * -- Un-authenticated data --
 *  Processes raw status data sent be the sensor.
 */
static void handle_raw_status(sensor_connection_t *con, uint8_t *message, uint8_t len)
{
	msg_raw_status_t *raw = (msg_raw_status_t *)message;

	// Number of channels is defined by the message size
	uint8_t channels = (len - sizeof(*raw)) / sizeof(raw->channels[0]);

	// print global data
	printf("Raw status data for node %u\n", con->node_id);
	printf("  Node status:      %08lX\n", u32_from_unaligned(&raw->node_status));
	printf("  Channel status:       %04X\n", u16_from_unaligned(&raw->channel_status));
	printf("  Status at master:     %04X\n", con->current_status);
	printf("  Channel enabled:      %04X\n", u16_from_unaligned(&raw->channel_enabled));
	printf("  Retransmissions:  %8lu\n", u32_from_unaligned(&raw->retransmission_counter));
	printf("  Uptime:           %8lu\n", u32_from_unaligned(&raw->uptime));
	printf("  ADC loop delay:   %8lu\n", u32_from_unaligned(&raw->sensor_loop_delay));
	printf("  RT delay:         %8u\n", raw->rt_base_delay);

	for (uint8_t i = 0; i < channels; i++)
	{
		printf("  Channel %u\n", i);
		printf("    Input:    %5u\n", u16_from_unaligned(&raw->channels[i].if_current));
		// Print this scaled to 15 bit to avoid confusion (state values are also 15 bit)
		printf("    Filtered: %5u\n", u16_from_unaligned(&raw->channels[i].rf_current) >> 1);
		printf("    Status:   %5u\n", raw->channels[i].current_status);
	}
}


/*
 * Parses a signed or unsigned list of 16 bit numbers.
 * One of both out parameters can be NULL.
 * The result arrays need to be 2 byte aligned.
 */
static int parse_int16_list(const char *str, int16_t *out_signed, uint16_t *out_unsigned, uint32_t count)
{
	uint32_t expected = count;

	while(count)
	{
		const char *next;

		// Always interpret the number as signed 32 bit first
		long v = strtol(str, (char **)&next, 10);

		if (next == str)
		{
			// Start == end => No char consumed
			printf("Unexpected char in number: %i(%c)\n", str[0], str[0]);
			return 1;
		}

		if (out_signed)
		{
			// signed values must be between int16 min and max
			if (v > INT16_MAX || v < INT16_MIN)
			{
				printf("Signed value out of range: %ld\n", v);
				return 1;
			}
			out_signed[0] = (int16_t)v;
			out_signed++;
		}

		if (out_unsigned)
		{
			// unsigned -> must be >= 0 and below uint16_max
			if (v > UINT16_MAX || v < 0)
			{
				printf("Unsigned value out of range: %ld\n", v);
				return 1;
			}
			out_unsigned[0] = (uint16_t)v;
			out_unsigned++;
		}

		count--;
		str = next;

		// end of string
		if (str[0] == 0)
		{
			break;
		}

		if (str[0] != ',')
		{
			// no ',' after number
			printf("Unexpected delimiter in number sequence expected \',\', is: %i(%c)\n", str[0], str[0]);
			return 1;
		}
		str++;

	}

	if (str[0] || count)
	{
		// expect end of string and count==0
		printf("Wrong parameter length, expected %lu\n", expected);
		return 1;
	}
	return 0;
}


/*
 * Signs and sends the message in the transmission buffer.
 * A message sent this was must be ack'ed.
 */
static int sign_and_send_msg(sensor_connection_t *con, uint32_t len)
{
	uint32_t packet_len = sizeof(con->last_sent_message);
	int res = auth_master_sign(&con->auth_config, con->last_sent_message, len, &packet_len, con->auth_add_data_cfg, sizeof(con->auth_add_data_cfg));

	if (res != 0)
	{
		printf("Failed to sign route request for node %u with error %i\n", con->node_id, res);
		return res;
	}

	con->last_sent_message_len = packet_len;
	con->ack_outstanding = 1;

	send_last_packet(con);
	return 0;
}


int sensor_connection_init(sensor_connection_t *con, nodeid_t node, nodeid_t node_reply_hop, nodeid_t master, uint8_t timeout)
{
	// the message buffer needs to be 16 bit aligned,
	// this way i can directly access 16 bit values within this buffer (as long as they are aligned)
	_Static_assert((offsetof(sensor_connection_t, last_sent_message) % sizeof(uint16_t) == 0), "Wrong alignment of last sent message buffer");

	// initialize data in con
	memset(con, 0, sizeof(*con));

	// get new random numbers
	// need to check if this makes any problems as this turns of the rf module
	// for a short period of time
	uint64_t rand_nonce = meshnw_get_random();
	uint64_t rand_cha = meshnw_get_random();

	printf("Initializing node %u. nonce=%08lx%08lx cha=%08lx%08lx\n", node, (uint32_t)(rand_nonce >> 32), (uint32_t)rand_nonce, (uint32_t)(rand_cha >> 32), (uint32_t)rand_cha);

	const node_auth_keys_t *keys = master_config_get_keys(node);
	if (keys == NULL)
	{
		printf("Failed to get keys for node %u\n", node);
		return -EINVAL;
	}

	con->timeout = timeout;

	auth_master_init(&con->auth_config, keys->key_config, rand_cha);
	auth_slave_init(&con->auth_status, keys->key_status, rand_nonce);
	con->node_id = node;

	// The add data contains source and destination (in this order)
	con->auth_add_data_cfg[0] = master;
	con->auth_add_data_cfg[1] = node;

	con->auth_add_data_sta[0] = node;
	con->auth_add_data_sta[1] = master;

	// Now send the hs1, the data is written directly into the last message buffer
	
	uint32_t msg_len = sizeof(con->last_sent_message);

	msg_auth_hs_1_t *hs1 = (msg_auth_hs_1_t *)con->last_sent_message;

	hs1->type = MSG_TYPE_AUTH_HS_1;

	// Tell the node it's next hop to send the handshake to
	hs1->reply_route = node_reply_hop;

	int res = auth_master_make_handshake(&con->auth_config, con->last_sent_message, sizeof(*hs1), &msg_len);

	if (res != 0)
	{
		printf("auth_master_make_handshake failed with error %i\n", res);
		return 1;
	}

	con->last_sent_message_len = msg_len;
	con->ack_outstanding = 1;

	send_last_packet(con);
	return 0;
}


void sensor_connection_handle_packet(sensor_connection_t *con, uint8_t *data, uint32_t len)
{
	if (len == 0)
	{
		return;
	}

	/*
	 * Call the handler function for the message type
	 */
	msg_union_t *msg = (msg_union_t *)data;
	switch(msg->type)
	{
		case MSG_TYPE_AUTH_HS_1:
			handle_hs1(con, data, len);
			return;

		case MSG_TYPE_AUTH_HS_2:
			handle_hs2(con, data, len);
			return;

		case MSG_TYPE_AUTH_ACK:
			handle_ack(con, data, len);
			break;

		case MSG_TYPE_STATUS_UPDATE:
			handle_status_update(con, data, len);
			break;

		case MSG_TYPE_RAW_FRAME_VALUES:
			handle_raw_frames(con, data, len);
			break;

		case MSG_TYPE_RAW_STATUS:
			handle_raw_status(con, data, len);
			break;
		default:
			printf("Got message with unexpected code %u from %u\n", msg->type, con->node_id);
	}
}


void sensor_connection_update(sensor_connection_t *con)
{
	if (!con->ack_outstanding)
	{
		// no ack outstanding -> nothing to do
		return;
	}

	if (con->timeout_counter == 0xff)
	{
		// already timeouted
		return;
	}

	// check the timeout counter
	if (con->timeout_counter < con->timeout)
	{
		// Not yet reached limit -> Increment it
		con->timeout_counter++;
		return;
	}

	// Set timeout counter to 0xff to mark that i already sent the TIMEOUT notification
	con->timeout_counter = 0xff;
	printf("###TIMEOUT%u\n", con->node_id);

}


int sensor_connection_retransmit(sensor_connection_t *con)
{
	if (!con->ack_outstanding || con->timeout_counter <= con->timeout)
	{
		// should not be called in this case
		puts("Illegal call to sensor_connection_retransmit!");
		return 1;
	}

	// re-send the message, this also resets the timer
	printf("Do retransmission to node %u\n", con->node_id);
	send_last_packet(con);
	return 0;
}


int sensor_connection_set_routes(sensor_connection_t *con, uint8_t reset, const char *routes)
{
	/*
	 * Size of route packet ( + auth header) must not exceed max packet length.
	 * If this is too large, either the static assert or the suth will fail.
	 */
	static const uint8_t MAX_ROUTES = 20;
	if (con->ack_outstanding)
	{
		printf("Can't send route request, channel still busy for node %u\n", con->node_id);
		return -EBUSY;
	}

	msg_route_data_t *routemsg = (msg_route_data_t *)con->last_sent_message;
	_Static_assert(MAX_ROUTES <= (sizeof(con->last_sent_message) - sizeof(*routemsg)) / sizeof(routemsg->r[0]) + 1, "Route packet too long");

	if (reset)
	{
		routemsg->type = MSG_TYPE_ROUTE_RESET;
	}
	else
	{
		routemsg->type = MSG_TYPE_ROUTE_APPEND;
	}

	uint8_t current = 0;

	// parse the routes
	while(routes[0] != 0)
	{
		if (current >= MAX_ROUTES)
		{
			printf("To many routes in route command, max number of routes: %u\n", MAX_ROUTES);
			return 1;
		}

		if (utils_parse_route(&routes, &routemsg->r[current].dst, &routemsg->r[current].next) != 0)
		{
			return 1;
		}

		if (routes[0] != 0 && routes[0] != ',')
		{
			printf("Unexpected route delim: %i(%c)\n", routes[0], routes[0]);
			return 1;
		}

		if (routes[0] == ',')
		{
			routes++;
		}

		current++;
	}

	if (current == 0)
	{
		puts("No routes specified!");
		return 1;
	}

	// now sign the packet
	uint32_t len = sizeof(*routemsg) + (current - 1) * sizeof(routemsg->r[0]);
	int res = sign_and_send_msg(con, len);

	if (res != 0)
	{
		printf("Failed to sign route request for node %u with error %i\n", con->node_id, res);
		return 1;
	}

	return 0;
}


int sensor_connection_configure_sensor(sensor_connection_t *con, uint8_t channel, const char *input_filter, const char *st_matrix, const char *st_window, const char *reject_filter)
{
	_Static_assert(sizeof(msg_configure_sensor_t) < sizeof(con->last_sent_message), "Config message size exceeds size limt");

	if (con->ack_outstanding)
	{
		printf("Can't send config request, channel still busy for node %u\n", con->node_id);
		return -EBUSY;
	}

	// i can do this (and still access the struct normally)
	// because there are only 16 bit values in here and the above ensures 16 bit alignment.
	msg_configure_sensor_t *msg = (msg_configure_sensor_t *)con->last_sent_message;

	msg->type = MSG_TYPE_CONFIGURE_SENSOR_CHANNEL;
	msg->channel_id = channel;

	uint16_t tmp[3];
	if (parse_int16_list(input_filter, NULL, tmp, 3) != 0)
	{
		printf("Invalid input filter parameters!");
		return 1;
	}

	// Check alignment to ensure that i can access the variables without causing a hardfault
	ASSERT_ALIGNED(msg_configure_sensor_t, params.input_filter.mid_value_adjustment_speed);
	ASSERT_ALIGNED(msg_configure_sensor_t, params.input_filter.lowpass_weight);
	ASSERT_ALIGNED(msg_configure_sensor_t, params.input_filter.num_samples);
	
	// Input filter
	msg->params.input_filter.mid_value_adjustment_speed = tmp[0];
	msg->params.input_filter.lowpass_weight = tmp[1];
	msg->params.input_filter.num_samples = tmp[2];


	// State matrix
	ASSERT_ALIGNED(msg_configure_sensor_t, params.state_filter.transition_matrix[0]);
	if (parse_int16_list(st_matrix,
		msg->params.state_filter.transition_matrix,
		NULL,
		ARRAYSIZE(msg->params.state_filter.transition_matrix)) != 0)
	{
		printf("Invalid state transition parameters!");
		return 1;
	}

	// Window sizes
	ASSERT_ALIGNED(msg_configure_sensor_t, params.state_filter.window_sizes[0]);
	if (parse_int16_list(st_window,
		NULL,
		msg->params.state_filter.window_sizes,
		ARRAYSIZE(msg->params.state_filter.window_sizes)) != 0)
	{
		printf("Invalid window size parameters!");
		return 1;
	}

	// Reject filter
	if (parse_int16_list(reject_filter, NULL, tmp, 2) != 0)
	{
		printf("Invalid reject filter parameters!");
		return 1;
	}
	ASSERT_ALIGNED(msg_configure_sensor_t, params.state_filter.reject_threshold);
	ASSERT_ALIGNED(msg_configure_sensor_t, params.state_filter.reject_consec_count);

	msg->params.state_filter.reject_threshold = tmp[0];
	msg->params.state_filter.reject_consec_count = tmp[1];

	// Finally all params are parsed -> time to sign it and bring it on the way

	int res = sign_and_send_msg(con, sizeof(*msg));

	if (res != 0)
	{
		printf("Failed to sign sensor config request for node %u with error %i\n", con->node_id, res);
		return 1;
	}

	return 0;
}


int sensor_connection_enable_sensors(sensor_connection_t *con, uint16_t mask, uint16_t samples_per_sec)
{
	if (con->ack_outstanding)
	{
		printf("Can't enable sensor request, channel still busy for node %u\n", con->node_id);
		return -EBUSY;
	}

	msg_start_sensor_t *startmsg = (msg_start_sensor_t *)con->last_sent_message;

	startmsg->type = MSG_TYPE_START_SENSOR;
	startmsg->status_retransmission_delay = con->timeout;

	ASSERT_ALIGNED(msg_start_sensor_t, active_sensors)
	ASSERT_ALIGNED(msg_start_sensor_t, adc_samples_per_sec)

	startmsg->active_sensors = mask;
	startmsg->adc_samples_per_sec = samples_per_sec;
	

	// sign and send
	int res = sign_and_send_msg(con, sizeof(*startmsg));

	if (res != 0)
	{
		printf("Failed to sign sensor start request for node %u with error %i\n", con->node_id, res);
		return 1;
	}

	return 0;
}


int sensor_connection_authping(sensor_connection_t *con)
{
	if (con->ack_outstanding)
	{
		printf("Can't send authping, channel still busy for node %u\n", con->node_id);
		return -EBUSY;
	}

	msg_nop_t *nopmsg = (msg_nop_t *)con->last_sent_message;

	nopmsg->type = MSG_TYPE_NOP;

	// sign and send
	int res = sign_and_send_msg(con, sizeof(*nopmsg));

	if (res != 0)
	{
		printf("Failed to sign nop request for node %u with error %i\n", con->node_id, res);
		return 1;
	}

	return 0;
}


int sensor_connection_led(sensor_connection_t *con, int num_leds, char **leds)
{
	if (con->ack_outstanding)
	{
		printf("Can't send led request, channel still busy for node %u\n", con->node_id);
		return -EBUSY;
	}


	msg_led_t *ledmsg = (msg_led_t *)con->last_sent_message;

	uint8_t bytes = (num_leds - 1) / 2 + 1;
	if (bytes + sizeof(*ledmsg) > sizeof(con->last_sent_message))
	{
		printf("Too many leds in LED request\n");
		return -EINVAL;
	}

	ledmsg->type = MSG_TYPE_LED;

	memset(ledmsg->data, 0, bytes);

	for (int i = 0; i < num_leds; i++)
	{
		int v = atoi(leds[i]);

		printf("Color of LED %i is %i\n", i, v);

		if (i & 1)
		{
			// odd -> use second nibble
			ledmsg->data[i >> 1] |= v & 0x0f;
		}
		else
		{
			// even -> use first nibble
			ledmsg->data[i >> 1] |= (v & 0x0f) << 4;
		}

		printf("Value source for %i: %u\n", i, ledmsg->data[i >> 1]);
	}

	puts("Dump led request");
	for (uint32_t i = 0; i < sizeof(*ledmsg) + bytes; i++)
	{
		printf("%02x", con->last_sent_message[i]);
	}
	puts("");

	// sign and send
	int res = sign_and_send_msg(con, sizeof(*ledmsg) + bytes);

	if (res != 0)
	{
		printf("Failed to sign led request for node %u with error %i\n", con->node_id, res);
		return 1;
	}

	return 0;
}


int sensor_connection_get_raw_data(sensor_connection_t *con, uint8_t channel, uint16_t num_frames)
{
	if (con->ack_outstanding)
	{
		printf("Can't send raw data request, channel still busy for node %u\n", con->node_id);
		return -EBUSY;
	}

	msg_begin_send_raw_frames_t *rfmsg = (msg_begin_send_raw_frames_t *)con->last_sent_message;

	rfmsg->type = MSG_TYPE_BEGIN_SEND_RAW_FRAMES;
	rfmsg->channel = channel;

	ASSERT_ALIGNED(msg_begin_send_raw_frames_t, num_of_frames);
	rfmsg->num_of_frames = num_frames;

	// sign and send
	int res = sign_and_send_msg(con, sizeof(*rfmsg));

	if (res != 0)
	{
		printf("Failed to sign raw frames request for node %u with error %i\n", con->node_id, res);
		return 1;
	}

	return 0;
}


int sensor_connection_get_raw_status(sensor_connection_t *con)
{
	if (con->ack_outstanding)
	{
		printf("Can't send raw status request, channel still busy for node %u\n", con->node_id);
		return -EBUSY;
	}

	msg_get_raw_status_t *rsmsg = (msg_get_raw_status_t *)con->last_sent_message;

	rsmsg->type = MSG_TYPE_GET_RAW_STATUS;

	// sign and send
	int res = sign_and_send_msg(con, sizeof(*rsmsg));

	if (res != 0)
	{
		printf("Failed to sign raw status request for node %u with error %i\n", con->node_id, res);
		return 1;
	}

	return 0;
}


uint16_t sensor_connection_get_sensor_status(const sensor_connection_t *con)
{
	return con->current_status;
}


nodeid_t sensor_connection_node(const sensor_connection_t *con)
{
	return con->node_id;
}
#endif
