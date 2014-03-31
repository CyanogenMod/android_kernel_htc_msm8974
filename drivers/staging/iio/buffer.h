/* The industrial I/O core - generic buffer interfaces.
 *
 * Copyright (c) 2008 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#ifndef _IIO_BUFFER_GENERIC_H_
#define _IIO_BUFFER_GENERIC_H_
#include <linux/sysfs.h>
#include "iio.h"

#ifdef CONFIG_IIO_BUFFER

struct iio_buffer;

struct iio_buffer_access_funcs {
	int (*store_to)(struct iio_buffer *buffer, u8 *data, s64 timestamp);
	int (*read_first_n)(struct iio_buffer *buffer,
			    size_t n,
			    char __user *buf);

	int (*request_update)(struct iio_buffer *buffer);

	int (*get_bytes_per_datum)(struct iio_buffer *buffer);
	int (*set_bytes_per_datum)(struct iio_buffer *buffer, size_t bpd);
	int (*get_length)(struct iio_buffer *buffer);
	int (*set_length)(struct iio_buffer *buffer, int length);
};

struct iio_buffer {
	int					length;
	int					bytes_per_datum;
	struct attribute_group			*scan_el_attrs;
	long					*scan_mask;
	bool					scan_timestamp;
	unsigned				scan_index_timestamp;
	const struct iio_buffer_access_funcs	*access;
	struct list_head			scan_el_dev_attr_list;
	struct attribute_group			scan_el_group;
	wait_queue_head_t			pollq;
	bool					stufftoread;
	const struct attribute_group *attrs;
	struct list_head			demux_list;
	unsigned char				*demux_bounce;
};

void iio_buffer_init(struct iio_buffer *buffer);

static inline void __iio_update_buffer(struct iio_buffer *buffer,
				       int bytes_per_datum, int length)
{
	buffer->bytes_per_datum = bytes_per_datum;
	buffer->length = length;
}

int iio_scan_mask_query(struct iio_dev *indio_dev,
			struct iio_buffer *buffer, int bit);

int iio_scan_mask_set(struct iio_dev *indio_dev,
		      struct iio_buffer *buffer, int bit);

int iio_push_to_buffer(struct iio_buffer *buffer, unsigned char *data,
		       s64 timestamp);

int iio_update_demux(struct iio_dev *indio_dev);

int iio_buffer_register(struct iio_dev *indio_dev,
			const struct iio_chan_spec *channels,
			int num_channels);

void iio_buffer_unregister(struct iio_dev *indio_dev);

ssize_t iio_buffer_read_length(struct device *dev,
			       struct device_attribute *attr,
			       char *buf);
ssize_t iio_buffer_write_length(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf,
			      size_t len);
ssize_t iio_buffer_store_enable(struct device *dev,
				struct device_attribute *attr,
				const char *buf,
				size_t len);
ssize_t iio_buffer_show_enable(struct device *dev,
			       struct device_attribute *attr,
			       char *buf);
#define IIO_BUFFER_LENGTH_ATTR DEVICE_ATTR(length, S_IRUGO | S_IWUSR,	\
					   iio_buffer_read_length,	\
					   iio_buffer_write_length)

#define IIO_BUFFER_ENABLE_ATTR DEVICE_ATTR(enable, S_IRUGO | S_IWUSR,	\
					   iio_buffer_show_enable,	\
					   iio_buffer_store_enable)

int iio_sw_buffer_preenable(struct iio_dev *indio_dev);

#else 

static inline int iio_buffer_register(struct iio_dev *indio_dev,
					   struct iio_chan_spec *channels,
					   int num_channels)
{
	return 0;
}

static inline void iio_buffer_unregister(struct iio_dev *indio_dev)
{};

#endif 

#endif 
