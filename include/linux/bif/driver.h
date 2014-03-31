/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _LINUX_BIF_DRIVER_H_
#define _LINUX_BIF_DRIVER_H_

#include <linux/device.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/bif/consumer.h>

struct bif_ctrl_dev;

struct bif_ctrl_ops {
	int (*bus_transaction) (struct bif_ctrl_dev *bdev, int transaction,
					u8 data);
	int (*bus_transaction_query) (struct bif_ctrl_dev *bdev,
					int transaction, u8 data,
					bool *query_response);
	int (*bus_transaction_read) (struct bif_ctrl_dev *bdev,
					int transaction, u8 data,
					int *response);
	int (*read_slave_registers) (struct bif_ctrl_dev *bdev, u16 addr,
					u8 *data, int len);
	int (*write_slave_registers) (struct bif_ctrl_dev *bdev, u16 addr,
					const u8 *data, int len);
	int (*get_bus_period) (struct bif_ctrl_dev *bdev);
	int (*set_bus_period) (struct bif_ctrl_dev *bdev, int period_ns);
	int (*get_battery_presence) (struct bif_ctrl_dev *bdev);
	int (*get_battery_rid) (struct bif_ctrl_dev *bdev);
	int (*get_bus_state) (struct bif_ctrl_dev *bdev);
	int (*set_bus_state) (struct bif_ctrl_dev *bdev, int state);
};

struct bif_ctrl_desc {
	const char *name;
	struct bif_ctrl_ops *ops;
	int bus_clock_min_ns;
	int bus_clock_max_ns;
};

#ifdef CONFIG_BIF

struct bif_ctrl_dev *bif_ctrl_register(struct bif_ctrl_desc *bif_desc,
	struct device *dev, void *driver_data, struct device_node *of_node);

void bif_ctrl_unregister(struct bif_ctrl_dev *bdev);

void *bdev_get_drvdata(struct bif_ctrl_dev *bdev);

int bif_ctrl_notify_battery_changed(struct bif_ctrl_dev *bdev);
int bif_ctrl_notify_slave_irq(struct bif_ctrl_dev *bdev);

#else

static inline struct bif_ctrl_dev *bif_ctrl_register(
	struct bif_ctrl_desc *bif_desc, struct device *dev, void *driver_data,
	struct device_node *of_node)
{ return ERR_PTR(-EINVAL); }

static inline void bif_ctrl_unregister(struct bif_ctrl_dev *bdev) { }

static inline void *bdev_get_drvdata(struct bif_ctrl_dev *bdev) { return NULL; }

int bif_ctrl_notify_slave_irq(struct bif_ctrl_dev *bdev) { return -EINVAL; }

#endif

#endif
