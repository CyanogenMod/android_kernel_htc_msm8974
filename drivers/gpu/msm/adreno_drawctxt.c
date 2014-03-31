/* Copyright (c) 2002,2007-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/slab.h>
#include <linux/msm_kgsl.h>
#include <linux/sched.h>

#include "kgsl.h"
#include "kgsl_sharedmem.h"
#include "adreno.h"
#include "adreno_trace.h"

#define KGSL_INIT_REFTIMESTAMP		0x7FFFFFFF

#define QUAD_LEN 12
#define QUAD_RESTORE_LEN 14

static unsigned int gmem_copy_quad[QUAD_LEN] = {
	0x00000000, 0x00000000, 0x3f800000,
	0x00000000, 0x00000000, 0x3f800000,
	0x00000000, 0x00000000, 0x3f800000,
	0x00000000, 0x00000000, 0x3f800000
};

static unsigned int gmem_restore_quad[QUAD_RESTORE_LEN] = {
	0x00000000, 0x3f800000, 0x3f800000,
	0x00000000, 0x00000000, 0x00000000,
	0x3f800000, 0x00000000, 0x00000000,
	0x3f800000, 0x00000000, 0x00000000,
	0x3f800000, 0x3f800000,
};

#define TEXCOORD_LEN 8

static unsigned int gmem_copy_texcoord[TEXCOORD_LEN] = {
	0x00000000, 0x3f800000,
	0x3f800000, 0x3f800000,
	0x00000000, 0x00000000,
	0x3f800000, 0x00000000
};



unsigned int uint2float(unsigned int uintval)
{
	unsigned int exp, frac = 0;

	if (uintval == 0)
		return 0;

	exp = ilog2(uintval);

	
	if (23 > exp)
		frac = (uintval & (~(1 << exp))) << (23 - exp);

	
	exp = (exp + 127) << 23;

	return exp | frac;
}

static void set_gmem_copy_quad(struct gmem_shadow_t *shadow)
{
	
	gmem_copy_quad[1] = uint2float(shadow->height);
	gmem_copy_quad[3] = uint2float(shadow->width);
	gmem_copy_quad[4] = uint2float(shadow->height);
	gmem_copy_quad[9] = uint2float(shadow->width);

	gmem_restore_quad[5] = uint2float(shadow->height);
	gmem_restore_quad[7] = uint2float(shadow->width);

	memcpy(shadow->quad_vertices.hostptr, gmem_copy_quad, QUAD_LEN << 2);
	memcpy(shadow->quad_vertices_restore.hostptr, gmem_restore_quad,
		QUAD_RESTORE_LEN << 2);

	memcpy(shadow->quad_texcoords.hostptr, gmem_copy_texcoord,
		TEXCOORD_LEN << 2);
}


void build_quad_vtxbuff(struct adreno_context *drawctxt,
		struct gmem_shadow_t *shadow, unsigned int **incmd)
{
	 unsigned int *cmd = *incmd;

	
	shadow->quad_vertices.hostptr = cmd;
	shadow->quad_vertices.gpuaddr = virt2gpu(cmd, &drawctxt->gpustate);

	cmd += QUAD_LEN;

	
	shadow->quad_vertices_restore.hostptr = cmd;
	shadow->quad_vertices_restore.gpuaddr =
		virt2gpu(cmd, &drawctxt->gpustate);

	cmd += QUAD_RESTORE_LEN;

	
	shadow->quad_texcoords.hostptr = cmd;
	shadow->quad_texcoords.gpuaddr = virt2gpu(cmd, &drawctxt->gpustate);

	cmd += TEXCOORD_LEN;

	set_gmem_copy_quad(shadow);
	*incmd = cmd;
}

static void wait_callback(struct kgsl_device *device, void *priv, u32 id,
		u32 timestamp, u32 type)
{
	struct adreno_context *drawctxt = priv;
	wake_up_all(&drawctxt->waiting);
}

#define adreno_wait_event_interruptible_timeout(wq, condition, timeout, io)   \
({                                                                            \
	long __ret = timeout;                                                 \
	if (io)                                                               \
		__wait_io_event_interruptible_timeout(wq, condition, __ret);  \
	else                                                                  \
		__wait_event_interruptible_timeout(wq, condition, __ret);     \
	__ret;                                                                \
})

#define adreno_wait_event_interruptible(wq, condition, io)                    \
({                                                                            \
	long __ret;                                                           \
	if (io)                                                               \
		__wait_io_event_interruptible(wq, condition, __ret);          \
	else                                                                  \
		__wait_event_interruptible(wq, condition, __ret);             \
	__ret;                                                                \
})

static int _check_context_timestamp(struct kgsl_device *device,
		struct adreno_context *drawctxt, unsigned int timestamp)
{
	int ret = 0;

	
	if (kgsl_context_detached(&drawctxt->base) ||
		drawctxt->state != ADRENO_CONTEXT_STATE_ACTIVE)
		return 1;

	mutex_lock(&device->mutex);
	ret = kgsl_check_timestamp(device, &drawctxt->base, timestamp);
	mutex_unlock(&device->mutex);

	return ret;
}

int adreno_drawctxt_wait(struct adreno_device *adreno_dev,
		struct kgsl_context *context,
		uint32_t timestamp, unsigned int timeout)
{
	static unsigned int io_cnt;
	struct kgsl_device *device = &adreno_dev->dev;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
	int ret, io;

	if (kgsl_context_detached(context))
		return -EINVAL;

	if (drawctxt->state == ADRENO_CONTEXT_STATE_INVALID)
		return -EDEADLK;

	
	BUG_ON(!mutex_is_locked(&device->mutex));

	trace_adreno_drawctxt_wait_start(context->id, timestamp);

	ret = kgsl_add_event(device, context->id, timestamp,
		wait_callback, drawctxt, NULL);
	if (ret)
		goto done;


	io_cnt = (io_cnt + 1) % 100;
	io = (io_cnt < pwr->pwrlevels[pwr->active_pwrlevel].io_fraction)
		? 0 : 1;

	mutex_unlock(&device->mutex);

	if (timeout) {
		ret = (int) adreno_wait_event_interruptible_timeout(
			drawctxt->waiting,
			_check_context_timestamp(device, drawctxt, timestamp),
			msecs_to_jiffies(timeout), io);

		if (ret == 0)
			ret = -ETIMEDOUT;
		else if (ret > 0)
			ret = 0;
	} else {
		ret = (int) adreno_wait_event_interruptible(drawctxt->waiting,
			_check_context_timestamp(device, drawctxt, timestamp),
				io);
	}

	mutex_lock(&device->mutex);

	
	if (drawctxt->state == ADRENO_CONTEXT_STATE_INVALID)
		ret = -EDEADLK;


	
	if (kgsl_context_detached(context))
		ret = -EINVAL;

done:
	trace_adreno_drawctxt_wait_done(context->id, timestamp, ret);
	return ret;
}

static void global_wait_callback(struct kgsl_device *device, void *priv, u32 id,
		u32 timestamp, u32 type)
{
	struct adreno_context *drawctxt = priv;

	wake_up_all(&drawctxt->waiting);
	kgsl_context_put(&drawctxt->base);
}

static int _check_global_timestamp(struct kgsl_device *device,
		struct adreno_context *drawctxt, unsigned int timestamp)
{
	
	if (drawctxt->state == ADRENO_CONTEXT_STATE_INVALID)
		return 1;

	return kgsl_check_timestamp(device, NULL, timestamp);
}

int adreno_drawctxt_wait_global(struct adreno_device *adreno_dev,
		struct kgsl_context *context,
		uint32_t timestamp, unsigned int timeout)
{
	struct kgsl_device *device = &adreno_dev->dev;
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);
	int ret = 0;

	
	BUG_ON(!mutex_is_locked(&device->mutex));

	if (!_kgsl_context_get(context)) {
		ret = -EINVAL;
		goto done;
	}

	if (drawctxt->state == ADRENO_CONTEXT_STATE_INVALID) {
		kgsl_context_put(context);
		goto done;
	}

	trace_adreno_drawctxt_wait_start(KGSL_MEMSTORE_GLOBAL, timestamp);

	ret = kgsl_add_event(device, KGSL_MEMSTORE_GLOBAL, timestamp,
		global_wait_callback, drawctxt, NULL);
	if (ret) {
		kgsl_context_put(context);
		goto done;
	}

	mutex_unlock(&device->mutex);

	if (timeout) {
		ret = (int) wait_event_timeout(drawctxt->waiting,
			_check_global_timestamp(device, drawctxt, timestamp),
			msecs_to_jiffies(timeout));

		if (ret == 0)
			ret = -ETIMEDOUT;
		else if (ret > 0)
			ret = 0;
	} else {
		wait_event(drawctxt->waiting,
			_check_global_timestamp(device, drawctxt, timestamp));
	}

	mutex_lock(&device->mutex);

	if (ret)
		kgsl_cancel_events_timestamp(device, NULL, timestamp);

done:
	trace_adreno_drawctxt_wait_done(KGSL_MEMSTORE_GLOBAL, timestamp, ret);
	return ret;
}

void adreno_drawctxt_invalidate(struct kgsl_device *device,
		struct kgsl_context *context)
{
	struct adreno_context *drawctxt = ADRENO_CONTEXT(context);

	trace_adreno_drawctxt_invalidate(drawctxt);

	drawctxt->state = ADRENO_CONTEXT_STATE_INVALID;

	
	mutex_lock(&drawctxt->mutex);

	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, soptimestamp),
			drawctxt->timestamp);

	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, eoptimestamp),
			drawctxt->timestamp);

	while (drawctxt->cmdqueue_head != drawctxt->cmdqueue_tail) {
		struct kgsl_cmdbatch *cmdbatch =
			drawctxt->cmdqueue[drawctxt->cmdqueue_head];

		drawctxt->cmdqueue_head = (drawctxt->cmdqueue_head + 1) %
			ADRENO_CONTEXT_CMDQUEUE_SIZE;

		mutex_unlock(&drawctxt->mutex);

		mutex_lock(&device->mutex);
		kgsl_cancel_events_timestamp(device, context,
			cmdbatch->timestamp);
		mutex_unlock(&device->mutex);

		kgsl_cmdbatch_destroy(cmdbatch);
		mutex_lock(&drawctxt->mutex);
	}

	mutex_unlock(&drawctxt->mutex);

	
	wake_up_all(&drawctxt->waiting);
	wake_up_all(&drawctxt->wq);
}

struct kgsl_context *
adreno_drawctxt_create(struct kgsl_device_private *dev_priv,
			uint32_t *flags)
{
	struct adreno_context *drawctxt;
	struct kgsl_device *device = dev_priv->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int ret;

	drawctxt = kzalloc(sizeof(struct adreno_context), GFP_KERNEL);

	if (drawctxt == NULL)
		return ERR_PTR(-ENOMEM);

	ret = kgsl_context_init(dev_priv, &drawctxt->base);
	if (ret != 0) {
		kfree(drawctxt);
		return ERR_PTR(ret);
	}

	drawctxt->bin_base_offset = 0;
	drawctxt->timestamp = 0;

	drawctxt->base.flags = *flags & (KGSL_CONTEXT_PREAMBLE |
		KGSL_CONTEXT_NO_GMEM_ALLOC |
		KGSL_CONTEXT_PER_CONTEXT_TS |
		KGSL_CONTEXT_USER_GENERATED_TS |
		KGSL_CONTEXT_NO_FAULT_TOLERANCE |
		KGSL_CONTEXT_TYPE_MASK);

	
	drawctxt->base.flags |= KGSL_CONTEXT_PER_CONTEXT_TS;
	drawctxt->type = (drawctxt->base.flags & KGSL_CONTEXT_TYPE_MASK)
	>> KGSL_CONTEXT_TYPE_SHIFT;
	mutex_init(&drawctxt->mutex);
	init_waitqueue_head(&drawctxt->wq);
	init_waitqueue_head(&drawctxt->waiting);


	plist_node_init(&drawctxt->pending, ADRENO_CONTEXT_DEFAULT_PRIORITY);

	if (adreno_dev->gpudev->ctxt_create) {
		ret = adreno_dev->gpudev->ctxt_create(adreno_dev, drawctxt);
		if (ret)
			goto err;
	} else if ((drawctxt->base.flags & KGSL_CONTEXT_PREAMBLE) == 0 ||
		  (drawctxt->base.flags & KGSL_CONTEXT_NO_GMEM_ALLOC) == 0) {
		KGSL_DEV_ERR_ONCE(device,
				"legacy context switch not supported\n");
		ret = -EINVAL;
		goto err;

	} else {
		drawctxt->ops = &adreno_preamble_ctx_ops;
	}

	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(drawctxt->base.id, soptimestamp),
			0);
	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(drawctxt->base.id, eoptimestamp),
			0);
	
	*flags = drawctxt->base.flags;
	return &drawctxt->base;
err:
	kgsl_context_detach(&drawctxt->base);
	return ERR_PTR(ret);
}

void adreno_drawctxt_sched(struct kgsl_device *device,
		struct kgsl_context *context)
{
	adreno_dispatcher_queue_context(device, ADRENO_CONTEXT(context));
}

int adreno_drawctxt_detach(struct kgsl_context *context)
{
	struct kgsl_device *device;
	struct adreno_device *adreno_dev;
	struct adreno_context *drawctxt;
	int ret;

	if (context == NULL)
		return 0;

	device = context->device;
	adreno_dev = ADRENO_DEVICE(device);
	drawctxt = ADRENO_CONTEXT(context);

	
	if (adreno_dev->drawctxt_active == drawctxt)
		adreno_drawctxt_switch(adreno_dev, NULL, 0);

	mutex_lock(&drawctxt->mutex);

	while (drawctxt->cmdqueue_head != drawctxt->cmdqueue_tail) {
		struct kgsl_cmdbatch *cmdbatch =
			drawctxt->cmdqueue[drawctxt->cmdqueue_head];

		drawctxt->cmdqueue_head = (drawctxt->cmdqueue_head + 1) %
			ADRENO_CONTEXT_CMDQUEUE_SIZE;

		mutex_unlock(&drawctxt->mutex);


		kgsl_cmdbatch_destroy(cmdbatch);
		mutex_lock(&drawctxt->mutex);
	}

	mutex_unlock(&drawctxt->mutex);
	BUG_ON(!mutex_is_locked(&device->mutex));

	
	ret = adreno_drawctxt_wait_global(adreno_dev, context,
		drawctxt->internal_timestamp, 10 * 1000);


	BUG_ON(ret);

	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, soptimestamp),
			drawctxt->timestamp);

	kgsl_sharedmem_writel(device, &device->memstore,
			KGSL_MEMSTORE_OFFSET(context->id, eoptimestamp),
			drawctxt->timestamp);

	adreno_profile_process_results(device);

	if (drawctxt->ops && drawctxt->ops->detach)
		drawctxt->ops->detach(drawctxt);

	
	wake_up_all(&drawctxt->waiting);
	wake_up_all(&drawctxt->wq);

	return ret;
}


void adreno_drawctxt_destroy(struct kgsl_context *context)
{
	struct adreno_context *drawctxt;
	if (context == NULL)
		return;

	drawctxt = ADRENO_CONTEXT(context);
	kfree(drawctxt);
}


int adreno_context_restore(struct adreno_device *adreno_dev,
				  struct adreno_context *context)
{
	struct kgsl_device *device;
	unsigned int cmds[5];

	if (adreno_dev == NULL || context == NULL)
		return -EINVAL;

	device = &adreno_dev->dev;

	
	cmds[0] = cp_nop_packet(1);
	cmds[1] = KGSL_CONTEXT_TO_MEM_IDENTIFIER;
	cmds[2] = cp_type3_packet(CP_MEM_WRITE, 2);
	cmds[3] = device->memstore.gpuaddr +
		KGSL_MEMSTORE_OFFSET(KGSL_MEMSTORE_GLOBAL, current_context);
	cmds[4] = context->base.id;
	return adreno_ringbuffer_issuecmds(device, context,
				KGSL_CMD_FLAGS_NONE, cmds, 5);
}


const struct adreno_context_ops adreno_preamble_ctx_ops = {
	.restore = adreno_context_restore,
};

static inline int context_save(struct adreno_device *adreno_dev,
				struct adreno_context *context)
{
	if (context->ops->save == NULL
		|| kgsl_context_detached(&context->base)
		|| context->state == ADRENO_CONTEXT_STATE_INVALID)
		return 0;

	return context->ops->save(adreno_dev, context);
}


void adreno_drawctxt_set_bin_base_offset(struct kgsl_device *device,
				      struct kgsl_context *context,
				      unsigned int offset)
{
	struct adreno_context *drawctxt;

	if (context == NULL)
		return;
	drawctxt = ADRENO_CONTEXT(context);
	drawctxt->bin_base_offset = offset;
}


int adreno_drawctxt_switch(struct adreno_device *adreno_dev,
				struct adreno_context *drawctxt,
				unsigned int flags)
{
	struct kgsl_device *device = &adreno_dev->dev;
	int ret = 0;

	if (drawctxt) {
		if (flags & KGSL_CONTEXT_SAVE_GMEM)
			set_bit(ADRENO_CONTEXT_GMEM_SAVE, &drawctxt->priv);
		else
			
			clear_bit(ADRENO_CONTEXT_GMEM_SAVE, &drawctxt->priv);
	}

	
	if (adreno_dev->drawctxt_active == drawctxt) {
		if (drawctxt && drawctxt->ops->draw_workaround)
			ret = drawctxt->ops->draw_workaround(adreno_dev,
							 drawctxt);
		return ret;
	}

	trace_adreno_drawctxt_switch(adreno_dev->drawctxt_active,
		drawctxt, flags);

	if (adreno_dev->drawctxt_active) {
		ret = context_save(adreno_dev, adreno_dev->drawctxt_active);
		if (ret) {
			KGSL_DRV_ERR(device,
				"Error in GPU context %d save: %d\n",
				adreno_dev->drawctxt_active->base.id, ret);
			return ret;
		}

	}

	
	if (drawctxt) {
		if (!_kgsl_context_get(&drawctxt->base))
			return -EINVAL;

		ret = kgsl_mmu_setstate(&device->mmu,
			drawctxt->base.proc_priv->pagetable,
			adreno_dev->drawctxt_active ?
			adreno_dev->drawctxt_active->base.id :
			KGSL_CONTEXT_INVALID);
		
		ret = drawctxt->ops->restore(adreno_dev, drawctxt);
		if (ret) {
			KGSL_DRV_ERR(device,
					"Error in GPU context %d restore: %d\n",
					drawctxt->base.id, ret);
			return ret;
		}
	} else {
		ret = kgsl_mmu_setstate(&device->mmu,
					 device->mmu.defaultpagetable,
					adreno_dev->drawctxt_active->base.id);
	}
	
	if (adreno_dev->drawctxt_active)
		kgsl_context_put(&adreno_dev->drawctxt_active->base);
	adreno_dev->drawctxt_active = drawctxt;
	return 0;
}
