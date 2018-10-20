/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#include "meshnw.h"
#include "sx127x.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "tinyprintf.h"

#define MESHNW_MSG_QUEUE   (16U)
#define NETDEV_ISR_EVENT_MESSAGE   (0x3456)


// Stack size of the receiving thread (in words)
#define RECV_THD_STACK_SIZE 512

/*
 * The structure definition of a layer 3 packet.
 * The lower layers are handled by the RF hardware, the higher layers by the callbacks.
 */
typedef struct
{
	/*
	 * Address of the next hop for this packet.
	 * This node will discard any packet that is NOT adressed to it. (even if the destination address matches)
	 */
	nodeid_t next_hop;

	/*
	 * Destination address of this packet.
	 * If a packet arrives (with correct next_hop), the destination is checked.
	 * If the destination is this node, the callback is called.
	 * Otherwise the packet is send again with the next hop as specified in the routing table.
	 */
	nodeid_t dst;

	/*
	 * The (original) source address of the packet.
	 */
	nodeid_t src;
} layer3_packet_header_t;


/*
 * The max OTA packet size (Max size of packet to send)
 * is the maximum packet size plus the header size.
 */
#define MESHNW_MAX_OTA_PACKET_SIZE (MESHNW_MAX_PACKET_SIZE + sizeof(layer3_packet_header_t))

/*
 * This struct bundles all globals to make it a bit less ugly.
 */
typedef struct
{
	// Stuff for receiving thread.
	StaticTask_t recv_thd_buffer;;
	StackType_t recv_thd_stack[RECV_THD_STACK_SIZE];

	// Mutex used for all operations on the driver
	SemaphoreHandle_t mutex;
	StaticSemaphore_t mutexBuffer;

	// This callback is called when a message is received.
	mesh_nw_message_cb_t recv_callback;

	// The id of this node
	nodeid_t my_node_id;

	// Routing table
	// This table contains an entry for every address.
	// A packet for n is sent to the next hop <routing_table[n]>.
	uint8_t routing_table[MESHNW_MAX_NODEID + 1];

	// If this is zero, incoming packets with destination addres unqual to my address are dropped.
	// Otherwise they are forwarded  according to the routing table.
	uint8_t enable_forwarding;
} meshnw_context_data_t;

static meshnw_context_data_t context = {};

static void hexdump(uint8_t *data, uint32_t cnt)
{
	for (uint32_t i = 0; i < cnt; i++)
	{
		printf("%02X ", data[i]);
		if (i % 16 == 15)
		{
			printf("\n");
		}
		else if (i % 4 == 3)
		{
			printf(" ");
		}
	}
	printf("\n");
}

/*
 * Gets the route to the specified destination
 * returns the next hop or MESHNW_INVALID_NODE if there is no route
 */
static nodeid_t get_route(nodeid_t destination)
{
	if (destination > MESHNW_MAX_NODEID)
	{
		return MESHNW_INVALID_NODE;
	}

	return context.routing_table[destination];
}

/*
 * Sends/Forwards a packet to the next hop specified in the routing table
 */
static bool forward_packet(void *packet, uint8_t len)
{
	layer3_packet_header_t *hdr = (layer3_packet_header_t *)packet;

	// Get the next route and set it as next_hop
	hdr->next_hop = get_route(hdr->dst);

	if (hdr->next_hop > MESHNW_MAX_NODEID)
	{
		// no route found -> drop packet
		printf("Can't forward packet, no route to %u!\n", hdr->dst);
		return false;
	}

	xSemaphoreTake(context.mutex, portMAX_DELAY);

	printf("Send packet\n");
	hexdump(packet, len);

	// Send packet, next_hop specifies the receiver
	bool ret = sx127x_send(packet, len);

	xSemaphoreGive(context.mutex);

	return ret;
}


/*
 * Should be called when a packet has been received.
 * Reads the packect and decices what to do with it:
 * I      Size is invalid (too short/long)
 *        => Discard
 * II     Next hop is not my id
 *        => Discard
 * III    Destination id is my id
 *        => Call receive callback
 * IV     Destination id is NOT my id
 *        => Forward packet as defined in routing table
 */
static void handle_rx_cplt(uint8_t *packet, uint8_t len)
{
	// Check if length is in bounds
	if (len < (int)sizeof(layer3_packet_header_t) + 1)
	{
		// I (invalid) => discard
		printf("Discard packet with invalid size %i.\n", len);
		return;
	}

	// Check the packet header
	layer3_packet_header_t *hdr = (layer3_packet_header_t *)packet;
	uint8_t rssi;
	int8_t snr;
	sx127x_get_last_pkt_stats(&rssi, &snr);

	printf("Received packet from %u for %u (%d bytes), RSSI: %i, SNR: %i/4 dB\n",
		   hdr->src, hdr->dst, (int)len,
		   rssi, snr);

	hexdump(packet, len);

	if (hdr->next_hop != context.my_node_id)
	{
		// II (not for me) => discard
		printf("Discard packet with next_hop %i\n", hdr->next_hop);
		return;
	}

	if (hdr->dst == context.my_node_id)
	{
		// III (data for the current node) => call callback
		uint8_t *pld = packet + sizeof(layer3_packet_header_t);
		uint8_t pld_len = len - sizeof(layer3_packet_header_t);
		(*context.recv_callback)(hdr->src, pld, pld_len);
	}
	else if(context.enable_forwarding)
	{
		// IV (need to forward) -> forward
		int res = forward_packet(packet, len);
		if (res != 0)
		{
			printf("Failed to forward packet, error %i.\n", res);
		}
	}
}


/*
 * The receiving thread.
 * This thread polls new packtes from the modem
 */
static void *recv_thread(void *arg)
{
	(void)arg;

	// Buffer for receiving packets
	uint8_t recv_buffer[MESHNW_MAX_OTA_PACKET_SIZE];

	while (1)
	{
		vTaskDelay(30); // 30ms @ 1kHz tick rate

		xSemaphoreTake(context.mutex, portMAX_DELAY);
		uint8_t len = sx127x_recv(recv_buffer, sizeof(recv_buffer));
		xSemaphoreGive(context.mutex);

		if (len == 0)
		{
			continue;
		}

		if (len > sizeof(recv_buffer))
		{
			printf("Discard overlong packet with size: %u\n", len);
			continue;
		}

		// call the recv handler
		handle_rx_cplt(recv_buffer, len);
	}
}


/*
 * Initializes the LoRa driver and internal data.
 */
bool meshnw_init(nodeid_t id, const sx127x_rf_config_t *config, mesh_nw_message_cb_t cb)
{
	context.mutex = xSemaphoreCreateMutexStatic(&context.mutexBuffer);


	xTaskCreateStatic(
		&recv_thread,
		"MESHNW_RECV",
		RECV_THD_STACK_SIZE,
		NULL,
		tskIDLE_PRIORITY + 1,
		context.recv_thd_stack,
		&context.recv_thd_buffer);


	context.my_node_id = id;
	context.recv_callback = cb;
	context.enable_forwarding = 0;

	meshnw_clear_routes();

	return sx127x_init(config);

}


void meshnw_enable_forwarding(void)
{
	context.enable_forwarding = 1;
}


bool meshnw_set_route(nodeid_t destination, nodeid_t next_hop)
{
	if (destination > MESHNW_MAX_NODEID)
	{
		printf("Attempt to set route to invalid destination %u", destination);
		return false;
	}

	// Set entry <destination> to <next_hop>
	context.routing_table[destination] = next_hop;

	return true;
}


void meshnw_clear_routes(void)
{
	_Static_assert((sizeof(context.routing_table) / sizeof(context.routing_table[0]) == MESHNW_MAX_NODEID + 1), "Invalid routing table size");

	for (uint32_t i = 0; i < MESHNW_MAX_NODEID; i++)
	{
		// Reset all routes to INVALID_NODE
		context.routing_table[i] = MESHNW_INVALID_NODE;
	}

	// and disable forwarding again
	context.enable_forwarding = 0;
}


bool meshnw_send(nodeid_t dst, void *data, uint8_t len)
{
	if (len + sizeof(layer3_packet_header_t) > MESHNW_MAX_OTA_PACKET_SIZE)
	{
		// too large
		return false;
	}

	// wrap data in layer3 packet (add header)
	uint8_t tx_buffer[MESHNW_MAX_OTA_PACKET_SIZE];
	layer3_packet_header_t *header = (layer3_packet_header_t *)tx_buffer;
	void *data_ptr = (void *)(&tx_buffer[sizeof(layer3_packet_header_t)]);

	header->src = context.my_node_id;
	header->dst = dst;
	memcpy(data_ptr, data, len);

	// And call the forward function.
	// This will set the "next_hop" in the packet to the value from the routing table for <dst>
	return forward_packet(tx_buffer, sizeof(layer3_packet_header_t) + len);
}


uint64_t meshnw_get_random(void)
{
	uint64_t rand;

	xSemaphoreTake(context.mutex, portMAX_DELAY);
	rand = sx127x_get_random();
	xSemaphoreGive(context.mutex);

	return rand;
}
