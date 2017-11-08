#pragma once

#include <stdio.h>

#ifdef ENABLE_ASSERT

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define ASSERT(exp)     \
	if (!(exp))         \
	{                    \
		printf("ASSERT FAILED: " #exp " IN " __FILE__ ":" TOSTRING(__LINE__) "\n"); \
		(*(int *)0) = 0; \
	}
#else
#define ASSERT(exp)
#endif
