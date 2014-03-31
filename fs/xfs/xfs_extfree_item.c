/*
 * Copyright (c) 2000-2001,2005 Silicon Graphics, Inc.
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
#include "xfs_buf_item.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_mount.h"
#include "xfs_trans_priv.h"
#include "xfs_extfree_item.h"


kmem_zone_t	*xfs_efi_zone;
kmem_zone_t	*xfs_efd_zone;

static inline struct xfs_efi_log_item *EFI_ITEM(struct xfs_log_item *lip)
{
	return container_of(lip, struct xfs_efi_log_item, efi_item);
}

void
xfs_efi_item_free(
	struct xfs_efi_log_item	*efip)
{
	if (efip->efi_format.efi_nextents > XFS_EFI_MAX_FAST_EXTENTS)
		kmem_free(efip);
	else
		kmem_zone_free(xfs_efi_zone, efip);
}

STATIC void
__xfs_efi_release(
	struct xfs_efi_log_item	*efip)
{
	struct xfs_ail		*ailp = efip->efi_item.li_ailp;

	if (!test_and_clear_bit(XFS_EFI_COMMITTED, &efip->efi_flags)) {
		spin_lock(&ailp->xa_lock);
		
		xfs_trans_ail_delete(ailp, &efip->efi_item);
		xfs_efi_item_free(efip);
	}
}

STATIC uint
xfs_efi_item_size(
	struct xfs_log_item	*lip)
{
	return 1;
}

STATIC void
xfs_efi_item_format(
	struct xfs_log_item	*lip,
	struct xfs_log_iovec	*log_vector)
{
	struct xfs_efi_log_item	*efip = EFI_ITEM(lip);
	uint			size;

	ASSERT(atomic_read(&efip->efi_next_extent) ==
				efip->efi_format.efi_nextents);

	efip->efi_format.efi_type = XFS_LI_EFI;

	size = sizeof(xfs_efi_log_format_t);
	size += (efip->efi_format.efi_nextents - 1) * sizeof(xfs_extent_t);
	efip->efi_format.efi_size = 1;

	log_vector->i_addr = &efip->efi_format;
	log_vector->i_len = size;
	log_vector->i_type = XLOG_REG_TYPE_EFI_FORMAT;
	ASSERT(size >= sizeof(xfs_efi_log_format_t));
}


STATIC void
xfs_efi_item_pin(
	struct xfs_log_item	*lip)
{
}

STATIC void
xfs_efi_item_unpin(
	struct xfs_log_item	*lip,
	int			remove)
{
	struct xfs_efi_log_item	*efip = EFI_ITEM(lip);

	if (remove) {
		ASSERT(!(lip->li_flags & XFS_LI_IN_AIL));
		if (lip->li_desc)
			xfs_trans_del_item(lip);
		xfs_efi_item_free(efip);
		return;
	}
	__xfs_efi_release(efip);
}

STATIC uint
xfs_efi_item_trylock(
	struct xfs_log_item	*lip)
{
	return XFS_ITEM_PINNED;
}

STATIC void
xfs_efi_item_unlock(
	struct xfs_log_item	*lip)
{
	if (lip->li_flags & XFS_LI_ABORTED)
		xfs_efi_item_free(EFI_ITEM(lip));
}

STATIC xfs_lsn_t
xfs_efi_item_committed(
	struct xfs_log_item	*lip,
	xfs_lsn_t		lsn)
{
	struct xfs_efi_log_item	*efip = EFI_ITEM(lip);

	set_bit(XFS_EFI_COMMITTED, &efip->efi_flags);
	return lsn;
}

STATIC void
xfs_efi_item_push(
	struct xfs_log_item	*lip)
{
}

STATIC void
xfs_efi_item_committing(
	struct xfs_log_item	*lip,
	xfs_lsn_t		lsn)
{
}

static const struct xfs_item_ops xfs_efi_item_ops = {
	.iop_size	= xfs_efi_item_size,
	.iop_format	= xfs_efi_item_format,
	.iop_pin	= xfs_efi_item_pin,
	.iop_unpin	= xfs_efi_item_unpin,
	.iop_trylock	= xfs_efi_item_trylock,
	.iop_unlock	= xfs_efi_item_unlock,
	.iop_committed	= xfs_efi_item_committed,
	.iop_push	= xfs_efi_item_push,
	.iop_committing = xfs_efi_item_committing
};


struct xfs_efi_log_item *
xfs_efi_init(
	struct xfs_mount	*mp,
	uint			nextents)

{
	struct xfs_efi_log_item	*efip;
	uint			size;

	ASSERT(nextents > 0);
	if (nextents > XFS_EFI_MAX_FAST_EXTENTS) {
		size = (uint)(sizeof(xfs_efi_log_item_t) +
			((nextents - 1) * sizeof(xfs_extent_t)));
		efip = kmem_zalloc(size, KM_SLEEP);
	} else {
		efip = kmem_zone_zalloc(xfs_efi_zone, KM_SLEEP);
	}

	xfs_log_item_init(mp, &efip->efi_item, XFS_LI_EFI, &xfs_efi_item_ops);
	efip->efi_format.efi_nextents = nextents;
	efip->efi_format.efi_id = (__psint_t)(void*)efip;
	atomic_set(&efip->efi_next_extent, 0);

	return efip;
}

int
xfs_efi_copy_format(xfs_log_iovec_t *buf, xfs_efi_log_format_t *dst_efi_fmt)
{
	xfs_efi_log_format_t *src_efi_fmt = buf->i_addr;
	uint i;
	uint len = sizeof(xfs_efi_log_format_t) + 
		(src_efi_fmt->efi_nextents - 1) * sizeof(xfs_extent_t);  
	uint len32 = sizeof(xfs_efi_log_format_32_t) + 
		(src_efi_fmt->efi_nextents - 1) * sizeof(xfs_extent_32_t);  
	uint len64 = sizeof(xfs_efi_log_format_64_t) + 
		(src_efi_fmt->efi_nextents - 1) * sizeof(xfs_extent_64_t);  

	if (buf->i_len == len) {
		memcpy((char *)dst_efi_fmt, (char*)src_efi_fmt, len);
		return 0;
	} else if (buf->i_len == len32) {
		xfs_efi_log_format_32_t *src_efi_fmt_32 = buf->i_addr;

		dst_efi_fmt->efi_type     = src_efi_fmt_32->efi_type;
		dst_efi_fmt->efi_size     = src_efi_fmt_32->efi_size;
		dst_efi_fmt->efi_nextents = src_efi_fmt_32->efi_nextents;
		dst_efi_fmt->efi_id       = src_efi_fmt_32->efi_id;
		for (i = 0; i < dst_efi_fmt->efi_nextents; i++) {
			dst_efi_fmt->efi_extents[i].ext_start =
				src_efi_fmt_32->efi_extents[i].ext_start;
			dst_efi_fmt->efi_extents[i].ext_len =
				src_efi_fmt_32->efi_extents[i].ext_len;
		}
		return 0;
	} else if (buf->i_len == len64) {
		xfs_efi_log_format_64_t *src_efi_fmt_64 = buf->i_addr;

		dst_efi_fmt->efi_type     = src_efi_fmt_64->efi_type;
		dst_efi_fmt->efi_size     = src_efi_fmt_64->efi_size;
		dst_efi_fmt->efi_nextents = src_efi_fmt_64->efi_nextents;
		dst_efi_fmt->efi_id       = src_efi_fmt_64->efi_id;
		for (i = 0; i < dst_efi_fmt->efi_nextents; i++) {
			dst_efi_fmt->efi_extents[i].ext_start =
				src_efi_fmt_64->efi_extents[i].ext_start;
			dst_efi_fmt->efi_extents[i].ext_len =
				src_efi_fmt_64->efi_extents[i].ext_len;
		}
		return 0;
	}
	return EFSCORRUPTED;
}

void
xfs_efi_release(xfs_efi_log_item_t	*efip,
		uint			nextents)
{
	ASSERT(atomic_read(&efip->efi_next_extent) >= nextents);
	if (atomic_sub_and_test(nextents, &efip->efi_next_extent))
		__xfs_efi_release(efip);
}

static inline struct xfs_efd_log_item *EFD_ITEM(struct xfs_log_item *lip)
{
	return container_of(lip, struct xfs_efd_log_item, efd_item);
}

STATIC void
xfs_efd_item_free(struct xfs_efd_log_item *efdp)
{
	if (efdp->efd_format.efd_nextents > XFS_EFD_MAX_FAST_EXTENTS)
		kmem_free(efdp);
	else
		kmem_zone_free(xfs_efd_zone, efdp);
}

STATIC uint
xfs_efd_item_size(
	struct xfs_log_item	*lip)
{
	return 1;
}

STATIC void
xfs_efd_item_format(
	struct xfs_log_item	*lip,
	struct xfs_log_iovec	*log_vector)
{
	struct xfs_efd_log_item	*efdp = EFD_ITEM(lip);
	uint			size;

	ASSERT(efdp->efd_next_extent == efdp->efd_format.efd_nextents);

	efdp->efd_format.efd_type = XFS_LI_EFD;

	size = sizeof(xfs_efd_log_format_t);
	size += (efdp->efd_format.efd_nextents - 1) * sizeof(xfs_extent_t);
	efdp->efd_format.efd_size = 1;

	log_vector->i_addr = &efdp->efd_format;
	log_vector->i_len = size;
	log_vector->i_type = XLOG_REG_TYPE_EFD_FORMAT;
	ASSERT(size >= sizeof(xfs_efd_log_format_t));
}

STATIC void
xfs_efd_item_pin(
	struct xfs_log_item	*lip)
{
}

STATIC void
xfs_efd_item_unpin(
	struct xfs_log_item	*lip,
	int			remove)
{
}

STATIC uint
xfs_efd_item_trylock(
	struct xfs_log_item	*lip)
{
	return XFS_ITEM_LOCKED;
}

STATIC void
xfs_efd_item_unlock(
	struct xfs_log_item	*lip)
{
	if (lip->li_flags & XFS_LI_ABORTED)
		xfs_efd_item_free(EFD_ITEM(lip));
}

STATIC xfs_lsn_t
xfs_efd_item_committed(
	struct xfs_log_item	*lip,
	xfs_lsn_t		lsn)
{
	struct xfs_efd_log_item	*efdp = EFD_ITEM(lip);

	if (!(lip->li_flags & XFS_LI_ABORTED))
		xfs_efi_release(efdp->efd_efip, efdp->efd_format.efd_nextents);

	xfs_efd_item_free(efdp);
	return (xfs_lsn_t)-1;
}

STATIC void
xfs_efd_item_push(
	struct xfs_log_item	*lip)
{
}

STATIC void
xfs_efd_item_committing(
	struct xfs_log_item	*lip,
	xfs_lsn_t		lsn)
{
}

static const struct xfs_item_ops xfs_efd_item_ops = {
	.iop_size	= xfs_efd_item_size,
	.iop_format	= xfs_efd_item_format,
	.iop_pin	= xfs_efd_item_pin,
	.iop_unpin	= xfs_efd_item_unpin,
	.iop_trylock	= xfs_efd_item_trylock,
	.iop_unlock	= xfs_efd_item_unlock,
	.iop_committed	= xfs_efd_item_committed,
	.iop_push	= xfs_efd_item_push,
	.iop_committing = xfs_efd_item_committing
};

struct xfs_efd_log_item *
xfs_efd_init(
	struct xfs_mount	*mp,
	struct xfs_efi_log_item	*efip,
	uint			nextents)

{
	struct xfs_efd_log_item	*efdp;
	uint			size;

	ASSERT(nextents > 0);
	if (nextents > XFS_EFD_MAX_FAST_EXTENTS) {
		size = (uint)(sizeof(xfs_efd_log_item_t) +
			((nextents - 1) * sizeof(xfs_extent_t)));
		efdp = kmem_zalloc(size, KM_SLEEP);
	} else {
		efdp = kmem_zone_zalloc(xfs_efd_zone, KM_SLEEP);
	}

	xfs_log_item_init(mp, &efdp->efd_item, XFS_LI_EFD, &xfs_efd_item_ops);
	efdp->efd_efip = efip;
	efdp->efd_format.efd_nextents = nextents;
	efdp->efd_format.efd_efi_id = efip->efi_format.efi_id;

	return efdp;
}
