#include "reiserfs.h"
#include <linux/mutex.h>

void reiserfs_write_lock(struct super_block *s)
{
	struct reiserfs_sb_info *sb_i = REISERFS_SB(s);

	if (sb_i->lock_owner != current) {
		mutex_lock(&sb_i->lock);
		sb_i->lock_owner = current;
	}

	
	sb_i->lock_depth++;
}

void reiserfs_write_unlock(struct super_block *s)
{
	struct reiserfs_sb_info *sb_i = REISERFS_SB(s);

	BUG_ON(sb_i->lock_owner != current);

	if (--sb_i->lock_depth == -1) {
		sb_i->lock_owner = NULL;
		mutex_unlock(&sb_i->lock);
	}
}

int reiserfs_write_lock_once(struct super_block *s)
{
	struct reiserfs_sb_info *sb_i = REISERFS_SB(s);

	if (sb_i->lock_owner != current) {
		mutex_lock(&sb_i->lock);
		sb_i->lock_owner = current;
		return sb_i->lock_depth++;
	}

	return sb_i->lock_depth;
}

void reiserfs_write_unlock_once(struct super_block *s, int lock_depth)
{
	if (lock_depth == -1)
		reiserfs_write_unlock(s);
}

void reiserfs_check_lock_depth(struct super_block *sb, char *caller)
{
	struct reiserfs_sb_info *sb_i = REISERFS_SB(sb);

	if (sb_i->lock_depth < 0)
		reiserfs_panic(sb, "%s called without kernel lock held %d",
			       caller);
}

#ifdef CONFIG_REISERFS_CHECK
void reiserfs_lock_check_recursive(struct super_block *sb)
{
	struct reiserfs_sb_info *sb_i = REISERFS_SB(sb);

	WARN_ONCE((sb_i->lock_depth > 0), "Unwanted recursive reiserfs lock!\n");
}
#endif
