/*
 * Copyright (C) 2008 Matt Fleming <matt@console-pimps.org>
 * Copyright (C) 2008 Paul Mundt <lethal@linux-sh.org>
 *
 * Code for replacing ftrace calls with jumps.
 *
 * Copyright (C) 2007-2008 Steven Rostedt <srostedt@redhat.com>
 *
 * Thanks goes to Ingo Molnar, for suggesting the idea.
 * Mathieu Desnoyers, for suggesting postponing the modifications.
 * Arjan van de Ven, for keeping me straight, and explaining to me
 * the dangers of modifying code on the run.
 */
#include <linux/uaccess.h>
#include <linux/ftrace.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <asm/ftrace.h>
#include <asm/cacheflush.h>
#include <asm/unistd.h>
#include <trace/syscall.h>

#ifdef CONFIG_DYNAMIC_FTRACE
static unsigned char ftrace_replaced_code[MCOUNT_INSN_SIZE];

static unsigned char ftrace_nop[4];
static unsigned char *ftrace_nop_replace(unsigned long ip)
{
	__raw_writel(ip + MCOUNT_INSN_SIZE, ftrace_nop);
	return ftrace_nop;
}

static unsigned char *ftrace_call_replace(unsigned long ip, unsigned long addr)
{
	
	__raw_writel(addr, ftrace_replaced_code);

	return ftrace_replaced_code;
}

/*
 * Modifying code must take extra care. On an SMP machine, if
 * the code being modified is also being executed on another CPU
 * that CPU will have undefined results and possibly take a GPF.
 * We use kstop_machine to stop other CPUS from exectuing code.
 * But this does not stop NMIs from happening. We still need
 * to protect against that. We separate out the modification of
 * the code to take care of this.
 *
 * Two buffers are added: An IP buffer and a "code" buffer.
 *
 * 1) Put the instruction pointer into the IP buffer
 *    and the new code into the "code" buffer.
 * 2) Wait for any running NMIs to finish and set a flag that says
 *    we are modifying code, it is done in an atomic operation.
 * 3) Write the code
 * 4) clear the flag.
 * 5) Wait for any running NMIs to finish.
 *
 * If an NMI is executed, the first thing it does is to call
 * "ftrace_nmi_enter". This will check if the flag is set to write
 * and if it is, it will write what is in the IP and "code" buffers.
 *
 * The trick is, it does not matter if everyone is writing the same
 * content to the code location. Also, if a CPU is executing code
 * it is OK to write to that code location if the contents being written
 * are the same as what exists.
 */
#define MOD_CODE_WRITE_FLAG (1 << 31)	
static atomic_t nmi_running = ATOMIC_INIT(0);
static int mod_code_status;		
static void *mod_code_ip;		
static void *mod_code_newcode;		

static unsigned nmi_wait_count;
static atomic_t nmi_update_count = ATOMIC_INIT(0);

int ftrace_arch_read_dyn_info(char *buf, int size)
{
	int r;

	r = snprintf(buf, size, "%u %u",
		     nmi_wait_count,
		     atomic_read(&nmi_update_count));
	return r;
}

static void clear_mod_flag(void)
{
	int old = atomic_read(&nmi_running);

	for (;;) {
		int new = old & ~MOD_CODE_WRITE_FLAG;

		if (old == new)
			break;

		old = atomic_cmpxchg(&nmi_running, old, new);
	}
}

static void ftrace_mod_code(void)
{
	mod_code_status = probe_kernel_write(mod_code_ip, mod_code_newcode,
					     MCOUNT_INSN_SIZE);

	
	if (mod_code_status)
		clear_mod_flag();
}

void ftrace_nmi_enter(void)
{
	if (atomic_inc_return(&nmi_running) & MOD_CODE_WRITE_FLAG) {
		smp_rmb();
		ftrace_mod_code();
		atomic_inc(&nmi_update_count);
	}
	
	smp_mb();
}

void ftrace_nmi_exit(void)
{
	
	smp_mb();
	atomic_dec(&nmi_running);
}

static void wait_for_nmi_and_set_mod_flag(void)
{
	if (!atomic_cmpxchg(&nmi_running, 0, MOD_CODE_WRITE_FLAG))
		return;

	do {
		cpu_relax();
	} while (atomic_cmpxchg(&nmi_running, 0, MOD_CODE_WRITE_FLAG));

	nmi_wait_count++;
}

static void wait_for_nmi(void)
{
	if (!atomic_read(&nmi_running))
		return;

	do {
		cpu_relax();
	} while (atomic_read(&nmi_running));

	nmi_wait_count++;
}

static int
do_ftrace_mod_code(unsigned long ip, void *new_code)
{
	mod_code_ip = (void *)ip;
	mod_code_newcode = new_code;

	
	smp_mb();

	wait_for_nmi_and_set_mod_flag();

	
	smp_mb();

	ftrace_mod_code();

	
	smp_mb();

	clear_mod_flag();
	wait_for_nmi();

	return mod_code_status;
}

static int ftrace_modify_code(unsigned long ip, unsigned char *old_code,
		       unsigned char *new_code)
{
	unsigned char replaced[MCOUNT_INSN_SIZE];


	
	if (probe_kernel_read(replaced, (void *)ip, MCOUNT_INSN_SIZE))
		return -EFAULT;

	
	if (memcmp(replaced, old_code, MCOUNT_INSN_SIZE) != 0)
		return -EINVAL;

	
	if (do_ftrace_mod_code(ip, new_code))
		return -EPERM;

	flush_icache_range(ip, ip + MCOUNT_INSN_SIZE);

	return 0;
}

int ftrace_update_ftrace_func(ftrace_func_t func)
{
	unsigned long ip = (unsigned long)(&ftrace_call) + MCOUNT_INSN_OFFSET;
	unsigned char old[MCOUNT_INSN_SIZE], *new;

	memcpy(old, (unsigned char *)ip, MCOUNT_INSN_SIZE);
	new = ftrace_call_replace(ip, (unsigned long)func);

	return ftrace_modify_code(ip, old, new);
}

int ftrace_make_nop(struct module *mod,
		    struct dyn_ftrace *rec, unsigned long addr)
{
	unsigned char *new, *old;
	unsigned long ip = rec->ip;

	old = ftrace_call_replace(ip, addr);
	new = ftrace_nop_replace(ip);

	return ftrace_modify_code(rec->ip, old, new);
}

int ftrace_make_call(struct dyn_ftrace *rec, unsigned long addr)
{
	unsigned char *new, *old;
	unsigned long ip = rec->ip;

	old = ftrace_nop_replace(ip);
	new = ftrace_call_replace(ip, addr);

	return ftrace_modify_code(rec->ip, old, new);
}

int __init ftrace_dyn_arch_init(void *data)
{
	
	__raw_writel(0, (unsigned long)data);

	return 0;
}
#endif 

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
#ifdef CONFIG_DYNAMIC_FTRACE
extern void ftrace_graph_call(void);

static int ftrace_mod(unsigned long ip, unsigned long old_addr,
		      unsigned long new_addr)
{
	unsigned char code[MCOUNT_INSN_SIZE];

	if (probe_kernel_read(code, (void *)ip, MCOUNT_INSN_SIZE))
		return -EFAULT;

	if (old_addr != __raw_readl((unsigned long *)code))
		return -EINVAL;

	__raw_writel(new_addr, ip);
	return 0;
}

int ftrace_enable_ftrace_graph_caller(void)
{
	unsigned long ip, old_addr, new_addr;

	ip = (unsigned long)(&ftrace_graph_call) + GRAPH_INSN_OFFSET;
	old_addr = (unsigned long)(&skip_trace);
	new_addr = (unsigned long)(&ftrace_graph_caller);

	return ftrace_mod(ip, old_addr, new_addr);
}

int ftrace_disable_ftrace_graph_caller(void)
{
	unsigned long ip, old_addr, new_addr;

	ip = (unsigned long)(&ftrace_graph_call) + GRAPH_INSN_OFFSET;
	old_addr = (unsigned long)(&ftrace_graph_caller);
	new_addr = (unsigned long)(&skip_trace);

	return ftrace_mod(ip, old_addr, new_addr);
}
#endif 

void prepare_ftrace_return(unsigned long *parent, unsigned long self_addr)
{
	unsigned long old;
	int faulted, err;
	struct ftrace_graph_ent trace;
	unsigned long return_hooker = (unsigned long)&return_to_handler;

	if (unlikely(atomic_read(&current->tracing_graph_pause)))
		return;

	__asm__ __volatile__(
		"1:						\n\t"
		"mov.l		@%2, %0				\n\t"
		"2:						\n\t"
		"mov.l		%3, @%2				\n\t"
		"mov		#0, %1				\n\t"
		"3:						\n\t"
		".section .fixup, \"ax\"			\n\t"
		"4:						\n\t"
		"mov.l		5f, %0				\n\t"
		"jmp		@%0				\n\t"
		" mov		#1, %1				\n\t"
		".balign 4					\n\t"
		"5:	.long 3b				\n\t"
		".previous					\n\t"
		".section __ex_table,\"a\"			\n\t"
		".long 1b, 4b					\n\t"
		".long 2b, 4b					\n\t"
		".previous					\n\t"
		: "=&r" (old), "=r" (faulted)
		: "r" (parent), "r" (return_hooker)
	);

	if (unlikely(faulted)) {
		ftrace_graph_stop();
		WARN_ON(1);
		return;
	}

	err = ftrace_push_return_trace(old, self_addr, &trace.depth, 0);
	if (err == -EBUSY) {
		__raw_writel(old, parent);
		return;
	}

	trace.func = self_addr;

	
	if (!ftrace_graph_entry(&trace)) {
		current->curr_ret_stack--;
		__raw_writel(old, parent);
	}
}
#endif 
