#ifndef _ASM_X86_CPUFEATURE_H
#define _ASM_X86_CPUFEATURE_H

#include <asm/required-features.h>

#define NCAPINTS	10	


#define X86_FEATURE_FPU		(0*32+ 0) 
#define X86_FEATURE_VME		(0*32+ 1) 
#define X86_FEATURE_DE		(0*32+ 2) 
#define X86_FEATURE_PSE		(0*32+ 3) 
#define X86_FEATURE_TSC		(0*32+ 4) 
#define X86_FEATURE_MSR		(0*32+ 5) 
#define X86_FEATURE_PAE		(0*32+ 6) 
#define X86_FEATURE_MCE		(0*32+ 7) 
#define X86_FEATURE_CX8		(0*32+ 8) 
#define X86_FEATURE_APIC	(0*32+ 9) 
#define X86_FEATURE_SEP		(0*32+11) 
#define X86_FEATURE_MTRR	(0*32+12) 
#define X86_FEATURE_PGE		(0*32+13) 
#define X86_FEATURE_MCA		(0*32+14) 
#define X86_FEATURE_CMOV	(0*32+15) 
					  
#define X86_FEATURE_PAT		(0*32+16) 
#define X86_FEATURE_PSE36	(0*32+17) 
#define X86_FEATURE_PN		(0*32+18) 
#define X86_FEATURE_CLFLSH	(0*32+19) 
#define X86_FEATURE_DS		(0*32+21) 
#define X86_FEATURE_ACPI	(0*32+22) 
#define X86_FEATURE_MMX		(0*32+23) 
#define X86_FEATURE_FXSR	(0*32+24) 
#define X86_FEATURE_XMM		(0*32+25) 
#define X86_FEATURE_XMM2	(0*32+26) 
#define X86_FEATURE_SELFSNOOP	(0*32+27) 
#define X86_FEATURE_HT		(0*32+28) 
#define X86_FEATURE_ACC		(0*32+29) 
#define X86_FEATURE_IA64	(0*32+30) 
#define X86_FEATURE_PBE		(0*32+31) 

#define X86_FEATURE_SYSCALL	(1*32+11) 
#define X86_FEATURE_MP		(1*32+19) 
#define X86_FEATURE_NX		(1*32+20) 
#define X86_FEATURE_MMXEXT	(1*32+22) 
#define X86_FEATURE_FXSR_OPT	(1*32+25) 
#define X86_FEATURE_GBPAGES	(1*32+26) 
#define X86_FEATURE_RDTSCP	(1*32+27) 
#define X86_FEATURE_LM		(1*32+29) 
#define X86_FEATURE_3DNOWEXT	(1*32+30) 
#define X86_FEATURE_3DNOW	(1*32+31) 

#define X86_FEATURE_RECOVERY	(2*32+ 0) 
#define X86_FEATURE_LONGRUN	(2*32+ 1) 
#define X86_FEATURE_LRTI	(2*32+ 3) 

#define X86_FEATURE_CXMMX	(3*32+ 0) 
#define X86_FEATURE_K6_MTRR	(3*32+ 1) 
#define X86_FEATURE_CYRIX_ARR	(3*32+ 2) 
#define X86_FEATURE_CENTAUR_MCR	(3*32+ 3) 
#define X86_FEATURE_K8		(3*32+ 4) 
#define X86_FEATURE_K7		(3*32+ 5) 
#define X86_FEATURE_P3		(3*32+ 6) 
#define X86_FEATURE_P4		(3*32+ 7) 
#define X86_FEATURE_CONSTANT_TSC (3*32+ 8) 
#define X86_FEATURE_UP		(3*32+ 9) 
#define X86_FEATURE_FXSAVE_LEAK (3*32+10) 
#define X86_FEATURE_ARCH_PERFMON (3*32+11) 
#define X86_FEATURE_PEBS	(3*32+12) 
#define X86_FEATURE_BTS		(3*32+13) 
#define X86_FEATURE_SYSCALL32	(3*32+14) 
#define X86_FEATURE_SYSENTER32	(3*32+15) 
#define X86_FEATURE_REP_GOOD	(3*32+16) 
#define X86_FEATURE_MFENCE_RDTSC (3*32+17) 
#define X86_FEATURE_LFENCE_RDTSC (3*32+18) 
#define X86_FEATURE_11AP	(3*32+19) 
#define X86_FEATURE_NOPL	(3*32+20) 
					  
#define X86_FEATURE_XTOPOLOGY	(3*32+22) 
#define X86_FEATURE_TSC_RELIABLE (3*32+23) 
#define X86_FEATURE_NONSTOP_TSC	(3*32+24) 
#define X86_FEATURE_CLFLUSH_MONITOR (3*32+25) 
#define X86_FEATURE_EXTD_APICID	(3*32+26) 
#define X86_FEATURE_AMD_DCM     (3*32+27) 
#define X86_FEATURE_APERFMPERF	(3*32+28) 

#define X86_FEATURE_XMM3	(4*32+ 0) 
#define X86_FEATURE_PCLMULQDQ	(4*32+ 1) 
#define X86_FEATURE_DTES64	(4*32+ 2) 
#define X86_FEATURE_MWAIT	(4*32+ 3) 
#define X86_FEATURE_DSCPL	(4*32+ 4) 
#define X86_FEATURE_VMX		(4*32+ 5) 
#define X86_FEATURE_SMX		(4*32+ 6) 
#define X86_FEATURE_EST		(4*32+ 7) 
#define X86_FEATURE_TM2		(4*32+ 8) 
#define X86_FEATURE_SSSE3	(4*32+ 9) 
#define X86_FEATURE_CID		(4*32+10) 
#define X86_FEATURE_FMA		(4*32+12) 
#define X86_FEATURE_CX16	(4*32+13) 
#define X86_FEATURE_XTPR	(4*32+14) 
#define X86_FEATURE_PDCM	(4*32+15) 
#define X86_FEATURE_PCID	(4*32+17) 
#define X86_FEATURE_DCA		(4*32+18) 
#define X86_FEATURE_XMM4_1	(4*32+19) 
#define X86_FEATURE_XMM4_2	(4*32+20) 
#define X86_FEATURE_X2APIC	(4*32+21) 
#define X86_FEATURE_MOVBE	(4*32+22) 
#define X86_FEATURE_POPCNT      (4*32+23) 
#define X86_FEATURE_TSC_DEADLINE_TIMER	(4*32+24) 
#define X86_FEATURE_AES		(4*32+25) 
#define X86_FEATURE_XSAVE	(4*32+26) 
#define X86_FEATURE_OSXSAVE	(4*32+27) 
#define X86_FEATURE_AVX		(4*32+28) 
#define X86_FEATURE_F16C	(4*32+29) 
#define X86_FEATURE_RDRAND	(4*32+30) 
#define X86_FEATURE_HYPERVISOR	(4*32+31) 

#define X86_FEATURE_XSTORE	(5*32+ 2) 
#define X86_FEATURE_XSTORE_EN	(5*32+ 3) 
#define X86_FEATURE_XCRYPT	(5*32+ 6) 
#define X86_FEATURE_XCRYPT_EN	(5*32+ 7) 
#define X86_FEATURE_ACE2	(5*32+ 8) 
#define X86_FEATURE_ACE2_EN	(5*32+ 9) 
#define X86_FEATURE_PHE		(5*32+10) 
#define X86_FEATURE_PHE_EN	(5*32+11) 
#define X86_FEATURE_PMM		(5*32+12) 
#define X86_FEATURE_PMM_EN	(5*32+13) 

#define X86_FEATURE_LAHF_LM	(6*32+ 0) 
#define X86_FEATURE_CMP_LEGACY	(6*32+ 1) 
#define X86_FEATURE_SVM		(6*32+ 2) 
#define X86_FEATURE_EXTAPIC	(6*32+ 3) 
#define X86_FEATURE_CR8_LEGACY	(6*32+ 4) 
#define X86_FEATURE_ABM		(6*32+ 5) 
#define X86_FEATURE_SSE4A	(6*32+ 6) 
#define X86_FEATURE_MISALIGNSSE (6*32+ 7) 
#define X86_FEATURE_3DNOWPREFETCH (6*32+ 8) 
#define X86_FEATURE_OSVW	(6*32+ 9) 
#define X86_FEATURE_IBS		(6*32+10) 
#define X86_FEATURE_XOP		(6*32+11) 
#define X86_FEATURE_SKINIT	(6*32+12) 
#define X86_FEATURE_WDT		(6*32+13) 
#define X86_FEATURE_LWP		(6*32+15) 
#define X86_FEATURE_FMA4	(6*32+16) 
#define X86_FEATURE_TCE		(6*32+17) 
#define X86_FEATURE_NODEID_MSR	(6*32+19) 
#define X86_FEATURE_TBM		(6*32+21) 
#define X86_FEATURE_TOPOEXT	(6*32+22) 
#define X86_FEATURE_PERFCTR_CORE (6*32+23) 

#define X86_FEATURE_IDA		(7*32+ 0) 
#define X86_FEATURE_ARAT	(7*32+ 1) 
#define X86_FEATURE_CPB		(7*32+ 2) 
#define X86_FEATURE_EPB		(7*32+ 3) 
#define X86_FEATURE_XSAVEOPT	(7*32+ 4) 
#define X86_FEATURE_PLN		(7*32+ 5) 
#define X86_FEATURE_PTS		(7*32+ 6) 
#define X86_FEATURE_DTS		(7*32+ 7) 
#define X86_FEATURE_HW_PSTATE	(7*32+ 8) 

#define X86_FEATURE_TPR_SHADOW  (8*32+ 0) 
#define X86_FEATURE_VNMI        (8*32+ 1) 
#define X86_FEATURE_FLEXPRIORITY (8*32+ 2) 
#define X86_FEATURE_EPT         (8*32+ 3) 
#define X86_FEATURE_VPID        (8*32+ 4) 
#define X86_FEATURE_NPT		(8*32+ 5) 
#define X86_FEATURE_LBRV	(8*32+ 6) 
#define X86_FEATURE_SVML	(8*32+ 7) 
#define X86_FEATURE_NRIPS	(8*32+ 8) 
#define X86_FEATURE_TSCRATEMSR  (8*32+ 9) 
#define X86_FEATURE_VMCBCLEAN   (8*32+10) 
#define X86_FEATURE_FLUSHBYASID (8*32+11) 
#define X86_FEATURE_DECODEASSISTS (8*32+12) 
#define X86_FEATURE_PAUSEFILTER (8*32+13) 
#define X86_FEATURE_PFTHRESHOLD (8*32+14) 


#define X86_FEATURE_FSGSBASE	(9*32+ 0) 
#define X86_FEATURE_BMI1	(9*32+ 3) 
#define X86_FEATURE_HLE		(9*32+ 4) 
#define X86_FEATURE_AVX2	(9*32+ 5) 
#define X86_FEATURE_SMEP	(9*32+ 7) 
#define X86_FEATURE_BMI2	(9*32+ 8) 
#define X86_FEATURE_ERMS	(9*32+ 9) 
#define X86_FEATURE_INVPCID	(9*32+10) 
#define X86_FEATURE_RTM		(9*32+11) 

#if defined(__KERNEL__) && !defined(__ASSEMBLY__)

#include <asm/asm.h>
#include <linux/bitops.h>

extern const char * const x86_cap_flags[NCAPINTS*32];
extern const char * const x86_power_flags[32];

#define test_cpu_cap(c, bit)						\
	 test_bit(bit, (unsigned long *)((c)->x86_capability))

#define REQUIRED_MASK_BIT_SET(bit)					\
	 ( (((bit)>>5)==0 && (1UL<<((bit)&31) & REQUIRED_MASK0)) ||	\
	   (((bit)>>5)==1 && (1UL<<((bit)&31) & REQUIRED_MASK1)) ||	\
	   (((bit)>>5)==2 && (1UL<<((bit)&31) & REQUIRED_MASK2)) ||	\
	   (((bit)>>5)==3 && (1UL<<((bit)&31) & REQUIRED_MASK3)) ||	\
	   (((bit)>>5)==4 && (1UL<<((bit)&31) & REQUIRED_MASK4)) ||	\
	   (((bit)>>5)==5 && (1UL<<((bit)&31) & REQUIRED_MASK5)) ||	\
	   (((bit)>>5)==6 && (1UL<<((bit)&31) & REQUIRED_MASK6)) ||	\
	   (((bit)>>5)==7 && (1UL<<((bit)&31) & REQUIRED_MASK7)) ||	\
	   (((bit)>>5)==8 && (1UL<<((bit)&31) & REQUIRED_MASK8)) ||	\
	   (((bit)>>5)==9 && (1UL<<((bit)&31) & REQUIRED_MASK9)) )

#define cpu_has(c, bit)							\
	(__builtin_constant_p(bit) && REQUIRED_MASK_BIT_SET(bit) ? 1 :	\
	 test_cpu_cap(c, bit))

#define this_cpu_has(bit)						\
	(__builtin_constant_p(bit) && REQUIRED_MASK_BIT_SET(bit) ? 1 : 	\
	 x86_this_cpu_test_bit(bit, (unsigned long *)&cpu_info.x86_capability))

#define boot_cpu_has(bit)	cpu_has(&boot_cpu_data, bit)

#define set_cpu_cap(c, bit)	set_bit(bit, (unsigned long *)((c)->x86_capability))
#define clear_cpu_cap(c, bit)	clear_bit(bit, (unsigned long *)((c)->x86_capability))
#define setup_clear_cpu_cap(bit) do { \
	clear_cpu_cap(&boot_cpu_data, bit);	\
	set_bit(bit, (unsigned long *)cpu_caps_cleared); \
} while (0)
#define setup_force_cpu_cap(bit) do { \
	set_cpu_cap(&boot_cpu_data, bit);	\
	set_bit(bit, (unsigned long *)cpu_caps_set);	\
} while (0)

#define cpu_has_fpu		boot_cpu_has(X86_FEATURE_FPU)
#define cpu_has_vme		boot_cpu_has(X86_FEATURE_VME)
#define cpu_has_de		boot_cpu_has(X86_FEATURE_DE)
#define cpu_has_pse		boot_cpu_has(X86_FEATURE_PSE)
#define cpu_has_tsc		boot_cpu_has(X86_FEATURE_TSC)
#define cpu_has_pae		boot_cpu_has(X86_FEATURE_PAE)
#define cpu_has_pge		boot_cpu_has(X86_FEATURE_PGE)
#define cpu_has_apic		boot_cpu_has(X86_FEATURE_APIC)
#define cpu_has_sep		boot_cpu_has(X86_FEATURE_SEP)
#define cpu_has_mtrr		boot_cpu_has(X86_FEATURE_MTRR)
#define cpu_has_mmx		boot_cpu_has(X86_FEATURE_MMX)
#define cpu_has_fxsr		boot_cpu_has(X86_FEATURE_FXSR)
#define cpu_has_xmm		boot_cpu_has(X86_FEATURE_XMM)
#define cpu_has_xmm2		boot_cpu_has(X86_FEATURE_XMM2)
#define cpu_has_xmm3		boot_cpu_has(X86_FEATURE_XMM3)
#define cpu_has_ssse3		boot_cpu_has(X86_FEATURE_SSSE3)
#define cpu_has_aes		boot_cpu_has(X86_FEATURE_AES)
#define cpu_has_avx		boot_cpu_has(X86_FEATURE_AVX)
#define cpu_has_ht		boot_cpu_has(X86_FEATURE_HT)
#define cpu_has_mp		boot_cpu_has(X86_FEATURE_MP)
#define cpu_has_nx		boot_cpu_has(X86_FEATURE_NX)
#define cpu_has_k6_mtrr		boot_cpu_has(X86_FEATURE_K6_MTRR)
#define cpu_has_cyrix_arr	boot_cpu_has(X86_FEATURE_CYRIX_ARR)
#define cpu_has_centaur_mcr	boot_cpu_has(X86_FEATURE_CENTAUR_MCR)
#define cpu_has_xstore		boot_cpu_has(X86_FEATURE_XSTORE)
#define cpu_has_xstore_enabled	boot_cpu_has(X86_FEATURE_XSTORE_EN)
#define cpu_has_xcrypt		boot_cpu_has(X86_FEATURE_XCRYPT)
#define cpu_has_xcrypt_enabled	boot_cpu_has(X86_FEATURE_XCRYPT_EN)
#define cpu_has_ace2		boot_cpu_has(X86_FEATURE_ACE2)
#define cpu_has_ace2_enabled	boot_cpu_has(X86_FEATURE_ACE2_EN)
#define cpu_has_phe		boot_cpu_has(X86_FEATURE_PHE)
#define cpu_has_phe_enabled	boot_cpu_has(X86_FEATURE_PHE_EN)
#define cpu_has_pmm		boot_cpu_has(X86_FEATURE_PMM)
#define cpu_has_pmm_enabled	boot_cpu_has(X86_FEATURE_PMM_EN)
#define cpu_has_ds		boot_cpu_has(X86_FEATURE_DS)
#define cpu_has_pebs		boot_cpu_has(X86_FEATURE_PEBS)
#define cpu_has_clflush		boot_cpu_has(X86_FEATURE_CLFLSH)
#define cpu_has_bts		boot_cpu_has(X86_FEATURE_BTS)
#define cpu_has_gbpages		boot_cpu_has(X86_FEATURE_GBPAGES)
#define cpu_has_arch_perfmon	boot_cpu_has(X86_FEATURE_ARCH_PERFMON)
#define cpu_has_pat		boot_cpu_has(X86_FEATURE_PAT)
#define cpu_has_xmm4_1		boot_cpu_has(X86_FEATURE_XMM4_1)
#define cpu_has_xmm4_2		boot_cpu_has(X86_FEATURE_XMM4_2)
#define cpu_has_x2apic		boot_cpu_has(X86_FEATURE_X2APIC)
#define cpu_has_xsave		boot_cpu_has(X86_FEATURE_XSAVE)
#define cpu_has_osxsave		boot_cpu_has(X86_FEATURE_OSXSAVE)
#define cpu_has_hypervisor	boot_cpu_has(X86_FEATURE_HYPERVISOR)
#define cpu_has_pclmulqdq	boot_cpu_has(X86_FEATURE_PCLMULQDQ)
#define cpu_has_perfctr_core	boot_cpu_has(X86_FEATURE_PERFCTR_CORE)
#define cpu_has_cx8		boot_cpu_has(X86_FEATURE_CX8)
#define cpu_has_cx16		boot_cpu_has(X86_FEATURE_CX16)

#if defined(CONFIG_X86_INVLPG) || defined(CONFIG_X86_64)
# define cpu_has_invlpg		1
#else
# define cpu_has_invlpg		(boot_cpu_data.x86 > 3)
#endif

#ifdef CONFIG_X86_64

#undef  cpu_has_vme
#define cpu_has_vme		0

#undef  cpu_has_pae
#define cpu_has_pae		___BUG___

#undef  cpu_has_mp
#define cpu_has_mp		1

#undef  cpu_has_k6_mtrr
#define cpu_has_k6_mtrr		0

#undef  cpu_has_cyrix_arr
#define cpu_has_cyrix_arr	0

#undef  cpu_has_centaur_mcr
#define cpu_has_centaur_mcr	0

#endif 

#if __GNUC__ >= 4
static __always_inline __pure bool __static_cpu_has(u16 bit)
{
#if __GNUC__ > 4 || __GNUC_MINOR__ >= 5
		asm goto("1: jmp %l[t_no]\n"
			 "2:\n"
			 ".section .altinstructions,\"a\"\n"
			 " .long 1b - .\n"
			 " .long 0\n"		
			 " .word %P0\n"		
			 " .byte 2b - 1b\n"	
			 " .byte 0\n"		
			 ".previous\n"
			 
			 : : "i" (bit) : : t_no);
		return true;
	t_no:
		return false;
#else
		u8 flag;
		
		asm volatile("1: movb $0,%0\n"
			     "2:\n"
			     ".section .altinstructions,\"a\"\n"
			     " .long 1b - .\n"
			     " .long 3f - .\n"
			     " .word %P1\n"		
			     " .byte 2b - 1b\n"		
			     " .byte 4f - 3f\n"		
			     ".previous\n"
			     ".section .discard,\"aw\",@progbits\n"
			     " .byte 0xff + (4f-3f) - (2b-1b)\n" 
			     ".previous\n"
			     ".section .altinstr_replacement,\"ax\"\n"
			     "3: movb $1,%0\n"
			     "4:\n"
			     ".previous\n"
			     : "=qm" (flag) : "i" (bit));
		return flag;
#endif
}

#define static_cpu_has(bit)					\
(								\
	__builtin_constant_p(boot_cpu_has(bit)) ?		\
		boot_cpu_has(bit) :				\
	__builtin_constant_p(bit) ?				\
		__static_cpu_has(bit) :				\
		boot_cpu_has(bit)				\
)
#else
#define static_cpu_has(bit) boot_cpu_has(bit)
#endif

#endif 

#endif 
