/*
 * hvcserver.c
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

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <asm/hvcall.h>
#include <asm/hvcserver.h>
#include <asm/io.h>

#define HVCS_ARCH_VERSION "1.0.0"

MODULE_AUTHOR("Ryan S. Arnold <rsa@us.ibm.com>");
MODULE_DESCRIPTION("IBM hvcs ppc64 API");
MODULE_LICENSE("GPL");
MODULE_VERSION(HVCS_ARCH_VERSION);

static int hvcs_convert(long to_convert)
{
	switch (to_convert) {
		case H_SUCCESS:
			return 0;
		case H_PARAMETER:
			return -EINVAL;
		case H_HARDWARE:
			return -EIO;
		case H_BUSY:
		case H_LONG_BUSY_ORDER_1_MSEC:
		case H_LONG_BUSY_ORDER_10_MSEC:
		case H_LONG_BUSY_ORDER_100_MSEC:
		case H_LONG_BUSY_ORDER_1_SEC:
		case H_LONG_BUSY_ORDER_10_SEC:
		case H_LONG_BUSY_ORDER_100_SEC:
			return -EBUSY;
		case H_FUNCTION: 
		default:
			return -EPERM;
	}
}

int hvcs_free_partner_info(struct list_head *head)
{
	struct hvcs_partner_info *pi;
	struct list_head *element;

	if (!head)
		return -EINVAL;

	while (!list_empty(head)) {
		element = head->next;
		pi = list_entry(element, struct hvcs_partner_info, node);
		list_del(element);
		kfree(pi);
	}

	return 0;
}
EXPORT_SYMBOL(hvcs_free_partner_info);

static int hvcs_next_partner(uint32_t unit_address,
		unsigned long last_p_partition_ID,
		unsigned long last_p_unit_address, unsigned long *pi_buff)

{
	long retval;
	retval = plpar_hcall_norets(H_VTERM_PARTNER_INFO, unit_address,
			last_p_partition_ID,
				last_p_unit_address, virt_to_phys(pi_buff));
	return hvcs_convert(retval);
}

int hvcs_get_partner_info(uint32_t unit_address, struct list_head *head,
		unsigned long *pi_buff)
{
	unsigned long	last_p_partition_ID;
	unsigned long	last_p_unit_address;
	struct hvcs_partner_info *next_partner_info = NULL;
	int more = 1;
	int retval;

	memset(pi_buff, 0x00, PAGE_SIZE);
	
	if (!head || !pi_buff)
		return -EINVAL;

	last_p_partition_ID = last_p_unit_address = ~0UL;
	INIT_LIST_HEAD(head);

	do {
		retval = hvcs_next_partner(unit_address, last_p_partition_ID,
				last_p_unit_address, pi_buff);
		if (retval) {
			if (!list_empty(head))
				return 0;
			return retval;
		}

		last_p_partition_ID = pi_buff[0];
		last_p_unit_address = pi_buff[1];

		
		if (last_p_partition_ID == ~0UL
				&& last_p_unit_address == ~0UL)
			break;

		next_partner_info = kmalloc(sizeof(struct hvcs_partner_info),
				GFP_ATOMIC);

		if (!next_partner_info) {
			printk(KERN_WARNING "HVCONSOLE: kmalloc() failed to"
				" allocate partner info struct.\n");
			hvcs_free_partner_info(head);
			return -ENOMEM;
		}

		next_partner_info->unit_address
			= (unsigned int)last_p_unit_address;
		next_partner_info->partition_ID
			= (unsigned int)last_p_partition_ID;

		
		strncpy(&next_partner_info->location_code[0],
			(char *)&pi_buff[2],
			strlen((char *)&pi_buff[2]) + 1);

		list_add_tail(&(next_partner_info->node), head);
		next_partner_info = NULL;

	} while (more);

	return 0;
}
EXPORT_SYMBOL(hvcs_get_partner_info);

int hvcs_register_connection( uint32_t unit_address,
		uint32_t p_partition_ID, uint32_t p_unit_address)
{
	long retval;
	retval = plpar_hcall_norets(H_REGISTER_VTERM, unit_address,
				p_partition_ID, p_unit_address);
	return hvcs_convert(retval);
}
EXPORT_SYMBOL(hvcs_register_connection);

int hvcs_free_connection(uint32_t unit_address)
{
	long retval;
	retval = plpar_hcall_norets(H_FREE_VTERM, unit_address);
	return hvcs_convert(retval);
}
EXPORT_SYMBOL(hvcs_free_connection);
