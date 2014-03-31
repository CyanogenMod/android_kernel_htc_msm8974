/*
 * Copyright © 2006-2011 Intel Corporation
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
 * Authors:
 *	Eric Anholt <eric@anholt.net>
 *	Dave Airlie <airlied@linux.ie>
 *	Jesse Barnes <jesse.barnes@intel.com>
 */

#include <linux/i2c.h>
#include <linux/dmi.h>
#include <drm/drmP.h>

#include "intel_bios.h"
#include "psb_drv.h"
#include "psb_intel_drv.h"
#include "psb_intel_reg.h"
#include "power.h"
#include <linux/pm_runtime.h>
#include "cdv_device.h"

#define BRIGHTNESS_MAX_LEVEL 100
#define BRIGHTNESS_MASK 0xFF
#define BLC_I2C_TYPE	0x01
#define BLC_PWM_TYPT	0x02

#define BLC_POLARITY_NORMAL 0
#define BLC_POLARITY_INVERSE 1

#define PSB_BLC_MAX_PWM_REG_FREQ       (0xFFFE)
#define PSB_BLC_MIN_PWM_REG_FREQ	(0x2)
#define PSB_BLC_PWM_PRECISION_FACTOR	(10)
#define PSB_BACKLIGHT_PWM_CTL_SHIFT	(16)
#define PSB_BACKLIGHT_PWM_POLARITY_BIT_CLEAR (0xFFFE)

struct cdv_intel_lvds_priv {
	uint32_t savePP_ON;
	uint32_t savePP_OFF;
	uint32_t saveLVDS;
	uint32_t savePP_CONTROL;
	uint32_t savePP_CYCLE;
	uint32_t savePFIT_CONTROL;
	uint32_t savePFIT_PGM_RATIOS;
	uint32_t saveBLC_PWM_CTL;
};

static u32 cdv_intel_lvds_get_max_backlight(struct drm_device *dev)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	u32 retval;

	if (gma_power_begin(dev, false)) {
		retval = ((REG_READ(BLC_PWM_CTL) &
			  BACKLIGHT_MODULATION_FREQ_MASK) >>
			  BACKLIGHT_MODULATION_FREQ_SHIFT) * 2;

		gma_power_end(dev);
	} else
		retval = ((dev_priv->regs.saveBLC_PWM_CTL &
			  BACKLIGHT_MODULATION_FREQ_MASK) >>
			  BACKLIGHT_MODULATION_FREQ_SHIFT) * 2;

	return retval;
}

#if 0
static int cdv_lvds_i2c_set_brightness(struct drm_device *dev,
					unsigned int level)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_intel_i2c_chan *lvds_i2c_bus = dev_priv->lvds_i2c_bus;
	u8 out_buf[2];
	unsigned int blc_i2c_brightness;

	struct i2c_msg msgs[] = {
		{
			.addr = lvds_i2c_bus->slave_addr,
			.flags = 0,
			.len = 2,
			.buf = out_buf,
		}
	};

	blc_i2c_brightness = BRIGHTNESS_MASK & ((unsigned int)level *
			     BRIGHTNESS_MASK /
			     BRIGHTNESS_MAX_LEVEL);

	if (dev_priv->lvds_bl->pol == BLC_POLARITY_INVERSE)
		blc_i2c_brightness = BRIGHTNESS_MASK - blc_i2c_brightness;

	out_buf[0] = dev_priv->lvds_bl->brightnesscmd;
	out_buf[1] = (u8)blc_i2c_brightness;

	if (i2c_transfer(&lvds_i2c_bus->adapter, msgs, 1) == 1)
		return 0;

	DRM_ERROR("I2C transfer error\n");
	return -1;
}


static int cdv_lvds_pwm_set_brightness(struct drm_device *dev, int level)
{
	struct drm_psb_private *dev_priv = dev->dev_private;

	u32 max_pwm_blc;
	u32 blc_pwm_duty_cycle;

	max_pwm_blc = cdv_intel_lvds_get_max_backlight(dev);

	
	BUG_ON((max_pwm_blc & PSB_BLC_MAX_PWM_REG_FREQ) == 0);

	blc_pwm_duty_cycle = level * max_pwm_blc / BRIGHTNESS_MAX_LEVEL;

	if (dev_priv->lvds_bl->pol == BLC_POLARITY_INVERSE)
		blc_pwm_duty_cycle = max_pwm_blc - blc_pwm_duty_cycle;

	blc_pwm_duty_cycle &= PSB_BACKLIGHT_PWM_POLARITY_BIT_CLEAR;
	REG_WRITE(BLC_PWM_CTL,
		  (max_pwm_blc << PSB_BACKLIGHT_PWM_CTL_SHIFT) |
		  (blc_pwm_duty_cycle));

	return 0;
}

void cdv_intel_lvds_set_brightness(struct drm_device *dev, int level)
{
	struct drm_psb_private *dev_priv = dev->dev_private;

	if (!dev_priv->lvds_bl) {
		DRM_ERROR("NO LVDS Backlight Info\n");
		return;
	}

	if (dev_priv->lvds_bl->type == BLC_I2C_TYPE)
		cdv_lvds_i2c_set_brightness(dev, level);
	else
		cdv_lvds_pwm_set_brightness(dev, level);
}
#endif

static void cdv_intel_lvds_set_backlight(struct drm_device *dev, int level)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	u32 blc_pwm_ctl;

	if (gma_power_begin(dev, false)) {
		blc_pwm_ctl =
			REG_READ(BLC_PWM_CTL) & ~BACKLIGHT_DUTY_CYCLE_MASK;
		REG_WRITE(BLC_PWM_CTL,
				(blc_pwm_ctl |
				(level << BACKLIGHT_DUTY_CYCLE_SHIFT)));
		gma_power_end(dev);
	} else {
		blc_pwm_ctl = dev_priv->regs.saveBLC_PWM_CTL &
				~BACKLIGHT_DUTY_CYCLE_MASK;
		dev_priv->regs.saveBLC_PWM_CTL = (blc_pwm_ctl |
					(level << BACKLIGHT_DUTY_CYCLE_SHIFT));
	}
}

static void cdv_intel_lvds_set_power(struct drm_device *dev,
				     struct drm_encoder *encoder, bool on)
{
	struct drm_psb_private *dev_priv = dev->dev_private;
	u32 pp_status;

	if (!gma_power_begin(dev, true))
		return;

	if (on) {
		REG_WRITE(PP_CONTROL, REG_READ(PP_CONTROL) |
			  POWER_TARGET_ON);
		do {
			pp_status = REG_READ(PP_STATUS);
		} while ((pp_status & PP_ON) == 0);

		cdv_intel_lvds_set_backlight(dev,
				dev_priv->mode_dev.backlight_duty_cycle);
	} else {
		cdv_intel_lvds_set_backlight(dev, 0);

		REG_WRITE(PP_CONTROL, REG_READ(PP_CONTROL) &
			  ~POWER_TARGET_ON);
		do {
			pp_status = REG_READ(PP_STATUS);
		} while (pp_status & PP_ON);
	}
	gma_power_end(dev);
}

static void cdv_intel_lvds_encoder_dpms(struct drm_encoder *encoder, int mode)
{
	struct drm_device *dev = encoder->dev;
	if (mode == DRM_MODE_DPMS_ON)
		cdv_intel_lvds_set_power(dev, encoder, true);
	else
		cdv_intel_lvds_set_power(dev, encoder, false);
	
}

static void cdv_intel_lvds_save(struct drm_connector *connector)
{
}

static void cdv_intel_lvds_restore(struct drm_connector *connector)
{
}

static int cdv_intel_lvds_mode_valid(struct drm_connector *connector,
			      struct drm_display_mode *mode)
{
	struct drm_device *dev = connector->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct drm_display_mode *fixed_mode =
					dev_priv->mode_dev.panel_fixed_mode;

	
	if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
		return MODE_NO_DBLESCAN;

	
	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		return MODE_NO_INTERLACE;

	if (fixed_mode) {
		if (mode->hdisplay > fixed_mode->hdisplay)
			return MODE_PANEL;
		if (mode->vdisplay > fixed_mode->vdisplay)
			return MODE_PANEL;
	}
	return MODE_OK;
}

static bool cdv_intel_lvds_mode_fixup(struct drm_encoder *encoder,
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_intel_mode_device *mode_dev = &dev_priv->mode_dev;
	struct drm_encoder *tmp_encoder;
	struct drm_display_mode *panel_fixed_mode = mode_dev->panel_fixed_mode;

	
	list_for_each_entry(tmp_encoder, &dev->mode_config.encoder_list,
			    head) {
		if (tmp_encoder != encoder
		    && tmp_encoder->crtc == encoder->crtc) {
			printk(KERN_ERR "Can't enable LVDS and another "
			       "encoder on the same pipe\n");
			return false;
		}
	}

	if (panel_fixed_mode != NULL) {
		adjusted_mode->hdisplay = panel_fixed_mode->hdisplay;
		adjusted_mode->hsync_start = panel_fixed_mode->hsync_start;
		adjusted_mode->hsync_end = panel_fixed_mode->hsync_end;
		adjusted_mode->htotal = panel_fixed_mode->htotal;
		adjusted_mode->vdisplay = panel_fixed_mode->vdisplay;
		adjusted_mode->vsync_start = panel_fixed_mode->vsync_start;
		adjusted_mode->vsync_end = panel_fixed_mode->vsync_end;
		adjusted_mode->vtotal = panel_fixed_mode->vtotal;
		adjusted_mode->clock = panel_fixed_mode->clock;
		drm_mode_set_crtcinfo(adjusted_mode,
				      CRTC_INTERLACE_HALVE_V);
	}


	return true;
}

static void cdv_intel_lvds_prepare(struct drm_encoder *encoder)
{
	struct drm_device *dev = encoder->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_intel_mode_device *mode_dev = &dev_priv->mode_dev;

	if (!gma_power_begin(dev, true))
		return;

	mode_dev->saveBLC_PWM_CTL = REG_READ(BLC_PWM_CTL);
	mode_dev->backlight_duty_cycle = (mode_dev->saveBLC_PWM_CTL &
					  BACKLIGHT_DUTY_CYCLE_MASK);

	cdv_intel_lvds_set_power(dev, encoder, false);

	gma_power_end(dev);
}

static void cdv_intel_lvds_commit(struct drm_encoder *encoder)
{
	struct drm_device *dev = encoder->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_intel_mode_device *mode_dev = &dev_priv->mode_dev;

	if (mode_dev->backlight_duty_cycle == 0)
		mode_dev->backlight_duty_cycle =
		    cdv_intel_lvds_get_max_backlight(dev);

	cdv_intel_lvds_set_power(dev, encoder, true);
}

static void cdv_intel_lvds_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	u32 pfit_control;


	if (mode->hdisplay != adjusted_mode->hdisplay ||
	    mode->vdisplay != adjusted_mode->vdisplay)
		pfit_control = (PFIT_ENABLE | VERT_AUTO_SCALE |
				HORIZ_AUTO_SCALE | VERT_INTERP_BILINEAR |
				HORIZ_INTERP_BILINEAR);
	else
		pfit_control = 0;

	if (dev_priv->lvds_dither)
		pfit_control |= PANEL_8TO6_DITHER_ENABLE;

	REG_WRITE(PFIT_CONTROL, pfit_control);
}

static enum drm_connector_status cdv_intel_lvds_detect(
				struct drm_connector *connector, bool force)
{
	return connector_status_connected;
}

static int cdv_intel_lvds_get_modes(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_psb_private *dev_priv = dev->dev_private;
	struct psb_intel_encoder *psb_intel_encoder =
					psb_intel_attached_encoder(connector);
	struct psb_intel_mode_device *mode_dev = &dev_priv->mode_dev;
	int ret;

	ret = psb_intel_ddc_get_modes(connector, &psb_intel_encoder->i2c_bus->adapter);

	if (ret)
		return ret;

	connector->display_info.min_vfreq = 0;
	connector->display_info.max_vfreq = 200;
	connector->display_info.min_hfreq = 0;
	connector->display_info.max_hfreq = 200;
	if (mode_dev->panel_fixed_mode != NULL) {
		struct drm_display_mode *mode =
		    drm_mode_duplicate(dev, mode_dev->panel_fixed_mode);
		drm_mode_probed_add(connector, mode);
		return 1;
	}

	return 0;
}

static void cdv_intel_lvds_destroy(struct drm_connector *connector)
{
	struct psb_intel_encoder *psb_intel_encoder =
					psb_intel_attached_encoder(connector);

	if (psb_intel_encoder->i2c_bus)
		psb_intel_i2c_destroy(psb_intel_encoder->i2c_bus);
	drm_sysfs_connector_remove(connector);
	drm_connector_cleanup(connector);
	kfree(connector);
}

static int cdv_intel_lvds_set_property(struct drm_connector *connector,
				       struct drm_property *property,
				       uint64_t value)
{
	struct drm_encoder *encoder = connector->encoder;

	if (!strcmp(property->name, "scaling mode") && encoder) {
		struct psb_intel_crtc *crtc =
					to_psb_intel_crtc(encoder->crtc);
		uint64_t curValue;

		if (!crtc)
			return -1;

		switch (value) {
		case DRM_MODE_SCALE_FULLSCREEN:
			break;
		case DRM_MODE_SCALE_NO_SCALE:
			break;
		case DRM_MODE_SCALE_ASPECT:
			break;
		default:
			return -1;
		}

		if (drm_connector_property_get_value(connector,
						     property,
						     &curValue))
			return -1;

		if (curValue == value)
			return 0;

		if (drm_connector_property_set_value(connector,
							property,
							value))
			return -1;

		if (crtc->saved_mode.hdisplay != 0 &&
		    crtc->saved_mode.vdisplay != 0) {
			if (!drm_crtc_helper_set_mode(encoder->crtc,
						      &crtc->saved_mode,
						      encoder->crtc->x,
						      encoder->crtc->y,
						      encoder->crtc->fb))
				return -1;
		}
	} else if (!strcmp(property->name, "backlight") && encoder) {
		if (drm_connector_property_set_value(connector,
							property,
							value))
			return -1;
		else {
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
			struct drm_psb_private *dev_priv =
						encoder->dev->dev_private;
			struct backlight_device *bd =
						dev_priv->backlight_device;
			bd->props.brightness = value;
			backlight_update_status(bd);
#endif
		}
	} else if (!strcmp(property->name, "DPMS") && encoder) {
		struct drm_encoder_helper_funcs *helpers =
					encoder->helper_private;
		helpers->dpms(encoder, value);
	}
	return 0;
}

static const struct drm_encoder_helper_funcs
					cdv_intel_lvds_helper_funcs = {
	.dpms = cdv_intel_lvds_encoder_dpms,
	.mode_fixup = cdv_intel_lvds_mode_fixup,
	.prepare = cdv_intel_lvds_prepare,
	.mode_set = cdv_intel_lvds_mode_set,
	.commit = cdv_intel_lvds_commit,
};

static const struct drm_connector_helper_funcs
				cdv_intel_lvds_connector_helper_funcs = {
	.get_modes = cdv_intel_lvds_get_modes,
	.mode_valid = cdv_intel_lvds_mode_valid,
	.best_encoder = psb_intel_best_encoder,
};

static const struct drm_connector_funcs cdv_intel_lvds_connector_funcs = {
	.dpms = drm_helper_connector_dpms,
	.save = cdv_intel_lvds_save,
	.restore = cdv_intel_lvds_restore,
	.detect = cdv_intel_lvds_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.set_property = cdv_intel_lvds_set_property,
	.destroy = cdv_intel_lvds_destroy,
};


static void cdv_intel_lvds_enc_destroy(struct drm_encoder *encoder)
{
	drm_encoder_cleanup(encoder);
}

const struct drm_encoder_funcs cdv_intel_lvds_enc_funcs = {
	.destroy = cdv_intel_lvds_enc_destroy,
};

void cdv_intel_lvds_init(struct drm_device *dev,
		     struct psb_intel_mode_device *mode_dev)
{
	struct psb_intel_encoder *psb_intel_encoder;
	struct psb_intel_connector *psb_intel_connector;
	struct cdv_intel_lvds_priv *lvds_priv;
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	struct drm_display_mode *scan;
	struct drm_crtc *crtc;
	struct drm_psb_private *dev_priv = dev->dev_private;
	u32 lvds;
	int pipe;

	psb_intel_encoder = kzalloc(sizeof(struct psb_intel_encoder),
				    GFP_KERNEL);
	if (!psb_intel_encoder)
		return;

	psb_intel_connector = kzalloc(sizeof(struct psb_intel_connector),
				      GFP_KERNEL);
	if (!psb_intel_connector)
		goto failed_connector;

	lvds_priv = kzalloc(sizeof(struct cdv_intel_lvds_priv), GFP_KERNEL);
	if (!lvds_priv)
		goto failed_lvds_priv;

	psb_intel_encoder->dev_priv = lvds_priv;

	connector = &psb_intel_connector->base;
	encoder = &psb_intel_encoder->base;


	drm_connector_init(dev, connector,
			   &cdv_intel_lvds_connector_funcs,
			   DRM_MODE_CONNECTOR_LVDS);

	drm_encoder_init(dev, encoder,
			 &cdv_intel_lvds_enc_funcs,
			 DRM_MODE_ENCODER_LVDS);


	psb_intel_connector_attach_encoder(psb_intel_connector,
					   psb_intel_encoder);
	psb_intel_encoder->type = INTEL_OUTPUT_LVDS;

	drm_encoder_helper_add(encoder, &cdv_intel_lvds_helper_funcs);
	drm_connector_helper_add(connector,
				 &cdv_intel_lvds_connector_helper_funcs);
	connector->display_info.subpixel_order = SubPixelHorizontalRGB;
	connector->interlace_allowed = false;
	connector->doublescan_allowed = false;

	
	drm_connector_attach_property(connector,
				      dev->mode_config.scaling_mode_property,
				      DRM_MODE_SCALE_FULLSCREEN);
	drm_connector_attach_property(connector,
				      dev_priv->backlight_property,
				      BRIGHTNESS_MAX_LEVEL);

	psb_intel_encoder->i2c_bus = psb_intel_i2c_create(dev,
							 GPIOB,
							 "LVDSBLC_B");
	if (!psb_intel_encoder->i2c_bus) {
		dev_printk(KERN_ERR,
			&dev->pdev->dev, "I2C bus registration failed.\n");
		goto failed_blc_i2c;
	}
	psb_intel_encoder->i2c_bus->slave_addr = 0x2C;
	dev_priv->lvds_i2c_bus = psb_intel_encoder->i2c_bus;


	
	psb_intel_encoder->ddc_bus = psb_intel_i2c_create(dev,
							 GPIOC,
							 "LVDSDDC_C");
	if (!psb_intel_encoder->ddc_bus) {
		dev_printk(KERN_ERR, &dev->pdev->dev,
			   "DDC bus registration " "failed.\n");
		goto failed_ddc;
	}

	psb_intel_ddc_get_modes(connector,
				&psb_intel_encoder->ddc_bus->adapter);
	list_for_each_entry(scan, &connector->probed_modes, head) {
		if (scan->type & DRM_MODE_TYPE_PREFERRED) {
			mode_dev->panel_fixed_mode =
			    drm_mode_duplicate(dev, scan);
			goto out;	
		}
	}

	
	if (dev_priv->lfp_lvds_vbt_mode) {
		mode_dev->panel_fixed_mode =
			drm_mode_duplicate(dev, dev_priv->lfp_lvds_vbt_mode);
		if (mode_dev->panel_fixed_mode) {
			mode_dev->panel_fixed_mode->type |=
				DRM_MODE_TYPE_PREFERRED;
			goto out;	
		}
	}
	lvds = REG_READ(LVDS);
	pipe = (lvds & LVDS_PIPEB_SELECT) ? 1 : 0;
	crtc = psb_intel_get_crtc_from_pipe(dev, pipe);

	if (crtc && (lvds & LVDS_PORT_EN)) {
		mode_dev->panel_fixed_mode =
		    cdv_intel_crtc_mode_get(dev, crtc);
		if (mode_dev->panel_fixed_mode) {
			mode_dev->panel_fixed_mode->type |=
			    DRM_MODE_TYPE_PREFERRED;
			goto out;	
		}
	}

	
	if (!mode_dev->panel_fixed_mode) {
		DRM_DEBUG
			("Found no modes on the lvds, ignoring the LVDS\n");
		goto failed_find;
	}

out:
	drm_sysfs_connector_add(connector);
	return;

failed_find:
	printk(KERN_ERR "Failed find\n");
	if (psb_intel_encoder->ddc_bus)
		psb_intel_i2c_destroy(psb_intel_encoder->ddc_bus);
failed_ddc:
	printk(KERN_ERR "Failed DDC\n");
	if (psb_intel_encoder->i2c_bus)
		psb_intel_i2c_destroy(psb_intel_encoder->i2c_bus);
failed_blc_i2c:
	printk(KERN_ERR "Failed BLC\n");
	drm_encoder_cleanup(encoder);
	drm_connector_cleanup(connector);
	kfree(lvds_priv);
failed_lvds_priv:
	kfree(psb_intel_connector);
failed_connector:
	kfree(psb_intel_encoder);
}
