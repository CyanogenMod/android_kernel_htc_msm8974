/* via_dmablit.h -- PCI DMA BitBlt support for the VIA Unichrome/Pro
 *
 * Copyright 2005 Thomas Hellstrom.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Thomas Hellstrom.
 *    Register info from Digeo Inc.
 */

#ifndef _VIA_DMABLIT_H
#define _VIA_DMABLIT_H

#include <linux/dma-mapping.h>

#define VIA_NUM_BLIT_ENGINES 2
#define VIA_NUM_BLIT_SLOTS 8

struct _drm_via_descriptor;

typedef struct _drm_via_sg_info {
	struct page **pages;
	unsigned long num_pages;
	struct _drm_via_descriptor **desc_pages;
	int num_desc_pages;
	int num_desc;
	enum dma_data_direction direction;
	unsigned char *bounce_buffer;
	dma_addr_t chain_start;
	uint32_t free_on_sequence;
	unsigned int descriptors_per_page;
	int aborted;
	enum {
		dr_via_device_mapped,
		dr_via_desc_pages_alloc,
		dr_via_pages_locked,
		dr_via_pages_alloc,
		dr_via_sg_init
	} state;
} drm_via_sg_info_t;

typedef struct _drm_via_blitq {
	struct drm_device *dev;
	uint32_t cur_blit_handle;
	uint32_t done_blit_handle;
	unsigned serviced;
	unsigned head;
	unsigned cur;
	unsigned num_free;
	unsigned num_outstanding;
	unsigned long end;
	int aborting;
	int is_active;
	drm_via_sg_info_t *blits[VIA_NUM_BLIT_SLOTS];
	spinlock_t blit_lock;
	wait_queue_head_t blit_queue[VIA_NUM_BLIT_SLOTS];
	wait_queue_head_t busy_queue;
	struct work_struct wq;
	struct timer_list poll_timer;
} drm_via_blitq_t;



#define VIA_PCI_DMA_MAR0            0xE40   
#define VIA_PCI_DMA_DAR0            0xE44   
#define VIA_PCI_DMA_BCR0            0xE48   
#define VIA_PCI_DMA_DPR0            0xE4C   

#define VIA_PCI_DMA_MAR1            0xE50   
#define VIA_PCI_DMA_DAR1            0xE54   
#define VIA_PCI_DMA_BCR1            0xE58   
#define VIA_PCI_DMA_DPR1            0xE5C   

#define VIA_PCI_DMA_MAR2            0xE60   
#define VIA_PCI_DMA_DAR2            0xE64   
#define VIA_PCI_DMA_BCR2            0xE68   
#define VIA_PCI_DMA_DPR2            0xE6C   

#define VIA_PCI_DMA_MAR3            0xE70   
#define VIA_PCI_DMA_DAR3            0xE74   
#define VIA_PCI_DMA_BCR3            0xE78   
#define VIA_PCI_DMA_DPR3            0xE7C   

#define VIA_PCI_DMA_MR0             0xE80   
#define VIA_PCI_DMA_MR1             0xE84   
#define VIA_PCI_DMA_MR2             0xE88   
#define VIA_PCI_DMA_MR3             0xE8C   

#define VIA_PCI_DMA_CSR0            0xE90   
#define VIA_PCI_DMA_CSR1            0xE94   
#define VIA_PCI_DMA_CSR2            0xE98   
#define VIA_PCI_DMA_CSR3            0xE9C   

#define VIA_PCI_DMA_PTR             0xEA0   

#define VIA_DMA_DPR_EC		(1<<1)	
#define VIA_DMA_DPR_DDIE	(1<<2)	
#define VIA_DMA_DPR_DT		(1<<3)	

#define VIA_DMA_MR_CM		(1<<0)	
#define VIA_DMA_MR_TDIE		(1<<1)	
#define VIA_DMA_MR_HENDMACMD		(1<<7) 

#define VIA_DMA_CSR_DE		(1<<0)	
#define VIA_DMA_CSR_TS		(1<<1)	
#define VIA_DMA_CSR_TA		(1<<2)	
#define VIA_DMA_CSR_TD		(1<<3)	
#define VIA_DMA_CSR_DD		(1<<4)	
#define VIA_DMA_DPR_EC          (1<<1)  



#endif
