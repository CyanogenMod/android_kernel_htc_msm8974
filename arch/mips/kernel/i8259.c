/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Code to handle x86 style IRQs plus some generic interrupt stuff.
 *
 * Copyright (C) 1992 Linus Torvalds
 * Copyright (C) 1994 - 2000 Ralf Baechle
 */
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/syscore_ops.h>
#include <linux/irq.h>

#include <asm/i8259.h>
#include <asm/io.h>


static int i8259A_auto_eoi = -1;
DEFINE_RAW_SPINLOCK(i8259A_lock);
static void disable_8259A_irq(struct irq_data *d);
static void enable_8259A_irq(struct irq_data *d);
static void mask_and_ack_8259A(struct irq_data *d);
static void init_8259A(int auto_eoi);

static struct irq_chip i8259A_chip = {
	.name			= "XT-PIC",
	.irq_mask		= disable_8259A_irq,
	.irq_disable		= disable_8259A_irq,
	.irq_unmask		= enable_8259A_irq,
	.irq_mask_ack		= mask_and_ack_8259A,
#ifdef CONFIG_MIPS_MT_SMTC_IRQAFF
	.irq_set_affinity	= plat_set_irq_affinity,
#endif 
};


static unsigned int cached_irq_mask = 0xffff;

#define cached_master_mask	(cached_irq_mask)
#define cached_slave_mask	(cached_irq_mask >> 8)

static void disable_8259A_irq(struct irq_data *d)
{
	unsigned int mask, irq = d->irq - I8259A_IRQ_BASE;
	unsigned long flags;

	mask = 1 << irq;
	raw_spin_lock_irqsave(&i8259A_lock, flags);
	cached_irq_mask |= mask;
	if (irq & 8)
		outb(cached_slave_mask, PIC_SLAVE_IMR);
	else
		outb(cached_master_mask, PIC_MASTER_IMR);
	raw_spin_unlock_irqrestore(&i8259A_lock, flags);
}

static void enable_8259A_irq(struct irq_data *d)
{
	unsigned int mask, irq = d->irq - I8259A_IRQ_BASE;
	unsigned long flags;

	mask = ~(1 << irq);
	raw_spin_lock_irqsave(&i8259A_lock, flags);
	cached_irq_mask &= mask;
	if (irq & 8)
		outb(cached_slave_mask, PIC_SLAVE_IMR);
	else
		outb(cached_master_mask, PIC_MASTER_IMR);
	raw_spin_unlock_irqrestore(&i8259A_lock, flags);
}

int i8259A_irq_pending(unsigned int irq)
{
	unsigned int mask;
	unsigned long flags;
	int ret;

	irq -= I8259A_IRQ_BASE;
	mask = 1 << irq;
	raw_spin_lock_irqsave(&i8259A_lock, flags);
	if (irq < 8)
		ret = inb(PIC_MASTER_CMD) & mask;
	else
		ret = inb(PIC_SLAVE_CMD) & (mask >> 8);
	raw_spin_unlock_irqrestore(&i8259A_lock, flags);

	return ret;
}

void make_8259A_irq(unsigned int irq)
{
	disable_irq_nosync(irq);
	irq_set_chip_and_handler(irq, &i8259A_chip, handle_level_irq);
	enable_irq(irq);
}

static inline int i8259A_irq_real(unsigned int irq)
{
	int value;
	int irqmask = 1 << irq;

	if (irq < 8) {
		outb(0x0B, PIC_MASTER_CMD);	
		value = inb(PIC_MASTER_CMD) & irqmask;
		outb(0x0A, PIC_MASTER_CMD);	
		return value;
	}
	outb(0x0B, PIC_SLAVE_CMD);	
	value = inb(PIC_SLAVE_CMD) & (irqmask >> 8);
	outb(0x0A, PIC_SLAVE_CMD);	
	return value;
}

static void mask_and_ack_8259A(struct irq_data *d)
{
	unsigned int irqmask, irq = d->irq - I8259A_IRQ_BASE;
	unsigned long flags;

	irqmask = 1 << irq;
	raw_spin_lock_irqsave(&i8259A_lock, flags);
	if (cached_irq_mask & irqmask)
		goto spurious_8259A_irq;
	cached_irq_mask |= irqmask;

handle_real_irq:
	if (irq & 8) {
		inb(PIC_SLAVE_IMR);	
		outb(cached_slave_mask, PIC_SLAVE_IMR);
		outb(0x60+(irq&7), PIC_SLAVE_CMD);
		outb(0x60+PIC_CASCADE_IR, PIC_MASTER_CMD); 
	} else {
		inb(PIC_MASTER_IMR);	
		outb(cached_master_mask, PIC_MASTER_IMR);
		outb(0x60+irq, PIC_MASTER_CMD);	
	}
	smtc_im_ack_irq(irq);
	raw_spin_unlock_irqrestore(&i8259A_lock, flags);
	return;

spurious_8259A_irq:
	if (i8259A_irq_real(irq))
		goto handle_real_irq;

	{
		static int spurious_irq_mask;
		if (!(spurious_irq_mask & irqmask)) {
			printk(KERN_DEBUG "spurious 8259A interrupt: IRQ%d.\n", irq);
			spurious_irq_mask |= irqmask;
		}
		atomic_inc(&irq_err_count);
		goto handle_real_irq;
	}
}

static void i8259A_resume(void)
{
	if (i8259A_auto_eoi >= 0)
		init_8259A(i8259A_auto_eoi);
}

static void i8259A_shutdown(void)
{
	if (i8259A_auto_eoi >= 0) {
		outb(0xff, PIC_MASTER_IMR);	
		outb(0xff, PIC_SLAVE_IMR);	
	}
}

static struct syscore_ops i8259_syscore_ops = {
	.resume = i8259A_resume,
	.shutdown = i8259A_shutdown,
};

static int __init i8259A_init_sysfs(void)
{
	register_syscore_ops(&i8259_syscore_ops);
	return 0;
}

device_initcall(i8259A_init_sysfs);

static void init_8259A(int auto_eoi)
{
	unsigned long flags;

	i8259A_auto_eoi = auto_eoi;

	raw_spin_lock_irqsave(&i8259A_lock, flags);

	outb(0xff, PIC_MASTER_IMR);	
	outb(0xff, PIC_SLAVE_IMR);	

	outb_p(0x11, PIC_MASTER_CMD);	
	outb_p(I8259A_IRQ_BASE + 0, PIC_MASTER_IMR);	
	outb_p(1U << PIC_CASCADE_IR, PIC_MASTER_IMR);	
	if (auto_eoi)	
		outb_p(MASTER_ICW4_DEFAULT | PIC_ICW4_AEOI, PIC_MASTER_IMR);
	else		
		outb_p(MASTER_ICW4_DEFAULT, PIC_MASTER_IMR);

	outb_p(0x11, PIC_SLAVE_CMD);	
	outb_p(I8259A_IRQ_BASE + 8, PIC_SLAVE_IMR);	
	outb_p(PIC_CASCADE_IR, PIC_SLAVE_IMR);	
	outb_p(SLAVE_ICW4_DEFAULT, PIC_SLAVE_IMR); 
	if (auto_eoi)
		i8259A_chip.irq_mask_ack = disable_8259A_irq;
	else
		i8259A_chip.irq_mask_ack = mask_and_ack_8259A;

	udelay(100);		

	outb(cached_master_mask, PIC_MASTER_IMR); 
	outb(cached_slave_mask, PIC_SLAVE_IMR);	  

	raw_spin_unlock_irqrestore(&i8259A_lock, flags);
}

static struct irqaction irq2 = {
	.handler = no_action,
	.name = "cascade",
	.flags = IRQF_NO_THREAD,
};

static struct resource pic1_io_resource = {
	.name = "pic1",
	.start = PIC_MASTER_CMD,
	.end = PIC_MASTER_IMR,
	.flags = IORESOURCE_BUSY
};

static struct resource pic2_io_resource = {
	.name = "pic2",
	.start = PIC_SLAVE_CMD,
	.end = PIC_SLAVE_IMR,
	.flags = IORESOURCE_BUSY
};

void __init init_i8259_irqs(void)
{
	int i;

	insert_resource(&ioport_resource, &pic1_io_resource);
	insert_resource(&ioport_resource, &pic2_io_resource);

	init_8259A(0);

	for (i = I8259A_IRQ_BASE; i < I8259A_IRQ_BASE + 16; i++) {
		irq_set_chip_and_handler(i, &i8259A_chip, handle_level_irq);
		irq_set_probe(i);
	}

	setup_irq(I8259A_IRQ_BASE + PIC_CASCADE_IR, &irq2);
}
