/*
 *  linux/fs/ufs/ialloc.c
 *
 * Copyright (c) 1998
 * Daniel Pirkl <daniel.pirkl@email.cz>
 * Charles University, Faculty of Mathematics and Physics
 *
 *  from
 *
 *  linux/fs/ext2/ialloc.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  BSD ufs-inspired inode and directory allocation by 
 *  Stephen Tweedie (sct@dcs.ed.ac.uk), 1993
 *  Big-endian to little-endian byte-swapping/bitmaps by
 *        David S. Miller (davem@caip.rutgers.edu), 1995
 *
 * UFS2 write support added by
 * Evgeniy Dushistov <dushistov@mail.ru>, 2007
 */

#include <linux/fs.h>
#include <linux/time.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/buffer_head.h>
#include <linux/sched.h>
#include <linux/bitops.h>
#include <asm/byteorder.h>

#include "ufs_fs.h"
#include "ufs.h"
#include "swab.h"
#include "util.h"

void ufs_free_inode (struct inode * inode)
{
	struct super_block * sb;
	struct ufs_sb_private_info * uspi;
	struct ufs_super_block_first * usb1;
	struct ufs_cg_private_info * ucpi;
	struct ufs_cylinder_group * ucg;
	int is_directory;
	unsigned ino, cg, bit;
	
	UFSD("ENTER, ino %lu\n", inode->i_ino);

	sb = inode->i_sb;
	uspi = UFS_SB(sb)->s_uspi;
	usb1 = ubh_get_usb_first(uspi);
	
	ino = inode->i_ino;

	lock_super (sb);

	if (!((ino > 1) && (ino < (uspi->s_ncg * uspi->s_ipg )))) {
		ufs_warning(sb, "ufs_free_inode", "reserved inode or nonexistent inode %u\n", ino);
		unlock_super (sb);
		return;
	}
	
	cg = ufs_inotocg (ino);
	bit = ufs_inotocgoff (ino);
	ucpi = ufs_load_cylinder (sb, cg);
	if (!ucpi) {
		unlock_super (sb);
		return;
	}
	ucg = ubh_get_ucg(UCPI_UBH(ucpi));
	if (!ufs_cg_chkmagic(sb, ucg))
		ufs_panic (sb, "ufs_free_fragments", "internal error, bad cg magic number");

	ucg->cg_time = cpu_to_fs32(sb, get_seconds());

	is_directory = S_ISDIR(inode->i_mode);

	if (ubh_isclr (UCPI_UBH(ucpi), ucpi->c_iusedoff, bit))
		ufs_error(sb, "ufs_free_inode", "bit already cleared for inode %u", ino);
	else {
		ubh_clrbit (UCPI_UBH(ucpi), ucpi->c_iusedoff, bit);
		if (ino < ucpi->c_irotor)
			ucpi->c_irotor = ino;
		fs32_add(sb, &ucg->cg_cs.cs_nifree, 1);
		uspi->cs_total.cs_nifree++;
		fs32_add(sb, &UFS_SB(sb)->fs_cs(cg).cs_nifree, 1);

		if (is_directory) {
			fs32_sub(sb, &ucg->cg_cs.cs_ndir, 1);
			uspi->cs_total.cs_ndir--;
			fs32_sub(sb, &UFS_SB(sb)->fs_cs(cg).cs_ndir, 1);
		}
	}

	ubh_mark_buffer_dirty (USPI_UBH(uspi));
	ubh_mark_buffer_dirty (UCPI_UBH(ucpi));
	if (sb->s_flags & MS_SYNCHRONOUS)
		ubh_sync_block(UCPI_UBH(ucpi));
	
	sb->s_dirt = 1;
	unlock_super (sb);
	UFSD("EXIT\n");
}

static void ufs2_init_inodes_chunk(struct super_block *sb,
				   struct ufs_cg_private_info *ucpi,
				   struct ufs_cylinder_group *ucg)
{
	struct buffer_head *bh;
	struct ufs_sb_private_info *uspi = UFS_SB(sb)->s_uspi;
	sector_t beg = uspi->s_sbbase +
		ufs_inotofsba(ucpi->c_cgx * uspi->s_ipg +
			      fs32_to_cpu(sb, ucg->cg_u.cg_u2.cg_initediblk));
	sector_t end = beg + uspi->s_fpb;

	UFSD("ENTER cgno %d\n", ucpi->c_cgx);

	for (; beg < end; ++beg) {
		bh = sb_getblk(sb, beg);
		lock_buffer(bh);
		memset(bh->b_data, 0, sb->s_blocksize);
		set_buffer_uptodate(bh);
		mark_buffer_dirty(bh);
		unlock_buffer(bh);
		if (sb->s_flags & MS_SYNCHRONOUS)
			sync_dirty_buffer(bh);
		brelse(bh);
	}

	fs32_add(sb, &ucg->cg_u.cg_u2.cg_initediblk, uspi->s_inopb);
	ubh_mark_buffer_dirty(UCPI_UBH(ucpi));
	if (sb->s_flags & MS_SYNCHRONOUS)
		ubh_sync_block(UCPI_UBH(ucpi));

	UFSD("EXIT\n");
}

struct inode *ufs_new_inode(struct inode *dir, umode_t mode)
{
	struct super_block * sb;
	struct ufs_sb_info * sbi;
	struct ufs_sb_private_info * uspi;
	struct ufs_super_block_first * usb1;
	struct ufs_cg_private_info * ucpi;
	struct ufs_cylinder_group * ucg;
	struct inode * inode;
	unsigned cg, bit, i, j, start;
	struct ufs_inode_info *ufsi;
	int err = -ENOSPC;

	UFSD("ENTER\n");
	
	
	if (!dir || !dir->i_nlink)
		return ERR_PTR(-EPERM);
	sb = dir->i_sb;
	inode = new_inode(sb);
	if (!inode)
		return ERR_PTR(-ENOMEM);
	ufsi = UFS_I(inode);
	sbi = UFS_SB(sb);
	uspi = sbi->s_uspi;
	usb1 = ubh_get_usb_first(uspi);

	lock_super (sb);

	i = ufs_inotocg(dir->i_ino);
	if (sbi->fs_cs(i).cs_nifree) {
		cg = i;
		goto cg_found;
	}

	for ( j = 1; j < uspi->s_ncg; j <<= 1 ) {
		i += j;
		if (i >= uspi->s_ncg)
			i -= uspi->s_ncg;
		if (sbi->fs_cs(i).cs_nifree) {
			cg = i;
			goto cg_found;
		}
	}

	i = ufs_inotocg(dir->i_ino) + 1;
	for (j = 2; j < uspi->s_ncg; j++) {
		i++;
		if (i >= uspi->s_ncg)
			i = 0;
		if (sbi->fs_cs(i).cs_nifree) {
			cg = i;
			goto cg_found;
		}
	}

	goto failed;

cg_found:
	ucpi = ufs_load_cylinder (sb, cg);
	if (!ucpi) {
		err = -EIO;
		goto failed;
	}
	ucg = ubh_get_ucg(UCPI_UBH(ucpi));
	if (!ufs_cg_chkmagic(sb, ucg)) 
		ufs_panic (sb, "ufs_new_inode", "internal error, bad cg magic number");

	start = ucpi->c_irotor;
	bit = ubh_find_next_zero_bit (UCPI_UBH(ucpi), ucpi->c_iusedoff, uspi->s_ipg, start);
	if (!(bit < uspi->s_ipg)) {
		bit = ubh_find_first_zero_bit (UCPI_UBH(ucpi), ucpi->c_iusedoff, start);
		if (!(bit < start)) {
			ufs_error (sb, "ufs_new_inode",
			    "cylinder group %u corrupted - error in inode bitmap\n", cg);
			err = -EIO;
			goto failed;
		}
	}
	UFSD("start = %u, bit = %u, ipg = %u\n", start, bit, uspi->s_ipg);
	if (ubh_isclr (UCPI_UBH(ucpi), ucpi->c_iusedoff, bit))
		ubh_setbit (UCPI_UBH(ucpi), ucpi->c_iusedoff, bit);
	else {
		ufs_panic (sb, "ufs_new_inode", "internal error");
		err = -EIO;
		goto failed;
	}

	if (uspi->fs_magic == UFS2_MAGIC) {
		u32 initediblk = fs32_to_cpu(sb, ucg->cg_u.cg_u2.cg_initediblk);

		if (bit + uspi->s_inopb > initediblk &&
		    initediblk < fs32_to_cpu(sb, ucg->cg_u.cg_u2.cg_niblk))
			ufs2_init_inodes_chunk(sb, ucpi, ucg);
	}

	fs32_sub(sb, &ucg->cg_cs.cs_nifree, 1);
	uspi->cs_total.cs_nifree--;
	fs32_sub(sb, &sbi->fs_cs(cg).cs_nifree, 1);
	
	if (S_ISDIR(mode)) {
		fs32_add(sb, &ucg->cg_cs.cs_ndir, 1);
		uspi->cs_total.cs_ndir++;
		fs32_add(sb, &sbi->fs_cs(cg).cs_ndir, 1);
	}
	ubh_mark_buffer_dirty (USPI_UBH(uspi));
	ubh_mark_buffer_dirty (UCPI_UBH(ucpi));
	if (sb->s_flags & MS_SYNCHRONOUS)
		ubh_sync_block(UCPI_UBH(ucpi));
	sb->s_dirt = 1;

	inode->i_ino = cg * uspi->s_ipg + bit;
	inode_init_owner(inode, dir, mode);
	inode->i_blocks = 0;
	inode->i_generation = 0;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME_SEC;
	ufsi->i_flags = UFS_I(dir)->i_flags;
	ufsi->i_lastfrag = 0;
	ufsi->i_shadow = 0;
	ufsi->i_osync = 0;
	ufsi->i_oeftflag = 0;
	ufsi->i_dir_start_lookup = 0;
	memset(&ufsi->i_u1, 0, sizeof(ufsi->i_u1));
	insert_inode_hash(inode);
	mark_inode_dirty(inode);

	if (uspi->fs_magic == UFS2_MAGIC) {
		struct buffer_head *bh;
		struct ufs2_inode *ufs2_inode;

		bh = sb_bread(sb, uspi->s_sbbase + ufs_inotofsba(inode->i_ino));
		if (!bh) {
			ufs_warning(sb, "ufs_read_inode",
				    "unable to read inode %lu\n",
				    inode->i_ino);
			err = -EIO;
			goto fail_remove_inode;
		}
		lock_buffer(bh);
		ufs2_inode = (struct ufs2_inode *)bh->b_data;
		ufs2_inode += ufs_inotofsbo(inode->i_ino);
		ufs2_inode->ui_birthtime = cpu_to_fs64(sb, CURRENT_TIME.tv_sec);
		ufs2_inode->ui_birthnsec = cpu_to_fs32(sb, CURRENT_TIME.tv_nsec);
		mark_buffer_dirty(bh);
		unlock_buffer(bh);
		if (sb->s_flags & MS_SYNCHRONOUS)
			sync_dirty_buffer(bh);
		brelse(bh);
	}

	unlock_super (sb);

	UFSD("allocating inode %lu\n", inode->i_ino);
	UFSD("EXIT\n");
	return inode;

fail_remove_inode:
	unlock_super(sb);
	clear_nlink(inode);
	iput(inode);
	UFSD("EXIT (FAILED): err %d\n", err);
	return ERR_PTR(err);
failed:
	unlock_super (sb);
	make_bad_inode(inode);
	iput (inode);
	UFSD("EXIT (FAILED): err %d\n", err);
	return ERR_PTR(err);
}
