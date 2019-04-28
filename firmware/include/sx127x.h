/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 *
 * This is a slim library for the sx127x lora modem based on libopencm3
 * It uses no static ram and only a small ammount of stack.
 * Current limitations:
 *   - Only polling mode available
 *   - Only LoRa mode (so no FSK or OOK)
 *   - No support for frequency hopping
 *   - ...
 */

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>


typedef struct _sx127x_rf_config
{
	// Center frequency in Hz
	uint32_t frequency;

	// Output power in dBm
	uint8_t tx_power;

	// Lora spreading factor [7 - 12]
	// refer to the datasheet for more information
	uint8_t lora_spread_factor;

	// Lora coding rate
	// 0 -> 4/5
	// 1 -> 4/6
	// 2 -> 4/7
	// 3 -> 4/8
	// refer to the datasheet for more information
	uint8_t lora_coderate;

	// Lora bandwidth [0 - 9]
	// refer to the datasheet for more information
	uint8_t lora_bandwidth;
} sx127x_rf_config_t;

// The pointer to the configuration is stored internally and used if a modem reset is required.
// Therefore, it must point to a persistent memory location.
bool sx127x_init(const sx127x_rf_config_t *cfg);

/*
 * Reads the packet from the rx buffer.
 * Should be called often enough to not lose any packets
 * Returns the size of the received packet or 0 is no packet has been received.
 * NOTE: The returned packet length may exceed the size of the buffer (<max>)!
 *       In this case only the first <max> bytes of the packet are written to
 *       the buffer and the rest of the packet is discarded.
 */
uint8_t sx127x_recv(uint8_t *buffer, uint8_t max);

/*
 * Sends a packet.
 * This fails if the modem is busy (transmitting or receiving)
 * The function does NOT wait for the transmission to complete.
 */
bool sx127x_send(const uint8_t *data, uint8_t len);

/*
 * Checks if the modem is busy transmitting or receiving.
 */
bool sx127x_is_busy(void);

/*
 * Use the modem's wideband RSSI measurement to get a "good" random number.
 */
uint64_t sx127x_get_random(void);

/*
 * Reads the snr and the rssi value for the last received packet
 * RSSI: <rssi> - 157 dB
 * SNR:  <snr> / 4 dB
 */
void sx127x_get_last_pkt_stats(uint8_t *rssi, int8_t *snr);


void sx127x_test_cmd(int argc, char **argv);
