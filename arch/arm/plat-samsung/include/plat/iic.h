/* arch/arm/plat-s3c/include/plat/iic.h
 *
 * Copyright 2004-2009 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C - I2C Controller platform_device info
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_IIC_H
#define __ASM_ARCH_IIC_H __FILE__

#define S3C_IICFLG_FILTER	(1<<0)	

struct s3c2410_platform_i2c {
	int		bus_num;
	unsigned int	flags;
	unsigned int	slave_addr;
	unsigned long	frequency;
	unsigned int	sda_delay;

	void	(*cfg_gpio)(struct platform_device *dev);
};

extern void s3c_i2c0_set_platdata(struct s3c2410_platform_i2c *i2c);
extern void s3c_i2c1_set_platdata(struct s3c2410_platform_i2c *i2c);
extern void s3c_i2c2_set_platdata(struct s3c2410_platform_i2c *i2c);
extern void s3c_i2c3_set_platdata(struct s3c2410_platform_i2c *i2c);
extern void s3c_i2c4_set_platdata(struct s3c2410_platform_i2c *i2c);
extern void s3c_i2c5_set_platdata(struct s3c2410_platform_i2c *i2c);
extern void s3c_i2c6_set_platdata(struct s3c2410_platform_i2c *i2c);
extern void s3c_i2c7_set_platdata(struct s3c2410_platform_i2c *i2c);
extern void s5p_i2c_hdmiphy_set_platdata(struct s3c2410_platform_i2c *i2c);

extern void s3c_i2c0_cfg_gpio(struct platform_device *dev);
extern void s3c_i2c1_cfg_gpio(struct platform_device *dev);
extern void s3c_i2c2_cfg_gpio(struct platform_device *dev);
extern void s3c_i2c3_cfg_gpio(struct platform_device *dev);
extern void s3c_i2c4_cfg_gpio(struct platform_device *dev);
extern void s3c_i2c5_cfg_gpio(struct platform_device *dev);
extern void s3c_i2c6_cfg_gpio(struct platform_device *dev);
extern void s3c_i2c7_cfg_gpio(struct platform_device *dev);

extern struct s3c2410_platform_i2c default_i2c_data;

#endif 
