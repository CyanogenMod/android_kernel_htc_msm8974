#ifndef _ASM_ARCH_IRQ_H
#define _ASM_ARCH_IRQ_H

#include <hwregs/intr_vect.h>

#define NR_IRQS NBR_INTR_VECT 
#define FIRST_IRQ 0x31 
#define NR_REAL_IRQS (NBR_INTR_VECT - FIRST_IRQ) 
#if NR_REAL_IRQS > 32
#define MACH_IRQS 64
#else
#define MACH_IRQS 32
#endif

#ifndef __ASSEMBLY__
typedef void (*irqvectptr)(void);

struct etrax_interrupt_vector {
	irqvectptr v[256];
};

extern struct etrax_interrupt_vector *etrax_irv;	

void crisv32_mask_irq(int irq);
void crisv32_unmask_irq(int irq);

void set_exception_vector(int n, irqvectptr addr);

#define SAVE_ALL \
	"subq 12,$sp\n\t"	\
	"move $erp,[$sp]\n\t"	\
	"subq 4,$sp\n\t"	\
	"move $srp,[$sp]\n\t"	\
	"subq 4,$sp\n\t"	\
	"move $ccs,[$sp]\n\t"	\
	"subq 4,$sp\n\t"	\
	"move $spc,[$sp]\n\t"	\
	"subq 4,$sp\n\t"	\
	"move $mof,[$sp]\n\t"	\
	"subq 4,$sp\n\t"	\
	"move $srs,[$sp]\n\t"	\
	"subq 4,$sp\n\t"	\
	"move.d $acr,[$sp]\n\t"	\
	"subq 14*4,$sp\n\t"	\
	"movem $r13,[$sp]\n\t"	\
	"subq 4,$sp\n\t"	\
	"move.d $r10,[$sp]\n"

#define STR2(x) #x
#define STR(x) STR2(x)

#define IRQ_NAME2(nr) nr##_interrupt(void)
#define IRQ_NAME(nr) IRQ_NAME2(IRQ##nr)

#ifdef CONFIG_ETRAX_KGDB
#define KGDB_FIXUP \
	"move $ccs, $r10\n\t"		\
	"or.d (1<<9), $r10\n\t"		\
	"move $r10, $ccs\n\t"
#else
#define KGDB_FIXUP ""
#endif

#define BUILD_IRQ(nr)		        \
void IRQ_NAME(nr);			\
__asm__ (				\
	".text\n\t"			\
	"IRQ" #nr "_interrupt:\n\t" 	\
	SAVE_ALL			\
	KGDB_FIXUP                      \
	"move.d "#nr",$r10\n\t"		\
	"move.d $sp, $r12\n\t"          \
	"jsr crisv32_do_IRQ\n\t"       	\
	"moveq 1, $r11\n\t"		\
	"jump ret_from_intr\n\t"	\
	"nop\n\t");
#define BUILD_TIMER_IRQ(nr, mask) 	\
void IRQ_NAME(nr);			\
__asm__ (				\
	".text\n\t"			\
	"IRQ" #nr "_interrupt:\n\t"	\
	SAVE_ALL			\
        KGDB_FIXUP                      \
	"move.d "#nr",$r10\n\t"		\
	"move.d $sp,$r12\n\t"		\
	"jsr crisv32_do_IRQ\n\t"	\
	"moveq 0,$r11\n\t"		\
	"jump ret_from_intr\n\t"	\
	"nop\n\t");

#endif 
#endif 
