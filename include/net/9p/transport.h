/*
 * include/net/9p/transport.h
 *
 * Transport Definition
 *
 *  Copyright (C) 2005 by Latchesar Ionkov <lucho@ionkov.net>
 *  Copyright (C) 2004-2008 by Eric Van Hensbergen <ericvh@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 *  Free Software Foundation
 *  51 Franklin Street, Fifth Floor
 *  Boston, MA  02111-1301  USA
 *
 */

#ifndef NET_9P_TRANSPORT_H
#define NET_9P_TRANSPORT_H


struct p9_trans_module {
	struct list_head list;
	char *name;		
	int maxsize;		
	int def;		
	struct module *owner;
	int (*create)(struct p9_client *, const char *, char *);
	void (*close) (struct p9_client *);
	int (*request) (struct p9_client *, struct p9_req_t *req);
	int (*cancel) (struct p9_client *, struct p9_req_t *req);
	int (*zc_request)(struct p9_client *, struct p9_req_t *,
			  char *, char *, int , int, int, int);
};

void v9fs_register_trans(struct p9_trans_module *m);
void v9fs_unregister_trans(struct p9_trans_module *m);
struct p9_trans_module *v9fs_get_trans_by_name(char *s);
struct p9_trans_module *v9fs_get_default_trans(void);
void v9fs_put_trans(struct p9_trans_module *m);
#endif 
