/*
 * Copyright 2018 Daniel Frejek
 * This source code is licensed under the MIT license that can be found
 * in the LICENSE file.
 */


/*
 * Default values for the sensor node configuration.
 */


#pragma once
#include "rgbcolor.h"

/*
 * This is the default color map for the leds.
 * It has to be exactly 16 elements long.
 */
#define DefaultColorMap {\
	{   0,   0,   0 },   \
	{   0,  10,   0 },   \
	{  15,   7,   0 },   \
	{  15,   0,   0 },   \
	{   0,  50,   0 },   \
	{  75,  35,   0 },   \
	{  85,   0,   0 },   \
	{   0, 140,   0 },   \
	{ 200, 100,   0 },   \
	{ 255,   0,   0 },   \
	{   0,   0,  10 },   \
	{   0,   0, 255 },   \
	{  10,   0,  10 },   \
	{ 255,   0, 255 },   \
	{   0,  10,  10 },   \
	{   0, 255, 255 }    \
}

#define DefaultRFSettings               \
{                                       \
     .frequency = 433500000,            \
	 .tx_power = 10,                    \
	 .lora_spread_factor = 10,          \
	 .lora_coderate = 2,                \
	 .lora_bandwidth = 7                \
}

#define DefaultMiscSettings             \
{                                       \
	.network_timeout            = 1800,	\
	.max_status_retransmissions =  100, \
	.rt_delay_random            =   10, \
	.rt_delay_lin_div           =    3  \
}
