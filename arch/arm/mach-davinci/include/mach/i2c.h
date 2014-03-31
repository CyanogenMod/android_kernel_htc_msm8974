/*
 * DaVinci I2C controller platform_device info
 *
 * Author: Vladimir Barinov, MontaVista Software, Inc. <source@mvista.com>
 *
 * 2007 (c) MontaVista Software, Inc. This file is licensed under
 * the terms of the GNU General Public License version 2. This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
*/

#ifndef __ASM_ARCH_I2C_H
#define __ASM_ARCH_I2C_H

struct davinci_i2c_platform_data {
	unsigned int	bus_freq;	
	unsigned int	bus_delay;	
	unsigned int    sda_pin;        
	unsigned int    scl_pin;        
};

void davinci_init_i2c(struct davinci_i2c_platform_data *);

#endif 
