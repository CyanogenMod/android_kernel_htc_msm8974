#ifndef PCI_BCM63XX_H_
#define PCI_BCM63XX_H_

#include <bcm63xx_cpu.h>
#include <bcm63xx_io.h>
#include <bcm63xx_regs.h>
#include <bcm63xx_dev_pci.h>

#define CARDBUS_PCI_IDSEL	0x8

extern struct pci_ops bcm63xx_pci_ops;
extern struct pci_ops bcm63xx_cb_ops;

extern void __iomem *pci_iospace_start;

#endif 
