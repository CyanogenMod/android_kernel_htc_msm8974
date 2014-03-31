#ifndef _FS_CEPH_AUTH_H
#define _FS_CEPH_AUTH_H

#include <linux/ceph/types.h>
#include <linux/ceph/buffer.h>


struct ceph_auth_client;
struct ceph_authorizer;

struct ceph_auth_client_ops {
	const char *name;

	int (*is_authenticated)(struct ceph_auth_client *ac);

	int (*should_authenticate)(struct ceph_auth_client *ac);

	int (*build_request)(struct ceph_auth_client *ac, void *buf, void *end);
	int (*handle_reply)(struct ceph_auth_client *ac, int result,
			    void *buf, void *end);

	int (*create_authorizer)(struct ceph_auth_client *ac, int peer_type,
				 struct ceph_authorizer **a,
				 void **buf, size_t *len,
				 void **reply_buf, size_t *reply_len);
	int (*verify_authorizer_reply)(struct ceph_auth_client *ac,
				       struct ceph_authorizer *a, size_t len);
	void (*destroy_authorizer)(struct ceph_auth_client *ac,
				   struct ceph_authorizer *a);
	void (*invalidate_authorizer)(struct ceph_auth_client *ac,
				      int peer_type);

	
	void (*reset)(struct ceph_auth_client *ac);

	void (*destroy)(struct ceph_auth_client *ac);
};

struct ceph_auth_client {
	u32 protocol;           
	void *private;          
	const struct ceph_auth_client_ops *ops;  

	bool negotiating;       
	const char *name;       
	u64 global_id;          
	const struct ceph_crypto_key *key;     
	unsigned want_keys;     
};

extern struct ceph_auth_client *ceph_auth_init(const char *name,
					       const struct ceph_crypto_key *key);
extern void ceph_auth_destroy(struct ceph_auth_client *ac);

extern void ceph_auth_reset(struct ceph_auth_client *ac);

extern int ceph_auth_build_hello(struct ceph_auth_client *ac,
				 void *buf, size_t len);
extern int ceph_handle_auth_reply(struct ceph_auth_client *ac,
				  void *buf, size_t len,
				  void *reply_buf, size_t reply_len);
extern int ceph_entity_name_encode(const char *name, void **p, void *end);

extern int ceph_build_auth(struct ceph_auth_client *ac,
		    void *msg_buf, size_t msg_len);

extern int ceph_auth_is_authenticated(struct ceph_auth_client *ac);

#endif
