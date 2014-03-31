/*
 *	linux/arch/cris/kernel/irq.c
 *
 *      Copyright (c) 2000-2002 Axis Communications AB
 *
 *      Authors: Bjorn Wesen (bjornw@axis.com)
 *
 *      This file contains the interrupt vectors and some 
 *      helper functions
 *
 */

#include <asm/irq.h>
#include <asm/current.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/init.h>

#define crisv10_mask_irq(irq_nr) (*R_VECT_MASK_CLR = 1 << (irq_nr));
#define crisv10_unmask_irq(irq_nr) (*R_VECT_MASK_SET = 1 << (irq_nr));

extern void kgdb_init(void);
extern void breakpoint(void);


void
set_int_vector(int n, irqvectptr addr)
{
	etrax_irv->v[n + 0x20] = (irqvectptr)addr;
}


void
set_break_vector(int n, irqvectptr addr)
{
	unsigned short *jinstr = (unsigned short *)&etrax_irv->v[n*2];
	unsigned long *jaddr = (unsigned long *)(jinstr + 1);

	
	
	*jinstr = 0x0d3f;
	*jaddr = (unsigned long)addr;

	
}

/*
 * This builds up the IRQ handler stubs using some ugly macros in irq.h
 *
 * These macros create the low-level assembly IRQ routines that do all
 * the operations that are needed. They are also written to be fast - and to
 * disable interrupts as little as humanly possible.
 *
 */

void hwbreakpoint(void);
void IRQ1_interrupt(void);
BUILD_TIMER_IRQ(2, 0x04)       
BUILD_IRQ(3, 0x08)
BUILD_IRQ(4, 0x10)
BUILD_IRQ(5, 0x20)
BUILD_IRQ(6, 0x40)
BUILD_IRQ(7, 0x80)
BUILD_IRQ(8, 0x100)
BUILD_IRQ(9, 0x200)
BUILD_IRQ(10, 0x400)
BUILD_IRQ(11, 0x800)
BUILD_IRQ(12, 0x1000)
BUILD_IRQ(13, 0x2000)
void mmu_bus_fault(void);      
void multiple_interrupt(void); 
BUILD_IRQ(16, 0x10000 | 0x20000)  
BUILD_IRQ(17, 0x20000 | 0x10000)  
BUILD_IRQ(18, 0x40000)
BUILD_IRQ(19, 0x80000)
BUILD_IRQ(20, 0x100000)
BUILD_IRQ(21, 0x200000)
BUILD_IRQ(22, 0x400000)
BUILD_IRQ(23, 0x800000)
BUILD_IRQ(24, 0x1000000)
BUILD_IRQ(25, 0x2000000)
BUILD_IRQ(31, 0x80000000)
 

static void (*interrupt[NR_IRQS])(void) = {
	NULL, NULL, IRQ2_interrupt, IRQ3_interrupt,
	IRQ4_interrupt, IRQ5_interrupt, IRQ6_interrupt, IRQ7_interrupt,
	IRQ8_interrupt, IRQ9_interrupt, IRQ10_interrupt, IRQ11_interrupt,
	IRQ12_interrupt, IRQ13_interrupt, NULL, NULL,	
	IRQ16_interrupt, IRQ17_interrupt, IRQ18_interrupt, IRQ19_interrupt,	
	IRQ20_interrupt, IRQ21_interrupt, IRQ22_interrupt, IRQ23_interrupt,	
	IRQ24_interrupt, IRQ25_interrupt, NULL, NULL, NULL, NULL, NULL,
	IRQ31_interrupt
};

static void enable_crisv10_irq(struct irq_data *data)
{
	crisv10_unmask_irq(data->irq);
}

static void disable_crisv10_irq(struct irq_data *data)
{
	crisv10_mask_irq(data->irq);
}

static struct irq_chip crisv10_irq_type = {
	.name		= "CRISv10",
	.irq_shutdown	= disable_crisv10_irq,
	.irq_enable	= enable_crisv10_irq,
	.irq_disable	= disable_crisv10_irq,
};

void weird_irq(void);
void system_call(void);  
void do_sigtrap(void); 
void gdb_handle_breakpoint(void); 

extern void do_IRQ(int irq, struct pt_regs * regs);

void do_multiple_IRQ(struct pt_regs* regs)
{
	int bit;
	unsigned masked;
	unsigned mask;
	unsigned ethmask = 0;

	
	mask = masked = *R_VECT_MASK_RD;

	
	mask &= ~(IO_MASK(R_VECT_MASK_RD, timer0));

	if (mask &
	    (IO_STATE(R_VECT_MASK_RD, dma0, active) |
	     IO_STATE(R_VECT_MASK_RD, dma1, active))) {
		ethmask = (IO_MASK(R_VECT_MASK_RD, dma0) |
			   IO_MASK(R_VECT_MASK_RD, dma1));
	}

	
	*R_VECT_MASK_CLR = (mask | ethmask);

	irq_enter();

	
	for (bit = 2; bit < 32; bit++) {
		if (masked & (1 << bit)) {
			do_IRQ(bit, regs);
		}
	}

	
	irq_exit();

	
	*R_VECT_MASK_SET = (masked | ethmask);
}


void __init
init_IRQ(void)
{
	int i;

	

#ifndef CONFIG_SVINTO_SIM
	*R_IRQ_MASK0_CLR = 0xffffffff;
	*R_IRQ_MASK1_CLR = 0xffffffff;
	*R_IRQ_MASK2_CLR = 0xffffffff;
#endif

	*R_VECT_MASK_CLR = 0xffffffff;

        for (i = 0; i < 256; i++)
               etrax_irv->v[i] = weird_irq;

	
	for(i = 2; i < NR_IRQS; i++) {
		irq_set_chip_and_handler(i, &crisv10_irq_type,
					 handle_simple_irq);
		set_int_vector(i, interrupt[i]);
	}


	for (i = 0; i < 16; i++)
                set_break_vector(i, do_sigtrap);
        
	

	set_int_vector(15, multiple_interrupt);
	
	

	set_int_vector(0, hwbreakpoint);
	set_int_vector(1, IRQ1_interrupt);

	

	set_int_vector(14, mmu_bus_fault);

	

	set_break_vector(13, system_call);

        set_break_vector(8, gdb_handle_breakpoint);

#ifdef CONFIG_ETRAX_KGDB
	
	kgdb_init();
	breakpoint();
#endif
}
