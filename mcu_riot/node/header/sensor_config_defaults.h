/*
 * Default values for the sensor node configuration.
 */


#pragma once
#include "rgbcolor.h"
#include "net/lora.h"

/*
 * This is the default color map for the leds.
 * It has to be exactly 16 elements long.
 */
#define DefaultColorMap {\
	{   0,   0,   0 },   \
	{ 255,   0,   0 },   \
	{   0, 255,   0 },   \
	{ 255, 255,   0 },   \
	{   0,   0, 255 },   \
	{ 255,   0, 255 },   \
	{   0, 255, 255 },   \
	{ 255, 255, 255 },   \
	{   0,   0,   0 },   \
	{  32,   0,   0 },   \
	{   0,  32,   0 },   \
	{  32,  32,   0 },   \
	{   0,   0,  32 },   \
	{  32,   0,  32 },   \
	{   0,  32,  32 },   \
	{  32,  32,  32 }    \
}

#define DefaultRFSettings               \
{                                       \
	433500000,      /* Frequency     */ \
	10,             /* TX power      */ \
	8,              /* Code rate     */ \
	2,              /* Spread factor */ \
	LORA_BW_125_KHZ /* Bandwidth     */ \
}
