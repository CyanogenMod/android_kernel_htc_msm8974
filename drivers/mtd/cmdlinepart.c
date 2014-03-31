/*
 * Read flash partition table from command line
 *
 * Copyright © 2002      SYSGO Real-Time Solutions GmbH
 * Copyright © 2002-2010 David Woodhouse <dwmw2@infradead.org>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * The format for the command line is as follows:
 *
 * mtdparts=<mtddef>[;<mtddef]
 * <mtddef>  := <mtd-id>:<partdef>[,<partdef>]
 *              where <mtd-id> is the name from the "cat /proc/mtd" command
 * <partdef> := <size>[@offset][<name>][ro][lk]
 * <mtd-id>  := unique name used in mapping driver/device (mtd->name)
 * <size>    := standard linux memsize OR "-" to denote all remaining space
 * <name>    := '(' NAME ')'
 *
 * Examples:
 *
 * 1 NOR Flash, with 1 single writable partition:
 * edb7312-nor:-
 *
 * 1 NOR Flash with 2 partitions, 1 NAND with one
 * edb7312-nor:256k(ARMboot)ro,-(root);edb7312-nand:-(home)
 */

#include <linux/kernel.h>
#include <linux/slab.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/bootmem.h>
#include <linux/module.h>

#define ERRP "mtd: "

#if 0
#define dbg(x) do { printk("DEBUG-CMDLINE-PART: "); printk x; } while(0)
#else
#define dbg(x)
#endif


#define SIZE_REMAINING UINT_MAX
#define OFFSET_CONTINUOUS UINT_MAX

struct cmdline_mtd_partition {
	struct cmdline_mtd_partition *next;
	char *mtd_id;
	int num_parts;
	struct mtd_partition *parts;
};

static struct cmdline_mtd_partition *partitions;

static char *cmdline;
static int cmdline_parsed = 0;

static struct mtd_partition * newpart(char *s,
                                      char **retptr,
                                      int *num_parts,
                                      int this_part,
                                      unsigned char **extra_mem_ptr,
                                      int extra_mem_size)
{
	struct mtd_partition *parts;
	unsigned long size;
	unsigned long offset = OFFSET_CONTINUOUS;
	char *name;
	int name_len;
	unsigned char *extra_mem;
	char delim;
	unsigned int mask_flags;

	
	if (*s == '-')
	{	
		size = SIZE_REMAINING;
		s++;
	}
	else
	{
		size = memparse(s, &s);
		if (size < PAGE_SIZE)
		{
			printk(KERN_ERR ERRP "partition size too small (%lx)\n", size);
			return NULL;
		}
	}

	
	mask_flags = 0; 
	delim = 0;
        
        if (*s == '@')
	{
                s++;
                offset = memparse(s, &s);
        }
        
	if (*s == '(')
	{
		delim = ')';
	}

	if (delim)
	{
		char *p;

	    	name = ++s;
		p = strchr(name, delim);
		if (!p)
		{
			printk(KERN_ERR ERRP "no closing %c found in partition name\n", delim);
			return NULL;
		}
		name_len = p - name;
		s = p + 1;
	}
	else
	{
	    	name = NULL;
		name_len = 13; 
	}

	
	extra_mem_size += name_len + 1;

        
        if (strncmp(s, "ro", 2) == 0)
	{
		mask_flags |= MTD_WRITEABLE;
		s += 2;
        }

        
        if (strncmp(s, "lk", 2) == 0)
	{
		mask_flags |= MTD_POWERUP_LOCK;
		s += 2;
        }

	
	if (*s == ',')
	{
		if (size == SIZE_REMAINING)
		{
			printk(KERN_ERR ERRP "no partitions allowed after a fill-up partition\n");
			return NULL;
		}
		
		parts = newpart(s + 1, &s, num_parts, this_part + 1,
				&extra_mem, extra_mem_size);
		if (!parts)
			return NULL;
	}
	else
	{	
		int alloc_size;

		*num_parts = this_part + 1;
		alloc_size = *num_parts * sizeof(struct mtd_partition) +
			     extra_mem_size;
		parts = kzalloc(alloc_size, GFP_KERNEL);
		if (!parts)
			return NULL;
		extra_mem = (unsigned char *)(parts + *num_parts);
	}
	
	parts[this_part].size = size;
	parts[this_part].offset = offset;
	parts[this_part].mask_flags = mask_flags;
	if (name)
	{
		strlcpy(extra_mem, name, name_len + 1);
	}
	else
	{
		sprintf(extra_mem, "Partition_%03d", this_part);
	}
	parts[this_part].name = extra_mem;
	extra_mem += name_len + 1;

	dbg(("partition %d: name <%s>, offset %llx, size %llx, mask flags %x\n",
	     this_part,
	     parts[this_part].name,
	     parts[this_part].offset,
	     parts[this_part].size,
	     parts[this_part].mask_flags));

	
	if (extra_mem_ptr)
	  *extra_mem_ptr = extra_mem;

	
	*retptr = s;

	
	return parts;
}

static int mtdpart_setup_real(char *s)
{
	cmdline_parsed = 1;

	for( ; s != NULL; )
	{
		struct cmdline_mtd_partition *this_mtd;
		struct mtd_partition *parts;
	    	int mtd_id_len;
		int num_parts;
		char *p, *mtd_id;

	    	mtd_id = s;
		
		if (!(p = strchr(s, ':')))
		{
			printk(KERN_ERR ERRP "no mtd-id\n");
			return 0;
		}
		mtd_id_len = p - mtd_id;

		dbg(("parsing <%s>\n", p+1));

		parts = newpart(p + 1,		
				&s,		
				&num_parts,	
				0,		
				(unsigned char**)&this_mtd, 
				mtd_id_len + 1 + sizeof(*this_mtd) +
				sizeof(void*)-1 );
		if(!parts)
		{
			 return 0;
		 }

		
		this_mtd = (struct cmdline_mtd_partition *)
			ALIGN((unsigned long)this_mtd, sizeof(void*));
		
		this_mtd->parts = parts;
		this_mtd->num_parts = num_parts;
		this_mtd->mtd_id = (char*)(this_mtd + 1);
		strlcpy(this_mtd->mtd_id, mtd_id, mtd_id_len + 1);

		
		this_mtd->next = partitions;
		partitions = this_mtd;

		dbg(("mtdid=<%s> num_parts=<%d>\n",
		     this_mtd->mtd_id, this_mtd->num_parts));


		
		if (*s == 0)
			break;

		
		if (*s != ';')
		{
			printk(KERN_ERR ERRP "bad character after partition (%c)\n", *s);
			return 0;
		}
		s++;
	}
	return 1;
}

static int parse_cmdline_partitions(struct mtd_info *master,
				    struct mtd_partition **pparts,
				    struct mtd_part_parser_data *data)
{
	unsigned long offset;
	int i;
	struct cmdline_mtd_partition *part;
	const char *mtd_id = master->name;

	
	if (!cmdline_parsed)
		mtdpart_setup_real(cmdline);

	for(part = partitions; part; part = part->next)
	{
		if ((!mtd_id) || (!strcmp(part->mtd_id, mtd_id)))
		{
			for(i = 0, offset = 0; i < part->num_parts; i++)
			{
				if (part->parts[i].offset == OFFSET_CONTINUOUS)
				  part->parts[i].offset = offset;
				else
				  offset = part->parts[i].offset;
				if (part->parts[i].size == SIZE_REMAINING)
				  part->parts[i].size = master->size - offset;
				if (offset + part->parts[i].size > master->size)
				{
					printk(KERN_WARNING ERRP
					       "%s: partitioning exceeds flash size, truncating\n",
					       part->mtd_id);
					part->parts[i].size = master->size - offset;
					part->num_parts = i;
				}
				offset += part->parts[i].size;
			}
			*pparts = kmemdup(part->parts,
					sizeof(*part->parts) * part->num_parts,
					GFP_KERNEL);
			if (!*pparts)
				return -ENOMEM;
			return part->num_parts;
		}
	}
	return 0;
}


static int mtdpart_setup(char *s)
{
	cmdline = s;
	return 1;
}

__setup("mtdparts=", mtdpart_setup);

static struct mtd_part_parser cmdline_parser = {
	.owner = THIS_MODULE,
	.parse_fn = parse_cmdline_partitions,
	.name = "cmdlinepart",
};

static int __init cmdline_parser_init(void)
{
	return register_mtd_parser(&cmdline_parser);
}

module_init(cmdline_parser_init);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marius Groeger <mag@sysgo.de>");
MODULE_DESCRIPTION("Command line configuration of MTD partitions");
