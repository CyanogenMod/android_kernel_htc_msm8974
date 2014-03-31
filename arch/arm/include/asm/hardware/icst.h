/*
 *  arch/arm/include/asm/hardware/icst.h
 *
 *  Copyright (C) 2003 Deep Blue Solutions, Ltd, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Support functions for calculating clocks/divisors for the ICST
 *  clock generators.  See http://www.idt.com/ for more information
 *  on these devices.
 */
#ifndef ASMARM_HARDWARE_ICST_H
#define ASMARM_HARDWARE_ICST_H

struct icst_params {
	unsigned long	ref;
	unsigned long	vco_max;	
	unsigned long	vco_min;	
	unsigned short	vd_min;		
	unsigned short	vd_max;		
	unsigned char	rd_min;		
	unsigned char	rd_max;		
	const unsigned char *s2div;	
	const unsigned char *idx2s;	
};

struct icst_vco {
	unsigned short	v;
	unsigned char	r;
	unsigned char	s;
};

unsigned long icst_hz(const struct icst_params *p, struct icst_vco vco);
struct icst_vco icst_hz_to_vco(const struct icst_params *p, unsigned long freq);

#define ICST307_VCO_MIN	6000000
#define ICST307_VCO_MAX	200000000

extern const unsigned char icst307_s2div[];
extern const unsigned char icst307_idx2s[];

#define ICST525_VCO_MIN		10000000
#define ICST525_VCO_MAX_3V	200000000
#define ICST525_VCO_MAX_5V	320000000

extern const unsigned char icst525_s2div[];
extern const unsigned char icst525_idx2s[];

#endif
