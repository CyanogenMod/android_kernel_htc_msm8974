/* exynos_drm.h
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

#ifndef _EXYNOS_DRM_H_
#define _EXYNOS_DRM_H_

struct drm_exynos_gem_create {
	uint64_t size;
	unsigned int flags;
	unsigned int handle;
};

struct drm_exynos_gem_map_off {
	unsigned int handle;
	unsigned int pad;
	uint64_t offset;
};

struct drm_exynos_gem_mmap {
	unsigned int handle;
	unsigned int size;
	uint64_t mapped;
};

struct drm_exynos_vidi_connection {
	unsigned int connection;
	unsigned int extensions;
	uint64_t edid;
};

struct drm_exynos_plane_set_zpos {
	__u32 plane_id;
	__s32 zpos;
};

enum e_drm_exynos_gem_mem_type {
	
	EXYNOS_BO_NONCONTIG	= 1 << 0,
	EXYNOS_BO_MASK		= EXYNOS_BO_NONCONTIG
};

#define DRM_EXYNOS_GEM_CREATE		0x00
#define DRM_EXYNOS_GEM_MAP_OFFSET	0x01
#define DRM_EXYNOS_GEM_MMAP		0x02
#define DRM_EXYNOS_PLANE_SET_ZPOS	0x06
#define DRM_EXYNOS_VIDI_CONNECTION	0x07

#define DRM_IOCTL_EXYNOS_GEM_CREATE		DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_EXYNOS_GEM_CREATE, struct drm_exynos_gem_create)

#define DRM_IOCTL_EXYNOS_GEM_MAP_OFFSET	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_EXYNOS_GEM_MAP_OFFSET, struct drm_exynos_gem_map_off)

#define DRM_IOCTL_EXYNOS_GEM_MMAP	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_EXYNOS_GEM_MMAP, struct drm_exynos_gem_mmap)

#define DRM_IOCTL_EXYNOS_PLANE_SET_ZPOS	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_EXYNOS_PLANE_SET_ZPOS, struct drm_exynos_plane_set_zpos)

#define DRM_IOCTL_EXYNOS_VIDI_CONNECTION	DRM_IOWR(DRM_COMMAND_BASE + \
		DRM_EXYNOS_VIDI_CONNECTION, struct drm_exynos_vidi_connection)

#ifdef __KERNEL__

struct exynos_drm_panel_info {
	struct fb_videomode timing;
	u32 width_mm;
	u32 height_mm;
};

struct exynos_drm_fimd_pdata {
	struct exynos_drm_panel_info panel;
	u32				vidcon0;
	u32				vidcon1;
	unsigned int			default_win;
	unsigned int			bpp;
};

struct exynos_drm_common_hdmi_pd {
	struct device *hdmi_dev;
	struct device *mixer_dev;
};

struct exynos_drm_hdmi_pdata {
	struct fb_videomode		timing;
	unsigned int			default_win;
	unsigned int			bpp;
	unsigned int			is_v13:1;
};

#endif	
#endif	
