#ifndef _ASM_MICROBLAZE_PCI_BRIDGE_H
#define _ASM_MICROBLAZE_PCI_BRIDGE_H
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

struct device_node;

#ifdef CONFIG_PCI
extern struct list_head hose_list;
extern int pcibios_vaddr_is_ioport(void __iomem *address);
#else
static inline int pcibios_vaddr_is_ioport(void __iomem *address)
{
	return 0;
}
#endif

struct pci_controller {
	struct pci_bus *bus;
	char is_dynamic;
	struct device_node *dn;
	struct list_head list_node;
	struct device *parent;

	int first_busno;
	int last_busno;

	int self_busno;

	void __iomem *io_base_virt;
	resource_size_t io_base_phys;

	resource_size_t pci_io_size;

	resource_size_t pci_mem_offset;

	resource_size_t isa_mem_phys;
	resource_size_t isa_mem_size;

	struct pci_ops *ops;
	unsigned int __iomem *cfg_addr;
	void __iomem *cfg_data;

#define INDIRECT_TYPE_SET_CFG_TYPE		0x00000001
#define INDIRECT_TYPE_EXT_REG		0x00000002
#define INDIRECT_TYPE_SURPRESS_PRIMARY_BUS	0x00000004
#define INDIRECT_TYPE_NO_PCIE_LINK		0x00000008
#define INDIRECT_TYPE_BIG_ENDIAN		0x00000010
#define INDIRECT_TYPE_BROKEN_MRM		0x00000020
	u32 indirect_type;

	struct resource io_resource;
	struct resource mem_resources[3];
	int global_number;	
};

#ifdef CONFIG_PCI
static inline struct pci_controller *pci_bus_to_host(const struct pci_bus *bus)
{
	return bus->sysdata;
}

static inline int isa_vaddr_is_ioport(void __iomem *address)
{
	return 0;
}
#endif 

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

extern void setup_indirect_pci(struct pci_controller *hose,
			       resource_size_t cfg_addr,
			       resource_size_t cfg_data, u32 flags);

extern struct pci_controller *pci_find_hose_for_OF_device(
			struct device_node *node);

extern void pci_process_bridge_OF_ranges(struct pci_controller *hose,
			struct device_node *dev, int primary);

extern struct pci_controller *pcibios_alloc_controller(struct device_node *dev);
extern void pcibios_free_controller(struct pci_controller *phb);

#endif	
#endif	
