#include "meshnw.h"
#include "meshnw_config.h"

#include <stdio.h>
#include <string.h>

#include "net/gnrc/netdev.h"
#include "net/netdev.h"

#include "sx127x_params.h"
#include "sx127x_netdev.h"

#include "thread.h"


#define MESHNW_MSG_QUEUE   (16U)
#define NETDEV_ISR_EVENT_MESSAGE   (0x3456)

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
	char recv_thd_stack[2048];
	kernel_pid_t recv_thd_pid;

	// This callback is called when a message is received.
	mesh_nw_message_cb_t recv_callback;

	// Netdev abstraction for the rf module
	netdev_t *netdev;

	// Direct instance of the rf module driver
	sx127x_t sx127x;

	// The id of this node
	nodeid_t my_node_id;

	// Routing table
	// This table contains an entry for every address.
	// A packet for n is sent to the next hop <routing_table[n]>.
	uint8_t routing_table[MESHNW_MAX_NODEID + 1];

	// Buffer for receiving packets
	uint8_t recv_buffer[MESHNW_MAX_OTA_PACKET_SIZE];

	// If this is zero, incoming packets with destination addres unqual to my address are dropped.
	// Otherwise they are forwarded  according to the routing table.
	uint8_t enable_forwarding;
} meshnw_context_data_t;

static meshnw_context_data_t context = { 0 };

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
static int forward_packet(void *packet, uint8_t len)
{
	layer3_packet_header_t *hdr = (layer3_packet_header_t *)packet;

	// Get the next route and set it as next_hop
	hdr->next_hop = get_route(hdr->dst);

	if (hdr->next_hop > MESHNW_MAX_NODEID)
	{
		// no route found -> drop packet
		printf("Can't forward packet, no route to %u!\n", hdr->dst);
		return -ENOENT;
	}

	// Send packet, next_hop specifies the receiver
	struct iovec vec[1];
	vec[0].iov_base = packet;
	vec[0].iov_len = len;
	if (context.netdev->driver->send(context.netdev, vec, 1) == -ENOTSUP)
	{
		puts("Can't send packet while radio is busy");
		return -EBUSY;
	}

	return 0;
}


/*
 * Switches the driver to listen mode.
 * This needs to be called after transmitting data
 */
static void start_listen(void)
{
	context.netdev->driver->set(context.netdev, NETOPT_SINGLE_RECEIVE, false, sizeof(uint8_t));
	sx127x_set_rx(&context.sx127x);
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
static void handle_rx_cplt(void)
{
	netdev_t *dev = context.netdev;

	size_t len;
	netdev_sx127x_lora_packet_info_t packet_info;

	// First get packet length
	len = dev->driver->recv(dev, NULL, 0, 0);

	// Check if length is in bounds
	if (len < sizeof(layer3_packet_header_t) + 1 || len > sizeof(context.recv_buffer))
	{
		// I (invalid) => discard
		printf("Discard packet with invalid size %u.\n", len);
		dev->driver->recv(dev, NULL, len, NULL);
		return;
	}

	// Now read whole packet into receive buffer
	dev->driver->recv(dev, context.recv_buffer, len, &packet_info);

	// Check the packet header
	layer3_packet_header_t *hdr = (layer3_packet_header_t *)context.recv_buffer;

	printf("Received packet for from %u for %u (%d bytes), RSSI: %i, SNR: %i\n",
		   hdr->src, hdr->dst, (int)len,
		   packet_info.rssi, (int)packet_info.snr);



	if (hdr->next_hop != context.my_node_id)
	{
		// II (not for me) => discard
		printf("Discard packet with next_hop %i\n", hdr->next_hop);
		return;
	}

	if (hdr->dst == context.my_node_id)
	{
		// III (data for the current node) => call callback
		uint8_t *pld = context.recv_buffer + sizeof(layer3_packet_header_t);
		uint8_t pld_len = len - sizeof(layer3_packet_header_t);
		(*context.recv_callback)(hdr->src, pld, pld_len);
	}
	else if(context.enable_forwarding)
	{
		// IV (need to forward) -> forward
		int res = forward_packet(context.recv_buffer, len);
		if (res != 0)
		{
			printf("Failed to forward packet, error %i.\n", res);
		}
	}
}


/*
 * Event callback called from the driver.
 * NOTE:
 * This callback is called from two different context types:
 * The interrupt context (NETDEV_EVENT_ISR, in this case I MUST forward this event
 * to the thread and call the drivers' isr function from the thread)
 * and the thread context (then i can handle the data)
 */
static void event_cb(netdev_t *dev, netdev_event_t event)
{
	if (event == NETDEV_EVENT_ISR)
	{
		// Interrut -> signal the thread to allow the driver handle stuff in non-interrupt context
		msg_t msg;

		msg.type = NETDEV_ISR_EVENT_MESSAGE;
		msg.content.ptr = dev;

		if (msg_send(&msg, context.recv_thd_pid) <= 0)
		{
			puts("Failed to signal thread from interrupt context.");
		}
	}
	else
	{
		switch (event)
		{
			case NETDEV_EVENT_RX_COMPLETE:
				handle_rx_cplt();
				break;


			case NETDEV_EVENT_TX_COMPLETE:
				puts("Transmission completed");
				start_listen();
				break;

			case NETDEV_EVENT_CAD_DONE:
				break;

			case NETDEV_EVENT_TX_TIMEOUT:
				start_listen();
				break;

			default:
				printf("Unexpected netdev event received: %d\n", event);
				break;
		}
	}
}


/*
 * The receiving thread.
 * This does nothing but waiting for the interrupt signal and calling the drivers interrupt function.
 */
static void *recv_thread(void *arg)
{
	(void)arg;
	static msg_t _msg_q[MESHNW_MSG_QUEUE];
	msg_init_queue(_msg_q, MESHNW_MSG_QUEUE);

	while (1)
	{
		msg_t msg;
		msg_receive(&msg);
		if (msg.type == NETDEV_ISR_EVENT_MESSAGE)
		{
			// call the drivers' isr function (data will be processed in thread context)
			netdev_t *dev = msg.content.ptr;
			dev->driver->isr(dev);
		}
		else
		{
			puts("Meshnw receiving thread got unexpected msg type.");
		}
	}
	return 0;
}


/*
 * Configures the LoRa driver.
 */
static int setup(const meshnw_rf_config_t *config)
{
	_Static_assert(SX127X_CONFIG_LORA_SPREAD >= 7 && SX127X_CONFIG_LORA_SPREAD <= 12, "Spread factor must be between 7 and 12");
	_Static_assert(SX127X_CONFIG_LORA_CODERATE >= 5 && SX127X_CONFIG_LORA_CODERATE <= 8, "Coderate must be between 5 and 8");

	uint8_t lora_bw = SX127X_CONFIG_LORA_BW;
	uint8_t lora_sf = SX127X_CONFIG_LORA_SPREAD;

	int lora_cr = SX127X_CONFIG_LORA_CODERATE - 4;

	netdev_t *netdev = (netdev_t*) &context.sx127x;
	netdev->driver->set(netdev, NETOPT_BANDWIDTH,
	                    &lora_bw, sizeof(uint8_t));
	netdev->driver->set(netdev, NETOPT_SPREADING_FACTOR,
	                    &lora_sf, 1);
	netdev->driver->set(netdev, NETOPT_CODING_RATE,
	                    &lora_cr, sizeof(uint8_t));

	if (config->frequency > SX127X_CONFIG_LORA_FREQUENCY_MAX ||
	    config->frequency < SX127X_CONFIG_LORA_FREQUENCY_MIN)
	{
		printf("RF frequency (%lu) out of allowed range (%u - %u)\n",
		       config->frequency,
		       SX127X_CONFIG_LORA_FREQUENCY_MIN,
		       SX127X_CONFIG_LORA_FREQUENCY_MAX);
		return 1;
	}
	uint32_t chan = config->frequency;
	netdev->driver->set(netdev, NETOPT_CHANNEL, &chan, sizeof(uint32_t));

	if (config->tx_power > SX127X_CONFIG_LORA_POWER_MAX)
	{
		printf("RF tx power (%u) too large, max: %u\n", config->tx_power, SX127X_CONFIG_LORA_POWER_MAX);
		return 2;
	}

	sx127x_set_tx_power(&context.sx127x, config->tx_power);

	start_listen();

	return 0;
}


/*
 * Util function to prevent "weak" random numbers
 * Sometimes the driver just returns 0 or ~0, dunno why
 */
static uint32_t get_random_checked(void)
{
	uint32_t rnd;
	do
	{
		rnd = sx127x_random(&context.sx127x);
	} while(rnd == 0 || rnd == 0xffffffff);
	return rnd;
}


/*
 * Initializes the LoRa driver and internal data.
 */
int meshnw_init(nodeid_t id, const meshnw_rf_config_t *config, mesh_nw_message_cb_t cb)
{
	if (context.recv_thd_pid != 0)
	{
		// Thread id is not 0 (the initial value)
		// => already running, only one instance allowed / possible
		return -ENOTSUP;
	}

	memcpy(&context.sx127x.params, sx127x_params, sizeof(sx127x_params));
	context.netdev = (netdev_t*) &context.sx127x;
	context.netdev->driver = &sx127x_driver;
	context.netdev->driver->init(context.netdev);
	context.netdev->event_callback = event_cb;

	context.recv_thd_pid = thread_create(context.recv_thd_stack, sizeof(context.recv_thd_stack), THREAD_PRIORITY_MAIN - 1,
	                          THREAD_CREATE_STACKTEST, recv_thread, NULL,
	                          "meshnw_recv_thread");

	if (context.recv_thd_pid <= KERNEL_PID_UNDEF)
	{
		puts("Creation of receiver thread failed");
		return -1;
	}

	context.my_node_id = id;
	context.recv_callback = cb;
	context.enable_forwarding = 0;

	meshnw_clear_routes();

	return setup(config);

}


void meshnw_enable_forwarding(void)
{
	context.enable_forwarding = 1;
}


int meshnw_set_route(nodeid_t destination, nodeid_t next_hop)
{
	if (destination > MESHNW_MAX_NODEID)
	{
		printf("Attempt to set route to invalid destination %u", destination);
		return -EINVAL;
	}

	// Set entry <destination> to <next_hop>
	context.routing_table[destination] = next_hop;

	return 0;
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


int meshnw_send(nodeid_t dst, void *data, uint8_t len)
{
	if (context.recv_thd_pid == KERNEL_PID_UNDEF)
	{
		// not initialized
		return -ENETDOWN;
	}

	if (len + sizeof(layer3_packet_header_t) > MESHNW_MAX_OTA_PACKET_SIZE)
	{
		// too large
		return -ENOTSUP;
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
	uint64_t rand = 0;

	for (uint8_t i = 0; i < 16; i++)
	{
		uint32_t rnd = get_random_checked();
		rand = (rand << 2) + rnd;
	}

	// Re-enter rx mode as this is disbaled by the random function
	sx127x_set_rx(&context.sx127x);

	return rand;
}
