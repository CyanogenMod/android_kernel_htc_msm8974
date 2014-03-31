/* NFS filesystem cache interface
 *
 * Copyright (C) 2008 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/nfs_fs.h>
#include <linux/nfs_fs_sb.h>
#include <linux/in6.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#include "internal.h"
#include "iostat.h"
#include "fscache.h"

#define NFSDBG_FACILITY		NFSDBG_FSCACHE

static struct rb_root nfs_fscache_keys = RB_ROOT;
static DEFINE_SPINLOCK(nfs_fscache_keys_lock);

void nfs_fscache_get_client_cookie(struct nfs_client *clp)
{
	
	clp->fscache = fscache_acquire_cookie(nfs_fscache_netfs.primary_index,
					      &nfs_fscache_server_index_def,
					      clp);
	dfprintk(FSCACHE, "NFS: get client cookie (0x%p/0x%p)\n",
		 clp, clp->fscache);
}

void nfs_fscache_release_client_cookie(struct nfs_client *clp)
{
	dfprintk(FSCACHE, "NFS: releasing client cookie (0x%p/0x%p)\n",
		 clp, clp->fscache);

	fscache_relinquish_cookie(clp->fscache, 0);
	clp->fscache = NULL;
}

void nfs_fscache_get_super_cookie(struct super_block *sb, const char *uniq,
				  struct nfs_clone_mount *mntdata)
{
	struct nfs_fscache_key *key, *xkey;
	struct nfs_server *nfss = NFS_SB(sb);
	struct rb_node **p, *parent;
	int diff, ulen;

	if (uniq) {
		ulen = strlen(uniq);
	} else if (mntdata) {
		struct nfs_server *mnt_s = NFS_SB(mntdata->sb);
		if (mnt_s->fscache_key) {
			uniq = mnt_s->fscache_key->key.uniquifier;
			ulen = mnt_s->fscache_key->key.uniq_len;
		}
	}

	if (!uniq) {
		uniq = "";
		ulen = 1;
	}

	key = kzalloc(sizeof(*key) + ulen, GFP_KERNEL);
	if (!key)
		return;

	key->nfs_client = nfss->nfs_client;
	key->key.super.s_flags = sb->s_flags & NFS_MS_MASK;
	key->key.nfs_server.flags = nfss->flags;
	key->key.nfs_server.rsize = nfss->rsize;
	key->key.nfs_server.wsize = nfss->wsize;
	key->key.nfs_server.acregmin = nfss->acregmin;
	key->key.nfs_server.acregmax = nfss->acregmax;
	key->key.nfs_server.acdirmin = nfss->acdirmin;
	key->key.nfs_server.acdirmax = nfss->acdirmax;
	key->key.nfs_server.fsid = nfss->fsid;
	key->key.rpc_auth.au_flavor = nfss->client->cl_auth->au_flavor;

	key->key.uniq_len = ulen;
	memcpy(key->key.uniquifier, uniq, ulen);

	spin_lock(&nfs_fscache_keys_lock);
	p = &nfs_fscache_keys.rb_node;
	parent = NULL;
	while (*p) {
		parent = *p;
		xkey = rb_entry(parent, struct nfs_fscache_key, node);

		if (key->nfs_client < xkey->nfs_client)
			goto go_left;
		if (key->nfs_client > xkey->nfs_client)
			goto go_right;

		diff = memcmp(&key->key, &xkey->key, sizeof(key->key));
		if (diff < 0)
			goto go_left;
		if (diff > 0)
			goto go_right;

		if (key->key.uniq_len == 0)
			goto non_unique;
		diff = memcmp(key->key.uniquifier,
			      xkey->key.uniquifier,
			      key->key.uniq_len);
		if (diff < 0)
			goto go_left;
		if (diff > 0)
			goto go_right;
		goto non_unique;

	go_left:
		p = &(*p)->rb_left;
		continue;
	go_right:
		p = &(*p)->rb_right;
	}

	rb_link_node(&key->node, parent, p);
	rb_insert_color(&key->node, &nfs_fscache_keys);
	spin_unlock(&nfs_fscache_keys_lock);
	nfss->fscache_key = key;

	
	nfss->fscache = fscache_acquire_cookie(nfss->nfs_client->fscache,
					       &nfs_fscache_super_index_def,
					       nfss);
	dfprintk(FSCACHE, "NFS: get superblock cookie (0x%p/0x%p)\n",
		 nfss, nfss->fscache);
	return;

non_unique:
	spin_unlock(&nfs_fscache_keys_lock);
	kfree(key);
	nfss->fscache_key = NULL;
	nfss->fscache = NULL;
	printk(KERN_WARNING "NFS:"
	       " Cache request denied due to non-unique superblock keys\n");
}

void nfs_fscache_release_super_cookie(struct super_block *sb)
{
	struct nfs_server *nfss = NFS_SB(sb);

	dfprintk(FSCACHE, "NFS: releasing superblock cookie (0x%p/0x%p)\n",
		 nfss, nfss->fscache);

	fscache_relinquish_cookie(nfss->fscache, 0);
	nfss->fscache = NULL;

	if (nfss->fscache_key) {
		spin_lock(&nfs_fscache_keys_lock);
		rb_erase(&nfss->fscache_key->node, &nfs_fscache_keys);
		spin_unlock(&nfs_fscache_keys_lock);
		kfree(nfss->fscache_key);
		nfss->fscache_key = NULL;
	}
}

void nfs_fscache_init_inode_cookie(struct inode *inode)
{
	NFS_I(inode)->fscache = NULL;
	if (S_ISREG(inode->i_mode))
		set_bit(NFS_INO_FSCACHE, &NFS_I(inode)->flags);
}

static void nfs_fscache_enable_inode_cookie(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	struct nfs_inode *nfsi = NFS_I(inode);

	if (nfsi->fscache || !NFS_FSCACHE(inode))
		return;

	if ((NFS_SB(sb)->options & NFS_OPTION_FSCACHE)) {
		nfsi->fscache = fscache_acquire_cookie(
			NFS_SB(sb)->fscache,
			&nfs_fscache_inode_object_def,
			nfsi);

		dfprintk(FSCACHE, "NFS: get FH cookie (0x%p/0x%p/0x%p)\n",
			 sb, nfsi, nfsi->fscache);
	}
}

void nfs_fscache_release_inode_cookie(struct inode *inode)
{
	struct nfs_inode *nfsi = NFS_I(inode);

	dfprintk(FSCACHE, "NFS: clear cookie (0x%p/0x%p)\n",
		 nfsi, nfsi->fscache);

	fscache_relinquish_cookie(nfsi->fscache, 0);
	nfsi->fscache = NULL;
}

void nfs_fscache_zap_inode_cookie(struct inode *inode)
{
	struct nfs_inode *nfsi = NFS_I(inode);

	dfprintk(FSCACHE, "NFS: zapping cookie (0x%p/0x%p)\n",
		 nfsi, nfsi->fscache);

	fscache_relinquish_cookie(nfsi->fscache, 1);
	nfsi->fscache = NULL;
}

static void nfs_fscache_disable_inode_cookie(struct inode *inode)
{
	clear_bit(NFS_INO_FSCACHE, &NFS_I(inode)->flags);

	if (NFS_I(inode)->fscache) {
		dfprintk(FSCACHE,
			 "NFS: nfsi 0x%p turning cache off\n", NFS_I(inode));

		fscache_uncache_all_inode_pages(NFS_I(inode)->fscache, inode);
		nfs_fscache_zap_inode_cookie(inode);
	}
}

static int nfs_fscache_wait_bit(void *flags)
{
	schedule();
	return 0;
}

static inline void nfs_fscache_inode_lock(struct inode *inode)
{
	struct nfs_inode *nfsi = NFS_I(inode);

	while (test_and_set_bit(NFS_INO_FSCACHE_LOCK, &nfsi->flags))
		wait_on_bit(&nfsi->flags, NFS_INO_FSCACHE_LOCK,
			    nfs_fscache_wait_bit, TASK_UNINTERRUPTIBLE);
}

static inline void nfs_fscache_inode_unlock(struct inode *inode)
{
	struct nfs_inode *nfsi = NFS_I(inode);

	smp_mb__before_clear_bit();
	clear_bit(NFS_INO_FSCACHE_LOCK, &nfsi->flags);
	smp_mb__after_clear_bit();
	wake_up_bit(&nfsi->flags, NFS_INO_FSCACHE_LOCK);
}

void nfs_fscache_set_inode_cookie(struct inode *inode, struct file *filp)
{
	if (NFS_FSCACHE(inode)) {
		nfs_fscache_inode_lock(inode);
		if ((filp->f_flags & O_ACCMODE) != O_RDONLY)
			nfs_fscache_disable_inode_cookie(inode);
		else
			nfs_fscache_enable_inode_cookie(inode);
		nfs_fscache_inode_unlock(inode);
	}
}

void nfs_fscache_reset_inode_cookie(struct inode *inode)
{
	struct nfs_inode *nfsi = NFS_I(inode);
	struct nfs_server *nfss = NFS_SERVER(inode);
	NFS_IFDEBUG(struct fscache_cookie *old = nfsi->fscache);

	nfs_fscache_inode_lock(inode);
	if (nfsi->fscache) {
		
		fscache_relinquish_cookie(nfsi->fscache, 1);

		nfsi->fscache = fscache_acquire_cookie(
			nfss->nfs_client->fscache,
			&nfs_fscache_inode_object_def,
			nfsi);

		dfprintk(FSCACHE,
			 "NFS: revalidation new cookie (0x%p/0x%p/0x%p/0x%p)\n",
			 nfss, nfsi, old, nfsi->fscache);
	}
	nfs_fscache_inode_unlock(inode);
}

int nfs_fscache_release_page(struct page *page, gfp_t gfp)
{
	if (PageFsCache(page)) {
		struct nfs_inode *nfsi = NFS_I(page->mapping->host);
		struct fscache_cookie *cookie = nfsi->fscache;

		BUG_ON(!cookie);
		dfprintk(FSCACHE, "NFS: fscache releasepage (0x%p/0x%p/0x%p)\n",
			 cookie, page, nfsi);

		if (!fscache_maybe_release_page(cookie, page, gfp))
			return 0;

		nfs_add_fscache_stats(page->mapping->host,
				      NFSIOS_FSCACHE_PAGES_UNCACHED, 1);
	}

	return 1;
}

void __nfs_fscache_invalidate_page(struct page *page, struct inode *inode)
{
	struct nfs_inode *nfsi = NFS_I(inode);
	struct fscache_cookie *cookie = nfsi->fscache;

	BUG_ON(!cookie);

	dfprintk(FSCACHE, "NFS: fscache invalidatepage (0x%p/0x%p/0x%p)\n",
		 cookie, page, nfsi);

	fscache_wait_on_page_write(cookie, page);

	BUG_ON(!PageLocked(page));
	fscache_uncache_page(cookie, page);
	nfs_add_fscache_stats(page->mapping->host,
			      NFSIOS_FSCACHE_PAGES_UNCACHED, 1);
}

static void nfs_readpage_from_fscache_complete(struct page *page,
					       void *context,
					       int error)
{
	dfprintk(FSCACHE,
		 "NFS: readpage_from_fscache_complete (0x%p/0x%p/%d)\n",
		 page, context, error);

	if (!error) {
		SetPageUptodate(page);
		unlock_page(page);
	} else {
		error = nfs_readpage_async(context, page->mapping->host, page);
		if (error)
			unlock_page(page);
	}
}

int __nfs_readpage_from_fscache(struct nfs_open_context *ctx,
				struct inode *inode, struct page *page)
{
	int ret;

	dfprintk(FSCACHE,
		 "NFS: readpage_from_fscache(fsc:%p/p:%p(i:%lx f:%lx)/0x%p)\n",
		 NFS_I(inode)->fscache, page, page->index, page->flags, inode);

	ret = fscache_read_or_alloc_page(NFS_I(inode)->fscache,
					 page,
					 nfs_readpage_from_fscache_complete,
					 ctx,
					 GFP_KERNEL);

	switch (ret) {
	case 0: 
		dfprintk(FSCACHE,
			 "NFS:    readpage_from_fscache: BIO submitted\n");
		nfs_add_fscache_stats(inode, NFSIOS_FSCACHE_PAGES_READ_OK, 1);
		return ret;

	case -ENOBUFS: 
	case -ENODATA: 
		nfs_add_fscache_stats(inode, NFSIOS_FSCACHE_PAGES_READ_FAIL, 1);
		dfprintk(FSCACHE,
			 "NFS:    readpage_from_fscache %d\n", ret);
		return 1;

	default:
		dfprintk(FSCACHE, "NFS:    readpage_from_fscache %d\n", ret);
		nfs_add_fscache_stats(inode, NFSIOS_FSCACHE_PAGES_READ_FAIL, 1);
	}
	return ret;
}

int __nfs_readpages_from_fscache(struct nfs_open_context *ctx,
				 struct inode *inode,
				 struct address_space *mapping,
				 struct list_head *pages,
				 unsigned *nr_pages)
{
	unsigned npages = *nr_pages;
	int ret;

	dfprintk(FSCACHE, "NFS: nfs_getpages_from_fscache (0x%p/%u/0x%p)\n",
		 NFS_I(inode)->fscache, npages, inode);

	ret = fscache_read_or_alloc_pages(NFS_I(inode)->fscache,
					  mapping, pages, nr_pages,
					  nfs_readpage_from_fscache_complete,
					  ctx,
					  mapping_gfp_mask(mapping));
	if (*nr_pages < npages)
		nfs_add_fscache_stats(inode, NFSIOS_FSCACHE_PAGES_READ_OK,
				      npages);
	if (*nr_pages > 0)
		nfs_add_fscache_stats(inode, NFSIOS_FSCACHE_PAGES_READ_FAIL,
				      *nr_pages);

	switch (ret) {
	case 0: 
		BUG_ON(!list_empty(pages));
		BUG_ON(*nr_pages != 0);
		dfprintk(FSCACHE,
			 "NFS: nfs_getpages_from_fscache: submitted\n");

		return ret;

	case -ENOBUFS: 
	case -ENODATA: 
		dfprintk(FSCACHE,
			 "NFS: nfs_getpages_from_fscache: no page: %d\n", ret);
		return 1;

	default:
		dfprintk(FSCACHE,
			 "NFS: nfs_getpages_from_fscache: ret  %d\n", ret);
	}

	return ret;
}

void __nfs_readpage_to_fscache(struct inode *inode, struct page *page, int sync)
{
	int ret;

	dfprintk(FSCACHE,
		 "NFS: readpage_to_fscache(fsc:%p/p:%p(i:%lx f:%lx)/%d)\n",
		 NFS_I(inode)->fscache, page, page->index, page->flags, sync);

	ret = fscache_write_page(NFS_I(inode)->fscache, page, GFP_KERNEL);
	dfprintk(FSCACHE,
		 "NFS:     readpage_to_fscache: p:%p(i:%lu f:%lx) ret %d\n",
		 page, page->index, page->flags, ret);

	if (ret != 0) {
		fscache_uncache_page(NFS_I(inode)->fscache, page);
		nfs_add_fscache_stats(inode,
				      NFSIOS_FSCACHE_PAGES_WRITTEN_FAIL, 1);
		nfs_add_fscache_stats(inode, NFSIOS_FSCACHE_PAGES_UNCACHED, 1);
	} else {
		nfs_add_fscache_stats(inode,
				      NFSIOS_FSCACHE_PAGES_WRITTEN_OK, 1);
	}
}
