/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
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
#include "xfs_fs.h"
#include "xfs_types.h"
#include "xfs_bit.h"
#include "xfs_log.h"
#include "xfs_inum.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_mount.h"
#include "xfs_da_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_alloc_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_alloc.h"
#include "xfs_btree.h"
#include "xfs_attr_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_inode_item.h"
#include "xfs_bmap.h"
#include "xfs_attr.h"
#include "xfs_attr_leaf.h"
#include "xfs_error.h"
#include "xfs_trace.h"



STATIC int xfs_attr_leaf_create(xfs_da_args_t *args, xfs_dablk_t which_block,
				    xfs_dabuf_t **bpp);
STATIC int xfs_attr_leaf_add_work(xfs_dabuf_t *leaf_buffer, xfs_da_args_t *args,
					      int freemap_index);
STATIC void xfs_attr_leaf_compact(xfs_trans_t *trans, xfs_dabuf_t *leaf_buffer);
STATIC void xfs_attr_leaf_rebalance(xfs_da_state_t *state,
						   xfs_da_state_blk_t *blk1,
						   xfs_da_state_blk_t *blk2);
STATIC int xfs_attr_leaf_figure_balance(xfs_da_state_t *state,
					   xfs_da_state_blk_t *leaf_blk_1,
					   xfs_da_state_blk_t *leaf_blk_2,
					   int *number_entries_in_blk1,
					   int *number_usedbytes_in_blk1);

STATIC int xfs_attr_node_inactive(xfs_trans_t **trans, xfs_inode_t *dp,
				  xfs_dabuf_t *bp, int level);
STATIC int xfs_attr_leaf_inactive(xfs_trans_t **trans, xfs_inode_t *dp,
				  xfs_dabuf_t *bp);
STATIC int xfs_attr_leaf_freextent(xfs_trans_t **trans, xfs_inode_t *dp,
				   xfs_dablk_t blkno, int blkcnt);

STATIC void xfs_attr_leaf_moveents(xfs_attr_leafblock_t *src_leaf,
					 int src_start,
					 xfs_attr_leafblock_t *dst_leaf,
					 int dst_start, int move_count,
					 xfs_mount_t *mp);
STATIC int xfs_attr_leaf_entsize(xfs_attr_leafblock_t *leaf, int index);


STATIC int
xfs_attr_namesp_match(int arg_flags, int ondisk_flags)
{
	return XFS_ATTR_NSP_ONDISK(ondisk_flags) == XFS_ATTR_NSP_ARGS_TO_ONDISK(arg_flags);
}



int
xfs_attr_shortform_bytesfit(xfs_inode_t *dp, int bytes)
{
	int offset;
	int minforkoff;	
	int maxforkoff;	
	int dsize;
	xfs_mount_t *mp = dp->i_mount;

	offset = (XFS_LITINO(mp) - bytes) >> 3; 

	switch (dp->i_d.di_format) {
	case XFS_DINODE_FMT_DEV:
		minforkoff = roundup(sizeof(xfs_dev_t), 8) >> 3;
		return (offset >= minforkoff) ? minforkoff : 0;
	case XFS_DINODE_FMT_UUID:
		minforkoff = roundup(sizeof(uuid_t), 8) >> 3;
		return (offset >= minforkoff) ? minforkoff : 0;
	}

	if (bytes <= XFS_IFORK_ASIZE(dp))
		return dp->i_d.di_forkoff;

	if (!(mp->m_flags & XFS_MOUNT_ATTR2))
		return 0;

	dsize = dp->i_df.if_bytes;

	switch (dp->i_d.di_format) {
	case XFS_DINODE_FMT_EXTENTS:
		if (!dp->i_d.di_forkoff && dp->i_df.if_bytes >
		    xfs_default_attroffset(dp))
			dsize = XFS_BMDR_SPACE_CALC(MINDBTPTRS);
		break;
	case XFS_DINODE_FMT_BTREE:
		if (dp->i_d.di_forkoff) {
			if (offset < dp->i_d.di_forkoff)
				return 0;
			return dp->i_d.di_forkoff;
		}
		dsize = XFS_BMAP_BROOT_SPACE(dp->i_df.if_broot);
		break;
	}

	minforkoff = MAX(dsize, XFS_BMDR_SPACE_CALC(MINDBTPTRS));
	minforkoff = roundup(minforkoff, 8) >> 3;

	
	maxforkoff = XFS_LITINO(mp) - XFS_BMDR_SPACE_CALC(MINABTPTRS);
	maxforkoff = maxforkoff >> 3;	

	if (offset >= maxforkoff)
		return maxforkoff;
	if (offset >= minforkoff)
		return offset;
	return 0;
}

STATIC void
xfs_sbversion_add_attr2(xfs_mount_t *mp, xfs_trans_t *tp)
{
	if ((mp->m_flags & XFS_MOUNT_ATTR2) &&
	    !(xfs_sb_version_hasattr2(&mp->m_sb))) {
		spin_lock(&mp->m_sb_lock);
		if (!xfs_sb_version_hasattr2(&mp->m_sb)) {
			xfs_sb_version_addattr2(&mp->m_sb);
			spin_unlock(&mp->m_sb_lock);
			xfs_mod_sb(tp, XFS_SB_VERSIONNUM | XFS_SB_FEATURES2);
		} else
			spin_unlock(&mp->m_sb_lock);
	}
}

void
xfs_attr_shortform_create(xfs_da_args_t *args)
{
	xfs_attr_sf_hdr_t *hdr;
	xfs_inode_t *dp;
	xfs_ifork_t *ifp;

	trace_xfs_attr_sf_create(args);

	dp = args->dp;
	ASSERT(dp != NULL);
	ifp = dp->i_afp;
	ASSERT(ifp != NULL);
	ASSERT(ifp->if_bytes == 0);
	if (dp->i_d.di_aformat == XFS_DINODE_FMT_EXTENTS) {
		ifp->if_flags &= ~XFS_IFEXTENTS;	
		dp->i_d.di_aformat = XFS_DINODE_FMT_LOCAL;
		ifp->if_flags |= XFS_IFINLINE;
	} else {
		ASSERT(ifp->if_flags & XFS_IFINLINE);
	}
	xfs_idata_realloc(dp, sizeof(*hdr), XFS_ATTR_FORK);
	hdr = (xfs_attr_sf_hdr_t *)ifp->if_u1.if_data;
	hdr->count = 0;
	hdr->totsize = cpu_to_be16(sizeof(*hdr));
	xfs_trans_log_inode(args->trans, dp, XFS_ILOG_CORE | XFS_ILOG_ADATA);
}

void
xfs_attr_shortform_add(xfs_da_args_t *args, int forkoff)
{
	xfs_attr_shortform_t *sf;
	xfs_attr_sf_entry_t *sfe;
	int i, offset, size;
	xfs_mount_t *mp;
	xfs_inode_t *dp;
	xfs_ifork_t *ifp;

	trace_xfs_attr_sf_add(args);

	dp = args->dp;
	mp = dp->i_mount;
	dp->i_d.di_forkoff = forkoff;

	ifp = dp->i_afp;
	ASSERT(ifp->if_flags & XFS_IFINLINE);
	sf = (xfs_attr_shortform_t *)ifp->if_u1.if_data;
	sfe = &sf->list[0];
	for (i = 0; i < sf->hdr.count; sfe = XFS_ATTR_SF_NEXTENTRY(sfe), i++) {
#ifdef DEBUG
		if (sfe->namelen != args->namelen)
			continue;
		if (memcmp(args->name, sfe->nameval, args->namelen) != 0)
			continue;
		if (!xfs_attr_namesp_match(args->flags, sfe->flags))
			continue;
		ASSERT(0);
#endif
	}

	offset = (char *)sfe - (char *)sf;
	size = XFS_ATTR_SF_ENTSIZE_BYNAME(args->namelen, args->valuelen);
	xfs_idata_realloc(dp, size, XFS_ATTR_FORK);
	sf = (xfs_attr_shortform_t *)ifp->if_u1.if_data;
	sfe = (xfs_attr_sf_entry_t *)((char *)sf + offset);

	sfe->namelen = args->namelen;
	sfe->valuelen = args->valuelen;
	sfe->flags = XFS_ATTR_NSP_ARGS_TO_ONDISK(args->flags);
	memcpy(sfe->nameval, args->name, args->namelen);
	memcpy(&sfe->nameval[args->namelen], args->value, args->valuelen);
	sf->hdr.count++;
	be16_add_cpu(&sf->hdr.totsize, size);
	xfs_trans_log_inode(args->trans, dp, XFS_ILOG_CORE | XFS_ILOG_ADATA);

	xfs_sbversion_add_attr2(mp, args->trans);
}

STATIC void
xfs_attr_fork_reset(
	struct xfs_inode	*ip,
	struct xfs_trans	*tp)
{
	xfs_idestroy_fork(ip, XFS_ATTR_FORK);
	ip->i_d.di_forkoff = 0;
	ip->i_d.di_aformat = XFS_DINODE_FMT_EXTENTS;

	ASSERT(ip->i_d.di_anextents == 0);
	ASSERT(ip->i_afp == NULL);

	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
}

int
xfs_attr_shortform_remove(xfs_da_args_t *args)
{
	xfs_attr_shortform_t *sf;
	xfs_attr_sf_entry_t *sfe;
	int base, size=0, end, totsize, i;
	xfs_mount_t *mp;
	xfs_inode_t *dp;

	trace_xfs_attr_sf_remove(args);

	dp = args->dp;
	mp = dp->i_mount;
	base = sizeof(xfs_attr_sf_hdr_t);
	sf = (xfs_attr_shortform_t *)dp->i_afp->if_u1.if_data;
	sfe = &sf->list[0];
	end = sf->hdr.count;
	for (i = 0; i < end; sfe = XFS_ATTR_SF_NEXTENTRY(sfe),
					base += size, i++) {
		size = XFS_ATTR_SF_ENTSIZE(sfe);
		if (sfe->namelen != args->namelen)
			continue;
		if (memcmp(sfe->nameval, args->name, args->namelen) != 0)
			continue;
		if (!xfs_attr_namesp_match(args->flags, sfe->flags))
			continue;
		break;
	}
	if (i == end)
		return(XFS_ERROR(ENOATTR));

	end = base + size;
	totsize = be16_to_cpu(sf->hdr.totsize);
	if (end != totsize)
		memmove(&((char *)sf)[base], &((char *)sf)[end], totsize - end);
	sf->hdr.count--;
	be16_add_cpu(&sf->hdr.totsize, -size);

	totsize -= size;
	if (totsize == sizeof(xfs_attr_sf_hdr_t) &&
	    (mp->m_flags & XFS_MOUNT_ATTR2) &&
	    (dp->i_d.di_format != XFS_DINODE_FMT_BTREE) &&
	    !(args->op_flags & XFS_DA_OP_ADDNAME)) {
		xfs_attr_fork_reset(dp, args->trans);
	} else {
		xfs_idata_realloc(dp, -size, XFS_ATTR_FORK);
		dp->i_d.di_forkoff = xfs_attr_shortform_bytesfit(dp, totsize);
		ASSERT(dp->i_d.di_forkoff);
		ASSERT(totsize > sizeof(xfs_attr_sf_hdr_t) ||
				(args->op_flags & XFS_DA_OP_ADDNAME) ||
				!(mp->m_flags & XFS_MOUNT_ATTR2) ||
				dp->i_d.di_format == XFS_DINODE_FMT_BTREE);
		xfs_trans_log_inode(args->trans, dp,
					XFS_ILOG_CORE | XFS_ILOG_ADATA);
	}

	xfs_sbversion_add_attr2(mp, args->trans);

	return(0);
}

int
xfs_attr_shortform_lookup(xfs_da_args_t *args)
{
	xfs_attr_shortform_t *sf;
	xfs_attr_sf_entry_t *sfe;
	int i;
	xfs_ifork_t *ifp;

	trace_xfs_attr_sf_lookup(args);

	ifp = args->dp->i_afp;
	ASSERT(ifp->if_flags & XFS_IFINLINE);
	sf = (xfs_attr_shortform_t *)ifp->if_u1.if_data;
	sfe = &sf->list[0];
	for (i = 0; i < sf->hdr.count;
				sfe = XFS_ATTR_SF_NEXTENTRY(sfe), i++) {
		if (sfe->namelen != args->namelen)
			continue;
		if (memcmp(args->name, sfe->nameval, args->namelen) != 0)
			continue;
		if (!xfs_attr_namesp_match(args->flags, sfe->flags))
			continue;
		return(XFS_ERROR(EEXIST));
	}
	return(XFS_ERROR(ENOATTR));
}

int
xfs_attr_shortform_getvalue(xfs_da_args_t *args)
{
	xfs_attr_shortform_t *sf;
	xfs_attr_sf_entry_t *sfe;
	int i;

	ASSERT(args->dp->i_d.di_aformat == XFS_IFINLINE);
	sf = (xfs_attr_shortform_t *)args->dp->i_afp->if_u1.if_data;
	sfe = &sf->list[0];
	for (i = 0; i < sf->hdr.count;
				sfe = XFS_ATTR_SF_NEXTENTRY(sfe), i++) {
		if (sfe->namelen != args->namelen)
			continue;
		if (memcmp(args->name, sfe->nameval, args->namelen) != 0)
			continue;
		if (!xfs_attr_namesp_match(args->flags, sfe->flags))
			continue;
		if (args->flags & ATTR_KERNOVAL) {
			args->valuelen = sfe->valuelen;
			return(XFS_ERROR(EEXIST));
		}
		if (args->valuelen < sfe->valuelen) {
			args->valuelen = sfe->valuelen;
			return(XFS_ERROR(ERANGE));
		}
		args->valuelen = sfe->valuelen;
		memcpy(args->value, &sfe->nameval[args->namelen],
						    args->valuelen);
		return(XFS_ERROR(EEXIST));
	}
	return(XFS_ERROR(ENOATTR));
}

int
xfs_attr_shortform_to_leaf(xfs_da_args_t *args)
{
	xfs_inode_t *dp;
	xfs_attr_shortform_t *sf;
	xfs_attr_sf_entry_t *sfe;
	xfs_da_args_t nargs;
	char *tmpbuffer;
	int error, i, size;
	xfs_dablk_t blkno;
	xfs_dabuf_t *bp;
	xfs_ifork_t *ifp;

	trace_xfs_attr_sf_to_leaf(args);

	dp = args->dp;
	ifp = dp->i_afp;
	sf = (xfs_attr_shortform_t *)ifp->if_u1.if_data;
	size = be16_to_cpu(sf->hdr.totsize);
	tmpbuffer = kmem_alloc(size, KM_SLEEP);
	ASSERT(tmpbuffer != NULL);
	memcpy(tmpbuffer, ifp->if_u1.if_data, size);
	sf = (xfs_attr_shortform_t *)tmpbuffer;

	xfs_idata_realloc(dp, -size, XFS_ATTR_FORK);
	bp = NULL;
	error = xfs_da_grow_inode(args, &blkno);
	if (error) {
		if (error == EIO)
			goto out;
		xfs_idata_realloc(dp, size, XFS_ATTR_FORK);	
		memcpy(ifp->if_u1.if_data, tmpbuffer, size);	
		goto out;
	}

	ASSERT(blkno == 0);
	error = xfs_attr_leaf_create(args, blkno, &bp);
	if (error) {
		error = xfs_da_shrink_inode(args, 0, bp);
		bp = NULL;
		if (error)
			goto out;
		xfs_idata_realloc(dp, size, XFS_ATTR_FORK);	
		memcpy(ifp->if_u1.if_data, tmpbuffer, size);	
		goto out;
	}

	memset((char *)&nargs, 0, sizeof(nargs));
	nargs.dp = dp;
	nargs.firstblock = args->firstblock;
	nargs.flist = args->flist;
	nargs.total = args->total;
	nargs.whichfork = XFS_ATTR_FORK;
	nargs.trans = args->trans;
	nargs.op_flags = XFS_DA_OP_OKNOENT;

	sfe = &sf->list[0];
	for (i = 0; i < sf->hdr.count; i++) {
		nargs.name = sfe->nameval;
		nargs.namelen = sfe->namelen;
		nargs.value = &sfe->nameval[nargs.namelen];
		nargs.valuelen = sfe->valuelen;
		nargs.hashval = xfs_da_hashname(sfe->nameval,
						sfe->namelen);
		nargs.flags = XFS_ATTR_NSP_ONDISK_TO_ARGS(sfe->flags);
		error = xfs_attr_leaf_lookup_int(bp, &nargs); 
		ASSERT(error == ENOATTR);
		error = xfs_attr_leaf_add(bp, &nargs);
		ASSERT(error != ENOSPC);
		if (error)
			goto out;
		sfe = XFS_ATTR_SF_NEXTENTRY(sfe);
	}
	error = 0;

out:
	if(bp)
		xfs_da_buf_done(bp);
	kmem_free(tmpbuffer);
	return(error);
}

STATIC int
xfs_attr_shortform_compare(const void *a, const void *b)
{
	xfs_attr_sf_sort_t *sa, *sb;

	sa = (xfs_attr_sf_sort_t *)a;
	sb = (xfs_attr_sf_sort_t *)b;
	if (sa->hash < sb->hash) {
		return(-1);
	} else if (sa->hash > sb->hash) {
		return(1);
	} else {
		return(sa->entno - sb->entno);
	}
}


#define XFS_ISRESET_CURSOR(cursor) \
	(!((cursor)->initted) && !((cursor)->hashval) && \
	 !((cursor)->blkno) && !((cursor)->offset))
int
xfs_attr_shortform_list(xfs_attr_list_context_t *context)
{
	attrlist_cursor_kern_t *cursor;
	xfs_attr_sf_sort_t *sbuf, *sbp;
	xfs_attr_shortform_t *sf;
	xfs_attr_sf_entry_t *sfe;
	xfs_inode_t *dp;
	int sbsize, nsbuf, count, i;
	int error;

	ASSERT(context != NULL);
	dp = context->dp;
	ASSERT(dp != NULL);
	ASSERT(dp->i_afp != NULL);
	sf = (xfs_attr_shortform_t *)dp->i_afp->if_u1.if_data;
	ASSERT(sf != NULL);
	if (!sf->hdr.count)
		return(0);
	cursor = context->cursor;
	ASSERT(cursor != NULL);

	trace_xfs_attr_list_sf(context);

	if (context->bufsize == 0 ||
	    (XFS_ISRESET_CURSOR(cursor) &&
             (dp->i_afp->if_bytes + sf->hdr.count * 16) < context->bufsize)) {
		for (i = 0, sfe = &sf->list[0]; i < sf->hdr.count; i++) {
			error = context->put_listent(context,
					   sfe->flags,
					   sfe->nameval,
					   (int)sfe->namelen,
					   (int)sfe->valuelen,
					   &sfe->nameval[sfe->namelen]);

			if (context->seen_enough)
				break;

			if (error)
				return error;
			sfe = XFS_ATTR_SF_NEXTENTRY(sfe);
		}
		trace_xfs_attr_list_sf_all(context);
		return(0);
	}

	
	if (context->bufsize == 0)
		return 0;

	sbsize = sf->hdr.count * sizeof(*sbuf);
	sbp = sbuf = kmem_alloc(sbsize, KM_SLEEP | KM_NOFS);

	nsbuf = 0;
	for (i = 0, sfe = &sf->list[0]; i < sf->hdr.count; i++) {
		if (unlikely(
		    ((char *)sfe < (char *)sf) ||
		    ((char *)sfe >= ((char *)sf + dp->i_afp->if_bytes)))) {
			XFS_CORRUPTION_ERROR("xfs_attr_shortform_list",
					     XFS_ERRLEVEL_LOW,
					     context->dp->i_mount, sfe);
			kmem_free(sbuf);
			return XFS_ERROR(EFSCORRUPTED);
		}

		sbp->entno = i;
		sbp->hash = xfs_da_hashname(sfe->nameval, sfe->namelen);
		sbp->name = sfe->nameval;
		sbp->namelen = sfe->namelen;
		
		sbp->valuelen = sfe->valuelen;
		sbp->flags = sfe->flags;
		sfe = XFS_ATTR_SF_NEXTENTRY(sfe);
		sbp++;
		nsbuf++;
	}

	xfs_sort(sbuf, nsbuf, sizeof(*sbuf), xfs_attr_shortform_compare);

	count = 0;
	cursor->initted = 1;
	cursor->blkno = 0;
	for (sbp = sbuf, i = 0; i < nsbuf; i++, sbp++) {
		if (sbp->hash == cursor->hashval) {
			if (cursor->offset == count) {
				break;
			}
			count++;
		} else if (sbp->hash > cursor->hashval) {
			break;
		}
	}
	if (i == nsbuf) {
		kmem_free(sbuf);
		return(0);
	}

	for ( ; i < nsbuf; i++, sbp++) {
		if (cursor->hashval != sbp->hash) {
			cursor->hashval = sbp->hash;
			cursor->offset = 0;
		}
		error = context->put_listent(context,
					sbp->flags,
					sbp->name,
					sbp->namelen,
					sbp->valuelen,
					&sbp->name[sbp->namelen]);
		if (error)
			return error;
		if (context->seen_enough)
			break;
		cursor->offset++;
	}

	kmem_free(sbuf);
	return(0);
}

int
xfs_attr_shortform_allfit(xfs_dabuf_t *bp, xfs_inode_t *dp)
{
	xfs_attr_leafblock_t *leaf;
	xfs_attr_leaf_entry_t *entry;
	xfs_attr_leaf_name_local_t *name_loc;
	int bytes, i;

	leaf = bp->data;
	ASSERT(leaf->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));

	entry = &leaf->entries[0];
	bytes = sizeof(struct xfs_attr_sf_hdr);
	for (i = 0; i < be16_to_cpu(leaf->hdr.count); entry++, i++) {
		if (entry->flags & XFS_ATTR_INCOMPLETE)
			continue;		
		if (!(entry->flags & XFS_ATTR_LOCAL))
			return(0);
		name_loc = xfs_attr_leaf_name_local(leaf, i);
		if (name_loc->namelen >= XFS_ATTR_SF_ENTSIZE_MAX)
			return(0);
		if (be16_to_cpu(name_loc->valuelen) >= XFS_ATTR_SF_ENTSIZE_MAX)
			return(0);
		bytes += sizeof(struct xfs_attr_sf_entry)-1
				+ name_loc->namelen
				+ be16_to_cpu(name_loc->valuelen);
	}
	if ((dp->i_mount->m_flags & XFS_MOUNT_ATTR2) &&
	    (dp->i_d.di_format != XFS_DINODE_FMT_BTREE) &&
	    (bytes == sizeof(struct xfs_attr_sf_hdr)))
		return(-1);
	return(xfs_attr_shortform_bytesfit(dp, bytes));
}

int
xfs_attr_leaf_to_shortform(xfs_dabuf_t *bp, xfs_da_args_t *args, int forkoff)
{
	xfs_attr_leafblock_t *leaf;
	xfs_attr_leaf_entry_t *entry;
	xfs_attr_leaf_name_local_t *name_loc;
	xfs_da_args_t nargs;
	xfs_inode_t *dp;
	char *tmpbuffer;
	int error, i;

	trace_xfs_attr_leaf_to_sf(args);

	dp = args->dp;
	tmpbuffer = kmem_alloc(XFS_LBSIZE(dp->i_mount), KM_SLEEP);
	ASSERT(tmpbuffer != NULL);

	ASSERT(bp != NULL);
	memcpy(tmpbuffer, bp->data, XFS_LBSIZE(dp->i_mount));
	leaf = (xfs_attr_leafblock_t *)tmpbuffer;
	ASSERT(leaf->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));
	memset(bp->data, 0, XFS_LBSIZE(dp->i_mount));

	error = xfs_da_shrink_inode(args, 0, bp);
	if (error)
		goto out;

	if (forkoff == -1) {
		ASSERT(dp->i_mount->m_flags & XFS_MOUNT_ATTR2);
		ASSERT(dp->i_d.di_format != XFS_DINODE_FMT_BTREE);
		xfs_attr_fork_reset(dp, args->trans);
		goto out;
	}

	xfs_attr_shortform_create(args);

	memset((char *)&nargs, 0, sizeof(nargs));
	nargs.dp = dp;
	nargs.firstblock = args->firstblock;
	nargs.flist = args->flist;
	nargs.total = args->total;
	nargs.whichfork = XFS_ATTR_FORK;
	nargs.trans = args->trans;
	nargs.op_flags = XFS_DA_OP_OKNOENT;
	entry = &leaf->entries[0];
	for (i = 0; i < be16_to_cpu(leaf->hdr.count); entry++, i++) {
		if (entry->flags & XFS_ATTR_INCOMPLETE)
			continue;	
		if (!entry->nameidx)
			continue;
		ASSERT(entry->flags & XFS_ATTR_LOCAL);
		name_loc = xfs_attr_leaf_name_local(leaf, i);
		nargs.name = name_loc->nameval;
		nargs.namelen = name_loc->namelen;
		nargs.value = &name_loc->nameval[nargs.namelen];
		nargs.valuelen = be16_to_cpu(name_loc->valuelen);
		nargs.hashval = be32_to_cpu(entry->hashval);
		nargs.flags = XFS_ATTR_NSP_ONDISK_TO_ARGS(entry->flags);
		xfs_attr_shortform_add(&nargs, forkoff);
	}
	error = 0;

out:
	kmem_free(tmpbuffer);
	return(error);
}

int
xfs_attr_leaf_to_node(xfs_da_args_t *args)
{
	xfs_attr_leafblock_t *leaf;
	xfs_da_intnode_t *node;
	xfs_inode_t *dp;
	xfs_dabuf_t *bp1, *bp2;
	xfs_dablk_t blkno;
	int error;

	trace_xfs_attr_leaf_to_node(args);

	dp = args->dp;
	bp1 = bp2 = NULL;
	error = xfs_da_grow_inode(args, &blkno);
	if (error)
		goto out;
	error = xfs_da_read_buf(args->trans, args->dp, 0, -1, &bp1,
					     XFS_ATTR_FORK);
	if (error)
		goto out;
	ASSERT(bp1 != NULL);
	bp2 = NULL;
	error = xfs_da_get_buf(args->trans, args->dp, blkno, -1, &bp2,
					    XFS_ATTR_FORK);
	if (error)
		goto out;
	ASSERT(bp2 != NULL);
	memcpy(bp2->data, bp1->data, XFS_LBSIZE(dp->i_mount));
	xfs_da_buf_done(bp1);
	bp1 = NULL;
	xfs_da_log_buf(args->trans, bp2, 0, XFS_LBSIZE(dp->i_mount) - 1);

	error = xfs_da_node_create(args, 0, 1, &bp1, XFS_ATTR_FORK);
	if (error)
		goto out;
	node = bp1->data;
	leaf = bp2->data;
	ASSERT(leaf->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));
	
	node->btree[0].hashval =
		leaf->entries[be16_to_cpu(leaf->hdr.count)-1 ].hashval;
	node->btree[0].before = cpu_to_be32(blkno);
	node->hdr.count = cpu_to_be16(1);
	xfs_da_log_buf(args->trans, bp1, 0, XFS_LBSIZE(dp->i_mount) - 1);
	error = 0;
out:
	if (bp1)
		xfs_da_buf_done(bp1);
	if (bp2)
		xfs_da_buf_done(bp2);
	return(error);
}



STATIC int
xfs_attr_leaf_create(xfs_da_args_t *args, xfs_dablk_t blkno, xfs_dabuf_t **bpp)
{
	xfs_attr_leafblock_t *leaf;
	xfs_attr_leaf_hdr_t *hdr;
	xfs_inode_t *dp;
	xfs_dabuf_t *bp;
	int error;

	trace_xfs_attr_leaf_create(args);

	dp = args->dp;
	ASSERT(dp != NULL);
	error = xfs_da_get_buf(args->trans, args->dp, blkno, -1, &bp,
					    XFS_ATTR_FORK);
	if (error)
		return(error);
	ASSERT(bp != NULL);
	leaf = bp->data;
	memset((char *)leaf, 0, XFS_LBSIZE(dp->i_mount));
	hdr = &leaf->hdr;
	hdr->info.magic = cpu_to_be16(XFS_ATTR_LEAF_MAGIC);
	hdr->firstused = cpu_to_be16(XFS_LBSIZE(dp->i_mount));
	if (!hdr->firstused) {
		hdr->firstused = cpu_to_be16(
			XFS_LBSIZE(dp->i_mount) - XFS_ATTR_LEAF_NAME_ALIGN);
	}

	hdr->freemap[0].base = cpu_to_be16(sizeof(xfs_attr_leaf_hdr_t));
	hdr->freemap[0].size = cpu_to_be16(be16_to_cpu(hdr->firstused) -
					   sizeof(xfs_attr_leaf_hdr_t));

	xfs_da_log_buf(args->trans, bp, 0, XFS_LBSIZE(dp->i_mount) - 1);

	*bpp = bp;
	return(0);
}

int
xfs_attr_leaf_split(xfs_da_state_t *state, xfs_da_state_blk_t *oldblk,
				   xfs_da_state_blk_t *newblk)
{
	xfs_dablk_t blkno;
	int error;

	trace_xfs_attr_leaf_split(state->args);

	ASSERT(oldblk->magic == XFS_ATTR_LEAF_MAGIC);
	error = xfs_da_grow_inode(state->args, &blkno);
	if (error)
		return(error);
	error = xfs_attr_leaf_create(state->args, blkno, &newblk->bp);
	if (error)
		return(error);
	newblk->blkno = blkno;
	newblk->magic = XFS_ATTR_LEAF_MAGIC;

	xfs_attr_leaf_rebalance(state, oldblk, newblk);
	error = xfs_da_blk_link(state, oldblk, newblk);
	if (error)
		return(error);

	if (state->inleaf) {
		trace_xfs_attr_leaf_add_old(state->args);
		error = xfs_attr_leaf_add(oldblk->bp, state->args);
	} else {
		trace_xfs_attr_leaf_add_new(state->args);
		error = xfs_attr_leaf_add(newblk->bp, state->args);
	}

	oldblk->hashval = xfs_attr_leaf_lasthash(oldblk->bp, NULL);
	newblk->hashval = xfs_attr_leaf_lasthash(newblk->bp, NULL);
	return(error);
}

int
xfs_attr_leaf_add(xfs_dabuf_t *bp, xfs_da_args_t *args)
{
	xfs_attr_leafblock_t *leaf;
	xfs_attr_leaf_hdr_t *hdr;
	xfs_attr_leaf_map_t *map;
	int tablesize, entsize, sum, tmp, i;

	trace_xfs_attr_leaf_add(args);

	leaf = bp->data;
	ASSERT(leaf->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));
	ASSERT((args->index >= 0)
		&& (args->index <= be16_to_cpu(leaf->hdr.count)));
	hdr = &leaf->hdr;
	entsize = xfs_attr_leaf_newentsize(args->namelen, args->valuelen,
			   args->trans->t_mountp->m_sb.sb_blocksize, NULL);

	tablesize = (be16_to_cpu(hdr->count) + 1)
					* sizeof(xfs_attr_leaf_entry_t)
					+ sizeof(xfs_attr_leaf_hdr_t);
	map = &hdr->freemap[XFS_ATTR_LEAF_MAPSIZE-1];
	for (sum = 0, i = XFS_ATTR_LEAF_MAPSIZE-1; i >= 0; map--, i--) {
		if (tablesize > be16_to_cpu(hdr->firstused)) {
			sum += be16_to_cpu(map->size);
			continue;
		}
		if (!map->size)
			continue;	
		tmp = entsize;
		if (be16_to_cpu(map->base) < be16_to_cpu(hdr->firstused))
			tmp += sizeof(xfs_attr_leaf_entry_t);
		if (be16_to_cpu(map->size) >= tmp) {
			tmp = xfs_attr_leaf_add_work(bp, args, i);
			return(tmp);
		}
		sum += be16_to_cpu(map->size);
	}

	if (!hdr->holes && (sum < entsize))
		return(XFS_ERROR(ENOSPC));

	xfs_attr_leaf_compact(args->trans, bp);

	if (be16_to_cpu(hdr->freemap[0].size)
				< (entsize + sizeof(xfs_attr_leaf_entry_t)))
		return(XFS_ERROR(ENOSPC));

	return(xfs_attr_leaf_add_work(bp, args, 0));
}

STATIC int
xfs_attr_leaf_add_work(xfs_dabuf_t *bp, xfs_da_args_t *args, int mapindex)
{
	xfs_attr_leafblock_t *leaf;
	xfs_attr_leaf_hdr_t *hdr;
	xfs_attr_leaf_entry_t *entry;
	xfs_attr_leaf_name_local_t *name_loc;
	xfs_attr_leaf_name_remote_t *name_rmt;
	xfs_attr_leaf_map_t *map;
	xfs_mount_t *mp;
	int tmp, i;

	leaf = bp->data;
	ASSERT(leaf->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));
	hdr = &leaf->hdr;
	ASSERT((mapindex >= 0) && (mapindex < XFS_ATTR_LEAF_MAPSIZE));
	ASSERT((args->index >= 0) && (args->index <= be16_to_cpu(hdr->count)));

	entry = &leaf->entries[args->index];
	if (args->index < be16_to_cpu(hdr->count)) {
		tmp  = be16_to_cpu(hdr->count) - args->index;
		tmp *= sizeof(xfs_attr_leaf_entry_t);
		memmove((char *)(entry+1), (char *)entry, tmp);
		xfs_da_log_buf(args->trans, bp,
		    XFS_DA_LOGRANGE(leaf, entry, tmp + sizeof(*entry)));
	}
	be16_add_cpu(&hdr->count, 1);

	map = &hdr->freemap[mapindex];
	mp = args->trans->t_mountp;
	ASSERT(be16_to_cpu(map->base) < XFS_LBSIZE(mp));
	ASSERT((be16_to_cpu(map->base) & 0x3) == 0);
	ASSERT(be16_to_cpu(map->size) >=
		xfs_attr_leaf_newentsize(args->namelen, args->valuelen,
					 mp->m_sb.sb_blocksize, NULL));
	ASSERT(be16_to_cpu(map->size) < XFS_LBSIZE(mp));
	ASSERT((be16_to_cpu(map->size) & 0x3) == 0);
	be16_add_cpu(&map->size,
		-xfs_attr_leaf_newentsize(args->namelen, args->valuelen,
					  mp->m_sb.sb_blocksize, &tmp));
	entry->nameidx = cpu_to_be16(be16_to_cpu(map->base) +
				     be16_to_cpu(map->size));
	entry->hashval = cpu_to_be32(args->hashval);
	entry->flags = tmp ? XFS_ATTR_LOCAL : 0;
	entry->flags |= XFS_ATTR_NSP_ARGS_TO_ONDISK(args->flags);
	if (args->op_flags & XFS_DA_OP_RENAME) {
		entry->flags |= XFS_ATTR_INCOMPLETE;
		if ((args->blkno2 == args->blkno) &&
		    (args->index2 <= args->index)) {
			args->index2++;
		}
	}
	xfs_da_log_buf(args->trans, bp,
			  XFS_DA_LOGRANGE(leaf, entry, sizeof(*entry)));
	ASSERT((args->index == 0) ||
	       (be32_to_cpu(entry->hashval) >= be32_to_cpu((entry-1)->hashval)));
	ASSERT((args->index == be16_to_cpu(hdr->count)-1) ||
	       (be32_to_cpu(entry->hashval) <= be32_to_cpu((entry+1)->hashval)));

	if (entry->flags & XFS_ATTR_LOCAL) {
		name_loc = xfs_attr_leaf_name_local(leaf, args->index);
		name_loc->namelen = args->namelen;
		name_loc->valuelen = cpu_to_be16(args->valuelen);
		memcpy((char *)name_loc->nameval, args->name, args->namelen);
		memcpy((char *)&name_loc->nameval[args->namelen], args->value,
				   be16_to_cpu(name_loc->valuelen));
	} else {
		name_rmt = xfs_attr_leaf_name_remote(leaf, args->index);
		name_rmt->namelen = args->namelen;
		memcpy((char *)name_rmt->name, args->name, args->namelen);
		entry->flags |= XFS_ATTR_INCOMPLETE;
		
		name_rmt->valuelen = 0;
		name_rmt->valueblk = 0;
		args->rmtblkno = 1;
		args->rmtblkcnt = XFS_B_TO_FSB(mp, args->valuelen);
	}
	xfs_da_log_buf(args->trans, bp,
	     XFS_DA_LOGRANGE(leaf, xfs_attr_leaf_name(leaf, args->index),
				   xfs_attr_leaf_entsize(leaf, args->index)));

	if (be16_to_cpu(entry->nameidx) < be16_to_cpu(hdr->firstused)) {
		
		hdr->firstused = entry->nameidx;
	}
	ASSERT(be16_to_cpu(hdr->firstused) >=
	       ((be16_to_cpu(hdr->count) * sizeof(*entry)) + sizeof(*hdr)));
	tmp = (be16_to_cpu(hdr->count)-1) * sizeof(xfs_attr_leaf_entry_t)
					+ sizeof(xfs_attr_leaf_hdr_t);
	map = &hdr->freemap[0];
	for (i = 0; i < XFS_ATTR_LEAF_MAPSIZE; map++, i++) {
		if (be16_to_cpu(map->base) == tmp) {
			be16_add_cpu(&map->base, sizeof(xfs_attr_leaf_entry_t));
			be16_add_cpu(&map->size,
				 -((int)sizeof(xfs_attr_leaf_entry_t)));
		}
	}
	be16_add_cpu(&hdr->usedbytes, xfs_attr_leaf_entsize(leaf, args->index));
	xfs_da_log_buf(args->trans, bp,
		XFS_DA_LOGRANGE(leaf, hdr, sizeof(*hdr)));
	return(0);
}

STATIC void
xfs_attr_leaf_compact(xfs_trans_t *trans, xfs_dabuf_t *bp)
{
	xfs_attr_leafblock_t *leaf_s, *leaf_d;
	xfs_attr_leaf_hdr_t *hdr_s, *hdr_d;
	xfs_mount_t *mp;
	char *tmpbuffer;

	mp = trans->t_mountp;
	tmpbuffer = kmem_alloc(XFS_LBSIZE(mp), KM_SLEEP);
	ASSERT(tmpbuffer != NULL);
	memcpy(tmpbuffer, bp->data, XFS_LBSIZE(mp));
	memset(bp->data, 0, XFS_LBSIZE(mp));

	leaf_s = (xfs_attr_leafblock_t *)tmpbuffer;
	leaf_d = bp->data;
	hdr_s = &leaf_s->hdr;
	hdr_d = &leaf_d->hdr;
	hdr_d->info = hdr_s->info;	
	hdr_d->firstused = cpu_to_be16(XFS_LBSIZE(mp));
	
	if (!hdr_d->firstused) {
		hdr_d->firstused = cpu_to_be16(
				XFS_LBSIZE(mp) - XFS_ATTR_LEAF_NAME_ALIGN);
	}
	hdr_d->usedbytes = 0;
	hdr_d->count = 0;
	hdr_d->holes = 0;
	hdr_d->freemap[0].base = cpu_to_be16(sizeof(xfs_attr_leaf_hdr_t));
	hdr_d->freemap[0].size = cpu_to_be16(be16_to_cpu(hdr_d->firstused) -
					     sizeof(xfs_attr_leaf_hdr_t));

	xfs_attr_leaf_moveents(leaf_s, 0, leaf_d, 0,
				be16_to_cpu(hdr_s->count), mp);
	xfs_da_log_buf(trans, bp, 0, XFS_LBSIZE(mp) - 1);

	kmem_free(tmpbuffer);
}

STATIC void
xfs_attr_leaf_rebalance(xfs_da_state_t *state, xfs_da_state_blk_t *blk1,
				       xfs_da_state_blk_t *blk2)
{
	xfs_da_args_t *args;
	xfs_da_state_blk_t *tmp_blk;
	xfs_attr_leafblock_t *leaf1, *leaf2;
	xfs_attr_leaf_hdr_t *hdr1, *hdr2;
	int count, totallen, max, space, swap;

	ASSERT(blk1->magic == XFS_ATTR_LEAF_MAGIC);
	ASSERT(blk2->magic == XFS_ATTR_LEAF_MAGIC);
	leaf1 = blk1->bp->data;
	leaf2 = blk2->bp->data;
	ASSERT(leaf1->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));
	ASSERT(leaf2->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));
	args = state->args;

	trace_xfs_attr_leaf_rebalance(args);

	swap = 0;
	if (xfs_attr_leaf_order(blk1->bp, blk2->bp)) {
		tmp_blk = blk1;
		blk1 = blk2;
		blk2 = tmp_blk;
		leaf1 = blk1->bp->data;
		leaf2 = blk2->bp->data;
		swap = 1;
	}
	hdr1 = &leaf1->hdr;
	hdr2 = &leaf2->hdr;

	state->inleaf = xfs_attr_leaf_figure_balance(state, blk1, blk2,
							    &count, &totallen);
	if (swap)
		state->inleaf = !state->inleaf;

	if (count < be16_to_cpu(hdr1->count)) {
		
		count = be16_to_cpu(hdr1->count) - count;
		space  = be16_to_cpu(hdr1->usedbytes) - totallen;
		space += count * sizeof(xfs_attr_leaf_entry_t);

		max  = be16_to_cpu(hdr2->firstused)
						- sizeof(xfs_attr_leaf_hdr_t);
		max -= be16_to_cpu(hdr2->count) * sizeof(xfs_attr_leaf_entry_t);
		if (space > max) {
			xfs_attr_leaf_compact(args->trans, blk2->bp);
		}

		xfs_attr_leaf_moveents(leaf1, be16_to_cpu(hdr1->count) - count,
				leaf2, 0, count, state->mp);

		xfs_da_log_buf(args->trans, blk1->bp, 0, state->blocksize-1);
		xfs_da_log_buf(args->trans, blk2->bp, 0, state->blocksize-1);
	} else if (count > be16_to_cpu(hdr1->count)) {

		
		count -= be16_to_cpu(hdr1->count);
		space  = totallen - be16_to_cpu(hdr1->usedbytes);
		space += count * sizeof(xfs_attr_leaf_entry_t);

		max  = be16_to_cpu(hdr1->firstused)
						- sizeof(xfs_attr_leaf_hdr_t);
		max -= be16_to_cpu(hdr1->count) * sizeof(xfs_attr_leaf_entry_t);
		if (space > max) {
			xfs_attr_leaf_compact(args->trans, blk1->bp);
		}

		xfs_attr_leaf_moveents(leaf2, 0, leaf1,
				be16_to_cpu(hdr1->count), count, state->mp);

		xfs_da_log_buf(args->trans, blk1->bp, 0, state->blocksize-1);
		xfs_da_log_buf(args->trans, blk2->bp, 0, state->blocksize-1);
	}

	blk1->hashval = be32_to_cpu(
		leaf1->entries[be16_to_cpu(leaf1->hdr.count)-1].hashval);
	blk2->hashval = be32_to_cpu(
		leaf2->entries[be16_to_cpu(leaf2->hdr.count)-1].hashval);

	if (blk1->index > be16_to_cpu(leaf1->hdr.count)) {
		ASSERT(state->inleaf == 0);
		blk2->index = blk1->index - be16_to_cpu(leaf1->hdr.count);
		args->index = args->index2 = blk2->index;
		args->blkno = args->blkno2 = blk2->blkno;
	} else if (blk1->index == be16_to_cpu(leaf1->hdr.count)) {
		if (state->inleaf) {
			args->index = blk1->index;
			args->blkno = blk1->blkno;
			args->index2 = 0;
			args->blkno2 = blk2->blkno;
		} else {
			blk2->index = blk1->index
				    - be16_to_cpu(leaf1->hdr.count);
			args->index = args->index2 = blk2->index;
			args->blkno = args->blkno2 = blk2->blkno;
		}
	} else {
		ASSERT(state->inleaf == 1);
		args->index = args->index2 = blk1->index;
		args->blkno = args->blkno2 = blk1->blkno;
	}
}

STATIC int
xfs_attr_leaf_figure_balance(xfs_da_state_t *state,
				    xfs_da_state_blk_t *blk1,
				    xfs_da_state_blk_t *blk2,
				    int *countarg, int *usedbytesarg)
{
	xfs_attr_leafblock_t *leaf1, *leaf2;
	xfs_attr_leaf_hdr_t *hdr1, *hdr2;
	xfs_attr_leaf_entry_t *entry;
	int count, max, index, totallen, half;
	int lastdelta, foundit, tmp;

	leaf1 = blk1->bp->data;
	leaf2 = blk2->bp->data;
	hdr1 = &leaf1->hdr;
	hdr2 = &leaf2->hdr;
	foundit = 0;
	totallen = 0;

	max = be16_to_cpu(hdr1->count) + be16_to_cpu(hdr2->count);
	half  = (max+1) * sizeof(*entry);
	half += be16_to_cpu(hdr1->usedbytes) +
		be16_to_cpu(hdr2->usedbytes) +
		xfs_attr_leaf_newentsize(
				state->args->namelen,
				state->args->valuelen,
				state->blocksize, NULL);
	half /= 2;
	lastdelta = state->blocksize;
	entry = &leaf1->entries[0];
	for (count = index = 0; count < max; entry++, index++, count++) {

#define XFS_ATTR_ABS(A)	(((A) < 0) ? -(A) : (A))
		if (count == blk1->index) {
			tmp = totallen + sizeof(*entry) +
				xfs_attr_leaf_newentsize(
						state->args->namelen,
						state->args->valuelen,
						state->blocksize, NULL);
			if (XFS_ATTR_ABS(half - tmp) > lastdelta)
				break;
			lastdelta = XFS_ATTR_ABS(half - tmp);
			totallen = tmp;
			foundit = 1;
		}

		if (count == be16_to_cpu(hdr1->count)) {
			leaf1 = leaf2;
			entry = &leaf1->entries[0];
			index = 0;
		}

		tmp = totallen + sizeof(*entry) + xfs_attr_leaf_entsize(leaf1,
									index);
		if (XFS_ATTR_ABS(half - tmp) > lastdelta)
			break;
		lastdelta = XFS_ATTR_ABS(half - tmp);
		totallen = tmp;
#undef XFS_ATTR_ABS
	}

	totallen -= count * sizeof(*entry);
	if (foundit) {
		totallen -= sizeof(*entry) +
				xfs_attr_leaf_newentsize(
						state->args->namelen,
						state->args->valuelen,
						state->blocksize, NULL);
	}

	*countarg = count;
	*usedbytesarg = totallen;
	return(foundit);
}


int
xfs_attr_leaf_toosmall(xfs_da_state_t *state, int *action)
{
	xfs_attr_leafblock_t *leaf;
	xfs_da_state_blk_t *blk;
	xfs_da_blkinfo_t *info;
	int count, bytes, forward, error, retval, i;
	xfs_dablk_t blkno;
	xfs_dabuf_t *bp;

	blk = &state->path.blk[ state->path.active-1 ];
	info = blk->bp->data;
	ASSERT(info->magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));
	leaf = (xfs_attr_leafblock_t *)info;
	count = be16_to_cpu(leaf->hdr.count);
	bytes = sizeof(xfs_attr_leaf_hdr_t) +
		count * sizeof(xfs_attr_leaf_entry_t) +
		be16_to_cpu(leaf->hdr.usedbytes);
	if (bytes > (state->blocksize >> 1)) {
		*action = 0;	
		return(0);
	}

	if (count == 0) {
		forward = (info->forw != 0);
		memcpy(&state->altpath, &state->path, sizeof(state->path));
		error = xfs_da_path_shift(state, &state->altpath, forward,
						 0, &retval);
		if (error)
			return(error);
		if (retval) {
			*action = 0;
		} else {
			*action = 2;
		}
		return(0);
	}

	
	forward = (be32_to_cpu(info->forw) < be32_to_cpu(info->back));
	for (i = 0; i < 2; forward = !forward, i++) {
		if (forward)
			blkno = be32_to_cpu(info->forw);
		else
			blkno = be32_to_cpu(info->back);
		if (blkno == 0)
			continue;
		error = xfs_da_read_buf(state->args->trans, state->args->dp,
					blkno, -1, &bp, XFS_ATTR_FORK);
		if (error)
			return(error);
		ASSERT(bp != NULL);

		leaf = (xfs_attr_leafblock_t *)info;
		count  = be16_to_cpu(leaf->hdr.count);
		bytes  = state->blocksize - (state->blocksize>>2);
		bytes -= be16_to_cpu(leaf->hdr.usedbytes);
		leaf = bp->data;
		ASSERT(leaf->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));
		count += be16_to_cpu(leaf->hdr.count);
		bytes -= be16_to_cpu(leaf->hdr.usedbytes);
		bytes -= count * sizeof(xfs_attr_leaf_entry_t);
		bytes -= sizeof(xfs_attr_leaf_hdr_t);
		xfs_da_brelse(state->args->trans, bp);
		if (bytes >= 0)
			break;	
	}
	if (i >= 2) {
		*action = 0;
		return(0);
	}

	memcpy(&state->altpath, &state->path, sizeof(state->path));
	if (blkno < blk->blkno) {
		error = xfs_da_path_shift(state, &state->altpath, forward,
						 0, &retval);
	} else {
		error = xfs_da_path_shift(state, &state->path, forward,
						 0, &retval);
	}
	if (error)
		return(error);
	if (retval) {
		*action = 0;
	} else {
		*action = 1;
	}
	return(0);
}

int
xfs_attr_leaf_remove(xfs_dabuf_t *bp, xfs_da_args_t *args)
{
	xfs_attr_leafblock_t *leaf;
	xfs_attr_leaf_hdr_t *hdr;
	xfs_attr_leaf_map_t *map;
	xfs_attr_leaf_entry_t *entry;
	int before, after, smallest, entsize;
	int tablesize, tmp, i;
	xfs_mount_t *mp;

	leaf = bp->data;
	ASSERT(leaf->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));
	hdr = &leaf->hdr;
	mp = args->trans->t_mountp;
	ASSERT((be16_to_cpu(hdr->count) > 0)
		&& (be16_to_cpu(hdr->count) < (XFS_LBSIZE(mp)/8)));
	ASSERT((args->index >= 0)
		&& (args->index < be16_to_cpu(hdr->count)));
	ASSERT(be16_to_cpu(hdr->firstused) >=
	       ((be16_to_cpu(hdr->count) * sizeof(*entry)) + sizeof(*hdr)));
	entry = &leaf->entries[args->index];
	ASSERT(be16_to_cpu(entry->nameidx) >= be16_to_cpu(hdr->firstused));
	ASSERT(be16_to_cpu(entry->nameidx) < XFS_LBSIZE(mp));

	tablesize = be16_to_cpu(hdr->count) * sizeof(xfs_attr_leaf_entry_t)
					+ sizeof(xfs_attr_leaf_hdr_t);
	map = &hdr->freemap[0];
	tmp = be16_to_cpu(map->size);
	before = after = -1;
	smallest = XFS_ATTR_LEAF_MAPSIZE - 1;
	entsize = xfs_attr_leaf_entsize(leaf, args->index);
	for (i = 0; i < XFS_ATTR_LEAF_MAPSIZE; map++, i++) {
		ASSERT(be16_to_cpu(map->base) < XFS_LBSIZE(mp));
		ASSERT(be16_to_cpu(map->size) < XFS_LBSIZE(mp));
		if (be16_to_cpu(map->base) == tablesize) {
			be16_add_cpu(&map->base,
				 -((int)sizeof(xfs_attr_leaf_entry_t)));
			be16_add_cpu(&map->size, sizeof(xfs_attr_leaf_entry_t));
		}

		if ((be16_to_cpu(map->base) + be16_to_cpu(map->size))
				== be16_to_cpu(entry->nameidx)) {
			before = i;
		} else if (be16_to_cpu(map->base)
			== (be16_to_cpu(entry->nameidx) + entsize)) {
			after = i;
		} else if (be16_to_cpu(map->size) < tmp) {
			tmp = be16_to_cpu(map->size);
			smallest = i;
		}
	}

	if ((before >= 0) || (after >= 0)) {
		if ((before >= 0) && (after >= 0)) {
			map = &hdr->freemap[before];
			be16_add_cpu(&map->size, entsize);
			be16_add_cpu(&map->size,
				 be16_to_cpu(hdr->freemap[after].size));
			hdr->freemap[after].base = 0;
			hdr->freemap[after].size = 0;
		} else if (before >= 0) {
			map = &hdr->freemap[before];
			be16_add_cpu(&map->size, entsize);
		} else {
			map = &hdr->freemap[after];
			
			map->base = entry->nameidx;
			be16_add_cpu(&map->size, entsize);
		}
	} else {
		map = &hdr->freemap[smallest];
		if (be16_to_cpu(map->size) < entsize) {
			map->base = cpu_to_be16(be16_to_cpu(entry->nameidx));
			map->size = cpu_to_be16(entsize);
		}
	}

	if (be16_to_cpu(entry->nameidx) == be16_to_cpu(hdr->firstused))
		smallest = 1;
	else
		smallest = 0;

	memset(xfs_attr_leaf_name(leaf, args->index), 0, entsize);
	be16_add_cpu(&hdr->usedbytes, -entsize);
	xfs_da_log_buf(args->trans, bp,
	     XFS_DA_LOGRANGE(leaf, xfs_attr_leaf_name(leaf, args->index),
				   entsize));

	tmp = (be16_to_cpu(hdr->count) - args->index)
					* sizeof(xfs_attr_leaf_entry_t);
	memmove((char *)entry, (char *)(entry+1), tmp);
	be16_add_cpu(&hdr->count, -1);
	xfs_da_log_buf(args->trans, bp,
	    XFS_DA_LOGRANGE(leaf, entry, tmp + sizeof(*entry)));
	entry = &leaf->entries[be16_to_cpu(hdr->count)];
	memset((char *)entry, 0, sizeof(xfs_attr_leaf_entry_t));

	if (smallest) {
		tmp = XFS_LBSIZE(mp);
		entry = &leaf->entries[0];
		for (i = be16_to_cpu(hdr->count)-1; i >= 0; entry++, i--) {
			ASSERT(be16_to_cpu(entry->nameidx) >=
			       be16_to_cpu(hdr->firstused));
			ASSERT(be16_to_cpu(entry->nameidx) < XFS_LBSIZE(mp));

			if (be16_to_cpu(entry->nameidx) < tmp)
				tmp = be16_to_cpu(entry->nameidx);
		}
		hdr->firstused = cpu_to_be16(tmp);
		if (!hdr->firstused) {
			hdr->firstused = cpu_to_be16(
					tmp - XFS_ATTR_LEAF_NAME_ALIGN);
		}
	} else {
		hdr->holes = 1;		
	}
	xfs_da_log_buf(args->trans, bp,
			  XFS_DA_LOGRANGE(leaf, hdr, sizeof(*hdr)));

	tmp  = sizeof(xfs_attr_leaf_hdr_t);
	tmp += be16_to_cpu(leaf->hdr.count) * sizeof(xfs_attr_leaf_entry_t);
	tmp += be16_to_cpu(leaf->hdr.usedbytes);
	return(tmp < mp->m_attr_magicpct); 
}

void
xfs_attr_leaf_unbalance(xfs_da_state_t *state, xfs_da_state_blk_t *drop_blk,
				       xfs_da_state_blk_t *save_blk)
{
	xfs_attr_leafblock_t *drop_leaf, *save_leaf, *tmp_leaf;
	xfs_attr_leaf_hdr_t *drop_hdr, *save_hdr, *tmp_hdr;
	xfs_mount_t *mp;
	char *tmpbuffer;

	trace_xfs_attr_leaf_unbalance(state->args);

	mp = state->mp;
	ASSERT(drop_blk->magic == XFS_ATTR_LEAF_MAGIC);
	ASSERT(save_blk->magic == XFS_ATTR_LEAF_MAGIC);
	drop_leaf = drop_blk->bp->data;
	save_leaf = save_blk->bp->data;
	ASSERT(drop_leaf->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));
	ASSERT(save_leaf->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));
	drop_hdr = &drop_leaf->hdr;
	save_hdr = &save_leaf->hdr;

	drop_blk->hashval = be32_to_cpu(
		drop_leaf->entries[be16_to_cpu(drop_leaf->hdr.count)-1].hashval);

	if (save_hdr->holes == 0) {
		if (xfs_attr_leaf_order(save_blk->bp, drop_blk->bp)) {
			xfs_attr_leaf_moveents(drop_leaf, 0, save_leaf, 0,
			     be16_to_cpu(drop_hdr->count), mp);
		} else {
			xfs_attr_leaf_moveents(drop_leaf, 0, save_leaf,
				  be16_to_cpu(save_hdr->count),
				  be16_to_cpu(drop_hdr->count), mp);
		}
	} else {
		tmpbuffer = kmem_alloc(state->blocksize, KM_SLEEP);
		ASSERT(tmpbuffer != NULL);
		memset(tmpbuffer, 0, state->blocksize);
		tmp_leaf = (xfs_attr_leafblock_t *)tmpbuffer;
		tmp_hdr = &tmp_leaf->hdr;
		tmp_hdr->info = save_hdr->info;	
		tmp_hdr->count = 0;
		tmp_hdr->firstused = cpu_to_be16(state->blocksize);
		if (!tmp_hdr->firstused) {
			tmp_hdr->firstused = cpu_to_be16(
				state->blocksize - XFS_ATTR_LEAF_NAME_ALIGN);
		}
		tmp_hdr->usedbytes = 0;
		if (xfs_attr_leaf_order(save_blk->bp, drop_blk->bp)) {
			xfs_attr_leaf_moveents(drop_leaf, 0, tmp_leaf, 0,
				be16_to_cpu(drop_hdr->count), mp);
			xfs_attr_leaf_moveents(save_leaf, 0, tmp_leaf,
				  be16_to_cpu(tmp_leaf->hdr.count),
				  be16_to_cpu(save_hdr->count), mp);
		} else {
			xfs_attr_leaf_moveents(save_leaf, 0, tmp_leaf, 0,
				be16_to_cpu(save_hdr->count), mp);
			xfs_attr_leaf_moveents(drop_leaf, 0, tmp_leaf,
				be16_to_cpu(tmp_leaf->hdr.count),
				be16_to_cpu(drop_hdr->count), mp);
		}
		memcpy((char *)save_leaf, (char *)tmp_leaf, state->blocksize);
		kmem_free(tmpbuffer);
	}

	xfs_da_log_buf(state->args->trans, save_blk->bp, 0,
					   state->blocksize - 1);

	save_blk->hashval = be32_to_cpu(
		save_leaf->entries[be16_to_cpu(save_leaf->hdr.count)-1].hashval);
}


int
xfs_attr_leaf_lookup_int(xfs_dabuf_t *bp, xfs_da_args_t *args)
{
	xfs_attr_leafblock_t *leaf;
	xfs_attr_leaf_entry_t *entry;
	xfs_attr_leaf_name_local_t *name_loc;
	xfs_attr_leaf_name_remote_t *name_rmt;
	int probe, span;
	xfs_dahash_t hashval;

	trace_xfs_attr_leaf_lookup(args);

	leaf = bp->data;
	ASSERT(leaf->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));
	ASSERT(be16_to_cpu(leaf->hdr.count)
					< (XFS_LBSIZE(args->dp->i_mount)/8));

	hashval = args->hashval;
	probe = span = be16_to_cpu(leaf->hdr.count) / 2;
	for (entry = &leaf->entries[probe]; span > 4;
		   entry = &leaf->entries[probe]) {
		span /= 2;
		if (be32_to_cpu(entry->hashval) < hashval)
			probe += span;
		else if (be32_to_cpu(entry->hashval) > hashval)
			probe -= span;
		else
			break;
	}
	ASSERT((probe >= 0) &&
	       (!leaf->hdr.count
	       || (probe < be16_to_cpu(leaf->hdr.count))));
	ASSERT((span <= 4) || (be32_to_cpu(entry->hashval) == hashval));

	while ((probe > 0) && (be32_to_cpu(entry->hashval) >= hashval)) {
		entry--;
		probe--;
	}
	while ((probe < be16_to_cpu(leaf->hdr.count)) &&
	       (be32_to_cpu(entry->hashval) < hashval)) {
		entry++;
		probe++;
	}
	if ((probe == be16_to_cpu(leaf->hdr.count)) ||
	    (be32_to_cpu(entry->hashval) != hashval)) {
		args->index = probe;
		return(XFS_ERROR(ENOATTR));
	}

	for (  ; (probe < be16_to_cpu(leaf->hdr.count)) &&
			(be32_to_cpu(entry->hashval) == hashval);
			entry++, probe++) {
		if ((args->flags & XFS_ATTR_INCOMPLETE) !=
		    (entry->flags & XFS_ATTR_INCOMPLETE)) {
			continue;
		}
		if (entry->flags & XFS_ATTR_LOCAL) {
			name_loc = xfs_attr_leaf_name_local(leaf, probe);
			if (name_loc->namelen != args->namelen)
				continue;
			if (memcmp(args->name, (char *)name_loc->nameval, args->namelen) != 0)
				continue;
			if (!xfs_attr_namesp_match(args->flags, entry->flags))
				continue;
			args->index = probe;
			return(XFS_ERROR(EEXIST));
		} else {
			name_rmt = xfs_attr_leaf_name_remote(leaf, probe);
			if (name_rmt->namelen != args->namelen)
				continue;
			if (memcmp(args->name, (char *)name_rmt->name,
					     args->namelen) != 0)
				continue;
			if (!xfs_attr_namesp_match(args->flags, entry->flags))
				continue;
			args->index = probe;
			args->rmtblkno = be32_to_cpu(name_rmt->valueblk);
			args->rmtblkcnt = XFS_B_TO_FSB(args->dp->i_mount,
						   be32_to_cpu(name_rmt->valuelen));
			return(XFS_ERROR(EEXIST));
		}
	}
	args->index = probe;
	return(XFS_ERROR(ENOATTR));
}

int
xfs_attr_leaf_getvalue(xfs_dabuf_t *bp, xfs_da_args_t *args)
{
	int valuelen;
	xfs_attr_leafblock_t *leaf;
	xfs_attr_leaf_entry_t *entry;
	xfs_attr_leaf_name_local_t *name_loc;
	xfs_attr_leaf_name_remote_t *name_rmt;

	leaf = bp->data;
	ASSERT(leaf->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));
	ASSERT(be16_to_cpu(leaf->hdr.count)
					< (XFS_LBSIZE(args->dp->i_mount)/8));
	ASSERT(args->index < be16_to_cpu(leaf->hdr.count));

	entry = &leaf->entries[args->index];
	if (entry->flags & XFS_ATTR_LOCAL) {
		name_loc = xfs_attr_leaf_name_local(leaf, args->index);
		ASSERT(name_loc->namelen == args->namelen);
		ASSERT(memcmp(args->name, name_loc->nameval, args->namelen) == 0);
		valuelen = be16_to_cpu(name_loc->valuelen);
		if (args->flags & ATTR_KERNOVAL) {
			args->valuelen = valuelen;
			return(0);
		}
		if (args->valuelen < valuelen) {
			args->valuelen = valuelen;
			return(XFS_ERROR(ERANGE));
		}
		args->valuelen = valuelen;
		memcpy(args->value, &name_loc->nameval[args->namelen], valuelen);
	} else {
		name_rmt = xfs_attr_leaf_name_remote(leaf, args->index);
		ASSERT(name_rmt->namelen == args->namelen);
		ASSERT(memcmp(args->name, name_rmt->name, args->namelen) == 0);
		valuelen = be32_to_cpu(name_rmt->valuelen);
		args->rmtblkno = be32_to_cpu(name_rmt->valueblk);
		args->rmtblkcnt = XFS_B_TO_FSB(args->dp->i_mount, valuelen);
		if (args->flags & ATTR_KERNOVAL) {
			args->valuelen = valuelen;
			return(0);
		}
		if (args->valuelen < valuelen) {
			args->valuelen = valuelen;
			return(XFS_ERROR(ERANGE));
		}
		args->valuelen = valuelen;
	}
	return(0);
}


STATIC void
xfs_attr_leaf_moveents(xfs_attr_leafblock_t *leaf_s, int start_s,
			xfs_attr_leafblock_t *leaf_d, int start_d,
			int count, xfs_mount_t *mp)
{
	xfs_attr_leaf_hdr_t *hdr_s, *hdr_d;
	xfs_attr_leaf_entry_t *entry_s, *entry_d;
	int desti, tmp, i;

	if (count == 0)
		return;

	ASSERT(leaf_s->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));
	ASSERT(leaf_d->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));
	hdr_s = &leaf_s->hdr;
	hdr_d = &leaf_d->hdr;
	ASSERT((be16_to_cpu(hdr_s->count) > 0) &&
	       (be16_to_cpu(hdr_s->count) < (XFS_LBSIZE(mp)/8)));
	ASSERT(be16_to_cpu(hdr_s->firstused) >=
		((be16_to_cpu(hdr_s->count)
					* sizeof(*entry_s))+sizeof(*hdr_s)));
	ASSERT(be16_to_cpu(hdr_d->count) < (XFS_LBSIZE(mp)/8));
	ASSERT(be16_to_cpu(hdr_d->firstused) >=
		((be16_to_cpu(hdr_d->count)
					* sizeof(*entry_d))+sizeof(*hdr_d)));

	ASSERT(start_s < be16_to_cpu(hdr_s->count));
	ASSERT(start_d <= be16_to_cpu(hdr_d->count));
	ASSERT(count <= be16_to_cpu(hdr_s->count));

	if (start_d < be16_to_cpu(hdr_d->count)) {
		tmp  = be16_to_cpu(hdr_d->count) - start_d;
		tmp *= sizeof(xfs_attr_leaf_entry_t);
		entry_s = &leaf_d->entries[start_d];
		entry_d = &leaf_d->entries[start_d + count];
		memmove((char *)entry_d, (char *)entry_s, tmp);
	}

	entry_s = &leaf_s->entries[start_s];
	entry_d = &leaf_d->entries[start_d];
	desti = start_d;
	for (i = 0; i < count; entry_s++, entry_d++, desti++, i++) {
		ASSERT(be16_to_cpu(entry_s->nameidx)
				>= be16_to_cpu(hdr_s->firstused));
		tmp = xfs_attr_leaf_entsize(leaf_s, start_s + i);
#ifdef GROT
		if (entry_s->flags & XFS_ATTR_INCOMPLETE) { 
			memset(xfs_attr_leaf_name(leaf_s, start_s + i), 0, tmp);
			be16_add_cpu(&hdr_s->usedbytes, -tmp);
			be16_add_cpu(&hdr_s->count, -1);
			entry_d--;	
			desti--;
			if ((start_s + i) < offset)
				result++;	
		} else {
#endif 
			be16_add_cpu(&hdr_d->firstused, -tmp);
			
			entry_d->hashval = entry_s->hashval;
			
			entry_d->nameidx = hdr_d->firstused;
			entry_d->flags = entry_s->flags;
			ASSERT(be16_to_cpu(entry_d->nameidx) + tmp
							<= XFS_LBSIZE(mp));
			memmove(xfs_attr_leaf_name(leaf_d, desti),
				xfs_attr_leaf_name(leaf_s, start_s + i), tmp);
			ASSERT(be16_to_cpu(entry_s->nameidx) + tmp
							<= XFS_LBSIZE(mp));
			memset(xfs_attr_leaf_name(leaf_s, start_s + i), 0, tmp);
			be16_add_cpu(&hdr_s->usedbytes, -tmp);
			be16_add_cpu(&hdr_d->usedbytes, tmp);
			be16_add_cpu(&hdr_s->count, -1);
			be16_add_cpu(&hdr_d->count, 1);
			tmp = be16_to_cpu(hdr_d->count)
						* sizeof(xfs_attr_leaf_entry_t)
						+ sizeof(xfs_attr_leaf_hdr_t);
			ASSERT(be16_to_cpu(hdr_d->firstused) >= tmp);
#ifdef GROT
		}
#endif 
	}

	if (start_s == be16_to_cpu(hdr_s->count)) {
		tmp = count * sizeof(xfs_attr_leaf_entry_t);
		entry_s = &leaf_s->entries[start_s];
		ASSERT(((char *)entry_s + tmp) <=
		       ((char *)leaf_s + XFS_LBSIZE(mp)));
		memset((char *)entry_s, 0, tmp);
	} else {
		tmp  = be16_to_cpu(hdr_s->count) - count;
		tmp *= sizeof(xfs_attr_leaf_entry_t);
		entry_s = &leaf_s->entries[start_s + count];
		entry_d = &leaf_s->entries[start_s];
		memmove((char *)entry_d, (char *)entry_s, tmp);

		tmp = count * sizeof(xfs_attr_leaf_entry_t);
		entry_s = &leaf_s->entries[be16_to_cpu(hdr_s->count)];
		ASSERT(((char *)entry_s + tmp) <=
		       ((char *)leaf_s + XFS_LBSIZE(mp)));
		memset((char *)entry_s, 0, tmp);
	}

	hdr_d->freemap[0].base = cpu_to_be16(sizeof(xfs_attr_leaf_hdr_t));
	be16_add_cpu(&hdr_d->freemap[0].base, be16_to_cpu(hdr_d->count) *
			sizeof(xfs_attr_leaf_entry_t));
	hdr_d->freemap[0].size = cpu_to_be16(be16_to_cpu(hdr_d->firstused)
			      - be16_to_cpu(hdr_d->freemap[0].base));
	hdr_d->freemap[1].base = 0;
	hdr_d->freemap[2].base = 0;
	hdr_d->freemap[1].size = 0;
	hdr_d->freemap[2].size = 0;
	hdr_s->holes = 1;	
}

int
xfs_attr_leaf_order(xfs_dabuf_t *leaf1_bp, xfs_dabuf_t *leaf2_bp)
{
	xfs_attr_leafblock_t *leaf1, *leaf2;

	leaf1 = leaf1_bp->data;
	leaf2 = leaf2_bp->data;
	ASSERT((leaf1->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC)) &&
	       (leaf2->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC)));
	if ((be16_to_cpu(leaf1->hdr.count) > 0) &&
	    (be16_to_cpu(leaf2->hdr.count) > 0) &&
	    ((be32_to_cpu(leaf2->entries[0].hashval) <
	      be32_to_cpu(leaf1->entries[0].hashval)) ||
	     (be32_to_cpu(leaf2->entries[
			be16_to_cpu(leaf2->hdr.count)-1].hashval) <
	      be32_to_cpu(leaf1->entries[
			be16_to_cpu(leaf1->hdr.count)-1].hashval)))) {
		return(1);
	}
	return(0);
}

xfs_dahash_t
xfs_attr_leaf_lasthash(xfs_dabuf_t *bp, int *count)
{
	xfs_attr_leafblock_t *leaf;

	leaf = bp->data;
	ASSERT(leaf->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));
	if (count)
		*count = be16_to_cpu(leaf->hdr.count);
	if (!leaf->hdr.count)
		return(0);
	return be32_to_cpu(leaf->entries[be16_to_cpu(leaf->hdr.count)-1].hashval);
}

STATIC int
xfs_attr_leaf_entsize(xfs_attr_leafblock_t *leaf, int index)
{
	xfs_attr_leaf_name_local_t *name_loc;
	xfs_attr_leaf_name_remote_t *name_rmt;
	int size;

	ASSERT(leaf->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));
	if (leaf->entries[index].flags & XFS_ATTR_LOCAL) {
		name_loc = xfs_attr_leaf_name_local(leaf, index);
		size = xfs_attr_leaf_entsize_local(name_loc->namelen,
						   be16_to_cpu(name_loc->valuelen));
	} else {
		name_rmt = xfs_attr_leaf_name_remote(leaf, index);
		size = xfs_attr_leaf_entsize_remote(name_rmt->namelen);
	}
	return(size);
}

int
xfs_attr_leaf_newentsize(int namelen, int valuelen, int blocksize, int *local)
{
	int size;

	size = xfs_attr_leaf_entsize_local(namelen, valuelen);
	if (size < xfs_attr_leaf_entsize_local_max(blocksize)) {
		if (local) {
			*local = 1;
		}
	} else {
		size = xfs_attr_leaf_entsize_remote(namelen);
		if (local) {
			*local = 0;
		}
	}
	return(size);
}

int
xfs_attr_leaf_list_int(xfs_dabuf_t *bp, xfs_attr_list_context_t *context)
{
	attrlist_cursor_kern_t *cursor;
	xfs_attr_leafblock_t *leaf;
	xfs_attr_leaf_entry_t *entry;
	int retval, i;

	ASSERT(bp != NULL);
	leaf = bp->data;
	cursor = context->cursor;
	cursor->initted = 1;

	trace_xfs_attr_list_leaf(context);

	if (context->resynch) {
		entry = &leaf->entries[0];
		for (i = 0; i < be16_to_cpu(leaf->hdr.count); entry++, i++) {
			if (be32_to_cpu(entry->hashval) == cursor->hashval) {
				if (cursor->offset == context->dupcnt) {
					context->dupcnt = 0;
					break;
				}
				context->dupcnt++;
			} else if (be32_to_cpu(entry->hashval) >
					cursor->hashval) {
				context->dupcnt = 0;
				break;
			}
		}
		if (i == be16_to_cpu(leaf->hdr.count)) {
			trace_xfs_attr_list_notfound(context);
			return(0);
		}
	} else {
		entry = &leaf->entries[0];
		i = 0;
	}
	context->resynch = 0;

	retval = 0;
	for (  ; (i < be16_to_cpu(leaf->hdr.count)); entry++, i++) {
		if (be32_to_cpu(entry->hashval) != cursor->hashval) {
			cursor->hashval = be32_to_cpu(entry->hashval);
			cursor->offset = 0;
		}

		if (entry->flags & XFS_ATTR_INCOMPLETE)
			continue;		

		if (entry->flags & XFS_ATTR_LOCAL) {
			xfs_attr_leaf_name_local_t *name_loc =
				xfs_attr_leaf_name_local(leaf, i);

			retval = context->put_listent(context,
						entry->flags,
						name_loc->nameval,
						(int)name_loc->namelen,
						be16_to_cpu(name_loc->valuelen),
						&name_loc->nameval[name_loc->namelen]);
			if (retval)
				return retval;
		} else {
			xfs_attr_leaf_name_remote_t *name_rmt =
				xfs_attr_leaf_name_remote(leaf, i);

			int valuelen = be32_to_cpu(name_rmt->valuelen);

			if (context->put_value) {
				xfs_da_args_t args;

				memset((char *)&args, 0, sizeof(args));
				args.dp = context->dp;
				args.whichfork = XFS_ATTR_FORK;
				args.valuelen = valuelen;
				args.value = kmem_alloc(valuelen, KM_SLEEP | KM_NOFS);
				args.rmtblkno = be32_to_cpu(name_rmt->valueblk);
				args.rmtblkcnt = XFS_B_TO_FSB(args.dp->i_mount, valuelen);
				retval = xfs_attr_rmtval_get(&args);
				if (retval)
					return retval;
				retval = context->put_listent(context,
						entry->flags,
						name_rmt->name,
						(int)name_rmt->namelen,
						valuelen,
						args.value);
				kmem_free(args.value);
			} else {
				retval = context->put_listent(context,
						entry->flags,
						name_rmt->name,
						(int)name_rmt->namelen,
						valuelen,
						NULL);
			}
			if (retval)
				return retval;
		}
		if (context->seen_enough)
			break;
		cursor->offset++;
	}
	trace_xfs_attr_list_leaf_end(context);
	return(retval);
}



int
xfs_attr_leaf_clearflag(xfs_da_args_t *args)
{
	xfs_attr_leafblock_t *leaf;
	xfs_attr_leaf_entry_t *entry;
	xfs_attr_leaf_name_remote_t *name_rmt;
	xfs_dabuf_t *bp;
	int error;
#ifdef DEBUG
	xfs_attr_leaf_name_local_t *name_loc;
	int namelen;
	char *name;
#endif 

	trace_xfs_attr_leaf_clearflag(args);
	error = xfs_da_read_buf(args->trans, args->dp, args->blkno, -1, &bp,
					     XFS_ATTR_FORK);
	if (error) {
		return(error);
	}
	ASSERT(bp != NULL);

	leaf = bp->data;
	ASSERT(leaf->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));
	ASSERT(args->index < be16_to_cpu(leaf->hdr.count));
	ASSERT(args->index >= 0);
	entry = &leaf->entries[ args->index ];
	ASSERT(entry->flags & XFS_ATTR_INCOMPLETE);

#ifdef DEBUG
	if (entry->flags & XFS_ATTR_LOCAL) {
		name_loc = xfs_attr_leaf_name_local(leaf, args->index);
		namelen = name_loc->namelen;
		name = (char *)name_loc->nameval;
	} else {
		name_rmt = xfs_attr_leaf_name_remote(leaf, args->index);
		namelen = name_rmt->namelen;
		name = (char *)name_rmt->name;
	}
	ASSERT(be32_to_cpu(entry->hashval) == args->hashval);
	ASSERT(namelen == args->namelen);
	ASSERT(memcmp(name, args->name, namelen) == 0);
#endif 

	entry->flags &= ~XFS_ATTR_INCOMPLETE;
	xfs_da_log_buf(args->trans, bp,
			 XFS_DA_LOGRANGE(leaf, entry, sizeof(*entry)));

	if (args->rmtblkno) {
		ASSERT((entry->flags & XFS_ATTR_LOCAL) == 0);
		name_rmt = xfs_attr_leaf_name_remote(leaf, args->index);
		name_rmt->valueblk = cpu_to_be32(args->rmtblkno);
		name_rmt->valuelen = cpu_to_be32(args->valuelen);
		xfs_da_log_buf(args->trans, bp,
			 XFS_DA_LOGRANGE(leaf, name_rmt, sizeof(*name_rmt)));
	}
	xfs_da_buf_done(bp);

	return xfs_trans_roll(&args->trans, args->dp);
}

int
xfs_attr_leaf_setflag(xfs_da_args_t *args)
{
	xfs_attr_leafblock_t *leaf;
	xfs_attr_leaf_entry_t *entry;
	xfs_attr_leaf_name_remote_t *name_rmt;
	xfs_dabuf_t *bp;
	int error;

	trace_xfs_attr_leaf_setflag(args);

	error = xfs_da_read_buf(args->trans, args->dp, args->blkno, -1, &bp,
					     XFS_ATTR_FORK);
	if (error) {
		return(error);
	}
	ASSERT(bp != NULL);

	leaf = bp->data;
	ASSERT(leaf->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));
	ASSERT(args->index < be16_to_cpu(leaf->hdr.count));
	ASSERT(args->index >= 0);
	entry = &leaf->entries[ args->index ];

	ASSERT((entry->flags & XFS_ATTR_INCOMPLETE) == 0);
	entry->flags |= XFS_ATTR_INCOMPLETE;
	xfs_da_log_buf(args->trans, bp,
			XFS_DA_LOGRANGE(leaf, entry, sizeof(*entry)));
	if ((entry->flags & XFS_ATTR_LOCAL) == 0) {
		name_rmt = xfs_attr_leaf_name_remote(leaf, args->index);
		name_rmt->valueblk = 0;
		name_rmt->valuelen = 0;
		xfs_da_log_buf(args->trans, bp,
			 XFS_DA_LOGRANGE(leaf, name_rmt, sizeof(*name_rmt)));
	}
	xfs_da_buf_done(bp);

	return xfs_trans_roll(&args->trans, args->dp);
}

int
xfs_attr_leaf_flipflags(xfs_da_args_t *args)
{
	xfs_attr_leafblock_t *leaf1, *leaf2;
	xfs_attr_leaf_entry_t *entry1, *entry2;
	xfs_attr_leaf_name_remote_t *name_rmt;
	xfs_dabuf_t *bp1, *bp2;
	int error;
#ifdef DEBUG
	xfs_attr_leaf_name_local_t *name_loc;
	int namelen1, namelen2;
	char *name1, *name2;
#endif 

	trace_xfs_attr_leaf_flipflags(args);

	error = xfs_da_read_buf(args->trans, args->dp, args->blkno, -1, &bp1,
					     XFS_ATTR_FORK);
	if (error) {
		return(error);
	}
	ASSERT(bp1 != NULL);

	if (args->blkno2 != args->blkno) {
		error = xfs_da_read_buf(args->trans, args->dp, args->blkno2,
					-1, &bp2, XFS_ATTR_FORK);
		if (error) {
			return(error);
		}
		ASSERT(bp2 != NULL);
	} else {
		bp2 = bp1;
	}

	leaf1 = bp1->data;
	ASSERT(leaf1->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));
	ASSERT(args->index < be16_to_cpu(leaf1->hdr.count));
	ASSERT(args->index >= 0);
	entry1 = &leaf1->entries[ args->index ];

	leaf2 = bp2->data;
	ASSERT(leaf2->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));
	ASSERT(args->index2 < be16_to_cpu(leaf2->hdr.count));
	ASSERT(args->index2 >= 0);
	entry2 = &leaf2->entries[ args->index2 ];

#ifdef DEBUG
	if (entry1->flags & XFS_ATTR_LOCAL) {
		name_loc = xfs_attr_leaf_name_local(leaf1, args->index);
		namelen1 = name_loc->namelen;
		name1 = (char *)name_loc->nameval;
	} else {
		name_rmt = xfs_attr_leaf_name_remote(leaf1, args->index);
		namelen1 = name_rmt->namelen;
		name1 = (char *)name_rmt->name;
	}
	if (entry2->flags & XFS_ATTR_LOCAL) {
		name_loc = xfs_attr_leaf_name_local(leaf2, args->index2);
		namelen2 = name_loc->namelen;
		name2 = (char *)name_loc->nameval;
	} else {
		name_rmt = xfs_attr_leaf_name_remote(leaf2, args->index2);
		namelen2 = name_rmt->namelen;
		name2 = (char *)name_rmt->name;
	}
	ASSERT(be32_to_cpu(entry1->hashval) == be32_to_cpu(entry2->hashval));
	ASSERT(namelen1 == namelen2);
	ASSERT(memcmp(name1, name2, namelen1) == 0);
#endif 

	ASSERT(entry1->flags & XFS_ATTR_INCOMPLETE);
	ASSERT((entry2->flags & XFS_ATTR_INCOMPLETE) == 0);

	entry1->flags &= ~XFS_ATTR_INCOMPLETE;
	xfs_da_log_buf(args->trans, bp1,
			  XFS_DA_LOGRANGE(leaf1, entry1, sizeof(*entry1)));
	if (args->rmtblkno) {
		ASSERT((entry1->flags & XFS_ATTR_LOCAL) == 0);
		name_rmt = xfs_attr_leaf_name_remote(leaf1, args->index);
		name_rmt->valueblk = cpu_to_be32(args->rmtblkno);
		name_rmt->valuelen = cpu_to_be32(args->valuelen);
		xfs_da_log_buf(args->trans, bp1,
			 XFS_DA_LOGRANGE(leaf1, name_rmt, sizeof(*name_rmt)));
	}

	entry2->flags |= XFS_ATTR_INCOMPLETE;
	xfs_da_log_buf(args->trans, bp2,
			  XFS_DA_LOGRANGE(leaf2, entry2, sizeof(*entry2)));
	if ((entry2->flags & XFS_ATTR_LOCAL) == 0) {
		name_rmt = xfs_attr_leaf_name_remote(leaf2, args->index2);
		name_rmt->valueblk = 0;
		name_rmt->valuelen = 0;
		xfs_da_log_buf(args->trans, bp2,
			 XFS_DA_LOGRANGE(leaf2, name_rmt, sizeof(*name_rmt)));
	}
	xfs_da_buf_done(bp1);
	if (bp1 != bp2)
		xfs_da_buf_done(bp2);

	error = xfs_trans_roll(&args->trans, args->dp);

	return(error);
}


int
xfs_attr_root_inactive(xfs_trans_t **trans, xfs_inode_t *dp)
{
	xfs_da_blkinfo_t *info;
	xfs_daddr_t blkno;
	xfs_dabuf_t *bp;
	int error;

	error = xfs_da_read_buf(*trans, dp, 0, -1, &bp, XFS_ATTR_FORK);
	if (error)
		return(error);
	blkno = xfs_da_blkno(bp);

	info = bp->data;
	if (info->magic == cpu_to_be16(XFS_DA_NODE_MAGIC)) {
		error = xfs_attr_node_inactive(trans, dp, bp, 1);
	} else if (info->magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC)) {
		error = xfs_attr_leaf_inactive(trans, dp, bp);
	} else {
		error = XFS_ERROR(EIO);
		xfs_da_brelse(*trans, bp);
	}
	if (error)
		return(error);

	error = xfs_da_get_buf(*trans, dp, 0, blkno, &bp, XFS_ATTR_FORK);
	if (error)
		return(error);
	xfs_da_binval(*trans, bp);	
	error = xfs_trans_roll(trans, dp);

	return (error);
}

STATIC int
xfs_attr_node_inactive(xfs_trans_t **trans, xfs_inode_t *dp, xfs_dabuf_t *bp,
				   int level)
{
	xfs_da_blkinfo_t *info;
	xfs_da_intnode_t *node;
	xfs_dablk_t child_fsb;
	xfs_daddr_t parent_blkno, child_blkno;
	int error, count, i;
	xfs_dabuf_t *child_bp;

	if (level > XFS_DA_NODE_MAXDEPTH) {
		xfs_da_brelse(*trans, bp);	
		return(XFS_ERROR(EIO));
	}

	node = bp->data;
	ASSERT(node->hdr.info.magic == cpu_to_be16(XFS_DA_NODE_MAGIC));
	parent_blkno = xfs_da_blkno(bp);	
	count = be16_to_cpu(node->hdr.count);
	if (!count) {
		xfs_da_brelse(*trans, bp);
		return(0);
	}
	child_fsb = be32_to_cpu(node->btree[0].before);
	xfs_da_brelse(*trans, bp);	

	for (i = 0; i < count; i++) {
		error = xfs_da_read_buf(*trans, dp, child_fsb, -2, &child_bp,
						XFS_ATTR_FORK);
		if (error)
			return(error);
		if (child_bp) {
						
			child_blkno = xfs_da_blkno(child_bp);

			info = child_bp->data;
			if (info->magic == cpu_to_be16(XFS_DA_NODE_MAGIC)) {
				error = xfs_attr_node_inactive(trans, dp,
						child_bp, level+1);
			} else if (info->magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC)) {
				error = xfs_attr_leaf_inactive(trans, dp,
						child_bp);
			} else {
				error = XFS_ERROR(EIO);
				xfs_da_brelse(*trans, child_bp);
			}
			if (error)
				return(error);

			error = xfs_da_get_buf(*trans, dp, 0, child_blkno,
				&child_bp, XFS_ATTR_FORK);
			if (error)
				return(error);
			xfs_da_binval(*trans, child_bp);
		}

		if ((i+1) < count) {
			error = xfs_da_read_buf(*trans, dp, 0, parent_blkno,
				&bp, XFS_ATTR_FORK);
			if (error)
				return(error);
			child_fsb = be32_to_cpu(node->btree[i+1].before);
			xfs_da_brelse(*trans, bp);
		}
		error = xfs_trans_roll(trans, dp);
		if (error)
			return (error);
	}

	return(0);
}

STATIC int
xfs_attr_leaf_inactive(xfs_trans_t **trans, xfs_inode_t *dp, xfs_dabuf_t *bp)
{
	xfs_attr_leafblock_t *leaf;
	xfs_attr_leaf_entry_t *entry;
	xfs_attr_leaf_name_remote_t *name_rmt;
	xfs_attr_inactive_list_t *list, *lp;
	int error, count, size, tmp, i;

	leaf = bp->data;
	ASSERT(leaf->hdr.info.magic == cpu_to_be16(XFS_ATTR_LEAF_MAGIC));

	count = 0;
	entry = &leaf->entries[0];
	for (i = 0; i < be16_to_cpu(leaf->hdr.count); entry++, i++) {
		if (be16_to_cpu(entry->nameidx) &&
		    ((entry->flags & XFS_ATTR_LOCAL) == 0)) {
			name_rmt = xfs_attr_leaf_name_remote(leaf, i);
			if (name_rmt->valueblk)
				count++;
		}
	}

	if (count == 0) {
		xfs_da_brelse(*trans, bp);
		return(0);
	}

	size = count * sizeof(xfs_attr_inactive_list_t);
	list = (xfs_attr_inactive_list_t *)kmem_alloc(size, KM_SLEEP);

	lp = list;
	entry = &leaf->entries[0];
	for (i = 0; i < be16_to_cpu(leaf->hdr.count); entry++, i++) {
		if (be16_to_cpu(entry->nameidx) &&
		    ((entry->flags & XFS_ATTR_LOCAL) == 0)) {
			name_rmt = xfs_attr_leaf_name_remote(leaf, i);
			if (name_rmt->valueblk) {
				lp->valueblk = be32_to_cpu(name_rmt->valueblk);
				lp->valuelen = XFS_B_TO_FSB(dp->i_mount,
						    be32_to_cpu(name_rmt->valuelen));
				lp++;
			}
		}
	}
	xfs_da_brelse(*trans, bp);	

	error = 0;
	for (lp = list, i = 0; i < count; i++, lp++) {
		tmp = xfs_attr_leaf_freextent(trans, dp,
				lp->valueblk, lp->valuelen);

		if (error == 0)
			error = tmp;	
	}

	kmem_free((xfs_caddr_t)list);
	return(error);
}

STATIC int
xfs_attr_leaf_freextent(xfs_trans_t **trans, xfs_inode_t *dp,
				    xfs_dablk_t blkno, int blkcnt)
{
	xfs_bmbt_irec_t map;
	xfs_dablk_t tblkno;
	int tblkcnt, dblkcnt, nmap, error;
	xfs_daddr_t dblkno;
	xfs_buf_t *bp;

	tblkno = blkno;
	tblkcnt = blkcnt;
	while (tblkcnt > 0) {
		nmap = 1;
		error = xfs_bmapi_read(dp, (xfs_fileoff_t)tblkno, tblkcnt,
				       &map, &nmap, XFS_BMAPI_ATTRFORK);
		if (error) {
			return(error);
		}
		ASSERT(nmap == 1);
		ASSERT(map.br_startblock != DELAYSTARTBLOCK);

		if (map.br_startblock != HOLESTARTBLOCK) {

			dblkno = XFS_FSB_TO_DADDR(dp->i_mount,
						  map.br_startblock);
			dblkcnt = XFS_FSB_TO_BB(dp->i_mount,
						map.br_blockcount);
			bp = xfs_trans_get_buf(*trans,
					dp->i_mount->m_ddev_targp,
					dblkno, dblkcnt, XBF_LOCK);
			if (!bp)
				return ENOMEM;
			xfs_trans_binval(*trans, bp);
			error = xfs_trans_roll(trans, dp);
			if (error)
				return (error);
		}

		tblkno += map.br_blockcount;
		tblkcnt -= map.br_blockcount;
	}

	return(0);
}
