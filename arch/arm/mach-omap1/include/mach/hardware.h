
#ifndef __MACH_HARDWARE_H
#define __MACH_HARDWARE_H

#ifndef __ASSEMBLER__
extern u8 omap_readb(u32 pa);
extern u16 omap_readw(u32 pa);
extern u32 omap_readl(u32 pa);
extern void omap_writeb(u8 v, u32 pa);
extern void omap_writew(u16 v, u32 pa);
extern void omap_writel(u32 v, u32 pa);

#include <plat/tc.h>

static inline u32 omap_cs0m_phys(void)
{
	return (omap_readl(EMIFS_CONFIG) & OMAP_EMIFS_CONFIG_BM)
			?  OMAP_CS3_PHYS : 0;
}

static inline u32 omap_cs3_phys(void)
{
	return (omap_readl(EMIFS_CONFIG) & OMAP_EMIFS_CONFIG_BM)
			? 0 : OMAP_CS3_PHYS;
}

#endif
#endif

#include <plat/hardware.h>
