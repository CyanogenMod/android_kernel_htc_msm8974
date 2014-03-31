/*
 * Copyright 2000 by Hans Reiser, licensing governed by reiserfs/README
 */

#include <linux/string.h>
#include <linux/random.h>
#include <linux/time.h>
#include "reiserfs.h"

#define objectid_map(s,rs) (old_format_only (s) ? \
                         (__le32 *)((struct reiserfs_super_block_v1 *)(rs) + 1) :\
			 (__le32 *)((rs) + 1))

#ifdef CONFIG_REISERFS_CHECK

static void check_objectid_map(struct super_block *s, __le32 * map)
{
	if (le32_to_cpu(map[0]) != 1)
		reiserfs_panic(s, "vs-15010", "map corrupted: %lx",
			       (long unsigned int)le32_to_cpu(map[0]));

	
}

#else
static void check_objectid_map(struct super_block *s, __le32 * map)
{;
}
#endif


__u32 reiserfs_get_unused_objectid(struct reiserfs_transaction_handle *th)
{
	struct super_block *s = th->t_super;
	struct reiserfs_super_block *rs = SB_DISK_SUPER_BLOCK(s);
	__le32 *map = objectid_map(s, rs);
	__u32 unused_objectid;

	BUG_ON(!th->t_trans_id);

	check_objectid_map(s, map);

	reiserfs_prepare_for_journal(s, SB_BUFFER_WITH_SB(s), 1);
	
	unused_objectid = le32_to_cpu(map[1]);
	if (unused_objectid == U32_MAX) {
		reiserfs_warning(s, "reiserfs-15100", "no more object ids");
		reiserfs_restore_prepared_buffer(s, SB_BUFFER_WITH_SB(s));
		return 0;
	}

	map[1] = cpu_to_le32(unused_objectid + 1);

	if (sb_oid_cursize(rs) > 2 && map[1] == map[2]) {
		memmove(map + 1, map + 3,
			(sb_oid_cursize(rs) - 3) * sizeof(__u32));
		set_sb_oid_cursize(rs, sb_oid_cursize(rs) - 2);
	}

	journal_mark_dirty(th, s, SB_BUFFER_WITH_SB(s));
	return unused_objectid;
}

void reiserfs_release_objectid(struct reiserfs_transaction_handle *th,
			       __u32 objectid_to_release)
{
	struct super_block *s = th->t_super;
	struct reiserfs_super_block *rs = SB_DISK_SUPER_BLOCK(s);
	__le32 *map = objectid_map(s, rs);
	int i = 0;

	BUG_ON(!th->t_trans_id);
	
	check_objectid_map(s, map);

	reiserfs_prepare_for_journal(s, SB_BUFFER_WITH_SB(s), 1);
	journal_mark_dirty(th, s, SB_BUFFER_WITH_SB(s));

	while (i < sb_oid_cursize(rs)) {
		if (objectid_to_release == le32_to_cpu(map[i])) {
			
			
			le32_add_cpu(&map[i], 1);

			
			if (map[i] == map[i + 1]) {
				
				memmove(map + i, map + i + 2,
					(sb_oid_cursize(rs) - i -
					 2) * sizeof(__u32));
				
				set_sb_oid_cursize(rs, sb_oid_cursize(rs) - 2);

				RFALSE(sb_oid_cursize(rs) < 2 ||
				       sb_oid_cursize(rs) > sb_oid_maxsize(rs),
				       "vs-15005: objectid map corrupted cur_size == %d (max == %d)",
				       sb_oid_cursize(rs), sb_oid_maxsize(rs));
			}
			return;
		}

		if (objectid_to_release > le32_to_cpu(map[i]) &&
		    objectid_to_release < le32_to_cpu(map[i + 1])) {
			
			if (objectid_to_release + 1 == le32_to_cpu(map[i + 1])) {
				
				le32_add_cpu(&map[i + 1], -1);
				return;
			}

			
			if (sb_oid_cursize(rs) == sb_oid_maxsize(rs)) {
				
				PROC_INFO_INC(s, leaked_oid);
				return;
			}

			
			memmove(map + i + 3, map + i + 1,
				(sb_oid_cursize(rs) - i - 1) * sizeof(__u32));
			map[i + 1] = cpu_to_le32(objectid_to_release);
			map[i + 2] = cpu_to_le32(objectid_to_release + 1);
			set_sb_oid_cursize(rs, sb_oid_cursize(rs) + 2);
			return;
		}
		i += 2;
	}

	reiserfs_error(s, "vs-15011", "tried to free free object id (%lu)",
		       (long unsigned)objectid_to_release);
}

int reiserfs_convert_objectid_map_v1(struct super_block *s)
{
	struct reiserfs_super_block *disk_sb = SB_DISK_SUPER_BLOCK(s);
	int cur_size = sb_oid_cursize(disk_sb);
	int new_size = (s->s_blocksize - SB_SIZE) / sizeof(__u32) / 2 * 2;
	int old_max = sb_oid_maxsize(disk_sb);
	struct reiserfs_super_block_v1 *disk_sb_v1;
	__le32 *objectid_map, *new_objectid_map;
	int i;

	disk_sb_v1 =
	    (struct reiserfs_super_block_v1 *)(SB_BUFFER_WITH_SB(s)->b_data);
	objectid_map = (__le32 *) (disk_sb_v1 + 1);
	new_objectid_map = (__le32 *) (disk_sb + 1);

	if (cur_size > new_size) {
		objectid_map[new_size - 1] = objectid_map[cur_size - 1];
		set_sb_oid_cursize(disk_sb, new_size);
	}
	
	for (i = new_size - 1; i >= 0; i--) {
		objectid_map[i + (old_max - new_size)] = objectid_map[i];
	}

	
	set_sb_oid_maxsize(disk_sb, new_size);

	
	memset(disk_sb->s_label, 0, sizeof(disk_sb->s_label));
	generate_random_uuid(disk_sb->s_uuid);

	
	memset(disk_sb->s_unused, 0, sizeof(disk_sb->s_unused));
	return 0;
}
