/*
 *  linux/fs/adfs/map.c
 *
 *  Copyright (C) 1997-2002 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/buffer_head.h>
#include <asm/unaligned.h>
#include "adfs.h"


static DEFINE_RWLOCK(adfs_map_lock);

#define GET_FRAG_ID(_map,_start,_idmask)				\
	({								\
		unsigned char *_m = _map + (_start >> 3);		\
		u32 _frag = get_unaligned_le32(_m);			\
		_frag >>= (_start & 7);					\
		_frag & _idmask;					\
	})

static int
lookup_zone(const struct adfs_discmap *dm, const unsigned int idlen,
	    const unsigned int frag_id, unsigned int *offset)
{
	const unsigned int mapsize = dm->dm_endbit;
	const u32 idmask = (1 << idlen) - 1;
	unsigned char *map = dm->dm_bh->b_data + 4;
	unsigned int start = dm->dm_startbit;
	unsigned int mapptr;
	u32 frag;

	do {
		frag = GET_FRAG_ID(map, start, idmask);
		mapptr = start + idlen;

		{
			__le32 *_map = (__le32 *)map;
			u32 v = le32_to_cpu(_map[mapptr >> 5]) >> (mapptr & 31);
			while (v == 0) {
				mapptr = (mapptr & ~31) + 32;
				if (mapptr >= mapsize)
					goto error;
				v = le32_to_cpu(_map[mapptr >> 5]);
			}

			mapptr += 1 + ffz(~v);
		}

		if (frag == frag_id)
			goto found;
again:
		start = mapptr;
	} while (mapptr < mapsize);
	return -1;

error:
	printk(KERN_ERR "adfs: oversized fragment 0x%x at 0x%x-0x%x\n",
		frag, start, mapptr);
	return -1;

found:
	{
		int length = mapptr - start;
		if (*offset >= length) {
			*offset -= length;
			goto again;
		}
	}
	return start + *offset;
}

static unsigned int
scan_free_map(struct adfs_sb_info *asb, struct adfs_discmap *dm)
{
	const unsigned int mapsize = dm->dm_endbit + 32;
	const unsigned int idlen  = asb->s_idlen;
	const unsigned int frag_idlen = idlen <= 15 ? idlen : 15;
	const u32 idmask = (1 << frag_idlen) - 1;
	unsigned char *map = dm->dm_bh->b_data;
	unsigned int start = 8, mapptr;
	u32 frag;
	unsigned long total = 0;

	frag = GET_FRAG_ID(map, start, idmask);

	if (frag == 0)
		return 0;

	do {
		start += frag;

		frag = GET_FRAG_ID(map, start, idmask);
		mapptr = start + idlen;

		{
			__le32 *_map = (__le32 *)map;
			u32 v = le32_to_cpu(_map[mapptr >> 5]) >> (mapptr & 31);
			while (v == 0) {
				mapptr = (mapptr & ~31) + 32;
				if (mapptr >= mapsize)
					goto error;
				v = le32_to_cpu(_map[mapptr >> 5]);
			}

			mapptr += 1 + ffz(~v);
		}

		total += mapptr - start;
	} while (frag >= idlen + 1);

	if (frag != 0)
		printk(KERN_ERR "adfs: undersized free fragment\n");

	return total;
error:
	printk(KERN_ERR "adfs: oversized free fragment\n");
	return 0;
}

static int
scan_map(struct adfs_sb_info *asb, unsigned int zone,
	 const unsigned int frag_id, unsigned int mapoff)
{
	const unsigned int idlen = asb->s_idlen;
	struct adfs_discmap *dm, *dm_end;
	int result;

	dm	= asb->s_map + zone;
	zone	= asb->s_map_size;
	dm_end	= asb->s_map + zone;

	do {
		result = lookup_zone(dm, idlen, frag_id, &mapoff);

		if (result != -1)
			goto found;

		dm ++;
		if (dm == dm_end)
			dm = asb->s_map;
	} while (--zone > 0);

	return -1;
found:
	result -= dm->dm_startbit;
	result += dm->dm_startblk;

	return result;
}

unsigned int
adfs_map_free(struct super_block *sb)
{
	struct adfs_sb_info *asb = ADFS_SB(sb);
	struct adfs_discmap *dm;
	unsigned int total = 0;
	unsigned int zone;

	dm   = asb->s_map;
	zone = asb->s_map_size;

	do {
		total += scan_free_map(asb, dm++);
	} while (--zone > 0);

	return signed_asl(total, asb->s_map2blk);
}

int
adfs_map_lookup(struct super_block *sb, unsigned int frag_id,
		unsigned int offset)
{
	struct adfs_sb_info *asb = ADFS_SB(sb);
	unsigned int zone, mapoff;
	int result;

	if (frag_id == ADFS_ROOT_FRAG)
		zone = asb->s_map_size >> 1;
	else
		zone = frag_id / asb->s_ids_per_zone;

	if (zone >= asb->s_map_size)
		goto bad_fragment;

	
	mapoff = signed_asl(offset, -asb->s_map2blk);

	read_lock(&adfs_map_lock);
	result = scan_map(asb, zone, frag_id, mapoff);
	read_unlock(&adfs_map_lock);

	if (result > 0) {
		unsigned int secoff;

		
		secoff = offset - signed_asl(mapoff, asb->s_map2blk);
		return secoff + signed_asl(result, asb->s_map2blk);
	}

	adfs_error(sb, "fragment 0x%04x at offset %d not found in map",
		   frag_id, offset);
	return 0;

bad_fragment:
	adfs_error(sb, "invalid fragment 0x%04x (zone = %d, max = %d)",
		   frag_id, zone, asb->s_map_size);
	return 0;
}
