/*
 * Copyright (c) 2000-2003,2005 Silicon Graphics, Inc.
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
#ifndef __XFS_RTALLOC_H__
#define	__XFS_RTALLOC_H__

struct xfs_mount;
struct xfs_trans;

#define	XFS_MAX_RTEXTSIZE	(1024 * 1024 * 1024)	
#define	XFS_DFL_RTEXTSIZE	(64 * 1024)	        
#define	XFS_MIN_RTEXTSIZE	(4 * 1024)		

#define	XFS_NBBYLOG	3		
#define	XFS_WORDLOG	2		
#define	XFS_NBWORDLOG	(XFS_NBBYLOG + XFS_WORDLOG)
#define	XFS_NBWORD	(1 << XFS_NBWORDLOG)
#define	XFS_WORDMASK	((1 << XFS_WORDLOG) - 1)

#define	XFS_BLOCKSIZE(mp)	((mp)->m_sb.sb_blocksize)
#define	XFS_BLOCKMASK(mp)	((mp)->m_blockmask)
#define	XFS_BLOCKWSIZE(mp)	((mp)->m_blockwsize)
#define	XFS_BLOCKWMASK(mp)	((mp)->m_blockwmask)

#define	XFS_SUMOFFS(mp,ls,bb)	((int)((ls) * (mp)->m_sb.sb_rbmblocks + (bb)))
#define	XFS_SUMOFFSTOBLOCK(mp,s)	\
	(((s) * (uint)sizeof(xfs_suminfo_t)) >> (mp)->m_sb.sb_blocklog)
#define	XFS_SUMPTR(mp,bp,so)	\
	((xfs_suminfo_t *)((bp)->b_addr + \
		(((so) * (uint)sizeof(xfs_suminfo_t)) & XFS_BLOCKMASK(mp))))

#define	XFS_BITTOBLOCK(mp,bi)	((bi) >> (mp)->m_blkbit_log)
#define	XFS_BLOCKTOBIT(mp,bb)	((bb) << (mp)->m_blkbit_log)
#define	XFS_BITTOWORD(mp,bi)	\
	((int)(((bi) >> XFS_NBWORDLOG) & XFS_BLOCKWMASK(mp)))

#define	XFS_RTMIN(a,b)	((a) < (b) ? (a) : (b))
#define	XFS_RTMAX(a,b)	((a) > (b) ? (a) : (b))

#define	XFS_RTLOBIT(w)	xfs_lowbit32(w)
#define	XFS_RTHIBIT(w)	xfs_highbit32(w)

#if XFS_BIG_BLKNOS
#define	XFS_RTBLOCKLOG(b)	xfs_highbit64(b)
#else
#define	XFS_RTBLOCKLOG(b)	xfs_highbit32(b)
#endif


#ifdef __KERNEL__

#ifdef CONFIG_XFS_RT

int					
xfs_rtallocate_extent(
	struct xfs_trans	*tp,	
	xfs_rtblock_t		bno,	
	xfs_extlen_t		minlen,	
	xfs_extlen_t		maxlen,	
	xfs_extlen_t		*len,	
	xfs_alloctype_t		type,	
	int			wasdel,	
	xfs_extlen_t		prod,	
	xfs_rtblock_t		*rtblock); 

int					
xfs_rtfree_extent(
	struct xfs_trans	*tp,	
	xfs_rtblock_t		bno,	
	xfs_extlen_t		len);	

int					
xfs_rtmount_init(
	struct xfs_mount	*mp);	
void
xfs_rtunmount_inodes(
	struct xfs_mount	*mp);

int					
xfs_rtmount_inodes(
	struct xfs_mount	*mp);	

int					
xfs_rtpick_extent(
	struct xfs_mount	*mp,	
	struct xfs_trans	*tp,	
	xfs_extlen_t		len,	
	xfs_rtblock_t		*pick);	

int
xfs_growfs_rt(
	struct xfs_mount	*mp,	
	xfs_growfs_rt_t		*in);	

#else
# define xfs_rtallocate_extent(t,b,min,max,l,a,f,p,rb)  (ENOSYS)
# define xfs_rtfree_extent(t,b,l)                       (ENOSYS)
# define xfs_rtpick_extent(m,t,l,rb)                    (ENOSYS)
# define xfs_growfs_rt(mp,in)                           (ENOSYS)
static inline int		
xfs_rtmount_init(
	xfs_mount_t	*mp)	
{
	if (mp->m_sb.sb_rblocks == 0)
		return 0;

	xfs_warn(mp, "Not built with CONFIG_XFS_RT");
	return ENOSYS;
}
# define xfs_rtmount_inodes(m)  (((mp)->m_sb.sb_rblocks == 0)? 0 : (ENOSYS))
# define xfs_rtunmount_inodes(m)
#endif	

#endif	

#endif	
