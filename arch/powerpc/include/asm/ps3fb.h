/*
 * Copyright (C) 2006 Sony Computer Entertainment Inc.
 * Copyright 2006, 2007 Sony Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published
 * by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _ASM_POWERPC_PS3FB_H_
#define _ASM_POWERPC_PS3FB_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define PS3FB_IOCTL_SETMODE       _IOW('r',  1, int) 
#define PS3FB_IOCTL_GETMODE       _IOR('r',  2, int) 
#define PS3FB_IOCTL_SCREENINFO    _IOR('r',  3, int) 
#define PS3FB_IOCTL_ON            _IO('r', 4)        
#define PS3FB_IOCTL_OFF           _IO('r', 5)        
#define PS3FB_IOCTL_FSEL          _IOW('r', 6, int)  

#ifndef FBIO_WAITFORVSYNC
#define FBIO_WAITFORVSYNC         _IOW('F', 0x20, __u32) 
#endif

struct ps3fb_ioctl_res {
	__u32 xres; 
	__u32 yres; 
	__u32 xoff; 
	__u32 yoff; 
	__u32 num_frames; 
};

#endif 
