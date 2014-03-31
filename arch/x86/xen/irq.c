#include <linux/hardirq.h>

#include <asm/x86_init.h>

#include <xen/interface/xen.h>
#include <xen/interface/sched.h>
#include <xen/interface/vcpu.h>

#include <asm/xen/hypercall.h>
#include <asm/xen/hypervisor.h>

#include "xen-ops.h"

void xen_force_evtchn_callback(void)
{
	(void)HYPERVISOR_xen_version(0, NULL);
}

static unsigned long xen_save_fl(void)
{
	struct vcpu_info *vcpu;
	unsigned long flags;

	vcpu = this_cpu_read(xen_vcpu);

	
	flags = !vcpu->evtchn_upcall_mask;

	return (-flags) & X86_EFLAGS_IF;
}
PV_CALLEE_SAVE_REGS_THUNK(xen_save_fl);

static void xen_restore_fl(unsigned long flags)
{
	struct vcpu_info *vcpu;

	
	flags = !(flags & X86_EFLAGS_IF);

	preempt_disable();
	vcpu = this_cpu_read(xen_vcpu);
	vcpu->evtchn_upcall_mask = flags;
	preempt_enable_no_resched();


	if (flags == 0) {
		preempt_check_resched();
		barrier(); 
		if (unlikely(vcpu->evtchn_upcall_pending))
			xen_force_evtchn_callback();
	}
}
PV_CALLEE_SAVE_REGS_THUNK(xen_restore_fl);

static void xen_irq_disable(void)
{
	preempt_disable();
	this_cpu_read(xen_vcpu)->evtchn_upcall_mask = 1;
	preempt_enable_no_resched();
}
PV_CALLEE_SAVE_REGS_THUNK(xen_irq_disable);

static void xen_irq_enable(void)
{
	struct vcpu_info *vcpu;


	vcpu = this_cpu_read(xen_vcpu);
	vcpu->evtchn_upcall_mask = 0;


	barrier(); 
	if (unlikely(vcpu->evtchn_upcall_pending))
		xen_force_evtchn_callback();
}
PV_CALLEE_SAVE_REGS_THUNK(xen_irq_enable);

static void xen_safe_halt(void)
{
	
	if (HYPERVISOR_sched_op(SCHEDOP_block, NULL) != 0)
		BUG();
}

static void xen_halt(void)
{
	if (irqs_disabled())
		HYPERVISOR_vcpu_op(VCPUOP_down, smp_processor_id(), NULL);
	else
		xen_safe_halt();
}

static const struct pv_irq_ops xen_irq_ops __initconst = {
	.save_fl = PV_CALLEE_SAVE(xen_save_fl),
	.restore_fl = PV_CALLEE_SAVE(xen_restore_fl),
	.irq_disable = PV_CALLEE_SAVE(xen_irq_disable),
	.irq_enable = PV_CALLEE_SAVE(xen_irq_enable),

	.safe_halt = xen_safe_halt,
	.halt = xen_halt,
#ifdef CONFIG_X86_64
	.adjust_exception_frame = xen_adjust_exception_frame,
#endif
};

void __init xen_init_irq_ops(void)
{
	pv_irq_ops = xen_irq_ops;
	x86_init.irqs.intr_init = xen_init_IRQ;
}
