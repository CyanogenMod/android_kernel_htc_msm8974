/*
 *  Kernel Probes (KProbes)
 *  kernel/kprobes.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) IBM Corporation, 2002, 2004
 *
 * 2002-Oct	Created by Vamsi Krishna S <vamsi_krishna@in.ibm.com> Kernel
 *		Probes initial implementation (includes suggestions from
 *		Rusty Russell).
 * 2004-Aug	Updated by Prasanna S Panchamukhi <prasanna@in.ibm.com> with
 *		hlists and exceptions notifier as suggested by Andi Kleen.
 * 2004-July	Suparna Bhattacharya <suparna@in.ibm.com> added jumper probes
 *		interface to access function arguments.
 * 2004-Sep	Prasanna S Panchamukhi <prasanna@in.ibm.com> Changed Kprobes
 *		exceptions notifier to be first on the priority list.
 * 2005-May	Hien Nguyen <hien@us.ibm.com>, Jim Keniston
 *		<jkenisto@us.ibm.com> and Prasanna S Panchamukhi
 *		<prasanna@in.ibm.com> added function-return probes.
 */
#include <linux/kprobes.h>
#include <linux/hash.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/stddef.h>
#include <linux/export.h>
#include <linux/moduleloader.h>
#include <linux/kallsyms.h>
#include <linux/freezer.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/sysctl.h>
#include <linux/kdebug.h>
#include <linux/memory.h>
#include <linux/ftrace.h>
#include <linux/cpu.h>
#include <linux/jump_label.h>

#include <asm-generic/sections.h>
#include <asm/cacheflush.h>
#include <asm/errno.h>
#include <asm/uaccess.h>

#define KPROBE_HASH_BITS 6
#define KPROBE_TABLE_SIZE (1 << KPROBE_HASH_BITS)


#ifndef kprobe_lookup_name
#define kprobe_lookup_name(name, addr) \
	addr = ((kprobe_opcode_t *)(kallsyms_lookup_name(name)))
#endif

static int kprobes_initialized;
static struct hlist_head kprobe_table[KPROBE_TABLE_SIZE];
static struct hlist_head kretprobe_inst_table[KPROBE_TABLE_SIZE];

static bool kprobes_all_disarmed;

static DEFINE_MUTEX(kprobe_mutex);
static DEFINE_PER_CPU(struct kprobe *, kprobe_instance) = NULL;
static struct {
	raw_spinlock_t lock ____cacheline_aligned_in_smp;
} kretprobe_table_locks[KPROBE_TABLE_SIZE];

static raw_spinlock_t *kretprobe_table_lock_ptr(unsigned long hash)
{
	return &(kretprobe_table_locks[hash].lock);
}

static struct kprobe_blackpoint kprobe_blacklist[] = {
	{"preempt_schedule",},
	{"native_get_debugreg",},
	{"irq_entries_start",},
	{"common_interrupt",},
	{"mcount",},	
	{NULL}    
};

#ifdef __ARCH_WANT_KPROBES_INSN_SLOT
struct kprobe_insn_page {
	struct list_head list;
	kprobe_opcode_t *insns;		
	int nused;
	int ngarbage;
	char slot_used[];
};

#define KPROBE_INSN_PAGE_SIZE(slots)			\
	(offsetof(struct kprobe_insn_page, slot_used) +	\
	 (sizeof(char) * (slots)))

struct kprobe_insn_cache {
	struct list_head pages;	
	size_t insn_size;	
	int nr_garbage;
};

static int slots_per_page(struct kprobe_insn_cache *c)
{
	return PAGE_SIZE/(c->insn_size * sizeof(kprobe_opcode_t));
}

enum kprobe_slot_state {
	SLOT_CLEAN = 0,
	SLOT_DIRTY = 1,
	SLOT_USED = 2,
};

static DEFINE_MUTEX(kprobe_insn_mutex);	
static struct kprobe_insn_cache kprobe_insn_slots = {
	.pages = LIST_HEAD_INIT(kprobe_insn_slots.pages),
	.insn_size = MAX_INSN_SIZE,
	.nr_garbage = 0,
};
static int __kprobes collect_garbage_slots(struct kprobe_insn_cache *c);

static kprobe_opcode_t __kprobes *__get_insn_slot(struct kprobe_insn_cache *c)
{
	struct kprobe_insn_page *kip;

 retry:
	list_for_each_entry(kip, &c->pages, list) {
		if (kip->nused < slots_per_page(c)) {
			int i;
			for (i = 0; i < slots_per_page(c); i++) {
				if (kip->slot_used[i] == SLOT_CLEAN) {
					kip->slot_used[i] = SLOT_USED;
					kip->nused++;
					return kip->insns + (i * c->insn_size);
				}
			}
			
			kip->nused = slots_per_page(c);
			WARN_ON(1);
		}
	}

	
	if (c->nr_garbage && collect_garbage_slots(c) == 0)
		goto retry;

	
	kip = kmalloc(KPROBE_INSN_PAGE_SIZE(slots_per_page(c)), GFP_KERNEL);
	if (!kip)
		return NULL;

	kip->insns = module_alloc(PAGE_SIZE);
	if (!kip->insns) {
		kfree(kip);
		return NULL;
	}
	INIT_LIST_HEAD(&kip->list);
	memset(kip->slot_used, SLOT_CLEAN, slots_per_page(c));
	kip->slot_used[0] = SLOT_USED;
	kip->nused = 1;
	kip->ngarbage = 0;
	list_add(&kip->list, &c->pages);
	return kip->insns;
}


kprobe_opcode_t __kprobes *get_insn_slot(void)
{
	kprobe_opcode_t *ret = NULL;

	mutex_lock(&kprobe_insn_mutex);
	ret = __get_insn_slot(&kprobe_insn_slots);
	mutex_unlock(&kprobe_insn_mutex);

	return ret;
}

static int __kprobes collect_one_slot(struct kprobe_insn_page *kip, int idx)
{
	kip->slot_used[idx] = SLOT_CLEAN;
	kip->nused--;
	if (kip->nused == 0) {
		if (!list_is_singular(&kip->list)) {
			list_del(&kip->list);
			module_free(NULL, kip->insns);
			kfree(kip);
		}
		return 1;
	}
	return 0;
}

static int __kprobes collect_garbage_slots(struct kprobe_insn_cache *c)
{
	struct kprobe_insn_page *kip, *next;

	
	synchronize_sched();

	list_for_each_entry_safe(kip, next, &c->pages, list) {
		int i;
		if (kip->ngarbage == 0)
			continue;
		kip->ngarbage = 0;	
		for (i = 0; i < slots_per_page(c); i++) {
			if (kip->slot_used[i] == SLOT_DIRTY &&
			    collect_one_slot(kip, i))
				break;
		}
	}
	c->nr_garbage = 0;
	return 0;
}

static void __kprobes __free_insn_slot(struct kprobe_insn_cache *c,
				       kprobe_opcode_t *slot, int dirty)
{
	struct kprobe_insn_page *kip;

	list_for_each_entry(kip, &c->pages, list) {
		long idx = ((long)slot - (long)kip->insns) /
				(c->insn_size * sizeof(kprobe_opcode_t));
		if (idx >= 0 && idx < slots_per_page(c)) {
			WARN_ON(kip->slot_used[idx] != SLOT_USED);
			if (dirty) {
				kip->slot_used[idx] = SLOT_DIRTY;
				kip->ngarbage++;
				if (++c->nr_garbage > slots_per_page(c))
					collect_garbage_slots(c);
			} else
				collect_one_slot(kip, idx);
			return;
		}
	}
	
	WARN_ON(1);
}

void __kprobes free_insn_slot(kprobe_opcode_t * slot, int dirty)
{
	mutex_lock(&kprobe_insn_mutex);
	__free_insn_slot(&kprobe_insn_slots, slot, dirty);
	mutex_unlock(&kprobe_insn_mutex);
}
#ifdef CONFIG_OPTPROBES
static DEFINE_MUTEX(kprobe_optinsn_mutex); 
static struct kprobe_insn_cache kprobe_optinsn_slots = {
	.pages = LIST_HEAD_INIT(kprobe_optinsn_slots.pages),
	
	.nr_garbage = 0,
};
kprobe_opcode_t __kprobes *get_optinsn_slot(void)
{
	kprobe_opcode_t *ret = NULL;

	mutex_lock(&kprobe_optinsn_mutex);
	ret = __get_insn_slot(&kprobe_optinsn_slots);
	mutex_unlock(&kprobe_optinsn_mutex);

	return ret;
}

void __kprobes free_optinsn_slot(kprobe_opcode_t * slot, int dirty)
{
	mutex_lock(&kprobe_optinsn_mutex);
	__free_insn_slot(&kprobe_optinsn_slots, slot, dirty);
	mutex_unlock(&kprobe_optinsn_mutex);
}
#endif
#endif

static inline void set_kprobe_instance(struct kprobe *kp)
{
	__this_cpu_write(kprobe_instance, kp);
}

static inline void reset_kprobe_instance(void)
{
	__this_cpu_write(kprobe_instance, NULL);
}

struct kprobe __kprobes *get_kprobe(void *addr)
{
	struct hlist_head *head;
	struct hlist_node *node;
	struct kprobe *p;

	head = &kprobe_table[hash_ptr(addr, KPROBE_HASH_BITS)];
	hlist_for_each_entry_rcu(p, node, head, hlist) {
		if (p->addr == addr)
			return p;
	}

	return NULL;
}

static int __kprobes aggr_pre_handler(struct kprobe *p, struct pt_regs *regs);

static inline int kprobe_aggrprobe(struct kprobe *p)
{
	return p->pre_handler == aggr_pre_handler;
}

static inline int kprobe_unused(struct kprobe *p)
{
	return kprobe_aggrprobe(p) && kprobe_disabled(p) &&
	       list_empty(&p->list);
}

static inline void copy_kprobe(struct kprobe *ap, struct kprobe *p)
{
	memcpy(&p->opcode, &ap->opcode, sizeof(kprobe_opcode_t));
	memcpy(&p->ainsn, &ap->ainsn, sizeof(struct arch_specific_insn));
}

#ifdef CONFIG_OPTPROBES
static bool kprobes_allow_optimization;

void __kprobes opt_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct kprobe *kp;

	list_for_each_entry_rcu(kp, &p->list, list) {
		if (kp->pre_handler && likely(!kprobe_disabled(kp))) {
			set_kprobe_instance(kp);
			kp->pre_handler(kp, regs);
		}
		reset_kprobe_instance();
	}
}

static __kprobes void free_aggr_kprobe(struct kprobe *p)
{
	struct optimized_kprobe *op;

	op = container_of(p, struct optimized_kprobe, kp);
	arch_remove_optimized_kprobe(op);
	arch_remove_kprobe(p);
	kfree(op);
}

static inline int kprobe_optready(struct kprobe *p)
{
	struct optimized_kprobe *op;

	if (kprobe_aggrprobe(p)) {
		op = container_of(p, struct optimized_kprobe, kp);
		return arch_prepared_optinsn(&op->optinsn);
	}

	return 0;
}

static inline int kprobe_disarmed(struct kprobe *p)
{
	struct optimized_kprobe *op;

	
	if (!kprobe_aggrprobe(p))
		return kprobe_disabled(p);

	op = container_of(p, struct optimized_kprobe, kp);

	return kprobe_disabled(p) && list_empty(&op->list);
}

static int __kprobes kprobe_queued(struct kprobe *p)
{
	struct optimized_kprobe *op;

	if (kprobe_aggrprobe(p)) {
		op = container_of(p, struct optimized_kprobe, kp);
		if (!list_empty(&op->list))
			return 1;
	}
	return 0;
}

static struct kprobe *__kprobes get_optimized_kprobe(unsigned long addr)
{
	int i;
	struct kprobe *p = NULL;
	struct optimized_kprobe *op;

	
	for (i = 1; !p && i < MAX_OPTIMIZED_LENGTH; i++)
		p = get_kprobe((void *)(addr - i));

	if (p && kprobe_optready(p)) {
		op = container_of(p, struct optimized_kprobe, kp);
		if (arch_within_optimized_kprobe(op, addr))
			return p;
	}

	return NULL;
}

static LIST_HEAD(optimizing_list);
static LIST_HEAD(unoptimizing_list);

static void kprobe_optimizer(struct work_struct *work);
static DECLARE_DELAYED_WORK(optimizing_work, kprobe_optimizer);
static DECLARE_COMPLETION(optimizer_comp);
#define OPTIMIZE_DELAY 5

static __kprobes void do_optimize_kprobes(void)
{
	
	if (kprobes_all_disarmed || !kprobes_allow_optimization ||
	    list_empty(&optimizing_list))
		return;

	get_online_cpus();
	mutex_lock(&text_mutex);
	arch_optimize_kprobes(&optimizing_list);
	mutex_unlock(&text_mutex);
	put_online_cpus();
}

static __kprobes void do_unoptimize_kprobes(struct list_head *free_list)
{
	struct optimized_kprobe *op, *tmp;

	
	if (list_empty(&unoptimizing_list))
		return;

	
	get_online_cpus();
	mutex_lock(&text_mutex);
	arch_unoptimize_kprobes(&unoptimizing_list, free_list);
	
	list_for_each_entry_safe(op, tmp, free_list, list) {
		
		if (kprobe_disabled(&op->kp))
			arch_disarm_kprobe(&op->kp);
		if (kprobe_unused(&op->kp)) {
			hlist_del_rcu(&op->kp.hlist);
		} else
			list_del_init(&op->list);
	}
	mutex_unlock(&text_mutex);
	put_online_cpus();
}

static __kprobes void do_free_cleaned_kprobes(struct list_head *free_list)
{
	struct optimized_kprobe *op, *tmp;

	list_for_each_entry_safe(op, tmp, free_list, list) {
		BUG_ON(!kprobe_unused(&op->kp));
		list_del_init(&op->list);
		free_aggr_kprobe(&op->kp);
	}
}

static __kprobes void kick_kprobe_optimizer(void)
{
	if (!delayed_work_pending(&optimizing_work))
		schedule_delayed_work(&optimizing_work, OPTIMIZE_DELAY);
}

static __kprobes void kprobe_optimizer(struct work_struct *work)
{
	LIST_HEAD(free_list);

	
	mutex_lock(&module_mutex);
	mutex_lock(&kprobe_mutex);

	do_unoptimize_kprobes(&free_list);

	synchronize_sched();

	
	do_optimize_kprobes();

	
	do_free_cleaned_kprobes(&free_list);

	mutex_unlock(&kprobe_mutex);
	mutex_unlock(&module_mutex);

	
	if (!list_empty(&optimizing_list) || !list_empty(&unoptimizing_list))
		kick_kprobe_optimizer();
	else
		
		complete_all(&optimizer_comp);
}

static __kprobes void wait_for_kprobe_optimizer(void)
{
	if (delayed_work_pending(&optimizing_work))
		wait_for_completion(&optimizer_comp);
}

static __kprobes void optimize_kprobe(struct kprobe *p)
{
	struct optimized_kprobe *op;

	
	if (!kprobe_optready(p) || !kprobes_allow_optimization ||
	    (kprobe_disabled(p) || kprobes_all_disarmed))
		return;

	
	if (p->break_handler || p->post_handler)
		return;

	op = container_of(p, struct optimized_kprobe, kp);

	
	if (arch_check_optimized_kprobe(op) < 0)
		return;

	
	if (op->kp.flags & KPROBE_FLAG_OPTIMIZED)
		return;
	op->kp.flags |= KPROBE_FLAG_OPTIMIZED;

	if (!list_empty(&op->list))
		
		list_del_init(&op->list);
	else {
		list_add(&op->list, &optimizing_list);
		kick_kprobe_optimizer();
	}
}

static __kprobes void force_unoptimize_kprobe(struct optimized_kprobe *op)
{
	get_online_cpus();
	arch_unoptimize_kprobe(op);
	put_online_cpus();
	if (kprobe_disabled(&op->kp))
		arch_disarm_kprobe(&op->kp);
}

static __kprobes void unoptimize_kprobe(struct kprobe *p, bool force)
{
	struct optimized_kprobe *op;

	if (!kprobe_aggrprobe(p) || kprobe_disarmed(p))
		return; 

	op = container_of(p, struct optimized_kprobe, kp);
	if (!kprobe_optimized(p)) {
		
		if (force && !list_empty(&op->list)) {
			list_del_init(&op->list);
			force_unoptimize_kprobe(op);
		}
		return;
	}

	op->kp.flags &= ~KPROBE_FLAG_OPTIMIZED;
	if (!list_empty(&op->list)) {
		
		list_del_init(&op->list);
		return;
	}
	
	if (force)
		
		force_unoptimize_kprobe(op);
	else {
		list_add(&op->list, &unoptimizing_list);
		kick_kprobe_optimizer();
	}
}

static void reuse_unused_kprobe(struct kprobe *ap)
{
	struct optimized_kprobe *op;

	BUG_ON(!kprobe_unused(ap));
	op = container_of(ap, struct optimized_kprobe, kp);
	if (unlikely(list_empty(&op->list)))
		printk(KERN_WARNING "Warning: found a stray unused "
			"aggrprobe@%p\n", ap->addr);
	
	ap->flags &= ~KPROBE_FLAG_DISABLED;
	
	BUG_ON(!kprobe_optready(ap));
	optimize_kprobe(ap);
}

static void __kprobes kill_optimized_kprobe(struct kprobe *p)
{
	struct optimized_kprobe *op;

	op = container_of(p, struct optimized_kprobe, kp);
	if (!list_empty(&op->list))
		
		list_del_init(&op->list);

	op->kp.flags &= ~KPROBE_FLAG_OPTIMIZED;
	
	arch_remove_optimized_kprobe(op);
}

static __kprobes void prepare_optimized_kprobe(struct kprobe *p)
{
	struct optimized_kprobe *op;

	op = container_of(p, struct optimized_kprobe, kp);
	arch_prepare_optimized_kprobe(op);
}

static __kprobes struct kprobe *alloc_aggr_kprobe(struct kprobe *p)
{
	struct optimized_kprobe *op;

	op = kzalloc(sizeof(struct optimized_kprobe), GFP_KERNEL);
	if (!op)
		return NULL;

	INIT_LIST_HEAD(&op->list);
	op->kp.addr = p->addr;
	arch_prepare_optimized_kprobe(op);

	return &op->kp;
}

static void __kprobes init_aggr_kprobe(struct kprobe *ap, struct kprobe *p);

static __kprobes void try_to_optimize_kprobe(struct kprobe *p)
{
	struct kprobe *ap;
	struct optimized_kprobe *op;

	ap = alloc_aggr_kprobe(p);
	if (!ap)
		return;

	op = container_of(ap, struct optimized_kprobe, kp);
	if (!arch_prepared_optinsn(&op->optinsn)) {
		
		arch_remove_optimized_kprobe(op);
		kfree(op);
		return;
	}

	init_aggr_kprobe(ap, p);
	optimize_kprobe(ap);
}

#ifdef CONFIG_SYSCTL
static void __kprobes optimize_all_kprobes(void)
{
	struct hlist_head *head;
	struct hlist_node *node;
	struct kprobe *p;
	unsigned int i;

	
	if (kprobes_allow_optimization)
		return;

	kprobes_allow_optimization = true;
	for (i = 0; i < KPROBE_TABLE_SIZE; i++) {
		head = &kprobe_table[i];
		hlist_for_each_entry_rcu(p, node, head, hlist)
			if (!kprobe_disabled(p))
				optimize_kprobe(p);
	}
	printk(KERN_INFO "Kprobes globally optimized\n");
}

static void __kprobes unoptimize_all_kprobes(void)
{
	struct hlist_head *head;
	struct hlist_node *node;
	struct kprobe *p;
	unsigned int i;

	
	if (!kprobes_allow_optimization)
		return;

	kprobes_allow_optimization = false;
	for (i = 0; i < KPROBE_TABLE_SIZE; i++) {
		head = &kprobe_table[i];
		hlist_for_each_entry_rcu(p, node, head, hlist) {
			if (!kprobe_disabled(p))
				unoptimize_kprobe(p, false);
		}
	}
	
	wait_for_kprobe_optimizer();
	printk(KERN_INFO "Kprobes globally unoptimized\n");
}

int sysctl_kprobes_optimization;
int proc_kprobes_optimization_handler(struct ctl_table *table, int write,
				      void __user *buffer, size_t *length,
				      loff_t *ppos)
{
	int ret;

	mutex_lock(&kprobe_mutex);
	sysctl_kprobes_optimization = kprobes_allow_optimization ? 1 : 0;
	ret = proc_dointvec_minmax(table, write, buffer, length, ppos);

	if (sysctl_kprobes_optimization)
		optimize_all_kprobes();
	else
		unoptimize_all_kprobes();
	mutex_unlock(&kprobe_mutex);

	return ret;
}
#endif 

static void __kprobes __arm_kprobe(struct kprobe *p)
{
	struct kprobe *_p;

	
	_p = get_optimized_kprobe((unsigned long)p->addr);
	if (unlikely(_p))
		
		unoptimize_kprobe(_p, true);

	arch_arm_kprobe(p);
	optimize_kprobe(p);	
}

static void __kprobes __disarm_kprobe(struct kprobe *p, bool reopt)
{
	struct kprobe *_p;

	unoptimize_kprobe(p, false);	

	if (!kprobe_queued(p)) {
		arch_disarm_kprobe(p);
		
		_p = get_optimized_kprobe((unsigned long)p->addr);
		if (unlikely(_p) && reopt)
			optimize_kprobe(_p);
	}
	
}

#else 

#define optimize_kprobe(p)			do {} while (0)
#define unoptimize_kprobe(p, f)			do {} while (0)
#define kill_optimized_kprobe(p)		do {} while (0)
#define prepare_optimized_kprobe(p)		do {} while (0)
#define try_to_optimize_kprobe(p)		do {} while (0)
#define __arm_kprobe(p)				arch_arm_kprobe(p)
#define __disarm_kprobe(p, o)			arch_disarm_kprobe(p)
#define kprobe_disarmed(p)			kprobe_disabled(p)
#define wait_for_kprobe_optimizer()		do {} while (0)

static void reuse_unused_kprobe(struct kprobe *ap)
{
	printk(KERN_ERR "Error: There should be no unused kprobe here.\n");
	BUG_ON(kprobe_unused(ap));
}

static __kprobes void free_aggr_kprobe(struct kprobe *p)
{
	arch_remove_kprobe(p);
	kfree(p);
}

static __kprobes struct kprobe *alloc_aggr_kprobe(struct kprobe *p)
{
	return kzalloc(sizeof(struct kprobe), GFP_KERNEL);
}
#endif 

static void __kprobes arm_kprobe(struct kprobe *kp)
{
	mutex_lock(&text_mutex);
	__arm_kprobe(kp);
	mutex_unlock(&text_mutex);
}

static void __kprobes disarm_kprobe(struct kprobe *kp)
{
	
	mutex_lock(&text_mutex);
	__disarm_kprobe(kp, true);
	mutex_unlock(&text_mutex);
}

static int __kprobes aggr_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct kprobe *kp;

	list_for_each_entry_rcu(kp, &p->list, list) {
		if (kp->pre_handler && likely(!kprobe_disabled(kp))) {
			set_kprobe_instance(kp);
			if (kp->pre_handler(kp, regs))
				return 1;
		}
		reset_kprobe_instance();
	}
	return 0;
}

static void __kprobes aggr_post_handler(struct kprobe *p, struct pt_regs *regs,
					unsigned long flags)
{
	struct kprobe *kp;

	list_for_each_entry_rcu(kp, &p->list, list) {
		if (kp->post_handler && likely(!kprobe_disabled(kp))) {
			set_kprobe_instance(kp);
			kp->post_handler(kp, regs, flags);
			reset_kprobe_instance();
		}
	}
}

static int __kprobes aggr_fault_handler(struct kprobe *p, struct pt_regs *regs,
					int trapnr)
{
	struct kprobe *cur = __this_cpu_read(kprobe_instance);

	if (cur && cur->fault_handler) {
		if (cur->fault_handler(cur, regs, trapnr))
			return 1;
	}
	return 0;
}

static int __kprobes aggr_break_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct kprobe *cur = __this_cpu_read(kprobe_instance);
	int ret = 0;

	if (cur && cur->break_handler) {
		if (cur->break_handler(cur, regs))
			ret = 1;
	}
	reset_kprobe_instance();
	return ret;
}

void __kprobes kprobes_inc_nmissed_count(struct kprobe *p)
{
	struct kprobe *kp;
	if (!kprobe_aggrprobe(p)) {
		p->nmissed++;
	} else {
		list_for_each_entry_rcu(kp, &p->list, list)
			kp->nmissed++;
	}
	return;
}

void __kprobes recycle_rp_inst(struct kretprobe_instance *ri,
				struct hlist_head *head)
{
	struct kretprobe *rp = ri->rp;

	
	hlist_del(&ri->hlist);
	INIT_HLIST_NODE(&ri->hlist);
	if (likely(rp)) {
		raw_spin_lock(&rp->lock);
		hlist_add_head(&ri->hlist, &rp->free_instances);
		raw_spin_unlock(&rp->lock);
	} else
		
		hlist_add_head(&ri->hlist, head);
}

void __kprobes kretprobe_hash_lock(struct task_struct *tsk,
			 struct hlist_head **head, unsigned long *flags)
__acquires(hlist_lock)
{
	unsigned long hash = hash_ptr(tsk, KPROBE_HASH_BITS);
	raw_spinlock_t *hlist_lock;

	*head = &kretprobe_inst_table[hash];
	hlist_lock = kretprobe_table_lock_ptr(hash);
	raw_spin_lock_irqsave(hlist_lock, *flags);
}

static void __kprobes kretprobe_table_lock(unsigned long hash,
	unsigned long *flags)
__acquires(hlist_lock)
{
	raw_spinlock_t *hlist_lock = kretprobe_table_lock_ptr(hash);
	raw_spin_lock_irqsave(hlist_lock, *flags);
}

void __kprobes kretprobe_hash_unlock(struct task_struct *tsk,
	unsigned long *flags)
__releases(hlist_lock)
{
	unsigned long hash = hash_ptr(tsk, KPROBE_HASH_BITS);
	raw_spinlock_t *hlist_lock;

	hlist_lock = kretprobe_table_lock_ptr(hash);
	raw_spin_unlock_irqrestore(hlist_lock, *flags);
}

static void __kprobes kretprobe_table_unlock(unsigned long hash,
       unsigned long *flags)
__releases(hlist_lock)
{
	raw_spinlock_t *hlist_lock = kretprobe_table_lock_ptr(hash);
	raw_spin_unlock_irqrestore(hlist_lock, *flags);
}

void __kprobes kprobe_flush_task(struct task_struct *tk)
{
	struct kretprobe_instance *ri;
	struct hlist_head *head, empty_rp;
	struct hlist_node *node, *tmp;
	unsigned long hash, flags = 0;

	if (unlikely(!kprobes_initialized))
		
		return;

	INIT_HLIST_HEAD(&empty_rp);
	hash = hash_ptr(tk, KPROBE_HASH_BITS);
	head = &kretprobe_inst_table[hash];
	kretprobe_table_lock(hash, &flags);
	hlist_for_each_entry_safe(ri, node, tmp, head, hlist) {
		if (ri->task == tk)
			recycle_rp_inst(ri, &empty_rp);
	}
	kretprobe_table_unlock(hash, &flags);
	hlist_for_each_entry_safe(ri, node, tmp, &empty_rp, hlist) {
		hlist_del(&ri->hlist);
		kfree(ri);
	}
}

static inline void free_rp_inst(struct kretprobe *rp)
{
	struct kretprobe_instance *ri;
	struct hlist_node *pos, *next;

	hlist_for_each_entry_safe(ri, pos, next, &rp->free_instances, hlist) {
		hlist_del(&ri->hlist);
		kfree(ri);
	}
}

static void __kprobes cleanup_rp_inst(struct kretprobe *rp)
{
	unsigned long flags, hash;
	struct kretprobe_instance *ri;
	struct hlist_node *pos, *next;
	struct hlist_head *head;

	
	for (hash = 0; hash < KPROBE_TABLE_SIZE; hash++) {
		kretprobe_table_lock(hash, &flags);
		head = &kretprobe_inst_table[hash];
		hlist_for_each_entry_safe(ri, pos, next, head, hlist) {
			if (ri->rp == rp)
				ri->rp = NULL;
		}
		kretprobe_table_unlock(hash, &flags);
	}
	free_rp_inst(rp);
}

static int __kprobes add_new_kprobe(struct kprobe *ap, struct kprobe *p)
{
	BUG_ON(kprobe_gone(ap) || kprobe_gone(p));

	if (p->break_handler || p->post_handler)
		unoptimize_kprobe(ap, true);	

	if (p->break_handler) {
		if (ap->break_handler)
			return -EEXIST;
		list_add_tail_rcu(&p->list, &ap->list);
		ap->break_handler = aggr_break_handler;
	} else
		list_add_rcu(&p->list, &ap->list);
	if (p->post_handler && !ap->post_handler)
		ap->post_handler = aggr_post_handler;

	if (kprobe_disabled(ap) && !kprobe_disabled(p)) {
		ap->flags &= ~KPROBE_FLAG_DISABLED;
		if (!kprobes_all_disarmed)
			
			__arm_kprobe(ap);
	}
	return 0;
}

static void __kprobes init_aggr_kprobe(struct kprobe *ap, struct kprobe *p)
{
	
	copy_kprobe(p, ap);
	flush_insn_slot(ap);
	ap->addr = p->addr;
	ap->flags = p->flags & ~KPROBE_FLAG_OPTIMIZED;
	ap->pre_handler = aggr_pre_handler;
	ap->fault_handler = aggr_fault_handler;
	
	if (p->post_handler && !kprobe_gone(p))
		ap->post_handler = aggr_post_handler;
	if (p->break_handler && !kprobe_gone(p))
		ap->break_handler = aggr_break_handler;

	INIT_LIST_HEAD(&ap->list);
	INIT_HLIST_NODE(&ap->hlist);

	list_add_rcu(&p->list, &ap->list);
	hlist_replace_rcu(&p->hlist, &ap->hlist);
}

static int __kprobes register_aggr_kprobe(struct kprobe *orig_p,
					  struct kprobe *p)
{
	int ret = 0;
	struct kprobe *ap = orig_p;

	if (!kprobe_aggrprobe(orig_p)) {
		
		ap = alloc_aggr_kprobe(orig_p);
		if (!ap)
			return -ENOMEM;
		init_aggr_kprobe(ap, orig_p);
	} else if (kprobe_unused(ap))
		
		reuse_unused_kprobe(ap);

	if (kprobe_gone(ap)) {
		ret = arch_prepare_kprobe(ap);
		if (ret)
			return ret;

		
		prepare_optimized_kprobe(ap);

		ap->flags = (ap->flags & ~KPROBE_FLAG_GONE)
			    | KPROBE_FLAG_DISABLED;
	}

	
	copy_kprobe(ap, p);
	return add_new_kprobe(ap, p);
}

static int __kprobes in_kprobes_functions(unsigned long addr)
{
	struct kprobe_blackpoint *kb;

	if (addr >= (unsigned long)__kprobes_text_start &&
	    addr < (unsigned long)__kprobes_text_end)
		return -EINVAL;
	for (kb = kprobe_blacklist; kb->name != NULL; kb++) {
		if (kb->start_addr) {
			if (addr >= kb->start_addr &&
			    addr < (kb->start_addr + kb->range))
				return -EINVAL;
		}
	}
	return 0;
}

static kprobe_opcode_t __kprobes *kprobe_addr(struct kprobe *p)
{
	kprobe_opcode_t *addr = p->addr;

	if ((p->symbol_name && p->addr) ||
	    (!p->symbol_name && !p->addr))
		goto invalid;

	if (p->symbol_name) {
		kprobe_lookup_name(p->symbol_name, addr);
		if (!addr)
			return ERR_PTR(-ENOENT);
	}

	addr = (kprobe_opcode_t *)(((char *)addr) + p->offset);
	if (addr)
		return addr;

invalid:
	return ERR_PTR(-EINVAL);
}

static struct kprobe * __kprobes __get_valid_kprobe(struct kprobe *p)
{
	struct kprobe *ap, *list_p;

	ap = get_kprobe(p->addr);
	if (unlikely(!ap))
		return NULL;

	if (p != ap) {
		list_for_each_entry_rcu(list_p, &ap->list, list)
			if (list_p == p)
			
				goto valid;
		return NULL;
	}
valid:
	return ap;
}

static inline int check_kprobe_rereg(struct kprobe *p)
{
	int ret = 0;

	mutex_lock(&kprobe_mutex);
	if (__get_valid_kprobe(p))
		ret = -EINVAL;
	mutex_unlock(&kprobe_mutex);

	return ret;
}

int __kprobes register_kprobe(struct kprobe *p)
{
	int ret = 0;
	struct kprobe *old_p;
	struct module *probed_mod;
	kprobe_opcode_t *addr;

	addr = kprobe_addr(p);
	if (IS_ERR(addr))
		return PTR_ERR(addr);
	p->addr = addr;

	ret = check_kprobe_rereg(p);
	if (ret)
		return ret;

	jump_label_lock();
	preempt_disable();
	if (!kernel_text_address((unsigned long) p->addr) ||
	    in_kprobes_functions((unsigned long) p->addr) ||
	    ftrace_text_reserved(p->addr, p->addr) ||
	    jump_label_text_reserved(p->addr, p->addr)) {
		ret = -EINVAL;
		goto cannot_probe;
	}

	
	p->flags &= KPROBE_FLAG_DISABLED;

	probed_mod = __module_text_address((unsigned long) p->addr);
	if (probed_mod) {
		
		ret = -ENOENT;
		if (unlikely(!try_module_get(probed_mod)))
			goto cannot_probe;

		if (within_module_init((unsigned long)p->addr, probed_mod) &&
		    probed_mod->state != MODULE_STATE_COMING) {
			module_put(probed_mod);
			goto cannot_probe;
		}
		
	}
	preempt_enable();
	jump_label_unlock();

	p->nmissed = 0;
	INIT_LIST_HEAD(&p->list);
	mutex_lock(&kprobe_mutex);

	jump_label_lock(); 

	get_online_cpus();	
	mutex_lock(&text_mutex);

	old_p = get_kprobe(p->addr);
	if (old_p) {
		
		ret = register_aggr_kprobe(old_p, p);
		goto out;
	}

	ret = arch_prepare_kprobe(p);
	if (ret)
		goto out;

	INIT_HLIST_NODE(&p->hlist);
	hlist_add_head_rcu(&p->hlist,
		       &kprobe_table[hash_ptr(p->addr, KPROBE_HASH_BITS)]);

	if (!kprobes_all_disarmed && !kprobe_disabled(p))
		__arm_kprobe(p);

	
	try_to_optimize_kprobe(p);

out:
	mutex_unlock(&text_mutex);
	put_online_cpus();
	jump_label_unlock();
	mutex_unlock(&kprobe_mutex);

	if (probed_mod)
		module_put(probed_mod);

	return ret;

cannot_probe:
	preempt_enable();
	jump_label_unlock();
	return ret;
}
EXPORT_SYMBOL_GPL(register_kprobe);

static int __kprobes aggr_kprobe_disabled(struct kprobe *ap)
{
	struct kprobe *kp;

	list_for_each_entry_rcu(kp, &ap->list, list)
		if (!kprobe_disabled(kp))
			return 0;

	return 1;
}

static struct kprobe *__kprobes __disable_kprobe(struct kprobe *p)
{
	struct kprobe *orig_p;

	
	orig_p = __get_valid_kprobe(p);
	if (unlikely(orig_p == NULL))
		return NULL;

	if (!kprobe_disabled(p)) {
		
		if (p != orig_p)
			p->flags |= KPROBE_FLAG_DISABLED;

		
		if (p == orig_p || aggr_kprobe_disabled(orig_p)) {
			disarm_kprobe(orig_p);
			orig_p->flags |= KPROBE_FLAG_DISABLED;
		}
	}

	return orig_p;
}

static int __kprobes __unregister_kprobe_top(struct kprobe *p)
{
	struct kprobe *ap, *list_p;

	
	ap = __disable_kprobe(p);
	if (ap == NULL)
		return -EINVAL;

	if (ap == p)
		goto disarmed;

	
	WARN_ON(!kprobe_aggrprobe(ap));

	if (list_is_singular(&ap->list) && kprobe_disarmed(ap))
		goto disarmed;
	else {
		
		if (p->break_handler && !kprobe_gone(p))
			ap->break_handler = NULL;
		if (p->post_handler && !kprobe_gone(p)) {
			list_for_each_entry_rcu(list_p, &ap->list, list) {
				if ((list_p != p) && (list_p->post_handler))
					goto noclean;
			}
			ap->post_handler = NULL;
		}
noclean:
		list_del_rcu(&p->list);
		if (!kprobe_disabled(ap) && !kprobes_all_disarmed)
			optimize_kprobe(ap);
	}
	return 0;

disarmed:
	BUG_ON(!kprobe_disarmed(ap));
	hlist_del_rcu(&ap->hlist);
	return 0;
}

static void __kprobes __unregister_kprobe_bottom(struct kprobe *p)
{
	struct kprobe *ap;

	if (list_empty(&p->list))
		
		arch_remove_kprobe(p);
	else if (list_is_singular(&p->list)) {
		
		ap = list_entry(p->list.next, struct kprobe, list);
		list_del(&p->list);
		free_aggr_kprobe(ap);
	}
	
}

int __kprobes register_kprobes(struct kprobe **kps, int num)
{
	int i, ret = 0;

	if (num <= 0)
		return -EINVAL;
	for (i = 0; i < num; i++) {
		ret = register_kprobe(kps[i]);
		if (ret < 0) {
			if (i > 0)
				unregister_kprobes(kps, i);
			break;
		}
	}
	return ret;
}
EXPORT_SYMBOL_GPL(register_kprobes);

void __kprobes unregister_kprobe(struct kprobe *p)
{
	unregister_kprobes(&p, 1);
}
EXPORT_SYMBOL_GPL(unregister_kprobe);

void __kprobes unregister_kprobes(struct kprobe **kps, int num)
{
	int i;

	if (num <= 0)
		return;
	mutex_lock(&kprobe_mutex);
	for (i = 0; i < num; i++)
		if (__unregister_kprobe_top(kps[i]) < 0)
			kps[i]->addr = NULL;
	mutex_unlock(&kprobe_mutex);

	synchronize_sched();
	for (i = 0; i < num; i++)
		if (kps[i]->addr)
			__unregister_kprobe_bottom(kps[i]);
}
EXPORT_SYMBOL_GPL(unregister_kprobes);

static struct notifier_block kprobe_exceptions_nb = {
	.notifier_call = kprobe_exceptions_notify,
	.priority = 0x7fffffff 
};

unsigned long __weak arch_deref_entry_point(void *entry)
{
	return (unsigned long)entry;
}

int __kprobes register_jprobes(struct jprobe **jps, int num)
{
	struct jprobe *jp;
	int ret = 0, i;

	if (num <= 0)
		return -EINVAL;
	for (i = 0; i < num; i++) {
		unsigned long addr, offset;
		jp = jps[i];
		addr = arch_deref_entry_point(jp->entry);

		
		if (kallsyms_lookup_size_offset(addr, NULL, &offset) &&
		    offset == 0) {
			jp->kp.pre_handler = setjmp_pre_handler;
			jp->kp.break_handler = longjmp_break_handler;
			ret = register_kprobe(&jp->kp);
		} else
			ret = -EINVAL;

		if (ret < 0) {
			if (i > 0)
				unregister_jprobes(jps, i);
			break;
		}
	}
	return ret;
}
EXPORT_SYMBOL_GPL(register_jprobes);

int __kprobes register_jprobe(struct jprobe *jp)
{
	return register_jprobes(&jp, 1);
}
EXPORT_SYMBOL_GPL(register_jprobe);

void __kprobes unregister_jprobe(struct jprobe *jp)
{
	unregister_jprobes(&jp, 1);
}
EXPORT_SYMBOL_GPL(unregister_jprobe);

void __kprobes unregister_jprobes(struct jprobe **jps, int num)
{
	int i;

	if (num <= 0)
		return;
	mutex_lock(&kprobe_mutex);
	for (i = 0; i < num; i++)
		if (__unregister_kprobe_top(&jps[i]->kp) < 0)
			jps[i]->kp.addr = NULL;
	mutex_unlock(&kprobe_mutex);

	synchronize_sched();
	for (i = 0; i < num; i++) {
		if (jps[i]->kp.addr)
			__unregister_kprobe_bottom(&jps[i]->kp);
	}
}
EXPORT_SYMBOL_GPL(unregister_jprobes);

#ifdef CONFIG_KRETPROBES
static int __kprobes pre_handler_kretprobe(struct kprobe *p,
					   struct pt_regs *regs)
{
	struct kretprobe *rp = container_of(p, struct kretprobe, kp);
	unsigned long hash, flags = 0;
	struct kretprobe_instance *ri;

	
	hash = hash_ptr(current, KPROBE_HASH_BITS);
	raw_spin_lock_irqsave(&rp->lock, flags);
	if (!hlist_empty(&rp->free_instances)) {
		ri = hlist_entry(rp->free_instances.first,
				struct kretprobe_instance, hlist);
		hlist_del(&ri->hlist);
		raw_spin_unlock_irqrestore(&rp->lock, flags);

		ri->rp = rp;
		ri->task = current;

		if (rp->entry_handler && rp->entry_handler(ri, regs)) {
			raw_spin_lock_irqsave(&rp->lock, flags);
			hlist_add_head(&ri->hlist, &rp->free_instances);
			raw_spin_unlock_irqrestore(&rp->lock, flags);
			return 0;
		}

		arch_prepare_kretprobe(ri, regs);

		
		INIT_HLIST_NODE(&ri->hlist);
		kretprobe_table_lock(hash, &flags);
		hlist_add_head(&ri->hlist, &kretprobe_inst_table[hash]);
		kretprobe_table_unlock(hash, &flags);
	} else {
		rp->nmissed++;
		raw_spin_unlock_irqrestore(&rp->lock, flags);
	}
	return 0;
}

int __kprobes register_kretprobe(struct kretprobe *rp)
{
	int ret = 0;
	struct kretprobe_instance *inst;
	int i;
	void *addr;

	if (kretprobe_blacklist_size) {
		addr = kprobe_addr(&rp->kp);
		if (IS_ERR(addr))
			return PTR_ERR(addr);

		for (i = 0; kretprobe_blacklist[i].name != NULL; i++) {
			if (kretprobe_blacklist[i].addr == addr)
				return -EINVAL;
		}
	}

	rp->kp.pre_handler = pre_handler_kretprobe;
	rp->kp.post_handler = NULL;
	rp->kp.fault_handler = NULL;
	rp->kp.break_handler = NULL;

	
	if (rp->maxactive <= 0) {
#ifdef CONFIG_PREEMPT
		rp->maxactive = max_t(unsigned int, 10, 2*num_possible_cpus());
#else
		rp->maxactive = num_possible_cpus();
#endif
	}
	raw_spin_lock_init(&rp->lock);
	INIT_HLIST_HEAD(&rp->free_instances);
	for (i = 0; i < rp->maxactive; i++) {
		inst = kmalloc(sizeof(struct kretprobe_instance) +
			       rp->data_size, GFP_KERNEL);
		if (inst == NULL) {
			free_rp_inst(rp);
			return -ENOMEM;
		}
		INIT_HLIST_NODE(&inst->hlist);
		hlist_add_head(&inst->hlist, &rp->free_instances);
	}

	rp->nmissed = 0;
	
	ret = register_kprobe(&rp->kp);
	if (ret != 0)
		free_rp_inst(rp);
	return ret;
}
EXPORT_SYMBOL_GPL(register_kretprobe);

int __kprobes register_kretprobes(struct kretprobe **rps, int num)
{
	int ret = 0, i;

	if (num <= 0)
		return -EINVAL;
	for (i = 0; i < num; i++) {
		ret = register_kretprobe(rps[i]);
		if (ret < 0) {
			if (i > 0)
				unregister_kretprobes(rps, i);
			break;
		}
	}
	return ret;
}
EXPORT_SYMBOL_GPL(register_kretprobes);

void __kprobes unregister_kretprobe(struct kretprobe *rp)
{
	unregister_kretprobes(&rp, 1);
}
EXPORT_SYMBOL_GPL(unregister_kretprobe);

void __kprobes unregister_kretprobes(struct kretprobe **rps, int num)
{
	int i;

	if (num <= 0)
		return;
	mutex_lock(&kprobe_mutex);
	for (i = 0; i < num; i++)
		if (__unregister_kprobe_top(&rps[i]->kp) < 0)
			rps[i]->kp.addr = NULL;
	mutex_unlock(&kprobe_mutex);

	synchronize_sched();
	for (i = 0; i < num; i++) {
		if (rps[i]->kp.addr) {
			__unregister_kprobe_bottom(&rps[i]->kp);
			cleanup_rp_inst(rps[i]);
		}
	}
}
EXPORT_SYMBOL_GPL(unregister_kretprobes);

#else 
int __kprobes register_kretprobe(struct kretprobe *rp)
{
	return -ENOSYS;
}
EXPORT_SYMBOL_GPL(register_kretprobe);

int __kprobes register_kretprobes(struct kretprobe **rps, int num)
{
	return -ENOSYS;
}
EXPORT_SYMBOL_GPL(register_kretprobes);

void __kprobes unregister_kretprobe(struct kretprobe *rp)
{
}
EXPORT_SYMBOL_GPL(unregister_kretprobe);

void __kprobes unregister_kretprobes(struct kretprobe **rps, int num)
{
}
EXPORT_SYMBOL_GPL(unregister_kretprobes);

static int __kprobes pre_handler_kretprobe(struct kprobe *p,
					   struct pt_regs *regs)
{
	return 0;
}

#endif 

static void __kprobes kill_kprobe(struct kprobe *p)
{
	struct kprobe *kp;

	p->flags |= KPROBE_FLAG_GONE;
	if (kprobe_aggrprobe(p)) {
		list_for_each_entry_rcu(kp, &p->list, list)
			kp->flags |= KPROBE_FLAG_GONE;
		p->post_handler = NULL;
		p->break_handler = NULL;
		kill_optimized_kprobe(p);
	}
	arch_remove_kprobe(p);
}

int __kprobes disable_kprobe(struct kprobe *kp)
{
	int ret = 0;

	mutex_lock(&kprobe_mutex);

	
	if (__disable_kprobe(kp) == NULL)
		ret = -EINVAL;

	mutex_unlock(&kprobe_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(disable_kprobe);

int __kprobes enable_kprobe(struct kprobe *kp)
{
	int ret = 0;
	struct kprobe *p;

	mutex_lock(&kprobe_mutex);

	
	p = __get_valid_kprobe(kp);
	if (unlikely(p == NULL)) {
		ret = -EINVAL;
		goto out;
	}

	if (kprobe_gone(kp)) {
		
		ret = -EINVAL;
		goto out;
	}

	if (p != kp)
		kp->flags &= ~KPROBE_FLAG_DISABLED;

	if (!kprobes_all_disarmed && kprobe_disabled(p)) {
		p->flags &= ~KPROBE_FLAG_DISABLED;
		arm_kprobe(p);
	}
out:
	mutex_unlock(&kprobe_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(enable_kprobe);

void __kprobes dump_kprobe(struct kprobe *kp)
{
	printk(KERN_WARNING "Dumping kprobe:\n");
	printk(KERN_WARNING "Name: %s\nAddress: %p\nOffset: %x\n",
	       kp->symbol_name, kp->addr, kp->offset);
}

static int __kprobes kprobes_module_callback(struct notifier_block *nb,
					     unsigned long val, void *data)
{
	struct module *mod = data;
	struct hlist_head *head;
	struct hlist_node *node;
	struct kprobe *p;
	unsigned int i;
	int checkcore = (val == MODULE_STATE_GOING);

	if (val != MODULE_STATE_GOING && val != MODULE_STATE_LIVE)
		return NOTIFY_DONE;

	mutex_lock(&kprobe_mutex);
	for (i = 0; i < KPROBE_TABLE_SIZE; i++) {
		head = &kprobe_table[i];
		hlist_for_each_entry_rcu(p, node, head, hlist)
			if (within_module_init((unsigned long)p->addr, mod) ||
			    (checkcore &&
			     within_module_core((unsigned long)p->addr, mod))) {
				kill_kprobe(p);
			}
	}
	mutex_unlock(&kprobe_mutex);
	return NOTIFY_DONE;
}

static struct notifier_block kprobe_module_nb = {
	.notifier_call = kprobes_module_callback,
	.priority = 0
};

static int __init init_kprobes(void)
{
	int i, err = 0;
	unsigned long offset = 0, size = 0;
	char *modname, namebuf[128];
	const char *symbol_name;
	void *addr;
	struct kprobe_blackpoint *kb;

	
	
	for (i = 0; i < KPROBE_TABLE_SIZE; i++) {
		INIT_HLIST_HEAD(&kprobe_table[i]);
		INIT_HLIST_HEAD(&kretprobe_inst_table[i]);
		raw_spin_lock_init(&(kretprobe_table_locks[i].lock));
	}

	for (kb = kprobe_blacklist; kb->name != NULL; kb++) {
		kprobe_lookup_name(kb->name, addr);
		if (!addr)
			continue;

		kb->start_addr = (unsigned long)addr;
		symbol_name = kallsyms_lookup(kb->start_addr,
				&size, &offset, &modname, namebuf);
		if (!symbol_name)
			kb->range = 0;
		else
			kb->range = size;
	}

	if (kretprobe_blacklist_size) {
		
		for (i = 0; kretprobe_blacklist[i].name != NULL; i++) {
			kprobe_lookup_name(kretprobe_blacklist[i].name,
					   kretprobe_blacklist[i].addr);
			if (!kretprobe_blacklist[i].addr)
				printk("kretprobe: lookup failed: %s\n",
				       kretprobe_blacklist[i].name);
		}
	}

#if defined(CONFIG_OPTPROBES)
#if defined(__ARCH_WANT_KPROBES_INSN_SLOT)
	
	kprobe_optinsn_slots.insn_size = MAX_OPTINSN_SIZE;
#endif
	
	kprobes_allow_optimization = true;
#endif

	
	kprobes_all_disarmed = false;

	err = arch_init_kprobes();
	if (!err)
		err = register_die_notifier(&kprobe_exceptions_nb);
	if (!err)
		err = register_module_notifier(&kprobe_module_nb);

	kprobes_initialized = (err == 0);

	if (!err)
		init_test_probes();
	return err;
}

#ifdef CONFIG_DEBUG_FS
static void __kprobes report_probe(struct seq_file *pi, struct kprobe *p,
		const char *sym, int offset, char *modname, struct kprobe *pp)
{
	char *kprobe_type;

	if (p->pre_handler == pre_handler_kretprobe)
		kprobe_type = "r";
	else if (p->pre_handler == setjmp_pre_handler)
		kprobe_type = "j";
	else
		kprobe_type = "k";

	if (sym)
		seq_printf(pi, "%p  %s  %s+0x%x  %s ",
			p->addr, kprobe_type, sym, offset,
			(modname ? modname : " "));
	else
		seq_printf(pi, "%p  %s  %p ",
			p->addr, kprobe_type, p->addr);

	if (!pp)
		pp = p;
	seq_printf(pi, "%s%s%s\n",
		(kprobe_gone(p) ? "[GONE]" : ""),
		((kprobe_disabled(p) && !kprobe_gone(p)) ?  "[DISABLED]" : ""),
		(kprobe_optimized(pp) ? "[OPTIMIZED]" : ""));
}

static void __kprobes *kprobe_seq_start(struct seq_file *f, loff_t *pos)
{
	return (*pos < KPROBE_TABLE_SIZE) ? pos : NULL;
}

static void __kprobes *kprobe_seq_next(struct seq_file *f, void *v, loff_t *pos)
{
	(*pos)++;
	if (*pos >= KPROBE_TABLE_SIZE)
		return NULL;
	return pos;
}

static void __kprobes kprobe_seq_stop(struct seq_file *f, void *v)
{
	
}

static int __kprobes show_kprobe_addr(struct seq_file *pi, void *v)
{
	struct hlist_head *head;
	struct hlist_node *node;
	struct kprobe *p, *kp;
	const char *sym = NULL;
	unsigned int i = *(loff_t *) v;
	unsigned long offset = 0;
	char *modname, namebuf[128];

	head = &kprobe_table[i];
	preempt_disable();
	hlist_for_each_entry_rcu(p, node, head, hlist) {
		sym = kallsyms_lookup((unsigned long)p->addr, NULL,
					&offset, &modname, namebuf);
		if (kprobe_aggrprobe(p)) {
			list_for_each_entry_rcu(kp, &p->list, list)
				report_probe(pi, kp, sym, offset, modname, p);
		} else
			report_probe(pi, p, sym, offset, modname, NULL);
	}
	preempt_enable();
	return 0;
}

static const struct seq_operations kprobes_seq_ops = {
	.start = kprobe_seq_start,
	.next  = kprobe_seq_next,
	.stop  = kprobe_seq_stop,
	.show  = show_kprobe_addr
};

static int __kprobes kprobes_open(struct inode *inode, struct file *filp)
{
	return seq_open(filp, &kprobes_seq_ops);
}

static const struct file_operations debugfs_kprobes_operations = {
	.open           = kprobes_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = seq_release,
};

static void __kprobes arm_all_kprobes(void)
{
	struct hlist_head *head;
	struct hlist_node *node;
	struct kprobe *p;
	unsigned int i;

	mutex_lock(&kprobe_mutex);

	
	if (!kprobes_all_disarmed)
		goto already_enabled;

	
	mutex_lock(&text_mutex);
	for (i = 0; i < KPROBE_TABLE_SIZE; i++) {
		head = &kprobe_table[i];
		hlist_for_each_entry_rcu(p, node, head, hlist)
			if (!kprobe_disabled(p))
				__arm_kprobe(p);
	}
	mutex_unlock(&text_mutex);

	kprobes_all_disarmed = false;
	printk(KERN_INFO "Kprobes globally enabled\n");

already_enabled:
	mutex_unlock(&kprobe_mutex);
	return;
}

static void __kprobes disarm_all_kprobes(void)
{
	struct hlist_head *head;
	struct hlist_node *node;
	struct kprobe *p;
	unsigned int i;

	mutex_lock(&kprobe_mutex);

	
	if (kprobes_all_disarmed) {
		mutex_unlock(&kprobe_mutex);
		return;
	}

	kprobes_all_disarmed = true;
	printk(KERN_INFO "Kprobes globally disabled\n");

	mutex_lock(&text_mutex);
	for (i = 0; i < KPROBE_TABLE_SIZE; i++) {
		head = &kprobe_table[i];
		hlist_for_each_entry_rcu(p, node, head, hlist) {
			if (!arch_trampoline_kprobe(p) && !kprobe_disabled(p))
				__disarm_kprobe(p, false);
		}
	}
	mutex_unlock(&text_mutex);
	mutex_unlock(&kprobe_mutex);

	
	wait_for_kprobe_optimizer();
}

static ssize_t read_enabled_file_bool(struct file *file,
	       char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[3];

	if (!kprobes_all_disarmed)
		buf[0] = '1';
	else
		buf[0] = '0';
	buf[1] = '\n';
	buf[2] = 0x00;
	return simple_read_from_buffer(user_buf, count, ppos, buf, 2);
}

static ssize_t write_enabled_file_bool(struct file *file,
	       const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[32];
	size_t buf_size;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	switch (buf[0]) {
	case 'y':
	case 'Y':
	case '1':
		arm_all_kprobes();
		break;
	case 'n':
	case 'N':
	case '0':
		disarm_all_kprobes();
		break;
	}

	return count;
}

static const struct file_operations fops_kp = {
	.read =         read_enabled_file_bool,
	.write =        write_enabled_file_bool,
	.llseek =	default_llseek,
};

static int __kprobes debugfs_kprobe_init(void)
{
	struct dentry *dir, *file;
	unsigned int value = 1;

	dir = debugfs_create_dir("kprobes", NULL);
	if (!dir)
		return -ENOMEM;

	file = debugfs_create_file("list", 0444, dir, NULL,
				&debugfs_kprobes_operations);
	if (!file) {
		debugfs_remove(dir);
		return -ENOMEM;
	}

	file = debugfs_create_file("enabled", 0600, dir,
					&value, &fops_kp);
	if (!file) {
		debugfs_remove(dir);
		return -ENOMEM;
	}

	return 0;
}

late_initcall(debugfs_kprobe_init);
#endif 

module_init(init_kprobes);

EXPORT_SYMBOL_GPL(jprobe_return);
