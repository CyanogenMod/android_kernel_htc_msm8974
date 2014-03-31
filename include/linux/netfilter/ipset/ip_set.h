#ifndef _IP_SET_H
#define _IP_SET_H

/* Copyright (C) 2000-2002 Joakim Axelsson <gozem@linux.nu>
 *                         Patrick Schaaf <bof@bof.de>
 *                         Martin Josefsson <gandalf@wlug.westbo.se>
 * Copyright (C) 2003-2011 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/types.h>

#define IPSET_PROTOCOL		6

#define IPSET_MAXNAMELEN	32

enum ipset_cmd {
	IPSET_CMD_NONE,
	IPSET_CMD_PROTOCOL,	
	IPSET_CMD_CREATE,	
	IPSET_CMD_DESTROY,	
	IPSET_CMD_FLUSH,	
	IPSET_CMD_RENAME,	
	IPSET_CMD_SWAP,		
	IPSET_CMD_LIST,		
	IPSET_CMD_SAVE,		
	IPSET_CMD_ADD,		
	IPSET_CMD_DEL,		
	IPSET_CMD_TEST,		
	IPSET_CMD_HEADER,	
	IPSET_CMD_TYPE,		
	IPSET_MSG_MAX,		

	
	IPSET_CMD_RESTORE = IPSET_MSG_MAX, 
	IPSET_CMD_HELP,		
	IPSET_CMD_VERSION,	
	IPSET_CMD_QUIT,		

	IPSET_CMD_MAX,

	IPSET_CMD_COMMIT = IPSET_CMD_MAX, 
};

enum {
	IPSET_ATTR_UNSPEC,
	IPSET_ATTR_PROTOCOL,	
	IPSET_ATTR_SETNAME,	
	IPSET_ATTR_TYPENAME,	
	IPSET_ATTR_SETNAME2 = IPSET_ATTR_TYPENAME, 
	IPSET_ATTR_REVISION,	
	IPSET_ATTR_FAMILY,	
	IPSET_ATTR_FLAGS,	
	IPSET_ATTR_DATA,	
	IPSET_ATTR_ADT,		
	IPSET_ATTR_LINENO,	
	IPSET_ATTR_PROTOCOL_MIN, 
	IPSET_ATTR_REVISION_MIN	= IPSET_ATTR_PROTOCOL_MIN, 
	__IPSET_ATTR_CMD_MAX,
};
#define IPSET_ATTR_CMD_MAX	(__IPSET_ATTR_CMD_MAX - 1)

enum {
	IPSET_ATTR_IP = IPSET_ATTR_UNSPEC + 1,
	IPSET_ATTR_IP_FROM = IPSET_ATTR_IP,
	IPSET_ATTR_IP_TO,	
	IPSET_ATTR_CIDR,	
	IPSET_ATTR_PORT,	
	IPSET_ATTR_PORT_FROM = IPSET_ATTR_PORT,
	IPSET_ATTR_PORT_TO,	
	IPSET_ATTR_TIMEOUT,	
	IPSET_ATTR_PROTO,	
	IPSET_ATTR_CADT_FLAGS,	
	IPSET_ATTR_CADT_LINENO = IPSET_ATTR_LINENO,	
	
	IPSET_ATTR_CADT_MAX = 16,
	
	IPSET_ATTR_GC,
	IPSET_ATTR_HASHSIZE,
	IPSET_ATTR_MAXELEM,
	IPSET_ATTR_NETMASK,
	IPSET_ATTR_PROBES,
	IPSET_ATTR_RESIZE,
	IPSET_ATTR_SIZE,
	
	IPSET_ATTR_ELEMENTS,
	IPSET_ATTR_REFERENCES,
	IPSET_ATTR_MEMSIZE,

	__IPSET_ATTR_CREATE_MAX,
};
#define IPSET_ATTR_CREATE_MAX	(__IPSET_ATTR_CREATE_MAX - 1)

enum {
	IPSET_ATTR_ETHER = IPSET_ATTR_CADT_MAX + 1,
	IPSET_ATTR_NAME,
	IPSET_ATTR_NAMEREF,
	IPSET_ATTR_IP2,
	IPSET_ATTR_CIDR2,
	IPSET_ATTR_IP2_TO,
	IPSET_ATTR_IFACE,
	__IPSET_ATTR_ADT_MAX,
};
#define IPSET_ATTR_ADT_MAX	(__IPSET_ATTR_ADT_MAX - 1)

enum {
	IPSET_ATTR_IPADDR_IPV4 = IPSET_ATTR_UNSPEC + 1,
	IPSET_ATTR_IPADDR_IPV6,
	__IPSET_ATTR_IPADDR_MAX,
};
#define IPSET_ATTR_IPADDR_MAX	(__IPSET_ATTR_IPADDR_MAX - 1)

enum ipset_errno {
	IPSET_ERR_PRIVATE = 4096,
	IPSET_ERR_PROTOCOL,
	IPSET_ERR_FIND_TYPE,
	IPSET_ERR_MAX_SETS,
	IPSET_ERR_BUSY,
	IPSET_ERR_EXIST_SETNAME2,
	IPSET_ERR_TYPE_MISMATCH,
	IPSET_ERR_EXIST,
	IPSET_ERR_INVALID_CIDR,
	IPSET_ERR_INVALID_NETMASK,
	IPSET_ERR_INVALID_FAMILY,
	IPSET_ERR_TIMEOUT,
	IPSET_ERR_REFERENCED,
	IPSET_ERR_IPADDR_IPV4,
	IPSET_ERR_IPADDR_IPV6,

	
	IPSET_ERR_TYPE_SPECIFIC = 4352,
};

enum ipset_cmd_flags {
	IPSET_FLAG_BIT_EXIST	= 0,
	IPSET_FLAG_EXIST	= (1 << IPSET_FLAG_BIT_EXIST),
	IPSET_FLAG_BIT_LIST_SETNAME = 1,
	IPSET_FLAG_LIST_SETNAME	= (1 << IPSET_FLAG_BIT_LIST_SETNAME),
	IPSET_FLAG_BIT_LIST_HEADER = 2,
	IPSET_FLAG_LIST_HEADER	= (1 << IPSET_FLAG_BIT_LIST_HEADER),
	IPSET_FLAG_CMD_MAX = 15,	
};

enum ipset_cadt_flags {
	IPSET_FLAG_BIT_BEFORE	= 0,
	IPSET_FLAG_BEFORE	= (1 << IPSET_FLAG_BIT_BEFORE),
	IPSET_FLAG_BIT_PHYSDEV	= 1,
	IPSET_FLAG_PHYSDEV	= (1 << IPSET_FLAG_BIT_PHYSDEV),
	IPSET_FLAG_BIT_NOMATCH	= 2,
	IPSET_FLAG_NOMATCH	= (1 << IPSET_FLAG_BIT_NOMATCH),
	IPSET_FLAG_CADT_MAX	= 15,	
};

enum ipset_adt {
	IPSET_ADD,
	IPSET_DEL,
	IPSET_TEST,
	IPSET_ADT_MAX,
	IPSET_CREATE = IPSET_ADT_MAX,
	IPSET_CADT_MAX,
};

typedef __u16 ip_set_id_t;

#define IPSET_INVALID_ID		65535

enum ip_set_dim {
	IPSET_DIM_ZERO = 0,
	IPSET_DIM_ONE,
	IPSET_DIM_TWO,
	IPSET_DIM_THREE,
	IPSET_DIM_MAX = 6,
};

enum ip_set_kopt {
	IPSET_INV_MATCH = (1 << IPSET_DIM_ZERO),
	IPSET_DIM_ONE_SRC = (1 << IPSET_DIM_ONE),
	IPSET_DIM_TWO_SRC = (1 << IPSET_DIM_TWO),
	IPSET_DIM_THREE_SRC = (1 << IPSET_DIM_THREE),
};

#ifdef __KERNEL__
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/netlink.h>
#include <linux/netfilter.h>
#include <linux/netfilter/x_tables.h>
#include <linux/vmalloc.h>
#include <net/netlink.h>

enum ip_set_feature {
	IPSET_TYPE_IP_FLAG = 0,
	IPSET_TYPE_IP = (1 << IPSET_TYPE_IP_FLAG),
	IPSET_TYPE_PORT_FLAG = 1,
	IPSET_TYPE_PORT = (1 << IPSET_TYPE_PORT_FLAG),
	IPSET_TYPE_MAC_FLAG = 2,
	IPSET_TYPE_MAC = (1 << IPSET_TYPE_MAC_FLAG),
	IPSET_TYPE_IP2_FLAG = 3,
	IPSET_TYPE_IP2 = (1 << IPSET_TYPE_IP2_FLAG),
	IPSET_TYPE_NAME_FLAG = 4,
	IPSET_TYPE_NAME = (1 << IPSET_TYPE_NAME_FLAG),
	IPSET_TYPE_IFACE_FLAG = 5,
	IPSET_TYPE_IFACE = (1 << IPSET_TYPE_IFACE_FLAG),
	IPSET_DUMP_LAST_FLAG = 7,
	IPSET_DUMP_LAST = (1 << IPSET_DUMP_LAST_FLAG),
};

struct ip_set;

typedef int (*ipset_adtfn)(struct ip_set *set, void *value,
			   u32 timeout, u32 flags);

struct ip_set_adt_opt {
	u8 family;		
	u8 dim;			
	u8 flags;		
	u32 cmdflags;		
	u32 timeout;		
};

struct ip_set_type_variant {
	int (*kadt)(struct ip_set *set, const struct sk_buff * skb,
		    const struct xt_action_param *par,
		    enum ipset_adt adt, const struct ip_set_adt_opt *opt);

	int (*uadt)(struct ip_set *set, struct nlattr *tb[],
		    enum ipset_adt adt, u32 *lineno, u32 flags, bool retried);

	
	ipset_adtfn adt[IPSET_ADT_MAX];

	
	int (*resize)(struct ip_set *set, bool retried);
	
	void (*destroy)(struct ip_set *set);
	
	void (*flush)(struct ip_set *set);
	
	void (*expire)(struct ip_set *set);
	
	int (*head)(struct ip_set *set, struct sk_buff *skb);
	
	int (*list)(const struct ip_set *set, struct sk_buff *skb,
		    struct netlink_callback *cb);

	bool (*same_set)(const struct ip_set *a, const struct ip_set *b);
};

struct ip_set_type {
	struct list_head list;

	
	char name[IPSET_MAXNAMELEN];
	
	u8 protocol;
	
	u8 features;
	
	u8 dimension;
	u8 family;
	
	u8 revision_min, revision_max;

	
	int (*create)(struct ip_set *set, struct nlattr *tb[], u32 flags);

	
	const struct nla_policy create_policy[IPSET_ATTR_CREATE_MAX + 1];
	const struct nla_policy adt_policy[IPSET_ATTR_ADT_MAX + 1];

	
	struct module *me;
};

extern int ip_set_type_register(struct ip_set_type *set_type);
extern void ip_set_type_unregister(struct ip_set_type *set_type);

struct ip_set {
	
	char name[IPSET_MAXNAMELEN];
	
	rwlock_t lock;
	
	u32 ref;
	
	struct ip_set_type *type;
	
	const struct ip_set_type_variant *variant;
	
	u8 family;
	
	u8 revision;
	
	void *data;
};

extern ip_set_id_t ip_set_get_byname(const char *name, struct ip_set **set);
extern void ip_set_put_byindex(ip_set_id_t index);
extern const char *ip_set_name_byindex(ip_set_id_t index);
extern ip_set_id_t ip_set_nfnl_get(const char *name);
extern ip_set_id_t ip_set_nfnl_get_byindex(ip_set_id_t index);
extern void ip_set_nfnl_put(ip_set_id_t index);


extern int ip_set_add(ip_set_id_t id, const struct sk_buff *skb,
		      const struct xt_action_param *par,
		      const struct ip_set_adt_opt *opt);
extern int ip_set_del(ip_set_id_t id, const struct sk_buff *skb,
		      const struct xt_action_param *par,
		      const struct ip_set_adt_opt *opt);
extern int ip_set_test(ip_set_id_t id, const struct sk_buff *skb,
		       const struct xt_action_param *par,
		       const struct ip_set_adt_opt *opt);

extern void *ip_set_alloc(size_t size);
extern void ip_set_free(void *members);
extern int ip_set_get_ipaddr4(struct nlattr *nla,  __be32 *ipaddr);
extern int ip_set_get_ipaddr6(struct nlattr *nla, union nf_inet_addr *ipaddr);

static inline int
ip_set_get_hostipaddr4(struct nlattr *nla, u32 *ipaddr)
{
	__be32 ip;
	int ret = ip_set_get_ipaddr4(nla, &ip);

	if (ret)
		return ret;
	*ipaddr = ntohl(ip);
	return 0;
}

static inline bool
ip_set_eexist(int ret, u32 flags)
{
	return ret == -IPSET_ERR_EXIST && (flags & IPSET_FLAG_EXIST);
}

static inline bool
ip_set_attr_netorder(struct nlattr *tb[], int type)
{
	return tb[type] && (tb[type]->nla_type & NLA_F_NET_BYTEORDER);
}

static inline bool
ip_set_optattr_netorder(struct nlattr *tb[], int type)
{
	return !tb[type] || (tb[type]->nla_type & NLA_F_NET_BYTEORDER);
}

static inline u32
ip_set_get_h32(const struct nlattr *attr)
{
	return ntohl(nla_get_be32(attr));
}

static inline u16
ip_set_get_h16(const struct nlattr *attr)
{
	return ntohs(nla_get_be16(attr));
}

#define ipset_nest_start(skb, attr) nla_nest_start(skb, attr | NLA_F_NESTED)
#define ipset_nest_end(skb, start)  nla_nest_end(skb, start)

#define NLA_PUT_IPADDR4(skb, type, ipaddr)			\
do {								\
	struct nlattr *__nested = ipset_nest_start(skb, type);	\
								\
	if (!__nested)						\
		goto nla_put_failure;				\
	NLA_PUT_NET32(skb, IPSET_ATTR_IPADDR_IPV4, ipaddr);	\
	ipset_nest_end(skb, __nested);				\
} while (0)

#define NLA_PUT_IPADDR6(skb, type, ipaddrptr)			\
do {								\
	struct nlattr *__nested = ipset_nest_start(skb, type);	\
								\
	if (!__nested)						\
		goto nla_put_failure;				\
	NLA_PUT(skb, IPSET_ATTR_IPADDR_IPV6,			\
		sizeof(struct in6_addr), ipaddrptr);		\
	ipset_nest_end(skb, __nested);				\
} while (0)

static inline __be32
ip4addr(const struct sk_buff *skb, bool src)
{
	return src ? ip_hdr(skb)->saddr : ip_hdr(skb)->daddr;
}

static inline void
ip4addrptr(const struct sk_buff *skb, bool src, __be32 *addr)
{
	*addr = src ? ip_hdr(skb)->saddr : ip_hdr(skb)->daddr;
}

static inline void
ip6addrptr(const struct sk_buff *skb, bool src, struct in6_addr *addr)
{
	memcpy(addr, src ? &ipv6_hdr(skb)->saddr : &ipv6_hdr(skb)->daddr,
	       sizeof(*addr));
}

static inline int
bitmap_bytes(u32 a, u32 b)
{
	return 4 * ((((b - a + 8) / 8) + 3) / 4);
}

#endif 


#define SO_IP_SET		83

union ip_set_name_index {
	char name[IPSET_MAXNAMELEN];
	ip_set_id_t index;
};

#define IP_SET_OP_GET_BYNAME	0x00000006	
struct ip_set_req_get_set {
	unsigned op;
	unsigned version;
	union ip_set_name_index set;
};

#define IP_SET_OP_GET_BYINDEX	0x00000007	

#define IP_SET_OP_VERSION	0x00000100	
struct ip_set_req_version {
	unsigned op;
	unsigned version;
};

#endif 
