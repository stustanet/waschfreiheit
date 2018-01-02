#pragma once

#include <stdio.h>

#ifdef ENABLE_ASSERT

#include "utils.h"

#define ASSERT(exp)     \
	if (!(exp))         \
	{                    \
		printf("ASSERT FAILED: " #exp " IN " __FILE__ ":" TOSTRING(__LINE__) "\n"); \
		(*(int *)0) = 0; \
	}
#else
#define ASSERT(exp)
#endif
