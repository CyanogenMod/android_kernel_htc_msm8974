#ifndef _ASM_IRQFLAGS_H
#define _ASM_IRQFLAGS_H

#include <asm/pil.h>

#ifndef __ASSEMBLY__

static inline notrace unsigned long arch_local_save_flags(void)
{
	unsigned long flags;

	__asm__ __volatile__(
		"rdpr	%%pil, %0"
		: "=r" (flags)
	);

	return flags;
}

static inline notrace void arch_local_irq_restore(unsigned long flags)
{
	__asm__ __volatile__(
		"wrpr	%0, %%pil"
		: 
		: "r" (flags)
		: "memory"
	);
}

static inline notrace void arch_local_irq_disable(void)
{
	__asm__ __volatile__(
		"wrpr	%0, %%pil"
		: 
		: "i" (PIL_NORMAL_MAX)
		: "memory"
	);
}

static inline notrace void arch_local_irq_enable(void)
{
	__asm__ __volatile__(
		"wrpr	0, %%pil"
		: 
		: 
		: "memory"
	);
}

static inline notrace int arch_irqs_disabled_flags(unsigned long flags)
{
	return (flags > 0);
}

static inline notrace int arch_irqs_disabled(void)
{
	return arch_irqs_disabled_flags(arch_local_save_flags());
}

static inline notrace unsigned long arch_local_irq_save(void)
{
	unsigned long flags, tmp;

	__asm__ __volatile__(
		"rdpr	%%pil, %0\n\t"
		"or	%0, %2, %1\n\t"
		"wrpr	%1, 0x0, %%pil"
		: "=r" (flags), "=r" (tmp)
		: "i" (PIL_NORMAL_MAX)
		: "memory"
	);

	return flags;
}

#endif 

#endif 
