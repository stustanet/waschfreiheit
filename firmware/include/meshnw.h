/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


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
#include "sx127x.h"

typedef uint8_t nodeid_t;

// Wieder ein define, weil Array-Größe
#define MESHNW_MAX_NODEID 254
#define MESHNW_MAX_PACKET_SIZE 64

static const nodeid_t MESHNW_INVALID_NODE = MESHNW_MAX_NODEID + 1;

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
 * config     Network configuration
 *            NOTE: The modem driver may store this pointer internally, so it must point to a persistent memory location.
 * cb         Receive callback handler
 * returns true on success
 * NOTE: At initial states there are no routes so all send calls will fail (route is unknown)
 */
bool meshnw_init(nodeid_t id, const sx127x_rf_config_t *config, mesh_nw_message_cb_t cb);

/*
 * Enables the message forwarding. (Disabled after init)
 * This should be enabled as soon as the final routes are set.
 */
void meshnw_enable_forwarding(void);

/*
 * Sets a route.
 * Packets with the <destination> will be forwarded to <next_hop>
 * returns true on success
 */
bool meshnw_set_route(nodeid_t destination, nodeid_t next_hop);

/*
 * Clears all routes
 */
void meshnw_clear_routes(void);

/*
 * Sends a packet to the specified destination.
 * len is the packet length in bytes, this must not exceed MESHNW_MAX_PACKET_SIZE
 * returns true on success
 */
bool meshnw_send(nodeid_t dst, void *data, uint8_t len);


/*
 * This function has nothing to do with the mesh network itself, but is uses the
 * tranceiver to generate a "good" random number.
 */
uint64_t meshnw_get_random(void);
