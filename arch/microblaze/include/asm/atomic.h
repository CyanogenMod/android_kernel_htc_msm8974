#ifndef _ASM_MICROBLAZE_ATOMIC_H
#define _ASM_MICROBLAZE_ATOMIC_H

#include <asm/cmpxchg.h>
#include <asm-generic/atomic.h>
#include <asm-generic/atomic64.h>

static inline int atomic_dec_if_positive(atomic_t *v)
{
	unsigned long flags;
	int res;

	local_irq_save(flags);
	res = v->counter - 1;
	if (res >= 0)
		v->counter = res;
	local_irq_restore(flags);

	return res;
}

#endif 
