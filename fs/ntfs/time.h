/*
 * time.h - NTFS time conversion functions.  Part of the Linux-NTFS project.
 *
 * Copyright (c) 2001-2005 Anton Altaparmakov
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _LINUX_NTFS_TIME_H
#define _LINUX_NTFS_TIME_H

#include <linux/time.h>		
#include <asm/div64.h>		

#include "endian.h"

#define NTFS_TIME_OFFSET ((s64)(369 * 365 + 89) * 24 * 3600 * 10000000)

static inline sle64 utc2ntfs(const struct timespec ts)
{
	return cpu_to_sle64((s64)ts.tv_sec * 10000000 + ts.tv_nsec / 100 +
			NTFS_TIME_OFFSET);
}

static inline sle64 get_current_ntfs_time(void)
{
	return utc2ntfs(current_kernel_time());
}

static inline struct timespec ntfs2utc(const sle64 time)
{
	struct timespec ts;

	
	u64 t = (u64)(sle64_to_cpu(time) - NTFS_TIME_OFFSET);
	ts.tv_nsec = do_div(t, 10000000) * 100;
	ts.tv_sec = t;
	return ts;
}

#endif 
