/* The industrial I/O core
 *
 *Copyright (c) 2008 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * General attributes
 */

#ifndef _INDUSTRIAL_IO_SYSFS_H_
#define _INDUSTRIAL_IO_SYSFS_H_

struct iio_chan_spec;

struct iio_dev_attr {
	struct device_attribute dev_attr;
	u64 address;
	struct list_head l;
	struct iio_chan_spec const *c;
};

#define to_iio_dev_attr(_dev_attr)				\
	container_of(_dev_attr, struct iio_dev_attr, dev_attr)

ssize_t iio_read_const_attr(struct device *dev,
			    struct device_attribute *attr,
			    char *len);

struct iio_const_attr {
	const char *string;
	struct device_attribute dev_attr;
};

#define to_iio_const_attr(_dev_attr) \
	container_of(_dev_attr, struct iio_const_attr, dev_attr)

#define IIO_ATTR(_name, _mode, _show, _store, _addr)		\
	{ .dev_attr = __ATTR(_name, _mode, _show, _store),	\
	  .address = _addr }

#define IIO_DEVICE_ATTR(_name, _mode, _show, _store, _addr)	\
	struct iio_dev_attr iio_dev_attr_##_name		\
	= IIO_ATTR(_name, _mode, _show, _store, _addr)

#define IIO_DEVICE_ATTR_NAMED(_vname, _name, _mode, _show, _store, _addr) \
	struct iio_dev_attr iio_dev_attr_##_vname			\
	= IIO_ATTR(_name, _mode, _show, _store, _addr)

#define IIO_CONST_ATTR(_name, _string)					\
	struct iio_const_attr iio_const_attr_##_name			\
	= { .string = _string,						\
	    .dev_attr = __ATTR(_name, S_IRUGO, iio_read_const_attr, NULL)}

#define IIO_CONST_ATTR_NAMED(_vname, _name, _string)			\
	struct iio_const_attr iio_const_attr_##_vname			\
	= { .string = _string,						\
	    .dev_attr = __ATTR(_name, S_IRUGO, iio_read_const_attr, NULL)}

#define IIO_DEV_ATTR_RESET(_store)			\
	IIO_DEVICE_ATTR(reset, S_IWUSR, NULL, _store, 0)

#define IIO_DEV_ATTR_SAMP_FREQ(_mode, _show, _store)			\
	IIO_DEVICE_ATTR(sampling_frequency, _mode, _show, _store, 0)

#define IIO_DEV_ATTR_SAMP_FREQ_AVAIL(_show)				\
	IIO_DEVICE_ATTR(sampling_frequency_available, S_IRUGO, _show, NULL, 0)
#define IIO_CONST_ATTR_SAMP_FREQ_AVAIL(_string)			\
	IIO_CONST_ATTR(sampling_frequency_available, _string)

#define IIO_DEV_ATTR_TEMP_RAW(_show)			\
	IIO_DEVICE_ATTR(in_temp_raw, S_IRUGO, _show, NULL, 0)

#define IIO_CONST_ATTR_TEMP_OFFSET(_string)		\
	IIO_CONST_ATTR(in_temp_offset, _string)

#define IIO_CONST_ATTR_TEMP_SCALE(_string)		\
	IIO_CONST_ATTR(in_temp_scale, _string)

#endif 
