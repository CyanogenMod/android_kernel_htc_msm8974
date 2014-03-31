#ifndef _AS5011_H
#define _AS5011_H

/*
 * Copyright (c) 2010, 2011 Fabien Marteau <fabien.marteau@armadeus.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

struct as5011_platform_data {
	unsigned int button_gpio;
	unsigned int axis_irq; 
	unsigned long axis_irqflags;
	char xp, xn; 
	char yp, yn; 
};

#endif 
