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
 * FILE		: mega_common.h
 *
 * Libaray of common routine used by all low-level megaraid drivers
 */

#ifndef _MEGA_COMMON_H_
#define _MEGA_COMMON_H_

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/blkdev.h>
#include <linux/list.h>
#include <linux/moduleparam.h>
#include <linux/dma-mapping.h>
#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>


#define LSI_MAX_CHANNELS		16
#define LSI_MAX_LOGICAL_DRIVES_64LD	(64+1)

#define HBA_SIGNATURE_64_BIT		0x299
#define PCI_CONF_AMISIG64		0xa4

#define MEGA_SCSI_INQ_EVPD		1
#define MEGA_INVALID_FIELD_IN_CDB	0x24


typedef struct {
	caddr_t			ccb;
	struct list_head	list;
	unsigned long		gp;
	unsigned int		sno;
	struct scsi_cmnd	*scp;
	uint32_t		state;
	uint32_t		dma_direction;
	uint32_t		dma_type;
	uint16_t		dev_channel;
	uint16_t		dev_target;
	uint32_t		status;
} scb_t;

#define SCB_FREE	0x0000	
#define SCB_ACTIVE	0x0001	
#define SCB_PENDQ	0x0002	
#define SCB_ISSUED	0x0004	
#define SCB_ABORT	0x0008	
#define SCB_RESET	0x0010	

#define MRAID_DMA_NONE	0x0000	
#define MRAID_DMA_WSG	0x0001	
#define MRAID_DMA_WBUF	0x0002	



#define VERSION_SIZE	16

typedef struct {
	struct tasklet_struct	dpc_h;
	struct pci_dev		*pdev;
	struct Scsi_Host	*host;
	spinlock_t		lock;
	uint8_t			quiescent;
	int			outstanding_cmds;
	scb_t			*kscb_list;
	struct list_head	kscb_pool;
	spinlock_t		kscb_pool_lock;
	struct list_head	pend_list;
	spinlock_t		pend_list_lock;
	struct list_head	completed_list;
	spinlock_t		completed_list_lock;
	uint16_t		sglen;
	int			device_ids[LSI_MAX_CHANNELS]
					[LSI_MAX_LOGICAL_DRIVES_64LD];
	caddr_t			raid_device;
	uint8_t			max_channel;
	uint16_t		max_target;
	uint8_t			max_lun;

	uint32_t		unique_id;
	int			irq;
	uint8_t			ito;
	caddr_t			ibuf;
	dma_addr_t		ibuf_dma_h;
	scb_t			*uscb_list;
	struct list_head	uscb_pool;
	spinlock_t		uscb_pool_lock;
	int			max_cmds;
	uint8_t			fw_version[VERSION_SIZE];
	uint8_t			bios_version[VERSION_SIZE];
	uint8_t			max_cdb_sz;
	uint8_t			ha;
	uint16_t		init_id;
	uint16_t		max_sectors;
	uint16_t		cmd_per_lun;
	atomic_t		being_detached;
} adapter_t;

#define SCSI_FREE_LIST_LOCK(adapter)	(&adapter->kscb_pool_lock)
#define USER_FREE_LIST_LOCK(adapter)	(&adapter->uscb_pool_lock)
#define PENDING_LIST_LOCK(adapter)	(&adapter->pend_list_lock)
#define COMPLETED_LIST_LOCK(adapter)	(&adapter->completed_list_lock)


#define SCP2HOST(scp)			(scp)->device->host	
#define SCP2HOSTDATA(scp)		SCP2HOST(scp)->hostdata	
#define SCP2CHANNEL(scp)		(scp)->device->channel	
#define SCP2TARGET(scp)			(scp)->device->id	
#define SCP2LUN(scp)			(scp)->device->lun	

#define SCSIHOST2ADAP(host)	(((caddr_t *)(host->hostdata))[0])
#define SCP2ADAPTER(scp)	(adapter_t *)SCSIHOST2ADAP(SCP2HOST(scp))


#define MRAID_IS_LOGICAL(adp, scp)	\
	(SCP2CHANNEL(scp) == (adp)->max_channel) ? 1 : 0

#define MRAID_IS_LOGICAL_SDEV(adp, sdev)	\
	(sdev->channel == (adp)->max_channel) ? 1 : 0

#define MRAID_GET_DEVICE_MAP(adp, scp, p_chan, target, islogical)	\
								\
	islogical = MRAID_IS_LOGICAL(adp, scp);				\
									\
								\
	if (islogical) {						\
		p_chan = 0xFF;						\
		target =						\
		(adp)->device_ids[(adp)->max_channel][SCP2TARGET(scp)];	\
	}								\
	else {								\
		p_chan = ((adp)->device_ids[SCP2CHANNEL(scp)]		\
					[SCP2TARGET(scp)] >> 8) & 0xFF;	\
		target = ((adp)->device_ids[SCP2CHANNEL(scp)]		\
					[SCP2TARGET(scp)] & 0xFF);	\
	}

#define LSI_DBGLVL mraid_debug_level	
 					

#ifdef DEBUG
#if defined (_ASSERT_PANIC)
#define ASSERT_ACTION	panic
#else
#define ASSERT_ACTION	printk
#endif

#define ASSERT(expression)						\
	if (!(expression)) {						\
	ASSERT_ACTION("assertion failed:(%s), file: %s, line: %d:%s\n",	\
			#expression, __FILE__, __LINE__, __func__);	\
	}
#else
#define ASSERT(expression)
#endif

struct mraid_pci_blk {
	caddr_t		vaddr;
	dma_addr_t	dma_addr;
};

#endif 

