/*
 * AD5446 SPI DAC driver
 *
 * Copyright 2010 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */
#ifndef IIO_DAC_AD5446_H_
#define IIO_DAC_AD5446_H_


#define AD5446_LOAD		(0x0 << 14) 
#define AD5446_SDO_DIS		(0x1 << 14) 
#define AD5446_NOP		(0x2 << 14) 
#define AD5446_CLK_RISING	(0x3 << 14) 

#define AD5620_LOAD		(0x0 << 14) 
#define AD5620_PWRDWN_1k	(0x1 << 14) 
#define AD5620_PWRDWN_100k	(0x2 << 14) 
#define AD5620_PWRDWN_TRISTATE	(0x3 << 14) 

#define AD5660_LOAD		(0x0 << 16) 
#define AD5660_PWRDWN_1k	(0x1 << 16) 
#define AD5660_PWRDWN_100k	(0x2 << 16) 
#define AD5660_PWRDWN_TRISTATE	(0x3 << 16) 

#define MODE_PWRDWN_1k		0x1
#define MODE_PWRDWN_100k	0x2
#define MODE_PWRDWN_TRISTATE	0x3


struct ad5446_state {
	struct spi_device		*spi;
	const struct ad5446_chip_info	*chip_info;
	struct regulator		*reg;
	struct work_struct		poll_work;
	unsigned short			vref_mv;
	unsigned			cached_val;
	unsigned			pwr_down_mode;
	unsigned			pwr_down;
	struct spi_transfer		xfer;
	struct spi_message		msg;
	union {
		unsigned short		d16;
		unsigned char		d24[3];
	} data;
};


struct ad5446_chip_info {
	struct iio_chan_spec	channel;
	u16			int_vref_mv;
	void (*store_sample)	(struct ad5446_state *st, unsigned val);
	void (*store_pwr_down)	(struct ad5446_state *st, unsigned mode);
};


enum ad5446_supported_device_ids {
	ID_AD5444,
	ID_AD5446,
	ID_AD5541A,
	ID_AD5542A,
	ID_AD5543,
	ID_AD5512A,
	ID_AD5553,
	ID_AD5601,
	ID_AD5611,
	ID_AD5621,
	ID_AD5620_2500,
	ID_AD5620_1250,
	ID_AD5640_2500,
	ID_AD5640_1250,
	ID_AD5660_2500,
	ID_AD5660_1250,
};

#endif 
