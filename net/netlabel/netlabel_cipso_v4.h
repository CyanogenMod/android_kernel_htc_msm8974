
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

#ifndef _NETLABEL_CIPSO_V4
#define _NETLABEL_CIPSO_V4

#include <net/netlabel.h>


enum {
	NLBL_CIPSOV4_C_UNSPEC,
	NLBL_CIPSOV4_C_ADD,
	NLBL_CIPSOV4_C_REMOVE,
	NLBL_CIPSOV4_C_LIST,
	NLBL_CIPSOV4_C_LISTALL,
	__NLBL_CIPSOV4_C_MAX,
};

enum {
	NLBL_CIPSOV4_A_UNSPEC,
	NLBL_CIPSOV4_A_DOI,
	NLBL_CIPSOV4_A_MTYPE,
	NLBL_CIPSOV4_A_TAG,
	NLBL_CIPSOV4_A_TAGLST,
	NLBL_CIPSOV4_A_MLSLVLLOC,
	NLBL_CIPSOV4_A_MLSLVLREM,
	NLBL_CIPSOV4_A_MLSLVL,
	NLBL_CIPSOV4_A_MLSLVLLST,
	NLBL_CIPSOV4_A_MLSCATLOC,
	NLBL_CIPSOV4_A_MLSCATREM,
	NLBL_CIPSOV4_A_MLSCAT,
	NLBL_CIPSOV4_A_MLSCATLST,
	__NLBL_CIPSOV4_A_MAX,
};
#define NLBL_CIPSOV4_A_MAX (__NLBL_CIPSOV4_A_MAX - 1)

int netlbl_cipsov4_genl_init(void);

void netlbl_cipsov4_doi_free(struct rcu_head *entry);

#endif
