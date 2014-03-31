/*
 * Kernel exception handling table support.  Derived from arch/alpha/mm/extable.c.
 *
 * Copyright (C) 1998, 1999, 2001-2002, 2004 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */

#include <linux/sort.h>

#include <asm/uaccess.h>
#include <linux/module.h>

static int cmp_ex(const void *a, const void *b)
{
	const struct exception_table_entry *l = a, *r = b;
	u64 lip = (u64) &l->addr + l->addr;
	u64 rip = (u64) &r->addr + r->addr;

	
	if (lip > rip)
		return 1;
	if (lip < rip)
		return -1;
	return 0;
}

static void swap_ex(void *a, void *b, int size)
{
	struct exception_table_entry *l = a, *r = b, tmp;
	u64 delta = (u64) r - (u64) l;

	tmp = *l;
	l->addr = r->addr + delta;
	l->cont = r->cont + delta;
	r->addr = tmp.addr - delta;
	r->cont = tmp.cont - delta;
}

void sort_extable (struct exception_table_entry *start,
		   struct exception_table_entry *finish)
{
	sort(start, finish - start, sizeof(struct exception_table_entry),
	     cmp_ex, swap_ex);
}

static inline unsigned long ex_to_addr(const struct exception_table_entry *x)
{
	return (unsigned long)&x->addr + x->addr;
}

#ifdef CONFIG_MODULES
void trim_init_extable(struct module *m)
{
	
	while (m->num_exentries &&
	       within_module_init(ex_to_addr(&m->extable[0]), m)) {
		m->extable++;
		m->num_exentries--;
	}
	
	while (m->num_exentries &&
	       within_module_init(ex_to_addr(&m->extable[m->num_exentries-1]),
				  m))
		m->num_exentries--;
}
#endif 

const struct exception_table_entry *
search_extable (const struct exception_table_entry *first,
		const struct exception_table_entry *last,
		unsigned long ip)
{
	const struct exception_table_entry *mid;
	unsigned long mid_ip;
	long diff;

        while (first <= last) {
		mid = &first[(last - first)/2];
		mid_ip = (u64) &mid->addr + mid->addr;
		diff = mid_ip - ip;
                if (diff == 0)
                        return mid;
                else if (diff < 0)
                        first = mid + 1;
                else
                        last = mid - 1;
        }
        return NULL;
}

void
ia64_handle_exception (struct pt_regs *regs, const struct exception_table_entry *e)
{
	long fix = (u64) &e->cont + e->cont;

	regs->r8 = -EFAULT;
	if (fix & 4)
		regs->r9 = 0;
	regs->cr_iip = fix & ~0xf;
	ia64_psr(regs)->ri = fix & 0x3;		
}
