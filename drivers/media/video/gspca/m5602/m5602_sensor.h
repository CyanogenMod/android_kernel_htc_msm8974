/*
 * USB Driver for ALi m5602 based webcams
 *
 * Copyright (C) 2008 Erik Andrén
 * Copyright (C) 2007 Ilyes Gouta. Based on the m5603x Linux Driver Project.
 * Copyright (C) 2005 m5603x Linux Driver Project <m5602@x3ng.com.br>
 *
 * Portions of code to USB interface and ALi driver software,
 * Copyright (c) 2006 Willem Duinker
 * v4l2 interface modeled after the V4L2 driver
 * for SN9C10x PC Camera Controllers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 *
 */

#ifndef M5602_SENSOR_H_
#define M5602_SENSOR_H_

#include "m5602_bridge.h"

#define M5602_V4L2_CID_GREEN_BALANCE	(V4L2_CID_PRIVATE_BASE + 0)
#define M5602_V4L2_CID_NOISE_SUPPRESION	(V4L2_CID_PRIVATE_BASE + 1)

enum sensors {
	OV9650_SENSOR	= 1,
	S5K83A_SENSOR	= 2,
	S5K4AA_SENSOR	= 3,
	MT9M111_SENSOR	= 4,
	PO1030_SENSOR	= 5,
	OV7660_SENSOR   = 6,
};

enum instruction {
	BRIDGE,
	SENSOR,
	SENSOR_LONG
};

struct m5602_sensor {
	
	char name[32];

	
	u8 i2c_slave_id;

	
	u8 i2c_regW;

	
	int (*probe)(struct sd *sd);

	
	int (*init)(struct sd *sd);

	
	int (*start)(struct sd *sd);

	
	int (*stop)(struct sd *sd);

	
	void (*disconnect)(struct sd *sd);
};

#endif
