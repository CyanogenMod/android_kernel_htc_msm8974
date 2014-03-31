/*
 * arch/arm/include/asm/hardware/locomo.h
 *
 * This file contains the definitions for the LoCoMo G/A Chip
 *
 * (C) Copyright 2004 John Lenz
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on sa1111.h
 */
#ifndef _ASM_ARCH_LOCOMO
#define _ASM_ARCH_LOCOMO

#define locomo_writel(val,addr)	({ *(volatile u16 *)(addr) = (val); })
#define locomo_readl(addr)	(*(volatile u16 *)(addr))

#define LOCOMO_VER	0x00

#define LOCOMO_ST	0x04

#define LOCOMO_C32K	0x08

#define LOCOMO_ICR	0x0C

#define LOCOMO_MCSX0	0x10
#define LOCOMO_MCSX1	0x14
#define LOCOMO_MCSX2	0x18
#define LOCOMO_MCSX3	0x1c

#define LOCOMO_ASD	0x20		
#define LOCOMO_HSD	0x28		
#define LOCOMO_HSC	0x2c		
#define LOCOMO_TADC	0x30		


#define LOCOMO_LTC	0xd8		
#define LOCOMO_LTINT	0xdc		

#define LOCOMO_DAC		0xe0
#define	LOCOMO_DAC_SCLOEB	0x08	
#define	LOCOMO_DAC_TEST		0x04	
#define	LOCOMO_DAC_SDA		0x02	
#define	LOCOMO_DAC_SDAOEB	0x01	

#define LOCOMO_SPI	0x60
#define LOCOMO_SPIMD	0x00		
#define LOCOMO_SPICT	0x04		
#define LOCOMO_SPIST	0x08		
#define	LOCOMO_SPI_TEND	(1 << 3)	
#define	LOCOMO_SPI_REND	(1 << 2)	
#define	LOCOMO_SPI_RFW	(1 << 1)	
#define	LOCOMO_SPI_RFR	(1)		

#define LOCOMO_SPIIS	0x10		
#define LOCOMO_SPIWE	0x14		
#define LOCOMO_SPIIE	0x18		
#define LOCOMO_SPIIR	0x1c		
#define LOCOMO_SPITD	0x20		
#define LOCOMO_SPIRD	0x24		
#define LOCOMO_SPITS	0x28		
#define LOCOMO_SPIRS	0x2C		

#define LOCOMO_GPD		0x90	
#define LOCOMO_GPE		0x94	
#define LOCOMO_GPL		0x98	
#define LOCOMO_GPO		0x9c	
#define LOCOMO_GRIE		0xa0	
#define LOCOMO_GFIE		0xa4	
#define LOCOMO_GIS		0xa8	
#define LOCOMO_GWE		0xac	
#define LOCOMO_GIE		0xb0	
#define LOCOMO_GIR		0xb4	
#define	LOCOMO_GPIO(Nb)		(0x01 << (Nb))
#define LOCOMO_GPIO_RTS		LOCOMO_GPIO(0)
#define LOCOMO_GPIO_CTS		LOCOMO_GPIO(1)
#define LOCOMO_GPIO_DSR		LOCOMO_GPIO(2)
#define LOCOMO_GPIO_DTR		LOCOMO_GPIO(3)
#define LOCOMO_GPIO_LCD_VSHA_ON	LOCOMO_GPIO(4)
#define LOCOMO_GPIO_LCD_VSHD_ON	LOCOMO_GPIO(5)
#define LOCOMO_GPIO_LCD_VEE_ON	LOCOMO_GPIO(6)
#define LOCOMO_GPIO_LCD_MOD	LOCOMO_GPIO(7)
#define LOCOMO_GPIO_DAC_ON	LOCOMO_GPIO(8)
#define LOCOMO_GPIO_FL_VR	LOCOMO_GPIO(9)
#define LOCOMO_GPIO_DAC_SDATA	LOCOMO_GPIO(10)
#define LOCOMO_GPIO_DAC_SCK	LOCOMO_GPIO(11)
#define LOCOMO_GPIO_DAC_SLOAD	LOCOMO_GPIO(12)
#define LOCOMO_GPIO_CARD_DETECT LOCOMO_GPIO(13)
#define LOCOMO_GPIO_WRITE_PROT  LOCOMO_GPIO(14)
#define LOCOMO_GPIO_CARD_POWER  LOCOMO_GPIO(15)


#define LOCOMO_KEYBOARD		0x40
#define LOCOMO_KIB		0x00	
#define LOCOMO_KSC		0x04	
#define LOCOMO_KCMD		0x08	
#define LOCOMO_KIC		0x0c	

#define LOCOMO_FRONTLIGHT	0xc8
#define LOCOMO_ALS		0x00	
#define LOCOMO_ALD		0x04	

#define LOCOMO_ALC_EN		0x8000

#define LOCOMO_BACKLIGHT	0x38
#define LOCOMO_TC		0x00		
#define LOCOMO_CPSD		0x04		

#define LOCOMO_AUDIO		0x54
#define LOCOMO_ACC		0x00	
#define LOCOMO_PAIF		0xD0	
#define	LOCOMO_ACC_XON		0x80
#define	LOCOMO_ACC_XEN		0x40
#define	LOCOMO_ACC_XSEL0	0x00
#define	LOCOMO_ACC_XSEL1	0x20
#define	LOCOMO_ACC_MCLKEN	0x10
#define	LOCOMO_ACC_64FSEN	0x08
#define	LOCOMO_ACC_CLKSEL000	0x00	
#define	LOCOMO_ACC_CLKSEL001	0x01	
#define	LOCOMO_ACC_CLKSEL010	0x02	
#define	LOCOMO_ACC_CLKSEL011	0x03	
#define	LOCOMO_ACC_CLKSEL100	0x04	
#define	LOCOMO_ACC_CLKSEL101	0x05	
#define	LOCOMO_PAIF_SCINV	0x20
#define	LOCOMO_PAIF_SCEN	0x10
#define	LOCOMO_PAIF_LRCRST	0x08
#define	LOCOMO_PAIF_LRCEVE	0x04
#define	LOCOMO_PAIF_LRCINV	0x02
#define	LOCOMO_PAIF_LRCEN	0x01

#define LOCOMO_LED		0xe8
#define LOCOMO_LPT0		0x00
#define LOCOMO_LPT1		0x04
#define LOCOMO_LPT_TOFH		0x80
#define LOCOMO_LPT_TOFL		0x08
#define LOCOMO_LPT_TOH(TOH)	((TOH & 0x7) << 4)
#define LOCOMO_LPT_TOL(TOL)	((TOL & 0x7))

extern struct bus_type locomo_bus_type;

#define LOCOMO_DEVID_KEYBOARD	0
#define LOCOMO_DEVID_FRONTLIGHT	1
#define LOCOMO_DEVID_BACKLIGHT	2
#define LOCOMO_DEVID_AUDIO	3
#define LOCOMO_DEVID_LED	4
#define LOCOMO_DEVID_UART	5
#define LOCOMO_DEVID_SPI	6

struct locomo_dev {
	struct device	dev;
	unsigned int	devid;
	unsigned int	irq[1];

	void		*mapbase;
	unsigned long	length;

	u64		dma_mask;
};

#define LOCOMO_DEV(_d)	container_of((_d), struct locomo_dev, dev)

#define locomo_get_drvdata(d)	dev_get_drvdata(&(d)->dev)
#define locomo_set_drvdata(d,p)	dev_set_drvdata(&(d)->dev, p)

struct locomo_driver {
	struct device_driver	drv;
	unsigned int		devid;
	int (*probe)(struct locomo_dev *);
	int (*remove)(struct locomo_dev *);
	int (*suspend)(struct locomo_dev *, pm_message_t);
	int (*resume)(struct locomo_dev *);
};

#define LOCOMO_DRV(_d)	container_of((_d), struct locomo_driver, drv)

#define LOCOMO_DRIVER_NAME(_ldev) ((_ldev)->dev.driver->name)

void locomo_lcd_power(struct locomo_dev *, int, unsigned int);

int locomo_driver_register(struct locomo_driver *);
void locomo_driver_unregister(struct locomo_driver *);

void locomo_gpio_set_dir(struct device *dev, unsigned int bits, unsigned int dir);
int locomo_gpio_read_level(struct device *dev, unsigned int bits);
int locomo_gpio_read_output(struct device *dev, unsigned int bits);
void locomo_gpio_write(struct device *dev, unsigned int bits, unsigned int set);

void locomo_m62332_senddata(struct locomo_dev *ldev, unsigned int dac_data, int channel);

void locomo_frontlight_set(struct locomo_dev *dev, int duty, int vr, int bpwf);

struct locomo_platform_data {
	int	irq_base;	
};

#endif
