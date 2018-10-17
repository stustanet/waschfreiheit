/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

typedef void (*cli_callback_t)(int, char **);

#define CLI_ARGC_MAX 16

typedef struct _cli_command
{
	const char *name;
	const char *desc;
	cli_callback_t cb;
} cli_command_t;

/*
 * Sets the command list to use.
 * The list is terminated by an entry with NULL as name.
 */
void cli_set_commandlist(const cli_command_t *list);

/*
 * Evaluates a command.
 */
void cli_evaluate(char *buffer);
