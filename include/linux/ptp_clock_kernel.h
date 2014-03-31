/*
 * PTP 1588 clock support
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

#ifndef _PTP_CLOCK_KERNEL_H_
#define _PTP_CLOCK_KERNEL_H_

#include <linux/ptp_clock.h>


struct ptp_clock_request {
	enum {
		PTP_CLK_REQ_EXTTS,
		PTP_CLK_REQ_PEROUT,
		PTP_CLK_REQ_PPS,
	} type;
	union {
		struct ptp_extts_request extts;
		struct ptp_perout_request perout;
	};
};


struct ptp_clock_info {
	struct module *owner;
	char name[16];
	s32 max_adj;
	int n_alarm;
	int n_ext_ts;
	int n_per_out;
	int pps;
	int (*adjfreq)(struct ptp_clock_info *ptp, s32 delta);
	int (*adjtime)(struct ptp_clock_info *ptp, s64 delta);
	int (*gettime)(struct ptp_clock_info *ptp, struct timespec *ts);
	int (*settime)(struct ptp_clock_info *ptp, const struct timespec *ts);
	int (*enable)(struct ptp_clock_info *ptp,
		      struct ptp_clock_request *request, int on);
};

struct ptp_clock;


extern struct ptp_clock *ptp_clock_register(struct ptp_clock_info *info);


extern int ptp_clock_unregister(struct ptp_clock *ptp);


enum ptp_clock_events {
	PTP_CLOCK_ALARM,
	PTP_CLOCK_EXTTS,
	PTP_CLOCK_PPS,
};


struct ptp_clock_event {
	int type;
	int index;
	u64 timestamp;
};


extern void ptp_clock_event(struct ptp_clock *ptp,
			    struct ptp_clock_event *event);

#endif
