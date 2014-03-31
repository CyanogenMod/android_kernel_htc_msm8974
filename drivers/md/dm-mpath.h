/*
 * Copyright (C) 2004 Red Hat, Inc. All rights reserved.
 *
 * This file is released under the GPL.
 *
 * Multipath.
 */

#ifndef	DM_MPATH_H
#define	DM_MPATH_H

struct dm_dev;

struct dm_path {
	struct dm_dev *dev;	
	void *pscontext;	
};

void dm_pg_init_complete(struct dm_path *path, unsigned err_flags);

#endif
