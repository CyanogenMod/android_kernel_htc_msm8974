/*
 * Machine check handler.
 *
 * K8 parts Copyright 2002,2003 Andi Kleen, SuSE Labs.
 * Rest from unknown author(s).
 * 2004 Andi Kleen. Rewrote most of it.
 * Copyright 2008 Intel Corporation
 * Author: Andi Kleen
 */
#include <linux/thread_info.h>
#include <linux/capability.h>
#include <linux/miscdevice.h>
#include <linux/ratelimit.h>
#include <linux/kallsyms.h>
#include <linux/rcupdate.h>
#include <linux/kobject.h>
#include <linux/uaccess.h>
#include <linux/kdebug.h>
#include <linux/kernel.h>
#include <linux/percpu.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/syscore_ops.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/sched.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/poll.h>
#include <linux/nmi.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/debugfs.h>
#include <linux/irq_work.h>
#include <linux/export.h>

#include <asm/processor.h>
#include <asm/mce.h>
#include <asm/msr.h>

#include "mce-internal.h"

static DEFINE_MUTEX(mce_chrdev_read_mutex);

#define rcu_dereference_check_mce(p) \
	rcu_dereference_index_check((p), \
			      rcu_read_lock_sched_held() || \
			      lockdep_is_held(&mce_chrdev_read_mutex))

#define CREATE_TRACE_POINTS
#include <trace/events/mce.h>

int mce_disabled __read_mostly;

#define MISC_MCELOG_MINOR	227

#define SPINUNIT 100	

atomic_t mce_entry;

DEFINE_PER_CPU(unsigned, mce_exception_count);

static int			tolerant		__read_mostly = 1;
static int			banks			__read_mostly;
static int			rip_msr			__read_mostly;
static int			mce_bootlog		__read_mostly = -1;
static int			monarch_timeout		__read_mostly = -1;
static int			mce_panic_timeout	__read_mostly;
static int			mce_dont_log_ce		__read_mostly;
int				mce_cmci_disabled	__read_mostly;
int				mce_ignore_ce		__read_mostly;
int				mce_ser			__read_mostly;

struct mce_bank                *mce_banks		__read_mostly;

static unsigned long		mce_need_notify;
static char			mce_helper[128];
static char			*mce_helper_argv[2] = { mce_helper, NULL };

static DECLARE_WAIT_QUEUE_HEAD(mce_chrdev_wait);

static DEFINE_PER_CPU(struct mce, mces_seen);
static int			cpu_missing;

DEFINE_PER_CPU(mce_banks_t, mce_poll_banks) = {
	[0 ... BITS_TO_LONGS(MAX_NR_BANKS)-1] = ~0UL
};

static DEFINE_PER_CPU(struct work_struct, mce_work);

ATOMIC_NOTIFIER_HEAD(x86_mce_decoder_chain);

void mce_setup(struct mce *m)
{
	memset(m, 0, sizeof(struct mce));
	m->cpu = m->extcpu = smp_processor_id();
	rdtscll(m->tsc);
	
	m->time = get_seconds();
	m->cpuvendor = boot_cpu_data.x86_vendor;
	m->cpuid = cpuid_eax(1);
	m->socketid = cpu_data(m->extcpu).phys_proc_id;
	m->apicid = cpu_data(m->extcpu).initial_apicid;
	rdmsrl(MSR_IA32_MCG_CAP, m->mcgcap);
}

DEFINE_PER_CPU(struct mce, injectm);
EXPORT_PER_CPU_SYMBOL_GPL(injectm);


static struct mce_log mcelog = {
	.signature	= MCE_LOG_SIGNATURE,
	.len		= MCE_LOG_LEN,
	.recordlen	= sizeof(struct mce),
};

void mce_log(struct mce *mce)
{
	unsigned next, entry;
	int ret = 0;

	
	trace_mce_record(mce);

	ret = atomic_notifier_call_chain(&x86_mce_decoder_chain, 0, mce);
	if (ret == NOTIFY_STOP)
		return;

	mce->finished = 0;
	wmb();
	for (;;) {
		entry = rcu_dereference_check_mce(mcelog.next);
		for (;;) {

			if (entry >= MCE_LOG_LEN) {
				set_bit(MCE_OVERFLOW,
					(unsigned long *)&mcelog.flags);
				return;
			}
			
			if (mcelog.entry[entry].finished) {
				entry++;
				continue;
			}
			break;
		}
		smp_rmb();
		next = entry + 1;
		if (cmpxchg(&mcelog.next, entry, next) == entry)
			break;
	}
	memcpy(mcelog.entry + entry, mce, sizeof(struct mce));
	wmb();
	mcelog.entry[entry].finished = 1;
	wmb();

	mce->finished = 1;
	set_bit(0, &mce_need_notify);
}

static void drain_mcelog_buffer(void)
{
	unsigned int next, i, prev = 0;

	next = ACCESS_ONCE(mcelog.next);

	do {
		struct mce *m;

		
		for (i = prev; i < next; i++) {
			unsigned long start = jiffies;
			unsigned retries = 1;

			m = &mcelog.entry[i];

			while (!m->finished) {
				if (time_after_eq(jiffies, start + 2*retries))
					retries++;

				cpu_relax();

				if (!m->finished && retries >= 4) {
					pr_err("MCE: skipping error being logged currently!\n");
					break;
				}
			}
			smp_rmb();
			atomic_notifier_call_chain(&x86_mce_decoder_chain, 0, m);
		}

		memset(mcelog.entry + prev, 0, (next - prev) * sizeof(*m));
		prev = next;
		next = cmpxchg(&mcelog.next, prev, 0);
	} while (next != prev);
}


void mce_register_decode_chain(struct notifier_block *nb)
{
	atomic_notifier_chain_register(&x86_mce_decoder_chain, nb);
	drain_mcelog_buffer();
}
EXPORT_SYMBOL_GPL(mce_register_decode_chain);

void mce_unregister_decode_chain(struct notifier_block *nb)
{
	atomic_notifier_chain_unregister(&x86_mce_decoder_chain, nb);
}
EXPORT_SYMBOL_GPL(mce_unregister_decode_chain);

static void print_mce(struct mce *m)
{
	int ret = 0;

	pr_emerg(HW_ERR "CPU %d: Machine Check Exception: %Lx Bank %d: %016Lx\n",
	       m->extcpu, m->mcgstatus, m->bank, m->status);

	if (m->ip) {
		pr_emerg(HW_ERR "RIP%s %02x:<%016Lx> ",
			!(m->mcgstatus & MCG_STATUS_EIPV) ? " !INEXACT!" : "",
				m->cs, m->ip);

		if (m->cs == __KERNEL_CS)
			print_symbol("{%s}", m->ip);
		pr_cont("\n");
	}

	pr_emerg(HW_ERR "TSC %llx ", m->tsc);
	if (m->addr)
		pr_cont("ADDR %llx ", m->addr);
	if (m->misc)
		pr_cont("MISC %llx ", m->misc);

	pr_cont("\n");
	pr_emerg(HW_ERR "PROCESSOR %u:%x TIME %llu SOCKET %u APIC %x microcode %x\n",
		m->cpuvendor, m->cpuid, m->time, m->socketid, m->apicid,
		cpu_data(m->extcpu).microcode);

	ret = atomic_notifier_call_chain(&x86_mce_decoder_chain, 0, m);
	if (ret == NOTIFY_STOP)
		return;

	pr_emerg_ratelimited(HW_ERR "Run the above through 'mcelog --ascii'\n");
}

#define PANIC_TIMEOUT 5 

static atomic_t mce_paniced;

static int fake_panic;
static atomic_t mce_fake_paniced;

static void wait_for_panic(void)
{
	long timeout = PANIC_TIMEOUT*USEC_PER_SEC;

	preempt_disable();
	local_irq_enable();
	while (timeout-- > 0)
		udelay(1);
	if (panic_timeout == 0)
		panic_timeout = mce_panic_timeout;
	panic("Panicing machine check CPU died");
}

static void mce_panic(char *msg, struct mce *final, char *exp)
{
	int i, apei_err = 0;

	if (!fake_panic) {
		if (atomic_inc_return(&mce_paniced) > 1)
			wait_for_panic();
		barrier();

		bust_spinlocks(1);
		console_verbose();
	} else {
		
		if (atomic_inc_return(&mce_fake_paniced) > 1)
			return;
	}
	
	for (i = 0; i < MCE_LOG_LEN; i++) {
		struct mce *m = &mcelog.entry[i];
		if (!(m->status & MCI_STATUS_VAL))
			continue;
		if (!(m->status & MCI_STATUS_UC)) {
			print_mce(m);
			if (!apei_err)
				apei_err = apei_write_mce(m);
		}
	}
	
	for (i = 0; i < MCE_LOG_LEN; i++) {
		struct mce *m = &mcelog.entry[i];
		if (!(m->status & MCI_STATUS_VAL))
			continue;
		if (!(m->status & MCI_STATUS_UC))
			continue;
		if (!final || memcmp(m, final, sizeof(struct mce))) {
			print_mce(m);
			if (!apei_err)
				apei_err = apei_write_mce(m);
		}
	}
	if (final) {
		print_mce(final);
		if (!apei_err)
			apei_err = apei_write_mce(final);
	}
	if (cpu_missing)
		pr_emerg(HW_ERR "Some CPUs didn't answer in synchronization\n");
	if (exp)
		pr_emerg(HW_ERR "Machine check: %s\n", exp);
	if (!fake_panic) {
		if (panic_timeout == 0)
			panic_timeout = mce_panic_timeout;
		panic(msg);
	} else
		pr_emerg(HW_ERR "Fake kernel panic: %s\n", msg);
}


static int msr_to_offset(u32 msr)
{
	unsigned bank = __this_cpu_read(injectm.bank);

	if (msr == rip_msr)
		return offsetof(struct mce, ip);
	if (msr == MSR_IA32_MCx_STATUS(bank))
		return offsetof(struct mce, status);
	if (msr == MSR_IA32_MCx_ADDR(bank))
		return offsetof(struct mce, addr);
	if (msr == MSR_IA32_MCx_MISC(bank))
		return offsetof(struct mce, misc);
	if (msr == MSR_IA32_MCG_STATUS)
		return offsetof(struct mce, mcgstatus);
	return -1;
}

static u64 mce_rdmsrl(u32 msr)
{
	u64 v;

	if (__this_cpu_read(injectm.finished)) {
		int offset = msr_to_offset(msr);

		if (offset < 0)
			return 0;
		return *(u64 *)((char *)&__get_cpu_var(injectm) + offset);
	}

	if (rdmsrl_safe(msr, &v)) {
		WARN_ONCE(1, "mce: Unable to read msr %d!\n", msr);
		v = 0;
	}

	return v;
}

static void mce_wrmsrl(u32 msr, u64 v)
{
	if (__this_cpu_read(injectm.finished)) {
		int offset = msr_to_offset(msr);

		if (offset >= 0)
			*(u64 *)((char *)&__get_cpu_var(injectm) + offset) = v;
		return;
	}
	wrmsrl(msr, v);
}

static inline void mce_gather_info(struct mce *m, struct pt_regs *regs)
{
	mce_setup(m);

	m->mcgstatus = mce_rdmsrl(MSR_IA32_MCG_STATUS);
	if (regs) {
		if (m->mcgstatus & (MCG_STATUS_RIPV|MCG_STATUS_EIPV)) {
			m->ip = regs->ip;
			m->cs = regs->cs;
		}
		
		if (rip_msr)
			m->ip = mce_rdmsrl(rip_msr);
	}
}

#define MCE_RING_SIZE 16	

struct mce_ring {
	unsigned short start;
	unsigned short end;
	unsigned long ring[MCE_RING_SIZE];
};
static DEFINE_PER_CPU(struct mce_ring, mce_ring);

static int mce_ring_empty(void)
{
	struct mce_ring *r = &__get_cpu_var(mce_ring);

	return r->start == r->end;
}

static int mce_ring_get(unsigned long *pfn)
{
	struct mce_ring *r;
	int ret = 0;

	*pfn = 0;
	get_cpu();
	r = &__get_cpu_var(mce_ring);
	if (r->start == r->end)
		goto out;
	*pfn = r->ring[r->start];
	r->start = (r->start + 1) % MCE_RING_SIZE;
	ret = 1;
out:
	put_cpu();
	return ret;
}

static int mce_ring_add(unsigned long pfn)
{
	struct mce_ring *r = &__get_cpu_var(mce_ring);
	unsigned next;

	next = (r->end + 1) % MCE_RING_SIZE;
	if (next == r->start)
		return -1;
	r->ring[r->end] = pfn;
	wmb();
	r->end = next;
	return 0;
}

int mce_available(struct cpuinfo_x86 *c)
{
	if (mce_disabled)
		return 0;
	return cpu_has(c, X86_FEATURE_MCE) && cpu_has(c, X86_FEATURE_MCA);
}

static void mce_schedule_work(void)
{
	if (!mce_ring_empty()) {
		struct work_struct *work = &__get_cpu_var(mce_work);
		if (!work_pending(work))
			schedule_work(work);
	}
}

DEFINE_PER_CPU(struct irq_work, mce_irq_work);

static void mce_irq_work_cb(struct irq_work *entry)
{
	mce_notify_irq();
	mce_schedule_work();
}

static void mce_report_event(struct pt_regs *regs)
{
	if (regs->flags & (X86_VM_MASK|X86_EFLAGS_IF)) {
		mce_notify_irq();
		mce_schedule_work();
		return;
	}

	irq_work_queue(&__get_cpu_var(mce_irq_work));
}

static void mce_read_aux(struct mce *m, int i)
{
	if (m->status & MCI_STATUS_MISCV)
		m->misc = mce_rdmsrl(MSR_IA32_MCx_MISC(i));
	if (m->status & MCI_STATUS_ADDRV) {
		m->addr = mce_rdmsrl(MSR_IA32_MCx_ADDR(i));

		if (mce_ser && (m->status & MCI_STATUS_MISCV)) {
			u8 shift = MCI_MISC_ADDR_LSB(m->misc);
			m->addr >>= shift;
			m->addr <<= shift;
		}
	}
}

DEFINE_PER_CPU(unsigned, mce_poll_count);

void machine_check_poll(enum mcp_flags flags, mce_banks_t *b)
{
	struct mce m;
	int i;

	percpu_inc(mce_poll_count);

	mce_gather_info(&m, NULL);

	for (i = 0; i < banks; i++) {
		if (!mce_banks[i].ctl || !test_bit(i, *b))
			continue;

		m.misc = 0;
		m.addr = 0;
		m.bank = i;
		m.tsc = 0;

		barrier();
		m.status = mce_rdmsrl(MSR_IA32_MCx_STATUS(i));
		if (!(m.status & MCI_STATUS_VAL))
			continue;

		if (!(flags & MCP_UC) &&
		    (m.status & (mce_ser ? MCI_STATUS_S : MCI_STATUS_UC)))
			continue;

		mce_read_aux(&m, i);

		if (!(flags & MCP_TIMESTAMP))
			m.tsc = 0;
		if (!(flags & MCP_DONTLOG) && !mce_dont_log_ce)
			mce_log(&m);

		mce_wrmsrl(MSR_IA32_MCx_STATUS(i), 0);
	}


	sync_core();
}
EXPORT_SYMBOL_GPL(machine_check_poll);

static int mce_no_way_out(struct mce *m, char **msg)
{
	int i;

	for (i = 0; i < banks; i++) {
		m->status = mce_rdmsrl(MSR_IA32_MCx_STATUS(i));
		if (mce_severity(m, tolerant, msg) >= MCE_PANIC_SEVERITY)
			return 1;
	}
	return 0;
}

static atomic_t mce_executing;

static atomic_t mce_callin;

static int mce_timed_out(u64 *t)
{
	rmb();
	if (atomic_read(&mce_paniced))
		wait_for_panic();
	if (!monarch_timeout)
		goto out;
	if ((s64)*t < SPINUNIT) {
		
		if (tolerant < 1)
			mce_panic("Timeout synchronizing machine check over CPUs",
				  NULL, NULL);
		cpu_missing = 1;
		return 1;
	}
	*t -= SPINUNIT;
out:
	touch_nmi_watchdog();
	return 0;
}

static void mce_reign(void)
{
	int cpu;
	struct mce *m = NULL;
	int global_worst = 0;
	char *msg = NULL;
	char *nmsg = NULL;

	for_each_possible_cpu(cpu) {
		int severity = mce_severity(&per_cpu(mces_seen, cpu), tolerant,
					    &nmsg);
		if (severity > global_worst) {
			msg = nmsg;
			global_worst = severity;
			m = &per_cpu(mces_seen, cpu);
		}
	}

	if (m && global_worst >= MCE_PANIC_SEVERITY && tolerant < 3)
		mce_panic("Fatal Machine check", m, msg);


	if (global_worst <= MCE_KEEP_SEVERITY && tolerant < 3)
		mce_panic("Machine check from unknown source", NULL, NULL);

	for_each_possible_cpu(cpu)
		memset(&per_cpu(mces_seen, cpu), 0, sizeof(struct mce));
}

static atomic_t global_nwo;

static int mce_start(int *no_way_out)
{
	int order;
	int cpus = num_online_cpus();
	u64 timeout = (u64)monarch_timeout * NSEC_PER_USEC;

	if (!timeout)
		return -1;

	atomic_add(*no_way_out, &global_nwo);
	smp_wmb();
	order = atomic_inc_return(&mce_callin);

	while (atomic_read(&mce_callin) != cpus) {
		if (mce_timed_out(&timeout)) {
			atomic_set(&global_nwo, 0);
			return -1;
		}
		ndelay(SPINUNIT);
	}

	smp_rmb();

	if (order == 1) {
		atomic_set(&mce_executing, 1);
	} else {
		while (atomic_read(&mce_executing) < order) {
			if (mce_timed_out(&timeout)) {
				atomic_set(&global_nwo, 0);
				return -1;
			}
			ndelay(SPINUNIT);
		}
	}

	*no_way_out = atomic_read(&global_nwo);

	return order;
}

static int mce_end(int order)
{
	int ret = -1;
	u64 timeout = (u64)monarch_timeout * NSEC_PER_USEC;

	if (!timeout)
		goto reset;
	if (order < 0)
		goto reset;

	atomic_inc(&mce_executing);

	if (order == 1) {
		
		int cpus = num_online_cpus();

		while (atomic_read(&mce_executing) <= cpus) {
			if (mce_timed_out(&timeout))
				goto reset;
			ndelay(SPINUNIT);
		}

		mce_reign();
		barrier();
		ret = 0;
	} else {
		while (atomic_read(&mce_executing) != 0) {
			if (mce_timed_out(&timeout))
				goto reset;
			ndelay(SPINUNIT);
		}

		return 0;
	}

reset:
	atomic_set(&global_nwo, 0);
	atomic_set(&mce_callin, 0);
	barrier();

	atomic_set(&mce_executing, 0);
	return ret;
}

static int mce_usable_address(struct mce *m)
{
	if (!(m->status & MCI_STATUS_MISCV) || !(m->status & MCI_STATUS_ADDRV))
		return 0;
	if (MCI_MISC_ADDR_LSB(m->misc) > PAGE_SHIFT)
		return 0;
	if (MCI_MISC_ADDR_MODE(m->misc) != MCI_MISC_ADDR_PHYS)
		return 0;
	return 1;
}

static void mce_clear_state(unsigned long *toclear)
{
	int i;

	for (i = 0; i < banks; i++) {
		if (test_bit(i, toclear))
			mce_wrmsrl(MSR_IA32_MCx_STATUS(i), 0);
	}
}

#define	MCE_INFO_MAX	16

struct mce_info {
	atomic_t		inuse;
	struct task_struct	*t;
	__u64			paddr;
	int			restartable;
} mce_info[MCE_INFO_MAX];

static void mce_save_info(__u64 addr, int c)
{
	struct mce_info *mi;

	for (mi = mce_info; mi < &mce_info[MCE_INFO_MAX]; mi++) {
		if (atomic_cmpxchg(&mi->inuse, 0, 1) == 0) {
			mi->t = current;
			mi->paddr = addr;
			mi->restartable = c;
			return;
		}
	}

	mce_panic("Too many concurrent recoverable errors", NULL, NULL);
}

static struct mce_info *mce_find_info(void)
{
	struct mce_info *mi;

	for (mi = mce_info; mi < &mce_info[MCE_INFO_MAX]; mi++)
		if (atomic_read(&mi->inuse) && mi->t == current)
			return mi;
	return NULL;
}

static void mce_clear_info(struct mce_info *mi)
{
	atomic_set(&mi->inuse, 0);
}

void do_machine_check(struct pt_regs *regs, long error_code)
{
	struct mce m, *final;
	int i;
	int worst = 0;
	int severity;
	int order;
	int no_way_out = 0;
	int kill_it = 0;
	DECLARE_BITMAP(toclear, MAX_NR_BANKS);
	char *msg = "Unknown";

	atomic_inc(&mce_entry);

	percpu_inc(mce_exception_count);

	if (!banks)
		goto out;

	mce_gather_info(&m, regs);

	final = &__get_cpu_var(mces_seen);
	*final = m;

	no_way_out = mce_no_way_out(&m, &msg);

	barrier();

	if (!(m.mcgstatus & MCG_STATUS_RIPV))
		kill_it = 1;

	order = mce_start(&no_way_out);
	for (i = 0; i < banks; i++) {
		__clear_bit(i, toclear);
		if (!mce_banks[i].ctl)
			continue;

		m.misc = 0;
		m.addr = 0;
		m.bank = i;

		m.status = mce_rdmsrl(MSR_IA32_MCx_STATUS(i));
		if ((m.status & MCI_STATUS_VAL) == 0)
			continue;

		if (!(m.status & (mce_ser ? MCI_STATUS_S : MCI_STATUS_UC)) &&
			!no_way_out)
			continue;

		add_taint(TAINT_MACHINE_CHECK);

		severity = mce_severity(&m, tolerant, NULL);

		if (severity == MCE_KEEP_SEVERITY && !no_way_out)
			continue;
		__set_bit(i, toclear);
		if (severity == MCE_NO_SEVERITY) {
			continue;
		}

		mce_read_aux(&m, i);

		if (severity == MCE_AO_SEVERITY && mce_usable_address(&m))
			mce_ring_add(m.addr >> PAGE_SHIFT);

		mce_log(&m);

		if (severity > worst) {
			*final = m;
			worst = severity;
		}
	}

	
	m = *final;

	if (!no_way_out)
		mce_clear_state(toclear);

	if (mce_end(order) < 0)
		no_way_out = worst >= MCE_PANIC_SEVERITY;

	if (tolerant < 3) {
		if (no_way_out)
			mce_panic("Fatal machine check on current CPU", &m, msg);
		if (worst == MCE_AR_SEVERITY) {
			
			mce_save_info(m.addr, m.mcgstatus & MCG_STATUS_RIPV);
			set_thread_flag(TIF_MCE_NOTIFY);
		} else if (kill_it) {
			force_sig(SIGBUS, current);
		}
	}

	if (worst > 0)
		mce_report_event(regs);
	mce_wrmsrl(MSR_IA32_MCG_STATUS, 0);
out:
	atomic_dec(&mce_entry);
	sync_core();
}
EXPORT_SYMBOL_GPL(do_machine_check);

#ifndef CONFIG_MEMORY_FAILURE
int memory_failure(unsigned long pfn, int vector, int flags)
{
	
	BUG_ON(flags & MF_ACTION_REQUIRED);
	printk(KERN_ERR "Uncorrected memory error in page 0x%lx ignored\n"
		"Rebuild kernel with CONFIG_MEMORY_FAILURE=y for smarter handling\n", pfn);

	return 0;
}
#endif

void mce_notify_process(void)
{
	unsigned long pfn;
	struct mce_info *mi = mce_find_info();

	if (!mi)
		mce_panic("Lost physical address for unconsumed uncorrectable error", NULL, NULL);
	pfn = mi->paddr >> PAGE_SHIFT;

	clear_thread_flag(TIF_MCE_NOTIFY);

	pr_err("Uncorrected hardware memory error in user-access at %llx",
		 mi->paddr);
	if (memory_failure(pfn, MCE_VECTOR, MF_ACTION_REQUIRED) < 0 ||
			   mi->restartable == 0) {
		pr_err("Memory error not recovered");
		force_sig(SIGBUS, current);
	}
	mce_clear_info(mi);
}

static void mce_process_work(struct work_struct *dummy)
{
	unsigned long pfn;

	while (mce_ring_get(&pfn))
		memory_failure(pfn, MCE_VECTOR, 0);
}

#ifdef CONFIG_X86_MCE_INTEL
void mce_log_therm_throt_event(__u64 status)
{
	struct mce m;

	mce_setup(&m);
	m.bank = MCE_THERMAL_BANK;
	m.status = status;
	mce_log(&m);
}
#endif 

static int check_interval = 5 * 60; 

static DEFINE_PER_CPU(int, mce_next_interval); 
static DEFINE_PER_CPU(struct timer_list, mce_timer);

static void mce_start_timer(unsigned long data)
{
	struct timer_list *t = &per_cpu(mce_timer, data);
	int *n;

	WARN_ON(smp_processor_id() != data);

	if (mce_available(__this_cpu_ptr(&cpu_info))) {
		machine_check_poll(MCP_TIMESTAMP,
				&__get_cpu_var(mce_poll_banks));
	}

	n = &__get_cpu_var(mce_next_interval);
	if (mce_notify_irq())
		*n = max(*n/2, HZ/100);
	else
		*n = min(*n*2, (int)round_jiffies_relative(check_interval*HZ));

	t->expires = jiffies + *n;
	add_timer_on(t, smp_processor_id());
}

static void mce_timer_delete_all(void)
{
	int cpu;

	for_each_online_cpu(cpu)
		del_timer_sync(&per_cpu(mce_timer, cpu));
}

static void mce_do_trigger(struct work_struct *work)
{
	call_usermodehelper(mce_helper, mce_helper_argv, NULL, UMH_NO_WAIT);
}

static DECLARE_WORK(mce_trigger_work, mce_do_trigger);

int mce_notify_irq(void)
{
	
	static DEFINE_RATELIMIT_STATE(ratelimit, 60*HZ, 2);

	if (test_and_clear_bit(0, &mce_need_notify)) {
		
		wake_up_interruptible(&mce_chrdev_wait);

		if (mce_helper[0] && !work_pending(&mce_trigger_work))
			schedule_work(&mce_trigger_work);

		if (__ratelimit(&ratelimit))
			pr_info(HW_ERR "Machine check events logged\n");

		return 1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(mce_notify_irq);

static int __cpuinit __mcheck_cpu_mce_banks_init(void)
{
	int i;

	mce_banks = kzalloc(banks * sizeof(struct mce_bank), GFP_KERNEL);
	if (!mce_banks)
		return -ENOMEM;
	for (i = 0; i < banks; i++) {
		struct mce_bank *b = &mce_banks[i];

		b->ctl = -1ULL;
		b->init = 1;
	}
	return 0;
}

static int __cpuinit __mcheck_cpu_cap_init(void)
{
	unsigned b;
	u64 cap;

	rdmsrl(MSR_IA32_MCG_CAP, cap);

	b = cap & MCG_BANKCNT_MASK;
	if (!banks)
		printk(KERN_INFO "mce: CPU supports %d MCE banks\n", b);

	if (b > MAX_NR_BANKS) {
		printk(KERN_WARNING
		       "MCE: Using only %u machine check banks out of %u\n",
			MAX_NR_BANKS, b);
		b = MAX_NR_BANKS;
	}

	
	WARN_ON(banks != 0 && b != banks);
	banks = b;
	if (!mce_banks) {
		int err = __mcheck_cpu_mce_banks_init();

		if (err)
			return err;
	}

	
	if ((cap & MCG_EXT_P) && MCG_EXT_CNT(cap) >= 9)
		rip_msr = MSR_IA32_MCG_EIP;

	if (cap & MCG_SER_P)
		mce_ser = 1;

	return 0;
}

static void __mcheck_cpu_init_generic(void)
{
	mce_banks_t all_banks;
	u64 cap;
	int i;

	bitmap_fill(all_banks, MAX_NR_BANKS);
	machine_check_poll(MCP_UC|(!mce_bootlog ? MCP_DONTLOG : 0), &all_banks);

	set_in_cr4(X86_CR4_MCE);

	rdmsrl(MSR_IA32_MCG_CAP, cap);
	if (cap & MCG_CTL_P)
		wrmsr(MSR_IA32_MCG_CTL, 0xffffffff, 0xffffffff);

	for (i = 0; i < banks; i++) {
		struct mce_bank *b = &mce_banks[i];

		if (!b->init)
			continue;
		wrmsrl(MSR_IA32_MCx_CTL(i), b->ctl);
		wrmsrl(MSR_IA32_MCx_STATUS(i), 0);
	}
}

static int __cpuinit __mcheck_cpu_apply_quirks(struct cpuinfo_x86 *c)
{
	if (c->x86_vendor == X86_VENDOR_UNKNOWN) {
		pr_info("MCE: unknown CPU type - not enabling MCE support.\n");
		return -EOPNOTSUPP;
	}

	
	if (c->x86_vendor == X86_VENDOR_AMD) {
		if (c->x86 == 15 && banks > 4) {
			clear_bit(10, (unsigned long *)&mce_banks[4].ctl);
		}
		if (c->x86 <= 17 && mce_bootlog < 0) {
			mce_bootlog = 0;
		}
		 if (c->x86 == 6 && banks > 0)
			mce_banks[0].ctl = 0;
	}

	if (c->x86_vendor == X86_VENDOR_INTEL) {
		/*
		 * SDM documents that on family 6 bank 0 should not be written
		 * because it aliases to another special BIOS controlled
		 * register.
		 * But it's not aliased anymore on model 0x1a+
		 * Don't ignore bank 0 completely because there could be a
		 * valid event later, merely don't write CTL0.
		 */

		if (c->x86 == 6 && c->x86_model < 0x1A && banks > 0)
			mce_banks[0].init = 0;

		if ((c->x86 > 6 || (c->x86 == 6 && c->x86_model >= 0xe)) &&
			monarch_timeout < 0)
			monarch_timeout = USEC_PER_SEC;

		if (c->x86 == 6 && c->x86_model <= 13 && mce_bootlog < 0)
			mce_bootlog = 0;
	}
	if (monarch_timeout < 0)
		monarch_timeout = 0;
	if (mce_bootlog != 0)
		mce_panic_timeout = 30;

	return 0;
}

static int __cpuinit __mcheck_cpu_ancient_init(struct cpuinfo_x86 *c)
{
	if (c->x86 != 5)
		return 0;

	switch (c->x86_vendor) {
	case X86_VENDOR_INTEL:
		intel_p5_mcheck_init(c);
		return 1;
		break;
	case X86_VENDOR_CENTAUR:
		winchip_mcheck_init(c);
		return 1;
		break;
	}

	return 0;
}

static void __mcheck_cpu_init_vendor(struct cpuinfo_x86 *c)
{
	switch (c->x86_vendor) {
	case X86_VENDOR_INTEL:
		mce_intel_feature_init(c);
		break;
	case X86_VENDOR_AMD:
		mce_amd_feature_init(c);
		break;
	default:
		break;
	}
}

static void __mcheck_cpu_init_timer(void)
{
	struct timer_list *t = &__get_cpu_var(mce_timer);
	int *n = &__get_cpu_var(mce_next_interval);

	setup_timer(t, mce_start_timer, smp_processor_id());

	if (mce_ignore_ce)
		return;

	*n = check_interval * HZ;
	if (!*n)
		return;
	t->expires = round_jiffies(jiffies + *n);
	add_timer_on(t, smp_processor_id());
}

static void unexpected_machine_check(struct pt_regs *regs, long error_code)
{
	printk(KERN_ERR "CPU#%d: Unexpected int18 (Machine Check).\n",
	       smp_processor_id());
}

void (*machine_check_vector)(struct pt_regs *, long error_code) =
						unexpected_machine_check;

void __cpuinit mcheck_cpu_init(struct cpuinfo_x86 *c)
{
	if (mce_disabled)
		return;

	if (__mcheck_cpu_ancient_init(c))
		return;

	if (!mce_available(c))
		return;

	if (__mcheck_cpu_cap_init() < 0 || __mcheck_cpu_apply_quirks(c) < 0) {
		mce_disabled = 1;
		return;
	}

	machine_check_vector = do_machine_check;

	__mcheck_cpu_init_generic();
	__mcheck_cpu_init_vendor(c);
	__mcheck_cpu_init_timer();
	INIT_WORK(&__get_cpu_var(mce_work), mce_process_work);
	init_irq_work(&__get_cpu_var(mce_irq_work), &mce_irq_work_cb);
}


static DEFINE_SPINLOCK(mce_chrdev_state_lock);
static int mce_chrdev_open_count;	
static int mce_chrdev_open_exclu;	

static int mce_chrdev_open(struct inode *inode, struct file *file)
{
	spin_lock(&mce_chrdev_state_lock);

	if (mce_chrdev_open_exclu ||
	    (mce_chrdev_open_count && (file->f_flags & O_EXCL))) {
		spin_unlock(&mce_chrdev_state_lock);

		return -EBUSY;
	}

	if (file->f_flags & O_EXCL)
		mce_chrdev_open_exclu = 1;
	mce_chrdev_open_count++;

	spin_unlock(&mce_chrdev_state_lock);

	return nonseekable_open(inode, file);
}

static int mce_chrdev_release(struct inode *inode, struct file *file)
{
	spin_lock(&mce_chrdev_state_lock);

	mce_chrdev_open_count--;
	mce_chrdev_open_exclu = 0;

	spin_unlock(&mce_chrdev_state_lock);

	return 0;
}

static void collect_tscs(void *data)
{
	unsigned long *cpu_tsc = (unsigned long *)data;

	rdtscll(cpu_tsc[smp_processor_id()]);
}

static int mce_apei_read_done;

static int __mce_read_apei(char __user **ubuf, size_t usize)
{
	int rc;
	u64 record_id;
	struct mce m;

	if (usize < sizeof(struct mce))
		return -EINVAL;

	rc = apei_read_mce(&m, &record_id);
	
	if (rc <= 0) {
		mce_apei_read_done = 1;
		if (rc == -ENODEV)
			return 0;
		return rc;
	}
	rc = -EFAULT;
	if (copy_to_user(*ubuf, &m, sizeof(struct mce)))
		return rc;
	rc = apei_clear_mce(record_id);
	if (rc) {
		mce_apei_read_done = 1;
		return rc;
	}
	*ubuf += sizeof(struct mce);

	return 0;
}

static ssize_t mce_chrdev_read(struct file *filp, char __user *ubuf,
				size_t usize, loff_t *off)
{
	char __user *buf = ubuf;
	unsigned long *cpu_tsc;
	unsigned prev, next;
	int i, err;

	cpu_tsc = kmalloc(nr_cpu_ids * sizeof(long), GFP_KERNEL);
	if (!cpu_tsc)
		return -ENOMEM;

	mutex_lock(&mce_chrdev_read_mutex);

	if (!mce_apei_read_done) {
		err = __mce_read_apei(&buf, usize);
		if (err || buf != ubuf)
			goto out;
	}

	next = rcu_dereference_check_mce(mcelog.next);

	
	err = -EINVAL;
	if (*off != 0 || usize < MCE_LOG_LEN*sizeof(struct mce))
		goto out;

	err = 0;
	prev = 0;
	do {
		for (i = prev; i < next; i++) {
			unsigned long start = jiffies;
			struct mce *m = &mcelog.entry[i];

			while (!m->finished) {
				if (time_after_eq(jiffies, start + 2)) {
					memset(m, 0, sizeof(*m));
					goto timeout;
				}
				cpu_relax();
			}
			smp_rmb();
			err |= copy_to_user(buf, m, sizeof(*m));
			buf += sizeof(*m);
timeout:
			;
		}

		memset(mcelog.entry + prev, 0,
		       (next - prev) * sizeof(struct mce));
		prev = next;
		next = cmpxchg(&mcelog.next, prev, 0);
	} while (next != prev);

	synchronize_sched();

	/*
	 * Collect entries that were still getting written before the
	 * synchronize.
	 */
	on_each_cpu(collect_tscs, cpu_tsc, 1);

	for (i = next; i < MCE_LOG_LEN; i++) {
		struct mce *m = &mcelog.entry[i];

		if (m->finished && m->tsc < cpu_tsc[m->cpu]) {
			err |= copy_to_user(buf, m, sizeof(*m));
			smp_rmb();
			buf += sizeof(*m);
			memset(m, 0, sizeof(*m));
		}
	}

	if (err)
		err = -EFAULT;

out:
	mutex_unlock(&mce_chrdev_read_mutex);
	kfree(cpu_tsc);

	return err ? err : buf - ubuf;
}

static unsigned int mce_chrdev_poll(struct file *file, poll_table *wait)
{
	poll_wait(file, &mce_chrdev_wait, wait);
	if (rcu_access_index(mcelog.next))
		return POLLIN | POLLRDNORM;
	if (!mce_apei_read_done && apei_check_mce())
		return POLLIN | POLLRDNORM;
	return 0;
}

static long mce_chrdev_ioctl(struct file *f, unsigned int cmd,
				unsigned long arg)
{
	int __user *p = (int __user *)arg;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	switch (cmd) {
	case MCE_GET_RECORD_LEN:
		return put_user(sizeof(struct mce), p);
	case MCE_GET_LOG_LEN:
		return put_user(MCE_LOG_LEN, p);
	case MCE_GETCLEAR_FLAGS: {
		unsigned flags;

		do {
			flags = mcelog.flags;
		} while (cmpxchg(&mcelog.flags, flags, 0) != flags);

		return put_user(flags, p);
	}
	default:
		return -ENOTTY;
	}
}

static ssize_t (*mce_write)(struct file *filp, const char __user *ubuf,
			    size_t usize, loff_t *off);

void register_mce_write_callback(ssize_t (*fn)(struct file *filp,
			     const char __user *ubuf,
			     size_t usize, loff_t *off))
{
	mce_write = fn;
}
EXPORT_SYMBOL_GPL(register_mce_write_callback);

ssize_t mce_chrdev_write(struct file *filp, const char __user *ubuf,
			 size_t usize, loff_t *off)
{
	if (mce_write)
		return mce_write(filp, ubuf, usize, off);
	else
		return -EINVAL;
}

static const struct file_operations mce_chrdev_ops = {
	.open			= mce_chrdev_open,
	.release		= mce_chrdev_release,
	.read			= mce_chrdev_read,
	.write			= mce_chrdev_write,
	.poll			= mce_chrdev_poll,
	.unlocked_ioctl		= mce_chrdev_ioctl,
	.llseek			= no_llseek,
};

static struct miscdevice mce_chrdev_device = {
	MISC_MCELOG_MINOR,
	"mcelog",
	&mce_chrdev_ops,
};

static int __init mcheck_enable(char *str)
{
	if (*str == 0) {
		enable_p5_mce();
		return 1;
	}
	if (*str == '=')
		str++;
	if (!strcmp(str, "off"))
		mce_disabled = 1;
	else if (!strcmp(str, "no_cmci"))
		mce_cmci_disabled = 1;
	else if (!strcmp(str, "dont_log_ce"))
		mce_dont_log_ce = 1;
	else if (!strcmp(str, "ignore_ce"))
		mce_ignore_ce = 1;
	else if (!strcmp(str, "bootlog") || !strcmp(str, "nobootlog"))
		mce_bootlog = (str[0] == 'b');
	else if (isdigit(str[0])) {
		get_option(&str, &tolerant);
		if (*str == ',') {
			++str;
			get_option(&str, &monarch_timeout);
		}
	} else {
		printk(KERN_INFO "mce argument %s ignored. Please use /sys\n",
		       str);
		return 0;
	}
	return 1;
}
__setup("mce", mcheck_enable);

int __init mcheck_init(void)
{
	mcheck_intel_therm_init();

	return 0;
}


static int mce_disable_error_reporting(void)
{
	int i;

	for (i = 0; i < banks; i++) {
		struct mce_bank *b = &mce_banks[i];

		if (b->init)
			wrmsrl(MSR_IA32_MCx_CTL(i), 0);
	}
	return 0;
}

static int mce_syscore_suspend(void)
{
	return mce_disable_error_reporting();
}

static void mce_syscore_shutdown(void)
{
	mce_disable_error_reporting();
}

static void mce_syscore_resume(void)
{
	__mcheck_cpu_init_generic();
	__mcheck_cpu_init_vendor(__this_cpu_ptr(&cpu_info));
}

static struct syscore_ops mce_syscore_ops = {
	.suspend	= mce_syscore_suspend,
	.shutdown	= mce_syscore_shutdown,
	.resume		= mce_syscore_resume,
};


static void mce_cpu_restart(void *data)
{
	if (!mce_available(__this_cpu_ptr(&cpu_info)))
		return;
	__mcheck_cpu_init_generic();
	__mcheck_cpu_init_timer();
}

static void mce_restart(void)
{
	mce_timer_delete_all();
	on_each_cpu(mce_cpu_restart, NULL, 1);
}

static void mce_disable_cmci(void *data)
{
	if (!mce_available(__this_cpu_ptr(&cpu_info)))
		return;
	cmci_clear();
}

static void mce_enable_ce(void *all)
{
	if (!mce_available(__this_cpu_ptr(&cpu_info)))
		return;
	cmci_reenable();
	cmci_recheck();
	if (all)
		__mcheck_cpu_init_timer();
}

static struct bus_type mce_subsys = {
	.name		= "machinecheck",
	.dev_name	= "machinecheck",
};

DEFINE_PER_CPU(struct device *, mce_device);

__cpuinitdata
void (*threshold_cpu_callback)(unsigned long action, unsigned int cpu);

static inline struct mce_bank *attr_to_bank(struct device_attribute *attr)
{
	return container_of(attr, struct mce_bank, attr);
}

static ssize_t show_bank(struct device *s, struct device_attribute *attr,
			 char *buf)
{
	return sprintf(buf, "%llx\n", attr_to_bank(attr)->ctl);
}

static ssize_t set_bank(struct device *s, struct device_attribute *attr,
			const char *buf, size_t size)
{
	u64 new;

	if (strict_strtoull(buf, 0, &new) < 0)
		return -EINVAL;

	attr_to_bank(attr)->ctl = new;
	mce_restart();

	return size;
}

static ssize_t
show_trigger(struct device *s, struct device_attribute *attr, char *buf)
{
	strcpy(buf, mce_helper);
	strcat(buf, "\n");
	return strlen(mce_helper) + 1;
}

static ssize_t set_trigger(struct device *s, struct device_attribute *attr,
				const char *buf, size_t siz)
{
	char *p;

	strncpy(mce_helper, buf, sizeof(mce_helper));
	mce_helper[sizeof(mce_helper)-1] = 0;
	p = strchr(mce_helper, '\n');

	if (p)
		*p = 0;

	return strlen(mce_helper) + !!p;
}

static ssize_t set_ignore_ce(struct device *s,
			     struct device_attribute *attr,
			     const char *buf, size_t size)
{
	u64 new;

	if (strict_strtoull(buf, 0, &new) < 0)
		return -EINVAL;

	if (mce_ignore_ce ^ !!new) {
		if (new) {
			
			mce_timer_delete_all();
			on_each_cpu(mce_disable_cmci, NULL, 1);
			mce_ignore_ce = 1;
		} else {
			
			mce_ignore_ce = 0;
			on_each_cpu(mce_enable_ce, (void *)1, 1);
		}
	}
	return size;
}

static ssize_t set_cmci_disabled(struct device *s,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	u64 new;

	if (strict_strtoull(buf, 0, &new) < 0)
		return -EINVAL;

	if (mce_cmci_disabled ^ !!new) {
		if (new) {
			
			on_each_cpu(mce_disable_cmci, NULL, 1);
			mce_cmci_disabled = 1;
		} else {
			
			mce_cmci_disabled = 0;
			on_each_cpu(mce_enable_ce, NULL, 1);
		}
	}
	return size;
}

static ssize_t store_int_with_restart(struct device *s,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	ssize_t ret = device_store_int(s, attr, buf, size);
	mce_restart();
	return ret;
}

static DEVICE_ATTR(trigger, 0644, show_trigger, set_trigger);
static DEVICE_INT_ATTR(tolerant, 0644, tolerant);
static DEVICE_INT_ATTR(monarch_timeout, 0644, monarch_timeout);
static DEVICE_INT_ATTR(dont_log_ce, 0644, mce_dont_log_ce);

static struct dev_ext_attribute dev_attr_check_interval = {
	__ATTR(check_interval, 0644, device_show_int, store_int_with_restart),
	&check_interval
};

static struct dev_ext_attribute dev_attr_ignore_ce = {
	__ATTR(ignore_ce, 0644, device_show_int, set_ignore_ce),
	&mce_ignore_ce
};

static struct dev_ext_attribute dev_attr_cmci_disabled = {
	__ATTR(cmci_disabled, 0644, device_show_int, set_cmci_disabled),
	&mce_cmci_disabled
};

static struct device_attribute *mce_device_attrs[] = {
	&dev_attr_tolerant.attr,
	&dev_attr_check_interval.attr,
	&dev_attr_trigger,
	&dev_attr_monarch_timeout.attr,
	&dev_attr_dont_log_ce.attr,
	&dev_attr_ignore_ce.attr,
	&dev_attr_cmci_disabled.attr,
	NULL
};

static cpumask_var_t mce_device_initialized;

static void mce_device_release(struct device *dev)
{
	kfree(dev);
}

static __cpuinit int mce_device_create(unsigned int cpu)
{
	struct device *dev;
	int err;
	int i, j;

	if (!mce_available(&boot_cpu_data))
		return -EIO;

	dev = kzalloc(sizeof *dev, GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	dev->id  = cpu;
	dev->bus = &mce_subsys;
	dev->release = &mce_device_release;

	err = device_register(dev);
	if (err)
		return err;

	for (i = 0; mce_device_attrs[i]; i++) {
		err = device_create_file(dev, mce_device_attrs[i]);
		if (err)
			goto error;
	}
	for (j = 0; j < banks; j++) {
		err = device_create_file(dev, &mce_banks[j].attr);
		if (err)
			goto error2;
	}
	cpumask_set_cpu(cpu, mce_device_initialized);
	per_cpu(mce_device, cpu) = dev;

	return 0;
error2:
	while (--j >= 0)
		device_remove_file(dev, &mce_banks[j].attr);
error:
	while (--i >= 0)
		device_remove_file(dev, mce_device_attrs[i]);

	device_unregister(dev);

	return err;
}

static __cpuinit void mce_device_remove(unsigned int cpu)
{
	struct device *dev = per_cpu(mce_device, cpu);
	int i;

	if (!cpumask_test_cpu(cpu, mce_device_initialized))
		return;

	for (i = 0; mce_device_attrs[i]; i++)
		device_remove_file(dev, mce_device_attrs[i]);

	for (i = 0; i < banks; i++)
		device_remove_file(dev, &mce_banks[i].attr);

	device_unregister(dev);
	cpumask_clear_cpu(cpu, mce_device_initialized);
	per_cpu(mce_device, cpu) = NULL;
}

static void __cpuinit mce_disable_cpu(void *h)
{
	unsigned long action = *(unsigned long *)h;
	int i;

	if (!mce_available(__this_cpu_ptr(&cpu_info)))
		return;

	if (!(action & CPU_TASKS_FROZEN))
		cmci_clear();
	for (i = 0; i < banks; i++) {
		struct mce_bank *b = &mce_banks[i];

		if (b->init)
			wrmsrl(MSR_IA32_MCx_CTL(i), 0);
	}
}

static void __cpuinit mce_reenable_cpu(void *h)
{
	unsigned long action = *(unsigned long *)h;
	int i;

	if (!mce_available(__this_cpu_ptr(&cpu_info)))
		return;

	if (!(action & CPU_TASKS_FROZEN))
		cmci_reenable();
	for (i = 0; i < banks; i++) {
		struct mce_bank *b = &mce_banks[i];

		if (b->init)
			wrmsrl(MSR_IA32_MCx_CTL(i), b->ctl);
	}
}

static int __cpuinit
mce_cpu_callback(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	struct timer_list *t = &per_cpu(mce_timer, cpu);

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		mce_device_create(cpu);
		if (threshold_cpu_callback)
			threshold_cpu_callback(action, cpu);
		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		if (threshold_cpu_callback)
			threshold_cpu_callback(action, cpu);
		mce_device_remove(cpu);
		break;
	case CPU_DOWN_PREPARE:
	case CPU_DOWN_PREPARE_FROZEN:
		del_timer_sync(t);
		smp_call_function_single(cpu, mce_disable_cpu, &action, 1);
		break;
	case CPU_DOWN_FAILED:
	case CPU_DOWN_FAILED_FROZEN:
		if (!mce_ignore_ce && check_interval) {
			t->expires = round_jiffies(jiffies +
					   __get_cpu_var(mce_next_interval));
			add_timer_on(t, cpu);
		}
		smp_call_function_single(cpu, mce_reenable_cpu, &action, 1);
		break;
	case CPU_POST_DEAD:
		
		cmci_rediscover(cpu);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block mce_cpu_notifier __cpuinitdata = {
	.notifier_call = mce_cpu_callback,
};

static __init void mce_init_banks(void)
{
	int i;

	for (i = 0; i < banks; i++) {
		struct mce_bank *b = &mce_banks[i];
		struct device_attribute *a = &b->attr;

		sysfs_attr_init(&a->attr);
		a->attr.name	= b->attrname;
		snprintf(b->attrname, ATTR_LEN, "bank%d", i);

		a->attr.mode	= 0644;
		a->show		= show_bank;
		a->store	= set_bank;
	}
}

static __init int mcheck_init_device(void)
{
	int err;
	int i = 0;

	if (!mce_available(&boot_cpu_data))
		return -EIO;

	zalloc_cpumask_var(&mce_device_initialized, GFP_KERNEL);

	mce_init_banks();

	err = subsys_system_register(&mce_subsys, NULL);
	if (err)
		return err;

	for_each_online_cpu(i) {
		err = mce_device_create(i);
		if (err)
			return err;
	}

	register_syscore_ops(&mce_syscore_ops);
	register_hotcpu_notifier(&mce_cpu_notifier);

	
	misc_register(&mce_chrdev_device);

	return err;
}
device_initcall(mcheck_init_device);

static int __init mcheck_disable(char *str)
{
	mce_disabled = 1;
	return 1;
}
__setup("nomce", mcheck_disable);

#ifdef CONFIG_DEBUG_FS
struct dentry *mce_get_debugfs_dir(void)
{
	static struct dentry *dmce;

	if (!dmce)
		dmce = debugfs_create_dir("mce", NULL);

	return dmce;
}

static void mce_reset(void)
{
	cpu_missing = 0;
	atomic_set(&mce_fake_paniced, 0);
	atomic_set(&mce_executing, 0);
	atomic_set(&mce_callin, 0);
	atomic_set(&global_nwo, 0);
}

static int fake_panic_get(void *data, u64 *val)
{
	*val = fake_panic;
	return 0;
}

static int fake_panic_set(void *data, u64 val)
{
	mce_reset();
	fake_panic = val;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fake_panic_fops, fake_panic_get,
			fake_panic_set, "%llu\n");

static int __init mcheck_debugfs_init(void)
{
	struct dentry *dmce, *ffake_panic;

	dmce = mce_get_debugfs_dir();
	if (!dmce)
		return -ENOMEM;
	ffake_panic = debugfs_create_file("fake_panic", 0444, dmce, NULL,
					  &fake_panic_fops);
	if (!ffake_panic)
		return -ENOMEM;

	return 0;
}
late_initcall(mcheck_debugfs_init);
#endif
