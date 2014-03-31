#ifndef _FS_CEPH_AUTH_NONE_H
#define _FS_CEPH_AUTH_NONE_H

#include <linux/slab.h>
#include <linux/ceph/auth.h>


struct ceph_none_authorizer {
	char buf[128];
	int buf_len;
	char reply_buf[0];
};

struct ceph_auth_none_info {
	bool starting;
	bool built_authorizer;
	struct ceph_none_authorizer au;   
};

extern int ceph_auth_none_init(struct ceph_auth_client *ac);

#endif

