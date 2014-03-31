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
 * FILE		: megaraid_mbox.h
 */

#ifndef _MEGARAID_H_
#define _MEGARAID_H_


#include "mega_common.h"
#include "mbox_defs.h"
#include "megaraid_ioctl.h"


#define MEGARAID_VERSION	"2.20.5.1"
#define MEGARAID_EXT_VERSION	"(Release Date: Thu Nov 16 15:32:35 EST 2006)"


#define PCI_DEVICE_ID_PERC4_DI_DISCOVERY		0x000E
#define PCI_SUBSYS_ID_PERC4_DI_DISCOVERY		0x0123

#define PCI_DEVICE_ID_PERC4_SC				0x1960
#define PCI_SUBSYS_ID_PERC4_SC				0x0520

#define PCI_DEVICE_ID_PERC4_DC				0x1960
#define PCI_SUBSYS_ID_PERC4_DC				0x0518

#define PCI_DEVICE_ID_VERDE				0x0407

#define PCI_DEVICE_ID_PERC4_DI_EVERGLADES		0x000F
#define PCI_SUBSYS_ID_PERC4_DI_EVERGLADES		0x014A

#define PCI_DEVICE_ID_PERC4E_SI_BIGBEND			0x0013
#define PCI_SUBSYS_ID_PERC4E_SI_BIGBEND			0x016c

#define PCI_DEVICE_ID_PERC4E_DI_KOBUK			0x0013
#define PCI_SUBSYS_ID_PERC4E_DI_KOBUK			0x016d

#define PCI_DEVICE_ID_PERC4E_DI_CORVETTE		0x0013
#define PCI_SUBSYS_ID_PERC4E_DI_CORVETTE		0x016e

#define PCI_DEVICE_ID_PERC4E_DI_EXPEDITION		0x0013
#define PCI_SUBSYS_ID_PERC4E_DI_EXPEDITION		0x016f

#define PCI_DEVICE_ID_PERC4E_DI_GUADALUPE		0x0013
#define PCI_SUBSYS_ID_PERC4E_DI_GUADALUPE		0x0170

#define PCI_DEVICE_ID_DOBSON				0x0408

#define PCI_DEVICE_ID_MEGARAID_SCSI_320_0		0x1960
#define PCI_SUBSYS_ID_MEGARAID_SCSI_320_0		0xA520

#define PCI_DEVICE_ID_MEGARAID_SCSI_320_1		0x1960
#define PCI_SUBSYS_ID_MEGARAID_SCSI_320_1		0x0520

#define PCI_DEVICE_ID_MEGARAID_SCSI_320_2		0x1960
#define PCI_SUBSYS_ID_MEGARAID_SCSI_320_2		0x0518

#define PCI_DEVICE_ID_MEGARAID_I4_133_RAID		0x1960
#define PCI_SUBSYS_ID_MEGARAID_I4_133_RAID		0x0522

#define PCI_DEVICE_ID_MEGARAID_SATA_150_4		0x1960
#define PCI_SUBSYS_ID_MEGARAID_SATA_150_4		0x4523

#define PCI_DEVICE_ID_MEGARAID_SATA_150_6		0x1960
#define PCI_SUBSYS_ID_MEGARAID_SATA_150_6		0x0523

#define PCI_DEVICE_ID_LINDSAY				0x0409

#define PCI_DEVICE_ID_INTEL_RAID_SRCS16			0x1960
#define PCI_SUBSYS_ID_INTEL_RAID_SRCS16			0x0523

#define PCI_DEVICE_ID_INTEL_RAID_SRCU41L_LAKE_SHETEK	0x1960
#define PCI_SUBSYS_ID_INTEL_RAID_SRCU41L_LAKE_SHETEK	0x0520

#define PCI_SUBSYS_ID_PERC3_QC				0x0471
#define PCI_SUBSYS_ID_PERC3_DC				0x0493
#define PCI_SUBSYS_ID_PERC3_SC				0x0475
#define PCI_SUBSYS_ID_CERC_ATA100_4CH			0x0511


#define MBOX_MAX_SCSI_CMDS	128	
#define MBOX_MAX_USER_CMDS	32	
#define MBOX_DEF_CMD_PER_LUN	64	
#define MBOX_DEFAULT_SG_SIZE	26	
#define MBOX_MAX_SG_SIZE	32	
#define MBOX_MAX_SECTORS	128	
#define MBOX_TIMEOUT		30	
#define MBOX_BUSY_WAIT		10	
#define MBOX_RESET_WAIT		180	
#define MBOX_RESET_EXT_WAIT	120	
#define MBOX_SYNC_WAIT_CNT	0xFFFF	

#define MBOX_SYNC_DELAY_200	200	

#define MBOX_IBUF_SIZE		4096


typedef struct {
	uint8_t			*raw_mbox;
	mbox_t			*mbox;
	mbox64_t		*mbox64;
	dma_addr_t		mbox_dma_h;
	mbox_sgl64		*sgl64;
	mbox_sgl32		*sgl32;
	dma_addr_t		sgl_dma_h;
	mraid_passthru_t	*pthru;
	dma_addr_t		pthru_dma_h;
	mraid_epassthru_t	*epthru;
	dma_addr_t		epthru_dma_h;
	dma_addr_t		buf_dma_h;
} mbox_ccb_t;


#define MAX_LD_EXTENDED64	64
typedef struct {
	mbox64_t			*una_mbox64;
	dma_addr_t			una_mbox64_dma;
	mbox_t				*mbox;
	mbox64_t			*mbox64;
	dma_addr_t			mbox_dma;
	spinlock_t			mailbox_lock;
	unsigned long			baseport;
	void __iomem *			baseaddr;
	struct mraid_pci_blk		mbox_pool[MBOX_MAX_SCSI_CMDS];
	struct dma_pool			*mbox_pool_handle;
	struct mraid_pci_blk		epthru_pool[MBOX_MAX_SCSI_CMDS];
	struct dma_pool			*epthru_pool_handle;
	struct mraid_pci_blk		sg_pool[MBOX_MAX_SCSI_CMDS];
	struct dma_pool			*sg_pool_handle;
	mbox_ccb_t			ccb_list[MBOX_MAX_SCSI_CMDS];
	mbox_ccb_t			uccb_list[MBOX_MAX_USER_CMDS];
	mbox64_t			umbox64[MBOX_MAX_USER_CMDS];

	uint8_t				pdrv_state[MBOX_MAX_PHYSICAL_DRIVES];
	uint32_t			last_disp;
	int				hw_error;
	int				fast_load;
	uint8_t				channel_class;
	struct mutex			sysfs_mtx;
	uioc_t				*sysfs_uioc;
	mbox64_t			*sysfs_mbox64;
	caddr_t				sysfs_buffer;
	dma_addr_t			sysfs_buffer_dma;
	wait_queue_head_t		sysfs_wait_q;
	int				random_del_supported;
	uint16_t			curr_ldmap[MAX_LD_EXTENDED64];
} mraid_device_t;

#define ADAP2RAIDDEV(adp)	((mraid_device_t *)((adp)->raid_device))

#define MAILBOX_LOCK(rdev)	(&(rdev)->mailbox_lock)

#define IS_RAID_CH(rdev, ch)	(((rdev)->channel_class >> (ch)) & 0x01)


#define RDINDOOR(rdev)		readl((rdev)->baseaddr + 0x20)
#define RDOUTDOOR(rdev)		readl((rdev)->baseaddr + 0x2C)
#define WRINDOOR(rdev, value)	writel(value, (rdev)->baseaddr + 0x20)
#define WROUTDOOR(rdev, value)	writel(value, (rdev)->baseaddr + 0x2C)

#endif 

