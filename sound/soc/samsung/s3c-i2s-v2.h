/* sound/soc/samsung/s3c-i2s-v2.h
 *
 * ALSA Soc Audio Layer - S3C_I2SV2 I2S driver
 *
 * Copyright (c) 2007 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
*/


#ifndef __SND_SOC_S3C24XX_S3C_I2SV2_I2S_H
#define __SND_SOC_S3C24XX_S3C_I2SV2_I2S_H __FILE__

#define S3C_I2SV2_DIV_BCLK	(1)
#define S3C_I2SV2_DIV_RCLK	(2)
#define S3C_I2SV2_DIV_PRESCALER	(3)

#define S3C_I2SV2_CLKSRC_PCLK		0
#define S3C_I2SV2_CLKSRC_AUDIOBUS	1
#define S3C_I2SV2_CLKSRC_CDCLK		2

#define S3C_FEATURE_CDCLKCON	(1 << 0)

struct s3c_i2sv2_info {
	struct device	*dev;
	void __iomem	*regs;

	u32		feature;

	struct clk	*iis_pclk;
	struct clk	*iis_cclk;

	unsigned char	 master;

	struct s3c_dma_params	*dma_playback;
	struct s3c_dma_params	*dma_capture;

	u32		 suspend_iismod;
	u32		 suspend_iiscon;
	u32		 suspend_iispsr;

	unsigned long	base;
};

extern struct clk *s3c_i2sv2_get_clock(struct snd_soc_dai *cpu_dai);

struct s3c_i2sv2_rate_calc {
	unsigned int	clk_div;	
	unsigned int	fs_div;		
};

extern int s3c_i2sv2_iis_calc_rate(struct s3c_i2sv2_rate_calc *info,
				   unsigned int *fstab,
				   unsigned int rate, struct clk *clk);

extern int s3c_i2sv2_probe(struct snd_soc_dai *dai,
			   struct s3c_i2sv2_info *i2s,
			   unsigned long base);

extern int s3c_i2sv2_register_dai(struct device *dev, int id,
		struct snd_soc_dai_driver *drv);

#endif 
