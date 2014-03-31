/*
 * CARMA DATA-FPGA Access Driver
 *
 * Copyright (c) 2009-2011 Ira W. Snyder <iws@ovro.caltech.edu>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */




#include <linux/of_platform.h>
#include <linux/dma-mapping.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/dmaengine.h>
#include <linux/seq_file.h>
#include <linux/highmem.h>
#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kref.h>
#include <linux/io.h>

#include <media/videobuf-dma-sg.h>

#define SYS_IRQ_SOURCE_CTL	0x24
#define SYS_IRQ_OUTPUT_EN	0x28
#define SYS_IRQ_OUTPUT_DATA	0x2C
#define SYS_IRQ_INPUT_DATA	0x30
#define SYS_FPGA_CONFIG_STATUS	0x44

#define IRQ_CORL_DONE		0x10

#define MMAP_REG_VERSION	0x00
#define MMAP_REG_CORL_CONF1	0x08
#define MMAP_REG_CORL_CONF2	0x0C
#define MMAP_REG_STATUS		0x48

#define SYS_FPGA_BLOCK		0xF0000000

#define DATA_FPGA_START		0x400000
#define DATA_FPGA_SIZE		0x80000

static const char drv_name[] = "carma-fpga";

#define NUM_FPGA	4

#define MIN_DATA_BUFS	8
#define MAX_DATA_BUFS	64

struct fpga_info {
	unsigned int num_lag_ram;
	unsigned int blk_size;
};

struct data_buf {
	struct list_head entry;
	struct videobuf_dmabuf vb;
	size_t size;
};

struct fpga_device {
	
	struct miscdevice miscdev;
	struct device *dev;
	struct mutex mutex;

	
	struct kref ref;

	
	struct fpga_info info[NUM_FPGA];
	void __iomem *regs;
	int irq;

	
	resource_size_t phys_addr;
	size_t phys_size;

	
	struct sg_table corl_table;
	unsigned int corl_nents;
	struct dma_chan *chan;

	
	spinlock_t lock;

	
	bool enabled;

	
	wait_queue_head_t wait;
	struct list_head free;
	struct list_head used;
	struct data_buf *inflight;

	
	unsigned int num_dropped;
	unsigned int num_buffers;
	size_t bufsize;
	struct dentry *dbg_entry;
};

struct fpga_reader {
	struct fpga_device *priv;
	struct data_buf *buf;
	off_t buf_start;
};

static void fpga_device_release(struct kref *ref)
{
	struct fpga_device *priv = container_of(ref, struct fpga_device, ref);

	
	mutex_destroy(&priv->mutex);
	kfree(priv);
}


static void data_free_buffer(struct data_buf *buf)
{
	
	if (!buf)
		return;

	
	videobuf_dma_free(&buf->vb);
	kfree(buf);
}

static struct data_buf *data_alloc_buffer(const size_t bytes)
{
	unsigned int nr_pages;
	struct data_buf *buf;
	int ret;

	
	nr_pages = DIV_ROUND_UP(bytes, PAGE_SIZE);

	
	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		goto out_return;

	
	INIT_LIST_HEAD(&buf->entry);
	buf->size = bytes;

	
	videobuf_dma_init(&buf->vb);
	ret = videobuf_dma_init_kernel(&buf->vb, DMA_FROM_DEVICE, nr_pages);
	if (ret)
		goto out_free_buf;

	return buf;

out_free_buf:
	kfree(buf);
out_return:
	return NULL;
}

static void data_free_buffers(struct fpga_device *priv)
{
	struct data_buf *buf, *tmp;

	
	BUG_ON(priv->inflight != NULL);

	list_for_each_entry_safe(buf, tmp, &priv->free, entry) {
		list_del_init(&buf->entry);
		videobuf_dma_unmap(priv->dev, &buf->vb);
		data_free_buffer(buf);
	}

	list_for_each_entry_safe(buf, tmp, &priv->used, entry) {
		list_del_init(&buf->entry);
		videobuf_dma_unmap(priv->dev, &buf->vb);
		data_free_buffer(buf);
	}

	priv->num_buffers = 0;
	priv->bufsize = 0;
}

static int data_alloc_buffers(struct fpga_device *priv)
{
	struct data_buf *buf;
	int i, ret;

	for (i = 0; i < MAX_DATA_BUFS; i++) {

		
		buf = data_alloc_buffer(priv->bufsize);
		if (!buf)
			break;

		
		ret = videobuf_dma_map(priv->dev, &buf->vb);
		if (ret) {
			data_free_buffer(buf);
			break;
		}

		
		list_add_tail(&buf->entry, &priv->free);
		priv->num_buffers++;
	}

	
	if (priv->num_buffers < MIN_DATA_BUFS) {
		dev_err(priv->dev, "Unable to allocate enough data buffers\n");
		data_free_buffers(priv);
		return -ENOMEM;
	}

	
	if (priv->num_buffers < MAX_DATA_BUFS) {
		dev_warn(priv->dev,
			 "Unable to allocate %d buffers, using %d buffers instead\n",
			 MAX_DATA_BUFS, i);
	}

	return 0;
}


static dma_addr_t fpga_start_addr(struct fpga_device *priv, unsigned int fpga)
{
	return priv->phys_addr + 0x400000 + (0x80000 * fpga);
}

static dma_addr_t fpga_block_addr(struct fpga_device *priv, unsigned int fpga,
				  unsigned int blknum)
{
	return fpga_start_addr(priv, fpga) + (0x10000 * (1 + blknum));
}

#define REG_BLOCK_SIZE	(32 * 4)

static int data_setup_corl_table(struct fpga_device *priv)
{
	struct sg_table *table = &priv->corl_table;
	struct scatterlist *sg;
	struct fpga_info *info;
	int i, j, ret;

	
	priv->corl_nents = (1 + NUM_FPGA) * REG_BLOCK_SIZE;
	for (i = 0; i < NUM_FPGA; i++)
		priv->corl_nents += priv->info[i].num_lag_ram;

	
	ret = sg_alloc_table(table, priv->corl_nents, GFP_KERNEL);
	if (ret) {
		dev_err(priv->dev, "unable to allocate DMA table\n");
		return ret;
	}

	
	sg = table->sgl;
	for (i = 0; i < NUM_FPGA; i++) {
		sg_dma_address(sg) = fpga_start_addr(priv, i);
		sg_dma_len(sg) = REG_BLOCK_SIZE;
		sg = sg_next(sg);
	}

	
	sg_dma_address(sg) = SYS_FPGA_BLOCK;
	sg_dma_len(sg) = REG_BLOCK_SIZE;
	sg = sg_next(sg);

	
	for (i = 0; i < NUM_FPGA; i++) {
		info = &priv->info[i];
		for (j = 0; j < info->num_lag_ram; j++) {
			sg_dma_address(sg) = fpga_block_addr(priv, i, j);
			sg_dma_len(sg) = info->blk_size;
			sg = sg_next(sg);
		}
	}

	return 0;
}


static void fpga_write_reg(struct fpga_device *priv, unsigned int fpga,
			   unsigned int reg, u32 val)
{
	const int fpga_start = DATA_FPGA_START + (fpga * DATA_FPGA_SIZE);
	iowrite32be(val, priv->regs + fpga_start + reg);
}

static u32 fpga_read_reg(struct fpga_device *priv, unsigned int fpga,
			 unsigned int reg)
{
	const int fpga_start = DATA_FPGA_START + (fpga * DATA_FPGA_SIZE);
	return ioread32be(priv->regs + fpga_start + reg);
}

static int data_calculate_bufsize(struct fpga_device *priv)
{
	u32 num_corl, num_lags, num_meta, num_qcnt, num_pack;
	u32 conf1, conf2, version;
	u32 num_lag_ram, blk_size;
	int i;

	
	priv->bufsize = (1 + NUM_FPGA) * REG_BLOCK_SIZE;

	
	for (i = 0; i < NUM_FPGA; i++) {
		version = fpga_read_reg(priv, i, MMAP_REG_VERSION);
		conf1 = fpga_read_reg(priv, i, MMAP_REG_CORL_CONF1);
		conf2 = fpga_read_reg(priv, i, MMAP_REG_CORL_CONF2);

		
		if ((version & 0x000000FF) >= 2) {
			num_corl = (conf1 & 0x000000F0) >> 4;
			num_pack = (conf1 & 0x00000F00) >> 8;
			num_lags = (conf1 & 0x00FFF000) >> 12;
			num_meta = (conf1 & 0x7F000000) >> 24;
			num_qcnt = (conf2 & 0x00000FFF) >> 0;
		} else {
			num_corl = (conf1 & 0x000000F0) >> 4;
			num_pack = 1; 
			num_lags = (conf1 & 0x000FFF00) >> 8;
			num_meta = (conf1 & 0x7FF00000) >> 20;
			num_qcnt = (conf2 & 0x00000FFF) >> 0;
		}

		num_lag_ram = (num_corl + num_pack - 1) / num_pack;
		blk_size = ((num_pack * num_lags) + num_meta + num_qcnt) * 8;

		priv->info[i].num_lag_ram = num_lag_ram;
		priv->info[i].blk_size = blk_size;
		priv->bufsize += num_lag_ram * blk_size;

		dev_dbg(priv->dev, "FPGA %d NUM_CORL: %d\n", i, num_corl);
		dev_dbg(priv->dev, "FPGA %d NUM_PACK: %d\n", i, num_pack);
		dev_dbg(priv->dev, "FPGA %d NUM_LAGS: %d\n", i, num_lags);
		dev_dbg(priv->dev, "FPGA %d NUM_META: %d\n", i, num_meta);
		dev_dbg(priv->dev, "FPGA %d NUM_QCNT: %d\n", i, num_qcnt);
		dev_dbg(priv->dev, "FPGA %d BLK_SIZE: %d\n", i, blk_size);
	}

	dev_dbg(priv->dev, "TOTAL BUFFER SIZE: %zu bytes\n", priv->bufsize);
	return 0;
}


static void data_disable_interrupts(struct fpga_device *priv)
{
	
	iowrite32be(0x2F, priv->regs + SYS_IRQ_SOURCE_CTL);
}

static void data_enable_interrupts(struct fpga_device *priv)
{
	
	fpga_write_reg(priv, 0, MMAP_REG_STATUS, 0x0);
	fpga_write_reg(priv, 1, MMAP_REG_STATUS, 0x0);
	fpga_write_reg(priv, 2, MMAP_REG_STATUS, 0x0);
	fpga_write_reg(priv, 3, MMAP_REG_STATUS, 0x0);

	
	fpga_read_reg(priv, 0, MMAP_REG_STATUS);
	fpga_read_reg(priv, 1, MMAP_REG_STATUS);
	fpga_read_reg(priv, 2, MMAP_REG_STATUS);
	fpga_read_reg(priv, 3, MMAP_REG_STATUS);

	
	iowrite32be(0x3F, priv->regs + SYS_IRQ_SOURCE_CTL);
}

static void data_dma_cb(void *data)
{
	struct fpga_device *priv = data;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);

	
	BUG_ON(priv->inflight == NULL);

	
	list_move_tail(&priv->inflight->entry, &priv->used);
	priv->inflight = NULL;

	if (priv->enabled)
		data_enable_interrupts(priv);

	spin_unlock_irqrestore(&priv->lock, flags);

	wake_up(&priv->wait);
}

static int data_submit_dma(struct fpga_device *priv, struct data_buf *buf)
{
	struct scatterlist *dst_sg, *src_sg;
	unsigned int dst_nents, src_nents;
	struct dma_chan *chan = priv->chan;
	struct dma_async_tx_descriptor *tx;
	dma_cookie_t cookie;
	dma_addr_t dst, src;

	dst_sg = buf->vb.sglist;
	dst_nents = buf->vb.sglen;

	src_sg = priv->corl_table.sgl;
	src_nents = priv->corl_nents;


	
	tx = chan->device->device_prep_dma_sg(chan,
					      dst_sg, dst_nents,
					      src_sg, src_nents,
					      0);
	if (!tx) {
		dev_err(priv->dev, "unable to prep scatterlist DMA\n");
		return -ENOMEM;
	}

	
	cookie = tx->tx_submit(tx);
	if (dma_submit_error(cookie)) {
		dev_err(priv->dev, "unable to submit scatterlist DMA\n");
		return -ENOMEM;
	}

	
	dst = sg_dma_address(dst_sg) + (NUM_FPGA * REG_BLOCK_SIZE);
	src = SYS_FPGA_BLOCK;
	tx = chan->device->device_prep_dma_memcpy(chan, dst, src,
						  REG_BLOCK_SIZE,
						  DMA_PREP_INTERRUPT);
	if (!tx) {
		dev_err(priv->dev, "unable to prep SYS-FPGA DMA\n");
		return -ENOMEM;
	}

	
	tx->callback = data_dma_cb;
	tx->callback_param = priv;

	
	cookie = tx->tx_submit(tx);
	if (dma_submit_error(cookie)) {
		dev_err(priv->dev, "unable to submit SYS-FPGA DMA\n");
		return -ENOMEM;
	}

	return 0;
}

#define CORL_DONE	0x1
#define CORL_ERR	0x2

static irqreturn_t data_irq(int irq, void *dev_id)
{
	struct fpga_device *priv = dev_id;
	bool submitted = false;
	struct data_buf *buf;
	u32 status;
	int i;

	
	for (i = 0; i < 4; i++) {
		status = fpga_read_reg(priv, i, MMAP_REG_STATUS);
		if (!(status & (CORL_DONE | CORL_ERR))) {
			dev_err(priv->dev, "spurious irq detected (FPGA)\n");
			return IRQ_NONE;
		}
	}

	
	status = ioread32be(priv->regs + SYS_IRQ_INPUT_DATA);
	if (status & IRQ_CORL_DONE) {
		dev_err(priv->dev, "spurious irq detected (IRQ)\n");
		return IRQ_NONE;
	}

	spin_lock(&priv->lock);

	BUG_ON(priv->inflight != NULL);

	
	data_disable_interrupts(priv);

	
	if (list_empty(&priv->free)) {
		priv->num_dropped++;
		goto out;
	}

	buf = list_first_entry(&priv->free, struct data_buf, entry);
	list_del_init(&buf->entry);
	BUG_ON(buf->size != priv->bufsize);

	
	if (data_submit_dma(priv, buf)) {
		dev_err(priv->dev, "Unable to setup DMA transfer\n");
		list_move_tail(&buf->entry, &priv->free);
		goto out;
	}

	
	priv->inflight = buf;
	submitted = true;

	
	dma_async_memcpy_issue_pending(priv->chan);

out:
	
	if (!submitted)
		data_enable_interrupts(priv);

	spin_unlock(&priv->lock);
	return IRQ_HANDLED;
}


static int data_device_enable(struct fpga_device *priv)
{
	bool enabled;
	u32 val;
	int ret;

	
	spin_lock_irq(&priv->lock);
	enabled = priv->enabled;
	spin_unlock_irq(&priv->lock);
	if (enabled)
		return 0;

	
	val = ioread32be(priv->regs + SYS_FPGA_CONFIG_STATUS);
	if (!(val & (1 << 18))) {
		dev_err(priv->dev, "DATA-FPGAs are not enabled\n");
		return -ENODATA;
	}

	
	ret = data_calculate_bufsize(priv);
	if (ret) {
		dev_err(priv->dev, "unable to calculate buffer size\n");
		goto out_error;
	}

	
	ret = data_alloc_buffers(priv);
	if (ret) {
		dev_err(priv->dev, "unable to allocate buffers\n");
		goto out_error;
	}

	
	ret = data_setup_corl_table(priv);
	if (ret) {
		dev_err(priv->dev, "unable to setup correlation DMA table\n");
		goto out_error;
	}

	
	data_disable_interrupts(priv);

	
	ret = request_irq(priv->irq, data_irq, IRQF_SHARED, drv_name, priv);
	if (ret) {
		dev_err(priv->dev, "unable to request IRQ handler\n");
		goto out_error;
	}

	
	spin_lock_irq(&priv->lock);
	priv->enabled = true;
	spin_unlock_irq(&priv->lock);

	
	data_enable_interrupts(priv);
	return 0;

out_error:
	sg_free_table(&priv->corl_table);
	priv->corl_nents = 0;

	data_free_buffers(priv);
	return ret;
}

static int data_device_disable(struct fpga_device *priv)
{
	spin_lock_irq(&priv->lock);

	
	if (!priv->enabled) {
		spin_unlock_irq(&priv->lock);
		return 0;
	}

	priv->enabled = false;

	
	data_disable_interrupts(priv);

	
	while (priv->inflight != NULL) {
		spin_unlock_irq(&priv->lock);
		wait_event(priv->wait, priv->inflight == NULL);
		spin_lock_irq(&priv->lock);
	}

	spin_unlock_irq(&priv->lock);

	
	free_irq(priv->irq, priv);

	
	sg_free_table(&priv->corl_table);
	priv->corl_nents = 0;

	
	data_free_buffers(priv);
	return 0;
}

#ifdef CONFIG_DEBUG_FS

static unsigned int list_num_entries(struct list_head *list)
{
	struct list_head *entry;
	unsigned int ret = 0;

	list_for_each(entry, list)
		ret++;

	return ret;
}

static int data_debug_show(struct seq_file *f, void *offset)
{
	struct fpga_device *priv = f->private;

	spin_lock_irq(&priv->lock);

	seq_printf(f, "enabled: %d\n", priv->enabled);
	seq_printf(f, "bufsize: %d\n", priv->bufsize);
	seq_printf(f, "num_buffers: %d\n", priv->num_buffers);
	seq_printf(f, "num_free: %d\n", list_num_entries(&priv->free));
	seq_printf(f, "inflight: %d\n", priv->inflight != NULL);
	seq_printf(f, "num_used: %d\n", list_num_entries(&priv->used));
	seq_printf(f, "num_dropped: %d\n", priv->num_dropped);

	spin_unlock_irq(&priv->lock);
	return 0;
}

static int data_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, data_debug_show, inode->i_private);
}

static const struct file_operations data_debug_fops = {
	.owner		= THIS_MODULE,
	.open		= data_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int data_debugfs_init(struct fpga_device *priv)
{
	priv->dbg_entry = debugfs_create_file(drv_name, S_IRUGO, NULL, priv,
					      &data_debug_fops);
	if (IS_ERR(priv->dbg_entry))
		return PTR_ERR(priv->dbg_entry);

	return 0;
}

static void data_debugfs_exit(struct fpga_device *priv)
{
	debugfs_remove(priv->dbg_entry);
}

#else

static inline int data_debugfs_init(struct fpga_device *priv)
{
	return 0;
}

static inline void data_debugfs_exit(struct fpga_device *priv)
{
}

#endif	


static ssize_t data_en_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct fpga_device *priv = dev_get_drvdata(dev);
	int ret;

	spin_lock_irq(&priv->lock);
	ret = snprintf(buf, PAGE_SIZE, "%u\n", priv->enabled);
	spin_unlock_irq(&priv->lock);

	return ret;
}

static ssize_t data_en_set(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct fpga_device *priv = dev_get_drvdata(dev);
	unsigned long enable;
	int ret;

	ret = strict_strtoul(buf, 0, &enable);
	if (ret) {
		dev_err(priv->dev, "unable to parse enable input\n");
		return -EINVAL;
	}

	
	ret = mutex_lock_interruptible(&priv->mutex);
	if (ret)
		return ret;

	if (enable)
		ret = data_device_enable(priv);
	else
		ret = data_device_disable(priv);

	if (ret) {
		dev_err(priv->dev, "device %s failed\n",
			enable ? "enable" : "disable");
		count = ret;
		goto out_unlock;
	}

out_unlock:
	mutex_unlock(&priv->mutex);
	return count;
}

static DEVICE_ATTR(enable, S_IWUSR | S_IRUGO, data_en_show, data_en_set);

static struct attribute *data_sysfs_attrs[] = {
	&dev_attr_enable.attr,
	NULL,
};

static const struct attribute_group rt_sysfs_attr_group = {
	.attrs = data_sysfs_attrs,
};


static int data_open(struct inode *inode, struct file *filp)
{
	struct fpga_device *priv = container_of(filp->private_data,
						struct fpga_device, miscdev);
	struct fpga_reader *reader;
	int ret;

	
	reader = kzalloc(sizeof(*reader), GFP_KERNEL);
	if (!reader)
		return -ENOMEM;

	reader->priv = priv;
	reader->buf = NULL;

	filp->private_data = reader;
	ret = nonseekable_open(inode, filp);
	if (ret) {
		dev_err(priv->dev, "nonseekable-open failed\n");
		kfree(reader);
		return ret;
	}

	kref_get(&priv->ref);
	return 0;
}

static int data_release(struct inode *inode, struct file *filp)
{
	struct fpga_reader *reader = filp->private_data;
	struct fpga_device *priv = reader->priv;

	
	data_free_buffer(reader->buf);
	kfree(reader);
	filp->private_data = NULL;

	
	kref_put(&priv->ref, fpga_device_release);
	return 0;
}

static ssize_t data_read(struct file *filp, char __user *ubuf, size_t count,
			 loff_t *f_pos)
{
	struct fpga_reader *reader = filp->private_data;
	struct fpga_device *priv = reader->priv;
	struct list_head *used = &priv->used;
	bool drop_buffer = false;
	struct data_buf *dbuf;
	size_t avail;
	void *data;
	int ret;

	
	if (reader->buf) {
		dbuf = reader->buf;
		goto have_buffer;
	}

	spin_lock_irq(&priv->lock);

	
	while (list_empty(used)) {
		spin_unlock_irq(&priv->lock);

		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;

		ret = wait_event_interruptible(priv->wait, !list_empty(used));
		if (ret)
			return ret;

		spin_lock_irq(&priv->lock);
	}

	
	dbuf = list_first_entry(used, struct data_buf, entry);
	list_del_init(&dbuf->entry);

	spin_unlock_irq(&priv->lock);

	
	videobuf_dma_unmap(priv->dev, &dbuf->vb);

	
	reader->buf = dbuf;
	reader->buf_start = 0;

have_buffer:
	
	avail = dbuf->size - reader->buf_start;
	data = dbuf->vb.vaddr + reader->buf_start;

	
	count = min(count, avail);

	
	if (copy_to_user(ubuf, data, count))
		return -EFAULT;

	
	avail -= count;

	if (avail > 0) {
		reader->buf_start += count;
		reader->buf = dbuf;
		return count;
	}

	ret = videobuf_dma_map(priv->dev, &dbuf->vb);
	if (ret) {
		dev_err(priv->dev, "unable to remap buffer for DMA\n");
		return -EFAULT;
	}

	
	spin_lock_irq(&priv->lock);

	
	reader->buf = NULL;

	if (!priv->enabled || dbuf->size != priv->bufsize) {
		drop_buffer = true;
		goto out_unlock;
	}

	
	list_add_tail(&dbuf->entry, &priv->free);

out_unlock:
	spin_unlock_irq(&priv->lock);

	if (drop_buffer) {
		videobuf_dma_unmap(priv->dev, &dbuf->vb);
		data_free_buffer(dbuf);
	}

	return count;
}

static unsigned int data_poll(struct file *filp, struct poll_table_struct *tbl)
{
	struct fpga_reader *reader = filp->private_data;
	struct fpga_device *priv = reader->priv;
	unsigned int mask = 0;

	poll_wait(filp, &priv->wait, tbl);

	if (!list_empty(&priv->used))
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static int data_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct fpga_reader *reader = filp->private_data;
	struct fpga_device *priv = reader->priv;
	unsigned long offset, vsize, psize, addr;

	
	offset = vma->vm_pgoff << PAGE_SHIFT;
	vsize = vma->vm_end - vma->vm_start;
	psize = priv->phys_size - offset;
	addr = (priv->phys_addr + offset) >> PAGE_SHIFT;

	
	if (vsize > psize) {
		dev_err(priv->dev, "requested mmap mapping too large\n");
		return -EINVAL;
	}

	
	vma->vm_flags |= VM_IO | VM_RESERVED;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	return io_remap_pfn_range(vma, vma->vm_start, addr, vsize,
				  vma->vm_page_prot);
}

static const struct file_operations data_fops = {
	.owner		= THIS_MODULE,
	.open		= data_open,
	.release	= data_release,
	.read		= data_read,
	.poll		= data_poll,
	.mmap		= data_mmap,
	.llseek		= no_llseek,
};


static bool dma_filter(struct dma_chan *chan, void *data)
{
	if (chan->chan_id == 0 && chan->device->dev_id == 0)
		return false;

	return true;
}

static int data_of_probe(struct platform_device *op)
{
	struct device_node *of_node = op->dev.of_node;
	struct device *this_device;
	struct fpga_device *priv;
	struct resource res;
	dma_cap_mask_t mask;
	int ret;

	
	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(&op->dev, "Unable to allocate device private data\n");
		ret = -ENOMEM;
		goto out_return;
	}

	dev_set_drvdata(&op->dev, priv);
	priv->dev = &op->dev;
	kref_init(&priv->ref);
	mutex_init(&priv->mutex);

	dev_set_drvdata(priv->dev, priv);
	spin_lock_init(&priv->lock);
	INIT_LIST_HEAD(&priv->free);
	INIT_LIST_HEAD(&priv->used);
	init_waitqueue_head(&priv->wait);

	
	priv->miscdev.minor = MISC_DYNAMIC_MINOR;
	priv->miscdev.name = drv_name;
	priv->miscdev.fops = &data_fops;

	
	ret = of_address_to_resource(of_node, 0, &res);
	if (ret) {
		dev_err(&op->dev, "Unable to find FPGA physical address\n");
		ret = -ENODEV;
		goto out_free_priv;
	}

	priv->phys_addr = res.start;
	priv->phys_size = resource_size(&res);

	
	priv->regs = of_iomap(of_node, 0);
	if (!priv->regs) {
		dev_err(&op->dev, "Unable to ioremap registers\n");
		ret = -ENOMEM;
		goto out_free_priv;
	}

	dma_cap_zero(mask);
	dma_cap_set(DMA_MEMCPY, mask);
	dma_cap_set(DMA_INTERRUPT, mask);
	dma_cap_set(DMA_SLAVE, mask);
	dma_cap_set(DMA_SG, mask);

	
	priv->chan = dma_request_channel(mask, dma_filter, NULL);
	if (!priv->chan) {
		dev_err(&op->dev, "Unable to request DMA channel\n");
		ret = -ENODEV;
		goto out_unmap_regs;
	}

	
	priv->irq = irq_of_parse_and_map(of_node, 0);
	if (priv->irq == NO_IRQ) {
		dev_err(&op->dev, "Unable to find IRQ line\n");
		ret = -ENODEV;
		goto out_release_dma;
	}

	
	iowrite32be(IRQ_CORL_DONE, priv->regs + SYS_IRQ_OUTPUT_DATA);

	
	ret = misc_register(&priv->miscdev);
	if (ret) {
		dev_err(&op->dev, "Unable to register miscdevice\n");
		goto out_irq_dispose_mapping;
	}

	
	ret = data_debugfs_init(priv);
	if (ret) {
		dev_err(&op->dev, "Unable to create debugfs files\n");
		goto out_misc_deregister;
	}

	
	this_device = priv->miscdev.this_device;
	dev_set_drvdata(this_device, priv);
	ret = sysfs_create_group(&this_device->kobj, &rt_sysfs_attr_group);
	if (ret) {
		dev_err(&op->dev, "Unable to create sysfs files\n");
		goto out_data_debugfs_exit;
	}

	dev_info(&op->dev, "CARMA FPGA Realtime Data Driver Loaded\n");
	return 0;

out_data_debugfs_exit:
	data_debugfs_exit(priv);
out_misc_deregister:
	misc_deregister(&priv->miscdev);
out_irq_dispose_mapping:
	irq_dispose_mapping(priv->irq);
out_release_dma:
	dma_release_channel(priv->chan);
out_unmap_regs:
	iounmap(priv->regs);
out_free_priv:
	kref_put(&priv->ref, fpga_device_release);
out_return:
	return ret;
}

static int data_of_remove(struct platform_device *op)
{
	struct fpga_device *priv = dev_get_drvdata(&op->dev);
	struct device *this_device = priv->miscdev.this_device;

	
	sysfs_remove_group(&this_device->kobj, &rt_sysfs_attr_group);

	
	data_debugfs_exit(priv);

	
	data_device_disable(priv);

	
	misc_deregister(&priv->miscdev);

	
	irq_dispose_mapping(priv->irq);
	dma_release_channel(priv->chan);
	iounmap(priv->regs);

	
	kref_put(&priv->ref, fpga_device_release);
	return 0;
}

static struct of_device_id data_of_match[] = {
	{ .compatible = "carma,carma-fpga", },
	{},
};

static struct platform_driver data_of_driver = {
	.probe		= data_of_probe,
	.remove		= data_of_remove,
	.driver		= {
		.name		= drv_name,
		.of_match_table	= data_of_match,
		.owner		= THIS_MODULE,
	},
};

module_platform_driver(data_of_driver);

MODULE_AUTHOR("Ira W. Snyder <iws@ovro.caltech.edu>");
MODULE_DESCRIPTION("CARMA DATA-FPGA Access Driver");
MODULE_LICENSE("GPL");
