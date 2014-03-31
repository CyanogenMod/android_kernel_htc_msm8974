/*
 * Copyright (c) 2000-2006 Silicon Graphics, Inc.
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
#include "xfs_error.h"
#include "xfs_bmap_btree.h"
#include "xfs_alloc_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_inode_item.h"
#include "xfs_alloc.h"
#include "xfs_ialloc.h"
#include "xfs_log_priv.h"
#include "xfs_buf_item.h"
#include "xfs_log_recover.h"
#include "xfs_extfree_item.h"
#include "xfs_trans_priv.h"
#include "xfs_quota.h"
#include "xfs_rw.h"
#include "xfs_utils.h"
#include "xfs_trace.h"

STATIC int	xlog_find_zeroed(xlog_t *, xfs_daddr_t *);
STATIC int	xlog_clear_stale_blocks(xlog_t *, xfs_lsn_t);
#if defined(DEBUG)
STATIC void	xlog_recover_check_summary(xlog_t *);
#else
#define	xlog_recover_check_summary(log)
#endif

struct xfs_buf_cancel {
	xfs_daddr_t		bc_blkno;
	uint			bc_len;
	int			bc_refcount;
	struct list_head	bc_list;
};



static inline int
xlog_buf_bbcount_valid(
	xlog_t		*log,
	int		bbcount)
{
	return bbcount > 0 && bbcount <= log->l_logBBsize;
}

STATIC xfs_buf_t *
xlog_get_bp(
	xlog_t		*log,
	int		nbblks)
{
	struct xfs_buf	*bp;

	if (!xlog_buf_bbcount_valid(log, nbblks)) {
		xfs_warn(log->l_mp, "Invalid block length (0x%x) for buffer",
			nbblks);
		XFS_ERROR_REPORT(__func__, XFS_ERRLEVEL_HIGH, log->l_mp);
		return NULL;
	}

	if (nbblks > 1 && log->l_sectBBsize > 1)
		nbblks += log->l_sectBBsize;
	nbblks = round_up(nbblks, log->l_sectBBsize);

	bp = xfs_buf_get_uncached(log->l_mp->m_logdev_targp, BBTOB(nbblks), 0);
	if (bp)
		xfs_buf_unlock(bp);
	return bp;
}

STATIC void
xlog_put_bp(
	xfs_buf_t	*bp)
{
	xfs_buf_free(bp);
}

STATIC xfs_caddr_t
xlog_align(
	xlog_t		*log,
	xfs_daddr_t	blk_no,
	int		nbblks,
	xfs_buf_t	*bp)
{
	xfs_daddr_t	offset = blk_no & ((xfs_daddr_t)log->l_sectBBsize - 1);

	ASSERT(BBTOB(offset + nbblks) <= XFS_BUF_SIZE(bp));
	return bp->b_addr + BBTOB(offset);
}


STATIC int
xlog_bread_noalign(
	xlog_t		*log,
	xfs_daddr_t	blk_no,
	int		nbblks,
	xfs_buf_t	*bp)
{
	int		error;

	if (!xlog_buf_bbcount_valid(log, nbblks)) {
		xfs_warn(log->l_mp, "Invalid block length (0x%x) for buffer",
			nbblks);
		XFS_ERROR_REPORT(__func__, XFS_ERRLEVEL_HIGH, log->l_mp);
		return EFSCORRUPTED;
	}

	blk_no = round_down(blk_no, log->l_sectBBsize);
	nbblks = round_up(nbblks, log->l_sectBBsize);

	ASSERT(nbblks > 0);
	ASSERT(BBTOB(nbblks) <= XFS_BUF_SIZE(bp));

	XFS_BUF_SET_ADDR(bp, log->l_logBBstart + blk_no);
	XFS_BUF_READ(bp);
	XFS_BUF_SET_COUNT(bp, BBTOB(nbblks));

	xfsbdstrat(log->l_mp, bp);
	error = xfs_buf_iowait(bp);
	if (error)
		xfs_buf_ioerror_alert(bp, __func__);
	return error;
}

STATIC int
xlog_bread(
	xlog_t		*log,
	xfs_daddr_t	blk_no,
	int		nbblks,
	xfs_buf_t	*bp,
	xfs_caddr_t	*offset)
{
	int		error;

	error = xlog_bread_noalign(log, blk_no, nbblks, bp);
	if (error)
		return error;

	*offset = xlog_align(log, blk_no, nbblks, bp);
	return 0;
}

STATIC int
xlog_bread_offset(
	xlog_t		*log,
	xfs_daddr_t	blk_no,		
	int		nbblks,		
	xfs_buf_t	*bp,
	xfs_caddr_t	offset)
{
	xfs_caddr_t	orig_offset = bp->b_addr;
	int		orig_len = bp->b_buffer_length;
	int		error, error2;

	error = xfs_buf_associate_memory(bp, offset, BBTOB(nbblks));
	if (error)
		return error;

	error = xlog_bread_noalign(log, blk_no, nbblks, bp);

	
	error2 = xfs_buf_associate_memory(bp, orig_offset, orig_len);
	if (error)
		return error;
	return error2;
}

STATIC int
xlog_bwrite(
	xlog_t		*log,
	xfs_daddr_t	blk_no,
	int		nbblks,
	xfs_buf_t	*bp)
{
	int		error;

	if (!xlog_buf_bbcount_valid(log, nbblks)) {
		xfs_warn(log->l_mp, "Invalid block length (0x%x) for buffer",
			nbblks);
		XFS_ERROR_REPORT(__func__, XFS_ERRLEVEL_HIGH, log->l_mp);
		return EFSCORRUPTED;
	}

	blk_no = round_down(blk_no, log->l_sectBBsize);
	nbblks = round_up(nbblks, log->l_sectBBsize);

	ASSERT(nbblks > 0);
	ASSERT(BBTOB(nbblks) <= XFS_BUF_SIZE(bp));

	XFS_BUF_SET_ADDR(bp, log->l_logBBstart + blk_no);
	XFS_BUF_ZEROFLAGS(bp);
	xfs_buf_hold(bp);
	xfs_buf_lock(bp);
	XFS_BUF_SET_COUNT(bp, BBTOB(nbblks));

	error = xfs_bwrite(bp);
	if (error)
		xfs_buf_ioerror_alert(bp, __func__);
	xfs_buf_relse(bp);
	return error;
}

#ifdef DEBUG
STATIC void
xlog_header_check_dump(
	xfs_mount_t		*mp,
	xlog_rec_header_t	*head)
{
	xfs_debug(mp, "%s:  SB : uuid = %pU, fmt = %d\n",
		__func__, &mp->m_sb.sb_uuid, XLOG_FMT);
	xfs_debug(mp, "    log : uuid = %pU, fmt = %d\n",
		&head->h_fs_uuid, be32_to_cpu(head->h_fmt));
}
#else
#define xlog_header_check_dump(mp, head)
#endif

STATIC int
xlog_header_check_recover(
	xfs_mount_t		*mp,
	xlog_rec_header_t	*head)
{
	ASSERT(head->h_magicno == cpu_to_be32(XLOG_HEADER_MAGIC_NUM));

	if (unlikely(head->h_fmt != cpu_to_be32(XLOG_FMT))) {
		xfs_warn(mp,
	"dirty log written in incompatible format - can't recover");
		xlog_header_check_dump(mp, head);
		XFS_ERROR_REPORT("xlog_header_check_recover(1)",
				 XFS_ERRLEVEL_HIGH, mp);
		return XFS_ERROR(EFSCORRUPTED);
	} else if (unlikely(!uuid_equal(&mp->m_sb.sb_uuid, &head->h_fs_uuid))) {
		xfs_warn(mp,
	"dirty log entry has mismatched uuid - can't recover");
		xlog_header_check_dump(mp, head);
		XFS_ERROR_REPORT("xlog_header_check_recover(2)",
				 XFS_ERRLEVEL_HIGH, mp);
		return XFS_ERROR(EFSCORRUPTED);
	}
	return 0;
}

STATIC int
xlog_header_check_mount(
	xfs_mount_t		*mp,
	xlog_rec_header_t	*head)
{
	ASSERT(head->h_magicno == cpu_to_be32(XLOG_HEADER_MAGIC_NUM));

	if (uuid_is_nil(&head->h_fs_uuid)) {
		xfs_warn(mp, "nil uuid in log - IRIX style log");
	} else if (unlikely(!uuid_equal(&mp->m_sb.sb_uuid, &head->h_fs_uuid))) {
		xfs_warn(mp, "log has mismatched uuid - can't recover");
		xlog_header_check_dump(mp, head);
		XFS_ERROR_REPORT("xlog_header_check_mount",
				 XFS_ERRLEVEL_HIGH, mp);
		return XFS_ERROR(EFSCORRUPTED);
	}
	return 0;
}

STATIC void
xlog_recover_iodone(
	struct xfs_buf	*bp)
{
	if (bp->b_error) {
		xfs_buf_ioerror_alert(bp, __func__);
		xfs_force_shutdown(bp->b_target->bt_mount,
					SHUTDOWN_META_IO_ERROR);
	}
	bp->b_iodone = NULL;
	xfs_buf_ioend(bp, 0);
}

STATIC int
xlog_find_cycle_start(
	xlog_t		*log,
	xfs_buf_t	*bp,
	xfs_daddr_t	first_blk,
	xfs_daddr_t	*last_blk,
	uint		cycle)
{
	xfs_caddr_t	offset;
	xfs_daddr_t	mid_blk;
	xfs_daddr_t	end_blk;
	uint		mid_cycle;
	int		error;

	end_blk = *last_blk;
	mid_blk = BLK_AVG(first_blk, end_blk);
	while (mid_blk != first_blk && mid_blk != end_blk) {
		error = xlog_bread(log, mid_blk, 1, bp, &offset);
		if (error)
			return error;
		mid_cycle = xlog_get_cycle(offset);
		if (mid_cycle == cycle)
			end_blk = mid_blk;   
		else
			first_blk = mid_blk; 
		mid_blk = BLK_AVG(first_blk, end_blk);
	}
	ASSERT((mid_blk == first_blk && mid_blk+1 == end_blk) ||
	       (mid_blk == end_blk && mid_blk-1 == first_blk));

	*last_blk = end_blk;

	return 0;
}

STATIC int
xlog_find_verify_cycle(
	xlog_t		*log,
	xfs_daddr_t	start_blk,
	int		nbblks,
	uint		stop_on_cycle_no,
	xfs_daddr_t	*new_blk)
{
	xfs_daddr_t	i, j;
	uint		cycle;
	xfs_buf_t	*bp;
	xfs_daddr_t	bufblks;
	xfs_caddr_t	buf = NULL;
	int		error = 0;

	bufblks = 1 << ffs(nbblks);
	while (!(bp = xlog_get_bp(log, bufblks))) {
		bufblks >>= 1;
		if (bufblks < log->l_sectBBsize)
			return ENOMEM;
	}

	for (i = start_blk; i < start_blk + nbblks; i += bufblks) {
		int	bcount;

		bcount = min(bufblks, (start_blk + nbblks - i));

		error = xlog_bread(log, i, bcount, bp, &buf);
		if (error)
			goto out;

		for (j = 0; j < bcount; j++) {
			cycle = xlog_get_cycle(buf);
			if (cycle == stop_on_cycle_no) {
				*new_blk = i+j;
				goto out;
			}

			buf += BBSIZE;
		}
	}

	*new_blk = -1;

out:
	xlog_put_bp(bp);
	return error;
}

STATIC int
xlog_find_verify_log_record(
	xlog_t			*log,
	xfs_daddr_t		start_blk,
	xfs_daddr_t		*last_blk,
	int			extra_bblks)
{
	xfs_daddr_t		i;
	xfs_buf_t		*bp;
	xfs_caddr_t		offset = NULL;
	xlog_rec_header_t	*head = NULL;
	int			error = 0;
	int			smallmem = 0;
	int			num_blks = *last_blk - start_blk;
	int			xhdrs;

	ASSERT(start_blk != 0 || *last_blk != start_blk);

	if (!(bp = xlog_get_bp(log, num_blks))) {
		if (!(bp = xlog_get_bp(log, 1)))
			return ENOMEM;
		smallmem = 1;
	} else {
		error = xlog_bread(log, start_blk, num_blks, bp, &offset);
		if (error)
			goto out;
		offset += ((num_blks - 1) << BBSHIFT);
	}

	for (i = (*last_blk) - 1; i >= 0; i--) {
		if (i < start_blk) {
			
			xfs_warn(log->l_mp,
		"Log inconsistent (didn't find previous header)");
			ASSERT(0);
			error = XFS_ERROR(EIO);
			goto out;
		}

		if (smallmem) {
			error = xlog_bread(log, i, 1, bp, &offset);
			if (error)
				goto out;
		}

		head = (xlog_rec_header_t *)offset;

		if (head->h_magicno == cpu_to_be32(XLOG_HEADER_MAGIC_NUM))
			break;

		if (!smallmem)
			offset -= BBSIZE;
	}

	if (i == -1) {
		error = -1;
		goto out;
	}

	if ((error = xlog_header_check_mount(log->l_mp, head)))
		goto out;

	if (xfs_sb_version_haslogv2(&log->l_mp->m_sb)) {
		uint	h_size = be32_to_cpu(head->h_size);

		xhdrs = h_size / XLOG_HEADER_CYCLE_SIZE;
		if (h_size % XLOG_HEADER_CYCLE_SIZE)
			xhdrs++;
	} else {
		xhdrs = 1;
	}

	if (*last_blk - i + extra_bblks !=
	    BTOBB(be32_to_cpu(head->h_len)) + xhdrs)
		*last_blk = i;

out:
	xlog_put_bp(bp);
	return error;
}

STATIC int
xlog_find_head(
	xlog_t 		*log,
	xfs_daddr_t	*return_head_blk)
{
	xfs_buf_t	*bp;
	xfs_caddr_t	offset;
	xfs_daddr_t	new_blk, first_blk, start_blk, last_blk, head_blk;
	int		num_scan_bblks;
	uint		first_half_cycle, last_half_cycle;
	uint		stop_on_cycle;
	int		error, log_bbnum = log->l_logBBsize;

	
	if ((error = xlog_find_zeroed(log, &first_blk)) == -1) {
		*return_head_blk = first_blk;

		
		if (!first_blk) {
			xfs_warn(log->l_mp, "totally zeroed log");
		}

		return 0;
	} else if (error) {
		xfs_warn(log->l_mp, "empty log check failed");
		return error;
	}

	first_blk = 0;			
	bp = xlog_get_bp(log, 1);
	if (!bp)
		return ENOMEM;

	error = xlog_bread(log, 0, 1, bp, &offset);
	if (error)
		goto bp_err;

	first_half_cycle = xlog_get_cycle(offset);

	last_blk = head_blk = log_bbnum - 1;	
	error = xlog_bread(log, last_blk, 1, bp, &offset);
	if (error)
		goto bp_err;

	last_half_cycle = xlog_get_cycle(offset);
	ASSERT(last_half_cycle != 0);

	if (first_half_cycle == last_half_cycle) {
		head_blk = log_bbnum;
		stop_on_cycle = last_half_cycle - 1;
	} else {
		stop_on_cycle = last_half_cycle;
		if ((error = xlog_find_cycle_start(log, bp, first_blk,
						&head_blk, last_half_cycle)))
			goto bp_err;
	}

	num_scan_bblks = XLOG_TOTAL_REC_SHIFT(log);
	if (head_blk >= num_scan_bblks) {
		start_blk = head_blk - num_scan_bblks;
		if ((error = xlog_find_verify_cycle(log,
						start_blk, num_scan_bblks,
						stop_on_cycle, &new_blk)))
			goto bp_err;
		if (new_blk != -1)
			head_blk = new_blk;
	} else {		
		/*
		 * We are going to scan backwards in the log in two parts.
		 * First we scan the physical end of the log.  In this part
		 * of the log, we are looking for blocks with cycle number
		 * last_half_cycle - 1.
		 * If we find one, then we know that the log starts there, as
		 * we've found a hole that didn't get written in going around
		 * the end of the physical log.  The simple case for this is
		 *        x + 1 ... | x ... | x - 1 | x
		 *        <---------> less than scan distance
		 * If all of the blocks at the end of the log have cycle number
		 * last_half_cycle, then we check the blocks at the start of
		 * the log looking for occurrences of last_half_cycle.  If we
		 * find one, then our current estimate for the location of the
		 * first occurrence of last_half_cycle is wrong and we move
		 * back to the hole we've found.  This case looks like
		 *        x + 1 ... | x | x + 1 | x ...
		 *                               ^ binary search stopped here
		 * Another case we need to handle that only occurs in 256k
		 * logs is
		 *        x + 1 ... | x ... | x+1 | x ...
		 *                   ^ binary search stops here
		 * In a 256k log, the scan at the end of the log will see the
		 * x + 1 blocks.  We need to skip past those since that is
		 * certainly not the head of the log.  By searching for
		 * last_half_cycle-1 we accomplish that.
		 */
		ASSERT(head_blk <= INT_MAX &&
			(xfs_daddr_t) num_scan_bblks >= head_blk);
		start_blk = log_bbnum - (num_scan_bblks - head_blk);
		if ((error = xlog_find_verify_cycle(log, start_blk,
					num_scan_bblks - (int)head_blk,
					(stop_on_cycle - 1), &new_blk)))
			goto bp_err;
		if (new_blk != -1) {
			head_blk = new_blk;
			goto validate_head;
		}

		start_blk = 0;
		ASSERT(head_blk <= INT_MAX);
		if ((error = xlog_find_verify_cycle(log,
					start_blk, (int)head_blk,
					stop_on_cycle, &new_blk)))
			goto bp_err;
		if (new_blk != -1)
			head_blk = new_blk;
	}

validate_head:
	num_scan_bblks = XLOG_REC_SHIFT(log);
	if (head_blk >= num_scan_bblks) {
		start_blk = head_blk - num_scan_bblks; 

		
		if ((error = xlog_find_verify_log_record(log, start_blk,
							&head_blk, 0)) == -1) {
			error = XFS_ERROR(EIO);
			goto bp_err;
		} else if (error)
			goto bp_err;
	} else {
		start_blk = 0;
		ASSERT(head_blk <= INT_MAX);
		if ((error = xlog_find_verify_log_record(log, start_blk,
							&head_blk, 0)) == -1) {
			
			start_blk = log_bbnum - (num_scan_bblks - head_blk);
			new_blk = log_bbnum;
			ASSERT(start_blk <= INT_MAX &&
				(xfs_daddr_t) log_bbnum-start_blk >= 0);
			ASSERT(head_blk <= INT_MAX);
			if ((error = xlog_find_verify_log_record(log,
							start_blk, &new_blk,
							(int)head_blk)) == -1) {
				error = XFS_ERROR(EIO);
				goto bp_err;
			} else if (error)
				goto bp_err;
			if (new_blk != log_bbnum)
				head_blk = new_blk;
		} else if (error)
			goto bp_err;
	}

	xlog_put_bp(bp);
	if (head_blk == log_bbnum)
		*return_head_blk = 0;
	else
		*return_head_blk = head_blk;
	return 0;

 bp_err:
	xlog_put_bp(bp);

	if (error)
		xfs_warn(log->l_mp, "failed to find log head");
	return error;
}

STATIC int
xlog_find_tail(
	xlog_t			*log,
	xfs_daddr_t		*head_blk,
	xfs_daddr_t		*tail_blk)
{
	xlog_rec_header_t	*rhead;
	xlog_op_header_t	*op_head;
	xfs_caddr_t		offset = NULL;
	xfs_buf_t		*bp;
	int			error, i, found;
	xfs_daddr_t		umount_data_blk;
	xfs_daddr_t		after_umount_blk;
	xfs_lsn_t		tail_lsn;
	int			hblks;

	found = 0;

	if ((error = xlog_find_head(log, head_blk)))
		return error;

	bp = xlog_get_bp(log, 1);
	if (!bp)
		return ENOMEM;
	if (*head_blk == 0) {				
		error = xlog_bread(log, 0, 1, bp, &offset);
		if (error)
			goto done;

		if (xlog_get_cycle(offset) == 0) {
			*tail_blk = 0;
			
			goto done;
		}
	}

	ASSERT(*head_blk < INT_MAX);
	for (i = (int)(*head_blk) - 1; i >= 0; i--) {
		error = xlog_bread(log, i, 1, bp, &offset);
		if (error)
			goto done;

		if (*(__be32 *)offset == cpu_to_be32(XLOG_HEADER_MAGIC_NUM)) {
			found = 1;
			break;
		}
	}
	if (!found) {
		for (i = log->l_logBBsize - 1; i >= (int)(*head_blk); i--) {
			error = xlog_bread(log, i, 1, bp, &offset);
			if (error)
				goto done;

			if (*(__be32 *)offset ==
			    cpu_to_be32(XLOG_HEADER_MAGIC_NUM)) {
				found = 2;
				break;
			}
		}
	}
	if (!found) {
		xfs_warn(log->l_mp, "%s: couldn't find sync record", __func__);
		ASSERT(0);
		return XFS_ERROR(EIO);
	}

	
	rhead = (xlog_rec_header_t *)offset;
	*tail_blk = BLOCK_LSN(be64_to_cpu(rhead->h_tail_lsn));

	/*
	 * Reset log values according to the state of the log when we
	 * crashed.  In the case where head_blk == 0, we bump curr_cycle
	 * one because the next write starts a new cycle rather than
	 * continuing the cycle of the last good log record.  At this
	 * point we have guaranteed that all partial log records have been
	 * accounted for.  Therefore, we know that the last good log record
	 * written was complete and ended exactly on the end boundary
	 * of the physical log.
	 */
	log->l_prev_block = i;
	log->l_curr_block = (int)*head_blk;
	log->l_curr_cycle = be32_to_cpu(rhead->h_cycle);
	if (found == 2)
		log->l_curr_cycle++;
	atomic64_set(&log->l_tail_lsn, be64_to_cpu(rhead->h_tail_lsn));
	atomic64_set(&log->l_last_sync_lsn, be64_to_cpu(rhead->h_lsn));
	xlog_assign_grant_head(&log->l_reserve_head.grant, log->l_curr_cycle,
					BBTOB(log->l_curr_block));
	xlog_assign_grant_head(&log->l_write_head.grant, log->l_curr_cycle,
					BBTOB(log->l_curr_block));

	if (xfs_sb_version_haslogv2(&log->l_mp->m_sb)) {
		int	h_size = be32_to_cpu(rhead->h_size);
		int	h_version = be32_to_cpu(rhead->h_version);

		if ((h_version & XLOG_VERSION_2) &&
		    (h_size > XLOG_HEADER_CYCLE_SIZE)) {
			hblks = h_size / XLOG_HEADER_CYCLE_SIZE;
			if (h_size % XLOG_HEADER_CYCLE_SIZE)
				hblks++;
		} else {
			hblks = 1;
		}
	} else {
		hblks = 1;
	}
	after_umount_blk = (i + hblks + (int)
		BTOBB(be32_to_cpu(rhead->h_len))) % log->l_logBBsize;
	tail_lsn = atomic64_read(&log->l_tail_lsn);
	if (*head_blk == after_umount_blk &&
	    be32_to_cpu(rhead->h_num_logops) == 1) {
		umount_data_blk = (i + hblks) % log->l_logBBsize;
		error = xlog_bread(log, umount_data_blk, 1, bp, &offset);
		if (error)
			goto done;

		op_head = (xlog_op_header_t *)offset;
		if (op_head->oh_flags & XLOG_UNMOUNT_TRANS) {
			/*
			 * Set tail and last sync so that newly written
			 * log records will point recovery to after the
			 * current unmount record.
			 */
			xlog_assign_atomic_lsn(&log->l_tail_lsn,
					log->l_curr_cycle, after_umount_blk);
			xlog_assign_atomic_lsn(&log->l_last_sync_lsn,
					log->l_curr_cycle, after_umount_blk);
			*tail_blk = after_umount_blk;

			log->l_mp->m_flags |= XFS_MOUNT_WAS_CLEAN;
		}
	}

	if (!xfs_readonly_buftarg(log->l_mp->m_logdev_targp))
		error = xlog_clear_stale_blocks(log, tail_lsn);

done:
	xlog_put_bp(bp);

	if (error)
		xfs_warn(log->l_mp, "failed to locate log tail");
	return error;
}

/*
 * Is the log zeroed at all?
 *
 * The last binary search should be changed to perform an X block read
 * once X becomes small enough.  You can then search linearly through
 * the X blocks.  This will cut down on the number of reads we need to do.
 *
 * If the log is partially zeroed, this routine will pass back the blkno
 * of the first block with cycle number 0.  It won't have a complete LR
 * preceding it.
 *
 * Return:
 *	0  => the log is completely written to
 *	-1 => use *blk_no as the first block of the log
 *	>0 => error has occurred
 */
STATIC int
xlog_find_zeroed(
	xlog_t		*log,
	xfs_daddr_t	*blk_no)
{
	xfs_buf_t	*bp;
	xfs_caddr_t	offset;
	uint	        first_cycle, last_cycle;
	xfs_daddr_t	new_blk, last_blk, start_blk;
	xfs_daddr_t     num_scan_bblks;
	int	        error, log_bbnum = log->l_logBBsize;

	*blk_no = 0;

	
	bp = xlog_get_bp(log, 1);
	if (!bp)
		return ENOMEM;
	error = xlog_bread(log, 0, 1, bp, &offset);
	if (error)
		goto bp_err;

	first_cycle = xlog_get_cycle(offset);
	if (first_cycle == 0) {		
		*blk_no = 0;
		xlog_put_bp(bp);
		return -1;
	}

	
	error = xlog_bread(log, log_bbnum-1, 1, bp, &offset);
	if (error)
		goto bp_err;

	last_cycle = xlog_get_cycle(offset);
	if (last_cycle != 0) {		/* log completely written to */
		xlog_put_bp(bp);
		return 0;
	} else if (first_cycle != 1) {
		xfs_warn(log->l_mp,
			"Log inconsistent or not a log (last==0, first!=1)");
		return XFS_ERROR(EINVAL);
	}

	
	last_blk = log_bbnum-1;
	if ((error = xlog_find_cycle_start(log, bp, 0, &last_blk, 0)))
		goto bp_err;

	num_scan_bblks = XLOG_TOTAL_REC_SHIFT(log);
	ASSERT(num_scan_bblks <= INT_MAX);

	if (last_blk < num_scan_bblks)
		num_scan_bblks = last_blk;
	start_blk = last_blk - num_scan_bblks;

	if ((error = xlog_find_verify_cycle(log, start_blk,
					 (int)num_scan_bblks, 0, &new_blk)))
		goto bp_err;
	if (new_blk != -1)
		last_blk = new_blk;

	if ((error = xlog_find_verify_log_record(log, start_blk,
				&last_blk, 0)) == -1) {
	    error = XFS_ERROR(EIO);
	    goto bp_err;
	} else if (error)
	    goto bp_err;

	*blk_no = last_blk;
bp_err:
	xlog_put_bp(bp);
	if (error)
		return error;
	return -1;
}

STATIC void
xlog_add_record(
	xlog_t			*log,
	xfs_caddr_t		buf,
	int			cycle,
	int			block,
	int			tail_cycle,
	int			tail_block)
{
	xlog_rec_header_t	*recp = (xlog_rec_header_t *)buf;

	memset(buf, 0, BBSIZE);
	recp->h_magicno = cpu_to_be32(XLOG_HEADER_MAGIC_NUM);
	recp->h_cycle = cpu_to_be32(cycle);
	recp->h_version = cpu_to_be32(
			xfs_sb_version_haslogv2(&log->l_mp->m_sb) ? 2 : 1);
	recp->h_lsn = cpu_to_be64(xlog_assign_lsn(cycle, block));
	recp->h_tail_lsn = cpu_to_be64(xlog_assign_lsn(tail_cycle, tail_block));
	recp->h_fmt = cpu_to_be32(XLOG_FMT);
	memcpy(&recp->h_fs_uuid, &log->l_mp->m_sb.sb_uuid, sizeof(uuid_t));
}

STATIC int
xlog_write_log_records(
	xlog_t		*log,
	int		cycle,
	int		start_block,
	int		blocks,
	int		tail_cycle,
	int		tail_block)
{
	xfs_caddr_t	offset;
	xfs_buf_t	*bp;
	int		balign, ealign;
	int		sectbb = log->l_sectBBsize;
	int		end_block = start_block + blocks;
	int		bufblks;
	int		error = 0;
	int		i, j = 0;

	/*
	 * Greedily allocate a buffer big enough to handle the full
	 * range of basic blocks to be written.  If that fails, try
	 * a smaller size.  We need to be able to write at least a
	 * log sector, or we're out of luck.
	 */
	bufblks = 1 << ffs(blocks);
	while (!(bp = xlog_get_bp(log, bufblks))) {
		bufblks >>= 1;
		if (bufblks < sectbb)
			return ENOMEM;
	}

	balign = round_down(start_block, sectbb);
	if (balign != start_block) {
		error = xlog_bread_noalign(log, start_block, 1, bp);
		if (error)
			goto out_put_bp;

		j = start_block - balign;
	}

	for (i = start_block; i < end_block; i += bufblks) {
		int		bcount, endcount;

		bcount = min(bufblks, end_block - start_block);
		endcount = bcount - j;

		ealign = round_down(end_block, sectbb);
		if (j == 0 && (start_block + endcount > ealign)) {
			offset = bp->b_addr + BBTOB(ealign - start_block);
			error = xlog_bread_offset(log, ealign, sectbb,
							bp, offset);
			if (error)
				break;

		}

		offset = xlog_align(log, start_block, endcount, bp);
		for (; j < endcount; j++) {
			xlog_add_record(log, offset, cycle, i+j,
					tail_cycle, tail_block);
			offset += BBSIZE;
		}
		error = xlog_bwrite(log, start_block, endcount, bp);
		if (error)
			break;
		start_block += endcount;
		j = 0;
	}

 out_put_bp:
	xlog_put_bp(bp);
	return error;
}

/*
 * This routine is called to blow away any incomplete log writes out
 * in front of the log head.  We do this so that we won't become confused
 * if we come up, write only a little bit more, and then crash again.
 * If we leave the partial log records out there, this situation could
 * cause us to think those partial writes are valid blocks since they
 * have the current cycle number.  We get rid of them by overwriting them
 * with empty log records with the old cycle number rather than the
 * current one.
 *
 * The tail lsn is passed in rather than taken from
 * the log so that we will not write over the unmount record after a
 * clean unmount in a 512 block log.  Doing so would leave the log without
 * any valid log records in it until a new one was written.  If we crashed
 * during that time we would not be able to recover.
 */
STATIC int
xlog_clear_stale_blocks(
	xlog_t		*log,
	xfs_lsn_t	tail_lsn)
{
	int		tail_cycle, head_cycle;
	int		tail_block, head_block;
	int		tail_distance, max_distance;
	int		distance;
	int		error;

	tail_cycle = CYCLE_LSN(tail_lsn);
	tail_block = BLOCK_LSN(tail_lsn);
	head_cycle = log->l_curr_cycle;
	head_block = log->l_curr_block;

	/*
	 * Figure out the distance between the new head of the log
	 * and the tail.  We want to write over any blocks beyond the
	 * head that we may have written just before the crash, but
	 * we don't want to overwrite the tail of the log.
	 */
	if (head_cycle == tail_cycle) {
		if (unlikely(head_block < tail_block || head_block >= log->l_logBBsize)) {
			XFS_ERROR_REPORT("xlog_clear_stale_blocks(1)",
					 XFS_ERRLEVEL_LOW, log->l_mp);
			return XFS_ERROR(EFSCORRUPTED);
		}
		tail_distance = tail_block + (log->l_logBBsize - head_block);
	} else {
		if (unlikely(head_block >= tail_block || head_cycle != (tail_cycle + 1))){
			XFS_ERROR_REPORT("xlog_clear_stale_blocks(2)",
					 XFS_ERRLEVEL_LOW, log->l_mp);
			return XFS_ERROR(EFSCORRUPTED);
		}
		tail_distance = tail_block - head_block;
	}

	if (tail_distance <= 0) {
		ASSERT(tail_distance == 0);
		return 0;
	}

	max_distance = XLOG_TOTAL_REC_SHIFT(log);
	max_distance = MIN(max_distance, tail_distance);

	if ((head_block + max_distance) <= log->l_logBBsize) {
		error = xlog_write_log_records(log, (head_cycle - 1),
				head_block, max_distance, tail_cycle,
				tail_block);
		if (error)
			return error;
	} else {
		distance = log->l_logBBsize - head_block;
		error = xlog_write_log_records(log, (head_cycle - 1),
				head_block, distance, tail_cycle,
				tail_block);

		if (error)
			return error;

		distance = max_distance - (log->l_logBBsize - head_block);
		error = xlog_write_log_records(log, head_cycle, 0, distance,
				tail_cycle, tail_block);
		if (error)
			return error;
	}

	return 0;
}


STATIC xlog_recover_t *
xlog_recover_find_tid(
	struct hlist_head	*head,
	xlog_tid_t		tid)
{
	xlog_recover_t		*trans;
	struct hlist_node	*n;

	hlist_for_each_entry(trans, n, head, r_list) {
		if (trans->r_log_tid == tid)
			return trans;
	}
	return NULL;
}

STATIC void
xlog_recover_new_tid(
	struct hlist_head	*head,
	xlog_tid_t		tid,
	xfs_lsn_t		lsn)
{
	xlog_recover_t		*trans;

	trans = kmem_zalloc(sizeof(xlog_recover_t), KM_SLEEP);
	trans->r_log_tid   = tid;
	trans->r_lsn	   = lsn;
	INIT_LIST_HEAD(&trans->r_itemq);

	INIT_HLIST_NODE(&trans->r_list);
	hlist_add_head(&trans->r_list, head);
}

STATIC void
xlog_recover_add_item(
	struct list_head	*head)
{
	xlog_recover_item_t	*item;

	item = kmem_zalloc(sizeof(xlog_recover_item_t), KM_SLEEP);
	INIT_LIST_HEAD(&item->ri_list);
	list_add_tail(&item->ri_list, head);
}

STATIC int
xlog_recover_add_to_cont_trans(
	struct log		*log,
	xlog_recover_t		*trans,
	xfs_caddr_t		dp,
	int			len)
{
	xlog_recover_item_t	*item;
	xfs_caddr_t		ptr, old_ptr;
	int			old_len;

	if (list_empty(&trans->r_itemq)) {
		
		xlog_recover_add_item(&trans->r_itemq);
		ptr = (xfs_caddr_t) &trans->r_theader +
				sizeof(xfs_trans_header_t) - len;
		memcpy(ptr, dp, len); 
		return 0;
	}
	
	item = list_entry(trans->r_itemq.prev, xlog_recover_item_t, ri_list);

	old_ptr = item->ri_buf[item->ri_cnt-1].i_addr;
	old_len = item->ri_buf[item->ri_cnt-1].i_len;

	ptr = kmem_realloc(old_ptr, len+old_len, old_len, KM_SLEEP);
	memcpy(&ptr[old_len], dp, len); 
	item->ri_buf[item->ri_cnt-1].i_len += len;
	item->ri_buf[item->ri_cnt-1].i_addr = ptr;
	trace_xfs_log_recover_item_add_cont(log, trans, item, 0);
	return 0;
}

STATIC int
xlog_recover_add_to_trans(
	struct log		*log,
	xlog_recover_t		*trans,
	xfs_caddr_t		dp,
	int			len)
{
	xfs_inode_log_format_t	*in_f;			
	xlog_recover_item_t	*item;
	xfs_caddr_t		ptr;

	if (!len)
		return 0;
	if (list_empty(&trans->r_itemq)) {
		
		if (*(uint *)dp != XFS_TRANS_HEADER_MAGIC) {
			xfs_warn(log->l_mp, "%s: bad header magic number",
				__func__);
			ASSERT(0);
			return XFS_ERROR(EIO);
		}
		if (len == sizeof(xfs_trans_header_t))
			xlog_recover_add_item(&trans->r_itemq);
		memcpy(&trans->r_theader, dp, len); 
		return 0;
	}

	ptr = kmem_alloc(len, KM_SLEEP);
	memcpy(ptr, dp, len);
	in_f = (xfs_inode_log_format_t *)ptr;

	
	item = list_entry(trans->r_itemq.prev, xlog_recover_item_t, ri_list);
	if (item->ri_total != 0 &&
	     item->ri_total == item->ri_cnt) {
		
		xlog_recover_add_item(&trans->r_itemq);
		item = list_entry(trans->r_itemq.prev,
					xlog_recover_item_t, ri_list);
	}

	if (item->ri_total == 0) {		
		if (in_f->ilf_size == 0 ||
		    in_f->ilf_size > XLOG_MAX_REGIONS_IN_ITEM) {
			xfs_warn(log->l_mp,
		"bad number of regions (%d) in inode log format",
				  in_f->ilf_size);
			ASSERT(0);
			return XFS_ERROR(EIO);
		}

		item->ri_total = in_f->ilf_size;
		item->ri_buf =
			kmem_zalloc(item->ri_total * sizeof(xfs_log_iovec_t),
				    KM_SLEEP);
	}
	ASSERT(item->ri_total > item->ri_cnt);
	
	item->ri_buf[item->ri_cnt].i_addr = ptr;
	item->ri_buf[item->ri_cnt].i_len  = len;
	item->ri_cnt++;
	trace_xfs_log_recover_item_add(log, trans, item, 0);
	return 0;
}

STATIC int
xlog_recover_reorder_trans(
	struct log		*log,
	xlog_recover_t		*trans,
	int			pass)
{
	xlog_recover_item_t	*item, *n;
	LIST_HEAD(sort_list);

	list_splice_init(&trans->r_itemq, &sort_list);
	list_for_each_entry_safe(item, n, &sort_list, ri_list) {
		xfs_buf_log_format_t	*buf_f = item->ri_buf[0].i_addr;

		switch (ITEM_TYPE(item)) {
		case XFS_LI_BUF:
			if (!(buf_f->blf_flags & XFS_BLF_CANCEL)) {
				trace_xfs_log_recover_item_reorder_head(log,
							trans, item, pass);
				list_move(&item->ri_list, &trans->r_itemq);
				break;
			}
		case XFS_LI_INODE:
		case XFS_LI_DQUOT:
		case XFS_LI_QUOTAOFF:
		case XFS_LI_EFD:
		case XFS_LI_EFI:
			trace_xfs_log_recover_item_reorder_tail(log,
							trans, item, pass);
			list_move_tail(&item->ri_list, &trans->r_itemq);
			break;
		default:
			xfs_warn(log->l_mp,
				"%s: unrecognized type of log operation",
				__func__);
			ASSERT(0);
			return XFS_ERROR(EIO);
		}
	}
	ASSERT(list_empty(&sort_list));
	return 0;
}

STATIC int
xlog_recover_buffer_pass1(
	struct log		*log,
	xlog_recover_item_t	*item)
{
	xfs_buf_log_format_t	*buf_f = item->ri_buf[0].i_addr;
	struct list_head	*bucket;
	struct xfs_buf_cancel	*bcp;

	if (!(buf_f->blf_flags & XFS_BLF_CANCEL)) {
		trace_xfs_log_recover_buf_not_cancel(log, buf_f);
		return 0;
	}

	bucket = XLOG_BUF_CANCEL_BUCKET(log, buf_f->blf_blkno);
	list_for_each_entry(bcp, bucket, bc_list) {
		if (bcp->bc_blkno == buf_f->blf_blkno &&
		    bcp->bc_len == buf_f->blf_len) {
			bcp->bc_refcount++;
			trace_xfs_log_recover_buf_cancel_ref_inc(log, buf_f);
			return 0;
		}
	}

	bcp = kmem_alloc(sizeof(struct xfs_buf_cancel), KM_SLEEP);
	bcp->bc_blkno = buf_f->blf_blkno;
	bcp->bc_len = buf_f->blf_len;
	bcp->bc_refcount = 1;
	list_add_tail(&bcp->bc_list, bucket);

	trace_xfs_log_recover_buf_cancel_add(log, buf_f);
	return 0;
}

STATIC int
xlog_check_buffer_cancelled(
	struct log		*log,
	xfs_daddr_t		blkno,
	uint			len,
	ushort			flags)
{
	struct list_head	*bucket;
	struct xfs_buf_cancel	*bcp;

	if (log->l_buf_cancel_table == NULL) {
		ASSERT(!(flags & XFS_BLF_CANCEL));
		return 0;
	}

	bucket = XLOG_BUF_CANCEL_BUCKET(log, blkno);
	list_for_each_entry(bcp, bucket, bc_list) {
		if (bcp->bc_blkno == blkno && bcp->bc_len == len)
			goto found;
	}

	ASSERT(!(flags & XFS_BLF_CANCEL));
	return 0;

found:
	if (flags & XFS_BLF_CANCEL) {
		if (--bcp->bc_refcount == 0) {
			list_del(&bcp->bc_list);
			kmem_free(bcp);
		}
	}
	return 1;
}

STATIC int
xlog_recover_do_inode_buffer(
	struct xfs_mount	*mp,
	xlog_recover_item_t	*item,
	struct xfs_buf		*bp,
	xfs_buf_log_format_t	*buf_f)
{
	int			i;
	int			item_index = 0;
	int			bit = 0;
	int			nbits = 0;
	int			reg_buf_offset = 0;
	int			reg_buf_bytes = 0;
	int			next_unlinked_offset;
	int			inodes_per_buf;
	xfs_agino_t		*logged_nextp;
	xfs_agino_t		*buffer_nextp;

	trace_xfs_log_recover_buf_inode_buf(mp->m_log, buf_f);

	inodes_per_buf = XFS_BUF_COUNT(bp) >> mp->m_sb.sb_inodelog;
	for (i = 0; i < inodes_per_buf; i++) {
		next_unlinked_offset = (i * mp->m_sb.sb_inodesize) +
			offsetof(xfs_dinode_t, di_next_unlinked);

		while (next_unlinked_offset >=
		       (reg_buf_offset + reg_buf_bytes)) {
			bit += nbits;
			bit = xfs_next_bit(buf_f->blf_data_map,
					   buf_f->blf_map_size, bit);

			if (bit == -1)
				return 0;

			nbits = xfs_contig_bits(buf_f->blf_data_map,
						buf_f->blf_map_size, bit);
			ASSERT(nbits > 0);
			reg_buf_offset = bit << XFS_BLF_SHIFT;
			reg_buf_bytes = nbits << XFS_BLF_SHIFT;
			item_index++;
		}

		if (next_unlinked_offset < reg_buf_offset)
			continue;

		ASSERT(item->ri_buf[item_index].i_addr != NULL);
		ASSERT((item->ri_buf[item_index].i_len % XFS_BLF_CHUNK) == 0);
		ASSERT((reg_buf_offset + reg_buf_bytes) <= XFS_BUF_COUNT(bp));

		logged_nextp = item->ri_buf[item_index].i_addr +
				next_unlinked_offset - reg_buf_offset;
		if (unlikely(*logged_nextp == 0)) {
			xfs_alert(mp,
		"Bad inode buffer log record (ptr = 0x%p, bp = 0x%p). "
		"Trying to replay bad (0) inode di_next_unlinked field.",
				item, bp);
			XFS_ERROR_REPORT("xlog_recover_do_inode_buf",
					 XFS_ERRLEVEL_LOW, mp);
			return XFS_ERROR(EFSCORRUPTED);
		}

		buffer_nextp = (xfs_agino_t *)xfs_buf_offset(bp,
					      next_unlinked_offset);
		*buffer_nextp = *logged_nextp;
	}

	return 0;
}

STATIC void
xlog_recover_do_reg_buffer(
	struct xfs_mount	*mp,
	xlog_recover_item_t	*item,
	struct xfs_buf		*bp,
	xfs_buf_log_format_t	*buf_f)
{
	int			i;
	int			bit;
	int			nbits;
	int                     error;

	trace_xfs_log_recover_buf_reg_buf(mp->m_log, buf_f);

	bit = 0;
	i = 1;  
	while (1) {
		bit = xfs_next_bit(buf_f->blf_data_map,
				   buf_f->blf_map_size, bit);
		if (bit == -1)
			break;
		nbits = xfs_contig_bits(buf_f->blf_data_map,
					buf_f->blf_map_size, bit);
		ASSERT(nbits > 0);
		ASSERT(item->ri_buf[i].i_addr != NULL);
		ASSERT(item->ri_buf[i].i_len % XFS_BLF_CHUNK == 0);
		ASSERT(XFS_BUF_COUNT(bp) >=
		       ((uint)bit << XFS_BLF_SHIFT)+(nbits<<XFS_BLF_SHIFT));

		error = 0;
		if (buf_f->blf_flags &
		   (XFS_BLF_UDQUOT_BUF|XFS_BLF_PDQUOT_BUF|XFS_BLF_GDQUOT_BUF)) {
			if (item->ri_buf[i].i_addr == NULL) {
				xfs_alert(mp,
					"XFS: NULL dquot in %s.", __func__);
				goto next;
			}
			if (item->ri_buf[i].i_len < sizeof(xfs_disk_dquot_t)) {
				xfs_alert(mp,
					"XFS: dquot too small (%d) in %s.",
					item->ri_buf[i].i_len, __func__);
				goto next;
			}
			error = xfs_qm_dqcheck(mp, item->ri_buf[i].i_addr,
					       -1, 0, XFS_QMOPT_DOWARN,
					       "dquot_buf_recover");
			if (error)
				goto next;
		}

		memcpy(xfs_buf_offset(bp,
			(uint)bit << XFS_BLF_SHIFT),	
			item->ri_buf[i].i_addr,		
			nbits<<XFS_BLF_SHIFT);		
 next:
		i++;
		bit += nbits;
	}

	
	ASSERT(i == item->ri_total);
}

int
xfs_qm_dqcheck(
	struct xfs_mount *mp,
	xfs_disk_dquot_t *ddq,
	xfs_dqid_t	 id,
	uint		 type,	  
	uint		 flags,
	char		 *str)
{
	xfs_dqblk_t	 *d = (xfs_dqblk_t *)ddq;
	int		errs = 0;

	if (ddq->d_magic != cpu_to_be16(XFS_DQUOT_MAGIC)) {
		if (flags & XFS_QMOPT_DOWARN)
			xfs_alert(mp,
			"%s : XFS dquot ID 0x%x, magic 0x%x != 0x%x",
			str, id, be16_to_cpu(ddq->d_magic), XFS_DQUOT_MAGIC);
		errs++;
	}
	if (ddq->d_version != XFS_DQUOT_VERSION) {
		if (flags & XFS_QMOPT_DOWARN)
			xfs_alert(mp,
			"%s : XFS dquot ID 0x%x, version 0x%x != 0x%x",
			str, id, ddq->d_version, XFS_DQUOT_VERSION);
		errs++;
	}

	if (ddq->d_flags != XFS_DQ_USER &&
	    ddq->d_flags != XFS_DQ_PROJ &&
	    ddq->d_flags != XFS_DQ_GROUP) {
		if (flags & XFS_QMOPT_DOWARN)
			xfs_alert(mp,
			"%s : XFS dquot ID 0x%x, unknown flags 0x%x",
			str, id, ddq->d_flags);
		errs++;
	}

	if (id != -1 && id != be32_to_cpu(ddq->d_id)) {
		if (flags & XFS_QMOPT_DOWARN)
			xfs_alert(mp,
			"%s : ondisk-dquot 0x%p, ID mismatch: "
			"0x%x expected, found id 0x%x",
			str, ddq, id, be32_to_cpu(ddq->d_id));
		errs++;
	}

	if (!errs && ddq->d_id) {
		if (ddq->d_blk_softlimit &&
		    be64_to_cpu(ddq->d_bcount) >
				be64_to_cpu(ddq->d_blk_softlimit)) {
			if (!ddq->d_btimer) {
				if (flags & XFS_QMOPT_DOWARN)
					xfs_alert(mp,
			"%s : Dquot ID 0x%x (0x%p) BLK TIMER NOT STARTED",
					str, (int)be32_to_cpu(ddq->d_id), ddq);
				errs++;
			}
		}
		if (ddq->d_ino_softlimit &&
		    be64_to_cpu(ddq->d_icount) >
				be64_to_cpu(ddq->d_ino_softlimit)) {
			if (!ddq->d_itimer) {
				if (flags & XFS_QMOPT_DOWARN)
					xfs_alert(mp,
			"%s : Dquot ID 0x%x (0x%p) INODE TIMER NOT STARTED",
					str, (int)be32_to_cpu(ddq->d_id), ddq);
				errs++;
			}
		}
		if (ddq->d_rtb_softlimit &&
		    be64_to_cpu(ddq->d_rtbcount) >
				be64_to_cpu(ddq->d_rtb_softlimit)) {
			if (!ddq->d_rtbtimer) {
				if (flags & XFS_QMOPT_DOWARN)
					xfs_alert(mp,
			"%s : Dquot ID 0x%x (0x%p) RTBLK TIMER NOT STARTED",
					str, (int)be32_to_cpu(ddq->d_id), ddq);
				errs++;
			}
		}
	}

	if (!errs || !(flags & XFS_QMOPT_DQREPAIR))
		return errs;

	if (flags & XFS_QMOPT_DOWARN)
		xfs_notice(mp, "Re-initializing dquot ID 0x%x", id);

	ASSERT(id != -1);
	ASSERT(flags & XFS_QMOPT_DQREPAIR);
	memset(d, 0, sizeof(xfs_dqblk_t));

	d->dd_diskdq.d_magic = cpu_to_be16(XFS_DQUOT_MAGIC);
	d->dd_diskdq.d_version = XFS_DQUOT_VERSION;
	d->dd_diskdq.d_flags = type;
	d->dd_diskdq.d_id = cpu_to_be32(id);

	return errs;
}

STATIC void
xlog_recover_do_dquot_buffer(
	xfs_mount_t		*mp,
	xlog_t			*log,
	xlog_recover_item_t	*item,
	xfs_buf_t		*bp,
	xfs_buf_log_format_t	*buf_f)
{
	uint			type;

	trace_xfs_log_recover_buf_dquot_buf(log, buf_f);

	if (mp->m_qflags == 0) {
		return;
	}

	type = 0;
	if (buf_f->blf_flags & XFS_BLF_UDQUOT_BUF)
		type |= XFS_DQ_USER;
	if (buf_f->blf_flags & XFS_BLF_PDQUOT_BUF)
		type |= XFS_DQ_PROJ;
	if (buf_f->blf_flags & XFS_BLF_GDQUOT_BUF)
		type |= XFS_DQ_GROUP;
	if (log->l_quotaoffs_flag & type)
		return;

	xlog_recover_do_reg_buffer(mp, item, bp, buf_f);
}

STATIC int
xlog_recover_buffer_pass2(
	xlog_t			*log,
	xlog_recover_item_t	*item)
{
	xfs_buf_log_format_t	*buf_f = item->ri_buf[0].i_addr;
	xfs_mount_t		*mp = log->l_mp;
	xfs_buf_t		*bp;
	int			error;
	uint			buf_flags;

	if (xlog_check_buffer_cancelled(log, buf_f->blf_blkno,
			buf_f->blf_len, buf_f->blf_flags)) {
		trace_xfs_log_recover_buf_cancel(log, buf_f);
		return 0;
	}

	trace_xfs_log_recover_buf_recover(log, buf_f);

	buf_flags = XBF_LOCK;
	if (!(buf_f->blf_flags & XFS_BLF_INODE_BUF))
		buf_flags |= XBF_MAPPED;

	bp = xfs_buf_read(mp->m_ddev_targp, buf_f->blf_blkno, buf_f->blf_len,
			  buf_flags);
	if (!bp)
		return XFS_ERROR(ENOMEM);
	error = bp->b_error;
	if (error) {
		xfs_buf_ioerror_alert(bp, "xlog_recover_do..(read#1)");
		xfs_buf_relse(bp);
		return error;
	}

	if (buf_f->blf_flags & XFS_BLF_INODE_BUF) {
		error = xlog_recover_do_inode_buffer(mp, item, bp, buf_f);
	} else if (buf_f->blf_flags &
		  (XFS_BLF_UDQUOT_BUF|XFS_BLF_PDQUOT_BUF|XFS_BLF_GDQUOT_BUF)) {
		xlog_recover_do_dquot_buffer(mp, log, item, bp, buf_f);
	} else {
		xlog_recover_do_reg_buffer(mp, item, bp, buf_f);
	}
	if (error)
		return XFS_ERROR(error);

	if (XFS_DINODE_MAGIC ==
	    be16_to_cpu(*((__be16 *)xfs_buf_offset(bp, 0))) &&
	    (XFS_BUF_COUNT(bp) != MAX(log->l_mp->m_sb.sb_blocksize,
			(__uint32_t)XFS_INODE_CLUSTER_SIZE(log->l_mp)))) {
		xfs_buf_stale(bp);
		error = xfs_bwrite(bp);
	} else {
		ASSERT(bp->b_target->bt_mount == mp);
		bp->b_iodone = xlog_recover_iodone;
		xfs_buf_delwri_queue(bp);
	}

	xfs_buf_relse(bp);
	return error;
}

STATIC int
xlog_recover_inode_pass2(
	xlog_t			*log,
	xlog_recover_item_t	*item)
{
	xfs_inode_log_format_t	*in_f;
	xfs_mount_t		*mp = log->l_mp;
	xfs_buf_t		*bp;
	xfs_dinode_t		*dip;
	int			len;
	xfs_caddr_t		src;
	xfs_caddr_t		dest;
	int			error;
	int			attr_index;
	uint			fields;
	xfs_icdinode_t		*dicp;
	int			need_free = 0;

	if (item->ri_buf[0].i_len == sizeof(xfs_inode_log_format_t)) {
		in_f = item->ri_buf[0].i_addr;
	} else {
		in_f = kmem_alloc(sizeof(xfs_inode_log_format_t), KM_SLEEP);
		need_free = 1;
		error = xfs_inode_item_format_convert(&item->ri_buf[0], in_f);
		if (error)
			goto error;
	}

	if (xlog_check_buffer_cancelled(log, in_f->ilf_blkno,
					in_f->ilf_len, 0)) {
		error = 0;
		trace_xfs_log_recover_inode_cancel(log, in_f);
		goto error;
	}
	trace_xfs_log_recover_inode_recover(log, in_f);

	bp = xfs_buf_read(mp->m_ddev_targp, in_f->ilf_blkno, in_f->ilf_len,
			  XBF_LOCK);
	if (!bp) {
		error = ENOMEM;
		goto error;
	}
	error = bp->b_error;
	if (error) {
		xfs_buf_ioerror_alert(bp, "xlog_recover_do..(read#2)");
		xfs_buf_relse(bp);
		goto error;
	}
	ASSERT(in_f->ilf_fields & XFS_ILOG_CORE);
	dip = (xfs_dinode_t *)xfs_buf_offset(bp, in_f->ilf_boffset);

	if (unlikely(dip->di_magic != cpu_to_be16(XFS_DINODE_MAGIC))) {
		xfs_buf_relse(bp);
		xfs_alert(mp,
	"%s: Bad inode magic number, dip = 0x%p, dino bp = 0x%p, ino = %Ld",
			__func__, dip, bp, in_f->ilf_ino);
		XFS_ERROR_REPORT("xlog_recover_inode_pass2(1)",
				 XFS_ERRLEVEL_LOW, mp);
		error = EFSCORRUPTED;
		goto error;
	}
	dicp = item->ri_buf[1].i_addr;
	if (unlikely(dicp->di_magic != XFS_DINODE_MAGIC)) {
		xfs_buf_relse(bp);
		xfs_alert(mp,
			"%s: Bad inode log record, rec ptr 0x%p, ino %Ld",
			__func__, item, in_f->ilf_ino);
		XFS_ERROR_REPORT("xlog_recover_inode_pass2(2)",
				 XFS_ERRLEVEL_LOW, mp);
		error = EFSCORRUPTED;
		goto error;
	}

	
	if (dicp->di_flushiter < be16_to_cpu(dip->di_flushiter)) {
		if (be16_to_cpu(dip->di_flushiter) == DI_MAX_FLUSH &&
		    dicp->di_flushiter < (DI_MAX_FLUSH >> 1)) {
			
		} else {
			xfs_buf_relse(bp);
			trace_xfs_log_recover_inode_skip(log, in_f);
			error = 0;
			goto error;
		}
	}
	
	dicp->di_flushiter = 0;

	if (unlikely(S_ISREG(dicp->di_mode))) {
		if ((dicp->di_format != XFS_DINODE_FMT_EXTENTS) &&
		    (dicp->di_format != XFS_DINODE_FMT_BTREE)) {
			XFS_CORRUPTION_ERROR("xlog_recover_inode_pass2(3)",
					 XFS_ERRLEVEL_LOW, mp, dicp);
			xfs_buf_relse(bp);
			xfs_alert(mp,
		"%s: Bad regular inode log record, rec ptr 0x%p, "
		"ino ptr = 0x%p, ino bp = 0x%p, ino %Ld",
				__func__, item, dip, bp, in_f->ilf_ino);
			error = EFSCORRUPTED;
			goto error;
		}
	} else if (unlikely(S_ISDIR(dicp->di_mode))) {
		if ((dicp->di_format != XFS_DINODE_FMT_EXTENTS) &&
		    (dicp->di_format != XFS_DINODE_FMT_BTREE) &&
		    (dicp->di_format != XFS_DINODE_FMT_LOCAL)) {
			XFS_CORRUPTION_ERROR("xlog_recover_inode_pass2(4)",
					     XFS_ERRLEVEL_LOW, mp, dicp);
			xfs_buf_relse(bp);
			xfs_alert(mp,
		"%s: Bad dir inode log record, rec ptr 0x%p, "
		"ino ptr = 0x%p, ino bp = 0x%p, ino %Ld",
				__func__, item, dip, bp, in_f->ilf_ino);
			error = EFSCORRUPTED;
			goto error;
		}
	}
	if (unlikely(dicp->di_nextents + dicp->di_anextents > dicp->di_nblocks)){
		XFS_CORRUPTION_ERROR("xlog_recover_inode_pass2(5)",
				     XFS_ERRLEVEL_LOW, mp, dicp);
		xfs_buf_relse(bp);
		xfs_alert(mp,
	"%s: Bad inode log record, rec ptr 0x%p, dino ptr 0x%p, "
	"dino bp 0x%p, ino %Ld, total extents = %d, nblocks = %Ld",
			__func__, item, dip, bp, in_f->ilf_ino,
			dicp->di_nextents + dicp->di_anextents,
			dicp->di_nblocks);
		error = EFSCORRUPTED;
		goto error;
	}
	if (unlikely(dicp->di_forkoff > mp->m_sb.sb_inodesize)) {
		XFS_CORRUPTION_ERROR("xlog_recover_inode_pass2(6)",
				     XFS_ERRLEVEL_LOW, mp, dicp);
		xfs_buf_relse(bp);
		xfs_alert(mp,
	"%s: Bad inode log record, rec ptr 0x%p, dino ptr 0x%p, "
	"dino bp 0x%p, ino %Ld, forkoff 0x%x", __func__,
			item, dip, bp, in_f->ilf_ino, dicp->di_forkoff);
		error = EFSCORRUPTED;
		goto error;
	}
	if (unlikely(item->ri_buf[1].i_len > sizeof(struct xfs_icdinode))) {
		XFS_CORRUPTION_ERROR("xlog_recover_inode_pass2(7)",
				     XFS_ERRLEVEL_LOW, mp, dicp);
		xfs_buf_relse(bp);
		xfs_alert(mp,
			"%s: Bad inode log record length %d, rec ptr 0x%p",
			__func__, item->ri_buf[1].i_len, item);
		error = EFSCORRUPTED;
		goto error;
	}

	
	xfs_dinode_to_disk(dip, item->ri_buf[1].i_addr);

	
	if (item->ri_buf[1].i_len > sizeof(struct xfs_icdinode)) {
		memcpy((xfs_caddr_t) dip + sizeof(struct xfs_icdinode),
			item->ri_buf[1].i_addr + sizeof(struct xfs_icdinode),
			item->ri_buf[1].i_len  - sizeof(struct xfs_icdinode));
	}

	fields = in_f->ilf_fields;
	switch (fields & (XFS_ILOG_DEV | XFS_ILOG_UUID)) {
	case XFS_ILOG_DEV:
		xfs_dinode_put_rdev(dip, in_f->ilf_u.ilfu_rdev);
		break;
	case XFS_ILOG_UUID:
		memcpy(XFS_DFORK_DPTR(dip),
		       &in_f->ilf_u.ilfu_uuid,
		       sizeof(uuid_t));
		break;
	}

	if (in_f->ilf_size == 2)
		goto write_inode_buffer;
	len = item->ri_buf[2].i_len;
	src = item->ri_buf[2].i_addr;
	ASSERT(in_f->ilf_size <= 4);
	ASSERT((in_f->ilf_size == 3) || (fields & XFS_ILOG_AFORK));
	ASSERT(!(fields & XFS_ILOG_DFORK) ||
	       (len == in_f->ilf_dsize));

	switch (fields & XFS_ILOG_DFORK) {
	case XFS_ILOG_DDATA:
	case XFS_ILOG_DEXT:
		memcpy(XFS_DFORK_DPTR(dip), src, len);
		break;

	case XFS_ILOG_DBROOT:
		xfs_bmbt_to_bmdr(mp, (struct xfs_btree_block *)src, len,
				 (xfs_bmdr_block_t *)XFS_DFORK_DPTR(dip),
				 XFS_DFORK_DSIZE(dip, mp));
		break;

	default:
		ASSERT((fields & XFS_ILOG_DFORK) == 0);
		break;
	}

	if (in_f->ilf_fields & XFS_ILOG_AFORK) {
		if (in_f->ilf_fields & XFS_ILOG_DFORK) {
			attr_index = 3;
		} else {
			attr_index = 2;
		}
		len = item->ri_buf[attr_index].i_len;
		src = item->ri_buf[attr_index].i_addr;
		ASSERT(len == in_f->ilf_asize);

		switch (in_f->ilf_fields & XFS_ILOG_AFORK) {
		case XFS_ILOG_ADATA:
		case XFS_ILOG_AEXT:
			dest = XFS_DFORK_APTR(dip);
			ASSERT(len <= XFS_DFORK_ASIZE(dip, mp));
			memcpy(dest, src, len);
			break;

		case XFS_ILOG_ABROOT:
			dest = XFS_DFORK_APTR(dip);
			xfs_bmbt_to_bmdr(mp, (struct xfs_btree_block *)src,
					 len, (xfs_bmdr_block_t*)dest,
					 XFS_DFORK_ASIZE(dip, mp));
			break;

		default:
			xfs_warn(log->l_mp, "%s: Invalid flag", __func__);
			ASSERT(0);
			xfs_buf_relse(bp);
			error = EIO;
			goto error;
		}
	}

write_inode_buffer:
	ASSERT(bp->b_target->bt_mount == mp);
	bp->b_iodone = xlog_recover_iodone;
	xfs_buf_delwri_queue(bp);
	xfs_buf_relse(bp);
error:
	if (need_free)
		kmem_free(in_f);
	return XFS_ERROR(error);
}

STATIC int
xlog_recover_quotaoff_pass1(
	xlog_t			*log,
	xlog_recover_item_t	*item)
{
	xfs_qoff_logformat_t	*qoff_f = item->ri_buf[0].i_addr;
	ASSERT(qoff_f);

	if (qoff_f->qf_flags & XFS_UQUOTA_ACCT)
		log->l_quotaoffs_flag |= XFS_DQ_USER;
	if (qoff_f->qf_flags & XFS_PQUOTA_ACCT)
		log->l_quotaoffs_flag |= XFS_DQ_PROJ;
	if (qoff_f->qf_flags & XFS_GQUOTA_ACCT)
		log->l_quotaoffs_flag |= XFS_DQ_GROUP;

	return (0);
}

STATIC int
xlog_recover_dquot_pass2(
	xlog_t			*log,
	xlog_recover_item_t	*item)
{
	xfs_mount_t		*mp = log->l_mp;
	xfs_buf_t		*bp;
	struct xfs_disk_dquot	*ddq, *recddq;
	int			error;
	xfs_dq_logformat_t	*dq_f;
	uint			type;


	if (mp->m_qflags == 0)
		return (0);

	recddq = item->ri_buf[1].i_addr;
	if (recddq == NULL) {
		xfs_alert(log->l_mp, "NULL dquot in %s.", __func__);
		return XFS_ERROR(EIO);
	}
	if (item->ri_buf[1].i_len < sizeof(xfs_disk_dquot_t)) {
		xfs_alert(log->l_mp, "dquot too small (%d) in %s.",
			item->ri_buf[1].i_len, __func__);
		return XFS_ERROR(EIO);
	}

	type = recddq->d_flags & (XFS_DQ_USER | XFS_DQ_PROJ | XFS_DQ_GROUP);
	ASSERT(type);
	if (log->l_quotaoffs_flag & type)
		return (0);

	dq_f = item->ri_buf[0].i_addr;
	ASSERT(dq_f);
	error = xfs_qm_dqcheck(mp, recddq, dq_f->qlf_id, 0, XFS_QMOPT_DOWARN,
			   "xlog_recover_dquot_pass2 (log copy)");
	if (error)
		return XFS_ERROR(EIO);
	ASSERT(dq_f->qlf_len == 1);

	error = xfs_read_buf(mp, mp->m_ddev_targp,
			     dq_f->qlf_blkno,
			     XFS_FSB_TO_BB(mp, dq_f->qlf_len),
			     0, &bp);
	if (error) {
		xfs_buf_ioerror_alert(bp, "xlog_recover_do..(read#3)");
		return error;
	}
	ASSERT(bp);
	ddq = (xfs_disk_dquot_t *)xfs_buf_offset(bp, dq_f->qlf_boffset);

	error = xfs_qm_dqcheck(mp, ddq, dq_f->qlf_id, 0, XFS_QMOPT_DOWARN,
			   "xlog_recover_dquot_pass2");
	if (error) {
		xfs_buf_relse(bp);
		return XFS_ERROR(EIO);
	}

	memcpy(ddq, recddq, item->ri_buf[1].i_len);

	ASSERT(dq_f->qlf_size == 2);
	ASSERT(bp->b_target->bt_mount == mp);
	bp->b_iodone = xlog_recover_iodone;
	xfs_buf_delwri_queue(bp);
	xfs_buf_relse(bp);

	return (0);
}

STATIC int
xlog_recover_efi_pass2(
	xlog_t			*log,
	xlog_recover_item_t	*item,
	xfs_lsn_t		lsn)
{
	int			error;
	xfs_mount_t		*mp = log->l_mp;
	xfs_efi_log_item_t	*efip;
	xfs_efi_log_format_t	*efi_formatp;

	efi_formatp = item->ri_buf[0].i_addr;

	efip = xfs_efi_init(mp, efi_formatp->efi_nextents);
	if ((error = xfs_efi_copy_format(&(item->ri_buf[0]),
					 &(efip->efi_format)))) {
		xfs_efi_item_free(efip);
		return error;
	}
	atomic_set(&efip->efi_next_extent, efi_formatp->efi_nextents);

	spin_lock(&log->l_ailp->xa_lock);
	xfs_trans_ail_update(log->l_ailp, &efip->efi_item, lsn);
	return 0;
}


STATIC int
xlog_recover_efd_pass2(
	xlog_t			*log,
	xlog_recover_item_t	*item)
{
	xfs_efd_log_format_t	*efd_formatp;
	xfs_efi_log_item_t	*efip = NULL;
	xfs_log_item_t		*lip;
	__uint64_t		efi_id;
	struct xfs_ail_cursor	cur;
	struct xfs_ail		*ailp = log->l_ailp;

	efd_formatp = item->ri_buf[0].i_addr;
	ASSERT((item->ri_buf[0].i_len == (sizeof(xfs_efd_log_format_32_t) +
		((efd_formatp->efd_nextents - 1) * sizeof(xfs_extent_32_t)))) ||
	       (item->ri_buf[0].i_len == (sizeof(xfs_efd_log_format_64_t) +
		((efd_formatp->efd_nextents - 1) * sizeof(xfs_extent_64_t)))));
	efi_id = efd_formatp->efd_efi_id;

	spin_lock(&ailp->xa_lock);
	lip = xfs_trans_ail_cursor_first(ailp, &cur, 0);
	while (lip != NULL) {
		if (lip->li_type == XFS_LI_EFI) {
			efip = (xfs_efi_log_item_t *)lip;
			if (efip->efi_format.efi_id == efi_id) {
				xfs_trans_ail_delete(ailp, lip);
				xfs_efi_item_free(efip);
				spin_lock(&ailp->xa_lock);
				break;
			}
		}
		lip = xfs_trans_ail_cursor_next(ailp, &cur);
	}
	xfs_trans_ail_cursor_done(ailp, &cur);
	spin_unlock(&ailp->xa_lock);

	return 0;
}

STATIC void
xlog_recover_free_trans(
	struct xlog_recover	*trans)
{
	xlog_recover_item_t	*item, *n;
	int			i;

	list_for_each_entry_safe(item, n, &trans->r_itemq, ri_list) {
		
		list_del(&item->ri_list);
		for (i = 0; i < item->ri_cnt; i++)
			kmem_free(item->ri_buf[i].i_addr);
		
		kmem_free(item->ri_buf);
		kmem_free(item);
	}
	
	kmem_free(trans);
}

STATIC int
xlog_recover_commit_pass1(
	struct log		*log,
	struct xlog_recover	*trans,
	xlog_recover_item_t	*item)
{
	trace_xfs_log_recover_item_recover(log, trans, item, XLOG_RECOVER_PASS1);

	switch (ITEM_TYPE(item)) {
	case XFS_LI_BUF:
		return xlog_recover_buffer_pass1(log, item);
	case XFS_LI_QUOTAOFF:
		return xlog_recover_quotaoff_pass1(log, item);
	case XFS_LI_INODE:
	case XFS_LI_EFI:
	case XFS_LI_EFD:
	case XFS_LI_DQUOT:
		
		return 0;
	default:
		xfs_warn(log->l_mp, "%s: invalid item type (%d)",
			__func__, ITEM_TYPE(item));
		ASSERT(0);
		return XFS_ERROR(EIO);
	}
}

STATIC int
xlog_recover_commit_pass2(
	struct log		*log,
	struct xlog_recover	*trans,
	xlog_recover_item_t	*item)
{
	trace_xfs_log_recover_item_recover(log, trans, item, XLOG_RECOVER_PASS2);

	switch (ITEM_TYPE(item)) {
	case XFS_LI_BUF:
		return xlog_recover_buffer_pass2(log, item);
	case XFS_LI_INODE:
		return xlog_recover_inode_pass2(log, item);
	case XFS_LI_EFI:
		return xlog_recover_efi_pass2(log, item, trans->r_lsn);
	case XFS_LI_EFD:
		return xlog_recover_efd_pass2(log, item);
	case XFS_LI_DQUOT:
		return xlog_recover_dquot_pass2(log, item);
	case XFS_LI_QUOTAOFF:
		
		return 0;
	default:
		xfs_warn(log->l_mp, "%s: invalid item type (%d)",
			__func__, ITEM_TYPE(item));
		ASSERT(0);
		return XFS_ERROR(EIO);
	}
}

STATIC int
xlog_recover_commit_trans(
	struct log		*log,
	struct xlog_recover	*trans,
	int			pass)
{
	int			error = 0;
	xlog_recover_item_t	*item;

	hlist_del(&trans->r_list);

	error = xlog_recover_reorder_trans(log, trans, pass);
	if (error)
		return error;

	list_for_each_entry(item, &trans->r_itemq, ri_list) {
		if (pass == XLOG_RECOVER_PASS1)
			error = xlog_recover_commit_pass1(log, trans, item);
		else
			error = xlog_recover_commit_pass2(log, trans, item);
		if (error)
			return error;
	}

	xlog_recover_free_trans(trans);
	return 0;
}

STATIC int
xlog_recover_unmount_trans(
	struct log		*log,
	xlog_recover_t		*trans)
{
	
	xfs_warn(log->l_mp, "%s: Unmount LR", __func__);
	return 0;
}

STATIC int
xlog_recover_process_data(
	xlog_t			*log,
	struct hlist_head	rhash[],
	xlog_rec_header_t	*rhead,
	xfs_caddr_t		dp,
	int			pass)
{
	xfs_caddr_t		lp;
	int			num_logops;
	xlog_op_header_t	*ohead;
	xlog_recover_t		*trans;
	xlog_tid_t		tid;
	int			error;
	unsigned long		hash;
	uint			flags;

	lp = dp + be32_to_cpu(rhead->h_len);
	num_logops = be32_to_cpu(rhead->h_num_logops);

	
	if (xlog_header_check_recover(log->l_mp, rhead))
		return (XFS_ERROR(EIO));

	while ((dp < lp) && num_logops) {
		ASSERT(dp + sizeof(xlog_op_header_t) <= lp);
		ohead = (xlog_op_header_t *)dp;
		dp += sizeof(xlog_op_header_t);
		if (ohead->oh_clientid != XFS_TRANSACTION &&
		    ohead->oh_clientid != XFS_LOG) {
			xfs_warn(log->l_mp, "%s: bad clientid 0x%x",
					__func__, ohead->oh_clientid);
			ASSERT(0);
			return (XFS_ERROR(EIO));
		}
		tid = be32_to_cpu(ohead->oh_tid);
		hash = XLOG_RHASH(tid);
		trans = xlog_recover_find_tid(&rhash[hash], tid);
		if (trans == NULL) {		   
			if (ohead->oh_flags & XLOG_START_TRANS)
				xlog_recover_new_tid(&rhash[hash], tid,
					be64_to_cpu(rhead->h_lsn));
		} else {
			if (dp + be32_to_cpu(ohead->oh_len) > lp) {
				xfs_warn(log->l_mp, "%s: bad length 0x%x",
					__func__, be32_to_cpu(ohead->oh_len));
				WARN_ON(1);
				return (XFS_ERROR(EIO));
			}
			flags = ohead->oh_flags & ~XLOG_END_TRANS;
			if (flags & XLOG_WAS_CONT_TRANS)
				flags &= ~XLOG_CONTINUE_TRANS;
			switch (flags) {
			case XLOG_COMMIT_TRANS:
				error = xlog_recover_commit_trans(log,
								trans, pass);
				break;
			case XLOG_UNMOUNT_TRANS:
				error = xlog_recover_unmount_trans(log, trans);
				break;
			case XLOG_WAS_CONT_TRANS:
				error = xlog_recover_add_to_cont_trans(log,
						trans, dp,
						be32_to_cpu(ohead->oh_len));
				break;
			case XLOG_START_TRANS:
				xfs_warn(log->l_mp, "%s: bad transaction",
					__func__);
				ASSERT(0);
				error = XFS_ERROR(EIO);
				break;
			case 0:
			case XLOG_CONTINUE_TRANS:
				error = xlog_recover_add_to_trans(log, trans,
						dp, be32_to_cpu(ohead->oh_len));
				break;
			default:
				xfs_warn(log->l_mp, "%s: bad flag 0x%x",
					__func__, flags);
				ASSERT(0);
				error = XFS_ERROR(EIO);
				break;
			}
			if (error)
				return error;
		}
		dp += be32_to_cpu(ohead->oh_len);
		num_logops--;
	}
	return 0;
}

STATIC int
xlog_recover_process_efi(
	xfs_mount_t		*mp,
	xfs_efi_log_item_t	*efip)
{
	xfs_efd_log_item_t	*efdp;
	xfs_trans_t		*tp;
	int			i;
	int			error = 0;
	xfs_extent_t		*extp;
	xfs_fsblock_t		startblock_fsb;

	ASSERT(!test_bit(XFS_EFI_RECOVERED, &efip->efi_flags));

	for (i = 0; i < efip->efi_format.efi_nextents; i++) {
		extp = &(efip->efi_format.efi_extents[i]);
		startblock_fsb = XFS_BB_TO_FSB(mp,
				   XFS_FSB_TO_DADDR(mp, extp->ext_start));
		if ((startblock_fsb == 0) ||
		    (extp->ext_len == 0) ||
		    (startblock_fsb >= mp->m_sb.sb_dblocks) ||
		    (extp->ext_len >= mp->m_sb.sb_agblocks)) {
			xfs_efi_release(efip, efip->efi_format.efi_nextents);
			return XFS_ERROR(EIO);
		}
	}

	tp = xfs_trans_alloc(mp, 0);
	error = xfs_trans_reserve(tp, 0, XFS_ITRUNCATE_LOG_RES(mp), 0, 0, 0);
	if (error)
		goto abort_error;
	efdp = xfs_trans_get_efd(tp, efip, efip->efi_format.efi_nextents);

	for (i = 0; i < efip->efi_format.efi_nextents; i++) {
		extp = &(efip->efi_format.efi_extents[i]);
		error = xfs_free_extent(tp, extp->ext_start, extp->ext_len);
		if (error)
			goto abort_error;
		xfs_trans_log_efd_extent(tp, efdp, extp->ext_start,
					 extp->ext_len);
	}

	set_bit(XFS_EFI_RECOVERED, &efip->efi_flags);
	error = xfs_trans_commit(tp, 0);
	return error;

abort_error:
	xfs_trans_cancel(tp, XFS_TRANS_ABORT);
	return error;
}

STATIC int
xlog_recover_process_efis(
	xlog_t			*log)
{
	xfs_log_item_t		*lip;
	xfs_efi_log_item_t	*efip;
	int			error = 0;
	struct xfs_ail_cursor	cur;
	struct xfs_ail		*ailp;

	ailp = log->l_ailp;
	spin_lock(&ailp->xa_lock);
	lip = xfs_trans_ail_cursor_first(ailp, &cur, 0);
	while (lip != NULL) {
		if (lip->li_type != XFS_LI_EFI) {
#ifdef DEBUG
			for (; lip; lip = xfs_trans_ail_cursor_next(ailp, &cur))
				ASSERT(lip->li_type != XFS_LI_EFI);
#endif
			break;
		}

		efip = (xfs_efi_log_item_t *)lip;
		if (test_bit(XFS_EFI_RECOVERED, &efip->efi_flags)) {
			lip = xfs_trans_ail_cursor_next(ailp, &cur);
			continue;
		}

		spin_unlock(&ailp->xa_lock);
		error = xlog_recover_process_efi(log->l_mp, efip);
		spin_lock(&ailp->xa_lock);
		if (error)
			goto out;
		lip = xfs_trans_ail_cursor_next(ailp, &cur);
	}
out:
	xfs_trans_ail_cursor_done(ailp, &cur);
	spin_unlock(&ailp->xa_lock);
	return error;
}

STATIC void
xlog_recover_clear_agi_bucket(
	xfs_mount_t	*mp,
	xfs_agnumber_t	agno,
	int		bucket)
{
	xfs_trans_t	*tp;
	xfs_agi_t	*agi;
	xfs_buf_t	*agibp;
	int		offset;
	int		error;

	tp = xfs_trans_alloc(mp, XFS_TRANS_CLEAR_AGI_BUCKET);
	error = xfs_trans_reserve(tp, 0, XFS_CLEAR_AGI_BUCKET_LOG_RES(mp),
				  0, 0, 0);
	if (error)
		goto out_abort;

	error = xfs_read_agi(mp, tp, agno, &agibp);
	if (error)
		goto out_abort;

	agi = XFS_BUF_TO_AGI(agibp);
	agi->agi_unlinked[bucket] = cpu_to_be32(NULLAGINO);
	offset = offsetof(xfs_agi_t, agi_unlinked) +
		 (sizeof(xfs_agino_t) * bucket);
	xfs_trans_log_buf(tp, agibp, offset,
			  (offset + sizeof(xfs_agino_t) - 1));

	error = xfs_trans_commit(tp, 0);
	if (error)
		goto out_error;
	return;

out_abort:
	xfs_trans_cancel(tp, XFS_TRANS_ABORT);
out_error:
	xfs_warn(mp, "%s: failed to clear agi %d. Continuing.", __func__, agno);
	return;
}

STATIC xfs_agino_t
xlog_recover_process_one_iunlink(
	struct xfs_mount		*mp,
	xfs_agnumber_t			agno,
	xfs_agino_t			agino,
	int				bucket)
{
	struct xfs_buf			*ibp;
	struct xfs_dinode		*dip;
	struct xfs_inode		*ip;
	xfs_ino_t			ino;
	int				error;

	ino = XFS_AGINO_TO_INO(mp, agno, agino);
	error = xfs_iget(mp, NULL, ino, 0, 0, &ip);
	if (error)
		goto fail;

	error = xfs_itobp(mp, NULL, ip, &dip, &ibp, XBF_LOCK);
	if (error)
		goto fail_iput;

	ASSERT(ip->i_d.di_nlink == 0);
	ASSERT(ip->i_d.di_mode != 0);

	
	agino = be32_to_cpu(dip->di_next_unlinked);
	xfs_buf_relse(ibp);

	ip->i_d.di_dmevmask = 0;

	IRELE(ip);
	return agino;

 fail_iput:
	IRELE(ip);
 fail:
	xlog_recover_clear_agi_bucket(mp, agno, bucket);
	return NULLAGINO;
}

STATIC void
xlog_recover_process_iunlinks(
	xlog_t		*log)
{
	xfs_mount_t	*mp;
	xfs_agnumber_t	agno;
	xfs_agi_t	*agi;
	xfs_buf_t	*agibp;
	xfs_agino_t	agino;
	int		bucket;
	int		error;
	uint		mp_dmevmask;

	mp = log->l_mp;

	mp_dmevmask = mp->m_dmevmask;
	mp->m_dmevmask = 0;

	for (agno = 0; agno < mp->m_sb.sb_agcount; agno++) {
		error = xfs_read_agi(mp, NULL, agno, &agibp);
		if (error) {
			continue;
		}
		agi = XFS_BUF_TO_AGI(agibp);
		xfs_buf_unlock(agibp);

		for (bucket = 0; bucket < XFS_AGI_UNLINKED_BUCKETS; bucket++) {
			agino = be32_to_cpu(agi->agi_unlinked[bucket]);
			while (agino != NULLAGINO) {
				agino = xlog_recover_process_one_iunlink(mp,
							agno, agino, bucket);
			}
		}
		xfs_buf_rele(agibp);
	}

	mp->m_dmevmask = mp_dmevmask;
}


#ifdef DEBUG
STATIC void
xlog_pack_data_checksum(
	xlog_t		*log,
	xlog_in_core_t	*iclog,
	int		size)
{
	int		i;
	__be32		*up;
	uint		chksum = 0;

	up = (__be32 *)iclog->ic_datap;
	
	for (i = 0; i < (size >> 2); i++) {
		chksum ^= be32_to_cpu(*up);
		up++;
	}
	iclog->ic_header.h_chksum = cpu_to_be32(chksum);
}
#else
#define xlog_pack_data_checksum(log, iclog, size)
#endif

void
xlog_pack_data(
	xlog_t			*log,
	xlog_in_core_t		*iclog,
	int			roundoff)
{
	int			i, j, k;
	int			size = iclog->ic_offset + roundoff;
	__be32			cycle_lsn;
	xfs_caddr_t		dp;

	xlog_pack_data_checksum(log, iclog, size);

	cycle_lsn = CYCLE_LSN_DISK(iclog->ic_header.h_lsn);

	dp = iclog->ic_datap;
	for (i = 0; i < BTOBB(size) &&
		i < (XLOG_HEADER_CYCLE_SIZE / BBSIZE); i++) {
		iclog->ic_header.h_cycle_data[i] = *(__be32 *)dp;
		*(__be32 *)dp = cycle_lsn;
		dp += BBSIZE;
	}

	if (xfs_sb_version_haslogv2(&log->l_mp->m_sb)) {
		xlog_in_core_2_t *xhdr = iclog->ic_data;

		for ( ; i < BTOBB(size); i++) {
			j = i / (XLOG_HEADER_CYCLE_SIZE / BBSIZE);
			k = i % (XLOG_HEADER_CYCLE_SIZE / BBSIZE);
			xhdr[j].hic_xheader.xh_cycle_data[k] = *(__be32 *)dp;
			*(__be32 *)dp = cycle_lsn;
			dp += BBSIZE;
		}

		for (i = 1; i < log->l_iclog_heads; i++) {
			xhdr[i].hic_xheader.xh_cycle = cycle_lsn;
		}
	}
}

STATIC void
xlog_unpack_data(
	xlog_rec_header_t	*rhead,
	xfs_caddr_t		dp,
	xlog_t			*log)
{
	int			i, j, k;

	for (i = 0; i < BTOBB(be32_to_cpu(rhead->h_len)) &&
		  i < (XLOG_HEADER_CYCLE_SIZE / BBSIZE); i++) {
		*(__be32 *)dp = *(__be32 *)&rhead->h_cycle_data[i];
		dp += BBSIZE;
	}

	if (xfs_sb_version_haslogv2(&log->l_mp->m_sb)) {
		xlog_in_core_2_t *xhdr = (xlog_in_core_2_t *)rhead;
		for ( ; i < BTOBB(be32_to_cpu(rhead->h_len)); i++) {
			j = i / (XLOG_HEADER_CYCLE_SIZE / BBSIZE);
			k = i % (XLOG_HEADER_CYCLE_SIZE / BBSIZE);
			*(__be32 *)dp = xhdr[j].hic_xheader.xh_cycle_data[k];
			dp += BBSIZE;
		}
	}
}

STATIC int
xlog_valid_rec_header(
	xlog_t			*log,
	xlog_rec_header_t	*rhead,
	xfs_daddr_t		blkno)
{
	int			hlen;

	if (unlikely(rhead->h_magicno != cpu_to_be32(XLOG_HEADER_MAGIC_NUM))) {
		XFS_ERROR_REPORT("xlog_valid_rec_header(1)",
				XFS_ERRLEVEL_LOW, log->l_mp);
		return XFS_ERROR(EFSCORRUPTED);
	}
	if (unlikely(
	    (!rhead->h_version ||
	    (be32_to_cpu(rhead->h_version) & (~XLOG_VERSION_OKBITS))))) {
		xfs_warn(log->l_mp, "%s: unrecognised log version (%d).",
			__func__, be32_to_cpu(rhead->h_version));
		return XFS_ERROR(EIO);
	}

	/* LR body must have data or it wouldn't have been written */
	hlen = be32_to_cpu(rhead->h_len);
	if (unlikely( hlen <= 0 || hlen > INT_MAX )) {
		XFS_ERROR_REPORT("xlog_valid_rec_header(2)",
				XFS_ERRLEVEL_LOW, log->l_mp);
		return XFS_ERROR(EFSCORRUPTED);
	}
	if (unlikely( blkno > log->l_logBBsize || blkno > INT_MAX )) {
		XFS_ERROR_REPORT("xlog_valid_rec_header(3)",
				XFS_ERRLEVEL_LOW, log->l_mp);
		return XFS_ERROR(EFSCORRUPTED);
	}
	return 0;
}

STATIC int
xlog_do_recovery_pass(
	xlog_t			*log,
	xfs_daddr_t		head_blk,
	xfs_daddr_t		tail_blk,
	int			pass)
{
	xlog_rec_header_t	*rhead;
	xfs_daddr_t		blk_no;
	xfs_caddr_t		offset;
	xfs_buf_t		*hbp, *dbp;
	int			error = 0, h_size;
	int			bblks, split_bblks;
	int			hblks, split_hblks, wrapped_hblks;
	struct hlist_head	rhash[XLOG_RHASH_SIZE];

	ASSERT(head_blk != tail_blk);

	if (xfs_sb_version_haslogv2(&log->l_mp->m_sb)) {
		hbp = xlog_get_bp(log, 1);
		if (!hbp)
			return ENOMEM;

		error = xlog_bread(log, tail_blk, 1, hbp, &offset);
		if (error)
			goto bread_err1;

		rhead = (xlog_rec_header_t *)offset;
		error = xlog_valid_rec_header(log, rhead, tail_blk);
		if (error)
			goto bread_err1;
		h_size = be32_to_cpu(rhead->h_size);
		if ((be32_to_cpu(rhead->h_version) & XLOG_VERSION_2) &&
		    (h_size > XLOG_HEADER_CYCLE_SIZE)) {
			hblks = h_size / XLOG_HEADER_CYCLE_SIZE;
			if (h_size % XLOG_HEADER_CYCLE_SIZE)
				hblks++;
			xlog_put_bp(hbp);
			hbp = xlog_get_bp(log, hblks);
		} else {
			hblks = 1;
		}
	} else {
		ASSERT(log->l_sectBBsize == 1);
		hblks = 1;
		hbp = xlog_get_bp(log, 1);
		h_size = XLOG_BIG_RECORD_BSIZE;
	}

	if (!hbp)
		return ENOMEM;
	dbp = xlog_get_bp(log, BTOBB(h_size));
	if (!dbp) {
		xlog_put_bp(hbp);
		return ENOMEM;
	}

	memset(rhash, 0, sizeof(rhash));
	if (tail_blk <= head_blk) {
		for (blk_no = tail_blk; blk_no < head_blk; ) {
			error = xlog_bread(log, blk_no, hblks, hbp, &offset);
			if (error)
				goto bread_err2;

			rhead = (xlog_rec_header_t *)offset;
			error = xlog_valid_rec_header(log, rhead, blk_no);
			if (error)
				goto bread_err2;

			
			bblks = (int)BTOBB(be32_to_cpu(rhead->h_len));
			error = xlog_bread(log, blk_no + hblks, bblks, dbp,
					   &offset);
			if (error)
				goto bread_err2;

			xlog_unpack_data(rhead, offset, log);
			if ((error = xlog_recover_process_data(log,
						rhash, rhead, offset, pass)))
				goto bread_err2;
			blk_no += bblks + hblks;
		}
	} else {
		blk_no = tail_blk;
		while (blk_no < log->l_logBBsize) {
			offset = hbp->b_addr;
			split_hblks = 0;
			wrapped_hblks = 0;
			if (blk_no + hblks <= log->l_logBBsize) {
				
				error = xlog_bread(log, blk_no, hblks, hbp,
						   &offset);
				if (error)
					goto bread_err2;
			} else {
				
				if (blk_no != log->l_logBBsize) {
					
					ASSERT(blk_no <= INT_MAX);
					split_hblks = log->l_logBBsize - (int)blk_no;
					ASSERT(split_hblks > 0);
					error = xlog_bread(log, blk_no,
							   split_hblks, hbp,
							   &offset);
					if (error)
						goto bread_err2;
				}

				wrapped_hblks = hblks - split_hblks;
				error = xlog_bread_offset(log, 0,
						wrapped_hblks, hbp,
						offset + BBTOB(split_hblks));
				if (error)
					goto bread_err2;
			}
			rhead = (xlog_rec_header_t *)offset;
			error = xlog_valid_rec_header(log, rhead,
						split_hblks ? blk_no : 0);
			if (error)
				goto bread_err2;

			bblks = (int)BTOBB(be32_to_cpu(rhead->h_len));
			blk_no += hblks;

			
			if (blk_no + bblks <= log->l_logBBsize) {
				error = xlog_bread(log, blk_no, bblks, dbp,
						   &offset);
				if (error)
					goto bread_err2;
			} else {
				offset = dbp->b_addr;
				split_bblks = 0;
				if (blk_no != log->l_logBBsize) {
					ASSERT(!wrapped_hblks);
					ASSERT(blk_no <= INT_MAX);
					split_bblks =
						log->l_logBBsize - (int)blk_no;
					ASSERT(split_bblks > 0);
					error = xlog_bread(log, blk_no,
							split_bblks, dbp,
							&offset);
					if (error)
						goto bread_err2;
				}

				error = xlog_bread_offset(log, 0,
						bblks - split_bblks, hbp,
						offset + BBTOB(split_bblks));
				if (error)
					goto bread_err2;
			}
			xlog_unpack_data(rhead, offset, log);
			if ((error = xlog_recover_process_data(log, rhash,
							rhead, offset, pass)))
				goto bread_err2;
			blk_no += bblks;
		}

		ASSERT(blk_no >= log->l_logBBsize);
		blk_no -= log->l_logBBsize;

		
		while (blk_no < head_blk) {
			error = xlog_bread(log, blk_no, hblks, hbp, &offset);
			if (error)
				goto bread_err2;

			rhead = (xlog_rec_header_t *)offset;
			error = xlog_valid_rec_header(log, rhead, blk_no);
			if (error)
				goto bread_err2;

			bblks = (int)BTOBB(be32_to_cpu(rhead->h_len));
			error = xlog_bread(log, blk_no+hblks, bblks, dbp,
					   &offset);
			if (error)
				goto bread_err2;

			xlog_unpack_data(rhead, offset, log);
			if ((error = xlog_recover_process_data(log, rhash,
							rhead, offset, pass)))
				goto bread_err2;
			blk_no += bblks + hblks;
		}
	}

 bread_err2:
	xlog_put_bp(dbp);
 bread_err1:
	xlog_put_bp(hbp);
	return error;
}

/*
 * Do the recovery of the log.  We actually do this in two phases.
 * The two passes are necessary in order to implement the function
 * of cancelling a record written into the log.  The first pass
 * determines those things which have been cancelled, and the
 * second pass replays log items normally except for those which
 * have been cancelled.  The handling of the replay and cancellations
 * takes place in the log item type specific routines.
 *
 * The table of items which have cancel records in the log is allocated
 * and freed at this level, since only here do we know when all of
 * the log recovery has been completed.
 */
STATIC int
xlog_do_log_recovery(
	xlog_t		*log,
	xfs_daddr_t	head_blk,
	xfs_daddr_t	tail_blk)
{
	int		error, i;

	ASSERT(head_blk != tail_blk);

	log->l_buf_cancel_table = kmem_zalloc(XLOG_BC_TABLE_SIZE *
						 sizeof(struct list_head),
						 KM_SLEEP);
	for (i = 0; i < XLOG_BC_TABLE_SIZE; i++)
		INIT_LIST_HEAD(&log->l_buf_cancel_table[i]);

	error = xlog_do_recovery_pass(log, head_blk, tail_blk,
				      XLOG_RECOVER_PASS1);
	if (error != 0) {
		kmem_free(log->l_buf_cancel_table);
		log->l_buf_cancel_table = NULL;
		return error;
	}
	error = xlog_do_recovery_pass(log, head_blk, tail_blk,
				      XLOG_RECOVER_PASS2);
#ifdef DEBUG
	if (!error) {
		int	i;

		for (i = 0; i < XLOG_BC_TABLE_SIZE; i++)
			ASSERT(list_empty(&log->l_buf_cancel_table[i]));
	}
#endif	

	kmem_free(log->l_buf_cancel_table);
	log->l_buf_cancel_table = NULL;

	return error;
}

STATIC int
xlog_do_recover(
	xlog_t		*log,
	xfs_daddr_t	head_blk,
	xfs_daddr_t	tail_blk)
{
	int		error;
	xfs_buf_t	*bp;
	xfs_sb_t	*sbp;

	error = xlog_do_log_recovery(log, head_blk, tail_blk);
	if (error) {
		return error;
	}

	xfs_flush_buftarg(log->l_mp->m_ddev_targp, 1);

	if (XFS_FORCED_SHUTDOWN(log->l_mp)) {
		return (EIO);
	}

	xlog_assign_tail_lsn(log->l_mp);

	bp = xfs_getsb(log->l_mp, 0);
	XFS_BUF_UNDONE(bp);
	ASSERT(!(XFS_BUF_ISWRITE(bp)));
	ASSERT(!(XFS_BUF_ISDELAYWRITE(bp)));
	XFS_BUF_READ(bp);
	XFS_BUF_UNASYNC(bp);
	xfsbdstrat(log->l_mp, bp);
	error = xfs_buf_iowait(bp);
	if (error) {
		xfs_buf_ioerror_alert(bp, __func__);
		ASSERT(0);
		xfs_buf_relse(bp);
		return error;
	}

	
	sbp = &log->l_mp->m_sb;
	xfs_sb_from_disk(log->l_mp, XFS_BUF_TO_SBP(bp));
	ASSERT(sbp->sb_magicnum == XFS_SB_MAGIC);
	ASSERT(xfs_sb_good_version(sbp));
	xfs_buf_relse(bp);

	
	xfs_icsb_reinit_counters(log->l_mp);

	xlog_recover_check_summary(log);

	
	log->l_flags &= ~XLOG_ACTIVE_RECOVERY;
	return 0;
}

int
xlog_recover(
	xlog_t		*log)
{
	xfs_daddr_t	head_blk, tail_blk;
	int		error;

	
	if ((error = xlog_find_tail(log, &head_blk, &tail_blk)))
		return error;

	if (tail_blk != head_blk) {
		if ((error = xfs_dev_is_read_only(log->l_mp, "recovery"))) {
			return error;
		}

		xfs_notice(log->l_mp, "Starting recovery (logdev: %s)",
				log->l_mp->m_logname ? log->l_mp->m_logname
						     : "internal");

		error = xlog_do_recover(log, head_blk, tail_blk);
		log->l_flags |= XLOG_RECOVERY_NEEDED;
	}
	return error;
}

int
xlog_recover_finish(
	xlog_t		*log)
{
	if (log->l_flags & XLOG_RECOVERY_NEEDED) {
		int	error;
		error = xlog_recover_process_efis(log);
		if (error) {
			xfs_alert(log->l_mp, "Failed to recover EFIs");
			return error;
		}
		xfs_log_force(log->l_mp, XFS_LOG_SYNC);

		xlog_recover_process_iunlinks(log);

		xlog_recover_check_summary(log);

		xfs_notice(log->l_mp, "Ending recovery (logdev: %s)",
				log->l_mp->m_logname ? log->l_mp->m_logname
						     : "internal");
		log->l_flags &= ~XLOG_RECOVERY_NEEDED;
	} else {
		xfs_info(log->l_mp, "Ending clean mount");
	}
	return 0;
}


#if defined(DEBUG)
void
xlog_recover_check_summary(
	xlog_t		*log)
{
	xfs_mount_t	*mp;
	xfs_agf_t	*agfp;
	xfs_buf_t	*agfbp;
	xfs_buf_t	*agibp;
	xfs_agnumber_t	agno;
	__uint64_t	freeblks;
	__uint64_t	itotal;
	__uint64_t	ifree;
	int		error;

	mp = log->l_mp;

	freeblks = 0LL;
	itotal = 0LL;
	ifree = 0LL;
	for (agno = 0; agno < mp->m_sb.sb_agcount; agno++) {
		error = xfs_read_agf(mp, NULL, agno, 0, &agfbp);
		if (error) {
			xfs_alert(mp, "%s agf read failed agno %d error %d",
						__func__, agno, error);
		} else {
			agfp = XFS_BUF_TO_AGF(agfbp);
			freeblks += be32_to_cpu(agfp->agf_freeblks) +
				    be32_to_cpu(agfp->agf_flcount);
			xfs_buf_relse(agfbp);
		}

		error = xfs_read_agi(mp, NULL, agno, &agibp);
		if (error) {
			xfs_alert(mp, "%s agi read failed agno %d error %d",
						__func__, agno, error);
		} else {
			struct xfs_agi	*agi = XFS_BUF_TO_AGI(agibp);

			itotal += be32_to_cpu(agi->agi_count);
			ifree += be32_to_cpu(agi->agi_freecount);
			xfs_buf_relse(agibp);
		}
	}
}
#endif 
