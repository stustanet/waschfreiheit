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
 * Sends the packet in the last packet buffer to the node and sets / updates the retransmission data
 */
static int send_last_packet(sensor_connection_t *con)
{
	int res = meshnw_send(con->node_id, con->last_sent_message, con->last_sent_message_len);

	if (res != 0)
	{
		printf("Failed to send message to node %u with error %i\n", con->node_id, res);
	}

	con->retransmission_timer = con->node_id * (1 + (con->retransmission_counter / SENSOR_CON_RETRANSMISSION_LIN_BACKOFF_DIV));
	con->retransmission_timer += SENSOR_CON_RETRANSMISSION_BASE_DELAY;
	con->retransmission_counter++;

	return res;
}


static void handle_hs1(sensor_connection_t *con, uint8_t *message, uint8_t len)
{

	uint8_t rep_buffer[sizeof(msg_auth_hs_2_t) + 24];
	uint32_t rep_len = sizeof(rep_buffer);

	msg_auth_hs_2_t *rep_hs = (msg_auth_hs_2_t*)rep_buffer;
	rep_hs->type = MSG_TYPE_AUTH_HS_2;

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


static void handle_hs2(sensor_connection_t *con, uint8_t *message, uint8_t len)
{
	if (!con->ack_outstanding)
	{
		puts("Received unexpected hs2\n");
		return;
	}


	int res = auth_master_process_handshake(&con->auth_config, message, sizeof(msg_auth_hs_2_t), len);

	if (res != 0)
	{
		printf("Failed to process hs2 from %u, error: %i\n", con->node_id, res);
		return;
	}

	con->ack_outstanding = 0;
	printf("###ACK%u-0\n", con->node_id);
	printf("Auth handshake complete for %u\n", con->node_id);
}


static void handle_ack(sensor_connection_t *con, uint8_t *message, uint8_t len)
{
	if (!con->ack_outstanding)
	{
		puts("Received unexpected ack\n");
		return;
	}

	msg_auth_ack_t *ack = (msg_auth_ack_t *)message;

	int res = auth_master_check_ack(&con->auth_config, message, sizeof(*ack), len);

	if (res != 0)
	{
		printf("Invalid ACK from %u, error: %i\n", con->node_id, res);
		return;
	}

	con->ack_outstanding = 0;
	printf("###ACK%u-%u\n", con->node_id, ack->result_code);
}


static void ack_status_message(sensor_connection_t *con)
{
	uint8_t ack_buffer[sizeof(msg_auth_ack_t) + 16];
	uint32_t ack_len = sizeof(ack_buffer);

	msg_auth_ack_t *ack = (msg_auth_ack_t*)ack_buffer;
	ack->type = MSG_TYPE_AUTH_ACK;

	// can't fail on this side
	ack->result_code = 0;

	int res = auth_slave_make_ack(&con->auth_status, ack_buffer, sizeof(*ack), &ack_len);
	if (res != 0)
	{
		printf("Failed to make ack for node %u. Error: %i\n", con->node_id, res);
		return;
	}

	// send directly
	res = meshnw_send(con->node_id, ack_buffer, ack_len);

	if (res != 0)
	{
		printf("Failed to send ack to node %u with error %i\n", con->node_id, res);
	}
}


static void handle_status_update(sensor_connection_t *con, uint8_t *message, uint8_t len)
{
	uint32_t contents_len = len;

	int res = auth_slave_verify(&con->auth_status, message, &contents_len, con->auth_add_data_sta, sizeof(con->auth_add_data_sta));

	if (res != 0)
	{
		if (res == AUTH_OLD_NONCE)
		{
			ack_status_message(con);
		}
		return;
	}

	msg_status_update_t *su = (msg_status_update_t *)message;
	if (contents_len != sizeof(*su))
	{
		printf("Received status message with wrong length: %lu\n", contents_len);
		return;
	}

	// copy status to my local variable and ack the message
	
	memcpy(&con->current_status, &su->status, sizeof(con->current_status));

	printf("###STATUS%u-%u\n", con->node_id, con->current_status);

	ack_status_message(con);
	return;
}


static void handle_raw_frames(sensor_connection_t *con, uint8_t *message, uint8_t len)
{
	msg_raw_frame_data_t *raw = (msg_raw_frame_data_t *)message;
	uint8_t count = (len - sizeof(*raw)) / sizeof(raw->values[0]);

	if (count == 0)
	{
		return;
	}

	printf("RAW%u-%u\n", con->node_id, count);
	for (uint8_t i = 1; i < count; i++)
	{
		uint16_t tmp;
		memcpy(&tmp, &raw->values[i], sizeof(tmp));
		printf("*%u\n", tmp);
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
		long v = strtol(str, (char **)&next, 10);

		if (next == str)
		{
			printf("Unexpected char in number: %i(%c)\n", str[0], str[0]);
		}

		if (out_signed)
		{
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

		if (str[0] == 0)
		{
			break;
		}

		if (str[0] != ',')
		{
			printf("Unexpected delimiter in number sequence expected \',\', is: %i(%c)\n", str[0], str[0]);
			return 1;
		}
		str++;

	}

	if (str[0] || count)
	{
		printf("Wrong parameter length, expected %lu\n", expected);
		return 1;
	}
	return 0;
}


/*
 * Signs and sends the message in the transmission buffer
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


int sensor_connection_init(sensor_connection_t *con, nodeid_t node, nodeid_t node_reply_hop, nodeid_t master)
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

	auth_master_init(&con->auth_config, keys->key_config, rand_cha);
	auth_slave_init(&con->auth_status, keys->key_status, rand_nonce);
	con->node_id = node;

	con->auth_add_data_cfg[0] = master;
	con->auth_add_data_cfg[1] = node;

	con->auth_add_data_sta[0] = node;
	con->auth_add_data_sta[1] = master;

	// now send the hs1, the data is written directly into the last message buffer
	
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

	// check the retransmission timer
	if (con->retransmission_timer > 0)
	{
		// > 0 => Decrement it
		con->retransmission_timer--;
		return;
	}

	if (con->retransmission_counter > SENSOR_CON_MAX_RETRANSMISSIONS)
	{
		printf("###TIMEOUT%u\n", con->node_id);
		con->ack_outstanding = 0;
		return;
	}

	// Timer is 0 -> re-send the message
	printf("Do retransmission to node %u, num: %u\n", con->node_id, con->retransmission_counter);
	send_last_packet(con);
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
		puts("No routes specified!\n");
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

	ASSERT_ALIGNED(msg_configure_sensor_t, params.input_filter.mid_value_adjustment_speed);
	ASSERT_ALIGNED(msg_configure_sensor_t, params.input_filter.lowpass_weight);
	ASSERT_ALIGNED(msg_configure_sensor_t, params.input_filter.num_samples);
	
	msg->params.input_filter.mid_value_adjustment_speed = tmp[0];
	msg->params.input_filter.lowpass_weight = tmp[1];
	msg->params.input_filter.num_samples = tmp[2];


	ASSERT_ALIGNED(msg_configure_sensor_t, params.state_filter.transition_matrix[0]);
	if (parse_int16_list(st_matrix,
		msg->params.state_filter.transition_matrix,
		NULL,
		ARRAYSIZE(msg->params.state_filter.transition_matrix)) != 0)
	{
		printf("Invalid state transition parameters!");
		return 1;
	}

	ASSERT_ALIGNED(msg_configure_sensor_t, params.state_filter.window_sizes[0]);
	if (parse_int16_list(st_window,
		NULL,
		msg->params.state_filter.window_sizes,
		ARRAYSIZE(msg->params.state_filter.window_sizes)) != 0)
	{
		printf("Invalid window size parameters!");
		return 1;
	}


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
	startmsg->status_retransmission_delay = con->node_id + 1;

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


uint16_t sensor_connection_get_sensor_status(const sensor_connection_t *con)
{
	return con->current_status;
}


nodeid_t sensor_connection_node(const sensor_connection_t *con)
{
	return con->node_id;
}
#endif
