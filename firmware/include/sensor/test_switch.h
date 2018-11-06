/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#include <stdbool.h>

enum TEST_SWITCH
{
	TEST_SWITCH_1,
	TEST_SWITCH_2,
	TEST_SWITCH_NUMOF
};

void test_switch_init(void);
bool test_switch_pressed(enum TEST_SWITCH sw);
