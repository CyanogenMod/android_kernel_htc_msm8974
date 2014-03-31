/**
 * Copyright (c) 2011 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * Join together the various functionality of iio_simple_dummy driver
 */

#include <linux/kernel.h>

struct iio_dummy_accel_calibscale;

struct iio_dummy_state {
	int dac_val;
	int single_ended_adc_val;
	int differential_adc_val[2];
	int accel_val;
	int accel_calibbias;
	const struct iio_dummy_accel_calibscale *accel_calibscale;
	struct mutex lock;
#ifdef CONFIG_IIO_SIMPLE_DUMMY_EVENTS
	int event_irq;
	int event_val;
	bool event_en;
#endif 
};

#ifdef CONFIG_IIO_SIMPLE_DUMMY_EVENTS

struct iio_dev;

int iio_simple_dummy_read_event_config(struct iio_dev *indio_dev,
				       u64 event_code);

int iio_simple_dummy_write_event_config(struct iio_dev *indio_dev,
					u64 event_code,
					int state);

int iio_simple_dummy_read_event_value(struct iio_dev *indio_dev,
				      u64 event_code,
				      int *val);

int iio_simple_dummy_write_event_value(struct iio_dev *indio_dev,
				       u64 event_code,
				       int val);

int iio_simple_dummy_events_register(struct iio_dev *indio_dev);
int iio_simple_dummy_events_unregister(struct iio_dev *indio_dev);

#else 

static inline int
iio_simple_dummy_events_register(struct iio_dev *indio_dev)
{
	return 0;
};

static inline int
iio_simple_dummy_events_unregister(struct iio_dev *indio_dev)
{
	return 0;
};

#endif 

enum iio_simple_dummy_scan_elements {
	voltage0,
	diffvoltage1m2,
	diffvoltage3m4,
	accelx,
};

#ifdef CONFIG_IIO_SIMPLE_DUMMY_BUFFER
int iio_simple_dummy_configure_buffer(struct iio_dev *indio_dev);
void iio_simple_dummy_unconfigure_buffer(struct iio_dev *indio_dev);
#else
static inline int iio_simple_dummy_configure_buffer(struct iio_dev *indio_dev)
{
	return 0;
};
static inline
void iio_simple_dummy_unconfigure_buffer(struct iio_dev *indio_dev)
{};
#endif 
