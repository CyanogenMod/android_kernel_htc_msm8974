
#ifndef __ASM_IA64_VGA_H_
#define __ASM_IA64_VGA_H_


extern unsigned long vga_console_iobase;
extern unsigned long vga_console_membase;

#define VGA_MAP_MEM(x,s)	((unsigned long) ioremap_nocache(vga_console_membase + (x), s))

#define vga_readb(x)	(*(x))
#define vga_writeb(x,y)	(*(y) = (x))

#endif 
