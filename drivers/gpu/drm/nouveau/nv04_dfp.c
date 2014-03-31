/*
 * Copyright 2003 NVIDIA, Corporation
 * Copyright 2006 Dave Airlie
 * Copyright 2007 Maarten Maathuis
 * Copyright 2007-2009 Stuart Bennett
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
 */

#include "drmP.h"
#include "drm_crtc_helper.h"

#include "nouveau_drv.h"
#include "nouveau_encoder.h"
#include "nouveau_connector.h"
#include "nouveau_crtc.h"
#include "nouveau_hw.h"
#include "nvreg.h"

#include "i2c/sil164.h"

#define FP_TG_CONTROL_ON  (NV_PRAMDAC_FP_TG_CONTROL_DISPEN_POS |	\
			   NV_PRAMDAC_FP_TG_CONTROL_HSYNC_POS |		\
			   NV_PRAMDAC_FP_TG_CONTROL_VSYNC_POS)
#define FP_TG_CONTROL_OFF (NV_PRAMDAC_FP_TG_CONTROL_DISPEN_DISABLE |	\
			   NV_PRAMDAC_FP_TG_CONTROL_HSYNC_DISABLE |	\
			   NV_PRAMDAC_FP_TG_CONTROL_VSYNC_DISABLE)

static inline bool is_fpc_off(uint32_t fpc)
{
	return ((fpc & (FP_TG_CONTROL_ON | FP_TG_CONTROL_OFF)) ==
			FP_TG_CONTROL_OFF);
}

int nv04_dfp_get_bound_head(struct drm_device *dev, struct dcb_entry *dcbent)
{
	int ramdac = (dcbent->or & OUTPUT_C) >> 2;

	NVWriteRAMDAC(dev, ramdac, NV_PRAMDAC_FP_TMDS_CONTROL,
	NV_PRAMDAC_FP_TMDS_CONTROL_WRITE_DISABLE | 0x4);
	return ((NVReadRAMDAC(dev, ramdac, NV_PRAMDAC_FP_TMDS_DATA) & 0x8) >> 3) ^ ramdac;
}

void nv04_dfp_bind_head(struct drm_device *dev, struct dcb_entry *dcbent,
			int head, bool dl)
{

	int ramdac = (dcbent->or & OUTPUT_C) >> 2;
	uint8_t tmds04 = 0x80;

	if (head != ramdac)
		tmds04 = 0x88;

	if (dcbent->type == OUTPUT_LVDS)
		tmds04 |= 0x01;

	nv_write_tmds(dev, dcbent->or, 0, 0x04, tmds04);

	if (dl)	
		nv_write_tmds(dev, dcbent->or, 1, 0x04, tmds04 ^ 0x08);
}

void nv04_dfp_disable(struct drm_device *dev, int head)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nv04_crtc_reg *crtcstate = dev_priv->mode_reg.crtc_reg;

	if (NVReadRAMDAC(dev, head, NV_PRAMDAC_FP_TG_CONTROL) &
	    FP_TG_CONTROL_ON) {
		NVWriteRAMDAC(dev, head, NV_PRAMDAC_FP_TG_CONTROL,
			      FP_TG_CONTROL_OFF);
		msleep(50);
	}
	/* don't inadvertently turn it on when state written later */
	crtcstate[head].fp_control = FP_TG_CONTROL_OFF;
	crtcstate[head].CRTC[NV_CIO_CRE_LCD__INDEX] &=
		~NV_CIO_CRE_LCD_ROUTE_MASK;
}

void nv04_dfp_update_fp_control(struct drm_encoder *encoder, int mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct drm_crtc *crtc;
	struct nouveau_crtc *nv_crtc;
	uint32_t *fpc;

	if (mode == DRM_MODE_DPMS_ON) {
		nv_crtc = nouveau_crtc(encoder->crtc);
		fpc = &dev_priv->mode_reg.crtc_reg[nv_crtc->index].fp_control;

		if (is_fpc_off(*fpc)) {
			*fpc = nv_crtc->dpms_saved_fp_control;
		}

		nv_crtc->fp_users |= 1 << nouveau_encoder(encoder)->dcb->index;
		NVWriteRAMDAC(dev, nv_crtc->index, NV_PRAMDAC_FP_TG_CONTROL, *fpc);
	} else {
		list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
			nv_crtc = nouveau_crtc(crtc);
			fpc = &dev_priv->mode_reg.crtc_reg[nv_crtc->index].fp_control;

			nv_crtc->fp_users &= ~(1 << nouveau_encoder(encoder)->dcb->index);
			if (!is_fpc_off(*fpc) && !nv_crtc->fp_users) {
				nv_crtc->dpms_saved_fp_control = *fpc;
				
				*fpc &= ~FP_TG_CONTROL_ON;
				*fpc |= FP_TG_CONTROL_OFF;
				NVWriteRAMDAC(dev, nv_crtc->index,
					      NV_PRAMDAC_FP_TG_CONTROL, *fpc);
			}
		}
	}
}

static struct drm_encoder *get_tmds_slave(struct drm_encoder *encoder)
{
	struct drm_device *dev = encoder->dev;
	struct dcb_entry *dcb = nouveau_encoder(encoder)->dcb;
	struct drm_encoder *slave;

	if (dcb->type != OUTPUT_TMDS || dcb->location == DCB_LOC_ON_CHIP)
		return NULL;

	list_for_each_entry(slave, &dev->mode_config.encoder_list, head) {
		struct dcb_entry *slave_dcb = nouveau_encoder(slave)->dcb;

		if (slave_dcb->type == OUTPUT_TMDS && get_slave_funcs(slave) &&
		    slave_dcb->tmdsconf.slave_addr == dcb->tmdsconf.slave_addr)
			return slave;
	}

	return NULL;
}

static bool nv04_dfp_mode_fixup(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct nouveau_encoder *nv_encoder = nouveau_encoder(encoder);
	struct nouveau_connector *nv_connector = nouveau_encoder_connector_get(nv_encoder);

	if (!nv_connector->native_mode ||
	    nv_connector->scaling_mode == DRM_MODE_SCALE_NONE ||
	    mode->hdisplay > nv_connector->native_mode->hdisplay ||
	    mode->vdisplay > nv_connector->native_mode->vdisplay) {
		nv_encoder->mode = *adjusted_mode;

	} else {
		nv_encoder->mode = *nv_connector->native_mode;
		adjusted_mode->clock = nv_connector->native_mode->clock;
	}

	return true;
}

static void nv04_dfp_prepare_sel_clk(struct drm_device *dev,
				     struct nouveau_encoder *nv_encoder, int head)
{
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nv04_mode_state *state = &dev_priv->mode_reg;
	uint32_t bits1618 = nv_encoder->dcb->or & OUTPUT_A ? 0x10000 : 0x40000;

	if (nv_encoder->dcb->location != DCB_LOC_ON_CHIP)
		return;

	if (head)
		state->sel_clk |= bits1618;
	else
		state->sel_clk &= ~bits1618;

	if (nv_encoder->dcb->type == OUTPUT_LVDS && dev_priv->saved_reg.sel_clk & 0xf0) {
		int shift = (dev_priv->saved_reg.sel_clk & 0x50) ? 0 : 1;

		state->sel_clk &= ~0xf0;
		state->sel_clk |= (head ? 0x40 : 0x10) << shift;
	}
}

static void nv04_dfp_prepare(struct drm_encoder *encoder)
{
	struct nouveau_encoder *nv_encoder = nouveau_encoder(encoder);
	struct drm_encoder_helper_funcs *helper = encoder->helper_private;
	struct drm_device *dev = encoder->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	int head = nouveau_crtc(encoder->crtc)->index;
	struct nv04_crtc_reg *crtcstate = dev_priv->mode_reg.crtc_reg;
	uint8_t *cr_lcd = &crtcstate[head].CRTC[NV_CIO_CRE_LCD__INDEX];
	uint8_t *cr_lcd_oth = &crtcstate[head ^ 1].CRTC[NV_CIO_CRE_LCD__INDEX];

	helper->dpms(encoder, DRM_MODE_DPMS_OFF);

	nv04_dfp_prepare_sel_clk(dev, nv_encoder, head);

	*cr_lcd = (*cr_lcd & ~NV_CIO_CRE_LCD_ROUTE_MASK) | 0x3;

	if (nv_two_heads(dev)) {
		if (nv_encoder->dcb->location == DCB_LOC_ON_CHIP)
			*cr_lcd |= head ? 0x0 : 0x8;
		else {
			*cr_lcd |= (nv_encoder->dcb->or << 4) & 0x30;
			if (nv_encoder->dcb->type == OUTPUT_LVDS)
				*cr_lcd |= 0x30;
			if ((*cr_lcd & 0x30) == (*cr_lcd_oth & 0x30)) {
				
				*cr_lcd_oth &= ~0x30;
				NVWriteVgaCrtc(dev, head ^ 1,
					       NV_CIO_CRE_LCD__INDEX,
					       *cr_lcd_oth);
			}
		}
	}
}


static void nv04_dfp_mode_set(struct drm_encoder *encoder,
			      struct drm_display_mode *mode,
			      struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nouveau_crtc *nv_crtc = nouveau_crtc(encoder->crtc);
	struct nv04_crtc_reg *regp = &dev_priv->mode_reg.crtc_reg[nv_crtc->index];
	struct nv04_crtc_reg *savep = &dev_priv->saved_reg.crtc_reg[nv_crtc->index];
	struct nouveau_connector *nv_connector = nouveau_crtc_connector_get(nv_crtc);
	struct nouveau_encoder *nv_encoder = nouveau_encoder(encoder);
	struct drm_display_mode *output_mode = &nv_encoder->mode;
	struct drm_connector *connector = &nv_connector->base;
	uint32_t mode_ratio, panel_ratio;

	NV_DEBUG_KMS(dev, "Output mode on CRTC %d:\n", nv_crtc->index);
	drm_mode_debug_printmodeline(output_mode);

	
	regp->fp_horiz_regs[FP_DISPLAY_END] = output_mode->hdisplay - 1;
	regp->fp_horiz_regs[FP_TOTAL] = output_mode->htotal - 1;
	if (!nv_gf4_disp_arch(dev) ||
	    (output_mode->hsync_start - output_mode->hdisplay) >=
					dev_priv->vbios.digital_min_front_porch)
		regp->fp_horiz_regs[FP_CRTC] = output_mode->hdisplay;
	else
		regp->fp_horiz_regs[FP_CRTC] = output_mode->hsync_start - dev_priv->vbios.digital_min_front_porch - 1;
	regp->fp_horiz_regs[FP_SYNC_START] = output_mode->hsync_start - 1;
	regp->fp_horiz_regs[FP_SYNC_END] = output_mode->hsync_end - 1;
	regp->fp_horiz_regs[FP_VALID_START] = output_mode->hskew;
	regp->fp_horiz_regs[FP_VALID_END] = output_mode->hdisplay - 1;

	regp->fp_vert_regs[FP_DISPLAY_END] = output_mode->vdisplay - 1;
	regp->fp_vert_regs[FP_TOTAL] = output_mode->vtotal - 1;
	regp->fp_vert_regs[FP_CRTC] = output_mode->vtotal - 5 - 1;
	regp->fp_vert_regs[FP_SYNC_START] = output_mode->vsync_start - 1;
	regp->fp_vert_regs[FP_SYNC_END] = output_mode->vsync_end - 1;
	regp->fp_vert_regs[FP_VALID_START] = 0;
	regp->fp_vert_regs[FP_VALID_END] = output_mode->vdisplay - 1;

	
	regp->fp_control = NV_PRAMDAC_FP_TG_CONTROL_DISPEN_POS |
			   (savep->fp_control & (1 << 26 | NV_PRAMDAC_FP_TG_CONTROL_READ_PROG));
	
	
	if (output_mode->flags & DRM_MODE_FLAG_PVSYNC)
		regp->fp_control |= NV_PRAMDAC_FP_TG_CONTROL_VSYNC_POS;
	if (output_mode->flags & DRM_MODE_FLAG_PHSYNC)
		regp->fp_control |= NV_PRAMDAC_FP_TG_CONTROL_HSYNC_POS;
	
	if (nv_connector->scaling_mode == DRM_MODE_SCALE_NONE ||
	    nv_connector->scaling_mode == DRM_MODE_SCALE_CENTER)	
		regp->fp_control |= NV_PRAMDAC_FP_TG_CONTROL_MODE_CENTER;
	else if (adjusted_mode->hdisplay == output_mode->hdisplay &&
		 adjusted_mode->vdisplay == output_mode->vdisplay) 
		regp->fp_control |= NV_PRAMDAC_FP_TG_CONTROL_MODE_NATIVE;
	else 
		regp->fp_control |= NV_PRAMDAC_FP_TG_CONTROL_MODE_SCALE;
	if (nvReadEXTDEV(dev, NV_PEXTDEV_BOOT_0) & NV_PEXTDEV_BOOT_0_STRAP_FP_IFACE_12BIT)
		regp->fp_control |= NV_PRAMDAC_FP_TG_CONTROL_WIDTH_12;
	if (nv_encoder->dcb->location != DCB_LOC_ON_CHIP &&
	    output_mode->clock > 165000)
		regp->fp_control |= (2 << 24);
	if (nv_encoder->dcb->type == OUTPUT_LVDS) {
		bool duallink = false, dummy;
		if (nv_connector->edid &&
		    nv_connector->type == DCB_CONNECTOR_LVDS_SPWG) {
			duallink = (((u8 *)nv_connector->edid)[121] == 2);
		} else {
			nouveau_bios_parse_lvds_table(dev, output_mode->clock,
						      &duallink, &dummy);
		}

		if (duallink)
			regp->fp_control |= (8 << 28);
	} else
	if (output_mode->clock > 165000)
		regp->fp_control |= (8 << 28);

	regp->fp_debug_0 = NV_PRAMDAC_FP_DEBUG_0_YWEIGHT_ROUND |
			   NV_PRAMDAC_FP_DEBUG_0_XWEIGHT_ROUND |
			   NV_PRAMDAC_FP_DEBUG_0_YINTERP_BILINEAR |
			   NV_PRAMDAC_FP_DEBUG_0_XINTERP_BILINEAR |
			   NV_RAMDAC_FP_DEBUG_0_TMDS_ENABLED |
			   NV_PRAMDAC_FP_DEBUG_0_YSCALE_ENABLE |
			   NV_PRAMDAC_FP_DEBUG_0_XSCALE_ENABLE;

	
	regp->fp_debug_1 = 0;
	
	regp->fp_debug_2 = 0;

	
	mode_ratio = (1 << 12) * adjusted_mode->hdisplay / adjusted_mode->vdisplay;
	panel_ratio = (1 << 12) * output_mode->hdisplay / output_mode->vdisplay;
	if (nv_connector->scaling_mode == DRM_MODE_SCALE_ASPECT &&
	    mode_ratio != panel_ratio) {
		uint32_t diff, scale;
		bool divide_by_2 = nv_gf4_disp_arch(dev);

		if (mode_ratio < panel_ratio) {

			scale = (1 << 12) * adjusted_mode->vdisplay / output_mode->vdisplay;
			regp->fp_debug_1 = NV_PRAMDAC_FP_DEBUG_1_XSCALE_TESTMODE_ENABLE |
					   XLATE(scale, divide_by_2, NV_PRAMDAC_FP_DEBUG_1_XSCALE_VALUE);

			
			diff = output_mode->hdisplay -
			       output_mode->vdisplay * mode_ratio / (1 << 12);
			regp->fp_horiz_regs[FP_VALID_START] += diff / 2;
			regp->fp_horiz_regs[FP_VALID_END] -= diff / 2;
		}

		if (mode_ratio > panel_ratio) {

			scale = (1 << 12) * adjusted_mode->hdisplay / output_mode->hdisplay;
			regp->fp_debug_1 = NV_PRAMDAC_FP_DEBUG_1_YSCALE_TESTMODE_ENABLE |
					   XLATE(scale, divide_by_2, NV_PRAMDAC_FP_DEBUG_1_YSCALE_VALUE);

			
			diff = output_mode->vdisplay -
			       (1 << 12) * output_mode->hdisplay / mode_ratio;
			regp->fp_vert_regs[FP_VALID_START] += diff / 2;
			regp->fp_vert_regs[FP_VALID_END] -= diff / 2;
		}
	}

	
	if ((nv_connector->dithering_mode == DITHERING_MODE_ON) ||
	    (nv_connector->dithering_mode == DITHERING_MODE_AUTO &&
	     encoder->crtc->fb->depth > connector->display_info.bpc * 3)) {
		if (dev_priv->chipset == 0x11)
			regp->dither = savep->dither | 0x00010000;
		else {
			int i;
			regp->dither = savep->dither | 0x00000001;
			for (i = 0; i < 3; i++) {
				regp->dither_regs[i] = 0xe4e4e4e4;
				regp->dither_regs[i + 3] = 0x44444444;
			}
		}
	} else {
		if (dev_priv->chipset != 0x11) {
			
			int i;
			for (i = 0; i < 3; i++) {
				regp->dither_regs[i] = savep->dither_regs[i];
				regp->dither_regs[i + 3] = savep->dither_regs[i + 3];
			}
		}
		regp->dither = savep->dither;
	}

	regp->fp_margin_color = 0;
}

static void nv04_dfp_commit(struct drm_encoder *encoder)
{
	struct drm_device *dev = encoder->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct drm_encoder_helper_funcs *helper = encoder->helper_private;
	struct nouveau_crtc *nv_crtc = nouveau_crtc(encoder->crtc);
	struct nouveau_encoder *nv_encoder = nouveau_encoder(encoder);
	struct dcb_entry *dcbe = nv_encoder->dcb;
	int head = nouveau_crtc(encoder->crtc)->index;
	struct drm_encoder *slave_encoder;

	if (dcbe->type == OUTPUT_TMDS)
		run_tmds_table(dev, dcbe, head, nv_encoder->mode.clock);
	else if (dcbe->type == OUTPUT_LVDS)
		call_lvds_script(dev, dcbe, head, LVDS_RESET, nv_encoder->mode.clock);

	/* update fp_control state for any changes made by scripts,
	 * so correct value is written at DPMS on */
	dev_priv->mode_reg.crtc_reg[head].fp_control =
		NVReadRAMDAC(dev, head, NV_PRAMDAC_FP_TG_CONTROL);

	
	if (dev_priv->chipset < 0x44)
		NVWriteRAMDAC(dev, 0, NV_PRAMDAC_TEST_CONTROL + nv04_dac_output_offset(encoder), 0xf0000000);
	else
		NVWriteRAMDAC(dev, 0, NV_PRAMDAC_TEST_CONTROL + nv04_dac_output_offset(encoder), 0x00100000);

	
	slave_encoder = get_tmds_slave(encoder);
	if (slave_encoder)
		get_slave_funcs(slave_encoder)->mode_set(
			slave_encoder, &nv_encoder->mode, &nv_encoder->mode);

	helper->dpms(encoder, DRM_MODE_DPMS_ON);

	NV_INFO(dev, "Output %s is running on CRTC %d using output %c\n",
		drm_get_connector_name(&nouveau_encoder_connector_get(nv_encoder)->base),
		nv_crtc->index, '@' + ffs(nv_encoder->dcb->or));
}

static void nv04_dfp_update_backlight(struct drm_encoder *encoder, int mode)
{
#ifdef __powerpc__
	struct drm_device *dev = encoder->dev;

	if (dev->pci_device == 0x0179 || dev->pci_device == 0x0189 ||
	    dev->pci_device == 0x0329) {
		if (mode == DRM_MODE_DPMS_ON) {
			nv_mask(dev, NV_PBUS_DEBUG_DUALHEAD_CTL, 0, 1 << 31);
			nv_mask(dev, NV_PCRTC_GPIO_EXT, 3, 1);
		} else {
			nv_mask(dev, NV_PBUS_DEBUG_DUALHEAD_CTL, 1 << 31, 0);
			nv_mask(dev, NV_PCRTC_GPIO_EXT, 3, 0);
		}
	}
#endif
}

static inline bool is_powersaving_dpms(int mode)
{
	return (mode != DRM_MODE_DPMS_ON);
}

static void nv04_lvds_dpms(struct drm_encoder *encoder, int mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_crtc *crtc = encoder->crtc;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	struct nouveau_encoder *nv_encoder = nouveau_encoder(encoder);
	bool was_powersaving = is_powersaving_dpms(nv_encoder->last_dpms);

	if (nv_encoder->last_dpms == mode)
		return;
	nv_encoder->last_dpms = mode;

	NV_INFO(dev, "Setting dpms mode %d on lvds encoder (output %d)\n",
		     mode, nv_encoder->dcb->index);

	if (was_powersaving && is_powersaving_dpms(mode))
		return;

	if (nv_encoder->dcb->lvdsconf.use_power_scripts) {
		int head = crtc ? nouveau_crtc(crtc)->index :
			   nv04_dfp_get_bound_head(dev, nv_encoder->dcb);

		if (mode == DRM_MODE_DPMS_ON) {
			call_lvds_script(dev, nv_encoder->dcb, head,
					 LVDS_PANEL_ON, nv_encoder->mode.clock);
		} else
			call_lvds_script(dev, nv_encoder->dcb, head,
					 LVDS_PANEL_OFF, 0);
	}

	nv04_dfp_update_backlight(encoder, mode);
	nv04_dfp_update_fp_control(encoder, mode);

	if (mode == DRM_MODE_DPMS_ON)
		nv04_dfp_prepare_sel_clk(dev, nv_encoder, nouveau_crtc(crtc)->index);
	else {
		dev_priv->mode_reg.sel_clk = NVReadRAMDAC(dev, 0, NV_PRAMDAC_SEL_CLK);
		dev_priv->mode_reg.sel_clk &= ~0xf0;
	}
	NVWriteRAMDAC(dev, 0, NV_PRAMDAC_SEL_CLK, dev_priv->mode_reg.sel_clk);
}

static void nv04_tmds_dpms(struct drm_encoder *encoder, int mode)
{
	struct drm_device *dev = encoder->dev;
	struct nouveau_encoder *nv_encoder = nouveau_encoder(encoder);

	if (nv_encoder->last_dpms == mode)
		return;
	nv_encoder->last_dpms = mode;

	NV_INFO(dev, "Setting dpms mode %d on tmds encoder (output %d)\n",
		     mode, nv_encoder->dcb->index);

	nv04_dfp_update_backlight(encoder, mode);
	nv04_dfp_update_fp_control(encoder, mode);
}

static void nv04_dfp_save(struct drm_encoder *encoder)
{
	struct nouveau_encoder *nv_encoder = nouveau_encoder(encoder);
	struct drm_device *dev = encoder->dev;

	if (nv_two_heads(dev))
		nv_encoder->restore.head =
			nv04_dfp_get_bound_head(dev, nv_encoder->dcb);
}

static void nv04_dfp_restore(struct drm_encoder *encoder)
{
	struct nouveau_encoder *nv_encoder = nouveau_encoder(encoder);
	struct drm_device *dev = encoder->dev;
	struct drm_nouveau_private *dev_priv = dev->dev_private;
	int head = nv_encoder->restore.head;

	if (nv_encoder->dcb->type == OUTPUT_LVDS) {
		struct nouveau_connector *connector =
			nouveau_encoder_connector_get(nv_encoder);

		if (connector && connector->native_mode)
			call_lvds_script(dev, nv_encoder->dcb, head,
					 LVDS_PANEL_ON,
					 connector->native_mode->clock);

	} else if (nv_encoder->dcb->type == OUTPUT_TMDS) {
		int clock = nouveau_hw_pllvals_to_clk
					(&dev_priv->saved_reg.crtc_reg[head].pllvals);

		run_tmds_table(dev, nv_encoder->dcb, head, clock);
	}

	nv_encoder->last_dpms = NV_DPMS_CLEARED;
}

static void nv04_dfp_destroy(struct drm_encoder *encoder)
{
	struct nouveau_encoder *nv_encoder = nouveau_encoder(encoder);

	NV_DEBUG_KMS(encoder->dev, "\n");

	if (get_slave_funcs(encoder))
		get_slave_funcs(encoder)->destroy(encoder);

	drm_encoder_cleanup(encoder);
	kfree(nv_encoder);
}

static void nv04_tmds_slave_init(struct drm_encoder *encoder)
{
	struct drm_device *dev = encoder->dev;
	struct dcb_entry *dcb = nouveau_encoder(encoder)->dcb;
	struct nouveau_i2c_chan *i2c = nouveau_i2c_find(dev, 2);
	struct i2c_board_info info[] = {
		{
			.type = "sil164",
			.addr = (dcb->tmdsconf.slave_addr == 0x7 ? 0x3a : 0x38),
			.platform_data = &(struct sil164_encoder_params) {
				SIL164_INPUT_EDGE_RISING
			}
		},
		{ }
	};
	int type;

	if (!nv_gf4_disp_arch(dev) || !i2c ||
	    get_tmds_slave(encoder))
		return;

	type = nouveau_i2c_identify(dev, "TMDS transmitter", info, NULL, 2);
	if (type < 0)
		return;

	drm_i2c_encoder_init(dev, to_encoder_slave(encoder),
			     &i2c->adapter, &info[type]);
}

static const struct drm_encoder_helper_funcs nv04_lvds_helper_funcs = {
	.dpms = nv04_lvds_dpms,
	.save = nv04_dfp_save,
	.restore = nv04_dfp_restore,
	.mode_fixup = nv04_dfp_mode_fixup,
	.prepare = nv04_dfp_prepare,
	.commit = nv04_dfp_commit,
	.mode_set = nv04_dfp_mode_set,
	.detect = NULL,
};

static const struct drm_encoder_helper_funcs nv04_tmds_helper_funcs = {
	.dpms = nv04_tmds_dpms,
	.save = nv04_dfp_save,
	.restore = nv04_dfp_restore,
	.mode_fixup = nv04_dfp_mode_fixup,
	.prepare = nv04_dfp_prepare,
	.commit = nv04_dfp_commit,
	.mode_set = nv04_dfp_mode_set,
	.detect = NULL,
};

static const struct drm_encoder_funcs nv04_dfp_funcs = {
	.destroy = nv04_dfp_destroy,
};

int
nv04_dfp_create(struct drm_connector *connector, struct dcb_entry *entry)
{
	const struct drm_encoder_helper_funcs *helper;
	struct nouveau_encoder *nv_encoder = NULL;
	struct drm_encoder *encoder;
	int type;

	switch (entry->type) {
	case OUTPUT_TMDS:
		type = DRM_MODE_ENCODER_TMDS;
		helper = &nv04_tmds_helper_funcs;
		break;
	case OUTPUT_LVDS:
		type = DRM_MODE_ENCODER_LVDS;
		helper = &nv04_lvds_helper_funcs;
		break;
	default:
		return -EINVAL;
	}

	nv_encoder = kzalloc(sizeof(*nv_encoder), GFP_KERNEL);
	if (!nv_encoder)
		return -ENOMEM;

	encoder = to_drm_encoder(nv_encoder);

	nv_encoder->dcb = entry;
	nv_encoder->or = ffs(entry->or) - 1;

	drm_encoder_init(connector->dev, encoder, &nv04_dfp_funcs, type);
	drm_encoder_helper_add(encoder, helper);

	encoder->possible_crtcs = entry->heads;
	encoder->possible_clones = 0;

	if (entry->type == OUTPUT_TMDS &&
	    entry->location != DCB_LOC_ON_CHIP)
		nv04_tmds_slave_init(encoder);

	drm_mode_connector_attach_encoder(connector, encoder);
	return 0;
}
