#ifndef _FS_CEPH_MDSMAP_H
#define _FS_CEPH_MDSMAP_H

#include <linux/bug.h>
#include "types.h"

struct ceph_mds_info {
	u64 global_id;
	struct ceph_entity_addr addr;
	s32 state;
	int num_export_targets;
	bool laggy;
	u32 *export_targets;
};

struct ceph_mdsmap {
	u32 m_epoch, m_client_epoch, m_last_failure;
	u32 m_root;
	u32 m_session_timeout;          
	u32 m_session_autoclose;        
	u64 m_max_file_size;
	u32 m_max_mds;                  
	struct ceph_mds_info *m_info;

	
	int m_num_data_pg_pools;
	u32 *m_data_pg_pools;
	u32 m_cas_pg_pool;
};

static inline struct ceph_entity_addr *
ceph_mdsmap_get_addr(struct ceph_mdsmap *m, int w)
{
	if (w >= m->m_max_mds)
		return NULL;
	return &m->m_info[w].addr;
}

static inline int ceph_mdsmap_get_state(struct ceph_mdsmap *m, int w)
{
	BUG_ON(w < 0);
	if (w >= m->m_max_mds)
		return CEPH_MDS_STATE_DNE;
	return m->m_info[w].state;
}

static inline bool ceph_mdsmap_is_laggy(struct ceph_mdsmap *m, int w)
{
	if (w >= 0 && w < m->m_max_mds)
		return m->m_info[w].laggy;
	return false;
}

extern int ceph_mdsmap_get_random_mds(struct ceph_mdsmap *m);
extern struct ceph_mdsmap *ceph_mdsmap_decode(void **p, void *end);
extern void ceph_mdsmap_destroy(struct ceph_mdsmap *m);

#endif
