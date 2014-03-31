/******************************************************************************
 * gntdev.h
 * 
 * Interface to /dev/xen/gntdev.
 * 
 * Copyright (c) 2007, D G Murray
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation; or, when distributed
 * separately from the Linux kernel or incorporated into other
 * software packages, subject to the following license:
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef __LINUX_PUBLIC_GNTDEV_H__
#define __LINUX_PUBLIC_GNTDEV_H__

struct ioctl_gntdev_grant_ref {
	
	uint32_t domid;
	
	uint32_t ref;
};

#define IOCTL_GNTDEV_MAP_GRANT_REF \
_IOC(_IOC_NONE, 'G', 0, sizeof(struct ioctl_gntdev_map_grant_ref))
struct ioctl_gntdev_map_grant_ref {
	
	
	uint32_t count;
	uint32_t pad;
	
	
	uint64_t index;
	
	
	struct ioctl_gntdev_grant_ref refs[1];
};

#define IOCTL_GNTDEV_UNMAP_GRANT_REF \
_IOC(_IOC_NONE, 'G', 1, sizeof(struct ioctl_gntdev_unmap_grant_ref))
struct ioctl_gntdev_unmap_grant_ref {
	
	
	uint64_t index;
	
	uint32_t count;
	uint32_t pad;
};

#define IOCTL_GNTDEV_GET_OFFSET_FOR_VADDR \
_IOC(_IOC_NONE, 'G', 2, sizeof(struct ioctl_gntdev_get_offset_for_vaddr))
struct ioctl_gntdev_get_offset_for_vaddr {
	
	
	uint64_t vaddr;
	
	
	uint64_t offset;
	
	uint32_t count;
	uint32_t pad;
};

#define IOCTL_GNTDEV_SET_MAX_GRANTS \
_IOC(_IOC_NONE, 'G', 3, sizeof(struct ioctl_gntdev_set_max_grants))
struct ioctl_gntdev_set_max_grants {
	
	
	uint32_t count;
};

#define IOCTL_GNTDEV_SET_UNMAP_NOTIFY \
_IOC(_IOC_NONE, 'G', 7, sizeof(struct ioctl_gntdev_unmap_notify))
struct ioctl_gntdev_unmap_notify {
	
	uint64_t index;
	
	uint32_t action;
	
	uint32_t event_channel_port;
};

#define UNMAP_NOTIFY_CLEAR_BYTE 0x1
#define UNMAP_NOTIFY_SEND_EVENT 0x2

#endif 
