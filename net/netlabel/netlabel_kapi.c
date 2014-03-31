
/*
 * (c) Copyright Hewlett-Packard Development Company, L.P., 2006, 2008
 *
 * This program is free software;  you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY;  without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program;  if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/audit.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/netlabel.h>
#include <net/cipso_ipv4.h>
#include <asm/bug.h>
#include <linux/atomic.h>

#include "netlabel_domainhash.h"
#include "netlabel_unlabeled.h"
#include "netlabel_cipso_v4.h"
#include "netlabel_user.h"
#include "netlabel_mgmt.h"
#include "netlabel_addrlist.h"


int netlbl_cfg_map_del(const char *domain,
		       u16 family,
		       const void *addr,
		       const void *mask,
		       struct netlbl_audit *audit_info)
{
	if (addr == NULL && mask == NULL) {
		return netlbl_domhsh_remove(domain, audit_info);
	} else if (addr != NULL && mask != NULL) {
		switch (family) {
		case AF_INET:
			return netlbl_domhsh_remove_af4(domain, addr, mask,
							audit_info);
		default:
			return -EPFNOSUPPORT;
		}
	} else
		return -EINVAL;
}

int netlbl_cfg_unlbl_map_add(const char *domain,
			     u16 family,
			     const void *addr,
			     const void *mask,
			     struct netlbl_audit *audit_info)
{
	int ret_val = -ENOMEM;
	struct netlbl_dom_map *entry;
	struct netlbl_domaddr_map *addrmap = NULL;
	struct netlbl_domaddr4_map *map4 = NULL;
	struct netlbl_domaddr6_map *map6 = NULL;

	entry = kzalloc(sizeof(*entry), GFP_ATOMIC);
	if (entry == NULL)
		return -ENOMEM;
	if (domain != NULL) {
		entry->domain = kstrdup(domain, GFP_ATOMIC);
		if (entry->domain == NULL)
			goto cfg_unlbl_map_add_failure;
	}

	if (addr == NULL && mask == NULL)
		entry->type = NETLBL_NLTYPE_UNLABELED;
	else if (addr != NULL && mask != NULL) {
		addrmap = kzalloc(sizeof(*addrmap), GFP_ATOMIC);
		if (addrmap == NULL)
			goto cfg_unlbl_map_add_failure;
		INIT_LIST_HEAD(&addrmap->list4);
		INIT_LIST_HEAD(&addrmap->list6);

		switch (family) {
		case AF_INET: {
			const struct in_addr *addr4 = addr;
			const struct in_addr *mask4 = mask;
			map4 = kzalloc(sizeof(*map4), GFP_ATOMIC);
			if (map4 == NULL)
				goto cfg_unlbl_map_add_failure;
			map4->type = NETLBL_NLTYPE_UNLABELED;
			map4->list.addr = addr4->s_addr & mask4->s_addr;
			map4->list.mask = mask4->s_addr;
			map4->list.valid = 1;
			ret_val = netlbl_af4list_add(&map4->list,
						     &addrmap->list4);
			if (ret_val != 0)
				goto cfg_unlbl_map_add_failure;
			break;
			}
#if IS_ENABLED(CONFIG_IPV6)
		case AF_INET6: {
			const struct in6_addr *addr6 = addr;
			const struct in6_addr *mask6 = mask;
			map6 = kzalloc(sizeof(*map6), GFP_ATOMIC);
			if (map6 == NULL)
				goto cfg_unlbl_map_add_failure;
			map6->type = NETLBL_NLTYPE_UNLABELED;
			map6->list.addr = *addr6;
			map6->list.addr.s6_addr32[0] &= mask6->s6_addr32[0];
			map6->list.addr.s6_addr32[1] &= mask6->s6_addr32[1];
			map6->list.addr.s6_addr32[2] &= mask6->s6_addr32[2];
			map6->list.addr.s6_addr32[3] &= mask6->s6_addr32[3];
			map6->list.mask = *mask6;
			map6->list.valid = 1;
			ret_val = netlbl_af6list_add(&map6->list,
						     &addrmap->list6);
			if (ret_val != 0)
				goto cfg_unlbl_map_add_failure;
			break;
			}
#endif 
		default:
			goto cfg_unlbl_map_add_failure;
			break;
		}

		entry->type_def.addrsel = addrmap;
		entry->type = NETLBL_NLTYPE_ADDRSELECT;
	} else {
		ret_val = -EINVAL;
		goto cfg_unlbl_map_add_failure;
	}

	ret_val = netlbl_domhsh_add(entry, audit_info);
	if (ret_val != 0)
		goto cfg_unlbl_map_add_failure;

	return 0;

cfg_unlbl_map_add_failure:
	kfree(entry->domain);
	kfree(entry);
	kfree(addrmap);
	kfree(map4);
	kfree(map6);
	return ret_val;
}


int netlbl_cfg_unlbl_static_add(struct net *net,
				const char *dev_name,
				const void *addr,
				const void *mask,
				u16 family,
				u32 secid,
				struct netlbl_audit *audit_info)
{
	u32 addr_len;

	switch (family) {
	case AF_INET:
		addr_len = sizeof(struct in_addr);
		break;
#if IS_ENABLED(CONFIG_IPV6)
	case AF_INET6:
		addr_len = sizeof(struct in6_addr);
		break;
#endif 
	default:
		return -EPFNOSUPPORT;
	}

	return netlbl_unlhsh_add(net,
				 dev_name, addr, mask, addr_len,
				 secid, audit_info);
}

int netlbl_cfg_unlbl_static_del(struct net *net,
				const char *dev_name,
				const void *addr,
				const void *mask,
				u16 family,
				struct netlbl_audit *audit_info)
{
	u32 addr_len;

	switch (family) {
	case AF_INET:
		addr_len = sizeof(struct in_addr);
		break;
#if IS_ENABLED(CONFIG_IPV6)
	case AF_INET6:
		addr_len = sizeof(struct in6_addr);
		break;
#endif 
	default:
		return -EPFNOSUPPORT;
	}

	return netlbl_unlhsh_remove(net,
				    dev_name, addr, mask, addr_len,
				    audit_info);
}

int netlbl_cfg_cipsov4_add(struct cipso_v4_doi *doi_def,
			   struct netlbl_audit *audit_info)
{
	return cipso_v4_doi_add(doi_def, audit_info);
}

void netlbl_cfg_cipsov4_del(u32 doi, struct netlbl_audit *audit_info)
{
	cipso_v4_doi_remove(doi, audit_info);
}

int netlbl_cfg_cipsov4_map_add(u32 doi,
			       const char *domain,
			       const struct in_addr *addr,
			       const struct in_addr *mask,
			       struct netlbl_audit *audit_info)
{
	int ret_val = -ENOMEM;
	struct cipso_v4_doi *doi_def;
	struct netlbl_dom_map *entry;
	struct netlbl_domaddr_map *addrmap = NULL;
	struct netlbl_domaddr4_map *addrinfo = NULL;

	doi_def = cipso_v4_doi_getdef(doi);
	if (doi_def == NULL)
		return -ENOENT;

	entry = kzalloc(sizeof(*entry), GFP_ATOMIC);
	if (entry == NULL)
		goto out_entry;
	if (domain != NULL) {
		entry->domain = kstrdup(domain, GFP_ATOMIC);
		if (entry->domain == NULL)
			goto out_domain;
	}

	if (addr == NULL && mask == NULL) {
		entry->type_def.cipsov4 = doi_def;
		entry->type = NETLBL_NLTYPE_CIPSOV4;
	} else if (addr != NULL && mask != NULL) {
		addrmap = kzalloc(sizeof(*addrmap), GFP_ATOMIC);
		if (addrmap == NULL)
			goto out_addrmap;
		INIT_LIST_HEAD(&addrmap->list4);
		INIT_LIST_HEAD(&addrmap->list6);

		addrinfo = kzalloc(sizeof(*addrinfo), GFP_ATOMIC);
		if (addrinfo == NULL)
			goto out_addrinfo;
		addrinfo->type_def.cipsov4 = doi_def;
		addrinfo->type = NETLBL_NLTYPE_CIPSOV4;
		addrinfo->list.addr = addr->s_addr & mask->s_addr;
		addrinfo->list.mask = mask->s_addr;
		addrinfo->list.valid = 1;
		ret_val = netlbl_af4list_add(&addrinfo->list, &addrmap->list4);
		if (ret_val != 0)
			goto cfg_cipsov4_map_add_failure;

		entry->type_def.addrsel = addrmap;
		entry->type = NETLBL_NLTYPE_ADDRSELECT;
	} else {
		ret_val = -EINVAL;
		goto out_addrmap;
	}

	ret_val = netlbl_domhsh_add(entry, audit_info);
	if (ret_val != 0)
		goto cfg_cipsov4_map_add_failure;

	return 0;

cfg_cipsov4_map_add_failure:
	kfree(addrinfo);
out_addrinfo:
	kfree(addrmap);
out_addrmap:
	kfree(entry->domain);
out_domain:
	kfree(entry);
out_entry:
	cipso_v4_doi_putdef(doi_def);
	return ret_val;
}


int netlbl_secattr_catmap_walk(struct netlbl_lsm_secattr_catmap *catmap,
			       u32 offset)
{
	struct netlbl_lsm_secattr_catmap *iter = catmap;
	u32 node_idx;
	u32 node_bit;
	NETLBL_CATMAP_MAPTYPE bitmap;

	if (offset > iter->startbit) {
		while (offset >= (iter->startbit + NETLBL_CATMAP_SIZE)) {
			iter = iter->next;
			if (iter == NULL)
				return -ENOENT;
		}
		node_idx = (offset - iter->startbit) / NETLBL_CATMAP_MAPSIZE;
		node_bit = offset - iter->startbit -
			   (NETLBL_CATMAP_MAPSIZE * node_idx);
	} else {
		node_idx = 0;
		node_bit = 0;
	}
	bitmap = iter->bitmap[node_idx] >> node_bit;

	for (;;) {
		if (bitmap != 0) {
			while ((bitmap & NETLBL_CATMAP_BIT) == 0) {
				bitmap >>= 1;
				node_bit++;
			}
			return iter->startbit +
				(NETLBL_CATMAP_MAPSIZE * node_idx) + node_bit;
		}
		if (++node_idx >= NETLBL_CATMAP_MAPCNT) {
			if (iter->next != NULL) {
				iter = iter->next;
				node_idx = 0;
			} else
				return -ENOENT;
		}
		bitmap = iter->bitmap[node_idx];
		node_bit = 0;
	}

	return -ENOENT;
}

int netlbl_secattr_catmap_walk_rng(struct netlbl_lsm_secattr_catmap *catmap,
				   u32 offset)
{
	struct netlbl_lsm_secattr_catmap *iter = catmap;
	u32 node_idx;
	u32 node_bit;
	NETLBL_CATMAP_MAPTYPE bitmask;
	NETLBL_CATMAP_MAPTYPE bitmap;

	if (offset > iter->startbit) {
		while (offset >= (iter->startbit + NETLBL_CATMAP_SIZE)) {
			iter = iter->next;
			if (iter == NULL)
				return -ENOENT;
		}
		node_idx = (offset - iter->startbit) / NETLBL_CATMAP_MAPSIZE;
		node_bit = offset - iter->startbit -
			   (NETLBL_CATMAP_MAPSIZE * node_idx);
	} else {
		node_idx = 0;
		node_bit = 0;
	}
	bitmask = NETLBL_CATMAP_BIT << node_bit;

	for (;;) {
		bitmap = iter->bitmap[node_idx];
		while (bitmask != 0 && (bitmap & bitmask) != 0) {
			bitmask <<= 1;
			node_bit++;
		}

		if (bitmask != 0)
			return iter->startbit +
				(NETLBL_CATMAP_MAPSIZE * node_idx) +
				node_bit - 1;
		else if (++node_idx >= NETLBL_CATMAP_MAPCNT) {
			if (iter->next == NULL)
				return iter->startbit +	NETLBL_CATMAP_SIZE - 1;
			iter = iter->next;
			node_idx = 0;
		}
		bitmask = NETLBL_CATMAP_BIT;
		node_bit = 0;
	}

	return -ENOENT;
}

int netlbl_secattr_catmap_setbit(struct netlbl_lsm_secattr_catmap *catmap,
				 u32 bit,
				 gfp_t flags)
{
	struct netlbl_lsm_secattr_catmap *iter = catmap;
	u32 node_bit;
	u32 node_idx;

	while (iter->next != NULL &&
	       bit >= (iter->startbit + NETLBL_CATMAP_SIZE))
		iter = iter->next;
	if (bit >= (iter->startbit + NETLBL_CATMAP_SIZE)) {
		iter->next = netlbl_secattr_catmap_alloc(flags);
		if (iter->next == NULL)
			return -ENOMEM;
		iter = iter->next;
		iter->startbit = bit & ~(NETLBL_CATMAP_SIZE - 1);
	}

	
	node_idx = (bit - iter->startbit) / NETLBL_CATMAP_MAPSIZE;
	node_bit = bit - iter->startbit - (NETLBL_CATMAP_MAPSIZE * node_idx);
	iter->bitmap[node_idx] |= NETLBL_CATMAP_BIT << node_bit;

	return 0;
}

int netlbl_secattr_catmap_setrng(struct netlbl_lsm_secattr_catmap *catmap,
				 u32 start,
				 u32 end,
				 gfp_t flags)
{
	int ret_val = 0;
	struct netlbl_lsm_secattr_catmap *iter = catmap;
	u32 iter_max_spot;
	u32 spot;


	while (iter->next != NULL &&
	       start >= (iter->startbit + NETLBL_CATMAP_SIZE))
		iter = iter->next;
	iter_max_spot = iter->startbit + NETLBL_CATMAP_SIZE;

	for (spot = start; spot <= end && ret_val == 0; spot++) {
		if (spot >= iter_max_spot && iter->next != NULL) {
			iter = iter->next;
			iter_max_spot = iter->startbit + NETLBL_CATMAP_SIZE;
		}
		ret_val = netlbl_secattr_catmap_setbit(iter, spot, flags);
	}

	return ret_val;
}


int netlbl_enabled(void)
{
	return (atomic_read(&netlabel_mgmt_protocount) > 0);
}

int netlbl_sock_setattr(struct sock *sk,
			u16 family,
			const struct netlbl_lsm_secattr *secattr)
{
	int ret_val;
	struct netlbl_dom_map *dom_entry;

	rcu_read_lock();
	dom_entry = netlbl_domhsh_getentry(secattr->domain);
	if (dom_entry == NULL) {
		ret_val = -ENOENT;
		goto socket_setattr_return;
	}
	switch (family) {
	case AF_INET:
		switch (dom_entry->type) {
		case NETLBL_NLTYPE_ADDRSELECT:
			ret_val = -EDESTADDRREQ;
			break;
		case NETLBL_NLTYPE_CIPSOV4:
			ret_val = cipso_v4_sock_setattr(sk,
						    dom_entry->type_def.cipsov4,
						    secattr);
			break;
		case NETLBL_NLTYPE_UNLABELED:
			ret_val = 0;
			break;
		default:
			ret_val = -ENOENT;
		}
		break;
#if IS_ENABLED(CONFIG_IPV6)
	case AF_INET6:
		ret_val = 0;
		break;
#endif 
	default:
		ret_val = -EPROTONOSUPPORT;
	}

socket_setattr_return:
	rcu_read_unlock();
	return ret_val;
}

void netlbl_sock_delattr(struct sock *sk)
{
	cipso_v4_sock_delattr(sk);
}

int netlbl_sock_getattr(struct sock *sk,
			struct netlbl_lsm_secattr *secattr)
{
	int ret_val;

	switch (sk->sk_family) {
	case AF_INET:
		ret_val = cipso_v4_sock_getattr(sk, secattr);
		break;
#if IS_ENABLED(CONFIG_IPV6)
	case AF_INET6:
		ret_val = -ENOMSG;
		break;
#endif 
	default:
		ret_val = -EPROTONOSUPPORT;
	}

	return ret_val;
}

int netlbl_conn_setattr(struct sock *sk,
			struct sockaddr *addr,
			const struct netlbl_lsm_secattr *secattr)
{
	int ret_val;
	struct sockaddr_in *addr4;
	struct netlbl_domaddr4_map *af4_entry;

	rcu_read_lock();
	switch (addr->sa_family) {
	case AF_INET:
		addr4 = (struct sockaddr_in *)addr;
		af4_entry = netlbl_domhsh_getentry_af4(secattr->domain,
						       addr4->sin_addr.s_addr);
		if (af4_entry == NULL) {
			ret_val = -ENOENT;
			goto conn_setattr_return;
		}
		switch (af4_entry->type) {
		case NETLBL_NLTYPE_CIPSOV4:
			ret_val = cipso_v4_sock_setattr(sk,
						   af4_entry->type_def.cipsov4,
						   secattr);
			break;
		case NETLBL_NLTYPE_UNLABELED:
			cipso_v4_sock_delattr(sk);
			ret_val = 0;
			break;
		default:
			ret_val = -ENOENT;
		}
		break;
#if IS_ENABLED(CONFIG_IPV6)
	case AF_INET6:
		ret_val = 0;
		break;
#endif 
	default:
		ret_val = -EPROTONOSUPPORT;
	}

conn_setattr_return:
	rcu_read_unlock();
	return ret_val;
}

int netlbl_req_setattr(struct request_sock *req,
		       const struct netlbl_lsm_secattr *secattr)
{
	int ret_val;
	struct netlbl_dom_map *dom_entry;
	struct netlbl_domaddr4_map *af4_entry;
	u32 proto_type;
	struct cipso_v4_doi *proto_cv4;

	rcu_read_lock();
	dom_entry = netlbl_domhsh_getentry(secattr->domain);
	if (dom_entry == NULL) {
		ret_val = -ENOENT;
		goto req_setattr_return;
	}
	switch (req->rsk_ops->family) {
	case AF_INET:
		if (dom_entry->type == NETLBL_NLTYPE_ADDRSELECT) {
			struct inet_request_sock *req_inet = inet_rsk(req);
			af4_entry = netlbl_domhsh_getentry_af4(secattr->domain,
							    req_inet->rmt_addr);
			if (af4_entry == NULL) {
				ret_val = -ENOENT;
				goto req_setattr_return;
			}
			proto_type = af4_entry->type;
			proto_cv4 = af4_entry->type_def.cipsov4;
		} else {
			proto_type = dom_entry->type;
			proto_cv4 = dom_entry->type_def.cipsov4;
		}
		switch (proto_type) {
		case NETLBL_NLTYPE_CIPSOV4:
			ret_val = cipso_v4_req_setattr(req, proto_cv4, secattr);
			break;
		case NETLBL_NLTYPE_UNLABELED:
			cipso_v4_req_delattr(req);
			ret_val = 0;
			break;
		default:
			ret_val = -ENOENT;
		}
		break;
#if IS_ENABLED(CONFIG_IPV6)
	case AF_INET6:
		ret_val = 0;
		break;
#endif 
	default:
		ret_val = -EPROTONOSUPPORT;
	}

req_setattr_return:
	rcu_read_unlock();
	return ret_val;
}

void netlbl_req_delattr(struct request_sock *req)
{
	cipso_v4_req_delattr(req);
}

int netlbl_skbuff_setattr(struct sk_buff *skb,
			  u16 family,
			  const struct netlbl_lsm_secattr *secattr)
{
	int ret_val;
	struct iphdr *hdr4;
	struct netlbl_domaddr4_map *af4_entry;

	rcu_read_lock();
	switch (family) {
	case AF_INET:
		hdr4 = ip_hdr(skb);
		af4_entry = netlbl_domhsh_getentry_af4(secattr->domain,
						       hdr4->daddr);
		if (af4_entry == NULL) {
			ret_val = -ENOENT;
			goto skbuff_setattr_return;
		}
		switch (af4_entry->type) {
		case NETLBL_NLTYPE_CIPSOV4:
			ret_val = cipso_v4_skbuff_setattr(skb,
						   af4_entry->type_def.cipsov4,
						   secattr);
			break;
		case NETLBL_NLTYPE_UNLABELED:
			ret_val = cipso_v4_skbuff_delattr(skb);
			break;
		default:
			ret_val = -ENOENT;
		}
		break;
#if IS_ENABLED(CONFIG_IPV6)
	case AF_INET6:
		ret_val = 0;
		break;
#endif 
	default:
		ret_val = -EPROTONOSUPPORT;
	}

skbuff_setattr_return:
	rcu_read_unlock();
	return ret_val;
}

int netlbl_skbuff_getattr(const struct sk_buff *skb,
			  u16 family,
			  struct netlbl_lsm_secattr *secattr)
{
	switch (family) {
	case AF_INET:
		if (CIPSO_V4_OPTEXIST(skb) &&
		    cipso_v4_skbuff_getattr(skb, secattr) == 0)
			return 0;
		break;
#if IS_ENABLED(CONFIG_IPV6)
	case AF_INET6:
		break;
#endif 
	}

	return netlbl_unlabel_getattr(skb, family, secattr);
}

void netlbl_skbuff_err(struct sk_buff *skb, int error, int gateway)
{
	if (CIPSO_V4_OPTEXIST(skb))
		cipso_v4_error(skb, error, gateway);
}

void netlbl_cache_invalidate(void)
{
	cipso_v4_cache_invalidate();
}

int netlbl_cache_add(const struct sk_buff *skb,
		     const struct netlbl_lsm_secattr *secattr)
{
	if ((secattr->flags & NETLBL_SECATTR_CACHE) == 0)
		return -ENOMSG;

	if (CIPSO_V4_OPTEXIST(skb))
		return cipso_v4_cache_add(skb, secattr);

	return -ENOMSG;
}


struct audit_buffer *netlbl_audit_start(int type,
					struct netlbl_audit *audit_info)
{
	return netlbl_audit_start_common(type, audit_info);
}


static int __init netlbl_init(void)
{
	int ret_val;

	printk(KERN_INFO "NetLabel: Initializing\n");
	printk(KERN_INFO "NetLabel:  domain hash size = %u\n",
	       (1 << NETLBL_DOMHSH_BITSIZE));
	printk(KERN_INFO "NetLabel:  protocols ="
	       " UNLABELED"
	       " CIPSOv4"
	       "\n");

	ret_val = netlbl_domhsh_init(NETLBL_DOMHSH_BITSIZE);
	if (ret_val != 0)
		goto init_failure;

	ret_val = netlbl_unlabel_init(NETLBL_UNLHSH_BITSIZE);
	if (ret_val != 0)
		goto init_failure;

	ret_val = netlbl_netlink_init();
	if (ret_val != 0)
		goto init_failure;

	ret_val = netlbl_unlabel_defconf();
	if (ret_val != 0)
		goto init_failure;
	printk(KERN_INFO "NetLabel:  unlabeled traffic allowed by default\n");

	return 0;

init_failure:
	panic("NetLabel: failed to initialize properly (%d)\n", ret_val);
}

subsys_initcall(netlbl_init);
