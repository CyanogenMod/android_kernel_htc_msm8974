/*
 * zfcp device driver
 *
 * Data structure and helper functions for tracking pending FSF
 * requests.
 *
 * Copyright IBM Corporation 2009
 */

#ifndef ZFCP_REQLIST_H
#define ZFCP_REQLIST_H

#define ZFCP_REQ_LIST_BUCKETS 128

struct zfcp_reqlist {
	spinlock_t lock;
	struct list_head buckets[ZFCP_REQ_LIST_BUCKETS];
};

static inline int zfcp_reqlist_hash(unsigned long req_id)
{
	return req_id % ZFCP_REQ_LIST_BUCKETS;
}

static inline struct zfcp_reqlist *zfcp_reqlist_alloc(void)
{
	unsigned int i;
	struct zfcp_reqlist *rl;

	rl = kzalloc(sizeof(struct zfcp_reqlist), GFP_KERNEL);
	if (!rl)
		return NULL;

	spin_lock_init(&rl->lock);

	for (i = 0; i < ZFCP_REQ_LIST_BUCKETS; i++)
		INIT_LIST_HEAD(&rl->buckets[i]);

	return rl;
}

static inline int zfcp_reqlist_isempty(struct zfcp_reqlist *rl)
{
	unsigned int i;

	for (i = 0; i < ZFCP_REQ_LIST_BUCKETS; i++)
		if (!list_empty(&rl->buckets[i]))
			return 0;
	return 1;
}

static inline void zfcp_reqlist_free(struct zfcp_reqlist *rl)
{
	
	BUG_ON(!zfcp_reqlist_isempty(rl));

	kfree(rl);
}

static inline struct zfcp_fsf_req *
_zfcp_reqlist_find(struct zfcp_reqlist *rl, unsigned long req_id)
{
	struct zfcp_fsf_req *req;
	unsigned int i;

	i = zfcp_reqlist_hash(req_id);
	list_for_each_entry(req, &rl->buckets[i], list)
		if (req->req_id == req_id)
			return req;
	return NULL;
}

static inline struct zfcp_fsf_req *
zfcp_reqlist_find(struct zfcp_reqlist *rl, unsigned long req_id)
{
	unsigned long flags;
	struct zfcp_fsf_req *req;

	spin_lock_irqsave(&rl->lock, flags);
	req = _zfcp_reqlist_find(rl, req_id);
	spin_unlock_irqrestore(&rl->lock, flags);

	return req;
}

static inline struct zfcp_fsf_req *
zfcp_reqlist_find_rm(struct zfcp_reqlist *rl, unsigned long req_id)
{
	unsigned long flags;
	struct zfcp_fsf_req *req;

	spin_lock_irqsave(&rl->lock, flags);
	req = _zfcp_reqlist_find(rl, req_id);
	if (req)
		list_del(&req->list);
	spin_unlock_irqrestore(&rl->lock, flags);

	return req;
}

static inline void zfcp_reqlist_add(struct zfcp_reqlist *rl,
				    struct zfcp_fsf_req *req)
{
	unsigned int i;
	unsigned long flags;

	i = zfcp_reqlist_hash(req->req_id);

	spin_lock_irqsave(&rl->lock, flags);
	list_add_tail(&req->list, &rl->buckets[i]);
	spin_unlock_irqrestore(&rl->lock, flags);
}

static inline void zfcp_reqlist_move(struct zfcp_reqlist *rl,
				     struct list_head *list)
{
	unsigned int i;
	unsigned long flags;

	spin_lock_irqsave(&rl->lock, flags);
	for (i = 0; i < ZFCP_REQ_LIST_BUCKETS; i++)
		list_splice_init(&rl->buckets[i], list);
	spin_unlock_irqrestore(&rl->lock, flags);
}

#endif 
