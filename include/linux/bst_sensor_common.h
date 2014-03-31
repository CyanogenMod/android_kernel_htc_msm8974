/*
 * Created on Sep 19th, 2012, by guz4sgh
 * Last modified: Sep 19th, 2012
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2012 Bosch Sensortec GmbH
 * All Rights Reserved
 */


#ifndef __BOSCH_SENSOR_COMMON_H
#define __BOSCH_SENSOR_COMMON_H

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/string.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#endif



struct bosch_sensor_specific {
	char *name;
	
	unsigned int place:3;
};


typedef struct bosch_sensor_axis_remap {
	
	int src_x:3;
	int src_y:3;
	int src_z:3;

	int sign_x:2;
	int sign_y:2;
	int sign_z:2;
} bosch_sensor_axis_remap;


struct bosch_sensor_data {
	union {
		int16_t v[3];
		struct {
			int16_t x;
			int16_t y;
			int16_t z;
		};
	};
};

extern unsigned int gs_kvalue;
extern unsigned char gyro_gsensor_kvalue[37];

void bst_remap_sensor_data(struct bosch_sensor_data *data,
		const bosch_sensor_axis_remap *remap);

void bst_remap_sensor_data_dft_tab(struct bosch_sensor_data *data,
		int place);
#endif
