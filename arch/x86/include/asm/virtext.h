/* CPU virtualization extensions handling
 *
 * This should carry the code for handling CPU virtualization extensions
 * that needs to live in the kernel core.
 *
 * Author: Eduardo Habkost <ehabkost@redhat.com>
 *
 * Copyright (C) 2008, Red Hat Inc.
 *
 * Contains code from KVM, Copyright (C) 2006 Qumranet, Inc.
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */
#ifndef _ASM_X86_VIRTEX_H
#define _ASM_X86_VIRTEX_H

#include <asm/processor.h>

#include <asm/vmx.h>
#include <asm/svm.h>


static inline int cpu_has_vmx(void)
{
	unsigned long ecx = cpuid_ecx(1);
	return test_bit(5, &ecx); 
}


static inline void cpu_vmxoff(void)
{
	asm volatile (ASM_VMX_VMXOFF : : : "cc");
	write_cr4(read_cr4() & ~X86_CR4_VMXE);
}

static inline int cpu_vmx_enabled(void)
{
	return read_cr4() & X86_CR4_VMXE;
}

static inline void __cpu_emergency_vmxoff(void)
{
	if (cpu_vmx_enabled())
		cpu_vmxoff();
}

static inline void cpu_emergency_vmxoff(void)
{
	if (cpu_has_vmx())
		__cpu_emergency_vmxoff();
}





static inline int cpu_has_svm(const char **msg)
{
	uint32_t eax, ebx, ecx, edx;

	if (boot_cpu_data.x86_vendor != X86_VENDOR_AMD) {
		if (msg)
			*msg = "not amd";
		return 0;
	}

	cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
	if (eax < SVM_CPUID_FUNC) {
		if (msg)
			*msg = "can't execute cpuid_8000000a";
		return 0;
	}

	cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
	if (!(ecx & (1 << SVM_CPUID_FEATURE_SHIFT))) {
		if (msg)
			*msg = "svm not available";
		return 0;
	}
	return 1;
}


static inline void cpu_svm_disable(void)
{
	uint64_t efer;

	wrmsrl(MSR_VM_HSAVE_PA, 0);
	rdmsrl(MSR_EFER, efer);
	wrmsrl(MSR_EFER, efer & ~EFER_SVME);
}

static inline void cpu_emergency_svm_disable(void)
{
	if (cpu_has_svm(NULL))
		cpu_svm_disable();
}

#endif 
