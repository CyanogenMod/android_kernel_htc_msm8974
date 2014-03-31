/*
 * Framework for ISA radio drivers.
 * This takes care of all the V4L2 scaffolding, allowing the ISA drivers
 * to concentrate on the actual hardware operation.
 *
 * Copyright (C) 2012 Hans Verkuil <hans.verkuil@cisco.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef _RADIO_ISA_H_
#define _RADIO_ISA_H_

#include <linux/isa.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>

struct radio_isa_driver;
struct radio_isa_ops;

struct radio_isa_card {
	const struct radio_isa_driver *drv;
	struct v4l2_device v4l2_dev;
	struct v4l2_ctrl_handler hdl;
	struct video_device vdev;
	struct mutex lock;
	const struct radio_isa_ops *ops;
	struct {	
		struct v4l2_ctrl *mute;
		struct v4l2_ctrl *volume;
	};
	
	int io;

	
	bool stereo;
	
	u32 freq;
};

struct radio_isa_ops {
	
	struct radio_isa_card *(*alloc)(void);
	
	bool (*probe)(struct radio_isa_card *isa, int io);
	int (*init)(struct radio_isa_card *isa);
	
	int (*s_mute_volume)(struct radio_isa_card *isa, bool mute, int volume);
	
	int (*s_frequency)(struct radio_isa_card *isa, u32 freq);
	
	int (*s_stereo)(struct radio_isa_card *isa, bool stereo);
	
	u32 (*g_rxsubchans)(struct radio_isa_card *isa);
	
	u32 (*g_signal)(struct radio_isa_card *isa);
};

struct radio_isa_driver {
	struct isa_driver driver;
	const struct radio_isa_ops *ops;
	
	int *io_params;
	
	int *radio_nr_params;
	
	bool probe;
	
	const int *io_ports;
	
	int num_of_io_ports;
	
	unsigned region_size;
	
	const char *card;
	
	bool has_stereo;
	int max_volume;
};

int radio_isa_match(struct device *pdev, unsigned int dev);
int radio_isa_probe(struct device *pdev, unsigned int dev);
int radio_isa_remove(struct device *pdev, unsigned int dev);

#endif
