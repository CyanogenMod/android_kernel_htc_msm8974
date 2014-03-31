/*
 * omap_voutlib.c
 *
 * Copyright (C) 2005-2010 Texas Instruments.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 * Based on the OMAP2 camera driver
 * Video-for-Linux (Version 2) camera capture driver for
 * the OMAP24xx camera controller.
 *
 * Author: Andy Lowe (source@mvista.com)
 *
 * Copyright (C) 2004 MontaVista Software, Inc.
 * Copyright (C) 2010 Texas Instruments.
 *
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/videodev2.h>

#include <linux/dma-mapping.h>

#include <plat/cpu.h>

#include "omap_voutlib.h"

MODULE_AUTHOR("Texas Instruments");
MODULE_DESCRIPTION("OMAP Video library");
MODULE_LICENSE("GPL");

void omap_vout_default_crop(struct v4l2_pix_format *pix,
		  struct v4l2_framebuffer *fbuf, struct v4l2_rect *crop)
{
	crop->width = (pix->width < fbuf->fmt.width) ?
		pix->width : fbuf->fmt.width;
	crop->height = (pix->height < fbuf->fmt.height) ?
		pix->height : fbuf->fmt.height;
	crop->width &= ~1;
	crop->height &= ~1;
	crop->left = ((pix->width - crop->width) >> 1) & ~1;
	crop->top = ((pix->height - crop->height) >> 1) & ~1;
}
EXPORT_SYMBOL_GPL(omap_vout_default_crop);

int omap_vout_try_window(struct v4l2_framebuffer *fbuf,
			struct v4l2_window *new_win)
{
	struct v4l2_rect try_win;

	
	try_win = new_win->w;

	if (try_win.left < 0) {
		try_win.width += try_win.left;
		try_win.left = 0;
	}
	if (try_win.top < 0) {
		try_win.height += try_win.top;
		try_win.top = 0;
	}
	try_win.width = (try_win.width < fbuf->fmt.width) ?
		try_win.width : fbuf->fmt.width;
	try_win.height = (try_win.height < fbuf->fmt.height) ?
		try_win.height : fbuf->fmt.height;
	if (try_win.left + try_win.width > fbuf->fmt.width)
		try_win.width = fbuf->fmt.width - try_win.left;
	if (try_win.top + try_win.height > fbuf->fmt.height)
		try_win.height = fbuf->fmt.height - try_win.top;
	try_win.width &= ~1;
	try_win.height &= ~1;

	if (try_win.width <= 0 || try_win.height <= 0)
		return -EINVAL;

	
	new_win->w = try_win;
	new_win->field = V4L2_FIELD_ANY;
	return 0;
}
EXPORT_SYMBOL_GPL(omap_vout_try_window);

int omap_vout_new_window(struct v4l2_rect *crop,
		struct v4l2_window *win, struct v4l2_framebuffer *fbuf,
		struct v4l2_window *new_win)
{
	int err;

	err = omap_vout_try_window(fbuf, new_win);
	if (err)
		return err;

	
	win->w = new_win->w;
	win->field = new_win->field;
	win->chromakey = new_win->chromakey;

	
	if (cpu_is_omap24xx()) {
		
		if ((crop->height/win->w.height) >= 2)
			crop->height = win->w.height * 2;

		if ((crop->width/win->w.width) >= 2)
			crop->width = win->w.width * 2;

		if (crop->width > 768) {
			if (crop->height != win->w.height)
				crop->width = 768;
		}
	} else if (cpu_is_omap34xx()) {
		
		if ((crop->height/win->w.height) >= 4)
			crop->height = win->w.height * 4;

		if ((crop->width/win->w.width) >= 4)
			crop->width = win->w.width * 4;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(omap_vout_new_window);

int omap_vout_new_crop(struct v4l2_pix_format *pix,
	      struct v4l2_rect *crop, struct v4l2_window *win,
	      struct v4l2_framebuffer *fbuf, const struct v4l2_rect *new_crop)
{
	struct v4l2_rect try_crop;
	unsigned long vresize, hresize;

	
	try_crop = *new_crop;

	
	if (try_crop.left < 0) {
		try_crop.width += try_crop.left;
		try_crop.left = 0;
	}
	if (try_crop.top < 0) {
		try_crop.height += try_crop.top;
		try_crop.top = 0;
	}
	try_crop.width = (try_crop.width < pix->width) ?
		try_crop.width : pix->width;
	try_crop.height = (try_crop.height < pix->height) ?
		try_crop.height : pix->height;
	if (try_crop.left + try_crop.width > pix->width)
		try_crop.width = pix->width - try_crop.left;
	if (try_crop.top + try_crop.height > pix->height)
		try_crop.height = pix->height - try_crop.top;

	try_crop.width &= ~1;
	try_crop.height &= ~1;

	if (try_crop.width <= 0 || try_crop.height <= 0)
		return -EINVAL;

	if (cpu_is_omap24xx()) {
		if (try_crop.height != win->w.height) {
			if (try_crop.width > 768)
				try_crop.width = 768;
		}
	}
	
	vresize = (1024 * try_crop.height) / win->w.height;
	if (cpu_is_omap24xx() && (vresize > 2048))
		vresize = 2048;
	else if (cpu_is_omap34xx() && (vresize > 4096))
		vresize = 4096;

	win->w.height = ((1024 * try_crop.height) / vresize) & ~1;
	if (win->w.height == 0)
		win->w.height = 2;
	if (win->w.height + win->w.top > fbuf->fmt.height) {
		win->w.height = (fbuf->fmt.height - win->w.top) & ~1;
		if (try_crop.height == 0)
			try_crop.height = 2;
	}
	
	hresize = (1024 * try_crop.width) / win->w.width;
	if (cpu_is_omap24xx() && (hresize > 2048))
		hresize = 2048;
	else if (cpu_is_omap34xx() && (hresize > 4096))
		hresize = 4096;

	win->w.width = ((1024 * try_crop.width) / hresize) & ~1;
	if (win->w.width == 0)
		win->w.width = 2;
	if (win->w.width + win->w.left > fbuf->fmt.width) {
		win->w.width = (fbuf->fmt.width - win->w.left) & ~1;
		if (try_crop.width == 0)
			try_crop.width = 2;
	}
	if (cpu_is_omap24xx()) {
		if ((try_crop.height/win->w.height) >= 2)
			try_crop.height = win->w.height * 2;

		if ((try_crop.width/win->w.width) >= 2)
			try_crop.width = win->w.width * 2;

		if (try_crop.width > 768) {
			if (try_crop.height != win->w.height)
				try_crop.width = 768;
		}
	} else if (cpu_is_omap34xx()) {
		if ((try_crop.height/win->w.height) >= 4)
			try_crop.height = win->w.height * 4;

		if ((try_crop.width/win->w.width) >= 4)
			try_crop.width = win->w.width * 4;
	}
	
	*crop = try_crop;
	return 0;
}
EXPORT_SYMBOL_GPL(omap_vout_new_crop);

void omap_vout_new_format(struct v4l2_pix_format *pix,
		struct v4l2_framebuffer *fbuf, struct v4l2_rect *crop,
		struct v4l2_window *win)
{
	omap_vout_default_crop(pix, fbuf, crop);

	
	win->w.width = crop->width;
	win->w.height = crop->height;
	win->w.left = ((fbuf->fmt.width - win->w.width) >> 1) & ~1;
	win->w.top = ((fbuf->fmt.height - win->w.height) >> 1) & ~1;
}
EXPORT_SYMBOL_GPL(omap_vout_new_format);

unsigned long omap_vout_alloc_buffer(u32 buf_size, u32 *phys_addr)
{
	u32 order, size;
	unsigned long virt_addr, addr;

	size = PAGE_ALIGN(buf_size);
	order = get_order(size);
	virt_addr = __get_free_pages(GFP_KERNEL, order);
	addr = virt_addr;

	if (virt_addr) {
		while (size > 0) {
			SetPageReserved(virt_to_page(addr));
			addr += PAGE_SIZE;
			size -= PAGE_SIZE;
		}
	}
	*phys_addr = (u32) virt_to_phys((void *) virt_addr);
	return virt_addr;
}

void omap_vout_free_buffer(unsigned long virtaddr, u32 buf_size)
{
	u32 order, size;
	unsigned long addr = virtaddr;

	size = PAGE_ALIGN(buf_size);
	order = get_order(size);

	while (size > 0) {
		ClearPageReserved(virt_to_page(addr));
		addr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	free_pages((unsigned long) virtaddr, order);
}
