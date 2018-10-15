/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <thread.h>
#include <shell.h>
#include <shell_commands.h>

#include <board.h>
#include <periph/rtc.h>

#include "utils.h"
#include "serial_getchar_dma.h"


int main(void)
{
	serial_getchar_dma_init();
	init();

	char line_buf[256];
	shell_run(shell_commands, line_buf, sizeof(line_buf));

	return 0;
}
