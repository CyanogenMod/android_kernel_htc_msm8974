/* linux/arch/arm/plat-samsung/include/plat/ata.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung CF-ATA platform_device info
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_PLAT_ATA_H
#define __ASM_PLAT_ATA_H __FILE__

struct s3c_ide_platdata {
	void (*setup_gpio)(void);
};

extern void s3c_ide_set_platdata(struct s3c_ide_platdata *pdata);

extern void s3c64xx_ide_setup_gpio(void);
extern void s5pc100_ide_setup_gpio(void);
extern void s5pv210_ide_setup_gpio(void);

#endif 
