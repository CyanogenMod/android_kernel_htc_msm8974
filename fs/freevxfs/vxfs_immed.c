/*
 * Copyright (c) 2000-2001 Christoph Hellwig.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL").
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/namei.h>

#include "vxfs.h"
#include "vxfs_extern.h"
#include "vxfs_inode.h"


static void *	vxfs_immed_follow_link(struct dentry *, struct nameidata *);

static int	vxfs_immed_readpage(struct file *, struct page *);

const struct inode_operations vxfs_immed_symlink_iops = {
	.readlink =		generic_readlink,
	.follow_link =		vxfs_immed_follow_link,
};

const struct address_space_operations vxfs_immed_aops = {
	.readpage =		vxfs_immed_readpage,
};

static void *
vxfs_immed_follow_link(struct dentry *dp, struct nameidata *np)
{
	struct vxfs_inode_info		*vip = VXFS_INO(dp->d_inode);
	nd_set_link(np, vip->vii_immed.vi_immed);
	return NULL;
}

static int
vxfs_immed_readpage(struct file *fp, struct page *pp)
{
	struct vxfs_inode_info	*vip = VXFS_INO(pp->mapping->host);
	u_int64_t	offset = (u_int64_t)pp->index << PAGE_CACHE_SHIFT;
	caddr_t		kaddr;

	kaddr = kmap(pp);
	memcpy(kaddr, vip->vii_immed.vi_immed + offset, PAGE_CACHE_SIZE);
	kunmap(pp);
	
	flush_dcache_page(pp);
	SetPageUptodate(pp);
        unlock_page(pp);

	return 0;
}
