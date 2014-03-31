#ifndef _RAID0_H
#define _RAID0_H

struct strip_zone {
	sector_t zone_end;	
	sector_t dev_start;	
	int	 nb_dev;	
};

struct r0conf {
	struct strip_zone	*strip_zone;
	struct md_rdev		**devlist; 
	int			nr_strip_zones;
	int			has_merge_bvec;	
};

#endif
