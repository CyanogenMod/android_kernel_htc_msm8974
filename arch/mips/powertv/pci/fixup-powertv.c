#include <linux/init.h>
#include <linux/export.h>
#include <linux/pci.h>
#include <asm/mach-powertv/interrupts.h>
#include "powertv-pci.h"

int __init pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	return asic_pcie_map_irq(dev, slot, pin);
}

int pcibios_plat_dev_init(struct pci_dev *dev)
{
	return 0;
}

int asic_pcie_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	return irq_pciexp;
}
EXPORT_SYMBOL(asic_pcie_map_irq);
