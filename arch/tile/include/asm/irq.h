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

#ifndef _ASM_TILE_IRQ_H
#define _ASM_TILE_IRQ_H

#include <linux/hardirq.h>

#define NR_IRQS 32

#define IRQ_RESCHEDULE 0

#define irq_canonicalize(irq)   (irq)

void ack_bad_irq(unsigned int irq);

enum {
	
	TILE_IRQ_PERCPU,
	
	TILE_IRQ_HW_CLEAR,
	
	TILE_IRQ_SW_CLEAR,
};


void tile_irq_activate(unsigned int irq, int tile_irq_type);

void setup_irq_regs(void);

#endif 
