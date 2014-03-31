/*
 * net/tipc/name_table.c: TIPC name table code
 *
 * Copyright (c) 2000-2006, Ericsson AB
 * Copyright (c) 2004-2008, 2010-2011, Wind River Systems
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "core.h"
#include "config.h"
#include "name_table.h"
#include "name_distr.h"
#include "subscr.h"
#include "port.h"

static int tipc_nametbl_size = 1024;		


struct name_info {
	struct list_head node_list;
	struct list_head cluster_list;
	struct list_head zone_list;
	u32 node_list_size;
	u32 cluster_list_size;
	u32 zone_list_size;
};


struct sub_seq {
	u32 lower;
	u32 upper;
	struct name_info *info;
};


struct name_seq {
	u32 type;
	struct sub_seq *sseqs;
	u32 alloc;
	u32 first_free;
	struct hlist_node ns_list;
	struct list_head subscriptions;
	spinlock_t lock;
};


struct name_table {
	struct hlist_head *types;
	u32 local_publ_count;
};

static struct name_table table;
DEFINE_RWLOCK(tipc_nametbl_lock);

static int hash(int x)
{
	return x & (tipc_nametbl_size - 1);
}


static struct publication *publ_create(u32 type, u32 lower, u32 upper,
				       u32 scope, u32 node, u32 port_ref,
				       u32 key)
{
	struct publication *publ = kzalloc(sizeof(*publ), GFP_ATOMIC);
	if (publ == NULL) {
		warn("Publication creation failure, no memory\n");
		return NULL;
	}

	publ->type = type;
	publ->lower = lower;
	publ->upper = upper;
	publ->scope = scope;
	publ->node = node;
	publ->ref = port_ref;
	publ->key = key;
	INIT_LIST_HEAD(&publ->local_list);
	INIT_LIST_HEAD(&publ->pport_list);
	INIT_LIST_HEAD(&publ->subscr.nodesub_list);
	return publ;
}


static struct sub_seq *tipc_subseq_alloc(u32 cnt)
{
	struct sub_seq *sseq = kcalloc(cnt, sizeof(struct sub_seq), GFP_ATOMIC);
	return sseq;
}


static struct name_seq *tipc_nameseq_create(u32 type, struct hlist_head *seq_head)
{
	struct name_seq *nseq = kzalloc(sizeof(*nseq), GFP_ATOMIC);
	struct sub_seq *sseq = tipc_subseq_alloc(1);

	if (!nseq || !sseq) {
		warn("Name sequence creation failed, no memory\n");
		kfree(nseq);
		kfree(sseq);
		return NULL;
	}

	spin_lock_init(&nseq->lock);
	nseq->type = type;
	nseq->sseqs = sseq;
	nseq->alloc = 1;
	INIT_HLIST_NODE(&nseq->ns_list);
	INIT_LIST_HEAD(&nseq->subscriptions);
	hlist_add_head(&nseq->ns_list, seq_head);
	return nseq;
}


static struct sub_seq *nameseq_find_subseq(struct name_seq *nseq,
					   u32 instance)
{
	struct sub_seq *sseqs = nseq->sseqs;
	int low = 0;
	int high = nseq->first_free - 1;
	int mid;

	while (low <= high) {
		mid = (low + high) / 2;
		if (instance < sseqs[mid].lower)
			high = mid - 1;
		else if (instance > sseqs[mid].upper)
			low = mid + 1;
		else
			return &sseqs[mid];
	}
	return NULL;
}


static u32 nameseq_locate_subseq(struct name_seq *nseq, u32 instance)
{
	struct sub_seq *sseqs = nseq->sseqs;
	int low = 0;
	int high = nseq->first_free - 1;
	int mid;

	while (low <= high) {
		mid = (low + high) / 2;
		if (instance < sseqs[mid].lower)
			high = mid - 1;
		else if (instance > sseqs[mid].upper)
			low = mid + 1;
		else
			return mid;
	}
	return low;
}


static struct publication *tipc_nameseq_insert_publ(struct name_seq *nseq,
						    u32 type, u32 lower, u32 upper,
						    u32 scope, u32 node, u32 port, u32 key)
{
	struct tipc_subscription *s;
	struct tipc_subscription *st;
	struct publication *publ;
	struct sub_seq *sseq;
	struct name_info *info;
	int created_subseq = 0;

	sseq = nameseq_find_subseq(nseq, lower);
	if (sseq) {

		

		if ((sseq->lower != lower) || (sseq->upper != upper)) {
			warn("Cannot publish {%u,%u,%u}, overlap error\n",
			     type, lower, upper);
			return NULL;
		}

		info = sseq->info;

		
		list_for_each_entry(publ, &info->zone_list, zone_list) {
			if ((publ->ref == port) && (publ->key == key) &&
			    (!publ->node || (publ->node == node)))
				return NULL;
		}
	} else {
		u32 inspos;
		struct sub_seq *freesseq;

		

		inspos = nameseq_locate_subseq(nseq, lower);

		

		if ((inspos < nseq->first_free) &&
		    (upper >= nseq->sseqs[inspos].lower)) {
			warn("Cannot publish {%u,%u,%u}, overlap error\n",
			     type, lower, upper);
			return NULL;
		}

		

		if (nseq->first_free == nseq->alloc) {
			struct sub_seq *sseqs = tipc_subseq_alloc(nseq->alloc * 2);

			if (!sseqs) {
				warn("Cannot publish {%u,%u,%u}, no memory\n",
				     type, lower, upper);
				return NULL;
			}
			memcpy(sseqs, nseq->sseqs,
			       nseq->alloc * sizeof(struct sub_seq));
			kfree(nseq->sseqs);
			nseq->sseqs = sseqs;
			nseq->alloc *= 2;
		}

		info = kzalloc(sizeof(*info), GFP_ATOMIC);
		if (!info) {
			warn("Cannot publish {%u,%u,%u}, no memory\n",
			     type, lower, upper);
			return NULL;
		}

		INIT_LIST_HEAD(&info->node_list);
		INIT_LIST_HEAD(&info->cluster_list);
		INIT_LIST_HEAD(&info->zone_list);

		

		sseq = &nseq->sseqs[inspos];
		freesseq = &nseq->sseqs[nseq->first_free];
		memmove(sseq + 1, sseq, (freesseq - sseq) * sizeof(*sseq));
		memset(sseq, 0, sizeof(*sseq));
		nseq->first_free++;
		sseq->lower = lower;
		sseq->upper = upper;
		sseq->info = info;
		created_subseq = 1;
	}

	

	publ = publ_create(type, lower, upper, scope, node, port, key);
	if (!publ)
		return NULL;

	list_add(&publ->zone_list, &info->zone_list);
	info->zone_list_size++;

	if (in_own_cluster(node)) {
		list_add(&publ->cluster_list, &info->cluster_list);
		info->cluster_list_size++;
	}

	if (node == tipc_own_addr) {
		list_add(&publ->node_list, &info->node_list);
		info->node_list_size++;
	}

	list_for_each_entry_safe(s, st, &nseq->subscriptions, nameseq_list) {
		tipc_subscr_report_overlap(s,
					   publ->lower,
					   publ->upper,
					   TIPC_PUBLISHED,
					   publ->ref,
					   publ->node,
					   created_subseq);
	}
	return publ;
}


static struct publication *tipc_nameseq_remove_publ(struct name_seq *nseq, u32 inst,
						    u32 node, u32 ref, u32 key)
{
	struct publication *publ;
	struct sub_seq *sseq = nameseq_find_subseq(nseq, inst);
	struct name_info *info;
	struct sub_seq *free;
	struct tipc_subscription *s, *st;
	int removed_subseq = 0;

	if (!sseq)
		return NULL;

	info = sseq->info;

	

	list_for_each_entry(publ, &info->zone_list, zone_list) {
		if ((publ->key == key) && (publ->ref == ref) &&
		    (!publ->node || (publ->node == node)))
			goto found;
	}
	return NULL;

found:
	

	list_del(&publ->zone_list);
	info->zone_list_size--;

	

	if (in_own_cluster(node)) {
		list_del(&publ->cluster_list);
		info->cluster_list_size--;
	}

	

	if (node == tipc_own_addr) {
		list_del(&publ->node_list);
		info->node_list_size--;
	}

	

	if (list_empty(&info->zone_list)) {
		kfree(info);
		free = &nseq->sseqs[nseq->first_free--];
		memmove(sseq, sseq + 1, (free - (sseq + 1)) * sizeof(*sseq));
		removed_subseq = 1;
	}

	

	list_for_each_entry_safe(s, st, &nseq->subscriptions, nameseq_list) {
		tipc_subscr_report_overlap(s,
					   publ->lower,
					   publ->upper,
					   TIPC_WITHDRAWN,
					   publ->ref,
					   publ->node,
					   removed_subseq);
	}

	return publ;
}


static void tipc_nameseq_subscribe(struct name_seq *nseq,
					struct tipc_subscription *s)
{
	struct sub_seq *sseq = nseq->sseqs;

	list_add(&s->nameseq_list, &nseq->subscriptions);

	if (!sseq)
		return;

	while (sseq != &nseq->sseqs[nseq->first_free]) {
		if (tipc_subscr_overlap(s, sseq->lower, sseq->upper)) {
			struct publication *crs;
			struct name_info *info = sseq->info;
			int must_report = 1;

			list_for_each_entry(crs, &info->zone_list, zone_list) {
				tipc_subscr_report_overlap(s,
							   sseq->lower,
							   sseq->upper,
							   TIPC_PUBLISHED,
							   crs->ref,
							   crs->node,
							   must_report);
				must_report = 0;
			}
		}
		sseq++;
	}
}

static struct name_seq *nametbl_find_seq(u32 type)
{
	struct hlist_head *seq_head;
	struct hlist_node *seq_node;
	struct name_seq *ns;

	seq_head = &table.types[hash(type)];
	hlist_for_each_entry(ns, seq_node, seq_head, ns_list) {
		if (ns->type == type)
			return ns;
	}

	return NULL;
};

struct publication *tipc_nametbl_insert_publ(u32 type, u32 lower, u32 upper,
					     u32 scope, u32 node, u32 port, u32 key)
{
	struct name_seq *seq = nametbl_find_seq(type);

	if (lower > upper) {
		warn("Failed to publish illegal {%u,%u,%u}\n",
		     type, lower, upper);
		return NULL;
	}

	if (!seq)
		seq = tipc_nameseq_create(type, &table.types[hash(type)]);
	if (!seq)
		return NULL;

	return tipc_nameseq_insert_publ(seq, type, lower, upper,
					scope, node, port, key);
}

struct publication *tipc_nametbl_remove_publ(u32 type, u32 lower,
					     u32 node, u32 ref, u32 key)
{
	struct publication *publ;
	struct name_seq *seq = nametbl_find_seq(type);

	if (!seq)
		return NULL;

	publ = tipc_nameseq_remove_publ(seq, lower, node, ref, key);

	if (!seq->first_free && list_empty(&seq->subscriptions)) {
		hlist_del_init(&seq->ns_list);
		kfree(seq->sseqs);
		kfree(seq);
	}
	return publ;
}


u32 tipc_nametbl_translate(u32 type, u32 instance, u32 *destnode)
{
	struct sub_seq *sseq;
	struct name_info *info;
	struct publication *publ;
	struct name_seq *seq;
	u32 ref = 0;
	u32 node = 0;

	if (!tipc_in_scope(*destnode, tipc_own_addr))
		return 0;

	read_lock_bh(&tipc_nametbl_lock);
	seq = nametbl_find_seq(type);
	if (unlikely(!seq))
		goto not_found;
	sseq = nameseq_find_subseq(seq, instance);
	if (unlikely(!sseq))
		goto not_found;
	spin_lock_bh(&seq->lock);
	info = sseq->info;

	
	if (likely(!*destnode)) {
		if (!list_empty(&info->node_list)) {
			publ = list_first_entry(&info->node_list,
						struct publication,
						node_list);
			list_move_tail(&publ->node_list,
				       &info->node_list);
		} else if (!list_empty(&info->cluster_list)) {
			publ = list_first_entry(&info->cluster_list,
						struct publication,
						cluster_list);
			list_move_tail(&publ->cluster_list,
				       &info->cluster_list);
		} else {
			publ = list_first_entry(&info->zone_list,
						struct publication,
						zone_list);
			list_move_tail(&publ->zone_list,
				       &info->zone_list);
		}
	}

	
	else if (*destnode == tipc_own_addr) {
		if (list_empty(&info->node_list))
			goto no_match;
		publ = list_first_entry(&info->node_list, struct publication,
					node_list);
		list_move_tail(&publ->node_list, &info->node_list);
	} else if (in_own_cluster(*destnode)) {
		if (list_empty(&info->cluster_list))
			goto no_match;
		publ = list_first_entry(&info->cluster_list, struct publication,
					cluster_list);
		list_move_tail(&publ->cluster_list, &info->cluster_list);
	} else {
		publ = list_first_entry(&info->zone_list, struct publication,
					zone_list);
		list_move_tail(&publ->zone_list, &info->zone_list);
	}

	ref = publ->ref;
	node = publ->node;
no_match:
	spin_unlock_bh(&seq->lock);
not_found:
	read_unlock_bh(&tipc_nametbl_lock);
	*destnode = node;
	return ref;
}


int tipc_nametbl_mc_translate(u32 type, u32 lower, u32 upper, u32 limit,
			      struct tipc_port_list *dports)
{
	struct name_seq *seq;
	struct sub_seq *sseq;
	struct sub_seq *sseq_stop;
	struct name_info *info;
	int res = 0;

	read_lock_bh(&tipc_nametbl_lock);
	seq = nametbl_find_seq(type);
	if (!seq)
		goto exit;

	spin_lock_bh(&seq->lock);

	sseq = seq->sseqs + nameseq_locate_subseq(seq, lower);
	sseq_stop = seq->sseqs + seq->first_free;
	for (; sseq != sseq_stop; sseq++) {
		struct publication *publ;

		if (sseq->lower > upper)
			break;

		info = sseq->info;
		list_for_each_entry(publ, &info->node_list, node_list) {
			if (publ->scope <= limit)
				tipc_port_list_add(dports, publ->ref);
		}

		if (info->cluster_list_size != info->node_list_size)
			res = 1;
	}

	spin_unlock_bh(&seq->lock);
exit:
	read_unlock_bh(&tipc_nametbl_lock);
	return res;
}


struct publication *tipc_nametbl_publish(u32 type, u32 lower, u32 upper,
				    u32 scope, u32 port_ref, u32 key)
{
	struct publication *publ;

	if (table.local_publ_count >= tipc_max_publications) {
		warn("Publication failed, local publication limit reached (%u)\n",
		     tipc_max_publications);
		return NULL;
	}

	write_lock_bh(&tipc_nametbl_lock);
	table.local_publ_count++;
	publ = tipc_nametbl_insert_publ(type, lower, upper, scope,
				   tipc_own_addr, port_ref, key);
	if (publ && (scope != TIPC_NODE_SCOPE))
		tipc_named_publish(publ);
	write_unlock_bh(&tipc_nametbl_lock);
	return publ;
}


int tipc_nametbl_withdraw(u32 type, u32 lower, u32 ref, u32 key)
{
	struct publication *publ;

	write_lock_bh(&tipc_nametbl_lock);
	publ = tipc_nametbl_remove_publ(type, lower, tipc_own_addr, ref, key);
	if (likely(publ)) {
		table.local_publ_count--;
		if (publ->scope != TIPC_NODE_SCOPE)
			tipc_named_withdraw(publ);
		write_unlock_bh(&tipc_nametbl_lock);
		list_del_init(&publ->pport_list);
		kfree(publ);
		return 1;
	}
	write_unlock_bh(&tipc_nametbl_lock);
	err("Unable to remove local publication\n"
	    "(type=%u, lower=%u, ref=%u, key=%u)\n",
	    type, lower, ref, key);
	return 0;
}


void tipc_nametbl_subscribe(struct tipc_subscription *s)
{
	u32 type = s->seq.type;
	struct name_seq *seq;

	write_lock_bh(&tipc_nametbl_lock);
	seq = nametbl_find_seq(type);
	if (!seq)
		seq = tipc_nameseq_create(type, &table.types[hash(type)]);
	if (seq) {
		spin_lock_bh(&seq->lock);
		tipc_nameseq_subscribe(seq, s);
		spin_unlock_bh(&seq->lock);
	} else {
		warn("Failed to create subscription for {%u,%u,%u}\n",
		     s->seq.type, s->seq.lower, s->seq.upper);
	}
	write_unlock_bh(&tipc_nametbl_lock);
}


void tipc_nametbl_unsubscribe(struct tipc_subscription *s)
{
	struct name_seq *seq;

	write_lock_bh(&tipc_nametbl_lock);
	seq = nametbl_find_seq(s->seq.type);
	if (seq != NULL) {
		spin_lock_bh(&seq->lock);
		list_del_init(&s->nameseq_list);
		spin_unlock_bh(&seq->lock);
		if ((seq->first_free == 0) && list_empty(&seq->subscriptions)) {
			hlist_del_init(&seq->ns_list);
			kfree(seq->sseqs);
			kfree(seq);
		}
	}
	write_unlock_bh(&tipc_nametbl_lock);
}



static void subseq_list(struct sub_seq *sseq, struct print_buf *buf, u32 depth,
			u32 index)
{
	char portIdStr[27];
	const char *scope_str[] = {"", " zone", " cluster", " node"};
	struct publication *publ;
	struct name_info *info;

	tipc_printf(buf, "%-10u %-10u ", sseq->lower, sseq->upper);

	if (depth == 2) {
		tipc_printf(buf, "\n");
		return;
	}

	info = sseq->info;

	list_for_each_entry(publ, &info->zone_list, zone_list) {
		sprintf(portIdStr, "<%u.%u.%u:%u>",
			 tipc_zone(publ->node), tipc_cluster(publ->node),
			 tipc_node(publ->node), publ->ref);
		tipc_printf(buf, "%-26s ", portIdStr);
		if (depth > 3) {
			tipc_printf(buf, "%-10u %s", publ->key,
				    scope_str[publ->scope]);
		}
		if (!list_is_last(&publ->zone_list, &info->zone_list))
			tipc_printf(buf, "\n%33s", " ");
	};

	tipc_printf(buf, "\n");
}


static void nameseq_list(struct name_seq *seq, struct print_buf *buf, u32 depth,
			 u32 type, u32 lowbound, u32 upbound, u32 index)
{
	struct sub_seq *sseq;
	char typearea[11];

	if (seq->first_free == 0)
		return;

	sprintf(typearea, "%-10u", seq->type);

	if (depth == 1) {
		tipc_printf(buf, "%s\n", typearea);
		return;
	}

	for (sseq = seq->sseqs; sseq != &seq->sseqs[seq->first_free]; sseq++) {
		if ((lowbound <= sseq->upper) && (upbound >= sseq->lower)) {
			tipc_printf(buf, "%s ", typearea);
			spin_lock_bh(&seq->lock);
			subseq_list(sseq, buf, depth, index);
			spin_unlock_bh(&seq->lock);
			sprintf(typearea, "%10s", " ");
		}
	}
}


static void nametbl_header(struct print_buf *buf, u32 depth)
{
	const char *header[] = {
		"Type       ",
		"Lower      Upper      ",
		"Port Identity              ",
		"Publication Scope"
	};

	int i;

	if (depth > 4)
		depth = 4;
	for (i = 0; i < depth; i++)
		tipc_printf(buf, header[i]);
	tipc_printf(buf, "\n");
}


static void nametbl_list(struct print_buf *buf, u32 depth_info,
			 u32 type, u32 lowbound, u32 upbound)
{
	struct hlist_head *seq_head;
	struct hlist_node *seq_node;
	struct name_seq *seq;
	int all_types;
	u32 depth;
	u32 i;

	all_types = (depth_info & TIPC_NTQ_ALLTYPES);
	depth = (depth_info & ~TIPC_NTQ_ALLTYPES);

	if (depth == 0)
		return;

	if (all_types) {
		
		nametbl_header(buf, depth);
		lowbound = 0;
		upbound = ~0;
		for (i = 0; i < tipc_nametbl_size; i++) {
			seq_head = &table.types[i];
			hlist_for_each_entry(seq, seq_node, seq_head, ns_list) {
				nameseq_list(seq, buf, depth, seq->type,
					     lowbound, upbound, i);
			}
		}
	} else {
		
		if (upbound < lowbound) {
			tipc_printf(buf, "invalid name sequence specified\n");
			return;
		}
		nametbl_header(buf, depth);
		i = hash(type);
		seq_head = &table.types[i];
		hlist_for_each_entry(seq, seq_node, seq_head, ns_list) {
			if (seq->type == type) {
				nameseq_list(seq, buf, depth, type,
					     lowbound, upbound, i);
				break;
			}
		}
	}
}

#define MAX_NAME_TBL_QUERY 32768

struct sk_buff *tipc_nametbl_get(const void *req_tlv_area, int req_tlv_space)
{
	struct sk_buff *buf;
	struct tipc_name_table_query *argv;
	struct tlv_desc *rep_tlv;
	struct print_buf b;
	int str_len;

	if (!TLV_CHECK(req_tlv_area, req_tlv_space, TIPC_TLV_NAME_TBL_QUERY))
		return tipc_cfg_reply_error_string(TIPC_CFG_TLV_ERROR);

	buf = tipc_cfg_reply_alloc(TLV_SPACE(MAX_NAME_TBL_QUERY));
	if (!buf)
		return NULL;

	rep_tlv = (struct tlv_desc *)buf->data;
	tipc_printbuf_init(&b, TLV_DATA(rep_tlv), MAX_NAME_TBL_QUERY);
	argv = (struct tipc_name_table_query *)TLV_DATA(req_tlv_area);
	read_lock_bh(&tipc_nametbl_lock);
	nametbl_list(&b, ntohl(argv->depth), ntohl(argv->type),
		     ntohl(argv->lowbound), ntohl(argv->upbound));
	read_unlock_bh(&tipc_nametbl_lock);
	str_len = tipc_printbuf_validate(&b);

	skb_put(buf, TLV_SPACE(str_len));
	TLV_SET(rep_tlv, TIPC_TLV_ULTRA_STRING, NULL, str_len);

	return buf;
}

int tipc_nametbl_init(void)
{
	table.types = kcalloc(tipc_nametbl_size, sizeof(struct hlist_head),
			      GFP_ATOMIC);
	if (!table.types)
		return -ENOMEM;

	table.local_publ_count = 0;
	return 0;
}

void tipc_nametbl_stop(void)
{
	u32 i;

	if (!table.types)
		return;

	

	write_lock_bh(&tipc_nametbl_lock);
	for (i = 0; i < tipc_nametbl_size; i++) {
		if (!hlist_empty(&table.types[i]))
			err("tipc_nametbl_stop(): hash chain %u is non-null\n", i);
	}
	kfree(table.types);
	table.types = NULL;
	write_unlock_bh(&tipc_nametbl_lock);
}

