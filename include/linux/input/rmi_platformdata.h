/**
 *
 * Synaptics RMI platform data definitions for use in board files.
 * Copyright (c) 2007 - 2011, Synaptics Incorporated
 *
 */
/*
 * This file is licensed under the GPL2 license.
 *
 *############################################################################
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
 *############################################################################
 */

#if !defined(_RMI_PLATFORMDATA_H)
#define _RMI_PLATFORMDATA_H

#define RMI_F01_INDEX 0x01
#define RMI_F11_INDEX 0x11
#define RMI_F19_INDEX 0x19
#define RMI_F34_INDEX 0x34


struct rmi_XY_pair {
	int x;
	int y;
};

struct rmi_range {
	int min;
	int max;
};

struct rmi_sensordata {
	int (*rmi_sensor_setup)(void);

	void (*rmi_sensor_teardown)(void);

	struct rmi_functiondata_list *perfunctiondata;
};

struct rmi_functiondata {
	unsigned char	function_index;
	void		*data;
};

struct rmi_functiondata_list {
	unsigned char	count;	
	struct		rmi_functiondata *functiondata;
};

struct rmi_f01_functiondata {
	bool	nonstandard_report_rate;
};

struct rmi_f11_functiondata {
	bool		swap_axes;
	bool		flipX;
	bool		flipY;
	int		button_height;
	struct		rmi_XY_pair *offset;
	struct		rmi_range *clipX;
	struct		rmi_range *clipY;
};

struct rmi_button_map {
	unsigned char nbuttons;
	unsigned char *map;
};

struct rmi_f19_functiondata {
	struct rmi_button_map *button_map;
};

#endif
