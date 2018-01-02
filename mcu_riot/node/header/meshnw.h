/*
 * Mesh network stuff
 *
 * To keep things simple, only one instance of the mesh network is allowed.
 * The interface is reduced to a minimum, if data with a matching node id is received, the message callback will be called.
 *
 * The callback function will be called from the internal receiving thread in this module.
 * This is the implementation of the protocol layer 3.
 */

#pragma once

#include <stdint.h>

typedef uint8_t nodeid_t;

// Wieder ein define, weil Array-Größe
#define MESHNW_MAX_NODEID 254
#define MESHNW_MAX_PACKET_SIZE 64

static const nodeid_t MESHNW_INVALID_NODE = MESHNW_MAX_NODEID + 1;

typedef struct
{
	// MAYBE: Add other LoRa parameters?
	uint32_t frequency;
	uint8_t tx_power;
} meshnw_rf_config_t;

/*
 * Callback for incoming data
 * src        Address of the sender
 * data       Payload
 * len        Length of the payload
 */
typedef void (*mesh_nw_message_cb_t)(nodeid_t src, void *data, uint8_t len);

/*
 * Initilizes the mesh network handler.
 * id         Address of the current node
 * cb         Receive callback handler
 * returns 0 on success
 * NOTE: At initial states there are no routes so all send calls will fail (route is unknown)
 */
int meshnw_init(nodeid_t id, const meshnw_rf_config_t *config, mesh_nw_message_cb_t cb);

/*
 * Enables the message forwarding. (Disabled after init)
 * This should be enabled as soon as the final routes are set.
 */
void meshnw_enable_forwarding(void);

/*
 * Sets a route.
 * Packets with the <destination> will be forwarded to <next_hop>
 * returns 0 on success
 */
int meshnw_set_route(nodeid_t destination, nodeid_t next_hop);

/*
 * Clears all routes
 */
void meshnw_clear_routes(void);

/*
 * Sends a packet to the specified destination.
 * len is the packet length in bytes, this must not exceed MESHNW_MAX_PACKET_SIZE
 * if the radio is busy crrently, this fails with -EBUSY
 * returns 0 on success
 */
int meshnw_send(nodeid_t dst, void *data, uint8_t len);


/*
 * This function has nothing to do with the mesh network itself, but is uses the
 * tranceiver to generate a "good" random number.
 */
uint64_t meshnw_get_random(void);
