#include "state_estimation.h"

static state_estimation_data_t se_data;

int se_init()
{
	static state_estimation_params_t se_params;

	se_params.input_filter.lowpass_weight = 50;
	se_params.input_filter.num_samples = 100;
	se_params.input_filter.mid_value_adjustment_speed = 20;

	se_params.state_filter.reject_consec_count = 15;
	se_params.state_filter.reject_threshold = 13 * 8;

	se_params.state_filter.window_sizes[0] = 150;
	se_params.state_filter.window_sizes[1] = 1500;
	se_params.state_filter.window_sizes[2] = 1500;
	se_params.state_filter.window_sizes[3] = 1500;

	se_params.state_filter.transition_matrix[0 + 0 * (SE_STATECOUNT - 1)] = 0;
	se_params.state_filter.transition_matrix[1 + 0 * (SE_STATECOUNT - 1)] = 80;
	se_params.state_filter.transition_matrix[2 + 0 * (SE_STATECOUNT - 1)] = 0;

	se_params.state_filter.transition_matrix[0 + 1 * (SE_STATECOUNT - 1)] = -24;
	se_params.state_filter.transition_matrix[1 + 1 * (SE_STATECOUNT - 1)] = 0;
	se_params.state_filter.transition_matrix[2 + 1 * (SE_STATECOUNT - 1)] = 160;

	se_params.state_filter.transition_matrix[0 + 2 * (SE_STATECOUNT - 1)] = -48;
	se_params.state_filter.transition_matrix[1 + 2 * (SE_STATECOUNT - 1)] = 0;
	se_params.state_filter.transition_matrix[2 + 2 * (SE_STATECOUNT - 1)] = 160;

	se_params.state_filter.transition_matrix[0 + 3 * (SE_STATECOUNT - 1)] = 0;
	se_params.state_filter.transition_matrix[1 + 3 * (SE_STATECOUNT - 1)] = -80;
	se_params.state_filter.transition_matrix[2 + 3 * (SE_STATECOUNT - 1)] = 0;

	return stateest_init(&se_data, &se_params, 500);
}

int se_pushval(int val)
{
	return stateest_update(&se_data, val);
}

int se_currentstate()
{
	return se_data.state_filter.current_state;
}

int se_current()
{
	return se_data.input_filter.current >> 3;
}

int se_is_frame()
{
	return se_data.input_filter.counter == 0;
}

int se_avg()
{
	uint16_t s;
	if (se_data.state_filter.window_oldest_valid > se_data.state_filter.window_next_free)
	{
		s = se_data.state_filter.window_next_free + (SE_MAX_WINDOW_SIZE - se_data.state_filter.window_oldest_valid);
	}
	else
	{
		s = se_data.state_filter.window_next_free - se_data.state_filter.window_oldest_valid;
	}
	return se_data.state_filter.window_sum / s;
}
