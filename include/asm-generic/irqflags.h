#ifndef __ASM_GENERIC_IRQFLAGS_H
#define __ASM_GENERIC_IRQFLAGS_H

#ifndef ARCH_IRQ_DISABLED
#define ARCH_IRQ_DISABLED 0
#define ARCH_IRQ_ENABLED 1
#endif

#ifndef arch_local_save_flags
unsigned long arch_local_save_flags(void);
#endif

#ifndef arch_local_irq_restore
void arch_local_irq_restore(unsigned long flags);
#endif

#ifndef arch_local_irq_save
static inline unsigned long arch_local_irq_save(void)
{
	unsigned long flags;
	flags = arch_local_save_flags();
	arch_local_irq_restore(ARCH_IRQ_DISABLED);
	return flags;
}
#endif

#ifndef arch_irqs_disabled_flags
static inline int arch_irqs_disabled_flags(unsigned long flags)
{
	return flags == ARCH_IRQ_DISABLED;
}
#endif

#ifndef arch_local_irq_enable
static inline void arch_local_irq_enable(void)
{
	arch_local_irq_restore(ARCH_IRQ_ENABLED);
}
#endif

#ifndef arch_local_irq_disable
static inline void arch_local_irq_disable(void)
{
	arch_local_irq_restore(ARCH_IRQ_DISABLED);
}
#endif

#ifndef arch_irqs_disabled
static inline int arch_irqs_disabled(void)
{
	return arch_irqs_disabled_flags(arch_local_save_flags());
}
#endif

#endif 
