
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

#include <linux/coda.h>
#include <linux/coda_psdev.h>
#include "coda_linux.h"
#include "coda_cache.h"

static atomic_t permission_epoch = ATOMIC_INIT(0);

void coda_cache_enter(struct inode *inode, int mask)
{
	struct coda_inode_info *cii = ITOC(inode);

	spin_lock(&cii->c_lock);
	cii->c_cached_epoch = atomic_read(&permission_epoch);
	if (cii->c_uid != current_fsuid()) {
		cii->c_uid = current_fsuid();
                cii->c_cached_perm = mask;
        } else
                cii->c_cached_perm |= mask;
	spin_unlock(&cii->c_lock);
}

void coda_cache_clear_inode(struct inode *inode)
{
	struct coda_inode_info *cii = ITOC(inode);
	spin_lock(&cii->c_lock);
	cii->c_cached_epoch = atomic_read(&permission_epoch) - 1;
	spin_unlock(&cii->c_lock);
}

void coda_cache_clear_all(struct super_block *sb)
{
	atomic_inc(&permission_epoch);
}


int coda_cache_check(struct inode *inode, int mask)
{
	struct coda_inode_info *cii = ITOC(inode);
	int hit;
	
	spin_lock(&cii->c_lock);
	hit = (mask & cii->c_cached_perm) == mask &&
	    cii->c_uid == current_fsuid() &&
	    cii->c_cached_epoch == atomic_read(&permission_epoch);
	spin_unlock(&cii->c_lock);

	return hit;
}



static void coda_flag_children(struct dentry *parent, int flag)
{
	struct list_head *child;
	struct dentry *de;

	spin_lock(&parent->d_lock);
	list_for_each(child, &parent->d_subdirs)
	{
		de = list_entry(child, struct dentry, d_u.d_child);
		
		if ( ! de->d_inode ) 
			continue;
		coda_flag_inode(de->d_inode, flag);
	}
	spin_unlock(&parent->d_lock);
	return; 
}

void coda_flag_inode_children(struct inode *inode, int flag)
{
	struct dentry *alias_de;

	if ( !inode || !S_ISDIR(inode->i_mode)) 
		return; 

	alias_de = d_find_alias(inode);
	if (!alias_de)
		return;
	coda_flag_children(alias_de, flag);
	shrink_dcache_parent(alias_de);
	dput(alias_de);
}

