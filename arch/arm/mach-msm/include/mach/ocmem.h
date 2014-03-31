/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef _ARCH_ARM_MACH_MSM_OCMEM_H
#define _ARCH_ARM_MACH_MSM_OCMEM_H

#include <asm/page.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/err.h>

#define OCMEM_MIN_ALLOC SZ_64K
#define OCMEM_MIN_ALIGN SZ_64K

#define OCMEM_MAX_CHUNKS 32
#define MIN_CHUNK_SIZE SZ_4K

struct ocmem_notifier;

struct ocmem_buf {
	unsigned long addr;
	unsigned long len;
};

struct ocmem_buf_attr {
	unsigned long paddr;
	unsigned long len;
};

struct ocmem_chunk {
	bool ro;
	unsigned long ddr_paddr;
	unsigned long size;
};

struct ocmem_map_list {
	unsigned num_chunks;
	struct ocmem_chunk chunks[OCMEM_MAX_CHUNKS];
};

enum ocmem_power_state {
	OCMEM_OFF = 0x0,
	OCMEM_RETENTION,
	OCMEM_ON,
	OCMEM_MAX = OCMEM_ON,
};

struct ocmem_resource {
	unsigned resource_id;
	unsigned num_keys;
	unsigned int *keys;
};

struct ocmem_vectors {
	unsigned num_resources;
	struct ocmem_resource *r;
};

enum ocmem_client {
	
	OCMEM_GRAPHICS = 0x0,
	
	OCMEM_VIDEO,
	OCMEM_CAMERA,
	
	OCMEM_HP_AUDIO,
	OCMEM_VOICE,
	
	OCMEM_LP_AUDIO,
	OCMEM_SENSORS,
	OCMEM_OTHER_OS,
	OCMEM_CLIENT_MAX,
};

enum ocmem_notif_type {
	OCMEM_MAP_DONE = 1,
	OCMEM_MAP_FAIL,
	OCMEM_UNMAP_DONE,
	OCMEM_UNMAP_FAIL,
	OCMEM_ALLOC_GROW,
	OCMEM_ALLOC_SHRINK,
	OCMEM_NOTIF_TYPE_COUNT,
};

#ifdef CONFIG_MSM_OCMEM
struct ocmem_notifier *ocmem_notifier_register(int client_id,
						struct notifier_block *nb);

int ocmem_notifier_unregister(struct ocmem_notifier *notif_hndl,
				struct notifier_block *nb);

unsigned long get_max_quota(int client_id);

struct ocmem_buf *ocmem_allocate(int client_id, unsigned long size);

struct ocmem_buf *ocmem_allocate_nowait(int client_id, unsigned long size);

struct ocmem_buf *ocmem_allocate_nb(int client_id, unsigned long size);

struct ocmem_buf *ocmem_allocate_range(int client_id, unsigned long min,
			unsigned long goal, unsigned long step);

int ocmem_free(int client_id, struct ocmem_buf *buf);

int ocmem_shrink(int client_id, struct ocmem_buf *buf,
			unsigned long new_size);

int ocmem_map(int client_id, struct ocmem_buf *buffer,
			struct ocmem_map_list *list);


int ocmem_unmap(int client_id, struct ocmem_buf *buffer,
			struct ocmem_map_list *list);

int ocmem_drop(int client_id, struct ocmem_buf *buffer,
			struct ocmem_map_list *list);

int ocmem_dump(int client_id, struct ocmem_buf *buffer,
				unsigned long dst_phys_addr);

int ocmem_evict(int client_id);

int ocmem_restore(int client_id);

int ocmem_set_power_state(int client_id, struct ocmem_buf *buf,
				enum ocmem_power_state new_state);

enum ocmem_power_state ocmem_get_power_state(int client_id,
				struct ocmem_buf *buf);

struct ocmem_vectors *ocmem_get_vectors(int client_id,
						struct ocmem_buf *buf);

#else
static inline struct ocmem_notifier *ocmem_notifier_register
				(int client_id, struct notifier_block *nb)
{
	return ERR_PTR(-ENODEV);
}

static inline int ocmem_notifier_unregister(struct ocmem_notifier *notif_hndl,
				struct notifier_block *nb)
{
	return -ENODEV;
}

static inline unsigned long get_max_quota(int client_id)
{
	return 0;
}

static inline struct ocmem_buf *ocmem_allocate(int client_id,
						unsigned long size)
{
	return ERR_PTR(-ENODEV);
}

static inline struct ocmem_buf *ocmem_allocate_nowait(int client_id,
							unsigned long size)
{
	return ERR_PTR(-ENODEV);
}

static inline struct ocmem_buf *ocmem_allocate_nb(int client_id,
							unsigned long size)
{
	return ERR_PTR(-ENODEV);
}

static inline struct ocmem_buf *ocmem_allocate_range(int client_id,
		unsigned long min, unsigned long goal, unsigned long step)
{
	return ERR_PTR(-ENODEV);
}

static inline int ocmem_free(int client_id, struct ocmem_buf *buf)
{
	return -ENODEV;
}

static inline int ocmem_shrink(int client_id, struct ocmem_buf *buf,
			unsigned long new_size)
{
	return -ENODEV;
}

static inline int ocmem_map(int client_id, struct ocmem_buf *buffer,
			struct ocmem_map_list *list)
{
	return -ENODEV;
}

static inline int ocmem_unmap(int client_id, struct ocmem_buf *buffer,
			struct ocmem_map_list *list)
{
	return -ENODEV;
}

static inline int ocmem_dump(int client_id, struct ocmem_buf *buffer,
				unsigned long dst_phys_addr)
{
	return -ENODEV;
}

static inline int ocmem_evict(int client_id)
{
	return -ENODEV;
}

static inline int ocmem_restore(int client_id)
{
	return -ENODEV;
}

static inline int ocmem_set_power_state(int client_id,
		struct ocmem_buf *buf, enum ocmem_power_state new_state)
{
	return -ENODEV;
}

static inline enum ocmem_power_state ocmem_get_power_state(int client_id,
				struct ocmem_buf *buf)
{
	return -ENODEV;
}
static inline struct ocmem_vectors *ocmem_get_vectors(int client_id,
						struct ocmem_buf *buf)
{
	return ERR_PTR(-ENODEV);
}
#endif
#endif
