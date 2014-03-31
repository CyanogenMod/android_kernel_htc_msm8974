
#include <linux/kernel.h>
#include <linux/cdrom.h>
#include <linux/delay.h>
#include <linux/export.h>
#include <linux/ide.h>
#include <linux/scatterlist.h>
#include <linux/gfp.h>

#include <scsi/scsi.h>

#define DRV_NAME "ide-atapi"
#define PFX DRV_NAME ": "

#ifdef DEBUG
#define debug_log(fmt, args...) \
	printk(KERN_INFO "ide: " fmt, ## args)
#else
#define debug_log(fmt, args...) do {} while (0)
#endif

#define ATAPI_MIN_CDB_BYTES	12

static inline int dev_is_idecd(ide_drive_t *drive)
{
	return drive->media == ide_cdrom || drive->media == ide_optical;
}

int ide_check_atapi_device(ide_drive_t *drive, const char *s)
{
	u16 *id = drive->id;
	u8 gcw[2], protocol, device_type, removable, drq_type, packet_size;

	*((u16 *)&gcw) = id[ATA_ID_CONFIG];

	protocol    = (gcw[1] & 0xC0) >> 6;
	device_type =  gcw[1] & 0x1F;
	removable   = (gcw[0] & 0x80) >> 7;
	drq_type    = (gcw[0] & 0x60) >> 5;
	packet_size =  gcw[0] & 0x03;

#ifdef CONFIG_PPC
	
	if (drive->media == ide_floppy && device_type == 5 &&
	    !strstr((char *)&id[ATA_ID_PROD], "CD-ROM") &&
	    strstr((char *)&id[ATA_ID_PROD], "ZIP"))
		device_type = 0;
#endif

	if (protocol != 2)
		printk(KERN_ERR "%s: %s: protocol (0x%02x) is not ATAPI\n",
			s, drive->name, protocol);
	else if ((drive->media == ide_floppy && device_type != 0) ||
		 (drive->media == ide_tape && device_type != 1))
		printk(KERN_ERR "%s: %s: invalid device type (0x%02x)\n",
			s, drive->name, device_type);
	else if (removable == 0)
		printk(KERN_ERR "%s: %s: the removable flag is not set\n",
			s, drive->name);
	else if (drive->media == ide_floppy && drq_type == 3)
		printk(KERN_ERR "%s: %s: sorry, DRQ type (0x%02x) not "
			"supported\n", s, drive->name, drq_type);
	else if (packet_size != 0)
		printk(KERN_ERR "%s: %s: packet size (0x%02x) is not 12 "
			"bytes\n", s, drive->name, packet_size);
	else
		return 1;
	return 0;
}
EXPORT_SYMBOL_GPL(ide_check_atapi_device);

void ide_init_pc(struct ide_atapi_pc *pc)
{
	memset(pc, 0, sizeof(*pc));
}
EXPORT_SYMBOL_GPL(ide_init_pc);

int ide_queue_pc_tail(ide_drive_t *drive, struct gendisk *disk,
		      struct ide_atapi_pc *pc, void *buf, unsigned int bufflen)
{
	struct request *rq;
	int error;

	rq = blk_get_request(drive->queue, READ, __GFP_WAIT);
	rq->cmd_type = REQ_TYPE_SPECIAL;
	rq->special = (char *)pc;

	if (buf && bufflen) {
		error = blk_rq_map_kern(drive->queue, rq, buf, bufflen,
					GFP_NOIO);
		if (error)
			goto put_req;
	}

	memcpy(rq->cmd, pc->c, 12);
	if (drive->media == ide_tape)
		rq->cmd[13] = REQ_IDETAPE_PC1;
	error = blk_execute_rq(drive->queue, disk, rq, 0);
put_req:
	blk_put_request(rq);
	return error;
}
EXPORT_SYMBOL_GPL(ide_queue_pc_tail);

int ide_do_test_unit_ready(ide_drive_t *drive, struct gendisk *disk)
{
	struct ide_atapi_pc pc;

	ide_init_pc(&pc);
	pc.c[0] = TEST_UNIT_READY;

	return ide_queue_pc_tail(drive, disk, &pc, NULL, 0);
}
EXPORT_SYMBOL_GPL(ide_do_test_unit_ready);

int ide_do_start_stop(ide_drive_t *drive, struct gendisk *disk, int start)
{
	struct ide_atapi_pc pc;

	ide_init_pc(&pc);
	pc.c[0] = START_STOP;
	pc.c[4] = start;

	if (drive->media == ide_tape)
		pc.flags |= PC_FLAG_WAIT_FOR_DSC;

	return ide_queue_pc_tail(drive, disk, &pc, NULL, 0);
}
EXPORT_SYMBOL_GPL(ide_do_start_stop);

int ide_set_media_lock(ide_drive_t *drive, struct gendisk *disk, int on)
{
	struct ide_atapi_pc pc;

	if ((drive->dev_flags & IDE_DFLAG_DOORLOCKING) == 0)
		return 0;

	ide_init_pc(&pc);
	pc.c[0] = ALLOW_MEDIUM_REMOVAL;
	pc.c[4] = on;

	return ide_queue_pc_tail(drive, disk, &pc, NULL, 0);
}
EXPORT_SYMBOL_GPL(ide_set_media_lock);

void ide_create_request_sense_cmd(ide_drive_t *drive, struct ide_atapi_pc *pc)
{
	ide_init_pc(pc);
	pc->c[0] = REQUEST_SENSE;
	if (drive->media == ide_floppy) {
		pc->c[4] = 255;
		pc->req_xfer = 18;
	} else {
		pc->c[4] = 20;
		pc->req_xfer = 20;
	}
}
EXPORT_SYMBOL_GPL(ide_create_request_sense_cmd);

void ide_prep_sense(ide_drive_t *drive, struct request *rq)
{
	struct request_sense *sense = &drive->sense_data;
	struct request *sense_rq = &drive->sense_rq;
	unsigned int cmd_len, sense_len;
	int err;

	switch (drive->media) {
	case ide_floppy:
		cmd_len = 255;
		sense_len = 18;
		break;
	case ide_tape:
		cmd_len = 20;
		sense_len = 20;
		break;
	default:
		cmd_len = 18;
		sense_len = 18;
	}

	BUG_ON(sense_len > sizeof(*sense));

	if (rq->cmd_type == REQ_TYPE_SENSE || drive->sense_rq_armed)
		return;

	memset(sense, 0, sizeof(*sense));

	blk_rq_init(rq->q, sense_rq);

	err = blk_rq_map_kern(drive->queue, sense_rq, sense, sense_len,
			      GFP_NOIO);
	if (unlikely(err)) {
		if (printk_ratelimit())
			printk(KERN_WARNING PFX "%s: failed to map sense "
					    "buffer\n", drive->name);
		return;
	}

	sense_rq->rq_disk = rq->rq_disk;
	sense_rq->cmd[0] = GPCMD_REQUEST_SENSE;
	sense_rq->cmd[4] = cmd_len;
	sense_rq->cmd_type = REQ_TYPE_SENSE;
	sense_rq->cmd_flags |= REQ_PREEMPT;

	if (drive->media == ide_tape)
		sense_rq->cmd[13] = REQ_IDETAPE_PC1;

	drive->sense_rq_armed = true;
}
EXPORT_SYMBOL_GPL(ide_prep_sense);

int ide_queue_sense_rq(ide_drive_t *drive, void *special)
{
	
	if (!drive->sense_rq_armed) {
		printk(KERN_WARNING PFX "%s: error queuing a sense request\n",
		       drive->name);
		return -ENOMEM;
	}

	drive->sense_rq.special = special;
	drive->sense_rq_armed = false;

	drive->hwif->rq = NULL;

	elv_add_request(drive->queue, &drive->sense_rq, ELEVATOR_INSERT_FRONT);
	return 0;
}
EXPORT_SYMBOL_GPL(ide_queue_sense_rq);

void ide_retry_pc(ide_drive_t *drive)
{
	struct request *failed_rq = drive->hwif->rq;
	struct request *sense_rq = &drive->sense_rq;
	struct ide_atapi_pc *pc = &drive->request_sense_pc;

	(void)ide_read_error(drive);

	
	ide_init_pc(pc);
	memcpy(pc->c, sense_rq->cmd, 12);

	if (drive->media == ide_tape)
		drive->atapi_flags |= IDE_AFLAG_IGNORE_DSC;

	drive->hwif->rq = NULL;
	ide_requeue_and_plug(drive, failed_rq);
	if (ide_queue_sense_rq(drive, pc)) {
		blk_start_request(failed_rq);
		ide_complete_rq(drive, -EIO, blk_rq_bytes(failed_rq));
	}
}
EXPORT_SYMBOL_GPL(ide_retry_pc);

int ide_cd_expiry(ide_drive_t *drive)
{
	struct request *rq = drive->hwif->rq;
	unsigned long wait = 0;

	debug_log("%s: rq->cmd[0]: 0x%x\n", __func__, rq->cmd[0]);

	switch (rq->cmd[0]) {
	case GPCMD_BLANK:
	case GPCMD_FORMAT_UNIT:
	case GPCMD_RESERVE_RZONE_TRACK:
	case GPCMD_CLOSE_TRACK:
	case GPCMD_FLUSH_CACHE:
		wait = ATAPI_WAIT_PC;
		break;
	default:
		if (!(rq->cmd_flags & REQ_QUIET))
			printk(KERN_INFO PFX "cmd 0x%x timed out\n",
					 rq->cmd[0]);
		wait = 0;
		break;
	}
	return wait;
}
EXPORT_SYMBOL_GPL(ide_cd_expiry);

int ide_cd_get_xferlen(struct request *rq)
{
	switch (rq->cmd_type) {
	case REQ_TYPE_FS:
		return 32768;
	case REQ_TYPE_SENSE:
	case REQ_TYPE_BLOCK_PC:
	case REQ_TYPE_ATA_PC:
		return blk_rq_bytes(rq);
	default:
		return 0;
	}
}
EXPORT_SYMBOL_GPL(ide_cd_get_xferlen);

void ide_read_bcount_and_ireason(ide_drive_t *drive, u16 *bcount, u8 *ireason)
{
	struct ide_taskfile tf;

	drive->hwif->tp_ops->tf_read(drive, &tf, IDE_VALID_NSECT |
				     IDE_VALID_LBAM | IDE_VALID_LBAH);

	*bcount = (tf.lbah << 8) | tf.lbam;
	*ireason = tf.nsect & 3;
}
EXPORT_SYMBOL_GPL(ide_read_bcount_and_ireason);

int ide_check_ireason(ide_drive_t *drive, struct request *rq, int len,
		      int ireason, int rw)
{
	ide_hwif_t *hwif = drive->hwif;

	debug_log("ireason: 0x%x, rw: 0x%x\n", ireason, rw);

	if (ireason == (!rw << 1))
		return 0;
	else if (ireason == (rw << 1)) {
		printk(KERN_ERR PFX "%s: %s: wrong transfer direction!\n",
				drive->name, __func__);

		if (dev_is_idecd(drive))
			ide_pad_transfer(drive, rw, len);
	} else if (!rw && ireason == ATAPI_COD) {
		if (dev_is_idecd(drive)) {
			(void)hwif->tp_ops->read_status(hwif);
			return 0;
		}
	} else {
		if (ireason & ATAPI_COD)
			printk(KERN_ERR PFX "%s: CoD != 0 in %s\n", drive->name,
					__func__);

		
		printk(KERN_ERR PFX "%s: %s: bad interrupt reason 0x%02x\n",
				drive->name, __func__, ireason);
	}

	if (dev_is_idecd(drive) && rq->cmd_type == REQ_TYPE_ATA_PC)
		rq->cmd_flags |= REQ_FAILED;

	return 1;
}
EXPORT_SYMBOL_GPL(ide_check_ireason);

static ide_startstop_t ide_pc_intr(ide_drive_t *drive)
{
	struct ide_atapi_pc *pc = drive->pc;
	ide_hwif_t *hwif = drive->hwif;
	struct ide_cmd *cmd = &hwif->cmd;
	struct request *rq = hwif->rq;
	const struct ide_tp_ops *tp_ops = hwif->tp_ops;
	unsigned int timeout, done;
	u16 bcount;
	u8 stat, ireason, dsc = 0;
	u8 write = !!(pc->flags & PC_FLAG_WRITING);

	debug_log("Enter %s - interrupt handler\n", __func__);

	timeout = (drive->media == ide_floppy) ? WAIT_FLOPPY_CMD
					       : WAIT_TAPE_CMD;

	
	stat = tp_ops->read_status(hwif);

	if (pc->flags & PC_FLAG_DMA_IN_PROGRESS) {
		int rc;

		drive->waiting_for_dma = 0;
		rc = hwif->dma_ops->dma_end(drive);
		ide_dma_unmap_sg(drive, cmd);

		if (rc || (drive->media == ide_tape && (stat & ATA_ERR))) {
			if (drive->media == ide_floppy)
				printk(KERN_ERR PFX "%s: DMA %s error\n",
					drive->name, rq_data_dir(pc->rq)
						     ? "write" : "read");
			pc->flags |= PC_FLAG_DMA_ERROR;
		} else
			rq->resid_len = 0;
		debug_log("%s: DMA finished\n", drive->name);
	}

	
	if ((stat & ATA_DRQ) == 0) {
		int uptodate, error;

		debug_log("Packet command completed, %d bytes transferred\n",
			  blk_rq_bytes(rq));

		pc->flags &= ~PC_FLAG_DMA_IN_PROGRESS;

		local_irq_enable_in_hardirq();

		if (drive->media == ide_tape &&
		    (stat & ATA_ERR) && rq->cmd[0] == REQUEST_SENSE)
			stat &= ~ATA_ERR;

		if ((stat & ATA_ERR) || (pc->flags & PC_FLAG_DMA_ERROR)) {
			
			debug_log("%s: I/O error\n", drive->name);

			if (drive->media != ide_tape)
				pc->rq->errors++;

			if (rq->cmd[0] == REQUEST_SENSE) {
				printk(KERN_ERR PFX "%s: I/O error in request "
						"sense command\n", drive->name);
				return ide_do_reset(drive);
			}

			debug_log("[cmd %x]: check condition\n", rq->cmd[0]);

			
			ide_retry_pc(drive);

			
			return ide_stopped;
		}
		pc->error = 0;

		if ((pc->flags & PC_FLAG_WAIT_FOR_DSC) && (stat & ATA_DSC) == 0)
			dsc = 1;

		done = blk_rq_bytes(rq);

		
		uptodate = drive->pc_callback(drive, dsc);

		if (uptodate == 0)
			drive->failed_pc = NULL;

		if (rq->cmd_type == REQ_TYPE_SPECIAL) {
			rq->errors = 0;
			error = 0;
		} else {

			if (rq->cmd_type != REQ_TYPE_FS && uptodate <= 0) {
				if (rq->errors == 0)
					rq->errors = -EIO;
			}

			error = uptodate ? 0 : -EIO;
		}

		ide_complete_rq(drive, error, blk_rq_bytes(rq));
		return ide_stopped;
	}

	if (pc->flags & PC_FLAG_DMA_IN_PROGRESS) {
		pc->flags &= ~PC_FLAG_DMA_IN_PROGRESS;
		printk(KERN_ERR PFX "%s: The device wants to issue more "
				"interrupts in DMA mode\n", drive->name);
		ide_dma_off(drive);
		return ide_do_reset(drive);
	}

	
	ide_read_bcount_and_ireason(drive, &bcount, &ireason);

	if (ide_check_ireason(drive, rq, bcount, ireason, write))
		return ide_do_reset(drive);

	done = min_t(unsigned int, bcount, cmd->nleft);
	ide_pio_bytes(drive, cmd, write, done);

	
	rq->resid_len -= done;

	bcount -= done;

	if (bcount)
		ide_pad_transfer(drive, write, bcount);

	debug_log("[cmd %x] transferred %d bytes, padded %d bytes, resid: %u\n",
		  rq->cmd[0], done, bcount, rq->resid_len);

	
	ide_set_handler(drive, ide_pc_intr, timeout);
	return ide_started;
}

static void ide_init_packet_cmd(struct ide_cmd *cmd, u8 valid_tf,
				u16 bcount, u8 dma)
{
	cmd->protocol = dma ? ATAPI_PROT_DMA : ATAPI_PROT_PIO;
	cmd->valid.out.tf = IDE_VALID_LBAH | IDE_VALID_LBAM |
			    IDE_VALID_FEATURE | valid_tf;
	cmd->tf.command = ATA_CMD_PACKET;
	cmd->tf.feature = dma;		
	cmd->tf.lbam    = bcount & 0xff;
	cmd->tf.lbah    = (bcount >> 8) & 0xff;
}

static u8 ide_read_ireason(ide_drive_t *drive)
{
	struct ide_taskfile tf;

	drive->hwif->tp_ops->tf_read(drive, &tf, IDE_VALID_NSECT);

	return tf.nsect & 3;
}

static u8 ide_wait_ireason(ide_drive_t *drive, u8 ireason)
{
	int retries = 100;

	while (retries-- && ((ireason & ATAPI_COD) == 0 ||
		(ireason & ATAPI_IO))) {
		printk(KERN_ERR PFX "%s: (IO,CoD != (0,1) while issuing "
				"a packet command, retrying\n", drive->name);
		udelay(100);
		ireason = ide_read_ireason(drive);
		if (retries == 0) {
			printk(KERN_ERR PFX "%s: (IO,CoD != (0,1) while issuing"
					" a packet command, ignoring\n",
					drive->name);
			ireason |= ATAPI_COD;
			ireason &= ~ATAPI_IO;
		}
	}

	return ireason;
}

static int ide_delayed_transfer_pc(ide_drive_t *drive)
{
	
	drive->hwif->tp_ops->output_data(drive, NULL, drive->pc->c, 12);

	
	return WAIT_FLOPPY_CMD;
}

static ide_startstop_t ide_transfer_pc(ide_drive_t *drive)
{
	struct ide_atapi_pc *uninitialized_var(pc);
	ide_hwif_t *hwif = drive->hwif;
	struct request *rq = hwif->rq;
	ide_expiry_t *expiry;
	unsigned int timeout;
	int cmd_len;
	ide_startstop_t startstop;
	u8 ireason;

	if (ide_wait_stat(&startstop, drive, ATA_DRQ, ATA_BUSY, WAIT_READY)) {
		printk(KERN_ERR PFX "%s: Strange, packet command initiated yet "
				"DRQ isn't asserted\n", drive->name);
		return startstop;
	}

	if (drive->atapi_flags & IDE_AFLAG_DRQ_INTERRUPT) {
		if (drive->dma)
			drive->waiting_for_dma = 1;
	}

	if (dev_is_idecd(drive)) {
		
		cmd_len = COMMAND_SIZE(rq->cmd[0]);
		if (cmd_len < ATAPI_MIN_CDB_BYTES)
			cmd_len = ATAPI_MIN_CDB_BYTES;

		timeout = rq->timeout;
		expiry  = ide_cd_expiry;
	} else {
		pc = drive->pc;

		cmd_len = ATAPI_MIN_CDB_BYTES;

		if (drive->atapi_flags & IDE_AFLAG_ZIP_DRIVE) {
			timeout = drive->pc_delay;
			expiry = &ide_delayed_transfer_pc;
		} else {
			timeout = (drive->media == ide_floppy) ? WAIT_FLOPPY_CMD
							       : WAIT_TAPE_CMD;
			expiry = NULL;
		}

		ireason = ide_read_ireason(drive);
		if (drive->media == ide_tape)
			ireason = ide_wait_ireason(drive, ireason);

		if ((ireason & ATAPI_COD) == 0 || (ireason & ATAPI_IO)) {
			printk(KERN_ERR PFX "%s: (IO,CoD) != (0,1) while "
				"issuing a packet command\n", drive->name);

			return ide_do_reset(drive);
		}
	}

	hwif->expiry = expiry;

	
	ide_set_handler(drive,
			(dev_is_idecd(drive) ? drive->irq_handler
					     : ide_pc_intr),
			timeout);

	
	if ((drive->atapi_flags & IDE_AFLAG_ZIP_DRIVE) == 0)
		hwif->tp_ops->output_data(drive, NULL, rq->cmd, cmd_len);

	
	if (dev_is_idecd(drive)) {
		if (drive->dma)
			hwif->dma_ops->dma_start(drive);
	} else {
		if (pc->flags & PC_FLAG_DMA_OK) {
			pc->flags |= PC_FLAG_DMA_IN_PROGRESS;
			hwif->dma_ops->dma_start(drive);
		}
	}

	return ide_started;
}

ide_startstop_t ide_issue_pc(ide_drive_t *drive, struct ide_cmd *cmd)
{
	struct ide_atapi_pc *pc;
	ide_hwif_t *hwif = drive->hwif;
	ide_expiry_t *expiry = NULL;
	struct request *rq = hwif->rq;
	unsigned int timeout, bytes;
	u16 bcount;
	u8 valid_tf;
	u8 drq_int = !!(drive->atapi_flags & IDE_AFLAG_DRQ_INTERRUPT);

	if (dev_is_idecd(drive)) {
		valid_tf = IDE_VALID_NSECT | IDE_VALID_LBAL;
		bcount = ide_cd_get_xferlen(rq);
		expiry = ide_cd_expiry;
		timeout = ATAPI_WAIT_PC;

		if (drive->dma)
			drive->dma = !ide_dma_prepare(drive, cmd);
	} else {
		pc = drive->pc;

		valid_tf = IDE_VALID_DEVICE;
		bytes = blk_rq_bytes(rq);
		bcount = ((drive->media == ide_tape) ? bytes
						     : min_t(unsigned int,
							     bytes, 63 * 1024));

		
		rq->resid_len = bcount;

		if (pc->flags & PC_FLAG_DMA_ERROR) {
			pc->flags &= ~PC_FLAG_DMA_ERROR;
			ide_dma_off(drive);
		}

		if (pc->flags & PC_FLAG_DMA_OK)
			drive->dma = !ide_dma_prepare(drive, cmd);

		if (!drive->dma)
			pc->flags &= ~PC_FLAG_DMA_OK;

		timeout = (drive->media == ide_floppy) ? WAIT_FLOPPY_CMD
						       : WAIT_TAPE_CMD;
	}

	ide_init_packet_cmd(cmd, valid_tf, bcount, drive->dma);

	(void)do_rw_taskfile(drive, cmd);

	if (drq_int) {
		if (drive->dma)
			drive->waiting_for_dma = 0;
		hwif->expiry = expiry;
	}

	ide_execute_command(drive, cmd, ide_transfer_pc, timeout);

	return drq_int ? ide_started : ide_transfer_pc(drive);
}
EXPORT_SYMBOL_GPL(ide_issue_pc);
