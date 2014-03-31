/******************************************************************************
*                  QLOGIC LINUX SOFTWARE
*
* QLogic  QLA1280 (Ultra2)  and  QLA12160 (Ultra3) SCSI driver
* Copyright (C) 2000 Qlogic Corporation (www.qlogic.com)
* Copyright (C) 2001-2004 Jes Sorensen, Wild Open Source Inc.
* Copyright (C) 2003-2004 Christoph Hellwig
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2, or (at your option) any
* later version.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
******************************************************************************/
#define QLA1280_VERSION      "3.27.1"


#include <linux/module.h>

#include <linux/types.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/pci_ids.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/byteorder.h>
#include <asm/processor.h>
#include <asm/types.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>

#if defined(CONFIG_IA64_GENERIC) || defined(CONFIG_IA64_SGI_SN2)
#include <asm/sn/io.h>
#endif


#define  DEBUG_QLA1280_INTR	0
#define  DEBUG_PRINT_NVRAM	0
#define  DEBUG_QLA1280		0

#ifdef CONFIG_X86_VISWS
#define	MEMORY_MAPPED_IO	0
#else
#define	MEMORY_MAPPED_IO	1
#endif

#include "qla1280.h"

#ifndef BITS_PER_LONG
#error "BITS_PER_LONG not defined!"
#endif
#if (BITS_PER_LONG == 64) || defined CONFIG_HIGHMEM
#define QLA_64BIT_PTR	1
#endif

#ifdef QLA_64BIT_PTR
#define pci_dma_hi32(a)			((a >> 16) >> 16)
#else
#define pci_dma_hi32(a)			0
#endif
#define pci_dma_lo32(a)			(a & 0xffffffff)

#define NVRAM_DELAY()			udelay(500)	

#if defined(__ia64__) && !defined(ia64_platform_is)
#define ia64_platform_is(foo)		(!strcmp(x, platform_name))
#endif


#define IS_ISP1040(ha) (ha->pdev->device == PCI_DEVICE_ID_QLOGIC_ISP1020)
#define IS_ISP1x40(ha) (ha->pdev->device == PCI_DEVICE_ID_QLOGIC_ISP1020 || \
			ha->pdev->device == PCI_DEVICE_ID_QLOGIC_ISP1240)
#define IS_ISP1x160(ha)        (ha->pdev->device == PCI_DEVICE_ID_QLOGIC_ISP10160 || \
				ha->pdev->device == PCI_DEVICE_ID_QLOGIC_ISP12160)


static int qla1280_probe_one(struct pci_dev *, const struct pci_device_id *);
static void qla1280_remove_one(struct pci_dev *);

static void qla1280_done(struct scsi_qla_host *);
static int qla1280_get_token(char *);
static int qla1280_setup(char *s) __init;

static int qla1280_load_firmware(struct scsi_qla_host *);
static int qla1280_init_rings(struct scsi_qla_host *);
static int qla1280_nvram_config(struct scsi_qla_host *);
static int qla1280_mailbox_command(struct scsi_qla_host *,
				   uint8_t, uint16_t *);
static int qla1280_bus_reset(struct scsi_qla_host *, int);
static int qla1280_device_reset(struct scsi_qla_host *, int, int);
static int qla1280_abort_command(struct scsi_qla_host *, struct srb *, int);
static int qla1280_abort_isp(struct scsi_qla_host *);
#ifdef QLA_64BIT_PTR
static int qla1280_64bit_start_scsi(struct scsi_qla_host *, struct srb *);
#else
static int qla1280_32bit_start_scsi(struct scsi_qla_host *, struct srb *);
#endif
static void qla1280_nv_write(struct scsi_qla_host *, uint16_t);
static void qla1280_poll(struct scsi_qla_host *);
static void qla1280_reset_adapter(struct scsi_qla_host *);
static void qla1280_marker(struct scsi_qla_host *, int, int, int, u8);
static void qla1280_isp_cmd(struct scsi_qla_host *);
static void qla1280_isr(struct scsi_qla_host *, struct list_head *);
static void qla1280_rst_aen(struct scsi_qla_host *);
static void qla1280_status_entry(struct scsi_qla_host *, struct response *,
				 struct list_head *);
static void qla1280_error_entry(struct scsi_qla_host *, struct response *,
				struct list_head *);
static uint16_t qla1280_get_nvram_word(struct scsi_qla_host *, uint32_t);
static uint16_t qla1280_nvram_request(struct scsi_qla_host *, uint32_t);
static uint16_t qla1280_debounce_register(volatile uint16_t __iomem *);
static request_t *qla1280_req_pkt(struct scsi_qla_host *);
static int qla1280_check_for_dead_scsi_bus(struct scsi_qla_host *,
					   unsigned int);
static void qla1280_get_target_parameters(struct scsi_qla_host *,
					   struct scsi_device *);
static int qla1280_set_target_parameters(struct scsi_qla_host *, int, int);


static struct qla_driver_setup driver_setup;

static inline uint16_t
qla1280_data_direction(struct scsi_cmnd *cmnd)
{
	switch(cmnd->sc_data_direction) {
	case DMA_FROM_DEVICE:
		return BIT_5;
	case DMA_TO_DEVICE:
		return BIT_6;
	case DMA_BIDIRECTIONAL:
		return BIT_5 | BIT_6;
	case DMA_NONE:
	default:
		return 0;
	}
}
		
#if DEBUG_QLA1280
static void __qla1280_print_scsi_cmd(struct scsi_cmnd * cmd);
static void __qla1280_dump_buffer(char *, int);
#endif


#ifdef MODULE
static char *qla1280;

module_param(qla1280, charp, 0);
#else
__setup("qla1280=", qla1280_setup);
#endif



#define	CMD_SP(Cmnd)		&Cmnd->SCp
#define	CMD_CDBLEN(Cmnd)	Cmnd->cmd_len
#define	CMD_CDBP(Cmnd)		Cmnd->cmnd
#define	CMD_SNSP(Cmnd)		Cmnd->sense_buffer
#define	CMD_SNSLEN(Cmnd)	SCSI_SENSE_BUFFERSIZE
#define	CMD_RESULT(Cmnd)	Cmnd->result
#define	CMD_HANDLE(Cmnd)	Cmnd->host_scribble
#define CMD_REQUEST(Cmnd)	Cmnd->request->cmd

#define CMD_HOST(Cmnd)		Cmnd->device->host
#define SCSI_BUS_32(Cmnd)	Cmnd->device->channel
#define SCSI_TCN_32(Cmnd)	Cmnd->device->id
#define SCSI_LUN_32(Cmnd)	Cmnd->device->lun



struct qla_boards {
	char *name;		
	int numPorts;		
	int fw_index;		
};

static struct pci_device_id qla1280_pci_tbl[] = {
	{PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP12160,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP1020,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 1},
	{PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP1080,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 2},
	{PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP1240,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 3},
	{PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP1280,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 4},
	{PCI_VENDOR_ID_QLOGIC, PCI_DEVICE_ID_QLOGIC_ISP10160,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 5},
	{0,}
};
MODULE_DEVICE_TABLE(pci, qla1280_pci_tbl);

DEFINE_MUTEX(qla1280_firmware_mutex);

struct qla_fw {
	char *fwname;
	const struct firmware *fw;
};

#define QL_NUM_FW_IMAGES 3

struct qla_fw qla1280_fw_tbl[QL_NUM_FW_IMAGES] = {
	{"qlogic/1040.bin",  NULL},	
	{"qlogic/1280.bin",  NULL},	
	{"qlogic/12160.bin", NULL},	
};

static struct qla_boards ql1280_board_tbl[] = {
	{.name = "QLA12160", .numPorts = 2, .fw_index = 2},
	{.name = "QLA1040" , .numPorts = 1, .fw_index = 0},
	{.name = "QLA1080" , .numPorts = 1, .fw_index = 1},
	{.name = "QLA1240" , .numPorts = 2, .fw_index = 1},
	{.name = "QLA1280" , .numPorts = 2, .fw_index = 1},
	{.name = "QLA10160", .numPorts = 1, .fw_index = 2},
	{.name = "        ", .numPorts = 0, .fw_index = -1},
};

static int qla1280_verbose = 1;

#if DEBUG_QLA1280
static int ql_debug_level = 1;
#define dprintk(level, format, a...)	\
	do { if (ql_debug_level >= level) printk(KERN_ERR format, ##a); } while(0)
#define qla1280_dump_buffer(level, buf, size)	\
	if (ql_debug_level >= level) __qla1280_dump_buffer(buf, size)
#define qla1280_print_scsi_cmd(level, cmd)	\
	if (ql_debug_level >= level) __qla1280_print_scsi_cmd(cmd)
#else
#define ql_debug_level			0
#define dprintk(level, format, a...)	do{}while(0)
#define qla1280_dump_buffer(a, b, c)	do{}while(0)
#define qla1280_print_scsi_cmd(a, b)	do{}while(0)
#endif

#define ENTER(x)		dprintk(3, "qla1280 : Entering %s()\n", x);
#define LEAVE(x)		dprintk(3, "qla1280 : Leaving %s()\n", x);
#define ENTER_INTR(x)		dprintk(4, "qla1280 : Entering %s()\n", x);
#define LEAVE_INTR(x)		dprintk(4, "qla1280 : Leaving %s()\n", x);


static int qla1280_read_nvram(struct scsi_qla_host *ha)
{
	uint16_t *wptr;
	uint8_t chksum;
	int cnt, i;
	struct nvram *nv;

	ENTER("qla1280_read_nvram");

	if (driver_setup.no_nvram)
		return 1;

	printk(KERN_INFO "scsi(%ld): Reading NVRAM\n", ha->host_no);

	wptr = (uint16_t *)&ha->nvram;
	nv = &ha->nvram;
	chksum = 0;
	for (cnt = 0; cnt < 3; cnt++) {
		*wptr = qla1280_get_nvram_word(ha, cnt);
		chksum += *wptr & 0xff;
		chksum += (*wptr >> 8) & 0xff;
		wptr++;
	}

	if (nv->id0 != 'I' || nv->id1 != 'S' ||
	    nv->id2 != 'P' || nv->id3 != ' ' || nv->version < 1) {
		dprintk(2, "Invalid nvram ID or version!\n");
		chksum = 1;
	} else {
		for (; cnt < sizeof(struct nvram); cnt++) {
			*wptr = qla1280_get_nvram_word(ha, cnt);
			chksum += *wptr & 0xff;
			chksum += (*wptr >> 8) & 0xff;
			wptr++;
		}
	}

	dprintk(3, "qla1280_read_nvram: NVRAM Magic ID= %c %c %c %02x"
	       " version %i\n", nv->id0, nv->id1, nv->id2, nv->id3,
	       nv->version);


	if (chksum) {
		if (!driver_setup.no_nvram)
			printk(KERN_WARNING "scsi(%ld): Unable to identify or "
			       "validate NVRAM checksum, using default "
			       "settings\n", ha->host_no);
		ha->nvram_valid = 0;
	} else
		ha->nvram_valid = 1;

	nv->isp_parameter = cpu_to_le16(nv->isp_parameter);
	nv->firmware_feature.w = cpu_to_le16(nv->firmware_feature.w);
	for(i = 0; i < MAX_BUSES; i++) {
		nv->bus[i].selection_timeout = cpu_to_le16(nv->bus[i].selection_timeout);
		nv->bus[i].max_queue_depth = cpu_to_le16(nv->bus[i].max_queue_depth);
	}
	dprintk(1, "qla1280_read_nvram: Completed Reading NVRAM\n");
	LEAVE("qla1280_read_nvram");

	return chksum;
}

static const char *
qla1280_info(struct Scsi_Host *host)
{
	static char qla1280_scsi_name_buffer[125];
	char *bp;
	struct scsi_qla_host *ha;
	struct qla_boards *bdp;

	bp = &qla1280_scsi_name_buffer[0];
	ha = (struct scsi_qla_host *)host->hostdata;
	bdp = &ql1280_board_tbl[ha->devnum];
	memset(bp, 0, sizeof(qla1280_scsi_name_buffer));

	sprintf (bp,
		 "QLogic %s PCI to SCSI Host Adapter\n"
		 "       Firmware version: %2d.%02d.%02d, Driver version %s",
		 &bdp->name[0], ha->fwver1, ha->fwver2, ha->fwver3,
		 QLA1280_VERSION);
	return bp;
}

static int
qla1280_queuecommand_lck(struct scsi_cmnd *cmd, void (*fn)(struct scsi_cmnd *))
{
	struct Scsi_Host *host = cmd->device->host;
	struct scsi_qla_host *ha = (struct scsi_qla_host *)host->hostdata;
	struct srb *sp = (struct srb *)CMD_SP(cmd);
	int status;

	cmd->scsi_done = fn;
	sp->cmd = cmd;
	sp->flags = 0;
	sp->wait = NULL;
	CMD_HANDLE(cmd) = (unsigned char *)NULL;

	qla1280_print_scsi_cmd(5, cmd);

#ifdef QLA_64BIT_PTR
	status = qla1280_64bit_start_scsi(ha, sp);
#else
	status = qla1280_32bit_start_scsi(ha, sp);
#endif
	return status;
}

static DEF_SCSI_QCMD(qla1280_queuecommand)

enum action {
	ABORT_COMMAND,
	DEVICE_RESET,
	BUS_RESET,
	ADAPTER_RESET,
};


static void qla1280_mailbox_timeout(unsigned long __data)
{
	struct scsi_qla_host *ha = (struct scsi_qla_host *)__data;
	struct device_reg __iomem *reg;
	reg = ha->iobase;

	ha->mailbox_out[0] = RD_REG_WORD(&reg->mailbox0);
	printk(KERN_ERR "scsi(%ld): mailbox timed out, mailbox0 %04x, "
	       "ictrl %04x, istatus %04x\n", ha->host_no, ha->mailbox_out[0],
	       RD_REG_WORD(&reg->ictrl), RD_REG_WORD(&reg->istatus));
	complete(ha->mailbox_wait);
}

static int
_qla1280_wait_for_single_command(struct scsi_qla_host *ha, struct srb *sp,
				 struct completion *wait)
{
	int	status = FAILED;
	struct scsi_cmnd *cmd = sp->cmd;

	spin_unlock_irq(ha->host->host_lock);
	wait_for_completion_timeout(wait, 4*HZ);
	spin_lock_irq(ha->host->host_lock);
	sp->wait = NULL;
	if(CMD_HANDLE(cmd) == COMPLETED_HANDLE) {
		status = SUCCESS;
		(*cmd->scsi_done)(cmd);
	}
	return status;
}

static int
qla1280_wait_for_single_command(struct scsi_qla_host *ha, struct srb *sp)
{
	DECLARE_COMPLETION_ONSTACK(wait);

	sp->wait = &wait;
	return _qla1280_wait_for_single_command(ha, sp, &wait);
}

static int
qla1280_wait_for_pending_commands(struct scsi_qla_host *ha, int bus, int target)
{
	int		cnt;
	int		status;
	struct srb	*sp;
	struct scsi_cmnd *cmd;

	status = SUCCESS;

	for (cnt = 0; cnt < MAX_OUTSTANDING_COMMANDS; cnt++) {
		sp = ha->outstanding_cmds[cnt];
		if (sp) {
			cmd = sp->cmd;

			if (bus >= 0 && SCSI_BUS_32(cmd) != bus)
				continue;
			if (target >= 0 && SCSI_TCN_32(cmd) != target)
				continue;

			status = qla1280_wait_for_single_command(ha, sp);
			if (status == FAILED)
				break;
		}
	}
	return status;
}

static int
qla1280_error_action(struct scsi_cmnd *cmd, enum action action)
{
	struct scsi_qla_host *ha;
	int bus, target, lun;
	struct srb *sp;
	int i, found;
	int result=FAILED;
	int wait_for_bus=-1;
	int wait_for_target = -1;
	DECLARE_COMPLETION_ONSTACK(wait);

	ENTER("qla1280_error_action");

	ha = (struct scsi_qla_host *)(CMD_HOST(cmd)->hostdata);
	sp = (struct srb *)CMD_SP(cmd);
	bus = SCSI_BUS_32(cmd);
	target = SCSI_TCN_32(cmd);
	lun = SCSI_LUN_32(cmd);

	dprintk(4, "error_action %i, istatus 0x%04x\n", action,
		RD_REG_WORD(&ha->iobase->istatus));

	dprintk(4, "host_cmd 0x%04x, ictrl 0x%04x, jiffies %li\n",
		RD_REG_WORD(&ha->iobase->host_cmd),
		RD_REG_WORD(&ha->iobase->ictrl), jiffies);

	if (qla1280_verbose)
		printk(KERN_INFO "scsi(%li): Resetting Cmnd=0x%p, "
		       "Handle=0x%p, action=0x%x\n",
		       ha->host_no, cmd, CMD_HANDLE(cmd), action);

	found = -1;
	for (i = 0; i < MAX_OUTSTANDING_COMMANDS; i++) {
		if (sp == ha->outstanding_cmds[i]) {
			found = i;
			sp->wait = &wait; 
			break;
		}
	}

	if (found < 0) {	
		result = SUCCESS;
		if (qla1280_verbose) {
			printk(KERN_INFO
			       "scsi(%ld:%d:%d:%d): specified command has "
			       "already completed.\n", ha->host_no, bus,
				target, lun);
		}
	}

	switch (action) {

	case ABORT_COMMAND:
		dprintk(1, "qla1280: RISC aborting command\n");
		if (found >= 0)
			qla1280_abort_command(ha, sp, found);
		break;

	case DEVICE_RESET:
		if (qla1280_verbose)
			printk(KERN_INFO
			       "scsi(%ld:%d:%d:%d): Queueing device reset "
			       "command.\n", ha->host_no, bus, target, lun);
		if (qla1280_device_reset(ha, bus, target) == 0) {
			
			wait_for_bus = bus;
			wait_for_target = target;
		}
		break;

	case BUS_RESET:
		if (qla1280_verbose)
			printk(KERN_INFO "qla1280(%ld:%d): Issued bus "
			       "reset.\n", ha->host_no, bus);
		if (qla1280_bus_reset(ha, bus) == 0) {
			
			wait_for_bus = bus;
		}
		break;

	case ADAPTER_RESET:
	default:
		if (qla1280_verbose) {
			printk(KERN_INFO
			       "scsi(%ld): Issued ADAPTER RESET\n",
			       ha->host_no);
			printk(KERN_INFO "scsi(%ld): I/O processing will "
			       "continue automatically\n", ha->host_no);
		}
		ha->flags.reset_active = 1;

		if (qla1280_abort_isp(ha) != 0) {	
			result = FAILED;
		}

		ha->flags.reset_active = 0;
	}


	if (found >= 0)
		result = _qla1280_wait_for_single_command(ha, sp, &wait);

	if (action == ABORT_COMMAND && result != SUCCESS) {
		printk(KERN_WARNING
		       "scsi(%li:%i:%i:%i): "
		       "Unable to abort command!\n",
		       ha->host_no, bus, target, lun);
	}

	if (result == SUCCESS && wait_for_bus >= 0) {
		result = qla1280_wait_for_pending_commands(ha,
					wait_for_bus, wait_for_target);
	}

	dprintk(1, "RESET returning %d\n", result);

	LEAVE("qla1280_error_action");
	return result;
}

static int
qla1280_eh_abort(struct scsi_cmnd * cmd)
{
	int rc;

	spin_lock_irq(cmd->device->host->host_lock);
	rc = qla1280_error_action(cmd, ABORT_COMMAND);
	spin_unlock_irq(cmd->device->host->host_lock);

	return rc;
}

static int
qla1280_eh_device_reset(struct scsi_cmnd *cmd)
{
	int rc;

	spin_lock_irq(cmd->device->host->host_lock);
	rc = qla1280_error_action(cmd, DEVICE_RESET);
	spin_unlock_irq(cmd->device->host->host_lock);

	return rc;
}

static int
qla1280_eh_bus_reset(struct scsi_cmnd *cmd)
{
	int rc;

	spin_lock_irq(cmd->device->host->host_lock);
	rc = qla1280_error_action(cmd, BUS_RESET);
	spin_unlock_irq(cmd->device->host->host_lock);

	return rc;
}

static int
qla1280_eh_adapter_reset(struct scsi_cmnd *cmd)
{
	int rc;

	spin_lock_irq(cmd->device->host->host_lock);
	rc = qla1280_error_action(cmd, ADAPTER_RESET);
	spin_unlock_irq(cmd->device->host->host_lock);

	return rc;
}

static int
qla1280_biosparam(struct scsi_device *sdev, struct block_device *bdev,
		  sector_t capacity, int geom[])
{
	int heads, sectors, cylinders;

	heads = 64;
	sectors = 32;
	cylinders = (unsigned long)capacity / (heads * sectors);
	if (cylinders > 1024) {
		heads = 255;
		sectors = 63;
		cylinders = (unsigned long)capacity / (heads * sectors);
	}

	geom[0] = heads;
	geom[1] = sectors;
	geom[2] = cylinders;

	return 0;
}

 
static inline void
qla1280_disable_intrs(struct scsi_qla_host *ha)
{
	WRT_REG_WORD(&ha->iobase->ictrl, 0);
	RD_REG_WORD(&ha->iobase->ictrl);	
}

static inline void
qla1280_enable_intrs(struct scsi_qla_host *ha)
{
	WRT_REG_WORD(&ha->iobase->ictrl, (ISP_EN_INT | ISP_EN_RISC));
	RD_REG_WORD(&ha->iobase->ictrl);	
}

static irqreturn_t
qla1280_intr_handler(int irq, void *dev_id)
{
	struct scsi_qla_host *ha;
	struct device_reg __iomem *reg;
	u16 data;
	int handled = 0;

	ENTER_INTR ("qla1280_intr_handler");
	ha = (struct scsi_qla_host *)dev_id;

	spin_lock(ha->host->host_lock);

	ha->isr_count++;
	reg = ha->iobase;

	qla1280_disable_intrs(ha);

	data = qla1280_debounce_register(&reg->istatus);
	
	if (data & RISC_INT) {	
		qla1280_isr(ha, &ha->done_q);
		handled = 1;
	}
	if (!list_empty(&ha->done_q))
		qla1280_done(ha);

	spin_unlock(ha->host->host_lock);

	qla1280_enable_intrs(ha);

	LEAVE_INTR("qla1280_intr_handler");
	return IRQ_RETVAL(handled);
}


static int
qla1280_set_target_parameters(struct scsi_qla_host *ha, int bus, int target)
{
	uint8_t mr;
	uint16_t mb[MAILBOX_REGISTER_COUNT];
	struct nvram *nv;
	int status, lun;

	nv = &ha->nvram;

	mr = BIT_3 | BIT_2 | BIT_1 | BIT_0;

	
	mb[0] = MBC_SET_TARGET_PARAMETERS;
	mb[1] = (uint16_t)((bus ? target | BIT_7 : target) << 8);
	mb[2] = nv->bus[bus].target[target].parameter.renegotiate_on_error << 8;
	mb[2] |= nv->bus[bus].target[target].parameter.stop_queue_on_check << 9;
	mb[2] |= nv->bus[bus].target[target].parameter.auto_request_sense << 10;
	mb[2] |= nv->bus[bus].target[target].parameter.tag_queuing << 11;
	mb[2] |= nv->bus[bus].target[target].parameter.enable_sync << 12;
	mb[2] |= nv->bus[bus].target[target].parameter.enable_wide << 13;
	mb[2] |= nv->bus[bus].target[target].parameter.parity_checking << 14;
	mb[2] |= nv->bus[bus].target[target].parameter.disconnect_allowed << 15;

	if (IS_ISP1x160(ha)) {
		mb[2] |= nv->bus[bus].target[target].ppr_1x160.flags.enable_ppr << 5;
		mb[3] =	(nv->bus[bus].target[target].flags.flags1x160.sync_offset << 8);
		mb[6] =	(nv->bus[bus].target[target].ppr_1x160.flags.ppr_options << 8) |
			 nv->bus[bus].target[target].ppr_1x160.flags.ppr_bus_width;
		mr |= BIT_6;
	} else {
		mb[3] =	(nv->bus[bus].target[target].flags.flags1x80.sync_offset << 8);
	}
	mb[3] |= nv->bus[bus].target[target].sync_period;

	status = qla1280_mailbox_command(ha, mr, mb);

	
	for (lun = 0; lun < MAX_LUNS; lun++) {
		mb[0] = MBC_SET_DEVICE_QUEUE;
		mb[1] = (uint16_t)((bus ? target | BIT_7 : target) << 8);
		mb[1] |= lun;
		mb[2] = nv->bus[bus].max_queue_depth;
		mb[3] = nv->bus[bus].target[target].execution_throttle;
		status |= qla1280_mailbox_command(ha, 0x0f, mb);
	}

	if (status)
		printk(KERN_WARNING "scsi(%ld:%i:%i): "
		       "qla1280_set_target_parameters() failed\n",
		       ha->host_no, bus, target);
	return status;
}


static int
qla1280_slave_configure(struct scsi_device *device)
{
	struct scsi_qla_host *ha;
	int default_depth = 3;
	int bus = device->channel;
	int target = device->id;
	int status = 0;
	struct nvram *nv;
	unsigned long flags;

	ha = (struct scsi_qla_host *)device->host->hostdata;
	nv = &ha->nvram;

	if (qla1280_check_for_dead_scsi_bus(ha, bus))
		return 1;

	if (device->tagged_supported &&
	    (ha->bus_settings[bus].qtag_enables & (BIT_0 << target))) {
		scsi_adjust_queue_depth(device, MSG_ORDERED_TAG,
					ha->bus_settings[bus].hiwat);
	} else {
		scsi_adjust_queue_depth(device, 0, default_depth);
	}

	nv->bus[bus].target[target].parameter.enable_sync = device->sdtr;
	nv->bus[bus].target[target].parameter.enable_wide = device->wdtr;
	nv->bus[bus].target[target].ppr_1x160.flags.enable_ppr = device->ppr;

	if (driver_setup.no_sync ||
	    (driver_setup.sync_mask &&
	     (~driver_setup.sync_mask & (1 << target))))
		nv->bus[bus].target[target].parameter.enable_sync = 0;
	if (driver_setup.no_wide ||
	    (driver_setup.wide_mask &&
	     (~driver_setup.wide_mask & (1 << target))))
		nv->bus[bus].target[target].parameter.enable_wide = 0;
	if (IS_ISP1x160(ha)) {
		if (driver_setup.no_ppr ||
		    (driver_setup.ppr_mask &&
		     (~driver_setup.ppr_mask & (1 << target))))
			nv->bus[bus].target[target].ppr_1x160.flags.enable_ppr = 0;
	}

	spin_lock_irqsave(ha->host->host_lock, flags);
	if (nv->bus[bus].target[target].parameter.enable_sync)
		status = qla1280_set_target_parameters(ha, bus, target);
	qla1280_get_target_parameters(ha, device);
	spin_unlock_irqrestore(ha->host->host_lock, flags);
	return status;
}


static void
qla1280_done(struct scsi_qla_host *ha)
{
	struct srb *sp;
	struct list_head *done_q;
	int bus, target, lun;
	struct scsi_cmnd *cmd;

	ENTER("qla1280_done");

	done_q = &ha->done_q;

	while (!list_empty(done_q)) {
		sp = list_entry(done_q->next, struct srb, list);

		list_del(&sp->list);
	
		cmd = sp->cmd;
		bus = SCSI_BUS_32(cmd);
		target = SCSI_TCN_32(cmd);
		lun = SCSI_LUN_32(cmd);

		switch ((CMD_RESULT(cmd) >> 16)) {
		case DID_RESET:
			
			if (!ha->flags.abort_isp_active)
				qla1280_marker(ha, bus, target, 0, MK_SYNC_ID);
			break;
		case DID_ABORT:
			sp->flags &= ~SRB_ABORT_PENDING;
			sp->flags |= SRB_ABORTED;
			break;
		default:
			break;
		}

		
		scsi_dma_unmap(cmd);

		
		ha->actthreads--;

		if (sp->wait == NULL)
			(*(cmd)->scsi_done)(cmd);
		else
			complete(sp->wait);
	}
	LEAVE("qla1280_done");
}

static int
qla1280_return_status(struct response * sts, struct scsi_cmnd *cp)
{
	int host_status = DID_ERROR;
	uint16_t comp_status = le16_to_cpu(sts->comp_status);
	uint16_t state_flags = le16_to_cpu(sts->state_flags);
	uint32_t residual_length = le32_to_cpu(sts->residual_length);
	uint16_t scsi_status = le16_to_cpu(sts->scsi_status);
#if DEBUG_QLA1280_INTR
	static char *reason[] = {
		"DID_OK",
		"DID_NO_CONNECT",
		"DID_BUS_BUSY",
		"DID_TIME_OUT",
		"DID_BAD_TARGET",
		"DID_ABORT",
		"DID_PARITY",
		"DID_ERROR",
		"DID_RESET",
		"DID_BAD_INTR"
	};
#endif				

	ENTER("qla1280_return_status");

#if DEBUG_QLA1280_INTR
#endif

	switch (comp_status) {
	case CS_COMPLETE:
		host_status = DID_OK;
		break;

	case CS_INCOMPLETE:
		if (!(state_flags & SF_GOT_BUS))
			host_status = DID_NO_CONNECT;
		else if (!(state_flags & SF_GOT_TARGET))
			host_status = DID_BAD_TARGET;
		else if (!(state_flags & SF_SENT_CDB))
			host_status = DID_ERROR;
		else if (!(state_flags & SF_TRANSFERRED_DATA))
			host_status = DID_ERROR;
		else if (!(state_flags & SF_GOT_STATUS))
			host_status = DID_ERROR;
		else if (!(state_flags & SF_GOT_SENSE))
			host_status = DID_ERROR;
		break;

	case CS_RESET:
		host_status = DID_RESET;
		break;

	case CS_ABORTED:
		host_status = DID_ABORT;
		break;

	case CS_TIMEOUT:
		host_status = DID_TIME_OUT;
		break;

	case CS_DATA_OVERRUN:
		dprintk(2, "Data overrun 0x%x\n", residual_length);
		dprintk(2, "qla1280_return_status: response packet data\n");
		qla1280_dump_buffer(2, (char *)sts, RESPONSE_ENTRY_SIZE);
		host_status = DID_ERROR;
		break;

	case CS_DATA_UNDERRUN:
		if ((scsi_bufflen(cp) - residual_length) <
		    cp->underflow) {
			printk(KERN_WARNING
			       "scsi: Underflow detected - retrying "
			       "command.\n");
			host_status = DID_ERROR;
		} else {
			scsi_set_resid(cp, residual_length);
			host_status = DID_OK;
		}
		break;

	default:
		host_status = DID_ERROR;
		break;
	}

#if DEBUG_QLA1280_INTR
	dprintk(1, "qla1280 ISP status: host status (%s) scsi status %x\n",
		reason[host_status], scsi_status);
#endif

	LEAVE("qla1280_return_status");

	return (scsi_status & 0xff) | (host_status << 16);
}


static int __devinit
qla1280_initialize_adapter(struct scsi_qla_host *ha)
{
	struct device_reg __iomem *reg;
	int status;
	int bus;
	unsigned long flags;

	ENTER("qla1280_initialize_adapter");

	
	ha->flags.online = 0;
	ha->flags.disable_host_adapter = 0;
	ha->flags.reset_active = 0;
	ha->flags.abort_isp_active = 0;

#if defined(CONFIG_IA64_GENERIC) || defined(CONFIG_IA64_SGI_SN2)
	if (ia64_platform_is("sn2")) {
		printk(KERN_INFO "scsi(%li): Enabling SN2 PCI DMA "
		       "dual channel lockup workaround\n", ha->host_no);
		ha->flags.use_pci_vchannel = 1;
		driver_setup.no_nvram = 1;
	}
#endif

	
	if (IS_ISP1040(ha))
		driver_setup.no_nvram = 1;

	dprintk(1, "Configure PCI space for adapter...\n");

	reg = ha->iobase;

	
	WRT_REG_WORD(&reg->semaphore, 0);
	WRT_REG_WORD(&reg->host_cmd, HC_CLR_RISC_INT);
	WRT_REG_WORD(&reg->host_cmd, HC_CLR_HOST_INT);
	RD_REG_WORD(&reg->host_cmd);

	if (qla1280_read_nvram(ha)) {
		dprintk(2, "qla1280_initialize_adapter: failed to read "
			"NVRAM\n");
	}

	spin_lock_irqsave(ha->host->host_lock, flags);

	status = qla1280_load_firmware(ha);
	if (status) {
		printk(KERN_ERR "scsi(%li): initialize: pci probe failed!\n",
		       ha->host_no);
		goto out;
	}

	
	dprintk(1, "scsi(%ld): Configure NVRAM parameters\n", ha->host_no);
	qla1280_nvram_config(ha);

	if (ha->flags.disable_host_adapter) {
		status = 1;
		goto out;
	}

	status = qla1280_init_rings(ha);
	if (status)
		goto out;

	
	for (bus = 0; bus < ha->ports; bus++) {
		if (!ha->bus_settings[bus].disable_scsi_reset &&
		    qla1280_bus_reset(ha, bus) &&
		    qla1280_bus_reset(ha, bus))
			ha->bus_settings[bus].scsi_bus_dead = 1;
	}

	ha->flags.online = 1;
 out:
	spin_unlock_irqrestore(ha->host->host_lock, flags);

	if (status)
		dprintk(2, "qla1280_initialize_adapter: **** FAILED ****\n");

	LEAVE("qla1280_initialize_adapter");
	return status;
}

static const struct firmware *
qla1280_request_firmware(struct scsi_qla_host *ha)
{
	const struct firmware *fw;
	int err;
	int index;
	char *fwname;

	spin_unlock_irq(ha->host->host_lock);
	mutex_lock(&qla1280_firmware_mutex);

	index = ql1280_board_tbl[ha->devnum].fw_index;
	fw = qla1280_fw_tbl[index].fw;
	if (fw)
		goto out;

	fwname = qla1280_fw_tbl[index].fwname;
	err = request_firmware(&fw, fwname, &ha->pdev->dev);

	if (err) {
		printk(KERN_ERR "Failed to load image \"%s\" err %d\n",
		       fwname, err);
		fw = ERR_PTR(err);
		goto unlock;
	}
	if ((fw->size % 2) || (fw->size < 6)) {
		printk(KERN_ERR "Invalid firmware length %zu in image \"%s\"\n",
		       fw->size, fwname);
		release_firmware(fw);
		fw = ERR_PTR(-EINVAL);
		goto unlock;
	}

	qla1280_fw_tbl[index].fw = fw;

 out:
	ha->fwver1 = fw->data[0];
	ha->fwver2 = fw->data[1];
	ha->fwver3 = fw->data[2];
 unlock:
	mutex_unlock(&qla1280_firmware_mutex);
	spin_lock_irq(ha->host->host_lock);
	return fw;
}

static int
qla1280_chip_diag(struct scsi_qla_host *ha)
{
	uint16_t mb[MAILBOX_REGISTER_COUNT];
	struct device_reg __iomem *reg = ha->iobase;
	int status = 0;
	int cnt;
	uint16_t data;
	dprintk(3, "qla1280_chip_diag: testing device at 0x%p \n", &reg->id_l);

	dprintk(1, "scsi(%ld): Verifying chip\n", ha->host_no);

	
	WRT_REG_WORD(&reg->ictrl, ISP_RESET);

	udelay(20);
	data = qla1280_debounce_register(&reg->ictrl);
	for (cnt = 1000000; cnt && data & ISP_RESET; cnt--) {
		udelay(5);
		data = RD_REG_WORD(&reg->ictrl);
	}

	if (!cnt)
		goto fail;

	
	dprintk(3, "qla1280_chip_diag: reset register cleared by chip reset\n");

	WRT_REG_WORD(&reg->cfg_1, 0);

	WRT_REG_WORD(&reg->host_cmd, HC_RESET_RISC |
		     HC_RELEASE_RISC | HC_DISABLE_BIOS);

	RD_REG_WORD(&reg->id_l);	
	data = qla1280_debounce_register(&reg->mailbox0);

	for (cnt = 1000000; cnt && data == MBS_BUSY; cnt--) {
		udelay(5);
		data = RD_REG_WORD(&reg->mailbox0);
	}

	if (!cnt)
		goto fail;

	
	dprintk(3, "qla1280_chip_diag: Checking product ID of chip\n");

	if (RD_REG_WORD(&reg->mailbox1) != PROD_ID_1 ||
	    (RD_REG_WORD(&reg->mailbox2) != PROD_ID_2 &&
	     RD_REG_WORD(&reg->mailbox2) != PROD_ID_2a) ||
	    RD_REG_WORD(&reg->mailbox3) != PROD_ID_3 ||
	    RD_REG_WORD(&reg->mailbox4) != PROD_ID_4) {
		printk(KERN_INFO "qla1280: Wrong product ID = "
		       "0x%x,0x%x,0x%x,0x%x\n",
		       RD_REG_WORD(&reg->mailbox1),
		       RD_REG_WORD(&reg->mailbox2),
		       RD_REG_WORD(&reg->mailbox3),
		       RD_REG_WORD(&reg->mailbox4));
		goto fail;
	}

	qla1280_enable_intrs(ha);

	dprintk(1, "qla1280_chip_diag: Checking mailboxes of chip\n");
	
	mb[0] = MBC_MAILBOX_REGISTER_TEST;
	mb[1] = 0xAAAA;
	mb[2] = 0x5555;
	mb[3] = 0xAA55;
	mb[4] = 0x55AA;
	mb[5] = 0xA5A5;
	mb[6] = 0x5A5A;
	mb[7] = 0x2525;

	status = qla1280_mailbox_command(ha, 0xff, mb);
	if (status)
		goto fail;

	if (mb[1] != 0xAAAA || mb[2] != 0x5555 || mb[3] != 0xAA55 ||
	    mb[4] != 0x55AA || mb[5] != 0xA5A5 || mb[6] != 0x5A5A ||
	    mb[7] != 0x2525) {
		printk(KERN_INFO "qla1280: Failed mbox check\n");
		goto fail;
	}

	dprintk(3, "qla1280_chip_diag: exiting normally\n");
	return 0;
 fail:
	dprintk(2, "qla1280_chip_diag: **** FAILED ****\n");
	return status;
}

static int
qla1280_load_firmware_pio(struct scsi_qla_host *ha)
{
	

	const struct firmware *fw;
	const __le16 *fw_data;
	uint16_t risc_address, risc_code_size;
	uint16_t mb[MAILBOX_REGISTER_COUNT], i;
	int err = 0;

	fw = qla1280_request_firmware(ha);
	if (IS_ERR(fw))
		return PTR_ERR(fw);

	fw_data = (const __le16 *)&fw->data[0];
	ha->fwstart = __le16_to_cpu(fw_data[2]);

	
	risc_address = ha->fwstart;
	fw_data = (const __le16 *)&fw->data[6];
	risc_code_size = (fw->size - 6) / 2;

	for (i = 0; i < risc_code_size; i++) {
		mb[0] = MBC_WRITE_RAM_WORD;
		mb[1] = risc_address + i;
		mb[2] = __le16_to_cpu(fw_data[i]);

		err = qla1280_mailbox_command(ha, BIT_0 | BIT_1 | BIT_2, mb);
		if (err) {
			printk(KERN_ERR "scsi(%li): Failed to load firmware\n",
					ha->host_no);
			break;
		}
	}

	return err;
}

#define DUMP_IT_BACK 0		
static int
qla1280_load_firmware_dma(struct scsi_qla_host *ha)
{
	
	const struct firmware *fw;
	const __le16 *fw_data;
	uint16_t risc_address, risc_code_size;
	uint16_t mb[MAILBOX_REGISTER_COUNT], cnt;
	int err = 0, num, i;
#if DUMP_IT_BACK
	uint8_t *sp, *tbuf;
	dma_addr_t p_tbuf;

	tbuf = pci_alloc_consistent(ha->pdev, 8000, &p_tbuf);
	if (!tbuf)
		return -ENOMEM;
#endif

	fw = qla1280_request_firmware(ha);
	if (IS_ERR(fw))
		return PTR_ERR(fw);

	fw_data = (const __le16 *)&fw->data[0];
	ha->fwstart = __le16_to_cpu(fw_data[2]);

	
	risc_address = ha->fwstart;
	fw_data = (const __le16 *)&fw->data[6];
	risc_code_size = (fw->size - 6) / 2;

	dprintk(1, "%s: DMA RISC code (%i) words\n",
			__func__, risc_code_size);

	num = 0;
	while (risc_code_size > 0) {
		int warn __attribute__((unused)) = 0;

		cnt = 2000 >> 1;

		if (cnt > risc_code_size)
			cnt = risc_code_size;

		dprintk(2, "qla1280_setup_chip:  loading risc @ =(0x%p),"
			"%d,%d(0x%x)\n",
			fw_data, cnt, num, risc_address);
		for(i = 0; i < cnt; i++)
			((__le16 *)ha->request_ring)[i] = fw_data[i];

		mb[0] = MBC_LOAD_RAM;
		mb[1] = risc_address;
		mb[4] = cnt;
		mb[3] = ha->request_dma & 0xffff;
		mb[2] = (ha->request_dma >> 16) & 0xffff;
		mb[7] = pci_dma_hi32(ha->request_dma) & 0xffff;
		mb[6] = pci_dma_hi32(ha->request_dma) >> 16;
		dprintk(2, "%s: op=%d  0x%p = 0x%4x,0x%4x,0x%4x,0x%4x\n",
				__func__, mb[0],
				(void *)(long)ha->request_dma,
				mb[6], mb[7], mb[2], mb[3]);
		err = qla1280_mailbox_command(ha, BIT_4 | BIT_3 | BIT_2 |
				BIT_1 | BIT_0, mb);
		if (err) {
			printk(KERN_ERR "scsi(%li): Failed to load partial "
			       "segment of f\n", ha->host_no);
			goto out;
		}

#if DUMP_IT_BACK
		mb[0] = MBC_DUMP_RAM;
		mb[1] = risc_address;
		mb[4] = cnt;
		mb[3] = p_tbuf & 0xffff;
		mb[2] = (p_tbuf >> 16) & 0xffff;
		mb[7] = pci_dma_hi32(p_tbuf) & 0xffff;
		mb[6] = pci_dma_hi32(p_tbuf) >> 16;

		err = qla1280_mailbox_command(ha, BIT_4 | BIT_3 | BIT_2 |
				BIT_1 | BIT_0, mb);
		if (err) {
			printk(KERN_ERR
			       "Failed to dump partial segment of f/w\n");
			goto out;
		}
		sp = (uint8_t *)ha->request_ring;
		for (i = 0; i < (cnt << 1); i++) {
			if (tbuf[i] != sp[i] && warn++ < 10) {
				printk(KERN_ERR "%s: FW compare error @ "
						"byte(0x%x) loop#=%x\n",
						__func__, i, num);
				printk(KERN_ERR "%s: FWbyte=%x  "
						"FWfromChip=%x\n",
						__func__, sp[i], tbuf[i]);
				
			}
		}
#endif
		risc_address += cnt;
		risc_code_size = risc_code_size - cnt;
		fw_data = fw_data + cnt;
		num++;
	}

 out:
#if DUMP_IT_BACK
	pci_free_consistent(ha->pdev, 8000, tbuf, p_tbuf);
#endif
	return err;
}

static int
qla1280_start_firmware(struct scsi_qla_host *ha)
{
	uint16_t mb[MAILBOX_REGISTER_COUNT];
	int err;

	dprintk(1, "%s: Verifying checksum of loaded RISC code.\n",
			__func__);

	
	mb[0] = MBC_VERIFY_CHECKSUM;
	
	mb[1] = ha->fwstart;
	err = qla1280_mailbox_command(ha, BIT_1 | BIT_0, mb);
	if (err) {
		printk(KERN_ERR "scsi(%li): RISC checksum failed.\n", ha->host_no);
		return err;
	}

	
	dprintk(1, "%s: start firmware running.\n", __func__);
	mb[0] = MBC_EXECUTE_FIRMWARE;
	mb[1] = ha->fwstart;
	err = qla1280_mailbox_command(ha, BIT_1 | BIT_0, &mb[0]);
	if (err) {
		printk(KERN_ERR "scsi(%li): Failed to start firmware\n",
				ha->host_no);
	}

	return err;
}

static int
qla1280_load_firmware(struct scsi_qla_host *ha)
{
	
	int err;

	err = qla1280_chip_diag(ha);
	if (err)
		goto out;
	if (IS_ISP1040(ha))
		err = qla1280_load_firmware_pio(ha);
	else
		err = qla1280_load_firmware_dma(ha);
	if (err)
		goto out;
	err = qla1280_start_firmware(ha);
 out:
	return err;
}

static int
qla1280_init_rings(struct scsi_qla_host *ha)
{
	uint16_t mb[MAILBOX_REGISTER_COUNT];
	int status = 0;

	ENTER("qla1280_init_rings");

	
	memset(ha->outstanding_cmds, 0,
	       sizeof(struct srb *) * MAX_OUTSTANDING_COMMANDS);

	
	ha->request_ring_ptr = ha->request_ring;
	ha->req_ring_index = 0;
	ha->req_q_cnt = REQUEST_ENTRY_CNT;
	
	mb[0] = MBC_INIT_REQUEST_QUEUE_A64;
	mb[1] = REQUEST_ENTRY_CNT;
	mb[3] = ha->request_dma & 0xffff;
	mb[2] = (ha->request_dma >> 16) & 0xffff;
	mb[4] = 0;
	mb[7] = pci_dma_hi32(ha->request_dma) & 0xffff;
	mb[6] = pci_dma_hi32(ha->request_dma) >> 16;
	if (!(status = qla1280_mailbox_command(ha, BIT_7 | BIT_6 | BIT_4 |
					       BIT_3 | BIT_2 | BIT_1 | BIT_0,
					       &mb[0]))) {
		
		ha->response_ring_ptr = ha->response_ring;
		ha->rsp_ring_index = 0;
		
		mb[0] = MBC_INIT_RESPONSE_QUEUE_A64;
		mb[1] = RESPONSE_ENTRY_CNT;
		mb[3] = ha->response_dma & 0xffff;
		mb[2] = (ha->response_dma >> 16) & 0xffff;
		mb[5] = 0;
		mb[7] = pci_dma_hi32(ha->response_dma) & 0xffff;
		mb[6] = pci_dma_hi32(ha->response_dma) >> 16;
		status = qla1280_mailbox_command(ha, BIT_7 | BIT_6 | BIT_5 |
						 BIT_3 | BIT_2 | BIT_1 | BIT_0,
						 &mb[0]);
	}

	if (status)
		dprintk(2, "qla1280_init_rings: **** FAILED ****\n");

	LEAVE("qla1280_init_rings");
	return status;
}

static void
qla1280_print_settings(struct nvram *nv)
{
	dprintk(1, "qla1280 : initiator scsi id bus[0]=%d\n",
		nv->bus[0].config_1.initiator_id);
	dprintk(1, "qla1280 : initiator scsi id bus[1]=%d\n",
		nv->bus[1].config_1.initiator_id);

	dprintk(1, "qla1280 : bus reset delay[0]=%d\n",
		nv->bus[0].bus_reset_delay);
	dprintk(1, "qla1280 : bus reset delay[1]=%d\n",
		nv->bus[1].bus_reset_delay);

	dprintk(1, "qla1280 : retry count[0]=%d\n", nv->bus[0].retry_count);
	dprintk(1, "qla1280 : retry delay[0]=%d\n", nv->bus[0].retry_delay);
	dprintk(1, "qla1280 : retry count[1]=%d\n", nv->bus[1].retry_count);
	dprintk(1, "qla1280 : retry delay[1]=%d\n", nv->bus[1].retry_delay);

	dprintk(1, "qla1280 : async data setup time[0]=%d\n",
		nv->bus[0].config_2.async_data_setup_time);
	dprintk(1, "qla1280 : async data setup time[1]=%d\n",
		nv->bus[1].config_2.async_data_setup_time);

	dprintk(1, "qla1280 : req/ack active negation[0]=%d\n",
		nv->bus[0].config_2.req_ack_active_negation);
	dprintk(1, "qla1280 : req/ack active negation[1]=%d\n",
		nv->bus[1].config_2.req_ack_active_negation);

	dprintk(1, "qla1280 : data line active negation[0]=%d\n",
		nv->bus[0].config_2.data_line_active_negation);
	dprintk(1, "qla1280 : data line active negation[1]=%d\n",
		nv->bus[1].config_2.data_line_active_negation);

	dprintk(1, "qla1280 : disable loading risc code=%d\n",
		nv->cntr_flags_1.disable_loading_risc_code);

	dprintk(1, "qla1280 : enable 64bit addressing=%d\n",
		nv->cntr_flags_1.enable_64bit_addressing);

	dprintk(1, "qla1280 : selection timeout limit[0]=%d\n",
		nv->bus[0].selection_timeout);
	dprintk(1, "qla1280 : selection timeout limit[1]=%d\n",
		nv->bus[1].selection_timeout);

	dprintk(1, "qla1280 : max queue depth[0]=%d\n",
		nv->bus[0].max_queue_depth);
	dprintk(1, "qla1280 : max queue depth[1]=%d\n",
		nv->bus[1].max_queue_depth);
}

static void
qla1280_set_target_defaults(struct scsi_qla_host *ha, int bus, int target)
{
	struct nvram *nv = &ha->nvram;

	nv->bus[bus].target[target].parameter.renegotiate_on_error = 1;
	nv->bus[bus].target[target].parameter.auto_request_sense = 1;
	nv->bus[bus].target[target].parameter.tag_queuing = 1;
	nv->bus[bus].target[target].parameter.enable_sync = 1;
#if 1	
	nv->bus[bus].target[target].parameter.enable_wide = 1;
#endif
	nv->bus[bus].target[target].execution_throttle =
		nv->bus[bus].max_queue_depth - 1;
	nv->bus[bus].target[target].parameter.parity_checking = 1;
	nv->bus[bus].target[target].parameter.disconnect_allowed = 1;

	if (IS_ISP1x160(ha)) {
		nv->bus[bus].target[target].flags.flags1x160.device_enable = 1;
		nv->bus[bus].target[target].flags.flags1x160.sync_offset = 0x0e;
		nv->bus[bus].target[target].sync_period = 9;
		nv->bus[bus].target[target].ppr_1x160.flags.enable_ppr = 1;
		nv->bus[bus].target[target].ppr_1x160.flags.ppr_options = 2;
		nv->bus[bus].target[target].ppr_1x160.flags.ppr_bus_width = 1;
	} else {
		nv->bus[bus].target[target].flags.flags1x80.device_enable = 1;
		nv->bus[bus].target[target].flags.flags1x80.sync_offset = 12;
		nv->bus[bus].target[target].sync_period = 10;
	}
}

static void
qla1280_set_defaults(struct scsi_qla_host *ha)
{
	struct nvram *nv = &ha->nvram;
	int bus, target;

	dprintk(1, "Using defaults for NVRAM: \n");
	memset(nv, 0, sizeof(struct nvram));

	
	nv->firmware_feature.f.enable_fast_posting = 1;
	nv->firmware_feature.f.disable_synchronous_backoff = 1;
	nv->termination.scsi_bus_0_control = 3;
	nv->termination.scsi_bus_1_control = 3;
	nv->termination.auto_term_support = 1;

	nv->isp_config.burst_enable = 1;
	if (IS_ISP1040(ha))
		nv->isp_config.fifo_threshold |= 3;
	else
		nv->isp_config.fifo_threshold |= 4;

	if (IS_ISP1x160(ha))
		nv->isp_parameter = 0x01; 

	for (bus = 0; bus < MAX_BUSES; bus++) {
		nv->bus[bus].config_1.initiator_id = 7;
		nv->bus[bus].config_2.req_ack_active_negation = 1;
		nv->bus[bus].config_2.data_line_active_negation = 1;
		nv->bus[bus].selection_timeout = 250;
		nv->bus[bus].max_queue_depth = 32;

		if (IS_ISP1040(ha)) {
			nv->bus[bus].bus_reset_delay = 3;
			nv->bus[bus].config_2.async_data_setup_time = 6;
			nv->bus[bus].retry_delay = 1;
		} else {
			nv->bus[bus].bus_reset_delay = 5;
			nv->bus[bus].config_2.async_data_setup_time = 8;
		}

		for (target = 0; target < MAX_TARGETS; target++)
			qla1280_set_target_defaults(ha, bus, target);
	}
}

static int
qla1280_config_target(struct scsi_qla_host *ha, int bus, int target)
{
	struct nvram *nv = &ha->nvram;
	uint16_t mb[MAILBOX_REGISTER_COUNT];
	int status, lun;
	uint16_t flag;

	
	mb[0] = MBC_SET_TARGET_PARAMETERS;
	mb[1] = (uint16_t)((bus ? target | BIT_7 : target) << 8);

	mb[2] = (TP_RENEGOTIATE | TP_AUTO_REQUEST_SENSE | TP_TAGGED_QUEUE
		 | TP_WIDE | TP_PARITY | TP_DISCONNECT);

	if (IS_ISP1x160(ha))
		mb[3] =	nv->bus[bus].target[target].flags.flags1x160.sync_offset << 8;
	else
		mb[3] =	nv->bus[bus].target[target].flags.flags1x80.sync_offset << 8;
	mb[3] |= nv->bus[bus].target[target].sync_period;
	status = qla1280_mailbox_command(ha, 0x0f, mb);

	
	flag = (BIT_0 << target);
	if (nv->bus[bus].target[target].parameter.tag_queuing)
		ha->bus_settings[bus].qtag_enables |= flag;

	
	if (IS_ISP1x160(ha)) {
		if (nv->bus[bus].target[target].flags.flags1x160.device_enable)
			ha->bus_settings[bus].device_enables |= flag;
		ha->bus_settings[bus].lun_disables |= 0;
	} else {
		if (nv->bus[bus].target[target].flags.flags1x80.device_enable)
			ha->bus_settings[bus].device_enables |= flag;
		
		if (nv->bus[bus].target[target].flags.flags1x80.lun_disable)
			ha->bus_settings[bus].lun_disables |= flag;
	}

	
	for (lun = 0; lun < MAX_LUNS; lun++) {
		mb[0] = MBC_SET_DEVICE_QUEUE;
		mb[1] = (uint16_t)((bus ? target | BIT_7 : target) << 8);
		mb[1] |= lun;
		mb[2] = nv->bus[bus].max_queue_depth;
		mb[3] = nv->bus[bus].target[target].execution_throttle;
		status |= qla1280_mailbox_command(ha, 0x0f, mb);
	}

	return status;
}

static int
qla1280_config_bus(struct scsi_qla_host *ha, int bus)
{
	struct nvram *nv = &ha->nvram;
	uint16_t mb[MAILBOX_REGISTER_COUNT];
	int target, status;

	
	ha->bus_settings[bus].disable_scsi_reset =
		nv->bus[bus].config_1.scsi_reset_disable;

	
	ha->bus_settings[bus].id = nv->bus[bus].config_1.initiator_id;
	mb[0] = MBC_SET_INITIATOR_ID;
	mb[1] = bus ? ha->bus_settings[bus].id | BIT_7 :
		ha->bus_settings[bus].id;
	status = qla1280_mailbox_command(ha, BIT_1 | BIT_0, &mb[0]);

	
	ha->bus_settings[bus].bus_reset_delay =
		nv->bus[bus].bus_reset_delay;

	
	ha->bus_settings[bus].hiwat = nv->bus[bus].max_queue_depth - 1;

	
	for (target = 0; target < MAX_TARGETS; target++)
		status |= qla1280_config_target(ha, bus, target);

	return status;
}

static int
qla1280_nvram_config(struct scsi_qla_host *ha)
{
	struct device_reg __iomem *reg = ha->iobase;
	struct nvram *nv = &ha->nvram;
	int bus, target, status = 0;
	uint16_t mb[MAILBOX_REGISTER_COUNT];

	ENTER("qla1280_nvram_config");

	if (ha->nvram_valid) {
		
		for (bus = 0; bus < MAX_BUSES; bus++)
			for (target = 0; target < MAX_TARGETS; target++) {
				nv->bus[bus].target[target].parameter.
					auto_request_sense = 1;
			}
	} else {
		qla1280_set_defaults(ha);
	}

	qla1280_print_settings(nv);

	
	ha->flags.disable_risc_code_load =
		nv->cntr_flags_1.disable_loading_risc_code;

	if (IS_ISP1040(ha)) {
		uint16_t hwrev, cfg1, cdma_conf, ddma_conf;

		hwrev = RD_REG_WORD(&reg->cfg_0) & ISP_CFG0_HWMSK;

		cfg1 = RD_REG_WORD(&reg->cfg_1) & ~(BIT_4 | BIT_5 | BIT_6);
		cdma_conf = RD_REG_WORD(&reg->cdma_cfg);
		ddma_conf = RD_REG_WORD(&reg->ddma_cfg);

		
		if (hwrev != ISP_CFG0_1040A)
			cfg1 |= nv->isp_config.fifo_threshold << 4;

		cfg1 |= nv->isp_config.burst_enable << 2;
		WRT_REG_WORD(&reg->cfg_1, cfg1);

		WRT_REG_WORD(&reg->cdma_cfg, cdma_conf | CDMA_CONF_BENAB);
		WRT_REG_WORD(&reg->ddma_cfg, cdma_conf | DDMA_CONF_BENAB);
	} else {
		uint16_t cfg1, term;

		
		cfg1 = nv->isp_config.fifo_threshold << 4;
		cfg1 |= nv->isp_config.burst_enable << 2;
		
		if (ha->ports > 1)
			cfg1 |= BIT_13;
		WRT_REG_WORD(&reg->cfg_1, cfg1);

		
		WRT_REG_WORD(&reg->gpio_enable,
			     BIT_7 | BIT_3 | BIT_2 | BIT_1 | BIT_0);
		term = nv->termination.scsi_bus_1_control;
		term |= nv->termination.scsi_bus_0_control << 2;
		term |= nv->termination.auto_term_support << 7;
		RD_REG_WORD(&reg->id_l);	
		WRT_REG_WORD(&reg->gpio_data, term);
	}
	RD_REG_WORD(&reg->id_l);	

	
	mb[0] = MBC_SET_SYSTEM_PARAMETER;
	mb[1] = nv->isp_parameter;
	status |= qla1280_mailbox_command(ha, BIT_1 | BIT_0, &mb[0]);

	if (IS_ISP1x40(ha)) {
		
		mb[0] = MBC_SET_CLOCK_RATE;
		mb[1] = 40;
	 	status |= qla1280_mailbox_command(ha, BIT_1 | BIT_0, mb);
	}

	
	mb[0] = MBC_SET_FIRMWARE_FEATURES;
	mb[1] = nv->firmware_feature.f.enable_fast_posting;
	mb[1] |= nv->firmware_feature.f.report_lvd_bus_transition << 1;
	mb[1] |= nv->firmware_feature.f.disable_synchronous_backoff << 5;
#if defined(CONFIG_IA64_GENERIC) || defined (CONFIG_IA64_SGI_SN2)
	if (ia64_platform_is("sn2")) {
		printk(KERN_INFO "scsi(%li): Enabling SN2 PCI DMA "
		       "workaround\n", ha->host_no);
		mb[1] |= nv->firmware_feature.f.unused_9 << 9; 
	}
#endif
	status |= qla1280_mailbox_command(ha, BIT_1 | BIT_0, mb);

	
	mb[0] = MBC_SET_RETRY_COUNT;
	mb[1] = nv->bus[0].retry_count;
	mb[2] = nv->bus[0].retry_delay;
	mb[6] = nv->bus[1].retry_count;
	mb[7] = nv->bus[1].retry_delay;
	status |= qla1280_mailbox_command(ha, BIT_7 | BIT_6 | BIT_2 |
					  BIT_1 | BIT_0, &mb[0]);

	
	mb[0] = MBC_SET_ASYNC_DATA_SETUP;
	mb[1] = nv->bus[0].config_2.async_data_setup_time;
	mb[2] = nv->bus[1].config_2.async_data_setup_time;
	status |= qla1280_mailbox_command(ha, BIT_2 | BIT_1 | BIT_0, &mb[0]);

	
	mb[0] = MBC_SET_ACTIVE_NEGATION;
	mb[1] = 0;
	if (nv->bus[0].config_2.req_ack_active_negation)
		mb[1] |= BIT_5;
	if (nv->bus[0].config_2.data_line_active_negation)
		mb[1] |= BIT_4;
	mb[2] = 0;
	if (nv->bus[1].config_2.req_ack_active_negation)
		mb[2] |= BIT_5;
	if (nv->bus[1].config_2.data_line_active_negation)
		mb[2] |= BIT_4;
	status |= qla1280_mailbox_command(ha, BIT_2 | BIT_1 | BIT_0, mb);

	mb[0] = MBC_SET_DATA_OVERRUN_RECOVERY;
	mb[1] = 2;	
	status |= qla1280_mailbox_command(ha, BIT_1 | BIT_0, mb);

	
	mb[0] = MBC_SET_PCI_CONTROL;
	mb[1] = BIT_1;	
	mb[2] = BIT_1;	
	status |= qla1280_mailbox_command(ha, BIT_2 | BIT_1 | BIT_0, mb);

	mb[0] = MBC_SET_TAG_AGE_LIMIT;
	mb[1] = 8;
	status |= qla1280_mailbox_command(ha, BIT_1 | BIT_0, mb);

	
	mb[0] = MBC_SET_SELECTION_TIMEOUT;
	mb[1] = nv->bus[0].selection_timeout;
	mb[2] = nv->bus[1].selection_timeout;
	status |= qla1280_mailbox_command(ha, BIT_2 | BIT_1 | BIT_0, mb);

	for (bus = 0; bus < ha->ports; bus++)
		status |= qla1280_config_bus(ha, bus);

	if (status)
		dprintk(2, "qla1280_nvram_config: **** FAILED ****\n");

	LEAVE("qla1280_nvram_config");
	return status;
}

static uint16_t
qla1280_get_nvram_word(struct scsi_qla_host *ha, uint32_t address)
{
	uint32_t nv_cmd;
	uint16_t data;

	nv_cmd = address << 16;
	nv_cmd |= NV_READ_OP;

	data = le16_to_cpu(qla1280_nvram_request(ha, nv_cmd));

	dprintk(8, "qla1280_get_nvram_word: exiting normally NVRAM data = "
		"0x%x", data);

	return data;
}

static uint16_t
qla1280_nvram_request(struct scsi_qla_host *ha, uint32_t nv_cmd)
{
	struct device_reg __iomem *reg = ha->iobase;
	int cnt;
	uint16_t data = 0;
	uint16_t reg_data;

	

	nv_cmd <<= 5;
	for (cnt = 0; cnt < 11; cnt++) {
		if (nv_cmd & BIT_31)
			qla1280_nv_write(ha, NV_DATA_OUT);
		else
			qla1280_nv_write(ha, 0);
		nv_cmd <<= 1;
	}

	

	for (cnt = 0; cnt < 16; cnt++) {
		WRT_REG_WORD(&reg->nvram, (NV_SELECT | NV_CLOCK));
		RD_REG_WORD(&reg->id_l);	
		NVRAM_DELAY();
		data <<= 1;
		reg_data = RD_REG_WORD(&reg->nvram);
		if (reg_data & NV_DATA_IN)
			data |= BIT_0;
		WRT_REG_WORD(&reg->nvram, NV_SELECT);
		RD_REG_WORD(&reg->id_l);	
		NVRAM_DELAY();
	}

	

	WRT_REG_WORD(&reg->nvram, NV_DESELECT);
	RD_REG_WORD(&reg->id_l);	
	NVRAM_DELAY();

	return data;
}

static void
qla1280_nv_write(struct scsi_qla_host *ha, uint16_t data)
{
	struct device_reg __iomem *reg = ha->iobase;

	WRT_REG_WORD(&reg->nvram, data | NV_SELECT);
	RD_REG_WORD(&reg->id_l);	
	NVRAM_DELAY();
	WRT_REG_WORD(&reg->nvram, data | NV_SELECT | NV_CLOCK);
	RD_REG_WORD(&reg->id_l);	
	NVRAM_DELAY();
	WRT_REG_WORD(&reg->nvram, data | NV_SELECT);
	RD_REG_WORD(&reg->id_l);	
	NVRAM_DELAY();
}

static int
qla1280_mailbox_command(struct scsi_qla_host *ha, uint8_t mr, uint16_t *mb)
{
	struct device_reg __iomem *reg = ha->iobase;
	int status = 0;
	int cnt;
	uint16_t *optr, *iptr;
	uint16_t __iomem *mptr;
	uint16_t data;
	DECLARE_COMPLETION_ONSTACK(wait);
	struct timer_list timer;

	ENTER("qla1280_mailbox_command");

	if (ha->mailbox_wait) {
		printk(KERN_ERR "Warning mailbox wait already in use!\n");
	}
	ha->mailbox_wait = &wait;

	
	mptr = (uint16_t __iomem *) &reg->mailbox0;
	iptr = mb;
	for (cnt = 0; cnt < MAILBOX_REGISTER_COUNT; cnt++) {
		if (mr & BIT_0) {
			WRT_REG_WORD(mptr, (*iptr));
		}

		mr >>= 1;
		mptr++;
		iptr++;
	}

	

	
	init_timer(&timer);
	timer.expires = jiffies + 20*HZ;
	timer.data = (unsigned long)ha;
	timer.function = qla1280_mailbox_timeout;
	add_timer(&timer);

	spin_unlock_irq(ha->host->host_lock);
	WRT_REG_WORD(&reg->host_cmd, HC_SET_HOST_INT);
	data = qla1280_debounce_register(&reg->istatus);

	wait_for_completion(&wait);
	del_timer_sync(&timer);

	spin_lock_irq(ha->host->host_lock);

	ha->mailbox_wait = NULL;

	
	if (ha->mailbox_out[0] != MBS_CMD_CMP) {
		printk(KERN_WARNING "qla1280_mailbox_command: Command failed, "
		       "mailbox0 = 0x%04x, mailbox_out0 = 0x%04x, istatus = "
		       "0x%04x\n", 
		       mb[0], ha->mailbox_out[0], RD_REG_WORD(&reg->istatus));
		printk(KERN_WARNING "m0 %04x, m1 %04x, m2 %04x, m3 %04x\n",
		       RD_REG_WORD(&reg->mailbox0), RD_REG_WORD(&reg->mailbox1),
		       RD_REG_WORD(&reg->mailbox2), RD_REG_WORD(&reg->mailbox3));
		printk(KERN_WARNING "m4 %04x, m5 %04x, m6 %04x, m7 %04x\n",
		       RD_REG_WORD(&reg->mailbox4), RD_REG_WORD(&reg->mailbox5),
		       RD_REG_WORD(&reg->mailbox6), RD_REG_WORD(&reg->mailbox7));
		status = 1;
	}

	
	optr = mb;
	iptr = (uint16_t *) &ha->mailbox_out[0];
	mr = MAILBOX_REGISTER_COUNT;
	memcpy(optr, iptr, MAILBOX_REGISTER_COUNT * sizeof(uint16_t));

	if (ha->flags.reset_marker)
		qla1280_rst_aen(ha);

	if (status)
		dprintk(2, "qla1280_mailbox_command: **** FAILED, mailbox0 = "
			"0x%x ****\n", mb[0]);

	LEAVE("qla1280_mailbox_command");
	return status;
}

static void
qla1280_poll(struct scsi_qla_host *ha)
{
	struct device_reg __iomem *reg = ha->iobase;
	uint16_t data;
	LIST_HEAD(done_q);

	

	
	data = RD_REG_WORD(&reg->istatus);
	if (data & RISC_INT)
		qla1280_isr(ha, &done_q);

	if (!ha->mailbox_wait) {
		if (ha->flags.reset_marker)
			qla1280_rst_aen(ha);
	}

	if (!list_empty(&done_q))
		qla1280_done(ha);

	
}

static int
qla1280_bus_reset(struct scsi_qla_host *ha, int bus)
{
	uint16_t mb[MAILBOX_REGISTER_COUNT];
	uint16_t reset_delay;
	int status;

	dprintk(3, "qla1280_bus_reset: entered\n");

	if (qla1280_verbose)
		printk(KERN_INFO "scsi(%li:%i): Resetting SCSI BUS\n",
		       ha->host_no, bus);

	reset_delay = ha->bus_settings[bus].bus_reset_delay;
	mb[0] = MBC_BUS_RESET;
	mb[1] = reset_delay;
	mb[2] = (uint16_t) bus;
	status = qla1280_mailbox_command(ha, BIT_2 | BIT_1 | BIT_0, &mb[0]);

	if (status) {
		if (ha->bus_settings[bus].failed_reset_count > 2)
			ha->bus_settings[bus].scsi_bus_dead = 1;
		ha->bus_settings[bus].failed_reset_count++;
	} else {
		spin_unlock_irq(ha->host->host_lock);
		ssleep(reset_delay);
		spin_lock_irq(ha->host->host_lock);

		ha->bus_settings[bus].scsi_bus_dead = 0;
		ha->bus_settings[bus].failed_reset_count = 0;
		ha->bus_settings[bus].reset_marker = 0;
		
		qla1280_marker(ha, bus, 0, 0, MK_SYNC_ALL);
	}


	if (status)
		dprintk(2, "qla1280_bus_reset: **** FAILED ****\n");
	else
		dprintk(3, "qla1280_bus_reset: exiting normally\n");

	return status;
}

static int
qla1280_device_reset(struct scsi_qla_host *ha, int bus, int target)
{
	uint16_t mb[MAILBOX_REGISTER_COUNT];
	int status;

	ENTER("qla1280_device_reset");

	mb[0] = MBC_ABORT_TARGET;
	mb[1] = (bus ? (target | BIT_7) : target) << 8;
	mb[2] = 1;
	status = qla1280_mailbox_command(ha, BIT_2 | BIT_1 | BIT_0, &mb[0]);

	
	qla1280_marker(ha, bus, target, 0, MK_SYNC_ID);

	if (status)
		dprintk(2, "qla1280_device_reset: **** FAILED ****\n");

	LEAVE("qla1280_device_reset");
	return status;
}

static int
qla1280_abort_command(struct scsi_qla_host *ha, struct srb * sp, int handle)
{
	uint16_t mb[MAILBOX_REGISTER_COUNT];
	unsigned int bus, target, lun;
	int status;

	ENTER("qla1280_abort_command");

	bus = SCSI_BUS_32(sp->cmd);
	target = SCSI_TCN_32(sp->cmd);
	lun = SCSI_LUN_32(sp->cmd);

	sp->flags |= SRB_ABORT_PENDING;

	mb[0] = MBC_ABORT_COMMAND;
	mb[1] = (bus ? target | BIT_7 : target) << 8 | lun;
	mb[2] = handle >> 16;
	mb[3] = handle & 0xffff;
	status = qla1280_mailbox_command(ha, 0x0f, &mb[0]);

	if (status) {
		dprintk(2, "qla1280_abort_command: **** FAILED ****\n");
		sp->flags &= ~SRB_ABORT_PENDING;
	}


	LEAVE("qla1280_abort_command");
	return status;
}

static void
qla1280_reset_adapter(struct scsi_qla_host *ha)
{
	struct device_reg __iomem *reg = ha->iobase;

	ENTER("qla1280_reset_adapter");

	
	ha->flags.online = 0;
	WRT_REG_WORD(&reg->ictrl, ISP_RESET);
	WRT_REG_WORD(&reg->host_cmd,
		     HC_RESET_RISC | HC_RELEASE_RISC | HC_DISABLE_BIOS);
	RD_REG_WORD(&reg->id_l);	

	LEAVE("qla1280_reset_adapter");
}

static void
qla1280_marker(struct scsi_qla_host *ha, int bus, int id, int lun, u8 type)
{
	struct mrk_entry *pkt;

	ENTER("qla1280_marker");

	
	if ((pkt = (struct mrk_entry *) qla1280_req_pkt(ha))) {
		pkt->entry_type = MARKER_TYPE;
		pkt->lun = (uint8_t) lun;
		pkt->target = (uint8_t) (bus ? (id | BIT_7) : id);
		pkt->modifier = type;
		pkt->entry_status = 0;

		
		qla1280_isp_cmd(ha);
	}

	LEAVE("qla1280_marker");
}


#ifdef QLA_64BIT_PTR
static int
qla1280_64bit_start_scsi(struct scsi_qla_host *ha, struct srb * sp)
{
	struct device_reg __iomem *reg = ha->iobase;
	struct scsi_cmnd *cmd = sp->cmd;
	cmd_a64_entry_t *pkt;
	__le32 *dword_ptr;
	dma_addr_t dma_handle;
	int status = 0;
	int cnt;
	int req_cnt;
	int seg_cnt;
	u8 dir;

	ENTER("qla1280_64bit_start_scsi:");

	
	req_cnt = 1;
	seg_cnt = scsi_dma_map(cmd);
	if (seg_cnt > 0) {
		if (seg_cnt > 2) {
			req_cnt += (seg_cnt - 2) / 5;
			if ((seg_cnt - 2) % 5)
				req_cnt++;
		}
	} else if (seg_cnt < 0) {
		status = 1;
		goto out;
	}

	if ((req_cnt + 2) >= ha->req_q_cnt) {
		
		cnt = RD_REG_WORD(&reg->mailbox4);
		if (ha->req_ring_index < cnt)
			ha->req_q_cnt = cnt - ha->req_ring_index;
		else
			ha->req_q_cnt =
				REQUEST_ENTRY_CNT - (ha->req_ring_index - cnt);
	}

	dprintk(3, "Number of free entries=(%d) seg_cnt=0x%x\n",
		ha->req_q_cnt, seg_cnt);

	
	if ((req_cnt + 2) >= ha->req_q_cnt) {
		status = SCSI_MLQUEUE_HOST_BUSY;
		dprintk(2, "qla1280_start_scsi: in-ptr=0x%x  req_q_cnt="
			"0x%xreq_cnt=0x%x", ha->req_ring_index, ha->req_q_cnt,
			req_cnt);
		goto out;
	}

	
	for (cnt = 0; cnt < MAX_OUTSTANDING_COMMANDS &&
		     ha->outstanding_cmds[cnt] != NULL; cnt++);

	if (cnt >= MAX_OUTSTANDING_COMMANDS) {
		status = SCSI_MLQUEUE_HOST_BUSY;
		dprintk(2, "qla1280_start_scsi: NO ROOM IN "
			"OUTSTANDING ARRAY, req_q_cnt=0x%x", ha->req_q_cnt);
		goto out;
	}

	ha->outstanding_cmds[cnt] = sp;
	ha->req_q_cnt -= req_cnt;
	CMD_HANDLE(sp->cmd) = (unsigned char *)(unsigned long)(cnt + 1);

	dprintk(2, "start: cmd=%p sp=%p CDB=%xm, handle %lx\n", cmd, sp,
		cmd->cmnd[0], (long)CMD_HANDLE(sp->cmd));
	dprintk(2, "             bus %i, target %i, lun %i\n",
		SCSI_BUS_32(cmd), SCSI_TCN_32(cmd), SCSI_LUN_32(cmd));
	qla1280_dump_buffer(2, cmd->cmnd, MAX_COMMAND_SIZE);

	pkt = (cmd_a64_entry_t *) ha->request_ring_ptr;

	pkt->entry_type = COMMAND_A64_TYPE;
	pkt->entry_count = (uint8_t) req_cnt;
	pkt->sys_define = (uint8_t) ha->req_ring_index;
	pkt->entry_status = 0;
	pkt->handle = cpu_to_le32(cnt);

	
	memset(((char *)pkt + 8), 0, (REQUEST_ENTRY_SIZE - 8));

	
	pkt->timeout = cpu_to_le16(cmd->request->timeout/HZ);

	
	pkt->lun = SCSI_LUN_32(cmd);
	pkt->target = SCSI_BUS_32(cmd) ?
		(SCSI_TCN_32(cmd) | BIT_7) : SCSI_TCN_32(cmd);

	
	if (cmd->device->simple_tags)
		pkt->control_flags |= cpu_to_le16(BIT_3);

	
	pkt->cdb_len = cpu_to_le16(CMD_CDBLEN(cmd));
	memcpy(pkt->scsi_cdb, CMD_CDBP(cmd), CMD_CDBLEN(cmd));
	

	
	dir = qla1280_data_direction(cmd);
	pkt->control_flags |= cpu_to_le16(dir);

	
	pkt->dseg_count = cpu_to_le16(seg_cnt);

	if (seg_cnt) {	
		struct scatterlist *sg, *s;
		int remseg = seg_cnt;

		sg = scsi_sglist(cmd);

		
		dword_ptr = (u32 *)&pkt->dseg_0_address;

		
		for_each_sg(sg, s, seg_cnt, cnt) {
			if (cnt == 2)
				break;

			dma_handle = sg_dma_address(s);
#if defined(CONFIG_IA64_GENERIC) || defined(CONFIG_IA64_SGI_SN2)
			if (ha->flags.use_pci_vchannel)
				sn_pci_set_vchan(ha->pdev,
						 (unsigned long *)&dma_handle,
						 SCSI_BUS_32(cmd));
#endif
			*dword_ptr++ =
				cpu_to_le32(pci_dma_lo32(dma_handle));
			*dword_ptr++ =
				cpu_to_le32(pci_dma_hi32(dma_handle));
			*dword_ptr++ = cpu_to_le32(sg_dma_len(s));
			dprintk(3, "S/G Segment phys_addr=%x %x, len=0x%x\n",
				cpu_to_le32(pci_dma_hi32(dma_handle)),
				cpu_to_le32(pci_dma_lo32(dma_handle)),
				cpu_to_le32(sg_dma_len(sg_next(s))));
			remseg--;
		}
		dprintk(5, "qla1280_64bit_start_scsi: Scatter/gather "
			"command packet data - b %i, t %i, l %i \n",
			SCSI_BUS_32(cmd), SCSI_TCN_32(cmd),
			SCSI_LUN_32(cmd));
		qla1280_dump_buffer(5, (char *)pkt,
				    REQUEST_ENTRY_SIZE);

		dprintk(3, "S/G Building Continuation...seg_cnt=0x%x "
			"remains\n", seg_cnt);

		while (remseg > 0) {
			
			sg = s;
			
			ha->req_ring_index++;
			if (ha->req_ring_index == REQUEST_ENTRY_CNT) {
				ha->req_ring_index = 0;
				ha->request_ring_ptr =
					ha->request_ring;
			} else
				ha->request_ring_ptr++;

			pkt = (cmd_a64_entry_t *)ha->request_ring_ptr;

			
			memset(pkt, 0, REQUEST_ENTRY_SIZE);

			
			((struct cont_a64_entry *) pkt)->entry_type =
				CONTINUE_A64_TYPE;
			((struct cont_a64_entry *) pkt)->entry_count = 1;
			((struct cont_a64_entry *) pkt)->sys_define =
				(uint8_t)ha->req_ring_index;
			
			dword_ptr =
				(u32 *)&((struct cont_a64_entry *) pkt)->dseg_0_address;

			
			for_each_sg(sg, s, remseg, cnt) {
				if (cnt == 5)
					break;
				dma_handle = sg_dma_address(s);
#if defined(CONFIG_IA64_GENERIC) || defined(CONFIG_IA64_SGI_SN2)
				if (ha->flags.use_pci_vchannel)
					sn_pci_set_vchan(ha->pdev,
							 (unsigned long *)&dma_handle,
							 SCSI_BUS_32(cmd));
#endif
				*dword_ptr++ =
					cpu_to_le32(pci_dma_lo32(dma_handle));
				*dword_ptr++ =
					cpu_to_le32(pci_dma_hi32(dma_handle));
				*dword_ptr++ =
					cpu_to_le32(sg_dma_len(s));
				dprintk(3, "S/G Segment Cont. phys_addr=%x %x, len=0x%x\n",
					cpu_to_le32(pci_dma_hi32(dma_handle)),
					cpu_to_le32(pci_dma_lo32(dma_handle)),
					cpu_to_le32(sg_dma_len(s)));
			}
			remseg -= cnt;
			dprintk(5, "qla1280_64bit_start_scsi: "
				"continuation packet data - b %i, t "
				"%i, l %i \n", SCSI_BUS_32(cmd),
				SCSI_TCN_32(cmd), SCSI_LUN_32(cmd));
			qla1280_dump_buffer(5, (char *)pkt,
					    REQUEST_ENTRY_SIZE);
		}
	} else {	
		dprintk(5, "qla1280_64bit_start_scsi: No data, command "
			"packet data - b %i, t %i, l %i \n",
			SCSI_BUS_32(cmd), SCSI_TCN_32(cmd), SCSI_LUN_32(cmd));
		qla1280_dump_buffer(5, (char *)pkt, REQUEST_ENTRY_SIZE);
	}
	
	ha->req_ring_index++;
	if (ha->req_ring_index == REQUEST_ENTRY_CNT) {
		ha->req_ring_index = 0;
		ha->request_ring_ptr = ha->request_ring;
	} else
		ha->request_ring_ptr++;

	
	dprintk(2,
		"qla1280_64bit_start_scsi: Wakeup RISC for pending command\n");
	sp->flags |= SRB_SENT;
	ha->actthreads++;
	WRT_REG_WORD(&reg->mailbox4, ha->req_ring_index);
	
	mmiowb();

 out:
	if (status)
		dprintk(2, "qla1280_64bit_start_scsi: **** FAILED ****\n");
	else
		dprintk(3, "qla1280_64bit_start_scsi: exiting normally\n");

	return status;
}
#else 

static int
qla1280_32bit_start_scsi(struct scsi_qla_host *ha, struct srb * sp)
{
	struct device_reg __iomem *reg = ha->iobase;
	struct scsi_cmnd *cmd = sp->cmd;
	struct cmd_entry *pkt;
	__le32 *dword_ptr;
	int status = 0;
	int cnt;
	int req_cnt;
	int seg_cnt;
	u8 dir;

	ENTER("qla1280_32bit_start_scsi");

	dprintk(1, "32bit_start: cmd=%p sp=%p CDB=%x\n", cmd, sp,
		cmd->cmnd[0]);

	
	req_cnt = 1;
	seg_cnt = scsi_dma_map(cmd);
	if (seg_cnt) {
		if (seg_cnt > 4) {
			req_cnt += (seg_cnt - 4) / 7;
			if ((seg_cnt - 4) % 7)
				req_cnt++;
		}
		dprintk(3, "S/G Transfer cmd=%p seg_cnt=0x%x, req_cnt=%x\n",
			cmd, seg_cnt, req_cnt);
	} else if (seg_cnt < 0) {
		status = 1;
		goto out;
	}

	if ((req_cnt + 2) >= ha->req_q_cnt) {
		
		cnt = RD_REG_WORD(&reg->mailbox4);
		if (ha->req_ring_index < cnt)
			ha->req_q_cnt = cnt - ha->req_ring_index;
		else
			ha->req_q_cnt =
				REQUEST_ENTRY_CNT - (ha->req_ring_index - cnt);
	}

	dprintk(3, "Number of free entries=(%d) seg_cnt=0x%x\n",
		ha->req_q_cnt, seg_cnt);
	
	if ((req_cnt + 2) >= ha->req_q_cnt) {
		status = SCSI_MLQUEUE_HOST_BUSY;
		dprintk(2, "qla1280_32bit_start_scsi: in-ptr=0x%x, "
			"req_q_cnt=0x%x, req_cnt=0x%x", ha->req_ring_index,
			ha->req_q_cnt, req_cnt);
		goto out;
	}

	
	for (cnt = 0; cnt < MAX_OUTSTANDING_COMMANDS &&
		     (ha->outstanding_cmds[cnt] != 0); cnt++) ;

	if (cnt >= MAX_OUTSTANDING_COMMANDS) {
		status = SCSI_MLQUEUE_HOST_BUSY;
		dprintk(2, "qla1280_32bit_start_scsi: NO ROOM IN OUTSTANDING "
			"ARRAY, req_q_cnt=0x%x\n", ha->req_q_cnt);
		goto out;
	}

	CMD_HANDLE(sp->cmd) = (unsigned char *) (unsigned long)(cnt + 1);
	ha->outstanding_cmds[cnt] = sp;
	ha->req_q_cnt -= req_cnt;

	pkt = (struct cmd_entry *) ha->request_ring_ptr;

	pkt->entry_type = COMMAND_TYPE;
	pkt->entry_count = (uint8_t) req_cnt;
	pkt->sys_define = (uint8_t) ha->req_ring_index;
	pkt->entry_status = 0;
	pkt->handle = cpu_to_le32(cnt);

	
	memset(((char *)pkt + 8), 0, (REQUEST_ENTRY_SIZE - 8));

	
	pkt->timeout = cpu_to_le16(cmd->request->timeout/HZ);

	
	pkt->lun = SCSI_LUN_32(cmd);
	pkt->target = SCSI_BUS_32(cmd) ?
		(SCSI_TCN_32(cmd) | BIT_7) : SCSI_TCN_32(cmd);

	
	if (cmd->device->simple_tags)
		pkt->control_flags |= cpu_to_le16(BIT_3);

	
	pkt->cdb_len = cpu_to_le16(CMD_CDBLEN(cmd));
	memcpy(pkt->scsi_cdb, CMD_CDBP(cmd), CMD_CDBLEN(cmd));

	
	
	dir = qla1280_data_direction(cmd);
	pkt->control_flags |= cpu_to_le16(dir);

	
	pkt->dseg_count = cpu_to_le16(seg_cnt);

	if (seg_cnt) {
		struct scatterlist *sg, *s;
		int remseg = seg_cnt;

		sg = scsi_sglist(cmd);

		
		dword_ptr = &pkt->dseg_0_address;

		dprintk(3, "Building S/G data segments..\n");
		qla1280_dump_buffer(1, (char *)sg, 4 * 16);

		
		for_each_sg(sg, s, seg_cnt, cnt) {
			if (cnt == 4)
				break;
			*dword_ptr++ =
				cpu_to_le32(pci_dma_lo32(sg_dma_address(s)));
			*dword_ptr++ = cpu_to_le32(sg_dma_len(s));
			dprintk(3, "S/G Segment phys_addr=0x%lx, len=0x%x\n",
				(pci_dma_lo32(sg_dma_address(s))),
				(sg_dma_len(s)));
			remseg--;
		}
		dprintk(3, "S/G Building Continuation"
			"...seg_cnt=0x%x remains\n", seg_cnt);
		while (remseg > 0) {
			
			sg = s;
			
			ha->req_ring_index++;
			if (ha->req_ring_index == REQUEST_ENTRY_CNT) {
				ha->req_ring_index = 0;
				ha->request_ring_ptr =
					ha->request_ring;
			} else
				ha->request_ring_ptr++;

			pkt = (struct cmd_entry *)ha->request_ring_ptr;

			
			memset(pkt, 0, REQUEST_ENTRY_SIZE);

			
			((struct cont_entry *) pkt)->
				entry_type = CONTINUE_TYPE;
			((struct cont_entry *) pkt)->entry_count = 1;

			((struct cont_entry *) pkt)->sys_define =
				(uint8_t) ha->req_ring_index;

			
			dword_ptr =
				&((struct cont_entry *) pkt)->dseg_0_address;

			
			for_each_sg(sg, s, remseg, cnt) {
				if (cnt == 7)
					break;
				*dword_ptr++ =
					cpu_to_le32(pci_dma_lo32(sg_dma_address(s)));
				*dword_ptr++ =
					cpu_to_le32(sg_dma_len(s));
				dprintk(1,
					"S/G Segment Cont. phys_addr=0x%x, "
					"len=0x%x\n",
					cpu_to_le32(pci_dma_lo32(sg_dma_address(s))),
					cpu_to_le32(sg_dma_len(s)));
			}
			remseg -= cnt;
			dprintk(5, "qla1280_32bit_start_scsi: "
				"continuation packet data - "
				"scsi(%i:%i:%i)\n", SCSI_BUS_32(cmd),
				SCSI_TCN_32(cmd), SCSI_LUN_32(cmd));
			qla1280_dump_buffer(5, (char *)pkt,
					    REQUEST_ENTRY_SIZE);
		}
	} else {	
		dprintk(5, "qla1280_32bit_start_scsi: No data, command "
			"packet data - \n");
		qla1280_dump_buffer(5, (char *)pkt, REQUEST_ENTRY_SIZE);
	}
	dprintk(5, "qla1280_32bit_start_scsi: First IOCB block:\n");
	qla1280_dump_buffer(5, (char *)ha->request_ring_ptr,
			    REQUEST_ENTRY_SIZE);

	
	ha->req_ring_index++;
	if (ha->req_ring_index == REQUEST_ENTRY_CNT) {
		ha->req_ring_index = 0;
		ha->request_ring_ptr = ha->request_ring;
	} else
		ha->request_ring_ptr++;

	
	dprintk(2, "qla1280_32bit_start_scsi: Wakeup RISC "
		"for pending command\n");
	sp->flags |= SRB_SENT;
	ha->actthreads++;
	WRT_REG_WORD(&reg->mailbox4, ha->req_ring_index);
	
	mmiowb();

out:
	if (status)
		dprintk(2, "qla1280_32bit_start_scsi: **** FAILED ****\n");

	LEAVE("qla1280_32bit_start_scsi");

	return status;
}
#endif

static request_t *
qla1280_req_pkt(struct scsi_qla_host *ha)
{
	struct device_reg __iomem *reg = ha->iobase;
	request_t *pkt = NULL;
	int cnt;
	uint32_t timer;

	ENTER("qla1280_req_pkt");

	
	for (timer = 15000000; timer; timer--) {
		if (ha->req_q_cnt > 0) {
			
			cnt = RD_REG_WORD(&reg->mailbox4);
			if (ha->req_ring_index < cnt)
				ha->req_q_cnt = cnt - ha->req_ring_index;
			else
				ha->req_q_cnt =
					REQUEST_ENTRY_CNT - (ha->req_ring_index - cnt);
		}

		
		if (ha->req_q_cnt > 0) {
			ha->req_q_cnt--;
			pkt = ha->request_ring_ptr;

			
			memset(pkt, 0, REQUEST_ENTRY_SIZE);

			
			pkt->sys_define = (uint8_t) ha->req_ring_index;

			
			pkt->entry_count = 1;

			break;
		}

		udelay(2);	

		
		qla1280_poll(ha);
	}

	if (!pkt)
		dprintk(2, "qla1280_req_pkt: **** FAILED ****\n");
	else
		dprintk(3, "qla1280_req_pkt: exiting normally\n");

	return pkt;
}

static void
qla1280_isp_cmd(struct scsi_qla_host *ha)
{
	struct device_reg __iomem *reg = ha->iobase;

	ENTER("qla1280_isp_cmd");

	dprintk(5, "qla1280_isp_cmd: IOCB data:\n");
	qla1280_dump_buffer(5, (char *)ha->request_ring_ptr,
			    REQUEST_ENTRY_SIZE);

	
	ha->req_ring_index++;
	if (ha->req_ring_index == REQUEST_ENTRY_CNT) {
		ha->req_ring_index = 0;
		ha->request_ring_ptr = ha->request_ring;
	} else
		ha->request_ring_ptr++;

	WRT_REG_WORD(&reg->mailbox4, ha->req_ring_index);
	mmiowb();

	LEAVE("qla1280_isp_cmd");
}


static void
qla1280_isr(struct scsi_qla_host *ha, struct list_head *done_q)
{
	struct device_reg __iomem *reg = ha->iobase;
	struct response *pkt;
	struct srb *sp = NULL;
	uint16_t mailbox[MAILBOX_REGISTER_COUNT];
	uint16_t *wptr;
	uint32_t index;
	u16 istatus;

	ENTER("qla1280_isr");

	istatus = RD_REG_WORD(&reg->istatus);
	if (!(istatus & (RISC_INT | PCI_INT)))
		return;

	
	mailbox[5] = RD_REG_WORD(&reg->mailbox5);

	

	mailbox[0] = RD_REG_WORD_dmasync(&reg->semaphore);

	if (mailbox[0] & BIT_0) {
		
		

		wptr = &mailbox[0];
		*wptr++ = RD_REG_WORD(&reg->mailbox0);
		*wptr++ = RD_REG_WORD(&reg->mailbox1);
		*wptr = RD_REG_WORD(&reg->mailbox2);
		if (mailbox[0] != MBA_SCSI_COMPLETION) {
			wptr++;
			*wptr++ = RD_REG_WORD(&reg->mailbox3);
			*wptr++ = RD_REG_WORD(&reg->mailbox4);
			wptr++;
			*wptr++ = RD_REG_WORD(&reg->mailbox6);
			*wptr = RD_REG_WORD(&reg->mailbox7);
		}

		

		WRT_REG_WORD(&reg->semaphore, 0);
		WRT_REG_WORD(&reg->host_cmd, HC_CLR_RISC_INT);

		dprintk(5, "qla1280_isr: mailbox interrupt mailbox[0] = 0x%x",
			mailbox[0]);

		
		switch (mailbox[0]) {
		case MBA_SCSI_COMPLETION:	
			dprintk(5, "qla1280_isr: mailbox SCSI response "
				"completion\n");

			if (ha->flags.online) {
				
				index = mailbox[2] << 16 | mailbox[1];

				
				if (index < MAX_OUTSTANDING_COMMANDS)
					sp = ha->outstanding_cmds[index];
				else
					sp = NULL;

				if (sp) {
					
					ha->outstanding_cmds[index] = NULL;

					
					CMD_RESULT(sp->cmd) = 0;
					CMD_HANDLE(sp->cmd) = COMPLETED_HANDLE;

					
					list_add_tail(&sp->list, done_q);
				} else {
					printk(KERN_WARNING
					       "qla1280: ISP invalid handle\n");
				}
			}
			break;

		case MBA_BUS_RESET:	
			ha->flags.reset_marker = 1;
			index = mailbox[6] & BIT_0;
			ha->bus_settings[index].reset_marker = 1;

			printk(KERN_DEBUG "qla1280_isr(): index %i "
			       "asynchronous BUS_RESET\n", index);
			break;

		case MBA_SYSTEM_ERR:	
			printk(KERN_WARNING
			       "qla1280: ISP System Error - mbx1=%xh, mbx2="
			       "%xh, mbx3=%xh\n", mailbox[1], mailbox[2],
			       mailbox[3]);
			break;

		case MBA_REQ_TRANSFER_ERR:	
			printk(KERN_WARNING
			       "qla1280: ISP Request Transfer Error\n");
			break;

		case MBA_RSP_TRANSFER_ERR:	
			printk(KERN_WARNING
			       "qla1280: ISP Response Transfer Error\n");
			break;

		case MBA_WAKEUP_THRES:	
			dprintk(2, "qla1280_isr: asynchronous WAKEUP_THRES\n");
			break;

		case MBA_TIMEOUT_RESET:	
			dprintk(2,
				"qla1280_isr: asynchronous TIMEOUT_RESET\n");
			break;

		case MBA_DEVICE_RESET:	
			printk(KERN_INFO "qla1280_isr(): asynchronous "
			       "BUS_DEVICE_RESET\n");

			ha->flags.reset_marker = 1;
			index = mailbox[6] & BIT_0;
			ha->bus_settings[index].reset_marker = 1;
			break;

		case MBA_BUS_MODE_CHANGE:
			dprintk(2,
				"qla1280_isr: asynchronous BUS_MODE_CHANGE\n");
			break;

		default:
			
			if (mailbox[0] < MBA_ASYNC_EVENT) {
				wptr = &mailbox[0];
				memcpy((uint16_t *) ha->mailbox_out, wptr,
				       MAILBOX_REGISTER_COUNT *
				       sizeof(uint16_t));

				if(ha->mailbox_wait != NULL)
					complete(ha->mailbox_wait);
			}
			break;
		}
	} else {
		WRT_REG_WORD(&reg->host_cmd, HC_CLR_RISC_INT);
	}

	if (!(ha->flags.online && !ha->mailbox_wait)) {
		dprintk(2, "qla1280_isr: Response pointer Error\n");
		goto out;
	}

	if (mailbox[5] >= RESPONSE_ENTRY_CNT)
		goto out;

	while (ha->rsp_ring_index != mailbox[5]) {
		pkt = ha->response_ring_ptr;

		dprintk(5, "qla1280_isr: ha->rsp_ring_index = 0x%x, mailbox[5]"
			" = 0x%x\n", ha->rsp_ring_index, mailbox[5]);
		dprintk(5,"qla1280_isr: response packet data\n");
		qla1280_dump_buffer(5, (char *)pkt, RESPONSE_ENTRY_SIZE);

		if (pkt->entry_type == STATUS_TYPE) {
			if ((le16_to_cpu(pkt->scsi_status) & 0xff)
			    || pkt->comp_status || pkt->entry_status) {
				dprintk(2, "qla1280_isr: ha->rsp_ring_index = "
					"0x%x mailbox[5] = 0x%x, comp_status "
					"= 0x%x, scsi_status = 0x%x\n",
					ha->rsp_ring_index, mailbox[5],
					le16_to_cpu(pkt->comp_status),
					le16_to_cpu(pkt->scsi_status));
			}
		} else {
			dprintk(2, "qla1280_isr: ha->rsp_ring_index = "
				"0x%x, mailbox[5] = 0x%x\n",
				ha->rsp_ring_index, mailbox[5]);
			dprintk(2, "qla1280_isr: response packet data\n");
			qla1280_dump_buffer(2, (char *)pkt,
					    RESPONSE_ENTRY_SIZE);
		}

		if (pkt->entry_type == STATUS_TYPE || pkt->entry_status) {
			dprintk(2, "status: Cmd %p, handle %i\n",
				ha->outstanding_cmds[pkt->handle]->cmd,
				pkt->handle);
			if (pkt->entry_type == STATUS_TYPE)
				qla1280_status_entry(ha, pkt, done_q);
			else
				qla1280_error_entry(ha, pkt, done_q);
			
			ha->rsp_ring_index++;
			if (ha->rsp_ring_index == RESPONSE_ENTRY_CNT) {
				ha->rsp_ring_index = 0;
				ha->response_ring_ptr =	ha->response_ring;
			} else
				ha->response_ring_ptr++;
			WRT_REG_WORD(&reg->mailbox5, ha->rsp_ring_index);
		}
	}
	
 out:
	LEAVE("qla1280_isr");
}

static void
qla1280_rst_aen(struct scsi_qla_host *ha)
{
	uint8_t bus;

	ENTER("qla1280_rst_aen");

	if (ha->flags.online && !ha->flags.reset_active &&
	    !ha->flags.abort_isp_active) {
		ha->flags.reset_active = 1;
		while (ha->flags.reset_marker) {
			
			ha->flags.reset_marker = 0;
			for (bus = 0; bus < ha->ports &&
				     !ha->flags.reset_marker; bus++) {
				if (ha->bus_settings[bus].reset_marker) {
					ha->bus_settings[bus].reset_marker = 0;
					qla1280_marker(ha, bus, 0, 0,
						       MK_SYNC_ALL);
				}
			}
		}
	}

	LEAVE("qla1280_rst_aen");
}


static void
qla1280_status_entry(struct scsi_qla_host *ha, struct response *pkt,
		     struct list_head *done_q)
{
	unsigned int bus, target, lun;
	int sense_sz;
	struct srb *sp;
	struct scsi_cmnd *cmd;
	uint32_t handle = le32_to_cpu(pkt->handle);
	uint16_t scsi_status = le16_to_cpu(pkt->scsi_status);
	uint16_t comp_status = le16_to_cpu(pkt->comp_status);

	ENTER("qla1280_status_entry");

	
	if (handle < MAX_OUTSTANDING_COMMANDS)
		sp = ha->outstanding_cmds[handle];
	else
		sp = NULL;

	if (!sp) {
		printk(KERN_WARNING "qla1280: Status Entry invalid handle\n");
		goto out;
	}

	
	ha->outstanding_cmds[handle] = NULL;

	cmd = sp->cmd;

	
	bus = SCSI_BUS_32(cmd);
	target = SCSI_TCN_32(cmd);
	lun = SCSI_LUN_32(cmd);

	if (comp_status || scsi_status) {
		dprintk(3, "scsi: comp_status = 0x%x, scsi_status = "
			"0x%x, handle = 0x%x\n", comp_status,
			scsi_status, handle);
	}

	
	if ((scsi_status & 0xFF) == SAM_STAT_TASK_SET_FULL ||
	    (scsi_status & 0xFF) == SAM_STAT_BUSY) {
		CMD_RESULT(cmd) = scsi_status & 0xff;
	} else {

		
		CMD_RESULT(cmd) = qla1280_return_status(pkt, cmd);

		if (scsi_status & SAM_STAT_CHECK_CONDITION) {
			if (comp_status != CS_ARS_FAILED) {
				uint16_t req_sense_length =
					le16_to_cpu(pkt->req_sense_length);
				if (req_sense_length < CMD_SNSLEN(cmd))
					sense_sz = req_sense_length;
				else
					sense_sz = CMD_SNSLEN(cmd) - 1;

				memcpy(cmd->sense_buffer,
				       &pkt->req_sense_data, sense_sz);
			} else
				sense_sz = 0;
			memset(cmd->sense_buffer + sense_sz, 0,
			       SCSI_SENSE_BUFFERSIZE - sense_sz);

			dprintk(2, "qla1280_status_entry: Check "
				"condition Sense data, b %i, t %i, "
				"l %i\n", bus, target, lun);
			if (sense_sz)
				qla1280_dump_buffer(2,
						    (char *)cmd->sense_buffer,
						    sense_sz);
		}
	}

	CMD_HANDLE(sp->cmd) = COMPLETED_HANDLE;

	
	list_add_tail(&sp->list, done_q);
 out:
	LEAVE("qla1280_status_entry");
}

static void
qla1280_error_entry(struct scsi_qla_host *ha, struct response *pkt,
		    struct list_head *done_q)
{
	struct srb *sp;
	uint32_t handle = le32_to_cpu(pkt->handle);

	ENTER("qla1280_error_entry");

	if (pkt->entry_status & BIT_3)
		dprintk(2, "qla1280_error_entry: BAD PAYLOAD flag error\n");
	else if (pkt->entry_status & BIT_2)
		dprintk(2, "qla1280_error_entry: BAD HEADER flag error\n");
	else if (pkt->entry_status & BIT_1)
		dprintk(2, "qla1280_error_entry: FULL flag error\n");
	else
		dprintk(2, "qla1280_error_entry: UNKNOWN flag error\n");

	
	if (handle < MAX_OUTSTANDING_COMMANDS)
		sp = ha->outstanding_cmds[handle];
	else
		sp = NULL;

	if (sp) {
		
		ha->outstanding_cmds[handle] = NULL;

		
		if (pkt->entry_status & (BIT_3 + BIT_2)) {
			
			
			CMD_RESULT(sp->cmd) = DID_ERROR << 16;
		} else if (pkt->entry_status & BIT_1) {	
			CMD_RESULT(sp->cmd) = DID_BUS_BUSY << 16;
		} else {
			
			CMD_RESULT(sp->cmd) = DID_ERROR << 16;
		}

		CMD_HANDLE(sp->cmd) = COMPLETED_HANDLE;

		
		list_add_tail(&sp->list, done_q);
	}
#ifdef QLA_64BIT_PTR
	else if (pkt->entry_type == COMMAND_A64_TYPE) {
		printk(KERN_WARNING "!qla1280: Error Entry invalid handle");
	}
#endif

	LEAVE("qla1280_error_entry");
}

static int
qla1280_abort_isp(struct scsi_qla_host *ha)
{
	struct device_reg __iomem *reg = ha->iobase;
	struct srb *sp;
	int status = 0;
	int cnt;
	int bus;

	ENTER("qla1280_abort_isp");

	if (ha->flags.abort_isp_active || !ha->flags.online)
		goto out;
	
	ha->flags.abort_isp_active = 1;

	
	qla1280_disable_intrs(ha);
	WRT_REG_WORD(&reg->host_cmd, HC_PAUSE_RISC);
	RD_REG_WORD(&reg->id_l);

	printk(KERN_INFO "scsi(%li): dequeuing outstanding commands\n",
	       ha->host_no);
	
	for (cnt = 0; cnt < MAX_OUTSTANDING_COMMANDS; cnt++) {
		struct scsi_cmnd *cmd;
		sp = ha->outstanding_cmds[cnt];
		if (sp) {
			cmd = sp->cmd;
			CMD_RESULT(cmd) = DID_RESET << 16;
			CMD_HANDLE(cmd) = COMPLETED_HANDLE;
			ha->outstanding_cmds[cnt] = NULL;
			list_add_tail(&sp->list, &ha->done_q);
		}
	}

	qla1280_done(ha);

	status = qla1280_load_firmware(ha);
	if (status)
		goto out;

	
	qla1280_nvram_config (ha);

	status = qla1280_init_rings(ha);
	if (status)
		goto out;
		
	
	for (bus = 0; bus < ha->ports; bus++)
		qla1280_bus_reset(ha, bus);
		
	ha->flags.abort_isp_active = 0;
 out:
	if (status) {
		printk(KERN_WARNING
		       "qla1280: ISP error recovery failed, board disabled");
		qla1280_reset_adapter(ha);
		dprintk(2, "qla1280_abort_isp: **** FAILED ****\n");
	}

	LEAVE("qla1280_abort_isp");
	return status;
}


static u16
qla1280_debounce_register(volatile u16 __iomem * addr)
{
	volatile u16 ret;
	volatile u16 ret2;

	ret = RD_REG_WORD(addr);
	ret2 = RD_REG_WORD(addr);

	if (ret == ret2)
		return ret;

	do {
		cpu_relax();
		ret = RD_REG_WORD(addr);
		ret2 = RD_REG_WORD(addr);
	} while (ret != ret2);

	return ret;
}


#define SET_SXP_BANK            0x0100
#define SCSI_PHASE_INVALID      0x87FF
static int
qla1280_check_for_dead_scsi_bus(struct scsi_qla_host *ha, unsigned int bus)
{
	uint16_t config_reg, scsi_control;
	struct device_reg __iomem *reg = ha->iobase;

	if (ha->bus_settings[bus].scsi_bus_dead) {
		WRT_REG_WORD(&reg->host_cmd, HC_PAUSE_RISC);
		config_reg = RD_REG_WORD(&reg->cfg_1);
		WRT_REG_WORD(&reg->cfg_1, SET_SXP_BANK);
		scsi_control = RD_REG_WORD(&reg->scsiControlPins);
		WRT_REG_WORD(&reg->cfg_1, config_reg);
		WRT_REG_WORD(&reg->host_cmd, HC_RELEASE_RISC);

		if (scsi_control == SCSI_PHASE_INVALID) {
			ha->bus_settings[bus].scsi_bus_dead = 1;
			return 1;	
		} else {
			ha->bus_settings[bus].scsi_bus_dead = 0;
			ha->bus_settings[bus].failed_reset_count = 0;
		}
	}
	return 0;		
}

static void
qla1280_get_target_parameters(struct scsi_qla_host *ha,
			      struct scsi_device *device)
{
	uint16_t mb[MAILBOX_REGISTER_COUNT];
	int bus, target, lun;

	bus = device->channel;
	target = device->id;
	lun = device->lun;


	mb[0] = MBC_GET_TARGET_PARAMETERS;
	mb[1] = (uint16_t) (bus ? target | BIT_7 : target);
	mb[1] <<= 8;
	qla1280_mailbox_command(ha, BIT_6 | BIT_3 | BIT_2 | BIT_1 | BIT_0,
				&mb[0]);

	printk(KERN_INFO "scsi(%li:%d:%d:%d):", ha->host_no, bus, target, lun);

	if (mb[3] != 0) {
		printk(" Sync: period %d, offset %d",
		       (mb[3] & 0xff), (mb[3] >> 8));
		if (mb[2] & BIT_13)
			printk(", Wide");
		if ((mb[2] & BIT_5) && ((mb[6] >> 8) & 0xff) >= 2)
			printk(", DT");
	} else
		printk(" Async");

	if (device->simple_tags)
		printk(", Tagged queuing: depth %d", device->queue_depth);
	printk("\n");
}


#if DEBUG_QLA1280
static void
__qla1280_dump_buffer(char *b, int size)
{
	int cnt;
	u8 c;

	printk(KERN_DEBUG " 0   1   2   3   4   5   6   7   8   9   Ah  "
	       "Bh  Ch  Dh  Eh  Fh\n");
	printk(KERN_DEBUG "---------------------------------------------"
	       "------------------\n");

	for (cnt = 0; cnt < size;) {
		c = *b++;

		printk("0x%02x", c);
		cnt++;
		if (!(cnt % 16))
			printk("\n");
		else
			printk(" ");
	}
	if (cnt % 16)
		printk("\n");
}

static void
__qla1280_print_scsi_cmd(struct scsi_cmnd *cmd)
{
	struct scsi_qla_host *ha;
	struct Scsi_Host *host = CMD_HOST(cmd);
	struct srb *sp;
	

	int i;
	ha = (struct scsi_qla_host *)host->hostdata;

	sp = (struct srb *)CMD_SP(cmd);
	printk("SCSI Command @= 0x%p, Handle=0x%p\n", cmd, CMD_HANDLE(cmd));
	printk("  chan=%d, target = 0x%02x, lun = 0x%02x, cmd_len = 0x%02x\n",
	       SCSI_BUS_32(cmd), SCSI_TCN_32(cmd), SCSI_LUN_32(cmd),
	       CMD_CDBLEN(cmd));
	printk(" CDB = ");
	for (i = 0; i < cmd->cmd_len; i++) {
		printk("0x%02x ", cmd->cmnd[i]);
	}
	printk("  seg_cnt =%d\n", scsi_sg_count(cmd));
	printk("  request buffer=0x%p, request buffer len=0x%x\n",
	       scsi_sglist(cmd), scsi_bufflen(cmd));
	printk("  tag=%d, transfersize=0x%x \n",
	       cmd->tag, cmd->transfersize);
	printk("  SP=0x%p\n", CMD_SP(cmd));
	printk(" underflow size = 0x%x, direction=0x%x\n",
	       cmd->underflow, cmd->sc_data_direction);
}

static void
ql1280_dump_device(struct scsi_qla_host *ha)
{

	struct scsi_cmnd *cp;
	struct srb *sp;
	int i;

	printk(KERN_DEBUG "Outstanding Commands on controller:\n");

	for (i = 0; i < MAX_OUTSTANDING_COMMANDS; i++) {
		if ((sp = ha->outstanding_cmds[i]) == NULL)
			continue;
		if ((cp = sp->cmd) == NULL)
			continue;
		qla1280_print_scsi_cmd(1, cp);
	}
}
#endif


enum tokens {
	TOKEN_NVRAM,
	TOKEN_SYNC,
	TOKEN_WIDE,
	TOKEN_PPR,
	TOKEN_VERBOSE,
	TOKEN_DEBUG,
};

struct setup_tokens {
	char *token;
	int val;
};

static struct setup_tokens setup_token[] __initdata = 
{
	{ "nvram", TOKEN_NVRAM },
	{ "sync", TOKEN_SYNC },
	{ "wide", TOKEN_WIDE },
	{ "ppr", TOKEN_PPR },
	{ "verbose", TOKEN_VERBOSE },
	{ "debug", TOKEN_DEBUG },
};


static int __init
qla1280_setup(char *s)
{
	char *cp, *ptr;
	unsigned long val;
	int toke;

	cp = s;

	while (cp && (ptr = strchr(cp, ':'))) {
		ptr++;
		if (!strcmp(ptr, "yes")) {
			val = 0x10000;
			ptr += 3;
		} else if (!strcmp(ptr, "no")) {
 			val = 0;
			ptr += 2;
		} else
			val = simple_strtoul(ptr, &ptr, 0);

		switch ((toke = qla1280_get_token(cp))) {
		case TOKEN_NVRAM:
			if (!val)
				driver_setup.no_nvram = 1;
			break;
		case TOKEN_SYNC:
			if (!val)
				driver_setup.no_sync = 1;
			else if (val != 0x10000)
				driver_setup.sync_mask = val;
			break;
		case TOKEN_WIDE:
			if (!val)
				driver_setup.no_wide = 1;
			else if (val != 0x10000)
				driver_setup.wide_mask = val;
			break;
		case TOKEN_PPR:
			if (!val)
				driver_setup.no_ppr = 1;
			else if (val != 0x10000)
				driver_setup.ppr_mask = val;
			break;
		case TOKEN_VERBOSE:
			qla1280_verbose = val;
			break;
		default:
			printk(KERN_INFO "qla1280: unknown boot option %s\n",
			       cp);
		}

		cp = strchr(ptr, ';');
		if (cp)
			cp++;
		else {
			break;
		}
	}
	return 1;
}


static int __init
qla1280_get_token(char *str)
{
	char *sep;
	long ret = -1;
	int i;

	sep = strchr(str, ':');

	if (sep) {
		for (i = 0; i < ARRAY_SIZE(setup_token); i++) {
			if (!strncmp(setup_token[i].token, str, (sep - str))) {
				ret =  setup_token[i].val;
				break;
			}
		}
	}

	return ret;
}


static struct scsi_host_template qla1280_driver_template = {
	.module			= THIS_MODULE,
	.proc_name		= "qla1280",
	.name			= "Qlogic ISP 1280/12160",
	.info			= qla1280_info,
	.slave_configure	= qla1280_slave_configure,
	.queuecommand		= qla1280_queuecommand,
	.eh_abort_handler	= qla1280_eh_abort,
	.eh_device_reset_handler= qla1280_eh_device_reset,
	.eh_bus_reset_handler	= qla1280_eh_bus_reset,
	.eh_host_reset_handler	= qla1280_eh_adapter_reset,
	.bios_param		= qla1280_biosparam,
	.can_queue		= 0xfffff,
	.this_id		= -1,
	.sg_tablesize		= SG_ALL,
	.cmd_per_lun		= 1,
	.use_clustering		= ENABLE_CLUSTERING,
};


static int __devinit
qla1280_probe_one(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int devnum = id->driver_data;
	struct qla_boards *bdp = &ql1280_board_tbl[devnum];
	struct Scsi_Host *host;
	struct scsi_qla_host *ha;
	int error = -ENODEV;

	
	if (pdev->subsystem_vendor == PCI_VENDOR_ID_AMI) {
		printk(KERN_INFO
		       "qla1280: Skipping AMI SubSys Vendor ID Chip\n");
		goto error;
	}

	printk(KERN_INFO "qla1280: %s found on PCI bus %i, dev %i\n",
	       bdp->name, pdev->bus->number, PCI_SLOT(pdev->devfn));
	
	if (pci_enable_device(pdev)) {
		printk(KERN_WARNING
		       "qla1280: Failed to enabled pci device, aborting.\n");
		goto error;
	}

	pci_set_master(pdev);

	error = -ENOMEM;
	host = scsi_host_alloc(&qla1280_driver_template, sizeof(*ha));
	if (!host) {
		printk(KERN_WARNING
		       "qla1280: Failed to register host, aborting.\n");
		goto error_disable_device;
	}

	ha = (struct scsi_qla_host *)host->hostdata;
	memset(ha, 0, sizeof(struct scsi_qla_host));

	ha->pdev = pdev;
	ha->devnum = devnum;	

#ifdef QLA_64BIT_PTR
	if (pci_set_dma_mask(ha->pdev, DMA_BIT_MASK(64))) {
		if (pci_set_dma_mask(ha->pdev, DMA_BIT_MASK(32))) {
			printk(KERN_WARNING "scsi(%li): Unable to set a "
			       "suitable DMA mask - aborting\n", ha->host_no);
			error = -ENODEV;
			goto error_put_host;
		}
	} else
		dprintk(2, "scsi(%li): 64 Bit PCI Addressing Enabled\n",
			ha->host_no);
#else
	if (pci_set_dma_mask(ha->pdev, DMA_BIT_MASK(32))) {
		printk(KERN_WARNING "scsi(%li): Unable to set a "
		       "suitable DMA mask - aborting\n", ha->host_no);
		error = -ENODEV;
		goto error_put_host;
	}
#endif

	ha->request_ring = pci_alloc_consistent(ha->pdev,
			((REQUEST_ENTRY_CNT + 1) * sizeof(request_t)),
			&ha->request_dma);
	if (!ha->request_ring) {
		printk(KERN_INFO "qla1280: Failed to get request memory\n");
		goto error_put_host;
	}

	ha->response_ring = pci_alloc_consistent(ha->pdev,
			((RESPONSE_ENTRY_CNT + 1) * sizeof(struct response)),
			&ha->response_dma);
	if (!ha->response_ring) {
		printk(KERN_INFO "qla1280: Failed to get response memory\n");
		goto error_free_request_ring;
	}

	ha->ports = bdp->numPorts;

	ha->host = host;
	ha->host_no = host->host_no;

	host->irq = pdev->irq;
	host->max_channel = bdp->numPorts - 1;
	host->max_lun = MAX_LUNS - 1;
	host->max_id = MAX_TARGETS;
	host->max_sectors = 1024;
	host->unique_id = host->host_no;

	error = -ENODEV;

#if MEMORY_MAPPED_IO
	ha->mmpbase = pci_ioremap_bar(ha->pdev, 1);
	if (!ha->mmpbase) {
		printk(KERN_INFO "qla1280: Unable to map I/O memory\n");
		goto error_free_response_ring;
	}

	host->base = (unsigned long)ha->mmpbase;
	ha->iobase = (struct device_reg __iomem *)ha->mmpbase;
#else
	host->io_port = pci_resource_start(ha->pdev, 0);
	if (!request_region(host->io_port, 0xff, "qla1280")) {
		printk(KERN_INFO "qla1280: Failed to reserve i/o region "
				 "0x%04lx-0x%04lx - already in use\n",
		       host->io_port, host->io_port + 0xff);
		goto error_free_response_ring;
	}

	ha->iobase = (struct device_reg *)host->io_port;
#endif

	INIT_LIST_HEAD(&ha->done_q);

	
	qla1280_disable_intrs(ha);

	if (request_irq(pdev->irq, qla1280_intr_handler, IRQF_SHARED,
				"qla1280", ha)) {
		printk("qla1280 : Failed to reserve interrupt %d already "
		       "in use\n", pdev->irq);
		goto error_release_region;
	}

	
	if (qla1280_initialize_adapter(ha)) {
		printk(KERN_INFO "qla1x160: Failed to initialize adapter\n");
		goto error_free_irq;
	}

	
	host->this_id = ha->bus_settings[0].id;

	pci_set_drvdata(pdev, host);

	error = scsi_add_host(host, &pdev->dev);
	if (error)
		goto error_disable_adapter;
	scsi_scan_host(host);

	return 0;

 error_disable_adapter:
	qla1280_disable_intrs(ha);
 error_free_irq:
	free_irq(pdev->irq, ha);
 error_release_region:
#if MEMORY_MAPPED_IO
	iounmap(ha->mmpbase);
#else
	release_region(host->io_port, 0xff);
#endif
 error_free_response_ring:
	pci_free_consistent(ha->pdev,
			((RESPONSE_ENTRY_CNT + 1) * sizeof(struct response)),
			ha->response_ring, ha->response_dma);
 error_free_request_ring:
	pci_free_consistent(ha->pdev,
			((REQUEST_ENTRY_CNT + 1) * sizeof(request_t)),
			ha->request_ring, ha->request_dma);
 error_put_host:
	scsi_host_put(host);
 error_disable_device:
	pci_disable_device(pdev);
 error:
	return error;
}


static void __devexit
qla1280_remove_one(struct pci_dev *pdev)
{
	struct Scsi_Host *host = pci_get_drvdata(pdev);
	struct scsi_qla_host *ha = (struct scsi_qla_host *)host->hostdata;

	scsi_remove_host(host);

	qla1280_disable_intrs(ha);

	free_irq(pdev->irq, ha);

#if MEMORY_MAPPED_IO
	iounmap(ha->mmpbase);
#else
	release_region(host->io_port, 0xff);
#endif

	pci_free_consistent(ha->pdev,
			((REQUEST_ENTRY_CNT + 1) * (sizeof(request_t))),
			ha->request_ring, ha->request_dma);
	pci_free_consistent(ha->pdev,
			((RESPONSE_ENTRY_CNT + 1) * (sizeof(struct response))),
			ha->response_ring, ha->response_dma);

	pci_disable_device(pdev);

	scsi_host_put(host);
}

static struct pci_driver qla1280_pci_driver = {
	.name		= "qla1280",
	.id_table	= qla1280_pci_tbl,
	.probe		= qla1280_probe_one,
	.remove		= __devexit_p(qla1280_remove_one),
};

static int __init
qla1280_init(void)
{
	if (sizeof(struct srb) > sizeof(struct scsi_pointer)) {
		printk(KERN_WARNING
		       "qla1280: struct srb too big, aborting\n");
		return -EINVAL;
	}

#ifdef MODULE
	if (qla1280)
		qla1280_setup(qla1280);
#endif

	return pci_register_driver(&qla1280_pci_driver);
}

static void __exit
qla1280_exit(void)
{
	int i;

	pci_unregister_driver(&qla1280_pci_driver);
	
	for (i = 0; i < QL_NUM_FW_IMAGES; i++) {
		if (qla1280_fw_tbl[i].fw) {
			release_firmware(qla1280_fw_tbl[i].fw);
			qla1280_fw_tbl[i].fw = NULL;
		}
	}
}

module_init(qla1280_init);
module_exit(qla1280_exit);


MODULE_AUTHOR("Qlogic & Jes Sorensen");
MODULE_DESCRIPTION("Qlogic ISP SCSI (qla1x80/qla1x160) driver");
MODULE_LICENSE("GPL");
MODULE_FIRMWARE("qlogic/1040.bin");
MODULE_FIRMWARE("qlogic/1280.bin");
MODULE_FIRMWARE("qlogic/12160.bin");
MODULE_VERSION(QLA1280_VERSION);

