/*
 * Copyright (C) 2006, Rusty Russell <rusty@rustcorp.com.au> IBM Corporation.
 * Copyright (C) 2007, Jes Sorensen <jes@sgi.com> SGI.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/kernel.h>
#include <linux/start_kernel.h>
#include <linux/string.h>
#include <linux/console.h>
#include <linux/screen_info.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/cpu.h>
#include <linux/lguest.h>
#include <linux/lguest_launcher.h>
#include <asm/paravirt.h>
#include <asm/param.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/desc.h>
#include <asm/setup.h>
#include <asm/lguest.h>
#include <asm/uaccess.h>
#include <asm/i387.h>
#include "../lg.h"

static int cpu_had_pge;

static struct {
	unsigned long offset;
	unsigned short segment;
} lguest_entry;

static unsigned long switcher_offset(void)
{
	return SWITCHER_ADDR - (unsigned long)start_switcher_text;
}

static struct lguest_pages *lguest_pages(unsigned int cpu)
{
	return &(((struct lguest_pages *)
		  (SWITCHER_ADDR + SHARED_SWITCHER_PAGES*PAGE_SIZE))[cpu]);
}

static DEFINE_PER_CPU(struct lg_cpu *, lg_last_cpu);

static void copy_in_guest_info(struct lg_cpu *cpu, struct lguest_pages *pages)
{
	if (__this_cpu_read(lg_last_cpu) != cpu || cpu->last_pages != pages) {
		__this_cpu_write(lg_last_cpu, cpu);
		cpu->last_pages = pages;
		cpu->changed = CHANGED_ALL;
	}

	pages->state.host_cr3 = __pa(current->mm->pgd);
	map_switcher_in_guest(cpu, pages);
	pages->state.guest_tss.sp1 = cpu->esp1;
	pages->state.guest_tss.ss1 = cpu->ss1;

	
	if (cpu->changed & CHANGED_IDT)
		copy_traps(cpu, pages->state.guest_idt, default_idt_entries);

	
	if (cpu->changed & CHANGED_GDT)
		copy_gdt(cpu, pages->state.guest_gdt);
	
	else if (cpu->changed & CHANGED_GDT_TLS)
		copy_gdt_tls(cpu, pages->state.guest_gdt);

	
	cpu->changed = 0;
}

static void run_guest_once(struct lg_cpu *cpu, struct lguest_pages *pages)
{
	
	unsigned int clobber;

	copy_in_guest_info(cpu, pages);

	cpu->regs->trapnum = 256;

	asm volatile("pushf; lcall *lguest_entry"
		     : "=a"(clobber), "=b"(clobber)
		     : "0"(pages), "1"(__pa(cpu->lg->pgdirs[cpu->cpu_pgd].pgdir))
		     : "memory", "%edx", "%ecx", "%edi", "%esi");
}


void lguest_arch_run_guest(struct lg_cpu *cpu)
{
	if (cpu->ts)
		unlazy_fpu(current);

	if (boot_cpu_has(X86_FEATURE_SEP))
		wrmsr(MSR_IA32_SYSENTER_CS, 0, 0);

	run_guest_once(cpu, lguest_pages(raw_smp_processor_id()));


	 
	 if (boot_cpu_has(X86_FEATURE_SEP))
		wrmsr(MSR_IA32_SYSENTER_CS, __KERNEL_CS, 0);

	if (cpu->regs->trapnum == 14)
		cpu->arch.last_pagefault = read_cr2();
	else if (cpu->regs->trapnum == 7)
		math_state_restore();
}

static int emulate_insn(struct lg_cpu *cpu)
{
	u8 insn;
	unsigned int insnlen = 0, in = 0, small_operand = 0;
	unsigned long physaddr = guest_pa(cpu, cpu->regs->eip);

	if ((cpu->regs->cs & 3) != GUEST_PL)
		return 0;

	
	insn = lgread(cpu, physaddr, u8);

	if (insn == 0xfa) {
		
		cpu->regs->eip++;
		return 1;
	}

	if (insn == 0x66) {
		small_operand = 1;
		
		insnlen = 1;
		insn = lgread(cpu, physaddr + insnlen, u8);
	}

	switch (insn & 0xFE) {
	case 0xE4: 
		insnlen += 2;
		in = 1;
		break;
	case 0xEC: 
		insnlen += 1;
		in = 1;
		break;
	case 0xE6: 
		insnlen += 2;
		break;
	case 0xEE: 
		insnlen += 1;
		break;
	default:
		
		return 0;
	}

	if (in) {
		
		if (insn & 0x1) {
			if (small_operand)
				cpu->regs->eax |= 0xFFFF;
			else
				cpu->regs->eax = 0xFFFFFFFF;
		} else
			cpu->regs->eax |= 0xFF;
	}
	
	cpu->regs->eip += insnlen;
	
	return 1;
}

void lguest_arch_handle_trap(struct lg_cpu *cpu)
{
	switch (cpu->regs->trapnum) {
	case 13: 
		if (cpu->regs->errcode == 0) {
			if (emulate_insn(cpu))
				return;
		}
		break;
	case 14: 
		if (demand_page(cpu, cpu->arch.last_pagefault,
				cpu->regs->errcode))
			return;

		if (cpu->lg->lguest_data &&
		    put_user(cpu->arch.last_pagefault,
			     &cpu->lg->lguest_data->cr2))
			kill_guest(cpu, "Writing cr2");
		break;
	case 7: 
		if (!cpu->ts)
			return;
		break;
	case 32 ... 255:
		cond_resched();
		return;
	case LGUEST_TRAP_ENTRY:
		cpu->hcall = (struct hcall_args *)cpu->regs;
		return;
	}

	
	if (!deliver_trap(cpu, cpu->regs->trapnum))
		kill_guest(cpu, "unhandled trap %li at %#lx (%#lx)",
			   cpu->regs->trapnum, cpu->regs->eip,
			   cpu->regs->trapnum == 14 ? cpu->arch.last_pagefault
			   : cpu->regs->errcode);
}

static void adjust_pge(void *on)
{
	if (on)
		write_cr4(read_cr4() | X86_CR4_PGE);
	else
		write_cr4(read_cr4() & ~X86_CR4_PGE);
}

void __init lguest_arch_host_init(void)
{
	int i;

	for (i = 0; i < IDT_ENTRIES; i++)
		default_idt_entries[i] += switcher_offset();

	for_each_possible_cpu(i) {
		
		struct lguest_pages *pages = lguest_pages(i);
		
		struct lguest_ro_state *state = &pages->state;

		state->host_gdt_desc.size = GDT_SIZE-1;
		state->host_gdt_desc.address = (long)get_cpu_gdt_table(i);

		store_idt(&state->host_idt_desc);

		state->guest_idt_desc.size = sizeof(state->guest_idt)-1;
		state->guest_idt_desc.address = (long)&state->guest_idt;
		state->guest_gdt_desc.size = sizeof(state->guest_gdt)-1;
		state->guest_gdt_desc.address = (long)&state->guest_gdt;

		state->guest_tss.sp0 = (long)(&pages->regs + 1);
		state->guest_tss.ss0 = LGUEST_DS;

		state->guest_tss.io_bitmap_base = sizeof(state->guest_tss);

		setup_default_gdt_entries(state);
		
		setup_default_idt_entries(state, default_idt_entries);

		get_cpu_gdt_table(i)[GDT_ENTRY_LGUEST_CS] = FULL_EXEC_SEGMENT;
		get_cpu_gdt_table(i)[GDT_ENTRY_LGUEST_DS] = FULL_SEGMENT;
	}

	lguest_entry.offset = (long)switch_to_guest + switcher_offset();
	lguest_entry.segment = LGUEST_CS;


	get_online_cpus();
	if (cpu_has_pge) { 
		
		cpu_had_pge = 1;
		on_each_cpu(adjust_pge, (void *)0, 1);
		
		clear_cpu_cap(&boot_cpu_data, X86_FEATURE_PGE);
	}
	put_online_cpus();
}

void __exit lguest_arch_host_fini(void)
{
	
	get_online_cpus();
	if (cpu_had_pge) {
		set_cpu_cap(&boot_cpu_data, X86_FEATURE_PGE);
		
		on_each_cpu(adjust_pge, (void *)1, 1);
	}
	put_online_cpus();
}


int lguest_arch_do_hcall(struct lg_cpu *cpu, struct hcall_args *args)
{
	switch (args->arg0) {
	case LHCALL_LOAD_GDT_ENTRY:
		load_guest_gdt_entry(cpu, args->arg1, args->arg2, args->arg3);
		break;
	case LHCALL_LOAD_IDT_ENTRY:
		load_guest_idt_entry(cpu, args->arg1, args->arg2, args->arg3);
		break;
	case LHCALL_LOAD_TLS:
		guest_load_tls(cpu, args->arg1);
		break;
	default:
		
		return -EIO;
	}
	return 0;
}

int lguest_arch_init_hypercalls(struct lg_cpu *cpu)
{
	u32 tsc_speed;

	if (!lguest_address_ok(cpu->lg, cpu->hcall->arg1,
			       sizeof(*cpu->lg->lguest_data)))
		return -EFAULT;

	cpu->lg->lguest_data = cpu->lg->mem_base + cpu->hcall->arg1;

	if (boot_cpu_has(X86_FEATURE_CONSTANT_TSC) && !check_tsc_unstable())
		tsc_speed = tsc_khz;
	else
		tsc_speed = 0;
	if (put_user(tsc_speed, &cpu->lg->lguest_data->tsc_khz))
		return -EFAULT;

	
	if (!check_syscall_vector(cpu->lg))
		kill_guest(cpu, "bad syscall vector");

	return 0;
}

void lguest_arch_setup_regs(struct lg_cpu *cpu, unsigned long start)
{
	struct lguest_regs *regs = cpu->regs;

	regs->ds = regs->es = regs->ss = __KERNEL_DS|GUEST_PL;
	regs->cs = __KERNEL_CS|GUEST_PL;

	regs->eflags = X86_EFLAGS_IF | X86_EFLAGS_BIT1;

	regs->eip = start;


	
	setup_guest_gdt(cpu);
}
