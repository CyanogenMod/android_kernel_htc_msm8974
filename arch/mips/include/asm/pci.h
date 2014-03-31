/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#ifndef _ASM_PCI_H
#define _ASM_PCI_H

#include <linux/mm.h>

#ifdef __KERNEL__


#include <linux/ioport.h>

struct pci_controller {
	struct pci_controller *next;
	struct pci_bus *bus;

	struct pci_ops *pci_ops;
	struct resource *mem_resource;
	unsigned long mem_offset;
	struct resource *io_resource;
	unsigned long io_offset;
	unsigned long io_map_base;

	unsigned int index;
	unsigned int need_domain_info;

	int iommu;

	int (*get_busno)(void);
	void (*set_busno)(int busno);
};

extern struct pci_controller * alloc_pci_controller(void);
extern void register_pci_controller(struct pci_controller *hose);

extern int pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin);



extern unsigned int pcibios_assign_all_busses(void);

extern unsigned long PCIBIOS_MIN_IO;
extern unsigned long PCIBIOS_MIN_MEM;

#define PCIBIOS_MIN_CARDBUS_IO	0x4000

extern void pcibios_set_master(struct pci_dev *dev);

static inline void pcibios_penalize_isa_irq(int irq, int active)
{
	
}

#define HAVE_PCI_MMAP

extern int pci_mmap_page_range(struct pci_dev *dev, struct vm_area_struct *vma,
	enum pci_mmap_state mmap_state, int write_combine);


#include <linux/types.h>
#include <linux/slab.h>
#include <asm/scatterlist.h>
#include <linux/string.h>
#include <asm/io.h>
#include <asm-generic/pci-bridge.h>

struct pci_dev;

extern unsigned int PCI_DMA_BUS_IS_PHYS;

#ifdef CONFIG_PCI
static inline void pci_dma_burst_advice(struct pci_dev *pdev,
					enum pci_dma_burst_strategy *strat,
					unsigned long *strategy_parameter)
{
	*strat = PCI_DMA_BURST_INFINITY;
	*strategy_parameter = ~0UL;
}
#endif

#define pci_domain_nr(bus) ((struct pci_controller *)(bus)->sysdata)->index

static inline int pci_proc_domain(struct pci_bus *bus)
{
	struct pci_controller *hose = bus->sysdata;
	return hose->need_domain_info;
}

#endif 

#include <asm-generic/pci-dma-compat.h>

extern int pcibios_plat_dev_init(struct pci_dev *dev);

static inline int pci_get_legacy_ide_irq(struct pci_dev *dev, int channel)
{
	return channel ? 15 : 14;
}

#ifdef CONFIG_CPU_CAVIUM_OCTEON
#define arch_setup_msi_irqs arch_setup_msi_irqs
#endif

extern char * (*pcibios_plat_setup)(char *str);

#endif 
