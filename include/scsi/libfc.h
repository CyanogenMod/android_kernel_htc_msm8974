/*
 * Copyright(c) 2007 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Maintained at www.Open-FCoE.org
 */

#ifndef _LIBFC_H_
#define _LIBFC_H_

#include <linux/timer.h>
#include <linux/if.h>
#include <linux/percpu.h>

#include <scsi/scsi_transport.h>
#include <scsi/scsi_transport_fc.h>
#include <scsi/scsi_bsg_fc.h>

#include <scsi/fc/fc_fcp.h>
#include <scsi/fc/fc_ns.h>
#include <scsi/fc/fc_ms.h>
#include <scsi/fc/fc_els.h>
#include <scsi/fc/fc_gs.h>

#include <scsi/fc_frame.h>

#define	FC_FC4_PROV_SIZE	(FC_TYPE_FCP + 1)	

#define	FC_NO_ERR	0	
#define	FC_EX_TIMEOUT	1	
#define	FC_EX_CLOSED	2	

enum fc_lport_state {
	LPORT_ST_DISABLED = 0,
	LPORT_ST_FLOGI,
	LPORT_ST_DNS,
	LPORT_ST_RNN_ID,
	LPORT_ST_RSNN_NN,
	LPORT_ST_RSPN_ID,
	LPORT_ST_RFT_ID,
	LPORT_ST_RFF_ID,
	LPORT_ST_FDMI,
	LPORT_ST_RHBA,
	LPORT_ST_RPA,
	LPORT_ST_DHBA,
	LPORT_ST_DPRT,
	LPORT_ST_SCR,
	LPORT_ST_READY,
	LPORT_ST_LOGO,
	LPORT_ST_RESET
};

enum fc_disc_event {
	DISC_EV_NONE = 0,
	DISC_EV_SUCCESS,
	DISC_EV_FAILED
};

enum fc_rport_state {
	RPORT_ST_INIT,
	RPORT_ST_FLOGI,
	RPORT_ST_PLOGI_WAIT,
	RPORT_ST_PLOGI,
	RPORT_ST_PRLI,
	RPORT_ST_RTV,
	RPORT_ST_READY,
	RPORT_ST_ADISC,
	RPORT_ST_DELETE,
};

struct fc_disc_port {
	struct fc_lport    *lp;
	struct list_head   peers;
	struct work_struct rport_work;
	u32                port_id;
};

enum fc_rport_event {
	RPORT_EV_NONE = 0,
	RPORT_EV_READY,
	RPORT_EV_FAILED,
	RPORT_EV_STOP,
	RPORT_EV_LOGO
};

struct fc_rport_priv;

struct fc_rport_operations {
	void (*event_callback)(struct fc_lport *, struct fc_rport_priv *,
			       enum fc_rport_event);
};

struct fc_rport_libfc_priv {
	struct fc_lport		   *local_port;
	enum fc_rport_state        rp_state;
	u16			   flags;
	#define FC_RP_FLAGS_REC_SUPPORTED	(1 << 0)
	#define FC_RP_FLAGS_RETRY		(1 << 1)
	#define FC_RP_STARTED			(1 << 2)
	#define FC_RP_FLAGS_CONF_REQ		(1 << 3)
	unsigned int	           e_d_tov;
	unsigned int	           r_a_tov;
};

struct fc_rport_priv {
	struct fc_lport		    *local_port;
	struct fc_rport		    *rport;
	struct kref		    kref;
	enum fc_rport_state         rp_state;
	struct fc_rport_identifiers ids;
	u16			    flags;
	u16		            max_seq;
	u16			    disc_id;
	u16			    maxframe_size;
	unsigned int	            retries;
	unsigned int	            major_retries;
	unsigned int	            e_d_tov;
	unsigned int	            r_a_tov;
	struct mutex                rp_mutex;
	struct delayed_work	    retry_work;
	enum fc_rport_event         event;
	struct fc_rport_operations  *ops;
	struct list_head            peers;
	struct work_struct          event_work;
	u32			    supported_classes;
	u16                         prli_count;
	struct rcu_head		    rcu;
	u16			    sp_features;
	u8			    spp_type;
	void			    (*lld_event_callback)(struct fc_lport *,
						      struct fc_rport_priv *,
						      enum fc_rport_event);
};

struct fcoe_dev_stats {
	u64		SecondsSinceLastReset;
	u64		TxFrames;
	u64		TxWords;
	u64		RxFrames;
	u64		RxWords;
	u64		ErrorFrames;
	u64		DumpedFrames;
	u64		LinkFailureCount;
	u64		LossOfSignalCount;
	u64		InvalidTxWordCount;
	u64		InvalidCRCCount;
	u64		InputRequests;
	u64		OutputRequests;
	u64		ControlRequests;
	u64		InputBytes;
	u64		OutputBytes;
	u64		VLinkFailureCount;
	u64		MissDiscAdvCount;
};

struct fc_seq_els_data {
	enum fc_els_rjt_reason reason;
	enum fc_els_rjt_explan explan;
};

struct fc_fcp_pkt {
	spinlock_t	  scsi_pkt_lock;
	atomic_t	  ref_cnt;

	
	u32		  data_len;

	
	struct scsi_cmnd  *cmd;
	struct list_head  list;

	
	struct fc_lport   *lp;
	u8		  state;

	
	u8		  cdb_status;
	u8		  status_code;
	u8		  scsi_comp_flags;
	u32		  io_status;
	u32		  req_flags;
	u32		  scsi_resid;

	
	size_t		  xfer_len;
	struct fcp_cmnd   cdb_cmd;
	u32		  xfer_contig_end;
	u16		  max_payload;
	u16		  xfer_ddp;

	
	struct fc_rport	  *rport;
	struct fc_seq	  *seq_ptr;

	
	struct timer_list timer;
	int	          wait_for_comp;
	u32		  recov_retry;
	struct fc_seq	  *recov_seq;
	struct completion tm_done;
} ____cacheline_aligned_in_smp;


struct fc_exch_mgr;
struct fc_exch_mgr_anchor;
extern u16 fc_cpu_mask;	

struct fc_seq {
	u8  id;
	u16 ssb_stat;
	u16 cnt;
	u32 rec_data;
};

#define FC_EX_DONE		(1 << 0) 
#define FC_EX_RST_CLEANUP	(1 << 1) 

struct fc_exch {
	spinlock_t	    ex_lock;
	atomic_t	    ex_refcnt;
	enum fc_class	    class;
	struct fc_exch_mgr  *em;
	struct fc_exch_pool *pool;
	struct list_head    ex_list;
	struct fc_lport	    *lp;
	u32		    esb_stat;
	u8		    state;
	u8		    fh_type;
	u8		    seq_id;
	u8		    encaps;
	u16		    xid;
	u16		    oxid;
	u16		    rxid;
	u32		    oid;
	u32		    sid;
	u32		    did;
	u32		    r_a_tov;
	u32		    f_ctl;
	struct fc_seq       seq;
	void		    (*resp)(struct fc_seq *, struct fc_frame *, void *);
	void		    *arg;
	void		    (*destructor)(struct fc_seq *, void *);
	struct delayed_work timeout_work;
} ____cacheline_aligned_in_smp;
#define	fc_seq_exch(sp) container_of(sp, struct fc_exch, seq)


struct libfc_function_template {
	int (*frame_send)(struct fc_lport *, struct fc_frame *);

	struct fc_seq *(*elsct_send)(struct fc_lport *, u32 did,
				     struct fc_frame *, unsigned int op,
				     void (*resp)(struct fc_seq *,
					     struct fc_frame *, void *arg),
				     void *arg, u32 timer_msec);

	struct fc_seq *(*exch_seq_send)(struct fc_lport *, struct fc_frame *,
					void (*resp)(struct fc_seq *,
						     struct fc_frame *,
						     void *),
					void (*destructor)(struct fc_seq *,
							   void *),
					void *, unsigned int timer_msec);

	int (*ddp_setup)(struct fc_lport *, u16, struct scatterlist *,
			 unsigned int);
	int (*ddp_done)(struct fc_lport *, u16);
	int (*ddp_target)(struct fc_lport *, u16, struct scatterlist *,
			  unsigned int);
	void (*get_lesb)(struct fc_lport *, struct fc_els_lesb *lesb);
	int (*seq_send)(struct fc_lport *, struct fc_seq *,
			struct fc_frame *);

	void (*seq_els_rsp_send)(struct fc_frame *, enum fc_els_cmd,
				 struct fc_seq_els_data *);

	int (*seq_exch_abort)(const struct fc_seq *,
			      unsigned int timer_msec);

	void (*exch_done)(struct fc_seq *);

	struct fc_seq *(*seq_start_next)(struct fc_seq *);

	void (*seq_set_resp)(struct fc_seq *sp,
			     void (*resp)(struct fc_seq *, struct fc_frame *,
					  void *),
			     void *arg);

	struct fc_seq *(*seq_assign)(struct fc_lport *, struct fc_frame *);

	void (*seq_release)(struct fc_seq *);

	void (*exch_mgr_reset)(struct fc_lport *, u32 s_id, u32 d_id);

	void (*rport_flush_queue)(void);

	void (*lport_recv)(struct fc_lport *, struct fc_frame *);

	int (*lport_reset)(struct fc_lport *);

	void (*lport_set_port_id)(struct fc_lport *, u32 port_id,
				  struct fc_frame *);

	struct fc_rport_priv *(*rport_create)(struct fc_lport *, u32);

	int (*rport_login)(struct fc_rport_priv *);

	int (*rport_logoff)(struct fc_rport_priv *);

	void (*rport_recv_req)(struct fc_lport *, struct fc_frame *);

	struct fc_rport_priv *(*rport_lookup)(const struct fc_lport *, u32);

	void (*rport_destroy)(struct kref *);

	void (*rport_event_callback)(struct fc_lport *,
				     struct fc_rport_priv *,
				     enum fc_rport_event);

	int (*fcp_cmd_send)(struct fc_lport *, struct fc_fcp_pkt *,
			    void (*resp)(struct fc_seq *, struct fc_frame *,
					 void *));

	void (*fcp_cleanup)(struct fc_lport *);

	void (*fcp_abort_io)(struct fc_lport *);

	void (*disc_recv_req)(struct fc_lport *, struct fc_frame *);

	void (*disc_start)(void (*disc_callback)(struct fc_lport *,
						 enum fc_disc_event),
			   struct fc_lport *);

	void (*disc_stop) (struct fc_lport *);

	void (*disc_stop_final) (struct fc_lport *);
};

struct fc_disc {
	unsigned char         retry_count;
	unsigned char         pending;
	unsigned char         requested;
	unsigned short        seq_count;
	unsigned char         buf_len;
	u16                   disc_id;

	struct list_head      rports;
	void		      *priv;
	struct mutex	      disc_mutex;
	struct fc_gpn_ft_resp partial_buf;
	struct delayed_work   disc_work;

	void (*disc_callback)(struct fc_lport *,
			      enum fc_disc_event);
};

extern struct blocking_notifier_head fc_lport_notifier_head;
enum fc_lport_event {
	FC_LPORT_EV_ADD,
	FC_LPORT_EV_DEL,
};

struct fc_lport {
	
	struct Scsi_Host	       *host;
	struct list_head	       ema_list;
	struct fc_rport_priv	       *dns_rdata;
	struct fc_rport_priv	       *ms_rdata;
	struct fc_rport_priv	       *ptp_rdata;
	void			       *scsi_priv;
	struct fc_disc                 disc;

	
	struct list_head	       vports;
	struct fc_vport		       *vport;

	
	struct libfc_function_template tt;
	u8			       link_up;
	u8			       qfull;
	enum fc_lport_state	       state;
	unsigned long		       boot_time;
	struct fc_host_statistics      host_stats;
	struct fcoe_dev_stats __percpu *dev_stats;
	u8			       retry_count;

	
	u32                            port_id;
	u64			       wwpn;
	u64			       wwnn;
	unsigned int		       service_params;
	unsigned int		       e_d_tov;
	unsigned int		       r_a_tov;
	struct fc_els_rnid_gen	       rnid_gen;

	
	u32			       sg_supp:1;
	u32			       seq_offload:1;
	u32			       crc_offload:1;
	u32			       lro_enabled:1;
	u32			       does_npiv:1;
	u32			       npiv_enabled:1;
	u32			       point_to_multipoint:1;
	u32			       fdmi_enabled:1;
	u32			       mfs;
	u8			       max_retry_count;
	u8			       max_rport_retry_count;
	u16			       rport_priv_size;
	u16			       link_speed;
	u16			       link_supported_speeds;
	u16			       lro_xid;
	unsigned int		       lso_max;
	struct fc_ns_fts	       fcts;

	
	struct mutex                   lp_mutex;
	struct list_head               list;
	struct delayed_work	       retry_work;
	void			       *prov[FC_FC4_PROV_SIZE];
	struct list_head               lport_list;
};

struct fc4_prov {
	int (*prli)(struct fc_rport_priv *, u32 spp_len,
		    const struct fc_els_spp *spp_in,
		    struct fc_els_spp *spp_out);
	void (*prlo)(struct fc_rport_priv *);
	void (*recv)(struct fc_lport *, struct fc_frame *);
	struct module *module;
};

int fc_fc4_register_provider(enum fc_fh_type type, struct fc4_prov *);
void fc_fc4_deregister_provider(enum fc_fh_type type, struct fc4_prov *);


static inline int fc_lport_test_ready(struct fc_lport *lport)
{
	return lport->state == LPORT_ST_READY;
}

static inline void fc_set_wwnn(struct fc_lport *lport, u64 wwnn)
{
	lport->wwnn = wwnn;
}

static inline void fc_set_wwpn(struct fc_lport *lport, u64 wwnn)
{
	lport->wwpn = wwnn;
}

static inline void fc_lport_state_enter(struct fc_lport *lport,
					enum fc_lport_state state)
{
	if (state != lport->state)
		lport->retry_count = 0;
	lport->state = state;
}

static inline int fc_lport_init_stats(struct fc_lport *lport)
{
	lport->dev_stats = alloc_percpu(struct fcoe_dev_stats);
	if (!lport->dev_stats)
		return -ENOMEM;
	return 0;
}

static inline void fc_lport_free_stats(struct fc_lport *lport)
{
	free_percpu(lport->dev_stats);
}

static inline void *lport_priv(const struct fc_lport *lport)
{
	return (void *)(lport + 1);
}

static inline struct fc_lport *
libfc_host_alloc(struct scsi_host_template *sht, int priv_size)
{
	struct fc_lport *lport;
	struct Scsi_Host *shost;

	shost = scsi_host_alloc(sht, sizeof(*lport) + priv_size);
	if (!shost)
		return NULL;
	lport = shost_priv(shost);
	lport->host = shost;
	INIT_LIST_HEAD(&lport->ema_list);
	INIT_LIST_HEAD(&lport->vports);
	return lport;
}

static inline bool fc_fcp_is_read(const struct fc_fcp_pkt *fsp)
{
	if (fsp && fsp->cmd)
		return fsp->cmd->sc_data_direction == DMA_FROM_DEVICE;
	return false;
}

int fc_lport_init(struct fc_lport *);
int fc_lport_destroy(struct fc_lport *);
int fc_fabric_logoff(struct fc_lport *);
int fc_fabric_login(struct fc_lport *);
void __fc_linkup(struct fc_lport *);
void fc_linkup(struct fc_lport *);
void __fc_linkdown(struct fc_lport *);
void fc_linkdown(struct fc_lport *);
void fc_vport_setlink(struct fc_lport *);
void fc_vports_linkchange(struct fc_lport *);
int fc_lport_config(struct fc_lport *);
int fc_lport_reset(struct fc_lport *);
int fc_set_mfs(struct fc_lport *, u32 mfs);
struct fc_lport *libfc_vport_create(struct fc_vport *, int privsize);
struct fc_lport *fc_vport_id_lookup(struct fc_lport *, u32 port_id);
int fc_lport_bsg_request(struct fc_bsg_job *);
void fc_lport_set_local_id(struct fc_lport *, u32 port_id);
void fc_lport_iterate(void (*func)(struct fc_lport *, void *), void *);

int fc_rport_init(struct fc_lport *);
void fc_rport_terminate_io(struct fc_rport *);

int fc_disc_init(struct fc_lport *);

static inline struct fc_lport *fc_disc_lport(struct fc_disc *disc)
{
	return container_of(disc, struct fc_lport, disc);
}

int fc_fcp_init(struct fc_lport *);
void fc_fcp_destroy(struct fc_lport *);

int fc_queuecommand(struct Scsi_Host *, struct scsi_cmnd *);
int fc_eh_abort(struct scsi_cmnd *);
int fc_eh_device_reset(struct scsi_cmnd *);
int fc_eh_host_reset(struct scsi_cmnd *);
int fc_slave_alloc(struct scsi_device *);
int fc_change_queue_depth(struct scsi_device *, int qdepth, int reason);
int fc_change_queue_type(struct scsi_device *, int tag_type);

int fc_elsct_init(struct fc_lport *);
struct fc_seq *fc_elsct_send(struct fc_lport *, u32 did,
				    struct fc_frame *,
				    unsigned int op,
				    void (*resp)(struct fc_seq *,
						 struct fc_frame *,
						 void *arg),
				    void *arg, u32 timer_msec);
void fc_lport_flogi_resp(struct fc_seq *, struct fc_frame *, void *);
void fc_lport_logo_resp(struct fc_seq *, struct fc_frame *, void *);
void fc_fill_reply_hdr(struct fc_frame *, const struct fc_frame *,
		       enum fc_rctl, u32 parm_offset);
void fc_fill_hdr(struct fc_frame *, const struct fc_frame *,
		 enum fc_rctl, u32 f_ctl, u16 seq_cnt, u32 parm_offset);


int fc_exch_init(struct fc_lport *);
struct fc_exch_mgr_anchor *fc_exch_mgr_add(struct fc_lport *,
					   struct fc_exch_mgr *,
					   bool (*match)(struct fc_frame *));
void fc_exch_mgr_del(struct fc_exch_mgr_anchor *);
int fc_exch_mgr_list_clone(struct fc_lport *src, struct fc_lport *dst);
struct fc_exch_mgr *fc_exch_mgr_alloc(struct fc_lport *, enum fc_class class,
				      u16 min_xid, u16 max_xid,
				      bool (*match)(struct fc_frame *));
void fc_exch_mgr_free(struct fc_lport *);
void fc_exch_recv(struct fc_lport *, struct fc_frame *);
void fc_exch_mgr_reset(struct fc_lport *, u32 s_id, u32 d_id);

void fc_get_host_speed(struct Scsi_Host *);
void fc_get_host_port_state(struct Scsi_Host *);
void fc_set_rport_loss_tmo(struct fc_rport *, u32 timeout);
struct fc_host_statistics *fc_get_host_stats(struct Scsi_Host *);

#endif 
