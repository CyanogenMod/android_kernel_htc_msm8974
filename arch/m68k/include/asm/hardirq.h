#ifndef __M68K_HARDIRQ_H
#define __M68K_HARDIRQ_H

#include <linux/threads.h>
#include <linux/cache.h>
#include <asm/irq.h>

#define HARDIRQ_BITS	8

#if (1 << HARDIRQ_BITS) < NR_IRQS
# error HARDIRQ_BITS is too low!
#endif

#ifdef CONFIG_MMU

static inline void ack_bad_irq(unsigned int irq)
{
	pr_crit("unexpected IRQ trap at vector %02x\n", irq);
}

typedef struct {
	unsigned int __softirq_pending;
} ____cacheline_aligned irq_cpustat_t;

#include <linux/irq_cpustat.h>	

#else

#include <asm-generic/hardirq.h>

#endif 

#endif
