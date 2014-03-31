/* arch/arm/mach-s3c2410/include/mach/gpio-fns.h
 *
 * Copyright (c) 2003-2009 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C2410 - hardware
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __MACH_GPIO_FNS_H
#define __MACH_GPIO_FNS_H __FILE__


#include <plat/gpio-cfg.h>

static inline void s3c2410_gpio_cfgpin(unsigned int pin, unsigned int cfg)
{
	
	s3c_gpio_cfgpin(pin, cfg);
}


extern unsigned int s3c2410_gpio_getcfg(unsigned int pin);


extern int s3c2410_gpio_getirq(unsigned int pin);


extern int s3c2410_gpio_irqfilter(unsigned int pin, unsigned int on,
				  unsigned int config);



extern void s3c2410_gpio_pullup(unsigned int pin, unsigned int to);

extern void s3c2410_gpio_setpin(unsigned int pin, unsigned int to);

extern unsigned int s3c2410_gpio_getpin(unsigned int pin);

#endif 
