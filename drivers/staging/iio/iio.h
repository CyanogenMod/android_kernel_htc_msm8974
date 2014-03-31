
/* The industrial I/O core
 *
 * Copyright (c) 2008 Jonathan Cameron
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */
#ifndef _INDUSTRIAL_IO_H_
#define _INDUSTRIAL_IO_H_

#include <linux/device.h>
#include <linux/cdev.h>
#include "types.h"

enum iio_data_type {
	IIO_RAW,
	IIO_PROCESSED,
};

enum iio_chan_info_enum {
	
	IIO_CHAN_INFO_SCALE = 1,
	IIO_CHAN_INFO_OFFSET,
	IIO_CHAN_INFO_CALIBSCALE,
	IIO_CHAN_INFO_CALIBBIAS,
	IIO_CHAN_INFO_PEAK,
	IIO_CHAN_INFO_PEAK_SCALE,
	IIO_CHAN_INFO_QUADRATURE_CORRECTION_RAW,
	IIO_CHAN_INFO_AVERAGE_RAW,
	IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY,
};

#define IIO_CHAN_INFO_SHARED_BIT(type) BIT(type*2)
#define IIO_CHAN_INFO_SEPARATE_BIT(type) BIT(type*2 + 1)

#define IIO_CHAN_INFO_SCALE_SEPARATE_BIT		\
	IIO_CHAN_INFO_SEPARATE_BIT(IIO_CHAN_INFO_SCALE)
#define IIO_CHAN_INFO_SCALE_SHARED_BIT			\
	IIO_CHAN_INFO_SHARED_BIT(IIO_CHAN_INFO_SCALE)
#define IIO_CHAN_INFO_OFFSET_SEPARATE_BIT			\
	IIO_CHAN_INFO_SEPARATE_BIT(IIO_CHAN_INFO_OFFSET)
#define IIO_CHAN_INFO_OFFSET_SHARED_BIT			\
	IIO_CHAN_INFO_SHARED_BIT(IIO_CHAN_INFO_OFFSET)
#define IIO_CHAN_INFO_CALIBSCALE_SEPARATE_BIT			\
	IIO_CHAN_INFO_SEPARATE_BIT(IIO_CHAN_INFO_CALIBSCALE)
#define IIO_CHAN_INFO_CALIBSCALE_SHARED_BIT			\
	IIO_CHAN_INFO_SHARED_BIT(IIO_CHAN_INFO_CALIBSCALE)
#define IIO_CHAN_INFO_CALIBBIAS_SEPARATE_BIT			\
	IIO_CHAN_INFO_SEPARATE_BIT(IIO_CHAN_INFO_CALIBBIAS)
#define IIO_CHAN_INFO_CALIBBIAS_SHARED_BIT			\
	IIO_CHAN_INFO_SHARED_BIT(IIO_CHAN_INFO_CALIBBIAS)
#define IIO_CHAN_INFO_PEAK_SEPARATE_BIT			\
	IIO_CHAN_INFO_SEPARATE_BIT(IIO_CHAN_INFO_PEAK)
#define IIO_CHAN_INFO_PEAK_SHARED_BIT			\
	IIO_CHAN_INFO_SHARED_BIT(IIO_CHAN_INFO_PEAK)
#define IIO_CHAN_INFO_PEAKSCALE_SEPARATE_BIT			\
	IIO_CHAN_INFO_SEPARATE_BIT(IIO_CHAN_INFO_PEAKSCALE)
#define IIO_CHAN_INFO_PEAKSCALE_SHARED_BIT			\
	IIO_CHAN_INFO_SHARED_BIT(IIO_CHAN_INFO_PEAKSCALE)
#define IIO_CHAN_INFO_QUADRATURE_CORRECTION_RAW_SEPARATE_BIT	\
	IIO_CHAN_INFO_SEPARATE_BIT(				\
		IIO_CHAN_INFO_QUADRATURE_CORRECTION_RAW)
#define IIO_CHAN_INFO_QUADRATURE_CORRECTION_RAW_SHARED_BIT	\
	IIO_CHAN_INFO_SHARED_BIT(				\
		IIO_CHAN_INFO_QUADRATURE_CORRECTION_RAW)
#define IIO_CHAN_INFO_AVERAGE_RAW_SEPARATE_BIT			\
	IIO_CHAN_INFO_SEPARATE_BIT(IIO_CHAN_INFO_AVERAGE_RAW)
#define IIO_CHAN_INFO_AVERAGE_RAW_SHARED_BIT			\
	IIO_CHAN_INFO_SHARED_BIT(IIO_CHAN_INFO_AVERAGE_RAW)
#define IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY_SHARED_BIT \
	IIO_CHAN_INFO_SHARED_BIT(			       \
		IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY)
#define IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY_SEPARATE_BIT \
	IIO_CHAN_INFO_SEPARATE_BIT(			       \
		IIO_CHAN_INFO_LOW_PASS_FILTER_3DB_FREQUENCY)

enum iio_endian {
	IIO_CPU,
	IIO_BE,
	IIO_LE,
};

struct iio_chan_spec;
struct iio_dev;

struct iio_chan_spec_ext_info {
	const char *name;
	bool shared;
	ssize_t (*read)(struct iio_dev *, struct iio_chan_spec const *,
			char *buf);
	ssize_t (*write)(struct iio_dev *, struct iio_chan_spec const *,
			const char *buf, size_t len);
};

struct iio_chan_spec {
	enum iio_chan_type	type;
	int			channel;
	int			channel2;
	unsigned long		address;
	int			scan_index;
	struct {
		char	sign;
		u8	realbits;
		u8	storagebits;
		u8	shift;
		enum iio_endian endianness;
	} scan_type;
	long			info_mask;
	long			event_mask;
	const struct iio_chan_spec_ext_info *ext_info;
	char			*extend_name;
	const char		*datasheet_name;
	unsigned		processed_val:1;
	unsigned		modified:1;
	unsigned		indexed:1;
	unsigned		output:1;
	unsigned		differential:1;
};

#define IIO_ST(si, rb, sb, sh)						\
	{ .sign = si, .realbits = rb, .storagebits = sb, .shift = sh }

#define IIO_CHAN(_type, _mod, _indexed, _proc, _name, _chan, _chan2, \
		 _inf_mask, _address, _si, _stype, _event_mask)		\
	{ .type = _type,						\
	  .output = 0,							\
	  .modified = _mod,						\
	  .indexed = _indexed,						\
	  .processed_val = _proc,					\
	  .extend_name = _name,						\
	  .channel = _chan,						\
	  .channel2 = _chan2,						\
	  .info_mask = _inf_mask,					\
	  .address = _address,						\
	  .scan_index = _si,						\
	  .scan_type = _stype,						\
	  .event_mask = _event_mask }

#define IIO_CHAN_SOFT_TIMESTAMP(_si)					\
	{ .type = IIO_TIMESTAMP, .channel = -1,				\
			.scan_index = _si, .scan_type = IIO_ST('s', 64, 64, 0) }

static inline s64 iio_get_time_ns(void)
{
	struct timespec ts;
	ktime_get_real_ts(&ts);

	return timespec_to_ns(&ts);
}

#define INDIO_DIRECT_MODE		0x01
#define INDIO_BUFFER_TRIGGERED		0x02
#define INDIO_BUFFER_HARDWARE		0x08

#define INDIO_ALL_BUFFER_MODES					\
	(INDIO_BUFFER_TRIGGERED | INDIO_BUFFER_HARDWARE)

struct iio_trigger; 
struct iio_dev;

struct iio_info {
	struct module			*driver_module;
	struct attribute_group		*event_attrs;
	const struct attribute_group	*attrs;

	int (*read_raw)(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			int *val,
			int *val2,
			long mask);

	int (*write_raw)(struct iio_dev *indio_dev,
			 struct iio_chan_spec const *chan,
			 int val,
			 int val2,
			 long mask);

	int (*write_raw_get_fmt)(struct iio_dev *indio_dev,
			 struct iio_chan_spec const *chan,
			 long mask);

	int (*read_event_config)(struct iio_dev *indio_dev,
				 u64 event_code);

	int (*write_event_config)(struct iio_dev *indio_dev,
				  u64 event_code,
				  int state);

	int (*read_event_value)(struct iio_dev *indio_dev,
				u64 event_code,
				int *val);
	int (*write_event_value)(struct iio_dev *indio_dev,
				 u64 event_code,
				 int val);
	int (*validate_trigger)(struct iio_dev *indio_dev,
				struct iio_trigger *trig);
	int (*update_scan_mode)(struct iio_dev *indio_dev,
				const unsigned long *scan_mask);
	int (*debugfs_reg_access)(struct iio_dev *indio_dev,
				  unsigned reg, unsigned writeval,
				  unsigned *readval);
};

struct iio_buffer_setup_ops {
	int				(*preenable)(struct iio_dev *);
	int				(*postenable)(struct iio_dev *);
	int				(*predisable)(struct iio_dev *);
	int				(*postdisable)(struct iio_dev *);
};

struct iio_dev {
	int				id;

	int				modes;
	int				currentmode;
	struct device			dev;

	struct iio_event_interface	*event_interface;

	struct iio_buffer		*buffer;
	struct mutex			mlock;

	const unsigned long		*available_scan_masks;
	unsigned			masklength;
	const unsigned long		*active_scan_mask;
	struct iio_trigger		*trig;
	struct iio_poll_func		*pollfunc;

	struct iio_chan_spec const	*channels;
	int				num_channels;

	struct list_head		channel_attr_list;
	struct attribute_group		chan_attr_group;
	const char			*name;
	const struct iio_info		*info;
	struct mutex			info_exist_lock;
	const struct iio_buffer_setup_ops	*setup_ops;
	struct cdev			chrdev;
#define IIO_MAX_GROUPS 6
	const struct attribute_group	*groups[IIO_MAX_GROUPS + 1];
	int				groupcounter;

	unsigned long			flags;
#if defined(CONFIG_DEBUG_FS)
	struct dentry			*debugfs_dentry;
	unsigned			cached_reg_addr;
#endif
};

const struct iio_chan_spec
*iio_find_channel_from_si(struct iio_dev *indio_dev, int si);

int iio_device_register(struct iio_dev *indio_dev);

void iio_device_unregister(struct iio_dev *indio_dev);

int iio_push_event(struct iio_dev *indio_dev, u64 ev_code, s64 timestamp);

extern struct bus_type iio_bus_type;

static inline void iio_put_device(struct iio_dev *indio_dev)
{
	if (indio_dev)
		put_device(&indio_dev->dev);
};

#define IIO_ALIGN L1_CACHE_BYTES
struct iio_dev *iio_allocate_device(int sizeof_priv);

static inline void *iio_priv(const struct iio_dev *indio_dev)
{
	return (char *)indio_dev + ALIGN(sizeof(struct iio_dev), IIO_ALIGN);
}

static inline struct iio_dev *iio_priv_to_dev(void *priv)
{
	return (struct iio_dev *)((char *)priv -
				  ALIGN(sizeof(struct iio_dev), IIO_ALIGN));
}

void iio_free_device(struct iio_dev *indio_dev);

static inline bool iio_buffer_enabled(struct iio_dev *indio_dev)
{
	return indio_dev->currentmode
		& (INDIO_BUFFER_TRIGGERED | INDIO_BUFFER_HARDWARE);
};

#if defined(CONFIG_DEBUG_FS)
static inline struct dentry *iio_get_debugfs_dentry(struct iio_dev *indio_dev)
{
	return indio_dev->debugfs_dentry;
};
#else
static inline struct dentry *iio_get_debugfs_dentry(struct iio_dev *indio_dev)
{
	return NULL;
};
#endif

#endif 
