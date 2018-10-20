/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


/*
 * State estimation engine.
 *
 * This processes the raw ADC values and estimates the current machine state from it.
 */

#pragma once

#include <stdint.h>
#include "state_estimation_params.h"

// Supply volatge of the sensor in mV
// This is used to calculate the initial mid value
#define SENSOR_VCC_MV                         4250

// Reference voltage of the ADC in mV
#define ADC_REFERENCE_MV                      3300

static const uint8_t SE_STATE_OFF =              0;
static const uint8_t SE_STATE_END =              1;
static const uint8_t SE_STATE_ON_THRESHOLD =     2; // >= 2 => ON
static const uint16_t SE_MAX_END_STATE_TIME = 1900; // 1900 sec

// Half the sensor voltage shifted 20 bit to the left to get 32 bit scale
static const uint32_t SE_INITIAL_MID_VALUE = ((((SENSOR_VCC_MV * (1 << 12)) / ADC_REFERENCE_MV) / 2) << 20);


struct state_estimation_data
{
	state_estimation_params_t params;

	struct
	{
		/*
		 * Current mid value.
		 * Scale: 32 bit
		 */
		uint32_t mid;

		/*
		 * Current low-passed absolute value.
		 * Scale: 18 bit
		 */
		uint32_t current;

		/*
		 * Counter for frame processing.
		 */
		uint16_t counter;

	} input_filter;

	struct
	{
		/*
		 * Buffer for old values
		 * The MSB is used to indicate this value as "contributed to the sum"
		 * (passed the reject threshold/block filter)
		 */
		uint16_t window[SE_MAX_WINDOW_SIZE];

		// start and end of circular buffer
		uint16_t window_next_free;
		uint16_t window_oldest_valid;

		/*
		 * Timer to limit duration of SE_STATE_END
		 * If the system stay for more than SE_MAX_END_STATE_TIME in the end state, the state is set to OFF
		 */
		uint16_t end_state_timer;

		/*
		 * For end state timer.
		 * This value contains the number of frames to switch from end state to off state.
		 */
		uint16_t max_end_state_time;

		/*
		 * Number of old values above reject threshold
		 */
		uint16_t above_reject_counter;

		/*
		 * The current sum of all values that passed the threshold/block filter in the window
		 * window_sum / current_window_size is the current avgerage scaled to 15 bit
		 */
		uint32_t window_sum;

		/*
		 * The current state.
		 */
		uint8_t current_state;

	} state_filter;
};

enum state_update_result
{
	state_update_unchanged,      // No on-off change
	state_update_changed_to_off, // Was on, is now off
	state_update_changed_to_on   // was off, is now on
};

typedef struct state_estimation_data state_estimation_data_t;
typedef enum state_update_result state_update_result_t;


/*
 * Initializes the state estimation engine.
 * Values in the state estimation data hould NOT be changed externally.
 * Returns 0 on success or nonzero on error.
 */
int stateest_init(state_estimation_data_t *data, const state_estimation_params_t *params, uint16_t adc_samples_per_sec);

/*
 * Changes the current samples per second, must be called after stateest_init()
 */
void stateest_set_adc_sps(state_estimation_data_t *data, uint16_t adc_samples_per_sec);

/*
 * Updates the state engine with new adc data.
 */
state_update_result_t stateest_update(state_estimation_data_t *data, uint16_t raw_value);

/*
 * If the last update was a frame update, the last frame value (sacled ro 16 bit) is returned,
 * otherwise 0xffffffff is returned.
 */
uint32_t stateest_get_frame(const state_estimation_data_t *data);

/*
 * Gets the current reject filter value.
 * This is the value used as condition for the state transitions.
 */
int16_t stateest_get_current_rf_value(const state_estimation_data_t *data);

/*
 * Gets the current state index.
 */
static inline uint8_t stateest_get_current_state(const state_estimation_data_t *data)
{
	return data->state_filter.current_state;
}

/*
 * Gets the current state index.
 */
static inline uint8_t stateest_is_on(const state_estimation_data_t *data)
{
	return data->state_filter.current_state >= SE_STATE_ON_THRESHOLD;
}
