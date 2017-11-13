/*
 * Logic of the master node / the gateway
 */

#pragma once

/*
 * Initializes the master node.
 */
int master_node_init(void);

// Commands

int master_node_cmd_connect(int argc, char **argv);
int master_node_cmd_node_routes(int argc, char **argv);
int master_node_cmd_configure_sensor(int argc, char **argv);
int master_node_cmd_enable_sensor(int argc, char **argv);
int master_node_cmd_raw_frames(int argc, char **argv);
int master_node_cmd_ping(int argc, char **argv);
int master_node_cmd_authping(int argc, char **argv);
int master_node_cmd_master_routes(int argc, char **argv);

