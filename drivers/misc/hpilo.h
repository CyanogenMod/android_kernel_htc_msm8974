/*
 * linux/drivers/char/hpilo.h
 *
 * Copyright (C) 2008 Hewlett-Packard Development Company, L.P.
 *	David Altobelli <david.altobelli@hp.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __HPILO_H
#define __HPILO_H

#define ILO_NAME "hpilo"

#define MAX_CCB		8
#define MAX_ILO_DEV	1
#define MAX_OPEN	(MAX_CCB * MAX_ILO_DEV)
#define MAX_WAIT_TIME	10000
#define WAIT_TIME	10
#define MAX_WAIT	(MAX_WAIT_TIME / WAIT_TIME)

struct ilo_hwinfo {
	
	char __iomem *mmio_vaddr;

	
	char __iomem *db_vaddr;

	
	char __iomem *ram_vaddr;

	
	struct ccb_data *ccb_alloc[MAX_CCB];

	struct pci_dev *ilo_dev;

	spinlock_t open_lock;
	spinlock_t alloc_lock;
	spinlock_t fifo_lock;

	struct cdev cdev;
};

#define DB_IRQ		0xB2
#define DB_OUT		0xD4
#define DB_RESET	26

#define ILOSW_CCB_SZ	64
#define ILOHW_CCB_SZ 	128
struct ccb {
	union {
		char *send_fifobar;
		u64 send_fifobar_pa;
	} ccb_u1;
	union {
		char *send_desc;
		u64 send_desc_pa;
	} ccb_u2;
	u64 send_ctrl;

	union {
		char *recv_fifobar;
		u64 recv_fifobar_pa;
	} ccb_u3;
	union {
		char *recv_desc;
		u64 recv_desc_pa;
	} ccb_u4;
	u64 recv_ctrl;

	union {
		char __iomem *db_base;
		u64 padding5;
	} ccb_u5;

	u64 channel;

	
};

#define SENDQ		1
#define RECVQ 		2
#define NR_QENTRY    	4
#define L2_QENTRY_SZ 	12

#define CTRL_BITPOS_L2SZ             0
#define CTRL_BITPOS_FIFOINDEXMASK    4
#define CTRL_BITPOS_DESCLIMIT        18
#define CTRL_BITPOS_A                30
#define CTRL_BITPOS_G                31

#define L2_DB_SIZE		14
#define ONE_DB_SIZE		(1 << L2_DB_SIZE)

struct ccb_data {
	
	struct ccb  driver_ccb;

	
	struct ccb  ilo_ccb;

	/* hardware ccb is written to this shared mapped device memory */
	struct ccb __iomem *mapped_ccb;

	
	void       *dma_va;
	dma_addr_t  dma_pa;
	size_t      dma_size;

	
	struct ilo_hwinfo *ilo_hw;

	
	wait_queue_head_t ccb_waitq;

	
	int	    ccb_cnt;

	
	int	    ccb_excl;
};

#define ILO_START_ALIGN	4096
#define ILO_CACHE_SZ 	 128
struct fifo {
    u64 nrents;	
    u64 imask;  
    u64 merge;	
    u64 reset;	
    u8  pad_0[ILO_CACHE_SZ - (sizeof(u64) * 4)];

    u64 head;
    u8  pad_1[ILO_CACHE_SZ - (sizeof(u64))];

    u64 tail;
    u8  pad_2[ILO_CACHE_SZ - (sizeof(u64))];

    u64 fifobar[1];
};

#define FIFOHANDLESIZE (sizeof(struct fifo) - sizeof(u64))
#define FIFOBARTOHANDLE(_fifo) \
	((struct fifo *)(((char *)(_fifo)) - FIFOHANDLESIZE))

#define ENTRY_BITPOS_QWORDS      0
#define ENTRY_BITPOS_DESCRIPTOR  10
#define ENTRY_BITPOS_C           22
#define ENTRY_BITPOS_O           23

#define ENTRY_BITS_QWORDS        10
#define ENTRY_BITS_DESCRIPTOR    12
#define ENTRY_BITS_C             1
#define ENTRY_BITS_O             1
#define ENTRY_BITS_TOTAL	\
	(ENTRY_BITS_C + ENTRY_BITS_O + \
	 ENTRY_BITS_QWORDS + ENTRY_BITS_DESCRIPTOR)

#define ENTRY_MASK ((1 << ENTRY_BITS_TOTAL) - 1)
#define ENTRY_MASK_C (((1 << ENTRY_BITS_C) - 1) << ENTRY_BITPOS_C)
#define ENTRY_MASK_O (((1 << ENTRY_BITS_O) - 1) << ENTRY_BITPOS_O)
#define ENTRY_MASK_QWORDS \
	(((1 << ENTRY_BITS_QWORDS) - 1) << ENTRY_BITPOS_QWORDS)
#define ENTRY_MASK_DESCRIPTOR \
	(((1 << ENTRY_BITS_DESCRIPTOR) - 1) << ENTRY_BITPOS_DESCRIPTOR)

#define ENTRY_MASK_NOSTATE (ENTRY_MASK >> (ENTRY_BITS_C + ENTRY_BITS_O))

#endif 
