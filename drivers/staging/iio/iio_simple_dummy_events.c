/**
 * Copyright (c) 2011 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * Event handling elements of industrial I/O reference driver.
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include "iio.h"
#include "sysfs.h"
#include "events.h"
#include "iio_simple_dummy.h"

#include "iio_dummy_evgen.h"

int iio_simple_dummy_read_event_config(struct iio_dev *indio_dev,
				       u64 event_code)
{
	struct iio_dummy_state *st = iio_priv(indio_dev);

	return st->event_en;
}

int iio_simple_dummy_write_event_config(struct iio_dev *indio_dev,
					u64 event_code,
					int state)
{
	struct iio_dummy_state *st = iio_priv(indio_dev);

	switch (IIO_EVENT_CODE_EXTRACT_CHAN_TYPE(event_code)) {
	case IIO_VOLTAGE:
		switch (IIO_EVENT_CODE_EXTRACT_TYPE(event_code)) {
		case IIO_EV_TYPE_THRESH:
			if (IIO_EVENT_CODE_EXTRACT_DIR(event_code) ==
			    IIO_EV_DIR_RISING)
				st->event_en = state;
			else
				return -EINVAL;
			break;
		default:
			return -EINVAL;
		}
	default:
		return -EINVAL;
	}

	return 0;
}

int iio_simple_dummy_read_event_value(struct iio_dev *indio_dev,
				      u64 event_code,
				      int *val)
{
	struct iio_dummy_state *st = iio_priv(indio_dev);

	*val = st->event_val;

	return 0;
}

int iio_simple_dummy_write_event_value(struct iio_dev *indio_dev,
				       u64 event_code,
				       int val)
{
	struct iio_dummy_state *st = iio_priv(indio_dev);

	st->event_val = val;

	return 0;
}

static irqreturn_t iio_simple_dummy_event_handler(int irq, void *private)
{
	struct iio_dev *indio_dev = private;
	iio_push_event(indio_dev,
		       IIO_EVENT_CODE(IIO_VOLTAGE, 0, 0,
				      IIO_EV_DIR_RISING,
				      IIO_EV_TYPE_THRESH, 0, 0, 0),
		       iio_get_time_ns());
	return IRQ_HANDLED;
}

int iio_simple_dummy_events_register(struct iio_dev *indio_dev)
{
	struct iio_dummy_state *st = iio_priv(indio_dev);
	int ret;

	
	st->event_irq = iio_dummy_evgen_get_irq();
	if (st->event_irq < 0) {
		ret = st->event_irq;
		goto error_ret;
	}
	ret = request_threaded_irq(st->event_irq,
				   NULL,
				   &iio_simple_dummy_event_handler,
				   IRQF_ONESHOT,
				   "iio_simple_event",
				   indio_dev);
	if (ret < 0)
		goto error_free_evgen;
	return 0;

error_free_evgen:
	iio_dummy_evgen_release_irq(st->event_irq);
error_ret:
	return ret;
}

int iio_simple_dummy_events_unregister(struct iio_dev *indio_dev)
{
	struct iio_dummy_state *st = iio_priv(indio_dev);

	free_irq(st->event_irq, indio_dev);
	
	iio_dummy_evgen_release_irq(st->event_irq);

	return 0;
}
