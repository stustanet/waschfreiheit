/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


/*
 * Logic of a normal sensor / slave node.
 */

#pragma once

#define NUM_OF_SENSORS 2

#define SN_ERROR_NOT_CONFIGURED 1
#define SN_ERROR_THREAD_START   2
#define SN_ERROR_MESHNW_INIT    3

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


/*
 * Test command for LED testing
 */
int sensor_node_cmd_led(int argc, char **argv);

/*
 * Test command for sensor callibration
 */
int sensor_node_cmd_print_frames(int argc, char **argv);

/*
 * Print status information
 */
int sensor_node_cmd_print_status(int argc, char **argv);


#ifndef WASCHV2
/*
 * Firmware update (Flash) command
 */
int sensor_node_cmd_firmware_upgrade(int argc, char **argv);

#endif
