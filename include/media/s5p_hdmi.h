/*
 * Driver header for S5P HDMI chip.
 *
 * Copyright (c) 2011 Samsung Electronics, Co. Ltd
 * Contact: Tomasz Stanislawski <t.stanislaws@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef S5P_HDMI_H
#define S5P_HDMI_H

struct i2c_board_info;

struct s5p_hdmi_platform_data {
	int hdmiphy_bus;
	struct i2c_board_info *hdmiphy_info;
	int mhl_bus;
	struct i2c_board_info *mhl_info;
};

#endif 

