/*
 * valkyriefb.h: Constants of all sorts for valkyriefb
 *
 *  Created 8 August 1998 by 
 *  Martin Costabel <costabel@wanadoo.fr> and Kevin Schoedel
 *
 * Vmode-switching changes and vmode 15/17 modifications created 29 August
 * 1998 by Barry K. Nathan <barryn@pobox.com>.
 * 
 * vmode 10 changed by Steven Borley <sjb@salix.demon.co.uk>, 14 mai 2000
 *
 * Ported to 68k Macintosh by David Huggins-Daines <dhd@debian.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Based directly on:
 *
 *  controlfb.h: Constants of all sorts for controlfb
 *  Copyright (C) 1998 Daniel Jacobowitz <dan@debian.org>
 *
 *  pmc-valkyrie.h: Console support for PowerMac "control" display adaptor.
 *  Copyright (C) 1997 Paul Mackerras.
 *
 *  pmc-valkyrie.c: Console support for PowerMac "control" display adaptor.
 *  Copyright (C) 1997 Paul Mackerras.
 *
 * and indirectly from:
 *
 *  pmc-control.h: Console support for PowerMac "control" display adaptor.
 *  Copyright (C) 1997 Paul Mackerras.
 *
 *  pmc-control.c: Console support for PowerMac "control" display adaptor.
 *  Copyright (C) 1996 Paul Mackerras.
 *
 *  platinumfb.c: Console support for PowerMac "platinum" display adaptor.
 *  Copyright (C) 1998 Jon Howell
 */

#ifdef CONFIG_MAC
#define VALKYRIE_REG_PADSIZE	3
#else
#define VALKYRIE_REG_PADSIZE	7
#endif

struct cmap_regs {
	unsigned char addr;
	char pad1[VALKYRIE_REG_PADSIZE];
	unsigned char lut;
};


struct vpreg {			
	unsigned char r;
	char pad[VALKYRIE_REG_PADSIZE];
};


struct valkyrie_regs {
	struct vpreg mode;
	struct vpreg depth;
	struct vpreg status;
	struct vpreg reg3;
	struct vpreg intr;
	struct vpreg reg5;
	struct vpreg intr_enb;
	struct vpreg msense;
};

struct valkyrie_regvals {
	unsigned char mode;
	unsigned char clock_params[3];
	int	pitch[2];		
	int	hres;
	int	vres;
};

#ifndef CONFIG_MAC

static struct valkyrie_regvals valkyrie_reg_init_17 = {
    15, 
    { 11, 28, 3 },  
    { 1024, 0 },
	1024, 768
};

static struct valkyrie_regvals valkyrie_reg_init_15 = {
    15,
    { 12, 29, 3 },  
    { 1024, 0 },
	1024, 768
};

static struct valkyrie_regvals valkyrie_reg_init_14 = {
    14,
    { 15, 31, 3 },  
    { 1024, 0 },
	1024, 768
};
#endif 

static struct valkyrie_regvals valkyrie_reg_init_13 = {
    9,
    { 23, 42, 3 },  
    { 832, 0 },
	832, 624
};

static struct valkyrie_regvals valkyrie_reg_init_11 = {
    13,
    { 17, 27, 3 },  
    { 800, 0 },
	800, 600
};

static struct valkyrie_regvals valkyrie_reg_init_10 = {
    12,
    { 25, 32, 3 },  
    { 800, 1600 },
	800, 600
};

static struct valkyrie_regvals valkyrie_reg_init_6 = {
    6,
    { 14, 27, 2 },  
    { 640, 1280 },
	640, 480
};

static struct valkyrie_regvals valkyrie_reg_init_5 = {
    11,
    { 23, 37, 2 },  
    { 640, 1280 },
	640, 480
};

static struct valkyrie_regvals *valkyrie_reg_init[VMODE_MAX] = {
	NULL,
	NULL,
	NULL,
	NULL,
	&valkyrie_reg_init_5,
	&valkyrie_reg_init_6,
	NULL,
	NULL,
	NULL,
	&valkyrie_reg_init_10,
	&valkyrie_reg_init_11,
	NULL,
	&valkyrie_reg_init_13,
#ifndef CONFIG_MAC
	&valkyrie_reg_init_14,
	&valkyrie_reg_init_15,
	NULL,
	&valkyrie_reg_init_17,
#endif
};
