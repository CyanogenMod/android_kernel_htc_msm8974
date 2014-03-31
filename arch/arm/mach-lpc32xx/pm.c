/*
 * arch/arm/mach-lpc32xx/pm.c
 *
 * Original authors: Vitaly Wool, Dmitry Chigirev <source@mvista.com>
 * Modified by Kevin Wells <kevin.wells@nxp.com>
 *
 * 2005 (c) MontaVista Software, Inc. This file is licensed under
 * the terms of the GNU General Public License version 2. This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */


#include <linux/suspend.h>
#include <linux/io.h>
#include <linux/slab.h>

#include <asm/cacheflush.h>

#include <mach/hardware.h>
#include <mach/platform.h>
#include "common.h"
#include "clock.h"

#define TEMP_IRAM_AREA  IO_ADDRESS(LPC32XX_IRAM_BASE)

static int lpc32xx_pm_enter(suspend_state_t state)
{
	int (*lpc32xx_suspend_ptr) (void);
	void *iram_swap_area;

	
	iram_swap_area = kmalloc(lpc32xx_sys_suspend_sz, GFP_KERNEL);
	if (!iram_swap_area) {
		printk(KERN_ERR
		       "PM Suspend: cannot allocate memory to save portion "
			"of SRAM\n");
		return -ENOMEM;
	}

	
	memcpy(iram_swap_area, (void *) TEMP_IRAM_AREA,
		lpc32xx_sys_suspend_sz);

	memcpy((void *) TEMP_IRAM_AREA, &lpc32xx_sys_suspend,
		lpc32xx_sys_suspend_sz);
	flush_icache_range((unsigned long)TEMP_IRAM_AREA,
		(unsigned long)(TEMP_IRAM_AREA) + lpc32xx_sys_suspend_sz);

	
	lpc32xx_suspend_ptr = (void *) TEMP_IRAM_AREA;
	flush_cache_all();
	(void) lpc32xx_suspend_ptr();

	
	memcpy((void *) TEMP_IRAM_AREA, iram_swap_area,
		lpc32xx_sys_suspend_sz);

	kfree(iram_swap_area);

	return 0;
}

static const struct platform_suspend_ops lpc32xx_pm_ops = {
	.valid	= suspend_valid_only_mem,
	.enter	= lpc32xx_pm_enter,
};

#define EMC_DYN_MEM_CTRL_OFS 0x20
#define EMC_SRMMC           (1 << 3)
#define EMC_CTRL_REG io_p2v(LPC32XX_EMC_BASE + EMC_DYN_MEM_CTRL_OFS)
static int __init lpc32xx_pm_init(void)
{
	__raw_writel(__raw_readl(EMC_CTRL_REG) | EMC_SRMMC, EMC_CTRL_REG);

	suspend_set_ops(&lpc32xx_pm_ops);

	return 0;
}
arch_initcall(lpc32xx_pm_init);
