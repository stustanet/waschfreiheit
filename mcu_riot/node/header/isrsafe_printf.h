/*
 * This offers a wrapper for printf that disables the interrupts before calling printf
 * and re-enbales them afterwards.
 * Obvoíously, this should only be used for short prints.
 */

#include "irq.h"
#define ISRSAFE_PRINTF(...)         \
{                                   \
	int state = irq_disable();      \
	printf(__VA_ARGS__);            \
	irq_restore(state);             \
}
