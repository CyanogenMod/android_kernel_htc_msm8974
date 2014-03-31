#ifndef _ASM_X86_XEN_PAGE_H
#define _ASM_X86_XEN_PAGE_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/pfn.h>
#include <linux/mm.h>

#include <asm/uaccess.h>
#include <asm/page.h>
#include <asm/pgtable.h>

#include <xen/interface/xen.h>
#include <xen/grant_table.h>
#include <xen/features.h>

typedef struct xmaddr {
	phys_addr_t maddr;
} xmaddr_t;

typedef struct xpaddr {
	phys_addr_t paddr;
} xpaddr_t;

#define XMADDR(x)	((xmaddr_t) { .maddr = (x) })
#define XPADDR(x)	((xpaddr_t) { .paddr = (x) })

#define INVALID_P2M_ENTRY	(~0UL)
#define FOREIGN_FRAME_BIT	(1UL<<(BITS_PER_LONG-1))
#define IDENTITY_FRAME_BIT	(1UL<<(BITS_PER_LONG-2))
#define FOREIGN_FRAME(m)	((m) | FOREIGN_FRAME_BIT)
#define IDENTITY_FRAME(m)	((m) | IDENTITY_FRAME_BIT)

#define MAX_DOMAIN_PAGES						\
    ((unsigned long)((u64)CONFIG_XEN_MAX_DOMAIN_MEMORY * 1024 * 1024 * 1024 / PAGE_SIZE))

extern unsigned long *machine_to_phys_mapping;
extern unsigned long  machine_to_phys_nr;

extern unsigned long get_phys_to_machine(unsigned long pfn);
extern bool set_phys_to_machine(unsigned long pfn, unsigned long mfn);
extern bool __set_phys_to_machine(unsigned long pfn, unsigned long mfn);
extern unsigned long set_phys_range_identity(unsigned long pfn_s,
					     unsigned long pfn_e);

extern int m2p_add_override(unsigned long mfn, struct page *page,
			    struct gnttab_map_grant_ref *kmap_op);
extern int m2p_remove_override(struct page *page, bool clear_pte);
extern struct page *m2p_find_override(unsigned long mfn);
extern unsigned long m2p_find_override_pfn(unsigned long mfn, unsigned long pfn);

static inline unsigned long pfn_to_mfn(unsigned long pfn)
{
	unsigned long mfn;

	if (xen_feature(XENFEAT_auto_translated_physmap))
		return pfn;

	mfn = get_phys_to_machine(pfn);

	if (mfn != INVALID_P2M_ENTRY)
		mfn &= ~(FOREIGN_FRAME_BIT | IDENTITY_FRAME_BIT);

	return mfn;
}

static inline int phys_to_machine_mapping_valid(unsigned long pfn)
{
	if (xen_feature(XENFEAT_auto_translated_physmap))
		return 1;

	return get_phys_to_machine(pfn) != INVALID_P2M_ENTRY;
}

static inline unsigned long mfn_to_pfn(unsigned long mfn)
{
	unsigned long pfn;
	int ret = 0;

	if (xen_feature(XENFEAT_auto_translated_physmap))
		return mfn;

	if (unlikely(mfn >= machine_to_phys_nr)) {
		pfn = ~0;
		goto try_override;
	}
	pfn = 0;
	ret = __get_user(pfn, &machine_to_phys_mapping[mfn]);
try_override:
	
	if (ret < 0)
		pfn = ~0;
	else if (get_phys_to_machine(pfn) != mfn)
		pfn = m2p_find_override_pfn(mfn, ~0);

	if (pfn == ~0 &&
			get_phys_to_machine(mfn) == IDENTITY_FRAME(mfn))
		pfn = mfn;

	return pfn;
}

static inline xmaddr_t phys_to_machine(xpaddr_t phys)
{
	unsigned offset = phys.paddr & ~PAGE_MASK;
	return XMADDR(PFN_PHYS(pfn_to_mfn(PFN_DOWN(phys.paddr))) | offset);
}

static inline xpaddr_t machine_to_phys(xmaddr_t machine)
{
	unsigned offset = machine.maddr & ~PAGE_MASK;
	return XPADDR(PFN_PHYS(mfn_to_pfn(PFN_DOWN(machine.maddr))) | offset);
}

static inline unsigned long mfn_to_local_pfn(unsigned long mfn)
{
	unsigned long pfn = mfn_to_pfn(mfn);
	if (get_phys_to_machine(pfn) != mfn)
		return -1; 
	return pfn;
}

#define virt_to_machine(v)	(phys_to_machine(XPADDR(__pa(v))))
#define virt_to_pfn(v)          (PFN_DOWN(__pa(v)))
#define virt_to_mfn(v)		(pfn_to_mfn(virt_to_pfn(v)))
#define mfn_to_virt(m)		(__va(mfn_to_pfn(m) << PAGE_SHIFT))

static inline unsigned long pte_mfn(pte_t pte)
{
	return (pte.pte & PTE_PFN_MASK) >> PAGE_SHIFT;
}

static inline pte_t mfn_pte(unsigned long page_nr, pgprot_t pgprot)
{
	pte_t pte;

	pte.pte = ((phys_addr_t)page_nr << PAGE_SHIFT) |
			massage_pgprot(pgprot);

	return pte;
}

static inline pteval_t pte_val_ma(pte_t pte)
{
	return pte.pte;
}

static inline pte_t __pte_ma(pteval_t x)
{
	return (pte_t) { .pte = x };
}

#define pmd_val_ma(v) ((v).pmd)
#ifdef __PAGETABLE_PUD_FOLDED
#define pud_val_ma(v) ((v).pgd.pgd)
#else
#define pud_val_ma(v) ((v).pud)
#endif
#define __pmd_ma(x)	((pmd_t) { (x) } )

#define pgd_val_ma(x)	((x).pgd)

void xen_set_domain_pte(pte_t *ptep, pte_t pteval, unsigned domid);

xmaddr_t arbitrary_virt_to_machine(void *address);
unsigned long arbitrary_virt_to_mfn(void *vaddr);
void make_lowmem_page_readonly(void *vaddr);
void make_lowmem_page_readwrite(void *vaddr);

#endif 
