#include "lg.h"


static bool ignored_gdt(unsigned int num)
{
	return (num == GDT_ENTRY_TSS
		|| num == GDT_ENTRY_LGUEST_CS
		|| num == GDT_ENTRY_LGUEST_DS
		|| num == GDT_ENTRY_DOUBLEFAULT_TSS);
}

static void fixup_gdt_table(struct lg_cpu *cpu, unsigned start, unsigned end)
{
	unsigned int i;

	for (i = start; i < end; i++) {
		if (ignored_gdt(i))
			continue;

		if (cpu->arch.gdt[i].dpl == 0)
			cpu->arch.gdt[i].dpl |= GUEST_PL;

		cpu->arch.gdt[i].type |= 0x1;
	}
}

void setup_default_gdt_entries(struct lguest_ro_state *state)
{
	struct desc_struct *gdt = state->guest_gdt;
	unsigned long tss = (unsigned long)&state->guest_tss;

	
	gdt[GDT_ENTRY_LGUEST_CS] = FULL_EXEC_SEGMENT;
	gdt[GDT_ENTRY_LGUEST_DS] = FULL_SEGMENT;

	gdt[GDT_ENTRY_TSS].a = 0;
	gdt[GDT_ENTRY_TSS].b = 0;

	gdt[GDT_ENTRY_TSS].limit0 = 0x67;
	gdt[GDT_ENTRY_TSS].base0  = tss & 0xFFFF;
	gdt[GDT_ENTRY_TSS].base1  = (tss >> 16) & 0xFF;
	gdt[GDT_ENTRY_TSS].base2  = tss >> 24;
	gdt[GDT_ENTRY_TSS].type   = 0x9; 
	gdt[GDT_ENTRY_TSS].p      = 0x1; 
	gdt[GDT_ENTRY_TSS].dpl    = 0x0; 
	gdt[GDT_ENTRY_TSS].s      = 0x0; 

}

void setup_guest_gdt(struct lg_cpu *cpu)
{
	cpu->arch.gdt[GDT_ENTRY_KERNEL_CS] = FULL_EXEC_SEGMENT;
	cpu->arch.gdt[GDT_ENTRY_KERNEL_DS] = FULL_SEGMENT;
	cpu->arch.gdt[GDT_ENTRY_KERNEL_CS].dpl |= GUEST_PL;
	cpu->arch.gdt[GDT_ENTRY_KERNEL_DS].dpl |= GUEST_PL;
}

void copy_gdt_tls(const struct lg_cpu *cpu, struct desc_struct *gdt)
{
	unsigned int i;

	for (i = GDT_ENTRY_TLS_MIN; i <= GDT_ENTRY_TLS_MAX; i++)
		gdt[i] = cpu->arch.gdt[i];
}

void copy_gdt(const struct lg_cpu *cpu, struct desc_struct *gdt)
{
	unsigned int i;

	for (i = 0; i < GDT_ENTRIES; i++)
		if (!ignored_gdt(i))
			gdt[i] = cpu->arch.gdt[i];
}

void load_guest_gdt_entry(struct lg_cpu *cpu, u32 num, u32 lo, u32 hi)
{
	if (num >= ARRAY_SIZE(cpu->arch.gdt)) {
		kill_guest(cpu, "too many gdt entries %i", num);
		return;
	}

	
	cpu->arch.gdt[num].a = lo;
	cpu->arch.gdt[num].b = hi;
	fixup_gdt_table(cpu, num, num+1);
	cpu->changed |= CHANGED_GDT;
}

void guest_load_tls(struct lg_cpu *cpu, unsigned long gtls)
{
	struct desc_struct *tls = &cpu->arch.gdt[GDT_ENTRY_TLS_MIN];

	__lgread(cpu, tls, gtls, sizeof(*tls)*GDT_ENTRY_TLS_ENTRIES);
	fixup_gdt_table(cpu, GDT_ENTRY_TLS_MIN, GDT_ENTRY_TLS_MAX+1);
	
	cpu->changed |= CHANGED_GDT_TLS;
}

