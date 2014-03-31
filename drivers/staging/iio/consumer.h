/*
 * Industrial I/O in kernel consumer interface
 *
 * Copyright (c) 2011 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */
#ifndef _IIO_INKERN_CONSUMER_H_
#define _IIO_INKERN_CONSUMER_H
#include "types.h"

struct iio_dev;
struct iio_chan_spec;

struct iio_channel {
	struct iio_dev *indio_dev;
	const struct iio_chan_spec *channel;
};

struct iio_channel *iio_st_channel_get(const char *name,
				       const char *consumer_channel);

void iio_st_channel_release(struct iio_channel *chan);

struct iio_channel *iio_st_channel_get_all(const char *name);

void iio_st_channel_release_all(struct iio_channel *chan);

int iio_st_read_channel_raw(struct iio_channel *chan,
			    int *val);

int iio_st_get_channel_type(struct iio_channel *channel,
			    enum iio_chan_type *type);

int iio_st_read_channel_scale(struct iio_channel *chan, int *val,
			      int *val2);

#endif
