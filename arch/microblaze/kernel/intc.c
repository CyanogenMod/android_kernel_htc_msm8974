/*
 * Copyright (C) 2007-2009 Michal Simek <monstr@monstr.eu>
 * Copyright (C) 2007-2009 PetaLogix
 * Copyright (C) 2006 Atmark Techno, Inc.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/init.h>
#include <linux/irqdomain.h>
#include <linux/irq.h>
#include <asm/page.h>
#include <linux/io.h>
#include <linux/bug.h>

#include <asm/prom.h>
#include <asm/irq.h>

#ifdef CONFIG_SELFMOD_INTC
#include <asm/selfmod.h>
#define INTC_BASE	BARRIER_BASE_ADDR
#else
static unsigned int intc_baseaddr;
#define INTC_BASE	intc_baseaddr
#endif

#define ISR 0x00			
#define IPR 0x04			
#define IER 0x08			
#define IAR 0x0c			
#define SIE 0x10			
#define CIE 0x14			
#define IVR 0x18			
#define MER 0x1c			

#define MER_ME (1<<0)
#define MER_HIE (1<<1)

static void intc_enable_or_unmask(struct irq_data *d)
{
	unsigned long mask = 1 << d->hwirq;

	pr_debug("enable_or_unmask: %ld\n", d->hwirq);
	out_be32(INTC_BASE + SIE, mask);

	if (irqd_is_level_type(d))
		out_be32(INTC_BASE + IAR, mask);
}

static void intc_disable_or_mask(struct irq_data *d)
{
	pr_debug("disable: %ld\n", d->hwirq);
	out_be32(INTC_BASE + CIE, 1 << d->hwirq);
}

static void intc_ack(struct irq_data *d)
{
	pr_debug("ack: %ld\n", d->hwirq);
	out_be32(INTC_BASE + IAR, 1 << d->hwirq);
}

static void intc_mask_ack(struct irq_data *d)
{
	unsigned long mask = 1 << d->hwirq;

	pr_debug("disable_and_ack: %ld\n", d->hwirq);
	out_be32(INTC_BASE + CIE, mask);
	out_be32(INTC_BASE + IAR, mask);
}

static struct irq_chip intc_dev = {
	.name = "Xilinx INTC",
	.irq_unmask = intc_enable_or_unmask,
	.irq_mask = intc_disable_or_mask,
	.irq_ack = intc_ack,
	.irq_mask_ack = intc_mask_ack,
};

static struct irq_domain *root_domain;

unsigned int get_irq(void)
{
	unsigned int hwirq, irq = -1;

	hwirq = in_be32(INTC_BASE + IVR);
	if (hwirq != -1U)
		irq = irq_find_mapping(root_domain, hwirq);

	pr_debug("get_irq: hwirq=%d, irq=%d\n", hwirq, irq);

	return irq;
}

int xintc_map(struct irq_domain *d, unsigned int irq, irq_hw_number_t hw)
{
	u32 intr_mask = (u32)d->host_data;

	if (intr_mask & (1 << hw)) {
		irq_set_chip_and_handler_name(irq, &intc_dev,
						handle_edge_irq, "edge");
		irq_clear_status_flags(irq, IRQ_LEVEL);
	} else {
		irq_set_chip_and_handler_name(irq, &intc_dev,
						handle_level_irq, "level");
		irq_set_status_flags(irq, IRQ_LEVEL);
	}
	return 0;
}

static const struct irq_domain_ops xintc_irq_domain_ops = {
	.xlate = irq_domain_xlate_onetwocell,
	.map = xintc_map,
};

void __init init_IRQ(void)
{
	u32 nr_irq, intr_mask;
	struct device_node *intc = NULL;
#ifdef CONFIG_SELFMOD_INTC
	unsigned int intc_baseaddr = 0;
	static int arr_func[] = {
				(int)&get_irq,
				(int)&intc_enable_or_unmask,
				(int)&intc_disable_or_mask,
				(int)&intc_mask_ack,
				(int)&intc_ack,
				(int)&intc_end,
				0
			};
#endif
	intc = of_find_compatible_node(NULL, NULL, "xlnx,xps-intc-1.00.a");
	BUG_ON(!intc);

	intc_baseaddr = be32_to_cpup(of_get_property(intc, "reg", NULL));
	intc_baseaddr = (unsigned long) ioremap(intc_baseaddr, PAGE_SIZE);
	nr_irq = be32_to_cpup(of_get_property(intc,
						"xlnx,num-intr-inputs", NULL));

	intr_mask =
		be32_to_cpup(of_get_property(intc, "xlnx,kind-of-intr", NULL));
	if (intr_mask > (u32)((1ULL << nr_irq) - 1))
		printk(KERN_INFO " ERROR: Mismatch in kind-of-intr param\n");

#ifdef CONFIG_SELFMOD_INTC
	selfmod_function((int *) arr_func, intc_baseaddr);
#endif
	printk(KERN_INFO "%s #0 at 0x%08x, num_irq=%d, edge=0x%x\n",
		intc->name, intc_baseaddr, nr_irq, intr_mask);

	out_be32(intc_baseaddr + IER, 0);

	
	out_be32(intc_baseaddr + IAR, 0xffffffff);

	
	out_be32(intc_baseaddr + MER, MER_HIE | MER_ME);

	root_domain = irq_domain_add_linear(intc, nr_irq, &xintc_irq_domain_ops,
							(void *)intr_mask);
}
