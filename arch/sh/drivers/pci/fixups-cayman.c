#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/types.h>
#include <cpu/irq.h>
#include "pci-sh5.h"

int __init pcibios_map_platform_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	int result = -1;


	struct slot_pin {
		int slot;
		int pin;
	} path[4];
	int i=0;

	while (dev->bus->number > 0) {

		slot = path[i].slot = PCI_SLOT(dev->devfn);
		pin = path[i].pin = pci_swizzle_interrupt_pin(dev, pin);
		dev = dev->bus->self;
		i++;
		if (i > 3) panic("PCI path to root bus too long!\n");
	}

	slot = PCI_SLOT(dev->devfn);

	
	if ((slot < 3) || (i == 0)) {
		result = IRQ_INTA + pci_swizzle_interrupt_pin(dev, pin) - 1;
	} else {
		i--;
		slot = path[i].slot;
		pin  = path[i].pin;
		if (slot > 0) {
			panic("PCI expansion bus device found - not handled!\n");
		} else {
			if (i > 0) {
				
				i--;
				slot = path[i].slot;
				pin  = path[i].pin;
				
				result = IRQ_P2INTA + (pin - 1);
			} else {
				
				result = -1;
			}
		}
	}

	return result;
}
