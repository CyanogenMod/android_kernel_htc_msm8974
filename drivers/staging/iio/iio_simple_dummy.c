/**
 * Copyright (c) 2011 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * A reference industrial I/O driver to illustrate the functionality available.
 *
 * There are numerous real drivers to illustrate the finer points.
 * The purpose of this driver is to provide a driver with far more comments
 * and explanatory notes than any 'real' driver would have.
 * Anyone starting out writing an IIO driver should first make sure they
 * understand all of this driver except those bits specifically marked
 * as being present to allow us to 'fake' the presence of hardware.
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include "iio.h"
#include "sysfs.h"
#include "events.h"
#include "buffer.h"
#include "iio_simple_dummy.h"

static unsigned instances = 1;
module_param(instances, int, 0);

static struct iio_dev **iio_dummy_devs;

static const char *iio_dummy_part_number = "iio_dummy_part_no";

struct iio_dummy_accel_calibscale {
	int val;
	int val2;
	int regval; /* what would be written to hardware */
};

static const struct iio_dummy_accel_calibscale dummy_scales[] = {
	{ 0, 100, 0x8 }, 
	{ 0, 133, 0x7 }, 
	{ 733, 13, 0x9 }, 
};

static struct iio_chan_spec iio_dummy_channels[] = {
	
	{
		.type = IIO_VOLTAGE,
		
		.indexed = 1,
		.channel = 0,
		
		.info_mask =
		IIO_CHAN_INFO_OFFSET_SEPARATE_BIT |
		IIO_CHAN_INFO_SCALE_SEPARATE_BIT,
		
		.scan_index = voltage0,
		.scan_type = { 
			.sign = 'u', 
			.realbits = 13, 
			.storagebits = 16, 
			.shift = 0, 
		},
#ifdef CONFIG_IIO_SIMPLE_DUMMY_EVENTS
		.event_mask = IIO_EV_BIT(IIO_EV_TYPE_THRESH,
					 IIO_EV_DIR_RISING),
#endif 
	},
	
	{
		.type = IIO_VOLTAGE,
		.differential = 1,
		.indexed = 1,
		.channel = 1,
		.channel2 = 2,
		.info_mask =
		IIO_CHAN_INFO_SCALE_SHARED_BIT,
		.scan_index = diffvoltage1m2,
		.scan_type = { 
			.sign = 's', 
			.realbits = 12, 
			.storagebits = 16, 
			.shift = 0, 
		},
	},
	
	{
		.type = IIO_VOLTAGE,
		.differential = 1,
		.indexed = 1,
		.channel = 3,
		.channel2 = 4,
		.info_mask =
		IIO_CHAN_INFO_SCALE_SHARED_BIT,
		.scan_index = diffvoltage3m4,
		.scan_type = {
			.sign = 's',
			.realbits = 11,
			.storagebits = 16,
			.shift = 0,
		},
	},
	{
		.type = IIO_ACCEL,
		.modified = 1,
		
		.channel2 = IIO_MOD_X,
		.info_mask =
		IIO_CHAN_INFO_CALIBBIAS_SEPARATE_BIT,
		.scan_index = accelx,
		.scan_type = { 
			.sign = 's', 
			.realbits = 16, 
			.storagebits = 16, 
			.shift = 0, 
		},
	},
	IIO_CHAN_SOFT_TIMESTAMP(4),
	
	{
		.type = IIO_VOLTAGE,
		.output = 1,
		.indexed = 1,
		.channel = 0,
	},
};

static int iio_dummy_read_raw(struct iio_dev *indio_dev,
			      struct iio_chan_spec const *chan,
			      int *val,
			      int *val2,
			      long mask)
{
	struct iio_dummy_state *st = iio_priv(indio_dev);
	int ret = -EINVAL;

	mutex_lock(&st->lock);
	switch (mask) {
	case 0: 
		switch (chan->type) {
		case IIO_VOLTAGE:
			if (chan->output) {
				
				*val = st->dac_val;
				ret = IIO_VAL_INT;
			} else if (chan->differential) {
				if (chan->channel == 1)
					*val = st->differential_adc_val[0];
				else
					*val = st->differential_adc_val[1];
				ret = IIO_VAL_INT;
			} else {
				*val = st->single_ended_adc_val;
				ret = IIO_VAL_INT;
			}
			break;
		case IIO_ACCEL:
			*val = st->accel_val;
			ret = IIO_VAL_INT;
			break;
		default:
			break;
		}
		break;
	case IIO_CHAN_INFO_OFFSET:
		
		*val = 7;
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SCALE:
		switch (chan->differential) {
		case 0:
			
			*val = 0;
			*val2 = 1333;
			ret = IIO_VAL_INT_PLUS_MICRO;
			break;
		case 1:
			
			*val = 0;
			*val2 = 1344;
			ret = IIO_VAL_INT_PLUS_NANO;
		}
		break;
	case IIO_CHAN_INFO_CALIBBIAS:
		
		*val = st->accel_calibbias;
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_CALIBSCALE:
		*val = st->accel_calibscale->val;
		*val2 = st->accel_calibscale->val2;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;
	default:
		break;
	}
	mutex_unlock(&st->lock);
	return ret;
}

static int iio_dummy_write_raw(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int val,
			       int val2,
			       long mask)
{
	int i;
	int ret = 0;
	struct iio_dummy_state *st = iio_priv(indio_dev);

	switch (mask) {
	case 0:
		if (chan->output == 0)
			return -EINVAL;

		
		mutex_lock(&st->lock);
		st->dac_val = val;
		mutex_unlock(&st->lock);
		return 0;
	case IIO_CHAN_INFO_CALIBBIAS:
		mutex_lock(&st->lock);
		
		for (i = 0; i < ARRAY_SIZE(dummy_scales); i++)
			if (val == dummy_scales[i].val &&
			    val2 == dummy_scales[i].val2)
				break;
		if (i == ARRAY_SIZE(dummy_scales))
			ret = -EINVAL;
		else
			st->accel_calibscale = &dummy_scales[i];
		mutex_unlock(&st->lock);
		return ret;
	default:
		return -EINVAL;
	}
}

static const struct iio_info iio_dummy_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &iio_dummy_read_raw,
	.write_raw = &iio_dummy_write_raw,
#ifdef CONFIG_IIO_SIMPLE_DUMMY_EVENTS
	.read_event_config = &iio_simple_dummy_read_event_config,
	.write_event_config = &iio_simple_dummy_write_event_config,
	.read_event_value = &iio_simple_dummy_read_event_value,
	.write_event_value = &iio_simple_dummy_write_event_value,
#endif 
};

static int iio_dummy_init_device(struct iio_dev *indio_dev)
{
	struct iio_dummy_state *st = iio_priv(indio_dev);

	st->dac_val = 0;
	st->single_ended_adc_val = 73;
	st->differential_adc_val[0] = 33;
	st->differential_adc_val[1] = -34;
	st->accel_val = 34;
	st->accel_calibbias = -7;
	st->accel_calibscale = &dummy_scales[0];

	return 0;
}

static int __devinit iio_dummy_probe(int index)
{
	int ret;
	struct iio_dev *indio_dev;
	struct iio_dummy_state *st;

	indio_dev = iio_allocate_device(sizeof(*st));
	if (indio_dev == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}

	st = iio_priv(indio_dev);
	mutex_init(&st->lock);

	iio_dummy_init_device(indio_dev);

	iio_dummy_devs[index] = indio_dev;


	indio_dev->name = iio_dummy_part_number;

	
	indio_dev->channels = iio_dummy_channels;
	indio_dev->num_channels = ARRAY_SIZE(iio_dummy_channels);

	indio_dev->info = &iio_dummy_info;

	
	indio_dev->modes = INDIO_DIRECT_MODE;

	ret = iio_simple_dummy_events_register(indio_dev);
	if (ret < 0)
		goto error_free_device;

	
	ret = iio_simple_dummy_configure_buffer(indio_dev);
	if (ret < 0)
		goto error_unregister_events;

	ret = iio_buffer_register(indio_dev, iio_dummy_channels, 5);
	if (ret < 0)
		goto error_unconfigure_buffer;

	ret = iio_device_register(indio_dev);
	if (ret < 0)
		goto error_unregister_buffer;

	return 0;
error_unregister_buffer:
	iio_buffer_unregister(indio_dev);
error_unconfigure_buffer:
	iio_simple_dummy_unconfigure_buffer(indio_dev);
error_unregister_events:
	iio_simple_dummy_events_unregister(indio_dev);
error_free_device:
	iio_free_device(indio_dev);
error_ret:
	return ret;
}

static int iio_dummy_remove(int index)
{
	int ret;
	struct iio_dev *indio_dev = iio_dummy_devs[index];


	
	iio_device_unregister(indio_dev);

	

	
	iio_buffer_unregister(indio_dev);
	iio_simple_dummy_unconfigure_buffer(indio_dev);

	ret = iio_simple_dummy_events_unregister(indio_dev);
	if (ret)
		goto error_ret;

	
	iio_free_device(indio_dev);

error_ret:
	return ret;
}

static __init int iio_dummy_init(void)
{
	int i, ret;
	if (instances > 10) {
		instances = 1;
		return -EINVAL;
	}
	
	iio_dummy_devs = kcalloc(instances, sizeof(*iio_dummy_devs),
				 GFP_KERNEL);
	
	for (i = 0; i < instances; i++) {
		ret = iio_dummy_probe(i);
		if (ret < 0)
			return ret;
	}
	return 0;
}
module_init(iio_dummy_init);

static __exit void iio_dummy_exit(void)
{
	int i;
	for (i = 0; i < instances; i++)
		iio_dummy_remove(i);
	kfree(iio_dummy_devs);
}
module_exit(iio_dummy_exit);

MODULE_AUTHOR("Jonathan Cameron <jic23@cam.ac.uk>");
MODULE_DESCRIPTION("IIO dummy driver");
MODULE_LICENSE("GPL v2");
