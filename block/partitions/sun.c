/*
 *  fs/partitions/sun.c
 *
 *  Code extracted from drivers/block/genhd.c
 *
 *  Copyright (C) 1991-1998  Linus Torvalds
 *  Re-organised Feb 1998 Russell King
 */

#include "check.h"
#include "sun.h"

int sun_partition(struct parsed_partitions *state)
{
	int i;
	__be16 csum;
	int slot = 1;
	__be16 *ush;
	Sector sect;
	struct sun_disklabel {
		unsigned char info[128];   
		struct sun_vtoc {
		    __be32 version;     
		    char   volume[8];   
		    __be16 nparts;      
		    struct sun_info {           
			__be16 id;
			__be16 flags;
		    } infos[8];
		    __be16 padding;     
		    __be32 bootinfo[3];  
		    __be32 sanity;       
		    __be32 reserved[10]; 
		    __be32 timestamp[8]; 
		} vtoc;
		__be32 write_reinstruct; 
		__be32 read_reinstruct;  
		unsigned char spare[148]; 
		__be16 rspeed;     
		__be16 pcylcount;  
		__be16 sparecyl;   
		__be16 obs1;       
		__be16 obs2;       
		__be16 ilfact;     
		__be16 ncyl;       
		__be16 nacyl;      
		__be16 ntrks;      
		__be16 nsect;      
		__be16 obs3;       
		__be16 obs4;       
		struct sun_partition {
			__be32 start_cylinder;
			__be32 num_sectors;
		} partitions[8];
		__be16 magic;      
		__be16 csum;       
	} * label;
	struct sun_partition *p;
	unsigned long spc;
	char b[BDEVNAME_SIZE];
	int use_vtoc;
	int nparts;

	label = read_part_sector(state, 0, &sect);
	if (!label)
		return -1;

	p = label->partitions;
	if (be16_to_cpu(label->magic) != SUN_LABEL_MAGIC) {
		put_dev_sector(sect);
		return 0;
	}
	
	ush = ((__be16 *) (label+1)) - 1;
	for (csum = 0; ush >= ((__be16 *) label);)
		csum ^= *ush--;
	if (csum) {
		printk("Dev %s Sun disklabel: Csum bad, label corrupted\n",
		       bdevname(state->bdev, b));
		put_dev_sector(sect);
		return 0;
	}

	
	use_vtoc = ((be32_to_cpu(label->vtoc.sanity) == SUN_VTOC_SANITY) &&
		    (be32_to_cpu(label->vtoc.version) == 1) &&
		    (be16_to_cpu(label->vtoc.nparts) <= 8));

	
	nparts = (use_vtoc) ? be16_to_cpu(label->vtoc.nparts) : 8;

	use_vtoc = use_vtoc || !(label->vtoc.sanity ||
				 label->vtoc.version || label->vtoc.nparts);
	spc = be16_to_cpu(label->ntrks) * be16_to_cpu(label->nsect);
	for (i = 0; i < nparts; i++, p++) {
		unsigned long st_sector;
		unsigned int num_sectors;

		st_sector = be32_to_cpu(p->start_cylinder) * spc;
		num_sectors = be32_to_cpu(p->num_sectors);
		if (num_sectors) {
			put_partition(state, slot, st_sector, num_sectors);
			state->parts[slot].flags = 0;
			if (use_vtoc) {
				if (be16_to_cpu(label->vtoc.infos[i].id) == LINUX_RAID_PARTITION)
					state->parts[slot].flags |= ADDPART_FLAG_RAID;
				else if (be16_to_cpu(label->vtoc.infos[i].id) == SUN_WHOLE_DISK)
					state->parts[slot].flags |= ADDPART_FLAG_WHOLEDISK;
			}
		}
		slot++;
	}
	strlcat(state->pp_buf, "\n", PAGE_SIZE);
	put_dev_sector(sect);
	return 1;
}
