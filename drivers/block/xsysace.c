/*
 * Xilinx SystemACE device driver
 *
 * Copyright 2007 Secret Lab Technologies Ltd.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */


#undef DEBUG

#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/mutex.h>
#include <linux/ata.h>
#include <linux/hdreg.h>
#include <linux/platform_device.h>
#if defined(CONFIG_OF)
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#endif

MODULE_AUTHOR("Grant Likely <grant.likely@secretlab.ca>");
MODULE_DESCRIPTION("Xilinx SystemACE device driver");
MODULE_LICENSE("GPL");

#define ACE_BUSMODE (0x00)

#define ACE_STATUS (0x04)
#define ACE_STATUS_CFGLOCK      (0x00000001)
#define ACE_STATUS_MPULOCK      (0x00000002)
#define ACE_STATUS_CFGERROR     (0x00000004)	
#define ACE_STATUS_CFCERROR     (0x00000008)	
#define ACE_STATUS_CFDETECT     (0x00000010)
#define ACE_STATUS_DATABUFRDY   (0x00000020)
#define ACE_STATUS_DATABUFMODE  (0x00000040)
#define ACE_STATUS_CFGDONE      (0x00000080)
#define ACE_STATUS_RDYFORCFCMD  (0x00000100)
#define ACE_STATUS_CFGMODEPIN   (0x00000200)
#define ACE_STATUS_CFGADDR_MASK (0x0000e000)
#define ACE_STATUS_CFBSY        (0x00020000)
#define ACE_STATUS_CFRDY        (0x00040000)
#define ACE_STATUS_CFDWF        (0x00080000)
#define ACE_STATUS_CFDSC        (0x00100000)
#define ACE_STATUS_CFDRQ        (0x00200000)
#define ACE_STATUS_CFCORR       (0x00400000)
#define ACE_STATUS_CFERR        (0x00800000)

#define ACE_ERROR (0x08)
#define ACE_CFGLBA (0x0c)
#define ACE_MPULBA (0x10)

#define ACE_SECCNTCMD (0x14)
#define ACE_SECCNTCMD_RESET      (0x0100)
#define ACE_SECCNTCMD_IDENTIFY   (0x0200)
#define ACE_SECCNTCMD_READ_DATA  (0x0300)
#define ACE_SECCNTCMD_WRITE_DATA (0x0400)
#define ACE_SECCNTCMD_ABORT      (0x0600)

#define ACE_VERSION (0x16)
#define ACE_VERSION_REVISION_MASK (0x00FF)
#define ACE_VERSION_MINOR_MASK    (0x0F00)
#define ACE_VERSION_MAJOR_MASK    (0xF000)

#define ACE_CTRL (0x18)
#define ACE_CTRL_FORCELOCKREQ   (0x0001)
#define ACE_CTRL_LOCKREQ        (0x0002)
#define ACE_CTRL_FORCECFGADDR   (0x0004)
#define ACE_CTRL_FORCECFGMODE   (0x0008)
#define ACE_CTRL_CFGMODE        (0x0010)
#define ACE_CTRL_CFGSTART       (0x0020)
#define ACE_CTRL_CFGSEL         (0x0040)
#define ACE_CTRL_CFGRESET       (0x0080)
#define ACE_CTRL_DATABUFRDYIRQ  (0x0100)
#define ACE_CTRL_ERRORIRQ       (0x0200)
#define ACE_CTRL_CFGDONEIRQ     (0x0400)
#define ACE_CTRL_RESETIRQ       (0x0800)
#define ACE_CTRL_CFGPROG        (0x1000)
#define ACE_CTRL_CFGADDR_MASK   (0xe000)

#define ACE_FATSTAT (0x1c)

#define ACE_NUM_MINORS 16
#define ACE_SECTOR_SIZE (512)
#define ACE_FIFO_SIZE (32)
#define ACE_BUF_PER_SECTOR (ACE_SECTOR_SIZE / ACE_FIFO_SIZE)

#define ACE_BUS_WIDTH_8  0
#define ACE_BUS_WIDTH_16 1

struct ace_reg_ops;

struct ace_device {
	
	int id;
	int media_change;
	int users;
	struct list_head list;

	
	struct tasklet_struct fsm_tasklet;
	uint fsm_task;		
	uint fsm_state;		
	uint fsm_continue_flag;	
	uint fsm_iter_num;
	struct timer_list stall_timer;

	
	struct request *req;	
	void *data_ptr;		
	int data_count;		
	int data_result;	

	int id_req_count;	
	int id_result;
	struct completion id_completion;	
	int in_irq;

	
	resource_size_t physaddr;
	void __iomem *baseaddr;
	int irq;
	int bus_width;		
	struct ace_reg_ops *reg_ops;
	int lock_count;

	
	spinlock_t lock;
	struct device *dev;
	struct request_queue *queue;
	struct gendisk *gd;

	
	u16 cf_id[ATA_ID_WORDS];
};

static DEFINE_MUTEX(xsysace_mutex);
static int ace_major;


struct ace_reg_ops {
	u16(*in) (struct ace_device * ace, int reg);
	void (*out) (struct ace_device * ace, int reg, u16 val);
	void (*datain) (struct ace_device * ace);
	void (*dataout) (struct ace_device * ace);
};

static u16 ace_in_8(struct ace_device *ace, int reg)
{
	void __iomem *r = ace->baseaddr + reg;
	return in_8(r) | (in_8(r + 1) << 8);
}

static void ace_out_8(struct ace_device *ace, int reg, u16 val)
{
	void __iomem *r = ace->baseaddr + reg;
	out_8(r, val);
	out_8(r + 1, val >> 8);
}

static void ace_datain_8(struct ace_device *ace)
{
	void __iomem *r = ace->baseaddr + 0x40;
	u8 *dst = ace->data_ptr;
	int i = ACE_FIFO_SIZE;
	while (i--)
		*dst++ = in_8(r++);
	ace->data_ptr = dst;
}

static void ace_dataout_8(struct ace_device *ace)
{
	void __iomem *r = ace->baseaddr + 0x40;
	u8 *src = ace->data_ptr;
	int i = ACE_FIFO_SIZE;
	while (i--)
		out_8(r++, *src++);
	ace->data_ptr = src;
}

static struct ace_reg_ops ace_reg_8_ops = {
	.in = ace_in_8,
	.out = ace_out_8,
	.datain = ace_datain_8,
	.dataout = ace_dataout_8,
};

static u16 ace_in_be16(struct ace_device *ace, int reg)
{
	return in_be16(ace->baseaddr + reg);
}

static void ace_out_be16(struct ace_device *ace, int reg, u16 val)
{
	out_be16(ace->baseaddr + reg, val);
}

static void ace_datain_be16(struct ace_device *ace)
{
	int i = ACE_FIFO_SIZE / 2;
	u16 *dst = ace->data_ptr;
	while (i--)
		*dst++ = in_le16(ace->baseaddr + 0x40);
	ace->data_ptr = dst;
}

static void ace_dataout_be16(struct ace_device *ace)
{
	int i = ACE_FIFO_SIZE / 2;
	u16 *src = ace->data_ptr;
	while (i--)
		out_le16(ace->baseaddr + 0x40, *src++);
	ace->data_ptr = src;
}

static u16 ace_in_le16(struct ace_device *ace, int reg)
{
	return in_le16(ace->baseaddr + reg);
}

static void ace_out_le16(struct ace_device *ace, int reg, u16 val)
{
	out_le16(ace->baseaddr + reg, val);
}

static void ace_datain_le16(struct ace_device *ace)
{
	int i = ACE_FIFO_SIZE / 2;
	u16 *dst = ace->data_ptr;
	while (i--)
		*dst++ = in_be16(ace->baseaddr + 0x40);
	ace->data_ptr = dst;
}

static void ace_dataout_le16(struct ace_device *ace)
{
	int i = ACE_FIFO_SIZE / 2;
	u16 *src = ace->data_ptr;
	while (i--)
		out_be16(ace->baseaddr + 0x40, *src++);
	ace->data_ptr = src;
}

static struct ace_reg_ops ace_reg_be16_ops = {
	.in = ace_in_be16,
	.out = ace_out_be16,
	.datain = ace_datain_be16,
	.dataout = ace_dataout_be16,
};

static struct ace_reg_ops ace_reg_le16_ops = {
	.in = ace_in_le16,
	.out = ace_out_le16,
	.datain = ace_datain_le16,
	.dataout = ace_dataout_le16,
};

static inline u16 ace_in(struct ace_device *ace, int reg)
{
	return ace->reg_ops->in(ace, reg);
}

static inline u32 ace_in32(struct ace_device *ace, int reg)
{
	return ace_in(ace, reg) | (ace_in(ace, reg + 2) << 16);
}

static inline void ace_out(struct ace_device *ace, int reg, u16 val)
{
	ace->reg_ops->out(ace, reg, val);
}

static inline void ace_out32(struct ace_device *ace, int reg, u32 val)
{
	ace_out(ace, reg, val);
	ace_out(ace, reg + 2, val >> 16);
}


#if defined(DEBUG)
static void ace_dump_mem(void *base, int len)
{
	const char *ptr = base;
	int i, j;

	for (i = 0; i < len; i += 16) {
		printk(KERN_INFO "%.8x:", i);
		for (j = 0; j < 16; j++) {
			if (!(j % 4))
				printk(" ");
			printk("%.2x", ptr[i + j]);
		}
		printk(" ");
		for (j = 0; j < 16; j++)
			printk("%c", isprint(ptr[i + j]) ? ptr[i + j] : '.');
		printk("\n");
	}
}
#else
static inline void ace_dump_mem(void *base, int len)
{
}
#endif

static void ace_dump_regs(struct ace_device *ace)
{
	dev_info(ace->dev,
		 "    ctrl:  %.8x  seccnt/cmd: %.4x      ver:%.4x\n"
		 "    status:%.8x  mpu_lba:%.8x  busmode:%4x\n"
		 "    error: %.8x  cfg_lba:%.8x  fatstat:%.4x\n",
		 ace_in32(ace, ACE_CTRL),
		 ace_in(ace, ACE_SECCNTCMD),
		 ace_in(ace, ACE_VERSION),
		 ace_in32(ace, ACE_STATUS),
		 ace_in32(ace, ACE_MPULBA),
		 ace_in(ace, ACE_BUSMODE),
		 ace_in32(ace, ACE_ERROR),
		 ace_in32(ace, ACE_CFGLBA), ace_in(ace, ACE_FATSTAT));
}

void ace_fix_driveid(u16 *id)
{
#if defined(__BIG_ENDIAN)
	int i;

	
	for (i = 0; i < ATA_ID_WORDS; i++, id++)
		*id = le16_to_cpu(*id);
#endif
}


#define ACE_TASK_IDLE      0
#define ACE_TASK_IDENTIFY  1
#define ACE_TASK_READ      2
#define ACE_TASK_WRITE     3
#define ACE_FSM_NUM_TASKS  4

#define ACE_FSM_STATE_IDLE               0
#define ACE_FSM_STATE_REQ_LOCK           1
#define ACE_FSM_STATE_WAIT_LOCK          2
#define ACE_FSM_STATE_WAIT_CFREADY       3
#define ACE_FSM_STATE_IDENTIFY_PREPARE   4
#define ACE_FSM_STATE_IDENTIFY_TRANSFER  5
#define ACE_FSM_STATE_IDENTIFY_COMPLETE  6
#define ACE_FSM_STATE_REQ_PREPARE        7
#define ACE_FSM_STATE_REQ_TRANSFER       8
#define ACE_FSM_STATE_REQ_COMPLETE       9
#define ACE_FSM_STATE_ERROR             10
#define ACE_FSM_NUM_STATES              11

static inline void ace_fsm_yield(struct ace_device *ace)
{
	dev_dbg(ace->dev, "ace_fsm_yield()\n");
	tasklet_schedule(&ace->fsm_tasklet);
	ace->fsm_continue_flag = 0;
}

static inline void ace_fsm_yieldirq(struct ace_device *ace)
{
	dev_dbg(ace->dev, "ace_fsm_yieldirq()\n");

	if (!ace->irq)
		
		tasklet_schedule(&ace->fsm_tasklet);
	ace->fsm_continue_flag = 0;
}

struct request *ace_get_next_request(struct request_queue * q)
{
	struct request *req;

	while ((req = blk_peek_request(q)) != NULL) {
		if (req->cmd_type == REQ_TYPE_FS)
			break;
		blk_start_request(req);
		__blk_end_request_all(req, -EIO);
	}
	return req;
}

static void ace_fsm_dostate(struct ace_device *ace)
{
	struct request *req;
	u32 status;
	u16 val;
	int count;

#if defined(DEBUG)
	dev_dbg(ace->dev, "fsm_state=%i, id_req_count=%i\n",
		ace->fsm_state, ace->id_req_count);
#endif

	status = ace_in32(ace, ACE_STATUS);
	if ((status & ACE_STATUS_CFDETECT) == 0) {
		ace->fsm_state = ACE_FSM_STATE_IDLE;
		ace->media_change = 1;
		set_capacity(ace->gd, 0);
		dev_info(ace->dev, "No CF in slot\n");

		
		if (ace->req) {
			__blk_end_request_all(ace->req, -EIO);
			ace->req = NULL;
		}
		while ((req = blk_fetch_request(ace->queue)) != NULL)
			__blk_end_request_all(req, -EIO);

		
		ace->fsm_state = ACE_FSM_STATE_IDLE;
		ace->id_result = -EIO;
		while (ace->id_req_count) {
			complete(&ace->id_completion);
			ace->id_req_count--;
		}
	}

	switch (ace->fsm_state) {
	case ACE_FSM_STATE_IDLE:
		
		if (ace->id_req_count || ace_get_next_request(ace->queue)) {
			ace->fsm_iter_num++;
			ace->fsm_state = ACE_FSM_STATE_REQ_LOCK;
			mod_timer(&ace->stall_timer, jiffies + HZ);
			if (!timer_pending(&ace->stall_timer))
				add_timer(&ace->stall_timer);
			break;
		}
		del_timer(&ace->stall_timer);
		ace->fsm_continue_flag = 0;
		break;

	case ACE_FSM_STATE_REQ_LOCK:
		if (ace_in(ace, ACE_STATUS) & ACE_STATUS_MPULOCK) {
			
			ace->fsm_state = ACE_FSM_STATE_WAIT_CFREADY;
			break;
		}

		
		val = ace_in(ace, ACE_CTRL);
		ace_out(ace, ACE_CTRL, val | ACE_CTRL_LOCKREQ);
		ace->fsm_state = ACE_FSM_STATE_WAIT_LOCK;
		break;

	case ACE_FSM_STATE_WAIT_LOCK:
		if (ace_in(ace, ACE_STATUS) & ACE_STATUS_MPULOCK) {
			
			ace->fsm_state = ACE_FSM_STATE_WAIT_CFREADY;
			break;
		}

		
		ace_fsm_yield(ace);
		break;

	case ACE_FSM_STATE_WAIT_CFREADY:
		status = ace_in32(ace, ACE_STATUS);
		if (!(status & ACE_STATUS_RDYFORCFCMD) ||
		    (status & ACE_STATUS_CFBSY)) {
			
			ace_fsm_yield(ace);
			break;
		}

		
		if (ace->id_req_count)
			ace->fsm_state = ACE_FSM_STATE_IDENTIFY_PREPARE;
		else
			ace->fsm_state = ACE_FSM_STATE_REQ_PREPARE;
		break;

	case ACE_FSM_STATE_IDENTIFY_PREPARE:
		
		ace->fsm_task = ACE_TASK_IDENTIFY;
		ace->data_ptr = ace->cf_id;
		ace->data_count = ACE_BUF_PER_SECTOR;
		ace_out(ace, ACE_SECCNTCMD, ACE_SECCNTCMD_IDENTIFY);

		
		val = ace_in(ace, ACE_CTRL);
		ace_out(ace, ACE_CTRL, val | ACE_CTRL_CFGRESET);

		ace->fsm_state = ACE_FSM_STATE_IDENTIFY_TRANSFER;
		ace_fsm_yieldirq(ace);
		break;

	case ACE_FSM_STATE_IDENTIFY_TRANSFER:
		
		status = ace_in32(ace, ACE_STATUS);
		if (status & ACE_STATUS_CFBSY) {
			dev_dbg(ace->dev, "CFBSY set; t=%i iter=%i dc=%i\n",
				ace->fsm_task, ace->fsm_iter_num,
				ace->data_count);
			ace_fsm_yield(ace);
			break;
		}
		if (!(status & ACE_STATUS_DATABUFRDY)) {
			ace_fsm_yield(ace);
			break;
		}

		
		ace->reg_ops->datain(ace);
		ace->data_count--;

		
		if (ace->data_count != 0) {
			ace_fsm_yieldirq(ace);
			break;
		}

		
		dev_dbg(ace->dev, "identify finished\n");
		ace->fsm_state = ACE_FSM_STATE_IDENTIFY_COMPLETE;
		break;

	case ACE_FSM_STATE_IDENTIFY_COMPLETE:
		ace_fix_driveid(ace->cf_id);
		ace_dump_mem(ace->cf_id, 512);	

		if (ace->data_result) {
			
			ace->media_change = 1;
			set_capacity(ace->gd, 0);
			dev_err(ace->dev, "error fetching CF id (%i)\n",
				ace->data_result);
		} else {
			ace->media_change = 0;

			
			set_capacity(ace->gd,
				ata_id_u32(ace->cf_id, ATA_ID_LBA_CAPACITY));
			dev_info(ace->dev, "capacity: %i sectors\n",
				ata_id_u32(ace->cf_id, ATA_ID_LBA_CAPACITY));
		}

		
		ace->fsm_state = ACE_FSM_STATE_IDLE;
		ace->id_result = ace->data_result;
		while (ace->id_req_count) {
			complete(&ace->id_completion);
			ace->id_req_count--;
		}
		break;

	case ACE_FSM_STATE_REQ_PREPARE:
		req = ace_get_next_request(ace->queue);
		if (!req) {
			ace->fsm_state = ACE_FSM_STATE_IDLE;
			break;
		}
		blk_start_request(req);

		
		dev_dbg(ace->dev,
			"request: sec=%llx hcnt=%x, ccnt=%x, dir=%i\n",
			(unsigned long long)blk_rq_pos(req),
			blk_rq_sectors(req), blk_rq_cur_sectors(req),
			rq_data_dir(req));

		ace->req = req;
		ace->data_ptr = req->buffer;
		ace->data_count = blk_rq_cur_sectors(req) * ACE_BUF_PER_SECTOR;
		ace_out32(ace, ACE_MPULBA, blk_rq_pos(req) & 0x0FFFFFFF);

		count = blk_rq_sectors(req);
		if (rq_data_dir(req)) {
			
			dev_dbg(ace->dev, "write data\n");
			ace->fsm_task = ACE_TASK_WRITE;
			ace_out(ace, ACE_SECCNTCMD,
				count | ACE_SECCNTCMD_WRITE_DATA);
		} else {
			
			dev_dbg(ace->dev, "read data\n");
			ace->fsm_task = ACE_TASK_READ;
			ace_out(ace, ACE_SECCNTCMD,
				count | ACE_SECCNTCMD_READ_DATA);
		}

		
		val = ace_in(ace, ACE_CTRL);
		ace_out(ace, ACE_CTRL, val | ACE_CTRL_CFGRESET);

		ace->fsm_state = ACE_FSM_STATE_REQ_TRANSFER;
		if (ace->fsm_task == ACE_TASK_READ)
			ace_fsm_yieldirq(ace);	
		break;

	case ACE_FSM_STATE_REQ_TRANSFER:
		
		status = ace_in32(ace, ACE_STATUS);
		if (status & ACE_STATUS_CFBSY) {
			dev_dbg(ace->dev,
				"CFBSY set; t=%i iter=%i c=%i dc=%i irq=%i\n",
				ace->fsm_task, ace->fsm_iter_num,
				blk_rq_cur_sectors(ace->req) * 16,
				ace->data_count, ace->in_irq);
			ace_fsm_yield(ace);	
			break;
		}
		if (!(status & ACE_STATUS_DATABUFRDY)) {
			dev_dbg(ace->dev,
				"DATABUF not set; t=%i iter=%i c=%i dc=%i irq=%i\n",
				ace->fsm_task, ace->fsm_iter_num,
				blk_rq_cur_sectors(ace->req) * 16,
				ace->data_count, ace->in_irq);
			ace_fsm_yieldirq(ace);
			break;
		}

		
		if (ace->fsm_task == ACE_TASK_WRITE)
			ace->reg_ops->dataout(ace);
		else
			ace->reg_ops->datain(ace);
		ace->data_count--;

		
		if (ace->data_count != 0) {
			ace_fsm_yieldirq(ace);
			break;
		}

		
		if (__blk_end_request_cur(ace->req, 0)) {
			ace->data_ptr = ace->req->buffer;
			ace->data_count = blk_rq_cur_sectors(ace->req) * 16;
			ace_fsm_yieldirq(ace);
			break;
		}

		ace->fsm_state = ACE_FSM_STATE_REQ_COMPLETE;
		break;

	case ACE_FSM_STATE_REQ_COMPLETE:
		ace->req = NULL;

		
		ace->fsm_state = ACE_FSM_STATE_IDLE;
		break;

	default:
		ace->fsm_state = ACE_FSM_STATE_IDLE;
		break;
	}
}

static void ace_fsm_tasklet(unsigned long data)
{
	struct ace_device *ace = (void *)data;
	unsigned long flags;

	spin_lock_irqsave(&ace->lock, flags);

	
	ace->fsm_continue_flag = 1;
	while (ace->fsm_continue_flag)
		ace_fsm_dostate(ace);

	spin_unlock_irqrestore(&ace->lock, flags);
}

static void ace_stall_timer(unsigned long data)
{
	struct ace_device *ace = (void *)data;
	unsigned long flags;

	dev_warn(ace->dev,
		 "kicking stalled fsm; state=%i task=%i iter=%i dc=%i\n",
		 ace->fsm_state, ace->fsm_task, ace->fsm_iter_num,
		 ace->data_count);
	spin_lock_irqsave(&ace->lock, flags);

	mod_timer(&ace->stall_timer, jiffies + HZ);

	
	ace->fsm_continue_flag = 1;
	while (ace->fsm_continue_flag)
		ace_fsm_dostate(ace);

	spin_unlock_irqrestore(&ace->lock, flags);
}

static int ace_interrupt_checkstate(struct ace_device *ace)
{
	u32 sreg = ace_in32(ace, ACE_STATUS);
	u16 creg = ace_in(ace, ACE_CTRL);

	
	if ((sreg & (ACE_STATUS_CFGERROR | ACE_STATUS_CFCERROR)) &&
	    (creg & ACE_CTRL_ERRORIRQ)) {
		dev_err(ace->dev, "transfer failure\n");
		ace_dump_regs(ace);
		return -EIO;
	}

	return 0;
}

static irqreturn_t ace_interrupt(int irq, void *dev_id)
{
	u16 creg;
	struct ace_device *ace = dev_id;

	
	spin_lock(&ace->lock);
	ace->in_irq = 1;

	
	creg = ace_in(ace, ACE_CTRL);
	ace_out(ace, ACE_CTRL, creg | ACE_CTRL_RESETIRQ);
	ace_out(ace, ACE_CTRL, creg);

	
	if (ace_interrupt_checkstate(ace))
		ace->data_result = -EIO;

	if (ace->fsm_task == 0) {
		dev_err(ace->dev,
			"spurious irq; stat=%.8x ctrl=%.8x cmd=%.4x\n",
			ace_in32(ace, ACE_STATUS), ace_in32(ace, ACE_CTRL),
			ace_in(ace, ACE_SECCNTCMD));
		dev_err(ace->dev, "fsm_task=%i fsm_state=%i data_count=%i\n",
			ace->fsm_task, ace->fsm_state, ace->data_count);
	}

	
	ace->fsm_continue_flag = 1;
	while (ace->fsm_continue_flag)
		ace_fsm_dostate(ace);

	
	ace->in_irq = 0;
	spin_unlock(&ace->lock);

	return IRQ_HANDLED;
}

static void ace_request(struct request_queue * q)
{
	struct request *req;
	struct ace_device *ace;

	req = ace_get_next_request(q);

	if (req) {
		ace = req->rq_disk->private_data;
		tasklet_schedule(&ace->fsm_tasklet);
	}
}

static unsigned int ace_check_events(struct gendisk *gd, unsigned int clearing)
{
	struct ace_device *ace = gd->private_data;
	dev_dbg(ace->dev, "ace_check_events(): %i\n", ace->media_change);

	return ace->media_change ? DISK_EVENT_MEDIA_CHANGE : 0;
}

static int ace_revalidate_disk(struct gendisk *gd)
{
	struct ace_device *ace = gd->private_data;
	unsigned long flags;

	dev_dbg(ace->dev, "ace_revalidate_disk()\n");

	if (ace->media_change) {
		dev_dbg(ace->dev, "requesting cf id and scheduling tasklet\n");

		spin_lock_irqsave(&ace->lock, flags);
		ace->id_req_count++;
		spin_unlock_irqrestore(&ace->lock, flags);

		tasklet_schedule(&ace->fsm_tasklet);
		wait_for_completion(&ace->id_completion);
	}

	dev_dbg(ace->dev, "revalidate complete\n");
	return ace->id_result;
}

static int ace_open(struct block_device *bdev, fmode_t mode)
{
	struct ace_device *ace = bdev->bd_disk->private_data;
	unsigned long flags;

	dev_dbg(ace->dev, "ace_open() users=%i\n", ace->users + 1);

	mutex_lock(&xsysace_mutex);
	spin_lock_irqsave(&ace->lock, flags);
	ace->users++;
	spin_unlock_irqrestore(&ace->lock, flags);

	check_disk_change(bdev);
	mutex_unlock(&xsysace_mutex);

	return 0;
}

static int ace_release(struct gendisk *disk, fmode_t mode)
{
	struct ace_device *ace = disk->private_data;
	unsigned long flags;
	u16 val;

	dev_dbg(ace->dev, "ace_release() users=%i\n", ace->users - 1);

	mutex_lock(&xsysace_mutex);
	spin_lock_irqsave(&ace->lock, flags);
	ace->users--;
	if (ace->users == 0) {
		val = ace_in(ace, ACE_CTRL);
		ace_out(ace, ACE_CTRL, val & ~ACE_CTRL_LOCKREQ);
	}
	spin_unlock_irqrestore(&ace->lock, flags);
	mutex_unlock(&xsysace_mutex);
	return 0;
}

static int ace_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	struct ace_device *ace = bdev->bd_disk->private_data;
	u16 *cf_id = ace->cf_id;

	dev_dbg(ace->dev, "ace_getgeo()\n");

	geo->heads	= cf_id[ATA_ID_HEADS];
	geo->sectors	= cf_id[ATA_ID_SECTORS];
	geo->cylinders	= cf_id[ATA_ID_CYLS];

	return 0;
}

static const struct block_device_operations ace_fops = {
	.owner = THIS_MODULE,
	.open = ace_open,
	.release = ace_release,
	.check_events = ace_check_events,
	.revalidate_disk = ace_revalidate_disk,
	.getgeo = ace_getgeo,
};

static int __devinit ace_setup(struct ace_device *ace)
{
	u16 version;
	u16 val;
	int rc;

	dev_dbg(ace->dev, "ace_setup(ace=0x%p)\n", ace);
	dev_dbg(ace->dev, "physaddr=0x%llx irq=%i\n",
		(unsigned long long)ace->physaddr, ace->irq);

	spin_lock_init(&ace->lock);
	init_completion(&ace->id_completion);

	ace->baseaddr = ioremap(ace->physaddr, 0x80);
	if (!ace->baseaddr)
		goto err_ioremap;

	tasklet_init(&ace->fsm_tasklet, ace_fsm_tasklet, (unsigned long)ace);
	setup_timer(&ace->stall_timer, ace_stall_timer, (unsigned long)ace);

	ace->queue = blk_init_queue(ace_request, &ace->lock);
	if (ace->queue == NULL)
		goto err_blk_initq;
	blk_queue_logical_block_size(ace->queue, 512);

	ace->gd = alloc_disk(ACE_NUM_MINORS);
	if (!ace->gd)
		goto err_alloc_disk;

	ace->gd->major = ace_major;
	ace->gd->first_minor = ace->id * ACE_NUM_MINORS;
	ace->gd->fops = &ace_fops;
	ace->gd->queue = ace->queue;
	ace->gd->private_data = ace;
	snprintf(ace->gd->disk_name, 32, "xs%c", ace->id + 'a');

	
	if (ace->bus_width == ACE_BUS_WIDTH_16) {
		
		ace_out_le16(ace, ACE_BUSMODE, 0x0101);

		
		if (ace_in_le16(ace, ACE_BUSMODE) == 0x0001)
			ace->reg_ops = &ace_reg_le16_ops;
		else
			ace->reg_ops = &ace_reg_be16_ops;
	} else {
		ace_out_8(ace, ACE_BUSMODE, 0x00);
		ace->reg_ops = &ace_reg_8_ops;
	}

	
	version = ace_in(ace, ACE_VERSION);
	if ((version == 0) || (version == 0xFFFF))
		goto err_read;

	
	ace_out(ace, ACE_CTRL, ACE_CTRL_FORCECFGMODE |
		ACE_CTRL_DATABUFRDYIRQ | ACE_CTRL_ERRORIRQ);

	
	if (ace->irq) {
		rc = request_irq(ace->irq, ace_interrupt, 0, "systemace", ace);
		if (rc) {
			
			dev_err(ace->dev, "request_irq failed\n");
			ace->irq = 0;
		}
	}

	
	val = ace_in(ace, ACE_CTRL);
	val |= ACE_CTRL_DATABUFRDYIRQ | ACE_CTRL_ERRORIRQ;
	ace_out(ace, ACE_CTRL, val);

	
	dev_info(ace->dev, "Xilinx SystemACE revision %i.%i.%i\n",
		 (version >> 12) & 0xf, (version >> 8) & 0x0f, version & 0xff);
	dev_dbg(ace->dev, "physaddr 0x%llx, mapped to 0x%p, irq=%i\n",
		(unsigned long long) ace->physaddr, ace->baseaddr, ace->irq);

	ace->media_change = 1;
	ace_revalidate_disk(ace->gd);

	
	add_disk(ace->gd);

	return 0;

err_read:
	put_disk(ace->gd);
err_alloc_disk:
	blk_cleanup_queue(ace->queue);
err_blk_initq:
	iounmap(ace->baseaddr);
err_ioremap:
	dev_info(ace->dev, "xsysace: error initializing device at 0x%llx\n",
		 (unsigned long long) ace->physaddr);
	return -ENOMEM;
}

static void __devexit ace_teardown(struct ace_device *ace)
{
	if (ace->gd) {
		del_gendisk(ace->gd);
		put_disk(ace->gd);
	}

	if (ace->queue)
		blk_cleanup_queue(ace->queue);

	tasklet_kill(&ace->fsm_tasklet);

	if (ace->irq)
		free_irq(ace->irq, ace);

	iounmap(ace->baseaddr);
}

static int __devinit
ace_alloc(struct device *dev, int id, resource_size_t physaddr,
	  int irq, int bus_width)
{
	struct ace_device *ace;
	int rc;
	dev_dbg(dev, "ace_alloc(%p)\n", dev);

	if (!physaddr) {
		rc = -ENODEV;
		goto err_noreg;
	}

	
	ace = kzalloc(sizeof(struct ace_device), GFP_KERNEL);
	if (!ace) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	ace->dev = dev;
	ace->id = id;
	ace->physaddr = physaddr;
	ace->irq = irq;
	ace->bus_width = bus_width;

	
	rc = ace_setup(ace);
	if (rc)
		goto err_setup;

	dev_set_drvdata(dev, ace);
	return 0;

err_setup:
	dev_set_drvdata(dev, NULL);
	kfree(ace);
err_alloc:
err_noreg:
	dev_err(dev, "could not initialize device, err=%i\n", rc);
	return rc;
}

static void __devexit ace_free(struct device *dev)
{
	struct ace_device *ace = dev_get_drvdata(dev);
	dev_dbg(dev, "ace_free(%p)\n", dev);

	if (ace) {
		ace_teardown(ace);
		dev_set_drvdata(dev, NULL);
		kfree(ace);
	}
}


static int __devinit ace_probe(struct platform_device *dev)
{
	resource_size_t physaddr = 0;
	int bus_width = ACE_BUS_WIDTH_16; 
	u32 id = dev->id;
	int irq = 0;
	int i;

	dev_dbg(&dev->dev, "ace_probe(%p)\n", dev);

	
	of_property_read_u32(dev->dev.of_node, "port-number", &id);
	if (id < 0)
		id = 0;
	if (of_find_property(dev->dev.of_node, "8-bit", NULL))
		bus_width = ACE_BUS_WIDTH_8;

	for (i = 0; i < dev->num_resources; i++) {
		if (dev->resource[i].flags & IORESOURCE_MEM)
			physaddr = dev->resource[i].start;
		if (dev->resource[i].flags & IORESOURCE_IRQ)
			irq = dev->resource[i].start;
	}

	
	return ace_alloc(&dev->dev, id, physaddr, irq, bus_width);
}

static int __devexit ace_remove(struct platform_device *dev)
{
	ace_free(&dev->dev);
	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id ace_of_match[] __devinitconst = {
	{ .compatible = "xlnx,opb-sysace-1.00.b", },
	{ .compatible = "xlnx,opb-sysace-1.00.c", },
	{ .compatible = "xlnx,xps-sysace-1.00.a", },
	{ .compatible = "xlnx,sysace", },
	{},
};
MODULE_DEVICE_TABLE(of, ace_of_match);
#else 
#define ace_of_match NULL
#endif 

static struct platform_driver ace_platform_driver = {
	.probe = ace_probe,
	.remove = __devexit_p(ace_remove),
	.driver = {
		.owner = THIS_MODULE,
		.name = "xsysace",
		.of_match_table = ace_of_match,
	},
};

static int __init ace_init(void)
{
	int rc;

	ace_major = register_blkdev(ace_major, "xsysace");
	if (ace_major <= 0) {
		rc = -ENOMEM;
		goto err_blk;
	}

	rc = platform_driver_register(&ace_platform_driver);
	if (rc)
		goto err_plat;

	pr_info("Xilinx SystemACE device driver, major=%i\n", ace_major);
	return 0;

err_plat:
	unregister_blkdev(ace_major, "xsysace");
err_blk:
	printk(KERN_ERR "xsysace: registration failed; err=%i\n", rc);
	return rc;
}
module_init(ace_init);

static void __exit ace_exit(void)
{
	pr_debug("Unregistering Xilinx SystemACE driver\n");
	platform_driver_unregister(&ace_platform_driver);
	unregister_blkdev(ace_major, "xsysace");
}
module_exit(ace_exit);
