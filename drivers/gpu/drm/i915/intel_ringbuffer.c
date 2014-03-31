/*
 * Copyright © 2008-2010 Intel Corporation
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *    Zou Nan hai <nanhai.zou@intel.com>
 *    Xiang Hai hao<haihao.xiang@intel.com>
 *
 */

#include "drmP.h"
#include "drm.h"
#include "i915_drv.h"
#include "i915_drm.h"
#include "i915_trace.h"
#include "intel_drv.h"

struct pipe_control {
	struct drm_i915_gem_object *obj;
	volatile u32 *cpu_page;
	u32 gtt_offset;
};

static inline int ring_space(struct intel_ring_buffer *ring)
{
	int space = (ring->head & HEAD_ADDR) - (ring->tail + 8);
	if (space < 0)
		space += ring->size;
	return space;
}

static int
render_ring_flush(struct intel_ring_buffer *ring,
		  u32	invalidate_domains,
		  u32	flush_domains)
{
	struct drm_device *dev = ring->dev;
	u32 cmd;
	int ret;


	cmd = MI_FLUSH | MI_NO_WRITE_FLUSH;
	if ((invalidate_domains|flush_domains) &
	    I915_GEM_DOMAIN_RENDER)
		cmd &= ~MI_NO_WRITE_FLUSH;
	if (INTEL_INFO(dev)->gen < 4) {
		if (invalidate_domains & I915_GEM_DOMAIN_SAMPLER)
			cmd |= MI_READ_FLUSH;
	}
	if (invalidate_domains & I915_GEM_DOMAIN_INSTRUCTION)
		cmd |= MI_EXE_FLUSH;

	if (invalidate_domains & I915_GEM_DOMAIN_COMMAND &&
	    (IS_G4X(dev) || IS_GEN5(dev)))
		cmd |= MI_INVALIDATE_ISP;

	ret = intel_ring_begin(ring, 2);
	if (ret)
		return ret;

	intel_ring_emit(ring, cmd);
	intel_ring_emit(ring, MI_NOOP);
	intel_ring_advance(ring);

	return 0;
}

static int
intel_emit_post_sync_nonzero_flush(struct intel_ring_buffer *ring)
{
	struct pipe_control *pc = ring->private;
	u32 scratch_addr = pc->gtt_offset + 128;
	int ret;


	ret = intel_ring_begin(ring, 6);
	if (ret)
		return ret;

	intel_ring_emit(ring, GFX_OP_PIPE_CONTROL(5));
	intel_ring_emit(ring, PIPE_CONTROL_CS_STALL |
			PIPE_CONTROL_STALL_AT_SCOREBOARD);
	intel_ring_emit(ring, scratch_addr | PIPE_CONTROL_GLOBAL_GTT); 
	intel_ring_emit(ring, 0); 
	intel_ring_emit(ring, 0); 
	intel_ring_emit(ring, MI_NOOP);
	intel_ring_advance(ring);

	ret = intel_ring_begin(ring, 6);
	if (ret)
		return ret;

	intel_ring_emit(ring, GFX_OP_PIPE_CONTROL(5));
	intel_ring_emit(ring, PIPE_CONTROL_QW_WRITE);
	intel_ring_emit(ring, scratch_addr | PIPE_CONTROL_GLOBAL_GTT); 
	intel_ring_emit(ring, 0);
	intel_ring_emit(ring, 0);
	intel_ring_emit(ring, MI_NOOP);
	intel_ring_advance(ring);

	return 0;
}

static int
gen6_render_ring_flush(struct intel_ring_buffer *ring,
                         u32 invalidate_domains, u32 flush_domains)
{
	u32 flags = 0;
	struct pipe_control *pc = ring->private;
	u32 scratch_addr = pc->gtt_offset + 128;
	int ret;

	
	intel_emit_post_sync_nonzero_flush(ring);

	flags |= PIPE_CONTROL_RENDER_TARGET_CACHE_FLUSH;
	flags |= PIPE_CONTROL_INSTRUCTION_CACHE_INVALIDATE;
	flags |= PIPE_CONTROL_TEXTURE_CACHE_INVALIDATE;
	flags |= PIPE_CONTROL_DEPTH_CACHE_FLUSH;
	flags |= PIPE_CONTROL_VF_CACHE_INVALIDATE;
	flags |= PIPE_CONTROL_CONST_CACHE_INVALIDATE;
	flags |= PIPE_CONTROL_STATE_CACHE_INVALIDATE;

	ret = intel_ring_begin(ring, 6);
	if (ret)
		return ret;

	intel_ring_emit(ring, GFX_OP_PIPE_CONTROL(5));
	intel_ring_emit(ring, flags);
	intel_ring_emit(ring, scratch_addr | PIPE_CONTROL_GLOBAL_GTT);
	intel_ring_emit(ring, 0); 
	intel_ring_emit(ring, 0); 
	intel_ring_emit(ring, MI_NOOP);
	intel_ring_advance(ring);

	return 0;
}

static void ring_write_tail(struct intel_ring_buffer *ring,
			    u32 value)
{
	drm_i915_private_t *dev_priv = ring->dev->dev_private;
	I915_WRITE_TAIL(ring, value);
}

u32 intel_ring_get_active_head(struct intel_ring_buffer *ring)
{
	drm_i915_private_t *dev_priv = ring->dev->dev_private;
	u32 acthd_reg = INTEL_INFO(ring->dev)->gen >= 4 ?
			RING_ACTHD(ring->mmio_base) : ACTHD;

	return I915_READ(acthd_reg);
}

static int init_ring_common(struct intel_ring_buffer *ring)
{
	drm_i915_private_t *dev_priv = ring->dev->dev_private;
	struct drm_i915_gem_object *obj = ring->obj;
	u32 head;

	
	I915_WRITE_CTL(ring, 0);
	I915_WRITE_HEAD(ring, 0);
	ring->write_tail(ring, 0);

	
	I915_WRITE_START(ring, obj->gtt_offset);
	head = I915_READ_HEAD(ring) & HEAD_ADDR;

	
	if (head != 0) {
		DRM_DEBUG_KMS("%s head not reset to zero "
			      "ctl %08x head %08x tail %08x start %08x\n",
			      ring->name,
			      I915_READ_CTL(ring),
			      I915_READ_HEAD(ring),
			      I915_READ_TAIL(ring),
			      I915_READ_START(ring));

		I915_WRITE_HEAD(ring, 0);

		if (I915_READ_HEAD(ring) & HEAD_ADDR) {
			DRM_ERROR("failed to set %s head to zero "
				  "ctl %08x head %08x tail %08x start %08x\n",
				  ring->name,
				  I915_READ_CTL(ring),
				  I915_READ_HEAD(ring),
				  I915_READ_TAIL(ring),
				  I915_READ_START(ring));
		}
	}

	I915_WRITE_CTL(ring,
			((ring->size - PAGE_SIZE) & RING_NR_PAGES)
			| RING_VALID);

	
	if ((I915_READ_CTL(ring) & RING_VALID) == 0 ||
	    I915_READ_START(ring) != obj->gtt_offset ||
	    (I915_READ_HEAD(ring) & HEAD_ADDR) != 0) {
		DRM_ERROR("%s initialization failed "
				"ctl %08x head %08x tail %08x start %08x\n",
				ring->name,
				I915_READ_CTL(ring),
				I915_READ_HEAD(ring),
				I915_READ_TAIL(ring),
				I915_READ_START(ring));
		return -EIO;
	}

	if (!drm_core_check_feature(ring->dev, DRIVER_MODESET))
		i915_kernel_lost_context(ring->dev);
	else {
		ring->head = I915_READ_HEAD(ring);
		ring->tail = I915_READ_TAIL(ring) & TAIL_ADDR;
		ring->space = ring_space(ring);
	}

	return 0;
}

static int
init_pipe_control(struct intel_ring_buffer *ring)
{
	struct pipe_control *pc;
	struct drm_i915_gem_object *obj;
	int ret;

	if (ring->private)
		return 0;

	pc = kmalloc(sizeof(*pc), GFP_KERNEL);
	if (!pc)
		return -ENOMEM;

	obj = i915_gem_alloc_object(ring->dev, 4096);
	if (obj == NULL) {
		DRM_ERROR("Failed to allocate seqno page\n");
		ret = -ENOMEM;
		goto err;
	}

	i915_gem_object_set_cache_level(obj, I915_CACHE_LLC);

	ret = i915_gem_object_pin(obj, 4096, true);
	if (ret)
		goto err_unref;

	pc->gtt_offset = obj->gtt_offset;
	pc->cpu_page =  kmap(obj->pages[0]);
	if (pc->cpu_page == NULL)
		goto err_unpin;

	pc->obj = obj;
	ring->private = pc;
	return 0;

err_unpin:
	i915_gem_object_unpin(obj);
err_unref:
	drm_gem_object_unreference(&obj->base);
err:
	kfree(pc);
	return ret;
}

static void
cleanup_pipe_control(struct intel_ring_buffer *ring)
{
	struct pipe_control *pc = ring->private;
	struct drm_i915_gem_object *obj;

	if (!ring->private)
		return;

	obj = pc->obj;
	kunmap(obj->pages[0]);
	i915_gem_object_unpin(obj);
	drm_gem_object_unreference(&obj->base);

	kfree(pc);
	ring->private = NULL;
}

static int init_render_ring(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	int ret = init_ring_common(ring);

	if (INTEL_INFO(dev)->gen > 3) {
		int mode = VS_TIMER_DISPATCH << 16 | VS_TIMER_DISPATCH;
		I915_WRITE(MI_MODE, mode);
		if (IS_GEN7(dev))
			I915_WRITE(GFX_MODE_GEN7,
				   GFX_MODE_DISABLE(GFX_TLB_INVALIDATE_ALWAYS) |
				   GFX_MODE_ENABLE(GFX_REPLAY_MODE));
	}

	if (INTEL_INFO(dev)->gen >= 5) {
		ret = init_pipe_control(ring);
		if (ret)
			return ret;
	}


	if (IS_GEN6(dev)) {
		I915_WRITE(CACHE_MODE_0,
			   CM0_STC_EVICT_DISABLE_LRA_SNB << CM0_MASK_SHIFT);
	}

	if (INTEL_INFO(dev)->gen >= 6) {
		I915_WRITE(INSTPM,
			   INSTPM_FORCE_ORDERING << 16 | INSTPM_FORCE_ORDERING);
	}

	return ret;
}

static void render_ring_cleanup(struct intel_ring_buffer *ring)
{
	if (!ring->private)
		return;

	cleanup_pipe_control(ring);
}

static void
update_mboxes(struct intel_ring_buffer *ring,
	    u32 seqno,
	    u32 mmio_offset)
{
	intel_ring_emit(ring, MI_SEMAPHORE_MBOX |
			      MI_SEMAPHORE_GLOBAL_GTT |
			      MI_SEMAPHORE_REGISTER |
			      MI_SEMAPHORE_UPDATE);
	intel_ring_emit(ring, seqno);
	intel_ring_emit(ring, mmio_offset);
}

static int
gen6_add_request(struct intel_ring_buffer *ring,
		 u32 *seqno)
{
	u32 mbox1_reg;
	u32 mbox2_reg;
	int ret;

	ret = intel_ring_begin(ring, 10);
	if (ret)
		return ret;

	mbox1_reg = ring->signal_mbox[0];
	mbox2_reg = ring->signal_mbox[1];

	*seqno = i915_gem_next_request_seqno(ring);

	update_mboxes(ring, *seqno, mbox1_reg);
	update_mboxes(ring, *seqno, mbox2_reg);
	intel_ring_emit(ring, MI_STORE_DWORD_INDEX);
	intel_ring_emit(ring, I915_GEM_HWS_INDEX << MI_STORE_DWORD_INDEX_SHIFT);
	intel_ring_emit(ring, *seqno);
	intel_ring_emit(ring, MI_USER_INTERRUPT);
	intel_ring_advance(ring);

	return 0;
}

static int
intel_ring_sync(struct intel_ring_buffer *waiter,
		struct intel_ring_buffer *signaller,
		int ring,
		u32 seqno)
{
	int ret;
	u32 dw1 = MI_SEMAPHORE_MBOX |
		  MI_SEMAPHORE_COMPARE |
		  MI_SEMAPHORE_REGISTER;

	ret = intel_ring_begin(waiter, 4);
	if (ret)
		return ret;

	intel_ring_emit(waiter, dw1 | signaller->semaphore_register[ring]);
	intel_ring_emit(waiter, seqno);
	intel_ring_emit(waiter, 0);
	intel_ring_emit(waiter, MI_NOOP);
	intel_ring_advance(waiter);

	return 0;
}

int
render_ring_sync_to(struct intel_ring_buffer *waiter,
		    struct intel_ring_buffer *signaller,
		    u32 seqno)
{
	WARN_ON(signaller->semaphore_register[RCS] == MI_SEMAPHORE_SYNC_INVALID);
	return intel_ring_sync(waiter,
			       signaller,
			       RCS,
			       seqno);
}

int
gen6_bsd_ring_sync_to(struct intel_ring_buffer *waiter,
		      struct intel_ring_buffer *signaller,
		      u32 seqno)
{
	WARN_ON(signaller->semaphore_register[VCS] == MI_SEMAPHORE_SYNC_INVALID);
	return intel_ring_sync(waiter,
			       signaller,
			       VCS,
			       seqno);
}

int
gen6_blt_ring_sync_to(struct intel_ring_buffer *waiter,
		      struct intel_ring_buffer *signaller,
		      u32 seqno)
{
	WARN_ON(signaller->semaphore_register[BCS] == MI_SEMAPHORE_SYNC_INVALID);
	return intel_ring_sync(waiter,
			       signaller,
			       BCS,
			       seqno);
}



#define PIPE_CONTROL_FLUSH(ring__, addr__)					\
do {									\
	intel_ring_emit(ring__, GFX_OP_PIPE_CONTROL(4) | PIPE_CONTROL_QW_WRITE |		\
		 PIPE_CONTROL_DEPTH_STALL);				\
	intel_ring_emit(ring__, (addr__) | PIPE_CONTROL_GLOBAL_GTT);			\
	intel_ring_emit(ring__, 0);							\
	intel_ring_emit(ring__, 0);							\
} while (0)

static int
pc_render_add_request(struct intel_ring_buffer *ring,
		      u32 *result)
{
	u32 seqno = i915_gem_next_request_seqno(ring);
	struct pipe_control *pc = ring->private;
	u32 scratch_addr = pc->gtt_offset + 128;
	int ret;

	ret = intel_ring_begin(ring, 32);
	if (ret)
		return ret;

	intel_ring_emit(ring, GFX_OP_PIPE_CONTROL(4) | PIPE_CONTROL_QW_WRITE |
			PIPE_CONTROL_WRITE_FLUSH |
			PIPE_CONTROL_TEXTURE_CACHE_INVALIDATE);
	intel_ring_emit(ring, pc->gtt_offset | PIPE_CONTROL_GLOBAL_GTT);
	intel_ring_emit(ring, seqno);
	intel_ring_emit(ring, 0);
	PIPE_CONTROL_FLUSH(ring, scratch_addr);
	scratch_addr += 128; 
	PIPE_CONTROL_FLUSH(ring, scratch_addr);
	scratch_addr += 128;
	PIPE_CONTROL_FLUSH(ring, scratch_addr);
	scratch_addr += 128;
	PIPE_CONTROL_FLUSH(ring, scratch_addr);
	scratch_addr += 128;
	PIPE_CONTROL_FLUSH(ring, scratch_addr);
	scratch_addr += 128;
	PIPE_CONTROL_FLUSH(ring, scratch_addr);

	intel_ring_emit(ring, GFX_OP_PIPE_CONTROL(4) | PIPE_CONTROL_QW_WRITE |
			PIPE_CONTROL_WRITE_FLUSH |
			PIPE_CONTROL_TEXTURE_CACHE_INVALIDATE |
			PIPE_CONTROL_NOTIFY);
	intel_ring_emit(ring, pc->gtt_offset | PIPE_CONTROL_GLOBAL_GTT);
	intel_ring_emit(ring, seqno);
	intel_ring_emit(ring, 0);
	intel_ring_advance(ring);

	*result = seqno;
	return 0;
}

static int
render_ring_add_request(struct intel_ring_buffer *ring,
			u32 *result)
{
	u32 seqno = i915_gem_next_request_seqno(ring);
	int ret;

	ret = intel_ring_begin(ring, 4);
	if (ret)
		return ret;

	intel_ring_emit(ring, MI_STORE_DWORD_INDEX);
	intel_ring_emit(ring, I915_GEM_HWS_INDEX << MI_STORE_DWORD_INDEX_SHIFT);
	intel_ring_emit(ring, seqno);
	intel_ring_emit(ring, MI_USER_INTERRUPT);
	intel_ring_advance(ring);

	*result = seqno;
	return 0;
}

static u32
gen6_ring_get_seqno(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;

	if (IS_GEN6(dev) || IS_GEN7(dev))
		intel_ring_get_active_head(ring);
	return intel_read_status_page(ring, I915_GEM_HWS_INDEX);
}

static u32
ring_get_seqno(struct intel_ring_buffer *ring)
{
	return intel_read_status_page(ring, I915_GEM_HWS_INDEX);
}

static u32
pc_render_get_seqno(struct intel_ring_buffer *ring)
{
	struct pipe_control *pc = ring->private;
	return pc->cpu_page[0];
}

static void
ironlake_enable_irq(drm_i915_private_t *dev_priv, u32 mask)
{
	dev_priv->gt_irq_mask &= ~mask;
	I915_WRITE(GTIMR, dev_priv->gt_irq_mask);
	POSTING_READ(GTIMR);
}

static void
ironlake_disable_irq(drm_i915_private_t *dev_priv, u32 mask)
{
	dev_priv->gt_irq_mask |= mask;
	I915_WRITE(GTIMR, dev_priv->gt_irq_mask);
	POSTING_READ(GTIMR);
}

static void
i915_enable_irq(drm_i915_private_t *dev_priv, u32 mask)
{
	dev_priv->irq_mask &= ~mask;
	I915_WRITE(IMR, dev_priv->irq_mask);
	POSTING_READ(IMR);
}

static void
i915_disable_irq(drm_i915_private_t *dev_priv, u32 mask)
{
	dev_priv->irq_mask |= mask;
	I915_WRITE(IMR, dev_priv->irq_mask);
	POSTING_READ(IMR);
}

static bool
render_ring_get_irq(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;

	if (!dev->irq_enabled)
		return false;

	spin_lock(&ring->irq_lock);
	if (ring->irq_refcount++ == 0) {
		if (HAS_PCH_SPLIT(dev))
			ironlake_enable_irq(dev_priv,
					    GT_PIPE_NOTIFY | GT_USER_INTERRUPT);
		else
			i915_enable_irq(dev_priv, I915_USER_INTERRUPT);
	}
	spin_unlock(&ring->irq_lock);

	return true;
}

static void
render_ring_put_irq(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;

	spin_lock(&ring->irq_lock);
	if (--ring->irq_refcount == 0) {
		if (HAS_PCH_SPLIT(dev))
			ironlake_disable_irq(dev_priv,
					     GT_USER_INTERRUPT |
					     GT_PIPE_NOTIFY);
		else
			i915_disable_irq(dev_priv, I915_USER_INTERRUPT);
	}
	spin_unlock(&ring->irq_lock);
}

void intel_ring_setup_status_page(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = ring->dev->dev_private;
	u32 mmio = 0;

	if (IS_GEN7(dev)) {
		switch (ring->id) {
		case RCS:
			mmio = RENDER_HWS_PGA_GEN7;
			break;
		case BCS:
			mmio = BLT_HWS_PGA_GEN7;
			break;
		case VCS:
			mmio = BSD_HWS_PGA_GEN7;
			break;
		}
	} else if (IS_GEN6(ring->dev)) {
		mmio = RING_HWS_PGA_GEN6(ring->mmio_base);
	} else {
		mmio = RING_HWS_PGA(ring->mmio_base);
	}

	I915_WRITE(mmio, (u32)ring->status_page.gfx_addr);
	POSTING_READ(mmio);
}

static int
bsd_ring_flush(struct intel_ring_buffer *ring,
	       u32     invalidate_domains,
	       u32     flush_domains)
{
	int ret;

	ret = intel_ring_begin(ring, 2);
	if (ret)
		return ret;

	intel_ring_emit(ring, MI_FLUSH);
	intel_ring_emit(ring, MI_NOOP);
	intel_ring_advance(ring);
	return 0;
}

static int
ring_add_request(struct intel_ring_buffer *ring,
		 u32 *result)
{
	u32 seqno;
	int ret;

	ret = intel_ring_begin(ring, 4);
	if (ret)
		return ret;

	seqno = i915_gem_next_request_seqno(ring);

	intel_ring_emit(ring, MI_STORE_DWORD_INDEX);
	intel_ring_emit(ring, I915_GEM_HWS_INDEX << MI_STORE_DWORD_INDEX_SHIFT);
	intel_ring_emit(ring, seqno);
	intel_ring_emit(ring, MI_USER_INTERRUPT);
	intel_ring_advance(ring);

	*result = seqno;
	return 0;
}

static bool
gen6_ring_get_irq(struct intel_ring_buffer *ring, u32 gflag, u32 rflag)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;

	if (!dev->irq_enabled)
	       return false;

	gen6_gt_force_wake_get(dev_priv);

	spin_lock(&ring->irq_lock);
	if (ring->irq_refcount++ == 0) {
		ring->irq_mask &= ~rflag;
		I915_WRITE_IMR(ring, ring->irq_mask);
		ironlake_enable_irq(dev_priv, gflag);
	}
	spin_unlock(&ring->irq_lock);

	return true;
}

static void
gen6_ring_put_irq(struct intel_ring_buffer *ring, u32 gflag, u32 rflag)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;

	spin_lock(&ring->irq_lock);
	if (--ring->irq_refcount == 0) {
		ring->irq_mask |= rflag;
		I915_WRITE_IMR(ring, ring->irq_mask);
		ironlake_disable_irq(dev_priv, gflag);
	}
	spin_unlock(&ring->irq_lock);

	gen6_gt_force_wake_put(dev_priv);
}

static bool
bsd_ring_get_irq(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;

	if (!dev->irq_enabled)
		return false;

	spin_lock(&ring->irq_lock);
	if (ring->irq_refcount++ == 0) {
		if (IS_G4X(dev))
			i915_enable_irq(dev_priv, I915_BSD_USER_INTERRUPT);
		else
			ironlake_enable_irq(dev_priv, GT_BSD_USER_INTERRUPT);
	}
	spin_unlock(&ring->irq_lock);

	return true;
}
static void
bsd_ring_put_irq(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;

	spin_lock(&ring->irq_lock);
	if (--ring->irq_refcount == 0) {
		if (IS_G4X(dev))
			i915_disable_irq(dev_priv, I915_BSD_USER_INTERRUPT);
		else
			ironlake_disable_irq(dev_priv, GT_BSD_USER_INTERRUPT);
	}
	spin_unlock(&ring->irq_lock);
}

static int
ring_dispatch_execbuffer(struct intel_ring_buffer *ring, u32 offset, u32 length)
{
	int ret;

	ret = intel_ring_begin(ring, 2);
	if (ret)
		return ret;

	intel_ring_emit(ring,
			MI_BATCH_BUFFER_START | (2 << 6) |
			MI_BATCH_NON_SECURE_I965);
	intel_ring_emit(ring, offset);
	intel_ring_advance(ring);

	return 0;
}

static int
render_ring_dispatch_execbuffer(struct intel_ring_buffer *ring,
				u32 offset, u32 len)
{
	struct drm_device *dev = ring->dev;
	int ret;

	if (IS_I830(dev) || IS_845G(dev)) {
		ret = intel_ring_begin(ring, 4);
		if (ret)
			return ret;

		intel_ring_emit(ring, MI_BATCH_BUFFER);
		intel_ring_emit(ring, offset | MI_BATCH_NON_SECURE);
		intel_ring_emit(ring, offset + len - 8);
		intel_ring_emit(ring, 0);
	} else {
		ret = intel_ring_begin(ring, 2);
		if (ret)
			return ret;

		if (INTEL_INFO(dev)->gen >= 4) {
			intel_ring_emit(ring,
					MI_BATCH_BUFFER_START | (2 << 6) |
					MI_BATCH_NON_SECURE_I965);
			intel_ring_emit(ring, offset);
		} else {
			intel_ring_emit(ring,
					MI_BATCH_BUFFER_START | (2 << 6));
			intel_ring_emit(ring, offset | MI_BATCH_NON_SECURE);
		}
	}
	intel_ring_advance(ring);

	return 0;
}

static void cleanup_status_page(struct intel_ring_buffer *ring)
{
	drm_i915_private_t *dev_priv = ring->dev->dev_private;
	struct drm_i915_gem_object *obj;

	obj = ring->status_page.obj;
	if (obj == NULL)
		return;

	kunmap(obj->pages[0]);
	i915_gem_object_unpin(obj);
	drm_gem_object_unreference(&obj->base);
	ring->status_page.obj = NULL;

	memset(&dev_priv->hws_map, 0, sizeof(dev_priv->hws_map));
}

static int init_status_page(struct intel_ring_buffer *ring)
{
	struct drm_device *dev = ring->dev;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj;
	int ret;

	obj = i915_gem_alloc_object(dev, 4096);
	if (obj == NULL) {
		DRM_ERROR("Failed to allocate status page\n");
		ret = -ENOMEM;
		goto err;
	}

	i915_gem_object_set_cache_level(obj, I915_CACHE_LLC);

	ret = i915_gem_object_pin(obj, 4096, true);
	if (ret != 0) {
		goto err_unref;
	}

	ring->status_page.gfx_addr = obj->gtt_offset;
	ring->status_page.page_addr = kmap(obj->pages[0]);
	if (ring->status_page.page_addr == NULL) {
		memset(&dev_priv->hws_map, 0, sizeof(dev_priv->hws_map));
		goto err_unpin;
	}
	ring->status_page.obj = obj;
	memset(ring->status_page.page_addr, 0, PAGE_SIZE);

	intel_ring_setup_status_page(ring);
	DRM_DEBUG_DRIVER("%s hws offset: 0x%08x\n",
			ring->name, ring->status_page.gfx_addr);

	return 0;

err_unpin:
	i915_gem_object_unpin(obj);
err_unref:
	drm_gem_object_unreference(&obj->base);
err:
	return ret;
}

int intel_init_ring_buffer(struct drm_device *dev,
			   struct intel_ring_buffer *ring)
{
	struct drm_i915_gem_object *obj;
	int ret;

	ring->dev = dev;
	INIT_LIST_HEAD(&ring->active_list);
	INIT_LIST_HEAD(&ring->request_list);
	INIT_LIST_HEAD(&ring->gpu_write_list);

	init_waitqueue_head(&ring->irq_queue);
	spin_lock_init(&ring->irq_lock);
	ring->irq_mask = ~0;

	if (I915_NEED_GFX_HWS(dev)) {
		ret = init_status_page(ring);
		if (ret)
			return ret;
	}

	obj = i915_gem_alloc_object(dev, ring->size);
	if (obj == NULL) {
		DRM_ERROR("Failed to allocate ringbuffer\n");
		ret = -ENOMEM;
		goto err_hws;
	}

	ring->obj = obj;

	ret = i915_gem_object_pin(obj, PAGE_SIZE, true);
	if (ret)
		goto err_unref;

	ring->map.size = ring->size;
	ring->map.offset = dev->agp->base + obj->gtt_offset;
	ring->map.type = 0;
	ring->map.flags = 0;
	ring->map.mtrr = 0;

	drm_core_ioremap_wc(&ring->map, dev);
	if (ring->map.handle == NULL) {
		DRM_ERROR("Failed to map ringbuffer.\n");
		ret = -EINVAL;
		goto err_unpin;
	}

	ring->virtual_start = ring->map.handle;
	ret = ring->init(ring);
	if (ret)
		goto err_unmap;

	ring->effective_size = ring->size;
	if (IS_I830(ring->dev) || IS_845G(ring->dev))
		ring->effective_size -= 128;

	return 0;

err_unmap:
	drm_core_ioremapfree(&ring->map, dev);
err_unpin:
	i915_gem_object_unpin(obj);
err_unref:
	drm_gem_object_unreference(&obj->base);
	ring->obj = NULL;
err_hws:
	cleanup_status_page(ring);
	return ret;
}

void intel_cleanup_ring_buffer(struct intel_ring_buffer *ring)
{
	struct drm_i915_private *dev_priv;
	int ret;

	if (ring->obj == NULL)
		return;

	
	dev_priv = ring->dev->dev_private;
	ret = intel_wait_ring_idle(ring);
	if (ret)
		DRM_ERROR("failed to quiesce %s whilst cleaning up: %d\n",
			  ring->name, ret);

	I915_WRITE_CTL(ring, 0);

	drm_core_ioremapfree(&ring->map, ring->dev);

	i915_gem_object_unpin(ring->obj);
	drm_gem_object_unreference(&ring->obj->base);
	ring->obj = NULL;

	if (ring->cleanup)
		ring->cleanup(ring);

	cleanup_status_page(ring);
}

static int intel_wrap_ring_buffer(struct intel_ring_buffer *ring)
{
	unsigned int *virt;
	int rem = ring->size - ring->tail;

	if (ring->space < rem) {
		int ret = intel_wait_ring_buffer(ring, rem);
		if (ret)
			return ret;
	}

	virt = (unsigned int *)(ring->virtual_start + ring->tail);
	rem /= 8;
	while (rem--) {
		*virt++ = MI_NOOP;
		*virt++ = MI_NOOP;
	}

	ring->tail = 0;
	ring->space = ring_space(ring);

	return 0;
}

static int intel_ring_wait_seqno(struct intel_ring_buffer *ring, u32 seqno)
{
	struct drm_i915_private *dev_priv = ring->dev->dev_private;
	bool was_interruptible;
	int ret;

	was_interruptible = dev_priv->mm.interruptible;
	dev_priv->mm.interruptible = false;

	ret = i915_wait_request(ring, seqno, true);

	dev_priv->mm.interruptible = was_interruptible;

	return ret;
}

static int intel_ring_wait_request(struct intel_ring_buffer *ring, int n)
{
	struct drm_i915_gem_request *request;
	u32 seqno = 0;
	int ret;

	i915_gem_retire_requests_ring(ring);

	if (ring->last_retired_head != -1) {
		ring->head = ring->last_retired_head;
		ring->last_retired_head = -1;
		ring->space = ring_space(ring);
		if (ring->space >= n)
			return 0;
	}

	list_for_each_entry(request, &ring->request_list, list) {
		int space;

		if (request->tail == -1)
			continue;

		space = request->tail - (ring->tail + 8);
		if (space < 0)
			space += ring->size;
		if (space >= n) {
			seqno = request->seqno;
			break;
		}

		request->tail = -1;
	}

	if (seqno == 0)
		return -ENOSPC;

	ret = intel_ring_wait_seqno(ring, seqno);
	if (ret)
		return ret;

	if (WARN_ON(ring->last_retired_head == -1))
		return -ENOSPC;

	ring->head = ring->last_retired_head;
	ring->last_retired_head = -1;
	ring->space = ring_space(ring);
	if (WARN_ON(ring->space < n))
		return -ENOSPC;

	return 0;
}

int intel_wait_ring_buffer(struct intel_ring_buffer *ring, int n)
{
	struct drm_device *dev = ring->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	unsigned long end;
	int ret;

	ret = intel_ring_wait_request(ring, n);
	if (ret != -ENOSPC)
		return ret;

	trace_i915_ring_wait_begin(ring);
	if (drm_core_check_feature(dev, DRIVER_GEM))
		end = jiffies + 60 * HZ;
	else
		end = jiffies + 3 * HZ;

	do {
		ring->head = I915_READ_HEAD(ring);
		ring->space = ring_space(ring);
		if (ring->space >= n) {
			trace_i915_ring_wait_end(ring);
			return 0;
		}

		if (dev->primary->master) {
			struct drm_i915_master_private *master_priv = dev->primary->master->driver_priv;
			if (master_priv->sarea_priv)
				master_priv->sarea_priv->perf_boxes |= I915_BOX_WAIT;
		}

		msleep(1);
		if (atomic_read(&dev_priv->mm.wedged))
			return -EAGAIN;
	} while (!time_after(jiffies, end));
	trace_i915_ring_wait_end(ring);
	return -EBUSY;
}

int intel_ring_begin(struct intel_ring_buffer *ring,
		     int num_dwords)
{
	struct drm_i915_private *dev_priv = ring->dev->dev_private;
	int n = 4*num_dwords;
	int ret;

	if (unlikely(atomic_read(&dev_priv->mm.wedged)))
		return -EIO;

	if (unlikely(ring->tail + n > ring->effective_size)) {
		ret = intel_wrap_ring_buffer(ring);
		if (unlikely(ret))
			return ret;
	}

	if (unlikely(ring->space < n)) {
		ret = intel_wait_ring_buffer(ring, n);
		if (unlikely(ret))
			return ret;
	}

	ring->space -= n;
	return 0;
}

void intel_ring_advance(struct intel_ring_buffer *ring)
{
	ring->tail &= ring->size - 1;
	ring->write_tail(ring, ring->tail);
}

static const struct intel_ring_buffer render_ring = {
	.name			= "render ring",
	.id			= RCS,
	.mmio_base		= RENDER_RING_BASE,
	.size			= 32 * PAGE_SIZE,
	.init			= init_render_ring,
	.write_tail		= ring_write_tail,
	.flush			= render_ring_flush,
	.add_request		= render_ring_add_request,
	.get_seqno		= ring_get_seqno,
	.irq_get		= render_ring_get_irq,
	.irq_put		= render_ring_put_irq,
	.dispatch_execbuffer	= render_ring_dispatch_execbuffer,
	.cleanup		= render_ring_cleanup,
	.sync_to		= render_ring_sync_to,
	.semaphore_register	= {MI_SEMAPHORE_SYNC_INVALID,
				   MI_SEMAPHORE_SYNC_RV,
				   MI_SEMAPHORE_SYNC_RB},
	.signal_mbox		= {GEN6_VRSYNC, GEN6_BRSYNC},
};


static const struct intel_ring_buffer bsd_ring = {
	.name                   = "bsd ring",
	.id			= VCS,
	.mmio_base		= BSD_RING_BASE,
	.size			= 32 * PAGE_SIZE,
	.init			= init_ring_common,
	.write_tail		= ring_write_tail,
	.flush			= bsd_ring_flush,
	.add_request		= ring_add_request,
	.get_seqno		= ring_get_seqno,
	.irq_get		= bsd_ring_get_irq,
	.irq_put		= bsd_ring_put_irq,
	.dispatch_execbuffer	= ring_dispatch_execbuffer,
};


static void gen6_bsd_ring_write_tail(struct intel_ring_buffer *ring,
				     u32 value)
{
	drm_i915_private_t *dev_priv = ring->dev->dev_private;

       
	I915_WRITE(GEN6_BSD_SLEEP_PSMI_CONTROL,
		GEN6_BSD_SLEEP_PSMI_CONTROL_RC_ILDL_MESSAGE_MODIFY_MASK |
		GEN6_BSD_SLEEP_PSMI_CONTROL_RC_ILDL_MESSAGE_DISABLE);
	I915_WRITE(GEN6_BSD_RNCID, 0x0);

	if (wait_for((I915_READ(GEN6_BSD_SLEEP_PSMI_CONTROL) &
		GEN6_BSD_SLEEP_PSMI_CONTROL_IDLE_INDICATOR) == 0,
		50))
	DRM_ERROR("timed out waiting for IDLE Indicator\n");

	I915_WRITE_TAIL(ring, value);
	I915_WRITE(GEN6_BSD_SLEEP_PSMI_CONTROL,
		GEN6_BSD_SLEEP_PSMI_CONTROL_RC_ILDL_MESSAGE_MODIFY_MASK |
		GEN6_BSD_SLEEP_PSMI_CONTROL_RC_ILDL_MESSAGE_ENABLE);
}

static int gen6_ring_flush(struct intel_ring_buffer *ring,
			   u32 invalidate, u32 flush)
{
	uint32_t cmd;
	int ret;

	ret = intel_ring_begin(ring, 4);
	if (ret)
		return ret;

	cmd = MI_FLUSH_DW;
	if (invalidate & I915_GEM_GPU_DOMAINS)
		cmd |= MI_INVALIDATE_TLB | MI_INVALIDATE_BSD;
	intel_ring_emit(ring, cmd);
	intel_ring_emit(ring, 0);
	intel_ring_emit(ring, 0);
	intel_ring_emit(ring, MI_NOOP);
	intel_ring_advance(ring);
	return 0;
}

static int
gen6_ring_dispatch_execbuffer(struct intel_ring_buffer *ring,
			      u32 offset, u32 len)
{
	int ret;

	ret = intel_ring_begin(ring, 2);
	if (ret)
		return ret;

	intel_ring_emit(ring, MI_BATCH_BUFFER_START | MI_BATCH_NON_SECURE_I965);
	
	intel_ring_emit(ring, offset);
	intel_ring_advance(ring);

	return 0;
}

static bool
gen6_render_ring_get_irq(struct intel_ring_buffer *ring)
{
	return gen6_ring_get_irq(ring,
				 GT_USER_INTERRUPT,
				 GEN6_RENDER_USER_INTERRUPT);
}

static void
gen6_render_ring_put_irq(struct intel_ring_buffer *ring)
{
	return gen6_ring_put_irq(ring,
				 GT_USER_INTERRUPT,
				 GEN6_RENDER_USER_INTERRUPT);
}

static bool
gen6_bsd_ring_get_irq(struct intel_ring_buffer *ring)
{
	return gen6_ring_get_irq(ring,
				 GT_GEN6_BSD_USER_INTERRUPT,
				 GEN6_BSD_USER_INTERRUPT);
}

static void
gen6_bsd_ring_put_irq(struct intel_ring_buffer *ring)
{
	return gen6_ring_put_irq(ring,
				 GT_GEN6_BSD_USER_INTERRUPT,
				 GEN6_BSD_USER_INTERRUPT);
}

static const struct intel_ring_buffer gen6_bsd_ring = {
	.name			= "gen6 bsd ring",
	.id			= VCS,
	.mmio_base		= GEN6_BSD_RING_BASE,
	.size			= 32 * PAGE_SIZE,
	.init			= init_ring_common,
	.write_tail		= gen6_bsd_ring_write_tail,
	.flush			= gen6_ring_flush,
	.add_request		= gen6_add_request,
	.get_seqno		= gen6_ring_get_seqno,
	.irq_get		= gen6_bsd_ring_get_irq,
	.irq_put		= gen6_bsd_ring_put_irq,
	.dispatch_execbuffer	= gen6_ring_dispatch_execbuffer,
	.sync_to		= gen6_bsd_ring_sync_to,
	.semaphore_register	= {MI_SEMAPHORE_SYNC_VR,
				   MI_SEMAPHORE_SYNC_INVALID,
				   MI_SEMAPHORE_SYNC_VB},
	.signal_mbox		= {GEN6_RVSYNC, GEN6_BVSYNC},
};


static bool
blt_ring_get_irq(struct intel_ring_buffer *ring)
{
	return gen6_ring_get_irq(ring,
				 GT_BLT_USER_INTERRUPT,
				 GEN6_BLITTER_USER_INTERRUPT);
}

static void
blt_ring_put_irq(struct intel_ring_buffer *ring)
{
	gen6_ring_put_irq(ring,
			  GT_BLT_USER_INTERRUPT,
			  GEN6_BLITTER_USER_INTERRUPT);
}

static int blt_ring_flush(struct intel_ring_buffer *ring,
			  u32 invalidate, u32 flush)
{
	uint32_t cmd;
	int ret;

	ret = intel_ring_begin(ring, 4);
	if (ret)
		return ret;

	cmd = MI_FLUSH_DW;
	if (invalidate & I915_GEM_DOMAIN_RENDER)
		cmd |= MI_INVALIDATE_TLB;
	intel_ring_emit(ring, cmd);
	intel_ring_emit(ring, 0);
	intel_ring_emit(ring, 0);
	intel_ring_emit(ring, MI_NOOP);
	intel_ring_advance(ring);
	return 0;
}

static const struct intel_ring_buffer gen6_blt_ring = {
	.name			= "blt ring",
	.id			= BCS,
	.mmio_base		= BLT_RING_BASE,
	.size			= 32 * PAGE_SIZE,
	.init			= init_ring_common,
	.write_tail		= ring_write_tail,
	.flush			= blt_ring_flush,
	.add_request		= gen6_add_request,
	.get_seqno		= gen6_ring_get_seqno,
	.irq_get		= blt_ring_get_irq,
	.irq_put		= blt_ring_put_irq,
	.dispatch_execbuffer	= gen6_ring_dispatch_execbuffer,
	.sync_to		= gen6_blt_ring_sync_to,
	.semaphore_register	= {MI_SEMAPHORE_SYNC_BR,
				   MI_SEMAPHORE_SYNC_BV,
				   MI_SEMAPHORE_SYNC_INVALID},
	.signal_mbox		= {GEN6_RBSYNC, GEN6_VBSYNC},
};

int intel_init_render_ring_buffer(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring = &dev_priv->ring[RCS];

	*ring = render_ring;
	if (INTEL_INFO(dev)->gen >= 6) {
		ring->add_request = gen6_add_request;
		ring->flush = gen6_render_ring_flush;
		ring->irq_get = gen6_render_ring_get_irq;
		ring->irq_put = gen6_render_ring_put_irq;
		ring->get_seqno = gen6_ring_get_seqno;
	} else if (IS_GEN5(dev)) {
		ring->add_request = pc_render_add_request;
		ring->get_seqno = pc_render_get_seqno;
	}

	if (!I915_NEED_GFX_HWS(dev)) {
		ring->status_page.page_addr = dev_priv->status_page_dmah->vaddr;
		memset(ring->status_page.page_addr, 0, PAGE_SIZE);
	}

	return intel_init_ring_buffer(dev, ring);
}

int intel_render_ring_init_dri(struct drm_device *dev, u64 start, u32 size)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring = &dev_priv->ring[RCS];

	*ring = render_ring;
	if (INTEL_INFO(dev)->gen >= 6) {
		ring->add_request = gen6_add_request;
		ring->irq_get = gen6_render_ring_get_irq;
		ring->irq_put = gen6_render_ring_put_irq;
	} else if (IS_GEN5(dev)) {
		ring->add_request = pc_render_add_request;
		ring->get_seqno = pc_render_get_seqno;
	}

	if (!I915_NEED_GFX_HWS(dev))
		ring->status_page.page_addr = dev_priv->status_page_dmah->vaddr;

	ring->dev = dev;
	INIT_LIST_HEAD(&ring->active_list);
	INIT_LIST_HEAD(&ring->request_list);
	INIT_LIST_HEAD(&ring->gpu_write_list);

	ring->size = size;
	ring->effective_size = ring->size;
	if (IS_I830(ring->dev))
		ring->effective_size -= 128;

	ring->map.offset = start;
	ring->map.size = size;
	ring->map.type = 0;
	ring->map.flags = 0;
	ring->map.mtrr = 0;

	drm_core_ioremap_wc(&ring->map, dev);
	if (ring->map.handle == NULL) {
		DRM_ERROR("can not ioremap virtual address for"
			  " ring buffer\n");
		return -ENOMEM;
	}

	ring->virtual_start = (void __force __iomem *)ring->map.handle;
	return 0;
}

int intel_init_bsd_ring_buffer(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring = &dev_priv->ring[VCS];

	if (IS_GEN6(dev) || IS_GEN7(dev))
		*ring = gen6_bsd_ring;
	else
		*ring = bsd_ring;

	return intel_init_ring_buffer(dev, ring);
}

int intel_init_blt_ring_buffer(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct intel_ring_buffer *ring = &dev_priv->ring[BCS];

	*ring = gen6_blt_ring;

	return intel_init_ring_buffer(dev, ring);
}
