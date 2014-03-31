#ifndef __SEP_DEV_H__
#define __SEP_DEV_H__

/*
 *
 *  sep_dev.h - Security Processor Device Structures
 *
 *  Copyright(c) 2009-2011 Intel Corporation. All rights reserved.
 *  Contributions(c) 2009-2011 Discretix. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  this program; if not, write to the Free Software Foundation, Inc., 59
 *  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *  CONTACTS:
 *
 *  Mark Allyn		mark.a.allyn@intel.com
 *  Jayant Mangalampalli jayant.mangalampalli@intel.com
 *
 *  CHANGES
 *  2010.09.14  upgrade to Medfield
 *  2011.02.22  enable kernel crypto
 */

struct sep_device {
	
	struct pci_dev *pdev;

	
	struct cdev sep_cdev;

	
	struct miscdevice miscdev_sep;

	
	dev_t sep_devno;
	
	spinlock_t snd_rply_lck;
	
	struct semaphore sep_doublebuf;

	
	u32 pid_doing_transaction;
	unsigned long in_use_flags;

	dma_addr_t shared_bus;
	size_t shared_size;
	void *shared_addr;

	
	dma_addr_t reg_physical_addr;
	dma_addr_t reg_physical_end;
	void __iomem *reg_addr;

	
	wait_queue_head_t event_interrupt;
	wait_queue_head_t event_transactions;

	struct list_head sep_queue_status;
	u32 sep_queue_num;
	spinlock_t sep_queue_lock;

	
	u32 in_use;

	
	u32 power_save_setup;

	
	u32 power_state;

	unsigned long send_ct;
	
	unsigned long reply_ct;

	
	u32 in_kernel; 
	struct tasklet_struct	finish_tasklet;
	enum type_of_request current_request;
	enum hash_stage	current_hash_stage;
	struct ahash_request	*current_hash_req;
	struct ablkcipher_request *current_cypher_req;
	struct this_task_ctx *ta_ctx;
	struct workqueue_struct	*workqueue;
};

extern struct sep_device *sep_dev;

struct sep_msgarea_hdr {
	u32 reserved[2];
	u32 token;
	u32 msg_len;
	u32 opcode;
};

struct sep_queue_data {
	u32 opcode;
	u32 size;
	s32 pid;
	u8 name[TASK_COMM_LEN];
};

struct sep_queue_info {
	struct list_head list;
	struct sep_queue_data data;
};

static inline void sep_write_reg(struct sep_device *dev, int reg, u32 value)
{
	void __iomem *addr = dev->reg_addr + reg;
	writel(value, addr);
}

static inline u32 sep_read_reg(struct sep_device *dev, int reg)
{
	void __iomem *addr = dev->reg_addr + reg;
	return readl(addr);
}

static inline void sep_wait_sram_write(struct sep_device *dev)
{
	u32 reg_val;
	do {
		reg_val = sep_read_reg(dev, HW_SRAM_DATA_READY_REG_ADDR);
	} while (!(reg_val & 1));
}


#endif
