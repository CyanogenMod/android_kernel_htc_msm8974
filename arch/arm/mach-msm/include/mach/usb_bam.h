/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#ifndef _USB_BAM_H_
#define _USB_BAM_H_
#include "sps.h"
#include <mach/ipa.h>
#include <linux/usb/msm_hsusb.h>

enum usb_bam {
	SSUSB_BAM = 0,
	HSUSB_BAM,
	HSIC_BAM,
	MAX_BAMS,
};

enum peer_bam {
	A2_P_BAM = 0,
	QDSS_P_BAM,
	IPA_P_BAM,
	MAX_PEER_BAMS,
};

enum usb_bam_pipe_dir {
	USB_TO_PEER_PERIPHERAL,
	PEER_PERIPHERAL_TO_USB,
};

enum usb_pipe_mem_type {
	SPS_PIPE_MEM = 0,	
	USB_PRIVATE_MEM,	
	SYSTEM_MEM,		
	OCI_MEM,		
};

enum usb_bam_event_type {
	USB_BAM_EVENT_WAKEUP_PIPE = 0,	
	USB_BAM_EVENT_WAKEUP,		
	USB_BAM_EVENT_INACTIVITY,	
};

struct usb_bam_connect_ipa_params {
	u8 src_idx;
	u8 dst_idx;
	u32 *src_pipe;
	u32 *dst_pipe;
	enum usb_bam_pipe_dir dir;
	
	u32 prod_clnt_hdl;
	u32 cons_clnt_hdl;
	
	enum ipa_client_type client;
	struct ipa_ep_cfg ipa_ep_cfg;
	void *priv;
	void (*notify)(void *priv, enum ipa_dp_evt_type evt,
			unsigned long data);
	int (*activity_notify)(void *priv);
	int (*inactivity_notify)(void *priv);
};

struct usb_bam_event_info {
	enum usb_bam_event_type type;
	struct sps_register_event event;
	int (*callback)(void *);
	void *param;
	struct work_struct event_w;
};

struct usb_bam_pipe_connect {
	const char *name;
	u32 pipe_num;
	enum usb_pipe_mem_type mem_type;
	enum usb_bam_pipe_dir dir;
	enum usb_bam bam_type;
	enum peer_bam peer_bam;
	u32 src_phy_addr;
	u32 src_pipe_index;
	u32 dst_phy_addr;
	u32 dst_pipe_index;
	u32 data_fifo_base_offset;
	u32 data_fifo_size;
	u32 desc_fifo_base_offset;
	u32 desc_fifo_size;
	struct sps_mem_buffer data_mem_buf;
	struct sps_mem_buffer desc_mem_buf;
	struct usb_bam_event_info event;
	bool enabled;
	bool suspended;
	int ipa_clnt_hdl;
	void *priv;
	int (*activity_notify)(void *priv);
	int (*inactivity_notify)(void *priv);
};

struct msm_usb_bam_platform_data {
	struct usb_bam_pipe_connect *connections;
	u8 max_connections;
	int usb_bam_num_pipes;
	phys_addr_t usb_bam_fifo_baseaddr;
	bool ignore_core_reset_ack;
	bool reset_on_connect[MAX_BAMS];
	bool disable_clk_gating;
};

#ifdef CONFIG_USB_BAM
int usb_bam_connect(u8 idx, u32 *bam_pipe_idx);

int usb_bam_connect_ipa(struct usb_bam_connect_ipa_params *ipa_params);

int usb_bam_disconnect_ipa(
		struct usb_bam_connect_ipa_params *ipa_params);

int usb_bam_register_wake_cb(u8 idx,
	int (*callback)(void *), void *param);

int usb_bam_register_peer_reset_cb(int (*callback)(void *), void *param);

int usb_bam_register_start_stop_cbs(
	void (*start)(void *, enum usb_bam_pipe_dir),
	void (*stop)(void *, enum usb_bam_pipe_dir),
	void *param);

void usb_bam_suspend(struct usb_bam_connect_ipa_params *ipa_params);

void usb_bam_resume(struct usb_bam_connect_ipa_params *ipa_params);
int usb_bam_disconnect_pipe(u8 idx);

int get_bam2bam_connection_info(u8 idx,
	u32 *usb_bam_handle, u32 *usb_bam_pipe_idx, u32 *peer_pipe_idx,
	struct sps_mem_buffer *desc_fifo, struct sps_mem_buffer *data_fifo);

int usb_bam_a2_reset(bool to_reconnect);

int usb_bam_client_ready(bool ready);

void usb_bam_reset_complete(void);

int usb_bam_get_qdss_idx(u8 num);

void usb_bam_set_qdss_core(const char *qdss_core);

int usb_bam_get_connection_idx(const char *name, enum peer_bam client,
	enum usb_bam_pipe_dir dir, u32 num);

#else
static inline int usb_bam_connect(u8 idx, u32 *bam_pipe_idx)
{
	return -ENODEV;
}

static inline int usb_bam_connect_ipa(
			struct usb_bam_connect_ipa_params *ipa_params)
{
	return -ENODEV;
}

static inline int usb_bam_disconnect_ipa(
			struct usb_bam_connect_ipa_params *ipa_params)
{
	return -ENODEV;
}

static inline void usb_bam_wait_for_cons_granted(
			struct usb_bam_connect_ipa_params *ipa_params)
{
	return;
}

static inline int usb_bam_register_wake_cb(u8 idx,
	int (*callback)(void *), void* param)
{
	return -ENODEV;
}

static inline int usb_bam_register_peer_reset_cb(
	int (*callback)(void *), void *param)
{
	return -ENODEV;
}

static inline int usb_bam_register_start_stop_cbs(
	void (*start)(void *, enum usb_bam_pipe_dir),
	void (*stop)(void *, enum usb_bam_pipe_dir),
	void *param)
{
	return -ENODEV;
}

static inline void usb_bam_suspend(
	struct usb_bam_connect_ipa_params *ipa_params){}

static inline void usb_bam_resume(
	struct usb_bam_connect_ipa_params *ipa_params) {}

static inline int usb_bam_disconnect_pipe(u8 idx)
{
	return -ENODEV;
}

static inline int get_bam2bam_connection_info(u8 idx,
	u32 *usb_bam_handle, u32 *usb_bam_pipe_idx, u32 *peer_pipe_idx,
	struct sps_mem_buffer *desc_fifo, struct sps_mem_buffer *data_fifo)
{
	return -ENODEV;
}

static inline int usb_bam_a2_reset(bool to_reconnect)
{
	return -ENODEV;
}

static inline int usb_bam_client_ready(bool ready)
{
	return -ENODEV;
}

static inline void usb_bam_reset_complete(void)
{
	return;
}

static inline int usb_bam_get_qdss_idx(u8 num)
{
	return -ENODEV;
}

static inline void usb_bam_set_qdss_core(const char *qdss_core)
{
	return;
}

static inline int usb_bam_get_connection_idx(const char *name,
		enum peer_bam client, enum usb_bam_pipe_dir dir, u32 num)
{
	return -ENODEV;
}
#endif
#endif				
