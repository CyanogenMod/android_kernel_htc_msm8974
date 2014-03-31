/*
 * Remote processor messaging
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Copyright (C) 2011 Google, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name Texas Instruments nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LINUX_RPMSG_H
#define _LINUX_RPMSG_H

#include <linux/types.h>
#include <linux/device.h>
#include <linux/mod_devicetable.h>

#define VIRTIO_RPMSG_F_NS	0 

struct rpmsg_hdr {
	u32 src;
	u32 dst;
	u32 reserved;
	u16 len;
	u16 flags;
	u8 data[0];
} __packed;

struct rpmsg_ns_msg {
	char name[RPMSG_NAME_SIZE];
	u32 addr;
	u32 flags;
} __packed;

enum rpmsg_ns_flags {
	RPMSG_NS_CREATE		= 0,
	RPMSG_NS_DESTROY	= 1,
};

#define RPMSG_ADDR_ANY		0xFFFFFFFF

struct virtproc_info;

struct rpmsg_channel {
	struct virtproc_info *vrp;
	struct device dev;
	struct rpmsg_device_id id;
	u32 src;
	u32 dst;
	struct rpmsg_endpoint *ept;
	bool announce;
};

typedef void (*rpmsg_rx_cb_t)(struct rpmsg_channel *, void *, int, void *, u32);

struct rpmsg_endpoint {
	struct rpmsg_channel *rpdev;
	rpmsg_rx_cb_t cb;
	u32 addr;
	void *priv;
};

struct rpmsg_driver {
	struct device_driver drv;
	const struct rpmsg_device_id *id_table;
	int (*probe)(struct rpmsg_channel *dev);
	void (*remove)(struct rpmsg_channel *dev);
	void (*callback)(struct rpmsg_channel *, void *, int, void *, u32);
};

int register_rpmsg_device(struct rpmsg_channel *dev);
void unregister_rpmsg_device(struct rpmsg_channel *dev);
int register_rpmsg_driver(struct rpmsg_driver *drv);
void unregister_rpmsg_driver(struct rpmsg_driver *drv);
void rpmsg_destroy_ept(struct rpmsg_endpoint *);
struct rpmsg_endpoint *rpmsg_create_ept(struct rpmsg_channel *,
				rpmsg_rx_cb_t cb, void *priv, u32 addr);
int
rpmsg_send_offchannel_raw(struct rpmsg_channel *, u32, u32, void *, int, bool);

static inline int rpmsg_send(struct rpmsg_channel *rpdev, void *data, int len)
{
	u32 src = rpdev->src, dst = rpdev->dst;

	return rpmsg_send_offchannel_raw(rpdev, src, dst, data, len, true);
}

static inline
int rpmsg_sendto(struct rpmsg_channel *rpdev, void *data, int len, u32 dst)
{
	u32 src = rpdev->src;

	return rpmsg_send_offchannel_raw(rpdev, src, dst, data, len, true);
}

static inline
int rpmsg_send_offchannel(struct rpmsg_channel *rpdev, u32 src, u32 dst,
							void *data, int len)
{
	return rpmsg_send_offchannel_raw(rpdev, src, dst, data, len, true);
}

static inline
int rpmsg_trysend(struct rpmsg_channel *rpdev, void *data, int len)
{
	u32 src = rpdev->src, dst = rpdev->dst;

	return rpmsg_send_offchannel_raw(rpdev, src, dst, data, len, false);
}

static inline
int rpmsg_trysendto(struct rpmsg_channel *rpdev, void *data, int len, u32 dst)
{
	u32 src = rpdev->src;

	return rpmsg_send_offchannel_raw(rpdev, src, dst, data, len, false);
}

static inline
int rpmsg_trysend_offchannel(struct rpmsg_channel *rpdev, u32 src, u32 dst,
							void *data, int len)
{
	return rpmsg_send_offchannel_raw(rpdev, src, dst, data, len, false);
}

#endif 
