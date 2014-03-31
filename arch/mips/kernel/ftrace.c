/*
 * Code for replacing ftrace calls with jumps.
 *
 * Copyright (C) 2007-2008 Steven Rostedt <srostedt@redhat.com>
 * Copyright (C) 2009, 2010 DSLab, Lanzhou University, China
 * Author: Wu Zhangjin <wuzhangjin@gmail.com>
 *
 * Thanks goes to Steven Rostedt for writing the original x86 version.
 */

#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/ftrace.h>

#include <asm/asm.h>
#include <asm/asm-offsets.h>
#include <asm/cacheflush.h>
#include <asm/uasm.h>

#include <asm-generic/sections.h>

#if defined(KBUILD_MCOUNT_RA_ADDRESS) && defined(CONFIG_32BIT)
#define MCOUNT_OFFSET_INSNS 5
#else
#define MCOUNT_OFFSET_INSNS 4
#endif

static inline int in_kernel_space(unsigned long ip)
{
	if (ip >= (unsigned long)_stext &&
	    ip <= (unsigned long)_etext)
		return 1;
	return 0;
}

#ifdef CONFIG_DYNAMIC_FTRACE

#define JAL 0x0c000000		
#define ADDR_MASK 0x03ffffff	
#define JUMP_RANGE_MASK ((1UL << 28) - 1)

#define INSN_NOP 0x00000000	
#define INSN_JAL(addr)	\
	((unsigned int)(JAL | (((addr) >> 2) & ADDR_MASK)))

static unsigned int insn_jal_ftrace_caller __read_mostly;
static unsigned int insn_lui_v1_hi16_mcount __read_mostly;
static unsigned int insn_j_ftrace_graph_caller __maybe_unused __read_mostly;

static inline void ftrace_dyn_arch_init_insns(void)
{
	u32 *buf;
	unsigned int v1;

	
	v1 = 3;
	buf = (u32 *)&insn_lui_v1_hi16_mcount;
	UASM_i_LA_mostly(&buf, v1, MCOUNT_ADDR);

	
	buf = (u32 *)&insn_jal_ftrace_caller;
	uasm_i_jal(&buf, (FTRACE_ADDR + 8) & JUMP_RANGE_MASK);

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	
	buf = (u32 *)&insn_j_ftrace_graph_caller;
	uasm_i_j(&buf, (unsigned long)ftrace_graph_caller & JUMP_RANGE_MASK);
#endif
}

static int ftrace_modify_code(unsigned long ip, unsigned int new_code)
{
	int faulted;

	
	safe_store_code(new_code, ip, faulted);

	if (unlikely(faulted))
		return -EFAULT;

	flush_icache_range(ip, ip + 8);

	return 0;
}


#define INSN_B_1F (0x10000000 | MCOUNT_OFFSET_INSNS)

int ftrace_make_nop(struct module *mod,
		    struct dyn_ftrace *rec, unsigned long addr)
{
	unsigned int new;
	unsigned long ip = rec->ip;

	new = in_kernel_space(ip) ? INSN_NOP : INSN_B_1F;

	return ftrace_modify_code(ip, new);
}

int ftrace_make_call(struct dyn_ftrace *rec, unsigned long addr)
{
	unsigned int new;
	unsigned long ip = rec->ip;

	new = in_kernel_space(ip) ? insn_jal_ftrace_caller :
		insn_lui_v1_hi16_mcount;

	return ftrace_modify_code(ip, new);
}

#define FTRACE_CALL_IP ((unsigned long)(&ftrace_call))

int ftrace_update_ftrace_func(ftrace_func_t func)
{
	unsigned int new;

	new = INSN_JAL((unsigned long)func);

	return ftrace_modify_code(FTRACE_CALL_IP, new);
}

int __init ftrace_dyn_arch_init(void *data)
{
	
	ftrace_dyn_arch_init_insns();

	
	ftrace_modify_code(MCOUNT_ADDR, INSN_NOP);

	
	*(unsigned long *)data = 0;

	return 0;
}
#endif	

#ifdef CONFIG_FUNCTION_GRAPH_TRACER

#ifdef CONFIG_DYNAMIC_FTRACE

extern void ftrace_graph_call(void);
#define FTRACE_GRAPH_CALL_IP	((unsigned long)(&ftrace_graph_call))

int ftrace_enable_ftrace_graph_caller(void)
{
	return ftrace_modify_code(FTRACE_GRAPH_CALL_IP,
			insn_j_ftrace_graph_caller);
}

int ftrace_disable_ftrace_graph_caller(void)
{
	return ftrace_modify_code(FTRACE_GRAPH_CALL_IP, INSN_NOP);
}

#endif	

#ifndef KBUILD_MCOUNT_RA_ADDRESS

#define S_RA_SP	(0xafbf << 16)	
#define S_R_SP	(0xafb0 << 16)  
#define OFFSET_MASK	0xffff	

unsigned long ftrace_get_parent_ra_addr(unsigned long self_ra, unsigned long
		old_parent_ra, unsigned long parent_ra_addr, unsigned long fp)
{
	unsigned long sp, ip, tmp;
	unsigned int code;
	int faulted;

	ip = self_ra - (in_kernel_space(self_ra) ? 16 : 24);

	do {
		
		safe_load_code(code, ip, faulted);

		if (unlikely(faulted))
			return 0;
		if ((code & S_R_SP) != S_R_SP)
			return parent_ra_addr;

		
		ip -= 4;
	} while ((code & S_RA_SP) != S_RA_SP);

	sp = fp + (code & OFFSET_MASK);

	
	safe_load_stack(tmp, sp, faulted);
	if (unlikely(faulted))
		return 0;

	if (tmp == old_parent_ra)
		return sp;
	return 0;
}

#endif	

void prepare_ftrace_return(unsigned long *parent_ra_addr, unsigned long self_ra,
			   unsigned long fp)
{
	unsigned long old_parent_ra;
	struct ftrace_graph_ent trace;
	unsigned long return_hooker = (unsigned long)
	    &return_to_handler;
	int faulted, insns;

	if (unlikely(atomic_read(&current->tracing_graph_pause)))
		return;


	
	safe_load_stack(old_parent_ra, parent_ra_addr, faulted);
	if (unlikely(faulted))
		goto out;
#ifndef KBUILD_MCOUNT_RA_ADDRESS
	parent_ra_addr = (unsigned long *)ftrace_get_parent_ra_addr(self_ra,
			old_parent_ra, (unsigned long)parent_ra_addr, fp);
	if (parent_ra_addr == 0)
		goto out;
#endif
	
	safe_store_stack(return_hooker, parent_ra_addr, faulted);
	if (unlikely(faulted))
		goto out;

	if (ftrace_push_return_trace(old_parent_ra, self_ra, &trace.depth, fp)
	    == -EBUSY) {
		*parent_ra_addr = old_parent_ra;
		return;
	}


	insns = in_kernel_space(self_ra) ? 2 : MCOUNT_OFFSET_INSNS + 1;
	trace.func = self_ra - (MCOUNT_INSN_SIZE * insns);

	
	if (!ftrace_graph_entry(&trace)) {
		current->curr_ret_stack--;
		*parent_ra_addr = old_parent_ra;
	}
	return;
out:
	ftrace_graph_stop();
	WARN_ON(1);
}
#endif	
