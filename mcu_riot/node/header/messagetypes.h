#pragma once

#include <stdint.h>
#include <meshnw.h>
#include <state_estimation.h>

typedef uint8_t msg_type_t;

/*
 * Part 1 of the auth handshake
 * master -> slave
 *
 * When this is sent by the master node, this also contains a temporyry route
 * to the master node
 */
#define MSG_TYPE_AUTH_HS_1                  1
typedef struct
{
	msg_type_t type;
	// The next hop used for the handshake reply
	nodeid_t reply_route;
} __attribute__((packed)) msg_auth_hs_1_t;


/*
 * Part 2 of the auth handshake
 * slave -> master
 */
#define MSG_TYPE_AUTH_HS_2                  2
typedef struct
{
	msg_type_t type;
} __attribute__((packed)) msg_auth_hs_2_t;


/*
 * ACK packet for authenticated packets.
 * Only the slave of a connection sends ACK
 * result_code is a command-defined result value, 
 *   if the msb is set, this is an retransmission-ack
 */
#define MSG_TYPE_AUTH_ACK                   3
typedef struct
{
	msg_type_t type;
	uint8_t result_code;
} __attribute__((packed)) msg_auth_ack_t;


/*
 * Clear all current routes
 * Message can contain new routes to set
 */
#define MSG_TYPE_ROUTE_RESET                4
typedef struct
{
	msg_type_t type;
	// The next hop used for the handshake reply
	struct
	{
		nodeid_t dst;
		nodeid_t next;
	} r[1]; // at least one route, an empty table would make no sense
} __attribute__((packed)) msg_route_data_t;


/*
 * Just like ROUTE_RESET but old routes are kept
 */
#define MSG_TYPE_ROUTE_APPEND               5


/*
 * Configures a sensor channel
 */
#define MSG_TYPE_CONFIGURE_SENSOR_CHANNEL   6
typedef struct
{
	msg_type_t type;
	uint8_t channel_id;
	state_estimation_params_t params;
} __attribute__((packed)) msg_configure_sensor_t;


/*
 * Command to the node to establish the status connection and activate the sensors.
 * The sensor configuration can still be changed after activation.
 *
 * status_retransmission_delay_* are the times (in seconds) before a status message
 * is re-send, if no ack arrived. the actual delay increases linear with the number of retries.
 * For every 3 failed attempts, the current delay is increased by the base value.
 * If no ack arrived after 100 retries, the network is assumed dead and
 * the node shuts down. (Needs to be reconfigured)
 */
#define MSG_TYPE_START_SENSOR               7
typedef struct
{
	msg_type_t type;
	uint16_t active_sensors;
	uint16_t adc_samples_per_sec;
	uint8_t status_retransmission_delay;
} __attribute__((packed)) msg_start_sensor_t;


/*
 * Requests a sensor node to begin transmitting raw frame values.
 * This is intended for remote sensor value calibration
 */
#define MSG_TYPE_BEGIN_SEND_RAW_FRAMES      8
typedef struct
{
	msg_type_t type;
	uint8_t channel;
	uint16_t num_of_frames;
} __attribute__((packed)) msg_begin_send_raw_frames_t;


/*
 * No operation, this is used by the master to check if a node is still alive
 * NOTE: Unlike the echo request, this packet is sent trough the authenticated channel,
 *       so that no one can fake the reply.
 */
#define MSG_TYPE_NOP                        9
typedef struct
{
	msg_type_t type;
} __attribute__((packed)) msg_nop_t;


/*
 * Status update message sent by the node through the status channel to the master.
 */
#define MSG_TYPE_STATUS_UPDATE             10
typedef struct
{
	msg_type_t type;
	uint16_t status;
} __attribute__((packed)) msg_status_update_t;

// above 128 -> not signed

/*
 * Request to send an echo reply
 * NOTE: This is only intended for testing and diagnostics.
 *       To check if a node is still alive, use the NOP request.
 */
#define MSG_TYPE_ECHO_REQUEST             128
typedef struct
{
	msg_type_t type;
} __attribute__((packed)) msg_echo_request_t;

/*
 * Echo reply
 */
#define MSG_TYPE_ECHO_REPLY               129
typedef struct
{
	msg_type_t type;
} __attribute__((packed)) msg_echo_reply_t;

/*
 * Packet containing raw frame values
 */
#define MSG_TYPE_RAW_FRAME_VALUES         130
typedef struct
{
	msg_type_t type;
	uint16_t values[0];
} __attribute__((packed)) msg_raw_frame_data_t;

typedef union
{
	msg_type_t type;
	msg_auth_hs_1_t hs1;
	msg_auth_hs_2_t hs2;
	msg_route_data_t route;
	msg_configure_sensor_t scfg;
	msg_begin_send_raw_frames_t srf;
	msg_echo_request_t echo_rq;
	msg_echo_reply_t echo_rp;
	msg_raw_frame_data_t frames;

} __attribute__((packed)) msg_union_t;
