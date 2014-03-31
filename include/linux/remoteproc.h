/*
 * Remote Processor Framework
 *
 * Copyright(c) 2011 Texas Instruments, Inc.
 * Copyright(c) 2011 Google, Inc.
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

#ifndef REMOTEPROC_H
#define REMOTEPROC_H

#include <linux/types.h>
#include <linux/kref.h>
#include <linux/klist.h>
#include <linux/mutex.h>
#include <linux/virtio.h>
#include <linux/completion.h>
#include <linux/idr.h>

struct resource_table {
	u32 ver;
	u32 num;
	u32 reserved[2];
	u32 offset[0];
} __packed;

struct fw_rsc_hdr {
	u32 type;
	u8 data[0];
} __packed;

enum fw_resource_type {
	RSC_CARVEOUT	= 0,
	RSC_DEVMEM	= 1,
	RSC_TRACE	= 2,
	RSC_VDEV	= 3,
	RSC_LAST	= 4,
};

#define FW_RSC_ADDR_ANY (0xFFFFFFFFFFFFFFFF)

struct fw_rsc_carveout {
	u32 da;
	u32 pa;
	u32 len;
	u32 flags;
	u32 reserved;
	u8 name[32];
} __packed;

struct fw_rsc_devmem {
	u32 da;
	u32 pa;
	u32 len;
	u32 flags;
	u32 reserved;
	u8 name[32];
} __packed;

struct fw_rsc_trace {
	u32 da;
	u32 len;
	u32 reserved;
	u8 name[32];
} __packed;

struct fw_rsc_vdev_vring {
	u32 da;
	u32 align;
	u32 num;
	u32 notifyid;
	u32 reserved;
} __packed;

struct fw_rsc_vdev {
	u32 id;
	u32 notifyid;
	u32 dfeatures;
	u32 gfeatures;
	u32 config_len;
	u8 status;
	u8 num_of_vrings;
	u8 reserved[2];
	struct fw_rsc_vdev_vring vring[0];
} __packed;

struct rproc_mem_entry {
	void *va;
	dma_addr_t dma;
	int len;
	u32 da;
	void *priv;
	struct list_head node;
};

struct rproc;

struct rproc_ops {
	int (*start)(struct rproc *rproc);
	int (*stop)(struct rproc *rproc);
	void (*kick)(struct rproc *rproc, int vqid);
};

enum rproc_state {
	RPROC_OFFLINE	= 0,
	RPROC_SUSPENDED	= 1,
	RPROC_RUNNING	= 2,
	RPROC_CRASHED	= 3,
	RPROC_LAST	= 4,
};

struct rproc {
	struct klist_node node;
	struct iommu_domain *domain;
	const char *name;
	const char *firmware;
	void *priv;
	const struct rproc_ops *ops;
	struct device *dev;
	struct kref refcount;
	atomic_t power;
	unsigned int state;
	struct mutex lock;
	struct dentry *dbg_dir;
	struct list_head traces;
	int num_traces;
	struct list_head carveouts;
	struct list_head mappings;
	struct completion firmware_loading_complete;
	u32 bootaddr;
	struct list_head rvdevs;
	struct idr notifyids;
};

#define RVDEV_NUM_VRINGS 2

struct rproc_vring {
	void *va;
	dma_addr_t dma;
	int len;
	u32 da;
	u32 align;
	int notifyid;
	struct rproc_vdev *rvdev;
	struct virtqueue *vq;
};

struct rproc_vdev {
	struct list_head node;
	struct rproc *rproc;
	struct virtio_device vdev;
	struct rproc_vring vring[RVDEV_NUM_VRINGS];
	unsigned long dfeatures;
	unsigned long gfeatures;
};

struct rproc *rproc_get_by_name(const char *name);
void rproc_put(struct rproc *rproc);

struct rproc *rproc_alloc(struct device *dev, const char *name,
				const struct rproc_ops *ops,
				const char *firmware, int len);
void rproc_free(struct rproc *rproc);
int rproc_register(struct rproc *rproc);
int rproc_unregister(struct rproc *rproc);

int rproc_boot(struct rproc *rproc);
void rproc_shutdown(struct rproc *rproc);

static inline struct rproc_vdev *vdev_to_rvdev(struct virtio_device *vdev)
{
	return container_of(vdev, struct rproc_vdev, vdev);
}

static inline struct rproc *vdev_to_rproc(struct virtio_device *vdev)
{
	struct rproc_vdev *rvdev = vdev_to_rvdev(vdev);

	return rvdev->rproc;
}

#endif 
