/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#ifndef ION_CP_COMMON_H
#define ION_CP_COMMON_H

#include <asm-generic/errno-base.h>
#include <linux/msm_ion.h>

#define ION_CP_V1	1
#define ION_CP_V2	2

struct ion_cp_buffer {
	phys_addr_t buffer;
	atomic_t secure_cnt;
	int is_secure;
	int want_delayed_unsecure;
	atomic_t map_cnt;
	struct mutex lock;
	int version;
	void *data;
	bool ignore_check;
};

#if defined(CONFIG_ION_MSM)
int ion_cp_change_chunks_state(unsigned long chunks, unsigned int nchunks,
			unsigned int chunk_size, enum cp_mem_usage usage,
			int lock);

int ion_cp_protect_mem(unsigned int phy_base, unsigned int size,
			unsigned int permission_type, int version,
			void *data);

int ion_cp_unprotect_mem(unsigned int phy_base, unsigned int size,
				unsigned int permission_type, int version,
				void *data);

int ion_cp_secure_buffer(struct ion_buffer *buffer, int version, void *data,
				int flags);

int ion_cp_unsecure_buffer(struct ion_buffer *buffer, int force_unsecure);
#else
static inline int ion_cp_change_chunks_state(unsigned long chunks,
			unsigned int nchunks, unsigned int chunk_size,
			enum cp_mem_usage usage, int lock)
{
	return -ENODEV;
}

static inline int ion_cp_protect_mem(unsigned int phy_base, unsigned int size,
			unsigned int permission_type, int version,
			void *data)
{
	return -ENODEV;
}

static inline int ion_cp_unprotect_mem(unsigned int phy_base, unsigned int size,
				unsigned int permission_type, int version,
				void *data)
{
	return -ENODEV;
}

static inline int ion_cp_secure_buffer(struct ion_buffer *buffer, int version,
				void *data, int flags)
{
	return -ENODEV;
}

static inline int ion_cp_unsecure_buffer(struct ion_buffer *buffer,
				int force_unsecure)
{
	return -ENODEV;
}
#endif

#endif
