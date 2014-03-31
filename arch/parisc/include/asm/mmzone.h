#ifndef _PARISC_MMZONE_H
#define _PARISC_MMZONE_H

#ifdef CONFIG_DISCONTIGMEM

#define MAX_PHYSMEM_RANGES 8 
extern int npmem_ranges;

struct node_map_data {
    pg_data_t pg_data;
};

extern struct node_map_data node_data[];

#define NODE_DATA(nid)          (&node_data[nid].pg_data)



#define PFNNID_SHIFT (30 - PAGE_SHIFT)
#define PFNNID_MAP_MAX  512     
extern unsigned char pfnnid_map[PFNNID_MAP_MAX];

#ifndef CONFIG_64BIT
#define pfn_is_io(pfn) ((pfn & (0xf0000000UL >> PAGE_SHIFT)) == (0xf0000000UL >> PAGE_SHIFT))
#else
#define pfn_is_io(pfn) ((pfn & (0xf000000000000000UL >> PAGE_SHIFT)) == (0xf000000000000000UL >> PAGE_SHIFT))
#endif

static inline int pfn_to_nid(unsigned long pfn)
{
	unsigned int i;
	unsigned char r;

	if (unlikely(pfn_is_io(pfn)))
		return 0;

	i = pfn >> PFNNID_SHIFT;
	BUG_ON(i >= sizeof(pfnnid_map) / sizeof(pfnnid_map[0]));
	r = pfnnid_map[i];
	BUG_ON(r == 0xff);

	return (int)r;
}

static inline int pfn_valid(int pfn)
{
	int nid = pfn_to_nid(pfn);

	if (nid >= 0)
		return (pfn < node_end_pfn(nid));
	return 0;
}

#else 
#define MAX_PHYSMEM_RANGES 	1 
#endif
#endif 
