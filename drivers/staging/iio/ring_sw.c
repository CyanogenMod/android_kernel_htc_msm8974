/* The industrial I/O simple minimally locked ring buffer.
 *
 * Copyright (c) 2008 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include "ring_sw.h"
#include "trigger.h"

struct iio_sw_ring_buffer {
	struct iio_buffer  buf;
	unsigned char		*data;
	unsigned char		*read_p;
	unsigned char		*write_p;
	
	unsigned char		*half_p;
	int			update_needed;
};

#define iio_to_sw_ring(r) container_of(r, struct iio_sw_ring_buffer, buf)

static inline int __iio_allocate_sw_ring_buffer(struct iio_sw_ring_buffer *ring,
						int bytes_per_datum, int length)
{
	if ((length == 0) || (bytes_per_datum == 0))
		return -EINVAL;
	__iio_update_buffer(&ring->buf, bytes_per_datum, length);
	ring->data = kmalloc(length*ring->buf.bytes_per_datum, GFP_ATOMIC);
	ring->read_p = NULL;
	ring->write_p = NULL;
	ring->half_p = NULL;
	return ring->data ? 0 : -ENOMEM;
}

static inline void __iio_free_sw_ring_buffer(struct iio_sw_ring_buffer *ring)
{
	kfree(ring->data);
}

static int iio_store_to_sw_ring(struct iio_sw_ring_buffer *ring,
				unsigned char *data, s64 timestamp)
{
	int ret = 0;
	unsigned char *temp_ptr, *change_test_ptr;

	
	if (unlikely(ring->write_p == NULL)) {
		ring->write_p = ring->data;
		ring->half_p = ring->data - ring->buf.length*ring->buf.bytes_per_datum/2;
	}
	
	memcpy(ring->write_p, data, ring->buf.bytes_per_datum);
	barrier();
	barrier();
	temp_ptr = ring->write_p + ring->buf.bytes_per_datum;
	
	if (temp_ptr == ring->data + ring->buf.length*ring->buf.bytes_per_datum)
		temp_ptr = ring->data;
	ring->write_p = temp_ptr;

	if (ring->read_p == NULL)
		ring->read_p = ring->data;
	else if (ring->write_p == ring->read_p) {
		change_test_ptr = ring->read_p;
		temp_ptr = change_test_ptr + ring->buf.bytes_per_datum;
		if (temp_ptr
		    == ring->data + ring->buf.length*ring->buf.bytes_per_datum) {
			temp_ptr = ring->data;
		}
		if (change_test_ptr == ring->read_p)
			ring->read_p = temp_ptr;
	}
	
	
	ring->half_p += ring->buf.bytes_per_datum;
	if (ring->half_p == ring->data + ring->buf.length*ring->buf.bytes_per_datum)
		ring->half_p = ring->data;
	if (ring->half_p == ring->read_p) {
		ring->buf.stufftoread = true;
		wake_up_interruptible(&ring->buf.pollq);
	}
	return ret;
}

static int iio_read_first_n_sw_rb(struct iio_buffer *r,
				  size_t n, char __user *buf)
{
	struct iio_sw_ring_buffer *ring = iio_to_sw_ring(r);

	u8 *initial_read_p, *initial_write_p, *current_read_p, *end_read_p;
	u8 *data;
	int ret, max_copied, bytes_to_rip, dead_offset;
	size_t data_available, buffer_size;

	if (n % ring->buf.bytes_per_datum) {
		ret = -EINVAL;
		printk(KERN_INFO "Ring buffer read request not whole number of"
		       "samples: Request bytes %zd, Current bytes per datum %d\n",
		       n, ring->buf.bytes_per_datum);
		goto error_ret;
	}

	buffer_size = ring->buf.bytes_per_datum*ring->buf.length;

	
	bytes_to_rip = min_t(size_t, buffer_size, n);

	data = kmalloc(bytes_to_rip, GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}

	
	initial_read_p = ring->read_p;
	if (unlikely(initial_read_p == NULL)) { 
		ret = 0;
		goto error_free_data_cpy;
	}

	initial_write_p = ring->write_p;

	
	while ((initial_read_p != ring->read_p)
	       || (initial_write_p != ring->write_p)) {
		initial_read_p = ring->read_p;
		initial_write_p = ring->write_p;
	}
	if (initial_write_p == initial_read_p) {
		
		ret = 0;
		goto error_free_data_cpy;
	}

	if (initial_write_p >= initial_read_p)
		data_available = initial_write_p - initial_read_p;
	else
		data_available = buffer_size - (initial_read_p - initial_write_p);

	if (data_available < bytes_to_rip)
		bytes_to_rip = data_available;

	if (initial_read_p + bytes_to_rip >= ring->data + buffer_size) {
		max_copied = ring->data + buffer_size - initial_read_p;
		memcpy(data, initial_read_p, max_copied);
		memcpy(data + max_copied, ring->data, bytes_to_rip - max_copied);
		end_read_p = ring->data + bytes_to_rip - max_copied;
	} else {
		memcpy(data, initial_read_p, bytes_to_rip);
		end_read_p = initial_read_p + bytes_to_rip;
	}

	current_read_p = ring->read_p;

	if (initial_read_p <= current_read_p)
		dead_offset = current_read_p - initial_read_p;
	else
		dead_offset = buffer_size - (initial_read_p - current_read_p);

	if (bytes_to_rip - dead_offset < 0) {
		ret = 0;
		goto error_free_data_cpy;
	}

	
	

	while (ring->read_p != end_read_p)
		ring->read_p = end_read_p;

	ret = bytes_to_rip - dead_offset;

	if (copy_to_user(buf, data + dead_offset, ret))  {
		ret =  -EFAULT;
		goto error_free_data_cpy;
	}

	if (bytes_to_rip >= ring->buf.length*ring->buf.bytes_per_datum/2)
		ring->buf.stufftoread = 0;

error_free_data_cpy:
	kfree(data);
error_ret:

	return ret;
}

static int iio_store_to_sw_rb(struct iio_buffer *r,
			      u8 *data,
			      s64 timestamp)
{
	struct iio_sw_ring_buffer *ring = iio_to_sw_ring(r);
	return iio_store_to_sw_ring(ring, data, timestamp);
}

static int iio_request_update_sw_rb(struct iio_buffer *r)
{
	int ret = 0;
	struct iio_sw_ring_buffer *ring = iio_to_sw_ring(r);

	r->stufftoread = false;
	if (!ring->update_needed)
		goto error_ret;
	__iio_free_sw_ring_buffer(ring);
	ret = __iio_allocate_sw_ring_buffer(ring, ring->buf.bytes_per_datum,
					    ring->buf.length);
error_ret:
	return ret;
}

static int iio_get_bytes_per_datum_sw_rb(struct iio_buffer *r)
{
	struct iio_sw_ring_buffer *ring = iio_to_sw_ring(r);
	return ring->buf.bytes_per_datum;
}

static int iio_mark_update_needed_sw_rb(struct iio_buffer *r)
{
	struct iio_sw_ring_buffer *ring = iio_to_sw_ring(r);
	ring->update_needed = true;
	return 0;
}

static int iio_set_bytes_per_datum_sw_rb(struct iio_buffer *r, size_t bpd)
{
	if (r->bytes_per_datum != bpd) {
		r->bytes_per_datum = bpd;
		iio_mark_update_needed_sw_rb(r);
	}
	return 0;
}

static int iio_get_length_sw_rb(struct iio_buffer *r)
{
	return r->length;
}

static int iio_set_length_sw_rb(struct iio_buffer *r, int length)
{
	if (r->length != length) {
		r->length = length;
		iio_mark_update_needed_sw_rb(r);
	}
	return 0;
}

static IIO_BUFFER_ENABLE_ATTR;
static IIO_BUFFER_LENGTH_ATTR;

static struct attribute *iio_ring_attributes[] = {
	&dev_attr_length.attr,
	&dev_attr_enable.attr,
	NULL,
};

static struct attribute_group iio_ring_attribute_group = {
	.attrs = iio_ring_attributes,
	.name = "buffer",
};

static const struct iio_buffer_access_funcs ring_sw_access_funcs = {
	.store_to = &iio_store_to_sw_rb,
	.read_first_n = &iio_read_first_n_sw_rb,
	.request_update = &iio_request_update_sw_rb,
	.get_bytes_per_datum = &iio_get_bytes_per_datum_sw_rb,
	.set_bytes_per_datum = &iio_set_bytes_per_datum_sw_rb,
	.get_length = &iio_get_length_sw_rb,
	.set_length = &iio_set_length_sw_rb,
};

struct iio_buffer *iio_sw_rb_allocate(struct iio_dev *indio_dev)
{
	struct iio_buffer *buf;
	struct iio_sw_ring_buffer *ring;

	ring = kzalloc(sizeof *ring, GFP_KERNEL);
	if (!ring)
		return NULL;
	ring->update_needed = true;
	buf = &ring->buf;
	iio_buffer_init(buf);
	buf->attrs = &iio_ring_attribute_group;
	buf->access = &ring_sw_access_funcs;

	return buf;
}
EXPORT_SYMBOL(iio_sw_rb_allocate);

void iio_sw_rb_free(struct iio_buffer *r)
{
	kfree(iio_to_sw_ring(r));
}
EXPORT_SYMBOL(iio_sw_rb_free);

MODULE_DESCRIPTION("Industrialio I/O software ring buffer");
MODULE_LICENSE("GPL");
