/*
 *  linux/arch/cris/traps.c
 *
 *  Here we handle the break vectors not used by the system call
 *  mechanism, as well as some general stack/register dumping
 *  things.
 *
 *  Copyright (C) 2000-2007 Axis Communications AB
 *
 *  Authors:   Bjorn Wesen
 *             Hans-Peter Nilsson
 *
 */

#include <linux/init.h>
#include <linux/module.h>

#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <arch/system.h>

extern void arch_enable_nmi(void);
extern void stop_watchdog(void);
extern void reset_watchdog(void);
extern void show_registers(struct pt_regs *regs);

#ifdef CONFIG_DEBUG_BUGVERBOSE
extern void handle_BUG(struct pt_regs *regs);
#else
#define handle_BUG(regs)
#endif

static int kstack_depth_to_print = 24;

void (*nmi_handler)(struct pt_regs *);

void
show_trace(unsigned long *stack)
{
	unsigned long addr, module_start, module_end;
	extern char _stext, _etext;
	int i;

	printk("\nCall Trace: ");

	i = 1;
	module_start = VMALLOC_START;
	module_end = VMALLOC_END;

	while (((long)stack & (THREAD_SIZE-1)) != 0) {
		if (__get_user(addr, stack)) {
			printk("Failing address 0x%lx\n", (unsigned long)stack);
			break;
		}
		stack++;

		if (((addr >= (unsigned long)&_stext) &&
		     (addr <= (unsigned long)&_etext)) ||
		    ((addr >= module_start) && (addr <= module_end))) {
			if (i && ((i % 8) == 0))
				printk("\n       ");
			printk("[<%08lx>] ", addr);
			i++;
		}
	}
}


#define MODULE_RANGE (8*1024*1024)


void
show_stack(struct task_struct *task, unsigned long *sp)
{
	unsigned long *stack, addr;
	int i;


	if (sp == NULL) {
		if (task)
			sp = (unsigned long*)task->thread.ksp;
		else
			sp = (unsigned long*)rdsp();
	}

	stack = sp;

	printk("\nStack from %08lx:\n       ", (unsigned long)stack);
	for (i = 0; i < kstack_depth_to_print; i++) {
		if (((long)stack & (THREAD_SIZE-1)) == 0)
			break;
		if (i && ((i % 8) == 0))
			printk("\n       ");
		if (__get_user(addr, stack)) {
			printk("Failing address 0x%lx\n", (unsigned long)stack);
			break;
		}
		stack++;
		printk("%08lx ", addr);
	}
	show_trace(sp);
}

#if 0

int
show_stack(void)
{
	unsigned long *sp = (unsigned long *)rdusp();
	int i;

	printk("Stack dump [0x%08lx]:\n", (unsigned long)sp);
	for (i = 0; i < 16; i++)
		printk("sp + %d: 0x%08lx\n", i*4, sp[i]);
	return 0;
}
#endif

void
dump_stack(void)
{
	show_stack(NULL, NULL);
}
EXPORT_SYMBOL(dump_stack);

void
set_nmi_handler(void (*handler)(struct pt_regs *))
{
	nmi_handler = handler;
	arch_enable_nmi();
}

#ifdef CONFIG_DEBUG_NMI_OOPS
void
oops_nmi_handler(struct pt_regs *regs)
{
	stop_watchdog();
	oops_in_progress = 1;
	printk("NMI!\n");
	show_registers(regs);
	oops_in_progress = 0;
}

static int __init
oops_nmi_register(void)
{
	set_nmi_handler(oops_nmi_handler);
	return 0;
}

__initcall(oops_nmi_register);

#endif

void
watchdog_bite_hook(struct pt_regs *regs)
{
#ifdef CONFIG_ETRAX_WATCHDOG_NICE_DOGGY
	local_irq_disable();
	stop_watchdog();
	show_registers(regs);

	while (1)
		; 
#else
	show_registers(regs);
#endif
}

void
die_if_kernel(const char *str, struct pt_regs *regs, long err)
{
	if (user_mode(regs))
		return;

#ifdef CONFIG_ETRAX_WATCHDOG_NICE_DOGGY
	stop_watchdog();
#endif

	handle_BUG(regs);

	printk("%s: %04lx\n", str, err & 0xffff);

	show_registers(regs);

	oops_in_progress = 0;

#ifdef CONFIG_ETRAX_WATCHDOG_NICE_DOGGY
	reset_watchdog();
#endif
	do_exit(SIGSEGV);
}

void __init
trap_init(void)
{
	
}
