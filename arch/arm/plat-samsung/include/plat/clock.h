/* linux/arch/arm/plat-s3c/include/plat/clock.h
 *
 * Copyright (c) 2004-2005 Simtec Electronics
 *	http://www.simtec.co.uk/products/SWLINUX/
 *	Written by Ben Dooks, <ben@simtec.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_PLAT_CLOCK_H
#define __ASM_PLAT_CLOCK_H __FILE__

#include <linux/spinlock.h>
#include <linux/clkdev.h>

struct clk;

struct clk_ops {
	int		    (*set_rate)(struct clk *c, unsigned long rate);
	unsigned long	    (*get_rate)(struct clk *c);
	unsigned long	    (*round_rate)(struct clk *c, unsigned long rate);
	int		    (*set_parent)(struct clk *c, struct clk *parent);
};

struct clk {
	struct list_head      list;
	struct module        *owner;
	struct clk           *parent;
	const char           *name;
	const char		*devname;
	int		      id;
	int		      usage;
	unsigned long         rate;
	unsigned long         ctrlbit;

	struct clk_ops		*ops;
	int		    (*enable)(struct clk *, int enable);
	struct clk_lookup	lookup;
#if defined(CONFIG_PM_DEBUG) && defined(CONFIG_DEBUG_FS)
	struct dentry		*dent;	
#endif
};


extern struct clk s3c24xx_dclk0;
extern struct clk s3c24xx_dclk1;
extern struct clk s3c24xx_clkout0;
extern struct clk s3c24xx_clkout1;
extern struct clk s3c24xx_uclk;

extern struct clk clk_usb_bus;


extern struct clk clk_f;
extern struct clk clk_h;
extern struct clk clk_p;
extern struct clk clk_mpll;
extern struct clk clk_upll;
extern struct clk clk_epll;
extern struct clk clk_xtal;
extern struct clk clk_ext;

extern struct clksrc_clk clk_epllref;
extern struct clksrc_clk clk_esysclk;

extern struct clk clk_h2;
extern struct clk clk_27m;
extern struct clk clk_48m;
extern struct clk clk_xusbxti;

extern int clk_default_setrate(struct clk *clk, unsigned long rate);
extern struct clk_ops clk_ops_def_setrate;


extern spinlock_t clocks_lock;

extern int s3c2410_clkcon_enable(struct clk *clk, int enable);

extern int s3c24xx_register_clock(struct clk *clk);
extern int s3c24xx_register_clocks(struct clk **clk, int nr_clks);

extern void s3c_register_clocks(struct clk *clk, int nr_clks);
extern void s3c_disable_clocks(struct clk *clkp, int nr_clks);

extern int s3c24xx_register_baseclocks(unsigned long xtal);

extern void s5p_register_clocks(unsigned long xtal_freq);

extern void s3c24xx_setup_clocks(unsigned long fclk,
				 unsigned long hclk,
				 unsigned long pclk);

extern void s3c2410_setup_clocks(void);
extern void s3c2412_setup_clocks(void);
extern void s3c244x_setup_clocks(void);


extern int s3c2410_baseclk_add(void);


typedef unsigned int (*pll_fn)(unsigned int reg, unsigned int base);

extern void s3c2443_common_setup_clocks(pll_fn get_mpll);
extern void s3c2443_common_init_clocks(int xtal, pll_fn get_mpll,
				       unsigned int *divs, int nr_divs,
				       int divmask);

extern int s3c2443_clkcon_enable_h(struct clk *clk, int enable);
extern int s3c2443_clkcon_enable_p(struct clk *clk, int enable);
extern int s3c2443_clkcon_enable_s(struct clk *clk, int enable);


extern int s3c64xx_sclk_ctrl(struct clk *clk, int enable);


extern void s3c_pwmclk_init(void);


extern struct clk *s3c2410_wdtclk;

#endif 
