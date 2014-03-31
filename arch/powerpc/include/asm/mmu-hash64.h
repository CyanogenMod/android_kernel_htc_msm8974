#ifndef _ASM_POWERPC_MMU_HASH64_H_
#define _ASM_POWERPC_MMU_HASH64_H_
/*
 * PowerPC64 memory management structures
 *
 * Dave Engebretsen & Mike Corrigan <{engebret|mikejc}@us.ibm.com>
 *   PPC64 rework.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <asm/asm-compat.h>
#include <asm/page.h>


#define STE_ESID_V	0x80
#define STE_ESID_KS	0x20
#define STE_ESID_KP	0x10
#define STE_ESID_N	0x08

#define STE_VSID_SHIFT	12

#define STAB0_PAGE	0x8
#define STAB0_OFFSET	(STAB0_PAGE << 12)
#define STAB0_PHYS_ADDR	(STAB0_OFFSET + PHYSICAL_START)

#ifndef __ASSEMBLY__
extern char initial_stab[];
#endif 


#define SLB_NUM_BOLTED		3
#define SLB_CACHE_ENTRIES	8
#define SLB_MIN_SIZE		32

#define SLB_ESID_V		ASM_CONST(0x0000000008000000) 

#define SLB_VSID_SHIFT		12
#define SLB_VSID_SHIFT_1T	24
#define SLB_VSID_SSIZE_SHIFT	62
#define SLB_VSID_B		ASM_CONST(0xc000000000000000)
#define SLB_VSID_B_256M		ASM_CONST(0x0000000000000000)
#define SLB_VSID_B_1T		ASM_CONST(0x4000000000000000)
#define SLB_VSID_KS		ASM_CONST(0x0000000000000800)
#define SLB_VSID_KP		ASM_CONST(0x0000000000000400)
#define SLB_VSID_N		ASM_CONST(0x0000000000000200) 
#define SLB_VSID_L		ASM_CONST(0x0000000000000100)
#define SLB_VSID_C		ASM_CONST(0x0000000000000080) 
#define SLB_VSID_LP		ASM_CONST(0x0000000000000030)
#define SLB_VSID_LP_00		ASM_CONST(0x0000000000000000)
#define SLB_VSID_LP_01		ASM_CONST(0x0000000000000010)
#define SLB_VSID_LP_10		ASM_CONST(0x0000000000000020)
#define SLB_VSID_LP_11		ASM_CONST(0x0000000000000030)
#define SLB_VSID_LLP		(SLB_VSID_L|SLB_VSID_LP)

#define SLB_VSID_KERNEL		(SLB_VSID_KP)
#define SLB_VSID_USER		(SLB_VSID_KP|SLB_VSID_KS|SLB_VSID_C)

#define SLBIE_C			(0x08000000)
#define SLBIE_SSIZE_SHIFT	25


#define HPTES_PER_GROUP 8

#define HPTE_V_SSIZE_SHIFT	62
#define HPTE_V_AVPN_SHIFT	7
#define HPTE_V_AVPN		ASM_CONST(0x3fffffffffffff80)
#define HPTE_V_AVPN_VAL(x)	(((x) & HPTE_V_AVPN) >> HPTE_V_AVPN_SHIFT)
#define HPTE_V_COMPARE(x,y)	(!(((x) ^ (y)) & 0xffffffffffffff80UL))
#define HPTE_V_BOLTED		ASM_CONST(0x0000000000000010)
#define HPTE_V_LOCK		ASM_CONST(0x0000000000000008)
#define HPTE_V_LARGE		ASM_CONST(0x0000000000000004)
#define HPTE_V_SECONDARY	ASM_CONST(0x0000000000000002)
#define HPTE_V_VALID		ASM_CONST(0x0000000000000001)

#define HPTE_R_PP0		ASM_CONST(0x8000000000000000)
#define HPTE_R_TS		ASM_CONST(0x4000000000000000)
#define HPTE_R_KEY_HI		ASM_CONST(0x3000000000000000)
#define HPTE_R_RPN_SHIFT	12
#define HPTE_R_RPN		ASM_CONST(0x0ffffffffffff000)
#define HPTE_R_PP		ASM_CONST(0x0000000000000003)
#define HPTE_R_N		ASM_CONST(0x0000000000000004)
#define HPTE_R_G		ASM_CONST(0x0000000000000008)
#define HPTE_R_M		ASM_CONST(0x0000000000000010)
#define HPTE_R_I		ASM_CONST(0x0000000000000020)
#define HPTE_R_W		ASM_CONST(0x0000000000000040)
#define HPTE_R_WIMG		ASM_CONST(0x0000000000000078)
#define HPTE_R_C		ASM_CONST(0x0000000000000080)
#define HPTE_R_R		ASM_CONST(0x0000000000000100)
#define HPTE_R_KEY_LO		ASM_CONST(0x0000000000000e00)

#define HPTE_V_1TB_SEG		ASM_CONST(0x4000000000000000)
#define HPTE_V_VRMA_MASK	ASM_CONST(0x4001ffffff000000)

#define PP_RWXX	0	
#define PP_RWRX 1	
#define PP_RWRW 2	
#define PP_RXRX 3	
#define PP_RXXX	(HPTE_R_PP0 | 2)	

#ifndef __ASSEMBLY__

struct hash_pte {
	unsigned long v;
	unsigned long r;
};

extern struct hash_pte *htab_address;
extern unsigned long htab_size_bytes;
extern unsigned long htab_hash_mask;

struct mmu_psize_def
{
	unsigned int	shift;	
	unsigned int	penc;	
	unsigned int	tlbiel;	
	unsigned long	avpnm;	
	unsigned long	sllp;	
};

#endif 

#define MMU_SEGSIZE_256M	0
#define MMU_SEGSIZE_1T		1


#ifndef __ASSEMBLY__

extern struct mmu_psize_def mmu_psize_defs[MMU_PAGE_COUNT];
extern int mmu_linear_psize;
extern int mmu_virtual_psize;
extern int mmu_vmalloc_psize;
extern int mmu_vmemmap_psize;
extern int mmu_io_psize;
extern int mmu_kernel_ssize;
extern int mmu_highuser_ssize;
extern u16 mmu_slb_size;
extern unsigned long tce_alloc_start, tce_alloc_end;

extern int mmu_ci_restrictions;

static inline unsigned long hpte_encode_v(unsigned long va, int psize,
					  int ssize)
{
	unsigned long v;
	v = (va >> 23) & ~(mmu_psize_defs[psize].avpnm);
	v <<= HPTE_V_AVPN_SHIFT;
	if (psize != MMU_PAGE_4K)
		v |= HPTE_V_LARGE;
	v |= ((unsigned long) ssize) << HPTE_V_SSIZE_SHIFT;
	return v;
}

static inline unsigned long hpte_encode_r(unsigned long pa, int psize)
{
	unsigned long r;

	
	if (psize == MMU_PAGE_4K)
		return pa & HPTE_R_RPN;
	else {
		unsigned int penc = mmu_psize_defs[psize].penc;
		unsigned int shift = mmu_psize_defs[psize].shift;
		return (pa & ~((1ul << shift) - 1)) | (penc << 12);
	}
	return r;
}

static inline unsigned long hpt_va(unsigned long ea, unsigned long vsid,
				   int ssize)
{
	if (ssize == MMU_SEGSIZE_256M)
		return (vsid << 28) | (ea & 0xfffffffUL);
	return (vsid << 40) | (ea & 0xffffffffffUL);
}


static inline unsigned long hpt_hash(unsigned long va, unsigned int shift,
				     int ssize)
{
	unsigned long hash, vsid;

	if (ssize == MMU_SEGSIZE_256M) {
		hash = (va >> 28) ^ ((va & 0x0fffffffUL) >> shift);
	} else {
		vsid = va >> 40;
		hash = vsid ^ (vsid << 25) ^ ((va & 0xffffffffffUL) >> shift);
	}
	return hash & 0x7fffffffffUL;
}

extern int __hash_page_4K(unsigned long ea, unsigned long access,
			  unsigned long vsid, pte_t *ptep, unsigned long trap,
			  unsigned int local, int ssize, int subpage_prot);
extern int __hash_page_64K(unsigned long ea, unsigned long access,
			   unsigned long vsid, pte_t *ptep, unsigned long trap,
			   unsigned int local, int ssize);
struct mm_struct;
unsigned int hash_page_do_lazy_icache(unsigned int pp, pte_t pte, int trap);
extern int hash_page(unsigned long ea, unsigned long access, unsigned long trap);
int __hash_page_huge(unsigned long ea, unsigned long access, unsigned long vsid,
		     pte_t *ptep, unsigned long trap, int local, int ssize,
		     unsigned int shift, unsigned int mmu_psize);
extern void hash_failure_debug(unsigned long ea, unsigned long access,
			       unsigned long vsid, unsigned long trap,
			       int ssize, int psize, unsigned long pte);
extern int htab_bolt_mapping(unsigned long vstart, unsigned long vend,
			     unsigned long pstart, unsigned long prot,
			     int psize, int ssize);
extern void add_gpage(u64 addr, u64 page_size, unsigned long number_of_pages);
extern void demote_segment_4k(struct mm_struct *mm, unsigned long addr);

extern void hpte_init_native(void);
extern void hpte_init_lpar(void);
extern void hpte_init_beat(void);
extern void hpte_init_beat_v3(void);

extern void stabs_alloc(void);
extern void slb_initialize(void);
extern void slb_flush_and_rebolt(void);
extern void stab_initialize(unsigned long stab);

extern void slb_vmalloc_update(void);
extern void slb_set_size(u16 size);
#endif 


#define VSID_MULTIPLIER_256M	ASM_CONST(200730139)	
#define VSID_BITS_256M		36
#define VSID_MODULUS_256M	((1UL<<VSID_BITS_256M)-1)

#define VSID_MULTIPLIER_1T	ASM_CONST(12538073)	
#define VSID_BITS_1T		24
#define VSID_MODULUS_1T		((1UL<<VSID_BITS_1T)-1)

#define CONTEXT_BITS		19
#define USER_ESID_BITS		16
#define USER_ESID_BITS_1T	4

#define USER_VSID_RANGE	(1UL << (USER_ESID_BITS + SID_SHIFT))

#define ASM_VSID_SCRAMBLE(rt, rx, size)					\
	lis	rx,VSID_MULTIPLIER_##size@h;				\
	ori	rx,rx,VSID_MULTIPLIER_##size@l;				\
	mulld	rt,rt,rx;			\
									\
	srdi	rx,rt,VSID_BITS_##size;					\
	clrldi	rt,rt,(64-VSID_BITS_##size);				\
	add	rt,rt,rx;			\
\
	addi	rx,rt,1;						\
	srdi	rx,rx,VSID_BITS_##size;		\
	add	rt,rt,rx


#ifndef __ASSEMBLY__

#ifdef CONFIG_PPC_SUBPAGE_PROT
struct subpage_prot_table {
	unsigned long maxaddr;	
	unsigned int **protptrs[2];
	unsigned int *low_prot[4];
};

#define SBP_L1_BITS		(PAGE_SHIFT - 2)
#define SBP_L2_BITS		(PAGE_SHIFT - 3)
#define SBP_L1_COUNT		(1 << SBP_L1_BITS)
#define SBP_L2_COUNT		(1 << SBP_L2_BITS)
#define SBP_L2_SHIFT		(PAGE_SHIFT + SBP_L1_BITS)
#define SBP_L3_SHIFT		(SBP_L2_SHIFT + SBP_L2_BITS)

extern void subpage_prot_free(struct mm_struct *mm);
extern void subpage_prot_init_new_context(struct mm_struct *mm);
#else
static inline void subpage_prot_free(struct mm_struct *mm) {}
static inline void subpage_prot_init_new_context(struct mm_struct *mm) { }
#endif 

typedef unsigned long mm_context_id_t;
struct spinlock;

typedef struct {
	mm_context_id_t id;
	u16 user_psize;		

#ifdef CONFIG_PPC_MM_SLICES
	u64 low_slices_psize;	
	u64 high_slices_psize;  
#else
	u16 sllp;		
#endif
	unsigned long vdso_base;
#ifdef CONFIG_PPC_SUBPAGE_PROT
	struct subpage_prot_table spt;
#endif 
#ifdef CONFIG_PPC_ICSWX
	struct spinlock *cop_lockp; 
	unsigned long acop;	
	unsigned int cop_pid;	
#endif 
} mm_context_t;


#if 0
#define vsid_scramble(protovsid, size) \
	((((protovsid) * VSID_MULTIPLIER_##size) % VSID_MODULUS_##size))

#else 
#define vsid_scramble(protovsid, size) \
	({								 \
		unsigned long x;					 \
		x = (protovsid) * VSID_MULTIPLIER_##size;		 \
		x = (x >> VSID_BITS_##size) + (x & VSID_MODULUS_##size); \
		(x + ((x+1) >> VSID_BITS_##size)) & VSID_MODULUS_##size; \
	})
#endif 

static inline unsigned long get_kernel_vsid(unsigned long ea, int ssize)
{
	if (ssize == MMU_SEGSIZE_256M)
		return vsid_scramble(ea >> SID_SHIFT, 256M);
	return vsid_scramble(ea >> SID_SHIFT_1T, 1T);
}

static inline int user_segment_size(unsigned long addr)
{
	
	if (addr >= (1UL << SID_SHIFT_1T))
		return mmu_highuser_ssize;
	return MMU_SEGSIZE_256M;
}

static inline unsigned long get_vsid(unsigned long context, unsigned long ea,
				     int ssize)
{
	if (ssize == MMU_SEGSIZE_256M)
		return vsid_scramble((context << USER_ESID_BITS)
				     | (ea >> SID_SHIFT), 256M);
	return vsid_scramble((context << USER_ESID_BITS_1T)
			     | (ea >> SID_SHIFT_1T), 1T);
}

#endif 

#endif 
