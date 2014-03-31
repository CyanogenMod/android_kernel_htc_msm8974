/*
 * hwmon-vid.c - VID/VRM/VRD voltage conversions
 *
 * Copyright (c) 2004 Rudolf Marek <r.marek@assembler.cz>
 *
 * Partly imported from i2c-vid.h of the lm_sensors project
 * Copyright (c) 2002 Mark D. Studebaker <mdsxyz123@yahoo.com>
 * With assistance from Trent Piepho <xyzzy@speakeasy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hwmon-vid.h>


int vid_from_reg(int val, u8 vrm)
{
	int vid;

	switch (vrm) {

	case 100:		
		
		val &= 0x3f;
		if ((val & 0x1f) == 0x1f)
			return 0;
		if ((val & 0x1f) <= 0x09 || val == 0x0a)
			vid = 1087500 - (val & 0x1f) * 25000;
		else
			vid = 1862500 - (val & 0x1f) * 25000;
		if (val & 0x20)
			vid -= 12500;
		return (vid + 500) / 1000;

	case 110:		
				
		val &= 0xff;
		if (val < 0x02 || val > 0xb2)
			return 0;
		return (1600000 - (val - 2) * 6250 + 500) / 1000;

	case 24:		
		val &= 0x1f;
		if (val == 0x1f)
			return 0;
				
	case 25:		
		val &= 0x3f;
		return (val < 32) ? 1550 - 25 * val
			: 775 - (25 * (val - 31)) / 2;

	case 91:		
	case 90:		
		val &= 0x1f;
		return val == 0x1f ? 0 :
				     1850 - val * 25;

	case 85:		
		val &= 0x1f;
		return (val & 0x10  ? 25 : 0) +
		       ((val & 0x0f) > 0x04 ? 2050 : 1250) -
		       ((val & 0x0f) * 50);

	case 84:		
		val &= 0x0f;
				
	case 82:		
		val &= 0x1f;
		return val == 0x1f ? 0 :
		       val & 0x10  ? 5100 - (val) * 100 :
				     2050 - (val) * 50;
	case 17:		
		val &= 0x1f;
		return val & 0x10 ? 975 - (val & 0xF) * 25 :
				    1750 - val * 50;
	case 13:
	case 131:
		val &= 0x3f;
		
		if (vrm == 131 && val == 0x3f)
			val++;
		return 1708 - val * 16;
	case 14:		
				
		val &= 0x7f;
		return val > 0x77 ? 0 : (1500000 - (val * 12500) + 500) / 1000;
	default:		
		if (vrm)
			pr_warn("Requested unsupported VRM version (%u)\n",
				(unsigned int)vrm);
		return 0;
	}
}
EXPORT_SYMBOL(vid_from_reg);


struct vrm_model {
	u8 vendor;
	u8 family;
	u8 model_from;
	u8 model_to;
	u8 stepping_to;
	u8 vrm_type;
};

#define ANY 0xFF

#ifdef CONFIG_X86


static struct vrm_model vrm_models[] = {
	{X86_VENDOR_AMD, 0x6, 0x0, ANY, ANY, 90},	
	{X86_VENDOR_AMD, 0xF, 0x0, 0x3F, ANY, 24},	
	{X86_VENDOR_AMD, 0xF, 0x40, 0x7F, ANY, 24},	
	{X86_VENDOR_AMD, 0xF, 0x80, ANY, ANY, 25},	
	{X86_VENDOR_AMD, 0x10, 0x0, ANY, ANY, 25},	

	{X86_VENDOR_INTEL, 0x6, 0x0, 0x6, ANY, 82},	
	{X86_VENDOR_INTEL, 0x6, 0x7, 0x7, ANY, 84},	
	{X86_VENDOR_INTEL, 0x6, 0x8, 0x8, ANY, 82},	
	{X86_VENDOR_INTEL, 0x6, 0x9, 0x9, ANY, 13},	
	{X86_VENDOR_INTEL, 0x6, 0xA, 0xA, ANY, 82},	
	{X86_VENDOR_INTEL, 0x6, 0xB, 0xB, ANY, 85},	
	{X86_VENDOR_INTEL, 0x6, 0xD, 0xD, ANY, 13},	
	{X86_VENDOR_INTEL, 0x6, 0xE, 0xE, ANY, 14},	
	{X86_VENDOR_INTEL, 0x6, 0xF, ANY, ANY, 110},	
	{X86_VENDOR_INTEL, 0xF, 0x0, 0x0, ANY, 90},	
	{X86_VENDOR_INTEL, 0xF, 0x1, 0x1, ANY, 90},	
	{X86_VENDOR_INTEL, 0xF, 0x2, 0x2, ANY, 90},	
	{X86_VENDOR_INTEL, 0xF, 0x3, ANY, ANY, 100},	

	{X86_VENDOR_CENTAUR, 0x6, 0x7, 0x7, ANY, 85},	
	{X86_VENDOR_CENTAUR, 0x6, 0x8, 0x8, 0x7, 85},	
	{X86_VENDOR_CENTAUR, 0x6, 0x9, 0x9, 0x7, 85},	
	{X86_VENDOR_CENTAUR, 0x6, 0x9, 0x9, ANY, 17},	
	{X86_VENDOR_CENTAUR, 0x6, 0xA, 0xA, 0x7, 0},	
	{X86_VENDOR_CENTAUR, 0x6, 0xA, 0xA, ANY, 13},	
	{X86_VENDOR_CENTAUR, 0x6, 0xD, 0xD, ANY, 134},	
};

static u8 get_via_model_d_vrm(void)
{
	unsigned int vid, brand, dummy;
	static const char *brands[4] = {
		"C7-M", "C7", "Eden", "C7-D"
	};

	rdmsr(0x198, dummy, vid);
	vid &= 0xff;

	rdmsr(0x1154, brand, dummy);
	brand = ((brand >> 4) ^ (brand >> 2)) & 0x03;

	if (vid > 0x3f) {
		pr_info("Using %d-bit VID table for VIA %s CPU\n",
			7, brands[brand]);
		return 14;
	} else {
		pr_info("Using %d-bit VID table for VIA %s CPU\n",
			6, brands[brand]);
		
		return brand == 2 ? 131 : 13;
	}
}

static u8 find_vrm(u8 family, u8 model, u8 stepping, u8 vendor)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(vrm_models); i++) {
		if (vendor == vrm_models[i].vendor &&
		    family == vrm_models[i].family &&
		    model >= vrm_models[i].model_from &&
		    model <= vrm_models[i].model_to &&
		    stepping <= vrm_models[i].stepping_to)
			return vrm_models[i].vrm_type;
	}

	return 0;
}

u8 vid_which_vrm(void)
{
	struct cpuinfo_x86 *c = &cpu_data(0);
	u8 vrm_ret;

	if (c->x86 < 6)		
		return 0;	

	vrm_ret = find_vrm(c->x86, c->x86_model, c->x86_mask, c->x86_vendor);
	if (vrm_ret == 134)
		vrm_ret = get_via_model_d_vrm();
	if (vrm_ret == 0)
		pr_info("Unknown VRM version of your x86 CPU\n");
	return vrm_ret;
}

#else
u8 vid_which_vrm(void)
{
	pr_info("Unknown VRM version of your CPU\n");
	return 0;
}
#endif
EXPORT_SYMBOL(vid_which_vrm);

MODULE_AUTHOR("Rudolf Marek <r.marek@assembler.cz>");

MODULE_DESCRIPTION("hwmon-vid driver");
MODULE_LICENSE("GPL");
