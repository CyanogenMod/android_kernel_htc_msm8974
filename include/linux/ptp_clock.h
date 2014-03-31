/*
 * PTP 1588 clock support - user space interface
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

#ifndef _PTP_CLOCK_H_
#define _PTP_CLOCK_H_

#include <linux/ioctl.h>
#include <linux/types.h>

#define PTP_ENABLE_FEATURE (1<<0)
#define PTP_RISING_EDGE    (1<<1)
#define PTP_FALLING_EDGE   (1<<2)

struct ptp_clock_time {
	__s64 sec;  
	__u32 nsec; 
	__u32 reserved;
};

struct ptp_clock_caps {
	int max_adj;   
	int n_alarm;   
	int n_ext_ts;  
	int n_per_out; 
	int pps;       
	int rsv[15];   
};

struct ptp_extts_request {
	unsigned int index;  
	unsigned int flags;  
	unsigned int rsv[2]; 
};

struct ptp_perout_request {
	struct ptp_clock_time start;  
	struct ptp_clock_time period; 
	unsigned int index;           
	unsigned int flags;           
	unsigned int rsv[4];          
};

#define PTP_CLK_MAGIC '='

#define PTP_CLOCK_GETCAPS  _IOR(PTP_CLK_MAGIC, 1, struct ptp_clock_caps)
#define PTP_EXTTS_REQUEST  _IOW(PTP_CLK_MAGIC, 2, struct ptp_extts_request)
#define PTP_PEROUT_REQUEST _IOW(PTP_CLK_MAGIC, 3, struct ptp_perout_request)
#define PTP_ENABLE_PPS     _IOW(PTP_CLK_MAGIC, 4, int)

struct ptp_extts_event {
	struct ptp_clock_time t; 
	unsigned int index;      
	unsigned int flags;      
	unsigned int rsv[2];     
};

#endif
