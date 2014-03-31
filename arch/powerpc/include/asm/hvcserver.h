/*
 * hvcserver.h
 * Copyright (C) 2004 Ryan S Arnold, IBM Corporation
 *
 * PPC64 virtual I/O console server support.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#ifndef _PPC64_HVCSERVER_H
#define _PPC64_HVCSERVER_H
#ifdef __KERNEL__

#include <linux/list.h>

#define HVCS_CLC_LENGTH	79

struct hvcs_partner_info {
	struct list_head node;
	uint32_t unit_address;
	uint32_t partition_ID;
	char location_code[HVCS_CLC_LENGTH + 1]; 
};

extern int hvcs_free_partner_info(struct list_head *head);
extern int hvcs_get_partner_info(uint32_t unit_address,
		struct list_head *head, unsigned long *pi_buff);
extern int hvcs_register_connection(uint32_t unit_address,
		uint32_t p_partition_ID, uint32_t p_unit_address);
extern int hvcs_free_connection(uint32_t unit_address);

#endif 
#endif 
