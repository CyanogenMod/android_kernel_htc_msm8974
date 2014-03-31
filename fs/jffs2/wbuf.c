/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright © 2001-2007 Red Hat, Inc.
 * Copyright © 2004 Thomas Gleixner <tglx@linutronix.de>
 *
 * Created by David Woodhouse <dwmw2@infradead.org>
 * Modified debugged and enhanced by Thomas Gleixner <tglx@linutronix.de>
 *
 * For licensing information, see the file 'LICENCE' in this directory.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mtd/mtd.h>
#include <linux/crc32.h>
#include <linux/mtd/nand.h>
#include <linux/jiffies.h>
#include <linux/sched.h>

#include "nodelist.h"

#undef BREAKME
#undef BREAKMEHEADER

#ifdef BREAKME
static unsigned char *brokenbuf;
#endif

#define PAGE_DIV(x) ( ((unsigned long)(x) / (unsigned long)(c->wbuf_pagesize)) * (unsigned long)(c->wbuf_pagesize) )
#define PAGE_MOD(x) ( (unsigned long)(x) % (unsigned long)(c->wbuf_pagesize) )

#define MAX_ERASE_FAILURES 	2

struct jffs2_inodirty {
	uint32_t ino;
	struct jffs2_inodirty *next;
};

static struct jffs2_inodirty inodirty_nomem;

static int jffs2_wbuf_pending_for_ino(struct jffs2_sb_info *c, uint32_t ino)
{
	struct jffs2_inodirty *this = c->wbuf_inodes;

	
	if (this == &inodirty_nomem)
		return 1;

	
	if (this && !ino)
		return 1;

	
	while (this) {
		if (this->ino == ino)
			return 1;
		this = this->next;
	}
	return 0;
}

static void jffs2_clear_wbuf_ino_list(struct jffs2_sb_info *c)
{
	struct jffs2_inodirty *this;

	this = c->wbuf_inodes;

	if (this != &inodirty_nomem) {
		while (this) {
			struct jffs2_inodirty *next = this->next;
			kfree(this);
			this = next;
		}
	}
	c->wbuf_inodes = NULL;
}

static void jffs2_wbuf_dirties_inode(struct jffs2_sb_info *c, uint32_t ino)
{
	struct jffs2_inodirty *new;

	
	jffs2_dirty_trigger(c);

	if (jffs2_wbuf_pending_for_ino(c, ino))
		return;

	new = kmalloc(sizeof(*new), GFP_KERNEL);
	if (!new) {
		jffs2_dbg(1, "No memory to allocate inodirty. Fallback to all considered dirty\n");
		jffs2_clear_wbuf_ino_list(c);
		c->wbuf_inodes = &inodirty_nomem;
		return;
	}
	new->ino = ino;
	new->next = c->wbuf_inodes;
	c->wbuf_inodes = new;
	return;
}

static inline void jffs2_refile_wbuf_blocks(struct jffs2_sb_info *c)
{
	struct list_head *this, *next;
	static int n;

	if (list_empty(&c->erasable_pending_wbuf_list))
		return;

	list_for_each_safe(this, next, &c->erasable_pending_wbuf_list) {
		struct jffs2_eraseblock *jeb = list_entry(this, struct jffs2_eraseblock, list);

		jffs2_dbg(1, "Removing eraseblock at 0x%08x from erasable_pending_wbuf_list...\n",
			  jeb->offset);
		list_del(this);
		if ((jiffies + (n++)) & 127) {
			jffs2_dbg(1, "...and adding to erase_pending_list\n");
			list_add_tail(&jeb->list, &c->erase_pending_list);
			c->nr_erasing_blocks++;
			jffs2_garbage_collect_trigger(c);
		} else {
			jffs2_dbg(1, "...and adding to erasable_list\n");
			list_add_tail(&jeb->list, &c->erasable_list);
		}
	}
}

#define REFILE_NOTEMPTY 0
#define REFILE_ANYWAY   1

static void jffs2_block_refile(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb, int allow_empty)
{
	jffs2_dbg(1, "About to refile bad block at %08x\n", jeb->offset);

	
	if (c->nextblock == jeb)
		c->nextblock = NULL;
	else 
		list_del(&jeb->list);
	if (jeb->first_node) {
		jffs2_dbg(1, "Refiling block at %08x to bad_used_list\n",
			  jeb->offset);
		list_add(&jeb->list, &c->bad_used_list);
	} else {
		BUG_ON(allow_empty == REFILE_NOTEMPTY);
		
		jffs2_dbg(1, "Refiling block at %08x to erase_pending_list\n",
			  jeb->offset);
		list_add(&jeb->list, &c->erase_pending_list);
		c->nr_erasing_blocks++;
		jffs2_garbage_collect_trigger(c);
	}

	if (!jffs2_prealloc_raw_node_refs(c, jeb, 1)) {
		uint32_t oldfree = jeb->free_size;

		jffs2_link_node_ref(c, jeb, 
				    (jeb->offset+c->sector_size-oldfree) | REF_OBSOLETE,
				    oldfree, NULL);
		
		c->wasted_size += oldfree;
		jeb->wasted_size += oldfree;
		c->dirty_size -= oldfree;
		jeb->dirty_size -= oldfree;
	}

	jffs2_dbg_dump_block_lists_nolock(c);
	jffs2_dbg_acct_sanity_check_nolock(c,jeb);
	jffs2_dbg_acct_paranoia_check_nolock(c, jeb);
}

static struct jffs2_raw_node_ref **jffs2_incore_replace_raw(struct jffs2_sb_info *c,
							    struct jffs2_inode_info *f,
							    struct jffs2_raw_node_ref *raw,
							    union jffs2_node_union *node)
{
	struct jffs2_node_frag *frag;
	struct jffs2_full_dirent *fd;

	dbg_noderef("incore_replace_raw: node at %p is {%04x,%04x}\n",
		    node, je16_to_cpu(node->u.magic), je16_to_cpu(node->u.nodetype));

	BUG_ON(je16_to_cpu(node->u.magic) != 0x1985 &&
	       je16_to_cpu(node->u.magic) != 0);

	switch (je16_to_cpu(node->u.nodetype)) {
	case JFFS2_NODETYPE_INODE:
		if (f->metadata && f->metadata->raw == raw) {
			dbg_noderef("Will replace ->raw in f->metadata at %p\n", f->metadata);
			return &f->metadata->raw;
		}
		frag = jffs2_lookup_node_frag(&f->fragtree, je32_to_cpu(node->i.offset));
		BUG_ON(!frag);
		
		while (!frag->node || frag->node->raw != raw) {
			frag = frag_next(frag);
			BUG_ON(!frag);
		}
		dbg_noderef("Will replace ->raw in full_dnode at %p\n", frag->node);
		return &frag->node->raw;

	case JFFS2_NODETYPE_DIRENT:
		for (fd = f->dents; fd; fd = fd->next) {
			if (fd->raw == raw) {
				dbg_noderef("Will replace ->raw in full_dirent at %p\n", fd);
				return &fd->raw;
			}
		}
		BUG();

	default:
		dbg_noderef("Don't care about replacing raw for nodetype %x\n",
			    je16_to_cpu(node->u.nodetype));
		break;
	}
	return NULL;
}

#ifdef CONFIG_JFFS2_FS_WBUF_VERIFY
static int jffs2_verify_write(struct jffs2_sb_info *c, unsigned char *buf,
			      uint32_t ofs)
{
	int ret;
	size_t retlen;
	char *eccstr;

	ret = mtd_read(c->mtd, ofs, c->wbuf_pagesize, &retlen, c->wbuf_verify);
	if (ret && ret != -EUCLEAN && ret != -EBADMSG) {
		pr_warn("%s(): Read back of page at %08x failed: %d\n",
			__func__, c->wbuf_ofs, ret);
		return ret;
	} else if (retlen != c->wbuf_pagesize) {
		pr_warn("%s(): Read back of page at %08x gave short read: %zd not %d\n",
			__func__, ofs, retlen, c->wbuf_pagesize);
		return -EIO;
	}
	if (!memcmp(buf, c->wbuf_verify, c->wbuf_pagesize))
		return 0;

	if (ret == -EUCLEAN)
		eccstr = "corrected";
	else if (ret == -EBADMSG)
		eccstr = "correction failed";
	else
		eccstr = "OK or unused";

	pr_warn("Write verify error (ECC %s) at %08x. Wrote:\n",
		eccstr, c->wbuf_ofs);
	print_hex_dump(KERN_WARNING, "", DUMP_PREFIX_OFFSET, 16, 1,
		       c->wbuf, c->wbuf_pagesize, 0);

	pr_warn("Read back:\n");
	print_hex_dump(KERN_WARNING, "", DUMP_PREFIX_OFFSET, 16, 1,
		       c->wbuf_verify, c->wbuf_pagesize, 0);

	return -EIO;
}
#else
#define jffs2_verify_write(c,b,o) (0)
#endif


static void jffs2_wbuf_recover(struct jffs2_sb_info *c)
{
	struct jffs2_eraseblock *jeb, *new_jeb;
	struct jffs2_raw_node_ref *raw, *next, *first_raw = NULL;
	size_t retlen;
	int ret;
	int nr_refile = 0;
	unsigned char *buf;
	uint32_t start, end, ofs, len;

	jeb = &c->blocks[c->wbuf_ofs / c->sector_size];

	spin_lock(&c->erase_completion_lock);
	if (c->wbuf_ofs % c->mtd->erasesize)
		jffs2_block_refile(c, jeb, REFILE_NOTEMPTY);
	else
		jffs2_block_refile(c, jeb, REFILE_ANYWAY);
	spin_unlock(&c->erase_completion_lock);

	BUG_ON(!ref_obsolete(jeb->last_node));

	for (next = raw = jeb->first_node; next; raw = next) {
		next = ref_next(raw);

		if (ref_obsolete(raw) || 
		    (next && ref_offset(next) <= c->wbuf_ofs)) {
			dbg_noderef("Skipping node at 0x%08x(%d)-0x%08x which is either before 0x%08x or obsolete\n",
				    ref_offset(raw), ref_flags(raw),
				    (ref_offset(raw) + ref_totlen(c, jeb, raw)),
				    c->wbuf_ofs);
			continue;
		}
		dbg_noderef("First node to be recovered is at 0x%08x(%d)-0x%08x\n",
			    ref_offset(raw), ref_flags(raw),
			    (ref_offset(raw) + ref_totlen(c, jeb, raw)));

		first_raw = raw;
		break;
	}

	if (!first_raw) {
		
		jffs2_dbg(1, "No non-obsolete nodes to be recovered. Just filing block bad\n");
		c->wbuf_len = 0;
		return;
	}

	start = ref_offset(first_raw);
	end = ref_offset(jeb->last_node);
	nr_refile = 1;

	
	while ((raw = ref_next(raw)) != jeb->last_node)
		nr_refile++;

	dbg_noderef("wbuf recover %08x-%08x (%d bytes in %d nodes)\n",
		    start, end, end - start, nr_refile);

	buf = NULL;
	if (start < c->wbuf_ofs) {
		/* First affected node was already partially written.
		 * Attempt to reread the old data into our buffer. */

		buf = kmalloc(end - start, GFP_KERNEL);
		if (!buf) {
			pr_crit("Malloc failure in wbuf recovery. Data loss ensues.\n");

			goto read_failed;
		}

		
		ret = mtd_read(c->mtd, start, c->wbuf_ofs - start, &retlen,
			       buf);

		
		if ((ret == -EUCLEAN || ret == -EBADMSG) &&
		    (retlen == c->wbuf_ofs - start))
			ret = 0;

		if (ret || retlen != c->wbuf_ofs - start) {
			pr_crit("Old data are already lost in wbuf recovery. Data loss ensues.\n");

			kfree(buf);
			buf = NULL;
		read_failed:
			first_raw = ref_next(first_raw);
			nr_refile--;
			while (first_raw && ref_obsolete(first_raw)) {
				first_raw = ref_next(first_raw);
				nr_refile--;
			}

			
			if (!first_raw) {
				c->wbuf_len = 0;
				return;
			}

			
			start = ref_offset(first_raw);
			dbg_noderef("wbuf now recover %08x-%08x (%d bytes in %d nodes)\n",
				    start, end, end - start, nr_refile);

		} else {
			
			memcpy(buf + (c->wbuf_ofs - start), c->wbuf, end - c->wbuf_ofs);
		}
	}

	
	ret = jffs2_reserve_space_gc(c, end-start, &len, JFFS2_SUMMARY_NOSUM_SIZE);
	if (ret) {
		pr_warn("Failed to allocate space for wbuf recovery. Data loss ensues.\n");
		kfree(buf);
		return;
	}

	
	jffs2_sum_disable_collecting(c->summary);

	ret = jffs2_prealloc_raw_node_refs(c, c->nextblock, nr_refile);
	if (ret) {
		pr_warn("Failed to allocate node refs for wbuf recovery. Data loss ensues.\n");
		kfree(buf);
		return;
	}

	ofs = write_ofs(c);

	if (end-start >= c->wbuf_pagesize) {
		unsigned char *rewrite_buf = buf?:c->wbuf;
		uint32_t towrite = (end-start) - ((end-start)%c->wbuf_pagesize);

		jffs2_dbg(1, "Write 0x%x bytes at 0x%08x in wbuf recover\n",
			  towrite, ofs);

#ifdef BREAKMEHEADER
		static int breakme;
		if (breakme++ == 20) {
			pr_notice("Faking write error at 0x%08x\n", ofs);
			breakme = 0;
			mtd_write(c->mtd, ofs, towrite, &retlen, brokenbuf);
			ret = -EIO;
		} else
#endif
			ret = mtd_write(c->mtd, ofs, towrite, &retlen,
					rewrite_buf);

		if (ret || retlen != towrite || jffs2_verify_write(c, rewrite_buf, ofs)) {
			
			pr_crit("Recovery of wbuf failed due to a second write error\n");
			kfree(buf);

			if (retlen)
				jffs2_add_physical_node_ref(c, ofs | REF_OBSOLETE, ref_totlen(c, jeb, first_raw), NULL);

			return;
		}
		pr_notice("Recovery of wbuf succeeded to %08x\n", ofs);

		c->wbuf_len = (end - start) - towrite;
		c->wbuf_ofs = ofs + towrite;
		memmove(c->wbuf, rewrite_buf + towrite, c->wbuf_len);
		
	} else {
		
		if (buf) {
			memcpy(c->wbuf, buf, end-start);
		} else {
			memmove(c->wbuf, c->wbuf + (start - c->wbuf_ofs), end - start);
		}
		c->wbuf_ofs = ofs;
		c->wbuf_len = end - start;
	}

	
	new_jeb = &c->blocks[ofs / c->sector_size];

	spin_lock(&c->erase_completion_lock);
	for (raw = first_raw; raw != jeb->last_node; raw = ref_next(raw)) {
		uint32_t rawlen = ref_totlen(c, jeb, raw);
		struct jffs2_inode_cache *ic;
		struct jffs2_raw_node_ref *new_ref;
		struct jffs2_raw_node_ref **adjust_ref = NULL;
		struct jffs2_inode_info *f = NULL;

		jffs2_dbg(1, "Refiling block of %08x at %08x(%d) to %08x\n",
			  rawlen, ref_offset(raw), ref_flags(raw), ofs);

		ic = jffs2_raw_ref_to_ic(raw);

		
		if (ic && ic->class == RAWNODE_CLASS_XATTR_DATUM) {
			struct jffs2_xattr_datum *xd = (void *)ic;
			BUG_ON(xd->node != raw);
			adjust_ref = &xd->node;
			raw->next_in_ino = NULL;
			ic = NULL;
		} else if (ic && ic->class == RAWNODE_CLASS_XATTR_REF) {
			struct jffs2_xattr_datum *xr = (void *)ic;
			BUG_ON(xr->node != raw);
			adjust_ref = &xr->node;
			raw->next_in_ino = NULL;
			ic = NULL;
		} else if (ic && ic->class == RAWNODE_CLASS_INODE_CACHE) {
			struct jffs2_raw_node_ref **p = &ic->nodes;

			
			while (*p && *p != (void *)ic) {
				if (*p == raw) {
					(*p) = (raw->next_in_ino);
					raw->next_in_ino = NULL;
					break;
				}
				p = &((*p)->next_in_ino);
			}

			if (ic->state == INO_STATE_PRESENT && !ref_obsolete(raw)) {
				f = jffs2_gc_fetch_inode(c, ic->ino, !ic->pino_nlink);
				if (IS_ERR(f)) {
					
					JFFS2_ERROR("Failed to iget() ino #%u, err %ld\n",
						    ic->ino, PTR_ERR(f));
					BUG();
				}
				adjust_ref = jffs2_incore_replace_raw(c, f, raw,
								      (void *)(buf?:c->wbuf) + (ref_offset(raw) - start));
			} else if (unlikely(ic->state != INO_STATE_PRESENT &&
					    ic->state != INO_STATE_CHECKEDABSENT &&
					    ic->state != INO_STATE_GC)) {
				JFFS2_ERROR("Inode #%u is in strange state %d!\n", ic->ino, ic->state);
				BUG();
			}
		}

		new_ref = jffs2_link_node_ref(c, new_jeb, ofs | ref_flags(raw), rawlen, ic);

		if (adjust_ref) {
			BUG_ON(*adjust_ref != raw);
			*adjust_ref = new_ref;
		}
		if (f)
			jffs2_gc_release_inode(c, f);

		if (!ref_obsolete(raw)) {
			jeb->dirty_size += rawlen;
			jeb->used_size  -= rawlen;
			c->dirty_size += rawlen;
			c->used_size -= rawlen;
			raw->flash_offset = ref_offset(raw) | REF_OBSOLETE;
			BUG_ON(raw->next_in_ino);
		}
		ofs += rawlen;
	}

	kfree(buf);

	
	if (first_raw == jeb->first_node) {
		jffs2_dbg(1, "Failing block at %08x is now empty. Moving to erase_pending_list\n",
			  jeb->offset);
		list_move(&jeb->list, &c->erase_pending_list);
		c->nr_erasing_blocks++;
		jffs2_garbage_collect_trigger(c);
	}

	jffs2_dbg_acct_sanity_check_nolock(c, jeb);
	jffs2_dbg_acct_paranoia_check_nolock(c, jeb);

	jffs2_dbg_acct_sanity_check_nolock(c, new_jeb);
	jffs2_dbg_acct_paranoia_check_nolock(c, new_jeb);

	spin_unlock(&c->erase_completion_lock);

	jffs2_dbg(1, "wbuf recovery completed OK. wbuf_ofs 0x%08x, len 0x%x\n",
		  c->wbuf_ofs, c->wbuf_len);

}

#define NOPAD		0
#define PAD_NOACCOUNT	1
#define PAD_ACCOUNTING	2

static int __jffs2_flush_wbuf(struct jffs2_sb_info *c, int pad)
{
	struct jffs2_eraseblock *wbuf_jeb;
	int ret;
	size_t retlen;

	if (!jffs2_is_writebuffered(c))
		return 0;

	if (!mutex_is_locked(&c->alloc_sem)) {
		pr_crit("jffs2_flush_wbuf() called with alloc_sem not locked!\n");
		BUG();
	}

	if (!c->wbuf_len)	
		return 0;

	wbuf_jeb = &c->blocks[c->wbuf_ofs / c->sector_size];
	if (jffs2_prealloc_raw_node_refs(c, wbuf_jeb, c->nextblock->allocated_refs + 1))
		return -ENOMEM;

	if (pad ) {
		c->wbuf_len = PAD(c->wbuf_len);

		memset(c->wbuf + c->wbuf_len, 0, c->wbuf_pagesize - c->wbuf_len);

		if ( c->wbuf_len + sizeof(struct jffs2_unknown_node) < c->wbuf_pagesize) {
			struct jffs2_unknown_node *padnode = (void *)(c->wbuf + c->wbuf_len);
			padnode->magic = cpu_to_je16(JFFS2_MAGIC_BITMASK);
			padnode->nodetype = cpu_to_je16(JFFS2_NODETYPE_PADDING);
			padnode->totlen = cpu_to_je32(c->wbuf_pagesize - c->wbuf_len);
			padnode->hdr_crc = cpu_to_je32(crc32(0, padnode, sizeof(*padnode)-4));
		}
	}

#ifdef BREAKME
	static int breakme;
	if (breakme++ == 20) {
		pr_notice("Faking write error at 0x%08x\n", c->wbuf_ofs);
		breakme = 0;
		mtd_write(c->mtd, c->wbuf_ofs, c->wbuf_pagesize, &retlen,
			  brokenbuf);
		ret = -EIO;
	} else
#endif

		ret = mtd_write(c->mtd, c->wbuf_ofs, c->wbuf_pagesize,
				&retlen, c->wbuf);

	if (ret) {
		pr_warn("jffs2_flush_wbuf(): Write failed with %d\n", ret);
		goto wfail;
	} else if (retlen != c->wbuf_pagesize) {
		pr_warn("jffs2_flush_wbuf(): Write was short: %zd instead of %d\n",
			retlen, c->wbuf_pagesize);
		ret = -EIO;
		goto wfail;
	} else if ((ret = jffs2_verify_write(c, c->wbuf, c->wbuf_ofs))) {
	wfail:
		jffs2_wbuf_recover(c);

		return ret;
	}

	
	if (pad) {
		uint32_t waste = c->wbuf_pagesize - c->wbuf_len;

		jffs2_dbg(1, "jffs2_flush_wbuf() adjusting free_size of %sblock at %08x\n",
			  (wbuf_jeb == c->nextblock) ? "next" : "",
			  wbuf_jeb->offset);

		if (wbuf_jeb->free_size < waste) {
			pr_crit("jffs2_flush_wbuf(): Accounting error. wbuf at 0x%08x has 0x%03x bytes, 0x%03x left.\n",
				c->wbuf_ofs, c->wbuf_len, waste);
			pr_crit("jffs2_flush_wbuf(): But free_size for block at 0x%08x is only 0x%08x\n",
				wbuf_jeb->offset, wbuf_jeb->free_size);
			BUG();
		}

		spin_lock(&c->erase_completion_lock);

		jffs2_link_node_ref(c, wbuf_jeb, (c->wbuf_ofs + c->wbuf_len) | REF_OBSOLETE, waste, NULL);
		
		wbuf_jeb->dirty_size -= waste;
		c->dirty_size -= waste;
		wbuf_jeb->wasted_size += waste;
		c->wasted_size += waste;
	} else
		spin_lock(&c->erase_completion_lock);

	
	jffs2_refile_wbuf_blocks(c);
	jffs2_clear_wbuf_ino_list(c);
	spin_unlock(&c->erase_completion_lock);

	memset(c->wbuf,0xff,c->wbuf_pagesize);
	
	c->wbuf_ofs += c->wbuf_pagesize;
	c->wbuf_len = 0;
	return 0;
}

int jffs2_flush_wbuf_gc(struct jffs2_sb_info *c, uint32_t ino)
{
	uint32_t old_wbuf_ofs;
	uint32_t old_wbuf_len;
	int ret = 0;

	jffs2_dbg(1, "jffs2_flush_wbuf_gc() called for ino #%u...\n", ino);

	if (!c->wbuf)
		return 0;

	mutex_lock(&c->alloc_sem);
	if (!jffs2_wbuf_pending_for_ino(c, ino)) {
		jffs2_dbg(1, "Ino #%d not pending in wbuf. Returning\n", ino);
		mutex_unlock(&c->alloc_sem);
		return 0;
	}

	old_wbuf_ofs = c->wbuf_ofs;
	old_wbuf_len = c->wbuf_len;

	if (c->unchecked_size) {
		
		jffs2_dbg(1, "%s(): padding. Not finished checking\n",
			  __func__);
		down_write(&c->wbuf_sem);
		ret = __jffs2_flush_wbuf(c, PAD_ACCOUNTING);
		if (ret)
			ret = __jffs2_flush_wbuf(c, PAD_ACCOUNTING);
		up_write(&c->wbuf_sem);
	} else while (old_wbuf_len &&
		      old_wbuf_ofs == c->wbuf_ofs) {

		mutex_unlock(&c->alloc_sem);

		jffs2_dbg(1, "%s(): calls gc pass\n", __func__);

		ret = jffs2_garbage_collect_pass(c);
		if (ret) {
			
			mutex_lock(&c->alloc_sem);
			down_write(&c->wbuf_sem);
			ret = __jffs2_flush_wbuf(c, PAD_ACCOUNTING);
			if (ret)
				ret = __jffs2_flush_wbuf(c, PAD_ACCOUNTING);
			up_write(&c->wbuf_sem);
			break;
		}
		mutex_lock(&c->alloc_sem);
	}

	jffs2_dbg(1, "%s(): ends...\n", __func__);

	mutex_unlock(&c->alloc_sem);
	return ret;
}

int jffs2_flush_wbuf_pad(struct jffs2_sb_info *c)
{
	int ret;

	if (!c->wbuf)
		return 0;

	down_write(&c->wbuf_sem);
	ret = __jffs2_flush_wbuf(c, PAD_NOACCOUNT);
	
	if (ret)
		ret = __jffs2_flush_wbuf(c, PAD_NOACCOUNT);
	up_write(&c->wbuf_sem);

	return ret;
}

static size_t jffs2_fill_wbuf(struct jffs2_sb_info *c, const uint8_t *buf,
			      size_t len)
{
	if (len && !c->wbuf_len && (len >= c->wbuf_pagesize))
		return 0;

	if (len > (c->wbuf_pagesize - c->wbuf_len))
		len = c->wbuf_pagesize - c->wbuf_len;
	memcpy(c->wbuf + c->wbuf_len, buf, len);
	c->wbuf_len += (uint32_t) len;
	return len;
}

int jffs2_flash_writev(struct jffs2_sb_info *c, const struct kvec *invecs,
		       unsigned long count, loff_t to, size_t *retlen,
		       uint32_t ino)
{
	struct jffs2_eraseblock *jeb;
	size_t wbuf_retlen, donelen = 0;
	uint32_t outvec_to = to;
	int ret, invec;

	
	if (!jffs2_is_writebuffered(c))
		return jffs2_flash_direct_writev(c, invecs, count, to, retlen);

	down_write(&c->wbuf_sem);

	
	if (c->wbuf_ofs == 0xFFFFFFFF) {
		c->wbuf_ofs = PAGE_DIV(to);
		c->wbuf_len = PAGE_MOD(to);
		memset(c->wbuf,0xff,c->wbuf_pagesize);
	}

	if (SECTOR_ADDR(to) != SECTOR_ADDR(c->wbuf_ofs)) {
		
		if (c->wbuf_len) {
			jffs2_dbg(1, "%s(): to 0x%lx causes flush of wbuf at 0x%08x\n",
				  __func__, (unsigned long)to, c->wbuf_ofs);
			ret = __jffs2_flush_wbuf(c, PAD_NOACCOUNT);
			if (ret)
				goto outerr;
		}
		
		c->wbuf_ofs = PAGE_DIV(to);
		c->wbuf_len = PAGE_MOD(to);
	}

	if (to != PAD(c->wbuf_ofs + c->wbuf_len)) {
		
		pr_crit("%s(): Non-contiguous write to %08lx\n",
			__func__, (unsigned long)to);
		if (c->wbuf_len)
			pr_crit("wbuf was previously %08x-%08x\n",
				c->wbuf_ofs, c->wbuf_ofs + c->wbuf_len);
		BUG();
	}

	
	if (c->wbuf_len != PAGE_MOD(to)) {
		c->wbuf_len = PAGE_MOD(to);
		
		if (!c->wbuf_len) {
			c->wbuf_len = c->wbuf_pagesize;
			ret = __jffs2_flush_wbuf(c, NOPAD);
			if (ret)
				goto outerr;
		}
	}

	for (invec = 0; invec < count; invec++) {
		int vlen = invecs[invec].iov_len;
		uint8_t *v = invecs[invec].iov_base;

		wbuf_retlen = jffs2_fill_wbuf(c, v, vlen);

		if (c->wbuf_len == c->wbuf_pagesize) {
			ret = __jffs2_flush_wbuf(c, NOPAD);
			if (ret)
				goto outerr;
		}
		vlen -= wbuf_retlen;
		outvec_to += wbuf_retlen;
		donelen += wbuf_retlen;
		v += wbuf_retlen;

		if (vlen >= c->wbuf_pagesize) {
			ret = mtd_write(c->mtd, outvec_to, PAGE_DIV(vlen),
					&wbuf_retlen, v);
			if (ret < 0 || wbuf_retlen != PAGE_DIV(vlen))
				goto outfile;

			vlen -= wbuf_retlen;
			outvec_to += wbuf_retlen;
			c->wbuf_ofs = outvec_to;
			donelen += wbuf_retlen;
			v += wbuf_retlen;
		}

		wbuf_retlen = jffs2_fill_wbuf(c, v, vlen);
		if (c->wbuf_len == c->wbuf_pagesize) {
			ret = __jffs2_flush_wbuf(c, NOPAD);
			if (ret)
				goto outerr;
		}

		outvec_to += wbuf_retlen;
		donelen += wbuf_retlen;
	}

	*retlen = donelen;

	if (jffs2_sum_active()) {
		int res = jffs2_sum_add_kvec(c, invecs, count, (uint32_t) to);
		if (res)
			return res;
	}

	if (c->wbuf_len && ino)
		jffs2_wbuf_dirties_inode(c, ino);

	ret = 0;
	up_write(&c->wbuf_sem);
	return ret;

outfile:

	spin_lock(&c->erase_completion_lock);

	jeb = &c->blocks[outvec_to / c->sector_size];
	jffs2_block_refile(c, jeb, REFILE_ANYWAY);

	spin_unlock(&c->erase_completion_lock);

outerr:
	*retlen = 0;
	up_write(&c->wbuf_sem);
	return ret;
}

int jffs2_flash_write(struct jffs2_sb_info *c, loff_t ofs, size_t len,
		      size_t *retlen, const u_char *buf)
{
	struct kvec vecs[1];

	if (!jffs2_is_writebuffered(c))
		return jffs2_flash_direct_write(c, ofs, len, retlen, buf);

	vecs[0].iov_base = (unsigned char *) buf;
	vecs[0].iov_len = len;
	return jffs2_flash_writev(c, vecs, 1, ofs, retlen, 0);
}

int jffs2_flash_read(struct jffs2_sb_info *c, loff_t ofs, size_t len, size_t *retlen, u_char *buf)
{
	loff_t	orbf = 0, owbf = 0, lwbf = 0;
	int	ret;

	if (!jffs2_is_writebuffered(c))
		return mtd_read(c->mtd, ofs, len, retlen, buf);

	
	down_read(&c->wbuf_sem);
	ret = mtd_read(c->mtd, ofs, len, retlen, buf);

	if ( (ret == -EBADMSG || ret == -EUCLEAN) && (*retlen == len) ) {
		if (ret == -EBADMSG)
			pr_warn("mtd->read(0x%zx bytes from 0x%llx) returned ECC error\n",
				len, ofs);
		ret = 0;
	}

	
	if (!c->wbuf_pagesize || !c->wbuf_len)
		goto exit;

	
	if (SECTOR_ADDR(ofs) != SECTOR_ADDR(c->wbuf_ofs))
		goto exit;

	if (ofs >= c->wbuf_ofs) {
		owbf = (ofs - c->wbuf_ofs);	
		if (owbf > c->wbuf_len)		
			goto exit;
		lwbf = c->wbuf_len - owbf;	
		if (lwbf > len)
			lwbf = len;
	} else {
		orbf = (c->wbuf_ofs - ofs);	
		if (orbf > len)			
			goto exit;
		lwbf = len - orbf;		
		if (lwbf > c->wbuf_len)
			lwbf = c->wbuf_len;
	}
	if (lwbf > 0)
		memcpy(buf+orbf,c->wbuf+owbf,lwbf);

exit:
	up_read(&c->wbuf_sem);
	return ret;
}

#define NR_OOB_SCAN_PAGES 4

#define OOB_CM_SIZE 8

static const struct jffs2_unknown_node oob_cleanmarker =
{
	.magic = constant_cpu_to_je16(JFFS2_MAGIC_BITMASK),
	.nodetype = constant_cpu_to_je16(JFFS2_NODETYPE_CLEANMARKER),
	.totlen = constant_cpu_to_je32(8)
};

int jffs2_check_oob_empty(struct jffs2_sb_info *c,
			  struct jffs2_eraseblock *jeb, int mode)
{
	int i, ret;
	int cmlen = min_t(int, c->oobavail, OOB_CM_SIZE);
	struct mtd_oob_ops ops;

	ops.mode = MTD_OPS_AUTO_OOB;
	ops.ooblen = NR_OOB_SCAN_PAGES * c->oobavail;
	ops.oobbuf = c->oobbuf;
	ops.len = ops.ooboffs = ops.retlen = ops.oobretlen = 0;
	ops.datbuf = NULL;

	ret = mtd_read_oob(c->mtd, jeb->offset, &ops);
	if (ret || ops.oobretlen != ops.ooblen) {
		pr_err("cannot read OOB for EB at %08x, requested %zd bytes, read %zd bytes, error %d\n",
		       jeb->offset, ops.ooblen, ops.oobretlen, ret);
		if (!ret)
			ret = -EIO;
		return ret;
	}

	for(i = 0; i < ops.ooblen; i++) {
		if (mode && i < cmlen)
			
			continue;

		if (ops.oobbuf[i] != 0xFF) {
			jffs2_dbg(2, "Found %02x at %x in OOB for "
				  "%08x\n", ops.oobbuf[i], i, jeb->offset);
			return 1;
		}
	}

	return 0;
}

int jffs2_check_nand_cleanmarker(struct jffs2_sb_info *c,
				 struct jffs2_eraseblock *jeb)
{
	struct mtd_oob_ops ops;
	int ret, cmlen = min_t(int, c->oobavail, OOB_CM_SIZE);

	ops.mode = MTD_OPS_AUTO_OOB;
	ops.ooblen = cmlen;
	ops.oobbuf = c->oobbuf;
	ops.len = ops.ooboffs = ops.retlen = ops.oobretlen = 0;
	ops.datbuf = NULL;

	ret = mtd_read_oob(c->mtd, jeb->offset, &ops);
	if (ret || ops.oobretlen != ops.ooblen) {
		pr_err("cannot read OOB for EB at %08x, requested %zd bytes, read %zd bytes, error %d\n",
		       jeb->offset, ops.ooblen, ops.oobretlen, ret);
		if (!ret)
			ret = -EIO;
		return ret;
	}

	return !!memcmp(&oob_cleanmarker, c->oobbuf, cmlen);
}

int jffs2_write_nand_cleanmarker(struct jffs2_sb_info *c,
				 struct jffs2_eraseblock *jeb)
{
	int ret;
	struct mtd_oob_ops ops;
	int cmlen = min_t(int, c->oobavail, OOB_CM_SIZE);

	ops.mode = MTD_OPS_AUTO_OOB;
	ops.ooblen = cmlen;
	ops.oobbuf = (uint8_t *)&oob_cleanmarker;
	ops.len = ops.ooboffs = ops.retlen = ops.oobretlen = 0;
	ops.datbuf = NULL;

	ret = mtd_write_oob(c->mtd, jeb->offset, &ops);
	if (ret || ops.oobretlen != ops.ooblen) {
		pr_err("cannot write OOB for EB at %08x, requested %zd bytes, read %zd bytes, error %d\n",
		       jeb->offset, ops.ooblen, ops.oobretlen, ret);
		if (!ret)
			ret = -EIO;
		return ret;
	}

	return 0;
}


int jffs2_write_nand_badblock(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb, uint32_t bad_offset)
{
	int 	ret;

	
	if( ++jeb->bad_count < MAX_ERASE_FAILURES)
		return 0;

	pr_warn("marking eraseblock at %08x as bad\n", bad_offset);
	ret = mtd_block_markbad(c->mtd, bad_offset);

	if (ret) {
		jffs2_dbg(1, "%s(): Write failed for block at %08x: error %d\n",
			  __func__, jeb->offset, ret);
		return ret;
	}
	return 1;
}

int jffs2_nand_flash_setup(struct jffs2_sb_info *c)
{
	struct nand_ecclayout *oinfo = c->mtd->ecclayout;

	if (!c->mtd->oobsize)
		return 0;

	
	c->cleanmarker_size = 0;

	if (!oinfo || oinfo->oobavail == 0) {
		pr_err("inconsistent device description\n");
		return -EINVAL;
	}

	jffs2_dbg(1, "using OOB on NAND\n");

	c->oobavail = oinfo->oobavail;

	
	init_rwsem(&c->wbuf_sem);
	c->wbuf_pagesize = c->mtd->writesize;
	c->wbuf_ofs = 0xFFFFFFFF;

	c->wbuf = kmalloc(c->wbuf_pagesize, GFP_KERNEL);
	if (!c->wbuf)
		return -ENOMEM;

	c->oobbuf = kmalloc(NR_OOB_SCAN_PAGES * c->oobavail, GFP_KERNEL);
	if (!c->oobbuf) {
		kfree(c->wbuf);
		return -ENOMEM;
	}

#ifdef CONFIG_JFFS2_FS_WBUF_VERIFY
	c->wbuf_verify = kmalloc(c->wbuf_pagesize, GFP_KERNEL);
	if (!c->wbuf_verify) {
		kfree(c->oobbuf);
		kfree(c->wbuf);
		return -ENOMEM;
	}
#endif
	return 0;
}

void jffs2_nand_flash_cleanup(struct jffs2_sb_info *c)
{
#ifdef CONFIG_JFFS2_FS_WBUF_VERIFY
	kfree(c->wbuf_verify);
#endif
	kfree(c->wbuf);
	kfree(c->oobbuf);
}

int jffs2_dataflash_setup(struct jffs2_sb_info *c) {
	c->cleanmarker_size = 0;		

	
	init_rwsem(&c->wbuf_sem);


	c->wbuf_pagesize =  c->mtd->erasesize;


	c->sector_size = 8 * c->mtd->erasesize;

	while (c->sector_size < 8192) {
		c->sector_size *= 2;
	}

	
	c->flash_size = c->mtd->size;

	if ((c->flash_size % c->sector_size) != 0) {
		c->flash_size = (c->flash_size / c->sector_size) * c->sector_size;
		pr_warn("flash size adjusted to %dKiB\n", c->flash_size);
	};

	c->wbuf_ofs = 0xFFFFFFFF;
	c->wbuf = kmalloc(c->wbuf_pagesize, GFP_KERNEL);
	if (!c->wbuf)
		return -ENOMEM;

#ifdef CONFIG_JFFS2_FS_WBUF_VERIFY
	c->wbuf_verify = kmalloc(c->wbuf_pagesize, GFP_KERNEL);
	if (!c->wbuf_verify) {
		kfree(c->oobbuf);
		kfree(c->wbuf);
		return -ENOMEM;
	}
#endif

	pr_info("write-buffering enabled buffer (%d) erasesize (%d)\n",
		c->wbuf_pagesize, c->sector_size);

	return 0;
}

void jffs2_dataflash_cleanup(struct jffs2_sb_info *c) {
#ifdef CONFIG_JFFS2_FS_WBUF_VERIFY
	kfree(c->wbuf_verify);
#endif
	kfree(c->wbuf);
}

int jffs2_nor_wbuf_flash_setup(struct jffs2_sb_info *c) {
	c->cleanmarker_size = max(16u, c->mtd->writesize);

	
	init_rwsem(&c->wbuf_sem);
	c->wbuf_pagesize = c->mtd->writesize;
	c->wbuf_ofs = 0xFFFFFFFF;

	c->wbuf = kmalloc(c->wbuf_pagesize, GFP_KERNEL);
	if (!c->wbuf)
		return -ENOMEM;

#ifdef CONFIG_JFFS2_FS_WBUF_VERIFY
	c->wbuf_verify = kmalloc(c->wbuf_pagesize, GFP_KERNEL);
	if (!c->wbuf_verify) {
		kfree(c->wbuf);
		return -ENOMEM;
	}
#endif
	return 0;
}

void jffs2_nor_wbuf_flash_cleanup(struct jffs2_sb_info *c) {
#ifdef CONFIG_JFFS2_FS_WBUF_VERIFY
	kfree(c->wbuf_verify);
#endif
	kfree(c->wbuf);
}

int jffs2_ubivol_setup(struct jffs2_sb_info *c) {
	c->cleanmarker_size = 0;

	if (c->mtd->writesize == 1)
		
		return 0;

	init_rwsem(&c->wbuf_sem);

	c->wbuf_pagesize =  c->mtd->writesize;
	c->wbuf_ofs = 0xFFFFFFFF;
	c->wbuf = kmalloc(c->wbuf_pagesize, GFP_KERNEL);
	if (!c->wbuf)
		return -ENOMEM;

	pr_info("write-buffering enabled buffer (%d) erasesize (%d)\n",
		c->wbuf_pagesize, c->sector_size);

	return 0;
}

void jffs2_ubivol_cleanup(struct jffs2_sb_info *c) {
	kfree(c->wbuf);
}
