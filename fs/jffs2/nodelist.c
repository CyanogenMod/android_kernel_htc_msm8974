/*
 * JFFS2 -- Journalling Flash File System, Version 2.
 *
 * Copyright © 2001-2007 Red Hat, Inc.
 *
 * Created by David Woodhouse <dwmw2@infradead.org>
 *
 * For licensing information, see the file 'LICENCE' in this directory.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/mtd/mtd.h>
#include <linux/rbtree.h>
#include <linux/crc32.h>
#include <linux/pagemap.h>
#include "nodelist.h"

static void jffs2_obsolete_node_frag(struct jffs2_sb_info *c,
				     struct jffs2_node_frag *this);

void jffs2_add_fd_to_list(struct jffs2_sb_info *c, struct jffs2_full_dirent *new, struct jffs2_full_dirent **list)
{
	struct jffs2_full_dirent **prev = list;

	dbg_dentlist("add dirent \"%s\", ino #%u\n", new->name, new->ino);

	while ((*prev) && (*prev)->nhash <= new->nhash) {
		if ((*prev)->nhash == new->nhash && !strcmp((*prev)->name, new->name)) {
			
			if (new->version < (*prev)->version) {
				dbg_dentlist("Eep! Marking new dirent node obsolete, old is \"%s\", ino #%u\n",
					(*prev)->name, (*prev)->ino);
				jffs2_mark_node_obsolete(c, new->raw);
				jffs2_free_full_dirent(new);
			} else {
				dbg_dentlist("marking old dirent \"%s\", ino #%u obsolete\n",
					(*prev)->name, (*prev)->ino);
				new->next = (*prev)->next;
				if ((*prev)->raw)
					jffs2_mark_node_obsolete(c, ((*prev)->raw));
				jffs2_free_full_dirent(*prev);
				*prev = new;
			}
			return;
		}
		prev = &((*prev)->next);
	}
	new->next = *prev;
	*prev = new;
}

uint32_t jffs2_truncate_fragtree(struct jffs2_sb_info *c, struct rb_root *list, uint32_t size)
{
	struct jffs2_node_frag *frag = jffs2_lookup_node_frag(list, size);

	dbg_fragtree("truncating fragtree to 0x%08x bytes\n", size);

	
	if (frag && frag->ofs != size) {
		if (frag->ofs+frag->size > size) {
			frag->size = size - frag->ofs;
		}
		frag = frag_next(frag);
	}
	while (frag && frag->ofs >= size) {
		struct jffs2_node_frag *next = frag_next(frag);

		frag_erase(frag, list);
		jffs2_obsolete_node_frag(c, frag);
		frag = next;
	}

	if (size == 0)
		return 0;

	frag = frag_last(list);

	
	if (!frag)
		return 0;
	if (frag->ofs + frag->size < size)
		return frag->ofs + frag->size;

	if (frag->node && (frag->ofs & (PAGE_CACHE_SIZE - 1)) == 0) {
		dbg_fragtree2("marking the last fragment 0x%08x-0x%08x REF_PRISTINE.\n",
			frag->ofs, frag->ofs + frag->size);
		frag->node->raw->flash_offset = ref_offset(frag->node->raw) | REF_PRISTINE;
	}
	return size;
}

static void jffs2_obsolete_node_frag(struct jffs2_sb_info *c,
				     struct jffs2_node_frag *this)
{
	if (this->node) {
		this->node->frags--;
		if (!this->node->frags) {
			
			dbg_fragtree2("marking old node @0x%08x (0x%04x-0x%04x) obsolete\n",
				ref_offset(this->node->raw), this->node->ofs, this->node->ofs+this->node->size);
			jffs2_mark_node_obsolete(c, this->node->raw);
			jffs2_free_full_dnode(this->node);
		} else {
			dbg_fragtree2("marking old node @0x%08x (0x%04x-0x%04x) REF_NORMAL. frags is %d\n",
				ref_offset(this->node->raw), this->node->ofs, this->node->ofs+this->node->size, this->node->frags);
			mark_ref_normal(this->node->raw);
		}

	}
	jffs2_free_node_frag(this);
}

static void jffs2_fragtree_insert(struct jffs2_node_frag *newfrag, struct jffs2_node_frag *base)
{
	struct rb_node *parent = &base->rb;
	struct rb_node **link = &parent;

	dbg_fragtree2("insert frag (0x%04x-0x%04x)\n", newfrag->ofs, newfrag->ofs + newfrag->size);

	while (*link) {
		parent = *link;
		base = rb_entry(parent, struct jffs2_node_frag, rb);

		if (newfrag->ofs > base->ofs)
			link = &base->rb.rb_right;
		else if (newfrag->ofs < base->ofs)
			link = &base->rb.rb_left;
		else {
			JFFS2_ERROR("duplicate frag at %08x (%p,%p)\n", newfrag->ofs, newfrag, base);
			BUG();
		}
	}

	rb_link_node(&newfrag->rb, &base->rb, link);
}

static struct jffs2_node_frag * new_fragment(struct jffs2_full_dnode *fn, uint32_t ofs, uint32_t size)
{
	struct jffs2_node_frag *newfrag;

	newfrag = jffs2_alloc_node_frag();
	if (likely(newfrag)) {
		newfrag->ofs = ofs;
		newfrag->size = size;
		newfrag->node = fn;
	} else {
		JFFS2_ERROR("cannot allocate a jffs2_node_frag object\n");
	}

	return newfrag;
}

static int no_overlapping_node(struct jffs2_sb_info *c, struct rb_root *root,
		 	       struct jffs2_node_frag *newfrag,
			       struct jffs2_node_frag *this, uint32_t lastend)
{
	if (lastend < newfrag->node->ofs) {
		
		struct jffs2_node_frag *holefrag;

		holefrag= new_fragment(NULL, lastend, newfrag->node->ofs - lastend);
		if (unlikely(!holefrag)) {
			jffs2_free_node_frag(newfrag);
			return -ENOMEM;
		}

		if (this) {
			dbg_fragtree2("add hole frag %#04x-%#04x on the right of the new frag.\n",
				holefrag->ofs, holefrag->ofs + holefrag->size);
			rb_link_node(&holefrag->rb, &this->rb, &this->rb.rb_right);
		} else {
			dbg_fragtree2("Add hole frag %#04x-%#04x to the root of the tree.\n",
				holefrag->ofs, holefrag->ofs + holefrag->size);
			rb_link_node(&holefrag->rb, NULL, &root->rb_node);
		}
		rb_insert_color(&holefrag->rb, root);
		this = holefrag;
	}

	if (this) {
		dbg_fragtree2("add the new node at the right\n");
		rb_link_node(&newfrag->rb, &this->rb, &this->rb.rb_right);
	} else {
		dbg_fragtree2("insert the new node at the root of the tree\n");
		rb_link_node(&newfrag->rb, NULL, &root->rb_node);
	}
	rb_insert_color(&newfrag->rb, root);

	return 0;
}

static int jffs2_add_frag_to_fragtree(struct jffs2_sb_info *c, struct rb_root *root, struct jffs2_node_frag *newfrag)
{
	struct jffs2_node_frag *this;
	uint32_t lastend;

	
	this = jffs2_lookup_node_frag(root, newfrag->node->ofs);

	if (this) {
		dbg_fragtree2("lookup gave frag 0x%04x-0x%04x; phys 0x%08x (*%p)\n",
			  this->ofs, this->ofs+this->size, this->node?(ref_offset(this->node->raw)):0xffffffff, this);
		lastend = this->ofs + this->size;
	} else {
		dbg_fragtree2("lookup gave no frag\n");
		lastend = 0;
	}

	
	if (lastend <= newfrag->ofs) {
		

		if (lastend && (lastend-1) >> PAGE_CACHE_SHIFT == newfrag->ofs >> PAGE_CACHE_SHIFT) {
			if (this->node)
				mark_ref_normal(this->node->raw);
			mark_ref_normal(newfrag->node->raw);
		}

		return no_overlapping_node(c, root, newfrag, this, lastend);
	}

	if (this->node)
		dbg_fragtree2("dealing with frag %u-%u, phys %#08x(%d).\n",
		this->ofs, this->ofs + this->size,
		ref_offset(this->node->raw), ref_flags(this->node->raw));
	else
		dbg_fragtree2("dealing with hole frag %u-%u.\n",
		this->ofs, this->ofs + this->size);

	if (newfrag->ofs > this->ofs) {
		

		mark_ref_normal(newfrag->node->raw);
		if (this->node)
			mark_ref_normal(this->node->raw);

		if (this->ofs + this->size > newfrag->ofs + newfrag->size) {
			
			struct jffs2_node_frag *newfrag2;

			if (this->node)
				dbg_fragtree2("split old frag 0x%04x-0x%04x, phys 0x%08x\n",
					this->ofs, this->ofs+this->size, ref_offset(this->node->raw));
			else
				dbg_fragtree2("split old hole frag 0x%04x-0x%04x\n",
					this->ofs, this->ofs+this->size);

			
			newfrag2 = new_fragment(this->node, newfrag->ofs + newfrag->size,
						this->ofs + this->size - newfrag->ofs - newfrag->size);
			if (unlikely(!newfrag2))
				return -ENOMEM;
			if (this->node)
				this->node->frags++;

			
			this->size = newfrag->ofs - this->ofs;

			jffs2_fragtree_insert(newfrag, this);
			rb_insert_color(&newfrag->rb, root);

			jffs2_fragtree_insert(newfrag2, newfrag);
			rb_insert_color(&newfrag2->rb, root);

			return 0;
		}
		
		this->size = newfrag->ofs - this->ofs;

		
		jffs2_fragtree_insert(newfrag, this);
		rb_insert_color(&newfrag->rb, root);
	} else {
		dbg_fragtree2("inserting newfrag (*%p),%d-%d in before 'this' (*%p),%d-%d\n",
			  newfrag, newfrag->ofs, newfrag->ofs+newfrag->size, this, this->ofs, this->ofs+this->size);

		rb_replace_node(&this->rb, &newfrag->rb, root);

		if (newfrag->ofs + newfrag->size >= this->ofs+this->size) {
			dbg_fragtree2("obsoleting node frag %p (%x-%x)\n", this, this->ofs, this->ofs+this->size);
			jffs2_obsolete_node_frag(c, this);
		} else {
			this->ofs += newfrag->size;
			this->size -= newfrag->size;

			jffs2_fragtree_insert(this, newfrag);
			rb_insert_color(&this->rb, root);
			return 0;
		}
	}
	while ((this = frag_next(newfrag)) && newfrag->ofs + newfrag->size >= this->ofs + this->size) {
		
		dbg_fragtree2("obsoleting node frag %p (%x-%x) and removing from tree\n",
			this, this->ofs, this->ofs+this->size);
		rb_erase(&this->rb, root);
		jffs2_obsolete_node_frag(c, this);
	}

	if (!this || newfrag->ofs + newfrag->size == this->ofs)
		return 0;

	
	this->size = (this->ofs + this->size) - (newfrag->ofs + newfrag->size);
	this->ofs = newfrag->ofs + newfrag->size;

	
	if (this->node)
		mark_ref_normal(this->node->raw);
	mark_ref_normal(newfrag->node->raw);

	return 0;
}

int jffs2_add_full_dnode_to_inode(struct jffs2_sb_info *c, struct jffs2_inode_info *f, struct jffs2_full_dnode *fn)
{
	int ret;
	struct jffs2_node_frag *newfrag;

	if (unlikely(!fn->size))
		return 0;

	newfrag = new_fragment(fn, fn->ofs, fn->size);
	if (unlikely(!newfrag))
		return -ENOMEM;
	newfrag->node->frags = 1;

	dbg_fragtree("adding node %#04x-%#04x @0x%08x on flash, newfrag *%p\n",
		  fn->ofs, fn->ofs+fn->size, ref_offset(fn->raw), newfrag);

	ret = jffs2_add_frag_to_fragtree(c, &f->fragtree, newfrag);
	if (unlikely(ret))
		return ret;

	if (newfrag->ofs & (PAGE_CACHE_SIZE-1)) {
		struct jffs2_node_frag *prev = frag_prev(newfrag);

		mark_ref_normal(fn->raw);
		
		if (prev->node)
			mark_ref_normal(prev->node->raw);
	}

	if ((newfrag->ofs+newfrag->size) & (PAGE_CACHE_SIZE-1)) {
		struct jffs2_node_frag *next = frag_next(newfrag);

		if (next) {
			mark_ref_normal(fn->raw);
			if (next->node)
				mark_ref_normal(next->node->raw);
		}
	}
	jffs2_dbg_fragtree_paranoia_check_nolock(f);

	return 0;
}

void jffs2_set_inocache_state(struct jffs2_sb_info *c, struct jffs2_inode_cache *ic, int state)
{
	spin_lock(&c->inocache_lock);
	ic->state = state;
	wake_up(&c->inocache_wq);
	spin_unlock(&c->inocache_lock);
}


struct jffs2_inode_cache *jffs2_get_ino_cache(struct jffs2_sb_info *c, uint32_t ino)
{
	struct jffs2_inode_cache *ret;

	ret = c->inocache_list[ino % c->inocache_hashsize];
	while (ret && ret->ino < ino) {
		ret = ret->next;
	}

	if (ret && ret->ino != ino)
		ret = NULL;

	return ret;
}

void jffs2_add_ino_cache (struct jffs2_sb_info *c, struct jffs2_inode_cache *new)
{
	struct jffs2_inode_cache **prev;

	spin_lock(&c->inocache_lock);
	if (!new->ino)
		new->ino = ++c->highest_ino;

	dbg_inocache("add %p (ino #%u)\n", new, new->ino);

	prev = &c->inocache_list[new->ino % c->inocache_hashsize];

	while ((*prev) && (*prev)->ino < new->ino) {
		prev = &(*prev)->next;
	}
	new->next = *prev;
	*prev = new;

	spin_unlock(&c->inocache_lock);
}

void jffs2_del_ino_cache(struct jffs2_sb_info *c, struct jffs2_inode_cache *old)
{
	struct jffs2_inode_cache **prev;

#ifdef CONFIG_JFFS2_FS_XATTR
	BUG_ON(old->xref);
#endif
	dbg_inocache("del %p (ino #%u)\n", old, old->ino);
	spin_lock(&c->inocache_lock);

	prev = &c->inocache_list[old->ino % c->inocache_hashsize];

	while ((*prev) && (*prev)->ino < old->ino) {
		prev = &(*prev)->next;
	}
	if ((*prev) == old) {
		*prev = old->next;
	}

	if (old->state != INO_STATE_READING && old->state != INO_STATE_CLEARING)
		jffs2_free_inode_cache(old);

	spin_unlock(&c->inocache_lock);
}

void jffs2_free_ino_caches(struct jffs2_sb_info *c)
{
	int i;
	struct jffs2_inode_cache *this, *next;

	for (i=0; i < c->inocache_hashsize; i++) {
		this = c->inocache_list[i];
		while (this) {
			next = this->next;
			jffs2_xattr_free_inode(c, this);
			jffs2_free_inode_cache(this);
			this = next;
		}
		c->inocache_list[i] = NULL;
	}
}

void jffs2_free_raw_node_refs(struct jffs2_sb_info *c)
{
	int i;
	struct jffs2_raw_node_ref *this, *next;

	for (i=0; i<c->nr_blocks; i++) {
		this = c->blocks[i].first_node;
		while (this) {
			if (this[REFS_PER_BLOCK].flash_offset == REF_LINK_NODE)
				next = this[REFS_PER_BLOCK].next_in_ino;
			else
				next = NULL;

			jffs2_free_refblock(this);
			this = next;
		}
		c->blocks[i].first_node = c->blocks[i].last_node = NULL;
	}
}

struct jffs2_node_frag *jffs2_lookup_node_frag(struct rb_root *fragtree, uint32_t offset)
{
	struct rb_node *next;
	struct jffs2_node_frag *prev = NULL;
	struct jffs2_node_frag *frag = NULL;

	dbg_fragtree2("root %p, offset %d\n", fragtree, offset);

	next = fragtree->rb_node;

	while(next) {
		frag = rb_entry(next, struct jffs2_node_frag, rb);

		if (frag->ofs + frag->size <= offset) {
			
			if (!prev || frag->ofs > prev->ofs)
				prev = frag;
			next = frag->rb.rb_right;
		} else if (frag->ofs > offset) {
			next = frag->rb.rb_left;
		} else {
			return frag;
		}
	}


	if (prev)
		dbg_fragtree2("no match. Returning frag %#04x-%#04x, closest previous\n",
			  prev->ofs, prev->ofs+prev->size);
	else
		dbg_fragtree2("returning NULL, empty fragtree\n");

	return prev;
}

void jffs2_kill_fragtree(struct rb_root *root, struct jffs2_sb_info *c)
{
	struct jffs2_node_frag *frag;
	struct jffs2_node_frag *parent;

	if (!root->rb_node)
		return;

	dbg_fragtree("killing\n");

	frag = (rb_entry(root->rb_node, struct jffs2_node_frag, rb));
	while(frag) {
		if (frag->rb.rb_left) {
			frag = frag_left(frag);
			continue;
		}
		if (frag->rb.rb_right) {
			frag = frag_right(frag);
			continue;
		}

		if (frag->node && !(--frag->node->frags)) {
			if (c)
				jffs2_mark_node_obsolete(c, frag->node->raw);

			jffs2_free_full_dnode(frag->node);
		}
		parent = frag_parent(frag);
		if (parent) {
			if (frag_left(parent) == frag)
				parent->rb.rb_left = NULL;
			else
				parent->rb.rb_right = NULL;
		}

		jffs2_free_node_frag(frag);
		frag = parent;

		cond_resched();
	}
}

struct jffs2_raw_node_ref *jffs2_link_node_ref(struct jffs2_sb_info *c,
					       struct jffs2_eraseblock *jeb,
					       uint32_t ofs, uint32_t len,
					       struct jffs2_inode_cache *ic)
{
	struct jffs2_raw_node_ref *ref;

	BUG_ON(!jeb->allocated_refs);
	jeb->allocated_refs--;

	ref = jeb->last_node;

	dbg_noderef("Last node at %p is (%08x,%p)\n", ref, ref->flash_offset,
		    ref->next_in_ino);

	while (ref->flash_offset != REF_EMPTY_NODE) {
		if (ref->flash_offset == REF_LINK_NODE)
			ref = ref->next_in_ino;
		else
			ref++;
	}

	dbg_noderef("New ref is %p (%08x becomes %08x,%p) len 0x%x\n", ref, 
		    ref->flash_offset, ofs, ref->next_in_ino, len);

	ref->flash_offset = ofs;

	if (!jeb->first_node) {
		jeb->first_node = ref;
		BUG_ON(ref_offset(ref) != jeb->offset);
	} else if (unlikely(ref_offset(ref) != jeb->offset + c->sector_size - jeb->free_size)) {
		uint32_t last_len = ref_totlen(c, jeb, jeb->last_node);

		JFFS2_ERROR("Adding new ref %p at (0x%08x-0x%08x) not immediately after previous (0x%08x-0x%08x)\n",
			    ref, ref_offset(ref), ref_offset(ref)+len,
			    ref_offset(jeb->last_node), 
			    ref_offset(jeb->last_node)+last_len);
		BUG();
	}
	jeb->last_node = ref;

	if (ic) {
		ref->next_in_ino = ic->nodes;
		ic->nodes = ref;
	} else {
		ref->next_in_ino = NULL;
	}

	switch(ref_flags(ref)) {
	case REF_UNCHECKED:
		c->unchecked_size += len;
		jeb->unchecked_size += len;
		break;

	case REF_NORMAL:
	case REF_PRISTINE:
		c->used_size += len;
		jeb->used_size += len;
		break;

	case REF_OBSOLETE:
		c->dirty_size += len;
		jeb->dirty_size += len;
		break;
	}
	c->free_size -= len;
	jeb->free_size -= len;

#ifdef TEST_TOTLEN
	
	ref->__totlen = len;
	ref_totlen(c, jeb, ref);
#endif
	return ref;
}

int jffs2_scan_dirty_space(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb,
			   uint32_t size)
{
	if (!size)
		return 0;
	if (unlikely(size > jeb->free_size)) {
		pr_crit("Dirty space 0x%x larger then free_size 0x%x (wasted 0x%x)\n",
			size, jeb->free_size, jeb->wasted_size);
		BUG();
	}
	
	if (jeb->last_node && ref_obsolete(jeb->last_node)) {
#ifdef TEST_TOTLEN
		jeb->last_node->__totlen += size;
#endif
		c->dirty_size += size;
		c->free_size -= size;
		jeb->dirty_size += size;
		jeb->free_size -= size;
	} else {
		uint32_t ofs = jeb->offset + c->sector_size - jeb->free_size;
		ofs |= REF_OBSOLETE;

		jffs2_link_node_ref(c, jeb, ofs, size, NULL);
	}

	return 0;
}

static inline uint32_t __ref_totlen(struct jffs2_sb_info *c,
				    struct jffs2_eraseblock *jeb,
				    struct jffs2_raw_node_ref *ref)
{
	uint32_t ref_end;
	struct jffs2_raw_node_ref *next_ref = ref_next(ref);

	if (next_ref)
		ref_end = ref_offset(next_ref);
	else {
		if (!jeb)
			jeb = &c->blocks[ref->flash_offset / c->sector_size];

		
		if (unlikely(ref != jeb->last_node)) {
			pr_crit("ref %p @0x%08x is not jeb->last_node (%p @0x%08x)\n",
				ref, ref_offset(ref), jeb->last_node,
				jeb->last_node ?
				ref_offset(jeb->last_node) : 0);
			BUG();
		}
		ref_end = jeb->offset + c->sector_size - jeb->free_size;
	}
	return ref_end - ref_offset(ref);
}

uint32_t __jffs2_ref_totlen(struct jffs2_sb_info *c, struct jffs2_eraseblock *jeb,
			    struct jffs2_raw_node_ref *ref)
{
	uint32_t ret;

	ret = __ref_totlen(c, jeb, ref);

#ifdef TEST_TOTLEN
	if (unlikely(ret != ref->__totlen)) {
		if (!jeb)
			jeb = &c->blocks[ref->flash_offset / c->sector_size];

		pr_crit("Totlen for ref at %p (0x%08x-0x%08x) miscalculated as 0x%x instead of %x\n",
			ref, ref_offset(ref), ref_offset(ref) + ref->__totlen,
			ret, ref->__totlen);
		if (ref_next(ref)) {
			pr_crit("next %p (0x%08x-0x%08x)\n",
				ref_next(ref), ref_offset(ref_next(ref)),
				ref_offset(ref_next(ref)) + ref->__totlen);
		} else 
			pr_crit("No next ref. jeb->last_node is %p\n",
				jeb->last_node);

		pr_crit("jeb->wasted_size %x, dirty_size %x, used_size %x, free_size %x\n",
			jeb->wasted_size, jeb->dirty_size, jeb->used_size,
			jeb->free_size);

#if defined(JFFS2_DBG_DUMPS) || defined(JFFS2_DBG_PARANOIA_CHECKS)
		__jffs2_dbg_dump_node_refs_nolock(c, jeb);
#endif

		WARN_ON(1);

		ret = ref->__totlen;
	}
#endif 
	return ret;
}
