#ifndef _ASM_POWERPC_VGA_H_
#define _ASM_POWERPC_VGA_H_

#ifdef __KERNEL__



#include <asm/io.h>


#if defined(CONFIG_VGA_CONSOLE) || defined(CONFIG_MDA_CONSOLE)

#define VT_BUF_HAVE_RW

static inline void scr_writew(u16 val, volatile u16 *addr)
{
    st_le16(addr, val);
}

static inline u16 scr_readw(volatile const u16 *addr)
{
    return ld_le16(addr);
}

#define VT_BUF_HAVE_MEMCPYW
#define scr_memcpyw	memcpy

#endif 

extern unsigned long vgacon_remap_base;

#ifdef __powerpc64__
#define VGA_MAP_MEM(x,s) ((unsigned long) ioremap((x), s))
#else
#define VGA_MAP_MEM(x,s) (x + vgacon_remap_base)
#endif

#define vga_readb(x) (*(x))
#define vga_writeb(x,y) (*(y) = (x))

#endif	
#endif	
