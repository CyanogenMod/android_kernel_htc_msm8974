/*
 *  Fast C2P (Chunky-to-Planar) Conversion
 *
 *  Copyright (C) 2003-2008 Geert Uytterhoeven
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive
 *  for more details.
 */

#include <linux/module.h>
#include <linux/string.h>

#include <asm/unaligned.h>

#include "c2p.h"
#include "c2p_core.h"



static void c2p_32x8(u32 d[8])
{
	transp8(d, 16, 4);
	transp8(d, 8, 2);
	transp8(d, 4, 1);
	transp8(d, 2, 4);
	transp8(d, 1, 2);
}



static const int perm_c2p_32x8[8] = { 7, 5, 3, 1, 6, 4, 2, 0 };



static inline void store_planar(void *dst, u32 dst_inc, u32 bpp, u32 d[8])
{
	int i;

	for (i = 0; i < bpp; i++, dst += dst_inc)
		put_unaligned_be32(d[perm_c2p_32x8[i]], dst);
}



static inline void store_planar_masked(void *dst, u32 dst_inc, u32 bpp,
				       u32 d[8], u32 mask)
{
	int i;

	for (i = 0; i < bpp; i++, dst += dst_inc)
		put_unaligned_be32(comp(d[perm_c2p_32x8[i]],
					get_unaligned_be32(dst), mask),
				   dst);
}



void c2p_planar(void *dst, const void *src, u32 dx, u32 dy, u32 width,
		u32 height, u32 dst_nextline, u32 dst_nextplane,
		u32 src_nextline, u32 bpp)
{
	union {
		u8 pixels[32];
		u32 words[8];
	} d;
	u32 dst_idx, first, last, w;
	const u8 *c;
	void *p;

	dst += dy*dst_nextline+(dx & ~31);
	dst_idx = dx % 32;
	first = 0xffffffffU >> dst_idx;
	last = ~(0xffffffffU >> ((dst_idx+width) % 32));
	while (height--) {
		c = src;
		p = dst;
		w = width;
		if (dst_idx+width <= 32) {
			
			first &= last;
			memset(d.pixels, 0, sizeof(d));
			memcpy(d.pixels+dst_idx, c, width);
			c += width;
			c2p_32x8(d.words);
			store_planar_masked(p, dst_nextplane, bpp, d.words,
					    first);
			p += 4;
		} else {
			
			w = width;
			
			if (dst_idx) {
				w = 32 - dst_idx;
				memset(d.pixels, 0, dst_idx);
				memcpy(d.pixels+dst_idx, c, w);
				c += w;
				c2p_32x8(d.words);
				store_planar_masked(p, dst_nextplane, bpp,
						    d.words, first);
				p += 4;
				w = width-w;
			}
			
			while (w >= 32) {
				memcpy(d.pixels, c, 32);
				c += 32;
				c2p_32x8(d.words);
				store_planar(p, dst_nextplane, bpp, d.words);
				p += 4;
				w -= 32;
			}
			
			w %= 32;
			if (w > 0) {
				memcpy(d.pixels, c, w);
				memset(d.pixels+w, 0, 32-w);
				c2p_32x8(d.words);
				store_planar_masked(p, dst_nextplane, bpp,
						    d.words, last);
			}
		}
		src += src_nextline;
		dst += dst_nextline;
	}
}
EXPORT_SYMBOL_GPL(c2p_planar);

MODULE_LICENSE("GPL");
