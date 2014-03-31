/* arch/arm/plat-samsung/include/plat/wakeup-mask.h
 *
 * Copyright 2010 Ben Dooks <ben-linux@fluff.org>
 *
 * Support for wakeup mask interrupts on newer SoCs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

#ifndef __PLAT_WAKEUP_MASK_H
#define __PLAT_WAKEUP_MASK_H __file__

#define NO_WAKEUP_IRQ (0x90000000)

 
struct samsung_wakeup_mask {
	unsigned int	irq;
	u32		bit;
};

extern void samsung_sync_wakemask(void __iomem *reg,
				  struct samsung_wakeup_mask *masks,
				  int nr_masks);

#endif 
