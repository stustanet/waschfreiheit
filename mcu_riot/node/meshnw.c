// TODO: Mesh functions, currently this is only direct


#include "meshnw.h"
#include "meshnw_config.h"

#include <stdio.h>
#include <string.h>

#include "net/gnrc/netdev.h"
#include "net/netdev.h"

#include "sx127x_internal.h"
#include "sx127x_params.h"
#include "sx127x_netdev.h"

#include "thread.h"


#define MESHNW_MSG_QUEUE   (16U)
#define NETDEV_ISR_EVENT_MESSAGE   (0x3456)

#define MESHNW_MAX_OTA_PACKET_SIZE MESHNW_MAX_PACKET_SIZE


static nodeid_t my_node_id;
static mesh_nw_message_cb_t recv_callback;

static char recv_thd_stack[THREAD_STACKSIZE_DEFAULT];
static kernel_pid_t recv_thd_pid = KERNEL_PID_UNDEF;
static sx127x_t sx127x;
static netdev_t *netdev;

static uint8_t recv_buffer[MESHNW_MAX_OTA_PACKET_SIZE];


static void start_listen(void)
{
    netdev->driver->set(netdev, NETOPT_SINGLE_RECEIVE, false, sizeof(uint8_t));
    sx127x_set_rx(&sx127x);
}


static void event_cb(netdev_t *dev, netdev_event_t event)
{
    if (event == NETDEV_EVENT_ISR)
	{
        msg_t msg;

        msg.type = NETDEV_ISR_EVENT_MESSAGE;
        msg.content.ptr = dev;

        if (msg_send(&msg, recv_thd_pid) <= 0)
		{
            puts("gnrc_netdev: possibly lost interrupt.");
        }
    }
    else
	{
        size_t len;
        netdev_sx127x_lora_packet_info_t packet_info;
        switch (event) {
            case NETDEV_EVENT_RX_COMPLETE:
                len = dev->driver->recv(dev, NULL, 0, 0);

				if (len > sizeof(recv_buffer))
				{
					printf("Discard too large packet with size %u.\n", len);
					dev->driver->recv(dev, NULL, len, NULL);
					break;
				}
                dev->driver->recv(dev, recv_buffer, len, &packet_info);
                printf("{Payload: \"%s\" (%d bytes), RSSI: %i, SNR: %i, TOA: %i}\n",
                       recv_buffer, (int)len,
                       packet_info.rssi, (int)packet_info.snr,
                       (int)packet_info.time_on_air);


				(*recv_callback)(recv_buffer, len);

				start_listen();

				break;


            case NETDEV_EVENT_TX_COMPLETE:
                puts("Transmission completed");
				start_listen();
                break;
            case NETDEV_EVENT_CAD_DONE:
                break;
            case NETDEV_EVENT_TX_TIMEOUT:
                break;
            default:
                printf("Unexpected netdev event received: %d\n", event);
                break;
        }
    }
}


static void *recv_thread(void *arg)
{
    static msg_t _msg_q[MESHNW_MSG_QUEUE];
    msg_init_queue(_msg_q, MESHNW_MSG_QUEUE);

    while (1)
	{
        msg_t msg;
        msg_receive(&msg);
        if (msg.type == NETDEV_ISR_EVENT_MESSAGE)
		{
            netdev_t *dev = msg.content.ptr;
            dev->driver->isr(dev);
        }
        else 
		{
            puts("Unexpected msg type");
        }
    }

	return 0;
}


static int setup(void)
{
	_Static_assert(SX127X_CONFIG_LORA_SPREAD >= 7 && SX127X_CONFIG_LORA_SPREAD <= 12, "Spread factor must be between 7 and 12");
	_Static_assert(SX127X_CONFIG_LORA_CODERATE >= 5 && SX127X_CONFIG_LORA_CODERATE <= 8, "Coderate must be between 5 and 8");

    uint8_t lora_bw = SX127X_CONFIG_LORA_BW;
    uint8_t lora_sf = SX127X_CONFIG_LORA_SPREAD;

    int lora_cr = SX127X_CONFIG_LORA_CODERATE - 4;

    netdev_t *netdev = (netdev_t*) &sx127x;
    netdev->driver->set(netdev, NETOPT_BANDWIDTH,
                        &lora_bw, sizeof(uint8_t));
    netdev->driver->set(netdev, NETOPT_SPREADING_FACTOR,
                        &lora_sf, 1);
    netdev->driver->set(netdev, NETOPT_CODING_RATE,
                        &lora_cr, sizeof(uint8_t));

    uint32_t chan = SX127X_CONFIG_LORA_FREQUENCY;
    netdev->driver->set(netdev, NETOPT_CHANNEL, &chan, sizeof(uint32_t));
   

	start_listen();

	return 0;
}


int meshnw_init(nodeid_t id, mesh_nw_message_cb_t cb)
{
	if (recv_thd_pid != KERNEL_PID_UNDEF)
	{
		return -ENOTSUP;
	}

    memcpy(&sx127x.params, sx127x_params, sizeof(sx127x_params));
    netdev = (netdev_t*) &sx127x;
    netdev->driver = &sx127x_driver;
    netdev->driver->init(netdev);
    netdev->event_callback = event_cb;

    recv_thd_pid = thread_create(recv_thd_stack, sizeof(recv_thd_stack), THREAD_PRIORITY_MAIN - 1,
                              THREAD_CREATE_STACKTEST, recv_thread, NULL,
                              "meshnw_recv_thread");

    if (recv_thd_pid <= KERNEL_PID_UNDEF)
	{
        puts("Creation of receiver thread failed");
        return -1;
    }

	my_node_id = id;
	recv_callback = cb;

	return setup();

}


int meshnw_send(nodeid_t dst, void *data, uint8_t len)
{
	if (recv_thd_pid == KERNEL_PID_UNDEF)
	{
		return -ENETDOWN;
	}

	if (len > MESHNW_MAX_OTA_PACKET_SIZE)
	{
		return -ENOTSUP;
	}

	// TODO: Need to pack the data into a wrapper here (add destiantion id)

    struct iovec vec[1];
    vec[0].iov_base = data;
    vec[0].iov_len = len;
    if (netdev->driver->send(netdev, vec, 1) == -ENOTSUP)
	{
        puts("Cannot send: radio is still transmitting");
		return -EBUSY;
    }

	return 0;
}


