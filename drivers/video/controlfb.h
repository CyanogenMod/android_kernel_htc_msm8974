/*
 * controlfb_hw.h: Constants of all sorts for controlfb
 *
 * Copyright (C) 1998 Daniel Jacobowitz <dan@debian.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Based on an awful lot of code, including:
 *
 * control.c: Console support for PowerMac "control" display adaptor.
 * Copyright (C) 1996 Paul Mackerras.
 *
 * The so far unpublished platinumfb.c
 * Copyright (C) 1998 Jon Howell
 */

struct cmap_regs {
	unsigned char addr;	
	char pad1[15];
	unsigned char crsr;	
	char pad2[15];
	unsigned char dat;	
	char pad3[15];
	unsigned char lut;	
	char pad4[15];
};

#define PAD(x)	char x[12]

struct preg {			
	unsigned r;
	char pad[12];
};

struct control_regs {
	struct preg vcount;	
	
	struct preg vswin;	
	struct preg vsblank;	
	struct preg veblank;	
	struct preg vewin;	
	struct preg vesync;	
	struct preg vssync;	
	struct preg vperiod;	
	struct preg piped;	
	
	struct preg hperiod;	
	struct preg hsblank;	
	struct preg heblank;	
	struct preg hesync;	
	struct preg hssync;	
	struct preg heq;	
	struct preg hlfln;	
	struct preg hserr;	
	struct preg cnttst;
	struct preg ctrl;	
	struct preg start_addr;	
	struct preg pitch;	
	struct preg mon_sense;	
	struct preg vram_attr;	
	struct preg mode;
	struct preg rfrcnt;	
	struct preg intr_ena;	
	struct preg intr_stat;	
	struct preg res[5];
};

struct control_regints {
	
	unsigned vswin;	
	unsigned vsblank;	
	unsigned veblank;	
	unsigned vewin;	
	unsigned vesync;	
	unsigned vssync;	
	unsigned vperiod;	
	unsigned piped;		
	
	
	unsigned hperiod;	
	unsigned hsblank;	
	unsigned heblank;	
	unsigned hesync;	
	unsigned hssync;	
	unsigned heq;		
	unsigned hlfln;		
	unsigned hserr;		
};
	
struct control_regvals {
	unsigned regs[16];		
	unsigned char mode;
	unsigned char radacal_ctrl;
	unsigned char clock_params[3];
};

#define CTRLFB_OFF 16	


struct max_cmodes {
	int m[2];	
};

static struct max_cmodes control_mac_modes[] = {
	{{-1,-1}},	
	{{-1,-1}},	
	{{-1,-1}},	
	{{-1,-1}},	
	{{ 2, 2}},	
	{{ 2, 2}},	
	{{-1,-1}},	
	{{-1,-1}},	
	{{ 2, 2}},	
	{{ 2, 2}},	
	{{ 2, 2}},	
	{{ 2, 2}},	
	{{ 1, 2}},	
	{{ 1, 2}},	
	{{ 1, 2}},	
	{{ 1, 2}},	
	{{ 1, 2}},	
	{{ 1, 2}},	
	{{ 0, 1}},	
	{{ 0, 1}},	
};

