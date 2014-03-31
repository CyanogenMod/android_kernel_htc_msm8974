/*
 * wdt.h
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * IO dispatcher for a shared memory channel driver.
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef __DSP_WDT3_H_
#define __DSP_WDT3_H_

#define OMAP3_WDT3_ISR_OFFSET	0x0018



struct dsp_wdt_setting {
	void __iomem *reg_base;
	struct shm *sm_wdt;
	struct tasklet_struct wdt3_tasklet;
	struct clk *fclk;
	struct clk *iclk;
};

int dsp_wdt_init(void);

void dsp_wdt_exit(void);

void dsp_wdt_enable(bool enable);

void dsp_wdt_sm_set(void *data);

#endif

