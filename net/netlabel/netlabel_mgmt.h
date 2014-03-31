
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

#ifndef _NETLABEL_MGMT_H
#define _NETLABEL_MGMT_H

#include <net/netlabel.h>
#include <linux/atomic.h>


enum {
	NLBL_MGMT_C_UNSPEC,
	NLBL_MGMT_C_ADD,
	NLBL_MGMT_C_REMOVE,
	NLBL_MGMT_C_LISTALL,
	NLBL_MGMT_C_ADDDEF,
	NLBL_MGMT_C_REMOVEDEF,
	NLBL_MGMT_C_LISTDEF,
	NLBL_MGMT_C_PROTOCOLS,
	NLBL_MGMT_C_VERSION,
	__NLBL_MGMT_C_MAX,
};

enum {
	NLBL_MGMT_A_UNSPEC,
	NLBL_MGMT_A_DOMAIN,
	NLBL_MGMT_A_PROTOCOL,
	NLBL_MGMT_A_VERSION,
	NLBL_MGMT_A_CV4DOI,
	NLBL_MGMT_A_IPV6ADDR,
	NLBL_MGMT_A_IPV6MASK,
	NLBL_MGMT_A_IPV4ADDR,
	NLBL_MGMT_A_IPV4MASK,
	NLBL_MGMT_A_ADDRSELECTOR,
	NLBL_MGMT_A_SELECTORLIST,
	__NLBL_MGMT_A_MAX,
};
#define NLBL_MGMT_A_MAX (__NLBL_MGMT_A_MAX - 1)

int netlbl_mgmt_genl_init(void);

extern atomic_t netlabel_mgmt_protocount;

#endif
