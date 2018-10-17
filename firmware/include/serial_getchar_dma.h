/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


/*
 * This provides serial_getchar() that reads a char from the USART1 using DMA.
 * The DMA comes with it's own 32-byte buffer. This will buffer any input, even if
 * serial getchar is not called.
 */

#include <stdint.h>

/*
 * Initialize DMA for getchar
 */
void serial_getchar_dma_init(void);

/*
 * Gets the next char from the input.
 * This function never waits, instead, if there is no next char, INT16_MIN is returned
 */
int16_t serial_getchar(void);
