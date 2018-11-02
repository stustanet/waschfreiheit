/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */

#include "cli.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "tinyprintf.h"

static const cli_command_t *command_list = NULL;


// Splits a command into parts
// Returns the number of parts
// parts must me at least CLI_ARGC_MAX long
static int split_command(char *buffer, char **parts)
{
	char par = 0;
	bool cur = false;

	int shift = 0;
	int argc = 0;

	while (*buffer && argc < CLI_ARGC_MAX)
	{
		if (*buffer == '\\')
		{
			buffer++;
			shift--;
			if (!(*buffer))
			{
				break;
			}
		}
		else if ((*buffer == '\"' ||
				  *buffer == '\'') &&
				 (par == 0 || par == *buffer))
		{
			if (par)
			{
				// end of group
				par = 0;
			}
			else
			{
				// start of a group
				par = *buffer;
			}
			shift--;
			buffer++;
			continue;
		}
		else if (!par && (*buffer == ' ' || *buffer == '\t'))
		{
			if (cur)
			{
				// end of an arg
				buffer[shift] = 0;
				cur = false;
				argc++;
			}
			// else: ignore space ouside of command

			buffer++;
			continue;
		}

		// Normal char
		if (!cur)
		{
			// start of a new arg
			shift = 0;
			parts[argc] = buffer;
			cur = true;
		}
		else if (shift != 0)
		{
			buffer[shift] = buffer[0];
		}

		buffer++;
	}

	if (cur)
	{
		// end last arg
		buffer[shift] = 0;
		argc++;
	}

	return argc;
}


static void print_help(void)
{
	printf("COMMAND LIST\n"
		   "-------------------------------------------------\n");

	for (size_t idx = 0; command_list[idx].name != 0; idx++)
	{
		printf("%s \t%s\n", command_list[idx].name, command_list[idx].desc);
	}
}


void cli_set_commandlist(const cli_command_t *list)
{
	command_list = list;
}


void cli_evaluate(char *buffer)
{
	if (!command_list)
	{
		return;
	}

	char *argv[CLI_ARGC_MAX];
	int argc = split_command(buffer, argv);

	if (argc == 0)
	{
		return;
	}

	if (strcmp("help", argv[0]) == 0)
	{
		print_help();
		return;
	}

	// Find the command
	for (size_t idx = 0; command_list[idx].name != 0; idx++)
	{
		if (strcmp(command_list[idx].name, argv[0]) == 0)
		{
			command_list[idx].cb(argc, argv);
			return;
		}
	}

	printf("Unknown command \"%s\"\n", argv[0]);
}
