
/*
 * (c) Copyright Hewlett-Packard Development Company, L.P., 2006
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

#ifndef _NETLABEL_UNLABELED_H
#define _NETLABEL_UNLABELED_H

#include <net/netlabel.h>


enum {
	NLBL_UNLABEL_C_UNSPEC,
	NLBL_UNLABEL_C_ACCEPT,
	NLBL_UNLABEL_C_LIST,
	NLBL_UNLABEL_C_STATICADD,
	NLBL_UNLABEL_C_STATICREMOVE,
	NLBL_UNLABEL_C_STATICLIST,
	NLBL_UNLABEL_C_STATICADDDEF,
	NLBL_UNLABEL_C_STATICREMOVEDEF,
	NLBL_UNLABEL_C_STATICLISTDEF,
	__NLBL_UNLABEL_C_MAX,
};

enum {
	NLBL_UNLABEL_A_UNSPEC,
	NLBL_UNLABEL_A_ACPTFLG,
	NLBL_UNLABEL_A_IPV6ADDR,
	NLBL_UNLABEL_A_IPV6MASK,
	NLBL_UNLABEL_A_IPV4ADDR,
	NLBL_UNLABEL_A_IPV4MASK,
	NLBL_UNLABEL_A_IFACE,
	NLBL_UNLABEL_A_SECCTX,
	__NLBL_UNLABEL_A_MAX,
};
#define NLBL_UNLABEL_A_MAX (__NLBL_UNLABEL_A_MAX - 1)

int netlbl_unlabel_genl_init(void);

#define NETLBL_UNLHSH_BITSIZE       7

int netlbl_unlabel_init(u32 size);

int netlbl_unlhsh_add(struct net *net,
		      const char *dev_name,
		      const void *addr,
		      const void *mask,
		      u32 addr_len,
		      u32 secid,
		      struct netlbl_audit *audit_info);
int netlbl_unlhsh_remove(struct net *net,
			 const char *dev_name,
			 const void *addr,
			 const void *mask,
			 u32 addr_len,
			 struct netlbl_audit *audit_info);

int netlbl_unlabel_getattr(const struct sk_buff *skb,
			   u16 family,
			   struct netlbl_lsm_secattr *secattr);

int netlbl_unlabel_defconf(void);

#endif
