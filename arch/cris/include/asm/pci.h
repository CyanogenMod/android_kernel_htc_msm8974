#ifndef __ASM_CRIS_PCI_H
#define __ASM_CRIS_PCI_H


#ifdef __KERNEL__
#include <linux/mm.h>		


#define pcibios_assign_all_busses(void) 1

extern unsigned long pci_mem_start;
#define PCIBIOS_MIN_IO		0x1000
#define PCIBIOS_MIN_MEM		0x10000000

#define PCIBIOS_MIN_CARDBUS_IO	0x4000

void pcibios_config_init(void);
struct pci_bus * pcibios_scan_root(int bus);
int pcibios_assign_resources(void);

void pcibios_set_master(struct pci_dev *dev);
void pcibios_penalize_isa_irq(int irq);
struct irq_routing_table *pcibios_get_irq_routing_table(void);
int pcibios_set_irq_routing(struct pci_dev *dev, int pin, int irq);


#include <linux/types.h>
#include <linux/slab.h>
#include <asm/scatterlist.h>
#include <linux/string.h>
#include <asm/io.h>

struct pci_dev;

#define PCI_DMA_BUS_IS_PHYS	(1)

#define HAVE_PCI_MMAP
extern int pci_mmap_page_range(struct pci_dev *dev, struct vm_area_struct *vma,
			       enum pci_mmap_state mmap_state, int write_combine);


#endif 

#include <asm-generic/pci-dma-compat.h>

#include <asm-generic/pci.h>

#endif 
