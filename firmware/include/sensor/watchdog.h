/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


/*
 * Interface for the IWDG of a STM32F1 CPU.
 *
 * This watchdog runs completely independent of the CPU. Once started, it can not be
 * turned off until the system is reset.
 */

#include <libopencm3/stm32/iwdg.h>

#define WATCHDOG_KEY_UNLOCK 0x5555
#define WATCHDOG_KEY_FEED   0xAAAA
#define WATCHDOG_KEY_START  0xCCCC


/*
 * Feed the IWDG by writing AAAA into the key register
 */
#define WATCHDOG_FEED() IWDG_KR = WATCHDOG_KEY_FEED

/*
 * Init the hardware watchdog with a 32 prescaler and the maximum reload value (4095).
 * This results in a timeout of ~ 4sec.
 */
static inline void watchdog_init(void)
{
	/*
	 * In order to access these registers, I need to unlock them first.
	 */
	IWDG_KR = WATCHDOG_KEY_UNLOCK;
	IWDG_PR = 3;      // 32 Prescaler
	IWDG_RLR = 0xFFF; // 4095, the max value

	// Now feed the WD, this re-locks the registers.
	WATCHDOG_FEED();

	// Finally start the WD
	IWDG_KR = WATCHDOG_KEY_START;

}
