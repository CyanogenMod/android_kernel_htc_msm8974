/*
 * Copyright (C) 2003 Sistina Software
 * Copyright (C) 2004-2008 Red Hat, Inc. All rights reserved.
 *
 * Device-Mapper dirty region log.
 *
 * This file is released under the LGPL.
 */

#ifndef _LINUX_DM_DIRTY_LOG
#define _LINUX_DM_DIRTY_LOG

#ifdef __KERNEL__

#include <linux/types.h>
#include <linux/device-mapper.h>

typedef sector_t region_t;

struct dm_dirty_log_type;

struct dm_dirty_log {
	struct dm_dirty_log_type *type;
	int (*flush_callback_fn)(struct dm_target *ti);
	void *context;
};

struct dm_dirty_log_type {
	const char *name;
	struct module *module;

	
	struct list_head list;

	int (*ctr)(struct dm_dirty_log *log, struct dm_target *ti,
		   unsigned argc, char **argv);
	void (*dtr)(struct dm_dirty_log *log);

	int (*presuspend)(struct dm_dirty_log *log);
	int (*postsuspend)(struct dm_dirty_log *log);
	int (*resume)(struct dm_dirty_log *log);

	uint32_t (*get_region_size)(struct dm_dirty_log *log);

	int (*is_clean)(struct dm_dirty_log *log, region_t region);

	int (*in_sync)(struct dm_dirty_log *log, region_t region,
		       int can_block);

	int (*flush)(struct dm_dirty_log *log);

	void (*mark_region)(struct dm_dirty_log *log, region_t region);
	void (*clear_region)(struct dm_dirty_log *log, region_t region);

	int (*get_resync_work)(struct dm_dirty_log *log, region_t *region);

	void (*set_region_sync)(struct dm_dirty_log *log,
				region_t region, int in_sync);

	region_t (*get_sync_count)(struct dm_dirty_log *log);

	int (*status)(struct dm_dirty_log *log, status_type_t status_type,
		      char *result, unsigned maxlen);

	int (*is_remote_recovering)(struct dm_dirty_log *log, region_t region);
};

int dm_dirty_log_type_register(struct dm_dirty_log_type *type);
int dm_dirty_log_type_unregister(struct dm_dirty_log_type *type);

struct dm_dirty_log *dm_dirty_log_create(const char *type_name,
			struct dm_target *ti,
			int (*flush_callback_fn)(struct dm_target *ti),
			unsigned argc, char **argv);
void dm_dirty_log_destroy(struct dm_dirty_log *log);

#endif	
#endif	
