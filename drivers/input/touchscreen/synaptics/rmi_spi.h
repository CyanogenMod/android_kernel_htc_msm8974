/**
 *
 * Register Mapped Interface SPI Physical Layer Driver Header File.
 * Copyright (C) 2008-2011, Synaptics Incorporated
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

#if !defined(_RMI_SPI_H)
#define _RMI_SPI_H

#include <linux/input/rmi_platformdata.h>

#define RMI_CHIP_VER_3	0
#define RMI_CHIP_VER_4	1

#define RMI_SUPPORT (RMI_CHIP_VER_3|RMI_CHIP_VER_4)

#define RMI4_SPI_DRIVER_NAME "rmi4_ts"
#define RMI4_SPI_DEVICE_NAME "rmi4_ts"

struct rmi_spi_platformdata {
	int chip;

	
	int irq;

	int irq_type;

	
	struct rmi_sensordata *sensordata;
};

#endif
