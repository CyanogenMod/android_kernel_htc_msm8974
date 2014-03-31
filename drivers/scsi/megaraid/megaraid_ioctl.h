/*
 *
 *			Linux MegaRAID device driver
 *
 * Copyright (c) 2003-2004  LSI Logic Corporation.
 *
 *	   This program is free software; you can redistribute it and/or
 *	   modify it under the terms of the GNU General Public License
 *	   as published by the Free Software Foundation; either version
 *	   2 of the License, or (at your option) any later version.
 *
 * FILE		: megaraid_ioctl.h
 *
 * Definitions to interface with user level applications
 */

#ifndef _MEGARAID_IOCTL_H_
#define _MEGARAID_IOCTL_H_

#include <linux/types.h>
#include <linux/semaphore.h>

#include "mbox_defs.h"

#define	CL_ANN		0	
#define CL_DLEVEL1	1	
#define CL_DLEVEL2	2	
#define CL_DLEVEL3	3	

#define	con_log(level, fmt) if (LSI_DBGLVL >= level) printk fmt;


#define MEGAIOC_MAGIC		'm'
#define MEGAIOCCMD		_IOWR(MEGAIOC_MAGIC, 0, mimd_t)

#define MEGAIOC_QNADAP		'm'	
#define MEGAIOC_QDRVRVER	'e'	
#define MEGAIOC_QADAPINFO   	'g'	

#define USCSICMD		0x80
#define UIOC_RD			0x00001
#define UIOC_WR			0x00002

#define MBOX_CMD		0x00000
#define GET_DRIVER_VER		0x10000
#define GET_N_ADAP		0x20000
#define GET_ADAP_INFO		0x30000
#define GET_CAP			0x40000
#define GET_STATS		0x50000
#define GET_IOCTL_VERSION	0x01

#define EXT_IOCTL_SIGN_SZ	16
#define EXT_IOCTL_SIGN		"$$_EXTD_IOCTL_$$"

#define	MBOX_LEGACY		0x00		
#define MBOX_HPE		0x01		

#define	APPTYPE_MIMD		0x00		
#define APPTYPE_UIOC		0x01		

#define IOCTL_ISSUE		0x00000001	
#define IOCTL_ABORT		0x00000002	

#define DRVRTYPE_MBOX		0x00000001	
#define DRVRTYPE_HPE		0x00000002	

#define MKADAP(adapno)	(MEGAIOC_MAGIC << 8 | (adapno) )
#define GETADAP(mkadap)	((mkadap) ^ MEGAIOC_MAGIC << 8)

#define MAX_DMA_POOLS		5		


typedef struct uioc {


	uint8_t			signature[EXT_IOCTL_SIGN_SZ];
	uint16_t		mb_type;
	uint16_t		app_type;
	uint32_t		opcode;
	uint32_t		adapno;
	uint64_t		cmdbuf;
	uint32_t		xferlen;
	uint32_t		data_dir;
	int32_t			status;
	uint8_t			reserved[128];

	void __user *		user_data;
	uint32_t		user_data_len;

	
	uint32_t                pad_for_64bit_align;

	mraid_passthru_t	__user *user_pthru;

	mraid_passthru_t	*pthru32;
	dma_addr_t		pthru32_h;

	struct list_head	list;
	void			(*done)(struct uioc*);

	caddr_t			buf_vaddr;
	dma_addr_t		buf_paddr;
	int8_t			pool_index;
	uint8_t			free_buf;

	uint8_t			timedout;

} __attribute__ ((aligned(1024),packed)) uioc_t;


typedef struct mraid_hba_info {

	uint16_t	pci_vendor_id;
	uint16_t	pci_device_id;
	uint16_t	subsys_vendor_id;
	uint16_t	subsys_device_id;

	uint64_t	baseport;
	uint8_t		pci_bus;
	uint8_t		pci_dev_fn;
	uint8_t		pci_slot;
	uint8_t		irq;

	uint32_t	unique_id;
	uint32_t	host_no;

	uint8_t		num_ldrv;
} __attribute__ ((aligned(256), packed)) mraid_hba_info_t;


typedef struct mcontroller {

	uint64_t	base;
	uint8_t		irq;
	uint8_t		numldrv;
	uint8_t		pcibus;
	uint16_t	pcidev;
	uint8_t		pcifun;
	uint16_t	pciid;
	uint16_t	pcivendor;
	uint8_t		pcislot;
	uint32_t	uid;

} __attribute__ ((packed)) mcontroller_t;


typedef struct mm_dmapool {
	caddr_t		vaddr;
	dma_addr_t	paddr;
	uint32_t	buf_size;
	struct dma_pool	*handle;
	spinlock_t	lock;
	uint8_t		in_use;
} mm_dmapool_t;



typedef struct mraid_mmadp {


	uint32_t		unique_id;
	uint32_t		drvr_type;
	unsigned long		drvr_data;
	uint16_t		timeout;
	uint8_t			max_kioc;

	struct pci_dev		*pdev;

	int(*issue_uioc)(unsigned long, uioc_t *, uint32_t);

	uint32_t		quiescent;

	struct list_head	list;
	uioc_t			*kioc_list;
	struct list_head	kioc_pool;
	spinlock_t		kioc_pool_lock;
	struct semaphore	kioc_semaphore;

	mbox64_t		*mbox_list;
	struct dma_pool		*pthru_dma_pool;
	mm_dmapool_t		dma_pool_list[MAX_DMA_POOLS];

} mraid_mmadp_t;

int mraid_mm_register_adp(mraid_mmadp_t *);
int mraid_mm_unregister_adp(uint32_t);
uint32_t mraid_mm_adapter_app_handle(uint32_t);

#endif 
