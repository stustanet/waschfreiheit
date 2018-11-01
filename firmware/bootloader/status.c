/*
 * Copyright (c) 2018 Daniel Frejek.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "status.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "i2s_rgb.h"


/*
 * LED colors for every status.
 * The colors are encoded into 16 bit, where each bit stands for a color of an LED.
 * LSB    : green of first LED
 * LSB + 1: red of first LED
 * LSB + 2: blue of first LED
 * LSB + 3: red of second LED
 * ...
 * red and blue of the last LED are always 0.
 *
 * 5444 3332 2211 1000
 * GBRG BRGB RGBR GBRG
 */
uint32_t STATUS_COLORS [] =
{
	0xa9bf, // Init
	0x0003, // Wait for USB
	0x0002, // USB timeout
	0x0001, // USB detected
	0x0012, // USB mount error
	0x001a, // No checksum file
	0x0022, // No firmware file
	0x003a, // image too large
	0x0091, // wrong checksum
	0x003f, // same firmware
	0xa497, // strorage error
	0x2492, // verification error
	0x9241, // success
	0x0007, // jump to app
};

_Static_assert(I2RGB_NUM_OF_LEDS == 6, "Expected to have 6 LEDs");
_Static_assert(I2RGB_BYTES_PER_LED == 3, "Expected to have 3 bytes per LED");

void status_init(void)
{
	i2s_rgb_init();
	status_update(STATUS_INIT);
}


void status_update(enum STATUS_TYPE status)
{
	uint8_t buffer[I2RGB_NUM_OF_LEDS * I2RGB_BYTES_PER_LED];

	if (status >= sizeof(STATUS_COLORS) / sizeof(STATUS_COLORS[0]))
	{
		return;
	}

	uint16_t status_code = STATUS_COLORS[status];

	// Decode status code
	for (size_t i = 0; i < sizeof(buffer); i++)
	{
		if (status_code & 0x01)
		{
			buffer[i] = 0x0f;
		}
		else
		{
			buffer[i] = 0x00;
		}
		status_code = status_code >> 1;
	}

	i2s_rgb_set(buffer);
}


void status_progress(enum PROGRESS_TYPE type, int percent)
{
	uint8_t buffer[I2RGB_NUM_OF_LEDS * I2RGB_BYTES_PER_LED];
	memset(buffer, 0, sizeof(buffer));

	static uint8_t toggle = 0;

	// Fisrt LED blinks green
	toggle = ~toggle;
	if (toggle)
	{
		buffer[1] = 0x0f;
	}

	for (int i = 0; i < 5; i++)
	{
		// First LED is always on
		// Others: 20%, 40%, 60%, 80%
		if (i * 20 > percent)
		{
			continue;
		}
		switch (type)
		{
			case PROGRESS_READ:
				buffer[i * 3 + 3] = 0x0f;
				break;

			case PROGRESS_ERASE:
				buffer[i * 3 + 4] = 0x0f;
				buffer[i * 3 + 5] = 0x0f;
				break;

			case PROGRESS_WRITE:
				buffer[i * 3 + 4] = 0x0f;
				break;

			case PROGRESS_VERIFY:
				buffer[i * 3 + 3] = 0x0f;
				buffer[i * 3 + 4] = 0x0f;
				break;
		}
	}

	i2s_rgb_set(buffer);
}
