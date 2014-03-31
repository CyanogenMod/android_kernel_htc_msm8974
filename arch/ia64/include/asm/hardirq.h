#ifndef _ASM_IA64_HARDIRQ_H
#define _ASM_IA64_HARDIRQ_H



#define __ARCH_IRQ_STAT	1

#define local_softirq_pending()		(local_cpu_data->softirq_pending)

#include <linux/threads.h>
#include <linux/irq.h>

#include <asm/processor.h>

extern void __iomem *ipi_base_addr;

void ack_bad_irq(unsigned int irq);

#endif 
