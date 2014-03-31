#ifndef _RDMA_NETLINK_H
#define _RDMA_NETLINK_H

#include <linux/types.h>

enum {
	RDMA_NL_RDMA_CM = 1
};

#define RDMA_NL_GET_CLIENT(type) ((type & (((1 << 6) - 1) << 10)) >> 10)
#define RDMA_NL_GET_OP(type) (type & ((1 << 10) - 1))
#define RDMA_NL_GET_TYPE(client, op) ((client << 10) + op)

enum {
	RDMA_NL_RDMA_CM_ID_STATS = 0,
	RDMA_NL_RDMA_CM_NUM_OPS
};

enum {
	RDMA_NL_RDMA_CM_ATTR_SRC_ADDR = 1,
	RDMA_NL_RDMA_CM_ATTR_DST_ADDR,
	RDMA_NL_RDMA_CM_NUM_ATTR,
};

struct rdma_cm_id_stats {
	__u32	qp_num;
	__u32	bound_dev_if;
	__u32	port_space;
	__s32	pid;
	__u8	cm_state;
	__u8	node_type;
	__u8	port_num;
	__u8	qp_type;
};

#ifdef __KERNEL__

#include <linux/netlink.h>

struct ibnl_client_cbs {
	int (*dump)(struct sk_buff *skb, struct netlink_callback *nlcb);
};

int ibnl_init(void);
void ibnl_cleanup(void);

int ibnl_add_client(int index, int nops,
		    const struct ibnl_client_cbs cb_table[]);

int ibnl_remove_client(int index);

void *ibnl_put_msg(struct sk_buff *skb, struct nlmsghdr **nlh, int seq,
		   int len, int client, int op);
int ibnl_put_attr(struct sk_buff *skb, struct nlmsghdr *nlh,
		  int len, void *data, int type);

#endif 

#endif 
