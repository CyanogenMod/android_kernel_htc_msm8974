#ifndef __SPARC_PCI_H
#define __SPARC_PCI_H

#ifdef __KERNEL__

#include <linux/dma-mapping.h>

#define pcibios_assign_all_busses()	0

#define PCIBIOS_MIN_IO		0UL
#define PCIBIOS_MIN_MEM		0UL

#define PCI_IRQ_NONE		0xffffffff

static inline void pcibios_penalize_isa_irq(int irq, int active)
{
	
}

#define PCI_DMA_BUS_IS_PHYS	(0)

struct pci_dev;

#ifdef CONFIG_PCI
static inline void pci_dma_burst_advice(struct pci_dev *pdev,
					enum pci_dma_burst_strategy *strat,
					unsigned long *strategy_parameter)
{
	*strat = PCI_DMA_BURST_INFINITY;
	*strategy_parameter = ~0UL;
}
#endif

#endif 

#ifndef CONFIG_LEON_PCI
#include <asm-generic/pci.h>
#else

static inline int pci_get_legacy_ide_irq(struct pci_dev *dev, int channel)
{
	return PCI_IRQ_NONE;
}
#endif

#endif 
