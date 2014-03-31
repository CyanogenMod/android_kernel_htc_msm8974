#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/sched.h>
#include "lg.h"

static unsigned int syscall_vector = SYSCALL_VECTOR;
module_param(syscall_vector, uint, 0444);

static unsigned long idt_address(u32 lo, u32 hi)
{
	return (lo & 0x0000FFFF) | (hi & 0xFFFF0000);
}

static int idt_type(u32 lo, u32 hi)
{
	return (hi >> 8) & 0xF;
}

static bool idt_present(u32 lo, u32 hi)
{
	return (hi & 0x8000);
}

static void push_guest_stack(struct lg_cpu *cpu, unsigned long *gstack, u32 val)
{
	
	*gstack -= 4;
	lgwrite(cpu, *gstack, u32, val);
}

static void set_guest_interrupt(struct lg_cpu *cpu, u32 lo, u32 hi,
				bool has_err)
{
	unsigned long gstack, origstack;
	u32 eflags, ss, irq_enable;
	unsigned long virtstack;

	if ((cpu->regs->ss&0x3) != GUEST_PL) {
		virtstack = cpu->esp1;
		ss = cpu->ss1;

		origstack = gstack = guest_pa(cpu, virtstack);
		push_guest_stack(cpu, &gstack, cpu->regs->ss);
		push_guest_stack(cpu, &gstack, cpu->regs->esp);
	} else {
		
		virtstack = cpu->regs->esp;
		ss = cpu->regs->ss;

		origstack = gstack = guest_pa(cpu, virtstack);
	}

	eflags = cpu->regs->eflags;
	if (get_user(irq_enable, &cpu->lg->lguest_data->irq_enabled) == 0
	    && !(irq_enable & X86_EFLAGS_IF))
		eflags &= ~X86_EFLAGS_IF;

	push_guest_stack(cpu, &gstack, eflags);
	push_guest_stack(cpu, &gstack, cpu->regs->cs);
	push_guest_stack(cpu, &gstack, cpu->regs->eip);

	
	if (has_err)
		push_guest_stack(cpu, &gstack, cpu->regs->errcode);

	cpu->regs->ss = ss;
	cpu->regs->esp = virtstack + (gstack - origstack);
	cpu->regs->cs = (__KERNEL_CS|GUEST_PL);
	cpu->regs->eip = idt_address(lo, hi);

	if (idt_type(lo, hi) == 0xE)
		if (put_user(0, &cpu->lg->lguest_data->irq_enabled))
			kill_guest(cpu, "Disabling interrupts");
}

unsigned int interrupt_pending(struct lg_cpu *cpu, bool *more)
{
	unsigned int irq;
	DECLARE_BITMAP(blk, LGUEST_IRQS);

	
	if (!cpu->lg->lguest_data)
		return LGUEST_IRQS;

	if (copy_from_user(&blk, cpu->lg->lguest_data->blocked_interrupts,
			   sizeof(blk)))
		return LGUEST_IRQS;
	bitmap_andnot(blk, cpu->irqs_pending, blk, LGUEST_IRQS);

	
	irq = find_first_bit(blk, LGUEST_IRQS);
	*more = find_next_bit(blk, LGUEST_IRQS, irq+1);

	return irq;
}

void try_deliver_interrupt(struct lg_cpu *cpu, unsigned int irq, bool more)
{
	struct desc_struct *idt;

	BUG_ON(irq >= LGUEST_IRQS);

	if (cpu->regs->eip >= cpu->lg->noirq_start &&
	   (cpu->regs->eip < cpu->lg->noirq_end))
		return;

	
	if (cpu->halted) {
		
		if (put_user(X86_EFLAGS_IF, &cpu->lg->lguest_data->irq_enabled))
			kill_guest(cpu, "Re-enabling interrupts");
		cpu->halted = 0;
	} else {
		
		u32 irq_enabled;
		if (get_user(irq_enabled, &cpu->lg->lguest_data->irq_enabled))
			irq_enabled = 0;
		if (!irq_enabled) {
			
			put_user(X86_EFLAGS_IF,
				 &cpu->lg->lguest_data->irq_pending);
			return;
		}
	}

	idt = &cpu->arch.idt[FIRST_EXTERNAL_VECTOR+irq];
	
	if (idt_present(idt->a, idt->b)) {
		
		clear_bit(irq, cpu->irqs_pending);
		set_guest_interrupt(cpu, idt->a, idt->b, false);
	}

	write_timestamp(cpu);

	if (!more)
		put_user(0, &cpu->lg->lguest_data->irq_pending);
}

void set_interrupt(struct lg_cpu *cpu, unsigned int irq)
{
	set_bit(irq, cpu->irqs_pending);

	if (!wake_up_process(cpu->tsk))
		kick_process(cpu->tsk);
}

static bool could_be_syscall(unsigned int num)
{
	
	return num == SYSCALL_VECTOR || num == syscall_vector;
}

bool check_syscall_vector(struct lguest *lg)
{
	u32 vector;

	if (get_user(vector, &lg->lguest_data->syscall_vec))
		return false;

	return could_be_syscall(vector);
}

int init_interrupts(void)
{
	
	if (syscall_vector != SYSCALL_VECTOR) {
		if (test_bit(syscall_vector, used_vectors) ||
		    vector_used_by_percpu_irq(syscall_vector)) {
			printk(KERN_ERR "lg: couldn't reserve syscall %u\n",
				 syscall_vector);
			return -EBUSY;
		}
		set_bit(syscall_vector, used_vectors);
	}

	return 0;
}

void free_interrupts(void)
{
	if (syscall_vector != SYSCALL_VECTOR)
		clear_bit(syscall_vector, used_vectors);
}

static bool has_err(unsigned int trap)
{
	return (trap == 8 || (trap >= 10 && trap <= 14) || trap == 17);
}

bool deliver_trap(struct lg_cpu *cpu, unsigned int num)
{
	if (num >= ARRAY_SIZE(cpu->arch.idt))
		return false;

	if (!idt_present(cpu->arch.idt[num].a, cpu->arch.idt[num].b))
		return false;
	set_guest_interrupt(cpu, cpu->arch.idt[num].a,
			    cpu->arch.idt[num].b, has_err(num));
	return true;
}

static bool direct_trap(unsigned int num)
{
	if (num >= FIRST_EXTERNAL_VECTOR && !could_be_syscall(num))
		return false;

	return num != 14 && num != 13 && num != 7 && num != LGUEST_TRAP_ENTRY;
}



void pin_stack_pages(struct lg_cpu *cpu)
{
	unsigned int i;

	for (i = 0; i < cpu->lg->stack_pages; i++)
		pin_page(cpu, cpu->esp1 - 1 - i * PAGE_SIZE);
}

void guest_set_stack(struct lg_cpu *cpu, u32 seg, u32 esp, unsigned int pages)
{
	if ((seg & 0x3) != GUEST_PL)
		kill_guest(cpu, "bad stack segment %i", seg);
	
	if (pages > 2)
		kill_guest(cpu, "bad stack pages %u", pages);
	
	cpu->ss1 = seg;
	cpu->esp1 = esp;
	cpu->lg->stack_pages = pages;
	
	pin_stack_pages(cpu);
}


static void set_trap(struct lg_cpu *cpu, struct desc_struct *trap,
		     unsigned int num, u32 lo, u32 hi)
{
	u8 type = idt_type(lo, hi);

	
	if (!idt_present(lo, hi)) {
		trap->a = trap->b = 0;
		return;
	}

	
	if (type != 0xE && type != 0xF)
		kill_guest(cpu, "bad IDT type %i", type);

	trap->a = ((__KERNEL_CS|GUEST_PL)<<16) | (lo&0x0000FFFF);
	trap->b = (hi&0xFFFFEF00);
}

void load_guest_idt_entry(struct lg_cpu *cpu, unsigned int num, u32 lo, u32 hi)
{
	if (num == 2 || num == 8 || num == 15 || num == LGUEST_TRAP_ENTRY)
		return;

	cpu->changed |= CHANGED_IDT;

	
	if (num >= ARRAY_SIZE(cpu->arch.idt))
		kill_guest(cpu, "Setting idt entry %u", num);
	else
		set_trap(cpu, &cpu->arch.idt[num], num, lo, hi);
}

static void default_idt_entry(struct desc_struct *idt,
			      int trap,
			      const unsigned long handler,
			      const struct desc_struct *base)
{
	
	u32 flags = 0x8e00;

	if (trap == LGUEST_TRAP_ENTRY)
		flags |= (GUEST_PL << 13);
	else if (base)
		flags |= (base->b & 0x6000);

	
	idt->a = (LGUEST_CS<<16) | (handler&0x0000FFFF);
	idt->b = (handler&0xFFFF0000) | flags;
}

void setup_default_idt_entries(struct lguest_ro_state *state,
			       const unsigned long *def)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(state->guest_idt); i++)
		default_idt_entry(&state->guest_idt[i], i, def[i], NULL);
}

void copy_traps(const struct lg_cpu *cpu, struct desc_struct *idt,
		const unsigned long *def)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(cpu->arch.idt); i++) {
		const struct desc_struct *gidt = &cpu->arch.idt[i];

		
		if (!direct_trap(i))
			continue;

		if (idt_type(gidt->a, gidt->b) == 0xF)
			idt[i] = *gidt;
		else
			default_idt_entry(&idt[i], i, def[i], gidt);
	}
}

void guest_set_clockevent(struct lg_cpu *cpu, unsigned long delta)
{
	ktime_t expires;

	if (unlikely(delta == 0)) {
		
		hrtimer_cancel(&cpu->hrt);
		return;
	}

	expires = ktime_add_ns(ktime_get_real(), delta);
	hrtimer_start(&cpu->hrt, expires, HRTIMER_MODE_ABS);
}

static enum hrtimer_restart clockdev_fn(struct hrtimer *timer)
{
	struct lg_cpu *cpu = container_of(timer, struct lg_cpu, hrt);

	
	set_interrupt(cpu, 0);
	return HRTIMER_NORESTART;
}

void init_clockdev(struct lg_cpu *cpu)
{
	hrtimer_init(&cpu->hrt, CLOCK_REALTIME, HRTIMER_MODE_ABS);
	cpu->hrt.function = clockdev_fn;
}
