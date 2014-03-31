/*
 * SN Platform GRU Driver
 *
 *            GRU DRIVER TABLES, MACROS, externs, etc
 *
 *  Copyright (c) 2008 Silicon Graphics, Inc.  All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#ifndef __GRUTABLES_H__
#define __GRUTABLES_H__


#include <linux/rmap.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/mmu_notifier.h>
#include "gru.h"
#include "grulib.h"
#include "gruhandles.h"

extern struct gru_stats_s gru_stats;
extern struct gru_blade_state *gru_base[];
extern unsigned long gru_start_paddr, gru_end_paddr;
extern void *gru_start_vaddr;
extern unsigned int gru_max_gids;

#define GRU_MAX_BLADES		MAX_NUMNODES
#define GRU_MAX_GRUS		(GRU_MAX_BLADES * GRU_CHIPLETS_PER_BLADE)

#define GRU_DRIVER_ID_STR	"SGI GRU Device Driver"
#define GRU_DRIVER_VERSION_STR	"0.85"

struct gru_stats_s {
	atomic_long_t vdata_alloc;
	atomic_long_t vdata_free;
	atomic_long_t gts_alloc;
	atomic_long_t gts_free;
	atomic_long_t gms_alloc;
	atomic_long_t gms_free;
	atomic_long_t gts_double_allocate;
	atomic_long_t assign_context;
	atomic_long_t assign_context_failed;
	atomic_long_t free_context;
	atomic_long_t load_user_context;
	atomic_long_t load_kernel_context;
	atomic_long_t lock_kernel_context;
	atomic_long_t unlock_kernel_context;
	atomic_long_t steal_user_context;
	atomic_long_t steal_kernel_context;
	atomic_long_t steal_context_failed;
	atomic_long_t nopfn;
	atomic_long_t asid_new;
	atomic_long_t asid_next;
	atomic_long_t asid_wrap;
	atomic_long_t asid_reuse;
	atomic_long_t intr;
	atomic_long_t intr_cbr;
	atomic_long_t intr_tfh;
	atomic_long_t intr_spurious;
	atomic_long_t intr_mm_lock_failed;
	atomic_long_t call_os;
	atomic_long_t call_os_wait_queue;
	atomic_long_t user_flush_tlb;
	atomic_long_t user_unload_context;
	atomic_long_t user_exception;
	atomic_long_t set_context_option;
	atomic_long_t check_context_retarget_intr;
	atomic_long_t check_context_unload;
	atomic_long_t tlb_dropin;
	atomic_long_t tlb_preload_page;
	atomic_long_t tlb_dropin_fail_no_asid;
	atomic_long_t tlb_dropin_fail_upm;
	atomic_long_t tlb_dropin_fail_invalid;
	atomic_long_t tlb_dropin_fail_range_active;
	atomic_long_t tlb_dropin_fail_idle;
	atomic_long_t tlb_dropin_fail_fmm;
	atomic_long_t tlb_dropin_fail_no_exception;
	atomic_long_t tfh_stale_on_fault;
	atomic_long_t mmu_invalidate_range;
	atomic_long_t mmu_invalidate_page;
	atomic_long_t flush_tlb;
	atomic_long_t flush_tlb_gru;
	atomic_long_t flush_tlb_gru_tgh;
	atomic_long_t flush_tlb_gru_zero_asid;

	atomic_long_t copy_gpa;
	atomic_long_t read_gpa;

	atomic_long_t mesq_receive;
	atomic_long_t mesq_receive_none;
	atomic_long_t mesq_send;
	atomic_long_t mesq_send_failed;
	atomic_long_t mesq_noop;
	atomic_long_t mesq_send_unexpected_error;
	atomic_long_t mesq_send_lb_overflow;
	atomic_long_t mesq_send_qlimit_reached;
	atomic_long_t mesq_send_amo_nacked;
	atomic_long_t mesq_send_put_nacked;
	atomic_long_t mesq_page_overflow;
	atomic_long_t mesq_qf_locked;
	atomic_long_t mesq_qf_noop_not_full;
	atomic_long_t mesq_qf_switch_head_failed;
	atomic_long_t mesq_qf_unexpected_error;
	atomic_long_t mesq_noop_unexpected_error;
	atomic_long_t mesq_noop_lb_overflow;
	atomic_long_t mesq_noop_qlimit_reached;
	atomic_long_t mesq_noop_amo_nacked;
	atomic_long_t mesq_noop_put_nacked;
	atomic_long_t mesq_noop_page_overflow;

};

enum mcs_op {cchop_allocate, cchop_start, cchop_interrupt, cchop_interrupt_sync,
	cchop_deallocate, tfhop_write_only, tfhop_write_restart,
	tghop_invalidate, mcsop_last};

struct mcs_op_statistic {
	atomic_long_t	count;
	atomic_long_t	total;
	unsigned long	max;
};

extern struct mcs_op_statistic mcs_op_statistics[mcsop_last];

#define OPT_DPRINT		1
#define OPT_STATS		2


#define IRQ_GRU			110	

#define GRU_ASSIGN_DELAY	((HZ * 20) / 1000)

#define GRU_STEAL_DELAY		((HZ * 200) / 1000)

#define STAT(id)	do {						\
				if (gru_options & OPT_STATS)		\
					atomic_long_inc(&gru_stats.id);	\
			} while (0)

#ifdef CONFIG_SGI_GRU_DEBUG
#define gru_dbg(dev, fmt, x...)						\
	do {								\
		if (gru_options & OPT_DPRINT)				\
			printk(KERN_DEBUG "GRU:%d %s: " fmt, smp_processor_id(), __func__, x);\
	} while (0)
#else
#define gru_dbg(x...)
#endif

#define MAX_ASID	0xfffff0
#define MIN_ASID	8
#define ASID_INC	8	

#define VADDR_HI_BIT		64
#define GRUREGION(addr)		((addr) >> (VADDR_HI_BIT - 3) & 3)
#define GRUASID(asid, addr)	((asid) + GRUREGION(addr))


struct gru_state;

struct gru_mm_tracker {				
	unsigned int		mt_asid_gen:24;	
	unsigned int		mt_asid:24;	
	unsigned short		mt_ctxbitmap:16;
} __attribute__ ((packed));

struct gru_mm_struct {
	struct mmu_notifier	ms_notifier;
	atomic_t		ms_refcnt;
	spinlock_t		ms_asid_lock;	
	atomic_t		ms_range_active;
	char			ms_released;
	wait_queue_head_t	ms_wait_queue;
	DECLARE_BITMAP(ms_asidmap, GRU_MAX_GRUS);
	struct gru_mm_tracker	ms_asids[GRU_MAX_GRUS];
};

struct gru_vma_data {
	spinlock_t		vd_lock;	
	struct list_head	vd_head;	
	long			vd_user_options;
	int			vd_cbr_au_count;
	int			vd_dsr_au_count;
	unsigned char		vd_tlb_preload_count;
};

struct gru_thread_state {
	struct list_head	ts_next;	
	struct mutex		ts_ctxlock;	
	struct mm_struct	*ts_mm;		
	struct vm_area_struct	*ts_vma;	
	struct gru_state	*ts_gru;	
	struct gru_mm_struct	*ts_gms;	
	unsigned char		ts_tlb_preload_count; 
	unsigned long		ts_cbr_map;	
	unsigned long		ts_dsr_map;	
	unsigned long		ts_steal_jiffies;
	long			ts_user_options;
	pid_t			ts_tgid_owner;	
	short			ts_user_blade_id;
	char			ts_user_chiplet_id;
	unsigned short		ts_sizeavail;	
	int			ts_tsid;	
	int			ts_tlb_int_select;
	int			ts_ctxnum;	
	atomic_t		ts_refcnt;	
	unsigned char		ts_dsr_au_count;
	unsigned char		ts_cbr_au_count;
	char			ts_cch_req_slice;
	char			ts_blade;	
	char			ts_force_cch_reload;
	char			ts_cbr_idx[GRU_CBR_AU];
	int			ts_data_valid;	
	struct gru_gseg_statistics ustats;	
	unsigned long		ts_gdata[0];	
};

#define TSID(a, v)		(((a) - (v)->vm_start) / GRU_GSEG_PAGESIZE)
#define UGRUADDR(gts)		((gts)->ts_vma->vm_start +		\
					(gts)->ts_tsid * GRU_GSEG_PAGESIZE)

#define NULLCTX			(-1)	


struct gru_state {
	struct gru_blade_state	*gs_blade;		
	unsigned long		gs_gru_base_paddr;	
	void			*gs_gru_base_vaddr;	
	unsigned short		gs_gid;			
	unsigned short		gs_blade_id;		
	unsigned char		gs_chiplet_id;		
	unsigned char		gs_tgh_local_shift;	
	unsigned char		gs_tgh_first_remote;	
	spinlock_t		gs_asid_lock;		
	spinlock_t		gs_lock;		

	
	unsigned int		gs_asid;		
	unsigned int		gs_asid_limit;		
	unsigned int		gs_asid_gen;		

	
	unsigned long		gs_context_map;		
	unsigned long		gs_cbr_map;		
	unsigned long		gs_dsr_map;		
	unsigned int		gs_reserved_cbrs;	
	unsigned int		gs_reserved_dsr_bytes;	
	unsigned short		gs_active_contexts;	
	struct gru_thread_state	*gs_gts[GRU_NUM_CCH];	
	int			gs_irq[GRU_NUM_TFM];	
};

struct gru_blade_state {
	void			*kernel_cb;		
	void			*kernel_dsr;		
	struct rw_semaphore	bs_kgts_sema;		
	struct gru_thread_state *bs_kgts;		

	
	int			bs_async_dsr_bytes;	
	int			bs_async_cbrs;		
	struct completion	*bs_async_wq;

	
	spinlock_t		bs_lock;		
	int			bs_lru_ctxnum;		
	struct gru_state	*bs_lru_gru;		

	struct gru_state	bs_grus[GRU_CHIPLETS_PER_BLADE];
};

#define get_tfm_for_cpu(g, c)						\
	((struct gru_tlb_fault_map *)get_tfm((g)->gs_gru_base_vaddr, (c)))
#define get_tfh_by_index(g, i)						\
	((struct gru_tlb_fault_handle *)get_tfh((g)->gs_gru_base_vaddr, (i)))
#define get_tgh_by_index(g, i)						\
	((struct gru_tlb_global_handle *)get_tgh((g)->gs_gru_base_vaddr, (i)))
#define get_cbe_by_index(g, i)						\
	((struct gru_control_block_extended *)get_cbe((g)->gs_gru_base_vaddr,\
			(i)))


#define get_gru(b, c)		(&gru_base[b]->bs_grus[c])

#define DSR_BYTES(dsr)		((dsr) * GRU_DSR_AU_BYTES)
#define CBR_BYTES(cbr)		((cbr) * GRU_HANDLE_BYTES * GRU_CBR_AU_SIZE * 2)

#define thread_cbr_number(gts, n) ((gts)->ts_cbr_idx[(n) / GRU_CBR_AU_SIZE] \
				  * GRU_CBR_AU_SIZE + (n) % GRU_CBR_AU_SIZE)

#define GID_TO_GRU(gid)							\
	(gru_base[(gid) / GRU_CHIPLETS_PER_BLADE] ?			\
		(&gru_base[(gid) / GRU_CHIPLETS_PER_BLADE]->		\
			bs_grus[(gid) % GRU_CHIPLETS_PER_BLADE]) :	\
	 NULL)

#define for_each_gru_in_bitmap(gid, map)				\
	for_each_set_bit((gid), (map), GRU_MAX_GRUS)

#define for_each_gru_on_blade(gru, nid, i)				\
	for ((gru) = gru_base[nid]->bs_grus, (i) = 0;			\
			(i) < GRU_CHIPLETS_PER_BLADE;			\
			(i)++, (gru)++)

#define foreach_gid(gid)						\
	for ((gid) = 0; (gid) < gru_max_gids; (gid)++)

#define for_each_gts_on_gru(gts, gru, ctxnum)				\
	for ((ctxnum) = 0; (ctxnum) < GRU_NUM_CCH; (ctxnum)++)		\
		if (((gts) = (gru)->gs_gts[ctxnum]))

#define for_each_cbr_in_tfm(i, map)					\
	for_each_set_bit((i), (map), GRU_NUM_CBE)

#define for_each_cbr_in_allocation_map(i, map, k)			\
	for_each_set_bit((k), (map), GRU_CBR_AU)			\
		for ((i) = (k)*GRU_CBR_AU_SIZE;				\
				(i) < ((k) + 1) * GRU_CBR_AU_SIZE; (i)++)

#define for_each_dsr_in_allocation_map(i, map, k)			\
	for_each_set_bit((k), (const unsigned long *)(map), GRU_DSR_AU)	\
		for ((i) = (k) * GRU_DSR_AU_CL;				\
				(i) < ((k) + 1) * GRU_DSR_AU_CL; (i)++)

#define gseg_physical_address(gru, ctxnum)				\
		((gru)->gs_gru_base_paddr + ctxnum * GRU_GSEG_STRIDE)
#define gseg_virtual_address(gru, ctxnum)				\
		((gru)->gs_gru_base_vaddr + ctxnum * GRU_GSEG_STRIDE)



static inline int __trylock_handle(void *h)
{
	return !test_and_set_bit(1, h);
}

static inline void __lock_handle(void *h)
{
	while (test_and_set_bit(1, h))
		cpu_relax();
}

static inline void __unlock_handle(void *h)
{
	clear_bit(1, h);
}

static inline int trylock_cch_handle(struct gru_context_configuration_handle *cch)
{
	return __trylock_handle(cch);
}

static inline void lock_cch_handle(struct gru_context_configuration_handle *cch)
{
	__lock_handle(cch);
}

static inline void unlock_cch_handle(struct gru_context_configuration_handle
				     *cch)
{
	__unlock_handle(cch);
}

static inline void lock_tgh_handle(struct gru_tlb_global_handle *tgh)
{
	__lock_handle(tgh);
}

static inline void unlock_tgh_handle(struct gru_tlb_global_handle *tgh)
{
	__unlock_handle(tgh);
}

static inline int is_kernel_context(struct gru_thread_state *gts)
{
	return !gts->ts_mm;
}

#define UV_MAX_INT_CORES		8
#define uv_cpu_socket_number(p)		((cpu_physical_id(p) >> 5) & 1)
#define uv_cpu_ht_number(p)		(cpu_physical_id(p) & 1)
#define uv_cpu_core_number(p)		(((cpu_physical_id(p) >> 2) & 4) |	\
					((cpu_physical_id(p) >> 1) & 3))
struct gru_unload_context_req;

extern const struct vm_operations_struct gru_vm_ops;
extern struct device *grudev;

extern struct gru_vma_data *gru_alloc_vma_data(struct vm_area_struct *vma,
				int tsid);
extern struct gru_thread_state *gru_find_thread_state(struct vm_area_struct
				*vma, int tsid);
extern struct gru_thread_state *gru_alloc_thread_state(struct vm_area_struct
				*vma, int tsid);
extern struct gru_state *gru_assign_gru_context(struct gru_thread_state *gts);
extern void gru_load_context(struct gru_thread_state *gts);
extern void gru_steal_context(struct gru_thread_state *gts);
extern void gru_unload_context(struct gru_thread_state *gts, int savestate);
extern int gru_update_cch(struct gru_thread_state *gts);
extern void gts_drop(struct gru_thread_state *gts);
extern void gru_tgh_flush_init(struct gru_state *gru);
extern int gru_kservices_init(void);
extern void gru_kservices_exit(void);
extern irqreturn_t gru0_intr(int irq, void *dev_id);
extern irqreturn_t gru1_intr(int irq, void *dev_id);
extern irqreturn_t gru_intr_mblade(int irq, void *dev_id);
extern int gru_dump_chiplet_request(unsigned long arg);
extern long gru_get_gseg_statistics(unsigned long arg);
extern int gru_handle_user_call_os(unsigned long address);
extern int gru_user_flush_tlb(unsigned long arg);
extern int gru_user_unload_context(unsigned long arg);
extern int gru_get_exception_detail(unsigned long arg);
extern int gru_set_context_option(unsigned long address);
extern void gru_check_context_placement(struct gru_thread_state *gts);
extern int gru_cpu_fault_map_id(void);
extern struct vm_area_struct *gru_find_vma(unsigned long vaddr);
extern void gru_flush_all_tlb(struct gru_state *gru);
extern int gru_proc_init(void);
extern void gru_proc_exit(void);

extern struct gru_thread_state *gru_alloc_gts(struct vm_area_struct *vma,
		int cbr_au_count, int dsr_au_count,
		unsigned char tlb_preload_count, int options, int tsid);
extern unsigned long gru_reserve_cb_resources(struct gru_state *gru,
		int cbr_au_count, char *cbmap);
extern unsigned long gru_reserve_ds_resources(struct gru_state *gru,
		int dsr_au_count, char *dsmap);
extern int gru_fault(struct vm_area_struct *, struct vm_fault *vmf);
extern struct gru_mm_struct *gru_register_mmu_notifier(void);
extern void gru_drop_mmu_notifier(struct gru_mm_struct *gms);

extern int gru_ktest(unsigned long arg);
extern void gru_flush_tlb_range(struct gru_mm_struct *gms, unsigned long start,
					unsigned long len);

extern unsigned long gru_options;

#endif 
