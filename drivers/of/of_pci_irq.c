#include <linux/kernel.h>
#include <linux/of_pci.h>
#include <linux/of_irq.h>
#include <linux/export.h>
#include <asm/prom.h>

int of_irq_map_pci(struct pci_dev *pdev, struct of_irq *out_irq)
{
	struct device_node *dn, *ppnode;
	struct pci_dev *ppdev;
	u32 lspec;
	__be32 lspec_be;
	__be32 laddr[3];
	u8 pin;
	int rc;

	dn = pci_device_to_OF_node(pdev);
	if (dn) {
		rc = of_irq_map_one(dn, 0, out_irq);
		if (!rc)
			return rc;
	}

	rc = pci_read_config_byte(pdev, PCI_INTERRUPT_PIN, &pin);
	if (rc != 0)
		return rc;
	
	if (pin == 0)
		return -ENODEV;

	
	lspec = pin;
	for (;;) {
		
		ppdev = pdev->bus->self;

		
		if (ppdev == NULL) {
			ppnode = pci_bus_to_OF_node(pdev->bus);

			
			if (ppnode == NULL)
				return -EINVAL;
		} else {
			
			ppnode = pci_device_to_OF_node(ppdev);
		}

		if (ppnode)
			break;

		lspec = pci_swizzle_interrupt_pin(pdev, lspec);
		pdev = ppdev;
	}

	lspec_be = cpu_to_be32(lspec);
	laddr[0] = cpu_to_be32((pdev->bus->number << 16) | (pdev->devfn << 8));
	laddr[1]  = laddr[2] = cpu_to_be32(0);
	return of_irq_map_raw(ppnode, &lspec_be, 1, laddr, out_irq);
}
EXPORT_SYMBOL_GPL(of_irq_map_pci);
