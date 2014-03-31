/*
 * Copyright (c) 2006-2008 Intel Corporation
 * Copyright (c) 2007 Dave Airlie <airlied@linux.ie>
 *
 * DRM core CRTC related functions
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 *
 * Authors:
 *      Keith Packard
 *	Eric Anholt <eric@anholt.net>
 *      Dave Airlie <airlied@linux.ie>
 *      Jesse Barnes <jesse.barnes@intel.com>
 */

#include <linux/export.h>
#include <linux/moduleparam.h>

#include "drmP.h"
#include "drm_crtc.h"
#include "drm_fourcc.h"
#include "drm_crtc_helper.h"
#include "drm_fb_helper.h"
#include "drm_edid.h"

static bool drm_kms_helper_poll = true;
module_param_named(poll, drm_kms_helper_poll, bool, 0600);

static void drm_mode_validate_flag(struct drm_connector *connector,
				   int flags)
{
	struct drm_display_mode *mode;

	if (flags == (DRM_MODE_FLAG_DBLSCAN | DRM_MODE_FLAG_INTERLACE))
		return;

	list_for_each_entry(mode, &connector->modes, head) {
		if ((mode->flags & DRM_MODE_FLAG_INTERLACE) &&
				!(flags & DRM_MODE_FLAG_INTERLACE))
			mode->status = MODE_NO_INTERLACE;
		if ((mode->flags & DRM_MODE_FLAG_DBLSCAN) &&
				!(flags & DRM_MODE_FLAG_DBLSCAN))
			mode->status = MODE_NO_DBLESCAN;
	}

	return;
}

int drm_helper_probe_single_connector_modes(struct drm_connector *connector,
					    uint32_t maxX, uint32_t maxY)
{
	struct drm_device *dev = connector->dev;
	struct drm_display_mode *mode;
	struct drm_connector_helper_funcs *connector_funcs =
		connector->helper_private;
	int count = 0;
	int mode_flags = 0;

	DRM_DEBUG_KMS("[CONNECTOR:%d:%s]\n", connector->base.id,
			drm_get_connector_name(connector));
	
	list_for_each_entry(mode, &connector->modes, head)
		mode->status = MODE_UNVERIFIED;

	if (connector->force) {
		if (connector->force == DRM_FORCE_ON)
			connector->status = connector_status_connected;
		else
			connector->status = connector_status_disconnected;
		if (connector->funcs->force)
			connector->funcs->force(connector);
	} else {
		connector->status = connector->funcs->detect(connector, true);
		drm_kms_helper_poll_enable(dev);
	}

	if (connector->status == connector_status_disconnected) {
		DRM_DEBUG_KMS("[CONNECTOR:%d:%s] disconnected\n",
			connector->base.id, drm_get_connector_name(connector));
		drm_mode_connector_update_edid_property(connector, NULL);
		goto prune;
	}

#ifdef CONFIG_DRM_LOAD_EDID_FIRMWARE
	count = drm_load_edid_firmware(connector);
	if (count == 0)
#endif
		count = (*connector_funcs->get_modes)(connector);

	if (count == 0 && connector->status == connector_status_connected)
		count = drm_add_modes_noedid(connector, 1024, 768);
	if (count == 0)
		goto prune;

	drm_mode_connector_list_update(connector);

	if (maxX && maxY)
		drm_mode_validate_size(dev, &connector->modes, maxX,
				       maxY, 0);

	if (connector->interlace_allowed)
		mode_flags |= DRM_MODE_FLAG_INTERLACE;
	if (connector->doublescan_allowed)
		mode_flags |= DRM_MODE_FLAG_DBLSCAN;
	drm_mode_validate_flag(connector, mode_flags);

	list_for_each_entry(mode, &connector->modes, head) {
		if (mode->status == MODE_OK)
			mode->status = connector_funcs->mode_valid(connector,
								   mode);
	}

prune:
	drm_mode_prune_invalid(dev, &connector->modes, true);

	if (list_empty(&connector->modes))
		return 0;

	drm_mode_sort(&connector->modes);

	DRM_DEBUG_KMS("[CONNECTOR:%d:%s] probed modes :\n", connector->base.id,
			drm_get_connector_name(connector));
	list_for_each_entry(mode, &connector->modes, head) {
		mode->vrefresh = drm_mode_vrefresh(mode);

		drm_mode_set_crtcinfo(mode, CRTC_INTERLACE_HALVE_V);
		drm_mode_debug_printmodeline(mode);
	}

	return count;
}
EXPORT_SYMBOL(drm_helper_probe_single_connector_modes);

bool drm_helper_encoder_in_use(struct drm_encoder *encoder)
{
	struct drm_connector *connector;
	struct drm_device *dev = encoder->dev;
	list_for_each_entry(connector, &dev->mode_config.connector_list, head)
		if (connector->encoder == encoder)
			return true;
	return false;
}
EXPORT_SYMBOL(drm_helper_encoder_in_use);

bool drm_helper_crtc_in_use(struct drm_crtc *crtc)
{
	struct drm_encoder *encoder;
	struct drm_device *dev = crtc->dev;
	
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head)
		if (encoder->crtc == crtc && drm_helper_encoder_in_use(encoder))
			return true;
	return false;
}
EXPORT_SYMBOL(drm_helper_crtc_in_use);

static void
drm_encoder_disable(struct drm_encoder *encoder)
{
	struct drm_encoder_helper_funcs *encoder_funcs = encoder->helper_private;

	if (encoder_funcs->disable)
		(*encoder_funcs->disable)(encoder);
	else
		(*encoder_funcs->dpms)(encoder, DRM_MODE_DPMS_OFF);
}

void drm_helper_disable_unused_functions(struct drm_device *dev)
{
	struct drm_encoder *encoder;
	struct drm_connector *connector;
	struct drm_crtc *crtc;

	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		if (!connector->encoder)
			continue;
		if (connector->status == connector_status_disconnected)
			connector->encoder = NULL;
	}

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		if (!drm_helper_encoder_in_use(encoder)) {
			drm_encoder_disable(encoder);
			
			encoder->crtc = NULL;
		}
	}

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;
		crtc->enabled = drm_helper_crtc_in_use(crtc);
		if (!crtc->enabled) {
			if (crtc_funcs->disable)
				(*crtc_funcs->disable)(crtc);
			else
				(*crtc_funcs->dpms)(crtc, DRM_MODE_DPMS_OFF);
			crtc->fb = NULL;
		}
	}
}
EXPORT_SYMBOL(drm_helper_disable_unused_functions);

static bool drm_encoder_crtc_ok(struct drm_encoder *encoder,
				struct drm_crtc *crtc)
{
	struct drm_device *dev;
	struct drm_crtc *tmp;
	int crtc_mask = 1;

	WARN(!crtc, "checking null crtc?\n");

	dev = crtc->dev;

	list_for_each_entry(tmp, &dev->mode_config.crtc_list, head) {
		if (tmp == crtc)
			break;
		crtc_mask <<= 1;
	}

	if (encoder->possible_crtcs & crtc_mask)
		return true;
	return false;
}

static void
drm_crtc_prepare_encoders(struct drm_device *dev)
{
	struct drm_encoder_helper_funcs *encoder_funcs;
	struct drm_encoder *encoder;

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		encoder_funcs = encoder->helper_private;
		
		if (encoder->crtc == NULL)
			drm_encoder_disable(encoder);
		
		if (encoder_funcs->get_crtc &&
		    encoder->crtc != (*encoder_funcs->get_crtc)(encoder))
			drm_encoder_disable(encoder);
	}
}

bool drm_crtc_helper_set_mode(struct drm_crtc *crtc,
			      struct drm_display_mode *mode,
			      int x, int y,
			      struct drm_framebuffer *old_fb)
{
	struct drm_device *dev = crtc->dev;
	struct drm_display_mode *adjusted_mode, saved_mode, saved_hwmode;
	struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;
	struct drm_encoder_helper_funcs *encoder_funcs;
	int saved_x, saved_y;
	struct drm_encoder *encoder;
	bool ret = true;

	crtc->enabled = drm_helper_crtc_in_use(crtc);
	if (!crtc->enabled)
		return true;

	adjusted_mode = drm_mode_duplicate(dev, mode);
	if (!adjusted_mode)
		return false;

	saved_hwmode = crtc->hwmode;
	saved_mode = crtc->mode;
	saved_x = crtc->x;
	saved_y = crtc->y;

	crtc->mode = *mode;
	crtc->x = x;
	crtc->y = y;

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {

		if (encoder->crtc != crtc)
			continue;
		encoder_funcs = encoder->helper_private;
		if (!(ret = encoder_funcs->mode_fixup(encoder, mode,
						      adjusted_mode))) {
			DRM_DEBUG_KMS("Encoder fixup failed\n");
			goto done;
		}
	}

	if (!(ret = crtc_funcs->mode_fixup(crtc, mode, adjusted_mode))) {
		DRM_DEBUG_KMS("CRTC fixup failed\n");
		goto done;
	}
	DRM_DEBUG_KMS("[CRTC:%d]\n", crtc->base.id);

	
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {

		if (encoder->crtc != crtc)
			continue;
		encoder_funcs = encoder->helper_private;
		
		encoder_funcs->prepare(encoder);
	}

	drm_crtc_prepare_encoders(dev);

	crtc_funcs->prepare(crtc);

	ret = !crtc_funcs->mode_set(crtc, mode, adjusted_mode, x, y, old_fb);
	if (!ret)
	    goto done;

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {

		if (encoder->crtc != crtc)
			continue;

		DRM_DEBUG_KMS("[ENCODER:%d:%s] set [MODE:%d:%s]\n",
			encoder->base.id, drm_get_encoder_name(encoder),
			mode->base.id, mode->name);
		encoder_funcs = encoder->helper_private;
		encoder_funcs->mode_set(encoder, mode, adjusted_mode);
	}

	
	crtc_funcs->commit(crtc);

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {

		if (encoder->crtc != crtc)
			continue;

		encoder_funcs = encoder->helper_private;
		encoder_funcs->commit(encoder);

	}

	
	crtc->hwmode = *adjusted_mode;

	drm_calc_timestamping_constants(crtc);

	
done:
	drm_mode_destroy(dev, adjusted_mode);
	if (!ret) {
		crtc->hwmode = saved_hwmode;
		crtc->mode = saved_mode;
		crtc->x = saved_x;
		crtc->y = saved_y;
	}

	return ret;
}
EXPORT_SYMBOL(drm_crtc_helper_set_mode);


static int
drm_crtc_helper_disable(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct drm_connector *connector;
	struct drm_encoder *encoder;

	
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		if (encoder->crtc != crtc)
			continue;

		list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
			if (connector->encoder != encoder)
				continue;

			connector->encoder = NULL;
		}
	}

	drm_helper_disable_unused_functions(dev);
	return 0;
}

int drm_crtc_helper_set_config(struct drm_mode_set *set)
{
	struct drm_device *dev;
	struct drm_crtc *save_crtcs, *new_crtc, *crtc;
	struct drm_encoder *save_encoders, *new_encoder, *encoder;
	struct drm_framebuffer *old_fb = NULL;
	bool mode_changed = false; 
	bool fb_changed = false; 
	struct drm_connector *save_connectors, *connector;
	int count = 0, ro, fail = 0;
	struct drm_crtc_helper_funcs *crtc_funcs;
	struct drm_mode_set save_set;
	int ret = 0;
	int i;

	DRM_DEBUG_KMS("\n");

	if (!set)
		return -EINVAL;

	if (!set->crtc)
		return -EINVAL;

	if (!set->crtc->helper_private)
		return -EINVAL;

	crtc_funcs = set->crtc->helper_private;

	if (!set->mode)
		set->fb = NULL;

	if (set->fb) {
		DRM_DEBUG_KMS("[CRTC:%d] [FB:%d] #connectors=%d (x y) (%i %i)\n",
				set->crtc->base.id, set->fb->base.id,
				(int)set->num_connectors, set->x, set->y);
	} else {
		DRM_DEBUG_KMS("[CRTC:%d] [NOFB]\n", set->crtc->base.id);
		return drm_crtc_helper_disable(set->crtc);
	}

	dev = set->crtc->dev;

	save_crtcs = kzalloc(dev->mode_config.num_crtc *
			     sizeof(struct drm_crtc), GFP_KERNEL);
	if (!save_crtcs)
		return -ENOMEM;

	save_encoders = kzalloc(dev->mode_config.num_encoder *
				sizeof(struct drm_encoder), GFP_KERNEL);
	if (!save_encoders) {
		kfree(save_crtcs);
		return -ENOMEM;
	}

	save_connectors = kzalloc(dev->mode_config.num_connector *
				sizeof(struct drm_connector), GFP_KERNEL);
	if (!save_connectors) {
		kfree(save_crtcs);
		kfree(save_encoders);
		return -ENOMEM;
	}

	count = 0;
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		save_crtcs[count++] = *crtc;
	}

	count = 0;
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		save_encoders[count++] = *encoder;
	}

	count = 0;
	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		save_connectors[count++] = *connector;
	}

	save_set.crtc = set->crtc;
	save_set.mode = &set->crtc->mode;
	save_set.x = set->crtc->x;
	save_set.y = set->crtc->y;
	save_set.fb = set->crtc->fb;

	if (set->crtc->fb != set->fb) {
		
		if (set->crtc->fb == NULL) {
			DRM_DEBUG_KMS("crtc has no fb, full mode set\n");
			mode_changed = true;
		} else if (set->fb == NULL) {
			mode_changed = true;
		} else if (set->fb->depth != set->crtc->fb->depth) {
			mode_changed = true;
		} else if (set->fb->bits_per_pixel !=
			   set->crtc->fb->bits_per_pixel) {
			mode_changed = true;
		} else
			fb_changed = true;
	}

	if (set->x != set->crtc->x || set->y != set->crtc->y)
		fb_changed = true;

	if (set->mode && !drm_mode_equal(set->mode, &set->crtc->mode)) {
		DRM_DEBUG_KMS("modes are different, full mode set\n");
		drm_mode_debug_printmodeline(&set->crtc->mode);
		drm_mode_debug_printmodeline(set->mode);
		mode_changed = true;
	}

	
	count = 0;
	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		struct drm_connector_helper_funcs *connector_funcs =
			connector->helper_private;
		new_encoder = connector->encoder;
		for (ro = 0; ro < set->num_connectors; ro++) {
			if (set->connectors[ro] == connector) {
				new_encoder = connector_funcs->best_encoder(connector);
				if (new_encoder == NULL)
					
					fail = 1;
				break;
			}
		}

		if (new_encoder != connector->encoder) {
			DRM_DEBUG_KMS("encoder changed, full mode switch\n");
			mode_changed = true;
			if (connector->encoder)
				connector->encoder->crtc = NULL;
			connector->encoder = new_encoder;
		}
	}

	if (fail) {
		ret = -EINVAL;
		goto fail;
	}

	count = 0;
	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		if (!connector->encoder)
			continue;

		if (connector->encoder->crtc == set->crtc)
			new_crtc = NULL;
		else
			new_crtc = connector->encoder->crtc;

		for (ro = 0; ro < set->num_connectors; ro++) {
			if (set->connectors[ro] == connector)
				new_crtc = set->crtc;
		}

		
		if (new_crtc &&
		    !drm_encoder_crtc_ok(connector->encoder, new_crtc)) {
			ret = -EINVAL;
			goto fail;
		}
		if (new_crtc != connector->encoder->crtc) {
			DRM_DEBUG_KMS("crtc changed, full mode switch\n");
			mode_changed = true;
			connector->encoder->crtc = new_crtc;
		}
		if (new_crtc) {
			DRM_DEBUG_KMS("[CONNECTOR:%d:%s] to [CRTC:%d]\n",
				connector->base.id, drm_get_connector_name(connector),
				new_crtc->base.id);
		} else {
			DRM_DEBUG_KMS("[CONNECTOR:%d:%s] to [NOCRTC]\n",
				connector->base.id, drm_get_connector_name(connector));
		}
	}

	
	if (fb_changed && !crtc_funcs->mode_set_base)
		mode_changed = true;

	if (mode_changed) {
		set->crtc->enabled = drm_helper_crtc_in_use(set->crtc);
		if (set->crtc->enabled) {
			DRM_DEBUG_KMS("attempting to set mode from"
					" userspace\n");
			drm_mode_debug_printmodeline(set->mode);
			old_fb = set->crtc->fb;
			set->crtc->fb = set->fb;
			if (!drm_crtc_helper_set_mode(set->crtc, set->mode,
						      set->x, set->y,
						      old_fb)) {
				DRM_ERROR("failed to set mode on [CRTC:%d]\n",
					  set->crtc->base.id);
				set->crtc->fb = old_fb;
				ret = -EINVAL;
				goto fail;
			}
			DRM_DEBUG_KMS("Setting connector DPMS state to on\n");
			for (i = 0; i < set->num_connectors; i++) {
				DRM_DEBUG_KMS("\t[CONNECTOR:%d:%s] set DPMS on\n", set->connectors[i]->base.id,
					      drm_get_connector_name(set->connectors[i]));
				set->connectors[i]->funcs->dpms(set->connectors[i], DRM_MODE_DPMS_ON);
			}
		}
		drm_helper_disable_unused_functions(dev);
	} else if (fb_changed) {
		set->crtc->x = set->x;
		set->crtc->y = set->y;

		old_fb = set->crtc->fb;
		if (set->crtc->fb != set->fb)
			set->crtc->fb = set->fb;
		ret = crtc_funcs->mode_set_base(set->crtc,
						set->x, set->y, old_fb);
		if (ret != 0) {
			set->crtc->fb = old_fb;
			goto fail;
		}
	}

	kfree(save_connectors);
	kfree(save_encoders);
	kfree(save_crtcs);
	return 0;

fail:
	
	count = 0;
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		*crtc = save_crtcs[count++];
	}

	count = 0;
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		*encoder = save_encoders[count++];
	}

	count = 0;
	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		*connector = save_connectors[count++];
	}

	
	if (mode_changed &&
	    !drm_crtc_helper_set_mode(save_set.crtc, save_set.mode, save_set.x,
				      save_set.y, save_set.fb))
		DRM_ERROR("failed to restore config after modeset failure\n");

	kfree(save_connectors);
	kfree(save_encoders);
	kfree(save_crtcs);
	return ret;
}
EXPORT_SYMBOL(drm_crtc_helper_set_config);

static int drm_helper_choose_encoder_dpms(struct drm_encoder *encoder)
{
	int dpms = DRM_MODE_DPMS_OFF;
	struct drm_connector *connector;
	struct drm_device *dev = encoder->dev;

	list_for_each_entry(connector, &dev->mode_config.connector_list, head)
		if (connector->encoder == encoder)
			if (connector->dpms < dpms)
				dpms = connector->dpms;
	return dpms;
}

static int drm_helper_choose_crtc_dpms(struct drm_crtc *crtc)
{
	int dpms = DRM_MODE_DPMS_OFF;
	struct drm_connector *connector;
	struct drm_device *dev = crtc->dev;

	list_for_each_entry(connector, &dev->mode_config.connector_list, head)
		if (connector->encoder && connector->encoder->crtc == crtc)
			if (connector->dpms < dpms)
				dpms = connector->dpms;
	return dpms;
}

void drm_helper_connector_dpms(struct drm_connector *connector, int mode)
{
	struct drm_encoder *encoder = connector->encoder;
	struct drm_crtc *crtc = encoder ? encoder->crtc : NULL;
	int old_dpms;

	if (mode == connector->dpms)
		return;

	old_dpms = connector->dpms;
	connector->dpms = mode;

	
	if (mode < old_dpms) {
		if (crtc) {
			struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;
			if (crtc_funcs->dpms)
				(*crtc_funcs->dpms) (crtc,
						     drm_helper_choose_crtc_dpms(crtc));
		}
		if (encoder) {
			struct drm_encoder_helper_funcs *encoder_funcs = encoder->helper_private;
			if (encoder_funcs->dpms)
				(*encoder_funcs->dpms) (encoder,
							drm_helper_choose_encoder_dpms(encoder));
		}
	}

	
	if (mode > old_dpms) {
		if (encoder) {
			struct drm_encoder_helper_funcs *encoder_funcs = encoder->helper_private;
			if (encoder_funcs->dpms)
				(*encoder_funcs->dpms) (encoder,
							drm_helper_choose_encoder_dpms(encoder));
		}
		if (crtc) {
			struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;
			if (crtc_funcs->dpms)
				(*crtc_funcs->dpms) (crtc,
						     drm_helper_choose_crtc_dpms(crtc));
		}
	}

	return;
}
EXPORT_SYMBOL(drm_helper_connector_dpms);

int drm_helper_mode_fill_fb_struct(struct drm_framebuffer *fb,
				   struct drm_mode_fb_cmd2 *mode_cmd)
{
	int i;

	fb->width = mode_cmd->width;
	fb->height = mode_cmd->height;
	for (i = 0; i < 4; i++) {
		fb->pitches[i] = mode_cmd->pitches[i];
		fb->offsets[i] = mode_cmd->offsets[i];
	}
	drm_fb_get_bpp_depth(mode_cmd->pixel_format, &fb->depth,
				    &fb->bits_per_pixel);
	fb->pixel_format = mode_cmd->pixel_format;

	return 0;
}
EXPORT_SYMBOL(drm_helper_mode_fill_fb_struct);

int drm_helper_resume_force_mode(struct drm_device *dev)
{
	struct drm_crtc *crtc;
	struct drm_encoder *encoder;
	struct drm_encoder_helper_funcs *encoder_funcs;
	struct drm_crtc_helper_funcs *crtc_funcs;
	int ret;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {

		if (!crtc->enabled)
			continue;

		ret = drm_crtc_helper_set_mode(crtc, &crtc->mode,
					       crtc->x, crtc->y, crtc->fb);

		if (ret == false)
			DRM_ERROR("failed to set mode on crtc %p\n", crtc);

		
		if (drm_helper_choose_crtc_dpms(crtc)) {
			list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {

				if(encoder->crtc != crtc)
					continue;

				encoder_funcs = encoder->helper_private;
				if (encoder_funcs->dpms)
					(*encoder_funcs->dpms) (encoder,
								drm_helper_choose_encoder_dpms(encoder));
			}

			crtc_funcs = crtc->helper_private;
			if (crtc_funcs->dpms)
				(*crtc_funcs->dpms) (crtc,
						     drm_helper_choose_crtc_dpms(crtc));
		}
	}
	
	drm_helper_disable_unused_functions(dev);
	return 0;
}
EXPORT_SYMBOL(drm_helper_resume_force_mode);

#define DRM_OUTPUT_POLL_PERIOD (10*HZ)
static void output_poll_execute(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct drm_device *dev = container_of(delayed_work, struct drm_device, mode_config.output_poll_work);
	struct drm_connector *connector;
	enum drm_connector_status old_status;
	bool repoll = false, changed = false;

	if (!drm_kms_helper_poll)
		return;

	mutex_lock(&dev->mode_config.mutex);
	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {

		if (!connector->polled)
			continue;

		else if (connector->polled & (DRM_CONNECTOR_POLL_CONNECT | DRM_CONNECTOR_POLL_DISCONNECT))
			repoll = true;

		old_status = connector->status;
		if (old_status == connector_status_connected &&
		    !(connector->polled & DRM_CONNECTOR_POLL_DISCONNECT) &&
		    !(connector->polled & DRM_CONNECTOR_POLL_HPD))
			continue;

		connector->status = connector->funcs->detect(connector, false);
		DRM_DEBUG_KMS("[CONNECTOR:%d:%s] status updated from %d to %d\n",
			      connector->base.id,
			      drm_get_connector_name(connector),
			      old_status, connector->status);
		if (old_status != connector->status)
			changed = true;
	}

	mutex_unlock(&dev->mode_config.mutex);

	if (changed) {
		
		drm_sysfs_hotplug_event(dev);
		if (dev->mode_config.funcs->output_poll_changed)
			dev->mode_config.funcs->output_poll_changed(dev);
	}

	if (repoll)
		queue_delayed_work(system_nrt_wq, delayed_work, DRM_OUTPUT_POLL_PERIOD);
}

void drm_kms_helper_poll_disable(struct drm_device *dev)
{
	if (!dev->mode_config.poll_enabled)
		return;
	cancel_delayed_work_sync(&dev->mode_config.output_poll_work);
}
EXPORT_SYMBOL(drm_kms_helper_poll_disable);

void drm_kms_helper_poll_enable(struct drm_device *dev)
{
	bool poll = false;
	struct drm_connector *connector;

	if (!dev->mode_config.poll_enabled || !drm_kms_helper_poll)
		return;

	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		if (connector->polled)
			poll = true;
	}

	if (poll)
		queue_delayed_work(system_nrt_wq, &dev->mode_config.output_poll_work, DRM_OUTPUT_POLL_PERIOD);
}
EXPORT_SYMBOL(drm_kms_helper_poll_enable);

void drm_kms_helper_poll_init(struct drm_device *dev)
{
	INIT_DELAYED_WORK(&dev->mode_config.output_poll_work, output_poll_execute);
	dev->mode_config.poll_enabled = true;

	drm_kms_helper_poll_enable(dev);
}
EXPORT_SYMBOL(drm_kms_helper_poll_init);

void drm_kms_helper_poll_fini(struct drm_device *dev)
{
	drm_kms_helper_poll_disable(dev);
}
EXPORT_SYMBOL(drm_kms_helper_poll_fini);

void drm_helper_hpd_irq_event(struct drm_device *dev)
{
	if (!dev->mode_config.poll_enabled)
		return;

	
	cancel_delayed_work(&dev->mode_config.output_poll_work);
	if (drm_kms_helper_poll)
		queue_delayed_work(system_nrt_wq, &dev->mode_config.output_poll_work, 0);
}
EXPORT_SYMBOL(drm_helper_hpd_irq_event);


int drm_format_num_planes(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_YUV410:
	case DRM_FORMAT_YVU410:
	case DRM_FORMAT_YUV411:
	case DRM_FORMAT_YVU411:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU422:
	case DRM_FORMAT_YUV444:
	case DRM_FORMAT_YVU444:
		return 3;
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		return 2;
	default:
		return 1;
	}
}
EXPORT_SYMBOL(drm_format_num_planes);
