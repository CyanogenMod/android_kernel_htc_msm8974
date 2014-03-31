/*
 * Code for replacing ftrace calls with jumps.
 *
 * Copyright (C) 2007-2008 Steven Rostedt <srostedt@redhat.com>
 *
 * Thanks goes out to P.A. Semi, Inc for supplying me with a PPC64 box.
 *
 * Added function graph tracer code, taken from x86 that was written
 * by Frederic Weisbecker, and ported to PPC by Steven Rostedt.
 *
 */

#include <linux/spinlock.h>
#include <linux/hardirq.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/ftrace.h>
#include <linux/percpu.h>
#include <linux/init.h>
#include <linux/list.h>

#include <asm/cacheflush.h>
#include <asm/code-patching.h>
#include <asm/ftrace.h>
#include <asm/syscall.h>


#ifdef CONFIG_DYNAMIC_FTRACE
static unsigned int
ftrace_call_replace(unsigned long ip, unsigned long addr, int link)
{
	unsigned int op;

	addr = ppc_function_entry((void *)addr);

	
	op = create_branch((unsigned int *)ip, addr, link ? 1 : 0);

	return op;
}

static int
ftrace_modify_code(unsigned long ip, unsigned int old, unsigned int new)
{
	unsigned int replaced;


	
	if (probe_kernel_read(&replaced, (void *)ip, MCOUNT_INSN_SIZE))
		return -EFAULT;

	
	if (replaced != old)
		return -EINVAL;

	
	if (probe_kernel_write((void *)ip, &new, MCOUNT_INSN_SIZE))
		return -EPERM;

	flush_icache_range(ip, ip + 8);

	return 0;
}

static int test_24bit_addr(unsigned long ip, unsigned long addr)
{

	
	return create_branch((unsigned int *)ip, addr, 0);
}

#ifdef CONFIG_MODULES

static int is_bl_op(unsigned int op)
{
	return (op & 0xfc000003) == 0x48000001;
}

static unsigned long find_bl_target(unsigned long ip, unsigned int op)
{
	static int offset;

	offset = (op & 0x03fffffc);
	
	if (offset & 0x02000000)
		offset |= 0xfe000000;

	return ip + (long)offset;
}

#ifdef CONFIG_PPC64
static int
__ftrace_make_nop(struct module *mod,
		  struct dyn_ftrace *rec, unsigned long addr)
{
	unsigned int op;
	unsigned int jmp[5];
	unsigned long ptr;
	unsigned long ip = rec->ip;
	unsigned long tramp;
	int offset;

	
	if (probe_kernel_read(&op, (void *)ip, sizeof(int)))
		return -EFAULT;

	
	if (!is_bl_op(op)) {
		printk(KERN_ERR "Not expected bl: opcode is %x\n", op);
		return -EINVAL;
	}

	
	tramp = find_bl_target(ip, op);


	pr_devel("ip:%lx jumps to %lx r2: %lx", ip, tramp, mod->arch.toc);

	
	if (probe_kernel_read(jmp, (void *)tramp, sizeof(jmp))) {
		printk(KERN_ERR "Failed to read %lx\n", tramp);
		return -EFAULT;
	}

	pr_devel(" %08x %08x", jmp[0], jmp[1]);

	
	if (((jmp[0] & 0xffff0000) != 0x3d820000) ||
	    ((jmp[1] & 0xffff0000) != 0x398c0000) ||
	    (jmp[2] != 0xf8410028) ||
	    (jmp[3] != 0xe96c0020) ||
	    (jmp[4] != 0xe84c0028)) {
		printk(KERN_ERR "Not a trampoline\n");
		return -EINVAL;
	}

	
	offset = ((unsigned)((unsigned short)jmp[0]) << 16) +
		(int)((short)jmp[1]);

	pr_devel(" %x ", offset);

	
	tramp = mod->arch.toc + offset + 32;
	pr_devel("toc: %lx", tramp);

	if (probe_kernel_read(jmp, (void *)tramp, 8)) {
		printk(KERN_ERR "Failed to read %lx\n", tramp);
		return -EFAULT;
	}

	pr_devel(" %08x %08x\n", jmp[0], jmp[1]);

	ptr = ((unsigned long)jmp[0] << 32) + jmp[1];

	
	if (ptr != ppc_function_entry((void *)addr)) {
		printk(KERN_ERR "addr does not match %lx\n", ptr);
		return -EINVAL;
	}

	if (probe_kernel_read(&op, (void *)(ip+4), MCOUNT_INSN_SIZE))
		return -EFAULT;

	if (op != 0xe8410028) {
		printk(KERN_ERR "Next line is not ld! (%08x)\n", op);
		return -EINVAL;
	}


	op = 0x48000008;	

	if (probe_kernel_write((void *)ip, &op, MCOUNT_INSN_SIZE))
		return -EPERM;


	flush_icache_range(ip, ip + 8);

	return 0;
}

#else 
static int
__ftrace_make_nop(struct module *mod,
		  struct dyn_ftrace *rec, unsigned long addr)
{
	unsigned int op;
	unsigned int jmp[4];
	unsigned long ip = rec->ip;
	unsigned long tramp;

	if (probe_kernel_read(&op, (void *)ip, MCOUNT_INSN_SIZE))
		return -EFAULT;

	
	if (!is_bl_op(op)) {
		printk(KERN_ERR "Not expected bl: opcode is %x\n", op);
		return -EINVAL;
	}

	
	tramp = find_bl_target(ip, op);


	pr_devel("ip:%lx jumps to %lx", ip, tramp);

	
	if (probe_kernel_read(jmp, (void *)tramp, sizeof(jmp))) {
		printk(KERN_ERR "Failed to read %lx\n", tramp);
		return -EFAULT;
	}

	pr_devel(" %08x %08x ", jmp[0], jmp[1]);

	
	if (((jmp[0] & 0xffff0000) != 0x3d600000) ||
	    ((jmp[1] & 0xffff0000) != 0x396b0000) ||
	    (jmp[2] != 0x7d6903a6) ||
	    (jmp[3] != 0x4e800420)) {
		printk(KERN_ERR "Not a trampoline\n");
		return -EINVAL;
	}

	tramp = (jmp[1] & 0xffff) |
		((jmp[0] & 0xffff) << 16);
	if (tramp & 0x8000)
		tramp -= 0x10000;

	pr_devel(" %lx ", tramp);

	if (tramp != addr) {
		printk(KERN_ERR
		       "Trampoline location %08lx does not match addr\n",
		       tramp);
		return -EINVAL;
	}

	op = PPC_INST_NOP;

	if (probe_kernel_write((void *)ip, &op, MCOUNT_INSN_SIZE))
		return -EPERM;

	flush_icache_range(ip, ip + 8);

	return 0;
}
#endif 
#endif 

int ftrace_make_nop(struct module *mod,
		    struct dyn_ftrace *rec, unsigned long addr)
{
	unsigned long ip = rec->ip;
	unsigned int old, new;

	if (test_24bit_addr(ip, addr)) {
		
		old = ftrace_call_replace(ip, addr, 1);
		new = PPC_INST_NOP;
		return ftrace_modify_code(ip, old, new);
	}

#ifdef CONFIG_MODULES
	if (!rec->arch.mod) {
		if (!mod) {
			printk(KERN_ERR "No module loaded addr=%lx\n",
			       addr);
			return -EFAULT;
		}
		rec->arch.mod = mod;
	} else if (mod) {
		if (mod != rec->arch.mod) {
			printk(KERN_ERR
			       "Record mod %p not equal to passed in mod %p\n",
			       rec->arch.mod, mod);
			return -EINVAL;
		}
		
	} else
		mod = rec->arch.mod;

	return __ftrace_make_nop(mod, rec, addr);
#else
	
	return -EINVAL;
#endif 
}

#ifdef CONFIG_MODULES
#ifdef CONFIG_PPC64
static int
__ftrace_make_call(struct dyn_ftrace *rec, unsigned long addr)
{
	unsigned int op[2];
	unsigned long ip = rec->ip;

	
	if (probe_kernel_read(op, (void *)ip, MCOUNT_INSN_SIZE * 2))
		return -EFAULT;

	if (((op[0] != 0x48000008) || (op[1] != 0xe8410028)) &&
	    ((op[0] != PPC_INST_NOP) || (op[1] != PPC_INST_NOP))) {
		printk(KERN_ERR "Expected NOPs but have %x %x\n", op[0], op[1]);
		return -EINVAL;
	}

	
	if (!rec->arch.mod->arch.tramp) {
		printk(KERN_ERR "No ftrace trampoline\n");
		return -EINVAL;
	}

	
	op[0] = create_branch((unsigned int *)ip,
			      rec->arch.mod->arch.tramp, BRANCH_SET_LINK);
	if (!op[0]) {
		printk(KERN_ERR "REL24 out of range!\n");
		return -EINVAL;
	}

	
	op[1] = 0xe8410028;

	pr_devel("write to %lx\n", rec->ip);

	if (probe_kernel_write((void *)ip, op, MCOUNT_INSN_SIZE * 2))
		return -EPERM;

	flush_icache_range(ip, ip + 8);

	return 0;
}
#else
static int
__ftrace_make_call(struct dyn_ftrace *rec, unsigned long addr)
{
	unsigned int op;
	unsigned long ip = rec->ip;

	
	if (probe_kernel_read(&op, (void *)ip, MCOUNT_INSN_SIZE))
		return -EFAULT;

	
	if (op != PPC_INST_NOP) {
		printk(KERN_ERR "Expected NOP but have %x\n", op);
		return -EINVAL;
	}

	
	if (!rec->arch.mod->arch.tramp) {
		printk(KERN_ERR "No ftrace trampoline\n");
		return -EINVAL;
	}

	
	op = create_branch((unsigned int *)ip,
			   rec->arch.mod->arch.tramp, BRANCH_SET_LINK);
	if (!op) {
		printk(KERN_ERR "REL24 out of range!\n");
		return -EINVAL;
	}

	pr_devel("write to %lx\n", rec->ip);

	if (probe_kernel_write((void *)ip, &op, MCOUNT_INSN_SIZE))
		return -EPERM;

	flush_icache_range(ip, ip + 8);

	return 0;
}
#endif 
#endif 

int ftrace_make_call(struct dyn_ftrace *rec, unsigned long addr)
{
	unsigned long ip = rec->ip;
	unsigned int old, new;

	if (test_24bit_addr(ip, addr)) {
		
		old = PPC_INST_NOP;
		new = ftrace_call_replace(ip, addr, 1);
		return ftrace_modify_code(ip, old, new);
	}

#ifdef CONFIG_MODULES
	if (!rec->arch.mod) {
		printk(KERN_ERR "No module loaded\n");
		return -EINVAL;
	}

	return __ftrace_make_call(rec, addr);
#else
	
	return -EINVAL;
#endif 
}

int ftrace_update_ftrace_func(ftrace_func_t func)
{
	unsigned long ip = (unsigned long)(&ftrace_call);
	unsigned int old, new;
	int ret;

	old = *(unsigned int *)&ftrace_call;
	new = ftrace_call_replace(ip, (unsigned long)func, 1);
	ret = ftrace_modify_code(ip, old, new);

	return ret;
}

int __init ftrace_dyn_arch_init(void *data)
{
	
	unsigned long *p = data;

	*p = 0;

	return 0;
}
#endif 

#ifdef CONFIG_FUNCTION_GRAPH_TRACER

#ifdef CONFIG_DYNAMIC_FTRACE
extern void ftrace_graph_call(void);
extern void ftrace_graph_stub(void);

int ftrace_enable_ftrace_graph_caller(void)
{
	unsigned long ip = (unsigned long)(&ftrace_graph_call);
	unsigned long addr = (unsigned long)(&ftrace_graph_caller);
	unsigned long stub = (unsigned long)(&ftrace_graph_stub);
	unsigned int old, new;

	old = ftrace_call_replace(ip, stub, 0);
	new = ftrace_call_replace(ip, addr, 0);

	return ftrace_modify_code(ip, old, new);
}

int ftrace_disable_ftrace_graph_caller(void)
{
	unsigned long ip = (unsigned long)(&ftrace_graph_call);
	unsigned long addr = (unsigned long)(&ftrace_graph_caller);
	unsigned long stub = (unsigned long)(&ftrace_graph_stub);
	unsigned int old, new;

	old = ftrace_call_replace(ip, addr, 0);
	new = ftrace_call_replace(ip, stub, 0);

	return ftrace_modify_code(ip, old, new);
}
#endif 

#ifdef CONFIG_PPC64
extern void mod_return_to_handler(void);
#endif

void prepare_ftrace_return(unsigned long *parent, unsigned long self_addr)
{
	unsigned long old;
	int faulted;
	struct ftrace_graph_ent trace;
	unsigned long return_hooker = (unsigned long)&return_to_handler;

	if (unlikely(atomic_read(&current->tracing_graph_pause)))
		return;

#ifdef CONFIG_PPC64
	
	if (REGION_ID(self_addr) != KERNEL_REGION_ID)
		return_hooker = (unsigned long)&mod_return_to_handler;
#endif

	return_hooker = ppc_function_entry((void *)return_hooker);

	asm volatile(
		"1: " PPC_LL "%[old], 0(%[parent])\n"
		"2: " PPC_STL "%[return_hooker], 0(%[parent])\n"
		"   li %[faulted], 0\n"
		"3:\n"

		".section .fixup, \"ax\"\n"
		"4: li %[faulted], 1\n"
		"   b 3b\n"
		".previous\n"

		".section __ex_table,\"a\"\n"
			PPC_LONG_ALIGN "\n"
			PPC_LONG "1b,4b\n"
			PPC_LONG "2b,4b\n"
		".previous"

		: [old] "=&r" (old), [faulted] "=r" (faulted)
		: [parent] "r" (parent), [return_hooker] "r" (return_hooker)
		: "memory"
	);

	if (unlikely(faulted)) {
		ftrace_graph_stop();
		WARN_ON(1);
		return;
	}

	if (ftrace_push_return_trace(old, self_addr, &trace.depth, 0) == -EBUSY) {
		*parent = old;
		return;
	}

	trace.func = self_addr;

	
	if (!ftrace_graph_entry(&trace)) {
		current->curr_ret_stack--;
		*parent = old;
	}
}
#endif 

#if defined(CONFIG_FTRACE_SYSCALLS) && defined(CONFIG_PPC64)
unsigned long __init arch_syscall_addr(int nr)
{
	return sys_call_table[nr*2];
}
#endif 
