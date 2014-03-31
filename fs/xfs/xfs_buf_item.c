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
#include "xfs_buf_item.h"
#include "xfs_trans_priv.h"
#include "xfs_error.h"
#include "xfs_trace.h"


kmem_zone_t	*xfs_buf_item_zone;

static inline struct xfs_buf_log_item *BUF_ITEM(struct xfs_log_item *lip)
{
	return container_of(lip, struct xfs_buf_log_item, bli_item);
}


#ifdef XFS_TRANS_DEBUG
STATIC void
xfs_buf_item_log_debug(
	xfs_buf_log_item_t	*bip,
	uint			first,
	uint			last)
{
	uint	x;
	uint	byte;
	uint	nbytes;
	uint	chunk_num;
	uint	word_num;
	uint	bit_num;
	uint	bit_set;
	uint	*wordp;

	ASSERT(bip->bli_logged != NULL);
	byte = first;
	nbytes = last - first + 1;
	bfset(bip->bli_logged, first, nbytes);
	for (x = 0; x < nbytes; x++) {
		chunk_num = byte >> XFS_BLF_SHIFT;
		word_num = chunk_num >> BIT_TO_WORD_SHIFT;
		bit_num = chunk_num & (NBWORD - 1);
		wordp = &(bip->bli_format.blf_data_map[word_num]);
		bit_set = *wordp & (1 << bit_num);
		ASSERT(bit_set);
		byte++;
	}
}

void
xfs_buf_item_flush_log_debug(
	xfs_buf_t	*bp,
	uint		first,
	uint		last)
{
	xfs_buf_log_item_t	*bip = bp->b_fspriv;
	uint			nbytes;

	if (bip == NULL || (bip->bli_item.li_type != XFS_LI_BUF))
		return;

	ASSERT(bip->bli_logged != NULL);
	nbytes = last - first + 1;
	bfset(bip->bli_logged, first, nbytes);
}

STATIC void
xfs_buf_item_log_check(
	xfs_buf_log_item_t	*bip)
{
	char		*orig;
	char		*buffer;
	int		x;
	xfs_buf_t	*bp;

	ASSERT(bip->bli_orig != NULL);
	ASSERT(bip->bli_logged != NULL);

	bp = bip->bli_buf;
	ASSERT(XFS_BUF_COUNT(bp) > 0);
	ASSERT(bp->b_addr != NULL);
	orig = bip->bli_orig;
	buffer = bp->b_addr;
	for (x = 0; x < XFS_BUF_COUNT(bp); x++) {
		if (orig[x] != buffer[x] && !btst(bip->bli_logged, x)) {
			xfs_emerg(bp->b_mount,
				"%s: bip %x buffer %x orig %x index %d",
				__func__, bip, bp, orig, x);
			ASSERT(0);
		}
	}
}
#else
#define		xfs_buf_item_log_debug(x,y,z)
#define		xfs_buf_item_log_check(x)
#endif

STATIC void	xfs_buf_do_callbacks(struct xfs_buf *bp);

STATIC uint
xfs_buf_item_size(
	struct xfs_log_item	*lip)
{
	struct xfs_buf_log_item	*bip = BUF_ITEM(lip);
	struct xfs_buf		*bp = bip->bli_buf;
	uint			nvecs;
	int			next_bit;
	int			last_bit;

	ASSERT(atomic_read(&bip->bli_refcount) > 0);
	if (bip->bli_flags & XFS_BLI_STALE) {
		trace_xfs_buf_item_size_stale(bip);
		ASSERT(bip->bli_format.blf_flags & XFS_BLF_CANCEL);
		return 1;
	}

	ASSERT(bip->bli_flags & XFS_BLI_LOGGED);
	nvecs = 1;
	last_bit = xfs_next_bit(bip->bli_format.blf_data_map,
					 bip->bli_format.blf_map_size, 0);
	ASSERT(last_bit != -1);
	nvecs++;
	while (last_bit != -1) {
		next_bit = xfs_next_bit(bip->bli_format.blf_data_map,
						 bip->bli_format.blf_map_size,
						 last_bit + 1);
		if (next_bit == -1) {
			last_bit = -1;
		} else if (next_bit != last_bit + 1) {
			last_bit = next_bit;
			nvecs++;
		} else if (xfs_buf_offset(bp, next_bit * XFS_BLF_CHUNK) !=
			   (xfs_buf_offset(bp, last_bit * XFS_BLF_CHUNK) +
			    XFS_BLF_CHUNK)) {
			last_bit = next_bit;
			nvecs++;
		} else {
			last_bit++;
		}
	}

	trace_xfs_buf_item_size(bip);
	return nvecs;
}

STATIC void
xfs_buf_item_format(
	struct xfs_log_item	*lip,
	struct xfs_log_iovec	*vecp)
{
	struct xfs_buf_log_item	*bip = BUF_ITEM(lip);
	struct xfs_buf	*bp = bip->bli_buf;
	uint		base_size;
	uint		nvecs;
	int		first_bit;
	int		last_bit;
	int		next_bit;
	uint		nbits;
	uint		buffer_offset;

	ASSERT(atomic_read(&bip->bli_refcount) > 0);
	ASSERT((bip->bli_flags & XFS_BLI_LOGGED) ||
	       (bip->bli_flags & XFS_BLI_STALE));

	base_size =
		(uint)(sizeof(xfs_buf_log_format_t) +
		       ((bip->bli_format.blf_map_size - 1) * sizeof(uint)));
	vecp->i_addr = &bip->bli_format;
	vecp->i_len = base_size;
	vecp->i_type = XLOG_REG_TYPE_BFORMAT;
	vecp++;
	nvecs = 1;

	if (bip->bli_flags & XFS_BLI_INODE_BUF) {
		if (!((bip->bli_flags & XFS_BLI_INODE_ALLOC_BUF) &&
		      xfs_log_item_in_current_chkpt(lip)))
			bip->bli_format.blf_flags |= XFS_BLF_INODE_BUF;
		bip->bli_flags &= ~XFS_BLI_INODE_BUF;
	}

	if (bip->bli_flags & XFS_BLI_STALE) {
		trace_xfs_buf_item_format_stale(bip);
		ASSERT(bip->bli_format.blf_flags & XFS_BLF_CANCEL);
		bip->bli_format.blf_size = nvecs;
		return;
	}

	first_bit = xfs_next_bit(bip->bli_format.blf_data_map,
					 bip->bli_format.blf_map_size, 0);
	ASSERT(first_bit != -1);
	last_bit = first_bit;
	nbits = 1;
	for (;;) {
		next_bit = xfs_next_bit(bip->bli_format.blf_data_map,
						 bip->bli_format.blf_map_size,
						 (uint)last_bit + 1);
		if (next_bit == -1) {
			buffer_offset = first_bit * XFS_BLF_CHUNK;
			vecp->i_addr = xfs_buf_offset(bp, buffer_offset);
			vecp->i_len = nbits * XFS_BLF_CHUNK;
			vecp->i_type = XLOG_REG_TYPE_BCHUNK;
			nvecs++;
			break;
		} else if (next_bit != last_bit + 1) {
			buffer_offset = first_bit * XFS_BLF_CHUNK;
			vecp->i_addr = xfs_buf_offset(bp, buffer_offset);
			vecp->i_len = nbits * XFS_BLF_CHUNK;
			vecp->i_type = XLOG_REG_TYPE_BCHUNK;
			nvecs++;
			vecp++;
			first_bit = next_bit;
			last_bit = next_bit;
			nbits = 1;
		} else if (xfs_buf_offset(bp, next_bit << XFS_BLF_SHIFT) !=
			   (xfs_buf_offset(bp, last_bit << XFS_BLF_SHIFT) +
			    XFS_BLF_CHUNK)) {
			buffer_offset = first_bit * XFS_BLF_CHUNK;
			vecp->i_addr = xfs_buf_offset(bp, buffer_offset);
			vecp->i_len = nbits * XFS_BLF_CHUNK;
			vecp->i_type = XLOG_REG_TYPE_BCHUNK;
			vecp++;
			first_bit = next_bit;
			last_bit = next_bit;
			nbits = 1;
		} else {
			last_bit++;
			nbits++;
		}
	}
	bip->bli_format.blf_size = nvecs;

	trace_xfs_buf_item_format(bip);
	xfs_buf_item_log_check(bip);
}

/*
 * This is called to pin the buffer associated with the buf log item in memory
 * so it cannot be written out.
 *
 * We also always take a reference to the buffer log item here so that the bli
 * is held while the item is pinned in memory. This means that we can
 * unconditionally drop the reference count a transaction holds when the
 * transaction is completed.
 */
STATIC void
xfs_buf_item_pin(
	struct xfs_log_item	*lip)
{
	struct xfs_buf_log_item	*bip = BUF_ITEM(lip);

	ASSERT(atomic_read(&bip->bli_refcount) > 0);
	ASSERT((bip->bli_flags & XFS_BLI_LOGGED) ||
	       (bip->bli_flags & XFS_BLI_STALE));

	trace_xfs_buf_item_pin(bip);

	atomic_inc(&bip->bli_refcount);
	atomic_inc(&bip->bli_buf->b_pin_count);
}

STATIC void
xfs_buf_item_unpin(
	struct xfs_log_item	*lip,
	int			remove)
{
	struct xfs_buf_log_item	*bip = BUF_ITEM(lip);
	xfs_buf_t	*bp = bip->bli_buf;
	struct xfs_ail	*ailp = lip->li_ailp;
	int		stale = bip->bli_flags & XFS_BLI_STALE;
	int		freed;

	ASSERT(bp->b_fspriv == bip);
	ASSERT(atomic_read(&bip->bli_refcount) > 0);

	trace_xfs_buf_item_unpin(bip);

	freed = atomic_dec_and_test(&bip->bli_refcount);

	if (atomic_dec_and_test(&bp->b_pin_count))
		wake_up_all(&bp->b_waiters);

	if (freed && stale) {
		ASSERT(bip->bli_flags & XFS_BLI_STALE);
		ASSERT(xfs_buf_islocked(bp));
		ASSERT(!(XFS_BUF_ISDELAYWRITE(bp)));
		ASSERT(XFS_BUF_ISSTALE(bp));
		ASSERT(bip->bli_format.blf_flags & XFS_BLF_CANCEL);

		trace_xfs_buf_item_unpin_stale(bip);

		if (remove) {
			if (lip->li_desc)
				xfs_trans_del_item(lip);

			bp->b_transp = NULL;
		}

		if (bip->bli_flags & XFS_BLI_STALE_INODE) {
			xfs_buf_do_callbacks(bp);
			bp->b_fspriv = NULL;
			bp->b_iodone = NULL;
		} else {
			spin_lock(&ailp->xa_lock);
			xfs_trans_ail_delete(ailp, (xfs_log_item_t *)bip);
			xfs_buf_item_relse(bp);
			ASSERT(bp->b_fspriv == NULL);
		}
		xfs_buf_relse(bp);
	}
}

/*
 * This is called to attempt to lock the buffer associated with this
 * buf log item.  Don't sleep on the buffer lock.  If we can't get
 * the lock right away, return 0.  If we can get the lock, take a
 * reference to the buffer. If this is a delayed write buffer that
 * needs AIL help to be written back, invoke the pushbuf routine
 * rather than the normal success path.
 */
STATIC uint
xfs_buf_item_trylock(
	struct xfs_log_item	*lip)
{
	struct xfs_buf_log_item	*bip = BUF_ITEM(lip);
	struct xfs_buf		*bp = bip->bli_buf;

	if (xfs_buf_ispinned(bp))
		return XFS_ITEM_PINNED;
	if (!xfs_buf_trylock(bp))
		return XFS_ITEM_LOCKED;

	
	xfs_buf_hold(bp);

	ASSERT(!(bip->bli_flags & XFS_BLI_STALE));
	trace_xfs_buf_item_trylock(bip);
	if (XFS_BUF_ISDELAYWRITE(bp))
		return XFS_ITEM_PUSHBUF;
	return XFS_ITEM_SUCCESS;
}

STATIC void
xfs_buf_item_unlock(
	struct xfs_log_item	*lip)
{
	struct xfs_buf_log_item	*bip = BUF_ITEM(lip);
	struct xfs_buf		*bp = bip->bli_buf;
	int			aborted;
	uint			hold;

	
	bp->b_transp = NULL;

	aborted = (lip->li_flags & XFS_LI_ABORTED) != 0;

	hold = bip->bli_flags & XFS_BLI_HOLD;

	
	bip->bli_flags &= ~(XFS_BLI_LOGGED | XFS_BLI_HOLD);

	if (bip->bli_flags & XFS_BLI_STALE) {
		trace_xfs_buf_item_unlock_stale(bip);
		ASSERT(bip->bli_format.blf_flags & XFS_BLF_CANCEL);
		if (!aborted) {
			atomic_dec(&bip->bli_refcount);
			return;
		}
	}

	trace_xfs_buf_item_unlock(bip);

	if (xfs_bitmap_empty(bip->bli_format.blf_data_map,
			     bip->bli_format.blf_map_size))
		xfs_buf_item_relse(bp);
	else
		atomic_dec(&bip->bli_refcount);

	if (!hold)
		xfs_buf_relse(bp);
}

STATIC xfs_lsn_t
xfs_buf_item_committed(
	struct xfs_log_item	*lip,
	xfs_lsn_t		lsn)
{
	struct xfs_buf_log_item	*bip = BUF_ITEM(lip);

	trace_xfs_buf_item_committed(bip);

	if ((bip->bli_flags & XFS_BLI_INODE_ALLOC_BUF) && lip->li_lsn != 0)
		return lip->li_lsn;
	return lsn;
}

STATIC void
xfs_buf_item_push(
	struct xfs_log_item	*lip)
{
	struct xfs_buf_log_item	*bip = BUF_ITEM(lip);
	struct xfs_buf		*bp = bip->bli_buf;

	ASSERT(!(bip->bli_flags & XFS_BLI_STALE));
	ASSERT(!XFS_BUF_ISDELAYWRITE(bp));

	trace_xfs_buf_item_push(bip);

	xfs_buf_relse(bp);
}

/*
 * The buffer is locked and is a delayed write buffer. Promote the buffer
 * in the delayed write queue as the caller knows that they must invoke
 * the xfsbufd to get this buffer written. We have to unlock the buffer
 * to allow the xfsbufd to write it, too.
 */
STATIC bool
xfs_buf_item_pushbuf(
	struct xfs_log_item	*lip)
{
	struct xfs_buf_log_item	*bip = BUF_ITEM(lip);
	struct xfs_buf		*bp = bip->bli_buf;

	ASSERT(!(bip->bli_flags & XFS_BLI_STALE));
	ASSERT(XFS_BUF_ISDELAYWRITE(bp));

	trace_xfs_buf_item_pushbuf(bip);

	xfs_buf_delwri_promote(bp);
	xfs_buf_relse(bp);
	return true;
}

STATIC void
xfs_buf_item_committing(
	struct xfs_log_item	*lip,
	xfs_lsn_t		commit_lsn)
{
}

static const struct xfs_item_ops xfs_buf_item_ops = {
	.iop_size	= xfs_buf_item_size,
	.iop_format	= xfs_buf_item_format,
	.iop_pin	= xfs_buf_item_pin,
	.iop_unpin	= xfs_buf_item_unpin,
	.iop_trylock	= xfs_buf_item_trylock,
	.iop_unlock	= xfs_buf_item_unlock,
	.iop_committed	= xfs_buf_item_committed,
	.iop_push	= xfs_buf_item_push,
	.iop_pushbuf	= xfs_buf_item_pushbuf,
	.iop_committing = xfs_buf_item_committing
};


void
xfs_buf_item_init(
	xfs_buf_t	*bp,
	xfs_mount_t	*mp)
{
	xfs_log_item_t		*lip = bp->b_fspriv;
	xfs_buf_log_item_t	*bip;
	int			chunks;
	int			map_size;

	ASSERT(bp->b_target->bt_mount == mp);
	if (lip != NULL && lip->li_type == XFS_LI_BUF)
		return;

	chunks = (int)((XFS_BUF_COUNT(bp) + (XFS_BLF_CHUNK - 1)) >> XFS_BLF_SHIFT);
	map_size = (int)((chunks + NBWORD) >> BIT_TO_WORD_SHIFT);

	bip = (xfs_buf_log_item_t*)kmem_zone_zalloc(xfs_buf_item_zone,
						    KM_SLEEP);
	xfs_log_item_init(mp, &bip->bli_item, XFS_LI_BUF, &xfs_buf_item_ops);
	bip->bli_buf = bp;
	xfs_buf_hold(bp);
	bip->bli_format.blf_type = XFS_LI_BUF;
	bip->bli_format.blf_blkno = (__int64_t)XFS_BUF_ADDR(bp);
	bip->bli_format.blf_len = (ushort)BTOBB(XFS_BUF_COUNT(bp));
	bip->bli_format.blf_map_size = map_size;

#ifdef XFS_TRANS_DEBUG
	bip->bli_orig = (char *)kmem_alloc(XFS_BUF_COUNT(bp), KM_SLEEP);
	memcpy(bip->bli_orig, bp->b_addr, XFS_BUF_COUNT(bp));
	bip->bli_logged = (char *)kmem_zalloc(XFS_BUF_COUNT(bp) / NBBY, KM_SLEEP);
#endif

	if (bp->b_fspriv)
		bip->bli_item.li_bio_list = bp->b_fspriv;
	bp->b_fspriv = bip;
}


void
xfs_buf_item_log(
	xfs_buf_log_item_t	*bip,
	uint			first,
	uint			last)
{
	uint		first_bit;
	uint		last_bit;
	uint		bits_to_set;
	uint		bits_set;
	uint		word_num;
	uint		*wordp;
	uint		bit;
	uint		end_bit;
	uint		mask;

	bip->bli_flags |= XFS_BLI_DIRTY;

	first_bit = first >> XFS_BLF_SHIFT;
	last_bit = last >> XFS_BLF_SHIFT;

	bits_to_set = last_bit - first_bit + 1;

	word_num = first_bit >> BIT_TO_WORD_SHIFT;
	wordp = &(bip->bli_format.blf_data_map[word_num]);

	bit = first_bit & (uint)(NBWORD - 1);

	if (bit) {
		end_bit = MIN(bit + bits_to_set, (uint)NBWORD);
		mask = ((1 << (end_bit - bit)) - 1) << bit;
		*wordp |= mask;
		wordp++;
		bits_set = end_bit - bit;
	} else {
		bits_set = 0;
	}

	while ((bits_to_set - bits_set) >= NBWORD) {
		*wordp |= 0xffffffff;
		bits_set += NBWORD;
		wordp++;
	}

	end_bit = bits_to_set - bits_set;
	if (end_bit) {
		mask = (1 << end_bit) - 1;
		*wordp |= mask;
	}

	xfs_buf_item_log_debug(bip, first, last);
}


uint
xfs_buf_item_dirty(
	xfs_buf_log_item_t	*bip)
{
	return (bip->bli_flags & XFS_BLI_DIRTY);
}

STATIC void
xfs_buf_item_free(
	xfs_buf_log_item_t	*bip)
{
#ifdef XFS_TRANS_DEBUG
	kmem_free(bip->bli_orig);
	kmem_free(bip->bli_logged);
#endif 

	kmem_zone_free(xfs_buf_item_zone, bip);
}

void
xfs_buf_item_relse(
	xfs_buf_t	*bp)
{
	xfs_buf_log_item_t	*bip;

	trace_xfs_buf_item_relse(bp, _RET_IP_);

	bip = bp->b_fspriv;
	bp->b_fspriv = bip->bli_item.li_bio_list;
	if (bp->b_fspriv == NULL)
		bp->b_iodone = NULL;

	xfs_buf_rele(bp);
	xfs_buf_item_free(bip);
}


void
xfs_buf_attach_iodone(
	xfs_buf_t	*bp,
	void		(*cb)(xfs_buf_t *, xfs_log_item_t *),
	xfs_log_item_t	*lip)
{
	xfs_log_item_t	*head_lip;

	ASSERT(xfs_buf_islocked(bp));

	lip->li_cb = cb;
	head_lip = bp->b_fspriv;
	if (head_lip) {
		lip->li_bio_list = head_lip->li_bio_list;
		head_lip->li_bio_list = lip;
	} else {
		bp->b_fspriv = lip;
	}

	ASSERT(bp->b_iodone == NULL ||
	       bp->b_iodone == xfs_buf_iodone_callbacks);
	bp->b_iodone = xfs_buf_iodone_callbacks;
}

STATIC void
xfs_buf_do_callbacks(
	struct xfs_buf		*bp)
{
	struct xfs_log_item	*lip;

	while ((lip = bp->b_fspriv) != NULL) {
		bp->b_fspriv = lip->li_bio_list;
		ASSERT(lip->li_cb != NULL);
		lip->li_bio_list = NULL;
		lip->li_cb(bp, lip);
	}
}

void
xfs_buf_iodone_callbacks(
	struct xfs_buf		*bp)
{
	struct xfs_log_item	*lip = bp->b_fspriv;
	struct xfs_mount	*mp = lip->li_mountp;
	static ulong		lasttime;
	static xfs_buftarg_t	*lasttarg;

	if (likely(!xfs_buf_geterror(bp)))
		goto do_callbacks;

	if (XFS_FORCED_SHUTDOWN(mp)) {
		xfs_buf_stale(bp);
		XFS_BUF_DONE(bp);
		trace_xfs_buf_item_iodone(bp, _RET_IP_);
		goto do_callbacks;
	}

	if (bp->b_target != lasttarg ||
	    time_after(jiffies, (lasttime + 5*HZ))) {
		lasttime = jiffies;
		xfs_buf_ioerror_alert(bp, __func__);
	}
	lasttarg = bp->b_target;

	if (XFS_BUF_ISASYNC(bp)) {
		xfs_buf_ioerror(bp, 0); 

		if (!XFS_BUF_ISSTALE(bp)) {
			xfs_buf_delwri_queue(bp);
			XFS_BUF_DONE(bp);
		}
		ASSERT(bp->b_iodone != NULL);
		trace_xfs_buf_item_iodone_async(bp, _RET_IP_);
		xfs_buf_relse(bp);
		return;
	}

	xfs_buf_stale(bp);
	XFS_BUF_DONE(bp);

	trace_xfs_buf_error_relse(bp, _RET_IP_);

do_callbacks:
	xfs_buf_do_callbacks(bp);
	bp->b_fspriv = NULL;
	bp->b_iodone = NULL;
	xfs_buf_ioend(bp, 0);
}

void
xfs_buf_iodone(
	struct xfs_buf		*bp,
	struct xfs_log_item	*lip)
{
	struct xfs_ail		*ailp = lip->li_ailp;

	ASSERT(BUF_ITEM(lip)->bli_buf == bp);

	xfs_buf_rele(bp);

	spin_lock(&ailp->xa_lock);
	xfs_trans_ail_delete(ailp, lip);
	xfs_buf_item_free(BUF_ITEM(lip));
}
