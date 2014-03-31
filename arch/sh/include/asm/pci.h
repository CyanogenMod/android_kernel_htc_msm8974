#ifndef __ASM_SH_PCI_H
#define __ASM_SH_PCI_H

#ifdef __KERNEL__


#define pcibios_assign_all_busses()	1

struct pci_channel {
	struct pci_channel	*next;
	struct pci_bus		*bus;

	struct pci_ops		*pci_ops;

	struct resource		*resources;
	unsigned int		nr_resources;

	unsigned long		io_offset;
	unsigned long		mem_offset;

	unsigned long		reg_base;
	unsigned long		io_map_base;

	unsigned int		index;
	unsigned int		need_domain_info;

	
	struct timer_list	err_timer, serr_timer;
	unsigned int		err_irq, serr_irq;
};

extern raw_spinlock_t pci_config_lock;

extern int register_pci_controller(struct pci_channel *hose);
extern void pcibios_report_status(unsigned int status_mask, int warn);

extern int early_read_config_byte(struct pci_channel *hose, int top_bus,
				  int bus, int devfn, int offset, u8 *value);
extern int early_read_config_word(struct pci_channel *hose, int top_bus,
				  int bus, int devfn, int offset, u16 *value);
extern int early_read_config_dword(struct pci_channel *hose, int top_bus,
				   int bus, int devfn, int offset, u32 *value);
extern int early_write_config_byte(struct pci_channel *hose, int top_bus,
				   int bus, int devfn, int offset, u8 value);
extern int early_write_config_word(struct pci_channel *hose, int top_bus,
				   int bus, int devfn, int offset, u16 value);
extern int early_write_config_dword(struct pci_channel *hose, int top_bus,
				    int bus, int devfn, int offset, u32 value);
extern void pcibios_enable_timers(struct pci_channel *hose);
extern unsigned int pcibios_handle_status_errors(unsigned long addr,
				 unsigned int status, struct pci_channel *hose);
extern int pci_is_66mhz_capable(struct pci_channel *hose,
				int top_bus, int current_bus);

extern unsigned long PCIBIOS_MIN_IO, PCIBIOS_MIN_MEM;

struct pci_dev;

#define HAVE_PCI_MMAP
extern int pci_mmap_page_range(struct pci_dev *dev, struct vm_area_struct *vma,
	enum pci_mmap_state mmap_state, int write_combine);
extern void pcibios_set_master(struct pci_dev *dev);

static inline void pcibios_penalize_isa_irq(int irq, int active)
{
	
}


#define PCI_DMA_BUS_IS_PHYS	(dma_ops->is_phys)

#ifdef CONFIG_PCI
#define PCI_DISABLE_MWI

static inline void pci_dma_burst_advice(struct pci_dev *pdev,
					enum pci_dma_burst_strategy *strat,
					unsigned long *strategy_parameter)
{
	unsigned long cacheline_size;
	u8 byte;

	pci_read_config_byte(pdev, PCI_CACHE_LINE_SIZE, &byte);

	if (byte == 0)
		cacheline_size = L1_CACHE_BYTES;
	else
		cacheline_size = byte << 2;

	*strat = PCI_DMA_BURST_MULTIPLE;
	*strategy_parameter = cacheline_size;
}
#endif

int pcibios_map_platform_irq(const struct pci_dev *dev, u8 slot, u8 pin);

#define pci_domain_nr(bus) ((struct pci_channel *)(bus)->sysdata)->index

static inline int pci_proc_domain(struct pci_bus *bus)
{
	struct pci_channel *hose = bus->sysdata;
	return hose->need_domain_info;
}

static inline int pci_get_legacy_ide_irq(struct pci_dev *dev, int channel)
{
	return channel ? 15 : 14;
}

#include <asm-generic/pci-dma-compat.h>

#endif 
#endif 

