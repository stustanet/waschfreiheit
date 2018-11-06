/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#include <stdbool.h>

enum COMMAND_QUEUE
{
	COMMAND_QUEUE_AUTOEXEC,
	COMMAND_QUEUE_TEST
};


/*
 * Sets the current queue and resets the position in that queue.
 */
bool debug_command_queue_load(enum COMMAND_QUEUE queue);

/*
 * Returns whether a command queue is currently running or not.
 */
bool debug_command_queue_running(void);

/*
 * Get the next command from the current queue.
 * Returns a pointer to a NUL-terminated buffer that remains valid unil
 * the next call to debug_command_queue_next.
 * The caller may modify the buffer.
 */
char *debug_command_queue_next(void);
