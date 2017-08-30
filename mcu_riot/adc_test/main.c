#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "thread.h"
#include "xtimer.h"
#include "periph/adc.h"

#include "board.h"
#include "periph/rtc.h"


void *adc_thread(void *arg)
{
	adc_init(0);
	//adc_init(1);

	//int counter = 0;
	//uint32_t estimates[2] = {3140 << 8, 3140 << 8};
	//uint32_t avg[2] = {3140 << 8, 3140 << 8};
	//uint64_t sums[2] = {0, 0};

	//int state_stable = 0;
	//int state_current = state_stable;
	//int state_transition_counter = 0;

    xtimer_ticks32_t last = xtimer_now();
    while (1) {


		uint32_t val = ((uint32_t)adc_sample(0, ADC_RES_12BIT));
		printf("%lu\n", val);
#if 0

		for (int i = 0; i < 2; i++)
		{
			uint32_t val = ((uint32_t)adc_sample(i, ADC_RES_12BIT)) << 8;

			// apply kalman filter
			//estimates[i] = (val + estimates[i] * 3) >> 2;
			estimates[i] = val;

			uint32_t delta = 0;

			if (estimates[i] > avg[i])
			{
				avg[i]++;
				delta = estimates[i] - avg[i];
			}
			else if (estimates[i] < avg[i])
			{
				avg[i]--;
				delta = avg[i] - estimates[i];
			}

			sums[i] += delta;
			
		}

		counter++;

		if (counter > 500)
		{
			// print values every second
			
			printf("a = %lu; b = %lu\n", (uint32_t)(sums[0] / 500) >> 6, (uint32_t)(sums[1] / 500) >> 6);

			/*
			int new_state = (((uint32_t)sums[0] / 500) >> 6) > 50;

			if (new_state != state_current)
			{
				state_transition_counter = 0;
				state_current = new_state;
			}

			if (state_current != state_stable)
			{
				state_transition_counter++;

				if (state_transition_counter > 3)
				{
					state_stable = state_current;
					state_transition_counter = 0;

					printf("Notify state change to: %i\n", state_current);

					char tmp[2];
					tmp[0] = '0' + state_stable;
					tmp[1] = 0;

					struct iovec vec[1];
					vec[0].iov_base = tmp;
					vec[0].iov_len = 2;
					if (netdev->driver->send(netdev, vec, 1) == -ENOTSUP) {
						puts("Cannot send: radio is still transmitting");
					}
				}
			}
			else
			{
				state_transition_counter = 0;
			}
			
			*/
			counter = 0;
			sums[0] = 0;
			sums[1] = 0;
		}
#endif

        xtimer_periodic_wakeup(&last, 2LU * US_PER_MS);
    }
}

int main(void)
{
	adc_thread(0);
	return 0;
}
