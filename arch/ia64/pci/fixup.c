
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/machvec.h>


static void __devinit pci_fixup_video(struct pci_dev *pdev)
{
	struct pci_dev *bridge;
	struct pci_bus *bus;
	u16 config;

	if ((strcmp(platform_name, "dig") != 0)
	    && (strcmp(platform_name, "hpzx1")  != 0))
		return;
	

	if ((pdev->class >> 8) != PCI_CLASS_DISPLAY_VGA)
		return;

	
	bus = pdev->bus;
	while (bus) {
		bridge = bus->self;

		if (bridge
		    &&((bridge->hdr_type == PCI_HEADER_TYPE_BRIDGE)
		       ||(bridge->hdr_type == PCI_HEADER_TYPE_CARDBUS))) {
			pci_read_config_word(bridge, PCI_BRIDGE_CONTROL,
						&config);
			if (!(config & PCI_BRIDGE_CTL_VGA))
				return;
		}
		bus = bus->parent;
	}
	pci_read_config_word(pdev, PCI_COMMAND, &config);
	if (config & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY)) {
		pdev->resource[PCI_ROM_RESOURCE].flags |= IORESOURCE_ROM_SHADOW;
		dev_printk(KERN_DEBUG, &pdev->dev, "Boot video device\n");
	}
}
DECLARE_PCI_FIXUP_HEADER(PCI_ANY_ID, PCI_ANY_ID, pci_fixup_video);
