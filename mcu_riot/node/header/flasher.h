#include <stdint.h>

/*
 * Starts the flashing procedure.
 * This function will never return.
 * When flashing is done or the command times out the CPU is reset.
 */
void flasher_start(uint32_t baudrate);
