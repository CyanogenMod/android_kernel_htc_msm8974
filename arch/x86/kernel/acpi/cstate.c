/*
 * Copyright (C) 2005 Intel Corporation
 * 	Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>
 * 	- Added _PDC for SMP C-states on Intel CPUs
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/acpi.h>
#include <linux/cpu.h>
#include <linux/sched.h>

#include <acpi/processor.h>
#include <asm/acpi.h>
#include <asm/mwait.h>
#include <asm/special_insns.h>

void acpi_processor_power_init_bm_check(struct acpi_processor_flags *flags,
					unsigned int cpu)
{
	struct cpuinfo_x86 *c = &cpu_data(cpu);

	flags->bm_check = 0;
	if (num_online_cpus() == 1)
		flags->bm_check = 1;
	else if (c->x86_vendor == X86_VENDOR_INTEL) {
		flags->bm_check = 1;
	}

	if (c->x86_vendor == X86_VENDOR_INTEL &&
	    (c->x86 > 0xf || (c->x86 == 6 && c->x86_model >= 0x0f)))
			flags->bm_control = 0;
}
EXPORT_SYMBOL(acpi_processor_power_init_bm_check);


struct cstate_entry {
	struct {
		unsigned int eax;
		unsigned int ecx;
	} states[ACPI_PROCESSOR_MAX_POWER];
};
static struct cstate_entry __percpu *cpu_cstate_entry;	

static short mwait_supported[ACPI_PROCESSOR_MAX_POWER];

#define NATIVE_CSTATE_BEYOND_HALT	(2)

static long acpi_processor_ffh_cstate_probe_cpu(void *_cx)
{
	struct acpi_processor_cx *cx = _cx;
	long retval;
	unsigned int eax, ebx, ecx, edx;
	unsigned int edx_part;
	unsigned int cstate_type; 
	unsigned int num_cstate_subtype;

	cpuid(CPUID_MWAIT_LEAF, &eax, &ebx, &ecx, &edx);

	
	cstate_type = ((cx->address >> MWAIT_SUBSTATE_SIZE) &
			MWAIT_CSTATE_MASK) + 1;
	edx_part = edx >> (cstate_type * MWAIT_SUBSTATE_SIZE);
	num_cstate_subtype = edx_part & MWAIT_SUBSTATE_MASK;

	retval = 0;
	if (num_cstate_subtype < (cx->address & MWAIT_SUBSTATE_MASK)) {
		retval = -1;
		goto out;
	}

	
	if (!(ecx & CPUID5_ECX_EXTENSIONS_SUPPORTED) ||
	    !(ecx & CPUID5_ECX_INTERRUPT_BREAK)) {
		retval = -1;
		goto out;
	}

	if (!mwait_supported[cstate_type]) {
		mwait_supported[cstate_type] = 1;
		printk(KERN_DEBUG
			"Monitor-Mwait will be used to enter C-%d "
			"state\n", cx->type);
	}
	snprintf(cx->desc,
			ACPI_CX_DESC_LEN, "ACPI FFH INTEL MWAIT 0x%x",
			cx->address);
out:
	return retval;
}

int acpi_processor_ffh_cstate_probe(unsigned int cpu,
		struct acpi_processor_cx *cx, struct acpi_power_register *reg)
{
	struct cstate_entry *percpu_entry;
	struct cpuinfo_x86 *c = &cpu_data(cpu);
	long retval;

	if (!cpu_cstate_entry || c->cpuid_level < CPUID_MWAIT_LEAF)
		return -1;

	if (reg->bit_offset != NATIVE_CSTATE_BEYOND_HALT)
		return -1;

	percpu_entry = per_cpu_ptr(cpu_cstate_entry, cpu);
	percpu_entry->states[cx->index].eax = 0;
	percpu_entry->states[cx->index].ecx = 0;

	

	retval = work_on_cpu(cpu, acpi_processor_ffh_cstate_probe_cpu, cx);
	if (retval == 0) {
		
		percpu_entry->states[cx->index].eax = cx->address;
		percpu_entry->states[cx->index].ecx = MWAIT_ECX_INTERRUPT_BREAK;
	}

	if ((c->x86_vendor == X86_VENDOR_INTEL) && !(reg->access_size & 0x2))
		cx->bm_sts_skip = 1;

	return retval;
}
EXPORT_SYMBOL_GPL(acpi_processor_ffh_cstate_probe);

void mwait_idle_with_hints(unsigned long ax, unsigned long cx)
{
	if (!need_resched()) {
		if (this_cpu_has(X86_FEATURE_CLFLUSH_MONITOR))
			clflush((void *)&current_thread_info()->flags);

		__monitor((void *)&current_thread_info()->flags, 0, 0);
		smp_mb();
		if (!need_resched())
			__mwait(ax, cx);
	}
}

void acpi_processor_ffh_cstate_enter(struct acpi_processor_cx *cx)
{
	unsigned int cpu = smp_processor_id();
	struct cstate_entry *percpu_entry;

	percpu_entry = per_cpu_ptr(cpu_cstate_entry, cpu);
	mwait_idle_with_hints(percpu_entry->states[cx->index].eax,
	                      percpu_entry->states[cx->index].ecx);
}
EXPORT_SYMBOL_GPL(acpi_processor_ffh_cstate_enter);

static int __init ffh_cstate_init(void)
{
	struct cpuinfo_x86 *c = &boot_cpu_data;
	if (c->x86_vendor != X86_VENDOR_INTEL)
		return -1;

	cpu_cstate_entry = alloc_percpu(struct cstate_entry);
	return 0;
}

static void __exit ffh_cstate_exit(void)
{
	free_percpu(cpu_cstate_entry);
	cpu_cstate_entry = NULL;
}

arch_initcall(ffh_cstate_init);
__exitcall(ffh_cstate_exit);
