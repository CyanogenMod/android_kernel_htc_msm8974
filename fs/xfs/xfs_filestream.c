/*
 * Copyright (c) 2006-2007 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "xfs.h"
#include "xfs_bmap_btree.h"
#include "xfs_inum.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_ag.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_mount.h"
#include "xfs_bmap.h"
#include "xfs_alloc.h"
#include "xfs_utils.h"
#include "xfs_mru_cache.h"
#include "xfs_filestream.h"
#include "xfs_trace.h"

#ifdef XFS_FILESTREAMS_TRACE

ktrace_t *xfs_filestreams_trace_buf;

STATIC void
xfs_filestreams_trace(
	xfs_mount_t	*mp,	
	int		type,	
	const char	*func,	
	int		line,	
	__psunsigned_t	arg0,
	__psunsigned_t	arg1,
	__psunsigned_t	arg2,
	__psunsigned_t	arg3,
	__psunsigned_t	arg4,
	__psunsigned_t	arg5)
{
	ktrace_enter(xfs_filestreams_trace_buf,
		(void *)(__psint_t)(type | (line << 16)),
		(void *)func,
		(void *)(__psunsigned_t)current_pid(),
		(void *)mp,
		(void *)(__psunsigned_t)arg0,
		(void *)(__psunsigned_t)arg1,
		(void *)(__psunsigned_t)arg2,
		(void *)(__psunsigned_t)arg3,
		(void *)(__psunsigned_t)arg4,
		(void *)(__psunsigned_t)arg5,
		NULL, NULL, NULL, NULL, NULL, NULL);
}

#define TRACE0(mp,t)			TRACE6(mp,t,0,0,0,0,0,0)
#define TRACE1(mp,t,a0)			TRACE6(mp,t,a0,0,0,0,0,0)
#define TRACE2(mp,t,a0,a1)		TRACE6(mp,t,a0,a1,0,0,0,0)
#define TRACE3(mp,t,a0,a1,a2)		TRACE6(mp,t,a0,a1,a2,0,0,0)
#define TRACE4(mp,t,a0,a1,a2,a3)	TRACE6(mp,t,a0,a1,a2,a3,0,0)
#define TRACE5(mp,t,a0,a1,a2,a3,a4)	TRACE6(mp,t,a0,a1,a2,a3,a4,0)
#define TRACE6(mp,t,a0,a1,a2,a3,a4,a5) \
	xfs_filestreams_trace(mp, t, __func__, __LINE__, \
				(__psunsigned_t)a0, (__psunsigned_t)a1, \
				(__psunsigned_t)a2, (__psunsigned_t)a3, \
				(__psunsigned_t)a4, (__psunsigned_t)a5)

#define TRACE_AG_SCAN(mp, ag, ag2) \
		TRACE2(mp, XFS_FSTRM_KTRACE_AGSCAN, ag, ag2);
#define TRACE_AG_PICK1(mp, max_ag, maxfree) \
		TRACE2(mp, XFS_FSTRM_KTRACE_AGPICK1, max_ag, maxfree);
#define TRACE_AG_PICK2(mp, ag, ag2, cnt, free, scan, flag) \
		TRACE6(mp, XFS_FSTRM_KTRACE_AGPICK2, ag, ag2, \
			 cnt, free, scan, flag)
#define TRACE_UPDATE(mp, ip, ag, cnt, ag2, cnt2) \
		TRACE5(mp, XFS_FSTRM_KTRACE_UPDATE, ip, ag, cnt, ag2, cnt2)
#define TRACE_FREE(mp, ip, pip, ag, cnt) \
		TRACE4(mp, XFS_FSTRM_KTRACE_FREE, ip, pip, ag, cnt)
#define TRACE_LOOKUP(mp, ip, pip, ag, cnt) \
		TRACE4(mp, XFS_FSTRM_KTRACE_ITEM_LOOKUP, ip, pip, ag, cnt)
#define TRACE_ASSOCIATE(mp, ip, pip, ag, cnt) \
		TRACE4(mp, XFS_FSTRM_KTRACE_ASSOCIATE, ip, pip, ag, cnt)
#define TRACE_MOVEAG(mp, ip, pip, oag, ocnt, nag, ncnt) \
		TRACE6(mp, XFS_FSTRM_KTRACE_MOVEAG, ip, pip, oag, ocnt, nag, ncnt)
#define TRACE_ORPHAN(mp, ip, ag) \
		TRACE2(mp, XFS_FSTRM_KTRACE_ORPHAN, ip, ag);


#else
#define TRACE_AG_SCAN(mp, ag, ag2)
#define TRACE_AG_PICK1(mp, max_ag, maxfree)
#define TRACE_AG_PICK2(mp, ag, ag2, cnt, free, scan, flag)
#define TRACE_UPDATE(mp, ip, ag, cnt, ag2, cnt2)
#define TRACE_FREE(mp, ip, pip, ag, cnt)
#define TRACE_LOOKUP(mp, ip, pip, ag, cnt)
#define TRACE_ASSOCIATE(mp, ip, pip, ag, cnt)
#define TRACE_MOVEAG(mp, ip, pip, oag, ocnt, nag, ncnt)
#define TRACE_ORPHAN(mp, ip, ag)
#endif

static kmem_zone_t *item_zone;

typedef struct fstrm_item
{
	xfs_agnumber_t	ag;	
	xfs_inode_t	*ip;	
	xfs_inode_t	*pip;	
} fstrm_item_t;

static int
xfs_filestream_peek_ag(
	xfs_mount_t	*mp,
	xfs_agnumber_t	agno)
{
	struct xfs_perag *pag;
	int		ret;

	pag = xfs_perag_get(mp, agno);
	ret = atomic_read(&pag->pagf_fstrms);
	xfs_perag_put(pag);
	return ret;
}

static int
xfs_filestream_get_ag(
	xfs_mount_t	*mp,
	xfs_agnumber_t	agno)
{
	struct xfs_perag *pag;
	int		ret;

	pag = xfs_perag_get(mp, agno);
	ret = atomic_inc_return(&pag->pagf_fstrms);
	xfs_perag_put(pag);
	return ret;
}

static void
xfs_filestream_put_ag(
	xfs_mount_t	*mp,
	xfs_agnumber_t	agno)
{
	struct xfs_perag *pag;

	pag = xfs_perag_get(mp, agno);
	atomic_dec(&pag->pagf_fstrms);
	xfs_perag_put(pag);
}

static int
_xfs_filestream_pick_ag(
	xfs_mount_t	*mp,
	xfs_agnumber_t	startag,
	xfs_agnumber_t	*agp,
	int		flags,
	xfs_extlen_t	minlen)
{
	int		streams, max_streams;
	int		err, trylock, nscan;
	xfs_extlen_t	longest, free, minfree, maxfree = 0;
	xfs_agnumber_t	ag, max_ag = NULLAGNUMBER;
	struct xfs_perag *pag;

	
	minfree = mp->m_sb.sb_agblocks / 50;

	ag = startag;
	*agp = NULLAGNUMBER;

	
	trylock = XFS_ALLOC_FLAG_TRYLOCK;

	for (nscan = 0; 1; nscan++) {
		pag = xfs_perag_get(mp, ag);
		TRACE_AG_SCAN(mp, ag, atomic_read(&pag->pagf_fstrms));

		if (!pag->pagf_init) {
			err = xfs_alloc_pagf_init(mp, NULL, ag, trylock);
			if (err && !trylock) {
				xfs_perag_put(pag);
				return err;
			}
		}

		
		if (!pag->pagf_init)
			goto next_ag;

		
		if (pag->pagf_freeblks > maxfree) {
			maxfree = pag->pagf_freeblks;
			max_streams = atomic_read(&pag->pagf_fstrms);
			max_ag = ag;
		}

		if (xfs_filestream_get_ag(mp, ag) > 1) {
			xfs_filestream_put_ag(mp, ag);
			goto next_ag;
		}

		longest = xfs_alloc_longest_free_extent(mp, pag);
		if (((minlen && longest >= minlen) ||
		     (!minlen && pag->pagf_freeblks >= minfree)) &&
		    (!pag->pagf_metadata || !(flags & XFS_PICK_USERDATA) ||
		     (flags & XFS_PICK_LOWSPACE))) {

			
			free = pag->pagf_freeblks;
			streams = atomic_read(&pag->pagf_fstrms);
			xfs_perag_put(pag);
			*agp = ag;
			break;
		}

		
		xfs_filestream_put_ag(mp, ag);
next_ag:
		xfs_perag_put(pag);
		
		if (++ag >= mp->m_sb.sb_agcount)
			ag = 0;

		
		if (ag != startag)
			continue;

		
		if (trylock != 0) {
			trylock = 0;
			continue;
		}

		
		if (!(flags & XFS_PICK_LOWSPACE)) {
			flags |= XFS_PICK_LOWSPACE;
			continue;
		}

		if (max_ag != NULLAGNUMBER) {
			xfs_filestream_get_ag(mp, max_ag);
			TRACE_AG_PICK1(mp, max_ag, maxfree);
			streams = max_streams;
			free = maxfree;
			*agp = max_ag;
			break;
		}

		
		TRACE_AG_PICK1(mp, max_ag, maxfree);
		*agp = 0;
		return 0;
	}

	TRACE_AG_PICK2(mp, startag, *agp, streams, free, nscan, flags);

	return 0;
}

static int
_xfs_filestream_update_ag(
	xfs_inode_t	*ip,
	xfs_inode_t	*pip,
	xfs_agnumber_t	ag)
{
	int		err = 0;
	xfs_mount_t	*mp;
	xfs_mru_cache_t	*cache;
	fstrm_item_t	*item;
	xfs_agnumber_t	old_ag;
	xfs_inode_t	*old_pip;

	ASSERT(ip && ((S_ISREG(ip->i_d.di_mode) && pip &&
	               S_ISDIR(pip->i_d.di_mode)) ||
	              (S_ISDIR(ip->i_d.di_mode) && !pip)));

	mp = ip->i_mount;
	cache = mp->m_filestream;

	item = xfs_mru_cache_lookup(cache, ip->i_ino);
	if (item) {
		ASSERT(item->ip == ip);
		old_ag = item->ag;
		item->ag = ag;
		old_pip = item->pip;
		item->pip = pip;
		xfs_mru_cache_done(cache);

		if (ag != old_ag) {
			xfs_filestream_put_ag(mp, old_ag);
			xfs_filestream_get_ag(mp, ag);
		}

		if (pip && pip != old_pip) {
			IRELE(old_pip);
			IHOLD(pip);
		}

		TRACE_UPDATE(mp, ip, old_ag, xfs_filestream_peek_ag(mp, old_ag),
				ag, xfs_filestream_peek_ag(mp, ag));
		return 0;
	}

	item = kmem_zone_zalloc(item_zone, KM_MAYFAIL);
	if (!item)
		return ENOMEM;

	item->ag = ag;
	item->ip = ip;
	item->pip = pip;

	err = xfs_mru_cache_insert(cache, ip->i_ino, item);
	if (err) {
		kmem_zone_free(item_zone, item);
		return err;
	}

	
	xfs_filestream_get_ag(mp, ag);

	IHOLD(ip);

	if (pip)
		IHOLD(pip);

	TRACE_UPDATE(mp, ip, ag, xfs_filestream_peek_ag(mp, ag),
			ag, xfs_filestream_peek_ag(mp, ag));

	return 0;
}

STATIC void
xfs_fstrm_free_func(
	unsigned long	ino,
	void		*data)
{
	fstrm_item_t	*item  = (fstrm_item_t *)data;
	xfs_inode_t	*ip = item->ip;

	ASSERT(ip->i_ino == ino);

	xfs_iflags_clear(ip, XFS_IFILESTREAM);

	
	xfs_filestream_put_ag(ip->i_mount, item->ag);

	TRACE_FREE(ip->i_mount, ip, item->pip, item->ag,
		xfs_filestream_peek_ag(ip->i_mount, item->ag));

	IRELE(ip);

	if (item->pip)
		IRELE(item->pip);

	
	kmem_zone_free(item_zone, item);
}

int
xfs_filestream_init(void)
{
	item_zone = kmem_zone_init(sizeof(fstrm_item_t), "fstrm_item");
	if (!item_zone)
		return -ENOMEM;

	return 0;
}

void
xfs_filestream_uninit(void)
{
	kmem_zone_destroy(item_zone);
}

int
xfs_filestream_mount(
	xfs_mount_t	*mp)
{
	int		err;
	unsigned int	lifetime, grp_count;

	lifetime  = xfs_fstrm_centisecs * 10;
	grp_count = 10;

	err = xfs_mru_cache_create(&mp->m_filestream, lifetime, grp_count,
	                     xfs_fstrm_free_func);

	return err;
}

void
xfs_filestream_unmount(
	xfs_mount_t	*mp)
{
	xfs_mru_cache_destroy(mp->m_filestream);
}

xfs_agnumber_t
xfs_filestream_lookup_ag(
	xfs_inode_t	*ip)
{
	xfs_mru_cache_t	*cache;
	fstrm_item_t	*item;
	xfs_agnumber_t	ag;
	int		ref;

	if (!S_ISREG(ip->i_d.di_mode) && !S_ISDIR(ip->i_d.di_mode)) {
		ASSERT(0);
		return NULLAGNUMBER;
	}

	cache = ip->i_mount->m_filestream;
	item = xfs_mru_cache_lookup(cache, ip->i_ino);
	if (!item) {
		TRACE_LOOKUP(ip->i_mount, ip, NULL, NULLAGNUMBER, 0);
		return NULLAGNUMBER;
	}

	ASSERT(ip == item->ip);
	ag = item->ag;
	ref = xfs_filestream_peek_ag(ip->i_mount, ag);
	xfs_mru_cache_done(cache);

	TRACE_LOOKUP(ip->i_mount, ip, item->pip, ag, ref);
	return ag;
}

int
xfs_filestream_associate(
	xfs_inode_t	*pip,
	xfs_inode_t	*ip)
{
	xfs_mount_t	*mp;
	xfs_mru_cache_t	*cache;
	fstrm_item_t	*item;
	xfs_agnumber_t	ag, rotorstep, startag;
	int		err = 0;

	ASSERT(S_ISDIR(pip->i_d.di_mode));
	ASSERT(S_ISREG(ip->i_d.di_mode));
	if (!S_ISDIR(pip->i_d.di_mode) || !S_ISREG(ip->i_d.di_mode))
		return -EINVAL;

	mp = pip->i_mount;
	cache = mp->m_filestream;

	if (!xfs_ilock_nowait(pip, XFS_IOLOCK_EXCL))
		return 1;

	
	item = xfs_mru_cache_lookup(cache, pip->i_ino);
	if (item) {
		ASSERT(item->ip == pip);
		ag = item->ag;
		xfs_mru_cache_done(cache);

		TRACE_LOOKUP(mp, pip, pip, ag, xfs_filestream_peek_ag(mp, ag));
		err = _xfs_filestream_update_ag(ip, pip, ag);

		goto exit;
	}

	if (mp->m_flags & XFS_MOUNT_32BITINODES) {
		rotorstep = xfs_rotorstep;
		startag = (mp->m_agfrotor / rotorstep) % mp->m_sb.sb_agcount;
		mp->m_agfrotor = (mp->m_agfrotor + 1) %
		                 (mp->m_sb.sb_agcount * rotorstep);
	} else
		startag = XFS_INO_TO_AGNO(mp, pip->i_ino);

	
	err = _xfs_filestream_pick_ag(mp, startag, &ag, 0, 0);
	if (err || ag == NULLAGNUMBER)
		goto exit_did_pick;

	
	err = _xfs_filestream_update_ag(pip, NULL, ag);
	if (err)
		goto exit_did_pick;

	
	err = _xfs_filestream_update_ag(ip, pip, ag);
	if (err)
		goto exit_did_pick;

	TRACE_ASSOCIATE(mp, ip, pip, ag, xfs_filestream_peek_ag(mp, ag));

exit_did_pick:
	if (ag != NULLAGNUMBER)
		xfs_filestream_put_ag(mp, ag);

exit:
	xfs_iunlock(pip, XFS_IOLOCK_EXCL);
	return -err;
}

int
xfs_filestream_new_ag(
	xfs_bmalloca_t	*ap,
	xfs_agnumber_t	*agp)
{
	int		flags, err;
	xfs_inode_t	*ip, *pip = NULL;
	xfs_mount_t	*mp;
	xfs_mru_cache_t	*cache;
	xfs_extlen_t	minlen;
	fstrm_item_t	*dir, *file;
	xfs_agnumber_t	ag = NULLAGNUMBER;

	ip = ap->ip;
	mp = ip->i_mount;
	cache = mp->m_filestream;
	minlen = ap->length;
	*agp = NULLAGNUMBER;

	file = xfs_mru_cache_remove(cache, ip->i_ino);
	if (file) {
		ASSERT(ip == file->ip);

		
		pip = file->pip;
		ag = file->ag;

		
		dir = xfs_mru_cache_lookup(cache, pip->i_ino);
		if (dir) {
			ASSERT(pip == dir->ip);

			if (dir->ag != file->ag) {
				xfs_filestream_put_ag(mp, file->ag);
				xfs_filestream_get_ag(mp, dir->ag);
				*agp = file->ag = dir->ag;
			}

			xfs_mru_cache_done(cache);
		}

		err = xfs_mru_cache_insert(cache, ip->i_ino, file);
		if (err) {
			xfs_fstrm_free_func(ip->i_ino, file);
			return err;
		}

		if (*agp != NULLAGNUMBER) {
			TRACE_MOVEAG(mp, ip, pip,
					ag, xfs_filestream_peek_ag(mp, ag),
					*agp, xfs_filestream_peek_ag(mp, *agp));
			return 0;
		}
	}

	if (pip)
		xfs_ilock(pip, XFS_IOLOCK_EXCL | XFS_IOLOCK_PARENT);

	ag = (ag == NULLAGNUMBER) ? 0 : (ag + 1) % mp->m_sb.sb_agcount;
	flags = (ap->userdata ? XFS_PICK_USERDATA : 0) |
	        (ap->flist->xbf_low ? XFS_PICK_LOWSPACE : 0);

	err = _xfs_filestream_pick_ag(mp, ag, agp, flags, minlen);
	if (err || *agp == NULLAGNUMBER)
		goto exit;

	if (!pip) {
		TRACE_ORPHAN(mp, ip, *agp);
		goto exit;
	}

	
	err = _xfs_filestream_update_ag(pip, NULL, *agp);
	if (err)
		goto exit;

	
	err = _xfs_filestream_update_ag(ip, pip, *agp);
	if (err)
		goto exit;

	TRACE_MOVEAG(mp, ip, pip, NULLAGNUMBER, 0,
			*agp, xfs_filestream_peek_ag(mp, *agp));

exit:
	if (*agp != NULLAGNUMBER)
		xfs_filestream_put_ag(mp, *agp);
	else
		*agp = 0;

	if (pip)
		xfs_iunlock(pip, XFS_IOLOCK_EXCL);

	return err;
}

void
xfs_filestream_deassociate(
	xfs_inode_t	*ip)
{
	xfs_mru_cache_t	*cache = ip->i_mount->m_filestream;

	xfs_mru_cache_delete(cache, ip->i_ino);
}
