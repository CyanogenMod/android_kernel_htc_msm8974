
/*
 * Created: Fri Mar 19 14:30:16 1999 by faith@valinux.com
 *
 * Copyright 1999, 2000 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "drmP.h"
#include "drm_trace.h"

#include <linux/interrupt.h>	
#include <linux/slab.h>

#include <linux/vgaarb.h>
#include <linux/export.h>

#define vblanktimestamp(dev, crtc, count) ( \
	(dev)->_vblank_time[(crtc) * DRM_VBLANKTIME_RBSIZE + \
	((count) % DRM_VBLANKTIME_RBSIZE)])

#define DRM_TIMESTAMP_MAXRETRIES 3

#define DRM_REDUNDANT_VBLIRQ_THRESH_NS 1000000

int drm_irq_by_busid(struct drm_device *dev, void *data,
		     struct drm_file *file_priv)
{
	struct drm_irq_busid *p = data;

	if (!dev->driver->bus->irq_by_busid)
		return -EINVAL;

	if (!drm_core_check_feature(dev, DRIVER_HAVE_IRQ))
		return -EINVAL;

	return dev->driver->bus->irq_by_busid(dev, p);
}

static void clear_vblank_timestamps(struct drm_device *dev, int crtc)
{
	memset(&dev->_vblank_time[crtc * DRM_VBLANKTIME_RBSIZE], 0,
		DRM_VBLANKTIME_RBSIZE * sizeof(struct timeval));
}

static void vblank_disable_and_save(struct drm_device *dev, int crtc)
{
	unsigned long irqflags;
	u32 vblcount;
	s64 diff_ns;
	int vblrc;
	struct timeval tvblank;

	spin_lock_irqsave(&dev->vblank_time_lock, irqflags);

	dev->driver->disable_vblank(dev, crtc);
	dev->vblank_enabled[crtc] = 0;

	do {
		dev->last_vblank[crtc] = dev->driver->get_vblank_counter(dev, crtc);
		vblrc = drm_get_last_vbltimestamp(dev, crtc, &tvblank, 0);
	} while (dev->last_vblank[crtc] != dev->driver->get_vblank_counter(dev, crtc));

	vblcount = atomic_read(&dev->_vblank_count[crtc]);
	diff_ns = timeval_to_ns(&tvblank) -
		  timeval_to_ns(&vblanktimestamp(dev, crtc, vblcount));

	if ((vblrc > 0) && (abs64(diff_ns) > 1000000)) {
		atomic_inc(&dev->_vblank_count[crtc]);
		smp_mb__after_atomic_inc();
	}

	
	clear_vblank_timestamps(dev, crtc);

	spin_unlock_irqrestore(&dev->vblank_time_lock, irqflags);
}

static void vblank_disable_fn(unsigned long arg)
{
	struct drm_device *dev = (struct drm_device *)arg;
	unsigned long irqflags;
	int i;

	if (!dev->vblank_disable_allowed)
		return;

	for (i = 0; i < dev->num_crtcs; i++) {
		spin_lock_irqsave(&dev->vbl_lock, irqflags);
		if (atomic_read(&dev->vblank_refcount[i]) == 0 &&
		    dev->vblank_enabled[i]) {
			DRM_DEBUG("disabling vblank on crtc %d\n", i);
			vblank_disable_and_save(dev, i);
		}
		spin_unlock_irqrestore(&dev->vbl_lock, irqflags);
	}
}

void drm_vblank_cleanup(struct drm_device *dev)
{
	
	if (dev->num_crtcs == 0)
		return;

	del_timer(&dev->vblank_disable_timer);

	vblank_disable_fn((unsigned long)dev);

	kfree(dev->vbl_queue);
	kfree(dev->_vblank_count);
	kfree(dev->vblank_refcount);
	kfree(dev->vblank_enabled);
	kfree(dev->last_vblank);
	kfree(dev->last_vblank_wait);
	kfree(dev->vblank_inmodeset);
	kfree(dev->_vblank_time);

	dev->num_crtcs = 0;
}
EXPORT_SYMBOL(drm_vblank_cleanup);

int drm_vblank_init(struct drm_device *dev, int num_crtcs)
{
	int i, ret = -ENOMEM;

	setup_timer(&dev->vblank_disable_timer, vblank_disable_fn,
		    (unsigned long)dev);
	spin_lock_init(&dev->vbl_lock);
	spin_lock_init(&dev->vblank_time_lock);

	dev->num_crtcs = num_crtcs;

	dev->vbl_queue = kmalloc(sizeof(wait_queue_head_t) * num_crtcs,
				 GFP_KERNEL);
	if (!dev->vbl_queue)
		goto err;

	dev->_vblank_count = kmalloc(sizeof(atomic_t) * num_crtcs, GFP_KERNEL);
	if (!dev->_vblank_count)
		goto err;

	dev->vblank_refcount = kmalloc(sizeof(atomic_t) * num_crtcs,
				       GFP_KERNEL);
	if (!dev->vblank_refcount)
		goto err;

	dev->vblank_enabled = kcalloc(num_crtcs, sizeof(int), GFP_KERNEL);
	if (!dev->vblank_enabled)
		goto err;

	dev->last_vblank = kcalloc(num_crtcs, sizeof(u32), GFP_KERNEL);
	if (!dev->last_vblank)
		goto err;

	dev->last_vblank_wait = kcalloc(num_crtcs, sizeof(u32), GFP_KERNEL);
	if (!dev->last_vblank_wait)
		goto err;

	dev->vblank_inmodeset = kcalloc(num_crtcs, sizeof(int), GFP_KERNEL);
	if (!dev->vblank_inmodeset)
		goto err;

	dev->_vblank_time = kcalloc(num_crtcs * DRM_VBLANKTIME_RBSIZE,
				    sizeof(struct timeval), GFP_KERNEL);
	if (!dev->_vblank_time)
		goto err;

	DRM_INFO("Supports vblank timestamp caching Rev 1 (10.10.2010).\n");

	
	if (dev->driver->get_vblank_timestamp)
		DRM_INFO("Driver supports precise vblank timestamp query.\n");
	else
		DRM_INFO("No driver support for vblank timestamp query.\n");

	
	for (i = 0; i < num_crtcs; i++) {
		init_waitqueue_head(&dev->vbl_queue[i]);
		atomic_set(&dev->_vblank_count[i], 0);
		atomic_set(&dev->vblank_refcount[i], 0);
	}

	dev->vblank_disable_allowed = 0;
	return 0;

err:
	drm_vblank_cleanup(dev);
	return ret;
}
EXPORT_SYMBOL(drm_vblank_init);

static void drm_irq_vgaarb_nokms(void *cookie, bool state)
{
	struct drm_device *dev = cookie;

	if (dev->driver->vgaarb_irq) {
		dev->driver->vgaarb_irq(dev, state);
		return;
	}

	if (!dev->irq_enabled)
		return;

	if (state) {
		if (dev->driver->irq_uninstall)
			dev->driver->irq_uninstall(dev);
	} else {
		if (dev->driver->irq_preinstall)
			dev->driver->irq_preinstall(dev);
		if (dev->driver->irq_postinstall)
			dev->driver->irq_postinstall(dev);
	}
}

int drm_irq_install(struct drm_device *dev)
{
	int ret = 0;
	unsigned long sh_flags = 0;
	char *irqname;

	if (!drm_core_check_feature(dev, DRIVER_HAVE_IRQ))
		return -EINVAL;

	if (drm_dev_to_irq(dev) == 0)
		return -EINVAL;

	mutex_lock(&dev->struct_mutex);

	
	if (!dev->dev_private) {
		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	}

	if (dev->irq_enabled) {
		mutex_unlock(&dev->struct_mutex);
		return -EBUSY;
	}
	dev->irq_enabled = 1;
	mutex_unlock(&dev->struct_mutex);

	DRM_DEBUG("irq=%d\n", drm_dev_to_irq(dev));

	
	if (dev->driver->irq_preinstall)
		dev->driver->irq_preinstall(dev);

	
	if (drm_core_check_feature(dev, DRIVER_IRQ_SHARED))
		sh_flags = IRQF_SHARED;

	if (dev->devname)
		irqname = dev->devname;
	else
		irqname = dev->driver->name;

	ret = request_irq(drm_dev_to_irq(dev), dev->driver->irq_handler,
			  sh_flags, irqname, dev);

	if (ret < 0) {
		mutex_lock(&dev->struct_mutex);
		dev->irq_enabled = 0;
		mutex_unlock(&dev->struct_mutex);
		return ret;
	}

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		vga_client_register(dev->pdev, (void *)dev, drm_irq_vgaarb_nokms, NULL);

	
	if (dev->driver->irq_postinstall)
		ret = dev->driver->irq_postinstall(dev);

	if (ret < 0) {
		mutex_lock(&dev->struct_mutex);
		dev->irq_enabled = 0;
		mutex_unlock(&dev->struct_mutex);
		if (!drm_core_check_feature(dev, DRIVER_MODESET))
			vga_client_register(dev->pdev, NULL, NULL, NULL);
		free_irq(drm_dev_to_irq(dev), dev);
	}

	return ret;
}
EXPORT_SYMBOL(drm_irq_install);

int drm_irq_uninstall(struct drm_device *dev)
{
	unsigned long irqflags;
	int irq_enabled, i;

	if (!drm_core_check_feature(dev, DRIVER_HAVE_IRQ))
		return -EINVAL;

	mutex_lock(&dev->struct_mutex);
	irq_enabled = dev->irq_enabled;
	dev->irq_enabled = 0;
	mutex_unlock(&dev->struct_mutex);

	if (dev->num_crtcs) {
		spin_lock_irqsave(&dev->vbl_lock, irqflags);
		for (i = 0; i < dev->num_crtcs; i++) {
			DRM_WAKEUP(&dev->vbl_queue[i]);
			dev->vblank_enabled[i] = 0;
			dev->last_vblank[i] =
				dev->driver->get_vblank_counter(dev, i);
		}
		spin_unlock_irqrestore(&dev->vbl_lock, irqflags);
	}

	if (!irq_enabled)
		return -EINVAL;

	DRM_DEBUG("irq=%d\n", drm_dev_to_irq(dev));

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		vga_client_register(dev->pdev, NULL, NULL, NULL);

	if (dev->driver->irq_uninstall)
		dev->driver->irq_uninstall(dev);

	free_irq(drm_dev_to_irq(dev), dev);

	return 0;
}
EXPORT_SYMBOL(drm_irq_uninstall);

int drm_control(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct drm_control *ctl = data;



	switch (ctl->func) {
	case DRM_INST_HANDLER:
		if (!drm_core_check_feature(dev, DRIVER_HAVE_IRQ))
			return 0;
		if (drm_core_check_feature(dev, DRIVER_MODESET))
			return 0;
		if (dev->if_version < DRM_IF_VERSION(1, 2) &&
		    ctl->irq != drm_dev_to_irq(dev))
			return -EINVAL;
		return drm_irq_install(dev);
	case DRM_UNINST_HANDLER:
		if (!drm_core_check_feature(dev, DRIVER_HAVE_IRQ))
			return 0;
		if (drm_core_check_feature(dev, DRIVER_MODESET))
			return 0;
		return drm_irq_uninstall(dev);
	default:
		return -EINVAL;
	}
}

void drm_calc_timestamping_constants(struct drm_crtc *crtc)
{
	s64 linedur_ns = 0, pixeldur_ns = 0, framedur_ns = 0;
	u64 dotclock;

	
	dotclock = (u64) crtc->hwmode.clock * 1000;

	if (crtc->hwmode.flags & DRM_MODE_FLAG_INTERLACE)
		dotclock *= 2;

	
	if (dotclock > 0) {
		pixeldur_ns = (s64) div64_u64(1000000000, dotclock);
		linedur_ns  = (s64) div64_u64(((u64) crtc->hwmode.crtc_htotal *
					      1000000000), dotclock);
		framedur_ns = (s64) crtc->hwmode.crtc_vtotal * linedur_ns;
	} else
		DRM_ERROR("crtc %d: Can't calculate constants, dotclock = 0!\n",
			  crtc->base.id);

	crtc->pixeldur_ns = pixeldur_ns;
	crtc->linedur_ns  = linedur_ns;
	crtc->framedur_ns = framedur_ns;

	DRM_DEBUG("crtc %d: hwmode: htotal %d, vtotal %d, vdisplay %d\n",
		  crtc->base.id, crtc->hwmode.crtc_htotal,
		  crtc->hwmode.crtc_vtotal, crtc->hwmode.crtc_vdisplay);
	DRM_DEBUG("crtc %d: clock %d kHz framedur %d linedur %d, pixeldur %d\n",
		  crtc->base.id, (int) dotclock/1000, (int) framedur_ns,
		  (int) linedur_ns, (int) pixeldur_ns);
}
EXPORT_SYMBOL(drm_calc_timestamping_constants);

int drm_calc_vbltimestamp_from_scanoutpos(struct drm_device *dev, int crtc,
					  int *max_error,
					  struct timeval *vblank_time,
					  unsigned flags,
					  struct drm_crtc *refcrtc)
{
	struct timeval stime, raw_time;
	struct drm_display_mode *mode;
	int vbl_status, vtotal, vdisplay;
	int vpos, hpos, i;
	s64 framedur_ns, linedur_ns, pixeldur_ns, delta_ns, duration_ns;
	bool invbl;

	if (crtc < 0 || crtc >= dev->num_crtcs) {
		DRM_ERROR("Invalid crtc %d\n", crtc);
		return -EINVAL;
	}

	
	if (!dev->driver->get_scanout_position) {
		DRM_ERROR("Called from driver w/o get_scanout_position()!?\n");
		return -EIO;
	}

	mode = &refcrtc->hwmode;
	vtotal = mode->crtc_vtotal;
	vdisplay = mode->crtc_vdisplay;

	
	framedur_ns = refcrtc->framedur_ns;
	linedur_ns  = refcrtc->linedur_ns;
	pixeldur_ns = refcrtc->pixeldur_ns;

	if (vtotal <= 0 || vdisplay <= 0 || framedur_ns == 0) {
		DRM_DEBUG("crtc %d: Noop due to uninitialized mode.\n", crtc);
		return -EAGAIN;
	}

	for (i = 0; i < DRM_TIMESTAMP_MAXRETRIES; i++) {
		preempt_disable();

		
		do_gettimeofday(&stime);

		
		vbl_status = dev->driver->get_scanout_position(dev, crtc, &vpos, &hpos);

		
		do_gettimeofday(&raw_time);

		preempt_enable();

		
		if (!(vbl_status & DRM_SCANOUTPOS_VALID)) {
			DRM_DEBUG("crtc %d : scanoutpos query failed [%d].\n",
				  crtc, vbl_status);
			return -EIO;
		}

		duration_ns = timeval_to_ns(&raw_time) - timeval_to_ns(&stime);

		
		if (duration_ns <= (s64) *max_error)
			break;
	}

	
	if (i == DRM_TIMESTAMP_MAXRETRIES) {
		DRM_DEBUG("crtc %d: Noisy timestamp %d us > %d us [%d reps].\n",
			  crtc, (int) duration_ns/1000, *max_error/1000, i);
	}

	
	*max_error = (int) duration_ns;

	invbl = vbl_status & DRM_SCANOUTPOS_INVBL;

	delta_ns = (s64) vpos * linedur_ns + (s64) hpos * pixeldur_ns;

	if ((flags & DRM_CALLED_FROM_VBLIRQ) && !invbl &&
	    ((vdisplay - vpos) < vtotal / 100)) {
		delta_ns = delta_ns - framedur_ns;

		
		vbl_status |= 0x8;
	}

	*vblank_time = ns_to_timeval(timeval_to_ns(&raw_time) - delta_ns);

	DRM_DEBUG("crtc %d : v %d p(%d,%d)@ %ld.%ld -> %ld.%ld [e %d us, %d rep]\n",
		  crtc, (int)vbl_status, hpos, vpos,
		  (long)raw_time.tv_sec, (long)raw_time.tv_usec,
		  (long)vblank_time->tv_sec, (long)vblank_time->tv_usec,
		  (int)duration_ns/1000, i);

	vbl_status = DRM_VBLANKTIME_SCANOUTPOS_METHOD;
	if (invbl)
		vbl_status |= DRM_VBLANKTIME_INVBL;

	return vbl_status;
}
EXPORT_SYMBOL(drm_calc_vbltimestamp_from_scanoutpos);

u32 drm_get_last_vbltimestamp(struct drm_device *dev, int crtc,
			      struct timeval *tvblank, unsigned flags)
{
	int ret = 0;

	
	int max_error = (int) drm_timestamp_precision * 1000;

	
	if (dev->driver->get_vblank_timestamp && (max_error > 0)) {
		ret = dev->driver->get_vblank_timestamp(dev, crtc, &max_error,
							tvblank, flags);
		if (ret > 0)
			return (u32) ret;
	}

	do_gettimeofday(tvblank);

	return 0;
}
EXPORT_SYMBOL(drm_get_last_vbltimestamp);

u32 drm_vblank_count(struct drm_device *dev, int crtc)
{
	return atomic_read(&dev->_vblank_count[crtc]);
}
EXPORT_SYMBOL(drm_vblank_count);

u32 drm_vblank_count_and_time(struct drm_device *dev, int crtc,
			      struct timeval *vblanktime)
{
	u32 cur_vblank;

	do {
		cur_vblank = atomic_read(&dev->_vblank_count[crtc]);
		*vblanktime = vblanktimestamp(dev, crtc, cur_vblank);
		smp_rmb();
	} while (cur_vblank != atomic_read(&dev->_vblank_count[crtc]));

	return cur_vblank;
}
EXPORT_SYMBOL(drm_vblank_count_and_time);

static void drm_update_vblank_count(struct drm_device *dev, int crtc)
{
	u32 cur_vblank, diff, tslot, rc;
	struct timeval t_vblank;

	do {
		cur_vblank = dev->driver->get_vblank_counter(dev, crtc);
		rc = drm_get_last_vbltimestamp(dev, crtc, &t_vblank, 0);
	} while (cur_vblank != dev->driver->get_vblank_counter(dev, crtc));

	
	diff = cur_vblank - dev->last_vblank[crtc];
	if (cur_vblank < dev->last_vblank[crtc]) {
		diff += dev->max_vblank_count;

		DRM_DEBUG("last_vblank[%d]=0x%x, cur_vblank=0x%x => diff=0x%x\n",
			  crtc, dev->last_vblank[crtc], cur_vblank, diff);
	}

	DRM_DEBUG("enabling vblank interrupts on crtc %d, missed %d\n",
		  crtc, diff);

	if (rc) {
		tslot = atomic_read(&dev->_vblank_count[crtc]) + diff;
		vblanktimestamp(dev, crtc, tslot) = t_vblank;
	}

	smp_mb__before_atomic_inc();
	atomic_add(diff, &dev->_vblank_count[crtc]);
	smp_mb__after_atomic_inc();
}

int drm_vblank_get(struct drm_device *dev, int crtc)
{
	unsigned long irqflags, irqflags2;
	int ret = 0;

	spin_lock_irqsave(&dev->vbl_lock, irqflags);
	
	if (atomic_add_return(1, &dev->vblank_refcount[crtc]) == 1) {
		spin_lock_irqsave(&dev->vblank_time_lock, irqflags2);
		if (!dev->vblank_enabled[crtc]) {
			ret = dev->driver->enable_vblank(dev, crtc);
			DRM_DEBUG("enabling vblank on crtc %d, ret: %d\n",
				  crtc, ret);
			if (ret)
				atomic_dec(&dev->vblank_refcount[crtc]);
			else {
				dev->vblank_enabled[crtc] = 1;
				drm_update_vblank_count(dev, crtc);
			}
		}
		spin_unlock_irqrestore(&dev->vblank_time_lock, irqflags2);
	} else {
		if (!dev->vblank_enabled[crtc]) {
			atomic_dec(&dev->vblank_refcount[crtc]);
			ret = -EINVAL;
		}
	}
	spin_unlock_irqrestore(&dev->vbl_lock, irqflags);

	return ret;
}
EXPORT_SYMBOL(drm_vblank_get);

void drm_vblank_put(struct drm_device *dev, int crtc)
{
	BUG_ON(atomic_read(&dev->vblank_refcount[crtc]) == 0);

	
	if (atomic_dec_and_test(&dev->vblank_refcount[crtc]) &&
	    (drm_vblank_offdelay > 0))
		mod_timer(&dev->vblank_disable_timer,
			  jiffies + ((drm_vblank_offdelay * DRM_HZ)/1000));
}
EXPORT_SYMBOL(drm_vblank_put);

void drm_vblank_off(struct drm_device *dev, int crtc)
{
	struct drm_pending_vblank_event *e, *t;
	struct timeval now;
	unsigned long irqflags;
	unsigned int seq;

	spin_lock_irqsave(&dev->vbl_lock, irqflags);
	vblank_disable_and_save(dev, crtc);
	DRM_WAKEUP(&dev->vbl_queue[crtc]);

	
	seq = drm_vblank_count_and_time(dev, crtc, &now);
	list_for_each_entry_safe(e, t, &dev->vblank_event_list, base.link) {
		if (e->pipe != crtc)
			continue;
		DRM_DEBUG("Sending premature vblank event on disable: \
			  wanted %d, current %d\n",
			  e->event.sequence, seq);

		e->event.sequence = seq;
		e->event.tv_sec = now.tv_sec;
		e->event.tv_usec = now.tv_usec;
		drm_vblank_put(dev, e->pipe);
		list_move_tail(&e->base.link, &e->base.file_priv->event_list);
		wake_up_interruptible(&e->base.file_priv->event_wait);
		trace_drm_vblank_event_delivered(e->base.pid, e->pipe,
						 e->event.sequence);
	}

	spin_unlock_irqrestore(&dev->vbl_lock, irqflags);
}
EXPORT_SYMBOL(drm_vblank_off);

void drm_vblank_pre_modeset(struct drm_device *dev, int crtc)
{
	
	if (!dev->num_crtcs)
		return;
	if (!dev->vblank_inmodeset[crtc]) {
		dev->vblank_inmodeset[crtc] = 0x1;
		if (drm_vblank_get(dev, crtc) == 0)
			dev->vblank_inmodeset[crtc] |= 0x2;
	}
}
EXPORT_SYMBOL(drm_vblank_pre_modeset);

void drm_vblank_post_modeset(struct drm_device *dev, int crtc)
{
	unsigned long irqflags;

	if (dev->vblank_inmodeset[crtc]) {
		spin_lock_irqsave(&dev->vbl_lock, irqflags);
		dev->vblank_disable_allowed = 1;
		spin_unlock_irqrestore(&dev->vbl_lock, irqflags);

		if (dev->vblank_inmodeset[crtc] & 0x2)
			drm_vblank_put(dev, crtc);

		dev->vblank_inmodeset[crtc] = 0;
	}
}
EXPORT_SYMBOL(drm_vblank_post_modeset);

int drm_modeset_ctl(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_modeset_ctl *modeset = data;
	int ret = 0;
	unsigned int crtc;

	
	if (!dev->num_crtcs)
		goto out;

	crtc = modeset->crtc;
	if (crtc >= dev->num_crtcs) {
		ret = -EINVAL;
		goto out;
	}

	switch (modeset->cmd) {
	case _DRM_PRE_MODESET:
		drm_vblank_pre_modeset(dev, crtc);
		break;
	case _DRM_POST_MODESET:
		drm_vblank_post_modeset(dev, crtc);
		break;
	default:
		ret = -EINVAL;
		break;
	}

out:
	return ret;
}

static int drm_queue_vblank_event(struct drm_device *dev, int pipe,
				  union drm_wait_vblank *vblwait,
				  struct drm_file *file_priv)
{
	struct drm_pending_vblank_event *e;
	struct timeval now;
	unsigned long flags;
	unsigned int seq;
	int ret;

	e = kzalloc(sizeof *e, GFP_KERNEL);
	if (e == NULL) {
		ret = -ENOMEM;
		goto err_put;
	}

	e->pipe = pipe;
	e->base.pid = current->pid;
	e->event.base.type = DRM_EVENT_VBLANK;
	e->event.base.length = sizeof e->event;
	e->event.user_data = vblwait->request.signal;
	e->base.event = &e->event.base;
	e->base.file_priv = file_priv;
	e->base.destroy = (void (*) (struct drm_pending_event *)) kfree;

	spin_lock_irqsave(&dev->event_lock, flags);

	if (file_priv->event_space < sizeof e->event) {
		ret = -EBUSY;
		goto err_unlock;
	}

	file_priv->event_space -= sizeof e->event;
	seq = drm_vblank_count_and_time(dev, pipe, &now);

	if ((vblwait->request.type & _DRM_VBLANK_NEXTONMISS) &&
	    (seq - vblwait->request.sequence) <= (1 << 23)) {
		vblwait->request.sequence = seq + 1;
		vblwait->reply.sequence = vblwait->request.sequence;
	}

	DRM_DEBUG("event on vblank count %d, current %d, crtc %d\n",
		  vblwait->request.sequence, seq, pipe);

	trace_drm_vblank_event_queued(current->pid, pipe,
				      vblwait->request.sequence);

	e->event.sequence = vblwait->request.sequence;
	if ((seq - vblwait->request.sequence) <= (1 << 23)) {
		e->event.sequence = seq;
		e->event.tv_sec = now.tv_sec;
		e->event.tv_usec = now.tv_usec;
		drm_vblank_put(dev, pipe);
		list_add_tail(&e->base.link, &e->base.file_priv->event_list);
		wake_up_interruptible(&e->base.file_priv->event_wait);
		vblwait->reply.sequence = seq;
		trace_drm_vblank_event_delivered(current->pid, pipe,
						 vblwait->request.sequence);
	} else {
		
		list_add_tail(&e->base.link, &dev->vblank_event_list);
		vblwait->reply.sequence = vblwait->request.sequence;
	}

	spin_unlock_irqrestore(&dev->event_lock, flags);

	return 0;

err_unlock:
	spin_unlock_irqrestore(&dev->event_lock, flags);
	kfree(e);
err_put:
	drm_vblank_put(dev, pipe);
	return ret;
}

int drm_wait_vblank(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	union drm_wait_vblank *vblwait = data;
	int ret = 0;
	unsigned int flags, seq, crtc, high_crtc;

	if ((!drm_dev_to_irq(dev)) || (!dev->irq_enabled))
		return -EINVAL;

	if (vblwait->request.type & _DRM_VBLANK_SIGNAL)
		return -EINVAL;

	if (vblwait->request.type &
	    ~(_DRM_VBLANK_TYPES_MASK | _DRM_VBLANK_FLAGS_MASK |
	      _DRM_VBLANK_HIGH_CRTC_MASK)) {
		DRM_ERROR("Unsupported type value 0x%x, supported mask 0x%x\n",
			  vblwait->request.type,
			  (_DRM_VBLANK_TYPES_MASK | _DRM_VBLANK_FLAGS_MASK |
			   _DRM_VBLANK_HIGH_CRTC_MASK));
		return -EINVAL;
	}

	flags = vblwait->request.type & _DRM_VBLANK_FLAGS_MASK;
	high_crtc = (vblwait->request.type & _DRM_VBLANK_HIGH_CRTC_MASK);
	if (high_crtc)
		crtc = high_crtc >> _DRM_VBLANK_HIGH_CRTC_SHIFT;
	else
		crtc = flags & _DRM_VBLANK_SECONDARY ? 1 : 0;
	if (crtc >= dev->num_crtcs)
		return -EINVAL;

	ret = drm_vblank_get(dev, crtc);
	if (ret) {
		DRM_DEBUG("failed to acquire vblank counter, %d\n", ret);
		return ret;
	}
	seq = drm_vblank_count(dev, crtc);

	switch (vblwait->request.type & _DRM_VBLANK_TYPES_MASK) {
	case _DRM_VBLANK_RELATIVE:
		vblwait->request.sequence += seq;
		vblwait->request.type &= ~_DRM_VBLANK_RELATIVE;
	case _DRM_VBLANK_ABSOLUTE:
		break;
	default:
		ret = -EINVAL;
		goto done;
	}

	if (flags & _DRM_VBLANK_EVENT) {
		return drm_queue_vblank_event(dev, crtc, vblwait, file_priv);
	}

	if ((flags & _DRM_VBLANK_NEXTONMISS) &&
	    (seq - vblwait->request.sequence) <= (1<<23)) {
		vblwait->request.sequence = seq + 1;
	}

	DRM_DEBUG("waiting on vblank count %d, crtc %d\n",
		  vblwait->request.sequence, crtc);
	dev->last_vblank_wait[crtc] = vblwait->request.sequence;
	DRM_WAIT_ON(ret, dev->vbl_queue[crtc], 3 * DRM_HZ,
		    (((drm_vblank_count(dev, crtc) -
		       vblwait->request.sequence) <= (1 << 23)) ||
		     !dev->irq_enabled));

	if (ret != -EINTR) {
		struct timeval now;

		vblwait->reply.sequence = drm_vblank_count_and_time(dev, crtc, &now);
		vblwait->reply.tval_sec = now.tv_sec;
		vblwait->reply.tval_usec = now.tv_usec;

		DRM_DEBUG("returning %d to client\n",
			  vblwait->reply.sequence);
	} else {
		DRM_DEBUG("vblank wait interrupted by signal\n");
	}

done:
	drm_vblank_put(dev, crtc);
	return ret;
}

void drm_handle_vblank_events(struct drm_device *dev, int crtc)
{
	struct drm_pending_vblank_event *e, *t;
	struct timeval now;
	unsigned long flags;
	unsigned int seq;

	seq = drm_vblank_count_and_time(dev, crtc, &now);

	spin_lock_irqsave(&dev->event_lock, flags);

	list_for_each_entry_safe(e, t, &dev->vblank_event_list, base.link) {
		if (e->pipe != crtc)
			continue;
		if ((seq - e->event.sequence) > (1<<23))
			continue;

		DRM_DEBUG("vblank event on %d, current %d\n",
			  e->event.sequence, seq);

		e->event.sequence = seq;
		e->event.tv_sec = now.tv_sec;
		e->event.tv_usec = now.tv_usec;
		drm_vblank_put(dev, e->pipe);
		list_move_tail(&e->base.link, &e->base.file_priv->event_list);
		wake_up_interruptible(&e->base.file_priv->event_wait);
		trace_drm_vblank_event_delivered(e->base.pid, e->pipe,
						 e->event.sequence);
	}

	spin_unlock_irqrestore(&dev->event_lock, flags);

	trace_drm_vblank_event(crtc, seq);
}

bool drm_handle_vblank(struct drm_device *dev, int crtc)
{
	u32 vblcount;
	s64 diff_ns;
	struct timeval tvblank;
	unsigned long irqflags;

	if (!dev->num_crtcs)
		return false;

	spin_lock_irqsave(&dev->vblank_time_lock, irqflags);

	
	if (!dev->vblank_enabled[crtc]) {
		spin_unlock_irqrestore(&dev->vblank_time_lock, irqflags);
		return false;
	}


	
	vblcount = atomic_read(&dev->_vblank_count[crtc]);
	drm_get_last_vbltimestamp(dev, crtc, &tvblank, DRM_CALLED_FROM_VBLIRQ);

	
	diff_ns = timeval_to_ns(&tvblank) -
		  timeval_to_ns(&vblanktimestamp(dev, crtc, vblcount));

	if (abs64(diff_ns) > DRM_REDUNDANT_VBLIRQ_THRESH_NS) {
		
		vblanktimestamp(dev, crtc, vblcount + 1) = tvblank;

		smp_mb__before_atomic_inc();
		atomic_inc(&dev->_vblank_count[crtc]);
		smp_mb__after_atomic_inc();
	} else {
		DRM_DEBUG("crtc %d: Redundant vblirq ignored. diff_ns = %d\n",
			  crtc, (int) diff_ns);
	}

	DRM_WAKEUP(&dev->vbl_queue[crtc]);
	drm_handle_vblank_events(dev, crtc);

	spin_unlock_irqrestore(&dev->vblank_time_lock, irqflags);
	return true;
}
EXPORT_SYMBOL(drm_handle_vblank);
