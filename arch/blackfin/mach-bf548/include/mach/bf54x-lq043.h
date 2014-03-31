/*
 * Copyright 2007-2008 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#ifndef BF54X_LQ043_H
#define BF54X_LQ043_H

struct bfin_bf54xfb_val {
	unsigned int	defval;
	unsigned int	min;
	unsigned int	max;
};

struct bfin_bf54xfb_mach_info {
	unsigned char	fixed_syncs;	

	
	int		type;

	
	int		width;
	int		height;

	
	struct bfin_bf54xfb_val xres;
	struct bfin_bf54xfb_val yres;
	struct bfin_bf54xfb_val bpp;

	
	unsigned short 		disp;

};

#endif 
