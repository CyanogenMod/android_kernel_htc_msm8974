/*
 *	Block OSM
 *
 * 	Copyright (C) 1999-2002	Red Hat Software
 *
 *	Written by Alan Cox, Building Number Three Ltd
 *
 *	This program is free software; you can redistribute it and/or modify it
 *	under the terms of the GNU General Public License as published by the
 *	Free Software Foundation; either version 2 of the License, or (at your
 *	option) any later version.
 *
 *	This program is distributed in the hope that it will be useful, but
 *	WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *	General Public License for more details.
 *
 *	For the purpose of avoiding doubt the preferred form of the work
 *	for making modifications shall be a standards compliant form such
 *	gzipped tar and not one requiring a proprietary or patent encumbered
 *	tool to unpack.
 *
 *	Fixes/additions:
 *		Steve Ralston:
 *			Multiple device handling error fixes,
 *			Added a queue depth.
 *		Alan Cox:
 *			FC920 has an rmw bug. Dont or in the end marker.
 *			Removed queue walk, fixed for 64bitness.
 *			Rewrote much of the code over time
 *			Added indirect block lists
 *			Handle 64K limits on many controllers
 *			Don't use indirects on the Promise (breaks)
 *			Heavily chop down the queue depths
 *		Deepak Saxena:
 *			Independent queues per IOP
 *			Support for dynamic device creation/deletion
 *			Code cleanup
 *	    		Support for larger I/Os through merge* functions
 *			(taken from DAC960 driver)
 *		Boji T Kannanthanam:
 *			Set the I2O Block devices to be detected in increasing
 *			order of TIDs during boot.
 *			Search and set the I2O block device that we boot off
 *			from as the first device to be claimed (as /dev/i2o/hda)
 *			Properly attach/detach I2O gendisk structure from the
 *			system gendisk list. The I2O block devices now appear in
 *			/proc/partitions.
 *		Markus Lidel <Markus.Lidel@shadowconnect.com>:
 *			Minor bugfixes for 2.6.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2o.h>
#include <linux/mutex.h>

#include <linux/mempool.h>

#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>

#include <scsi/scsi.h>

#include "i2o_block.h"

#define OSM_NAME	"block-osm"
#define OSM_VERSION	"1.325"
#define OSM_DESCRIPTION	"I2O Block Device OSM"

static DEFINE_MUTEX(i2o_block_mutex);
static struct i2o_driver i2o_block_driver;

static struct i2o_block_mempool i2o_blk_req_pool;

static struct i2o_class_id i2o_block_class_id[] = {
	{I2O_CLASS_RANDOM_BLOCK_STORAGE},
	{I2O_CLASS_END}
};

static void i2o_block_device_free(struct i2o_block_device *dev)
{
	blk_cleanup_queue(dev->gd->queue);

	put_disk(dev->gd);

	kfree(dev);
};

static int i2o_block_remove(struct device *dev)
{
	struct i2o_device *i2o_dev = to_i2o_device(dev);
	struct i2o_block_device *i2o_blk_dev = dev_get_drvdata(dev);

	osm_info("device removed (TID: %03x): %s\n", i2o_dev->lct_data.tid,
		 i2o_blk_dev->gd->disk_name);

	i2o_event_register(i2o_dev, &i2o_block_driver, 0, 0);

	del_gendisk(i2o_blk_dev->gd);

	dev_set_drvdata(dev, NULL);

	i2o_device_claim_release(i2o_dev);

	i2o_block_device_free(i2o_blk_dev);

	return 0;
};

static int i2o_block_device_flush(struct i2o_device *dev)
{
	struct i2o_message *msg;

	msg = i2o_msg_get_wait(dev->iop, I2O_TIMEOUT_MESSAGE_GET);
	if (IS_ERR(msg))
		return PTR_ERR(msg);

	msg->u.head[0] = cpu_to_le32(FIVE_WORD_MSG_SIZE | SGL_OFFSET_0);
	msg->u.head[1] =
	    cpu_to_le32(I2O_CMD_BLOCK_CFLUSH << 24 | HOST_TID << 12 | dev->
			lct_data.tid);
	msg->body[0] = cpu_to_le32(60 << 16);
	osm_debug("Flushing...\n");

	return i2o_msg_post_wait(dev->iop, msg, 60);
};

static int i2o_block_device_mount(struct i2o_device *dev, u32 media_id)
{
	struct i2o_message *msg;

	msg = i2o_msg_get_wait(dev->iop, I2O_TIMEOUT_MESSAGE_GET);
	if (IS_ERR(msg))
		return PTR_ERR(msg);

	msg->u.head[0] = cpu_to_le32(FIVE_WORD_MSG_SIZE | SGL_OFFSET_0);
	msg->u.head[1] =
	    cpu_to_le32(I2O_CMD_BLOCK_MMOUNT << 24 | HOST_TID << 12 | dev->
			lct_data.tid);
	msg->body[0] = cpu_to_le32(-1);
	msg->body[1] = cpu_to_le32(0x00000000);
	osm_debug("Mounting...\n");

	return i2o_msg_post_wait(dev->iop, msg, 2);
};

static int i2o_block_device_lock(struct i2o_device *dev, u32 media_id)
{
	struct i2o_message *msg;

	msg = i2o_msg_get_wait(dev->iop, I2O_TIMEOUT_MESSAGE_GET);
	if (IS_ERR(msg))
		return PTR_ERR(msg);

	msg->u.head[0] = cpu_to_le32(FIVE_WORD_MSG_SIZE | SGL_OFFSET_0);
	msg->u.head[1] =
	    cpu_to_le32(I2O_CMD_BLOCK_MLOCK << 24 | HOST_TID << 12 | dev->
			lct_data.tid);
	msg->body[0] = cpu_to_le32(-1);
	osm_debug("Locking...\n");

	return i2o_msg_post_wait(dev->iop, msg, 2);
};

static int i2o_block_device_unlock(struct i2o_device *dev, u32 media_id)
{
	struct i2o_message *msg;

	msg = i2o_msg_get_wait(dev->iop, I2O_TIMEOUT_MESSAGE_GET);
	if (IS_ERR(msg))
		return PTR_ERR(msg);

	msg->u.head[0] = cpu_to_le32(FIVE_WORD_MSG_SIZE | SGL_OFFSET_0);
	msg->u.head[1] =
	    cpu_to_le32(I2O_CMD_BLOCK_MUNLOCK << 24 | HOST_TID << 12 | dev->
			lct_data.tid);
	msg->body[0] = cpu_to_le32(media_id);
	osm_debug("Unlocking...\n");

	return i2o_msg_post_wait(dev->iop, msg, 2);
};

static int i2o_block_device_power(struct i2o_block_device *dev, u8 op)
{
	struct i2o_device *i2o_dev = dev->i2o_dev;
	struct i2o_controller *c = i2o_dev->iop;
	struct i2o_message *msg;
	int rc;

	msg = i2o_msg_get_wait(c, I2O_TIMEOUT_MESSAGE_GET);
	if (IS_ERR(msg))
		return PTR_ERR(msg);

	msg->u.head[0] = cpu_to_le32(FOUR_WORD_MSG_SIZE | SGL_OFFSET_0);
	msg->u.head[1] =
	    cpu_to_le32(I2O_CMD_BLOCK_POWER << 24 | HOST_TID << 12 | i2o_dev->
			lct_data.tid);
	msg->body[0] = cpu_to_le32(op << 24);
	osm_debug("Power...\n");

	rc = i2o_msg_post_wait(c, msg, 60);
	if (!rc)
		dev->power = op;

	return rc;
};

static inline struct i2o_block_request *i2o_block_request_alloc(void)
{
	struct i2o_block_request *ireq;

	ireq = mempool_alloc(i2o_blk_req_pool.pool, GFP_ATOMIC);
	if (!ireq)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&ireq->queue);
	sg_init_table(ireq->sg_table, I2O_MAX_PHYS_SEGMENTS);

	return ireq;
};

static inline void i2o_block_request_free(struct i2o_block_request *ireq)
{
	mempool_free(ireq, i2o_blk_req_pool.pool);
};

static inline int i2o_block_sglist_alloc(struct i2o_controller *c,
					 struct i2o_block_request *ireq,
					 u32 ** mptr)
{
	int nents;
	enum dma_data_direction direction;

	ireq->dev = &c->pdev->dev;
	nents = blk_rq_map_sg(ireq->req->q, ireq->req, ireq->sg_table);

	if (rq_data_dir(ireq->req) == READ)
		direction = PCI_DMA_FROMDEVICE;
	else
		direction = PCI_DMA_TODEVICE;

	ireq->sg_nents = nents;

	return i2o_dma_map_sg(c, ireq->sg_table, nents, direction, mptr);
};

static inline void i2o_block_sglist_free(struct i2o_block_request *ireq)
{
	enum dma_data_direction direction;

	if (rq_data_dir(ireq->req) == READ)
		direction = PCI_DMA_FROMDEVICE;
	else
		direction = PCI_DMA_TODEVICE;

	dma_unmap_sg(ireq->dev, ireq->sg_table, ireq->sg_nents, direction);
};

static int i2o_block_prep_req_fn(struct request_queue *q, struct request *req)
{
	struct i2o_block_device *i2o_blk_dev = q->queuedata;
	struct i2o_block_request *ireq;

	if (unlikely(!i2o_blk_dev)) {
		osm_err("block device already removed\n");
		return BLKPREP_KILL;
	}

	
	if (!req->special) {
		ireq = i2o_block_request_alloc();
		if (IS_ERR(ireq)) {
			osm_debug("unable to allocate i2o_block_request!\n");
			return BLKPREP_DEFER;
		}

		ireq->i2o_blk_dev = i2o_blk_dev;
		req->special = ireq;
		ireq->req = req;
	}
	
	req->cmd_flags |= REQ_DONTPREP;

	return BLKPREP_OK;
};

static void i2o_block_delayed_request_fn(struct work_struct *work)
{
	struct i2o_block_delayed_request *dreq =
		container_of(work, struct i2o_block_delayed_request,
			     work.work);
	struct request_queue *q = dreq->queue;
	unsigned long flags;

	spin_lock_irqsave(q->queue_lock, flags);
	blk_start_queue(q);
	spin_unlock_irqrestore(q->queue_lock, flags);
	kfree(dreq);
};

static void i2o_block_end_request(struct request *req, int error,
				  int nr_bytes)
{
	struct i2o_block_request *ireq = req->special;
	struct i2o_block_device *dev = ireq->i2o_blk_dev;
	struct request_queue *q = req->q;
	unsigned long flags;

	if (blk_end_request(req, error, nr_bytes))
		if (error)
			blk_end_request_all(req, -EIO);

	spin_lock_irqsave(q->queue_lock, flags);

	if (likely(dev)) {
		dev->open_queue_depth--;
		list_del(&ireq->queue);
	}

	blk_start_queue(q);

	spin_unlock_irqrestore(q->queue_lock, flags);

	i2o_block_sglist_free(ireq);
	i2o_block_request_free(ireq);
};

static int i2o_block_reply(struct i2o_controller *c, u32 m,
			   struct i2o_message *msg)
{
	struct request *req;
	int error = 0;

	req = i2o_cntxt_list_get(c, le32_to_cpu(msg->u.s.tcntxt));
	if (unlikely(!req)) {
		osm_err("NULL reply received!\n");
		return -1;
	}


	if ((le32_to_cpu(msg->body[0]) >> 24) != 0) {
		u32 status = le32_to_cpu(msg->body[0]);

		osm_err("TID %03x error status: 0x%02x, detailed status: "
			"0x%04x\n", (le32_to_cpu(msg->u.head[1]) >> 12 & 0xfff),
			status >> 24, status & 0xffff);

		req->errors++;

		error = -EIO;
	}

	i2o_block_end_request(req, error, le32_to_cpu(msg->body[1]));

	return 1;
};

static void i2o_block_event(struct work_struct *work)
{
	struct i2o_event *evt = container_of(work, struct i2o_event, work);
	osm_debug("event received\n");
	kfree(evt);
};

#define	BLOCK_SIZE_528M		1081344
#define	BLOCK_SIZE_1G		2097152
#define	BLOCK_SIZE_21G		4403200
#define	BLOCK_SIZE_42G		8806400
#define	BLOCK_SIZE_84G		17612800

static void i2o_block_biosparam(unsigned long capacity, unsigned short *cyls,
				unsigned char *hds, unsigned char *secs)
{
	unsigned long heads, sectors, cylinders;

	sectors = 63L;		
	if (capacity <= BLOCK_SIZE_528M)
		heads = 16;
	else if (capacity <= BLOCK_SIZE_1G)
		heads = 32;
	else if (capacity <= BLOCK_SIZE_21G)
		heads = 64;
	else if (capacity <= BLOCK_SIZE_42G)
		heads = 128;
	else
		heads = 255;

	cylinders = (unsigned long)capacity / (heads * sectors);

	*cyls = (unsigned short)cylinders;	
	*secs = (unsigned char)sectors;
	*hds = (unsigned char)heads;
}

static int i2o_block_open(struct block_device *bdev, fmode_t mode)
{
	struct i2o_block_device *dev = bdev->bd_disk->private_data;

	if (!dev->i2o_dev)
		return -ENODEV;

	mutex_lock(&i2o_block_mutex);
	if (dev->power > 0x1f)
		i2o_block_device_power(dev, 0x02);

	i2o_block_device_mount(dev->i2o_dev, -1);

	i2o_block_device_lock(dev->i2o_dev, -1);

	osm_debug("Ready.\n");
	mutex_unlock(&i2o_block_mutex);

	return 0;
};

static int i2o_block_release(struct gendisk *disk, fmode_t mode)
{
	struct i2o_block_device *dev = disk->private_data;
	u8 operation;

	if (!dev->i2o_dev)
		return 0;

	mutex_lock(&i2o_block_mutex);
	i2o_block_device_flush(dev->i2o_dev);

	i2o_block_device_unlock(dev->i2o_dev, -1);

	if (dev->flags & (1 << 3 | 1 << 4))	
		operation = 0x21;
	else
		operation = 0x24;

	i2o_block_device_power(dev, operation);
	mutex_unlock(&i2o_block_mutex);

	return 0;
}

static int i2o_block_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	i2o_block_biosparam(get_capacity(bdev->bd_disk),
			    &geo->cylinders, &geo->heads, &geo->sectors);
	return 0;
}

static int i2o_block_ioctl(struct block_device *bdev, fmode_t mode,
			   unsigned int cmd, unsigned long arg)
{
	struct gendisk *disk = bdev->bd_disk;
	struct i2o_block_device *dev = disk->private_data;
	int ret = -ENOTTY;

	

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	mutex_lock(&i2o_block_mutex);
	switch (cmd) {
	case BLKI2OGRSTRAT:
		ret = put_user(dev->rcache, (int __user *)arg);
		break;
	case BLKI2OGWSTRAT:
		ret = put_user(dev->wcache, (int __user *)arg);
		break;
	case BLKI2OSRSTRAT:
		ret = -EINVAL;
		if (arg < 0 || arg > CACHE_SMARTFETCH)
			break;
		dev->rcache = arg;
		ret = 0;
		break;
	case BLKI2OSWSTRAT:
		ret = -EINVAL;
		if (arg != 0
		    && (arg < CACHE_WRITETHROUGH || arg > CACHE_SMARTBACK))
			break;
		dev->wcache = arg;
		ret = 0;
		break;
	}
	mutex_unlock(&i2o_block_mutex);

	return ret;
};

static unsigned int i2o_block_check_events(struct gendisk *disk,
					   unsigned int clearing)
{
	struct i2o_block_device *p = disk->private_data;

	if (p->media_change_flag) {
		p->media_change_flag = 0;
		return DISK_EVENT_MEDIA_CHANGE;
	}
	return 0;
}

static int i2o_block_transfer(struct request *req)
{
	struct i2o_block_device *dev = req->rq_disk->private_data;
	struct i2o_controller *c;
	u32 tid;
	struct i2o_message *msg;
	u32 *mptr;
	struct i2o_block_request *ireq = req->special;
	u32 tcntxt;
	u32 sgl_offset = SGL_OFFSET_8;
	u32 ctl_flags = 0x00000000;
	int rc;
	u32 cmd;

	if (unlikely(!dev->i2o_dev)) {
		osm_err("transfer to removed drive\n");
		rc = -ENODEV;
		goto exit;
	}

	tid = dev->i2o_dev->lct_data.tid;
	c = dev->i2o_dev->iop;

	msg = i2o_msg_get(c);
	if (IS_ERR(msg)) {
		rc = PTR_ERR(msg);
		goto exit;
	}

	tcntxt = i2o_cntxt_list_add(c, req);
	if (!tcntxt) {
		rc = -ENOMEM;
		goto nop_msg;
	}

	msg->u.s.icntxt = cpu_to_le32(i2o_block_driver.context);
	msg->u.s.tcntxt = cpu_to_le32(tcntxt);

	mptr = &msg->body[0];

	if (rq_data_dir(req) == READ) {
		cmd = I2O_CMD_BLOCK_READ << 24;

		switch (dev->rcache) {
		case CACHE_PREFETCH:
			ctl_flags = 0x201F0008;
			break;

		case CACHE_SMARTFETCH:
			if (blk_rq_sectors(req) > 16)
				ctl_flags = 0x201F0008;
			else
				ctl_flags = 0x001F0000;
			break;

		default:
			break;
		}
	} else {
		cmd = I2O_CMD_BLOCK_WRITE << 24;

		switch (dev->wcache) {
		case CACHE_WRITETHROUGH:
			ctl_flags = 0x001F0008;
			break;
		case CACHE_WRITEBACK:
			ctl_flags = 0x001F0010;
			break;
		case CACHE_SMARTBACK:
			if (blk_rq_sectors(req) > 16)
				ctl_flags = 0x001F0004;
			else
				ctl_flags = 0x001F0010;
			break;
		case CACHE_SMARTTHROUGH:
			if (blk_rq_sectors(req) > 16)
				ctl_flags = 0x001F0004;
			else
				ctl_flags = 0x001F0010;
		default:
			break;
		}
	}

#ifdef CONFIG_I2O_EXT_ADAPTEC
	if (c->adaptec) {
		u8 cmd[10];
		u32 scsi_flags;
		u16 hwsec;

		hwsec = queue_logical_block_size(req->q) >> KERNEL_SECTOR_SHIFT;
		memset(cmd, 0, 10);

		sgl_offset = SGL_OFFSET_12;

		msg->u.head[1] =
		    cpu_to_le32(I2O_CMD_PRIVATE << 24 | HOST_TID << 12 | tid);

		*mptr++ = cpu_to_le32(I2O_VENDOR_DPT << 16 | I2O_CMD_SCSI_EXEC);
		*mptr++ = cpu_to_le32(tid);

		if (rq_data_dir(req) == READ) {
			cmd[0] = READ_10;
			scsi_flags = 0x60a0000a;
		} else {
			cmd[0] = WRITE_10;
			scsi_flags = 0xa0a0000a;
		}

		*mptr++ = cpu_to_le32(scsi_flags);

		*((u32 *) & cmd[2]) = cpu_to_be32(blk_rq_pos(req) * hwsec);
		*((u16 *) & cmd[7]) = cpu_to_be16(blk_rq_sectors(req) * hwsec);

		memcpy(mptr, cmd, 10);
		mptr += 4;
		*mptr++ = cpu_to_le32(blk_rq_bytes(req));
	} else
#endif
	{
		msg->u.head[1] = cpu_to_le32(cmd | HOST_TID << 12 | tid);
		*mptr++ = cpu_to_le32(ctl_flags);
		*mptr++ = cpu_to_le32(blk_rq_bytes(req));
		*mptr++ =
		    cpu_to_le32((u32) (blk_rq_pos(req) << KERNEL_SECTOR_SHIFT));
		*mptr++ =
		    cpu_to_le32(blk_rq_pos(req) >> (32 - KERNEL_SECTOR_SHIFT));
	}

	if (!i2o_block_sglist_alloc(c, ireq, &mptr)) {
		rc = -ENOMEM;
		goto context_remove;
	}

	msg->u.head[0] =
	    cpu_to_le32(I2O_MESSAGE_SIZE(mptr - &msg->u.head[0]) | sgl_offset);

	list_add_tail(&ireq->queue, &dev->open_queue);
	dev->open_queue_depth++;

	i2o_msg_post(c, msg);

	return 0;

      context_remove:
	i2o_cntxt_list_remove(c, req);

      nop_msg:
	i2o_msg_nop(c, msg);

      exit:
	return rc;
};

static void i2o_block_request_fn(struct request_queue *q)
{
	struct request *req;

	while ((req = blk_peek_request(q)) != NULL) {
		if (req->cmd_type == REQ_TYPE_FS) {
			struct i2o_block_delayed_request *dreq;
			struct i2o_block_request *ireq = req->special;
			unsigned int queue_depth;

			queue_depth = ireq->i2o_blk_dev->open_queue_depth;

			if (queue_depth < I2O_BLOCK_MAX_OPEN_REQUESTS) {
				if (!i2o_block_transfer(req)) {
					blk_start_request(req);
					continue;
				} else
					osm_info("transfer error\n");
			}

			if (queue_depth)
				break;

			
			dreq = kmalloc(sizeof(*dreq), GFP_ATOMIC);
			if (!dreq)
				continue;

			dreq->queue = q;
			INIT_DELAYED_WORK(&dreq->work,
					  i2o_block_delayed_request_fn);

			if (!queue_delayed_work(i2o_block_driver.event_queue,
						&dreq->work,
						I2O_BLOCK_RETRY_TIME))
				kfree(dreq);
			else {
				blk_stop_queue(q);
				break;
			}
		} else {
			blk_start_request(req);
			__blk_end_request_all(req, -EIO);
		}
	}
};

static const struct block_device_operations i2o_block_fops = {
	.owner = THIS_MODULE,
	.open = i2o_block_open,
	.release = i2o_block_release,
	.ioctl = i2o_block_ioctl,
	.compat_ioctl = i2o_block_ioctl,
	.getgeo = i2o_block_getgeo,
	.check_events = i2o_block_check_events,
};

static struct i2o_block_device *i2o_block_device_alloc(void)
{
	struct i2o_block_device *dev;
	struct gendisk *gd;
	struct request_queue *queue;
	int rc;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		osm_err("Insufficient memory to allocate I2O Block disk.\n");
		rc = -ENOMEM;
		goto exit;
	}

	INIT_LIST_HEAD(&dev->open_queue);
	spin_lock_init(&dev->lock);
	dev->rcache = CACHE_PREFETCH;
	dev->wcache = CACHE_WRITEBACK;

	
	gd = alloc_disk(16);
	if (!gd) {
		osm_err("Insufficient memory to allocate gendisk.\n");
		rc = -ENOMEM;
		goto cleanup_dev;
	}

	
	queue = blk_init_queue(i2o_block_request_fn, &dev->lock);
	if (!queue) {
		osm_err("Insufficient memory to allocate request queue.\n");
		rc = -ENOMEM;
		goto cleanup_queue;
	}

	blk_queue_prep_rq(queue, i2o_block_prep_req_fn);

	gd->major = I2O_MAJOR;
	gd->queue = queue;
	gd->fops = &i2o_block_fops;
	gd->private_data = dev;

	dev->gd = gd;

	return dev;

      cleanup_queue:
	put_disk(gd);

      cleanup_dev:
	kfree(dev);

      exit:
	return ERR_PTR(rc);
};

static int i2o_block_probe(struct device *dev)
{
	struct i2o_device *i2o_dev = to_i2o_device(dev);
	struct i2o_controller *c = i2o_dev->iop;
	struct i2o_block_device *i2o_blk_dev;
	struct gendisk *gd;
	struct request_queue *queue;
	static int unit = 0;
	int rc;
	u64 size;
	u32 blocksize;
	u16 body_size = 4;
	u16 power;
	unsigned short max_sectors;

#ifdef CONFIG_I2O_EXT_ADAPTEC
	if (c->adaptec)
		body_size = 8;
#endif

	if (c->limit_sectors)
		max_sectors = I2O_MAX_SECTORS_LIMITED;
	else
		max_sectors = I2O_MAX_SECTORS;

	
	if (i2o_dev->lct_data.user_tid != 0xfff) {
		osm_debug("skipping used device %03x\n", i2o_dev->lct_data.tid);
		return -ENODEV;
	}

	if (i2o_device_claim(i2o_dev)) {
		osm_warn("Unable to claim device. Installation aborted\n");
		rc = -EFAULT;
		goto exit;
	}

	i2o_blk_dev = i2o_block_device_alloc();
	if (IS_ERR(i2o_blk_dev)) {
		osm_err("could not alloc a new I2O block device");
		rc = PTR_ERR(i2o_blk_dev);
		goto claim_release;
	}

	i2o_blk_dev->i2o_dev = i2o_dev;
	dev_set_drvdata(dev, i2o_blk_dev);

	
	gd = i2o_blk_dev->gd;
	gd->first_minor = unit << 4;
	sprintf(gd->disk_name, "i2o/hd%c", 'a' + unit);
	gd->driverfs_dev = &i2o_dev->device;

	
	queue = gd->queue;
	queue->queuedata = i2o_blk_dev;

	blk_queue_max_hw_sectors(queue, max_sectors);
	blk_queue_max_segments(queue, i2o_sg_tablesize(c, body_size));

	osm_debug("max sectors = %d\n", queue->max_sectors);
	osm_debug("phys segments = %d\n", queue->max_phys_segments);
	osm_debug("max hw segments = %d\n", queue->max_hw_segments);

	if (!i2o_parm_field_get(i2o_dev, 0x0004, 1, &blocksize, 4) ||
	    !i2o_parm_field_get(i2o_dev, 0x0000, 3, &blocksize, 4)) {
		blk_queue_logical_block_size(queue, le32_to_cpu(blocksize));
	} else
		osm_warn("unable to get blocksize of %s\n", gd->disk_name);

	if (!i2o_parm_field_get(i2o_dev, 0x0004, 0, &size, 8) ||
	    !i2o_parm_field_get(i2o_dev, 0x0000, 4, &size, 8)) {
		set_capacity(gd, le64_to_cpu(size) >> KERNEL_SECTOR_SHIFT);
	} else
		osm_warn("could not get size of %s\n", gd->disk_name);

	if (!i2o_parm_field_get(i2o_dev, 0x0000, 2, &power, 2))
		i2o_blk_dev->power = power;

	i2o_event_register(i2o_dev, &i2o_block_driver, 0, 0xffffffff);

	add_disk(gd);

	unit++;

	osm_info("device added (TID: %03x): %s\n", i2o_dev->lct_data.tid,
		 i2o_blk_dev->gd->disk_name);

	return 0;

      claim_release:
	i2o_device_claim_release(i2o_dev);

      exit:
	return rc;
};

static struct i2o_driver i2o_block_driver = {
	.name = OSM_NAME,
	.event = i2o_block_event,
	.reply = i2o_block_reply,
	.classes = i2o_block_class_id,
	.driver = {
		   .probe = i2o_block_probe,
		   .remove = i2o_block_remove,
		   },
};

static int __init i2o_block_init(void)
{
	int rc;
	int size;

	printk(KERN_INFO OSM_DESCRIPTION " v" OSM_VERSION "\n");

	
	size = sizeof(struct i2o_block_request);
	i2o_blk_req_pool.slab = kmem_cache_create("i2o_block_req", size, 0,
						  SLAB_HWCACHE_ALIGN, NULL);
	if (!i2o_blk_req_pool.slab) {
		osm_err("can't init request slab\n");
		rc = -ENOMEM;
		goto exit;
	}

	i2o_blk_req_pool.pool =
		mempool_create_slab_pool(I2O_BLOCK_REQ_MEMPOOL_SIZE,
					 i2o_blk_req_pool.slab);
	if (!i2o_blk_req_pool.pool) {
		osm_err("can't init request mempool\n");
		rc = -ENOMEM;
		goto free_slab;
	}

	
	rc = register_blkdev(I2O_MAJOR, "i2o_block");
	if (rc) {
		osm_err("unable to register block device\n");
		goto free_mempool;
	}
#ifdef MODULE
	osm_info("registered device at major %d\n", I2O_MAJOR);
#endif

	
	rc = i2o_driver_register(&i2o_block_driver);
	if (rc) {
		osm_err("Could not register Block driver\n");
		goto unregister_blkdev;
	}

	return 0;

      unregister_blkdev:
	unregister_blkdev(I2O_MAJOR, "i2o_block");

      free_mempool:
	mempool_destroy(i2o_blk_req_pool.pool);

      free_slab:
	kmem_cache_destroy(i2o_blk_req_pool.slab);

      exit:
	return rc;
};

static void __exit i2o_block_exit(void)
{
	
	i2o_driver_unregister(&i2o_block_driver);

	
	unregister_blkdev(I2O_MAJOR, "i2o_block");

	
	mempool_destroy(i2o_blk_req_pool.pool);
	kmem_cache_destroy(i2o_blk_req_pool.slab);
};

MODULE_AUTHOR("Red Hat");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION(OSM_DESCRIPTION);
MODULE_VERSION(OSM_VERSION);

module_init(i2o_block_init);
module_exit(i2o_block_exit);
