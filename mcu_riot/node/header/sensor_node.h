/*
 * Logic of a normal sensor / slave node.
 */

#pragma once

#define NUM_OF_SENSORS 2

#define SN_ERROR_NOT_CONFIGURED 1
#define SN_ERROR_THREAD_START   2

/*
 * Initializes the sensor / slave node.
 *
 * This starts the thread for ADC measuring and initializes the mesh network
 */
int sensor_node_init(void);

/*
 * Test command to read / print raw adc values
 */
int sensor_node_cmd_raw(int argc, char **argv);
