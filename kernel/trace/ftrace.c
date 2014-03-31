/*
 * Infrastructure for profiling code inserted by 'gcc -pg'.
 *
 * Copyright (C) 2007-2008 Steven Rostedt <srostedt@redhat.com>
 * Copyright (C) 2004-2008 Ingo Molnar <mingo@redhat.com>
 *
 * Originally ported from the -rt patch by:
 *   Copyright (C) 2007 Arnaldo Carvalho de Melo <acme@redhat.com>
 *
 * Based on code in the latency_tracer, that is:
 *
 *  Copyright (C) 2004-2006 Ingo Molnar
 *  Copyright (C) 2004 William Lee Irwin III
 */

#include <linux/stop_machine.h>
#include <linux/clocksource.h>
#include <linux/kallsyms.h>
#include <linux/seq_file.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>
#include <linux/hardirq.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/bsearch.h>
#include <linux/module.h>
#include <linux/ftrace.h>
#include <linux/sysctl.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/sort.h>
#include <linux/list.h>
#include <linux/hash.h>
#include <linux/rcupdate.h>

#include <trace/events/sched.h>

#include <asm/setup.h>

#include "trace_output.h"
#include "trace_stat.h"

#define FTRACE_WARN_ON(cond)			\
	({					\
		int ___r = cond;		\
		if (WARN_ON(___r))		\
			ftrace_kill();		\
		___r;				\
	})

#define FTRACE_WARN_ON_ONCE(cond)		\
	({					\
		int ___r = cond;		\
		if (WARN_ON_ONCE(___r))		\
			ftrace_kill();		\
		___r;				\
	})

#define FTRACE_HASH_BITS 7
#define FTRACE_FUNC_HASHSIZE (1 << FTRACE_HASH_BITS)
#define FTRACE_HASH_DEFAULT_BITS 10
#define FTRACE_HASH_MAX_BITS 12

#define FL_GLOBAL_CONTROL_MASK (FTRACE_OPS_FL_GLOBAL | FTRACE_OPS_FL_CONTROL)

int ftrace_enabled __read_mostly;
static int last_ftrace_enabled;

int function_trace_stop;

LIST_HEAD(ftrace_pids);
struct ftrace_pid {
	struct list_head list;
	struct pid *pid;
};

static int ftrace_disabled __read_mostly;

static DEFINE_MUTEX(ftrace_lock);

static struct ftrace_ops ftrace_list_end __read_mostly = {
	.func		= ftrace_stub,
};

static struct ftrace_ops *ftrace_global_list __read_mostly = &ftrace_list_end;
static struct ftrace_ops *ftrace_control_list __read_mostly = &ftrace_list_end;
static struct ftrace_ops *ftrace_ops_list __read_mostly = &ftrace_list_end;
ftrace_func_t ftrace_trace_function __read_mostly = ftrace_stub;
static ftrace_func_t __ftrace_trace_function_delay __read_mostly = ftrace_stub;
ftrace_func_t __ftrace_trace_function __read_mostly = ftrace_stub;
ftrace_func_t ftrace_pid_function __read_mostly = ftrace_stub;
static struct ftrace_ops global_ops;
static struct ftrace_ops control_ops;

static void
ftrace_ops_list_func(unsigned long ip, unsigned long parent_ip);

static void ftrace_global_list_func(unsigned long ip,
				    unsigned long parent_ip)
{
	struct ftrace_ops *op;

	if (unlikely(trace_recursion_test(TRACE_GLOBAL_BIT)))
		return;

	trace_recursion_set(TRACE_GLOBAL_BIT);
	op = rcu_dereference_raw(ftrace_global_list); 
	while (op != &ftrace_list_end) {
		op->func(ip, parent_ip);
		op = rcu_dereference_raw(op->next); 
	};
	trace_recursion_clear(TRACE_GLOBAL_BIT);
}

static void ftrace_pid_func(unsigned long ip, unsigned long parent_ip)
{
	if (!test_tsk_trace_trace(current))
		return;

	ftrace_pid_function(ip, parent_ip);
}

static void set_ftrace_pid_function(ftrace_func_t func)
{
	
	if (func != ftrace_pid_func)
		ftrace_pid_function = func;
}

void clear_ftrace_function(void)
{
	ftrace_trace_function = ftrace_stub;
	__ftrace_trace_function = ftrace_stub;
	__ftrace_trace_function_delay = ftrace_stub;
	ftrace_pid_function = ftrace_stub;
}

#ifndef CONFIG_HAVE_FUNCTION_TRACE_MCOUNT_TEST
static void ftrace_test_stop_func(unsigned long ip, unsigned long parent_ip)
{
	if (function_trace_stop)
		return;

	__ftrace_trace_function(ip, parent_ip);
}
#endif

static void control_ops_disable_all(struct ftrace_ops *ops)
{
	int cpu;

	for_each_possible_cpu(cpu)
		*per_cpu_ptr(ops->disabled, cpu) = 1;
}

static int control_ops_alloc(struct ftrace_ops *ops)
{
	int __percpu *disabled;

	disabled = alloc_percpu(int);
	if (!disabled)
		return -ENOMEM;

	ops->disabled = disabled;
	control_ops_disable_all(ops);
	return 0;
}

static void control_ops_free(struct ftrace_ops *ops)
{
	free_percpu(ops->disabled);
}

static void update_global_ops(void)
{
	ftrace_func_t func;

	if (ftrace_global_list == &ftrace_list_end ||
	    ftrace_global_list->next == &ftrace_list_end)
		func = ftrace_global_list->func;
	else
		func = ftrace_global_list_func;

	
	if (!list_empty(&ftrace_pids)) {
		set_ftrace_pid_function(func);
		func = ftrace_pid_func;
	}

	global_ops.func = func;
}

static void update_ftrace_function(void)
{
	ftrace_func_t func;

	update_global_ops();

	if (ftrace_ops_list == &ftrace_list_end ||
	    (ftrace_ops_list->next == &ftrace_list_end &&
	     !(ftrace_ops_list->flags & FTRACE_OPS_FL_DYNAMIC)))
		func = ftrace_ops_list->func;
	else
		func = ftrace_ops_list_func;

#ifdef CONFIG_HAVE_FUNCTION_TRACE_MCOUNT_TEST
	ftrace_trace_function = func;
#else
#ifdef CONFIG_DYNAMIC_FTRACE
	
	__ftrace_trace_function_delay = func;
#else
	__ftrace_trace_function = func;
#endif
	ftrace_trace_function =
		(func == ftrace_stub) ? func : ftrace_test_stop_func;
#endif
}

static void add_ftrace_ops(struct ftrace_ops **list, struct ftrace_ops *ops)
{
	ops->next = *list;
	rcu_assign_pointer(*list, ops);
}

static int remove_ftrace_ops(struct ftrace_ops **list, struct ftrace_ops *ops)
{
	struct ftrace_ops **p;

	if (*list == ops && ops->next == &ftrace_list_end) {
		*list = &ftrace_list_end;
		return 0;
	}

	for (p = list; *p != &ftrace_list_end; p = &(*p)->next)
		if (*p == ops)
			break;

	if (*p != ops)
		return -1;

	*p = (*p)->next;
	return 0;
}

static void add_ftrace_list_ops(struct ftrace_ops **list,
				struct ftrace_ops *main_ops,
				struct ftrace_ops *ops)
{
	int first = *list == &ftrace_list_end;
	add_ftrace_ops(list, ops);
	if (first)
		add_ftrace_ops(&ftrace_ops_list, main_ops);
}

static int remove_ftrace_list_ops(struct ftrace_ops **list,
				  struct ftrace_ops *main_ops,
				  struct ftrace_ops *ops)
{
	int ret = remove_ftrace_ops(list, ops);
	if (!ret && *list == &ftrace_list_end)
		ret = remove_ftrace_ops(&ftrace_ops_list, main_ops);
	return ret;
}

static int __register_ftrace_function(struct ftrace_ops *ops)
{
	if (ftrace_disabled)
		return -ENODEV;

	if (FTRACE_WARN_ON(ops == &global_ops))
		return -EINVAL;

	if (WARN_ON(ops->flags & FTRACE_OPS_FL_ENABLED))
		return -EBUSY;

	
	if ((ops->flags & FL_GLOBAL_CONTROL_MASK) == FL_GLOBAL_CONTROL_MASK)
		return -EINVAL;

	if (!core_kernel_data((unsigned long)ops))
		ops->flags |= FTRACE_OPS_FL_DYNAMIC;

	if (ops->flags & FTRACE_OPS_FL_GLOBAL) {
		add_ftrace_list_ops(&ftrace_global_list, &global_ops, ops);
		ops->flags |= FTRACE_OPS_FL_ENABLED;
	} else if (ops->flags & FTRACE_OPS_FL_CONTROL) {
		if (control_ops_alloc(ops))
			return -ENOMEM;
		add_ftrace_list_ops(&ftrace_control_list, &control_ops, ops);
	} else
		add_ftrace_ops(&ftrace_ops_list, ops);

	if (ftrace_enabled)
		update_ftrace_function();

	return 0;
}

static int __unregister_ftrace_function(struct ftrace_ops *ops)
{
	int ret;

	if (ftrace_disabled)
		return -ENODEV;

	if (WARN_ON(!(ops->flags & FTRACE_OPS_FL_ENABLED)))
		return -EBUSY;

	if (FTRACE_WARN_ON(ops == &global_ops))
		return -EINVAL;

	if (ops->flags & FTRACE_OPS_FL_GLOBAL) {
		ret = remove_ftrace_list_ops(&ftrace_global_list,
					     &global_ops, ops);
		if (!ret)
			ops->flags &= ~FTRACE_OPS_FL_ENABLED;
	} else if (ops->flags & FTRACE_OPS_FL_CONTROL) {
		ret = remove_ftrace_list_ops(&ftrace_control_list,
					     &control_ops, ops);
		if (!ret) {
			synchronize_sched();
			control_ops_free(ops);
		}
	} else
		ret = remove_ftrace_ops(&ftrace_ops_list, ops);

	if (ret < 0)
		return ret;

	if (ftrace_enabled)
		update_ftrace_function();

	if (ops->flags & FTRACE_OPS_FL_DYNAMIC)
		synchronize_sched();

	return 0;
}

static void ftrace_update_pid_func(void)
{
	
	if (ftrace_trace_function == ftrace_stub)
		return;

	update_ftrace_function();
}

#ifdef CONFIG_FUNCTION_PROFILER
struct ftrace_profile {
	struct hlist_node		node;
	unsigned long			ip;
	unsigned long			counter;
#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	unsigned long long		time;
	unsigned long long		time_squared;
#endif
};

struct ftrace_profile_page {
	struct ftrace_profile_page	*next;
	unsigned long			index;
	struct ftrace_profile		records[];
};

struct ftrace_profile_stat {
	atomic_t			disabled;
	struct hlist_head		*hash;
	struct ftrace_profile_page	*pages;
	struct ftrace_profile_page	*start;
	struct tracer_stat		stat;
};

#define PROFILE_RECORDS_SIZE						\
	(PAGE_SIZE - offsetof(struct ftrace_profile_page, records))

#define PROFILES_PER_PAGE					\
	(PROFILE_RECORDS_SIZE / sizeof(struct ftrace_profile))

static int ftrace_profile_bits __read_mostly;
static int ftrace_profile_enabled __read_mostly;

static DEFINE_MUTEX(ftrace_profile_lock);

static DEFINE_PER_CPU(struct ftrace_profile_stat, ftrace_profile_stats);

#define FTRACE_PROFILE_HASH_SIZE 1024 

static void *
function_stat_next(void *v, int idx)
{
	struct ftrace_profile *rec = v;
	struct ftrace_profile_page *pg;

	pg = (struct ftrace_profile_page *)((unsigned long)rec & PAGE_MASK);

 again:
	if (idx != 0)
		rec++;

	if ((void *)rec >= (void *)&pg->records[pg->index]) {
		pg = pg->next;
		if (!pg)
			return NULL;
		rec = &pg->records[0];
		if (!rec->counter)
			goto again;
	}

	return rec;
}

static void *function_stat_start(struct tracer_stat *trace)
{
	struct ftrace_profile_stat *stat =
		container_of(trace, struct ftrace_profile_stat, stat);

	if (!stat || !stat->start)
		return NULL;

	return function_stat_next(&stat->start->records[0], 0);
}

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
static int function_stat_cmp(void *p1, void *p2)
{
	struct ftrace_profile *a = p1;
	struct ftrace_profile *b = p2;

	if (a->time < b->time)
		return -1;
	if (a->time > b->time)
		return 1;
	else
		return 0;
}
#else
static int function_stat_cmp(void *p1, void *p2)
{
	struct ftrace_profile *a = p1;
	struct ftrace_profile *b = p2;

	if (a->counter < b->counter)
		return -1;
	if (a->counter > b->counter)
		return 1;
	else
		return 0;
}
#endif

static int function_stat_headers(struct seq_file *m)
{
#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	seq_printf(m, "  Function                               "
		   "Hit    Time            Avg             s^2\n"
		      "  --------                               "
		   "---    ----            ---             ---\n");
#else
	seq_printf(m, "  Function                               Hit\n"
		      "  --------                               ---\n");
#endif
	return 0;
}

static int function_stat_show(struct seq_file *m, void *v)
{
	struct ftrace_profile *rec = v;
	char str[KSYM_SYMBOL_LEN];
	int ret = 0;
#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	static struct trace_seq s;
	unsigned long long avg;
	unsigned long long stddev;
#endif
	mutex_lock(&ftrace_profile_lock);

	
	if (unlikely(rec->counter == 0)) {
		ret = -EBUSY;
		goto out;
	}

	kallsyms_lookup(rec->ip, NULL, NULL, NULL, str);
	seq_printf(m, "  %-30.30s  %10lu", str, rec->counter);

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	seq_printf(m, "    ");
	avg = rec->time;
	do_div(avg, rec->counter);

	
	if (rec->counter <= 1)
		stddev = 0;
	else {
		stddev = rec->time_squared - rec->counter * avg * avg;
		do_div(stddev, (rec->counter - 1) * 1000);
	}

	trace_seq_init(&s);
	trace_print_graph_duration(rec->time, &s);
	trace_seq_puts(&s, "    ");
	trace_print_graph_duration(avg, &s);
	trace_seq_puts(&s, "    ");
	trace_print_graph_duration(stddev, &s);
	trace_print_seq(m, &s);
#endif
	seq_putc(m, '\n');
out:
	mutex_unlock(&ftrace_profile_lock);

	return ret;
}

static void ftrace_profile_reset(struct ftrace_profile_stat *stat)
{
	struct ftrace_profile_page *pg;

	pg = stat->pages = stat->start;

	while (pg) {
		memset(pg->records, 0, PROFILE_RECORDS_SIZE);
		pg->index = 0;
		pg = pg->next;
	}

	memset(stat->hash, 0,
	       FTRACE_PROFILE_HASH_SIZE * sizeof(struct hlist_head));
}

int ftrace_profile_pages_init(struct ftrace_profile_stat *stat)
{
	struct ftrace_profile_page *pg;
	int functions;
	int pages;
	int i;

	
	if (stat->pages)
		return 0;

	stat->pages = (void *)get_zeroed_page(GFP_KERNEL);
	if (!stat->pages)
		return -ENOMEM;

#ifdef CONFIG_DYNAMIC_FTRACE
	functions = ftrace_update_tot_cnt;
#else
	functions = 20000;
#endif

	pg = stat->start = stat->pages;

	pages = DIV_ROUND_UP(functions, PROFILES_PER_PAGE);

	for (i = 0; i < pages; i++) {
		pg->next = (void *)get_zeroed_page(GFP_KERNEL);
		if (!pg->next)
			goto out_free;
		pg = pg->next;
	}

	return 0;

 out_free:
	pg = stat->start;
	while (pg) {
		unsigned long tmp = (unsigned long)pg;

		pg = pg->next;
		free_page(tmp);
	}

	free_page((unsigned long)stat->pages);
	stat->pages = NULL;
	stat->start = NULL;

	return -ENOMEM;
}

static int ftrace_profile_init_cpu(int cpu)
{
	struct ftrace_profile_stat *stat;
	int size;

	stat = &per_cpu(ftrace_profile_stats, cpu);

	if (stat->hash) {
		
		ftrace_profile_reset(stat);
		return 0;
	}

	size = FTRACE_PROFILE_HASH_SIZE;

	stat->hash = kzalloc(sizeof(struct hlist_head) * size, GFP_KERNEL);

	if (!stat->hash)
		return -ENOMEM;

	if (!ftrace_profile_bits) {
		size--;

		for (; size; size >>= 1)
			ftrace_profile_bits++;
	}

	
	if (ftrace_profile_pages_init(stat) < 0) {
		kfree(stat->hash);
		stat->hash = NULL;
		return -ENOMEM;
	}

	return 0;
}

static int ftrace_profile_init(void)
{
	int cpu;
	int ret = 0;

	for_each_online_cpu(cpu) {
		ret = ftrace_profile_init_cpu(cpu);
		if (ret)
			break;
	}

	return ret;
}

static struct ftrace_profile *
ftrace_find_profiled_func(struct ftrace_profile_stat *stat, unsigned long ip)
{
	struct ftrace_profile *rec;
	struct hlist_head *hhd;
	struct hlist_node *n;
	unsigned long key;

	key = hash_long(ip, ftrace_profile_bits);
	hhd = &stat->hash[key];

	if (hlist_empty(hhd))
		return NULL;

	hlist_for_each_entry_rcu(rec, n, hhd, node) {
		if (rec->ip == ip)
			return rec;
	}

	return NULL;
}

static void ftrace_add_profile(struct ftrace_profile_stat *stat,
			       struct ftrace_profile *rec)
{
	unsigned long key;

	key = hash_long(rec->ip, ftrace_profile_bits);
	hlist_add_head_rcu(&rec->node, &stat->hash[key]);
}

static struct ftrace_profile *
ftrace_profile_alloc(struct ftrace_profile_stat *stat, unsigned long ip)
{
	struct ftrace_profile *rec = NULL;

	
	if (atomic_inc_return(&stat->disabled) != 1)
		goto out;

	rec = ftrace_find_profiled_func(stat, ip);
	if (rec)
		goto out;

	if (stat->pages->index == PROFILES_PER_PAGE) {
		if (!stat->pages->next)
			goto out;
		stat->pages = stat->pages->next;
	}

	rec = &stat->pages->records[stat->pages->index++];
	rec->ip = ip;
	ftrace_add_profile(stat, rec);

 out:
	atomic_dec(&stat->disabled);

	return rec;
}

static void
function_profile_call(unsigned long ip, unsigned long parent_ip)
{
	struct ftrace_profile_stat *stat;
	struct ftrace_profile *rec;
	unsigned long flags;

	if (!ftrace_profile_enabled)
		return;

	local_irq_save(flags);

	stat = &__get_cpu_var(ftrace_profile_stats);
	if (!stat->hash || !ftrace_profile_enabled)
		goto out;

	rec = ftrace_find_profiled_func(stat, ip);
	if (!rec) {
		rec = ftrace_profile_alloc(stat, ip);
		if (!rec)
			goto out;
	}

	rec->counter++;
 out:
	local_irq_restore(flags);
}

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
static int profile_graph_entry(struct ftrace_graph_ent *trace)
{
	function_profile_call(trace->func, 0);
	return 1;
}

static void profile_graph_return(struct ftrace_graph_ret *trace)
{
	struct ftrace_profile_stat *stat;
	unsigned long long calltime;
	struct ftrace_profile *rec;
	unsigned long flags;

	local_irq_save(flags);
	stat = &__get_cpu_var(ftrace_profile_stats);
	if (!stat->hash || !ftrace_profile_enabled)
		goto out;

	
	if (!trace->calltime)
		goto out;

	calltime = trace->rettime - trace->calltime;

	if (!(trace_flags & TRACE_ITER_GRAPH_TIME)) {
		int index;

		index = trace->depth;

		
		if (index)
			current->ret_stack[index - 1].subtime += calltime;

		if (current->ret_stack[index].subtime < calltime)
			calltime -= current->ret_stack[index].subtime;
		else
			calltime = 0;
	}

	rec = ftrace_find_profiled_func(stat, trace->func);
	if (rec) {
		rec->time += calltime;
		rec->time_squared += calltime * calltime;
	}

 out:
	local_irq_restore(flags);
}

static int register_ftrace_profiler(void)
{
	return register_ftrace_graph(&profile_graph_return,
				     &profile_graph_entry);
}

static void unregister_ftrace_profiler(void)
{
	unregister_ftrace_graph();
}
#else
static struct ftrace_ops ftrace_profile_ops __read_mostly = {
	.func		= function_profile_call,
};

static int register_ftrace_profiler(void)
{
	return register_ftrace_function(&ftrace_profile_ops);
}

static void unregister_ftrace_profiler(void)
{
	unregister_ftrace_function(&ftrace_profile_ops);
}
#endif 

static ssize_t
ftrace_profile_write(struct file *filp, const char __user *ubuf,
		     size_t cnt, loff_t *ppos)
{
	unsigned long val;
	int ret;

	ret = kstrtoul_from_user(ubuf, cnt, 10, &val);
	if (ret)
		return ret;

	val = !!val;

	mutex_lock(&ftrace_profile_lock);
	if (ftrace_profile_enabled ^ val) {
		if (val) {
			ret = ftrace_profile_init();
			if (ret < 0) {
				cnt = ret;
				goto out;
			}

			ret = register_ftrace_profiler();
			if (ret < 0) {
				cnt = ret;
				goto out;
			}
			ftrace_profile_enabled = 1;
		} else {
			ftrace_profile_enabled = 0;
			unregister_ftrace_profiler();
		}
	}
 out:
	mutex_unlock(&ftrace_profile_lock);

	*ppos += cnt;

	return cnt;
}

static ssize_t
ftrace_profile_read(struct file *filp, char __user *ubuf,
		     size_t cnt, loff_t *ppos)
{
	char buf[64];		
	int r;

	r = sprintf(buf, "%u\n", ftrace_profile_enabled);
	return simple_read_from_buffer(ubuf, cnt, ppos, buf, r);
}

static const struct file_operations ftrace_profile_fops = {
	.open		= tracing_open_generic,
	.read		= ftrace_profile_read,
	.write		= ftrace_profile_write,
	.llseek		= default_llseek,
};

static struct tracer_stat function_stats __initdata = {
	.name		= "functions",
	.stat_start	= function_stat_start,
	.stat_next	= function_stat_next,
	.stat_cmp	= function_stat_cmp,
	.stat_headers	= function_stat_headers,
	.stat_show	= function_stat_show
};

static __init void ftrace_profile_debugfs(struct dentry *d_tracer)
{
	struct ftrace_profile_stat *stat;
	struct dentry *entry;
	char *name;
	int ret;
	int cpu;

	for_each_possible_cpu(cpu) {
		stat = &per_cpu(ftrace_profile_stats, cpu);

		
		name = kmalloc(32, GFP_KERNEL);
		if (!name) {
			WARN(1,
			     "Could not allocate stat file for cpu %d\n",
			     cpu);
			return;
		}
		stat->stat = function_stats;
		snprintf(name, 32, "function%d", cpu);
		stat->stat.name = name;
		ret = register_stat_tracer(&stat->stat);
		if (ret) {
			WARN(1,
			     "Could not register function stat for cpu %d\n",
			     cpu);
			kfree(name);
			return;
		}
	}

	entry = debugfs_create_file("function_profile_enabled", 0644,
				    d_tracer, NULL, &ftrace_profile_fops);
	if (!entry)
		pr_warning("Could not create debugfs "
			   "'function_profile_enabled' entry\n");
}

#else 
static __init void ftrace_profile_debugfs(struct dentry *d_tracer)
{
}
#endif 

static struct pid * const ftrace_swapper_pid = &init_struct_pid;

#ifdef CONFIG_DYNAMIC_FTRACE

#ifndef CONFIG_FTRACE_MCOUNT_RECORD
# error Dynamic ftrace depends on MCOUNT_RECORD
#endif

static struct hlist_head ftrace_func_hash[FTRACE_FUNC_HASHSIZE] __read_mostly;

struct ftrace_func_probe {
	struct hlist_node	node;
	struct ftrace_probe_ops	*ops;
	unsigned long		flags;
	unsigned long		ip;
	void			*data;
	struct rcu_head		rcu;
};

struct ftrace_func_entry {
	struct hlist_node hlist;
	unsigned long ip;
};

struct ftrace_hash {
	unsigned long		size_bits;
	struct hlist_head	*buckets;
	unsigned long		count;
	struct rcu_head		rcu;
};

static const struct hlist_head empty_buckets[1];
static const struct ftrace_hash empty_hash = {
	.buckets = (struct hlist_head *)empty_buckets,
};
#define EMPTY_HASH	((struct ftrace_hash *)&empty_hash)

static struct ftrace_ops global_ops = {
	.func			= ftrace_stub,
	.notrace_hash		= EMPTY_HASH,
	.filter_hash		= EMPTY_HASH,
};

static DEFINE_MUTEX(ftrace_regex_lock);

struct ftrace_page {
	struct ftrace_page	*next;
	struct dyn_ftrace	*records;
	int			index;
	int			size;
};

static struct ftrace_page *ftrace_new_pgs;

#define ENTRY_SIZE sizeof(struct dyn_ftrace)
#define ENTRIES_PER_PAGE (PAGE_SIZE / ENTRY_SIZE)

#define NR_TO_INIT		10000

static struct ftrace_page	*ftrace_pages_start;
static struct ftrace_page	*ftrace_pages;

static bool ftrace_hash_empty(struct ftrace_hash *hash)
{
	return !hash || !hash->count;
}

static struct ftrace_func_entry *
ftrace_lookup_ip(struct ftrace_hash *hash, unsigned long ip)
{
	unsigned long key;
	struct ftrace_func_entry *entry;
	struct hlist_head *hhd;
	struct hlist_node *n;

	if (ftrace_hash_empty(hash))
		return NULL;

	if (hash->size_bits > 0)
		key = hash_long(ip, hash->size_bits);
	else
		key = 0;

	hhd = &hash->buckets[key];

	hlist_for_each_entry_rcu(entry, n, hhd, hlist) {
		if (entry->ip == ip)
			return entry;
	}
	return NULL;
}

static void __add_hash_entry(struct ftrace_hash *hash,
			     struct ftrace_func_entry *entry)
{
	struct hlist_head *hhd;
	unsigned long key;

	if (hash->size_bits)
		key = hash_long(entry->ip, hash->size_bits);
	else
		key = 0;

	hhd = &hash->buckets[key];
	hlist_add_head(&entry->hlist, hhd);
	hash->count++;
}

static int add_hash_entry(struct ftrace_hash *hash, unsigned long ip)
{
	struct ftrace_func_entry *entry;

	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;

	entry->ip = ip;
	__add_hash_entry(hash, entry);

	return 0;
}

static void
free_hash_entry(struct ftrace_hash *hash,
		  struct ftrace_func_entry *entry)
{
	hlist_del(&entry->hlist);
	kfree(entry);
	hash->count--;
}

static void
remove_hash_entry(struct ftrace_hash *hash,
		  struct ftrace_func_entry *entry)
{
	hlist_del(&entry->hlist);
	hash->count--;
}

static void ftrace_hash_clear(struct ftrace_hash *hash)
{
	struct hlist_head *hhd;
	struct hlist_node *tp, *tn;
	struct ftrace_func_entry *entry;
	int size = 1 << hash->size_bits;
	int i;

	if (!hash->count)
		return;

	for (i = 0; i < size; i++) {
		hhd = &hash->buckets[i];
		hlist_for_each_entry_safe(entry, tp, tn, hhd, hlist)
			free_hash_entry(hash, entry);
	}
	FTRACE_WARN_ON(hash->count);
}

static void free_ftrace_hash(struct ftrace_hash *hash)
{
	if (!hash || hash == EMPTY_HASH)
		return;
	ftrace_hash_clear(hash);
	kfree(hash->buckets);
	kfree(hash);
}

static void __free_ftrace_hash_rcu(struct rcu_head *rcu)
{
	struct ftrace_hash *hash;

	hash = container_of(rcu, struct ftrace_hash, rcu);
	free_ftrace_hash(hash);
}

static void free_ftrace_hash_rcu(struct ftrace_hash *hash)
{
	if (!hash || hash == EMPTY_HASH)
		return;
	call_rcu_sched(&hash->rcu, __free_ftrace_hash_rcu);
}

void ftrace_free_filter(struct ftrace_ops *ops)
{
	free_ftrace_hash(ops->filter_hash);
	free_ftrace_hash(ops->notrace_hash);
}

static struct ftrace_hash *alloc_ftrace_hash(int size_bits)
{
	struct ftrace_hash *hash;
	int size;

	hash = kzalloc(sizeof(*hash), GFP_KERNEL);
	if (!hash)
		return NULL;

	size = 1 << size_bits;
	hash->buckets = kcalloc(size, sizeof(*hash->buckets), GFP_KERNEL);

	if (!hash->buckets) {
		kfree(hash);
		return NULL;
	}

	hash->size_bits = size_bits;

	return hash;
}

static struct ftrace_hash *
alloc_and_copy_ftrace_hash(int size_bits, struct ftrace_hash *hash)
{
	struct ftrace_func_entry *entry;
	struct ftrace_hash *new_hash;
	struct hlist_node *tp;
	int size;
	int ret;
	int i;

	new_hash = alloc_ftrace_hash(size_bits);
	if (!new_hash)
		return NULL;

	
	if (ftrace_hash_empty(hash))
		return new_hash;

	size = 1 << hash->size_bits;
	for (i = 0; i < size; i++) {
		hlist_for_each_entry(entry, tp, &hash->buckets[i], hlist) {
			ret = add_hash_entry(new_hash, entry->ip);
			if (ret < 0)
				goto free_hash;
		}
	}

	FTRACE_WARN_ON(new_hash->count != hash->count);

	return new_hash;

 free_hash:
	free_ftrace_hash(new_hash);
	return NULL;
}

static void
ftrace_hash_rec_disable(struct ftrace_ops *ops, int filter_hash);
static void
ftrace_hash_rec_enable(struct ftrace_ops *ops, int filter_hash);

static int
ftrace_hash_move(struct ftrace_ops *ops, int enable,
		 struct ftrace_hash **dst, struct ftrace_hash *src)
{
	struct ftrace_func_entry *entry;
	struct hlist_node *tp, *tn;
	struct hlist_head *hhd;
	struct ftrace_hash *old_hash;
	struct ftrace_hash *new_hash;
	unsigned long key;
	int size = src->count;
	int bits = 0;
	int ret;
	int i;

	ftrace_hash_rec_disable(ops, enable);

	if (!src->count) {
		free_ftrace_hash_rcu(*dst);
		rcu_assign_pointer(*dst, EMPTY_HASH);
		
		ret = 0;
		goto out;
	}

	for (size /= 2; size; size >>= 1)
		bits++;

	
	if (bits > FTRACE_HASH_MAX_BITS)
		bits = FTRACE_HASH_MAX_BITS;

	ret = -ENOMEM;
	new_hash = alloc_ftrace_hash(bits);
	if (!new_hash)
		goto out;

	size = 1 << src->size_bits;
	for (i = 0; i < size; i++) {
		hhd = &src->buckets[i];
		hlist_for_each_entry_safe(entry, tp, tn, hhd, hlist) {
			if (bits > 0)
				key = hash_long(entry->ip, bits);
			else
				key = 0;
			remove_hash_entry(src, entry);
			__add_hash_entry(new_hash, entry);
		}
	}

	old_hash = *dst;
	rcu_assign_pointer(*dst, new_hash);
	free_ftrace_hash_rcu(old_hash);

	ret = 0;
 out:
	ftrace_hash_rec_enable(ops, enable);

	return ret;
}

static int
ftrace_ops_test(struct ftrace_ops *ops, unsigned long ip)
{
	struct ftrace_hash *filter_hash;
	struct ftrace_hash *notrace_hash;
	int ret;

	filter_hash = rcu_dereference_raw(ops->filter_hash);
	notrace_hash = rcu_dereference_raw(ops->notrace_hash);

	if ((ftrace_hash_empty(filter_hash) ||
	     ftrace_lookup_ip(filter_hash, ip)) &&
	    (ftrace_hash_empty(notrace_hash) ||
	     !ftrace_lookup_ip(notrace_hash, ip)))
		ret = 1;
	else
		ret = 0;

	return ret;
}

#define do_for_each_ftrace_rec(pg, rec)					\
	for (pg = ftrace_pages_start; pg; pg = pg->next) {		\
		int _____i;						\
		for (_____i = 0; _____i < pg->index; _____i++) {	\
			rec = &pg->records[_____i];

#define while_for_each_ftrace_rec()		\
		}				\
	}


static int ftrace_cmp_recs(const void *a, const void *b)
{
	const struct dyn_ftrace *reca = a;
	const struct dyn_ftrace *recb = b;

	if (reca->ip > recb->ip)
		return 1;
	if (reca->ip < recb->ip)
		return -1;
	return 0;
}

int ftrace_location(unsigned long ip)
{
	struct ftrace_page *pg;
	struct dyn_ftrace *rec;
	struct dyn_ftrace key;

	key.ip = ip;

	for (pg = ftrace_pages_start; pg; pg = pg->next) {
		rec = bsearch(&key, pg->records, pg->index,
			      sizeof(struct dyn_ftrace),
			      ftrace_cmp_recs);
		if (rec)
			return 1;
	}

	return 0;
}

static void __ftrace_hash_rec_update(struct ftrace_ops *ops,
				     int filter_hash,
				     bool inc)
{
	struct ftrace_hash *hash;
	struct ftrace_hash *other_hash;
	struct ftrace_page *pg;
	struct dyn_ftrace *rec;
	int count = 0;
	int all = 0;

	
	if (!(ops->flags & FTRACE_OPS_FL_ENABLED))
		return;

	if (filter_hash) {
		hash = ops->filter_hash;
		other_hash = ops->notrace_hash;
		if (ftrace_hash_empty(hash))
			all = 1;
	} else {
		inc = !inc;
		hash = ops->notrace_hash;
		other_hash = ops->filter_hash;
		if (ftrace_hash_empty(hash))
			return;
	}

	do_for_each_ftrace_rec(pg, rec) {
		int in_other_hash = 0;
		int in_hash = 0;
		int match = 0;

		if (all) {
			if (!other_hash || !ftrace_lookup_ip(other_hash, rec->ip))
				match = 1;
		} else {
			in_hash = !!ftrace_lookup_ip(hash, rec->ip);
			in_other_hash = !!ftrace_lookup_ip(other_hash, rec->ip);

			if (filter_hash && in_hash && !in_other_hash)
				match = 1;
			else if (!filter_hash && in_hash &&
				 (in_other_hash || ftrace_hash_empty(other_hash)))
				match = 1;
		}
		if (!match)
			continue;

		if (inc) {
			rec->flags++;
			if (FTRACE_WARN_ON((rec->flags & ~FTRACE_FL_MASK) == FTRACE_REF_MAX))
				return;
		} else {
			if (FTRACE_WARN_ON((rec->flags & ~FTRACE_FL_MASK) == 0))
				return;
			rec->flags--;
		}
		count++;
		
		if (!all && count == hash->count)
			return;
	} while_for_each_ftrace_rec();
}

static void ftrace_hash_rec_disable(struct ftrace_ops *ops,
				    int filter_hash)
{
	__ftrace_hash_rec_update(ops, filter_hash, 0);
}

static void ftrace_hash_rec_enable(struct ftrace_ops *ops,
				   int filter_hash)
{
	__ftrace_hash_rec_update(ops, filter_hash, 1);
}

static struct dyn_ftrace *ftrace_alloc_dyn_node(unsigned long ip)
{
	if (ftrace_pages->index == ftrace_pages->size) {
		
		if (WARN_ON(!ftrace_pages->next))
			return NULL;
		ftrace_pages = ftrace_pages->next;
	}

	return &ftrace_pages->records[ftrace_pages->index++];
}

static struct dyn_ftrace *
ftrace_record_ip(unsigned long ip)
{
	struct dyn_ftrace *rec;

	if (ftrace_disabled)
		return NULL;

	rec = ftrace_alloc_dyn_node(ip);
	if (!rec)
		return NULL;

	rec->ip = ip;

	return rec;
}

static void print_ip_ins(const char *fmt, unsigned char *p)
{
	int i;

	printk(KERN_CONT "%s", fmt);

	for (i = 0; i < MCOUNT_INSN_SIZE; i++)
		printk(KERN_CONT "%s%02x", i ? ":" : "", p[i]);
}

void ftrace_bug(int failed, unsigned long ip)
{
	switch (failed) {
	case -EFAULT:
		FTRACE_WARN_ON_ONCE(1);
		pr_info("ftrace faulted on modifying ");
		print_ip_sym(ip);
		break;
	case -EINVAL:
		FTRACE_WARN_ON_ONCE(1);
		pr_info("ftrace failed to modify ");
		print_ip_sym(ip);
		print_ip_ins(" actual: ", (unsigned char *)ip);
		printk(KERN_CONT "\n");
		break;
	case -EPERM:
		FTRACE_WARN_ON_ONCE(1);
		pr_info("ftrace faulted on writing ");
		print_ip_sym(ip);
		break;
	default:
		FTRACE_WARN_ON_ONCE(1);
		pr_info("ftrace faulted on unknown error ");
		print_ip_sym(ip);
	}
}


int ftrace_text_reserved(void *start, void *end)
{
	struct dyn_ftrace *rec;
	struct ftrace_page *pg;

	do_for_each_ftrace_rec(pg, rec) {
		if (rec->ip <= (unsigned long)end &&
		    rec->ip + MCOUNT_INSN_SIZE > (unsigned long)start)
			return 1;
	} while_for_each_ftrace_rec();
	return 0;
}

static int ftrace_check_record(struct dyn_ftrace *rec, int enable, int update)
{
	unsigned long flag = 0UL;

	if (enable && (rec->flags & ~FTRACE_FL_MASK))
		flag = FTRACE_FL_ENABLED;

	
	if ((rec->flags & FTRACE_FL_ENABLED) == flag)
		return FTRACE_UPDATE_IGNORE;

	if (flag) {
		if (update)
			rec->flags |= FTRACE_FL_ENABLED;
		return FTRACE_UPDATE_MAKE_CALL;
	}

	if (update)
		rec->flags &= ~FTRACE_FL_ENABLED;

	return FTRACE_UPDATE_MAKE_NOP;
}

int ftrace_update_record(struct dyn_ftrace *rec, int enable)
{
	return ftrace_check_record(rec, enable, 1);
}

int ftrace_test_record(struct dyn_ftrace *rec, int enable)
{
	return ftrace_check_record(rec, enable, 0);
}

static int
__ftrace_replace_code(struct dyn_ftrace *rec, int enable)
{
	unsigned long ftrace_addr;
	int ret;

	ftrace_addr = (unsigned long)FTRACE_ADDR;

	ret = ftrace_update_record(rec, enable);

	switch (ret) {
	case FTRACE_UPDATE_IGNORE:
		return 0;

	case FTRACE_UPDATE_MAKE_CALL:
		return ftrace_make_call(rec, ftrace_addr);

	case FTRACE_UPDATE_MAKE_NOP:
		return ftrace_make_nop(NULL, rec, ftrace_addr);
	}

	return -1; 
}

static void ftrace_replace_code(int update)
{
	struct dyn_ftrace *rec;
	struct ftrace_page *pg;
	int failed;

	if (unlikely(ftrace_disabled))
		return;

	do_for_each_ftrace_rec(pg, rec) {
		failed = __ftrace_replace_code(rec, update);
		if (failed) {
			ftrace_bug(failed, rec->ip);
			
			return;
		}
	} while_for_each_ftrace_rec();
}

struct ftrace_rec_iter {
	struct ftrace_page	*pg;
	int			index;
};

struct ftrace_rec_iter *ftrace_rec_iter_start(void)
{
	static struct ftrace_rec_iter ftrace_rec_iter;
	struct ftrace_rec_iter *iter = &ftrace_rec_iter;

	iter->pg = ftrace_pages_start;
	iter->index = 0;

	
	while (iter->pg && !iter->pg->index)
		iter->pg = iter->pg->next;

	if (!iter->pg)
		return NULL;

	return iter;
}

struct ftrace_rec_iter *ftrace_rec_iter_next(struct ftrace_rec_iter *iter)
{
	iter->index++;

	if (iter->index >= iter->pg->index) {
		iter->pg = iter->pg->next;
		iter->index = 0;

		
		while (iter->pg && !iter->pg->index)
			iter->pg = iter->pg->next;
	}

	if (!iter->pg)
		return NULL;

	return iter;
}

struct dyn_ftrace *ftrace_rec_iter_record(struct ftrace_rec_iter *iter)
{
	return &iter->pg->records[iter->index];
}

static int
ftrace_code_disable(struct module *mod, struct dyn_ftrace *rec)
{
	unsigned long ip;
	int ret;

	ip = rec->ip;

	if (unlikely(ftrace_disabled))
		return 0;

	ret = ftrace_make_nop(mod, rec, MCOUNT_ADDR);
	if (ret) {
		ftrace_bug(ret, ip);
		return 0;
	}
	return 1;
}

int __weak ftrace_arch_code_modify_prepare(void)
{
	return 0;
}

int __weak ftrace_arch_code_modify_post_process(void)
{
	return 0;
}

static int __ftrace_modify_code(void *data)
{
	int *command = data;

	if (*command & FTRACE_UPDATE_CALLS)
		ftrace_replace_code(1);
	else if (*command & FTRACE_DISABLE_CALLS)
		ftrace_replace_code(0);

	if (*command & FTRACE_UPDATE_TRACE_FUNC)
		ftrace_update_ftrace_func(ftrace_trace_function);

	if (*command & FTRACE_START_FUNC_RET)
		ftrace_enable_ftrace_graph_caller();
	else if (*command & FTRACE_STOP_FUNC_RET)
		ftrace_disable_ftrace_graph_caller();

	return 0;
}

void ftrace_run_stop_machine(int command)
{
	stop_machine(__ftrace_modify_code, &command, NULL);
}

void __weak arch_ftrace_update_code(int command)
{
	ftrace_run_stop_machine(command);
}

static void ftrace_run_update_code(int command)
{
	int ret;

	ret = ftrace_arch_code_modify_prepare();
	FTRACE_WARN_ON(ret);
	if (ret)
		return;
	function_trace_stop++;

	arch_ftrace_update_code(command);

#ifndef CONFIG_HAVE_FUNCTION_TRACE_MCOUNT_TEST
	__ftrace_trace_function = __ftrace_trace_function_delay;
#endif
	function_trace_stop--;

	ret = ftrace_arch_code_modify_post_process();
	FTRACE_WARN_ON(ret);
}

static ftrace_func_t saved_ftrace_func;
static int ftrace_start_up;
static int global_start_up;

static void ftrace_startup_enable(int command)
{
	if (saved_ftrace_func != ftrace_trace_function) {
		saved_ftrace_func = ftrace_trace_function;
		command |= FTRACE_UPDATE_TRACE_FUNC;
	}

	if (!command || !ftrace_enabled)
		return;

	ftrace_run_update_code(command);
}

static int ftrace_startup(struct ftrace_ops *ops, int command)
{
	bool hash_enable = true;

	if (unlikely(ftrace_disabled))
		return -ENODEV;

	ftrace_start_up++;
	command |= FTRACE_UPDATE_CALLS;

	
	if (ops->flags & FTRACE_OPS_FL_GLOBAL) {
		ops = &global_ops;
		
		if (global_start_up)
			hash_enable = false;
		global_start_up++;
	}

	ops->flags |= FTRACE_OPS_FL_ENABLED;
	if (hash_enable)
		ftrace_hash_rec_enable(ops, 1);

	ftrace_startup_enable(command);

	return 0;
}

static void ftrace_shutdown(struct ftrace_ops *ops, int command)
{
	bool hash_disable = true;

	if (unlikely(ftrace_disabled))
		return;

	ftrace_start_up--;
	WARN_ON_ONCE(ftrace_start_up < 0);

	if (ops->flags & FTRACE_OPS_FL_GLOBAL) {
		ops = &global_ops;
		global_start_up--;
		WARN_ON_ONCE(global_start_up < 0);
		
		if (global_start_up) {
			WARN_ON_ONCE(!ftrace_start_up);
			hash_disable = false;
		}
	}

	if (hash_disable)
		ftrace_hash_rec_disable(ops, 1);

	if (ops != &global_ops || !global_start_up)
		ops->flags &= ~FTRACE_OPS_FL_ENABLED;

	command |= FTRACE_UPDATE_CALLS;

	if (saved_ftrace_func != ftrace_trace_function) {
		saved_ftrace_func = ftrace_trace_function;
		command |= FTRACE_UPDATE_TRACE_FUNC;
	}

	if (!command || !ftrace_enabled)
		return;

	ftrace_run_update_code(command);
}

static void ftrace_startup_sysctl(void)
{
	if (unlikely(ftrace_disabled))
		return;

	
	saved_ftrace_func = NULL;
	
	if (ftrace_start_up)
		ftrace_run_update_code(FTRACE_UPDATE_CALLS);
}

static void ftrace_shutdown_sysctl(void)
{
	if (unlikely(ftrace_disabled))
		return;

	
	if (ftrace_start_up)
		ftrace_run_update_code(FTRACE_DISABLE_CALLS);
}

static cycle_t		ftrace_update_time;
static unsigned long	ftrace_update_cnt;
unsigned long		ftrace_update_tot_cnt;

static int ops_traces_mod(struct ftrace_ops *ops)
{
	struct ftrace_hash *hash;

	hash = ops->filter_hash;
	return ftrace_hash_empty(hash);
}

static int ftrace_update_code(struct module *mod)
{
	struct ftrace_page *pg;
	struct dyn_ftrace *p;
	cycle_t start, stop;
	unsigned long ref = 0;
	int i;

	if (mod) {
		struct ftrace_ops *ops;

		for (ops = ftrace_ops_list;
		     ops != &ftrace_list_end; ops = ops->next) {
			if (ops->flags & FTRACE_OPS_FL_ENABLED &&
			    ops_traces_mod(ops))
				ref++;
		}
	}

	start = ftrace_now(raw_smp_processor_id());
	ftrace_update_cnt = 0;

	for (pg = ftrace_new_pgs; pg; pg = pg->next) {

		for (i = 0; i < pg->index; i++) {
			
			if (unlikely(ftrace_disabled))
				return -1;

			p = &pg->records[i];
			p->flags = ref;

			if (!ftrace_code_disable(mod, p))
				break;

			ftrace_update_cnt++;

			if (ftrace_start_up && ref) {
				int failed = __ftrace_replace_code(p, 1);
				if (failed)
					ftrace_bug(failed, p->ip);
			}
		}
	}

	ftrace_new_pgs = NULL;

	stop = ftrace_now(raw_smp_processor_id());
	ftrace_update_time = stop - start;
	ftrace_update_tot_cnt += ftrace_update_cnt;

	return 0;
}

static int ftrace_allocate_records(struct ftrace_page *pg, int count)
{
	int order;
	int cnt;

	if (WARN_ON(!count))
		return -EINVAL;

	order = get_count_order(DIV_ROUND_UP(count, ENTRIES_PER_PAGE));

	while ((PAGE_SIZE << order) / ENTRY_SIZE >= count + ENTRIES_PER_PAGE)
		order--;

 again:
	pg->records = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, order);

	if (!pg->records) {
		
		if (!order)
			return -ENOMEM;
		order >>= 1;
		goto again;
	}

	cnt = (PAGE_SIZE << order) / ENTRY_SIZE;
	pg->size = cnt;

	if (cnt > count)
		cnt = count;

	return cnt;
}

static struct ftrace_page *
ftrace_allocate_pages(unsigned long num_to_init)
{
	struct ftrace_page *start_pg;
	struct ftrace_page *pg;
	int order;
	int cnt;

	if (!num_to_init)
		return 0;

	start_pg = pg = kzalloc(sizeof(*pg), GFP_KERNEL);
	if (!pg)
		return NULL;

	for (;;) {
		cnt = ftrace_allocate_records(pg, num_to_init);
		if (cnt < 0)
			goto free_pages;

		num_to_init -= cnt;
		if (!num_to_init)
			break;

		pg->next = kzalloc(sizeof(*pg), GFP_KERNEL);
		if (!pg->next)
			goto free_pages;

		pg = pg->next;
	}

	return start_pg;

 free_pages:
	while (start_pg) {
		order = get_count_order(pg->size / ENTRIES_PER_PAGE);
		free_pages((unsigned long)pg->records, order);
		start_pg = pg->next;
		kfree(pg);
		pg = start_pg;
	}
	pr_info("ftrace: FAILED to allocate memory for functions\n");
	return NULL;
}

static int __init ftrace_dyn_table_alloc(unsigned long num_to_init)
{
	int cnt;

	if (!num_to_init) {
		pr_info("ftrace: No functions to be traced?\n");
		return -1;
	}

	cnt = num_to_init / ENTRIES_PER_PAGE;
	pr_info("ftrace: allocating %ld entries in %d pages\n",
		num_to_init, cnt + 1);

	return 0;
}

#define FTRACE_BUFF_MAX (KSYM_SYMBOL_LEN+4) 

struct ftrace_iterator {
	loff_t				pos;
	loff_t				func_pos;
	struct ftrace_page		*pg;
	struct dyn_ftrace		*func;
	struct ftrace_func_probe	*probe;
	struct trace_parser		parser;
	struct ftrace_hash		*hash;
	struct ftrace_ops		*ops;
	int				hidx;
	int				idx;
	unsigned			flags;
};

static void *
t_hash_next(struct seq_file *m, loff_t *pos)
{
	struct ftrace_iterator *iter = m->private;
	struct hlist_node *hnd = NULL;
	struct hlist_head *hhd;

	(*pos)++;
	iter->pos = *pos;

	if (iter->probe)
		hnd = &iter->probe->node;
 retry:
	if (iter->hidx >= FTRACE_FUNC_HASHSIZE)
		return NULL;

	hhd = &ftrace_func_hash[iter->hidx];

	if (hlist_empty(hhd)) {
		iter->hidx++;
		hnd = NULL;
		goto retry;
	}

	if (!hnd)
		hnd = hhd->first;
	else {
		hnd = hnd->next;
		if (!hnd) {
			iter->hidx++;
			goto retry;
		}
	}

	if (WARN_ON_ONCE(!hnd))
		return NULL;

	iter->probe = hlist_entry(hnd, struct ftrace_func_probe, node);

	return iter;
}

static void *t_hash_start(struct seq_file *m, loff_t *pos)
{
	struct ftrace_iterator *iter = m->private;
	void *p = NULL;
	loff_t l;

	if (!(iter->flags & FTRACE_ITER_DO_HASH))
		return NULL;

	if (iter->func_pos > *pos)
		return NULL;

	iter->hidx = 0;
	for (l = 0; l <= (*pos - iter->func_pos); ) {
		p = t_hash_next(m, &l);
		if (!p)
			break;
	}
	if (!p)
		return NULL;

	
	iter->flags |= FTRACE_ITER_HASH;

	return iter;
}

static int
t_hash_show(struct seq_file *m, struct ftrace_iterator *iter)
{
	struct ftrace_func_probe *rec;

	rec = iter->probe;
	if (WARN_ON_ONCE(!rec))
		return -EIO;

	if (rec->ops->print)
		return rec->ops->print(m, rec->ip, rec->ops, rec->data);

	seq_printf(m, "%ps:%ps", (void *)rec->ip, (void *)rec->ops->func);

	if (rec->data)
		seq_printf(m, ":%p", rec->data);
	seq_putc(m, '\n');

	return 0;
}

static void *
t_next(struct seq_file *m, void *v, loff_t *pos)
{
	struct ftrace_iterator *iter = m->private;
	struct ftrace_ops *ops = iter->ops;
	struct dyn_ftrace *rec = NULL;

	if (unlikely(ftrace_disabled))
		return NULL;

	if (iter->flags & FTRACE_ITER_HASH)
		return t_hash_next(m, pos);

	(*pos)++;
	iter->pos = iter->func_pos = *pos;

	if (iter->flags & FTRACE_ITER_PRINTALL)
		return t_hash_start(m, pos);

 retry:
	if (iter->idx >= iter->pg->index) {
		if (iter->pg->next) {
			iter->pg = iter->pg->next;
			iter->idx = 0;
			goto retry;
		}
	} else {
		rec = &iter->pg->records[iter->idx++];
		if (((iter->flags & FTRACE_ITER_FILTER) &&
		     !(ftrace_lookup_ip(ops->filter_hash, rec->ip))) ||

		    ((iter->flags & FTRACE_ITER_NOTRACE) &&
		     !ftrace_lookup_ip(ops->notrace_hash, rec->ip)) ||

		    ((iter->flags & FTRACE_ITER_ENABLED) &&
		     !(rec->flags & ~FTRACE_FL_MASK))) {

			rec = NULL;
			goto retry;
		}
	}

	if (!rec)
		return t_hash_start(m, pos);

	iter->func = rec;

	return iter;
}

static void reset_iter_read(struct ftrace_iterator *iter)
{
	iter->pos = 0;
	iter->func_pos = 0;
	iter->flags &= ~(FTRACE_ITER_PRINTALL & FTRACE_ITER_HASH);
}

static void *t_start(struct seq_file *m, loff_t *pos)
{
	struct ftrace_iterator *iter = m->private;
	struct ftrace_ops *ops = iter->ops;
	void *p = NULL;
	loff_t l;

	mutex_lock(&ftrace_lock);

	if (unlikely(ftrace_disabled))
		return NULL;

	if (*pos < iter->pos)
		reset_iter_read(iter);

	if (iter->flags & FTRACE_ITER_FILTER &&
	    ftrace_hash_empty(ops->filter_hash)) {
		if (*pos > 0)
			return t_hash_start(m, pos);
		iter->flags |= FTRACE_ITER_PRINTALL;
		
		iter->flags &= ~FTRACE_ITER_HASH;
		return iter;
	}

	if (iter->flags & FTRACE_ITER_HASH)
		return t_hash_start(m, pos);

	iter->pg = ftrace_pages_start;
	iter->idx = 0;
	for (l = 0; l <= *pos; ) {
		p = t_next(m, p, &l);
		if (!p)
			break;
	}

	if (!p)
		return t_hash_start(m, pos);

	return iter;
}

static void t_stop(struct seq_file *m, void *p)
{
	mutex_unlock(&ftrace_lock);
}

static int t_show(struct seq_file *m, void *v)
{
	struct ftrace_iterator *iter = m->private;
	struct dyn_ftrace *rec;

	if (iter->flags & FTRACE_ITER_HASH)
		return t_hash_show(m, iter);

	if (iter->flags & FTRACE_ITER_PRINTALL) {
		seq_printf(m, "#### all functions enabled ####\n");
		return 0;
	}

	rec = iter->func;

	if (!rec)
		return 0;

	seq_printf(m, "%ps", (void *)rec->ip);
	if (iter->flags & FTRACE_ITER_ENABLED)
		seq_printf(m, " (%ld)",
			   rec->flags & ~FTRACE_FL_MASK);
	seq_printf(m, "\n");

	return 0;
}

static const struct seq_operations show_ftrace_seq_ops = {
	.start = t_start,
	.next = t_next,
	.stop = t_stop,
	.show = t_show,
};

static int
ftrace_avail_open(struct inode *inode, struct file *file)
{
	struct ftrace_iterator *iter;
	int ret;

	if (unlikely(ftrace_disabled))
		return -ENODEV;

	iter = kzalloc(sizeof(*iter), GFP_KERNEL);
	if (!iter)
		return -ENOMEM;

	iter->pg = ftrace_pages_start;
	iter->ops = &global_ops;

	ret = seq_open(file, &show_ftrace_seq_ops);
	if (!ret) {
		struct seq_file *m = file->private_data;

		m->private = iter;
	} else {
		kfree(iter);
	}

	return ret;
}

static int
ftrace_enabled_open(struct inode *inode, struct file *file)
{
	struct ftrace_iterator *iter;
	int ret;

	if (unlikely(ftrace_disabled))
		return -ENODEV;

	iter = kzalloc(sizeof(*iter), GFP_KERNEL);
	if (!iter)
		return -ENOMEM;

	iter->pg = ftrace_pages_start;
	iter->flags = FTRACE_ITER_ENABLED;
	iter->ops = &global_ops;

	ret = seq_open(file, &show_ftrace_seq_ops);
	if (!ret) {
		struct seq_file *m = file->private_data;

		m->private = iter;
	} else {
		kfree(iter);
	}

	return ret;
}

static void ftrace_filter_reset(struct ftrace_hash *hash)
{
	mutex_lock(&ftrace_lock);
	ftrace_hash_clear(hash);
	mutex_unlock(&ftrace_lock);
}

int
ftrace_regex_open(struct ftrace_ops *ops, int flag,
		  struct inode *inode, struct file *file)
{
	struct ftrace_iterator *iter;
	struct ftrace_hash *hash;
	int ret = 0;

	if (unlikely(ftrace_disabled))
		return -ENODEV;

	iter = kzalloc(sizeof(*iter), GFP_KERNEL);
	if (!iter)
		return -ENOMEM;

	if (trace_parser_get_init(&iter->parser, FTRACE_BUFF_MAX)) {
		kfree(iter);
		return -ENOMEM;
	}

	if (flag & FTRACE_ITER_NOTRACE)
		hash = ops->notrace_hash;
	else
		hash = ops->filter_hash;

	iter->ops = ops;
	iter->flags = flag;

	if (file->f_mode & FMODE_WRITE) {
		mutex_lock(&ftrace_lock);
		iter->hash = alloc_and_copy_ftrace_hash(FTRACE_HASH_DEFAULT_BITS, hash);
		mutex_unlock(&ftrace_lock);

		if (!iter->hash) {
			trace_parser_put(&iter->parser);
			kfree(iter);
			return -ENOMEM;
		}
	}

	mutex_lock(&ftrace_regex_lock);

	if ((file->f_mode & FMODE_WRITE) &&
	    (file->f_flags & O_TRUNC))
		ftrace_filter_reset(iter->hash);

	if (file->f_mode & FMODE_READ) {
		iter->pg = ftrace_pages_start;

		ret = seq_open(file, &show_ftrace_seq_ops);
		if (!ret) {
			struct seq_file *m = file->private_data;
			m->private = iter;
		} else {
			
			free_ftrace_hash(iter->hash);
			trace_parser_put(&iter->parser);
			kfree(iter);
		}
	} else
		file->private_data = iter;
	mutex_unlock(&ftrace_regex_lock);

	return ret;
}

static int
ftrace_filter_open(struct inode *inode, struct file *file)
{
	return ftrace_regex_open(&global_ops,
			FTRACE_ITER_FILTER | FTRACE_ITER_DO_HASH,
			inode, file);
}

static int
ftrace_notrace_open(struct inode *inode, struct file *file)
{
	return ftrace_regex_open(&global_ops, FTRACE_ITER_NOTRACE,
				 inode, file);
}

loff_t
ftrace_regex_lseek(struct file *file, loff_t offset, int origin)
{
	loff_t ret;

	if (file->f_mode & FMODE_READ)
		ret = seq_lseek(file, offset, origin);
	else
		file->f_pos = ret = 1;

	return ret;
}

static int ftrace_match(char *str, char *regex, int len, int type)
{
	int matched = 0;
	int slen;

	switch (type) {
	case MATCH_FULL:
		if (strcmp(str, regex) == 0)
			matched = 1;
		break;
	case MATCH_FRONT_ONLY:
		if (strncmp(str, regex, len) == 0)
			matched = 1;
		break;
	case MATCH_MIDDLE_ONLY:
		if (strstr(str, regex))
			matched = 1;
		break;
	case MATCH_END_ONLY:
		slen = strlen(str);
		if (slen >= len && memcmp(str + slen - len, regex, len) == 0)
			matched = 1;
		break;
	}

	return matched;
}

static int
enter_record(struct ftrace_hash *hash, struct dyn_ftrace *rec, int not)
{
	struct ftrace_func_entry *entry;
	int ret = 0;

	entry = ftrace_lookup_ip(hash, rec->ip);
	if (not) {
		
		if (!entry)
			return 0;

		free_hash_entry(hash, entry);
	} else {
		
		if (entry)
			return 0;

		ret = add_hash_entry(hash, rec->ip);
	}
	return ret;
}

static int
ftrace_match_record(struct dyn_ftrace *rec, char *mod,
		    char *regex, int len, int type)
{
	char str[KSYM_SYMBOL_LEN];
	char *modname;

	kallsyms_lookup(rec->ip, NULL, NULL, &modname, str);

	if (mod) {
		
		if (!modname || strcmp(modname, mod))
			return 0;

		
		if (!len)
			return 1;
	}

	return ftrace_match(str, regex, len, type);
}

static int
match_records(struct ftrace_hash *hash, char *buff,
	      int len, char *mod, int not)
{
	unsigned search_len = 0;
	struct ftrace_page *pg;
	struct dyn_ftrace *rec;
	int type = MATCH_FULL;
	char *search = buff;
	int found = 0;
	int ret;

	if (len) {
		type = filter_parse_regex(buff, len, &search, &not);
		search_len = strlen(search);
	}

	mutex_lock(&ftrace_lock);

	if (unlikely(ftrace_disabled))
		goto out_unlock;

	do_for_each_ftrace_rec(pg, rec) {
		if (ftrace_match_record(rec, mod, search, search_len, type)) {
			ret = enter_record(hash, rec, not);
			if (ret < 0) {
				found = ret;
				goto out_unlock;
			}
			found = 1;
		}
	} while_for_each_ftrace_rec();
 out_unlock:
	mutex_unlock(&ftrace_lock);

	return found;
}

static int
ftrace_match_records(struct ftrace_hash *hash, char *buff, int len)
{
	return match_records(hash, buff, len, NULL, 0);
}

static int
ftrace_match_module_records(struct ftrace_hash *hash, char *buff, char *mod)
{
	int not = 0;

	
	if (strcmp(buff, "*") == 0)
		buff[0] = 0;

	
	if (strcmp(buff, "!") == 0 || strcmp(buff, "!*") == 0) {
		buff[0] = 0;
		not = 1;
	}

	return match_records(hash, buff, strlen(buff), mod, not);
}


static int
ftrace_mod_callback(struct ftrace_hash *hash,
		    char *func, char *cmd, char *param, int enable)
{
	char *mod;
	int ret = -EINVAL;


	
	if (!param)
		return ret;

	mod = strsep(&param, ":");
	if (!strlen(mod))
		return ret;

	ret = ftrace_match_module_records(hash, func, mod);
	if (!ret)
		ret = -EINVAL;
	if (ret < 0)
		return ret;

	return 0;
}

static struct ftrace_func_command ftrace_mod_cmd = {
	.name			= "mod",
	.func			= ftrace_mod_callback,
};

static int __init ftrace_mod_cmd_init(void)
{
	return register_ftrace_command(&ftrace_mod_cmd);
}
device_initcall(ftrace_mod_cmd_init);

static void
function_trace_probe_call(unsigned long ip, unsigned long parent_ip)
{
	struct ftrace_func_probe *entry;
	struct hlist_head *hhd;
	struct hlist_node *n;
	unsigned long key;

	key = hash_long(ip, FTRACE_HASH_BITS);

	hhd = &ftrace_func_hash[key];

	if (hlist_empty(hhd))
		return;

	preempt_disable_notrace();
	hlist_for_each_entry_rcu(entry, n, hhd, node) {
		if (entry->ip == ip)
			entry->ops->func(ip, parent_ip, &entry->data);
	}
	preempt_enable_notrace();
}

static struct ftrace_ops trace_probe_ops __read_mostly =
{
	.func		= function_trace_probe_call,
};

static int ftrace_probe_registered;

static void __enable_ftrace_function_probe(void)
{
	int ret;
	int i;

	if (ftrace_probe_registered)
		return;

	for (i = 0; i < FTRACE_FUNC_HASHSIZE; i++) {
		struct hlist_head *hhd = &ftrace_func_hash[i];
		if (hhd->first)
			break;
	}
	
	if (i == FTRACE_FUNC_HASHSIZE)
		return;

	ret = __register_ftrace_function(&trace_probe_ops);
	if (!ret)
		ret = ftrace_startup(&trace_probe_ops, 0);

	ftrace_probe_registered = 1;
}

static void __disable_ftrace_function_probe(void)
{
	int ret;
	int i;

	if (!ftrace_probe_registered)
		return;

	for (i = 0; i < FTRACE_FUNC_HASHSIZE; i++) {
		struct hlist_head *hhd = &ftrace_func_hash[i];
		if (hhd->first)
			return;
	}

	
	ret = __unregister_ftrace_function(&trace_probe_ops);
	if (!ret)
		ftrace_shutdown(&trace_probe_ops, 0);

	ftrace_probe_registered = 0;
}


static void ftrace_free_entry_rcu(struct rcu_head *rhp)
{
	struct ftrace_func_probe *entry =
		container_of(rhp, struct ftrace_func_probe, rcu);

	if (entry->ops->free)
		entry->ops->free(&entry->data);
	kfree(entry);
}


int
register_ftrace_function_probe(char *glob, struct ftrace_probe_ops *ops,
			      void *data)
{
	struct ftrace_func_probe *entry;
	struct ftrace_page *pg;
	struct dyn_ftrace *rec;
	int type, len, not;
	unsigned long key;
	int count = 0;
	char *search;

	type = filter_parse_regex(glob, strlen(glob), &search, &not);
	len = strlen(search);

	
	if (WARN_ON(not))
		return -EINVAL;

	mutex_lock(&ftrace_lock);

	if (unlikely(ftrace_disabled))
		goto out_unlock;

	do_for_each_ftrace_rec(pg, rec) {

		if (!ftrace_match_record(rec, NULL, search, len, type))
			continue;

		entry = kmalloc(sizeof(*entry), GFP_KERNEL);
		if (!entry) {
			
			if (!count)
				count = -ENOMEM;
			goto out_unlock;
		}

		count++;

		entry->data = data;

		if (ops->callback) {
			if (ops->callback(rec->ip, &entry->data) < 0) {
				
				kfree(entry);
				continue;
			}
		}

		entry->ops = ops;
		entry->ip = rec->ip;

		key = hash_long(entry->ip, FTRACE_HASH_BITS);
		hlist_add_head_rcu(&entry->node, &ftrace_func_hash[key]);

	} while_for_each_ftrace_rec();
	__enable_ftrace_function_probe();

 out_unlock:
	mutex_unlock(&ftrace_lock);

	return count;
}

enum {
	PROBE_TEST_FUNC		= 1,
	PROBE_TEST_DATA		= 2
};

static void
__unregister_ftrace_function_probe(char *glob, struct ftrace_probe_ops *ops,
				  void *data, int flags)
{
	struct ftrace_func_probe *entry;
	struct hlist_node *n, *tmp;
	char str[KSYM_SYMBOL_LEN];
	int type = MATCH_FULL;
	int i, len = 0;
	char *search;

	if (glob && (strcmp(glob, "*") == 0 || !strlen(glob)))
		glob = NULL;
	else if (glob) {
		int not;

		type = filter_parse_regex(glob, strlen(glob), &search, &not);
		len = strlen(search);

		
		if (WARN_ON(not))
			return;
	}

	mutex_lock(&ftrace_lock);
	for (i = 0; i < FTRACE_FUNC_HASHSIZE; i++) {
		struct hlist_head *hhd = &ftrace_func_hash[i];

		hlist_for_each_entry_safe(entry, n, tmp, hhd, node) {

			
			if ((flags & PROBE_TEST_FUNC) && entry->ops != ops)
				continue;

			if ((flags & PROBE_TEST_DATA) && entry->data != data)
				continue;

			
			if (glob) {
				kallsyms_lookup(entry->ip, NULL, NULL,
						NULL, str);
				if (!ftrace_match(str, glob, len, type))
					continue;
			}

			hlist_del(&entry->node);
			call_rcu(&entry->rcu, ftrace_free_entry_rcu);
		}
	}
	__disable_ftrace_function_probe();
	mutex_unlock(&ftrace_lock);
}

void
unregister_ftrace_function_probe(char *glob, struct ftrace_probe_ops *ops,
				void *data)
{
	__unregister_ftrace_function_probe(glob, ops, data,
					  PROBE_TEST_FUNC | PROBE_TEST_DATA);
}

void
unregister_ftrace_function_probe_func(char *glob, struct ftrace_probe_ops *ops)
{
	__unregister_ftrace_function_probe(glob, ops, NULL, PROBE_TEST_FUNC);
}

void unregister_ftrace_function_probe_all(char *glob)
{
	__unregister_ftrace_function_probe(glob, NULL, NULL, 0);
}

static LIST_HEAD(ftrace_commands);
static DEFINE_MUTEX(ftrace_cmd_mutex);

int register_ftrace_command(struct ftrace_func_command *cmd)
{
	struct ftrace_func_command *p;
	int ret = 0;

	mutex_lock(&ftrace_cmd_mutex);
	list_for_each_entry(p, &ftrace_commands, list) {
		if (strcmp(cmd->name, p->name) == 0) {
			ret = -EBUSY;
			goto out_unlock;
		}
	}
	list_add(&cmd->list, &ftrace_commands);
 out_unlock:
	mutex_unlock(&ftrace_cmd_mutex);

	return ret;
}

int unregister_ftrace_command(struct ftrace_func_command *cmd)
{
	struct ftrace_func_command *p, *n;
	int ret = -ENODEV;

	mutex_lock(&ftrace_cmd_mutex);
	list_for_each_entry_safe(p, n, &ftrace_commands, list) {
		if (strcmp(cmd->name, p->name) == 0) {
			ret = 0;
			list_del_init(&p->list);
			goto out_unlock;
		}
	}
 out_unlock:
	mutex_unlock(&ftrace_cmd_mutex);

	return ret;
}

static int ftrace_process_regex(struct ftrace_hash *hash,
				char *buff, int len, int enable)
{
	char *func, *command, *next = buff;
	struct ftrace_func_command *p;
	int ret = -EINVAL;

	func = strsep(&next, ":");

	if (!next) {
		ret = ftrace_match_records(hash, func, len);
		if (!ret)
			ret = -EINVAL;
		if (ret < 0)
			return ret;
		return 0;
	}

	

	command = strsep(&next, ":");

	mutex_lock(&ftrace_cmd_mutex);
	list_for_each_entry(p, &ftrace_commands, list) {
		if (strcmp(p->name, command) == 0) {
			ret = p->func(hash, func, command, next, enable);
			goto out_unlock;
		}
	}
 out_unlock:
	mutex_unlock(&ftrace_cmd_mutex);

	return ret;
}

static ssize_t
ftrace_regex_write(struct file *file, const char __user *ubuf,
		   size_t cnt, loff_t *ppos, int enable)
{
	struct ftrace_iterator *iter;
	struct trace_parser *parser;
	ssize_t ret, read;

	if (!cnt)
		return 0;

	mutex_lock(&ftrace_regex_lock);

	ret = -ENODEV;
	if (unlikely(ftrace_disabled))
		goto out_unlock;

	if (file->f_mode & FMODE_READ) {
		struct seq_file *m = file->private_data;
		iter = m->private;
	} else
		iter = file->private_data;

	parser = &iter->parser;
	read = trace_get_user(parser, ubuf, cnt, ppos);

	if (read >= 0 && trace_parser_loaded(parser) &&
	    !trace_parser_cont(parser)) {
		ret = ftrace_process_regex(iter->hash, parser->buffer,
					   parser->idx, enable);
		trace_parser_clear(parser);
		if (ret)
			goto out_unlock;
	}

	ret = read;
out_unlock:
	mutex_unlock(&ftrace_regex_lock);

	return ret;
}

ssize_t
ftrace_filter_write(struct file *file, const char __user *ubuf,
		    size_t cnt, loff_t *ppos)
{
	return ftrace_regex_write(file, ubuf, cnt, ppos, 1);
}

ssize_t
ftrace_notrace_write(struct file *file, const char __user *ubuf,
		     size_t cnt, loff_t *ppos)
{
	return ftrace_regex_write(file, ubuf, cnt, ppos, 0);
}

static int
ftrace_set_regex(struct ftrace_ops *ops, unsigned char *buf, int len,
		 int reset, int enable)
{
	struct ftrace_hash **orig_hash;
	struct ftrace_hash *hash;
	int ret;

	
	if (ops->flags & FTRACE_OPS_FL_GLOBAL)
		ops = &global_ops;

	if (unlikely(ftrace_disabled))
		return -ENODEV;

	if (enable)
		orig_hash = &ops->filter_hash;
	else
		orig_hash = &ops->notrace_hash;

	hash = alloc_and_copy_ftrace_hash(FTRACE_HASH_DEFAULT_BITS, *orig_hash);
	if (!hash)
		return -ENOMEM;

	mutex_lock(&ftrace_regex_lock);
	if (reset)
		ftrace_filter_reset(hash);
	if (buf && !ftrace_match_records(hash, buf, len)) {
		ret = -EINVAL;
		goto out_regex_unlock;
	}

	mutex_lock(&ftrace_lock);
	ret = ftrace_hash_move(ops, enable, orig_hash, hash);
	if (!ret && ops->flags & FTRACE_OPS_FL_ENABLED
	    && ftrace_enabled)
		ftrace_run_update_code(FTRACE_UPDATE_CALLS);

	mutex_unlock(&ftrace_lock);

 out_regex_unlock:
	mutex_unlock(&ftrace_regex_lock);

	free_ftrace_hash(hash);
	return ret;
}

int ftrace_set_filter(struct ftrace_ops *ops, unsigned char *buf,
		       int len, int reset)
{
	return ftrace_set_regex(ops, buf, len, reset, 1);
}
EXPORT_SYMBOL_GPL(ftrace_set_filter);

int ftrace_set_notrace(struct ftrace_ops *ops, unsigned char *buf,
			int len, int reset)
{
	return ftrace_set_regex(ops, buf, len, reset, 0);
}
EXPORT_SYMBOL_GPL(ftrace_set_notrace);
void ftrace_set_global_filter(unsigned char *buf, int len, int reset)
{
	ftrace_set_regex(&global_ops, buf, len, reset, 1);
}
EXPORT_SYMBOL_GPL(ftrace_set_global_filter);

void ftrace_set_global_notrace(unsigned char *buf, int len, int reset)
{
	ftrace_set_regex(&global_ops, buf, len, reset, 0);
}
EXPORT_SYMBOL_GPL(ftrace_set_global_notrace);

#define FTRACE_FILTER_SIZE		COMMAND_LINE_SIZE
static char ftrace_notrace_buf[FTRACE_FILTER_SIZE] __initdata;
static char ftrace_filter_buf[FTRACE_FILTER_SIZE] __initdata;

static int __init set_ftrace_notrace(char *str)
{
	strncpy(ftrace_notrace_buf, str, FTRACE_FILTER_SIZE);
	return 1;
}
__setup("ftrace_notrace=", set_ftrace_notrace);

static int __init set_ftrace_filter(char *str)
{
	strncpy(ftrace_filter_buf, str, FTRACE_FILTER_SIZE);
	return 1;
}
__setup("ftrace_filter=", set_ftrace_filter);

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
static char ftrace_graph_buf[FTRACE_FILTER_SIZE] __initdata;
static int ftrace_set_func(unsigned long *array, int *idx, char *buffer);

static int __init set_graph_function(char *str)
{
	strlcpy(ftrace_graph_buf, str, FTRACE_FILTER_SIZE);
	return 1;
}
__setup("ftrace_graph_filter=", set_graph_function);

static void __init set_ftrace_early_graph(char *buf)
{
	int ret;
	char *func;

	while (buf) {
		func = strsep(&buf, ",");
		
		ret = ftrace_set_func(ftrace_graph_funcs, &ftrace_graph_count,
				      func);
		if (ret)
			printk(KERN_DEBUG "ftrace: function %s not "
					  "traceable\n", func);
	}
}
#endif 

void __init
ftrace_set_early_filter(struct ftrace_ops *ops, char *buf, int enable)
{
	char *func;

	while (buf) {
		func = strsep(&buf, ",");
		ftrace_set_regex(ops, func, strlen(func), 0, enable);
	}
}

static void __init set_ftrace_early_filters(void)
{
	if (ftrace_filter_buf[0])
		ftrace_set_early_filter(&global_ops, ftrace_filter_buf, 1);
	if (ftrace_notrace_buf[0])
		ftrace_set_early_filter(&global_ops, ftrace_notrace_buf, 0);
#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	if (ftrace_graph_buf[0])
		set_ftrace_early_graph(ftrace_graph_buf);
#endif 
}

int ftrace_regex_release(struct inode *inode, struct file *file)
{
	struct seq_file *m = (struct seq_file *)file->private_data;
	struct ftrace_iterator *iter;
	struct ftrace_hash **orig_hash;
	struct trace_parser *parser;
	int filter_hash;
	int ret;

	mutex_lock(&ftrace_regex_lock);
	if (file->f_mode & FMODE_READ) {
		iter = m->private;

		seq_release(inode, file);
	} else
		iter = file->private_data;

	parser = &iter->parser;
	if (trace_parser_loaded(parser)) {
		parser->buffer[parser->idx] = 0;
		ftrace_match_records(iter->hash, parser->buffer, parser->idx);
	}

	trace_parser_put(parser);

	if (file->f_mode & FMODE_WRITE) {
		filter_hash = !!(iter->flags & FTRACE_ITER_FILTER);

		if (filter_hash)
			orig_hash = &iter->ops->filter_hash;
		else
			orig_hash = &iter->ops->notrace_hash;

		mutex_lock(&ftrace_lock);
		ret = ftrace_hash_move(iter->ops, filter_hash,
				       orig_hash, iter->hash);
		if (!ret && (iter->ops->flags & FTRACE_OPS_FL_ENABLED)
		    && ftrace_enabled)
			ftrace_run_update_code(FTRACE_UPDATE_CALLS);

		mutex_unlock(&ftrace_lock);
	}
	free_ftrace_hash(iter->hash);
	kfree(iter);

	mutex_unlock(&ftrace_regex_lock);
	return 0;
}

static const struct file_operations ftrace_avail_fops = {
	.open = ftrace_avail_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release_private,
};

static const struct file_operations ftrace_enabled_fops = {
	.open = ftrace_enabled_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release_private,
};

static const struct file_operations ftrace_filter_fops = {
	.open = ftrace_filter_open,
	.read = seq_read,
	.write = ftrace_filter_write,
	.llseek = ftrace_regex_lseek,
	.release = ftrace_regex_release,
};

static const struct file_operations ftrace_notrace_fops = {
	.open = ftrace_notrace_open,
	.read = seq_read,
	.write = ftrace_notrace_write,
	.llseek = ftrace_regex_lseek,
	.release = ftrace_regex_release,
};

#ifdef CONFIG_FUNCTION_GRAPH_TRACER

static DEFINE_MUTEX(graph_lock);

int ftrace_graph_count;
int ftrace_graph_filter_enabled;
unsigned long ftrace_graph_funcs[FTRACE_GRAPH_MAX_FUNCS] __read_mostly;

static void *
__g_next(struct seq_file *m, loff_t *pos)
{
	if (*pos >= ftrace_graph_count)
		return NULL;
	return &ftrace_graph_funcs[*pos];
}

static void *
g_next(struct seq_file *m, void *v, loff_t *pos)
{
	(*pos)++;
	return __g_next(m, pos);
}

static void *g_start(struct seq_file *m, loff_t *pos)
{
	mutex_lock(&graph_lock);

	
	if (!ftrace_graph_filter_enabled && !*pos)
		return (void *)1;

	return __g_next(m, pos);
}

static void g_stop(struct seq_file *m, void *p)
{
	mutex_unlock(&graph_lock);
}

static int g_show(struct seq_file *m, void *v)
{
	unsigned long *ptr = v;

	if (!ptr)
		return 0;

	if (ptr == (unsigned long *)1) {
		seq_printf(m, "#### all functions enabled ####\n");
		return 0;
	}

	seq_printf(m, "%ps\n", (void *)*ptr);

	return 0;
}

static const struct seq_operations ftrace_graph_seq_ops = {
	.start = g_start,
	.next = g_next,
	.stop = g_stop,
	.show = g_show,
};

static int
ftrace_graph_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	if (unlikely(ftrace_disabled))
		return -ENODEV;

	mutex_lock(&graph_lock);
	if ((file->f_mode & FMODE_WRITE) &&
	    (file->f_flags & O_TRUNC)) {
		ftrace_graph_filter_enabled = 0;
		ftrace_graph_count = 0;
		memset(ftrace_graph_funcs, 0, sizeof(ftrace_graph_funcs));
	}
	mutex_unlock(&graph_lock);

	if (file->f_mode & FMODE_READ)
		ret = seq_open(file, &ftrace_graph_seq_ops);

	return ret;
}

static int
ftrace_graph_release(struct inode *inode, struct file *file)
{
	if (file->f_mode & FMODE_READ)
		seq_release(inode, file);
	return 0;
}

static int
ftrace_set_func(unsigned long *array, int *idx, char *buffer)
{
	struct dyn_ftrace *rec;
	struct ftrace_page *pg;
	int search_len;
	int fail = 1;
	int type, not;
	char *search;
	bool exists;
	int i;

	
	type = filter_parse_regex(buffer, strlen(buffer), &search, &not);
	if (!not && *idx >= FTRACE_GRAPH_MAX_FUNCS)
		return -EBUSY;

	search_len = strlen(search);

	mutex_lock(&ftrace_lock);

	if (unlikely(ftrace_disabled)) {
		mutex_unlock(&ftrace_lock);
		return -ENODEV;
	}

	do_for_each_ftrace_rec(pg, rec) {

		if (ftrace_match_record(rec, NULL, search, search_len, type)) {
			
			exists = false;
			for (i = 0; i < *idx; i++) {
				if (array[i] == rec->ip) {
					exists = true;
					break;
				}
			}

			if (!not) {
				fail = 0;
				if (!exists) {
					array[(*idx)++] = rec->ip;
					if (*idx >= FTRACE_GRAPH_MAX_FUNCS)
						goto out;
				}
			} else {
				if (exists) {
					array[i] = array[--(*idx)];
					array[*idx] = 0;
					fail = 0;
				}
			}
		}
	} while_for_each_ftrace_rec();
out:
	mutex_unlock(&ftrace_lock);

	if (fail)
		return -EINVAL;

	ftrace_graph_filter_enabled = 1;
	return 0;
}

static ssize_t
ftrace_graph_write(struct file *file, const char __user *ubuf,
		   size_t cnt, loff_t *ppos)
{
	struct trace_parser parser;
	ssize_t read, ret;

	if (!cnt)
		return 0;

	mutex_lock(&graph_lock);

	if (trace_parser_get_init(&parser, FTRACE_BUFF_MAX)) {
		ret = -ENOMEM;
		goto out_unlock;
	}

	read = trace_get_user(&parser, ubuf, cnt, ppos);

	if (read >= 0 && trace_parser_loaded((&parser))) {
		parser.buffer[parser.idx] = 0;

		
		ret = ftrace_set_func(ftrace_graph_funcs, &ftrace_graph_count,
					parser.buffer);
		if (ret)
			goto out_free;
	}

	ret = read;

out_free:
	trace_parser_put(&parser);
out_unlock:
	mutex_unlock(&graph_lock);

	return ret;
}

static const struct file_operations ftrace_graph_fops = {
	.open		= ftrace_graph_open,
	.read		= seq_read,
	.write		= ftrace_graph_write,
	.release	= ftrace_graph_release,
	.llseek		= seq_lseek,
};
#endif 

static __init int ftrace_init_dyn_debugfs(struct dentry *d_tracer)
{

	trace_create_file("available_filter_functions", 0444,
			d_tracer, NULL, &ftrace_avail_fops);

	trace_create_file("enabled_functions", 0444,
			d_tracer, NULL, &ftrace_enabled_fops);

	trace_create_file("set_ftrace_filter", 0644, d_tracer,
			NULL, &ftrace_filter_fops);

	trace_create_file("set_ftrace_notrace", 0644, d_tracer,
				    NULL, &ftrace_notrace_fops);

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	trace_create_file("set_graph_function", 0444, d_tracer,
				    NULL,
				    &ftrace_graph_fops);
#endif 

	return 0;
}

static void ftrace_swap_recs(void *a, void *b, int size)
{
	struct dyn_ftrace *reca = a;
	struct dyn_ftrace *recb = b;
	struct dyn_ftrace t;

	t = *reca;
	*reca = *recb;
	*recb = t;
}

static int ftrace_process_locs(struct module *mod,
			       unsigned long *start,
			       unsigned long *end)
{
	struct ftrace_page *pg;
	unsigned long count;
	unsigned long *p;
	unsigned long addr;
	unsigned long flags = 0; 
	int ret = -ENOMEM;

	count = end - start;

	if (!count)
		return 0;

	pg = ftrace_allocate_pages(count);
	if (!pg)
		return -ENOMEM;

	mutex_lock(&ftrace_lock);

	if (!mod) {
		WARN_ON(ftrace_pages || ftrace_pages_start);
		
		ftrace_pages = ftrace_pages_start = pg;
	} else {
		if (!ftrace_pages)
			goto out;

		if (WARN_ON(ftrace_pages->next)) {
			
			while (ftrace_pages->next)
				ftrace_pages = ftrace_pages->next;
		}

		ftrace_pages->next = pg;
		ftrace_pages = pg;
	}

	p = start;
	while (p < end) {
		addr = ftrace_call_adjust(*p++);
		if (!addr)
			continue;
		if (!ftrace_record_ip(addr))
			break;
	}

	
	ftrace_new_pgs = pg;

	
	for (; pg; pg = pg->next)
		sort(pg->records, pg->index, sizeof(struct dyn_ftrace),
		     ftrace_cmp_recs, ftrace_swap_recs);

	if (!mod)
		local_irq_save(flags);
	ftrace_update_code(mod);
	if (!mod)
		local_irq_restore(flags);
	ret = 0;
 out:
	mutex_unlock(&ftrace_lock);

	return ret;
}

#ifdef CONFIG_MODULES

#define next_to_ftrace_page(p) container_of(p, struct ftrace_page, next)

void ftrace_release_mod(struct module *mod)
{
	struct dyn_ftrace *rec;
	struct ftrace_page **last_pg;
	struct ftrace_page *pg;
	int order;

	mutex_lock(&ftrace_lock);

	if (ftrace_disabled)
		goto out_unlock;

	last_pg = &ftrace_pages_start;
	for (pg = ftrace_pages_start; pg; pg = *last_pg) {
		rec = &pg->records[0];
		if (within_module_core(rec->ip, mod)) {
			if (WARN_ON(pg == ftrace_pages_start))
				goto out_unlock;

			
			if (pg == ftrace_pages)
				ftrace_pages = next_to_ftrace_page(last_pg);

			*last_pg = pg->next;
			order = get_count_order(pg->size / ENTRIES_PER_PAGE);
			free_pages((unsigned long)pg->records, order);
			kfree(pg);
		} else
			last_pg = &pg->next;
	}
 out_unlock:
	mutex_unlock(&ftrace_lock);
}

static void ftrace_init_module(struct module *mod,
			       unsigned long *start, unsigned long *end)
{
	if (ftrace_disabled || start == end)
		return;
	ftrace_process_locs(mod, start, end);
}

static int ftrace_module_notify(struct notifier_block *self,
				unsigned long val, void *data)
{
	struct module *mod = data;

	switch (val) {
	case MODULE_STATE_COMING:
		ftrace_init_module(mod, mod->ftrace_callsites,
				   mod->ftrace_callsites +
				   mod->num_ftrace_callsites);
		break;
	case MODULE_STATE_GOING:
		ftrace_release_mod(mod);
		break;
	}

	return 0;
}
#else
static int ftrace_module_notify(struct notifier_block *self,
				unsigned long val, void *data)
{
	return 0;
}
#endif 

struct notifier_block ftrace_module_nb = {
	.notifier_call = ftrace_module_notify,
	.priority = 0,
};

extern unsigned long __start_mcount_loc[];
extern unsigned long __stop_mcount_loc[];

void __init ftrace_init(void)
{
	unsigned long count, addr, flags;
	int ret;

	
	addr = (unsigned long)ftrace_stub;

	local_irq_save(flags);
	ftrace_dyn_arch_init(&addr);
	local_irq_restore(flags);

	
	if (addr)
		goto failed;

	count = __stop_mcount_loc - __start_mcount_loc;

	ret = ftrace_dyn_table_alloc(count);
	if (ret)
		goto failed;

	last_ftrace_enabled = ftrace_enabled = 1;

	ret = ftrace_process_locs(NULL,
				  __start_mcount_loc,
				  __stop_mcount_loc);

	ret = register_module_notifier(&ftrace_module_nb);
	if (ret)
		pr_warning("Failed to register trace ftrace module notifier\n");

	set_ftrace_early_filters();

	return;
 failed:
	ftrace_disabled = 1;
}

#else

static struct ftrace_ops global_ops = {
	.func			= ftrace_stub,
};

static int __init ftrace_nodyn_init(void)
{
	ftrace_enabled = 1;
	return 0;
}
device_initcall(ftrace_nodyn_init);

static inline int ftrace_init_dyn_debugfs(struct dentry *d_tracer) { return 0; }
static inline void ftrace_startup_enable(int command) { }
# define ftrace_startup(ops, command)			\
	({						\
		(ops)->flags |= FTRACE_OPS_FL_ENABLED;	\
		0;					\
	})
# define ftrace_shutdown(ops, command)	do { } while (0)
# define ftrace_startup_sysctl()	do { } while (0)
# define ftrace_shutdown_sysctl()	do { } while (0)

static inline int
ftrace_ops_test(struct ftrace_ops *ops, unsigned long ip)
{
	return 1;
}

#endif 

static void
ftrace_ops_control_func(unsigned long ip, unsigned long parent_ip)
{
	struct ftrace_ops *op;

	if (unlikely(trace_recursion_test(TRACE_CONTROL_BIT)))
		return;

	preempt_disable_notrace();
	trace_recursion_set(TRACE_CONTROL_BIT);
	op = rcu_dereference_raw(ftrace_control_list);
	while (op != &ftrace_list_end) {
		if (!ftrace_function_local_disabled(op) &&
		    ftrace_ops_test(op, ip))
			op->func(ip, parent_ip);

		op = rcu_dereference_raw(op->next);
	};
	trace_recursion_clear(TRACE_CONTROL_BIT);
	preempt_enable_notrace();
}

static struct ftrace_ops control_ops = {
	.func = ftrace_ops_control_func,
};

static void
ftrace_ops_list_func(unsigned long ip, unsigned long parent_ip)
{
	struct ftrace_ops *op;

	if (unlikely(trace_recursion_test(TRACE_INTERNAL_BIT)))
		return;

	trace_recursion_set(TRACE_INTERNAL_BIT);
	preempt_disable_notrace();
	op = rcu_dereference_raw(ftrace_ops_list);
	while (op != &ftrace_list_end) {
		if (ftrace_ops_test(op, ip))
			op->func(ip, parent_ip);
		op = rcu_dereference_raw(op->next);
	};
	preempt_enable_notrace();
	trace_recursion_clear(TRACE_INTERNAL_BIT);
}

static void clear_ftrace_swapper(void)
{
	struct task_struct *p;
	int cpu;

	get_online_cpus();
	for_each_online_cpu(cpu) {
		p = idle_task(cpu);
		clear_tsk_trace_trace(p);
	}
	put_online_cpus();
}

static void set_ftrace_swapper(void)
{
	struct task_struct *p;
	int cpu;

	get_online_cpus();
	for_each_online_cpu(cpu) {
		p = idle_task(cpu);
		set_tsk_trace_trace(p);
	}
	put_online_cpus();
}

static void clear_ftrace_pid(struct pid *pid)
{
	struct task_struct *p;

	rcu_read_lock();
	do_each_pid_task(pid, PIDTYPE_PID, p) {
		clear_tsk_trace_trace(p);
	} while_each_pid_task(pid, PIDTYPE_PID, p);
	rcu_read_unlock();

	put_pid(pid);
}

static void set_ftrace_pid(struct pid *pid)
{
	struct task_struct *p;

	rcu_read_lock();
	do_each_pid_task(pid, PIDTYPE_PID, p) {
		set_tsk_trace_trace(p);
	} while_each_pid_task(pid, PIDTYPE_PID, p);
	rcu_read_unlock();
}

static void clear_ftrace_pid_task(struct pid *pid)
{
	if (pid == ftrace_swapper_pid)
		clear_ftrace_swapper();
	else
		clear_ftrace_pid(pid);
}

static void set_ftrace_pid_task(struct pid *pid)
{
	if (pid == ftrace_swapper_pid)
		set_ftrace_swapper();
	else
		set_ftrace_pid(pid);
}

static int ftrace_pid_add(int p)
{
	struct pid *pid;
	struct ftrace_pid *fpid;
	int ret = -EINVAL;

	mutex_lock(&ftrace_lock);

	if (!p)
		pid = ftrace_swapper_pid;
	else
		pid = find_get_pid(p);

	if (!pid)
		goto out;

	ret = 0;

	list_for_each_entry(fpid, &ftrace_pids, list)
		if (fpid->pid == pid)
			goto out_put;

	ret = -ENOMEM;

	fpid = kmalloc(sizeof(*fpid), GFP_KERNEL);
	if (!fpid)
		goto out_put;

	list_add(&fpid->list, &ftrace_pids);
	fpid->pid = pid;

	set_ftrace_pid_task(pid);

	ftrace_update_pid_func();
	ftrace_startup_enable(0);

	mutex_unlock(&ftrace_lock);
	return 0;

out_put:
	if (pid != ftrace_swapper_pid)
		put_pid(pid);

out:
	mutex_unlock(&ftrace_lock);
	return ret;
}

static void ftrace_pid_reset(void)
{
	struct ftrace_pid *fpid, *safe;

	mutex_lock(&ftrace_lock);
	list_for_each_entry_safe(fpid, safe, &ftrace_pids, list) {
		struct pid *pid = fpid->pid;

		clear_ftrace_pid_task(pid);

		list_del(&fpid->list);
		kfree(fpid);
	}

	ftrace_update_pid_func();
	ftrace_startup_enable(0);

	mutex_unlock(&ftrace_lock);
}

static void *fpid_start(struct seq_file *m, loff_t *pos)
{
	mutex_lock(&ftrace_lock);

	if (list_empty(&ftrace_pids) && (!*pos))
		return (void *) 1;

	return seq_list_start(&ftrace_pids, *pos);
}

static void *fpid_next(struct seq_file *m, void *v, loff_t *pos)
{
	if (v == (void *)1)
		return NULL;

	return seq_list_next(v, &ftrace_pids, pos);
}

static void fpid_stop(struct seq_file *m, void *p)
{
	mutex_unlock(&ftrace_lock);
}

static int fpid_show(struct seq_file *m, void *v)
{
	const struct ftrace_pid *fpid = list_entry(v, struct ftrace_pid, list);

	if (v == (void *)1) {
		seq_printf(m, "no pid\n");
		return 0;
	}

	if (fpid->pid == ftrace_swapper_pid)
		seq_printf(m, "swapper tasks\n");
	else
		seq_printf(m, "%u\n", pid_vnr(fpid->pid));

	return 0;
}

static const struct seq_operations ftrace_pid_sops = {
	.start = fpid_start,
	.next = fpid_next,
	.stop = fpid_stop,
	.show = fpid_show,
};

static int
ftrace_pid_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	if ((file->f_mode & FMODE_WRITE) &&
	    (file->f_flags & O_TRUNC))
		ftrace_pid_reset();

	if (file->f_mode & FMODE_READ)
		ret = seq_open(file, &ftrace_pid_sops);

	return ret;
}

static ssize_t
ftrace_pid_write(struct file *filp, const char __user *ubuf,
		   size_t cnt, loff_t *ppos)
{
	char buf[64], *tmp;
	long val;
	int ret;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	tmp = strstrip(buf);
	if (strlen(tmp) == 0)
		return 1;

	ret = strict_strtol(tmp, 10, &val);
	if (ret < 0)
		return ret;

	ret = ftrace_pid_add(val);

	return ret ? ret : cnt;
}

static int
ftrace_pid_release(struct inode *inode, struct file *file)
{
	if (file->f_mode & FMODE_READ)
		seq_release(inode, file);

	return 0;
}

static const struct file_operations ftrace_pid_fops = {
	.open		= ftrace_pid_open,
	.write		= ftrace_pid_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= ftrace_pid_release,
};

static __init int ftrace_init_debugfs(void)
{
	struct dentry *d_tracer;

	d_tracer = tracing_init_dentry();
	if (!d_tracer)
		return 0;

	ftrace_init_dyn_debugfs(d_tracer);

	trace_create_file("set_ftrace_pid", 0644, d_tracer,
			    NULL, &ftrace_pid_fops);

	ftrace_profile_debugfs(d_tracer);

	return 0;
}
fs_initcall(ftrace_init_debugfs);

void ftrace_kill(void)
{
	ftrace_disabled = 1;
	ftrace_enabled = 0;
	clear_ftrace_function();
}

int ftrace_is_dead(void)
{
	return ftrace_disabled;
}

int register_ftrace_function(struct ftrace_ops *ops)
{
	int ret = -1;

	mutex_lock(&ftrace_lock);

	if (unlikely(ftrace_disabled))
		goto out_unlock;

	ret = __register_ftrace_function(ops);
	if (!ret)
		ret = ftrace_startup(ops, 0);


 out_unlock:
	mutex_unlock(&ftrace_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(register_ftrace_function);

int unregister_ftrace_function(struct ftrace_ops *ops)
{
	int ret;

	mutex_lock(&ftrace_lock);
	ret = __unregister_ftrace_function(ops);
	if (!ret)
		ftrace_shutdown(ops, 0);
	mutex_unlock(&ftrace_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(unregister_ftrace_function);

int
ftrace_enable_sysctl(struct ctl_table *table, int write,
		     void __user *buffer, size_t *lenp,
		     loff_t *ppos)
{
	int ret = -ENODEV;

	mutex_lock(&ftrace_lock);

	if (unlikely(ftrace_disabled))
		goto out;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);

	if (ret || !write || (last_ftrace_enabled == !!ftrace_enabled))
		goto out;

	last_ftrace_enabled = !!ftrace_enabled;

	if (ftrace_enabled) {

		ftrace_startup_sysctl();

		
		if (ftrace_ops_list != &ftrace_list_end) {
			if (ftrace_ops_list->next == &ftrace_list_end)
				ftrace_trace_function = ftrace_ops_list->func;
			else
				ftrace_trace_function = ftrace_ops_list_func;
		}

	} else {
		
		ftrace_trace_function = ftrace_stub;

		ftrace_shutdown_sysctl();
	}

 out:
	mutex_unlock(&ftrace_lock);
	return ret;
}

#ifdef CONFIG_FUNCTION_GRAPH_TRACER

static int ftrace_graph_active;
static struct notifier_block ftrace_suspend_notifier;

int ftrace_graph_entry_stub(struct ftrace_graph_ent *trace)
{
	return 0;
}

trace_func_graph_ret_t ftrace_graph_return =
			(trace_func_graph_ret_t)ftrace_stub;
trace_func_graph_ent_t ftrace_graph_entry = ftrace_graph_entry_stub;

static int alloc_retstack_tasklist(struct ftrace_ret_stack **ret_stack_list)
{
	int i;
	int ret = 0;
	unsigned long flags;
	int start = 0, end = FTRACE_RETSTACK_ALLOC_SIZE;
	struct task_struct *g, *t;

	for (i = 0; i < FTRACE_RETSTACK_ALLOC_SIZE; i++) {
		ret_stack_list[i] = kmalloc(FTRACE_RETFUNC_DEPTH
					* sizeof(struct ftrace_ret_stack),
					GFP_KERNEL);
		if (!ret_stack_list[i]) {
			start = 0;
			end = i;
			ret = -ENOMEM;
			goto free;
		}
	}

	read_lock_irqsave(&tasklist_lock, flags);
	do_each_thread(g, t) {
		if (start == end) {
			ret = -EAGAIN;
			goto unlock;
		}

		if (t->ret_stack == NULL) {
			atomic_set(&t->tracing_graph_pause, 0);
			atomic_set(&t->trace_overrun, 0);
			t->curr_ret_stack = -1;
			
			smp_wmb();
			t->ret_stack = ret_stack_list[start++];
		}
	} while_each_thread(g, t);

unlock:
	read_unlock_irqrestore(&tasklist_lock, flags);
free:
	for (i = start; i < end; i++)
		kfree(ret_stack_list[i]);
	return ret;
}

static void
ftrace_graph_probe_sched_switch(void *ignore,
			struct task_struct *prev, struct task_struct *next)
{
	unsigned long long timestamp;
	int index;

	if (trace_flags & TRACE_ITER_SLEEP_TIME)
		return;

	timestamp = trace_clock_local();

	prev->ftrace_timestamp = timestamp;

	
	if (!next->ftrace_timestamp)
		return;

	timestamp -= next->ftrace_timestamp;

	for (index = next->curr_ret_stack; index >= 0; index--)
		next->ret_stack[index].calltime += timestamp;
}

static int start_graph_tracing(void)
{
	struct ftrace_ret_stack **ret_stack_list;
	int ret, cpu;

	ret_stack_list = kmalloc(FTRACE_RETSTACK_ALLOC_SIZE *
				sizeof(struct ftrace_ret_stack *),
				GFP_KERNEL);

	if (!ret_stack_list)
		return -ENOMEM;

	
	for_each_online_cpu(cpu) {
		if (!idle_task(cpu)->ret_stack)
			ftrace_graph_init_idle_task(idle_task(cpu), cpu);
	}

	do {
		ret = alloc_retstack_tasklist(ret_stack_list);
	} while (ret == -EAGAIN);

	if (!ret) {
		ret = register_trace_sched_switch(ftrace_graph_probe_sched_switch, NULL);
		if (ret)
			pr_info("ftrace_graph: Couldn't activate tracepoint"
				" probe to kernel_sched_switch\n");
	}

	kfree(ret_stack_list);
	return ret;
}

static int
ftrace_suspend_notifier_call(struct notifier_block *bl, unsigned long state,
							void *unused)
{
	switch (state) {
	case PM_HIBERNATION_PREPARE:
		pause_graph_tracing();
		break;

	case PM_POST_HIBERNATION:
		unpause_graph_tracing();
		break;
	}
	return NOTIFY_DONE;
}

int register_ftrace_graph(trace_func_graph_ret_t retfunc,
			trace_func_graph_ent_t entryfunc)
{
	int ret = 0;

	mutex_lock(&ftrace_lock);

	
	if (ftrace_graph_active) {
		ret = -EBUSY;
		goto out;
	}

	ftrace_suspend_notifier.notifier_call = ftrace_suspend_notifier_call;
	register_pm_notifier(&ftrace_suspend_notifier);

	ftrace_graph_active++;
	ret = start_graph_tracing();
	if (ret) {
		ftrace_graph_active--;
		goto out;
	}

	ftrace_graph_return = retfunc;
	ftrace_graph_entry = entryfunc;

	ret = ftrace_startup(&global_ops, FTRACE_START_FUNC_RET);

out:
	mutex_unlock(&ftrace_lock);
	return ret;
}

void unregister_ftrace_graph(void)
{
	mutex_lock(&ftrace_lock);

	if (unlikely(!ftrace_graph_active))
		goto out;

	ftrace_graph_active--;
	ftrace_graph_return = (trace_func_graph_ret_t)ftrace_stub;
	ftrace_graph_entry = ftrace_graph_entry_stub;
	ftrace_shutdown(&global_ops, FTRACE_STOP_FUNC_RET);
	unregister_pm_notifier(&ftrace_suspend_notifier);
	unregister_trace_sched_switch(ftrace_graph_probe_sched_switch, NULL);

 out:
	mutex_unlock(&ftrace_lock);
}

static DEFINE_PER_CPU(struct ftrace_ret_stack *, idle_ret_stack);

static void
graph_init_task(struct task_struct *t, struct ftrace_ret_stack *ret_stack)
{
	atomic_set(&t->tracing_graph_pause, 0);
	atomic_set(&t->trace_overrun, 0);
	t->ftrace_timestamp = 0;
	
	smp_wmb();
	t->ret_stack = ret_stack;
}

void ftrace_graph_init_idle_task(struct task_struct *t, int cpu)
{
	t->curr_ret_stack = -1;
	if (t->ret_stack)
		WARN_ON(t->ret_stack != per_cpu(idle_ret_stack, cpu));

	if (ftrace_graph_active) {
		struct ftrace_ret_stack *ret_stack;

		ret_stack = per_cpu(idle_ret_stack, cpu);
		if (!ret_stack) {
			ret_stack = kmalloc(FTRACE_RETFUNC_DEPTH
					    * sizeof(struct ftrace_ret_stack),
					    GFP_KERNEL);
			if (!ret_stack)
				return;
			per_cpu(idle_ret_stack, cpu) = ret_stack;
		}
		graph_init_task(t, ret_stack);
	}
}

void ftrace_graph_init_task(struct task_struct *t)
{
	
	t->ret_stack = NULL;
	t->curr_ret_stack = -1;

	if (ftrace_graph_active) {
		struct ftrace_ret_stack *ret_stack;

		ret_stack = kmalloc(FTRACE_RETFUNC_DEPTH
				* sizeof(struct ftrace_ret_stack),
				GFP_KERNEL);
		if (!ret_stack)
			return;
		graph_init_task(t, ret_stack);
	}
}

void ftrace_graph_exit_task(struct task_struct *t)
{
	struct ftrace_ret_stack	*ret_stack = t->ret_stack;

	t->ret_stack = NULL;
	
	barrier();

	kfree(ret_stack);
}

void ftrace_graph_stop(void)
{
	ftrace_stop();
}
#endif
