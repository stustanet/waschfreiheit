/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


/*
 * Logic of the master node / the gateway
 */

#pragma once

/*
 * Initializes the master node.
 */
int master_node_init(void);

// Commands

void master_node_cmd_connect(int argc, char **argv);
void master_node_cmd_retransmit(int argc, char **argv);
void master_node_cmd_node_routes(int argc, char **argv);
void master_node_cmd_configure_sensor(int argc, char **argv);
void master_node_cmd_enable_sensor(int argc, char **argv);
void master_node_cmd_raw_frames(int argc, char **argv);
void master_node_cmd_raw_status(int argc, char **argv);
void master_node_cmd_authping(int argc, char **argv);
void master_node_cmd_led(int argc, char **argv);
void master_node_cmd_rebuild_status_channel(int argc, char **argv);
void master_node_cmd_configure_status_change_indicator(int argc, char **argv);
void master_node_cmd_configure_freq_sensor(int argc, char **argv);
void master_node_cmd_storage_ctl(int argc, char **argv);
