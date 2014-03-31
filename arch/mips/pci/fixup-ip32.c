#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <asm/ip32/ip32_ints.h>

#define SCSI0  MACEPCI_SCSI0_IRQ
#define SCSI1  MACEPCI_SCSI1_IRQ
#define INTA0  MACEPCI_SLOT0_IRQ
#define INTA1  MACEPCI_SLOT1_IRQ
#define INTA2  MACEPCI_SLOT2_IRQ
#define INTB   MACEPCI_SHARED0_IRQ
#define INTC   MACEPCI_SHARED1_IRQ
#define INTD   MACEPCI_SHARED2_IRQ
static char irq_tab_mace[][5] __initdata = {
      
	{0,         0,     0,     0,     0}, 
	{0,     SCSI0, SCSI0, SCSI0, SCSI0},
	{0,     SCSI1, SCSI1, SCSI1, SCSI1},
	{0,     INTA0,  INTB,  INTC,  INTD},
	{0,     INTA1,  INTC,  INTD,  INTB},
	{0,     INTA2,  INTD,  INTB,  INTC},
};


int __init pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	return irq_tab_mace[slot][pin];
}

int pcibios_plat_dev_init(struct pci_dev *dev)
{
	return 0;
}
