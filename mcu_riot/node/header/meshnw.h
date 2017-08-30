/*
 * Mesh network stuff
 *
 * To keep things simple, only one instance of the mesh network is allowed.
 * The interface is reduced to a minimum, if data with a matching node id is received, the message callback will be called.
 *
 * The callback function will be called from the internal receiving thread in this module.
 */

#include <stdint.h>

#define MESHNW_MAX_PACKET_SIZE 32

typedef  uint16_t nodeid_t;
typedef void (*mesh_nw_message_cb_t)(void *data, uint8_t len);

int meshnw_init(nodeid_t id, mesh_nw_message_cb_t cb);

int meshnw_send(nodeid_t dst, void *data, uint8_t len);
