/* linux/i2c/tps6507x-ts.h
 *
 * Functions to access TPS65070 touch screen chip.
 *
 * Copyright (c) 2009 RidgeRun (todd.fischer@ridgerun.com)
 *
 *
 *  For licencing details see kernel-base/COPYING
 */

#ifndef __LINUX_I2C_TPS6507X_TS_H
#define __LINUX_I2C_TPS6507X_TS_H

struct touchscreen_init_data {
	int	poll_period;	
	int	vref;		
	__u16	min_pressure;	
	__u16	vendor;
	__u16	product;
	__u16	version;
};

#endif 
