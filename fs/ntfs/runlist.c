/**
 * runlist.c - NTFS runlist handling code.  Part of the Linux-NTFS project.
 *
 * Copyright (c) 2001-2007 Anton Altaparmakov
 * Copyright (c) 2002-2005 Richard Russon
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

#include "debug.h"
#include "dir.h"
#include "endian.h"
#include "malloc.h"
#include "ntfs.h"

static inline void ntfs_rl_mm(runlist_element *base, int dst, int src,
		int size)
{
	if (likely((dst != src) && (size > 0)))
		memmove(base + dst, base + src, size * sizeof(*base));
}

static inline void ntfs_rl_mc(runlist_element *dstbase, int dst,
		runlist_element *srcbase, int src, int size)
{
	if (likely(size > 0))
		memcpy(dstbase + dst, srcbase + src, size * sizeof(*dstbase));
}

static inline runlist_element *ntfs_rl_realloc(runlist_element *rl,
		int old_size, int new_size)
{
	runlist_element *new_rl;

	old_size = PAGE_ALIGN(old_size * sizeof(*rl));
	new_size = PAGE_ALIGN(new_size * sizeof(*rl));
	if (old_size == new_size)
		return rl;

	new_rl = ntfs_malloc_nofs(new_size);
	if (unlikely(!new_rl))
		return ERR_PTR(-ENOMEM);

	if (likely(rl != NULL)) {
		if (unlikely(old_size > new_size))
			old_size = new_size;
		memcpy(new_rl, rl, old_size);
		ntfs_free(rl);
	}
	return new_rl;
}

static inline runlist_element *ntfs_rl_realloc_nofail(runlist_element *rl,
		int old_size, int new_size)
{
	runlist_element *new_rl;

	old_size = PAGE_ALIGN(old_size * sizeof(*rl));
	new_size = PAGE_ALIGN(new_size * sizeof(*rl));
	if (old_size == new_size)
		return rl;

	new_rl = ntfs_malloc_nofs_nofail(new_size);
	BUG_ON(!new_rl);

	if (likely(rl != NULL)) {
		if (unlikely(old_size > new_size))
			old_size = new_size;
		memcpy(new_rl, rl, old_size);
		ntfs_free(rl);
	}
	return new_rl;
}

static inline bool ntfs_are_rl_mergeable(runlist_element *dst,
		runlist_element *src)
{
	BUG_ON(!dst);
	BUG_ON(!src);

	
	if ((dst->lcn == LCN_RL_NOT_MAPPED) && (src->lcn == LCN_RL_NOT_MAPPED))
		return true;
	
	if ((dst->vcn + dst->length) != src->vcn)
		return false;
	
	if ((dst->lcn >= 0) && (src->lcn >= 0) &&
			((dst->lcn + dst->length) == src->lcn))
		return true;
	
	if ((dst->lcn == LCN_HOLE) && (src->lcn == LCN_HOLE))
		return true;
	
	return false;
}

static inline void __ntfs_rl_merge(runlist_element *dst, runlist_element *src)
{
	dst->length += src->length;
}

static inline runlist_element *ntfs_rl_append(runlist_element *dst,
		int dsize, runlist_element *src, int ssize, int loc)
{
	bool right = false;	
	int marker;		

	BUG_ON(!dst);
	BUG_ON(!src);

	
	if ((loc + 1) < dsize)
		right = ntfs_are_rl_mergeable(src + ssize - 1, dst + loc + 1);

	
	dst = ntfs_rl_realloc(dst, dsize, dsize + ssize - right);
	if (IS_ERR(dst))
		return dst;

	
	if (right)
		__ntfs_rl_merge(src + ssize - 1, dst + loc + 1);

	
	marker = loc + ssize + 1;

	
	ntfs_rl_mm(dst, marker, loc + 1 + right, dsize - (loc + 1 + right));
	ntfs_rl_mc(dst, loc + 1, src, 0, ssize);

	
	dst[loc].length = dst[loc + 1].vcn - dst[loc].vcn;

	
	if (dst[marker].lcn == LCN_ENOENT)
		dst[marker].vcn = dst[marker - 1].vcn + dst[marker - 1].length;

	return dst;
}

static inline runlist_element *ntfs_rl_insert(runlist_element *dst,
		int dsize, runlist_element *src, int ssize, int loc)
{
	bool left = false;	
	bool disc = false;	
	int marker;		

	BUG_ON(!dst);
	BUG_ON(!src);

	if (loc == 0)
		disc = (src[0].vcn > 0);
	else {
		s64 merged_length;

		left = ntfs_are_rl_mergeable(dst + loc - 1, src);

		merged_length = dst[loc - 1].length;
		if (left)
			merged_length += src->length;

		disc = (src[0].vcn > dst[loc - 1].vcn + merged_length);
	}
	dst = ntfs_rl_realloc(dst, dsize, dsize + ssize - left + disc);
	if (IS_ERR(dst))
		return dst;
	if (left)
		__ntfs_rl_merge(dst + loc - 1, src);
	marker = loc + ssize - left + disc;

	
	ntfs_rl_mm(dst, marker, loc, dsize - loc);
	ntfs_rl_mc(dst, loc + disc, src, left, ssize - left);

	
	dst[marker].vcn = dst[marker - 1].vcn + dst[marker - 1].length;
	
	if (dst[marker].lcn == LCN_HOLE || dst[marker].lcn == LCN_RL_NOT_MAPPED)
		dst[marker].length = dst[marker + 1].vcn - dst[marker].vcn;

	
	if (disc) {
		if (loc > 0) {
			dst[loc].vcn = dst[loc - 1].vcn + dst[loc - 1].length;
			dst[loc].length = dst[loc + 1].vcn - dst[loc].vcn;
		} else {
			dst[loc].vcn = 0;
			dst[loc].length = dst[loc + 1].vcn;
		}
		dst[loc].lcn = LCN_RL_NOT_MAPPED;
	}
	return dst;
}

static inline runlist_element *ntfs_rl_replace(runlist_element *dst,
		int dsize, runlist_element *src, int ssize, int loc)
{
	signed delta;
	bool left = false;	
	bool right = false;	
	int tail;		
	int marker;		

	BUG_ON(!dst);
	BUG_ON(!src);

	
	if ((loc + 1) < dsize)
		right = ntfs_are_rl_mergeable(src + ssize - 1, dst + loc + 1);
	if (loc > 0)
		left = ntfs_are_rl_mergeable(dst + loc - 1, src);
	delta = ssize - 1 - left - right;
	if (delta > 0) {
		dst = ntfs_rl_realloc(dst, dsize, dsize + delta);
		if (IS_ERR(dst))
			return dst;
	}

	
	if (right)
		__ntfs_rl_merge(src + ssize - 1, dst + loc + 1);
	if (left)
		__ntfs_rl_merge(dst + loc - 1, src);
	tail = loc + right + 1;
	marker = loc + ssize - left;

	
	ntfs_rl_mm(dst, marker, tail, dsize - tail);
	ntfs_rl_mc(dst, loc, src, left, ssize - left);

	
	if (dsize - tail > 0 && dst[marker].lcn == LCN_ENOENT)
		dst[marker].vcn = dst[marker - 1].vcn + dst[marker - 1].length;
	return dst;
}

static inline runlist_element *ntfs_rl_split(runlist_element *dst, int dsize,
		runlist_element *src, int ssize, int loc)
{
	BUG_ON(!dst);
	BUG_ON(!src);

	
	dst = ntfs_rl_realloc(dst, dsize, dsize + ssize + 1);
	if (IS_ERR(dst))
		return dst;

	
	ntfs_rl_mm(dst, loc + 1 + ssize, loc, dsize - loc);
	ntfs_rl_mc(dst, loc + 1, src, 0, ssize);

	
	dst[loc].length		= dst[loc+1].vcn       - dst[loc].vcn;
	dst[loc+ssize+1].vcn    = dst[loc+ssize].vcn   + dst[loc+ssize].length;
	dst[loc+ssize+1].length = dst[loc+ssize+2].vcn - dst[loc+ssize+1].vcn;

	return dst;
}

runlist_element *ntfs_runlists_merge(runlist_element *drl,
		runlist_element *srl)
{
	int di, si;		
	int sstart;		
	int dins;		
	int dend, send;		
	int dfinal, sfinal;	
	int marker = 0;
	VCN marker_vcn = 0;

#ifdef DEBUG
	ntfs_debug("dst:");
	ntfs_debug_dump_runlist(drl);
	ntfs_debug("src:");
	ntfs_debug_dump_runlist(srl);
#endif

	
	if (unlikely(!srl))
		return drl;
	if (IS_ERR(srl) || IS_ERR(drl))
		return ERR_PTR(-EINVAL);

	
	if (unlikely(!drl)) {
		drl = srl;
		
		if (unlikely(drl[0].vcn)) {
			
			for (dend = 0; likely(drl[dend].length); dend++)
				;
			dend++;
			drl = ntfs_rl_realloc(drl, dend, dend + 1);
			if (IS_ERR(drl))
				return drl;
			
			ntfs_rl_mm(drl, 1, 0, dend);
			drl[0].vcn = 0;
			drl[0].lcn = LCN_RL_NOT_MAPPED;
			drl[0].length = drl[1].vcn;
		}
		goto finished;
	}

	si = di = 0;

	
	while (srl[si].length && srl[si].lcn < LCN_HOLE)
		si++;

	
	BUG_ON(!srl[si].length);

	
	sstart = si;

	for (; drl[di].length; di++) {
		if (drl[di].vcn + drl[di].length > srl[sstart].vcn)
			break;
	}
	dins = di;

	
	if ((drl[di].vcn == srl[si].vcn) && (drl[di].lcn >= 0) &&
			(srl[si].lcn >= 0)) {
		ntfs_error(NULL, "Run lists overlap. Cannot merge!");
		return ERR_PTR(-ERANGE);
	}

	
	for (send = si; srl[send].length; send++)
		;
	for (dend = di; drl[dend].length; dend++)
		;

	if (srl[send].lcn == LCN_ENOENT)
		marker_vcn = srl[marker = send].vcn;

	
	for (sfinal = send; sfinal >= 0 && srl[sfinal].lcn < LCN_HOLE; sfinal--)
		;
	for (dfinal = dend; dfinal >= 0 && drl[dfinal].lcn < LCN_HOLE; dfinal--)
		;

	{
	bool start;
	bool finish;
	int ds = dend + 1;		
	int ss = sfinal - sstart + 1;

	start  = ((drl[dins].lcn <  LCN_RL_NOT_MAPPED) ||    
		  (drl[dins].vcn == srl[sstart].vcn));	     
	finish = ((drl[dins].lcn >= LCN_RL_NOT_MAPPED) &&    
		 ((drl[dins].vcn + drl[dins].length) <=      
		  (srl[send - 1].vcn + srl[send - 1].length)));

	
	if (finish && !drl[dins].length)
		ss++;
	if (marker && (drl[dins].vcn + drl[dins].length > srl[send - 1].vcn))
		finish = false;
#if 0
	ntfs_debug("dfinal = %i, dend = %i", dfinal, dend);
	ntfs_debug("sstart = %i, sfinal = %i, send = %i", sstart, sfinal, send);
	ntfs_debug("start = %i, finish = %i", start, finish);
	ntfs_debug("ds = %i, ss = %i, dins = %i", ds, ss, dins);
#endif
	if (start) {
		if (finish)
			drl = ntfs_rl_replace(drl, ds, srl + sstart, ss, dins);
		else
			drl = ntfs_rl_insert(drl, ds, srl + sstart, ss, dins);
	} else {
		if (finish)
			drl = ntfs_rl_append(drl, ds, srl + sstart, ss, dins);
		else
			drl = ntfs_rl_split(drl, ds, srl + sstart, ss, dins);
	}
	if (IS_ERR(drl)) {
		ntfs_error(NULL, "Merge failed.");
		return drl;
	}
	ntfs_free(srl);
	if (marker) {
		ntfs_debug("Triggering marker code.");
		for (ds = dend; drl[ds].length; ds++)
			;
		
		if (drl[ds].vcn <= marker_vcn) {
			int slots = 0;

			if (drl[ds].vcn == marker_vcn) {
				ntfs_debug("Old marker = 0x%llx, replacing "
						"with LCN_ENOENT.",
						(unsigned long long)
						drl[ds].lcn);
				drl[ds].lcn = LCN_ENOENT;
				goto finished;
			}
			if (drl[ds].lcn == LCN_ENOENT) {
				ds--;
				slots = 1;
			}
			if (drl[ds].lcn != LCN_RL_NOT_MAPPED) {
				
				if (!slots) {
					drl = ntfs_rl_realloc_nofail(drl, ds,
							ds + 2);
					slots = 2;
				}
				ds++;
				
				if (slots != 1)
					drl[ds].vcn = drl[ds - 1].vcn +
							drl[ds - 1].length;
				drl[ds].lcn = LCN_RL_NOT_MAPPED;
				
				slots--;
			}
			drl[ds].length = marker_vcn - drl[ds].vcn;
			
			ds++;
			if (!slots)
				drl = ntfs_rl_realloc_nofail(drl, ds, ds + 1);
			drl[ds].vcn = marker_vcn;
			drl[ds].lcn = LCN_ENOENT;
			drl[ds].length = (s64)0;
		}
	}
	}

finished:
	
	ntfs_debug("Merged runlist:");
	ntfs_debug_dump_runlist(drl);
	return drl;
}

runlist_element *ntfs_mapping_pairs_decompress(const ntfs_volume *vol,
		const ATTR_RECORD *attr, runlist_element *old_rl)
{
	VCN vcn;		
	LCN lcn;		
	s64 deltaxcn;		
	runlist_element *rl;	
	u8 *buf;		
	u8 *attr_end;		
	int rlsize;		
	u16 rlpos;		
	u8 b;			

#ifdef DEBUG
	
	if (!attr || !attr->non_resident || sle64_to_cpu(
			attr->data.non_resident.lowest_vcn) < (VCN)0) {
		ntfs_error(vol->sb, "Invalid arguments.");
		return ERR_PTR(-EINVAL);
	}
#endif
	
	vcn = sle64_to_cpu(attr->data.non_resident.lowest_vcn);
	lcn = 0;
	
	buf = (u8*)attr + le16_to_cpu(
			attr->data.non_resident.mapping_pairs_offset);
	attr_end = (u8*)attr + le32_to_cpu(attr->length);
	if (unlikely(buf < (u8*)attr || buf > attr_end)) {
		ntfs_error(vol->sb, "Corrupt attribute.");
		return ERR_PTR(-EIO);
	}
	
	if (!vcn && !*buf)
		return old_rl;
	
	rlpos = 0;
	
	rl = ntfs_malloc_nofs(rlsize = PAGE_SIZE);
	if (unlikely(!rl))
		return ERR_PTR(-ENOMEM);
	
	if (vcn) {
		rl->vcn = 0;
		rl->lcn = LCN_RL_NOT_MAPPED;
		rl->length = vcn;
		rlpos++;
	}
	while (buf < attr_end && *buf) {
		if (((rlpos + 3) * sizeof(*old_rl)) > rlsize) {
			runlist_element *rl2;

			rl2 = ntfs_malloc_nofs(rlsize + (int)PAGE_SIZE);
			if (unlikely(!rl2)) {
				ntfs_free(rl);
				return ERR_PTR(-ENOMEM);
			}
			memcpy(rl2, rl, rlsize);
			ntfs_free(rl);
			rl = rl2;
			rlsize += PAGE_SIZE;
		}
		
		rl[rlpos].vcn = vcn;
		b = *buf & 0xf;
		if (b) {
			if (unlikely(buf + b > attr_end))
				goto io_error;
			for (deltaxcn = (s8)buf[b--]; b; b--)
				deltaxcn = (deltaxcn << 8) + buf[b];
		} else { 
			ntfs_error(vol->sb, "Missing length entry in mapping "
					"pairs array.");
			deltaxcn = (s64)-1;
		}
		if (unlikely(deltaxcn < 0)) {
			ntfs_error(vol->sb, "Invalid length in mapping pairs "
					"array.");
			goto err_out;
		}
		rl[rlpos].length = deltaxcn;
		
		vcn += deltaxcn;
		if (!(*buf & 0xf0))
			rl[rlpos].lcn = LCN_HOLE;
		else {
			
			u8 b2 = *buf & 0xf;
			b = b2 + ((*buf >> 4) & 0xf);
			if (buf + b > attr_end)
				goto io_error;
			for (deltaxcn = (s8)buf[b--]; b > b2; b--)
				deltaxcn = (deltaxcn << 8) + buf[b];
			
			lcn += deltaxcn;
#ifdef DEBUG
			if (vol->major_ver < 3) {
				if (unlikely(deltaxcn == (LCN)-1))
					ntfs_error(vol->sb, "lcn delta == -1");
				if (unlikely(lcn == (LCN)-1))
					ntfs_error(vol->sb, "lcn == -1");
			}
#endif
			
			if (unlikely(lcn < (LCN)-1)) {
				ntfs_error(vol->sb, "Invalid LCN < -1 in "
						"mapping pairs array.");
				goto err_out;
			}
			
			rl[rlpos].lcn = lcn;
		}
		
		rlpos++;
		
		buf += (*buf & 0xf) + ((*buf >> 4) & 0xf) + 1;
	}
	if (unlikely(buf >= attr_end))
		goto io_error;
	deltaxcn = sle64_to_cpu(attr->data.non_resident.highest_vcn);
	if (unlikely(deltaxcn && vcn - 1 != deltaxcn)) {
mpa_err:
		ntfs_error(vol->sb, "Corrupt mapping pairs array in "
				"non-resident attribute.");
		goto err_out;
	}
	
	if (!attr->data.non_resident.lowest_vcn) {
		VCN max_cluster;

		max_cluster = ((sle64_to_cpu(
				attr->data.non_resident.allocated_size) +
				vol->cluster_size - 1) >>
				vol->cluster_size_bits) - 1;
		if (deltaxcn) {
			if (deltaxcn < max_cluster) {
				ntfs_debug("More extents to follow; deltaxcn "
						"= 0x%llx, max_cluster = "
						"0x%llx",
						(unsigned long long)deltaxcn,
						(unsigned long long)
						max_cluster);
				rl[rlpos].vcn = vcn;
				vcn += rl[rlpos].length = max_cluster -
						deltaxcn;
				rl[rlpos].lcn = LCN_RL_NOT_MAPPED;
				rlpos++;
			} else if (unlikely(deltaxcn > max_cluster)) {
				ntfs_error(vol->sb, "Corrupt attribute.  "
						"deltaxcn = 0x%llx, "
						"max_cluster = 0x%llx",
						(unsigned long long)deltaxcn,
						(unsigned long long)
						max_cluster);
				goto mpa_err;
			}
		}
		rl[rlpos].lcn = LCN_ENOENT;
	} else 
		rl[rlpos].lcn = LCN_RL_NOT_MAPPED;

	
	rl[rlpos].vcn = vcn;
	rl[rlpos].length = (s64)0;
	
	if (!old_rl) {
		ntfs_debug("Mapping pairs array successfully decompressed:");
		ntfs_debug_dump_runlist(rl);
		return rl;
	}
	
	old_rl = ntfs_runlists_merge(old_rl, rl);
	if (likely(!IS_ERR(old_rl)))
		return old_rl;
	ntfs_free(rl);
	ntfs_error(vol->sb, "Failed to merge runlists.");
	return old_rl;
io_error:
	ntfs_error(vol->sb, "Corrupt attribute.");
err_out:
	ntfs_free(rl);
	return ERR_PTR(-EIO);
}

LCN ntfs_rl_vcn_to_lcn(const runlist_element *rl, const VCN vcn)
{
	int i;

	BUG_ON(vcn < 0);
	if (unlikely(!rl))
		return LCN_RL_NOT_MAPPED;

	
	if (unlikely(vcn < rl[0].vcn))
		return LCN_ENOENT;

	for (i = 0; likely(rl[i].length); i++) {
		if (unlikely(vcn < rl[i+1].vcn)) {
			if (likely(rl[i].lcn >= (LCN)0))
				return rl[i].lcn + (vcn - rl[i].vcn);
			return rl[i].lcn;
		}
	}
	if (likely(rl[i].lcn < (LCN)0))
		return rl[i].lcn;
	
	return LCN_ENOENT;
}

#ifdef NTFS_RW

runlist_element *ntfs_rl_find_vcn_nolock(runlist_element *rl, const VCN vcn)
{
	BUG_ON(vcn < 0);
	if (unlikely(!rl || vcn < rl[0].vcn))
		return NULL;
	while (likely(rl->length)) {
		if (unlikely(vcn < rl[1].vcn)) {
			if (likely(rl->lcn >= LCN_HOLE))
				return rl;
			return NULL;
		}
		rl++;
	}
	if (likely(rl->lcn == LCN_ENOENT))
		return rl;
	return NULL;
}

/**
 * ntfs_get_nr_significant_bytes - get number of bytes needed to store a number
 * @n:		number for which to get the number of bytes for
 *
 * Return the number of bytes required to store @n unambiguously as
 * a signed number.
 *
 * This is used in the context of the mapping pairs array to determine how
 * many bytes will be needed in the array to store a given logical cluster
 * number (lcn) or a specific run length.
 *
 * Return the number of bytes written.  This function cannot fail.
 */
static inline int ntfs_get_nr_significant_bytes(const s64 n)
{
	s64 l = n;
	int i;
	s8 j;

	i = 0;
	do {
		l >>= 8;
		i++;
	} while (l != 0 && l != -1);
	j = (n >> 8 * (i - 1)) & 0xff;
	
	if ((n < 0 && j >= 0) || (n > 0 && j < 0))
		i++;
	return i;
}

int ntfs_get_size_for_mapping_pairs(const ntfs_volume *vol,
		const runlist_element *rl, const VCN first_vcn,
		const VCN last_vcn)
{
	LCN prev_lcn;
	int rls;
	bool the_end = false;

	BUG_ON(first_vcn < 0);
	BUG_ON(last_vcn < -1);
	BUG_ON(last_vcn >= 0 && first_vcn > last_vcn);
	if (!rl) {
		BUG_ON(first_vcn);
		BUG_ON(last_vcn > 0);
		return 1;
	}
	
	while (rl->length && first_vcn >= rl[1].vcn)
		rl++;
	if (unlikely((!rl->length && first_vcn > rl->vcn) ||
			first_vcn < rl->vcn))
		return -EINVAL;
	prev_lcn = 0;
	
	rls = 1;
	
	if (first_vcn > rl->vcn) {
		s64 delta, length = rl->length;

		
		if (unlikely(length < 0 || rl->lcn < LCN_HOLE))
			goto err_out;
		if (unlikely(last_vcn >= 0 && rl[1].vcn > last_vcn)) {
			s64 s1 = last_vcn + 1;
			if (unlikely(rl[1].vcn > s1))
				length = s1 - rl->vcn;
			the_end = true;
		}
		delta = first_vcn - rl->vcn;
		
		rls += 1 + ntfs_get_nr_significant_bytes(length - delta);
		if (likely(rl->lcn >= 0 || vol->major_ver < 3)) {
			prev_lcn = rl->lcn;
			if (likely(rl->lcn >= 0))
				prev_lcn += delta;
			
			rls += ntfs_get_nr_significant_bytes(prev_lcn);
		}
		
		rl++;
	}
	
	for (; rl->length && !the_end; rl++) {
		s64 length = rl->length;

		if (unlikely(length < 0 || rl->lcn < LCN_HOLE))
			goto err_out;
		if (unlikely(last_vcn >= 0 && rl[1].vcn > last_vcn)) {
			s64 s1 = last_vcn + 1;
			if (unlikely(rl[1].vcn > s1))
				length = s1 - rl->vcn;
			the_end = true;
		}
		
		rls += 1 + ntfs_get_nr_significant_bytes(length);
		if (likely(rl->lcn >= 0 || vol->major_ver < 3)) {
			
			rls += ntfs_get_nr_significant_bytes(rl->lcn -
					prev_lcn);
			prev_lcn = rl->lcn;
		}
	}
	return rls;
err_out:
	if (rl->lcn == LCN_RL_NOT_MAPPED)
		rls = -EINVAL;
	else
		rls = -EIO;
	return rls;
}

/**
 * ntfs_write_significant_bytes - write the significant bytes of a number
 * @dst:	destination buffer to write to
 * @dst_max:	pointer to last byte of destination buffer for bounds checking
 * @n:		number whose significant bytes to write
 *
 * Store in @dst, the minimum bytes of the number @n which are required to
 * identify @n unambiguously as a signed number, taking care not to exceed
 * @dest_max, the maximum position within @dst to which we are allowed to
 * write.
 *
 * This is used when building the mapping pairs array of a runlist to compress
 * a given logical cluster number (lcn) or a specific run length to the minimum
 * size possible.
 *
 * Return the number of bytes written on success.  On error, i.e. the
 * destination buffer @dst is too small, return -ENOSPC.
 */
static inline int ntfs_write_significant_bytes(s8 *dst, const s8 *dst_max,
		const s64 n)
{
	s64 l = n;
	int i;
	s8 j;

	i = 0;
	do {
		if (unlikely(dst > dst_max))
			goto err_out;
		*dst++ = l & 0xffll;
		l >>= 8;
		i++;
	} while (l != 0 && l != -1);
	j = (n >> 8 * (i - 1)) & 0xff;
	
	if (n < 0 && j >= 0) {
		if (unlikely(dst > dst_max))
			goto err_out;
		i++;
		*dst = (s8)-1;
	} else if (n > 0 && j < 0) {
		if (unlikely(dst > dst_max))
			goto err_out;
		i++;
		*dst = (s8)0;
	}
	return i;
err_out:
	return -ENOSPC;
}

int ntfs_mapping_pairs_build(const ntfs_volume *vol, s8 *dst,
		const int dst_len, const runlist_element *rl,
		const VCN first_vcn, const VCN last_vcn, VCN *const stop_vcn)
{
	LCN prev_lcn;
	s8 *dst_max, *dst_next;
	int err = -ENOSPC;
	bool the_end = false;
	s8 len_len, lcn_len;

	BUG_ON(first_vcn < 0);
	BUG_ON(last_vcn < -1);
	BUG_ON(last_vcn >= 0 && first_vcn > last_vcn);
	BUG_ON(dst_len < 1);
	if (!rl) {
		BUG_ON(first_vcn);
		BUG_ON(last_vcn > 0);
		if (stop_vcn)
			*stop_vcn = 0;
		
		*dst = 0;
		return 0;
	}
	
	while (rl->length && first_vcn >= rl[1].vcn)
		rl++;
	if (unlikely((!rl->length && first_vcn > rl->vcn) ||
			first_vcn < rl->vcn))
		return -EINVAL;
	dst_max = dst + dst_len - 1;
	prev_lcn = 0;
	
	if (first_vcn > rl->vcn) {
		s64 delta, length = rl->length;

		
		if (unlikely(length < 0 || rl->lcn < LCN_HOLE))
			goto err_out;
		if (unlikely(last_vcn >= 0 && rl[1].vcn > last_vcn)) {
			s64 s1 = last_vcn + 1;
			if (unlikely(rl[1].vcn > s1))
				length = s1 - rl->vcn;
			the_end = true;
		}
		delta = first_vcn - rl->vcn;
		
		len_len = ntfs_write_significant_bytes(dst + 1, dst_max,
				length - delta);
		if (unlikely(len_len < 0))
			goto size_err;
		if (likely(rl->lcn >= 0 || vol->major_ver < 3)) {
			prev_lcn = rl->lcn;
			if (likely(rl->lcn >= 0))
				prev_lcn += delta;
			
			lcn_len = ntfs_write_significant_bytes(dst + 1 +
					len_len, dst_max, prev_lcn);
			if (unlikely(lcn_len < 0))
				goto size_err;
		} else
			lcn_len = 0;
		dst_next = dst + len_len + lcn_len + 1;
		if (unlikely(dst_next > dst_max))
			goto size_err;
		
		*dst = lcn_len << 4 | len_len;
		
		dst = dst_next;
		
		rl++;
	}
	
	for (; rl->length && !the_end; rl++) {
		s64 length = rl->length;

		if (unlikely(length < 0 || rl->lcn < LCN_HOLE))
			goto err_out;
		if (unlikely(last_vcn >= 0 && rl[1].vcn > last_vcn)) {
			s64 s1 = last_vcn + 1;
			if (unlikely(rl[1].vcn > s1))
				length = s1 - rl->vcn;
			the_end = true;
		}
		
		len_len = ntfs_write_significant_bytes(dst + 1, dst_max,
				length);
		if (unlikely(len_len < 0))
			goto size_err;
		if (likely(rl->lcn >= 0 || vol->major_ver < 3)) {
			
			lcn_len = ntfs_write_significant_bytes(dst + 1 +
					len_len, dst_max, rl->lcn - prev_lcn);
			if (unlikely(lcn_len < 0))
				goto size_err;
			prev_lcn = rl->lcn;
		} else
			lcn_len = 0;
		dst_next = dst + len_len + lcn_len + 1;
		if (unlikely(dst_next > dst_max))
			goto size_err;
		
		*dst = lcn_len << 4 | len_len;
		
		dst = dst_next;
	}
	
	err = 0;
size_err:
	
	if (stop_vcn)
		*stop_vcn = rl->vcn;
	
	*dst = 0;
	return err;
err_out:
	if (rl->lcn == LCN_RL_NOT_MAPPED)
		err = -EINVAL;
	else
		err = -EIO;
	return err;
}

int ntfs_rl_truncate_nolock(const ntfs_volume *vol, runlist *const runlist,
		const s64 new_length)
{
	runlist_element *rl;
	int old_size;

	ntfs_debug("Entering for new_length 0x%llx.", (long long)new_length);
	BUG_ON(!runlist);
	BUG_ON(new_length < 0);
	rl = runlist->rl;
	if (!new_length) {
		ntfs_debug("Freeing runlist.");
		runlist->rl = NULL;
		if (rl)
			ntfs_free(rl);
		return 0;
	}
	if (unlikely(!rl)) {
		rl = ntfs_malloc_nofs(PAGE_SIZE);
		if (unlikely(!rl)) {
			ntfs_error(vol->sb, "Not enough memory to allocate "
					"runlist element buffer.");
			return -ENOMEM;
		}
		runlist->rl = rl;
		rl[1].length = rl->vcn = 0;
		rl->lcn = LCN_HOLE;
		rl[1].vcn = rl->length = new_length;
		rl[1].lcn = LCN_ENOENT;
		return 0;
	}
	BUG_ON(new_length < rl->vcn);
	
	while (likely(rl->length && new_length >= rl[1].vcn))
		rl++;
	if (rl->length) {
		runlist_element *trl;
		bool is_end;

		ntfs_debug("Shrinking runlist.");
		
		trl = rl + 1;
		while (likely(trl->length))
			trl++;
		old_size = trl - runlist->rl + 1;
		
		rl->length = new_length - rl->vcn;
		is_end = false;
		if (rl->length) {
			rl++;
			if (!rl->length)
				is_end = true;
			rl->vcn = new_length;
			rl->length = 0;
		}
		rl->lcn = LCN_ENOENT;
		
		if (!is_end) {
			int new_size = rl - runlist->rl + 1;
			rl = ntfs_rl_realloc(runlist->rl, old_size, new_size);
			if (IS_ERR(rl))
				ntfs_warning(vol->sb, "Failed to shrink "
						"runlist buffer.  This just "
						"wastes a bit of memory "
						"temporarily so we ignore it "
						"and return success.");
			else
				runlist->rl = rl;
		}
	} else if (likely( new_length > rl->vcn)) {
		ntfs_debug("Expanding runlist.");
		if ((rl > runlist->rl) && ((rl - 1)->lcn == LCN_HOLE))
			(rl - 1)->length = new_length - (rl - 1)->vcn;
		else {
			
			old_size = rl - runlist->rl + 1;
			
			rl = ntfs_rl_realloc(runlist->rl, old_size,
					old_size + 1);
			if (IS_ERR(rl)) {
				ntfs_error(vol->sb, "Failed to expand runlist "
						"buffer, aborting.");
				return PTR_ERR(rl);
			}
			runlist->rl = rl;
			rl += old_size - 1;
			
			rl->lcn = LCN_HOLE;
			rl->length = new_length - rl->vcn;
			
			rl++;
			rl->length = 0;
		}
		rl->vcn = new_length;
		rl->lcn = LCN_ENOENT;
	} else  {
		
		rl->lcn = LCN_ENOENT;
	}
	ntfs_debug("Done.");
	return 0;
}

int ntfs_rl_punch_nolock(const ntfs_volume *vol, runlist *const runlist,
		const VCN start, const s64 length)
{
	const VCN end = start + length;
	s64 delta;
	runlist_element *rl, *rl_end, *rl_real_end, *trl;
	int old_size;
	bool lcn_fixup = false;

	ntfs_debug("Entering for start 0x%llx, length 0x%llx.",
			(long long)start, (long long)length);
	BUG_ON(!runlist);
	BUG_ON(start < 0);
	BUG_ON(length < 0);
	BUG_ON(end < 0);
	rl = runlist->rl;
	if (unlikely(!rl)) {
		if (likely(!start && !length))
			return 0;
		return -ENOENT;
	}
	
	while (likely(rl->length && start >= rl[1].vcn))
		rl++;
	rl_end = rl;
	
	while (likely(rl_end->length && end >= rl_end[1].vcn)) {
		
		if (unlikely(rl_end->lcn < LCN_HOLE))
			return -EINVAL;
		rl_end++;
	}
	
	if (unlikely(rl_end->length && rl_end->lcn < LCN_HOLE))
		return -EINVAL;
	
	if (!rl_end->length && end > rl_end->vcn)
		return -ENOENT;
	if (!length)
		return 0;
	if (!rl->length)
		return -ENOENT;
	rl_real_end = rl_end;
	
	while (likely(rl_real_end->length))
		rl_real_end++;
	old_size = rl_real_end - runlist->rl + 1;
	
	if (rl->lcn == LCN_HOLE) {
		if (end <= rl[1].vcn) {
			ntfs_debug("Done (requested hole is already sparse).");
			return 0;
		}
extend_hole:
		
		rl->length = end - rl->vcn;
		
		if (rl_end->lcn == LCN_HOLE) {
			rl_end++;
			rl->length = rl_end->vcn - rl->vcn;
		}
		
		rl++;
		
		if (rl < rl_end)
			memmove(rl, rl_end, (rl_real_end - rl_end + 1) *
					sizeof(*rl));
		
		if (end > rl->vcn) {
			delta = end - rl->vcn;
			rl->vcn = end;
			rl->length -= delta;
			
			if (rl->lcn >= 0)
				rl->lcn += delta;
		}
shrink_allocation:
		
		if (rl < rl_end) {
			rl = ntfs_rl_realloc(runlist->rl, old_size,
					old_size - (rl_end - rl));
			if (IS_ERR(rl))
				ntfs_warning(vol->sb, "Failed to shrink "
						"runlist buffer.  This just "
						"wastes a bit of memory "
						"temporarily so we ignore it "
						"and return success.");
			else
				runlist->rl = rl;
		}
		ntfs_debug("Done (extend hole).");
		return 0;
	}
	if (start == rl->vcn) {
		if (rl > runlist->rl && (rl - 1)->lcn == LCN_HOLE) {
			rl--;
			goto extend_hole;
		}
		if (end >= rl[1].vcn) {
			rl->lcn = LCN_HOLE;
			goto extend_hole;
		}
		trl = ntfs_rl_realloc(runlist->rl, old_size, old_size + 1);
		if (IS_ERR(trl))
			goto enomem_out;
		old_size++;
		if (runlist->rl != trl) {
			rl = trl + (rl - runlist->rl);
			rl_end = trl + (rl_end - runlist->rl);
			rl_real_end = trl + (rl_real_end - runlist->rl);
			runlist->rl = trl;
		}
split_end:
		
		memmove(rl + 1, rl, (rl_real_end - rl + 1) * sizeof(*rl));
		
		rl->lcn = LCN_HOLE;
		rl->length = length;
		rl++;
		rl->vcn += length;
		
		if (rl->lcn >= 0 || lcn_fixup)
			rl->lcn += length;
		rl->length -= length;
		ntfs_debug("Done (split one).");
		return 0;
	}
	if (rl_end->lcn == LCN_HOLE) {
		
		rl->length = start - rl->vcn;
		rl++;
		
		if (rl < rl_end)
			memmove(rl, rl_end, (rl_real_end - rl_end + 1) *
					sizeof(*rl));
		
		rl->vcn = start;
		rl->length = rl[1].vcn - start;
		goto shrink_allocation;
	}
	if (end >= rl[1].vcn) {
		if (rl[1].length && end >= rl[2].vcn) {
			
			rl->length = start - rl->vcn;
			rl++;
			rl->vcn = start;
			rl->lcn = LCN_HOLE;
			goto extend_hole;
		}
		trl = ntfs_rl_realloc(runlist->rl, old_size, old_size + 1);
		if (IS_ERR(trl))
			goto enomem_out;
		old_size++;
		if (runlist->rl != trl) {
			rl = trl + (rl - runlist->rl);
			rl_end = trl + (rl_end - runlist->rl);
			rl_real_end = trl + (rl_real_end - runlist->rl);
			runlist->rl = trl;
		}
		
		rl->length = start - rl->vcn;
		rl++;
		delta = rl->vcn - start;
		rl->vcn = start;
		if (rl->lcn >= 0) {
			rl->lcn -= delta;
			
			lcn_fixup = true;
		}
		rl->length += delta;
		goto split_end;
	}
	trl = ntfs_rl_realloc(runlist->rl, old_size, old_size + 2);
	if (IS_ERR(trl))
		goto enomem_out;
	old_size += 2;
	if (runlist->rl != trl) {
		rl = trl + (rl - runlist->rl);
		rl_end = trl + (rl_end - runlist->rl);
		rl_real_end = trl + (rl_real_end - runlist->rl);
		runlist->rl = trl;
	}
	
	memmove(rl + 2, rl, (rl_real_end - rl + 1) * sizeof(*rl));
	
	rl->length = start - rl->vcn;
	rl++;
	rl->vcn = start;
	rl->lcn = LCN_HOLE;
	rl->length = length;
	rl++;
	delta = end - rl->vcn;
	rl->vcn = end;
	rl->lcn += delta;
	rl->length -= delta;
	ntfs_debug("Done (split both).");
	return 0;
enomem_out:
	ntfs_error(vol->sb, "Not enough memory to extend runlist buffer.");
	return -ENOMEM;
}

#endif 
