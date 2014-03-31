/*
 * AD5504 SPI DAC driver
 *
 * Copyright 2011 Analog Devices Inc.
 *
 * Licensed under the GPL-2.
 */

#ifndef SPI_AD5504_H_
#define SPI_AD5504_H_

#define AD5505_BITS			12
#define AD5504_RES_MASK			((1 << (AD5505_BITS)) - 1)

#define AD5504_CMD_READ			(1 << 15)
#define AD5504_CMD_WRITE		(0 << 15)
#define AD5504_ADDR(addr)		((addr) << 12)

#define AD5504_ADDR_NOOP		0
#define AD5504_ADDR_DAC(x)		((x) + 1)
#define AD5504_ADDR_ALL_DAC		5
#define AD5504_ADDR_CTRL		7

#define AD5504_DAC_PWR(ch)		((ch) << 2)
#define AD5504_DAC_PWRDWN_MODE(mode)	((mode) << 6)
#define AD5504_DAC_PWRDN_20K		0
#define AD5504_DAC_PWRDN_3STATE		1


struct ad5504_platform_data {
	u16				vref_mv;
};


struct ad5504_state {
	struct spi_device		*spi;
	struct regulator		*reg;
	unsigned short			vref_mv;
	unsigned			pwr_down_mask;
	unsigned			pwr_down_mode;
};


enum ad5504_supported_device_ids {
	ID_AD5504,
	ID_AD5501,
};

#endif 
