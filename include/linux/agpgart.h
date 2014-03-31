/*
 * AGPGART module version 0.99
 * Copyright (C) 1999 Jeff Hartmann
 * Copyright (C) 1999 Precision Insight, Inc.
 * Copyright (C) 1999 Xi Graphics, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * JEFF HARTMANN, OR ANY OTHER CONTRIBUTORS BE LIABLE FOR ANY CLAIM, 
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE 
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef _AGP_H
#define _AGP_H 1

#define AGPIOC_BASE       'A'
#define AGPIOC_INFO       _IOR (AGPIOC_BASE, 0, struct agp_info*)
#define AGPIOC_ACQUIRE    _IO  (AGPIOC_BASE, 1)
#define AGPIOC_RELEASE    _IO  (AGPIOC_BASE, 2)
#define AGPIOC_SETUP      _IOW (AGPIOC_BASE, 3, struct agp_setup*)
#define AGPIOC_RESERVE    _IOW (AGPIOC_BASE, 4, struct agp_region*)
#define AGPIOC_PROTECT    _IOW (AGPIOC_BASE, 5, struct agp_region*)
#define AGPIOC_ALLOCATE   _IOWR(AGPIOC_BASE, 6, struct agp_allocate*)
#define AGPIOC_DEALLOCATE _IOW (AGPIOC_BASE, 7, int)
#define AGPIOC_BIND       _IOW (AGPIOC_BASE, 8, struct agp_bind*)
#define AGPIOC_UNBIND     _IOW (AGPIOC_BASE, 9, struct agp_unbind*)
#define AGPIOC_CHIPSET_FLUSH _IO (AGPIOC_BASE, 10)

#define AGP_DEVICE      "/dev/agpgart"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef __KERNEL__
#include <linux/types.h>

struct agp_version {
	__u16 major;
	__u16 minor;
};

typedef struct _agp_info {
	struct agp_version version;	
	__u32 bridge_id;	
	__u32 agp_mode;		
	unsigned long aper_base;
	size_t aper_size;	
	size_t pg_total;	
	size_t pg_system;	
	size_t pg_used;		
} agp_info;

typedef struct _agp_setup {
	__u32 agp_mode;		
} agp_setup;

typedef struct _agp_segment {
	__kernel_off_t pg_start;	
	__kernel_size_t pg_count;	
	int prot;			
} agp_segment;

typedef struct _agp_region {
	__kernel_pid_t pid;		
	__kernel_size_t seg_count;	
	struct _agp_segment *seg_list;
} agp_region;

typedef struct _agp_allocate {
	int key;		
	__kernel_size_t pg_count;
	__u32 type;		
   	__u32 physical;         
} agp_allocate;

typedef struct _agp_bind {
	int key;		
	__kernel_off_t pg_start;
} agp_bind;

typedef struct _agp_unbind {
	int key;		
	__u32 priority;		
} agp_unbind;

#else				
#include <linux/mutex.h>
#include <linux/agp_backend.h>

#define AGPGART_MINOR 175

struct agp_info {
	struct agp_version version;	
	u32 bridge_id;		
	u32 agp_mode;		
	unsigned long aper_base;
	size_t aper_size;	
	size_t pg_total;	
	size_t pg_system;	
	size_t pg_used;		
};

struct agp_setup {
	u32 agp_mode;		
};

struct agp_segment {
	off_t pg_start;		
	size_t pg_count;	
	int prot;		
};

struct agp_segment_priv {
	off_t pg_start;
	size_t pg_count;
	pgprot_t prot;
};

struct agp_region {
	pid_t pid;		
	size_t seg_count;	
	struct agp_segment *seg_list;
};

struct agp_allocate {
	int key;		
	size_t pg_count;	
	u32 type;		
	u32 physical;           
};

struct agp_bind {
	int key;		
	off_t pg_start;		
};

struct agp_unbind {
	int key;		
	u32 priority;		
};

struct agp_client {
	struct agp_client *next;
	struct agp_client *prev;
	pid_t pid;
	int num_segments;
	struct agp_segment_priv **segments;
};

struct agp_controller {
	struct agp_controller *next;
	struct agp_controller *prev;
	pid_t pid;
	int num_clients;
	struct agp_memory *pool;
	struct agp_client *clients;
};

#define AGP_FF_ALLOW_CLIENT		0
#define AGP_FF_ALLOW_CONTROLLER 	1
#define AGP_FF_IS_CLIENT		2
#define AGP_FF_IS_CONTROLLER		3
#define AGP_FF_IS_VALID 		4

struct agp_file_private {
	struct agp_file_private *next;
	struct agp_file_private *prev;
	pid_t my_pid;
	unsigned long access_flags;	
};

struct agp_front_data {
	struct mutex agp_mutex;
	struct agp_controller *current_controller;
	struct agp_controller *controllers;
	struct agp_file_private *file_priv_list;
	bool used_by_controller;
	bool backend_acquired;
};

#endif				

#endif				
