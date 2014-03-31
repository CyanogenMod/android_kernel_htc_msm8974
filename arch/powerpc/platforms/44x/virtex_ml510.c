#include <asm/i8259.h>
#include <linux/pci.h>
#include "44x.h"

static void __devinit ml510_ali_quirk(struct pci_dev *dev)
{
	
	pci_write_config_byte(dev, 0x58, 0x4c);
	
	pci_write_config_byte(dev, 0x44, 0x0d);
	
	pci_write_config_byte(dev, 0x75, 0x0f);
	
	pci_write_config_byte(dev, 0x09, 0xff);

	
	pci_write_config_byte(dev, 0x48, 0x00);
	
	pci_write_config_byte(dev, 0x4a, 0x00);
	
	pci_write_config_byte(dev, 0x4b, 0x60);
	
	pci_write_config_byte(dev, 0x74, 0x06);
}
DECLARE_PCI_FIXUP_EARLY(0x10b9, 0x1533, ml510_ali_quirk);

