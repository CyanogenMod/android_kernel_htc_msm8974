/*
 * include/linux/ion.h
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_ION_H
#define _LINUX_ION_H

#include <linux/ioctl.h>
#include <linux/types.h>

struct ion_handle;
typedef struct ion_handle *ion_user_handle_t;

enum ion_heap_type {
	ION_HEAP_TYPE_SYSTEM,
	ION_HEAP_TYPE_SYSTEM_CONTIG,
	ION_HEAP_TYPE_CARVEOUT,
	ION_HEAP_TYPE_CHUNK,
	ION_HEAP_TYPE_CUSTOM, 
	ION_NUM_HEAPS,
};

#define ION_HEAP_SYSTEM_MASK		(1 << ION_HEAP_TYPE_SYSTEM)
#define ION_HEAP_SYSTEM_CONTIG_MASK	(1 << ION_HEAP_TYPE_SYSTEM_CONTIG)
#define ION_HEAP_CARVEOUT_MASK		(1 << ION_HEAP_TYPE_CARVEOUT)

#define ION_NUM_HEAP_IDS		sizeof(unsigned int) * 8

#define ION_FLAG_CACHED 1		
#define ION_FLAG_CACHED_NEEDS_SYNC 2	
#define ION_FLAG_FREED_FROM_SHRINKER 4	

#ifdef __KERNEL__
#include <linux/err.h>
#include <mach/ion.h>
struct ion_device;
struct ion_heap;
struct ion_mapper;
struct ion_client;
struct ion_buffer;

#define ion_phys_addr_t unsigned long

struct ion_platform_heap {
	enum ion_heap_type type;
	unsigned int id;
	const char *name;
	ion_phys_addr_t base;
	size_t size;
	enum ion_memory_types memory_type;
	unsigned int has_outer_cache;
	void *extra_data;
	ion_phys_addr_t align;
	void *priv;
};

struct ion_platform_data {
	unsigned int has_outer_cache;
	int nr;
	struct ion_platform_heap *heaps;
};

#ifdef CONFIG_ION

int ion_iommu_heap_dump_size(void);

void ion_reserve(struct ion_platform_data *data);

struct ion_client *ion_client_create(struct ion_device *dev,
				     const char *name);

void ion_client_destroy(struct ion_client *client);

struct ion_handle *ion_alloc(struct ion_client *client, size_t len,
			     size_t align, unsigned int heap_id_mask,
			     unsigned int flags);

void ion_free(struct ion_client *client, struct ion_handle *handle);

int ion_phys(struct ion_client *client, struct ion_handle *handle,
	     ion_phys_addr_t *addr, size_t *len);

struct sg_table *ion_sg_table(struct ion_client *client,
			      struct ion_handle *handle);

void *ion_map_kernel(struct ion_client *client, struct ion_handle *handle);

void ion_unmap_kernel(struct ion_client *client, struct ion_handle *handle);

struct dma_buf *ion_share_dma_buf(struct ion_client *client,
						struct ion_handle *handle);

int ion_share_dma_buf_fd(struct ion_client *client, struct ion_handle *handle);

struct ion_handle *ion_import_dma_buf(struct ion_client *client, int fd);

#else
static inline void ion_reserve(struct ion_platform_data *data)
{

}

static inline struct ion_client *ion_client_create(struct ion_device *dev,
				     unsigned int heap_mask, const char *name)
{
	return ERR_PTR(-ENODEV);
}

static inline void ion_client_destroy(struct ion_client *client) { }

static inline struct ion_handle *ion_alloc(struct ion_client *client,
					size_t len, size_t align,
					unsigned int heap_mask,
					unsigned int flags)
{
	return ERR_PTR(-ENODEV);
}

static inline void ion_free(struct ion_client *client,
	struct ion_handle *handle) { }


static inline int ion_phys(struct ion_client *client,
	struct ion_handle *handle, ion_phys_addr_t *addr, size_t *len)
{
	return -ENODEV;
}

static inline struct sg_table *ion_sg_table(struct ion_client *client,
			      struct ion_handle *handle)
{
	return ERR_PTR(-ENODEV);
}

static inline void *ion_map_kernel(struct ion_client *client,
	struct ion_handle *handle)
{
	return ERR_PTR(-ENODEV);
}

static inline void ion_unmap_kernel(struct ion_client *client,
	struct ion_handle *handle) { }

static inline int ion_share_dma_buf(struct ion_client *client, struct ion_handle *handle)
{
	return -ENODEV;
}

static inline struct ion_handle *ion_import_dma_buf(struct ion_client *client, int fd)
{
	return ERR_PTR(-ENODEV);
}

static inline int ion_handle_get_flags(struct ion_client *client,
	struct ion_handle *handle, unsigned long *flags)
{
	return -ENODEV;
}

#endif 
#endif 


struct ion_allocation_data {
	size_t len;
	size_t align;
	unsigned int heap_mask;
	unsigned int flags;
	ion_user_handle_t handle;
};

struct ion_fd_data {
	ion_user_handle_t handle;
	int fd;
};

struct ion_handle_data {
	ion_user_handle_t handle;
};

struct ion_custom_data {
	unsigned int cmd;
	unsigned long arg;
};
#define ION_IOC_MAGIC		'I'

#define ION_IOC_ALLOC		_IOWR(ION_IOC_MAGIC, 0, \
				      struct ion_allocation_data)

#define ION_IOC_FREE		_IOWR(ION_IOC_MAGIC, 1, struct ion_handle_data)

#define ION_IOC_MAP		_IOWR(ION_IOC_MAGIC, 2, struct ion_fd_data)

#define ION_IOC_SHARE		_IOWR(ION_IOC_MAGIC, 4, struct ion_fd_data)

#define ION_IOC_IMPORT		_IOWR(ION_IOC_MAGIC, 5, struct ion_fd_data)

#define ION_IOC_SYNC		_IOWR(ION_IOC_MAGIC, 7, struct ion_fd_data)

#define ION_IOC_CUSTOM		_IOWR(ION_IOC_MAGIC, 6, struct ion_custom_data)


#endif 
