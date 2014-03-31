/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel_stat.h>
#include <linux/uaccess.h>
#include <hv/drv_pcie_rc_intf.h>
#include <arch/spr_def.h>
#include <asm/traps.h>

#define IS_HW_CLEARED 1

DEFINE_PER_CPU(unsigned long long, interrupts_enabled_mask) =
  INITIAL_INTERRUPTS_ENABLED;
EXPORT_PER_CPU_SYMBOL(interrupts_enabled_mask);

DEFINE_PER_CPU(irq_cpustat_t, irq_stat) ____cacheline_internodealigned_in_smp;
EXPORT_PER_CPU_SYMBOL(irq_stat);

static DEFINE_PER_CPU(unsigned long, irq_disable_mask)
	____cacheline_internodealigned_in_smp;

static DEFINE_PER_CPU(int, irq_depth);

#if CHIP_HAS_IPI()
static unsigned long available_irqs = ~(1UL << IRQ_RESCHEDULE);
static DEFINE_SPINLOCK(available_irqs_lock);
#endif

#if CHIP_HAS_IPI()
#define mask_irqs(irq_mask) __insn_mtspr(SPR_IPI_MASK_SET_K, irq_mask)
#define unmask_irqs(irq_mask) __insn_mtspr(SPR_IPI_MASK_RESET_K, irq_mask)
#define clear_irqs(irq_mask) __insn_mtspr(SPR_IPI_EVENT_RESET_K, irq_mask)
#else
#define mask_irqs(irq_mask) hv_disable_intr(irq_mask)
#define unmask_irqs(irq_mask) hv_enable_intr(irq_mask)
#define clear_irqs(irq_mask) hv_clear_intr(irq_mask)
#endif

void tile_dev_intr(struct pt_regs *regs, int intnum)
{
	int depth = __get_cpu_var(irq_depth)++;
	unsigned long original_irqs;
	unsigned long remaining_irqs;
	struct pt_regs *old_regs;

#if CHIP_HAS_IPI()
	unsigned long masked = __insn_mfspr(SPR_IPI_MASK_K);
	original_irqs = __insn_mfspr(SPR_IPI_EVENT_K) & ~masked;
	__insn_mtspr(SPR_IPI_MASK_SET_K, original_irqs);
#else
	original_irqs = __insn_mfspr(SPR_SYSTEM_SAVE_K_3);
#endif
	remaining_irqs = original_irqs;

	
	old_regs = set_irq_regs(regs);
	irq_enter();

#ifdef CONFIG_DEBUG_STACKOVERFLOW
	
	{
		long sp = stack_pointer - (long) current_thread_info();
		if (unlikely(sp < (sizeof(struct thread_info) + STACK_WARN))) {
			pr_emerg("tile_dev_intr: "
			       "stack overflow: %ld\n",
			       sp - sizeof(struct thread_info));
			dump_stack();
		}
	}
#endif
	while (remaining_irqs) {
		unsigned long irq = __ffs(remaining_irqs);
		remaining_irqs &= ~(1UL << irq);

		
		if (irq != IRQ_RESCHEDULE)
			__get_cpu_var(irq_stat).irq_dev_intr_count++;

		generic_handle_irq(irq);
	}

	if (depth == 0)
		unmask_irqs(~__get_cpu_var(irq_disable_mask));

	__get_cpu_var(irq_depth)--;

	irq_exit();
	set_irq_regs(old_regs);
}


static void tile_irq_chip_enable(struct irq_data *d)
{
	get_cpu_var(irq_disable_mask) &= ~(1UL << d->irq);
	if (__get_cpu_var(irq_depth) == 0)
		unmask_irqs(1UL << d->irq);
	put_cpu_var(irq_disable_mask);
}

static void tile_irq_chip_disable(struct irq_data *d)
{
	get_cpu_var(irq_disable_mask) |= (1UL << d->irq);
	mask_irqs(1UL << d->irq);
	put_cpu_var(irq_disable_mask);
}

static void tile_irq_chip_mask(struct irq_data *d)
{
	mask_irqs(1UL << d->irq);
}

static void tile_irq_chip_unmask(struct irq_data *d)
{
	unmask_irqs(1UL << d->irq);
}

static void tile_irq_chip_ack(struct irq_data *d)
{
	if ((unsigned long)irq_data_get_irq_chip_data(d) != IS_HW_CLEARED)
		clear_irqs(1UL << d->irq);
}

static void tile_irq_chip_eoi(struct irq_data *d)
{
	if (!(__get_cpu_var(irq_disable_mask) & (1UL << d->irq)))
		unmask_irqs(1UL << d->irq);
}

static struct irq_chip tile_irq_chip = {
	.name = "tile_irq_chip",
	.irq_enable = tile_irq_chip_enable,
	.irq_disable = tile_irq_chip_disable,
	.irq_ack = tile_irq_chip_ack,
	.irq_eoi = tile_irq_chip_eoi,
	.irq_mask = tile_irq_chip_mask,
	.irq_unmask = tile_irq_chip_unmask,
};

void __init init_IRQ(void)
{
	ipi_init();
}

void __cpuinit setup_irq_regs(void)
{
	
	unmask_irqs(~0UL);
#if CHIP_HAS_IPI()
	arch_local_irq_unmask(INT_IPI_K);
#endif
}

void tile_irq_activate(unsigned int irq, int tile_irq_type)
{
	irq_flow_handler_t handle = handle_level_irq;
	if (tile_irq_type == TILE_IRQ_PERCPU)
		handle = handle_percpu_irq;
	irq_set_chip_and_handler(irq, &tile_irq_chip, handle);

	if (tile_irq_type == TILE_IRQ_HW_CLEAR)
		irq_set_chip_data(irq, (void *)IS_HW_CLEARED);
}
EXPORT_SYMBOL(tile_irq_activate);


void ack_bad_irq(unsigned int irq)
{
	pr_err("unexpected IRQ trap at vector %02x\n", irq);
}


#if CHIP_HAS_IPI()
int create_irq(void)
{
	unsigned long flags;
	int result;

	spin_lock_irqsave(&available_irqs_lock, flags);
	if (available_irqs == 0)
		result = -ENOMEM;
	else {
		result = __ffs(available_irqs);
		available_irqs &= ~(1UL << result);
		dynamic_irq_init(result);
	}
	spin_unlock_irqrestore(&available_irqs_lock, flags);

	return result;
}
EXPORT_SYMBOL(create_irq);

void destroy_irq(unsigned int irq)
{
	unsigned long flags;

	spin_lock_irqsave(&available_irqs_lock, flags);
	available_irqs |= (1UL << irq);
	dynamic_irq_cleanup(irq);
	spin_unlock_irqrestore(&available_irqs_lock, flags);
}
EXPORT_SYMBOL(destroy_irq);
#endif
