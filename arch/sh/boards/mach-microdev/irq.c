/*
 * arch/sh/boards/superh/microdev/irq.c
 *
 * Copyright (C) 2003 Sean McGoogan (Sean.McGoogan@superh.com)
 *
 * SuperH SH4-202 MicroDev board support.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <mach/microdev.h>

#define NUM_EXTERNAL_IRQS 16	

static const struct {
	unsigned char fpgaIrq;
	unsigned char mapped;
	const char *name;
} fpgaIrqTable[NUM_EXTERNAL_IRQS] = {
	{ 0,				0,	"unused"   },		
	{ MICRODEV_FPGA_IRQ_KEYBOARD,	1,	"keyboard" },		
	{ MICRODEV_FPGA_IRQ_SERIAL1,	1,	"Serial #1"},		
	{ MICRODEV_FPGA_IRQ_ETHERNET,	1,	"Ethernet" },		
	{ MICRODEV_FPGA_IRQ_SERIAL2,	0,	"Serial #2"},		
	{ 0,				0,	"unused"   },		
	{ 0,				0,	"unused"   },		
	{ MICRODEV_FPGA_IRQ_USB_HC,	1,	"USB"	   },		
	{ MICRODEV_IRQ_PCI_INTA,	1,	"PCI INTA" },		
	{ MICRODEV_IRQ_PCI_INTB,	1,	"PCI INTB" },		
	{ MICRODEV_IRQ_PCI_INTC,	1,	"PCI INTC" },		
	{ MICRODEV_IRQ_PCI_INTD,	1,	"PCI INTD" },		
	{ MICRODEV_FPGA_IRQ_MOUSE,	1,	"mouse"    },		
	{ MICRODEV_FPGA_IRQ_IDE2,	1,	"IDE #2"   },		
	{ MICRODEV_FPGA_IRQ_IDE1,	1,	"IDE #1"   },		
	{ 0,				0,	"unused"   },		
};

#if (MICRODEV_LINUX_IRQ_KEYBOARD != 1)
#  error Inconsistancy in defining the IRQ# for Keyboard!
#endif

#if (MICRODEV_LINUX_IRQ_ETHERNET != 3)
#  error Inconsistancy in defining the IRQ# for Ethernet!
#endif

#if (MICRODEV_LINUX_IRQ_USB_HC != 7)
#  error Inconsistancy in defining the IRQ# for USB!
#endif

#if (MICRODEV_LINUX_IRQ_MOUSE != 12)
#  error Inconsistancy in defining the IRQ# for PS/2 Mouse!
#endif

#if (MICRODEV_LINUX_IRQ_IDE2 != 13)
#  error Inconsistancy in defining the IRQ# for secondary IDE!
#endif

#if (MICRODEV_LINUX_IRQ_IDE1 != 14)
#  error Inconsistancy in defining the IRQ# for primary IDE!
#endif

static void disable_microdev_irq(struct irq_data *data)
{
	unsigned int irq = data->irq;
	unsigned int fpgaIrq;

	if (irq >= NUM_EXTERNAL_IRQS)
		return;
	if (!fpgaIrqTable[irq].mapped)
		return;

	fpgaIrq = fpgaIrqTable[irq].fpgaIrq;

	
	__raw_writel(MICRODEV_FPGA_INTC_MASK(fpgaIrq), MICRODEV_FPGA_INTDSB_REG);
}

static void enable_microdev_irq(struct irq_data *data)
{
	unsigned int irq = data->irq;
	unsigned long priorityReg, priorities, pri;
	unsigned int fpgaIrq;

	if (unlikely(irq >= NUM_EXTERNAL_IRQS))
		return;
	if (unlikely(!fpgaIrqTable[irq].mapped))
		return;

	pri = 15 - irq;

	fpgaIrq = fpgaIrqTable[irq].fpgaIrq;
	priorityReg = MICRODEV_FPGA_INTPRI_REG(fpgaIrq);

	
	priorities = __raw_readl(priorityReg);
	priorities &= ~MICRODEV_FPGA_INTPRI_MASK(fpgaIrq);
	priorities |= MICRODEV_FPGA_INTPRI_LEVEL(fpgaIrq, pri);
	__raw_writel(priorities, priorityReg);

	
	__raw_writel(MICRODEV_FPGA_INTC_MASK(fpgaIrq), MICRODEV_FPGA_INTENB_REG);
}

static struct irq_chip microdev_irq_type = {
	.name = "MicroDev-IRQ",
	.irq_unmask = enable_microdev_irq,
	.irq_mask = disable_microdev_irq,
};

static void __init make_microdev_irq(unsigned int irq)
{
	disable_irq_nosync(irq);
	irq_set_chip_and_handler(irq, &microdev_irq_type, handle_level_irq);
	disable_microdev_irq(irq_get_irq_data(irq));
}

extern void __init init_microdev_irq(void)
{
	int i;

	
	__raw_writel(~0ul, MICRODEV_FPGA_INTDSB_REG);

	for (i = 0; i < NUM_EXTERNAL_IRQS; i++)
		make_microdev_irq(i);
}

extern void microdev_print_fpga_intc_status(void)
{
	volatile unsigned int * const intenb = (unsigned int*)MICRODEV_FPGA_INTENB_REG;
	volatile unsigned int * const intdsb = (unsigned int*)MICRODEV_FPGA_INTDSB_REG;
	volatile unsigned int * const intpria = (unsigned int*)MICRODEV_FPGA_INTPRI_REG(0);
	volatile unsigned int * const intprib = (unsigned int*)MICRODEV_FPGA_INTPRI_REG(8);
	volatile unsigned int * const intpric = (unsigned int*)MICRODEV_FPGA_INTPRI_REG(16);
	volatile unsigned int * const intprid = (unsigned int*)MICRODEV_FPGA_INTPRI_REG(24);
	volatile unsigned int * const intsrc = (unsigned int*)MICRODEV_FPGA_INTSRC_REG;
	volatile unsigned int * const intreq = (unsigned int*)MICRODEV_FPGA_INTREQ_REG;

	printk("-------------------------- microdev_print_fpga_intc_status() ------------------\n");
	printk("FPGA_INTENB = 0x%08x\n", *intenb);
	printk("FPGA_INTDSB = 0x%08x\n", *intdsb);
	printk("FPGA_INTSRC = 0x%08x\n", *intsrc);
	printk("FPGA_INTREQ = 0x%08x\n", *intreq);
	printk("FPGA_INTPRI[3..0] = %08x:%08x:%08x:%08x\n", *intprid, *intpric, *intprib, *intpria);
	printk("-------------------------------------------------------------------------------\n");
}
