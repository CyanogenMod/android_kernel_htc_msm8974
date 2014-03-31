/*
 *  Support for the w100 frame buffer.
 *
 *  Copyright (c) 2004-2005 Richard Purdie
 *  Copyright (c) 2005 Ian Molton
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#define W100_GPIO_PORT_A	0
#define W100_GPIO_PORT_B	1

#define CLK_SRC_XTAL  0
#define CLK_SRC_PLL   1

struct w100fb_par;

unsigned long w100fb_gpio_read(int port);
void w100fb_gpio_write(int port, unsigned long value);
unsigned long w100fb_get_hsynclen(struct device *dev);

struct w100_tg_info {
	void (*change)(struct w100fb_par*);
	void (*suspend)(struct w100fb_par*);
	void (*resume)(struct w100fb_par*);
};

struct w100_gen_regs {
	unsigned long lcd_format;
	unsigned long lcdd_cntl1;
	unsigned long lcdd_cntl2;
	unsigned long genlcd_cntl1;
	unsigned long genlcd_cntl2;
	unsigned long genlcd_cntl3;
};

struct w100_gpio_regs {
	unsigned long init_data1;
	unsigned long init_data2;
	unsigned long gpio_dir1;
	unsigned long gpio_oe1;
	unsigned long gpio_dir2;
	unsigned long gpio_oe2;
};

struct w100_mem_info {
	unsigned long ext_cntl;
	unsigned long sdram_mode_reg;
	unsigned long ext_timing_cntl;
	unsigned long io_cntl;
	unsigned int size;
};

struct w100_bm_mem_info {
	unsigned long ext_mem_bw;
	unsigned long offset;
	unsigned long ext_timing_ctl;
	unsigned long ext_cntl;
	unsigned long mode_reg;
	unsigned long io_cntl;
	unsigned long config;
};

struct w100_mode {
	unsigned int xres;
	unsigned int yres;
	unsigned short left_margin;
	unsigned short right_margin;
	unsigned short upper_margin;
	unsigned short lower_margin;
	unsigned long crtc_ss;
	unsigned long crtc_ls;
	unsigned long crtc_gs;
	unsigned long crtc_vpos_gs;
	unsigned long crtc_rev;
	unsigned long crtc_dclk;
	unsigned long crtc_gclk;
	unsigned long crtc_goe;
	unsigned long crtc_ps1_active;
	char pll_freq;
	char fast_pll_freq;
	int sysclk_src;
	int sysclk_divider;
	int pixclk_src;
	int pixclk_divider;
	int pixclk_divider_rotated;
};

struct w100_pll_info {
	uint16_t freq;  
	uint8_t M;      
	uint8_t N_int;  
	uint8_t N_fac;  
	uint8_t tfgoal;
	uint8_t lock_time;
};

#define INIT_MODE_ROTATED  0x1
#define INIT_MODE_FLIPPED  0x2

struct w100fb_mach_info {
	
	struct w100_gen_regs *regs;
	
	struct w100_mode *modelist;
	unsigned int num_modes;
	
	struct w100_tg_info *tg;
	
	struct w100_mem_info *mem;
	
	struct w100_bm_mem_info *bm_mem;
	
	struct w100_gpio_regs *gpio;
	
	unsigned int init_mode;
	
	unsigned int xtal_freq;
	
	unsigned int xtal_dbl;
};

struct w100fb_par {
	unsigned int chip_id;
	unsigned int xres;
	unsigned int yres;
	unsigned int extmem_active;
	unsigned int flip;
	unsigned int blanked;
	unsigned int fastpll_mode;
	unsigned long hsync_len;
	struct w100_mode *mode;
	struct w100_pll_info *pll_table;
	struct w100fb_mach_info *mach;
	uint32_t *saved_intmem;
	uint32_t *saved_extmem;
};
