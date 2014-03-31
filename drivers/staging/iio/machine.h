/*
 * Industrial I/O in kernel access map definitions for board files.
 *
 * Copyright (c) 2011 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

struct iio_map {
	const char *adc_channel_label;
	const char *consumer_dev_name;
	const char *consumer_channel;
};
