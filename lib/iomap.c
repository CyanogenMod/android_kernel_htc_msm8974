/*
 * Implement the default iomap interfaces
 *
 * (C) Copyright 2004 Linus Torvalds
 */
#include <linux/pci.h>
#include <linux/io.h>

#include <linux/export.h>


#ifndef HAVE_ARCH_PIO_SIZE
#define PIO_OFFSET	0x10000UL
#define PIO_MASK	0x0ffffUL
#define PIO_RESERVED	0x40000UL
#endif

static void bad_io_access(unsigned long port, const char *access)
{
	static int count = 10;
	if (count) {
		count--;
		WARN(1, KERN_ERR "Bad IO access at port %#lx (%s)\n", port, access);
	}
}

#define IO_COND(addr, is_pio, is_mmio) do {			\
	unsigned long port = (unsigned long __force)addr;	\
	if (port >= PIO_RESERVED) {				\
		is_mmio;					\
	} else if (port > PIO_OFFSET) {				\
		port &= PIO_MASK;				\
		is_pio;						\
	} else							\
		bad_io_access(port, #is_pio );			\
} while (0)

#ifndef pio_read16be
#define pio_read16be(port) swab16(inw(port))
#define pio_read32be(port) swab32(inl(port))
#endif

#ifndef mmio_read16be
#define mmio_read16be(addr) be16_to_cpu(__raw_readw(addr))
#define mmio_read32be(addr) be32_to_cpu(__raw_readl(addr))
#endif

unsigned int ioread8(void __iomem *addr)
{
	IO_COND(addr, return inb(port), return readb(addr));
	return 0xff;
}
unsigned int ioread16(void __iomem *addr)
{
	IO_COND(addr, return inw(port), return readw(addr));
	return 0xffff;
}
unsigned int ioread16be(void __iomem *addr)
{
	IO_COND(addr, return pio_read16be(port), return mmio_read16be(addr));
	return 0xffff;
}
unsigned int ioread32(void __iomem *addr)
{
	IO_COND(addr, return inl(port), return readl(addr));
	return 0xffffffff;
}
unsigned int ioread32be(void __iomem *addr)
{
	IO_COND(addr, return pio_read32be(port), return mmio_read32be(addr));
	return 0xffffffff;
}
EXPORT_SYMBOL(ioread8);
EXPORT_SYMBOL(ioread16);
EXPORT_SYMBOL(ioread16be);
EXPORT_SYMBOL(ioread32);
EXPORT_SYMBOL(ioread32be);

#ifndef pio_write16be
#define pio_write16be(val,port) outw(swab16(val),port)
#define pio_write32be(val,port) outl(swab32(val),port)
#endif

#ifndef mmio_write16be
#define mmio_write16be(val,port) __raw_writew(be16_to_cpu(val),port)
#define mmio_write32be(val,port) __raw_writel(be32_to_cpu(val),port)
#endif

void iowrite8(u8 val, void __iomem *addr)
{
	IO_COND(addr, outb(val,port), writeb(val, addr));
}
void iowrite16(u16 val, void __iomem *addr)
{
	IO_COND(addr, outw(val,port), writew(val, addr));
}
void iowrite16be(u16 val, void __iomem *addr)
{
	IO_COND(addr, pio_write16be(val,port), mmio_write16be(val, addr));
}
void iowrite32(u32 val, void __iomem *addr)
{
	IO_COND(addr, outl(val,port), writel(val, addr));
}
void iowrite32be(u32 val, void __iomem *addr)
{
	IO_COND(addr, pio_write32be(val,port), mmio_write32be(val, addr));
}
EXPORT_SYMBOL(iowrite8);
EXPORT_SYMBOL(iowrite16);
EXPORT_SYMBOL(iowrite16be);
EXPORT_SYMBOL(iowrite32);
EXPORT_SYMBOL(iowrite32be);

#ifndef mmio_insb
static inline void mmio_insb(void __iomem *addr, u8 *dst, int count)
{
	while (--count >= 0) {
		u8 data = __raw_readb(addr);
		*dst = data;
		dst++;
	}
}
static inline void mmio_insw(void __iomem *addr, u16 *dst, int count)
{
	while (--count >= 0) {
		u16 data = __raw_readw(addr);
		*dst = data;
		dst++;
	}
}
static inline void mmio_insl(void __iomem *addr, u32 *dst, int count)
{
	while (--count >= 0) {
		u32 data = __raw_readl(addr);
		*dst = data;
		dst++;
	}
}
#endif

#ifndef mmio_outsb
static inline void mmio_outsb(void __iomem *addr, const u8 *src, int count)
{
	while (--count >= 0) {
		__raw_writeb(*src, addr);
		src++;
	}
}
static inline void mmio_outsw(void __iomem *addr, const u16 *src, int count)
{
	while (--count >= 0) {
		__raw_writew(*src, addr);
		src++;
	}
}
static inline void mmio_outsl(void __iomem *addr, const u32 *src, int count)
{
	while (--count >= 0) {
		__raw_writel(*src, addr);
		src++;
	}
}
#endif

void ioread8_rep(void __iomem *addr, void *dst, unsigned long count)
{
	IO_COND(addr, insb(port,dst,count), mmio_insb(addr, dst, count));
}
void ioread16_rep(void __iomem *addr, void *dst, unsigned long count)
{
	IO_COND(addr, insw(port,dst,count), mmio_insw(addr, dst, count));
}
void ioread32_rep(void __iomem *addr, void *dst, unsigned long count)
{
	IO_COND(addr, insl(port,dst,count), mmio_insl(addr, dst, count));
}
EXPORT_SYMBOL(ioread8_rep);
EXPORT_SYMBOL(ioread16_rep);
EXPORT_SYMBOL(ioread32_rep);

void iowrite8_rep(void __iomem *addr, const void *src, unsigned long count)
{
	IO_COND(addr, outsb(port, src, count), mmio_outsb(addr, src, count));
}
void iowrite16_rep(void __iomem *addr, const void *src, unsigned long count)
{
	IO_COND(addr, outsw(port, src, count), mmio_outsw(addr, src, count));
}
void iowrite32_rep(void __iomem *addr, const void *src, unsigned long count)
{
	IO_COND(addr, outsl(port, src,count), mmio_outsl(addr, src, count));
}
EXPORT_SYMBOL(iowrite8_rep);
EXPORT_SYMBOL(iowrite16_rep);
EXPORT_SYMBOL(iowrite32_rep);

#ifdef CONFIG_HAS_IOPORT
void __iomem *ioport_map(unsigned long port, unsigned int nr)
{
	if (port > PIO_MASK)
		return NULL;
	return (void __iomem *) (unsigned long) (port + PIO_OFFSET);
}

void ioport_unmap(void __iomem *addr)
{
	
}
EXPORT_SYMBOL(ioport_map);
EXPORT_SYMBOL(ioport_unmap);
#endif 

#ifdef CONFIG_PCI
void pci_iounmap(struct pci_dev *dev, void __iomem * addr)
{
	IO_COND(addr, , iounmap(addr));
}
EXPORT_SYMBOL(pci_iounmap);
#endif 
