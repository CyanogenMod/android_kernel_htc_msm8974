/* include/video/exynos_mipi_dsim.h
 *
 * Platform data header for Samsung SoC MIPI-DSIM.
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd
 *
 * InKi Dae <inki.dae@samsung.com>
 * Donghwa Lee <dh09.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _EXYNOS_MIPI_DSIM_H
#define _EXYNOS_MIPI_DSIM_H

#include <linux/device.h>
#include <linux/fb.h>

#define PANEL_NAME_SIZE		(32)

enum mipi_dsim_interface_type {
	DSIM_COMMAND,
	DSIM_VIDEO
};

enum mipi_dsim_virtual_ch_no {
	DSIM_VIRTUAL_CH_0,
	DSIM_VIRTUAL_CH_1,
	DSIM_VIRTUAL_CH_2,
	DSIM_VIRTUAL_CH_3
};

enum mipi_dsim_burst_mode_type {
	DSIM_NON_BURST_SYNC_EVENT,
	DSIM_BURST_SYNC_EVENT,
	DSIM_NON_BURST_SYNC_PULSE,
	DSIM_BURST,
	DSIM_NON_VIDEO_MODE
};

enum mipi_dsim_no_of_data_lane {
	DSIM_DATA_LANE_1,
	DSIM_DATA_LANE_2,
	DSIM_DATA_LANE_3,
	DSIM_DATA_LANE_4
};

enum mipi_dsim_byte_clk_src {
	DSIM_PLL_OUT_DIV8,
	DSIM_EXT_CLK_DIV8,
	DSIM_EXT_CLK_BYPASS
};

enum mipi_dsim_pixel_format {
	DSIM_CMD_3BPP,
	DSIM_CMD_8BPP,
	DSIM_CMD_12BPP,
	DSIM_CMD_16BPP,
	DSIM_VID_16BPP_565,
	DSIM_VID_18BPP_666PACKED,
	DSIM_18BPP_666LOOSELYPACKED,
	DSIM_24BPP_888
};

struct mipi_dsim_config {
	unsigned char			auto_flush;
	unsigned char			eot_disable;

	unsigned char			auto_vertical_cnt;
	unsigned char			hse;
	unsigned char			hfp;
	unsigned char			hbp;
	unsigned char			hsa;
	unsigned char			cmd_allow;

	enum mipi_dsim_interface_type	e_interface;
	enum mipi_dsim_virtual_ch_no	e_virtual_ch;
	enum mipi_dsim_pixel_format	e_pixel_format;
	enum mipi_dsim_burst_mode_type	e_burst_mode;
	enum mipi_dsim_no_of_data_lane	e_no_data_lane;
	enum mipi_dsim_byte_clk_src	e_byte_clk;


	unsigned char			p;
	unsigned short			m;
	unsigned char			s;

	unsigned int			pll_stable_time;
	unsigned long			esc_clk;

	unsigned short			stop_holding_cnt;
	unsigned char			bta_timeout;
	unsigned short			rx_timeout;
};

struct mipi_dsim_device {
	struct device			*dev;
	int				id;
	struct resource			*res;
	struct clk			*clock;
	unsigned int			irq;
	void __iomem			*reg_base;
	struct mutex			lock;

	struct mipi_dsim_config		*dsim_config;
	struct mipi_dsim_master_ops	*master_ops;
	struct mipi_dsim_lcd_device	*dsim_lcd_dev;
	struct mipi_dsim_lcd_driver	*dsim_lcd_drv;

	unsigned int			state;
	unsigned int			data_lane;
	unsigned int			e_clk_src;
	bool				suspended;

	struct mipi_dsim_platform_data	*pd;
};

struct mipi_dsim_platform_data {
	char				lcd_panel_name[PANEL_NAME_SIZE];

	struct mipi_dsim_config		*dsim_config;
	unsigned int			enabled;
	void				*lcd_panel_info;

	int (*phy_enable)(struct platform_device *pdev, bool on);
};


struct mipi_dsim_master_ops {
	int (*cmd_write)(struct mipi_dsim_device *dsim, unsigned int data_id,
		const unsigned char *data0, unsigned int data1);
	int (*cmd_read)(struct mipi_dsim_device *dsim, unsigned int data_id,
		unsigned int data0, unsigned int req_size, u8 *rx_buf);
	int (*get_dsim_frame_done)(struct mipi_dsim_device *dsim);
	int (*clear_dsim_frame_done)(struct mipi_dsim_device *dsim);

	int (*get_fb_frame_done)(struct fb_info *info);
	void (*trigger)(struct fb_info *info);
	int (*set_early_blank_mode)(struct mipi_dsim_device *dsim, int power);
	int (*set_blank_mode)(struct mipi_dsim_device *dsim, int power);
};

struct mipi_dsim_lcd_device {
	char			*name;
	struct device		dev;
	int			id;
	int			bus_id;
	int			irq;

	struct mipi_dsim_device *master;
	void			*platform_data;
};

struct mipi_dsim_lcd_driver {
	char			*name;
	int			id;

	void	(*power_on)(struct mipi_dsim_lcd_device *dsim_dev, int enable);
	void	(*set_sequence)(struct mipi_dsim_lcd_device *dsim_dev);
	int	(*probe)(struct mipi_dsim_lcd_device *dsim_dev);
	int	(*remove)(struct mipi_dsim_lcd_device *dsim_dev);
	void	(*shutdown)(struct mipi_dsim_lcd_device *dsim_dev);
	int	(*suspend)(struct mipi_dsim_lcd_device *dsim_dev);
	int	(*resume)(struct mipi_dsim_lcd_device *dsim_dev);
};

int exynos_mipi_dsi_register_lcd_device(struct mipi_dsim_lcd_device
						*lcd_dev);
int exynos_mipi_dsi_register_lcd_driver(struct mipi_dsim_lcd_driver
						*lcd_drv);
#endif 
