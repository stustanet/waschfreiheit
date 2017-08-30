#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "thread.h"
#include "xtimer.h"
#include "shell.h"
#include "shell_commands.h"

#include "board.h"
#include "periph/rtc.h"

#include "meshnw.h"


static void mesh_message_reveived(void *data, uint8_t len)
{
	
}


static int send_cmd(int argc, char **argv)
{
    if (argc <= 1)
	{
        puts("usage: send <payload>");
        return -1;
    }

    printf("sending \"%s\" payload (%d bytes)\n",
           argv[1], strlen(argv[1]) + 1);


	meshnw_send(0, argv[1], strlen(argv[1]));

    return 0;
}


static const shell_command_t shell_commands[] = {
    { "send",     "Send raw payload string",                 send_cmd },
    { NULL, NULL, NULL }
};


int main(void)
{
	meshnw_init(0, &mesh_message_reveived);

    /* start the shell */
    puts("Initialization successful - starting the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
