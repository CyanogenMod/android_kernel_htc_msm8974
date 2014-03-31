/*
 * Copyright 2011-2012, Meador Inge, Mentor Graphics Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2 of the
 * License.
 *
 */

#ifndef _ASM_MPIC_MSGR_H
#define _ASM_MPIC_MSGR_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <asm/smp.h>

struct mpic_msgr {
	u32 __iomem *base;
	u32 __iomem *mer;
	int irq;
	unsigned char in_use;
	raw_spinlock_t lock;
	int num;
};

extern struct mpic_msgr *mpic_msgr_get(unsigned int reg_num);

extern void mpic_msgr_put(struct mpic_msgr *msgr);

extern void mpic_msgr_enable(struct mpic_msgr *msgr);

extern void mpic_msgr_disable(struct mpic_msgr *msgr);

/* Write a message to a message register
 *
 * @msgr:	the message register to write to
 * @message:	the message to write
 *
 * The given 32-bit message is written to the given message
 * register.  Writing to an enabled message registers fires
 * an interrupt.
 */
static inline void mpic_msgr_write(struct mpic_msgr *msgr, u32 message)
{
	out_be32(msgr->base, message);
}

static inline u32 mpic_msgr_read(struct mpic_msgr *msgr)
{
	return in_be32(msgr->base);
}

static inline void mpic_msgr_clear(struct mpic_msgr *msgr)
{
	(void) mpic_msgr_read(msgr);
}

static inline void mpic_msgr_set_destination(struct mpic_msgr *msgr,
					     u32 cpu_num)
{
	out_be32(msgr->base, 1 << get_hard_smp_processor_id(cpu_num));
}

static inline int mpic_msgr_get_irq(struct mpic_msgr *msgr)
{
	return msgr->irq;
}

#endif
