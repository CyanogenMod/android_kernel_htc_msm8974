/*
 *    Disk Array driver for HP Smart Array SAS controllers
 *    Copyright 2000, 2009 Hewlett-Packard Development Company, L.P.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; version 2 of the License.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *    NON INFRINGEMENT.  See the GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *    Questions/Comments/Bugfixes to iss_storagedev@hp.com
 *
 */
#ifndef HPSA_H
#define HPSA_H

#include <scsi/scsicam.h>

#define IO_OK		0
#define IO_ERROR	1

struct ctlr_info;

struct access_method {
	void (*submit_command)(struct ctlr_info *h,
		struct CommandList *c);
	void (*set_intr_mask)(struct ctlr_info *h, unsigned long val);
	unsigned long (*fifo_full)(struct ctlr_info *h);
	bool (*intr_pending)(struct ctlr_info *h);
	unsigned long (*command_completed)(struct ctlr_info *h);
};

struct hpsa_scsi_dev_t {
	int devtype;
	int bus, target, lun;		
	unsigned char scsi3addr[8];	
#define RAID_CTLR_LUNID "\0\0\0\0\0\0\0\0"
	unsigned char device_id[16];    
	unsigned char vendor[8];        
	unsigned char model[16];        
	unsigned char raid_level;	
};

struct ctlr_info {
	int	ctlr;
	char	devname[8];
	char    *product_name;
	struct pci_dev *pdev;
	u32	board_id;
	void __iomem *vaddr;
	unsigned long paddr;
	int 	nr_cmds; 
	struct CfgTable __iomem *cfgtable;
	int	interrupts_enabled;
	int	major;
	int 	max_commands;
	int	commands_outstanding;
	int 	max_outstanding; 
	int	usage_count;  
#	define PERF_MODE_INT	0
#	define DOORBELL_INT	1
#	define SIMPLE_MODE_INT	2
#	define MEMQ_MODE_INT	3
	unsigned int intr[4];
	unsigned int msix_vector;
	unsigned int msi_vector;
	int intr_mode; 
	struct access_method access;

	
	struct list_head reqQ;
	struct list_head cmpQ;
	unsigned int Qdepth;
	unsigned int maxQsinceinit;
	unsigned int maxSG;
	spinlock_t lock;
	int maxsgentries;
	u8 max_cmd_sg_entries;
	int chainsize;
	struct SGDescriptor **cmd_sg_list;

	
	struct CommandList 	*cmd_pool;
	dma_addr_t		cmd_pool_dhandle;
	struct ErrorInfo 	*errinfo_pool;
	dma_addr_t		errinfo_pool_dhandle;
	unsigned long  		*cmd_pool_bits;
	int			nr_allocs;
	int			nr_frees;
	int			scan_finished;
	spinlock_t		scan_lock;
	wait_queue_head_t	scan_wait_queue;

	struct Scsi_Host *scsi_host;
	spinlock_t devlock; 
	int ndevices; 
	struct hpsa_scsi_dev_t *dev[HPSA_MAX_DEVICES];
	u32 trans_support;
	u32 trans_offset;
	struct TransTable_struct *transtable;
	unsigned long transMethod;

	u64 *reply_pool;
	dma_addr_t reply_pool_dhandle;
	u64 *reply_pool_head;
	size_t reply_pool_size;
	unsigned char reply_pool_wraparound;
	u32 *blockFetchTable;
	unsigned char *hba_inquiry_data;
	u64 last_intr_timestamp;
	u32 last_heartbeat;
	u64 last_heartbeat_timestamp;
	u32 lockup_detected;
	struct list_head lockup_list;
};
#define HPSA_ABORT_MSG 0
#define HPSA_DEVICE_RESET_MSG 1
#define HPSA_RESET_TYPE_CONTROLLER 0x00
#define HPSA_RESET_TYPE_BUS 0x01
#define HPSA_RESET_TYPE_TARGET 0x03
#define HPSA_RESET_TYPE_LUN 0x04
#define HPSA_MSG_SEND_RETRY_LIMIT 10
#define HPSA_MSG_SEND_RETRY_INTERVAL_MSECS (10000)

#define HPSA_MAX_POLL_TIME_SECS (20)

#define HPSA_TUR_RETRY_LIMIT (20)
#define HPSA_MAX_WAIT_INTERVAL_SECS (30)

#define HPSA_BOARD_READY_WAIT_SECS (120)
#define HPSA_BOARD_NOT_READY_WAIT_SECS (100)
#define HPSA_BOARD_READY_POLL_INTERVAL_MSECS (100)
#define HPSA_BOARD_READY_POLL_INTERVAL \
	((HPSA_BOARD_READY_POLL_INTERVAL_MSECS * HZ) / 1000)
#define HPSA_BOARD_READY_ITERATIONS \
	((HPSA_BOARD_READY_WAIT_SECS * 1000) / \
		HPSA_BOARD_READY_POLL_INTERVAL_MSECS)
#define HPSA_BOARD_NOT_READY_ITERATIONS \
	((HPSA_BOARD_NOT_READY_WAIT_SECS * 1000) / \
		HPSA_BOARD_READY_POLL_INTERVAL_MSECS)
#define HPSA_POST_RESET_PAUSE_MSECS (3000)
#define HPSA_POST_RESET_NOOP_RETRIES (12)

#define SA5_DOORBELL	0x20
#define SA5_REQUEST_PORT_OFFSET	0x40
#define SA5_REPLY_INTR_MASK_OFFSET	0x34
#define SA5_REPLY_PORT_OFFSET		0x44
#define SA5_INTR_STATUS		0x30
#define SA5_SCRATCHPAD_OFFSET	0xB0

#define SA5_CTCFG_OFFSET	0xB4
#define SA5_CTMEM_OFFSET	0xB8

#define SA5_INTR_OFF		0x08
#define SA5B_INTR_OFF		0x04
#define SA5_INTR_PENDING	0x08
#define SA5B_INTR_PENDING	0x04
#define FIFO_EMPTY		0xffffffff
#define HPSA_FIRMWARE_READY	0xffff0000 

#define HPSA_ERROR_BIT		0x02

#define SA5_PERF_INTR_PENDING   0x04
#define SA5_PERF_INTR_OFF       0x05
#define SA5_OUTDB_STATUS_PERF_BIT       0x01
#define SA5_OUTDB_CLEAR_PERF_BIT        0x01
#define SA5_OUTDB_CLEAR         0xA0
#define SA5_OUTDB_CLEAR_PERF_BIT        0x01
#define SA5_OUTDB_STATUS        0x9C


#define HPSA_INTR_ON 	1
#define HPSA_INTR_OFF	0
static void SA5_submit_command(struct ctlr_info *h,
	struct CommandList *c)
{
	dev_dbg(&h->pdev->dev, "Sending %x, tag = %x\n", c->busaddr,
		c->Header.Tag.lower);
	writel(c->busaddr, h->vaddr + SA5_REQUEST_PORT_OFFSET);
	(void) readl(h->vaddr + SA5_SCRATCHPAD_OFFSET);
	h->commands_outstanding++;
	if (h->commands_outstanding > h->max_outstanding)
		h->max_outstanding = h->commands_outstanding;
}

static void SA5_intr_mask(struct ctlr_info *h, unsigned long val)
{
	if (val) { 
		h->interrupts_enabled = 1;
		writel(0, h->vaddr + SA5_REPLY_INTR_MASK_OFFSET);
		(void) readl(h->vaddr + SA5_REPLY_INTR_MASK_OFFSET);
	} else { 
		h->interrupts_enabled = 0;
		writel(SA5_INTR_OFF,
			h->vaddr + SA5_REPLY_INTR_MASK_OFFSET);
		(void) readl(h->vaddr + SA5_REPLY_INTR_MASK_OFFSET);
	}
}

static void SA5_performant_intr_mask(struct ctlr_info *h, unsigned long val)
{
	if (val) { 
		h->interrupts_enabled = 1;
		writel(0, h->vaddr + SA5_REPLY_INTR_MASK_OFFSET);
		(void) readl(h->vaddr + SA5_REPLY_INTR_MASK_OFFSET);
	} else {
		h->interrupts_enabled = 0;
		writel(SA5_PERF_INTR_OFF,
			h->vaddr + SA5_REPLY_INTR_MASK_OFFSET);
		(void) readl(h->vaddr + SA5_REPLY_INTR_MASK_OFFSET);
	}
}

static unsigned long SA5_performant_completed(struct ctlr_info *h)
{
	unsigned long register_value = FIFO_EMPTY;

	register_value = readl(h->vaddr + SA5_OUTDB_STATUS);
	
	if (!(h->msi_vector || h->msix_vector)) {
		writel(SA5_OUTDB_CLEAR_PERF_BIT, h->vaddr + SA5_OUTDB_CLEAR);
		register_value = readl(h->vaddr + SA5_OUTDB_STATUS);
	}

	if ((*(h->reply_pool_head) & 1) == (h->reply_pool_wraparound)) {
		register_value = *(h->reply_pool_head);
		(h->reply_pool_head)++;
		h->commands_outstanding--;
	} else {
		register_value = FIFO_EMPTY;
	}
	
	if (h->reply_pool_head == (h->reply_pool + h->max_commands)) {
		h->reply_pool_head = h->reply_pool;
		h->reply_pool_wraparound ^= 1;
	}

	return register_value;
}

static unsigned long SA5_fifo_full(struct ctlr_info *h)
{
	if (h->commands_outstanding >= h->max_commands)
		return 1;
	else
		return 0;

}
static unsigned long SA5_completed(struct ctlr_info *h)
{
	unsigned long register_value
		= readl(h->vaddr + SA5_REPLY_PORT_OFFSET);

	if (register_value != FIFO_EMPTY)
		h->commands_outstanding--;

#ifdef HPSA_DEBUG
	if (register_value != FIFO_EMPTY)
		dev_dbg(&h->pdev->dev, "Read %lx back from board\n",
			register_value);
	else
		dev_dbg(&h->pdev->dev, "FIFO Empty read\n");
#endif

	return register_value;
}
static bool SA5_intr_pending(struct ctlr_info *h)
{
	unsigned long register_value  =
		readl(h->vaddr + SA5_INTR_STATUS);
	dev_dbg(&h->pdev->dev, "intr_pending %lx\n", register_value);
	return register_value & SA5_INTR_PENDING;
}

static bool SA5_performant_intr_pending(struct ctlr_info *h)
{
	unsigned long register_value = readl(h->vaddr + SA5_INTR_STATUS);

	if (!register_value)
		return false;

	if (h->msi_vector || h->msix_vector)
		return true;

	
	register_value = readl(h->vaddr + SA5_OUTDB_STATUS);
	return register_value & SA5_OUTDB_STATUS_PERF_BIT;
}

static struct access_method SA5_access = {
	SA5_submit_command,
	SA5_intr_mask,
	SA5_fifo_full,
	SA5_intr_pending,
	SA5_completed,
};

static struct access_method SA5_performant_access = {
	SA5_submit_command,
	SA5_performant_intr_mask,
	SA5_fifo_full,
	SA5_performant_intr_pending,
	SA5_performant_completed,
};

struct board_type {
	u32	board_id;
	char	*product_name;
	struct access_method *access;
};

#endif 

