#ifndef _ASM_X86_CACHEFLUSH_H
#define _ASM_X86_CACHEFLUSH_H

#include <asm-generic/cacheflush.h>
#include <asm/special_insns.h>

#ifdef CONFIG_X86_PAT

#define _PGMT_DEFAULT		0
#define _PGMT_WC		(1UL << PG_arch_1)
#define _PGMT_UC_MINUS		(1UL << PG_uncached)
#define _PGMT_WB		(1UL << PG_uncached | 1UL << PG_arch_1)
#define _PGMT_MASK		(1UL << PG_uncached | 1UL << PG_arch_1)
#define _PGMT_CLEAR_MASK	(~_PGMT_MASK)

static inline unsigned long get_page_memtype(struct page *pg)
{
	unsigned long pg_flags = pg->flags & _PGMT_MASK;

	if (pg_flags == _PGMT_DEFAULT)
		return -1;
	else if (pg_flags == _PGMT_WC)
		return _PAGE_CACHE_WC;
	else if (pg_flags == _PGMT_UC_MINUS)
		return _PAGE_CACHE_UC_MINUS;
	else
		return _PAGE_CACHE_WB;
}

static inline void set_page_memtype(struct page *pg, unsigned long memtype)
{
	unsigned long memtype_flags = _PGMT_DEFAULT;
	unsigned long old_flags;
	unsigned long new_flags;

	switch (memtype) {
	case _PAGE_CACHE_WC:
		memtype_flags = _PGMT_WC;
		break;
	case _PAGE_CACHE_UC_MINUS:
		memtype_flags = _PGMT_UC_MINUS;
		break;
	case _PAGE_CACHE_WB:
		memtype_flags = _PGMT_WB;
		break;
	}

	do {
		old_flags = pg->flags;
		new_flags = (old_flags & _PGMT_CLEAR_MASK) | memtype_flags;
	} while (cmpxchg(&pg->flags, old_flags, new_flags) != old_flags);
}
#else
static inline unsigned long get_page_memtype(struct page *pg) { return -1; }
static inline void set_page_memtype(struct page *pg, unsigned long memtype) { }
#endif


int _set_memory_uc(unsigned long addr, int numpages);
int _set_memory_wc(unsigned long addr, int numpages);
int _set_memory_wb(unsigned long addr, int numpages);
int set_memory_uc(unsigned long addr, int numpages);
int set_memory_wc(unsigned long addr, int numpages);
int set_memory_wb(unsigned long addr, int numpages);
int set_memory_x(unsigned long addr, int numpages);
int set_memory_nx(unsigned long addr, int numpages);
int set_memory_ro(unsigned long addr, int numpages);
int set_memory_rw(unsigned long addr, int numpages);
int set_memory_np(unsigned long addr, int numpages);
int set_memory_4k(unsigned long addr, int numpages);

int set_memory_array_uc(unsigned long *addr, int addrinarray);
int set_memory_array_wc(unsigned long *addr, int addrinarray);
int set_memory_array_wb(unsigned long *addr, int addrinarray);

int set_pages_array_uc(struct page **pages, int addrinarray);
int set_pages_array_wc(struct page **pages, int addrinarray);
int set_pages_array_wb(struct page **pages, int addrinarray);


int set_pages_uc(struct page *page, int numpages);
int set_pages_wb(struct page *page, int numpages);
int set_pages_x(struct page *page, int numpages);
int set_pages_nx(struct page *page, int numpages);
int set_pages_ro(struct page *page, int numpages);
int set_pages_rw(struct page *page, int numpages);


void clflush_cache_range(void *addr, unsigned int size);

#ifdef CONFIG_DEBUG_RODATA
void mark_rodata_ro(void);
extern const int rodata_test_data;
extern int kernel_set_to_readonly;
void set_kernel_text_rw(void);
void set_kernel_text_ro(void);
#else
static inline void set_kernel_text_rw(void) { }
static inline void set_kernel_text_ro(void) { }
#endif

#ifdef CONFIG_DEBUG_RODATA_TEST
int rodata_test(void);
#else
static inline int rodata_test(void)
{
	return 0;
}
#endif

#endif 
