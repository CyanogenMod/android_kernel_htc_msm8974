/*
 * bitmap.c - NTFS kernel bitmap handling.  Part of the Linux-NTFS project.
 *
 * Copyright (c) 2004-2005 Anton Altaparmakov
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef NTFS_RW

#include <linux/pagemap.h>

#include "bitmap.h"
#include "debug.h"
#include "aops.h"
#include "ntfs.h"

int __ntfs_bitmap_set_bits_in_run(struct inode *vi, const s64 start_bit,
		const s64 count, const u8 value, const bool is_rollback)
{
	s64 cnt = count;
	pgoff_t index, end_index;
	struct address_space *mapping;
	struct page *page;
	u8 *kaddr;
	int pos, len;
	u8 bit;

	BUG_ON(!vi);
	ntfs_debug("Entering for i_ino 0x%lx, start_bit 0x%llx, count 0x%llx, "
			"value %u.%s", vi->i_ino, (unsigned long long)start_bit,
			(unsigned long long)cnt, (unsigned int)value,
			is_rollback ? " (rollback)" : "");
	BUG_ON(start_bit < 0);
	BUG_ON(cnt < 0);
	BUG_ON(value > 1);
	index = start_bit >> (3 + PAGE_CACHE_SHIFT);
	end_index = (start_bit + cnt - 1) >> (3 + PAGE_CACHE_SHIFT);

	
	mapping = vi->i_mapping;
	page = ntfs_map_page(mapping, index);
	if (IS_ERR(page)) {
		if (!is_rollback)
			ntfs_error(vi->i_sb, "Failed to map first page (error "
					"%li), aborting.", PTR_ERR(page));
		return PTR_ERR(page);
	}
	kaddr = page_address(page);

	
	pos = (start_bit >> 3) & ~PAGE_CACHE_MASK;

	
	bit = start_bit & 7;

	
	if (bit) {
		u8 *byte = kaddr + pos;
		while ((bit & 7) && cnt) {
			cnt--;
			if (value)
				*byte |= 1 << bit++;
			else
				*byte &= ~(1 << bit++);
		}
		
		if (!cnt)
			goto done;

		
		pos++;
	}
	len = min_t(s64, cnt >> 3, PAGE_CACHE_SIZE - pos);
	memset(kaddr + pos, value ? 0xff : 0, len);
	cnt -= len << 3;

	
	if (cnt < 8)
		len += pos;

	
	while (index < end_index) {
		BUG_ON(cnt <= 0);

		
		flush_dcache_page(page);
		set_page_dirty(page);
		ntfs_unmap_page(page);
		page = ntfs_map_page(mapping, ++index);
		if (IS_ERR(page))
			goto rollback;
		kaddr = page_address(page);
		len = min_t(s64, cnt >> 3, PAGE_CACHE_SIZE);
		memset(kaddr, value ? 0xff : 0, len);
		cnt -= len << 3;
	}
	if (cnt) {
		u8 *byte;

		BUG_ON(cnt > 7);

		bit = cnt;
		byte = kaddr + len;
		while (bit--) {
			if (value)
				*byte |= 1 << bit;
			else
				*byte &= ~(1 << bit);
		}
	}
done:
	
	flush_dcache_page(page);
	set_page_dirty(page);
	ntfs_unmap_page(page);
	ntfs_debug("Done.");
	return 0;
rollback:
	if (is_rollback)
		return PTR_ERR(page);
	if (count != cnt)
		pos = __ntfs_bitmap_set_bits_in_run(vi, start_bit, count - cnt,
				value ? 0 : 1, true);
	else
		pos = 0;
	if (!pos) {
		
		ntfs_error(vi->i_sb, "Failed to map subsequent page (error "
				"%li), aborting.", PTR_ERR(page));
	} else {
		
		ntfs_error(vi->i_sb, "Failed to map subsequent page (error "
				"%li) and rollback failed (error %i).  "
				"Aborting and leaving inconsistent metadata.  "
				"Unmount and run chkdsk.", PTR_ERR(page), pos);
		NVolSetErrors(NTFS_SB(vi->i_sb));
	}
	return PTR_ERR(page);
}

#endif 
