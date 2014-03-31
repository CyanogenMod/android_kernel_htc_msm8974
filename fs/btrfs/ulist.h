/*
 * Copyright (C) 2011 STRATO AG
 * written by Arne Jansen <sensille@gmx.net>
 * Distributed under the GNU GPL license version 2.
 *
 */

#ifndef __ULIST__
#define __ULIST__


#define ULIST_SIZE 16

struct ulist_node {
	u64 val;		
	unsigned long aux;	
};

struct ulist {
	unsigned long nnodes;

	unsigned long nodes_alloced;

	struct ulist_node *nodes;

	struct ulist_node int_nodes[ULIST_SIZE];
};

void ulist_init(struct ulist *ulist);
void ulist_fini(struct ulist *ulist);
void ulist_reinit(struct ulist *ulist);
struct ulist *ulist_alloc(unsigned long gfp_mask);
void ulist_free(struct ulist *ulist);
int ulist_add(struct ulist *ulist, u64 val, unsigned long aux,
	      unsigned long gfp_mask);
struct ulist_node *ulist_next(struct ulist *ulist, struct ulist_node *prev);

#endif
