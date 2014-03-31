/*
 * Copyright (C) 2000,2001,2002,2003,2004 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/linkage.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/kernel_stat.h>

#include <asm/errno.h>
#include <asm/irq_regs.h>
#include <asm/signal.h>
#include <asm/io.h>

#include <asm/sibyte/bcm1480_regs.h>
#include <asm/sibyte/bcm1480_int.h>
#include <asm/sibyte/bcm1480_scd.h>

#include <asm/sibyte/sb1250_uart.h>
#include <asm/sibyte/sb1250.h>


#ifdef CONFIG_PCI
extern unsigned long ht_eoi_space;
#endif

int bcm1480_irq_owner[BCM1480_NR_IRQS];

static DEFINE_RAW_SPINLOCK(bcm1480_imr_lock);

void bcm1480_mask_irq(int cpu, int irq)
{
	unsigned long flags, hl_spacing;
	u64 cur_ints;

	raw_spin_lock_irqsave(&bcm1480_imr_lock, flags);
	hl_spacing = 0;
	if ((irq >= BCM1480_NR_IRQS_HALF) && (irq <= BCM1480_NR_IRQS)) {
		hl_spacing = BCM1480_IMR_HL_SPACING;
		irq -= BCM1480_NR_IRQS_HALF;
	}
	cur_ints = ____raw_readq(IOADDR(A_BCM1480_IMR_MAPPER(cpu) + R_BCM1480_IMR_INTERRUPT_MASK_H + hl_spacing));
	cur_ints |= (((u64) 1) << irq);
	____raw_writeq(cur_ints, IOADDR(A_BCM1480_IMR_MAPPER(cpu) + R_BCM1480_IMR_INTERRUPT_MASK_H + hl_spacing));
	raw_spin_unlock_irqrestore(&bcm1480_imr_lock, flags);
}

void bcm1480_unmask_irq(int cpu, int irq)
{
	unsigned long flags, hl_spacing;
	u64 cur_ints;

	raw_spin_lock_irqsave(&bcm1480_imr_lock, flags);
	hl_spacing = 0;
	if ((irq >= BCM1480_NR_IRQS_HALF) && (irq <= BCM1480_NR_IRQS)) {
		hl_spacing = BCM1480_IMR_HL_SPACING;
		irq -= BCM1480_NR_IRQS_HALF;
	}
	cur_ints = ____raw_readq(IOADDR(A_BCM1480_IMR_MAPPER(cpu) + R_BCM1480_IMR_INTERRUPT_MASK_H + hl_spacing));
	cur_ints &= ~(((u64) 1) << irq);
	____raw_writeq(cur_ints, IOADDR(A_BCM1480_IMR_MAPPER(cpu) + R_BCM1480_IMR_INTERRUPT_MASK_H + hl_spacing));
	raw_spin_unlock_irqrestore(&bcm1480_imr_lock, flags);
}

#ifdef CONFIG_SMP
static int bcm1480_set_affinity(struct irq_data *d, const struct cpumask *mask,
				bool force)
{
	unsigned int irq_dirty, irq = d->irq;
	int i = 0, old_cpu, cpu, int_on, k;
	u64 cur_ints;
	unsigned long flags;

	i = cpumask_first(mask);

	
	cpu = cpu_logical_map(i);

	
	raw_spin_lock_irqsave(&bcm1480_imr_lock, flags);

	
	old_cpu = bcm1480_irq_owner[irq];
	irq_dirty = irq;
	if ((irq_dirty >= BCM1480_NR_IRQS_HALF) && (irq_dirty <= BCM1480_NR_IRQS)) {
		irq_dirty -= BCM1480_NR_IRQS_HALF;
	}

	for (k=0; k<2; k++) { 
		cur_ints = ____raw_readq(IOADDR(A_BCM1480_IMR_MAPPER(old_cpu) + R_BCM1480_IMR_INTERRUPT_MASK_H + (k*BCM1480_IMR_HL_SPACING)));
		int_on = !(cur_ints & (((u64) 1) << irq_dirty));
		if (int_on) {
			
			cur_ints |= (((u64) 1) << irq_dirty);
			____raw_writeq(cur_ints, IOADDR(A_BCM1480_IMR_MAPPER(old_cpu) + R_BCM1480_IMR_INTERRUPT_MASK_H + (k*BCM1480_IMR_HL_SPACING)));
		}
		bcm1480_irq_owner[irq] = cpu;
		if (int_on) {
			
			cur_ints = ____raw_readq(IOADDR(A_BCM1480_IMR_MAPPER(cpu) + R_BCM1480_IMR_INTERRUPT_MASK_H + (k*BCM1480_IMR_HL_SPACING)));
			cur_ints &= ~(((u64) 1) << irq_dirty);
			____raw_writeq(cur_ints, IOADDR(A_BCM1480_IMR_MAPPER(cpu) + R_BCM1480_IMR_INTERRUPT_MASK_H + (k*BCM1480_IMR_HL_SPACING)));
		}
	}
	raw_spin_unlock_irqrestore(&bcm1480_imr_lock, flags);

	return 0;
}
#endif



static void disable_bcm1480_irq(struct irq_data *d)
{
	unsigned int irq = d->irq;

	bcm1480_mask_irq(bcm1480_irq_owner[irq], irq);
}

static void enable_bcm1480_irq(struct irq_data *d)
{
	unsigned int irq = d->irq;

	bcm1480_unmask_irq(bcm1480_irq_owner[irq], irq);
}


static void ack_bcm1480_irq(struct irq_data *d)
{
	unsigned int irq_dirty, irq = d->irq;
	u64 pending;
	int k;

	irq_dirty = irq;
	if ((irq_dirty >= BCM1480_NR_IRQS_HALF) && (irq_dirty <= BCM1480_NR_IRQS)) {
		irq_dirty -= BCM1480_NR_IRQS_HALF;
	}
	for (k=0; k<2; k++) { 
		pending = __raw_readq(IOADDR(A_BCM1480_IMR_REGISTER(bcm1480_irq_owner[irq],
						R_BCM1480_IMR_LDT_INTERRUPT_H + (k*BCM1480_IMR_HL_SPACING))));
		pending &= ((u64)1 << (irq_dirty));
		if (pending) {
#ifdef CONFIG_SMP
			int i;
			for (i=0; i<NR_CPUS; i++) {
				__raw_writeq(pending, IOADDR(A_BCM1480_IMR_REGISTER(cpu_logical_map(i),
								R_BCM1480_IMR_LDT_INTERRUPT_CLR_H + (k*BCM1480_IMR_HL_SPACING))));
			}
#else
			__raw_writeq(pending, IOADDR(A_BCM1480_IMR_REGISTER(0, R_BCM1480_IMR_LDT_INTERRUPT_CLR_H + (k*BCM1480_IMR_HL_SPACING))));
#endif

#ifdef CONFIG_PCI
			if (ht_eoi_space)
				*(uint32_t *)(ht_eoi_space+(irq<<16)+(7<<2)) = 0;
#endif
		}
	}
	bcm1480_mask_irq(bcm1480_irq_owner[irq], irq);
}

static struct irq_chip bcm1480_irq_type = {
	.name = "BCM1480-IMR",
	.irq_mask_ack = ack_bcm1480_irq,
	.irq_mask = disable_bcm1480_irq,
	.irq_unmask = enable_bcm1480_irq,
#ifdef CONFIG_SMP
	.irq_set_affinity = bcm1480_set_affinity
#endif
};

void __init init_bcm1480_irqs(void)
{
	int i;

	for (i = 0; i < BCM1480_NR_IRQS; i++) {
		irq_set_chip_and_handler(i, &bcm1480_irq_type,
					 handle_level_irq);
		bcm1480_irq_owner[i] = 0;
	}
}


#define IMR_IP2_VAL	K_BCM1480_INT_MAP_I0
#define IMR_IP3_VAL	K_BCM1480_INT_MAP_I1
#define IMR_IP4_VAL	K_BCM1480_INT_MAP_I2
#define IMR_IP5_VAL	K_BCM1480_INT_MAP_I3
#define IMR_IP6_VAL	K_BCM1480_INT_MAP_I4

void __init arch_init_irq(void)
{
	unsigned int i, cpu;
	u64 tmp;
	unsigned int imask = STATUSF_IP4 | STATUSF_IP3 | STATUSF_IP2 |
		STATUSF_IP1 | STATUSF_IP0;

	
	
	for (i = 1; i < BCM1480_NR_IRQS_HALF; i++) {	
		for (cpu = 0; cpu < 4; cpu++) {
			__raw_writeq(IMR_IP2_VAL,
				     IOADDR(A_BCM1480_IMR_REGISTER(cpu,
								   R_BCM1480_IMR_INTERRUPT_MAP_BASE_H) + (i << 3)));
		}
	}

	
	for (i = 0; i < BCM1480_NR_IRQS_HALF; i++) {
		for (cpu = 0; cpu < 4; cpu++) {
			__raw_writeq(IMR_IP2_VAL,
				     IOADDR(A_BCM1480_IMR_REGISTER(cpu,
								   R_BCM1480_IMR_INTERRUPT_MAP_BASE_L) + (i << 3)));
		}
	}

	init_bcm1480_irqs();

	
	for (cpu = 0; cpu < 4; cpu++) {
		__raw_writeq(IMR_IP3_VAL, IOADDR(A_BCM1480_IMR_REGISTER(cpu, R_BCM1480_IMR_INTERRUPT_MAP_BASE_H) +
						 (K_BCM1480_INT_MBOX_0_0 << 3)));
        }


	
	for (cpu = 0; cpu < 4; cpu++) {
		__raw_writeq(0xffffffffffffffffULL,
			     IOADDR(A_BCM1480_IMR_REGISTER(cpu, R_BCM1480_IMR_MAILBOX_0_CLR_CPU)));
		__raw_writeq(0xffffffffffffffffULL,
			     IOADDR(A_BCM1480_IMR_REGISTER(cpu, R_BCM1480_IMR_MAILBOX_1_CLR_CPU)));
	}


	
	tmp = ~((u64) 0) ^ ( (((u64) 1) << K_BCM1480_INT_MBOX_0_0));
	for (cpu = 0; cpu < 4; cpu++) {
		__raw_writeq(tmp, IOADDR(A_BCM1480_IMR_REGISTER(cpu, R_BCM1480_IMR_INTERRUPT_MASK_H)));
	}
	tmp = ~((u64) 0);
	for (cpu = 0; cpu < 4; cpu++) {
		__raw_writeq(tmp, IOADDR(A_BCM1480_IMR_REGISTER(cpu, R_BCM1480_IMR_INTERRUPT_MASK_L)));
	}


	
	change_c0_status(ST0_IM, imask);
}

extern void bcm1480_mailbox_interrupt(void);

static inline void dispatch_ip2(void)
{
	unsigned long long mask_h, mask_l;
	unsigned int cpu = smp_processor_id();
	unsigned long base;

	base = A_BCM1480_IMR_MAPPER(cpu);
	mask_h = __raw_readq(
		IOADDR(base + R_BCM1480_IMR_INTERRUPT_STATUS_BASE_H));
	mask_l = __raw_readq(
		IOADDR(base + R_BCM1480_IMR_INTERRUPT_STATUS_BASE_L));

	if (mask_h) {
		if (mask_h ^ 1)
			do_IRQ(fls64(mask_h) - 1);
		else if (mask_l)
			do_IRQ(63 + fls64(mask_l));
	}
}

asmlinkage void plat_irq_dispatch(void)
{
	unsigned int cpu = smp_processor_id();
	unsigned int pending;

#ifdef CONFIG_SIBYTE_BCM1480_PROF
	
	write_c0_compare(read_c0_count());
#endif

	pending = read_c0_cause() & read_c0_status();

#ifdef CONFIG_SIBYTE_BCM1480_PROF
	if (pending & CAUSEF_IP7)	
		sbprof_cpu_intr();
	else
#endif

	if (pending & CAUSEF_IP4)
		do_IRQ(K_BCM1480_INT_TIMER_0 + cpu);
#ifdef CONFIG_SMP
	else if (pending & CAUSEF_IP3)
		bcm1480_mailbox_interrupt();
#endif

	else if (pending & CAUSEF_IP2)
		dispatch_ip2();
}
