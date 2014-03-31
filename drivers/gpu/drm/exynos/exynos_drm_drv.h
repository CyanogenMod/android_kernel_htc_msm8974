/* exynos_drm_drv.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *	Joonyoung Shim <jy0922.shim@samsung.com>
 *	Seung-Woo Kim <sw0312.kim@samsung.com>
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

#ifndef _EXYNOS_DRM_DRV_H_
#define _EXYNOS_DRM_DRV_H_

#include <linux/module.h>
#include "drm.h"

#define MAX_CRTC	3
#define MAX_PLANE	5
#define MAX_FB_BUFFER	4
#define DEFAULT_ZPOS	-1

struct drm_device;
struct exynos_drm_overlay;
struct drm_connector;

extern unsigned int drm_vblank_offdelay;

enum exynos_drm_output_type {
	EXYNOS_DISPLAY_TYPE_NONE,
	
	EXYNOS_DISPLAY_TYPE_LCD,
	
	EXYNOS_DISPLAY_TYPE_HDMI,
	
	EXYNOS_DISPLAY_TYPE_VIDI,
};

struct exynos_drm_overlay_ops {
	void (*mode_set)(struct device *subdrv_dev,
			 struct exynos_drm_overlay *overlay);
	void (*commit)(struct device *subdrv_dev, int zpos);
	void (*disable)(struct device *subdrv_dev, int zpos);
};

struct exynos_drm_overlay {
	unsigned int fb_x;
	unsigned int fb_y;
	unsigned int fb_width;
	unsigned int fb_height;
	unsigned int crtc_x;
	unsigned int crtc_y;
	unsigned int crtc_width;
	unsigned int crtc_height;
	unsigned int mode_width;
	unsigned int mode_height;
	unsigned int refresh;
	unsigned int scan_flag;
	unsigned int bpp;
	unsigned int pitch;
	uint32_t pixel_format;
	dma_addr_t dma_addr[MAX_FB_BUFFER];
	void __iomem *vaddr[MAX_FB_BUFFER];
	int zpos;

	bool default_win;
	bool color_key;
	unsigned int index_color;
	bool local_path;
	bool transparency;
	bool activated;
};

struct exynos_drm_display_ops {
	enum exynos_drm_output_type type;
	bool (*is_connected)(struct device *dev);
	int (*get_edid)(struct device *dev, struct drm_connector *connector,
				u8 *edid, int len);
	void *(*get_panel)(struct device *dev);
	int (*check_timing)(struct device *dev, void *timing);
	int (*power_on)(struct device *dev, int mode);
};

struct exynos_drm_manager_ops {
	void (*dpms)(struct device *subdrv_dev, int mode);
	void (*apply)(struct device *subdrv_dev);
	void (*mode_fixup)(struct device *subdrv_dev,
				struct drm_connector *connector,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode);
	void (*mode_set)(struct device *subdrv_dev, void *mode);
	void (*get_max_resol)(struct device *subdrv_dev, unsigned int *width,
				unsigned int *height);
	void (*commit)(struct device *subdrv_dev);
	int (*enable_vblank)(struct device *subdrv_dev);
	void (*disable_vblank)(struct device *subdrv_dev);
};

struct exynos_drm_manager {
	struct device *dev;
	int pipe;
	struct exynos_drm_manager_ops *ops;
	struct exynos_drm_overlay_ops *overlay_ops;
	struct exynos_drm_display_ops *display_ops;
};

struct exynos_drm_private {
	struct drm_fb_helper *fb_helper;

	
	struct list_head pageflip_event_list;

	struct drm_crtc *crtc[MAX_CRTC];
};

struct exynos_drm_subdrv {
	struct list_head list;
	struct device *dev;
	struct drm_device *drm_dev;
	struct exynos_drm_manager *manager;

	int (*probe)(struct drm_device *drm_dev, struct device *dev);
	void (*remove)(struct drm_device *dev);
	int (*open)(struct drm_device *drm_dev, struct device *dev,
			struct drm_file *file);
	void (*close)(struct drm_device *drm_dev, struct device *dev,
			struct drm_file *file);

	struct drm_encoder *encoder;
	struct drm_connector *connector;
};

int exynos_drm_device_register(struct drm_device *dev);
int exynos_drm_device_unregister(struct drm_device *dev);

int exynos_drm_subdrv_register(struct exynos_drm_subdrv *drm_subdrv);

int exynos_drm_subdrv_unregister(struct exynos_drm_subdrv *drm_subdrv);

int exynos_drm_subdrv_open(struct drm_device *dev, struct drm_file *file);
void exynos_drm_subdrv_close(struct drm_device *dev, struct drm_file *file);

extern struct platform_driver fimd_driver;
extern struct platform_driver hdmi_driver;
extern struct platform_driver mixer_driver;
extern struct platform_driver exynos_drm_common_hdmi_driver;
extern struct platform_driver vidi_driver;
#endif
