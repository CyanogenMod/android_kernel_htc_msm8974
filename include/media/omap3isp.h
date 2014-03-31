/*
 * omap3isp.h
 *
 * TI OMAP3 ISP - Platform data
 *
 * Copyright (C) 2011 Nokia Corporation
 *
 * Contacts: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *	     Sakari Ailus <sakari.ailus@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef __MEDIA_OMAP3ISP_H__
#define __MEDIA_OMAP3ISP_H__

struct i2c_board_info;
struct isp_device;

enum isp_interface_type {
	ISP_INTERFACE_PARALLEL,
	ISP_INTERFACE_CSI2A_PHY2,
	ISP_INTERFACE_CCP2B_PHY1,
	ISP_INTERFACE_CCP2B_PHY2,
	ISP_INTERFACE_CSI2C_PHY1,
};

enum {
	ISP_BRIDGE_DISABLE = 0,
	ISP_BRIDGE_LITTLE_ENDIAN = 2,
	ISP_BRIDGE_BIG_ENDIAN = 3,
};

enum {
	ISP_LANE_SHIFT_0 = 0,
	ISP_LANE_SHIFT_2 = 1,
	ISP_LANE_SHIFT_4 = 2,
	ISP_LANE_SHIFT_6 = 3,
};

struct isp_parallel_platform_data {
	unsigned int data_lane_shift:2;
	unsigned int clk_pol:1;
	unsigned int hs_pol:1;
	unsigned int vs_pol:1;
	unsigned int bridge:2;
};

enum {
	ISP_CCP2_PHY_DATA_CLOCK = 0,
	ISP_CCP2_PHY_DATA_STROBE = 1,
};

enum {
	ISP_CCP2_MODE_MIPI = 0,
	ISP_CCP2_MODE_CCP2 = 1,
};

struct isp_ccp2_platform_data {
	unsigned int strobe_clk_pol:1;
	unsigned int crc:1;
	unsigned int ccp2_mode:1;
	unsigned int phy_layer:1;
	unsigned int vpclk_div:2;
};

struct isp_csi2_platform_data {
	unsigned crc:1;
	unsigned vpclk_div:2;
};

struct isp_subdev_i2c_board_info {
	struct i2c_board_info *board_info;
	int i2c_adapter_id;
};

struct isp_v4l2_subdevs_group {
	struct isp_subdev_i2c_board_info *subdevs;
	enum isp_interface_type interface;
	union {
		struct isp_parallel_platform_data parallel;
		struct isp_ccp2_platform_data ccp2;
		struct isp_csi2_platform_data csi2;
	} bus; 
};

struct isp_platform_data {
	struct isp_v4l2_subdevs_group *subdevs;
	void (*set_constraints)(struct isp_device *isp, bool enable);
};

#endif	
