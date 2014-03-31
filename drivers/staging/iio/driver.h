/*
 * Industrial I/O in kernel access map interface.
 *
 * Copyright (c) 2011 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#ifndef _IIO_INKERN_H_
#define _IIO_INKERN_H_

struct iio_map;

int iio_map_array_register(struct iio_dev *indio_dev,
			   struct iio_map *map);

int iio_map_array_unregister(struct iio_dev *indio_dev,
			     struct iio_map *map);

#endif
