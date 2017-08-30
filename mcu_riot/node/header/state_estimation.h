/*
 * State estimation engine.
 *
 * This reads the value from the specified ADC and estimates the current machine state from it.
 */

#include "periph/adc.h"


struct state_estimation_data
{
	adc_t channel;
	/*
	uint32_t mid;
	uint32_t counter;
	uint8_t state_stable;
	uint8_t state_last;
	uint8_t state_change_counter;
	*/
};

enum state_update_result
{
	state_update_unchanged,
	state_update_changed_to_off,
	state_update_changed_to_on
};

typedef struct state_estimation_data state_estimation_data_t;
typedef enum state_update_result state_update_result_t;

int stateest_init(state_estimation_data_t *, adc_t channel);

state_update_result_t stateest_update(state_estimation_data_t *);

