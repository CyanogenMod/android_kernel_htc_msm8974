/*
 * Copyright (C) 2011 STRATO AG
 * written by Arne Jansen <sensille@gmx.net>
 * Distributed under the GNU GPL license version 2.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include "ulist.h"


void ulist_init(struct ulist *ulist)
{
	ulist->nnodes = 0;
	ulist->nodes = ulist->int_nodes;
	ulist->nodes_alloced = ULIST_SIZE;
}
EXPORT_SYMBOL(ulist_init);

void ulist_fini(struct ulist *ulist)
{
	if (ulist->nodes_alloced > ULIST_SIZE)
		kfree(ulist->nodes);
	ulist->nodes_alloced = 0;	
}
EXPORT_SYMBOL(ulist_fini);

void ulist_reinit(struct ulist *ulist)
{
	ulist_fini(ulist);
	ulist_init(ulist);
}
EXPORT_SYMBOL(ulist_reinit);

struct ulist *ulist_alloc(unsigned long gfp_mask)
{
	struct ulist *ulist = kmalloc(sizeof(*ulist), gfp_mask);

	if (!ulist)
		return NULL;

	ulist_init(ulist);

	return ulist;
}
EXPORT_SYMBOL(ulist_alloc);

void ulist_free(struct ulist *ulist)
{
	if (!ulist)
		return;
	ulist_fini(ulist);
	kfree(ulist);
}
EXPORT_SYMBOL(ulist_free);

int ulist_add(struct ulist *ulist, u64 val, unsigned long aux,
	      unsigned long gfp_mask)
{
	int i;

	for (i = 0; i < ulist->nnodes; ++i) {
		if (ulist->nodes[i].val == val)
			return 0;
	}

	if (ulist->nnodes >= ulist->nodes_alloced) {
		u64 new_alloced = ulist->nodes_alloced + 128;
		struct ulist_node *new_nodes;
		void *old = NULL;

		if (ulist->nodes_alloced > ULIST_SIZE)
			old = ulist->nodes;

		new_nodes = krealloc(old, sizeof(*new_nodes) * new_alloced,
				     gfp_mask);
		if (!new_nodes)
			return -ENOMEM;

		if (!old)
			memcpy(new_nodes, ulist->int_nodes,
			       sizeof(ulist->int_nodes));

		ulist->nodes = new_nodes;
		ulist->nodes_alloced = new_alloced;
	}
	ulist->nodes[ulist->nnodes].val = val;
	ulist->nodes[ulist->nnodes].aux = aux;
	++ulist->nnodes;

	return 1;
}
EXPORT_SYMBOL(ulist_add);

struct ulist_node *ulist_next(struct ulist *ulist, struct ulist_node *prev)
{
	int next;

	if (ulist->nnodes == 0)
		return NULL;

	if (!prev)
		return &ulist->nodes[0];

	next = (prev - ulist->nodes) + 1;
	if (next < 0 || next >= ulist->nnodes)
		return NULL;

	return &ulist->nodes[next];
}
EXPORT_SYMBOL(ulist_next);
