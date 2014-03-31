
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/string.h>

#include <asm/io.h>

#ifdef __sparc__
#include <asm/fbio.h>
#endif

#include <video/mach64.h>
#include "atyfb.h"


static const u8 cursor_bits_lookup[16] = {
	0x00, 0x40, 0x10, 0x50, 0x04, 0x44, 0x14, 0x54,
	0x01, 0x41, 0x11, 0x51, 0x05, 0x45, 0x15, 0x55
};

static int atyfb_cursor(struct fb_info *info, struct fb_cursor *cursor)
{
	struct atyfb_par *par = (struct atyfb_par *) info->par;
	u16 xoff, yoff;
	int x, y, h;

#ifdef __sparc__
	if (par->mmaped)
		return -EPERM;
#endif
	if (par->asleep)
		return -EPERM;

	wait_for_fifo(1, par);
	if (cursor->enable)
		aty_st_le32(GEN_TEST_CNTL, aty_ld_le32(GEN_TEST_CNTL, par)
			    | HWCURSOR_ENABLE, par);
	else
		aty_st_le32(GEN_TEST_CNTL, aty_ld_le32(GEN_TEST_CNTL, par)
				& ~HWCURSOR_ENABLE, par);

	
	if (cursor->set & FB_CUR_SETPOS) {
		x = cursor->image.dx - cursor->hot.x - info->var.xoffset;
		if (x < 0) {
			xoff = -x;
			x = 0;
		} else {
			xoff = 0;
		}

		y = cursor->image.dy - cursor->hot.y - info->var.yoffset;
		if (y < 0) {
			yoff = -y;
			y = 0;
		} else {
			yoff = 0;
		}

		h = cursor->image.height;

                if (par->crtc.gen_cntl & CRTC_DBL_SCAN_EN) {
			y<<=1;
			h<<=1;
		}
		wait_for_fifo(3, par);
		aty_st_le32(CUR_OFFSET, (info->fix.smem_len >> 3) + (yoff << 1), par);
		aty_st_le32(CUR_HORZ_VERT_OFF,
			    ((u32) (64 - h + yoff) << 16) | xoff, par);
		aty_st_le32(CUR_HORZ_VERT_POSN, ((u32) y << 16) | x, par);
	}

	
	if (cursor->set & FB_CUR_SETCMAP) {
		u32 fg_idx, bg_idx, fg, bg;

		fg_idx = cursor->image.fg_color;
		bg_idx = cursor->image.bg_color;

		fg = ((info->cmap.red[fg_idx] & 0xff) << 24) |
		     ((info->cmap.green[fg_idx] & 0xff) << 16) |
		     ((info->cmap.blue[fg_idx] & 0xff) << 8) | 0xff;

		bg = ((info->cmap.red[bg_idx] & 0xff) << 24) |
		     ((info->cmap.green[bg_idx] & 0xff) << 16) |
		     ((info->cmap.blue[bg_idx] & 0xff) << 8);

		wait_for_fifo(2, par);
		aty_st_le32(CUR_CLR0, bg, par);
		aty_st_le32(CUR_CLR1, fg, par);
	}

	if (cursor->set & (FB_CUR_SETSHAPE | FB_CUR_SETIMAGE)) {
	    u8 *src = (u8 *)cursor->image.data;
	    u8 *msk = (u8 *)cursor->mask;
	    u8 __iomem *dst = (u8 __iomem *)info->sprite.addr;
	    unsigned int width = (cursor->image.width + 7) >> 3;
	    unsigned int height = cursor->image.height;
	    unsigned int align = info->sprite.scan_align;

	    unsigned int i, j, offset;
	    u8 m, b;

	    
	    fb_memset(dst, 0xaa, 1024);

	    offset = align - width*2;

	    for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			b = *src++;
			m = *msk++;
			switch (cursor->rop) {
			case ROP_XOR:
			    
			    fb_writeb(cursor_bits_lookup[(b ^ m) >> 4], dst++);
			    
			    fb_writeb(cursor_bits_lookup[(b ^ m) & 0x0f],
				      dst++);
			    break;
			case ROP_COPY:
			    
			    fb_writeb(cursor_bits_lookup[(b & m) >> 4], dst++);
			    
			    fb_writeb(cursor_bits_lookup[(b & m) & 0x0f],
				      dst++);
			    break;
			}
		}
		dst += offset;
	    }
	}

	return 0;
}

int __devinit aty_init_cursor(struct fb_info *info)
{
	unsigned long addr;

	info->fix.smem_len -= PAGE_SIZE;

#ifdef __sparc__
	addr = (unsigned long) info->screen_base - 0x800000 + info->fix.smem_len;
	info->sprite.addr = (u8 *) addr;
#else
#ifdef __BIG_ENDIAN
	addr = info->fix.smem_start - 0x800000 + info->fix.smem_len;
	info->sprite.addr = (u8 *) ioremap(addr, 1024);
#else
	addr = (unsigned long) info->screen_base + info->fix.smem_len;
	info->sprite.addr = (u8 *) addr;
#endif
#endif
	if (!info->sprite.addr)
		return -ENXIO;
	info->sprite.size = PAGE_SIZE;
	info->sprite.scan_align = 16;	
	info->sprite.buf_align = 16; 	
	info->sprite.flags = FB_PIXMAP_IO;

	info->fbops->fb_cursor = atyfb_cursor;

	return 0;
}

