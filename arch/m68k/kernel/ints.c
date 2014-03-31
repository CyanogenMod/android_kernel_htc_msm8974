/*
 * linux/arch/m68k/kernel/ints.c -- Linux/m68k general interrupt handling code
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/errno.h>
#include <linux/init.h>

#include <asm/setup.h>
#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/page.h>
#include <asm/machdep.h>
#include <asm/cacheflush.h>
#include <asm/irq_regs.h>

#ifdef CONFIG_Q40
#include <asm/q40ints.h>
#endif

extern u32 auto_irqhandler_fixup[];
extern u16 user_irqvec_fixup[];

static int m68k_first_user_vec;

static struct irq_chip auto_irq_chip = {
	.name		= "auto",
	.irq_startup	= m68k_irq_startup,
	.irq_shutdown	= m68k_irq_shutdown,
};

static struct irq_chip user_irq_chip = {
	.name		= "user",
	.irq_startup	= m68k_irq_startup,
	.irq_shutdown	= m68k_irq_shutdown,
};


void __init init_IRQ(void)
{
	int i;

	
	if (HARDIRQ_MASK != 0x00ff0000) {
		extern void hardirq_mask_is_broken(void);
		hardirq_mask_is_broken();
	}

	for (i = IRQ_AUTO_1; i <= IRQ_AUTO_7; i++)
		irq_set_chip_and_handler(i, &auto_irq_chip, handle_simple_irq);

	mach_init_IRQ();
}

void __init m68k_setup_auto_interrupt(void (*handler)(unsigned int, struct pt_regs *))
{
	if (handler)
		*auto_irqhandler_fixup = (u32)handler;
	flush_icache();
}

void __init m68k_setup_user_interrupt(unsigned int vec, unsigned int cnt)
{
	int i;

	BUG_ON(IRQ_USER + cnt > NR_IRQS);
	m68k_first_user_vec = vec;
	for (i = 0; i < cnt; i++)
		irq_set_chip(IRQ_USER + i, &user_irq_chip);
	*user_irqvec_fixup = vec - IRQ_USER;
	flush_icache();
}

void m68k_setup_irq_controller(struct irq_chip *chip,
			       irq_flow_handler_t handle, unsigned int irq,
			       unsigned int cnt)
{
	int i;

	for (i = 0; i < cnt; i++) {
		irq_set_chip(irq + i, chip);
		if (handle)
			irq_set_handler(irq + i, handle);
	}
}

unsigned int m68k_irq_startup_irq(unsigned int irq)
{
	if (irq <= IRQ_AUTO_7)
		vectors[VEC_SPUR + irq] = auto_inthandler;
	else
		vectors[m68k_first_user_vec + irq - IRQ_USER] = user_inthandler;
	return 0;
}

unsigned int m68k_irq_startup(struct irq_data *data)
{
	return m68k_irq_startup_irq(data->irq);
}

void m68k_irq_shutdown(struct irq_data *data)
{
	unsigned int irq = data->irq;

	if (irq <= IRQ_AUTO_7)
		vectors[VEC_SPUR + irq] = bad_inthandler;
	else
		vectors[m68k_first_user_vec + irq - IRQ_USER] = bad_inthandler;
}


unsigned int irq_canonicalize(unsigned int irq)
{
#ifdef CONFIG_Q40
	if (MACH_IS_Q40 && irq == 11)
		irq = 10;
#endif
	return irq;
}

EXPORT_SYMBOL(irq_canonicalize);


asmlinkage void handle_badint(struct pt_regs *regs)
{
	atomic_inc(&irq_err_count);
	pr_warn("unexpected interrupt from %u\n", regs->vector);
}
