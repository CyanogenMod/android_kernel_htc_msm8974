#include <linux/pci.h>
#include <linux/module.h>
#include <asm/io.h>

void __iomem *ioport_map(unsigned long port, unsigned int nr)
{
	return (void __iomem *) (unsigned long) port;
}

void ioport_unmap(void __iomem *addr)
{
	
}
EXPORT_SYMBOL(ioport_map);
EXPORT_SYMBOL(ioport_unmap);

void pci_iounmap(struct pci_dev *dev, void __iomem * addr)
{
	
}
EXPORT_SYMBOL(pci_iounmap);
