/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _IPA_I_H_
#define _IPA_I_H_

#include <linux/bitops.h>
#include <linux/cdev.h>
#include <linux/export.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <mach/ipa.h>
#include <mach/sps.h>
#include "ipa_hw_defs.h"
#include "ipa_ram_mmap.h"
#include "ipa_reg.h"

#define DRV_NAME "ipa"
#define IPA_COOKIE 0xfacefeed

#define IPA_NUM_PIPES 0x14
#define IPA_SYS_DESC_FIFO_SZ 0x800
#define IPA_SYS_TX_DATA_DESC_FIFO_SZ 0x1000

#ifdef IPA_DEBUG
#define IPADBG(fmt, args...) \
	pr_err(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)
#else
#define IPADBG(fmt, args...)
#endif

#define WLAN_AMPDU_TX_EP 15
#define WLAN_PROD_TX_EP  19

#define MAX_NUM_EXCP     8
#define MAX_NUM_IMM_CMD 17

#define IPA_STATS

#ifdef IPA_STATS
#define IPA_STATS_INC_CNT(val) do {			\
				++val;			\
			} while (0)
#define IPA_STATS_INC_CNT_SAFE(val) do {		\
				atomic_inc(&val);	\
			} while (0)
#define IPA_STATS_EXCP_CNT(flags, base) do {			\
			int i;					\
			for (i = 0; i < MAX_NUM_EXCP; i++)	\
				if (flags & BIT(i))		\
					++base[i];		\
			} while (0)
#define IPA_STATS_INC_TX_CNT(ep, sw, hw) do {		\
			if (ep == WLAN_AMPDU_TX_EP)	\
				++hw;			\
			else				\
				++sw;			\
			} while (0)
#define IPA_STATS_INC_IC_CNT(num, base, stat_base) do {			\
			int i;						\
			for (i = 0; i < num; i++)			\
				++stat_base[base[i].opcode];		\
			} while (0)
#define IPA_STATS_INC_BRIDGE_CNT(type, dir, base) do {		\
			++base[type][dir];			\
			} while (0)
#else
#define IPA_STATS_INC_CNT(x) do { } while (0)
#define IPA_STATS_INC_CNT_SAFE(x) do { } while (0)
#define IPA_STATS_EXCP_CNT(flags, base) do { } while (0)
#define IPA_STATS_INC_TX_CNT(ep, sw, hw) do { } while (0)
#define IPA_STATS_INC_IC_CNT(num, base, stat_base) do { } while (0)
#define IPA_STATS_INC_BRIDGE_CNT(type, dir, base) do { } while (0)
#endif

#define IPAERR(fmt, args...) \
	pr_err(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)

#define IPA_TOS_EQ			BIT(0)
#define IPA_PROTOCOL_EQ		BIT(1)
#define IPA_OFFSET_MEQ32_0		BIT(2)
#define IPA_OFFSET_MEQ32_1		BIT(3)
#define IPA_IHL_OFFSET_RANGE16_0	BIT(4)
#define IPA_IHL_OFFSET_RANGE16_1	BIT(5)
#define IPA_IHL_OFFSET_EQ_16		BIT(6)
#define IPA_IHL_OFFSET_EQ_32		BIT(7)
#define IPA_IHL_OFFSET_MEQ32_0		BIT(8)
#define IPA_OFFSET_MEQ128_0		BIT(9)
#define IPA_OFFSET_MEQ128_1		BIT(10)
#define IPA_TC_EQ			BIT(11)
#define IPA_FL_EQ			BIT(12)
#define IPA_IHL_OFFSET_MEQ32_1		BIT(13)
#define IPA_METADATA_COMPARE		BIT(14)
#define IPA_IPV4_IS_FRAG		BIT(15)

#define IPA_HDR_BIN0 0
#define IPA_HDR_BIN1 1
#define IPA_HDR_BIN2 2
#define IPA_HDR_BIN3 3
#define IPA_HDR_BIN_MAX 4

#define IPA_EVENT_THRESHOLD 0x10

#define IPA_RX_POOL_CEIL 32
#define IPA_RX_SKB_SIZE 2048

#define IPA_DFLT_HDR_NAME "ipa_excp_hdr"
#define IPA_INVALID_L4_PROTOCOL 0xFF

#define IPA_CLIENT_IS_PROD(x) (x >= IPA_CLIENT_PROD && x < IPA_CLIENT_CONS)
#define IPA_CLIENT_IS_CONS(x) (x >= IPA_CLIENT_CONS && x < IPA_CLIENT_MAX)
#define IPA_SETFIELD(val, shift, mask) (((val) << (shift)) & (mask))

#define IPA_HW_TABLE_ALIGNMENT(start_ofst) \
	(((start_ofst) + 127) & ~127)
#define IPA_RT_FLT_HW_RULE_BUF_SIZE	(128)

enum ipa_sys_pipe {
	IPA_A5_UNUSED,
	IPA_A5_CMD,
	IPA_A5_LAN_WAN_OUT,
	IPA_A5_LAN_WAN_IN,
	IPA_A5_WLAN_AMPDU_OUT,
	IPA_A5_SYS_MAX
};

enum ipa_operating_mode {
	IPA_MODE_USB_DONGLE,
	IPA_MODE_MSM,
	IPA_MODE_EXT_APPS,
	IPA_MODE_MOBILE_AP_WAN,
	IPA_MODE_MOBILE_AP_WLAN,
	IPA_MODE_MOBILE_AP_ETH,
	IPA_MODE_MAX
};

struct ipa_mem_buffer {
	void *base;
	dma_addr_t phys_base;
	u32 size;
};

struct ipa_flt_entry {
	struct list_head link;
	struct ipa_flt_rule rule;
	u32 cookie;
	struct ipa_flt_tbl *tbl;
	struct ipa_rt_tbl *rt_tbl;
	u32 hw_len;
};

struct ipa_rt_tbl {
	struct list_head link;
	struct list_head head_rt_rule_list;
	char name[IPA_RESOURCE_NAME_MAX];
	u32 idx;
	u32 rule_cnt;
	u32 ref_cnt;
	struct ipa_rt_tbl_set *set;
	u32 cookie;
	bool in_sys;
	u32 sz;
	struct ipa_mem_buffer curr_mem;
	struct ipa_mem_buffer prev_mem;
};

struct ipa_hdr_entry {
	struct list_head link;
	u8 hdr[IPA_HDR_MAX_SIZE];
	u32 hdr_len;
	char name[IPA_RESOURCE_NAME_MAX];
	u8 is_partial;
	struct ipa_hdr_offset_entry *offset_entry;
	u32 cookie;
	u32 ref_cnt;
};

struct ipa_hdr_offset_entry {
	struct list_head link;
	u32 offset;
	u32 bin;
};

struct ipa_hdr_tbl {
	struct list_head head_hdr_entry_list;
	struct list_head head_offset_list[IPA_HDR_BIN_MAX];
	struct list_head head_free_offset_list[IPA_HDR_BIN_MAX];
	u32 hdr_cnt;
	u32 end;
};

struct ipa_flt_tbl {
	struct list_head head_flt_rule_list;
	u32 rule_cnt;
	bool in_sys;
	u32 sz;
	struct ipa_mem_buffer curr_mem;
	struct ipa_mem_buffer prev_mem;
};

struct ipa_rt_entry {
	struct list_head link;
	struct ipa_rt_rule rule;
	u32 cookie;
	struct ipa_rt_tbl *tbl;
	struct ipa_hdr_entry *hdr;
	u32 hw_len;
};

struct ipa_rt_tbl_set {
	struct list_head head_rt_tbl_list;
	u32 tbl_cnt;
};

struct ipa_tree_node {
	struct rb_node node;
	u32 hdl;
};

struct ipa_ep_context {
	int valid;
	enum ipa_client_type client;
	struct sps_pipe *ep_hdl;
	struct ipa_ep_cfg cfg;
	struct ipa_ep_cfg_holb holb;
	u32 dst_pipe_index;
	u32 rt_tbl_idx;
	struct sps_connect connect;
	void *priv;
	void (*client_notify)(void *priv, enum ipa_dp_evt_type evt,
		       unsigned long data);
	bool desc_fifo_in_pipe_mem;
	bool data_fifo_in_pipe_mem;
	u32 desc_fifo_pipe_mem_ofst;
	u32 data_fifo_pipe_mem_ofst;
	bool desc_fifo_client_allocated;
	bool data_fifo_client_allocated;
	bool suspended;
};

struct ipa_sys_context {
	struct list_head head_desc_list;
	u32 len;
	spinlock_t spinlock;
	struct sps_register_event event;
	struct ipa_ep_context *ep;
	atomic_t curr_polling_state;
	struct delayed_work switch_to_intr_work;
};

enum ipa_desc_type {
	IPA_DATA_DESC,
	IPA_DATA_DESC_SKB,
	IPA_IMM_CMD_DESC
};

struct ipa_tx_pkt_wrapper {
	enum ipa_desc_type type;
	struct ipa_mem_buffer mem;
	struct work_struct work;
	struct list_head link;
	void (*callback)(void *user1, void *user2);
	void *user1;
	void *user2;
	struct ipa_sys_context *sys;
	struct ipa_mem_buffer mult;
	u32 cnt;
	void *bounce;
};

struct ipa_desc {
	enum ipa_desc_type type;
	void *pyld;
	u16 len;
	u16 opcode;
	void (*callback)(void *user1, void *user2);
	void *user1;
	void *user2;
	struct completion xfer_done;
};

struct ipa_rx_pkt_wrapper {
	struct sk_buff *skb;
	dma_addr_t dma_address;
	struct list_head link;
	u32 len;
};

struct ipa_nat_mem {
	struct class *class;
	struct device *dev;
	struct cdev cdev;
	dev_t dev_num;
	void *vaddr;
	dma_addr_t dma_handle;
	size_t size;
	bool is_mapped;
	bool is_sys_mem;
	bool is_dev_init;
	struct mutex lock;
	void *nat_base_address;
	char *ipv4_rules_addr;
	char *ipv4_expansion_rules_addr;
	char *index_table_addr;
	char *index_table_expansion_addr;
	u32 size_base_tables;
	u32 size_expansion_tables;
	u32 public_ip_addr;
};

enum ipa_hw_type {
	IPA_HW_None = 0,
	IPA_HW_v1_0 = 1,
	IPA_HW_v1_1 = 2,
	IPA_HW_v2_0 = 3
};

enum ipa_hw_mode {
	IPA_HW_MODE_NORMAL  = 0,
	IPA_HW_MODE_VIRTUAL = 1,
	IPA_HW_MODE_PCIE    = 2
};


struct ipa_stats {
	u32 imm_cmds[MAX_NUM_IMM_CMD];
	u32 tx_sw_pkts;
	u32 tx_hw_pkts;
	u32 rx_pkts;
	u32 rx_excp_pkts[MAX_NUM_EXCP];
	u32 bridged_pkts[IPA_BRIDGE_TYPE_MAX][IPA_BRIDGE_DIR_MAX];
	u32 rx_repl_repost;
	u32 x_intr_repost;
	u32 x_intr_repost_tx;
	u32 rx_q_len;
	u32 msg_w[IPA_EVENT_MAX];
	u32 msg_r[IPA_EVENT_MAX];
	u32 a2_power_on_reqs_in;
	u32 a2_power_on_reqs_out;
	u32 a2_power_off_reqs_in;
	u32 a2_power_off_reqs_out;
	u32 a2_power_modem_acks;
	u32 a2_power_apps_acks;
};

struct ipa_context {
	struct class *class;
	dev_t dev_num;
	struct device *dev;
	struct cdev cdev;
	u32 bam_handle;
	struct ipa_ep_context ep[IPA_NUM_PIPES];
	struct ipa_flt_tbl flt_tbl[IPA_NUM_PIPES][IPA_IP_MAX];
	enum ipa_operating_mode mode;
	void __iomem *mmio;
	u32 ipa_wrapper_base;
	struct ipa_flt_tbl glob_flt_tbl[IPA_IP_MAX];
	struct ipa_hdr_tbl hdr_tbl;
	struct ipa_rt_tbl_set rt_tbl_set[IPA_IP_MAX];
	struct ipa_rt_tbl_set reap_rt_tbl_set[IPA_IP_MAX];
	struct kmem_cache *flt_rule_cache;
	struct kmem_cache *rt_rule_cache;
	struct kmem_cache *hdr_cache;
	struct kmem_cache *hdr_offset_cache;
	struct kmem_cache *rt_tbl_cache;
	struct kmem_cache *tx_pkt_wrapper_cache;
	struct kmem_cache *rx_pkt_wrapper_cache;
	struct kmem_cache *tree_node_cache;
	unsigned long rt_idx_bitmap[IPA_IP_MAX];
	struct mutex lock;
	struct ipa_sys_context sys[IPA_A5_SYS_MAX];
	struct workqueue_struct *rx_wq;
	struct workqueue_struct *tx_wq;
	u16 smem_sz;
	struct rb_root hdr_hdl_tree;
	struct rb_root rt_rule_hdl_tree;
	struct rb_root rt_tbl_hdl_tree;
	struct rb_root flt_rule_hdl_tree;
	struct ipa_nat_mem nat_mem;
	u32 excp_hdr_hdl;
	u32 dflt_v4_rt_rule_hdl;
	u32 dflt_v6_rt_rule_hdl;
	bool polling_mode;
	uint aggregation_type;
	uint aggregation_byte_limit;
	uint aggregation_time_limit;
	struct delayed_work poll_work;
	bool hdr_tbl_lcl;
	struct ipa_mem_buffer hdr_mem;
	bool ip4_rt_tbl_lcl;
	bool ip6_rt_tbl_lcl;
	bool ip4_flt_tbl_lcl;
	bool ip6_flt_tbl_lcl;
	struct ipa_mem_buffer empty_rt_tbl_mem;
	struct gen_pool *pipe_mem_pool;
	struct dma_pool *dma_pool;
	struct mutex ipa_active_clients_lock;
	int ipa_active_clients;
	u32 clnt_hdl_cmd;
	u32 clnt_hdl_data_in;
	u32 clnt_hdl_data_out;
	u8 a5_pipe_index;
	struct list_head intf_list;
	struct list_head msg_list;
	struct list_head pull_msg_list;
	struct mutex msg_lock;
	wait_queue_head_t msg_waitq;
	enum ipa_hw_type ipa_hw_type;
	enum ipa_hw_mode ipa_hw_mode;
	
	struct ipa_stats stats;
	void *smem_pipe_mem;
};

struct ipa_route {
	u32 route_dis;
	u32 route_def_pipe;
	u32 route_def_hdr_table;
	u32 route_def_hdr_ofst;
};

enum ipa_pipe_mem_type {
	IPA_SPS_PIPE_MEM = 0,
	IPA_PRIVATE_MEM  = 1,
	IPA_SYSTEM_MEM   = 2,
};

enum a2_mux_pipe_direction {
	A2_TO_IPA = 0,
	IPA_TO_A2 = 1
};

struct a2_mux_pipe_connection {
	int			src_phy_addr;
	int			src_pipe_index;
	int			dst_phy_addr;
	int			dst_pipe_index;
	enum ipa_pipe_mem_type	mem_type;
	int			data_fifo_base_offset;
	int			data_fifo_size;
	int			desc_fifo_base_offset;
	int			desc_fifo_size;
};

struct ipa_plat_drv_res {
	u32 ipa_mem_base;
	u32 ipa_mem_size;
	u32 bam_mem_base;
	u32 bam_mem_size;
	u32 a2_bam_mem_base;
	u32 a2_bam_mem_size;
	u32 ipa_irq;
	u32 bam_irq;
	u32 a2_bam_irq;
	u32 ipa_pipe_mem_start_ofst;
	u32 ipa_pipe_mem_size;
	enum ipa_hw_type ipa_hw_type;
	enum ipa_hw_mode ipa_hw_mode;
	struct a2_mux_pipe_connection a2_to_ipa_pipe;
	struct a2_mux_pipe_connection ipa_to_a2_pipe;
};

extern struct ipa_context *ipa_ctx;

int ipa_get_a2_mux_pipe_info(enum a2_mux_pipe_direction pipe_dir,
				struct a2_mux_pipe_connection *pipe_connect);
int ipa_get_a2_mux_bam_info(u32 *a2_bam_mem_base, u32 *a2_bam_mem_size,
			    u32 *a2_bam_irq);
void teth_bridge_get_client_handles(u32 *producer_handle,
		u32 *consumer_handle);
int ipa_send_one(struct ipa_sys_context *sys, struct ipa_desc *desc,
		bool in_atomic);
int ipa_send(struct ipa_sys_context *sys, u32 num_desc, struct ipa_desc *desc,
		bool in_atomic);
int ipa_get_ep_mapping(enum ipa_operating_mode mode,
		       enum ipa_client_type client);
int ipa_get_client_mapping(enum ipa_operating_mode mode, int pipe_idx);
int ipa_generate_hw_rule(enum ipa_ip_type ip,
			 const struct ipa_rule_attrib *attrib,
			 u8 **buf,
			 u16 *en_rule);
u8 *ipa_write_32(u32 w, u8 *dest);
u8 *ipa_write_16(u16 hw, u8 *dest);
u8 *ipa_write_8(u8 b, u8 *dest);
u8 *ipa_pad_to_32(u8 *dest);
int ipa_init_hw(void);
struct ipa_rt_tbl *__ipa_find_rt_tbl(enum ipa_ip_type ip, const char *name);
void ipa_dump(void);
int ipa_generate_hdr_hw_tbl(struct ipa_mem_buffer *mem);
int ipa_generate_rt_hw_tbl(enum ipa_ip_type ip, struct ipa_mem_buffer *mem);
int ipa_generate_flt_hw_tbl(enum ipa_ip_type ip, struct ipa_mem_buffer *mem);
int ipa_set_single_ndp_per_mbim(bool);
int ipa_set_hw_timer_fix_for_mbim_aggr(bool);
void ipa_debugfs_init(void);
void ipa_debugfs_remove(void);

int ipa_insert(struct rb_root *root, struct ipa_tree_node *data);
struct ipa_tree_node *ipa_search(struct rb_root *root, u32 hdl);
void ipa_dump_buff_internal(void *base, dma_addr_t phy_base, u32 size);

#ifdef IPA_DEBUG
#define IPA_DUMP_BUFF(base, phy_base, size) \
	ipa_dump_buff_internal(base, phy_base, size)
#else
#define IPA_DUMP_BUFF(base, phy_base, size)
#endif

int ipa_cfg_route(struct ipa_route *route);
int ipa_send_cmd(u16 num_desc, struct ipa_desc *descr);
void ipa_replenish_rx_cache(void);
void ipa_cleanup_rx(void);
int ipa_cfg_filter(u32 disable);
void ipa_wq_write_done(struct work_struct *work);
int ipa_handle_rx_core(struct ipa_sys_context *sys, bool process_all,
		bool in_poll_state);
int ipa_handle_tx_core(struct ipa_sys_context *sys, bool process_all,
		bool in_poll_state);
int ipa_pipe_mem_init(u32 start_ofst, u32 size);
int ipa_pipe_mem_alloc(u32 *ofst, u32 size);
int ipa_pipe_mem_free(u32 ofst, u32 size);
int ipa_straddle_boundary(u32 start, u32 end, u32 boundary);
struct ipa_context *ipa_get_ctx(void);
void ipa_enable_clks(void);
void ipa_disable_clks(void);
void ipa_inc_client_enable_clks(void);
void ipa_dec_client_disable_clks(void);
int __ipa_del_rt_rule(u32 rule_hdl);
int __ipa_del_hdr(u32 hdr_hdl);
int __ipa_release_hdr(u32 hdr_hdl);

static inline u32 ipa_read_reg(void *base, u32 offset)
{
	u32 val = ioread32(base + offset);
	IPADBG("0x%x(va) read reg 0x%x r_val 0x%x.\n",
		(u32)base, offset, val);
	return val;
}

static inline void ipa_write_reg(void *base, u32 offset, u32 val)
{
	iowrite32(val, base + offset);
	IPADBG("0x%x(va) write reg 0x%x w_val 0x%x.\n",
		(u32)base, offset, val);
}

int ipa_bridge_init(void);
void ipa_bridge_cleanup(void);

ssize_t ipa_read(struct file *filp, char __user *buf, size_t count,
		 loff_t *f_pos);
int ipa_pull_msg(struct ipa_msg_meta *meta, char *buff, size_t count);
int ipa_query_intf(struct ipa_ioc_query_intf *lookup);
int ipa_query_intf_tx_props(struct ipa_ioc_query_intf_tx_props *tx);
int ipa_query_intf_rx_props(struct ipa_ioc_query_intf_rx_props *rx);

int a2_mux_init(void);
int a2_mux_exit(void);

void wwan_cleanup(void);

int teth_bridge_driver_init(void);

#endif 
