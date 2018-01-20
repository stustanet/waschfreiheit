/*
 * This implements getchar() that reads a char from the USART1 using DMA.
 * This is required because the default getchar() implementation is unbuffered.
 * Therefore the shell will lose input if the current CPU load is too high.
 * The DMA comes with it's own 32-byte buffer. This will buffer any input, even if
 * getchar is not called.
 */

/*
 * Initialize DMA for getchar
 */
void serial_getchar_dma_init(void);
