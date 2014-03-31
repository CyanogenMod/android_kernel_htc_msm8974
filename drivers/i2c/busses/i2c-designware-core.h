/*
 * Synopsys DesignWare I2C adapter driver (master only).
 *
 * Based on the TI DAVINCI I2C adapter driver.
 *
 * Copyright (C) 2006 Texas Instruments.
 * Copyright (C) 2007 MontaVista Software Inc.
 * Copyright (C) 2009 Provigent Ltd.
 *
 * ----------------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * ----------------------------------------------------------------------------
 *
 */


#define DW_IC_CON_MASTER		0x1
#define DW_IC_CON_SPEED_STD		0x2
#define DW_IC_CON_SPEED_FAST		0x4
#define DW_IC_CON_10BITADDR_MASTER	0x10
#define DW_IC_CON_RESTART_EN		0x20
#define DW_IC_CON_SLAVE_DISABLE		0x40


struct dw_i2c_dev {
	struct device		*dev;
	void __iomem		*base;
	struct completion	cmd_complete;
	struct mutex		lock;
	struct clk		*clk;
	u32			(*get_clk_rate_khz) (struct dw_i2c_dev *dev);
	struct dw_pci_controller *controller;
	int			cmd_err;
	struct i2c_msg		*msgs;
	int			msgs_num;
	int			msg_write_idx;
	u32			tx_buf_len;
	u8			*tx_buf;
	int			msg_read_idx;
	u32			rx_buf_len;
	u8			*rx_buf;
	int			msg_err;
	unsigned int		status;
	u32			abort_source;
	int			irq;
	int			swab;
	struct i2c_adapter	adapter;
	u32			functionality;
	u32			master_cfg;
	unsigned int		tx_fifo_depth;
	unsigned int		rx_fifo_depth;
};

extern u32 dw_readl(struct dw_i2c_dev *dev, int offset);
extern void dw_writel(struct dw_i2c_dev *dev, u32 b, int offset);
extern int i2c_dw_init(struct dw_i2c_dev *dev);
extern int i2c_dw_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[],
		int num);
extern u32 i2c_dw_func(struct i2c_adapter *adap);
extern irqreturn_t i2c_dw_isr(int this_irq, void *dev_id);
extern void i2c_dw_enable(struct dw_i2c_dev *dev);
extern u32 i2c_dw_is_enabled(struct dw_i2c_dev *dev);
extern void i2c_dw_disable(struct dw_i2c_dev *dev);
extern void i2c_dw_clear_int(struct dw_i2c_dev *dev);
extern void i2c_dw_disable_int(struct dw_i2c_dev *dev);
extern u32 i2c_dw_read_comp_param(struct dw_i2c_dev *dev);
