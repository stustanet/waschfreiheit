#include "state_estimation.h"

#include <stdio.h>
#include <string.h>

#include "debug_assert.h"


/*
 * Update the input filter.
 * This proceses the raw ADC values and calculates the <current> input filter value from the data.
 * <current> describes the short-time average power consumption of the measured device. 
 * This also increments the current filter counter.
 */
static void update_input_filter(state_estimation_data_t *data, uint16_t value)
{
	ASSERT(value < (1 << 12));

	// sacle from 12 bit to 32 bit
	uint32_t value_scaled = value << 20;

	uint32_t absval = 0;

	if (value_scaled > data->input_filter.mid)
	{
		absval = value_scaled - data->input_filter.mid;
		data->input_filter.mid += data->params.input_filter.mid_value_adjustment_speed;
	}
	else if (value_scaled < data->input_filter.mid)
	{
		absval = data->input_filter.mid - value_scaled;
		data->input_filter.mid -= data->params.input_filter.mid_value_adjustment_speed;
	}

	// sacle from 32 bit to 18 bit
	absval = absval >> 14;

	data->input_filter.counter++;
	data->input_filter.current = (data->input_filter.current * data->params.input_filter.lowpass_weight + absval) / (data->params.input_filter.lowpass_weight + 1);
}


/*
 * Calculates the number of valid elements in the current window.
 *
 * NOTE:
 * The return value assumes that the element referenced by next_free already in use
 */
static uint16_t calc_current_window_used(const state_estimation_data_t *data)
{
	if (data->state_filter.window_oldest_valid > data->state_filter.window_next_free)
	{
		// Wrap-around
		// => valid values before next free and after oldest_valid
		return (data->state_filter.window_next_free + 1) + (SE_MAX_WINDOW_SIZE - data->state_filter.window_oldest_valid);
	}
	else
	{
		// Linear
		// => valid values between next free and oldest valid
		return (data->state_filter.window_next_free - data->state_filter.window_oldest_valid) + 1;
	}
}


/*
 * Adjust the window size to the current window size as defined by the current state.
 * If old values that contributed to the <window_sum> falls out, it is subtracted from the sum.
 * Also this function advances the window by one so that <window_next_free> will point to the next unused element
 */
static void adjust_window_size(state_estimation_data_t *data)
{
	ASSERT(data->state_filter.current_state < SE_STATECOUNT);

	// first adjust the window size so that at least one new value fits in
	uint16_t current_wnd_size = data->params.state_filter.window_sizes[data->state_filter.current_state];
	uint16_t current_window_used = calc_current_window_used(data);

	uint16_t discard_idx = data->state_filter.window_oldest_valid;
	while (current_window_used >= current_wnd_size)
	{
		// discard value
		if (data->state_filter.window[discard_idx] & (0x8000))
		{
			// msb set -> this value was used
			// => subtract it from sum
			
			ASSERT(data->state_filter.window_sum >= (data->state_filter.window[discard_idx] & 0x7fff));

			data->state_filter.window_sum -= (data->state_filter.window[discard_idx] & 0x7fff);
		}
		
		current_window_used--;
		discard_idx = (discard_idx + 1) % SE_MAX_WINDOW_SIZE;
	}

	data->state_filter.window_oldest_valid = discard_idx;
	data->state_filter.window_next_free = (data->state_filter.window_next_free + 1) % SE_MAX_WINDOW_SIZE;

	ASSERT(data->state_filter.window_oldest_valid < SE_MAX_WINDOW_SIZE);
	ASSERT(data->state_filter.window_next_free < SE_MAX_WINDOW_SIZE);
}


/*
 * Adds the <current> input value to the window at <window_next_free> and updates the <window_sum>
 * according to the reject threshold/block filter.
 *
 * NOTE: After this function <window_next_free> will point to a used element!.
 */
static void update_reject_thd_filter(state_estimation_data_t *data)
{
	// scale to 15 bit
	uint16_t currentval = data->input_filter.current >> 3;
	ASSERT(currentval < (1 << 15));

	uint16_t buffer_pos = data->state_filter.window_next_free;

	if (currentval > data->params.state_filter.reject_threshold)
	{
		/*
		 * Value is above threshold => Three possibilities
		 * I    Less than <reject_consec_count> consecutive values are above threshold
		 *      (<above_reject_counter> < <reject_consec_count>)
		 *      => Insert normally, increment <above_reject_counter>
		 * II   Exactly <reject_consec_count> consecutive values are above the threshold
		 *      => Insert and add <current> to sum (mark as used)
		 *      => Insert and add all not yet added old values to the sum (and mark all as used)
		 * III  More than <reject_consec_count> consecutive values are above the threshold
		 *      => Insert and add <current> to sum (mark as used)
		 */


		if (data->state_filter.above_reject_counter >= data->params.state_filter.reject_consec_count)
		{
			// II or III => Insert and add <current> to sum (and mark as used)
			data->state_filter.window_sum += currentval;
			data->state_filter.window[buffer_pos] = currentval | (0x8000);

			if (data->state_filter.above_reject_counter == data->params.state_filter.reject_consec_count)
			{
				// II => add all old values to sum (and mark as added)
				
				ASSERT(data->params.state_filter.reject_consec_count <= calc_current_window_used(data));
				
				for (uint16_t i = 0; i < data->params.state_filter.reject_consec_count; i++)
				{
					if (buffer_pos > 0)
					{
						buffer_pos--;
					}
					else
					{
						buffer_pos = SE_MAX_WINDOW_SIZE - 1;
					}

					// Must not be added yet
					ASSERT((data->state_filter.window[buffer_pos] & 0x8000) == 0);

					data->state_filter.window_sum += data->state_filter.window[buffer_pos];
					data->state_filter.window[buffer_pos] |= 0x8000; // mark as used (contributed to sum)
				}

				// Max value -> directly accept next value
				data->state_filter.above_reject_counter = 0xffff;
			}
			return;
		}
		else
		{
			// I => only increment counter
			data->state_filter.above_reject_counter++;
		}
	}
	else
	{
		/*
		 * Value is below threshold
		 * => Only need to reset the counter
		 */
		data->state_filter.above_reject_counter = 0;
	}

	// add 15 bit scaled value (without "contributed" tag)
	data->state_filter.window[buffer_pos] = currentval;
}


/*
 * Updates / changes the current state according to the transition matrix and the old state
 * Also check the end state timer.
 */
static void do_state_transition(state_estimation_data_t *data)
{
	ASSERT(data->state_filter.current_state < SE_STATECOUNT);

	/*
	 * This is the offset of the state transition values for the current state
	 */
	uint8_t row_offset = data->state_filter.current_state * SE_STATECOUNT;

	int16_t average = data->state_filter.window_sum / calc_current_window_used(data);

	ASSERT(average >= 0);
	

	for (uint8_t i = 0; i < SE_STATECOUNT; i++)
	{
		int16_t v = data->params.state_filter.transition_matrix[row_offset + i];

		if (v < 0)
		{
			// Switch to state i, if average < -v
			if (average < -v)
			{
				data->state_filter.current_state = i;
				break;
			}
		}
		else if (v > 0)
		{
			// Switch to state i, if average > v
			if (average > v)
			{
				data->state_filter.current_state = i;
				break;
			}
		}
	}

	// finally check max time condition for end state
	if (data->state_filter.current_state == SE_STATE_END)
	{
		data->state_filter.end_state_timer++;
		if (data->state_filter.end_state_timer > data->state_filter.max_end_state_time)
		{
			// Too long in end state => switch to OFF
			data->state_filter.end_state_timer = 0;
			data->state_filter.current_state = SE_STATE_OFF;
		}
	}
	else
	{
		data->state_filter.end_state_timer = 0;
	}
}


static void update_state_filter(state_estimation_data_t *data)
{
	adjust_window_size(data);
	update_reject_thd_filter(data);
	do_state_transition(data);
}


int stateest_init(state_estimation_data_t *data, const state_estimation_params_t *params, uint16_t adc_samples_per_sec)
{
	// check window sizes
	// NOTE: This is no real sanity check, it only prevents out-of-bounds memory access
	
	for (uint8_t i = 0; i < SE_STATECOUNT; i++)
	{
		if (params->state_filter.window_sizes[i] > SE_MAX_WINDOW_SIZE)
		{
			printf("Invalid stateest window size %u\n", params->state_filter.window_sizes[i]);
			return 1;
		}
	}

	// init data
	data->params = *params;
	data->input_filter.mid = SE_INITIAL_MID_VALUE;
	data->input_filter.current = 0;
	data->input_filter.counter = 0;

	memset(data->state_filter.window, 0, sizeof(data->state_filter.window));
	data->state_filter.window_next_free = 0;
	data->state_filter.window_oldest_valid = 0;
	data->state_filter.end_state_timer = 0;
	data->state_filter.max_end_state_time = (uint16_t)(((uint32_t)SE_MAX_END_STATE_TIME) * adc_samples_per_sec / params->input_filter.num_samples);
	data->state_filter.above_reject_counter = 0;
	data->state_filter.window_sum = 0;
	data->state_filter.current_state = SE_STATE_OFF;

	return 0;
}


state_update_result_t stateest_update(state_estimation_data_t *data, uint16_t raw_value)
{
	update_input_filter(data, raw_value);
	if (data->input_filter.counter >= data->params.input_filter.num_samples)
	{
		data->input_filter.counter = 0;

		uint8_t was_on = (data->state_filter.current_state >= SE_STATE_ON_THRESHOLD);
		
		update_state_filter(data);

		if (was_on != (data->state_filter.current_state >= SE_STATE_ON_THRESHOLD))
		{
			if (was_on)
			{
				return state_update_changed_to_off;
			}
			return state_update_changed_to_on;
		}
	}
	return state_update_unchanged;
}
