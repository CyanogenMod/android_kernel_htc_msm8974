#include <linux/percpu.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/cache.h>

#include <asm/tlb.h>
#include <asm/tlbflush.h>
#include <asm/mmu_context.h>
#include <asm/page.h>

void __init paging_init(void)
{
	memset(swapper_pg_dir, 0, PAGE_SIZE);
}

void __init init_mmu(void)
{
	set_itlbcfg_register(0);
	set_dtlbcfg_register(0);
	flush_tlb_all();

	

	set_rasid_register(ASID_USER_FIRST);

	set_ptevaddr_register(PGTABLE_START);
}

struct kmem_cache *pgtable_cache __read_mostly;

static void pgd_ctor(void *addr)
{
	pte_t *ptep = (pte_t *)addr;
	int i;

	for (i = 0; i < 1024; i++, ptep++)
		pte_clear(NULL, 0, ptep);

}

void __init pgtable_cache_init(void)
{
	pgtable_cache = kmem_cache_create("pgd",
			PAGE_SIZE, PAGE_SIZE,
			SLAB_HWCACHE_ALIGN,
			pgd_ctor);
}
