/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#pragma once
#include <stdint.h>


// These two must be defines because they are used as array size
#define SE_MAX_WINDOW_SIZE                (512 * 3)
#define SE_STATECOUNT                            4

struct state_estimation_params
{
	/*
	 * Configuration of the input filter.
	 * The input filter is applied to every raw adc value.
	 */
	struct
	{
		/*
		 * Step size for adjusting the current mid value.
		 * If the current adc value is above mid, mid is increased by this step size, if the value is below, mid is reduced.
		 * If this is too small, the mid value takes very long to settle (and reach the desited value)
		 * If this is too large, mid will follow the input waveform which results in signal reduction.
		 */
		uint16_t mid_value_adjustment_speed;

		/*
		 * The lowpass is applied after calculating the absolute value (deviation from mid)
		 *
		 * Weight for the initial low-pass filter. The update term for all incoming new data is:
		 * current = (new + current * lowpass_weight) / (lowpass_weigth + 1)
		 * Large weight => Slower
		 * The max value is 16383.
		 */
		uint16_t lowpass_weight;

		/*
		 * Number of samples per frame.
		 * Every num_samples the state filter is updated.
		 */
		uint16_t num_samples;
	} __attribute__((packed)) input_filter;

	/*
	 * Configuration of the state filter.
	 * This filter is updated once for every frame (num_samples in input filter)
	 */
	struct
	{
		/*
		 * State transition matrix.
		 * The entry c * SE_STATECOUNT + n describes the transition condition from c to n (or n + 1, if c >= n).
		 * The matrix is stored in a compressed form, without the diagonal as this is always 0.
		 *  > 0  transition if the current value is above the transition value
		 *  < 0  transition if the current value is below the transition value * -1
		 *    0  no transition possible
		 */
		int16_t transition_matrix[(SE_STATECOUNT - 1) * SE_STATECOUNT];

		/*
		 * Window sizes for every state.
		 */
		uint16_t window_sizes[SE_STATECOUNT];

		/*
		 * Values below this threshold are ignored.
		 * Also if not at least reject_consec_count consecutive values are above the reject_threshold this values are ignored.
		 * reject_consec_count MUST BE smaller than the smallest window size
		 *
		 * This values are 15-bit scaled
		 */
		uint16_t reject_threshold;
		uint16_t reject_consec_count;
	} __attribute__((packed)) state_filter;
} __attribute__((packed));

typedef struct state_estimation_params state_estimation_params_t;
