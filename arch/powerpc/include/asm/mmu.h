#ifndef _ASM_POWERPC_MMU_H_
#define _ASM_POWERPC_MMU_H_
#ifdef __KERNEL__

#include <linux/types.h>

#include <asm/asm-compat.h>
#include <asm/feature-fixups.h>


#define MMU_FTR_HPTE_TABLE		ASM_CONST(0x00000001)
#define MMU_FTR_TYPE_8xx		ASM_CONST(0x00000002)
#define MMU_FTR_TYPE_40x		ASM_CONST(0x00000004)
#define MMU_FTR_TYPE_44x		ASM_CONST(0x00000008)
#define MMU_FTR_TYPE_FSL_E		ASM_CONST(0x00000010)
#define MMU_FTR_TYPE_3E			ASM_CONST(0x00000020)
#define MMU_FTR_TYPE_47x		ASM_CONST(0x00000040)


#define MMU_FTR_USE_HIGH_BATS		ASM_CONST(0x00010000)

#define MMU_FTR_BIG_PHYS		ASM_CONST(0x00020000)

#define MMU_FTR_USE_TLBIVAX_BCAST	ASM_CONST(0x00040000)

#define MMU_FTR_USE_TLBILX		ASM_CONST(0x00080000)

#define MMU_FTR_LOCK_BCAST_INVAL	ASM_CONST(0x00100000)

#define MMU_FTR_NEED_DTLB_SW_LRU	ASM_CONST(0x00200000)

#define MMU_FTR_USE_TLBRSRV		ASM_CONST(0x00800000)

#define MMU_FTR_USE_PAIRED_MAS		ASM_CONST(0x01000000)

#define MMU_FTR_SLB			ASM_CONST(0x02000000)

#define MMU_FTR_16M_PAGE		ASM_CONST(0x04000000)

#define MMU_FTR_TLBIEL			ASM_CONST(0x08000000)

#define MMU_FTR_LOCKLESS_TLBIE		ASM_CONST(0x10000000)

#define MMU_FTR_CI_LARGE_PAGE		ASM_CONST(0x20000000)

#define MMU_FTR_1T_SEGMENT		ASM_CONST(0x40000000)

#define MMU_FTR_NO_SLBIE_B		ASM_CONST(0x80000000)

#define MMU_FTRS_DEFAULT_HPTE_ARCH_V2	\
	MMU_FTR_HPTE_TABLE | MMU_FTR_PPCAS_ARCH_V2
#define MMU_FTRS_POWER4		MMU_FTRS_DEFAULT_HPTE_ARCH_V2
#define MMU_FTRS_PPC970		MMU_FTRS_POWER4
#define MMU_FTRS_POWER5		MMU_FTRS_POWER4 | MMU_FTR_LOCKLESS_TLBIE
#define MMU_FTRS_POWER6		MMU_FTRS_POWER4 | MMU_FTR_LOCKLESS_TLBIE
#define MMU_FTRS_POWER7		MMU_FTRS_POWER4 | MMU_FTR_LOCKLESS_TLBIE
#define MMU_FTRS_CELL		MMU_FTRS_DEFAULT_HPTE_ARCH_V2 | \
				MMU_FTR_CI_LARGE_PAGE
#define MMU_FTRS_PA6T		MMU_FTRS_DEFAULT_HPTE_ARCH_V2 | \
				MMU_FTR_CI_LARGE_PAGE | MMU_FTR_NO_SLBIE_B
#define MMU_FTRS_A2		MMU_FTR_TYPE_3E | MMU_FTR_USE_TLBILX | \
				MMU_FTR_USE_TLBIVAX_BCAST | \
				MMU_FTR_LOCK_BCAST_INVAL | \
				MMU_FTR_USE_TLBRSRV | \
				MMU_FTR_USE_PAIRED_MAS | \
				MMU_FTR_TLBIEL | \
				MMU_FTR_16M_PAGE
#ifndef __ASSEMBLY__
#include <asm/cputable.h>

#ifdef CONFIG_PPC_FSL_BOOK3E
#include <asm/percpu.h>
DECLARE_PER_CPU(int, next_tlbcam_idx);
#endif

static inline int mmu_has_feature(unsigned long feature)
{
	return (cur_cpu_spec->mmu_features & feature);
}

static inline void mmu_clear_feature(unsigned long feature)
{
	cur_cpu_spec->mmu_features &= ~feature;
}

extern unsigned int __start___mmu_ftr_fixup, __stop___mmu_ftr_fixup;

extern void early_init_mmu(void);
extern void early_init_mmu_secondary(void);

extern void setup_initial_memory_limit(phys_addr_t first_memblock_base,
				       phys_addr_t first_memblock_size);

#ifdef CONFIG_PPC64
extern u64 ppc64_rma_size;
#endif 

#endif 


#define MMU_PAGE_4K	0
#define MMU_PAGE_16K	1
#define MMU_PAGE_64K	2
#define MMU_PAGE_64K_AP	3	
#define MMU_PAGE_256K	4
#define MMU_PAGE_1M	5
#define MMU_PAGE_4M	6
#define MMU_PAGE_8M	7
#define MMU_PAGE_16M	8
#define MMU_PAGE_64M	9
#define MMU_PAGE_256M	10
#define MMU_PAGE_1G	11
#define MMU_PAGE_16G	12
#define MMU_PAGE_64G	13

#define MMU_PAGE_COUNT	14

#if defined(CONFIG_PPC_STD_MMU_64)
#  include <asm/mmu-hash64.h>
#elif defined(CONFIG_PPC_STD_MMU_32)
#  include <asm/mmu-hash32.h>
#elif defined(CONFIG_40x)
#  include <asm/mmu-40x.h>
#elif defined(CONFIG_44x)
#  include <asm/mmu-44x.h>
#elif defined(CONFIG_PPC_BOOK3E_MMU)
#  include <asm/mmu-book3e.h>
#elif defined (CONFIG_PPC_8xx)
#  include <asm/mmu-8xx.h>
#endif


#endif 
#endif 
