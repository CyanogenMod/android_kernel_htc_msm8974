#ifndef _FS_CEPH_MON_CLIENT_H
#define _FS_CEPH_MON_CLIENT_H

#include <linux/completion.h>
#include <linux/kref.h>
#include <linux/rbtree.h>

#include "messenger.h"

struct ceph_client;
struct ceph_mount_args;
struct ceph_auth_client;

struct ceph_monmap {
	struct ceph_fsid fsid;
	u32 epoch;
	u32 num_mon;
	struct ceph_entity_inst mon_inst[0];
};

struct ceph_mon_client;
struct ceph_mon_generic_request;


typedef void (*ceph_monc_request_func_t)(struct ceph_mon_client *monc,
					 int newmon);

struct ceph_mon_request {
	struct ceph_mon_client *monc;
	struct delayed_work delayed_work;
	unsigned long delay;
	ceph_monc_request_func_t do_request;
};

struct ceph_mon_generic_request {
	struct kref kref;
	u64 tid;
	struct rb_node node;
	int result;
	void *buf;
	int buf_len;
	struct completion completion;
	struct ceph_msg *request;  
	struct ceph_msg *reply;    
};

struct ceph_mon_client {
	struct ceph_client *client;
	struct ceph_monmap *monmap;

	struct mutex mutex;
	struct delayed_work delayed_work;

	struct ceph_auth_client *auth;
	struct ceph_msg *m_auth, *m_auth_reply, *m_subscribe, *m_subscribe_ack;
	int pending_auth;

	bool hunting;
	int cur_mon;                       
	unsigned long sub_sent, sub_renew_after;
	struct ceph_connection *con;
	bool have_fsid;

	
	struct rb_root generic_request_tree;
	int num_generic_requests;
	u64 last_tid;

	
	int want_mdsmap;
	int want_next_osdmap; 
	u32 have_osdmap, have_mdsmap;

#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_file;
#endif
};

extern struct ceph_monmap *ceph_monmap_decode(void *p, void *end);
extern int ceph_monmap_contains(struct ceph_monmap *m,
				struct ceph_entity_addr *addr);

extern int ceph_monc_init(struct ceph_mon_client *monc, struct ceph_client *cl);
extern void ceph_monc_stop(struct ceph_mon_client *monc);

extern int ceph_monc_got_mdsmap(struct ceph_mon_client *monc, u32 have);
extern int ceph_monc_got_osdmap(struct ceph_mon_client *monc, u32 have);

extern void ceph_monc_request_next_osdmap(struct ceph_mon_client *monc);

extern int ceph_monc_do_statfs(struct ceph_mon_client *monc,
			       struct ceph_statfs *buf);

extern int ceph_monc_open_session(struct ceph_mon_client *monc);

extern int ceph_monc_validate_auth(struct ceph_mon_client *monc);

extern int ceph_monc_create_snapid(struct ceph_mon_client *monc,
				   u32 pool, u64 *snapid);

extern int ceph_monc_delete_snapid(struct ceph_mon_client *monc,
				   u32 pool, u64 snapid);

#endif
