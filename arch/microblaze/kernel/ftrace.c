/*
 * Ftrace support for Microblaze.
 *
 * Copyright (C) 2009 Michal Simek <monstr@monstr.eu>
 * Copyright (C) 2009 PetaLogix
 *
 * Based on MIPS and PowerPC ftrace code
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <asm/cacheflush.h>
#include <linux/ftrace.h>

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
void prepare_ftrace_return(unsigned long *parent, unsigned long self_addr)
{
	unsigned long old;
	int faulted, err;
	struct ftrace_graph_ent trace;
	unsigned long return_hooker = (unsigned long)
				&return_to_handler;

	if (unlikely(atomic_read(&current->tracing_graph_pause)))
		return;

	asm volatile("	1:	lwi	%0, %2, 0;		\
			2:	swi	%3, %2, 0;		\
				addik	%1, r0, 0;		\
			3:					\
				.section .fixup, \"ax\";	\
			4:	brid	3b;			\
				addik	%1, r0, 1;		\
				.previous;			\
				.section __ex_table,\"a\";	\
				.word	1b,4b;			\
				.word	2b,4b;			\
				.previous;"			\
			: "=&r" (old), "=r" (faulted)
			: "r" (parent), "r" (return_hooker)
	);

	flush_dcache_range((u32)parent, (u32)parent + 4);
	flush_icache_range((u32)parent, (u32)parent + 4);

	if (unlikely(faulted)) {
		ftrace_graph_stop();
		WARN_ON(1);
		return;
	}

	err = ftrace_push_return_trace(old, self_addr, &trace.depth, 0);
	if (err == -EBUSY) {
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

#ifdef CONFIG_DYNAMIC_FTRACE
static int ftrace_modify_code(unsigned long addr, unsigned int value)
{
	int faulted = 0;

	__asm__ __volatile__("	1:	swi	%2, %1, 0;		\
					addik	%0, r0, 0;		\
				2:					\
					.section .fixup, \"ax\";	\
				3:	brid	2b;			\
					addik	%0, r0, 1;		\
					.previous;			\
					.section __ex_table,\"a\";	\
					.word	1b,3b;			\
					.previous;"			\
				: "=r" (faulted)
				: "r" (addr), "r" (value)
	);

	if (unlikely(faulted))
		return -EFAULT;

	flush_dcache_range(addr, addr + 4);
	flush_icache_range(addr, addr + 4);

	return 0;
}

#define MICROBLAZE_NOP 0x80000000
#define MICROBLAZE_BRI 0xb800000C

static unsigned int recorded; 
static unsigned int imm; 

#undef USE_FTRACE_NOP

#ifdef USE_FTRACE_NOP
static unsigned int bralid; 
#endif

int ftrace_make_nop(struct module *mod,
			struct dyn_ftrace *rec, unsigned long addr)
{
	int ret = 0;

	if (recorded == 0) {
		recorded = 1;
		imm = *(unsigned int *)rec->ip;
		pr_debug("%s: imm:0x%x\n", __func__, imm);
#ifdef USE_FTRACE_NOP
		bralid = *(unsigned int *)(rec->ip + 4);
		pr_debug("%s: bralid 0x%x\n", __func__, bralid);
#endif 
	}

#ifdef USE_FTRACE_NOP
	ret = ftrace_modify_code(rec->ip, MICROBLAZE_NOP);
	ret += ftrace_modify_code(rec->ip + 4, MICROBLAZE_NOP);
#else 
	ret = ftrace_modify_code(rec->ip, MICROBLAZE_BRI);
#endif 
	return ret;
}

int ftrace_make_call(struct dyn_ftrace *rec, unsigned long addr)
{
	int ret;
	pr_debug("%s: addr:0x%x, rec->ip: 0x%x, imm:0x%x\n",
		__func__, (unsigned int)addr, (unsigned int)rec->ip, imm);
	ret = ftrace_modify_code(rec->ip, imm);
#ifdef USE_FTRACE_NOP
	pr_debug("%s: bralid:0x%x\n", __func__, bralid);
	ret += ftrace_modify_code(rec->ip + 4, bralid);
#endif 
	return ret;
}

int __init ftrace_dyn_arch_init(void *data)
{
	
	*(unsigned long *)data = 0;

	return 0;
}

int ftrace_update_ftrace_func(ftrace_func_t func)
{
	unsigned long ip = (unsigned long)(&ftrace_call);
	unsigned int upper = (unsigned int)func;
	unsigned int lower = (unsigned int)func;
	int ret = 0;

	
	upper = 0xb0000000 + (upper >> 16); 
	lower = 0x32800000 + (lower & 0xFFFF); 

	pr_debug("%s: func=0x%x, ip=0x%x, upper=0x%x, lower=0x%x\n",
		__func__, (unsigned int)func, (unsigned int)ip, upper, lower);

	
	ret = ftrace_modify_code(ip, upper);
	ret += ftrace_modify_code(ip + 4, lower);

	
	ret += ftrace_modify_code((unsigned long)&ftrace_caller,
				  MICROBLAZE_NOP);

	return ret;
}

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
unsigned int old_jump; 

int ftrace_enable_ftrace_graph_caller(void)
{
	unsigned int ret;
	unsigned long ip = (unsigned long)(&ftrace_call_graph);

	old_jump = *(unsigned int *)ip; 
	ret = ftrace_modify_code(ip, MICROBLAZE_NOP);

	pr_debug("%s: Replace instruction: 0x%x\n", __func__, old_jump);
	return ret;
}

int ftrace_disable_ftrace_graph_caller(void)
{
	unsigned int ret;
	unsigned long ip = (unsigned long)(&ftrace_call_graph);

	ret = ftrace_modify_code(ip, old_jump);

	pr_debug("%s\n", __func__);
	return ret;
}
#endif 
#endif 
