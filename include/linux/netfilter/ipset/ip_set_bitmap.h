#ifndef __IP_SET_BITMAP_H
#define __IP_SET_BITMAP_H

enum {
	
	IPSET_ERR_BITMAP_RANGE = IPSET_ERR_TYPE_SPECIFIC,
	
	IPSET_ERR_BITMAP_RANGE_SIZE,
};

#ifdef __KERNEL__
#define IPSET_BITMAP_MAX_RANGE	0x0000FFFF


static inline u32
range_to_mask(u32 from, u32 to, u8 *bits)
{
	u32 mask = 0xFFFFFFFE;

	*bits = 32;
	while (--(*bits) > 0 && mask && (to & mask) != from)
		mask <<= 1;

	return mask;
}

#endif 

#endif 
