#ifndef _SPARC64_TSB_H
#define _SPARC64_TSB_H


#define TSB_TAG_LOCK_BIT	47
#define TSB_TAG_LOCK_HIGH	(1 << (TSB_TAG_LOCK_BIT - 32))

#define TSB_TAG_INVALID_BIT	46
#define TSB_TAG_INVALID_HIGH	(1 << (TSB_TAG_INVALID_BIT - 32))

#ifndef __ASSEMBLY__
struct tsb_ldquad_phys_patch_entry {
	unsigned int	addr;
	unsigned int	sun4u_insn;
	unsigned int	sun4v_insn;
};
extern struct tsb_ldquad_phys_patch_entry __tsb_ldquad_phys_patch,
	__tsb_ldquad_phys_patch_end;

struct tsb_phys_patch_entry {
	unsigned int	addr;
	unsigned int	insn;
};
extern struct tsb_phys_patch_entry __tsb_phys_patch, __tsb_phys_patch_end;
#endif
#define TSB_LOAD_QUAD(TSB, REG)	\
661:	ldda		[TSB] ASI_NUCLEUS_QUAD_LDD, REG; \
	.section	.tsb_ldquad_phys_patch, "ax"; \
	.word		661b; \
	ldda		[TSB] ASI_QUAD_LDD_PHYS, REG; \
	ldda		[TSB] ASI_QUAD_LDD_PHYS_4V, REG; \
	.previous

#define TSB_LOAD_TAG_HIGH(TSB, REG) \
661:	lduwa		[TSB] ASI_N, REG; \
	.section	.tsb_phys_patch, "ax"; \
	.word		661b; \
	lduwa		[TSB] ASI_PHYS_USE_EC, REG; \
	.previous

#define TSB_LOAD_TAG(TSB, REG) \
661:	ldxa		[TSB] ASI_N, REG; \
	.section	.tsb_phys_patch, "ax"; \
	.word		661b; \
	ldxa		[TSB] ASI_PHYS_USE_EC, REG; \
	.previous

#define TSB_CAS_TAG_HIGH(TSB, REG1, REG2) \
661:	casa		[TSB] ASI_N, REG1, REG2; \
	.section	.tsb_phys_patch, "ax"; \
	.word		661b; \
	casa		[TSB] ASI_PHYS_USE_EC, REG1, REG2; \
	.previous

#define TSB_CAS_TAG(TSB, REG1, REG2) \
661:	casxa		[TSB] ASI_N, REG1, REG2; \
	.section	.tsb_phys_patch, "ax"; \
	.word		661b; \
	casxa		[TSB] ASI_PHYS_USE_EC, REG1, REG2; \
	.previous

#define TSB_STORE(ADDR, VAL) \
661:	stxa		VAL, [ADDR] ASI_N; \
	.section	.tsb_phys_patch, "ax"; \
	.word		661b; \
	stxa		VAL, [ADDR] ASI_PHYS_USE_EC; \
	.previous

#define TSB_LOCK_TAG(TSB, REG1, REG2)	\
99:	TSB_LOAD_TAG_HIGH(TSB, REG1);	\
	sethi	%hi(TSB_TAG_LOCK_HIGH), REG2;\
	andcc	REG1, REG2, %g0;	\
	bne,pn	%icc, 99b;		\
	 nop;				\
	TSB_CAS_TAG_HIGH(TSB, REG1, REG2);	\
	cmp	REG1, REG2;		\
	bne,pn	%icc, 99b;		\
	 nop;				\

#define TSB_WRITE(TSB, TTE, TAG) \
	add	TSB, 0x8, TSB;   \
	TSB_STORE(TSB, TTE);     \
	sub	TSB, 0x8, TSB;   \
	TSB_STORE(TSB, TAG);

#define KERN_PGTABLE_WALK(VADDR, REG1, REG2, FAIL_LABEL)	\
	sethi		%hi(swapper_pg_dir), REG1; \
	or		REG1, %lo(swapper_pg_dir), REG1; \
	sllx		VADDR, 64 - (PGDIR_SHIFT + PGDIR_BITS), REG2; \
	srlx		REG2, 64 - PAGE_SHIFT, REG2; \
	andn		REG2, 0x3, REG2; \
	lduw		[REG1 + REG2], REG1; \
	brz,pn		REG1, FAIL_LABEL; \
	 sllx		VADDR, 64 - (PMD_SHIFT + PMD_BITS), REG2; \
	srlx		REG2, 64 - PAGE_SHIFT, REG2; \
	sllx		REG1, 11, REG1; \
	andn		REG2, 0x3, REG2; \
	lduwa		[REG1 + REG2] ASI_PHYS_USE_EC, REG1; \
	brz,pn		REG1, FAIL_LABEL; \
	 sllx		VADDR, 64 - PMD_SHIFT, REG2; \
	srlx		REG2, 64 - PAGE_SHIFT, REG2; \
	sllx		REG1, 11, REG1; \
	andn		REG2, 0x7, REG2; \
	add		REG1, REG2, REG1;

#define USER_PGTABLE_WALK_TL1(VADDR, PHYS_PGD, REG1, REG2, FAIL_LABEL)	\
	sllx		VADDR, 64 - (PGDIR_SHIFT + PGDIR_BITS), REG2; \
	srlx		REG2, 64 - PAGE_SHIFT, REG2; \
	andn		REG2, 0x3, REG2; \
	lduwa		[PHYS_PGD + REG2] ASI_PHYS_USE_EC, REG1; \
	brz,pn		REG1, FAIL_LABEL; \
	 sllx		VADDR, 64 - (PMD_SHIFT + PMD_BITS), REG2; \
	srlx		REG2, 64 - PAGE_SHIFT, REG2; \
	sllx		REG1, 11, REG1; \
	andn		REG2, 0x3, REG2; \
	lduwa		[REG1 + REG2] ASI_PHYS_USE_EC, REG1; \
	brz,pn		REG1, FAIL_LABEL; \
	 sllx		VADDR, 64 - PMD_SHIFT, REG2; \
	srlx		REG2, 64 - PAGE_SHIFT, REG2; \
	sllx		REG1, 11, REG1; \
	andn		REG2, 0x7, REG2; \
	add		REG1, REG2, REG1;

#define OBP_TRANS_LOOKUP(VADDR, REG1, REG2, REG3, FAIL_LABEL) \
	sethi		%hi(prom_trans), REG1; \
	or		REG1, %lo(prom_trans), REG1; \
97:	ldx		[REG1 + 0x00], REG2; \
	brz,pn		REG2, FAIL_LABEL; \
	 nop; \
	ldx		[REG1 + 0x08], REG3; \
	add		REG2, REG3, REG3; \
	cmp		REG2, VADDR; \
	bgu,pt		%xcc, 98f; \
	 cmp		VADDR, REG3; \
	bgeu,pt		%xcc, 98f; \
	 ldx		[REG1 + 0x10], REG3; \
	sub		VADDR, REG2, REG2; \
	ba,pt		%xcc, 99f; \
	 add		REG3, REG2, REG1; \
98:	ba,pt		%xcc, 97b; \
	 add		REG1, (3 * 8), REG1; \
99:

#define KERNEL_TSB_SIZE_BYTES	(32 * 1024)
#define KERNEL_TSB_NENTRIES	\
	(KERNEL_TSB_SIZE_BYTES / 16)
#define KERNEL_TSB4M_NENTRIES	4096

#define KTSB_PHYS_SHIFT		15

#define KERN_TSB_LOOKUP_TL1(VADDR, TAG, REG1, REG2, REG3, REG4, OK_LABEL) \
661:	sethi		%hi(swapper_tsb), REG1;			\
	or		REG1, %lo(swapper_tsb), REG1; \
	.section	.swapper_tsb_phys_patch, "ax"; \
	.word		661b; \
	.previous; \
661:	nop; \
	.section	.tsb_ldquad_phys_patch, "ax"; \
	.word		661b; \
	sllx		REG1, KTSB_PHYS_SHIFT, REG1; \
	sllx		REG1, KTSB_PHYS_SHIFT, REG1; \
	.previous; \
	srlx		VADDR, PAGE_SHIFT, REG2; \
	and		REG2, (KERNEL_TSB_NENTRIES - 1), REG2; \
	sllx		REG2, 4, REG2; \
	add		REG1, REG2, REG2; \
	TSB_LOAD_QUAD(REG2, REG3); \
	cmp		REG3, TAG; \
	be,a,pt		%xcc, OK_LABEL; \
	 mov		REG4, REG1;

#ifndef CONFIG_DEBUG_PAGEALLOC
#define KERN_TSB4M_LOOKUP_TL1(TAG, REG1, REG2, REG3, REG4, OK_LABEL) \
661:	sethi		%hi(swapper_4m_tsb), REG1;	     \
	or		REG1, %lo(swapper_4m_tsb), REG1; \
	.section	.swapper_4m_tsb_phys_patch, "ax"; \
	.word		661b; \
	.previous; \
661:	nop; \
	.section	.tsb_ldquad_phys_patch, "ax"; \
	.word		661b; \
	sllx		REG1, KTSB_PHYS_SHIFT, REG1; \
	sllx		REG1, KTSB_PHYS_SHIFT, REG1; \
	.previous; \
	and		TAG, (KERNEL_TSB4M_NENTRIES - 1), REG2; \
	sllx		REG2, 4, REG2; \
	add		REG1, REG2, REG2; \
	TSB_LOAD_QUAD(REG2, REG3); \
	cmp		REG3, TAG; \
	be,a,pt		%xcc, OK_LABEL; \
	 mov		REG4, REG1;
#endif

#endif 
