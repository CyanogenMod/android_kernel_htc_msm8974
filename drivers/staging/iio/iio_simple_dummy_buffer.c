/**
 * Copyright (c) 2011 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * Buffer handling elements of industrial I/O reference driver.
 * Uses the kfifo buffer.
 *
 * To test without hardware use the sysfs trigger.
 */

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/bitmap.h>

#include "iio.h"
#include "trigger_consumer.h"
#include "kfifo_buf.h"

#include "iio_simple_dummy.h"


static const s16 fakedata[] = {
	[voltage0] = 7,
	[diffvoltage1m2] = -33,
	[diffvoltage3m4] = -2,
	[accelx] = 344,
};
static irqreturn_t iio_simple_dummy_trigger_h(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct iio_buffer *buffer = indio_dev->buffer;
	int len = 0;
	size_t datasize = buffer->access->get_bytes_per_datum(buffer);
	u16 *data = kmalloc(datasize, GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	if (!bitmap_empty(indio_dev->active_scan_mask, indio_dev->masklength)) {
		int i, j;
		for (i = 0, j = 0;
		     i < bitmap_weight(indio_dev->active_scan_mask,
				       indio_dev->masklength);
		     i++) {
			j = find_next_bit(buffer->scan_mask,
					  indio_dev->masklength, j + 1);
			
			data[i] = fakedata[j];
			len += 2;
		}
	}
	
	if (buffer->scan_timestamp)
		*(s64 *)(((phys_addr_t)data + len
				+ sizeof(s64) - 1) & ~(sizeof(s64) - 1))
			= iio_get_time_ns();
	buffer->access->store_to(buffer, (u8 *)data, pf->timestamp);

	kfree(data);

	iio_trigger_notify_done(indio_dev->trig);

	return IRQ_HANDLED;
}

static const struct iio_buffer_setup_ops iio_simple_dummy_buffer_setup_ops = {
	.preenable = &iio_sw_buffer_preenable,
	.postenable = &iio_triggered_buffer_postenable,
	.predisable = &iio_triggered_buffer_predisable,
};

int iio_simple_dummy_configure_buffer(struct iio_dev *indio_dev)
{
	int ret;
	struct iio_buffer *buffer;

	
	buffer = iio_kfifo_allocate(indio_dev);
	if (buffer == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}

	indio_dev->buffer = buffer;

	
	buffer->scan_timestamp = true;

	indio_dev->setup_ops = &iio_simple_dummy_buffer_setup_ops;

	indio_dev->pollfunc = iio_alloc_pollfunc(NULL,
						 &iio_simple_dummy_trigger_h,
						 IRQF_ONESHOT,
						 indio_dev,
						 "iio_simple_dummy_consumer%d",
						 indio_dev->id);

	if (indio_dev->pollfunc == NULL) {
		ret = -ENOMEM;
		goto error_free_buffer;
	}

	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;
	return 0;

error_free_buffer:
	iio_kfifo_free(indio_dev->buffer);
error_ret:
	return ret;

}

void iio_simple_dummy_unconfigure_buffer(struct iio_dev *indio_dev)
{
	iio_dealloc_pollfunc(indio_dev->pollfunc);
	iio_kfifo_free(indio_dev->buffer);
}
