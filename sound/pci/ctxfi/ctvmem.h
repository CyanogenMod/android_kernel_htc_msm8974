/**
 * Copyright (C) 2008, Creative Technology Ltd. All Rights Reserved.
 *
 * This source file is released under GPL v2 license (no other versions).
 * See the COPYING file included in the main directory of this source
 * distribution for the license terms and conditions.
 *
 * @File    ctvmem.h
 *
 * @Brief
 * This file contains the definition of virtual memory management object
 * for card device.
 *
 * @Author Liu Chun
 * @Date Mar 28 2008
 */

#ifndef CTVMEM_H
#define CTVMEM_H

#define CT_PTP_NUM	4	

#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <sound/memalloc.h>

#define CT_PAGE_SIZE	4096
#define CT_PAGE_SHIFT	12
#define CT_PAGE_MASK	(~(PAGE_SIZE - 1))
#define CT_PAGE_ALIGN(addr)	ALIGN(addr, CT_PAGE_SIZE)

struct ct_vm_block {
	unsigned int addr;	
	unsigned int size;	
	struct list_head list;
};

struct snd_pcm_substream;

struct ct_vm {
	struct snd_dma_buffer ptp[CT_PTP_NUM];	
	unsigned int size;		
	struct list_head unused;	
	struct list_head used;		
	struct mutex lock;

	
	struct ct_vm_block *(*map)(struct ct_vm *, struct snd_pcm_substream *,
				   int size);
	
	void (*unmap)(struct ct_vm *, struct ct_vm_block *block);
	dma_addr_t (*get_ptp_phys)(struct ct_vm *vm, int index);
};

int ct_vm_create(struct ct_vm **rvm, struct pci_dev *pci);
void ct_vm_destroy(struct ct_vm *vm);

#endif 
