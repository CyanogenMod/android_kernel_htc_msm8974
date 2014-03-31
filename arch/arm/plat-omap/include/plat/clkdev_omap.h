
#ifndef __ARCH_ARM_PLAT_OMAP_INCLUDE_PLAT_CLKDEV_OMAP_H
#define __ARCH_ARM_PLAT_OMAP_INCLUDE_PLAT_CLKDEV_OMAP_H

#include <linux/clkdev.h>

struct omap_clk {
	u16				cpu;
	struct clk_lookup		lk;
};

#define CLK(dev, con, ck, cp) 		\
	{				\
		 .cpu = cp,		\
		.lk = {			\
			.dev_id = dev,	\
			.con_id = con,	\
			.clk = ck,	\
		},			\
	}

#define CK_310		(1 << 0)
#define CK_7XX		(1 << 1)	
#define CK_1510		(1 << 2)
#define CK_16XX		(1 << 3)	
#define CK_242X		(1 << 4)
#define CK_243X		(1 << 5)	
#define CK_3430ES1	(1 << 6)	
#define CK_3430ES2PLUS	(1 << 7)	
#define CK_3505		(1 << 8)
#define CK_3517		(1 << 9)
#define CK_36XX		(1 << 10)	
#define CK_443X		(1 << 11)
#define CK_TI816X	(1 << 12)
#define CK_446X		(1 << 13)
#define CK_1710		(1 << 15)	


#define CK_34XX		(CK_3430ES1 | CK_3430ES2PLUS)
#define CK_AM35XX	(CK_3505 | CK_3517)	
#define CK_3XXX		(CK_34XX | CK_AM35XX | CK_36XX)


#endif

