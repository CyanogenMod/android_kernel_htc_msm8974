/*
 *
 * Programmable Interrupt Controller functions for the Freescale MPC52xx.
 *
 * Copyright (C) 2008 Secret Lab Technologies Ltd.
 * Copyright (C) 2006 bplan GmbH
 * Copyright (C) 2004 Sylvain Munaut <tnt@246tNt.com>
 * Copyright (C) 2003 Montavista Software, Inc
 *
 * Based on the code from the 2.4 kernel by
 * Dale Farnsworth <dfarnsworth@mvista.com> and Kent Borg.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 */

#undef DEBUG

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/mpc52xx.h>

#define MPC52xx_IRQ_L1_CRIT	(0)
#define MPC52xx_IRQ_L1_MAIN	(1)
#define MPC52xx_IRQ_L1_PERP	(2)
#define MPC52xx_IRQ_L1_SDMA	(3)

#define MPC52xx_IRQ_L1_OFFSET	(6)
#define MPC52xx_IRQ_L1_MASK	(0x00c0)
#define MPC52xx_IRQ_L2_MASK	(0x003f)

#define MPC52xx_IRQ_HIGHTESTHWIRQ (0xd0)


static struct of_device_id mpc52xx_pic_ids[] __initdata = {
	{ .compatible = "fsl,mpc5200-pic", },
	{ .compatible = "mpc5200-pic", },
	{}
};
static struct of_device_id mpc52xx_sdma_ids[] __initdata = {
	{ .compatible = "fsl,mpc5200-bestcomm", },
	{ .compatible = "mpc5200-bestcomm", },
	{}
};

static struct mpc52xx_intr __iomem *intr;
static struct mpc52xx_sdma __iomem *sdma;
static struct irq_domain *mpc52xx_irqhost = NULL;

static unsigned char mpc52xx_map_senses[4] = {
	IRQ_TYPE_LEVEL_HIGH,
	IRQ_TYPE_EDGE_RISING,
	IRQ_TYPE_EDGE_FALLING,
	IRQ_TYPE_LEVEL_LOW,
};

static inline void io_be_setbit(u32 __iomem *addr, int bitno)
{
	out_be32(addr, in_be32(addr) | (1 << bitno));
}

static inline void io_be_clrbit(u32 __iomem *addr, int bitno)
{
	out_be32(addr, in_be32(addr) & ~(1 << bitno));
}

static void mpc52xx_extirq_mask(struct irq_data *d)
{
	int l2irq = irqd_to_hwirq(d) & MPC52xx_IRQ_L2_MASK;
	io_be_clrbit(&intr->ctrl, 11 - l2irq);
}

static void mpc52xx_extirq_unmask(struct irq_data *d)
{
	int l2irq = irqd_to_hwirq(d) & MPC52xx_IRQ_L2_MASK;
	io_be_setbit(&intr->ctrl, 11 - l2irq);
}

static void mpc52xx_extirq_ack(struct irq_data *d)
{
	int l2irq = irqd_to_hwirq(d) & MPC52xx_IRQ_L2_MASK;
	io_be_setbit(&intr->ctrl, 27-l2irq);
}

static int mpc52xx_extirq_set_type(struct irq_data *d, unsigned int flow_type)
{
	u32 ctrl_reg, type;
	int l2irq = irqd_to_hwirq(d) & MPC52xx_IRQ_L2_MASK;
	void *handler = handle_level_irq;

	pr_debug("%s: irq=%x. l2=%d flow_type=%d\n", __func__,
		(int) irqd_to_hwirq(d), l2irq, flow_type);

	switch (flow_type) {
	case IRQF_TRIGGER_HIGH: type = 0; break;
	case IRQF_TRIGGER_RISING: type = 1; handler = handle_edge_irq; break;
	case IRQF_TRIGGER_FALLING: type = 2; handler = handle_edge_irq; break;
	case IRQF_TRIGGER_LOW: type = 3; break;
	default:
		type = 0;
	}

	ctrl_reg = in_be32(&intr->ctrl);
	ctrl_reg &= ~(0x3 << (22 - (l2irq * 2)));
	ctrl_reg |= (type << (22 - (l2irq * 2)));
	out_be32(&intr->ctrl, ctrl_reg);

	__irq_set_handler_locked(d->irq, handler);

	return 0;
}

static struct irq_chip mpc52xx_extirq_irqchip = {
	.name = "MPC52xx External",
	.irq_mask = mpc52xx_extirq_mask,
	.irq_unmask = mpc52xx_extirq_unmask,
	.irq_ack = mpc52xx_extirq_ack,
	.irq_set_type = mpc52xx_extirq_set_type,
};

static int mpc52xx_null_set_type(struct irq_data *d, unsigned int flow_type)
{
	return 0; 
}

static void mpc52xx_main_mask(struct irq_data *d)
{
	int l2irq = irqd_to_hwirq(d) & MPC52xx_IRQ_L2_MASK;
	io_be_setbit(&intr->main_mask, 16 - l2irq);
}

static void mpc52xx_main_unmask(struct irq_data *d)
{
	int l2irq = irqd_to_hwirq(d) & MPC52xx_IRQ_L2_MASK;
	io_be_clrbit(&intr->main_mask, 16 - l2irq);
}

static struct irq_chip mpc52xx_main_irqchip = {
	.name = "MPC52xx Main",
	.irq_mask = mpc52xx_main_mask,
	.irq_mask_ack = mpc52xx_main_mask,
	.irq_unmask = mpc52xx_main_unmask,
	.irq_set_type = mpc52xx_null_set_type,
};

static void mpc52xx_periph_mask(struct irq_data *d)
{
	int l2irq = irqd_to_hwirq(d) & MPC52xx_IRQ_L2_MASK;
	io_be_setbit(&intr->per_mask, 31 - l2irq);
}

static void mpc52xx_periph_unmask(struct irq_data *d)
{
	int l2irq = irqd_to_hwirq(d) & MPC52xx_IRQ_L2_MASK;
	io_be_clrbit(&intr->per_mask, 31 - l2irq);
}

static struct irq_chip mpc52xx_periph_irqchip = {
	.name = "MPC52xx Peripherals",
	.irq_mask = mpc52xx_periph_mask,
	.irq_mask_ack = mpc52xx_periph_mask,
	.irq_unmask = mpc52xx_periph_unmask,
	.irq_set_type = mpc52xx_null_set_type,
};

static void mpc52xx_sdma_mask(struct irq_data *d)
{
	int l2irq = irqd_to_hwirq(d) & MPC52xx_IRQ_L2_MASK;
	io_be_setbit(&sdma->IntMask, l2irq);
}

static void mpc52xx_sdma_unmask(struct irq_data *d)
{
	int l2irq = irqd_to_hwirq(d) & MPC52xx_IRQ_L2_MASK;
	io_be_clrbit(&sdma->IntMask, l2irq);
}

static void mpc52xx_sdma_ack(struct irq_data *d)
{
	int l2irq = irqd_to_hwirq(d) & MPC52xx_IRQ_L2_MASK;
	out_be32(&sdma->IntPend, 1 << l2irq);
}

static struct irq_chip mpc52xx_sdma_irqchip = {
	.name = "MPC52xx SDMA",
	.irq_mask = mpc52xx_sdma_mask,
	.irq_unmask = mpc52xx_sdma_unmask,
	.irq_ack = mpc52xx_sdma_ack,
	.irq_set_type = mpc52xx_null_set_type,
};

static int mpc52xx_is_extirq(int l1, int l2)
{
	return ((l1 == 0) && (l2 == 0)) ||
	       ((l1 == 1) && (l2 >= 1) && (l2 <= 3));
}

static int mpc52xx_irqhost_xlate(struct irq_domain *h, struct device_node *ct,
				 const u32 *intspec, unsigned int intsize,
				 irq_hw_number_t *out_hwirq,
				 unsigned int *out_flags)
{
	int intrvect_l1;
	int intrvect_l2;
	int intrvect_type;
	int intrvect_linux;

	if (intsize != 3)
		return -1;

	intrvect_l1 = (int)intspec[0];
	intrvect_l2 = (int)intspec[1];
	intrvect_type = (int)intspec[2] & 0x3;

	intrvect_linux = (intrvect_l1 << MPC52xx_IRQ_L1_OFFSET) &
			 MPC52xx_IRQ_L1_MASK;
	intrvect_linux |= intrvect_l2 & MPC52xx_IRQ_L2_MASK;

	*out_hwirq = intrvect_linux;
	*out_flags = IRQ_TYPE_LEVEL_LOW;
	if (mpc52xx_is_extirq(intrvect_l1, intrvect_l2))
		*out_flags = mpc52xx_map_senses[intrvect_type];

	pr_debug("return %x, l1=%d, l2=%d\n", intrvect_linux, intrvect_l1,
		 intrvect_l2);
	return 0;
}

static int mpc52xx_irqhost_map(struct irq_domain *h, unsigned int virq,
			       irq_hw_number_t irq)
{
	int l1irq;
	int l2irq;
	struct irq_chip *irqchip;
	void *hndlr;
	int type;
	u32 reg;

	l1irq = (irq & MPC52xx_IRQ_L1_MASK) >> MPC52xx_IRQ_L1_OFFSET;
	l2irq = irq & MPC52xx_IRQ_L2_MASK;

	if (mpc52xx_is_extirq(l1irq, l2irq)) {
		reg = in_be32(&intr->ctrl);
		type = mpc52xx_map_senses[(reg >> (22 - l2irq * 2)) & 0x3];
		if ((type == IRQ_TYPE_EDGE_FALLING) ||
		    (type == IRQ_TYPE_EDGE_RISING))
			hndlr = handle_edge_irq;
		else
			hndlr = handle_level_irq;

		irq_set_chip_and_handler(virq, &mpc52xx_extirq_irqchip, hndlr);
		pr_debug("%s: External IRQ%i virq=%x, hw=%x. type=%x\n",
			 __func__, l2irq, virq, (int)irq, type);
		return 0;
	}

	
	switch (l1irq) {
	case MPC52xx_IRQ_L1_MAIN: irqchip = &mpc52xx_main_irqchip; break;
	case MPC52xx_IRQ_L1_PERP: irqchip = &mpc52xx_periph_irqchip; break;
	case MPC52xx_IRQ_L1_SDMA: irqchip = &mpc52xx_sdma_irqchip; break;
	default:
		pr_err("%s: invalid irq: virq=%i, l1=%i, l2=%i\n",
		       __func__, virq, l1irq, l2irq);
		return -EINVAL;
	}

	irq_set_chip_and_handler(virq, irqchip, handle_level_irq);
	pr_debug("%s: virq=%x, l1=%i, l2=%i\n", __func__, virq, l1irq, l2irq);

	return 0;
}

static const struct irq_domain_ops mpc52xx_irqhost_ops = {
	.xlate = mpc52xx_irqhost_xlate,
	.map = mpc52xx_irqhost_map,
};

void __init mpc52xx_init_irq(void)
{
	u32 intr_ctrl;
	struct device_node *picnode;
	struct device_node *np;

	
	picnode = of_find_matching_node(NULL, mpc52xx_pic_ids);
	intr = of_iomap(picnode, 0);
	if (!intr)
		panic(__FILE__	": find_and_map failed on 'mpc5200-pic'. "
				"Check node !");

	np = of_find_matching_node(NULL, mpc52xx_sdma_ids);
	sdma = of_iomap(np, 0);
	of_node_put(np);
	if (!sdma)
		panic(__FILE__	": find_and_map failed on 'mpc5200-bestcomm'. "
				"Check node !");

	pr_debug("MPC5200 IRQ controller mapped to 0x%p\n", intr);

	
	out_be32(&sdma->IntPend, 0xffffffff);	
	out_be32(&sdma->IntMask, 0xffffffff);	
	out_be32(&intr->per_mask, 0x7ffffc00);	
	out_be32(&intr->main_mask, 0x00010fff);	
	intr_ctrl = in_be32(&intr->ctrl);
	intr_ctrl &= 0x00ff0000;	
	intr_ctrl |=	0x0f000000 |	
			0x00001000 |	
			0x00000000 |	
			0x00000001;	
	out_be32(&intr->ctrl, intr_ctrl);

	
	out_be32(&intr->per_pri1, 0);
	out_be32(&intr->per_pri2, 0);
	out_be32(&intr->per_pri3, 0);
	out_be32(&intr->main_pri1, 0);
	out_be32(&intr->main_pri2, 0);

	mpc52xx_irqhost = irq_domain_add_linear(picnode,
	                                 MPC52xx_IRQ_HIGHTESTHWIRQ,
	                                 &mpc52xx_irqhost_ops, NULL);

	if (!mpc52xx_irqhost)
		panic(__FILE__ ": Cannot allocate the IRQ host\n");

	irq_set_default_host(mpc52xx_irqhost);

	pr_info("MPC52xx PIC is up and running!\n");
}

unsigned int mpc52xx_get_irq(void)
{
	u32 status;
	int irq;

	status = in_be32(&intr->enc_status);
	if (status & 0x00000400) {	
		irq = (status >> 8) & 0x3;
		if (irq == 2)	
			goto peripheral;
		irq |= (MPC52xx_IRQ_L1_CRIT << MPC52xx_IRQ_L1_OFFSET);
	} else if (status & 0x00200000) {	
		irq = (status >> 16) & 0x1f;
		if (irq == 4)	
			goto peripheral;
		irq |= (MPC52xx_IRQ_L1_MAIN << MPC52xx_IRQ_L1_OFFSET);
	} else if (status & 0x20000000) {	
	      peripheral:
		irq = (status >> 24) & 0x1f;
		if (irq == 0) {	
			status = in_be32(&sdma->IntPend);
			irq = ffs(status) - 1;
			irq |= (MPC52xx_IRQ_L1_SDMA << MPC52xx_IRQ_L1_OFFSET);
		} else {
			irq |= (MPC52xx_IRQ_L1_PERP << MPC52xx_IRQ_L1_OFFSET);
		}
	} else {
		return NO_IRQ;
	}

	return irq_linear_revmap(mpc52xx_irqhost, irq);
}
