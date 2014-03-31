/*
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

#ifndef __XEN_BLKIF__BACKEND__COMMON_H__
#define __XEN_BLKIF__BACKEND__COMMON_H__

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/io.h>
#include <asm/setup.h>
#include <asm/pgalloc.h>
#include <asm/hypervisor.h>
#include <xen/grant_table.h>
#include <xen/xenbus.h>
#include <xen/interface/io/ring.h>
#include <xen/interface/io/blkif.h>
#include <xen/interface/io/protocols.h>

#define DRV_PFX "xen-blkback:"
#define DPRINTK(fmt, args...)				\
	pr_debug(DRV_PFX "(%s:%d) " fmt ".\n",		\
		 __func__, __LINE__, ##args)


struct blkif_common_request {
	char dummy;
};
struct blkif_common_response {
	char dummy;
};

struct blkif_x86_32_request_rw {
	uint8_t        nr_segments;  
	blkif_vdev_t   handle;       
	uint64_t       id;           
	blkif_sector_t sector_number;
	struct blkif_request_segment seg[BLKIF_MAX_SEGMENTS_PER_REQUEST];
} __attribute__((__packed__));

struct blkif_x86_32_request_discard {
	uint8_t        flag;         
	blkif_vdev_t   _pad1;        
	uint64_t       id;           
	blkif_sector_t sector_number;
	uint64_t       nr_sectors;
} __attribute__((__packed__));

struct blkif_x86_32_request {
	uint8_t        operation;    
	union {
		struct blkif_x86_32_request_rw rw;
		struct blkif_x86_32_request_discard discard;
	} u;
} __attribute__((__packed__));

#pragma pack(push, 4)
struct blkif_x86_32_response {
	uint64_t        id;              
	uint8_t         operation;       
	int16_t         status;          
};
#pragma pack(pop)

struct blkif_x86_64_request_rw {
	uint8_t        nr_segments;  
	blkif_vdev_t   handle;       
	uint32_t       _pad1;        
	uint64_t       id;
	blkif_sector_t sector_number;
	struct blkif_request_segment seg[BLKIF_MAX_SEGMENTS_PER_REQUEST];
} __attribute__((__packed__));

struct blkif_x86_64_request_discard {
	uint8_t        flag;         
	blkif_vdev_t   _pad1;        
        uint32_t       _pad2;        
	uint64_t       id;
	blkif_sector_t sector_number;
	uint64_t       nr_sectors;
} __attribute__((__packed__));

struct blkif_x86_64_request {
	uint8_t        operation;    
	union {
		struct blkif_x86_64_request_rw rw;
		struct blkif_x86_64_request_discard discard;
	} u;
} __attribute__((__packed__));

struct blkif_x86_64_response {
	uint64_t       __attribute__((__aligned__(8))) id;
	uint8_t         operation;       
	int16_t         status;          
};

DEFINE_RING_TYPES(blkif_common, struct blkif_common_request,
		  struct blkif_common_response);
DEFINE_RING_TYPES(blkif_x86_32, struct blkif_x86_32_request,
		  struct blkif_x86_32_response);
DEFINE_RING_TYPES(blkif_x86_64, struct blkif_x86_64_request,
		  struct blkif_x86_64_response);

union blkif_back_rings {
	struct blkif_back_ring        native;
	struct blkif_common_back_ring common;
	struct blkif_x86_32_back_ring x86_32;
	struct blkif_x86_64_back_ring x86_64;
};

enum blkif_protocol {
	BLKIF_PROTOCOL_NATIVE = 1,
	BLKIF_PROTOCOL_X86_32 = 2,
	BLKIF_PROTOCOL_X86_64 = 3,
};

struct xen_vbd {
	
	blkif_vdev_t		handle;
	
	unsigned char		readonly;
	
	unsigned char		type;
	
	u32			pdevice;
	struct block_device	*bdev;
	
	sector_t		size;
	bool			flush_support;
	bool			discard_secure;
};

struct backend_info;

struct xen_blkif {
	
	domid_t			domid;
	unsigned int		handle;
	
	unsigned int		irq;
	
	enum blkif_protocol	blk_protocol;
	union blkif_back_rings	blk_rings;
	void			*blk_ring;
	
	struct xen_vbd		vbd;
	
	struct backend_info	*be;
	
	spinlock_t		blk_ring_lock;
	atomic_t		refcnt;

	wait_queue_head_t	wq;
	
	struct completion	drain_complete;
	atomic_t		drain;
	
	struct task_struct	*xenblkd;
	unsigned int		waiting_reqs;

	
	unsigned long		st_print;
	int			st_rd_req;
	int			st_wr_req;
	int			st_oo_req;
	int			st_f_req;
	int			st_ds_req;
	int			st_rd_sect;
	int			st_wr_sect;

	wait_queue_head_t	waiting_to_free;
};


#define vbd_sz(_v)	((_v)->bdev->bd_part ? \
			 (_v)->bdev->bd_part->nr_sects : \
			  get_capacity((_v)->bdev->bd_disk))

#define xen_blkif_get(_b) (atomic_inc(&(_b)->refcnt))
#define xen_blkif_put(_b)				\
	do {						\
		if (atomic_dec_and_test(&(_b)->refcnt))	\
			wake_up(&(_b)->waiting_to_free);\
	} while (0)

struct phys_req {
	unsigned short		dev;
	blkif_sector_t		nr_sects;
	struct block_device	*bdev;
	blkif_sector_t		sector_number;
};
int xen_blkif_interface_init(void);

int xen_blkif_xenbus_init(void);

irqreturn_t xen_blkif_be_int(int irq, void *dev_id);
int xen_blkif_schedule(void *arg);

int xen_blkbk_flush_diskcache(struct xenbus_transaction xbt,
			      struct backend_info *be, int state);

int xen_blkbk_barrier(struct xenbus_transaction xbt,
		      struct backend_info *be, int state);
struct xenbus_device *xen_blkbk_xenbus(struct backend_info *be);

static inline void blkif_get_x86_32_req(struct blkif_request *dst,
					struct blkif_x86_32_request *src)
{
	int i, n = BLKIF_MAX_SEGMENTS_PER_REQUEST;
	dst->operation = src->operation;
	switch (src->operation) {
	case BLKIF_OP_READ:
	case BLKIF_OP_WRITE:
	case BLKIF_OP_WRITE_BARRIER:
	case BLKIF_OP_FLUSH_DISKCACHE:
		dst->u.rw.nr_segments = src->u.rw.nr_segments;
		dst->u.rw.handle = src->u.rw.handle;
		dst->u.rw.id = src->u.rw.id;
		dst->u.rw.sector_number = src->u.rw.sector_number;
		barrier();
		if (n > dst->u.rw.nr_segments)
			n = dst->u.rw.nr_segments;
		for (i = 0; i < n; i++)
			dst->u.rw.seg[i] = src->u.rw.seg[i];
		break;
	case BLKIF_OP_DISCARD:
		dst->u.discard.flag = src->u.discard.flag;
		dst->u.discard.sector_number = src->u.discard.sector_number;
		dst->u.discard.nr_sectors = src->u.discard.nr_sectors;
		break;
	default:
		break;
	}
}

static inline void blkif_get_x86_64_req(struct blkif_request *dst,
					struct blkif_x86_64_request *src)
{
	int i, n = BLKIF_MAX_SEGMENTS_PER_REQUEST;
	dst->operation = src->operation;
	switch (src->operation) {
	case BLKIF_OP_READ:
	case BLKIF_OP_WRITE:
	case BLKIF_OP_WRITE_BARRIER:
	case BLKIF_OP_FLUSH_DISKCACHE:
		dst->u.rw.nr_segments = src->u.rw.nr_segments;
		dst->u.rw.handle = src->u.rw.handle;
		dst->u.rw.id = src->u.rw.id;
		dst->u.rw.sector_number = src->u.rw.sector_number;
		barrier();
		if (n > dst->u.rw.nr_segments)
			n = dst->u.rw.nr_segments;
		for (i = 0; i < n; i++)
			dst->u.rw.seg[i] = src->u.rw.seg[i];
		break;
	case BLKIF_OP_DISCARD:
		dst->u.discard.flag = src->u.discard.flag;
		dst->u.discard.sector_number = src->u.discard.sector_number;
		dst->u.discard.nr_sectors = src->u.discard.nr_sectors;
		break;
	default:
		break;
	}
}

#endif 
