/*-*- linux-c -*-
 *  linux/drivers/video/i810_accel.c -- Hardware Acceleration
 *
 *      Copyright (C) 2001 Antonino Daplas<adaplas@pol.net>
 *      All Rights Reserved      
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/fb.h>

#include "i810_regs.h"
#include "i810.h"
#include "i810_main.h"

static u32 i810fb_rop[] = {
	COLOR_COPY_ROP, 
	XOR_ROP         
};

#define PUT_RING(n) {                                        \
	i810_writel(par->cur_tail, par->iring.virtual, n);   \
        par->cur_tail += 4;                                  \
        par->cur_tail &= RING_SIZE_MASK;                     \
}                                                                      

extern void flush_cache(void);


static inline void i810_report_error(u8 __iomem *mmio)
{
	printk("IIR     : 0x%04x\n"
	       "EIR     : 0x%04x\n"
	       "PGTBL_ER: 0x%04x\n"
	       "IPEIR   : 0x%04x\n"
	       "IPEHR   : 0x%04x\n",
	       i810_readw(IIR, mmio),
	       i810_readb(EIR, mmio),
	       i810_readl(PGTBL_ER, mmio),
	       i810_readl(IPEIR, mmio), 
	       i810_readl(IPEHR, mmio));
}

	
static inline int wait_for_space(struct fb_info *info, u32 space)
{
	struct i810fb_par *par = info->par;
	u32 head, count = WAIT_COUNT, tail;
	u8 __iomem *mmio = par->mmio_start_virtual;

	tail = par->cur_tail;
	while (count--) {
		head = i810_readl(IRING + 4, mmio) & RBUFFER_HEAD_MASK;	
		if ((tail == head) || 
		    (tail > head && 
		     (par->iring.size - tail + head) >= space) || 
		    (tail < head && (head - tail) >= space)) {
			return 0;	
		}
	}
	printk("ringbuffer lockup!!!\n");
	i810_report_error(mmio); 
	par->dev_flags |= LOCKUP;
	info->pixmap.scan_align = 1;
	return 1;
}

static inline int wait_for_engine_idle(struct fb_info *info)
{
	struct i810fb_par *par = info->par;
	u8 __iomem *mmio = par->mmio_start_virtual;
	int count = WAIT_COUNT;

	if (wait_for_space(info, par->iring.size)) 
		return 1;

	while((i810_readw(INSTDONE, mmio) & 0x7B) != 0x7B && --count); 
	if (count) return 0;

	printk("accel engine lockup!!!\n");
	printk("INSTDONE: 0x%04x\n", i810_readl(INSTDONE, mmio));
	i810_report_error(mmio); 
	par->dev_flags |= LOCKUP;
	info->pixmap.scan_align = 1;
	return 1;
}

 
static inline u32 begin_iring(struct fb_info *info, u32 space)
{
	struct i810fb_par *par = info->par;

	if (par->dev_flags & ALWAYS_SYNC) 
		wait_for_engine_idle(info);
	return wait_for_space(info, space);
}

static inline void end_iring(struct i810fb_par *par)
{
	u8 __iomem *mmio = par->mmio_start_virtual;

	i810_writel(IRING, mmio, par->cur_tail);
}

static inline void source_copy_blit(int dwidth, int dheight, int dpitch, 
				    int xdir, int src, int dest, int rop, 
				    int blit_bpp, struct fb_info *info)
{
	struct i810fb_par *par = info->par;

	if (begin_iring(info, 24 + IRING_PAD)) return;

	PUT_RING(BLIT | SOURCE_COPY_BLIT | 4);
	PUT_RING(xdir | rop << 16 | dpitch | DYN_COLOR_EN | blit_bpp);
	PUT_RING(dheight << 16 | dwidth);
	PUT_RING(dest);
	PUT_RING(dpitch);
	PUT_RING(src);

	end_iring(par);
}	

static inline void color_blit(int width, int height, int pitch,  int dest, 
			      int rop, int what, int blit_bpp, 
			      struct fb_info *info)
{
	struct i810fb_par *par = info->par;

	if (begin_iring(info, 24 + IRING_PAD)) return;

	PUT_RING(BLIT | COLOR_BLT | 3);
	PUT_RING(rop << 16 | pitch | SOLIDPATTERN | DYN_COLOR_EN | blit_bpp);
	PUT_RING(height << 16 | width);
	PUT_RING(dest);
	PUT_RING(what);
	PUT_RING(NOP);

	end_iring(par);
}
 
static inline void mono_src_copy_imm_blit(int dwidth, int dheight, int dpitch,
					  int dsize, int blit_bpp, int rop,
					  int dest, const u32 *src, int bg,
					  int fg, struct fb_info *info)
{
	struct i810fb_par *par = info->par;

	if (begin_iring(info, 24 + (dsize << 2) + IRING_PAD)) return;

	PUT_RING(BLIT | MONO_SOURCE_COPY_IMMEDIATE | (4 + dsize));
	PUT_RING(DYN_COLOR_EN | blit_bpp | rop << 16 | dpitch);
	PUT_RING(dheight << 16 | dwidth);
	PUT_RING(dest);
	PUT_RING(bg);
	PUT_RING(fg);
	while (dsize--) 
		PUT_RING(*src++);

	end_iring(par);
}

static inline void load_front(int offset, struct fb_info *info)
{
	struct i810fb_par *par = info->par;

	if (begin_iring(info, 8 + IRING_PAD)) return;

	PUT_RING(PARSER | FLUSH);
	PUT_RING(NOP);

	end_iring(par);

	if (begin_iring(info, 8 + IRING_PAD)) return;

	PUT_RING(PARSER | FRONT_BUFFER | ((par->pitch >> 3) << 8));
	PUT_RING((par->fb.offset << 12) + offset);

	end_iring(par);
}

static inline void i810fb_iring_enable(struct i810fb_par *par, u32 mode)
{
	u32 tmp;
	u8 __iomem *mmio = par->mmio_start_virtual;

	tmp = i810_readl(IRING + 12, mmio);
	if (mode == OFF) 
		tmp &= ~1;
	else 
		tmp |= 1;
	flush_cache();
	i810_writel(IRING + 12, mmio, tmp);
}       

void i810fb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
	struct i810fb_par *par = info->par;
	u32 dx, dy, width, height, dest, rop = 0, color = 0;

	if (!info->var.accel_flags || par->dev_flags & LOCKUP ||
	    par->depth == 4) {
		cfb_fillrect(info, rect);
		return;
	}

	if (par->depth == 1) 
		color = rect->color;
	else 
		color = ((u32 *) (info->pseudo_palette))[rect->color];

	rop = i810fb_rop[rect->rop];

	dx = rect->dx * par->depth;
	width = rect->width * par->depth;
	dy = rect->dy;
	height = rect->height;

	dest = info->fix.smem_start + (dy * info->fix.line_length) + dx;
	color_blit(width, height, info->fix.line_length, dest, rop, color, 
		   par->blit_bpp, info);
}
	
void i810fb_copyarea(struct fb_info *info, const struct fb_copyarea *region) 
{
	struct i810fb_par *par = info->par;
	u32 sx, sy, dx, dy, pitch, width, height, src, dest, xdir;

	if (!info->var.accel_flags || par->dev_flags & LOCKUP ||
	    par->depth == 4) {
		cfb_copyarea(info, region);
		return;
	}

	dx = region->dx * par->depth;
	sx = region->sx * par->depth;
	width = region->width * par->depth;
	sy = region->sy;
	dy = region->dy;
	height = region->height;

	if (dx <= sx) {
		xdir = INCREMENT;
	}
	else {
		xdir = DECREMENT;
		sx += width - 1;
		dx += width - 1;
	}
	if (dy <= sy) {
		pitch = info->fix.line_length;
	}
	else {
		pitch = (-(info->fix.line_length)) & 0xFFFF;
		sy += height - 1;
		dy += height - 1;
	}
	src = info->fix.smem_start + (sy * info->fix.line_length) + sx;
	dest = info->fix.smem_start + (dy * info->fix.line_length) + dx;

	source_copy_blit(width, height, pitch, xdir, src, dest,
			 PAT_COPY_ROP, par->blit_bpp, info);
}

void i810fb_imageblit(struct fb_info *info, const struct fb_image *image)
{
	struct i810fb_par *par = info->par;
	u32 fg = 0, bg = 0, size, dst;
	
	if (!info->var.accel_flags || par->dev_flags & LOCKUP ||
	    par->depth == 4 || image->depth != 1) {
		cfb_imageblit(info, image);
		return;
	}

	switch (info->var.bits_per_pixel) {
	case 8:
		fg = image->fg_color;
		bg = image->bg_color;
		break;
	case 16:
	case 24:
		fg = ((u32 *)(info->pseudo_palette))[image->fg_color];
		bg = ((u32 *)(info->pseudo_palette))[image->bg_color];
		break;
	}	
	
	dst = info->fix.smem_start + (image->dy * info->fix.line_length) + 
		(image->dx * par->depth);

	size = (image->width+7)/8 + 1;
	size &= ~1;
	size *= image->height;
	size += 7;
	size &= ~7;
	mono_src_copy_imm_blit(image->width * par->depth, 
			       image->height, info->fix.line_length, 
			       size/4, par->blit_bpp,
			       PAT_COPY_ROP, dst, (u32 *) image->data, 
			       bg, fg, info);
} 

int i810fb_sync(struct fb_info *info)
{
	struct i810fb_par *par = info->par;
	
	if (!info->var.accel_flags || par->dev_flags & LOCKUP)
		return 0;

	return wait_for_engine_idle(info);
}

void i810fb_load_front(u32 offset, struct fb_info *info)
{
	struct i810fb_par *par = info->par;
	u8 __iomem *mmio = par->mmio_start_virtual;

	if (!info->var.accel_flags || par->dev_flags & LOCKUP)
		i810_writel(DPLYBASE, mmio, par->fb.physical + offset);
	else 
		load_front(offset, info);
}

void i810fb_init_ringbuffer(struct fb_info *info)
{
	struct i810fb_par *par = info->par;
	u32 tmp1, tmp2;
	u8 __iomem *mmio = par->mmio_start_virtual;
	
	wait_for_engine_idle(info);
	i810fb_iring_enable(par, OFF);
	i810_writel(IRING, mmio, 0);
	i810_writel(IRING + 4, mmio, 0);
	par->cur_tail = 0;

	tmp2 = i810_readl(IRING + 8, mmio) & ~RBUFFER_START_MASK; 
	tmp1 = par->iring.physical;
	i810_writel(IRING + 8, mmio, tmp2 | tmp1);

	tmp1 = i810_readl(IRING + 12, mmio);
	tmp1 &= ~RBUFFER_SIZE_MASK;
	tmp2 = (par->iring.size - I810_PAGESIZE) & RBUFFER_SIZE_MASK;
	i810_writel(IRING + 12, mmio, tmp1 | tmp2);
	i810fb_iring_enable(par, ON);
}
