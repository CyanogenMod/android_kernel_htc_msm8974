/**
 *
 * Synaptics RMI over I2C Physical Layer Driver Header File.
 * Copyright (c) 2007 - 2011, Synaptics Incorporated
 *
 */
/*
 * This file is licensed under the GPL2 license.
 *
 *#############################################################################
 * GPL
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 *#############################################################################
 */

#ifndef _RMI_I2C_H
#define _RMI_I2C_H

#include <linux/input/rmi_platformdata.h>

struct rmi_i2c_platformdata {
	
	int i2c_address;
	
	int irq;
	int irq_type;

	int delay_ms;

	
	struct rmi_sensordata *sensordata;
};

#endif
