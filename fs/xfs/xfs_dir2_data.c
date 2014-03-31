/*
 * Copyright (c) 2000-2002,2005 Silicon Graphics, Inc.
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
#include "xfs_log.h"
#include "xfs_inum.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_mount.h"
#include "xfs_da_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_dir2_format.h"
#include "xfs_dir2_priv.h"
#include "xfs_error.h"

STATIC xfs_dir2_data_free_t *
xfs_dir2_data_freefind(xfs_dir2_data_hdr_t *hdr, xfs_dir2_data_unused_t *dup);

#ifdef DEBUG
void
xfs_dir2_data_check(
	xfs_inode_t		*dp,		
	xfs_dabuf_t		*bp)		
{
	xfs_dir2_dataptr_t	addr;		
	xfs_dir2_data_free_t	*bf;		
	xfs_dir2_block_tail_t	*btp=NULL;	
	int			count;		
	xfs_dir2_data_hdr_t	*hdr;		
	xfs_dir2_data_entry_t	*dep;		
	xfs_dir2_data_free_t	*dfp;		
	xfs_dir2_data_unused_t	*dup;		
	char			*endp;		
	int			freeseen;	
	xfs_dahash_t		hash;		
	int			i;		
	int			lastfree;	
	xfs_dir2_leaf_entry_t	*lep=NULL;	
	xfs_mount_t		*mp;		
	char			*p;		
	int			stale;		
	struct xfs_name		name;

	mp = dp->i_mount;
	hdr = bp->data;
	bf = hdr->bestfree;
	p = (char *)(hdr + 1);

	if (hdr->magic == cpu_to_be32(XFS_DIR2_BLOCK_MAGIC)) {
		btp = xfs_dir2_block_tail_p(mp, hdr);
		lep = xfs_dir2_block_leaf_p(btp);
		endp = (char *)lep;
	} else {
		ASSERT(hdr->magic == cpu_to_be32(XFS_DIR2_DATA_MAGIC));
		endp = (char *)hdr + mp->m_dirblksize;
	}

	count = lastfree = freeseen = 0;
	if (!bf[0].length) {
		ASSERT(!bf[0].offset);
		freeseen |= 1 << 0;
	}
	if (!bf[1].length) {
		ASSERT(!bf[1].offset);
		freeseen |= 1 << 1;
	}
	if (!bf[2].length) {
		ASSERT(!bf[2].offset);
		freeseen |= 1 << 2;
	}
	ASSERT(be16_to_cpu(bf[0].length) >= be16_to_cpu(bf[1].length));
	ASSERT(be16_to_cpu(bf[1].length) >= be16_to_cpu(bf[2].length));
	while (p < endp) {
		dup = (xfs_dir2_data_unused_t *)p;
		if (be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG) {
			ASSERT(lastfree == 0);
			ASSERT(be16_to_cpu(*xfs_dir2_data_unused_tag_p(dup)) ==
			       (char *)dup - (char *)hdr);
			dfp = xfs_dir2_data_freefind(hdr, dup);
			if (dfp) {
				i = (int)(dfp - bf);
				ASSERT((freeseen & (1 << i)) == 0);
				freeseen |= 1 << i;
			} else {
				ASSERT(be16_to_cpu(dup->length) <=
				       be16_to_cpu(bf[2].length));
			}
			p += be16_to_cpu(dup->length);
			lastfree = 1;
			continue;
		}
		dep = (xfs_dir2_data_entry_t *)p;
		ASSERT(dep->namelen != 0);
		ASSERT(xfs_dir_ino_validate(mp, be64_to_cpu(dep->inumber)) == 0);
		ASSERT(be16_to_cpu(*xfs_dir2_data_entry_tag_p(dep)) ==
		       (char *)dep - (char *)hdr);
		count++;
		lastfree = 0;
		if (hdr->magic == cpu_to_be32(XFS_DIR2_BLOCK_MAGIC)) {
			addr = xfs_dir2_db_off_to_dataptr(mp, mp->m_dirdatablk,
				(xfs_dir2_data_aoff_t)
				((char *)dep - (char *)hdr));
			name.name = dep->name;
			name.len = dep->namelen;
			hash = mp->m_dirnameops->hashname(&name);
			for (i = 0; i < be32_to_cpu(btp->count); i++) {
				if (be32_to_cpu(lep[i].address) == addr &&
				    be32_to_cpu(lep[i].hashval) == hash)
					break;
			}
			ASSERT(i < be32_to_cpu(btp->count));
		}
		p += xfs_dir2_data_entsize(dep->namelen);
	}
	ASSERT(freeseen == 7);
	if (hdr->magic == cpu_to_be32(XFS_DIR2_BLOCK_MAGIC)) {
		for (i = stale = 0; i < be32_to_cpu(btp->count); i++) {
			if (lep[i].address ==
			    cpu_to_be32(XFS_DIR2_NULL_DATAPTR))
				stale++;
			if (i > 0)
				ASSERT(be32_to_cpu(lep[i].hashval) >= be32_to_cpu(lep[i - 1].hashval));
		}
		ASSERT(count == be32_to_cpu(btp->count) - be32_to_cpu(btp->stale));
		ASSERT(stale == be32_to_cpu(btp->stale));
	}
}
#endif

STATIC xfs_dir2_data_free_t *
xfs_dir2_data_freefind(
	xfs_dir2_data_hdr_t	*hdr,		
	xfs_dir2_data_unused_t	*dup)		
{
	xfs_dir2_data_free_t	*dfp;		
	xfs_dir2_data_aoff_t	off;		
#if defined(DEBUG) && defined(__KERNEL__)
	int			matched;	
	int			seenzero;	
#endif

	off = (xfs_dir2_data_aoff_t)((char *)dup - (char *)hdr);
#if defined(DEBUG) && defined(__KERNEL__)
	ASSERT(hdr->magic == cpu_to_be32(XFS_DIR2_DATA_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR2_BLOCK_MAGIC));
	for (dfp = &hdr->bestfree[0], seenzero = matched = 0;
	     dfp < &hdr->bestfree[XFS_DIR2_DATA_FD_COUNT];
	     dfp++) {
		if (!dfp->offset) {
			ASSERT(!dfp->length);
			seenzero = 1;
			continue;
		}
		ASSERT(seenzero == 0);
		if (be16_to_cpu(dfp->offset) == off) {
			matched = 1;
			ASSERT(dfp->length == dup->length);
		} else if (off < be16_to_cpu(dfp->offset))
			ASSERT(off + be16_to_cpu(dup->length) <= be16_to_cpu(dfp->offset));
		else
			ASSERT(be16_to_cpu(dfp->offset) + be16_to_cpu(dfp->length) <= off);
		ASSERT(matched || be16_to_cpu(dfp->length) >= be16_to_cpu(dup->length));
		if (dfp > &hdr->bestfree[0])
			ASSERT(be16_to_cpu(dfp[-1].length) >= be16_to_cpu(dfp[0].length));
	}
#endif
	if (be16_to_cpu(dup->length) <
	    be16_to_cpu(hdr->bestfree[XFS_DIR2_DATA_FD_COUNT - 1].length))
		return NULL;
	for (dfp = &hdr->bestfree[0];
	     dfp < &hdr->bestfree[XFS_DIR2_DATA_FD_COUNT];
	     dfp++) {
		if (!dfp->offset)
			return NULL;
		if (be16_to_cpu(dfp->offset) == off)
			return dfp;
	}
	return NULL;
}

xfs_dir2_data_free_t *				
xfs_dir2_data_freeinsert(
	xfs_dir2_data_hdr_t	*hdr,		
	xfs_dir2_data_unused_t	*dup,		
	int			*loghead)	
{
	xfs_dir2_data_free_t	*dfp;		
	xfs_dir2_data_free_t	new;		

#ifdef __KERNEL__
	ASSERT(hdr->magic == cpu_to_be32(XFS_DIR2_DATA_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR2_BLOCK_MAGIC));
#endif
	dfp = hdr->bestfree;
	new.length = dup->length;
	new.offset = cpu_to_be16((char *)dup - (char *)hdr);

	if (be16_to_cpu(new.length) > be16_to_cpu(dfp[0].length)) {
		dfp[2] = dfp[1];
		dfp[1] = dfp[0];
		dfp[0] = new;
		*loghead = 1;
		return &dfp[0];
	}
	if (be16_to_cpu(new.length) > be16_to_cpu(dfp[1].length)) {
		dfp[2] = dfp[1];
		dfp[1] = new;
		*loghead = 1;
		return &dfp[1];
	}
	if (be16_to_cpu(new.length) > be16_to_cpu(dfp[2].length)) {
		dfp[2] = new;
		*loghead = 1;
		return &dfp[2];
	}
	return NULL;
}

STATIC void
xfs_dir2_data_freeremove(
	xfs_dir2_data_hdr_t	*hdr,		
	xfs_dir2_data_free_t	*dfp,		
	int			*loghead)	
{
#ifdef __KERNEL__
	ASSERT(hdr->magic == cpu_to_be32(XFS_DIR2_DATA_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR2_BLOCK_MAGIC));
#endif
	if (dfp == &hdr->bestfree[0]) {
		hdr->bestfree[0] = hdr->bestfree[1];
		hdr->bestfree[1] = hdr->bestfree[2];
	}
	else if (dfp == &hdr->bestfree[1])
		hdr->bestfree[1] = hdr->bestfree[2];
	else
		ASSERT(dfp == &hdr->bestfree[2]);
	hdr->bestfree[2].length = 0;
	hdr->bestfree[2].offset = 0;
	*loghead = 1;
}

void
xfs_dir2_data_freescan(
	xfs_mount_t		*mp,		
	xfs_dir2_data_hdr_t	*hdr,		
	int			*loghead)	
{
	xfs_dir2_block_tail_t	*btp;		
	xfs_dir2_data_entry_t	*dep;		
	xfs_dir2_data_unused_t	*dup;		
	char			*endp;		
	char			*p;		

#ifdef __KERNEL__
	ASSERT(hdr->magic == cpu_to_be32(XFS_DIR2_DATA_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR2_BLOCK_MAGIC));
#endif
	memset(hdr->bestfree, 0, sizeof(hdr->bestfree));
	*loghead = 1;
	p = (char *)(hdr + 1);
	if (hdr->magic == cpu_to_be32(XFS_DIR2_BLOCK_MAGIC)) {
		btp = xfs_dir2_block_tail_p(mp, hdr);
		endp = (char *)xfs_dir2_block_leaf_p(btp);
	} else
		endp = (char *)hdr + mp->m_dirblksize;
	while (p < endp) {
		dup = (xfs_dir2_data_unused_t *)p;
		if (be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG) {
			ASSERT((char *)dup - (char *)hdr ==
			       be16_to_cpu(*xfs_dir2_data_unused_tag_p(dup)));
			xfs_dir2_data_freeinsert(hdr, dup, loghead);
			p += be16_to_cpu(dup->length);
		}
		else {
			dep = (xfs_dir2_data_entry_t *)p;
			ASSERT((char *)dep - (char *)hdr ==
			       be16_to_cpu(*xfs_dir2_data_entry_tag_p(dep)));
			p += xfs_dir2_data_entsize(dep->namelen);
		}
	}
}

int						
xfs_dir2_data_init(
	xfs_da_args_t		*args,		
	xfs_dir2_db_t		blkno,		
	xfs_dabuf_t		**bpp)		
{
	xfs_dabuf_t		*bp;		
	xfs_dir2_data_hdr_t	*hdr;		
	xfs_inode_t		*dp;		
	xfs_dir2_data_unused_t	*dup;		
	int			error;		
	int			i;		
	xfs_mount_t		*mp;		
	xfs_trans_t		*tp;		
	int                     t;              

	dp = args->dp;
	mp = dp->i_mount;
	tp = args->trans;
	error = xfs_da_get_buf(tp, dp, xfs_dir2_db_to_da(mp, blkno), -1, &bp,
		XFS_DATA_FORK);
	if (error) {
		return error;
	}
	ASSERT(bp != NULL);

	hdr = bp->data;
	hdr->magic = cpu_to_be32(XFS_DIR2_DATA_MAGIC);
	hdr->bestfree[0].offset = cpu_to_be16(sizeof(*hdr));
	for (i = 1; i < XFS_DIR2_DATA_FD_COUNT; i++) {
		hdr->bestfree[i].length = 0;
		hdr->bestfree[i].offset = 0;
	}

	dup = (xfs_dir2_data_unused_t *)(hdr + 1);
	dup->freetag = cpu_to_be16(XFS_DIR2_DATA_FREE_TAG);

	t = mp->m_dirblksize - (uint)sizeof(*hdr);
	hdr->bestfree[0].length = cpu_to_be16(t);
	dup->length = cpu_to_be16(t);
	*xfs_dir2_data_unused_tag_p(dup) = cpu_to_be16((char *)dup - (char *)hdr);
	xfs_dir2_data_log_header(tp, bp);
	xfs_dir2_data_log_unused(tp, bp, dup);
	*bpp = bp;
	return 0;
}

void
xfs_dir2_data_log_entry(
	xfs_trans_t		*tp,		
	xfs_dabuf_t		*bp,		
	xfs_dir2_data_entry_t	*dep)		
{
	xfs_dir2_data_hdr_t	*hdr = bp->data;

	ASSERT(hdr->magic == cpu_to_be32(XFS_DIR2_DATA_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR2_BLOCK_MAGIC));

	xfs_da_log_buf(tp, bp, (uint)((char *)dep - (char *)hdr),
		(uint)((char *)(xfs_dir2_data_entry_tag_p(dep) + 1) -
		       (char *)hdr - 1));
}

void
xfs_dir2_data_log_header(
	xfs_trans_t		*tp,		
	xfs_dabuf_t		*bp)		
{
	xfs_dir2_data_hdr_t	*hdr = bp->data;

	ASSERT(hdr->magic == cpu_to_be32(XFS_DIR2_DATA_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR2_BLOCK_MAGIC));

	xfs_da_log_buf(tp, bp, 0, sizeof(*hdr) - 1);
}

void
xfs_dir2_data_log_unused(
	xfs_trans_t		*tp,		
	xfs_dabuf_t		*bp,		
	xfs_dir2_data_unused_t	*dup)		
{
	xfs_dir2_data_hdr_t	*hdr = bp->data;

	ASSERT(hdr->magic == cpu_to_be32(XFS_DIR2_DATA_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR2_BLOCK_MAGIC));

	xfs_da_log_buf(tp, bp, (uint)((char *)dup - (char *)hdr),
		(uint)((char *)&dup->length + sizeof(dup->length) -
		       1 - (char *)hdr));
	xfs_da_log_buf(tp, bp,
		(uint)((char *)xfs_dir2_data_unused_tag_p(dup) - (char *)hdr),
		(uint)((char *)xfs_dir2_data_unused_tag_p(dup) - (char *)hdr +
		       sizeof(xfs_dir2_data_off_t) - 1));
}

void
xfs_dir2_data_make_free(
	xfs_trans_t		*tp,		
	xfs_dabuf_t		*bp,		
	xfs_dir2_data_aoff_t	offset,		
	xfs_dir2_data_aoff_t	len,		
	int			*needlogp,	
	int			*needscanp)	
{
	xfs_dir2_data_hdr_t	*hdr;		
	xfs_dir2_data_free_t	*dfp;		
	char			*endptr;	
	xfs_mount_t		*mp;		
	int			needscan;	
	xfs_dir2_data_unused_t	*newdup;	
	xfs_dir2_data_unused_t	*postdup;	
	xfs_dir2_data_unused_t	*prevdup;	

	mp = tp->t_mountp;
	hdr = bp->data;

	if (hdr->magic == cpu_to_be32(XFS_DIR2_DATA_MAGIC))
		endptr = (char *)hdr + mp->m_dirblksize;
	else {
		xfs_dir2_block_tail_t	*btp;	

		ASSERT(hdr->magic == cpu_to_be32(XFS_DIR2_BLOCK_MAGIC));
		btp = xfs_dir2_block_tail_p(mp, hdr);
		endptr = (char *)xfs_dir2_block_leaf_p(btp);
	}
	if (offset > sizeof(*hdr)) {
		__be16			*tagp;	

		tagp = (__be16 *)((char *)hdr + offset) - 1;
		prevdup = (xfs_dir2_data_unused_t *)((char *)hdr + be16_to_cpu(*tagp));
		if (be16_to_cpu(prevdup->freetag) != XFS_DIR2_DATA_FREE_TAG)
			prevdup = NULL;
	} else
		prevdup = NULL;
	if ((char *)hdr + offset + len < endptr) {
		postdup =
			(xfs_dir2_data_unused_t *)((char *)hdr + offset + len);
		if (be16_to_cpu(postdup->freetag) != XFS_DIR2_DATA_FREE_TAG)
			postdup = NULL;
	} else
		postdup = NULL;
	ASSERT(*needscanp == 0);
	needscan = 0;
	if (prevdup && postdup) {
		xfs_dir2_data_free_t	*dfp2;	

		dfp = xfs_dir2_data_freefind(hdr, prevdup);
		dfp2 = xfs_dir2_data_freefind(hdr, postdup);
		needscan = (hdr->bestfree[2].length != 0);
		be16_add_cpu(&prevdup->length, len + be16_to_cpu(postdup->length));
		*xfs_dir2_data_unused_tag_p(prevdup) =
			cpu_to_be16((char *)prevdup - (char *)hdr);
		xfs_dir2_data_log_unused(tp, bp, prevdup);
		if (!needscan) {
			ASSERT(dfp && dfp2);
			if (dfp == &hdr->bestfree[1]) {
				dfp = &hdr->bestfree[0];
				ASSERT(dfp2 == dfp);
				dfp2 = &hdr->bestfree[1];
			}
			xfs_dir2_data_freeremove(hdr, dfp2, needlogp);
			xfs_dir2_data_freeremove(hdr, dfp, needlogp);
			dfp = xfs_dir2_data_freeinsert(hdr, prevdup, needlogp);
			ASSERT(dfp == &hdr->bestfree[0]);
			ASSERT(dfp->length == prevdup->length);
			ASSERT(!dfp[1].length);
			ASSERT(!dfp[2].length);
		}
	}
	else if (prevdup) {
		dfp = xfs_dir2_data_freefind(hdr, prevdup);
		be16_add_cpu(&prevdup->length, len);
		*xfs_dir2_data_unused_tag_p(prevdup) =
			cpu_to_be16((char *)prevdup - (char *)hdr);
		xfs_dir2_data_log_unused(tp, bp, prevdup);
		if (dfp) {
			xfs_dir2_data_freeremove(hdr, dfp, needlogp);
			xfs_dir2_data_freeinsert(hdr, prevdup, needlogp);
		}
		else {
			needscan = be16_to_cpu(prevdup->length) >
				   be16_to_cpu(hdr->bestfree[2].length);
		}
	}
	else if (postdup) {
		dfp = xfs_dir2_data_freefind(hdr, postdup);
		newdup = (xfs_dir2_data_unused_t *)((char *)hdr + offset);
		newdup->freetag = cpu_to_be16(XFS_DIR2_DATA_FREE_TAG);
		newdup->length = cpu_to_be16(len + be16_to_cpu(postdup->length));
		*xfs_dir2_data_unused_tag_p(newdup) =
			cpu_to_be16((char *)newdup - (char *)hdr);
		xfs_dir2_data_log_unused(tp, bp, newdup);
		if (dfp) {
			xfs_dir2_data_freeremove(hdr, dfp, needlogp);
			xfs_dir2_data_freeinsert(hdr, newdup, needlogp);
		}
		else {
			needscan = be16_to_cpu(newdup->length) >
				   be16_to_cpu(hdr->bestfree[2].length);
		}
	}
	else {
		newdup = (xfs_dir2_data_unused_t *)((char *)hdr + offset);
		newdup->freetag = cpu_to_be16(XFS_DIR2_DATA_FREE_TAG);
		newdup->length = cpu_to_be16(len);
		*xfs_dir2_data_unused_tag_p(newdup) =
			cpu_to_be16((char *)newdup - (char *)hdr);
		xfs_dir2_data_log_unused(tp, bp, newdup);
		xfs_dir2_data_freeinsert(hdr, newdup, needlogp);
	}
	*needscanp = needscan;
}

void
xfs_dir2_data_use_free(
	xfs_trans_t		*tp,		
	xfs_dabuf_t		*bp,		
	xfs_dir2_data_unused_t	*dup,		
	xfs_dir2_data_aoff_t	offset,		
	xfs_dir2_data_aoff_t	len,		
	int			*needlogp,	
	int			*needscanp)	
{
	xfs_dir2_data_hdr_t	*hdr;		
	xfs_dir2_data_free_t	*dfp;		
	int			matchback;	
	int			matchfront;	
	int			needscan;	
	xfs_dir2_data_unused_t	*newdup;	
	xfs_dir2_data_unused_t	*newdup2;	
	int			oldlen;		

	hdr = bp->data;
	ASSERT(hdr->magic == cpu_to_be32(XFS_DIR2_DATA_MAGIC) ||
	       hdr->magic == cpu_to_be32(XFS_DIR2_BLOCK_MAGIC));
	ASSERT(be16_to_cpu(dup->freetag) == XFS_DIR2_DATA_FREE_TAG);
	ASSERT(offset >= (char *)dup - (char *)hdr);
	ASSERT(offset + len <= (char *)dup + be16_to_cpu(dup->length) - (char *)hdr);
	ASSERT((char *)dup - (char *)hdr == be16_to_cpu(*xfs_dir2_data_unused_tag_p(dup)));
	dfp = xfs_dir2_data_freefind(hdr, dup);
	oldlen = be16_to_cpu(dup->length);
	ASSERT(dfp || oldlen <= be16_to_cpu(hdr->bestfree[2].length));
	matchfront = (char *)dup - (char *)hdr == offset;
	matchback = (char *)dup + oldlen - (char *)hdr == offset + len;
	ASSERT(*needscanp == 0);
	needscan = 0;
	if (matchfront && matchback) {
		if (dfp) {
			needscan = (hdr->bestfree[2].offset != 0);
			if (!needscan)
				xfs_dir2_data_freeremove(hdr, dfp, needlogp);
		}
	}
	else if (matchfront) {
		newdup = (xfs_dir2_data_unused_t *)((char *)hdr + offset + len);
		newdup->freetag = cpu_to_be16(XFS_DIR2_DATA_FREE_TAG);
		newdup->length = cpu_to_be16(oldlen - len);
		*xfs_dir2_data_unused_tag_p(newdup) =
			cpu_to_be16((char *)newdup - (char *)hdr);
		xfs_dir2_data_log_unused(tp, bp, newdup);
		if (dfp) {
			xfs_dir2_data_freeremove(hdr, dfp, needlogp);
			dfp = xfs_dir2_data_freeinsert(hdr, newdup, needlogp);
			ASSERT(dfp != NULL);
			ASSERT(dfp->length == newdup->length);
			ASSERT(be16_to_cpu(dfp->offset) == (char *)newdup - (char *)hdr);
			needscan = dfp == &hdr->bestfree[2];
		}
	}
	else if (matchback) {
		newdup = dup;
		newdup->length = cpu_to_be16(((char *)hdr + offset) - (char *)newdup);
		*xfs_dir2_data_unused_tag_p(newdup) =
			cpu_to_be16((char *)newdup - (char *)hdr);
		xfs_dir2_data_log_unused(tp, bp, newdup);
		if (dfp) {
			xfs_dir2_data_freeremove(hdr, dfp, needlogp);
			dfp = xfs_dir2_data_freeinsert(hdr, newdup, needlogp);
			ASSERT(dfp != NULL);
			ASSERT(dfp->length == newdup->length);
			ASSERT(be16_to_cpu(dfp->offset) == (char *)newdup - (char *)hdr);
			needscan = dfp == &hdr->bestfree[2];
		}
	}
	else {
		newdup = dup;
		newdup->length = cpu_to_be16(((char *)hdr + offset) - (char *)newdup);
		*xfs_dir2_data_unused_tag_p(newdup) =
			cpu_to_be16((char *)newdup - (char *)hdr);
		xfs_dir2_data_log_unused(tp, bp, newdup);
		newdup2 = (xfs_dir2_data_unused_t *)((char *)hdr + offset + len);
		newdup2->freetag = cpu_to_be16(XFS_DIR2_DATA_FREE_TAG);
		newdup2->length = cpu_to_be16(oldlen - len - be16_to_cpu(newdup->length));
		*xfs_dir2_data_unused_tag_p(newdup2) =
			cpu_to_be16((char *)newdup2 - (char *)hdr);
		xfs_dir2_data_log_unused(tp, bp, newdup2);
		if (dfp) {
			needscan = (hdr->bestfree[2].length != 0);
			if (!needscan) {
				xfs_dir2_data_freeremove(hdr, dfp, needlogp);
				xfs_dir2_data_freeinsert(hdr, newdup, needlogp);
				xfs_dir2_data_freeinsert(hdr, newdup2,
							 needlogp);
			}
		}
	}
	*needscanp = needscan;
}
