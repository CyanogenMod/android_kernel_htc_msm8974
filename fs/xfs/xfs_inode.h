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
#ifndef	__XFS_INODE_H__
#define	__XFS_INODE_H__

struct posix_acl;
struct xfs_dinode;
struct xfs_inode;

#define	XFS_DATA_FORK	0
#define	XFS_ATTR_FORK	1

typedef struct xfs_ext_irec {
	xfs_bmbt_rec_host_t *er_extbuf;	
	xfs_extnum_t	er_extoff;	
	xfs_extnum_t	er_extcount;	
} xfs_ext_irec_t;

#define	XFS_IEXT_BUFSZ		4096
#define	XFS_LINEAR_EXTS		(XFS_IEXT_BUFSZ / (uint)sizeof(xfs_bmbt_rec_t))
#define	XFS_INLINE_EXTS		2
#define	XFS_INLINE_DATA		32
typedef struct xfs_ifork {
	int			if_bytes;	
	int			if_real_bytes;	
	struct xfs_btree_block	*if_broot;	
	short			if_broot_bytes;	
	unsigned char		if_flags;	
	union {
		xfs_bmbt_rec_host_t *if_extents;
		xfs_ext_irec_t	*if_ext_irec;	
		char		*if_data;	
	} if_u1;
	union {
		xfs_bmbt_rec_host_t if_inline_ext[XFS_INLINE_EXTS];
						
		char		if_inline_data[XFS_INLINE_DATA];
						
		xfs_dev_t	if_rdev;	
		uuid_t		if_uuid;	
	} if_u2;
} xfs_ifork_t;

struct xfs_imap {
	xfs_daddr_t	im_blkno;	
	ushort		im_len;		
	ushort		im_boffset;	
};


typedef struct xfs_ictimestamp {
	__int32_t	t_sec;		
	__int32_t	t_nsec;		
} xfs_ictimestamp_t;

typedef struct xfs_icdinode {
	__uint16_t	di_magic;	
	__uint16_t	di_mode;	
	__int8_t	di_version;	
	__int8_t	di_format;	
	__uint16_t	di_onlink;	
	__uint32_t	di_uid;		
	__uint32_t	di_gid;		
	__uint32_t	di_nlink;	
	__uint16_t	di_projid_lo;	
	__uint16_t	di_projid_hi;	
	__uint8_t	di_pad[6];	
	__uint16_t	di_flushiter;	
	xfs_ictimestamp_t di_atime;	
	xfs_ictimestamp_t di_mtime;	
	xfs_ictimestamp_t di_ctime;	
	xfs_fsize_t	di_size;	
	xfs_drfsbno_t	di_nblocks;	
	xfs_extlen_t	di_extsize;	
	xfs_extnum_t	di_nextents;	
	xfs_aextnum_t	di_anextents;	
	__uint8_t	di_forkoff;	
	__int8_t	di_aformat;	
	__uint32_t	di_dmevmask;	
	__uint16_t	di_dmstate;	
	__uint16_t	di_flags;	
	__uint32_t	di_gen;		
} xfs_icdinode_t;

#define	XFS_ICHGTIME_MOD	0x1	
#define	XFS_ICHGTIME_CHG	0x2	

#define	XFS_IFINLINE	0x01	
#define	XFS_IFEXTENTS	0x02	
#define	XFS_IFBROOT	0x04	
#define	XFS_IFEXTIREC	0x08	


#define XFS_IFORK_Q(ip)			((ip)->i_d.di_forkoff != 0)
#define XFS_IFORK_BOFF(ip)		((int)((ip)->i_d.di_forkoff << 3))

#define XFS_IFORK_PTR(ip,w)		\
	((w) == XFS_DATA_FORK ? \
		&(ip)->i_df : \
		(ip)->i_afp)
#define XFS_IFORK_DSIZE(ip) \
	(XFS_IFORK_Q(ip) ? \
		XFS_IFORK_BOFF(ip) : \
		XFS_LITINO((ip)->i_mount))
#define XFS_IFORK_ASIZE(ip) \
	(XFS_IFORK_Q(ip) ? \
		XFS_LITINO((ip)->i_mount) - XFS_IFORK_BOFF(ip) : \
		0)
#define XFS_IFORK_SIZE(ip,w) \
	((w) == XFS_DATA_FORK ? \
		XFS_IFORK_DSIZE(ip) : \
		XFS_IFORK_ASIZE(ip))
#define XFS_IFORK_FORMAT(ip,w) \
	((w) == XFS_DATA_FORK ? \
		(ip)->i_d.di_format : \
		(ip)->i_d.di_aformat)
#define XFS_IFORK_FMT_SET(ip,w,n) \
	((w) == XFS_DATA_FORK ? \
		((ip)->i_d.di_format = (n)) : \
		((ip)->i_d.di_aformat = (n)))
#define XFS_IFORK_NEXTENTS(ip,w) \
	((w) == XFS_DATA_FORK ? \
		(ip)->i_d.di_nextents : \
		(ip)->i_d.di_anextents)
#define XFS_IFORK_NEXT_SET(ip,w,n) \
	((w) == XFS_DATA_FORK ? \
		((ip)->i_d.di_nextents = (n)) : \
		((ip)->i_d.di_anextents = (n)))
#define XFS_IFORK_MAXEXT(ip, w) \
	(XFS_IFORK_SIZE(ip, w) / sizeof(xfs_bmbt_rec_t))


#ifdef __KERNEL__

struct xfs_buf;
struct xfs_bmap_free;
struct xfs_bmbt_irec;
struct xfs_inode_log_item;
struct xfs_mount;
struct xfs_trans;
struct xfs_dquot;

typedef struct xfs_inode {
	
	struct xfs_mount	*i_mount;	
	struct xfs_dquot	*i_udquot;	
	struct xfs_dquot	*i_gdquot;	

	
	xfs_ino_t		i_ino;		
	struct xfs_imap		i_imap;		

	
	xfs_ifork_t		*i_afp;		
	xfs_ifork_t		i_df;		

	
	struct xfs_inode_log_item *i_itemp;	
	mrlock_t		i_lock;		
	mrlock_t		i_iolock;	
	atomic_t		i_pincount;	
	spinlock_t		i_flags_lock;	
	
	unsigned long		i_flags;	
	unsigned int		i_delayed_blks;	

	xfs_icdinode_t		i_d;		

	
	struct inode		i_vnode;	
} xfs_inode_t;

static inline struct xfs_inode *XFS_I(struct inode *inode)
{
	return container_of(inode, struct xfs_inode, i_vnode);
}

static inline struct inode *VFS_I(struct xfs_inode *ip)
{
	return &ip->i_vnode;
}

static inline xfs_fsize_t XFS_ISIZE(struct xfs_inode *ip)
{
	if (S_ISREG(ip->i_d.di_mode))
		return i_size_read(VFS_I(ip));
	return ip->i_d.di_size;
}

static inline xfs_fsize_t
xfs_new_eof(struct xfs_inode *ip, xfs_fsize_t new_size)
{
	xfs_fsize_t i_size = i_size_read(VFS_I(ip));

	if (new_size > i_size)
		new_size = i_size;
	return new_size > ip->i_d.di_size ? new_size : 0;
}

static inline void
__xfs_iflags_set(xfs_inode_t *ip, unsigned short flags)
{
	ip->i_flags |= flags;
}

static inline void
xfs_iflags_set(xfs_inode_t *ip, unsigned short flags)
{
	spin_lock(&ip->i_flags_lock);
	__xfs_iflags_set(ip, flags);
	spin_unlock(&ip->i_flags_lock);
}

static inline void
xfs_iflags_clear(xfs_inode_t *ip, unsigned short flags)
{
	spin_lock(&ip->i_flags_lock);
	ip->i_flags &= ~flags;
	spin_unlock(&ip->i_flags_lock);
}

static inline int
__xfs_iflags_test(xfs_inode_t *ip, unsigned short flags)
{
	return (ip->i_flags & flags);
}

static inline int
xfs_iflags_test(xfs_inode_t *ip, unsigned short flags)
{
	int ret;
	spin_lock(&ip->i_flags_lock);
	ret = __xfs_iflags_test(ip, flags);
	spin_unlock(&ip->i_flags_lock);
	return ret;
}

static inline int
xfs_iflags_test_and_clear(xfs_inode_t *ip, unsigned short flags)
{
	int ret;

	spin_lock(&ip->i_flags_lock);
	ret = ip->i_flags & flags;
	if (ret)
		ip->i_flags &= ~flags;
	spin_unlock(&ip->i_flags_lock);
	return ret;
}

static inline int
xfs_iflags_test_and_set(xfs_inode_t *ip, unsigned short flags)
{
	int ret;

	spin_lock(&ip->i_flags_lock);
	ret = ip->i_flags & flags;
	if (!ret)
		ip->i_flags |= flags;
	spin_unlock(&ip->i_flags_lock);
	return ret;
}

static inline prid_t
xfs_get_projid(struct xfs_inode *ip)
{
	return (prid_t)ip->i_d.di_projid_hi << 16 | ip->i_d.di_projid_lo;
}

static inline void
xfs_set_projid(struct xfs_inode *ip,
		prid_t projid)
{
	ip->i_d.di_projid_hi = (__uint16_t) (projid >> 16);
	ip->i_d.di_projid_lo = (__uint16_t) (projid & 0xffff);
}

#define XFS_IRECLAIM		(1 << 0) 
#define XFS_ISTALE		(1 << 1) 
#define XFS_IRECLAIMABLE	(1 << 2) 
#define XFS_INEW		(1 << 3) 
#define XFS_IFILESTREAM		(1 << 4) 
#define XFS_ITRUNCATED		(1 << 5) 
#define XFS_IDIRTY_RELEASE	(1 << 6) 
#define __XFS_IFLOCK_BIT	7	 
#define XFS_IFLOCK		(1 << __XFS_IFLOCK_BIT)
#define __XFS_IPINNED_BIT	8	 
#define XFS_IPINNED		(1 << __XFS_IPINNED_BIT)
#define XFS_IDONTCACHE		(1 << 9) 

#define XFS_IRECLAIM_RESET_FLAGS	\
	(XFS_IRECLAIMABLE | XFS_IRECLAIM | \
	 XFS_IDIRTY_RELEASE | XFS_ITRUNCATED | \
	 XFS_IFILESTREAM);


extern void __xfs_iflock(struct xfs_inode *ip);

static inline int xfs_iflock_nowait(struct xfs_inode *ip)
{
	return !xfs_iflags_test_and_set(ip, XFS_IFLOCK);
}

static inline void xfs_iflock(struct xfs_inode *ip)
{
	if (!xfs_iflock_nowait(ip))
		__xfs_iflock(ip);
}

static inline void xfs_ifunlock(struct xfs_inode *ip)
{
	xfs_iflags_clear(ip, XFS_IFLOCK);
	wake_up_bit(&ip->i_flags, __XFS_IFLOCK_BIT);
}

static inline int xfs_isiflocked(struct xfs_inode *ip)
{
	return xfs_iflags_test(ip, XFS_IFLOCK);
}

#define	XFS_IOLOCK_EXCL		(1<<0)
#define	XFS_IOLOCK_SHARED	(1<<1)
#define	XFS_ILOCK_EXCL		(1<<2)
#define	XFS_ILOCK_SHARED	(1<<3)

#define XFS_LOCK_MASK		(XFS_IOLOCK_EXCL | XFS_IOLOCK_SHARED \
				| XFS_ILOCK_EXCL | XFS_ILOCK_SHARED)

#define XFS_LOCK_FLAGS \
	{ XFS_IOLOCK_EXCL,	"IOLOCK_EXCL" }, \
	{ XFS_IOLOCK_SHARED,	"IOLOCK_SHARED" }, \
	{ XFS_ILOCK_EXCL,	"ILOCK_EXCL" }, \
	{ XFS_ILOCK_SHARED,	"ILOCK_SHARED" }


#define XFS_LOCK_PARENT		1
#define XFS_LOCK_RTBITMAP	2
#define XFS_LOCK_RTSUM		3
#define XFS_LOCK_INUMORDER	4

#define XFS_IOLOCK_SHIFT	16
#define	XFS_IOLOCK_PARENT	(XFS_LOCK_PARENT << XFS_IOLOCK_SHIFT)

#define XFS_ILOCK_SHIFT		24
#define	XFS_ILOCK_PARENT	(XFS_LOCK_PARENT << XFS_ILOCK_SHIFT)
#define	XFS_ILOCK_RTBITMAP	(XFS_LOCK_RTBITMAP << XFS_ILOCK_SHIFT)
#define	XFS_ILOCK_RTSUM		(XFS_LOCK_RTSUM << XFS_ILOCK_SHIFT)

#define XFS_IOLOCK_DEP_MASK	0x00ff0000
#define XFS_ILOCK_DEP_MASK	0xff000000
#define XFS_LOCK_DEP_MASK	(XFS_IOLOCK_DEP_MASK | XFS_ILOCK_DEP_MASK)

#define XFS_IOLOCK_DEP(flags)	(((flags) & XFS_IOLOCK_DEP_MASK) >> XFS_IOLOCK_SHIFT)
#define XFS_ILOCK_DEP(flags)	(((flags) & XFS_ILOCK_DEP_MASK) >> XFS_ILOCK_SHIFT)

extern struct lock_class_key xfs_iolock_reclaimable;

#define XFS_INHERIT_GID(pip)	\
	(((pip)->i_mount->m_flags & XFS_MOUNT_GRPID) || \
	 ((pip)->i_d.di_mode & S_ISGID))

int		xfs_iget(struct xfs_mount *, struct xfs_trans *, xfs_ino_t,
			 uint, uint, xfs_inode_t **);
void		xfs_ilock(xfs_inode_t *, uint);
int		xfs_ilock_nowait(xfs_inode_t *, uint);
void		xfs_iunlock(xfs_inode_t *, uint);
void		xfs_ilock_demote(xfs_inode_t *, uint);
int		xfs_isilocked(xfs_inode_t *, uint);
uint		xfs_ilock_map_shared(xfs_inode_t *);
void		xfs_iunlock_map_shared(xfs_inode_t *, uint);
void		xfs_inode_free(struct xfs_inode *ip);

int		xfs_ialloc(struct xfs_trans *, xfs_inode_t *, umode_t,
			   xfs_nlink_t, xfs_dev_t, prid_t, int,
			   struct xfs_buf **, boolean_t *, xfs_inode_t **);

uint		xfs_ip2xflags(struct xfs_inode *);
uint		xfs_dic2xflags(struct xfs_dinode *);
int		xfs_ifree(struct xfs_trans *, xfs_inode_t *,
			   struct xfs_bmap_free *);
int		xfs_itruncate_extents(struct xfs_trans **, struct xfs_inode *,
				      int, xfs_fsize_t);
int		xfs_iunlink(struct xfs_trans *, xfs_inode_t *);

void		xfs_iext_realloc(xfs_inode_t *, int, int);
void		xfs_iunpin_wait(xfs_inode_t *);
int		xfs_iflush(xfs_inode_t *, uint);
void		xfs_promote_inode(struct xfs_inode *);
void		xfs_lock_inodes(xfs_inode_t **, int, uint);
void		xfs_lock_two_inodes(xfs_inode_t *, xfs_inode_t *, uint);

#define IHOLD(ip) \
do { \
	ASSERT(atomic_read(&VFS_I(ip)->i_count) > 0) ; \
	ihold(VFS_I(ip)); \
	trace_xfs_ihold(ip, _THIS_IP_); \
} while (0)

#define IRELE(ip) \
do { \
	trace_xfs_irele(ip, _THIS_IP_); \
	iput(VFS_I(ip)); \
} while (0)

#endif 

#define XFS_IGET_CREATE		0x1
#define XFS_IGET_UNTRUSTED	0x2
#define XFS_IGET_DONTCACHE	0x4

int		xfs_inotobp(struct xfs_mount *, struct xfs_trans *,
			    xfs_ino_t, struct xfs_dinode **,
			    struct xfs_buf **, int *, uint);
int		xfs_itobp(struct xfs_mount *, struct xfs_trans *,
			  struct xfs_inode *, struct xfs_dinode **,
			  struct xfs_buf **, uint);
int		xfs_iread(struct xfs_mount *, struct xfs_trans *,
			  struct xfs_inode *, uint);
void		xfs_dinode_to_disk(struct xfs_dinode *,
				   struct xfs_icdinode *);
void		xfs_idestroy_fork(struct xfs_inode *, int);
void		xfs_idata_realloc(struct xfs_inode *, int, int);
void		xfs_iroot_realloc(struct xfs_inode *, int, int);
int		xfs_iread_extents(struct xfs_trans *, struct xfs_inode *, int);
int		xfs_iextents_copy(struct xfs_inode *, xfs_bmbt_rec_t *, int);

xfs_bmbt_rec_host_t *xfs_iext_get_ext(xfs_ifork_t *, xfs_extnum_t);
void		xfs_iext_insert(xfs_inode_t *, xfs_extnum_t, xfs_extnum_t,
				xfs_bmbt_irec_t *, int);
void		xfs_iext_add(xfs_ifork_t *, xfs_extnum_t, int);
void		xfs_iext_add_indirect_multi(xfs_ifork_t *, int, xfs_extnum_t, int);
void		xfs_iext_remove(xfs_inode_t *, xfs_extnum_t, int, int);
void		xfs_iext_remove_inline(xfs_ifork_t *, xfs_extnum_t, int);
void		xfs_iext_remove_direct(xfs_ifork_t *, xfs_extnum_t, int);
void		xfs_iext_remove_indirect(xfs_ifork_t *, xfs_extnum_t, int);
void		xfs_iext_realloc_direct(xfs_ifork_t *, int);
void		xfs_iext_direct_to_inline(xfs_ifork_t *, xfs_extnum_t);
void		xfs_iext_inline_to_direct(xfs_ifork_t *, int);
void		xfs_iext_destroy(xfs_ifork_t *);
xfs_bmbt_rec_host_t *xfs_iext_bno_to_ext(xfs_ifork_t *, xfs_fileoff_t, int *);
xfs_ext_irec_t	*xfs_iext_bno_to_irec(xfs_ifork_t *, xfs_fileoff_t, int *);
xfs_ext_irec_t	*xfs_iext_idx_to_irec(xfs_ifork_t *, xfs_extnum_t *, int *, int);
void		xfs_iext_irec_init(xfs_ifork_t *);
xfs_ext_irec_t *xfs_iext_irec_new(xfs_ifork_t *, int);
void		xfs_iext_irec_remove(xfs_ifork_t *, int);
void		xfs_iext_irec_compact(xfs_ifork_t *);
void		xfs_iext_irec_compact_pages(xfs_ifork_t *);
void		xfs_iext_irec_compact_full(xfs_ifork_t *);
void		xfs_iext_irec_update_extoffs(xfs_ifork_t *, int, int);

#define xfs_ipincount(ip)	((unsigned int) atomic_read(&ip->i_pincount))

#if defined(DEBUG)
void		xfs_inobp_check(struct xfs_mount *, struct xfs_buf *);
#else
#define	xfs_inobp_check(mp, bp)
#endif 

extern struct kmem_zone	*xfs_ifork_zone;
extern struct kmem_zone	*xfs_inode_zone;
extern struct kmem_zone	*xfs_ili_zone;

#endif	
