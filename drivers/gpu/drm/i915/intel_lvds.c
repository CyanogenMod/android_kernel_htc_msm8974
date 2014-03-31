/*
 * Copyright © 2006-2007 Intel Corporation
 * Copyright (c) 2006 Dave Airlie <airlied@linux.ie>
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	Eric Anholt <eric@anholt.net>
 *      Dave Airlie <airlied@linux.ie>
 *      Jesse Barnes <jesse.barnes@intel.com>
 */

#include <acpi/button.h>
#include <linux/dmi.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include "drmP.h"
#include "drm.h"
#include "drm_crtc.h"
#include "drm_edid.h"
#include "intel_drv.h"
#include "i915_drm.h"
#include "i915_drv.h"
#include <linux/acpi.h>

struct intel_lvds {
	struct intel_encoder base;

	struct edid *edid;

	int fitting_mode;
	u32 pfit_control;
	u32 pfit_pgm_ratios;
	bool pfit_dirty;

	struct drm_display_mode *fixed_mode;
};

static struct intel_lvds *to_intel_lvds(struct drm_encoder *encoder)
{
	return container_of(encoder, struct intel_lvds, base.base);
}

static struct intel_lvds *intel_attached_lvds(struct drm_connector *connector)
{
	return container_of(intel_attached_encoder(connector),
			    struct intel_lvds, base);
}

static void intel_lvds_enable(struct intel_lvds *intel_lvds)
{
	struct drm_device *dev = intel_lvds->base.base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 ctl_reg, lvds_reg, stat_reg;

	if (HAS_PCH_SPLIT(dev)) {
		ctl_reg = PCH_PP_CONTROL;
		lvds_reg = PCH_LVDS;
		stat_reg = PCH_PP_STATUS;
	} else {
		ctl_reg = PP_CONTROL;
		lvds_reg = LVDS;
		stat_reg = PP_STATUS;
	}

	I915_WRITE(lvds_reg, I915_READ(lvds_reg) | LVDS_PORT_EN);

	if (intel_lvds->pfit_dirty) {
		DRM_DEBUG_KMS("applying panel-fitter: %x, %x\n",
			      intel_lvds->pfit_control,
			      intel_lvds->pfit_pgm_ratios);

		I915_WRITE(PFIT_PGM_RATIOS, intel_lvds->pfit_pgm_ratios);
		I915_WRITE(PFIT_CONTROL, intel_lvds->pfit_control);
		intel_lvds->pfit_dirty = false;
	}

	I915_WRITE(ctl_reg, I915_READ(ctl_reg) | POWER_TARGET_ON);
	POSTING_READ(lvds_reg);
	if (wait_for((I915_READ(stat_reg) & PP_ON) != 0, 1000))
		DRM_ERROR("timed out waiting for panel to power on\n");

	intel_panel_enable_backlight(dev);
}

static void intel_lvds_disable(struct intel_lvds *intel_lvds)
{
	struct drm_device *dev = intel_lvds->base.base.dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	u32 ctl_reg, lvds_reg, stat_reg;

	if (HAS_PCH_SPLIT(dev)) {
		ctl_reg = PCH_PP_CONTROL;
		lvds_reg = PCH_LVDS;
		stat_reg = PCH_PP_STATUS;
	} else {
		ctl_reg = PP_CONTROL;
		lvds_reg = LVDS;
		stat_reg = PP_STATUS;
	}

	intel_panel_disable_backlight(dev);

	I915_WRITE(ctl_reg, I915_READ(ctl_reg) & ~POWER_TARGET_ON);
	if (wait_for((I915_READ(stat_reg) & PP_ON) == 0, 1000))
		DRM_ERROR("timed out waiting for panel to power off\n");

	if (intel_lvds->pfit_control) {
		I915_WRITE(PFIT_CONTROL, 0);
		intel_lvds->pfit_dirty = true;
	}

	I915_WRITE(lvds_reg, I915_READ(lvds_reg) & ~LVDS_PORT_EN);
	POSTING_READ(lvds_reg);
}

static void intel_lvds_dpms(struct drm_encoder *encoder, int mode)
{
	struct intel_lvds *intel_lvds = to_intel_lvds(encoder);

	if (mode == DRM_MODE_DPMS_ON)
		intel_lvds_enable(intel_lvds);
	else
		intel_lvds_disable(intel_lvds);

	
}

static int intel_lvds_mode_valid(struct drm_connector *connector,
				 struct drm_display_mode *mode)
{
	struct intel_lvds *intel_lvds = intel_attached_lvds(connector);
	struct drm_display_mode *fixed_mode = intel_lvds->fixed_mode;

	if (mode->hdisplay > fixed_mode->hdisplay)
		return MODE_PANEL;
	if (mode->vdisplay > fixed_mode->vdisplay)
		return MODE_PANEL;

	return MODE_OK;
}

static void
centre_horizontally(struct drm_display_mode *mode,
		    int width)
{
	u32 border, sync_pos, blank_width, sync_width;

	
	sync_width = mode->crtc_hsync_end - mode->crtc_hsync_start;
	blank_width = mode->crtc_hblank_end - mode->crtc_hblank_start;
	sync_pos = (blank_width - sync_width + 1) / 2;

	border = (mode->hdisplay - width + 1) / 2;
	border += border & 1; 

	mode->crtc_hdisplay = width;
	mode->crtc_hblank_start = width + border;
	mode->crtc_hblank_end = mode->crtc_hblank_start + blank_width;

	mode->crtc_hsync_start = mode->crtc_hblank_start + sync_pos;
	mode->crtc_hsync_end = mode->crtc_hsync_start + sync_width;

	mode->private_flags |= INTEL_MODE_CRTC_TIMINGS_SET;
}

static void
centre_vertically(struct drm_display_mode *mode,
		  int height)
{
	u32 border, sync_pos, blank_width, sync_width;

	
	sync_width = mode->crtc_vsync_end - mode->crtc_vsync_start;
	blank_width = mode->crtc_vblank_end - mode->crtc_vblank_start;
	sync_pos = (blank_width - sync_width + 1) / 2;

	border = (mode->vdisplay - height + 1) / 2;

	mode->crtc_vdisplay = height;
	mode->crtc_vblank_start = height + border;
	mode->crtc_vblank_end = mode->crtc_vblank_start + blank_width;

	mode->crtc_vsync_start = mode->crtc_vblank_start + sync_pos;
	mode->crtc_vsync_end = mode->crtc_vsync_start + sync_width;

	mode->private_flags |= INTEL_MODE_CRTC_TIMINGS_SET;
}

static inline u32 panel_fitter_scaling(u32 source, u32 target)
{
#define ACCURACY 12
#define FACTOR (1 << ACCURACY)
	u32 ratio = source * FACTOR / target;
	return (FACTOR * ratio + FACTOR/2) / FACTOR;
}

static bool intel_lvds_mode_fixup(struct drm_encoder *encoder,
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_crtc *intel_crtc = to_intel_crtc(encoder->crtc);
	struct intel_lvds *intel_lvds = to_intel_lvds(encoder);
	struct drm_encoder *tmp_encoder;
	u32 pfit_control = 0, pfit_pgm_ratios = 0, border = 0;
	int pipe;

	
	if (INTEL_INFO(dev)->gen < 4 && intel_crtc->pipe == 0) {
		DRM_ERROR("Can't support LVDS on pipe A\n");
		return false;
	}

	
	list_for_each_entry(tmp_encoder, &dev->mode_config.encoder_list, head) {
		if (tmp_encoder != encoder && tmp_encoder->crtc == encoder->crtc) {
			DRM_ERROR("Can't enable LVDS and another "
			       "encoder on the same pipe\n");
			return false;
		}
	}

	intel_fixed_panel_mode(intel_lvds->fixed_mode, adjusted_mode);

	if (HAS_PCH_SPLIT(dev)) {
		intel_pch_panel_fitting(dev, intel_lvds->fitting_mode,
					mode, adjusted_mode);
		return true;
	}

	
	if (adjusted_mode->hdisplay == mode->hdisplay &&
	    adjusted_mode->vdisplay == mode->vdisplay)
		goto out;

	
	if (INTEL_INFO(dev)->gen >= 4)
		pfit_control |= ((intel_crtc->pipe << PFIT_PIPE_SHIFT) |
				 PFIT_FILTER_FUZZY);

	for_each_pipe(pipe)
		I915_WRITE(BCLRPAT(pipe), 0);

	drm_mode_set_crtcinfo(adjusted_mode, 0);

	switch (intel_lvds->fitting_mode) {
	case DRM_MODE_SCALE_CENTER:
		centre_horizontally(adjusted_mode, mode->hdisplay);
		centre_vertically(adjusted_mode, mode->vdisplay);
		border = LVDS_BORDER_ENABLE;
		break;

	case DRM_MODE_SCALE_ASPECT:
		
		if (INTEL_INFO(dev)->gen >= 4) {
			u32 scaled_width = adjusted_mode->hdisplay * mode->vdisplay;
			u32 scaled_height = mode->hdisplay * adjusted_mode->vdisplay;

			
			if (scaled_width > scaled_height)
				pfit_control |= PFIT_ENABLE | PFIT_SCALING_PILLAR;
			else if (scaled_width < scaled_height)
				pfit_control |= PFIT_ENABLE | PFIT_SCALING_LETTER;
			else if (adjusted_mode->hdisplay != mode->hdisplay)
				pfit_control |= PFIT_ENABLE | PFIT_SCALING_AUTO;
		} else {
			u32 scaled_width = adjusted_mode->hdisplay * mode->vdisplay;
			u32 scaled_height = mode->hdisplay * adjusted_mode->vdisplay;
			if (scaled_width > scaled_height) { 
				centre_horizontally(adjusted_mode, scaled_height / mode->vdisplay);

				border = LVDS_BORDER_ENABLE;
				if (mode->vdisplay != adjusted_mode->vdisplay) {
					u32 bits = panel_fitter_scaling(mode->vdisplay, adjusted_mode->vdisplay);
					pfit_pgm_ratios |= (bits << PFIT_HORIZ_SCALE_SHIFT |
							    bits << PFIT_VERT_SCALE_SHIFT);
					pfit_control |= (PFIT_ENABLE |
							 VERT_INTERP_BILINEAR |
							 HORIZ_INTERP_BILINEAR);
				}
			} else if (scaled_width < scaled_height) { 
				centre_vertically(adjusted_mode, scaled_width / mode->hdisplay);

				border = LVDS_BORDER_ENABLE;
				if (mode->hdisplay != adjusted_mode->hdisplay) {
					u32 bits = panel_fitter_scaling(mode->hdisplay, adjusted_mode->hdisplay);
					pfit_pgm_ratios |= (bits << PFIT_HORIZ_SCALE_SHIFT |
							    bits << PFIT_VERT_SCALE_SHIFT);
					pfit_control |= (PFIT_ENABLE |
							 VERT_INTERP_BILINEAR |
							 HORIZ_INTERP_BILINEAR);
				}
			} else
				
				pfit_control |= (PFIT_ENABLE |
						 VERT_AUTO_SCALE | HORIZ_AUTO_SCALE |
						 VERT_INTERP_BILINEAR |
						 HORIZ_INTERP_BILINEAR);
		}
		break;

	case DRM_MODE_SCALE_FULLSCREEN:
		if (mode->vdisplay != adjusted_mode->vdisplay ||
		    mode->hdisplay != adjusted_mode->hdisplay) {
			pfit_control |= PFIT_ENABLE;
			if (INTEL_INFO(dev)->gen >= 4)
				pfit_control |= PFIT_SCALING_AUTO;
			else
				pfit_control |= (VERT_AUTO_SCALE |
						 VERT_INTERP_BILINEAR |
						 HORIZ_AUTO_SCALE |
						 HORIZ_INTERP_BILINEAR);
		}
		break;

	default:
		break;
	}

out:
	
	if ((pfit_control & PFIT_ENABLE) == 0) {
		pfit_control = 0;
		pfit_pgm_ratios = 0;
	}

	
	if (INTEL_INFO(dev)->gen < 4 && dev_priv->lvds_dither)
		pfit_control |= PANEL_8TO6_DITHER_ENABLE;

	if (pfit_control != intel_lvds->pfit_control ||
	    pfit_pgm_ratios != intel_lvds->pfit_pgm_ratios) {
		intel_lvds->pfit_control = pfit_control;
		intel_lvds->pfit_pgm_ratios = pfit_pgm_ratios;
		intel_lvds->pfit_dirty = true;
	}
	dev_priv->lvds_border_bits = border;


	return true;
}

static void intel_lvds_prepare(struct drm_encoder *encoder)
{
	struct intel_lvds *intel_lvds = to_intel_lvds(encoder);

	if (!HAS_PCH_SPLIT(encoder->dev) && intel_lvds->pfit_dirty)
		intel_lvds_disable(intel_lvds);
}

static void intel_lvds_commit(struct drm_encoder *encoder)
{
	struct intel_lvds *intel_lvds = to_intel_lvds(encoder);

	intel_lvds_enable(intel_lvds);
}

static void intel_lvds_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
}

static enum drm_connector_status
intel_lvds_detect(struct drm_connector *connector, bool force)
{
	struct drm_device *dev = connector->dev;
	enum drm_connector_status status;

	status = intel_panel_detect(dev);
	if (status != connector_status_unknown)
		return status;

	return connector_status_connected;
}

static int intel_lvds_get_modes(struct drm_connector *connector)
{
	struct intel_lvds *intel_lvds = intel_attached_lvds(connector);
	struct drm_device *dev = connector->dev;
	struct drm_display_mode *mode;

	if (intel_lvds->edid)
		return drm_add_edid_modes(connector, intel_lvds->edid);

	mode = drm_mode_duplicate(dev, intel_lvds->fixed_mode);
	if (mode == NULL)
		return 0;

	drm_mode_probed_add(connector, mode);
	return 1;
}

static int intel_no_modeset_on_lid_dmi_callback(const struct dmi_system_id *id)
{
	DRM_DEBUG_KMS("Skipping forced modeset for %s\n", id->ident);
	return 1;
}

static const struct dmi_system_id intel_no_modeset_on_lid[] = {
	{
		.callback = intel_no_modeset_on_lid_dmi_callback,
		.ident = "Toshiba Tecra A11",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "TOSHIBA"),
			DMI_MATCH(DMI_PRODUCT_NAME, "TECRA A11"),
		},
	},

	{ }	
};

static int intel_lid_notify(struct notifier_block *nb, unsigned long val,
			    void *unused)
{
	struct drm_i915_private *dev_priv =
		container_of(nb, struct drm_i915_private, lid_notifier);
	struct drm_device *dev = dev_priv->dev;
	struct drm_connector *connector = dev_priv->int_lvds_connector;

	if (dev->switch_power_state != DRM_SWITCH_POWER_ON)
		return NOTIFY_OK;

	if (connector)
		connector->status = connector->funcs->detect(connector,
							     false);

	
	if (dmi_check_system(intel_no_modeset_on_lid))
		return NOTIFY_OK;
	if (!acpi_lid_open()) {
		dev_priv->modeset_on_lid = 1;
		return NOTIFY_OK;
	}

	if (!dev_priv->modeset_on_lid)
		return NOTIFY_OK;

	dev_priv->modeset_on_lid = 0;

	mutex_lock(&dev->mode_config.mutex);
	drm_helper_resume_force_mode(dev);
	mutex_unlock(&dev->mode_config.mutex);

	return NOTIFY_OK;
}

static void intel_lvds_destroy(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;

	intel_panel_destroy_backlight(dev);

	if (dev_priv->lid_notifier.notifier_call)
		acpi_lid_notifier_unregister(&dev_priv->lid_notifier);
	drm_sysfs_connector_remove(connector);
	drm_connector_cleanup(connector);
	kfree(connector);
}

static int intel_lvds_set_property(struct drm_connector *connector,
				   struct drm_property *property,
				   uint64_t value)
{
	struct intel_lvds *intel_lvds = intel_attached_lvds(connector);
	struct drm_device *dev = connector->dev;

	if (property == dev->mode_config.scaling_mode_property) {
		struct drm_crtc *crtc = intel_lvds->base.base.crtc;

		if (value == DRM_MODE_SCALE_NONE) {
			DRM_DEBUG_KMS("no scaling not supported\n");
			return -EINVAL;
		}

		if (intel_lvds->fitting_mode == value) {
			
			return 0;
		}
		intel_lvds->fitting_mode = value;
		if (crtc && crtc->enabled) {
			drm_crtc_helper_set_mode(crtc, &crtc->mode,
				crtc->x, crtc->y, crtc->fb);
		}
	}

	return 0;
}

static const struct drm_encoder_helper_funcs intel_lvds_helper_funcs = {
	.dpms = intel_lvds_dpms,
	.mode_fixup = intel_lvds_mode_fixup,
	.prepare = intel_lvds_prepare,
	.mode_set = intel_lvds_mode_set,
	.commit = intel_lvds_commit,
};

static const struct drm_connector_helper_funcs intel_lvds_connector_helper_funcs = {
	.get_modes = intel_lvds_get_modes,
	.mode_valid = intel_lvds_mode_valid,
	.best_encoder = intel_best_encoder,
};

static const struct drm_connector_funcs intel_lvds_connector_funcs = {
	.dpms = drm_helper_connector_dpms,
	.detect = intel_lvds_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.set_property = intel_lvds_set_property,
	.destroy = intel_lvds_destroy,
};

static const struct drm_encoder_funcs intel_lvds_enc_funcs = {
	.destroy = intel_encoder_destroy,
};

static int __init intel_no_lvds_dmi_callback(const struct dmi_system_id *id)
{
	DRM_DEBUG_KMS("Skipping LVDS initialization for %s\n", id->ident);
	return 1;
}

static const struct dmi_system_id intel_no_lvds[] = {
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "Apple Mac Mini (Core series)",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Apple"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Macmini1,1"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "Apple Mac Mini (Core 2 series)",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Apple"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Macmini2,1"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "MSI IM-945GSE-A",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "MSI"),
			DMI_MATCH(DMI_PRODUCT_NAME, "A9830IMS"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "Dell Studio Hybrid",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "Studio Hybrid 140g"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "Dell OptiPlex FX170",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "OptiPlex FX170"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "AOpen Mini PC",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "AOpen"),
			DMI_MATCH(DMI_PRODUCT_NAME, "i965GMx-IF"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "AOpen Mini PC MP915",
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "AOpen"),
			DMI_MATCH(DMI_BOARD_NAME, "i915GMx-F"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "AOpen i915GMm-HFS",
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "AOpen"),
			DMI_MATCH(DMI_BOARD_NAME, "i915GMm-HFS"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
                .ident = "AOpen i45GMx-I",
                .matches = {
                        DMI_MATCH(DMI_BOARD_VENDOR, "AOpen"),
                        DMI_MATCH(DMI_BOARD_NAME, "i45GMx-I"),
                },
        },
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "Aopen i945GTt-VFA",
		.matches = {
			DMI_MATCH(DMI_PRODUCT_VERSION, "AO00001JW"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "Clientron U800",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Clientron"),
			DMI_MATCH(DMI_PRODUCT_NAME, "U800"),
		},
	},
	{
                .callback = intel_no_lvds_dmi_callback,
                .ident = "Clientron E830",
                .matches = {
                        DMI_MATCH(DMI_SYS_VENDOR, "Clientron"),
                        DMI_MATCH(DMI_PRODUCT_NAME, "E830"),
                },
        },
        {
		.callback = intel_no_lvds_dmi_callback,
		.ident = "Asus EeeBox PC EB1007",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK Computer INC."),
			DMI_MATCH(DMI_PRODUCT_NAME, "EB1007"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "Asus AT5NM10T-I",
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "ASUSTeK Computer INC."),
			DMI_MATCH(DMI_BOARD_NAME, "AT5NM10T-I"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "Hewlett-Packard t5745",
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "Hewlett-Packard"),
			DMI_MATCH(DMI_PRODUCT_NAME, "hp t5745"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "Hewlett-Packard st5747",
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "Hewlett-Packard"),
			DMI_MATCH(DMI_PRODUCT_NAME, "hp st5747"),
		},
	},
	{
		.callback = intel_no_lvds_dmi_callback,
		.ident = "MSI Wind Box DC500",
		.matches = {
			DMI_MATCH(DMI_BOARD_VENDOR, "MICRO-STAR INTERNATIONAL CO., LTD"),
			DMI_MATCH(DMI_BOARD_NAME, "MS-7469"),
		},
	},

	{ }	
};

static void intel_find_lvds_downclock(struct drm_device *dev,
				      struct drm_display_mode *fixed_mode,
				      struct drm_connector *connector)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_display_mode *scan;
	int temp_downclock;

	temp_downclock = fixed_mode->clock;
	list_for_each_entry(scan, &connector->probed_modes, head) {
		if (scan->hdisplay == fixed_mode->hdisplay &&
		    scan->hsync_start == fixed_mode->hsync_start &&
		    scan->hsync_end == fixed_mode->hsync_end &&
		    scan->htotal == fixed_mode->htotal &&
		    scan->vdisplay == fixed_mode->vdisplay &&
		    scan->vsync_start == fixed_mode->vsync_start &&
		    scan->vsync_end == fixed_mode->vsync_end &&
		    scan->vtotal == fixed_mode->vtotal) {
			if (scan->clock < temp_downclock) {
				temp_downclock = scan->clock;
			}
		}
	}
	if (temp_downclock < fixed_mode->clock && i915_lvds_downclock) {
		
		dev_priv->lvds_downclock_avail = 1;
		dev_priv->lvds_downclock = temp_downclock;
		DRM_DEBUG_KMS("LVDS downclock is found in EDID. "
			      "Normal clock %dKhz, downclock %dKhz\n",
			      fixed_mode->clock, temp_downclock);
	}
}

static bool lvds_is_present_in_vbt(struct drm_device *dev,
				   u8 *i2c_pin)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	int i;

	if (!dev_priv->child_dev_num)
		return true;

	for (i = 0; i < dev_priv->child_dev_num; i++) {
		struct child_device_config *child = dev_priv->child_dev + i;

		if (child->device_type != DEVICE_TYPE_INT_LFP &&
		    child->device_type != DEVICE_TYPE_LFP)
			continue;

		if (child->i2c_pin)
		    *i2c_pin = child->i2c_pin;

		if (child->addin_offset)
			return true;

		/* But even then some BIOS writers perform some black magic
		 * and instantiate the device without reference to any
		 * additional data.  Trust that if the VBT was written into
		 * the OpRegion then they have validated the LVDS's existence.
		 */
		if (dev_priv->opregion.vbt)
			return true;
	}

	return false;
}

static bool intel_lvds_supported(struct drm_device *dev)
{
	if (HAS_PCH_SPLIT(dev))
		return true;

	return IS_MOBILE(dev) && !IS_I830(dev);
}

bool intel_lvds_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_lvds *intel_lvds;
	struct intel_encoder *intel_encoder;
	struct intel_connector *intel_connector;
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	struct drm_display_mode *scan; 
	struct drm_crtc *crtc;
	u32 lvds;
	int pipe;
	u8 pin;

	if (!intel_lvds_supported(dev))
		return false;

	
	if (dmi_check_system(intel_no_lvds))
		return false;

	pin = GMBUS_PORT_PANEL;
	if (!lvds_is_present_in_vbt(dev, &pin)) {
		DRM_DEBUG_KMS("LVDS is not present in VBT\n");
		return false;
	}

	if (HAS_PCH_SPLIT(dev)) {
		if ((I915_READ(PCH_LVDS) & LVDS_DETECTED) == 0)
			return false;
		if (dev_priv->edp.support) {
			DRM_DEBUG_KMS("disable LVDS for eDP support\n");
			return false;
		}
	}

	intel_lvds = kzalloc(sizeof(struct intel_lvds), GFP_KERNEL);
	if (!intel_lvds) {
		return false;
	}

	intel_connector = kzalloc(sizeof(struct intel_connector), GFP_KERNEL);
	if (!intel_connector) {
		kfree(intel_lvds);
		return false;
	}

	if (!HAS_PCH_SPLIT(dev)) {
		intel_lvds->pfit_control = I915_READ(PFIT_CONTROL);
	}

	intel_encoder = &intel_lvds->base;
	encoder = &intel_encoder->base;
	connector = &intel_connector->base;
	drm_connector_init(dev, &intel_connector->base, &intel_lvds_connector_funcs,
			   DRM_MODE_CONNECTOR_LVDS);

	drm_encoder_init(dev, &intel_encoder->base, &intel_lvds_enc_funcs,
			 DRM_MODE_ENCODER_LVDS);

	intel_connector_attach_encoder(intel_connector, intel_encoder);
	intel_encoder->type = INTEL_OUTPUT_LVDS;

	intel_encoder->clone_mask = (1 << INTEL_LVDS_CLONE_BIT);
	if (HAS_PCH_SPLIT(dev))
		intel_encoder->crtc_mask = (1 << 0) | (1 << 1) | (1 << 2);
	else
		intel_encoder->crtc_mask = (1 << 1);

	drm_encoder_helper_add(encoder, &intel_lvds_helper_funcs);
	drm_connector_helper_add(connector, &intel_lvds_connector_helper_funcs);
	connector->display_info.subpixel_order = SubPixelHorizontalRGB;
	connector->interlace_allowed = false;
	connector->doublescan_allowed = false;

	
	drm_mode_create_scaling_mode_property(dev);

	drm_connector_attach_property(&intel_connector->base,
				      dev->mode_config.scaling_mode_property,
				      DRM_MODE_SCALE_ASPECT);
	intel_lvds->fitting_mode = DRM_MODE_SCALE_ASPECT;

	intel_lvds->edid = drm_get_edid(connector,
					&dev_priv->gmbus[pin].adapter);
	if (intel_lvds->edid) {
		if (drm_add_edid_modes(connector,
				       intel_lvds->edid)) {
			drm_mode_connector_update_edid_property(connector,
								intel_lvds->edid);
		} else {
			kfree(intel_lvds->edid);
			intel_lvds->edid = NULL;
		}
	}
	if (!intel_lvds->edid) {
		connector->display_info.min_vfreq = 0;
		connector->display_info.max_vfreq = 200;
		connector->display_info.min_hfreq = 0;
		connector->display_info.max_hfreq = 200;
	}

	list_for_each_entry(scan, &connector->probed_modes, head) {
		if (scan->type & DRM_MODE_TYPE_PREFERRED) {
			intel_lvds->fixed_mode =
				drm_mode_duplicate(dev, scan);
			intel_find_lvds_downclock(dev,
						  intel_lvds->fixed_mode,
						  connector);
			goto out;
		}
	}

	
	if (dev_priv->lfp_lvds_vbt_mode) {
		intel_lvds->fixed_mode =
			drm_mode_duplicate(dev, dev_priv->lfp_lvds_vbt_mode);
		if (intel_lvds->fixed_mode) {
			intel_lvds->fixed_mode->type |=
				DRM_MODE_TYPE_PREFERRED;
			goto out;
		}
	}


	
	if (HAS_PCH_SPLIT(dev))
		goto failed;

	lvds = I915_READ(LVDS);
	pipe = (lvds & LVDS_PIPEB_SELECT) ? 1 : 0;
	crtc = intel_get_crtc_for_pipe(dev, pipe);

	if (crtc && (lvds & LVDS_PORT_EN)) {
		intel_lvds->fixed_mode = intel_crtc_mode_get(dev, crtc);
		if (intel_lvds->fixed_mode) {
			intel_lvds->fixed_mode->type |=
				DRM_MODE_TYPE_PREFERRED;
			goto out;
		}
	}

	
	if (!intel_lvds->fixed_mode)
		goto failed;

out:
	if (HAS_PCH_SPLIT(dev)) {
		u32 pwm;

		pipe = (I915_READ(PCH_LVDS) & LVDS_PIPEB_SELECT) ? 1 : 0;

		
		pwm = I915_READ(BLC_PWM_CPU_CTL2);
		if (pipe == 0 && (pwm & PWM_PIPE_B))
			I915_WRITE(BLC_PWM_CPU_CTL2, pwm & ~PWM_ENABLE);
		if (pipe)
			pwm |= PWM_PIPE_B;
		else
			pwm &= ~PWM_PIPE_B;
		I915_WRITE(BLC_PWM_CPU_CTL2, pwm | PWM_ENABLE);

		pwm = I915_READ(BLC_PWM_PCH_CTL1);
		pwm |= PWM_PCH_ENABLE;
		I915_WRITE(BLC_PWM_PCH_CTL1, pwm);
		I915_WRITE(PCH_PP_CONTROL,
			   I915_READ(PCH_PP_CONTROL) | PANEL_UNLOCK_REGS);
	} else {
		I915_WRITE(PP_CONTROL,
			   I915_READ(PP_CONTROL) | PANEL_UNLOCK_REGS);
	}
	dev_priv->lid_notifier.notifier_call = intel_lid_notify;
	if (acpi_lid_notifier_register(&dev_priv->lid_notifier)) {
		DRM_DEBUG_KMS("lid notifier registration failed\n");
		dev_priv->lid_notifier.notifier_call = NULL;
	}
	
	dev_priv->int_lvds_connector = connector;
	drm_sysfs_connector_add(connector);

	intel_panel_setup_backlight(dev);

	return true;

failed:
	DRM_DEBUG_KMS("No LVDS modes found, disabling.\n");
	drm_connector_cleanup(connector);
	drm_encoder_cleanup(encoder);
	kfree(intel_lvds);
	kfree(intel_connector);
	return false;
}
