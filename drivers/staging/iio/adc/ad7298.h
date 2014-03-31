/*
 * AD7298 SPI ADC driver
 *
 * Copyright 2011 Analog Devices Inc.
 *
 * Licensed under the GPL-2.
 */

#ifndef IIO_ADC_AD7298_H_
#define IIO_ADC_AD7298_H_

#define AD7298_WRITE	(1 << 15) 
#define AD7298_REPEAT	(1 << 14) 
#define AD7298_CH(x)	(1 << (13 - (x))) 
#define AD7298_TSENSE	(1 << 5) 
#define AD7298_EXTREF	(1 << 2) 
#define AD7298_TAVG	(1 << 1) 
#define AD7298_PDD	(1 << 0) 

#define AD7298_MAX_CHAN		8
#define AD7298_BITS		12
#define AD7298_STORAGE_BITS	16
#define AD7298_INTREF_mV	2500

#define AD7298_CH_TEMP		9

#define RES_MASK(bits)	((1 << (bits)) - 1)


struct ad7298_platform_data {
	
	u16				vref_mv;
};

struct ad7298_state {
	struct spi_device		*spi;
	struct regulator		*reg;
	size_t				d_size;
	u16				int_vref_mv;
	unsigned			ext_ref;
	struct spi_transfer		ring_xfer[10];
	struct spi_transfer		scan_single_xfer[3];
	struct spi_message		ring_msg;
	struct spi_message		scan_single_msg;
	unsigned short			rx_buf[8] ____cacheline_aligned;
	unsigned short			tx_buf[2];
};

#ifdef CONFIG_IIO_BUFFER
int ad7298_register_ring_funcs_and_init(struct iio_dev *indio_dev);
void ad7298_ring_cleanup(struct iio_dev *indio_dev);
#else 

static inline int
ad7298_register_ring_funcs_and_init(struct iio_dev *indio_dev)
{
	return 0;
}

static inline void ad7298_ring_cleanup(struct iio_dev *indio_dev)
{
}
#endif 
#endif 
