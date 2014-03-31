
/*
 * (c) Copyright Hewlett-Packard Development Company, L.P., 2006, 2008
 *
 * This program is free software;  you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY;  without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program;  if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/rcupdate.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/jhash.h>
#include <linux/audit.h>
#include <linux/slab.h>
#include <net/ip.h>
#include <net/icmp.h>
#include <net/tcp.h>
#include <net/netlabel.h>
#include <net/cipso_ipv4.h>
#include <linux/atomic.h>
#include <asm/bug.h>
#include <asm/unaligned.h>

static DEFINE_SPINLOCK(cipso_v4_doi_list_lock);
static LIST_HEAD(cipso_v4_doi_list);

int cipso_v4_cache_enabled = 1;
int cipso_v4_cache_bucketsize = 10;
#define CIPSO_V4_CACHE_BUCKETBITS     7
#define CIPSO_V4_CACHE_BUCKETS        (1 << CIPSO_V4_CACHE_BUCKETBITS)
#define CIPSO_V4_CACHE_REORDERLIMIT   10
struct cipso_v4_map_cache_bkt {
	spinlock_t lock;
	u32 size;
	struct list_head list;
};
struct cipso_v4_map_cache_entry {
	u32 hash;
	unsigned char *key;
	size_t key_len;

	struct netlbl_lsm_cache *lsm_data;

	u32 activity;
	struct list_head list;
};
static struct cipso_v4_map_cache_bkt *cipso_v4_cache = NULL;

int cipso_v4_rbm_optfmt = 0;
int cipso_v4_rbm_strictvalid = 1;


#define CIPSO_V4_OPT_LEN_MAX          40

#define CIPSO_V4_HDR_LEN              6

#define CIPSO_V4_TAG_RBM_BLEN         4

#define CIPSO_V4_TAG_ENUM_BLEN        4

#define CIPSO_V4_TAG_RNG_BLEN         4
#define CIPSO_V4_TAG_RNG_CAT_MAX      8

#define CIPSO_V4_TAG_LOC_BLEN         6


static int cipso_v4_bitmap_walk(const unsigned char *bitmap,
				u32 bitmap_len,
				u32 offset,
				u8 state)
{
	u32 bit_spot;
	u32 byte_offset;
	unsigned char bitmask;
	unsigned char byte;

	
	byte_offset = offset / 8;
	byte = bitmap[byte_offset];
	bit_spot = offset;
	bitmask = 0x80 >> (offset % 8);

	while (bit_spot < bitmap_len) {
		if ((state && (byte & bitmask) == bitmask) ||
		    (state == 0 && (byte & bitmask) == 0))
			return bit_spot;

		bit_spot++;
		bitmask >>= 1;
		if (bitmask == 0) {
			byte = bitmap[++byte_offset];
			bitmask = 0x80;
		}
	}

	return -1;
}

static void cipso_v4_bitmap_setbit(unsigned char *bitmap,
				   u32 bit,
				   u8 state)
{
	u32 byte_spot;
	u8 bitmask;

	
	byte_spot = bit / 8;
	bitmask = 0x80 >> (bit % 8);
	if (state)
		bitmap[byte_spot] |= bitmask;
	else
		bitmap[byte_spot] &= ~bitmask;
}

static void cipso_v4_cache_entry_free(struct cipso_v4_map_cache_entry *entry)
{
	if (entry->lsm_data)
		netlbl_secattr_cache_free(entry->lsm_data);
	kfree(entry->key);
	kfree(entry);
}

static u32 cipso_v4_map_cache_hash(const unsigned char *key, u32 key_len)
{
	return jhash(key, key_len, 0);
}


static int cipso_v4_cache_init(void)
{
	u32 iter;

	cipso_v4_cache = kcalloc(CIPSO_V4_CACHE_BUCKETS,
				 sizeof(struct cipso_v4_map_cache_bkt),
				 GFP_KERNEL);
	if (cipso_v4_cache == NULL)
		return -ENOMEM;

	for (iter = 0; iter < CIPSO_V4_CACHE_BUCKETS; iter++) {
		spin_lock_init(&cipso_v4_cache[iter].lock);
		cipso_v4_cache[iter].size = 0;
		INIT_LIST_HEAD(&cipso_v4_cache[iter].list);
	}

	return 0;
}

void cipso_v4_cache_invalidate(void)
{
	struct cipso_v4_map_cache_entry *entry, *tmp_entry;
	u32 iter;

	for (iter = 0; iter < CIPSO_V4_CACHE_BUCKETS; iter++) {
		spin_lock_bh(&cipso_v4_cache[iter].lock);
		list_for_each_entry_safe(entry,
					 tmp_entry,
					 &cipso_v4_cache[iter].list, list) {
			list_del(&entry->list);
			cipso_v4_cache_entry_free(entry);
		}
		cipso_v4_cache[iter].size = 0;
		spin_unlock_bh(&cipso_v4_cache[iter].lock);
	}
}

static int cipso_v4_cache_check(const unsigned char *key,
				u32 key_len,
				struct netlbl_lsm_secattr *secattr)
{
	u32 bkt;
	struct cipso_v4_map_cache_entry *entry;
	struct cipso_v4_map_cache_entry *prev_entry = NULL;
	u32 hash;

	if (!cipso_v4_cache_enabled)
		return -ENOENT;

	hash = cipso_v4_map_cache_hash(key, key_len);
	bkt = hash & (CIPSO_V4_CACHE_BUCKETS - 1);
	spin_lock_bh(&cipso_v4_cache[bkt].lock);
	list_for_each_entry(entry, &cipso_v4_cache[bkt].list, list) {
		if (entry->hash == hash &&
		    entry->key_len == key_len &&
		    memcmp(entry->key, key, key_len) == 0) {
			entry->activity += 1;
			atomic_inc(&entry->lsm_data->refcount);
			secattr->cache = entry->lsm_data;
			secattr->flags |= NETLBL_SECATTR_CACHE;
			secattr->type = NETLBL_NLTYPE_CIPSOV4;
			if (prev_entry == NULL) {
				spin_unlock_bh(&cipso_v4_cache[bkt].lock);
				return 0;
			}

			if (prev_entry->activity > 0)
				prev_entry->activity -= 1;
			if (entry->activity > prev_entry->activity &&
			    entry->activity - prev_entry->activity >
			    CIPSO_V4_CACHE_REORDERLIMIT) {
				__list_del(entry->list.prev, entry->list.next);
				__list_add(&entry->list,
					   prev_entry->list.prev,
					   &prev_entry->list);
			}

			spin_unlock_bh(&cipso_v4_cache[bkt].lock);
			return 0;
		}
		prev_entry = entry;
	}
	spin_unlock_bh(&cipso_v4_cache[bkt].lock);

	return -ENOENT;
}

int cipso_v4_cache_add(const struct sk_buff *skb,
		       const struct netlbl_lsm_secattr *secattr)
{
	int ret_val = -EPERM;
	u32 bkt;
	struct cipso_v4_map_cache_entry *entry = NULL;
	struct cipso_v4_map_cache_entry *old_entry = NULL;
	unsigned char *cipso_ptr;
	u32 cipso_ptr_len;

	if (!cipso_v4_cache_enabled || cipso_v4_cache_bucketsize <= 0)
		return 0;

	cipso_ptr = CIPSO_V4_OPTPTR(skb);
	cipso_ptr_len = cipso_ptr[1];

	entry = kzalloc(sizeof(*entry), GFP_ATOMIC);
	if (entry == NULL)
		return -ENOMEM;
	entry->key = kmemdup(cipso_ptr, cipso_ptr_len, GFP_ATOMIC);
	if (entry->key == NULL) {
		ret_val = -ENOMEM;
		goto cache_add_failure;
	}
	entry->key_len = cipso_ptr_len;
	entry->hash = cipso_v4_map_cache_hash(cipso_ptr, cipso_ptr_len);
	atomic_inc(&secattr->cache->refcount);
	entry->lsm_data = secattr->cache;

	bkt = entry->hash & (CIPSO_V4_CACHE_BUCKETS - 1);
	spin_lock_bh(&cipso_v4_cache[bkt].lock);
	if (cipso_v4_cache[bkt].size < cipso_v4_cache_bucketsize) {
		list_add(&entry->list, &cipso_v4_cache[bkt].list);
		cipso_v4_cache[bkt].size += 1;
	} else {
		old_entry = list_entry(cipso_v4_cache[bkt].list.prev,
				       struct cipso_v4_map_cache_entry, list);
		list_del(&old_entry->list);
		list_add(&entry->list, &cipso_v4_cache[bkt].list);
		cipso_v4_cache_entry_free(old_entry);
	}
	spin_unlock_bh(&cipso_v4_cache[bkt].lock);

	return 0;

cache_add_failure:
	if (entry)
		cipso_v4_cache_entry_free(entry);
	return ret_val;
}


static struct cipso_v4_doi *cipso_v4_doi_search(u32 doi)
{
	struct cipso_v4_doi *iter;

	list_for_each_entry_rcu(iter, &cipso_v4_doi_list, list)
		if (iter->doi == doi && atomic_read(&iter->refcount))
			return iter;
	return NULL;
}

int cipso_v4_doi_add(struct cipso_v4_doi *doi_def,
		     struct netlbl_audit *audit_info)
{
	int ret_val = -EINVAL;
	u32 iter;
	u32 doi;
	u32 doi_type;
	struct audit_buffer *audit_buf;

	doi = doi_def->doi;
	doi_type = doi_def->type;

	if (doi_def->doi == CIPSO_V4_DOI_UNKNOWN)
		goto doi_add_return;
	for (iter = 0; iter < CIPSO_V4_TAG_MAXCNT; iter++) {
		switch (doi_def->tags[iter]) {
		case CIPSO_V4_TAG_RBITMAP:
			break;
		case CIPSO_V4_TAG_RANGE:
		case CIPSO_V4_TAG_ENUM:
			if (doi_def->type != CIPSO_V4_MAP_PASS)
				goto doi_add_return;
			break;
		case CIPSO_V4_TAG_LOCAL:
			if (doi_def->type != CIPSO_V4_MAP_LOCAL)
				goto doi_add_return;
			break;
		case CIPSO_V4_TAG_INVALID:
			if (iter == 0)
				goto doi_add_return;
			break;
		default:
			goto doi_add_return;
		}
	}

	atomic_set(&doi_def->refcount, 1);

	spin_lock(&cipso_v4_doi_list_lock);
	if (cipso_v4_doi_search(doi_def->doi) != NULL) {
		spin_unlock(&cipso_v4_doi_list_lock);
		ret_val = -EEXIST;
		goto doi_add_return;
	}
	list_add_tail_rcu(&doi_def->list, &cipso_v4_doi_list);
	spin_unlock(&cipso_v4_doi_list_lock);
	ret_val = 0;

doi_add_return:
	audit_buf = netlbl_audit_start(AUDIT_MAC_CIPSOV4_ADD, audit_info);
	if (audit_buf != NULL) {
		const char *type_str;
		switch (doi_type) {
		case CIPSO_V4_MAP_TRANS:
			type_str = "trans";
			break;
		case CIPSO_V4_MAP_PASS:
			type_str = "pass";
			break;
		case CIPSO_V4_MAP_LOCAL:
			type_str = "local";
			break;
		default:
			type_str = "(unknown)";
		}
		audit_log_format(audit_buf,
				 " cipso_doi=%u cipso_type=%s res=%u",
				 doi, type_str, ret_val == 0 ? 1 : 0);
		audit_log_end(audit_buf);
	}

	return ret_val;
}

void cipso_v4_doi_free(struct cipso_v4_doi *doi_def)
{
	if (doi_def == NULL)
		return;

	switch (doi_def->type) {
	case CIPSO_V4_MAP_TRANS:
		kfree(doi_def->map.std->lvl.cipso);
		kfree(doi_def->map.std->lvl.local);
		kfree(doi_def->map.std->cat.cipso);
		kfree(doi_def->map.std->cat.local);
		break;
	}
	kfree(doi_def);
}

static void cipso_v4_doi_free_rcu(struct rcu_head *entry)
{
	struct cipso_v4_doi *doi_def;

	doi_def = container_of(entry, struct cipso_v4_doi, rcu);
	cipso_v4_doi_free(doi_def);
}

int cipso_v4_doi_remove(u32 doi, struct netlbl_audit *audit_info)
{
	int ret_val;
	struct cipso_v4_doi *doi_def;
	struct audit_buffer *audit_buf;

	spin_lock(&cipso_v4_doi_list_lock);
	doi_def = cipso_v4_doi_search(doi);
	if (doi_def == NULL) {
		spin_unlock(&cipso_v4_doi_list_lock);
		ret_val = -ENOENT;
		goto doi_remove_return;
	}
	if (!atomic_dec_and_test(&doi_def->refcount)) {
		spin_unlock(&cipso_v4_doi_list_lock);
		ret_val = -EBUSY;
		goto doi_remove_return;
	}
	list_del_rcu(&doi_def->list);
	spin_unlock(&cipso_v4_doi_list_lock);

	cipso_v4_cache_invalidate();
	call_rcu(&doi_def->rcu, cipso_v4_doi_free_rcu);
	ret_val = 0;

doi_remove_return:
	audit_buf = netlbl_audit_start(AUDIT_MAC_CIPSOV4_DEL, audit_info);
	if (audit_buf != NULL) {
		audit_log_format(audit_buf,
				 " cipso_doi=%u res=%u",
				 doi, ret_val == 0 ? 1 : 0);
		audit_log_end(audit_buf);
	}

	return ret_val;
}

struct cipso_v4_doi *cipso_v4_doi_getdef(u32 doi)
{
	struct cipso_v4_doi *doi_def;

	rcu_read_lock();
	doi_def = cipso_v4_doi_search(doi);
	if (doi_def == NULL)
		goto doi_getdef_return;
	if (!atomic_inc_not_zero(&doi_def->refcount))
		doi_def = NULL;

doi_getdef_return:
	rcu_read_unlock();
	return doi_def;
}

void cipso_v4_doi_putdef(struct cipso_v4_doi *doi_def)
{
	if (doi_def == NULL)
		return;

	if (!atomic_dec_and_test(&doi_def->refcount))
		return;
	spin_lock(&cipso_v4_doi_list_lock);
	list_del_rcu(&doi_def->list);
	spin_unlock(&cipso_v4_doi_list_lock);

	cipso_v4_cache_invalidate();
	call_rcu(&doi_def->rcu, cipso_v4_doi_free_rcu);
}

int cipso_v4_doi_walk(u32 *skip_cnt,
		     int (*callback) (struct cipso_v4_doi *doi_def, void *arg),
		     void *cb_arg)
{
	int ret_val = -ENOENT;
	u32 doi_cnt = 0;
	struct cipso_v4_doi *iter_doi;

	rcu_read_lock();
	list_for_each_entry_rcu(iter_doi, &cipso_v4_doi_list, list)
		if (atomic_read(&iter_doi->refcount) > 0) {
			if (doi_cnt++ < *skip_cnt)
				continue;
			ret_val = callback(iter_doi, cb_arg);
			if (ret_val < 0) {
				doi_cnt--;
				goto doi_walk_return;
			}
		}

doi_walk_return:
	rcu_read_unlock();
	*skip_cnt = doi_cnt;
	return ret_val;
}


static int cipso_v4_map_lvl_valid(const struct cipso_v4_doi *doi_def, u8 level)
{
	switch (doi_def->type) {
	case CIPSO_V4_MAP_PASS:
		return 0;
	case CIPSO_V4_MAP_TRANS:
		if (doi_def->map.std->lvl.cipso[level] < CIPSO_V4_INV_LVL)
			return 0;
		break;
	}

	return -EFAULT;
}

static int cipso_v4_map_lvl_hton(const struct cipso_v4_doi *doi_def,
				 u32 host_lvl,
				 u32 *net_lvl)
{
	switch (doi_def->type) {
	case CIPSO_V4_MAP_PASS:
		*net_lvl = host_lvl;
		return 0;
	case CIPSO_V4_MAP_TRANS:
		if (host_lvl < doi_def->map.std->lvl.local_size &&
		    doi_def->map.std->lvl.local[host_lvl] < CIPSO_V4_INV_LVL) {
			*net_lvl = doi_def->map.std->lvl.local[host_lvl];
			return 0;
		}
		return -EPERM;
	}

	return -EINVAL;
}

static int cipso_v4_map_lvl_ntoh(const struct cipso_v4_doi *doi_def,
				 u32 net_lvl,
				 u32 *host_lvl)
{
	struct cipso_v4_std_map_tbl *map_tbl;

	switch (doi_def->type) {
	case CIPSO_V4_MAP_PASS:
		*host_lvl = net_lvl;
		return 0;
	case CIPSO_V4_MAP_TRANS:
		map_tbl = doi_def->map.std;
		if (net_lvl < map_tbl->lvl.cipso_size &&
		    map_tbl->lvl.cipso[net_lvl] < CIPSO_V4_INV_LVL) {
			*host_lvl = doi_def->map.std->lvl.cipso[net_lvl];
			return 0;
		}
		return -EPERM;
	}

	return -EINVAL;
}

static int cipso_v4_map_cat_rbm_valid(const struct cipso_v4_doi *doi_def,
				      const unsigned char *bitmap,
				      u32 bitmap_len)
{
	int cat = -1;
	u32 bitmap_len_bits = bitmap_len * 8;
	u32 cipso_cat_size;
	u32 *cipso_array;

	switch (doi_def->type) {
	case CIPSO_V4_MAP_PASS:
		return 0;
	case CIPSO_V4_MAP_TRANS:
		cipso_cat_size = doi_def->map.std->cat.cipso_size;
		cipso_array = doi_def->map.std->cat.cipso;
		for (;;) {
			cat = cipso_v4_bitmap_walk(bitmap,
						   bitmap_len_bits,
						   cat + 1,
						   1);
			if (cat < 0)
				break;
			if (cat >= cipso_cat_size ||
			    cipso_array[cat] >= CIPSO_V4_INV_CAT)
				return -EFAULT;
		}

		if (cat == -1)
			return 0;
		break;
	}

	return -EFAULT;
}

static int cipso_v4_map_cat_rbm_hton(const struct cipso_v4_doi *doi_def,
				     const struct netlbl_lsm_secattr *secattr,
				     unsigned char *net_cat,
				     u32 net_cat_len)
{
	int host_spot = -1;
	u32 net_spot = CIPSO_V4_INV_CAT;
	u32 net_spot_max = 0;
	u32 net_clen_bits = net_cat_len * 8;
	u32 host_cat_size = 0;
	u32 *host_cat_array = NULL;

	if (doi_def->type == CIPSO_V4_MAP_TRANS) {
		host_cat_size = doi_def->map.std->cat.local_size;
		host_cat_array = doi_def->map.std->cat.local;
	}

	for (;;) {
		host_spot = netlbl_secattr_catmap_walk(secattr->attr.mls.cat,
						       host_spot + 1);
		if (host_spot < 0)
			break;

		switch (doi_def->type) {
		case CIPSO_V4_MAP_PASS:
			net_spot = host_spot;
			break;
		case CIPSO_V4_MAP_TRANS:
			if (host_spot >= host_cat_size)
				return -EPERM;
			net_spot = host_cat_array[host_spot];
			if (net_spot >= CIPSO_V4_INV_CAT)
				return -EPERM;
			break;
		}
		if (net_spot >= net_clen_bits)
			return -ENOSPC;
		cipso_v4_bitmap_setbit(net_cat, net_spot, 1);

		if (net_spot > net_spot_max)
			net_spot_max = net_spot;
	}

	if (++net_spot_max % 8)
		return net_spot_max / 8 + 1;
	return net_spot_max / 8;
}

static int cipso_v4_map_cat_rbm_ntoh(const struct cipso_v4_doi *doi_def,
				     const unsigned char *net_cat,
				     u32 net_cat_len,
				     struct netlbl_lsm_secattr *secattr)
{
	int ret_val;
	int net_spot = -1;
	u32 host_spot = CIPSO_V4_INV_CAT;
	u32 net_clen_bits = net_cat_len * 8;
	u32 net_cat_size = 0;
	u32 *net_cat_array = NULL;

	if (doi_def->type == CIPSO_V4_MAP_TRANS) {
		net_cat_size = doi_def->map.std->cat.cipso_size;
		net_cat_array = doi_def->map.std->cat.cipso;
	}

	for (;;) {
		net_spot = cipso_v4_bitmap_walk(net_cat,
						net_clen_bits,
						net_spot + 1,
						1);
		if (net_spot < 0) {
			if (net_spot == -2)
				return -EFAULT;
			return 0;
		}

		switch (doi_def->type) {
		case CIPSO_V4_MAP_PASS:
			host_spot = net_spot;
			break;
		case CIPSO_V4_MAP_TRANS:
			if (net_spot >= net_cat_size)
				return -EPERM;
			host_spot = net_cat_array[net_spot];
			if (host_spot >= CIPSO_V4_INV_CAT)
				return -EPERM;
			break;
		}
		ret_val = netlbl_secattr_catmap_setbit(secattr->attr.mls.cat,
						       host_spot,
						       GFP_ATOMIC);
		if (ret_val != 0)
			return ret_val;
	}

	return -EINVAL;
}

static int cipso_v4_map_cat_enum_valid(const struct cipso_v4_doi *doi_def,
				       const unsigned char *enumcat,
				       u32 enumcat_len)
{
	u16 cat;
	int cat_prev = -1;
	u32 iter;

	if (doi_def->type != CIPSO_V4_MAP_PASS || enumcat_len & 0x01)
		return -EFAULT;

	for (iter = 0; iter < enumcat_len; iter += 2) {
		cat = get_unaligned_be16(&enumcat[iter]);
		if (cat <= cat_prev)
			return -EFAULT;
		cat_prev = cat;
	}

	return 0;
}

static int cipso_v4_map_cat_enum_hton(const struct cipso_v4_doi *doi_def,
				      const struct netlbl_lsm_secattr *secattr,
				      unsigned char *net_cat,
				      u32 net_cat_len)
{
	int cat = -1;
	u32 cat_iter = 0;

	for (;;) {
		cat = netlbl_secattr_catmap_walk(secattr->attr.mls.cat,
						 cat + 1);
		if (cat < 0)
			break;
		if ((cat_iter + 2) > net_cat_len)
			return -ENOSPC;

		*((__be16 *)&net_cat[cat_iter]) = htons(cat);
		cat_iter += 2;
	}

	return cat_iter;
}

static int cipso_v4_map_cat_enum_ntoh(const struct cipso_v4_doi *doi_def,
				      const unsigned char *net_cat,
				      u32 net_cat_len,
				      struct netlbl_lsm_secattr *secattr)
{
	int ret_val;
	u32 iter;

	for (iter = 0; iter < net_cat_len; iter += 2) {
		ret_val = netlbl_secattr_catmap_setbit(secattr->attr.mls.cat,
				get_unaligned_be16(&net_cat[iter]),
				GFP_ATOMIC);
		if (ret_val != 0)
			return ret_val;
	}

	return 0;
}

static int cipso_v4_map_cat_rng_valid(const struct cipso_v4_doi *doi_def,
				      const unsigned char *rngcat,
				      u32 rngcat_len)
{
	u16 cat_high;
	u16 cat_low;
	u32 cat_prev = CIPSO_V4_MAX_REM_CATS + 1;
	u32 iter;

	if (doi_def->type != CIPSO_V4_MAP_PASS || rngcat_len & 0x01)
		return -EFAULT;

	for (iter = 0; iter < rngcat_len; iter += 4) {
		cat_high = get_unaligned_be16(&rngcat[iter]);
		if ((iter + 4) <= rngcat_len)
			cat_low = get_unaligned_be16(&rngcat[iter + 2]);
		else
			cat_low = 0;

		if (cat_high > cat_prev)
			return -EFAULT;

		cat_prev = cat_low;
	}

	return 0;
}

static int cipso_v4_map_cat_rng_hton(const struct cipso_v4_doi *doi_def,
				     const struct netlbl_lsm_secattr *secattr,
				     unsigned char *net_cat,
				     u32 net_cat_len)
{
	int iter = -1;
	u16 array[CIPSO_V4_TAG_RNG_CAT_MAX * 2];
	u32 array_cnt = 0;
	u32 cat_size = 0;

	
	if (net_cat_len >
	    (CIPSO_V4_OPT_LEN_MAX - CIPSO_V4_HDR_LEN - CIPSO_V4_TAG_RNG_BLEN))
		return -ENOSPC;

	for (;;) {
		iter = netlbl_secattr_catmap_walk(secattr->attr.mls.cat,
						  iter + 1);
		if (iter < 0)
			break;
		cat_size += (iter == 0 ? 0 : sizeof(u16));
		if (cat_size > net_cat_len)
			return -ENOSPC;
		array[array_cnt++] = iter;

		iter = netlbl_secattr_catmap_walk_rng(secattr->attr.mls.cat,
						      iter);
		if (iter < 0)
			return -EFAULT;
		cat_size += sizeof(u16);
		if (cat_size > net_cat_len)
			return -ENOSPC;
		array[array_cnt++] = iter;
	}

	for (iter = 0; array_cnt > 0;) {
		*((__be16 *)&net_cat[iter]) = htons(array[--array_cnt]);
		iter += 2;
		array_cnt--;
		if (array[array_cnt] != 0) {
			*((__be16 *)&net_cat[iter]) = htons(array[array_cnt]);
			iter += 2;
		}
	}

	return cat_size;
}

static int cipso_v4_map_cat_rng_ntoh(const struct cipso_v4_doi *doi_def,
				     const unsigned char *net_cat,
				     u32 net_cat_len,
				     struct netlbl_lsm_secattr *secattr)
{
	int ret_val;
	u32 net_iter;
	u16 cat_low;
	u16 cat_high;

	for (net_iter = 0; net_iter < net_cat_len; net_iter += 4) {
		cat_high = get_unaligned_be16(&net_cat[net_iter]);
		if ((net_iter + 4) <= net_cat_len)
			cat_low = get_unaligned_be16(&net_cat[net_iter + 2]);
		else
			cat_low = 0;

		ret_val = netlbl_secattr_catmap_setrng(secattr->attr.mls.cat,
						       cat_low,
						       cat_high,
						       GFP_ATOMIC);
		if (ret_val != 0)
			return ret_val;
	}

	return 0;
}


static void cipso_v4_gentag_hdr(const struct cipso_v4_doi *doi_def,
				unsigned char *buf,
				u32 len)
{
	buf[0] = IPOPT_CIPSO;
	buf[1] = CIPSO_V4_HDR_LEN + len;
	*(__be32 *)&buf[2] = htonl(doi_def->doi);
}

static int cipso_v4_gentag_rbm(const struct cipso_v4_doi *doi_def,
			       const struct netlbl_lsm_secattr *secattr,
			       unsigned char *buffer,
			       u32 buffer_len)
{
	int ret_val;
	u32 tag_len;
	u32 level;

	if ((secattr->flags & NETLBL_SECATTR_MLS_LVL) == 0)
		return -EPERM;

	ret_val = cipso_v4_map_lvl_hton(doi_def,
					secattr->attr.mls.lvl,
					&level);
	if (ret_val != 0)
		return ret_val;

	if (secattr->flags & NETLBL_SECATTR_MLS_CAT) {
		ret_val = cipso_v4_map_cat_rbm_hton(doi_def,
						    secattr,
						    &buffer[4],
						    buffer_len - 4);
		if (ret_val < 0)
			return ret_val;

		if (cipso_v4_rbm_optfmt && ret_val > 0 && ret_val <= 10)
			tag_len = 14;
		else
			tag_len = 4 + ret_val;
	} else
		tag_len = 4;

	buffer[0] = CIPSO_V4_TAG_RBITMAP;
	buffer[1] = tag_len;
	buffer[3] = level;

	return tag_len;
}

static int cipso_v4_parsetag_rbm(const struct cipso_v4_doi *doi_def,
				 const unsigned char *tag,
				 struct netlbl_lsm_secattr *secattr)
{
	int ret_val;
	u8 tag_len = tag[1];
	u32 level;

	ret_val = cipso_v4_map_lvl_ntoh(doi_def, tag[3], &level);
	if (ret_val != 0)
		return ret_val;
	secattr->attr.mls.lvl = level;
	secattr->flags |= NETLBL_SECATTR_MLS_LVL;

	if (tag_len > 4) {
		secattr->attr.mls.cat =
		                       netlbl_secattr_catmap_alloc(GFP_ATOMIC);
		if (secattr->attr.mls.cat == NULL)
			return -ENOMEM;

		ret_val = cipso_v4_map_cat_rbm_ntoh(doi_def,
						    &tag[4],
						    tag_len - 4,
						    secattr);
		if (ret_val != 0) {
			netlbl_secattr_catmap_free(secattr->attr.mls.cat);
			return ret_val;
		}

		secattr->flags |= NETLBL_SECATTR_MLS_CAT;
	}

	return 0;
}

static int cipso_v4_gentag_enum(const struct cipso_v4_doi *doi_def,
				const struct netlbl_lsm_secattr *secattr,
				unsigned char *buffer,
				u32 buffer_len)
{
	int ret_val;
	u32 tag_len;
	u32 level;

	if (!(secattr->flags & NETLBL_SECATTR_MLS_LVL))
		return -EPERM;

	ret_val = cipso_v4_map_lvl_hton(doi_def,
					secattr->attr.mls.lvl,
					&level);
	if (ret_val != 0)
		return ret_val;

	if (secattr->flags & NETLBL_SECATTR_MLS_CAT) {
		ret_val = cipso_v4_map_cat_enum_hton(doi_def,
						     secattr,
						     &buffer[4],
						     buffer_len - 4);
		if (ret_val < 0)
			return ret_val;

		tag_len = 4 + ret_val;
	} else
		tag_len = 4;

	buffer[0] = CIPSO_V4_TAG_ENUM;
	buffer[1] = tag_len;
	buffer[3] = level;

	return tag_len;
}

static int cipso_v4_parsetag_enum(const struct cipso_v4_doi *doi_def,
				  const unsigned char *tag,
				  struct netlbl_lsm_secattr *secattr)
{
	int ret_val;
	u8 tag_len = tag[1];
	u32 level;

	ret_val = cipso_v4_map_lvl_ntoh(doi_def, tag[3], &level);
	if (ret_val != 0)
		return ret_val;
	secattr->attr.mls.lvl = level;
	secattr->flags |= NETLBL_SECATTR_MLS_LVL;

	if (tag_len > 4) {
		secattr->attr.mls.cat =
			               netlbl_secattr_catmap_alloc(GFP_ATOMIC);
		if (secattr->attr.mls.cat == NULL)
			return -ENOMEM;

		ret_val = cipso_v4_map_cat_enum_ntoh(doi_def,
						     &tag[4],
						     tag_len - 4,
						     secattr);
		if (ret_val != 0) {
			netlbl_secattr_catmap_free(secattr->attr.mls.cat);
			return ret_val;
		}

		secattr->flags |= NETLBL_SECATTR_MLS_CAT;
	}

	return 0;
}

static int cipso_v4_gentag_rng(const struct cipso_v4_doi *doi_def,
			       const struct netlbl_lsm_secattr *secattr,
			       unsigned char *buffer,
			       u32 buffer_len)
{
	int ret_val;
	u32 tag_len;
	u32 level;

	if (!(secattr->flags & NETLBL_SECATTR_MLS_LVL))
		return -EPERM;

	ret_val = cipso_v4_map_lvl_hton(doi_def,
					secattr->attr.mls.lvl,
					&level);
	if (ret_val != 0)
		return ret_val;

	if (secattr->flags & NETLBL_SECATTR_MLS_CAT) {
		ret_val = cipso_v4_map_cat_rng_hton(doi_def,
						    secattr,
						    &buffer[4],
						    buffer_len - 4);
		if (ret_val < 0)
			return ret_val;

		tag_len = 4 + ret_val;
	} else
		tag_len = 4;

	buffer[0] = CIPSO_V4_TAG_RANGE;
	buffer[1] = tag_len;
	buffer[3] = level;

	return tag_len;
}

static int cipso_v4_parsetag_rng(const struct cipso_v4_doi *doi_def,
				 const unsigned char *tag,
				 struct netlbl_lsm_secattr *secattr)
{
	int ret_val;
	u8 tag_len = tag[1];
	u32 level;

	ret_val = cipso_v4_map_lvl_ntoh(doi_def, tag[3], &level);
	if (ret_val != 0)
		return ret_val;
	secattr->attr.mls.lvl = level;
	secattr->flags |= NETLBL_SECATTR_MLS_LVL;

	if (tag_len > 4) {
		secattr->attr.mls.cat =
			               netlbl_secattr_catmap_alloc(GFP_ATOMIC);
		if (secattr->attr.mls.cat == NULL)
			return -ENOMEM;

		ret_val = cipso_v4_map_cat_rng_ntoh(doi_def,
						    &tag[4],
						    tag_len - 4,
						    secattr);
		if (ret_val != 0) {
			netlbl_secattr_catmap_free(secattr->attr.mls.cat);
			return ret_val;
		}

		secattr->flags |= NETLBL_SECATTR_MLS_CAT;
	}

	return 0;
}

static int cipso_v4_gentag_loc(const struct cipso_v4_doi *doi_def,
			       const struct netlbl_lsm_secattr *secattr,
			       unsigned char *buffer,
			       u32 buffer_len)
{
	if (!(secattr->flags & NETLBL_SECATTR_SECID))
		return -EPERM;

	buffer[0] = CIPSO_V4_TAG_LOCAL;
	buffer[1] = CIPSO_V4_TAG_LOC_BLEN;
	*(u32 *)&buffer[2] = secattr->attr.secid;

	return CIPSO_V4_TAG_LOC_BLEN;
}

static int cipso_v4_parsetag_loc(const struct cipso_v4_doi *doi_def,
				 const unsigned char *tag,
				 struct netlbl_lsm_secattr *secattr)
{
	secattr->attr.secid = *(u32 *)&tag[2];
	secattr->flags |= NETLBL_SECATTR_SECID;

	return 0;
}

int cipso_v4_validate(const struct sk_buff *skb, unsigned char **option)
{
	unsigned char *opt = *option;
	unsigned char *tag;
	unsigned char opt_iter;
	unsigned char err_offset = 0;
	u8 opt_len;
	u8 tag_len;
	struct cipso_v4_doi *doi_def = NULL;
	u32 tag_iter;

	
	opt_len = opt[1];
	if (opt_len < 8) {
		err_offset = 1;
		goto validate_return;
	}

	rcu_read_lock();
	doi_def = cipso_v4_doi_search(get_unaligned_be32(&opt[2]));
	if (doi_def == NULL) {
		err_offset = 2;
		goto validate_return_locked;
	}

	opt_iter = CIPSO_V4_HDR_LEN;
	tag = opt + opt_iter;
	while (opt_iter < opt_len) {
		for (tag_iter = 0; doi_def->tags[tag_iter] != tag[0];)
			if (doi_def->tags[tag_iter] == CIPSO_V4_TAG_INVALID ||
			    ++tag_iter == CIPSO_V4_TAG_MAXCNT) {
				err_offset = opt_iter;
				goto validate_return_locked;
			}

		tag_len = tag[1];
		if (tag_len > (opt_len - opt_iter)) {
			err_offset = opt_iter + 1;
			goto validate_return_locked;
		}

		switch (tag[0]) {
		case CIPSO_V4_TAG_RBITMAP:
			if (tag_len < CIPSO_V4_TAG_RBM_BLEN) {
				err_offset = opt_iter + 1;
				goto validate_return_locked;
			}

			if (cipso_v4_rbm_strictvalid) {
				if (cipso_v4_map_lvl_valid(doi_def,
							   tag[3]) < 0) {
					err_offset = opt_iter + 3;
					goto validate_return_locked;
				}
				if (tag_len > CIPSO_V4_TAG_RBM_BLEN &&
				    cipso_v4_map_cat_rbm_valid(doi_def,
							    &tag[4],
							    tag_len - 4) < 0) {
					err_offset = opt_iter + 4;
					goto validate_return_locked;
				}
			}
			break;
		case CIPSO_V4_TAG_ENUM:
			if (tag_len < CIPSO_V4_TAG_ENUM_BLEN) {
				err_offset = opt_iter + 1;
				goto validate_return_locked;
			}

			if (cipso_v4_map_lvl_valid(doi_def,
						   tag[3]) < 0) {
				err_offset = opt_iter + 3;
				goto validate_return_locked;
			}
			if (tag_len > CIPSO_V4_TAG_ENUM_BLEN &&
			    cipso_v4_map_cat_enum_valid(doi_def,
							&tag[4],
							tag_len - 4) < 0) {
				err_offset = opt_iter + 4;
				goto validate_return_locked;
			}
			break;
		case CIPSO_V4_TAG_RANGE:
			if (tag_len < CIPSO_V4_TAG_RNG_BLEN) {
				err_offset = opt_iter + 1;
				goto validate_return_locked;
			}

			if (cipso_v4_map_lvl_valid(doi_def,
						   tag[3]) < 0) {
				err_offset = opt_iter + 3;
				goto validate_return_locked;
			}
			if (tag_len > CIPSO_V4_TAG_RNG_BLEN &&
			    cipso_v4_map_cat_rng_valid(doi_def,
						       &tag[4],
						       tag_len - 4) < 0) {
				err_offset = opt_iter + 4;
				goto validate_return_locked;
			}
			break;
		case CIPSO_V4_TAG_LOCAL:
			if (!(skb->dev->flags & IFF_LOOPBACK)) {
				err_offset = opt_iter;
				goto validate_return_locked;
			}
			if (tag_len != CIPSO_V4_TAG_LOC_BLEN) {
				err_offset = opt_iter + 1;
				goto validate_return_locked;
			}
			break;
		default:
			err_offset = opt_iter;
			goto validate_return_locked;
		}

		tag += tag_len;
		opt_iter += tag_len;
	}

validate_return_locked:
	rcu_read_unlock();
validate_return:
	*option = opt + err_offset;
	return err_offset;
}

void cipso_v4_error(struct sk_buff *skb, int error, u32 gateway)
{
	if (ip_hdr(skb)->protocol == IPPROTO_ICMP || error != -EACCES)
		return;

	if (gateway)
		icmp_send(skb, ICMP_DEST_UNREACH, ICMP_NET_ANO, 0);
	else
		icmp_send(skb, ICMP_DEST_UNREACH, ICMP_HOST_ANO, 0);
}

static int cipso_v4_genopt(unsigned char *buf, u32 buf_len,
			   const struct cipso_v4_doi *doi_def,
			   const struct netlbl_lsm_secattr *secattr)
{
	int ret_val;
	u32 iter;

	if (buf_len <= CIPSO_V4_HDR_LEN)
		return -ENOSPC;

	iter = 0;
	do {
		memset(buf, 0, buf_len);
		switch (doi_def->tags[iter]) {
		case CIPSO_V4_TAG_RBITMAP:
			ret_val = cipso_v4_gentag_rbm(doi_def,
						   secattr,
						   &buf[CIPSO_V4_HDR_LEN],
						   buf_len - CIPSO_V4_HDR_LEN);
			break;
		case CIPSO_V4_TAG_ENUM:
			ret_val = cipso_v4_gentag_enum(doi_def,
						   secattr,
						   &buf[CIPSO_V4_HDR_LEN],
						   buf_len - CIPSO_V4_HDR_LEN);
			break;
		case CIPSO_V4_TAG_RANGE:
			ret_val = cipso_v4_gentag_rng(doi_def,
						   secattr,
						   &buf[CIPSO_V4_HDR_LEN],
						   buf_len - CIPSO_V4_HDR_LEN);
			break;
		case CIPSO_V4_TAG_LOCAL:
			ret_val = cipso_v4_gentag_loc(doi_def,
						   secattr,
						   &buf[CIPSO_V4_HDR_LEN],
						   buf_len - CIPSO_V4_HDR_LEN);
			break;
		default:
			return -EPERM;
		}

		iter++;
	} while (ret_val < 0 &&
		 iter < CIPSO_V4_TAG_MAXCNT &&
		 doi_def->tags[iter] != CIPSO_V4_TAG_INVALID);
	if (ret_val < 0)
		return ret_val;
	cipso_v4_gentag_hdr(doi_def, buf, ret_val);
	return CIPSO_V4_HDR_LEN + ret_val;
}

int cipso_v4_sock_setattr(struct sock *sk,
			  const struct cipso_v4_doi *doi_def,
			  const struct netlbl_lsm_secattr *secattr)
{
	int ret_val = -EPERM;
	unsigned char *buf = NULL;
	u32 buf_len;
	u32 opt_len;
	struct ip_options_rcu *old, *opt = NULL;
	struct inet_sock *sk_inet;
	struct inet_connection_sock *sk_conn;

	if (sk == NULL)
		return 0;

	buf_len = CIPSO_V4_OPT_LEN_MAX;
	buf = kmalloc(buf_len, GFP_ATOMIC);
	if (buf == NULL) {
		ret_val = -ENOMEM;
		goto socket_setattr_failure;
	}

	ret_val = cipso_v4_genopt(buf, buf_len, doi_def, secattr);
	if (ret_val < 0)
		goto socket_setattr_failure;
	buf_len = ret_val;

	opt_len = (buf_len + 3) & ~3;
	opt = kzalloc(sizeof(*opt) + opt_len, GFP_ATOMIC);
	if (opt == NULL) {
		ret_val = -ENOMEM;
		goto socket_setattr_failure;
	}
	memcpy(opt->opt.__data, buf, buf_len);
	opt->opt.optlen = opt_len;
	opt->opt.cipso = sizeof(struct iphdr);
	kfree(buf);
	buf = NULL;

	sk_inet = inet_sk(sk);

	old = rcu_dereference_protected(sk_inet->inet_opt, sock_owned_by_user(sk));
	if (sk_inet->is_icsk) {
		sk_conn = inet_csk(sk);
		if (old)
			sk_conn->icsk_ext_hdr_len -= old->opt.optlen;
		sk_conn->icsk_ext_hdr_len += opt->opt.optlen;
		sk_conn->icsk_sync_mss(sk, sk_conn->icsk_pmtu_cookie);
	}
	rcu_assign_pointer(sk_inet->inet_opt, opt);
	if (old)
		kfree_rcu(old, rcu);

	return 0;

socket_setattr_failure:
	kfree(buf);
	kfree(opt);
	return ret_val;
}

int cipso_v4_req_setattr(struct request_sock *req,
			 const struct cipso_v4_doi *doi_def,
			 const struct netlbl_lsm_secattr *secattr)
{
	int ret_val = -EPERM;
	unsigned char *buf = NULL;
	u32 buf_len;
	u32 opt_len;
	struct ip_options_rcu *opt = NULL;
	struct inet_request_sock *req_inet;

	buf_len = CIPSO_V4_OPT_LEN_MAX;
	buf = kmalloc(buf_len, GFP_ATOMIC);
	if (buf == NULL) {
		ret_val = -ENOMEM;
		goto req_setattr_failure;
	}

	ret_val = cipso_v4_genopt(buf, buf_len, doi_def, secattr);
	if (ret_val < 0)
		goto req_setattr_failure;
	buf_len = ret_val;

	opt_len = (buf_len + 3) & ~3;
	opt = kzalloc(sizeof(*opt) + opt_len, GFP_ATOMIC);
	if (opt == NULL) {
		ret_val = -ENOMEM;
		goto req_setattr_failure;
	}
	memcpy(opt->opt.__data, buf, buf_len);
	opt->opt.optlen = opt_len;
	opt->opt.cipso = sizeof(struct iphdr);
	kfree(buf);
	buf = NULL;

	req_inet = inet_rsk(req);
	opt = xchg(&req_inet->opt, opt);
	if (opt)
		kfree_rcu(opt, rcu);

	return 0;

req_setattr_failure:
	kfree(buf);
	kfree(opt);
	return ret_val;
}

static int cipso_v4_delopt(struct ip_options_rcu **opt_ptr)
{
	int hdr_delta = 0;
	struct ip_options_rcu *opt = *opt_ptr;

	if (opt->opt.srr || opt->opt.rr || opt->opt.ts || opt->opt.router_alert) {
		u8 cipso_len;
		u8 cipso_off;
		unsigned char *cipso_ptr;
		int iter;
		int optlen_new;

		cipso_off = opt->opt.cipso - sizeof(struct iphdr);
		cipso_ptr = &opt->opt.__data[cipso_off];
		cipso_len = cipso_ptr[1];

		if (opt->opt.srr > opt->opt.cipso)
			opt->opt.srr -= cipso_len;
		if (opt->opt.rr > opt->opt.cipso)
			opt->opt.rr -= cipso_len;
		if (opt->opt.ts > opt->opt.cipso)
			opt->opt.ts -= cipso_len;
		if (opt->opt.router_alert > opt->opt.cipso)
			opt->opt.router_alert -= cipso_len;
		opt->opt.cipso = 0;

		memmove(cipso_ptr, cipso_ptr + cipso_len,
			opt->opt.optlen - cipso_off - cipso_len);

		iter = 0;
		optlen_new = 0;
		while (iter < opt->opt.optlen)
			if (opt->opt.__data[iter] != IPOPT_NOP) {
				iter += opt->opt.__data[iter + 1];
				optlen_new = iter;
			} else
				iter++;
		hdr_delta = opt->opt.optlen;
		opt->opt.optlen = (optlen_new + 3) & ~3;
		hdr_delta -= opt->opt.optlen;
	} else {
		*opt_ptr = NULL;
		hdr_delta = opt->opt.optlen;
		kfree_rcu(opt, rcu);
	}

	return hdr_delta;
}

void cipso_v4_sock_delattr(struct sock *sk)
{
	int hdr_delta;
	struct ip_options_rcu *opt;
	struct inet_sock *sk_inet;

	sk_inet = inet_sk(sk);
	opt = rcu_dereference_protected(sk_inet->inet_opt, 1);
	if (opt == NULL || opt->opt.cipso == 0)
		return;

	hdr_delta = cipso_v4_delopt(&sk_inet->inet_opt);
	if (sk_inet->is_icsk && hdr_delta > 0) {
		struct inet_connection_sock *sk_conn = inet_csk(sk);
		sk_conn->icsk_ext_hdr_len -= hdr_delta;
		sk_conn->icsk_sync_mss(sk, sk_conn->icsk_pmtu_cookie);
	}
}

void cipso_v4_req_delattr(struct request_sock *req)
{
	struct ip_options_rcu *opt;
	struct inet_request_sock *req_inet;

	req_inet = inet_rsk(req);
	opt = req_inet->opt;
	if (opt == NULL || opt->opt.cipso == 0)
		return;

	cipso_v4_delopt(&req_inet->opt);
}

static int cipso_v4_getattr(const unsigned char *cipso,
			    struct netlbl_lsm_secattr *secattr)
{
	int ret_val = -ENOMSG;
	u32 doi;
	struct cipso_v4_doi *doi_def;

	if (cipso_v4_cache_check(cipso, cipso[1], secattr) == 0)
		return 0;

	doi = get_unaligned_be32(&cipso[2]);
	rcu_read_lock();
	doi_def = cipso_v4_doi_search(doi);
	if (doi_def == NULL)
		goto getattr_return;
	switch (cipso[6]) {
	case CIPSO_V4_TAG_RBITMAP:
		ret_val = cipso_v4_parsetag_rbm(doi_def, &cipso[6], secattr);
		break;
	case CIPSO_V4_TAG_ENUM:
		ret_val = cipso_v4_parsetag_enum(doi_def, &cipso[6], secattr);
		break;
	case CIPSO_V4_TAG_RANGE:
		ret_val = cipso_v4_parsetag_rng(doi_def, &cipso[6], secattr);
		break;
	case CIPSO_V4_TAG_LOCAL:
		ret_val = cipso_v4_parsetag_loc(doi_def, &cipso[6], secattr);
		break;
	}
	if (ret_val == 0)
		secattr->type = NETLBL_NLTYPE_CIPSOV4;

getattr_return:
	rcu_read_unlock();
	return ret_val;
}

int cipso_v4_sock_getattr(struct sock *sk, struct netlbl_lsm_secattr *secattr)
{
	struct ip_options_rcu *opt;
	int res = -ENOMSG;

	rcu_read_lock();
	opt = rcu_dereference(inet_sk(sk)->inet_opt);
	if (opt && opt->opt.cipso)
		res = cipso_v4_getattr(opt->opt.__data +
						opt->opt.cipso -
						sizeof(struct iphdr),
				       secattr);
	rcu_read_unlock();
	return res;
}

int cipso_v4_skbuff_setattr(struct sk_buff *skb,
			    const struct cipso_v4_doi *doi_def,
			    const struct netlbl_lsm_secattr *secattr)
{
	int ret_val;
	struct iphdr *iph;
	struct ip_options *opt = &IPCB(skb)->opt;
	unsigned char buf[CIPSO_V4_OPT_LEN_MAX];
	u32 buf_len = CIPSO_V4_OPT_LEN_MAX;
	u32 opt_len;
	int len_delta;

	ret_val = cipso_v4_genopt(buf, buf_len, doi_def, secattr);
	if (ret_val < 0)
		return ret_val;
	buf_len = ret_val;
	opt_len = (buf_len + 3) & ~3;


	len_delta = opt_len - opt->optlen;
	ret_val = skb_cow(skb, skb_headroom(skb) + len_delta);
	if (ret_val < 0)
		return ret_val;

	if (len_delta > 0) {
		iph = ip_hdr(skb);
		skb_push(skb, len_delta);
		memmove((char *)iph - len_delta, iph, iph->ihl << 2);
		skb_reset_network_header(skb);
		iph = ip_hdr(skb);
	} else if (len_delta < 0) {
		iph = ip_hdr(skb);
		memset(iph + 1, IPOPT_NOP, opt->optlen);
	} else
		iph = ip_hdr(skb);

	if (opt->optlen > 0)
		memset(opt, 0, sizeof(*opt));
	opt->optlen = opt_len;
	opt->cipso = sizeof(struct iphdr);
	opt->is_changed = 1;

	memcpy(iph + 1, buf, buf_len);
	if (opt_len > buf_len)
		memset((char *)(iph + 1) + buf_len, 0, opt_len - buf_len);
	if (len_delta != 0) {
		iph->ihl = 5 + (opt_len >> 2);
		iph->tot_len = htons(skb->len);
	}
	ip_send_check(iph);

	return 0;
}

int cipso_v4_skbuff_delattr(struct sk_buff *skb)
{
	int ret_val;
	struct iphdr *iph;
	struct ip_options *opt = &IPCB(skb)->opt;
	unsigned char *cipso_ptr;

	if (opt->cipso == 0)
		return 0;

	
	ret_val = skb_cow(skb, skb_headroom(skb));
	if (ret_val < 0)
		return ret_val;


	iph = ip_hdr(skb);
	cipso_ptr = (unsigned char *)iph + opt->cipso;
	memset(cipso_ptr, IPOPT_NOOP, cipso_ptr[1]);
	opt->cipso = 0;
	opt->is_changed = 1;

	ip_send_check(iph);

	return 0;
}

int cipso_v4_skbuff_getattr(const struct sk_buff *skb,
			    struct netlbl_lsm_secattr *secattr)
{
	return cipso_v4_getattr(CIPSO_V4_OPTPTR(skb), secattr);
}


static int __init cipso_v4_init(void)
{
	int ret_val;

	ret_val = cipso_v4_cache_init();
	if (ret_val != 0)
		panic("Failed to initialize the CIPSO/IPv4 cache (%d)\n",
		      ret_val);

	return 0;
}

subsys_initcall(cipso_v4_init);
