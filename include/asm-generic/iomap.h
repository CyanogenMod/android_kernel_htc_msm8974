#ifndef __GENERIC_IO_H
#define __GENERIC_IO_H

#include <linux/linkage.h>
#include <asm/byteorder.h>


extern unsigned int ioread8(void __iomem *);
extern unsigned int ioread16(void __iomem *);
extern unsigned int ioread16be(void __iomem *);
extern unsigned int ioread32(void __iomem *);
extern unsigned int ioread32be(void __iomem *);

extern void iowrite8(u8, void __iomem *);
extern void iowrite16(u16, void __iomem *);
extern void iowrite16be(u16, void __iomem *);
extern void iowrite32(u32, void __iomem *);
extern void iowrite32be(u32, void __iomem *);

extern void ioread8_rep(void __iomem *port, void *buf, unsigned long count);
extern void ioread16_rep(void __iomem *port, void *buf, unsigned long count);
extern void ioread32_rep(void __iomem *port, void *buf, unsigned long count);

extern void iowrite8_rep(void __iomem *port, const void *buf, unsigned long count);
extern void iowrite16_rep(void __iomem *port, const void *buf, unsigned long count);
extern void iowrite32_rep(void __iomem *port, const void *buf, unsigned long count);

#ifdef CONFIG_HAS_IOPORT
extern void __iomem *ioport_map(unsigned long port, unsigned int nr);
extern void ioport_unmap(void __iomem *);
#endif

#ifndef ARCH_HAS_IOREMAP_WC
#define ioremap_wc ioremap_nocache
#endif

#ifdef CONFIG_PCI
struct pci_dev;
extern void pci_iounmap(struct pci_dev *dev, void __iomem *);
#elif defined(CONFIG_GENERIC_IOMAP)
struct pci_dev;
static inline void pci_iounmap(struct pci_dev *dev, void __iomem *addr)
{ }
#endif

#include <asm-generic/pci_iomap.h>

#endif
