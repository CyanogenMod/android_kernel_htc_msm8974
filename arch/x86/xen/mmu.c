#include <linux/sched.h>
#include <linux/highmem.h>
#include <linux/debugfs.h>
#include <linux/bug.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/gfp.h>
#include <linux/memblock.h>
#include <linux/seq_file.h>

#include <trace/events/xen.h>

#include <asm/pgtable.h>
#include <asm/tlbflush.h>
#include <asm/fixmap.h>
#include <asm/mmu_context.h>
#include <asm/setup.h>
#include <asm/paravirt.h>
#include <asm/e820.h>
#include <asm/linkage.h>
#include <asm/page.h>
#include <asm/init.h>
#include <asm/pat.h>
#include <asm/smp.h>

#include <asm/xen/hypercall.h>
#include <asm/xen/hypervisor.h>

#include <xen/xen.h>
#include <xen/page.h>
#include <xen/interface/xen.h>
#include <xen/interface/hvm/hvm_op.h>
#include <xen/interface/version.h>
#include <xen/interface/memory.h>
#include <xen/hvc-console.h>

#include "multicalls.h"
#include "mmu.h"
#include "debugfs.h"

DEFINE_SPINLOCK(xen_reservation_lock);

#define LEVEL1_IDENT_ENTRIES	(PTRS_PER_PTE * 4)
static RESERVE_BRK_ARRAY(pte_t, level1_ident_pgt, LEVEL1_IDENT_ENTRIES);

#ifdef CONFIG_X86_64
static pud_t level3_user_vsyscall[PTRS_PER_PUD] __page_aligned_bss;
#endif 

DEFINE_PER_CPU(unsigned long, xen_cr3);	 
DEFINE_PER_CPU(unsigned long, xen_current_cr3);	 


#define USER_LIMIT	((STACK_TOP_MAX + PGDIR_SIZE - 1) & PGDIR_MASK)

unsigned long arbitrary_virt_to_mfn(void *vaddr)
{
	xmaddr_t maddr = arbitrary_virt_to_machine(vaddr);

	return PFN_DOWN(maddr.maddr);
}

xmaddr_t arbitrary_virt_to_machine(void *vaddr)
{
	unsigned long address = (unsigned long)vaddr;
	unsigned int level;
	pte_t *pte;
	unsigned offset;

	if (virt_addr_valid(vaddr))
		return virt_to_machine(vaddr);

	

	pte = lookup_address(address, &level);
	BUG_ON(pte == NULL);
	offset = address & ~PAGE_MASK;
	return XMADDR(((phys_addr_t)pte_mfn(*pte) << PAGE_SHIFT) + offset);
}
EXPORT_SYMBOL_GPL(arbitrary_virt_to_machine);

void make_lowmem_page_readonly(void *vaddr)
{
	pte_t *pte, ptev;
	unsigned long address = (unsigned long)vaddr;
	unsigned int level;

	pte = lookup_address(address, &level);
	if (pte == NULL)
		return;		

	ptev = pte_wrprotect(*pte);

	if (HYPERVISOR_update_va_mapping(address, ptev, 0))
		BUG();
}

void make_lowmem_page_readwrite(void *vaddr)
{
	pte_t *pte, ptev;
	unsigned long address = (unsigned long)vaddr;
	unsigned int level;

	pte = lookup_address(address, &level);
	if (pte == NULL)
		return;		

	ptev = pte_mkwrite(*pte);

	if (HYPERVISOR_update_va_mapping(address, ptev, 0))
		BUG();
}


static bool xen_page_pinned(void *ptr)
{
	struct page *page = virt_to_page(ptr);

	return PagePinned(page);
}

void xen_set_domain_pte(pte_t *ptep, pte_t pteval, unsigned domid)
{
	struct multicall_space mcs;
	struct mmu_update *u;

	trace_xen_mmu_set_domain_pte(ptep, pteval, domid);

	mcs = xen_mc_entry(sizeof(*u));
	u = mcs.args;

	
	u->ptr = virt_to_machine(ptep).maddr;
	u->val = pte_val_ma(pteval);

	MULTI_mmu_update(mcs.mc, mcs.args, 1, NULL, domid);

	xen_mc_issue(PARAVIRT_LAZY_MMU);
}
EXPORT_SYMBOL_GPL(xen_set_domain_pte);

static void xen_extend_mmu_update(const struct mmu_update *update)
{
	struct multicall_space mcs;
	struct mmu_update *u;

	mcs = xen_mc_extend_args(__HYPERVISOR_mmu_update, sizeof(*u));

	if (mcs.mc != NULL) {
		mcs.mc->args[1]++;
	} else {
		mcs = __xen_mc_entry(sizeof(*u));
		MULTI_mmu_update(mcs.mc, mcs.args, 1, NULL, DOMID_SELF);
	}

	u = mcs.args;
	*u = *update;
}

static void xen_extend_mmuext_op(const struct mmuext_op *op)
{
	struct multicall_space mcs;
	struct mmuext_op *u;

	mcs = xen_mc_extend_args(__HYPERVISOR_mmuext_op, sizeof(*u));

	if (mcs.mc != NULL) {
		mcs.mc->args[1]++;
	} else {
		mcs = __xen_mc_entry(sizeof(*u));
		MULTI_mmuext_op(mcs.mc, mcs.args, 1, NULL, DOMID_SELF);
	}

	u = mcs.args;
	*u = *op;
}

static void xen_set_pmd_hyper(pmd_t *ptr, pmd_t val)
{
	struct mmu_update u;

	preempt_disable();

	xen_mc_batch();

	
	u.ptr = arbitrary_virt_to_machine(ptr).maddr;
	u.val = pmd_val_ma(val);
	xen_extend_mmu_update(&u);

	xen_mc_issue(PARAVIRT_LAZY_MMU);

	preempt_enable();
}

static void xen_set_pmd(pmd_t *ptr, pmd_t val)
{
	trace_xen_mmu_set_pmd(ptr, val);

	if (!xen_page_pinned(ptr)) {
		*ptr = val;
		return;
	}

	xen_set_pmd_hyper(ptr, val);
}

void set_pte_mfn(unsigned long vaddr, unsigned long mfn, pgprot_t flags)
{
	set_pte_vaddr(vaddr, mfn_pte(mfn, flags));
}

static bool xen_batched_set_pte(pte_t *ptep, pte_t pteval)
{
	struct mmu_update u;

	if (paravirt_get_lazy_mode() != PARAVIRT_LAZY_MMU)
		return false;

	xen_mc_batch();

	u.ptr = virt_to_machine(ptep).maddr | MMU_NORMAL_PT_UPDATE;
	u.val = pte_val_ma(pteval);
	xen_extend_mmu_update(&u);

	xen_mc_issue(PARAVIRT_LAZY_MMU);

	return true;
}

static inline void __xen_set_pte(pte_t *ptep, pte_t pteval)
{
	if (!xen_batched_set_pte(ptep, pteval))
		native_set_pte(ptep, pteval);
}

static void xen_set_pte(pte_t *ptep, pte_t pteval)
{
	trace_xen_mmu_set_pte(ptep, pteval);
	__xen_set_pte(ptep, pteval);
}

static void xen_set_pte_at(struct mm_struct *mm, unsigned long addr,
		    pte_t *ptep, pte_t pteval)
{
	trace_xen_mmu_set_pte_at(mm, addr, ptep, pteval);
	__xen_set_pte(ptep, pteval);
}

pte_t xen_ptep_modify_prot_start(struct mm_struct *mm,
				 unsigned long addr, pte_t *ptep)
{
	
	trace_xen_mmu_ptep_modify_prot_start(mm, addr, ptep, *ptep);
	return *ptep;
}

void xen_ptep_modify_prot_commit(struct mm_struct *mm, unsigned long addr,
				 pte_t *ptep, pte_t pte)
{
	struct mmu_update u;

	trace_xen_mmu_ptep_modify_prot_commit(mm, addr, ptep, pte);
	xen_mc_batch();

	u.ptr = virt_to_machine(ptep).maddr | MMU_PT_UPDATE_PRESERVE_AD;
	u.val = pte_val_ma(pte);
	xen_extend_mmu_update(&u);

	xen_mc_issue(PARAVIRT_LAZY_MMU);
}

static pteval_t pte_mfn_to_pfn(pteval_t val)
{
	if (val & _PAGE_PRESENT) {
		unsigned long mfn = (val & PTE_PFN_MASK) >> PAGE_SHIFT;
		unsigned long pfn = mfn_to_pfn(mfn);

		pteval_t flags = val & PTE_FLAGS_MASK;
		if (unlikely(pfn == ~0))
			val = flags & ~_PAGE_PRESENT;
		else
			val = ((pteval_t)pfn << PAGE_SHIFT) | flags;
	}

	return val;
}

static pteval_t pte_pfn_to_mfn(pteval_t val)
{
	if (val & _PAGE_PRESENT) {
		unsigned long pfn = (val & PTE_PFN_MASK) >> PAGE_SHIFT;
		pteval_t flags = val & PTE_FLAGS_MASK;
		unsigned long mfn;

		if (!xen_feature(XENFEAT_auto_translated_physmap))
			mfn = get_phys_to_machine(pfn);
		else
			mfn = pfn;
		if (unlikely(mfn == INVALID_P2M_ENTRY)) {
			mfn = 0;
			flags = 0;
		} else {
			mfn &= ~FOREIGN_FRAME_BIT;
			if (mfn & IDENTITY_FRAME_BIT) {
				mfn &= ~IDENTITY_FRAME_BIT;
				flags |= _PAGE_IOMAP;
			}
		}
		val = ((pteval_t)mfn << PAGE_SHIFT) | flags;
	}

	return val;
}

static pteval_t iomap_pte(pteval_t val)
{
	if (val & _PAGE_PRESENT) {
		unsigned long pfn = (val & PTE_PFN_MASK) >> PAGE_SHIFT;
		pteval_t flags = val & PTE_FLAGS_MASK;

		val = ((pteval_t)pfn << PAGE_SHIFT) | flags;
	}

	return val;
}

static pteval_t xen_pte_val(pte_t pte)
{
	pteval_t pteval = pte.pte;
#if 0
	
	if ((pteval & (_PAGE_PAT | _PAGE_PCD | _PAGE_PWT)) == _PAGE_PAT) {
		WARN_ON(!pat_enabled);
		pteval = (pteval & ~_PAGE_PAT) | _PAGE_PWT;
	}
#endif
	if (xen_initial_domain() && (pteval & _PAGE_IOMAP))
		return pteval;

	return pte_mfn_to_pfn(pteval);
}
PV_CALLEE_SAVE_REGS_THUNK(xen_pte_val);

static pgdval_t xen_pgd_val(pgd_t pgd)
{
	return pte_mfn_to_pfn(pgd.pgd);
}
PV_CALLEE_SAVE_REGS_THUNK(xen_pgd_val);


void xen_set_pat(u64 pat)
{
	WARN_ON(pat != 0x0007010600070106ull);
}

static pte_t xen_make_pte(pteval_t pte)
{
	phys_addr_t addr = (pte & PTE_PFN_MASK);
#if 0
	if (pat_enabled && !WARN_ON(pte & _PAGE_PAT)) {
		if ((pte & (_PAGE_PCD | _PAGE_PWT)) == _PAGE_PWT)
			pte = (pte & ~(_PAGE_PCD | _PAGE_PWT)) | _PAGE_PAT;
	}
#endif
	if (unlikely(pte & _PAGE_IOMAP) &&
	    (xen_initial_domain() || addr >= ISA_END_ADDRESS)) {
		pte = iomap_pte(pte);
	} else {
		pte &= ~_PAGE_IOMAP;
		pte = pte_pfn_to_mfn(pte);
	}

	return native_make_pte(pte);
}
PV_CALLEE_SAVE_REGS_THUNK(xen_make_pte);

static pgd_t xen_make_pgd(pgdval_t pgd)
{
	pgd = pte_pfn_to_mfn(pgd);
	return native_make_pgd(pgd);
}
PV_CALLEE_SAVE_REGS_THUNK(xen_make_pgd);

static pmdval_t xen_pmd_val(pmd_t pmd)
{
	return pte_mfn_to_pfn(pmd.pmd);
}
PV_CALLEE_SAVE_REGS_THUNK(xen_pmd_val);

static void xen_set_pud_hyper(pud_t *ptr, pud_t val)
{
	struct mmu_update u;

	preempt_disable();

	xen_mc_batch();

	
	u.ptr = arbitrary_virt_to_machine(ptr).maddr;
	u.val = pud_val_ma(val);
	xen_extend_mmu_update(&u);

	xen_mc_issue(PARAVIRT_LAZY_MMU);

	preempt_enable();
}

static void xen_set_pud(pud_t *ptr, pud_t val)
{
	trace_xen_mmu_set_pud(ptr, val);

	if (!xen_page_pinned(ptr)) {
		*ptr = val;
		return;
	}

	xen_set_pud_hyper(ptr, val);
}

#ifdef CONFIG_X86_PAE
static void xen_set_pte_atomic(pte_t *ptep, pte_t pte)
{
	trace_xen_mmu_set_pte_atomic(ptep, pte);
	set_64bit((u64 *)ptep, native_pte_val(pte));
}

static void xen_pte_clear(struct mm_struct *mm, unsigned long addr, pte_t *ptep)
{
	trace_xen_mmu_pte_clear(mm, addr, ptep);
	if (!xen_batched_set_pte(ptep, native_make_pte(0)))
		native_pte_clear(mm, addr, ptep);
}

static void xen_pmd_clear(pmd_t *pmdp)
{
	trace_xen_mmu_pmd_clear(pmdp);
	set_pmd(pmdp, __pmd(0));
}
#endif	

static pmd_t xen_make_pmd(pmdval_t pmd)
{
	pmd = pte_pfn_to_mfn(pmd);
	return native_make_pmd(pmd);
}
PV_CALLEE_SAVE_REGS_THUNK(xen_make_pmd);

#if PAGETABLE_LEVELS == 4
static pudval_t xen_pud_val(pud_t pud)
{
	return pte_mfn_to_pfn(pud.pud);
}
PV_CALLEE_SAVE_REGS_THUNK(xen_pud_val);

static pud_t xen_make_pud(pudval_t pud)
{
	pud = pte_pfn_to_mfn(pud);

	return native_make_pud(pud);
}
PV_CALLEE_SAVE_REGS_THUNK(xen_make_pud);

static pgd_t *xen_get_user_pgd(pgd_t *pgd)
{
	pgd_t *pgd_page = (pgd_t *)(((unsigned long)pgd) & PAGE_MASK);
	unsigned offset = pgd - pgd_page;
	pgd_t *user_ptr = NULL;

	if (offset < pgd_index(USER_LIMIT)) {
		struct page *page = virt_to_page(pgd_page);
		user_ptr = (pgd_t *)page->private;
		if (user_ptr)
			user_ptr += offset;
	}

	return user_ptr;
}

static void __xen_set_pgd_hyper(pgd_t *ptr, pgd_t val)
{
	struct mmu_update u;

	u.ptr = virt_to_machine(ptr).maddr;
	u.val = pgd_val_ma(val);
	xen_extend_mmu_update(&u);
}

static void __init xen_set_pgd_hyper(pgd_t *ptr, pgd_t val)
{
	preempt_disable();

	xen_mc_batch();

	__xen_set_pgd_hyper(ptr, val);

	xen_mc_issue(PARAVIRT_LAZY_MMU);

	preempt_enable();
}

static void xen_set_pgd(pgd_t *ptr, pgd_t val)
{
	pgd_t *user_ptr = xen_get_user_pgd(ptr);

	trace_xen_mmu_set_pgd(ptr, user_ptr, val);

	if (!xen_page_pinned(ptr)) {
		*ptr = val;
		if (user_ptr) {
			WARN_ON(xen_page_pinned(user_ptr));
			*user_ptr = val;
		}
		return;
	}

	xen_mc_batch();

	__xen_set_pgd_hyper(ptr, val);
	if (user_ptr)
		__xen_set_pgd_hyper(user_ptr, val);

	xen_mc_issue(PARAVIRT_LAZY_MMU);
}
#endif	

static int __xen_pgd_walk(struct mm_struct *mm, pgd_t *pgd,
			  int (*func)(struct mm_struct *mm, struct page *,
				      enum pt_level),
			  unsigned long limit)
{
	int flush = 0;
	unsigned hole_low, hole_high;
	unsigned pgdidx_limit, pudidx_limit, pmdidx_limit;
	unsigned pgdidx, pudidx, pmdidx;

	
	limit--;
	BUG_ON(limit >= FIXADDR_TOP);

	if (xen_feature(XENFEAT_auto_translated_physmap))
		return 0;

	hole_low = pgd_index(USER_LIMIT);
	hole_high = pgd_index(PAGE_OFFSET);

	pgdidx_limit = pgd_index(limit);
#if PTRS_PER_PUD > 1
	pudidx_limit = pud_index(limit);
#else
	pudidx_limit = 0;
#endif
#if PTRS_PER_PMD > 1
	pmdidx_limit = pmd_index(limit);
#else
	pmdidx_limit = 0;
#endif

	for (pgdidx = 0; pgdidx <= pgdidx_limit; pgdidx++) {
		pud_t *pud;

		if (pgdidx >= hole_low && pgdidx < hole_high)
			continue;

		if (!pgd_val(pgd[pgdidx]))
			continue;

		pud = pud_offset(&pgd[pgdidx], 0);

		if (PTRS_PER_PUD > 1) 
			flush |= (*func)(mm, virt_to_page(pud), PT_PUD);

		for (pudidx = 0; pudidx < PTRS_PER_PUD; pudidx++) {
			pmd_t *pmd;

			if (pgdidx == pgdidx_limit &&
			    pudidx > pudidx_limit)
				goto out;

			if (pud_none(pud[pudidx]))
				continue;

			pmd = pmd_offset(&pud[pudidx], 0);

			if (PTRS_PER_PMD > 1) 
				flush |= (*func)(mm, virt_to_page(pmd), PT_PMD);

			for (pmdidx = 0; pmdidx < PTRS_PER_PMD; pmdidx++) {
				struct page *pte;

				if (pgdidx == pgdidx_limit &&
				    pudidx == pudidx_limit &&
				    pmdidx > pmdidx_limit)
					goto out;

				if (pmd_none(pmd[pmdidx]))
					continue;

				pte = pmd_page(pmd[pmdidx]);
				flush |= (*func)(mm, pte, PT_PTE);
			}
		}
	}

out:
	flush |= (*func)(mm, virt_to_page(pgd), PT_PGD);

	return flush;
}

static int xen_pgd_walk(struct mm_struct *mm,
			int (*func)(struct mm_struct *mm, struct page *,
				    enum pt_level),
			unsigned long limit)
{
	return __xen_pgd_walk(mm, mm->pgd, func, limit);
}

static spinlock_t *xen_pte_lock(struct page *page, struct mm_struct *mm)
{
	spinlock_t *ptl = NULL;

#if USE_SPLIT_PTLOCKS
	ptl = __pte_lockptr(page);
	spin_lock_nest_lock(ptl, &mm->page_table_lock);
#endif

	return ptl;
}

static void xen_pte_unlock(void *v)
{
	spinlock_t *ptl = v;
	spin_unlock(ptl);
}

static void xen_do_pin(unsigned level, unsigned long pfn)
{
	struct mmuext_op op;

	op.cmd = level;
	op.arg1.mfn = pfn_to_mfn(pfn);

	xen_extend_mmuext_op(&op);
}

static int xen_pin_page(struct mm_struct *mm, struct page *page,
			enum pt_level level)
{
	unsigned pgfl = TestSetPagePinned(page);
	int flush;

	if (pgfl)
		flush = 0;		
	else if (PageHighMem(page))
		flush = 1;
	else {
		void *pt = lowmem_page_address(page);
		unsigned long pfn = page_to_pfn(page);
		struct multicall_space mcs = __xen_mc_entry(0);
		spinlock_t *ptl;

		flush = 0;

		ptl = NULL;
		if (level == PT_PTE)
			ptl = xen_pte_lock(page, mm);

		MULTI_update_va_mapping(mcs.mc, (unsigned long)pt,
					pfn_pte(pfn, PAGE_KERNEL_RO),
					level == PT_PGD ? UVMF_TLB_FLUSH : 0);

		if (ptl) {
			xen_do_pin(MMUEXT_PIN_L1_TABLE, pfn);

			xen_mc_callback(xen_pte_unlock, ptl);
		}
	}

	return flush;
}

static void __xen_pgd_pin(struct mm_struct *mm, pgd_t *pgd)
{
	trace_xen_mmu_pgd_pin(mm, pgd);

	xen_mc_batch();

	if (__xen_pgd_walk(mm, pgd, xen_pin_page, USER_LIMIT)) {
		
		xen_mc_issue(0);

		kmap_flush_unused();

		xen_mc_batch();
	}

#ifdef CONFIG_X86_64
	{
		pgd_t *user_pgd = xen_get_user_pgd(pgd);

		xen_do_pin(MMUEXT_PIN_L4_TABLE, PFN_DOWN(__pa(pgd)));

		if (user_pgd) {
			xen_pin_page(mm, virt_to_page(user_pgd), PT_PGD);
			xen_do_pin(MMUEXT_PIN_L4_TABLE,
				   PFN_DOWN(__pa(user_pgd)));
		}
	}
#else 
#ifdef CONFIG_X86_PAE
	
	xen_pin_page(mm, pgd_page(pgd[pgd_index(TASK_SIZE)]),
		     PT_PMD);
#endif
	xen_do_pin(MMUEXT_PIN_L3_TABLE, PFN_DOWN(__pa(pgd)));
#endif 
	xen_mc_issue(0);
}

static void xen_pgd_pin(struct mm_struct *mm)
{
	__xen_pgd_pin(mm, mm->pgd);
}

void xen_mm_pin_all(void)
{
	struct page *page;

	spin_lock(&pgd_lock);

	list_for_each_entry(page, &pgd_list, lru) {
		if (!PagePinned(page)) {
			__xen_pgd_pin(&init_mm, (pgd_t *)page_address(page));
			SetPageSavePinned(page);
		}
	}

	spin_unlock(&pgd_lock);
}

static int __init xen_mark_pinned(struct mm_struct *mm, struct page *page,
				  enum pt_level level)
{
	SetPagePinned(page);
	return 0;
}

static void __init xen_mark_init_mm_pinned(void)
{
	xen_pgd_walk(&init_mm, xen_mark_pinned, FIXADDR_TOP);
}

static int xen_unpin_page(struct mm_struct *mm, struct page *page,
			  enum pt_level level)
{
	unsigned pgfl = TestClearPagePinned(page);

	if (pgfl && !PageHighMem(page)) {
		void *pt = lowmem_page_address(page);
		unsigned long pfn = page_to_pfn(page);
		spinlock_t *ptl = NULL;
		struct multicall_space mcs;

		if (level == PT_PTE) {
			ptl = xen_pte_lock(page, mm);

			if (ptl)
				xen_do_pin(MMUEXT_UNPIN_TABLE, pfn);
		}

		mcs = __xen_mc_entry(0);

		MULTI_update_va_mapping(mcs.mc, (unsigned long)pt,
					pfn_pte(pfn, PAGE_KERNEL),
					level == PT_PGD ? UVMF_TLB_FLUSH : 0);

		if (ptl) {
			
			xen_mc_callback(xen_pte_unlock, ptl);
		}
	}

	return 0;		
}

static void __xen_pgd_unpin(struct mm_struct *mm, pgd_t *pgd)
{
	trace_xen_mmu_pgd_unpin(mm, pgd);

	xen_mc_batch();

	xen_do_pin(MMUEXT_UNPIN_TABLE, PFN_DOWN(__pa(pgd)));

#ifdef CONFIG_X86_64
	{
		pgd_t *user_pgd = xen_get_user_pgd(pgd);

		if (user_pgd) {
			xen_do_pin(MMUEXT_UNPIN_TABLE,
				   PFN_DOWN(__pa(user_pgd)));
			xen_unpin_page(mm, virt_to_page(user_pgd), PT_PGD);
		}
	}
#endif

#ifdef CONFIG_X86_PAE
	
	xen_unpin_page(mm, pgd_page(pgd[pgd_index(TASK_SIZE)]),
		       PT_PMD);
#endif

	__xen_pgd_walk(mm, pgd, xen_unpin_page, USER_LIMIT);

	xen_mc_issue(0);
}

static void xen_pgd_unpin(struct mm_struct *mm)
{
	__xen_pgd_unpin(mm, mm->pgd);
}

void xen_mm_unpin_all(void)
{
	struct page *page;

	spin_lock(&pgd_lock);

	list_for_each_entry(page, &pgd_list, lru) {
		if (PageSavePinned(page)) {
			BUG_ON(!PagePinned(page));
			__xen_pgd_unpin(&init_mm, (pgd_t *)page_address(page));
			ClearPageSavePinned(page);
		}
	}

	spin_unlock(&pgd_lock);
}

static void xen_activate_mm(struct mm_struct *prev, struct mm_struct *next)
{
	spin_lock(&next->page_table_lock);
	xen_pgd_pin(next);
	spin_unlock(&next->page_table_lock);
}

static void xen_dup_mmap(struct mm_struct *oldmm, struct mm_struct *mm)
{
	spin_lock(&mm->page_table_lock);
	xen_pgd_pin(mm);
	spin_unlock(&mm->page_table_lock);
}


#ifdef CONFIG_SMP
static void drop_other_mm_ref(void *info)
{
	struct mm_struct *mm = info;
	struct mm_struct *active_mm;

	active_mm = this_cpu_read(cpu_tlbstate.active_mm);

	if (active_mm == mm && this_cpu_read(cpu_tlbstate.state) != TLBSTATE_OK)
		leave_mm(smp_processor_id());

	if (this_cpu_read(xen_current_cr3) == __pa(mm->pgd))
		load_cr3(swapper_pg_dir);
}

static void xen_drop_mm_ref(struct mm_struct *mm)
{
	cpumask_var_t mask;
	unsigned cpu;

	if (current->active_mm == mm) {
		if (current->mm == mm)
			load_cr3(swapper_pg_dir);
		else
			leave_mm(smp_processor_id());
	}

	
	if (!alloc_cpumask_var(&mask, GFP_ATOMIC)) {
		for_each_online_cpu(cpu) {
			if (!cpumask_test_cpu(cpu, mm_cpumask(mm))
			    && per_cpu(xen_current_cr3, cpu) != __pa(mm->pgd))
				continue;
			smp_call_function_single(cpu, drop_other_mm_ref, mm, 1);
		}
		return;
	}
	cpumask_copy(mask, mm_cpumask(mm));

	for_each_online_cpu(cpu) {
		if (per_cpu(xen_current_cr3, cpu) == __pa(mm->pgd))
			cpumask_set_cpu(cpu, mask);
	}

	if (!cpumask_empty(mask))
		smp_call_function_many(mask, drop_other_mm_ref, mm, 1);
	free_cpumask_var(mask);
}
#else
static void xen_drop_mm_ref(struct mm_struct *mm)
{
	if (current->active_mm == mm)
		load_cr3(swapper_pg_dir);
}
#endif

static void xen_exit_mmap(struct mm_struct *mm)
{
	get_cpu();		
	xen_drop_mm_ref(mm);
	put_cpu();

	spin_lock(&mm->page_table_lock);

	
	if (xen_page_pinned(mm->pgd))
		xen_pgd_unpin(mm);

	spin_unlock(&mm->page_table_lock);
}

static void __init xen_pagetable_setup_start(pgd_t *base)
{
}

static __init void xen_mapping_pagetable_reserve(u64 start, u64 end)
{
	
	native_pagetable_reserve(start, end);

	
	printk(KERN_DEBUG "xen: setting RW the range %llx - %llx\n", end,
			PFN_PHYS(pgt_buf_top));
	while (end < PFN_PHYS(pgt_buf_top)) {
		make_lowmem_page_readwrite(__va(end));
		end += PAGE_SIZE;
	}
}

static void xen_post_allocator_init(void);

static void __init xen_pagetable_setup_done(pgd_t *base)
{
	xen_setup_shared_info();
	xen_post_allocator_init();
}

static void xen_write_cr2(unsigned long cr2)
{
	this_cpu_read(xen_vcpu)->arch.cr2 = cr2;
}

static unsigned long xen_read_cr2(void)
{
	return this_cpu_read(xen_vcpu)->arch.cr2;
}

unsigned long xen_read_cr2_direct(void)
{
	return this_cpu_read(xen_vcpu_info.arch.cr2);
}

static void xen_flush_tlb(void)
{
	struct mmuext_op *op;
	struct multicall_space mcs;

	trace_xen_mmu_flush_tlb(0);

	preempt_disable();

	mcs = xen_mc_entry(sizeof(*op));

	op = mcs.args;
	op->cmd = MMUEXT_TLB_FLUSH_LOCAL;
	MULTI_mmuext_op(mcs.mc, op, 1, NULL, DOMID_SELF);

	xen_mc_issue(PARAVIRT_LAZY_MMU);

	preempt_enable();
}

static void xen_flush_tlb_single(unsigned long addr)
{
	struct mmuext_op *op;
	struct multicall_space mcs;

	trace_xen_mmu_flush_tlb_single(addr);

	preempt_disable();

	mcs = xen_mc_entry(sizeof(*op));
	op = mcs.args;
	op->cmd = MMUEXT_INVLPG_LOCAL;
	op->arg1.linear_addr = addr & PAGE_MASK;
	MULTI_mmuext_op(mcs.mc, op, 1, NULL, DOMID_SELF);

	xen_mc_issue(PARAVIRT_LAZY_MMU);

	preempt_enable();
}

static void xen_flush_tlb_others(const struct cpumask *cpus,
				 struct mm_struct *mm, unsigned long va)
{
	struct {
		struct mmuext_op op;
#ifdef CONFIG_SMP
		DECLARE_BITMAP(mask, num_processors);
#else
		DECLARE_BITMAP(mask, NR_CPUS);
#endif
	} *args;
	struct multicall_space mcs;

	trace_xen_mmu_flush_tlb_others(cpus, mm, va);

	if (cpumask_empty(cpus))
		return;		

	mcs = xen_mc_entry(sizeof(*args));
	args = mcs.args;
	args->op.arg2.vcpumask = to_cpumask(args->mask);

	
	cpumask_and(to_cpumask(args->mask), cpus, cpu_online_mask);
	cpumask_clear_cpu(smp_processor_id(), to_cpumask(args->mask));

	if (va == TLB_FLUSH_ALL) {
		args->op.cmd = MMUEXT_TLB_FLUSH_MULTI;
	} else {
		args->op.cmd = MMUEXT_INVLPG_MULTI;
		args->op.arg1.linear_addr = va;
	}

	MULTI_mmuext_op(mcs.mc, &args->op, 1, NULL, DOMID_SELF);

	xen_mc_issue(PARAVIRT_LAZY_MMU);
}

static unsigned long xen_read_cr3(void)
{
	return this_cpu_read(xen_cr3);
}

static void set_current_cr3(void *v)
{
	this_cpu_write(xen_current_cr3, (unsigned long)v);
}

static void __xen_write_cr3(bool kernel, unsigned long cr3)
{
	struct mmuext_op op;
	unsigned long mfn;

	trace_xen_mmu_write_cr3(kernel, cr3);

	if (cr3)
		mfn = pfn_to_mfn(PFN_DOWN(cr3));
	else
		mfn = 0;

	WARN_ON(mfn == 0 && kernel);

	op.cmd = kernel ? MMUEXT_NEW_BASEPTR : MMUEXT_NEW_USER_BASEPTR;
	op.arg1.mfn = mfn;

	xen_extend_mmuext_op(&op);

	if (kernel) {
		this_cpu_write(xen_cr3, cr3);

		xen_mc_callback(set_current_cr3, (void *)cr3);
	}
}

static void xen_write_cr3(unsigned long cr3)
{
	BUG_ON(preemptible());

	xen_mc_batch();  

	this_cpu_write(xen_cr3, cr3);

	__xen_write_cr3(true, cr3);

#ifdef CONFIG_X86_64
	{
		pgd_t *user_pgd = xen_get_user_pgd(__va(cr3));
		if (user_pgd)
			__xen_write_cr3(false, __pa(user_pgd));
		else
			__xen_write_cr3(false, 0);
	}
#endif

	xen_mc_issue(PARAVIRT_LAZY_CPU);  
}

static int xen_pgd_alloc(struct mm_struct *mm)
{
	pgd_t *pgd = mm->pgd;
	int ret = 0;

	BUG_ON(PagePinned(virt_to_page(pgd)));

#ifdef CONFIG_X86_64
	{
		struct page *page = virt_to_page(pgd);
		pgd_t *user_pgd;

		BUG_ON(page->private != 0);

		ret = -ENOMEM;

		user_pgd = (pgd_t *)__get_free_page(GFP_KERNEL | __GFP_ZERO);
		page->private = (unsigned long)user_pgd;

		if (user_pgd != NULL) {
			user_pgd[pgd_index(VSYSCALL_START)] =
				__pgd(__pa(level3_user_vsyscall) | _PAGE_TABLE);
			ret = 0;
		}

		BUG_ON(PagePinned(virt_to_page(xen_get_user_pgd(pgd))));
	}
#endif

	return ret;
}

static void xen_pgd_free(struct mm_struct *mm, pgd_t *pgd)
{
#ifdef CONFIG_X86_64
	pgd_t *user_pgd = xen_get_user_pgd(pgd);

	if (user_pgd)
		free_page((unsigned long)user_pgd);
#endif
}

#ifdef CONFIG_X86_32
static pte_t __init mask_rw_pte(pte_t *ptep, pte_t pte)
{
	
	if (pte_val_ma(*ptep) & _PAGE_PRESENT)
		pte = __pte_ma(((pte_val_ma(*ptep) & _PAGE_RW) | ~_PAGE_RW) &
			       pte_val_ma(pte));

	return pte;
}
#else 
static pte_t __init mask_rw_pte(pte_t *ptep, pte_t pte)
{
	unsigned long pfn = pte_pfn(pte);

	if (((!is_early_ioremap_ptep(ptep) &&
			pfn >= pgt_buf_start && pfn < pgt_buf_top)) ||
			(is_early_ioremap_ptep(ptep) && pfn != (pgt_buf_end - 1)))
		pte = pte_wrprotect(pte);

	return pte;
}
#endif 

static void __init xen_set_pte_init(pte_t *ptep, pte_t pte)
{
	pte = mask_rw_pte(ptep, pte);

	xen_set_pte(ptep, pte);
}

static void pin_pagetable_pfn(unsigned cmd, unsigned long pfn)
{
	struct mmuext_op op;
	op.cmd = cmd;
	op.arg1.mfn = pfn_to_mfn(pfn);
	if (HYPERVISOR_mmuext_op(&op, 1, NULL, DOMID_SELF))
		BUG();
}

static void __init xen_alloc_pte_init(struct mm_struct *mm, unsigned long pfn)
{
#ifdef CONFIG_FLATMEM
	BUG_ON(mem_map);	
#endif
	make_lowmem_page_readonly(__va(PFN_PHYS(pfn)));
	pin_pagetable_pfn(MMUEXT_PIN_L1_TABLE, pfn);
}

static void __init xen_alloc_pmd_init(struct mm_struct *mm, unsigned long pfn)
{
#ifdef CONFIG_FLATMEM
	BUG_ON(mem_map);	
#endif
	make_lowmem_page_readonly(__va(PFN_PHYS(pfn)));
}

static void __init xen_release_pte_init(unsigned long pfn)
{
	pin_pagetable_pfn(MMUEXT_UNPIN_TABLE, pfn);
	make_lowmem_page_readwrite(__va(PFN_PHYS(pfn)));
}

static void __init xen_release_pmd_init(unsigned long pfn)
{
	make_lowmem_page_readwrite(__va(PFN_PHYS(pfn)));
}

static inline void __pin_pagetable_pfn(unsigned cmd, unsigned long pfn)
{
	struct multicall_space mcs;
	struct mmuext_op *op;

	mcs = __xen_mc_entry(sizeof(*op));
	op = mcs.args;
	op->cmd = cmd;
	op->arg1.mfn = pfn_to_mfn(pfn);

	MULTI_mmuext_op(mcs.mc, mcs.args, 1, NULL, DOMID_SELF);
}

static inline void __set_pfn_prot(unsigned long pfn, pgprot_t prot)
{
	struct multicall_space mcs;
	unsigned long addr = (unsigned long)__va(pfn << PAGE_SHIFT);

	mcs = __xen_mc_entry(0);
	MULTI_update_va_mapping(mcs.mc, (unsigned long)addr,
				pfn_pte(pfn, prot), 0);
}

static inline void xen_alloc_ptpage(struct mm_struct *mm, unsigned long pfn,
				    unsigned level)
{
	bool pinned = PagePinned(virt_to_page(mm->pgd));

	trace_xen_mmu_alloc_ptpage(mm, pfn, level, pinned);

	if (pinned) {
		struct page *page = pfn_to_page(pfn);

		SetPagePinned(page);

		if (!PageHighMem(page)) {
			xen_mc_batch();

			__set_pfn_prot(pfn, PAGE_KERNEL_RO);

			if (level == PT_PTE && USE_SPLIT_PTLOCKS)
				__pin_pagetable_pfn(MMUEXT_PIN_L1_TABLE, pfn);

			xen_mc_issue(PARAVIRT_LAZY_MMU);
		} else {
			kmap_flush_unused();
		}
	}
}

static void xen_alloc_pte(struct mm_struct *mm, unsigned long pfn)
{
	xen_alloc_ptpage(mm, pfn, PT_PTE);
}

static void xen_alloc_pmd(struct mm_struct *mm, unsigned long pfn)
{
	xen_alloc_ptpage(mm, pfn, PT_PMD);
}

static inline void xen_release_ptpage(unsigned long pfn, unsigned level)
{
	struct page *page = pfn_to_page(pfn);
	bool pinned = PagePinned(page);

	trace_xen_mmu_release_ptpage(pfn, level, pinned);

	if (pinned) {
		if (!PageHighMem(page)) {
			xen_mc_batch();

			if (level == PT_PTE && USE_SPLIT_PTLOCKS)
				__pin_pagetable_pfn(MMUEXT_UNPIN_TABLE, pfn);

			__set_pfn_prot(pfn, PAGE_KERNEL);

			xen_mc_issue(PARAVIRT_LAZY_MMU);
		}
		ClearPagePinned(page);
	}
}

static void xen_release_pte(unsigned long pfn)
{
	xen_release_ptpage(pfn, PT_PTE);
}

static void xen_release_pmd(unsigned long pfn)
{
	xen_release_ptpage(pfn, PT_PMD);
}

#if PAGETABLE_LEVELS == 4
static void xen_alloc_pud(struct mm_struct *mm, unsigned long pfn)
{
	xen_alloc_ptpage(mm, pfn, PT_PUD);
}

static void xen_release_pud(unsigned long pfn)
{
	xen_release_ptpage(pfn, PT_PUD);
}
#endif

void __init xen_reserve_top(void)
{
#ifdef CONFIG_X86_32
	unsigned long top = HYPERVISOR_VIRT_START;
	struct xen_platform_parameters pp;

	if (HYPERVISOR_xen_version(XENVER_platform_parameters, &pp) == 0)
		top = pp.virt_start;

	reserve_top_address(-top);
#endif	
}

static void *__ka(phys_addr_t paddr)
{
#ifdef CONFIG_X86_64
	return (void *)(paddr + __START_KERNEL_map);
#else
	return __va(paddr);
#endif
}

static unsigned long m2p(phys_addr_t maddr)
{
	phys_addr_t paddr;

	maddr &= PTE_PFN_MASK;
	paddr = mfn_to_pfn(maddr >> PAGE_SHIFT) << PAGE_SHIFT;

	return paddr;
}

static void *m2v(phys_addr_t maddr)
{
	return __ka(m2p(maddr));
}

static void set_page_prot(void *addr, pgprot_t prot)
{
	unsigned long pfn = __pa(addr) >> PAGE_SHIFT;
	pte_t pte = pfn_pte(pfn, prot);

	if (HYPERVISOR_update_va_mapping((unsigned long)addr, pte, 0))
		BUG();
}

static void __init xen_map_identity_early(pmd_t *pmd, unsigned long max_pfn)
{
	unsigned pmdidx, pteidx;
	unsigned ident_pte;
	unsigned long pfn;

	level1_ident_pgt = extend_brk(sizeof(pte_t) * LEVEL1_IDENT_ENTRIES,
				      PAGE_SIZE);

	ident_pte = 0;
	pfn = 0;
	for (pmdidx = 0; pmdidx < PTRS_PER_PMD && pfn < max_pfn; pmdidx++) {
		pte_t *pte_page;

		
		if (pmd_present(pmd[pmdidx]))
			pte_page = m2v(pmd[pmdidx].pmd);
		else {
			
			if (ident_pte == LEVEL1_IDENT_ENTRIES)
				break;

			pte_page = &level1_ident_pgt[ident_pte];
			ident_pte += PTRS_PER_PTE;

			pmd[pmdidx] = __pmd(__pa(pte_page) | _PAGE_TABLE);
		}

		
		for (pteidx = 0; pteidx < PTRS_PER_PTE; pteidx++, pfn++) {
			pte_t pte;

#ifdef CONFIG_X86_32
			if (pfn > max_pfn_mapped)
				max_pfn_mapped = pfn;
#endif

			if (!pte_none(pte_page[pteidx]))
				continue;

			pte = pfn_pte(pfn, PAGE_KERNEL_EXEC);
			pte_page[pteidx] = pte;
		}
	}

	for (pteidx = 0; pteidx < ident_pte; pteidx += PTRS_PER_PTE)
		set_page_prot(&level1_ident_pgt[pteidx], PAGE_KERNEL_RO);

	set_page_prot(pmd, PAGE_KERNEL_RO);
}

void __init xen_setup_machphys_mapping(void)
{
	struct xen_machphys_mapping mapping;

	if (HYPERVISOR_memory_op(XENMEM_machphys_mapping, &mapping) == 0) {
		machine_to_phys_mapping = (unsigned long *)mapping.v_start;
		machine_to_phys_nr = mapping.max_mfn + 1;
	} else {
		machine_to_phys_nr = MACH2PHYS_NR_ENTRIES;
	}
#ifdef CONFIG_X86_32
	WARN_ON((machine_to_phys_mapping + (machine_to_phys_nr - 1))
		< machine_to_phys_mapping);
#endif
}

#ifdef CONFIG_X86_64
static void convert_pfn_mfn(void *v)
{
	pte_t *pte = v;
	int i;

	for (i = 0; i < PTRS_PER_PTE; i++)
		pte[i] = xen_make_pte(pte[i].pte);
}

pgd_t * __init xen_setup_kernel_pagetable(pgd_t *pgd,
					 unsigned long max_pfn)
{
	pud_t *l3;
	pmd_t *l2;

	max_pfn_mapped = PFN_DOWN(__pa(xen_start_info->mfn_list));

	
	init_level4_pgt[0] = __pgd(0);

	
	convert_pfn_mfn(init_level4_pgt);
	convert_pfn_mfn(level3_ident_pgt);
	convert_pfn_mfn(level3_kernel_pgt);

	l3 = m2v(pgd[pgd_index(__START_KERNEL_map)].pgd);
	l2 = m2v(l3[pud_index(__START_KERNEL_map)].pud);

	memcpy(level2_ident_pgt, l2, sizeof(pmd_t) * PTRS_PER_PMD);
	memcpy(level2_kernel_pgt, l2, sizeof(pmd_t) * PTRS_PER_PMD);

	l3 = m2v(pgd[pgd_index(__START_KERNEL_map + PMD_SIZE)].pgd);
	l2 = m2v(l3[pud_index(__START_KERNEL_map + PMD_SIZE)].pud);
	memcpy(level2_fixmap_pgt, l2, sizeof(pmd_t) * PTRS_PER_PMD);

	
	xen_map_identity_early(level2_ident_pgt, max_pfn);

	
	set_page_prot(init_level4_pgt, PAGE_KERNEL_RO);
	set_page_prot(level3_ident_pgt, PAGE_KERNEL_RO);
	set_page_prot(level3_kernel_pgt, PAGE_KERNEL_RO);
	set_page_prot(level3_user_vsyscall, PAGE_KERNEL_RO);
	set_page_prot(level2_kernel_pgt, PAGE_KERNEL_RO);
	set_page_prot(level2_fixmap_pgt, PAGE_KERNEL_RO);

	
	pin_pagetable_pfn(MMUEXT_PIN_L4_TABLE,
			  PFN_DOWN(__pa_symbol(init_level4_pgt)));

	
	pin_pagetable_pfn(MMUEXT_UNPIN_TABLE, PFN_DOWN(__pa(pgd)));

	
	pgd = init_level4_pgt;

	xen_mc_batch();
	__xen_write_cr3(true, __pa(pgd));
	xen_mc_issue(PARAVIRT_LAZY_CPU);

	memblock_reserve(__pa(xen_start_info->pt_base),
			 xen_start_info->nr_pt_frames * PAGE_SIZE);

	return pgd;
}
#else	
static RESERVE_BRK_ARRAY(pmd_t, initial_kernel_pmd, PTRS_PER_PMD);
static RESERVE_BRK_ARRAY(pmd_t, swapper_kernel_pmd, PTRS_PER_PMD);

static void __init xen_write_cr3_init(unsigned long cr3)
{
	unsigned long pfn = PFN_DOWN(__pa(swapper_pg_dir));

	BUG_ON(read_cr3() != __pa(initial_page_table));
	BUG_ON(cr3 != __pa(swapper_pg_dir));

	swapper_kernel_pmd =
		extend_brk(sizeof(pmd_t) * PTRS_PER_PMD, PAGE_SIZE);
	memcpy(swapper_kernel_pmd, initial_kernel_pmd,
	       sizeof(pmd_t) * PTRS_PER_PMD);
	swapper_pg_dir[KERNEL_PGD_BOUNDARY] =
		__pgd(__pa(swapper_kernel_pmd) | _PAGE_PRESENT);
	set_page_prot(swapper_kernel_pmd, PAGE_KERNEL_RO);

	set_page_prot(swapper_pg_dir, PAGE_KERNEL_RO);
	xen_write_cr3(cr3);
	pin_pagetable_pfn(MMUEXT_PIN_L3_TABLE, pfn);

	pin_pagetable_pfn(MMUEXT_UNPIN_TABLE,
			  PFN_DOWN(__pa(initial_page_table)));
	set_page_prot(initial_page_table, PAGE_KERNEL);
	set_page_prot(initial_kernel_pmd, PAGE_KERNEL);

	pv_mmu_ops.write_cr3 = &xen_write_cr3;
}

pgd_t * __init xen_setup_kernel_pagetable(pgd_t *pgd,
					 unsigned long max_pfn)
{
	pmd_t *kernel_pmd;

	initial_kernel_pmd =
		extend_brk(sizeof(pmd_t) * PTRS_PER_PMD, PAGE_SIZE);

	max_pfn_mapped = PFN_DOWN(__pa(xen_start_info->pt_base) +
				  xen_start_info->nr_pt_frames * PAGE_SIZE +
				  512*1024);

	kernel_pmd = m2v(pgd[KERNEL_PGD_BOUNDARY].pgd);
	memcpy(initial_kernel_pmd, kernel_pmd, sizeof(pmd_t) * PTRS_PER_PMD);

	xen_map_identity_early(initial_kernel_pmd, max_pfn);

	memcpy(initial_page_table, pgd, sizeof(pgd_t) * PTRS_PER_PGD);
	initial_page_table[KERNEL_PGD_BOUNDARY] =
		__pgd(__pa(initial_kernel_pmd) | _PAGE_PRESENT);

	set_page_prot(initial_kernel_pmd, PAGE_KERNEL_RO);
	set_page_prot(initial_page_table, PAGE_KERNEL_RO);
	set_page_prot(empty_zero_page, PAGE_KERNEL_RO);

	pin_pagetable_pfn(MMUEXT_UNPIN_TABLE, PFN_DOWN(__pa(pgd)));

	pin_pagetable_pfn(MMUEXT_PIN_L3_TABLE,
			  PFN_DOWN(__pa(initial_page_table)));
	xen_write_cr3(__pa(initial_page_table));

	memblock_reserve(__pa(xen_start_info->pt_base),
			 xen_start_info->nr_pt_frames * PAGE_SIZE);

	return initial_page_table;
}
#endif	

static unsigned char dummy_mapping[PAGE_SIZE] __page_aligned_bss;
static unsigned char fake_ioapic_mapping[PAGE_SIZE] __page_aligned_bss;

static void xen_set_fixmap(unsigned idx, phys_addr_t phys, pgprot_t prot)
{
	pte_t pte;

	phys >>= PAGE_SHIFT;

	switch (idx) {
	case FIX_BTMAP_END ... FIX_BTMAP_BEGIN:
#ifdef CONFIG_X86_F00F_BUG
	case FIX_F00F_IDT:
#endif
#ifdef CONFIG_X86_32
	case FIX_WP_TEST:
	case FIX_VDSO:
# ifdef CONFIG_HIGHMEM
	case FIX_KMAP_BEGIN ... FIX_KMAP_END:
# endif
#else
	case VSYSCALL_LAST_PAGE ... VSYSCALL_FIRST_PAGE:
	case VVAR_PAGE:
#endif
	case FIX_TEXT_POKE0:
	case FIX_TEXT_POKE1:
		
		pte = pfn_pte(phys, prot);
		break;

#ifdef CONFIG_X86_LOCAL_APIC
	case FIX_APIC_BASE:	
		pte = pfn_pte(PFN_DOWN(__pa(dummy_mapping)), PAGE_KERNEL);
		break;
#endif

#ifdef CONFIG_X86_IO_APIC
	case FIX_IO_APIC_BASE_0 ... FIX_IO_APIC_BASE_END:
		pte = pfn_pte(PFN_DOWN(__pa(fake_ioapic_mapping)), PAGE_KERNEL);
		break;
#endif

	case FIX_PARAVIRT_BOOTMAP:
		pte = mfn_pte(phys, prot);
		break;

	default:
		
		pte = mfn_pte(phys, __pgprot(pgprot_val(prot) | _PAGE_IOMAP));
		break;
	}

	__native_set_fixmap(idx, pte);

#ifdef CONFIG_X86_64
	if ((idx >= VSYSCALL_LAST_PAGE && idx <= VSYSCALL_FIRST_PAGE) ||
	    idx == VVAR_PAGE) {
		unsigned long vaddr = __fix_to_virt(idx);
		set_pte_vaddr_pud(level3_user_vsyscall, vaddr, pte);
	}
#endif
}

void __init xen_ident_map_ISA(void)
{
	unsigned long pa;

	if (!xen_initial_domain())
		return;

	xen_raw_printk("Xen: setup ISA identity maps\n");

	for (pa = ISA_START_ADDRESS; pa < ISA_END_ADDRESS; pa += PAGE_SIZE) {
		pte_t pte = mfn_pte(PFN_DOWN(pa), PAGE_KERNEL_IO);

		if (HYPERVISOR_update_va_mapping(PAGE_OFFSET + pa, pte, 0))
			BUG();
	}

	xen_flush_tlb();
}

static void __init xen_post_allocator_init(void)
{
	pv_mmu_ops.set_pte = xen_set_pte;
	pv_mmu_ops.set_pmd = xen_set_pmd;
	pv_mmu_ops.set_pud = xen_set_pud;
#if PAGETABLE_LEVELS == 4
	pv_mmu_ops.set_pgd = xen_set_pgd;
#endif

	pv_mmu_ops.alloc_pte = xen_alloc_pte;
	pv_mmu_ops.alloc_pmd = xen_alloc_pmd;
	pv_mmu_ops.release_pte = xen_release_pte;
	pv_mmu_ops.release_pmd = xen_release_pmd;
#if PAGETABLE_LEVELS == 4
	pv_mmu_ops.alloc_pud = xen_alloc_pud;
	pv_mmu_ops.release_pud = xen_release_pud;
#endif

#ifdef CONFIG_X86_64
	SetPagePinned(virt_to_page(level3_user_vsyscall));
#endif
	xen_mark_init_mm_pinned();
}

static void xen_leave_lazy_mmu(void)
{
	preempt_disable();
	xen_mc_flush();
	paravirt_leave_lazy_mmu();
	preempt_enable();
}

static const struct pv_mmu_ops xen_mmu_ops __initconst = {
	.read_cr2 = xen_read_cr2,
	.write_cr2 = xen_write_cr2,

	.read_cr3 = xen_read_cr3,
#ifdef CONFIG_X86_32
	.write_cr3 = xen_write_cr3_init,
#else
	.write_cr3 = xen_write_cr3,
#endif

	.flush_tlb_user = xen_flush_tlb,
	.flush_tlb_kernel = xen_flush_tlb,
	.flush_tlb_single = xen_flush_tlb_single,
	.flush_tlb_others = xen_flush_tlb_others,

	.pte_update = paravirt_nop,
	.pte_update_defer = paravirt_nop,

	.pgd_alloc = xen_pgd_alloc,
	.pgd_free = xen_pgd_free,

	.alloc_pte = xen_alloc_pte_init,
	.release_pte = xen_release_pte_init,
	.alloc_pmd = xen_alloc_pmd_init,
	.release_pmd = xen_release_pmd_init,

	.set_pte = xen_set_pte_init,
	.set_pte_at = xen_set_pte_at,
	.set_pmd = xen_set_pmd_hyper,

	.ptep_modify_prot_start = __ptep_modify_prot_start,
	.ptep_modify_prot_commit = __ptep_modify_prot_commit,

	.pte_val = PV_CALLEE_SAVE(xen_pte_val),
	.pgd_val = PV_CALLEE_SAVE(xen_pgd_val),

	.make_pte = PV_CALLEE_SAVE(xen_make_pte),
	.make_pgd = PV_CALLEE_SAVE(xen_make_pgd),

#ifdef CONFIG_X86_PAE
	.set_pte_atomic = xen_set_pte_atomic,
	.pte_clear = xen_pte_clear,
	.pmd_clear = xen_pmd_clear,
#endif	
	.set_pud = xen_set_pud_hyper,

	.make_pmd = PV_CALLEE_SAVE(xen_make_pmd),
	.pmd_val = PV_CALLEE_SAVE(xen_pmd_val),

#if PAGETABLE_LEVELS == 4
	.pud_val = PV_CALLEE_SAVE(xen_pud_val),
	.make_pud = PV_CALLEE_SAVE(xen_make_pud),
	.set_pgd = xen_set_pgd_hyper,

	.alloc_pud = xen_alloc_pmd_init,
	.release_pud = xen_release_pmd_init,
#endif	

	.activate_mm = xen_activate_mm,
	.dup_mmap = xen_dup_mmap,
	.exit_mmap = xen_exit_mmap,

	.lazy_mode = {
		.enter = paravirt_enter_lazy_mmu,
		.leave = xen_leave_lazy_mmu,
	},

	.set_fixmap = xen_set_fixmap,
};

void __init xen_init_mmu_ops(void)
{
	x86_init.mapping.pagetable_reserve = xen_mapping_pagetable_reserve;
	x86_init.paging.pagetable_setup_start = xen_pagetable_setup_start;
	x86_init.paging.pagetable_setup_done = xen_pagetable_setup_done;
	pv_mmu_ops = xen_mmu_ops;

	memset(dummy_mapping, 0xff, PAGE_SIZE);
	memset(fake_ioapic_mapping, 0xfd, PAGE_SIZE);
}

#define MAX_CONTIG_ORDER 9 
static unsigned long discontig_frames[1<<MAX_CONTIG_ORDER];

#define VOID_PTE (mfn_pte(0, __pgprot(0)))
static void xen_zap_pfn_range(unsigned long vaddr, unsigned int order,
				unsigned long *in_frames,
				unsigned long *out_frames)
{
	int i;
	struct multicall_space mcs;

	xen_mc_batch();
	for (i = 0; i < (1UL<<order); i++, vaddr += PAGE_SIZE) {
		mcs = __xen_mc_entry(0);

		if (in_frames)
			in_frames[i] = virt_to_mfn(vaddr);

		MULTI_update_va_mapping(mcs.mc, vaddr, VOID_PTE, 0);
		__set_phys_to_machine(virt_to_pfn(vaddr), INVALID_P2M_ENTRY);

		if (out_frames)
			out_frames[i] = virt_to_pfn(vaddr);
	}
	xen_mc_issue(0);
}

static void xen_remap_exchanged_ptes(unsigned long vaddr, int order,
				     unsigned long *mfns,
				     unsigned long first_mfn)
{
	unsigned i, limit;
	unsigned long mfn;

	xen_mc_batch();

	limit = 1u << order;
	for (i = 0; i < limit; i++, vaddr += PAGE_SIZE) {
		struct multicall_space mcs;
		unsigned flags;

		mcs = __xen_mc_entry(0);
		if (mfns)
			mfn = mfns[i];
		else
			mfn = first_mfn + i;

		if (i < (limit - 1))
			flags = 0;
		else {
			if (order == 0)
				flags = UVMF_INVLPG | UVMF_ALL;
			else
				flags = UVMF_TLB_FLUSH | UVMF_ALL;
		}

		MULTI_update_va_mapping(mcs.mc, vaddr,
				mfn_pte(mfn, PAGE_KERNEL), flags);

		set_phys_to_machine(virt_to_pfn(vaddr), mfn);
	}

	xen_mc_issue(0);
}

static int xen_exchange_memory(unsigned long extents_in, unsigned int order_in,
			       unsigned long *pfns_in,
			       unsigned long extents_out,
			       unsigned int order_out,
			       unsigned long *mfns_out,
			       unsigned int address_bits)
{
	long rc;
	int success;

	struct xen_memory_exchange exchange = {
		.in = {
			.nr_extents   = extents_in,
			.extent_order = order_in,
			.extent_start = pfns_in,
			.domid        = DOMID_SELF
		},
		.out = {
			.nr_extents   = extents_out,
			.extent_order = order_out,
			.extent_start = mfns_out,
			.address_bits = address_bits,
			.domid        = DOMID_SELF
		}
	};

	BUG_ON(extents_in << order_in != extents_out << order_out);

	rc = HYPERVISOR_memory_op(XENMEM_exchange, &exchange);
	success = (exchange.nr_exchanged == extents_in);

	BUG_ON(!success && ((exchange.nr_exchanged != 0) || (rc == 0)));
	BUG_ON(success && (rc != 0));

	return success;
}

int xen_create_contiguous_region(unsigned long vstart, unsigned int order,
				 unsigned int address_bits)
{
	unsigned long *in_frames = discontig_frames, out_frame;
	unsigned long  flags;
	int            success;


	if (xen_feature(XENFEAT_auto_translated_physmap))
		return 0;

	if (unlikely(order > MAX_CONTIG_ORDER))
		return -ENOMEM;

	memset((void *) vstart, 0, PAGE_SIZE << order);

	spin_lock_irqsave(&xen_reservation_lock, flags);

	
	xen_zap_pfn_range(vstart, order, in_frames, NULL);

	
	out_frame = virt_to_pfn(vstart);
	success = xen_exchange_memory(1UL << order, 0, in_frames,
				      1, order, &out_frame,
				      address_bits);

	
	if (success)
		xen_remap_exchanged_ptes(vstart, order, NULL, out_frame);
	else
		xen_remap_exchanged_ptes(vstart, order, in_frames, 0);

	spin_unlock_irqrestore(&xen_reservation_lock, flags);

	return success ? 0 : -ENOMEM;
}
EXPORT_SYMBOL_GPL(xen_create_contiguous_region);

void xen_destroy_contiguous_region(unsigned long vstart, unsigned int order)
{
	unsigned long *out_frames = discontig_frames, in_frame;
	unsigned long  flags;
	int success;

	if (xen_feature(XENFEAT_auto_translated_physmap))
		return;

	if (unlikely(order > MAX_CONTIG_ORDER))
		return;

	memset((void *) vstart, 0, PAGE_SIZE << order);

	spin_lock_irqsave(&xen_reservation_lock, flags);

	
	in_frame = virt_to_mfn(vstart);

	
	xen_zap_pfn_range(vstart, order, NULL, out_frames);

	
	success = xen_exchange_memory(1, order, &in_frame, 1UL << order,
					0, out_frames, 0);

	
	if (success)
		xen_remap_exchanged_ptes(vstart, order, out_frames, 0);
	else
		xen_remap_exchanged_ptes(vstart, order, NULL, in_frame);

	spin_unlock_irqrestore(&xen_reservation_lock, flags);
}
EXPORT_SYMBOL_GPL(xen_destroy_contiguous_region);

#ifdef CONFIG_XEN_PVHVM
static void xen_hvm_exit_mmap(struct mm_struct *mm)
{
	struct xen_hvm_pagetable_dying a;
	int rc;

	a.domid = DOMID_SELF;
	a.gpa = __pa(mm->pgd);
	rc = HYPERVISOR_hvm_op(HVMOP_pagetable_dying, &a);
	WARN_ON_ONCE(rc < 0);
}

static int is_pagetable_dying_supported(void)
{
	struct xen_hvm_pagetable_dying a;
	int rc = 0;

	a.domid = DOMID_SELF;
	a.gpa = 0x00;
	rc = HYPERVISOR_hvm_op(HVMOP_pagetable_dying, &a);
	if (rc < 0) {
		printk(KERN_DEBUG "HVMOP_pagetable_dying not supported\n");
		return 0;
	}
	return 1;
}

void __init xen_hvm_init_mmu_ops(void)
{
	if (is_pagetable_dying_supported())
		pv_mmu_ops.exit_mmap = xen_hvm_exit_mmap;
}
#endif

#define REMAP_BATCH_SIZE 16

struct remap_data {
	unsigned long mfn;
	pgprot_t prot;
	struct mmu_update *mmu_update;
};

static int remap_area_mfn_pte_fn(pte_t *ptep, pgtable_t token,
				 unsigned long addr, void *data)
{
	struct remap_data *rmd = data;
	pte_t pte = pte_mkspecial(pfn_pte(rmd->mfn++, rmd->prot));

	rmd->mmu_update->ptr = virt_to_machine(ptep).maddr;
	rmd->mmu_update->val = pte_val_ma(pte);
	rmd->mmu_update++;

	return 0;
}

int xen_remap_domain_mfn_range(struct vm_area_struct *vma,
			       unsigned long addr,
			       unsigned long mfn, int nr,
			       pgprot_t prot, unsigned domid)
{
	struct remap_data rmd;
	struct mmu_update mmu_update[REMAP_BATCH_SIZE];
	int batch;
	unsigned long range;
	int err = 0;

	prot = __pgprot(pgprot_val(prot) | _PAGE_IOMAP);

	BUG_ON(!((vma->vm_flags & (VM_PFNMAP | VM_RESERVED | VM_IO)) ==
				(VM_PFNMAP | VM_RESERVED | VM_IO)));

	rmd.mfn = mfn;
	rmd.prot = prot;

	while (nr) {
		batch = min(REMAP_BATCH_SIZE, nr);
		range = (unsigned long)batch << PAGE_SHIFT;

		rmd.mmu_update = mmu_update;
		err = apply_to_page_range(vma->vm_mm, addr, range,
					  remap_area_mfn_pte_fn, &rmd);
		if (err)
			goto out;

		err = -EFAULT;
		if (HYPERVISOR_mmu_update(mmu_update, batch, NULL, domid) < 0)
			goto out;

		nr -= batch;
		addr += range;
	}

	err = 0;
out:

	flush_tlb_all();

	return err;
}
EXPORT_SYMBOL_GPL(xen_remap_domain_mfn_range);
