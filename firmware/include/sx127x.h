/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>


typedef struct _sx127x_rf_config
{
	uint32_t frequency;
	uint8_t tx_power;
	uint8_t lora_spread_factor;
	uint8_t lora_coderate;
	uint8_t lora_bandwidth;
} sx127x_rf_config_t;

bool sx127x_init(const sx127x_rf_config_t *cfg);

// Reads the next packet from the rx buffer
// Should be called often enough to not lose any packets
// Returns the size of the received packet. (This may be larger than max!)
uint8_t sx127x_recv(uint8_t *buffer, uint8_t max);

bool sx127x_send(const uint8_t *data, uint8_t len);
bool sx127x_is_busy(void);
uint64_t sx127x_get_random(void);

/*
 * Reads the snr and the rssi value for the last received packet
 * RSSI: <rssi> - 157 dB
 * SNR:  <snr> / 4 dB
 */
void sx127x_get_last_pkt_stats(uint8_t *rssi, int8_t *snr);


void sx127x_test_cmd(int argc, char **argv);
