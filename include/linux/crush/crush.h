#ifndef CEPH_CRUSH_CRUSH_H
#define CEPH_CRUSH_CRUSH_H

#include <linux/types.h>

/*
 * CRUSH is a pseudo-random data distribution algorithm that
 * efficiently distributes input values (typically, data objects)
 * across a heterogeneous, structured storage cluster.
 *
 * The algorithm was originally described in detail in this paper
 * (although the algorithm has evolved somewhat since then):
 *
 *     http://www.ssrc.ucsc.edu/Papers/weil-sc06.pdf
 *
 * LGPL2
 */


#define CRUSH_MAGIC 0x00010000ul   


#define CRUSH_MAX_DEPTH 10  
#define CRUSH_MAX_SET   10  


struct crush_rule_step {
	__u32 op;
	__s32 arg1;
	__s32 arg2;
};

enum {
	CRUSH_RULE_NOOP = 0,
	CRUSH_RULE_TAKE = 1,          
	CRUSH_RULE_CHOOSE_FIRSTN = 2, 
				      
	CRUSH_RULE_CHOOSE_INDEP = 3,  
	CRUSH_RULE_EMIT = 4,          
	CRUSH_RULE_CHOOSE_LEAF_FIRSTN = 6,
	CRUSH_RULE_CHOOSE_LEAF_INDEP = 7,
};

#define CRUSH_CHOOSE_N            0
#define CRUSH_CHOOSE_N_MINUS(x)   (-(x))

struct crush_rule_mask {
	__u8 ruleset;
	__u8 type;
	__u8 min_size;
	__u8 max_size;
};

struct crush_rule {
	__u32 len;
	struct crush_rule_mask mask;
	struct crush_rule_step steps[0];
};

#define crush_rule_size(len) (sizeof(struct crush_rule) + \
			      (len)*sizeof(struct crush_rule_step))



enum {
	CRUSH_BUCKET_UNIFORM = 1,
	CRUSH_BUCKET_LIST = 2,
	CRUSH_BUCKET_TREE = 3,
	CRUSH_BUCKET_STRAW = 4
};
extern const char *crush_bucket_alg_name(int alg);

struct crush_bucket {
	__s32 id;        
	__u16 type;      
	__u8 alg;        
	__u8 hash;       
	__u32 weight;    
	__u32 size;      
	__s32 *items;

	__u32 perm_x;  
	__u32 perm_n;  
	__u32 *perm;
};

struct crush_bucket_uniform {
	struct crush_bucket h;
	__u32 item_weight;  
};

struct crush_bucket_list {
	struct crush_bucket h;
	__u32 *item_weights;  
	__u32 *sum_weights;   
};

struct crush_bucket_tree {
	struct crush_bucket h;  
	__u8 num_nodes;
	__u32 *node_weights;
};

struct crush_bucket_straw {
	struct crush_bucket h;
	__u32 *item_weights;   
	__u32 *straws;         
};



struct crush_map {
	struct crush_bucket **buckets;
	struct crush_rule **rules;

	__u32 *bucket_parents;
	__u32 *device_parents;

	__s32 max_buckets;
	__u32 max_rules;
	__s32 max_devices;
};


extern int crush_get_bucket_item_weight(struct crush_bucket *b, int pos);
extern void crush_calc_parents(struct crush_map *map);
extern void crush_destroy_bucket_uniform(struct crush_bucket_uniform *b);
extern void crush_destroy_bucket_list(struct crush_bucket_list *b);
extern void crush_destroy_bucket_tree(struct crush_bucket_tree *b);
extern void crush_destroy_bucket_straw(struct crush_bucket_straw *b);
extern void crush_destroy_bucket(struct crush_bucket *b);
extern void crush_destroy(struct crush_map *map);

#endif
