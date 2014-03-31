
#include "check.h"
#include "ultrix.h"

int ultrix_partition(struct parsed_partitions *state)
{
	int i;
	Sector sect;
	unsigned char *data;
	struct ultrix_disklabel {
		s32	pt_magic;	
		s32	pt_valid;	
		struct  pt_info {
			s32		pi_nblocks; 
			u32		pi_blkoff;  
		} pt_part[8];
	} *label;

#define PT_MAGIC	0x032957	
#define PT_VALID	1		

	data = read_part_sector(state, (16384 - sizeof(*label))/512, &sect);
	if (!data)
		return -1;
	
	label = (struct ultrix_disklabel *)(data + 512 - sizeof(*label));

	if (label->pt_magic == PT_MAGIC && label->pt_valid == PT_VALID) {
		for (i=0; i<8; i++)
			if (label->pt_part[i].pi_nblocks)
				put_partition(state, i+1, 
					      label->pt_part[i].pi_blkoff,
					      label->pt_part[i].pi_nblocks);
		put_dev_sector(sect);
		strlcat(state->pp_buf, "\n", PAGE_SIZE);
		return 1;
	} else {
		put_dev_sector(sect);
		return 0;
	}
}
