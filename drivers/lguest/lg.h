#ifndef _LGUEST_H
#define _LGUEST_H

#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <linux/init.h>
#include <linux/stringify.h>
#include <linux/lguest.h>
#include <linux/lguest_launcher.h>
#include <linux/wait.h>
#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/slab.h>

#include <asm/lguest.h>

void free_pagetables(void);
int init_pagetables(struct page **switcher_page, unsigned int pages);

struct pgdir {
	unsigned long gpgdir;
	pgd_t *pgdir;
};

struct lguest_pages {
	
	char spare[PAGE_SIZE - sizeof(struct lguest_regs)];
	struct lguest_regs regs;

	
	struct lguest_ro_state state;
} __attribute__((aligned(PAGE_SIZE)));

#define CHANGED_IDT		1
#define CHANGED_GDT		2
#define CHANGED_GDT_TLS		4 
#define CHANGED_ALL	        3

struct lg_cpu {
	unsigned int id;
	struct lguest *lg;
	struct task_struct *tsk;
	struct mm_struct *mm; 	

	u32 cr2;
	int ts;
	u32 esp1;
	u16 ss1;

	
	int changed;

	unsigned long pending_notify; 

	
	unsigned long regs_page;
	struct lguest_regs *regs;

	struct lguest_pages *last_pages;

	
	bool linear_pages;
	int cpu_pgd; 

	
	struct hcall_args *hcall;
	u32 next_hcall;

	
	struct hrtimer hrt;

	
	int halted;

	
	DECLARE_BITMAP(irqs_pending, LGUEST_IRQS);

	struct lg_cpu_arch arch;
};

struct lg_eventfd {
	unsigned long addr;
	struct eventfd_ctx *event;
};

struct lg_eventfd_map {
	unsigned int num;
	struct lg_eventfd map[];
};

struct lguest {
	struct lguest_data __user *lguest_data;
	struct lg_cpu cpus[NR_CPUS];
	unsigned int nr_cpus;

	u32 pfn_limit;

	void __user *mem_base;
	unsigned long kernel_address;

	struct pgdir pgdirs[4];

	unsigned long noirq_start, noirq_end;

	unsigned int stack_pages;
	u32 tsc_khz;

	struct lg_eventfd_map *eventfds;

	
	const char *dead;
};

extern struct mutex lguest_lock;

bool lguest_address_ok(const struct lguest *lg,
		       unsigned long addr, unsigned long len);
void __lgread(struct lg_cpu *, void *, unsigned long, unsigned);
void __lgwrite(struct lg_cpu *, unsigned long, const void *, unsigned);

#define lgread(cpu, addr, type)						\
	({ type _v; __lgread((cpu), &_v, (addr), sizeof(_v)); _v; })

#define lgwrite(cpu, addr, type, val)				\
	do {							\
		typecheck(type, val);				\
		__lgwrite((cpu), (addr), &(val), sizeof(val));	\
	} while(0)

int run_guest(struct lg_cpu *cpu, unsigned long __user *user);

#define pgd_flags(x)	(pgd_val(x) & ~PAGE_MASK)
#define pgd_pfn(x)	(pgd_val(x) >> PAGE_SHIFT)
#define pmd_flags(x)    (pmd_val(x) & ~PAGE_MASK)
#define pmd_pfn(x)	(pmd_val(x) >> PAGE_SHIFT)

unsigned int interrupt_pending(struct lg_cpu *cpu, bool *more);
void try_deliver_interrupt(struct lg_cpu *cpu, unsigned int irq, bool more);
void set_interrupt(struct lg_cpu *cpu, unsigned int irq);
bool deliver_trap(struct lg_cpu *cpu, unsigned int num);
void load_guest_idt_entry(struct lg_cpu *cpu, unsigned int i,
			  u32 low, u32 hi);
void guest_set_stack(struct lg_cpu *cpu, u32 seg, u32 esp, unsigned int pages);
void pin_stack_pages(struct lg_cpu *cpu);
void setup_default_idt_entries(struct lguest_ro_state *state,
			       const unsigned long *def);
void copy_traps(const struct lg_cpu *cpu, struct desc_struct *idt,
		const unsigned long *def);
void guest_set_clockevent(struct lg_cpu *cpu, unsigned long delta);
bool send_notify_to_eventfd(struct lg_cpu *cpu);
void init_clockdev(struct lg_cpu *cpu);
bool check_syscall_vector(struct lguest *lg);
int init_interrupts(void);
void free_interrupts(void);

void setup_default_gdt_entries(struct lguest_ro_state *state);
void setup_guest_gdt(struct lg_cpu *cpu);
void load_guest_gdt_entry(struct lg_cpu *cpu, unsigned int i,
			  u32 low, u32 hi);
void guest_load_tls(struct lg_cpu *cpu, unsigned long tls_array);
void copy_gdt(const struct lg_cpu *cpu, struct desc_struct *gdt);
void copy_gdt_tls(const struct lg_cpu *cpu, struct desc_struct *gdt);

int init_guest_pagetable(struct lguest *lg);
void free_guest_pagetable(struct lguest *lg);
void guest_new_pagetable(struct lg_cpu *cpu, unsigned long pgtable);
void guest_set_pgd(struct lguest *lg, unsigned long gpgdir, u32 i);
#ifdef CONFIG_X86_PAE
void guest_set_pmd(struct lguest *lg, unsigned long gpgdir, u32 i);
#endif
void guest_pagetable_clear_all(struct lg_cpu *cpu);
void guest_pagetable_flush_user(struct lg_cpu *cpu);
void guest_set_pte(struct lg_cpu *cpu, unsigned long gpgdir,
		   unsigned long vaddr, pte_t val);
void map_switcher_in_guest(struct lg_cpu *cpu, struct lguest_pages *pages);
bool demand_page(struct lg_cpu *cpu, unsigned long cr2, int errcode);
void pin_page(struct lg_cpu *cpu, unsigned long vaddr);
unsigned long guest_pa(struct lg_cpu *cpu, unsigned long vaddr);
void page_table_guest_data_init(struct lg_cpu *cpu);

void lguest_arch_host_init(void);
void lguest_arch_host_fini(void);
void lguest_arch_run_guest(struct lg_cpu *cpu);
void lguest_arch_handle_trap(struct lg_cpu *cpu);
int lguest_arch_init_hypercalls(struct lg_cpu *cpu);
int lguest_arch_do_hcall(struct lg_cpu *cpu, struct hcall_args *args);
void lguest_arch_setup_regs(struct lg_cpu *cpu, unsigned long start);

extern char start_switcher_text[], end_switcher_text[], switch_to_guest[];

int lguest_device_init(void);
void lguest_device_remove(void);

void do_hypercalls(struct lg_cpu *cpu);
void write_timestamp(struct lg_cpu *cpu);

#define kill_guest(cpu, fmt...)					\
do {								\
	if (!(cpu)->lg->dead) {					\
		(cpu)->lg->dead = kasprintf(GFP_ATOMIC, fmt);	\
		if (!(cpu)->lg->dead)				\
			(cpu)->lg->dead = ERR_PTR(-ENOMEM);	\
	}							\
} while(0)

#endif	
#endif	
