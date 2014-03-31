/* AFS volume management
 *
 * Copyright (C) 2002, 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/sched.h>
#include "internal.h"

static const char *afs_voltypes[] = { "R/W", "R/O", "BAK" };

struct afs_volume *afs_volume_lookup(struct afs_mount_params *params)
{
	struct afs_vlocation *vlocation = NULL;
	struct afs_volume *volume = NULL;
	struct afs_server *server = NULL;
	char srvtmask;
	int ret, loop;

	_enter("{%*.*s,%d}",
	       params->volnamesz, params->volnamesz, params->volname, params->rwpath);

	
	vlocation = afs_vlocation_lookup(params->cell, params->key,
					 params->volname, params->volnamesz);
	if (IS_ERR(vlocation)) {
		ret = PTR_ERR(vlocation);
		vlocation = NULL;
		goto error;
	}

	
	ret = -ENOMEDIUM;
	if (params->force && !(vlocation->vldb.vidmask & (1 << params->type)))
		goto error;

	srvtmask = 0;
	for (loop = 0; loop < vlocation->vldb.nservers; loop++)
		srvtmask |= vlocation->vldb.srvtmask[loop];

	if (params->force) {
		if (!(srvtmask & (1 << params->type)))
			goto error;
	} else if (srvtmask & AFS_VOL_VTM_RO) {
		params->type = AFSVL_ROVOL;
	} else if (srvtmask & AFS_VOL_VTM_RW) {
		params->type = AFSVL_RWVOL;
	} else {
		goto error;
	}

	down_write(&params->cell->vl_sem);

	
	if (vlocation->vols[params->type]) {
		
		volume = vlocation->vols[params->type];
		afs_get_volume(volume);
		goto success;
	}

	
	_debug("creating new volume record");

	ret = -ENOMEM;
	volume = kzalloc(sizeof(struct afs_volume), GFP_KERNEL);
	if (!volume)
		goto error_up;

	atomic_set(&volume->usage, 1);
	volume->type		= params->type;
	volume->type_force	= params->force;
	volume->cell		= params->cell;
	volume->vid		= vlocation->vldb.vid[params->type];

	ret = bdi_setup_and_register(&volume->bdi, "afs", BDI_CAP_MAP_COPY);
	if (ret)
		goto error_bdi;

	init_rwsem(&volume->server_sem);

	
	for (loop = 0; loop < 8; loop++) {
		if (vlocation->vldb.srvtmask[loop] & (1 << volume->type)) {
			server = afs_lookup_server(
			       volume->cell, &vlocation->vldb.servers[loop]);
			if (IS_ERR(server)) {
				ret = PTR_ERR(server);
				goto error_discard;
			}

			volume->servers[volume->nservers] = server;
			volume->nservers++;
		}
	}

	
#ifdef CONFIG_AFS_FSCACHE
	volume->cache = fscache_acquire_cookie(vlocation->cache,
					       &afs_volume_cache_index_def,
					       volume);
#endif
	afs_get_vlocation(vlocation);
	volume->vlocation = vlocation;

	vlocation->vols[volume->type] = volume;

success:
	_debug("kAFS selected %s volume %08x",
	       afs_voltypes[volume->type], volume->vid);
	up_write(&params->cell->vl_sem);
	afs_put_vlocation(vlocation);
	_leave(" = %p", volume);
	return volume;

	
error_up:
	up_write(&params->cell->vl_sem);
error:
	afs_put_vlocation(vlocation);
	_leave(" = %d", ret);
	return ERR_PTR(ret);

error_discard:
	bdi_destroy(&volume->bdi);
error_bdi:
	up_write(&params->cell->vl_sem);

	for (loop = volume->nservers - 1; loop >= 0; loop--)
		afs_put_server(volume->servers[loop]);

	kfree(volume);
	goto error;
}

void afs_put_volume(struct afs_volume *volume)
{
	struct afs_vlocation *vlocation;
	int loop;

	if (!volume)
		return;

	_enter("%p", volume);

	ASSERTCMP(atomic_read(&volume->usage), >, 0);

	vlocation = volume->vlocation;

	down_write(&vlocation->cell->vl_sem);

	if (likely(!atomic_dec_and_test(&volume->usage))) {
		up_write(&vlocation->cell->vl_sem);
		_leave("");
		return;
	}

	vlocation->vols[volume->type] = NULL;

	up_write(&vlocation->cell->vl_sem);

	
#ifdef CONFIG_AFS_FSCACHE
	fscache_relinquish_cookie(volume->cache, 0);
#endif
	afs_put_vlocation(vlocation);

	for (loop = volume->nservers - 1; loop >= 0; loop--)
		afs_put_server(volume->servers[loop]);

	bdi_destroy(&volume->bdi);
	kfree(volume);

	_leave(" [destroyed]");
}

struct afs_server *afs_volume_pick_fileserver(struct afs_vnode *vnode)
{
	struct afs_volume *volume = vnode->volume;
	struct afs_server *server;
	int ret, state, loop;

	_enter("%s", volume->vlocation->vldb.name);

	
	if (vnode->server && vnode->server->fs_state == 0) {
		afs_get_server(vnode->server);
		_leave(" = %p [current]", vnode->server);
		return vnode->server;
	}

	down_read(&volume->server_sem);

	
	if (volume->nservers == 0) {
		ret = volume->rjservers ? -ENOMEDIUM : -ESTALE;
		up_read(&volume->server_sem);
		_leave(" = %d [no servers]", ret);
		return ERR_PTR(ret);
	}

	ret = 0;
	for (loop = 0; loop < volume->nservers; loop++) {
		server = volume->servers[loop];
		state = server->fs_state;

		_debug("consider %d [%d]", loop, state);

		switch (state) {
			
		case 0:
			afs_get_server(server);
			up_read(&volume->server_sem);
			_leave(" = %p (picked %08x)",
			       server, ntohl(server->addr.s_addr));
			return server;

		case -ENETUNREACH:
			if (ret == 0)
				ret = state;
			break;

		case -EHOSTUNREACH:
			if (ret == 0 ||
			    ret == -ENETUNREACH)
				ret = state;
			break;

		case -ECONNREFUSED:
			if (ret == 0 ||
			    ret == -ENETUNREACH ||
			    ret == -EHOSTUNREACH)
				ret = state;
			break;

		default:
		case -EREMOTEIO:
			if (ret == 0 ||
			    ret == -ENETUNREACH ||
			    ret == -EHOSTUNREACH ||
			    ret == -ECONNREFUSED)
				ret = state;
			break;
		}
	}

	up_read(&volume->server_sem);
	_leave(" = %d", ret);
	return ERR_PTR(ret);
}

int afs_volume_release_fileserver(struct afs_vnode *vnode,
				  struct afs_server *server,
				  int result)
{
	struct afs_volume *volume = vnode->volume;
	unsigned loop;

	_enter("%s,%08x,%d",
	       volume->vlocation->vldb.name, ntohl(server->addr.s_addr),
	       result);

	switch (result) {
		
	case 0:
		server->fs_act_jif = jiffies;
		server->fs_state = 0;
		_leave("");
		return 1;

		
	case -ENOMEDIUM:
		server->fs_act_jif = jiffies;
		down_write(&volume->server_sem);

		for (loop = 0; loop < volume->nservers; loop++)
			if (volume->servers[loop] == server)
				goto present;

		
		goto try_next_server_upw;

	present:
		volume->nservers--;
		memmove(&volume->servers[loop],
			&volume->servers[loop + 1],
			sizeof(volume->servers[loop]) *
			(volume->nservers - loop));
		volume->servers[volume->nservers] = NULL;
		afs_put_server(server);
		volume->rjservers++;

		if (volume->nservers > 0)
			
			goto try_next_server_upw;

		up_write(&volume->server_sem);
		afs_put_server(server);
		_leave(" [completely rejected]");
		return 1;

		
	case -ENETUNREACH:
	case -EHOSTUNREACH:
	case -ECONNREFUSED:
	case -ETIME:
	case -ETIMEDOUT:
	case -EREMOTEIO:
		spin_lock(&server->fs_lock);
		if (!server->fs_state) {
			server->fs_dead_jif = jiffies + HZ * 10;
			server->fs_state = result;
			printk("kAFS: SERVER DEAD state=%d\n", result);
		}
		spin_unlock(&server->fs_lock);
		goto try_next_server;

		
	default:
		server->fs_act_jif = jiffies;
	case -ENOMEM:
	case -ENONET:
		
		afs_put_server(server);
		_leave(" [local failure]");
		return 1;
	}

	
try_next_server_upw:
	up_write(&volume->server_sem);
try_next_server:
	afs_put_server(server);
	_leave(" [try next server]");
	return 0;
}
