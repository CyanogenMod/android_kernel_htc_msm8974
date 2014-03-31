#ifndef _ASM_IA64_IRQ_H
#define _ASM_IA64_IRQ_H

/*
 * Copyright (C) 1999-2000, 2002 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *	Stephane Eranian <eranian@hpl.hp.com>
 *
 * 11/24/98	S.Eranian 	updated TIMER_IRQ and irq_canonicalize
 * 01/20/99	S.Eranian	added keyboard interrupt
 * 02/29/00     D.Mosberger	moved most things into hw_irq.h
 */

#include <linux/types.h>
#include <linux/cpumask.h>
#include <generated/nr-irqs.h>

static __inline__ int
irq_canonicalize (int irq)
{
	return ((irq == 2) ? 9 : irq);
}

extern void set_irq_affinity_info (unsigned int irq, int dest, int redir);
bool is_affinity_mask_valid(const struct cpumask *cpumask);

#define is_affinity_mask_valid is_affinity_mask_valid

#endif 
