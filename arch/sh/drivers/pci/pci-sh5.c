/*
 * Copyright (C) 2001 David J. Mckay (david.mckay@st.com)
 * Copyright (C) 2003, 2004 Paul Mundt
 * Copyright (C) 2004 Richard Curnow
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Support functions for the SH5 PCI hardware.
 */

#include <linux/kernel.h>
#include <linux/rwsem.h>
#include <linux/smp.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/irq.h>
#include <cpu/irq.h>
#include <asm/pci.h>
#include <asm/io.h>
#include "pci-sh5.h"

unsigned long pcicr_virt;
unsigned long PCI_IO_AREA;

static u32 __init r2p2(u32 num)
{
	int i = 31;
	u32 tmp = num;

	if (num == 0)
		return 0;

	do {
		if (tmp & (1 << 31))
			break;
		i--;
		tmp <<= 1;
	} while (i >= 0);

	tmp = 1 << i;
	
	if (tmp != num)
		tmp <<= 1;

	return tmp;
}

static irqreturn_t pcish5_err_irq(int irq, void *dev_id)
{
	struct pt_regs *regs = get_irq_regs();
	unsigned pci_int, pci_air, pci_cir, pci_aint;

	pci_int = SH5PCI_READ(INT);
	pci_cir = SH5PCI_READ(CIR);
	pci_air = SH5PCI_READ(AIR);

	if (pci_int) {
		printk("PCI INTERRUPT (at %08llx)!\n", regs->pc);
		printk("PCI INT -> 0x%x\n", pci_int & 0xffff);
		printk("PCI AIR -> 0x%x\n", pci_air);
		printk("PCI CIR -> 0x%x\n", pci_cir);
		SH5PCI_WRITE(INT, ~0);
	}

	pci_aint = SH5PCI_READ(AINT);
	if (pci_aint) {
		printk("PCI ARB INTERRUPT!\n");
		printk("PCI AINT -> 0x%x\n", pci_aint);
		printk("PCI AIR -> 0x%x\n", pci_air);
		printk("PCI CIR -> 0x%x\n", pci_cir);
		SH5PCI_WRITE(AINT, ~0);
	}

	return IRQ_HANDLED;
}

static irqreturn_t pcish5_serr_irq(int irq, void *dev_id)
{
	printk("SERR IRQ\n");

	return IRQ_NONE;
}

static struct resource sh5_pci_resources[2];

static struct pci_channel sh5pci_controller = {
	.pci_ops		= &sh5_pci_ops,
	.resources		= sh5_pci_resources,
	.nr_resources		= ARRAY_SIZE(sh5_pci_resources),
	.mem_offset		= 0x00000000,
	.io_offset		= 0x00000000,
};

static int __init sh5pci_init(void)
{
	unsigned long memStart = __pa(memory_start);
	unsigned long memSize = __pa(memory_end) - memStart;
	u32 lsr0;
	u32 uval;

        if (request_irq(IRQ_ERR, pcish5_err_irq,
                        0, "PCI Error",NULL) < 0) {
                printk(KERN_ERR "PCISH5: Cannot hook PCI_PERR interrupt\n");
                return -EINVAL;
        }

        if (request_irq(IRQ_SERR, pcish5_serr_irq,
                        0, "PCI SERR interrupt", NULL) < 0) {
                printk(KERN_ERR "PCISH5: Cannot hook PCI_SERR interrupt\n");
                return -EINVAL;
        }

	pcicr_virt = (unsigned long)ioremap_nocache(SH5PCI_ICR_BASE, 1024);
	if (!pcicr_virt) {
		panic("Unable to remap PCICR\n");
	}

	PCI_IO_AREA = (unsigned long)ioremap_nocache(SH5PCI_IO_BASE, 0x10000);
	if (!PCI_IO_AREA) {
		panic("Unable to remap PCIIO\n");
	}

	
        SH5PCI_WRITE(CSCR0, 0);
        SH5PCI_WRITE(CSCR1, 0);

        
        SH5PCI_WRITE(INTM,  0);
        SH5PCI_WRITE(AINTM, 0);
        SH5PCI_WRITE(PINTM, 0);

        
        uval = SH5PCI_READ(CR);

	
        SH5PCI_WRITE(CR, uval | CR_LOCK_MASK | CR_CFINT| CR_FTO | CR_PFE |
		     CR_PFCS | CR_BMAM);

	uval=SH5PCI_READ(CR);

        
	
        
	SH5PCI_WRITE_SHORT(CSR_CMD,
			    PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER |
			    PCI_COMMAND_WAIT);

        SH5PCI_WRITE(MBR,0x40000000);

        
        SH5PCI_WRITE(MBMR, PCISH5_MEM_SIZCONV(512*1024*1024));


        SH5PCI_WRITE(IOBR,0x0);

        
        SH5PCI_WRITE(IOBMR,0);

	SH5PCI_WRITE(CSR_IBAR0,~0);
	
        memSize = memory_end - memory_start;

	if (memSize < (1024 * 1024)) {
                printk(KERN_ERR "PCISH5: Ridiculous memory size of 0x%lx?\n",
		       memSize);
                return -EINVAL;
        }

        
        lsr0 = (memSize > (512 * 1024 * 1024)) ? 0x1ff00001 :
		((r2p2(memSize) - 0x100000) | 0x1);
        SH5PCI_WRITE(LSR0, lsr0);

        
        SH5PCI_WRITE(CSR_MBAR0, memory_start);
        SH5PCI_WRITE(LAR0, memory_start);

        SH5PCI_WRITE(CSR_MBAR1,0);
        SH5PCI_WRITE(LAR1,0);
        SH5PCI_WRITE(LSR1,0);

        
        SH5PCI_WRITE(INTM,  ~0);
        SH5PCI_WRITE(AINTM, ~0);
        SH5PCI_WRITE(PINTM, ~0);

	sh5_pci_resources[0].start = PCI_IO_AREA;
	sh5_pci_resources[0].end = PCI_IO_AREA + 0x10000;

	sh5_pci_resources[1].start = memStart;
	sh5_pci_resources[1].end = memStart + memSize;

	return register_pci_controller(&sh5pci_controller);
}
arch_initcall(sh5pci_init);
