
#include <linux/init.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/hash.h>
#include <linux/sched.h>
#include <linux/seq_file.h>

#include <asm/cache.h>
#include <asm/setup.h>

#include <asm/xen/page.h>
#include <asm/xen/hypercall.h>
#include <asm/xen/hypervisor.h>
#include <xen/grant_table.h>

#include "multicalls.h"
#include "xen-ops.h"

static void __init m2p_override_init(void);

unsigned long xen_max_p2m_pfn __read_mostly;

#define P2M_PER_PAGE		(PAGE_SIZE / sizeof(unsigned long))
#define P2M_MID_PER_PAGE	(PAGE_SIZE / sizeof(unsigned long *))
#define P2M_TOP_PER_PAGE	(PAGE_SIZE / sizeof(unsigned long **))

#define MAX_P2M_PFN		(P2M_TOP_PER_PAGE * P2M_MID_PER_PAGE * P2M_PER_PAGE)

static RESERVE_BRK_ARRAY(unsigned long, p2m_missing, P2M_PER_PAGE);
static RESERVE_BRK_ARRAY(unsigned long *, p2m_mid_missing, P2M_MID_PER_PAGE);
static RESERVE_BRK_ARRAY(unsigned long, p2m_mid_missing_mfn, P2M_MID_PER_PAGE);

static RESERVE_BRK_ARRAY(unsigned long **, p2m_top, P2M_TOP_PER_PAGE);
static RESERVE_BRK_ARRAY(unsigned long, p2m_top_mfn, P2M_TOP_PER_PAGE);
static RESERVE_BRK_ARRAY(unsigned long *, p2m_top_mfn_p, P2M_TOP_PER_PAGE);

static RESERVE_BRK_ARRAY(unsigned long, p2m_identity, P2M_PER_PAGE);

RESERVE_BRK(p2m_mid, PAGE_SIZE * (MAX_DOMAIN_PAGES / (P2M_PER_PAGE * P2M_MID_PER_PAGE)));
RESERVE_BRK(p2m_mid_mfn, PAGE_SIZE * (MAX_DOMAIN_PAGES / (P2M_PER_PAGE * P2M_MID_PER_PAGE)));

RESERVE_BRK(p2m_mid_identity, PAGE_SIZE * 2 * 3);

static inline unsigned p2m_top_index(unsigned long pfn)
{
	BUG_ON(pfn >= MAX_P2M_PFN);
	return pfn / (P2M_MID_PER_PAGE * P2M_PER_PAGE);
}

static inline unsigned p2m_mid_index(unsigned long pfn)
{
	return (pfn / P2M_PER_PAGE) % P2M_MID_PER_PAGE;
}

static inline unsigned p2m_index(unsigned long pfn)
{
	return pfn % P2M_PER_PAGE;
}

static void p2m_top_init(unsigned long ***top)
{
	unsigned i;

	for (i = 0; i < P2M_TOP_PER_PAGE; i++)
		top[i] = p2m_mid_missing;
}

static void p2m_top_mfn_init(unsigned long *top)
{
	unsigned i;

	for (i = 0; i < P2M_TOP_PER_PAGE; i++)
		top[i] = virt_to_mfn(p2m_mid_missing_mfn);
}

static void p2m_top_mfn_p_init(unsigned long **top)
{
	unsigned i;

	for (i = 0; i < P2M_TOP_PER_PAGE; i++)
		top[i] = p2m_mid_missing_mfn;
}

static void p2m_mid_init(unsigned long **mid)
{
	unsigned i;

	for (i = 0; i < P2M_MID_PER_PAGE; i++)
		mid[i] = p2m_missing;
}

static void p2m_mid_mfn_init(unsigned long *mid)
{
	unsigned i;

	for (i = 0; i < P2M_MID_PER_PAGE; i++)
		mid[i] = virt_to_mfn(p2m_missing);
}

static void p2m_init(unsigned long *p2m)
{
	unsigned i;

	for (i = 0; i < P2M_MID_PER_PAGE; i++)
		p2m[i] = INVALID_P2M_ENTRY;
}

void __ref xen_build_mfn_list_list(void)
{
	unsigned long pfn;

	
	if (p2m_top_mfn == NULL) {
		p2m_mid_missing_mfn = extend_brk(PAGE_SIZE, PAGE_SIZE);
		p2m_mid_mfn_init(p2m_mid_missing_mfn);

		p2m_top_mfn_p = extend_brk(PAGE_SIZE, PAGE_SIZE);
		p2m_top_mfn_p_init(p2m_top_mfn_p);

		p2m_top_mfn = extend_brk(PAGE_SIZE, PAGE_SIZE);
		p2m_top_mfn_init(p2m_top_mfn);
	} else {
		
		p2m_mid_mfn_init(p2m_mid_missing_mfn);
	}

	for (pfn = 0; pfn < xen_max_p2m_pfn; pfn += P2M_PER_PAGE) {
		unsigned topidx = p2m_top_index(pfn);
		unsigned mididx = p2m_mid_index(pfn);
		unsigned long **mid;
		unsigned long *mid_mfn_p;

		mid = p2m_top[topidx];
		mid_mfn_p = p2m_top_mfn_p[topidx];

		if (mid == p2m_mid_missing) {
			BUG_ON(mididx);
			BUG_ON(mid_mfn_p != p2m_mid_missing_mfn);
			p2m_top_mfn[topidx] = virt_to_mfn(p2m_mid_missing_mfn);
			pfn += (P2M_MID_PER_PAGE - 1) * P2M_PER_PAGE;
			continue;
		}

		if (mid_mfn_p == p2m_mid_missing_mfn) {
			mid_mfn_p = extend_brk(PAGE_SIZE, PAGE_SIZE);
			p2m_mid_mfn_init(mid_mfn_p);

			p2m_top_mfn_p[topidx] = mid_mfn_p;
		}

		p2m_top_mfn[topidx] = virt_to_mfn(mid_mfn_p);
		mid_mfn_p[mididx] = virt_to_mfn(mid[mididx]);
	}
}

void xen_setup_mfn_list_list(void)
{
	BUG_ON(HYPERVISOR_shared_info == &xen_dummy_shared_info);

	HYPERVISOR_shared_info->arch.pfn_to_mfn_frame_list_list =
		virt_to_mfn(p2m_top_mfn);
	HYPERVISOR_shared_info->arch.max_pfn = xen_max_p2m_pfn;
}

void __init xen_build_dynamic_phys_to_machine(void)
{
	unsigned long *mfn_list = (unsigned long *)xen_start_info->mfn_list;
	unsigned long max_pfn = min(MAX_DOMAIN_PAGES, xen_start_info->nr_pages);
	unsigned long pfn;

	xen_max_p2m_pfn = max_pfn;

	p2m_missing = extend_brk(PAGE_SIZE, PAGE_SIZE);
	p2m_init(p2m_missing);

	p2m_mid_missing = extend_brk(PAGE_SIZE, PAGE_SIZE);
	p2m_mid_init(p2m_mid_missing);

	p2m_top = extend_brk(PAGE_SIZE, PAGE_SIZE);
	p2m_top_init(p2m_top);

	p2m_identity = extend_brk(PAGE_SIZE, PAGE_SIZE);
	p2m_init(p2m_identity);

	for (pfn = 0; pfn < max_pfn; pfn += P2M_PER_PAGE) {
		unsigned topidx = p2m_top_index(pfn);
		unsigned mididx = p2m_mid_index(pfn);

		if (p2m_top[topidx] == p2m_mid_missing) {
			unsigned long **mid = extend_brk(PAGE_SIZE, PAGE_SIZE);
			p2m_mid_init(mid);

			p2m_top[topidx] = mid;
		}

		if (unlikely(pfn + P2M_PER_PAGE > max_pfn)) {
			unsigned long p2midx;

			p2midx = max_pfn % P2M_PER_PAGE;
			for ( ; p2midx < P2M_PER_PAGE; p2midx++)
				mfn_list[pfn + p2midx] = INVALID_P2M_ENTRY;
		}
		p2m_top[topidx][mididx] = &mfn_list[pfn];
	}

	m2p_override_init();
}

unsigned long get_phys_to_machine(unsigned long pfn)
{
	unsigned topidx, mididx, idx;

	if (unlikely(pfn >= MAX_P2M_PFN))
		return INVALID_P2M_ENTRY;

	topidx = p2m_top_index(pfn);
	mididx = p2m_mid_index(pfn);
	idx = p2m_index(pfn);

	if (p2m_top[topidx][mididx] == p2m_identity)
		return IDENTITY_FRAME(pfn);

	return p2m_top[topidx][mididx][idx];
}
EXPORT_SYMBOL_GPL(get_phys_to_machine);

static void *alloc_p2m_page(void)
{
	return (void *)__get_free_page(GFP_KERNEL | __GFP_REPEAT);
}

static void free_p2m_page(void *p)
{
	free_page((unsigned long)p);
}

static bool alloc_p2m(unsigned long pfn)
{
	unsigned topidx, mididx;
	unsigned long ***top_p, **mid;
	unsigned long *top_mfn_p, *mid_mfn;

	topidx = p2m_top_index(pfn);
	mididx = p2m_mid_index(pfn);

	top_p = &p2m_top[topidx];
	mid = *top_p;

	if (mid == p2m_mid_missing) {
		
		mid = alloc_p2m_page();
		if (!mid)
			return false;

		p2m_mid_init(mid);

		if (cmpxchg(top_p, p2m_mid_missing, mid) != p2m_mid_missing)
			free_p2m_page(mid);
	}

	top_mfn_p = &p2m_top_mfn[topidx];
	mid_mfn = p2m_top_mfn_p[topidx];

	BUG_ON(virt_to_mfn(mid_mfn) != *top_mfn_p);

	if (mid_mfn == p2m_mid_missing_mfn) {
		
		unsigned long missing_mfn;
		unsigned long mid_mfn_mfn;

		mid_mfn = alloc_p2m_page();
		if (!mid_mfn)
			return false;

		p2m_mid_mfn_init(mid_mfn);

		missing_mfn = virt_to_mfn(p2m_mid_missing_mfn);
		mid_mfn_mfn = virt_to_mfn(mid_mfn);
		if (cmpxchg(top_mfn_p, missing_mfn, mid_mfn_mfn) != missing_mfn)
			free_p2m_page(mid_mfn);
		else
			p2m_top_mfn_p[topidx] = mid_mfn;
	}

	if (p2m_top[topidx][mididx] == p2m_identity ||
	    p2m_top[topidx][mididx] == p2m_missing) {
		
		unsigned long *p2m;
		unsigned long *p2m_orig = p2m_top[topidx][mididx];

		p2m = alloc_p2m_page();
		if (!p2m)
			return false;

		p2m_init(p2m);

		if (cmpxchg(&mid[mididx], p2m_orig, p2m) != p2m_orig)
			free_p2m_page(p2m);
		else
			mid_mfn[mididx] = virt_to_mfn(p2m);
	}

	return true;
}

static bool __init __early_alloc_p2m(unsigned long pfn)
{
	unsigned topidx, mididx, idx;

	topidx = p2m_top_index(pfn);
	mididx = p2m_mid_index(pfn);
	idx = p2m_index(pfn);

	
	if (!idx)
		return false;

	WARN(p2m_top[topidx][mididx] == p2m_identity,
		"P2M[%d][%d] == IDENTITY, should be MISSING (or alloced)!\n",
		topidx, mididx);

	if (p2m_top[topidx][mididx] != p2m_missing)
		return false;

	
	if (idx) {
		unsigned long *p2m = extend_brk(PAGE_SIZE, PAGE_SIZE);
		unsigned long *mid_mfn_p;

		p2m_init(p2m);

		p2m_top[topidx][mididx] = p2m;

		
		
		mid_mfn_p = p2m_top_mfn_p[topidx];
		WARN(mid_mfn_p[mididx] != virt_to_mfn(p2m_missing),
			"P2M_TOP_P[%d][%d] != MFN of p2m_missing!\n",
			topidx, mididx);
		mid_mfn_p[mididx] = virt_to_mfn(p2m);

	}
	return idx != 0;
}
unsigned long __init set_phys_range_identity(unsigned long pfn_s,
				      unsigned long pfn_e)
{
	unsigned long pfn;

	if (unlikely(pfn_s >= MAX_P2M_PFN || pfn_e >= MAX_P2M_PFN))
		return 0;

	if (unlikely(xen_feature(XENFEAT_auto_translated_physmap)))
		return pfn_e - pfn_s;

	if (pfn_s > pfn_e)
		return 0;

	for (pfn = (pfn_s & ~(P2M_MID_PER_PAGE * P2M_PER_PAGE - 1));
		pfn < ALIGN(pfn_e, (P2M_MID_PER_PAGE * P2M_PER_PAGE));
		pfn += P2M_MID_PER_PAGE * P2M_PER_PAGE)
	{
		unsigned topidx = p2m_top_index(pfn);
		unsigned long *mid_mfn_p;
		unsigned long **mid;

		mid = p2m_top[topidx];
		mid_mfn_p = p2m_top_mfn_p[topidx];
		if (mid == p2m_mid_missing) {
			mid = extend_brk(PAGE_SIZE, PAGE_SIZE);

			p2m_mid_init(mid);

			p2m_top[topidx] = mid;

			BUG_ON(mid_mfn_p != p2m_mid_missing_mfn);
		}
		
		if (mid_mfn_p == p2m_mid_missing_mfn) {
			mid_mfn_p = extend_brk(PAGE_SIZE, PAGE_SIZE);
			p2m_mid_mfn_init(mid_mfn_p);

			p2m_top_mfn_p[topidx] = mid_mfn_p;
			p2m_top_mfn[topidx] = virt_to_mfn(mid_mfn_p);
		}
	}

	__early_alloc_p2m(pfn_s);
	__early_alloc_p2m(pfn_e);

	for (pfn = pfn_s; pfn < pfn_e; pfn++)
		if (!__set_phys_to_machine(pfn, IDENTITY_FRAME(pfn)))
			break;

	if (!WARN((pfn - pfn_s) != (pfn_e - pfn_s),
		"Identity mapping failed. We are %ld short of 1-1 mappings!\n",
		(pfn_e - pfn_s) - (pfn - pfn_s)))
		printk(KERN_DEBUG "1-1 mapping on %lx->%lx\n", pfn_s, pfn);

	return pfn - pfn_s;
}

bool __set_phys_to_machine(unsigned long pfn, unsigned long mfn)
{
	unsigned topidx, mididx, idx;

	if (unlikely(xen_feature(XENFEAT_auto_translated_physmap))) {
		BUG_ON(pfn != mfn && mfn != INVALID_P2M_ENTRY);
		return true;
	}
	if (unlikely(pfn >= MAX_P2M_PFN)) {
		BUG_ON(mfn != INVALID_P2M_ENTRY);
		return true;
	}

	topidx = p2m_top_index(pfn);
	mididx = p2m_mid_index(pfn);
	idx = p2m_index(pfn);

	if (mfn != INVALID_P2M_ENTRY && (mfn & IDENTITY_FRAME_BIT)) {
		if (p2m_top[topidx][mididx] == p2m_identity)
			return true;

		
		if (p2m_top[topidx][mididx] == p2m_missing) {
			WARN_ON(cmpxchg(&p2m_top[topidx][mididx], p2m_missing,
				p2m_identity) != p2m_missing);
			return true;
		}
	}

	if (p2m_top[topidx][mididx] == p2m_missing)
		return mfn == INVALID_P2M_ENTRY;

	p2m_top[topidx][mididx][idx] = mfn;

	return true;
}

bool set_phys_to_machine(unsigned long pfn, unsigned long mfn)
{
	if (unlikely(!__set_phys_to_machine(pfn, mfn)))  {
		if (!alloc_p2m(pfn))
			return false;

		if (!__set_phys_to_machine(pfn, mfn))
			return false;
	}

	return true;
}

#define M2P_OVERRIDE_HASH_SHIFT	10
#define M2P_OVERRIDE_HASH	(1 << M2P_OVERRIDE_HASH_SHIFT)

static RESERVE_BRK_ARRAY(struct list_head, m2p_overrides, M2P_OVERRIDE_HASH);
static DEFINE_SPINLOCK(m2p_override_lock);

static void __init m2p_override_init(void)
{
	unsigned i;

	m2p_overrides = extend_brk(sizeof(*m2p_overrides) * M2P_OVERRIDE_HASH,
				   sizeof(unsigned long));

	for (i = 0; i < M2P_OVERRIDE_HASH; i++)
		INIT_LIST_HEAD(&m2p_overrides[i]);
}

static unsigned long mfn_hash(unsigned long mfn)
{
	return hash_long(mfn, M2P_OVERRIDE_HASH_SHIFT);
}

int m2p_add_override(unsigned long mfn, struct page *page,
		struct gnttab_map_grant_ref *kmap_op)
{
	unsigned long flags;
	unsigned long pfn;
	unsigned long uninitialized_var(address);
	unsigned level;
	pte_t *ptep = NULL;

	pfn = page_to_pfn(page);
	if (!PageHighMem(page)) {
		address = (unsigned long)__va(pfn << PAGE_SHIFT);
		ptep = lookup_address(address, &level);
		if (WARN(ptep == NULL || level != PG_LEVEL_4K,
					"m2p_add_override: pfn %lx not mapped", pfn))
			return -EINVAL;
	}
	WARN_ON(PagePrivate(page));
	SetPagePrivate(page);
	set_page_private(page, mfn);
	page->index = pfn_to_mfn(pfn);

	if (unlikely(!set_phys_to_machine(pfn, FOREIGN_FRAME(mfn))))
		return -ENOMEM;

	if (kmap_op != NULL) {
		if (!PageHighMem(page)) {
			struct multicall_space mcs =
				xen_mc_entry(sizeof(*kmap_op));

			MULTI_grant_table_op(mcs.mc,
					GNTTABOP_map_grant_ref, kmap_op, 1);

			xen_mc_issue(PARAVIRT_LAZY_MMU);
		}
		
		kmap_op->dev_bus_addr = page->index;
		page->index = (unsigned long) kmap_op;
	}
	spin_lock_irqsave(&m2p_override_lock, flags);
	list_add(&page->lru,  &m2p_overrides[mfn_hash(mfn)]);
	spin_unlock_irqrestore(&m2p_override_lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(m2p_add_override);
int m2p_remove_override(struct page *page, bool clear_pte)
{
	unsigned long flags;
	unsigned long mfn;
	unsigned long pfn;
	unsigned long uninitialized_var(address);
	unsigned level;
	pte_t *ptep = NULL;

	pfn = page_to_pfn(page);
	mfn = get_phys_to_machine(pfn);
	if (mfn == INVALID_P2M_ENTRY || !(mfn & FOREIGN_FRAME_BIT))
		return -EINVAL;

	if (!PageHighMem(page)) {
		address = (unsigned long)__va(pfn << PAGE_SHIFT);
		ptep = lookup_address(address, &level);

		if (WARN(ptep == NULL || level != PG_LEVEL_4K,
					"m2p_remove_override: pfn %lx not mapped", pfn))
			return -EINVAL;
	}

	spin_lock_irqsave(&m2p_override_lock, flags);
	list_del(&page->lru);
	spin_unlock_irqrestore(&m2p_override_lock, flags);
	WARN_ON(!PagePrivate(page));
	ClearPagePrivate(page);

	if (clear_pte) {
		struct gnttab_map_grant_ref *map_op =
			(struct gnttab_map_grant_ref *) page->index;
		set_phys_to_machine(pfn, map_op->dev_bus_addr);
		if (!PageHighMem(page)) {
			struct multicall_space mcs;
			struct gnttab_unmap_grant_ref *unmap_op;

			if (map_op->handle == -1)
				xen_mc_flush();
			if (map_op->handle == GNTST_general_error) {
				printk(KERN_WARNING "m2p_remove_override: "
						"pfn %lx mfn %lx, failed to modify kernel mappings",
						pfn, mfn);
				return -1;
			}

			mcs = xen_mc_entry(
					sizeof(struct gnttab_unmap_grant_ref));
			unmap_op = mcs.args;
			unmap_op->host_addr = map_op->host_addr;
			unmap_op->handle = map_op->handle;
			unmap_op->dev_bus_addr = 0;

			MULTI_grant_table_op(mcs.mc,
					GNTTABOP_unmap_grant_ref, unmap_op, 1);

			xen_mc_issue(PARAVIRT_LAZY_MMU);

			set_pte_at(&init_mm, address, ptep,
					pfn_pte(pfn, PAGE_KERNEL));
			__flush_tlb_single(address);
			map_op->host_addr = 0;
		}
	} else
		set_phys_to_machine(pfn, page->index);

	return 0;
}
EXPORT_SYMBOL_GPL(m2p_remove_override);

struct page *m2p_find_override(unsigned long mfn)
{
	unsigned long flags;
	struct list_head *bucket = &m2p_overrides[mfn_hash(mfn)];
	struct page *p, *ret;

	ret = NULL;

	spin_lock_irqsave(&m2p_override_lock, flags);

	list_for_each_entry(p, bucket, lru) {
		if (page_private(p) == mfn) {
			ret = p;
			break;
		}
	}

	spin_unlock_irqrestore(&m2p_override_lock, flags);

	return ret;
}

unsigned long m2p_find_override_pfn(unsigned long mfn, unsigned long pfn)
{
	struct page *p = m2p_find_override(mfn);
	unsigned long ret = pfn;

	if (p)
		ret = page_to_pfn(p);

	return ret;
}
EXPORT_SYMBOL_GPL(m2p_find_override_pfn);

#ifdef CONFIG_XEN_DEBUG_FS
#include <linux/debugfs.h>
#include "debugfs.h"
static int p2m_dump_show(struct seq_file *m, void *v)
{
	static const char * const level_name[] = { "top", "middle",
						"entry", "abnormal", "error"};
#define TYPE_IDENTITY 0
#define TYPE_MISSING 1
#define TYPE_PFN 2
#define TYPE_UNKNOWN 3
	static const char * const type_name[] = {
				[TYPE_IDENTITY] = "identity",
				[TYPE_MISSING] = "missing",
				[TYPE_PFN] = "pfn",
				[TYPE_UNKNOWN] = "abnormal"};
	unsigned long pfn, prev_pfn_type = 0, prev_pfn_level = 0;
	unsigned int uninitialized_var(prev_level);
	unsigned int uninitialized_var(prev_type);

	if (!p2m_top)
		return 0;

	for (pfn = 0; pfn < MAX_DOMAIN_PAGES; pfn++) {
		unsigned topidx = p2m_top_index(pfn);
		unsigned mididx = p2m_mid_index(pfn);
		unsigned idx = p2m_index(pfn);
		unsigned lvl, type;

		lvl = 4;
		type = TYPE_UNKNOWN;
		if (p2m_top[topidx] == p2m_mid_missing) {
			lvl = 0; type = TYPE_MISSING;
		} else if (p2m_top[topidx] == NULL) {
			lvl = 0; type = TYPE_UNKNOWN;
		} else if (p2m_top[topidx][mididx] == NULL) {
			lvl = 1; type = TYPE_UNKNOWN;
		} else if (p2m_top[topidx][mididx] == p2m_identity) {
			lvl = 1; type = TYPE_IDENTITY;
		} else if (p2m_top[topidx][mididx] == p2m_missing) {
			lvl = 1; type = TYPE_MISSING;
		} else if (p2m_top[topidx][mididx][idx] == 0) {
			lvl = 2; type = TYPE_UNKNOWN;
		} else if (p2m_top[topidx][mididx][idx] == IDENTITY_FRAME(pfn)) {
			lvl = 2; type = TYPE_IDENTITY;
		} else if (p2m_top[topidx][mididx][idx] == INVALID_P2M_ENTRY) {
			lvl = 2; type = TYPE_MISSING;
		} else if (p2m_top[topidx][mididx][idx] == pfn) {
			lvl = 2; type = TYPE_PFN;
		} else if (p2m_top[topidx][mididx][idx] != pfn) {
			lvl = 2; type = TYPE_PFN;
		}
		if (pfn == 0) {
			prev_level = lvl;
			prev_type = type;
		}
		if (pfn == MAX_DOMAIN_PAGES-1) {
			lvl = 3;
			type = TYPE_UNKNOWN;
		}
		if (prev_type != type) {
			seq_printf(m, " [0x%lx->0x%lx] %s\n",
				prev_pfn_type, pfn, type_name[prev_type]);
			prev_pfn_type = pfn;
			prev_type = type;
		}
		if (prev_level != lvl) {
			seq_printf(m, " [0x%lx->0x%lx] level %s\n",
				prev_pfn_level, pfn, level_name[prev_level]);
			prev_pfn_level = pfn;
			prev_level = lvl;
		}
	}
	return 0;
#undef TYPE_IDENTITY
#undef TYPE_MISSING
#undef TYPE_PFN
#undef TYPE_UNKNOWN
}

static int p2m_dump_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, p2m_dump_show, NULL);
}

static const struct file_operations p2m_dump_fops = {
	.open		= p2m_dump_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static struct dentry *d_mmu_debug;

static int __init xen_p2m_debugfs(void)
{
	struct dentry *d_xen = xen_init_debugfs();

	if (d_xen == NULL)
		return -ENOMEM;

	d_mmu_debug = debugfs_create_dir("mmu", d_xen);

	debugfs_create_file("p2m", 0600, d_mmu_debug, NULL, &p2m_dump_fops);
	return 0;
}
fs_initcall(xen_p2m_debugfs);
#endif 
