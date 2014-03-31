/*
 * Header file for I2C support on PNX010x/4008.
 *
 * Author: Dennis Kovalev <dkovalev@ru.mvista.com>
 *
 * 2004-2006 (c) MontaVista Software, Inc. This file is licensed under
 * the terms of the GNU General Public License version 2. This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */

#ifndef __I2C_PNX_H__
#define __I2C_PNX_H__

struct platform_device;
struct clk;

struct i2c_pnx_mif {
	int			ret;		
	int			mode;		
	struct completion	complete;	
	struct timer_list	timer;		
	u8 *			buf;		
	int			len;		
};

struct i2c_pnx_algo_data {
	void __iomem		*ioaddr;
	struct i2c_pnx_mif	mif;
	int			last;
	struct clk		*clk;
	struct i2c_pnx_data	*i2c_pnx;
	struct i2c_adapter	adapter;
};

struct i2c_pnx_data {
	const char *name;
	u32 base;
	int irq;
};

#endif 
