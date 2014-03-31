#include <linux/kernel.h>
#include <linux/io.h>

#include <plat/prcm.h>

#include <mach/hardware.h>

void omap1_restart(char mode, const char *cmd)
{
	if (cpu_is_omap5912()) {
		omap_writew(omap_readw(DPLL_CTL) & ~(1 << 4), DPLL_CTL);
		omap_writew(0x8, ARM_RSTCT1);
	}

	omap_writew(1, ARM_RSTCT1);
}
