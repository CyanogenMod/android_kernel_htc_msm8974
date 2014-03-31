#ifndef __ALPHA_JENSEN_H
#define __ALPHA_JENSEN_H

#include <asm/compiler.h>



#define EISA_INTA		(IDENT_ADDR + 0x100000000UL)

#define EISA_FEPROM0		(IDENT_ADDR + 0x180000000UL)
#define EISA_FEPROM1		(IDENT_ADDR + 0x1A0000000UL)

#define EISA_VL82C106		(IDENT_ADDR + 0x1C0000000UL)

#define EISA_HAE		(IDENT_ADDR + 0x1D0000000UL)

#define EISA_SYSCTL		(IDENT_ADDR + 0x1E0000000UL)

#define EISA_SPARE		(IDENT_ADDR + 0x1F0000000UL)

#define EISA_MEM		(IDENT_ADDR + 0x200000000UL)

#define EISA_IO			(IDENT_ADDR + 0x300000000UL)


#ifdef __KERNEL__

#ifndef __EXTERN_INLINE
#define __EXTERN_INLINE extern inline
#define __IO_EXTERN_INLINE
#endif


#define JENSEN_HAE_ADDRESS	EISA_HAE
#define JENSEN_HAE_MASK		0x1ffffff

__EXTERN_INLINE void jensen_set_hae(unsigned long addr)
{
	
	addr >>= 25;
	if (addr != alpha_mv.hae_cache)
		set_hae(addr);
}

#define vuip	volatile unsigned int *


static inline unsigned int jensen_local_inb(unsigned long addr)
{
	return 0xff & *(vuip)((addr << 9) + EISA_VL82C106);
}

static inline void jensen_local_outb(u8 b, unsigned long addr)
{
	*(vuip)((addr << 9) + EISA_VL82C106) = b;
	mb();
}

static inline unsigned int jensen_bus_inb(unsigned long addr)
{
	long result;

	jensen_set_hae(0);
	result = *(volatile int *)((addr << 7) + EISA_IO + 0x00);
	return __kernel_extbl(result, addr & 3);
}

static inline void jensen_bus_outb(u8 b, unsigned long addr)
{
	jensen_set_hae(0);
	*(vuip)((addr << 7) + EISA_IO + 0x00) = b * 0x01010101;
	mb();
}


#define jensen_is_local(addr) ( \
	(addr == 0x60 || addr == 0x64) || \
	(addr == 0x170 || addr == 0x171) || \
	(addr >= 0x2f8 && addr <= 0x2ff) || \
	(addr >= 0x3bc && addr <= 0x3be) || \
	(addr >= 0x3f8 && addr <= 0x3ff))

__EXTERN_INLINE u8 jensen_inb(unsigned long addr)
{
	if (jensen_is_local(addr))
		return jensen_local_inb(addr);
	else
		return jensen_bus_inb(addr);
}

__EXTERN_INLINE void jensen_outb(u8 b, unsigned long addr)
{
	if (jensen_is_local(addr))
		jensen_local_outb(b, addr);
	else
		jensen_bus_outb(b, addr);
}

__EXTERN_INLINE u16 jensen_inw(unsigned long addr)
{
	long result;

	jensen_set_hae(0);
	result = *(volatile int *) ((addr << 7) + EISA_IO + 0x20);
	result >>= (addr & 3) * 8;
	return 0xffffUL & result;
}

__EXTERN_INLINE u32 jensen_inl(unsigned long addr)
{
	jensen_set_hae(0);
	return *(vuip) ((addr << 7) + EISA_IO + 0x60);
}

__EXTERN_INLINE void jensen_outw(u16 b, unsigned long addr)
{
	jensen_set_hae(0);
	*(vuip) ((addr << 7) + EISA_IO + 0x20) = b * 0x00010001;
	mb();
}

__EXTERN_INLINE void jensen_outl(u32 b, unsigned long addr)
{
	jensen_set_hae(0);
	*(vuip) ((addr << 7) + EISA_IO + 0x60) = b;
	mb();
}


__EXTERN_INLINE u8 jensen_readb(const volatile void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr;
	long result;

	jensen_set_hae(addr);
	addr &= JENSEN_HAE_MASK;
	result = *(volatile int *) ((addr << 7) + EISA_MEM + 0x00);
	result >>= (addr & 3) * 8;
	return 0xffUL & result;
}

__EXTERN_INLINE u16 jensen_readw(const volatile void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr;
	long result;

	jensen_set_hae(addr);
	addr &= JENSEN_HAE_MASK;
	result = *(volatile int *) ((addr << 7) + EISA_MEM + 0x20);
	result >>= (addr & 3) * 8;
	return 0xffffUL & result;
}

__EXTERN_INLINE u32 jensen_readl(const volatile void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr;
	jensen_set_hae(addr);
	addr &= JENSEN_HAE_MASK;
	return *(vuip) ((addr << 7) + EISA_MEM + 0x60);
}

__EXTERN_INLINE u64 jensen_readq(const volatile void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr;
	unsigned long r0, r1;

	jensen_set_hae(addr);
	addr &= JENSEN_HAE_MASK;
	addr = (addr << 7) + EISA_MEM + 0x60;
	r0 = *(vuip) (addr);
	r1 = *(vuip) (addr + (4 << 7));
	return r1 << 32 | r0;
}

__EXTERN_INLINE void jensen_writeb(u8 b, volatile void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr;
	jensen_set_hae(addr);
	addr &= JENSEN_HAE_MASK;
	*(vuip) ((addr << 7) + EISA_MEM + 0x00) = b * 0x01010101;
}

__EXTERN_INLINE void jensen_writew(u16 b, volatile void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr;
	jensen_set_hae(addr);
	addr &= JENSEN_HAE_MASK;
	*(vuip) ((addr << 7) + EISA_MEM + 0x20) = b * 0x00010001;
}

__EXTERN_INLINE void jensen_writel(u32 b, volatile void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr;
	jensen_set_hae(addr);
	addr &= JENSEN_HAE_MASK;
	*(vuip) ((addr << 7) + EISA_MEM + 0x60) = b;
}

__EXTERN_INLINE void jensen_writeq(u64 b, volatile void __iomem *xaddr)
{
	unsigned long addr = (unsigned long) xaddr;
	jensen_set_hae(addr);
	addr &= JENSEN_HAE_MASK;
	addr = (addr << 7) + EISA_MEM + 0x60;
	*(vuip) (addr) = b;
	*(vuip) (addr + (4 << 7)) = b >> 32;
}

__EXTERN_INLINE void __iomem *jensen_ioportmap(unsigned long addr)
{
	return (void __iomem *)addr;
}

__EXTERN_INLINE void __iomem *jensen_ioremap(unsigned long addr,
					     unsigned long size)
{
	return (void __iomem *)(addr + 0x100000000ul);
}

__EXTERN_INLINE int jensen_is_ioaddr(unsigned long addr)
{
	return (long)addr >= 0;
}

__EXTERN_INLINE int jensen_is_mmio(const volatile void __iomem *addr)
{
	return (unsigned long)addr >= 0x100000000ul;
}


#define IOPORT(OS, NS)							\
__EXTERN_INLINE unsigned int jensen_ioread##NS(void __iomem *xaddr)	\
{									\
	if (jensen_is_mmio(xaddr))					\
		return jensen_read##OS(xaddr - 0x100000000ul);		\
	else								\
		return jensen_in##OS((unsigned long)xaddr);		\
}									\
__EXTERN_INLINE void jensen_iowrite##NS(u##NS b, void __iomem *xaddr)	\
{									\
	if (jensen_is_mmio(xaddr))					\
		jensen_write##OS(b, xaddr - 0x100000000ul);		\
	else								\
		jensen_out##OS(b, (unsigned long)xaddr);		\
}

IOPORT(b, 8)
IOPORT(w, 16)
IOPORT(l, 32)

#undef IOPORT

#undef vuip

#undef __IO_PREFIX
#define __IO_PREFIX		jensen
#define jensen_trivial_rw_bw	0
#define jensen_trivial_rw_lq	0
#define jensen_trivial_io_bw	0
#define jensen_trivial_io_lq	0
#define jensen_trivial_iounmap	1
#include <asm/io_trivial.h>

#ifdef __IO_EXTERN_INLINE
#undef __EXTERN_INLINE
#undef __IO_EXTERN_INLINE
#endif

#endif 

#endif 
