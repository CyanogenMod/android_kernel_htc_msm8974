
/*
 * Copyright (C) 2006, Rusty Russell <rusty@rustcorp.com.au> IBM Corporation.
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
#include <linux/lguest.h>
#include <linux/lguest_launcher.h>
#include <linux/virtio_console.h>
#include <linux/pm.h>
#include <linux/export.h>
#include <asm/apic.h>
#include <asm/lguest.h>
#include <asm/paravirt.h>
#include <asm/param.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/desc.h>
#include <asm/setup.h>
#include <asm/e820.h>
#include <asm/mce.h>
#include <asm/io.h>
#include <asm/i387.h>
#include <asm/stackprotector.h>
#include <asm/reboot.h>		
#include <asm/kvm_para.h>


struct lguest_data lguest_data = {
	.hcall_status = { [0 ... LHCALL_RING_SIZE-1] = 0xFF },
	.noirq_start = (u32)lguest_noirq_start,
	.noirq_end = (u32)lguest_noirq_end,
	.kernel_address = PAGE_OFFSET,
	.blocked_interrupts = { 1 }, 
	.syscall_vec = SYSCALL_VECTOR,
};

static void async_hcall(unsigned long call, unsigned long arg1,
			unsigned long arg2, unsigned long arg3,
			unsigned long arg4)
{
	
	static unsigned int next_call;
	unsigned long flags;

	local_irq_save(flags);
	if (lguest_data.hcall_status[next_call] != 0xFF) {
		
		hcall(call, arg1, arg2, arg3, arg4);
	} else {
		lguest_data.hcalls[next_call].arg0 = call;
		lguest_data.hcalls[next_call].arg1 = arg1;
		lguest_data.hcalls[next_call].arg2 = arg2;
		lguest_data.hcalls[next_call].arg3 = arg3;
		lguest_data.hcalls[next_call].arg4 = arg4;
		/* Arguments must all be written before we mark it to go */
		wmb();
		lguest_data.hcall_status[next_call] = 0;
		if (++next_call == LHCALL_RING_SIZE)
			next_call = 0;
	}
	local_irq_restore(flags);
}

static void lazy_hcall1(unsigned long call, unsigned long arg1)
{
	if (paravirt_get_lazy_mode() == PARAVIRT_LAZY_NONE)
		hcall(call, arg1, 0, 0, 0);
	else
		async_hcall(call, arg1, 0, 0, 0);
}

static void lazy_hcall2(unsigned long call,
			unsigned long arg1,
			unsigned long arg2)
{
	if (paravirt_get_lazy_mode() == PARAVIRT_LAZY_NONE)
		hcall(call, arg1, arg2, 0, 0);
	else
		async_hcall(call, arg1, arg2, 0, 0);
}

static void lazy_hcall3(unsigned long call,
			unsigned long arg1,
			unsigned long arg2,
			unsigned long arg3)
{
	if (paravirt_get_lazy_mode() == PARAVIRT_LAZY_NONE)
		hcall(call, arg1, arg2, arg3, 0);
	else
		async_hcall(call, arg1, arg2, arg3, 0);
}

#ifdef CONFIG_X86_PAE
static void lazy_hcall4(unsigned long call,
			unsigned long arg1,
			unsigned long arg2,
			unsigned long arg3,
			unsigned long arg4)
{
	if (paravirt_get_lazy_mode() == PARAVIRT_LAZY_NONE)
		hcall(call, arg1, arg2, arg3, arg4);
	else
		async_hcall(call, arg1, arg2, arg3, arg4);
}
#endif

static void lguest_leave_lazy_mmu_mode(void)
{
	hcall(LHCALL_FLUSH_ASYNC, 0, 0, 0, 0);
	paravirt_leave_lazy_mmu();
}

static void lguest_end_context_switch(struct task_struct *next)
{
	hcall(LHCALL_FLUSH_ASYNC, 0, 0, 0, 0);
	paravirt_end_context_switch(next);
}


static unsigned long save_fl(void)
{
	return lguest_data.irq_enabled;
}

static void irq_disable(void)
{
	lguest_data.irq_enabled = 0;
}

PV_CALLEE_SAVE_REGS_THUNK(save_fl);
PV_CALLEE_SAVE_REGS_THUNK(irq_disable);

extern void lg_irq_enable(void);
extern void lg_restore_fl(unsigned long flags);


static void lguest_write_idt_entry(gate_desc *dt,
				   int entrynum, const gate_desc *g)
{
	u32 *desc = (u32 *)g;
	
	native_write_idt_entry(dt, entrynum, g);
	
	hcall(LHCALL_LOAD_IDT_ENTRY, entrynum, desc[0], desc[1], 0);
}

/*
 * Changing to a different IDT is very rare: we keep the IDT up-to-date every
 * time it is written, so we can simply loop through all entries and tell the
 * Host about them.
 */
static void lguest_load_idt(const struct desc_ptr *desc)
{
	unsigned int i;
	struct desc_struct *idt = (void *)desc->address;

	for (i = 0; i < (desc->size+1)/8; i++)
		hcall(LHCALL_LOAD_IDT_ENTRY, i, idt[i].a, idt[i].b, 0);
}

static void lguest_load_gdt(const struct desc_ptr *desc)
{
	unsigned int i;
	struct desc_struct *gdt = (void *)desc->address;

	for (i = 0; i < (desc->size+1)/8; i++)
		hcall(LHCALL_LOAD_GDT_ENTRY, i, gdt[i].a, gdt[i].b, 0);
}

static void lguest_write_gdt_entry(struct desc_struct *dt, int entrynum,
				   const void *desc, int type)
{
	native_write_gdt_entry(dt, entrynum, desc, type);
	
	hcall(LHCALL_LOAD_GDT_ENTRY, entrynum,
	      dt[entrynum].a, dt[entrynum].b, 0);
}

static void lguest_load_tls(struct thread_struct *t, unsigned int cpu)
{
	lazy_load_gs(0);
	lazy_hcall2(LHCALL_LOAD_TLS, __pa(&t->tls_array), cpu);
}

static void lguest_set_ldt(const void *addr, unsigned entries)
{
}

static void lguest_load_tr_desc(void)
{
}

static void lguest_cpuid(unsigned int *ax, unsigned int *bx,
			 unsigned int *cx, unsigned int *dx)
{
	int function = *ax;

	native_cpuid(ax, bx, cx, dx);
	switch (function) {
	case 0:
		if (*ax > 5)
			*ax = 5;
		break;

	case 1:
		*cx &= 0x00002201;
		*dx &= 0x07808151;
		*dx |= 0x00002000;
		*ax &= 0xFFFFF0FF;
		*ax |= 0x00000500;
		break;

	case KVM_CPUID_SIGNATURE:
		*bx = *cx = *dx = 0;
		break;

	case 0x80000000:
		if (*ax > 0x80000008)
			*ax = 0x80000008;
		break;

	case 0x80000001:
		*dx &= ~(1 << 20);
		break;
	}
}

static unsigned long current_cr0;
static void lguest_write_cr0(unsigned long val)
{
	lazy_hcall1(LHCALL_TS, val & X86_CR0_TS);
	current_cr0 = val;
}

static unsigned long lguest_read_cr0(void)
{
	return current_cr0;
}

static void lguest_clts(void)
{
	lazy_hcall1(LHCALL_TS, 0);
	current_cr0 &= ~X86_CR0_TS;
}

static unsigned long lguest_read_cr2(void)
{
	return lguest_data.cr2;
}

static bool cr3_changed = false;
static unsigned long current_cr3;

static void lguest_write_cr3(unsigned long cr3)
{
	lazy_hcall1(LHCALL_NEW_PGTABLE, cr3);
	current_cr3 = cr3;

	
	if (cr3 != __pa(swapper_pg_dir) && cr3 != __pa(initial_page_table))
		cr3_changed = true;
}

static unsigned long lguest_read_cr3(void)
{
	return current_cr3;
}

static unsigned long lguest_read_cr4(void)
{
	return 0;
}

static void lguest_write_cr4(unsigned long val)
{
}


static void lguest_pte_update(struct mm_struct *mm, unsigned long addr,
			       pte_t *ptep)
{
#ifdef CONFIG_X86_PAE
	
	lazy_hcall4(LHCALL_SET_PTE, __pa(mm->pgd), addr,
		    ptep->pte_low, ptep->pte_high);
#else
	lazy_hcall3(LHCALL_SET_PTE, __pa(mm->pgd), addr, ptep->pte_low);
#endif
}

static void lguest_set_pte_at(struct mm_struct *mm, unsigned long addr,
			      pte_t *ptep, pte_t pteval)
{
	native_set_pte(ptep, pteval);
	lguest_pte_update(mm, addr, ptep);
}

#ifdef CONFIG_X86_PAE
static void lguest_set_pud(pud_t *pudp, pud_t pudval)
{
	native_set_pud(pudp, pudval);

	
	lazy_hcall2(LHCALL_SET_PGD, __pa(pudp) & 0xFFFFFFE0,
		   (__pa(pudp) & 0x1F) / sizeof(pud_t));
}

static void lguest_set_pmd(pmd_t *pmdp, pmd_t pmdval)
{
	native_set_pmd(pmdp, pmdval);
	lazy_hcall2(LHCALL_SET_PMD, __pa(pmdp) & PAGE_MASK,
		   (__pa(pmdp) & (PAGE_SIZE - 1)) / sizeof(pmd_t));
}
#else

static void lguest_set_pmd(pmd_t *pmdp, pmd_t pmdval)
{
	native_set_pmd(pmdp, pmdval);
	lazy_hcall2(LHCALL_SET_PGD, __pa(pmdp) & PAGE_MASK,
		   (__pa(pmdp) & (PAGE_SIZE - 1)) / sizeof(pmd_t));
}
#endif

static void lguest_set_pte(pte_t *ptep, pte_t pteval)
{
	native_set_pte(ptep, pteval);
	if (cr3_changed)
		lazy_hcall1(LHCALL_FLUSH_TLB, 1);
}

#ifdef CONFIG_X86_PAE
static void lguest_set_pte_atomic(pte_t *ptep, pte_t pte)
{
	native_set_pte_atomic(ptep, pte);
	if (cr3_changed)
		lazy_hcall1(LHCALL_FLUSH_TLB, 1);
}

static void lguest_pte_clear(struct mm_struct *mm, unsigned long addr,
			     pte_t *ptep)
{
	native_pte_clear(mm, addr, ptep);
	lguest_pte_update(mm, addr, ptep);
}

static void lguest_pmd_clear(pmd_t *pmdp)
{
	lguest_set_pmd(pmdp, __pmd(0));
}
#endif

/*
 * Unfortunately for Lguest, the pv_mmu_ops for page tables were based on
 * native page table operations.  On native hardware you can set a new page
 * table entry whenever you want, but if you want to remove one you have to do
 * a TLB flush (a TLB is a little cache of page table entries kept by the CPU).
 *
 * So the lguest_set_pte_at() and lguest_set_pmd() functions above are only
 * called when a valid entry is written, not when it's removed (ie. marked not
 * present).  Instead, this is where we come when the Guest wants to remove a
 * page table entry: we tell the Host to set that entry to 0 (ie. the present
 * bit is zero).
 */
static void lguest_flush_tlb_single(unsigned long addr)
{
	
	lazy_hcall3(LHCALL_SET_PTE, current_cr3, addr, 0);
}

static void lguest_flush_tlb_user(void)
{
	lazy_hcall1(LHCALL_FLUSH_TLB, 0);
}

static void lguest_flush_tlb_kernel(void)
{
	lazy_hcall1(LHCALL_FLUSH_TLB, 1);
}

static void disable_lguest_irq(struct irq_data *data)
{
	set_bit(data->irq, lguest_data.blocked_interrupts);
}

static void enable_lguest_irq(struct irq_data *data)
{
	clear_bit(data->irq, lguest_data.blocked_interrupts);
}

static struct irq_chip lguest_irq_controller = {
	.name		= "lguest",
	.irq_mask	= disable_lguest_irq,
	.irq_mask_ack	= disable_lguest_irq,
	.irq_unmask	= enable_lguest_irq,
};

static void __init lguest_init_IRQ(void)
{
	unsigned int i;

	for (i = FIRST_EXTERNAL_VECTOR; i < NR_VECTORS; i++) {
		
		__this_cpu_write(vector_irq[i], i - FIRST_EXTERNAL_VECTOR);
		if (i != SYSCALL_VECTOR)
			set_intr_gate(i, interrupt[i - FIRST_EXTERNAL_VECTOR]);
	}

	irq_ctx_init(smp_processor_id());
}

int lguest_setup_irq(unsigned int irq)
{
	int err;

	
	err = irq_alloc_desc_at(irq, 0);
	if (err < 0 && err != -EEXIST)
		return err;

	irq_set_chip_and_handler_name(irq, &lguest_irq_controller,
				      handle_level_irq, "level");
	return 0;
}

static unsigned long lguest_get_wallclock(void)
{
	return lguest_data.time.tv_sec;
}

static unsigned long lguest_tsc_khz(void)
{
	return lguest_data.tsc_khz;
}

static cycle_t lguest_clock_read(struct clocksource *cs)
{
	unsigned long sec, nsec;

	do {
		
		sec = lguest_data.time.tv_sec;
		rmb();
		
		nsec = lguest_data.time.tv_nsec;
		
		rmb();
		
	} while (unlikely(lguest_data.time.tv_sec != sec));

	
	return sec*1000000000ULL + nsec;
}

static struct clocksource lguest_clock = {
	.name		= "lguest",
	.rating		= 200,
	.read		= lguest_clock_read,
	.mask		= CLOCKSOURCE_MASK(64),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static int lguest_clockevent_set_next_event(unsigned long delta,
                                           struct clock_event_device *evt)
{
	if (delta < LG_CLOCK_MIN_DELTA) {
		if (printk_ratelimit())
			printk(KERN_DEBUG "%s: small delta %lu ns\n",
			       __func__, delta);
		return -ETIME;
	}

	
	hcall(LHCALL_SET_CLOCKEVENT, delta, 0, 0, 0);
	return 0;
}

static void lguest_clockevent_set_mode(enum clock_event_mode mode,
                                      struct clock_event_device *evt)
{
	switch (mode) {
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		
		hcall(LHCALL_SET_CLOCKEVENT, 0, 0, 0, 0);
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		
		break;
	case CLOCK_EVT_MODE_PERIODIC:
		BUG();
	case CLOCK_EVT_MODE_RESUME:
		break;
	}
}

static struct clock_event_device lguest_clockevent = {
	.name                   = "lguest",
	.features               = CLOCK_EVT_FEAT_ONESHOT,
	.set_next_event         = lguest_clockevent_set_next_event,
	.set_mode               = lguest_clockevent_set_mode,
	.rating                 = INT_MAX,
	.mult                   = 1,
	.shift                  = 0,
	.min_delta_ns           = LG_CLOCK_MIN_DELTA,
	.max_delta_ns           = LG_CLOCK_MAX_DELTA,
};

static void lguest_time_irq(unsigned int irq, struct irq_desc *desc)
{
	unsigned long flags;

	
	local_irq_save(flags);
	lguest_clockevent.event_handler(&lguest_clockevent);
	local_irq_restore(flags);
}

static void lguest_time_init(void)
{
	
	lguest_setup_irq(0);
	irq_set_handler(0, lguest_time_irq);

	clocksource_register_hz(&lguest_clock, NSEC_PER_SEC);

	lguest_clockevent.cpumask = cpumask_of(0);
	clockevents_register_device(&lguest_clockevent);

	
	clear_bit(0, lguest_data.blocked_interrupts);
}


static void lguest_load_sp0(struct tss_struct *tss,
			    struct thread_struct *thread)
{
	lazy_hcall3(LHCALL_SET_STACK, __KERNEL_DS | 0x1, thread->sp0,
		   THREAD_SIZE / PAGE_SIZE);
}

static void lguest_set_debugreg(int regno, unsigned long value)
{
	
}

static void lguest_wbinvd(void)
{
}

#ifdef CONFIG_X86_LOCAL_APIC
static void lguest_apic_write(u32 reg, u32 v)
{
}

static u32 lguest_apic_read(u32 reg)
{
	return 0;
}

static u64 lguest_apic_icr_read(void)
{
	return 0;
}

static void lguest_apic_icr_write(u32 low, u32 id)
{
	
	WARN_ON(1);
}

static void lguest_apic_wait_icr_idle(void)
{
	return;
}

static u32 lguest_apic_safe_wait_icr_idle(void)
{
	return 0;
}

static void set_lguest_basic_apic_ops(void)
{
	apic->read = lguest_apic_read;
	apic->write = lguest_apic_write;
	apic->icr_read = lguest_apic_icr_read;
	apic->icr_write = lguest_apic_icr_write;
	apic->wait_icr_idle = lguest_apic_wait_icr_idle;
	apic->safe_wait_icr_idle = lguest_apic_safe_wait_icr_idle;
};
#endif

static void lguest_safe_halt(void)
{
	hcall(LHCALL_HALT, 0, 0, 0, 0);
}

static void lguest_power_off(void)
{
	hcall(LHCALL_SHUTDOWN, __pa("Power down"),
	      LGUEST_SHUTDOWN_POWEROFF, 0, 0);
}

static int lguest_panic(struct notifier_block *nb, unsigned long l, void *p)
{
	hcall(LHCALL_SHUTDOWN, __pa(p), LGUEST_SHUTDOWN_POWEROFF, 0, 0);
	
	return NOTIFY_DONE;
}

static struct notifier_block paniced = {
	.notifier_call = lguest_panic
};

static __init char *lguest_memory_setup(void)
{
	e820_add_region(boot_params.e820_map[0].addr,
			  boot_params.e820_map[0].size,
			  boot_params.e820_map[0].type);

	
	return "LGUEST";
}

static __init int early_put_chars(u32 vtermno, const char *buf, int count)
{
	char scratch[17];
	unsigned int len = count;

	
	if (len > sizeof(scratch) - 1)
		len = sizeof(scratch) - 1;
	scratch[len] = '\0';
	memcpy(scratch, buf, len);
	hcall(LHCALL_NOTIFY, __pa(scratch), 0, 0, 0);

	/* This routine returns the number of bytes actually written. */
	return len;
}

static void lguest_restart(char *reason)
{
	hcall(LHCALL_SHUTDOWN, __pa(reason), LGUEST_SHUTDOWN_RESTART, 0, 0);
}


static const struct lguest_insns
{
	const char *start, *end;
} lguest_insns[] = {
	[PARAVIRT_PATCH(pv_irq_ops.irq_disable)] = { lgstart_cli, lgend_cli },
	[PARAVIRT_PATCH(pv_irq_ops.save_fl)] = { lgstart_pushf, lgend_pushf },
};

static unsigned lguest_patch(u8 type, u16 clobber, void *ibuf,
			     unsigned long addr, unsigned len)
{
	unsigned int insn_len;

	
	if (type >= ARRAY_SIZE(lguest_insns) || !lguest_insns[type].start)
		return paravirt_patch_default(type, clobber, ibuf, addr, len);

	insn_len = lguest_insns[type].end - lguest_insns[type].start;

	
	if (len < insn_len)
		return paravirt_patch_default(type, clobber, ibuf, addr, len);

	
	memcpy(ibuf, lguest_insns[type].start, insn_len);
	return insn_len;
}

__init void lguest_init(void)
{
	
	pv_info.name = "lguest";
	
	pv_info.paravirt_enabled = 1;
	
	pv_info.kernel_rpl = 1;
	
	pv_info.shared_kernel_pmd = 1;


	
	pv_irq_ops.save_fl = PV_CALLEE_SAVE(save_fl);
	pv_irq_ops.restore_fl = __PV_IS_CALLEE_SAVE(lg_restore_fl);
	pv_irq_ops.irq_disable = PV_CALLEE_SAVE(irq_disable);
	pv_irq_ops.irq_enable = __PV_IS_CALLEE_SAVE(lg_irq_enable);
	pv_irq_ops.safe_halt = lguest_safe_halt;

	
	pv_init_ops.patch = lguest_patch;

	
	pv_cpu_ops.load_gdt = lguest_load_gdt;
	pv_cpu_ops.cpuid = lguest_cpuid;
	pv_cpu_ops.load_idt = lguest_load_idt;
	pv_cpu_ops.iret = lguest_iret;
	pv_cpu_ops.load_sp0 = lguest_load_sp0;
	pv_cpu_ops.load_tr_desc = lguest_load_tr_desc;
	pv_cpu_ops.set_ldt = lguest_set_ldt;
	pv_cpu_ops.load_tls = lguest_load_tls;
	pv_cpu_ops.set_debugreg = lguest_set_debugreg;
	pv_cpu_ops.clts = lguest_clts;
	pv_cpu_ops.read_cr0 = lguest_read_cr0;
	pv_cpu_ops.write_cr0 = lguest_write_cr0;
	pv_cpu_ops.read_cr4 = lguest_read_cr4;
	pv_cpu_ops.write_cr4 = lguest_write_cr4;
	pv_cpu_ops.write_gdt_entry = lguest_write_gdt_entry;
	pv_cpu_ops.write_idt_entry = lguest_write_idt_entry;
	pv_cpu_ops.wbinvd = lguest_wbinvd;
	pv_cpu_ops.start_context_switch = paravirt_start_context_switch;
	pv_cpu_ops.end_context_switch = lguest_end_context_switch;

	
	pv_mmu_ops.write_cr3 = lguest_write_cr3;
	pv_mmu_ops.flush_tlb_user = lguest_flush_tlb_user;
	pv_mmu_ops.flush_tlb_single = lguest_flush_tlb_single;
	pv_mmu_ops.flush_tlb_kernel = lguest_flush_tlb_kernel;
	pv_mmu_ops.set_pte = lguest_set_pte;
	pv_mmu_ops.set_pte_at = lguest_set_pte_at;
	pv_mmu_ops.set_pmd = lguest_set_pmd;
#ifdef CONFIG_X86_PAE
	pv_mmu_ops.set_pte_atomic = lguest_set_pte_atomic;
	pv_mmu_ops.pte_clear = lguest_pte_clear;
	pv_mmu_ops.pmd_clear = lguest_pmd_clear;
	pv_mmu_ops.set_pud = lguest_set_pud;
#endif
	pv_mmu_ops.read_cr2 = lguest_read_cr2;
	pv_mmu_ops.read_cr3 = lguest_read_cr3;
	pv_mmu_ops.lazy_mode.enter = paravirt_enter_lazy_mmu;
	pv_mmu_ops.lazy_mode.leave = lguest_leave_lazy_mmu_mode;
	pv_mmu_ops.pte_update = lguest_pte_update;
	pv_mmu_ops.pte_update_defer = lguest_pte_update;

#ifdef CONFIG_X86_LOCAL_APIC
	
	set_lguest_basic_apic_ops();
#endif

	x86_init.resources.memory_setup = lguest_memory_setup;
	x86_init.irqs.intr_init = lguest_init_IRQ;
	x86_init.timers.timer_init = lguest_time_init;
	x86_platform.calibrate_tsc = lguest_tsc_khz;
	x86_platform.get_wallclock =  lguest_get_wallclock;



	setup_stack_canary_segment(0);

	switch_to_new_gdt(0);

	reserve_top_address(lguest_data.reserve_mem);

	lockdep_init();

	
	atomic_notifier_chain_register(&panic_notifier_list, &paniced);

	paravirt_disable_iospace();

	cpu_detect(&new_cpu_data);
	
	new_cpu_data.x86_capability[0] = cpuid_edx(1);

	
	new_cpu_data.hard_math = 1;

	
#ifdef CONFIG_X86_MCE
	mce_disabled = 1;
#endif
#ifdef CONFIG_ACPI
	acpi_disabled = 1;
#endif

	/*
	 * We set the preferred console to "hvc".  This is the "hypervisor
	 * virtual console" driver written by the PowerPC people, which we also
	 * adapted for lguest's use.
	 */
	add_preferred_console("hvc", 0, NULL);

	
	virtio_cons_early_init(early_put_chars);

	pm_power_off = lguest_power_off;
	machine_ops.restart = lguest_restart;

	i386_start_kernel();
}
