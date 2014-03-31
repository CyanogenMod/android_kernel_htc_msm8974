/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 Linus Torvalds
 * Copyright (C) 1994 - 2001, 2003, 07 Ralf Baechle
 */
#include <linux/clockchips.h>
#include <linux/i8253.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/irq.h>

#include <asm/irq_cpu.h>
#include <asm/i8259.h>
#include <asm/io.h>
#include <asm/jazz.h>
#include <asm/pgtable.h>
#include <asm/tlbmisc.h>

static DEFINE_RAW_SPINLOCK(r4030_lock);

static void enable_r4030_irq(struct irq_data *d)
{
	unsigned int mask = 1 << (d->irq - JAZZ_IRQ_START);
	unsigned long flags;

	raw_spin_lock_irqsave(&r4030_lock, flags);
	mask |= r4030_read_reg16(JAZZ_IO_IRQ_ENABLE);
	r4030_write_reg16(JAZZ_IO_IRQ_ENABLE, mask);
	raw_spin_unlock_irqrestore(&r4030_lock, flags);
}

void disable_r4030_irq(struct irq_data *d)
{
	unsigned int mask = ~(1 << (d->irq - JAZZ_IRQ_START));
	unsigned long flags;

	raw_spin_lock_irqsave(&r4030_lock, flags);
	mask &= r4030_read_reg16(JAZZ_IO_IRQ_ENABLE);
	r4030_write_reg16(JAZZ_IO_IRQ_ENABLE, mask);
	raw_spin_unlock_irqrestore(&r4030_lock, flags);
}

static struct irq_chip r4030_irq_type = {
	.name = "R4030",
	.irq_mask = disable_r4030_irq,
	.irq_unmask = enable_r4030_irq,
};

void __init init_r4030_ints(void)
{
	int i;

	for (i = JAZZ_IRQ_START; i <= JAZZ_IRQ_END; i++)
		irq_set_chip_and_handler(i, &r4030_irq_type, handle_level_irq);

	r4030_write_reg16(JAZZ_IO_IRQ_ENABLE, 0);
	r4030_read_reg16(JAZZ_IO_IRQ_SOURCE);		
	r4030_read_reg32(JAZZ_R4030_INVAL_ADDR);	
}

void __init arch_init_irq(void)
{

	
	add_wired_entry(0x02000017, 0x03c00017, 0xe0000000, PM_64K);
	
	add_wired_entry(0x02400017, 0x02440017, 0xe2000000, PM_16M);
	
	add_wired_entry(0x01800017, 0x01000017, 0xe4000000, PM_4M);

	init_i8259_irqs();			
	mips_cpu_irq_init();
	init_r4030_ints();

	change_c0_status(ST0_IM, IE_IRQ2 | IE_IRQ1);
}

asmlinkage void plat_irq_dispatch(void)
{
	unsigned int pending = read_c0_cause() & read_c0_status();
	unsigned int irq;

	if (pending & IE_IRQ4) {
		r4030_read_reg32(JAZZ_TIMER_REGISTER);
		do_IRQ(JAZZ_TIMER_IRQ);
	} else if (pending & IE_IRQ2) {
		irq = *(volatile u8 *)JAZZ_EISA_IRQ_ACK;
		do_IRQ(irq);
	} else if (pending & IE_IRQ1) {
		irq = *(volatile u8 *)JAZZ_IO_IRQ_SOURCE >> 2;
		if (likely(irq > 0))
			do_IRQ(irq + JAZZ_IRQ_START - 1);
		else
			panic("Unimplemented loc_no_irq handler");
	}
}

static void r4030_set_mode(enum clock_event_mode mode,
                           struct clock_event_device *evt)
{
	
}

struct clock_event_device r4030_clockevent = {
	.name		= "r4030",
	.features	= CLOCK_EVT_FEAT_PERIODIC,
	.rating		= 300,
	.irq		= JAZZ_TIMER_IRQ,
	.set_mode	= r4030_set_mode,
};

static irqreturn_t r4030_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *cd = dev_id;

	cd->event_handler(cd);
	return IRQ_HANDLED;
}

static struct irqaction r4030_timer_irqaction = {
	.handler	= r4030_timer_interrupt,
	.flags		= IRQF_TIMER,
	.name		= "R4030 timer",
};

void __init plat_time_init(void)
{
	struct clock_event_device *cd = &r4030_clockevent;
	struct irqaction *action = &r4030_timer_irqaction;
	unsigned int cpu = smp_processor_id();

	BUG_ON(HZ != 100);

	cd->cpumask             = cpumask_of(cpu);
	clockevents_register_device(cd);
	action->dev_id = cd;
	setup_irq(JAZZ_TIMER_IRQ, action);

	r4030_write_reg32(JAZZ_TIMER_INTERVAL, 9);
	setup_pit_timer();
}
