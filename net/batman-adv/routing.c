/*
 * Copyright (C) 2007-2012 B.A.T.M.A.N. contributors:
 *
 * Marek Lindner, Simon Wunderlich
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 */

#include "main.h"
#include "routing.h"
#include "send.h"
#include "soft-interface.h"
#include "hard-interface.h"
#include "icmp_socket.h"
#include "translation-table.h"
#include "originator.h"
#include "vis.h"
#include "unicast.h"

void slide_own_bcast_window(struct hard_iface *hard_iface)
{
	struct bat_priv *bat_priv = netdev_priv(hard_iface->soft_iface);
	struct hashtable_t *hash = bat_priv->orig_hash;
	struct hlist_node *node;
	struct hlist_head *head;
	struct orig_node *orig_node;
	unsigned long *word;
	uint32_t i;
	size_t word_index;

	for (i = 0; i < hash->size; i++) {
		head = &hash->table[i];

		rcu_read_lock();
		hlist_for_each_entry_rcu(orig_node, node, head, hash_entry) {
			spin_lock_bh(&orig_node->ogm_cnt_lock);
			word_index = hard_iface->if_num * NUM_WORDS;
			word = &(orig_node->bcast_own[word_index]);

			bit_get_packet(bat_priv, word, 1, 0);
			orig_node->bcast_own_sum[hard_iface->if_num] =
				bit_packet_count(word);
			spin_unlock_bh(&orig_node->ogm_cnt_lock);
		}
		rcu_read_unlock();
	}
}

static void _update_route(struct bat_priv *bat_priv,
			  struct orig_node *orig_node,
			  struct neigh_node *neigh_node)
{
	struct neigh_node *curr_router;

	curr_router = orig_node_get_router(orig_node);

	
	if ((curr_router) && (!neigh_node)) {
		bat_dbg(DBG_ROUTES, bat_priv, "Deleting route towards: %pM\n",
			orig_node->orig);
		tt_global_del_orig(bat_priv, orig_node,
				   "Deleted route towards originator");

	
	} else if ((!curr_router) && (neigh_node)) {

		bat_dbg(DBG_ROUTES, bat_priv,
			"Adding route towards: %pM (via %pM)\n",
			orig_node->orig, neigh_node->addr);
	
	} else if (neigh_node && curr_router) {
		bat_dbg(DBG_ROUTES, bat_priv,
			"Changing route towards: %pM (now via %pM - was via %pM)\n",
			orig_node->orig, neigh_node->addr,
			curr_router->addr);
	}

	if (curr_router)
		neigh_node_free_ref(curr_router);

	
	if (neigh_node && !atomic_inc_not_zero(&neigh_node->refcount))
		neigh_node = NULL;

	spin_lock_bh(&orig_node->neigh_list_lock);
	rcu_assign_pointer(orig_node->router, neigh_node);
	spin_unlock_bh(&orig_node->neigh_list_lock);

	
	if (curr_router)
		neigh_node_free_ref(curr_router);
}

void update_route(struct bat_priv *bat_priv, struct orig_node *orig_node,
		  struct neigh_node *neigh_node)
{
	struct neigh_node *router = NULL;

	if (!orig_node)
		goto out;

	router = orig_node_get_router(orig_node);

	if (router != neigh_node)
		_update_route(bat_priv, orig_node, neigh_node);

out:
	if (router)
		neigh_node_free_ref(router);
}

void bonding_candidate_del(struct orig_node *orig_node,
			   struct neigh_node *neigh_node)
{
	
	if (list_empty(&neigh_node->bonding_list))
		goto out;

	list_del_rcu(&neigh_node->bonding_list);
	INIT_LIST_HEAD(&neigh_node->bonding_list);
	neigh_node_free_ref(neigh_node);
	atomic_dec(&orig_node->bond_candidates);

out:
	return;
}

void bonding_candidate_add(struct orig_node *orig_node,
			   struct neigh_node *neigh_node)
{
	struct hlist_node *node;
	struct neigh_node *tmp_neigh_node, *router = NULL;
	uint8_t interference_candidate = 0;

	spin_lock_bh(&orig_node->neigh_list_lock);

	
	if (!compare_eth(orig_node->orig,
			 neigh_node->orig_node->primary_addr))
		goto candidate_del;

	router = orig_node_get_router(orig_node);
	if (!router)
		goto candidate_del;

	
	if (neigh_node->tq_avg < router->tq_avg - BONDING_TQ_THRESHOLD)
		goto candidate_del;

	hlist_for_each_entry_rcu(tmp_neigh_node, node,
				 &orig_node->neigh_list, list) {

		if (tmp_neigh_node == neigh_node)
			continue;

		if (list_empty(&tmp_neigh_node->bonding_list))
			continue;

		if ((neigh_node->if_incoming == tmp_neigh_node->if_incoming) ||
		    (compare_eth(neigh_node->addr, tmp_neigh_node->addr))) {
			interference_candidate = 1;
			break;
		}
	}

	
	if (interference_candidate)
		goto candidate_del;

	
	if (!list_empty(&neigh_node->bonding_list))
		goto out;

	if (!atomic_inc_not_zero(&neigh_node->refcount))
		goto out;

	list_add_rcu(&neigh_node->bonding_list, &orig_node->bond_list);
	atomic_inc(&orig_node->bond_candidates);
	goto out;

candidate_del:
	bonding_candidate_del(orig_node, neigh_node);

out:
	spin_unlock_bh(&orig_node->neigh_list_lock);

	if (router)
		neigh_node_free_ref(router);
}

void bonding_save_primary(const struct orig_node *orig_node,
			  struct orig_node *orig_neigh_node,
			  const struct batman_ogm_packet *batman_ogm_packet)
{
	if (!(batman_ogm_packet->flags & PRIMARIES_FIRST_HOP))
		return;

	memcpy(orig_neigh_node->primary_addr, orig_node->orig, ETH_ALEN);
}

int window_protected(struct bat_priv *bat_priv, int32_t seq_num_diff,
		     unsigned long *last_reset)
{
	if ((seq_num_diff <= -TQ_LOCAL_WINDOW_SIZE) ||
	    (seq_num_diff >= EXPECTED_SEQNO_RANGE)) {
		if (has_timed_out(*last_reset, RESET_PROTECTION_MS)) {

			*last_reset = jiffies;
			bat_dbg(DBG_BATMAN, bat_priv,
				"old packet received, start protection\n");

			return 0;
		} else {
			return 1;
		}
	}
	return 0;
}

int recv_bat_ogm_packet(struct sk_buff *skb, struct hard_iface *hard_iface)
{
	struct bat_priv *bat_priv = netdev_priv(hard_iface->soft_iface);
	struct ethhdr *ethhdr;

	
	if (unlikely(!pskb_may_pull(skb, BATMAN_OGM_LEN)))
		return NET_RX_DROP;

	ethhdr = (struct ethhdr *)skb_mac_header(skb);

	
	if (!is_broadcast_ether_addr(ethhdr->h_dest))
		return NET_RX_DROP;

	
	if (is_broadcast_ether_addr(ethhdr->h_source))
		return NET_RX_DROP;

	
	if (skb_cow(skb, 0) < 0)
		return NET_RX_DROP;

	
	if (skb_linearize(skb) < 0)
		return NET_RX_DROP;

	bat_priv->bat_algo_ops->bat_ogm_receive(hard_iface, skb);

	kfree_skb(skb);
	return NET_RX_SUCCESS;
}

static int recv_my_icmp_packet(struct bat_priv *bat_priv,
			       struct sk_buff *skb, size_t icmp_len)
{
	struct hard_iface *primary_if = NULL;
	struct orig_node *orig_node = NULL;
	struct neigh_node *router = NULL;
	struct icmp_packet_rr *icmp_packet;
	int ret = NET_RX_DROP;

	icmp_packet = (struct icmp_packet_rr *)skb->data;

	
	if (icmp_packet->msg_type != ECHO_REQUEST) {
		bat_socket_receive_packet(icmp_packet, icmp_len);
		goto out;
	}

	primary_if = primary_if_get_selected(bat_priv);
	if (!primary_if)
		goto out;

	
	
	orig_node = orig_hash_find(bat_priv, icmp_packet->orig);
	if (!orig_node)
		goto out;

	router = orig_node_get_router(orig_node);
	if (!router)
		goto out;

	
	if (skb_cow(skb, sizeof(struct ethhdr)) < 0)
		goto out;

	icmp_packet = (struct icmp_packet_rr *)skb->data;

	memcpy(icmp_packet->dst, icmp_packet->orig, ETH_ALEN);
	memcpy(icmp_packet->orig, primary_if->net_dev->dev_addr, ETH_ALEN);
	icmp_packet->msg_type = ECHO_REPLY;
	icmp_packet->header.ttl = TTL;

	send_skb_packet(skb, router->if_incoming, router->addr);
	ret = NET_RX_SUCCESS;

out:
	if (primary_if)
		hardif_free_ref(primary_if);
	if (router)
		neigh_node_free_ref(router);
	if (orig_node)
		orig_node_free_ref(orig_node);
	return ret;
}

static int recv_icmp_ttl_exceeded(struct bat_priv *bat_priv,
				  struct sk_buff *skb)
{
	struct hard_iface *primary_if = NULL;
	struct orig_node *orig_node = NULL;
	struct neigh_node *router = NULL;
	struct icmp_packet *icmp_packet;
	int ret = NET_RX_DROP;

	icmp_packet = (struct icmp_packet *)skb->data;

	
	if (icmp_packet->msg_type != ECHO_REQUEST) {
		pr_debug("Warning - can't forward icmp packet from %pM to %pM: ttl exceeded\n",
			 icmp_packet->orig, icmp_packet->dst);
		goto out;
	}

	primary_if = primary_if_get_selected(bat_priv);
	if (!primary_if)
		goto out;

	
	orig_node = orig_hash_find(bat_priv, icmp_packet->orig);
	if (!orig_node)
		goto out;

	router = orig_node_get_router(orig_node);
	if (!router)
		goto out;

	
	if (skb_cow(skb, sizeof(struct ethhdr)) < 0)
		goto out;

	icmp_packet = (struct icmp_packet *)skb->data;

	memcpy(icmp_packet->dst, icmp_packet->orig, ETH_ALEN);
	memcpy(icmp_packet->orig, primary_if->net_dev->dev_addr, ETH_ALEN);
	icmp_packet->msg_type = TTL_EXCEEDED;
	icmp_packet->header.ttl = TTL;

	send_skb_packet(skb, router->if_incoming, router->addr);
	ret = NET_RX_SUCCESS;

out:
	if (primary_if)
		hardif_free_ref(primary_if);
	if (router)
		neigh_node_free_ref(router);
	if (orig_node)
		orig_node_free_ref(orig_node);
	return ret;
}


int recv_icmp_packet(struct sk_buff *skb, struct hard_iface *recv_if)
{
	struct bat_priv *bat_priv = netdev_priv(recv_if->soft_iface);
	struct icmp_packet_rr *icmp_packet;
	struct ethhdr *ethhdr;
	struct orig_node *orig_node = NULL;
	struct neigh_node *router = NULL;
	int hdr_size = sizeof(struct icmp_packet);
	int ret = NET_RX_DROP;

	if (skb->len >= sizeof(struct icmp_packet_rr))
		hdr_size = sizeof(struct icmp_packet_rr);

	
	if (unlikely(!pskb_may_pull(skb, hdr_size)))
		goto out;

	ethhdr = (struct ethhdr *)skb_mac_header(skb);

	
	if (is_broadcast_ether_addr(ethhdr->h_dest))
		goto out;

	
	if (is_broadcast_ether_addr(ethhdr->h_source))
		goto out;

	
	if (!is_my_mac(ethhdr->h_dest))
		goto out;

	icmp_packet = (struct icmp_packet_rr *)skb->data;

	
	if ((hdr_size == sizeof(struct icmp_packet_rr)) &&
	    (icmp_packet->rr_cur < BAT_RR_LEN)) {
		memcpy(&(icmp_packet->rr[icmp_packet->rr_cur]),
		       ethhdr->h_dest, ETH_ALEN);
		icmp_packet->rr_cur++;
	}

	
	if (is_my_mac(icmp_packet->dst))
		return recv_my_icmp_packet(bat_priv, skb, hdr_size);

	
	if (icmp_packet->header.ttl < 2)
		return recv_icmp_ttl_exceeded(bat_priv, skb);

	
	orig_node = orig_hash_find(bat_priv, icmp_packet->dst);
	if (!orig_node)
		goto out;

	router = orig_node_get_router(orig_node);
	if (!router)
		goto out;

	
	if (skb_cow(skb, sizeof(struct ethhdr)) < 0)
		goto out;

	icmp_packet = (struct icmp_packet_rr *)skb->data;

	
	icmp_packet->header.ttl--;

	
	send_skb_packet(skb, router->if_incoming, router->addr);
	ret = NET_RX_SUCCESS;

out:
	if (router)
		neigh_node_free_ref(router);
	if (orig_node)
		orig_node_free_ref(orig_node);
	return ret;
}

static struct neigh_node *find_bond_router(struct orig_node *primary_orig,
					   const struct hard_iface *recv_if)
{
	struct neigh_node *tmp_neigh_node;
	struct neigh_node *router = NULL, *first_candidate = NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(tmp_neigh_node, &primary_orig->bond_list,
				bonding_list) {
		if (!first_candidate)
			first_candidate = tmp_neigh_node;

		
		if (tmp_neigh_node->if_incoming == recv_if)
			continue;

		if (!atomic_inc_not_zero(&tmp_neigh_node->refcount))
			continue;

		router = tmp_neigh_node;
		break;
	}

	
	if (!router && first_candidate &&
	    atomic_inc_not_zero(&first_candidate->refcount))
		router = first_candidate;

	if (!router)
		goto out;

	spin_lock_bh(&primary_orig->neigh_list_lock);
	list_del_rcu(&primary_orig->bond_list);
	list_add_rcu(&primary_orig->bond_list,
		     &router->bonding_list);
	spin_unlock_bh(&primary_orig->neigh_list_lock);

out:
	rcu_read_unlock();
	return router;
}

static struct neigh_node *find_ifalter_router(struct orig_node *primary_orig,
					      const struct hard_iface *recv_if)
{
	struct neigh_node *tmp_neigh_node;
	struct neigh_node *router = NULL, *first_candidate = NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(tmp_neigh_node, &primary_orig->bond_list,
				bonding_list) {
		if (!first_candidate)
			first_candidate = tmp_neigh_node;

		
		if (tmp_neigh_node->if_incoming == recv_if)
			continue;

		if (!atomic_inc_not_zero(&tmp_neigh_node->refcount))
			continue;

		if ((!router) ||
		    (tmp_neigh_node->tq_avg > router->tq_avg)) {
			if (router)
				neigh_node_free_ref(router);

			router = tmp_neigh_node;
			atomic_inc_not_zero(&router->refcount);
		}

		neigh_node_free_ref(tmp_neigh_node);
	}

	
	if (!router && first_candidate &&
	    atomic_inc_not_zero(&first_candidate->refcount))
		router = first_candidate;

	rcu_read_unlock();
	return router;
}

int recv_tt_query(struct sk_buff *skb, struct hard_iface *recv_if)
{
	struct bat_priv *bat_priv = netdev_priv(recv_if->soft_iface);
	struct tt_query_packet *tt_query;
	uint16_t tt_len;
	struct ethhdr *ethhdr;

	
	if (unlikely(!pskb_may_pull(skb, sizeof(struct tt_query_packet))))
		goto out;

	
	if (skb_cow(skb, sizeof(struct tt_query_packet)) < 0)
		goto out;

	ethhdr = (struct ethhdr *)skb_mac_header(skb);

	
	if (is_broadcast_ether_addr(ethhdr->h_dest))
		goto out;

	
	if (is_broadcast_ether_addr(ethhdr->h_source))
		goto out;

	tt_query = (struct tt_query_packet *)skb->data;

	tt_query->tt_data = ntohs(tt_query->tt_data);

	switch (tt_query->flags & TT_QUERY_TYPE_MASK) {
	case TT_REQUEST:
		if (!send_tt_response(bat_priv, tt_query)) {
			bat_dbg(DBG_TT, bat_priv,
				"Routing TT_REQUEST to %pM [%c]\n",
				tt_query->dst,
				(tt_query->flags & TT_FULL_TABLE ? 'F' : '.'));
			tt_query->tt_data = htons(tt_query->tt_data);
			return route_unicast_packet(skb, recv_if);
		}
		break;
	case TT_RESPONSE:
		if (is_my_mac(tt_query->dst)) {
			if (skb_linearize(skb) < 0)
				goto out;

			tt_len = tt_query->tt_data * sizeof(struct tt_change);

			
			if (unlikely(skb_headlen(skb) <
				     sizeof(struct tt_query_packet) + tt_len))
				goto out;

			handle_tt_response(bat_priv, tt_query);
		} else {
			bat_dbg(DBG_TT, bat_priv,
				"Routing TT_RESPONSE to %pM [%c]\n",
				tt_query->dst,
				(tt_query->flags & TT_FULL_TABLE ? 'F' : '.'));
			tt_query->tt_data = htons(tt_query->tt_data);
			return route_unicast_packet(skb, recv_if);
		}
		break;
	}

out:
	
	return NET_RX_DROP;
}

int recv_roam_adv(struct sk_buff *skb, struct hard_iface *recv_if)
{
	struct bat_priv *bat_priv = netdev_priv(recv_if->soft_iface);
	struct roam_adv_packet *roam_adv_packet;
	struct orig_node *orig_node;
	struct ethhdr *ethhdr;

	
	if (unlikely(!pskb_may_pull(skb, sizeof(struct roam_adv_packet))))
		goto out;

	ethhdr = (struct ethhdr *)skb_mac_header(skb);

	
	if (is_broadcast_ether_addr(ethhdr->h_dest))
		goto out;

	
	if (is_broadcast_ether_addr(ethhdr->h_source))
		goto out;

	roam_adv_packet = (struct roam_adv_packet *)skb->data;

	if (!is_my_mac(roam_adv_packet->dst))
		return route_unicast_packet(skb, recv_if);

	orig_node = orig_hash_find(bat_priv, roam_adv_packet->src);
	if (!orig_node)
		goto out;

	bat_dbg(DBG_TT, bat_priv,
		"Received ROAMING_ADV from %pM (client %pM)\n",
		roam_adv_packet->src, roam_adv_packet->client);

	tt_global_add(bat_priv, orig_node, roam_adv_packet->client,
		      atomic_read(&orig_node->last_ttvn) + 1, true, false);

	bat_priv->tt_poss_change = true;

	orig_node_free_ref(orig_node);
out:
	
	return NET_RX_DROP;
}

struct neigh_node *find_router(struct bat_priv *bat_priv,
			       struct orig_node *orig_node,
			       const struct hard_iface *recv_if)
{
	struct orig_node *primary_orig_node;
	struct orig_node *router_orig;
	struct neigh_node *router;
	static uint8_t zero_mac[ETH_ALEN] = {0, 0, 0, 0, 0, 0};
	int bonding_enabled;

	if (!orig_node)
		return NULL;

	router = orig_node_get_router(orig_node);
	if (!router)
		goto err;

	bonding_enabled = atomic_read(&bat_priv->bonding);

	rcu_read_lock();
	
	router_orig = router->orig_node;
	if (!router_orig)
		goto err_unlock;

	if ((!recv_if) && (!bonding_enabled))
		goto return_router;

	if (compare_eth(router_orig->primary_addr, zero_mac))
		goto return_router;


	if (compare_eth(router_orig->primary_addr, router_orig->orig)) {
		primary_orig_node = router_orig;
	} else {
		primary_orig_node = orig_hash_find(bat_priv,
						   router_orig->primary_addr);
		if (!primary_orig_node)
			goto return_router;

		orig_node_free_ref(primary_orig_node);
	}

	if (atomic_read(&primary_orig_node->bond_candidates) < 2)
		goto return_router;


	neigh_node_free_ref(router);

	if (bonding_enabled)
		router = find_bond_router(primary_orig_node, recv_if);
	else
		router = find_ifalter_router(primary_orig_node, recv_if);

return_router:
	if (router && router->if_incoming->if_status != IF_ACTIVE)
		goto err_unlock;

	rcu_read_unlock();
	return router;
err_unlock:
	rcu_read_unlock();
err:
	if (router)
		neigh_node_free_ref(router);
	return NULL;
}

static int check_unicast_packet(struct sk_buff *skb, int hdr_size)
{
	struct ethhdr *ethhdr;

	
	if (unlikely(!pskb_may_pull(skb, hdr_size)))
		return -1;

	ethhdr = (struct ethhdr *)skb_mac_header(skb);

	
	if (is_broadcast_ether_addr(ethhdr->h_dest))
		return -1;

	
	if (is_broadcast_ether_addr(ethhdr->h_source))
		return -1;

	
	if (!is_my_mac(ethhdr->h_dest))
		return -1;

	return 0;
}

int route_unicast_packet(struct sk_buff *skb, struct hard_iface *recv_if)
{
	struct bat_priv *bat_priv = netdev_priv(recv_if->soft_iface);
	struct orig_node *orig_node = NULL;
	struct neigh_node *neigh_node = NULL;
	struct unicast_packet *unicast_packet;
	struct ethhdr *ethhdr = (struct ethhdr *)skb_mac_header(skb);
	int ret = NET_RX_DROP;
	struct sk_buff *new_skb;

	unicast_packet = (struct unicast_packet *)skb->data;

	
	if (unicast_packet->header.ttl < 2) {
		pr_debug("Warning - can't forward unicast packet from %pM to %pM: ttl exceeded\n",
			 ethhdr->h_source, unicast_packet->dest);
		goto out;
	}

	
	orig_node = orig_hash_find(bat_priv, unicast_packet->dest);

	if (!orig_node)
		goto out;

	
	neigh_node = find_router(bat_priv, orig_node, recv_if);

	if (!neigh_node)
		goto out;

	
	if (skb_cow(skb, sizeof(struct ethhdr)) < 0)
		goto out;

	unicast_packet = (struct unicast_packet *)skb->data;

	if (unicast_packet->header.packet_type == BAT_UNICAST &&
	    atomic_read(&bat_priv->fragmentation) &&
	    skb->len > neigh_node->if_incoming->net_dev->mtu) {
		ret = frag_send_skb(skb, bat_priv,
				    neigh_node->if_incoming, neigh_node->addr);
		goto out;
	}

	if (unicast_packet->header.packet_type == BAT_UNICAST_FRAG &&
	    frag_can_reassemble(skb, neigh_node->if_incoming->net_dev->mtu)) {

		ret = frag_reassemble_skb(skb, bat_priv, &new_skb);

		if (ret == NET_RX_DROP)
			goto out;

		
		if (!new_skb) {
			ret = NET_RX_SUCCESS;
			goto out;
		}

		skb = new_skb;
		unicast_packet = (struct unicast_packet *)skb->data;
	}

	
	unicast_packet->header.ttl--;

	
	send_skb_packet(skb, neigh_node->if_incoming, neigh_node->addr);
	ret = NET_RX_SUCCESS;

out:
	if (neigh_node)
		neigh_node_free_ref(neigh_node);
	if (orig_node)
		orig_node_free_ref(orig_node);
	return ret;
}

static int check_unicast_ttvn(struct bat_priv *bat_priv,
			       struct sk_buff *skb) {
	uint8_t curr_ttvn;
	struct orig_node *orig_node;
	struct ethhdr *ethhdr;
	struct hard_iface *primary_if;
	struct unicast_packet *unicast_packet;
	bool tt_poss_change;

	
	if (skb_cow(skb, sizeof(struct unicast_packet)) < 0)
		return 0;

	unicast_packet = (struct unicast_packet *)skb->data;

	if (is_my_mac(unicast_packet->dest)) {
		tt_poss_change = bat_priv->tt_poss_change;
		curr_ttvn = (uint8_t)atomic_read(&bat_priv->ttvn);
	} else {
		orig_node = orig_hash_find(bat_priv, unicast_packet->dest);

		if (!orig_node)
			return 0;

		curr_ttvn = (uint8_t)atomic_read(&orig_node->last_ttvn);
		tt_poss_change = orig_node->tt_poss_change;
		orig_node_free_ref(orig_node);
	}

	
	if (seq_before(unicast_packet->ttvn, curr_ttvn) || tt_poss_change) {
		
		if (skb_linearize(skb) < 0)
			return 0;

		ethhdr = (struct ethhdr *)(skb->data +
			sizeof(struct unicast_packet));
		orig_node = transtable_search(bat_priv, NULL, ethhdr->h_dest);

		if (!orig_node) {
			if (!is_my_client(bat_priv, ethhdr->h_dest))
				return 0;
			primary_if = primary_if_get_selected(bat_priv);
			if (!primary_if)
				return 0;
			memcpy(unicast_packet->dest,
			       primary_if->net_dev->dev_addr, ETH_ALEN);
			hardif_free_ref(primary_if);
		} else {
			memcpy(unicast_packet->dest, orig_node->orig,
			       ETH_ALEN);
			curr_ttvn = (uint8_t)
				atomic_read(&orig_node->last_ttvn);
			orig_node_free_ref(orig_node);
		}

		bat_dbg(DBG_ROUTES, bat_priv,
			"TTVN mismatch (old_ttvn %u new_ttvn %u)! Rerouting unicast packet (for %pM) to %pM\n",
			unicast_packet->ttvn, curr_ttvn, ethhdr->h_dest,
			unicast_packet->dest);

		unicast_packet->ttvn = curr_ttvn;
	}
	return 1;
}

int recv_unicast_packet(struct sk_buff *skb, struct hard_iface *recv_if)
{
	struct bat_priv *bat_priv = netdev_priv(recv_if->soft_iface);
	struct unicast_packet *unicast_packet;
	int hdr_size = sizeof(*unicast_packet);

	if (check_unicast_packet(skb, hdr_size) < 0)
		return NET_RX_DROP;

	if (!check_unicast_ttvn(bat_priv, skb))
		return NET_RX_DROP;

	unicast_packet = (struct unicast_packet *)skb->data;

	
	if (is_my_mac(unicast_packet->dest)) {
		interface_rx(recv_if->soft_iface, skb, recv_if, hdr_size);
		return NET_RX_SUCCESS;
	}

	return route_unicast_packet(skb, recv_if);
}

int recv_ucast_frag_packet(struct sk_buff *skb, struct hard_iface *recv_if)
{
	struct bat_priv *bat_priv = netdev_priv(recv_if->soft_iface);
	struct unicast_frag_packet *unicast_packet;
	int hdr_size = sizeof(*unicast_packet);
	struct sk_buff *new_skb = NULL;
	int ret;

	if (check_unicast_packet(skb, hdr_size) < 0)
		return NET_RX_DROP;

	if (!check_unicast_ttvn(bat_priv, skb))
		return NET_RX_DROP;

	unicast_packet = (struct unicast_frag_packet *)skb->data;

	
	if (is_my_mac(unicast_packet->dest)) {

		ret = frag_reassemble_skb(skb, bat_priv, &new_skb);

		if (ret == NET_RX_DROP)
			return NET_RX_DROP;

		
		if (!new_skb)
			return NET_RX_SUCCESS;

		interface_rx(recv_if->soft_iface, new_skb, recv_if,
			     sizeof(struct unicast_packet));
		return NET_RX_SUCCESS;
	}

	return route_unicast_packet(skb, recv_if);
}


int recv_bcast_packet(struct sk_buff *skb, struct hard_iface *recv_if)
{
	struct bat_priv *bat_priv = netdev_priv(recv_if->soft_iface);
	struct orig_node *orig_node = NULL;
	struct bcast_packet *bcast_packet;
	struct ethhdr *ethhdr;
	int hdr_size = sizeof(*bcast_packet);
	int ret = NET_RX_DROP;
	int32_t seq_diff;

	
	if (unlikely(!pskb_may_pull(skb, hdr_size)))
		goto out;

	ethhdr = (struct ethhdr *)skb_mac_header(skb);

	
	if (!is_broadcast_ether_addr(ethhdr->h_dest))
		goto out;

	
	if (is_broadcast_ether_addr(ethhdr->h_source))
		goto out;

	
	if (is_my_mac(ethhdr->h_source))
		goto out;

	bcast_packet = (struct bcast_packet *)skb->data;

	
	if (is_my_mac(bcast_packet->orig))
		goto out;

	if (bcast_packet->header.ttl < 2)
		goto out;

	orig_node = orig_hash_find(bat_priv, bcast_packet->orig);

	if (!orig_node)
		goto out;

	spin_lock_bh(&orig_node->bcast_seqno_lock);

	
	if (get_bit_status(orig_node->bcast_bits, orig_node->last_bcast_seqno,
			   ntohl(bcast_packet->seqno)))
		goto spin_unlock;

	seq_diff = ntohl(bcast_packet->seqno) - orig_node->last_bcast_seqno;

	
	if (window_protected(bat_priv, seq_diff,
			     &orig_node->bcast_seqno_reset))
		goto spin_unlock;

	if (bit_get_packet(bat_priv, orig_node->bcast_bits, seq_diff, 1))
		orig_node->last_bcast_seqno = ntohl(bcast_packet->seqno);

	spin_unlock_bh(&orig_node->bcast_seqno_lock);

	
	add_bcast_packet_to_list(bat_priv, skb, 1);

	
	interface_rx(recv_if->soft_iface, skb, recv_if, hdr_size);
	ret = NET_RX_SUCCESS;
	goto out;

spin_unlock:
	spin_unlock_bh(&orig_node->bcast_seqno_lock);
out:
	if (orig_node)
		orig_node_free_ref(orig_node);
	return ret;
}

int recv_vis_packet(struct sk_buff *skb, struct hard_iface *recv_if)
{
	struct vis_packet *vis_packet;
	struct ethhdr *ethhdr;
	struct bat_priv *bat_priv = netdev_priv(recv_if->soft_iface);
	int hdr_size = sizeof(*vis_packet);

	
	if (skb_linearize(skb) < 0)
		return NET_RX_DROP;

	if (unlikely(!pskb_may_pull(skb, hdr_size)))
		return NET_RX_DROP;

	vis_packet = (struct vis_packet *)skb->data;
	ethhdr = (struct ethhdr *)skb_mac_header(skb);

	
	if (!is_my_mac(ethhdr->h_dest))
		return NET_RX_DROP;

	
	if (is_my_mac(vis_packet->vis_orig))
		return NET_RX_DROP;

	if (is_my_mac(vis_packet->sender_orig))
		return NET_RX_DROP;

	switch (vis_packet->vis_type) {
	case VIS_TYPE_SERVER_SYNC:
		receive_server_sync_packet(bat_priv, vis_packet,
					   skb_headlen(skb));
		break;

	case VIS_TYPE_CLIENT_UPDATE:
		receive_client_update_packet(bat_priv, vis_packet,
					     skb_headlen(skb));
		break;

	default:	
		break;
	}

	return NET_RX_DROP;
}
