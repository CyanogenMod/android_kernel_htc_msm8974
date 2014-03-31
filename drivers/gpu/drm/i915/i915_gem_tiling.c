/*
 * Copyright © 2008 Intel Corporation
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
 *
 */

#include "linux/string.h"
#include "linux/bitops.h"
#include "drmP.h"
#include "drm.h"
#include "i915_drm.h"
#include "i915_drv.h"


void
i915_gem_detect_bit_6_swizzle(struct drm_device *dev)
{
	drm_i915_private_t *dev_priv = dev->dev_private;
	uint32_t swizzle_x = I915_BIT_6_SWIZZLE_UNKNOWN;
	uint32_t swizzle_y = I915_BIT_6_SWIZZLE_UNKNOWN;

	if (INTEL_INFO(dev)->gen >= 6) {
		uint32_t dimm_c0, dimm_c1;
		dimm_c0 = I915_READ(MAD_DIMM_C0);
		dimm_c1 = I915_READ(MAD_DIMM_C1);
		dimm_c0 &= MAD_DIMM_A_SIZE_MASK | MAD_DIMM_B_SIZE_MASK;
		dimm_c1 &= MAD_DIMM_A_SIZE_MASK | MAD_DIMM_B_SIZE_MASK;
		if (dimm_c0 == dimm_c1) {
			swizzle_x = I915_BIT_6_SWIZZLE_9_10;
			swizzle_y = I915_BIT_6_SWIZZLE_9;
		} else {
			swizzle_x = I915_BIT_6_SWIZZLE_NONE;
			swizzle_y = I915_BIT_6_SWIZZLE_NONE;
		}
	} else if (IS_GEN5(dev)) {
		swizzle_x = I915_BIT_6_SWIZZLE_9_10;
		swizzle_y = I915_BIT_6_SWIZZLE_9;
	} else if (IS_GEN2(dev)) {
		swizzle_x = I915_BIT_6_SWIZZLE_NONE;
		swizzle_y = I915_BIT_6_SWIZZLE_NONE;
	} else if (IS_MOBILE(dev) || (IS_GEN3(dev) && !IS_G33(dev))) {
		uint32_t dcc;

		dcc = I915_READ(DCC);
		switch (dcc & DCC_ADDRESSING_MODE_MASK) {
		case DCC_ADDRESSING_MODE_SINGLE_CHANNEL:
		case DCC_ADDRESSING_MODE_DUAL_CHANNEL_ASYMMETRIC:
			swizzle_x = I915_BIT_6_SWIZZLE_NONE;
			swizzle_y = I915_BIT_6_SWIZZLE_NONE;
			break;
		case DCC_ADDRESSING_MODE_DUAL_CHANNEL_INTERLEAVED:
			if (dcc & DCC_CHANNEL_XOR_DISABLE) {
				swizzle_x = I915_BIT_6_SWIZZLE_9_10;
				swizzle_y = I915_BIT_6_SWIZZLE_9;
			} else if ((dcc & DCC_CHANNEL_XOR_BIT_17) == 0) {
				
				swizzle_x = I915_BIT_6_SWIZZLE_9_10_11;
				swizzle_y = I915_BIT_6_SWIZZLE_9_11;
			} else {
				
				swizzle_x = I915_BIT_6_SWIZZLE_9_10_17;
				swizzle_y = I915_BIT_6_SWIZZLE_9_17;
			}
			break;
		}
		if (dcc == 0xffffffff) {
			DRM_ERROR("Couldn't read from MCHBAR.  "
				  "Disabling tiling.\n");
			swizzle_x = I915_BIT_6_SWIZZLE_UNKNOWN;
			swizzle_y = I915_BIT_6_SWIZZLE_UNKNOWN;
		}
	} else {
		if (I915_READ16(C0DRB3) != I915_READ16(C1DRB3)) {
			swizzle_x = I915_BIT_6_SWIZZLE_NONE;
			swizzle_y = I915_BIT_6_SWIZZLE_NONE;
		} else {
			swizzle_x = I915_BIT_6_SWIZZLE_9_10;
			swizzle_y = I915_BIT_6_SWIZZLE_9;
		}
	}

	dev_priv->mm.bit_6_swizzle_x = swizzle_x;
	dev_priv->mm.bit_6_swizzle_y = swizzle_y;
}

static bool
i915_tiling_ok(struct drm_device *dev, int stride, int size, int tiling_mode)
{
	int tile_width;

	
	if (tiling_mode == I915_TILING_NONE)
		return true;

	if (IS_GEN2(dev) ||
	    (tiling_mode == I915_TILING_Y && HAS_128_BYTE_Y_TILING(dev)))
		tile_width = 128;
	else
		tile_width = 512;

	
	if (INTEL_INFO(dev)->gen >= 4) {
		if (stride / 128 > I965_FENCE_MAX_PITCH_VAL)
			return false;
	} else {
		if (stride > 8192)
			return false;

		if (IS_GEN3(dev)) {
			if (size > I830_FENCE_MAX_SIZE_VAL << 20)
				return false;
		} else {
			if (size > I830_FENCE_MAX_SIZE_VAL << 19)
				return false;
		}
	}

	
	if (INTEL_INFO(dev)->gen >= 4) {
		if (stride & (tile_width - 1))
			return false;
		return true;
	}

	
	if (stride < tile_width)
		return false;

	if (stride & (stride - 1))
		return false;

	return true;
}

static bool
i915_gem_object_fence_ok(struct drm_i915_gem_object *obj, int tiling_mode)
{
	u32 size;

	if (tiling_mode == I915_TILING_NONE)
		return true;

	if (INTEL_INFO(obj->base.dev)->gen >= 4)
		return true;

	if (INTEL_INFO(obj->base.dev)->gen == 3) {
		if (obj->gtt_offset & ~I915_FENCE_START_MASK)
			return false;
	} else {
		if (obj->gtt_offset & ~I830_FENCE_START_MASK)
			return false;
	}

	if (INTEL_INFO(obj->base.dev)->gen == 3)
		size = 1024*1024;
	else
		size = 512*1024;

	while (size < obj->base.size)
		size <<= 1;

	if (obj->gtt_space->size != size)
		return false;

	if (obj->gtt_offset & (size - 1))
		return false;

	return true;
}

int
i915_gem_set_tiling(struct drm_device *dev, void *data,
		   struct drm_file *file)
{
	struct drm_i915_gem_set_tiling *args = data;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj;
	int ret = 0;

	obj = to_intel_bo(drm_gem_object_lookup(dev, file, args->handle));
	if (&obj->base == NULL)
		return -ENOENT;

	if (!i915_tiling_ok(dev,
			    args->stride, obj->base.size, args->tiling_mode)) {
		drm_gem_object_unreference_unlocked(&obj->base);
		return -EINVAL;
	}

	if (obj->pin_count) {
		drm_gem_object_unreference_unlocked(&obj->base);
		return -EBUSY;
	}

	if (args->tiling_mode == I915_TILING_NONE) {
		args->swizzle_mode = I915_BIT_6_SWIZZLE_NONE;
		args->stride = 0;
	} else {
		if (args->tiling_mode == I915_TILING_X)
			args->swizzle_mode = dev_priv->mm.bit_6_swizzle_x;
		else
			args->swizzle_mode = dev_priv->mm.bit_6_swizzle_y;

		if (args->swizzle_mode == I915_BIT_6_SWIZZLE_9_17)
			args->swizzle_mode = I915_BIT_6_SWIZZLE_9;
		if (args->swizzle_mode == I915_BIT_6_SWIZZLE_9_10_17)
			args->swizzle_mode = I915_BIT_6_SWIZZLE_9_10;

		
		if (args->swizzle_mode == I915_BIT_6_SWIZZLE_UNKNOWN) {
			args->tiling_mode = I915_TILING_NONE;
			args->swizzle_mode = I915_BIT_6_SWIZZLE_NONE;
			args->stride = 0;
		}
	}

	mutex_lock(&dev->struct_mutex);
	if (args->tiling_mode != obj->tiling_mode ||
	    args->stride != obj->stride) {
		i915_gem_release_mmap(obj);

		obj->map_and_fenceable =
			obj->gtt_space == NULL ||
			(obj->gtt_offset + obj->base.size <= dev_priv->mm.gtt_mappable_end &&
			 i915_gem_object_fence_ok(obj, args->tiling_mode));

		
		if (!obj->map_and_fenceable) {
			u32 unfenced_alignment =
				i915_gem_get_unfenced_gtt_alignment(dev,
								    obj->base.size,
								    args->tiling_mode);
			if (obj->gtt_offset & (unfenced_alignment - 1))
				ret = i915_gem_object_unbind(obj);
		}

		if (ret == 0) {
			obj->tiling_changed = true;
			obj->tiling_mode = args->tiling_mode;
			obj->stride = args->stride;
		}
	}
	
	args->stride = obj->stride;
	args->tiling_mode = obj->tiling_mode;
	drm_gem_object_unreference(&obj->base);
	mutex_unlock(&dev->struct_mutex);

	return ret;
}

int
i915_gem_get_tiling(struct drm_device *dev, void *data,
		   struct drm_file *file)
{
	struct drm_i915_gem_get_tiling *args = data;
	drm_i915_private_t *dev_priv = dev->dev_private;
	struct drm_i915_gem_object *obj;

	obj = to_intel_bo(drm_gem_object_lookup(dev, file, args->handle));
	if (&obj->base == NULL)
		return -ENOENT;

	mutex_lock(&dev->struct_mutex);

	args->tiling_mode = obj->tiling_mode;
	switch (obj->tiling_mode) {
	case I915_TILING_X:
		args->swizzle_mode = dev_priv->mm.bit_6_swizzle_x;
		break;
	case I915_TILING_Y:
		args->swizzle_mode = dev_priv->mm.bit_6_swizzle_y;
		break;
	case I915_TILING_NONE:
		args->swizzle_mode = I915_BIT_6_SWIZZLE_NONE;
		break;
	default:
		DRM_ERROR("unknown tiling mode\n");
	}

	
	if (args->swizzle_mode == I915_BIT_6_SWIZZLE_9_17)
		args->swizzle_mode = I915_BIT_6_SWIZZLE_9;
	if (args->swizzle_mode == I915_BIT_6_SWIZZLE_9_10_17)
		args->swizzle_mode = I915_BIT_6_SWIZZLE_9_10;

	drm_gem_object_unreference(&obj->base);
	mutex_unlock(&dev->struct_mutex);

	return 0;
}

static void
i915_gem_swizzle_page(struct page *page)
{
	char temp[64];
	char *vaddr;
	int i;

	vaddr = kmap(page);

	for (i = 0; i < PAGE_SIZE; i += 128) {
		memcpy(temp, &vaddr[i], 64);
		memcpy(&vaddr[i], &vaddr[i + 64], 64);
		memcpy(&vaddr[i + 64], temp, 64);
	}

	kunmap(page);
}

void
i915_gem_object_do_bit_17_swizzle(struct drm_i915_gem_object *obj)
{
	int page_count = obj->base.size >> PAGE_SHIFT;
	int i;

	if (obj->bit_17 == NULL)
		return;

	for (i = 0; i < page_count; i++) {
		char new_bit_17 = page_to_phys(obj->pages[i]) >> 17;
		if ((new_bit_17 & 0x1) !=
		    (test_bit(i, obj->bit_17) != 0)) {
			i915_gem_swizzle_page(obj->pages[i]);
			set_page_dirty(obj->pages[i]);
		}
	}
}

void
i915_gem_object_save_bit_17_swizzle(struct drm_i915_gem_object *obj)
{
	int page_count = obj->base.size >> PAGE_SHIFT;
	int i;

	if (obj->bit_17 == NULL) {
		obj->bit_17 = kmalloc(BITS_TO_LONGS(page_count) *
					   sizeof(long), GFP_KERNEL);
		if (obj->bit_17 == NULL) {
			DRM_ERROR("Failed to allocate memory for bit 17 "
				  "record\n");
			return;
		}
	}

	for (i = 0; i < page_count; i++) {
		if (page_to_phys(obj->pages[i]) & (1 << 17))
			__set_bit(i, obj->bit_17);
		else
			__clear_bit(i, obj->bit_17);
	}
}
