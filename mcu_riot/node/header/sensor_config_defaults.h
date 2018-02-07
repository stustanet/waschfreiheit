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
	433500000,      /* Frequency     */ \
	10,             /* TX power      */ \
	8,              /* Code rate     */ \
	2,              /* Spread factor */ \
	LORA_BW_125_KHZ /* Bandwidth     */ \
}
