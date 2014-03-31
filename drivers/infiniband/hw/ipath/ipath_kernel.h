#ifndef _IPATH_KERNEL_H
#define _IPATH_KERNEL_H
/*
 * Copyright (c) 2006, 2007, 2008 QLogic Corporation. All rights reserved.
 * Copyright (c) 2003, 2004, 2005, 2006 PathScale, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/scatterlist.h>
#include <asm/io.h>
#include <rdma/ib_verbs.h>

#include "ipath_common.h"
#include "ipath_debug.h"
#include "ipath_registers.h"

#define IPATH_CHIP_VERS_MAJ 2U

#define IPATH_CHIP_VERS_MIN 0U

extern struct infinipath_stats ipath_stats;

#define IPATH_CHIP_SWVERSION IPATH_CHIP_VERS_MAJ
#define IPATH_TRAFFIC_ACTIVE_THRESHOLD (2000)
#define IPATH_EEP_LOG_CNT (4)
struct ipath_eep_log_mask {
	u64 errs_to_log;
	u64 hwerrs_to_log;
};

struct ipath_portdata {
	void **port_rcvegrbuf;
	dma_addr_t *port_rcvegrbuf_phys;
	
	void *port_rcvhdrq;
	
	void *port_rcvhdrtail_kvaddr;
	void *port_tid_pg_list;
	
	wait_queue_head_t port_wait;
	dma_addr_t port_rcvegr_phys;
	
	dma_addr_t port_rcvhdrq_phys;
	dma_addr_t port_rcvhdrqtailaddr_phys;
	int port_cnt;
	
	unsigned port_port;
	
	u16 port_subport_cnt;
	
	u16 port_subport_id;
	
	u32 port_piocnt;
	
	u32 port_pio_base;
	
	u32 port_piobufs;
	
	u32 port_rcvegrbuf_chunks;
	
	u32 port_rcvegrbufs_perchunk;
	
	size_t port_rcvegrbuf_size;
	
	size_t port_rcvhdrq_size;
	
	u32 port_tidcursor;
	
	unsigned long port_flag;
	
	unsigned long int_flag;
	
	u32 port_rcvwait_to;
	
	u32 port_piowait_to;
	
	u32 port_rcvnowait;
	
	u32 port_pionowait;
	
	u32 port_hdrqfull;
	u32 port_lastrcvhdrqtail;
	
	u32 port_hdrqfull_poll;
	
	u32 port_urgent;
	
	u32 port_urgent_poll;
	
	struct pid *port_pid;
	struct pid *port_subpid[INFINIPATH_MAX_SUBPORT];
	
	char port_comm[16];
	
	u16 port_pkeys[4];
	
	struct ipath_devdata *port_dd;
	
	void *subport_uregbase;
	
	void *subport_rcvegrbuf;
	
	void *subport_rcvhdr_base;
	
	u32 userversion;
	
	u32 active_slaves;
	
	u16 poll_type;
	
	u32 port_head;
	
	u32 port_seq_cnt;
};

struct sk_buff;
struct ipath_sge_state;
struct ipath_verbs_txreq;

struct _ipath_layer {
	void *l_arg;
};

struct ipath_skbinfo {
	struct sk_buff *skb;
	dma_addr_t phys;
};

struct ipath_sdma_txreq {
	int                 flags;
	int                 sg_count;
	union {
		struct scatterlist *sg;
		void *map_addr;
	};
	void              (*callback)(void *, int);
	void               *callback_cookie;
	int                 callback_status;
	u16                 start_idx;  
	u16                 next_descq_idx;  
	struct list_head    list;       
};

struct ipath_sdma_desc {
	__le64 qw[2];
};

#define IPATH_SDMA_TXREQ_F_USELARGEBUF  0x1
#define IPATH_SDMA_TXREQ_F_HEADTOHOST   0x2
#define IPATH_SDMA_TXREQ_F_INTREQ       0x4
#define IPATH_SDMA_TXREQ_F_FREEBUF      0x8
#define IPATH_SDMA_TXREQ_F_FREEDESC     0x10
#define IPATH_SDMA_TXREQ_F_VL15         0x20

#define IPATH_SDMA_TXREQ_S_OK        0
#define IPATH_SDMA_TXREQ_S_SENDERROR 1
#define IPATH_SDMA_TXREQ_S_ABORTED   2
#define IPATH_SDMA_TXREQ_S_SHUTDOWN  3

#define IPATH_SDMA_STATUS_SCORE_BOARD_DRAIN_IN_PROG	(1ull << 63)
#define IPATH_SDMA_STATUS_ABORT_IN_PROG			(1ull << 62)
#define IPATH_SDMA_STATUS_INTERNAL_SDMA_ENABLE		(1ull << 61)
#define IPATH_SDMA_STATUS_SCB_EMPTY			(1ull << 30)

#define IPATH_SMALLBUF_DWORDS (dd->ipath_piosize2k >> 2)

#define IPATH_IB_CFG_LIDLMC 0 
#define IPATH_IB_CFG_HRTBT 1 
#define IPATH_IB_HRTBT_ON 3 
#define IPATH_IB_HRTBT_OFF 0 
#define IPATH_IB_CFG_LWID_ENB 2 
#define IPATH_IB_CFG_LWID 3 
#define IPATH_IB_CFG_SPD_ENB 4 
#define IPATH_IB_CFG_SPD 5 
#define IPATH_IB_CFG_RXPOL_ENB 6 
#define IPATH_IB_CFG_LREV_ENB 7 
#define IPATH_IB_CFG_LINKLATENCY 8 


struct ipath_devdata {
	struct list_head ipath_list;

	struct ipath_kregs const *ipath_kregs;
	struct ipath_cregs const *ipath_cregs;

	
	u64 __iomem *ipath_kregbase;
	
	u64 __iomem *ipath_kregend;
	
	unsigned long ipath_physaddr;
	
	u64 *ipath_kregalloc;
	
	struct ipath_portdata **ipath_pd;
	
	struct ipath_skbinfo *ipath_port0_skbinfo;
	
	void __iomem *ipath_pio2kbase;
	
	void __iomem *ipath_pio4kbase;
	/*
	 * points to area where PIOavail registers will be DMA'ed.
	 * Has to be on a page of it's own, because the page will be
	 * mapped into user program space.  This copy is *ONLY* ever
	 * written by DMA, not by the driver!  Need a copy per device
	 * when we get to multiple devices
	 */
	volatile __le64 *ipath_pioavailregs_dma;
	
	dma_addr_t ipath_pioavailregs_phys;
	struct _ipath_layer ipath_layer;
	
	int (*ipath_f_intrsetup)(struct ipath_devdata *);
	
	int (*ipath_f_intr_fallback)(struct ipath_devdata *);
	
	int (*ipath_f_bus)(struct ipath_devdata *, struct pci_dev *);
	
	int (*ipath_f_reset)(struct ipath_devdata *);
	int (*ipath_f_get_boardname)(struct ipath_devdata *, char *,
				     size_t);
	void (*ipath_f_init_hwerrors)(struct ipath_devdata *);
	void (*ipath_f_handle_hwerrors)(struct ipath_devdata *, char *,
					size_t);
	void (*ipath_f_quiet_serdes)(struct ipath_devdata *);
	int (*ipath_f_bringup_serdes)(struct ipath_devdata *);
	int (*ipath_f_early_init)(struct ipath_devdata *);
	void (*ipath_f_clear_tids)(struct ipath_devdata *, unsigned);
	void (*ipath_f_put_tid)(struct ipath_devdata *, u64 __iomem*,
				u32, unsigned long);
	void (*ipath_f_tidtemplate)(struct ipath_devdata *);
	void (*ipath_f_cleanup)(struct ipath_devdata *);
	void (*ipath_f_setextled)(struct ipath_devdata *, u64, u64);
	
	int (*ipath_f_get_base_info)(struct ipath_portdata *, void *);
	
	void (*ipath_f_free_irq)(struct ipath_devdata *);
	struct ipath_message_header *(*ipath_f_get_msgheader)
					(struct ipath_devdata *, __le32 *);
	void (*ipath_f_config_ports)(struct ipath_devdata *, ushort);
	int (*ipath_f_get_ib_cfg)(struct ipath_devdata *, int);
	int (*ipath_f_set_ib_cfg)(struct ipath_devdata *, int, u32);
	void (*ipath_f_config_jint)(struct ipath_devdata *, u16 , u16);
	void (*ipath_f_read_counters)(struct ipath_devdata *,
					struct infinipath_counters *);
	void (*ipath_f_xgxs_reset)(struct ipath_devdata *);
	
	int (*ipath_f_ib_updown)(struct ipath_devdata *, int, u64);

	unsigned ipath_lastegr_idx;
	struct ipath_ibdev *verbs_dev;
	struct timer_list verbs_timer;
	
	u64 ipath_sword;
	
	u64 ipath_rword;
	
	u64 ipath_spkts;
	
	u64 ipath_rpkts;
	
	u64 _ipath_status;
	
	__be64 ipath_guid;
	ipath_err_t ipath_lasterror;
	ipath_err_t ipath_lasthwerror;
	
	ipath_err_t ipath_maskederrs;
	u64 ipath_lastlinkrecov; 
	u64 ibdeltainprog;
	u64 ibsymdelta;
	u64 ibsymsnap;
	u64 iblnkerrdelta;
	u64 iblnkerrsnap;

	
	unsigned long ipath_unmasktime;
	
	u64 ipath_last_tidfull;
	
	u64 ipath_lastport0rcv_cnt;
	
	u64 ipath_tidtemplate;
	
	u64 ipath_tidinvalid;
	
	u64 ipath_rhdrhead_intr_off;

	
	u32 ipath_kregsize;
	
	u32 ipath_pioavregs;
	
	u32 ipath_flags;
	
	u32 ipath_state_wanted;
	u32 ipath_lastport_piobuf;
	
	u32 ipath_stats_timer_active;
	
	u32 ipath_int_counter;
	
	u32 ipath_lastsword;
	
	u32 ipath_lastrword;
	
	u32 ipath_lastspkts;
	
	u32 ipath_lastrpkts;
	
	u32 ipath_pbufsport;
	
	u32 ipath_ports_extrabuf;
	u32 ipath_pioupd_thresh; 
	u32 ipath_cfgports;
	
	u32 ipath_p0_hdrqfull;
	
	u32 ipath_p0_rcvegrcnt;

	u32 ipath_lastpioindex;
	u32 ipath_lastpioindexl;
	
	u32 ipath_freezelen;
	u32 ipath_consec_nopiobuf;
	u32 ipath_upd_pio_shadow;
	
	u32 ipath_pcibar0;
	
	u32 ipath_pcibar1;
	u32 ipath_x1_fix_tries;
	u32 ipath_autoneg_tries;
	u32 serdes_first_init_done;

	struct ipath_relock {
		atomic_t ipath_relock_timer_active;
		struct timer_list ipath_relock_timer;
		unsigned int ipath_relock_interval; 
	} ipath_relock_singleton;

	
	int ipath_irq;
	
	u16 ipath_vendorid;
	
	u16 ipath_deviceid;
	
	u8 ipath_ht_slave_off;
	
	unsigned long ipath_wc_cookie;
	unsigned long ipath_wc_base;
	unsigned long ipath_wc_len;
	
	atomic_t ipath_pkeyrefs[4];
	
	struct page **ipath_pageshadow;
	
	dma_addr_t *ipath_physshadow;
	u64 __iomem *ipath_egrtidbase;
	
	spinlock_t ipath_kernel_tid_lock;
	spinlock_t ipath_user_tid_lock;
	spinlock_t ipath_sendctrl_lock;
	
	spinlock_t ipath_uctxt_lock;

	u64 *ipath_statusp;
	
	char *ipath_freezemsg;
	
	struct pci_dev *pcidev;
	struct cdev *user_cdev;
	struct cdev *diag_cdev;
	struct device *user_dev;
	struct device *diag_dev;
	
	struct timer_list ipath_stats_timer;
	
	struct timer_list ipath_intrchk_timer;
	void *ipath_dummy_hdrq;	
	dma_addr_t ipath_dummy_hdrq_phys;

	
	spinlock_t            ipath_sdma_lock;
	unsigned long         ipath_sdma_status;
	unsigned long         ipath_sdma_abort_jiffies;
	unsigned long         ipath_sdma_abort_intr_timeout;
	unsigned long         ipath_sdma_buf_jiffies;
	struct ipath_sdma_desc *ipath_sdma_descq;
	u64		      ipath_sdma_descq_added;
	u64		      ipath_sdma_descq_removed;
	int		      ipath_sdma_desc_nreserved;
	u16                   ipath_sdma_descq_cnt;
	u16                   ipath_sdma_descq_tail;
	u16                   ipath_sdma_descq_head;
	u16                   ipath_sdma_next_intr;
	u16                   ipath_sdma_reset_wait;
	u8                    ipath_sdma_generation;
	struct tasklet_struct ipath_sdma_abort_task;
	struct tasklet_struct ipath_sdma_notify_task;
	struct list_head      ipath_sdma_activelist;
	struct list_head      ipath_sdma_notifylist;
	atomic_t              ipath_sdma_vl15_count;
	struct timer_list     ipath_sdma_vl15_timer;

	dma_addr_t       ipath_sdma_descq_phys;
	volatile __le64 *ipath_sdma_head_dma;
	dma_addr_t       ipath_sdma_head_phys;

	unsigned long ipath_ureg_align; 

	struct delayed_work ipath_autoneg_work;
	wait_queue_head_t ipath_autoneg_wait;

	
	unsigned          ipath_hol_state;
	unsigned          ipath_hol_next;
	struct timer_list ipath_hol_timer;


	

	unsigned long ipath_pioavailshadow[8];
	
	unsigned long ipath_pioavailkernel[8];
	
	u64 ipath_gpio_out;
	
	u64 ipath_gpio_mask;
	
	u64 ipath_extctrl;
	
	u64 ipath_revision;
	u64 ipath_ibcctrl;
	u64 ipath_lastibcstat;
	
	ipath_err_t ipath_hwerrmask;
	ipath_err_t ipath_errormask; 
	
	u64 ipath_intconfig;
	
	u64 ipath_piobufbase;
	
	u64 ipath_ibcddrctrl;

	

	u32 ipath_nguid;
	
	unsigned long ipath_rcvctrl;
	
	unsigned long ipath_sendctrl;
	
	unsigned long ipath_lastcancel;
	
	unsigned long ipath_spectriggerhit;

	
	u32 ipath_rcvhdrcnt;
	
	u32 ipath_rcvhdrsize;
	
	u32 ipath_rcvhdrentsize;
	
	u32 ipath_hdrqlast;
	
	u32 ipath_portcnt;
	
	u32 ipath_palign;
	
	u32 ipath_piobcnt2k;
	
	u32 ipath_piosize2k;
	
	u32 ipath_piobcnt4k;
	
	u32 ipath_piosize4k;
	u32 ipath_pioreserved; 
	
	u32 ipath_rcvegrbase;
	
	u32 ipath_rcvegrcnt;
	
	u32 ipath_rcvtidbase;
	
	u32 ipath_rcvtidcnt;
	
	u32 ipath_sregbase;
	
	u32 ipath_uregbase;
	
	u32 ipath_cregbase;
	
	u32 ipath_control;
	
	u32 ipath_pcirev;

	
	u32 ipath_4kalign;
	
	u32 ipath_ibmtu;
	u32 ipath_ibmaxlen;
	u32 ipath_init_ibmaxlen;
	
	u32 ipath_rcvegrbufsize;
	
	u32 ipath_lbus_width;
	
	u32 ipath_lbus_speed;
	u32 ipath_ibpollcnt;
	
	u32 ipath_msi_lo;
	
	u32 ipath_msi_hi;
	
	u16 ipath_msi_data;
	
	u16 ipath_mlid;
	
	u16 ipath_lid;
	
	u16 ipath_pkeys[4];
	u8 ipath_serial[16];
	
	u8 ipath_boardversion[96];
	u8 ipath_lbus_info[32]; 
	
	u8 ipath_majrev;
	
	u8 ipath_minrev;
	
	u8 ipath_boardrev;
	
	u8 ipath_pci_cacheline;
	
	u8 ipath_lmc;
	
	u8 ipath_link_width_supported;
	
	u8 ipath_link_speed_supported;
	u8 ipath_link_width_enabled;
	u8 ipath_link_speed_enabled;
	u8 ipath_link_width_active;
	u8 ipath_link_speed_active;
	
	u8 ipath_rx_pol_inv;

	u8 ipath_r_portenable_shift;
	u8 ipath_r_intravail_shift;
	u8 ipath_r_tailupd_shift;
	u8 ipath_r_portcfg_shift;

	
	int ipath_unit;

	
	u32 ipath_lli_counter;
	
	u32 ipath_lli_errors;
	u32 ipath_rxfc_unsupvl_errs;
	u32 ipath_overrun_thresh_errs;
	u32 ipath_lli_errs;

	u64 ipath_i_bitsextant;
	ipath_err_t ipath_e_bitsextant;
	ipath_err_t ipath_hwe_bitsextant;

	u64 ipath_i_rcvavail_mask;
	u64 ipath_i_rcvurg_mask;
	u16 ipath_i_rcvurg_shift;
	u16 ipath_i_rcvavail_shift;

	u8 ipath_gpio_sda_num;
	u8 ipath_gpio_scl_num;
	u8 ipath_i2c_chain_type;
	u64 ipath_gpio_sda;
	u64 ipath_gpio_scl;

	
	spinlock_t ipath_gpio_lock;

	u8 ibcs_ls_shift;
	u8 ibcs_lts_mask;
	u32 ibcs_mask;
	u32 ib_init;
	u32 ib_arm;
	u32 ib_active;

	u16 ipath_rhf_offset; 

	u8 ibcc_lic_mask; 
	u8 ibcc_lc_shift; 
	u8 ibcc_mpl_shift; 

	u8 delay_mult;

	
	u8 ipath_led_override;  
	u16 ipath_led_override_timeoff; 
	u8 ipath_led_override_vals[2]; 
	u8 ipath_led_override_phase; 
	atomic_t ipath_led_override_timer_active;
	
	struct timer_list ipath_led_override_timer;

	
	
	spinlock_t ipath_eep_st_lock;
	
	struct mutex ipath_eep_lock;
	
	uint64_t ipath_traffic_wds;
	
	atomic_t ipath_active_time;
	
	uint8_t ipath_eep_st_errs[IPATH_EEP_LOG_CNT];
	uint8_t ipath_eep_st_new_errs[IPATH_EEP_LOG_CNT];
	uint16_t ipath_eep_hrs;
	struct ipath_eep_log_mask ipath_eep_st_masks[IPATH_EEP_LOG_CNT];

	
	u16 ipath_jint_idle_ticks;	
	u16 ipath_jint_max_packets;	

	spinlock_t ipath_sdepb_lock;
	u8 ipath_presets_needed; 
};

#define IPATH_HOL_UP       0
#define IPATH_HOL_DOWN     1
#define IPATH_HOL_DOWNSTOP 0
#define IPATH_HOL_DOWNCONT 1

#define IPATH_SDMA_ABORTING  0
#define IPATH_SDMA_DISARMED  1
#define IPATH_SDMA_DISABLED  2
#define IPATH_SDMA_LAYERBUF  3
#define IPATH_SDMA_RUNNING  30
#define IPATH_SDMA_SHUTDOWN 31

#define IPATH_SDMA_ABORT_NONE 0
#define IPATH_SDMA_ABORT_ABORTING (1UL << IPATH_SDMA_ABORTING)
#define IPATH_SDMA_ABORT_DISARMED ((1UL << IPATH_SDMA_ABORTING) | \
	(1UL << IPATH_SDMA_DISARMED))
#define IPATH_SDMA_ABORT_DISABLED ((1UL << IPATH_SDMA_ABORTING) | \
	(1UL << IPATH_SDMA_DISABLED))
#define IPATH_SDMA_ABORT_ABORTED ((1UL << IPATH_SDMA_ABORTING) | \
	(1UL << IPATH_SDMA_DISARMED) | (1UL << IPATH_SDMA_DISABLED))
#define IPATH_SDMA_ABORT_MASK ((1UL<<IPATH_SDMA_ABORTING) | \
	(1UL << IPATH_SDMA_DISARMED) | (1UL << IPATH_SDMA_DISABLED))

#define IPATH_SDMA_BUF_NONE 0
#define IPATH_SDMA_BUF_MASK (1UL<<IPATH_SDMA_LAYERBUF)

struct ipath_filedata {
	struct ipath_portdata *pd;
	unsigned subport;
	unsigned tidcursor;
	struct ipath_user_sdma_queue *pq;
};
extern struct list_head ipath_dev_list;
extern spinlock_t ipath_devs_lock;
extern struct ipath_devdata *ipath_lookup(int unit);

int ipath_init_chip(struct ipath_devdata *, int);
int ipath_enable_wc(struct ipath_devdata *dd);
void ipath_disable_wc(struct ipath_devdata *dd);
int ipath_count_units(int *npresentp, int *nupp, int *maxportsp);
void ipath_shutdown_device(struct ipath_devdata *);
void ipath_clear_freeze(struct ipath_devdata *);

struct file_operations;
int ipath_cdev_init(int minor, char *name, const struct file_operations *fops,
		    struct cdev **cdevp, struct device **devp);
void ipath_cdev_cleanup(struct cdev **cdevp,
			struct device **devp);

int ipath_diag_add(struct ipath_devdata *);
void ipath_diag_remove(struct ipath_devdata *);

extern wait_queue_head_t ipath_state_wait;

int ipath_user_add(struct ipath_devdata *dd);
void ipath_user_remove(struct ipath_devdata *dd);

struct sk_buff *ipath_alloc_skb(struct ipath_devdata *dd, gfp_t);

extern int ipath_diag_inuse;

irqreturn_t ipath_intr(int irq, void *devid);
int ipath_decode_err(struct ipath_devdata *dd, char *buf, size_t blen,
		     ipath_err_t err);
#if __IPATH_INFO || __IPATH_DBG
extern const char *ipath_ibcstatus_str[];
#endif

void ipath_chip_cleanup(struct ipath_devdata *);
void ipath_chip_done(void);

int ipath_unordered_wc(void);

void ipath_disarm_piobufs(struct ipath_devdata *, unsigned first,
			  unsigned cnt);
void ipath_cancel_sends(struct ipath_devdata *, int);

int ipath_create_rcvhdrq(struct ipath_devdata *, struct ipath_portdata *);
void ipath_free_pddata(struct ipath_devdata *, struct ipath_portdata *);

int ipath_parse_ushort(const char *str, unsigned short *valp);

void ipath_kreceive(struct ipath_portdata *);
int ipath_setrcvhdrsize(struct ipath_devdata *, unsigned);
int ipath_reset_device(int);
void ipath_get_faststats(unsigned long);
int ipath_wait_linkstate(struct ipath_devdata *, u32, int);
int ipath_set_linkstate(struct ipath_devdata *, u8);
int ipath_set_mtu(struct ipath_devdata *, u16);
int ipath_set_lid(struct ipath_devdata *, u32, u8);
int ipath_set_rx_pol_inv(struct ipath_devdata *dd, u8 new_pol_inv);
void ipath_enable_armlaunch(struct ipath_devdata *);
void ipath_disable_armlaunch(struct ipath_devdata *);
void ipath_hol_down(struct ipath_devdata *);
void ipath_hol_up(struct ipath_devdata *);
void ipath_hol_event(unsigned long);
void ipath_toggle_rclkrls(struct ipath_devdata *);
void ipath_sd7220_clr_ibpar(struct ipath_devdata *);
void ipath_set_relock_poll(struct ipath_devdata *, int);
void ipath_shutdown_relock_poll(struct ipath_devdata *);

#define port_fp(fp) ((struct ipath_filedata *)(fp)->private_data)->pd
#define subport_fp(fp) \
	((struct ipath_filedata *)(fp)->private_data)->subport
#define tidcursor_fp(fp) \
	((struct ipath_filedata *)(fp)->private_data)->tidcursor
#define user_sdma_queue_fp(fp) \
	((struct ipath_filedata *)(fp)->private_data)->pq

		
#define IPATH_HAS_LINK_LATENCY 0x1
		
#define IPATH_INITTED       0x2
		
#define IPATH_RCVHDRSZ_SET  0x4
		
#define IPATH_PRESENT       0x8
#define IPATH_8BIT_IN_HT0   0x10
#define IPATH_8BIT_IN_HT1   0x20
		
#define IPATH_LINKDOWN      0x40
		
#define IPATH_LINKINIT      0x80
		
#define IPATH_LINKARMED     0x100
		
#define IPATH_LINKACTIVE    0x200
		
#define IPATH_LINKUNK       0x400
		
#define IPATH_PIO_FLUSH_WC  0x1000
		
#define IPATH_NODMA_RTAIL   0x2000
		
#define IPATH_NOCABLE       0x4000
#define IPATH_GPIO_INTR     0x8000
		
#define IPATH_4BYTE_TID     0x10000
#define IPATH_32BITCOUNTERS 0x20000
		
#define IPATH_INTREG_64     0x40000
		
#define IPATH_DISABLED      0x80000 
		
#define IPATH_GPIO_ERRINTRS 0x100000
#define IPATH_SWAP_PIOBUFS  0x200000
		
#define IPATH_HAS_SEND_DMA  0x400000
		
#define IPATH_HAS_PBC_CNT   0x800000
		
#define IPATH_NO_HRTBT      0x1000000
#define IPATH_HAS_THRESH_UPDATE 0x4000000
#define IPATH_HAS_MULT_IB_SPEED 0x8000000
#define IPATH_IB_AUTONEG_INPROG 0x10000000
#define IPATH_IB_AUTONEG_FAILED 0x20000000
		
#define IPATH_IB_LINK_DISABLED 0x40000000
#define IPATH_IB_FORCE_NOTIFY 0x80000000 

#define IPATH_GPIO_PORT0_BIT 2
#define IPATH_GPIO_RXUVL_BIT 3
#define IPATH_GPIO_OVRUN_BIT 4
#define IPATH_GPIO_LLI_BIT 5
#define IPATH_GPIO_ERRINTR_MASK 0x38

		
#define IPATH_PORT_WAITING_RCV   2
		
#define IPATH_PORT_MASTER_UNINIT 4
		
#define IPATH_PORT_WAITING_URG 5

void ipath_free_data(struct ipath_portdata *dd);
u32 __iomem *ipath_getpiobuf(struct ipath_devdata *, u32, u32 *);
void ipath_chg_pioavailkernel(struct ipath_devdata *dd, unsigned start,
				unsigned len, int avail);
void ipath_init_iba6110_funcs(struct ipath_devdata *);
void ipath_get_eeprom_info(struct ipath_devdata *);
int ipath_update_eeprom_log(struct ipath_devdata *dd);
void ipath_inc_eeprom_err(struct ipath_devdata *dd, u32 eidx, u32 incr);
u64 ipath_snap_cntr(struct ipath_devdata *, ipath_creg);
void ipath_disarm_senderrbufs(struct ipath_devdata *);
void ipath_force_pio_avail_update(struct ipath_devdata *);
void signal_ib_event(struct ipath_devdata *dd, enum ib_event_type ev);

#define IPATH_LED_PHYS 1 
#define IPATH_LED_LOG 2  
void ipath_set_led_override(struct ipath_devdata *dd, unsigned int val);

int setup_sdma(struct ipath_devdata *);
void teardown_sdma(struct ipath_devdata *);
void ipath_restart_sdma(struct ipath_devdata *);
void ipath_sdma_intr(struct ipath_devdata *);
int ipath_sdma_verbs_send(struct ipath_devdata *, struct ipath_sge_state *,
			  u32, struct ipath_verbs_txreq *);
int ipath_sdma_make_progress(struct ipath_devdata *dd);

static inline u16 ipath_sdma_descq_freecnt(const struct ipath_devdata *dd)
{
	return dd->ipath_sdma_descq_cnt -
		(dd->ipath_sdma_descq_added - dd->ipath_sdma_descq_removed) -
		1 - dd->ipath_sdma_desc_nreserved;
}

static inline void ipath_sdma_desc_reserve(struct ipath_devdata *dd, u16 cnt)
{
	dd->ipath_sdma_desc_nreserved += cnt;
}

static inline void ipath_sdma_desc_unreserve(struct ipath_devdata *dd, u16 cnt)
{
	dd->ipath_sdma_desc_nreserved -= cnt;
}

#define IPATH_DFLT_RCVHDRSIZE 9

int ipath_get_user_pages(unsigned long, size_t, struct page **);
void ipath_release_user_pages(struct page **, size_t);
void ipath_release_user_pages_on_close(struct page **, size_t);
int ipath_eeprom_read(struct ipath_devdata *, u8, void *, int);
int ipath_eeprom_write(struct ipath_devdata *, u8, const void *, int);
int ipath_tempsense_read(struct ipath_devdata *, u8 regnum);
int ipath_tempsense_write(struct ipath_devdata *, u8 regnum, u8 data);

void ipath_write_kreg_port(const struct ipath_devdata *, ipath_kreg,
			   unsigned, u64);



static inline u32 ipath_read_ureg32(const struct ipath_devdata *dd,
				    ipath_ureg regno, int port)
{
	if (!dd->ipath_kregbase || !(dd->ipath_flags & IPATH_PRESENT))
		return 0;

	return readl(regno + (u64 __iomem *)
		     (dd->ipath_uregbase +
		      (char __iomem *)dd->ipath_kregbase +
		      dd->ipath_ureg_align * port));
}

static inline void ipath_write_ureg(const struct ipath_devdata *dd,
				    ipath_ureg regno, u64 value, int port)
{
	u64 __iomem *ubase = (u64 __iomem *)
		(dd->ipath_uregbase + (char __iomem *) dd->ipath_kregbase +
		 dd->ipath_ureg_align * port);
	if (dd->ipath_kregbase)
		writeq(value, &ubase[regno]);
}

static inline u32 ipath_read_kreg32(const struct ipath_devdata *dd,
				    ipath_kreg regno)
{
	if (!dd->ipath_kregbase || !(dd->ipath_flags & IPATH_PRESENT))
		return -1;
	return readl((u32 __iomem *) & dd->ipath_kregbase[regno]);
}

static inline u64 ipath_read_kreg64(const struct ipath_devdata *dd,
				    ipath_kreg regno)
{
	if (!dd->ipath_kregbase || !(dd->ipath_flags & IPATH_PRESENT))
		return -1;

	return readq(&dd->ipath_kregbase[regno]);
}

static inline void ipath_write_kreg(const struct ipath_devdata *dd,
				    ipath_kreg regno, u64 value)
{
	if (dd->ipath_kregbase)
		writeq(value, &dd->ipath_kregbase[regno]);
}

static inline u64 ipath_read_creg(const struct ipath_devdata *dd,
				  ipath_sreg regno)
{
	if (!dd->ipath_kregbase || !(dd->ipath_flags & IPATH_PRESENT))
		return 0;

	return readq(regno + (u64 __iomem *)
		     (dd->ipath_cregbase +
		      (char __iomem *)dd->ipath_kregbase));
}

static inline u32 ipath_read_creg32(const struct ipath_devdata *dd,
					 ipath_sreg regno)
{
	if (!dd->ipath_kregbase || !(dd->ipath_flags & IPATH_PRESENT))
		return 0;
	return readl(regno + (u64 __iomem *)
		     (dd->ipath_cregbase +
		      (char __iomem *)dd->ipath_kregbase));
}

static inline void ipath_write_creg(const struct ipath_devdata *dd,
				    ipath_creg regno, u64 value)
{
	if (dd->ipath_kregbase)
		writeq(value, regno + (u64 __iomem *)
		       (dd->ipath_cregbase +
			(char __iomem *)dd->ipath_kregbase));
}

static inline void ipath_clear_rcvhdrtail(const struct ipath_portdata *pd)
{
	*((u64 *) pd->port_rcvhdrtail_kvaddr) = 0ULL;
}

static inline u32 ipath_get_rcvhdrtail(const struct ipath_portdata *pd)
{
	return (u32) le64_to_cpu(*((volatile __le64 *)
				pd->port_rcvhdrtail_kvaddr));
}

static inline u32 ipath_get_hdrqtail(const struct ipath_portdata *pd)
{
	const struct ipath_devdata *dd = pd->port_dd;
	u32 hdrqtail;

	if (dd->ipath_flags & IPATH_NODMA_RTAIL) {
		__le32 *rhf_addr;
		u32 seq;

		rhf_addr = (__le32 *) pd->port_rcvhdrq +
			pd->port_head + dd->ipath_rhf_offset;
		seq = ipath_hdrget_seq(rhf_addr);
		hdrqtail = pd->port_head;
		if (seq == pd->port_seq_cnt)
			hdrqtail++;
	} else
		hdrqtail = ipath_get_rcvhdrtail(pd);

	return hdrqtail;
}

static inline u64 ipath_read_ireg(const struct ipath_devdata *dd, ipath_kreg r)
{
	return (dd->ipath_flags & IPATH_INTREG_64) ?
		ipath_read_kreg64(dd, r) : ipath_read_kreg32(dd, r);
}

static inline u32 ipath_ib_linkstate(struct ipath_devdata *dd, u64 ibcs)
{
	u32 state = (u32)(ibcs >> dd->ibcs_ls_shift) &
		INFINIPATH_IBCS_LINKSTATE_MASK;
	if (state == INFINIPATH_IBCS_L_STATE_ACT_DEFER)
		state = INFINIPATH_IBCS_L_STATE_ACTIVE;
	return state;
}

static inline u32 ipath_ib_linktrstate(struct ipath_devdata *dd, u64 ibcs)
{
	return (u32)(ibcs >> INFINIPATH_IBCS_LINKTRAININGSTATE_SHIFT) &
		dd->ibcs_lts_mask;
}

static inline u32 ipath_ib_state(struct ipath_devdata *dd, u64 ibcs)
{
	u32 ibs;
	ibs = (u32)(ibcs >> INFINIPATH_IBCS_LINKTRAININGSTATE_SHIFT) &
		dd->ibcs_lts_mask;
	ibs |= (u32)(ibcs &
		(INFINIPATH_IBCS_LINKSTATE_MASK << dd->ibcs_ls_shift));
	return ibs;
}


struct device_driver;

extern const char ib_ipath_version[];

extern const struct attribute_group *ipath_driver_attr_groups[];

int ipath_device_create_group(struct device *, struct ipath_devdata *);
void ipath_device_remove_group(struct device *, struct ipath_devdata *);
int ipath_expose_reset(struct device *);

int ipath_init_ipathfs(void);
void ipath_exit_ipathfs(void);
int ipathfs_add_device(struct ipath_devdata *);
int ipathfs_remove_device(struct ipath_devdata *);

dma_addr_t ipath_map_page(struct pci_dev *, struct page *, unsigned long,
			  size_t, int);
dma_addr_t ipath_map_single(struct pci_dev *, void *, size_t, int);
const char *ipath_get_unit_name(int unit);

#if defined(CONFIG_X86_64)
#define ipath_flush_wc() asm volatile("sfence" ::: "memory")
#else
#define ipath_flush_wc() wmb()
#endif

extern unsigned ipath_debug; 
extern unsigned ipath_linkrecovery;
extern unsigned ipath_mtu4096;
extern struct mutex ipath_mutex;

#define IPATH_DRV_NAME		"ib_ipath"
#define IPATH_MAJOR		233
#define IPATH_USER_MINOR_BASE	0
#define IPATH_DIAGPKT_MINOR	127
#define IPATH_DIAG_MINOR_BASE	129
#define IPATH_NMINORS		255

#define ipath_dev_err(dd,fmt,...) \
	do { \
		const struct ipath_devdata *__dd = (dd); \
		if (__dd->pcidev) \
			dev_err(&__dd->pcidev->dev, "%s: " fmt, \
				ipath_get_unit_name(__dd->ipath_unit), \
				##__VA_ARGS__); \
		else \
			printk(KERN_ERR IPATH_DRV_NAME ": %s: " fmt, \
			       ipath_get_unit_name(__dd->ipath_unit), \
			       ##__VA_ARGS__); \
	} while (0)

#if _IPATH_DEBUGGING

# define __IPATH_DBG_WHICH(which,fmt,...) \
	do { \
		if (unlikely(ipath_debug & (which))) \
			printk(KERN_DEBUG IPATH_DRV_NAME ": %s: " fmt, \
			       __func__,##__VA_ARGS__); \
	} while(0)

# define ipath_dbg(fmt,...) \
	__IPATH_DBG_WHICH(__IPATH_DBG,fmt,##__VA_ARGS__)
# define ipath_cdbg(which,fmt,...) \
	__IPATH_DBG_WHICH(__IPATH_##which##DBG,fmt,##__VA_ARGS__)

#else 

# define ipath_dbg(fmt,...)
# define ipath_cdbg(which,fmt,...)

#endif 

struct ipath_hwerror_msgs {
	u64 mask;
	const char *msg;
};

#define INFINIPATH_HWE_MSG(a, b) { .mask = INFINIPATH_HWE_##a, .msg = b }

void ipath_format_hwerrors(u64 hwerrs,
			   const struct ipath_hwerror_msgs *hwerrmsgs,
			   size_t nhwerrmsgs,
			   char *msg, size_t lmsg);

#endif				
