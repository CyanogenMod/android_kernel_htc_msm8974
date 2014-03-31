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

#ifndef _IPA_H_
#define _IPA_H_

#include <linux/msm_ipa.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <mach/sps.h>

enum ipa_nat_en_type {
	IPA_BYPASS_NAT,
	IPA_SRC_NAT,
	IPA_DST_NAT,
};

enum ipa_mode_type {
	IPA_BASIC,
	IPA_ENABLE_FRAMING_HDLC,
	IPA_ENABLE_DEFRAMING_HDLC,
	IPA_DMA,
};

enum ipa_aggr_en_type {
	IPA_BYPASS_AGGR,
	IPA_ENABLE_AGGR,
	IPA_ENABLE_DEAGGR,
};

enum ipa_aggr_type {
	IPA_MBIM_16,
	IPA_MBIM_32,
	IPA_TLP,
};

enum ipa_aggr_mode {
	IPA_MBIM,
	IPA_QCNCM,
};

enum ipa_dp_evt_type {
	IPA_RECEIVE,
	IPA_WRITE_DONE,
};

struct ipa_ep_cfg_nat {
	enum ipa_nat_en_type nat_en;
};

struct ipa_ep_cfg_hdr {
	u32 hdr_len;
	u32 hdr_ofst_metadata_valid;
	u32 hdr_ofst_metadata;
	u32 hdr_additional_const_len;
	u32 hdr_ofst_pkt_size_valid;
	u32 hdr_ofst_pkt_size;
	u32 hdr_a5_mux;
};

struct ipa_ep_cfg_mode {
	enum ipa_mode_type mode;
	enum ipa_client_type dst;
};

struct ipa_ep_cfg_aggr {
	enum ipa_aggr_en_type aggr_en;
	enum ipa_aggr_type aggr;
	u32 aggr_byte_limit;
	u32 aggr_time_limit;
};

struct ipa_ep_cfg_route {
	u32 rt_tbl_hdl;
};

struct ipa_ep_cfg_holb {
	u16 en;
	u16 tmr_val;
};

struct ipa_ep_cfg {
	struct ipa_ep_cfg_nat nat;
	struct ipa_ep_cfg_hdr hdr;
	struct ipa_ep_cfg_mode mode;
	struct ipa_ep_cfg_aggr aggr;
	struct ipa_ep_cfg_route route;
};

typedef void (*ipa_notify_cb)(void *priv, enum ipa_dp_evt_type evt,
		       unsigned long data);

struct ipa_connect_params {
	struct ipa_ep_cfg ipa_ep_cfg;
	enum ipa_client_type client;
	u32 client_bam_hdl;
	u32 client_ep_idx;
	void *priv;
	ipa_notify_cb notify;
	u32 desc_fifo_sz;
	u32 data_fifo_sz;
	bool pipe_mem_preferred;
	struct sps_mem_buffer desc;
	struct sps_mem_buffer data;
};

struct ipa_sps_params {
	u32 ipa_bam_hdl;
	u32 ipa_ep_idx;
	struct sps_mem_buffer desc;
	struct sps_mem_buffer data;
};

struct ipa_tx_intf {
	u32 num_props;
	struct ipa_ioc_tx_intf_prop *prop;
};

struct ipa_rx_intf {
	u32 num_props;
	struct ipa_ioc_rx_intf_prop *prop;
};

struct ipa_sys_connect_params {
	struct ipa_ep_cfg ipa_ep_cfg;
	enum ipa_client_type client;
	u32 desc_fifo_sz;
	void *priv;
	ipa_notify_cb notify;
};

struct ipa_tx_meta {
	u8 mbim_stream_id;
	bool mbim_stream_id_valid;
};

typedef void (*ipa_msg_free_fn)(void *buff, u32 len, u32 type);

typedef int (*ipa_msg_pull_fn)(void *buff, u32 len, u32 type);

enum ipa_bridge_dir {
	IPA_BRIDGE_DIR_DL,
	IPA_BRIDGE_DIR_UL,
	IPA_BRIDGE_DIR_MAX
};

enum ipa_bridge_type {
	IPA_BRIDGE_TYPE_TETHERED,
	IPA_BRIDGE_TYPE_EMBEDDED,
	IPA_BRIDGE_TYPE_MAX
};

enum ipa_rm_event {
	IPA_RM_RESOURCE_GRANTED,
	IPA_RM_RESOURCE_RELEASED
};

typedef void (*ipa_rm_notify_cb)(void *user_data,
		enum ipa_rm_event event,
		unsigned long data);
struct ipa_rm_register_params {
	void *user_data;
	ipa_rm_notify_cb notify_cb;
};

struct ipa_rm_create_params {
	enum ipa_rm_resource_name name;
	struct ipa_rm_register_params reg_params;
	int (*request_resource)(void);
	int (*release_resource)(void);
};

#define A2_MUX_HDR_NAME_V4_PREF "dmux_hdr_v4_"
#define A2_MUX_HDR_NAME_V6_PREF "dmux_hdr_v6_"

enum a2_mux_event_type {
	A2_MUX_RECEIVE,
	A2_MUX_WRITE_DONE
};

enum a2_mux_logical_channel_id {
	A2_MUX_WWAN_0,
	A2_MUX_WWAN_1,
	A2_MUX_WWAN_2,
	A2_MUX_WWAN_3,
	A2_MUX_WWAN_4,
	A2_MUX_WWAN_5,
	A2_MUX_WWAN_6,
	A2_MUX_WWAN_7,
	A2_MUX_TETHERED_0,
	A2_MUX_NUM_CHANNELS
};

typedef void (*a2_mux_notify_cb)(void *user_data,
		enum a2_mux_event_type event,
		unsigned long data);

enum teth_tethering_mode {
	TETH_TETHERING_MODE_RMNET,
	TETH_TETHERING_MODE_MBIM,
	TETH_TETHERING_MODE_MAX,
};

struct teth_bridge_connect_params {
	u32 ipa_usb_pipe_hdl;
	u32 usb_ipa_pipe_hdl;
	enum teth_tethering_mode tethering_mode;
};

#ifdef CONFIG_IPA

int ipa_connect(const struct ipa_connect_params *in, struct ipa_sps_params *sps,
		u32 *clnt_hdl);
int ipa_disconnect(u32 clnt_hdl);

int ipa_resume(u32 clnt_hdl);

int ipa_suspend(u32 clnt_hdl);

int ipa_cfg_ep(u32 clnt_hdl, const struct ipa_ep_cfg *ipa_ep_cfg);

int ipa_cfg_ep_nat(u32 clnt_hdl, const struct ipa_ep_cfg_nat *ipa_ep_cfg);

int ipa_cfg_ep_hdr(u32 clnt_hdl, const struct ipa_ep_cfg_hdr *ipa_ep_cfg);

int ipa_cfg_ep_mode(u32 clnt_hdl, const struct ipa_ep_cfg_mode *ipa_ep_cfg);

int ipa_cfg_ep_aggr(u32 clnt_hdl, const struct ipa_ep_cfg_aggr *ipa_ep_cfg);

int ipa_cfg_ep_route(u32 clnt_hdl, const struct ipa_ep_cfg_route *ipa_ep_cfg);

int ipa_cfg_ep_holb(u32 clnt_hdl, const struct ipa_ep_cfg_holb *ipa_ep_cfg);

int ipa_cfg_ep_holb_by_client(enum ipa_client_type client,
				const struct ipa_ep_cfg_holb *ipa_ep_cfg);

int ipa_add_hdr(struct ipa_ioc_add_hdr *hdrs);

int ipa_del_hdr(struct ipa_ioc_del_hdr *hdls);

int ipa_commit_hdr(void);

int ipa_reset_hdr(void);

int ipa_get_hdr(struct ipa_ioc_get_hdr *lookup);

int ipa_put_hdr(u32 hdr_hdl);

int ipa_copy_hdr(struct ipa_ioc_copy_hdr *copy);

int ipa_add_rt_rule(struct ipa_ioc_add_rt_rule *rules);

int ipa_del_rt_rule(struct ipa_ioc_del_rt_rule *hdls);

int ipa_commit_rt(enum ipa_ip_type ip);

int ipa_reset_rt(enum ipa_ip_type ip);

int ipa_get_rt_tbl(struct ipa_ioc_get_rt_tbl *lookup);

int ipa_put_rt_tbl(u32 rt_tbl_hdl);

int ipa_add_flt_rule(struct ipa_ioc_add_flt_rule *rules);

int ipa_del_flt_rule(struct ipa_ioc_del_flt_rule *hdls);

int ipa_commit_flt(enum ipa_ip_type ip);

int ipa_reset_flt(enum ipa_ip_type ip);

int allocate_nat_device(struct ipa_ioc_nat_alloc_mem *mem);

int ipa_nat_init_cmd(struct ipa_ioc_v4_nat_init *init);

int ipa_nat_dma_cmd(struct ipa_ioc_nat_dma_cmd *dma);

int ipa_nat_del_cmd(struct ipa_ioc_v4_nat_del *del);

int ipa_send_msg(struct ipa_msg_meta *meta, void *buff,
		  ipa_msg_free_fn callback);
int ipa_register_pull_msg(struct ipa_msg_meta *meta, ipa_msg_pull_fn callback);
int ipa_deregister_pull_msg(struct ipa_msg_meta *meta);

int ipa_register_intf(const char *name, const struct ipa_tx_intf *tx,
		       const struct ipa_rx_intf *rx);
int ipa_deregister_intf(const char *name);

int ipa_set_aggr_mode(enum ipa_aggr_mode mode);

int ipa_set_qcncm_ndp_sig(char sig[3]);

int ipa_set_single_ndp_per_mbim(bool enable);

int ipa_bridge_setup(enum ipa_bridge_dir dir, enum ipa_bridge_type type,
		     struct ipa_sys_connect_params *sys_in, u32 *clnt_hdl);
int ipa_bridge_teardown(enum ipa_bridge_dir dir, enum ipa_bridge_type type,
			u32 clnt_hdl);


int ipa_tx_dp(enum ipa_client_type dst, struct sk_buff *skb,
		struct ipa_tx_meta *metadata);

int ipa_setup_sys_pipe(struct ipa_sys_connect_params *sys_in, u32 *clnt_hdl);

int ipa_teardown_sys_pipe(u32 clnt_hdl);

int ipa_rm_create_resource(struct ipa_rm_create_params *create_params);

int ipa_rm_delete_resource(enum ipa_rm_resource_name resource_name);

int ipa_rm_register(enum ipa_rm_resource_name resource_name,
			struct ipa_rm_register_params *reg_params);

int ipa_rm_deregister(enum ipa_rm_resource_name resource_name,
			struct ipa_rm_register_params *reg_params);

int ipa_rm_add_dependency(enum ipa_rm_resource_name resource_name,
			enum ipa_rm_resource_name depends_on_name);

int ipa_rm_delete_dependency(enum ipa_rm_resource_name resource_name,
			enum ipa_rm_resource_name depends_on_name);

int ipa_rm_request_resource(enum ipa_rm_resource_name resource_name);

int ipa_rm_release_resource(enum ipa_rm_resource_name resource_name);

int ipa_rm_notify_completion(enum ipa_rm_event event,
		enum ipa_rm_resource_name resource_name);

int ipa_rm_inactivity_timer_init(enum ipa_rm_resource_name resource_name,
				 unsigned long msecs);

int ipa_rm_inactivity_timer_destroy(enum ipa_rm_resource_name resource_name);

int ipa_rm_inactivity_timer_request_resource(
				enum ipa_rm_resource_name resource_name);

int ipa_rm_inactivity_timer_release_resource(
				enum ipa_rm_resource_name resource_name);

int a2_mux_open_channel(enum a2_mux_logical_channel_id lcid,
			void *user_data,
			a2_mux_notify_cb notify_cb);

int a2_mux_close_channel(enum a2_mux_logical_channel_id lcid);

int a2_mux_write(enum a2_mux_logical_channel_id lcid, struct sk_buff *skb);

int a2_mux_is_ch_empty(enum a2_mux_logical_channel_id lcid);

int a2_mux_is_ch_low(enum a2_mux_logical_channel_id lcid);

int a2_mux_is_ch_full(enum a2_mux_logical_channel_id lcid);

int a2_mux_get_tethered_client_handles(enum a2_mux_logical_channel_id lcid,
		unsigned int *clnt_cons_handle,
		unsigned int *clnt_prod_handle);

int teth_bridge_init(ipa_notify_cb *usb_notify_cb_ptr, void **private_data_ptr);

int teth_bridge_disconnect(void);

int teth_bridge_connect(struct teth_bridge_connect_params *connect_params);

int teth_bridge_set_aggr_params(struct teth_aggr_params *aggr_params);

void ipa_bam_reg_dump(void);
bool ipa_emb_ul_pipes_empty(void);

#else 

static inline int a2_mux_open_channel(enum a2_mux_logical_channel_id lcid,
	void *user_data, a2_mux_notify_cb notify_cb)
{
	return -EPERM;
}

static inline int a2_mux_close_channel(enum a2_mux_logical_channel_id lcid)
{
	return -EPERM;
}

static inline int a2_mux_write(enum a2_mux_logical_channel_id lcid,
			       struct sk_buff *skb)
{
	return -EPERM;
}

static inline int a2_mux_is_ch_empty(enum a2_mux_logical_channel_id lcid)
{
	return -EPERM;
}

static inline int a2_mux_is_ch_low(enum a2_mux_logical_channel_id lcid)
{
	return -EPERM;
}

static inline int a2_mux_is_ch_full(enum a2_mux_logical_channel_id lcid)
{
	return -EPERM;
}

static inline int a2_mux_get_tethered_client_handles(
	enum a2_mux_logical_channel_id lcid, unsigned int *clnt_cons_handle,
	unsigned int *clnt_prod_handle)
{
	return -EPERM;
}

static inline int ipa_connect(const struct ipa_connect_params *in,
		struct ipa_sps_params *sps,	u32 *clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_disconnect(u32 clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_resume(u32 clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_suspend(u32 clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_cfg_ep(u32 clnt_hdl,
		const struct ipa_ep_cfg *ipa_ep_cfg)
{
	return -EPERM;
}

static inline int ipa_cfg_ep_nat(u32 clnt_hdl,
		const struct ipa_ep_cfg_nat *ipa_ep_cfg)
{
	return -EPERM;
}

static inline int ipa_cfg_ep_hdr(u32 clnt_hdl,
		const struct ipa_ep_cfg_hdr *ipa_ep_cfg)
{
	return -EPERM;
}

static inline int ipa_cfg_ep_mode(u32 clnt_hdl,
		const struct ipa_ep_cfg_mode *ipa_ep_cfg)
{
	return -EPERM;
}

static inline int ipa_cfg_ep_aggr(u32 clnt_hdl,
		const struct ipa_ep_cfg_aggr *ipa_ep_cfg)
{
	return -EPERM;
}

static inline int ipa_cfg_ep_route(u32 clnt_hdl,
		const struct ipa_ep_cfg_route *ipa_ep_cfg)
{
	return -EPERM;
}

static inline int ipa_cfg_ep_holb(u32 clnt_hdl,
		const struct ipa_ep_cfg_holb *ipa_ep_cfg)
{
	return -EPERM;
}

static inline int ipa_add_hdr(struct ipa_ioc_add_hdr *hdrs)
{
	return -EPERM;
}

static inline int ipa_del_hdr(struct ipa_ioc_del_hdr *hdls)
{
	return -EPERM;
}

static inline int ipa_commit_hdr(void)
{
	return -EPERM;
}

static inline int ipa_reset_hdr(void)
{
	return -EPERM;
}

static inline int ipa_get_hdr(struct ipa_ioc_get_hdr *lookup)
{
	return -EPERM;
}

static inline int ipa_put_hdr(u32 hdr_hdl)
{
	return -EPERM;
}

static inline int ipa_copy_hdr(struct ipa_ioc_copy_hdr *copy)
{
	return -EPERM;
}

static inline int ipa_add_rt_rule(struct ipa_ioc_add_rt_rule *rules)
{
	return -EPERM;
}

static inline int ipa_del_rt_rule(struct ipa_ioc_del_rt_rule *hdls)
{
	return -EPERM;
}

static inline int ipa_commit_rt(enum ipa_ip_type ip)
{
	return -EPERM;
}

static inline int ipa_reset_rt(enum ipa_ip_type ip)
{
	return -EPERM;
}

static inline int ipa_get_rt_tbl(struct ipa_ioc_get_rt_tbl *lookup)
{
	return -EPERM;
}

static inline int ipa_put_rt_tbl(u32 rt_tbl_hdl)
{
	return -EPERM;
}

static inline int ipa_add_flt_rule(struct ipa_ioc_add_flt_rule *rules)
{
	return -EPERM;
}

static inline int ipa_del_flt_rule(struct ipa_ioc_del_flt_rule *hdls)
{
	return -EPERM;
}

static inline int ipa_commit_flt(enum ipa_ip_type ip)
{
	return -EPERM;
}

static inline int ipa_reset_flt(enum ipa_ip_type ip)
{
	return -EPERM;
}

static inline int allocate_nat_device(struct ipa_ioc_nat_alloc_mem *mem)
{
	return -EPERM;
}


static inline int ipa_nat_init_cmd(struct ipa_ioc_v4_nat_init *init)
{
	return -EPERM;
}


static inline int ipa_nat_dma_cmd(struct ipa_ioc_nat_dma_cmd *dma)
{
	return -EPERM;
}


static inline int ipa_nat_del_cmd(struct ipa_ioc_v4_nat_del *del)
{
	return -EPERM;
}

static inline int ipa_send_msg(struct ipa_msg_meta *meta, void *buff,
		ipa_msg_free_fn callback)
{
	return -EPERM;
}

static inline int ipa_register_pull_msg(struct ipa_msg_meta *meta,
		ipa_msg_pull_fn callback)
{
	return -EPERM;
}

static inline int ipa_deregister_pull_msg(struct ipa_msg_meta *meta)
{
	return -EPERM;
}

static inline int ipa_register_intf(const char *name,
				     const struct ipa_tx_intf *tx,
				     const struct ipa_rx_intf *rx)
{
	return -EPERM;
}

static inline int ipa_deregister_intf(const char *name)
{
	return -EPERM;
}

static inline int ipa_set_aggr_mode(enum ipa_aggr_mode mode)
{
	return -EPERM;
}

static inline int ipa_set_qcncm_ndp_sig(char sig[3])
{
	return -EPERM;
}

static inline int ipa_set_single_ndp_per_mbim(bool enable)
{
	return -EPERM;
}

static inline int ipa_bridge_setup(enum ipa_bridge_dir dir,
				    enum ipa_bridge_type type,
				    struct ipa_sys_connect_params *sys_in,
				    u32 *clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_bridge_teardown(enum ipa_bridge_dir dir,
				       enum ipa_bridge_type type,
				      u32 clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_tx_dp(enum ipa_client_type dst, struct sk_buff *skb,
		struct ipa_tx_meta *metadata)
{
	return -EPERM;
}

static inline int ipa_setup_sys_pipe(struct ipa_sys_connect_params *sys_in,
		u32 *clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_teardown_sys_pipe(u32 clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_rm_create_resource(
		struct ipa_rm_create_params *create_params)
{
	return -EPERM;
}

static inline int ipa_rm_delete_resource(
		enum ipa_rm_resource_name resource_name)
{
	return -EPERM;
}

static inline int ipa_rm_register(enum ipa_rm_resource_name resource_name,
			struct ipa_rm_register_params *reg_params)
{
	return -EPERM;
}

static inline int ipa_rm_deregister(enum ipa_rm_resource_name resource_name,
			struct ipa_rm_register_params *reg_params)
{
	return -EPERM;
}

static inline int ipa_rm_add_dependency(
		enum ipa_rm_resource_name resource_name,
		enum ipa_rm_resource_name depends_on_name)
{
	return -EPERM;
}

static inline int ipa_rm_delete_dependency(
		enum ipa_rm_resource_name resource_name,
		enum ipa_rm_resource_name depends_on_name)
{
	return -EPERM;
}

static inline int ipa_rm_request_resource(
		enum ipa_rm_resource_name resource_name)
{
	return -EPERM;
}

static inline int ipa_rm_release_resource(
		enum ipa_rm_resource_name resource_name)
{
	return -EPERM;
}

static inline int ipa_rm_notify_completion(enum ipa_rm_event event,
		enum ipa_rm_resource_name resource_name)
{
	return -EPERM;
}

static inline int ipa_rm_inactivity_timer_init(
		enum ipa_rm_resource_name resource_name,
			unsigned long msecs)
{
	return -EPERM;
}

static inline int ipa_rm_inactivity_timer_destroy(
		enum ipa_rm_resource_name resource_name)
{
	return -EPERM;
}

static inline int ipa_rm_inactivity_timer_request_resource(
				enum ipa_rm_resource_name resource_name)
{
	return -EPERM;
}

static inline int ipa_rm_inactivity_timer_release_resource(
				enum ipa_rm_resource_name resource_name)
{
	return -EPERM;
}

static inline int teth_bridge_init(ipa_notify_cb *usb_notify_cb_ptr,
				   void **private_data_ptr)
{
	return -EPERM;
}

static inline int teth_bridge_disconnect(void)
{
	return -EPERM;
}

static inline int teth_bridge_connect(struct teth_bridge_connect_params
				      *connect_params)
{
	return -EPERM;
}

static inline int teth_bridge_set_aggr_params(struct teth_aggr_params
					      *aggr_params)
{
	return -EPERM;
}

static inline void ipa_bam_reg_dump(void)
{
	return;
}

static inline bool ipa_emb_ul_pipes_empty(void)
{
	return false;
}

#endif 

#endif 
