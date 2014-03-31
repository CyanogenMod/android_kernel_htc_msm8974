/*
 * Kernel Debugger Architecture Independent Support Functions
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 1999-2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2009 Wind River Systems, Inc.  All Rights Reserved.
 * 03/02/13    added new 2.5 kallsyms <xavier.bru@bull.net>
 */

#include <stdarg.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/kallsyms.h>
#include <linux/stddef.h>
#include <linux/vmalloc.h>
#include <linux/ptrace.h>
#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/hardirq.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/kdb.h>
#include <linux/slab.h>
#include "kdb_private.h"

int kdbgetsymval(const char *symname, kdb_symtab_t *symtab)
{
	if (KDB_DEBUG(AR))
		kdb_printf("kdbgetsymval: symname=%s, symtab=%p\n", symname,
			   symtab);
	memset(symtab, 0, sizeof(*symtab));
	symtab->sym_start = kallsyms_lookup_name(symname);
	if (symtab->sym_start) {
		if (KDB_DEBUG(AR))
			kdb_printf("kdbgetsymval: returns 1, "
				   "symtab->sym_start=0x%lx\n",
				   symtab->sym_start);
		return 1;
	}
	if (KDB_DEBUG(AR))
		kdb_printf("kdbgetsymval: returns 0\n");
	return 0;
}
EXPORT_SYMBOL(kdbgetsymval);

static char *kdb_name_table[100];	

int kdbnearsym(unsigned long addr, kdb_symtab_t *symtab)
{
	int ret = 0;
	unsigned long symbolsize = 0;
	unsigned long offset = 0;
#define knt1_size 128		
	char *knt1 = NULL;

	if (KDB_DEBUG(AR))
		kdb_printf("kdbnearsym: addr=0x%lx, symtab=%p\n", addr, symtab);
	memset(symtab, 0, sizeof(*symtab));

	if (addr < 4096)
		goto out;
	knt1 = debug_kmalloc(knt1_size, GFP_ATOMIC);
	if (!knt1) {
		kdb_printf("kdbnearsym: addr=0x%lx cannot kmalloc knt1\n",
			   addr);
		goto out;
	}
	symtab->sym_name = kallsyms_lookup(addr, &symbolsize , &offset,
				(char **)(&symtab->mod_name), knt1);
	if (offset > 8*1024*1024) {
		symtab->sym_name = NULL;
		addr = offset = symbolsize = 0;
	}
	symtab->sym_start = addr - offset;
	symtab->sym_end = symtab->sym_start + symbolsize;
	ret = symtab->sym_name != NULL && *(symtab->sym_name) != '\0';

	if (ret) {
		int i;
		if (symtab->sym_name != knt1) {
			strncpy(knt1, symtab->sym_name, knt1_size);
			knt1[knt1_size-1] = '\0';
		}
		for (i = 0; i < ARRAY_SIZE(kdb_name_table); ++i) {
			if (kdb_name_table[i] &&
			    strcmp(kdb_name_table[i], knt1) == 0)
				break;
		}
		if (i >= ARRAY_SIZE(kdb_name_table)) {
			debug_kfree(kdb_name_table[0]);
			memcpy(kdb_name_table, kdb_name_table+1,
			       sizeof(kdb_name_table[0]) *
			       (ARRAY_SIZE(kdb_name_table)-1));
		} else {
			debug_kfree(knt1);
			knt1 = kdb_name_table[i];
			memcpy(kdb_name_table+i, kdb_name_table+i+1,
			       sizeof(kdb_name_table[0]) *
			       (ARRAY_SIZE(kdb_name_table)-i-1));
		}
		i = ARRAY_SIZE(kdb_name_table) - 1;
		kdb_name_table[i] = knt1;
		symtab->sym_name = kdb_name_table[i];
		knt1 = NULL;
	}

	if (symtab->mod_name == NULL)
		symtab->mod_name = "kernel";
	if (KDB_DEBUG(AR))
		kdb_printf("kdbnearsym: returns %d symtab->sym_start=0x%lx, "
		   "symtab->mod_name=%p, symtab->sym_name=%p (%s)\n", ret,
		   symtab->sym_start, symtab->mod_name, symtab->sym_name,
		   symtab->sym_name);

out:
	debug_kfree(knt1);
	return ret;
}

void kdbnearsym_cleanup(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(kdb_name_table); ++i) {
		if (kdb_name_table[i]) {
			debug_kfree(kdb_name_table[i]);
			kdb_name_table[i] = NULL;
		}
	}
}

static char ks_namebuf[KSYM_NAME_LEN+1], ks_namebuf_prev[KSYM_NAME_LEN+1];

int kallsyms_symbol_complete(char *prefix_name, int max_len)
{
	loff_t pos = 0;
	int prefix_len = strlen(prefix_name), prev_len = 0;
	int i, number = 0;
	const char *name;

	while ((name = kdb_walk_kallsyms(&pos))) {
		if (strncmp(name, prefix_name, prefix_len) == 0) {
			strcpy(ks_namebuf, name);
			
			if (++number == 1) {
				prev_len = min_t(int, max_len-1,
						 strlen(ks_namebuf));
				memcpy(ks_namebuf_prev, ks_namebuf, prev_len);
				ks_namebuf_prev[prev_len] = '\0';
				continue;
			}
			for (i = 0; i < prev_len; i++) {
				if (ks_namebuf[i] != ks_namebuf_prev[i]) {
					prev_len = i;
					ks_namebuf_prev[i] = '\0';
					break;
				}
			}
		}
	}
	if (prev_len > prefix_len)
		memcpy(prefix_name, ks_namebuf_prev, prev_len+1);
	return number;
}

int kallsyms_symbol_next(char *prefix_name, int flag)
{
	int prefix_len = strlen(prefix_name);
	static loff_t pos;
	const char *name;

	if (!flag)
		pos = 0;

	while ((name = kdb_walk_kallsyms(&pos))) {
		if (strncmp(name, prefix_name, prefix_len) == 0) {
			strncpy(prefix_name, name, strlen(name)+1);
			return 1;
		}
	}
	return 0;
}

void kdb_symbol_print(unsigned long addr, const kdb_symtab_t *symtab_p,
		      unsigned int punc)
{
	kdb_symtab_t symtab, *symtab_p2;
	if (symtab_p) {
		symtab_p2 = (kdb_symtab_t *)symtab_p;
	} else {
		symtab_p2 = &symtab;
		kdbnearsym(addr, symtab_p2);
	}
	if (!(symtab_p2->sym_name || (punc & KDB_SP_VALUE)))
		return;
	if (punc & KDB_SP_SPACEB)
		kdb_printf(" ");
	if (punc & KDB_SP_VALUE)
		kdb_printf(kdb_machreg_fmt0, addr);
	if (symtab_p2->sym_name) {
		if (punc & KDB_SP_VALUE)
			kdb_printf(" ");
		if (punc & KDB_SP_PAREN)
			kdb_printf("(");
		if (strcmp(symtab_p2->mod_name, "kernel"))
			kdb_printf("[%s]", symtab_p2->mod_name);
		kdb_printf("%s", symtab_p2->sym_name);
		if (addr != symtab_p2->sym_start)
			kdb_printf("+0x%lx", addr - symtab_p2->sym_start);
		if (punc & KDB_SP_SYMSIZE)
			kdb_printf("/0x%lx",
				   symtab_p2->sym_end - symtab_p2->sym_start);
		if (punc & KDB_SP_PAREN)
			kdb_printf(")");
	}
	if (punc & KDB_SP_SPACEA)
		kdb_printf(" ");
	if (punc & KDB_SP_NEWLINE)
		kdb_printf("\n");
}

char *kdb_strdup(const char *str, gfp_t type)
{
	int n = strlen(str)+1;
	char *s = kmalloc(n, type);
	if (!s)
		return NULL;
	return strcpy(s, str);
}

int kdb_getarea_size(void *res, unsigned long addr, size_t size)
{
	int ret = probe_kernel_read((char *)res, (char *)addr, size);
	if (ret) {
		if (!KDB_STATE(SUPPRESS)) {
			kdb_printf("kdb_getarea: Bad address 0x%lx\n", addr);
			KDB_STATE_SET(SUPPRESS);
		}
		ret = KDB_BADADDR;
	} else {
		KDB_STATE_CLEAR(SUPPRESS);
	}
	return ret;
}

int kdb_putarea_size(unsigned long addr, void *res, size_t size)
{
	int ret = probe_kernel_read((char *)addr, (char *)res, size);
	if (ret) {
		if (!KDB_STATE(SUPPRESS)) {
			kdb_printf("kdb_putarea: Bad address 0x%lx\n", addr);
			KDB_STATE_SET(SUPPRESS);
		}
		ret = KDB_BADADDR;
	} else {
		KDB_STATE_CLEAR(SUPPRESS);
	}
	return ret;
}

static int kdb_getphys(void *res, unsigned long addr, size_t size)
{
	unsigned long pfn;
	void *vaddr;
	struct page *page;

	pfn = (addr >> PAGE_SHIFT);
	if (!pfn_valid(pfn))
		return 1;
	page = pfn_to_page(pfn);
	vaddr = kmap_atomic(page);
	memcpy(res, vaddr + (addr & (PAGE_SIZE - 1)), size);
	kunmap_atomic(vaddr);

	return 0;
}

int kdb_getphysword(unsigned long *word, unsigned long addr, size_t size)
{
	int diag;
	__u8  w1;
	__u16 w2;
	__u32 w4;
	__u64 w8;
	*word = 0;	

	switch (size) {
	case 1:
		diag = kdb_getphys(&w1, addr, sizeof(w1));
		if (!diag)
			*word = w1;
		break;
	case 2:
		diag = kdb_getphys(&w2, addr, sizeof(w2));
		if (!diag)
			*word = w2;
		break;
	case 4:
		diag = kdb_getphys(&w4, addr, sizeof(w4));
		if (!diag)
			*word = w4;
		break;
	case 8:
		if (size <= sizeof(*word)) {
			diag = kdb_getphys(&w8, addr, sizeof(w8));
			if (!diag)
				*word = w8;
			break;
		}
		
	default:
		diag = KDB_BADWIDTH;
		kdb_printf("kdb_getphysword: bad width %ld\n", (long) size);
	}
	return diag;
}

int kdb_getword(unsigned long *word, unsigned long addr, size_t size)
{
	int diag;
	__u8  w1;
	__u16 w2;
	__u32 w4;
	__u64 w8;
	*word = 0;	
	switch (size) {
	case 1:
		diag = kdb_getarea(w1, addr);
		if (!diag)
			*word = w1;
		break;
	case 2:
		diag = kdb_getarea(w2, addr);
		if (!diag)
			*word = w2;
		break;
	case 4:
		diag = kdb_getarea(w4, addr);
		if (!diag)
			*word = w4;
		break;
	case 8:
		if (size <= sizeof(*word)) {
			diag = kdb_getarea(w8, addr);
			if (!diag)
				*word = w8;
			break;
		}
		
	default:
		diag = KDB_BADWIDTH;
		kdb_printf("kdb_getword: bad width %ld\n", (long) size);
	}
	return diag;
}

int kdb_putword(unsigned long addr, unsigned long word, size_t size)
{
	int diag;
	__u8  w1;
	__u16 w2;
	__u32 w4;
	__u64 w8;
	switch (size) {
	case 1:
		w1 = word;
		diag = kdb_putarea(addr, w1);
		break;
	case 2:
		w2 = word;
		diag = kdb_putarea(addr, w2);
		break;
	case 4:
		w4 = word;
		diag = kdb_putarea(addr, w4);
		break;
	case 8:
		if (size <= sizeof(word)) {
			w8 = word;
			diag = kdb_putarea(addr, w8);
			break;
		}
		
	default:
		diag = KDB_BADWIDTH;
		kdb_printf("kdb_putword: bad width %ld\n", (long) size);
	}
	return diag;
}


#define UNRUNNABLE	(1UL << (8*sizeof(unsigned long) - 1))
#define RUNNING		(1UL << (8*sizeof(unsigned long) - 2))
#define IDLE		(1UL << (8*sizeof(unsigned long) - 3))
#define DAEMON		(1UL << (8*sizeof(unsigned long) - 4))

unsigned long kdb_task_state_string(const char *s)
{
	long res = 0;
	if (!s) {
		s = kdbgetenv("PS");
		if (!s)
			s = "DRSTCZEU";	
	}
	while (*s) {
		switch (*s) {
		case 'D':
			res |= TASK_UNINTERRUPTIBLE;
			break;
		case 'R':
			res |= RUNNING;
			break;
		case 'S':
			res |= TASK_INTERRUPTIBLE;
			break;
		case 'T':
			res |= TASK_STOPPED;
			break;
		case 'C':
			res |= TASK_TRACED;
			break;
		case 'Z':
			res |= EXIT_ZOMBIE << 16;
			break;
		case 'E':
			res |= EXIT_DEAD << 16;
			break;
		case 'U':
			res |= UNRUNNABLE;
			break;
		case 'I':
			res |= IDLE;
			break;
		case 'M':
			res |= DAEMON;
			break;
		case 'A':
			res = ~0UL;
			break;
		default:
			  kdb_printf("%s: unknown flag '%c' ignored\n",
				     __func__, *s);
			  break;
		}
		++s;
	}
	return res;
}

char kdb_task_state_char (const struct task_struct *p)
{
	int cpu;
	char state;
	unsigned long tmp;

	if (!p || probe_kernel_read(&tmp, (char *)p, sizeof(unsigned long)))
		return 'E';

	cpu = kdb_process_cpu(p);
	state = (p->state == 0) ? 'R' :
		(p->state < 0) ? 'U' :
		(p->state & TASK_UNINTERRUPTIBLE) ? 'D' :
		(p->state & TASK_STOPPED) ? 'T' :
		(p->state & TASK_TRACED) ? 'C' :
		(p->exit_state & EXIT_ZOMBIE) ? 'Z' :
		(p->exit_state & EXIT_DEAD) ? 'E' :
		(p->state & TASK_INTERRUPTIBLE) ? 'S' : '?';
	if (is_idle_task(p)) {
		if (!kdb_task_has_cpu(p) || kgdb_info[cpu].irq_depth == 1) {
			if (cpu != kdb_initial_cpu)
				state = 'I';	
		}
	} else if (!p->mm && state == 'S') {
		state = 'M';	
	}
	return state;
}

unsigned long kdb_task_state(const struct task_struct *p, unsigned long mask)
{
	char state[] = { kdb_task_state_char(p), '\0' };
	return (mask & kdb_task_state_string(state)) != 0;
}

void kdb_print_nameval(const char *name, unsigned long val)
{
	kdb_symtab_t symtab;
	kdb_printf("  %-11.11s ", name);
	if (kdbnearsym(val, &symtab))
		kdb_symbol_print(val, &symtab,
				 KDB_SP_VALUE|KDB_SP_SYMSIZE|KDB_SP_NEWLINE);
	else
		kdb_printf("0x%lx\n", val);
}


struct debug_alloc_header {
	u32 next;	
	u32 size;
	void *caller;
};

#define dah_align 8
#define dah_overhead ALIGN(sizeof(struct debug_alloc_header), dah_align)

static u64 debug_alloc_pool_aligned[256*1024/dah_align];	
static char *debug_alloc_pool = (char *)debug_alloc_pool_aligned;
static u32 dah_first, dah_first_call = 1, dah_used, dah_used_max;

static DEFINE_SPINLOCK(dap_lock);
static int get_dap_lock(void)
	__acquires(dap_lock)
{
	static int dap_locked = -1;
	int count;
	if (dap_locked == smp_processor_id())
		count = 1;
	else
		count = 1000;
	while (1) {
		if (spin_trylock(&dap_lock)) {
			dap_locked = -1;
			return 1;
		}
		if (!count--)
			break;
		udelay(1000);
	}
	dap_locked = smp_processor_id();
	__acquire(dap_lock);
	return 0;
}

void *debug_kmalloc(size_t size, gfp_t flags)
{
	unsigned int rem, h_offset;
	struct debug_alloc_header *best, *bestprev, *prev, *h;
	void *p = NULL;
	if (!get_dap_lock()) {
		__release(dap_lock);	
		return NULL;
	}
	h = (struct debug_alloc_header *)(debug_alloc_pool + dah_first);
	if (dah_first_call) {
		h->size = sizeof(debug_alloc_pool_aligned) - dah_overhead;
		dah_first_call = 0;
	}
	size = ALIGN(size, dah_align);
	prev = best = bestprev = NULL;
	while (1) {
		if (h->size >= size && (!best || h->size < best->size)) {
			best = h;
			bestprev = prev;
			if (h->size == size)
				break;
		}
		if (!h->next)
			break;
		prev = h;
		h = (struct debug_alloc_header *)(debug_alloc_pool + h->next);
	}
	if (!best)
		goto out;
	rem = best->size - size;
	
	if (best->next == 0 && bestprev == NULL && rem < dah_overhead)
		goto out;
	if (rem >= dah_overhead) {
		best->size = size;
		h_offset = ((char *)best - debug_alloc_pool) +
			   dah_overhead + best->size;
		h = (struct debug_alloc_header *)(debug_alloc_pool + h_offset);
		h->size = rem - dah_overhead;
		h->next = best->next;
	} else
		h_offset = best->next;
	best->caller = __builtin_return_address(0);
	dah_used += best->size;
	dah_used_max = max(dah_used, dah_used_max);
	if (bestprev)
		bestprev->next = h_offset;
	else
		dah_first = h_offset;
	p = (char *)best + dah_overhead;
	memset(p, POISON_INUSE, best->size - 1);
	*((char *)p + best->size - 1) = POISON_END;
out:
	spin_unlock(&dap_lock);
	return p;
}

void debug_kfree(void *p)
{
	struct debug_alloc_header *h;
	unsigned int h_offset;
	if (!p)
		return;
	if ((char *)p < debug_alloc_pool ||
	    (char *)p >= debug_alloc_pool + sizeof(debug_alloc_pool_aligned)) {
		kfree(p);
		return;
	}
	if (!get_dap_lock()) {
		__release(dap_lock);	
		return;		
	}
	h = (struct debug_alloc_header *)((char *)p - dah_overhead);
	memset(p, POISON_FREE, h->size - 1);
	*((char *)p + h->size - 1) = POISON_END;
	h->caller = NULL;
	dah_used -= h->size;
	h_offset = (char *)h - debug_alloc_pool;
	if (h_offset < dah_first) {
		h->next = dah_first;
		dah_first = h_offset;
	} else {
		struct debug_alloc_header *prev;
		unsigned int prev_offset;
		prev = (struct debug_alloc_header *)(debug_alloc_pool +
						     dah_first);
		while (1) {
			if (!prev->next || prev->next > h_offset)
				break;
			prev = (struct debug_alloc_header *)
				(debug_alloc_pool + prev->next);
		}
		prev_offset = (char *)prev - debug_alloc_pool;
		if (prev_offset + dah_overhead + prev->size == h_offset) {
			prev->size += dah_overhead + h->size;
			memset(h, POISON_FREE, dah_overhead - 1);
			*((char *)h + dah_overhead - 1) = POISON_END;
			h = prev;
			h_offset = prev_offset;
		} else {
			h->next = prev->next;
			prev->next = h_offset;
		}
	}
	if (h_offset + dah_overhead + h->size == h->next) {
		struct debug_alloc_header *next;
		next = (struct debug_alloc_header *)
			(debug_alloc_pool + h->next);
		h->size += dah_overhead + next->size;
		h->next = next->next;
		memset(next, POISON_FREE, dah_overhead - 1);
		*((char *)next + dah_overhead - 1) = POISON_END;
	}
	spin_unlock(&dap_lock);
}

void debug_kusage(void)
{
	struct debug_alloc_header *h_free, *h_used;
#ifdef	CONFIG_IA64
	static int debug_kusage_one_time;
#else
	static int debug_kusage_one_time = 1;
#endif
	if (!get_dap_lock()) {
		__release(dap_lock);	
		return;
	}
	h_free = (struct debug_alloc_header *)(debug_alloc_pool + dah_first);
	if (dah_first == 0 &&
	    (h_free->size == sizeof(debug_alloc_pool_aligned) - dah_overhead ||
	     dah_first_call))
		goto out;
	if (!debug_kusage_one_time)
		goto out;
	debug_kusage_one_time = 0;
	kdb_printf("%s: debug_kmalloc memory leak dah_first %d\n",
		   __func__, dah_first);
	if (dah_first) {
		h_used = (struct debug_alloc_header *)debug_alloc_pool;
		kdb_printf("%s: h_used %p size %d\n", __func__, h_used,
			   h_used->size);
	}
	do {
		h_used = (struct debug_alloc_header *)
			  ((char *)h_free + dah_overhead + h_free->size);
		kdb_printf("%s: h_used %p size %d caller %p\n",
			   __func__, h_used, h_used->size, h_used->caller);
		h_free = (struct debug_alloc_header *)
			  (debug_alloc_pool + h_free->next);
	} while (h_free->next);
	h_used = (struct debug_alloc_header *)
		  ((char *)h_free + dah_overhead + h_free->size);
	if ((char *)h_used - debug_alloc_pool !=
	    sizeof(debug_alloc_pool_aligned))
		kdb_printf("%s: h_used %p size %d caller %p\n",
			   __func__, h_used, h_used->size, h_used->caller);
out:
	spin_unlock(&dap_lock);
}


static int kdb_flags_stack[4], kdb_flags_index;

void kdb_save_flags(void)
{
	BUG_ON(kdb_flags_index >= ARRAY_SIZE(kdb_flags_stack));
	kdb_flags_stack[kdb_flags_index++] = kdb_flags;
}

void kdb_restore_flags(void)
{
	BUG_ON(kdb_flags_index <= 0);
	kdb_flags = kdb_flags_stack[--kdb_flags_index];
}
