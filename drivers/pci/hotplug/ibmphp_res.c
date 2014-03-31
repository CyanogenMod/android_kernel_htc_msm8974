/*
 * IBM Hot Plug Controller Driver
 *
 * Written By: Irene Zubarev, IBM Corporation
 *
 * Copyright (C) 2001 Greg Kroah-Hartman (greg@kroah.com)
 * Copyright (C) 2001,2002 IBM Corp.
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Send feedback to <gregkh@us.ibm.com>
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/list.h>
#include <linux/init.h>
#include "ibmphp.h"

static int flags = 0;		

static void update_resources (struct bus_node *bus_cur, int type, int rangeno);
static int once_over (void);
static int remove_ranges (struct bus_node *, struct bus_node *);
static int update_bridge_ranges (struct bus_node **);
static int add_bus_range (int type, struct range_node *, struct bus_node *);
static void fix_resources (struct bus_node *);
static struct bus_node *find_bus_wprev (u8, struct bus_node **, u8);

static LIST_HEAD(gbuses);

static struct bus_node * __init alloc_error_bus (struct ebda_pci_rsrc * curr, u8 busno, int flag)
{
	struct bus_node * newbus;

	if (!(curr) && !(flag)) {
		err ("NULL pointer passed\n");
		return NULL;
	}

	newbus = kzalloc(sizeof(struct bus_node), GFP_KERNEL);
	if (!newbus) {
		err ("out of system memory\n");
		return NULL;
	}

	if (flag)
		newbus->busno = busno;
	else
		newbus->busno = curr->bus_num;
	list_add_tail (&newbus->bus_list, &gbuses);
	return newbus;
}

static struct resource_node * __init alloc_resources (struct ebda_pci_rsrc * curr)
{
	struct resource_node *rs;
	
	if (!curr) {
		err ("NULL passed to allocate\n");
		return NULL;
	}

	rs = kzalloc(sizeof(struct resource_node), GFP_KERNEL);
	if (!rs) {
		err ("out of system memory\n");
		return NULL;
	}
	rs->busno = curr->bus_num;
	rs->devfunc = curr->dev_fun;
	rs->start = curr->start_addr;
	rs->end = curr->end_addr;
	rs->len = curr->end_addr - curr->start_addr + 1;
	return rs;
}

static int __init alloc_bus_range (struct bus_node **new_bus, struct range_node **new_range, struct ebda_pci_rsrc *curr, int flag, u8 first_bus)
{
	struct bus_node * newbus;
	struct range_node *newrange;
	u8 num_ranges = 0;

	if (first_bus) {
		newbus = kzalloc(sizeof(struct bus_node), GFP_KERNEL);
		if (!newbus) {
			err ("out of system memory.\n");
			return -ENOMEM;
		}
		newbus->busno = curr->bus_num;
	} else {
		newbus = *new_bus;
		switch (flag) {
			case MEM:
				num_ranges = newbus->noMemRanges;
				break;
			case PFMEM:
				num_ranges = newbus->noPFMemRanges;
				break;
			case IO:
				num_ranges = newbus->noIORanges;
				break;
		}
	}

	newrange = kzalloc(sizeof(struct range_node), GFP_KERNEL);
	if (!newrange) {
		if (first_bus)
			kfree (newbus);
		err ("out of system memory\n");
		return -ENOMEM;
	}
	newrange->start = curr->start_addr;
	newrange->end = curr->end_addr;
		
	if (first_bus || (!num_ranges))
		newrange->rangeno = 1;
	else {
		
		add_bus_range (flag, newrange, newbus);
		debug ("%d resource Primary Bus inserted on bus %x [%x - %x]\n", flag, newbus->busno, newrange->start, newrange->end);
	}

	switch (flag) {
		case MEM:
			newbus->rangeMem = newrange;
			if (first_bus)
				newbus->noMemRanges = 1;
			else {
				debug ("First Memory Primary on bus %x, [%x - %x]\n", newbus->busno, newrange->start, newrange->end);
				++newbus->noMemRanges;
				fix_resources (newbus);
			}
			break;
		case IO:
			newbus->rangeIO = newrange;
			if (first_bus)
				newbus->noIORanges = 1;
			else {
				debug ("First IO Primary on bus %x, [%x - %x]\n", newbus->busno, newrange->start, newrange->end);
				++newbus->noIORanges;
				fix_resources (newbus);
			}
			break;
		case PFMEM:
			newbus->rangePFMem = newrange;
			if (first_bus)
				newbus->noPFMemRanges = 1;
			else {	
				debug ("1st PFMemory Primary on Bus %x [%x - %x]\n", newbus->busno, newrange->start, newrange->end);
				++newbus->noPFMemRanges;
				fix_resources (newbus);
			}

			break;
	}

	*new_bus = newbus;
	*new_range = newrange;
	return 0;
}



int __init ibmphp_rsrc_init (void)
{
	struct ebda_pci_rsrc *curr;
	struct range_node *newrange = NULL;
	struct bus_node *newbus = NULL;
	struct bus_node *bus_cur;
	struct bus_node *bus_prev;
	struct list_head *tmp;
	struct resource_node *new_io = NULL;
	struct resource_node *new_mem = NULL;
	struct resource_node *new_pfmem = NULL;
	int rc;
	struct list_head *tmp_ebda;

	list_for_each (tmp_ebda, &ibmphp_ebda_pci_rsrc_head) {
		curr = list_entry (tmp_ebda, struct ebda_pci_rsrc, ebda_pci_rsrc_list);
		if (!(curr->rsrc_type & PCIDEVMASK)) {
			
			debug ("this is not a PCI DEVICE in rsrc_init, please take care\n");
			
		}

		
		if (curr->rsrc_type & PRIMARYBUSMASK) {
			
			if ((curr->rsrc_type & RESTYPE) == MMASK) {
				
				if (list_empty (&gbuses)) {
					if ((rc = alloc_bus_range (&newbus, &newrange, curr, MEM, 1)))
						return rc;
					list_add_tail (&newbus->bus_list, &gbuses);
					debug ("gbuses = NULL, Memory Primary Bus %x [%x - %x]\n", newbus->busno, newrange->start, newrange->end);
				} else {
					bus_cur = find_bus_wprev (curr->bus_num, &bus_prev, 1);
					
					if (bus_cur) {
						rc = alloc_bus_range (&bus_cur, &newrange, curr, MEM, 0);
						if (rc)
							return rc;
					} else {
						
						if ((rc = alloc_bus_range (&newbus, &newrange, curr, MEM, 1)))
							return rc;

						list_add_tail (&newbus->bus_list, &gbuses);
						debug ("New Bus, Memory Primary Bus %x [%x - %x]\n", newbus->busno, newrange->start, newrange->end);
					}
				}
			} else if ((curr->rsrc_type & RESTYPE) == PFMASK) {
				
				if (list_empty (&gbuses)) {
					
					if ((rc = alloc_bus_range (&newbus, &newrange, curr, PFMEM, 1)))
						return rc;
					list_add_tail (&newbus->bus_list, &gbuses);
					debug ("gbuses = NULL, PFMemory Primary Bus %x [%x - %x]\n", newbus->busno, newrange->start, newrange->end);
				} else {
					bus_cur = find_bus_wprev (curr->bus_num, &bus_prev, 1);
					if (bus_cur) {
						
						rc = alloc_bus_range (&bus_cur, &newrange, curr, PFMEM, 0);
						if (rc)
							return rc;
					} else {
						
						if ((rc = alloc_bus_range (&newbus, &newrange, curr, PFMEM, 1)))
							return rc;
						list_add_tail (&newbus->bus_list, &gbuses);
						debug ("1st Bus, PFMemory Primary Bus %x [%x - %x]\n", newbus->busno, newrange->start, newrange->end);
					}
				}
			} else if ((curr->rsrc_type & RESTYPE) == IOMASK) {
				
				if (list_empty (&gbuses)) {
					
					if ((rc = alloc_bus_range (&newbus, &newrange, curr, IO, 1)))
						return rc;
					list_add_tail (&newbus->bus_list, &gbuses);
					debug ("gbuses = NULL, IO Primary Bus %x [%x - %x]\n", newbus->busno, newrange->start, newrange->end);
				} else {
					bus_cur = find_bus_wprev (curr->bus_num, &bus_prev, 1);
					if (bus_cur) {
						rc = alloc_bus_range (&bus_cur, &newrange, curr, IO, 0);
						if (rc)
							return rc;
					} else {
						
						if ((rc = alloc_bus_range (&newbus, &newrange, curr, IO, 1)))
							return rc;
						list_add_tail (&newbus->bus_list, &gbuses);
						debug ("1st Bus, IO Primary Bus %x [%x - %x]\n", newbus->busno, newrange->start, newrange->end);
					}
				}

			} else {
				;	
			}
		} else {
			
			if ((curr->rsrc_type & RESTYPE) == MMASK) {
				
				new_mem = alloc_resources (curr);
				if (!new_mem)
					return -ENOMEM;
				new_mem->type = MEM;
				if (ibmphp_add_resource (new_mem) < 0) {
					newbus = alloc_error_bus (curr, 0, 0);
					if (!newbus)
						return -ENOMEM;
					newbus->firstMem = new_mem;
					++newbus->needMemUpdate;
					new_mem->rangeno = -1;
				}
				debug ("Memory resource for device %x, bus %x, [%x - %x]\n", new_mem->devfunc, new_mem->busno, new_mem->start, new_mem->end);

			} else if ((curr->rsrc_type & RESTYPE) == PFMASK) {
				
				new_pfmem = alloc_resources (curr);
				if (!new_pfmem)
					return -ENOMEM;
				new_pfmem->type = PFMEM;
				new_pfmem->fromMem = 0;
				if (ibmphp_add_resource (new_pfmem) < 0) {
					newbus = alloc_error_bus (curr, 0, 0);
					if (!newbus)
						return -ENOMEM;
					newbus->firstPFMem = new_pfmem;
					++newbus->needPFMemUpdate;
					new_pfmem->rangeno = -1;
				}

				debug ("PFMemory resource for device %x, bus %x, [%x - %x]\n", new_pfmem->devfunc, new_pfmem->busno, new_pfmem->start, new_pfmem->end);
			} else if ((curr->rsrc_type & RESTYPE) == IOMASK) {
				
				new_io = alloc_resources (curr);
				if (!new_io)
					return -ENOMEM;
				new_io->type = IO;

				if (ibmphp_add_resource (new_io) < 0) {
					newbus = alloc_error_bus (curr, 0, 0);
					if (!newbus)
						return -ENOMEM;
					newbus->firstIO = new_io;
					++newbus->needIOUpdate;
					new_io->rangeno = -1;
				}
				debug ("IO resource for device %x, bus %x, [%x - %x]\n", new_io->devfunc, new_io->busno, new_io->start, new_io->end);
			}
		}
	}

	list_for_each (tmp, &gbuses) {
		bus_cur = list_entry (tmp, struct bus_node, bus_list);
		
		rc = update_bridge_ranges (&bus_cur);
		if (rc)
			return rc;
	}
	rc = once_over ();  
	if (rc)
		return rc;
	return 0;
}

static int add_bus_range (int type, struct range_node *range, struct bus_node *bus_cur)
{
	struct range_node *range_cur = NULL;
	struct range_node *range_prev;
	int count = 0, i_init;
	int noRanges = 0;

	switch (type) {
		case MEM:
			range_cur = bus_cur->rangeMem;
			noRanges = bus_cur->noMemRanges;
			break;
		case PFMEM:
			range_cur = bus_cur->rangePFMem;
			noRanges = bus_cur->noPFMemRanges;
			break;
		case IO:
			range_cur = bus_cur->rangeIO;
			noRanges = bus_cur->noIORanges;
			break;
	}

	range_prev = NULL;
	while (range_cur) {
		if (range->start < range_cur->start)
			break;
		range_prev = range_cur;
		range_cur = range_cur->next;
		count = count + 1;
	}
	if (!count) {
		
		switch (type) {
			case MEM:
				bus_cur->rangeMem = range;
				break;
			case PFMEM:
				bus_cur->rangePFMem = range;
				break;
			case IO:
				bus_cur->rangeIO = range;
				break;
		}
		range->next = range_cur;
		range->rangeno = 1;
		i_init = 0;
	} else if (!range_cur) {
		
		range->next = NULL;
		range_prev->next = range;
		range->rangeno = range_prev->rangeno + 1;
		return 0;
	} else {
		
		range_prev->next = range;
		range->next = range_cur;
		range->rangeno = range_cur->rangeno;
		i_init = range_prev->rangeno;
	}

	for (count = i_init; count < noRanges; ++count) {
		++range_cur->rangeno;
		range_cur = range_cur->next;
	}

	update_resources (bus_cur, type, i_init + 1);
	return 0;
}

static void update_resources (struct bus_node *bus_cur, int type, int rangeno)
{
	struct resource_node *res = NULL;
	u8 eol = 0;	

	switch (type) {
		case MEM:
			if (bus_cur->firstMem) 
				res = bus_cur->firstMem;
			break;
		case PFMEM:
			if (bus_cur->firstPFMem)
				res = bus_cur->firstPFMem;
			break;
		case IO:
			if (bus_cur->firstIO)
				res = bus_cur->firstIO;
			break;
	}

	if (res) {
		while (res) {
			if (res->rangeno == rangeno)
				break;
			if (res->next)
				res = res->next;
			else if (res->nextRange)
				res = res->nextRange;
			else {
				eol = 1;
				break;
			}
		}

		if (!eol) {
			
			while (res) {
				++res->rangeno;
				res = res->next;
			}
		}
	}
}

static void fix_me (struct resource_node *res, struct bus_node *bus_cur, struct range_node *range)
{
	char * str = "";
	switch (res->type) {
		case IO:
			str = "io";
			break;
		case MEM:
			str = "mem";
			break;
		case PFMEM:
			str = "pfmem";
			break;
	}

	while (res) {
		if (res->rangeno == -1) {
			while (range) {
				if ((res->start >= range->start) && (res->end <= range->end)) {
					res->rangeno = range->rangeno;
					debug ("%s->rangeno in fix_resources is %d\n", str, res->rangeno);
					switch (res->type) {
						case IO:
							--bus_cur->needIOUpdate;
							break;
						case MEM:
							--bus_cur->needMemUpdate;
							break;
						case PFMEM:
							--bus_cur->needPFMemUpdate;
							break;
					}
					break;
				}
				range = range->next;
			}
		}
		if (res->next)
			res = res->next;
		else
			res = res->nextRange;
	}

}

static void fix_resources (struct bus_node *bus_cur)
{
	struct range_node *range;
	struct resource_node *res;

	debug ("%s - bus_cur->busno = %d\n", __func__, bus_cur->busno);

	if (bus_cur->needIOUpdate) {
		res = bus_cur->firstIO;
		range = bus_cur->rangeIO;
		fix_me (res, bus_cur, range);
	}
	if (bus_cur->needMemUpdate) {
		res = bus_cur->firstMem;
		range = bus_cur->rangeMem;
		fix_me (res, bus_cur, range);
	}
	if (bus_cur->needPFMemUpdate) {
		res = bus_cur->firstPFMem;
		range = bus_cur->rangePFMem;
		fix_me (res, bus_cur, range);
	}
}

int ibmphp_add_resource (struct resource_node *res)
{
	struct resource_node *res_cur;
	struct resource_node *res_prev;
	struct bus_node *bus_cur;
	struct range_node *range_cur = NULL;
	struct resource_node *res_start = NULL;

	debug ("%s - enter\n", __func__);

	if (!res) {
		err ("NULL passed to add\n");
		return -ENODEV;
	}
	
	bus_cur = find_bus_wprev (res->busno, NULL, 0);
	
	if (!bus_cur) {
		
		debug ("no bus in the system, either pci_dev's wrong or allocation failed\n");
		return -ENODEV;
	}

	
	switch (res->type) {
		case IO:
			range_cur = bus_cur->rangeIO;
			res_start = bus_cur->firstIO;
			break;
		case MEM:
			range_cur = bus_cur->rangeMem;
			res_start = bus_cur->firstMem;
			break;
		case PFMEM:
			range_cur = bus_cur->rangePFMem;
			res_start = bus_cur->firstPFMem;
			break;
		default:
			err ("cannot read the type of the resource to add... problem\n");
			return -EINVAL;
	}
	while (range_cur) {
		if ((res->start >= range_cur->start) && (res->end <= range_cur->end)) {
			res->rangeno = range_cur->rangeno;
			break;
		}
		range_cur = range_cur->next;
	}


	if (!range_cur) {
		switch (res->type) {
			case IO:
				++bus_cur->needIOUpdate;					
				break;
			case MEM:
				++bus_cur->needMemUpdate;
				break;
			case PFMEM:
				++bus_cur->needPFMemUpdate;
				break;
		}
		res->rangeno = -1;
	}
	
	debug ("The range is %d\n", res->rangeno);
	if (!res_start) {
		
		switch (res->type) {
			case IO:
				bus_cur->firstIO = res;					
				break;
			case MEM:
				bus_cur->firstMem = res;
				break;
			case PFMEM:
				bus_cur->firstPFMem = res;
				break;
		}	
		res->next = NULL;
		res->nextRange = NULL;
	} else {
		res_cur = res_start;
		res_prev = NULL;

		debug ("res_cur->rangeno is %d\n", res_cur->rangeno);

		while (res_cur) {
			if (res_cur->rangeno >= res->rangeno)
				break;
			res_prev = res_cur;
			if (res_cur->next)
				res_cur = res_cur->next;
			else
				res_cur = res_cur->nextRange;
		}

		if (!res_cur) {
			
			debug ("i should be here, [%x - %x]\n", res->start, res->end);
			res_prev->nextRange = res;
			res->next = NULL;
			res->nextRange = NULL;
		} else if (res_cur->rangeno == res->rangeno) {
			
			while (res_cur) {
				if (res->start < res_cur->start)
					break;
				res_prev = res_cur;
				res_cur = res_cur->next;
			}
			if (!res_cur) {
				
				res_prev->next = res;
				res->next = NULL;
				res->nextRange = res_prev->nextRange;
				res_prev->nextRange = NULL;
			} else if (res->start < res_cur->start) {
				
				if (!res_prev)	{
					switch (res->type) {
						case IO:
							bus_cur->firstIO = res;
							break;
						case MEM:
							bus_cur->firstMem = res;
							break;
						case PFMEM:
							bus_cur->firstPFMem = res;
							break;
					}
				} else if (res_prev->rangeno == res_cur->rangeno)
					res_prev->next = res;
				else
					res_prev->nextRange = res;

				res->next = res_cur;
				res->nextRange = NULL;
			}
		} else {
			
			if (!res_prev) {
				
				res->next = NULL;
				switch (res->type) {
					case IO:
						res->nextRange = bus_cur->firstIO;
						bus_cur->firstIO = res;
						break;
					case MEM:
						res->nextRange = bus_cur->firstMem;
						bus_cur->firstMem = res;
						break;
					case PFMEM:
						res->nextRange = bus_cur->firstPFMem;
						bus_cur->firstPFMem = res;
						break;
				}
			} else if (res_cur->rangeno > res->rangeno) {
				
				res_prev->nextRange = res;
				res->next = NULL;
				res->nextRange = res_cur;
			}
		}
	}

	debug ("%s - exit\n", __func__);
	return 0;
}

int ibmphp_remove_resource (struct resource_node *res)
{
	struct bus_node *bus_cur;
	struct resource_node *res_cur = NULL;
	struct resource_node *res_prev;
	struct resource_node *mem_cur;
	char * type = "";

	if (!res)  {
		err ("resource to remove is NULL\n");
		return -ENODEV;
	}

	bus_cur = find_bus_wprev (res->busno, NULL, 0);

	if (!bus_cur) {
		err ("cannot find corresponding bus of the io resource to remove  "
			"bailing out...\n");
		return -ENODEV;
	}

	switch (res->type) {
		case IO:
			res_cur = bus_cur->firstIO;
			type = "io";
			break;
		case MEM:
			res_cur = bus_cur->firstMem;
			type = "mem";
			break;
		case PFMEM:
			res_cur = bus_cur->firstPFMem;
			type = "pfmem";
			break;
		default:
			err ("unknown type for resource to remove\n");
			return -EINVAL;
	}
	res_prev = NULL;

	while (res_cur) {
		if ((res_cur->start == res->start) && (res_cur->end == res->end))
			break;
		res_prev = res_cur;
		if (res_cur->next)
			res_cur = res_cur->next;
		else
			res_cur = res_cur->nextRange;
	}

	if (!res_cur) {
		if (res->type == PFMEM) {
			res_cur = bus_cur->firstPFMemFromMem;
			res_prev = NULL;

			while (res_cur) {
				if ((res_cur->start == res->start) && (res_cur->end == res->end)) {
					mem_cur = bus_cur->firstMem;
					while (mem_cur) {
						if ((mem_cur->start == res_cur->start)
						    && (mem_cur->end == res_cur->end))
							break;
						if (mem_cur->next)
							mem_cur = mem_cur->next;
						else
							mem_cur = mem_cur->nextRange;
					}
					if (!mem_cur) {
						err ("cannot find corresponding mem node for pfmem...\n");
						return -EINVAL;
					}

					ibmphp_remove_resource (mem_cur);
					if (!res_prev)
						bus_cur->firstPFMemFromMem = res_cur->next;
					else
						res_prev->next = res_cur->next;
					kfree (res_cur);
					return 0;
				}
				res_prev = res_cur;
				if (res_cur->next)
					res_cur = res_cur->next;
				else
					res_cur = res_cur->nextRange;
			}
			if (!res_cur) {
				err ("cannot find pfmem to delete...\n");
				return -EINVAL;
			}
		} else {
			err ("the %s resource is not in the list to be deleted...\n", type);
			return -EINVAL;
		}
	}
	if (!res_prev) {
		
		if (res_cur->next) {
			switch (res->type) {
				case IO:
					bus_cur->firstIO = res_cur->next;
					break;
				case MEM:
					bus_cur->firstMem = res_cur->next;
					break;
				case PFMEM:
					bus_cur->firstPFMem = res_cur->next;
					break;
			}
		} else if (res_cur->nextRange) {
			switch (res->type) {
				case IO:
					bus_cur->firstIO = res_cur->nextRange;
					break;
				case MEM:
					bus_cur->firstMem = res_cur->nextRange;
					break;
				case PFMEM:
					bus_cur->firstPFMem = res_cur->nextRange;
					break;
			}
		} else {
			switch (res->type) {
				case IO:
					bus_cur->firstIO = NULL;
					break;
				case MEM:
					bus_cur->firstMem = NULL;
					break;
				case PFMEM:
					bus_cur->firstPFMem = NULL;
					break;
			}
		}
		kfree (res_cur);
		return 0;
	} else {
		if (res_cur->next) {
			if (res_prev->rangeno == res_cur->rangeno)
				res_prev->next = res_cur->next;
			else
				res_prev->nextRange = res_cur->next;
		} else if (res_cur->nextRange) {
			res_prev->next = NULL;
			res_prev->nextRange = res_cur->nextRange;
		} else {
			res_prev->next = NULL;
			res_prev->nextRange = NULL;
		}
		kfree (res_cur);
		return 0;
	}

	return 0;
}

static struct range_node * find_range (struct bus_node *bus_cur, struct resource_node * res)
{
	struct range_node * range = NULL;

	switch (res->type) {
		case IO:
			range = bus_cur->rangeIO;
			break;
		case MEM:
			range = bus_cur->rangeMem;
			break;
		case PFMEM:
			range = bus_cur->rangePFMem;
			break;
		default:
			err ("cannot read resource type in find_range\n");
	}

	while (range) {
		if (res->rangeno == range->rangeno)
			break;
		range = range->next;
	}
	return range;
}

int ibmphp_check_resource (struct resource_node *res, u8 bridge)
{
	struct bus_node *bus_cur;
	struct range_node *range = NULL;
	struct resource_node *res_prev;
	struct resource_node *res_cur = NULL;
	u32 len_cur = 0, start_cur = 0, len_tmp = 0;
	int noranges = 0;
	u32 tmp_start;		
	u32 tmp_divide;
	u8 flag = 0;

	if (!res)
		return -EINVAL;

	if (bridge) {
		
		if (res->type == IO)
			tmp_divide = IOBRIDGE;
		else
			tmp_divide = MEMBRIDGE;
	} else
		tmp_divide = res->len;

	bus_cur = find_bus_wprev (res->busno, NULL, 0);

	if (!bus_cur) {
		
		debug ("no bus in the system, either pci_dev's wrong or allocation failed\n");
		return -EINVAL;
	}

	debug ("%s - enter\n", __func__);
	debug ("bus_cur->busno is %d\n", bus_cur->busno);

	res->len -= 1;

	switch (res->type) {
		case IO:
			res_cur = bus_cur->firstIO;
			noranges = bus_cur->noIORanges;
			break;
		case MEM:
			res_cur = bus_cur->firstMem;
			noranges = bus_cur->noMemRanges;
			break;
		case PFMEM:
			res_cur = bus_cur->firstPFMem;
			noranges = bus_cur->noPFMemRanges;
			break;
		default:
			err ("wrong type of resource to check\n");
			return -EINVAL;
	}
	res_prev = NULL;

	while (res_cur) {
		range = find_range (bus_cur, res_cur);
		debug ("%s - rangeno = %d\n", __func__, res_cur->rangeno);

		if (!range) {
			err ("no range for the device exists... bailing out...\n");
			return -EINVAL;
		}

		
		if (!res_prev) {
			
			if ((res_cur->start != range->start) && ((len_tmp = res_cur->start - 1 - range->start) >= res->len)) {
				debug ("len_tmp = %x\n", len_tmp);

				if ((len_tmp < len_cur) || (len_cur == 0)) {

					if ((range->start % tmp_divide) == 0) {
						
						flag = 1;
						len_cur = len_tmp;
						start_cur = range->start;
					} else {
						
						tmp_start = range->start;
						flag = 0;

						while ((len_tmp = res_cur->start - 1 - tmp_start) >= res->len) {
							if ((tmp_start % tmp_divide) == 0) {
								flag = 1;
								len_cur = len_tmp;
								start_cur = tmp_start;
								break;
							}
							tmp_start += tmp_divide - tmp_start % tmp_divide;
							if (tmp_start >= res_cur->start - 1)
								break;
						}
					}
			
					if (flag && len_cur == res->len) {
						debug ("but we are not here, right?\n");
						res->start = start_cur;
						res->len += 1; 
						res->end = res->start + res->len - 1;
						return 0;
					}
				}
			}
		}
		if (!res_cur->next) {
			
			if ((range->end != res_cur->end) && ((len_tmp = range->end - (res_cur->end + 1)) >= res->len)) {
				debug ("len_tmp = %x\n", len_tmp);
				if ((len_tmp < len_cur) || (len_cur == 0)) {

					if (((res_cur->end + 1) % tmp_divide) == 0) {
						
						flag = 1;
						len_cur = len_tmp;
						start_cur = res_cur->end + 1;
					} else {
						
						tmp_start = res_cur->end + 1;
						flag = 0;

						while ((len_tmp = range->end - tmp_start) >= res->len) {
							if ((tmp_start % tmp_divide) == 0) {
								flag = 1;
								len_cur = len_tmp;
								start_cur = tmp_start;
								break;
							}
							tmp_start += tmp_divide - tmp_start % tmp_divide;
							if (tmp_start >= range->end)
								break;
						}
					}
					if (flag && len_cur == res->len) {
						res->start = start_cur;
						res->len += 1; 
						res->end = res->start + res->len - 1;
						return 0;
					}
				}
			}
		}

		if (res_prev) {
			if (res_prev->rangeno != res_cur->rangeno) {
				
				if ((res_cur->start != range->start) && 
					((len_tmp = res_cur->start - 1 - range->start) >= res->len)) {
					if ((len_tmp < len_cur) || (len_cur == 0)) {
						if ((range->start % tmp_divide) == 0) {	
							
							flag = 1;
							len_cur = len_tmp;
							start_cur = range->start;
						} else {
							
							tmp_start = range->start;
							flag = 0;

							while ((len_tmp = res_cur->start - 1 - tmp_start) >= res->len) {
								if ((tmp_start % tmp_divide) == 0) {
									flag = 1;
									len_cur = len_tmp;
									start_cur = tmp_start;
									break;
								}
								tmp_start += tmp_divide - tmp_start % tmp_divide;
								if (tmp_start >= res_cur->start - 1)
									break;
							}
						}

						if (flag && len_cur == res->len) {
							res->start = start_cur;
							res->len += 1; 
							res->end = res->start + res->len - 1;
							return 0;
						}
					}
				}
			} else {
				
				if ((len_tmp = res_cur->start - 1 - res_prev->end - 1) >= res->len) {
					if ((len_tmp < len_cur) || (len_cur == 0)) {
						if (((res_prev->end + 1) % tmp_divide) == 0) {
							
							flag = 1;
							len_cur = len_tmp;
							start_cur = res_prev->end + 1;
						} else {
							
							tmp_start = res_prev->end + 1;
							flag = 0;

							while ((len_tmp = res_cur->start - 1 - tmp_start) >= res->len) {
								if ((tmp_start % tmp_divide) == 0) {
									flag = 1;
									len_cur = len_tmp;
									start_cur = tmp_start;
									break;
								}
								tmp_start += tmp_divide - tmp_start % tmp_divide;
								if (tmp_start >= res_cur->start - 1)
									break;
							}
						}

						if (flag && len_cur == res->len) {
							res->start = start_cur;
							res->len += 1; 
							res->end = res->start + res->len - 1;
							return 0;
						}
					}
				}
			}
		}
		
		res_prev = res_cur;
		if (res_cur->next)
			res_cur = res_cur->next;
		else
			res_cur = res_cur->nextRange;
	}	


	if (!res_prev) {
		
		
		switch (res->type) {
			case IO:
				range = bus_cur->rangeIO;
				break;
			case MEM:
				range = bus_cur->rangeMem;
				break;
			case PFMEM:
				range = bus_cur->rangePFMem;
				break;
		}
		while (range) {
			if ((len_tmp = range->end - range->start) >= res->len) {
				if ((len_tmp < len_cur) || (len_cur == 0)) {
					if ((range->start % tmp_divide) == 0) {
						
						flag = 1;
						len_cur = len_tmp;
						start_cur = range->start;
					} else {
						
						tmp_start = range->start;
						flag = 0;

						while ((len_tmp = range->end - tmp_start) >= res->len) {
							if ((tmp_start % tmp_divide) == 0) {
								flag = 1;
								len_cur = len_tmp;
								start_cur = tmp_start;
								break;
							}
							tmp_start += tmp_divide - tmp_start % tmp_divide;
							if (tmp_start >= range->end)
								break;
						}
					}

					if (flag && len_cur == res->len) {
						res->start = start_cur;
						res->len += 1; 
						res->end = res->start + res->len - 1;
						return 0;
					}
				}
			}
			range = range->next;
		}		

		if ((!range) && (len_cur == 0)) {
			
			err ("no appropriate range.. bailing out...\n");
			return -EINVAL;
		} else if (len_cur) {
			res->start = start_cur;
			res->len += 1; 
			res->end = res->start + res->len - 1;
			return 0;
		}
	}

	if (!res_cur) {
		debug ("prev->rangeno = %d, noranges = %d\n", res_prev->rangeno, noranges);
		if (res_prev->rangeno < noranges) {
			
			switch (res->type) {
				case IO:
					range = bus_cur->rangeIO;
					break;
				case MEM:
					range = bus_cur->rangeMem;
					break;
				case PFMEM:
					range = bus_cur->rangePFMem;
					break;
			}
			while (range) {
				if ((len_tmp = range->end - range->start) >= res->len) {
					if ((len_tmp < len_cur) || (len_cur == 0)) {
						if ((range->start % tmp_divide) == 0) {
							
							flag = 1;
							len_cur = len_tmp;
							start_cur = range->start;
						} else {
							
							tmp_start = range->start;
							flag = 0;

							while ((len_tmp = range->end - tmp_start) >= res->len) {
								if ((tmp_start % tmp_divide) == 0) {
									flag = 1;
									len_cur = len_tmp;
									start_cur = tmp_start;
									break;
								}
								tmp_start += tmp_divide - tmp_start % tmp_divide;
								if (tmp_start >= range->end)
									break;
							}
						}

						if (flag && len_cur == res->len) {
							res->start = start_cur;
							res->len += 1; 
							res->end = res->start + res->len - 1;
							return 0;
						}
					}
				}
				range = range->next;
			}	

			if ((!range) && (len_cur == 0)) {
				
				err ("no appropriate range.. bailing out...\n");
				return -EINVAL;
			} else if (len_cur) {
				res->start = start_cur;
				res->len += 1; 
				res->end = res->start + res->len - 1;
				return 0;
			}
		} else {
			
			if (len_cur) {
				res->start = start_cur;
				res->len += 1; 
				res->end = res->start + res->len - 1;
				return 0;
			} else {
				
				err ("no appropriate range.. bailing out...\n");
				return -EINVAL;
			}
		}
	}	
	return -EINVAL;
}

int ibmphp_remove_bus (struct bus_node *bus, u8 parent_busno)
{
	struct resource_node *res_cur;
	struct resource_node *res_tmp;
	struct bus_node *prev_bus;
	int rc;

	prev_bus = find_bus_wprev (parent_busno, NULL, 0);	

	if (!prev_bus) {
		debug ("something terribly wrong. Cannot find parent bus to the one to remove\n");
		return -ENODEV;
	}

	debug ("In ibmphp_remove_bus... prev_bus->busno is %x\n", prev_bus->busno);

	rc = remove_ranges (bus, prev_bus);
	if (rc)
		return rc;

	if (bus->firstIO) {
		res_cur = bus->firstIO;
		while (res_cur) {
			res_tmp = res_cur;
			if (res_cur->next)
				res_cur = res_cur->next;
			else
				res_cur = res_cur->nextRange;
			kfree (res_tmp);
			res_tmp = NULL;
		}
		bus->firstIO = NULL;
	}
	if (bus->firstMem) {
		res_cur = bus->firstMem;
		while (res_cur) {
			res_tmp = res_cur;
			if (res_cur->next)
				res_cur = res_cur->next;
			else
				res_cur = res_cur->nextRange;
			kfree (res_tmp);
			res_tmp = NULL;
		}
		bus->firstMem = NULL;
	}
	if (bus->firstPFMem) {
		res_cur = bus->firstPFMem;
		while (res_cur) {
			res_tmp = res_cur;
			if (res_cur->next)
				res_cur = res_cur->next;
			else
				res_cur = res_cur->nextRange;
			kfree (res_tmp);
			res_tmp = NULL;
		}
		bus->firstPFMem = NULL;
	}

	if (bus->firstPFMemFromMem) {
		res_cur = bus->firstPFMemFromMem;
		while (res_cur) {
			res_tmp = res_cur;
			res_cur = res_cur->next;

			kfree (res_tmp);
			res_tmp = NULL;
		}
		bus->firstPFMemFromMem = NULL;
	}

	list_del (&bus->bus_list);
	kfree (bus);
	return 0;
}

static int remove_ranges (struct bus_node *bus_cur, struct bus_node *bus_prev)
{
	struct range_node *range_cur;
	struct range_node *range_tmp;
	int i;
	struct resource_node *res = NULL;

	if (bus_cur->noIORanges) {
		range_cur = bus_cur->rangeIO;
		for (i = 0; i < bus_cur->noIORanges; i++) {
			if (ibmphp_find_resource (bus_prev, range_cur->start, &res, IO) < 0)
				return -EINVAL;
			ibmphp_remove_resource (res);

			range_tmp = range_cur;
			range_cur = range_cur->next;
			kfree (range_tmp);
			range_tmp = NULL;
		}
		bus_cur->rangeIO = NULL;
	}
	if (bus_cur->noMemRanges) {
		range_cur = bus_cur->rangeMem;
		for (i = 0; i < bus_cur->noMemRanges; i++) {
			if (ibmphp_find_resource (bus_prev, range_cur->start, &res, MEM) < 0) 
				return -EINVAL;

			ibmphp_remove_resource (res);
			range_tmp = range_cur;
			range_cur = range_cur->next;
			kfree (range_tmp);
			range_tmp = NULL;
		}
		bus_cur->rangeMem = NULL;
	}
	if (bus_cur->noPFMemRanges) {
		range_cur = bus_cur->rangePFMem;
		for (i = 0; i < bus_cur->noPFMemRanges; i++) {
			if (ibmphp_find_resource (bus_prev, range_cur->start, &res, PFMEM) < 0) 
				return -EINVAL;

			ibmphp_remove_resource (res);
			range_tmp = range_cur;
			range_cur = range_cur->next;
			kfree (range_tmp);
			range_tmp = NULL;
		}
		bus_cur->rangePFMem = NULL;
	}
	return 0;
}

int ibmphp_find_resource (struct bus_node *bus, u32 start_address, struct resource_node **res, int flag)
{
	struct resource_node *res_cur = NULL;
	char * type = "";

	if (!bus) {
		err ("The bus passed in NULL to find resource\n");
		return -ENODEV;
	}

	switch (flag) {
		case IO:
			res_cur = bus->firstIO;
			type = "io";
			break;
		case MEM:
			res_cur = bus->firstMem;
			type = "mem";
			break;
		case PFMEM:
			res_cur = bus->firstPFMem;
			type = "pfmem";
			break;
		default:
			err ("wrong type of flag\n");
			return -EINVAL;
	}
	
	while (res_cur) {
		if (res_cur->start == start_address) {
			*res = res_cur;
			break;
		}
		if (res_cur->next)
			res_cur = res_cur->next;
		else
			res_cur = res_cur->nextRange;
	}

	if (!res_cur) {
		if (flag == PFMEM) {
			res_cur = bus->firstPFMemFromMem;
			while (res_cur) {
				if (res_cur->start == start_address) {
					*res = res_cur;
					break;
				}
				res_cur = res_cur->next;
			}
			if (!res_cur) {
				debug ("SOS...cannot find %s resource in the bus.\n", type);
				return -EINVAL;
			}
		} else {
			debug ("SOS... cannot find %s resource in the bus.\n", type);
			return -EINVAL;
		}
	}

	if (*res)
		debug ("*res->start = %x\n", (*res)->start);

	return 0;
}

void ibmphp_free_resources (void)
{
	struct bus_node *bus_cur = NULL;
	struct bus_node *bus_tmp;
	struct range_node *range_cur;
	struct range_node *range_tmp;
	struct resource_node *res_cur;
	struct resource_node *res_tmp;
	struct list_head *tmp;
	struct list_head *next;
	int i = 0;
	flags = 1;

	list_for_each_safe (tmp, next, &gbuses) {
		bus_cur = list_entry (tmp, struct bus_node, bus_list);
		if (bus_cur->noIORanges) {
			range_cur = bus_cur->rangeIO;
			for (i = 0; i < bus_cur->noIORanges; i++) {
				if (!range_cur)
					break;
				range_tmp = range_cur;
				range_cur = range_cur->next;
				kfree (range_tmp);
				range_tmp = NULL;
			}
		}
		if (bus_cur->noMemRanges) {
			range_cur = bus_cur->rangeMem;
			for (i = 0; i < bus_cur->noMemRanges; i++) {
				if (!range_cur)
					break;
				range_tmp = range_cur;
				range_cur = range_cur->next;
				kfree (range_tmp);
				range_tmp = NULL;
			}
		}
		if (bus_cur->noPFMemRanges) {
			range_cur = bus_cur->rangePFMem;
			for (i = 0; i < bus_cur->noPFMemRanges; i++) {
				if (!range_cur)
					break;
				range_tmp = range_cur;
				range_cur = range_cur->next;
				kfree (range_tmp);
				range_tmp = NULL;
			}
		}

		if (bus_cur->firstIO) {
			res_cur = bus_cur->firstIO;
			while (res_cur) {
				res_tmp = res_cur;
				if (res_cur->next)
					res_cur = res_cur->next;
				else
					res_cur = res_cur->nextRange;
				kfree (res_tmp);
				res_tmp = NULL;
			}
			bus_cur->firstIO = NULL;
		}
		if (bus_cur->firstMem) {
			res_cur = bus_cur->firstMem;
			while (res_cur) {
				res_tmp = res_cur;
				if (res_cur->next)
					res_cur = res_cur->next;
				else
					res_cur = res_cur->nextRange;
				kfree (res_tmp);
				res_tmp = NULL;
			}
			bus_cur->firstMem = NULL;
		}
		if (bus_cur->firstPFMem) {
			res_cur = bus_cur->firstPFMem;
			while (res_cur) {
				res_tmp = res_cur;
				if (res_cur->next)
					res_cur = res_cur->next;
				else
					res_cur = res_cur->nextRange;
				kfree (res_tmp);
				res_tmp = NULL;
			}
			bus_cur->firstPFMem = NULL;
		}

		if (bus_cur->firstPFMemFromMem) {
			res_cur = bus_cur->firstPFMemFromMem;
			while (res_cur) {
				res_tmp = res_cur;
				res_cur = res_cur->next;

				kfree (res_tmp);
				res_tmp = NULL;
			}
			bus_cur->firstPFMemFromMem = NULL;
		}

		bus_tmp = bus_cur;
		list_del (&bus_cur->bus_list);
		kfree (bus_tmp);
		bus_tmp = NULL;
	}
}

static int __init once_over (void)
{
	struct resource_node *pfmem_cur;
	struct resource_node *pfmem_prev;
	struct resource_node *mem;
	struct bus_node *bus_cur;
	struct list_head *tmp;

	list_for_each (tmp, &gbuses) {
		bus_cur = list_entry (tmp, struct bus_node, bus_list);
		if ((!bus_cur->rangePFMem) && (bus_cur->firstPFMem)) {
			for (pfmem_cur = bus_cur->firstPFMem, pfmem_prev = NULL; pfmem_cur; pfmem_prev = pfmem_cur, pfmem_cur = pfmem_cur->next) {
				pfmem_cur->fromMem = 1;
				if (pfmem_prev)
					pfmem_prev->next = pfmem_cur->next;
				else
					bus_cur->firstPFMem = pfmem_cur->next;

				if (!bus_cur->firstPFMemFromMem)
					pfmem_cur->next = NULL;
				else
					pfmem_cur->next = bus_cur->firstPFMemFromMem;

				bus_cur->firstPFMemFromMem = pfmem_cur;

				mem = kzalloc(sizeof(struct resource_node), GFP_KERNEL);
				if (!mem) {
					err ("out of system memory\n");
					return -ENOMEM;
				}
				mem->type = MEM;
				mem->busno = pfmem_cur->busno;
				mem->devfunc = pfmem_cur->devfunc;
				mem->start = pfmem_cur->start;
				mem->end = pfmem_cur->end;
				mem->len = pfmem_cur->len;
				if (ibmphp_add_resource (mem) < 0)
					err ("Trouble...trouble... EBDA allocated pfmem from mem, but system doesn't display it has this space... unless not PCI device...\n");
				pfmem_cur->rangeno = mem->rangeno;
			}	
		}	
	}	
	return 0; 
}

int ibmphp_add_pfmem_from_mem (struct resource_node *pfmem)
{
	struct bus_node *bus_cur = find_bus_wprev (pfmem->busno, NULL, 0);

	if (!bus_cur) {
		err ("cannot find bus of pfmem to add...\n");
		return -ENODEV;
	}

	if (bus_cur->firstPFMemFromMem)
		pfmem->next = bus_cur->firstPFMemFromMem;
	else
		pfmem->next = NULL;

	bus_cur->firstPFMemFromMem = pfmem;

	return 0;
}

struct bus_node *ibmphp_find_res_bus (u8 bus_number)
{
	return find_bus_wprev (bus_number, NULL, 0);
}

static struct bus_node *find_bus_wprev (u8 bus_number, struct bus_node **prev, u8 flag)
{
	struct bus_node *bus_cur;
	struct list_head *tmp;
	struct list_head *tmp_prev;

	list_for_each (tmp, &gbuses) {
		tmp_prev = tmp->prev;
		bus_cur = list_entry (tmp, struct bus_node, bus_list);
		if (flag) 
			*prev = list_entry (tmp_prev, struct bus_node, bus_list);
		if (bus_cur->busno == bus_number) 
			return bus_cur;
	}

	return NULL;
}

void ibmphp_print_test (void)
{
	int i = 0;
	struct bus_node *bus_cur = NULL;
	struct range_node *range;
	struct resource_node *res;
	struct list_head *tmp;
	
	debug_pci ("*****************START**********************\n");

	if ((!list_empty(&gbuses)) && flags) {
		err ("The GBUSES is not NULL?!?!?!?!?\n");
		return;
	}

	list_for_each (tmp, &gbuses) {
		bus_cur = list_entry (tmp, struct bus_node, bus_list);
		debug_pci ("This is bus # %d.  There are\n", bus_cur->busno);
		debug_pci ("IORanges = %d\t", bus_cur->noIORanges);
		debug_pci ("MemRanges = %d\t", bus_cur->noMemRanges);
		debug_pci ("PFMemRanges = %d\n", bus_cur->noPFMemRanges);
		debug_pci ("The IO Ranges are as follows:\n");
		if (bus_cur->rangeIO) {
			range = bus_cur->rangeIO;
			for (i = 0; i < bus_cur->noIORanges; i++) {
				debug_pci ("rangeno is %d\n", range->rangeno);
				debug_pci ("[%x - %x]\n", range->start, range->end);
				range = range->next;
			}
		}

		debug_pci ("The Mem Ranges are as follows:\n");
		if (bus_cur->rangeMem) {
			range = bus_cur->rangeMem;
			for (i = 0; i < bus_cur->noMemRanges; i++) {
				debug_pci ("rangeno is %d\n", range->rangeno);
				debug_pci ("[%x - %x]\n", range->start, range->end);
				range = range->next;
			}
		}

		debug_pci ("The PFMem Ranges are as follows:\n");

		if (bus_cur->rangePFMem) {
			range = bus_cur->rangePFMem;
			for (i = 0; i < bus_cur->noPFMemRanges; i++) {
				debug_pci ("rangeno is %d\n", range->rangeno);
				debug_pci ("[%x - %x]\n", range->start, range->end);
				range = range->next;
			}
		}

		debug_pci ("The resources on this bus are as follows\n");

		debug_pci ("IO...\n");
		if (bus_cur->firstIO) {
			res = bus_cur->firstIO;
			while (res) {
				debug_pci ("The range # is %d\n", res->rangeno);
				debug_pci ("The bus, devfnc is %d, %x\n", res->busno, res->devfunc);
				debug_pci ("[%x - %x], len=%x\n", res->start, res->end, res->len);
				if (res->next)
					res = res->next;
				else if (res->nextRange)
					res = res->nextRange;
				else
					break;
			}
		}
		debug_pci ("Mem...\n");
		if (bus_cur->firstMem) {
			res = bus_cur->firstMem;
			while (res) {
				debug_pci ("The range # is %d\n", res->rangeno);
				debug_pci ("The bus, devfnc is %d, %x\n", res->busno, res->devfunc);
				debug_pci ("[%x - %x], len=%x\n", res->start, res->end, res->len);
				if (res->next)
					res = res->next;
				else if (res->nextRange)
					res = res->nextRange;
				else
					break;
			}
		}
		debug_pci ("PFMem...\n");
		if (bus_cur->firstPFMem) {
			res = bus_cur->firstPFMem;
			while (res) {
				debug_pci ("The range # is %d\n", res->rangeno);
				debug_pci ("The bus, devfnc is %d, %x\n", res->busno, res->devfunc);
				debug_pci ("[%x - %x], len=%x\n", res->start, res->end, res->len);
				if (res->next)
					res = res->next;
				else if (res->nextRange)
					res = res->nextRange;
				else
					break;
			}
		}

		debug_pci ("PFMemFromMem...\n");
		if (bus_cur->firstPFMemFromMem) {
			res = bus_cur->firstPFMemFromMem;
			while (res) {
				debug_pci ("The range # is %d\n", res->rangeno);
				debug_pci ("The bus, devfnc is %d, %x\n", res->busno, res->devfunc);
				debug_pci ("[%x - %x], len=%x\n", res->start, res->end, res->len);
				res = res->next;
			}
		}
	}
	debug_pci ("***********************END***********************\n");
}

static int range_exists_already (struct range_node * range, struct bus_node * bus_cur, u8 type)
{
	struct range_node * range_cur = NULL;
	switch (type) {
		case IO:
			range_cur = bus_cur->rangeIO;
			break;
		case MEM:
			range_cur = bus_cur->rangeMem;
			break;
		case PFMEM:
			range_cur = bus_cur->rangePFMem;
			break;
		default:
			err ("wrong type passed to find out if range already exists\n");
			return -ENODEV;
	}

	while (range_cur) {
		if ((range_cur->start == range->start) && (range_cur->end == range->end))
			return 1;
		range_cur = range_cur->next;
	}
	
	return 0;
}

static int __init update_bridge_ranges (struct bus_node **bus)
{
	u8 sec_busno, device, function, hdr_type, start_io_address, end_io_address;
	u16 vendor_id, upper_io_start, upper_io_end, start_mem_address, end_mem_address;
	u32 start_address, end_address, upper_start, upper_end;
	struct bus_node *bus_sec;
	struct bus_node *bus_cur;
	struct resource_node *io;
	struct resource_node *mem;
	struct resource_node *pfmem;
	struct range_node *range;
	unsigned int devfn;

	bus_cur = *bus;
	if (!bus_cur)
		return -ENODEV;
	ibmphp_pci_bus->number = bus_cur->busno;

	debug ("inside %s\n", __func__);
	debug ("bus_cur->busno = %x\n", bus_cur->busno);

	for (device = 0; device < 32; device++) {
		for (function = 0x00; function < 0x08; function++) {
			devfn = PCI_DEVFN(device, function);
			pci_bus_read_config_word (ibmphp_pci_bus, devfn, PCI_VENDOR_ID, &vendor_id);

			if (vendor_id != PCI_VENDOR_ID_NOTVALID) {
				
				pci_bus_read_config_byte (ibmphp_pci_bus, devfn, PCI_HEADER_TYPE, &hdr_type);

				switch (hdr_type) {
					case PCI_HEADER_TYPE_NORMAL:
						function = 0x8;
						break;
					case PCI_HEADER_TYPE_MULTIDEVICE:
						break;
					case PCI_HEADER_TYPE_BRIDGE:
						function = 0x8;
					case PCI_HEADER_TYPE_MULTIBRIDGE:
						pci_bus_read_config_byte (ibmphp_pci_bus, devfn, PCI_SECONDARY_BUS, &sec_busno);
						bus_sec = find_bus_wprev (sec_busno, NULL, 0); 
						
						if (!bus_sec) {
							bus_sec = alloc_error_bus (NULL, sec_busno, 1);
							
							return 0;
						}
						pci_bus_read_config_byte (ibmphp_pci_bus, devfn, PCI_IO_BASE, &start_io_address);
						pci_bus_read_config_byte (ibmphp_pci_bus, devfn, PCI_IO_LIMIT, &end_io_address);
						pci_bus_read_config_word (ibmphp_pci_bus, devfn, PCI_IO_BASE_UPPER16, &upper_io_start);
						pci_bus_read_config_word (ibmphp_pci_bus, devfn, PCI_IO_LIMIT_UPPER16, &upper_io_end);
						start_address = (start_io_address & PCI_IO_RANGE_MASK) << 8;
						start_address |= (upper_io_start << 16);
						end_address = (end_io_address & PCI_IO_RANGE_MASK) << 8;
						end_address |= (upper_io_end << 16);

						if ((start_address) && (start_address <= end_address)) {
							range = kzalloc(sizeof(struct range_node), GFP_KERNEL);
							if (!range) {
								err ("out of system memory\n");
								return -ENOMEM;
							}
							range->start = start_address;
							range->end = end_address + 0xfff;

							if (bus_sec->noIORanges > 0) {
								if (!range_exists_already (range, bus_sec, IO)) {
									add_bus_range (IO, range, bus_sec);
									++bus_sec->noIORanges;
								} else {
									kfree (range);
									range = NULL;
								}
							} else {
								
								range->rangeno = 1;
								bus_sec->rangeIO = range;
								++bus_sec->noIORanges;
							}
							fix_resources (bus_sec);

							if (ibmphp_find_resource (bus_cur, start_address, &io, IO)) {
								io = kzalloc(sizeof(struct resource_node), GFP_KERNEL);
								if (!io) {
									kfree (range);
									err ("out of system memory\n");
									return -ENOMEM;
								}
								io->type = IO;
								io->busno = bus_cur->busno;
								io->devfunc = ((device << 3) | (function & 0x7));
								io->start = start_address;
								io->end = end_address + 0xfff;
								io->len = io->end - io->start + 1;
								ibmphp_add_resource (io);
							}
						}	

						pci_bus_read_config_word (ibmphp_pci_bus, devfn, PCI_MEMORY_BASE, &start_mem_address);
						pci_bus_read_config_word (ibmphp_pci_bus, devfn, PCI_MEMORY_LIMIT, &end_mem_address);

						start_address = 0x00000000 | (start_mem_address & PCI_MEMORY_RANGE_MASK) << 16;
						end_address = 0x00000000 | (end_mem_address & PCI_MEMORY_RANGE_MASK) << 16;

						if ((start_address) && (start_address <= end_address)) {

							range = kzalloc(sizeof(struct range_node), GFP_KERNEL);
							if (!range) {
								err ("out of system memory\n");
								return -ENOMEM;
							}
							range->start = start_address;
							range->end = end_address + 0xfffff;

							if (bus_sec->noMemRanges > 0) {
								if (!range_exists_already (range, bus_sec, MEM)) {
									add_bus_range (MEM, range, bus_sec);
									++bus_sec->noMemRanges;
								} else {
									kfree (range);
									range = NULL;
								}
							} else {
								
								range->rangeno = 1;
								bus_sec->rangeMem = range;
								++bus_sec->noMemRanges;
							}

							fix_resources (bus_sec);

							if (ibmphp_find_resource (bus_cur, start_address, &mem, MEM)) {
								mem = kzalloc(sizeof(struct resource_node), GFP_KERNEL);
								if (!mem) {
									kfree (range);
									err ("out of system memory\n");
									return -ENOMEM;
								}
								mem->type = MEM;
								mem->busno = bus_cur->busno;
								mem->devfunc = ((device << 3) | (function & 0x7));
								mem->start = start_address;
								mem->end = end_address + 0xfffff;
								mem->len = mem->end - mem->start + 1;
								ibmphp_add_resource (mem);
							}
						}
						pci_bus_read_config_word (ibmphp_pci_bus, devfn, PCI_PREF_MEMORY_BASE, &start_mem_address);
						pci_bus_read_config_word (ibmphp_pci_bus, devfn, PCI_PREF_MEMORY_LIMIT, &end_mem_address);
						pci_bus_read_config_dword (ibmphp_pci_bus, devfn, PCI_PREF_BASE_UPPER32, &upper_start);
						pci_bus_read_config_dword (ibmphp_pci_bus, devfn, PCI_PREF_LIMIT_UPPER32, &upper_end);
						start_address = 0x00000000 | (start_mem_address & PCI_MEMORY_RANGE_MASK) << 16;
						end_address = 0x00000000 | (end_mem_address & PCI_MEMORY_RANGE_MASK) << 16;
#if BITS_PER_LONG == 64
						start_address |= ((long) upper_start) << 32;
						end_address |= ((long) upper_end) << 32;
#endif

						if ((start_address) && (start_address <= end_address)) {

							range = kzalloc(sizeof(struct range_node), GFP_KERNEL);
							if (!range) {
								err ("out of system memory\n");
								return -ENOMEM;
							}
							range->start = start_address;
							range->end = end_address + 0xfffff;

							if (bus_sec->noPFMemRanges > 0) {
								if (!range_exists_already (range, bus_sec, PFMEM)) {
									add_bus_range (PFMEM, range, bus_sec);
									++bus_sec->noPFMemRanges;
								} else {
									kfree (range);
									range = NULL;
								}
							} else {
								
								range->rangeno = 1;
								bus_sec->rangePFMem = range;
								++bus_sec->noPFMemRanges;
							}

							fix_resources (bus_sec);
							if (ibmphp_find_resource (bus_cur, start_address, &pfmem, PFMEM)) {
								pfmem = kzalloc(sizeof(struct resource_node), GFP_KERNEL);
								if (!pfmem) {
									kfree (range);
									err ("out of system memory\n");
									return -ENOMEM;
								}
								pfmem->type = PFMEM;
								pfmem->busno = bus_cur->busno;
								pfmem->devfunc = ((device << 3) | (function & 0x7));
								pfmem->start = start_address;
								pfmem->end = end_address + 0xfffff;
								pfmem->len = pfmem->end - pfmem->start + 1;
								pfmem->fromMem = 0;

								ibmphp_add_resource (pfmem);
							}
						}
						break;
				}	
			}	
		}	
	}	

	bus = &bus_cur;
	return 0;
}
