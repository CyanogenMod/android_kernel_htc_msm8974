/*
 * Copyright ST-Ericsson 2010.
 *
 * Author: Bibek Basu <bibek.basu@stericsson.com>
 * Licensed under GPLv2.
 */

#ifndef _AB8500_GPIO_H
#define _AB8500_GPIO_H


struct ab8500_gpio_platform_data {
	int gpio_base;
	u32 irq_base;
	u8  config_reg[8];
};

#endif 
