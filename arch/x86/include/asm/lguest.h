#ifndef _ASM_X86_LGUEST_H
#define _ASM_X86_LGUEST_H

#define GDT_ENTRY_LGUEST_CS	10
#define GDT_ENTRY_LGUEST_DS	11
#define LGUEST_CS		(GDT_ENTRY_LGUEST_CS * 8)
#define LGUEST_DS		(GDT_ENTRY_LGUEST_DS * 8)

#ifndef __ASSEMBLY__
#include <asm/desc.h>

#define GUEST_PL 1

#define SHARED_SWITCHER_PAGES \
	DIV_ROUND_UP(end_switcher_text - start_switcher_text, PAGE_SIZE)
#define TOTAL_SWITCHER_PAGES (SHARED_SWITCHER_PAGES + 2 * nr_cpu_ids)

#ifdef CONFIG_X86_PAE
#define SWITCHER_ADDR 0xFFE00000
#else
#define SWITCHER_ADDR 0xFFC00000
#endif

extern unsigned long default_idt_entries[];

extern char lguest_noirq_start[], lguest_noirq_end[];
extern const char lgstart_cli[], lgend_cli[];
extern const char lgstart_sti[], lgend_sti[];
extern const char lgstart_popf[], lgend_popf[];
extern const char lgstart_pushf[], lgend_pushf[];
extern const char lgstart_iret[], lgend_iret[];

extern void lguest_iret(void);
extern void lguest_init(void);

struct lguest_regs {
	
	unsigned long eax, ebx, ecx, edx;
	unsigned long esi, edi, ebp;
	unsigned long gs;
	unsigned long fs, ds, es;
	unsigned long trapnum, errcode;
	
	unsigned long eip;
	unsigned long cs;
	unsigned long eflags;
	unsigned long esp;
	unsigned long ss;
};

struct lguest_ro_state {
	
	u32 host_cr3;
	struct desc_ptr host_idt_desc;
	struct desc_ptr host_gdt_desc;
	u32 host_sp;

	
	struct desc_ptr guest_idt_desc;
	struct desc_ptr guest_gdt_desc;
	struct x86_hw_tss guest_tss;
	struct desc_struct guest_idt[IDT_ENTRIES];
	struct desc_struct guest_gdt[GDT_ENTRIES];
};

struct lg_cpu_arch {
	
	struct desc_struct gdt[GDT_ENTRIES];

	
	struct desc_struct idt[IDT_ENTRIES];

	
	unsigned long last_pagefault;
};

static inline void lguest_set_ts(void)
{
	u32 cr0;

	cr0 = read_cr0();
	if (!(cr0 & 8))
		write_cr0(cr0 | 8);
}

#define FULL_EXEC_SEGMENT \
	((struct desc_struct)GDT_ENTRY_INIT(0xc09b, 0, 0xfffff))
#define FULL_SEGMENT ((struct desc_struct)GDT_ENTRY_INIT(0xc093, 0, 0xfffff))

#endif 

#endif 
