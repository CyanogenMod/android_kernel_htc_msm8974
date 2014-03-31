/*
 *	IDE I/O functions
 *
 *	Basic PIO and command management functionality.
 *
 * This code was split off from ide.c. See ide.c for history and original
 * copyrights.
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
 * For the avoidance of doubt the "preferred form" of this code is one which
 * is in an open non patent encumbered format. Where cryptographic key signing
 * forms part of the process of creating an executable the information
 * including keys needed to generate an equivalently functional executable
 * are deemed to be part of the source code.
 */
 
 
#include <linux/module.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/genhd.h>
#include <linux/blkpg.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/completion.h>
#include <linux/reboot.h>
#include <linux/cdrom.h>
#include <linux/seq_file.h>
#include <linux/device.h>
#include <linux/kmod.h>
#include <linux/scatterlist.h>
#include <linux/bitops.h>

#include <asm/byteorder.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/io.h>

int ide_end_rq(ide_drive_t *drive, struct request *rq, int error,
	       unsigned int nr_bytes)
{
	if ((drive->dev_flags & IDE_DFLAG_DMA_PIO_RETRY) &&
	    drive->retry_pio <= 3) {
		drive->dev_flags &= ~IDE_DFLAG_DMA_PIO_RETRY;
		ide_dma_on(drive);
	}

	return blk_end_request(rq, error, nr_bytes);
}
EXPORT_SYMBOL_GPL(ide_end_rq);

void ide_complete_cmd(ide_drive_t *drive, struct ide_cmd *cmd, u8 stat, u8 err)
{
	const struct ide_tp_ops *tp_ops = drive->hwif->tp_ops;
	struct ide_taskfile *tf = &cmd->tf;
	struct request *rq = cmd->rq;
	u8 tf_cmd = tf->command;

	tf->error = err;
	tf->status = stat;

	if (cmd->ftf_flags & IDE_FTFLAG_IN_DATA) {
		u8 data[2];

		tp_ops->input_data(drive, cmd, data, 2);

		cmd->tf.data  = data[0];
		cmd->hob.data = data[1];
	}

	ide_tf_readback(drive, cmd);

	if ((cmd->tf_flags & IDE_TFLAG_CUSTOM_HANDLER) &&
	    tf_cmd == ATA_CMD_IDLEIMMEDIATE) {
		if (tf->lbal != 0xc4) {
			printk(KERN_ERR "%s: head unload failed!\n",
			       drive->name);
			ide_tf_dump(drive->name, cmd);
		} else
			drive->dev_flags |= IDE_DFLAG_PARKED;
	}

	if (rq && rq->cmd_type == REQ_TYPE_ATA_TASKFILE) {
		struct ide_cmd *orig_cmd = rq->special;

		if (cmd->tf_flags & IDE_TFLAG_DYN)
			kfree(orig_cmd);
		else
			memcpy(orig_cmd, cmd, sizeof(*cmd));
	}
}

int ide_complete_rq(ide_drive_t *drive, int error, unsigned int nr_bytes)
{
	ide_hwif_t *hwif = drive->hwif;
	struct request *rq = hwif->rq;
	int rc;

	if (blk_noretry_request(rq) && error <= 0)
		nr_bytes = blk_rq_sectors(rq) << 9;

	rc = ide_end_rq(drive, rq, error, nr_bytes);
	if (rc == 0)
		hwif->rq = NULL;

	return rc;
}
EXPORT_SYMBOL(ide_complete_rq);

void ide_kill_rq(ide_drive_t *drive, struct request *rq)
{
	u8 drv_req = (rq->cmd_type == REQ_TYPE_SPECIAL) && rq->rq_disk;
	u8 media = drive->media;

	drive->failed_pc = NULL;

	if ((media == ide_floppy || media == ide_tape) && drv_req) {
		rq->errors = 0;
	} else {
		if (media == ide_tape)
			rq->errors = IDE_DRV_ERROR_GENERAL;
		else if (rq->cmd_type != REQ_TYPE_FS && rq->errors == 0)
			rq->errors = -EIO;
	}

	ide_complete_rq(drive, -EIO, blk_rq_bytes(rq));
}

static void ide_tf_set_specify_cmd(ide_drive_t *drive, struct ide_taskfile *tf)
{
	tf->nsect   = drive->sect;
	tf->lbal    = drive->sect;
	tf->lbam    = drive->cyl;
	tf->lbah    = drive->cyl >> 8;
	tf->device  = (drive->head - 1) | drive->select;
	tf->command = ATA_CMD_INIT_DEV_PARAMS;
}

static void ide_tf_set_restore_cmd(ide_drive_t *drive, struct ide_taskfile *tf)
{
	tf->nsect   = drive->sect;
	tf->command = ATA_CMD_RESTORE;
}

static void ide_tf_set_setmult_cmd(ide_drive_t *drive, struct ide_taskfile *tf)
{
	tf->nsect   = drive->mult_req;
	tf->command = ATA_CMD_SET_MULTI;
}


static ide_startstop_t do_special(ide_drive_t *drive)
{
	struct ide_cmd cmd;

#ifdef DEBUG
	printk(KERN_DEBUG "%s: %s: 0x%02x\n", drive->name, __func__,
		drive->special_flags);
#endif
	if (drive->media != ide_disk) {
		drive->special_flags = 0;
		drive->mult_req = 0;
		return ide_stopped;
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.protocol = ATA_PROT_NODATA;

	if (drive->special_flags & IDE_SFLAG_SET_GEOMETRY) {
		drive->special_flags &= ~IDE_SFLAG_SET_GEOMETRY;
		ide_tf_set_specify_cmd(drive, &cmd.tf);
	} else if (drive->special_flags & IDE_SFLAG_RECALIBRATE) {
		drive->special_flags &= ~IDE_SFLAG_RECALIBRATE;
		ide_tf_set_restore_cmd(drive, &cmd.tf);
	} else if (drive->special_flags & IDE_SFLAG_SET_MULTMODE) {
		drive->special_flags &= ~IDE_SFLAG_SET_MULTMODE;
		ide_tf_set_setmult_cmd(drive, &cmd.tf);
	} else
		BUG();

	cmd.valid.out.tf = IDE_VALID_OUT_TF | IDE_VALID_DEVICE;
	cmd.valid.in.tf  = IDE_VALID_IN_TF  | IDE_VALID_DEVICE;
	cmd.tf_flags = IDE_TFLAG_CUSTOM_HANDLER;

	do_rw_taskfile(drive, &cmd);

	return ide_started;
}

void ide_map_sg(ide_drive_t *drive, struct ide_cmd *cmd)
{
	ide_hwif_t *hwif = drive->hwif;
	struct scatterlist *sg = hwif->sg_table;
	struct request *rq = cmd->rq;

	cmd->sg_nents = blk_rq_map_sg(drive->queue, rq, sg);
}
EXPORT_SYMBOL_GPL(ide_map_sg);

void ide_init_sg_cmd(struct ide_cmd *cmd, unsigned int nr_bytes)
{
	cmd->nbytes = cmd->nleft = nr_bytes;
	cmd->cursg_ofs = 0;
	cmd->cursg = NULL;
}
EXPORT_SYMBOL_GPL(ide_init_sg_cmd);


static ide_startstop_t execute_drive_cmd (ide_drive_t *drive,
		struct request *rq)
{
	struct ide_cmd *cmd = rq->special;

	if (cmd) {
		if (cmd->protocol == ATA_PROT_PIO) {
			ide_init_sg_cmd(cmd, blk_rq_sectors(rq) << 9);
			ide_map_sg(drive, cmd);
		}

		return do_rw_taskfile(drive, cmd);
	}

#ifdef DEBUG
 	printk("%s: DRIVE_CMD (null)\n", drive->name);
#endif
	rq->errors = 0;
	ide_complete_rq(drive, 0, blk_rq_bytes(rq));

 	return ide_stopped;
}

static ide_startstop_t ide_special_rq(ide_drive_t *drive, struct request *rq)
{
	u8 cmd = rq->cmd[0];

	switch (cmd) {
	case REQ_PARK_HEADS:
	case REQ_UNPARK_HEADS:
		return ide_do_park_unpark(drive, rq);
	case REQ_DEVSET_EXEC:
		return ide_do_devset(drive, rq);
	case REQ_DRIVE_RESET:
		return ide_do_reset(drive);
	default:
		BUG();
	}
}

 
static ide_startstop_t start_request (ide_drive_t *drive, struct request *rq)
{
	ide_startstop_t startstop;

	BUG_ON(!(rq->cmd_flags & REQ_STARTED));

#ifdef DEBUG
	printk("%s: start_request: current=0x%08lx\n",
		drive->hwif->name, (unsigned long) rq);
#endif

	
	if (drive->max_failures && (drive->failures > drive->max_failures)) {
		rq->cmd_flags |= REQ_FAILED;
		goto kill_rq;
	}

	if (blk_pm_request(rq))
		ide_check_pm_state(drive, rq);

	drive->hwif->tp_ops->dev_select(drive);
	if (ide_wait_stat(&startstop, drive, drive->ready_stat,
			  ATA_BUSY | ATA_DRQ, WAIT_READY)) {
		printk(KERN_ERR "%s: drive not ready for command\n", drive->name);
		return startstop;
	}

	if (drive->special_flags == 0) {
		struct ide_driver *drv;

		if (drive->current_speed == 0xff)
			ide_config_drive_speed(drive, drive->desired_speed);

		if (rq->cmd_type == REQ_TYPE_ATA_TASKFILE)
			return execute_drive_cmd(drive, rq);
		else if (blk_pm_request(rq)) {
			struct request_pm_state *pm = rq->special;
#ifdef DEBUG_PM
			printk("%s: start_power_step(step: %d)\n",
				drive->name, pm->pm_step);
#endif
			startstop = ide_start_power_step(drive, rq);
			if (startstop == ide_stopped &&
			    pm->pm_step == IDE_PM_COMPLETED)
				ide_complete_pm_rq(drive, rq);
			return startstop;
		} else if (!rq->rq_disk && rq->cmd_type == REQ_TYPE_SPECIAL)
			return ide_special_rq(drive, rq);

		drv = *(struct ide_driver **)rq->rq_disk->private_data;

		return drv->do_request(drive, rq, blk_rq_pos(rq));
	}
	return do_special(drive);
kill_rq:
	ide_kill_rq(drive, rq);
	return ide_stopped;
}

 
void ide_stall_queue (ide_drive_t *drive, unsigned long timeout)
{
	if (timeout > WAIT_WORSTCASE)
		timeout = WAIT_WORSTCASE;
	drive->sleep = timeout + jiffies;
	drive->dev_flags |= IDE_DFLAG_SLEEPING;
}
EXPORT_SYMBOL(ide_stall_queue);

static inline int ide_lock_port(ide_hwif_t *hwif)
{
	if (hwif->busy)
		return 1;

	hwif->busy = 1;

	return 0;
}

static inline void ide_unlock_port(ide_hwif_t *hwif)
{
	hwif->busy = 0;
}

static inline int ide_lock_host(struct ide_host *host, ide_hwif_t *hwif)
{
	int rc = 0;

	if (host->host_flags & IDE_HFLAG_SERIALIZE) {
		rc = test_and_set_bit_lock(IDE_HOST_BUSY, &host->host_busy);
		if (rc == 0) {
			if (host->get_lock)
				host->get_lock(ide_intr, hwif);
		}
	}
	return rc;
}

static inline void ide_unlock_host(struct ide_host *host)
{
	if (host->host_flags & IDE_HFLAG_SERIALIZE) {
		if (host->release_lock)
			host->release_lock();
		clear_bit_unlock(IDE_HOST_BUSY, &host->host_busy);
	}
}

static void __ide_requeue_and_plug(struct request_queue *q, struct request *rq)
{
	if (rq)
		blk_requeue_request(q, rq);
	if (rq || blk_peek_request(q)) {
		
		blk_delay_queue(q, 3);
	}
}

void ide_requeue_and_plug(ide_drive_t *drive, struct request *rq)
{
	struct request_queue *q = drive->queue;
	unsigned long flags;

	spin_lock_irqsave(q->queue_lock, flags);
	__ide_requeue_and_plug(q, rq);
	spin_unlock_irqrestore(q->queue_lock, flags);
}

void do_ide_request(struct request_queue *q)
{
	ide_drive_t	*drive = q->queuedata;
	ide_hwif_t	*hwif = drive->hwif;
	struct ide_host *host = hwif->host;
	struct request	*rq = NULL;
	ide_startstop_t	startstop;
	unsigned long queue_run_ms = 3; 

	spin_unlock_irq(q->queue_lock);

	
	might_sleep();

	if (ide_lock_host(host, hwif))
		goto plug_device_2;

	spin_lock_irq(&hwif->lock);

	if (!ide_lock_port(hwif)) {
		ide_hwif_t *prev_port;

		WARN_ON_ONCE(hwif->rq);
repeat:
		prev_port = hwif->host->cur_port;
		if (drive->dev_flags & IDE_DFLAG_SLEEPING &&
		    time_after(drive->sleep, jiffies)) {
			unsigned long left = jiffies - drive->sleep;

			queue_run_ms = jiffies_to_msecs(left + 1);
			ide_unlock_port(hwif);
			goto plug_device;
		}

		if ((hwif->host->host_flags & IDE_HFLAG_SERIALIZE) &&
		    hwif != prev_port) {
			ide_drive_t *cur_dev =
				prev_port ? prev_port->cur_dev : NULL;

			if (cur_dev &&
			    (cur_dev->dev_flags & IDE_DFLAG_NIEN_QUIRK) == 0)
				prev_port->tp_ops->write_devctl(prev_port,
								ATA_NIEN |
								ATA_DEVCTL_OBS);

			hwif->host->cur_port = hwif;
		}
		hwif->cur_dev = drive;
		drive->dev_flags &= ~(IDE_DFLAG_SLEEPING | IDE_DFLAG_PARKED);

		spin_unlock_irq(&hwif->lock);
		spin_lock_irq(q->queue_lock);
		if (!rq)
			rq = blk_fetch_request(drive->queue);

		spin_unlock_irq(q->queue_lock);
		spin_lock_irq(&hwif->lock);

		if (!rq) {
			ide_unlock_port(hwif);
			goto out;
		}

		if ((drive->dev_flags & IDE_DFLAG_BLOCKED) &&
		    blk_pm_request(rq) == 0 &&
		    (rq->cmd_flags & REQ_PREEMPT) == 0) {
			
			ide_unlock_port(hwif);
			goto plug_device;
		}

		hwif->rq = rq;

		spin_unlock_irq(&hwif->lock);
		startstop = start_request(drive, rq);
		spin_lock_irq(&hwif->lock);

		if (startstop == ide_stopped) {
			rq = hwif->rq;
			hwif->rq = NULL;
			goto repeat;
		}
	} else
		goto plug_device;
out:
	spin_unlock_irq(&hwif->lock);
	if (rq == NULL)
		ide_unlock_host(host);
	spin_lock_irq(q->queue_lock);
	return;

plug_device:
	spin_unlock_irq(&hwif->lock);
	ide_unlock_host(host);
plug_device_2:
	spin_lock_irq(q->queue_lock);
	__ide_requeue_and_plug(q, rq);
}

static int drive_is_ready(ide_drive_t *drive)
{
	ide_hwif_t *hwif = drive->hwif;
	u8 stat = 0;

	if (drive->waiting_for_dma)
		return hwif->dma_ops->dma_test_irq(drive);

	if (hwif->io_ports.ctl_addr &&
	    (hwif->host_flags & IDE_HFLAG_BROKEN_ALTSTATUS) == 0)
		stat = hwif->tp_ops->read_altstatus(hwif);
	else
		
		stat = hwif->tp_ops->read_status(hwif);

	if (stat & ATA_BUSY)
		
		return 0;

	
	return 1;
}

 
void ide_timer_expiry (unsigned long data)
{
	ide_hwif_t	*hwif = (ide_hwif_t *)data;
	ide_drive_t	*uninitialized_var(drive);
	ide_handler_t	*handler;
	unsigned long	flags;
	int		wait = -1;
	int		plug_device = 0;
	struct request	*uninitialized_var(rq_in_flight);

	spin_lock_irqsave(&hwif->lock, flags);

	handler = hwif->handler;

	if (handler == NULL || hwif->req_gen != hwif->req_gen_timer) {
	} else {
		ide_expiry_t *expiry = hwif->expiry;
		ide_startstop_t startstop = ide_stopped;

		drive = hwif->cur_dev;

		if (expiry) {
			wait = expiry(drive);
			if (wait > 0) { 
				
				hwif->timer.expires = jiffies + wait;
				hwif->req_gen_timer = hwif->req_gen;
				add_timer(&hwif->timer);
				spin_unlock_irqrestore(&hwif->lock, flags);
				return;
			}
		}
		hwif->handler = NULL;
		hwif->expiry = NULL;
		spin_unlock(&hwif->lock);
		
		disable_irq(hwif->irq);
		
		local_irq_disable();
		if (hwif->polling) {
			startstop = handler(drive);
		} else if (drive_is_ready(drive)) {
			if (drive->waiting_for_dma)
				hwif->dma_ops->dma_lost_irq(drive);
			if (hwif->port_ops && hwif->port_ops->clear_irq)
				hwif->port_ops->clear_irq(drive);

			printk(KERN_WARNING "%s: lost interrupt\n",
				drive->name);
			startstop = handler(drive);
		} else {
			if (drive->waiting_for_dma)
				startstop = ide_dma_timeout_retry(drive, wait);
			else
				startstop = ide_error(drive, "irq timeout",
					hwif->tp_ops->read_status(hwif));
		}
		spin_lock_irq(&hwif->lock);
		enable_irq(hwif->irq);
		if (startstop == ide_stopped && hwif->polling == 0) {
			rq_in_flight = hwif->rq;
			hwif->rq = NULL;
			ide_unlock_port(hwif);
			plug_device = 1;
		}
	}
	spin_unlock_irqrestore(&hwif->lock, flags);

	if (plug_device) {
		ide_unlock_host(hwif->host);
		ide_requeue_and_plug(drive, rq_in_flight);
	}
}


static void unexpected_intr(int irq, ide_hwif_t *hwif)
{
	u8 stat = hwif->tp_ops->read_status(hwif);

	if (!OK_STAT(stat, ATA_DRDY, BAD_STAT)) {
		
		static unsigned long last_msgtime, count;
		++count;

		if (time_after(jiffies, last_msgtime + HZ)) {
			last_msgtime = jiffies;
			printk(KERN_ERR "%s: unexpected interrupt, "
				"status=0x%02x, count=%ld\n",
				hwif->name, stat, count);
		}
	}
}


irqreturn_t ide_intr (int irq, void *dev_id)
{
	ide_hwif_t *hwif = (ide_hwif_t *)dev_id;
	struct ide_host *host = hwif->host;
	ide_drive_t *uninitialized_var(drive);
	ide_handler_t *handler;
	unsigned long flags;
	ide_startstop_t startstop;
	irqreturn_t irq_ret = IRQ_NONE;
	int plug_device = 0;
	struct request *uninitialized_var(rq_in_flight);

	if (host->host_flags & IDE_HFLAG_SERIALIZE) {
		if (hwif != host->cur_port)
			goto out_early;
	}

	spin_lock_irqsave(&hwif->lock, flags);

	if (hwif->port_ops && hwif->port_ops->test_irq &&
	    hwif->port_ops->test_irq(hwif) == 0)
		goto out;

	handler = hwif->handler;

	if (handler == NULL || hwif->polling) {
		if ((host->irq_flags & IRQF_SHARED) == 0) {
			unexpected_intr(irq, hwif);
		} else {
			(void)hwif->tp_ops->read_status(hwif);
		}
		goto out;
	}

	drive = hwif->cur_dev;

	if (!drive_is_ready(drive))
		goto out;

	hwif->handler = NULL;
	hwif->expiry = NULL;
	hwif->req_gen++;
	del_timer(&hwif->timer);
	spin_unlock(&hwif->lock);

	if (hwif->port_ops && hwif->port_ops->clear_irq)
		hwif->port_ops->clear_irq(drive);

	if (drive->dev_flags & IDE_DFLAG_UNMASK)
		local_irq_enable_in_hardirq();

	
	startstop = handler(drive);

	spin_lock_irq(&hwif->lock);
	if (startstop == ide_stopped && hwif->polling == 0) {
		BUG_ON(hwif->handler);
		rq_in_flight = hwif->rq;
		hwif->rq = NULL;
		ide_unlock_port(hwif);
		plug_device = 1;
	}
	irq_ret = IRQ_HANDLED;
out:
	spin_unlock_irqrestore(&hwif->lock, flags);
out_early:
	if (plug_device) {
		ide_unlock_host(hwif->host);
		ide_requeue_and_plug(drive, rq_in_flight);
	}

	return irq_ret;
}
EXPORT_SYMBOL_GPL(ide_intr);

void ide_pad_transfer(ide_drive_t *drive, int write, int len)
{
	ide_hwif_t *hwif = drive->hwif;
	u8 buf[4] = { 0 };

	while (len > 0) {
		if (write)
			hwif->tp_ops->output_data(drive, NULL, buf, min(4, len));
		else
			hwif->tp_ops->input_data(drive, NULL, buf, min(4, len));
		len -= 4;
	}
}
EXPORT_SYMBOL_GPL(ide_pad_transfer);
