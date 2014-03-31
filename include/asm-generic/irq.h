#ifndef __ASM_GENERIC_IRQ_H
#define __ASM_GENERIC_IRQ_H

#ifndef NR_IRQS
#define NR_IRQS 64
#endif

static inline int irq_canonicalize(int irq)
{
	return irq;
}

#endif 
