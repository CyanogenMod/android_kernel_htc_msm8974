
/*
 * Created: Fri Nov 24 18:31:37 2000 by gareth@valinux.com
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


void drm_ctxbitmap_free(struct drm_device * dev, int ctx_handle)
{
	mutex_lock(&dev->struct_mutex);
	idr_remove(&dev->ctx_idr, ctx_handle);
	mutex_unlock(&dev->struct_mutex);
}

static int drm_ctxbitmap_next(struct drm_device * dev)
{
	int new_id;
	int ret;

again:
	if (idr_pre_get(&dev->ctx_idr, GFP_KERNEL) == 0) {
		DRM_ERROR("Out of memory expanding drawable idr\n");
		return -ENOMEM;
	}
	mutex_lock(&dev->struct_mutex);
	ret = idr_get_new_above(&dev->ctx_idr, NULL,
				DRM_RESERVED_CONTEXTS, &new_id);
	if (ret == -EAGAIN) {
		mutex_unlock(&dev->struct_mutex);
		goto again;
	}
	mutex_unlock(&dev->struct_mutex);
	return new_id;
}

int drm_ctxbitmap_init(struct drm_device * dev)
{
	idr_init(&dev->ctx_idr);
	return 0;
}

void drm_ctxbitmap_cleanup(struct drm_device * dev)
{
	mutex_lock(&dev->struct_mutex);
	idr_remove_all(&dev->ctx_idr);
	mutex_unlock(&dev->struct_mutex);
}



int drm_getsareactx(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_ctx_priv_map *request = data;
	struct drm_local_map *map;
	struct drm_map_list *_entry;

	mutex_lock(&dev->struct_mutex);

	map = idr_find(&dev->ctx_idr, request->ctx_id);
	if (!map) {
		mutex_unlock(&dev->struct_mutex);
		return -EINVAL;
	}

	request->handle = NULL;
	list_for_each_entry(_entry, &dev->maplist, head) {
		if (_entry->map == map) {
			request->handle =
			    (void *)(unsigned long)_entry->user_token;
			break;
		}
	}

	mutex_unlock(&dev->struct_mutex);

	if (request->handle == NULL)
		return -EINVAL;

	return 0;
}

int drm_setsareactx(struct drm_device *dev, void *data,
		    struct drm_file *file_priv)
{
	struct drm_ctx_priv_map *request = data;
	struct drm_local_map *map = NULL;
	struct drm_map_list *r_list = NULL;

	mutex_lock(&dev->struct_mutex);
	list_for_each_entry(r_list, &dev->maplist, head) {
		if (r_list->map
		    && r_list->user_token == (unsigned long) request->handle)
			goto found;
	}
      bad:
	mutex_unlock(&dev->struct_mutex);
	return -EINVAL;

      found:
	map = r_list->map;
	if (!map)
		goto bad;

	if (IS_ERR(idr_replace(&dev->ctx_idr, map, request->ctx_id)))
		goto bad;

	mutex_unlock(&dev->struct_mutex);

	return 0;
}



static int drm_context_switch(struct drm_device * dev, int old, int new)
{
	if (test_and_set_bit(0, &dev->context_flag)) {
		DRM_ERROR("Reentering -- FIXME\n");
		return -EBUSY;
	}

	DRM_DEBUG("Context switch from %d to %d\n", old, new);

	if (new == dev->last_context) {
		clear_bit(0, &dev->context_flag);
		return 0;
	}

	return 0;
}

static int drm_context_switch_complete(struct drm_device *dev,
				       struct drm_file *file_priv, int new)
{
	dev->last_context = new;	
	dev->last_switch = jiffies;

	if (!_DRM_LOCK_IS_HELD(file_priv->master->lock.hw_lock->lock)) {
		DRM_ERROR("Lock isn't held after context switch\n");
	}

	clear_bit(0, &dev->context_flag);
	wake_up(&dev->context_wait);

	return 0;
}

int drm_resctx(struct drm_device *dev, void *data,
	       struct drm_file *file_priv)
{
	struct drm_ctx_res *res = data;
	struct drm_ctx ctx;
	int i;

	if (res->count >= DRM_RESERVED_CONTEXTS) {
		memset(&ctx, 0, sizeof(ctx));
		for (i = 0; i < DRM_RESERVED_CONTEXTS; i++) {
			ctx.handle = i;
			if (copy_to_user(&res->contexts[i], &ctx, sizeof(ctx)))
				return -EFAULT;
		}
	}
	res->count = DRM_RESERVED_CONTEXTS;

	return 0;
}

int drm_addctx(struct drm_device *dev, void *data,
	       struct drm_file *file_priv)
{
	struct drm_ctx_list *ctx_entry;
	struct drm_ctx *ctx = data;

	ctx->handle = drm_ctxbitmap_next(dev);
	if (ctx->handle == DRM_KERNEL_CONTEXT) {
		
		ctx->handle = drm_ctxbitmap_next(dev);
	}
	DRM_DEBUG("%d\n", ctx->handle);
	if (ctx->handle == -1) {
		DRM_DEBUG("Not enough free contexts.\n");
		
		return -ENOMEM;
	}

	ctx_entry = kmalloc(sizeof(*ctx_entry), GFP_KERNEL);
	if (!ctx_entry) {
		DRM_DEBUG("out of memory\n");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&ctx_entry->head);
	ctx_entry->handle = ctx->handle;
	ctx_entry->tag = file_priv;

	mutex_lock(&dev->ctxlist_mutex);
	list_add(&ctx_entry->head, &dev->ctxlist);
	++dev->ctx_count;
	mutex_unlock(&dev->ctxlist_mutex);

	return 0;
}

int drm_modctx(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	
	return 0;
}

int drm_getctx(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	struct drm_ctx *ctx = data;

	
	ctx->flags = 0;

	return 0;
}

int drm_switchctx(struct drm_device *dev, void *data,
		  struct drm_file *file_priv)
{
	struct drm_ctx *ctx = data;

	DRM_DEBUG("%d\n", ctx->handle);
	return drm_context_switch(dev, dev->last_context, ctx->handle);
}

int drm_newctx(struct drm_device *dev, void *data,
	       struct drm_file *file_priv)
{
	struct drm_ctx *ctx = data;

	DRM_DEBUG("%d\n", ctx->handle);
	drm_context_switch_complete(dev, file_priv, ctx->handle);

	return 0;
}

int drm_rmctx(struct drm_device *dev, void *data,
	      struct drm_file *file_priv)
{
	struct drm_ctx *ctx = data;

	DRM_DEBUG("%d\n", ctx->handle);
	if (ctx->handle != DRM_KERNEL_CONTEXT) {
		if (dev->driver->context_dtor)
			dev->driver->context_dtor(dev, ctx->handle);
		drm_ctxbitmap_free(dev, ctx->handle);
	}

	mutex_lock(&dev->ctxlist_mutex);
	if (!list_empty(&dev->ctxlist)) {
		struct drm_ctx_list *pos, *n;

		list_for_each_entry_safe(pos, n, &dev->ctxlist, head) {
			if (pos->handle == ctx->handle) {
				list_del(&pos->head);
				kfree(pos);
				--dev->ctx_count;
			}
		}
	}
	mutex_unlock(&dev->ctxlist_mutex);

	return 0;
}

