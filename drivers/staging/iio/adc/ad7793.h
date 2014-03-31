/*
 * AD7792/AD7793 SPI ADC driver
 *
 * Copyright 2011 Analog Devices Inc.
 *
 * Licensed under the GPL-2.
 */
#ifndef IIO_ADC_AD7793_H_
#define IIO_ADC_AD7793_H_


#define AD7793_REG_COMM		0 
#define AD7793_REG_STAT		0 
#define AD7793_REG_MODE		1 
#define AD7793_REG_CONF		2 
#define AD7793_REG_DATA		3 
#define AD7793_REG_ID		4 
#define AD7793_REG_IO		5 
#define AD7793_REG_OFFSET	6 
#define AD7793_REG_FULLSALE	7 

#define AD7793_COMM_WEN		(1 << 7) 
#define AD7793_COMM_WRITE	(0 << 6) 
#define AD7793_COMM_READ	(1 << 6) 
#define AD7793_COMM_ADDR(x)	(((x) & 0x7) << 3) 
#define AD7793_COMM_CREAD	(1 << 2) 

#define AD7793_STAT_RDY		(1 << 7) 
#define AD7793_STAT_ERR		(1 << 6) 
#define AD7793_STAT_CH3		(1 << 2) 
#define AD7793_STAT_CH2		(1 << 1) 
#define AD7793_STAT_CH1		(1 << 0) 

#define AD7793_MODE_SEL(x)	(((x) & 0x7) << 13) 
#define AD7793_MODE_CLKSRC(x)	(((x) & 0x3) << 6) 
#define AD7793_MODE_RATE(x)	((x) & 0xF) 

#define AD7793_MODE_CONT		0 
#define AD7793_MODE_SINGLE		1 
#define AD7793_MODE_IDLE		2 
#define AD7793_MODE_PWRDN		3 
#define AD7793_MODE_CAL_INT_ZERO	4 
#define AD7793_MODE_CAL_INT_FULL	5 
#define AD7793_MODE_CAL_SYS_ZERO	6 
#define AD7793_MODE_CAL_SYS_FULL	7 

#define AD7793_CLK_INT		0 
#define AD7793_CLK_INT_CO	1 
#define AD7793_CLK_EXT		2 
#define AD7793_CLK_EXT_DIV2	3 

#define AD7793_CONF_VBIAS(x)	(((x) & 0x3) << 14) 
#define AD7793_CONF_BO_EN	(1 << 13) 
#define AD7793_CONF_UNIPOLAR	(1 << 12) 
#define AD7793_CONF_BOOST	(1 << 11) 
#define AD7793_CONF_GAIN(x)	(((x) & 0x7) << 8) 
#define AD7793_CONF_REFSEL	(1 << 7) 
#define AD7793_CONF_BUF		(1 << 4) 
#define AD7793_CONF_CHAN(x)	((x) & 0x7) 

#define AD7793_CH_AIN1P_AIN1M	0 
#define AD7793_CH_AIN2P_AIN2M	1 
#define AD7793_CH_AIN3P_AIN3M	2 
#define AD7793_CH_AIN1M_AIN1M	3 
#define AD7793_CH_TEMP		6 
#define AD7793_CH_AVDD_MONITOR	7 

#define AD7792_ID		0xA
#define AD7793_ID		0xB
#define AD7793_ID_MASK		0xF

#define AD7793_IO_IEXC1_IOUT1_IEXC2_IOUT2	0 
#define AD7793_IO_IEXC1_IOUT2_IEXC2_IOUT1	1 
#define AD7793_IO_IEXC1_IEXC2_IOUT1		2 
#define AD7793_IO_IEXC1_IEXC2_IOUT2		3 

#define AD7793_IO_IXCEN_10uA	(1 << 0) 
#define AD7793_IO_IXCEN_210uA	(2 << 0) 
#define AD7793_IO_IXCEN_1mA	(3 << 0) 

struct ad7793_platform_data {
	u16			vref_mv;
	u16			mode;
	u16			conf;
	u8			io;
};

#endif 
