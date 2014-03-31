#ifndef _ASM_POWERPC_PCI_BRIDGE_H
#define _ASM_POWERPC_PCI_BRIDGE_H
#ifdef __KERNEL__
/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <linux/pci.h>
#include <linux/list.h>
#include <linux/ioport.h>
#include <asm-generic/pci-bridge.h>

struct device_node;

struct pci_controller {
	struct pci_bus *bus;
	char is_dynamic;
#ifdef CONFIG_PPC64
	int node;
#endif
	struct device_node *dn;
	struct list_head list_node;
	struct device *parent;

	int first_busno;
	int last_busno;
	int self_busno;

	void __iomem *io_base_virt;
#ifdef CONFIG_PPC64
	void *io_base_alloc;
#endif
	resource_size_t io_base_phys;
	resource_size_t pci_io_size;

	resource_size_t pci_mem_offset;

	resource_size_t	isa_mem_phys;
	resource_size_t	isa_mem_size;

	struct pci_ops *ops;
	unsigned int __iomem *cfg_addr;
	void __iomem *cfg_data;

#define PPC_INDIRECT_TYPE_SET_CFG_TYPE		0x00000001
#define PPC_INDIRECT_TYPE_EXT_REG		0x00000002
#define PPC_INDIRECT_TYPE_SURPRESS_PRIMARY_BUS	0x00000004
#define PPC_INDIRECT_TYPE_NO_PCIE_LINK		0x00000008
#define PPC_INDIRECT_TYPE_BIG_ENDIAN		0x00000010
#define PPC_INDIRECT_TYPE_BROKEN_MRM		0x00000020
	u32 indirect_type;
	struct resource	io_resource;
	struct resource mem_resources[3];
	int global_number;		

	resource_size_t dma_window_base_cur;
	resource_size_t dma_window_size;

#ifdef CONFIG_PPC64
	unsigned long buid;

	void *private_data;
#endif	
};

extern int early_read_config_byte(struct pci_controller *hose, int bus,
			int dev_fn, int where, u8 *val);
extern int early_read_config_word(struct pci_controller *hose, int bus,
			int dev_fn, int where, u16 *val);
extern int early_read_config_dword(struct pci_controller *hose, int bus,
			int dev_fn, int where, u32 *val);
extern int early_write_config_byte(struct pci_controller *hose, int bus,
			int dev_fn, int where, u8 val);
extern int early_write_config_word(struct pci_controller *hose, int bus,
			int dev_fn, int where, u16 val);
extern int early_write_config_dword(struct pci_controller *hose, int bus,
			int dev_fn, int where, u32 val);

extern int early_find_capability(struct pci_controller *hose, int bus,
				 int dev_fn, int cap);

extern void setup_indirect_pci(struct pci_controller* hose,
			       resource_size_t cfg_addr,
			       resource_size_t cfg_data, u32 flags);

static inline struct pci_controller *pci_bus_to_host(const struct pci_bus *bus)
{
	return bus->sysdata;
}

#ifndef CONFIG_PPC64

extern int pci_device_from_OF_node(struct device_node *node,
				   u8 *bus, u8 *devfn);
extern void pci_create_OF_bus_map(void);

static inline int isa_vaddr_is_ioport(void __iomem *address)
{
	return 0;
}

#else	

struct iommu_table;

struct pci_dn {
	int	busno;			
	int	devfn;			

	struct  pci_controller *phb;	
	struct	iommu_table *iommu_table;	
	struct	device_node *node;	

	int	pci_ext_config_space;	

	struct	pci_dev *pcidev;	
#ifdef CONFIG_EEH
	struct eeh_dev *edev;		
#endif
#define IODA_INVALID_PE		(-1)
#ifdef CONFIG_PPC_POWERNV
	int	pe_number;
#endif
};

#define PCI_DN(dn)	((struct pci_dn *) (dn)->data)

extern void * update_dn_pci_info(struct device_node *dn, void *data);

static inline int pci_device_from_OF_node(struct device_node *np,
					  u8 *bus, u8 *devfn)
{
	if (!PCI_DN(np))
		return -ENODEV;
	*bus = PCI_DN(np)->busno;
	*devfn = PCI_DN(np)->devfn;
	return 0;
}

#if defined(CONFIG_EEH)
static inline struct eeh_dev *of_node_to_eeh_dev(struct device_node *dn)
{
	return PCI_DN(dn)->edev;
}
#endif

extern struct pci_bus *pcibios_find_pci_bus(struct device_node *dn);

extern void pcibios_remove_pci_devices(struct pci_bus *bus);

extern void pcibios_add_pci_devices(struct pci_bus *bus);


extern void isa_bridge_find_early(struct pci_controller *hose);

static inline int isa_vaddr_is_ioport(void __iomem *address)
{
	
	unsigned long ea = (unsigned long)address;
	return ea >= ISA_IO_BASE && ea < ISA_IO_END;
}

extern int pcibios_unmap_io_space(struct pci_bus *bus);
extern int pcibios_map_io_space(struct pci_bus *bus);

#ifdef CONFIG_NUMA
#define PHB_SET_NODE(PHB, NODE)		((PHB)->node = (NODE))
#else
#define PHB_SET_NODE(PHB, NODE)		((PHB)->node = -1)
#endif

#endif	

extern struct pci_controller *pci_find_hose_for_OF_device(
			struct device_node* node);

extern void pci_process_bridge_OF_ranges(struct pci_controller *hose,
			struct device_node *dev, int primary);

extern struct pci_controller *pcibios_alloc_controller(struct device_node *dev);
extern void pcibios_free_controller(struct pci_controller *phb);

#ifdef CONFIG_PCI
extern int pcibios_vaddr_is_ioport(void __iomem *address);
#else
static inline int pcibios_vaddr_is_ioport(void __iomem *address)
{
	return 0;
}
#endif	

#endif	
#endif	
