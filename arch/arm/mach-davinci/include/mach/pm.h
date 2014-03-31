/*
 * TI DaVinci platform support for power management.
 *
 * Copyright (C) 2009 Texas Instruments, Inc. http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _MACH_DAVINCI_PM_H
#define _MACH_DAVINCI_PM_H

struct davinci_pm_config {
	void __iomem *ddr2_ctlr_base;
	void __iomem *ddrpsc_reg_base;
	int ddrpsc_num;
	void __iomem *ddrpll_reg_base;
	void __iomem *deepsleep_reg;
	void __iomem *cpupll_reg_base;
	int sleepcount;
};

extern unsigned int davinci_cpu_suspend_sz;
extern void davinci_cpu_suspend(struct davinci_pm_config *);

#endif
