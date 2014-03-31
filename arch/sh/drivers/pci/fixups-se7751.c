#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/io.h>
#include "pci-sh4.h"

int __init pcibios_map_platform_irq(const struct pci_dev *, u8 slot, u8 pin)
{
        switch (slot) {
        case 0: return 13;
        case 1: return 13;	
        case 2: return -1;
        case 3: return -1;
        case 4: return -1;
        default:
                printk("PCI: Bad IRQ mapping request for slot %d\n", slot);
                return -1;
        }
}

#define PCIMCR_MRSET_OFF	0xBFFFFFFF
#define PCIMCR_RFSH_OFF		0xFFFFFFFB

#define PCIC_WRITE(x,v) writel((v), PCI_REG(x))
#define PCIC_READ(x) readl(PCI_REG(x))

int pci_fixup_pcic(struct pci_channel *chan)
{
	unsigned long bcr1, wcr1, wcr2, wcr3, mcr;
	unsigned short bcr2;

	bcr1 = (*(volatile unsigned long*)(SH7751_BCR1));
	bcr2 = (*(volatile unsigned short*)(SH7751_BCR2));
	wcr1 = (*(volatile unsigned long*)(SH7751_WCR1));
	wcr2 = (*(volatile unsigned long*)(SH7751_WCR2));
	wcr3 = (*(volatile unsigned long*)(SH7751_WCR3));
	mcr = (*(volatile unsigned long*)(SH7751_MCR));

	bcr1 = bcr1 | 0x00080000;  
	(*(volatile unsigned long*)(SH7751_BCR1)) = bcr1;

	bcr1 = bcr1 | 0x40080000;  
	PCIC_WRITE(SH7751_PCIBCR1, bcr1);	 
	PCIC_WRITE(SH7751_PCIBCR2, bcr2);     
	PCIC_WRITE(SH7751_PCIWCR1, wcr1);     
	PCIC_WRITE(SH7751_PCIWCR2, wcr2);     
	PCIC_WRITE(SH7751_PCIWCR3, wcr3);     
	mcr = (mcr & PCIMCR_MRSET_OFF) & PCIMCR_RFSH_OFF;
	PCIC_WRITE(SH7751_PCIMCR, mcr);      


	
	PCIC_WRITE(SH7751_PCIINTM, 0x0000c3ff);
	PCIC_WRITE(SH7751_PCIAINTM, 0x0000380f);

	
	PCIC_WRITE(SH7751_PCICONF1,	0xF39000C7); 
	PCIC_WRITE(SH7751_PCICONF2,	0x00000000); 
	PCIC_WRITE(SH7751_PCICONF4,	0xab000001); 
	PCIC_WRITE(SH7751_PCICONF5,	0x0c000000); 
	PCIC_WRITE(SH7751_PCICONF6,	0xd0000000); 
	PCIC_WRITE(SH7751_PCICONF11, 0x35051054); 
	PCIC_WRITE(SH7751_PCILSR0, 0x03f00000);   
	PCIC_WRITE(SH7751_PCILSR1, 0x00000000);   
	PCIC_WRITE(SH7751_PCILAR0, 0x0c000000);   
	PCIC_WRITE(SH7751_PCILAR1, 0x00000000);   

	
	PCIC_WRITE(SH7751_PCICR, 0xa5000001);


	BUG_ON(chan->resources[1].start != SH7751_PCI_MEMORY_BASE);

	PCIC_WRITE(SH7751_PCIMBR, chan->resources[1].start);

	
	PCIC_WRITE(SH7751_PCIIOBR, (chan->resources[0].start & SH7751_PCIIOBR_MASK));

	
	printk("SH7751 PCI: Finished initialization of the PCI controller\n");

	return 1;
}
