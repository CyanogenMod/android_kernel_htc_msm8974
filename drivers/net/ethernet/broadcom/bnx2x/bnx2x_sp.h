/* bnx2x_sp.h: Broadcom Everest network driver.
 *
 * Copyright (c) 2011-2012 Broadcom Corporation
 *
 * Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2, available
 * at http://www.gnu.org/licenses/old-licenses/gpl-2.0.html (the "GPL").
 *
 * Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a
 * license other than the GPL, without Broadcom's express prior written
 * consent.
 *
 * Maintained by: Eilon Greenstein <eilong@broadcom.com>
 * Written by: Vladislav Zolotarov
 *
 */
#ifndef BNX2X_SP_VERBS
#define BNX2X_SP_VERBS

struct bnx2x;
struct eth_context;

enum {
	RAMROD_TX,
	RAMROD_RX,
	
	RAMROD_COMP_WAIT,
	
	RAMROD_DRV_CLR_ONLY,
	
	RAMROD_RESTORE,
	 
	RAMROD_EXEC,
	RAMROD_CONT,
};

typedef enum {
	BNX2X_OBJ_TYPE_RX,
	BNX2X_OBJ_TYPE_TX,
	BNX2X_OBJ_TYPE_RX_TX,
} bnx2x_obj_type;

enum {
	BNX2X_FILTER_MAC_PENDING,
	BNX2X_FILTER_VLAN_PENDING,
	BNX2X_FILTER_VLAN_MAC_PENDING,
	BNX2X_FILTER_RX_MODE_PENDING,
	BNX2X_FILTER_RX_MODE_SCHED,
	BNX2X_FILTER_ISCSI_ETH_START_SCHED,
	BNX2X_FILTER_ISCSI_ETH_STOP_SCHED,
	BNX2X_FILTER_FCOE_ETH_START_SCHED,
	BNX2X_FILTER_FCOE_ETH_STOP_SCHED,
	BNX2X_FILTER_MCAST_PENDING,
	BNX2X_FILTER_MCAST_SCHED,
	BNX2X_FILTER_RSS_CONF_PENDING,
};

struct bnx2x_raw_obj {
	u8		func_id;

	
	u8		cl_id;
	u32		cid;

	
	void		*rdata;
	dma_addr_t	rdata_mapping;

	
	int		state;   
	unsigned long	*pstate; 

	bnx2x_obj_type	obj_type;

	int (*wait_comp)(struct bnx2x *bp,
			 struct bnx2x_raw_obj *o);

	bool (*check_pending)(struct bnx2x_raw_obj *o);
	void (*clear_pending)(struct bnx2x_raw_obj *o);
	void (*set_pending)(struct bnx2x_raw_obj *o);
};

struct bnx2x_mac_ramrod_data {
	u8 mac[ETH_ALEN];
};

struct bnx2x_vlan_ramrod_data {
	u16 vlan;
};

struct bnx2x_vlan_mac_ramrod_data {
	u8 mac[ETH_ALEN];
	u16 vlan;
};

union bnx2x_classification_ramrod_data {
	struct bnx2x_mac_ramrod_data mac;
	struct bnx2x_vlan_ramrod_data vlan;
	struct bnx2x_vlan_mac_ramrod_data vlan_mac;
};

enum bnx2x_vlan_mac_cmd {
	BNX2X_VLAN_MAC_ADD,
	BNX2X_VLAN_MAC_DEL,
	BNX2X_VLAN_MAC_MOVE,
};

struct bnx2x_vlan_mac_data {
	
	enum bnx2x_vlan_mac_cmd cmd;
	unsigned long vlan_mac_flags;

	
	struct bnx2x_vlan_mac_obj *target_obj;

	union bnx2x_classification_ramrod_data u;
};

union bnx2x_exe_queue_cmd_data {
	struct bnx2x_vlan_mac_data vlan_mac;

	struct {
		
	} mcast;
};

struct bnx2x_exeq_elem {
	struct list_head		link;

	
	int				cmd_len;

	union bnx2x_exe_queue_cmd_data	cmd_data;
};

union bnx2x_qable_obj;

union bnx2x_exeq_comp_elem {
	union event_ring_elem *elem;
};

struct bnx2x_exe_queue_obj;

typedef int (*exe_q_validate)(struct bnx2x *bp,
			      union bnx2x_qable_obj *o,
			      struct bnx2x_exeq_elem *elem);

typedef int (*exe_q_remove)(struct bnx2x *bp,
			    union bnx2x_qable_obj *o,
			    struct bnx2x_exeq_elem *elem);

typedef int (*exe_q_optimize)(struct bnx2x *bp,
			      union bnx2x_qable_obj *o,
			      struct bnx2x_exeq_elem *elem);
typedef int (*exe_q_execute)(struct bnx2x *bp,
			     union bnx2x_qable_obj *o,
			     struct list_head *exe_chunk,
			     unsigned long *ramrod_flags);
typedef struct bnx2x_exeq_elem *
			(*exe_q_get)(struct bnx2x_exe_queue_obj *o,
				     struct bnx2x_exeq_elem *elem);

struct bnx2x_exe_queue_obj {
	struct list_head	exe_queue;

	struct list_head	pending_comp;

	spinlock_t		lock;

	
	int			exe_chunk_len;

	union bnx2x_qable_obj	*owner;

	
	exe_q_validate		validate;

	 exe_q_remove		remove;

	exe_q_optimize		optimize;

	exe_q_execute		execute;

	exe_q_get		get;
};
struct bnx2x_vlan_mac_registry_elem {
	struct list_head	link;

	int			cam_offset;

	
	unsigned long		vlan_mac_flags;

	union bnx2x_classification_ramrod_data u;
};

enum {
	BNX2X_UC_LIST_MAC,
	BNX2X_ETH_MAC,
	BNX2X_ISCSI_ETH_MAC,
	BNX2X_NETQ_ETH_MAC,
	BNX2X_DONT_CONSUME_CAM_CREDIT,
	BNX2X_DONT_CONSUME_CAM_CREDIT_DEST,
};

struct bnx2x_vlan_mac_ramrod_params {
	
	struct bnx2x_vlan_mac_obj *vlan_mac_obj;

	
	unsigned long ramrod_flags;

	
	struct bnx2x_vlan_mac_data user_req;
};

struct bnx2x_vlan_mac_obj {
	struct bnx2x_raw_obj raw;

	struct list_head		head;

	
	struct bnx2x_exe_queue_obj	exe_queue;

	
	struct bnx2x_credit_pool_obj	*macs_pool;

	
	struct bnx2x_credit_pool_obj	*vlans_pool;

	
	int				ramrod_cmd;

	int (*get_n_elements)(struct bnx2x *bp, struct bnx2x_vlan_mac_obj *o,
			      int n, u8 *buf);


	int (*check_add)(struct bnx2x *bp,
			 struct bnx2x_vlan_mac_obj *o,
			 union bnx2x_classification_ramrod_data *data);

	struct bnx2x_vlan_mac_registry_elem *
		(*check_del)(struct bnx2x *bp,
			     struct bnx2x_vlan_mac_obj *o,
			     union bnx2x_classification_ramrod_data *data);

	bool (*check_move)(struct bnx2x *bp,
			   struct bnx2x_vlan_mac_obj *src_o,
			   struct bnx2x_vlan_mac_obj *dst_o,
			   union bnx2x_classification_ramrod_data *data);

	bool (*get_credit)(struct bnx2x_vlan_mac_obj *o);
	bool (*put_credit)(struct bnx2x_vlan_mac_obj *o);
	bool (*get_cam_offset)(struct bnx2x_vlan_mac_obj *o, int *offset);
	bool (*put_cam_offset)(struct bnx2x_vlan_mac_obj *o, int offset);

	void (*set_one_rule)(struct bnx2x *bp,
			     struct bnx2x_vlan_mac_obj *o,
			     struct bnx2x_exeq_elem *elem, int rule_idx,
			     int cam_offset);

	int (*delete_all)(struct bnx2x *bp,
			  struct bnx2x_vlan_mac_obj *o,
			  unsigned long *vlan_mac_flags,
			  unsigned long *ramrod_flags);

	int (*restore)(struct bnx2x *bp,
		       struct bnx2x_vlan_mac_ramrod_params *p,
		       struct bnx2x_vlan_mac_registry_elem **ppos);

	int (*complete)(struct bnx2x *bp, struct bnx2x_vlan_mac_obj *o,
			union event_ring_elem *cqe,
			unsigned long *ramrod_flags);

	int (*wait)(struct bnx2x *bp, struct bnx2x_vlan_mac_obj *o);
};

enum {
	BNX2X_LLH_CAM_ISCSI_ETH_LINE = 0,
	BNX2X_LLH_CAM_ETH_LINE,
	BNX2X_LLH_CAM_MAX_PF_LINE = NIG_REG_LLH1_FUNC_MEM_SIZE / 2
};



enum {
	BNX2X_RX_MODE_FCOE_ETH,
	BNX2X_RX_MODE_ISCSI_ETH,
};

enum {
	BNX2X_ACCEPT_UNICAST,
	BNX2X_ACCEPT_MULTICAST,
	BNX2X_ACCEPT_ALL_UNICAST,
	BNX2X_ACCEPT_ALL_MULTICAST,
	BNX2X_ACCEPT_BROADCAST,
	BNX2X_ACCEPT_UNMATCHED,
	BNX2X_ACCEPT_ANY_VLAN
};

struct bnx2x_rx_mode_ramrod_params {
	struct bnx2x_rx_mode_obj *rx_mode_obj;
	unsigned long *pstate;
	int state;
	u8 cl_id;
	u32 cid;
	u8 func_id;
	unsigned long ramrod_flags;
	unsigned long rx_mode_flags;

	void *rdata;
	dma_addr_t rdata_mapping;

	
	unsigned long rx_accept_flags;

	
	unsigned long tx_accept_flags;
};

struct bnx2x_rx_mode_obj {
	int (*config_rx_mode)(struct bnx2x *bp,
			      struct bnx2x_rx_mode_ramrod_params *p);

	int (*wait_comp)(struct bnx2x *bp,
			 struct bnx2x_rx_mode_ramrod_params *p);
};


struct bnx2x_mcast_list_elem {
	struct list_head link;
	u8 *mac;
};

union bnx2x_mcast_config_data {
	u8 *mac;
	u8 bin; 
};

struct bnx2x_mcast_ramrod_params {
	struct bnx2x_mcast_obj *mcast_obj;

	
	unsigned long ramrod_flags;

	struct list_head mcast_list; 
	int mcast_list_len;
};

enum {
	BNX2X_MCAST_CMD_ADD,
	BNX2X_MCAST_CMD_CONT,
	BNX2X_MCAST_CMD_DEL,
	BNX2X_MCAST_CMD_RESTORE,
};

struct bnx2x_mcast_obj {
	struct bnx2x_raw_obj raw;

	union {
		struct {
		#define BNX2X_MCAST_BINS_NUM	256
		#define BNX2X_MCAST_VEC_SZ	(BNX2X_MCAST_BINS_NUM / 64)
			u64 vec[BNX2X_MCAST_VEC_SZ];

			int num_bins_set;
		} aprox_match;

		struct {
			struct list_head macs;
			int num_macs_set;
		} exact_match;
	} registry;

	
	struct list_head pending_cmds_head;

	
	int sched_state;

	
	int max_cmd_len;

	int total_pending_num;

	u8 engine_id;

	int (*config_mcast)(struct bnx2x *bp,
				struct bnx2x_mcast_ramrod_params *p, int cmd);

	int (*hdl_restore)(struct bnx2x *bp, struct bnx2x_mcast_obj *o,
			   int start_bin, int *rdata_idx);

	int (*enqueue_cmd)(struct bnx2x *bp, struct bnx2x_mcast_obj *o,
			   struct bnx2x_mcast_ramrod_params *p, int cmd);

	void (*set_one_rule)(struct bnx2x *bp,
			     struct bnx2x_mcast_obj *o, int idx,
			     union bnx2x_mcast_config_data *cfg_data, int cmd);

	bool (*check_pending)(struct bnx2x_mcast_obj *o);

	void (*set_sched)(struct bnx2x_mcast_obj *o);
	void (*clear_sched)(struct bnx2x_mcast_obj *o);
	bool (*check_sched)(struct bnx2x_mcast_obj *o);

	
	int (*wait_comp)(struct bnx2x *bp, struct bnx2x_mcast_obj *o);

	int (*validate)(struct bnx2x *bp,
			struct bnx2x_mcast_ramrod_params *p, int cmd);

	void (*revert)(struct bnx2x *bp,
		       struct bnx2x_mcast_ramrod_params *p,
		       int old_num_bins);

	int (*get_registry_size)(struct bnx2x_mcast_obj *o);
	void (*set_registry_size)(struct bnx2x_mcast_obj *o, int n);
};

struct bnx2x_credit_pool_obj {

	
	atomic_t	credit;

	
	int		pool_sz;

#define BNX2X_POOL_VEC_SIZE	(MAX_MAC_CREDIT_E2 / 64)
	u64		pool_mirror[BNX2X_POOL_VEC_SIZE];

	
	int		base_pool_offset;

	bool (*get_entry)(struct bnx2x_credit_pool_obj *o, int *entry);

	bool (*put_entry)(struct bnx2x_credit_pool_obj *o, int entry);

	bool (*get)(struct bnx2x_credit_pool_obj *o, int cnt);

	bool (*put)(struct bnx2x_credit_pool_obj *o, int cnt);

	int (*check)(struct bnx2x_credit_pool_obj *o);
};

enum {
	
	BNX2X_RSS_MODE_DISABLED,
	BNX2X_RSS_MODE_REGULAR,
	BNX2X_RSS_MODE_VLAN_PRI,
	BNX2X_RSS_MODE_E1HOV_PRI,
	BNX2X_RSS_MODE_IP_DSCP,

	BNX2X_RSS_SET_SRCH, 

	BNX2X_RSS_IPV4,
	BNX2X_RSS_IPV4_TCP,
	BNX2X_RSS_IPV6,
	BNX2X_RSS_IPV6_TCP,
};

struct bnx2x_config_rss_params {
	struct bnx2x_rss_config_obj *rss_obj;

	
	unsigned long	ramrod_flags;

	
	unsigned long	rss_flags;

	
	u8		rss_result_mask;

	
	u8		ind_table[T_ETH_INDIRECTION_TABLE_SIZE];

	
	u32		rss_key[10];

	
	u16		toe_rss_bitmap;
};

struct bnx2x_rss_config_obj {
	struct bnx2x_raw_obj	raw;

	
	u8			engine_id;

	
	u8			ind_table[T_ETH_INDIRECTION_TABLE_SIZE];

	int (*config_rss)(struct bnx2x *bp,
			  struct bnx2x_config_rss_params *p);
};


enum {
	BNX2X_Q_UPDATE_IN_VLAN_REM,
	BNX2X_Q_UPDATE_IN_VLAN_REM_CHNG,
	BNX2X_Q_UPDATE_OUT_VLAN_REM,
	BNX2X_Q_UPDATE_OUT_VLAN_REM_CHNG,
	BNX2X_Q_UPDATE_ANTI_SPOOF,
	BNX2X_Q_UPDATE_ANTI_SPOOF_CHNG,
	BNX2X_Q_UPDATE_ACTIVATE,
	BNX2X_Q_UPDATE_ACTIVATE_CHNG,
	BNX2X_Q_UPDATE_DEF_VLAN_EN,
	BNX2X_Q_UPDATE_DEF_VLAN_EN_CHNG,
	BNX2X_Q_UPDATE_SILENT_VLAN_REM_CHNG,
	BNX2X_Q_UPDATE_SILENT_VLAN_REM
};

enum bnx2x_q_state {
	BNX2X_Q_STATE_RESET,
	BNX2X_Q_STATE_INITIALIZED,
	BNX2X_Q_STATE_ACTIVE,
	BNX2X_Q_STATE_MULTI_COS,
	BNX2X_Q_STATE_MCOS_TERMINATED,
	BNX2X_Q_STATE_INACTIVE,
	BNX2X_Q_STATE_STOPPED,
	BNX2X_Q_STATE_TERMINATED,
	BNX2X_Q_STATE_FLRED,
	BNX2X_Q_STATE_MAX,
};

enum bnx2x_queue_cmd {
	BNX2X_Q_CMD_INIT,
	BNX2X_Q_CMD_SETUP,
	BNX2X_Q_CMD_SETUP_TX_ONLY,
	BNX2X_Q_CMD_DEACTIVATE,
	BNX2X_Q_CMD_ACTIVATE,
	BNX2X_Q_CMD_UPDATE,
	BNX2X_Q_CMD_UPDATE_TPA,
	BNX2X_Q_CMD_HALT,
	BNX2X_Q_CMD_CFC_DEL,
	BNX2X_Q_CMD_TERMINATE,
	BNX2X_Q_CMD_EMPTY,
	BNX2X_Q_CMD_MAX,
};

enum {
	BNX2X_Q_FLG_TPA,
	BNX2X_Q_FLG_TPA_IPV6,
	BNX2X_Q_FLG_TPA_GRO,
	BNX2X_Q_FLG_STATS,
	BNX2X_Q_FLG_ZERO_STATS,
	BNX2X_Q_FLG_ACTIVE,
	BNX2X_Q_FLG_OV,
	BNX2X_Q_FLG_VLAN,
	BNX2X_Q_FLG_COS,
	BNX2X_Q_FLG_HC,
	BNX2X_Q_FLG_HC_EN,
	BNX2X_Q_FLG_DHC,
	BNX2X_Q_FLG_FCOE,
	BNX2X_Q_FLG_LEADING_RSS,
	BNX2X_Q_FLG_MCAST,
	BNX2X_Q_FLG_DEF_VLAN,
	BNX2X_Q_FLG_TX_SWITCH,
	BNX2X_Q_FLG_TX_SEC,
	BNX2X_Q_FLG_ANTI_SPOOF,
	BNX2X_Q_FLG_SILENT_VLAN_REM
};

enum bnx2x_q_type {
	BNX2X_Q_TYPE_HAS_RX,
	BNX2X_Q_TYPE_HAS_TX,
};

#define BNX2X_PRIMARY_CID_INDEX			0
#define BNX2X_MULTI_TX_COS_E1X			3 
#define BNX2X_MULTI_TX_COS_E2_E3A0		2
#define BNX2X_MULTI_TX_COS_E3B0			3
#define BNX2X_MULTI_TX_COS			3 


struct bnx2x_queue_init_params {
	struct {
		unsigned long	flags;
		u16		hc_rate;
		u8		fw_sb_id;
		u8		sb_cq_index;
	} tx;

	struct {
		unsigned long	flags;
		u16		hc_rate;
		u8		fw_sb_id;
		u8		sb_cq_index;
	} rx;

	
	struct eth_context *cxts[BNX2X_MULTI_TX_COS];

	
	u8 max_cos;
};

struct bnx2x_queue_terminate_params {
	
	u8 cid_index;
};

struct bnx2x_queue_cfc_del_params {
	
	u8 cid_index;
};

struct bnx2x_queue_update_params {
	unsigned long	update_flags; 
	u16		def_vlan;
	u16		silent_removal_value;
	u16		silent_removal_mask;
	u8		cid_index;
};

struct rxq_pause_params {
	u16		bd_th_lo;
	u16		bd_th_hi;
	u16		rcq_th_lo;
	u16		rcq_th_hi;
	u16		sge_th_lo; 
	u16		sge_th_hi; 
	u16		pri_map;
};

struct bnx2x_general_setup_params {
	
	u8		stat_id;

	u8		spcl_id;
	u16		mtu;
	u8		cos;
};

struct bnx2x_rxq_setup_params {
	
	dma_addr_t	dscr_map;
	dma_addr_t	sge_map;
	dma_addr_t	rcq_map;
	dma_addr_t	rcq_np_map;

	u16		drop_flags;
	u16		buf_sz;
	u8		fw_sb_id;
	u8		cl_qzone_id;

	
	u16		tpa_agg_sz;
	u16		sge_buf_sz;
	u8		max_sges_pkt;
	u8		max_tpa_queues;
	u8		rss_engine_id;

	
	u8		mcast_engine_id;

	u8		cache_line_log;

	u8		sb_cq_index;

	
	u16 silent_removal_value;
	u16 silent_removal_mask;
};

struct bnx2x_txq_setup_params {
	
	dma_addr_t	dscr_map;

	u8		fw_sb_id;
	u8		sb_cq_index;
	u8		cos;		
	u16		traffic_type;
	
	u8		tss_leading_cl_id;

	
	u16		default_vlan;
};

struct bnx2x_queue_setup_params {
	struct bnx2x_general_setup_params gen_params;
	struct bnx2x_txq_setup_params txq_params;
	struct bnx2x_rxq_setup_params rxq_params;
	struct rxq_pause_params pause_params;
	unsigned long flags;
};

struct bnx2x_queue_setup_tx_only_params {
	struct bnx2x_general_setup_params	gen_params;
	struct bnx2x_txq_setup_params		txq_params;
	unsigned long				flags;
	
	u8					cid_index;
};

struct bnx2x_queue_state_params {
	struct bnx2x_queue_sp_obj *q_obj;

	
	enum bnx2x_queue_cmd cmd;

	
	unsigned long ramrod_flags;

	
	union {
		struct bnx2x_queue_update_params	update;
		struct bnx2x_queue_setup_params		setup;
		struct bnx2x_queue_init_params		init;
		struct bnx2x_queue_setup_tx_only_params	tx_only;
		struct bnx2x_queue_terminate_params	terminate;
		struct bnx2x_queue_cfc_del_params	cfc_del;
	} params;
};

struct bnx2x_queue_sp_obj {
	u32		cids[BNX2X_MULTI_TX_COS];
	u8		cl_id;
	u8		func_id;

	u8 max_cos;
	u8 num_tx_only, next_tx_only;

	enum bnx2x_q_state state, next_state;

	
	unsigned long	type;

	unsigned long	pending;

	
	void		*rdata;
	dma_addr_t	rdata_mapping;

	int (*send_cmd)(struct bnx2x *bp,
			struct bnx2x_queue_state_params *params);

	int (*set_pending)(struct bnx2x_queue_sp_obj *o,
			   struct bnx2x_queue_state_params *params);

	int (*check_transition)(struct bnx2x *bp,
				struct bnx2x_queue_sp_obj *o,
				struct bnx2x_queue_state_params *params);

	int (*complete_cmd)(struct bnx2x *bp,
			    struct bnx2x_queue_sp_obj *o,
			    enum bnx2x_queue_cmd);

	int (*wait_comp)(struct bnx2x *bp,
			 struct bnx2x_queue_sp_obj *o,
			 enum bnx2x_queue_cmd cmd);
};

enum bnx2x_func_state {
	BNX2X_F_STATE_RESET,
	BNX2X_F_STATE_INITIALIZED,
	BNX2X_F_STATE_STARTED,
	BNX2X_F_STATE_TX_STOPPED,
	BNX2X_F_STATE_MAX,
};

enum bnx2x_func_cmd {
	BNX2X_F_CMD_HW_INIT,
	BNX2X_F_CMD_START,
	BNX2X_F_CMD_STOP,
	BNX2X_F_CMD_HW_RESET,
	BNX2X_F_CMD_TX_STOP,
	BNX2X_F_CMD_TX_START,
	BNX2X_F_CMD_MAX,
};

struct bnx2x_func_hw_init_params {
	u32 load_phase;
};

struct bnx2x_func_hw_reset_params {
	u32 reset_phase;
};

struct bnx2x_func_start_params {
	u16 mf_mode;

	
	u16 sd_vlan_tag;

	
	u8 network_cos_mode;
};

struct bnx2x_func_tx_start_params {
	struct priority_cos traffic_type_to_priority_cos[MAX_TRAFFIC_TYPES];
	u8 dcb_enabled;
	u8 dcb_version;
	u8 dont_add_pri_0_en;
};

struct bnx2x_func_state_params {
	struct bnx2x_func_sp_obj *f_obj;

	
	enum bnx2x_func_cmd cmd;

	
	unsigned long	ramrod_flags;

	
	union {
		struct bnx2x_func_hw_init_params hw_init;
		struct bnx2x_func_hw_reset_params hw_reset;
		struct bnx2x_func_start_params start;
		struct bnx2x_func_tx_start_params tx_start;
	} params;
};

struct bnx2x_func_sp_drv_ops {
	int (*init_hw_cmn_chip)(struct bnx2x *bp);
	int (*init_hw_cmn)(struct bnx2x *bp);
	int (*init_hw_port)(struct bnx2x *bp);
	int (*init_hw_func)(struct bnx2x *bp);

	
	void (*reset_hw_cmn)(struct bnx2x *bp);
	void (*reset_hw_port)(struct bnx2x *bp);
	void (*reset_hw_func)(struct bnx2x *bp);

	
	int (*gunzip_init)(struct bnx2x *bp);
	void (*gunzip_end)(struct bnx2x *bp);

	
	int (*init_fw)(struct bnx2x *bp);
	void (*release_fw)(struct bnx2x *bp);
};

struct bnx2x_func_sp_obj {
	enum bnx2x_func_state	state, next_state;

	unsigned long		pending;

	
	void			*rdata;
	dma_addr_t		rdata_mapping;

	struct mutex		one_pending_mutex;

	
	struct bnx2x_func_sp_drv_ops	*drv;

	int (*send_cmd)(struct bnx2x *bp,
			struct bnx2x_func_state_params *params);

	int (*check_transition)(struct bnx2x *bp,
				struct bnx2x_func_sp_obj *o,
				struct bnx2x_func_state_params *params);

	int (*complete_cmd)(struct bnx2x *bp,
			    struct bnx2x_func_sp_obj *o,
			    enum bnx2x_func_cmd cmd);

	int (*wait_comp)(struct bnx2x *bp, struct bnx2x_func_sp_obj *o,
			 enum bnx2x_func_cmd cmd);
};

union bnx2x_qable_obj {
	struct bnx2x_vlan_mac_obj vlan_mac;
};
void bnx2x_init_func_obj(struct bnx2x *bp,
			 struct bnx2x_func_sp_obj *obj,
			 void *rdata, dma_addr_t rdata_mapping,
			 struct bnx2x_func_sp_drv_ops *drv_iface);

int bnx2x_func_state_change(struct bnx2x *bp,
			    struct bnx2x_func_state_params *params);

enum bnx2x_func_state bnx2x_func_get_state(struct bnx2x *bp,
					   struct bnx2x_func_sp_obj *o);
void bnx2x_init_queue_obj(struct bnx2x *bp,
			  struct bnx2x_queue_sp_obj *obj, u8 cl_id, u32 *cids,
			  u8 cid_cnt, u8 func_id, void *rdata,
			  dma_addr_t rdata_mapping, unsigned long type);

int bnx2x_queue_state_change(struct bnx2x *bp,
			     struct bnx2x_queue_state_params *params);

void bnx2x_init_mac_obj(struct bnx2x *bp,
			struct bnx2x_vlan_mac_obj *mac_obj,
			u8 cl_id, u32 cid, u8 func_id, void *rdata,
			dma_addr_t rdata_mapping, int state,
			unsigned long *pstate, bnx2x_obj_type type,
			struct bnx2x_credit_pool_obj *macs_pool);

void bnx2x_init_vlan_obj(struct bnx2x *bp,
			 struct bnx2x_vlan_mac_obj *vlan_obj,
			 u8 cl_id, u32 cid, u8 func_id, void *rdata,
			 dma_addr_t rdata_mapping, int state,
			 unsigned long *pstate, bnx2x_obj_type type,
			 struct bnx2x_credit_pool_obj *vlans_pool);

void bnx2x_init_vlan_mac_obj(struct bnx2x *bp,
			     struct bnx2x_vlan_mac_obj *vlan_mac_obj,
			     u8 cl_id, u32 cid, u8 func_id, void *rdata,
			     dma_addr_t rdata_mapping, int state,
			     unsigned long *pstate, bnx2x_obj_type type,
			     struct bnx2x_credit_pool_obj *macs_pool,
			     struct bnx2x_credit_pool_obj *vlans_pool);

int bnx2x_config_vlan_mac(struct bnx2x *bp,
			  struct bnx2x_vlan_mac_ramrod_params *p);

int bnx2x_vlan_mac_move(struct bnx2x *bp,
			struct bnx2x_vlan_mac_ramrod_params *p,
			struct bnx2x_vlan_mac_obj *dest_o);


void bnx2x_init_rx_mode_obj(struct bnx2x *bp,
			    struct bnx2x_rx_mode_obj *o);

int bnx2x_config_rx_mode(struct bnx2x *bp,
			 struct bnx2x_rx_mode_ramrod_params *p);


void bnx2x_init_mcast_obj(struct bnx2x *bp,
			  struct bnx2x_mcast_obj *mcast_obj,
			  u8 mcast_cl_id, u32 mcast_cid, u8 func_id,
			  u8 engine_id, void *rdata, dma_addr_t rdata_mapping,
			  int state, unsigned long *pstate,
			  bnx2x_obj_type type);

int bnx2x_config_mcast(struct bnx2x *bp,
		       struct bnx2x_mcast_ramrod_params *p, int cmd);

void bnx2x_init_mac_credit_pool(struct bnx2x *bp,
				struct bnx2x_credit_pool_obj *p, u8 func_id,
				u8 func_num);
void bnx2x_init_vlan_credit_pool(struct bnx2x *bp,
				 struct bnx2x_credit_pool_obj *p, u8 func_id,
				 u8 func_num);


void bnx2x_init_rss_config_obj(struct bnx2x *bp,
			       struct bnx2x_rss_config_obj *rss_obj,
			       u8 cl_id, u32 cid, u8 func_id, u8 engine_id,
			       void *rdata, dma_addr_t rdata_mapping,
			       int state, unsigned long *pstate,
			       bnx2x_obj_type type);

int bnx2x_config_rss(struct bnx2x *bp,
		     struct bnx2x_config_rss_params *p);

void bnx2x_get_rss_ind_table(struct bnx2x_rss_config_obj *rss_obj,
			     u8 *ind_table);

#endif 
