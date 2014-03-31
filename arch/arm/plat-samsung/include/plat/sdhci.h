/* linux/arch/arm/plat-samsung/include/plat/sdhci.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C Platform - SDHCI (HSMMC) platform data definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __PLAT_S3C_SDHCI_H
#define __PLAT_S3C_SDHCI_H __FILE__

#include <plat/devs.h>

struct platform_device;
struct mmc_host;
struct mmc_card;
struct mmc_ios;

enum cd_types {
	S3C_SDHCI_CD_INTERNAL,	
	S3C_SDHCI_CD_EXTERNAL,	
	S3C_SDHCI_CD_GPIO,	
	S3C_SDHCI_CD_NONE,	
	S3C_SDHCI_CD_PERMANENT,	
};

enum clk_types {
	S3C_SDHCI_CLK_DIV_INTERNAL,	
	S3C_SDHCI_CLK_DIV_EXTERNAL,	
};

struct s3c_sdhci_platdata {
	unsigned int	max_width;
	unsigned int	host_caps;
	unsigned int	host_caps2;
	unsigned int	pm_caps;
	enum cd_types	cd_type;
	enum clk_types	clk_type;

	int		ext_cd_gpio;
	bool		ext_cd_gpio_invert;
	int	(*ext_cd_init)(void (*notify_func)(struct platform_device *,
						   int state));
	int	(*ext_cd_cleanup)(void (*notify_func)(struct platform_device *,
						      int state));

	void	(*cfg_gpio)(struct platform_device *dev, int width);
};

extern void s3c_sdhci_set_platdata(struct s3c_sdhci_platdata *pd,
				    struct s3c_sdhci_platdata *set);

extern void s3c_sdhci0_set_platdata(struct s3c_sdhci_platdata *pd);
extern void s3c_sdhci1_set_platdata(struct s3c_sdhci_platdata *pd);
extern void s3c_sdhci2_set_platdata(struct s3c_sdhci_platdata *pd);
extern void s3c_sdhci3_set_platdata(struct s3c_sdhci_platdata *pd);


extern struct s3c_sdhci_platdata s3c_hsmmc0_def_platdata;
extern struct s3c_sdhci_platdata s3c_hsmmc1_def_platdata;
extern struct s3c_sdhci_platdata s3c_hsmmc2_def_platdata;
extern struct s3c_sdhci_platdata s3c_hsmmc3_def_platdata;


extern void s3c2416_setup_sdhci0_cfg_gpio(struct platform_device *, int w);
extern void s3c2416_setup_sdhci1_cfg_gpio(struct platform_device *, int w);
extern void s3c64xx_setup_sdhci0_cfg_gpio(struct platform_device *, int w);
extern void s3c64xx_setup_sdhci1_cfg_gpio(struct platform_device *, int w);
extern void s5pc100_setup_sdhci0_cfg_gpio(struct platform_device *, int w);
extern void s5pc100_setup_sdhci1_cfg_gpio(struct platform_device *, int w);
extern void s5pc100_setup_sdhci2_cfg_gpio(struct platform_device *, int w);
extern void s3c64xx_setup_sdhci2_cfg_gpio(struct platform_device *, int w);
extern void s5pv210_setup_sdhci0_cfg_gpio(struct platform_device *, int w);
extern void s5pv210_setup_sdhci1_cfg_gpio(struct platform_device *, int w);
extern void s5pv210_setup_sdhci2_cfg_gpio(struct platform_device *, int w);
extern void s5pv210_setup_sdhci3_cfg_gpio(struct platform_device *, int w);
extern void exynos4_setup_sdhci0_cfg_gpio(struct platform_device *, int w);
extern void exynos4_setup_sdhci1_cfg_gpio(struct platform_device *, int w);
extern void exynos4_setup_sdhci2_cfg_gpio(struct platform_device *, int w);
extern void exynos4_setup_sdhci3_cfg_gpio(struct platform_device *, int w);
extern void s5p64x0_setup_sdhci0_cfg_gpio(struct platform_device *, int w);
extern void s5p64x0_setup_sdhci1_cfg_gpio(struct platform_device *, int w);
extern void s5p6440_setup_sdhci2_cfg_gpio(struct platform_device *, int w);
extern void s5p6450_setup_sdhci2_cfg_gpio(struct platform_device *, int w);


#ifdef CONFIG_S3C2416_SETUP_SDHCI
static inline void s3c2416_default_sdhci0(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC
	s3c_hsmmc0_def_platdata.cfg_gpio = s3c2416_setup_sdhci0_cfg_gpio;
#endif 
}

static inline void s3c2416_default_sdhci1(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC1
	s3c_hsmmc1_def_platdata.cfg_gpio = s3c2416_setup_sdhci1_cfg_gpio;
#endif 
}

#else
static inline void s3c2416_default_sdhci0(void) { }
static inline void s3c2416_default_sdhci1(void) { }

#endif 


#ifdef CONFIG_S3C64XX_SETUP_SDHCI
static inline void s3c6400_default_sdhci0(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC
	s3c_hsmmc0_def_platdata.cfg_gpio = s3c64xx_setup_sdhci0_cfg_gpio;
#endif
}

static inline void s3c6400_default_sdhci1(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC1
	s3c_hsmmc1_def_platdata.cfg_gpio = s3c64xx_setup_sdhci1_cfg_gpio;
#endif
}

static inline void s3c6400_default_sdhci2(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC2
	s3c_hsmmc2_def_platdata.cfg_gpio = s3c64xx_setup_sdhci2_cfg_gpio;
#endif
}

static inline void s3c6410_default_sdhci0(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC
	s3c_hsmmc0_def_platdata.cfg_gpio = s3c64xx_setup_sdhci0_cfg_gpio;
#endif
}

static inline void s3c6410_default_sdhci1(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC1
	s3c_hsmmc1_def_platdata.cfg_gpio = s3c64xx_setup_sdhci1_cfg_gpio;
#endif
}

static inline void s3c6410_default_sdhci2(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC2
	s3c_hsmmc2_def_platdata.cfg_gpio = s3c64xx_setup_sdhci2_cfg_gpio;
#endif
}

#else
static inline void s3c6410_default_sdhci0(void) { }
static inline void s3c6410_default_sdhci1(void) { }
static inline void s3c6410_default_sdhci2(void) { }
static inline void s3c6400_default_sdhci0(void) { }
static inline void s3c6400_default_sdhci1(void) { }
static inline void s3c6400_default_sdhci2(void) { }

#endif 


#ifdef CONFIG_S5P64X0_SETUP_SDHCI
static inline void s5p64x0_default_sdhci0(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC
	s3c_hsmmc0_def_platdata.cfg_gpio = s5p64x0_setup_sdhci0_cfg_gpio;
#endif
}

static inline void s5p64x0_default_sdhci1(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC1
	s3c_hsmmc1_def_platdata.cfg_gpio = s5p64x0_setup_sdhci1_cfg_gpio;
#endif
}

static inline void s5p6440_default_sdhci2(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC2
	s3c_hsmmc2_def_platdata.cfg_gpio = s5p6440_setup_sdhci2_cfg_gpio;
#endif
}

static inline void s5p6450_default_sdhci2(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC2
	s3c_hsmmc2_def_platdata.cfg_gpio = s5p6450_setup_sdhci2_cfg_gpio;
#endif
}

#else
static inline void s5p64x0_default_sdhci0(void) { }
static inline void s5p64x0_default_sdhci1(void) { }
static inline void s5p6440_default_sdhci2(void) { }
static inline void s5p6450_default_sdhci2(void) { }

#endif 


#ifdef CONFIG_S5PC100_SETUP_SDHCI
static inline void s5pc100_default_sdhci0(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC
	s3c_hsmmc0_def_platdata.cfg_gpio = s5pc100_setup_sdhci0_cfg_gpio;
#endif
}

static inline void s5pc100_default_sdhci1(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC1
	s3c_hsmmc1_def_platdata.cfg_gpio = s5pc100_setup_sdhci1_cfg_gpio;
#endif
}

static inline void s5pc100_default_sdhci2(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC2
	s3c_hsmmc2_def_platdata.cfg_gpio = s5pc100_setup_sdhci2_cfg_gpio;
#endif
}

#else
static inline void s5pc100_default_sdhci0(void) { }
static inline void s5pc100_default_sdhci1(void) { }
static inline void s5pc100_default_sdhci2(void) { }

#endif 


#ifdef CONFIG_S5PV210_SETUP_SDHCI
static inline void s5pv210_default_sdhci0(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC
	s3c_hsmmc0_def_platdata.cfg_gpio = s5pv210_setup_sdhci0_cfg_gpio;
#endif
}

static inline void s5pv210_default_sdhci1(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC1
	s3c_hsmmc1_def_platdata.cfg_gpio = s5pv210_setup_sdhci1_cfg_gpio;
#endif
}

static inline void s5pv210_default_sdhci2(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC2
	s3c_hsmmc2_def_platdata.cfg_gpio = s5pv210_setup_sdhci2_cfg_gpio;
#endif
}

static inline void s5pv210_default_sdhci3(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC3
	s3c_hsmmc3_def_platdata.cfg_gpio = s5pv210_setup_sdhci3_cfg_gpio;
#endif
}

#else
static inline void s5pv210_default_sdhci0(void) { }
static inline void s5pv210_default_sdhci1(void) { }
static inline void s5pv210_default_sdhci2(void) { }
static inline void s5pv210_default_sdhci3(void) { }

#endif 

#ifdef CONFIG_EXYNOS4_SETUP_SDHCI
static inline void exynos4_default_sdhci0(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC
	s3c_hsmmc0_def_platdata.cfg_gpio = exynos4_setup_sdhci0_cfg_gpio;
#endif
}

static inline void exynos4_default_sdhci1(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC1
	s3c_hsmmc1_def_platdata.cfg_gpio = exynos4_setup_sdhci1_cfg_gpio;
#endif
}

static inline void exynos4_default_sdhci2(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC2
	s3c_hsmmc2_def_platdata.cfg_gpio = exynos4_setup_sdhci2_cfg_gpio;
#endif
}

static inline void exynos4_default_sdhci3(void)
{
#ifdef CONFIG_S3C_DEV_HSMMC3
	s3c_hsmmc3_def_platdata.cfg_gpio = exynos4_setup_sdhci3_cfg_gpio;
#endif
}

#else
static inline void exynos4_default_sdhci0(void) { }
static inline void exynos4_default_sdhci1(void) { }
static inline void exynos4_default_sdhci2(void) { }
static inline void exynos4_default_sdhci3(void) { }

#endif 

static inline void s3c_sdhci_setname(int id, char *name)
{
	switch (id) {
#ifdef CONFIG_S3C_DEV_HSMMC
	case 0:
		s3c_device_hsmmc0.name = name;
		break;
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	case 1:
		s3c_device_hsmmc1.name = name;
		break;
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	case 2:
		s3c_device_hsmmc2.name = name;
		break;
#endif
#ifdef CONFIG_S3C_DEV_HSMMC3
	case 3:
		s3c_device_hsmmc3.name = name;
		break;
#endif
	}
}

#endif 
