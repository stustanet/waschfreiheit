/*
 * Copyright (C) 2015 TriaGnoSys GmbH
 *               2017 Alexander Kurth, Sören Tempel, Tristan Bruns
 *               2018 Daniel Frejek
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     boards_waschv1
 * @{
 *
 * @file
 * @brief       Waschfreiheit application board based on the bluepill board
 *
 * @author      Víctor Ariño <victor.arino@triagnosys.com>
 * @author      Sören Tempel <tempel@uni-bremen.de>
 * @author      Tristan Bruns <tbruns@uni-bremen.de>
 * @author      Alexander Kurth <kurth1@uni-bremen.de>
 * @author      Daniel Frejek <daniel.frejek@stusta.net>
 *
 * @}
 */

#include "board.h"
#include "periph/gpio.h"

void board_init(void)
{
    cpu_init();
    gpio_init(LED0_PIN, GPIO_OUT);
}
