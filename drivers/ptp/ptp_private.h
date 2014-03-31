/*
 * PTP 1588 clock support - private declarations for the core module.
 *
 * Copyright (C) 2010 OMICRON electronics GmbH
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef _PTP_PRIVATE_H_
#define _PTP_PRIVATE_H_

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/posix-clock.h>
#include <linux/ptp_clock.h>
#include <linux/ptp_clock_kernel.h>
#include <linux/time.h>

#define PTP_MAX_TIMESTAMPS 128
#define PTP_BUF_TIMESTAMPS 30

struct timestamp_event_queue {
	struct ptp_extts_event buf[PTP_MAX_TIMESTAMPS];
	int head;
	int tail;
	spinlock_t lock;
};

struct ptp_clock {
	struct posix_clock clock;
	struct device *dev;
	struct ptp_clock_info *info;
	dev_t devid;
	int index; 
	struct pps_device *pps_source;
	struct timestamp_event_queue tsevq; 
	struct mutex tsevq_mux; 
	wait_queue_head_t tsev_wq;
	int defunct; 
};

static inline int queue_cnt(struct timestamp_event_queue *q)
{
	int cnt = q->tail - q->head;
	return cnt < 0 ? PTP_MAX_TIMESTAMPS + cnt : cnt;
}


long ptp_ioctl(struct posix_clock *pc,
	       unsigned int cmd, unsigned long arg);

int ptp_open(struct posix_clock *pc, fmode_t fmode);

ssize_t ptp_read(struct posix_clock *pc,
		 uint flags, char __user *buf, size_t cnt);

uint ptp_poll(struct posix_clock *pc,
	      struct file *fp, poll_table *wait);


extern struct device_attribute ptp_dev_attrs[];

int ptp_cleanup_sysfs(struct ptp_clock *ptp);

int ptp_populate_sysfs(struct ptp_clock *ptp);

#endif
