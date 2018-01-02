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
