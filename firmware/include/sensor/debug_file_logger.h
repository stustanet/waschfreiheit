/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#include <stdint.h>
#include <stdbool.h>
#include "state_estimation.h"

#define DEBUG_FILE_LOGGER_AVAILABLE

enum DEBUG_LOG_TYPE
{
	DEBUG_LOG_TYPE_ADC,
	DEBUG_LOG_TYPE_STATEEST,
	DEBUG_LOG_TYPE_NETWORK,
	DEBUG_LOG_TYPE_NUMOF
};

// Enable RX/TX logging for the network
#define DEBUG_FILE_LOGGER_OPTION_NW_RX  1
#define DEBUG_FILE_LOGGER_OPTION_NW_TX  2

// Init the internal structures.
// Should be called before any other debug_file_logger function.
void debug_file_logger_init(void);

// Sets the file for the log output
bool debug_file_logger_set_file(const char *file, bool append);

// Configures the logging for a specific type
// Options:
// ADC:      Lower 16 bit: Channel bitmask for raw values
//           Upper 16 bit: Channel bitmask for filtered values
// STATEEST: Channels (Bitmask)
// NETWORK:  DEBUG_FILE_LOGGER_OPTION_NW_*
void debug_file_logger_configure(enum DEBUG_LOG_TYPE what, uint32_t options);

// Log raw ADC values
void debug_file_logger_log_raw_adc(const uint16_t *vals, uint8_t cnt);

// Log filtered adc values
void debug_file_logger_log_filtered_adc(const uint16_t *vals, uint8_t cnt);

// Log a state change
void debug_file_logger_log_stateest(uint8_t channel, uint8_t new_state);

// Log a network packet
// rssi and snr are ignored if tx is true
void debug_file_logger_log_network_packet(const void *data, uint8_t len, bool tx, uint8_t rssi, int8_t snr);

// Log some string, can be useful as a marker
// This option is always enabled
void debug_file_logger_log_marker(const char *str);

// Flush the internal log buffer
void debug_file_logger_flush(void);

bool debug_file_logger_is_open(void);
uint32_t debug_file_logger_get_opt(enum DEBUG_LOG_TYPE what);
