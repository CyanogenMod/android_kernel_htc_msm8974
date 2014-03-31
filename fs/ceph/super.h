#ifndef _FS_CEPH_SUPER_H
#define _FS_CEPH_SUPER_H

#include <linux/ceph/ceph_debug.h>

#include <asm/unaligned.h>
#include <linux/backing-dev.h>
#include <linux/completion.h>
#include <linux/exportfs.h>
#include <linux/fs.h>
#include <linux/mempool.h>
#include <linux/pagemap.h>
#include <linux/wait.h>
#include <linux/writeback.h>
#include <linux/slab.h>

#include <linux/ceph/libceph.h>

#define CEPH_SUPER_MAGIC 0x00c36400

#define CEPH_BLOCK_SHIFT   20  
#define CEPH_BLOCK         (1 << CEPH_BLOCK_SHIFT)

#define CEPH_MOUNT_OPT_DIRSTAT         (1<<4) 
#define CEPH_MOUNT_OPT_RBYTES          (1<<5) 
#define CEPH_MOUNT_OPT_NOASYNCREADDIR  (1<<7) 
#define CEPH_MOUNT_OPT_INO32           (1<<8) 
#define CEPH_MOUNT_OPT_DCACHE          (1<<9) 

#define CEPH_MOUNT_OPT_DEFAULT    (CEPH_MOUNT_OPT_RBYTES)

#define ceph_set_mount_opt(fsc, opt) \
	(fsc)->mount_options->flags |= CEPH_MOUNT_OPT_##opt;
#define ceph_test_mount_opt(fsc, opt) \
	(!!((fsc)->mount_options->flags & CEPH_MOUNT_OPT_##opt))

#define CEPH_RSIZE_DEFAULT             0           
#define CEPH_RASIZE_DEFAULT            (8192*1024) 
#define CEPH_MAX_READDIR_DEFAULT        1024
#define CEPH_MAX_READDIR_BYTES_DEFAULT  (512*1024)
#define CEPH_SNAPDIRNAME_DEFAULT        ".snap"

struct ceph_mount_options {
	int flags;
	int sb_flags;

	int wsize;            
	int rsize;            
	int rasize;           
	int congestion_kb;    
	int caps_wanted_delay_min, caps_wanted_delay_max;
	int cap_release_safety;
	int max_readdir;       
	int max_readdir_bytes; 


	char *snapdir_name;   
};

struct ceph_fs_client {
	struct super_block *sb;

	struct ceph_mount_options *mount_options;
	struct ceph_client *client;

	unsigned long mount_state;
	int min_caps;                  

	struct ceph_mds_client *mdsc;

	
	mempool_t *wb_pagevec_pool;
	struct workqueue_struct *wb_wq;
	struct workqueue_struct *pg_inv_wq;
	struct workqueue_struct *trunc_wq;
	atomic_long_t writeback_count;

	struct backing_dev_info backing_dev_info;

#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_dentry_lru, *debugfs_caps;
	struct dentry *debugfs_congestion_kb;
	struct dentry *debugfs_bdi;
	struct dentry *debugfs_mdsc, *debugfs_mdsmap;
#endif
};


struct ceph_cap {
	struct ceph_inode_info *ci;
	struct rb_node ci_node;          
	struct ceph_mds_session *session;
	struct list_head session_caps;   
	int mds;
	u64 cap_id;       
	int issued;       
	int implemented;  
	int mds_wanted;
	u32 seq, issue_seq, mseq;
	u32 cap_gen;      
	unsigned long last_used;
	struct list_head caps_item;
};

#define CHECK_CAPS_NODELAY    1  
#define CHECK_CAPS_AUTHONLY   2  
#define CHECK_CAPS_FLUSH      4  

struct ceph_cap_snap {
	atomic_t nref;
	struct ceph_inode_info *ci;
	struct list_head ci_item, flushing_item;

	u64 follows, flush_tid;
	int issued, dirty;
	struct ceph_snap_context *context;

	umode_t mode;
	uid_t uid;
	gid_t gid;

	struct ceph_buffer *xattr_blob;
	u64 xattr_version;

	u64 size;
	struct timespec mtime, atime, ctime;
	u64 time_warp_seq;
	int writing;   
	int dirty_pages;     
};

static inline void ceph_put_cap_snap(struct ceph_cap_snap *capsnap)
{
	if (atomic_dec_and_test(&capsnap->nref)) {
		if (capsnap->xattr_blob)
			ceph_buffer_put(capsnap->xattr_blob);
		kfree(capsnap);
	}
}

#define CEPH_MAX_DIRFRAG_REP 4

struct ceph_inode_frag {
	struct rb_node node;

	
	u32 frag;
	int split_by;         

	
	int mds;              
	int ndist;            
	int dist[CEPH_MAX_DIRFRAG_REP];
};

struct ceph_inode_xattr {
	struct rb_node node;

	const char *name;
	int name_len;
	const char *val;
	int val_len;
	int dirty;

	int should_free_name;
	int should_free_val;
};

struct ceph_dentry_info {
	unsigned long flags;
	struct ceph_mds_session *lease_session;
	u32 lease_gen, lease_shared_gen;
	u32 lease_seq;
	unsigned long lease_renew_after, lease_renew_from;
	struct list_head lru;
	struct dentry *dentry;
	u64 time;
	u64 offset;
};

#define CEPH_D_COMPLETE 1  

struct ceph_inode_xattrs_info {
	struct ceph_buffer *blob, *prealloc_blob;

	struct rb_root index;
	bool dirty;
	int count;
	int names_size;
	int vals_size;
	u64 version, index_version;
};

struct ceph_inode_info {
	struct ceph_vino i_vino;   

	spinlock_t i_ceph_lock;

	u64 i_version;
	u32 i_time_warp_seq;

	unsigned i_ceph_flags;
	unsigned long i_release_count;

	struct ceph_dir_layout i_dir_layout;
	struct ceph_file_layout i_layout;
	char *i_symlink;

	
	struct timespec i_rctime;
	u64 i_rbytes, i_rfiles, i_rsubdirs;
	u64 i_files, i_subdirs;
	u64 i_max_offset;  

	struct rb_root i_fragtree;
	struct mutex i_fragtree_mutex;

	struct ceph_inode_xattrs_info i_xattrs;

	struct rb_root i_caps;           
	struct ceph_cap *i_auth_cap;     
	unsigned i_dirty_caps, i_flushing_caps;     
	struct list_head i_dirty_item, i_flushing_item;
	u64 i_cap_flush_seq;
	u16 i_cap_flush_last_tid, i_cap_flush_tid[CEPH_CAP_BITS];
	wait_queue_head_t i_cap_wq;      
	unsigned long i_hold_caps_min; 
	unsigned long i_hold_caps_max; 
	struct list_head i_cap_delay_list;  
	int i_cap_exporting_mds;         
	unsigned i_cap_exporting_mseq;   
	unsigned i_cap_exporting_issued;
	struct ceph_cap_reservation i_cap_migration_resv;
	struct list_head i_cap_snaps;   
	struct ceph_snap_context *i_head_snapc;  
	unsigned i_snap_caps;           

	int i_nr_by_mode[CEPH_FILE_MODE_NUM];  

	u32 i_truncate_seq;        
	u64 i_truncate_size;       
	int i_truncate_pending;    

	u64 i_max_size;            
	u64 i_reported_size; 
	u64 i_wanted_max_size;     
	u64 i_requested_max_size;  

	
	int i_pin_ref;
	int i_rd_ref, i_rdcache_ref, i_wr_ref, i_wb_ref;
	int i_wrbuffer_ref, i_wrbuffer_ref_head;
	u32 i_shared_gen;       
	u32 i_rdcache_gen;      
	u32 i_rdcache_revoking; 

	struct list_head i_unsafe_writes; 
	struct list_head i_unsafe_dirops; 
	spinlock_t i_unsafe_lock;

	struct ceph_snap_realm *i_snap_realm; 
	int i_snap_realm_counter; 
	struct list_head i_snap_realm_item;
	struct list_head i_snap_flush_item;

	struct work_struct i_wb_work;  
	struct work_struct i_pg_inv_work;  

	struct work_struct i_vmtruncate_work;

	struct inode vfs_inode; 
};

static inline struct ceph_inode_info *ceph_inode(struct inode *inode)
{
	return container_of(inode, struct ceph_inode_info, vfs_inode);
}

static inline struct ceph_fs_client *ceph_inode_to_client(struct inode *inode)
{
	return (struct ceph_fs_client *)inode->i_sb->s_fs_info;
}

static inline struct ceph_fs_client *ceph_sb_to_client(struct super_block *sb)
{
	return (struct ceph_fs_client *)sb->s_fs_info;
}

static inline struct ceph_vino ceph_vino(struct inode *inode)
{
	return ceph_inode(inode)->i_vino;
}

static inline u32 ceph_ino_to_ino32(__u64 vino)
{
	u32 ino = vino & 0xffffffff;
	ino ^= vino >> 32;
	if (!ino)
		ino = 2;
	return ino;
}

static inline ino_t ceph_vino_to_ino(struct ceph_vino vino)
{
#if BITS_PER_LONG == 32
	return ceph_ino_to_ino32(vino.ino);
#else
	return (ino_t)vino.ino;
#endif
}

#if BITS_PER_LONG == 32
static inline ino_t ceph_translate_ino(struct super_block *sb, ino_t ino)
{
	return ino;
}
#else
static inline ino_t ceph_translate_ino(struct super_block *sb, ino_t ino)
{
	if (ceph_test_mount_opt(ceph_sb_to_client(sb), INO32))
		ino = ceph_ino_to_ino32(ino);
	return ino;
}
#endif


#define ceph_vinop(i) ceph_inode(i)->i_vino.ino, ceph_inode(i)->i_vino.snap

static inline u64 ceph_ino(struct inode *inode)
{
	return ceph_inode(inode)->i_vino.ino;
}
static inline u64 ceph_snap(struct inode *inode)
{
	return ceph_inode(inode)->i_vino.snap;
}

static inline int ceph_ino_compare(struct inode *inode, void *data)
{
	struct ceph_vino *pvino = (struct ceph_vino *)data;
	struct ceph_inode_info *ci = ceph_inode(inode);
	return ci->i_vino.ino == pvino->ino &&
		ci->i_vino.snap == pvino->snap;
}

static inline struct inode *ceph_find_inode(struct super_block *sb,
					    struct ceph_vino vino)
{
	ino_t t = ceph_vino_to_ino(vino);
	return ilookup5(sb, t, ceph_ino_compare, &vino);
}


#define CEPH_I_NODELAY   4  
#define CEPH_I_FLUSH     8  
#define CEPH_I_NOFLUSH  16  

static inline void ceph_i_clear(struct inode *inode, unsigned mask)
{
	struct ceph_inode_info *ci = ceph_inode(inode);

	spin_lock(&ci->i_ceph_lock);
	ci->i_ceph_flags &= ~mask;
	spin_unlock(&ci->i_ceph_lock);
}

static inline void ceph_i_set(struct inode *inode, unsigned mask)
{
	struct ceph_inode_info *ci = ceph_inode(inode);

	spin_lock(&ci->i_ceph_lock);
	ci->i_ceph_flags |= mask;
	spin_unlock(&ci->i_ceph_lock);
}

static inline bool ceph_i_test(struct inode *inode, unsigned mask)
{
	struct ceph_inode_info *ci = ceph_inode(inode);
	bool r;

	spin_lock(&ci->i_ceph_lock);
	r = (ci->i_ceph_flags & mask) == mask;
	spin_unlock(&ci->i_ceph_lock);
	return r;
}


extern struct ceph_inode_frag *__ceph_find_frag(struct ceph_inode_info *ci,
						u32 f);

extern u32 ceph_choose_frag(struct ceph_inode_info *ci, u32 v,
			    struct ceph_inode_frag *pfrag,
			    int *found);

static inline struct ceph_dentry_info *ceph_dentry(struct dentry *dentry)
{
	return (struct ceph_dentry_info *)dentry->d_fsdata;
}

static inline loff_t ceph_make_fpos(unsigned frag, unsigned off)
{
	return ((loff_t)frag << 32) | (loff_t)off;
}

void ceph_dir_set_complete(struct inode *inode);
void ceph_dir_clear_complete(struct inode *inode);
bool ceph_dir_test_complete(struct inode *inode);

static inline bool __ceph_is_any_real_caps(struct ceph_inode_info *ci)
{
	return !RB_EMPTY_ROOT(&ci->i_caps);
}

extern int __ceph_caps_issued(struct ceph_inode_info *ci, int *implemented);
extern int __ceph_caps_issued_mask(struct ceph_inode_info *ci, int mask, int t);
extern int __ceph_caps_issued_other(struct ceph_inode_info *ci,
				    struct ceph_cap *cap);

static inline int ceph_caps_issued(struct ceph_inode_info *ci)
{
	int issued;
	spin_lock(&ci->i_ceph_lock);
	issued = __ceph_caps_issued(ci, NULL);
	spin_unlock(&ci->i_ceph_lock);
	return issued;
}

static inline int ceph_caps_issued_mask(struct ceph_inode_info *ci, int mask,
					int touch)
{
	int r;
	spin_lock(&ci->i_ceph_lock);
	r = __ceph_caps_issued_mask(ci, mask, touch);
	spin_unlock(&ci->i_ceph_lock);
	return r;
}

static inline int __ceph_caps_dirty(struct ceph_inode_info *ci)
{
	return ci->i_dirty_caps | ci->i_flushing_caps;
}
extern int __ceph_mark_dirty_caps(struct ceph_inode_info *ci, int mask);

extern int ceph_caps_revoking(struct ceph_inode_info *ci, int mask);
extern int __ceph_caps_used(struct ceph_inode_info *ci);

extern int __ceph_caps_file_wanted(struct ceph_inode_info *ci);

static inline int __ceph_caps_wanted(struct ceph_inode_info *ci)
{
	int w = __ceph_caps_file_wanted(ci) | __ceph_caps_used(ci);
	if (w & CEPH_CAP_FILE_BUFFER)
		w |= CEPH_CAP_FILE_EXCL;  
	return w;
}

extern int __ceph_caps_mds_wanted(struct ceph_inode_info *ci);

extern void ceph_caps_init(struct ceph_mds_client *mdsc);
extern void ceph_caps_finalize(struct ceph_mds_client *mdsc);
extern void ceph_adjust_min_caps(struct ceph_mds_client *mdsc, int delta);
extern int ceph_reserve_caps(struct ceph_mds_client *mdsc,
			     struct ceph_cap_reservation *ctx, int need);
extern int ceph_unreserve_caps(struct ceph_mds_client *mdsc,
			       struct ceph_cap_reservation *ctx);
extern void ceph_reservation_status(struct ceph_fs_client *client,
				    int *total, int *avail, int *used,
				    int *reserved, int *min);



#define CEPH_F_SYNC     1
#define CEPH_F_ATEND    2

struct ceph_file_info {
	short fmode;     
	short flags;     

	
	u32 frag;
	struct ceph_mds_request *last_readdir;

	
	unsigned offset;       
	u64 next_offset;       
	char *last_name;       
	struct dentry *dentry; 
	unsigned long dir_release_count;

	
	char *dir_info;
	int dir_info_len;
};



struct ceph_snap_realm {
	u64 ino;
	atomic_t nref;
	struct rb_node node;

	u64 created, seq;
	u64 parent_ino;
	u64 parent_since;   

	u64 *prior_parent_snaps;      
	int num_prior_parent_snaps;   
	u64 *snaps;                   
	int num_snaps;

	struct ceph_snap_realm *parent;
	struct list_head children;       
	struct list_head child_item;

	struct list_head empty_item;     

	struct list_head dirty_item;     

	
	struct ceph_snap_context *cached_context;

	struct list_head inodes_with_caps;
	spinlock_t inodes_with_caps_lock;
};

static inline int default_congestion_kb(void)
{
	int congestion_kb;

	congestion_kb = (16*int_sqrt(totalram_pages)) << (PAGE_SHIFT-10);
	if (congestion_kb > 256*1024)
		congestion_kb = 256*1024;

	return congestion_kb;
}



struct ceph_snap_realm *ceph_lookup_snap_realm(struct ceph_mds_client *mdsc,
					       u64 ino);
extern void ceph_get_snap_realm(struct ceph_mds_client *mdsc,
				struct ceph_snap_realm *realm);
extern void ceph_put_snap_realm(struct ceph_mds_client *mdsc,
				struct ceph_snap_realm *realm);
extern int ceph_update_snap_trace(struct ceph_mds_client *m,
				  void *p, void *e, bool deletion);
extern void ceph_handle_snap(struct ceph_mds_client *mdsc,
			     struct ceph_mds_session *session,
			     struct ceph_msg *msg);
extern void ceph_queue_cap_snap(struct ceph_inode_info *ci);
extern int __ceph_finish_cap_snap(struct ceph_inode_info *ci,
				  struct ceph_cap_snap *capsnap);
extern void ceph_cleanup_empty_realms(struct ceph_mds_client *mdsc);

static inline bool __ceph_have_pending_cap_snap(struct ceph_inode_info *ci)
{
	return !list_empty(&ci->i_cap_snaps) &&
		list_entry(ci->i_cap_snaps.prev, struct ceph_cap_snap,
			   ci_item)->writing;
}

extern const struct inode_operations ceph_file_iops;

extern struct inode *ceph_alloc_inode(struct super_block *sb);
extern void ceph_destroy_inode(struct inode *inode);

extern struct inode *ceph_get_inode(struct super_block *sb,
				    struct ceph_vino vino);
extern struct inode *ceph_get_snapdir(struct inode *parent);
extern int ceph_fill_file_size(struct inode *inode, int issued,
			       u32 truncate_seq, u64 truncate_size, u64 size);
extern void ceph_fill_file_time(struct inode *inode, int issued,
				u64 time_warp_seq, struct timespec *ctime,
				struct timespec *mtime, struct timespec *atime);
extern int ceph_fill_trace(struct super_block *sb,
			   struct ceph_mds_request *req,
			   struct ceph_mds_session *session);
extern int ceph_readdir_prepopulate(struct ceph_mds_request *req,
				    struct ceph_mds_session *session);

extern int ceph_inode_holds_cap(struct inode *inode, int mask);

extern int ceph_inode_set_size(struct inode *inode, loff_t size);
extern void __ceph_do_pending_vmtruncate(struct inode *inode);
extern void ceph_queue_vmtruncate(struct inode *inode);

extern void ceph_queue_invalidate(struct inode *inode);
extern void ceph_queue_writeback(struct inode *inode);

extern int ceph_do_getattr(struct inode *inode, int mask);
extern int ceph_permission(struct inode *inode, int mask);
extern int ceph_setattr(struct dentry *dentry, struct iattr *attr);
extern int ceph_getattr(struct vfsmount *mnt, struct dentry *dentry,
			struct kstat *stat);

extern int ceph_setxattr(struct dentry *, const char *, const void *,
			 size_t, int);
extern ssize_t ceph_getxattr(struct dentry *, const char *, void *, size_t);
extern ssize_t ceph_listxattr(struct dentry *, char *, size_t);
extern int ceph_removexattr(struct dentry *, const char *);
extern void __ceph_build_xattrs_blob(struct ceph_inode_info *ci);
extern void __ceph_destroy_xattrs(struct ceph_inode_info *ci);
extern void __init ceph_xattr_init(void);
extern void ceph_xattr_exit(void);

extern const char *ceph_cap_string(int c);
extern void ceph_handle_caps(struct ceph_mds_session *session,
			     struct ceph_msg *msg);
extern int ceph_add_cap(struct inode *inode,
			struct ceph_mds_session *session, u64 cap_id,
			int fmode, unsigned issued, unsigned wanted,
			unsigned cap, unsigned seq, u64 realmino, int flags,
			struct ceph_cap_reservation *caps_reservation);
extern void __ceph_remove_cap(struct ceph_cap *cap);
static inline void ceph_remove_cap(struct ceph_cap *cap)
{
	spin_lock(&cap->ci->i_ceph_lock);
	__ceph_remove_cap(cap);
	spin_unlock(&cap->ci->i_ceph_lock);
}
extern void ceph_put_cap(struct ceph_mds_client *mdsc,
			 struct ceph_cap *cap);

extern void ceph_queue_caps_release(struct inode *inode);
extern int ceph_write_inode(struct inode *inode, struct writeback_control *wbc);
extern int ceph_fsync(struct file *file, loff_t start, loff_t end,
		      int datasync);
extern void ceph_kick_flushing_caps(struct ceph_mds_client *mdsc,
				    struct ceph_mds_session *session);
extern struct ceph_cap *ceph_get_cap_for_mds(struct ceph_inode_info *ci,
					     int mds);
extern int ceph_get_cap_mds(struct inode *inode);
extern void ceph_get_cap_refs(struct ceph_inode_info *ci, int caps);
extern void ceph_put_cap_refs(struct ceph_inode_info *ci, int had);
extern void ceph_put_wrbuffer_cap_refs(struct ceph_inode_info *ci, int nr,
				       struct ceph_snap_context *snapc);
extern void __ceph_flush_snaps(struct ceph_inode_info *ci,
			       struct ceph_mds_session **psession,
			       int again);
extern void ceph_check_caps(struct ceph_inode_info *ci, int flags,
			    struct ceph_mds_session *session);
extern void ceph_check_delayed_caps(struct ceph_mds_client *mdsc);
extern void ceph_flush_dirty_caps(struct ceph_mds_client *mdsc);

extern int ceph_encode_inode_release(void **p, struct inode *inode,
				     int mds, int drop, int unless, int force);
extern int ceph_encode_dentry_release(void **p, struct dentry *dn,
				      int mds, int drop, int unless);

extern int ceph_get_caps(struct ceph_inode_info *ci, int need, int want,
			 int *got, loff_t endoff);

static inline void __ceph_get_fmode(struct ceph_inode_info *ci, int mode)
{
	ci->i_nr_by_mode[mode]++;
}
extern void ceph_put_fmode(struct ceph_inode_info *ci, int mode);

extern const struct address_space_operations ceph_aops;
extern int ceph_mmap(struct file *file, struct vm_area_struct *vma);

extern const struct file_operations ceph_file_fops;
extern const struct address_space_operations ceph_aops;
extern int ceph_copy_to_page_vector(struct page **pages,
				    const char *data,
				    loff_t off, size_t len);
extern int ceph_copy_from_page_vector(struct page **pages,
				    char *data,
				    loff_t off, size_t len);
extern struct page **ceph_alloc_page_vector(int num_pages, gfp_t flags);
extern int ceph_open(struct inode *inode, struct file *file);
extern struct dentry *ceph_lookup_open(struct inode *dir, struct dentry *dentry,
				       struct nameidata *nd, int mode,
				       int locked_dir);
extern int ceph_release(struct inode *inode, struct file *filp);

extern const struct file_operations ceph_dir_fops;
extern const struct inode_operations ceph_dir_iops;
extern const struct dentry_operations ceph_dentry_ops, ceph_snap_dentry_ops,
	ceph_snapdir_dentry_ops;

extern int ceph_handle_notrace_create(struct inode *dir, struct dentry *dentry);
extern int ceph_handle_snapdir(struct ceph_mds_request *req,
			       struct dentry *dentry, int err);
extern struct dentry *ceph_finish_lookup(struct ceph_mds_request *req,
					 struct dentry *dentry, int err);

extern void ceph_dentry_lru_add(struct dentry *dn);
extern void ceph_dentry_lru_touch(struct dentry *dn);
extern void ceph_dentry_lru_del(struct dentry *dn);
extern void ceph_invalidate_dentry_lease(struct dentry *dentry);
extern unsigned ceph_dentry_hash(struct inode *dir, struct dentry *dn);
extern struct inode *ceph_get_dentry_parent_inode(struct dentry *dentry);

int ceph_init_dentry(struct dentry *dentry);


extern long ceph_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

extern const struct export_operations ceph_export_ops;

extern int ceph_lock(struct file *file, int cmd, struct file_lock *fl);
extern int ceph_flock(struct file *file, int cmd, struct file_lock *fl);
extern void ceph_count_locks(struct inode *inode, int *p_num, int *f_num);
extern int ceph_encode_locks(struct inode *i, struct ceph_pagelist *p,
			     int p_locks, int f_locks);
extern int lock_to_ceph_filelock(struct file_lock *fl, struct ceph_filelock *c);

extern int ceph_fs_debugfs_init(struct ceph_fs_client *client);
extern void ceph_fs_debugfs_cleanup(struct ceph_fs_client *client);

#endif 
