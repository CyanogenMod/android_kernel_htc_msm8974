/*
 *  Copyright 2003-2005 Red Hat, Inc.  All rights reserved.
 *  Copyright 2003-2005 Jeff Garzik
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *  libata documentation is available via 'make {ps|pdf}docs',
 *  as Documentation/DocBook/libata.*
 *
 */

#ifndef __LINUX_LIBATA_H__
#define __LINUX_LIBATA_H__

#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/io.h>
#include <linux/ata.h>
#include <linux/workqueue.h>
#include <scsi/scsi_host.h>
#include <linux/acpi.h>
#include <linux/cdrom.h>
#include <linux/sched.h>

#ifdef CONFIG_ATA_NONSTANDARD
#include <asm/libata-portmap.h>
#else
#include <asm-generic/libata-portmap.h>
#endif

#undef ATA_DEBUG		
#undef ATA_VERBOSE_DEBUG	
#undef ATA_IRQ_TRAP		
#undef ATA_NDEBUG		


#ifdef ATA_DEBUG
#define DPRINTK(fmt, args...) printk(KERN_ERR "%s: " fmt, __func__, ## args)
#ifdef ATA_VERBOSE_DEBUG
#define VPRINTK(fmt, args...) printk(KERN_ERR "%s: " fmt, __func__, ## args)
#else
#define VPRINTK(fmt, args...)
#endif	
#else
#define DPRINTK(fmt, args...)
#define VPRINTK(fmt, args...)
#endif	

#define BPRINTK(fmt, args...) if (ap->flags & ATA_FLAG_DEBUGMSG) printk(KERN_ERR "%s: " fmt, __func__, ## args)

#define ata_print_version_once(dev, version)			\
({								\
	static bool __print_once;				\
								\
	if (!__print_once) {					\
		__print_once = true;				\
		ata_print_version(dev, version);		\
	}							\
})

#define HAVE_LIBATA_MSG 1

enum {
	ATA_MSG_DRV	= 0x0001,
	ATA_MSG_INFO	= 0x0002,
	ATA_MSG_PROBE	= 0x0004,
	ATA_MSG_WARN	= 0x0008,
	ATA_MSG_MALLOC	= 0x0010,
	ATA_MSG_CTL	= 0x0020,
	ATA_MSG_INTR	= 0x0040,
	ATA_MSG_ERR	= 0x0080,
};

#define ata_msg_drv(p)    ((p)->msg_enable & ATA_MSG_DRV)
#define ata_msg_info(p)   ((p)->msg_enable & ATA_MSG_INFO)
#define ata_msg_probe(p)  ((p)->msg_enable & ATA_MSG_PROBE)
#define ata_msg_warn(p)   ((p)->msg_enable & ATA_MSG_WARN)
#define ata_msg_malloc(p) ((p)->msg_enable & ATA_MSG_MALLOC)
#define ata_msg_ctl(p)    ((p)->msg_enable & ATA_MSG_CTL)
#define ata_msg_intr(p)   ((p)->msg_enable & ATA_MSG_INTR)
#define ata_msg_err(p)    ((p)->msg_enable & ATA_MSG_ERR)

static inline u32 ata_msg_init(int dval, int default_msg_enable_bits)
{
	if (dval < 0 || dval >= (sizeof(u32) * 8))
		return default_msg_enable_bits; 
	if (!dval)
		return 0;
	return (1 << dval) - 1;
}

#define ATA_TAG_POISON		0xfafbfcfdU

enum {
	
	LIBATA_MAX_PRD		= ATA_MAX_PRD / 2,
	LIBATA_DUMB_MAX_PRD	= ATA_MAX_PRD / 4,	
	ATA_DEF_QUEUE		= 1,
	
	ATA_MAX_QUEUE		= 32,
	ATA_TAG_INTERNAL	= ATA_MAX_QUEUE - 1,
	ATA_SHORT_PAUSE		= 16,

	ATAPI_MAX_DRAIN		= 16 << 10,

	ATA_ALL_DEVICES		= (1 << ATA_MAX_DEVICES) - 1,

	ATA_SHT_EMULATED	= 1,
	ATA_SHT_CMD_PER_LUN	= 1,
	ATA_SHT_THIS_ID		= -1,
	ATA_SHT_USE_CLUSTERING	= 1,

	
	ATA_DFLAG_LBA		= (1 << 0), 
	ATA_DFLAG_LBA48		= (1 << 1), 
	ATA_DFLAG_CDB_INTR	= (1 << 2), 
	ATA_DFLAG_NCQ		= (1 << 3), 
	ATA_DFLAG_FLUSH_EXT	= (1 << 4), 
	ATA_DFLAG_ACPI_PENDING	= (1 << 5), 
	ATA_DFLAG_ACPI_FAILED	= (1 << 6), 
	ATA_DFLAG_AN		= (1 << 7), 
	ATA_DFLAG_DMADIR	= (1 << 10), 
	ATA_DFLAG_CFG_MASK	= (1 << 12) - 1,

	ATA_DFLAG_PIO		= (1 << 12), 
	ATA_DFLAG_NCQ_OFF	= (1 << 13), 
	ATA_DFLAG_SLEEPING	= (1 << 15), 
	ATA_DFLAG_DUBIOUS_XFER	= (1 << 16), 
	ATA_DFLAG_NO_UNLOAD	= (1 << 17), 
	ATA_DFLAG_UNLOCK_HPA	= (1 << 18), 
	ATA_DFLAG_INIT_MASK	= (1 << 24) - 1,

	ATA_DFLAG_DETACH	= (1 << 24),
	ATA_DFLAG_DETACHED	= (1 << 25),

	ATA_DEV_UNKNOWN		= 0,	
	ATA_DEV_ATA		= 1,	
	ATA_DEV_ATA_UNSUP	= 2,	
	ATA_DEV_ATAPI		= 3,	
	ATA_DEV_ATAPI_UNSUP	= 4,	
	ATA_DEV_PMP		= 5,	
	ATA_DEV_PMP_UNSUP	= 6,	
	ATA_DEV_SEMB		= 7,	
	ATA_DEV_SEMB_UNSUP	= 8,	
	ATA_DEV_NONE		= 9,	

	
	ATA_LFLAG_NO_HRST	= (1 << 1), 
	ATA_LFLAG_NO_SRST	= (1 << 2), 
	ATA_LFLAG_ASSUME_ATA	= (1 << 3), 
	ATA_LFLAG_ASSUME_SEMB	= (1 << 4), 
	ATA_LFLAG_ASSUME_CLASS	= ATA_LFLAG_ASSUME_ATA | ATA_LFLAG_ASSUME_SEMB,
	ATA_LFLAG_NO_RETRY	= (1 << 5), 
	ATA_LFLAG_DISABLED	= (1 << 6), 
	ATA_LFLAG_SW_ACTIVITY	= (1 << 7), 
	ATA_LFLAG_NO_LPM	= (1 << 8), 

	
	ATA_FLAG_SLAVE_POSS	= (1 << 0), 
					    
	ATA_FLAG_SATA		= (1 << 1),
	ATA_FLAG_NO_ATAPI	= (1 << 6), 
	ATA_FLAG_PIO_DMA	= (1 << 7), 
	ATA_FLAG_PIO_LBA48	= (1 << 8), 
	ATA_FLAG_PIO_POLLING	= (1 << 9), 
	ATA_FLAG_NCQ		= (1 << 10), 
	ATA_FLAG_NO_POWEROFF_SPINDOWN = (1 << 11), 
	ATA_FLAG_NO_HIBERNATE_SPINDOWN = (1 << 12), 
	ATA_FLAG_DEBUGMSG	= (1 << 13),
	ATA_FLAG_FPDMA_AA		= (1 << 14), 
	ATA_FLAG_IGN_SIMPLEX	= (1 << 15), 
	ATA_FLAG_NO_IORDY	= (1 << 16), 
	ATA_FLAG_ACPI_SATA	= (1 << 17), 
	ATA_FLAG_AN		= (1 << 18), 
	ATA_FLAG_PMP		= (1 << 19), 
	ATA_FLAG_EM		= (1 << 21), 
	ATA_FLAG_SW_ACTIVITY	= (1 << 22), 
	ATA_FLAG_NO_DIPM	= (1 << 23), 

	


	
	ATA_PFLAG_EH_PENDING	= (1 << 0), 
	ATA_PFLAG_EH_IN_PROGRESS = (1 << 1), 
	ATA_PFLAG_FROZEN	= (1 << 2), 
	ATA_PFLAG_RECOVERED	= (1 << 3), 
	ATA_PFLAG_LOADING	= (1 << 4), 
	ATA_PFLAG_SCSI_HOTPLUG	= (1 << 6), 
	ATA_PFLAG_INITIALIZING	= (1 << 7), 
	ATA_PFLAG_RESETTING	= (1 << 8), 
	ATA_PFLAG_UNLOADING	= (1 << 9), 
	ATA_PFLAG_UNLOADED	= (1 << 10), 

	ATA_PFLAG_SUSPENDED	= (1 << 17), 
	ATA_PFLAG_PM_PENDING	= (1 << 18), 
	ATA_PFLAG_INIT_GTM_VALID = (1 << 19), 

	ATA_PFLAG_PIO32		= (1 << 20),  
	ATA_PFLAG_PIO32CHANGE	= (1 << 21),  

	
	ATA_QCFLAG_ACTIVE	= (1 << 0), 
	ATA_QCFLAG_DMAMAP	= (1 << 1), 
	ATA_QCFLAG_IO		= (1 << 3), 
	ATA_QCFLAG_RESULT_TF	= (1 << 4), 
	ATA_QCFLAG_CLEAR_EXCL	= (1 << 5), 
	ATA_QCFLAG_QUIET	= (1 << 6), 
	ATA_QCFLAG_RETRY	= (1 << 7), 

	ATA_QCFLAG_FAILED	= (1 << 16), 
	ATA_QCFLAG_SENSE_VALID	= (1 << 17), 
	ATA_QCFLAG_EH_SCHEDULED = (1 << 18), 

	
	ATA_HOST_SIMPLEX	= (1 << 0),	
	ATA_HOST_STARTED	= (1 << 1),	
	ATA_HOST_PARALLEL_SCAN	= (1 << 2),	

	

	
	ATA_TMOUT_BOOT		= 30000,	
	ATA_TMOUT_BOOT_QUICK	=  7000,	
	ATA_TMOUT_INTERNAL_QUICK = 5000,
	ATA_TMOUT_MAX_PARK	= 30000,

	ATA_TMOUT_FF_WAIT_LONG	=  2000,
	ATA_TMOUT_FF_WAIT	=   800,

	/* Spec mandates to wait for ">= 2ms" before checking status
	 * after reset.  We wait 150ms, because that was the magic
	 * delay used for ATAPI devices in Hale Landis's ATADRVR, for
	 * the period of time between when the ATA command register is
	 * written, and then status is checked.  Because waiting for
	 * "a while" before checking status is fine, post SRST, we
	 * perform this magic delay here as well.
	 *
	 * Old drivers/ide uses the 2mS rule and then waits for ready.
	 */
	ATA_WAIT_AFTER_RESET	=  150,

	ATA_TMOUT_PMP_SRST_WAIT	= 5000,

	
	BUS_UNKNOWN		= 0,
	BUS_DMA			= 1,
	BUS_IDLE		= 2,
	BUS_NOINTR		= 3,
	BUS_NODATA		= 4,
	BUS_TIMER		= 5,
	BUS_PIO			= 6,
	BUS_EDD			= 7,
	BUS_IDENTIFY		= 8,
	BUS_PACKET		= 9,

	
	PORT_UNKNOWN		= 0,
	PORT_ENABLED		= 1,
	PORT_DISABLED		= 2,

	ATA_NR_PIO_MODES	= 7,
	ATA_NR_MWDMA_MODES	= 5,
	ATA_NR_UDMA_MODES	= 8,

	ATA_SHIFT_PIO		= 0,
	ATA_SHIFT_MWDMA		= ATA_SHIFT_PIO + ATA_NR_PIO_MODES,
	ATA_SHIFT_UDMA		= ATA_SHIFT_MWDMA + ATA_NR_MWDMA_MODES,

	
	ATA_DMA_PAD_SZ		= 4,

	
	ATA_ERING_SIZE		= 32,

	
	ATA_DEFER_LINK		= 1,
	ATA_DEFER_PORT		= 2,

	
	ATA_EH_DESC_LEN		= 80,

	
	ATA_EH_REVALIDATE	= (1 << 0),
	ATA_EH_SOFTRESET	= (1 << 1), 
	ATA_EH_HARDRESET	= (1 << 2), 
	ATA_EH_RESET		= ATA_EH_SOFTRESET | ATA_EH_HARDRESET,
	ATA_EH_ENABLE_LINK	= (1 << 3),
	ATA_EH_PARK		= (1 << 5), 

	ATA_EH_PERDEV_MASK	= ATA_EH_REVALIDATE | ATA_EH_PARK,
	ATA_EH_ALL_ACTIONS	= ATA_EH_REVALIDATE | ATA_EH_RESET |
				  ATA_EH_ENABLE_LINK,

	
	ATA_EHI_HOTPLUGGED	= (1 << 0),  
	ATA_EHI_NO_AUTOPSY	= (1 << 2),  
	ATA_EHI_QUIET		= (1 << 3),  
	ATA_EHI_NO_RECOVERY	= (1 << 4),  

	ATA_EHI_DID_SOFTRESET	= (1 << 16), 
	ATA_EHI_DID_HARDRESET	= (1 << 17), 
	ATA_EHI_PRINTINFO	= (1 << 18), 
	ATA_EHI_SETMODE		= (1 << 19), 
	ATA_EHI_POST_SETMODE	= (1 << 20), 

	ATA_EHI_DID_RESET	= ATA_EHI_DID_SOFTRESET | ATA_EHI_DID_HARDRESET,

	
	ATA_EHI_TO_SLAVE_MASK	= ATA_EHI_NO_AUTOPSY | ATA_EHI_QUIET,

	
	ATA_EH_MAX_TRIES	= 5,

	
	ATA_LINK_RESUME_TRIES	= 5,

	
	ATA_PROBE_MAX_TRIES	= 3,
	ATA_EH_DEV_TRIES	= 3,
	ATA_EH_PMP_TRIES	= 5,
	ATA_EH_PMP_LINK_TRIES	= 3,

	SATA_PMP_RW_TIMEOUT	= 3000,		

	ATA_EH_CMD_TIMEOUT_TABLE_SIZE = 6,


	ATA_HORKAGE_DIAGNOSTIC	= (1 << 0),	
	ATA_HORKAGE_NODMA	= (1 << 1),	
	ATA_HORKAGE_NONCQ	= (1 << 2),	
	ATA_HORKAGE_MAX_SEC_128	= (1 << 3),	
	ATA_HORKAGE_BROKEN_HPA	= (1 << 4),	
	ATA_HORKAGE_DISABLE	= (1 << 5),	
	ATA_HORKAGE_HPA_SIZE	= (1 << 6),	
	ATA_HORKAGE_IVB		= (1 << 8),	
	ATA_HORKAGE_STUCK_ERR	= (1 << 9),	
	ATA_HORKAGE_BRIDGE_OK	= (1 << 10),	
	ATA_HORKAGE_ATAPI_MOD16_DMA = (1 << 11), 
	ATA_HORKAGE_FIRMWARE_WARN = (1 << 12),	
	ATA_HORKAGE_1_5_GBPS	= (1 << 13),	
	ATA_HORKAGE_NOSETXFER	= (1 << 14),	
	ATA_HORKAGE_BROKEN_FPDMA_AA	= (1 << 15),	
	ATA_HORKAGE_DUMP_ID	= (1 << 16),	

	ATA_DMA_MASK_ATA	= (1 << 0),	
	ATA_DMA_MASK_ATAPI	= (1 << 1),	
	ATA_DMA_MASK_CFA	= (1 << 2),	

	
	ATAPI_READ		= 0,		
	ATAPI_WRITE		= 1,		
	ATAPI_READ_CD		= 2,		
	ATAPI_PASS_THRU		= 3,		
	ATAPI_MISC		= 4,		

	
	ATA_TIMING_SETUP	= (1 << 0),
	ATA_TIMING_ACT8B	= (1 << 1),
	ATA_TIMING_REC8B	= (1 << 2),
	ATA_TIMING_CYC8B	= (1 << 3),
	ATA_TIMING_8BIT		= ATA_TIMING_ACT8B | ATA_TIMING_REC8B |
				  ATA_TIMING_CYC8B,
	ATA_TIMING_ACTIVE	= (1 << 4),
	ATA_TIMING_RECOVER	= (1 << 5),
	ATA_TIMING_DMACK_HOLD	= (1 << 6),
	ATA_TIMING_CYCLE	= (1 << 7),
	ATA_TIMING_UDMA		= (1 << 8),
	ATA_TIMING_ALL		= ATA_TIMING_SETUP | ATA_TIMING_ACT8B |
				  ATA_TIMING_REC8B | ATA_TIMING_CYC8B |
				  ATA_TIMING_ACTIVE | ATA_TIMING_RECOVER |
				  ATA_TIMING_DMACK_HOLD | ATA_TIMING_CYCLE |
				  ATA_TIMING_UDMA,

	
	ATA_ACPI_FILTER_SETXFER	= 1 << 0,
	ATA_ACPI_FILTER_LOCK	= 1 << 1,
	ATA_ACPI_FILTER_DIPM	= 1 << 2,
	ATA_ACPI_FILTER_FPDMA_OFFSET = 1 << 3,	
	ATA_ACPI_FILTER_FPDMA_AA = 1 << 4,	

	ATA_ACPI_FILTER_DEFAULT	= ATA_ACPI_FILTER_SETXFER |
				  ATA_ACPI_FILTER_LOCK |
				  ATA_ACPI_FILTER_DIPM,
};

enum ata_xfer_mask {
	ATA_MASK_PIO		= ((1LU << ATA_NR_PIO_MODES) - 1)
					<< ATA_SHIFT_PIO,
	ATA_MASK_MWDMA		= ((1LU << ATA_NR_MWDMA_MODES) - 1)
					<< ATA_SHIFT_MWDMA,
	ATA_MASK_UDMA		= ((1LU << ATA_NR_UDMA_MODES) - 1)
					<< ATA_SHIFT_UDMA,
};

enum hsm_task_states {
	HSM_ST_IDLE,		
	HSM_ST_FIRST,		
	HSM_ST,			
	HSM_ST_LAST,		
	HSM_ST_ERR,		
};

enum ata_completion_errors {
	AC_ERR_DEV		= (1 << 0), 
	AC_ERR_HSM		= (1 << 1), 
	AC_ERR_TIMEOUT		= (1 << 2), 
	AC_ERR_MEDIA		= (1 << 3), 
	AC_ERR_ATA_BUS		= (1 << 4), 
	AC_ERR_HOST_BUS		= (1 << 5), 
	AC_ERR_SYSTEM		= (1 << 6), 
	AC_ERR_INVALID		= (1 << 7), 
	AC_ERR_OTHER		= (1 << 8), 
	AC_ERR_NODEV_HINT	= (1 << 9), 
	AC_ERR_NCQ		= (1 << 10), 
};

enum ata_lpm_policy {
	ATA_LPM_UNKNOWN,
	ATA_LPM_MAX_POWER,
	ATA_LPM_MED_POWER,
	ATA_LPM_MIN_POWER,
};

enum ata_lpm_hints {
	ATA_LPM_EMPTY		= (1 << 0), 
	ATA_LPM_HIPM		= (1 << 1), 
};

struct scsi_device;
struct ata_port_operations;
struct ata_port;
struct ata_link;
struct ata_queued_cmd;

typedef void (*ata_qc_cb_t) (struct ata_queued_cmd *qc);
typedef int (*ata_prereset_fn_t)(struct ata_link *link, unsigned long deadline);
typedef int (*ata_reset_fn_t)(struct ata_link *link, unsigned int *classes,
			      unsigned long deadline);
typedef void (*ata_postreset_fn_t)(struct ata_link *link, unsigned int *classes);

extern struct device_attribute dev_attr_link_power_management_policy;
extern struct device_attribute dev_attr_unload_heads;
extern struct device_attribute dev_attr_em_message_type;
extern struct device_attribute dev_attr_em_message;
extern struct device_attribute dev_attr_sw_activity;

enum sw_activity {
	OFF,
	BLINK_ON,
	BLINK_OFF,
};

#ifdef CONFIG_ATA_SFF
struct ata_ioports {
	void __iomem		*cmd_addr;
	void __iomem		*data_addr;
	void __iomem		*error_addr;
	void __iomem		*feature_addr;
	void __iomem		*nsect_addr;
	void __iomem		*lbal_addr;
	void __iomem		*lbam_addr;
	void __iomem		*lbah_addr;
	void __iomem		*device_addr;
	void __iomem		*status_addr;
	void __iomem		*command_addr;
	void __iomem		*altstatus_addr;
	void __iomem		*ctl_addr;
#ifdef CONFIG_ATA_BMDMA
	void __iomem		*bmdma_addr;
#endif 
	void __iomem		*scr_addr;
};
#endif 

struct ata_host {
	spinlock_t		lock;
	struct device 		*dev;
	void __iomem * const	*iomap;
	unsigned int		n_ports;
	void			*private_data;
	struct ata_port_operations *ops;
	unsigned long		flags;

	struct mutex		eh_mutex;
	struct task_struct	*eh_owner;

#ifdef CONFIG_ATA_ACPI
	acpi_handle		acpi_handle;
#endif
	struct ata_port		*simplex_claimed;	
	struct ata_port		*ports[0];
};

struct ata_queued_cmd {
	struct ata_port		*ap;
	struct ata_device	*dev;

	struct scsi_cmnd	*scsicmd;
	void			(*scsidone)(struct scsi_cmnd *);

	struct ata_taskfile	tf;
	u8			cdb[ATAPI_CDB_LEN];

	unsigned long		flags;		
	unsigned int		tag;
	unsigned int		n_elem;
	unsigned int		orig_n_elem;

	int			dma_dir;

	unsigned int		sect_size;

	unsigned int		nbytes;
	unsigned int		extrabytes;
	unsigned int		curbytes;

	struct scatterlist	sgent;

	struct scatterlist	*sg;

	struct scatterlist	*cursg;
	unsigned int		cursg_ofs;

	unsigned int		err_mask;
	struct ata_taskfile	result_tf;
	ata_qc_cb_t		complete_fn;

	void			*private_data;
	void			*lldd_task;
};

struct ata_port_stats {
	unsigned long		unhandled_irq;
	unsigned long		idle_irq;
	unsigned long		rw_reqbuf;
};

struct ata_ering_entry {
	unsigned int		eflags;
	unsigned int		err_mask;
	u64			timestamp;
};

struct ata_ering {
	int			cursor;
	struct ata_ering_entry	ring[ATA_ERING_SIZE];
};

struct ata_device {
	struct ata_link		*link;
	unsigned int		devno;		
	unsigned int		horkage;	
	unsigned long		flags;		
	struct scsi_device	*sdev;		
	void			*private_data;
#ifdef CONFIG_ATA_ACPI
	acpi_handle		acpi_handle;
	union acpi_object	*gtf_cache;
	unsigned int		gtf_filter;
#endif
	struct device		tdev;
	
	u64			n_sectors;	
	u64			n_native_sectors; 
	unsigned int		class;		
	unsigned long		unpark_deadline;

	u8			pio_mode;
	u8			dma_mode;
	u8			xfer_mode;
	unsigned int		xfer_shift;	

	unsigned int		multi_count;	
	unsigned int		max_sectors;	
	unsigned int		cdb_len;

	
	unsigned long		pio_mask;
	unsigned long		mwdma_mask;
	unsigned long		udma_mask;

	
	u16			cylinders;	
	u16			heads;		
	u16			sectors;	

	union {
		u16		id[ATA_ID_WORDS]; 
		u32		gscr[SATA_PMP_GSCR_DWORDS]; 
	};

	
	int			spdn_cnt;
	
	struct ata_ering	ering;
};

#define ATA_DEVICE_CLEAR_BEGIN		offsetof(struct ata_device, n_sectors)
#define ATA_DEVICE_CLEAR_END		offsetof(struct ata_device, ering)

struct ata_eh_info {
	struct ata_device	*dev;		
	u32			serror;		
	unsigned int		err_mask;	
	unsigned int		action;		
	unsigned int		dev_action[ATA_MAX_DEVICES]; 
	unsigned int		flags;		

	unsigned int		probe_mask;

	char			desc[ATA_EH_DESC_LEN];
	int			desc_len;
};

struct ata_eh_context {
	struct ata_eh_info	i;
	int			tries[ATA_MAX_DEVICES];
	int			cmd_timeout_idx[ATA_MAX_DEVICES]
					       [ATA_EH_CMD_TIMEOUT_TABLE_SIZE];
	unsigned int		classes[ATA_MAX_DEVICES];
	unsigned int		did_probe_mask;
	unsigned int		unloaded_mask;
	unsigned int		saved_ncq_enabled;
	u8			saved_xfer_mode[ATA_MAX_DEVICES];
	
	unsigned long		last_reset;
};

struct ata_acpi_drive
{
	u32 pio;
	u32 dma;
} __packed;

struct ata_acpi_gtm {
	struct ata_acpi_drive drive[2];
	u32 flags;
} __packed;

struct ata_link {
	struct ata_port		*ap;
	int			pmp;		

	struct device		tdev;
	unsigned int		active_tag;	
	u32			sactive;	

	unsigned int		flags;		

	u32			saved_scontrol;	
	unsigned int		hw_sata_spd_limit;
	unsigned int		sata_spd_limit;
	unsigned int		sata_spd;	
	enum ata_lpm_policy	lpm_policy;

	
	struct ata_eh_info	eh_info;
	
	struct ata_eh_context	eh_context;

	struct ata_device	device[ATA_MAX_DEVICES];
};
#define ATA_LINK_CLEAR_BEGIN		offsetof(struct ata_link, active_tag)
#define ATA_LINK_CLEAR_END		offsetof(struct ata_link, device[0])

struct ata_port {
	struct Scsi_Host	*scsi_host; 
	struct ata_port_operations *ops;
	spinlock_t		*lock;
	unsigned long		flags;	
	
	unsigned int		pflags; 
	unsigned int		print_id; 
	unsigned int		port_no; 

#ifdef CONFIG_ATA_SFF
	struct ata_ioports	ioaddr;	
	u8			ctl;	
	u8			last_ctl;	/* Cache last written value */
	struct ata_link*	sff_pio_task_link; 
	struct delayed_work	sff_pio_task;
#ifdef CONFIG_ATA_BMDMA
	struct ata_bmdma_prd	*bmdma_prd;	
	dma_addr_t		bmdma_prd_dma;	
#endif 
#endif 

	unsigned int		pio_mask;
	unsigned int		mwdma_mask;
	unsigned int		udma_mask;
	unsigned int		cbl;	

	struct ata_queued_cmd	qcmd[ATA_MAX_QUEUE];
	unsigned long		qc_allocated;
	unsigned int		qc_active;
	int			nr_active_links; 

	struct ata_link		link;		
	struct ata_link		*slave_link;	

	int			nr_pmp_links;	
	struct ata_link		*pmp_link;	
	struct ata_link		*excl_link;	

	struct ata_port_stats	stats;
	struct ata_host		*host;
	struct device 		*dev;
	struct device		tdev;

	struct mutex		scsi_scan_mutex;
	struct delayed_work	hotplug_task;
	struct work_struct	scsi_rescan_task;

	unsigned int		hsm_task_state;

	u32			msg_enable;
	struct list_head	eh_done_q;
	wait_queue_head_t	eh_wait_q;
	int			eh_tries;
	struct completion	park_req_pending;

	pm_message_t		pm_mesg;
	int			*pm_result;
	enum ata_lpm_policy	target_lpm_policy;

	struct timer_list	fastdrain_timer;
	unsigned long		fastdrain_cnt;

	int			em_message_type;
	void			*private_data;

#ifdef CONFIG_ATA_ACPI
	acpi_handle		acpi_handle;
	struct ata_acpi_gtm	__acpi_init_gtm; 
#endif
	
	u8			sector_buf[ATA_SECT_SIZE] ____cacheline_aligned;
};

#define ATA_OP_NULL		(void *)(unsigned long)(-ENOENT)

struct ata_port_operations {
	int  (*qc_defer)(struct ata_queued_cmd *qc);
	int  (*check_atapi_dma)(struct ata_queued_cmd *qc);
	void (*qc_prep)(struct ata_queued_cmd *qc);
	unsigned int (*qc_issue)(struct ata_queued_cmd *qc);
	bool (*qc_fill_rtf)(struct ata_queued_cmd *qc);

	int  (*cable_detect)(struct ata_port *ap);
	unsigned long (*mode_filter)(struct ata_device *dev, unsigned long xfer_mask);
	void (*set_piomode)(struct ata_port *ap, struct ata_device *dev);
	void (*set_dmamode)(struct ata_port *ap, struct ata_device *dev);
	int  (*set_mode)(struct ata_link *link, struct ata_device **r_failed_dev);
	unsigned int (*read_id)(struct ata_device *dev, struct ata_taskfile *tf, u16 *id);

	void (*dev_config)(struct ata_device *dev);

	void (*freeze)(struct ata_port *ap);
	void (*thaw)(struct ata_port *ap);
	ata_prereset_fn_t	prereset;
	ata_reset_fn_t		softreset;
	ata_reset_fn_t		hardreset;
	ata_postreset_fn_t	postreset;
	ata_prereset_fn_t	pmp_prereset;
	ata_reset_fn_t		pmp_softreset;
	ata_reset_fn_t		pmp_hardreset;
	ata_postreset_fn_t	pmp_postreset;
	void (*error_handler)(struct ata_port *ap);
	void (*lost_interrupt)(struct ata_port *ap);
	void (*post_internal_cmd)(struct ata_queued_cmd *qc);

	int  (*scr_read)(struct ata_link *link, unsigned int sc_reg, u32 *val);
	int  (*scr_write)(struct ata_link *link, unsigned int sc_reg, u32 val);
	void (*pmp_attach)(struct ata_port *ap);
	void (*pmp_detach)(struct ata_port *ap);
	int  (*set_lpm)(struct ata_link *link, enum ata_lpm_policy policy,
			unsigned hints);

	int  (*port_suspend)(struct ata_port *ap, pm_message_t mesg);
	int  (*port_resume)(struct ata_port *ap);
	int  (*port_start)(struct ata_port *ap);
	void (*port_stop)(struct ata_port *ap);
	void (*host_stop)(struct ata_host *host);

#ifdef CONFIG_ATA_SFF
	void (*sff_dev_select)(struct ata_port *ap, unsigned int device);
	void (*sff_set_devctl)(struct ata_port *ap, u8 ctl);
	u8   (*sff_check_status)(struct ata_port *ap);
	u8   (*sff_check_altstatus)(struct ata_port *ap);
	void (*sff_tf_load)(struct ata_port *ap, const struct ata_taskfile *tf);
	void (*sff_tf_read)(struct ata_port *ap, struct ata_taskfile *tf);
	void (*sff_exec_command)(struct ata_port *ap,
				 const struct ata_taskfile *tf);
	unsigned int (*sff_data_xfer)(struct ata_device *dev,
			unsigned char *buf, unsigned int buflen, int rw);
	void (*sff_irq_on)(struct ata_port *);
	bool (*sff_irq_check)(struct ata_port *);
	void (*sff_irq_clear)(struct ata_port *);
	void (*sff_drain_fifo)(struct ata_queued_cmd *qc);

#ifdef CONFIG_ATA_BMDMA
	void (*bmdma_setup)(struct ata_queued_cmd *qc);
	void (*bmdma_start)(struct ata_queued_cmd *qc);
	void (*bmdma_stop)(struct ata_queued_cmd *qc);
	u8   (*bmdma_status)(struct ata_port *ap);
#endif 
#endif 

	ssize_t (*em_show)(struct ata_port *ap, char *buf);
	ssize_t (*em_store)(struct ata_port *ap, const char *message,
			    size_t size);
	ssize_t (*sw_activity_show)(struct ata_device *dev, char *buf);
	ssize_t (*sw_activity_store)(struct ata_device *dev,
				     enum sw_activity val);
	void (*phy_reset)(struct ata_port *ap);
	void (*eng_timeout)(struct ata_port *ap);

	const struct ata_port_operations	*inherits;
};

struct ata_port_info {
	unsigned long		flags;
	unsigned long		link_flags;
	unsigned long		pio_mask;
	unsigned long		mwdma_mask;
	unsigned long		udma_mask;
	struct ata_port_operations *port_ops;
	void 			*private_data;
};

struct ata_timing {
	unsigned short mode;		
	unsigned short setup;		
	unsigned short act8b;		
	unsigned short rec8b;		
	unsigned short cyc8b;		
	unsigned short active;		
	unsigned short recover;		
	unsigned short dmack_hold;	
	unsigned short cycle;		
	unsigned short udma;		
};

extern const unsigned long sata_deb_timing_normal[];
extern const unsigned long sata_deb_timing_hotplug[];
extern const unsigned long sata_deb_timing_long[];

extern struct ata_port_operations ata_dummy_port_ops;
extern const struct ata_port_info ata_dummy_port_info;

static inline const unsigned long *
sata_ehc_deb_timing(struct ata_eh_context *ehc)
{
	if (ehc->i.flags & ATA_EHI_HOTPLUGGED)
		return sata_deb_timing_hotplug;
	else
		return sata_deb_timing_normal;
}

static inline int ata_port_is_dummy(struct ata_port *ap)
{
	return ap->ops == &ata_dummy_port_ops;
}

extern int sata_set_spd(struct ata_link *link);
extern int ata_std_prereset(struct ata_link *link, unsigned long deadline);
extern int ata_wait_after_reset(struct ata_link *link, unsigned long deadline,
				int (*check_ready)(struct ata_link *link));
extern int sata_link_debounce(struct ata_link *link,
			const unsigned long *params, unsigned long deadline);
extern int sata_link_resume(struct ata_link *link, const unsigned long *params,
			    unsigned long deadline);
extern int sata_link_scr_lpm(struct ata_link *link, enum ata_lpm_policy policy,
			     bool spm_wakeup);
extern int sata_link_hardreset(struct ata_link *link,
			const unsigned long *timing, unsigned long deadline,
			bool *online, int (*check_ready)(struct ata_link *));
extern int sata_std_hardreset(struct ata_link *link, unsigned int *class,
			      unsigned long deadline);
extern void ata_std_postreset(struct ata_link *link, unsigned int *classes);

extern struct ata_host *ata_host_alloc(struct device *dev, int max_ports);
extern struct ata_host *ata_host_alloc_pinfo(struct device *dev,
			const struct ata_port_info * const * ppi, int n_ports);
extern int ata_slave_link_init(struct ata_port *ap);
extern int ata_host_start(struct ata_host *host);
extern int ata_host_register(struct ata_host *host,
			     struct scsi_host_template *sht);
extern int ata_host_activate(struct ata_host *host, int irq,
			     irq_handler_t irq_handler, unsigned long irq_flags,
			     struct scsi_host_template *sht);
extern void ata_host_detach(struct ata_host *host);
extern void ata_host_init(struct ata_host *, struct device *,
			  unsigned long, struct ata_port_operations *);
extern int ata_scsi_detect(struct scsi_host_template *sht);
extern int ata_scsi_ioctl(struct scsi_device *dev, int cmd, void __user *arg);
extern int ata_scsi_queuecmd(struct Scsi_Host *h, struct scsi_cmnd *cmd);
extern int ata_sas_scsi_ioctl(struct ata_port *ap, struct scsi_device *dev,
			    int cmd, void __user *arg);
extern void ata_sas_port_destroy(struct ata_port *);
extern struct ata_port *ata_sas_port_alloc(struct ata_host *,
					   struct ata_port_info *, struct Scsi_Host *);
extern void ata_sas_async_probe(struct ata_port *ap);
extern int ata_sas_sync_probe(struct ata_port *ap);
extern int ata_sas_port_init(struct ata_port *);
extern int ata_sas_port_start(struct ata_port *ap);
extern void ata_sas_port_stop(struct ata_port *ap);
extern int ata_sas_slave_configure(struct scsi_device *, struct ata_port *);
extern int ata_sas_queuecmd(struct scsi_cmnd *cmd, struct ata_port *ap);
extern int sata_scr_valid(struct ata_link *link);
extern int sata_scr_read(struct ata_link *link, int reg, u32 *val);
extern int sata_scr_write(struct ata_link *link, int reg, u32 val);
extern int sata_scr_write_flush(struct ata_link *link, int reg, u32 val);
extern bool ata_link_online(struct ata_link *link);
extern bool ata_link_offline(struct ata_link *link);
#ifdef CONFIG_PM
extern int ata_host_suspend(struct ata_host *host, pm_message_t mesg);
extern void ata_host_resume(struct ata_host *host);
#endif
extern int ata_ratelimit(void);
extern void ata_msleep(struct ata_port *ap, unsigned int msecs);
extern u32 ata_wait_register(struct ata_port *ap, void __iomem *reg, u32 mask,
			u32 val, unsigned long interval, unsigned long timeout);
extern int atapi_cmd_type(u8 opcode);
extern void ata_tf_to_fis(const struct ata_taskfile *tf,
			  u8 pmp, int is_cmd, u8 *fis);
extern void ata_tf_from_fis(const u8 *fis, struct ata_taskfile *tf);
extern unsigned long ata_pack_xfermask(unsigned long pio_mask,
			unsigned long mwdma_mask, unsigned long udma_mask);
extern void ata_unpack_xfermask(unsigned long xfer_mask,
			unsigned long *pio_mask, unsigned long *mwdma_mask,
			unsigned long *udma_mask);
extern u8 ata_xfer_mask2mode(unsigned long xfer_mask);
extern unsigned long ata_xfer_mode2mask(u8 xfer_mode);
extern int ata_xfer_mode2shift(unsigned long xfer_mode);
extern const char *ata_mode_string(unsigned long xfer_mask);
extern unsigned long ata_id_xfermask(const u16 *id);
extern int ata_std_qc_defer(struct ata_queued_cmd *qc);
extern void ata_noop_qc_prep(struct ata_queued_cmd *qc);
extern void ata_sg_init(struct ata_queued_cmd *qc, struct scatterlist *sg,
		 unsigned int n_elem);
extern unsigned int ata_dev_classify(const struct ata_taskfile *tf);
extern void ata_dev_disable(struct ata_device *adev);
extern void ata_id_string(const u16 *id, unsigned char *s,
			  unsigned int ofs, unsigned int len);
extern void ata_id_c_string(const u16 *id, unsigned char *s,
			    unsigned int ofs, unsigned int len);
extern unsigned int ata_do_dev_read_id(struct ata_device *dev,
					struct ata_taskfile *tf, u16 *id);
extern void ata_qc_complete(struct ata_queued_cmd *qc);
extern int ata_qc_complete_multiple(struct ata_port *ap, u32 qc_active);
extern void ata_scsi_simulate(struct ata_device *dev, struct scsi_cmnd *cmd);
extern int ata_std_bios_param(struct scsi_device *sdev,
			      struct block_device *bdev,
			      sector_t capacity, int geom[]);
extern void ata_scsi_unlock_native_capacity(struct scsi_device *sdev);
extern int ata_scsi_slave_config(struct scsi_device *sdev);
extern void ata_scsi_slave_destroy(struct scsi_device *sdev);
extern int ata_scsi_change_queue_depth(struct scsi_device *sdev,
				       int queue_depth, int reason);
extern int __ata_change_queue_depth(struct ata_port *ap, struct scsi_device *sdev,
				    int queue_depth, int reason);
extern struct ata_device *ata_dev_pair(struct ata_device *adev);
extern int ata_do_set_mode(struct ata_link *link, struct ata_device **r_failed_dev);
extern void ata_scsi_port_error_handler(struct Scsi_Host *host, struct ata_port *ap);
extern void ata_scsi_cmd_error_handler(struct Scsi_Host *host, struct ata_port *ap, struct list_head *eh_q);

extern int ata_cable_40wire(struct ata_port *ap);
extern int ata_cable_80wire(struct ata_port *ap);
extern int ata_cable_sata(struct ata_port *ap);
extern int ata_cable_ignore(struct ata_port *ap);
extern int ata_cable_unknown(struct ata_port *ap);

extern unsigned int ata_pio_need_iordy(const struct ata_device *);
extern const struct ata_timing *ata_timing_find_mode(u8 xfer_mode);
extern int ata_timing_compute(struct ata_device *, unsigned short,
			      struct ata_timing *, int, int);
extern void ata_timing_merge(const struct ata_timing *,
			     const struct ata_timing *, struct ata_timing *,
			     unsigned int);
extern u8 ata_timing_cycle2mode(unsigned int xfer_shift, int cycle);

#ifdef CONFIG_PCI
struct pci_dev;

struct pci_bits {
	unsigned int		reg;	
	unsigned int		width;	
	unsigned long		mask;
	unsigned long		val;
};

extern int pci_test_config_bits(struct pci_dev *pdev, const struct pci_bits *bits);
extern void ata_pci_remove_one(struct pci_dev *pdev);

#ifdef CONFIG_PM
extern void ata_pci_device_do_suspend(struct pci_dev *pdev, pm_message_t mesg);
extern int __must_check ata_pci_device_do_resume(struct pci_dev *pdev);
extern int ata_pci_device_suspend(struct pci_dev *pdev, pm_message_t mesg);
extern int ata_pci_device_resume(struct pci_dev *pdev);
#endif 
#endif 

#ifdef CONFIG_ATA_ACPI
static inline const struct ata_acpi_gtm *ata_acpi_init_gtm(struct ata_port *ap)
{
	if (ap->pflags & ATA_PFLAG_INIT_GTM_VALID)
		return &ap->__acpi_init_gtm;
	return NULL;
}
int ata_acpi_stm(struct ata_port *ap, const struct ata_acpi_gtm *stm);
int ata_acpi_gtm(struct ata_port *ap, struct ata_acpi_gtm *stm);
unsigned long ata_acpi_gtm_xfermask(struct ata_device *dev,
				    const struct ata_acpi_gtm *gtm);
int ata_acpi_cbl_80wire(struct ata_port *ap, const struct ata_acpi_gtm *gtm);
#else
static inline const struct ata_acpi_gtm *ata_acpi_init_gtm(struct ata_port *ap)
{
	return NULL;
}

static inline int ata_acpi_stm(const struct ata_port *ap,
			       struct ata_acpi_gtm *stm)
{
	return -ENOSYS;
}

static inline int ata_acpi_gtm(const struct ata_port *ap,
			       struct ata_acpi_gtm *stm)
{
	return -ENOSYS;
}

static inline unsigned int ata_acpi_gtm_xfermask(struct ata_device *dev,
					const struct ata_acpi_gtm *gtm)
{
	return 0;
}

static inline int ata_acpi_cbl_80wire(struct ata_port *ap,
				      const struct ata_acpi_gtm *gtm)
{
	return 0;
}
#endif

extern void ata_port_schedule_eh(struct ata_port *ap);
extern void ata_port_wait_eh(struct ata_port *ap);
extern int ata_link_abort(struct ata_link *link);
extern int ata_port_abort(struct ata_port *ap);
extern int ata_port_freeze(struct ata_port *ap);
extern int sata_async_notification(struct ata_port *ap);

extern void ata_eh_freeze_port(struct ata_port *ap);
extern void ata_eh_thaw_port(struct ata_port *ap);

extern void ata_eh_qc_complete(struct ata_queued_cmd *qc);
extern void ata_eh_qc_retry(struct ata_queued_cmd *qc);
extern void ata_eh_analyze_ncq_error(struct ata_link *link);

extern void ata_do_eh(struct ata_port *ap, ata_prereset_fn_t prereset,
		      ata_reset_fn_t softreset, ata_reset_fn_t hardreset,
		      ata_postreset_fn_t postreset);
extern void ata_std_error_handler(struct ata_port *ap);
extern int ata_link_nr_enabled(struct ata_link *link);

extern const struct ata_port_operations ata_base_port_ops;
extern const struct ata_port_operations sata_port_ops;
extern struct device_attribute *ata_common_sdev_attrs[];

#define ATA_BASE_SHT(drv_name)					\
	.module			= THIS_MODULE,			\
	.name			= drv_name,			\
	.ioctl			= ata_scsi_ioctl,		\
	.queuecommand		= ata_scsi_queuecmd,		\
	.can_queue		= ATA_DEF_QUEUE,		\
	.this_id		= ATA_SHT_THIS_ID,		\
	.cmd_per_lun		= ATA_SHT_CMD_PER_LUN,		\
	.emulated		= ATA_SHT_EMULATED,		\
	.use_clustering		= ATA_SHT_USE_CLUSTERING,	\
	.proc_name		= drv_name,			\
	.slave_configure	= ata_scsi_slave_config,	\
	.slave_destroy		= ata_scsi_slave_destroy,	\
	.bios_param		= ata_std_bios_param,		\
	.unlock_native_capacity	= ata_scsi_unlock_native_capacity, \
	.sdev_attrs		= ata_common_sdev_attrs

#define ATA_NCQ_SHT(drv_name)					\
	ATA_BASE_SHT(drv_name),					\
	.change_queue_depth	= ata_scsi_change_queue_depth

#ifdef CONFIG_SATA_PMP
static inline bool sata_pmp_supported(struct ata_port *ap)
{
	return ap->flags & ATA_FLAG_PMP;
}

static inline bool sata_pmp_attached(struct ata_port *ap)
{
	return ap->nr_pmp_links != 0;
}

static inline int ata_is_host_link(const struct ata_link *link)
{
	return link == &link->ap->link || link == link->ap->slave_link;
}
#else 
static inline bool sata_pmp_supported(struct ata_port *ap)
{
	return false;
}

static inline bool sata_pmp_attached(struct ata_port *ap)
{
	return false;
}

static inline int ata_is_host_link(const struct ata_link *link)
{
	return 1;
}
#endif 

static inline int sata_srst_pmp(struct ata_link *link)
{
	if (sata_pmp_supported(link->ap) && ata_is_host_link(link))
		return SATA_PMP_CTRL_PORT;
	return link->pmp;
}

__printf(3, 4)
int ata_port_printk(const struct ata_port *ap, const char *level,
		    const char *fmt, ...);
__printf(3, 4)
int ata_link_printk(const struct ata_link *link, const char *level,
		    const char *fmt, ...);
__printf(3, 4)
int ata_dev_printk(const struct ata_device *dev, const char *level,
		   const char *fmt, ...);

#define ata_port_err(ap, fmt, ...)				\
	ata_port_printk(ap, KERN_ERR, fmt, ##__VA_ARGS__)
#define ata_port_warn(ap, fmt, ...)				\
	ata_port_printk(ap, KERN_WARNING, fmt, ##__VA_ARGS__)
#define ata_port_notice(ap, fmt, ...)				\
	ata_port_printk(ap, KERN_NOTICE, fmt, ##__VA_ARGS__)
#define ata_port_info(ap, fmt, ...)				\
	ata_port_printk(ap, KERN_INFO, fmt, ##__VA_ARGS__)
#define ata_port_dbg(ap, fmt, ...)				\
	ata_port_printk(ap, KERN_DEBUG, fmt, ##__VA_ARGS__)

#define ata_link_err(link, fmt, ...)				\
	ata_link_printk(link, KERN_ERR, fmt, ##__VA_ARGS__)
#define ata_link_warn(link, fmt, ...)				\
	ata_link_printk(link, KERN_WARNING, fmt, ##__VA_ARGS__)
#define ata_link_notice(link, fmt, ...)				\
	ata_link_printk(link, KERN_NOTICE, fmt, ##__VA_ARGS__)
#define ata_link_info(link, fmt, ...)				\
	ata_link_printk(link, KERN_INFO, fmt, ##__VA_ARGS__)
#define ata_link_dbg(link, fmt, ...)				\
	ata_link_printk(link, KERN_DEBUG, fmt, ##__VA_ARGS__)

#define ata_dev_err(dev, fmt, ...)				\
	ata_dev_printk(dev, KERN_ERR, fmt, ##__VA_ARGS__)
#define ata_dev_warn(dev, fmt, ...)				\
	ata_dev_printk(dev, KERN_WARNING, fmt, ##__VA_ARGS__)
#define ata_dev_notice(dev, fmt, ...)				\
	ata_dev_printk(dev, KERN_NOTICE, fmt, ##__VA_ARGS__)
#define ata_dev_info(dev, fmt, ...)				\
	ata_dev_printk(dev, KERN_INFO, fmt, ##__VA_ARGS__)
#define ata_dev_dbg(dev, fmt, ...)				\
	ata_dev_printk(dev, KERN_DEBUG, fmt, ##__VA_ARGS__)

void ata_print_version(const struct device *dev, const char *version);

extern __printf(2, 3)
void __ata_ehi_push_desc(struct ata_eh_info *ehi, const char *fmt, ...);
extern __printf(2, 3)
void ata_ehi_push_desc(struct ata_eh_info *ehi, const char *fmt, ...);
extern void ata_ehi_clear_desc(struct ata_eh_info *ehi);

static inline void ata_ehi_hotplugged(struct ata_eh_info *ehi)
{
	ehi->probe_mask |= (1 << ATA_MAX_DEVICES) - 1;
	ehi->flags |= ATA_EHI_HOTPLUGGED;
	ehi->action |= ATA_EH_RESET | ATA_EH_ENABLE_LINK;
	ehi->err_mask |= AC_ERR_ATA_BUS;
}

extern __printf(2, 3)
void ata_port_desc(struct ata_port *ap, const char *fmt, ...);
#ifdef CONFIG_PCI
extern void ata_port_pbar_desc(struct ata_port *ap, int bar, ssize_t offset,
			       const char *name);
#endif

static inline unsigned int ata_tag_valid(unsigned int tag)
{
	return (tag < ATA_MAX_QUEUE) ? 1 : 0;
}

static inline unsigned int ata_tag_internal(unsigned int tag)
{
	return tag == ATA_TAG_INTERNAL;
}

static inline unsigned int ata_class_enabled(unsigned int class)
{
	return class == ATA_DEV_ATA || class == ATA_DEV_ATAPI ||
		class == ATA_DEV_PMP || class == ATA_DEV_SEMB;
}

static inline unsigned int ata_class_disabled(unsigned int class)
{
	return class == ATA_DEV_ATA_UNSUP || class == ATA_DEV_ATAPI_UNSUP ||
		class == ATA_DEV_PMP_UNSUP || class == ATA_DEV_SEMB_UNSUP;
}

static inline unsigned int ata_class_absent(unsigned int class)
{
	return !ata_class_enabled(class) && !ata_class_disabled(class);
}

static inline unsigned int ata_dev_enabled(const struct ata_device *dev)
{
	return ata_class_enabled(dev->class);
}

static inline unsigned int ata_dev_disabled(const struct ata_device *dev)
{
	return ata_class_disabled(dev->class);
}

static inline unsigned int ata_dev_absent(const struct ata_device *dev)
{
	return ata_class_absent(dev->class);
}

static inline int ata_link_max_devices(const struct ata_link *link)
{
	if (ata_is_host_link(link) && link->ap->flags & ATA_FLAG_SLAVE_POSS)
		return 2;
	return 1;
}

static inline int ata_link_active(struct ata_link *link)
{
	return ata_tag_valid(link->active_tag) || link->sactive;
}

enum ata_link_iter_mode {
	ATA_LITER_EDGE,		
	ATA_LITER_HOST_FIRST,	
	ATA_LITER_PMP_FIRST,	
};

enum ata_dev_iter_mode {
	ATA_DITER_ENABLED,
	ATA_DITER_ENABLED_REVERSE,
	ATA_DITER_ALL,
	ATA_DITER_ALL_REVERSE,
};

extern struct ata_link *ata_link_next(struct ata_link *link,
				      struct ata_port *ap,
				      enum ata_link_iter_mode mode);

extern struct ata_device *ata_dev_next(struct ata_device *dev,
				       struct ata_link *link,
				       enum ata_dev_iter_mode mode);

#define ata_for_each_link(link, ap, mode) \
	for ((link) = ata_link_next(NULL, (ap), ATA_LITER_##mode); (link); \
	     (link) = ata_link_next((link), (ap), ATA_LITER_##mode))

#define ata_for_each_dev(dev, link, mode) \
	for ((dev) = ata_dev_next(NULL, (link), ATA_DITER_##mode); (dev); \
	     (dev) = ata_dev_next((dev), (link), ATA_DITER_##mode))

static inline int ata_ncq_enabled(struct ata_device *dev)
{
	return (dev->flags & (ATA_DFLAG_PIO | ATA_DFLAG_NCQ_OFF |
			      ATA_DFLAG_NCQ)) == ATA_DFLAG_NCQ;
}

static inline void ata_qc_set_polling(struct ata_queued_cmd *qc)
{
	qc->tf.ctl |= ATA_NIEN;
}

static inline struct ata_queued_cmd *__ata_qc_from_tag(struct ata_port *ap,
						       unsigned int tag)
{
	if (likely(ata_tag_valid(tag)))
		return &ap->qcmd[tag];
	return NULL;
}

static inline struct ata_queued_cmd *ata_qc_from_tag(struct ata_port *ap,
						     unsigned int tag)
{
	struct ata_queued_cmd *qc = __ata_qc_from_tag(ap, tag);

	if (unlikely(!qc) || !ap->ops->error_handler)
		return qc;

	if ((qc->flags & (ATA_QCFLAG_ACTIVE |
			  ATA_QCFLAG_FAILED)) == ATA_QCFLAG_ACTIVE)
		return qc;

	return NULL;
}

static inline unsigned int ata_qc_raw_nbytes(struct ata_queued_cmd *qc)
{
	return qc->nbytes - min(qc->extrabytes, qc->nbytes);
}

static inline void ata_tf_init(struct ata_device *dev, struct ata_taskfile *tf)
{
	memset(tf, 0, sizeof(*tf));

#ifdef CONFIG_ATA_SFF
	tf->ctl = dev->link->ap->ctl;
#else
	tf->ctl = ATA_DEVCTL_OBS;
#endif
	if (dev->devno == 0)
		tf->device = ATA_DEVICE_OBS;
	else
		tf->device = ATA_DEVICE_OBS | ATA_DEV1;
}

static inline void ata_qc_reinit(struct ata_queued_cmd *qc)
{
	qc->dma_dir = DMA_NONE;
	qc->sg = NULL;
	qc->flags = 0;
	qc->cursg = NULL;
	qc->cursg_ofs = 0;
	qc->nbytes = qc->extrabytes = qc->curbytes = 0;
	qc->n_elem = 0;
	qc->err_mask = 0;
	qc->sect_size = ATA_SECT_SIZE;

	ata_tf_init(qc->dev, &qc->tf);

	
	qc->result_tf.command = ATA_DRDY;
	qc->result_tf.feature = 0;
}

static inline int ata_try_flush_cache(const struct ata_device *dev)
{
	return ata_id_wcache_enabled(dev->id) ||
	       ata_id_has_flush(dev->id) ||
	       ata_id_has_flush_ext(dev->id);
}

static inline unsigned int ac_err_mask(u8 status)
{
	if (status & (ATA_BUSY | ATA_DRQ))
		return AC_ERR_HSM;
	if (status & (ATA_ERR | ATA_DF))
		return AC_ERR_DEV;
	return 0;
}

static inline unsigned int __ac_err_mask(u8 status)
{
	unsigned int mask = ac_err_mask(status);
	if (mask == 0)
		return AC_ERR_OTHER;
	return mask;
}

static inline struct ata_port *ata_shost_to_port(struct Scsi_Host *host)
{
	return *(struct ata_port **)&host->hostdata[0];
}

static inline int ata_check_ready(u8 status)
{
	if (!(status & ATA_BUSY))
		return 1;

	
	if (status == 0xff)
		return -ENODEV;

	return 0;
}

static inline unsigned long ata_deadline(unsigned long from_jiffies,
					 unsigned long timeout_msecs)
{
	return from_jiffies + msecs_to_jiffies(timeout_msecs);
}


static inline int ata_using_mwdma(struct ata_device *adev)
{
	if (adev->dma_mode >= XFER_MW_DMA_0 && adev->dma_mode <= XFER_MW_DMA_4)
		return 1;
	return 0;
}

static inline int ata_using_udma(struct ata_device *adev)
{
	if (adev->dma_mode >= XFER_UDMA_0 && adev->dma_mode <= XFER_UDMA_7)
		return 1;
	return 0;
}

static inline int ata_dma_enabled(struct ata_device *adev)
{
	return (adev->dma_mode == 0xFF ? 0 : 1);
}

#ifdef CONFIG_SATA_PMP

extern const struct ata_port_operations sata_pmp_port_ops;

extern int sata_pmp_qc_defer_cmd_switch(struct ata_queued_cmd *qc);
extern void sata_pmp_error_handler(struct ata_port *ap);

#else 

#define sata_pmp_port_ops		sata_port_ops
#define sata_pmp_qc_defer_cmd_switch	ata_std_qc_defer
#define sata_pmp_error_handler		ata_std_error_handler

#endif 


#ifdef CONFIG_ATA_SFF

extern const struct ata_port_operations ata_sff_port_ops;
extern const struct ata_port_operations ata_bmdma32_port_ops;

#define ATA_PIO_SHT(drv_name)					\
	ATA_BASE_SHT(drv_name),					\
	.sg_tablesize		= LIBATA_MAX_PRD,		\
	.dma_boundary		= ATA_DMA_BOUNDARY

extern void ata_sff_dev_select(struct ata_port *ap, unsigned int device);
extern u8 ata_sff_check_status(struct ata_port *ap);
extern void ata_sff_pause(struct ata_port *ap);
extern void ata_sff_dma_pause(struct ata_port *ap);
extern int ata_sff_busy_sleep(struct ata_port *ap,
			      unsigned long timeout_pat, unsigned long timeout);
extern int ata_sff_wait_ready(struct ata_link *link, unsigned long deadline);
extern void ata_sff_tf_load(struct ata_port *ap, const struct ata_taskfile *tf);
extern void ata_sff_tf_read(struct ata_port *ap, struct ata_taskfile *tf);
extern void ata_sff_exec_command(struct ata_port *ap,
				 const struct ata_taskfile *tf);
extern unsigned int ata_sff_data_xfer(struct ata_device *dev,
			unsigned char *buf, unsigned int buflen, int rw);
extern unsigned int ata_sff_data_xfer32(struct ata_device *dev,
			unsigned char *buf, unsigned int buflen, int rw);
extern unsigned int ata_sff_data_xfer_noirq(struct ata_device *dev,
			unsigned char *buf, unsigned int buflen, int rw);
extern void ata_sff_irq_on(struct ata_port *ap);
extern void ata_sff_irq_clear(struct ata_port *ap);
extern int ata_sff_hsm_move(struct ata_port *ap, struct ata_queued_cmd *qc,
			    u8 status, int in_wq);
extern void ata_sff_queue_work(struct work_struct *work);
extern void ata_sff_queue_delayed_work(struct delayed_work *dwork,
		unsigned long delay);
extern void ata_sff_queue_pio_task(struct ata_link *link, unsigned long delay);
extern unsigned int ata_sff_qc_issue(struct ata_queued_cmd *qc);
extern bool ata_sff_qc_fill_rtf(struct ata_queued_cmd *qc);
extern unsigned int ata_sff_port_intr(struct ata_port *ap,
				      struct ata_queued_cmd *qc);
extern irqreturn_t ata_sff_interrupt(int irq, void *dev_instance);
extern void ata_sff_lost_interrupt(struct ata_port *ap);
extern void ata_sff_freeze(struct ata_port *ap);
extern void ata_sff_thaw(struct ata_port *ap);
extern int ata_sff_prereset(struct ata_link *link, unsigned long deadline);
extern unsigned int ata_sff_dev_classify(struct ata_device *dev, int present,
					  u8 *r_err);
extern int ata_sff_wait_after_reset(struct ata_link *link, unsigned int devmask,
				    unsigned long deadline);
extern int ata_sff_softreset(struct ata_link *link, unsigned int *classes,
			     unsigned long deadline);
extern int sata_sff_hardreset(struct ata_link *link, unsigned int *class,
			       unsigned long deadline);
extern void ata_sff_postreset(struct ata_link *link, unsigned int *classes);
extern void ata_sff_drain_fifo(struct ata_queued_cmd *qc);
extern void ata_sff_error_handler(struct ata_port *ap);
extern void ata_sff_std_ports(struct ata_ioports *ioaddr);
#ifdef CONFIG_PCI
extern int ata_pci_sff_init_host(struct ata_host *host);
extern int ata_pci_sff_prepare_host(struct pci_dev *pdev,
				    const struct ata_port_info * const * ppi,
				    struct ata_host **r_host);
extern int ata_pci_sff_activate_host(struct ata_host *host,
				     irq_handler_t irq_handler,
				     struct scsi_host_template *sht);
extern int ata_pci_sff_init_one(struct pci_dev *pdev,
		const struct ata_port_info * const * ppi,
		struct scsi_host_template *sht, void *host_priv, int hflags);
#endif 

#ifdef CONFIG_ATA_BMDMA

extern const struct ata_port_operations ata_bmdma_port_ops;

#define ATA_BMDMA_SHT(drv_name)					\
	ATA_BASE_SHT(drv_name),					\
	.sg_tablesize		= LIBATA_MAX_PRD,		\
	.dma_boundary		= ATA_DMA_BOUNDARY

extern void ata_bmdma_qc_prep(struct ata_queued_cmd *qc);
extern unsigned int ata_bmdma_qc_issue(struct ata_queued_cmd *qc);
extern void ata_bmdma_dumb_qc_prep(struct ata_queued_cmd *qc);
extern unsigned int ata_bmdma_port_intr(struct ata_port *ap,
				      struct ata_queued_cmd *qc);
extern irqreturn_t ata_bmdma_interrupt(int irq, void *dev_instance);
extern void ata_bmdma_error_handler(struct ata_port *ap);
extern void ata_bmdma_post_internal_cmd(struct ata_queued_cmd *qc);
extern void ata_bmdma_irq_clear(struct ata_port *ap);
extern void ata_bmdma_setup(struct ata_queued_cmd *qc);
extern void ata_bmdma_start(struct ata_queued_cmd *qc);
extern void ata_bmdma_stop(struct ata_queued_cmd *qc);
extern u8 ata_bmdma_status(struct ata_port *ap);
extern int ata_bmdma_port_start(struct ata_port *ap);
extern int ata_bmdma_port_start32(struct ata_port *ap);

#ifdef CONFIG_PCI
extern int ata_pci_bmdma_clear_simplex(struct pci_dev *pdev);
extern void ata_pci_bmdma_init(struct ata_host *host);
extern int ata_pci_bmdma_prepare_host(struct pci_dev *pdev,
				      const struct ata_port_info * const * ppi,
				      struct ata_host **r_host);
extern int ata_pci_bmdma_init_one(struct pci_dev *pdev,
				  const struct ata_port_info * const * ppi,
				  struct scsi_host_template *sht,
				  void *host_priv, int hflags);
#endif 
#endif 

static inline u8 ata_sff_busy_wait(struct ata_port *ap, unsigned int bits,
				   unsigned int max)
{
	u8 status;

	do {
		udelay(10);
		status = ap->ops->sff_check_status(ap);
		max--;
	} while (status != 0xff && (status & bits) && (max > 0));

	return status;
}

static inline u8 ata_wait_idle(struct ata_port *ap)
{
	u8 status = ata_sff_busy_wait(ap, ATA_BUSY | ATA_DRQ, 1000);

#ifdef ATA_DEBUG
	if (status != 0xff && (status & (ATA_BUSY | ATA_DRQ)))
		ata_port_printk(ap, KERN_DEBUG, "abnormal Status 0x%X\n",
				status);
#endif

	return status;
}
#endif 

#endif 
