/*
 * Copyright (c) 2004-2011 Atheros Communications Inc.
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef HTC_H
#define HTC_H

#include "common.h"


#define HTC_FLAGS_NEED_CREDIT_UPDATE (1 << 0)
#define HTC_FLAGS_SEND_BUNDLE        (1 << 1)

#define HTC_FLG_RX_UNUSED        (1 << 0)
#define HTC_FLG_RX_TRAILER       (1 << 1)
#define HTC_FLG_RX_BNDL_CNT	 (0xF0)
#define HTC_FLG_RX_BNDL_CNT_S	 4

#define HTC_HDR_LENGTH  (sizeof(struct htc_frame_hdr))
#define HTC_MAX_PAYLOAD_LENGTH   (4096 - sizeof(struct htc_frame_hdr))


#define HTC_MSG_READY_ID		1
#define HTC_MSG_CONN_SVC_ID		2
#define HTC_MSG_CONN_SVC_RESP_ID	3
#define HTC_MSG_SETUP_COMPLETE_ID	4
#define HTC_MSG_SETUP_COMPLETE_EX_ID	5

#define HTC_MAX_CTRL_MSG_LEN  256

#define HTC_VERSION_2P0  0x00
#define HTC_VERSION_2P1  0x01

#define HTC_SERVICE_META_DATA_MAX_LENGTH 128

#define HTC_CONN_FLGS_THRESH_LVL_QUAT		0x0
#define HTC_CONN_FLGS_THRESH_LVL_HALF		0x1
#define HTC_CONN_FLGS_THRESH_LVL_THREE_QUAT	0x2
#define HTC_CONN_FLGS_REDUCE_CRED_DRIB		0x4
#define HTC_CONN_FLGS_THRESH_MASK		0x3

#define HTC_SERVICE_SUCCESS      0
#define HTC_SERVICE_NOT_FOUND    1
#define HTC_SERVICE_FAILED       2

#define HTC_SERVICE_NO_RESOURCES 3

#define HTC_SERVICE_NO_MORE_EP   4

#define HTC_RECORD_NULL             0
#define HTC_RECORD_CREDITS          1
#define HTC_RECORD_LOOKAHEAD        2
#define HTC_RECORD_LOOKAHEAD_BUNDLE 3

#define HTC_SETUP_COMP_FLG_RX_BNDL_EN     (1 << 0)

#define MAKE_SERVICE_ID(group, index) \
	(int)(((int)group << 8) | (int)(index))

#define HTC_CTRL_RSVD_SVC MAKE_SERVICE_ID(RSVD_SERVICE_GROUP, 1)
#define WMI_CONTROL_SVC   MAKE_SERVICE_ID(WMI_SERVICE_GROUP, 0)
#define WMI_DATA_BE_SVC   MAKE_SERVICE_ID(WMI_SERVICE_GROUP, 1)
#define WMI_DATA_BK_SVC   MAKE_SERVICE_ID(WMI_SERVICE_GROUP, 2)
#define WMI_DATA_VI_SVC   MAKE_SERVICE_ID(WMI_SERVICE_GROUP, 3)
#define WMI_DATA_VO_SVC   MAKE_SERVICE_ID(WMI_SERVICE_GROUP, 4)
#define WMI_MAX_SERVICES  5

#define WMM_NUM_AC  4

#define HTC_TX_PACKET_TAG_ALL          0
#define HTC_SERVICE_TX_PACKET_TAG      1
#define HTC_TX_PACKET_TAG_USER_DEFINED (HTC_SERVICE_TX_PACKET_TAG + 9)

#define HTC_RX_FLAGS_INDICATE_MORE_PKTS  (1 << 0)

#define ENDPOINT1 0
#define HTC_MAILBOX_NUM_MAX    4

#define HTC_FLGS_TX_BNDL_PAD_EN	 (1 << 0)
#define HTC_EP_ACTIVE                            ((u32) (1u << 31))

#define HTC_TARGET_RESPONSE_TIMEOUT        2000	
#define HTC_TARGET_DEBUG_INTR_MASK         0x01
#define HTC_TARGET_CREDIT_INTR_MASK        0xF0

#define HTC_HOST_MAX_MSG_PER_BUNDLE        8
#define HTC_MIN_HTC_MSGS_TO_BUNDLE         2


#define HTC_RX_PKT_IGNORE_LOOKAHEAD      (1 << 0)
#define HTC_RX_PKT_REFRESH_HDR           (1 << 1)
#define HTC_RX_PKT_PART_OF_BUNDLE        (1 << 2)
#define HTC_RX_PKT_NO_RECYCLE            (1 << 3)

#define NUM_CONTROL_BUFFERS     8
#define NUM_CONTROL_TX_BUFFERS  2
#define NUM_CONTROL_RX_BUFFERS  (NUM_CONTROL_BUFFERS - NUM_CONTROL_TX_BUFFERS)

#define HTC_RECV_WAIT_BUFFERS        (1 << 0)
#define HTC_OP_STATE_STOPPING        (1 << 0)


struct htc_frame_hdr {
	u8 eid;
	u8 flags;

	
	__le16 payld_len;

	

	u8 ctrl[2];
} __packed;

struct htc_ready_msg {
	__le16 msg_id;
	__le16 cred_cnt;
	__le16 cred_sz;
	u8 max_ep;
	u8 pad;
} __packed;

struct htc_ready_ext_msg {
	struct htc_ready_msg ver2_0_info;
	u8 htc_ver;
	u8 msg_per_htc_bndl;
} __packed;

struct htc_conn_service_msg {
	__le16 msg_id;
	__le16 svc_id;
	__le16 conn_flags;
	u8 svc_meta_len;
	u8 pad;
} __packed;

struct htc_conn_service_resp {
	__le16 msg_id;
	__le16 svc_id;
	u8 status;
	u8 eid;
	__le16 max_msg_sz;
	u8 svc_meta_len;
	u8 pad;
} __packed;

struct htc_setup_comp_msg {
	__le16 msg_id;
} __packed;

struct htc_setup_comp_ext_msg {
	__le16 msg_id;
	__le32 flags;
	u8 msg_per_rxbndl;
	u8 Rsvd[3];
} __packed;

struct htc_record_hdr {
	u8 rec_id;
	u8 len;
} __packed;

struct htc_credit_report {
	u8 eid;
	u8 credits;
} __packed;

struct htc_lookahead_report {
	u8 pre_valid;
	u8 lk_ahd[4];
	u8 post_valid;
} __packed;

struct htc_bundle_lkahd_rpt {
	u8 lk_ahd[4];
} __packed;


enum htc_service_grp_ids {
	RSVD_SERVICE_GROUP = 0,
	WMI_SERVICE_GROUP = 1,

	HTC_TEST_GROUP = 254,
	HTC_SERVICE_GROUP_LAST = 255
};


enum htc_endpoint_id {
	ENDPOINT_UNUSED = -1,
	ENDPOINT_0 = 0,
	ENDPOINT_1 = 1,
	ENDPOINT_2 = 2,
	ENDPOINT_3,
	ENDPOINT_4,
	ENDPOINT_5,
	ENDPOINT_6,
	ENDPOINT_7,
	ENDPOINT_8,
	ENDPOINT_MAX,
};

struct htc_tx_packet_info {
	u16 tag;
	int cred_used;
	u8 flags;
	int seqno;
};

struct htc_rx_packet_info {
	u32 exp_hdr;
	u32 rx_flags;
	u32 indicat_flags;
};

struct htc_target;

struct htc_packet {
	struct list_head list;

	
	void *pkt_cntxt;

	u8 *buf_start;

	u8 *buf;
	u32 buf_len;

	
	u32 act_len;

	
	enum htc_endpoint_id endpoint;

	

	int status;
	union {
		struct htc_tx_packet_info tx;
		struct htc_rx_packet_info rx;
	} info;

	void (*completion) (struct htc_target *, struct htc_packet *);
	struct htc_target *context;
};

enum htc_send_full_action {
	HTC_SEND_FULL_KEEP = 0,
	HTC_SEND_FULL_DROP = 1,
};

struct htc_ep_callbacks {
	void (*rx) (struct htc_target *, struct htc_packet *);
	void (*rx_refill) (struct htc_target *, enum htc_endpoint_id endpoint);
	enum htc_send_full_action (*tx_full) (struct htc_target *,
					      struct htc_packet *);
	struct htc_packet *(*rx_allocthresh) (struct htc_target *,
					      enum htc_endpoint_id, int);
	int rx_alloc_thresh;
	int rx_refill_thresh;
};

struct htc_service_connect_req {
	u16 svc_id;
	u16 conn_flags;
	struct htc_ep_callbacks ep_cb;
	int max_txq_depth;
	u32 flags;
	unsigned int max_rxmsg_sz;
};

struct htc_service_connect_resp {
	u8 buf_len;
	u8 act_len;
	enum htc_endpoint_id endpoint;
	unsigned int len_max;
	u8 resp_code;
};

struct htc_endpoint_credit_dist {
	struct list_head list;

	
	u16 svc_id;

	
	enum htc_endpoint_id endpoint;

	u32 dist_flags;

	int cred_norm;

	
	int cred_min;

	int cred_assngd;

	
	int credits;

	int cred_to_dist;

	int seek_cred;

	
	int cred_sz;

	
	int cred_per_msg;

	
	struct htc_endpoint *htc_ep;

	int txq_depth;
};

enum htc_credit_dist_reason {
	HTC_CREDIT_DIST_SEND_COMPLETE = 0,
	HTC_CREDIT_DIST_ACTIVITY_CHANGE = 1,
	HTC_CREDIT_DIST_SEEK_CREDITS,
};

struct ath6kl_htc_credit_info {
	int total_avail_credits;
	int cur_free_credits;

	
	struct list_head lowestpri_ep_dist;
};

struct htc_endpoint_stats {
	u32 cred_low_indicate;

	u32 tx_issued;
	u32 tx_pkt_bundled;
	u32 tx_bundles;
	u32 tx_dropped;

	
	u32 tx_cred_rpt;

	
	u32 cred_rpt_from_rx;

	
	u32 cred_rpt_from_other;

	
	u32 cred_rpt_ep0;

	
	u32 cred_from_rx;

	
	u32 cred_from_other;

	
	u32 cred_from_ep0;

	
	u32 cred_cosumd;

	
	u32 cred_retnd;

	u32 rx_pkts;

	
	u32 rx_lkahds;

	
	u32 rx_bundl;

	
	u32 rx_bundle_lkahd;

	
	u32 rx_bundle_from_hdr;

	
	u32 rx_alloc_thresh_hit;

	
	u32 rxalloc_thresh_byte;
};

struct htc_endpoint {
	enum htc_endpoint_id eid;
	u16 svc_id;
	struct list_head txq;
	struct list_head rx_bufq;
	struct htc_endpoint_credit_dist cred_dist;
	struct htc_ep_callbacks ep_cb;
	int max_txq_depth;
	int len_max;
	int tx_proc_cnt;
	int rx_proc_cnt;
	struct htc_target *target;
	u8 seqno;
	u32 conn_flags;
	struct htc_endpoint_stats ep_st;
	u16 tx_drop_packet_threshold;
};

struct htc_control_buffer {
	struct htc_packet packet;
	u8 *buf;
};

struct ath6kl_device;

struct htc_target {
	struct htc_endpoint endpoint[ENDPOINT_MAX];

	
	struct list_head cred_dist_list;

	struct list_head free_ctrl_txbuf;
	struct list_head free_ctrl_rxbuf;
	struct ath6kl_htc_credit_info *credit_info;
	int tgt_creds;
	unsigned int tgt_cred_sz;

	
	spinlock_t htc_lock;

	
	spinlock_t rx_lock;

	
	spinlock_t tx_lock;

	struct ath6kl_device *dev;
	u32 htc_flags;
	u32 rx_st_flags;
	enum htc_endpoint_id ep_waiting;
	u8 htc_tgt_ver;

	
	int msg_per_bndl_max;

	u32 tx_bndl_mask;
	int rx_bndl_enable;
	int max_rx_bndl_sz;
	int max_tx_bndl_sz;

	u32 block_sz;
	u32 block_mask;

	int max_scat_entries;
	int max_xfer_szper_scatreq;

	int chk_irq_status_cnt;

	
	u32 ac_tx_count[WMM_NUM_AC];
};

void *ath6kl_htc_create(struct ath6kl *ar);
void ath6kl_htc_set_credit_dist(struct htc_target *target,
				struct ath6kl_htc_credit_info *cred_info,
				u16 svc_pri_order[], int len);
int ath6kl_htc_wait_target(struct htc_target *target);
int ath6kl_htc_start(struct htc_target *target);
int ath6kl_htc_conn_service(struct htc_target *target,
			    struct htc_service_connect_req *req,
			    struct htc_service_connect_resp *resp);
int ath6kl_htc_tx(struct htc_target *target, struct htc_packet *packet);
void ath6kl_htc_stop(struct htc_target *target);
void ath6kl_htc_cleanup(struct htc_target *target);
void ath6kl_htc_flush_txep(struct htc_target *target,
			   enum htc_endpoint_id endpoint, u16 tag);
void ath6kl_htc_flush_rx_buf(struct htc_target *target);
void ath6kl_htc_indicate_activity_change(struct htc_target *target,
					 enum htc_endpoint_id endpoint,
					 bool active);
int ath6kl_htc_get_rxbuf_num(struct htc_target *target,
			     enum htc_endpoint_id endpoint);
int ath6kl_htc_add_rxbuf_multiple(struct htc_target *target,
				  struct list_head *pktq);
int ath6kl_htc_rxmsg_pending_handler(struct htc_target *target,
				     u32 msg_look_ahead, int *n_pkts);

int ath6kl_credit_setup(void *htc_handle,
			struct ath6kl_htc_credit_info *cred_info);

static inline void set_htc_pkt_info(struct htc_packet *packet, void *context,
				    u8 *buf, unsigned int len,
				    enum htc_endpoint_id eid, u16 tag)
{
	packet->pkt_cntxt = context;
	packet->buf = buf;
	packet->act_len = len;
	packet->endpoint = eid;
	packet->info.tx.tag = tag;
}

static inline void htc_rxpkt_reset(struct htc_packet *packet)
{
	packet->buf = packet->buf_start;
	packet->act_len = 0;
}

static inline void set_htc_rxpkt_info(struct htc_packet *packet, void *context,
				      u8 *buf, unsigned long len,
				      enum htc_endpoint_id eid)
{
	packet->pkt_cntxt = context;
	packet->buf = buf;
	packet->buf_start = buf;
	packet->buf_len = len;
	packet->endpoint = eid;
}

static inline int get_queue_depth(struct list_head *queue)
{
	struct list_head *tmp_list;
	int depth = 0;

	list_for_each(tmp_list, queue)
	    depth++;

	return depth;
}

#endif
