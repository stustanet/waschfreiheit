/*
 * This is an interface for managing a single master <-> sensor connection.
 */

#include "auth.h"
#include "meshnw.h"

#define SENSOR_CON_MAX_RETRANSMISSIONS 100
#define SENSOR_CON_RETRANSMISSION_LIN_BACKOFF_DIV 3
#define SENSOR_CON_RETRANSMISSION_BASE_DELAY      5

typedef struct
{
	// auth context for receiving status information
	auth_context_t auth_status;

	// auth context for sending config commands
	auth_context_t auth_config;

	uint32_t retransmission_timer;

	// The current status of the node's sensors
	uint16_t current_status;

	// Contents of the last sent message
	uint8_t last_sent_message[MESHNW_MAX_PACKET_SIZE];

	// Length of the last sent message
	uint8_t last_sent_message_len;

	// set to nonzero, if a packet has been sent that has not yet ack'ed
	uint8_t ack_outstanding;

	uint8_t retransmission_counter;

	nodeid_t auth_add_data_cfg[2];
	nodeid_t auth_add_data_sta[2];

	// the node's id
	nodeid_t node_id;
} sensor_connection_t;

// Initializes the connection data structure and starts the connection attempt
// node_reply_hop is the next hop where the node sends the hs2
int sensor_connection_init(sensor_connection_t *con, nodeid_t node, nodeid_t node_reply_hop, nodeid_t master);

// Handles retransmissions, should be called once per second
void sensor_connection_update(sensor_connection_t *con);

// Processes a packet.
void sensor_connection_handle_packet(sensor_connection_t *con, uint8_t *data, uint32_t len);

// Sets the routes on the client. If <reset> is nonzero, a ROUTE_RESET request is sent, otherwise
// it's a ROUTE_APPEND request. <routes> is a null terminated string containing dstination/hop pairs.
// The format is <DST1>:<HOP1>,<DST2>:<HOP2>,...
int sensor_connection_set_routes(sensor_connection_t *con, uint8_t reset, const char *routes);

/*
 * Configures the sensor channel <channel>
 * The parameter format is as follows:
 * input_filter : <adjustment_speed>,<lowpass_weight>,<frame_size>
 * st_matrix    : State transition matrix, 4x4, without the diagonal (12 values total)
 *                The parameter is a comma separated list of the matrix values (row fater row)
 * st_window    : Window sizes in different states, 4 comma seperated values.
 * reject_filter: <reject_threshold>,<reject_min_consec>
 */
int sensor_connection_configure_sensor(sensor_connection_t *con, uint8_t channel, const char *input_filter, const char *st_matrix, const char *st_window, const char *reject_filter);

/*
 * Sets the active sensor channels and samples_per_sec.
 */
int sensor_connection_enable_sensors(sensor_connection_t *con, uint16_t mask, uint16_t samples_per_sec);

/*
 * Sends an authping to the node. This is used to check if the node is still alive.
 */
int sensor_connection_authping(sensor_connection_t *con);

/*
 * Reuests raw data from the sensor.
 * Raw data is printed on the console (Prefix: *** followed by comma seperated values (16bit, hex encoded))
 */
int sensor_connection_get_raw_data(sensor_connection_t *con, uint8_t channel, uint16_t num_frames);

/*
 * Gets the current status.
 * The status should only be read by calling this function and not by reading it directly from the context struct.
 */
uint16_t sensor_connection_get_sensor_status(const sensor_connection_t *con);

/*
 * Gets the node id.
 * The id should only be read by calling this function and not by reading it directly from the context struct.
 */
nodeid_t sensor_connection_node(const sensor_connection_t *con);
