/*
 *  linux/arch/h8300/platform/h8300h/ptrace_h8300h.c
 *    ptrace cpu depend helper functions
 *
 *  Yoshinori Sato <ysato@users.sourceforge.jp>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of
 * this archive for more details.
 */

#include <linux/linkage.h>
#include <linux/sched.h>
#include <asm/ptrace.h>

#define CCR_MASK 0x6f    
#define BREAKINST 0x5730 

static const int h8300_register_offset[] = {
	PT_REG(er1), PT_REG(er2), PT_REG(er3), PT_REG(er4),
	PT_REG(er5), PT_REG(er6), PT_REG(er0), PT_REG(orig_er0),
	PT_REG(ccr), PT_REG(pc)
};

long h8300_get_reg(struct task_struct *task, int regno)
{
	switch (regno) {
	case PT_USP:
		return task->thread.usp + sizeof(long)*2;
	case PT_CCR:
	    return *(unsigned short *)(task->thread.esp0 + h8300_register_offset[regno]);
	default:
	    return *(unsigned long *)(task->thread.esp0 + h8300_register_offset[regno]);
	}
}

int h8300_put_reg(struct task_struct *task, int regno, unsigned long data)
{
	unsigned short oldccr;
	switch (regno) {
	case PT_USP:
		task->thread.usp = data - sizeof(long)*2;
	case PT_CCR:
		oldccr = *(unsigned short *)(task->thread.esp0 + h8300_register_offset[regno]);
		oldccr &= ~CCR_MASK;
		data &= CCR_MASK;
		data |= oldccr;
		*(unsigned short *)(task->thread.esp0 + h8300_register_offset[regno]) = data;
		break;
	default:
		*(unsigned long *)(task->thread.esp0 + h8300_register_offset[regno]) = data;
		break;
	}
	return 0;
}

void user_disable_single_step(struct task_struct *child)
{
	if((long)child->thread.breakinfo.addr != -1L) {
		*child->thread.breakinfo.addr = child->thread.breakinfo.inst;
		child->thread.breakinfo.addr = (unsigned short *)-1L;
	}
}

enum jump_type {none,    
		jabs,    
		ind,     
		ret,     
		reg,     
		relb,    
		relw,    
               };

struct optable {
	unsigned char bitpattern;
	unsigned char bitmask;
	signed char length;
	signed char type;
} __attribute__((aligned(1),packed));

#define OPTABLE(ptn,msk,len,jmp)   \
        {                          \
		.bitpattern = ptn, \
		.bitmask    = msk, \
		.length	    = len, \
		.type       = jmp, \
	}

static const struct optable optable_0[] = {
	OPTABLE(0x00,0xff, 1,none), 
	OPTABLE(0x01,0xff,-1,none), 
	OPTABLE(0x02,0xfe, 1,none), 
	OPTABLE(0x04,0xee, 1,none), 
	OPTABLE(0x06,0xfe, 1,none), 
	OPTABLE(0x08,0xea, 1,none), 
	OPTABLE(0x0a,0xee, 1,none), 
	OPTABLE(0x0e,0xee, 1,none), 
	OPTABLE(0x10,0xfc, 1,none), 
	OPTABLE(0x16,0xfe, 1,none), 
	OPTABLE(0x20,0xe0, 1,none), 
	OPTABLE(0x40,0xf0, 1,relb), 
	OPTABLE(0x50,0xfc, 1,none), 
	OPTABLE(0x54,0xfd, 1,ret ), 
	OPTABLE(0x55,0xff, 1,relb), 
	OPTABLE(0x57,0xff, 1,none), 
	OPTABLE(0x58,0xfb, 2,relw), 
	OPTABLE(0x59,0xfb, 1,reg ), 
	OPTABLE(0x5a,0xfb, 2,jabs), 
	OPTABLE(0x5b,0xfb, 2,ind ), 
	OPTABLE(0x60,0xe8, 1,none), 
	OPTABLE(0x68,0xfa, 1,none), 
	OPTABLE(0x6a,0xfe,-2,none), 
	OPTABLE(0x6e,0xfe, 2,none), 
	OPTABLE(0x78,0xff, 4,none), 
	OPTABLE(0x79,0xff, 2,none), 
	OPTABLE(0x7a,0xff, 3,none), 
	OPTABLE(0x7b,0xff, 2,none), 
	OPTABLE(0x7c,0xfc, 2,none), 
	OPTABLE(0x80,0x80, 1,none), 
};

static const struct optable optable_1[] = {
	OPTABLE(0x00,0xff,-3,none), 
	OPTABLE(0x40,0xf0,-3,none), 
	OPTABLE(0x80,0xf0, 1,none), 
	OPTABLE(0xc0,0xc0, 2,none), 
};

static const struct optable optable_2[] = {
	OPTABLE(0x00,0x20, 2,none), 
	OPTABLE(0x20,0x20, 3,none), 
};

static const struct optable optable_3[] = {
	OPTABLE(0x69,0xfb, 2,none), 
	OPTABLE(0x6b,0xff,-4,none), 
	OPTABLE(0x6f,0xff, 3,none), 
	OPTABLE(0x78,0xff, 5,none), 
};

static const struct optable optable_4[] = {
	OPTABLE(0x00,0x78, 3,none), 
	OPTABLE(0x20,0x78, 4,none), 
};

static const struct optables_list {
	const struct optable *ptr;
	int size;
} optables[] = {
#define OPTABLES(no)                                                   \
        {                                                              \
		.ptr  = optable_##no,                                  \
		.size = sizeof(optable_##no) / sizeof(struct optable), \
	}
	OPTABLES(0),
	OPTABLES(1),
	OPTABLES(2),
	OPTABLES(3),
	OPTABLES(4),

};

const unsigned char condmask[] = {
	0x00,0x40,0x01,0x04,0x02,0x08,0x10,0x20
};

static int isbranch(struct task_struct *task,int reson)
{
	unsigned char cond = h8300_get_reg(task, PT_CCR);
	
	__asm__("bld #3,%w0\n\t"
		"bxor #1,%w0\n\t"
		"bst #4,%w0\n\t"
		"bor #2,%w0\n\t"
		"bst #5,%w0\n\t"
		"bld #2,%w0\n\t"
		"bor #0,%w0\n\t"
		"bst #6,%w0\n\t"
		:"=&r"(cond)::"cc");
	cond &= condmask[reson >> 1];
	if (!(reson & 1))
		return cond == 0;
	else
		return cond != 0;
}

static unsigned short *getnextpc(struct task_struct *child, unsigned short *pc)
{
	const struct optable *op;
	unsigned char *fetch_p;
	unsigned char inst;
	unsigned long addr;
	unsigned long *sp;
	int op_len,regno;
	op = optables[0].ptr;
	op_len = optables[0].size;
	fetch_p = (unsigned char *)pc;
	inst = *fetch_p++;
	do {
		if ((inst & op->bitmask) == op->bitpattern) {
			if (op->length < 0) {
				op = optables[-op->length].ptr;
				op_len = optables[-op->length].size + 1;
				inst = *fetch_p++;
			} else {
				switch (op->type) {
				case none:
					return pc + op->length;
				case jabs:
					addr = *(unsigned long *)pc;
					return (unsigned short *)(addr & 0x00ffffff);
				case ind:
					addr = *pc & 0xff;
					return (unsigned short *)(*(unsigned long *)addr);
				case ret:
					sp = (unsigned long *)h8300_get_reg(child, PT_USP);
					return (unsigned short *)(*(sp+2) & 0x00ffffff);
				case reg:
					regno = (*pc >> 4) & 0x07;
					if (regno == 0)
						addr = h8300_get_reg(child, PT_ER0);
					else
						addr = h8300_get_reg(child, regno-1+PT_ER1);
					return (unsigned short *)addr;
				case relb:
					if (inst == 0x55 || isbranch(child,inst & 0x0f))
						pc = (unsigned short *)((unsigned long)pc +
								       ((signed char)(*fetch_p)));
					return pc+1; 
				case relw:
					if (inst == 0x5c || isbranch(child,(*fetch_p & 0xf0) >> 4))
						pc = (unsigned short *)((unsigned long)pc +
								       ((signed short)(*(pc+1))));
					return pc+2; 
				}
			}
		} else
			op++;
	} while(--op_len > 0);
	return NULL;
}


void user_enable_single_step(struct task_struct *child)
{
	unsigned short *nextpc;
	nextpc = getnextpc(child,(unsigned short *)h8300_get_reg(child, PT_PC));
	child->thread.breakinfo.addr = nextpc;
	child->thread.breakinfo.inst = *nextpc;
	*nextpc = BREAKINST;
}

asmlinkage void trace_trap(unsigned long bp)
{
	if ((unsigned long)current->thread.breakinfo.addr == bp) {
		user_disable_single_step(current);
		force_sig(SIGTRAP,current);
	} else
	        force_sig(SIGILL,current);
}

