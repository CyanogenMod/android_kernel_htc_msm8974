/*
 *	drivers/pci/setup-irq.c
 *
 * Extruded from code written by
 *      Dave Rusling (david.rusling@reo.mts.dec.com)
 *      David Mosberger (davidm@cs.arizona.edu)
 *	David Miller (davem@redhat.com)
 *
 * Support routines for initializing a PCI subsystem.
 */


#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/cache.h>


static void __init
pdev_fixup_irq(struct pci_dev *dev,
	       u8 (*swizzle)(struct pci_dev *, u8 *),
	       int (*map_irq)(const struct pci_dev *, u8, u8))
{
	u8 pin, slot;
	int irq = 0;


	pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
	
	if (pin > 4)
		pin = 1;

	if (pin != 0) {
		
		slot = (*swizzle)(dev, &pin);

		irq = (*map_irq)(dev, slot, pin);
		if (irq == -1)
			irq = 0;
	}
	dev->irq = irq;

	dev_dbg(&dev->dev, "fixup irq: got %d\n", dev->irq);

	pcibios_update_irq(dev, irq);
}

void __init
pci_fixup_irqs(u8 (*swizzle)(struct pci_dev *, u8 *),
	       int (*map_irq)(const struct pci_dev *, u8, u8))
{
	struct pci_dev *dev = NULL;
	for_each_pci_dev(dev)
		pdev_fixup_irq(dev, swizzle, map_irq);
}
