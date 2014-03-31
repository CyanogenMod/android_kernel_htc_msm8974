/*
 * Copyright © 1997-2003 by The XFree86 Project, Inc.
 * Copyright © 2007 Dave Airlie
 * Copyright © 2007-2008 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 * Copyright 2005-2006 Luc Verhaegen
 * Copyright (c) 2001, Andy Ritger  aritger@nvidia.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of the copyright holder(s)
 * and author(s) shall not be used in advertising or otherwise to promote
 * the sale, use or other dealings in this Software without prior written
 * authorization from the copyright holder(s) and author(s).
 */

#include <linux/list.h>
#include <linux/list_sort.h>
#include <linux/export.h>
#include "drmP.h"
#include "drm.h"
#include "drm_crtc.h"

void drm_mode_debug_printmodeline(struct drm_display_mode *mode)
{
	DRM_DEBUG_KMS("Modeline %d:\"%s\" %d %d %d %d %d %d %d %d %d %d "
			"0x%x 0x%x\n",
		mode->base.id, mode->name, mode->vrefresh, mode->clock,
		mode->hdisplay, mode->hsync_start,
		mode->hsync_end, mode->htotal,
		mode->vdisplay, mode->vsync_start,
		mode->vsync_end, mode->vtotal, mode->type, mode->flags);
}
EXPORT_SYMBOL(drm_mode_debug_printmodeline);

#define HV_FACTOR			1000
struct drm_display_mode *drm_cvt_mode(struct drm_device *dev, int hdisplay,
				      int vdisplay, int vrefresh,
				      bool reduced, bool interlaced, bool margins)
{
	
#define	CVT_MARGIN_PERCENTAGE		18
	
#define	CVT_H_GRANULARITY		8
	
#define	CVT_MIN_V_PORCH			3
	
#define	CVT_MIN_V_BPORCH		6
	
#define CVT_CLOCK_STEP			250
	struct drm_display_mode *drm_mode;
	unsigned int vfieldrate, hperiod;
	int hdisplay_rnd, hmargin, vdisplay_rnd, vmargin, vsync;
	int interlace;

	drm_mode = drm_mode_create(dev);
	if (!drm_mode)
		return NULL;

	
	if (!vrefresh)
		vrefresh = 60;

	
	if (interlaced)
		vfieldrate = vrefresh * 2;
	else
		vfieldrate = vrefresh;

	
	hdisplay_rnd = hdisplay - (hdisplay % CVT_H_GRANULARITY);

	
	hmargin = 0;
	if (margins) {
		hmargin = hdisplay_rnd * CVT_MARGIN_PERCENTAGE / 1000;
		hmargin -= hmargin % CVT_H_GRANULARITY;
	}
	
	drm_mode->hdisplay = hdisplay_rnd + 2 * hmargin;

	
	if (interlaced)
		vdisplay_rnd = vdisplay / 2;
	else
		vdisplay_rnd = vdisplay;

	
	vmargin = 0;
	if (margins)
		vmargin = vdisplay_rnd * CVT_MARGIN_PERCENTAGE / 1000;

	drm_mode->vdisplay = vdisplay + 2 * vmargin;

	
	if (interlaced)
		interlace = 1;
	else
		interlace = 0;

	
	if (!(vdisplay % 3) && ((vdisplay * 4 / 3) == hdisplay))
		vsync = 4;
	else if (!(vdisplay % 9) && ((vdisplay * 16 / 9) == hdisplay))
		vsync = 5;
	else if (!(vdisplay % 10) && ((vdisplay * 16 / 10) == hdisplay))
		vsync = 6;
	else if (!(vdisplay % 4) && ((vdisplay * 5 / 4) == hdisplay))
		vsync = 7;
	else if (!(vdisplay % 9) && ((vdisplay * 15 / 9) == hdisplay))
		vsync = 7;
	else 
		vsync = 10;

	if (!reduced) {
		
		int tmp1, tmp2;
#define CVT_MIN_VSYNC_BP	550
		
#define CVT_HSYNC_PERCENTAGE	8
		unsigned int hblank_percentage;
		int vsyncandback_porch, vback_porch, hblank;

		
		tmp1 = HV_FACTOR * 1000000  -
				CVT_MIN_VSYNC_BP * HV_FACTOR * vfieldrate;
		tmp2 = (vdisplay_rnd + 2 * vmargin + CVT_MIN_V_PORCH) * 2 +
				interlace;
		hperiod = tmp1 * 2 / (tmp2 * vfieldrate);

		tmp1 = CVT_MIN_VSYNC_BP * HV_FACTOR / hperiod + 1;
		
		if (tmp1 < (vsync + CVT_MIN_V_PORCH))
			vsyncandback_porch = vsync + CVT_MIN_V_PORCH;
		else
			vsyncandback_porch = tmp1;
		
		vback_porch = vsyncandback_porch - vsync;
		drm_mode->vtotal = vdisplay_rnd + 2 * vmargin +
				vsyncandback_porch + CVT_MIN_V_PORCH;
		
		
#define CVT_M_FACTOR	600
		
#define CVT_C_FACTOR	40
		
#define CVT_K_FACTOR	128
		
#define CVT_J_FACTOR	20
#define CVT_M_PRIME	(CVT_M_FACTOR * CVT_K_FACTOR / 256)
#define CVT_C_PRIME	((CVT_C_FACTOR - CVT_J_FACTOR) * CVT_K_FACTOR / 256 + \
			 CVT_J_FACTOR)
		
		hblank_percentage = CVT_C_PRIME * HV_FACTOR - CVT_M_PRIME *
					hperiod / 1000;
		
		if (hblank_percentage < 20 * HV_FACTOR)
			hblank_percentage = 20 * HV_FACTOR;
		hblank = drm_mode->hdisplay * hblank_percentage /
			 (100 * HV_FACTOR - hblank_percentage);
		hblank -= hblank % (2 * CVT_H_GRANULARITY);
		
		drm_mode->htotal = drm_mode->hdisplay + hblank;
		drm_mode->hsync_end = drm_mode->hdisplay + hblank / 2;
		drm_mode->hsync_start = drm_mode->hsync_end -
			(drm_mode->htotal * CVT_HSYNC_PERCENTAGE) / 100;
		drm_mode->hsync_start += CVT_H_GRANULARITY -
			drm_mode->hsync_start % CVT_H_GRANULARITY;
		
		drm_mode->vsync_start = drm_mode->vdisplay + CVT_MIN_V_PORCH;
		drm_mode->vsync_end = drm_mode->vsync_start + vsync;
	} else {
		
		
#define CVT_RB_MIN_VBLANK	460
		
#define CVT_RB_H_SYNC		32
		
#define CVT_RB_H_BLANK		160
		
#define CVT_RB_VFPORCH		3
		int vbilines;
		int tmp1, tmp2;
		
		tmp1 = HV_FACTOR * 1000000 -
			CVT_RB_MIN_VBLANK * HV_FACTOR * vfieldrate;
		tmp2 = vdisplay_rnd + 2 * vmargin;
		hperiod = tmp1 / (tmp2 * vfieldrate);
		
		vbilines = CVT_RB_MIN_VBLANK * HV_FACTOR / hperiod + 1;
		
		if (vbilines < (CVT_RB_VFPORCH + vsync + CVT_MIN_V_BPORCH))
			vbilines = CVT_RB_VFPORCH + vsync + CVT_MIN_V_BPORCH;
		
		drm_mode->vtotal = vdisplay_rnd + 2 * vmargin + vbilines;
		
		drm_mode->htotal = drm_mode->hdisplay + CVT_RB_H_BLANK;
		
		drm_mode->hsync_end = drm_mode->hdisplay + CVT_RB_H_BLANK / 2;
		drm_mode->hsync_start = drm_mode->hsync_end - CVT_RB_H_SYNC;
		
		drm_mode->vsync_start = drm_mode->vdisplay + CVT_RB_VFPORCH;
		drm_mode->vsync_end = drm_mode->vsync_start + vsync;
	}
	
	drm_mode->clock = drm_mode->htotal * HV_FACTOR * 1000 / hperiod;
	drm_mode->clock -= drm_mode->clock % CVT_CLOCK_STEP;
	
	
	if (interlaced) {
		drm_mode->vtotal *= 2;
		drm_mode->flags |= DRM_MODE_FLAG_INTERLACE;
	}
	
	drm_mode_set_name(drm_mode);
	if (reduced)
		drm_mode->flags |= (DRM_MODE_FLAG_PHSYNC |
					DRM_MODE_FLAG_NVSYNC);
	else
		drm_mode->flags |= (DRM_MODE_FLAG_PVSYNC |
					DRM_MODE_FLAG_NHSYNC);

	return drm_mode;
}
EXPORT_SYMBOL(drm_cvt_mode);

struct drm_display_mode *
drm_gtf_mode_complex(struct drm_device *dev, int hdisplay, int vdisplay,
		     int vrefresh, bool interlaced, int margins,
		     int GTF_M, int GTF_2C, int GTF_K, int GTF_2J)
{	
#define	GTF_MARGIN_PERCENTAGE		18
	
#define	GTF_CELL_GRAN			8
	
#define	GTF_MIN_V_PORCH			1
	
#define V_SYNC_RQD			3
	
#define H_SYNC_PERCENT			8
	
#define MIN_VSYNC_PLUS_BP		550
	
#define GTF_C_PRIME	((((GTF_2C - GTF_2J) * GTF_K / 256) + GTF_2J) / 2)
#define GTF_M_PRIME	(GTF_K * GTF_M / 256)
	struct drm_display_mode *drm_mode;
	unsigned int hdisplay_rnd, vdisplay_rnd, vfieldrate_rqd;
	int top_margin, bottom_margin;
	int interlace;
	unsigned int hfreq_est;
	int vsync_plus_bp, vback_porch;
	unsigned int vtotal_lines, vfieldrate_est, hperiod;
	unsigned int vfield_rate, vframe_rate;
	int left_margin, right_margin;
	unsigned int total_active_pixels, ideal_duty_cycle;
	unsigned int hblank, total_pixels, pixel_freq;
	int hsync, hfront_porch, vodd_front_porch_lines;
	unsigned int tmp1, tmp2;

	drm_mode = drm_mode_create(dev);
	if (!drm_mode)
		return NULL;

	hdisplay_rnd = (hdisplay + GTF_CELL_GRAN / 2) / GTF_CELL_GRAN;
	hdisplay_rnd = hdisplay_rnd * GTF_CELL_GRAN;

	if (interlaced)
		vdisplay_rnd = vdisplay / 2;
	else
		vdisplay_rnd = vdisplay;

	
	if (interlaced)
		vfieldrate_rqd = vrefresh * 2;
	else
		vfieldrate_rqd = vrefresh;

	
	top_margin = 0;
	if (margins)
		top_margin = (vdisplay_rnd * GTF_MARGIN_PERCENTAGE + 500) /
				1000;
	
	bottom_margin = top_margin;

	
	if (interlaced)
		interlace = 1;
	else
		interlace = 0;

	
	{
		tmp1 = (1000000  - MIN_VSYNC_PLUS_BP * vfieldrate_rqd) / 500;
		tmp2 = (vdisplay_rnd + 2 * top_margin + GTF_MIN_V_PORCH) *
				2 + interlace;
		hfreq_est = (tmp2 * 1000 * vfieldrate_rqd) / tmp1;
	}

	
	
	vsync_plus_bp = MIN_VSYNC_PLUS_BP * hfreq_est / 1000;
	vsync_plus_bp = (vsync_plus_bp + 500) / 1000;
	
	vback_porch = vsync_plus_bp - V_SYNC_RQD;
	
	vtotal_lines = vdisplay_rnd + top_margin + bottom_margin +
			vsync_plus_bp + GTF_MIN_V_PORCH;
	
	vfieldrate_est = hfreq_est / vtotal_lines;
	
	hperiod = 1000000 / (vfieldrate_rqd * vtotal_lines);

	
	vfield_rate = hfreq_est / vtotal_lines;
	
	if (interlaced)
		vframe_rate = vfield_rate / 2;
	else
		vframe_rate = vfield_rate;
	
	if (margins)
		left_margin = (hdisplay_rnd * GTF_MARGIN_PERCENTAGE + 500) /
				1000;
	else
		left_margin = 0;

	
	right_margin = left_margin;
	
	total_active_pixels = hdisplay_rnd + left_margin + right_margin;
	
	ideal_duty_cycle = GTF_C_PRIME * 1000 -
				(GTF_M_PRIME * 1000000 / hfreq_est);
	hblank = total_active_pixels * ideal_duty_cycle /
			(100000 - ideal_duty_cycle);
	hblank = (hblank + GTF_CELL_GRAN) / (2 * GTF_CELL_GRAN);
	hblank = hblank * 2 * GTF_CELL_GRAN;
	
	total_pixels = total_active_pixels + hblank;
	
	pixel_freq = total_pixels * hfreq_est / 1000;
	
	hsync = H_SYNC_PERCENT * total_pixels / 100;
	hsync = (hsync + GTF_CELL_GRAN / 2) / GTF_CELL_GRAN;
	hsync = hsync * GTF_CELL_GRAN;
	
	hfront_porch = hblank / 2 - hsync;
	
	vodd_front_porch_lines = GTF_MIN_V_PORCH ;

	
	drm_mode->hdisplay = hdisplay_rnd;
	drm_mode->hsync_start = hdisplay_rnd + hfront_porch;
	drm_mode->hsync_end = drm_mode->hsync_start + hsync;
	drm_mode->htotal = total_pixels;
	drm_mode->vdisplay = vdisplay_rnd;
	drm_mode->vsync_start = vdisplay_rnd + vodd_front_porch_lines;
	drm_mode->vsync_end = drm_mode->vsync_start + V_SYNC_RQD;
	drm_mode->vtotal = vtotal_lines;

	drm_mode->clock = pixel_freq;

	if (interlaced) {
		drm_mode->vtotal *= 2;
		drm_mode->flags |= DRM_MODE_FLAG_INTERLACE;
	}

	drm_mode_set_name(drm_mode);
	if (GTF_M == 600 && GTF_2C == 80 && GTF_K == 128 && GTF_2J == 40)
		drm_mode->flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC;
	else
		drm_mode->flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC;

	return drm_mode;
}
EXPORT_SYMBOL(drm_gtf_mode_complex);

struct drm_display_mode *
drm_gtf_mode(struct drm_device *dev, int hdisplay, int vdisplay, int vrefresh,
	     bool lace, int margins)
{
	return drm_gtf_mode_complex(dev, hdisplay, vdisplay, vrefresh, lace,
				    margins, 600, 40 * 2, 128, 20 * 2);
}
EXPORT_SYMBOL(drm_gtf_mode);

void drm_mode_set_name(struct drm_display_mode *mode)
{
	bool interlaced = !!(mode->flags & DRM_MODE_FLAG_INTERLACE);

	snprintf(mode->name, DRM_DISPLAY_MODE_LEN, "%dx%d%s",
		 mode->hdisplay, mode->vdisplay,
		 interlaced ? "i" : "");
}
EXPORT_SYMBOL(drm_mode_set_name);

void drm_mode_list_concat(struct list_head *head, struct list_head *new)
{

	struct list_head *entry, *tmp;

	list_for_each_safe(entry, tmp, head) {
		list_move_tail(entry, new);
	}
}
EXPORT_SYMBOL(drm_mode_list_concat);

int drm_mode_width(struct drm_display_mode *mode)
{
	return mode->hdisplay;

}
EXPORT_SYMBOL(drm_mode_width);

int drm_mode_height(struct drm_display_mode *mode)
{
	return mode->vdisplay;
}
EXPORT_SYMBOL(drm_mode_height);

int drm_mode_hsync(const struct drm_display_mode *mode)
{
	unsigned int calc_val;

	if (mode->hsync)
		return mode->hsync;

	if (mode->htotal < 0)
		return 0;

	calc_val = (mode->clock * 1000) / mode->htotal; 
	calc_val += 500;				
	calc_val /= 1000;				

	return calc_val;
}
EXPORT_SYMBOL(drm_mode_hsync);

int drm_mode_vrefresh(const struct drm_display_mode *mode)
{
	int refresh = 0;
	unsigned int calc_val;

	if (mode->vrefresh > 0)
		refresh = mode->vrefresh;
	else if (mode->htotal > 0 && mode->vtotal > 0) {
		int vtotal;
		vtotal = mode->vtotal;
		
		calc_val = (mode->clock * 1000);
		calc_val /= mode->htotal;
		refresh = (calc_val + vtotal / 2) / vtotal;

		if (mode->flags & DRM_MODE_FLAG_INTERLACE)
			refresh *= 2;
		if (mode->flags & DRM_MODE_FLAG_DBLSCAN)
			refresh /= 2;
		if (mode->vscan > 1)
			refresh /= mode->vscan;
	}
	return refresh;
}
EXPORT_SYMBOL(drm_mode_vrefresh);

void drm_mode_set_crtcinfo(struct drm_display_mode *p, int adjust_flags)
{
	if ((p == NULL) || ((p->type & DRM_MODE_TYPE_CRTC_C) == DRM_MODE_TYPE_BUILTIN))
		return;

	p->crtc_hdisplay = p->hdisplay;
	p->crtc_hsync_start = p->hsync_start;
	p->crtc_hsync_end = p->hsync_end;
	p->crtc_htotal = p->htotal;
	p->crtc_hskew = p->hskew;
	p->crtc_vdisplay = p->vdisplay;
	p->crtc_vsync_start = p->vsync_start;
	p->crtc_vsync_end = p->vsync_end;
	p->crtc_vtotal = p->vtotal;

	if (p->flags & DRM_MODE_FLAG_INTERLACE) {
		if (adjust_flags & CRTC_INTERLACE_HALVE_V) {
			p->crtc_vdisplay /= 2;
			p->crtc_vsync_start /= 2;
			p->crtc_vsync_end /= 2;
			p->crtc_vtotal /= 2;
		}
	}

	if (p->flags & DRM_MODE_FLAG_DBLSCAN) {
		p->crtc_vdisplay *= 2;
		p->crtc_vsync_start *= 2;
		p->crtc_vsync_end *= 2;
		p->crtc_vtotal *= 2;
	}

	if (p->vscan > 1) {
		p->crtc_vdisplay *= p->vscan;
		p->crtc_vsync_start *= p->vscan;
		p->crtc_vsync_end *= p->vscan;
		p->crtc_vtotal *= p->vscan;
	}

	p->crtc_vblank_start = min(p->crtc_vsync_start, p->crtc_vdisplay);
	p->crtc_vblank_end = max(p->crtc_vsync_end, p->crtc_vtotal);
	p->crtc_hblank_start = min(p->crtc_hsync_start, p->crtc_hdisplay);
	p->crtc_hblank_end = max(p->crtc_hsync_end, p->crtc_htotal);

	p->crtc_hadjusted = false;
	p->crtc_vadjusted = false;
}
EXPORT_SYMBOL(drm_mode_set_crtcinfo);


void drm_mode_copy(struct drm_display_mode *dst, const struct drm_display_mode *src)
{
	int id = dst->base.id;

	*dst = *src;
	dst->base.id = id;
	INIT_LIST_HEAD(&dst->head);
}
EXPORT_SYMBOL(drm_mode_copy);

struct drm_display_mode *drm_mode_duplicate(struct drm_device *dev,
					    const struct drm_display_mode *mode)
{
	struct drm_display_mode *nmode;

	nmode = drm_mode_create(dev);
	if (!nmode)
		return NULL;

	drm_mode_copy(nmode, mode);

	return nmode;
}
EXPORT_SYMBOL(drm_mode_duplicate);

bool drm_mode_equal(struct drm_display_mode *mode1, struct drm_display_mode *mode2)
{
	if (mode1->clock && mode2->clock) {
		if (KHZ2PICOS(mode1->clock) != KHZ2PICOS(mode2->clock))
			return false;
	} else if (mode1->clock != mode2->clock)
		return false;

	if (mode1->hdisplay == mode2->hdisplay &&
	    mode1->hsync_start == mode2->hsync_start &&
	    mode1->hsync_end == mode2->hsync_end &&
	    mode1->htotal == mode2->htotal &&
	    mode1->hskew == mode2->hskew &&
	    mode1->vdisplay == mode2->vdisplay &&
	    mode1->vsync_start == mode2->vsync_start &&
	    mode1->vsync_end == mode2->vsync_end &&
	    mode1->vtotal == mode2->vtotal &&
	    mode1->vscan == mode2->vscan &&
	    mode1->flags == mode2->flags)
		return true;

	return false;
}
EXPORT_SYMBOL(drm_mode_equal);

void drm_mode_validate_size(struct drm_device *dev,
			    struct list_head *mode_list,
			    int maxX, int maxY, int maxPitch)
{
	struct drm_display_mode *mode;

	list_for_each_entry(mode, mode_list, head) {
		if (maxPitch > 0 && mode->hdisplay > maxPitch)
			mode->status = MODE_BAD_WIDTH;

		if (maxX > 0 && mode->hdisplay > maxX)
			mode->status = MODE_VIRTUAL_X;

		if (maxY > 0 && mode->vdisplay > maxY)
			mode->status = MODE_VIRTUAL_Y;
	}
}
EXPORT_SYMBOL(drm_mode_validate_size);

void drm_mode_validate_clocks(struct drm_device *dev,
			      struct list_head *mode_list,
			      int *min, int *max, int n_ranges)
{
	struct drm_display_mode *mode;
	int i;

	list_for_each_entry(mode, mode_list, head) {
		bool good = false;
		for (i = 0; i < n_ranges; i++) {
			if (mode->clock >= min[i] && mode->clock <= max[i]) {
				good = true;
				break;
			}
		}
		if (!good)
			mode->status = MODE_CLOCK_RANGE;
	}
}
EXPORT_SYMBOL(drm_mode_validate_clocks);

void drm_mode_prune_invalid(struct drm_device *dev,
			    struct list_head *mode_list, bool verbose)
{
	struct drm_display_mode *mode, *t;

	list_for_each_entry_safe(mode, t, mode_list, head) {
		if (mode->status != MODE_OK) {
			list_del(&mode->head);
			if (verbose) {
				drm_mode_debug_printmodeline(mode);
				DRM_DEBUG_KMS("Not using %s mode %d\n",
					mode->name, mode->status);
			}
			drm_mode_destroy(dev, mode);
		}
	}
}
EXPORT_SYMBOL(drm_mode_prune_invalid);

static int drm_mode_compare(void *priv, struct list_head *lh_a, struct list_head *lh_b)
{
	struct drm_display_mode *a = list_entry(lh_a, struct drm_display_mode, head);
	struct drm_display_mode *b = list_entry(lh_b, struct drm_display_mode, head);
	int diff;

	diff = ((b->type & DRM_MODE_TYPE_PREFERRED) != 0) -
		((a->type & DRM_MODE_TYPE_PREFERRED) != 0);
	if (diff)
		return diff;
	diff = b->hdisplay * b->vdisplay - a->hdisplay * a->vdisplay;
	if (diff)
		return diff;
	diff = b->clock - a->clock;
	return diff;
}

void drm_mode_sort(struct list_head *mode_list)
{
	list_sort(NULL, mode_list, drm_mode_compare);
}
EXPORT_SYMBOL(drm_mode_sort);

void drm_mode_connector_list_update(struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *pmode, *pt;
	int found_it;

	list_for_each_entry_safe(pmode, pt, &connector->probed_modes,
				 head) {
		found_it = 0;
		
		list_for_each_entry(mode, &connector->modes, head) {
			if (drm_mode_equal(pmode, mode)) {
				found_it = 1;
				
				mode->status = pmode->status;
				
				mode->type |= pmode->type;
				list_del(&pmode->head);
				drm_mode_destroy(connector->dev, pmode);
				break;
			}
		}

		if (!found_it) {
			list_move_tail(&pmode->head, &connector->modes);
		}
	}
}
EXPORT_SYMBOL(drm_mode_connector_list_update);

bool drm_mode_parse_command_line_for_connector(const char *mode_option,
					       struct drm_connector *connector,
					       struct drm_cmdline_mode *mode)
{
	const char *name;
	unsigned int namelen;
	bool res_specified = false, bpp_specified = false, refresh_specified = false;
	unsigned int xres = 0, yres = 0, bpp = 32, refresh = 0;
	bool yres_specified = false, cvt = false, rb = false;
	bool interlace = false, margins = false, was_digit = false;
	int i;
	enum drm_connector_force force = DRM_FORCE_UNSPECIFIED;

#ifdef CONFIG_FB
	if (!mode_option)
		mode_option = fb_mode_option;
#endif

	if (!mode_option) {
		mode->specified = false;
		return false;
	}

	name = mode_option;
	namelen = strlen(name);
	for (i = namelen-1; i >= 0; i--) {
		switch (name[i]) {
		case '@':
			if (!refresh_specified && !bpp_specified &&
			    !yres_specified && !cvt && !rb && was_digit) {
				refresh = simple_strtol(&name[i+1], NULL, 10);
				refresh_specified = true;
				was_digit = false;
			} else
				goto done;
			break;
		case '-':
			if (!bpp_specified && !yres_specified && !cvt &&
			    !rb && was_digit) {
				bpp = simple_strtol(&name[i+1], NULL, 10);
				bpp_specified = true;
				was_digit = false;
			} else
				goto done;
			break;
		case 'x':
			if (!yres_specified && was_digit) {
				yres = simple_strtol(&name[i+1], NULL, 10);
				yres_specified = true;
				was_digit = false;
			} else
				goto done;
		case '0' ... '9':
			was_digit = true;
			break;
		case 'M':
			if (yres_specified || cvt || was_digit)
				goto done;
			cvt = true;
			break;
		case 'R':
			if (yres_specified || cvt || rb || was_digit)
				goto done;
			rb = true;
			break;
		case 'm':
			if (cvt || yres_specified || was_digit)
				goto done;
			margins = true;
			break;
		case 'i':
			if (cvt || yres_specified || was_digit)
				goto done;
			interlace = true;
			break;
		case 'e':
			if (yres_specified || bpp_specified || refresh_specified ||
			    was_digit || (force != DRM_FORCE_UNSPECIFIED))
				goto done;

			force = DRM_FORCE_ON;
			break;
		case 'D':
			if (yres_specified || bpp_specified || refresh_specified ||
			    was_digit || (force != DRM_FORCE_UNSPECIFIED))
				goto done;

			if ((connector->connector_type != DRM_MODE_CONNECTOR_DVII) &&
			    (connector->connector_type != DRM_MODE_CONNECTOR_HDMIB))
				force = DRM_FORCE_ON;
			else
				force = DRM_FORCE_ON_DIGITAL;
			break;
		case 'd':
			if (yres_specified || bpp_specified || refresh_specified ||
			    was_digit || (force != DRM_FORCE_UNSPECIFIED))
				goto done;

			force = DRM_FORCE_OFF;
			break;
		default:
			goto done;
		}
	}

	if (i < 0 && yres_specified) {
		char *ch;
		xres = simple_strtol(name, &ch, 10);
		if ((ch != NULL) && (*ch == 'x'))
			res_specified = true;
		else
			i = ch - name;
	} else if (!yres_specified && was_digit) {
		
		i = 0;
	}
done:
	if (i >= 0) {
		printk(KERN_WARNING
			"parse error at position %i in video mode '%s'\n",
			i, name);
		mode->specified = false;
		return false;
	}

	if (res_specified) {
		mode->specified = true;
		mode->xres = xres;
		mode->yres = yres;
	}

	if (refresh_specified) {
		mode->refresh_specified = true;
		mode->refresh = refresh;
	}

	if (bpp_specified) {
		mode->bpp_specified = true;
		mode->bpp = bpp;
	}
	mode->rb = rb;
	mode->cvt = cvt;
	mode->interlace = interlace;
	mode->margins = margins;
	mode->force = force;

	return true;
}
EXPORT_SYMBOL(drm_mode_parse_command_line_for_connector);

struct drm_display_mode *
drm_mode_create_from_cmdline_mode(struct drm_device *dev,
				  struct drm_cmdline_mode *cmd)
{
	struct drm_display_mode *mode;

	if (cmd->cvt)
		mode = drm_cvt_mode(dev,
				    cmd->xres, cmd->yres,
				    cmd->refresh_specified ? cmd->refresh : 60,
				    cmd->rb, cmd->interlace,
				    cmd->margins);
	else
		mode = drm_gtf_mode(dev,
				    cmd->xres, cmd->yres,
				    cmd->refresh_specified ? cmd->refresh : 60,
				    cmd->interlace,
				    cmd->margins);
	if (!mode)
		return NULL;

	drm_mode_set_crtcinfo(mode, CRTC_INTERLACE_HALVE_V);
	return mode;
}
EXPORT_SYMBOL(drm_mode_create_from_cmdline_mode);
