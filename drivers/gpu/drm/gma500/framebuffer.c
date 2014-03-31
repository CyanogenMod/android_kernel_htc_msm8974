/**************************************************************************
 * Copyright (c) 2007-2011, Intel Corporation.
 * All Rights Reserved.
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
 **************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/console.h>

#include <drm/drmP.h>
#include <drm/drm.h>
#include <drm/drm_crtc.h>
#include <drm/drm_fb_helper.h>

#include "psb_drv.h"
#include "psb_intel_reg.h"
#include "psb_intel_drv.h"
#include "framebuffer.h"
#include "gtt.h"

static void psb_user_framebuffer_destroy(struct drm_framebuffer *fb);
static int psb_user_framebuffer_create_handle(struct drm_framebuffer *fb,
					      struct drm_file *file_priv,
					      unsigned int *handle);

static const struct drm_framebuffer_funcs psb_fb_funcs = {
	.destroy = psb_user_framebuffer_destroy,
	.create_handle = psb_user_framebuffer_create_handle,
};

#define CMAP_TOHW(_val, _width) ((((_val) << (_width)) + 0x7FFF - (_val)) >> 16)

static int psbfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp,
			   struct fb_info *info)
{
	struct psb_fbdev *fbdev = info->par;
	struct drm_framebuffer *fb = fbdev->psb_fb_helper.fb;
	uint32_t v;

	if (!fb)
		return -ENOMEM;

	if (regno > 255)
		return 1;

	red = CMAP_TOHW(red, info->var.red.length);
	blue = CMAP_TOHW(blue, info->var.blue.length);
	green = CMAP_TOHW(green, info->var.green.length);
	transp = CMAP_TOHW(transp, info->var.transp.length);

	v = (red << info->var.red.offset) |
	    (green << info->var.green.offset) |
	    (blue << info->var.blue.offset) |
	    (transp << info->var.transp.offset);

	if (regno < 16) {
		switch (fb->bits_per_pixel) {
		case 16:
			((uint32_t *) info->pseudo_palette)[regno] = v;
			break;
		case 24:
		case 32:
			((uint32_t *) info->pseudo_palette)[regno] = v;
			break;
		}
	}

	return 0;
}

static int psbfb_pan(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct psb_fbdev *fbdev = info->par;
	struct psb_framebuffer *psbfb = &fbdev->pfb;
	struct drm_device *dev = psbfb->base.dev;

	if (psbfb->gtt->npage) {
		int pages = info->fix.line_length >> 12;
		psb_gtt_roll(dev, psbfb->gtt, var->yoffset * pages);
	}
        return 0;
}

static int psbfb_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct psb_framebuffer *psbfb = vma->vm_private_data;
	struct drm_device *dev = psbfb->base.dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	int page_num;
	int i;
	unsigned long address;
	int ret;
	unsigned long pfn;
	
	unsigned long phys_addr = (unsigned long)dev_priv->stolen_base;

	page_num = (vma->vm_end - vma->vm_start) >> PAGE_SHIFT;
	address = (unsigned long)vmf->virtual_address - (vmf->pgoff << PAGE_SHIFT);

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	for (i = 0; i < page_num; i++) {
		pfn = (phys_addr >> PAGE_SHIFT);

		ret = vm_insert_mixed(vma, address, pfn);
		if (unlikely((ret == -EBUSY) || (ret != 0 && i > 0)))
			break;
		else if (unlikely(ret != 0)) {
			ret = (ret == -ENOMEM) ? VM_FAULT_OOM : VM_FAULT_SIGBUS;
			return ret;
		}
		address += PAGE_SIZE;
		phys_addr += PAGE_SIZE;
	}
	return VM_FAULT_NOPAGE;
}

static void psbfb_vm_open(struct vm_area_struct *vma)
{
}

static void psbfb_vm_close(struct vm_area_struct *vma)
{
}

static struct vm_operations_struct psbfb_vm_ops = {
	.fault	= psbfb_vm_fault,
	.open	= psbfb_vm_open,
	.close	= psbfb_vm_close
};

static int psbfb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	struct psb_fbdev *fbdev = info->par;
	struct psb_framebuffer *psbfb = &fbdev->pfb;

	if (vma->vm_pgoff != 0)
		return -EINVAL;
	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
		return -EINVAL;

	if (!psbfb->addr_space)
		psbfb->addr_space = vma->vm_file->f_mapping;
	vma->vm_ops = &psbfb_vm_ops;
	vma->vm_private_data = (void *)psbfb;
	vma->vm_flags |= VM_RESERVED | VM_IO |
					VM_MIXEDMAP | VM_DONTEXPAND;
	return 0;
}

static int psbfb_ioctl(struct fb_info *info, unsigned int cmd,
						unsigned long arg)
{
	return -ENOTTY;
}

static struct fb_ops psbfb_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = drm_fb_helper_check_var,
	.fb_set_par = drm_fb_helper_set_par,
	.fb_blank = drm_fb_helper_blank,
	.fb_setcolreg = psbfb_setcolreg,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = psbfb_copyarea,
	.fb_imageblit = cfb_imageblit,
	.fb_mmap = psbfb_mmap,
	.fb_sync = psbfb_sync,
	.fb_ioctl = psbfb_ioctl,
};

static struct fb_ops psbfb_roll_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = drm_fb_helper_check_var,
	.fb_set_par = drm_fb_helper_set_par,
	.fb_blank = drm_fb_helper_blank,
	.fb_setcolreg = psbfb_setcolreg,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
	.fb_pan_display = psbfb_pan,
	.fb_mmap = psbfb_mmap,
	.fb_ioctl = psbfb_ioctl,
};

static struct fb_ops psbfb_unaccel_ops = {
	.owner = THIS_MODULE,
	.fb_check_var = drm_fb_helper_check_var,
	.fb_set_par = drm_fb_helper_set_par,
	.fb_blank = drm_fb_helper_blank,
	.fb_setcolreg = psbfb_setcolreg,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
	.fb_mmap = psbfb_mmap,
	.fb_ioctl = psbfb_ioctl,
};

static int psb_framebuffer_init(struct drm_device *dev,
					struct psb_framebuffer *fb,
					struct drm_mode_fb_cmd2 *mode_cmd,
					struct gtt_range *gt)
{
	u32 bpp, depth;
	int ret;

	drm_fb_get_bpp_depth(mode_cmd->pixel_format, &depth, &bpp);

	if (mode_cmd->pitches[0] & 63)
		return -EINVAL;
	switch (bpp) {
	case 8:
	case 16:
	case 24:
	case 32:
		break;
	default:
		return -EINVAL;
	}
	ret = drm_framebuffer_init(dev, &fb->base, &psb_fb_funcs);
	if (ret) {
		dev_err(dev->dev, "framebuffer init failed: %d\n", ret);
		return ret;
	}
	drm_helper_mode_fill_fb_struct(&fb->base, mode_cmd);
	fb->gtt = gt;
	return 0;
}


static struct drm_framebuffer *psb_framebuffer_create
			(struct drm_device *dev,
			 struct drm_mode_fb_cmd2 *mode_cmd,
			 struct gtt_range *gt)
{
	struct psb_framebuffer *fb;
	int ret;

	fb = kzalloc(sizeof(*fb), GFP_KERNEL);
	if (!fb)
		return ERR_PTR(-ENOMEM);

	ret = psb_framebuffer_init(dev, fb, mode_cmd, gt);
	if (ret) {
		kfree(fb);
		return ERR_PTR(ret);
	}
	return &fb->base;
}

static struct gtt_range *psbfb_alloc(struct drm_device *dev, int aligned_size)
{
	struct gtt_range *backing;
	
	backing = psb_gtt_alloc_range(dev, aligned_size, "fb", 1);
	if (backing) {
		if (drm_gem_private_object_init(dev,
					&backing->gem, aligned_size) == 0)
			return backing;
		psb_gtt_free_range(dev, backing);
	}
	return NULL;
}

static int psbfb_create(struct psb_fbdev *fbdev,
				struct drm_fb_helper_surface_size *sizes)
{
	struct drm_device *dev = fbdev->psb_fb_helper.dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct fb_info *info;
	struct drm_framebuffer *fb;
	struct psb_framebuffer *psbfb = &fbdev->pfb;
	struct drm_mode_fb_cmd2 mode_cmd;
	struct device *device = &dev->pdev->dev;
	int size;
	int ret;
	struct gtt_range *backing;
	u32 bpp, depth;
	int gtt_roll = 0;
	int pitch_lines = 0;

	mode_cmd.width = sizes->surface_width;
	mode_cmd.height = sizes->surface_height;
	bpp = sizes->surface_bpp;
	depth = sizes->surface_depth;

	
	if (bpp == 24)
		bpp = 32;

	do {
        	mode_cmd.pitches[0] =  ALIGN(mode_cmd.width * ((bpp + 7) / 8), 4096 >> pitch_lines);

        	size = mode_cmd.pitches[0] * mode_cmd.height;
        	size = ALIGN(size, PAGE_SIZE);

		
		backing = psbfb_alloc(dev, size);

		if (pitch_lines)
			pitch_lines *= 2;
		else
			pitch_lines = 1;
		gtt_roll++;
	} while (backing == NULL && pitch_lines <= 16);

	
	pitch_lines /= 2;

	if (backing == NULL) {

		gtt_roll = 0;	
		pitch_lines = 64;

		mode_cmd.pitches[0] =  ALIGN(mode_cmd.width * ((bpp + 7) / 8), 64);

		size = mode_cmd.pitches[0] * mode_cmd.height;
		size = ALIGN(size, PAGE_SIZE);

		
		backing = psbfb_alloc(dev, size);
		if (backing == NULL)
			return -ENOMEM;
	}

	mutex_lock(&dev->struct_mutex);

	info = framebuffer_alloc(0, device);
	if (!info) {
		ret = -ENOMEM;
		goto out_err1;
	}
	info->par = fbdev;

	mode_cmd.pixel_format = drm_mode_legacy_fb_format(bpp, depth);

	ret = psb_framebuffer_init(dev, psbfb, &mode_cmd, backing);
	if (ret)
		goto out_unref;

	fb = &psbfb->base;
	psbfb->fbdev = info;

	fbdev->psb_fb_helper.fb = fb;
	fbdev->psb_fb_helper.fbdev = info;

	drm_fb_helper_fill_fix(info, fb->pitches[0], fb->depth);
	strcpy(info->fix.id, "psbfb");

	info->flags = FBINFO_DEFAULT;
	if (dev_priv->ops->accel_2d && pitch_lines > 8)	
		info->fbops = &psbfb_ops;
	else if (gtt_roll) {	
		info->fbops = &psbfb_roll_ops;
		info->flags |= FBINFO_HWACCEL_YPAN;
	} else	
		info->fbops = &psbfb_unaccel_ops;

	ret = fb_alloc_cmap(&info->cmap, 256, 0);
	if (ret) {
		ret = -ENOMEM;
		goto out_unref;
	}

	info->fix.smem_start = dev->mode_config.fb_base;
	info->fix.smem_len = size;
	info->fix.ywrapstep = gtt_roll;
	info->fix.ypanstep = 0;

	
	info->screen_base = (char *)dev_priv->vram_addr +
							backing->offset;
	info->screen_size = size;

	if (dev_priv->gtt.stolen_size) {
		info->apertures = alloc_apertures(1);
		if (!info->apertures) {
			ret = -ENOMEM;
			goto out_unref;
		}
		info->apertures->ranges[0].base = dev->mode_config.fb_base;
		info->apertures->ranges[0].size = dev_priv->gtt.stolen_size;
	}

	drm_fb_helper_fill_var(info, &fbdev->psb_fb_helper,
				sizes->fb_width, sizes->fb_height);

	info->fix.mmio_start = pci_resource_start(dev->pdev, 0);
	info->fix.mmio_len = pci_resource_len(dev->pdev, 0);

	

	dev_info(dev->dev, "allocated %dx%d fb\n",
					psbfb->base.width, psbfb->base.height);

	mutex_unlock(&dev->struct_mutex);
	return 0;
out_unref:
	if (backing->stolen)
		psb_gtt_free_range(dev, backing);
	else
		drm_gem_object_unreference(&backing->gem);
out_err1:
	mutex_unlock(&dev->struct_mutex);
	psb_gtt_free_range(dev, backing);
	return ret;
}

static struct drm_framebuffer *psb_user_framebuffer_create
			(struct drm_device *dev, struct drm_file *filp,
			 struct drm_mode_fb_cmd2 *cmd)
{
	struct gtt_range *r;
	struct drm_gem_object *obj;

	obj = drm_gem_object_lookup(dev, filp, cmd->handles[0]);
	if (obj == NULL)
		return ERR_PTR(-ENOENT);

	
	r = container_of(obj, struct gtt_range, gem);
	return psb_framebuffer_create(dev, cmd, r);
}

static void psbfb_gamma_set(struct drm_crtc *crtc, u16 red, u16 green,
							u16 blue, int regno)
{
	struct psb_intel_crtc *intel_crtc = to_psb_intel_crtc(crtc);

	intel_crtc->lut_r[regno] = red >> 8;
	intel_crtc->lut_g[regno] = green >> 8;
	intel_crtc->lut_b[regno] = blue >> 8;
}

static void psbfb_gamma_get(struct drm_crtc *crtc, u16 *red,
					u16 *green, u16 *blue, int regno)
{
	struct psb_intel_crtc *intel_crtc = to_psb_intel_crtc(crtc);

	*red = intel_crtc->lut_r[regno] << 8;
	*green = intel_crtc->lut_g[regno] << 8;
	*blue = intel_crtc->lut_b[regno] << 8;
}

static int psbfb_probe(struct drm_fb_helper *helper,
				struct drm_fb_helper_surface_size *sizes)
{
	struct psb_fbdev *psb_fbdev = (struct psb_fbdev *)helper;
	int new_fb = 0;
	int ret;

	if (!helper->fb) {
		ret = psbfb_create(psb_fbdev, sizes);
		if (ret)
			return ret;
		new_fb = 1;
	}
	return new_fb;
}

struct drm_fb_helper_funcs psb_fb_helper_funcs = {
	.gamma_set = psbfb_gamma_set,
	.gamma_get = psbfb_gamma_get,
	.fb_probe = psbfb_probe,
};

static int psb_fbdev_destroy(struct drm_device *dev, struct psb_fbdev *fbdev)
{
	struct fb_info *info;
	struct psb_framebuffer *psbfb = &fbdev->pfb;

	if (fbdev->psb_fb_helper.fbdev) {
		info = fbdev->psb_fb_helper.fbdev;
		unregister_framebuffer(info);
		if (info->cmap.len)
			fb_dealloc_cmap(&info->cmap);
		framebuffer_release(info);
	}
	drm_fb_helper_fini(&fbdev->psb_fb_helper);
	drm_framebuffer_cleanup(&psbfb->base);

	if (psbfb->gtt)
		drm_gem_object_unreference(&psbfb->gtt->gem);
	return 0;
}

int psb_fbdev_init(struct drm_device *dev)
{
	struct psb_fbdev *fbdev;
	struct drm_psb_private *dev_priv = dev->dev_private;

	fbdev = kzalloc(sizeof(struct psb_fbdev), GFP_KERNEL);
	if (!fbdev) {
		dev_err(dev->dev, "no memory\n");
		return -ENOMEM;
	}

	dev_priv->fbdev = fbdev;
	fbdev->psb_fb_helper.funcs = &psb_fb_helper_funcs;

	drm_fb_helper_init(dev, &fbdev->psb_fb_helper, dev_priv->ops->crtcs,
							INTELFB_CONN_LIMIT);

	drm_fb_helper_single_add_all_connectors(&fbdev->psb_fb_helper);
	drm_fb_helper_initial_config(&fbdev->psb_fb_helper, 32);
	return 0;
}

static void psb_fbdev_fini(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;

	if (!dev_priv->fbdev)
		return;

	psb_fbdev_destroy(dev, dev_priv->fbdev);
	kfree(dev_priv->fbdev);
	dev_priv->fbdev = NULL;
}

static void psbfb_output_poll_changed(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_fbdev *fbdev = (struct psb_fbdev *)dev_priv->fbdev;
	drm_fb_helper_hotplug_event(&fbdev->psb_fb_helper);
}

static int psb_user_framebuffer_create_handle(struct drm_framebuffer *fb,
					      struct drm_file *file_priv,
					      unsigned int *handle)
{
	struct psb_framebuffer *psbfb = to_psb_fb(fb);
	struct gtt_range *r = psbfb->gtt;
	return drm_gem_handle_create(file_priv, &r->gem, handle);
}

static void psb_user_framebuffer_destroy(struct drm_framebuffer *fb)
{
	struct psb_framebuffer *psbfb = to_psb_fb(fb);
	struct gtt_range *r = psbfb->gtt;
	struct drm_device *dev = fb->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_fbdev *fbdev = dev_priv->fbdev;
	struct drm_crtc *crtc;
	int reset = 0;

	
	WARN_ON(r->stolen);

	
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head)
		if (crtc->fb == fb)
			reset = 1;

	if (reset)
		drm_fb_helper_restore_fbdev_mode(&fbdev->psb_fb_helper);

	
	drm_framebuffer_cleanup(fb);
	
	drm_gem_object_unreference_unlocked(&r->gem);
	kfree(fb);
}

static const struct drm_mode_config_funcs psb_mode_funcs = {
	.fb_create = psb_user_framebuffer_create,
	.output_poll_changed = psbfb_output_poll_changed,
};

static int psb_create_backlight_property(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct drm_property *backlight;

	if (dev_priv->backlight_property)
		return 0;

	backlight = drm_property_create_range(dev, 0, "backlight", 0, 100);

	dev_priv->backlight_property = backlight;

	return 0;
}

static void psb_setup_outputs(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct drm_connector *connector;

	drm_mode_create_scaling_mode_property(dev);
	psb_create_backlight_property(dev);

	dev_priv->ops->output_init(dev);

	list_for_each_entry(connector, &dev->mode_config.connector_list,
			    head) {
		struct psb_intel_encoder *psb_intel_encoder =
			psb_intel_attached_encoder(connector);
		struct drm_encoder *encoder = &psb_intel_encoder->base;
		int crtc_mask = 0, clone_mask = 0;

		
		switch (psb_intel_encoder->type) {
		case INTEL_OUTPUT_ANALOG:
			crtc_mask = (1 << 0);
			clone_mask = (1 << INTEL_OUTPUT_ANALOG);
			break;
		case INTEL_OUTPUT_SDVO:
			crtc_mask = ((1 << 0) | (1 << 1));
			clone_mask = (1 << INTEL_OUTPUT_SDVO);
			break;
		case INTEL_OUTPUT_LVDS:
			if (IS_MRST(dev))
				crtc_mask = (1 << 0);
			else
				crtc_mask = (1 << 1);
			clone_mask = (1 << INTEL_OUTPUT_LVDS);
			break;
		case INTEL_OUTPUT_MIPI:
			crtc_mask = (1 << 0);
			clone_mask = (1 << INTEL_OUTPUT_MIPI);
			break;
		case INTEL_OUTPUT_MIPI2:
			crtc_mask = (1 << 2);
			clone_mask = (1 << INTEL_OUTPUT_MIPI2);
			break;
		case INTEL_OUTPUT_HDMI:
			if (IS_MFLD(dev))
				crtc_mask = (1 << 1);
			else	
				crtc_mask = (1 << 0);
			clone_mask = (1 << INTEL_OUTPUT_HDMI);
			break;
		}
		encoder->possible_crtcs = crtc_mask;
		encoder->possible_clones =
		    psb_intel_connector_clones(dev, clone_mask);
	}
}

void psb_modeset_init(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_intel_mode_device *mode_dev = &dev_priv->mode_dev;
	int i;

	drm_mode_config_init(dev);

	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;

	dev->mode_config.funcs = (void *) &psb_mode_funcs;

	
	
	pci_read_config_dword(dev->pdev, PSB_BSM, (u32 *)
					&(dev->mode_config.fb_base));

	
	for (i = 0; i < dev_priv->num_pipe; i++)
		psb_intel_crtc_init(dev, i, mode_dev);

	dev->mode_config.max_width = 2048;
	dev->mode_config.max_height = 2048;

	psb_setup_outputs(dev);
}

void psb_modeset_cleanup(struct drm_device *dev)
{
	mutex_lock(&dev->struct_mutex);

	drm_kms_helper_poll_fini(dev);
	psb_fbdev_fini(dev);
	drm_mode_config_cleanup(dev);

	mutex_unlock(&dev->struct_mutex);
}
