/*
 * NinjaSCSI-32Bi Cardbus, NinjaSCSI-32UDE PCI/CardBus SCSI driver
 * Copyright (C) 2001, 2002, 2003
 *      YOKOTA Hiroshi <yokota@netlab.is.tsukuba.ac.jp>
 *      GOTO Masanori <gotom@debian.or.jp>, <gotom@debian.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * Revision History:
 *   1.0: Initial Release.
 *   1.1: Add /proc SDTR status.
 *        Remove obsolete error handler nsp32_reset.
 *        Some clean up.
 *   1.2: PowerPC (big endian) support.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/ioport.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/dma-mapping.h>

#include <asm/dma.h>
#include <asm/io.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_ioctl.h>

#include "nsp32.h"


static int       trans_mode = 0;	
module_param     (trans_mode, int, 0);
MODULE_PARM_DESC(trans_mode, "transfer mode (0: BIOS(default) 1: Async 2: Ultra20M");
#define ASYNC_MODE    1
#define ULTRA20M_MODE 2

static bool      auto_param = 0;	
module_param     (auto_param, bool, 0);
MODULE_PARM_DESC(auto_param, "AutoParameter mode (0: ON(default) 1: OFF)");

static bool      disc_priv  = 1;	
module_param     (disc_priv, bool, 0);
MODULE_PARM_DESC(disc_priv,  "disconnection privilege mode (0: ON 1: OFF(default))");

MODULE_AUTHOR("YOKOTA Hiroshi <yokota@netlab.is.tsukuba.ac.jp>, GOTO Masanori <gotom@debian.or.jp>");
MODULE_DESCRIPTION("Workbit NinjaSCSI-32Bi/UDE CardBus/PCI SCSI host bus adapter module");
MODULE_LICENSE("GPL");

static const char *nsp32_release_version = "1.2";


static struct pci_device_id nsp32_pci_table[] __devinitdata = {
	{
		.vendor      = PCI_VENDOR_ID_IODATA,
		.device      = PCI_DEVICE_ID_NINJASCSI_32BI_CBSC_II,
		.subvendor   = PCI_ANY_ID,
		.subdevice   = PCI_ANY_ID,
		.driver_data = MODEL_IODATA,
	},
	{
		.vendor      = PCI_VENDOR_ID_WORKBIT,
		.device      = PCI_DEVICE_ID_NINJASCSI_32BI_KME,
		.subvendor   = PCI_ANY_ID,
		.subdevice   = PCI_ANY_ID,
		.driver_data = MODEL_KME,
	},
	{
		.vendor      = PCI_VENDOR_ID_WORKBIT,
		.device      = PCI_DEVICE_ID_NINJASCSI_32BI_WBT,
		.subvendor   = PCI_ANY_ID,
		.subdevice   = PCI_ANY_ID,
		.driver_data = MODEL_WORKBIT,
	},
	{
		.vendor      = PCI_VENDOR_ID_WORKBIT,
		.device      = PCI_DEVICE_ID_WORKBIT_STANDARD,
		.subvendor   = PCI_ANY_ID,
		.subdevice   = PCI_ANY_ID,
		.driver_data = MODEL_PCI_WORKBIT,
	},
	{
		.vendor      = PCI_VENDOR_ID_WORKBIT,
		.device      = PCI_DEVICE_ID_NINJASCSI_32BI_LOGITEC,
		.subvendor   = PCI_ANY_ID,
		.subdevice   = PCI_ANY_ID,
		.driver_data = MODEL_LOGITEC,
	},
	{
		.vendor      = PCI_VENDOR_ID_WORKBIT,
		.device      = PCI_DEVICE_ID_NINJASCSI_32BIB_LOGITEC,
		.subvendor   = PCI_ANY_ID,
		.subdevice   = PCI_ANY_ID,
		.driver_data = MODEL_PCI_LOGITEC,
	},
	{
		.vendor      = PCI_VENDOR_ID_WORKBIT,
		.device      = PCI_DEVICE_ID_NINJASCSI_32UDE_MELCO,
		.subvendor   = PCI_ANY_ID,
		.subdevice   = PCI_ANY_ID,
		.driver_data = MODEL_PCI_MELCO,
	},
	{
		.vendor      = PCI_VENDOR_ID_WORKBIT,
		.device      = PCI_DEVICE_ID_NINJASCSI_32UDE_MELCO_II,
		.subvendor   = PCI_ANY_ID,
		.subdevice   = PCI_ANY_ID,
		.driver_data = MODEL_PCI_MELCO,
	},
	{0,0,},
};
MODULE_DEVICE_TABLE(pci, nsp32_pci_table);

static nsp32_hw_data nsp32_data_base;  


static nsp32_sync_table nsp32_sync_table_40M[] = {
     
	{0x1,  0, 0x0c, 0x0c, SMPL_40M},  
	{0x2,  0, 0x0d, 0x18, SMPL_40M},  
	{0x3,  1, 0x19, 0x19, SMPL_40M},  
	{0x4,  1, 0x1a, 0x1f, SMPL_20M},  
	{0x5,  2, 0x20, 0x25, SMPL_20M},  
	{0x6,  2, 0x26, 0x31, SMPL_20M},  
	{0x7,  3, 0x32, 0x32, SMPL_20M},  
	{0x8,  3, 0x33, 0x38, SMPL_10M},  
	{0x9,  3, 0x39, 0x3e, SMPL_10M},  
};

static nsp32_sync_table nsp32_sync_table_20M[] = {
	{0x1,  0, 0x19, 0x19, SMPL_40M},  
	{0x2,  0, 0x1a, 0x25, SMPL_20M},  
	{0x3,  1, 0x26, 0x32, SMPL_20M},  
	{0x4,  1, 0x33, 0x3e, SMPL_10M},  
	{0x5,  2, 0x3f, 0x4b, SMPL_10M},  
	{0x6,  2, 0x4c, 0x57, SMPL_10M},  
	{0x7,  3, 0x58, 0x64, SMPL_10M},  
	{0x8,  3, 0x65, 0x70, SMPL_10M},  
	{0x9,  3, 0x71, 0x7d, SMPL_10M},  
};

static nsp32_sync_table nsp32_sync_table_pci[] = {
	{0x1,  0, 0x0c, 0x0f, SMPL_40M},  
	{0x2,  0, 0x10, 0x16, SMPL_40M},  
	{0x3,  1, 0x17, 0x1e, SMPL_20M},  
	{0x4,  1, 0x1f, 0x25, SMPL_20M},  
	{0x5,  2, 0x26, 0x2d, SMPL_20M},  
	{0x6,  2, 0x2e, 0x34, SMPL_10M},  
	{0x7,  3, 0x35, 0x3c, SMPL_10M},  
	{0x8,  3, 0x3d, 0x43, SMPL_10M},  
	{0x9,  3, 0x44, 0x4b, SMPL_10M},  
};

static int  __devinit nsp32_probe (struct pci_dev *, const struct pci_device_id *);
static void __devexit nsp32_remove(struct pci_dev *);
static int  __init    init_nsp32  (void);
static void __exit    exit_nsp32  (void);

static int         nsp32_proc_info   (struct Scsi_Host *, char *, char **, off_t, int, int);

static int         nsp32_detect      (struct pci_dev *pdev);
static int         nsp32_queuecommand(struct Scsi_Host *, struct scsi_cmnd *);
static const char *nsp32_info        (struct Scsi_Host *);
static int         nsp32_release     (struct Scsi_Host *);

static int         nsp32_eh_abort     (struct scsi_cmnd *);
static int         nsp32_eh_bus_reset (struct scsi_cmnd *);
static int         nsp32_eh_host_reset(struct scsi_cmnd *);

static void nsp32_build_identify(struct scsi_cmnd *);
static void nsp32_build_nop     (struct scsi_cmnd *);
static void nsp32_build_reject  (struct scsi_cmnd *);
static void nsp32_build_sdtr    (struct scsi_cmnd *, unsigned char, unsigned char);

static int  nsp32_busfree_occur(struct scsi_cmnd *, unsigned short);
static void nsp32_msgout_occur (struct scsi_cmnd *);
static void nsp32_msgin_occur  (struct scsi_cmnd *, unsigned long, unsigned short);

static int  nsp32_setup_sg_table    (struct scsi_cmnd *);
static int  nsp32_selection_autopara(struct scsi_cmnd *);
static int  nsp32_selection_autoscsi(struct scsi_cmnd *);
static void nsp32_scsi_done         (struct scsi_cmnd *);
static int  nsp32_arbitration       (struct scsi_cmnd *, unsigned int);
static int  nsp32_reselection       (struct scsi_cmnd *, unsigned char);
static void nsp32_adjust_busfree    (struct scsi_cmnd *, unsigned int);
static void nsp32_restart_autoscsi  (struct scsi_cmnd *, unsigned short);

static void nsp32_analyze_sdtr       (struct scsi_cmnd *);
static int  nsp32_search_period_entry(nsp32_hw_data *, nsp32_target *, unsigned char);
static void nsp32_set_async          (nsp32_hw_data *, nsp32_target *);
static void nsp32_set_max_sync       (nsp32_hw_data *, nsp32_target *, unsigned char *, unsigned char *);
static void nsp32_set_sync_entry     (nsp32_hw_data *, nsp32_target *, int, unsigned char);

static void nsp32_wait_req    (nsp32_hw_data *, int);
static void nsp32_wait_sack   (nsp32_hw_data *, int);
static void nsp32_sack_assert (nsp32_hw_data *);
static void nsp32_sack_negate (nsp32_hw_data *);
static void nsp32_do_bus_reset(nsp32_hw_data *);

static irqreturn_t do_nsp32_isr(int, void *);

static int  nsp32hw_init(nsp32_hw_data *);

static        int  nsp32_getprom_param (nsp32_hw_data *);
static        int  nsp32_getprom_at24  (nsp32_hw_data *);
static        int  nsp32_getprom_c16   (nsp32_hw_data *);
static        void nsp32_prom_start    (nsp32_hw_data *);
static        void nsp32_prom_stop     (nsp32_hw_data *);
static        int  nsp32_prom_read     (nsp32_hw_data *, int);
static        int  nsp32_prom_read_bit (nsp32_hw_data *);
static        void nsp32_prom_write_bit(nsp32_hw_data *, int);
static        void nsp32_prom_set      (nsp32_hw_data *, int, int);
static        int  nsp32_prom_get      (nsp32_hw_data *, int);

static void nsp32_message (const char *, int, char *, char *, ...);
#ifdef NSP32_DEBUG
static void nsp32_dmessage(const char *, int, int,    char *, ...);
#endif

static struct scsi_host_template nsp32_template = {
	.proc_name			= "nsp32",
	.name				= "Workbit NinjaSCSI-32Bi/UDE",
	.proc_info			= nsp32_proc_info,
	.info				= nsp32_info,
	.queuecommand			= nsp32_queuecommand,
	.can_queue			= 1,
	.sg_tablesize			= NSP32_SG_SIZE,
	.max_sectors			= 128,
	.cmd_per_lun			= 1,
	.this_id			= NSP32_HOST_SCSIID,
	.use_clustering			= DISABLE_CLUSTERING,
	.eh_abort_handler       	= nsp32_eh_abort,
	.eh_bus_reset_handler		= nsp32_eh_bus_reset,
	.eh_host_reset_handler		= nsp32_eh_host_reset,
};

#include "nsp32_io.h"

#ifndef NSP32_DEBUG
# define NSP32_DEBUG_MASK	      0x000000
# define nsp32_msg(type, args...)     nsp32_message ("", 0, (type), args)
# define nsp32_dbg(mask, args...)     
#else
# define NSP32_DEBUG_MASK	      0xffffff
# define nsp32_msg(type, args...) \
	nsp32_message (__func__, __LINE__, (type), args)
# define nsp32_dbg(mask, args...) \
	nsp32_dmessage(__func__, __LINE__, (mask), args)
#endif

#define NSP32_DEBUG_QUEUECOMMAND	BIT(0)
#define NSP32_DEBUG_REGISTER		BIT(1)
#define NSP32_DEBUG_AUTOSCSI		BIT(2)
#define NSP32_DEBUG_INTR		BIT(3)
#define NSP32_DEBUG_SGLIST		BIT(4)
#define NSP32_DEBUG_BUSFREE		BIT(5)
#define NSP32_DEBUG_CDB_CONTENTS	BIT(6)
#define NSP32_DEBUG_RESELECTION		BIT(7)
#define NSP32_DEBUG_MSGINOCCUR		BIT(8)
#define NSP32_DEBUG_EEPROM		BIT(9)
#define NSP32_DEBUG_MSGOUTOCCUR		BIT(10)
#define NSP32_DEBUG_BUSRESET		BIT(11)
#define NSP32_DEBUG_RESTART		BIT(12)
#define NSP32_DEBUG_SYNC		BIT(13)
#define NSP32_DEBUG_WAIT		BIT(14)
#define NSP32_DEBUG_TARGETFLAG		BIT(15)
#define NSP32_DEBUG_PROC		BIT(16)
#define NSP32_DEBUG_INIT		BIT(17)
#define NSP32_SPECIAL_PRINT_REGISTER	BIT(20)

#define NSP32_DEBUG_BUF_LEN		100

static void nsp32_message(const char *func, int line, char *type, char *fmt, ...)
{
	va_list args;
	char buf[NSP32_DEBUG_BUF_LEN];

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

#ifndef NSP32_DEBUG
	printk("%snsp32: %s\n", type, buf);
#else
	printk("%snsp32: %s (%d): %s\n", type, func, line, buf);
#endif
}

#ifdef NSP32_DEBUG
static void nsp32_dmessage(const char *func, int line, int mask, char *fmt, ...)
{
	va_list args;
	char buf[NSP32_DEBUG_BUF_LEN];

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (mask & NSP32_DEBUG_MASK) {
		printk("nsp32-debug: 0x%x %s (%d): %s\n", mask, func, line, buf);
	}
}
#endif

#ifdef NSP32_DEBUG
# include "nsp32_debug.c"
#else
# define show_command(arg)   
# define show_busphase(arg)  
# define show_autophase(arg) 
#endif

static void nsp32_build_identify(struct scsi_cmnd *SCpnt)
{
	nsp32_hw_data *data = (nsp32_hw_data *)SCpnt->device->host->hostdata;
	int pos             = data->msgout_len;
	int mode            = FALSE;

	
	if (disc_priv == 0) {
		
	}

	data->msgoutbuf[pos] = IDENTIFY(mode, SCpnt->device->lun); pos++;

	data->msgout_len = pos;
}

static void nsp32_build_sdtr(struct scsi_cmnd    *SCpnt,
			     unsigned char period,
			     unsigned char offset)
{
	nsp32_hw_data *data = (nsp32_hw_data *)SCpnt->device->host->hostdata;
	int pos             = data->msgout_len;

	data->msgoutbuf[pos] = EXTENDED_MESSAGE;  pos++;
	data->msgoutbuf[pos] = EXTENDED_SDTR_LEN; pos++;
	data->msgoutbuf[pos] = EXTENDED_SDTR;     pos++;
	data->msgoutbuf[pos] = period;            pos++;
	data->msgoutbuf[pos] = offset;            pos++;

	data->msgout_len = pos;
}

static void nsp32_build_nop(struct scsi_cmnd *SCpnt)
{
	nsp32_hw_data *data = (nsp32_hw_data *)SCpnt->device->host->hostdata;
	int            pos  = data->msgout_len;

	if (pos != 0) {
		nsp32_msg(KERN_WARNING,
			  "Some messages are already contained!");
		return;
	}

	data->msgoutbuf[pos] = NOP; pos++;
	data->msgout_len = pos;
}

static void nsp32_build_reject(struct scsi_cmnd *SCpnt)
{
	nsp32_hw_data *data = (nsp32_hw_data *)SCpnt->device->host->hostdata;
	int            pos  = data->msgout_len;

	data->msgoutbuf[pos] = MESSAGE_REJECT; pos++;
	data->msgout_len = pos;
}
	
#if 0
static void nsp32_start_timer(struct scsi_cmnd *SCpnt, int time)
{
	unsigned int base = SCpnt->host->io_port;

	nsp32_dbg(NSP32_DEBUG_INTR, "timer=%d", time);

	if (time & (~TIMER_CNT_MASK)) {
		nsp32_dbg(NSP32_DEBUG_INTR, "timer set overflow");
	}

	nsp32_write2(base, TIMER_SET, time & TIMER_CNT_MASK);
}
#endif


static int nsp32_selection_autopara(struct scsi_cmnd *SCpnt)
{
	nsp32_hw_data  *data = (nsp32_hw_data *)SCpnt->device->host->hostdata;
	unsigned int	base    = SCpnt->device->host->io_port;
	unsigned int	host_id = SCpnt->device->host->this_id;
	unsigned char	target  = scmd_id(SCpnt);
	nsp32_autoparam *param  = data->autoparam;
	unsigned char	phase;
	int		i, ret;
	unsigned int	msgout;
	u16_le	        s;

	nsp32_dbg(NSP32_DEBUG_AUTOSCSI, "in");

	phase = nsp32_read1(base, SCSI_BUS_MONITOR);
	if (phase != BUSMON_BUS_FREE) {
		nsp32_msg(KERN_WARNING, "bus busy");
		show_busphase(phase & BUSMON_PHASE_MASK);
		SCpnt->result = DID_BUS_BUSY << 16;
		return FALSE;
	}

	if (data->msgout_len == 0) {
		nsp32_msg(KERN_ERR, "SCSI MsgOut without any message!");
		SCpnt->result = DID_ERROR << 16;
		return FALSE;
	} else if (data->msgout_len > 0 && data->msgout_len <= 3) {
		msgout = 0;
		for (i = 0; i < data->msgout_len; i++) {
			msgout >>= 8;
			msgout |= ((unsigned int)(data->msgoutbuf[i]) << 24);
		}
		msgout |= MV_VALID;	
		msgout |= (unsigned int)data->msgout_len; 
	} else {
		
		msgout = 0;
	}

	
	

	memset(param, 0, sizeof(nsp32_autoparam));

	
	for (i = 0; i < SCpnt->cmd_len; i++) {
		param->cdb[4 * i] = SCpnt->cmnd[i];
	}

	
	param->msgout = cpu_to_le32(msgout);

	
	param->syncreg    = data->cur_target->syncreg;
	param->ackwidth   = data->cur_target->ackwidth;
	param->target_id  = BIT(host_id) | BIT(target);
	param->sample_reg = data->cur_target->sample_reg;

	

	
	param->command_control = cpu_to_le16(CLEAR_CDB_FIFO_POINTER |
					     AUTOSCSI_START         |
					     AUTO_MSGIN_00_OR_04    |
					     AUTO_MSGIN_02          |
					     AUTO_ATN               );


	
	s = 0;
	switch (data->trans_method) {
	case NSP32_TRANSFER_BUSMASTER:
		s |= BM_START;
		break;
	case NSP32_TRANSFER_MMIO:
		s |= CB_MMIO_MODE;
		break;
	case NSP32_TRANSFER_PIO:
		s |= CB_IO_MODE;
		break;
	default:
		nsp32_msg(KERN_ERR, "unknown trans_method");
		break;
	}
	s |= (TRANSFER_GO | ALL_COUNTER_CLR);
	param->transfer_control = cpu_to_le16(s);

	
	param->sgt_pointer = cpu_to_le32(data->cur_lunt->sglun_paddr);

	nsp32_write4(base, SGT_ADR,         data->auto_paddr);
	nsp32_write2(base, COMMAND_CONTROL, CLEAR_CDB_FIFO_POINTER |
		                            AUTO_PARAMETER         );

	ret = nsp32_arbitration(SCpnt, base);

	return ret;
}


static int nsp32_selection_autoscsi(struct scsi_cmnd *SCpnt)
{
	nsp32_hw_data  *data = (nsp32_hw_data *)SCpnt->device->host->hostdata;
	unsigned int	base    = SCpnt->device->host->io_port;
	unsigned int	host_id = SCpnt->device->host->this_id;
	unsigned char	target  = scmd_id(SCpnt);
	unsigned char	phase;
	int		status;
	unsigned short	command	= 0;
	unsigned int	msgout  = 0;
	unsigned short	execph;
	int		i;

	nsp32_dbg(NSP32_DEBUG_AUTOSCSI, "in");

	nsp32_write2(base, IRQ_CONTROL, IRQ_CONTROL_ALL_IRQ_MASK);

	phase = nsp32_read1(base, SCSI_BUS_MONITOR);
	if(((phase & BUSMON_BSY) == 1) || (phase & BUSMON_SEL) == 1) {
		nsp32_msg(KERN_WARNING, "bus busy");
		SCpnt->result = DID_BUS_BUSY << 16;
		status = 1;
		goto out;
        }

	execph = nsp32_read2(base, SCSI_EXECUTE_PHASE);

	nsp32_write2(base, COMMAND_CONTROL, CLEAR_CDB_FIFO_POINTER);

	for (i = 0; i < SCpnt->cmd_len; i++) {
		nsp32_write1(base, COMMAND_DATA, SCpnt->cmnd[i]);
        }
	nsp32_dbg(NSP32_DEBUG_CDB_CONTENTS, "CDB[0]=[0x%x]", SCpnt->cmnd[0]);

	nsp32_write1(base, SCSI_OUT_LATCH_TARGET_ID, BIT(host_id) | BIT(target));

	if (data->msgout_len == 0) {
		nsp32_msg(KERN_ERR, "SCSI MsgOut without any message!");
		SCpnt->result = DID_ERROR << 16;
		status = 1;
		goto out;
	} else if (data->msgout_len > 0 && data->msgout_len <= 3) {
		msgout = 0;
		for (i = 0; i < data->msgout_len; i++) {
			msgout >>= 8;
			msgout |= ((unsigned int)(data->msgoutbuf[i]) << 24);
		}
		msgout |= MV_VALID;	
		msgout |= (unsigned int)data->msgout_len; 
		nsp32_write4(base, SCSI_MSG_OUT, msgout);
	} else {
		
		nsp32_write4(base, SCSI_MSG_OUT, 0);
	}

	nsp32_write2(base, SEL_TIME_OUT,   SEL_TIMEOUT_TIME);

	nsp32_write1(base, SREQ_SMPL_RATE, data->cur_target->sample_reg);

	nsp32_write1(base, SET_ARBIT,      ARBIT_CLEAR);

	nsp32_write1(base, SYNC_REG,  data->cur_target->syncreg);

	nsp32_write1(base, ACK_WIDTH, data->cur_target->ackwidth);

	nsp32_dbg(NSP32_DEBUG_AUTOSCSI,
		  "syncreg=0x%x, ackwidth=0x%x, sgtpaddr=0x%x, id=0x%x",
		  nsp32_read1(base, SYNC_REG), nsp32_read1(base, ACK_WIDTH),
		  nsp32_read4(base, SGT_ADR), nsp32_read1(base, SCSI_OUT_LATCH_TARGET_ID));
	nsp32_dbg(NSP32_DEBUG_AUTOSCSI, "msgout_len=%d, msgout=0x%x",
		  data->msgout_len, msgout);

	nsp32_write4(base, SGT_ADR, data->cur_lunt->sglun_paddr);

	command = 0;
	command |= (TRANSFER_GO | ALL_COUNTER_CLR);
	if (data->trans_method & NSP32_TRANSFER_BUSMASTER) {
		if (scsi_bufflen(SCpnt) > 0) {
			command |= BM_START;
		}
	} else if (data->trans_method & NSP32_TRANSFER_MMIO) {
		command |= CB_MMIO_MODE;
	} else if (data->trans_method & NSP32_TRANSFER_PIO) {
		command |= CB_IO_MODE;
	}
	nsp32_write2(base, TRANSFER_CONTROL, command);

	command = (CLEAR_CDB_FIFO_POINTER |
		   AUTOSCSI_START         |
		   AUTO_MSGIN_00_OR_04    |
		   AUTO_MSGIN_02          |
		   AUTO_ATN                );
	nsp32_write2(base, COMMAND_CONTROL, command);

	status = nsp32_arbitration(SCpnt, base);

 out:
	nsp32_write2(base, IRQ_CONTROL, 0);

	return status;
}


static int nsp32_arbitration(struct scsi_cmnd *SCpnt, unsigned int base)
{
	unsigned char arbit;
	int	      status = TRUE;
	int	      time   = 0;

	do {
		arbit = nsp32_read1(base, ARBIT_STATUS);
		time++;
	} while ((arbit & (ARBIT_WIN | ARBIT_FAIL)) == 0 &&
		 (time <= ARBIT_TIMEOUT_TIME));

	nsp32_dbg(NSP32_DEBUG_AUTOSCSI,
		  "arbit: 0x%x, delay time: %d", arbit, time);

	if (arbit & ARBIT_WIN) {
		
		SCpnt->result = DID_OK << 16;
		nsp32_index_write1(base, EXT_PORT, LED_ON); 
	} else if (arbit & ARBIT_FAIL) {
		
		SCpnt->result = DID_BUS_BUSY << 16;
		status = FALSE;
	} else {
		nsp32_dbg(NSP32_DEBUG_AUTOSCSI, "arbit timeout");
		SCpnt->result = DID_NO_CONNECT << 16;
		status = FALSE;
        }

	nsp32_write1(base, SET_ARBIT, ARBIT_CLEAR);

	return status;
}


static int nsp32_reselection(struct scsi_cmnd *SCpnt, unsigned char newlun)
{
	nsp32_hw_data *data = (nsp32_hw_data *)SCpnt->device->host->hostdata;
	unsigned int   host_id = SCpnt->device->host->this_id;
	unsigned int   base    = SCpnt->device->host->io_port;
	unsigned char  tmpid, newid;

	nsp32_dbg(NSP32_DEBUG_RESELECTION, "enter");

	tmpid = nsp32_read1(base, RESELECT_ID);
	tmpid &= (~BIT(host_id));
	newid = 0;
	while (tmpid) {
		if (tmpid & 1) {
			break;
		}
		tmpid >>= 1;
		newid++;
	}

	if (newid >= ARRAY_SIZE(data->lunt) || newlun >= ARRAY_SIZE(data->lunt[0])) {
		nsp32_msg(KERN_WARNING, "unknown id/lun");
		return FALSE;
	} else if(data->lunt[newid][newlun].SCpnt == NULL) {
		nsp32_msg(KERN_WARNING, "no SCSI command is processing");
		return FALSE;
	}

	data->cur_id    = newid;
	data->cur_lun   = newlun;
	data->cur_target = &(data->target[newid]);
	data->cur_lunt   = &(data->lunt[newid][newlun]);

	
	nsp32_write4(base, CLR_COUNTER, CLRCOUNTER_ALLMASK);

	return TRUE;
}


static int nsp32_setup_sg_table(struct scsi_cmnd *SCpnt)
{
	nsp32_hw_data *data = (nsp32_hw_data *)SCpnt->device->host->hostdata;
	struct scatterlist *sg;
	nsp32_sgtable *sgt = data->cur_lunt->sglun->sgt;
	int num, i;
	u32_le l;

	if (sgt == NULL) {
		nsp32_dbg(NSP32_DEBUG_SGLIST, "SGT == null");
		return FALSE;
	}

	num = scsi_dma_map(SCpnt);
	if (!num)
		return TRUE;
	else if (num < 0)
		return FALSE;
	else {
		scsi_for_each_sg(SCpnt, sg, num, i) {
			sgt[i].addr = cpu_to_le32(sg_dma_address(sg));
			sgt[i].len  = cpu_to_le32(sg_dma_len(sg));

			if (le32_to_cpu(sgt[i].len) > 0x10000) {
				nsp32_msg(KERN_ERR,
					"can't transfer over 64KB at a time, size=0x%lx", le32_to_cpu(sgt[i].len));
				return FALSE;
			}
			nsp32_dbg(NSP32_DEBUG_SGLIST,
				  "num 0x%x : addr 0x%lx len 0x%lx",
				  i,
				  le32_to_cpu(sgt[i].addr),
				  le32_to_cpu(sgt[i].len ));
		}

		
		l = le32_to_cpu(sgt[num-1].len);
		sgt[num-1].len = cpu_to_le32(l | SGTEND);
	}

	return TRUE;
}

static int nsp32_queuecommand_lck(struct scsi_cmnd *SCpnt, void (*done)(struct scsi_cmnd *))
{
	nsp32_hw_data *data = (nsp32_hw_data *)SCpnt->device->host->hostdata;
	nsp32_target *target;
	nsp32_lunt   *cur_lunt;
	int ret;

	nsp32_dbg(NSP32_DEBUG_QUEUECOMMAND,
		  "enter. target: 0x%x LUN: 0x%x cmnd: 0x%x cmndlen: 0x%x "
		  "use_sg: 0x%x reqbuf: 0x%lx reqlen: 0x%x",
		  SCpnt->device->id, SCpnt->device->lun, SCpnt->cmnd[0], SCpnt->cmd_len,
		  scsi_sg_count(SCpnt), scsi_sglist(SCpnt), scsi_bufflen(SCpnt));

	if (data->CurrentSC != NULL) {
		nsp32_msg(KERN_ERR, "Currentsc != NULL. Cancel this command request");
		data->CurrentSC = NULL;
		SCpnt->result   = DID_NO_CONNECT << 16;
		done(SCpnt);
		return 0;
	}

	
	if (scmd_id(SCpnt) == SCpnt->device->host->this_id) {
		nsp32_dbg(NSP32_DEBUG_QUEUECOMMAND, "terget==host???");
		SCpnt->result = DID_BAD_TARGET << 16;
		done(SCpnt);
		return 0;
	}

	
	if (SCpnt->device->lun >= MAX_LUN) {
		nsp32_dbg(NSP32_DEBUG_QUEUECOMMAND, "no more lun");
		SCpnt->result = DID_BAD_TARGET << 16;
		done(SCpnt);
		return 0;
	}

	show_command(SCpnt);

	SCpnt->scsi_done     = done;
	data->CurrentSC      = SCpnt;
	SCpnt->SCp.Status    = CHECK_CONDITION;
	SCpnt->SCp.Message   = 0;
	scsi_set_resid(SCpnt, scsi_bufflen(SCpnt));

	SCpnt->SCp.ptr		    = (char *)scsi_sglist(SCpnt);
	SCpnt->SCp.this_residual    = scsi_bufflen(SCpnt);
	SCpnt->SCp.buffer	    = NULL;
	SCpnt->SCp.buffers_residual = 0;

	
	data->msgout_len	= 0;
	data->msgin_len		= 0;
	cur_lunt		= &(data->lunt[SCpnt->device->id][SCpnt->device->lun]);
	cur_lunt->SCpnt		= SCpnt;
	cur_lunt->save_datp	= 0;
	cur_lunt->msgin03	= FALSE;
	data->cur_lunt		= cur_lunt;
	data->cur_id		= SCpnt->device->id;
	data->cur_lun		= SCpnt->device->lun;

	ret = nsp32_setup_sg_table(SCpnt);
	if (ret == FALSE) {
		nsp32_msg(KERN_ERR, "SGT fail");
		SCpnt->result = DID_ERROR << 16;
		nsp32_scsi_done(SCpnt);
		return 0;
	}

	
	nsp32_build_identify(SCpnt);

	target = &data->target[scmd_id(SCpnt)];
	data->cur_target = target;

	if (!(target->sync_flag & (SDTR_DONE | SDTR_INITIATOR | SDTR_TARGET))) {
		unsigned char period, offset;

		if (trans_mode != ASYNC_MODE) {
			nsp32_set_max_sync(data, target, &period, &offset);
			nsp32_build_sdtr(SCpnt, period, offset);
			target->sync_flag |= SDTR_INITIATOR;
		} else {
			nsp32_set_async(data, target);
			target->sync_flag |= SDTR_DONE;
		}

		nsp32_dbg(NSP32_DEBUG_QUEUECOMMAND,
			  "SDTR: entry: %d start_period: 0x%x offset: 0x%x\n",
			  target->limit_entry, period, offset);
	} else if (target->sync_flag & SDTR_INITIATOR) {
		nsp32_set_async(data, target);
		target->sync_flag &= ~SDTR_INITIATOR;
		target->sync_flag |= SDTR_DONE;

		nsp32_dbg(NSP32_DEBUG_QUEUECOMMAND,
			  "SDTR_INITIATOR: fall back to async");
	} else if (target->sync_flag & SDTR_TARGET) {
		nsp32_set_async(data, target);
		target->sync_flag &= ~SDTR_TARGET;
		target->sync_flag |= SDTR_DONE;

		nsp32_dbg(NSP32_DEBUG_QUEUECOMMAND,
			  "Unknown SDTR from target is reached, fall back to async.");
	}

	nsp32_dbg(NSP32_DEBUG_TARGETFLAG,
		  "target: %d sync_flag: 0x%x syncreg: 0x%x ackwidth: 0x%x",
		  SCpnt->device->id, target->sync_flag, target->syncreg,
		  target->ackwidth);

	
	if (auto_param == 0) {
		ret = nsp32_selection_autopara(SCpnt);
	} else {
		ret = nsp32_selection_autoscsi(SCpnt);
	}

	if (ret != TRUE) {
		nsp32_dbg(NSP32_DEBUG_QUEUECOMMAND, "selection fail");
		nsp32_scsi_done(SCpnt);
	}

	return 0;
}

static DEF_SCSI_QCMD(nsp32_queuecommand)

static int nsp32hw_init(nsp32_hw_data *data)
{
	unsigned int   base = data->BaseAddress;
	unsigned short irq_stat;
	unsigned long  lc_reg;
	unsigned char  power;

	lc_reg = nsp32_index_read4(base, CFG_LATE_CACHE);
	if ((lc_reg & 0xff00) == 0) {
		lc_reg |= (0x20 << 8);
		nsp32_index_write2(base, CFG_LATE_CACHE, lc_reg & 0xffff);
	}

	nsp32_write2(base, IRQ_CONTROL,        IRQ_CONTROL_ALL_IRQ_MASK);
	nsp32_write2(base, TRANSFER_CONTROL,   0);
	nsp32_write4(base, BM_CNT,             0);
	nsp32_write2(base, SCSI_EXECUTE_PHASE, 0);

	do {
		irq_stat = nsp32_read2(base, IRQ_STATUS);
		nsp32_dbg(NSP32_DEBUG_INIT, "irq_stat 0x%x", irq_stat);
	} while (irq_stat & IRQSTATUS_ANY_IRQ);

	if ((data->trans_method & NSP32_TRANSFER_PIO) ||
	    (data->trans_method & NSP32_TRANSFER_MMIO)) {
		nsp32_index_write1(base, FIFO_FULL_SHLD_COUNT,  0x40);
		nsp32_index_write1(base, FIFO_EMPTY_SHLD_COUNT, 0x40);
	} else if (data->trans_method & NSP32_TRANSFER_BUSMASTER) {
		nsp32_index_write1(base, FIFO_FULL_SHLD_COUNT,  0x10);
		nsp32_index_write1(base, FIFO_EMPTY_SHLD_COUNT, 0x60);
	} else {
		nsp32_dbg(NSP32_DEBUG_INIT, "unknown transfer mode");
	}

	nsp32_dbg(NSP32_DEBUG_INIT, "full 0x%x emp 0x%x",
		  nsp32_index_read1(base, FIFO_FULL_SHLD_COUNT),
		  nsp32_index_read1(base, FIFO_EMPTY_SHLD_COUNT));

	nsp32_index_write1(base, CLOCK_DIV, data->clock);
	nsp32_index_write1(base, BM_CYCLE,  MEMRD_CMD1 | SGT_AUTO_PARA_MEMED_CMD);
	nsp32_write1(base, PARITY_CONTROL, 0);	

	nsp32_index_write2(base, MISC_WR,
			   (SCSI_DIRECTION_DETECTOR_SELECT |
			    DELAYED_BMSTART                |
			    MASTER_TERMINATION_SELECT      |
			    BMREQ_NEGATE_TIMING_SEL        |
			    AUTOSEL_TIMING_SEL             |
			    BMSTOP_CHANGE2_NONDATA_PHASE));

	nsp32_index_write1(base, TERM_PWR_CONTROL, 0);
	power = nsp32_index_read1(base, TERM_PWR_CONTROL);
	if (!(power & SENSE)) {
		nsp32_msg(KERN_INFO, "term power on");
		nsp32_index_write1(base, TERM_PWR_CONTROL, BPWR);
	}

	nsp32_write2(base, TIMER_SET, TIMER_STOP);
	nsp32_write2(base, TIMER_SET, TIMER_STOP); 

	nsp32_write1(base, SYNC_REG,     0);
	nsp32_write1(base, ACK_WIDTH,    0);
	nsp32_write2(base, SEL_TIME_OUT, SEL_TIMEOUT_TIME);

	nsp32_index_write2(base, IRQ_SELECT, IRQSELECT_TIMER_IRQ         |
			                     IRQSELECT_SCSIRESET_IRQ     |
			                     IRQSELECT_FIFO_SHLD_IRQ     |
			                     IRQSELECT_RESELECT_IRQ      |
			                     IRQSELECT_PHASE_CHANGE_IRQ  |
			                     IRQSELECT_AUTO_SCSI_SEQ_IRQ |
			                  
			                     IRQSELECT_TARGET_ABORT_IRQ  |
			                     IRQSELECT_MASTER_ABORT_IRQ );
	nsp32_write2(base, IRQ_CONTROL, 0);

	
	nsp32_index_write1(base, EXT_PORT_DDR, LED_OFF);
	nsp32_index_write1(base, EXT_PORT,     LED_OFF);

	return TRUE;
}


static irqreturn_t do_nsp32_isr(int irq, void *dev_id)
{
	nsp32_hw_data *data = dev_id;
	unsigned int base = data->BaseAddress;
	struct scsi_cmnd *SCpnt = data->CurrentSC;
	unsigned short auto_stat, irq_stat, trans_stat;
	unsigned char busmon, busphase;
	unsigned long flags;
	int ret;
	int handled = 0;
	struct Scsi_Host *host = data->Host;

	spin_lock_irqsave(host->host_lock, flags);

	irq_stat = nsp32_read2(base, IRQ_STATUS);
	nsp32_dbg(NSP32_DEBUG_INTR, 
		  "enter IRQ: %d, IRQstatus: 0x%x", irq, irq_stat);
	
	if ((irq_stat & IRQSTATUS_ANY_IRQ) == 0) {
		nsp32_dbg(NSP32_DEBUG_INTR, "shared interrupt: irq other 0x%x", irq_stat);
		goto out2;
	}
	handled = 1;
	nsp32_write2(base, IRQ_CONTROL, IRQ_CONTROL_ALL_IRQ_MASK);

	busmon = nsp32_read1(base, SCSI_BUS_MONITOR);
	busphase = busmon & BUSMON_PHASE_MASK;

	trans_stat = nsp32_read2(base, TRANSFER_STATUS);
	if ((irq_stat == 0xffff) && (trans_stat == 0xffff)) {
		nsp32_msg(KERN_INFO, "card disconnect");
		if (data->CurrentSC != NULL) {
			nsp32_msg(KERN_INFO, "clean up current SCSI command");
			SCpnt->result = DID_BAD_TARGET << 16;
			nsp32_scsi_done(SCpnt);
		}
		goto out;
	}

	
	if (irq_stat & IRQSTATUS_TIMER_IRQ) {
		nsp32_dbg(NSP32_DEBUG_INTR, "timer stop");
		nsp32_write2(base, TIMER_SET, TIMER_STOP);
		goto out;
	}

	
	if (irq_stat & IRQSTATUS_SCSIRESET_IRQ) {
		nsp32_msg(KERN_INFO, "detected someone do bus reset");
		nsp32_do_bus_reset(data);
		if (SCpnt != NULL) {
			SCpnt->result = DID_RESET << 16;
			nsp32_scsi_done(SCpnt);
		}
		goto out;
	}

	if (SCpnt == NULL) {
		nsp32_msg(KERN_WARNING, "SCpnt==NULL this can't be happened");
		nsp32_msg(KERN_WARNING, "irq_stat=0x%x trans_stat=0x%x", irq_stat, trans_stat);
		goto out;
	}

	if(irq_stat & IRQSTATUS_AUTOSCSI_IRQ) {
		
		auto_stat = nsp32_read2(base, SCSI_EXECUTE_PHASE);
		nsp32_write2(base, SCSI_EXECUTE_PHASE, 0);

		
		if (auto_stat & SELECTION_TIMEOUT) {
			nsp32_dbg(NSP32_DEBUG_INTR,
				  "selection timeout occurred");

			SCpnt->result = DID_TIME_OUT << 16;
			nsp32_scsi_done(SCpnt);
			goto out;
		}

		if (auto_stat & MSGOUT_PHASE) {
			if (!(auto_stat & MSG_IN_OCCUER) &&
			     (data->msgout_len <= 3)) {
				data->msgout_len = 0;
			};

			nsp32_dbg(NSP32_DEBUG_INTR, "MsgOut phase processed");
		}

		if ((auto_stat & DATA_IN_PHASE) &&
		    (scsi_get_resid(SCpnt) > 0) &&
		    ((nsp32_read2(base, FIFO_REST_CNT) & FIFO_REST_MASK) != 0)) {
			printk( "auto+fifo\n");
			
		}

		if (auto_stat & (DATA_IN_PHASE | DATA_OUT_PHASE)) {
			
			nsp32_dbg(NSP32_DEBUG_INTR,
				  "Data in/out phase processed");

			
			nsp32_dbg(NSP32_DEBUG_INTR, "BMCNT=0x%lx", 
				    nsp32_read4(base, BM_CNT));
			nsp32_dbg(NSP32_DEBUG_INTR, "addr=0x%lx", 
				    nsp32_read4(base, SGT_ADR));
			nsp32_dbg(NSP32_DEBUG_INTR, "SACK=0x%lx", 
				    nsp32_read4(base, SACK_CNT));
			nsp32_dbg(NSP32_DEBUG_INTR, "SSACK=0x%lx", 
				    nsp32_read4(base, SAVED_SACK_CNT));

			scsi_set_resid(SCpnt, 0); 
		}

		if (auto_stat & MSG_IN_OCCUER) {
			nsp32_msgin_occur(SCpnt, irq_stat, auto_stat);
		}

		if (auto_stat & MSG_OUT_OCCUER) {
			nsp32_msgout_occur(SCpnt);
		}

		if (auto_stat & BUS_FREE_OCCUER) {
			ret = nsp32_busfree_occur(SCpnt, auto_stat);
			if (ret == TRUE) {
				goto out;
			}
		}

		if (auto_stat & STATUS_PHASE) {
			SCpnt->result =	(int)nsp32_read1(base, SCSI_CSB_IN);
		}

		if (auto_stat & ILLEGAL_PHASE) {
			
			nsp32_msg(KERN_WARNING, 
				  "AUTO SCSI ILLEGAL PHASE OCCUR!!!!");

			

			nsp32_sack_assert(data);
			nsp32_wait_req(data, NEGATE);
			nsp32_sack_negate(data);

		}

		if (auto_stat & COMMAND_PHASE) {
			
			nsp32_dbg(NSP32_DEBUG_INTR, "Command phase processed");
		}

		if (auto_stat & AUTOSCSI_BUSY) {
			
		}

		show_autophase(auto_stat);
	}

	
	if (irq_stat & IRQSTATUS_FIFO_SHLD_IRQ) {
		nsp32_dbg(NSP32_DEBUG_INTR, "FIFO IRQ");

		switch(busphase) {
		case BUSPHASE_DATA_OUT:
			nsp32_dbg(NSP32_DEBUG_INTR, "fifo/write");

			

			break;

		case BUSPHASE_DATA_IN:
			nsp32_dbg(NSP32_DEBUG_INTR, "fifo/read");

			

			break;

		case BUSPHASE_STATUS:
			nsp32_dbg(NSP32_DEBUG_INTR, "fifo/status");

			SCpnt->SCp.Status = nsp32_read1(base, SCSI_CSB_IN);

			break;
		default:
			nsp32_dbg(NSP32_DEBUG_INTR, "fifo/other phase");
			nsp32_dbg(NSP32_DEBUG_INTR, "irq_stat=0x%x trans_stat=0x%x", irq_stat, trans_stat);
			show_busphase(busphase);
			break;
		}

		goto out;
	}

	
	if (irq_stat & IRQSTATUS_PHASE_CHANGE_IRQ) {
		nsp32_dbg(NSP32_DEBUG_INTR, "phase change IRQ");

		switch(busphase) {
		case BUSPHASE_MESSAGE_IN:
			nsp32_dbg(NSP32_DEBUG_INTR, "phase chg/msg in");
			nsp32_msgin_occur(SCpnt, irq_stat, 0);
			break;
		default:
			nsp32_msg(KERN_WARNING, "phase chg/other phase?");
			nsp32_msg(KERN_WARNING, "irq_stat=0x%x trans_stat=0x%x\n",
				  irq_stat, trans_stat);
			show_busphase(busphase);
			break;
		}
		goto out;
	}

	
	if (irq_stat & IRQSTATUS_PCI_IRQ) {
		nsp32_dbg(NSP32_DEBUG_INTR, "PCI IRQ occurred");
		
	}

	
	if (irq_stat & IRQSTATUS_BMCNTERR_IRQ) {
		nsp32_msg(KERN_ERR, "Received unexpected BMCNTERR IRQ! ");
	}

#if 0
	nsp32_dbg(NSP32_DEBUG_INTR,
		  "irq_stat=0x%x trans_stat=0x%x", irq_stat, trans_stat);
	show_busphase(busphase);
#endif

 out:
	
	nsp32_write2(base, IRQ_CONTROL, 0);

 out2:
	spin_unlock_irqrestore(host->host_lock, flags);

	nsp32_dbg(NSP32_DEBUG_INTR, "exit");

	return IRQ_RETVAL(handled);
}

#undef SPRINTF
#define SPRINTF(args...) \
	do { \
		if(length > (pos - buffer)) { \
			pos += snprintf(pos, length - (pos - buffer) + 1, ## args); \
			nsp32_dbg(NSP32_DEBUG_PROC, "buffer=0x%p pos=0x%p length=%d %d\n", buffer, pos, length,  length - (pos - buffer));\
		} \
	} while(0)

static int nsp32_proc_info(struct Scsi_Host *host, char *buffer, char **start,
			   off_t offset, int length, int inout)
{
	char             *pos = buffer;
	int               thislength;
	unsigned long     flags;
	nsp32_hw_data    *data;
	int               hostno;
	unsigned int      base;
	unsigned char     mode_reg;
	int               id, speed;
	long              model;

	
	if (inout == TRUE) {
		return -EINVAL;
	}

	hostno = host->host_no;
	data = (nsp32_hw_data *)host->hostdata;
	base = host->io_port;

	SPRINTF("NinjaSCSI-32 status\n\n");
	SPRINTF("Driver version:        %s, $Revision: 1.33 $\n", nsp32_release_version);
	SPRINTF("SCSI host No.:         %d\n",		hostno);
	SPRINTF("IRQ:                   %d\n",		host->irq);
	SPRINTF("IO:                    0x%lx-0x%lx\n", host->io_port, host->io_port + host->n_io_port - 1);
	SPRINTF("MMIO(virtual address): 0x%lx-0x%lx\n",	host->base, host->base + data->MmioLength - 1);
	SPRINTF("sg_tablesize:          %d\n",		host->sg_tablesize);
	SPRINTF("Chip revision:         0x%x\n",       	(nsp32_read2(base, INDEX_REG) >> 8) & 0xff);

	mode_reg = nsp32_index_read1(base, CHIP_MODE);
	model    = data->pci_devid->driver_data;

#ifdef CONFIG_PM
	SPRINTF("Power Management:      %s\n",          (mode_reg & OPTF) ? "yes" : "no");
#endif
	SPRINTF("OEM:                   %ld, %s\n",     (mode_reg & (OEM0|OEM1)), nsp32_model[model]);

	spin_lock_irqsave(&(data->Lock), flags);
	SPRINTF("CurrentSC:             0x%p\n\n",      data->CurrentSC);
	spin_unlock_irqrestore(&(data->Lock), flags);


	SPRINTF("SDTR status\n");
	for (id = 0; id < ARRAY_SIZE(data->target); id++) {

                SPRINTF("id %d: ", id);

		if (id == host->this_id) {
			SPRINTF("----- NinjaSCSI-32 host adapter\n");
			continue;
		}

		if (data->target[id].sync_flag == SDTR_DONE) {
			if (data->target[id].period == 0            &&
			    data->target[id].offset == ASYNC_OFFSET ) {
				SPRINTF("async");
			} else {
				SPRINTF(" sync");
			}
		} else {
			SPRINTF(" none");
		}

		if (data->target[id].period != 0) {

			speed = 1000000 / (data->target[id].period * 4);

			SPRINTF(" transfer %d.%dMB/s, offset %d",
				speed / 1000,
				speed % 1000,
				data->target[id].offset
				);
		}
		SPRINTF("\n");
	}


	thislength = pos - (buffer + offset);

	if(thislength < 0) {
		*start = NULL;
                return 0;
        }


	thislength = min(thislength, length);
	*start = buffer + offset;

	return thislength;
}
#undef SPRINTF



static void nsp32_scsi_done(struct scsi_cmnd *SCpnt)
{
	nsp32_hw_data *data = (nsp32_hw_data *)SCpnt->device->host->hostdata;
	unsigned int   base = SCpnt->device->host->io_port;

	scsi_dma_unmap(SCpnt);

	nsp32_write2(base, TRANSFER_CONTROL, 0);
	nsp32_write4(base, BM_CNT,           0);

	(*SCpnt->scsi_done)(SCpnt);

	data->cur_lunt->SCpnt = NULL;
	data->cur_lunt        = NULL;
	data->cur_target      = NULL;
	data->CurrentSC      = NULL;
}


static int nsp32_busfree_occur(struct scsi_cmnd *SCpnt, unsigned short execph)
{
	nsp32_hw_data *data = (nsp32_hw_data *)SCpnt->device->host->hostdata;
	unsigned int base   = SCpnt->device->host->io_port;

	nsp32_dbg(NSP32_DEBUG_BUSFREE, "enter execph=0x%x", execph);
	show_autophase(execph);

	nsp32_write4(base, BM_CNT,           0);
	nsp32_write2(base, TRANSFER_CONTROL, 0);

	if (execph & MSGIN_02_VALID) {
		nsp32_dbg(NSP32_DEBUG_BUSFREE, "MsgIn02_Valid");

		if (!(execph & MSGIN_00_VALID) && 
		    ((execph & DATA_IN_PHASE) || (execph & DATA_OUT_PHASE))) {
			unsigned int sacklen, s_sacklen;

			sacklen   = nsp32_read4(base, SACK_CNT      );
			s_sacklen = nsp32_read4(base, SAVED_SACK_CNT);

			if (s_sacklen > 0) {
				if (sacklen != s_sacklen) {
					data->cur_lunt->msgin03 = FALSE;
				} else {
					data->cur_lunt->msgin03 = TRUE;
				}

				nsp32_adjust_busfree(SCpnt, s_sacklen);
			}
		}

		
		
	} else {
	}
	
	if (execph & MSGIN_03_VALID) {
		
	}

	if (data->cur_target->sync_flag & SDTR_INITIATOR) {
		nsp32_set_async(data, data->cur_target);
		data->cur_target->sync_flag &= ~SDTR_INITIATOR;
		data->cur_target->sync_flag |= SDTR_DONE;
	} else if (data->cur_target->sync_flag & SDTR_TARGET) {
		if (execph & (MSGIN_00_VALID | MSGIN_04_VALID)) {
		} else {
			nsp32_set_async(data, data->cur_target);
		}
		data->cur_target->sync_flag &= ~SDTR_TARGET;
		data->cur_target->sync_flag |= SDTR_DONE;
	}

	if (execph & MSGIN_00_VALID) {
		
		nsp32_dbg(NSP32_DEBUG_BUSFREE, "command complete");

		SCpnt->SCp.Status  = nsp32_read1(base, SCSI_CSB_IN);
		SCpnt->SCp.Message = 0;
		nsp32_dbg(NSP32_DEBUG_BUSFREE, 
			  "normal end stat=0x%x resid=0x%x\n",
			  SCpnt->SCp.Status, scsi_get_resid(SCpnt));
		SCpnt->result = (DID_OK             << 16) |
			        (SCpnt->SCp.Message <<  8) |
			        (SCpnt->SCp.Status  <<  0);
		nsp32_scsi_done(SCpnt);
		
		return TRUE;
	} else if (execph & MSGIN_04_VALID) {
		
		SCpnt->SCp.Status  = nsp32_read1(base, SCSI_CSB_IN);
		SCpnt->SCp.Message = 4;
		
		nsp32_dbg(NSP32_DEBUG_BUSFREE, "disconnect");
		return TRUE;
	} else {
		
		nsp32_msg(KERN_WARNING, "unexpected bus free occurred");

		
		
		SCpnt->result = DID_ERROR << 16;
		nsp32_scsi_done(SCpnt);
		return TRUE;
	}
	return FALSE;
}


static void nsp32_adjust_busfree(struct scsi_cmnd *SCpnt, unsigned int s_sacklen)
{
	nsp32_hw_data *data = (nsp32_hw_data *)SCpnt->device->host->hostdata;
	int                   old_entry = data->cur_entry;
	int                   new_entry;
	int                   sg_num = data->cur_lunt->sg_num;
	nsp32_sgtable *sgt    = data->cur_lunt->sglun->sgt;
	unsigned int          restlen, sentlen;
	u32_le                len, addr;

	nsp32_dbg(NSP32_DEBUG_SGLIST, "old resid=0x%x", scsi_get_resid(SCpnt));

	
	s_sacklen -= le32_to_cpu(sgt[old_entry].addr) & 3;

	sentlen = 0;
	for (new_entry = old_entry; new_entry < sg_num; new_entry++) {
		sentlen += (le32_to_cpu(sgt[new_entry].len) & ~SGTEND);
		if (sentlen > s_sacklen) {
			break;
		}
	}

	
	if (new_entry == sg_num) {
		goto last;
	}

	if (sentlen == s_sacklen) {
		
	}

	
	restlen = sentlen - s_sacklen;

	
	len  = le32_to_cpu(sgt[new_entry].len);
	addr = le32_to_cpu(sgt[new_entry].addr);
	addr += (len - restlen);
	sgt[new_entry].addr = cpu_to_le32(addr);
	sgt[new_entry].len  = cpu_to_le32(restlen);

	
	data->cur_entry = new_entry;
 
	return;

 last:
	if (scsi_get_resid(SCpnt) < sentlen) {
		nsp32_msg(KERN_ERR, "resid underflow");
	}

	scsi_set_resid(SCpnt, scsi_get_resid(SCpnt) - sentlen);
	nsp32_dbg(NSP32_DEBUG_SGLIST, "new resid=0x%x", scsi_get_resid(SCpnt));

	

	return;
}


static void nsp32_msgout_occur(struct scsi_cmnd *SCpnt)
{
	nsp32_hw_data *data = (nsp32_hw_data *)SCpnt->device->host->hostdata;
	unsigned int base   = SCpnt->device->host->io_port;
	
	long new_sgtp;
	int i;
	
	nsp32_dbg(NSP32_DEBUG_MSGOUTOCCUR,
		  "enter: msgout_len: 0x%x", data->msgout_len);

	if (data->msgout_len == 0) {
		nsp32_build_nop(SCpnt);
	}

 	new_sgtp = data->cur_lunt->sglun_paddr + 
		   (data->cur_lunt->cur_entry * sizeof(nsp32_sgtable));

	for (i = 0; i < data->msgout_len; i++) {
		nsp32_dbg(NSP32_DEBUG_MSGOUTOCCUR,
			  "%d : 0x%x", i, data->msgoutbuf[i]);

		nsp32_wait_req(data, ASSERT);

		if (i == (data->msgout_len - 1)) {
			
			
			nsp32_write2(base, COMMAND_CONTROL,
					 (CLEAR_CDB_FIFO_POINTER |
					  AUTO_COMMAND_PHASE     |
					  AUTOSCSI_RESTART       |
					  AUTO_MSGIN_00_OR_04    |
					  AUTO_MSGIN_02          ));
		}
		nsp32_write1(base, SCSI_DATA_WITH_ACK, data->msgoutbuf[i]);
		nsp32_wait_sack(data, NEGATE);

		nsp32_dbg(NSP32_DEBUG_MSGOUTOCCUR, "bus: 0x%x\n",
			  nsp32_read1(base, SCSI_BUS_MONITOR));
	};

	data->msgout_len = 0;

	nsp32_dbg(NSP32_DEBUG_MSGOUTOCCUR, "exit");
}

static void nsp32_restart_autoscsi(struct scsi_cmnd *SCpnt, unsigned short command)
{
	nsp32_hw_data *data = (nsp32_hw_data *)SCpnt->device->host->hostdata;
	unsigned int   base = data->BaseAddress;
	unsigned short transfer = 0;

	nsp32_dbg(NSP32_DEBUG_RESTART, "enter");

	if (data->cur_target == NULL || data->cur_lunt == NULL) {
		nsp32_msg(KERN_ERR, "Target or Lun is invalid");
	}

	nsp32_write1(base, SYNC_REG, data->cur_target->syncreg);

	nsp32_write1(base, ACK_WIDTH, data->cur_target->ackwidth);

	nsp32_write1(base, SREQ_SMPL_RATE, data->cur_target->sample_reg);

	nsp32_write4(base, SGT_ADR, data->cur_lunt->sglun_paddr);

	transfer = 0;
	transfer |= (TRANSFER_GO | ALL_COUNTER_CLR);
	if (data->trans_method & NSP32_TRANSFER_BUSMASTER) {
		if (scsi_bufflen(SCpnt) > 0) {
			transfer |= BM_START;
		}
	} else if (data->trans_method & NSP32_TRANSFER_MMIO) {
		transfer |= CB_MMIO_MODE;
	} else if (data->trans_method & NSP32_TRANSFER_PIO) {
		transfer |= CB_IO_MODE;
	}
	nsp32_write2(base, TRANSFER_CONTROL, transfer);

	command |= (CLEAR_CDB_FIFO_POINTER |
		    AUTO_COMMAND_PHASE     |
		    AUTOSCSI_RESTART       );
	nsp32_write2(base, COMMAND_CONTROL, command);

	nsp32_dbg(NSP32_DEBUG_RESTART, "exit");
}


static void nsp32_msgin_occur(struct scsi_cmnd     *SCpnt,
			      unsigned long  irq_status,
			      unsigned short execph)
{
	nsp32_hw_data *data = (nsp32_hw_data *)SCpnt->device->host->hostdata;
	unsigned int   base = SCpnt->device->host->io_port;
	unsigned char  msg;
	unsigned char  msgtype;
	unsigned char  newlun;
	unsigned short command  = 0;
	int            msgclear = TRUE;
	long           new_sgtp;
	int            ret;

	msg = nsp32_read1(base, SCSI_DATA_IN);
	data->msginbuf[(unsigned char)data->msgin_len] = msg;
	msgtype = data->msginbuf[0];
	nsp32_dbg(NSP32_DEBUG_MSGINOCCUR,
		  "enter: msglen: 0x%x msgin: 0x%x msgtype: 0x%x",
		  data->msgin_len, msg, msgtype);


	nsp32_sack_assert(data);

	if (msgtype & 0x80) {
		if (!(irq_status & IRQSTATUS_RESELECT_OCCUER)) {
			
			goto reject;
		}

		newlun = msgtype & 0x1f; 
		ret = nsp32_reselection(SCpnt, newlun);
		if (ret == TRUE) {
			goto restart;
		} else {
			goto reject;
		}
	}
	
	switch (msgtype) {
	case COMMAND_COMPLETE:
	case DISCONNECT:
		nsp32_msg(KERN_WARNING, 
			   "unexpected message of AutoSCSI MsgIn: 0x%x", msg);
		break;
		
	case RESTORE_POINTERS:

		if ((execph & DATA_IN_PHASE) || (execph & DATA_OUT_PHASE)) {
			unsigned int s_sacklen;

			s_sacklen = nsp32_read4(base, SAVED_SACK_CNT);
			if ((execph & MSGIN_02_VALID) && (s_sacklen > 0)) {
				nsp32_adjust_busfree(SCpnt, s_sacklen);
			} else {
				
			}
		}
		data->cur_lunt->msgin03 = FALSE;

		

		
		nsp32_write4(base, CLR_COUNTER, CLRCOUNTER_ALLMASK);

		new_sgtp = data->cur_lunt->sglun_paddr + 
			(data->cur_lunt->cur_entry * sizeof(nsp32_sgtable));
		nsp32_write4(base, SGT_ADR, new_sgtp);

		break;

	case SAVE_POINTERS:
		nsp32_msg (KERN_WARNING, 
			   "unexpected message of AutoSCSI MsgIn: SAVE_POINTERS");
		
		break;
		
	case MESSAGE_REJECT:
		if (data->cur_target->sync_flag &
				(SDTR_INITIATOR | SDTR_TARGET)) {
			nsp32_set_async(data, data->cur_target);
			data->cur_target->sync_flag &= ~SDTR_INITIATOR;
			data->cur_target->sync_flag |= SDTR_DONE;

		}
		break;

	case LINKED_CMD_COMPLETE:
	case LINKED_FLG_CMD_COMPLETE:
		
		nsp32_msg (KERN_WARNING, 
			   "unsupported message: 0x%x", msgtype);
		break;

	case INITIATE_RECOVERY:
		
		

		goto reject;

	case SIMPLE_QUEUE_TAG:
	case 0x23:
		if (data->msgin_len >= 1) {
			goto reject;
		}

		
		msgclear = FALSE;

		break;

	case EXTENDED_MESSAGE:
		if (data->msgin_len < 1) {
			msgclear = FALSE;
			break;
		}

		if ((data->msginbuf[1] + 1) > data->msgin_len) {
			msgclear = FALSE;
			break;
		}

		switch (data->msginbuf[2]) {
		case EXTENDED_MODIFY_DATA_POINTER:
			
			goto reject; 
			break;

		case EXTENDED_SDTR:
			if (data->msgin_len != EXTENDED_SDTR_LEN + 1) {
				goto reject;
				break;
			}

			nsp32_analyze_sdtr(SCpnt);

			break;

		case EXTENDED_EXTENDED_IDENTIFY:
			
			goto reject; 

			break;

		case EXTENDED_WDTR:
			goto reject; 

			break;
			
		default:
			goto reject;
		}
		break;
		
	default:
		goto reject;
	}

 restart:
	if (msgclear == TRUE) {
		data->msgin_len = 0;

		/*
		 * If restarting AutoSCSI, but there are some message to out
		 * (msgout_len > 0), set AutoATN, and set SCSIMSGOUT as 0
		 * (MV_VALID = 0). When commandcontrol is written with
		 * AutoSCSI restart, at the same time MsgOutOccur should be
		 * happened (however, such situation is really possible...?).
		 */
		if (data->msgout_len > 0) {	
			nsp32_write4(base, SCSI_MSG_OUT, 0);
			command |= AUTO_ATN;
		}

		command |= (AUTO_MSGIN_00_OR_04 | AUTO_MSGIN_02);

		if (data->cur_lunt->msgin03 == TRUE) {
			command |= AUTO_MSGIN_03;
		}
		data->cur_lunt->msgin03 = FALSE;
	} else {
		data->msgin_len++;
	}

	nsp32_restart_autoscsi(SCpnt, command);

	nsp32_wait_req(data, NEGATE);

	nsp32_sack_negate(data);

	nsp32_dbg(NSP32_DEBUG_MSGINOCCUR, "exit");

	return;

 reject:
	nsp32_msg(KERN_WARNING, 
		  "invalid or unsupported MessageIn, rejected. "
		  "current msg: 0x%x (len: 0x%x), processing msg: 0x%x",
		  msg, data->msgin_len, msgtype);
	nsp32_build_reject(SCpnt);
	data->msgin_len = 0;

	goto restart;
}

static void nsp32_analyze_sdtr(struct scsi_cmnd *SCpnt)
{
	nsp32_hw_data   *data = (nsp32_hw_data *)SCpnt->device->host->hostdata;
	nsp32_target     *target     = data->cur_target;
	nsp32_sync_table *synct;
	unsigned char     get_period = data->msginbuf[3];
	unsigned char     get_offset = data->msginbuf[4];
	int               entry;
	int               syncnum;

	nsp32_dbg(NSP32_DEBUG_MSGINOCCUR, "enter");

	synct   = data->synct;
	syncnum = data->syncnum;

	
	if (target->sync_flag & SDTR_INITIATOR) {
		nsp32_dbg(NSP32_DEBUG_MSGINOCCUR, "target responds SDTR");
	
		target->sync_flag &= ~SDTR_INITIATOR;
		target->sync_flag |= SDTR_DONE;

		if (get_offset > SYNC_OFFSET) {
			goto reject;
		}
		
		if (get_offset == ASYNC_OFFSET) {
			goto async;
		}

		if (get_period < data->synct[0].period_num) {
			goto reject;
		}

		entry = nsp32_search_period_entry(data, target, get_period);

		if (entry < 0) {
			goto reject;
		}

		nsp32_set_sync_entry(data, target, entry, get_offset);
	} else {
		
		nsp32_dbg(NSP32_DEBUG_MSGINOCCUR, "target send SDTR");
	
		target->sync_flag |= SDTR_INITIATOR;

		
		if (get_offset > SYNC_OFFSET) {
			
			get_offset = SYNC_OFFSET;
		}

		
		if (get_period < data->synct[0].period_num) {
			get_period = data->synct[0].period_num;
		}

		entry = nsp32_search_period_entry(data, target, get_period);

		if (get_offset == ASYNC_OFFSET || entry < 0) {
			nsp32_set_async(data, target);
			nsp32_build_sdtr(SCpnt, 0, ASYNC_OFFSET);
		} else {
			nsp32_set_sync_entry(data, target, entry, get_offset);
			nsp32_build_sdtr(SCpnt, get_period, get_offset);
		}
	}

	target->period = get_period;
	nsp32_dbg(NSP32_DEBUG_MSGINOCCUR, "exit");
	return;

 reject:
	nsp32_build_reject(SCpnt);

 async:
	nsp32_set_async(data, target);	

	target->period = 0;
	nsp32_dbg(NSP32_DEBUG_MSGINOCCUR, "exit: set async");
	return;
}


static int nsp32_search_period_entry(nsp32_hw_data *data,
				     nsp32_target  *target,
				     unsigned char  period)
{
	int i;

	if (target->limit_entry >= data->syncnum) {
		nsp32_msg(KERN_ERR, "limit_entry exceeds syncnum!");
		target->limit_entry = 0;
	}

	for (i = target->limit_entry; i < data->syncnum; i++) {
		if (period >= data->synct[i].start_period &&
		    period <= data->synct[i].end_period) {
				break;
		}
	}

	if (i == data->syncnum) {
		i = -1;
	}

	return i;
}


static void nsp32_set_async(nsp32_hw_data *data, nsp32_target *target)
{
	unsigned char period = data->synct[target->limit_entry].period_num;

	target->offset     = ASYNC_OFFSET;
	target->period     = 0;
	target->syncreg    = TO_SYNCREG(period, ASYNC_OFFSET);
	target->ackwidth   = 0;
	target->sample_reg = 0;

	nsp32_dbg(NSP32_DEBUG_SYNC, "set async");
}


static void nsp32_set_max_sync(nsp32_hw_data *data,
			       nsp32_target  *target,
			       unsigned char *period,
			       unsigned char *offset)
{
	unsigned char period_num, ackwidth;

	period_num = data->synct[target->limit_entry].period_num;
	*period    = data->synct[target->limit_entry].start_period;
	ackwidth   = data->synct[target->limit_entry].ackwidth;
	*offset    = SYNC_OFFSET;

	target->syncreg    = TO_SYNCREG(period_num, *offset);
	target->ackwidth   = ackwidth;
	target->offset     = *offset;
	target->sample_reg = 0;       
}


static void nsp32_set_sync_entry(nsp32_hw_data *data,
				 nsp32_target  *target,
				 int            entry,
				 unsigned char  offset)
{
	unsigned char period, ackwidth, sample_rate;

	period      = data->synct[entry].period_num;
	ackwidth    = data->synct[entry].ackwidth;
	offset      = offset;
	sample_rate = data->synct[entry].sample_rate;

	target->syncreg    = TO_SYNCREG(period, offset);
	target->ackwidth   = ackwidth;
	target->offset     = offset;
	target->sample_reg = sample_rate | SAMPLING_ENABLE;

	nsp32_dbg(NSP32_DEBUG_SYNC, "set sync");
}


static void nsp32_wait_req(nsp32_hw_data *data, int state)
{
	unsigned int  base      = data->BaseAddress;
	int           wait_time = 0;
	unsigned char bus, req_bit;

	if (!((state == ASSERT) || (state == NEGATE))) {
		nsp32_msg(KERN_ERR, "unknown state designation");
	}
	
	req_bit = (state == ASSERT ? BUSMON_REQ : 0);

	do {
		bus = nsp32_read1(base, SCSI_BUS_MONITOR);
		if ((bus & BUSMON_REQ) == req_bit) {
			nsp32_dbg(NSP32_DEBUG_WAIT, 
				  "wait_time: %d", wait_time);
			return;
		}
		udelay(1);
		wait_time++;
	} while (wait_time < REQSACK_TIMEOUT_TIME);

	nsp32_msg(KERN_WARNING, "wait REQ timeout, req_bit: 0x%x", req_bit);
}

static void nsp32_wait_sack(nsp32_hw_data *data, int state)
{
	unsigned int  base      = data->BaseAddress;
	int           wait_time = 0;
	unsigned char bus, ack_bit;

	if (!((state == ASSERT) || (state == NEGATE))) {
		nsp32_msg(KERN_ERR, "unknown state designation");
	}
	
	ack_bit = (state == ASSERT ? BUSMON_ACK : 0);

	do {
		bus = nsp32_read1(base, SCSI_BUS_MONITOR);
		if ((bus & BUSMON_ACK) == ack_bit) {
			nsp32_dbg(NSP32_DEBUG_WAIT,
				  "wait_time: %d", wait_time);
			return;
		}
		udelay(1);
		wait_time++;
	} while (wait_time < REQSACK_TIMEOUT_TIME);

	nsp32_msg(KERN_WARNING, "wait SACK timeout, ack_bit: 0x%x", ack_bit);
}

static void nsp32_sack_assert(nsp32_hw_data *data)
{
	unsigned int  base = data->BaseAddress;
	unsigned char busctrl;

	busctrl  = nsp32_read1(base, SCSI_BUS_CONTROL);
	busctrl	|= (BUSCTL_ACK | AUTODIRECTION | ACKENB);
	nsp32_write1(base, SCSI_BUS_CONTROL, busctrl);
}

static void nsp32_sack_negate(nsp32_hw_data *data)
{
	unsigned int  base = data->BaseAddress;
	unsigned char busctrl;

	busctrl  = nsp32_read1(base, SCSI_BUS_CONTROL);
	busctrl	&= ~BUSCTL_ACK;
	nsp32_write1(base, SCSI_BUS_CONTROL, busctrl);
}



static int nsp32_detect(struct pci_dev *pdev)
{
	struct Scsi_Host *host;	
	struct resource  *res;
	nsp32_hw_data    *data;
	int               ret;
	int               i, j;

	nsp32_dbg(NSP32_DEBUG_REGISTER, "enter");

	host = scsi_host_alloc(&nsp32_template, sizeof(nsp32_hw_data));
	if (host == NULL) {
		nsp32_msg (KERN_ERR, "failed to scsi register");
		goto err;
	}

	data = (nsp32_hw_data *)host->hostdata;

	memcpy(data, &nsp32_data_base, sizeof(nsp32_hw_data));

	host->irq       = data->IrqNumber;
	host->io_port   = data->BaseAddress;
	host->unique_id = data->BaseAddress;
	host->n_io_port	= data->NumAddress;
	host->base      = (unsigned long)data->MmioAddress;

	data->Host      = host;
	spin_lock_init(&(data->Lock));

	data->cur_lunt   = NULL;
	data->cur_target = NULL;

	data->trans_method = NSP32_TRANSFER_BUSMASTER;

	data->clock = CLOCK_4;

	switch (data->clock) {
	case CLOCK_4:
		
		data->synct   = nsp32_sync_table_40M;
		data->syncnum = ARRAY_SIZE(nsp32_sync_table_40M);
		break;
	case CLOCK_2:
		
		data->synct   = nsp32_sync_table_20M;
		data->syncnum = ARRAY_SIZE(nsp32_sync_table_20M);
		break;
	case PCICLK:
		
		data->synct   = nsp32_sync_table_pci;
		data->syncnum = ARRAY_SIZE(nsp32_sync_table_pci);
		break;
	default:
		nsp32_msg(KERN_WARNING,
			  "Invalid clock div is selected, set CLOCK_4.");
		
		data->clock   = CLOCK_4;
		data->synct   = nsp32_sync_table_40M;
		data->syncnum = ARRAY_SIZE(nsp32_sync_table_40M);
	}


	if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32)) != 0) {
		nsp32_msg (KERN_ERR, "failed to set PCI DMA mask");
		goto scsi_unregister;
	}

	data->autoparam = pci_alloc_consistent(pdev, sizeof(nsp32_autoparam), &(data->auto_paddr));
	if (data->autoparam == NULL) {
		nsp32_msg(KERN_ERR, "failed to allocate DMA memory");
		goto scsi_unregister;
	}

	data->sg_list = pci_alloc_consistent(pdev, NSP32_SG_TABLE_SIZE,
					     &(data->sg_paddr));
	if (data->sg_list == NULL) {
		nsp32_msg(KERN_ERR, "failed to allocate DMA memory");
		goto free_autoparam;
	}

	for (i = 0; i < ARRAY_SIZE(data->lunt); i++) {
		for (j = 0; j < ARRAY_SIZE(data->lunt[0]); j++) {
			int offset = i * ARRAY_SIZE(data->lunt[0]) + j;
			nsp32_lunt tmp = {
				.SCpnt       = NULL,
				.save_datp   = 0,
				.msgin03     = FALSE,
				.sg_num      = 0,
				.cur_entry   = 0,
				.sglun       = &(data->sg_list[offset]),
				.sglun_paddr = data->sg_paddr + (offset * sizeof(nsp32_sglun)),
			};

			data->lunt[i][j] = tmp;
		}
	}

	for (i = 0; i < ARRAY_SIZE(data->target); i++) {
		nsp32_target *target = &(data->target[i]);

		target->limit_entry  = 0;
		target->sync_flag    = 0;
		nsp32_set_async(data, target);
	}

	ret = nsp32_getprom_param(data);
	if (ret == FALSE) {
		data->resettime = 3;	
	}

	nsp32hw_init(data);

	snprintf(data->info_str, sizeof(data->info_str),
		 "NinjaSCSI-32Bi/UDE: irq %d, io 0x%lx+0x%x",
		 host->irq, host->io_port, host->n_io_port);

	nsp32_do_bus_reset(data);

	ret = request_irq(host->irq, do_nsp32_isr, IRQF_SHARED, "nsp32", data);
	if (ret < 0) {
		nsp32_msg(KERN_ERR, "Unable to allocate IRQ for NinjaSCSI32 "
			  "SCSI PCI controller. Interrupt: %d", host->irq);
		goto free_sg_list;
	}

	res = request_region(host->io_port, host->n_io_port, "nsp32");
	if (res == NULL) {
		nsp32_msg(KERN_ERR, 
			  "I/O region 0x%lx+0x%lx is already used",
			  data->BaseAddress, data->NumAddress);
		goto free_irq;
        }

	ret = scsi_add_host(host, &pdev->dev);
	if (ret) {
		nsp32_msg(KERN_ERR, "failed to add scsi host");
		goto free_region;
	}
	scsi_scan_host(host);
	pci_set_drvdata(pdev, host);
	return 0;

 free_region:
	release_region(host->io_port, host->n_io_port);

 free_irq:
	free_irq(host->irq, data);

 free_sg_list:
	pci_free_consistent(pdev, NSP32_SG_TABLE_SIZE,
			    data->sg_list, data->sg_paddr);

 free_autoparam:
	pci_free_consistent(pdev, sizeof(nsp32_autoparam),
			    data->autoparam, data->auto_paddr);
	
 scsi_unregister:
	scsi_host_put(host);

 err:
	return 1;
}

static int nsp32_release(struct Scsi_Host *host)
{
	nsp32_hw_data *data = (nsp32_hw_data *)host->hostdata;

	if (data->autoparam) {
		pci_free_consistent(data->Pci, sizeof(nsp32_autoparam),
				    data->autoparam, data->auto_paddr);
	}

	if (data->sg_list) {
		pci_free_consistent(data->Pci, NSP32_SG_TABLE_SIZE,
				    data->sg_list, data->sg_paddr);
	}

	if (host->irq) {
		free_irq(host->irq, data);
	}

	if (host->io_port && host->n_io_port) {
		release_region(host->io_port, host->n_io_port);
	}

	if (data->MmioAddress) {
		iounmap(data->MmioAddress);
	}

	return 0;
}

static const char *nsp32_info(struct Scsi_Host *shpnt)
{
	nsp32_hw_data *data = (nsp32_hw_data *)shpnt->hostdata;

	return data->info_str;
}


static int nsp32_eh_abort(struct scsi_cmnd *SCpnt)
{
	nsp32_hw_data *data = (nsp32_hw_data *)SCpnt->device->host->hostdata;
	unsigned int   base = SCpnt->device->host->io_port;

	nsp32_msg(KERN_WARNING, "abort");

	if (data->cur_lunt->SCpnt == NULL) {
		nsp32_dbg(NSP32_DEBUG_BUSRESET, "abort failed");
		return FAILED;
	}

	if (data->cur_target->sync_flag & (SDTR_INITIATOR | SDTR_TARGET)) {
		
		data->cur_target->sync_flag = 0;
		nsp32_set_async(data, data->cur_target);
	}

	nsp32_write2(base, TRANSFER_CONTROL, 0);
	nsp32_write2(base, BM_CNT,           0);

	SCpnt->result = DID_ABORT << 16;
	nsp32_scsi_done(SCpnt);

	nsp32_dbg(NSP32_DEBUG_BUSRESET, "abort success");
	return SUCCESS;
}

static int nsp32_eh_bus_reset(struct scsi_cmnd *SCpnt)
{
	nsp32_hw_data *data = (nsp32_hw_data *)SCpnt->device->host->hostdata;
	unsigned int   base = SCpnt->device->host->io_port;

	spin_lock_irq(SCpnt->device->host->host_lock);

	nsp32_msg(KERN_INFO, "Bus Reset");	
	nsp32_dbg(NSP32_DEBUG_BUSRESET, "SCpnt=0x%x", SCpnt);

	nsp32_write2(base, IRQ_CONTROL, IRQ_CONTROL_ALL_IRQ_MASK);
	nsp32_do_bus_reset(data);
	nsp32_write2(base, IRQ_CONTROL, 0);

	spin_unlock_irq(SCpnt->device->host->host_lock);
	return SUCCESS;	
}

static void nsp32_do_bus_reset(nsp32_hw_data *data)
{
	unsigned int   base = data->BaseAddress;
	unsigned short intrdat;
	int i;

	nsp32_dbg(NSP32_DEBUG_BUSRESET, "in");

	nsp32_write2(base, TRANSFER_CONTROL, 0);
	nsp32_write4(base, BM_CNT,           0);
	nsp32_write4(base, CLR_COUNTER,      CLRCOUNTER_ALLMASK);

	for (i = 0; i < ARRAY_SIZE(data->target); i++) {
		nsp32_target *target = &data->target[i];

		target->sync_flag = 0;
		nsp32_set_async(data, target);
	}

	nsp32_write1(base, SCSI_BUS_CONTROL, BUSCTL_RST);
	udelay(RESET_HOLD_TIME);
	nsp32_write1(base, SCSI_BUS_CONTROL, 0);
	for(i = 0; i < 5; i++) {
		intrdat = nsp32_read2(base, IRQ_STATUS); 
		nsp32_dbg(NSP32_DEBUG_BUSRESET, "irq:1: 0x%x", intrdat);
        }

	data->CurrentSC = NULL;
}

static int nsp32_eh_host_reset(struct scsi_cmnd *SCpnt)
{
	struct Scsi_Host *host = SCpnt->device->host;
	unsigned int      base = SCpnt->device->host->io_port;
	nsp32_hw_data    *data = (nsp32_hw_data *)host->hostdata;

	nsp32_msg(KERN_INFO, "Host Reset");	
	nsp32_dbg(NSP32_DEBUG_BUSRESET, "SCpnt=0x%x", SCpnt);

	spin_lock_irq(SCpnt->device->host->host_lock);

	nsp32hw_init(data);
	nsp32_write2(base, IRQ_CONTROL, IRQ_CONTROL_ALL_IRQ_MASK);
	nsp32_do_bus_reset(data);
	nsp32_write2(base, IRQ_CONTROL, 0);

	spin_unlock_irq(SCpnt->device->host->host_lock);
	return SUCCESS;	
}



static int nsp32_getprom_param(nsp32_hw_data *data)
{
	int vendor = data->pci_devid->vendor;
	int device = data->pci_devid->device;
	int ret, val, i;

	ret = nsp32_prom_read(data, 0x7e);
	if (ret != 0x55) {
		nsp32_msg(KERN_INFO, "No EEPROM detected: 0x%x", ret);
		return FALSE;
	}
	ret = nsp32_prom_read(data, 0x7f);
	if (ret != 0xaa) {
		nsp32_msg(KERN_INFO, "Invalid number: 0x%x", ret);
		return FALSE;
	}

	if (vendor == PCI_VENDOR_ID_WORKBIT &&
	    device == PCI_DEVICE_ID_WORKBIT_STANDARD) {
		ret = nsp32_getprom_c16(data);
	} else if (vendor == PCI_VENDOR_ID_WORKBIT &&
		   device == PCI_DEVICE_ID_NINJASCSI_32BIB_LOGITEC) {
		ret = nsp32_getprom_at24(data);
	} else if (vendor == PCI_VENDOR_ID_WORKBIT &&
		   device == PCI_DEVICE_ID_NINJASCSI_32UDE_MELCO ) {
		ret = nsp32_getprom_at24(data);
	} else {
		nsp32_msg(KERN_WARNING, "Unknown EEPROM");
		ret = FALSE;
	}

	
	for (i = 0; i <= 0x1f; i++) {
		val = nsp32_prom_read(data, i);
		nsp32_dbg(NSP32_DEBUG_EEPROM,
			  "rom address 0x%x : 0x%x", i, val);
	}

	return ret;
}


static int nsp32_getprom_at24(nsp32_hw_data *data)
{
	int           ret, i;
	int           auto_sync;
	nsp32_target *target;
	int           entry;

	data->resettime = nsp32_prom_read(data, 0x12);

	ret = nsp32_prom_read(data, 0x07);
	switch (ret) {
	case 0:
		auto_sync = TRUE;
		break;
	case 1:
		auto_sync = FALSE;
		break;
	default:
		nsp32_msg(KERN_WARNING,
			  "Unsupported Auto Sync mode. Fall back to manual mode.");
		auto_sync = TRUE;
	}

	if (trans_mode == ULTRA20M_MODE) {
		auto_sync = TRUE;
	}

	for (i = 0; i < NSP32_HOST_SCSIID; i++) {
		target = &data->target[i];
		if (auto_sync == TRUE) {
			target->limit_entry = 0;   
		} else {
			ret   = nsp32_prom_read(data, i);
			entry = nsp32_search_period_entry(data, target, ret);
			if (entry < 0) {
				
				entry = 0;
			}
			target->limit_entry = entry;
		}
	}

	return TRUE;
}


static int nsp32_getprom_c16(nsp32_hw_data *data)
{
	int           ret, i;
	nsp32_target *target;
	int           entry, val;

	data->resettime = nsp32_prom_read(data, 0x11);

	for (i = 0; i < NSP32_HOST_SCSIID; i++) {
		target = &data->target[i];
		ret = nsp32_prom_read(data, i);
		switch (ret) {
		case 0:		
			val = 0x0c;
			break;
		case 1:		
			val = 0x19;
			break;
		case 2:		
			val = 0x32;
			break;
		case 3:		
			val = 0x00;
			break;
		default:	
			val = 0x0c;
			break;
		}
		entry = nsp32_search_period_entry(data, target, val);
		if (entry < 0 || trans_mode == ULTRA20M_MODE) {
			
			entry = 0;
		}
		target->limit_entry = entry;
	}

	return TRUE;
}


static int nsp32_prom_read(nsp32_hw_data *data, int romaddr)
{
	int i, val;

	
	nsp32_prom_start(data);

	
	nsp32_prom_write_bit(data, 1);	
	nsp32_prom_write_bit(data, 0);	
	nsp32_prom_write_bit(data, 1);	
	nsp32_prom_write_bit(data, 0);	
	nsp32_prom_write_bit(data, 0);	
	nsp32_prom_write_bit(data, 0);	
	nsp32_prom_write_bit(data, 0);	

	
	nsp32_prom_write_bit(data, 0);

	
	nsp32_prom_write_bit(data, 0);

	
	for (i = 7; i >= 0; i--) {
		nsp32_prom_write_bit(data, ((romaddr >> i) & 1));
	}

	
	nsp32_prom_write_bit(data, 0);

	
	nsp32_prom_start(data);

	
	nsp32_prom_write_bit(data, 1);	
	nsp32_prom_write_bit(data, 0);	
	nsp32_prom_write_bit(data, 1);	
	nsp32_prom_write_bit(data, 0);	
	nsp32_prom_write_bit(data, 0);	
	nsp32_prom_write_bit(data, 0);	
	nsp32_prom_write_bit(data, 0);	

	
	nsp32_prom_write_bit(data, 1);

	
	nsp32_prom_write_bit(data, 0);

	
	val = 0;
	for (i = 7; i >= 0; i--) {
		val += (nsp32_prom_read_bit(data) << i);
	}
	
	
	nsp32_prom_write_bit(data, 1);

	
	nsp32_prom_stop(data);

	return val;
}

static void nsp32_prom_set(nsp32_hw_data *data, int bit, int val)
{
	int base = data->BaseAddress;
	int tmp;

	tmp = nsp32_index_read1(base, SERIAL_ROM_CTL);

	if (val == 0) {
		tmp &= ~bit;
	} else {
		tmp |=  bit;
	}

	nsp32_index_write1(base, SERIAL_ROM_CTL, tmp);

	udelay(10);
}

static int nsp32_prom_get(nsp32_hw_data *data, int bit)
{
	int base = data->BaseAddress;
	int tmp, ret;

	if (bit != SDA) {
		nsp32_msg(KERN_ERR, "return value is not appropriate");
		return 0;
	}


	tmp = nsp32_index_read1(base, SERIAL_ROM_CTL) & bit;

	if (tmp == 0) {
		ret = 0;
	} else {
		ret = 1;
	}

	udelay(10);

	return ret;
}

static void nsp32_prom_start (nsp32_hw_data *data)
{
	
	nsp32_prom_set(data, SCL, 1);
	nsp32_prom_set(data, SDA, 1);
	nsp32_prom_set(data, ENA, 1);	
	nsp32_prom_set(data, SDA, 0);	
	nsp32_prom_set(data, SCL, 0);
}

static void nsp32_prom_stop (nsp32_hw_data *data)
{
	
	nsp32_prom_set(data, SCL, 1);
	nsp32_prom_set(data, SDA, 0);
	nsp32_prom_set(data, ENA, 1);	
	nsp32_prom_set(data, SDA, 1);
	nsp32_prom_set(data, SCL, 0);
}

static void nsp32_prom_write_bit(nsp32_hw_data *data, int val)
{
	
	nsp32_prom_set(data, SDA, val);
	nsp32_prom_set(data, SCL, 1  );
	nsp32_prom_set(data, SCL, 0  );
}

static int nsp32_prom_read_bit(nsp32_hw_data *data)
{
	int val;

	
	nsp32_prom_set(data, ENA, 0);	
	nsp32_prom_set(data, SCL, 1);

	val = nsp32_prom_get(data, SDA);

	nsp32_prom_set(data, SCL, 0);
	nsp32_prom_set(data, ENA, 1);	

	return val;
}


#ifdef CONFIG_PM

static int nsp32_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct Scsi_Host *host = pci_get_drvdata(pdev);

	nsp32_msg(KERN_INFO, "pci-suspend: pdev=0x%p, state=%ld, slot=%s, host=0x%p", pdev, state, pci_name(pdev), host);

	pci_save_state     (pdev);
	pci_disable_device (pdev);
	pci_set_power_state(pdev, pci_choose_state(pdev, state));

	return 0;
}

static int nsp32_resume(struct pci_dev *pdev)
{
	struct Scsi_Host *host = pci_get_drvdata(pdev);
	nsp32_hw_data    *data = (nsp32_hw_data *)host->hostdata;
	unsigned short    reg;

	nsp32_msg(KERN_INFO, "pci-resume: pdev=0x%p, slot=%s, host=0x%p", pdev, pci_name(pdev), host);

	pci_set_power_state(pdev, PCI_D0);
	pci_enable_wake    (pdev, PCI_D0, 0);
	pci_restore_state  (pdev);

	reg = nsp32_read2(data->BaseAddress, INDEX_REG);

	nsp32_msg(KERN_INFO, "io=0x%x reg=0x%x", data->BaseAddress, reg);

	if (reg == 0xffff) {
		nsp32_msg(KERN_INFO, "missing device. abort resume.");
		return 0;
	}

	nsp32hw_init      (data);
	nsp32_do_bus_reset(data);

	nsp32_msg(KERN_INFO, "resume success");

	return 0;
}

#endif

static int __devinit nsp32_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int ret;
	nsp32_hw_data *data = &nsp32_data_base;

	nsp32_dbg(NSP32_DEBUG_REGISTER, "enter");

        ret = pci_enable_device(pdev);
	if (ret) {
		nsp32_msg(KERN_ERR, "failed to enable pci device");
		return ret;
	}

	data->Pci         = pdev;
	data->pci_devid   = id;
	data->IrqNumber   = pdev->irq;
	data->BaseAddress = pci_resource_start(pdev, 0);
	data->NumAddress  = pci_resource_len  (pdev, 0);
	data->MmioAddress = pci_ioremap_bar(pdev, 1);
	data->MmioLength  = pci_resource_len  (pdev, 1);

	pci_set_master(pdev);

	ret = nsp32_detect(pdev);

	nsp32_msg(KERN_INFO, "irq: %i mmio: %p+0x%lx slot: %s model: %s",
		  pdev->irq,
		  data->MmioAddress, data->MmioLength,
		  pci_name(pdev),
		  nsp32_model[id->driver_data]);

	nsp32_dbg(NSP32_DEBUG_REGISTER, "exit %d", ret);

	return ret;
}

static void __devexit nsp32_remove(struct pci_dev *pdev)
{
	struct Scsi_Host *host = pci_get_drvdata(pdev);

	nsp32_dbg(NSP32_DEBUG_REGISTER, "enter");

        scsi_remove_host(host);

	nsp32_release(host);

	scsi_host_put(host);
}

static struct pci_driver nsp32_driver = {
	.name		= "nsp32",
	.id_table	= nsp32_pci_table,
	.probe		= nsp32_probe,
	.remove		= __devexit_p(nsp32_remove),
#ifdef CONFIG_PM
	.suspend	= nsp32_suspend, 
	.resume		= nsp32_resume, 
#endif
};

static int __init init_nsp32(void) {
	nsp32_msg(KERN_INFO, "loading...");
	return pci_register_driver(&nsp32_driver);
}

static void __exit exit_nsp32(void) {
	nsp32_msg(KERN_INFO, "unloading...");
	pci_unregister_driver(&nsp32_driver);
}

module_init(init_nsp32);
module_exit(exit_nsp32);

