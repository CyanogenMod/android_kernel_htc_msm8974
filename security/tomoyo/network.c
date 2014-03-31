/*
 * security/tomoyo/network.c
 *
 * Copyright (C) 2005-2011  NTT DATA CORPORATION
 */

#include "common.h"
#include <linux/slab.h>

struct tomoyo_inet_addr_info {
	__be16 port;           
	const __be32 *address; 
	bool is_ipv6;
};

struct tomoyo_unix_addr_info {
	u8 *addr; 
	unsigned int addr_len;
};

struct tomoyo_addr_info {
	u8 protocol;
	u8 operation;
	struct tomoyo_inet_addr_info inet;
	struct tomoyo_unix_addr_info unix0;
};

const char * const tomoyo_proto_keyword[TOMOYO_SOCK_MAX] = {
	[SOCK_STREAM]    = "stream",
	[SOCK_DGRAM]     = "dgram",
	[SOCK_RAW]       = "raw",
	[SOCK_SEQPACKET] = "seqpacket",
	[0] = " ", 
	[4] = " ", 
};

bool tomoyo_parse_ipaddr_union(struct tomoyo_acl_param *param,
			       struct tomoyo_ipaddr_union *ptr)
{
	u8 * const min = ptr->ip[0].in6_u.u6_addr8;
	u8 * const max = ptr->ip[1].in6_u.u6_addr8;
	char *address = tomoyo_read_token(param);
	const char *end;

	if (!strchr(address, ':') &&
	    in4_pton(address, -1, min, '-', &end) > 0) {
		ptr->is_ipv6 = false;
		if (!*end)
			ptr->ip[1].s6_addr32[0] = ptr->ip[0].s6_addr32[0];
		else if (*end++ != '-' ||
			 in4_pton(end, -1, max, '\0', &end) <= 0 || *end)
			return false;
		return true;
	}
	if (in6_pton(address, -1, min, '-', &end) > 0) {
		ptr->is_ipv6 = true;
		if (!*end)
			memmove(max, min, sizeof(u16) * 8);
		else if (*end++ != '-' ||
			 in6_pton(end, -1, max, '\0', &end) <= 0 || *end)
			return false;
		return true;
	}
	return false;
}

static void tomoyo_print_ipv4(char *buffer, const unsigned int buffer_len,
			      const __be32 *min_ip, const __be32 *max_ip)
{
	snprintf(buffer, buffer_len, "%pI4%c%pI4", min_ip,
		 *min_ip == *max_ip ? '\0' : '-', max_ip);
}

static void tomoyo_print_ipv6(char *buffer, const unsigned int buffer_len,
			      const struct in6_addr *min_ip,
			      const struct in6_addr *max_ip)
{
	snprintf(buffer, buffer_len, "%pI6c%c%pI6c", min_ip,
		 !memcmp(min_ip, max_ip, 16) ? '\0' : '-', max_ip);
}

void tomoyo_print_ip(char *buf, const unsigned int size,
		     const struct tomoyo_ipaddr_union *ptr)
{
	if (ptr->is_ipv6)
		tomoyo_print_ipv6(buf, size, &ptr->ip[0], &ptr->ip[1]);
	else
		tomoyo_print_ipv4(buf, size, &ptr->ip[0].s6_addr32[0],
				  &ptr->ip[1].s6_addr32[0]);
}

static const u8 tomoyo_inet2mac
[TOMOYO_SOCK_MAX][TOMOYO_MAX_NETWORK_OPERATION] = {
	[SOCK_STREAM] = {
		[TOMOYO_NETWORK_BIND]    = TOMOYO_MAC_NETWORK_INET_STREAM_BIND,
		[TOMOYO_NETWORK_LISTEN]  =
		TOMOYO_MAC_NETWORK_INET_STREAM_LISTEN,
		[TOMOYO_NETWORK_CONNECT] =
		TOMOYO_MAC_NETWORK_INET_STREAM_CONNECT,
	},
	[SOCK_DGRAM] = {
		[TOMOYO_NETWORK_BIND]    = TOMOYO_MAC_NETWORK_INET_DGRAM_BIND,
		[TOMOYO_NETWORK_SEND]    = TOMOYO_MAC_NETWORK_INET_DGRAM_SEND,
	},
	[SOCK_RAW]    = {
		[TOMOYO_NETWORK_BIND]    = TOMOYO_MAC_NETWORK_INET_RAW_BIND,
		[TOMOYO_NETWORK_SEND]    = TOMOYO_MAC_NETWORK_INET_RAW_SEND,
	},
};

static const u8 tomoyo_unix2mac
[TOMOYO_SOCK_MAX][TOMOYO_MAX_NETWORK_OPERATION] = {
	[SOCK_STREAM] = {
		[TOMOYO_NETWORK_BIND]    = TOMOYO_MAC_NETWORK_UNIX_STREAM_BIND,
		[TOMOYO_NETWORK_LISTEN]  =
		TOMOYO_MAC_NETWORK_UNIX_STREAM_LISTEN,
		[TOMOYO_NETWORK_CONNECT] =
		TOMOYO_MAC_NETWORK_UNIX_STREAM_CONNECT,
	},
	[SOCK_DGRAM] = {
		[TOMOYO_NETWORK_BIND]    = TOMOYO_MAC_NETWORK_UNIX_DGRAM_BIND,
		[TOMOYO_NETWORK_SEND]    = TOMOYO_MAC_NETWORK_UNIX_DGRAM_SEND,
	},
	[SOCK_SEQPACKET] = {
		[TOMOYO_NETWORK_BIND]    =
		TOMOYO_MAC_NETWORK_UNIX_SEQPACKET_BIND,
		[TOMOYO_NETWORK_LISTEN]  =
		TOMOYO_MAC_NETWORK_UNIX_SEQPACKET_LISTEN,
		[TOMOYO_NETWORK_CONNECT] =
		TOMOYO_MAC_NETWORK_UNIX_SEQPACKET_CONNECT,
	},
};

static bool tomoyo_same_inet_acl(const struct tomoyo_acl_info *a,
				 const struct tomoyo_acl_info *b)
{
	const struct tomoyo_inet_acl *p1 = container_of(a, typeof(*p1), head);
	const struct tomoyo_inet_acl *p2 = container_of(b, typeof(*p2), head);

	return p1->protocol == p2->protocol &&
		tomoyo_same_ipaddr_union(&p1->address, &p2->address) &&
		tomoyo_same_number_union(&p1->port, &p2->port);
}

static bool tomoyo_same_unix_acl(const struct tomoyo_acl_info *a,
				 const struct tomoyo_acl_info *b)
{
	const struct tomoyo_unix_acl *p1 = container_of(a, typeof(*p1), head);
	const struct tomoyo_unix_acl *p2 = container_of(b, typeof(*p2), head);

	return p1->protocol == p2->protocol &&
		tomoyo_same_name_union(&p1->name, &p2->name);
}

static bool tomoyo_merge_inet_acl(struct tomoyo_acl_info *a,
				  struct tomoyo_acl_info *b,
				  const bool is_delete)
{
	u8 * const a_perm =
		&container_of(a, struct tomoyo_inet_acl, head)->perm;
	u8 perm = *a_perm;
	const u8 b_perm = container_of(b, struct tomoyo_inet_acl, head)->perm;

	if (is_delete)
		perm &= ~b_perm;
	else
		perm |= b_perm;
	*a_perm = perm;
	return !perm;
}

static bool tomoyo_merge_unix_acl(struct tomoyo_acl_info *a,
				  struct tomoyo_acl_info *b,
				  const bool is_delete)
{
	u8 * const a_perm =
		&container_of(a, struct tomoyo_unix_acl, head)->perm;
	u8 perm = *a_perm;
	const u8 b_perm = container_of(b, struct tomoyo_unix_acl, head)->perm;

	if (is_delete)
		perm &= ~b_perm;
	else
		perm |= b_perm;
	*a_perm = perm;
	return !perm;
}

int tomoyo_write_inet_network(struct tomoyo_acl_param *param)
{
	struct tomoyo_inet_acl e = { .head.type = TOMOYO_TYPE_INET_ACL };
	int error = -EINVAL;
	u8 type;
	const char *protocol = tomoyo_read_token(param);
	const char *operation = tomoyo_read_token(param);

	for (e.protocol = 0; e.protocol < TOMOYO_SOCK_MAX; e.protocol++)
		if (!strcmp(protocol, tomoyo_proto_keyword[e.protocol]))
			break;
	for (type = 0; type < TOMOYO_MAX_NETWORK_OPERATION; type++)
		if (tomoyo_permstr(operation, tomoyo_socket_keyword[type]))
			e.perm |= 1 << type;
	if (e.protocol == TOMOYO_SOCK_MAX || !e.perm)
		return -EINVAL;
	if (param->data[0] == '@') {
		param->data++;
		e.address.group =
			tomoyo_get_group(param, TOMOYO_ADDRESS_GROUP);
		if (!e.address.group)
			return -ENOMEM;
	} else {
		if (!tomoyo_parse_ipaddr_union(param, &e.address))
			goto out;
	}
	if (!tomoyo_parse_number_union(param, &e.port) ||
	    e.port.values[1] > 65535)
		goto out;
	error = tomoyo_update_domain(&e.head, sizeof(e), param,
				     tomoyo_same_inet_acl,
				     tomoyo_merge_inet_acl);
out:
	tomoyo_put_group(e.address.group);
	tomoyo_put_number_union(&e.port);
	return error;
}

int tomoyo_write_unix_network(struct tomoyo_acl_param *param)
{
	struct tomoyo_unix_acl e = { .head.type = TOMOYO_TYPE_UNIX_ACL };
	int error;
	u8 type;
	const char *protocol = tomoyo_read_token(param);
	const char *operation = tomoyo_read_token(param);

	for (e.protocol = 0; e.protocol < TOMOYO_SOCK_MAX; e.protocol++)
		if (!strcmp(protocol, tomoyo_proto_keyword[e.protocol]))
			break;
	for (type = 0; type < TOMOYO_MAX_NETWORK_OPERATION; type++)
		if (tomoyo_permstr(operation, tomoyo_socket_keyword[type]))
			e.perm |= 1 << type;
	if (e.protocol == TOMOYO_SOCK_MAX || !e.perm)
		return -EINVAL;
	if (!tomoyo_parse_name_union(param, &e.name))
		return -EINVAL;
	error = tomoyo_update_domain(&e.head, sizeof(e), param,
				     tomoyo_same_unix_acl,
				     tomoyo_merge_unix_acl);
	tomoyo_put_name_union(&e.name);
	return error;
}

static int tomoyo_audit_net_log(struct tomoyo_request_info *r,
				const char *family, const u8 protocol,
				const u8 operation, const char *address)
{
	return tomoyo_supervisor(r, "network %s %s %s %s\n", family,
				 tomoyo_proto_keyword[protocol],
				 tomoyo_socket_keyword[operation], address);
}

static int tomoyo_audit_inet_log(struct tomoyo_request_info *r)
{
	char buf[128];
	int len;
	const __be32 *address = r->param.inet_network.address;

	if (r->param.inet_network.is_ipv6)
		tomoyo_print_ipv6(buf, sizeof(buf), (const struct in6_addr *)
				  address, (const struct in6_addr *) address);
	else
		tomoyo_print_ipv4(buf, sizeof(buf), address, address);
	len = strlen(buf);
	snprintf(buf + len, sizeof(buf) - len, " %u",
		 r->param.inet_network.port);
	return tomoyo_audit_net_log(r, "inet", r->param.inet_network.protocol,
				    r->param.inet_network.operation, buf);
}

static int tomoyo_audit_unix_log(struct tomoyo_request_info *r)
{
	return tomoyo_audit_net_log(r, "unix", r->param.unix_network.protocol,
				    r->param.unix_network.operation,
				    r->param.unix_network.address->name);
}

static bool tomoyo_check_inet_acl(struct tomoyo_request_info *r,
				  const struct tomoyo_acl_info *ptr)
{
	const struct tomoyo_inet_acl *acl =
		container_of(ptr, typeof(*acl), head);
	const u8 size = r->param.inet_network.is_ipv6 ? 16 : 4;

	if (!(acl->perm & (1 << r->param.inet_network.operation)) ||
	    !tomoyo_compare_number_union(r->param.inet_network.port,
					 &acl->port))
		return false;
	if (acl->address.group)
		return tomoyo_address_matches_group
			(r->param.inet_network.is_ipv6,
			 r->param.inet_network.address, acl->address.group);
	return acl->address.is_ipv6 == r->param.inet_network.is_ipv6 &&
		memcmp(&acl->address.ip[0],
		       r->param.inet_network.address, size) <= 0 &&
		memcmp(r->param.inet_network.address,
		       &acl->address.ip[1], size) <= 0;
}

static bool tomoyo_check_unix_acl(struct tomoyo_request_info *r,
				  const struct tomoyo_acl_info *ptr)
{
	const struct tomoyo_unix_acl *acl =
		container_of(ptr, typeof(*acl), head);

	return (acl->perm & (1 << r->param.unix_network.operation)) &&
		tomoyo_compare_name_union(r->param.unix_network.address,
					  &acl->name);
}

static int tomoyo_inet_entry(const struct tomoyo_addr_info *address)
{
	const int idx = tomoyo_read_lock();
	struct tomoyo_request_info r;
	int error = 0;
	const u8 type = tomoyo_inet2mac[address->protocol][address->operation];

	if (type && tomoyo_init_request_info(&r, NULL, type)
	    != TOMOYO_CONFIG_DISABLED) {
		r.param_type = TOMOYO_TYPE_INET_ACL;
		r.param.inet_network.protocol = address->protocol;
		r.param.inet_network.operation = address->operation;
		r.param.inet_network.is_ipv6 = address->inet.is_ipv6;
		r.param.inet_network.address = address->inet.address;
		r.param.inet_network.port = ntohs(address->inet.port);
		do {
			tomoyo_check_acl(&r, tomoyo_check_inet_acl);
			error = tomoyo_audit_inet_log(&r);
		} while (error == TOMOYO_RETRY_REQUEST);
	}
	tomoyo_read_unlock(idx);
	return error;
}

static int tomoyo_check_inet_address(const struct sockaddr *addr,
				     const unsigned int addr_len,
				     const u16 port,
				     struct tomoyo_addr_info *address)
{
	struct tomoyo_inet_addr_info *i = &address->inet;

	switch (addr->sa_family) {
	case AF_INET6:
		if (addr_len < SIN6_LEN_RFC2133)
			goto skip;
		i->is_ipv6 = true;
		i->address = (__be32 *)
			((struct sockaddr_in6 *) addr)->sin6_addr.s6_addr;
		i->port = ((struct sockaddr_in6 *) addr)->sin6_port;
		break;
	case AF_INET:
		if (addr_len < sizeof(struct sockaddr_in))
			goto skip;
		i->is_ipv6 = false;
		i->address = (__be32 *)
			&((struct sockaddr_in *) addr)->sin_addr;
		i->port = ((struct sockaddr_in *) addr)->sin_port;
		break;
	default:
		goto skip;
	}
	if (address->protocol == SOCK_RAW)
		i->port = htons(port);
	return tomoyo_inet_entry(address);
skip:
	return 0;
}

static int tomoyo_unix_entry(const struct tomoyo_addr_info *address)
{
	const int idx = tomoyo_read_lock();
	struct tomoyo_request_info r;
	int error = 0;
	const u8 type = tomoyo_unix2mac[address->protocol][address->operation];

	if (type && tomoyo_init_request_info(&r, NULL, type)
	    != TOMOYO_CONFIG_DISABLED) {
		char *buf = address->unix0.addr;
		int len = address->unix0.addr_len - sizeof(sa_family_t);

		if (len <= 0) {
			buf = "anonymous";
			len = 9;
		} else if (buf[0]) {
			len = strnlen(buf, len);
		}
		buf = tomoyo_encode2(buf, len);
		if (buf) {
			struct tomoyo_path_info addr;

			addr.name = buf;
			tomoyo_fill_path_info(&addr);
			r.param_type = TOMOYO_TYPE_UNIX_ACL;
			r.param.unix_network.protocol = address->protocol;
			r.param.unix_network.operation = address->operation;
			r.param.unix_network.address = &addr;
			do {
				tomoyo_check_acl(&r, tomoyo_check_unix_acl);
				error = tomoyo_audit_unix_log(&r);
			} while (error == TOMOYO_RETRY_REQUEST);
			kfree(buf);
		} else
			error = -ENOMEM;
	}
	tomoyo_read_unlock(idx);
	return error;
}

static int tomoyo_check_unix_address(struct sockaddr *addr,
				     const unsigned int addr_len,
				     struct tomoyo_addr_info *address)
{
	struct tomoyo_unix_addr_info *u = &address->unix0;

	if (addr->sa_family != AF_UNIX)
		return 0;
	u->addr = ((struct sockaddr_un *) addr)->sun_path;
	u->addr_len = addr_len;
	return tomoyo_unix_entry(address);
}

static bool tomoyo_kernel_service(void)
{
	
	return segment_eq(get_fs(), KERNEL_DS);
}

static u8 tomoyo_sock_family(struct sock *sk)
{
	u8 family;

	if (tomoyo_kernel_service())
		return 0;
	family = sk->sk_family;
	switch (family) {
	case PF_INET:
	case PF_INET6:
	case PF_UNIX:
		return family;
	default:
		return 0;
	}
}

int tomoyo_socket_listen_permission(struct socket *sock)
{
	struct tomoyo_addr_info address;
	const u8 family = tomoyo_sock_family(sock->sk);
	const unsigned int type = sock->type;
	struct sockaddr_storage addr;
	int addr_len;

	if (!family || (type != SOCK_STREAM && type != SOCK_SEQPACKET))
		return 0;
	{
		const int error = sock->ops->getname(sock, (struct sockaddr *)
						     &addr, &addr_len, 0);

		if (error)
			return error;
	}
	address.protocol = type;
	address.operation = TOMOYO_NETWORK_LISTEN;
	if (family == PF_UNIX)
		return tomoyo_check_unix_address((struct sockaddr *) &addr,
						 addr_len, &address);
	return tomoyo_check_inet_address((struct sockaddr *) &addr, addr_len,
					 0, &address);
}

int tomoyo_socket_connect_permission(struct socket *sock,
				     struct sockaddr *addr, int addr_len)
{
	struct tomoyo_addr_info address;
	const u8 family = tomoyo_sock_family(sock->sk);
	const unsigned int type = sock->type;

	if (!family)
		return 0;
	address.protocol = type;
	switch (type) {
	case SOCK_DGRAM:
	case SOCK_RAW:
		address.operation = TOMOYO_NETWORK_SEND;
		break;
	case SOCK_STREAM:
	case SOCK_SEQPACKET:
		address.operation = TOMOYO_NETWORK_CONNECT;
		break;
	default:
		return 0;
	}
	if (family == PF_UNIX)
		return tomoyo_check_unix_address(addr, addr_len, &address);
	return tomoyo_check_inet_address(addr, addr_len, sock->sk->sk_protocol,
					 &address);
}

int tomoyo_socket_bind_permission(struct socket *sock, struct sockaddr *addr,
				  int addr_len)
{
	struct tomoyo_addr_info address;
	const u8 family = tomoyo_sock_family(sock->sk);
	const unsigned int type = sock->type;

	if (!family)
		return 0;
	switch (type) {
	case SOCK_STREAM:
	case SOCK_DGRAM:
	case SOCK_RAW:
	case SOCK_SEQPACKET:
		address.protocol = type;
		address.operation = TOMOYO_NETWORK_BIND;
		break;
	default:
		return 0;
	}
	if (family == PF_UNIX)
		return tomoyo_check_unix_address(addr, addr_len, &address);
	return tomoyo_check_inet_address(addr, addr_len, sock->sk->sk_protocol,
					 &address);
}

int tomoyo_socket_sendmsg_permission(struct socket *sock, struct msghdr *msg,
				     int size)
{
	struct tomoyo_addr_info address;
	const u8 family = tomoyo_sock_family(sock->sk);
	const unsigned int type = sock->type;

	if (!msg->msg_name || !family ||
	    (type != SOCK_DGRAM && type != SOCK_RAW))
		return 0;
	address.protocol = type;
	address.operation = TOMOYO_NETWORK_SEND;
	if (family == PF_UNIX)
		return tomoyo_check_unix_address((struct sockaddr *)
						 msg->msg_name,
						 msg->msg_namelen, &address);
	return tomoyo_check_inet_address((struct sockaddr *) msg->msg_name,
					 msg->msg_namelen,
					 sock->sk->sk_protocol, &address);
}
