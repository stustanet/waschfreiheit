/*
 * State estimation engine.
 *
 * This processes the raw ADC values and estimates the current machine state from it.
 */

#pragma once

#include <stdint.h>

// These two must be defines because they are used as array size
#define SE_MAX_WINDOW_SIZE                (512 * 3)
#define SE_STATECOUNT                            4
static const uint8_t SE_STATE_OFF =              0;
static const uint8_t SE_STATE_END =              1;
static const uint8_t SE_STATE_ON_THRESHOLD =     2; // >= 2 => ON
static const uint16_t SE_MAX_END_STATE_TIME = 1900; // 1900 sec

// Half the adc value (0x0800) shifted 20 bit to the left to get 32 bit scale
static const uint32_t SE_INITIAL_MID_VALUE = (0x800 << 20);

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
		 * Scale 15 bit
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
