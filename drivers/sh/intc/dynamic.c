/*
 * Dynamic IRQ management
 *
 * Copyright (C) 2010  Paul Mundt
 *
 * Modelled after arch/x86/kernel/apic/io_apic.c
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#define pr_fmt(fmt) "intc: " fmt

#include <linux/irq.h>
#include <linux/bitmap.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include "internals.h" 


unsigned int create_irq_nr(unsigned int irq_want, int node)
{
	int irq = irq_alloc_desc_at(irq_want, node);
	if (irq < 0)
		return 0;

	activate_irq(irq);
	return irq;
}

int create_irq(void)
{
	int irq = irq_alloc_desc(numa_node_id());
	if (irq >= 0)
		activate_irq(irq);

	return irq;
}

void destroy_irq(unsigned int irq)
{
	irq_free_desc(irq);
}

void reserve_intc_vectors(struct intc_vect *vectors, unsigned int nr_vecs)
{
	int i;

	for (i = 0; i < nr_vecs; i++)
		irq_reserve_irq(evt2irq(vectors[i].vect));
}
