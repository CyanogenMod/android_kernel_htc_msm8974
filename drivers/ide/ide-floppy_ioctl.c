
#include <linux/kernel.h>
#include <linux/ide.h>
#include <linux/cdrom.h>
#include <linux/mutex.h>

#include <asm/unaligned.h>

#include <scsi/scsi_ioctl.h>

#include "ide-floppy.h"


static DEFINE_MUTEX(ide_floppy_ioctl_mutex);
static int ide_floppy_get_format_capacities(ide_drive_t *drive,
					    struct ide_atapi_pc *pc,
					    int __user *arg)
{
	struct ide_disk_obj *floppy = drive->driver_data;
	int i, blocks, length, u_array_size, u_index;
	int __user *argp;
	u8 pc_buf[256], header_len, desc_cnt;

	if (get_user(u_array_size, arg))
		return -EFAULT;

	if (u_array_size <= 0)
		return -EINVAL;

	ide_floppy_create_read_capacity_cmd(pc);

	if (ide_queue_pc_tail(drive, floppy->disk, pc, pc_buf, pc->req_xfer)) {
		printk(KERN_ERR "ide-floppy: Can't get floppy parameters\n");
		return -EIO;
	}

	header_len = pc_buf[3];
	desc_cnt = header_len / 8; 

	u_index = 0;
	argp = arg + 1;

	for (i = 1; i < desc_cnt; i++) {
		unsigned int desc_start = 4 + i*8;

		if (u_index >= u_array_size)
			break;	

		blocks = be32_to_cpup((__be32 *)&pc_buf[desc_start]);
		length = be16_to_cpup((__be16 *)&pc_buf[desc_start + 6]);

		if (put_user(blocks, argp))
			return -EFAULT;

		++argp;

		if (put_user(length, argp))
			return -EFAULT;

		++argp;

		++u_index;
	}

	if (put_user(u_index, arg))
		return -EFAULT;

	return 0;
}

static void ide_floppy_create_format_unit_cmd(struct ide_atapi_pc *pc,
					      u8 *buf, int b, int l,
					      int flags)
{
	ide_init_pc(pc);
	pc->c[0] = GPCMD_FORMAT_UNIT;
	pc->c[1] = 0x17;

	memset(buf, 0, 12);
	buf[1] = 0xA2;
	

	if (flags & 1)				
		buf[1] ^= 0x20;			
	buf[3] = 8;

	put_unaligned(cpu_to_be32(b), (unsigned int *)(&buf[4]));
	put_unaligned(cpu_to_be32(l), (unsigned int *)(&buf[8]));
	pc->req_xfer = 12;
	pc->flags |= PC_FLAG_WRITING;
}

static int ide_floppy_get_sfrp_bit(ide_drive_t *drive, struct ide_atapi_pc *pc)
{
	struct ide_disk_obj *floppy = drive->driver_data;
	u8 buf[20];

	drive->atapi_flags &= ~IDE_AFLAG_SRFP;

	ide_floppy_create_mode_sense_cmd(pc, IDEFLOPPY_CAPABILITIES_PAGE);
	pc->flags |= PC_FLAG_SUPPRESS_ERROR;

	if (ide_queue_pc_tail(drive, floppy->disk, pc, buf, pc->req_xfer))
		return 1;

	if (buf[8 + 2] & 0x40)
		drive->atapi_flags |= IDE_AFLAG_SRFP;

	return 0;
}

static int ide_floppy_format_unit(ide_drive_t *drive, struct ide_atapi_pc *pc,
				  int __user *arg)
{
	struct ide_disk_obj *floppy = drive->driver_data;
	u8 buf[12];
	int blocks, length, flags, err = 0;

	if (floppy->openers > 1) {
		
		drive->dev_flags &= ~IDE_DFLAG_FORMAT_IN_PROGRESS;
		return -EBUSY;
	}

	drive->dev_flags |= IDE_DFLAG_FORMAT_IN_PROGRESS;

	if (get_user(blocks, arg) ||
			get_user(length, arg+1) ||
			get_user(flags, arg+2)) {
		err = -EFAULT;
		goto out;
	}

	ide_floppy_get_sfrp_bit(drive, pc);
	ide_floppy_create_format_unit_cmd(pc, buf, blocks, length, flags);

	if (ide_queue_pc_tail(drive, floppy->disk, pc, buf, pc->req_xfer))
		err = -EIO;

out:
	if (err)
		drive->dev_flags &= ~IDE_DFLAG_FORMAT_IN_PROGRESS;
	return err;
}


static int ide_floppy_get_format_progress(ide_drive_t *drive,
					  struct ide_atapi_pc *pc,
					  int __user *arg)
{
	struct ide_disk_obj *floppy = drive->driver_data;
	u8 sense_buf[18];
	int progress_indication = 0x10000;

	if (drive->atapi_flags & IDE_AFLAG_SRFP) {
		ide_create_request_sense_cmd(drive, pc);
		if (ide_queue_pc_tail(drive, floppy->disk, pc, sense_buf,
				      pc->req_xfer))
			return -EIO;

		if (floppy->sense_key == 2 &&
		    floppy->asc == 4 &&
		    floppy->ascq == 4)
			progress_indication = floppy->progress_indication;

		
	} else {
		ide_hwif_t *hwif = drive->hwif;
		unsigned long flags;
		u8 stat;

		local_irq_save(flags);
		stat = hwif->tp_ops->read_status(hwif);
		local_irq_restore(flags);

		progress_indication = ((stat & ATA_DSC) == 0) ? 0 : 0x10000;
	}

	if (put_user(progress_indication, arg))
		return -EFAULT;

	return 0;
}

static int ide_floppy_lockdoor(ide_drive_t *drive, struct ide_atapi_pc *pc,
			       unsigned long arg, unsigned int cmd)
{
	struct ide_disk_obj *floppy = drive->driver_data;
	struct gendisk *disk = floppy->disk;
	int prevent = (arg && cmd != CDROMEJECT) ? 1 : 0;

	if (floppy->openers > 1)
		return -EBUSY;

	ide_set_media_lock(drive, disk, prevent);

	if (cmd == CDROMEJECT)
		ide_do_start_stop(drive, disk, 2);

	return 0;
}

static int ide_floppy_format_ioctl(ide_drive_t *drive, struct ide_atapi_pc *pc,
				   fmode_t mode, unsigned int cmd,
				   void __user *argp)
{
	switch (cmd) {
	case IDEFLOPPY_IOCTL_FORMAT_SUPPORTED:
		return 0;
	case IDEFLOPPY_IOCTL_FORMAT_GET_CAPACITY:
		return ide_floppy_get_format_capacities(drive, pc, argp);
	case IDEFLOPPY_IOCTL_FORMAT_START:
		if (!(mode & FMODE_WRITE))
			return -EPERM;
		return ide_floppy_format_unit(drive, pc, (int __user *)argp);
	case IDEFLOPPY_IOCTL_FORMAT_GET_PROGRESS:
		return ide_floppy_get_format_progress(drive, pc, argp);
	default:
		return -ENOTTY;
	}
}

int ide_floppy_ioctl(ide_drive_t *drive, struct block_device *bdev,
		     fmode_t mode, unsigned int cmd, unsigned long arg)
{
	struct ide_atapi_pc pc;
	void __user *argp = (void __user *)arg;
	int err;

	mutex_lock(&ide_floppy_ioctl_mutex);
	if (cmd == CDROMEJECT || cmd == CDROM_LOCKDOOR) {
		err = ide_floppy_lockdoor(drive, &pc, arg, cmd);
		goto out;
	}

	err = ide_floppy_format_ioctl(drive, &pc, mode, cmd, argp);
	if (err != -ENOTTY)
		goto out;

	if (cmd != CDROM_SEND_PACKET && cmd != SCSI_IOCTL_SEND_COMMAND)
		err = scsi_cmd_blk_ioctl(bdev, mode, cmd, argp);

	if (err == -ENOTTY)
		err = generic_ide_ioctl(drive, bdev, cmd, arg);

out:
	mutex_unlock(&ide_floppy_ioctl_mutex);
	return err;
}
