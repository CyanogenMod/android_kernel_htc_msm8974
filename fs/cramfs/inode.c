/*
 * Compressed rom filesystem for Linux.
 *
 * Copyright (C) 1999 Linus Torvalds.
 *
 * This file is released under the GPL.
 */


#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/blkdev.h>
#include <linux/cramfs_fs.h>
#include <linux/slab.h>
#include <linux/cramfs_fs_sb.h>
#include <linux/vfs.h>
#include <linux/mutex.h>

#include <asm/uaccess.h>

static const struct super_operations cramfs_ops;
static const struct inode_operations cramfs_dir_inode_operations;
static const struct file_operations cramfs_directory_operations;
static const struct address_space_operations cramfs_aops;

static DEFINE_MUTEX(read_mutex);


#define OFFSET(x)	((x)->i_ino)

static unsigned long cramino(const struct cramfs_inode *cino, unsigned int offset)
{
	if (!cino->offset)
		return offset + 1;
	if (!cino->size)
		return offset + 1;

	switch (cino->mode & S_IFMT) {
	case S_IFREG:
	case S_IFDIR:
	case S_IFLNK:
		return cino->offset << 2;
	default:
		break;
	}
	return offset + 1;
}

static struct inode *get_cramfs_inode(struct super_block *sb,
	const struct cramfs_inode *cramfs_inode, unsigned int offset)
{
	struct inode *inode;
	static struct timespec zerotime;

	inode = iget_locked(sb, cramino(cramfs_inode, offset));
	if (!inode)
		return ERR_PTR(-ENOMEM);
	if (!(inode->i_state & I_NEW))
		return inode;

	switch (cramfs_inode->mode & S_IFMT) {
	case S_IFREG:
		inode->i_fop = &generic_ro_fops;
		inode->i_data.a_ops = &cramfs_aops;
		break;
	case S_IFDIR:
		inode->i_op = &cramfs_dir_inode_operations;
		inode->i_fop = &cramfs_directory_operations;
		break;
	case S_IFLNK:
		inode->i_op = &page_symlink_inode_operations;
		inode->i_data.a_ops = &cramfs_aops;
		break;
	default:
		init_special_inode(inode, cramfs_inode->mode,
				old_decode_dev(cramfs_inode->size));
	}

	inode->i_mode = cramfs_inode->mode;
	inode->i_uid = cramfs_inode->uid;
	inode->i_gid = cramfs_inode->gid;

	
	if (!(inode->i_ino & 3)) {
		inode->i_size = cramfs_inode->size;
		inode->i_blocks = (cramfs_inode->size - 1) / 512 + 1;
	}

	
	inode->i_mtime = inode->i_atime = inode->i_ctime = zerotime;

	unlock_new_inode(inode);

	return inode;
}

#define READ_BUFFERS (2)
#define NEXT_BUFFER(_ix) ((_ix) ^ 1)

#define BLKS_PER_BUF_SHIFT	(2)
#define BLKS_PER_BUF		(1 << BLKS_PER_BUF_SHIFT)
#define BUFFER_SIZE		(BLKS_PER_BUF*PAGE_CACHE_SIZE)

static unsigned char read_buffers[READ_BUFFERS][BUFFER_SIZE];
static unsigned buffer_blocknr[READ_BUFFERS];
static struct super_block * buffer_dev[READ_BUFFERS];
static int next_buffer;

static void *cramfs_read(struct super_block *sb, unsigned int offset, unsigned int len)
{
	struct address_space *mapping = sb->s_bdev->bd_inode->i_mapping;
	struct page *pages[BLKS_PER_BUF];
	unsigned i, blocknr, buffer;
	unsigned long devsize;
	char *data;

	if (!len)
		return NULL;
	blocknr = offset >> PAGE_CACHE_SHIFT;
	offset &= PAGE_CACHE_SIZE - 1;

	
	for (i = 0; i < READ_BUFFERS; i++) {
		unsigned int blk_offset;

		if (buffer_dev[i] != sb)
			continue;
		if (blocknr < buffer_blocknr[i])
			continue;
		blk_offset = (blocknr - buffer_blocknr[i]) << PAGE_CACHE_SHIFT;
		blk_offset += offset;
		if (blk_offset + len > BUFFER_SIZE)
			continue;
		return read_buffers[i] + blk_offset;
	}

	devsize = mapping->host->i_size >> PAGE_CACHE_SHIFT;

	
	for (i = 0; i < BLKS_PER_BUF; i++) {
		struct page *page = NULL;

		if (blocknr + i < devsize) {
			page = read_mapping_page_async(mapping, blocknr + i,
									NULL);
			
			if (IS_ERR(page))
				page = NULL;
		}
		pages[i] = page;
	}

	for (i = 0; i < BLKS_PER_BUF; i++) {
		struct page *page = pages[i];
		if (page) {
			wait_on_page_locked(page);
			if (!PageUptodate(page)) {
				
				page_cache_release(page);
				pages[i] = NULL;
			}
		}
	}

	buffer = next_buffer;
	next_buffer = NEXT_BUFFER(buffer);
	buffer_blocknr[buffer] = blocknr;
	buffer_dev[buffer] = sb;

	data = read_buffers[buffer];
	for (i = 0; i < BLKS_PER_BUF; i++) {
		struct page *page = pages[i];
		if (page) {
			memcpy(data, kmap(page), PAGE_CACHE_SIZE);
			kunmap(page);
			page_cache_release(page);
		} else
			memset(data, 0, PAGE_CACHE_SIZE);
		data += PAGE_CACHE_SIZE;
	}
	return read_buffers[buffer] + offset;
}

static void cramfs_put_super(struct super_block *sb)
{
	kfree(sb->s_fs_info);
	sb->s_fs_info = NULL;
}

static int cramfs_remount(struct super_block *sb, int *flags, char *data)
{
	*flags |= MS_RDONLY;
	return 0;
}

static int cramfs_fill_super(struct super_block *sb, void *data, int silent)
{
	int i;
	struct cramfs_super super;
	unsigned long root_offset;
	struct cramfs_sb_info *sbi;
	struct inode *root;

	sb->s_flags |= MS_RDONLY;

	sbi = kzalloc(sizeof(struct cramfs_sb_info), GFP_KERNEL);
	if (!sbi)
		return -ENOMEM;
	sb->s_fs_info = sbi;

	
	mutex_lock(&read_mutex);
	for (i = 0; i < READ_BUFFERS; i++)
		buffer_blocknr[i] = -1;

	
	memcpy(&super, cramfs_read(sb, 0, sizeof(super)), sizeof(super));
	mutex_unlock(&read_mutex);

	
	if (super.magic != CRAMFS_MAGIC) {
		
		if (super.magic == CRAMFS_MAGIC_WEND) {
			if (!silent)
				printk(KERN_ERR "cramfs: wrong endianness\n");
			goto out;
		}

		
		mutex_lock(&read_mutex);
		memcpy(&super, cramfs_read(sb, 512, sizeof(super)), sizeof(super));
		mutex_unlock(&read_mutex);
		if (super.magic != CRAMFS_MAGIC) {
			if (super.magic == CRAMFS_MAGIC_WEND && !silent)
				printk(KERN_ERR "cramfs: wrong endianness\n");
			else if (!silent)
				printk(KERN_ERR "cramfs: wrong magic\n");
			goto out;
		}
	}

	
	if (super.flags & ~CRAMFS_SUPPORTED_FLAGS) {
		printk(KERN_ERR "cramfs: unsupported filesystem features\n");
		goto out;
	}

	
	if (!S_ISDIR(super.root.mode)) {
		printk(KERN_ERR "cramfs: root is not a directory\n");
		goto out;
	}
	
	super.root.mode |= (S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);

	root_offset = super.root.offset << 2;
	if (super.flags & CRAMFS_FLAG_FSID_VERSION_2) {
		sbi->size=super.size;
		sbi->blocks=super.fsid.blocks;
		sbi->files=super.fsid.files;
	} else {
		sbi->size=1<<28;
		sbi->blocks=0;
		sbi->files=0;
	}
	sbi->magic=super.magic;
	sbi->flags=super.flags;
	if (root_offset == 0)
		printk(KERN_INFO "cramfs: empty filesystem");
	else if (!(super.flags & CRAMFS_FLAG_SHIFTED_ROOT_OFFSET) &&
		 ((root_offset != sizeof(struct cramfs_super)) &&
		  (root_offset != 512 + sizeof(struct cramfs_super))))
	{
		printk(KERN_ERR "cramfs: bad root offset %lu\n", root_offset);
		goto out;
	}

	
	sb->s_op = &cramfs_ops;
	root = get_cramfs_inode(sb, &super.root, 0);
	if (IS_ERR(root))
		goto out;
	sb->s_root = d_make_root(root);
	if (!sb->s_root)
		goto out;
	return 0;
out:
	kfree(sbi);
	sb->s_fs_info = NULL;
	return -EINVAL;
}

static int cramfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	struct super_block *sb = dentry->d_sb;
	u64 id = huge_encode_dev(sb->s_bdev->bd_dev);

	buf->f_type = CRAMFS_MAGIC;
	buf->f_bsize = PAGE_CACHE_SIZE;
	buf->f_blocks = CRAMFS_SB(sb)->blocks;
	buf->f_bfree = 0;
	buf->f_bavail = 0;
	buf->f_files = CRAMFS_SB(sb)->files;
	buf->f_ffree = 0;
	buf->f_fsid.val[0] = (u32)id;
	buf->f_fsid.val[1] = (u32)(id >> 32);
	buf->f_namelen = CRAMFS_MAXPATHLEN;
	return 0;
}

static int cramfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_path.dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	char *buf;
	unsigned int offset;
	int copied;

	
	offset = filp->f_pos;
	if (offset >= inode->i_size)
		return 0;
	
	if (offset & 3)
		return -EINVAL;

	buf = kmalloc(CRAMFS_MAXPATHLEN, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	copied = 0;
	while (offset < inode->i_size) {
		struct cramfs_inode *de;
		unsigned long nextoffset;
		char *name;
		ino_t ino;
		umode_t mode;
		int namelen, error;

		mutex_lock(&read_mutex);
		de = cramfs_read(sb, OFFSET(inode) + offset, sizeof(*de)+CRAMFS_MAXPATHLEN);
		name = (char *)(de+1);

		namelen = de->namelen << 2;
		memcpy(buf, name, namelen);
		ino = cramino(de, OFFSET(inode) + offset);
		mode = de->mode;
		mutex_unlock(&read_mutex);
		nextoffset = offset + sizeof(*de) + namelen;
		for (;;) {
			if (!namelen) {
				kfree(buf);
				return -EIO;
			}
			if (buf[namelen-1])
				break;
			namelen--;
		}
		error = filldir(dirent, buf, namelen, offset, ino, mode >> 12);
		if (error)
			break;

		offset = nextoffset;
		filp->f_pos = offset;
		copied++;
	}
	kfree(buf);
	return 0;
}

static struct dentry * cramfs_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
	unsigned int offset = 0;
	struct inode *inode = NULL;
	int sorted;

	mutex_lock(&read_mutex);
	sorted = CRAMFS_SB(dir->i_sb)->flags & CRAMFS_FLAG_SORTED_DIRS;
	while (offset < dir->i_size) {
		struct cramfs_inode *de;
		char *name;
		int namelen, retval;
		int dir_off = OFFSET(dir) + offset;

		de = cramfs_read(dir->i_sb, dir_off, sizeof(*de)+CRAMFS_MAXPATHLEN);
		name = (char *)(de+1);

		
		if (sorted && (dentry->d_name.name[0] < name[0]))
			break;

		namelen = de->namelen << 2;
		offset += sizeof(*de) + namelen;

		
		if (((dentry->d_name.len + 3) & ~3) != namelen)
			continue;

		for (;;) {
			if (!namelen) {
				inode = ERR_PTR(-EIO);
				goto out;
			}
			if (name[namelen-1])
				break;
			namelen--;
		}
		if (namelen != dentry->d_name.len)
			continue;
		retval = memcmp(dentry->d_name.name, name, namelen);
		if (retval > 0)
			continue;
		if (!retval) {
			inode = get_cramfs_inode(dir->i_sb, de, dir_off);
			break;
		}
		
		if (sorted)
			break;
	}
out:
	mutex_unlock(&read_mutex);
	if (IS_ERR(inode))
		return ERR_CAST(inode);
	d_add(dentry, inode);
	return NULL;
}

static int cramfs_readpage(struct file *file, struct page * page)
{
	struct inode *inode = page->mapping->host;
	u32 maxblock;
	int bytes_filled;
	void *pgdata;

	maxblock = (inode->i_size + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT;
	bytes_filled = 0;
	pgdata = kmap(page);

	if (page->index < maxblock) {
		struct super_block *sb = inode->i_sb;
		u32 blkptr_offset = OFFSET(inode) + page->index*4;
		u32 start_offset, compr_len;

		start_offset = OFFSET(inode) + maxblock*4;
		mutex_lock(&read_mutex);
		if (page->index)
			start_offset = *(u32 *) cramfs_read(sb, blkptr_offset-4,
				4);
		compr_len = (*(u32 *) cramfs_read(sb, blkptr_offset, 4) -
			start_offset);
		mutex_unlock(&read_mutex);

		if (compr_len == 0)
			; 
		else if (unlikely(compr_len > (PAGE_CACHE_SIZE << 1))) {
			pr_err("cramfs: bad compressed blocksize %u\n",
				compr_len);
			goto err;
		} else {
			mutex_lock(&read_mutex);
			bytes_filled = cramfs_uncompress_block(pgdata,
				 PAGE_CACHE_SIZE,
				 cramfs_read(sb, start_offset, compr_len),
				 compr_len);
			mutex_unlock(&read_mutex);
			if (unlikely(bytes_filled < 0))
				goto err;
		}
	}

	memset(pgdata + bytes_filled, 0, PAGE_CACHE_SIZE - bytes_filled);
	flush_dcache_page(page);
	kunmap(page);
	SetPageUptodate(page);
	unlock_page(page);
	return 0;

err:
	kunmap(page);
	ClearPageUptodate(page);
	SetPageError(page);
	unlock_page(page);
	return 0;
}

static const struct address_space_operations cramfs_aops = {
	.readpage = cramfs_readpage
};


static const struct file_operations cramfs_directory_operations = {
	.llseek		= generic_file_llseek,
	.read		= generic_read_dir,
	.readdir	= cramfs_readdir,
};

static const struct inode_operations cramfs_dir_inode_operations = {
	.lookup		= cramfs_lookup,
};

static const struct super_operations cramfs_ops = {
	.put_super	= cramfs_put_super,
	.remount_fs	= cramfs_remount,
	.statfs		= cramfs_statfs,
};

static struct dentry *cramfs_mount(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, cramfs_fill_super);
}

static struct file_system_type cramfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "cramfs",
	.mount		= cramfs_mount,
	.kill_sb	= kill_block_super,
	.fs_flags	= FS_REQUIRES_DEV,
};

static int __init init_cramfs_fs(void)
{
	int rv;

	rv = cramfs_uncompress_init();
	if (rv < 0)
		return rv;
	rv = register_filesystem(&cramfs_fs_type);
	if (rv < 0)
		cramfs_uncompress_exit();
	return rv;
}

static void __exit exit_cramfs_fs(void)
{
	cramfs_uncompress_exit();
	unregister_filesystem(&cramfs_fs_type);
}

module_init(init_cramfs_fs)
module_exit(exit_cramfs_fs)
MODULE_LICENSE("GPL");
