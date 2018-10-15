/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


#include <stdint.h>

/*
 * Starts the flashing procedure.
 * This function will never return.
 * When flashing is done or the command times out the CPU is reset.
 */
void flasher_start(uint32_t baudrate);
