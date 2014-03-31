#ifndef _XEN_MULTICALLS_H
#define _XEN_MULTICALLS_H

#include <trace/events/xen.h>

#include "xen-ops.h"

struct multicall_space
{
	struct multicall_entry *mc;
	void *args;
};

struct multicall_space __xen_mc_entry(size_t args);

DECLARE_PER_CPU(unsigned long, xen_mc_irq_flags);

static inline void xen_mc_batch(void)
{
	unsigned long flags;

	
	local_irq_save(flags);
	trace_xen_mc_batch(paravirt_get_lazy_mode());
	__this_cpu_write(xen_mc_irq_flags, flags);
}

static inline struct multicall_space xen_mc_entry(size_t args)
{
	xen_mc_batch();
	return __xen_mc_entry(args);
}

void xen_mc_flush(void);

static inline void xen_mc_issue(unsigned mode)
{
	trace_xen_mc_issue(mode);

	if ((paravirt_get_lazy_mode() & mode) == 0)
		xen_mc_flush();

	
	local_irq_restore(this_cpu_read(xen_mc_irq_flags));
}

void xen_mc_callback(void (*fn)(void *), void *data);

struct multicall_space xen_mc_extend_args(unsigned long op, size_t arg_size);

#endif 
