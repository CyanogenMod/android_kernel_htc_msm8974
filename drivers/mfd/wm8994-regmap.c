/*
 * wm8994-regmap.c  --  Register map data for WM8994 series devices
 *
 * Copyright 2011 Wolfson Microelectronics PLC.
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/mfd/wm8994/core.h>
#include <linux/mfd/wm8994/registers.h>
#include <linux/regmap.h>
#include <linux/device.h>

#include "wm8994.h"

static struct reg_default wm1811_defaults[] = {
	{ 0x0001, 0x0000 },    
	{ 0x0002, 0x6000 },    
	{ 0x0003, 0x0000 },    
	{ 0x0004, 0x0000 },    
	{ 0x0005, 0x0000 },    
	{ 0x0006, 0x0000 },    
	{ 0x0015, 0x0000 },    
	{ 0x0018, 0x008B },    
	{ 0x0019, 0x008B },    
	{ 0x001A, 0x008B },    
	{ 0x001B, 0x008B },    
	{ 0x001C, 0x006D },    
	{ 0x001D, 0x006D },    
	{ 0x001E, 0x0066 },    
	{ 0x001F, 0x0020 },    
	{ 0x0020, 0x0079 },    
	{ 0x0021, 0x0079 },    
	{ 0x0022, 0x0003 },    
	{ 0x0023, 0x0003 },    
	{ 0x0024, 0x0011 },    
	{ 0x0025, 0x0140 },    
	{ 0x0026, 0x0079 },    
	{ 0x0027, 0x0079 },    
	{ 0x0028, 0x0000 },    
	{ 0x0029, 0x0000 },    
	{ 0x002A, 0x0000 },    
	{ 0x002B, 0x0000 },    
	{ 0x002C, 0x0000 },    
	{ 0x002D, 0x0000 },    
	{ 0x002E, 0x0000 },    
	{ 0x002F, 0x0000 },    
	{ 0x0030, 0x0000 },    
	{ 0x0031, 0x0000 },    
	{ 0x0032, 0x0000 },    
	{ 0x0033, 0x0000 },    
	{ 0x0034, 0x0000 },    
	{ 0x0035, 0x0000 },    
	{ 0x0036, 0x0000 },    
	{ 0x0037, 0x0000 },    
	{ 0x0038, 0x0000 },    
	{ 0x0039, 0x0000 },    
	{ 0x003B, 0x000D },    
	{ 0x003C, 0x0003 },    
	{ 0x003D, 0x0039 },    
	{ 0x003E, 0x0039 },    
	{ 0x004C, 0x1F25 },    
	{ 0x004D, 0xAB19 },    
	{ 0x0051, 0x0004 },    
	{ 0x0055, 0x054A },    
	{ 0x0059, 0x0000 },    
	{ 0x0060, 0x0000 },    
	{ 0x00C5, 0x0000 },    
	{ 0x00D0, 0x7600 },    
	{ 0x00D1, 0x007F },    
	{ 0x0101, 0x8004 },    
	{ 0x0200, 0x0000 },    
	{ 0x0201, 0x0000 },    
	{ 0x0204, 0x0000 },    
	{ 0x0205, 0x0000 },    
	{ 0x0208, 0x0000 },    
	{ 0x0209, 0x0000 },    
	{ 0x0210, 0x0083 },    
	{ 0x0211, 0x0083 },    
	{ 0x0220, 0x0000 },    
	{ 0x0221, 0x0000 },    
	{ 0x0222, 0x0000 },    
	{ 0x0223, 0x0000 },    
	{ 0x0224, 0x0C80 },    
	{ 0x0226, 0x0000 },    
	{ 0x0227, 0x0006 },    
	{ 0x0240, 0x0000 },    
	{ 0x0241, 0x0000 },    
	{ 0x0242, 0x0000 },    
	{ 0x0243, 0x0000 },    
	{ 0x0244, 0x0C80 },    
	{ 0x0246, 0x0000 },    
	{ 0x0247, 0x0006 },    
	{ 0x0300, 0x4050 },    
	{ 0x0301, 0x4000 },    
	{ 0x0302, 0x0000 },    
	{ 0x0303, 0x0040 },    
	{ 0x0304, 0x0040 },    
	{ 0x0305, 0x0040 },    
	{ 0x0306, 0x0004 },    
	{ 0x0307, 0x0100 },    
	{ 0x0310, 0x4050 },    
	{ 0x0311, 0x4000 },    
	{ 0x0312, 0x0000 },    
	{ 0x0313, 0x0040 },    
	{ 0x0314, 0x0040 },    
	{ 0x0315, 0x0040 },    
	{ 0x0316, 0x0000 },    
	{ 0x0317, 0x0000 },    
	{ 0x0318, 0x0003 },    
	{ 0x0320, 0x0040 },    
	{ 0x0321, 0x0000 },    
	{ 0x0322, 0x0000 },    
	{ 0x0323, 0x0000 },    
	{ 0x0400, 0x00C0 },    
	{ 0x0401, 0x00C0 },    
	{ 0x0402, 0x00C0 },    
	{ 0x0403, 0x00C0 },    
	{ 0x0410, 0x0000 },    
	{ 0x0420, 0x0200 },    
	{ 0x0421, 0x0010 },    
	{ 0x0430, 0x0068 },    
	{ 0x0440, 0x0098 },    
	{ 0x0441, 0x0845 },    
	{ 0x0442, 0x0000 },    
	{ 0x0443, 0x0000 },    
	{ 0x0444, 0x0000 },    
	{ 0x0480, 0x6318 },    
	{ 0x0481, 0x6300 },    
	{ 0x0482, 0x0FCA },    
	{ 0x0483, 0x0400 },    
	{ 0x0484, 0x00D8 },    
	{ 0x0485, 0x1EB5 },    
	{ 0x0486, 0xF145 },    
	{ 0x0487, 0x0B75 },    
	{ 0x0488, 0x01C5 },    
	{ 0x0489, 0x1C58 },    
	{ 0x048A, 0xF373 },    
	{ 0x048B, 0x0A54 },    
	{ 0x048C, 0x0558 },    
	{ 0x048D, 0x168E },    
	{ 0x048E, 0xF829 },    
	{ 0x048F, 0x07AD },    
	{ 0x0490, 0x1103 },    
	{ 0x0491, 0x0564 },    
	{ 0x0492, 0x0559 },    
	{ 0x0493, 0x4000 },    
	{ 0x0494, 0x0000 },    
	{ 0x0500, 0x00C0 },    
	{ 0x0501, 0x00C0 },    
	{ 0x0502, 0x00C0 },    
	{ 0x0503, 0x00C0 },    
	{ 0x0510, 0x0000 },    
	{ 0x0520, 0x0200 },    
	{ 0x0521, 0x0010 },    
	{ 0x0530, 0x0068 },    
	{ 0x0540, 0x0098 },    
	{ 0x0541, 0x0845 },    
	{ 0x0542, 0x0000 },    
	{ 0x0543, 0x0000 },    
	{ 0x0544, 0x0000 },    
	{ 0x0580, 0x6318 },    
	{ 0x0581, 0x6300 },    
	{ 0x0582, 0x0FCA },    
	{ 0x0583, 0x0400 },    
	{ 0x0584, 0x00D8 },    
	{ 0x0585, 0x1EB5 },    
	{ 0x0586, 0xF145 },    
	{ 0x0587, 0x0B75 },    
	{ 0x0588, 0x01C5 },    
	{ 0x0589, 0x1C58 },    
	{ 0x058A, 0xF373 },    
	{ 0x058B, 0x0A54 },    
	{ 0x058C, 0x0558 },    
	{ 0x058D, 0x168E },    
	{ 0x058E, 0xF829 },    
	{ 0x058F, 0x07AD },    
	{ 0x0590, 0x1103 },    
	{ 0x0591, 0x0564 },    
	{ 0x0592, 0x0559 },    
	{ 0x0593, 0x4000 },    
	{ 0x0594, 0x0000 },    
	{ 0x0600, 0x0000 },    
	{ 0x0601, 0x0000 },    
	{ 0x0602, 0x0000 },    
	{ 0x0603, 0x0000 },    
	{ 0x0604, 0x0000 },    
	{ 0x0605, 0x0000 },    
	{ 0x0606, 0x0000 },    
	{ 0x0607, 0x0000 },    
	{ 0x0610, 0x02C0 },    
	{ 0x0611, 0x02C0 },    
	{ 0x0612, 0x02C0 },    
	{ 0x0613, 0x02C0 },    
	{ 0x0614, 0x0000 },    
	{ 0x0620, 0x0002 },    
	{ 0x0621, 0x0000 },    
	{ 0x0700, 0x8100 },    
	{ 0x0701, 0xA101 },    
	{ 0x0702, 0xA101 },    
	{ 0x0703, 0xA101 },    
	{ 0x0704, 0xA101 },    
	{ 0x0707, 0xA101 },    
	{ 0x0708, 0xA101 },    
	{ 0x0709, 0xA101 },    
	{ 0x070A, 0xA101 },    
	{ 0x0720, 0x0000 },    
	{ 0x0721, 0x0156 },    
	{ 0x0732, 0x0000 },    
	{ 0x0738, 0x07FF },    
	{ 0x0739, 0xDFEF },    
	{ 0x0740, 0x0000 },    
	{ 0x0748, 0x003F },    
};

static struct reg_default wm8994_defaults[] = {
	{ 0x0001, 0x0000 },     
	{ 0x0002, 0x6000 },     
	{ 0x0003, 0x0000 },     
	{ 0x0004, 0x0000 },     
	{ 0x0005, 0x0000 },     
	{ 0x0006, 0x0000 },     
	{ 0x0015, 0x0000 },     
	{ 0x0018, 0x008B },     
	{ 0x0019, 0x008B },     
	{ 0x001A, 0x008B },     
	{ 0x001B, 0x008B },     
	{ 0x001C, 0x006D },     
	{ 0x001D, 0x006D },     
	{ 0x001E, 0x0066 },     
	{ 0x001F, 0x0020 },     
	{ 0x0020, 0x0079 },     
	{ 0x0021, 0x0079 },     
	{ 0x0022, 0x0003 },     
	{ 0x0023, 0x0003 },     
	{ 0x0024, 0x0011 },     
	{ 0x0025, 0x0140 },     
	{ 0x0026, 0x0079 },     
	{ 0x0027, 0x0079 },     
	{ 0x0028, 0x0000 },     
	{ 0x0029, 0x0000 },     
	{ 0x002A, 0x0000 },     
	{ 0x002B, 0x0000 },     
	{ 0x002C, 0x0000 },     
	{ 0x002D, 0x0000 },     
	{ 0x002E, 0x0000 },     
	{ 0x002F, 0x0000 },     
	{ 0x0030, 0x0000 },     
	{ 0x0031, 0x0000 },     
	{ 0x0032, 0x0000 },     
	{ 0x0033, 0x0000 },     
	{ 0x0034, 0x0000 },     
	{ 0x0035, 0x0000 },     
	{ 0x0036, 0x0000 },     
	{ 0x0037, 0x0000 },     
	{ 0x0038, 0x0000 },     
	{ 0x0039, 0x0000 },     
	{ 0x003A, 0x0000 },     
	{ 0x003B, 0x000D },     
	{ 0x003C, 0x0003 },     
	{ 0x004C, 0x1F25 },     
	{ 0x0051, 0x0004 },     
	{ 0x0055, 0x054A },     
	{ 0x0057, 0x0000 },     
	{ 0x0060, 0x0000 },     
	{ 0x0101, 0x8004 },     
	{ 0x0110, 0x0000 },     
	{ 0x0111, 0x0000 },     
	{ 0x0200, 0x0000 },     
	{ 0x0201, 0x0000 },     
	{ 0x0204, 0x0000 },     
	{ 0x0205, 0x0000 },     
	{ 0x0208, 0x0000 },     
	{ 0x0209, 0x0000 },     
	{ 0x0210, 0x0083 },     
	{ 0x0211, 0x0083 },     
	{ 0x0220, 0x0000 },     
	{ 0x0221, 0x0000 },     
	{ 0x0222, 0x0000 },     
	{ 0x0223, 0x0000 },     
	{ 0x0224, 0x0C80 },     
	{ 0x0240, 0x0000 },     
	{ 0x0241, 0x0000 },     
	{ 0x0242, 0x0000 },     
	{ 0x0243, 0x0000 },     
	{ 0x0244, 0x0C80 },     
	{ 0x0300, 0x4050 },     
	{ 0x0301, 0x4000 },     
	{ 0x0302, 0x0000 },     
	{ 0x0303, 0x0040 },     
	{ 0x0304, 0x0040 },     
	{ 0x0305, 0x0040 },     
	{ 0x0306, 0x0004 },     
	{ 0x0307, 0x0100 },     
	{ 0x0310, 0x4050 },     
	{ 0x0311, 0x4000 },     
	{ 0x0312, 0x0000 },     
	{ 0x0313, 0x0040 },     
	{ 0x0314, 0x0040 },     
	{ 0x0315, 0x0040 },     
	{ 0x0316, 0x0000 },     
	{ 0x0317, 0x0000 },     
	{ 0x0400, 0x00C0 },     
	{ 0x0401, 0x00C0 },     
	{ 0x0402, 0x00C0 },     
	{ 0x0403, 0x00C0 },     
	{ 0x0404, 0x00C0 },     
	{ 0x0405, 0x00C0 },     
	{ 0x0406, 0x00C0 },     
	{ 0x0407, 0x00C0 },     
	{ 0x0410, 0x0000 },     
	{ 0x0411, 0x0000 },     
	{ 0x0420, 0x0200 },     
	{ 0x0421, 0x0010 },     
	{ 0x0422, 0x0200 },     
	{ 0x0423, 0x0010 },     
	{ 0x0440, 0x0098 },     
	{ 0x0441, 0x0845 },     
	{ 0x0442, 0x0000 },     
	{ 0x0443, 0x0000 },     
	{ 0x0444, 0x0000 },     
	{ 0x0450, 0x0098 },     
	{ 0x0451, 0x0845 },     
	{ 0x0452, 0x0000 },     
	{ 0x0453, 0x0000 },     
	{ 0x0454, 0x0000 },     
	{ 0x0480, 0x6318 },     
	{ 0x0481, 0x6300 },     
	{ 0x0482, 0x0FCA },     
	{ 0x0483, 0x0400 },     
	{ 0x0484, 0x00D8 },     
	{ 0x0485, 0x1EB5 },     
	{ 0x0486, 0xF145 },     
	{ 0x0487, 0x0B75 },     
	{ 0x0488, 0x01C5 },     
	{ 0x0489, 0x1C58 },     
	{ 0x048A, 0xF373 },     
	{ 0x048B, 0x0A54 },     
	{ 0x048C, 0x0558 },     
	{ 0x048D, 0x168E },     
	{ 0x048E, 0xF829 },     
	{ 0x048F, 0x07AD },     
	{ 0x0490, 0x1103 },     
	{ 0x0491, 0x0564 },     
	{ 0x0492, 0x0559 },     
	{ 0x0493, 0x4000 },     
	{ 0x04A0, 0x6318 },     
	{ 0x04A1, 0x6300 },     
	{ 0x04A2, 0x0FCA },     
	{ 0x04A3, 0x0400 },     
	{ 0x04A4, 0x00D8 },     
	{ 0x04A5, 0x1EB5 },     
	{ 0x04A6, 0xF145 },     
	{ 0x04A7, 0x0B75 },     
	{ 0x04A8, 0x01C5 },     
	{ 0x04A9, 0x1C58 },     
	{ 0x04AA, 0xF373 },     
	{ 0x04AB, 0x0A54 },     
	{ 0x04AC, 0x0558 },     
	{ 0x04AD, 0x168E },     
	{ 0x04AE, 0xF829 },     
	{ 0x04AF, 0x07AD },     
	{ 0x04B0, 0x1103 },     
	{ 0x04B1, 0x0564 },     
	{ 0x04B2, 0x0559 },     
	{ 0x04B3, 0x4000 },     
	{ 0x0500, 0x00C0 },     
	{ 0x0501, 0x00C0 },     
	{ 0x0502, 0x00C0 },     
	{ 0x0503, 0x00C0 },     
	{ 0x0510, 0x0000 },     
	{ 0x0520, 0x0200 },     
	{ 0x0521, 0x0010 },     
	{ 0x0540, 0x0098 },     
	{ 0x0541, 0x0845 },     
	{ 0x0542, 0x0000 },     
	{ 0x0543, 0x0000 },     
	{ 0x0544, 0x0000 },     
	{ 0x0580, 0x6318 },     
	{ 0x0581, 0x6300 },     
	{ 0x0582, 0x0FCA },     
	{ 0x0583, 0x0400 },     
	{ 0x0584, 0x00D8 },     
	{ 0x0585, 0x1EB5 },     
	{ 0x0586, 0xF145 },     
	{ 0x0587, 0x0B75 },     
	{ 0x0588, 0x01C5 },     
	{ 0x0589, 0x1C58 },     
	{ 0x058A, 0xF373 },     
	{ 0x058B, 0x0A54 },     
	{ 0x058C, 0x0558 },     
	{ 0x058D, 0x168E },     
	{ 0x058E, 0xF829 },     
	{ 0x058F, 0x07AD },     
	{ 0x0590, 0x1103 },     
	{ 0x0591, 0x0564 },     
	{ 0x0592, 0x0559 },     
	{ 0x0593, 0x4000 },     
	{ 0x0600, 0x0000 },     
	{ 0x0601, 0x0000 },     
	{ 0x0602, 0x0000 },     
	{ 0x0603, 0x0000 },     
	{ 0x0604, 0x0000 },     
	{ 0x0605, 0x0000 },     
	{ 0x0606, 0x0000 },     
	{ 0x0607, 0x0000 },     
	{ 0x0608, 0x0000 },     
	{ 0x0609, 0x0000 },     
	{ 0x0610, 0x02C0 },     
	{ 0x0611, 0x02C0 },     
	{ 0x0612, 0x02C0 },     
	{ 0x0613, 0x02C0 },     
	{ 0x0614, 0x0000 },     
	{ 0x0620, 0x0002 },     
	{ 0x0621, 0x0000 },     
	{ 0x0700, 0x8100 },     
	{ 0x0701, 0xA101 },     
	{ 0x0702, 0xA101 },     
	{ 0x0703, 0xA101 },     
	{ 0x0704, 0xA101 },     
	{ 0x0705, 0xA101 },     
	{ 0x0706, 0xA101 },     
	{ 0x0707, 0xA101 },     
	{ 0x0708, 0xA101 },     
	{ 0x0709, 0xA101 },     
	{ 0x070A, 0xA101 },     
	{ 0x0720, 0x0000 },     
	{ 0x0721, 0x0156 },     
	{ 0x0738, 0x07FF },     
	{ 0x0739, 0xFFFF },     
	{ 0x0740, 0x0000 },     
	{ 0x0748, 0x003F },     
};

static struct reg_default wm8958_defaults[] = {
	{ 0x0001, 0x0000 },    
	{ 0x0002, 0x6000 },    
	{ 0x0003, 0x0000 },    
	{ 0x0004, 0x0000 },    
	{ 0x0005, 0x0000 },    
	{ 0x0006, 0x0000 },    
	{ 0x0015, 0x0000 },    
	{ 0x0018, 0x008B },    
	{ 0x0019, 0x008B },    
	{ 0x001A, 0x008B },    
	{ 0x001B, 0x008B },    
	{ 0x001C, 0x006D },    
	{ 0x001D, 0x006D },    
	{ 0x001E, 0x0066 },    
	{ 0x001F, 0x0020 },    
	{ 0x0020, 0x0079 },    
	{ 0x0021, 0x0079 },    
	{ 0x0022, 0x0003 },    
	{ 0x0023, 0x0003 },    
	{ 0x0024, 0x0011 },    
	{ 0x0025, 0x0140 },    
	{ 0x0026, 0x0079 },    
	{ 0x0027, 0x0079 },    
	{ 0x0028, 0x0000 },    
	{ 0x0029, 0x0000 },    
	{ 0x002A, 0x0000 },    
	{ 0x002B, 0x0000 },    
	{ 0x002C, 0x0000 },    
	{ 0x002D, 0x0000 },    
	{ 0x002E, 0x0000 },    
	{ 0x002F, 0x0000 },    
	{ 0x0030, 0x0000 },    
	{ 0x0031, 0x0000 },    
	{ 0x0032, 0x0000 },    
	{ 0x0033, 0x0000 },    
	{ 0x0034, 0x0000 },    
	{ 0x0035, 0x0000 },    
	{ 0x0036, 0x0000 },    
	{ 0x0037, 0x0000 },    
	{ 0x0038, 0x0000 },    
	{ 0x0039, 0x0180 },    
	{ 0x003B, 0x000D },    
	{ 0x003C, 0x0005 },    
	{ 0x003D, 0x0039 },    
	{ 0x003E, 0x0039 },    
	{ 0x004C, 0x1F25 },    
	{ 0x004D, 0xAB19 },    
	{ 0x0051, 0x0004 },    
	{ 0x0055, 0x054A },    
	{ 0x0057, 0x0000 },    
	{ 0x0060, 0x0000 },    
	{ 0x00C5, 0x0000 },    
	{ 0x00D0, 0x5600 },    
	{ 0x00D1, 0x007F },    
	{ 0x0101, 0x8004 },    
	{ 0x0110, 0x0000 },    
	{ 0x0111, 0x0000 },    
	{ 0x0200, 0x0000 },    
	{ 0x0201, 0x0000 },    
	{ 0x0204, 0x0000 },    
	{ 0x0205, 0x0000 },    
	{ 0x0208, 0x0000 },    
	{ 0x0209, 0x0000 },    
	{ 0x0210, 0x0083 },    
	{ 0x0211, 0x0083 },    
	{ 0x0220, 0x0000 },    
	{ 0x0221, 0x0000 },    
	{ 0x0222, 0x0000 },    
	{ 0x0223, 0x0000 },    
	{ 0x0224, 0x0C80 },    
	{ 0x0226, 0x0000 },    
	{ 0x0227, 0x0006 },    
	{ 0x0240, 0x0000 },    
	{ 0x0241, 0x0000 },    
	{ 0x0242, 0x0000 },    
	{ 0x0243, 0x0000 },    
	{ 0x0244, 0x0C80 },    
	{ 0x0246, 0x0000 },    
	{ 0x0247, 0x0006 },    
	{ 0x0300, 0x4050 },    
	{ 0x0301, 0x4000 },    
	{ 0x0302, 0x0000 },    
	{ 0x0303, 0x0040 },    
	{ 0x0304, 0x0040 },    
	{ 0x0305, 0x0040 },    
	{ 0x0306, 0x0004 },    
	{ 0x0307, 0x0100 },    
	{ 0x0310, 0x4053 },    
	{ 0x0311, 0x4000 },    
	{ 0x0312, 0x0000 },    
	{ 0x0313, 0x0040 },    
	{ 0x0314, 0x0040 },    
	{ 0x0315, 0x0040 },    
	{ 0x0316, 0x0000 },    
	{ 0x0317, 0x0000 },    
	{ 0x0320, 0x0040 },    
	{ 0x0321, 0x0000 },    
	{ 0x0322, 0x0000 },    
	{ 0x0323, 0x0000 },    
	{ 0x0400, 0x00C0 },    
	{ 0x0401, 0x00C0 },    
	{ 0x0402, 0x00C0 },    
	{ 0x0403, 0x00C0 },    
	{ 0x0404, 0x00C0 },    
	{ 0x0405, 0x00C0 },    
	{ 0x0406, 0x00C0 },    
	{ 0x0407, 0x00C0 },    
	{ 0x0410, 0x0000 },    
	{ 0x0411, 0x0000 },    
	{ 0x0420, 0x0200 },    
	{ 0x0421, 0x0010 },    
	{ 0x0422, 0x0200 },    
	{ 0x0423, 0x0010 },    
	{ 0x0430, 0x0068 },    
	{ 0x0431, 0x0068 },    
	{ 0x0440, 0x0098 },    
	{ 0x0441, 0x0845 },    
	{ 0x0442, 0x0000 },    
	{ 0x0443, 0x0000 },    
	{ 0x0444, 0x0000 },    
	{ 0x0450, 0x0098 },    
	{ 0x0451, 0x0845 },    
	{ 0x0452, 0x0000 },    
	{ 0x0453, 0x0000 },    
	{ 0x0454, 0x0000 },    
	{ 0x0480, 0x6318 },    
	{ 0x0481, 0x6300 },    
	{ 0x0482, 0x0FCA },    
	{ 0x0483, 0x0400 },    
	{ 0x0484, 0x00D8 },    
	{ 0x0485, 0x1EB5 },    
	{ 0x0486, 0xF145 },    
	{ 0x0487, 0x0B75 },    
	{ 0x0488, 0x01C5 },    
	{ 0x0489, 0x1C58 },    
	{ 0x048A, 0xF373 },    
	{ 0x048B, 0x0A54 },    
	{ 0x048C, 0x0558 },    
	{ 0x048D, 0x168E },    
	{ 0x048E, 0xF829 },    
	{ 0x048F, 0x07AD },    
	{ 0x0490, 0x1103 },    
	{ 0x0491, 0x0564 },    
	{ 0x0492, 0x0559 },    
	{ 0x0493, 0x4000 },    
	{ 0x0494, 0x0000 },    
	{ 0x04A0, 0x6318 },    
	{ 0x04A1, 0x6300 },    
	{ 0x04A2, 0x0FCA },    
	{ 0x04A3, 0x0400 },    
	{ 0x04A4, 0x00D8 },    
	{ 0x04A5, 0x1EB5 },    
	{ 0x04A6, 0xF145 },    
	{ 0x04A7, 0x0B75 },    
	{ 0x04A8, 0x01C5 },    
	{ 0x04A9, 0x1C58 },    
	{ 0x04AA, 0xF373 },    
	{ 0x04AB, 0x0A54 },    
	{ 0x04AC, 0x0558 },    
	{ 0x04AD, 0x168E },    
	{ 0x04AE, 0xF829 },    
	{ 0x04AF, 0x07AD },    
	{ 0x04B0, 0x1103 },    
	{ 0x04B1, 0x0564 },    
	{ 0x04B2, 0x0559 },    
	{ 0x04B3, 0x4000 },    
	{ 0x04B4, 0x0000 },    
	{ 0x0500, 0x00C0 },    
	{ 0x0501, 0x00C0 },    
	{ 0x0502, 0x00C0 },    
	{ 0x0503, 0x00C0 },    
	{ 0x0510, 0x0000 },    
	{ 0x0520, 0x0200 },    
	{ 0x0521, 0x0010 },    
	{ 0x0530, 0x0068 },    
	{ 0x0540, 0x0098 },    
	{ 0x0541, 0x0845 },    
	{ 0x0542, 0x0000 },    
	{ 0x0543, 0x0000 },    
	{ 0x0544, 0x0000 },    
	{ 0x0580, 0x6318 },    
	{ 0x0581, 0x6300 },    
	{ 0x0582, 0x0FCA },    
	{ 0x0583, 0x0400 },    
	{ 0x0584, 0x00D8 },    
	{ 0x0585, 0x1EB5 },    
	{ 0x0586, 0xF145 },    
	{ 0x0587, 0x0B75 },    
	{ 0x0588, 0x01C5 },    
	{ 0x0589, 0x1C58 },    
	{ 0x058A, 0xF373 },    
	{ 0x058B, 0x0A54 },    
	{ 0x058C, 0x0558 },    
	{ 0x058D, 0x168E },    
	{ 0x058E, 0xF829 },    
	{ 0x058F, 0x07AD },    
	{ 0x0590, 0x1103 },    
	{ 0x0591, 0x0564 },    
	{ 0x0592, 0x0559 },    
	{ 0x0593, 0x4000 },    
	{ 0x0594, 0x0000 },    
	{ 0x0600, 0x0000 },    
	{ 0x0601, 0x0000 },    
	{ 0x0602, 0x0000 },    
	{ 0x0603, 0x0000 },    
	{ 0x0604, 0x0000 },    
	{ 0x0605, 0x0000 },    
	{ 0x0606, 0x0000 },    
	{ 0x0607, 0x0000 },    
	{ 0x0608, 0x0000 },    
	{ 0x0609, 0x0000 },    
	{ 0x0610, 0x02C0 },    
	{ 0x0611, 0x02C0 },    
	{ 0x0612, 0x02C0 },    
	{ 0x0613, 0x02C0 },    
	{ 0x0614, 0x0000 },    
	{ 0x0620, 0x0002 },    
	{ 0x0621, 0x0000 },    
	{ 0x0700, 0x8100 },    
	{ 0x0701, 0xA101 },    
	{ 0x0702, 0xA101 },    
	{ 0x0703, 0xA101 },    
	{ 0x0704, 0xA101 },    
	{ 0x0705, 0xA101 },    
	{ 0x0707, 0xA101 },    
	{ 0x0708, 0xA101 },    
	{ 0x0709, 0xA101 },    
	{ 0x070A, 0xA101 },    
	{ 0x0720, 0x0000 },    
	{ 0x0721, 0x0156 },    
	{ 0x0738, 0x07FF },    
	{ 0x0739, 0xFFEF },    
	{ 0x0740, 0x0000 },    
	{ 0x0748, 0x003F },    
	{ 0x0900, 0x1C00 },    
	{ 0x0901, 0x0000 },    
	{ 0x0A0D, 0x0000 },    
	{ 0x2400, 0x003F },    
	{ 0x2401, 0x8BD8 },    
	{ 0x2402, 0x0032 },    
	{ 0x2403, 0xF52D },    
	{ 0x2404, 0x0065 },    
	{ 0x2405, 0xAC8C },    
	{ 0x2406, 0x006B },    
	{ 0x2407, 0xE087 },    
	{ 0x2408, 0x0072 },    
	{ 0x2409, 0x1483 },    
	{ 0x240A, 0x0072 },    
	{ 0x240B, 0x1483 },    
	{ 0x240C, 0x0043 },    
	{ 0x240D, 0x3525 },    
	{ 0x240E, 0x0006 },    
	{ 0x240F, 0x6A4A },    
	{ 0x2410, 0x0043 },    
	{ 0x2411, 0x6079 },    
	{ 0x2412, 0x000C },    
	{ 0x2413, 0xCCCD },    
	{ 0x2414, 0x0000 },    
	{ 0x2415, 0x0800 },    
	{ 0x2416, 0x003F },    
	{ 0x2417, 0x8BD8 },    
	{ 0x2418, 0x0032 },    
	{ 0x2419, 0xF52D },    
	{ 0x241A, 0x0065 },    
	{ 0x241B, 0xAC8C },    
	{ 0x241C, 0x006B },    
	{ 0x241D, 0xE087 },    
	{ 0x241E, 0x0072 },    
	{ 0x241F, 0x1483 },    
	{ 0x2420, 0x0072 },    
	{ 0x2421, 0x1483 },    
	{ 0x2422, 0x0043 },    
	{ 0x2423, 0x3525 },    
	{ 0x2424, 0x0006 },    
	{ 0x2425, 0x6A4A },    
	{ 0x2426, 0x0043 },    
	{ 0x2427, 0x6079 },    
	{ 0x2428, 0x000C },    
	{ 0x2429, 0xCCCD },    
	{ 0x242A, 0x0000 },    
	{ 0x242B, 0x0800 },    
	{ 0x242C, 0x005A },    
	{ 0x242D, 0x7EFA },    
	{ 0x242E, 0x005A },    
	{ 0x242F, 0x7EFA },    
	{ 0x2600, 0x00A7 },    
	{ 0x2601, 0x0D1C },    
	{ 0x2602, 0x0083 },    
	{ 0x2603, 0x98AD },    
	{ 0x2606, 0x0008 },    
	{ 0x2607, 0xE7A2 },    
	{ 0x260A, 0x0055 },    
	{ 0x260B, 0x8C4B },    
};

static bool wm1811_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case WM8994_SOFTWARE_RESET:
	case WM8994_POWER_MANAGEMENT_1:
	case WM8994_POWER_MANAGEMENT_2:
	case WM8994_POWER_MANAGEMENT_3:
	case WM8994_POWER_MANAGEMENT_4:
	case WM8994_POWER_MANAGEMENT_5:
	case WM8994_POWER_MANAGEMENT_6:
	case WM8994_INPUT_MIXER_1:
	case WM8994_LEFT_LINE_INPUT_1_2_VOLUME:
	case WM8994_LEFT_LINE_INPUT_3_4_VOLUME:
	case WM8994_RIGHT_LINE_INPUT_1_2_VOLUME:
	case WM8994_RIGHT_LINE_INPUT_3_4_VOLUME:
	case WM8994_LEFT_OUTPUT_VOLUME:
	case WM8994_RIGHT_OUTPUT_VOLUME:
	case WM8994_LINE_OUTPUTS_VOLUME:
	case WM8994_HPOUT2_VOLUME:
	case WM8994_LEFT_OPGA_VOLUME:
	case WM8994_RIGHT_OPGA_VOLUME:
	case WM8994_SPKMIXL_ATTENUATION:
	case WM8994_SPKMIXR_ATTENUATION:
	case WM8994_SPKOUT_MIXERS:
	case WM8994_CLASSD:
	case WM8994_SPEAKER_VOLUME_LEFT:
	case WM8994_SPEAKER_VOLUME_RIGHT:
	case WM8994_INPUT_MIXER_2:
	case WM8994_INPUT_MIXER_3:
	case WM8994_INPUT_MIXER_4:
	case WM8994_INPUT_MIXER_5:
	case WM8994_INPUT_MIXER_6:
	case WM8994_OUTPUT_MIXER_1:
	case WM8994_OUTPUT_MIXER_2:
	case WM8994_OUTPUT_MIXER_3:
	case WM8994_OUTPUT_MIXER_4:
	case WM8994_OUTPUT_MIXER_5:
	case WM8994_OUTPUT_MIXER_6:
	case WM8994_HPOUT2_MIXER:
	case WM8994_LINE_MIXER_1:
	case WM8994_LINE_MIXER_2:
	case WM8994_SPEAKER_MIXER:
	case WM8994_ADDITIONAL_CONTROL:
	case WM8994_ANTIPOP_1:
	case WM8994_ANTIPOP_2:
	case WM8994_LDO_1:
	case WM8994_LDO_2:
	case WM8958_MICBIAS1:
	case WM8958_MICBIAS2:
	case WM8994_CHARGE_PUMP_1:
	case WM8958_CHARGE_PUMP_2:
	case WM8994_CLASS_W_1:
	case WM8994_DC_SERVO_1:
	case WM8994_DC_SERVO_2:
	case WM8994_DC_SERVO_READBACK:
	case WM8994_DC_SERVO_4:
	case WM8994_DC_SERVO_4E:
	case WM8994_ANALOGUE_HP_1:
	case WM8958_MIC_DETECT_1:
	case WM8958_MIC_DETECT_2:
	case WM8958_MIC_DETECT_3:
	case WM8994_CHIP_REVISION:
	case WM8994_CONTROL_INTERFACE:
	case WM8994_AIF1_CLOCKING_1:
	case WM8994_AIF1_CLOCKING_2:
	case WM8994_AIF2_CLOCKING_1:
	case WM8994_AIF2_CLOCKING_2:
	case WM8994_CLOCKING_1:
	case WM8994_CLOCKING_2:
	case WM8994_AIF1_RATE:
	case WM8994_AIF2_RATE:
	case WM8994_RATE_STATUS:
	case WM8994_FLL1_CONTROL_1:
	case WM8994_FLL1_CONTROL_2:
	case WM8994_FLL1_CONTROL_3:
	case WM8994_FLL1_CONTROL_4:
	case WM8994_FLL1_CONTROL_5:
	case WM8958_FLL1_EFS_1:
	case WM8958_FLL1_EFS_2:
	case WM8994_FLL2_CONTROL_1:
	case WM8994_FLL2_CONTROL_2:
	case WM8994_FLL2_CONTROL_3:
	case WM8994_FLL2_CONTROL_4:
	case WM8994_FLL2_CONTROL_5:
	case WM8958_FLL2_EFS_1:
	case WM8958_FLL2_EFS_2:
	case WM8994_AIF1_CONTROL_1:
	case WM8994_AIF1_CONTROL_2:
	case WM8994_AIF1_MASTER_SLAVE:
	case WM8994_AIF1_BCLK:
	case WM8994_AIF1ADC_LRCLK:
	case WM8994_AIF1DAC_LRCLK:
	case WM8994_AIF1DAC_DATA:
	case WM8994_AIF1ADC_DATA:
	case WM8994_AIF2_CONTROL_1:
	case WM8994_AIF2_CONTROL_2:
	case WM8994_AIF2_MASTER_SLAVE:
	case WM8994_AIF2_BCLK:
	case WM8994_AIF2ADC_LRCLK:
	case WM8994_AIF2DAC_LRCLK:
	case WM8994_AIF2DAC_DATA:
	case WM8994_AIF2ADC_DATA:
	case WM1811_AIF2TX_CONTROL:
	case WM8958_AIF3_CONTROL_1:
	case WM8958_AIF3_CONTROL_2:
	case WM8958_AIF3DAC_DATA:
	case WM8958_AIF3ADC_DATA:
	case WM8994_AIF1_ADC1_LEFT_VOLUME:
	case WM8994_AIF1_ADC1_RIGHT_VOLUME:
	case WM8994_AIF1_DAC1_LEFT_VOLUME:
	case WM8994_AIF1_DAC1_RIGHT_VOLUME:
	case WM8994_AIF1_ADC1_FILTERS:
	case WM8994_AIF1_DAC1_FILTERS_1:
	case WM8994_AIF1_DAC1_FILTERS_2:
	case WM8958_AIF1_DAC1_NOISE_GATE:
	case WM8994_AIF1_DRC1_1:
	case WM8994_AIF1_DRC1_2:
	case WM8994_AIF1_DRC1_3:
	case WM8994_AIF1_DRC1_4:
	case WM8994_AIF1_DRC1_5:
	case WM8994_AIF1_DAC1_EQ_GAINS_1:
	case WM8994_AIF1_DAC1_EQ_GAINS_2:
	case WM8994_AIF1_DAC1_EQ_BAND_1_A:
	case WM8994_AIF1_DAC1_EQ_BAND_1_B:
	case WM8994_AIF1_DAC1_EQ_BAND_1_PG:
	case WM8994_AIF1_DAC1_EQ_BAND_2_A:
	case WM8994_AIF1_DAC1_EQ_BAND_2_B:
	case WM8994_AIF1_DAC1_EQ_BAND_2_C:
	case WM8994_AIF1_DAC1_EQ_BAND_2_PG:
	case WM8994_AIF1_DAC1_EQ_BAND_3_A:
	case WM8994_AIF1_DAC1_EQ_BAND_3_B:
	case WM8994_AIF1_DAC1_EQ_BAND_3_C:
	case WM8994_AIF1_DAC1_EQ_BAND_3_PG:
	case WM8994_AIF1_DAC1_EQ_BAND_4_A:
	case WM8994_AIF1_DAC1_EQ_BAND_4_B:
	case WM8994_AIF1_DAC1_EQ_BAND_4_C:
	case WM8994_AIF1_DAC1_EQ_BAND_4_PG:
	case WM8994_AIF1_DAC1_EQ_BAND_5_A:
	case WM8994_AIF1_DAC1_EQ_BAND_5_B:
	case WM8994_AIF1_DAC1_EQ_BAND_5_PG:
	case WM8994_AIF1_DAC1_EQ_BAND_1_C:
	case WM8994_AIF2_ADC_LEFT_VOLUME:
	case WM8994_AIF2_ADC_RIGHT_VOLUME:
	case WM8994_AIF2_DAC_LEFT_VOLUME:
	case WM8994_AIF2_DAC_RIGHT_VOLUME:
	case WM8994_AIF2_ADC_FILTERS:
	case WM8994_AIF2_DAC_FILTERS_1:
	case WM8994_AIF2_DAC_FILTERS_2:
	case WM8958_AIF2_DAC_NOISE_GATE:
	case WM8994_AIF2_DRC_1:
	case WM8994_AIF2_DRC_2:
	case WM8994_AIF2_DRC_3:
	case WM8994_AIF2_DRC_4:
	case WM8994_AIF2_DRC_5:
	case WM8994_AIF2_EQ_GAINS_1:
	case WM8994_AIF2_EQ_GAINS_2:
	case WM8994_AIF2_EQ_BAND_1_A:
	case WM8994_AIF2_EQ_BAND_1_B:
	case WM8994_AIF2_EQ_BAND_1_PG:
	case WM8994_AIF2_EQ_BAND_2_A:
	case WM8994_AIF2_EQ_BAND_2_B:
	case WM8994_AIF2_EQ_BAND_2_C:
	case WM8994_AIF2_EQ_BAND_2_PG:
	case WM8994_AIF2_EQ_BAND_3_A:
	case WM8994_AIF2_EQ_BAND_3_B:
	case WM8994_AIF2_EQ_BAND_3_C:
	case WM8994_AIF2_EQ_BAND_3_PG:
	case WM8994_AIF2_EQ_BAND_4_A:
	case WM8994_AIF2_EQ_BAND_4_B:
	case WM8994_AIF2_EQ_BAND_4_C:
	case WM8994_AIF2_EQ_BAND_4_PG:
	case WM8994_AIF2_EQ_BAND_5_A:
	case WM8994_AIF2_EQ_BAND_5_B:
	case WM8994_AIF2_EQ_BAND_5_PG:
	case WM8994_AIF2_EQ_BAND_1_C:
	case WM8994_DAC1_MIXER_VOLUMES:
	case WM8994_DAC1_LEFT_MIXER_ROUTING:
	case WM8994_DAC1_RIGHT_MIXER_ROUTING:
	case WM8994_DAC2_MIXER_VOLUMES:
	case WM8994_DAC2_LEFT_MIXER_ROUTING:
	case WM8994_DAC2_RIGHT_MIXER_ROUTING:
	case WM8994_AIF1_ADC1_LEFT_MIXER_ROUTING:
	case WM8994_AIF1_ADC1_RIGHT_MIXER_ROUTING:
	case WM8994_DAC1_LEFT_VOLUME:
	case WM8994_DAC1_RIGHT_VOLUME:
	case WM8994_DAC2_LEFT_VOLUME:
	case WM8994_DAC2_RIGHT_VOLUME:
	case WM8994_DAC_SOFTMUTE:
	case WM8994_OVERSAMPLING:
	case WM8994_SIDETONE:
	case WM8994_GPIO_1:
	case WM8994_GPIO_2:
	case WM8994_GPIO_3:
	case WM8994_GPIO_4:
	case WM8994_GPIO_5:
	case WM8994_GPIO_6:
	case WM8994_GPIO_8:
	case WM8994_GPIO_9:
	case WM8994_GPIO_10:
	case WM8994_GPIO_11:
	case WM8994_PULL_CONTROL_1:
	case WM8994_PULL_CONTROL_2:
	case WM8994_INTERRUPT_STATUS_1:
	case WM8994_INTERRUPT_STATUS_2:
	case WM8994_INTERRUPT_RAW_STATUS_2:
	case WM8994_INTERRUPT_STATUS_1_MASK:
	case WM8994_INTERRUPT_STATUS_2_MASK:
	case WM8994_INTERRUPT_CONTROL:
	case WM8994_IRQ_DEBOUNCE:
		return true;
	default:
		return false;
	}
}

static bool wm8994_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case WM8994_DC_SERVO_READBACK:
	case WM8994_MICBIAS:
	case WM8994_WRITE_SEQUENCER_CTRL_1:
	case WM8994_WRITE_SEQUENCER_CTRL_2:
	case WM8994_AIF1_ADC2_LEFT_VOLUME:
	case WM8994_AIF1_ADC2_RIGHT_VOLUME:
	case WM8994_AIF1_DAC2_LEFT_VOLUME:
	case WM8994_AIF1_DAC2_RIGHT_VOLUME:
	case WM8994_AIF1_ADC2_FILTERS:
	case WM8994_AIF1_DAC2_FILTERS_1:
	case WM8994_AIF1_DAC2_FILTERS_2:
	case WM8958_AIF1_DAC2_NOISE_GATE:
	case WM8994_AIF1_DRC2_1:
	case WM8994_AIF1_DRC2_2:
	case WM8994_AIF1_DRC2_3:
	case WM8994_AIF1_DRC2_4:
	case WM8994_AIF1_DRC2_5:
	case WM8994_AIF1_DAC2_EQ_GAINS_1:
	case WM8994_AIF1_DAC2_EQ_GAINS_2:
	case WM8994_AIF1_DAC2_EQ_BAND_1_A:
	case WM8994_AIF1_DAC2_EQ_BAND_1_B:
	case WM8994_AIF1_DAC2_EQ_BAND_1_PG:
	case WM8994_AIF1_DAC2_EQ_BAND_2_A:
	case WM8994_AIF1_DAC2_EQ_BAND_2_B:
	case WM8994_AIF1_DAC2_EQ_BAND_2_C:
	case WM8994_AIF1_DAC2_EQ_BAND_2_PG:
	case WM8994_AIF1_DAC2_EQ_BAND_3_A:
	case WM8994_AIF1_DAC2_EQ_BAND_3_B:
	case WM8994_AIF1_DAC2_EQ_BAND_3_C:
	case WM8994_AIF1_DAC2_EQ_BAND_3_PG:
	case WM8994_AIF1_DAC2_EQ_BAND_4_A:
	case WM8994_AIF1_DAC2_EQ_BAND_4_B:
	case WM8994_AIF1_DAC2_EQ_BAND_4_C:
	case WM8994_AIF1_DAC2_EQ_BAND_4_PG:
	case WM8994_AIF1_DAC2_EQ_BAND_5_A:
	case WM8994_AIF1_DAC2_EQ_BAND_5_B:
	case WM8994_AIF1_DAC2_EQ_BAND_5_PG:
	case WM8994_AIF1_DAC2_EQ_BAND_1_C:
	case WM8994_DAC2_MIXER_VOLUMES:
	case WM8994_DAC2_LEFT_MIXER_ROUTING:
	case WM8994_DAC2_RIGHT_MIXER_ROUTING:
	case WM8994_AIF1_ADC2_LEFT_MIXER_ROUTING:
	case WM8994_AIF1_ADC2_RIGHT_MIXER_ROUTING:
	case WM8994_DAC2_LEFT_VOLUME:
	case WM8994_DAC2_RIGHT_VOLUME:
		return true;
	default:
		return wm1811_readable_register(dev, reg);
	}
}

static bool wm8958_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case WM8958_DSP2_PROGRAM:
	case WM8958_DSP2_CONFIG:
	case WM8958_DSP2_MAGICNUM:
	case WM8958_DSP2_RELEASEYEAR:
	case WM8958_DSP2_RELEASEMONTHDAY:
	case WM8958_DSP2_RELEASETIME:
	case WM8958_DSP2_VERMAJMIN:
	case WM8958_DSP2_VERBUILD:
	case WM8958_DSP2_TESTREG:
	case WM8958_DSP2_XORREG:
	case WM8958_DSP2_SHIFTMAXX:
	case WM8958_DSP2_SHIFTMAXY:
	case WM8958_DSP2_SHIFTMAXZ:
	case WM8958_DSP2_SHIFTMAXEXTLO:
	case WM8958_DSP2_AESSELECT:
	case WM8958_DSP2_EXECCONTROL:
	case WM8958_DSP2_SAMPLEBREAK:
	case WM8958_DSP2_COUNTBREAK:
	case WM8958_DSP2_INTSTATUS:
	case WM8958_DSP2_EVENTSTATUS:
	case WM8958_DSP2_INTMASK:
	case WM8958_DSP2_CONFIGDWIDTH:
	case WM8958_DSP2_CONFIGINSTR:
	case WM8958_DSP2_CONFIGDMEM:
	case WM8958_DSP2_CONFIGDELAYS:
	case WM8958_DSP2_CONFIGNUMIO:
	case WM8958_DSP2_CONFIGEXTDEPTH:
	case WM8958_DSP2_CONFIGMULTIPLIER:
	case WM8958_DSP2_CONFIGCTRLDWIDTH:
	case WM8958_DSP2_CONFIGPIPELINE:
	case WM8958_DSP2_SHIFTMAXEXTHI:
	case WM8958_DSP2_SWVERSIONREG:
	case WM8958_DSP2_CONFIGXMEM:
	case WM8958_DSP2_CONFIGYMEM:
	case WM8958_DSP2_CONFIGZMEM:
	case WM8958_FW_BUILD_1:
	case WM8958_FW_BUILD_0:
	case WM8958_FW_ID_1:
	case WM8958_FW_ID_0:
	case WM8958_FW_MAJOR_1:
	case WM8958_FW_MAJOR_0:
	case WM8958_FW_MINOR_1:
	case WM8958_FW_MINOR_0:
	case WM8958_FW_PATCH_1:
	case WM8958_FW_PATCH_0:
	case WM8958_MBC_BAND_1_K_1:
	case WM8958_MBC_BAND_1_K_2:
	case WM8958_MBC_BAND_1_N1_1:
	case WM8958_MBC_BAND_1_N1_2:
	case WM8958_MBC_BAND_1_N2_1:
	case WM8958_MBC_BAND_1_N2_2:
	case WM8958_MBC_BAND_1_N3_1:
	case WM8958_MBC_BAND_1_N3_2:
	case WM8958_MBC_BAND_1_N4_1:
	case WM8958_MBC_BAND_1_N4_2:
	case WM8958_MBC_BAND_1_N5_1:
	case WM8958_MBC_BAND_1_N5_2:
	case WM8958_MBC_BAND_1_X1_1:
	case WM8958_MBC_BAND_1_X1_2:
	case WM8958_MBC_BAND_1_X2_1:
	case WM8958_MBC_BAND_1_X2_2:
	case WM8958_MBC_BAND_1_X3_1:
	case WM8958_MBC_BAND_1_X3_2:
	case WM8958_MBC_BAND_1_ATTACK_1:
	case WM8958_MBC_BAND_1_ATTACK_2:
	case WM8958_MBC_BAND_1_DECAY_1:
	case WM8958_MBC_BAND_1_DECAY_2:
	case WM8958_MBC_BAND_2_K_1:
	case WM8958_MBC_BAND_2_K_2:
	case WM8958_MBC_BAND_2_N1_1:
	case WM8958_MBC_BAND_2_N1_2:
	case WM8958_MBC_BAND_2_N2_1:
	case WM8958_MBC_BAND_2_N2_2:
	case WM8958_MBC_BAND_2_N3_1:
	case WM8958_MBC_BAND_2_N3_2:
	case WM8958_MBC_BAND_2_N4_1:
	case WM8958_MBC_BAND_2_N4_2:
	case WM8958_MBC_BAND_2_N5_1:
	case WM8958_MBC_BAND_2_N5_2:
	case WM8958_MBC_BAND_2_X1_1:
	case WM8958_MBC_BAND_2_X1_2:
	case WM8958_MBC_BAND_2_X2_1:
	case WM8958_MBC_BAND_2_X2_2:
	case WM8958_MBC_BAND_2_X3_1:
	case WM8958_MBC_BAND_2_X3_2:
	case WM8958_MBC_BAND_2_ATTACK_1:
	case WM8958_MBC_BAND_2_ATTACK_2:
	case WM8958_MBC_BAND_2_DECAY_1:
	case WM8958_MBC_BAND_2_DECAY_2:
	case WM8958_MBC_B2_PG2_1:
	case WM8958_MBC_B2_PG2_2:
	case WM8958_MBC_B1_PG2_1:
	case WM8958_MBC_B1_PG2_2:
	case WM8958_MBC_CROSSOVER_1:
	case WM8958_MBC_CROSSOVER_2:
	case WM8958_MBC_HPF_1:
	case WM8958_MBC_HPF_2:
	case WM8958_MBC_LPF_1:
	case WM8958_MBC_LPF_2:
	case WM8958_MBC_RMS_LIMIT_1:
	case WM8958_MBC_RMS_LIMIT_2:
		return true;
	default:
		return wm8994_readable_register(dev, reg);
	}
}

static bool wm8994_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case WM8994_SOFTWARE_RESET:
	case WM8994_DC_SERVO_1:
	case WM8994_DC_SERVO_READBACK:
	case WM8994_RATE_STATUS:
	case WM8958_MIC_DETECT_3:
	case WM8994_DC_SERVO_4E:
	case WM8994_CHIP_REVISION:
	case WM8994_INTERRUPT_STATUS_1:
	case WM8994_INTERRUPT_STATUS_2:
		return true;
	default:
		return false;
	}
}

static bool wm1811_volatile_register(struct device *dev, unsigned int reg)
{
	struct wm8994 *wm8994 = dev_get_drvdata(dev);

	switch (reg) {
	case WM8994_GPIO_6:
		if (wm8994->revision > 1)
			return true;
		else
			return false;
	default:
		return wm8994_volatile_register(dev, reg);
	}
}

static bool wm8958_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case WM8958_DSP2_MAGICNUM:
	case WM8958_DSP2_RELEASEYEAR:
	case WM8958_DSP2_RELEASEMONTHDAY:
	case WM8958_DSP2_RELEASETIME:
	case WM8958_DSP2_VERMAJMIN:
	case WM8958_DSP2_VERBUILD:
	case WM8958_DSP2_EXECCONTROL:
	case WM8958_DSP2_SWVERSIONREG:
	case WM8958_DSP2_CONFIGXMEM:
	case WM8958_DSP2_CONFIGYMEM:
	case WM8958_DSP2_CONFIGZMEM:
	case WM8958_FW_BUILD_1:
	case WM8958_FW_BUILD_0:
	case WM8958_FW_ID_1:
	case WM8958_FW_ID_0:
	case WM8958_FW_MAJOR_1:
	case WM8958_FW_MAJOR_0:
	case WM8958_FW_MINOR_1:
	case WM8958_FW_MINOR_0:
	case WM8958_FW_PATCH_1:
	case WM8958_FW_PATCH_0:
		return true;
	default:
		return wm8994_volatile_register(dev, reg);
	}
}

struct regmap_config wm1811_regmap_config = {
	.reg_bits = 16,
	.val_bits = 16,

	.cache_type = REGCACHE_RBTREE,

	.reg_defaults = wm1811_defaults,
	.num_reg_defaults = ARRAY_SIZE(wm1811_defaults),

	.max_register = WM8994_MAX_REGISTER,
	.volatile_reg = wm1811_volatile_register,
	.readable_reg = wm1811_readable_register,
};

struct regmap_config wm8994_regmap_config = {
	.reg_bits = 16,
	.val_bits = 16,

	.cache_type = REGCACHE_RBTREE,

	.reg_defaults = wm8994_defaults,
	.num_reg_defaults = ARRAY_SIZE(wm8994_defaults),

	.max_register = WM8994_MAX_REGISTER,
	.volatile_reg = wm8994_volatile_register,
	.readable_reg = wm8994_readable_register,
};

struct regmap_config wm8958_regmap_config = {
	.reg_bits = 16,
	.val_bits = 16,

	.cache_type = REGCACHE_RBTREE,

	.reg_defaults = wm8958_defaults,
	.num_reg_defaults = ARRAY_SIZE(wm8958_defaults),

	.max_register = WM8994_MAX_REGISTER,
	.volatile_reg = wm8958_volatile_register,
	.readable_reg = wm8958_readable_register,
};

struct regmap_config wm8994_base_regmap_config = {
	.reg_bits = 16,
	.val_bits = 16,
};
