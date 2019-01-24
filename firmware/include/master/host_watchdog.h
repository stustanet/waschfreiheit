/*
 * Copyright 2019 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#pragma once


void host_watchdog_init(void);
void host_watchdog_feed_cmd(int argc, char **argv);
