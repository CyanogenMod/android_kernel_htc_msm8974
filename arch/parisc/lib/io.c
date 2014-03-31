/*
 * arch/parisc/lib/io.c
 *
 * Copyright (c) Matthew Wilcox 2001 for Hewlett-Packard
 * Copyright (c) Randolph Chung 2001 <tausq@debian.org>
 *
 * IO accessing functions which shouldn't be inlined because they're too big
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/io.h>

void memcpy_toio(volatile void __iomem *dst, const void *src, int count)
{
	if (((unsigned long)dst & 3) != ((unsigned long)src & 3))
		goto bytecopy;
	while ((unsigned long)dst & 3) {
		writeb(*(char *)src, dst++);
		src++;
		count--;
	}
	while (count > 3) {
		__raw_writel(*(u32 *)src, dst);
		src += 4;
		dst += 4;
		count -= 4;
	}
 bytecopy:
	while (count--) {
		writeb(*(char *)src, dst++);
		src++;
	}
}

void memcpy_fromio(void *dst, const volatile void __iomem *src, int count)
{
	 
	if ( (((unsigned long)dst ^ (unsigned long)src) & 1) || (count < 2) )
		goto bytecopy;

	if ( (((unsigned long)dst ^ (unsigned long)src) & 2) || (count < 4) )
		goto shortcopy;

	
	if ((unsigned long)src & 1) {
		*(u8 *)dst = readb(src);
		src++;
		dst++;
		count--;
		if (count < 2) goto bytecopy;
	}

	if ((unsigned long)src & 2) {
		*(u16 *)dst = __raw_readw(src);
		src += 2;
		dst += 2;
		count -= 2;
	}

	while (count > 3) {
		*(u32 *)dst = __raw_readl(src);
		dst += 4;
		src += 4;
		count -= 4;
	}

 shortcopy:
	while (count > 1) {
		*(u16 *)dst = __raw_readw(src);
		src += 2;
		dst += 2;
		count -= 2;
	}

 bytecopy:
	while (count--) {
		*(char *)dst = readb(src);
		src++;
		dst++;
	}
}

void memset_io(volatile void __iomem *addr, unsigned char val, int count)
{
	u32 val32 = (val << 24) | (val << 16) | (val << 8) | val;
	while ((unsigned long)addr & 3) {
		writeb(val, addr++);
		count--;
	}
	while (count > 3) {
		__raw_writel(val32, addr);
		addr += 4;
		count -= 4;
	}
	while (count--) {
		writeb(val, addr++);
	}
}

void insb (unsigned long port, void *dst, unsigned long count)
{
	unsigned char *p;

	p = (unsigned char *)dst;

	while (((unsigned long)p) & 0x3) {
		if (!count)
			return;
		count--;
		*p = inb(port);
		p++;
	}

	while (count >= 4) {
		unsigned int w;
		count -= 4;
		w = inb(port) << 24;
		w |= inb(port) << 16;
		w |= inb(port) << 8;
		w |= inb(port);
		*(unsigned int *) p = w;
		p += 4;
	}

	while (count) {
		--count;
		*p = inb(port);
		p++;
	}
}


void insw (unsigned long port, void *dst, unsigned long count)
{
	unsigned int l = 0, l2;
	unsigned char *p;

	p = (unsigned char *)dst;
	
	if (!count)
		return;
	
	switch (((unsigned long)p) & 0x3)
	{
	 case 0x00:			
		while (count>=2) {
			
			count -= 2;
			l = cpu_to_le16(inw(port)) << 16;
			l |= cpu_to_le16(inw(port));
			*(unsigned int *)p = l;
			p += 4;
		}
		if (count) {
			*(unsigned short *)p = cpu_to_le16(inw(port));
		}
		break;
	
	 case 0x02:			
		*(unsigned short *)p = cpu_to_le16(inw(port));
		p += 2;
		count--;
		while (count>=2) {
			
			count -= 2;
			l = cpu_to_le16(inw(port)) << 16;
			l |= cpu_to_le16(inw(port));
			*(unsigned int *)p = l;
			p += 4;
		}
		if (count) {
			*(unsigned short *)p = cpu_to_le16(inw(port));
		}
		break;
		
	 case 0x01:			
	 case 0x03:
		--count;
		
		l = cpu_to_le16(inw(port));
		*p = l >> 8;
		p++;
		while (count--)
		{
			l2 = cpu_to_le16(inw(port));
			*(unsigned short *)p = (l & 0xff) << 8 | (l2 >> 8);
			p += 2;
			l = l2;
		}
		*p = l & 0xff;
		break;
	}
}



void insl (unsigned long port, void *dst, unsigned long count)
{
	unsigned int l = 0, l2;
	unsigned char *p;

	p = (unsigned char *)dst;
	
	if (!count)
		return;
	
	switch (((unsigned long) dst) & 0x3)
	{
	 case 0x00:			
		while (count--)
		{
			*(unsigned int *)p = cpu_to_le32(inl(port));
			p += 4;
		}
		break;
	
	 case 0x02:			
		--count;
		
		l = cpu_to_le32(inl(port));
		*(unsigned short *)p = l >> 16;
		p += 2;
		
		while (count--)
		{
			l2 = cpu_to_le32(inl(port));
			*(unsigned int *)p = (l & 0xffff) << 16 | (l2 >> 16);
			p += 4;
			l = l2;
		}
		*(unsigned short *)p = l & 0xffff;
		break;
	 case 0x01:			
		--count;
		
		l = cpu_to_le32(inl(port));
		*(unsigned char *)p = l >> 24;
		p++;
		*(unsigned short *)p = (l >> 8) & 0xffff;
		p += 2;
		while (count--)
		{
			l2 = cpu_to_le32(inl(port));
			*(unsigned int *)p = (l & 0xff) << 24 | (l2 >> 8);
			p += 4;
			l = l2;
		}
		*p = l & 0xff;
		break;
	 case 0x03:			
		--count;
		
		l = cpu_to_le32(inl(port));
		*p = l >> 24;
		p++;
		while (count--)
		{
			l2 = cpu_to_le32(inl(port));
			*(unsigned int *)p = (l & 0xffffff) << 8 | l2 >> 24;
			p += 4;
			l = l2;
		}
		*(unsigned short *)p = (l >> 8) & 0xffff;
		p += 2;
		*p = l & 0xff;
		break;
	}
}


void outsb(unsigned long port, const void * src, unsigned long count)
{
	const unsigned char *p;

	p = (const unsigned char *)src;
	while (count) {
		count--;
		outb(*p, port);
		p++;
	}
}

void outsw (unsigned long port, const void *src, unsigned long count)
{
	unsigned int l = 0, l2;
	const unsigned char *p;

	p = (const unsigned char *)src;
	
	if (!count)
		return;
	
	switch (((unsigned long)p) & 0x3)
	{
	 case 0x00:			
		while (count>=2) {
			count -= 2;
			l = *(unsigned int *)p;
			p += 4;
			outw(le16_to_cpu(l >> 16), port);
			outw(le16_to_cpu(l & 0xffff), port);
		}
		if (count) {
			outw(le16_to_cpu(*(unsigned short*)p), port);
		}
		break;
	
	 case 0x02:			
		
		outw(le16_to_cpu(*(unsigned short*)p), port);
		p += 2;
		count--;
		
		while (count>=2) {
			count -= 2;
			l = *(unsigned int *)p;
			p += 4;
			outw(le16_to_cpu(l >> 16), port);
			outw(le16_to_cpu(l & 0xffff), port);
		}
		if (count) {
			outw(le16_to_cpu(*(unsigned short *)p), port);
		}
		break;
		
	 case 0x01:				
		
		l  = *p << 8;
		p++;
		count--;
		while (count)
		{
			count--;
			l2 = *(unsigned short *)p;
			p += 2;
			outw(le16_to_cpu(l | l2 >> 8), port);
		        l = l2 << 8;
		}
		l2 = *(unsigned char *)p;
		outw (le16_to_cpu(l | l2>>8), port);
		break;
	
	}
}


void outsl (unsigned long port, const void *src, unsigned long count)
{
	unsigned int l = 0, l2;
	const unsigned char *p;

	p = (const unsigned char *)src;
	
	if (!count)
		return;
	
	switch (((unsigned long)p) & 0x3)
	{
	 case 0x00:			
		while (count--)
		{
			outl(le32_to_cpu(*(unsigned int *)p), port);
			p += 4;
		}
		break;
	
	 case 0x02:			
		--count;
		
		l = *(unsigned short *)p;
		p += 2;
		
		while (count--)
		{
			l2 = *(unsigned int *)p;
			p += 4;
			outl (le32_to_cpu(l << 16 | l2 >> 16), port);
			l = l2;
		}
		l2 = *(unsigned short *)p;
		outl (le32_to_cpu(l << 16 | l2), port);
		break;
	 case 0x01:			
		--count;

		l = *p << 24;
		p++;
		l |= *(unsigned short *)p << 8;
		p += 2;

		while (count--)
		{
			l2 = *(unsigned int *)p;
			p += 4;
			outl (le32_to_cpu(l | l2 >> 24), port);
			l = l2 << 8;
		}
		l2 = *p;
		outl (le32_to_cpu(l | l2), port);
		break;
	 case 0x03:			
		--count;
		
		l = *p << 24;
		p++;

		while (count--)
		{
			l2 = *(unsigned int *)p;
			p += 4;
			outl (le32_to_cpu(l | l2 >> 8), port);
			l = l2 << 24;
		}
		l2 = *(unsigned short *)p << 16;
		p += 2;
		l2 |= *p;
		outl (le32_to_cpu(l | l2), port);
		break;
	}
}

EXPORT_SYMBOL(insb);
EXPORT_SYMBOL(insw);
EXPORT_SYMBOL(insl);
EXPORT_SYMBOL(outsb);
EXPORT_SYMBOL(outsw);
EXPORT_SYMBOL(outsl);
