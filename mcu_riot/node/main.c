#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "thread.h"
#include "xtimer.h"
#include "shell.h"
#include "shell_commands.h"

#include "board.h"
#include "periph/rtc.h"

#include "sensor_node.h"
#include "sensor_config.h"

static const shell_command_t shell_commands[] = {
    { "cfg",     "Node configuration",                 sensor_config_set_cmd },
    { NULL, NULL, NULL }
};

int main(void)
{
	int sni = sensor_node_init();
	if (sni != 0)
	{
		printf("Sensor ndoe initialization failed with error %i\n", sni);
	}

    /* start the shell */
    puts("Initialization successful - starting the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
