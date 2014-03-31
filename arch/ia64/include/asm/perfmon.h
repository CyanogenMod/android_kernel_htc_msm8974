/*
 * Copyright (C) 2001-2003 Hewlett-Packard Co
 *               Stephane Eranian <eranian@hpl.hp.com>
 */

#ifndef _ASM_IA64_PERFMON_H
#define _ASM_IA64_PERFMON_H

#define PFM_WRITE_PMCS		0x01
#define PFM_WRITE_PMDS		0x02
#define PFM_READ_PMDS		0x03
#define PFM_STOP		0x04
#define PFM_START		0x05
#define PFM_ENABLE		0x06 
#define PFM_DISABLE		0x07 
#define PFM_CREATE_CONTEXT	0x08
#define PFM_DESTROY_CONTEXT	0x09 
#define PFM_RESTART		0x0a
#define PFM_PROTECT_CONTEXT	0x0b 
#define PFM_GET_FEATURES	0x0c
#define PFM_DEBUG		0x0d
#define PFM_UNPROTECT_CONTEXT	0x0e 
#define PFM_GET_PMC_RESET_VAL	0x0f
#define PFM_LOAD_CONTEXT	0x10
#define PFM_UNLOAD_CONTEXT	0x11

#define PFM_WRITE_IBRS		0x20
#define PFM_WRITE_DBRS		0x21

#define PFM_FL_NOTIFY_BLOCK    	 0x01	
#define PFM_FL_SYSTEM_WIDE	 0x02	
#define PFM_FL_OVFL_NO_MSG	 0x80   

#define PFM_SETFL_EXCL_IDLE      0x01   

#define PFM_REGFL_OVFL_NOTIFY	0x1	
#define PFM_REGFL_RANDOM	0x2	

#define PFM_REG_RETFL_NOTAVAIL	(1UL<<31) 
#define PFM_REG_RETFL_EINVAL	(1UL<<30) 
#define PFM_REG_RETFL_MASK	(PFM_REG_RETFL_NOTAVAIL|PFM_REG_RETFL_EINVAL)

#define PFM_REG_HAS_ERROR(flag)	(((flag) & PFM_REG_RETFL_MASK) != 0)

typedef unsigned char pfm_uuid_t[16];	

typedef struct {
	pfm_uuid_t     ctx_smpl_buf_id;	 
	unsigned long  ctx_flags;	 
	unsigned short ctx_nextra_sets;	 
	unsigned short ctx_reserved1;	 
	int	       ctx_fd;		 
	void	       *ctx_smpl_vaddr;	 
	unsigned long  ctx_reserved2[11];
} pfarg_context_t;

typedef struct {
	unsigned int	reg_num;	   
	unsigned short	reg_set;	   
	unsigned short	reg_reserved1;	   

	unsigned long	reg_value;	   
	unsigned long	reg_flags;	   

	unsigned long	reg_long_reset;	   
	unsigned long	reg_short_reset;   

	unsigned long	reg_reset_pmds[4]; 
	unsigned long	reg_random_seed;   
	unsigned long	reg_random_mask;   
	unsigned long   reg_last_reset_val;

	unsigned long	reg_smpl_pmds[4];  
	unsigned long	reg_smpl_eventid;  

	unsigned long   reg_reserved2[3];   
} pfarg_reg_t;

typedef struct {
	unsigned int	dbreg_num;		
	unsigned short	dbreg_set;		
	unsigned short	dbreg_reserved1;	
	unsigned long	dbreg_value;		
	unsigned long	dbreg_flags;		
	unsigned long	dbreg_reserved2[1];	
} pfarg_dbreg_t;

typedef struct {
	unsigned int	ft_version;	
	unsigned int	ft_reserved;	
	unsigned long	reserved[4];	
} pfarg_features_t;

typedef struct {
	pid_t		load_pid;	   
	unsigned short	load_set;	   
	unsigned short	load_reserved1;	   
	unsigned long	load_reserved2[3]; 
} pfarg_load_t;

typedef struct {
	int		msg_type;		
	int		msg_ctx_fd;		
	unsigned long	msg_ovfl_pmds[4];	
	unsigned short  msg_active_set;		
	unsigned short  msg_reserved1;		
	unsigned int    msg_reserved2;		
	unsigned long	msg_tstamp;		
} pfm_ovfl_msg_t;

typedef struct {
	int		msg_type;		
	int		msg_ctx_fd;		
	unsigned long	msg_tstamp;		
} pfm_end_msg_t;

typedef struct {
	int		msg_type;		
	int		msg_ctx_fd;		
	unsigned long	msg_tstamp;		
} pfm_gen_msg_t;

#define PFM_MSG_OVFL	1	
#define PFM_MSG_END	2	

typedef union {
	pfm_ovfl_msg_t	pfm_ovfl_msg;
	pfm_end_msg_t	pfm_end_msg;
	pfm_gen_msg_t	pfm_gen_msg;
} pfm_msg_t;

#define PFM_VERSION_MAJ		 2U
#define PFM_VERSION_MIN		 0U
#define PFM_VERSION		 (((PFM_VERSION_MAJ&0xffff)<<16)|(PFM_VERSION_MIN & 0xffff))
#define PFM_VERSION_MAJOR(x)	 (((x)>>16) & 0xffff)
#define PFM_VERSION_MINOR(x)	 ((x) & 0xffff)


#define PMU_FIRST_COUNTER	4	
#define PMU_MAX_PMCS		256	
#define PMU_MAX_PMDS		256	

#ifdef __KERNEL__

extern long perfmonctl(int fd, int cmd, void *arg, int narg);

typedef struct {
	void (*handler)(int irq, void *arg, struct pt_regs *regs);
} pfm_intr_handler_desc_t;

extern void pfm_save_regs (struct task_struct *);
extern void pfm_load_regs (struct task_struct *);

extern void pfm_exit_thread(struct task_struct *);
extern int  pfm_use_debug_registers(struct task_struct *);
extern int  pfm_release_debug_registers(struct task_struct *);
extern void pfm_syst_wide_update_task(struct task_struct *, unsigned long info, int is_ctxswin);
extern void pfm_inherit(struct task_struct *task, struct pt_regs *regs);
extern void pfm_init_percpu(void);
extern void pfm_handle_work(void);
extern int  pfm_install_alt_pmu_interrupt(pfm_intr_handler_desc_t *h);
extern int  pfm_remove_alt_pmu_interrupt(pfm_intr_handler_desc_t *h);



#define PFM_PMD_SHORT_RESET	0
#define PFM_PMD_LONG_RESET	1

typedef union {
	unsigned int val;
	struct {
		unsigned int notify_user:1;	
		unsigned int reset_ovfl_pmds:1;	
		unsigned int block_task:1;	
		unsigned int mask_monitoring:1; 
		unsigned int reserved:28;	
	} bits;
} pfm_ovfl_ctrl_t;

typedef struct {
	unsigned char	ovfl_pmd;			
	unsigned char   ovfl_notify;			
	unsigned short  active_set;			
	pfm_ovfl_ctrl_t ovfl_ctrl;			

	unsigned long   pmd_last_reset;			
	unsigned long	smpl_pmds[4];			
	unsigned long   smpl_pmds_values[PMU_MAX_PMDS]; 
	unsigned long   pmd_value;			
	unsigned long	pmd_eventid;			
} pfm_ovfl_arg_t;


typedef struct {
	char		*fmt_name;
	pfm_uuid_t	fmt_uuid;
	size_t		fmt_arg_size;
	unsigned long	fmt_flags;

	int		(*fmt_validate)(struct task_struct *task, unsigned int flags, int cpu, void *arg);
	int		(*fmt_getsize)(struct task_struct *task, unsigned int flags, int cpu, void *arg, unsigned long *size);
	int 		(*fmt_init)(struct task_struct *task, void *buf, unsigned int flags, int cpu, void *arg);
	int		(*fmt_handler)(struct task_struct *task, void *buf, pfm_ovfl_arg_t *arg, struct pt_regs *regs, unsigned long stamp);
	int		(*fmt_restart)(struct task_struct *task, pfm_ovfl_ctrl_t *ctrl, void *buf, struct pt_regs *regs);
	int		(*fmt_restart_active)(struct task_struct *task, pfm_ovfl_ctrl_t *ctrl, void *buf, struct pt_regs *regs);
	int		(*fmt_exit)(struct task_struct *task, void *buf, struct pt_regs *regs);

	struct list_head fmt_list;
} pfm_buffer_fmt_t;

extern int pfm_register_buffer_fmt(pfm_buffer_fmt_t *fmt);
extern int pfm_unregister_buffer_fmt(pfm_uuid_t uuid);

extern int pfm_mod_read_pmds(struct task_struct *, void *req, unsigned int nreq, struct pt_regs *regs);
extern int pfm_mod_write_pmcs(struct task_struct *, void *req, unsigned int nreq, struct pt_regs *regs);
extern int pfm_mod_write_ibrs(struct task_struct *task, void *req, unsigned int nreq, struct pt_regs *regs);
extern int pfm_mod_write_dbrs(struct task_struct *task, void *req, unsigned int nreq, struct pt_regs *regs);

#define PFM_CPUINFO_SYST_WIDE	0x1	
#define PFM_CPUINFO_DCR_PP	0x2	
#define PFM_CPUINFO_EXCL_IDLE	0x4	

typedef struct {
	int	debug;		
	int	debug_ovfl;	
	int	fastctxsw;	
	int	expert_mode;	
} pfm_sysctl_t;
extern pfm_sysctl_t pfm_sysctl;


#endif 

#endif 
