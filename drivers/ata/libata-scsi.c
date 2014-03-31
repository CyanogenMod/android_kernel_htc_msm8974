/*
 *  libata-scsi.c - helper library for ATA
 *
 *  Maintained by:  Jeff Garzik <jgarzik@pobox.com>
 *    		    Please ALWAYS copy linux-ide@vger.kernel.org
 *		    on emails.
 *
 *  Copyright 2003-2004 Red Hat, Inc.  All rights reserved.
 *  Copyright 2003-2004 Jeff Garzik
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
 *  Hardware documentation available from
 *  - http://www.t10.org/
 *  - http://www.t13.org/
 *
 */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/spinlock.h>
#include <linux/export.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi_transport.h>
#include <linux/libata.h>
#include <linux/hdreg.h>
#include <linux/uaccess.h>
#include <linux/suspend.h>
#include <asm/unaligned.h>

#include "libata.h"
#include "libata-transport.h"

#define ATA_SCSI_RBUF_SIZE	4096

static DEFINE_SPINLOCK(ata_scsi_rbuf_lock);
static u8 ata_scsi_rbuf[ATA_SCSI_RBUF_SIZE];

typedef unsigned int (*ata_xlat_func_t)(struct ata_queued_cmd *qc);

static struct ata_device *__ata_scsi_find_dev(struct ata_port *ap,
					const struct scsi_device *scsidev);
static struct ata_device *ata_scsi_find_dev(struct ata_port *ap,
					    const struct scsi_device *scsidev);

#define RW_RECOVERY_MPAGE 0x1
#define RW_RECOVERY_MPAGE_LEN 12
#define CACHE_MPAGE 0x8
#define CACHE_MPAGE_LEN 20
#define CONTROL_MPAGE 0xa
#define CONTROL_MPAGE_LEN 12
#define ALL_MPAGES 0x3f
#define ALL_SUB_MPAGES 0xff


static const u8 def_rw_recovery_mpage[RW_RECOVERY_MPAGE_LEN] = {
	RW_RECOVERY_MPAGE,
	RW_RECOVERY_MPAGE_LEN - 2,
	(1 << 7),	
	0,		
	0, 0, 0, 0,
	0,		
	0, 0, 0
};

static const u8 def_cache_mpage[CACHE_MPAGE_LEN] = {
	CACHE_MPAGE,
	CACHE_MPAGE_LEN - 2,
	0,		
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0,		
	0, 0, 0, 0, 0, 0, 0
};

static const u8 def_control_mpage[CONTROL_MPAGE_LEN] = {
	CONTROL_MPAGE,
	CONTROL_MPAGE_LEN - 2,
	2,	
	0,	
	0, 0, 0, 0, 0xff, 0xff,
	0, 30	
};

static const char *ata_lpm_policy_names[] = {
	[ATA_LPM_UNKNOWN]	= "max_performance",
	[ATA_LPM_MAX_POWER]	= "max_performance",
	[ATA_LPM_MED_POWER]	= "medium_power",
	[ATA_LPM_MIN_POWER]	= "min_power",
};

static ssize_t ata_scsi_lpm_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ata_port *ap = ata_shost_to_port(shost);
	enum ata_lpm_policy policy;
	unsigned long flags;

	
	for (policy = ATA_LPM_MAX_POWER;
	     policy < ARRAY_SIZE(ata_lpm_policy_names); policy++) {
		const char *name = ata_lpm_policy_names[policy];

		if (strncmp(name, buf, strlen(name)) == 0)
			break;
	}
	if (policy == ARRAY_SIZE(ata_lpm_policy_names))
		return -EINVAL;

	spin_lock_irqsave(ap->lock, flags);
	ap->target_lpm_policy = policy;
	ata_port_schedule_eh(ap);
	spin_unlock_irqrestore(ap->lock, flags);

	return count;
}

static ssize_t ata_scsi_lpm_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ata_port *ap = ata_shost_to_port(shost);

	if (ap->target_lpm_policy >= ARRAY_SIZE(ata_lpm_policy_names))
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "%s\n",
			ata_lpm_policy_names[ap->target_lpm_policy]);
}
DEVICE_ATTR(link_power_management_policy, S_IRUGO | S_IWUSR,
	    ata_scsi_lpm_show, ata_scsi_lpm_store);
EXPORT_SYMBOL_GPL(dev_attr_link_power_management_policy);

static ssize_t ata_scsi_park_show(struct device *device,
				  struct device_attribute *attr, char *buf)
{
	struct scsi_device *sdev = to_scsi_device(device);
	struct ata_port *ap;
	struct ata_link *link;
	struct ata_device *dev;
	unsigned long flags, now;
	unsigned int uninitialized_var(msecs);
	int rc = 0;

	ap = ata_shost_to_port(sdev->host);

	spin_lock_irqsave(ap->lock, flags);
	dev = ata_scsi_find_dev(ap, sdev);
	if (!dev) {
		rc = -ENODEV;
		goto unlock;
	}
	if (dev->flags & ATA_DFLAG_NO_UNLOAD) {
		rc = -EOPNOTSUPP;
		goto unlock;
	}

	link = dev->link;
	now = jiffies;
	if (ap->pflags & ATA_PFLAG_EH_IN_PROGRESS &&
	    link->eh_context.unloaded_mask & (1 << dev->devno) &&
	    time_after(dev->unpark_deadline, now))
		msecs = jiffies_to_msecs(dev->unpark_deadline - now);
	else
		msecs = 0;

unlock:
	spin_unlock_irq(ap->lock);

	return rc ? rc : snprintf(buf, 20, "%u\n", msecs);
}

static ssize_t ata_scsi_park_store(struct device *device,
				   struct device_attribute *attr,
				   const char *buf, size_t len)
{
	struct scsi_device *sdev = to_scsi_device(device);
	struct ata_port *ap;
	struct ata_device *dev;
	long int input;
	unsigned long flags;
	int rc;

	rc = strict_strtol(buf, 10, &input);
	if (rc || input < -2)
		return -EINVAL;
	if (input > ATA_TMOUT_MAX_PARK) {
		rc = -EOVERFLOW;
		input = ATA_TMOUT_MAX_PARK;
	}

	ap = ata_shost_to_port(sdev->host);

	spin_lock_irqsave(ap->lock, flags);
	dev = ata_scsi_find_dev(ap, sdev);
	if (unlikely(!dev)) {
		rc = -ENODEV;
		goto unlock;
	}
	if (dev->class != ATA_DEV_ATA) {
		rc = -EOPNOTSUPP;
		goto unlock;
	}

	if (input >= 0) {
		if (dev->flags & ATA_DFLAG_NO_UNLOAD) {
			rc = -EOPNOTSUPP;
			goto unlock;
		}

		dev->unpark_deadline = ata_deadline(jiffies, input);
		dev->link->eh_info.dev_action[dev->devno] |= ATA_EH_PARK;
		ata_port_schedule_eh(ap);
		complete(&ap->park_req_pending);
	} else {
		switch (input) {
		case -1:
			dev->flags &= ~ATA_DFLAG_NO_UNLOAD;
			break;
		case -2:
			dev->flags |= ATA_DFLAG_NO_UNLOAD;
			break;
		}
	}
unlock:
	spin_unlock_irqrestore(ap->lock, flags);

	return rc ? rc : len;
}
DEVICE_ATTR(unload_heads, S_IRUGO | S_IWUSR,
	    ata_scsi_park_show, ata_scsi_park_store);
EXPORT_SYMBOL_GPL(dev_attr_unload_heads);

static void ata_scsi_set_sense(struct scsi_cmnd *cmd, u8 sk, u8 asc, u8 ascq)
{
	cmd->result = (DRIVER_SENSE << 24) | SAM_STAT_CHECK_CONDITION;

	scsi_build_sense_buffer(0, cmd->sense_buffer, sk, asc, ascq);
}

static ssize_t
ata_scsi_em_message_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ata_port *ap = ata_shost_to_port(shost);
	if (ap->ops->em_store && (ap->flags & ATA_FLAG_EM))
		return ap->ops->em_store(ap, buf, count);
	return -EINVAL;
}

static ssize_t
ata_scsi_em_message_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ata_port *ap = ata_shost_to_port(shost);

	if (ap->ops->em_show && (ap->flags & ATA_FLAG_EM))
		return ap->ops->em_show(ap, buf);
	return -EINVAL;
}
DEVICE_ATTR(em_message, S_IRUGO | S_IWUSR,
		ata_scsi_em_message_show, ata_scsi_em_message_store);
EXPORT_SYMBOL_GPL(dev_attr_em_message);

static ssize_t
ata_scsi_em_message_type_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct Scsi_Host *shost = class_to_shost(dev);
	struct ata_port *ap = ata_shost_to_port(shost);

	return snprintf(buf, 23, "%d\n", ap->em_message_type);
}
DEVICE_ATTR(em_message_type, S_IRUGO,
		  ata_scsi_em_message_type_show, NULL);
EXPORT_SYMBOL_GPL(dev_attr_em_message_type);

static ssize_t
ata_scsi_activity_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct ata_port *ap = ata_shost_to_port(sdev->host);
	struct ata_device *atadev = ata_scsi_find_dev(ap, sdev);

	if (ap->ops->sw_activity_show && (ap->flags & ATA_FLAG_SW_ACTIVITY))
		return ap->ops->sw_activity_show(atadev, buf);
	return -EINVAL;
}

static ssize_t
ata_scsi_activity_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct scsi_device *sdev = to_scsi_device(dev);
	struct ata_port *ap = ata_shost_to_port(sdev->host);
	struct ata_device *atadev = ata_scsi_find_dev(ap, sdev);
	enum sw_activity val;
	int rc;

	if (ap->ops->sw_activity_store && (ap->flags & ATA_FLAG_SW_ACTIVITY)) {
		val = simple_strtoul(buf, NULL, 0);
		switch (val) {
		case OFF: case BLINK_ON: case BLINK_OFF:
			rc = ap->ops->sw_activity_store(atadev, val);
			if (!rc)
				return count;
			else
				return rc;
		}
	}
	return -EINVAL;
}
DEVICE_ATTR(sw_activity, S_IWUSR | S_IRUGO, ata_scsi_activity_show,
			ata_scsi_activity_store);
EXPORT_SYMBOL_GPL(dev_attr_sw_activity);

struct device_attribute *ata_common_sdev_attrs[] = {
	&dev_attr_unload_heads,
	NULL
};
EXPORT_SYMBOL_GPL(ata_common_sdev_attrs);

static void ata_scsi_invalid_field(struct scsi_cmnd *cmd)
{
	ata_scsi_set_sense(cmd, ILLEGAL_REQUEST, 0x24, 0x0);
	
	cmd->scsi_done(cmd);
}

int ata_std_bios_param(struct scsi_device *sdev, struct block_device *bdev,
		       sector_t capacity, int geom[])
{
	geom[0] = 255;
	geom[1] = 63;
	sector_div(capacity, 255*63);
	geom[2] = capacity;

	return 0;
}

void ata_scsi_unlock_native_capacity(struct scsi_device *sdev)
{
	struct ata_port *ap = ata_shost_to_port(sdev->host);
	struct ata_device *dev;
	unsigned long flags;

	spin_lock_irqsave(ap->lock, flags);

	dev = ata_scsi_find_dev(ap, sdev);
	if (dev && dev->n_sectors < dev->n_native_sectors) {
		dev->flags |= ATA_DFLAG_UNLOCK_HPA;
		dev->link->eh_info.action |= ATA_EH_RESET;
		ata_port_schedule_eh(ap);
	}

	spin_unlock_irqrestore(ap->lock, flags);
	ata_port_wait_eh(ap);
}

static int ata_get_identity(struct ata_port *ap, struct scsi_device *sdev,
			    void __user *arg)
{
	struct ata_device *dev = ata_scsi_find_dev(ap, sdev);
	u16 __user *dst = arg;
	char buf[40];

	if (!dev)
		return -ENOMSG;

	if (copy_to_user(dst, dev->id, ATA_ID_WORDS * sizeof(u16)))
		return -EFAULT;

	ata_id_string(dev->id, buf, ATA_ID_PROD, ATA_ID_PROD_LEN);
	if (copy_to_user(dst + ATA_ID_PROD, buf, ATA_ID_PROD_LEN))
		return -EFAULT;

	ata_id_string(dev->id, buf, ATA_ID_FW_REV, ATA_ID_FW_REV_LEN);
	if (copy_to_user(dst + ATA_ID_FW_REV, buf, ATA_ID_FW_REV_LEN))
		return -EFAULT;

	ata_id_string(dev->id, buf, ATA_ID_SERNO, ATA_ID_SERNO_LEN);
	if (copy_to_user(dst + ATA_ID_SERNO, buf, ATA_ID_SERNO_LEN))
		return -EFAULT;

	return 0;
}

int ata_cmd_ioctl(struct scsi_device *scsidev, void __user *arg)
{
	int rc = 0;
	u8 scsi_cmd[MAX_COMMAND_SIZE];
	u8 args[4], *argbuf = NULL, *sensebuf = NULL;
	int argsize = 0;
	enum dma_data_direction data_dir;
	int cmd_result;

	if (arg == NULL)
		return -EINVAL;

	if (copy_from_user(args, arg, sizeof(args)))
		return -EFAULT;

	sensebuf = kzalloc(SCSI_SENSE_BUFFERSIZE, GFP_NOIO);
	if (!sensebuf)
		return -ENOMEM;

	memset(scsi_cmd, 0, sizeof(scsi_cmd));

	if (args[3]) {
		argsize = ATA_SECT_SIZE * args[3];
		argbuf = kmalloc(argsize, GFP_KERNEL);
		if (argbuf == NULL) {
			rc = -ENOMEM;
			goto error;
		}

		scsi_cmd[1]  = (4 << 1); 
		scsi_cmd[2]  = 0x0e;     
		data_dir = DMA_FROM_DEVICE;
	} else {
		scsi_cmd[1]  = (3 << 1); 
		scsi_cmd[2]  = 0x20;     
		data_dir = DMA_NONE;
	}

	scsi_cmd[0] = ATA_16;

	scsi_cmd[4] = args[2];
	if (args[0] == ATA_CMD_SMART) { 
		scsi_cmd[6]  = args[3];
		scsi_cmd[8]  = args[1];
		scsi_cmd[10] = 0x4f;
		scsi_cmd[12] = 0xc2;
	} else {
		scsi_cmd[6]  = args[1];
	}
	scsi_cmd[14] = args[0];

	cmd_result = scsi_execute(scsidev, scsi_cmd, data_dir, argbuf, argsize,
				  sensebuf, (10*HZ), 5, 0, NULL);

	if (driver_byte(cmd_result) == DRIVER_SENSE) {
		u8 *desc = sensebuf + 8;
		cmd_result &= ~(0xFF<<24); 

		if (cmd_result & SAM_STAT_CHECK_CONDITION) {
			struct scsi_sense_hdr sshdr;
			scsi_normalize_sense(sensebuf, SCSI_SENSE_BUFFERSIZE,
					     &sshdr);
			if (sshdr.sense_key == 0 &&
			    sshdr.asc == 0 && sshdr.ascq == 0)
				cmd_result &= ~SAM_STAT_CHECK_CONDITION;
		}

		
		if (sensebuf[0] == 0x72 &&	
		    desc[0] == 0x09) {		
			args[0] = desc[13];	
			args[1] = desc[3];	
			args[2] = desc[5];	
			if (copy_to_user(arg, args, sizeof(args)))
				rc = -EFAULT;
		}
	}


	if (cmd_result) {
		rc = -EIO;
		goto error;
	}

	if ((argbuf)
	 && copy_to_user(arg + sizeof(args), argbuf, argsize))
		rc = -EFAULT;
error:
	kfree(sensebuf);
	kfree(argbuf);
	return rc;
}

int ata_task_ioctl(struct scsi_device *scsidev, void __user *arg)
{
	int rc = 0;
	u8 scsi_cmd[MAX_COMMAND_SIZE];
	u8 args[7], *sensebuf = NULL;
	int cmd_result;

	if (arg == NULL)
		return -EINVAL;

	if (copy_from_user(args, arg, sizeof(args)))
		return -EFAULT;

	sensebuf = kzalloc(SCSI_SENSE_BUFFERSIZE, GFP_NOIO);
	if (!sensebuf)
		return -ENOMEM;

	memset(scsi_cmd, 0, sizeof(scsi_cmd));
	scsi_cmd[0]  = ATA_16;
	scsi_cmd[1]  = (3 << 1); 
	scsi_cmd[2]  = 0x20;     
	scsi_cmd[4]  = args[1];
	scsi_cmd[6]  = args[2];
	scsi_cmd[8]  = args[3];
	scsi_cmd[10] = args[4];
	scsi_cmd[12] = args[5];
	scsi_cmd[13] = args[6] & 0x4f;
	scsi_cmd[14] = args[0];

	cmd_result = scsi_execute(scsidev, scsi_cmd, DMA_NONE, NULL, 0,
				sensebuf, (10*HZ), 5, 0, NULL);

	if (driver_byte(cmd_result) == DRIVER_SENSE) {
		u8 *desc = sensebuf + 8;
		cmd_result &= ~(0xFF<<24); 

		if (cmd_result & SAM_STAT_CHECK_CONDITION) {
			struct scsi_sense_hdr sshdr;
			scsi_normalize_sense(sensebuf, SCSI_SENSE_BUFFERSIZE,
						&sshdr);
			if (sshdr.sense_key == 0 &&
				sshdr.asc == 0 && sshdr.ascq == 0)
				cmd_result &= ~SAM_STAT_CHECK_CONDITION;
		}

		
		if (sensebuf[0] == 0x72 &&	
				desc[0] == 0x09) {
			args[0] = desc[13];	
			args[1] = desc[3];	
			args[2] = desc[5];	
			args[3] = desc[7];	
			args[4] = desc[9];	
			args[5] = desc[11];	
			args[6] = desc[12];	
			if (copy_to_user(arg, args, sizeof(args)))
				rc = -EFAULT;
		}
	}

	if (cmd_result) {
		rc = -EIO;
		goto error;
	}

 error:
	kfree(sensebuf);
	return rc;
}

static int ata_ioc32(struct ata_port *ap)
{
	if (ap->flags & ATA_FLAG_PIO_DMA)
		return 1;
	if (ap->pflags & ATA_PFLAG_PIO32)
		return 1;
	return 0;
}

int ata_sas_scsi_ioctl(struct ata_port *ap, struct scsi_device *scsidev,
		     int cmd, void __user *arg)
{
	int val = -EINVAL, rc = -EINVAL;
	unsigned long flags;

	switch (cmd) {
	case ATA_IOC_GET_IO32:
		spin_lock_irqsave(ap->lock, flags);
		val = ata_ioc32(ap);
		spin_unlock_irqrestore(ap->lock, flags);
		if (copy_to_user(arg, &val, 1))
			return -EFAULT;
		return 0;

	case ATA_IOC_SET_IO32:
		val = (unsigned long) arg;
		rc = 0;
		spin_lock_irqsave(ap->lock, flags);
		if (ap->pflags & ATA_PFLAG_PIO32CHANGE) {
			if (val)
				ap->pflags |= ATA_PFLAG_PIO32;
			else
				ap->pflags &= ~ATA_PFLAG_PIO32;
		} else {
			if (val != ata_ioc32(ap))
				rc = -EINVAL;
		}
		spin_unlock_irqrestore(ap->lock, flags);
		return rc;

	case HDIO_GET_IDENTITY:
		return ata_get_identity(ap, scsidev, arg);

	case HDIO_DRIVE_CMD:
		if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RAWIO))
			return -EACCES;
		return ata_cmd_ioctl(scsidev, arg);

	case HDIO_DRIVE_TASK:
		if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RAWIO))
			return -EACCES;
		return ata_task_ioctl(scsidev, arg);

	default:
		rc = -ENOTTY;
		break;
	}

	return rc;
}
EXPORT_SYMBOL_GPL(ata_sas_scsi_ioctl);

int ata_scsi_ioctl(struct scsi_device *scsidev, int cmd, void __user *arg)
{
	return ata_sas_scsi_ioctl(ata_shost_to_port(scsidev->host),
				scsidev, cmd, arg);
}
EXPORT_SYMBOL_GPL(ata_scsi_ioctl);

static struct ata_queued_cmd *ata_scsi_qc_new(struct ata_device *dev,
					      struct scsi_cmnd *cmd)
{
	struct ata_queued_cmd *qc;

	qc = ata_qc_new_init(dev);
	if (qc) {
		qc->scsicmd = cmd;
		qc->scsidone = cmd->scsi_done;

		qc->sg = scsi_sglist(cmd);
		qc->n_elem = scsi_sg_count(cmd);
	} else {
		cmd->result = (DID_OK << 16) | (QUEUE_FULL << 1);
		cmd->scsi_done(cmd);
	}

	return qc;
}

static void ata_qc_set_pc_nbytes(struct ata_queued_cmd *qc)
{
	struct scsi_cmnd *scmd = qc->scsicmd;

	qc->extrabytes = scmd->request->extra_len;
	qc->nbytes = scsi_bufflen(scmd) + qc->extrabytes;
}

static void ata_dump_status(unsigned id, struct ata_taskfile *tf)
{
	u8 stat = tf->command, err = tf->feature;

	printk(KERN_WARNING "ata%u: status=0x%02x { ", id, stat);
	if (stat & ATA_BUSY) {
		printk("Busy }\n");	
	} else {
		if (stat & 0x40)	printk("DriveReady ");
		if (stat & 0x20)	printk("DeviceFault ");
		if (stat & 0x10)	printk("SeekComplete ");
		if (stat & 0x08)	printk("DataRequest ");
		if (stat & 0x04)	printk("CorrectedError ");
		if (stat & 0x02)	printk("Index ");
		if (stat & 0x01)	printk("Error ");
		printk("}\n");

		if (err) {
			printk(KERN_WARNING "ata%u: error=0x%02x { ", id, err);
			if (err & 0x04)		printk("DriveStatusError ");
			if (err & 0x80) {
				if (err & 0x04)	printk("BadCRC ");
				else		printk("Sector ");
			}
			if (err & 0x40)		printk("UncorrectableError ");
			if (err & 0x10)		printk("SectorIdNotFound ");
			if (err & 0x02)		printk("TrackZeroNotFound ");
			if (err & 0x01)		printk("AddrMarkNotFound ");
			printk("}\n");
		}
	}
}

static void ata_to_sense_error(unsigned id, u8 drv_stat, u8 drv_err, u8 *sk,
			       u8 *asc, u8 *ascq, int verbose)
{
	int i;

	
	static const unsigned char sense_table[][4] = {
		
		{0xd1, 		ABORTED_COMMAND, 0x00, 0x00}, 	
		
		{0xd0,  	ABORTED_COMMAND, 0x00, 0x00}, 	
		
		{0x61, 		HARDWARE_ERROR, 0x00, 0x00}, 	
				
		{0x84, 		ABORTED_COMMAND, 0x47, 0x00}, 	
		
		{0x37, 		NOT_READY, 0x04, 0x00}, 	
		
		{0x09, 		NOT_READY, 0x04, 0x00}, 	
		
		{0x01, 		MEDIUM_ERROR, 0x13, 0x00}, 	
		
		{0x02, 		HARDWARE_ERROR, 0x00, 0x00}, 	
		
		{0x04, 		ABORTED_COMMAND, 0x00, 0x00}, 	
		
		{0x08, 		NOT_READY, 0x04, 0x00}, 	
		
		{0x10, 		ABORTED_COMMAND, 0x14, 0x00}, 	
		
		{0x08,  	NOT_READY, 0x04, 0x00}, 	
		
		{0x40, 		MEDIUM_ERROR, 0x11, 0x04}, 	
		
		{0x80, 		MEDIUM_ERROR, 0x11, 0x04}, 	
		{0xFF, 0xFF, 0xFF, 0xFF}, 
	};
	static const unsigned char stat_table[][4] = {
		
		{0x80, 		ABORTED_COMMAND, 0x47, 0x00},	
		{0x20, 		HARDWARE_ERROR,  0x00, 0x00}, 	
		{0x08, 		ABORTED_COMMAND, 0x47, 0x00},	
		{0x04, 		RECOVERED_ERROR, 0x11, 0x00},	
		{0xFF, 0xFF, 0xFF, 0xFF}, 
	};

	if (drv_stat & ATA_BUSY) {
		drv_err = 0;	
	}

	if (drv_err) {
		
		for (i = 0; sense_table[i][0] != 0xFF; i++) {
			
			if ((sense_table[i][0] & drv_err) ==
			    sense_table[i][0]) {
				*sk = sense_table[i][1];
				*asc = sense_table[i][2];
				*ascq = sense_table[i][3];
				goto translate_done;
			}
		}
		
		if (verbose)
			printk(KERN_WARNING "ata%u: no sense translation for "
			       "error 0x%02x\n", id, drv_err);
	}

	
	for (i = 0; stat_table[i][0] != 0xFF; i++) {
		if (stat_table[i][0] & drv_stat) {
			*sk = stat_table[i][1];
			*asc = stat_table[i][2];
			*ascq = stat_table[i][3];
			goto translate_done;
		}
	}
	
	if (verbose)
		printk(KERN_WARNING "ata%u: no sense translation for "
		       "status: 0x%02x\n", id, drv_stat);

	*sk = ABORTED_COMMAND;
	*asc = 0x00;
	*ascq = 0x00;

 translate_done:
	if (verbose)
		printk(KERN_ERR "ata%u: translated ATA stat/err 0x%02x/%02x "
		       "to SCSI SK/ASC/ASCQ 0x%x/%02x/%02x\n",
		       id, drv_stat, drv_err, *sk, *asc, *ascq);
	return;
}

static void ata_gen_passthru_sense(struct ata_queued_cmd *qc)
{
	struct scsi_cmnd *cmd = qc->scsicmd;
	struct ata_taskfile *tf = &qc->result_tf;
	unsigned char *sb = cmd->sense_buffer;
	unsigned char *desc = sb + 8;
	int verbose = qc->ap->ops->error_handler == NULL;

	memset(sb, 0, SCSI_SENSE_BUFFERSIZE);

	cmd->result = (DRIVER_SENSE << 24) | SAM_STAT_CHECK_CONDITION;

	if (qc->err_mask ||
	    tf->command & (ATA_BUSY | ATA_DF | ATA_ERR | ATA_DRQ)) {
		ata_to_sense_error(qc->ap->print_id, tf->command, tf->feature,
				   &sb[1], &sb[2], &sb[3], verbose);
		sb[1] &= 0x0f;
	}

	sb[0] = 0x72;

	desc[0] = 0x09;

	
	sb[7] = 14;
	desc[1] = 12;

	desc[2] = 0x00;
	desc[3] = tf->feature;	
	desc[5] = tf->nsect;
	desc[7] = tf->lbal;
	desc[9] = tf->lbam;
	desc[11] = tf->lbah;
	desc[12] = tf->device;
	desc[13] = tf->command; 

	if (tf->flags & ATA_TFLAG_LBA48) {
		desc[2] |= 0x01;
		desc[4] = tf->hob_nsect;
		desc[6] = tf->hob_lbal;
		desc[8] = tf->hob_lbam;
		desc[10] = tf->hob_lbah;
	}
}

static void ata_gen_ata_sense(struct ata_queued_cmd *qc)
{
	struct ata_device *dev = qc->dev;
	struct scsi_cmnd *cmd = qc->scsicmd;
	struct ata_taskfile *tf = &qc->result_tf;
	unsigned char *sb = cmd->sense_buffer;
	unsigned char *desc = sb + 8;
	int verbose = qc->ap->ops->error_handler == NULL;
	u64 block;

	memset(sb, 0, SCSI_SENSE_BUFFERSIZE);

	cmd->result = (DRIVER_SENSE << 24) | SAM_STAT_CHECK_CONDITION;

	
	sb[0] = 0x72;

	if (qc->err_mask ||
	    tf->command & (ATA_BUSY | ATA_DF | ATA_ERR | ATA_DRQ)) {
		ata_to_sense_error(qc->ap->print_id, tf->command, tf->feature,
				   &sb[1], &sb[2], &sb[3], verbose);
		sb[1] &= 0x0f;
	}

	block = ata_tf_read_block(&qc->result_tf, dev);

	
	sb[7] = 12;
	desc[0] = 0x00;
	desc[1] = 10;

	desc[2] |= 0x80;	
	desc[6] = block >> 40;
	desc[7] = block >> 32;
	desc[8] = block >> 24;
	desc[9] = block >> 16;
	desc[10] = block >> 8;
	desc[11] = block;
}

static void ata_scsi_sdev_config(struct scsi_device *sdev)
{
	sdev->use_10_for_rw = 1;
	sdev->use_10_for_ms = 1;

	sdev->max_device_blocked = 1;
}

static int atapi_drain_needed(struct request *rq)
{
	if (likely(rq->cmd_type != REQ_TYPE_BLOCK_PC))
		return 0;

	if (!blk_rq_bytes(rq) || (rq->cmd_flags & REQ_WRITE))
		return 0;

	return atapi_cmd_type(rq->cmd[0]) == ATAPI_MISC;
}

static int ata_scsi_dev_config(struct scsi_device *sdev,
			       struct ata_device *dev)
{
	struct request_queue *q = sdev->request_queue;

	if (!ata_id_has_unload(dev->id))
		dev->flags |= ATA_DFLAG_NO_UNLOAD;

	
	blk_queue_max_hw_sectors(q, dev->max_sectors);

	if (dev->class == ATA_DEV_ATAPI) {
		void *buf;

		sdev->sector_size = ATA_SECT_SIZE;

		
		blk_queue_update_dma_pad(q, ATA_DMA_PAD_SZ - 1);

		
		buf = kmalloc(ATAPI_MAX_DRAIN, q->bounce_gfp | GFP_KERNEL);
		if (!buf) {
			ata_dev_err(dev, "drain buffer allocation failed\n");
			return -ENOMEM;
		}

		blk_queue_dma_drain(q, atapi_drain_needed, buf, ATAPI_MAX_DRAIN);
	} else {
		sdev->sector_size = ata_id_logical_sector_size(dev->id);
		sdev->manage_start_stop = 1;
	}

	if (sdev->sector_size > PAGE_SIZE)
		ata_dev_warn(dev,
			"sector_size=%u > PAGE_SIZE, PIO may malfunction\n",
			sdev->sector_size);

	blk_queue_update_dma_alignment(q, sdev->sector_size - 1);

	if (dev->flags & ATA_DFLAG_AN)
		set_bit(SDEV_EVT_MEDIA_CHANGE, sdev->supported_events);

	if (dev->flags & ATA_DFLAG_NCQ) {
		int depth;

		depth = min(sdev->host->can_queue, ata_id_queue_depth(dev->id));
		depth = min(ATA_MAX_QUEUE - 1, depth);
		scsi_adjust_queue_depth(sdev, MSG_SIMPLE_TAG, depth);
	}

	blk_queue_flush_queueable(q, false);

	dev->sdev = sdev;
	return 0;
}


int ata_scsi_slave_config(struct scsi_device *sdev)
{
	struct ata_port *ap = ata_shost_to_port(sdev->host);
	struct ata_device *dev = __ata_scsi_find_dev(ap, sdev);
	int rc = 0;

	ata_scsi_sdev_config(sdev);

	if (dev)
		rc = ata_scsi_dev_config(sdev, dev);

	return rc;
}

void ata_scsi_slave_destroy(struct scsi_device *sdev)
{
	struct ata_port *ap = ata_shost_to_port(sdev->host);
	struct request_queue *q = sdev->request_queue;
	unsigned long flags;
	struct ata_device *dev;

	if (!ap->ops->error_handler)
		return;

	spin_lock_irqsave(ap->lock, flags);
	dev = __ata_scsi_find_dev(ap, sdev);
	if (dev && dev->sdev) {
		
		dev->sdev = NULL;
		dev->flags |= ATA_DFLAG_DETACH;
		ata_port_schedule_eh(ap);
	}
	spin_unlock_irqrestore(ap->lock, flags);

	kfree(q->dma_drain_buffer);
	q->dma_drain_buffer = NULL;
	q->dma_drain_size = 0;
}

int __ata_change_queue_depth(struct ata_port *ap, struct scsi_device *sdev,
			     int queue_depth, int reason)
{
	struct ata_device *dev;
	unsigned long flags;

	if (reason != SCSI_QDEPTH_DEFAULT)
		return -EOPNOTSUPP;

	if (queue_depth < 1 || queue_depth == sdev->queue_depth)
		return sdev->queue_depth;

	dev = ata_scsi_find_dev(ap, sdev);
	if (!dev || !ata_dev_enabled(dev))
		return sdev->queue_depth;

	
	spin_lock_irqsave(ap->lock, flags);
	dev->flags &= ~ATA_DFLAG_NCQ_OFF;
	if (queue_depth == 1 || !ata_ncq_enabled(dev)) {
		dev->flags |= ATA_DFLAG_NCQ_OFF;
		queue_depth = 1;
	}
	spin_unlock_irqrestore(ap->lock, flags);

	
	queue_depth = min(queue_depth, sdev->host->can_queue);
	queue_depth = min(queue_depth, ata_id_queue_depth(dev->id));
	queue_depth = min(queue_depth, ATA_MAX_QUEUE - 1);

	if (sdev->queue_depth == queue_depth)
		return -EINVAL;

	scsi_adjust_queue_depth(sdev, MSG_SIMPLE_TAG, queue_depth);
	return queue_depth;
}

int ata_scsi_change_queue_depth(struct scsi_device *sdev, int queue_depth,
				int reason)
{
	struct ata_port *ap = ata_shost_to_port(sdev->host);

	return __ata_change_queue_depth(ap, sdev, queue_depth, reason);
}

static unsigned int ata_scsi_start_stop_xlat(struct ata_queued_cmd *qc)
{
	struct scsi_cmnd *scmd = qc->scsicmd;
	struct ata_taskfile *tf = &qc->tf;
	const u8 *cdb = scmd->cmnd;

	if (scmd->cmd_len < 5)
		goto invalid_fld;

	tf->flags |= ATA_TFLAG_DEVICE | ATA_TFLAG_ISADDR;
	tf->protocol = ATA_PROT_NODATA;
	if (cdb[1] & 0x1) {
		;	
	}
	if (cdb[4] & 0x2)
		goto invalid_fld;       
	if (((cdb[4] >> 4) & 0xf) != 0)
		goto invalid_fld;       

	if (cdb[4] & 0x1) {
		tf->nsect = 1;	

		if (qc->dev->flags & ATA_DFLAG_LBA) {
			tf->flags |= ATA_TFLAG_LBA;

			tf->lbah = 0x0;
			tf->lbam = 0x0;
			tf->lbal = 0x0;
			tf->device |= ATA_LBA;
		} else {
			
			tf->lbal = 0x1; 
			tf->lbam = 0x0; 
			tf->lbah = 0x0; 
		}

		tf->command = ATA_CMD_VERIFY;	
	} else {
		if ((qc->ap->flags & ATA_FLAG_NO_POWEROFF_SPINDOWN) &&
		    system_state == SYSTEM_POWER_OFF)
			goto skip;

		if ((qc->ap->flags & ATA_FLAG_NO_HIBERNATE_SPINDOWN) &&
		     system_entering_hibernation())
			goto skip;

		
		tf->command = ATA_CMD_STANDBYNOW1;
	}


	return 0;

 invalid_fld:
	ata_scsi_set_sense(scmd, ILLEGAL_REQUEST, 0x24, 0x0);
	
	return 1;
 skip:
	scmd->result = SAM_STAT_GOOD;
	return 1;
}


static unsigned int ata_scsi_flush_xlat(struct ata_queued_cmd *qc)
{
	struct ata_taskfile *tf = &qc->tf;

	tf->flags |= ATA_TFLAG_DEVICE;
	tf->protocol = ATA_PROT_NODATA;

	if (qc->dev->flags & ATA_DFLAG_FLUSH_EXT)
		tf->command = ATA_CMD_FLUSH_EXT;
	else
		tf->command = ATA_CMD_FLUSH;

	
	qc->flags |= ATA_QCFLAG_IO;

	return 0;
}

static void scsi_6_lba_len(const u8 *cdb, u64 *plba, u32 *plen)
{
	u64 lba = 0;
	u32 len;

	VPRINTK("six-byte command\n");

	lba |= ((u64)(cdb[1] & 0x1f)) << 16;
	lba |= ((u64)cdb[2]) << 8;
	lba |= ((u64)cdb[3]);

	len = cdb[4];

	*plba = lba;
	*plen = len;
}

static void scsi_10_lba_len(const u8 *cdb, u64 *plba, u32 *plen)
{
	u64 lba = 0;
	u32 len = 0;

	VPRINTK("ten-byte command\n");

	lba |= ((u64)cdb[2]) << 24;
	lba |= ((u64)cdb[3]) << 16;
	lba |= ((u64)cdb[4]) << 8;
	lba |= ((u64)cdb[5]);

	len |= ((u32)cdb[7]) << 8;
	len |= ((u32)cdb[8]);

	*plba = lba;
	*plen = len;
}

static void scsi_16_lba_len(const u8 *cdb, u64 *plba, u32 *plen)
{
	u64 lba = 0;
	u32 len = 0;

	VPRINTK("sixteen-byte command\n");

	lba |= ((u64)cdb[2]) << 56;
	lba |= ((u64)cdb[3]) << 48;
	lba |= ((u64)cdb[4]) << 40;
	lba |= ((u64)cdb[5]) << 32;
	lba |= ((u64)cdb[6]) << 24;
	lba |= ((u64)cdb[7]) << 16;
	lba |= ((u64)cdb[8]) << 8;
	lba |= ((u64)cdb[9]);

	len |= ((u32)cdb[10]) << 24;
	len |= ((u32)cdb[11]) << 16;
	len |= ((u32)cdb[12]) << 8;
	len |= ((u32)cdb[13]);

	*plba = lba;
	*plen = len;
}

static unsigned int ata_scsi_verify_xlat(struct ata_queued_cmd *qc)
{
	struct scsi_cmnd *scmd = qc->scsicmd;
	struct ata_taskfile *tf = &qc->tf;
	struct ata_device *dev = qc->dev;
	u64 dev_sectors = qc->dev->n_sectors;
	const u8 *cdb = scmd->cmnd;
	u64 block;
	u32 n_block;

	tf->flags |= ATA_TFLAG_ISADDR | ATA_TFLAG_DEVICE;
	tf->protocol = ATA_PROT_NODATA;

	if (cdb[0] == VERIFY) {
		if (scmd->cmd_len < 10)
			goto invalid_fld;
		scsi_10_lba_len(cdb, &block, &n_block);
	} else if (cdb[0] == VERIFY_16) {
		if (scmd->cmd_len < 16)
			goto invalid_fld;
		scsi_16_lba_len(cdb, &block, &n_block);
	} else
		goto invalid_fld;

	if (!n_block)
		goto nothing_to_do;
	if (block >= dev_sectors)
		goto out_of_range;
	if ((block + n_block) > dev_sectors)
		goto out_of_range;

	if (dev->flags & ATA_DFLAG_LBA) {
		tf->flags |= ATA_TFLAG_LBA;

		if (lba_28_ok(block, n_block)) {
			
			tf->command = ATA_CMD_VERIFY;
			tf->device |= (block >> 24) & 0xf;
		} else if (lba_48_ok(block, n_block)) {
			if (!(dev->flags & ATA_DFLAG_LBA48))
				goto out_of_range;

			
			tf->flags |= ATA_TFLAG_LBA48;
			tf->command = ATA_CMD_VERIFY_EXT;

			tf->hob_nsect = (n_block >> 8) & 0xff;

			tf->hob_lbah = (block >> 40) & 0xff;
			tf->hob_lbam = (block >> 32) & 0xff;
			tf->hob_lbal = (block >> 24) & 0xff;
		} else
			
			goto out_of_range;

		tf->nsect = n_block & 0xff;

		tf->lbah = (block >> 16) & 0xff;
		tf->lbam = (block >> 8) & 0xff;
		tf->lbal = block & 0xff;

		tf->device |= ATA_LBA;
	} else {
		
		u32 sect, head, cyl, track;

		if (!lba_28_ok(block, n_block))
			goto out_of_range;

		
		track = (u32)block / dev->sectors;
		cyl   = track / dev->heads;
		head  = track % dev->heads;
		sect  = (u32)block % dev->sectors + 1;

		DPRINTK("block %u track %u cyl %u head %u sect %u\n",
			(u32)block, track, cyl, head, sect);

		if ((cyl >> 16) || (head >> 4) || (sect >> 8) || (!sect))
			goto out_of_range;

		tf->command = ATA_CMD_VERIFY;
		tf->nsect = n_block & 0xff; 
		tf->lbal = sect;
		tf->lbam = cyl;
		tf->lbah = cyl >> 8;
		tf->device |= head;
	}

	return 0;

invalid_fld:
	ata_scsi_set_sense(scmd, ILLEGAL_REQUEST, 0x24, 0x0);
	
	return 1;

out_of_range:
	ata_scsi_set_sense(scmd, ILLEGAL_REQUEST, 0x21, 0x0);
	
	return 1;

nothing_to_do:
	scmd->result = SAM_STAT_GOOD;
	return 1;
}

static unsigned int ata_scsi_rw_xlat(struct ata_queued_cmd *qc)
{
	struct scsi_cmnd *scmd = qc->scsicmd;
	const u8 *cdb = scmd->cmnd;
	unsigned int tf_flags = 0;
	u64 block;
	u32 n_block;
	int rc;

	if (cdb[0] == WRITE_10 || cdb[0] == WRITE_6 || cdb[0] == WRITE_16)
		tf_flags |= ATA_TFLAG_WRITE;

	
	switch (cdb[0]) {
	case READ_10:
	case WRITE_10:
		if (unlikely(scmd->cmd_len < 10))
			goto invalid_fld;
		scsi_10_lba_len(cdb, &block, &n_block);
		if (unlikely(cdb[1] & (1 << 3)))
			tf_flags |= ATA_TFLAG_FUA;
		break;
	case READ_6:
	case WRITE_6:
		if (unlikely(scmd->cmd_len < 6))
			goto invalid_fld;
		scsi_6_lba_len(cdb, &block, &n_block);

		if (!n_block)
			n_block = 256;
		break;
	case READ_16:
	case WRITE_16:
		if (unlikely(scmd->cmd_len < 16))
			goto invalid_fld;
		scsi_16_lba_len(cdb, &block, &n_block);
		if (unlikely(cdb[1] & (1 << 3)))
			tf_flags |= ATA_TFLAG_FUA;
		break;
	default:
		DPRINTK("no-byte command\n");
		goto invalid_fld;
	}

	
	if (!n_block)
		goto nothing_to_do;

	qc->flags |= ATA_QCFLAG_IO;
	qc->nbytes = n_block * scmd->device->sector_size;

	rc = ata_build_rw_tf(&qc->tf, qc->dev, block, n_block, tf_flags,
			     qc->tag);
	if (likely(rc == 0))
		return 0;

	if (rc == -ERANGE)
		goto out_of_range;
	
invalid_fld:
	ata_scsi_set_sense(scmd, ILLEGAL_REQUEST, 0x24, 0x0);
	
	return 1;

out_of_range:
	ata_scsi_set_sense(scmd, ILLEGAL_REQUEST, 0x21, 0x0);
	
	return 1;

nothing_to_do:
	scmd->result = SAM_STAT_GOOD;
	return 1;
}

static void ata_scsi_qc_complete(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct scsi_cmnd *cmd = qc->scsicmd;
	u8 *cdb = cmd->cmnd;
	int need_sense = (qc->err_mask != 0);

	if (((cdb[0] == ATA_16) || (cdb[0] == ATA_12)) &&
	    ((cdb[2] & 0x20) || need_sense)) {
		ata_gen_passthru_sense(qc);
	} else {
		if (!need_sense) {
			cmd->result = SAM_STAT_GOOD;
		} else {
			ata_gen_ata_sense(qc);
		}
	}

	if (need_sense && !ap->ops->error_handler)
		ata_dump_status(ap->print_id, &qc->result_tf);

	qc->scsidone(cmd);

	ata_qc_free(qc);
}

static int ata_scsi_translate(struct ata_device *dev, struct scsi_cmnd *cmd,
			      ata_xlat_func_t xlat_func)
{
	struct ata_port *ap = dev->link->ap;
	struct ata_queued_cmd *qc;
	int rc;

	VPRINTK("ENTER\n");

	qc = ata_scsi_qc_new(dev, cmd);
	if (!qc)
		goto err_mem;

	
	if (cmd->sc_data_direction == DMA_FROM_DEVICE ||
	    cmd->sc_data_direction == DMA_TO_DEVICE) {
		if (unlikely(scsi_bufflen(cmd) < 1)) {
			ata_dev_warn(dev, "WARNING: zero len r/w req\n");
			goto err_did;
		}

		ata_sg_init(qc, scsi_sglist(cmd), scsi_sg_count(cmd));

		qc->dma_dir = cmd->sc_data_direction;
	}

	qc->complete_fn = ata_scsi_qc_complete;

	if (xlat_func(qc))
		goto early_finish;

	if (ap->ops->qc_defer) {
		if ((rc = ap->ops->qc_defer(qc)))
			goto defer;
	}

	
	ata_qc_issue(qc);

	VPRINTK("EXIT\n");
	return 0;

early_finish:
	ata_qc_free(qc);
	cmd->scsi_done(cmd);
	DPRINTK("EXIT - early finish (good or error)\n");
	return 0;

err_did:
	ata_qc_free(qc);
	cmd->result = (DID_ERROR << 16);
	cmd->scsi_done(cmd);
err_mem:
	DPRINTK("EXIT - internal\n");
	return 0;

defer:
	ata_qc_free(qc);
	DPRINTK("EXIT - defer\n");
	if (rc == ATA_DEFER_LINK)
		return SCSI_MLQUEUE_DEVICE_BUSY;
	else
		return SCSI_MLQUEUE_HOST_BUSY;
}

static void *ata_scsi_rbuf_get(struct scsi_cmnd *cmd, bool copy_in,
			       unsigned long *flags)
{
	spin_lock_irqsave(&ata_scsi_rbuf_lock, *flags);

	memset(ata_scsi_rbuf, 0, ATA_SCSI_RBUF_SIZE);
	if (copy_in)
		sg_copy_to_buffer(scsi_sglist(cmd), scsi_sg_count(cmd),
				  ata_scsi_rbuf, ATA_SCSI_RBUF_SIZE);
	return ata_scsi_rbuf;
}

static inline void ata_scsi_rbuf_put(struct scsi_cmnd *cmd, bool copy_out,
				     unsigned long *flags)
{
	if (copy_out)
		sg_copy_from_buffer(scsi_sglist(cmd), scsi_sg_count(cmd),
				    ata_scsi_rbuf, ATA_SCSI_RBUF_SIZE);
	spin_unlock_irqrestore(&ata_scsi_rbuf_lock, *flags);
}

static void ata_scsi_rbuf_fill(struct ata_scsi_args *args,
		unsigned int (*actor)(struct ata_scsi_args *args, u8 *rbuf))
{
	u8 *rbuf;
	unsigned int rc;
	struct scsi_cmnd *cmd = args->cmd;
	unsigned long flags;

	rbuf = ata_scsi_rbuf_get(cmd, false, &flags);
	rc = actor(args, rbuf);
	ata_scsi_rbuf_put(cmd, rc == 0, &flags);

	if (rc == 0)
		cmd->result = SAM_STAT_GOOD;
	args->done(cmd);
}

static unsigned int ata_scsiop_inq_std(struct ata_scsi_args *args, u8 *rbuf)
{
	const u8 versions[] = {
		0x60,	

		0x03,
		0x20,	

		0x02,
		0x60	
	};
	u8 hdr[] = {
		TYPE_DISK,
		0,
		0x5,	
		2,
		95 - 4
	};

	VPRINTK("ENTER\n");

	
	if (ata_id_removeable(args->id))
		hdr[1] |= (1 << 7);

	memcpy(rbuf, hdr, sizeof(hdr));
	memcpy(&rbuf[8], "ATA     ", 8);
	ata_id_string(args->id, &rbuf[16], ATA_ID_PROD, 16);
	ata_id_string(args->id, &rbuf[32], ATA_ID_FW_REV, 4);

	if (rbuf[32] == 0 || rbuf[32] == ' ')
		memcpy(&rbuf[32], "n/a ", 4);

	memcpy(rbuf + 59, versions, sizeof(versions));

	return 0;
}

static unsigned int ata_scsiop_inq_00(struct ata_scsi_args *args, u8 *rbuf)
{
	const u8 pages[] = {
		0x00,	
		0x80,	
		0x83,	
		0x89,	
		0xb0,	
		0xb1,	
		0xb2,	
	};

	rbuf[3] = sizeof(pages);	
	memcpy(rbuf + 4, pages, sizeof(pages));
	return 0;
}

static unsigned int ata_scsiop_inq_80(struct ata_scsi_args *args, u8 *rbuf)
{
	const u8 hdr[] = {
		0,
		0x80,			
		0,
		ATA_ID_SERNO_LEN,	
	};

	memcpy(rbuf, hdr, sizeof(hdr));
	ata_id_string(args->id, (unsigned char *) &rbuf[4],
		      ATA_ID_SERNO, ATA_ID_SERNO_LEN);
	return 0;
}

static unsigned int ata_scsiop_inq_83(struct ata_scsi_args *args, u8 *rbuf)
{
	const int sat_model_serial_desc_len = 68;
	int num;

	rbuf[1] = 0x83;			
	num = 4;

	
	rbuf[num + 0] = 2;
	rbuf[num + 3] = ATA_ID_SERNO_LEN;
	num += 4;
	ata_id_string(args->id, (unsigned char *) rbuf + num,
		      ATA_ID_SERNO, ATA_ID_SERNO_LEN);
	num += ATA_ID_SERNO_LEN;

	
	
	rbuf[num + 0] = 2;
	rbuf[num + 1] = 1;
	rbuf[num + 3] = sat_model_serial_desc_len;
	num += 4;
	memcpy(rbuf + num, "ATA     ", 8);
	num += 8;
	ata_id_string(args->id, (unsigned char *) rbuf + num, ATA_ID_PROD,
		      ATA_ID_PROD_LEN);
	num += ATA_ID_PROD_LEN;
	ata_id_string(args->id, (unsigned char *) rbuf + num, ATA_ID_SERNO,
		      ATA_ID_SERNO_LEN);
	num += ATA_ID_SERNO_LEN;

	if (ata_id_has_wwn(args->id)) {
		
		
		rbuf[num + 0] = 1;
		rbuf[num + 1] = 3;
		rbuf[num + 3] = ATA_ID_WWN_LEN;
		num += 4;
		ata_id_string(args->id, (unsigned char *) rbuf + num,
			      ATA_ID_WWN, ATA_ID_WWN_LEN);
		num += ATA_ID_WWN_LEN;
	}
	rbuf[3] = num - 4;    
	return 0;
}

static unsigned int ata_scsiop_inq_89(struct ata_scsi_args *args, u8 *rbuf)
{
	struct ata_taskfile tf;

	memset(&tf, 0, sizeof(tf));

	rbuf[1] = 0x89;			
	rbuf[2] = (0x238 >> 8);		
	rbuf[3] = (0x238 & 0xff);

	memcpy(&rbuf[8], "linux   ", 8);
	memcpy(&rbuf[16], "libata          ", 16);
	memcpy(&rbuf[32], DRV_VERSION, 4);
	ata_id_string(args->id, &rbuf[32], ATA_ID_FW_REV, 4);

	

	tf.command = ATA_DRDY;		
	tf.lbal = 0x1;
	tf.nsect = 0x1;

	ata_tf_to_fis(&tf, 0, 1, &rbuf[36]);	
	rbuf[36] = 0x34;		

	rbuf[56] = ATA_CMD_ID_ATA;

	memcpy(&rbuf[60], &args->id[0], 512);
	return 0;
}

static unsigned int ata_scsiop_inq_b0(struct ata_scsi_args *args, u8 *rbuf)
{
	u16 min_io_sectors;

	rbuf[1] = 0xb0;
	rbuf[3] = 0x3c;		

	min_io_sectors = 1 << ata_id_log2_per_physical_sector(args->id);
	put_unaligned_be16(min_io_sectors, &rbuf[6]);

	if (ata_id_has_trim(args->id)) {
		put_unaligned_be64(65535 * 512 / 8, &rbuf[36]);
		put_unaligned_be32(1, &rbuf[28]);
	}

	return 0;
}

static unsigned int ata_scsiop_inq_b1(struct ata_scsi_args *args, u8 *rbuf)
{
	int form_factor = ata_id_form_factor(args->id);
	int media_rotation_rate = ata_id_rotation_rate(args->id);

	rbuf[1] = 0xb1;
	rbuf[3] = 0x3c;
	rbuf[4] = media_rotation_rate >> 8;
	rbuf[5] = media_rotation_rate;
	rbuf[7] = form_factor;

	return 0;
}

static unsigned int ata_scsiop_inq_b2(struct ata_scsi_args *args, u8 *rbuf)
{
	
	rbuf[1] = 0xb2;
	rbuf[3] = 0x4;
	rbuf[5] = 1 << 6;	

	return 0;
}

static unsigned int ata_scsiop_noop(struct ata_scsi_args *args, u8 *rbuf)
{
	VPRINTK("ENTER\n");
	return 0;
}

static unsigned int ata_msense_caching(u16 *id, u8 *buf)
{
	memcpy(buf, def_cache_mpage, sizeof(def_cache_mpage));
	if (ata_id_wcache_enabled(id))
		buf[2] |= (1 << 2);	
	if (!ata_id_rahead_enabled(id))
		buf[12] |= (1 << 5);	
	return sizeof(def_cache_mpage);
}

static unsigned int ata_msense_ctl_mode(u8 *buf)
{
	memcpy(buf, def_control_mpage, sizeof(def_control_mpage));
	return sizeof(def_control_mpage);
}

static unsigned int ata_msense_rw_recovery(u8 *buf)
{
	memcpy(buf, def_rw_recovery_mpage, sizeof(def_rw_recovery_mpage));
	return sizeof(def_rw_recovery_mpage);
}

static int ata_dev_supports_fua(u16 *id)
{
	unsigned char model[ATA_ID_PROD_LEN + 1], fw[ATA_ID_FW_REV_LEN + 1];

	if (!libata_fua)
		return 0;
	if (!ata_id_has_fua(id))
		return 0;

	ata_id_c_string(id, model, ATA_ID_PROD, sizeof(model));
	ata_id_c_string(id, fw, ATA_ID_FW_REV, sizeof(fw));

	if (strcmp(model, "Maxtor"))
		return 1;
	if (strcmp(fw, "BANC1G10"))
		return 1;

	return 0; 
}

static unsigned int ata_scsiop_mode_sense(struct ata_scsi_args *args, u8 *rbuf)
{
	struct ata_device *dev = args->dev;
	u8 *scsicmd = args->cmd->cmnd, *p = rbuf;
	const u8 sat_blk_desc[] = {
		0, 0, 0, 0,	
		0,
		0, 0x2, 0x0	
	};
	u8 pg, spg;
	unsigned int ebd, page_control, six_byte;
	u8 dpofua;

	VPRINTK("ENTER\n");

	six_byte = (scsicmd[0] == MODE_SENSE);
	ebd = !(scsicmd[1] & 0x8);      

	page_control = scsicmd[2] >> 6;
	switch (page_control) {
	case 0: 
		break;  
	case 3: 
		goto saving_not_supp;
	case 1: 
	case 2: 
	default:
		goto invalid_fld;
	}

	if (six_byte)
		p += 4 + (ebd ? 8 : 0);
	else
		p += 8 + (ebd ? 8 : 0);

	pg = scsicmd[2] & 0x3f;
	spg = scsicmd[3];
	if (spg && (spg != ALL_SUB_MPAGES))
		goto invalid_fld;

	switch(pg) {
	case RW_RECOVERY_MPAGE:
		p += ata_msense_rw_recovery(p);
		break;

	case CACHE_MPAGE:
		p += ata_msense_caching(args->id, p);
		break;

	case CONTROL_MPAGE:
		p += ata_msense_ctl_mode(p);
		break;

	case ALL_MPAGES:
		p += ata_msense_rw_recovery(p);
		p += ata_msense_caching(args->id, p);
		p += ata_msense_ctl_mode(p);
		break;

	default:		
		goto invalid_fld;
	}

	dpofua = 0;
	if (ata_dev_supports_fua(args->id) && (dev->flags & ATA_DFLAG_LBA48) &&
	    (!(dev->flags & ATA_DFLAG_PIO) || dev->multi_count))
		dpofua = 1 << 4;

	if (six_byte) {
		rbuf[0] = p - rbuf - 1;
		rbuf[2] |= dpofua;
		if (ebd) {
			rbuf[3] = sizeof(sat_blk_desc);
			memcpy(rbuf + 4, sat_blk_desc, sizeof(sat_blk_desc));
		}
	} else {
		unsigned int output_len = p - rbuf - 2;

		rbuf[0] = output_len >> 8;
		rbuf[1] = output_len;
		rbuf[3] |= dpofua;
		if (ebd) {
			rbuf[7] = sizeof(sat_blk_desc);
			memcpy(rbuf + 8, sat_blk_desc, sizeof(sat_blk_desc));
		}
	}
	return 0;

invalid_fld:
	ata_scsi_set_sense(args->cmd, ILLEGAL_REQUEST, 0x24, 0x0);
	
	return 1;

saving_not_supp:
	ata_scsi_set_sense(args->cmd, ILLEGAL_REQUEST, 0x39, 0x0);
	 
	return 1;
}

static unsigned int ata_scsiop_read_cap(struct ata_scsi_args *args, u8 *rbuf)
{
	struct ata_device *dev = args->dev;
	u64 last_lba = dev->n_sectors - 1; 
	u32 sector_size; 
	u8 log2_per_phys;
	u16 lowest_aligned;

	sector_size = ata_id_logical_sector_size(dev->id);
	log2_per_phys = ata_id_log2_per_physical_sector(dev->id);
	lowest_aligned = ata_id_logical_sector_offset(dev->id, log2_per_phys);

	VPRINTK("ENTER\n");

	if (args->cmd->cmnd[0] == READ_CAPACITY) {
		if (last_lba >= 0xffffffffULL)
			last_lba = 0xffffffff;

		
		rbuf[0] = last_lba >> (8 * 3);
		rbuf[1] = last_lba >> (8 * 2);
		rbuf[2] = last_lba >> (8 * 1);
		rbuf[3] = last_lba;

		
		rbuf[4] = sector_size >> (8 * 3);
		rbuf[5] = sector_size >> (8 * 2);
		rbuf[6] = sector_size >> (8 * 1);
		rbuf[7] = sector_size;
	} else {
		
		rbuf[0] = last_lba >> (8 * 7);
		rbuf[1] = last_lba >> (8 * 6);
		rbuf[2] = last_lba >> (8 * 5);
		rbuf[3] = last_lba >> (8 * 4);
		rbuf[4] = last_lba >> (8 * 3);
		rbuf[5] = last_lba >> (8 * 2);
		rbuf[6] = last_lba >> (8 * 1);
		rbuf[7] = last_lba;

		
		rbuf[ 8] = sector_size >> (8 * 3);
		rbuf[ 9] = sector_size >> (8 * 2);
		rbuf[10] = sector_size >> (8 * 1);
		rbuf[11] = sector_size;

		rbuf[12] = 0;
		rbuf[13] = log2_per_phys;
		rbuf[14] = (lowest_aligned >> 8) & 0x3f;
		rbuf[15] = lowest_aligned;

		if (ata_id_has_trim(args->id)) {
			rbuf[14] |= 0x80; 

			if (ata_id_has_zero_after_trim(args->id))
				rbuf[14] |= 0x40; 
		}
	}

	return 0;
}

static unsigned int ata_scsiop_report_luns(struct ata_scsi_args *args, u8 *rbuf)
{
	VPRINTK("ENTER\n");
	rbuf[3] = 8;	

	return 0;
}

static void atapi_sense_complete(struct ata_queued_cmd *qc)
{
	if (qc->err_mask && ((qc->err_mask & AC_ERR_DEV) == 0)) {
		ata_gen_passthru_sense(qc);
	}

	qc->scsidone(qc->scsicmd);
	ata_qc_free(qc);
}

static inline int ata_pio_use_silly(struct ata_port *ap)
{
	return (ap->flags & ATA_FLAG_PIO_DMA);
}

static void atapi_request_sense(struct ata_queued_cmd *qc)
{
	struct ata_port *ap = qc->ap;
	struct scsi_cmnd *cmd = qc->scsicmd;

	DPRINTK("ATAPI request sense\n");

	
	memset(cmd->sense_buffer, 0, SCSI_SENSE_BUFFERSIZE);

#ifdef CONFIG_ATA_SFF
	if (ap->ops->sff_tf_read)
		ap->ops->sff_tf_read(ap, &qc->tf);
#endif

	/* fill these in, for the case where they are -not- overwritten */
	cmd->sense_buffer[0] = 0x70;
	cmd->sense_buffer[2] = qc->tf.feature >> 4;

	ata_qc_reinit(qc);

	
	sg_init_one(&qc->sgent, cmd->sense_buffer, SCSI_SENSE_BUFFERSIZE);
	ata_sg_init(qc, &qc->sgent, 1);
	qc->dma_dir = DMA_FROM_DEVICE;

	memset(&qc->cdb, 0, qc->dev->cdb_len);
	qc->cdb[0] = REQUEST_SENSE;
	qc->cdb[4] = SCSI_SENSE_BUFFERSIZE;

	qc->tf.flags |= ATA_TFLAG_ISADDR | ATA_TFLAG_DEVICE;
	qc->tf.command = ATA_CMD_PACKET;

	if (ata_pio_use_silly(ap)) {
		qc->tf.protocol = ATAPI_PROT_DMA;
		qc->tf.feature |= ATAPI_PKT_DMA;
	} else {
		qc->tf.protocol = ATAPI_PROT_PIO;
		qc->tf.lbam = SCSI_SENSE_BUFFERSIZE;
		qc->tf.lbah = 0;
	}
	qc->nbytes = SCSI_SENSE_BUFFERSIZE;

	qc->complete_fn = atapi_sense_complete;

	ata_qc_issue(qc);

	DPRINTK("EXIT\n");
}

static void atapi_qc_complete(struct ata_queued_cmd *qc)
{
	struct scsi_cmnd *cmd = qc->scsicmd;
	unsigned int err_mask = qc->err_mask;

	VPRINTK("ENTER, err_mask 0x%X\n", err_mask);

	
	if (unlikely(qc->ap->ops->error_handler &&
		     (err_mask || qc->flags & ATA_QCFLAG_SENSE_VALID))) {

		if (!(qc->flags & ATA_QCFLAG_SENSE_VALID)) {
			ata_gen_passthru_sense(qc);
		}

		if (qc->cdb[0] == ALLOW_MEDIUM_REMOVAL && qc->dev->sdev)
			qc->dev->sdev->locked = 0;

		qc->scsicmd->result = SAM_STAT_CHECK_CONDITION;
		qc->scsidone(cmd);
		ata_qc_free(qc);
		return;
	}

	
	if (unlikely(err_mask & AC_ERR_DEV)) {
		cmd->result = SAM_STAT_CHECK_CONDITION;
		atapi_request_sense(qc);
		return;
	} else if (unlikely(err_mask)) {
		ata_gen_passthru_sense(qc);
	} else {
		u8 *scsicmd = cmd->cmnd;

		if ((scsicmd[0] == INQUIRY) && ((scsicmd[1] & 0x03) == 0)) {
			unsigned long flags;
			u8 *buf;

			buf = ata_scsi_rbuf_get(cmd, true, &flags);

			if (buf[2] == 0) {
				buf[2] = 0x5;
				buf[3] = 0x32;
			}

			ata_scsi_rbuf_put(cmd, true, &flags);
		}

		cmd->result = SAM_STAT_GOOD;
	}

	qc->scsidone(cmd);
	ata_qc_free(qc);
}
static unsigned int atapi_xlat(struct ata_queued_cmd *qc)
{
	struct scsi_cmnd *scmd = qc->scsicmd;
	struct ata_device *dev = qc->dev;
	int nodata = (scmd->sc_data_direction == DMA_NONE);
	int using_pio = !nodata && (dev->flags & ATA_DFLAG_PIO);
	unsigned int nbytes;

	memset(qc->cdb, 0, dev->cdb_len);
	memcpy(qc->cdb, scmd->cmnd, scmd->cmd_len);

	qc->complete_fn = atapi_qc_complete;

	qc->tf.flags |= ATA_TFLAG_ISADDR | ATA_TFLAG_DEVICE;
	if (scmd->sc_data_direction == DMA_TO_DEVICE) {
		qc->tf.flags |= ATA_TFLAG_WRITE;
		DPRINTK("direction: write\n");
	}

	qc->tf.command = ATA_CMD_PACKET;
	ata_qc_set_pc_nbytes(qc);

	
	if (!nodata && !using_pio && atapi_check_dma(qc))
		using_pio = 1;

	nbytes = min(ata_qc_raw_nbytes(qc), (unsigned int)63 * 1024);

	if (nbytes & 0x1)
		nbytes++;

	qc->tf.lbam = (nbytes & 0xFF);
	qc->tf.lbah = (nbytes >> 8);

	if (nodata)
		qc->tf.protocol = ATAPI_PROT_NODATA;
	else if (using_pio)
		qc->tf.protocol = ATAPI_PROT_PIO;
	else {
		
		qc->tf.protocol = ATAPI_PROT_DMA;
		qc->tf.feature |= ATAPI_PKT_DMA;

		if ((dev->flags & ATA_DFLAG_DMADIR) &&
		    (scmd->sc_data_direction != DMA_TO_DEVICE))
			
			qc->tf.feature |= ATAPI_DMADIR;
	}


	return 0;
}

static struct ata_device *ata_find_dev(struct ata_port *ap, int devno)
{
	if (!sata_pmp_attached(ap)) {
		if (likely(devno < ata_link_max_devices(&ap->link)))
			return &ap->link.device[devno];
	} else {
		if (likely(devno < ap->nr_pmp_links))
			return &ap->pmp_link[devno].device[0];
	}

	return NULL;
}

static struct ata_device *__ata_scsi_find_dev(struct ata_port *ap,
					      const struct scsi_device *scsidev)
{
	int devno;

	
	if (!sata_pmp_attached(ap)) {
		if (unlikely(scsidev->channel || scsidev->lun))
			return NULL;
		devno = scsidev->id;
	} else {
		if (unlikely(scsidev->id || scsidev->lun))
			return NULL;
		devno = scsidev->channel;
	}

	return ata_find_dev(ap, devno);
}

static struct ata_device *
ata_scsi_find_dev(struct ata_port *ap, const struct scsi_device *scsidev)
{
	struct ata_device *dev = __ata_scsi_find_dev(ap, scsidev);

	if (unlikely(!dev || !ata_dev_enabled(dev)))
		return NULL;

	return dev;
}

static u8
ata_scsi_map_proto(u8 byte1)
{
	switch((byte1 & 0x1e) >> 1) {
	case 3:		
		return ATA_PROT_NODATA;

	case 6:		
	case 10:	
	case 11:	
		return ATA_PROT_DMA;

	case 4:		
	case 5:		
		return ATA_PROT_PIO;

	case 0:		
	case 1:		
	case 8:		
	case 9:		
	case 7:		
	case 12:	
	case 15:	
	default:	
		break;
	}

	return ATA_PROT_UNKNOWN;
}

static unsigned int ata_scsi_pass_thru(struct ata_queued_cmd *qc)
{
	struct ata_taskfile *tf = &(qc->tf);
	struct scsi_cmnd *scmd = qc->scsicmd;
	struct ata_device *dev = qc->dev;
	const u8 *cdb = scmd->cmnd;

	if ((tf->protocol = ata_scsi_map_proto(cdb[1])) == ATA_PROT_UNKNOWN)
		goto invalid_fld;

	if (cdb[0] == ATA_16) {
		if (cdb[1] & 0x01) {
			tf->hob_feature = cdb[3];
			tf->hob_nsect = cdb[5];
			tf->hob_lbal = cdb[7];
			tf->hob_lbam = cdb[9];
			tf->hob_lbah = cdb[11];
			tf->flags |= ATA_TFLAG_LBA48;
		} else
			tf->flags &= ~ATA_TFLAG_LBA48;

		tf->feature = cdb[4];
		tf->nsect = cdb[6];
		tf->lbal = cdb[8];
		tf->lbam = cdb[10];
		tf->lbah = cdb[12];
		tf->device = cdb[13];
		tf->command = cdb[14];
	} else {
		tf->flags &= ~ATA_TFLAG_LBA48;

		tf->feature = cdb[3];
		tf->nsect = cdb[4];
		tf->lbal = cdb[5];
		tf->lbam = cdb[6];
		tf->lbah = cdb[7];
		tf->device = cdb[8];
		tf->command = cdb[9];
	}

	
	tf->device = dev->devno ?
		tf->device | ATA_DEV1 : tf->device & ~ATA_DEV1;

	switch (tf->command) {
	
	case ATA_CMD_READ_LONG:
	case ATA_CMD_READ_LONG_ONCE:
	case ATA_CMD_WRITE_LONG:
	case ATA_CMD_WRITE_LONG_ONCE:
		if (tf->protocol != ATA_PROT_PIO || tf->nsect != 1)
			goto invalid_fld;
		qc->sect_size = scsi_bufflen(scmd);
		break;

	
	case ATA_CMD_CFA_WRITE_NE:
	case ATA_CMD_CFA_TRANS_SECT:
	case ATA_CMD_CFA_WRITE_MULT_NE:
	
	case ATA_CMD_READ:
	case ATA_CMD_READ_EXT:
	case ATA_CMD_READ_QUEUED:
	
	case ATA_CMD_FPDMA_READ:
	case ATA_CMD_READ_MULTI:
	case ATA_CMD_READ_MULTI_EXT:
	case ATA_CMD_PIO_READ:
	case ATA_CMD_PIO_READ_EXT:
	case ATA_CMD_READ_STREAM_DMA_EXT:
	case ATA_CMD_READ_STREAM_EXT:
	case ATA_CMD_VERIFY:
	case ATA_CMD_VERIFY_EXT:
	case ATA_CMD_WRITE:
	case ATA_CMD_WRITE_EXT:
	case ATA_CMD_WRITE_FUA_EXT:
	case ATA_CMD_WRITE_QUEUED:
	case ATA_CMD_WRITE_QUEUED_FUA_EXT:
	case ATA_CMD_FPDMA_WRITE:
	case ATA_CMD_WRITE_MULTI:
	case ATA_CMD_WRITE_MULTI_EXT:
	case ATA_CMD_WRITE_MULTI_FUA_EXT:
	case ATA_CMD_PIO_WRITE:
	case ATA_CMD_PIO_WRITE_EXT:
	case ATA_CMD_WRITE_STREAM_DMA_EXT:
	case ATA_CMD_WRITE_STREAM_EXT:
		qc->sect_size = scmd->device->sector_size;
		break;

	
	default:
		qc->sect_size = ATA_SECT_SIZE;
	}

	/*
	 * Set flags so that all registers will be written, pass on
	 * write indication (used for PIO/DMA setup), result TF is
	 * copied back and we don't whine too much about its failure.
	 */
	tf->flags |= ATA_TFLAG_ISADDR | ATA_TFLAG_DEVICE;
	if (scmd->sc_data_direction == DMA_TO_DEVICE)
		tf->flags |= ATA_TFLAG_WRITE;

	qc->flags |= ATA_QCFLAG_RESULT_TF | ATA_QCFLAG_QUIET;

	ata_qc_set_pc_nbytes(qc);

	
	if (tf->protocol == ATA_PROT_DMA && dev->dma_mode == 0)
		goto invalid_fld;

	
	if ((cdb[1] & 0xe0) && !is_multi_taskfile(tf))
		goto invalid_fld;

	if (is_multi_taskfile(tf)) {
		unsigned int multi_count = 1 << (cdb[1] >> 5);

		if (multi_count != dev->multi_count)
			ata_dev_warn(dev, "invalid multi_count %u ignored\n",
				     multi_count);
	}

	if (tf->command == ATA_CMD_SET_FEATURES &&
	    tf->feature == SETFEATURES_XFER)
		goto invalid_fld;

	if (tf->command >= 0x5C && tf->command <= 0x5F && !libata_allow_tpm)
		goto invalid_fld;

	return 0;

 invalid_fld:
	ata_scsi_set_sense(scmd, ILLEGAL_REQUEST, 0x24, 0x00);
	
	return 1;
}

static unsigned int ata_scsi_write_same_xlat(struct ata_queued_cmd *qc)
{
	struct ata_taskfile *tf = &qc->tf;
	struct scsi_cmnd *scmd = qc->scsicmd;
	struct ata_device *dev = qc->dev;
	const u8 *cdb = scmd->cmnd;
	u64 block;
	u32 n_block;
	u32 size;
	void *buf;

	
	if (unlikely(!dev->dma_mode))
		goto invalid_fld;

	if (unlikely(scmd->cmd_len < 16))
		goto invalid_fld;
	scsi_16_lba_len(cdb, &block, &n_block);

	
	if (unlikely(!(cdb[1] & 0x8)))
		goto invalid_fld;

	if (!scsi_sg_count(scmd))
		goto invalid_fld;

	buf = page_address(sg_page(scsi_sglist(scmd)));
	size = ata_set_lba_range_entries(buf, 512, block, n_block);

	tf->protocol = ATA_PROT_DMA;
	tf->hob_feature = 0;
	tf->feature = ATA_DSM_TRIM;
	tf->hob_nsect = (size / 512) >> 8;
	tf->nsect = size / 512;
	tf->command = ATA_CMD_DSM;
	tf->flags |= ATA_TFLAG_ISADDR | ATA_TFLAG_DEVICE | ATA_TFLAG_LBA48 |
		     ATA_TFLAG_WRITE;

	ata_qc_set_pc_nbytes(qc);

	return 0;

 invalid_fld:
	ata_scsi_set_sense(scmd, ILLEGAL_REQUEST, 0x24, 0x00);
	
	return 1;
}


static inline ata_xlat_func_t ata_get_xlat_func(struct ata_device *dev, u8 cmd)
{
	switch (cmd) {
	case READ_6:
	case READ_10:
	case READ_16:

	case WRITE_6:
	case WRITE_10:
	case WRITE_16:
		return ata_scsi_rw_xlat;

	case WRITE_SAME_16:
		return ata_scsi_write_same_xlat;

	case SYNCHRONIZE_CACHE:
		if (ata_try_flush_cache(dev))
			return ata_scsi_flush_xlat;
		break;

	case VERIFY:
	case VERIFY_16:
		return ata_scsi_verify_xlat;

	case ATA_12:
	case ATA_16:
		return ata_scsi_pass_thru;

	case START_STOP:
		return ata_scsi_start_stop_xlat;
	}

	return NULL;
}


static inline void ata_scsi_dump_cdb(struct ata_port *ap,
				     struct scsi_cmnd *cmd)
{
#ifdef ATA_DEBUG
	struct scsi_device *scsidev = cmd->device;
	u8 *scsicmd = cmd->cmnd;

	DPRINTK("CDB (%u:%d,%d,%d) %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
		ap->print_id,
		scsidev->channel, scsidev->id, scsidev->lun,
		scsicmd[0], scsicmd[1], scsicmd[2], scsicmd[3],
		scsicmd[4], scsicmd[5], scsicmd[6], scsicmd[7],
		scsicmd[8]);
#endif
}

static inline int __ata_scsi_queuecmd(struct scsi_cmnd *scmd,
				      struct ata_device *dev)
{
	u8 scsi_op = scmd->cmnd[0];
	ata_xlat_func_t xlat_func;
	int rc = 0;

	if (dev->class == ATA_DEV_ATA) {
		if (unlikely(!scmd->cmd_len || scmd->cmd_len > dev->cdb_len))
			goto bad_cdb_len;

		xlat_func = ata_get_xlat_func(dev, scsi_op);
	} else {
		if (unlikely(!scmd->cmd_len))
			goto bad_cdb_len;

		xlat_func = NULL;
		if (likely((scsi_op != ATA_16) || !atapi_passthru16)) {
			
			int len = COMMAND_SIZE(scsi_op);
			if (unlikely(len > scmd->cmd_len || len > dev->cdb_len))
				goto bad_cdb_len;

			xlat_func = atapi_xlat;
		} else {
			
			if (unlikely(scmd->cmd_len > 16))
				goto bad_cdb_len;

			xlat_func = ata_get_xlat_func(dev, scsi_op);
		}
	}

	if (xlat_func)
		rc = ata_scsi_translate(dev, scmd, xlat_func);
	else
		ata_scsi_simulate(dev, scmd);

	return rc;

 bad_cdb_len:
	DPRINTK("bad CDB len=%u, scsi_op=0x%02x, max=%u\n",
		scmd->cmd_len, scsi_op, dev->cdb_len);
	scmd->result = DID_ERROR << 16;
	scmd->scsi_done(scmd);
	return 0;
}

int ata_scsi_queuecmd(struct Scsi_Host *shost, struct scsi_cmnd *cmd)
{
	struct ata_port *ap;
	struct ata_device *dev;
	struct scsi_device *scsidev = cmd->device;
	int rc = 0;
	unsigned long irq_flags;

	ap = ata_shost_to_port(shost);

	spin_lock_irqsave(ap->lock, irq_flags);

	ata_scsi_dump_cdb(ap, cmd);

	dev = ata_scsi_find_dev(ap, scsidev);
	if (likely(dev))
		rc = __ata_scsi_queuecmd(cmd, dev);
	else {
		cmd->result = (DID_BAD_TARGET << 16);
		cmd->scsi_done(cmd);
	}

	spin_unlock_irqrestore(ap->lock, irq_flags);

	return rc;
}


void ata_scsi_simulate(struct ata_device *dev, struct scsi_cmnd *cmd)
{
	struct ata_scsi_args args;
	const u8 *scsicmd = cmd->cmnd;
	u8 tmp8;

	args.dev = dev;
	args.id = dev->id;
	args.cmd = cmd;
	args.done = cmd->scsi_done;

	switch(scsicmd[0]) {
	
	case FORMAT_UNIT:
		ata_scsi_invalid_field(cmd);
		break;

	case INQUIRY:
		if (scsicmd[1] & 2)	           
			ata_scsi_invalid_field(cmd);
		else if ((scsicmd[1] & 1) == 0)    
			ata_scsi_rbuf_fill(&args, ata_scsiop_inq_std);
		else switch (scsicmd[2]) {
		case 0x00:
			ata_scsi_rbuf_fill(&args, ata_scsiop_inq_00);
			break;
		case 0x80:
			ata_scsi_rbuf_fill(&args, ata_scsiop_inq_80);
			break;
		case 0x83:
			ata_scsi_rbuf_fill(&args, ata_scsiop_inq_83);
			break;
		case 0x89:
			ata_scsi_rbuf_fill(&args, ata_scsiop_inq_89);
			break;
		case 0xb0:
			ata_scsi_rbuf_fill(&args, ata_scsiop_inq_b0);
			break;
		case 0xb1:
			ata_scsi_rbuf_fill(&args, ata_scsiop_inq_b1);
			break;
		case 0xb2:
			ata_scsi_rbuf_fill(&args, ata_scsiop_inq_b2);
			break;
		default:
			ata_scsi_invalid_field(cmd);
			break;
		}
		break;

	case MODE_SENSE:
	case MODE_SENSE_10:
		ata_scsi_rbuf_fill(&args, ata_scsiop_mode_sense);
		break;

	case MODE_SELECT:	
	case MODE_SELECT_10:	
		ata_scsi_invalid_field(cmd);
		break;

	case READ_CAPACITY:
		ata_scsi_rbuf_fill(&args, ata_scsiop_read_cap);
		break;

	case SERVICE_ACTION_IN:
		if ((scsicmd[1] & 0x1f) == SAI_READ_CAPACITY_16)
			ata_scsi_rbuf_fill(&args, ata_scsiop_read_cap);
		else
			ata_scsi_invalid_field(cmd);
		break;

	case REPORT_LUNS:
		ata_scsi_rbuf_fill(&args, ata_scsiop_report_luns);
		break;

	case REQUEST_SENSE:
		ata_scsi_set_sense(cmd, 0, 0, 0);
		cmd->result = (DRIVER_SENSE << 24);
		cmd->scsi_done(cmd);
		break;

	case SYNCHRONIZE_CACHE:
		

	
	case REZERO_UNIT:
	case SEEK_6:
	case SEEK_10:
	case TEST_UNIT_READY:
		ata_scsi_rbuf_fill(&args, ata_scsiop_noop);
		break;

	case SEND_DIAGNOSTIC:
		tmp8 = scsicmd[1] & ~(1 << 3);
		if ((tmp8 == 0x4) && (!scsicmd[3]) && (!scsicmd[4]))
			ata_scsi_rbuf_fill(&args, ata_scsiop_noop);
		else
			ata_scsi_invalid_field(cmd);
		break;

	
	default:
		ata_scsi_set_sense(cmd, ILLEGAL_REQUEST, 0x20, 0x0);
		
		cmd->scsi_done(cmd);
		break;
	}
}

int ata_scsi_add_hosts(struct ata_host *host, struct scsi_host_template *sht)
{
	int i, rc;

	for (i = 0; i < host->n_ports; i++) {
		struct ata_port *ap = host->ports[i];
		struct Scsi_Host *shost;

		rc = -ENOMEM;
		shost = scsi_host_alloc(sht, sizeof(struct ata_port *));
		if (!shost)
			goto err_alloc;

		shost->eh_noresume = 1;
		*(struct ata_port **)&shost->hostdata[0] = ap;
		ap->scsi_host = shost;

		shost->transportt = ata_scsi_transport_template;
		shost->unique_id = ap->print_id;
		shost->max_id = 16;
		shost->max_lun = 1;
		shost->max_channel = 1;
		shost->max_cmd_len = 16;

		shost->max_host_blocked = 1;

		rc = scsi_add_host_with_dma(ap->scsi_host,
						&ap->tdev, ap->host->dev);
		if (rc)
			goto err_add;
	}

	return 0;

 err_add:
	scsi_host_put(host->ports[i]->scsi_host);
 err_alloc:
	while (--i >= 0) {
		struct Scsi_Host *shost = host->ports[i]->scsi_host;

		scsi_remove_host(shost);
		scsi_host_put(shost);
	}
	return rc;
}

void ata_scsi_scan_host(struct ata_port *ap, int sync)
{
	int tries = 5;
	struct ata_device *last_failed_dev = NULL;
	struct ata_link *link;
	struct ata_device *dev;

 repeat:
	ata_for_each_link(link, ap, EDGE) {
		ata_for_each_dev(dev, link, ENABLED) {
			struct scsi_device *sdev;
			int channel = 0, id = 0;

			if (dev->sdev)
				continue;

			if (ata_is_host_link(link))
				id = dev->devno;
			else
				channel = link->pmp;

			sdev = __scsi_add_device(ap->scsi_host, channel, id, 0,
						 NULL);
			if (!IS_ERR(sdev)) {
				dev->sdev = sdev;
				scsi_device_put(sdev);
			} else {
				dev->sdev = NULL;
			}
		}
	}

	ata_for_each_link(link, ap, EDGE) {
		ata_for_each_dev(dev, link, ENABLED) {
			if (!dev->sdev)
				goto exit_loop;
		}
	}
 exit_loop:
	if (!link)
		return;

	
	if (sync) {
		if (dev != last_failed_dev) {
			msleep(100);
			last_failed_dev = dev;
			goto repeat;
		}

		if (--tries) {
			msleep(100);
			goto repeat;
		}

		ata_port_err(ap,
			     "WARNING: synchronous SCSI scan failed without making any progress, switching to async\n");
	}

	queue_delayed_work(system_long_wq, &ap->hotplug_task,
			   round_jiffies_relative(HZ));
}

int ata_scsi_offline_dev(struct ata_device *dev)
{
	if (dev->sdev) {
		scsi_device_set_state(dev->sdev, SDEV_OFFLINE);
		return 1;
	}
	return 0;
}

static void ata_scsi_remove_dev(struct ata_device *dev)
{
	struct ata_port *ap = dev->link->ap;
	struct scsi_device *sdev;
	unsigned long flags;

	mutex_lock(&ap->scsi_host->scan_mutex);
	spin_lock_irqsave(ap->lock, flags);

	
	sdev = dev->sdev;
	dev->sdev = NULL;

	if (sdev) {
		if (scsi_device_get(sdev) == 0) {
			scsi_device_set_state(sdev, SDEV_OFFLINE);
		} else {
			WARN_ON(1);
			sdev = NULL;
		}
	}

	spin_unlock_irqrestore(ap->lock, flags);
	mutex_unlock(&ap->scsi_host->scan_mutex);

	if (sdev) {
		ata_dev_info(dev, "detaching (SCSI %s)\n",
			     dev_name(&sdev->sdev_gendev));

		scsi_remove_device(sdev);
		scsi_device_put(sdev);
	}
}

static void ata_scsi_handle_link_detach(struct ata_link *link)
{
	struct ata_port *ap = link->ap;
	struct ata_device *dev;

	ata_for_each_dev(dev, link, ALL) {
		unsigned long flags;

		if (!(dev->flags & ATA_DFLAG_DETACHED))
			continue;

		spin_lock_irqsave(ap->lock, flags);
		dev->flags &= ~ATA_DFLAG_DETACHED;
		spin_unlock_irqrestore(ap->lock, flags);

		ata_scsi_remove_dev(dev);
	}
}

void ata_scsi_media_change_notify(struct ata_device *dev)
{
	if (dev->sdev)
		sdev_evt_send_simple(dev->sdev, SDEV_EVT_MEDIA_CHANGE,
				     GFP_ATOMIC);
}

void ata_scsi_hotplug(struct work_struct *work)
{
	struct ata_port *ap =
		container_of(work, struct ata_port, hotplug_task.work);
	int i;

	if (ap->pflags & ATA_PFLAG_UNLOADING) {
		DPRINTK("ENTER/EXIT - unloading\n");
		return;
	}

	DPRINTK("ENTER\n");
	mutex_lock(&ap->scsi_scan_mutex);

	ata_scsi_handle_link_detach(&ap->link);
	if (ap->pmp_link)
		for (i = 0; i < SATA_PMP_MAX_PORTS; i++)
			ata_scsi_handle_link_detach(&ap->pmp_link[i]);

	
	ata_scsi_scan_host(ap, 0);

	mutex_unlock(&ap->scsi_scan_mutex);
	DPRINTK("EXIT\n");
}

int ata_scsi_user_scan(struct Scsi_Host *shost, unsigned int channel,
		       unsigned int id, unsigned int lun)
{
	struct ata_port *ap = ata_shost_to_port(shost);
	unsigned long flags;
	int devno, rc = 0;

	if (!ap->ops->error_handler)
		return -EOPNOTSUPP;

	if (lun != SCAN_WILD_CARD && lun)
		return -EINVAL;

	if (!sata_pmp_attached(ap)) {
		if (channel != SCAN_WILD_CARD && channel)
			return -EINVAL;
		devno = id;
	} else {
		if (id != SCAN_WILD_CARD && id)
			return -EINVAL;
		devno = channel;
	}

	spin_lock_irqsave(ap->lock, flags);

	if (devno == SCAN_WILD_CARD) {
		struct ata_link *link;

		ata_for_each_link(link, ap, EDGE) {
			struct ata_eh_info *ehi = &link->eh_info;
			ehi->probe_mask |= ATA_ALL_DEVICES;
			ehi->action |= ATA_EH_RESET;
		}
	} else {
		struct ata_device *dev = ata_find_dev(ap, devno);

		if (dev) {
			struct ata_eh_info *ehi = &dev->link->eh_info;
			ehi->probe_mask |= 1 << dev->devno;
			ehi->action |= ATA_EH_RESET;
		} else
			rc = -EINVAL;
	}

	if (rc == 0) {
		ata_port_schedule_eh(ap);
		spin_unlock_irqrestore(ap->lock, flags);
		ata_port_wait_eh(ap);
	} else
		spin_unlock_irqrestore(ap->lock, flags);

	return rc;
}

void ata_scsi_dev_rescan(struct work_struct *work)
{
	struct ata_port *ap =
		container_of(work, struct ata_port, scsi_rescan_task);
	struct ata_link *link;
	struct ata_device *dev;
	unsigned long flags;

	mutex_lock(&ap->scsi_scan_mutex);
	spin_lock_irqsave(ap->lock, flags);

	ata_for_each_link(link, ap, EDGE) {
		ata_for_each_dev(dev, link, ENABLED) {
			struct scsi_device *sdev = dev->sdev;

			if (!sdev)
				continue;
			if (scsi_device_get(sdev))
				continue;

			spin_unlock_irqrestore(ap->lock, flags);
			scsi_rescan_device(&(sdev->sdev_gendev));
			scsi_device_put(sdev);
			spin_lock_irqsave(ap->lock, flags);
		}
	}

	spin_unlock_irqrestore(ap->lock, flags);
	mutex_unlock(&ap->scsi_scan_mutex);
}


struct ata_port *ata_sas_port_alloc(struct ata_host *host,
				    struct ata_port_info *port_info,
				    struct Scsi_Host *shost)
{
	struct ata_port *ap;

	ap = ata_port_alloc(host);
	if (!ap)
		return NULL;

	ap->port_no = 0;
	ap->lock = &host->lock;
	ap->pio_mask = port_info->pio_mask;
	ap->mwdma_mask = port_info->mwdma_mask;
	ap->udma_mask = port_info->udma_mask;
	ap->flags |= port_info->flags;
	ap->ops = port_info->port_ops;
	ap->cbl = ATA_CBL_SATA;

	return ap;
}
EXPORT_SYMBOL_GPL(ata_sas_port_alloc);

int ata_sas_port_start(struct ata_port *ap)
{
	if (!ap->ops->error_handler)
		ap->pflags &= ~ATA_PFLAG_FROZEN;
	return 0;
}
EXPORT_SYMBOL_GPL(ata_sas_port_start);


void ata_sas_port_stop(struct ata_port *ap)
{
}
EXPORT_SYMBOL_GPL(ata_sas_port_stop);

void ata_sas_async_probe(struct ata_port *ap)
{
	__ata_port_probe(ap);
}
EXPORT_SYMBOL_GPL(ata_sas_async_probe);

int ata_sas_sync_probe(struct ata_port *ap)
{
	return ata_port_probe(ap);
}
EXPORT_SYMBOL_GPL(ata_sas_sync_probe);



int ata_sas_port_init(struct ata_port *ap)
{
	int rc = ap->ops->port_start(ap);

	if (rc)
		return rc;
	ap->print_id = atomic_inc_return(&ata_print_id);
	return 0;
}
EXPORT_SYMBOL_GPL(ata_sas_port_init);


void ata_sas_port_destroy(struct ata_port *ap)
{
	if (ap->ops->port_stop)
		ap->ops->port_stop(ap);
	kfree(ap);
}
EXPORT_SYMBOL_GPL(ata_sas_port_destroy);


int ata_sas_slave_configure(struct scsi_device *sdev, struct ata_port *ap)
{
	ata_scsi_sdev_config(sdev);
	ata_scsi_dev_config(sdev, ap->link.device);
	return 0;
}
EXPORT_SYMBOL_GPL(ata_sas_slave_configure);


int ata_sas_queuecmd(struct scsi_cmnd *cmd, struct ata_port *ap)
{
	int rc = 0;

	ata_scsi_dump_cdb(ap, cmd);

	if (likely(ata_dev_enabled(ap->link.device)))
		rc = __ata_scsi_queuecmd(cmd, ap->link.device);
	else {
		cmd->result = (DID_BAD_TARGET << 16);
		cmd->scsi_done(cmd);
	}
	return rc;
}
EXPORT_SYMBOL_GPL(ata_sas_queuecmd);
