/*
 * ispstat.c
 *
 * TI OMAP3 ISP - Statistics core
 *
 * Copyright (C) 2010 Nokia Corporation
 * Copyright (C) 2009 Texas Instruments, Inc
 *
 * Contacts: David Cohen <dacohen@gmail.com>
 *	     Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *	     Sakari Ailus <sakari.ailus@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "isp.h"

#define IS_COHERENT_BUF(stat)	((stat)->dma_ch >= 0)

#define MAGIC_SIZE		16
#define MAGIC_NUM		0x55

#define AF_EXTRA_DATA		OMAP3ISP_AF_PAXEL_SIZE

/*
 * HACK: H3A modules go to an invalid state after have a SBL overflow. It makes
 * the next buffer to start to be written in the same point where the overflow
 * occurred instead of the configured address. The only known way to make it to
 * go back to a valid state is having a valid buffer processing. Of course it
 * requires at least a doubled buffer size to avoid an access to invalid memory
 * region. But it does not fix everything. It may happen more than one
 * consecutive SBL overflows. In that case, it might be unpredictable how many
 * buffers the allocated memory should fit. For that case, a recover
 * configuration was created. It produces the minimum buffer size for each H3A
 * module and decrease the change for more SBL overflows. This recover state
 * will be enabled every time a SBL overflow occur. As the output buffer size
 * isn't big, it's possible to have an extra size able to fit many recover
 * buffers making it extreamily unlikely to have an access to invalid memory
 * region.
 */
#define NUM_H3A_RECOVER_BUFS	10

#define IS_H3A_AF(stat)		((stat) == &(stat)->isp->isp_af)
#define IS_H3A_AEWB(stat)	((stat) == &(stat)->isp->isp_aewb)
#define IS_H3A(stat)		(IS_H3A_AF(stat) || IS_H3A_AEWB(stat))

static void __isp_stat_buf_sync_magic(struct ispstat *stat,
				      struct ispstat_buffer *buf,
				      u32 buf_size, enum dma_data_direction dir,
				      void (*dma_sync)(struct device *,
					dma_addr_t, unsigned long, size_t,
					enum dma_data_direction))
{
	struct device *dev = stat->isp->dev;
	struct page *pg;
	dma_addr_t dma_addr;
	u32 offset;

	
	pg = vmalloc_to_page(buf->virt_addr);
	dma_addr = pfn_to_dma(dev, page_to_pfn(pg));
	dma_sync(dev, dma_addr, 0, MAGIC_SIZE, dir);

	
	pg = vmalloc_to_page(buf->virt_addr + buf_size);
	dma_addr = pfn_to_dma(dev, page_to_pfn(pg));
	offset = ((u32)buf->virt_addr + buf_size) & ~PAGE_MASK;
	dma_sync(dev, dma_addr, offset, MAGIC_SIZE, dir);
}

static void isp_stat_buf_sync_magic_for_device(struct ispstat *stat,
					       struct ispstat_buffer *buf,
					       u32 buf_size,
					       enum dma_data_direction dir)
{
	if (IS_COHERENT_BUF(stat))
		return;

	__isp_stat_buf_sync_magic(stat, buf, buf_size, dir,
				  dma_sync_single_range_for_device);
}

static void isp_stat_buf_sync_magic_for_cpu(struct ispstat *stat,
					    struct ispstat_buffer *buf,
					    u32 buf_size,
					    enum dma_data_direction dir)
{
	if (IS_COHERENT_BUF(stat))
		return;

	__isp_stat_buf_sync_magic(stat, buf, buf_size, dir,
				  dma_sync_single_range_for_cpu);
}

static int isp_stat_buf_check_magic(struct ispstat *stat,
				    struct ispstat_buffer *buf)
{
	const u32 buf_size = IS_H3A_AF(stat) ?
			     buf->buf_size + AF_EXTRA_DATA : buf->buf_size;
	u8 *w;
	u8 *end;
	int ret = -EINVAL;

	isp_stat_buf_sync_magic_for_cpu(stat, buf, buf_size, DMA_FROM_DEVICE);

	
	for (w = buf->virt_addr, end = w + MAGIC_SIZE; w < end; w++)
		if (likely(*w != MAGIC_NUM))
			ret = 0;

	if (ret) {
		dev_dbg(stat->isp->dev, "%s: beginning magic check does not "
					"match.\n", stat->subdev.name);
		return ret;
	}

	
	for (w = buf->virt_addr + buf_size, end = w + MAGIC_SIZE;
	     w < end; w++) {
		if (unlikely(*w != MAGIC_NUM)) {
			dev_dbg(stat->isp->dev, "%s: endding magic check does "
				"not match.\n", stat->subdev.name);
			return -EINVAL;
		}
	}

	isp_stat_buf_sync_magic_for_device(stat, buf, buf_size,
					   DMA_FROM_DEVICE);

	return 0;
}

static void isp_stat_buf_insert_magic(struct ispstat *stat,
				      struct ispstat_buffer *buf)
{
	const u32 buf_size = IS_H3A_AF(stat) ?
			     stat->buf_size + AF_EXTRA_DATA : stat->buf_size;

	isp_stat_buf_sync_magic_for_cpu(stat, buf, buf_size, DMA_FROM_DEVICE);

	memset(buf->virt_addr, MAGIC_NUM, MAGIC_SIZE);
	memset(buf->virt_addr + buf_size, MAGIC_NUM, MAGIC_SIZE);

	isp_stat_buf_sync_magic_for_device(stat, buf, buf_size,
					   DMA_BIDIRECTIONAL);
}

static void isp_stat_buf_sync_for_device(struct ispstat *stat,
					 struct ispstat_buffer *buf)
{
	if (IS_COHERENT_BUF(stat))
		return;

	dma_sync_sg_for_device(stat->isp->dev, buf->iovm->sgt->sgl,
			       buf->iovm->sgt->nents, DMA_FROM_DEVICE);
}

static void isp_stat_buf_sync_for_cpu(struct ispstat *stat,
				      struct ispstat_buffer *buf)
{
	if (IS_COHERENT_BUF(stat))
		return;

	dma_sync_sg_for_cpu(stat->isp->dev, buf->iovm->sgt->sgl,
			    buf->iovm->sgt->nents, DMA_FROM_DEVICE);
}

static void isp_stat_buf_clear(struct ispstat *stat)
{
	int i;

	for (i = 0; i < STAT_MAX_BUFS; i++)
		stat->buf[i].empty = 1;
}

static struct ispstat_buffer *
__isp_stat_buf_find(struct ispstat *stat, int look_empty)
{
	struct ispstat_buffer *found = NULL;
	int i;

	for (i = 0; i < STAT_MAX_BUFS; i++) {
		struct ispstat_buffer *curr = &stat->buf[i];

		if (curr == stat->locked_buf || curr == stat->active_buf)
			continue;

		
		if (!look_empty && curr->empty)
			continue;

		
		if (curr->empty) {
			found = curr;
			break;
		}

		
		if (!found ||
		    (s32)curr->frame_number - (s32)found->frame_number < 0)
			found = curr;
	}

	return found;
}

static inline struct ispstat_buffer *
isp_stat_buf_find_oldest(struct ispstat *stat)
{
	return __isp_stat_buf_find(stat, 0);
}

static inline struct ispstat_buffer *
isp_stat_buf_find_oldest_or_empty(struct ispstat *stat)
{
	return __isp_stat_buf_find(stat, 1);
}

static int isp_stat_buf_queue(struct ispstat *stat)
{
	if (!stat->active_buf)
		return STAT_NO_BUF;

	do_gettimeofday(&stat->active_buf->ts);

	stat->active_buf->buf_size = stat->buf_size;
	if (isp_stat_buf_check_magic(stat, stat->active_buf)) {
		dev_dbg(stat->isp->dev, "%s: data wasn't properly written.\n",
			stat->subdev.name);
		return STAT_NO_BUF;
	}
	stat->active_buf->config_counter = stat->config_counter;
	stat->active_buf->frame_number = stat->frame_number;
	stat->active_buf->empty = 0;
	stat->active_buf = NULL;

	return STAT_BUF_DONE;
}

static void isp_stat_buf_next(struct ispstat *stat)
{
	if (unlikely(stat->active_buf))
		
		dev_dbg(stat->isp->dev, "%s: new buffer requested without "
					"queuing active one.\n",
					stat->subdev.name);
	else
		stat->active_buf = isp_stat_buf_find_oldest_or_empty(stat);
}

static void isp_stat_buf_release(struct ispstat *stat)
{
	unsigned long flags;

	isp_stat_buf_sync_for_device(stat, stat->locked_buf);
	spin_lock_irqsave(&stat->isp->stat_lock, flags);
	stat->locked_buf = NULL;
	spin_unlock_irqrestore(&stat->isp->stat_lock, flags);
}

static struct ispstat_buffer *isp_stat_buf_get(struct ispstat *stat,
					       struct omap3isp_stat_data *data)
{
	int rval = 0;
	unsigned long flags;
	struct ispstat_buffer *buf;

	spin_lock_irqsave(&stat->isp->stat_lock, flags);

	while (1) {
		buf = isp_stat_buf_find_oldest(stat);
		if (!buf) {
			spin_unlock_irqrestore(&stat->isp->stat_lock, flags);
			dev_dbg(stat->isp->dev, "%s: cannot find a buffer.\n",
				stat->subdev.name);
			return ERR_PTR(-EBUSY);
		}
		if (isp_stat_buf_check_magic(stat, buf)) {
			dev_dbg(stat->isp->dev, "%s: current buffer has "
				"corrupted data\n.", stat->subdev.name);
			
			buf->empty = 1;
		} else {
			
			break;
		}
	}

	stat->locked_buf = buf;

	spin_unlock_irqrestore(&stat->isp->stat_lock, flags);

	if (buf->buf_size > data->buf_size) {
		dev_warn(stat->isp->dev, "%s: userspace's buffer size is "
					 "not enough.\n", stat->subdev.name);
		isp_stat_buf_release(stat);
		return ERR_PTR(-EINVAL);
	}

	isp_stat_buf_sync_for_cpu(stat, buf);

	rval = copy_to_user(data->buf,
			    buf->virt_addr,
			    buf->buf_size);

	if (rval) {
		dev_info(stat->isp->dev,
			 "%s: failed copying %d bytes of stat data\n",
			 stat->subdev.name, rval);
		buf = ERR_PTR(-EFAULT);
		isp_stat_buf_release(stat);
	}

	return buf;
}

static void isp_stat_bufs_free(struct ispstat *stat)
{
	struct isp_device *isp = stat->isp;
	int i;

	for (i = 0; i < STAT_MAX_BUFS; i++) {
		struct ispstat_buffer *buf = &stat->buf[i];

		if (!IS_COHERENT_BUF(stat)) {
			if (IS_ERR_OR_NULL((void *)buf->iommu_addr))
				continue;
			if (buf->iovm)
				dma_unmap_sg(isp->dev, buf->iovm->sgt->sgl,
					     buf->iovm->sgt->nents,
					     DMA_FROM_DEVICE);
			omap_iommu_vfree(isp->domain, isp->dev,
							buf->iommu_addr);
		} else {
			if (!buf->virt_addr)
				continue;
			dma_free_coherent(stat->isp->dev, stat->buf_alloc_size,
					  buf->virt_addr, buf->dma_addr);
		}
		buf->iommu_addr = 0;
		buf->iovm = NULL;
		buf->dma_addr = 0;
		buf->virt_addr = NULL;
		buf->empty = 1;
	}

	dev_dbg(stat->isp->dev, "%s: all buffers were freed.\n",
		stat->subdev.name);

	stat->buf_alloc_size = 0;
	stat->active_buf = NULL;
}

static int isp_stat_bufs_alloc_iommu(struct ispstat *stat, unsigned int size)
{
	struct isp_device *isp = stat->isp;
	int i;

	stat->buf_alloc_size = size;

	for (i = 0; i < STAT_MAX_BUFS; i++) {
		struct ispstat_buffer *buf = &stat->buf[i];
		struct iovm_struct *iovm;

		WARN_ON(buf->dma_addr);
		buf->iommu_addr = omap_iommu_vmalloc(isp->domain, isp->dev, 0,
							size, IOMMU_FLAG);
		if (IS_ERR((void *)buf->iommu_addr)) {
			dev_err(stat->isp->dev,
				 "%s: Can't acquire memory for "
				 "buffer %d\n", stat->subdev.name, i);
			isp_stat_bufs_free(stat);
			return -ENOMEM;
		}

		iovm = omap_find_iovm_area(isp->dev, buf->iommu_addr);
		if (!iovm ||
		    !dma_map_sg(isp->dev, iovm->sgt->sgl, iovm->sgt->nents,
				DMA_FROM_DEVICE)) {
			isp_stat_bufs_free(stat);
			return -ENOMEM;
		}
		buf->iovm = iovm;

		buf->virt_addr = omap_da_to_va(stat->isp->dev,
					  (u32)buf->iommu_addr);
		buf->empty = 1;
		dev_dbg(stat->isp->dev, "%s: buffer[%d] allocated."
			"iommu_addr=0x%08lx virt_addr=0x%08lx",
			stat->subdev.name, i, buf->iommu_addr,
			(unsigned long)buf->virt_addr);
	}

	return 0;
}

static int isp_stat_bufs_alloc_dma(struct ispstat *stat, unsigned int size)
{
	int i;

	stat->buf_alloc_size = size;

	for (i = 0; i < STAT_MAX_BUFS; i++) {
		struct ispstat_buffer *buf = &stat->buf[i];

		WARN_ON(buf->iommu_addr);
		buf->virt_addr = dma_alloc_coherent(stat->isp->dev, size,
					&buf->dma_addr, GFP_KERNEL | GFP_DMA);

		if (!buf->virt_addr || !buf->dma_addr) {
			dev_info(stat->isp->dev,
				 "%s: Can't acquire memory for "
				 "DMA buffer %d\n", stat->subdev.name, i);
			isp_stat_bufs_free(stat);
			return -ENOMEM;
		}
		buf->empty = 1;

		dev_dbg(stat->isp->dev, "%s: buffer[%d] allocated."
			"dma_addr=0x%08lx virt_addr=0x%08lx\n",
			stat->subdev.name, i, (unsigned long)buf->dma_addr,
			(unsigned long)buf->virt_addr);
	}

	return 0;
}

static int isp_stat_bufs_alloc(struct ispstat *stat, u32 size)
{
	unsigned long flags;

	spin_lock_irqsave(&stat->isp->stat_lock, flags);

	BUG_ON(stat->locked_buf != NULL);

	
	if (stat->buf_alloc_size >= size) {
		spin_unlock_irqrestore(&stat->isp->stat_lock, flags);
		return 0;
	}

	if (stat->state != ISPSTAT_DISABLED || stat->buf_processing) {
		dev_info(stat->isp->dev,
			 "%s: trying to allocate memory when busy\n",
			 stat->subdev.name);
		spin_unlock_irqrestore(&stat->isp->stat_lock, flags);
		return -EBUSY;
	}

	spin_unlock_irqrestore(&stat->isp->stat_lock, flags);

	isp_stat_bufs_free(stat);

	if (IS_COHERENT_BUF(stat))
		return isp_stat_bufs_alloc_dma(stat, size);
	else
		return isp_stat_bufs_alloc_iommu(stat, size);
}

static void isp_stat_queue_event(struct ispstat *stat, int err)
{
	struct video_device *vdev = stat->subdev.devnode;
	struct v4l2_event event;
	struct omap3isp_stat_event_status *status = (void *)event.u.data;

	memset(&event, 0, sizeof(event));
	if (!err) {
		status->frame_number = stat->frame_number;
		status->config_counter = stat->config_counter;
	} else {
		status->buf_err = 1;
	}
	event.type = stat->event_type;
	v4l2_event_queue(vdev, &event);
}


int omap3isp_stat_request_statistics(struct ispstat *stat,
				     struct omap3isp_stat_data *data)
{
	struct ispstat_buffer *buf;

	if (stat->state != ISPSTAT_ENABLED) {
		dev_dbg(stat->isp->dev, "%s: engine not enabled.\n",
			stat->subdev.name);
		return -EINVAL;
	}

	mutex_lock(&stat->ioctl_lock);
	buf = isp_stat_buf_get(stat, data);
	if (IS_ERR(buf)) {
		mutex_unlock(&stat->ioctl_lock);
		return PTR_ERR(buf);
	}

	data->ts = buf->ts;
	data->config_counter = buf->config_counter;
	data->frame_number = buf->frame_number;
	data->buf_size = buf->buf_size;

	buf->empty = 1;
	isp_stat_buf_release(stat);
	mutex_unlock(&stat->ioctl_lock);

	return 0;
}

int omap3isp_stat_config(struct ispstat *stat, void *new_conf)
{
	int ret;
	unsigned long irqflags;
	struct ispstat_generic_config *user_cfg = new_conf;
	u32 buf_size = user_cfg->buf_size;

	if (!new_conf) {
		dev_dbg(stat->isp->dev, "%s: configuration is NULL\n",
			stat->subdev.name);
		return -EINVAL;
	}

	mutex_lock(&stat->ioctl_lock);

	dev_dbg(stat->isp->dev, "%s: configuring module with buffer "
		"size=0x%08lx\n", stat->subdev.name, (unsigned long)buf_size);

	ret = stat->ops->validate_params(stat, new_conf);
	if (ret) {
		mutex_unlock(&stat->ioctl_lock);
		dev_dbg(stat->isp->dev, "%s: configuration values are "
					"invalid.\n", stat->subdev.name);
		return ret;
	}

	if (buf_size != user_cfg->buf_size)
		dev_dbg(stat->isp->dev, "%s: driver has corrected buffer size "
			"request to 0x%08lx\n", stat->subdev.name,
			(unsigned long)user_cfg->buf_size);

	if (IS_H3A(stat)) {
		buf_size = user_cfg->buf_size * 2 + MAGIC_SIZE;
		if (IS_H3A_AF(stat))
			buf_size += AF_EXTRA_DATA * (NUM_H3A_RECOVER_BUFS + 2);
		if (stat->recover_priv) {
			struct ispstat_generic_config *recover_cfg =
				stat->recover_priv;
			buf_size += recover_cfg->buf_size *
				    NUM_H3A_RECOVER_BUFS;
		}
		buf_size = PAGE_ALIGN(buf_size);
	} else { 
		buf_size = PAGE_ALIGN(user_cfg->buf_size + MAGIC_SIZE);
	}

	ret = isp_stat_bufs_alloc(stat, buf_size);
	if (ret) {
		mutex_unlock(&stat->ioctl_lock);
		return ret;
	}

	spin_lock_irqsave(&stat->isp->stat_lock, irqflags);
	stat->ops->set_params(stat, new_conf);
	spin_unlock_irqrestore(&stat->isp->stat_lock, irqflags);

	user_cfg->config_counter = stat->config_counter + stat->inc_config;

	
	stat->configured = 1;
	dev_dbg(stat->isp->dev, "%s: module has been successfully "
		"configured.\n", stat->subdev.name);

	mutex_unlock(&stat->ioctl_lock);

	return 0;
}

static int isp_stat_buf_process(struct ispstat *stat, int buf_state)
{
	int ret = STAT_NO_BUF;

	if (!atomic_add_unless(&stat->buf_err, -1, 0) &&
	    buf_state == STAT_BUF_DONE && stat->state == ISPSTAT_ENABLED) {
		ret = isp_stat_buf_queue(stat);
		isp_stat_buf_next(stat);
	}

	return ret;
}

int omap3isp_stat_pcr_busy(struct ispstat *stat)
{
	return stat->ops->busy(stat);
}

int omap3isp_stat_busy(struct ispstat *stat)
{
	return omap3isp_stat_pcr_busy(stat) | stat->buf_processing |
		(stat->state != ISPSTAT_DISABLED);
}

static void isp_stat_pcr_enable(struct ispstat *stat, u8 pcr_enable)
{
	if ((stat->state != ISPSTAT_ENABLING &&
	     stat->state != ISPSTAT_ENABLED) && pcr_enable)
		
		return;

	stat->ops->enable(stat, pcr_enable);
	if (stat->state == ISPSTAT_DISABLING && !pcr_enable)
		stat->state = ISPSTAT_DISABLED;
	else if (stat->state == ISPSTAT_ENABLING && pcr_enable)
		stat->state = ISPSTAT_ENABLED;
}

void omap3isp_stat_suspend(struct ispstat *stat)
{
	unsigned long flags;

	spin_lock_irqsave(&stat->isp->stat_lock, flags);

	if (stat->state != ISPSTAT_DISABLED)
		stat->ops->enable(stat, 0);
	if (stat->state == ISPSTAT_ENABLED)
		stat->state = ISPSTAT_SUSPENDED;

	spin_unlock_irqrestore(&stat->isp->stat_lock, flags);
}

void omap3isp_stat_resume(struct ispstat *stat)
{
	
	if (stat->state == ISPSTAT_SUSPENDED)
		stat->state = ISPSTAT_ENABLING;
}

static void isp_stat_try_enable(struct ispstat *stat)
{
	unsigned long irqflags;

	if (stat->priv == NULL)
		
		return;

	spin_lock_irqsave(&stat->isp->stat_lock, irqflags);
	if (stat->state == ISPSTAT_ENABLING && !stat->buf_processing &&
	    stat->buf_alloc_size) {
		stat->update = 1;
		isp_stat_buf_next(stat);
		stat->ops->setup_regs(stat, stat->priv);
		isp_stat_buf_insert_magic(stat, stat->active_buf);

		if (!IS_H3A(stat))
			atomic_set(&stat->buf_err, 0);

		isp_stat_pcr_enable(stat, 1);
		spin_unlock_irqrestore(&stat->isp->stat_lock, irqflags);
		dev_dbg(stat->isp->dev, "%s: module is enabled.\n",
			stat->subdev.name);
	} else {
		spin_unlock_irqrestore(&stat->isp->stat_lock, irqflags);
	}
}

void omap3isp_stat_isr_frame_sync(struct ispstat *stat)
{
	isp_stat_try_enable(stat);
}

void omap3isp_stat_sbl_overflow(struct ispstat *stat)
{
	unsigned long irqflags;

	spin_lock_irqsave(&stat->isp->stat_lock, irqflags);
	atomic_set(&stat->buf_err, 2);

	if (stat->recover_priv)
		stat->sbl_ovl_recover = 1;
	spin_unlock_irqrestore(&stat->isp->stat_lock, irqflags);
}

int omap3isp_stat_enable(struct ispstat *stat, u8 enable)
{
	unsigned long irqflags;

	dev_dbg(stat->isp->dev, "%s: user wants to %s module.\n",
		stat->subdev.name, enable ? "enable" : "disable");

	
	mutex_lock(&stat->ioctl_lock);

	spin_lock_irqsave(&stat->isp->stat_lock, irqflags);

	if (!stat->configured && enable) {
		spin_unlock_irqrestore(&stat->isp->stat_lock, irqflags);
		mutex_unlock(&stat->ioctl_lock);
		dev_dbg(stat->isp->dev, "%s: cannot enable module as it's "
			"never been successfully configured so far.\n",
			stat->subdev.name);
		return -EINVAL;
	}

	if (enable) {
		if (stat->state == ISPSTAT_DISABLING)
			
			stat->state = ISPSTAT_ENABLED;
		else if (stat->state == ISPSTAT_DISABLED)
			
			stat->state = ISPSTAT_ENABLING;
	} else {
		if (stat->state == ISPSTAT_ENABLING) {
			
			stat->state = ISPSTAT_DISABLED;
		} else if (stat->state == ISPSTAT_ENABLED) {
			
			stat->state = ISPSTAT_DISABLING;
			isp_stat_buf_clear(stat);
		}
	}

	spin_unlock_irqrestore(&stat->isp->stat_lock, irqflags);
	mutex_unlock(&stat->ioctl_lock);

	return 0;
}

int omap3isp_stat_s_stream(struct v4l2_subdev *subdev, int enable)
{
	struct ispstat *stat = v4l2_get_subdevdata(subdev);

	if (enable) {
		isp_stat_try_enable(stat);
	} else {
		unsigned long flags;
		
		omap3isp_stat_enable(stat, 0);
		spin_lock_irqsave(&stat->isp->stat_lock, flags);
		stat->ops->enable(stat, 0);
		spin_unlock_irqrestore(&stat->isp->stat_lock, flags);

		if (!omap3isp_stat_pcr_busy(stat))
			omap3isp_stat_isr(stat);

		dev_dbg(stat->isp->dev, "%s: module is being disabled\n",
			stat->subdev.name);
	}

	return 0;
}

static void __stat_isr(struct ispstat *stat, int from_dma)
{
	int ret = STAT_BUF_DONE;
	int buf_processing;
	unsigned long irqflags;
	struct isp_pipeline *pipe;

	spin_lock_irqsave(&stat->isp->stat_lock, irqflags);
	if (stat->state == ISPSTAT_DISABLED) {
		spin_unlock_irqrestore(&stat->isp->stat_lock, irqflags);
		return;
	}
	buf_processing = stat->buf_processing;
	stat->buf_processing = 1;
	stat->ops->enable(stat, 0);

	if (buf_processing && !from_dma) {
		if (stat->state == ISPSTAT_ENABLED) {
			spin_unlock_irqrestore(&stat->isp->stat_lock, irqflags);
			dev_err(stat->isp->dev,
				"%s: interrupt occurred when module was still "
				"processing a buffer.\n", stat->subdev.name);
			ret = STAT_NO_BUF;
			goto out;
		} else {
			spin_unlock_irqrestore(&stat->isp->stat_lock, irqflags);
			return;
		}
	}
	spin_unlock_irqrestore(&stat->isp->stat_lock, irqflags);

	
	if (!omap3isp_stat_pcr_busy(stat)) {
		if (!from_dma && stat->ops->buf_process)
			
			ret = stat->ops->buf_process(stat);
		if (ret == STAT_BUF_WAITING_DMA)
			
			return;

		spin_lock_irqsave(&stat->isp->stat_lock, irqflags);

		if (stat->state == ISPSTAT_DISABLING) {
			stat->state = ISPSTAT_DISABLED;
			spin_unlock_irqrestore(&stat->isp->stat_lock, irqflags);
			stat->buf_processing = 0;
			return;
		}
		pipe = to_isp_pipeline(&stat->subdev.entity);
		stat->frame_number = atomic_read(&pipe->frame_number);

		ret = isp_stat_buf_process(stat, ret);

		if (likely(!stat->sbl_ovl_recover)) {
			stat->ops->setup_regs(stat, stat->priv);
		} else {
			stat->update = 1;
			stat->ops->setup_regs(stat, stat->recover_priv);
			stat->sbl_ovl_recover = 0;

			stat->update = 1;
		}

		isp_stat_buf_insert_magic(stat, stat->active_buf);

		isp_stat_pcr_enable(stat, 1);
		spin_unlock_irqrestore(&stat->isp->stat_lock, irqflags);
	} else {

		if (stat->ops->buf_process)
			atomic_set(&stat->buf_err, 1);

		ret = STAT_NO_BUF;
		dev_dbg(stat->isp->dev, "%s: cannot process buffer, "
					"device is busy.\n", stat->subdev.name);
	}

out:
	stat->buf_processing = 0;
	isp_stat_queue_event(stat, ret != STAT_BUF_DONE);
}

void omap3isp_stat_isr(struct ispstat *stat)
{
	__stat_isr(stat, 0);
}

void omap3isp_stat_dma_isr(struct ispstat *stat)
{
	__stat_isr(stat, 1);
}

int omap3isp_stat_subscribe_event(struct v4l2_subdev *subdev,
				  struct v4l2_fh *fh,
				  struct v4l2_event_subscription *sub)
{
	struct ispstat *stat = v4l2_get_subdevdata(subdev);

	if (sub->type != stat->event_type)
		return -EINVAL;

	return v4l2_event_subscribe(fh, sub, STAT_NEVENTS);
}

int omap3isp_stat_unsubscribe_event(struct v4l2_subdev *subdev,
				    struct v4l2_fh *fh,
				    struct v4l2_event_subscription *sub)
{
	return v4l2_event_unsubscribe(fh, sub);
}

void omap3isp_stat_unregister_entities(struct ispstat *stat)
{
	v4l2_device_unregister_subdev(&stat->subdev);
}

int omap3isp_stat_register_entities(struct ispstat *stat,
				    struct v4l2_device *vdev)
{
	return v4l2_device_register_subdev(vdev, &stat->subdev);
}

static int isp_stat_init_entities(struct ispstat *stat, const char *name,
				  const struct v4l2_subdev_ops *sd_ops)
{
	struct v4l2_subdev *subdev = &stat->subdev;
	struct media_entity *me = &subdev->entity;

	v4l2_subdev_init(subdev, sd_ops);
	snprintf(subdev->name, V4L2_SUBDEV_NAME_SIZE, "OMAP3 ISP %s", name);
	subdev->grp_id = 1 << 16;	
	subdev->flags |= V4L2_SUBDEV_FL_HAS_EVENTS | V4L2_SUBDEV_FL_HAS_DEVNODE;
	v4l2_set_subdevdata(subdev, stat);

	stat->pad.flags = MEDIA_PAD_FL_SINK;
	me->ops = NULL;

	return media_entity_init(me, 1, &stat->pad, 0);
}

int omap3isp_stat_init(struct ispstat *stat, const char *name,
		       const struct v4l2_subdev_ops *sd_ops)
{
	int ret;

	stat->buf = kcalloc(STAT_MAX_BUFS, sizeof(*stat->buf), GFP_KERNEL);
	if (!stat->buf)
		return -ENOMEM;

	isp_stat_buf_clear(stat);
	mutex_init(&stat->ioctl_lock);
	atomic_set(&stat->buf_err, 0);

	ret = isp_stat_init_entities(stat, name, sd_ops);
	if (ret < 0) {
		mutex_destroy(&stat->ioctl_lock);
		kfree(stat->buf);
	}

	return ret;
}

void omap3isp_stat_cleanup(struct ispstat *stat)
{
	media_entity_cleanup(&stat->subdev.entity);
	mutex_destroy(&stat->ioctl_lock);
	isp_stat_bufs_free(stat);
	kfree(stat->buf);
}
