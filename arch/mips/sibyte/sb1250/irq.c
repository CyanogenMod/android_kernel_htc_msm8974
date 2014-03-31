/*
 * Copyright (C) 2000, 2001, 2002, 2003 Broadcom Corporation
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
#include <linux/spinlock.h>
#include <linux/smp.h>
#include <linux/mm.h>
#include <linux/kernel_stat.h>

#include <asm/errno.h>
#include <asm/signal.h>
#include <asm/time.h>
#include <asm/io.h>

#include <asm/sibyte/sb1250_regs.h>
#include <asm/sibyte/sb1250_int.h>
#include <asm/sibyte/sb1250_uart.h>
#include <asm/sibyte/sb1250_scd.h>
#include <asm/sibyte/sb1250.h>


#ifdef CONFIG_SIBYTE_HAS_LDT
extern unsigned long ldt_eoi_space;
#endif

int sb1250_irq_owner[SB1250_NR_IRQS];

static DEFINE_RAW_SPINLOCK(sb1250_imr_lock);

void sb1250_mask_irq(int cpu, int irq)
{
	unsigned long flags;
	u64 cur_ints;

	raw_spin_lock_irqsave(&sb1250_imr_lock, flags);
	cur_ints = ____raw_readq(IOADDR(A_IMR_MAPPER(cpu) +
					R_IMR_INTERRUPT_MASK));
	cur_ints |= (((u64) 1) << irq);
	____raw_writeq(cur_ints, IOADDR(A_IMR_MAPPER(cpu) +
					R_IMR_INTERRUPT_MASK));
	raw_spin_unlock_irqrestore(&sb1250_imr_lock, flags);
}

void sb1250_unmask_irq(int cpu, int irq)
{
	unsigned long flags;
	u64 cur_ints;

	raw_spin_lock_irqsave(&sb1250_imr_lock, flags);
	cur_ints = ____raw_readq(IOADDR(A_IMR_MAPPER(cpu) +
					R_IMR_INTERRUPT_MASK));
	cur_ints &= ~(((u64) 1) << irq);
	____raw_writeq(cur_ints, IOADDR(A_IMR_MAPPER(cpu) +
					R_IMR_INTERRUPT_MASK));
	raw_spin_unlock_irqrestore(&sb1250_imr_lock, flags);
}

#ifdef CONFIG_SMP
static int sb1250_set_affinity(struct irq_data *d, const struct cpumask *mask,
			       bool force)
{
	int i = 0, old_cpu, cpu, int_on;
	unsigned int irq = d->irq;
	u64 cur_ints;
	unsigned long flags;

	i = cpumask_first(mask);

	
	cpu = cpu_logical_map(i);

	
	raw_spin_lock_irqsave(&sb1250_imr_lock, flags);

	
	old_cpu = sb1250_irq_owner[irq];
	cur_ints = ____raw_readq(IOADDR(A_IMR_MAPPER(old_cpu) +
					R_IMR_INTERRUPT_MASK));
	int_on = !(cur_ints & (((u64) 1) << irq));
	if (int_on) {
		
		cur_ints |= (((u64) 1) << irq);
		____raw_writeq(cur_ints, IOADDR(A_IMR_MAPPER(old_cpu) +
					R_IMR_INTERRUPT_MASK));
	}
	sb1250_irq_owner[irq] = cpu;
	if (int_on) {
		
		cur_ints = ____raw_readq(IOADDR(A_IMR_MAPPER(cpu) +
					R_IMR_INTERRUPT_MASK));
		cur_ints &= ~(((u64) 1) << irq);
		____raw_writeq(cur_ints, IOADDR(A_IMR_MAPPER(cpu) +
					R_IMR_INTERRUPT_MASK));
	}
	raw_spin_unlock_irqrestore(&sb1250_imr_lock, flags);

	return 0;
}
#endif

static void disable_sb1250_irq(struct irq_data *d)
{
	unsigned int irq = d->irq;

	sb1250_mask_irq(sb1250_irq_owner[irq], irq);
}

static void enable_sb1250_irq(struct irq_data *d)
{
	unsigned int irq = d->irq;

	sb1250_unmask_irq(sb1250_irq_owner[irq], irq);
}


static void ack_sb1250_irq(struct irq_data *d)
{
	unsigned int irq = d->irq;
#ifdef CONFIG_SIBYTE_HAS_LDT
	u64 pending;

	pending = __raw_readq(IOADDR(A_IMR_REGISTER(sb1250_irq_owner[irq],
						    R_IMR_LDT_INTERRUPT)));
	pending &= ((u64)1 << (irq));
	if (pending) {
		int i;
		for (i=0; i<NR_CPUS; i++) {
			int cpu;
#ifdef CONFIG_SMP
			cpu = cpu_logical_map(i);
#else
			cpu = i;
#endif
			__raw_writeq(pending,
				     IOADDR(A_IMR_REGISTER(cpu,
						R_IMR_LDT_INTERRUPT_CLR)));
		}

		*(uint32_t *)(ldt_eoi_space+(irq<<16)+(7<<2)) = 0;
	}
#endif
	sb1250_mask_irq(sb1250_irq_owner[irq], irq);
}

static struct irq_chip sb1250_irq_type = {
	.name = "SB1250-IMR",
	.irq_mask_ack = ack_sb1250_irq,
	.irq_unmask = enable_sb1250_irq,
	.irq_mask = disable_sb1250_irq,
#ifdef CONFIG_SMP
	.irq_set_affinity = sb1250_set_affinity
#endif
};

void __init init_sb1250_irqs(void)
{
	int i;

	for (i = 0; i < SB1250_NR_IRQS; i++) {
		irq_set_chip_and_handler(i, &sb1250_irq_type,
					 handle_level_irq);
		sb1250_irq_owner[i] = 0;
	}
}



#define IMR_IP2_VAL	K_INT_MAP_I0
#define IMR_IP3_VAL	K_INT_MAP_I1
#define IMR_IP4_VAL	K_INT_MAP_I2
#define IMR_IP5_VAL	K_INT_MAP_I3
#define IMR_IP6_VAL	K_INT_MAP_I4

void __init arch_init_irq(void)
{

	unsigned int i;
	u64 tmp;
	unsigned int imask = STATUSF_IP4 | STATUSF_IP3 | STATUSF_IP2 |
		STATUSF_IP1 | STATUSF_IP0;

	
	for (i = 0; i < SB1250_NR_IRQS; i++) {	
		__raw_writeq(IMR_IP2_VAL,
			     IOADDR(A_IMR_REGISTER(0,
						   R_IMR_INTERRUPT_MAP_BASE) +
				    (i << 3)));
		__raw_writeq(IMR_IP2_VAL,
			     IOADDR(A_IMR_REGISTER(1,
						   R_IMR_INTERRUPT_MAP_BASE) +
				    (i << 3)));
	}

	init_sb1250_irqs();

	
	__raw_writeq(IMR_IP3_VAL,
		     IOADDR(A_IMR_REGISTER(0, R_IMR_INTERRUPT_MAP_BASE) +
			    (K_INT_MBOX_0 << 3)));
	__raw_writeq(IMR_IP3_VAL,
		     IOADDR(A_IMR_REGISTER(1, R_IMR_INTERRUPT_MAP_BASE) +
			    (K_INT_MBOX_0 << 3)));

	
	__raw_writeq(0xffffffffffffffffULL,
		     IOADDR(A_IMR_REGISTER(0, R_IMR_MAILBOX_CLR_CPU)));
	__raw_writeq(0xffffffffffffffffULL,
		     IOADDR(A_IMR_REGISTER(1, R_IMR_MAILBOX_CLR_CPU)));

	
	tmp = ~((u64) 0) ^ (((u64) 1) << K_INT_MBOX_0);
	__raw_writeq(tmp, IOADDR(A_IMR_REGISTER(0, R_IMR_INTERRUPT_MASK)));
	__raw_writeq(tmp, IOADDR(A_IMR_REGISTER(1, R_IMR_INTERRUPT_MASK)));


	
	change_c0_status(ST0_IM, imask);
}

extern void sb1250_mailbox_interrupt(void);

static inline void dispatch_ip2(void)
{
	unsigned int cpu = smp_processor_id();
	unsigned long long mask;

	mask = __raw_readq(IOADDR(A_IMR_REGISTER(cpu,
				  R_IMR_INTERRUPT_STATUS_BASE)));
	if (mask)
		do_IRQ(fls64(mask) - 1);
}

asmlinkage void plat_irq_dispatch(void)
{
	unsigned int cpu = smp_processor_id();
	unsigned int pending;


	pending = read_c0_cause() & read_c0_status() & ST0_IM;

	if (pending & CAUSEF_IP7) 
		do_IRQ(MIPS_CPU_IRQ_BASE + 7);
	else if (pending & CAUSEF_IP4)
		do_IRQ(K_INT_TIMER_0 + cpu); 	

#ifdef CONFIG_SMP
	else if (pending & CAUSEF_IP3)
		sb1250_mailbox_interrupt();
#endif

	else if (pending & CAUSEF_IP2)
		dispatch_ip2();
	else
		spurious_interrupt();
}
