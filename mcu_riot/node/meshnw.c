// This include needs to be first beacuse it sets some defines used by later includes
#include "board.h"

#include "meshnw.h"
#include "meshnw_config.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "mutex.h"

#include "net/netdev.h"

#include "sx127x_params.h"
#include "sx127x_netdev.h"

#include "thread.h"


#define MESHNW_MSG_QUEUE   (16U)
#define NETDEV_ISR_EVENT_MESSAGE   (0x3456)

/*
 * Strategy to avoid collisions when transmitting:
 *
 * The straight forward approuch would be to read the netdev state and simply don't send anything, if the state is "RX"
 * sadly this does NOT work with the sx127x. So i have to abuse the CAD for this.
 * TODO: Check if I can fix the driver so that the RX mode is detected correctly!
 *
 * I have a local tx data buffer, when I get a tx request i first check if the tx buffer is already in use,
 * if so the tx will fail. If not, the message is copied into this buffer and the CAD is started.
 * When i get the CAD done event, I check if the channel is used, if not the tansmission is started.
 * Otherwise (channel busy) the transmission fails (but i have no method to notify the sender).
 * In both cases the tx buffer is cleared.
 *
 * To avoid unneccessary redundant buffers, the routing is done when the packet is actually sent.
 * This allows me to assemble the packet directly in the tx buffer.
 */

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

	// Buffer for sending the messages
	uint8_t tx_buffer[MESHNW_MAX_OTA_PACKET_SIZE];

	// Number of bytes in the tx buffer.
	// If this is 0, the buffer is unused.
	size_t tx_buffer_used;

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
 * Switches the driver to listen mode.
 * This needs to be called after transmitting data
 */
static void start_listen(void)
{
	/*
	 * Disable timeout
	 */
	uint32_t timeout = 0;
	context.netdev->driver->set(context.netdev, NETOPT_RX_TIMEOUT, &timeout, sizeof(timeout));

	/*
	 * Disable timeout
	 */
	netopt_enable_t en = NETOPT_DISABLE;
	context.netdev->driver->set(context.netdev, NETOPT_SINGLE_RECEIVE, &en, sizeof(en));

	/*
	 * Switch to RX mode
	 */
	netopt_state_t rx = NETOPT_STATE_RX;
	context.netdev->driver->set(context.netdev, NETOPT_STATE, &rx, sizeof(rx));
}


/*
 * Starts the CAD.
 * This should be called after new data has been written to the tx buffer.
 */
static void begin_cad(void)
{
	/*
	 * For the device to get to start the CAD i first need to switch it into standby mode, otherwise the cad gets stuck.
	 */
	netopt_state_t mode = NETOPT_STATE_STANDBY;
	context.netdev->driver->set(context.netdev, NETOPT_STATE, &mode, sizeof(mode));

	 // For now there is no "normal" way to start the CAD so  I'll just call the drivers' function directly.
	 sx127x_start_cad(&context.sx127x);
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
		
		if (context.tx_buffer_used > 0)
		{
			puts("Can't forward packet beacuse tx is still in progress");
		}
		else
		{
			// copy packet to tx buffer
			_Static_assert(sizeof(context.tx_buffer) == sizeof(context.recv_buffer), "tx and rx buffer must have the same size");
			memcpy(context.tx_buffer, context.recv_buffer, len);
			context.tx_buffer_used = len;

			// Start CAD, the packet will be sent in the handler
			begin_cad();
		}
	}
}


/*
 * Should be called when i get a CAD_DONE event from the netdev.
 * This check, if the channel is free and if so, the message is sent according to the routing table.
 * Sends/Forwards a packet to the next hop specified in the routing table
 */
static void handle_cad_done(void)
{
	if (context.tx_buffer_used < sizeof(layer3_packet_header_t))
	{
		// buffer empty -> nothing to do
		puts("Got CAD done event but packet buffer is empty!");
		context.tx_buffer_used = 0;
		return;
	}

	// check if the channel is in use
	// TODO: This is super super ugly, becasue I directly access internal variables of the sx127x driver.
	//       Sadly for now, this is the only way to get this information.
	//       For now this also riquires manually fixing the driver (the internal value is NOT set correctly)
	//       I intend to resolve both problems and bring the changes upstream!
	if (context.sx127x._internal.is_last_cad_success)
	{
		puts("CAD is positive -> Don't send to avoid collision");
		
		// discard tx buffer
		context.tx_buffer_used = 0;
		return;
	}

	layer3_packet_header_t *hdr = (layer3_packet_header_t *)context.tx_buffer;

	// Get the next route and set it as next_hop
	hdr->next_hop = get_route(hdr->dst);

	if (hdr->next_hop > MESHNW_MAX_NODEID)
	{
		// no route found -> drop packet
		printf("Can't forward packet, no route to %u!\n", hdr->dst);
		context.tx_buffer_used = 0;
		return;
	}

	// Send packet, next_hop specifies the receiver
	struct iovec vec[1];
	vec[0].iov_base = context.tx_buffer;
	vec[0].iov_len = context.tx_buffer_used;
	if (context.netdev->driver->send(context.netdev, vec, 1) != 0)
	{
		// This should never happen
		puts("Failed to send packet with netdev!");
		context.tx_buffer_used = 0;
		return;
	}
	puts("TX success");

	// tx buffer is reset in the tx complete event
	return;
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
	switch (event)
	{
		case NETDEV_EVENT_ISR:
		{
			// Interrut -> signal the thread to allow the driver handle stuff in non-interrupt context
			msg_t msg;

			msg.type = NETDEV_ISR_EVENT_MESSAGE;
			msg.content.ptr = dev;

			if (msg_send(&msg, context.recv_thd_pid) <= 0)
			{
				puts("Failed to signal thread from interrupt context.");
			}
			break;
		}
		case NETDEV_EVENT_RX_COMPLETE:
		    // Packet arrived -> call rx handler
			handle_rx_cplt();
			break;


		case NETDEV_EVENT_TX_COMPLETE:
			// Packet sent -> clear tx buffer and re-enter listen mode
			context.tx_buffer_used = 0;
			puts("TX OK");
			start_listen();
			break;

		case NETDEV_EVENT_CAD_DONE:
			// This happens when I'm about to send something -> call cad done handler
			puts("CAD done");
			handle_cad_done();
			break;

		case NETDEV_EVENT_TX_TIMEOUT:
			// Something went wrong during TX -> clear tx buffer and re-enter listen mode
			context.tx_buffer_used = 0;
			puts("TX TIMEOUT");
			start_listen();
			break;

		default:
			printf("Unexpected netdev event received: %d\n", event);
			break;
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
	netdev->driver->set(netdev, NETOPT_BANDWIDTH, &lora_bw, sizeof(uint8_t));
	netdev->driver->set(netdev, NETOPT_SPREADING_FACTOR, &lora_sf, 1);
	netdev->driver->set(netdev, NETOPT_CODING_RATE, &lora_cr, sizeof(uint8_t));

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
	netdev->driver->set(netdev, NETOPT_CHANNEL_FREQUENCY, &chan, sizeof(uint32_t));

	if (config->tx_power > SX127X_CONFIG_LORA_POWER_MAX)
	{
		printf("RF tx power (%u) too large, max: %u\n", config->tx_power, SX127X_CONFIG_LORA_POWER_MAX);
		return 2;
	}

	int16_t tx_pwr = config->tx_power;
	netdev->driver->set(netdev, NETOPT_TX_POWER, &tx_pwr, sizeof(uint16_t));

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

	if (context.tx_buffer_used > 0)
	{
		puts("Can't send data, tx buffer not empty");
		return -EBUSY;
	}

	/*
	 * Wrap data in layer3 packet (add header)
	 * This packet is assembled directly in the global packet buffer.
	 */

	layer3_packet_header_t *header = (layer3_packet_header_t *)context.tx_buffer;
	void *data_ptr = (void *)(&context.tx_buffer[sizeof(layer3_packet_header_t)]);

	header->src = context.my_node_id;
	header->dst = dst;
	memcpy(data_ptr, data, len);

	context.tx_buffer_used = sizeof(layer3_packet_header_t) + len;

	/*
	 * Start a CAD (channel activity detection)
	 * The data is sent if this results in the channel being empty.
	 * Otherwise (channel used) the data is discarded.
	 */

	begin_cad();

	return 0;
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
	start_listen();

	return rand;
}
