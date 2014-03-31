#ifndef _FS_CEPH_OSD_CLIENT_H
#define _FS_CEPH_OSD_CLIENT_H

#include <linux/completion.h>
#include <linux/kref.h>
#include <linux/mempool.h>
#include <linux/rbtree.h>

#include "types.h"
#include "osdmap.h"
#include "messenger.h"

#define MAX_OBJ_NAME_SIZE 100

struct ceph_msg;
struct ceph_snap_context;
struct ceph_osd_request;
struct ceph_osd_client;
struct ceph_authorizer;
struct ceph_pagelist;

typedef void (*ceph_osdc_callback_t)(struct ceph_osd_request *,
				     struct ceph_msg *);

struct ceph_osd {
	atomic_t o_ref;
	struct ceph_osd_client *o_osdc;
	int o_osd;
	int o_incarnation;
	struct rb_node o_node;
	struct ceph_connection o_con;
	struct list_head o_requests;
	struct list_head o_linger_requests;
	struct list_head o_osd_lru;
	struct ceph_authorizer *o_authorizer;
	void *o_authorizer_buf, *o_authorizer_reply_buf;
	size_t o_authorizer_buf_len, o_authorizer_reply_buf_len;
	unsigned long lru_ttl;
	int o_marked_for_keepalive;
	struct list_head o_keepalive_item;
};

struct ceph_osd_request {
	u64             r_tid;              
	struct rb_node  r_node;
	struct list_head r_req_lru_item;
	struct list_head r_osd_item;
	struct list_head r_linger_item;
	struct list_head r_linger_osd;
	struct ceph_osd *r_osd;
	struct ceph_pg   r_pgid;
	int              r_pg_osds[CEPH_PG_MAX_SIZE];
	int              r_num_pg_osds;

	struct ceph_connection *r_con_filling_msg;

	struct ceph_msg  *r_request, *r_reply;
	int               r_result;
	int               r_flags;     
	u32               r_sent;      
	int               r_got_reply;
	int		  r_linger;

	struct ceph_osd_client *r_osdc;
	struct kref       r_kref;
	bool              r_mempool;
	struct completion r_completion, r_safe_completion;
	ceph_osdc_callback_t r_callback, r_safe_callback;
	struct ceph_eversion r_reassert_version;
	struct list_head  r_unsafe_item;

	struct inode *r_inode;         	      
	void *r_priv;			      

	char              r_oid[MAX_OBJ_NAME_SIZE];          
	int               r_oid_len;
	unsigned long     r_stamp;            

	struct ceph_file_layout r_file_layout;
	struct ceph_snap_context *r_snapc;    
	unsigned          r_num_pages;        
	unsigned          r_page_alignment;   
	struct page     **r_pages;            
	int               r_pages_from_pool;
	int               r_own_pages;        
#ifdef CONFIG_BLOCK
	struct bio       *r_bio;	      
#endif

	struct ceph_pagelist *r_trail;	      
};

struct ceph_osd_event {
	u64 cookie;
	int one_shot;
	struct ceph_osd_client *osdc;
	void (*cb)(u64, u64, u8, void *);
	void *data;
	struct rb_node node;
	struct list_head osd_node;
	struct kref kref;
	struct completion completion;
};

struct ceph_osd_event_work {
	struct work_struct work;
	struct ceph_osd_event *event;
        u64 ver;
        u64 notify_id;
        u8 opcode;
};

struct ceph_osd_client {
	struct ceph_client     *client;

	struct ceph_osdmap     *osdmap;       
	struct rw_semaphore    map_sem;
	struct completion      map_waiters;
	u64                    last_requested_map;

	struct mutex           request_mutex;
	struct rb_root         osds;          
	struct list_head       osd_lru;       
	u64                    timeout_tid;   
	u64                    last_tid;      
	struct rb_root         requests;      
	struct list_head       req_lru;	      
	struct list_head       req_unsent;    
	struct list_head       req_notarget;  
	struct list_head       req_linger;    
	int                    num_requests;
	struct delayed_work    timeout_work;
	struct delayed_work    osds_timeout_work;
#ifdef CONFIG_DEBUG_FS
	struct dentry 	       *debugfs_file;
#endif

	mempool_t              *req_mempool;

	struct ceph_msgpool	msgpool_op;
	struct ceph_msgpool	msgpool_op_reply;

	spinlock_t		event_lock;
	struct rb_root		event_tree;
	u64			event_count;

	struct workqueue_struct	*notify_wq;
};

struct ceph_osd_req_op {
	u16 op;           
	u32 flags;        
	union {
		struct {
			u64 offset, length;
			u64 truncate_size;
			u32 truncate_seq;
		} extent;
		struct {
			const char *name;
			u32 name_len;
			const char  *val;
			u32 value_len;
			__u8 cmp_op;       
			__u8 cmp_mode;     
		} xattr;
		struct {
			const char *class_name;
			__u8 class_len;
			const char *method_name;
			__u8 method_len;
			__u8 argc;
			const char *indata;
			u32 indata_len;
		} cls;
		struct {
			u64 cookie, count;
		} pgls;
	        struct {
		        u64 snapid;
	        } snap;
		struct {
			u64 cookie;
			u64 ver;
			__u8 flag;
			u32 prot_ver;
			u32 timeout;
		} watch;
	};
	u32 payload_len;
};

extern int ceph_osdc_init(struct ceph_osd_client *osdc,
			  struct ceph_client *client);
extern void ceph_osdc_stop(struct ceph_osd_client *osdc);

extern void ceph_osdc_handle_reply(struct ceph_osd_client *osdc,
				   struct ceph_msg *msg);
extern void ceph_osdc_handle_map(struct ceph_osd_client *osdc,
				 struct ceph_msg *msg);

extern void ceph_calc_raw_layout(struct ceph_osd_client *osdc,
			struct ceph_file_layout *layout,
			u64 snapid,
			u64 off, u64 *plen, u64 *bno,
			struct ceph_osd_request *req,
			struct ceph_osd_req_op *op);

extern struct ceph_osd_request *ceph_osdc_alloc_request(struct ceph_osd_client *osdc,
					       int flags,
					       struct ceph_snap_context *snapc,
					       struct ceph_osd_req_op *ops,
					       bool use_mempool,
					       gfp_t gfp_flags,
					       struct page **pages,
					       struct bio *bio);

extern void ceph_osdc_build_request(struct ceph_osd_request *req,
				    u64 off, u64 *plen,
				    struct ceph_osd_req_op *src_ops,
				    struct ceph_snap_context *snapc,
				    struct timespec *mtime,
				    const char *oid,
				    int oid_len);

extern struct ceph_osd_request *ceph_osdc_new_request(struct ceph_osd_client *,
				      struct ceph_file_layout *layout,
				      struct ceph_vino vino,
				      u64 offset, u64 *len, int op, int flags,
				      struct ceph_snap_context *snapc,
				      int do_sync, u32 truncate_seq,
				      u64 truncate_size,
				      struct timespec *mtime,
				      bool use_mempool, int num_reply,
				      int page_align);

extern void ceph_osdc_set_request_linger(struct ceph_osd_client *osdc,
					 struct ceph_osd_request *req);
extern void ceph_osdc_unregister_linger_request(struct ceph_osd_client *osdc,
						struct ceph_osd_request *req);

static inline void ceph_osdc_get_request(struct ceph_osd_request *req)
{
	kref_get(&req->r_kref);
}
extern void ceph_osdc_release_request(struct kref *kref);
static inline void ceph_osdc_put_request(struct ceph_osd_request *req)
{
	kref_put(&req->r_kref, ceph_osdc_release_request);
}

extern int ceph_osdc_start_request(struct ceph_osd_client *osdc,
				   struct ceph_osd_request *req,
				   bool nofail);
extern int ceph_osdc_wait_request(struct ceph_osd_client *osdc,
				  struct ceph_osd_request *req);
extern void ceph_osdc_sync(struct ceph_osd_client *osdc);

extern int ceph_osdc_readpages(struct ceph_osd_client *osdc,
			       struct ceph_vino vino,
			       struct ceph_file_layout *layout,
			       u64 off, u64 *plen,
			       u32 truncate_seq, u64 truncate_size,
			       struct page **pages, int nr_pages,
			       int page_align);

extern int ceph_osdc_writepages(struct ceph_osd_client *osdc,
				struct ceph_vino vino,
				struct ceph_file_layout *layout,
				struct ceph_snap_context *sc,
				u64 off, u64 len,
				u32 truncate_seq, u64 truncate_size,
				struct timespec *mtime,
				struct page **pages, int nr_pages,
				int flags, int do_sync, bool nofail);

extern int ceph_osdc_create_event(struct ceph_osd_client *osdc,
				  void (*event_cb)(u64, u64, u8, void *),
				  int one_shot, void *data,
				  struct ceph_osd_event **pevent);
extern void ceph_osdc_cancel_event(struct ceph_osd_event *event);
extern int ceph_osdc_wait_event(struct ceph_osd_event *event,
				unsigned long timeout);
extern void ceph_osdc_put_event(struct ceph_osd_event *event);
#endif

