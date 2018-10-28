/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#include <stdint.h>
#include <stdbool.h>

// Number of channels
#define FREQUENCY_SENSOR_NUM_OF_CHANNELS      1

// Time for a sample in ticks
#define FREQUENCY_SENSOR_SAMPLE_TIME      10000

// Max number of samples
#define FREQUENCY_SENSOR_MAX_SAMPLE_COUNT    64

/*
 * Initilizes the frequency sensor for the given channel.
 * (The channels are defined in the c file)
 *
 * channel                    Channel to configure / initialize
 * threshold                  Min frequency (number of detected positive edges within FREQUENCY_SENSOR_SAMPLE_TIME)
 * sample_count               Number of samples used to determine the status
 *                            Setting sample_count to 0 disables the channel
 * negative_sample_threshold  If more than the specified number of samples are negative (below the threshold)
 *                            the status is set to false
 */
bool frequency_sensor_init(uint8_t channel, uint16_t threshold, uint8_t sample_count, uint8_t negative_sample_threshold);
bool frequency_sensor_get_status(uint8_t channel);
uint8_t frequency_sensor_get_negative_counter(uint8_t channel);
