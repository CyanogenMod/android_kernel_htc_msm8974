/*
 * HSI clients registration interface
 *
 * Copyright (C) 2010 Nokia Corporation. All rights reserved.
 *
 * Contact: Carlos Chinea <carlos.chinea@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */
#include <linux/hsi/hsi.h>
#include <linux/list.h>
#include <linux/slab.h>
#include "hsi_core.h"

LIST_HEAD(hsi_board_list);
EXPORT_SYMBOL_GPL(hsi_board_list);

int __init hsi_register_board_info(struct hsi_board_info const *info,
							unsigned int len)
{
	struct hsi_cl_info *cl_info;

	cl_info = kzalloc(sizeof(*cl_info) * len, GFP_KERNEL);
	if (!cl_info)
		return -ENOMEM;

	for (; len; len--, info++, cl_info++) {
		cl_info->info = *info;
		list_add_tail(&cl_info->list, &hsi_board_list);
	}

	return 0;
}
