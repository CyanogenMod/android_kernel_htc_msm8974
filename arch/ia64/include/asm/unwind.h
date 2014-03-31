#ifndef _ASM_IA64_UNWIND_H
#define _ASM_IA64_UNWIND_H

/*
 * Copyright (C) 1999-2000, 2003 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *
 * A simple API for unwinding kernel stacks.  This is used for
 * debugging and error reporting purposes.  The kernel doesn't need
 * full-blown stack unwinding with all the bells and whitles, so there
 * is not much point in implementing the full IA-64 unwind API (though
 * it would of course be possible to implement the kernel API on top
 * of it).
 */

struct task_struct;	
struct switch_stack;	

enum unw_application_register {
	UNW_AR_BSP,
	UNW_AR_BSPSTORE,
	UNW_AR_PFS,
	UNW_AR_RNAT,
	UNW_AR_UNAT,
	UNW_AR_LC,
	UNW_AR_EC,
	UNW_AR_FPSR,
	UNW_AR_RSC,
	UNW_AR_CCV,
	UNW_AR_CSD,
	UNW_AR_SSD
};


struct unw_stack {
	unsigned long limit;
	unsigned long top;
};

#define UNW_FLAG_INTERRUPT_FRAME	(1UL << 0)

struct unw_frame_info {
	struct unw_stack regstk;
	struct unw_stack memstk;
	unsigned int flags;
	short hint;
	short prev_script;

	
	unsigned long bsp;		
	unsigned long sp;		
	unsigned long psp;		
	unsigned long ip;		
	unsigned long pr;		
	unsigned long *cfm_loc;		
	unsigned long pt;		

	struct task_struct *task;
	struct switch_stack *sw;

	
	unsigned long *bsp_loc;		
	unsigned long *bspstore_loc;
	unsigned long *pfs_loc;
	unsigned long *rnat_loc;
	unsigned long *rp_loc;
	unsigned long *pri_unat_loc;
	unsigned long *unat_loc;
	unsigned long *pr_loc;
	unsigned long *lc_loc;
	unsigned long *fpsr_loc;
	struct unw_ireg {
		unsigned long *loc;
		struct unw_ireg_nat {
			unsigned long type : 3;		
			signed long off : 61;		
		} nat;
	} r4, r5, r6, r7;
	unsigned long *b1_loc, *b2_loc, *b3_loc, *b4_loc, *b5_loc;
	struct ia64_fpreg *f2_loc, *f3_loc, *f4_loc, *f5_loc, *fr_loc[16];
};


struct unw_table_entry {
	u64 start_offset;
	u64 end_offset;
	u64 info_offset;
};

extern void unw_init (void);

extern void *unw_add_unwind_table (const char *name, unsigned long segment_base, unsigned long gp,
				   const void *table_start, const void *table_end);

extern void unw_remove_unwind_table (void *handle);

extern void unw_init_from_blocked_task (struct unw_frame_info *info, struct task_struct *t);

extern void unw_init_frame_info (struct unw_frame_info *info, struct task_struct *t,
				 struct switch_stack *sw);

extern void unw_init_running (void (*callback)(struct unw_frame_info *info, void *arg), void *arg);

extern int unw_unwind (struct unw_frame_info *info);

extern int unw_unwind_to_user (struct unw_frame_info *info);

#define unw_is_intr_frame(info)	(((info)->flags & UNW_FLAG_INTERRUPT_FRAME) != 0)

static inline int
unw_get_ip (struct unw_frame_info *info, unsigned long *valp)
{
	*valp = (info)->ip;
	return 0;
}

static inline int
unw_get_sp (struct unw_frame_info *info, unsigned long *valp)
{
	*valp = (info)->sp;
	return 0;
}

static inline int
unw_get_psp (struct unw_frame_info *info, unsigned long *valp)
{
	*valp = (info)->psp;
	return 0;
}

static inline int
unw_get_bsp (struct unw_frame_info *info, unsigned long *valp)
{
	*valp = (info)->bsp;
	return 0;
}

static inline int
unw_get_cfm (struct unw_frame_info *info, unsigned long *valp)
{
	*valp = *(info)->cfm_loc;
	return 0;
}

static inline int
unw_set_cfm (struct unw_frame_info *info, unsigned long val)
{
	*(info)->cfm_loc = val;
	return 0;
}

static inline int
unw_get_rp (struct unw_frame_info *info, unsigned long *val)
{
	if (!info->rp_loc)
		return -1;
	*val = *info->rp_loc;
	return 0;
}

extern int unw_access_gr (struct unw_frame_info *, int, unsigned long *, char *, int);
extern int unw_access_br (struct unw_frame_info *, int, unsigned long *, int);
extern int unw_access_fr (struct unw_frame_info *, int, struct ia64_fpreg *, int);
extern int unw_access_ar (struct unw_frame_info *, int, unsigned long *, int);
extern int unw_access_pr (struct unw_frame_info *, unsigned long *, int);

static inline int
unw_set_gr (struct unw_frame_info *i, int n, unsigned long v, char nat)
{
	return unw_access_gr(i, n, &v, &nat, 1);
}

static inline int
unw_set_br (struct unw_frame_info *i, int n, unsigned long v)
{
	return unw_access_br(i, n, &v, 1);
}

static inline int
unw_set_fr (struct unw_frame_info *i, int n, struct ia64_fpreg v)
{
	return unw_access_fr(i, n, &v, 1);
}

static inline int
unw_set_ar (struct unw_frame_info *i, int n, unsigned long v)
{
	return unw_access_ar(i, n, &v, 1);
}

static inline int
unw_set_pr (struct unw_frame_info *i, unsigned long v)
{
	return unw_access_pr(i, &v, 1);
}

#define unw_get_gr(i,n,v,nat)	unw_access_gr(i,n,v,nat,0)
#define unw_get_br(i,n,v)	unw_access_br(i,n,v,0)
#define unw_get_fr(i,n,v)	unw_access_fr(i,n,v,0)
#define unw_get_ar(i,n,v)	unw_access_ar(i,n,v,0)
#define unw_get_pr(i,v)		unw_access_pr(i,v,0)

#endif 
