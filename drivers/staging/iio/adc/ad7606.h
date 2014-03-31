/*
 * AD7606 ADC driver
 *
 * Copyright 2011 Analog Devices Inc.
 *
 * Licensed under the GPL-2.
 */

#ifndef IIO_ADC_AD7606_H_
#define IIO_ADC_AD7606_H_



struct ad7606_platform_data {
	unsigned			default_os;
	unsigned			default_range;
	unsigned			gpio_convst;
	unsigned			gpio_reset;
	unsigned			gpio_range;
	unsigned			gpio_os0;
	unsigned			gpio_os1;
	unsigned			gpio_os2;
	unsigned			gpio_frstdata;
	unsigned			gpio_stby;
};


struct ad7606_chip_info {
	const char			*name;
	u16				int_vref_mv;
	struct iio_chan_spec		*channels;
	unsigned			num_channels;
};


struct ad7606_state {
	struct device			*dev;
	const struct ad7606_chip_info	*chip_info;
	struct ad7606_platform_data	*pdata;
	struct regulator		*reg;
	struct work_struct		poll_work;
	wait_queue_head_t		wq_data_avail;
	const struct ad7606_bus_ops	*bops;
	unsigned			range;
	unsigned			oversampling;
	bool				done;
	void __iomem			*base_address;


	unsigned short			data[8] ____cacheline_aligned;
};

struct ad7606_bus_ops {
	
	int (*read_block)(struct device *, int, void *);
};

void ad7606_suspend(struct iio_dev *indio_dev);
void ad7606_resume(struct iio_dev *indio_dev);
struct iio_dev *ad7606_probe(struct device *dev, int irq,
			      void __iomem *base_address, unsigned id,
			      const struct ad7606_bus_ops *bops);
int ad7606_remove(struct iio_dev *indio_dev, int irq);
int ad7606_reset(struct ad7606_state *st);

enum ad7606_supported_device_ids {
	ID_AD7606_8,
	ID_AD7606_6,
	ID_AD7606_4
};

int ad7606_register_ring_funcs_and_init(struct iio_dev *indio_dev);
void ad7606_ring_cleanup(struct iio_dev *indio_dev);
#endif 
