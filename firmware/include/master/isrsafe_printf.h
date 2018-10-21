/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


/*
 * This offers a wrapper for printf that disables the interrupts before calling printf
 * and re-enbales them afterwards.
 * Obvoíously, this should only be used for short prints.
 */

#include <libopencm3/cm3/cortex.h>
#define ISRSAFE_PRINTF(...)         \
{                                   \
	cm_disable_interrupts();        \
	printf(__VA_ARGS__);            \
	cm_enable_interrupts();         \
}
