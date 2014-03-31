/*
 Low Level Linux Driver for the IBM Microchannel SCSI Subsystem for
 Linux Kernel >= 2.4.0.
 Copyright (c) 1995 Strom Systems, Inc. under the terms of the GNU
 General Public License. Written by Martin Kolinek, December 1995.
 Further development by: Chris Beauregard, Klaus Kudielka, Michael Lang
 See the file Documentation/scsi/ibmmca.txt for a detailed description
 of this driver, the commandline arguments and the history of its
 development.
 See the WWW-page: http://www.uni-mainz.de/~langm000/linux.html for latest
 updates, info and ADF-files for adapters supported by this driver.

 Alan Cox <alan@lxorguk.ukuu.org.uk>
 Updated for Linux 2.5.45 to use the new error handler, cleaned up the
 lock macros and did a few unavoidable locking tweaks, plus one locking
 fix in the irq and completion path.
 
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/blkdev.h>
#include <linux/proc_fs.h>
#include <linux/stat.h>
#include <linux/mca.h>
#include <linux/spinlock.h>
#include <linux/init.h>

#include <asm/io.h>

#include "scsi.h"
#include <scsi/scsi_host.h>

static int ibmmca_queuecommand (struct Scsi_Host *, struct scsi_cmnd *);
static int ibmmca_abort (Scsi_Cmnd *);
static int ibmmca_host_reset (Scsi_Cmnd *);
static int ibmmca_biosparam (struct scsi_device *, struct block_device *, sector_t, int *);
static int ibmmca_proc_info(struct Scsi_Host *shpnt, char *buffer, char **start, off_t offset, int length, int inout);



#define IBMMCA_SCSI_DRIVER_VERSION "4.0b-ac"

#define IM_MAX_HOSTS     8	
#define IM_RESET_DELAY	60	

#undef	IM_DEBUG_TIMEOUT	
#define TIMEOUT_PUN	0
#define TIMEOUT_LUN	0
#undef IM_DEBUG_INT
#undef IM_DEBUG_CMD
#undef IM_DEBUG_CMD_SPEC_DEV
#undef IM_DEBUG_PROBE

#define IM_DEBUG_CMD_DEVICE	TYPE_TAPE

#define IM_CMD_REG(h)	((h)->io_port)	
#define IM_ATTN_REG(h)	((h)->io_port+4)	
#define IM_CTR_REG(h)	((h)->io_port+5)	
#define IM_INTR_REG(h)	((h)->io_port+6)	
#define IM_STAT_REG(h)	((h)->io_port+7)	

#define IM_IO_PORT	0x3540
#define IM_N_IO_PORT	8

#define IM_IMM_CMD	0x10	
#define IM_SCB		0x30	
#define IM_LONG_SCB	0x40	
#define IM_EOI		0xe0	

#define IM_HW_RESET	0x80	
#define IM_ENABLE_DMA	0x02	
#define IM_ENABLE_INTR	0x01	

#define IM_SCB_CMD_COMPLETED			0x10
#define IM_SCB_CMD_COMPLETED_WITH_RETRIES	0x50
#define IM_LOOP_SCATTER_BUFFER_FULL		0x60
#define IM_ADAPTER_HW_FAILURE			0x70
#define IM_IMMEDIATE_CMD_COMPLETED		0xa0
#define IM_CMD_COMPLETED_WITH_FAILURE		0xc0
#define IM_CMD_ERROR				0xe0
#define IM_SOFTWARE_SEQUENCING_ERROR		0xf0

#define IM_CMD_REG_FULL		0x08
#define IM_CMD_REG_EMPTY	0x04
#define IM_INTR_REQUEST		0x02
#define IM_BUSY			0x01

/*immediate commands (word written into low 2 bytes of command reg) */
#define IM_RESET_IMM_CMD	0x0400
#define IM_FEATURE_CTR_IMM_CMD	0x040c
#define IM_DMA_PACING_IMM_CMD	0x040d
#define IM_ASSIGN_IMM_CMD	0x040e
#define IM_ABORT_IMM_CMD	0x040f
#define IM_FORMAT_PREP_IMM_CMD	0x0417

struct im_scb {
	unsigned short command;	
	unsigned short enable;	
	union {
		unsigned long log_blk_adr;	
		unsigned char scsi_cmd_length;	
	} u1;
	unsigned long sys_buf_adr;	
	unsigned long sys_buf_length;	
	unsigned long tsb_adr;	
	unsigned long scb_chain_adr;	
	union {
		struct {
			unsigned short count;	
			unsigned short length;	
		} blk;
		unsigned char scsi_command[12];	
	} u2;
};

struct im_sge {
	void *address;
	unsigned long byte_length;
};

struct im_pos_info {
	unsigned short pos_id;	
	unsigned char pos_3a;	
	unsigned char pos_2;	
	unsigned char int_level;	
	unsigned char pos_4a;	
	unsigned short connector_size;	
	unsigned char num_luns;	
	unsigned char num_puns;	
	unsigned char pacing_factor;	
	unsigned char num_ldns;	
	unsigned char eoi_off;	
	unsigned char max_busy;	
	unsigned short cache_stat;	
	unsigned short retry_stat;	
	unsigned char pos_4b;	
	unsigned char pos_3b;	
	unsigned char pos_6;	
	unsigned char pos_5;	
	unsigned short max_overlap;	
	unsigned short num_bus;	
};

#define IM_NO_SYNCHRONOUS      0x0040	
#define IM_NO_DISCONNECT       0x0080	
#define IM_READ_DATA_CMD       0x1c01
#define IM_WRITE_DATA_CMD      0x1c02
#define IM_READ_VERIFY_CMD     0x1c03
#define IM_WRITE_VERIFY_CMD    0x1c04
#define IM_REQUEST_SENSE_CMD   0x1c08
#define IM_READ_CAPACITY_CMD   0x1c09
#define IM_DEVICE_INQUIRY_CMD  0x1c0b
#define IM_READ_LOGICAL_CMD    0x1c2a
#define IM_OTHER_SCSI_CMD_CMD  0x241f

#define IM_GET_COMMAND_COMPLETE_STATUS_CMD   0x1c07	
#define IM_GET_POS_INFO_CMD                  0x1c0a	
#define IM_READ_PREFETCH_CMD                 0x1c31	
#define IM_FOMAT_UNIT_CMD                    0x1c16	
#define IM_REASSIGN_BLOCK_CMD                0x1c18	

#define IM_READ_CONTROL              0x8000
#define IM_REPORT_TSB_ONLY_ON_ERROR  0x4000
#define IM_RETRY_ENABLE              0x2000
#define IM_POINTER_TO_LIST           0x1000
#define IM_SUPRESS_EXCEPTION_SHORT   0x0400
#define IM_BYPASS_BUFFER             0x0200
#define IM_CHAIN_ON_NO_ERROR         0x0001

struct im_tsb {
	unsigned short end_status;
	unsigned short reserved1;
	unsigned long residual_byte_count;
	unsigned long sg_list_element_adr;
	unsigned short status_length;
	unsigned char dev_status;
	unsigned char cmd_status;
	unsigned char dev_error;
	unsigned char cmd_error;
	unsigned short reserved2;
	unsigned short reserved3;
	unsigned short low_of_last_scb_adr;
	unsigned short high_of_last_scb_adr;
};

#define IM_IRQ     14
#define IM_IRQ_FW  11


#define MOD95_LED_PORT	   0x108
#define PS2_SYS_CTR        0x92
#define LED_DISP           1
#define LED_ADISP          2
#define LED_ACTIVITY       4
#define CMD_FAIL           255


static int display_mode = 0;
static unsigned int adapter_timeout = 45;
static unsigned int global_command_error_excuse = 0;
static int global_adapter_speed = 0;	

#define PS2_DISK_LED_ON(ad,id) { if (display_mode & LED_DISP) { if (id>9) \
    outw((ad+48)|((id+55)<<8), MOD95_LED_PORT ); else \
    outw((ad+48)|((id+48)<<8), MOD95_LED_PORT ); } else \
    if (display_mode & LED_ADISP) { if (id<7) outb((char)(id+48),MOD95_LED_PORT+1+id); \
    outb((char)(ad+48), MOD95_LED_PORT); } \
    if ((display_mode & LED_ACTIVITY)||(!display_mode)) \
    outb(inb(PS2_SYS_CTR) | 0xc0, PS2_SYS_CTR); }

#define PS2_DISK_LED_OFF() { if (display_mode & LED_DISP) \
    outw(0x2020, MOD95_LED_PORT ); else if (display_mode & LED_ADISP) { \
    outl(0x20202020,MOD95_LED_PORT); outl(0x20202020,MOD95_LED_PORT+4); } \
    if ((display_mode & LED_ACTIVITY)||(!display_mode)) \
    outb(inb(PS2_SYS_CTR) & 0x3f, PS2_SYS_CTR); }

#define IBM_SCSI2_FW     0
#define IBM_7568_WCACHE  1
#define IBM_EXP_UNIT     2
#define IBM_SCSI_WCACHE  3
#define IBM_SCSI         4
#define IBM_INTEGSCSI	 5

#define FORCED_DETECTION         100
#define INTEGRATED_SCSI          101

static short ibmmca_id_table[] = {
	0x8efc,
	0x8efd,
	0x8ef8,
	0x8eff,
	0x8efe,
	
	0
};

static const char *ibmmca_description[] = {
	"IBM SCSI-2 F/W Adapter",	
	"IBM 7568 Industrial Computer SCSI Adapter w/Cache",	
	"IBM Expansion Unit SCSI Controller",	
	"IBM SCSI Adapter w/Cache",	
	"IBM SCSI Adapter",	
	"IBM Integrated SCSI Controller", 
};

#define MAX_LOG_DEV  15

struct logical_device {
	struct im_scb scb;	
	struct im_tsb tsb;	
	struct im_sge sge[16];	
	unsigned char buf[256];	
	Scsi_Cmnd *cmd;		
	int device_type;	
	int block_length;	
	int cache_flag;		
	int retry_flag;		
};

struct Driver_Statistics {
	
	int ldn_access[MAX_LOG_DEV + 1];	
	int ldn_read_access[MAX_LOG_DEV + 1];	
	int ldn_write_access[MAX_LOG_DEV + 1];	
	int ldn_inquiry_access[MAX_LOG_DEV + 1];	
	int ldn_modeselect_access[MAX_LOG_DEV + 1];	
	int scbs;		
	int long_scbs;		
	int total_accesses;	
	int total_interrupts;	
	int total_errors;	
	
	int total_scsi_devices;	
	int dyn_flag;		
	int dynamical_assignments;	
	int ldn_assignments[MAX_LOG_DEV + 1];	
};

struct ibmmca_hostdata {
	
	struct logical_device _ld[MAX_LOG_DEV + 1];
	
	unsigned char _get_ldn[16][8];
	unsigned char _get_scsi[16][8];
	
	int _local_checking_phase_flag;
	
	int _got_interrupt;
	
	int _stat_result;
	
	int _reset_status;
	
	int _last_scsi_command[MAX_LOG_DEV + 1];
	
	int _last_scsi_type[MAX_LOG_DEV + 1];
	
	int _last_scsi_blockcount[MAX_LOG_DEV + 1];
	
	unsigned long _last_scsi_logical_block[MAX_LOG_DEV + 1];
	int _next_ldn;
	
	struct Driver_Statistics _IBM_DS;
	
	unsigned int _pos[8];
	int _special;
	
	int _connector_size;
	
	int _adapter_speed;
};

#define subsystem_pun(h) ((h)->this_id)
#define subsystem_maxid(h) ((h)->max_id)
#define ld(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_ld)
#define get_ldn(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_get_ldn)
#define get_scsi(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_get_scsi)
#define local_checking_phase_flag(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_local_checking_phase_flag)
#define got_interrupt(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_got_interrupt)
#define stat_result(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_stat_result)
#define reset_status(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_reset_status)
#define last_scsi_command(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_last_scsi_command)
#define last_scsi_type(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_last_scsi_type)
#define last_scsi_blockcount(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_last_scsi_blockcount)
#define last_scsi_logical_block(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_last_scsi_logical_block)
#define last_scsi_type(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_last_scsi_type)
#define next_ldn(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_next_ldn)
#define IBM_DS(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_IBM_DS)
#define special(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_special)
#define subsystem_connector_size(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_connector_size)
#define adapter_speed(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_adapter_speed)
#define pos2(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_pos[2])
#define pos3(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_pos[3])
#define pos4(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_pos[4])
#define pos5(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_pos[5])
#define pos6(h) (((struct ibmmca_hostdata *) (h)->hostdata)->_pos[6])

#define TYPE_IBM_SCSI_ADAPTER   0x2F

#define TYPE_NO_DEVICE          0xFF

#ifndef TYPE_MEDIUM_CHANGER
#define TYPE_MEDIUM_CHANGER     0x08
#endif

#define SET_LDN        0
#define REMOVE_LDN     1

#define PROBE_LDN      0

#define IM_RESET_NOT_IN_PROGRESS         0
#define IM_RESET_IN_PROGRESS             1
#define IM_RESET_FINISHED_OK             2
#define IM_RESET_FINISHED_FAIL           3
#define IM_RESET_NOT_IN_PROGRESS_NO_INT  4
#define IM_RESET_FINISHED_OK_NO_INT      5

#define NO_SCSI                  0xffff


static int io_port[IM_MAX_HOSTS] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static int scsi_id[IM_MAX_HOSTS] = { 7, 7, 7, 7, 7, 7, 7, 7 };

#if defined(MODULE)
static char *boot_options = NULL;
module_param(boot_options, charp, 0);
module_param_array(io_port, int, NULL, 0);
module_param_array(scsi_id, int, NULL, 0);

MODULE_LICENSE("GPL");
#endif
static int disk_rw_in_progress = 0;

static unsigned int pos[8];	
#ifdef CONFIG_IBMMCA_SCSI_ORDER_STANDARD
static char ibm_ansi_order = 1;
#else
static char ibm_ansi_order = 0;
#endif

static void issue_cmd(struct Scsi_Host *, unsigned long, unsigned char);
static void internal_done(Scsi_Cmnd * cmd);
static void check_devices(struct Scsi_Host *, int);
static int immediate_assign(struct Scsi_Host *, unsigned int, unsigned int, unsigned int, unsigned int);
static int immediate_feature(struct Scsi_Host *, unsigned int, unsigned int);
#ifdef CONFIG_IBMMCA_SCSI_DEV_RESET
static int immediate_reset(struct Scsi_Host *, unsigned int);
#endif
static int device_inquiry(struct Scsi_Host *, int);
static int read_capacity(struct Scsi_Host *, int);
static int get_pos_info(struct Scsi_Host *);
static char *ti_p(int);
static char *ti_l(int);
static char *ibmrate(unsigned int, int);
static int probe_display(int);
static int probe_bus_mode(struct Scsi_Host *);
static int device_exists(struct Scsi_Host *, int, int *, int *);
static int option_setup(char *);
static int ldn_access_load(struct Scsi_Host *, int);
static int ldn_access_total_read_write(struct Scsi_Host *);

static irqreturn_t interrupt_handler(int irq, void *dev_id)
{
	unsigned int intr_reg;
	unsigned int cmd_result;
	unsigned int ldn;
	unsigned long flags;
	Scsi_Cmnd *cmd;
	int lastSCSI;
	struct device *dev = dev_id;
	struct Scsi_Host *shpnt = dev_get_drvdata(dev);

	spin_lock_irqsave(shpnt->host_lock, flags);

	if(!(inb(IM_STAT_REG(shpnt)) & IM_INTR_REQUEST)) {
		spin_unlock_irqrestore(shpnt->host_lock, flags);
		return IRQ_NONE;
	}

	if ((reset_status(shpnt) == IM_RESET_NOT_IN_PROGRESS_NO_INT) || (reset_status(shpnt) == IM_RESET_FINISHED_OK_NO_INT)) {
		reset_status(shpnt) = IM_RESET_NOT_IN_PROGRESS;
		spin_unlock_irqrestore(shpnt->host_lock, flags);
		return IRQ_HANDLED;
	}

	
	while (1) {
		if (!(inb(IM_STAT_REG(shpnt)) & IM_BUSY))
			break;
		cpu_relax();
	}

	
	intr_reg = (unsigned char) (inb(IM_INTR_REG(shpnt)));
	cmd_result = intr_reg & 0xf0;
	ldn = intr_reg & 0x0f;
	
	lastSCSI = last_scsi_command(shpnt)[ldn];
	outb(IM_EOI | ldn, IM_ATTN_REG(shpnt));
	
	
	if (!global_command_error_excuse) {
		switch (cmd_result) {
			
		case IM_ADAPTER_HW_FAILURE:
		case IM_SOFTWARE_SEQUENCING_ERROR:
		case IM_CMD_ERROR:
			printk(KERN_ERR "IBM MCA SCSI: Fatal Subsystem ERROR!\n");
			printk(KERN_ERR "              Last cmd=0x%x, ena=%x, len=", lastSCSI, ld(shpnt)[ldn].scb.enable);
			if (ld(shpnt)[ldn].cmd)
				printk("%ld/%ld,", (long) (scsi_bufflen(ld(shpnt)[ldn].cmd)), (long) (ld(shpnt)[ldn].scb.sys_buf_length));
			else
				printk("none,");
			if (ld(shpnt)[ldn].cmd)
				printk("Blocksize=%d", ld(shpnt)[ldn].scb.u2.blk.length);
			else
				printk("Blocksize=none");
			printk(", host=%p, ldn=0x%x\n", shpnt, ldn);
			if (ld(shpnt)[ldn].cmd) {
				printk(KERN_ERR "Blockcount=%d/%d\n", last_scsi_blockcount(shpnt)[ldn], ld(shpnt)[ldn].scb.u2.blk.count);
				printk(KERN_ERR "Logical block=%lx/%lx\n", last_scsi_logical_block(shpnt)[ldn], ld(shpnt)[ldn].scb.u1.log_blk_adr);
			}
			printk(KERN_ERR "Reason given: %s\n", (cmd_result == IM_ADAPTER_HW_FAILURE) ? "HARDWARE FAILURE" : (cmd_result == IM_SOFTWARE_SEQUENCING_ERROR) ? "SOFTWARE SEQUENCING ERROR" : (cmd_result == IM_CMD_ERROR) ? "COMMAND ERROR" : "UNKNOWN");
			
			printk(KERN_ERR "IBM MCA SCSI: Subsystem Error-Status follows:\n");
			printk(KERN_ERR "              Command Type................: %x\n", last_scsi_type(shpnt)[ldn]);
			printk(KERN_ERR "              Attention Register..........: %x\n", inb(IM_ATTN_REG(shpnt)));
			printk(KERN_ERR "              Basic Control Register......: %x\n", inb(IM_CTR_REG(shpnt)));
			printk(KERN_ERR "              Interrupt Status Register...: %x\n", intr_reg);
			printk(KERN_ERR "              Basic Status Register.......: %x\n", inb(IM_STAT_REG(shpnt)));
			if ((last_scsi_type(shpnt)[ldn] == IM_SCB) || (last_scsi_type(shpnt)[ldn] == IM_LONG_SCB)) {
				printk(KERN_ERR "              SCB-Command.................: %x\n", ld(shpnt)[ldn].scb.command);
				printk(KERN_ERR "              SCB-Enable..................: %x\n", ld(shpnt)[ldn].scb.enable);
				printk(KERN_ERR "              SCB-logical block address...: %lx\n", ld(shpnt)[ldn].scb.u1.log_blk_adr);
				printk(KERN_ERR "              SCB-system buffer address...: %lx\n", ld(shpnt)[ldn].scb.sys_buf_adr);
				printk(KERN_ERR "              SCB-system buffer length....: %lx\n", ld(shpnt)[ldn].scb.sys_buf_length);
				printk(KERN_ERR "              SCB-tsb address.............: %lx\n", ld(shpnt)[ldn].scb.tsb_adr);
				printk(KERN_ERR "              SCB-Chain address...........: %lx\n", ld(shpnt)[ldn].scb.scb_chain_adr);
				printk(KERN_ERR "              SCB-block count.............: %x\n", ld(shpnt)[ldn].scb.u2.blk.count);
				printk(KERN_ERR "              SCB-block length............: %x\n", ld(shpnt)[ldn].scb.u2.blk.length);
			}
			printk(KERN_ERR "              Send this report to the maintainer.\n");
			panic("IBM MCA SCSI: Fatal error message from the subsystem (0x%X,0x%X)!\n", lastSCSI, cmd_result);
			break;
		}
	} else {
		switch (cmd_result) {
		case IM_ADAPTER_HW_FAILURE:
		case IM_SOFTWARE_SEQUENCING_ERROR:
		case IM_CMD_ERROR:
			global_command_error_excuse = CMD_FAIL;
			break;
		default:
			global_command_error_excuse = 0;
			break;
		}
	}
	
	IBM_DS(shpnt).total_interrupts++;
	
	if (local_checking_phase_flag(shpnt)) {
		stat_result(shpnt) = cmd_result;
		got_interrupt(shpnt) = 1;
		reset_status(shpnt) = IM_RESET_FINISHED_OK;
		last_scsi_command(shpnt)[ldn] = NO_SCSI;
		spin_unlock_irqrestore(shpnt->host_lock, flags);
		return IRQ_HANDLED;
	}
	
	if (last_scsi_type(shpnt)[ldn] == IM_IMM_CMD) {
		
		if ((reset_status(shpnt) == IM_RESET_IN_PROGRESS) && (last_scsi_command(shpnt)[ldn] == IM_RESET_IMM_CMD)) {
			if (cmd_result == IM_CMD_COMPLETED_WITH_FAILURE) {
				disk_rw_in_progress = 0;
				PS2_DISK_LED_OFF();
				reset_status(shpnt) = IM_RESET_FINISHED_FAIL;
			} else {
				
				disk_rw_in_progress = 0;
				PS2_DISK_LED_OFF();
				reset_status(shpnt) = IM_RESET_FINISHED_OK;
			}
			stat_result(shpnt) = cmd_result;
			last_scsi_command(shpnt)[ldn] = NO_SCSI;
			last_scsi_type(shpnt)[ldn] = 0;
			spin_unlock_irqrestore(shpnt->host_lock, flags);
			return IRQ_HANDLED;
		} else if (last_scsi_command(shpnt)[ldn] == IM_ABORT_IMM_CMD) {
			
#ifdef IM_DEBUG_PROBE
			printk("IBM MCA SCSI: Interrupt from SCSI-abort.\n");
#endif
			disk_rw_in_progress = 0;
			PS2_DISK_LED_OFF();
			cmd = ld(shpnt)[ldn].cmd;
			ld(shpnt)[ldn].cmd = NULL;
			if (cmd_result == IM_CMD_COMPLETED_WITH_FAILURE)
				cmd->result = DID_NO_CONNECT << 16;
			else
				cmd->result = DID_ABORT << 16;
			stat_result(shpnt) = cmd_result;
			last_scsi_command(shpnt)[ldn] = NO_SCSI;
			last_scsi_type(shpnt)[ldn] = 0;
			if (cmd->scsi_done)
				(cmd->scsi_done) (cmd);	
			spin_unlock_irqrestore(shpnt->host_lock, flags);
			return IRQ_HANDLED;
		} else {
			disk_rw_in_progress = 0;
			PS2_DISK_LED_OFF();
			reset_status(shpnt) = IM_RESET_FINISHED_OK;
			stat_result(shpnt) = cmd_result;
			last_scsi_command(shpnt)[ldn] = NO_SCSI;
			spin_unlock_irqrestore(shpnt->host_lock, flags);
			return IRQ_HANDLED;
		}
	}
	last_scsi_command(shpnt)[ldn] = NO_SCSI;
	last_scsi_type(shpnt)[ldn] = 0;
	cmd = ld(shpnt)[ldn].cmd;
	ld(shpnt)[ldn].cmd = NULL;
#ifdef IM_DEBUG_TIMEOUT
	if (cmd) {
		if ((cmd->target == TIMEOUT_PUN) && (cmd->device->lun == TIMEOUT_LUN)) {
			spin_unlock_irqsave(shpnt->host_lock, flags);
			printk("IBM MCA SCSI: Ignoring interrupt from pun=%x, lun=%x.\n", cmd->target, cmd->device->lun);
			return IRQ_HANDLED;
		}
	}
#endif
	
	if (!cmd)
	{
		spin_unlock_irqrestore(shpnt->host_lock, flags);
		return IRQ_HANDLED;
	}

#ifdef IM_DEBUG_INT
	printk("cmd=%02x ireg=%02x ds=%02x cs=%02x de=%02x ce=%02x\n", cmd->cmnd[0], intr_reg, ld(shpnt)[ldn].tsb.dev_status, ld(shpnt)[ldn].tsb.cmd_status, ld(shpnt)[ldn].tsb.dev_error, ld(shpnt)[ldn].tsb.cmd_error);
#endif
	
	if ((ld(shpnt)[ldn].device_type != TYPE_NO_LUN) && (ld(shpnt)[ldn].device_type != TYPE_NO_DEVICE)) {
		
		if (--disk_rw_in_progress == 0)
			PS2_DISK_LED_OFF();
	}

	if (cmd_result == IM_CMD_COMPLETED_WITH_FAILURE) {
		cmd->result = (unsigned char) (ld(shpnt)[ldn].tsb.dev_status & 0x1e);
		IBM_DS(shpnt).total_errors++;
	} else
		cmd->result = 0;
	
	if (lastSCSI == NO_SCSI) {	
		cmd->result |= DID_BAD_INTR << 16;
		printk("IBM MCA SCSI: WARNING - Interrupt from non-pending SCSI-command!\n");
	} else			
		cmd->result |= DID_OK << 16;
	if (cmd->scsi_done)
		(cmd->scsi_done) (cmd);
	spin_unlock_irqrestore(shpnt->host_lock, flags);
	return IRQ_HANDLED;
}

static void issue_cmd(struct Scsi_Host *shpnt, unsigned long cmd_reg,
		      unsigned char attn_reg)
{
	unsigned long flags;
	
	while (1) {
		spin_lock_irqsave(shpnt->host_lock, flags);
		if (!(inb(IM_STAT_REG(shpnt)) & IM_BUSY))
			break;
		spin_unlock_irqrestore(shpnt->host_lock, flags);
	}
	
	outl(cmd_reg, IM_CMD_REG(shpnt));
	outb(attn_reg, IM_ATTN_REG(shpnt));
	spin_unlock_irqrestore(shpnt->host_lock, flags);
}

static void internal_done(Scsi_Cmnd * cmd)
{
	cmd->SCp.Status++;
	return;
}

static int device_inquiry(struct Scsi_Host *shpnt, int ldn)
{
	int retr;
	struct im_scb *scb;
	struct im_tsb *tsb;
	unsigned char *buf;

	scb = &(ld(shpnt)[ldn].scb);
	tsb = &(ld(shpnt)[ldn].tsb);
	buf = (unsigned char *) (&(ld(shpnt)[ldn].buf));
	ld(shpnt)[ldn].tsb.dev_status = 0;	
	for (retr = 0; retr < 3; retr++) {
		
		scb->command = IM_DEVICE_INQUIRY_CMD | IM_NO_DISCONNECT;
		scb->enable = IM_REPORT_TSB_ONLY_ON_ERROR | IM_READ_CONTROL | IM_SUPRESS_EXCEPTION_SHORT | IM_RETRY_ENABLE | IM_BYPASS_BUFFER;
		last_scsi_command(shpnt)[ldn] = IM_DEVICE_INQUIRY_CMD;
		last_scsi_type(shpnt)[ldn] = IM_SCB;
		scb->sys_buf_adr = isa_virt_to_bus(buf);
		scb->sys_buf_length = 255;	
		scb->tsb_adr = isa_virt_to_bus(tsb);
		
		got_interrupt(shpnt) = 0;
		issue_cmd(shpnt, isa_virt_to_bus(scb), IM_SCB | ldn);
		while (!got_interrupt(shpnt))
			barrier();

		
		if ((stat_result(shpnt) == IM_SCB_CMD_COMPLETED) || (stat_result(shpnt) == IM_SCB_CMD_COMPLETED_WITH_RETRIES))
			return 1;
	}
	
	if (retr >= 3)
		return 0;
	else
		return 1;
}

static int read_capacity(struct Scsi_Host *shpnt, int ldn)
{
	int retr;
	struct im_scb *scb;
	struct im_tsb *tsb;
	unsigned char *buf;

	scb = &(ld(shpnt)[ldn].scb);
	tsb = &(ld(shpnt)[ldn].tsb);
	buf = (unsigned char *) (&(ld(shpnt)[ldn].buf));
	ld(shpnt)[ldn].tsb.dev_status = 0;
	for (retr = 0; retr < 3; retr++) {
		
		scb->command = IM_READ_CAPACITY_CMD;
		scb->enable = IM_REPORT_TSB_ONLY_ON_ERROR | IM_READ_CONTROL | IM_RETRY_ENABLE | IM_BYPASS_BUFFER;
		last_scsi_command(shpnt)[ldn] = IM_READ_CAPACITY_CMD;
		last_scsi_type(shpnt)[ldn] = IM_SCB;
		scb->sys_buf_adr = isa_virt_to_bus(buf);
		scb->sys_buf_length = 8;
		scb->tsb_adr = isa_virt_to_bus(tsb);
		
		got_interrupt(shpnt) = 0;
		issue_cmd(shpnt, isa_virt_to_bus(scb), IM_SCB | ldn);
		while (!got_interrupt(shpnt))
			barrier();

		
		if ((stat_result(shpnt) == IM_SCB_CMD_COMPLETED) || (stat_result(shpnt) == IM_SCB_CMD_COMPLETED_WITH_RETRIES))
			return 1;
	}
	
	if (retr >= 3)
		return 0;
	else
		return 1;
}

static int get_pos_info(struct Scsi_Host *shpnt)
{
	int retr;
	struct im_scb *scb;
	struct im_tsb *tsb;
	unsigned char *buf;

	scb = &(ld(shpnt)[MAX_LOG_DEV].scb);
	tsb = &(ld(shpnt)[MAX_LOG_DEV].tsb);
	buf = (unsigned char *) (&(ld(shpnt)[MAX_LOG_DEV].buf));
	ld(shpnt)[MAX_LOG_DEV].tsb.dev_status = 0;
	for (retr = 0; retr < 3; retr++) {
		
		scb->command = IM_GET_POS_INFO_CMD;
		scb->enable = IM_READ_CONTROL | IM_REPORT_TSB_ONLY_ON_ERROR | IM_RETRY_ENABLE | IM_BYPASS_BUFFER;
		last_scsi_command(shpnt)[MAX_LOG_DEV] = IM_GET_POS_INFO_CMD;
		last_scsi_type(shpnt)[MAX_LOG_DEV] = IM_SCB;
		scb->sys_buf_adr = isa_virt_to_bus(buf);
		if (special(shpnt) == IBM_SCSI2_FW)
			scb->sys_buf_length = 256;	
		else
			scb->sys_buf_length = 18;	
		scb->tsb_adr = isa_virt_to_bus(tsb);
		
		got_interrupt(shpnt) = 0;
		issue_cmd(shpnt, isa_virt_to_bus(scb), IM_SCB | MAX_LOG_DEV);
		
		
		while (!got_interrupt(shpnt))
			barrier();

		
		if ((stat_result(shpnt) == IM_SCB_CMD_COMPLETED) || (stat_result(shpnt) == IM_SCB_CMD_COMPLETED_WITH_RETRIES))
			return 1;
	}
	
	if (retr >= 3)
		return 0;
	else
		return 1;
}

static int immediate_assign(struct Scsi_Host *shpnt, unsigned int pun,
			    unsigned int lun, unsigned int ldn,
			    unsigned int operation)
{
	int retr;
	unsigned long imm_cmd;

	for (retr = 0; retr < 3; retr++) {
		
		switch (special(shpnt)) {
		case IBM_SCSI2_FW:
			imm_cmd = (unsigned long) (IM_ASSIGN_IMM_CMD);
			imm_cmd |= (unsigned long) ((lun & 7) << 24);
			imm_cmd |= (unsigned long) ((operation & 1) << 23);
			imm_cmd |= (unsigned long) ((pun & 7) << 20) | ((pun & 8) << 24);
			imm_cmd |= (unsigned long) ((ldn & 15) << 16);
			break;
		default:
			imm_cmd = inl(IM_CMD_REG(shpnt));
			imm_cmd &= (unsigned long) (0xF8000000);	
			imm_cmd |= (unsigned long) (IM_ASSIGN_IMM_CMD);
			imm_cmd |= (unsigned long) ((lun & 7) << 24);
			imm_cmd |= (unsigned long) ((operation & 1) << 23);
			imm_cmd |= (unsigned long) ((pun & 7) << 20);
			imm_cmd |= (unsigned long) ((ldn & 15) << 16);
			break;
		}
		last_scsi_command(shpnt)[MAX_LOG_DEV] = IM_ASSIGN_IMM_CMD;
		last_scsi_type(shpnt)[MAX_LOG_DEV] = IM_IMM_CMD;
		got_interrupt(shpnt) = 0;
		issue_cmd(shpnt, (unsigned long) (imm_cmd), IM_IMM_CMD | MAX_LOG_DEV);
		while (!got_interrupt(shpnt))
			barrier();

		
		if (stat_result(shpnt) == IM_IMMEDIATE_CMD_COMPLETED)
			return 1;
	}
	if (retr >= 3)
		return 0;
	else
		return 1;
}

static int immediate_feature(struct Scsi_Host *shpnt, unsigned int speed, unsigned int timeout)
{
	int retr;
	unsigned long imm_cmd;

	for (retr = 0; retr < 3; retr++) {
		
		imm_cmd = IM_FEATURE_CTR_IMM_CMD;
		imm_cmd |= (unsigned long) ((speed & 0x7) << 29);
		imm_cmd |= (unsigned long) ((timeout & 0x1fff) << 16);
		last_scsi_command(shpnt)[MAX_LOG_DEV] = IM_FEATURE_CTR_IMM_CMD;
		last_scsi_type(shpnt)[MAX_LOG_DEV] = IM_IMM_CMD;
		got_interrupt(shpnt) = 0;
		global_command_error_excuse = 1;
		issue_cmd(shpnt, (unsigned long) (imm_cmd), IM_IMM_CMD | MAX_LOG_DEV);
		
		
		while (!got_interrupt(shpnt))
			barrier();
		if (global_command_error_excuse == CMD_FAIL) {
			global_command_error_excuse = 0;
			return 2;
		} else
			global_command_error_excuse = 0;
		
		if (stat_result(shpnt) == IM_IMMEDIATE_CMD_COMPLETED)
			return 1;
	}
	if (retr >= 3)
		return 0;
	else
		return 1;
}

#ifdef CONFIG_IBMMCA_SCSI_DEV_RESET
static int immediate_reset(struct Scsi_Host *shpnt, unsigned int ldn)
{
	int retries;
	int ticks;
	unsigned long imm_command;

	for (retries = 0; retries < 3; retries++) {
		imm_command = inl(IM_CMD_REG(shpnt));
		imm_command &= (unsigned long) (0xFFFF0000);	
		imm_command |= (unsigned long) (IM_RESET_IMM_CMD);
		last_scsi_command(shpnt)[ldn] = IM_RESET_IMM_CMD;
		last_scsi_type(shpnt)[ldn] = IM_IMM_CMD;
		got_interrupt(shpnt) = 0;
		reset_status(shpnt) = IM_RESET_IN_PROGRESS;
		issue_cmd(shpnt, (unsigned long) (imm_command), IM_IMM_CMD | ldn);
		ticks = IM_RESET_DELAY * HZ;
		while (reset_status(shpnt) == IM_RESET_IN_PROGRESS && --ticks) {
			udelay((1 + 999 / HZ) * 1000);
			barrier();
		}
		
		if (!ticks) {
			printk(KERN_ERR "IBM MCA SCSI: reset did not complete within %d seconds.\n", IM_RESET_DELAY);
			reset_status(shpnt) = IM_RESET_FINISHED_OK;
			
			return 1;
		}
		
		if (stat_result(shpnt) == IM_IMMEDIATE_CMD_COMPLETED)
			return 1;
	}
	if (retries >= 3)
		return 0;
	else
		return 1;
}
#endif

static char *ti_p(int dev)
{
	switch (dev) {
	case TYPE_IBM_SCSI_ADAPTER:
		return ("A");
	case TYPE_DISK:
		return ("D");
	case TYPE_TAPE:
		return ("T");
	case TYPE_PROCESSOR:
		return ("P");
	case TYPE_WORM:
		return ("W");
	case TYPE_ROM:
		return ("R");
	case TYPE_SCANNER:
		return ("S");
	case TYPE_MOD:
		return ("M");
	case TYPE_MEDIUM_CHANGER:
		return ("C");
	case TYPE_NO_LUN:
		return ("+");	
	}
	return ("-");		
}

static char *ti_l(int val)
{
	const char hex[16] = "0123456789abcdef";
	static char answer[2];

	answer[1] = (char) (0x0);
	if (val <= MAX_LOG_DEV)
		answer[0] = hex[val];
	else
		answer[0] = '-';
	return (char *) &answer;
}

static char *ibmrate(unsigned int speed, int i)
{
	switch (speed) {
	case 0:
		return i ? "5.00" : "10.00";
	case 1:
		return i ? "4.00" : "8.00";
	case 2:
		return i ? "3.33" : "6.66";
	case 3:
		return i ? "2.86" : "5.00";
	case 4:
		return i ? "2.50" : "4.00";
	case 5:
		return i ? "2.22" : "3.10";
	case 6:
		return i ? "2.00" : "2.50";
	case 7:
		return i ? "1.82" : "2.00";
	}
	return "---";
}

static int probe_display(int what)
{
	static int rotator = 0;
	const char rotor[] = "|/-\\";

	if (!(display_mode & LED_DISP))
		return 0;
	if (!what) {
		outl(0x20202020, MOD95_LED_PORT);
		outl(0x20202020, MOD95_LED_PORT + 4);
	} else {
		outb('S', MOD95_LED_PORT + 7);
		outb('C', MOD95_LED_PORT + 6);
		outb('S', MOD95_LED_PORT + 5);
		outb('I', MOD95_LED_PORT + 4);
		outb('i', MOD95_LED_PORT + 3);
		outb('n', MOD95_LED_PORT + 2);
		outb('i', MOD95_LED_PORT + 1);
		outb((char) (rotor[rotator]), MOD95_LED_PORT);
		rotator++;
		if (rotator > 3)
			rotator = 0;
	}
	return 0;
}

static int probe_bus_mode(struct Scsi_Host *shpnt)
{
	struct im_pos_info *info;
	int num_bus = 0;
	int ldn;

	info = (struct im_pos_info *) (&(ld(shpnt)[MAX_LOG_DEV].buf));
	if (get_pos_info(shpnt)) {
		if (info->connector_size & 0xf000)
			subsystem_connector_size(shpnt) = 16;
		else
			subsystem_connector_size(shpnt) = 32;
		num_bus |= (info->pos_4b & 8) >> 3;
		for (ldn = 0; ldn <= MAX_LOG_DEV; ldn++) {
			if ((special(shpnt) == IBM_SCSI_WCACHE) || (special(shpnt) == IBM_7568_WCACHE)) {
				if (!((info->cache_stat >> ldn) & 1))
					ld(shpnt)[ldn].cache_flag = 0;
			}
			if (!((info->retry_stat >> ldn) & 1))
				ld(shpnt)[ldn].retry_flag = 0;
		}
#ifdef IM_DEBUG_PROBE
		printk("IBM MCA SCSI: SCSI-Cache bits: ");
		for (ldn = 0; ldn <= MAX_LOG_DEV; ldn++) {
			printk("%d", ld(shpnt)[ldn].cache_flag);
		}
		printk("\nIBM MCA SCSI: SCSI-Retry bits: ");
		for (ldn = 0; ldn <= MAX_LOG_DEV; ldn++) {
			printk("%d", ld(shpnt)[ldn].retry_flag);
		}
		printk("\n");
#endif
	}
	return num_bus;
}

static void check_devices(struct Scsi_Host *shpnt, int adaptertype)
{
	int id, lun, ldn, ticks;
	int count_devices;	
	int max_pun;
	int num_bus;
	int speedrun;		

	
	ticks = 0;
	count_devices = 0;
	IBM_DS(shpnt).dyn_flag = 0;	
	IBM_DS(shpnt).total_errors = 0;	
	next_ldn(shpnt) = 7;	

	
	memset(ld(shpnt), 0, sizeof(ld(shpnt)));
	for (ldn = 0; ldn <= MAX_LOG_DEV; ldn++) {
		last_scsi_command(shpnt)[ldn] = NO_SCSI;	
		last_scsi_type(shpnt)[ldn] = 0;
		ld(shpnt)[ldn].cache_flag = 1;
		ld(shpnt)[ldn].retry_flag = 1;
	}
	memset(get_ldn(shpnt), TYPE_NO_DEVICE, sizeof(get_ldn(shpnt)));	
	memset(get_scsi(shpnt), TYPE_NO_DEVICE, sizeof(get_scsi(shpnt)));	
	for (lun = 0; lun < 8; lun++) {
		
		get_scsi(shpnt)[subsystem_pun(shpnt)][lun] = TYPE_IBM_SCSI_ADAPTER;
		get_ldn(shpnt)[subsystem_pun(shpnt)][lun] = MAX_LOG_DEV;	
	}
	probe_display(0);	
	
	

	
	adapter_speed(shpnt) = global_adapter_speed;
	speedrun = adapter_speed(shpnt);
	while (immediate_feature(shpnt, speedrun, adapter_timeout) == 2) {
		probe_display(1);
		if (speedrun == 7)
			panic("IBM MCA SCSI: Cannot set Synchronous-Transfer-Rate!\n");
		speedrun++;
		if (speedrun > 7)
			speedrun = 7;
	}
	adapter_speed(shpnt) = speedrun;
	num_bus = probe_bus_mode(shpnt);

	
	if (adaptertype == IBM_SCSI2_FW) {	
		
		if (num_bus) {
			printk(KERN_INFO "IBM MCA SCSI: Separate bus mode (wide-addressing enabled)\n");
			subsystem_maxid(shpnt) = 16;
		} else {
			printk(KERN_INFO "IBM MCA SCSI: Combined bus mode (wide-addressing disabled)\n");
			subsystem_maxid(shpnt) = 8;
		}
		printk(KERN_INFO "IBM MCA SCSI: Sync.-Rate (F/W: 20, Int.: 10, Ext.: %s) MBytes/s\n", ibmrate(speedrun, adaptertype));
	} else			
		printk(KERN_INFO "IBM MCA SCSI: Synchronous-SCSI-Transfer-Rate: %s MBytes/s\n", ibmrate(speedrun, adaptertype));

	
	max_pun = subsystem_maxid(shpnt);

#ifdef IM_DEBUG_PROBE
	printk("IBM MCA SCSI: Current SCSI-host index: %d\n", shpnt);
	printk("IBM MCA SCSI: Removing default logical SCSI-device mapping.");
#else
	printk(KERN_INFO "IBM MCA SCSI: Dev. Order: %s, Mapping (takes <2min): ", (ibm_ansi_order) ? "ANSI" : "New");
#endif
	for (ldn = 0; ldn < MAX_LOG_DEV; ldn++) {
		probe_display(1);
#ifdef IM_DEBUG_PROBE
		printk(".");
#endif
		immediate_assign(shpnt, 0, 0, ldn, REMOVE_LDN);	
	}
	lun = 0;		
#ifndef IM_DEBUG_PROBE
	printk("cleared,");
#endif
	
#ifdef IM_DEBUG_PROBE
	printk("\nIBM MCA SCSI: Scanning SCSI-devices.");
#endif
	for (id = 0; id < max_pun; id++)
#ifdef CONFIG_SCSI_MULTI_LUN
		for (lun = 0; lun < 8; lun++)
#endif
		{
			probe_display(1);
#ifdef IM_DEBUG_PROBE
			printk(".");
#endif
			if (id != subsystem_pun(shpnt)) {
				
				
				immediate_assign(shpnt, id, lun, PROBE_LDN, SET_LDN);
				if (device_inquiry(shpnt, PROBE_LDN)) {	
					get_scsi(shpnt)[id][lun] = (unsigned char) (ld(shpnt)[PROBE_LDN].buf[0]);
					
					if (ld(shpnt)[PROBE_LDN].buf[0] != TYPE_NO_LUN)
						count_devices++;	
				}
				
				immediate_assign(shpnt, id, lun, PROBE_LDN, REMOVE_LDN);
			}
		}
#ifndef IM_DEBUG_PROBE
	printk("scanned,");
#endif
	
#ifdef IM_DEBUG_PROBE
	printk("\nIBM MCA SCSI: Mapping SCSI-devices.");
#endif
	ldn = 0;
	lun = 0;
#ifdef CONFIG_SCSI_MULTI_LUN
	for (lun = 0; lun < 8 && ldn < MAX_LOG_DEV; lun++)
#endif
		for (id = 0; id < max_pun && ldn < MAX_LOG_DEV; id++) {
			probe_display(1);
#ifdef IM_DEBUG_PROBE
			printk(".");
#endif
			if (id != subsystem_pun(shpnt)) {
				if (get_scsi(shpnt)[id][lun] != TYPE_NO_LUN && get_scsi(shpnt)[id][lun] != TYPE_NO_DEVICE) {
					immediate_assign(shpnt, id, lun, ldn, SET_LDN);
					get_ldn(shpnt)[id][lun] = ldn;	
					if (device_exists(shpnt, ldn, &ld(shpnt)[ldn].block_length, &ld(shpnt)[ldn].device_type)) {
#ifdef CONFIG_IBMMCA_SCSI_DEV_RESET
						printk("resetting device at ldn=%x ... ", ldn);
						immediate_reset(shpnt, ldn);
#endif
						ldn++;
					} else {
						if (lun > 0) {
							
							get_ldn(shpnt)[id][lun] = TYPE_NO_DEVICE;
							immediate_assign(shpnt, 0, 0, ldn, REMOVE_LDN);
						} else
							ldn++;
					}
				} else if (lun == 0) {
					
					immediate_assign(shpnt, id, lun, ldn, SET_LDN);
					get_ldn(shpnt)[id][lun] = ldn;	
					ldn++;
				}
			}
		}
	

	
	for (lun = 1; lun < 8 && ldn < MAX_LOG_DEV; lun++)
		for (id = 0; id < max_pun && ldn < MAX_LOG_DEV; id++) {
			if (get_scsi(shpnt)[id][lun] == TYPE_NO_LUN || get_scsi(shpnt)[id][lun] == TYPE_NO_DEVICE) {
				probe_display(1);
				immediate_assign(shpnt, id, lun, ldn, SET_LDN);
				get_ldn(shpnt)[id][lun] = ldn;
				ldn++;
			}
		}
#ifndef IM_DEBUG_PROBE
	printk("mapped.");
#endif
	printk("\n");
#ifdef IM_DEBUG_PROBE
	if (ibm_ansi_order)
		printk("IBM MCA SCSI: Device order: IBM/ANSI (pun=7 is first).\n");
	else
		printk("IBM MCA SCSI: Device order: New Industry Standard (pun=0 is first).\n");
#endif

#ifdef IM_DEBUG_PROBE
	
	printk("IBM MCA SCSI: Determined SCSI-device-mapping:\n");
	printk("    Physical SCSI-Device Map               Logical SCSI-Device Map\n");
	printk("ID\\LUN  0  1  2  3  4  5  6  7       ID\\LUN  0  1  2  3  4  5  6  7\n");
	for (id = 0; id < max_pun; id++) {
		printk("%2d     ", id);
		for (lun = 0; lun < 8; lun++)
			printk("%2s ", ti_p(get_scsi(shpnt)[id][lun]));
		printk("      %2d     ", id);
		for (lun = 0; lun < 8; lun++)
			printk("%2s ", ti_l(get_ldn(shpnt)[id][lun]));
		printk("\n");
	}
#endif

	
	IBM_DS(shpnt).total_scsi_devices = count_devices;

	if (count_devices >= MAX_LOG_DEV)
		IBM_DS(shpnt).dyn_flag = 1;	
	else
		IBM_DS(shpnt).dyn_flag = 0;	

	
	if (ldn == 0)
		printk("IBM MCA SCSI: Warning: No SCSI-devices found/assigned!\n");

	
	IBM_DS(shpnt).scbs = 0;
	IBM_DS(shpnt).long_scbs = 0;
	IBM_DS(shpnt).total_accesses = 0;
	IBM_DS(shpnt).total_interrupts = 0;
	IBM_DS(shpnt).dynamical_assignments = 0;
	memset(IBM_DS(shpnt).ldn_access, 0x0, sizeof(IBM_DS(shpnt).ldn_access));
	memset(IBM_DS(shpnt).ldn_read_access, 0x0, sizeof(IBM_DS(shpnt).ldn_read_access));
	memset(IBM_DS(shpnt).ldn_write_access, 0x0, sizeof(IBM_DS(shpnt).ldn_write_access));
	memset(IBM_DS(shpnt).ldn_inquiry_access, 0x0, sizeof(IBM_DS(shpnt).ldn_inquiry_access));
	memset(IBM_DS(shpnt).ldn_modeselect_access, 0x0, sizeof(IBM_DS(shpnt).ldn_modeselect_access));
	memset(IBM_DS(shpnt).ldn_assignments, 0x0, sizeof(IBM_DS(shpnt).ldn_assignments));
	probe_display(0);
	return;
}

static int device_exists(struct Scsi_Host *shpnt, int ldn, int *block_length, int *device_type)
{
	unsigned char *buf;
	
	if (!(device_inquiry(shpnt, ldn)))
		return 0;
	buf = (unsigned char *) (&(ld(shpnt)[ldn].buf));
	if (*buf == TYPE_ROM) {
		*device_type = TYPE_ROM;
		*block_length = 2048;	
		return 1;
	}
	if (*buf == TYPE_WORM) {
		*device_type = TYPE_WORM;
		*block_length = 2048;
		return 1;
	}
	if (*buf == TYPE_DISK) {
		*device_type = TYPE_DISK;
		if (read_capacity(shpnt, ldn)) {
			*block_length = *(buf + 7) + (*(buf + 6) << 8) + (*(buf + 5) << 16) + (*(buf + 4) << 24);
			return 1;
		} else
			return 0;
	}
	if (*buf == TYPE_MOD) {
		*device_type = TYPE_MOD;
		if (read_capacity(shpnt, ldn)) {
			*block_length = *(buf + 7) + (*(buf + 6) << 8) + (*(buf + 5) << 16) + (*(buf + 4) << 24);
			return 1;
		} else
			return 0;
	}
	if (*buf == TYPE_TAPE) {
		*device_type = TYPE_TAPE;
		*block_length = 0;	
		return 1;
	}
	if (*buf == TYPE_PROCESSOR) {
		*device_type = TYPE_PROCESSOR;
		*block_length = 0;	
		return 1;
	}
	if (*buf == TYPE_SCANNER) {
		*device_type = TYPE_SCANNER;
		*block_length = 0;	
		return 1;
	}
	if (*buf == TYPE_MEDIUM_CHANGER) {
		*device_type = TYPE_MEDIUM_CHANGER;
		*block_length = 0;	
		return 1;
	}
	return 0;
}

static void internal_ibmmca_scsi_setup(char *str, int *ints)
{
	int i, j, io_base, id_base;
	char *token;

	io_base = 0;
	id_base = 0;
	if (str) {
		j = 0;
		while ((token = strsep(&str, ",")) != NULL) {
			if (!strcmp(token, "activity"))
				display_mode |= LED_ACTIVITY;
			if (!strcmp(token, "display"))
				display_mode |= LED_DISP;
			if (!strcmp(token, "adisplay"))
				display_mode |= LED_ADISP;
			if (!strcmp(token, "normal"))
				ibm_ansi_order = 0;
			if (!strcmp(token, "ansi"))
				ibm_ansi_order = 1;
			if (!strcmp(token, "fast"))
				global_adapter_speed = 0;
			if (!strcmp(token, "medium"))
				global_adapter_speed = 4;
			if (!strcmp(token, "slow"))
				global_adapter_speed = 7;
			if ((*token == '-') || (isdigit(*token))) {
				if (!(j % 2) && (io_base < IM_MAX_HOSTS))
					io_port[io_base++] = simple_strtoul(token, NULL, 0);
				if ((j % 2) && (id_base < IM_MAX_HOSTS))
					scsi_id[id_base++] = simple_strtoul(token, NULL, 0);
				j++;
			}
		}
	} else if (ints) {
		for (i = 0; i < IM_MAX_HOSTS && 2 * i + 2 < ints[0]; i++) {
			io_port[i] = ints[2 * i + 2];
			scsi_id[i] = ints[2 * i + 2];
		}
	}
	return;
}

#if 0
 FIXME NEED TO MOVE TO SYSFS

static int ibmmca_getinfo(char *buf, int slot, void *dev_id)
{
	struct Scsi_Host *shpnt;
	int len, speciale, connectore, k;
	unsigned int pos[8];
	unsigned long flags;
	struct Scsi_Host *dev = dev_id;

	spin_lock_irqsave(dev->host_lock, flags);

	shpnt = dev;		
	len = 0;		
	
	speciale = ((struct ibmmca_hostdata *) shpnt->hostdata)->_special;
	connectore = ((struct ibmmca_hostdata *) shpnt->hostdata)->_connector_size;
	for (k = 2; k < 4; k++)
		pos[k] = ((struct ibmmca_hostdata *) shpnt->hostdata)->_pos[k];
	if (speciale == FORCED_DETECTION) {	
		len += sprintf(buf + len,
			       "Adapter category: forced detected\n" "***************************************\n" "***  Forced detected SCSI Adapter   ***\n" "***  No chip-information available  ***\n" "***************************************\n");
	} else if (speciale == INTEGRATED_SCSI) {
		
		len += sprintf(buf + len,
			       "Adapter category: integrated\n" "Chip revision level: %d\n" "Chip status: %s\n" "8 kByte NVRAM status: %s\n", ((pos[2] & 0xf0) >> 4), (pos[2] & 1) ? "enabled" : "disabled", (pos[2] & 2) ? "locked" : "accessible");
	} else if ((speciale >= 0) && (speciale < ARRAY_SIZE(subsys_list))) {
		
		len += sprintf(buf + len, "Adapter category: slot-card\n" "ROM Segment Address: ");
		if ((pos[2] & 0xf0) == 0xf0)
			len += sprintf(buf + len, "off\n");
		else
			len += sprintf(buf + len, "0x%x\n", ((pos[2] & 0xf0) << 13) + 0xc0000);
		len += sprintf(buf + len, "Chip status: %s\n", (pos[2] & 1) ? "enabled" : "disabled");
		len += sprintf(buf + len, "Adapter I/O Offset: 0x%x\n", ((pos[2] & 0x0e) << 2));
	} else {
		len += sprintf(buf + len, "Adapter category: unknown\n");
	}
	
	len += sprintf(buf + len, "Subsystem PUN: %d\n", shpnt->this_id);
	len += sprintf(buf + len, "I/O base address range: 0x%x-0x%x\n", (unsigned int) (shpnt->io_port), (unsigned int) (shpnt->io_port + 7));
	len += sprintf(buf + len, "MCA-slot size: %d bits", connectore);
	while (len % sizeof(int) != (sizeof(int) - 1))
		len += sprintf(buf + len, " ");
	len += sprintf(buf + len, "\n");

	spin_unlock_irqrestore(shpnt->host_lock, flags);

	return len;
}
#endif

static struct scsi_host_template ibmmca_driver_template = {
          .proc_name      = "ibmmca",
	  .proc_info	  = ibmmca_proc_info,
          .name           = "IBM SCSI-Subsystem",
          .queuecommand   = ibmmca_queuecommand,
	  .eh_abort_handler = ibmmca_abort,
	  .eh_host_reset_handler = ibmmca_host_reset,
          .bios_param     = ibmmca_biosparam,
          .can_queue      = 16,
          .this_id        = 7,
          .sg_tablesize   = 16,
          .cmd_per_lun    = 1,
          .use_clustering = ENABLE_CLUSTERING,
};

static int ibmmca_probe(struct device *dev)
{
	struct Scsi_Host *shpnt;
	int port, id, i, j, k, irq, enabled, ret = -EINVAL;
	struct mca_device *mca_dev = to_mca_device(dev);
	const char *description = ibmmca_description[mca_dev->index];

	printk(KERN_INFO "IBM MCA SCSI: Version %s\n", IBMMCA_SCSI_DRIVER_VERSION);

	
	for (j = 0; j < 8; j++)	
		pos[j] = mca_device_read_pos(mca_dev, j);
	id = (pos[3] & 0xe0) >> 5; 
	enabled = (pos[2] &0x01);
	if (!enabled) {
		printk(KERN_WARNING "IBM MCA SCSI: WARNING - Your SCSI-subsystem is disabled!\n");
		printk(KERN_WARNING "              SCSI-operations may not work.\n");
	}

	if (mca_dev->slot == MCA_INTEGSCSI &&
	    ((!pos[0] && !pos[1] && pos[2] > 0 &&
	      pos[3] > 0 && !pos[4] && !pos[5] &&
	      !pos[6] && !pos[7]) ||
	     (pos[0] == 0xff && pos[1] == 0xff &&
	      pos[2] < 0xff && pos[3] < 0xff &&
	      pos[4] == 0xff && pos[5] == 0xff &&
	      pos[6] == 0xff && pos[7] == 0xff))) {
		irq = IM_IRQ;
		port = IM_IO_PORT;
	} else {
		irq = IM_IRQ;
		port = IM_IO_PORT + ((pos[2] &0x0e) << 2);
		if ((mca_dev->index == IBM_SCSI2_FW) && (pos[6] != 0)) {
			printk(KERN_ERR "IBM MCA SCSI: ERROR - Wrong POS(6)-register setting!\n");
			printk(KERN_ERR "              Impossible to determine adapter PUN!\n");
			printk(KERN_ERR "              Guessing adapter PUN = 7.\n");
			id = 7;
		} else {
			id = (pos[3] & 0xe0) >> 5;	
			if (mca_dev->index == IBM_SCSI2_FW) {
				id |= (pos[3] & 0x10) >> 1;	
			}
		}
		if ((mca_dev->index == IBM_SCSI2_FW) &&
		    (pos[4] & 0x01) && (pos[6] == 0)) {
			
			printk(KERN_DEBUG "IBM MCA SCSI: SCSI-2 F/W adapter needs IRQ 11.\n");
			irq = IM_IRQ_FW;
		}
	}



	printk(KERN_INFO "IBM MCA SCSI: %s found, io=0x%x, scsi id=%d,\n",
	       description, port, id);
	if (mca_dev->slot == MCA_INTEGSCSI)
		printk(KERN_INFO "              chip rev.=%d, 8K NVRAM=%s, subsystem=%s\n", ((pos[2] & 0xf0) >> 4), (pos[2] & 2) ? "locked" : "accessible", (pos[2] & 1) ? "enabled." : "disabled.");
	else {
		if ((pos[2] & 0xf0) == 0xf0)
			printk(KERN_DEBUG "              ROM Addr.=off,");
		else
			printk(KERN_DEBUG "              ROM Addr.=0x%x,", ((pos[2] & 0xf0) << 13) + 0xc0000);

		printk(KERN_DEBUG " port-offset=0x%x, subsystem=%s\n", ((pos[2] & 0x0e) << 2), (pos[2] & 1) ? "enabled." : "disabled.");
	}

	
	if (!request_region(port, IM_N_IO_PORT, description)) {
		printk(KERN_ERR "IBM MCA SCSI: Unable to get I/O region 0x%x-0x%x (%d ports).\n", port, port + IM_N_IO_PORT - 1, IM_N_IO_PORT);
		goto out_fail;
	}

	
	shpnt = scsi_host_alloc(&ibmmca_driver_template,
				sizeof(struct ibmmca_hostdata));
	if (!shpnt) {
		printk(KERN_ERR "IBM MCA SCSI: Unable to register host.\n");
		goto out_release;
	}

	dev_set_drvdata(dev, shpnt);
	if(request_irq(irq, interrupt_handler, IRQF_SHARED, description, dev)) {
		printk(KERN_ERR "IBM MCA SCSI: failed to request interrupt %d\n", irq);
		goto out_free_host;
	}

	
	special(shpnt) = mca_dev->index;	
	subsystem_connector_size(shpnt) = 0;	
	shpnt->irq = irq;	
	shpnt->io_port = port;
	shpnt->n_io_port = IM_N_IO_PORT;
	shpnt->this_id = id;
	shpnt->max_id = 8;	
	

#ifdef IM_DEBUG_PROBE
	ctrl = (unsigned int) (inb(IM_CTR_REG(found)));	
	printk("IBM MCA SCSI: Control Register contents: %x, status: %x\n", ctrl, inb(IM_STAT_REG(found)));
	printk("IBM MCA SCSI: This adapters' POS-registers: ");
	for (i = 0; i < 8; i++)
		printk("%x ", pos[i]);
	printk("\n");
#endif
	reset_status(shpnt) = IM_RESET_NOT_IN_PROGRESS;

	for (i = 0; i < 16; i++)	
		for (j = 0; j < 8; j++)
			get_ldn(shpnt)[i][j] = MAX_LOG_DEV;

	
	
	local_checking_phase_flag(shpnt) = 1;
	check_devices(shpnt, mca_dev->index);	
	local_checking_phase_flag(shpnt) = 0;

	

	for (k = 2; k < 7; k++)
		((struct ibmmca_hostdata *) shpnt->hostdata)->_pos[k] = pos[k];
	((struct ibmmca_hostdata *) shpnt->hostdata)->_special = INTEGRATED_SCSI;
	mca_device_set_name(mca_dev, description);
	mca_device_set_claim(mca_dev, 1);
	if (scsi_add_host(shpnt, dev)) {
		dev_printk(KERN_ERR, dev, "IBM MCA SCSI: scsi_add_host failed\n");
		goto out_free_host;
	}
	scsi_scan_host(shpnt);

	return 0;
 out_free_host:
	scsi_host_put(shpnt);
 out_release:
	release_region(port, IM_N_IO_PORT);
 out_fail:
	return ret;
}

static int __devexit ibmmca_remove(struct device *dev)
{
	struct Scsi_Host *shpnt = dev_get_drvdata(dev);
	scsi_remove_host(shpnt);
	release_region(shpnt->io_port, shpnt->n_io_port);
	free_irq(shpnt->irq, dev);
	scsi_host_put(shpnt);
	return 0;
}

static int ibmmca_queuecommand_lck(Scsi_Cmnd * cmd, void (*done) (Scsi_Cmnd *))
{
	unsigned int ldn;
	unsigned int scsi_cmd;
	struct im_scb *scb;
	struct Scsi_Host *shpnt;
	int current_ldn;
	int id, lun;
	int target;
	int max_pun;
	int i;
	struct scatterlist *sg;

	shpnt = cmd->device->host;

	max_pun = subsystem_maxid(shpnt);
	if (ibm_ansi_order) {
		target = max_pun - 1 - cmd->device->id;
		if ((target <= subsystem_pun(shpnt)) && (cmd->device->id <= subsystem_pun(shpnt)))
			target--;
		else if ((target >= subsystem_pun(shpnt)) && (cmd->device->id >= subsystem_pun(shpnt)))
			target++;
	} else
		target = cmd->device->id;

	
	if ((get_scsi(shpnt)[target][cmd->device->lun] == TYPE_NO_LUN) || (get_scsi(shpnt)[target][cmd->device->lun] == TYPE_NO_DEVICE)) {
		cmd->result = DID_NO_CONNECT << 16;
		if (done)
			done(cmd);
		return 0;
	}

	
	ldn = get_ldn(shpnt)[target][cmd->device->lun];
	if (ldn >= MAX_LOG_DEV) {	
		if (ldn > MAX_LOG_DEV) {	
			current_ldn = next_ldn(shpnt);	
			while (ld(shpnt)[next_ldn(shpnt)].cmd) {	
				
				next_ldn(shpnt)++;
				if (next_ldn(shpnt) >= MAX_LOG_DEV)
					next_ldn(shpnt) = 7;
				if (current_ldn == next_ldn(shpnt)) {	
					
					scmd_printk(KERN_WARNING, cmd,
	"IBM MCA SCSI: Cannot assign SCSI-device dynamically!\n"
	"              On ldn 7-14 SCSI-commands everywhere in progress.\n"
	"              Reporting DID_NO_CONNECT for device.\n");
					cmd->result = DID_NO_CONNECT << 16;	
					if (done)
						done(cmd);
					return 0;
				}
			}

			
			for (id = 0; id < max_pun; id++)
				for (lun = 0; lun < 8; lun++) {
					if (get_ldn(shpnt)[id][lun] == next_ldn(shpnt)) {
						get_ldn(shpnt)[id][lun] = TYPE_NO_DEVICE;
						get_scsi(shpnt)[id][lun] = TYPE_NO_DEVICE;
						
					}
				}
			
			local_checking_phase_flag(shpnt) = 1;
			
			get_ldn(shpnt)[target][cmd->device->lun] = next_ldn(shpnt);
			
			ldn = next_ldn(shpnt);
			
			immediate_assign(shpnt, 0, 0, 0, REMOVE_LDN);
			
			immediate_assign(shpnt, target, cmd->device->lun, ldn, SET_LDN);
			
			if (device_exists(shpnt, ldn, &ld(shpnt)[ldn].block_length, &ld(shpnt)[ldn].device_type)) {
				ld(shpnt)[ldn].cmd = NULL;	
				get_scsi(shpnt)[target][cmd->device->lun] = ld(shpnt)[ldn].device_type;
				
				IBM_DS(shpnt).dynamical_assignments++;
				IBM_DS(shpnt).ldn_assignments[ldn]++;
			} else
				panic("IBM MCA SCSI: ldn=0x%x, SCSI-device on (%d,%d) vanished!\n", ldn, target, cmd->device->lun);
			
			immediate_assign(shpnt, 0, 0, 0, REMOVE_LDN);
			/* remap all ldns, as written in the pun/lun table */
			lun = 0;
#ifdef CONFIG_SCSI_MULTI_LUN
			for (lun = 0; lun < 8; lun++)
#endif
				for (id = 0; id < max_pun; id++) {
					if (get_ldn(shpnt)[id][lun] <= MAX_LOG_DEV)
						immediate_assign(shpnt, id, lun, get_ldn(shpnt)[id][lun], SET_LDN);
				}
			
			local_checking_phase_flag(shpnt) = 0;
#ifdef IM_DEBUG_PROBE
			
			printk("IBM MCA SCSI: ldn=0x%x dynamically reassigned to (%d,%d).\n", ldn, target, cmd->device->lun);
#endif
			
			next_ldn(shpnt)++;
			if (next_ldn(shpnt) >= MAX_LOG_DEV)
				next_ldn(shpnt) = 7;
		} else {	
			cmd->result = DID_BAD_TARGET << 16;
			if (done)
				done(cmd);
			return 0;
		}
	}

	
	if (ld(shpnt)[ldn].cmd)
		panic("IBM MCA SCSI: cmd already in progress for this ldn.\n");

	
	cmd->scsi_done = done;
	ld(shpnt)[ldn].cmd = cmd;

	
	scb = &(ld(shpnt)[ldn].scb);
	ld(shpnt)[ldn].tsb.dev_status = 0;
	scb->enable = IM_REPORT_TSB_ONLY_ON_ERROR | IM_RETRY_ENABLE;
	scb->tsb_adr = isa_virt_to_bus(&(ld(shpnt)[ldn].tsb));
	scsi_cmd = cmd->cmnd[0];

	if (scsi_sg_count(cmd)) {
		BUG_ON(scsi_sg_count(cmd) > 16);

		scsi_for_each_sg(cmd, sg, scsi_sg_count(cmd), i) {
			ld(shpnt)[ldn].sge[i].address = (void *) (isa_page_to_bus(sg_page(sg)) + sg->offset);
			ld(shpnt)[ldn].sge[i].byte_length = sg->length;
		}
		scb->enable |= IM_POINTER_TO_LIST;
		scb->sys_buf_adr = isa_virt_to_bus(&(ld(shpnt)[ldn].sge[0]));
		scb->sys_buf_length = scsi_sg_count(cmd) * sizeof(struct im_sge);
	} else {
 		scb->sys_buf_adr = isa_virt_to_bus(scsi_sglist(cmd));
		switch (scsi_cmd) {
		case INQUIRY:
		case REQUEST_SENSE:
		case MODE_SENSE:
		case MODE_SELECT:
			if (scsi_bufflen(cmd) > 255)
				scb->sys_buf_length = 255;
			else
				scb->sys_buf_length = scsi_bufflen(cmd);
			break;
		case TEST_UNIT_READY:
			scb->sys_buf_length = 0;
			break;
		default:
			scb->sys_buf_length = scsi_bufflen(cmd);
			break;
		}
	}
	

#ifdef IM_DEBUG_CMD
	printk("issue scsi cmd=%02x to ldn=%d\n", scsi_cmd, ldn);
#endif

	
#ifdef IM_DEBUG_CMD_SPEC_DEV
	if (ld(shpnt)[ldn].device_type == IM_DEBUG_CMD_DEVICE)
		printk("(SCSI-device-type=0x%x) issue scsi cmd=%02x to ldn=%d\n", ld(shpnt)[ldn].device_type, scsi_cmd, ldn);
#endif

	
	last_scsi_command(shpnt)[ldn] = scsi_cmd;
	last_scsi_type(shpnt)[ldn] = IM_SCB;
	
	IBM_DS(shpnt).total_accesses++;
	IBM_DS(shpnt).ldn_access[ldn]++;

	switch (scsi_cmd) {
	case READ_6:
	case WRITE_6:
	case READ_10:
	case WRITE_10:
	case READ_12:
	case WRITE_12:
		switch (ld(shpnt)[ldn].device_type) {
		case TYPE_DISK:	
		case TYPE_MOD:	
			
			if (scsi_cmd == READ_6 || scsi_cmd == READ_10 || scsi_cmd == READ_12) {
				
				scb->enable |= IM_READ_CONTROL;
				IBM_DS(shpnt).ldn_read_access[ldn]++;	
				scb->command = IM_READ_DATA_CMD | IM_NO_DISCONNECT;
			} else {	
				IBM_DS(shpnt).ldn_write_access[ldn]++;	
				scb->command = IM_WRITE_DATA_CMD | IM_NO_DISCONNECT;
			}
			if (scsi_cmd == READ_6 || scsi_cmd == WRITE_6) {
				scb->u1.log_blk_adr = (((unsigned) cmd->cmnd[3]) << 0) | (((unsigned) cmd->cmnd[2]) << 8) | ((((unsigned) cmd->cmnd[1]) & 0x1f) << 16);
				scb->u2.blk.count = (unsigned) cmd->cmnd[4];
			} else {
				scb->u1.log_blk_adr = (((unsigned) cmd->cmnd[5]) << 0) | (((unsigned) cmd->cmnd[4]) << 8) | (((unsigned) cmd->cmnd[3]) << 16) | (((unsigned) cmd->cmnd[2]) << 24);
				scb->u2.blk.count = (((unsigned) cmd->cmnd[8]) << 0) | (((unsigned) cmd->cmnd[7]) << 8);
			}
			last_scsi_logical_block(shpnt)[ldn] = scb->u1.log_blk_adr;
			last_scsi_blockcount(shpnt)[ldn] = scb->u2.blk.count;
			scb->u2.blk.length = ld(shpnt)[ldn].block_length;
			break;
		case TYPE_ROM:
		case TYPE_TAPE:
		case TYPE_PROCESSOR:
		case TYPE_WORM:
		case TYPE_SCANNER:
		case TYPE_MEDIUM_CHANGER:
			scb->command = IM_OTHER_SCSI_CMD_CMD;
			if (scsi_cmd == READ_6 || scsi_cmd == READ_10 || scsi_cmd == READ_12)
				
				scb->enable |= IM_READ_CONTROL;
			scb->enable |= IM_BYPASS_BUFFER;
			scb->u1.scsi_cmd_length = cmd->cmd_len;
			memcpy(scb->u2.scsi_command, cmd->cmnd, cmd->cmd_len);
			last_scsi_type(shpnt)[ldn] = IM_LONG_SCB;
			break;
		}
		break;
	case INQUIRY:
		IBM_DS(shpnt).ldn_inquiry_access[ldn]++;
		scb->command = IM_DEVICE_INQUIRY_CMD;
		scb->enable |= IM_READ_CONTROL | IM_SUPRESS_EXCEPTION_SHORT | IM_BYPASS_BUFFER;
		scb->u1.log_blk_adr = 0;
		break;
	case TEST_UNIT_READY:
		scb->command = IM_OTHER_SCSI_CMD_CMD;
		scb->enable |= IM_READ_CONTROL | IM_SUPRESS_EXCEPTION_SHORT | IM_BYPASS_BUFFER;
		scb->u1.log_blk_adr = 0;
		scb->u1.scsi_cmd_length = 6;
		memcpy(scb->u2.scsi_command, cmd->cmnd, 6);
		last_scsi_type(shpnt)[ldn] = IM_LONG_SCB;
		break;
	case READ_CAPACITY:
		
		scb->command = IM_READ_CAPACITY_CMD;
		scb->enable |= IM_READ_CONTROL | IM_BYPASS_BUFFER;
		if (scb->sys_buf_length > 8)
			scb->sys_buf_length = 8;
		break;
		
	case REQUEST_SENSE:
		scb->command = IM_REQUEST_SENSE_CMD;
		scb->enable |= IM_READ_CONTROL | IM_SUPRESS_EXCEPTION_SHORT | IM_BYPASS_BUFFER;
		break;
		
	case MODE_SELECT:
	case MODE_SELECT_10:
		IBM_DS(shpnt).ldn_modeselect_access[ldn]++;
		scb->command = IM_OTHER_SCSI_CMD_CMD;
		scb->enable |= IM_SUPRESS_EXCEPTION_SHORT | IM_BYPASS_BUFFER;	
		scb->u1.scsi_cmd_length = cmd->cmd_len;
		memcpy(scb->u2.scsi_command, cmd->cmnd, cmd->cmd_len);
		last_scsi_type(shpnt)[ldn] = IM_LONG_SCB;
		break;
	default:
		scb->command = IM_OTHER_SCSI_CMD_CMD;
		scb->enable |= IM_READ_CONTROL | IM_SUPRESS_EXCEPTION_SHORT | IM_BYPASS_BUFFER;
		scb->u1.scsi_cmd_length = cmd->cmd_len;
		memcpy(scb->u2.scsi_command, cmd->cmnd, cmd->cmd_len);
		last_scsi_type(shpnt)[ldn] = IM_LONG_SCB;
		break;
	}
	
	if (++disk_rw_in_progress == 1)
		PS2_DISK_LED_ON(shpnt->host_no, target);

	if (last_scsi_type(shpnt)[ldn] == IM_LONG_SCB) {
		issue_cmd(shpnt, isa_virt_to_bus(scb), IM_LONG_SCB | ldn);
		IBM_DS(shpnt).long_scbs++;
	} else {
		issue_cmd(shpnt, isa_virt_to_bus(scb), IM_SCB | ldn);
		IBM_DS(shpnt).scbs++;
	}
	return 0;
}

static DEF_SCSI_QCMD(ibmmca_queuecommand)

static int __ibmmca_abort(Scsi_Cmnd * cmd)
{

	struct Scsi_Host *shpnt;
	unsigned int ldn;
	void (*saved_done) (Scsi_Cmnd *);
	int target;
	int max_pun;
	unsigned long imm_command;

#ifdef IM_DEBUG_PROBE
	printk("IBM MCA SCSI: Abort subroutine called...\n");
#endif

	shpnt = cmd->device->host;

	max_pun = subsystem_maxid(shpnt);
	if (ibm_ansi_order) {
		target = max_pun - 1 - cmd->device->id;
		if ((target <= subsystem_pun(shpnt)) && (cmd->device->id <= subsystem_pun(shpnt)))
			target--;
		else if ((target >= subsystem_pun(shpnt)) && (cmd->device->id >= subsystem_pun(shpnt)))
			target++;
	} else
		target = cmd->device->id;

	
	printk(KERN_WARNING "IBM MCA SCSI: Sending abort to device pun=%d, lun=%d.\n", target, cmd->device->lun);
	ldn = get_ldn(shpnt)[target][cmd->device->lun];

	
	if (!ld(shpnt)[ldn].cmd) {
		    return SUCCESS;
	}

	saved_done = cmd->scsi_done;
	cmd->scsi_done = internal_done;
	cmd->SCp.Status = 0;
	last_scsi_command(shpnt)[ldn] = IM_ABORT_IMM_CMD;
	last_scsi_type(shpnt)[ldn] = IM_IMM_CMD;
	imm_command = inl(IM_CMD_REG(shpnt));
	imm_command &= (unsigned long) (0xffff0000);	
	imm_command |= (unsigned long) (IM_ABORT_IMM_CMD);
	
	
	while (1) {
		if (!(inb(IM_STAT_REG(shpnt)) & IM_BUSY))
			break;
	}
	
	outl(imm_command, IM_CMD_REG(shpnt));
	outb(IM_IMM_CMD | ldn, IM_ATTN_REG(shpnt));
#ifdef IM_DEBUG_PROBE
	printk("IBM MCA SCSI: Abort queued to adapter...\n");
#endif
	spin_unlock_irq(shpnt->host_lock);
	while (!cmd->SCp.Status)
		yield();
	spin_lock_irq(shpnt->host_lock);
	cmd->scsi_done = saved_done;
#ifdef IM_DEBUG_PROBE
	printk("IBM MCA SCSI: Abort returned with adapter response...\n");
#endif

	
	if (cmd->result == (DID_ABORT << 16)) 
	{
		cmd->result |= DID_ABORT << 16;
		if (cmd->scsi_done)
			(cmd->scsi_done) (cmd);
		ld(shpnt)[ldn].cmd = NULL;
#ifdef IM_DEBUG_PROBE
		printk("IBM MCA SCSI: Abort finished with success.\n");
#endif
		return SUCCESS;
	} else {
		cmd->result |= DID_NO_CONNECT << 16;
		if (cmd->scsi_done)
			(cmd->scsi_done) (cmd);
		ld(shpnt)[ldn].cmd = NULL;
#ifdef IM_DEBUG_PROBE
		printk("IBM MCA SCSI: Abort failed.\n");
#endif
		return FAILED;
	}
}

static int ibmmca_abort(Scsi_Cmnd * cmd)
{
	struct Scsi_Host *shpnt = cmd->device->host;
	int rc;

	spin_lock_irq(shpnt->host_lock);
	rc = __ibmmca_abort(cmd);
	spin_unlock_irq(shpnt->host_lock);

	return rc;
}

static int __ibmmca_host_reset(Scsi_Cmnd * cmd)
{
	struct Scsi_Host *shpnt;
	Scsi_Cmnd *cmd_aid;
	int ticks, i;
	unsigned long imm_command;

	BUG_ON(cmd == NULL);

	ticks = IM_RESET_DELAY * HZ;
	shpnt = cmd->device->host;

	if (local_checking_phase_flag(shpnt)) {
		printk(KERN_WARNING "IBM MCA SCSI: unable to reset while checking devices.\n");
		return FAILED;
	}

	
	printk("IBM MCA SCSI: resetting all devices.\n");
	reset_status(shpnt) = IM_RESET_IN_PROGRESS;
	last_scsi_command(shpnt)[0xf] = IM_RESET_IMM_CMD;
	last_scsi_type(shpnt)[0xf] = IM_IMM_CMD;
	imm_command = inl(IM_CMD_REG(shpnt));
	imm_command &= (unsigned long) (0xffff0000);	
	imm_command |= (unsigned long) (IM_RESET_IMM_CMD);
	
	while (1) {
		if (!(inb(IM_STAT_REG(shpnt)) & IM_BUSY))
			break;
		spin_unlock_irq(shpnt->host_lock);
		yield();
		spin_lock_irq(shpnt->host_lock);
	}
	
	outl(imm_command, IM_CMD_REG(shpnt));
	outb(IM_IMM_CMD | 0xf, IM_ATTN_REG(shpnt));
	 
	
	while (reset_status(shpnt) == IM_RESET_IN_PROGRESS && --ticks && ((inb(IM_INTR_REG(shpnt)) & 0x8f) != 0x8f)) {
		udelay((1 + 999 / HZ) * 1000);
		barrier();
	}
	
	if (!ticks) {
		printk(KERN_ERR "IBM MCA SCSI: reset did not complete within %d seconds.\n", IM_RESET_DELAY);
		reset_status(shpnt) = IM_RESET_FINISHED_FAIL;
		return FAILED;
	}

	if ((inb(IM_INTR_REG(shpnt)) & 0x8f) == 0x8f) {
		
		if (inb(IM_INTR_REG(shpnt)) == 0xaf)
			reset_status(shpnt) = IM_RESET_FINISHED_OK_NO_INT;
		else if (inb(IM_INTR_REG(shpnt)) == 0xcf)
			reset_status(shpnt) = IM_RESET_FINISHED_FAIL;
		else		
			reset_status(shpnt) = IM_RESET_NOT_IN_PROGRESS_NO_INT;
		outb(IM_EOI | 0xf, IM_ATTN_REG(shpnt));
	}

	
	if (reset_status(shpnt) == IM_RESET_FINISHED_FAIL) {
		printk(KERN_ERR "IBM MCA SCSI: reset failed.\n");
		return FAILED;
	}

	
	printk(KERN_INFO "IBM MCA SCSI: Reset successfully completed.\n");
	for (i = 0; i < MAX_LOG_DEV; i++) {
		cmd_aid = ld(shpnt)[i].cmd;
		if (cmd_aid && cmd_aid->scsi_done) {
			ld(shpnt)[i].cmd = NULL;
			cmd_aid->result = DID_RESET << 16;
		}
	}
	return SUCCESS;
}

static int ibmmca_host_reset(Scsi_Cmnd * cmd)
{
	struct Scsi_Host *shpnt = cmd->device->host;
	int rc;

	spin_lock_irq(shpnt->host_lock);
	rc = __ibmmca_host_reset(cmd);
	spin_unlock_irq(shpnt->host_lock);

	return rc;
}

static int ibmmca_biosparam(struct scsi_device *sdev, struct block_device *bdev, sector_t capacity, int *info)
{
	int size = capacity;
	info[0] = 64;
	info[1] = 32;
	info[2] = size / (info[0] * info[1]);
	if (info[2] >= 1024) {
		info[0] = 128;
		info[1] = 63;
		info[2] = size / (info[0] * info[1]);
		if (info[2] >= 1024) {
			info[0] = 255;
			info[1] = 63;
			info[2] = size / (info[0] * info[1]);
			if (info[2] >= 1024)
				info[2] = 1023;
		}
	}
	return 0;
}

static int ldn_access_load(struct Scsi_Host *shpnt, int ldn)
{
	if (IBM_DS(shpnt).total_accesses == 0)
		return (0);
	if (IBM_DS(shpnt).ldn_access[ldn] == 0)
		return (0);
	return (IBM_DS(shpnt).ldn_access[ldn] * 100) / IBM_DS(shpnt).total_accesses;
}

static int ldn_access_total_read_write(struct Scsi_Host *shpnt)
{
	int a;
	int i;

	a = 0;
	for (i = 0; i <= MAX_LOG_DEV; i++)
		a += IBM_DS(shpnt).ldn_read_access[i] + IBM_DS(shpnt).ldn_write_access[i];
	return (a);
}

static int ldn_access_total_inquiry(struct Scsi_Host *shpnt)
{
	int a;
	int i;

	a = 0;
	for (i = 0; i <= MAX_LOG_DEV; i++)
		a += IBM_DS(shpnt).ldn_inquiry_access[i];
	return (a);
}

static int ldn_access_total_modeselect(struct Scsi_Host *shpnt)
{
	int a;
	int i;

	a = 0;
	for (i = 0; i <= MAX_LOG_DEV; i++)
		a += IBM_DS(shpnt).ldn_modeselect_access[i];
	return (a);
}

static int ibmmca_proc_info(struct Scsi_Host *shpnt, char *buffer, char **start, off_t offset, int length, int inout)
{
	int len = 0;
	int i, id, lun;
	unsigned long flags;
	int max_pun;

	
	spin_lock_irqsave(shpnt->host_lock, flags);	

	max_pun = subsystem_maxid(shpnt);

	len += sprintf(buffer + len, "\n             IBM-SCSI-Subsystem-Linux-Driver, Version %s\n\n\n", IBMMCA_SCSI_DRIVER_VERSION);
	len += sprintf(buffer + len, " SCSI Access-Statistics:\n");
	len += sprintf(buffer + len, "               Device Scanning Order....: %s\n", (ibm_ansi_order) ? "IBM/ANSI" : "New Industry Standard");
#ifdef CONFIG_SCSI_MULTI_LUN
	len += sprintf(buffer + len, "               Multiple LUN probing.....: Yes\n");
#else
	len += sprintf(buffer + len, "               Multiple LUN probing.....: No\n");
#endif
	len += sprintf(buffer + len, "               This Hostnumber..........: %d\n", shpnt->host_no);
	len += sprintf(buffer + len, "               Base I/O-Port............: 0x%x\n", (unsigned int) (IM_CMD_REG(shpnt)));
	len += sprintf(buffer + len, "               (Shared) IRQ.............: %d\n", IM_IRQ);
	len += sprintf(buffer + len, "               Total Interrupts.........: %d\n", IBM_DS(shpnt).total_interrupts);
	len += sprintf(buffer + len, "               Total SCSI Accesses......: %d\n", IBM_DS(shpnt).total_accesses);
	len += sprintf(buffer + len, "               Total short SCBs.........: %d\n", IBM_DS(shpnt).scbs);
	len += sprintf(buffer + len, "               Total long SCBs..........: %d\n", IBM_DS(shpnt).long_scbs);
	len += sprintf(buffer + len, "                 Total SCSI READ/WRITE..: %d\n", ldn_access_total_read_write(shpnt));
	len += sprintf(buffer + len, "                 Total SCSI Inquiries...: %d\n", ldn_access_total_inquiry(shpnt));
	len += sprintf(buffer + len, "                 Total SCSI Modeselects.: %d\n", ldn_access_total_modeselect(shpnt));
	len += sprintf(buffer + len, "                 Total SCSI other cmds..: %d\n", IBM_DS(shpnt).total_accesses - ldn_access_total_read_write(shpnt)
		       - ldn_access_total_modeselect(shpnt)
		       - ldn_access_total_inquiry(shpnt));
	len += sprintf(buffer + len, "               Total SCSI command fails.: %d\n\n", IBM_DS(shpnt).total_errors);
	len += sprintf(buffer + len, " Logical-Device-Number (LDN) Access-Statistics:\n");
	len += sprintf(buffer + len, "         LDN | Accesses [%%] |   READ    |   WRITE   | ASSIGNMENTS\n");
	len += sprintf(buffer + len, "        -----|--------------|-----------|-----------|--------------\n");
	for (i = 0; i <= MAX_LOG_DEV; i++)
		len += sprintf(buffer + len, "         %2X  |    %3d       |  %8d |  %8d | %8d\n", i, ldn_access_load(shpnt, i), IBM_DS(shpnt).ldn_read_access[i], IBM_DS(shpnt).ldn_write_access[i], IBM_DS(shpnt).ldn_assignments[i]);
	len += sprintf(buffer + len, "        -----------------------------------------------------------\n\n");
	len += sprintf(buffer + len, " Dynamical-LDN-Assignment-Statistics:\n");
	len += sprintf(buffer + len, "               Number of physical SCSI-devices..: %d (+ Adapter)\n", IBM_DS(shpnt).total_scsi_devices);
	len += sprintf(buffer + len, "               Dynamical Assignment necessary...: %s\n", IBM_DS(shpnt).dyn_flag ? "Yes" : "No ");
	len += sprintf(buffer + len, "               Next LDN to be assigned..........: 0x%x\n", next_ldn(shpnt));
	len += sprintf(buffer + len, "               Dynamical assignments done yet...: %d\n", IBM_DS(shpnt).dynamical_assignments);
	len += sprintf(buffer + len, "\n Current SCSI-Device-Mapping:\n");
	len += sprintf(buffer + len, "        Physical SCSI-Device Map               Logical SCSI-Device Map\n");
	len += sprintf(buffer + len, "    ID\\LUN  0  1  2  3  4  5  6  7       ID\\LUN  0  1  2  3  4  5  6  7\n");
	for (id = 0; id < max_pun; id++) {
		len += sprintf(buffer + len, "    %2d     ", id);
		for (lun = 0; lun < 8; lun++)
			len += sprintf(buffer + len, "%2s ", ti_p(get_scsi(shpnt)[id][lun]));
		len += sprintf(buffer + len, "      %2d     ", id);
		for (lun = 0; lun < 8; lun++)
			len += sprintf(buffer + len, "%2s ", ti_l(get_ldn(shpnt)[id][lun]));
		len += sprintf(buffer + len, "\n");
	}

	len += sprintf(buffer + len, "(A = IBM-Subsystem, D = Harddisk, T = Tapedrive, P = Processor, W = WORM,\n");
	len += sprintf(buffer + len, " R = CD-ROM, S = Scanner, M = MO-Drive, C = Medium-Changer, + = unprovided LUN,\n");
	len += sprintf(buffer + len, " - = nothing found, nothing assigned or unprobed LUN)\n\n");

	*start = buffer + offset;
	len -= offset;
	if (len > length)
		len = length;
	spin_unlock_irqrestore(shpnt->host_lock, flags);
	return len;
}

static int option_setup(char *str)
{
	int ints[IM_MAX_HOSTS];
	char *cur = str;
	int i = 1;

	while (cur && isdigit(*cur) && i < IM_MAX_HOSTS) {
		ints[i++] = simple_strtoul(cur, NULL, 0);
		if ((cur = strchr(cur, ',')) != NULL)
			cur++;
	}
	ints[0] = i - 1;
	internal_ibmmca_scsi_setup(cur, ints);
	return 1;
}

__setup("ibmmcascsi=", option_setup);

static struct mca_driver ibmmca_driver = {
	.id_table = ibmmca_id_table,
	.driver = {
		.name	= "ibmmca",
		.bus	= &mca_bus_type,
		.probe	= ibmmca_probe,
		.remove	= __devexit_p(ibmmca_remove),
	},
};

static int __init ibmmca_init(void)
{
#ifdef MODULE
	
	if (boot_options)
		option_setup(boot_options);
#endif

	return mca_register_driver_integrated(&ibmmca_driver, MCA_INTEGSCSI);
}

static void __exit ibmmca_exit(void)
{
	mca_unregister_driver(&ibmmca_driver);
}

module_init(ibmmca_init);
module_exit(ibmmca_exit);
