#ifndef _ASM_VGA_H
#define _ASM_VGA_H

#include <asm/byteorder.h>


#define VGA_MAP_MEM(x, s)	(0xb0000000L + (unsigned long)(x))

#define vga_readb(x)	(*(x))
#define vga_writeb(x, y)	(*(y) = (x))

#define VT_BUF_HAVE_RW

#undef scr_writew
#undef scr_readw

static inline void scr_writew(u16 val, volatile u16 *addr)
{
	*addr = cpu_to_le16(val);
}

static inline u16 scr_readw(volatile const u16 *addr)
{
	return le16_to_cpu(*addr);
}

#define scr_memcpyw(d, s, c) memcpy(d, s, c)
#define scr_memmovew(d, s, c) memmove(d, s, c)
#define VT_BUF_HAVE_MEMCPYW
#define VT_BUF_HAVE_MEMMOVEW

#endif 
