#include <stdio.h>

#define ENABLE_ASSERT

#ifdef ENABLE_ASSERT

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define ASSERT(exp)     \
	if (!(exp))         \
	{                    \
		printf("ASSERT FAILED: " #exp " IN " __FILE__ ":" TOSTRING(__LINE__) "\n"); \
		return;\
	}
#else
#define ASSERT(exp)
#endif
