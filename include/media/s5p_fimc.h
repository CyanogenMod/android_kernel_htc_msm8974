/*
 * Samsung S5P SoC camera interface driver header
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd
 * Author: Sylwester Nawrocki, <s.nawrocki@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef S5P_FIMC_H_
#define S5P_FIMC_H_

enum cam_bus_type {
	FIMC_ITU_601 = 1,
	FIMC_ITU_656,
	FIMC_MIPI_CSI2,
	FIMC_LCD_WB, 
};

struct i2c_board_info;

struct s5p_fimc_isp_info {
	struct i2c_board_info *board_info;
	unsigned long clk_frequency;
	enum cam_bus_type bus_type;
	u16 csi_data_align;
	u16 i2c_bus_num;
	u16 mux_id;
	u16 flags;
	u8 clk_id;
};

struct s5p_platform_fimc {
	struct s5p_fimc_isp_info *isp_info;
	int num_clients;
};

#define S5P_FIMC_TX_END_NOTIFY _IO('e', 0)

#endif 
