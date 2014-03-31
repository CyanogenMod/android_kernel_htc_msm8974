
#include "isofs.h"

static struct dentry *
isofs_export_iget(struct super_block *sb,
		  unsigned long block,
		  unsigned long offset,
		  __u32 generation)
{
	struct inode *inode;

	if (block == 0)
		return ERR_PTR(-ESTALE);
	inode = isofs_iget(sb, block, offset);
	if (IS_ERR(inode))
		return ERR_CAST(inode);
	if (generation && inode->i_generation != generation) {
		iput(inode);
		return ERR_PTR(-ESTALE);
	}
	return d_obtain_alias(inode);
}

static struct dentry *isofs_export_get_parent(struct dentry *child)
{
	unsigned long parent_block = 0;
	unsigned long parent_offset = 0;
	struct inode *child_inode = child->d_inode;
	struct iso_inode_info *e_child_inode = ISOFS_I(child_inode);
	struct iso_directory_record *de = NULL;
	struct buffer_head * bh = NULL;
	struct dentry *rv = NULL;

	
	if (!S_ISDIR(child_inode->i_mode)) {
		printk(KERN_ERR "isofs: isofs_export_get_parent(): "
		       "child is not a directory!\n");
		rv = ERR_PTR(-EACCES);
		goto out;
	}

	if (e_child_inode->i_iget5_offset != 0) {
		printk(KERN_ERR "isofs: isofs_export_get_parent(): "
		       "child directory not normalized!\n");
		rv = ERR_PTR(-EACCES);
		goto out;
	}

	parent_block = e_child_inode->i_iget5_block;

	
	bh = sb_bread(child_inode->i_sb, parent_block);
	if (bh == NULL) {
		rv = ERR_PTR(-EACCES);
		goto out;
	}

	
	de = (struct iso_directory_record*)bh->b_data;

	
	parent_offset = (unsigned long)isonum_711(de->length);
	de = (struct iso_directory_record*)(bh->b_data + parent_offset);

	
	if ((isonum_711(de->name_len) != 1) || (de->name[0] != 1)) {
		printk(KERN_ERR "isofs: Unable to find the \"..\" "
		       "directory for NFS.\n");
		rv = ERR_PTR(-EACCES);
		goto out;
	}

	
	isofs_normalize_block_and_offset(de, &parent_block, &parent_offset);

	rv = d_obtain_alias(isofs_iget(child_inode->i_sb, parent_block,
				     parent_offset));
 out:
	if (bh)
		brelse(bh);
	return rv;
}

static int
isofs_export_encode_fh(struct dentry *dentry,
		       __u32 *fh32,
		       int *max_len,
		       int connectable)
{
	struct inode * inode = dentry->d_inode;
	struct iso_inode_info * ei = ISOFS_I(inode);
	int len = *max_len;
	int type = 1;
	__u16 *fh16 = (__u16*)fh32;

	if (connectable && (len < 5)) {
		*max_len = 5;
		return 255;
	} else if (len < 3) {
		*max_len = 3;
		return 255;
	}

	len = 3;
	fh32[0] = ei->i_iget5_block;
 	fh16[2] = (__u16)ei->i_iget5_offset;  
	fh32[2] = inode->i_generation;
	if (connectable && !S_ISDIR(inode->i_mode)) {
		struct inode *parent;
		struct iso_inode_info *eparent;
		spin_lock(&dentry->d_lock);
		parent = dentry->d_parent->d_inode;
		eparent = ISOFS_I(parent);
		fh32[3] = eparent->i_iget5_block;
		fh16[3] = (__u16)eparent->i_iget5_offset;  
		fh32[4] = parent->i_generation;
		spin_unlock(&dentry->d_lock);
		len = 5;
		type = 2;
	}
	*max_len = len;
	return type;
}

struct isofs_fid {
	u32 block;
	u16 offset;
	u16 parent_offset;
	u32 generation;
	u32 parent_block;
	u32 parent_generation;
};

static struct dentry *isofs_fh_to_dentry(struct super_block *sb,
	struct fid *fid, int fh_len, int fh_type)
{
	struct isofs_fid *ifid = (struct isofs_fid *)fid;

	if (fh_len < 3 || fh_type > 2)
		return NULL;

	return isofs_export_iget(sb, ifid->block, ifid->offset,
			ifid->generation);
}

static struct dentry *isofs_fh_to_parent(struct super_block *sb,
		struct fid *fid, int fh_len, int fh_type)
{
	struct isofs_fid *ifid = (struct isofs_fid *)fid;

	if (fh_type != 2)
		return NULL;

	return isofs_export_iget(sb,
			fh_len > 2 ? ifid->parent_block : 0,
			ifid->parent_offset,
			fh_len > 4 ? ifid->parent_generation : 0);
}

const struct export_operations isofs_export_ops = {
	.encode_fh	= isofs_export_encode_fh,
	.fh_to_dentry	= isofs_fh_to_dentry,
	.fh_to_parent	= isofs_fh_to_parent,
	.get_parent     = isofs_export_get_parent,
};
