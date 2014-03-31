/*
 * This file implements the perfmon-2 subsystem which is used
 * to program the IA-64 Performance Monitoring Unit (PMU).
 *
 * The initial version of perfmon.c was written by
 * Ganesh Venkitachalam, IBM Corp.
 *
 * Then it was modified for perfmon-1.x by Stephane Eranian and
 * David Mosberger, Hewlett Packard Co.
 *
 * Version Perfmon-2.x is a rewrite of perfmon-1.x
 * by Stephane Eranian, Hewlett Packard Co.
 *
 * Copyright (C) 1999-2005  Hewlett Packard Co
 *               Stephane Eranian <eranian@hpl.hp.com>
 *               David Mosberger-Tang <davidm@hpl.hp.com>
 *
 * More information about perfmon available at:
 * 	http://www.hpl.hp.com/research/linux/perfmon
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/sysctl.h>
#include <linux/list.h>
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/vfs.h>
#include <linux/smp.h>
#include <linux/pagemap.h>
#include <linux/mount.h>
#include <linux/bitops.h>
#include <linux/capability.h>
#include <linux/rcupdate.h>
#include <linux/completion.h>
#include <linux/tracehook.h>
#include <linux/slab.h>

#include <asm/errno.h>
#include <asm/intrinsics.h>
#include <asm/page.h>
#include <asm/perfmon.h>
#include <asm/processor.h>
#include <asm/signal.h>
#include <asm/uaccess.h>
#include <asm/delay.h>

#ifdef CONFIG_PERFMON
#define PFM_CTX_UNLOADED	1	
#define PFM_CTX_LOADED		2	
#define PFM_CTX_MASKED		3	
#define PFM_CTX_ZOMBIE		4	

#define PFM_INVALID_ACTIVATION	(~0UL)

#define PFM_NUM_PMC_REGS	64	
#define PFM_NUM_PMD_REGS	64	

#define PFM_MAX_MSGS		32
#define PFM_CTXQ_EMPTY(g)	((g)->ctx_msgq_head == (g)->ctx_msgq_tail)

#define PFM_REG_NOTIMPL		0x0 
#define PFM_REG_IMPL		0x1 
#define PFM_REG_END		0x2 
#define PFM_REG_MONITOR		(0x1<<4|PFM_REG_IMPL) 
#define PFM_REG_COUNTING	(0x2<<4|PFM_REG_MONITOR) 
#define PFM_REG_CONTROL		(0x4<<4|PFM_REG_IMPL) 
#define	PFM_REG_CONFIG		(0x8<<4|PFM_REG_IMPL) 
#define PFM_REG_BUFFER	 	(0xc<<4|PFM_REG_IMPL) 

#define PMC_IS_LAST(i)	(pmu_conf->pmc_desc[i].type & PFM_REG_END)
#define PMD_IS_LAST(i)	(pmu_conf->pmd_desc[i].type & PFM_REG_END)

#define PMC_OVFL_NOTIFY(ctx, i)	((ctx)->ctx_pmds[i].flags &  PFM_REGFL_OVFL_NOTIFY)

#define PMC_IS_IMPL(i)	  (i< PMU_MAX_PMCS && (pmu_conf->pmc_desc[i].type & PFM_REG_IMPL))
#define PMD_IS_IMPL(i)	  (i< PMU_MAX_PMDS && (pmu_conf->pmd_desc[i].type & PFM_REG_IMPL))

#define PMD_IS_COUNTING(i) ((pmu_conf->pmd_desc[i].type & PFM_REG_COUNTING) == PFM_REG_COUNTING)
#define PMC_IS_COUNTING(i) ((pmu_conf->pmc_desc[i].type & PFM_REG_COUNTING) == PFM_REG_COUNTING)
#define PMC_IS_MONITOR(i)  ((pmu_conf->pmc_desc[i].type & PFM_REG_MONITOR)  == PFM_REG_MONITOR)
#define PMC_IS_CONTROL(i)  ((pmu_conf->pmc_desc[i].type & PFM_REG_CONTROL)  == PFM_REG_CONTROL)

#define PMC_DFL_VAL(i)     pmu_conf->pmc_desc[i].default_value
#define PMC_RSVD_MASK(i)   pmu_conf->pmc_desc[i].reserved_mask
#define PMD_PMD_DEP(i)	   pmu_conf->pmd_desc[i].dep_pmd[0]
#define PMC_PMD_DEP(i)	   pmu_conf->pmc_desc[i].dep_pmd[0]

#define PFM_NUM_IBRS	  IA64_NUM_DBG_REGS
#define PFM_NUM_DBRS	  IA64_NUM_DBG_REGS

#define CTX_OVFL_NOBLOCK(c)	((c)->ctx_fl_block == 0)
#define CTX_HAS_SMPL(c)		((c)->ctx_fl_is_sampling)
#define PFM_CTX_TASK(h)		(h)->ctx_task

#define PMU_PMC_OI		5 

#define CTX_USED_PMD(ctx, mask) (ctx)->ctx_used_pmds[0] |= (mask)
#define CTX_IS_USED_PMD(ctx, c) (((ctx)->ctx_used_pmds[0] & (1UL << (c))) != 0UL)

#define CTX_USED_MONITOR(ctx, mask) (ctx)->ctx_used_monitors[0] |= (mask)

#define CTX_USED_IBR(ctx,n) 	(ctx)->ctx_used_ibrs[(n)>>6] |= 1UL<< ((n) % 64)
#define CTX_USED_DBR(ctx,n) 	(ctx)->ctx_used_dbrs[(n)>>6] |= 1UL<< ((n) % 64)
#define CTX_USES_DBREGS(ctx)	(((pfm_context_t *)(ctx))->ctx_fl_using_dbreg==1)
#define PFM_CODE_RR	0	
#define PFM_DATA_RR	1	

#define PFM_CPUINFO_CLEAR(v)	pfm_get_cpu_var(pfm_syst_info) &= ~(v)
#define PFM_CPUINFO_SET(v)	pfm_get_cpu_var(pfm_syst_info) |= (v)
#define PFM_CPUINFO_GET()	pfm_get_cpu_var(pfm_syst_info)

#define RDEP(x)	(1UL<<(x))

#define PROTECT_CTX(c, f) \
	do {  \
		DPRINT(("spinlock_irq_save ctx %p by [%d]\n", c, task_pid_nr(current))); \
		spin_lock_irqsave(&(c)->ctx_lock, f); \
		DPRINT(("spinlocked ctx %p  by [%d]\n", c, task_pid_nr(current))); \
	} while(0)

#define UNPROTECT_CTX(c, f) \
	do { \
		DPRINT(("spinlock_irq_restore ctx %p by [%d]\n", c, task_pid_nr(current))); \
		spin_unlock_irqrestore(&(c)->ctx_lock, f); \
	} while(0)

#define PROTECT_CTX_NOPRINT(c, f) \
	do {  \
		spin_lock_irqsave(&(c)->ctx_lock, f); \
	} while(0)


#define UNPROTECT_CTX_NOPRINT(c, f) \
	do { \
		spin_unlock_irqrestore(&(c)->ctx_lock, f); \
	} while(0)


#define PROTECT_CTX_NOIRQ(c) \
	do {  \
		spin_lock(&(c)->ctx_lock); \
	} while(0)

#define UNPROTECT_CTX_NOIRQ(c) \
	do { \
		spin_unlock(&(c)->ctx_lock); \
	} while(0)


#ifdef CONFIG_SMP

#define GET_ACTIVATION()	pfm_get_cpu_var(pmu_activation_number)
#define INC_ACTIVATION()	pfm_get_cpu_var(pmu_activation_number)++
#define SET_ACTIVATION(c)	(c)->ctx_last_activation = GET_ACTIVATION()

#else 
#define SET_ACTIVATION(t) 	do {} while(0)
#define GET_ACTIVATION(t) 	do {} while(0)
#define INC_ACTIVATION(t) 	do {} while(0)
#endif 

#define SET_PMU_OWNER(t, c)	do { pfm_get_cpu_var(pmu_owner) = (t); pfm_get_cpu_var(pmu_ctx) = (c); } while(0)
#define GET_PMU_OWNER()		pfm_get_cpu_var(pmu_owner)
#define GET_PMU_CTX()		pfm_get_cpu_var(pmu_ctx)

#define LOCK_PFS(g)	    	spin_lock_irqsave(&pfm_sessions.pfs_lock, g)
#define UNLOCK_PFS(g)	    	spin_unlock_irqrestore(&pfm_sessions.pfs_lock, g)

#define PFM_REG_RETFLAG_SET(flags, val)	do { flags &= ~PFM_REG_RETFL_MASK; flags |= (val); } while(0)

#define PMC0_HAS_OVFL(cmp0)  (cmp0 & ~0x1UL)

#define PFMFS_MAGIC 0xa0b4d889

#define PFM_DEBUGGING 1
#ifdef PFM_DEBUGGING
#define DPRINT(a) \
	do { \
		if (unlikely(pfm_sysctl.debug >0)) { printk("%s.%d: CPU%d [%d] ", __func__, __LINE__, smp_processor_id(), task_pid_nr(current)); printk a; } \
	} while (0)

#define DPRINT_ovfl(a) \
	do { \
		if (unlikely(pfm_sysctl.debug > 0 && pfm_sysctl.debug_ovfl >0)) { printk("%s.%d: CPU%d [%d] ", __func__, __LINE__, smp_processor_id(), task_pid_nr(current)); printk a; } \
	} while (0)
#endif

typedef struct {
	unsigned long	val;		
	unsigned long	lval;		
	unsigned long	long_reset;	
	unsigned long	short_reset;    
	unsigned long	reset_pmds[4];  
	unsigned long	smpl_pmds[4];   
	unsigned long	seed;		
	unsigned long	mask;		
	unsigned int 	flags;		
	unsigned long	eventid;	
} pfm_counter_t;

typedef struct {
	unsigned int block:1;		
	unsigned int system:1;		
	unsigned int using_dbreg:1;	
	unsigned int is_sampling:1;	
	unsigned int excl_idle:1;	
	unsigned int going_zombie:1;	
	unsigned int trap_reason:2;	
	unsigned int no_msg:1;		
	unsigned int can_restart:1;	
	unsigned int reserved:22;
} pfm_context_flags_t;

#define PFM_TRAP_REASON_NONE		0x0	
#define PFM_TRAP_REASON_BLOCK		0x1	
#define PFM_TRAP_REASON_RESET		0x2	



typedef struct pfm_context {
	spinlock_t		ctx_lock;		

	pfm_context_flags_t	ctx_flags;		
	unsigned int		ctx_state;		

	struct task_struct 	*ctx_task;		

	unsigned long		ctx_ovfl_regs[4];	

	struct completion	ctx_restart_done;  	

	unsigned long		ctx_used_pmds[4];	
	unsigned long		ctx_all_pmds[4];	
	unsigned long		ctx_reload_pmds[4];	

	unsigned long		ctx_all_pmcs[4];	
	unsigned long		ctx_reload_pmcs[4];	
	unsigned long		ctx_used_monitors[4];	

	unsigned long		ctx_pmcs[PFM_NUM_PMC_REGS];	

	unsigned int		ctx_used_ibrs[1];		
	unsigned int		ctx_used_dbrs[1];		
	unsigned long		ctx_dbrs[IA64_NUM_DBG_REGS];	
	unsigned long		ctx_ibrs[IA64_NUM_DBG_REGS];	

	pfm_counter_t		ctx_pmds[PFM_NUM_PMD_REGS]; 

	unsigned long		th_pmcs[PFM_NUM_PMC_REGS];	
	unsigned long		th_pmds[PFM_NUM_PMD_REGS];	

	unsigned long		ctx_saved_psr_up;	

	unsigned long		ctx_last_activation;	
	unsigned int		ctx_last_cpu;		
	unsigned int		ctx_cpu;		

	int			ctx_fd;			
	pfm_ovfl_arg_t		ctx_ovfl_arg;		

	pfm_buffer_fmt_t	*ctx_buf_fmt;		
	void			*ctx_smpl_hdr;		
	unsigned long		ctx_smpl_size;		
	void			*ctx_smpl_vaddr;	

	wait_queue_head_t 	ctx_msgq_wait;
	pfm_msg_t		ctx_msgq[PFM_MAX_MSGS];
	int			ctx_msgq_head;
	int			ctx_msgq_tail;
	struct fasync_struct	*ctx_async_queue;

	wait_queue_head_t 	ctx_zombieq;		
} pfm_context_t;

#define PFM_IS_FILE(f)		((f)->f_op == &pfm_file_ops)

#define PFM_GET_CTX(t)	 	((pfm_context_t *)(t)->thread.pfm_context)

#ifdef CONFIG_SMP
#define SET_LAST_CPU(ctx, v)	(ctx)->ctx_last_cpu = (v)
#define GET_LAST_CPU(ctx)	(ctx)->ctx_last_cpu
#else
#define SET_LAST_CPU(ctx, v)	do {} while(0)
#define GET_LAST_CPU(ctx)	do {} while(0)
#endif


#define ctx_fl_block		ctx_flags.block
#define ctx_fl_system		ctx_flags.system
#define ctx_fl_using_dbreg	ctx_flags.using_dbreg
#define ctx_fl_is_sampling	ctx_flags.is_sampling
#define ctx_fl_excl_idle	ctx_flags.excl_idle
#define ctx_fl_going_zombie	ctx_flags.going_zombie
#define ctx_fl_trap_reason	ctx_flags.trap_reason
#define ctx_fl_no_msg		ctx_flags.no_msg
#define ctx_fl_can_restart	ctx_flags.can_restart

#define PFM_SET_WORK_PENDING(t, v)	do { (t)->thread.pfm_needs_checking = v; } while(0);
#define PFM_GET_WORK_PENDING(t)		(t)->thread.pfm_needs_checking

typedef struct {
	spinlock_t		pfs_lock;		   

	unsigned int		pfs_task_sessions;	   
	unsigned int		pfs_sys_sessions;	   
	unsigned int		pfs_sys_use_dbregs;	   
	unsigned int		pfs_ptrace_use_dbregs;	   
	struct task_struct	*pfs_sys_session[NR_CPUS]; 
} pfm_session_t;

typedef int (*pfm_reg_check_t)(struct task_struct *task, pfm_context_t *ctx, unsigned int cnum, unsigned long *val, struct pt_regs *regs);
typedef struct {
	unsigned int		type;
	int			pm_pos;
	unsigned long		default_value;	
	unsigned long		reserved_mask;	
	pfm_reg_check_t		read_check;
	pfm_reg_check_t		write_check;
	unsigned long		dep_pmd[4];
	unsigned long		dep_pmc[4];
} pfm_reg_desc_t;

#define PMC_PM(cnum, val)	(((val) >> (pmu_conf->pmc_desc[cnum].pm_pos)) & 0x1)

typedef struct {
	unsigned long  ovfl_val;	

	pfm_reg_desc_t *pmc_desc;	
	pfm_reg_desc_t *pmd_desc;	

	unsigned int   num_pmcs;	
	unsigned int   num_pmds;	
	unsigned long  impl_pmcs[4];	
	unsigned long  impl_pmds[4];	

	char	      *pmu_name;	
	unsigned int  pmu_family;	
	unsigned int  flags;		
	unsigned int  num_ibrs;		
	unsigned int  num_dbrs;		
	unsigned int  num_counters;	
	int           (*probe)(void);   
	unsigned int  use_rr_dbregs:1;	
} pmu_config_t;
#define PFM_PMU_IRQ_RESEND	1	

typedef struct {
	unsigned long ibr_mask:56;
	unsigned long ibr_plm:4;
	unsigned long ibr_ig:3;
	unsigned long ibr_x:1;
} ibr_mask_reg_t;

typedef struct {
	unsigned long dbr_mask:56;
	unsigned long dbr_plm:4;
	unsigned long dbr_ig:2;
	unsigned long dbr_w:1;
	unsigned long dbr_r:1;
} dbr_mask_reg_t;

typedef union {
	unsigned long  val;
	ibr_mask_reg_t ibr;
	dbr_mask_reg_t dbr;
} dbreg_t;


typedef struct {
	int		(*cmd_func)(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs);
	char		*cmd_name;
	int		cmd_flags;
	unsigned int	cmd_narg;
	size_t		cmd_argsize;
	int		(*cmd_getsize)(void *arg, size_t *sz);
} pfm_cmd_desc_t;

#define PFM_CMD_FD		0x01	
#define PFM_CMD_ARG_READ	0x02	
#define PFM_CMD_ARG_RW		0x04	
#define PFM_CMD_STOP		0x08	


#define PFM_CMD_NAME(cmd)	pfm_cmd_tab[(cmd)].cmd_name
#define PFM_CMD_READ_ARG(cmd)	(pfm_cmd_tab[(cmd)].cmd_flags & PFM_CMD_ARG_READ)
#define PFM_CMD_RW_ARG(cmd)	(pfm_cmd_tab[(cmd)].cmd_flags & PFM_CMD_ARG_RW)
#define PFM_CMD_USE_FD(cmd)	(pfm_cmd_tab[(cmd)].cmd_flags & PFM_CMD_FD)
#define PFM_CMD_STOPPED(cmd)	(pfm_cmd_tab[(cmd)].cmd_flags & PFM_CMD_STOP)

#define PFM_CMD_ARG_MANY	-1 

typedef struct {
	unsigned long pfm_spurious_ovfl_intr_count;	
	unsigned long pfm_replay_ovfl_intr_count;	
	unsigned long pfm_ovfl_intr_count; 		
	unsigned long pfm_ovfl_intr_cycles;		
	unsigned long pfm_ovfl_intr_cycles_min;		
	unsigned long pfm_ovfl_intr_cycles_max;		
	unsigned long pfm_smpl_handler_calls;
	unsigned long pfm_smpl_handler_cycles;
	char pad[SMP_CACHE_BYTES] ____cacheline_aligned;
} pfm_stats_t;

static pfm_stats_t		pfm_stats[NR_CPUS];
static pfm_session_t		pfm_sessions;	

static DEFINE_SPINLOCK(pfm_alt_install_check);
static pfm_intr_handler_desc_t  *pfm_alt_intr_handler;

static struct proc_dir_entry 	*perfmon_dir;
static pfm_uuid_t		pfm_null_uuid = {0,};

static spinlock_t		pfm_buffer_fmt_lock;
static LIST_HEAD(pfm_buffer_fmt_list);

static pmu_config_t		*pmu_conf;

pfm_sysctl_t pfm_sysctl;
EXPORT_SYMBOL(pfm_sysctl);

static ctl_table pfm_ctl_table[]={
	{
		.procname	= "debug",
		.data		= &pfm_sysctl.debug,
		.maxlen		= sizeof(int),
		.mode		= 0666,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "debug_ovfl",
		.data		= &pfm_sysctl.debug_ovfl,
		.maxlen		= sizeof(int),
		.mode		= 0666,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "fastctxsw",
		.data		= &pfm_sysctl.fastctxsw,
		.maxlen		= sizeof(int),
		.mode		= 0600,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "expert_mode",
		.data		= &pfm_sysctl.expert_mode,
		.maxlen		= sizeof(int),
		.mode		= 0600,
		.proc_handler	= proc_dointvec,
	},
	{}
};
static ctl_table pfm_sysctl_dir[] = {
	{
		.procname	= "perfmon",
		.mode		= 0555,
		.child		= pfm_ctl_table,
	},
 	{}
};
static ctl_table pfm_sysctl_root[] = {
	{
		.procname	= "kernel",
		.mode		= 0555,
		.child		= pfm_sysctl_dir,
	},
 	{}
};
static struct ctl_table_header *pfm_sysctl_header;

static int pfm_context_unload(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs);

#define pfm_get_cpu_var(v)		__ia64_per_cpu_var(v)
#define pfm_get_cpu_data(a,b)		per_cpu(a, b)

static inline void
pfm_put_task(struct task_struct *task)
{
	if (task != current) put_task_struct(task);
}

static inline void
pfm_reserve_page(unsigned long a)
{
	SetPageReserved(vmalloc_to_page((void *)a));
}
static inline void
pfm_unreserve_page(unsigned long a)
{
	ClearPageReserved(vmalloc_to_page((void*)a));
}

static inline unsigned long
pfm_protect_ctx_ctxsw(pfm_context_t *x)
{
	spin_lock(&(x)->ctx_lock);
	return 0UL;
}

static inline void
pfm_unprotect_ctx_ctxsw(pfm_context_t *x, unsigned long f)
{
	spin_unlock(&(x)->ctx_lock);
}

static inline unsigned long 
pfm_get_unmapped_area(struct file *file, unsigned long addr, unsigned long len, unsigned long pgoff, unsigned long flags, unsigned long exec)
{
	return get_unmapped_area(file, addr, len, pgoff, flags);
}

static const struct dentry_operations pfmfs_dentry_operations;

static struct dentry *
pfmfs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data)
{
	return mount_pseudo(fs_type, "pfm:", NULL, &pfmfs_dentry_operations,
			PFMFS_MAGIC);
}

static struct file_system_type pfm_fs_type = {
	.name     = "pfmfs",
	.mount    = pfmfs_mount,
	.kill_sb  = kill_anon_super,
};

DEFINE_PER_CPU(unsigned long, pfm_syst_info);
DEFINE_PER_CPU(struct task_struct *, pmu_owner);
DEFINE_PER_CPU(pfm_context_t  *, pmu_ctx);
DEFINE_PER_CPU(unsigned long, pmu_activation_number);
EXPORT_PER_CPU_SYMBOL_GPL(pfm_syst_info);


static const struct file_operations pfm_file_ops;

#ifndef CONFIG_SMP
static void pfm_lazy_save_regs (struct task_struct *ta);
#endif

void dump_pmu_state(const char *);
static int pfm_write_ibr_dbr(int mode, pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs);

#include "perfmon_itanium.h"
#include "perfmon_mckinley.h"
#include "perfmon_montecito.h"
#include "perfmon_generic.h"

static pmu_config_t *pmu_confs[]={
	&pmu_conf_mont,
	&pmu_conf_mck,
	&pmu_conf_ita,
	&pmu_conf_gen, 
	NULL
};


static int pfm_end_notify_user(pfm_context_t *ctx);

static inline void
pfm_clear_psr_pp(void)
{
	ia64_rsm(IA64_PSR_PP);
	ia64_srlz_i();
}

static inline void
pfm_set_psr_pp(void)
{
	ia64_ssm(IA64_PSR_PP);
	ia64_srlz_i();
}

static inline void
pfm_clear_psr_up(void)
{
	ia64_rsm(IA64_PSR_UP);
	ia64_srlz_i();
}

static inline void
pfm_set_psr_up(void)
{
	ia64_ssm(IA64_PSR_UP);
	ia64_srlz_i();
}

static inline unsigned long
pfm_get_psr(void)
{
	unsigned long tmp;
	tmp = ia64_getreg(_IA64_REG_PSR);
	ia64_srlz_i();
	return tmp;
}

static inline void
pfm_set_psr_l(unsigned long val)
{
	ia64_setreg(_IA64_REG_PSR_L, val);
	ia64_srlz_i();
}

static inline void
pfm_freeze_pmu(void)
{
	ia64_set_pmc(0,1UL);
	ia64_srlz_d();
}

static inline void
pfm_unfreeze_pmu(void)
{
	ia64_set_pmc(0,0UL);
	ia64_srlz_d();
}

static inline void
pfm_restore_ibrs(unsigned long *ibrs, unsigned int nibrs)
{
	int i;

	for (i=0; i < nibrs; i++) {
		ia64_set_ibr(i, ibrs[i]);
		ia64_dv_serialize_instruction();
	}
	ia64_srlz_i();
}

static inline void
pfm_restore_dbrs(unsigned long *dbrs, unsigned int ndbrs)
{
	int i;

	for (i=0; i < ndbrs; i++) {
		ia64_set_dbr(i, dbrs[i]);
		ia64_dv_serialize_data();
	}
	ia64_srlz_d();
}

static inline unsigned long
pfm_read_soft_counter(pfm_context_t *ctx, int i)
{
	return ctx->ctx_pmds[i].val + (ia64_get_pmd(i) & pmu_conf->ovfl_val);
}

static inline void
pfm_write_soft_counter(pfm_context_t *ctx, int i, unsigned long val)
{
	unsigned long ovfl_val = pmu_conf->ovfl_val;

	ctx->ctx_pmds[i].val = val  & ~ovfl_val;
	ia64_set_pmd(i, val & ovfl_val);
}

static pfm_msg_t *
pfm_get_new_msg(pfm_context_t *ctx)
{
	int idx, next;

	next = (ctx->ctx_msgq_tail+1) % PFM_MAX_MSGS;

	DPRINT(("ctx_fd=%p head=%d tail=%d\n", ctx, ctx->ctx_msgq_head, ctx->ctx_msgq_tail));
	if (next == ctx->ctx_msgq_head) return NULL;

 	idx = 	ctx->ctx_msgq_tail;
	ctx->ctx_msgq_tail = next;

	DPRINT(("ctx=%p head=%d tail=%d msg=%d\n", ctx, ctx->ctx_msgq_head, ctx->ctx_msgq_tail, idx));

	return ctx->ctx_msgq+idx;
}

static pfm_msg_t *
pfm_get_next_msg(pfm_context_t *ctx)
{
	pfm_msg_t *msg;

	DPRINT(("ctx=%p head=%d tail=%d\n", ctx, ctx->ctx_msgq_head, ctx->ctx_msgq_tail));

	if (PFM_CTXQ_EMPTY(ctx)) return NULL;

	msg = ctx->ctx_msgq+ctx->ctx_msgq_head;

	ctx->ctx_msgq_head = (ctx->ctx_msgq_head+1) % PFM_MAX_MSGS;

	DPRINT(("ctx=%p head=%d tail=%d type=%d\n", ctx, ctx->ctx_msgq_head, ctx->ctx_msgq_tail, msg->pfm_gen_msg.msg_type));

	return msg;
}

static void
pfm_reset_msgq(pfm_context_t *ctx)
{
	ctx->ctx_msgq_head = ctx->ctx_msgq_tail = 0;
	DPRINT(("ctx=%p msgq reset\n", ctx));
}

static void *
pfm_rvmalloc(unsigned long size)
{
	void *mem;
	unsigned long addr;

	size = PAGE_ALIGN(size);
	mem  = vzalloc(size);
	if (mem) {
		
		addr = (unsigned long)mem;
		while (size > 0) {
			pfm_reserve_page(addr);
			addr+=PAGE_SIZE;
			size-=PAGE_SIZE;
		}
	}
	return mem;
}

static void
pfm_rvfree(void *mem, unsigned long size)
{
	unsigned long addr;

	if (mem) {
		DPRINT(("freeing physical buffer @%p size=%lu\n", mem, size));
		addr = (unsigned long) mem;
		while ((long) size > 0) {
			pfm_unreserve_page(addr);
			addr+=PAGE_SIZE;
			size-=PAGE_SIZE;
		}
		vfree(mem);
	}
	return;
}

static pfm_context_t *
pfm_context_alloc(int ctx_flags)
{
	pfm_context_t *ctx;

	ctx = kzalloc(sizeof(pfm_context_t), GFP_KERNEL);
	if (ctx) {
		DPRINT(("alloc ctx @%p\n", ctx));

		spin_lock_init(&ctx->ctx_lock);

		ctx->ctx_state = PFM_CTX_UNLOADED;

		ctx->ctx_fl_block       = (ctx_flags & PFM_FL_NOTIFY_BLOCK) ? 1 : 0;
		ctx->ctx_fl_system      = (ctx_flags & PFM_FL_SYSTEM_WIDE) ? 1: 0;
		ctx->ctx_fl_no_msg      = (ctx_flags & PFM_FL_OVFL_NO_MSG) ? 1: 0;

		init_completion(&ctx->ctx_restart_done);

		ctx->ctx_last_activation = PFM_INVALID_ACTIVATION;
		SET_LAST_CPU(ctx, -1);

		ctx->ctx_msgq_head = ctx->ctx_msgq_tail = 0;
		init_waitqueue_head(&ctx->ctx_msgq_wait);
		init_waitqueue_head(&ctx->ctx_zombieq);

	}
	return ctx;
}

static void
pfm_context_free(pfm_context_t *ctx)
{
	if (ctx) {
		DPRINT(("free ctx @%p\n", ctx));
		kfree(ctx);
	}
}

static void
pfm_mask_monitoring(struct task_struct *task)
{
	pfm_context_t *ctx = PFM_GET_CTX(task);
	unsigned long mask, val, ovfl_mask;
	int i;

	DPRINT_ovfl(("masking monitoring for [%d]\n", task_pid_nr(task)));

	ovfl_mask = pmu_conf->ovfl_val;
	mask = ctx->ctx_used_pmds[0];
	for (i = 0; mask; i++, mask>>=1) {
		
		if ((mask & 0x1) == 0) continue;
		val = ia64_get_pmd(i);

		if (PMD_IS_COUNTING(i)) {
			ctx->ctx_pmds[i].val += (val & ovfl_mask);
		} else {
			ctx->ctx_pmds[i].val = val;
		}
		DPRINT_ovfl(("pmd[%d]=0x%lx hw_pmd=0x%lx\n",
			i,
			ctx->ctx_pmds[i].val,
			val & ovfl_mask));
	}
	mask = ctx->ctx_used_monitors[0] >> PMU_FIRST_COUNTER;
	for(i= PMU_FIRST_COUNTER; mask; i++, mask>>=1) {
		if ((mask & 0x1) == 0UL) continue;
		ia64_set_pmc(i, ctx->th_pmcs[i] & ~0xfUL);
		ctx->th_pmcs[i] &= ~0xfUL;
		DPRINT_ovfl(("pmc[%d]=0x%lx\n", i, ctx->th_pmcs[i]));
	}
	ia64_srlz_d();
}

static void
pfm_restore_monitoring(struct task_struct *task)
{
	pfm_context_t *ctx = PFM_GET_CTX(task);
	unsigned long mask, ovfl_mask;
	unsigned long psr, val;
	int i, is_system;

	is_system = ctx->ctx_fl_system;
	ovfl_mask = pmu_conf->ovfl_val;

	if (task != current) {
		printk(KERN_ERR "perfmon.%d: invalid task[%d] current[%d]\n", __LINE__, task_pid_nr(task), task_pid_nr(current));
		return;
	}
	if (ctx->ctx_state != PFM_CTX_MASKED) {
		printk(KERN_ERR "perfmon.%d: task[%d] current[%d] invalid state=%d\n", __LINE__,
			task_pid_nr(task), task_pid_nr(current), ctx->ctx_state);
		return;
	}
	psr = pfm_get_psr();
	if (is_system && (PFM_CPUINFO_GET() & PFM_CPUINFO_DCR_PP)) {
		
		ia64_setreg(_IA64_REG_CR_DCR, ia64_getreg(_IA64_REG_CR_DCR) & ~IA64_DCR_PP);
		pfm_clear_psr_pp();
	} else {
		pfm_clear_psr_up();
	}
	mask = ctx->ctx_used_pmds[0];
	for (i = 0; mask; i++, mask>>=1) {
		
		if ((mask & 0x1) == 0) continue;

		if (PMD_IS_COUNTING(i)) {
			val = ctx->ctx_pmds[i].val & ovfl_mask;
			ctx->ctx_pmds[i].val &= ~ovfl_mask;
		} else {
			val = ctx->ctx_pmds[i].val;
		}
		ia64_set_pmd(i, val);

		DPRINT(("pmd[%d]=0x%lx hw_pmd=0x%lx\n",
			i,
			ctx->ctx_pmds[i].val,
			val));
	}
	mask = ctx->ctx_used_monitors[0] >> PMU_FIRST_COUNTER;
	for(i= PMU_FIRST_COUNTER; mask; i++, mask>>=1) {
		if ((mask & 0x1) == 0UL) continue;
		ctx->th_pmcs[i] = ctx->ctx_pmcs[i];
		ia64_set_pmc(i, ctx->th_pmcs[i]);
		DPRINT(("[%d] pmc[%d]=0x%lx\n",
					task_pid_nr(task), i, ctx->th_pmcs[i]));
	}
	ia64_srlz_d();

	if (ctx->ctx_fl_using_dbreg) {
		pfm_restore_ibrs(ctx->ctx_ibrs, pmu_conf->num_ibrs);
		pfm_restore_dbrs(ctx->ctx_dbrs, pmu_conf->num_dbrs);
	}

	if (is_system && (PFM_CPUINFO_GET() & PFM_CPUINFO_DCR_PP)) {
		
		ia64_setreg(_IA64_REG_CR_DCR, ia64_getreg(_IA64_REG_CR_DCR) | IA64_DCR_PP);
		ia64_srlz_i();
	}
	pfm_set_psr_l(psr);
}

static inline void
pfm_save_pmds(unsigned long *pmds, unsigned long mask)
{
	int i;

	ia64_srlz_d();

	for (i=0; mask; i++, mask>>=1) {
		if (mask & 0x1) pmds[i] = ia64_get_pmd(i);
	}
}

static inline void
pfm_restore_pmds(unsigned long *pmds, unsigned long mask)
{
	int i;
	unsigned long val, ovfl_val = pmu_conf->ovfl_val;

	for (i=0; mask; i++, mask>>=1) {
		if ((mask & 0x1) == 0) continue;
		val = PMD_IS_COUNTING(i) ? pmds[i] & ovfl_val : pmds[i];
		ia64_set_pmd(i, val);
	}
	ia64_srlz_d();
}

static inline void
pfm_copy_pmds(struct task_struct *task, pfm_context_t *ctx)
{
	unsigned long ovfl_val = pmu_conf->ovfl_val;
	unsigned long mask = ctx->ctx_all_pmds[0];
	unsigned long val;
	int i;

	DPRINT(("mask=0x%lx\n", mask));

	for (i=0; mask; i++, mask>>=1) {

		val = ctx->ctx_pmds[i].val;

		if (PMD_IS_COUNTING(i)) {
			ctx->ctx_pmds[i].val = val & ~ovfl_val;
			 val &= ovfl_val;
		}
		ctx->th_pmds[i] = val;

		DPRINT(("pmd[%d]=0x%lx soft_val=0x%lx\n",
			i,
			ctx->th_pmds[i],
			ctx->ctx_pmds[i].val));
	}
}

static inline void
pfm_copy_pmcs(struct task_struct *task, pfm_context_t *ctx)
{
	unsigned long mask = ctx->ctx_all_pmcs[0];
	int i;

	DPRINT(("mask=0x%lx\n", mask));

	for (i=0; mask; i++, mask>>=1) {
		
		ctx->th_pmcs[i] = ctx->ctx_pmcs[i];
		DPRINT(("pmc[%d]=0x%lx\n", i, ctx->th_pmcs[i]));
	}
}



static inline void
pfm_restore_pmcs(unsigned long *pmcs, unsigned long mask)
{
	int i;

	for (i=0; mask; i++, mask>>=1) {
		if ((mask & 0x1) == 0) continue;
		ia64_set_pmc(i, pmcs[i]);
	}
	ia64_srlz_d();
}

static inline int
pfm_uuid_cmp(pfm_uuid_t a, pfm_uuid_t b)
{
	return memcmp(a, b, sizeof(pfm_uuid_t));
}

static inline int
pfm_buf_fmt_exit(pfm_buffer_fmt_t *fmt, struct task_struct *task, void *buf, struct pt_regs *regs)
{
	int ret = 0;
	if (fmt->fmt_exit) ret = (*fmt->fmt_exit)(task, buf, regs);
	return ret;
}

static inline int
pfm_buf_fmt_getsize(pfm_buffer_fmt_t *fmt, struct task_struct *task, unsigned int flags, int cpu, void *arg, unsigned long *size)
{
	int ret = 0;
	if (fmt->fmt_getsize) ret = (*fmt->fmt_getsize)(task, flags, cpu, arg, size);
	return ret;
}


static inline int
pfm_buf_fmt_validate(pfm_buffer_fmt_t *fmt, struct task_struct *task, unsigned int flags,
		     int cpu, void *arg)
{
	int ret = 0;
	if (fmt->fmt_validate) ret = (*fmt->fmt_validate)(task, flags, cpu, arg);
	return ret;
}

static inline int
pfm_buf_fmt_init(pfm_buffer_fmt_t *fmt, struct task_struct *task, void *buf, unsigned int flags,
		     int cpu, void *arg)
{
	int ret = 0;
	if (fmt->fmt_init) ret = (*fmt->fmt_init)(task, buf, flags, cpu, arg);
	return ret;
}

static inline int
pfm_buf_fmt_restart(pfm_buffer_fmt_t *fmt, struct task_struct *task, pfm_ovfl_ctrl_t *ctrl, void *buf, struct pt_regs *regs)
{
	int ret = 0;
	if (fmt->fmt_restart) ret = (*fmt->fmt_restart)(task, ctrl, buf, regs);
	return ret;
}

static inline int
pfm_buf_fmt_restart_active(pfm_buffer_fmt_t *fmt, struct task_struct *task, pfm_ovfl_ctrl_t *ctrl, void *buf, struct pt_regs *regs)
{
	int ret = 0;
	if (fmt->fmt_restart_active) ret = (*fmt->fmt_restart_active)(task, ctrl, buf, regs);
	return ret;
}

static pfm_buffer_fmt_t *
__pfm_find_buffer_fmt(pfm_uuid_t uuid)
{
	struct list_head * pos;
	pfm_buffer_fmt_t * entry;

	list_for_each(pos, &pfm_buffer_fmt_list) {
		entry = list_entry(pos, pfm_buffer_fmt_t, fmt_list);
		if (pfm_uuid_cmp(uuid, entry->fmt_uuid) == 0)
			return entry;
	}
	return NULL;
}
 
static pfm_buffer_fmt_t *
pfm_find_buffer_fmt(pfm_uuid_t uuid)
{
	pfm_buffer_fmt_t * fmt;
	spin_lock(&pfm_buffer_fmt_lock);
	fmt = __pfm_find_buffer_fmt(uuid);
	spin_unlock(&pfm_buffer_fmt_lock);
	return fmt;
}
 
int
pfm_register_buffer_fmt(pfm_buffer_fmt_t *fmt)
{
	int ret = 0;

	
	if (fmt == NULL || fmt->fmt_name == NULL) return -EINVAL;

	
	if (fmt->fmt_handler == NULL) return -EINVAL;


	spin_lock(&pfm_buffer_fmt_lock);

	if (__pfm_find_buffer_fmt(fmt->fmt_uuid)) {
		printk(KERN_ERR "perfmon: duplicate sampling format: %s\n", fmt->fmt_name);
		ret = -EBUSY;
		goto out;
	} 
	list_add(&fmt->fmt_list, &pfm_buffer_fmt_list);
	printk(KERN_INFO "perfmon: added sampling format %s\n", fmt->fmt_name);

out:
	spin_unlock(&pfm_buffer_fmt_lock);
 	return ret;
}
EXPORT_SYMBOL(pfm_register_buffer_fmt);

int
pfm_unregister_buffer_fmt(pfm_uuid_t uuid)
{
	pfm_buffer_fmt_t *fmt;
	int ret = 0;

	spin_lock(&pfm_buffer_fmt_lock);

	fmt = __pfm_find_buffer_fmt(uuid);
	if (!fmt) {
		printk(KERN_ERR "perfmon: cannot unregister format, not found\n");
		ret = -EINVAL;
		goto out;
	}
	list_del_init(&fmt->fmt_list);
	printk(KERN_INFO "perfmon: removed sampling format: %s\n", fmt->fmt_name);

out:
	spin_unlock(&pfm_buffer_fmt_lock);
	return ret;

}
EXPORT_SYMBOL(pfm_unregister_buffer_fmt);

extern void update_pal_halt_status(int);

static int
pfm_reserve_session(struct task_struct *task, int is_syswide, unsigned int cpu)
{
	unsigned long flags;
	LOCK_PFS(flags);

	DPRINT(("in sys_sessions=%u task_sessions=%u dbregs=%u syswide=%d cpu=%u\n",
		pfm_sessions.pfs_sys_sessions,
		pfm_sessions.pfs_task_sessions,
		pfm_sessions.pfs_sys_use_dbregs,
		is_syswide,
		cpu));

	if (is_syswide) {
		if (pfm_sessions.pfs_task_sessions > 0UL) {
			DPRINT(("system wide not possible, %u conflicting task_sessions\n",
			  	pfm_sessions.pfs_task_sessions));
			goto abort;
		}

		if (pfm_sessions.pfs_sys_session[cpu]) goto error_conflict;

		DPRINT(("reserving system wide session on CPU%u currently on CPU%u\n", cpu, smp_processor_id()));

		pfm_sessions.pfs_sys_session[cpu] = task;

		pfm_sessions.pfs_sys_sessions++ ;

	} else {
		if (pfm_sessions.pfs_sys_sessions) goto abort;
		pfm_sessions.pfs_task_sessions++;
	}

	DPRINT(("out sys_sessions=%u task_sessions=%u dbregs=%u syswide=%d cpu=%u\n",
		pfm_sessions.pfs_sys_sessions,
		pfm_sessions.pfs_task_sessions,
		pfm_sessions.pfs_sys_use_dbregs,
		is_syswide,
		cpu));

	update_pal_halt_status(0);

	UNLOCK_PFS(flags);

	return 0;

error_conflict:
	DPRINT(("system wide not possible, conflicting session [%d] on CPU%d\n",
  		task_pid_nr(pfm_sessions.pfs_sys_session[cpu]),
		cpu));
abort:
	UNLOCK_PFS(flags);

	return -EBUSY;

}

static int
pfm_unreserve_session(pfm_context_t *ctx, int is_syswide, unsigned int cpu)
{
	unsigned long flags;
	LOCK_PFS(flags);

	DPRINT(("in sys_sessions=%u task_sessions=%u dbregs=%u syswide=%d cpu=%u\n",
		pfm_sessions.pfs_sys_sessions,
		pfm_sessions.pfs_task_sessions,
		pfm_sessions.pfs_sys_use_dbregs,
		is_syswide,
		cpu));


	if (is_syswide) {
		pfm_sessions.pfs_sys_session[cpu] = NULL;
		if (ctx && ctx->ctx_fl_using_dbreg) {
			if (pfm_sessions.pfs_sys_use_dbregs == 0) {
				printk(KERN_ERR "perfmon: invalid release for ctx %p sys_use_dbregs=0\n", ctx);
			} else {
				pfm_sessions.pfs_sys_use_dbregs--;
			}
		}
		pfm_sessions.pfs_sys_sessions--;
	} else {
		pfm_sessions.pfs_task_sessions--;
	}
	DPRINT(("out sys_sessions=%u task_sessions=%u dbregs=%u syswide=%d cpu=%u\n",
		pfm_sessions.pfs_sys_sessions,
		pfm_sessions.pfs_task_sessions,
		pfm_sessions.pfs_sys_use_dbregs,
		is_syswide,
		cpu));

	if (pfm_sessions.pfs_task_sessions == 0 && pfm_sessions.pfs_sys_sessions == 0)
		update_pal_halt_status(1);

	UNLOCK_PFS(flags);

	return 0;
}

static int
pfm_remove_smpl_mapping(void *vaddr, unsigned long size)
{
	struct task_struct *task = current;
	int r;

	
	if (task->mm == NULL || size == 0UL || vaddr == NULL) {
		printk(KERN_ERR "perfmon: pfm_remove_smpl_mapping [%d] invalid context mm=%p\n", task_pid_nr(task), task->mm);
		return -EINVAL;
	}

	DPRINT(("smpl_vaddr=%p size=%lu\n", vaddr, size));

	r = vm_munmap((unsigned long)vaddr, size);

	if (r !=0) {
		printk(KERN_ERR "perfmon: [%d] unable to unmap sampling buffer @%p size=%lu\n", task_pid_nr(task), vaddr, size);
	}

	DPRINT(("do_unmap(%p, %lu)=%d\n", vaddr, size, r));

	return 0;
}

#if 0
static int
pfm_free_smpl_buffer(pfm_context_t *ctx)
{
	pfm_buffer_fmt_t *fmt;

	if (ctx->ctx_smpl_hdr == NULL) goto invalid_free;

	fmt = ctx->ctx_buf_fmt;

	DPRINT(("sampling buffer @%p size %lu vaddr=%p\n",
		ctx->ctx_smpl_hdr,
		ctx->ctx_smpl_size,
		ctx->ctx_smpl_vaddr));

	pfm_buf_fmt_exit(fmt, current, NULL, NULL);

	pfm_rvfree(ctx->ctx_smpl_hdr, ctx->ctx_smpl_size);

	ctx->ctx_smpl_hdr  = NULL;
	ctx->ctx_smpl_size = 0UL;

	return 0;

invalid_free:
	printk(KERN_ERR "perfmon: pfm_free_smpl_buffer [%d] no buffer\n", task_pid_nr(current));
	return -EINVAL;
}
#endif

static inline void
pfm_exit_smpl_buffer(pfm_buffer_fmt_t *fmt)
{
	if (fmt == NULL) return;

	pfm_buf_fmt_exit(fmt, current, NULL, NULL);

}

static struct vfsmount *pfmfs_mnt __read_mostly;

static int __init
init_pfm_fs(void)
{
	int err = register_filesystem(&pfm_fs_type);
	if (!err) {
		pfmfs_mnt = kern_mount(&pfm_fs_type);
		err = PTR_ERR(pfmfs_mnt);
		if (IS_ERR(pfmfs_mnt))
			unregister_filesystem(&pfm_fs_type);
		else
			err = 0;
	}
	return err;
}

static ssize_t
pfm_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
	pfm_context_t *ctx;
	pfm_msg_t *msg;
	ssize_t ret;
	unsigned long flags;
  	DECLARE_WAITQUEUE(wait, current);
	if (PFM_IS_FILE(filp) == 0) {
		printk(KERN_ERR "perfmon: pfm_poll: bad magic [%d]\n", task_pid_nr(current));
		return -EINVAL;
	}

	ctx = filp->private_data;
	if (ctx == NULL) {
		printk(KERN_ERR "perfmon: pfm_read: NULL ctx [%d]\n", task_pid_nr(current));
		return -EINVAL;
	}

	if (size < sizeof(pfm_msg_t)) {
		DPRINT(("message is too small ctx=%p (>=%ld)\n", ctx, sizeof(pfm_msg_t)));
		return -EINVAL;
	}

	PROTECT_CTX(ctx, flags);

  	add_wait_queue(&ctx->ctx_msgq_wait, &wait);


  	for(;;) {

  		set_current_state(TASK_INTERRUPTIBLE);

		DPRINT(("head=%d tail=%d\n", ctx->ctx_msgq_head, ctx->ctx_msgq_tail));

		ret = 0;
		if(PFM_CTXQ_EMPTY(ctx) == 0) break;

		UNPROTECT_CTX(ctx, flags);

      		ret = -EAGAIN;
		if(filp->f_flags & O_NONBLOCK) break;

		if(signal_pending(current)) {
			ret = -EINTR;
			break;
		}
      		schedule();

		PROTECT_CTX(ctx, flags);
	}
	DPRINT(("[%d] back to running ret=%ld\n", task_pid_nr(current), ret));
  	set_current_state(TASK_RUNNING);
	remove_wait_queue(&ctx->ctx_msgq_wait, &wait);

	if (ret < 0) goto abort;

	ret = -EINVAL;
	msg = pfm_get_next_msg(ctx);
	if (msg == NULL) {
		printk(KERN_ERR "perfmon: pfm_read no msg for ctx=%p [%d]\n", ctx, task_pid_nr(current));
		goto abort_locked;
	}

	DPRINT(("fd=%d type=%d\n", msg->pfm_gen_msg.msg_ctx_fd, msg->pfm_gen_msg.msg_type));

	ret = -EFAULT;
  	if(copy_to_user(buf, msg, sizeof(pfm_msg_t)) == 0) ret = sizeof(pfm_msg_t);

abort_locked:
	UNPROTECT_CTX(ctx, flags);
abort:
	return ret;
}

static ssize_t
pfm_write(struct file *file, const char __user *ubuf,
			  size_t size, loff_t *ppos)
{
	DPRINT(("pfm_write called\n"));
	return -EINVAL;
}

static unsigned int
pfm_poll(struct file *filp, poll_table * wait)
{
	pfm_context_t *ctx;
	unsigned long flags;
	unsigned int mask = 0;

	if (PFM_IS_FILE(filp) == 0) {
		printk(KERN_ERR "perfmon: pfm_poll: bad magic [%d]\n", task_pid_nr(current));
		return 0;
	}

	ctx = filp->private_data;
	if (ctx == NULL) {
		printk(KERN_ERR "perfmon: pfm_poll: NULL ctx [%d]\n", task_pid_nr(current));
		return 0;
	}


	DPRINT(("pfm_poll ctx_fd=%d before poll_wait\n", ctx->ctx_fd));

	poll_wait(filp, &ctx->ctx_msgq_wait, wait);

	PROTECT_CTX(ctx, flags);

	if (PFM_CTXQ_EMPTY(ctx) == 0)
		mask =  POLLIN | POLLRDNORM;

	UNPROTECT_CTX(ctx, flags);

	DPRINT(("pfm_poll ctx_fd=%d mask=0x%x\n", ctx->ctx_fd, mask));

	return mask;
}

static long
pfm_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	DPRINT(("pfm_ioctl called\n"));
	return -EINVAL;
}

static inline int
pfm_do_fasync(int fd, struct file *filp, pfm_context_t *ctx, int on)
{
	int ret;

	ret = fasync_helper (fd, filp, on, &ctx->ctx_async_queue);

	DPRINT(("pfm_fasync called by [%d] on ctx_fd=%d on=%d async_queue=%p ret=%d\n",
		task_pid_nr(current),
		fd,
		on,
		ctx->ctx_async_queue, ret));

	return ret;
}

static int
pfm_fasync(int fd, struct file *filp, int on)
{
	pfm_context_t *ctx;
	int ret;

	if (PFM_IS_FILE(filp) == 0) {
		printk(KERN_ERR "perfmon: pfm_fasync bad magic [%d]\n", task_pid_nr(current));
		return -EBADF;
	}

	ctx = filp->private_data;
	if (ctx == NULL) {
		printk(KERN_ERR "perfmon: pfm_fasync NULL ctx [%d]\n", task_pid_nr(current));
		return -EBADF;
	}
	ret = pfm_do_fasync(fd, filp, ctx, on);


	DPRINT(("pfm_fasync called on ctx_fd=%d on=%d async_queue=%p ret=%d\n",
		fd,
		on,
		ctx->ctx_async_queue, ret));

	return ret;
}

#ifdef CONFIG_SMP
static void
pfm_syswide_force_stop(void *info)
{
	pfm_context_t   *ctx = (pfm_context_t *)info;
	struct pt_regs *regs = task_pt_regs(current);
	struct task_struct *owner;
	unsigned long flags;
	int ret;

	if (ctx->ctx_cpu != smp_processor_id()) {
		printk(KERN_ERR "perfmon: pfm_syswide_force_stop for CPU%d  but on CPU%d\n",
			ctx->ctx_cpu,
			smp_processor_id());
		return;
	}
	owner = GET_PMU_OWNER();
	if (owner != ctx->ctx_task) {
		printk(KERN_ERR "perfmon: pfm_syswide_force_stop CPU%d unexpected owner [%d] instead of [%d]\n",
			smp_processor_id(),
			task_pid_nr(owner), task_pid_nr(ctx->ctx_task));
		return;
	}
	if (GET_PMU_CTX() != ctx) {
		printk(KERN_ERR "perfmon: pfm_syswide_force_stop CPU%d unexpected ctx %p instead of %p\n",
			smp_processor_id(),
			GET_PMU_CTX(), ctx);
		return;
	}

	DPRINT(("on CPU%d forcing system wide stop for [%d]\n", smp_processor_id(), task_pid_nr(ctx->ctx_task)));
	local_irq_save(flags);

	ret = pfm_context_unload(ctx, NULL, 0, regs);
	if (ret) {
		DPRINT(("context_unload returned %d\n", ret));
	}

	local_irq_restore(flags);
}

static void
pfm_syswide_cleanup_other_cpu(pfm_context_t *ctx)
{
	int ret;

	DPRINT(("calling CPU%d for cleanup\n", ctx->ctx_cpu));
	ret = smp_call_function_single(ctx->ctx_cpu, pfm_syswide_force_stop, ctx, 1);
	DPRINT(("called CPU%d for cleanup ret=%d\n", ctx->ctx_cpu, ret));
}
#endif 

static int
pfm_flush(struct file *filp, fl_owner_t id)
{
	pfm_context_t *ctx;
	struct task_struct *task;
	struct pt_regs *regs;
	unsigned long flags;
	unsigned long smpl_buf_size = 0UL;
	void *smpl_buf_vaddr = NULL;
	int state, is_system;

	if (PFM_IS_FILE(filp) == 0) {
		DPRINT(("bad magic for\n"));
		return -EBADF;
	}

	ctx = filp->private_data;
	if (ctx == NULL) {
		printk(KERN_ERR "perfmon: pfm_flush: NULL ctx [%d]\n", task_pid_nr(current));
		return -EBADF;
	}

	PROTECT_CTX(ctx, flags);

	state     = ctx->ctx_state;
	is_system = ctx->ctx_fl_system;

	task = PFM_CTX_TASK(ctx);
	regs = task_pt_regs(task);

	DPRINT(("ctx_state=%d is_current=%d\n",
		state,
		task == current ? 1 : 0));


	if (task == current) {
#ifdef CONFIG_SMP
		if (is_system && ctx->ctx_cpu != smp_processor_id()) {

			DPRINT(("should be running on CPU%d\n", ctx->ctx_cpu));
			local_irq_restore(flags);

			pfm_syswide_cleanup_other_cpu(ctx);

			local_irq_save(flags);

		} else
#endif 
		{

			DPRINT(("forcing unload\n"));
			pfm_context_unload(ctx, NULL, 0, regs);

			DPRINT(("ctx_state=%d\n", ctx->ctx_state));
		}
	}

	if (ctx->ctx_smpl_vaddr && current->mm) {
		smpl_buf_vaddr = ctx->ctx_smpl_vaddr;
		smpl_buf_size  = ctx->ctx_smpl_size;
	}

	UNPROTECT_CTX(ctx, flags);

	if (smpl_buf_vaddr) pfm_remove_smpl_mapping(smpl_buf_vaddr, smpl_buf_size);

	return 0;
}
static int
pfm_close(struct inode *inode, struct file *filp)
{
	pfm_context_t *ctx;
	struct task_struct *task;
	struct pt_regs *regs;
  	DECLARE_WAITQUEUE(wait, current);
	unsigned long flags;
	unsigned long smpl_buf_size = 0UL;
	void *smpl_buf_addr = NULL;
	int free_possible = 1;
	int state, is_system;

	DPRINT(("pfm_close called private=%p\n", filp->private_data));

	if (PFM_IS_FILE(filp) == 0) {
		DPRINT(("bad magic\n"));
		return -EBADF;
	}
	
	ctx = filp->private_data;
	if (ctx == NULL) {
		printk(KERN_ERR "perfmon: pfm_close: NULL ctx [%d]\n", task_pid_nr(current));
		return -EBADF;
	}

	PROTECT_CTX(ctx, flags);

	state     = ctx->ctx_state;
	is_system = ctx->ctx_fl_system;

	task = PFM_CTX_TASK(ctx);
	regs = task_pt_regs(task);

	DPRINT(("ctx_state=%d is_current=%d\n", 
		state,
		task == current ? 1 : 0));

	if (state == PFM_CTX_UNLOADED) goto doit;


	if (state == PFM_CTX_MASKED && CTX_OVFL_NOBLOCK(ctx) == 0) {

		ctx->ctx_fl_going_zombie = 1;

		complete(&ctx->ctx_restart_done);

		DPRINT(("waking up ctx_state=%d\n", state));

  		set_current_state(TASK_INTERRUPTIBLE);
  		add_wait_queue(&ctx->ctx_zombieq, &wait);

		UNPROTECT_CTX(ctx, flags);

      		schedule();


		PROTECT_CTX(ctx, flags);


		remove_wait_queue(&ctx->ctx_zombieq, &wait);
  		set_current_state(TASK_RUNNING);

		DPRINT(("after zombie wakeup ctx_state=%d for\n", state));
	}
	else if (task != current) {
#ifdef CONFIG_SMP
		ctx->ctx_state = PFM_CTX_ZOMBIE;

		DPRINT(("zombie ctx for [%d]\n", task_pid_nr(task)));
		free_possible = 0;
#else
		pfm_context_unload(ctx, NULL, 0, regs);
#endif
	}

doit:
	
	state = ctx->ctx_state;


	if (ctx->ctx_smpl_hdr) {
		smpl_buf_addr = ctx->ctx_smpl_hdr;
		smpl_buf_size = ctx->ctx_smpl_size;
		
		ctx->ctx_smpl_hdr = NULL;
		ctx->ctx_fl_is_sampling = 0;
	}

	DPRINT(("ctx_state=%d free_possible=%d addr=%p size=%lu\n",
		state,
		free_possible,
		smpl_buf_addr,
		smpl_buf_size));

	if (smpl_buf_addr) pfm_exit_smpl_buffer(ctx->ctx_buf_fmt);

	if (state == PFM_CTX_ZOMBIE) {
		pfm_unreserve_session(ctx, ctx->ctx_fl_system , ctx->ctx_cpu);
	}

	filp->private_data = NULL;

	UNPROTECT_CTX(ctx, flags);

	if (smpl_buf_addr)  pfm_rvfree(smpl_buf_addr, smpl_buf_size);

	if (free_possible) pfm_context_free(ctx);

	return 0;
}

static int
pfm_no_open(struct inode *irrelevant, struct file *dontcare)
{
	DPRINT(("pfm_no_open called\n"));
	return -ENXIO;
}



static const struct file_operations pfm_file_ops = {
	.llseek		= no_llseek,
	.read		= pfm_read,
	.write		= pfm_write,
	.poll		= pfm_poll,
	.unlocked_ioctl = pfm_ioctl,
	.open		= pfm_no_open,	
	.fasync		= pfm_fasync,
	.release	= pfm_close,
	.flush		= pfm_flush
};

static int
pfmfs_delete_dentry(const struct dentry *dentry)
{
	return 1;
}

static char *pfmfs_dname(struct dentry *dentry, char *buffer, int buflen)
{
	return dynamic_dname(dentry, buffer, buflen, "pfm:[%lu]",
			     dentry->d_inode->i_ino);
}

static const struct dentry_operations pfmfs_dentry_operations = {
	.d_delete = pfmfs_delete_dentry,
	.d_dname = pfmfs_dname,
};


static struct file *
pfm_alloc_file(pfm_context_t *ctx)
{
	struct file *file;
	struct inode *inode;
	struct path path;
	struct qstr this = { .name = "" };

	inode = new_inode(pfmfs_mnt->mnt_sb);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	DPRINT(("new inode ino=%ld @%p\n", inode->i_ino, inode));

	inode->i_mode = S_IFCHR|S_IRUGO;
	inode->i_uid  = current_fsuid();
	inode->i_gid  = current_fsgid();

	path.dentry = d_alloc(pfmfs_mnt->mnt_root, &this);
	if (!path.dentry) {
		iput(inode);
		return ERR_PTR(-ENOMEM);
	}
	path.mnt = mntget(pfmfs_mnt);

	d_add(path.dentry, inode);

	file = alloc_file(&path, FMODE_READ, &pfm_file_ops);
	if (!file) {
		path_put(&path);
		return ERR_PTR(-ENFILE);
	}

	file->f_flags = O_RDONLY;
	file->private_data = ctx;

	return file;
}

static int
pfm_remap_buffer(struct vm_area_struct *vma, unsigned long buf, unsigned long addr, unsigned long size)
{
	DPRINT(("CPU%d buf=0x%lx addr=0x%lx size=%ld\n", smp_processor_id(), buf, addr, size));

	while (size > 0) {
		unsigned long pfn = ia64_tpa(buf) >> PAGE_SHIFT;


		if (remap_pfn_range(vma, addr, pfn, PAGE_SIZE, PAGE_READONLY))
			return -ENOMEM;

		addr  += PAGE_SIZE;
		buf   += PAGE_SIZE;
		size  -= PAGE_SIZE;
	}
	return 0;
}

static int
pfm_smpl_buffer_alloc(struct task_struct *task, struct file *filp, pfm_context_t *ctx, unsigned long rsize, void **user_vaddr)
{
	struct mm_struct *mm = task->mm;
	struct vm_area_struct *vma = NULL;
	unsigned long size;
	void *smpl_buf;


	size = PAGE_ALIGN(rsize);

	DPRINT(("sampling buffer rsize=%lu size=%lu bytes\n", rsize, size));

	if (size > task_rlimit(task, RLIMIT_MEMLOCK))
		return -ENOMEM;

	smpl_buf = pfm_rvmalloc(size);
	if (smpl_buf == NULL) {
		DPRINT(("Can't allocate sampling buffer\n"));
		return -ENOMEM;
	}

	DPRINT(("smpl_buf @%p\n", smpl_buf));

	
	vma = kmem_cache_zalloc(vm_area_cachep, GFP_KERNEL);
	if (!vma) {
		DPRINT(("Cannot allocate vma\n"));
		goto error_kmem;
	}
	INIT_LIST_HEAD(&vma->anon_vma_chain);

	vma->vm_mm	     = mm;
	vma->vm_file	     = filp;
	vma->vm_flags	     = VM_READ| VM_MAYREAD |VM_RESERVED;
	vma->vm_page_prot    = PAGE_READONLY; 


	ctx->ctx_smpl_hdr   = smpl_buf;
	ctx->ctx_smpl_size  = size; 

	down_write(&task->mm->mmap_sem);

	
	vma->vm_start = pfm_get_unmapped_area(NULL, 0, size, 0, MAP_PRIVATE|MAP_ANONYMOUS, 0);
	if (vma->vm_start == 0UL) {
		DPRINT(("Cannot find unmapped area for size %ld\n", size));
		up_write(&task->mm->mmap_sem);
		goto error;
	}
	vma->vm_end = vma->vm_start + size;
	vma->vm_pgoff = vma->vm_start >> PAGE_SHIFT;

	DPRINT(("aligned size=%ld, hdr=%p mapped @0x%lx\n", size, ctx->ctx_smpl_hdr, vma->vm_start));

	
	if (pfm_remap_buffer(vma, (unsigned long)smpl_buf, vma->vm_start, size)) {
		DPRINT(("Can't remap buffer\n"));
		up_write(&task->mm->mmap_sem);
		goto error;
	}

	get_file(filp);

	insert_vm_struct(mm, vma);

	mm->total_vm  += size >> PAGE_SHIFT;
	vm_stat_account(vma->vm_mm, vma->vm_flags, vma->vm_file,
							vma_pages(vma));
	up_write(&task->mm->mmap_sem);

	ctx->ctx_smpl_vaddr = (void *)vma->vm_start;
	*(unsigned long *)user_vaddr = vma->vm_start;

	return 0;

error:
	kmem_cache_free(vm_area_cachep, vma);
error_kmem:
	pfm_rvfree(smpl_buf, size);

	return -ENOMEM;
}

static int
pfm_bad_permissions(struct task_struct *task)
{
	const struct cred *tcred;
	uid_t uid = current_uid();
	gid_t gid = current_gid();
	int ret;

	rcu_read_lock();
	tcred = __task_cred(task);

	
	DPRINT(("cur: uid=%d gid=%d task: euid=%d suid=%d uid=%d egid=%d sgid=%d\n",
		uid,
		gid,
		tcred->euid,
		tcred->suid,
		tcred->uid,
		tcred->egid,
		tcred->sgid));

	ret = ((uid != tcred->euid)
	       || (uid != tcred->suid)
	       || (uid != tcred->uid)
	       || (gid != tcred->egid)
	       || (gid != tcred->sgid)
	       || (gid != tcred->gid)) && !capable(CAP_SYS_PTRACE);

	rcu_read_unlock();
	return ret;
}

static int
pfarg_is_sane(struct task_struct *task, pfarg_context_t *pfx)
{
	int ctx_flags;

	

	ctx_flags = pfx->ctx_flags;

	if (ctx_flags & PFM_FL_SYSTEM_WIDE) {

		if (ctx_flags & PFM_FL_NOTIFY_BLOCK) {
			DPRINT(("cannot use blocking mode when in system wide monitoring\n"));
			return -EINVAL;
		}
	} else {
	}
	

	return 0;
}

static int
pfm_setup_buffer_fmt(struct task_struct *task, struct file *filp, pfm_context_t *ctx, unsigned int ctx_flags,
		     unsigned int cpu, pfarg_context_t *arg)
{
	pfm_buffer_fmt_t *fmt = NULL;
	unsigned long size = 0UL;
	void *uaddr = NULL;
	void *fmt_arg = NULL;
	int ret = 0;
#define PFM_CTXARG_BUF_ARG(a)	(pfm_buffer_fmt_t *)(a+1)

	
	fmt = pfm_find_buffer_fmt(arg->ctx_smpl_buf_id);
	if (fmt == NULL) {
		DPRINT(("[%d] cannot find buffer format\n", task_pid_nr(task)));
		return -EINVAL;
	}

	if (fmt->fmt_arg_size) fmt_arg = PFM_CTXARG_BUF_ARG(arg);

	ret = pfm_buf_fmt_validate(fmt, task, ctx_flags, cpu, fmt_arg);

	DPRINT(("[%d] after validate(0x%x,%d,%p)=%d\n", task_pid_nr(task), ctx_flags, cpu, fmt_arg, ret));

	if (ret) goto error;

	
	ctx->ctx_buf_fmt = fmt;
	ctx->ctx_fl_is_sampling = 1; 

	ret = pfm_buf_fmt_getsize(fmt, task, ctx_flags, cpu, fmt_arg, &size);
	if (ret) goto error;

	if (size) {
		ret = pfm_smpl_buffer_alloc(current, filp, ctx, size, &uaddr);
		if (ret) goto error;

		
		arg->ctx_smpl_vaddr = uaddr;
	}
	ret = pfm_buf_fmt_init(fmt, task, ctx->ctx_smpl_hdr, ctx_flags, cpu, fmt_arg);

error:
	return ret;
}

static void
pfm_reset_pmu_state(pfm_context_t *ctx)
{
	int i;

	for (i=1; PMC_IS_LAST(i) == 0; i++) {
		if (PMC_IS_IMPL(i) == 0) continue;
		ctx->ctx_pmcs[i] = PMC_DFL_VAL(i);
		DPRINT(("pmc[%d]=0x%lx\n", i, ctx->ctx_pmcs[i]));
	}


	ctx->ctx_all_pmcs[0] = pmu_conf->impl_pmcs[0] & ~0x1;

	ctx->ctx_all_pmds[0] = pmu_conf->impl_pmds[0];

	DPRINT(("<%d> all_pmcs=0x%lx all_pmds=0x%lx\n", ctx->ctx_fd, ctx->ctx_all_pmcs[0],ctx->ctx_all_pmds[0]));

	ctx->ctx_used_ibrs[0] = 0UL;
	ctx->ctx_used_dbrs[0] = 0UL;
}

static int
pfm_ctx_getsize(void *arg, size_t *sz)
{
	pfarg_context_t *req = (pfarg_context_t *)arg;
	pfm_buffer_fmt_t *fmt;

	*sz = 0;

	if (!pfm_uuid_cmp(req->ctx_smpl_buf_id, pfm_null_uuid)) return 0;

	fmt = pfm_find_buffer_fmt(req->ctx_smpl_buf_id);
	if (fmt == NULL) {
		DPRINT(("cannot find buffer format\n"));
		return -EINVAL;
	}
	
	*sz = fmt->fmt_arg_size;
	DPRINT(("arg_size=%lu\n", *sz));

	return 0;
}



static int
pfm_task_incompatible(pfm_context_t *ctx, struct task_struct *task)
{
	if (task->mm == NULL) {
		DPRINT(("task [%d] has not memory context (kernel thread)\n", task_pid_nr(task)));
		return -EPERM;
	}
	if (pfm_bad_permissions(task)) {
		DPRINT(("no permission to attach to  [%d]\n", task_pid_nr(task)));
		return -EPERM;
	}
	if (CTX_OVFL_NOBLOCK(ctx) == 0 && task == current) {
		DPRINT(("cannot load a blocking context on self for [%d]\n", task_pid_nr(task)));
		return -EINVAL;
	}

	if (task->exit_state == EXIT_ZOMBIE) {
		DPRINT(("cannot attach to  zombie task [%d]\n", task_pid_nr(task)));
		return -EBUSY;
	}

	if (task == current) return 0;

	if (!task_is_stopped_or_traced(task)) {
		DPRINT(("cannot attach to non-stopped task [%d] state=%ld\n", task_pid_nr(task), task->state));
		return -EBUSY;
	}
	wait_task_inactive(task, 0);

	

	return 0;
}

static int
pfm_get_task(pfm_context_t *ctx, pid_t pid, struct task_struct **task)
{
	struct task_struct *p = current;
	int ret;

	
	if (pid < 2) return -EPERM;

	if (pid != task_pid_vnr(current)) {

		read_lock(&tasklist_lock);

		p = find_task_by_vpid(pid);

		
		if (p) get_task_struct(p);

		read_unlock(&tasklist_lock);

		if (p == NULL) return -ESRCH;
	}

	ret = pfm_task_incompatible(ctx, p);
	if (ret == 0) {
		*task = p;
	} else if (p != current) {
		pfm_put_task(p);
	}
	return ret;
}



static int
pfm_context_create(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	pfarg_context_t *req = (pfarg_context_t *)arg;
	struct file *filp;
	struct path path;
	int ctx_flags;
	int fd;
	int ret;

	
	ret = pfarg_is_sane(current, req);
	if (ret < 0)
		return ret;

	ctx_flags = req->ctx_flags;

	ret = -ENOMEM;

	fd = get_unused_fd();
	if (fd < 0)
		return fd;

	ctx = pfm_context_alloc(ctx_flags);
	if (!ctx)
		goto error;

	filp = pfm_alloc_file(ctx);
	if (IS_ERR(filp)) {
		ret = PTR_ERR(filp);
		goto error_file;
	}

	req->ctx_fd = ctx->ctx_fd = fd;

	if (pfm_uuid_cmp(req->ctx_smpl_buf_id, pfm_null_uuid)) {
		ret = pfm_setup_buffer_fmt(current, filp, ctx, ctx_flags, 0, req);
		if (ret)
			goto buffer_error;
	}

	DPRINT(("ctx=%p flags=0x%x system=%d notify_block=%d excl_idle=%d no_msg=%d ctx_fd=%d\n",
		ctx,
		ctx_flags,
		ctx->ctx_fl_system,
		ctx->ctx_fl_block,
		ctx->ctx_fl_excl_idle,
		ctx->ctx_fl_no_msg,
		ctx->ctx_fd));

	pfm_reset_pmu_state(ctx);

	fd_install(fd, filp);

	return 0;

buffer_error:
	path = filp->f_path;
	put_filp(filp);
	path_put(&path);

	if (ctx->ctx_buf_fmt) {
		pfm_buf_fmt_exit(ctx->ctx_buf_fmt, current, NULL, regs);
	}
error_file:
	pfm_context_free(ctx);

error:
	put_unused_fd(fd);
	return ret;
}

static inline unsigned long
pfm_new_counter_value (pfm_counter_t *reg, int is_long_reset)
{
	unsigned long val = is_long_reset ? reg->long_reset : reg->short_reset;
	unsigned long new_seed, old_seed = reg->seed, mask = reg->mask;
	extern unsigned long carta_random32 (unsigned long seed);

	if (reg->flags & PFM_REGFL_RANDOM) {
		new_seed = carta_random32(old_seed);
		val -= (old_seed & mask);	
		if ((mask >> 32) != 0)
			
			new_seed |= carta_random32(old_seed >> 32) << 32;
		reg->seed = new_seed;
	}
	reg->lval = val;
	return val;
}

static void
pfm_reset_regs_masked(pfm_context_t *ctx, unsigned long *ovfl_regs, int is_long_reset)
{
	unsigned long mask = ovfl_regs[0];
	unsigned long reset_others = 0UL;
	unsigned long val;
	int i;

	mask >>= PMU_FIRST_COUNTER;
	for(i = PMU_FIRST_COUNTER; mask; i++, mask >>= 1) {

		if ((mask & 0x1UL) == 0UL) continue;

		ctx->ctx_pmds[i].val = val = pfm_new_counter_value(ctx->ctx_pmds+ i, is_long_reset);
		reset_others        |= ctx->ctx_pmds[i].reset_pmds[0];

		DPRINT_ovfl((" %s reset ctx_pmds[%d]=%lx\n", is_long_reset ? "long" : "short", i, val));
	}

	for(i = 0; reset_others; i++, reset_others >>= 1) {

		if ((reset_others & 0x1) == 0) continue;

		ctx->ctx_pmds[i].val = val = pfm_new_counter_value(ctx->ctx_pmds + i, is_long_reset);

		DPRINT_ovfl(("%s reset_others pmd[%d]=%lx\n",
			  is_long_reset ? "long" : "short", i, val));
	}
}

static void
pfm_reset_regs(pfm_context_t *ctx, unsigned long *ovfl_regs, int is_long_reset)
{
	unsigned long mask = ovfl_regs[0];
	unsigned long reset_others = 0UL;
	unsigned long val;
	int i;

	DPRINT_ovfl(("ovfl_regs=0x%lx is_long_reset=%d\n", ovfl_regs[0], is_long_reset));

	if (ctx->ctx_state == PFM_CTX_MASKED) {
		pfm_reset_regs_masked(ctx, ovfl_regs, is_long_reset);
		return;
	}

	mask >>= PMU_FIRST_COUNTER;
	for(i = PMU_FIRST_COUNTER; mask; i++, mask >>= 1) {

		if ((mask & 0x1UL) == 0UL) continue;

		val           = pfm_new_counter_value(ctx->ctx_pmds+ i, is_long_reset);
		reset_others |= ctx->ctx_pmds[i].reset_pmds[0];

		DPRINT_ovfl((" %s reset ctx_pmds[%d]=%lx\n", is_long_reset ? "long" : "short", i, val));

		pfm_write_soft_counter(ctx, i, val);
	}

	for(i = 0; reset_others; i++, reset_others >>= 1) {

		if ((reset_others & 0x1) == 0) continue;

		val = pfm_new_counter_value(ctx->ctx_pmds + i, is_long_reset);

		if (PMD_IS_COUNTING(i)) {
			pfm_write_soft_counter(ctx, i, val);
		} else {
			ia64_set_pmd(i, val);
		}
		DPRINT_ovfl(("%s reset_others pmd[%d]=%lx\n",
			  is_long_reset ? "long" : "short", i, val));
	}
	ia64_srlz_d();
}

static int
pfm_write_pmcs(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	struct task_struct *task;
	pfarg_reg_t *req = (pfarg_reg_t *)arg;
	unsigned long value, pmc_pm;
	unsigned long smpl_pmds, reset_pmds, impl_pmds;
	unsigned int cnum, reg_flags, flags, pmc_type;
	int i, can_access_pmu = 0, is_loaded, is_system, expert_mode;
	int is_monitor, is_counting, state;
	int ret = -EINVAL;
	pfm_reg_check_t	wr_func;
#define PFM_CHECK_PMC_PM(x, y, z) ((x)->ctx_fl_system ^ PMC_PM(y, z))

	state     = ctx->ctx_state;
	is_loaded = state == PFM_CTX_LOADED ? 1 : 0;
	is_system = ctx->ctx_fl_system;
	task      = ctx->ctx_task;
	impl_pmds = pmu_conf->impl_pmds[0];

	if (state == PFM_CTX_ZOMBIE) return -EINVAL;

	if (is_loaded) {
		if (is_system && ctx->ctx_cpu != smp_processor_id()) {
			DPRINT(("should be running on CPU%d\n", ctx->ctx_cpu));
			return -EBUSY;
		}
		can_access_pmu = GET_PMU_OWNER() == task || is_system ? 1 : 0;
	}
	expert_mode = pfm_sysctl.expert_mode; 

	for (i = 0; i < count; i++, req++) {

		cnum       = req->reg_num;
		reg_flags  = req->reg_flags;
		value      = req->reg_value;
		smpl_pmds  = req->reg_smpl_pmds[0];
		reset_pmds = req->reg_reset_pmds[0];
		flags      = 0;


		if (cnum >= PMU_MAX_PMCS) {
			DPRINT(("pmc%u is invalid\n", cnum));
			goto error;
		}

		pmc_type   = pmu_conf->pmc_desc[cnum].type;
		pmc_pm     = (value >> pmu_conf->pmc_desc[cnum].pm_pos) & 0x1;
		is_counting = (pmc_type & PFM_REG_COUNTING) == PFM_REG_COUNTING ? 1 : 0;
		is_monitor  = (pmc_type & PFM_REG_MONITOR) == PFM_REG_MONITOR ? 1 : 0;

		if ((pmc_type & PFM_REG_IMPL) == 0 || (pmc_type & PFM_REG_CONTROL) == PFM_REG_CONTROL) {
			DPRINT(("pmc%u is unimplemented or no-access pmc_type=%x\n", cnum, pmc_type));
			goto error;
		}
		wr_func = pmu_conf->pmc_desc[cnum].write_check;
		if (is_monitor && value != PMC_DFL_VAL(cnum) && is_system ^ pmc_pm) {
			DPRINT(("pmc%u pmc_pm=%lu is_system=%d\n",
				cnum,
				pmc_pm,
				is_system));
			goto error;
		}

		if (is_counting) {
			value |= 1 << PMU_PMC_OI;

			if (reg_flags & PFM_REGFL_OVFL_NOTIFY) {
				flags |= PFM_REGFL_OVFL_NOTIFY;
			}

			if (reg_flags & PFM_REGFL_RANDOM) flags |= PFM_REGFL_RANDOM;

			
			if ((smpl_pmds & impl_pmds) != smpl_pmds) {
				DPRINT(("invalid smpl_pmds 0x%lx for pmc%u\n", smpl_pmds, cnum));
				goto error;
			}

			
			if ((reset_pmds & impl_pmds) != reset_pmds) {
				DPRINT(("invalid reset_pmds 0x%lx for pmc%u\n", reset_pmds, cnum));
				goto error;
			}
		} else {
			if (reg_flags & (PFM_REGFL_OVFL_NOTIFY|PFM_REGFL_RANDOM)) {
				DPRINT(("cannot set ovfl_notify or random on pmc%u\n", cnum));
				goto error;
			}
			
		}

		if (likely(expert_mode == 0 && wr_func)) {
			ret = (*wr_func)(task, ctx, cnum, &value, regs);
			if (ret) goto error;
			ret = -EINVAL;
		}

		PFM_REG_RETFLAG_SET(req->reg_flags, 0);


		if (is_counting) {
			ctx->ctx_pmds[cnum].flags = flags;

			ctx->ctx_pmds[cnum].reset_pmds[0] = reset_pmds;
			ctx->ctx_pmds[cnum].smpl_pmds[0]  = smpl_pmds;
			ctx->ctx_pmds[cnum].eventid       = req->reg_smpl_eventid;

			CTX_USED_PMD(ctx, reset_pmds);
			CTX_USED_PMD(ctx, smpl_pmds);
			if (state == PFM_CTX_MASKED) ctx->ctx_ovfl_regs[0] &= ~1UL << cnum;
		}
		CTX_USED_PMD(ctx, pmu_conf->pmc_desc[cnum].dep_pmd[0]);

		if (is_monitor) CTX_USED_MONITOR(ctx, 1UL << cnum);

		ctx->ctx_pmcs[cnum] = value;

		if (is_loaded) {
			if (is_system == 0) ctx->th_pmcs[cnum] = value;

			if (can_access_pmu) {
				ia64_set_pmc(cnum, value);
			}
#ifdef CONFIG_SMP
			else {
				ctx->ctx_reload_pmcs[0] |= 1UL << cnum;
			}
#endif
		}

		DPRINT(("pmc[%u]=0x%lx ld=%d apmu=%d flags=0x%x all_pmcs=0x%lx used_pmds=0x%lx eventid=%ld smpl_pmds=0x%lx reset_pmds=0x%lx reloads_pmcs=0x%lx used_monitors=0x%lx ovfl_regs=0x%lx\n",
			  cnum,
			  value,
			  is_loaded,
			  can_access_pmu,
			  flags,
			  ctx->ctx_all_pmcs[0],
			  ctx->ctx_used_pmds[0],
			  ctx->ctx_pmds[cnum].eventid,
			  smpl_pmds,
			  reset_pmds,
			  ctx->ctx_reload_pmcs[0],
			  ctx->ctx_used_monitors[0],
			  ctx->ctx_ovfl_regs[0]));
	}

	if (can_access_pmu) ia64_srlz_d();

	return 0;
error:
	PFM_REG_RETFLAG_SET(req->reg_flags, PFM_REG_RETFL_EINVAL);
	return ret;
}

static int
pfm_write_pmds(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	struct task_struct *task;
	pfarg_reg_t *req = (pfarg_reg_t *)arg;
	unsigned long value, hw_value, ovfl_mask;
	unsigned int cnum;
	int i, can_access_pmu = 0, state;
	int is_counting, is_loaded, is_system, expert_mode;
	int ret = -EINVAL;
	pfm_reg_check_t wr_func;


	state     = ctx->ctx_state;
	is_loaded = state == PFM_CTX_LOADED ? 1 : 0;
	is_system = ctx->ctx_fl_system;
	ovfl_mask = pmu_conf->ovfl_val;
	task      = ctx->ctx_task;

	if (unlikely(state == PFM_CTX_ZOMBIE)) return -EINVAL;

	if (likely(is_loaded)) {
		if (unlikely(is_system && ctx->ctx_cpu != smp_processor_id())) {
			DPRINT(("should be running on CPU%d\n", ctx->ctx_cpu));
			return -EBUSY;
		}
		can_access_pmu = GET_PMU_OWNER() == task || is_system ? 1 : 0;
	}
	expert_mode = pfm_sysctl.expert_mode; 

	for (i = 0; i < count; i++, req++) {

		cnum  = req->reg_num;
		value = req->reg_value;

		if (!PMD_IS_IMPL(cnum)) {
			DPRINT(("pmd[%u] is unimplemented or invalid\n", cnum));
			goto abort_mission;
		}
		is_counting = PMD_IS_COUNTING(cnum);
		wr_func     = pmu_conf->pmd_desc[cnum].write_check;

		if (unlikely(expert_mode == 0 && wr_func)) {
			unsigned long v = value;

			ret = (*wr_func)(task, ctx, cnum, &v, regs);
			if (ret) goto abort_mission;

			value = v;
			ret   = -EINVAL;
		}

		PFM_REG_RETFLAG_SET(req->reg_flags, 0);

		hw_value = value;

		if (is_counting) {
			ctx->ctx_pmds[cnum].lval = value;

			if (is_loaded) {
				hw_value = value &  ovfl_mask;
				value    = value & ~ovfl_mask;
			}
		}
		ctx->ctx_pmds[cnum].long_reset  = req->reg_long_reset;
		ctx->ctx_pmds[cnum].short_reset = req->reg_short_reset;

		ctx->ctx_pmds[cnum].seed = req->reg_random_seed;
		ctx->ctx_pmds[cnum].mask = req->reg_random_mask;

		ctx->ctx_pmds[cnum].val  = value;

		CTX_USED_PMD(ctx, PMD_PMD_DEP(cnum));

		CTX_USED_PMD(ctx, RDEP(cnum));

		if (is_counting && state == PFM_CTX_MASKED) {
			ctx->ctx_ovfl_regs[0] &= ~1UL << cnum;
		}

		if (is_loaded) {
			if (is_system == 0) ctx->th_pmds[cnum] = hw_value;

			if (can_access_pmu) {
				ia64_set_pmd(cnum, hw_value);
			} else {
#ifdef CONFIG_SMP
				ctx->ctx_reload_pmds[0] |= 1UL << cnum;
#endif
			}
		}

		DPRINT(("pmd[%u]=0x%lx ld=%d apmu=%d, hw_value=0x%lx ctx_pmd=0x%lx  short_reset=0x%lx "
			  "long_reset=0x%lx notify=%c seed=0x%lx mask=0x%lx used_pmds=0x%lx reset_pmds=0x%lx reload_pmds=0x%lx all_pmds=0x%lx ovfl_regs=0x%lx\n",
			cnum,
			value,
			is_loaded,
			can_access_pmu,
			hw_value,
			ctx->ctx_pmds[cnum].val,
			ctx->ctx_pmds[cnum].short_reset,
			ctx->ctx_pmds[cnum].long_reset,
			PMC_OVFL_NOTIFY(ctx, cnum) ? 'Y':'N',
			ctx->ctx_pmds[cnum].seed,
			ctx->ctx_pmds[cnum].mask,
			ctx->ctx_used_pmds[0],
			ctx->ctx_pmds[cnum].reset_pmds[0],
			ctx->ctx_reload_pmds[0],
			ctx->ctx_all_pmds[0],
			ctx->ctx_ovfl_regs[0]));
	}

	if (can_access_pmu) ia64_srlz_d();

	return 0;

abort_mission:
	PFM_REG_RETFLAG_SET(req->reg_flags, PFM_REG_RETFL_EINVAL);
	return ret;
}

static int
pfm_read_pmds(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	struct task_struct *task;
	unsigned long val = 0UL, lval, ovfl_mask, sval;
	pfarg_reg_t *req = (pfarg_reg_t *)arg;
	unsigned int cnum, reg_flags = 0;
	int i, can_access_pmu = 0, state;
	int is_loaded, is_system, is_counting, expert_mode;
	int ret = -EINVAL;
	pfm_reg_check_t rd_func;


	state     = ctx->ctx_state;
	is_loaded = state == PFM_CTX_LOADED ? 1 : 0;
	is_system = ctx->ctx_fl_system;
	ovfl_mask = pmu_conf->ovfl_val;
	task      = ctx->ctx_task;

	if (state == PFM_CTX_ZOMBIE) return -EINVAL;

	if (likely(is_loaded)) {
		if (unlikely(is_system && ctx->ctx_cpu != smp_processor_id())) {
			DPRINT(("should be running on CPU%d\n", ctx->ctx_cpu));
			return -EBUSY;
		}
		can_access_pmu = GET_PMU_OWNER() == task || is_system ? 1 : 0;

		if (can_access_pmu) ia64_srlz_d();
	}
	expert_mode = pfm_sysctl.expert_mode; 

	DPRINT(("ld=%d apmu=%d ctx_state=%d\n",
		is_loaded,
		can_access_pmu,
		state));


	for (i = 0; i < count; i++, req++) {

		cnum        = req->reg_num;
		reg_flags   = req->reg_flags;

		if (unlikely(!PMD_IS_IMPL(cnum))) goto error;
		if (unlikely(!CTX_IS_USED_PMD(ctx, cnum))) goto error;

		sval        = ctx->ctx_pmds[cnum].val;
		lval        = ctx->ctx_pmds[cnum].lval;
		is_counting = PMD_IS_COUNTING(cnum);

		if (can_access_pmu){
			val = ia64_get_pmd(cnum);
		} else {
			val = is_loaded ? ctx->th_pmds[cnum] : 0UL;
		}
		rd_func = pmu_conf->pmd_desc[cnum].read_check;

		if (is_counting) {
			val &= ovfl_mask;
			val += sval;
		}

		if (unlikely(expert_mode == 0 && rd_func)) {
			unsigned long v = val;
			ret = (*rd_func)(ctx->ctx_task, ctx, cnum, &v, regs);
			if (ret) goto error;
			val = v;
			ret = -EINVAL;
		}

		PFM_REG_RETFLAG_SET(reg_flags, 0);

		DPRINT(("pmd[%u]=0x%lx\n", cnum, val));

		req->reg_value            = val;
		req->reg_flags            = reg_flags;
		req->reg_last_reset_val   = lval;
	}

	return 0;

error:
	PFM_REG_RETFLAG_SET(req->reg_flags, PFM_REG_RETFL_EINVAL);
	return ret;
}

int
pfm_mod_write_pmcs(struct task_struct *task, void *req, unsigned int nreq, struct pt_regs *regs)
{
	pfm_context_t *ctx;

	if (req == NULL) return -EINVAL;

 	ctx = GET_PMU_CTX();

	if (ctx == NULL) return -EINVAL;

	if (task != current && ctx->ctx_fl_system == 0) return -EBUSY;

	return pfm_write_pmcs(ctx, req, nreq, regs);
}
EXPORT_SYMBOL(pfm_mod_write_pmcs);

int
pfm_mod_read_pmds(struct task_struct *task, void *req, unsigned int nreq, struct pt_regs *regs)
{
	pfm_context_t *ctx;

	if (req == NULL) return -EINVAL;

 	ctx = GET_PMU_CTX();

	if (ctx == NULL) return -EINVAL;

	if (task != current && ctx->ctx_fl_system == 0) return -EBUSY;

	return pfm_read_pmds(ctx, req, nreq, regs);
}
EXPORT_SYMBOL(pfm_mod_read_pmds);

int
pfm_use_debug_registers(struct task_struct *task)
{
	pfm_context_t *ctx = task->thread.pfm_context;
	unsigned long flags;
	int ret = 0;

	if (pmu_conf->use_rr_dbregs == 0) return 0;

	DPRINT(("called for [%d]\n", task_pid_nr(task)));

	if (task->thread.flags & IA64_THREAD_DBG_VALID) return 0;

	if (ctx && ctx->ctx_fl_using_dbreg == 1) return -1;

	LOCK_PFS(flags);

	if (pfm_sessions.pfs_sys_use_dbregs> 0)
		ret = -1;
	else
		pfm_sessions.pfs_ptrace_use_dbregs++;

	DPRINT(("ptrace_use_dbregs=%u  sys_use_dbregs=%u by [%d] ret = %d\n",
		  pfm_sessions.pfs_ptrace_use_dbregs,
		  pfm_sessions.pfs_sys_use_dbregs,
		  task_pid_nr(task), ret));

	UNLOCK_PFS(flags);

	return ret;
}

int
pfm_release_debug_registers(struct task_struct *task)
{
	unsigned long flags;
	int ret;

	if (pmu_conf->use_rr_dbregs == 0) return 0;

	LOCK_PFS(flags);
	if (pfm_sessions.pfs_ptrace_use_dbregs == 0) {
		printk(KERN_ERR "perfmon: invalid release for [%d] ptrace_use_dbregs=0\n", task_pid_nr(task));
		ret = -1;
	}  else {
		pfm_sessions.pfs_ptrace_use_dbregs--;
		ret = 0;
	}
	UNLOCK_PFS(flags);

	return ret;
}

static int
pfm_restart(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	struct task_struct *task;
	pfm_buffer_fmt_t *fmt;
	pfm_ovfl_ctrl_t rst_ctrl;
	int state, is_system;
	int ret = 0;

	state     = ctx->ctx_state;
	fmt       = ctx->ctx_buf_fmt;
	is_system = ctx->ctx_fl_system;
	task      = PFM_CTX_TASK(ctx);

	switch(state) {
		case PFM_CTX_MASKED:
			break;
		case PFM_CTX_LOADED: 
			if (CTX_HAS_SMPL(ctx) && fmt->fmt_restart_active) break;
			
		case PFM_CTX_UNLOADED:
		case PFM_CTX_ZOMBIE:
			DPRINT(("invalid state=%d\n", state));
			return -EBUSY;
		default:
			DPRINT(("state=%d, cannot operate (no active_restart handler)\n", state));
			return -EINVAL;
	}

	if (is_system && ctx->ctx_cpu != smp_processor_id()) {
		DPRINT(("should be running on CPU%d\n", ctx->ctx_cpu));
		return -EBUSY;
	}

	
	if (unlikely(task == NULL)) {
		printk(KERN_ERR "perfmon: [%d] pfm_restart no task\n", task_pid_nr(current));
		return -EINVAL;
	}

	if (task == current || is_system) {

		fmt = ctx->ctx_buf_fmt;

		DPRINT(("restarting self %d ovfl=0x%lx\n",
			task_pid_nr(task),
			ctx->ctx_ovfl_regs[0]));

		if (CTX_HAS_SMPL(ctx)) {

			prefetch(ctx->ctx_smpl_hdr);

			rst_ctrl.bits.mask_monitoring = 0;
			rst_ctrl.bits.reset_ovfl_pmds = 0;

			if (state == PFM_CTX_LOADED)
				ret = pfm_buf_fmt_restart_active(fmt, task, &rst_ctrl, ctx->ctx_smpl_hdr, regs);
			else
				ret = pfm_buf_fmt_restart(fmt, task, &rst_ctrl, ctx->ctx_smpl_hdr, regs);
		} else {
			rst_ctrl.bits.mask_monitoring = 0;
			rst_ctrl.bits.reset_ovfl_pmds = 1;
		}

		if (ret == 0) {
			if (rst_ctrl.bits.reset_ovfl_pmds)
				pfm_reset_regs(ctx, ctx->ctx_ovfl_regs, PFM_PMD_LONG_RESET);

			if (rst_ctrl.bits.mask_monitoring == 0) {
				DPRINT(("resuming monitoring for [%d]\n", task_pid_nr(task)));

				if (state == PFM_CTX_MASKED) pfm_restore_monitoring(task);
			} else {
				DPRINT(("keeping monitoring stopped for [%d]\n", task_pid_nr(task)));

				
			}
		}
		ctx->ctx_ovfl_regs[0] = 0UL;

		ctx->ctx_state = PFM_CTX_LOADED;

		ctx->ctx_fl_can_restart = 0;

		return 0;
	}


	if (state == PFM_CTX_MASKED) {
		if (ctx->ctx_fl_can_restart == 0) return -EINVAL;
		ctx->ctx_fl_can_restart = 0;
	}

	if (CTX_OVFL_NOBLOCK(ctx) == 0 && state == PFM_CTX_MASKED) {
		DPRINT(("unblocking [%d]\n", task_pid_nr(task)));
		complete(&ctx->ctx_restart_done);
	} else {
		DPRINT(("[%d] armed exit trap\n", task_pid_nr(task)));

		ctx->ctx_fl_trap_reason = PFM_TRAP_REASON_RESET;

		PFM_SET_WORK_PENDING(task, 1);

		set_notify_resume(task);

	}
	return 0;
}

static int
pfm_debug(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	unsigned int m = *(unsigned int *)arg;

	pfm_sysctl.debug = m == 0 ? 0 : 1;

	printk(KERN_INFO "perfmon debugging %s (timing reset)\n", pfm_sysctl.debug ? "on" : "off");

	if (m == 0) {
		memset(pfm_stats, 0, sizeof(pfm_stats));
		for(m=0; m < NR_CPUS; m++) pfm_stats[m].pfm_ovfl_intr_cycles_min = ~0UL;
	}
	return 0;
}

static int
pfm_write_ibr_dbr(int mode, pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	struct thread_struct *thread = NULL;
	struct task_struct *task;
	pfarg_dbreg_t *req = (pfarg_dbreg_t *)arg;
	unsigned long flags;
	dbreg_t dbreg;
	unsigned int rnum;
	int first_time;
	int ret = 0, state;
	int i, can_access_pmu = 0;
	int is_system, is_loaded;

	if (pmu_conf->use_rr_dbregs == 0) return -EINVAL;

	state     = ctx->ctx_state;
	is_loaded = state == PFM_CTX_LOADED ? 1 : 0;
	is_system = ctx->ctx_fl_system;
	task      = ctx->ctx_task;

	if (state == PFM_CTX_ZOMBIE) return -EINVAL;

	if (is_loaded) {
		thread = &task->thread;
		if (unlikely(is_system && ctx->ctx_cpu != smp_processor_id())) {
			DPRINT(("should be running on CPU%d\n", ctx->ctx_cpu));
			return -EBUSY;
		}
		can_access_pmu = GET_PMU_OWNER() == task || is_system ? 1 : 0;
	}


	first_time = ctx->ctx_fl_using_dbreg == 0;

	if (is_loaded && (thread->flags & IA64_THREAD_DBG_VALID) != 0) {
		DPRINT(("debug registers already in use for [%d]\n", task_pid_nr(task)));
		return -EBUSY;
	}

	/*
	 * check for debug registers in system wide mode
	 *
	 * If though a check is done in pfm_context_load(),
	 * we must repeat it here, in case the registers are
	 * written after the context is loaded
	 */
	if (is_loaded) {
		LOCK_PFS(flags);

		if (first_time && is_system) {
			if (pfm_sessions.pfs_ptrace_use_dbregs)
				ret = -EBUSY;
			else
				pfm_sessions.pfs_sys_use_dbregs++;
		}
		UNLOCK_PFS(flags);
	}

	if (ret != 0) return ret;

	ctx->ctx_fl_using_dbreg = 1;

	if (first_time && can_access_pmu) {
		DPRINT(("[%d] clearing ibrs, dbrs\n", task_pid_nr(task)));
		for (i=0; i < pmu_conf->num_ibrs; i++) {
			ia64_set_ibr(i, 0UL);
			ia64_dv_serialize_instruction();
		}
		ia64_srlz_i();
		for (i=0; i < pmu_conf->num_dbrs; i++) {
			ia64_set_dbr(i, 0UL);
			ia64_dv_serialize_data();
		}
		ia64_srlz_d();
	}

	for (i = 0; i < count; i++, req++) {

		rnum      = req->dbreg_num;
		dbreg.val = req->dbreg_value;

		ret = -EINVAL;

		if ((mode == PFM_CODE_RR && rnum >= PFM_NUM_IBRS) || ((mode == PFM_DATA_RR) && rnum >= PFM_NUM_DBRS)) {
			DPRINT(("invalid register %u val=0x%lx mode=%d i=%d count=%d\n",
				  rnum, dbreg.val, mode, i, count));

			goto abort_mission;
		}

		if (rnum & 0x1) {
			if (mode == PFM_CODE_RR)
				dbreg.ibr.ibr_x = 0;
			else
				dbreg.dbr.dbr_r = dbreg.dbr.dbr_w = 0;
		}

		PFM_REG_RETFLAG_SET(req->dbreg_flags, 0);

		/*
		 * Debug registers, just like PMC, can only be modified
		 * by a kernel call. Moreover, perfmon() access to those
		 * registers are centralized in this routine. The hardware
		 * does not modify the value of these registers, therefore,
		 * if we save them as they are written, we can avoid having
		 * to save them on context switch out. This is made possible
		 * by the fact that when perfmon uses debug registers, ptrace()
		 * won't be able to modify them concurrently.
		 */
		if (mode == PFM_CODE_RR) {
			CTX_USED_IBR(ctx, rnum);

			if (can_access_pmu) {
				ia64_set_ibr(rnum, dbreg.val);
				ia64_dv_serialize_instruction();
			}

			ctx->ctx_ibrs[rnum] = dbreg.val;

			DPRINT(("write ibr%u=0x%lx used_ibrs=0x%x ld=%d apmu=%d\n",
				rnum, dbreg.val, ctx->ctx_used_ibrs[0], is_loaded, can_access_pmu));
		} else {
			CTX_USED_DBR(ctx, rnum);

			if (can_access_pmu) {
				ia64_set_dbr(rnum, dbreg.val);
				ia64_dv_serialize_data();
			}
			ctx->ctx_dbrs[rnum] = dbreg.val;

			DPRINT(("write dbr%u=0x%lx used_dbrs=0x%x ld=%d apmu=%d\n",
				rnum, dbreg.val, ctx->ctx_used_dbrs[0], is_loaded, can_access_pmu));
		}
	}

	return 0;

abort_mission:
	if (first_time) {
		LOCK_PFS(flags);
		if (ctx->ctx_fl_system) {
			pfm_sessions.pfs_sys_use_dbregs--;
		}
		UNLOCK_PFS(flags);
		ctx->ctx_fl_using_dbreg = 0;
	}
	PFM_REG_RETFLAG_SET(req->dbreg_flags, PFM_REG_RETFL_EINVAL);

	return ret;
}

static int
pfm_write_ibrs(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	return pfm_write_ibr_dbr(PFM_CODE_RR, ctx, arg, count, regs);
}

static int
pfm_write_dbrs(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	return pfm_write_ibr_dbr(PFM_DATA_RR, ctx, arg, count, regs);
}

int
pfm_mod_write_ibrs(struct task_struct *task, void *req, unsigned int nreq, struct pt_regs *regs)
{
	pfm_context_t *ctx;

	if (req == NULL) return -EINVAL;

 	ctx = GET_PMU_CTX();

	if (ctx == NULL) return -EINVAL;

	if (task != current && ctx->ctx_fl_system == 0) return -EBUSY;

	return pfm_write_ibrs(ctx, req, nreq, regs);
}
EXPORT_SYMBOL(pfm_mod_write_ibrs);

int
pfm_mod_write_dbrs(struct task_struct *task, void *req, unsigned int nreq, struct pt_regs *regs)
{
	pfm_context_t *ctx;

	if (req == NULL) return -EINVAL;

 	ctx = GET_PMU_CTX();

	if (ctx == NULL) return -EINVAL;

	if (task != current && ctx->ctx_fl_system == 0) return -EBUSY;

	return pfm_write_dbrs(ctx, req, nreq, regs);
}
EXPORT_SYMBOL(pfm_mod_write_dbrs);


static int
pfm_get_features(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	pfarg_features_t *req = (pfarg_features_t *)arg;

	req->ft_version = PFM_VERSION;
	return 0;
}

static int
pfm_stop(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	struct pt_regs *tregs;
	struct task_struct *task = PFM_CTX_TASK(ctx);
	int state, is_system;

	state     = ctx->ctx_state;
	is_system = ctx->ctx_fl_system;

	if (state == PFM_CTX_UNLOADED) return -EINVAL;

	if (is_system && ctx->ctx_cpu != smp_processor_id()) {
		DPRINT(("should be running on CPU%d\n", ctx->ctx_cpu));
		return -EBUSY;
	}
	DPRINT(("task [%d] ctx_state=%d is_system=%d\n",
		task_pid_nr(PFM_CTX_TASK(ctx)),
		state,
		is_system));
	if (is_system) {
		ia64_setreg(_IA64_REG_CR_DCR, ia64_getreg(_IA64_REG_CR_DCR) & ~IA64_DCR_PP);
		ia64_srlz_i();

		PFM_CPUINFO_CLEAR(PFM_CPUINFO_DCR_PP);

		pfm_clear_psr_pp();

		ia64_psr(regs)->pp = 0;

		return 0;
	}

	if (task == current) {
		
		pfm_clear_psr_up();

		ia64_psr(regs)->up = 0;
	} else {
		tregs = task_pt_regs(task);

		ia64_psr(tregs)->up = 0;

		ctx->ctx_saved_psr_up = 0;
		DPRINT(("task=[%d]\n", task_pid_nr(task)));
	}
	return 0;
}


static int
pfm_start(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	struct pt_regs *tregs;
	int state, is_system;

	state     = ctx->ctx_state;
	is_system = ctx->ctx_fl_system;

	if (state != PFM_CTX_LOADED) return -EINVAL;

	if (is_system && ctx->ctx_cpu != smp_processor_id()) {
		DPRINT(("should be running on CPU%d\n", ctx->ctx_cpu));
		return -EBUSY;
	}

	if (is_system) {

		ia64_psr(regs)->pp = 1;

		PFM_CPUINFO_SET(PFM_CPUINFO_DCR_PP);

		pfm_set_psr_pp();

		
		ia64_setreg(_IA64_REG_CR_DCR, ia64_getreg(_IA64_REG_CR_DCR) | IA64_DCR_PP);
		ia64_srlz_i();

		return 0;
	}


	if (ctx->ctx_task == current) {

		
		pfm_set_psr_up();

		ia64_psr(regs)->up = 1;

	} else {
		tregs = task_pt_regs(ctx->ctx_task);

		ctx->ctx_saved_psr_up = IA64_PSR_UP;

		ia64_psr(tregs)->up = 1;
	}
	return 0;
}

static int
pfm_get_pmc_reset(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	pfarg_reg_t *req = (pfarg_reg_t *)arg;
	unsigned int cnum;
	int i;
	int ret = -EINVAL;

	for (i = 0; i < count; i++, req++) {

		cnum = req->reg_num;

		if (!PMC_IS_IMPL(cnum)) goto abort_mission;

		req->reg_value = PMC_DFL_VAL(cnum);

		PFM_REG_RETFLAG_SET(req->reg_flags, 0);

		DPRINT(("pmc_reset_val pmc[%u]=0x%lx\n", cnum, req->reg_value));
	}
	return 0;

abort_mission:
	PFM_REG_RETFLAG_SET(req->reg_flags, PFM_REG_RETFL_EINVAL);
	return ret;
}

static int
pfm_check_task_exist(pfm_context_t *ctx)
{
	struct task_struct *g, *t;
	int ret = -ESRCH;

	read_lock(&tasklist_lock);

	do_each_thread (g, t) {
		if (t->thread.pfm_context == ctx) {
			ret = 0;
			goto out;
		}
	} while_each_thread (g, t);
out:
	read_unlock(&tasklist_lock);

	DPRINT(("pfm_check_task_exist: ret=%d ctx=%p\n", ret, ctx));

	return ret;
}

static int
pfm_context_load(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	struct task_struct *task;
	struct thread_struct *thread;
	struct pfm_context_t *old;
	unsigned long flags;
#ifndef CONFIG_SMP
	struct task_struct *owner_task = NULL;
#endif
	pfarg_load_t *req = (pfarg_load_t *)arg;
	unsigned long *pmcs_source, *pmds_source;
	int the_cpu;
	int ret = 0;
	int state, is_system, set_dbregs = 0;

	state     = ctx->ctx_state;
	is_system = ctx->ctx_fl_system;
	if (state != PFM_CTX_UNLOADED) {
		DPRINT(("cannot load to [%d], invalid ctx_state=%d\n",
			req->load_pid,
			ctx->ctx_state));
		return -EBUSY;
	}

	DPRINT(("load_pid [%d] using_dbreg=%d\n", req->load_pid, ctx->ctx_fl_using_dbreg));

	if (CTX_OVFL_NOBLOCK(ctx) == 0 && req->load_pid == current->pid) {
		DPRINT(("cannot use blocking mode on self\n"));
		return -EINVAL;
	}

	ret = pfm_get_task(ctx, req->load_pid, &task);
	if (ret) {
		DPRINT(("load_pid [%d] get_task=%d\n", req->load_pid, ret));
		return ret;
	}

	ret = -EINVAL;

	if (is_system && task != current) {
		DPRINT(("system wide is self monitoring only load_pid=%d\n",
			req->load_pid));
		goto error;
	}

	thread = &task->thread;

	ret = 0;
	if (ctx->ctx_fl_using_dbreg) {
		if (thread->flags & IA64_THREAD_DBG_VALID) {
			ret = -EBUSY;
			DPRINT(("load_pid [%d] task is debugged, cannot load range restrictions\n", req->load_pid));
			goto error;
		}
		LOCK_PFS(flags);

		if (is_system) {
			if (pfm_sessions.pfs_ptrace_use_dbregs) {
				DPRINT(("cannot load [%d] dbregs in use\n",
							task_pid_nr(task)));
				ret = -EBUSY;
			} else {
				pfm_sessions.pfs_sys_use_dbregs++;
				DPRINT(("load [%d] increased sys_use_dbreg=%u\n", task_pid_nr(task), pfm_sessions.pfs_sys_use_dbregs));
				set_dbregs = 1;
			}
		}

		UNLOCK_PFS(flags);

		if (ret) goto error;
	}

	the_cpu = ctx->ctx_cpu = smp_processor_id();

	ret = -EBUSY;
	ret = pfm_reserve_session(current, is_system, the_cpu);
	if (ret) goto error;

	DPRINT(("before cmpxchg() old_ctx=%p new_ctx=%p\n",
		thread->pfm_context, ctx));

	ret = -EBUSY;
	old = ia64_cmpxchg(acq, &thread->pfm_context, NULL, ctx, sizeof(pfm_context_t *));
	if (old != NULL) {
		DPRINT(("load_pid [%d] already has a context\n", req->load_pid));
		goto error_unres;
	}

	pfm_reset_msgq(ctx);

	ctx->ctx_state = PFM_CTX_LOADED;

	ctx->ctx_task = task;

	if (is_system) {
		PFM_CPUINFO_SET(PFM_CPUINFO_SYST_WIDE);
		PFM_CPUINFO_CLEAR(PFM_CPUINFO_DCR_PP);

		if (ctx->ctx_fl_excl_idle) PFM_CPUINFO_SET(PFM_CPUINFO_EXCL_IDLE);
	} else {
		thread->flags |= IA64_THREAD_PM_VALID;
	}

	pfm_copy_pmds(task, ctx);
	pfm_copy_pmcs(task, ctx);

	pmcs_source = ctx->th_pmcs;
	pmds_source = ctx->th_pmds;

	if (task == current) {

		if (is_system == 0) {

			
			ia64_psr(regs)->sp = 0;
			DPRINT(("clearing psr.sp for [%d]\n", task_pid_nr(task)));

			SET_LAST_CPU(ctx, smp_processor_id());
			INC_ACTIVATION();
			SET_ACTIVATION(ctx);
#ifndef CONFIG_SMP
			owner_task = GET_PMU_OWNER();
			if (owner_task) pfm_lazy_save_regs(owner_task);
#endif
		}
		pfm_restore_pmds(pmds_source, ctx->ctx_all_pmds[0]);
		pfm_restore_pmcs(pmcs_source, ctx->ctx_all_pmcs[0]);

		ctx->ctx_reload_pmcs[0] = 0UL;
		ctx->ctx_reload_pmds[0] = 0UL;

		if (ctx->ctx_fl_using_dbreg) {
			pfm_restore_ibrs(ctx->ctx_ibrs, pmu_conf->num_ibrs);
			pfm_restore_dbrs(ctx->ctx_dbrs, pmu_conf->num_dbrs);
		}
		SET_PMU_OWNER(task, ctx);

		DPRINT(("context loaded on PMU for [%d]\n", task_pid_nr(task)));
	} else {
		regs = task_pt_regs(task);

		
		ctx->ctx_last_activation = PFM_INVALID_ACTIVATION;
		SET_LAST_CPU(ctx, -1);

		
		ctx->ctx_saved_psr_up = 0UL;
		ia64_psr(regs)->up = ia64_psr(regs)->pp = 0;
	}

	ret = 0;

error_unres:
	if (ret) pfm_unreserve_session(ctx, ctx->ctx_fl_system, the_cpu);
error:
	if (ret && set_dbregs) {
		LOCK_PFS(flags);
		pfm_sessions.pfs_sys_use_dbregs--;
		UNLOCK_PFS(flags);
	}
	if (is_system == 0 && task != current) {
		pfm_put_task(task);

		if (ret == 0) {
			ret = pfm_check_task_exist(ctx);
			if (ret) {
				ctx->ctx_state = PFM_CTX_UNLOADED;
				ctx->ctx_task  = NULL;
			}
		}
	}
	return ret;
}

static void pfm_flush_pmds(struct task_struct *, pfm_context_t *ctx);

static int
pfm_context_unload(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs)
{
	struct task_struct *task = PFM_CTX_TASK(ctx);
	struct pt_regs *tregs;
	int prev_state, is_system;
	int ret;

	DPRINT(("ctx_state=%d task [%d]\n", ctx->ctx_state, task ? task_pid_nr(task) : -1));

	prev_state = ctx->ctx_state;
	is_system  = ctx->ctx_fl_system;

	if (prev_state == PFM_CTX_UNLOADED) {
		DPRINT(("ctx_state=%d, nothing to do\n", prev_state));
		return 0;
	}

	ret = pfm_stop(ctx, NULL, 0, regs);
	if (ret) return ret;

	ctx->ctx_state = PFM_CTX_UNLOADED;

	if (is_system) {

		PFM_CPUINFO_CLEAR(PFM_CPUINFO_SYST_WIDE);
		PFM_CPUINFO_CLEAR(PFM_CPUINFO_EXCL_IDLE);

		pfm_flush_pmds(current, ctx);

		if (prev_state != PFM_CTX_ZOMBIE) 
			pfm_unreserve_session(ctx, 1 , ctx->ctx_cpu);

		task->thread.pfm_context = NULL;
		ctx->ctx_task = NULL;

		return 0;
	}

	tregs = task == current ? regs : task_pt_regs(task);

	if (task == current) {
		ia64_psr(regs)->sp = 1;

		DPRINT(("setting psr.sp for [%d]\n", task_pid_nr(task)));
	}
	pfm_flush_pmds(task, ctx);

	if (prev_state != PFM_CTX_ZOMBIE) 
		pfm_unreserve_session(ctx, 0 , ctx->ctx_cpu);

	ctx->ctx_last_activation = PFM_INVALID_ACTIVATION;
	SET_LAST_CPU(ctx, -1);

	task->thread.flags &= ~IA64_THREAD_PM_VALID;

	task->thread.pfm_context  = NULL;
	ctx->ctx_task             = NULL;

	PFM_SET_WORK_PENDING(task, 0);

	ctx->ctx_fl_trap_reason  = PFM_TRAP_REASON_NONE;
	ctx->ctx_fl_can_restart  = 0;
	ctx->ctx_fl_going_zombie = 0;

	DPRINT(("disconnected [%d] from context\n", task_pid_nr(task)));

	return 0;
}


void
pfm_exit_thread(struct task_struct *task)
{
	pfm_context_t *ctx;
	unsigned long flags;
	struct pt_regs *regs = task_pt_regs(task);
	int ret, state;
	int free_ok = 0;

	ctx = PFM_GET_CTX(task);

	PROTECT_CTX(ctx, flags);

	DPRINT(("state=%d task [%d]\n", ctx->ctx_state, task_pid_nr(task)));

	state = ctx->ctx_state;
	switch(state) {
		case PFM_CTX_UNLOADED:
			printk(KERN_ERR "perfmon: pfm_exit_thread [%d] ctx unloaded\n", task_pid_nr(task));
			break;
		case PFM_CTX_LOADED:
		case PFM_CTX_MASKED:
			ret = pfm_context_unload(ctx, NULL, 0, regs);
			if (ret) {
				printk(KERN_ERR "perfmon: pfm_exit_thread [%d] state=%d unload failed %d\n", task_pid_nr(task), state, ret);
			}
			DPRINT(("ctx unloaded for current state was %d\n", state));

			pfm_end_notify_user(ctx);
			break;
		case PFM_CTX_ZOMBIE:
			ret = pfm_context_unload(ctx, NULL, 0, regs);
			if (ret) {
				printk(KERN_ERR "perfmon: pfm_exit_thread [%d] state=%d unload failed %d\n", task_pid_nr(task), state, ret);
			}
			free_ok = 1;
			break;
		default:
			printk(KERN_ERR "perfmon: pfm_exit_thread [%d] unexpected state=%d\n", task_pid_nr(task), state);
			break;
	}
	UNPROTECT_CTX(ctx, flags);

	{ u64 psr = pfm_get_psr();
	  BUG_ON(psr & (IA64_PSR_UP|IA64_PSR_PP));
	  BUG_ON(GET_PMU_OWNER());
	  BUG_ON(ia64_psr(regs)->up);
	  BUG_ON(ia64_psr(regs)->pp);
	}

	if (free_ok) pfm_context_free(ctx);
}

#define PFM_CMD(name, flags, arg_count, arg_type, getsz) { name, #name, flags, arg_count, sizeof(arg_type), getsz }
#define PFM_CMD_S(name, flags) { name, #name, flags, 0, 0, NULL }
#define PFM_CMD_PCLRWS	(PFM_CMD_FD|PFM_CMD_ARG_RW|PFM_CMD_STOP)
#define PFM_CMD_PCLRW	(PFM_CMD_FD|PFM_CMD_ARG_RW)
#define PFM_CMD_NONE	{ NULL, "no-cmd", 0, 0, 0, NULL}

static pfm_cmd_desc_t pfm_cmd_tab[]={
PFM_CMD_NONE,
PFM_CMD(pfm_write_pmcs, PFM_CMD_PCLRWS, PFM_CMD_ARG_MANY, pfarg_reg_t, NULL),
PFM_CMD(pfm_write_pmds, PFM_CMD_PCLRWS, PFM_CMD_ARG_MANY, pfarg_reg_t, NULL),
PFM_CMD(pfm_read_pmds, PFM_CMD_PCLRWS, PFM_CMD_ARG_MANY, pfarg_reg_t, NULL),
PFM_CMD_S(pfm_stop, PFM_CMD_PCLRWS),
PFM_CMD_S(pfm_start, PFM_CMD_PCLRWS),
PFM_CMD_NONE,
PFM_CMD_NONE,
PFM_CMD(pfm_context_create, PFM_CMD_ARG_RW, 1, pfarg_context_t, pfm_ctx_getsize),
PFM_CMD_NONE,
PFM_CMD_S(pfm_restart, PFM_CMD_PCLRW),
PFM_CMD_NONE,
PFM_CMD(pfm_get_features, PFM_CMD_ARG_RW, 1, pfarg_features_t, NULL),
PFM_CMD(pfm_debug, 0, 1, unsigned int, NULL),
PFM_CMD_NONE,
PFM_CMD(pfm_get_pmc_reset, PFM_CMD_ARG_RW, PFM_CMD_ARG_MANY, pfarg_reg_t, NULL),
PFM_CMD(pfm_context_load, PFM_CMD_PCLRWS, 1, pfarg_load_t, NULL),
PFM_CMD_S(pfm_context_unload, PFM_CMD_PCLRWS),
PFM_CMD_NONE,
PFM_CMD_NONE,
PFM_CMD_NONE,
PFM_CMD_NONE,
PFM_CMD_NONE,
PFM_CMD_NONE,
PFM_CMD_NONE,
PFM_CMD_NONE,
PFM_CMD_NONE,
PFM_CMD_NONE,
PFM_CMD_NONE,
PFM_CMD_NONE,
PFM_CMD_NONE,
PFM_CMD_NONE,
PFM_CMD(pfm_write_ibrs, PFM_CMD_PCLRWS, PFM_CMD_ARG_MANY, pfarg_dbreg_t, NULL),
PFM_CMD(pfm_write_dbrs, PFM_CMD_PCLRWS, PFM_CMD_ARG_MANY, pfarg_dbreg_t, NULL)
};
#define PFM_CMD_COUNT	(sizeof(pfm_cmd_tab)/sizeof(pfm_cmd_desc_t))

static int
pfm_check_task_state(pfm_context_t *ctx, int cmd, unsigned long flags)
{
	struct task_struct *task;
	int state, old_state;

recheck:
	state = ctx->ctx_state;
	task  = ctx->ctx_task;

	if (task == NULL) {
		DPRINT(("context %d no task, state=%d\n", ctx->ctx_fd, state));
		return 0;
	}

	DPRINT(("context %d state=%d [%d] task_state=%ld must_stop=%d\n",
		ctx->ctx_fd,
		state,
		task_pid_nr(task),
		task->state, PFM_CMD_STOPPED(cmd)));

	if (task == current || ctx->ctx_fl_system) return 0;

	switch(state) {
		case PFM_CTX_UNLOADED:
			return 0;
		case PFM_CTX_ZOMBIE:
			DPRINT(("cmd %d state zombie cannot operate on context\n", cmd));
			return -EINVAL;
		case PFM_CTX_MASKED:
			if (cmd != PFM_UNLOAD_CONTEXT) return 0;
	}

	if (PFM_CMD_STOPPED(cmd)) {
		if (!task_is_stopped_or_traced(task)) {
			DPRINT(("[%d] task not in stopped state\n", task_pid_nr(task)));
			return -EBUSY;
		}
		old_state = state;

		UNPROTECT_CTX(ctx, flags);

		wait_task_inactive(task, 0);

		PROTECT_CTX(ctx, flags);

		if (ctx->ctx_state != old_state) {
			DPRINT(("old_state=%d new_state=%d\n", old_state, ctx->ctx_state));
			goto recheck;
		}
	}
	return 0;
}

asmlinkage long
sys_perfmonctl (int fd, int cmd, void __user *arg, int count)
{
	struct file *file = NULL;
	pfm_context_t *ctx = NULL;
	unsigned long flags = 0UL;
	void *args_k = NULL;
	long ret; 
	size_t base_sz, sz, xtra_sz = 0;
	int narg, completed_args = 0, call_made = 0, cmd_flags;
	int (*func)(pfm_context_t *ctx, void *arg, int count, struct pt_regs *regs);
	int (*getsize)(void *arg, size_t *sz);
#define PFM_MAX_ARGSIZE	4096

	if (unlikely(pmu_conf == NULL)) return -ENOSYS;

	if (unlikely(cmd < 0 || cmd >= PFM_CMD_COUNT)) {
		DPRINT(("invalid cmd=%d\n", cmd));
		return -EINVAL;
	}

	func      = pfm_cmd_tab[cmd].cmd_func;
	narg      = pfm_cmd_tab[cmd].cmd_narg;
	base_sz   = pfm_cmd_tab[cmd].cmd_argsize;
	getsize   = pfm_cmd_tab[cmd].cmd_getsize;
	cmd_flags = pfm_cmd_tab[cmd].cmd_flags;

	if (unlikely(func == NULL)) {
		DPRINT(("invalid cmd=%d\n", cmd));
		return -EINVAL;
	}

	DPRINT(("cmd=%s idx=%d narg=0x%x argsz=%lu count=%d\n",
		PFM_CMD_NAME(cmd),
		cmd,
		narg,
		base_sz,
		count));

	if (unlikely((narg == PFM_CMD_ARG_MANY && count <= 0) || (narg > 0 && narg != count)))
		return -EINVAL;

restart_args:
	sz = xtra_sz + base_sz*count;
	if (unlikely(sz > PFM_MAX_ARGSIZE)) {
		printk(KERN_ERR "perfmon: [%d] argument too big %lu\n", task_pid_nr(current), sz);
		return -E2BIG;
	}

	if (likely(count && args_k == NULL)) {
		args_k = kmalloc(PFM_MAX_ARGSIZE, GFP_KERNEL);
		if (args_k == NULL) return -ENOMEM;
	}

	ret = -EFAULT;

	if (sz && copy_from_user(args_k, arg, sz)) {
		DPRINT(("cannot copy_from_user %lu bytes @%p\n", sz, arg));
		goto error_args;
	}

	if (completed_args == 0 && getsize) {
		ret = (*getsize)(args_k, &xtra_sz);
		if (ret) goto error_args;

		completed_args = 1;

		DPRINT(("restart_args sz=%lu xtra_sz=%lu\n", sz, xtra_sz));

		
		if (likely(xtra_sz)) goto restart_args;
	}

	if (unlikely((cmd_flags & PFM_CMD_FD) == 0)) goto skip_fd;

	ret = -EBADF;

	file = fget(fd);
	if (unlikely(file == NULL)) {
		DPRINT(("invalid fd %d\n", fd));
		goto error_args;
	}
	if (unlikely(PFM_IS_FILE(file) == 0)) {
		DPRINT(("fd %d not related to perfmon\n", fd));
		goto error_args;
	}

	ctx = file->private_data;
	if (unlikely(ctx == NULL)) {
		DPRINT(("no context for fd %d\n", fd));
		goto error_args;
	}
	prefetch(&ctx->ctx_state);

	PROTECT_CTX(ctx, flags);

	ret = pfm_check_task_state(ctx, cmd, flags);
	if (unlikely(ret)) goto abort_locked;

skip_fd:
	ret = (*func)(ctx, args_k, count, task_pt_regs(current));

	call_made = 1;

abort_locked:
	if (likely(ctx)) {
		DPRINT(("context unlocked\n"));
		UNPROTECT_CTX(ctx, flags);
	}

	
	if (call_made && PFM_CMD_RW_ARG(cmd) && copy_to_user(arg, args_k, base_sz*count)) ret = -EFAULT;

error_args:
	if (file)
		fput(file);

	kfree(args_k);

	DPRINT(("cmd=%s ret=%ld\n", PFM_CMD_NAME(cmd), ret));

	return ret;
}

static void
pfm_resume_after_ovfl(pfm_context_t *ctx, unsigned long ovfl_regs, struct pt_regs *regs)
{
	pfm_buffer_fmt_t *fmt = ctx->ctx_buf_fmt;
	pfm_ovfl_ctrl_t rst_ctrl;
	int state;
	int ret = 0;

	state = ctx->ctx_state;
	if (CTX_HAS_SMPL(ctx)) {

		rst_ctrl.bits.mask_monitoring = 0;
		rst_ctrl.bits.reset_ovfl_pmds = 0;

		if (state == PFM_CTX_LOADED)
			ret = pfm_buf_fmt_restart_active(fmt, current, &rst_ctrl, ctx->ctx_smpl_hdr, regs);
		else
			ret = pfm_buf_fmt_restart(fmt, current, &rst_ctrl, ctx->ctx_smpl_hdr, regs);
	} else {
		rst_ctrl.bits.mask_monitoring = 0;
		rst_ctrl.bits.reset_ovfl_pmds = 1;
	}

	if (ret == 0) {
		if (rst_ctrl.bits.reset_ovfl_pmds) {
			pfm_reset_regs(ctx, &ovfl_regs, PFM_PMD_LONG_RESET);
		}
		if (rst_ctrl.bits.mask_monitoring == 0) {
			DPRINT(("resuming monitoring\n"));
			if (ctx->ctx_state == PFM_CTX_MASKED) pfm_restore_monitoring(current);
		} else {
			DPRINT(("stopping monitoring\n"));
			
		}
		ctx->ctx_state = PFM_CTX_LOADED;
	}
}

static void
pfm_context_force_terminate(pfm_context_t *ctx, struct pt_regs *regs)
{
	int ret;

	DPRINT(("entering for [%d]\n", task_pid_nr(current)));

	ret = pfm_context_unload(ctx, NULL, 0, regs);
	if (ret) {
		printk(KERN_ERR "pfm_context_force_terminate: [%d] unloaded failed with %d\n", task_pid_nr(current), ret);
	}

	wake_up_interruptible(&ctx->ctx_zombieq);

}

static int pfm_ovfl_notify_user(pfm_context_t *ctx, unsigned long ovfl_pmds);

void
pfm_handle_work(void)
{
	pfm_context_t *ctx;
	struct pt_regs *regs;
	unsigned long flags, dummy_flags;
	unsigned long ovfl_regs;
	unsigned int reason;
	int ret;

	ctx = PFM_GET_CTX(current);
	if (ctx == NULL) {
		printk(KERN_ERR "perfmon: [%d] has no PFM context\n",
			task_pid_nr(current));
		return;
	}

	PROTECT_CTX(ctx, flags);

	PFM_SET_WORK_PENDING(current, 0);

	regs = task_pt_regs(current);

	reason = ctx->ctx_fl_trap_reason;
	ctx->ctx_fl_trap_reason = PFM_TRAP_REASON_NONE;
	ovfl_regs = ctx->ctx_ovfl_regs[0];

	DPRINT(("reason=%d state=%d\n", reason, ctx->ctx_state));

	if (ctx->ctx_fl_going_zombie || ctx->ctx_state == PFM_CTX_ZOMBIE)
		goto do_zombie;

	
	if (reason == PFM_TRAP_REASON_RESET)
		goto skip_blocking;

	UNPROTECT_CTX(ctx, flags);

	local_irq_enable();

	DPRINT(("before block sleeping\n"));

	ret = wait_for_completion_interruptible(&ctx->ctx_restart_done);

	DPRINT(("after block sleeping ret=%d\n", ret));

	PROTECT_CTX(ctx, dummy_flags);

	ovfl_regs = ctx->ctx_ovfl_regs[0];

	if (ctx->ctx_fl_going_zombie) {
do_zombie:
		DPRINT(("context is zombie, bailing out\n"));
		pfm_context_force_terminate(ctx, regs);
		goto nothing_to_do;
	}
	if (ret < 0)
		goto nothing_to_do;

skip_blocking:
	pfm_resume_after_ovfl(ctx, ovfl_regs, regs);
	ctx->ctx_ovfl_regs[0] = 0UL;

nothing_to_do:
	UNPROTECT_CTX(ctx, flags);
}

static int
pfm_notify_user(pfm_context_t *ctx, pfm_msg_t *msg)
{
	if (ctx->ctx_state == PFM_CTX_ZOMBIE) {
		DPRINT(("ignoring overflow notification, owner is zombie\n"));
		return 0;
	}

	DPRINT(("waking up somebody\n"));

	if (msg) wake_up_interruptible(&ctx->ctx_msgq_wait);

	kill_fasync (&ctx->ctx_async_queue, SIGIO, POLL_IN);

	return 0;
}

static int
pfm_ovfl_notify_user(pfm_context_t *ctx, unsigned long ovfl_pmds)
{
	pfm_msg_t *msg = NULL;

	if (ctx->ctx_fl_no_msg == 0) {
		msg = pfm_get_new_msg(ctx);
		if (msg == NULL) {
			printk(KERN_ERR "perfmon: pfm_ovfl_notify_user no more notification msgs\n");
			return -1;
		}

		msg->pfm_ovfl_msg.msg_type         = PFM_MSG_OVFL;
		msg->pfm_ovfl_msg.msg_ctx_fd       = ctx->ctx_fd;
		msg->pfm_ovfl_msg.msg_active_set   = 0;
		msg->pfm_ovfl_msg.msg_ovfl_pmds[0] = ovfl_pmds;
		msg->pfm_ovfl_msg.msg_ovfl_pmds[1] = 0UL;
		msg->pfm_ovfl_msg.msg_ovfl_pmds[2] = 0UL;
		msg->pfm_ovfl_msg.msg_ovfl_pmds[3] = 0UL;
		msg->pfm_ovfl_msg.msg_tstamp       = 0UL;
	}

	DPRINT(("ovfl msg: msg=%p no_msg=%d fd=%d ovfl_pmds=0x%lx\n",
		msg,
		ctx->ctx_fl_no_msg,
		ctx->ctx_fd,
		ovfl_pmds));

	return pfm_notify_user(ctx, msg);
}

static int
pfm_end_notify_user(pfm_context_t *ctx)
{
	pfm_msg_t *msg;

	msg = pfm_get_new_msg(ctx);
	if (msg == NULL) {
		printk(KERN_ERR "perfmon: pfm_end_notify_user no more notification msgs\n");
		return -1;
	}
	
	memset(msg, 0, sizeof(*msg));

	msg->pfm_end_msg.msg_type    = PFM_MSG_END;
	msg->pfm_end_msg.msg_ctx_fd  = ctx->ctx_fd;
	msg->pfm_ovfl_msg.msg_tstamp = 0UL;

	DPRINT(("end msg: msg=%p no_msg=%d ctx_fd=%d\n",
		msg,
		ctx->ctx_fl_no_msg,
		ctx->ctx_fd));

	return pfm_notify_user(ctx, msg);
}

static void pfm_overflow_handler(struct task_struct *task, pfm_context_t *ctx,
				unsigned long pmc0, struct pt_regs *regs)
{
	pfm_ovfl_arg_t *ovfl_arg;
	unsigned long mask;
	unsigned long old_val, ovfl_val, new_val;
	unsigned long ovfl_notify = 0UL, ovfl_pmds = 0UL, smpl_pmds = 0UL, reset_pmds;
	unsigned long tstamp;
	pfm_ovfl_ctrl_t	ovfl_ctrl;
	unsigned int i, has_smpl;
	int must_notify = 0;

	if (unlikely(ctx->ctx_state == PFM_CTX_ZOMBIE)) goto stop_monitoring;

	if (unlikely((pmc0 & 0x1) == 0)) goto sanity_check;

	tstamp   = ia64_get_itc();
	mask     = pmc0 >> PMU_FIRST_COUNTER;
	ovfl_val = pmu_conf->ovfl_val;
	has_smpl = CTX_HAS_SMPL(ctx);

	DPRINT_ovfl(("pmc0=0x%lx pid=%d iip=0x%lx, %s "
		     "used_pmds=0x%lx\n",
			pmc0,
			task ? task_pid_nr(task): -1,
			(regs ? regs->cr_iip : 0),
			CTX_OVFL_NOBLOCK(ctx) ? "nonblocking" : "blocking",
			ctx->ctx_used_pmds[0]));


	for (i = PMU_FIRST_COUNTER; mask ; i++, mask >>= 1) {

		
		if ((mask & 0x1) == 0) continue;

		old_val              = new_val = ctx->ctx_pmds[i].val;
		new_val             += 1 + ovfl_val;
		ctx->ctx_pmds[i].val = new_val;

		if (likely(old_val > new_val)) {
			ovfl_pmds |= 1UL << i;
			if (PMC_OVFL_NOTIFY(ctx, i)) ovfl_notify |= 1UL << i;
		}

		DPRINT_ovfl(("ctx_pmd[%d].val=0x%lx old_val=0x%lx pmd=0x%lx ovfl_pmds=0x%lx ovfl_notify=0x%lx\n",
			i,
			new_val,
			old_val,
			ia64_get_pmd(i) & ovfl_val,
			ovfl_pmds,
			ovfl_notify));
	}

	if (ovfl_pmds == 0UL) return;

	ovfl_ctrl.val = 0;
	reset_pmds    = 0UL;

	if (has_smpl) {
		unsigned long start_cycles, end_cycles;
		unsigned long pmd_mask;
		int j, k, ret = 0;
		int this_cpu = smp_processor_id();

		pmd_mask = ovfl_pmds >> PMU_FIRST_COUNTER;
		ovfl_arg = &ctx->ctx_ovfl_arg;

		prefetch(ctx->ctx_smpl_hdr);

		for(i=PMU_FIRST_COUNTER; pmd_mask && ret == 0; i++, pmd_mask >>=1) {

			mask = 1UL << i;

			if ((pmd_mask & 0x1) == 0) continue;

			ovfl_arg->ovfl_pmd      = (unsigned char )i;
			ovfl_arg->ovfl_notify   = ovfl_notify & mask ? 1 : 0;
			ovfl_arg->active_set    = 0;
			ovfl_arg->ovfl_ctrl.val = 0; 
			ovfl_arg->smpl_pmds[0]  = smpl_pmds = ctx->ctx_pmds[i].smpl_pmds[0];

			ovfl_arg->pmd_value      = ctx->ctx_pmds[i].val;
			ovfl_arg->pmd_last_reset = ctx->ctx_pmds[i].lval;
			ovfl_arg->pmd_eventid    = ctx->ctx_pmds[i].eventid;

			if (smpl_pmds) {
				for(j=0, k=0; smpl_pmds; j++, smpl_pmds >>=1) {
					if ((smpl_pmds & 0x1) == 0) continue;
					ovfl_arg->smpl_pmds_values[k++] = PMD_IS_COUNTING(j) ?  pfm_read_soft_counter(ctx, j) : ia64_get_pmd(j);
					DPRINT_ovfl(("smpl_pmd[%d]=pmd%u=0x%lx\n", k-1, j, ovfl_arg->smpl_pmds_values[k-1]));
				}
			}

			pfm_stats[this_cpu].pfm_smpl_handler_calls++;

			start_cycles = ia64_get_itc();

			ret = (*ctx->ctx_buf_fmt->fmt_handler)(task, ctx->ctx_smpl_hdr, ovfl_arg, regs, tstamp);

			end_cycles = ia64_get_itc();

			ovfl_ctrl.bits.notify_user     |= ovfl_arg->ovfl_ctrl.bits.notify_user;
			ovfl_ctrl.bits.block_task      |= ovfl_arg->ovfl_ctrl.bits.block_task;
			ovfl_ctrl.bits.mask_monitoring |= ovfl_arg->ovfl_ctrl.bits.mask_monitoring;
			if (ovfl_arg->ovfl_ctrl.bits.reset_ovfl_pmds) reset_pmds |= mask;

			pfm_stats[this_cpu].pfm_smpl_handler_cycles += end_cycles - start_cycles;
		}
		if (ret && pmd_mask) {
			DPRINT(("handler aborts leftover ovfl_pmds=0x%lx\n",
				pmd_mask<<PMU_FIRST_COUNTER));
		}
		ovfl_pmds &= ~reset_pmds;
	} else {
		ovfl_ctrl.bits.notify_user     = ovfl_notify ? 1 : 0;
		ovfl_ctrl.bits.block_task      = ovfl_notify ? 1 : 0;
		ovfl_ctrl.bits.mask_monitoring = ovfl_notify ? 1 : 0; 
		ovfl_ctrl.bits.reset_ovfl_pmds = ovfl_notify ? 0 : 1;
		if (ovfl_notify == 0) reset_pmds = ovfl_pmds;
	}

	DPRINT_ovfl(("ovfl_pmds=0x%lx reset_pmds=0x%lx\n", ovfl_pmds, reset_pmds));

	if (reset_pmds) {
		unsigned long bm = reset_pmds;
		pfm_reset_regs(ctx, &bm, PFM_PMD_SHORT_RESET);
	}

	if (ovfl_notify && ovfl_ctrl.bits.notify_user) {
		ctx->ctx_ovfl_regs[0] = ovfl_pmds;

		if (CTX_OVFL_NOBLOCK(ctx) == 0 && ovfl_ctrl.bits.block_task) {

			ctx->ctx_fl_trap_reason = PFM_TRAP_REASON_BLOCK;

			PFM_SET_WORK_PENDING(task, 1);

			set_notify_resume(task);
		}
		must_notify = 1;
	}

	DPRINT_ovfl(("owner [%d] pending=%ld reason=%u ovfl_pmds=0x%lx ovfl_notify=0x%lx masked=%d\n",
			GET_PMU_OWNER() ? task_pid_nr(GET_PMU_OWNER()) : -1,
			PFM_GET_WORK_PENDING(task),
			ctx->ctx_fl_trap_reason,
			ovfl_pmds,
			ovfl_notify,
			ovfl_ctrl.bits.mask_monitoring ? 1 : 0));
	if (ovfl_ctrl.bits.mask_monitoring) {
		pfm_mask_monitoring(task);
		ctx->ctx_state = PFM_CTX_MASKED;
		ctx->ctx_fl_can_restart = 1;
	}

	if (must_notify) pfm_ovfl_notify_user(ctx, ovfl_notify);

	return;

sanity_check:
	printk(KERN_ERR "perfmon: CPU%d overflow handler [%d] pmc0=0x%lx\n",
			smp_processor_id(),
			task ? task_pid_nr(task) : -1,
			pmc0);
	return;

stop_monitoring:
	DPRINT(("ctx is zombie for [%d], converted to spurious\n", task ? task_pid_nr(task): -1));
	pfm_clear_psr_up();
	ia64_psr(regs)->up = 0;
	ia64_psr(regs)->sp = 1;
	return;
}

static int
pfm_do_interrupt_handler(void *arg, struct pt_regs *regs)
{
	struct task_struct *task;
	pfm_context_t *ctx;
	unsigned long flags;
	u64 pmc0;
	int this_cpu = smp_processor_id();
	int retval = 0;

	pfm_stats[this_cpu].pfm_ovfl_intr_count++;

	pmc0 = ia64_get_pmc(0);

	task = GET_PMU_OWNER();
	ctx  = GET_PMU_CTX();

	if (PMC0_HAS_OVFL(pmc0) && task) {

		
		if (!ctx) goto report_spurious1;

		if (ctx->ctx_fl_system == 0 && (task->thread.flags & IA64_THREAD_PM_VALID) == 0) 
			goto report_spurious2;

		PROTECT_CTX_NOPRINT(ctx, flags);

		pfm_overflow_handler(task, ctx, pmc0, regs);

		UNPROTECT_CTX_NOPRINT(ctx, flags);

	} else {
		pfm_stats[this_cpu].pfm_spurious_ovfl_intr_count++;
		retval = -1;
	}
	pfm_unfreeze_pmu();

	return retval;

report_spurious1:
	printk(KERN_INFO "perfmon: spurious overflow interrupt on CPU%d: process %d has no PFM context\n",
		this_cpu, task_pid_nr(task));
	pfm_unfreeze_pmu();
	return -1;
report_spurious2:
	printk(KERN_INFO "perfmon: spurious overflow interrupt on CPU%d: process %d, invalid flag\n", 
		this_cpu, 
		task_pid_nr(task));
	pfm_unfreeze_pmu();
	return -1;
}

static irqreturn_t
pfm_interrupt_handler(int irq, void *arg)
{
	unsigned long start_cycles, total_cycles;
	unsigned long min, max;
	int this_cpu;
	int ret;
	struct pt_regs *regs = get_irq_regs();

	this_cpu = get_cpu();
	if (likely(!pfm_alt_intr_handler)) {
		min = pfm_stats[this_cpu].pfm_ovfl_intr_cycles_min;
		max = pfm_stats[this_cpu].pfm_ovfl_intr_cycles_max;

		start_cycles = ia64_get_itc();

		ret = pfm_do_interrupt_handler(arg, regs);

		total_cycles = ia64_get_itc();

		if (likely(ret == 0)) {
			total_cycles -= start_cycles;

			if (total_cycles < min) pfm_stats[this_cpu].pfm_ovfl_intr_cycles_min = total_cycles;
			if (total_cycles > max) pfm_stats[this_cpu].pfm_ovfl_intr_cycles_max = total_cycles;

			pfm_stats[this_cpu].pfm_ovfl_intr_cycles += total_cycles;
		}
	}
	else {
		(*pfm_alt_intr_handler->handler)(irq, arg, regs);
	}

	put_cpu();
	return IRQ_HANDLED;
}


#define PFM_PROC_SHOW_HEADER	((void *)(long)nr_cpu_ids+1)

static void *
pfm_proc_start(struct seq_file *m, loff_t *pos)
{
	if (*pos == 0) {
		return PFM_PROC_SHOW_HEADER;
	}

	while (*pos <= nr_cpu_ids) {
		if (cpu_online(*pos - 1)) {
			return (void *)*pos;
		}
		++*pos;
	}
	return NULL;
}

static void *
pfm_proc_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return pfm_proc_start(m, pos);
}

static void
pfm_proc_stop(struct seq_file *m, void *v)
{
}

static void
pfm_proc_show_header(struct seq_file *m)
{
	struct list_head * pos;
	pfm_buffer_fmt_t * entry;
	unsigned long flags;

 	seq_printf(m,
		"perfmon version           : %u.%u\n"
		"model                     : %s\n"
		"fastctxsw                 : %s\n"
		"expert mode               : %s\n"
		"ovfl_mask                 : 0x%lx\n"
		"PMU flags                 : 0x%x\n",
		PFM_VERSION_MAJ, PFM_VERSION_MIN,
		pmu_conf->pmu_name,
		pfm_sysctl.fastctxsw > 0 ? "Yes": "No",
		pfm_sysctl.expert_mode > 0 ? "Yes": "No",
		pmu_conf->ovfl_val,
		pmu_conf->flags);

  	LOCK_PFS(flags);

 	seq_printf(m,
 		"proc_sessions             : %u\n"
 		"sys_sessions              : %u\n"
 		"sys_use_dbregs            : %u\n"
 		"ptrace_use_dbregs         : %u\n",
 		pfm_sessions.pfs_task_sessions,
 		pfm_sessions.pfs_sys_sessions,
 		pfm_sessions.pfs_sys_use_dbregs,
 		pfm_sessions.pfs_ptrace_use_dbregs);

  	UNLOCK_PFS(flags);

	spin_lock(&pfm_buffer_fmt_lock);

	list_for_each(pos, &pfm_buffer_fmt_list) {
		entry = list_entry(pos, pfm_buffer_fmt_t, fmt_list);
		seq_printf(m, "format                    : %02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x %s\n",
			entry->fmt_uuid[0],
			entry->fmt_uuid[1],
			entry->fmt_uuid[2],
			entry->fmt_uuid[3],
			entry->fmt_uuid[4],
			entry->fmt_uuid[5],
			entry->fmt_uuid[6],
			entry->fmt_uuid[7],
			entry->fmt_uuid[8],
			entry->fmt_uuid[9],
			entry->fmt_uuid[10],
			entry->fmt_uuid[11],
			entry->fmt_uuid[12],
			entry->fmt_uuid[13],
			entry->fmt_uuid[14],
			entry->fmt_uuid[15],
			entry->fmt_name);
	}
	spin_unlock(&pfm_buffer_fmt_lock);

}

static int
pfm_proc_show(struct seq_file *m, void *v)
{
	unsigned long psr;
	unsigned int i;
	int cpu;

	if (v == PFM_PROC_SHOW_HEADER) {
		pfm_proc_show_header(m);
		return 0;
	}

	

	cpu = (long)v - 1;
	seq_printf(m,
		"CPU%-2d overflow intrs      : %lu\n"
		"CPU%-2d overflow cycles     : %lu\n"
		"CPU%-2d overflow min        : %lu\n"
		"CPU%-2d overflow max        : %lu\n"
		"CPU%-2d smpl handler calls  : %lu\n"
		"CPU%-2d smpl handler cycles : %lu\n"
		"CPU%-2d spurious intrs      : %lu\n"
		"CPU%-2d replay   intrs      : %lu\n"
		"CPU%-2d syst_wide           : %d\n"
		"CPU%-2d dcr_pp              : %d\n"
		"CPU%-2d exclude idle        : %d\n"
		"CPU%-2d owner               : %d\n"
		"CPU%-2d context             : %p\n"
		"CPU%-2d activations         : %lu\n",
		cpu, pfm_stats[cpu].pfm_ovfl_intr_count,
		cpu, pfm_stats[cpu].pfm_ovfl_intr_cycles,
		cpu, pfm_stats[cpu].pfm_ovfl_intr_cycles_min,
		cpu, pfm_stats[cpu].pfm_ovfl_intr_cycles_max,
		cpu, pfm_stats[cpu].pfm_smpl_handler_calls,
		cpu, pfm_stats[cpu].pfm_smpl_handler_cycles,
		cpu, pfm_stats[cpu].pfm_spurious_ovfl_intr_count,
		cpu, pfm_stats[cpu].pfm_replay_ovfl_intr_count,
		cpu, pfm_get_cpu_data(pfm_syst_info, cpu) & PFM_CPUINFO_SYST_WIDE ? 1 : 0,
		cpu, pfm_get_cpu_data(pfm_syst_info, cpu) & PFM_CPUINFO_DCR_PP ? 1 : 0,
		cpu, pfm_get_cpu_data(pfm_syst_info, cpu) & PFM_CPUINFO_EXCL_IDLE ? 1 : 0,
		cpu, pfm_get_cpu_data(pmu_owner, cpu) ? pfm_get_cpu_data(pmu_owner, cpu)->pid: -1,
		cpu, pfm_get_cpu_data(pmu_ctx, cpu),
		cpu, pfm_get_cpu_data(pmu_activation_number, cpu));

	if (num_online_cpus() == 1 && pfm_sysctl.debug > 0) {

		psr = pfm_get_psr();

		ia64_srlz_d();

		seq_printf(m, 
			"CPU%-2d psr                 : 0x%lx\n"
			"CPU%-2d pmc0                : 0x%lx\n", 
			cpu, psr,
			cpu, ia64_get_pmc(0));

		for (i=0; PMC_IS_LAST(i) == 0;  i++) {
			if (PMC_IS_COUNTING(i) == 0) continue;
   			seq_printf(m, 
				"CPU%-2d pmc%u                : 0x%lx\n"
   				"CPU%-2d pmd%u                : 0x%lx\n", 
				cpu, i, ia64_get_pmc(i),
				cpu, i, ia64_get_pmd(i));
  		}
	}
	return 0;
}

const struct seq_operations pfm_seq_ops = {
	.start =	pfm_proc_start,
 	.next =		pfm_proc_next,
 	.stop =		pfm_proc_stop,
 	.show =		pfm_proc_show
};

static int
pfm_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &pfm_seq_ops);
}


void
pfm_syst_wide_update_task(struct task_struct *task, unsigned long info, int is_ctxswin)
{
	struct pt_regs *regs;
	unsigned long dcr;
	unsigned long dcr_pp;

	dcr_pp = info & PFM_CPUINFO_DCR_PP ? 1 : 0;

	if ((info & PFM_CPUINFO_EXCL_IDLE) == 0 || task->pid) {
		regs = task_pt_regs(task);
		ia64_psr(regs)->pp = is_ctxswin ? dcr_pp : 0;
		return;
	}
	if (dcr_pp) {
		dcr = ia64_getreg(_IA64_REG_CR_DCR);
		if (is_ctxswin) {
			
			ia64_setreg(_IA64_REG_CR_DCR, dcr & ~IA64_DCR_PP);
			pfm_clear_psr_pp();
			ia64_srlz_i();
			return;
		}
		ia64_setreg(_IA64_REG_CR_DCR, dcr |IA64_DCR_PP);
		pfm_set_psr_pp();
		ia64_srlz_i();
	}
}

#ifdef CONFIG_SMP

static void
pfm_force_cleanup(pfm_context_t *ctx, struct pt_regs *regs)
{
	struct task_struct *task = ctx->ctx_task;

	ia64_psr(regs)->up = 0;
	ia64_psr(regs)->sp = 1;

	if (GET_PMU_OWNER() == task) {
		DPRINT(("cleared ownership for [%d]\n",
					task_pid_nr(ctx->ctx_task)));
		SET_PMU_OWNER(NULL, NULL);
	}

	PFM_SET_WORK_PENDING(task, 0);

	task->thread.pfm_context  = NULL;
	task->thread.flags       &= ~IA64_THREAD_PM_VALID;

	DPRINT(("force cleanup for [%d]\n",  task_pid_nr(task)));
}


void
pfm_save_regs(struct task_struct *task)
{
	pfm_context_t *ctx;
	unsigned long flags;
	u64 psr;


	ctx = PFM_GET_CTX(task);
	if (ctx == NULL) return;

	flags = pfm_protect_ctx_ctxsw(ctx);

	if (ctx->ctx_state == PFM_CTX_ZOMBIE) {
		struct pt_regs *regs = task_pt_regs(task);

		pfm_clear_psr_up();

		pfm_force_cleanup(ctx, regs);

		BUG_ON(ctx->ctx_smpl_hdr);

		pfm_unprotect_ctx_ctxsw(ctx, flags);

		pfm_context_free(ctx);
		return;
	}

	ia64_srlz_d();
	psr = pfm_get_psr();

	BUG_ON(psr & (IA64_PSR_I));

	pfm_clear_psr_up();

	ctx->ctx_saved_psr_up = psr & IA64_PSR_UP;

	SET_PMU_OWNER(NULL, NULL);

	pfm_save_pmds(ctx->th_pmds, ctx->ctx_used_pmds[0]);

	ctx->th_pmcs[0] = ia64_get_pmc(0);

	if (ctx->th_pmcs[0] & ~0x1UL) pfm_unfreeze_pmu();

	pfm_unprotect_ctx_ctxsw(ctx, flags);
}

#else 
void
pfm_save_regs(struct task_struct *task)
{
	pfm_context_t *ctx;
	u64 psr;

	ctx = PFM_GET_CTX(task);
	if (ctx == NULL) return;

	psr = pfm_get_psr();

	BUG_ON(psr & (IA64_PSR_I));

	pfm_clear_psr_up();

	ctx->ctx_saved_psr_up = psr & IA64_PSR_UP;
}

static void
pfm_lazy_save_regs (struct task_struct *task)
{
	pfm_context_t *ctx;
	unsigned long flags;

	{ u64 psr  = pfm_get_psr();
	  BUG_ON(psr & IA64_PSR_UP);
	}

	ctx = PFM_GET_CTX(task);

	PROTECT_CTX(ctx,flags);

	SET_PMU_OWNER(NULL, NULL);

	pfm_save_pmds(ctx->th_pmds, ctx->ctx_used_pmds[0]);

	ctx->th_pmcs[0] = ia64_get_pmc(0);

	if (ctx->th_pmcs[0] & ~0x1UL) pfm_unfreeze_pmu();

	UNPROTECT_CTX(ctx,flags);
}
#endif 

#ifdef CONFIG_SMP
void
pfm_load_regs (struct task_struct *task)
{
	pfm_context_t *ctx;
	unsigned long pmc_mask = 0UL, pmd_mask = 0UL;
	unsigned long flags;
	u64 psr, psr_up;
	int need_irq_resend;

	ctx = PFM_GET_CTX(task);
	if (unlikely(ctx == NULL)) return;

	BUG_ON(GET_PMU_OWNER());

	if (unlikely((task->thread.flags & IA64_THREAD_PM_VALID) == 0)) return;

	flags = pfm_protect_ctx_ctxsw(ctx);
	psr   = pfm_get_psr();

	need_irq_resend = pmu_conf->flags & PFM_PMU_IRQ_RESEND;

	BUG_ON(psr & (IA64_PSR_UP|IA64_PSR_PP));
	BUG_ON(psr & IA64_PSR_I);

	if (unlikely(ctx->ctx_state == PFM_CTX_ZOMBIE)) {
		struct pt_regs *regs = task_pt_regs(task);

		BUG_ON(ctx->ctx_smpl_hdr);

		pfm_force_cleanup(ctx, regs);

		pfm_unprotect_ctx_ctxsw(ctx, flags);

		pfm_context_free(ctx);

		return;
	}

	if (ctx->ctx_fl_using_dbreg) {
		pfm_restore_ibrs(ctx->ctx_ibrs, pmu_conf->num_ibrs);
		pfm_restore_dbrs(ctx->ctx_dbrs, pmu_conf->num_dbrs);
	}
	psr_up = ctx->ctx_saved_psr_up;

	if (GET_LAST_CPU(ctx) == smp_processor_id() && ctx->ctx_last_activation == GET_ACTIVATION()) {

		pmc_mask = ctx->ctx_reload_pmcs[0];
		pmd_mask = ctx->ctx_reload_pmds[0];

	} else {
		pmd_mask = pfm_sysctl.fastctxsw ?  ctx->ctx_used_pmds[0] : ctx->ctx_all_pmds[0];

		pmc_mask = ctx->ctx_all_pmcs[0];
	}
	if (pmd_mask) pfm_restore_pmds(ctx->th_pmds, pmd_mask);
	if (pmc_mask) pfm_restore_pmcs(ctx->th_pmcs, pmc_mask);

	if (unlikely(PMC0_HAS_OVFL(ctx->th_pmcs[0]))) {
		ia64_set_pmc(0, ctx->th_pmcs[0]);
		ia64_srlz_d();
		ctx->th_pmcs[0] = 0UL;

		if (need_irq_resend) ia64_resend_irq(IA64_PERFMON_VECTOR);

		pfm_stats[smp_processor_id()].pfm_replay_ovfl_intr_count++;
	}

	ctx->ctx_reload_pmcs[0] = 0UL;
	ctx->ctx_reload_pmds[0] = 0UL;

	SET_LAST_CPU(ctx, smp_processor_id());

	INC_ACTIVATION();
	SET_ACTIVATION(ctx);

	SET_PMU_OWNER(task, ctx);

	if (likely(psr_up)) pfm_set_psr_up();

	pfm_unprotect_ctx_ctxsw(ctx, flags);
}
#else 
void
pfm_load_regs (struct task_struct *task)
{
	pfm_context_t *ctx;
	struct task_struct *owner;
	unsigned long pmd_mask, pmc_mask;
	u64 psr, psr_up;
	int need_irq_resend;

	owner = GET_PMU_OWNER();
	ctx   = PFM_GET_CTX(task);
	psr   = pfm_get_psr();

	BUG_ON(psr & (IA64_PSR_UP|IA64_PSR_PP));
	BUG_ON(psr & IA64_PSR_I);

	if (ctx->ctx_fl_using_dbreg) {
		pfm_restore_ibrs(ctx->ctx_ibrs, pmu_conf->num_ibrs);
		pfm_restore_dbrs(ctx->ctx_dbrs, pmu_conf->num_dbrs);
	}

	psr_up = ctx->ctx_saved_psr_up;
	need_irq_resend = pmu_conf->flags & PFM_PMU_IRQ_RESEND;

	if (likely(owner == task)) {
		if (likely(psr_up)) pfm_set_psr_up();
		return;
	}

	if (owner) pfm_lazy_save_regs(owner);

	pmd_mask = pfm_sysctl.fastctxsw ?  ctx->ctx_used_pmds[0] : ctx->ctx_all_pmds[0];

	pmc_mask = ctx->ctx_all_pmcs[0];

	pfm_restore_pmds(ctx->th_pmds, pmd_mask);
	pfm_restore_pmcs(ctx->th_pmcs, pmc_mask);

	if (unlikely(PMC0_HAS_OVFL(ctx->th_pmcs[0]))) {
		ia64_set_pmc(0, ctx->th_pmcs[0]);
		ia64_srlz_d();

		ctx->th_pmcs[0] = 0UL;

		if (need_irq_resend) ia64_resend_irq(IA64_PERFMON_VECTOR);

		pfm_stats[smp_processor_id()].pfm_replay_ovfl_intr_count++;
	}

	SET_PMU_OWNER(task, ctx);

	if (likely(psr_up)) pfm_set_psr_up();
}
#endif 

static void
pfm_flush_pmds(struct task_struct *task, pfm_context_t *ctx)
{
	u64 pmc0;
	unsigned long mask2, val, pmd_val, ovfl_val;
	int i, can_access_pmu = 0;
	int is_self;

	is_self = ctx->ctx_task == task ? 1 : 0;

	can_access_pmu = (GET_PMU_OWNER() == task) || (ctx->ctx_fl_system && ctx->ctx_cpu == smp_processor_id());
	if (can_access_pmu) {
		SET_PMU_OWNER(NULL, NULL);
		DPRINT(("releasing ownership\n"));

		ia64_srlz_d();
		pmc0 = ia64_get_pmc(0); 

		pfm_unfreeze_pmu();
	} else {
		pmc0 = ctx->th_pmcs[0];
		ctx->th_pmcs[0] = 0;
	}
	ovfl_val = pmu_conf->ovfl_val;
	mask2 = ctx->ctx_used_pmds[0];

	DPRINT(("is_self=%d ovfl_val=0x%lx mask2=0x%lx\n", is_self, ovfl_val, mask2));

	for (i = 0; mask2; i++, mask2>>=1) {

		
		if ((mask2 & 0x1) == 0) continue;

		val = pmd_val = can_access_pmu ? ia64_get_pmd(i) : ctx->th_pmds[i];

		if (PMD_IS_COUNTING(i)) {
			DPRINT(("[%d] pmd[%d] ctx_pmd=0x%lx hw_pmd=0x%lx\n",
				task_pid_nr(task),
				i,
				ctx->ctx_pmds[i].val,
				val & ovfl_val));

			val = ctx->ctx_pmds[i].val + (val & ovfl_val);

			pmd_val = 0UL;

			if (pmc0 & (1UL << i)) {
				val += 1 + ovfl_val;
				DPRINT(("[%d] pmd[%d] overflowed\n", task_pid_nr(task), i));
			}
		}

		DPRINT(("[%d] ctx_pmd[%d]=0x%lx  pmd_val=0x%lx\n", task_pid_nr(task), i, val, pmd_val));

		if (is_self) ctx->th_pmds[i] = pmd_val;

		ctx->ctx_pmds[i].val = val;
	}
}

static struct irqaction perfmon_irqaction = {
	.handler = pfm_interrupt_handler,
	.flags   = IRQF_DISABLED,
	.name    = "perfmon"
};

static void
pfm_alt_save_pmu_state(void *data)
{
	struct pt_regs *regs;

	regs = task_pt_regs(current);

	DPRINT(("called\n"));

	pfm_clear_psr_up();
	pfm_clear_psr_pp();
	ia64_psr(regs)->pp = 0;

	pfm_freeze_pmu();

	ia64_srlz_d();
}

void
pfm_alt_restore_pmu_state(void *data)
{
	struct pt_regs *regs;

	regs = task_pt_regs(current);

	DPRINT(("called\n"));

	pfm_clear_psr_up();
	pfm_clear_psr_pp();
	ia64_psr(regs)->pp = 0;

	pfm_unfreeze_pmu();

	ia64_srlz_d();
}

int
pfm_install_alt_pmu_interrupt(pfm_intr_handler_desc_t *hdl)
{
	int ret, i;
	int reserve_cpu;

	
	if (hdl == NULL || hdl->handler == NULL) return -EINVAL;

	
	if (pfm_alt_intr_handler) return -EBUSY;

	
	if (!spin_trylock(&pfm_alt_install_check)) {
		return -EBUSY;
	}

	
	for_each_online_cpu(reserve_cpu) {
		ret = pfm_reserve_session(NULL, 1, reserve_cpu);
		if (ret) goto cleanup_reserve;
	}

	
	ret = on_each_cpu(pfm_alt_save_pmu_state, NULL, 1);
	if (ret) {
		DPRINT(("on_each_cpu() failed: %d\n", ret));
		goto cleanup_reserve;
	}

	
	pfm_alt_intr_handler = hdl;

	spin_unlock(&pfm_alt_install_check);

	return 0;

cleanup_reserve:
	for_each_online_cpu(i) {
		
		if (i >= reserve_cpu) break;

		pfm_unreserve_session(NULL, 1, i);
	}

	spin_unlock(&pfm_alt_install_check);

	return ret;
}
EXPORT_SYMBOL_GPL(pfm_install_alt_pmu_interrupt);

int
pfm_remove_alt_pmu_interrupt(pfm_intr_handler_desc_t *hdl)
{
	int i;
	int ret;

	if (hdl == NULL) return -EINVAL;

	
	if (pfm_alt_intr_handler != hdl) return -EINVAL;

	
	if (!spin_trylock(&pfm_alt_install_check)) {
		return -EBUSY;
	}

	pfm_alt_intr_handler = NULL;

	ret = on_each_cpu(pfm_alt_restore_pmu_state, NULL, 1);
	if (ret) {
		DPRINT(("on_each_cpu() failed: %d\n", ret));
	}

	for_each_online_cpu(i) {
		pfm_unreserve_session(NULL, 1, i);
	}

	spin_unlock(&pfm_alt_install_check);

	return 0;
}
EXPORT_SYMBOL_GPL(pfm_remove_alt_pmu_interrupt);

static int init_pfm_fs(void);

static int __init
pfm_probe_pmu(void)
{
	pmu_config_t **p;
	int family;

	family = local_cpu_data->family;
	p      = pmu_confs;

	while(*p) {
		if ((*p)->probe) {
			if ((*p)->probe() == 0) goto found;
		} else if ((*p)->pmu_family == family || (*p)->pmu_family == 0xff) {
			goto found;
		}
		p++;
	}
	return -1;
found:
	pmu_conf = *p;
	return 0;
}

static const struct file_operations pfm_proc_fops = {
	.open		= pfm_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

int __init
pfm_init(void)
{
	unsigned int n, n_counters, i;

	printk("perfmon: version %u.%u IRQ %u\n",
		PFM_VERSION_MAJ,
		PFM_VERSION_MIN,
		IA64_PERFMON_VECTOR);

	if (pfm_probe_pmu()) {
		printk(KERN_INFO "perfmon: disabled, there is no support for processor family %d\n", 
				local_cpu_data->family);
		return -ENODEV;
	}

	n = 0;
	for (i=0; PMC_IS_LAST(i) == 0;  i++) {
		if (PMC_IS_IMPL(i) == 0) continue;
		pmu_conf->impl_pmcs[i>>6] |= 1UL << (i&63);
		n++;
	}
	pmu_conf->num_pmcs = n;

	n = 0; n_counters = 0;
	for (i=0; PMD_IS_LAST(i) == 0;  i++) {
		if (PMD_IS_IMPL(i) == 0) continue;
		pmu_conf->impl_pmds[i>>6] |= 1UL << (i&63);
		n++;
		if (PMD_IS_COUNTING(i)) n_counters++;
	}
	pmu_conf->num_pmds      = n;
	pmu_conf->num_counters  = n_counters;

	if (pmu_conf->use_rr_dbregs) {
		if (pmu_conf->num_ibrs > IA64_NUM_DBG_REGS) {
			printk(KERN_INFO "perfmon: unsupported number of code debug registers (%u)\n", pmu_conf->num_ibrs);
			pmu_conf = NULL;
			return -1;
		}
		if (pmu_conf->num_dbrs > IA64_NUM_DBG_REGS) {
			printk(KERN_INFO "perfmon: unsupported number of data debug registers (%u)\n", pmu_conf->num_ibrs);
			pmu_conf = NULL;
			return -1;
		}
	}

	printk("perfmon: %s PMU detected, %u PMCs, %u PMDs, %u counters (%lu bits)\n",
	       pmu_conf->pmu_name,
	       pmu_conf->num_pmcs,
	       pmu_conf->num_pmds,
	       pmu_conf->num_counters,
	       ffz(pmu_conf->ovfl_val));

	
	if (pmu_conf->num_pmds >= PFM_NUM_PMD_REGS || pmu_conf->num_pmcs >= PFM_NUM_PMC_REGS) {
		printk(KERN_ERR "perfmon: not enough pmc/pmd, perfmon disabled\n");
		pmu_conf = NULL;
		return -1;
	}

	perfmon_dir = proc_create("perfmon", S_IRUGO, NULL, &pfm_proc_fops);
	if (perfmon_dir == NULL) {
		printk(KERN_ERR "perfmon: cannot create /proc entry, perfmon disabled\n");
		pmu_conf = NULL;
		return -1;
	}

	pfm_sysctl_header = register_sysctl_table(pfm_sysctl_root);

	spin_lock_init(&pfm_sessions.pfs_lock);
	spin_lock_init(&pfm_buffer_fmt_lock);

	init_pfm_fs();

	for(i=0; i < NR_CPUS; i++) pfm_stats[i].pfm_ovfl_intr_cycles_min = ~0UL;

	return 0;
}

__initcall(pfm_init);

void
pfm_init_percpu (void)
{
	static int first_time=1;
	pfm_clear_psr_pp();
	pfm_clear_psr_up();

	pfm_unfreeze_pmu();

	if (first_time) {
		register_percpu_irq(IA64_PERFMON_VECTOR, &perfmon_irqaction);
		first_time=0;
	}

	ia64_setreg(_IA64_REG_CR_PMV, IA64_PERFMON_VECTOR);
	ia64_srlz_d();
}

void
dump_pmu_state(const char *from)
{
	struct task_struct *task;
	struct pt_regs *regs;
	pfm_context_t *ctx;
	unsigned long psr, dcr, info, flags;
	int i, this_cpu;

	local_irq_save(flags);

	this_cpu = smp_processor_id();
	regs     = task_pt_regs(current);
	info     = PFM_CPUINFO_GET();
	dcr      = ia64_getreg(_IA64_REG_CR_DCR);

	if (info == 0 && ia64_psr(regs)->pp == 0 && (dcr & IA64_DCR_PP) == 0) {
		local_irq_restore(flags);
		return;
	}

	printk("CPU%d from %s() current [%d] iip=0x%lx %s\n", 
		this_cpu, 
		from, 
		task_pid_nr(current),
		regs->cr_iip,
		current->comm);

	task = GET_PMU_OWNER();
	ctx  = GET_PMU_CTX();

	printk("->CPU%d owner [%d] ctx=%p\n", this_cpu, task ? task_pid_nr(task) : -1, ctx);

	psr = pfm_get_psr();

	printk("->CPU%d pmc0=0x%lx psr.pp=%d psr.up=%d dcr.pp=%d syst_info=0x%lx user_psr.up=%d user_psr.pp=%d\n", 
		this_cpu,
		ia64_get_pmc(0),
		psr & IA64_PSR_PP ? 1 : 0,
		psr & IA64_PSR_UP ? 1 : 0,
		dcr & IA64_DCR_PP ? 1 : 0,
		info,
		ia64_psr(regs)->up,
		ia64_psr(regs)->pp);

	ia64_psr(regs)->up = 0;
	ia64_psr(regs)->pp = 0;

	for (i=1; PMC_IS_LAST(i) == 0; i++) {
		if (PMC_IS_IMPL(i) == 0) continue;
		printk("->CPU%d pmc[%d]=0x%lx thread_pmc[%d]=0x%lx\n", this_cpu, i, ia64_get_pmc(i), i, ctx->th_pmcs[i]);
	}

	for (i=1; PMD_IS_LAST(i) == 0; i++) {
		if (PMD_IS_IMPL(i) == 0) continue;
		printk("->CPU%d pmd[%d]=0x%lx thread_pmd[%d]=0x%lx\n", this_cpu, i, ia64_get_pmd(i), i, ctx->th_pmds[i]);
	}

	if (ctx) {
		printk("->CPU%d ctx_state=%d vaddr=%p addr=%p fd=%d ctx_task=[%d] saved_psr_up=0x%lx\n",
				this_cpu,
				ctx->ctx_state,
				ctx->ctx_smpl_vaddr,
				ctx->ctx_smpl_hdr,
				ctx->ctx_msgq_head,
				ctx->ctx_msgq_tail,
				ctx->ctx_saved_psr_up);
	}
	local_irq_restore(flags);
}

void
pfm_inherit(struct task_struct *task, struct pt_regs *regs)
{
	struct thread_struct *thread;

	DPRINT(("perfmon: pfm_inherit clearing state for [%d]\n", task_pid_nr(task)));

	thread = &task->thread;

	thread->pfm_context = NULL;

	PFM_SET_WORK_PENDING(task, 0);

}
#else  
asmlinkage long
sys_perfmonctl (int fd, int cmd, void *arg, int count)
{
	return -ENOSYS;
}
#endif 
