#pragma once

#include <stdint.h>

#define USBACM_INT_PRIO 0x88

// Needs to be implemented by user
void usbacm_recv_handler(void *data, uint32_t len);

/*
 * usbacm_init depends on the delay function, so you MUST call delay_init before this
 */
void usbacm_init(void);
void usbacm_send(const void *data, uint32_t len);
