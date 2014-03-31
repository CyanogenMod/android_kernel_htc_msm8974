#ifndef _FS_CEPH_MDS_CLIENT_H
#define _FS_CEPH_MDS_CLIENT_H

#include <linux/completion.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/spinlock.h>

#include <linux/ceph/types.h>
#include <linux/ceph/messenger.h>
#include <linux/ceph/mdsmap.h>


struct ceph_fs_client;
struct ceph_cap;

struct ceph_mds_reply_info_in {
	struct ceph_mds_reply_inode *in;
	struct ceph_dir_layout dir_layout;
	u32 symlink_len;
	char *symlink;
	u32 xattr_len;
	char *xattr_data;
};

struct ceph_mds_reply_info_parsed {
	struct ceph_mds_reply_head    *head;

	
	struct ceph_mds_reply_info_in diri, targeti;
	struct ceph_mds_reply_dirfrag *dirfrag;
	char                          *dname;
	u32                           dname_len;
	struct ceph_mds_reply_lease   *dlease;

	
	union {
		
		struct ceph_filelock *filelock_reply;

		
		struct {
			struct ceph_mds_reply_dirfrag *dir_dir;
			int                           dir_nr;
			char                          **dir_dname;
			u32                           *dir_dname_len;
			struct ceph_mds_reply_lease   **dir_dlease;
			struct ceph_mds_reply_info_in *dir_in;
			u8                            dir_complete, dir_end;
		};
	};

	void *snapblob;
	int snapblob_len;
};


#define CEPH_CAPS_PER_RELEASE ((PAGE_CACHE_SIZE -			\
				sizeof(struct ceph_mds_cap_release)) /	\
			       sizeof(struct ceph_mds_cap_item))


enum {
	CEPH_MDS_SESSION_NEW = 1,
	CEPH_MDS_SESSION_OPENING = 2,
	CEPH_MDS_SESSION_OPEN = 3,
	CEPH_MDS_SESSION_HUNG = 4,
	CEPH_MDS_SESSION_CLOSING = 5,
	CEPH_MDS_SESSION_RESTARTING = 6,
	CEPH_MDS_SESSION_RECONNECTING = 7,
};

struct ceph_mds_session {
	struct ceph_mds_client *s_mdsc;
	int               s_mds;
	int               s_state;
	unsigned long     s_ttl;      
	u64               s_seq;      
	struct mutex      s_mutex;    

	struct ceph_connection s_con;

	struct ceph_authorizer *s_authorizer;
	void             *s_authorizer_buf, *s_authorizer_reply_buf;
	size_t            s_authorizer_buf_len, s_authorizer_reply_buf_len;

	
	spinlock_t        s_gen_ttl_lock;
	u32               s_cap_gen;  
	unsigned long     s_cap_ttl;  

	
	spinlock_t        s_cap_lock;
	struct list_head  s_caps;     
	int               s_nr_caps, s_trim_caps;
	int               s_num_cap_releases;
	struct list_head  s_cap_releases; 
	struct list_head  s_cap_releases_done; 
	struct ceph_cap  *s_cap_iterator;

	
	struct list_head  s_cap_flushing;     
	struct list_head  s_cap_snaps_flushing;
	unsigned long     s_renew_requested; 
	u64               s_renew_seq;

	atomic_t          s_ref;
	struct list_head  s_waiting;  
	struct list_head  s_unsafe;   
};

enum {
	USE_ANY_MDS,
	USE_RANDOM_MDS,
	USE_AUTH_MDS,   
};

struct ceph_mds_request;
struct ceph_mds_client;

typedef void (*ceph_mds_request_callback_t) (struct ceph_mds_client *mdsc,
					     struct ceph_mds_request *req);

struct ceph_mds_request {
	u64 r_tid;                   
	struct rb_node r_node;
	struct ceph_mds_client *r_mdsc;

	int r_op;                    

	
	struct inode *r_inode;              
	struct dentry *r_dentry;            
	struct dentry *r_old_dentry;        
	struct inode *r_old_dentry_dir;     
	char *r_path1, *r_path2;
	struct ceph_vino r_ino1, r_ino2;

	struct inode *r_locked_dir; 
	struct inode *r_target_inode;       

	struct mutex r_fill_mutex;

	union ceph_mds_request_args r_args;
	int r_fmode;        
	uid_t r_uid;
	gid_t r_gid;

	
	int r_direct_mode;
	u32 r_direct_hash;      
	bool r_direct_is_hash;  

	
	struct page **r_pages;
	int r_num_pages;
	int r_data_len;

	
	int r_inode_drop, r_inode_unless;
	int r_dentry_drop, r_dentry_unless;
	int r_old_dentry_drop, r_old_dentry_unless;
	struct inode *r_old_inode;
	int r_old_inode_drop, r_old_inode_unless;

	struct ceph_msg  *r_request;  
	int r_request_release_offset;
	struct ceph_msg  *r_reply;
	struct ceph_mds_reply_info_parsed r_reply_info;
	int r_err;
	bool r_aborted;

	unsigned long r_timeout;  
	unsigned long r_started;  
	unsigned long r_request_started; 

	
	struct inode	*r_unsafe_dir;
	struct list_head r_unsafe_dir_item;

	struct ceph_mds_session *r_session;

	int               r_attempts;   
	int               r_num_fwd;    
	int               r_resend_mds; 
	u32               r_sent_on_mseq; 

	struct kref       r_kref;
	struct list_head  r_wait;
	struct completion r_completion;
	struct completion r_safe_completion;
	ceph_mds_request_callback_t r_callback;
	struct list_head  r_unsafe_item;  
	bool		  r_got_unsafe, r_got_safe, r_got_result;

	bool              r_did_prepopulate;
	u32               r_readdir_offset;

	struct ceph_cap_reservation r_caps_reservation;
	int r_num_caps;
};

struct ceph_mds_client {
	struct ceph_fs_client  *fsc;
	struct mutex            mutex;         

	struct ceph_mdsmap      *mdsmap;
	struct completion       safe_umount_waiters;
	wait_queue_head_t       session_close_wq;
	struct list_head        waiting_for_map;

	struct ceph_mds_session **sessions;    
	int                     max_sessions;  
	int                     stopping;      

	struct rw_semaphore     snap_rwsem;
	struct rb_root          snap_realms;
	struct list_head        snap_empty;
	spinlock_t              snap_empty_lock;  

	u64                    last_tid;      
	struct rb_root         request_tree;  
	struct delayed_work    delayed_work;  
	unsigned long    last_renew_caps;  
	struct list_head cap_delay_list;   
	spinlock_t       cap_delay_lock;   
	struct list_head snap_flush_list;  
	spinlock_t       snap_flush_lock;

	u64               cap_flush_seq;
	struct list_head  cap_dirty;        
	struct list_head  cap_dirty_migrating; 
	int               num_cap_flushing; 
	spinlock_t        cap_dirty_lock;   
	wait_queue_head_t cap_flushing_wq;

	spinlock_t	caps_list_lock;
	struct		list_head caps_list; 
	int		caps_total_count;    
	int		caps_use_count;      
	int		caps_reserve_count;  
	int		caps_avail_count;    
	int		caps_min_count;      
	spinlock_t	  dentry_lru_lock;
	struct list_head  dentry_lru;
	int		  num_dentry;
};

extern const char *ceph_mds_op_name(int op);

extern struct ceph_mds_session *
__ceph_lookup_mds_session(struct ceph_mds_client *, int mds);

static inline struct ceph_mds_session *
ceph_get_mds_session(struct ceph_mds_session *s)
{
	atomic_inc(&s->s_ref);
	return s;
}

extern void ceph_put_mds_session(struct ceph_mds_session *s);

extern int ceph_send_msg_mds(struct ceph_mds_client *mdsc,
			     struct ceph_msg *msg, int mds);

extern int ceph_mdsc_init(struct ceph_fs_client *fsc);
extern void ceph_mdsc_close_sessions(struct ceph_mds_client *mdsc);
extern void ceph_mdsc_destroy(struct ceph_fs_client *fsc);

extern void ceph_mdsc_sync(struct ceph_mds_client *mdsc);

extern void ceph_mdsc_lease_release(struct ceph_mds_client *mdsc,
				    struct inode *inode,
				    struct dentry *dn);

extern void ceph_invalidate_dir_request(struct ceph_mds_request *req);

extern struct ceph_mds_request *
ceph_mdsc_create_request(struct ceph_mds_client *mdsc, int op, int mode);
extern void ceph_mdsc_submit_request(struct ceph_mds_client *mdsc,
				     struct ceph_mds_request *req);
extern int ceph_mdsc_do_request(struct ceph_mds_client *mdsc,
				struct inode *dir,
				struct ceph_mds_request *req);
static inline void ceph_mdsc_get_request(struct ceph_mds_request *req)
{
	kref_get(&req->r_kref);
}
extern void ceph_mdsc_release_request(struct kref *kref);
static inline void ceph_mdsc_put_request(struct ceph_mds_request *req)
{
	kref_put(&req->r_kref, ceph_mdsc_release_request);
}

extern int ceph_add_cap_releases(struct ceph_mds_client *mdsc,
				 struct ceph_mds_session *session);
extern void ceph_send_cap_releases(struct ceph_mds_client *mdsc,
				   struct ceph_mds_session *session);

extern void ceph_mdsc_pre_umount(struct ceph_mds_client *mdsc);

extern char *ceph_mdsc_build_path(struct dentry *dentry, int *plen, u64 *base,
				  int stop_on_nosnap);

extern void __ceph_mdsc_drop_dentry_lease(struct dentry *dentry);
extern void ceph_mdsc_lease_send_msg(struct ceph_mds_session *session,
				     struct inode *inode,
				     struct dentry *dentry, char action,
				     u32 seq);

extern void ceph_mdsc_handle_map(struct ceph_mds_client *mdsc,
				 struct ceph_msg *msg);

extern void ceph_mdsc_open_export_target_sessions(struct ceph_mds_client *mdsc,
					  struct ceph_mds_session *session);

#endif
