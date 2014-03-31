/*
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
 *
 *  Complications for I2O scsi
 *
 *	o	Each (bus,lun) is a logical device in I2O. We keep a map
 *		table. We spoof failed selection for unmapped units
 *	o	Request sense buffers can come back for free.
 *	o	Scatter gather is a bit dynamic. We have to investigate at
 *		setup time.
 *	o	Some of our resources are dynamically shared. The i2o core
 *		needs a message reservation protocol to avoid swap v net
 *		deadlocking. We need to back off queue requests.
 *
 *	In general the firmware wants to help. Where its help isn't performance
 *	useful we just ignore the aid. Its not worth the code in truth.
 *
 * Fixes/additions:
 *	Steve Ralston:
 *		Scatter gather now works
 *	Markus Lidel <Markus.Lidel@shadowconnect.com>:
 *		Minor fixes for 2.6.
 *
 * To Do:
 *	64bit cleanups
 *	Fix the resource management problems.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/jiffies.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/prefetch.h>
#include <linux/pci.h>
#include <linux/blkdev.h>
#include <linux/i2o.h>
#include <linux/scatterlist.h>

#include <asm/dma.h>
#include <asm/io.h>
#include <linux/atomic.h>

#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/sg.h>

#define OSM_NAME	"scsi-osm"
#define OSM_VERSION	"1.316"
#define OSM_DESCRIPTION	"I2O SCSI Peripheral OSM"

static struct i2o_driver i2o_scsi_driver;

static unsigned int i2o_scsi_max_id = 16;
static unsigned int i2o_scsi_max_lun = 255;

struct i2o_scsi_host {
	struct Scsi_Host *scsi_host;	
	struct i2o_controller *iop;	
	unsigned int lun;	
	struct i2o_device *channel[0];	
};

static struct scsi_host_template i2o_scsi_host_template;

#define I2O_SCSI_CAN_QUEUE	4

static struct i2o_class_id i2o_scsi_class_id[] = {
	{I2O_CLASS_SCSI_PERIPHERAL},
	{I2O_CLASS_END}
};

static struct i2o_scsi_host *i2o_scsi_host_alloc(struct i2o_controller *c)
{
	struct i2o_scsi_host *i2o_shost;
	struct i2o_device *i2o_dev;
	struct Scsi_Host *scsi_host;
	int max_channel = 0;
	u8 type;
	int i;
	size_t size;
	u16 body_size = 6;

#ifdef CONFIG_I2O_EXT_ADAPTEC
	if (c->adaptec)
		body_size = 8;
#endif

	list_for_each_entry(i2o_dev, &c->devices, list)
	    if (i2o_dev->lct_data.class_id == I2O_CLASS_BUS_ADAPTER) {
		if (!i2o_parm_field_get(i2o_dev, 0x0000, 0, &type, 1)
		    && (type == 0x01))	
			max_channel++;
	}

	if (!max_channel) {
		osm_warn("no channels found on %s\n", c->name);
		return ERR_PTR(-EFAULT);
	}

	size = max_channel * sizeof(struct i2o_device *)
	    + sizeof(struct i2o_scsi_host);

	scsi_host = scsi_host_alloc(&i2o_scsi_host_template, size);
	if (!scsi_host) {
		osm_warn("Could not allocate SCSI host\n");
		return ERR_PTR(-ENOMEM);
	}

	scsi_host->max_channel = max_channel - 1;
	scsi_host->max_id = i2o_scsi_max_id;
	scsi_host->max_lun = i2o_scsi_max_lun;
	scsi_host->this_id = c->unit;
	scsi_host->sg_tablesize = i2o_sg_tablesize(c, body_size);

	i2o_shost = (struct i2o_scsi_host *)scsi_host->hostdata;
	i2o_shost->scsi_host = scsi_host;
	i2o_shost->iop = c;
	i2o_shost->lun = 1;

	i = 0;
	list_for_each_entry(i2o_dev, &c->devices, list)
	    if (i2o_dev->lct_data.class_id == I2O_CLASS_BUS_ADAPTER) {
		if (!i2o_parm_field_get(i2o_dev, 0x0000, 0, &type, 1)
		    && (type == 0x01))	
			i2o_shost->channel[i++] = i2o_dev;

		if (i >= max_channel)
			break;
	}

	return i2o_shost;
};

static struct i2o_scsi_host *i2o_scsi_get_host(struct i2o_controller *c)
{
	return c->driver_data[i2o_scsi_driver.context];
};

static int i2o_scsi_remove(struct device *dev)
{
	struct i2o_device *i2o_dev = to_i2o_device(dev);
	struct i2o_controller *c = i2o_dev->iop;
	struct i2o_scsi_host *i2o_shost;
	struct scsi_device *scsi_dev;

	osm_info("device removed (TID: %03x)\n", i2o_dev->lct_data.tid);

	i2o_shost = i2o_scsi_get_host(c);

	shost_for_each_device(scsi_dev, i2o_shost->scsi_host)
	    if (scsi_dev->hostdata == i2o_dev) {
		sysfs_remove_link(&i2o_dev->device.kobj, "scsi");
		scsi_remove_device(scsi_dev);
		scsi_device_put(scsi_dev);
		break;
	}

	return 0;
};

static int i2o_scsi_probe(struct device *dev)
{
	struct i2o_device *i2o_dev = to_i2o_device(dev);
	struct i2o_controller *c = i2o_dev->iop;
	struct i2o_scsi_host *i2o_shost;
	struct Scsi_Host *scsi_host;
	struct i2o_device *parent;
	struct scsi_device *scsi_dev;
	u32 id = -1;
	u64 lun = -1;
	int channel = -1;
	int i, rc;

	i2o_shost = i2o_scsi_get_host(c);
	if (!i2o_shost)
		return -EFAULT;

	scsi_host = i2o_shost->scsi_host;

	switch (i2o_dev->lct_data.class_id) {
	case I2O_CLASS_RANDOM_BLOCK_STORAGE:
	case I2O_CLASS_EXECUTIVE:
#ifdef CONFIG_I2O_EXT_ADAPTEC
		if (c->adaptec) {
			u8 type;
			struct i2o_device *d = i2o_shost->channel[0];

			if (!i2o_parm_field_get(d, 0x0000, 0, &type, 1)
			    && (type == 0x01))	
				if (!i2o_parm_field_get(d, 0x0200, 4, &id, 4)) {
					channel = 0;
					if (i2o_dev->lct_data.class_id ==
					    I2O_CLASS_RANDOM_BLOCK_STORAGE)
						lun =
						    cpu_to_le64(i2o_shost->
								lun++);
					else
						lun = 0;
				}
		}
#endif
		break;

	case I2O_CLASS_SCSI_PERIPHERAL:
		if (i2o_parm_field_get(i2o_dev, 0x0000, 3, &id, 4))
			return -EFAULT;

		if (i2o_parm_field_get(i2o_dev, 0x0000, 4, &lun, 8))
			return -EFAULT;

		parent = i2o_iop_find_device(c, i2o_dev->lct_data.parent_tid);
		if (!parent) {
			osm_warn("can not find parent of device %03x\n",
				 i2o_dev->lct_data.tid);
			return -EFAULT;
		}

		for (i = 0; i <= i2o_shost->scsi_host->max_channel; i++)
			if (i2o_shost->channel[i] == parent)
				channel = i;
		break;

	default:
		return -EFAULT;
	}

	if (channel == -1) {
		osm_warn("can not find channel of device %03x\n",
			 i2o_dev->lct_data.tid);
		return -EFAULT;
	}

	if (le32_to_cpu(id) >= scsi_host->max_id) {
		osm_warn("SCSI device id (%d) >= max_id of I2O host (%d)",
			 le32_to_cpu(id), scsi_host->max_id);
		return -EFAULT;
	}

	if (le64_to_cpu(lun) >= scsi_host->max_lun) {
		osm_warn("SCSI device lun (%lu) >= max_lun of I2O host (%d)",
			 (long unsigned int)le64_to_cpu(lun),
			 scsi_host->max_lun);
		return -EFAULT;
	}

	scsi_dev =
	    __scsi_add_device(i2o_shost->scsi_host, channel, le32_to_cpu(id),
			      le64_to_cpu(lun), i2o_dev);

	if (IS_ERR(scsi_dev)) {
		osm_warn("can not add SCSI device %03x\n",
			 i2o_dev->lct_data.tid);
		return PTR_ERR(scsi_dev);
	}

	rc = sysfs_create_link(&i2o_dev->device.kobj,
			       &scsi_dev->sdev_gendev.kobj, "scsi");
	if (rc)
		goto err;

	osm_info("device added (TID: %03x) channel: %d, id: %d, lun: %ld\n",
		 i2o_dev->lct_data.tid, channel, le32_to_cpu(id),
		 (long unsigned int)le64_to_cpu(lun));

	return 0;

err:
	scsi_remove_device(scsi_dev);
	return rc;
};

static const char *i2o_scsi_info(struct Scsi_Host *SChost)
{
	struct i2o_scsi_host *hostdata;
	hostdata = (struct i2o_scsi_host *)SChost->hostdata;
	return hostdata->iop->name;
}

static int i2o_scsi_reply(struct i2o_controller *c, u32 m,
			  struct i2o_message *msg)
{
	struct scsi_cmnd *cmd;
	u32 error;
	struct device *dev;

	cmd = i2o_cntxt_list_get(c, le32_to_cpu(msg->u.s.tcntxt));
	if (unlikely(!cmd)) {
		osm_err("NULL reply received!\n");
		return -1;
	}

	error = le32_to_cpu(msg->body[0]);

	osm_debug("Completed %0x%p\n", cmd);

	cmd->result = error & 0xff;
	if (cmd->result)
		memcpy(cmd->sense_buffer, &msg->body[3],
		       min(SCSI_SENSE_BUFFERSIZE, 40));

	
	if ((error >> 8) & 0xff)
		osm_err("SCSI error %08x\n", error);

	dev = &c->pdev->dev;

	scsi_dma_unmap(cmd);

	cmd->scsi_done(cmd);

	return 1;
};

static void i2o_scsi_notify_device_add(struct i2o_device *i2o_dev)
{
	switch (i2o_dev->lct_data.class_id) {
	case I2O_CLASS_EXECUTIVE:
	case I2O_CLASS_RANDOM_BLOCK_STORAGE:
		i2o_scsi_probe(&i2o_dev->device);
		break;

	default:
		break;
	}
};

static void i2o_scsi_notify_device_remove(struct i2o_device *i2o_dev)
{
	switch (i2o_dev->lct_data.class_id) {
	case I2O_CLASS_EXECUTIVE:
	case I2O_CLASS_RANDOM_BLOCK_STORAGE:
		i2o_scsi_remove(&i2o_dev->device);
		break;

	default:
		break;
	}
};

static void i2o_scsi_notify_controller_add(struct i2o_controller *c)
{
	struct i2o_scsi_host *i2o_shost;
	int rc;

	i2o_shost = i2o_scsi_host_alloc(c);
	if (IS_ERR(i2o_shost)) {
		osm_err("Could not initialize SCSI host\n");
		return;
	}

	rc = scsi_add_host(i2o_shost->scsi_host, &c->device);
	if (rc) {
		osm_err("Could not add SCSI host\n");
		scsi_host_put(i2o_shost->scsi_host);
		return;
	}

	c->driver_data[i2o_scsi_driver.context] = i2o_shost;

	osm_debug("new I2O SCSI host added\n");
};

static void i2o_scsi_notify_controller_remove(struct i2o_controller *c)
{
	struct i2o_scsi_host *i2o_shost;
	i2o_shost = i2o_scsi_get_host(c);
	if (!i2o_shost)
		return;

	c->driver_data[i2o_scsi_driver.context] = NULL;

	scsi_remove_host(i2o_shost->scsi_host);
	scsi_host_put(i2o_shost->scsi_host);
	osm_debug("I2O SCSI host removed\n");
};

static struct i2o_driver i2o_scsi_driver = {
	.name = OSM_NAME,
	.reply = i2o_scsi_reply,
	.classes = i2o_scsi_class_id,
	.notify_device_add = i2o_scsi_notify_device_add,
	.notify_device_remove = i2o_scsi_notify_device_remove,
	.notify_controller_add = i2o_scsi_notify_controller_add,
	.notify_controller_remove = i2o_scsi_notify_controller_remove,
	.driver = {
		   .probe = i2o_scsi_probe,
		   .remove = i2o_scsi_remove,
		   },
};


static int i2o_scsi_queuecommand_lck(struct scsi_cmnd *SCpnt,
				 void (*done) (struct scsi_cmnd *))
{
	struct i2o_controller *c;
	struct i2o_device *i2o_dev;
	int tid;
	struct i2o_message *msg;
	u32 scsi_flags = 0x20a00000;
	u32 sgl_offset;
	u32 *mptr;
	u32 cmd = I2O_CMD_SCSI_EXEC << 24;
	int rc = 0;

	i2o_dev = SCpnt->device->hostdata;

	SCpnt->scsi_done = done;

	if (unlikely(!i2o_dev)) {
		osm_warn("no I2O device in request\n");
		SCpnt->result = DID_NO_CONNECT << 16;
		done(SCpnt);
		goto exit;
	}
	c = i2o_dev->iop;
	tid = i2o_dev->lct_data.tid;

	osm_debug("qcmd: Tid = %03x\n", tid);
	osm_debug("Real scsi messages.\n");

	switch (SCpnt->sc_data_direction) {
	case PCI_DMA_NONE:
		
		sgl_offset = SGL_OFFSET_0;
		break;

	case PCI_DMA_TODEVICE:
		
		scsi_flags |= 0x80000000;
		sgl_offset = SGL_OFFSET_10;
		break;

	case PCI_DMA_FROMDEVICE:
		
		scsi_flags |= 0x40000000;
		sgl_offset = SGL_OFFSET_10;
		break;

	default:
		
		SCpnt->result = DID_NO_CONNECT << 16;
		done(SCpnt);
		goto exit;
	}


	msg = i2o_msg_get(c);
	if (IS_ERR(msg)) {
		rc = SCSI_MLQUEUE_HOST_BUSY;
		goto exit;
	}

	mptr = &msg->body[0];

#if 0 
#ifdef CONFIG_I2O_EXT_ADAPTEC
	if (c->adaptec) {
		u32 adpt_flags = 0;

		if (SCpnt->sc_request && SCpnt->sc_request->upper_private_data) {
			i2o_sg_io_hdr_t __user *usr_ptr =
			    ((Sg_request *) (SCpnt->sc_request->
					     upper_private_data))->header.
			    usr_ptr;

			if (usr_ptr)
				get_user(adpt_flags, &usr_ptr->flags);
		}

		switch (i2o_dev->lct_data.class_id) {
		case I2O_CLASS_EXECUTIVE:
		case I2O_CLASS_RANDOM_BLOCK_STORAGE:
			
			adpt_flags ^= I2O_DPT_SG_FLAG_INTERPRET;
			break;

		default:
			break;
		}

		if (sgl_offset == SGL_OFFSET_10)
			sgl_offset = SGL_OFFSET_12;
		cmd = I2O_CMD_PRIVATE << 24;
		*mptr++ = cpu_to_le32(I2O_VENDOR_DPT << 16 | I2O_CMD_SCSI_EXEC);
		*mptr++ = cpu_to_le32(adpt_flags | tid);
	}
#endif
#endif

	msg->u.head[1] = cpu_to_le32(cmd | HOST_TID << 12 | tid);
	msg->u.s.icntxt = cpu_to_le32(i2o_scsi_driver.context);

	
	msg->u.s.tcntxt = cpu_to_le32(i2o_cntxt_list_add(c, SCpnt));


	

	*mptr++ = cpu_to_le32(scsi_flags | SCpnt->cmd_len);

	
	memcpy(mptr, SCpnt->cmnd, 16);
	mptr += 4;

	if (sgl_offset != SGL_OFFSET_0) {
		
		*mptr++ = cpu_to_le32(scsi_bufflen(SCpnt));

		

		if (scsi_sg_count(SCpnt)) {
			if (!i2o_dma_map_sg(c, scsi_sglist(SCpnt),
					    scsi_sg_count(SCpnt),
					    SCpnt->sc_data_direction, &mptr))
				goto nomem;
		}
	}

	
	msg->u.head[0] =
	    cpu_to_le32(I2O_MESSAGE_SIZE(mptr - &msg->u.head[0]) | sgl_offset);

	
	i2o_msg_post(c, msg);

	osm_debug("Issued %0x%p\n", SCpnt);

	return 0;

      nomem:
	rc = -ENOMEM;
	i2o_msg_nop(c, msg);

      exit:
	return rc;
}

static DEF_SCSI_QCMD(i2o_scsi_queuecommand)

static int i2o_scsi_abort(struct scsi_cmnd *SCpnt)
{
	struct i2o_device *i2o_dev;
	struct i2o_controller *c;
	struct i2o_message *msg;
	int tid;
	int status = FAILED;

	osm_warn("Aborting command block.\n");

	i2o_dev = SCpnt->device->hostdata;
	c = i2o_dev->iop;
	tid = i2o_dev->lct_data.tid;

	msg = i2o_msg_get_wait(c, I2O_TIMEOUT_MESSAGE_GET);
	if (IS_ERR(msg))
		return SCSI_MLQUEUE_HOST_BUSY;

	msg->u.head[0] = cpu_to_le32(FIVE_WORD_MSG_SIZE | SGL_OFFSET_0);
	msg->u.head[1] =
	    cpu_to_le32(I2O_CMD_SCSI_ABORT << 24 | HOST_TID << 12 | tid);
	msg->body[0] = cpu_to_le32(i2o_cntxt_list_get_ptr(c, SCpnt));

	if (!i2o_msg_post_wait(c, msg, I2O_TIMEOUT_SCSI_SCB_ABORT))
		status = SUCCESS;

	return status;
}


static int i2o_scsi_bios_param(struct scsi_device *sdev,
			       struct block_device *dev, sector_t capacity,
			       int *ip)
{
	int size;

	size = capacity;
	ip[0] = 64;		
	ip[1] = 32;		
	if ((ip[2] = size >> 11) > 1024) {	
		ip[0] = 255;	
		ip[1] = 63;	
		ip[2] = size / (255 * 63);	
	}
	return 0;
}

static struct scsi_host_template i2o_scsi_host_template = {
	.proc_name = OSM_NAME,
	.name = OSM_DESCRIPTION,
	.info = i2o_scsi_info,
	.queuecommand = i2o_scsi_queuecommand,
	.eh_abort_handler = i2o_scsi_abort,
	.bios_param = i2o_scsi_bios_param,
	.can_queue = I2O_SCSI_CAN_QUEUE,
	.sg_tablesize = 8,
	.cmd_per_lun = 6,
	.use_clustering = ENABLE_CLUSTERING,
};

static int __init i2o_scsi_init(void)
{
	int rc;

	printk(KERN_INFO OSM_DESCRIPTION " v" OSM_VERSION "\n");

	
	rc = i2o_driver_register(&i2o_scsi_driver);
	if (rc) {
		osm_err("Could not register SCSI driver\n");
		return rc;
	}

	return 0;
};

static void __exit i2o_scsi_exit(void)
{
	
	i2o_driver_unregister(&i2o_scsi_driver);
};

MODULE_AUTHOR("Red Hat Software");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(OSM_DESCRIPTION);
MODULE_VERSION(OSM_VERSION);

module_init(i2o_scsi_init);
module_exit(i2o_scsi_exit);
