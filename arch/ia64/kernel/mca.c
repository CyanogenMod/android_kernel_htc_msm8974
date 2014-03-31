/*
 * File:	mca.c
 * Purpose:	Generic MCA handling layer
 *
 * Copyright (C) 2003 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *
 * Copyright (C) 2002 Dell Inc.
 * Copyright (C) Matt Domsch <Matt_Domsch@dell.com>
 *
 * Copyright (C) 2002 Intel
 * Copyright (C) Jenna Hall <jenna.s.hall@intel.com>
 *
 * Copyright (C) 2001 Intel
 * Copyright (C) Fred Lewis <frederick.v.lewis@intel.com>
 *
 * Copyright (C) 2000 Intel
 * Copyright (C) Chuck Fleckenstein <cfleck@co.intel.com>
 *
 * Copyright (C) 1999, 2004-2008 Silicon Graphics, Inc.
 * Copyright (C) Vijay Chander <vijay@engr.sgi.com>
 *
 * Copyright (C) 2006 FUJITSU LIMITED
 * Copyright (C) Hidetoshi Seto <seto.hidetoshi@jp.fujitsu.com>
 *
 * 2000-03-29 Chuck Fleckenstein <cfleck@co.intel.com>
 *	      Fixed PAL/SAL update issues, began MCA bug fixes, logging issues,
 *	      added min save state dump, added INIT handler.
 *
 * 2001-01-03 Fred Lewis <frederick.v.lewis@intel.com>
 *	      Added setup of CMCI and CPEI IRQs, logging of corrected platform
 *	      errors, completed code for logging of corrected & uncorrected
 *	      machine check errors, and updated for conformance with Nov. 2000
 *	      revision of the SAL 3.0 spec.
 *
 * 2002-01-04 Jenna Hall <jenna.s.hall@intel.com>
 *	      Aligned MCA stack to 16 bytes, added platform vs. CPU error flag,
 *	      set SAL default return values, changed error record structure to
 *	      linked list, added init call to sal_get_state_info_size().
 *
 * 2002-03-25 Matt Domsch <Matt_Domsch@dell.com>
 *	      GUID cleanups.
 *
 * 2003-04-15 David Mosberger-Tang <davidm@hpl.hp.com>
 *	      Added INIT backtrace support.
 *
 * 2003-12-08 Keith Owens <kaos@sgi.com>
 *	      smp_call_function() must not be called from interrupt context
 *	      (can deadlock on tasklist_lock).
 *	      Use keventd to call smp_call_function().
 *
 * 2004-02-01 Keith Owens <kaos@sgi.com>
 *	      Avoid deadlock when using printk() for MCA and INIT records.
 *	      Delete all record printing code, moved to salinfo_decode in user
 *	      space.  Mark variables and functions static where possible.
 *	      Delete dead variables and functions.  Reorder to remove the need
 *	      for forward declarations and to consolidate related code.
 *
 * 2005-08-12 Keith Owens <kaos@sgi.com>
 *	      Convert MCA/INIT handlers to use per event stacks and SAL/OS
 *	      state.
 *
 * 2005-10-07 Keith Owens <kaos@sgi.com>
 *	      Add notify_die() hooks.
 *
 * 2006-09-15 Hidetoshi Seto <seto.hidetoshi@jp.fujitsu.com>
 *	      Add printing support for MCA/INIT.
 *
 * 2007-04-27 Russ Anderson <rja@sgi.com>
 *	      Support multiple cpus going through OS_MCA in the same event.
 */
#include <linux/jiffies.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/bootmem.h>
#include <linux/acpi.h>
#include <linux/timer.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/smp.h>
#include <linux/workqueue.h>
#include <linux/cpumask.h>
#include <linux/kdebug.h>
#include <linux/cpu.h>
#include <linux/gfp.h>

#include <asm/delay.h>
#include <asm/machvec.h>
#include <asm/meminit.h>
#include <asm/page.h>
#include <asm/ptrace.h>
#include <asm/sal.h>
#include <asm/mca.h>
#include <asm/kexec.h>

#include <asm/irq.h>
#include <asm/hw_irq.h>
#include <asm/tlb.h>

#include "mca_drv.h"
#include "entry.h"

#if defined(IA64_MCA_DEBUG_INFO)
# define IA64_MCA_DEBUG(fmt...)	printk(fmt)
#else
# define IA64_MCA_DEBUG(fmt...)
#endif

#define NOTIFY_INIT(event, regs, arg, spin)				\
do {									\
	if ((notify_die((event), "INIT", (regs), (arg), 0, 0)		\
			== NOTIFY_STOP) && ((spin) == 1))		\
		ia64_mca_spin(__func__);				\
} while (0)

#define NOTIFY_MCA(event, regs, arg, spin)				\
do {									\
	if ((notify_die((event), "MCA", (regs), (arg), 0, 0)		\
			== NOTIFY_STOP) && ((spin) == 1))		\
		ia64_mca_spin(__func__);				\
} while (0)

DEFINE_PER_CPU(u64, ia64_mca_data); 
DEFINE_PER_CPU(u64, ia64_mca_per_cpu_pte); 
DEFINE_PER_CPU(u64, ia64_mca_pal_pte);	    
DEFINE_PER_CPU(u64, ia64_mca_pal_base);    
DEFINE_PER_CPU(u64, ia64_mca_tr_reload);   

unsigned long __per_cpu_mca[NR_CPUS];

extern void			ia64_os_init_dispatch_monarch (void);
extern void			ia64_os_init_dispatch_slave (void);

static int monarch_cpu = -1;

static ia64_mc_info_t		ia64_mc_info;

#define MAX_CPE_POLL_INTERVAL (15*60*HZ) 
#define MIN_CPE_POLL_INTERVAL (2*60*HZ)  
#define CMC_POLL_INTERVAL     (1*60*HZ)  
#define CPE_HISTORY_LENGTH    5
#define CMC_HISTORY_LENGTH    5

#ifdef CONFIG_ACPI
static struct timer_list cpe_poll_timer;
#endif
static struct timer_list cmc_poll_timer;
static int cmc_polling_enabled = 1;

static int cpe_poll_enabled = 1;

extern void salinfo_log_wakeup(int type, u8 *buffer, u64 size, int irqsafe);

static int mca_init __initdata;


#define mprintk(fmt...) ia64_mca_printk(fmt)

#define MLOGBUF_SIZE (512+256*NR_CPUS)
#define MLOGBUF_MSGMAX 256
static char mlogbuf[MLOGBUF_SIZE];
static DEFINE_SPINLOCK(mlogbuf_wlock);	
static DEFINE_SPINLOCK(mlogbuf_rlock);	
static unsigned long mlogbuf_start;
static unsigned long mlogbuf_end;
static unsigned int mlogbuf_finished = 0;
static unsigned long mlogbuf_timestamp = 0;

static int loglevel_save = -1;
#define BREAK_LOGLEVEL(__console_loglevel)		\
	oops_in_progress = 1;				\
	if (loglevel_save < 0)				\
		loglevel_save = __console_loglevel;	\
	__console_loglevel = 15;

#define RESTORE_LOGLEVEL(__console_loglevel)		\
	if (loglevel_save >= 0) {			\
		__console_loglevel = loglevel_save;	\
		loglevel_save = -1;			\
	}						\
	mlogbuf_finished = 0;				\
	oops_in_progress = 0;

void ia64_mca_printk(const char *fmt, ...)
{
	va_list args;
	int printed_len;
	char temp_buf[MLOGBUF_MSGMAX];
	char *p;

	va_start(args, fmt);
	printed_len = vscnprintf(temp_buf, sizeof(temp_buf), fmt, args);
	va_end(args);

	
	if (oops_in_progress) {
		
		printk(temp_buf);
	} else {
		spin_lock(&mlogbuf_wlock);
		for (p = temp_buf; *p; p++) {
			unsigned long next = (mlogbuf_end + 1) % MLOGBUF_SIZE;
			if (next != mlogbuf_start) {
				mlogbuf[mlogbuf_end] = *p;
				mlogbuf_end = next;
			} else {
				
				break;
			}
		}
		mlogbuf[mlogbuf_end] = '\0';
		spin_unlock(&mlogbuf_wlock);
	}
}
EXPORT_SYMBOL(ia64_mca_printk);

void ia64_mlogbuf_dump(void)
{
	char temp_buf[MLOGBUF_MSGMAX];
	char *p;
	unsigned long index;
	unsigned long flags;
	unsigned int printed_len;

	
	while (mlogbuf_start != mlogbuf_end) {
		temp_buf[0] = '\0';
		p = temp_buf;
		printed_len = 0;

		spin_lock_irqsave(&mlogbuf_rlock, flags);

		index = mlogbuf_start;
		while (index != mlogbuf_end) {
			*p = mlogbuf[index];
			index = (index + 1) % MLOGBUF_SIZE;
			if (!*p)
				break;
			p++;
			if (++printed_len >= MLOGBUF_MSGMAX - 1)
				break;
		}
		*p = '\0';
		if (temp_buf[0])
			printk(temp_buf);
		mlogbuf_start = index;

		mlogbuf_timestamp = 0;
		spin_unlock_irqrestore(&mlogbuf_rlock, flags);
	}
}
EXPORT_SYMBOL(ia64_mlogbuf_dump);

static void ia64_mlogbuf_finish(int wait)
{
	BREAK_LOGLEVEL(console_loglevel);

	spin_lock_init(&mlogbuf_rlock);
	ia64_mlogbuf_dump();
	printk(KERN_EMERG "mlogbuf_finish: printing switched to urgent mode, "
		"MCA/INIT might be dodgy or fail.\n");

	if (!wait)
		return;

	
	printk("Delaying for 5 seconds...\n");
	udelay(5*1000000);

	mlogbuf_finished = 1;
}

static void ia64_mlogbuf_dump_from_init(void)
{
	if (mlogbuf_finished)
		return;

	if (mlogbuf_timestamp &&
			time_before(jiffies, mlogbuf_timestamp + 30 * HZ)) {
		printk(KERN_ERR "INIT: mlogbuf_dump is interrupted by INIT "
			" and the system seems to be messed up.\n");
		ia64_mlogbuf_finish(0);
		return;
	}

	if (!spin_trylock(&mlogbuf_rlock)) {
		printk(KERN_ERR "INIT: mlogbuf_dump is interrupted by INIT. "
			"Generated messages other than stack dump will be "
			"buffered to mlogbuf and will be printed later.\n");
		printk(KERN_ERR "INIT: If messages would not printed after "
			"this INIT, wait 30sec and assert INIT again.\n");
		if (!mlogbuf_timestamp)
			mlogbuf_timestamp = jiffies;
		return;
	}
	spin_unlock(&mlogbuf_rlock);
	ia64_mlogbuf_dump();
}

static void inline
ia64_mca_spin(const char *func)
{
	if (monarch_cpu == smp_processor_id())
		ia64_mlogbuf_finish(0);
	mprintk(KERN_EMERG "%s: spinning here, not returning to SAL\n", func);
	while (1)
		cpu_relax();
}
#define IA64_MAX_LOGS		2	
#define IA64_MAX_LOG_TYPES      4   

typedef struct ia64_state_log_s
{
	spinlock_t	isl_lock;
	int		isl_index;
	unsigned long	isl_count;
	ia64_err_rec_t  *isl_log[IA64_MAX_LOGS]; 
} ia64_state_log_t;

static ia64_state_log_t ia64_state_log[IA64_MAX_LOG_TYPES];

#define IA64_LOG_ALLOCATE(it, size) \
	{ia64_state_log[it].isl_log[IA64_LOG_CURR_INDEX(it)] = \
		(ia64_err_rec_t *)alloc_bootmem(size); \
	ia64_state_log[it].isl_log[IA64_LOG_NEXT_INDEX(it)] = \
		(ia64_err_rec_t *)alloc_bootmem(size);}
#define IA64_LOG_LOCK_INIT(it) spin_lock_init(&ia64_state_log[it].isl_lock)
#define IA64_LOG_LOCK(it)      spin_lock_irqsave(&ia64_state_log[it].isl_lock, s)
#define IA64_LOG_UNLOCK(it)    spin_unlock_irqrestore(&ia64_state_log[it].isl_lock,s)
#define IA64_LOG_NEXT_INDEX(it)    ia64_state_log[it].isl_index
#define IA64_LOG_CURR_INDEX(it)    1 - ia64_state_log[it].isl_index
#define IA64_LOG_INDEX_INC(it) \
    {ia64_state_log[it].isl_index = 1 - ia64_state_log[it].isl_index; \
    ia64_state_log[it].isl_count++;}
#define IA64_LOG_INDEX_DEC(it) \
    ia64_state_log[it].isl_index = 1 - ia64_state_log[it].isl_index
#define IA64_LOG_NEXT_BUFFER(it)   (void *)((ia64_state_log[it].isl_log[IA64_LOG_NEXT_INDEX(it)]))
#define IA64_LOG_CURR_BUFFER(it)   (void *)((ia64_state_log[it].isl_log[IA64_LOG_CURR_INDEX(it)]))
#define IA64_LOG_COUNT(it)         ia64_state_log[it].isl_count

static void __init
ia64_log_init(int sal_info_type)
{
	u64	max_size = 0;

	IA64_LOG_NEXT_INDEX(sal_info_type) = 0;
	IA64_LOG_LOCK_INIT(sal_info_type);

	
	max_size = ia64_sal_get_state_info_size(sal_info_type);
	if (!max_size)
		
		return;

	
	IA64_LOG_ALLOCATE(sal_info_type, max_size);
	memset(IA64_LOG_CURR_BUFFER(sal_info_type), 0, max_size);
	memset(IA64_LOG_NEXT_BUFFER(sal_info_type), 0, max_size);
}

static u64
ia64_log_get(int sal_info_type, u8 **buffer, int irq_safe)
{
	sal_log_record_header_t     *log_buffer;
	u64                         total_len = 0;
	unsigned long               s;

	IA64_LOG_LOCK(sal_info_type);

	
	log_buffer = IA64_LOG_NEXT_BUFFER(sal_info_type);

	total_len = ia64_sal_get_state_info(sal_info_type, (u64 *)log_buffer);

	if (total_len) {
		IA64_LOG_INDEX_INC(sal_info_type);
		IA64_LOG_UNLOCK(sal_info_type);
		if (irq_safe) {
			IA64_MCA_DEBUG("%s: SAL error record type %d retrieved. Record length = %ld\n",
				       __func__, sal_info_type, total_len);
		}
		*buffer = (u8 *) log_buffer;
		return total_len;
	} else {
		IA64_LOG_UNLOCK(sal_info_type);
		return 0;
	}
}

static void
ia64_mca_log_sal_error_record(int sal_info_type)
{
	u8 *buffer;
	sal_log_record_header_t *rh;
	u64 size;
	int irq_safe = sal_info_type != SAL_INFO_TYPE_MCA;
#ifdef IA64_MCA_DEBUG_INFO
	static const char * const rec_name[] = { "MCA", "INIT", "CMC", "CPE" };
#endif

	size = ia64_log_get(sal_info_type, &buffer, irq_safe);
	if (!size)
		return;

	salinfo_log_wakeup(sal_info_type, buffer, size, irq_safe);

	if (irq_safe)
		IA64_MCA_DEBUG("CPU %d: SAL log contains %s error record\n",
			smp_processor_id(),
			sal_info_type < ARRAY_SIZE(rec_name) ? rec_name[sal_info_type] : "UNKNOWN");

	
	rh = (sal_log_record_header_t *)buffer;
	if (rh->severity == sal_log_severity_corrected)
		ia64_sal_clear_state_info(sal_info_type);
}

int
search_mca_table (const struct mca_table_entry *first,
                const struct mca_table_entry *last,
                unsigned long ip)
{
        const struct mca_table_entry *curr;
        u64 curr_start, curr_end;

        curr = first;
        while (curr <= last) {
                curr_start = (u64) &curr->start_addr + curr->start_addr;
                curr_end = (u64) &curr->end_addr + curr->end_addr;

                if ((ip >= curr_start) && (ip <= curr_end)) {
                        return 1;
                }
                curr++;
        }
        return 0;
}

int mca_recover_range(unsigned long addr)
{
	extern struct mca_table_entry __start___mca_table[];
	extern struct mca_table_entry __stop___mca_table[];

	return search_mca_table(__start___mca_table, __stop___mca_table-1, addr);
}
EXPORT_SYMBOL_GPL(mca_recover_range);

#ifdef CONFIG_ACPI

int cpe_vector = -1;
int ia64_cpe_irq = -1;

static irqreturn_t
ia64_mca_cpe_int_handler (int cpe_irq, void *arg)
{
	static unsigned long	cpe_history[CPE_HISTORY_LENGTH];
	static int		index;
	static DEFINE_SPINLOCK(cpe_history_lock);

	IA64_MCA_DEBUG("%s: received interrupt vector = %#x on CPU %d\n",
		       __func__, cpe_irq, smp_processor_id());

	
	local_irq_enable();

	spin_lock(&cpe_history_lock);
	if (!cpe_poll_enabled && cpe_vector >= 0) {

		int i, count = 1; 
		unsigned long now = jiffies;

		for (i = 0; i < CPE_HISTORY_LENGTH; i++) {
			if (now - cpe_history[i] <= HZ)
				count++;
		}

		IA64_MCA_DEBUG(KERN_INFO "CPE threshold %d/%d\n", count, CPE_HISTORY_LENGTH);
		if (count >= CPE_HISTORY_LENGTH) {

			cpe_poll_enabled = 1;
			spin_unlock(&cpe_history_lock);
			disable_irq_nosync(local_vector_to_irq(IA64_CPE_VECTOR));

			printk(KERN_WARNING "WARNING: Switching to polling CPE handler; error records may be lost\n");

			mod_timer(&cpe_poll_timer, jiffies + MIN_CPE_POLL_INTERVAL);

			
			goto out;
		} else {
			cpe_history[index++] = now;
			if (index == CPE_HISTORY_LENGTH)
				index = 0;
		}
	}
	spin_unlock(&cpe_history_lock);
out:
	
	ia64_mca_log_sal_error_record(SAL_INFO_TYPE_CPE);

	local_irq_disable();

	return IRQ_HANDLED;
}

#endif 

#ifdef CONFIG_ACPI
void
ia64_mca_register_cpev (int cpev)
{
	
	struct ia64_sal_retval isrv;

	isrv = ia64_sal_mc_set_params(SAL_MC_PARAM_CPE_INT, SAL_MC_PARAM_MECHANISM_INT, cpev, 0, 0);
	if (isrv.status) {
		printk(KERN_ERR "Failed to register Corrected Platform "
		       "Error interrupt vector with SAL (status %ld)\n", isrv.status);
		return;
	}

	IA64_MCA_DEBUG("%s: corrected platform error "
		       "vector %#x registered\n", __func__, cpev);
}
#endif 

void __cpuinit
ia64_mca_cmc_vector_setup (void)
{
	cmcv_reg_t	cmcv;

	cmcv.cmcv_regval	= 0;
	cmcv.cmcv_mask		= 1;        
	cmcv.cmcv_vector	= IA64_CMC_VECTOR;
	ia64_setreg(_IA64_REG_CR_CMCV, cmcv.cmcv_regval);

	IA64_MCA_DEBUG("%s: CPU %d corrected machine check vector %#x registered.\n",
		       __func__, smp_processor_id(), IA64_CMC_VECTOR);

	IA64_MCA_DEBUG("%s: CPU %d CMCV = %#016lx\n",
		       __func__, smp_processor_id(), ia64_getreg(_IA64_REG_CR_CMCV));
}

static void
ia64_mca_cmc_vector_disable (void *dummy)
{
	cmcv_reg_t	cmcv;

	cmcv.cmcv_regval = ia64_getreg(_IA64_REG_CR_CMCV);

	cmcv.cmcv_mask = 1; 
	ia64_setreg(_IA64_REG_CR_CMCV, cmcv.cmcv_regval);

	IA64_MCA_DEBUG("%s: CPU %d corrected machine check vector %#x disabled.\n",
		       __func__, smp_processor_id(), cmcv.cmcv_vector);
}

static void
ia64_mca_cmc_vector_enable (void *dummy)
{
	cmcv_reg_t	cmcv;

	cmcv.cmcv_regval = ia64_getreg(_IA64_REG_CR_CMCV);

	cmcv.cmcv_mask = 0; 
	ia64_setreg(_IA64_REG_CR_CMCV, cmcv.cmcv_regval);

	IA64_MCA_DEBUG("%s: CPU %d corrected machine check vector %#x enabled.\n",
		       __func__, smp_processor_id(), cmcv.cmcv_vector);
}

static void
ia64_mca_cmc_vector_disable_keventd(struct work_struct *unused)
{
	on_each_cpu(ia64_mca_cmc_vector_disable, NULL, 0);
}

static void
ia64_mca_cmc_vector_enable_keventd(struct work_struct *unused)
{
	on_each_cpu(ia64_mca_cmc_vector_enable, NULL, 0);
}

static void
ia64_mca_wakeup(int cpu)
{
	platform_send_ipi(cpu, IA64_MCA_WAKEUP_VECTOR, IA64_IPI_DM_INT, 0);
}

static void
ia64_mca_wakeup_all(void)
{
	int cpu;

	
	for_each_online_cpu(cpu) {
		if (ia64_mc_info.imi_rendez_checkin[cpu] == IA64_MCA_RENDEZ_CHECKIN_DONE)
			ia64_mca_wakeup(cpu);
	}

}

static irqreturn_t
ia64_mca_rendez_int_handler(int rendez_irq, void *arg)
{
	unsigned long flags;
	int cpu = smp_processor_id();
	struct ia64_mca_notify_die nd =
		{ .sos = NULL, .monarch_cpu = &monarch_cpu };

	
	local_irq_save(flags);

	NOTIFY_MCA(DIE_MCA_RENDZVOUS_ENTER, get_irq_regs(), (long)&nd, 1);

	ia64_mc_info.imi_rendez_checkin[cpu] = IA64_MCA_RENDEZ_CHECKIN_DONE;
	ia64_sal_mc_rendez();

	NOTIFY_MCA(DIE_MCA_RENDZVOUS_PROCESS, get_irq_regs(), (long)&nd, 1);

	
	while (monarch_cpu != -1)
	       cpu_relax();	

	NOTIFY_MCA(DIE_MCA_RENDZVOUS_LEAVE, get_irq_regs(), (long)&nd, 1);

	ia64_mc_info.imi_rendez_checkin[cpu] = IA64_MCA_RENDEZ_CHECKIN_NOTDONE;
	
	local_irq_restore(flags);
	return IRQ_HANDLED;
}

static irqreturn_t
ia64_mca_wakeup_int_handler(int wakeup_irq, void *arg)
{
	return IRQ_HANDLED;
}

int (*ia64_mca_ucmc_extension)
	(void*,struct ia64_sal_os_state*)
	= NULL;

int
ia64_reg_MCA_extension(int (*fn)(void *, struct ia64_sal_os_state *))
{
	if (ia64_mca_ucmc_extension)
		return 1;

	ia64_mca_ucmc_extension = fn;
	return 0;
}

void
ia64_unreg_MCA_extension(void)
{
	if (ia64_mca_ucmc_extension)
		ia64_mca_ucmc_extension = NULL;
}

EXPORT_SYMBOL(ia64_reg_MCA_extension);
EXPORT_SYMBOL(ia64_unreg_MCA_extension);


static inline void
copy_reg(const u64 *fr, u64 fnat, unsigned long *tr, unsigned long *tnat)
{
	u64 fslot, tslot, nat;
	*tr = *fr;
	fslot = ((unsigned long)fr >> 3) & 63;
	tslot = ((unsigned long)tr >> 3) & 63;
	*tnat &= ~(1UL << tslot);
	nat = (fnat >> fslot) & 1;
	*tnat |= (nat << tslot);
}


static void
ia64_mca_modify_comm(const struct task_struct *previous_current)
{
	char *p, comm[sizeof(current->comm)];
	if (previous_current->pid)
		snprintf(comm, sizeof(comm), "%s %d",
			current->comm, previous_current->pid);
	else {
		int l;
		if ((p = strchr(previous_current->comm, ' ')))
			l = p - previous_current->comm;
		else
			l = strlen(previous_current->comm);
		snprintf(comm, sizeof(comm), "%s %*s %d",
			current->comm, l, previous_current->comm,
			task_thread_info(previous_current)->cpu);
	}
	memcpy(current->comm, comm, sizeof(current->comm));
}

static void
finish_pt_regs(struct pt_regs *regs, struct ia64_sal_os_state *sos,
		unsigned long *nat)
{
	const pal_min_state_area_t *ms = sos->pal_min_state;
	const u64 *bank;

	if (ia64_psr(regs)->ic) {
		regs->cr_iip = ms->pmsa_iip;
		regs->cr_ipsr = ms->pmsa_ipsr;
		regs->cr_ifs = ms->pmsa_ifs;
	} else {
		regs->cr_iip = ms->pmsa_xip;
		regs->cr_ipsr = ms->pmsa_xpsr;
		regs->cr_ifs = ms->pmsa_xfs;

		sos->iip = ms->pmsa_iip;
		sos->ipsr = ms->pmsa_ipsr;
		sos->ifs = ms->pmsa_ifs;
	}
	regs->pr = ms->pmsa_pr;
	regs->b0 = ms->pmsa_br0;
	regs->ar_rsc = ms->pmsa_rsc;
	copy_reg(&ms->pmsa_gr[1-1], ms->pmsa_nat_bits, &regs->r1, nat);
	copy_reg(&ms->pmsa_gr[2-1], ms->pmsa_nat_bits, &regs->r2, nat);
	copy_reg(&ms->pmsa_gr[3-1], ms->pmsa_nat_bits, &regs->r3, nat);
	copy_reg(&ms->pmsa_gr[8-1], ms->pmsa_nat_bits, &regs->r8, nat);
	copy_reg(&ms->pmsa_gr[9-1], ms->pmsa_nat_bits, &regs->r9, nat);
	copy_reg(&ms->pmsa_gr[10-1], ms->pmsa_nat_bits, &regs->r10, nat);
	copy_reg(&ms->pmsa_gr[11-1], ms->pmsa_nat_bits, &regs->r11, nat);
	copy_reg(&ms->pmsa_gr[12-1], ms->pmsa_nat_bits, &regs->r12, nat);
	copy_reg(&ms->pmsa_gr[13-1], ms->pmsa_nat_bits, &regs->r13, nat);
	copy_reg(&ms->pmsa_gr[14-1], ms->pmsa_nat_bits, &regs->r14, nat);
	copy_reg(&ms->pmsa_gr[15-1], ms->pmsa_nat_bits, &regs->r15, nat);
	if (ia64_psr(regs)->bn)
		bank = ms->pmsa_bank1_gr;
	else
		bank = ms->pmsa_bank0_gr;
	copy_reg(&bank[16-16], ms->pmsa_nat_bits, &regs->r16, nat);
	copy_reg(&bank[17-16], ms->pmsa_nat_bits, &regs->r17, nat);
	copy_reg(&bank[18-16], ms->pmsa_nat_bits, &regs->r18, nat);
	copy_reg(&bank[19-16], ms->pmsa_nat_bits, &regs->r19, nat);
	copy_reg(&bank[20-16], ms->pmsa_nat_bits, &regs->r20, nat);
	copy_reg(&bank[21-16], ms->pmsa_nat_bits, &regs->r21, nat);
	copy_reg(&bank[22-16], ms->pmsa_nat_bits, &regs->r22, nat);
	copy_reg(&bank[23-16], ms->pmsa_nat_bits, &regs->r23, nat);
	copy_reg(&bank[24-16], ms->pmsa_nat_bits, &regs->r24, nat);
	copy_reg(&bank[25-16], ms->pmsa_nat_bits, &regs->r25, nat);
	copy_reg(&bank[26-16], ms->pmsa_nat_bits, &regs->r26, nat);
	copy_reg(&bank[27-16], ms->pmsa_nat_bits, &regs->r27, nat);
	copy_reg(&bank[28-16], ms->pmsa_nat_bits, &regs->r28, nat);
	copy_reg(&bank[29-16], ms->pmsa_nat_bits, &regs->r29, nat);
	copy_reg(&bank[30-16], ms->pmsa_nat_bits, &regs->r30, nat);
	copy_reg(&bank[31-16], ms->pmsa_nat_bits, &regs->r31, nat);
}


static struct task_struct *
ia64_mca_modify_original_stack(struct pt_regs *regs,
		const struct switch_stack *sw,
		struct ia64_sal_os_state *sos,
		const char *type)
{
	char *p;
	ia64_va va;
	extern char ia64_leave_kernel[];	
	const pal_min_state_area_t *ms = sos->pal_min_state;
	struct task_struct *previous_current;
	struct pt_regs *old_regs;
	struct switch_stack *old_sw;
	unsigned size = sizeof(struct pt_regs) +
			sizeof(struct switch_stack) + 16;
	unsigned long *old_bspstore, *old_bsp;
	unsigned long *new_bspstore, *new_bsp;
	unsigned long old_unat, old_rnat, new_rnat, nat;
	u64 slots, loadrs = regs->loadrs;
	u64 r12 = ms->pmsa_gr[12-1], r13 = ms->pmsa_gr[13-1];
	u64 ar_bspstore = regs->ar_bspstore;
	u64 ar_bsp = regs->ar_bspstore + (loadrs >> 16);
	const char *msg;
	int cpu = smp_processor_id();

	previous_current = curr_task(cpu);
	set_curr_task(cpu, current);
	if ((p = strchr(current->comm, ' ')))
		*p = '\0';

	regs->cr_ipsr = ms->pmsa_ipsr;
	if (ia64_psr(regs)->dt == 0) {
		va.l = r12;
		if (va.f.reg == 0) {
			va.f.reg = 7;
			r12 = va.l;
		}
		va.l = r13;
		if (va.f.reg == 0) {
			va.f.reg = 7;
			r13 = va.l;
		}
	}
	if (ia64_psr(regs)->rt == 0) {
		va.l = ar_bspstore;
		if (va.f.reg == 0) {
			va.f.reg = 7;
			ar_bspstore = va.l;
		}
		va.l = ar_bsp;
		if (va.f.reg == 0) {
			va.f.reg = 7;
			ar_bsp = va.l;
		}
	}

	old_bspstore = (unsigned long *)ar_bspstore;
	old_bsp = (unsigned long *)ar_bsp;
	slots = ia64_rse_num_regs(old_bspstore, old_bsp);
	new_bspstore = (unsigned long *)((u64)current + IA64_RBS_OFFSET);
	new_bsp = ia64_rse_skip_regs(new_bspstore, slots);
	regs->loadrs = (new_bsp - new_bspstore) * 8 << 16;

	
	if (user_mode(regs)) {
		msg = "occurred in user space";
		ia64_mca_modify_comm(previous_current);
		goto no_mod;
	}

	if (r13 != sos->prev_IA64_KR_CURRENT) {
		msg = "inconsistent previous current and r13";
		goto no_mod;
	}

	if (!mca_recover_range(ms->pmsa_iip)) {
		if ((r12 - r13) >= KERNEL_STACK_SIZE) {
			msg = "inconsistent r12 and r13";
			goto no_mod;
		}
		if ((ar_bspstore - r13) >= KERNEL_STACK_SIZE) {
			msg = "inconsistent ar.bspstore and r13";
			goto no_mod;
		}
		va.p = old_bspstore;
		if (va.f.reg < 5) {
			msg = "old_bspstore is in the wrong region";
			goto no_mod;
		}
		if ((ar_bsp - r13) >= KERNEL_STACK_SIZE) {
			msg = "inconsistent ar.bsp and r13";
			goto no_mod;
		}
		size += (ia64_rse_skip_regs(old_bspstore, slots) - old_bspstore) * 8;
		if (ar_bspstore + size > r12) {
			msg = "no room for blocked state";
			goto no_mod;
		}
	}

	ia64_mca_modify_comm(previous_current);

	p = (char *)r12 - sizeof(*regs);
	old_regs = (struct pt_regs *)p;
	memcpy(old_regs, regs, sizeof(*regs));
	old_regs->loadrs = loadrs;
	old_unat = old_regs->ar_unat;
	finish_pt_regs(old_regs, sos, &old_unat);

	p -= sizeof(struct switch_stack);
	old_sw = (struct switch_stack *)p;
	memcpy(old_sw, sw, sizeof(*sw));
	old_sw->caller_unat = old_unat;
	old_sw->ar_fpsr = old_regs->ar_fpsr;
	copy_reg(&ms->pmsa_gr[4-1], ms->pmsa_nat_bits, &old_sw->r4, &old_unat);
	copy_reg(&ms->pmsa_gr[5-1], ms->pmsa_nat_bits, &old_sw->r5, &old_unat);
	copy_reg(&ms->pmsa_gr[6-1], ms->pmsa_nat_bits, &old_sw->r6, &old_unat);
	copy_reg(&ms->pmsa_gr[7-1], ms->pmsa_nat_bits, &old_sw->r7, &old_unat);
	old_sw->b0 = (u64)ia64_leave_kernel;
	old_sw->b1 = ms->pmsa_br1;
	old_sw->ar_pfs = 0;
	old_sw->ar_unat = old_unat;
	old_sw->pr = old_regs->pr | (1UL << PRED_NON_SYSCALL);
	previous_current->thread.ksp = (u64)p - 16;

	/* Finally copy the original stack's registers back to its RBS.
	 * Registers from ar.bspstore through ar.bsp at the time of the event
	 * are in the current RBS, copy them back to the original stack.  The
	 * copy must be done register by register because the original bspstore
	 * and the current one have different alignments, so the saved RNAT
	 * data occurs at different places.
	 *
	 * mca_asm does cover, so the old_bsp already includes all registers at
	 * the time of MCA/INIT.  It also does flushrs, so all registers before
	 * this function have been written to backing store on the MCA/INIT
	 * stack.
	 */
	new_rnat = ia64_get_rnat(ia64_rse_rnat_addr(new_bspstore));
	old_rnat = regs->ar_rnat;
	while (slots--) {
		if (ia64_rse_is_rnat_slot(new_bspstore)) {
			new_rnat = ia64_get_rnat(new_bspstore++);
		}
		if (ia64_rse_is_rnat_slot(old_bspstore)) {
			*old_bspstore++ = old_rnat;
			old_rnat = 0;
		}
		nat = (new_rnat >> ia64_rse_slot_num(new_bspstore)) & 1UL;
		old_rnat &= ~(1UL << ia64_rse_slot_num(old_bspstore));
		old_rnat |= (nat << ia64_rse_slot_num(old_bspstore));
		*old_bspstore++ = *new_bspstore++;
	}
	old_sw->ar_bspstore = (unsigned long)old_bspstore;
	old_sw->ar_rnat = old_rnat;

	sos->prev_task = previous_current;
	return previous_current;

no_mod:
	mprintk(KERN_INFO "cpu %d, %s %s, original stack not modified\n",
			smp_processor_id(), type, msg);
	old_unat = regs->ar_unat;
	finish_pt_regs(regs, sos, &old_unat);
	return previous_current;
}


static void
ia64_wait_for_slaves(int monarch, const char *type)
{
	int c, i , wait;

	for (i = 0; i < 5000; i++) {
		wait = 0;
		for_each_online_cpu(c) {
			if (c == monarch)
				continue;
			if (ia64_mc_info.imi_rendez_checkin[c]
					== IA64_MCA_RENDEZ_CHECKIN_NOTDONE) {
				udelay(1000);		
				wait = 1;
				break;
			}
		}
		if (!wait)
			goto all_in;
	}

	ia64_mlogbuf_finish(0);
	mprintk(KERN_INFO "OS %s slave did not rendezvous on cpu", type);
	for_each_online_cpu(c) {
		if (c == monarch)
			continue;
		if (ia64_mc_info.imi_rendez_checkin[c] == IA64_MCA_RENDEZ_CHECKIN_NOTDONE)
			mprintk(" %d", c);
	}
	mprintk("\n");
	return;

all_in:
	mprintk(KERN_INFO "All OS %s slaves have reached rendezvous\n", type);
	return;
}

static void mca_insert_tr(u64 iord)
{

	int i;
	u64 old_rr;
	struct ia64_tr_entry *p;
	unsigned long psr;
	int cpu = smp_processor_id();

	if (!ia64_idtrs[cpu])
		return;

	psr = ia64_clear_ic();
	for (i = IA64_TR_ALLOC_BASE; i < IA64_TR_ALLOC_MAX; i++) {
		p = ia64_idtrs[cpu] + (iord - 1) * IA64_TR_ALLOC_MAX;
		if (p->pte & 0x1) {
			old_rr = ia64_get_rr(p->ifa);
			if (old_rr != p->rr) {
				ia64_set_rr(p->ifa, p->rr);
				ia64_srlz_d();
			}
			ia64_ptr(iord, p->ifa, p->itir >> 2);
			ia64_srlz_i();
			if (iord & 0x1) {
				ia64_itr(0x1, i, p->ifa, p->pte, p->itir >> 2);
				ia64_srlz_i();
			}
			if (iord & 0x2) {
				ia64_itr(0x2, i, p->ifa, p->pte, p->itir >> 2);
				ia64_srlz_i();
			}
			if (old_rr != p->rr) {
				ia64_set_rr(p->ifa, old_rr);
				ia64_srlz_d();
			}
		}
	}
	ia64_set_psr(psr);
}

void
ia64_mca_handler(struct pt_regs *regs, struct switch_stack *sw,
		 struct ia64_sal_os_state *sos)
{
	int recover, cpu = smp_processor_id();
	struct task_struct *previous_current;
	struct ia64_mca_notify_die nd =
		{ .sos = sos, .monarch_cpu = &monarch_cpu, .data = &recover };
	static atomic_t mca_count;
	static cpumask_t mca_cpu;

	if (atomic_add_return(1, &mca_count) == 1) {
		monarch_cpu = cpu;
		sos->monarch = 1;
	} else {
		cpu_set(cpu, mca_cpu);
		sos->monarch = 0;
	}
	mprintk(KERN_INFO "Entered OS MCA handler. PSP=%lx cpu=%d "
		"monarch=%ld\n", sos->proc_state_param, cpu, sos->monarch);

	previous_current = ia64_mca_modify_original_stack(regs, sw, sos, "MCA");

	NOTIFY_MCA(DIE_MCA_MONARCH_ENTER, regs, (long)&nd, 1);

	ia64_mc_info.imi_rendez_checkin[cpu] = IA64_MCA_RENDEZ_CHECKIN_CONCURRENT_MCA;
	if (sos->monarch) {
		ia64_wait_for_slaves(cpu, "MCA");

		ia64_mca_wakeup_all();
	} else {
		while (cpu_isset(cpu, mca_cpu))
			cpu_relax();	
	}

	NOTIFY_MCA(DIE_MCA_MONARCH_PROCESS, regs, (long)&nd, 1);

	
	ia64_mca_log_sal_error_record(SAL_INFO_TYPE_MCA);

	
	recover = (ia64_mca_ucmc_extension
		&& ia64_mca_ucmc_extension(
			IA64_LOG_CURR_BUFFER(SAL_INFO_TYPE_MCA),
			sos));

	if (recover) {
		sal_log_record_header_t *rh = IA64_LOG_CURR_BUFFER(SAL_INFO_TYPE_MCA);
		rh->severity = sal_log_severity_corrected;
		ia64_sal_clear_state_info(SAL_INFO_TYPE_MCA);
		sos->os_status = IA64_MCA_CORRECTED;
	} else {
		
		ia64_mlogbuf_finish(1);
	}

	if (__get_cpu_var(ia64_mca_tr_reload)) {
		mca_insert_tr(0x1); 
		mca_insert_tr(0x2); 
	}

	NOTIFY_MCA(DIE_MCA_MONARCH_LEAVE, regs, (long)&nd, 1);

	if (atomic_dec_return(&mca_count) > 0) {
		int i;

		for_each_online_cpu(i) {
			if (cpu_isset(i, mca_cpu)) {
				monarch_cpu = i;
				cpu_clear(i, mca_cpu);	
				while (monarch_cpu != -1)
					cpu_relax();	
				set_curr_task(cpu, previous_current);
				ia64_mc_info.imi_rendez_checkin[cpu]
						= IA64_MCA_RENDEZ_CHECKIN_NOTDONE;
				return;
			}
		}
	}
	set_curr_task(cpu, previous_current);
	ia64_mc_info.imi_rendez_checkin[cpu] = IA64_MCA_RENDEZ_CHECKIN_NOTDONE;
	monarch_cpu = -1;	
}

static DECLARE_WORK(cmc_disable_work, ia64_mca_cmc_vector_disable_keventd);
static DECLARE_WORK(cmc_enable_work, ia64_mca_cmc_vector_enable_keventd);

static irqreturn_t
ia64_mca_cmc_int_handler(int cmc_irq, void *arg)
{
	static unsigned long	cmc_history[CMC_HISTORY_LENGTH];
	static int		index;
	static DEFINE_SPINLOCK(cmc_history_lock);

	IA64_MCA_DEBUG("%s: received interrupt vector = %#x on CPU %d\n",
		       __func__, cmc_irq, smp_processor_id());

	
	local_irq_enable();

	spin_lock(&cmc_history_lock);
	if (!cmc_polling_enabled) {
		int i, count = 1; 
		unsigned long now = jiffies;

		for (i = 0; i < CMC_HISTORY_LENGTH; i++) {
			if (now - cmc_history[i] <= HZ)
				count++;
		}

		IA64_MCA_DEBUG(KERN_INFO "CMC threshold %d/%d\n", count, CMC_HISTORY_LENGTH);
		if (count >= CMC_HISTORY_LENGTH) {

			cmc_polling_enabled = 1;
			spin_unlock(&cmc_history_lock);
			ia64_mca_cmc_vector_disable(NULL);
			schedule_work(&cmc_disable_work);

			printk(KERN_WARNING "WARNING: Switching to polling CMC handler; error records may be lost\n");

			mod_timer(&cmc_poll_timer, jiffies + CMC_POLL_INTERVAL);

			
			goto out;
		} else {
			cmc_history[index++] = now;
			if (index == CMC_HISTORY_LENGTH)
				index = 0;
		}
	}
	spin_unlock(&cmc_history_lock);
out:
	
	ia64_mca_log_sal_error_record(SAL_INFO_TYPE_CMC);

	local_irq_disable();

	return IRQ_HANDLED;
}

static irqreturn_t
ia64_mca_cmc_int_caller(int cmc_irq, void *arg)
{
	static int start_count = -1;
	unsigned int cpuid;

	cpuid = smp_processor_id();

	
	if (start_count == -1)
		start_count = IA64_LOG_COUNT(SAL_INFO_TYPE_CMC);

	ia64_mca_cmc_int_handler(cmc_irq, arg);

	cpuid = cpumask_next(cpuid+1, cpu_online_mask);

	if (cpuid < nr_cpu_ids) {
		platform_send_ipi(cpuid, IA64_CMCP_VECTOR, IA64_IPI_DM_INT, 0);
	} else {
		
		if (start_count == IA64_LOG_COUNT(SAL_INFO_TYPE_CMC)) {

			printk(KERN_WARNING "Returning to interrupt driven CMC handler\n");
			schedule_work(&cmc_enable_work);
			cmc_polling_enabled = 0;

		} else {

			mod_timer(&cmc_poll_timer, jiffies + CMC_POLL_INTERVAL);
		}

		start_count = -1;
	}

	return IRQ_HANDLED;
}

static void
ia64_mca_cmc_poll (unsigned long dummy)
{
	
	platform_send_ipi(cpumask_first(cpu_online_mask), IA64_CMCP_VECTOR,
							IA64_IPI_DM_INT, 0);
}

#ifdef CONFIG_ACPI

static irqreturn_t
ia64_mca_cpe_int_caller(int cpe_irq, void *arg)
{
	static int start_count = -1;
	static int poll_time = MIN_CPE_POLL_INTERVAL;
	unsigned int cpuid;

	cpuid = smp_processor_id();

	
	if (start_count == -1)
		start_count = IA64_LOG_COUNT(SAL_INFO_TYPE_CPE);

	ia64_mca_cpe_int_handler(cpe_irq, arg);

	cpuid = cpumask_next(cpuid+1, cpu_online_mask);

	if (cpuid < NR_CPUS) {
		platform_send_ipi(cpuid, IA64_CPEP_VECTOR, IA64_IPI_DM_INT, 0);
	} else {
		if (start_count != IA64_LOG_COUNT(SAL_INFO_TYPE_CPE)) {
			poll_time = max(MIN_CPE_POLL_INTERVAL, poll_time / 2);
		} else if (cpe_vector < 0) {
			poll_time = min(MAX_CPE_POLL_INTERVAL, poll_time * 2);
		} else {
			poll_time = MIN_CPE_POLL_INTERVAL;

			printk(KERN_WARNING "Returning to interrupt driven CPE handler\n");
			enable_irq(local_vector_to_irq(IA64_CPE_VECTOR));
			cpe_poll_enabled = 0;
		}

		if (cpe_poll_enabled)
			mod_timer(&cpe_poll_timer, jiffies + poll_time);
		start_count = -1;
	}

	return IRQ_HANDLED;
}

static void
ia64_mca_cpe_poll (unsigned long dummy)
{
	
	platform_send_ipi(cpumask_first(cpu_online_mask), IA64_CPEP_VECTOR,
							IA64_IPI_DM_INT, 0);
}

#endif 

static int
default_monarch_init_process(struct notifier_block *self, unsigned long val, void *data)
{
	int c;
	struct task_struct *g, *t;
	if (val != DIE_INIT_MONARCH_PROCESS)
		return NOTIFY_DONE;
#ifdef CONFIG_KEXEC
	if (atomic_read(&kdump_in_progress))
		return NOTIFY_DONE;
#endif

	BREAK_LOGLEVEL(console_loglevel);
	ia64_mlogbuf_dump_from_init();

	printk(KERN_ERR "Processes interrupted by INIT -");
	for_each_online_cpu(c) {
		struct ia64_sal_os_state *s;
		t = __va(__per_cpu_mca[c] + IA64_MCA_CPU_INIT_STACK_OFFSET);
		s = (struct ia64_sal_os_state *)((char *)t + MCA_SOS_OFFSET);
		g = s->prev_task;
		if (g) {
			if (g->pid)
				printk(" %d", g->pid);
			else
				printk(" %d (cpu %d task 0x%p)", g->pid, task_cpu(g), g);
		}
	}
	printk("\n\n");
	if (read_trylock(&tasklist_lock)) {
		do_each_thread (g, t) {
			printk("\nBacktrace of pid %d (%s)\n", t->pid, t->comm);
			show_stack(t, NULL);
		} while_each_thread (g, t);
		read_unlock(&tasklist_lock);
	}
	
	RESTORE_LOGLEVEL(console_loglevel);
	return NOTIFY_DONE;
}


void
ia64_init_handler(struct pt_regs *regs, struct switch_stack *sw,
		  struct ia64_sal_os_state *sos)
{
	static atomic_t slaves;
	static atomic_t monarchs;
	struct task_struct *previous_current;
	int cpu = smp_processor_id();
	struct ia64_mca_notify_die nd =
		{ .sos = sos, .monarch_cpu = &monarch_cpu };

	NOTIFY_INIT(DIE_INIT_ENTER, regs, (long)&nd, 0);

	mprintk(KERN_INFO "Entered OS INIT handler. PSP=%lx cpu=%d monarch=%ld\n",
		sos->proc_state_param, cpu, sos->monarch);
	salinfo_log_wakeup(SAL_INFO_TYPE_INIT, NULL, 0, 0);

	previous_current = ia64_mca_modify_original_stack(regs, sw, sos, "INIT");
	sos->os_status = IA64_INIT_RESUME;

	if (!sos->monarch && atomic_add_return(1, &slaves) == num_online_cpus()) {
		mprintk(KERN_WARNING "%s: Promoting cpu %d to monarch.\n",
		        __func__, cpu);
		atomic_dec(&slaves);
		sos->monarch = 1;
	}

	if (sos->monarch && atomic_add_return(1, &monarchs) > 1) {
		mprintk(KERN_WARNING "%s: Demoting cpu %d to slave.\n",
			       __func__, cpu);
		atomic_dec(&monarchs);
		sos->monarch = 0;
	}

	if (!sos->monarch) {
		ia64_mc_info.imi_rendez_checkin[cpu] = IA64_MCA_RENDEZ_CHECKIN_INIT;

#ifdef CONFIG_KEXEC
		while (monarch_cpu == -1 && !atomic_read(&kdump_in_progress))
			udelay(1000);
#else
		while (monarch_cpu == -1)
			cpu_relax();	
#endif

		NOTIFY_INIT(DIE_INIT_SLAVE_ENTER, regs, (long)&nd, 1);
		NOTIFY_INIT(DIE_INIT_SLAVE_PROCESS, regs, (long)&nd, 1);

#ifdef CONFIG_KEXEC
		while (monarch_cpu != -1 && !atomic_read(&kdump_in_progress))
			udelay(1000);
#else
		while (monarch_cpu != -1)
			cpu_relax();	
#endif

		NOTIFY_INIT(DIE_INIT_SLAVE_LEAVE, regs, (long)&nd, 1);

		mprintk("Slave on cpu %d returning to normal service.\n", cpu);
		set_curr_task(cpu, previous_current);
		ia64_mc_info.imi_rendez_checkin[cpu] = IA64_MCA_RENDEZ_CHECKIN_NOTDONE;
		atomic_dec(&slaves);
		return;
	}

	monarch_cpu = cpu;
	NOTIFY_INIT(DIE_INIT_MONARCH_ENTER, regs, (long)&nd, 1);

	mprintk("Delaying for 5 seconds...\n");
	udelay(5*1000000);
	ia64_wait_for_slaves(cpu, "INIT");
	NOTIFY_INIT(DIE_INIT_MONARCH_PROCESS, regs, (long)&nd, 1);
	NOTIFY_INIT(DIE_INIT_MONARCH_LEAVE, regs, (long)&nd, 1);

	mprintk("\nINIT dump complete.  Monarch on cpu %d returning to normal service.\n", cpu);
	atomic_dec(&monarchs);
	set_curr_task(cpu, previous_current);
	monarch_cpu = -1;
	return;
}

static int __init
ia64_mca_disable_cpe_polling(char *str)
{
	cpe_poll_enabled = 0;
	return 1;
}

__setup("disable_cpe_poll", ia64_mca_disable_cpe_polling);

static struct irqaction cmci_irqaction = {
	.handler =	ia64_mca_cmc_int_handler,
	.flags =	IRQF_DISABLED,
	.name =		"cmc_hndlr"
};

static struct irqaction cmcp_irqaction = {
	.handler =	ia64_mca_cmc_int_caller,
	.flags =	IRQF_DISABLED,
	.name =		"cmc_poll"
};

static struct irqaction mca_rdzv_irqaction = {
	.handler =	ia64_mca_rendez_int_handler,
	.flags =	IRQF_DISABLED,
	.name =		"mca_rdzv"
};

static struct irqaction mca_wkup_irqaction = {
	.handler =	ia64_mca_wakeup_int_handler,
	.flags =	IRQF_DISABLED,
	.name =		"mca_wkup"
};

#ifdef CONFIG_ACPI
static struct irqaction mca_cpe_irqaction = {
	.handler =	ia64_mca_cpe_int_handler,
	.flags =	IRQF_DISABLED,
	.name =		"cpe_hndlr"
};

static struct irqaction mca_cpep_irqaction = {
	.handler =	ia64_mca_cpe_int_caller,
	.flags =	IRQF_DISABLED,
	.name =		"cpe_poll"
};
#endif 


static void __cpuinit
format_mca_init_stack(void *mca_data, unsigned long offset,
		const char *type, int cpu)
{
	struct task_struct *p = (struct task_struct *)((char *)mca_data + offset);
	struct thread_info *ti;
	memset(p, 0, KERNEL_STACK_SIZE);
	ti = task_thread_info(p);
	ti->flags = _TIF_MCA_INIT;
	ti->preempt_count = 1;
	ti->task = p;
	ti->cpu = cpu;
	p->stack = ti;
	p->state = TASK_UNINTERRUPTIBLE;
	cpu_set(cpu, p->cpus_allowed);
	INIT_LIST_HEAD(&p->tasks);
	p->parent = p->real_parent = p->group_leader = p;
	INIT_LIST_HEAD(&p->children);
	INIT_LIST_HEAD(&p->sibling);
	strncpy(p->comm, type, sizeof(p->comm)-1);
}

static void * __init_refok mca_bootmem(void)
{
	return __alloc_bootmem(sizeof(struct ia64_mca_cpu),
	                    KERNEL_STACK_SIZE, 0);
}

void __cpuinit
ia64_mca_cpu_init(void *cpu_data)
{
	void *pal_vaddr;
	void *data;
	long sz = sizeof(struct ia64_mca_cpu);
	int cpu = smp_processor_id();
	static int first_time = 1;

	if (__per_cpu_mca[cpu]) {
		data = __va(__per_cpu_mca[cpu]);
	} else {
		if (first_time) {
			data = mca_bootmem();
			first_time = 0;
		} else
			data = (void *)__get_free_pages(GFP_KERNEL,
							get_order(sz));
		if (!data)
			panic("Could not allocate MCA memory for cpu %d\n",
					cpu);
	}
	format_mca_init_stack(data, offsetof(struct ia64_mca_cpu, mca_stack),
		"MCA", cpu);
	format_mca_init_stack(data, offsetof(struct ia64_mca_cpu, init_stack),
		"INIT", cpu);
	__get_cpu_var(ia64_mca_data) = __per_cpu_mca[cpu] = __pa(data);

	__get_cpu_var(ia64_mca_per_cpu_pte) =
		pte_val(mk_pte_phys(__pa(cpu_data), PAGE_KERNEL));

	pal_vaddr = efi_get_pal_addr();
	if (!pal_vaddr)
		return;
	__get_cpu_var(ia64_mca_pal_base) =
		GRANULEROUNDDOWN((unsigned long) pal_vaddr);
	__get_cpu_var(ia64_mca_pal_pte) = pte_val(mk_pte_phys(__pa(pal_vaddr),
							      PAGE_KERNEL));
}

static void __cpuinit ia64_mca_cmc_vector_adjust(void *dummy)
{
	unsigned long flags;

	local_irq_save(flags);
	if (!cmc_polling_enabled)
		ia64_mca_cmc_vector_enable(NULL);
	local_irq_restore(flags);
}

static int __cpuinit mca_cpu_callback(struct notifier_block *nfb,
				      unsigned long action,
				      void *hcpu)
{
	int hotcpu = (unsigned long) hcpu;

	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		smp_call_function_single(hotcpu, ia64_mca_cmc_vector_adjust,
					 NULL, 0);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block mca_cpu_notifier __cpuinitdata = {
	.notifier_call = mca_cpu_callback
};

void __init
ia64_mca_init(void)
{
	ia64_fptr_t *init_hldlr_ptr_monarch = (ia64_fptr_t *)ia64_os_init_dispatch_monarch;
	ia64_fptr_t *init_hldlr_ptr_slave = (ia64_fptr_t *)ia64_os_init_dispatch_slave;
	ia64_fptr_t *mca_hldlr_ptr = (ia64_fptr_t *)ia64_os_mca_dispatch;
	int i;
	long rc;
	struct ia64_sal_retval isrv;
	unsigned long timeout = IA64_MCA_RENDEZ_TIMEOUT; 
	static struct notifier_block default_init_monarch_nb = {
		.notifier_call = default_monarch_init_process,
		.priority = 0
	};

	IA64_MCA_DEBUG("%s: begin\n", __func__);

	
	for(i = 0 ; i < NR_CPUS; i++)
		ia64_mc_info.imi_rendez_checkin[i] = IA64_MCA_RENDEZ_CHECKIN_NOTDONE;


	
	while (1) {
		isrv = ia64_sal_mc_set_params(SAL_MC_PARAM_RENDEZ_INT,
					      SAL_MC_PARAM_MECHANISM_INT,
					      IA64_MCA_RENDEZ_VECTOR,
					      timeout,
					      SAL_MC_PARAM_RZ_ALWAYS);
		rc = isrv.status;
		if (rc == 0)
			break;
		if (rc == -2) {
			printk(KERN_INFO "Increasing MCA rendezvous timeout from "
				"%ld to %ld milliseconds\n", timeout, isrv.v0);
			timeout = isrv.v0;
			NOTIFY_MCA(DIE_MCA_NEW_TIMEOUT, NULL, timeout, 0);
			continue;
		}
		printk(KERN_ERR "Failed to register rendezvous interrupt "
		       "with SAL (status %ld)\n", rc);
		return;
	}

	
	isrv = ia64_sal_mc_set_params(SAL_MC_PARAM_RENDEZ_WAKEUP,
				      SAL_MC_PARAM_MECHANISM_INT,
				      IA64_MCA_WAKEUP_VECTOR,
				      0, 0);
	rc = isrv.status;
	if (rc) {
		printk(KERN_ERR "Failed to register wakeup interrupt with SAL "
		       "(status %ld)\n", rc);
		return;
	}

	IA64_MCA_DEBUG("%s: registered MCA rendezvous spinloop and wakeup mech.\n", __func__);

	ia64_mc_info.imi_mca_handler        = ia64_tpa(mca_hldlr_ptr->fp);
	ia64_mc_info.imi_mca_handler_size	= 0;

	
	if ((rc = ia64_sal_set_vectors(SAL_VECTOR_OS_MCA,
				       ia64_mc_info.imi_mca_handler,
				       ia64_tpa(mca_hldlr_ptr->gp),
				       ia64_mc_info.imi_mca_handler_size,
				       0, 0, 0)))
	{
		printk(KERN_ERR "Failed to register OS MCA handler with SAL "
		       "(status %ld)\n", rc);
		return;
	}

	IA64_MCA_DEBUG("%s: registered OS MCA handler with SAL at 0x%lx, gp = 0x%lx\n", __func__,
		       ia64_mc_info.imi_mca_handler, ia64_tpa(mca_hldlr_ptr->gp));

	ia64_mc_info.imi_monarch_init_handler		= ia64_tpa(init_hldlr_ptr_monarch->fp);
	ia64_mc_info.imi_monarch_init_handler_size	= 0;
	ia64_mc_info.imi_slave_init_handler		= ia64_tpa(init_hldlr_ptr_slave->fp);
	ia64_mc_info.imi_slave_init_handler_size	= 0;

	IA64_MCA_DEBUG("%s: OS INIT handler at %lx\n", __func__,
		       ia64_mc_info.imi_monarch_init_handler);

	
	if ((rc = ia64_sal_set_vectors(SAL_VECTOR_OS_INIT,
				       ia64_mc_info.imi_monarch_init_handler,
				       ia64_tpa(ia64_getreg(_IA64_REG_GP)),
				       ia64_mc_info.imi_monarch_init_handler_size,
				       ia64_mc_info.imi_slave_init_handler,
				       ia64_tpa(ia64_getreg(_IA64_REG_GP)),
				       ia64_mc_info.imi_slave_init_handler_size)))
	{
		printk(KERN_ERR "Failed to register m/s INIT handlers with SAL "
		       "(status %ld)\n", rc);
		return;
	}
	if (register_die_notifier(&default_init_monarch_nb)) {
		printk(KERN_ERR "Failed to register default monarch INIT process\n");
		return;
	}

	IA64_MCA_DEBUG("%s: registered OS INIT handler with SAL\n", __func__);

	ia64_log_init(SAL_INFO_TYPE_MCA);
	ia64_log_init(SAL_INFO_TYPE_INIT);
	ia64_log_init(SAL_INFO_TYPE_CMC);
	ia64_log_init(SAL_INFO_TYPE_CPE);

	mca_init = 1;
	printk(KERN_INFO "MCA related initialization done\n");
}

static int __init
ia64_mca_late_init(void)
{
	if (!mca_init)
		return 0;

	register_percpu_irq(IA64_CMC_VECTOR, &cmci_irqaction);
	register_percpu_irq(IA64_CMCP_VECTOR, &cmcp_irqaction);
	ia64_mca_cmc_vector_setup();       

	
	register_percpu_irq(IA64_MCA_RENDEZ_VECTOR, &mca_rdzv_irqaction);

	
	register_percpu_irq(IA64_MCA_WAKEUP_VECTOR, &mca_wkup_irqaction);

#ifdef CONFIG_ACPI
	
	register_percpu_irq(IA64_CPEP_VECTOR, &mca_cpep_irqaction);
#endif

	register_hotcpu_notifier(&mca_cpu_notifier);

	
	init_timer(&cmc_poll_timer);
	cmc_poll_timer.function = ia64_mca_cmc_poll;

	
	cmc_polling_enabled = 0;
	schedule_work(&cmc_enable_work);

	IA64_MCA_DEBUG("%s: CMCI/P setup and enabled.\n", __func__);

#ifdef CONFIG_ACPI
	
	cpe_vector = acpi_request_vector(ACPI_INTERRUPT_CPEI);
	init_timer(&cpe_poll_timer);
	cpe_poll_timer.function = ia64_mca_cpe_poll;

	{
		unsigned int irq;

		if (cpe_vector >= 0) {
			
			irq = local_vector_to_irq(cpe_vector);
			if (irq > 0) {
				cpe_poll_enabled = 0;
				irq_set_status_flags(irq, IRQ_PER_CPU);
				setup_irq(irq, &mca_cpe_irqaction);
				ia64_cpe_irq = irq;
				ia64_mca_register_cpev(cpe_vector);
				IA64_MCA_DEBUG("%s: CPEI/P setup and enabled.\n",
					__func__);
				return 0;
			}
			printk(KERN_ERR "%s: Failed to find irq for CPE "
					"interrupt handler, vector %d\n",
					__func__, cpe_vector);
		}
		
		if (cpe_poll_enabled) {
			ia64_mca_cpe_poll(0UL);
			IA64_MCA_DEBUG("%s: CPEP setup and enabled.\n", __func__);
		}
	}
#endif

	return 0;
}

device_initcall(ia64_mca_late_init);
