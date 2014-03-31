/*
 *  psb GEM interface
 *
 * Copyright (c) 2011, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Authors: Alan Cox
 *
 * TODO:
 *	-	we need to work out if the MMU is relevant (eg for
 *		accelerated operations on a GEM object)
 */

#include <drm/drmP.h>
#include <drm/drm.h>
#include "gma_drm.h"
#include "psb_drv.h"

int psb_gem_init_object(struct drm_gem_object *obj)
{
	return -EINVAL;
}

void psb_gem_free_object(struct drm_gem_object *obj)
{
	struct gtt_range *gtt = container_of(obj, struct gtt_range, gem);
	drm_gem_object_release_wrap(obj);
	
	psb_gtt_free_range(obj->dev, gtt);
}

int psb_gem_get_aperture(struct drm_device *dev, void *data,
				struct drm_file *file)
{
	return -EINVAL;
}

int psb_gem_dumb_map_gtt(struct drm_file *file, struct drm_device *dev,
			 uint32_t handle, uint64_t *offset)
{
	int ret = 0;
	struct drm_gem_object *obj;

	if (!(dev->driver->driver_features & DRIVER_GEM))
		return -ENODEV;

	mutex_lock(&dev->struct_mutex);

	
	obj = drm_gem_object_lookup(dev, file, handle);
	if (obj == NULL) {
		ret = -ENOENT;
		goto unlock;
	}
	

	
	if (!obj->map_list.map) {
		ret = gem_create_mmap_offset(obj);
		if (ret)
			goto out;
	}
	
	*offset = (u64)obj->map_list.hash.key << PAGE_SHIFT;
out:
	drm_gem_object_unreference(obj);
unlock:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

static int psb_gem_create(struct drm_file *file,
	struct drm_device *dev, uint64_t size, uint32_t *handlep)
{
	struct gtt_range *r;
	int ret;
	u32 handle;

	size = roundup(size, PAGE_SIZE);

	r = psb_gtt_alloc_range(dev, size, "gem", 0);
	if (r == NULL) {
		dev_err(dev->dev, "no memory for %lld byte GEM object\n", size);
		return -ENOSPC;
	}
	
	if (drm_gem_object_init(dev, &r->gem, size) != 0) {
		psb_gtt_free_range(dev, r);
		
		dev_err(dev->dev, "GEM init failed for %lld\n", size);
		return -ENOMEM;
	}
	
	ret = drm_gem_handle_create(file, &r->gem, &handle);
	if (ret) {
		dev_err(dev->dev, "GEM handle failed for %p, %lld\n",
							&r->gem, size);
		drm_gem_object_release(&r->gem);
		psb_gtt_free_range(dev, r);
		return ret;
	}
	
	drm_gem_object_unreference(&r->gem);
	*handlep = handle;
	return 0;
}

int psb_gem_dumb_create(struct drm_file *file, struct drm_device *dev,
			struct drm_mode_create_dumb *args)
{
	args->pitch = ALIGN(args->width * ((args->bpp + 7) / 8), 64);
	args->size = args->pitch * args->height;
	return psb_gem_create(file, dev, args->size, &args->handle);
}

int psb_gem_dumb_destroy(struct drm_file *file, struct drm_device *dev,
			uint32_t handle)
{
	
	return drm_gem_handle_delete(file, handle);
}

int psb_gem_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct drm_gem_object *obj;
	struct gtt_range *r;
	int ret;
	unsigned long pfn;
	pgoff_t page_offset;
	struct drm_device *dev;
	struct drm_psb_private *dev_priv;

	obj = vma->vm_private_data;	
	dev = obj->dev;
	dev_priv = dev->dev_private;

	r = container_of(obj, struct gtt_range, gem);	

	mutex_lock(&dev->struct_mutex);

	if (r->mmapping == 0) {
		ret = psb_gtt_pin(r);
		if (ret < 0) {
			dev_err(dev->dev, "gma500: pin failed: %d\n", ret);
			goto fail;
		}
		r->mmapping = 1;
	}

	page_offset = ((unsigned long) vmf->virtual_address - vma->vm_start)
				>> PAGE_SHIFT;

	
	if (r->stolen)
		pfn = (dev_priv->stolen_base + r->offset) >> PAGE_SHIFT;
	else
		pfn = page_to_pfn(r->pages[page_offset]);
	ret = vm_insert_pfn(vma, (unsigned long)vmf->virtual_address, pfn);

fail:
	mutex_unlock(&dev->struct_mutex);
	switch (ret) {
	case 0:
	case -ERESTARTSYS:
	case -EINTR:
		return VM_FAULT_NOPAGE;
	case -ENOMEM:
		return VM_FAULT_OOM;
	default:
		return VM_FAULT_SIGBUS;
	}
}

static int psb_gem_create_stolen(struct drm_file *file, struct drm_device *dev,
						int size, u32 *handle)
{
	struct gtt_range *gtt = psb_gtt_alloc_range(dev, size, "gem", 1);
	if (gtt == NULL)
		return -ENOMEM;
	if (drm_gem_private_object_init(dev, &gtt->gem, size) != 0)
		goto free_gtt;
	if (drm_gem_handle_create(file, &gtt->gem, handle) == 0)
		return 0;
free_gtt:
	psb_gtt_free_range(dev, gtt);
	return -ENOMEM;
}

int psb_gem_create_ioctl(struct drm_device *dev, void *data,
					struct drm_file *file)
{
	struct drm_psb_gem_create *args = data;
	int ret;
	if (args->flags & GMA_GEM_CREATE_STOLEN) {
		ret = psb_gem_create_stolen(file, dev, args->size,
							&args->handle);
		if (ret == 0)
			return 0;
		
		args->flags &= ~GMA_GEM_CREATE_STOLEN;
	}
	return psb_gem_create(file, dev, args->size, &args->handle);
}

int psb_gem_mmap_ioctl(struct drm_device *dev, void *data,
					struct drm_file *file)
{
	struct drm_psb_gem_mmap *args = data;
	return dev->driver->dumb_map_offset(file, dev,
						args->handle, &args->offset);
}

