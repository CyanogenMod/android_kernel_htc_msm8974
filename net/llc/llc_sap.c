/*
 * llc_sap.c - driver routines for SAP component.
 *
 * Copyright (c) 1997 by Procom Technology, Inc.
 * 		 2001-2003 by Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *
 * This program can be redistributed or modified under the terms of the
 * GNU General Public License as published by the Free Software Foundation.
 * This program is distributed without any warranty or implied warranty
 * of merchantability or fitness for a particular purpose.
 *
 * See the GNU General Public License for more details.
 */

#include <net/llc.h>
#include <net/llc_if.h>
#include <net/llc_conn.h>
#include <net/llc_pdu.h>
#include <net/llc_sap.h>
#include <net/llc_s_ac.h>
#include <net/llc_s_ev.h>
#include <net/llc_s_st.h>
#include <net/sock.h>
#include <net/tcp_states.h>
#include <linux/llc.h>
#include <linux/slab.h>

static int llc_mac_header_len(unsigned short devtype)
{
	switch (devtype) {
	case ARPHRD_ETHER:
	case ARPHRD_LOOPBACK:
		return sizeof(struct ethhdr);
#if defined(CONFIG_TR) || defined(CONFIG_TR_MODULE)
	case ARPHRD_IEEE802_TR:
		return sizeof(struct trh_hdr);
#endif
	}
	return 0;
}

struct sk_buff *llc_alloc_frame(struct sock *sk, struct net_device *dev,
				u8 type, u32 data_size)
{
	int hlen = type == LLC_PDU_TYPE_U ? 3 : 4;
	struct sk_buff *skb;

	hlen += llc_mac_header_len(dev->type);
	skb = alloc_skb(hlen + data_size, GFP_ATOMIC);

	if (skb) {
		skb_reset_mac_header(skb);
		skb_reserve(skb, hlen);
		skb_reset_network_header(skb);
		skb_reset_transport_header(skb);
		skb->protocol = htons(ETH_P_802_2);
		skb->dev      = dev;
		if (sk != NULL)
			skb_set_owner_w(skb, sk);
	}
	return skb;
}

void llc_save_primitive(struct sock *sk, struct sk_buff* skb, u8 prim)
{
	struct sockaddr_llc *addr;

       
	addr		  = llc_ui_skb_cb(skb);

	memset(addr, 0, sizeof(*addr));
	addr->sllc_family = sk->sk_family;
	addr->sllc_arphrd = skb->dev->type;
	addr->sllc_test   = prim == LLC_TEST_PRIM;
	addr->sllc_xid    = prim == LLC_XID_PRIM;
	addr->sllc_ua     = prim == LLC_DATAUNIT_PRIM;
	llc_pdu_decode_sa(skb, addr->sllc_mac);
	llc_pdu_decode_ssap(skb, &addr->sllc_sap);
}

void llc_sap_rtn_pdu(struct llc_sap *sap, struct sk_buff *skb)
{
	struct llc_sap_state_ev *ev = llc_sap_ev(skb);
	struct llc_pdu_un *pdu = llc_pdu_un_hdr(skb);

	switch (LLC_U_PDU_RSP(pdu)) {
	case LLC_1_PDU_CMD_TEST:
		ev->prim = LLC_TEST_PRIM;	break;
	case LLC_1_PDU_CMD_XID:
		ev->prim = LLC_XID_PRIM;	break;
	case LLC_1_PDU_CMD_UI:
		ev->prim = LLC_DATAUNIT_PRIM;	break;
	}
	ev->ind_cfm_flag = LLC_IND;
}

static struct llc_sap_state_trans *llc_find_sap_trans(struct llc_sap *sap,
						      struct sk_buff* skb)
{
	int i = 0;
	struct llc_sap_state_trans *rc = NULL;
	struct llc_sap_state_trans **next_trans;
	struct llc_sap_state *curr_state = &llc_sap_state_table[sap->state - 1];
	for (next_trans = curr_state->transitions; next_trans[i]->ev; i++)
		if (!next_trans[i]->ev(sap, skb)) {
			rc = next_trans[i]; 
			break;
		}
	return rc;
}

static int llc_exec_sap_trans_actions(struct llc_sap *sap,
				      struct llc_sap_state_trans *trans,
				      struct sk_buff *skb)
{
	int rc = 0;
	llc_sap_action_t *next_action = trans->ev_actions;

	for (; next_action && *next_action; next_action++)
		if ((*next_action)(sap, skb))
			rc = 1;
	return rc;
}

static int llc_sap_next_state(struct llc_sap *sap, struct sk_buff *skb)
{
	int rc = 1;
	struct llc_sap_state_trans *trans;

	if (sap->state > LLC_NR_SAP_STATES)
		goto out;
	trans = llc_find_sap_trans(sap, skb);
	if (!trans)
		goto out;
	rc = llc_exec_sap_trans_actions(sap, trans, skb);
	if (rc)
		goto out;
	sap->state = trans->next_state;
out:
	return rc;
}

static void llc_sap_state_process(struct llc_sap *sap, struct sk_buff *skb)
{
	struct llc_sap_state_ev *ev = llc_sap_ev(skb);

	skb_get(skb);
	ev->ind_cfm_flag = 0;
	llc_sap_next_state(sap, skb);
	if (ev->ind_cfm_flag == LLC_IND) {
		if (skb->sk->sk_state == TCP_LISTEN)
			kfree_skb(skb);
		else {
			llc_save_primitive(skb->sk, skb, ev->prim);

			
			if (sock_queue_rcv_skb(skb->sk, skb))
				kfree_skb(skb);
		}
	}
	kfree_skb(skb);
}

void llc_build_and_send_test_pkt(struct llc_sap *sap,
				 struct sk_buff *skb, u8 *dmac, u8 dsap)
{
	struct llc_sap_state_ev *ev = llc_sap_ev(skb);

	ev->saddr.lsap = sap->laddr.lsap;
	ev->daddr.lsap = dsap;
	memcpy(ev->saddr.mac, skb->dev->dev_addr, IFHWADDRLEN);
	memcpy(ev->daddr.mac, dmac, IFHWADDRLEN);

	ev->type      = LLC_SAP_EV_TYPE_PRIM;
	ev->prim      = LLC_TEST_PRIM;
	ev->prim_type = LLC_PRIM_TYPE_REQ;
	llc_sap_state_process(sap, skb);
}

void llc_build_and_send_xid_pkt(struct llc_sap *sap, struct sk_buff *skb,
				u8 *dmac, u8 dsap)
{
	struct llc_sap_state_ev *ev = llc_sap_ev(skb);

	ev->saddr.lsap = sap->laddr.lsap;
	ev->daddr.lsap = dsap;
	memcpy(ev->saddr.mac, skb->dev->dev_addr, IFHWADDRLEN);
	memcpy(ev->daddr.mac, dmac, IFHWADDRLEN);

	ev->type      = LLC_SAP_EV_TYPE_PRIM;
	ev->prim      = LLC_XID_PRIM;
	ev->prim_type = LLC_PRIM_TYPE_REQ;
	llc_sap_state_process(sap, skb);
}

static void llc_sap_rcv(struct llc_sap *sap, struct sk_buff *skb,
			struct sock *sk)
{
	struct llc_sap_state_ev *ev = llc_sap_ev(skb);

	ev->type   = LLC_SAP_EV_TYPE_PDU;
	ev->reason = 0;
	skb->sk = sk;
	llc_sap_state_process(sap, skb);
}

static inline bool llc_dgram_match(const struct llc_sap *sap,
				   const struct llc_addr *laddr,
				   const struct sock *sk)
{
     struct llc_sock *llc = llc_sk(sk);

     return sk->sk_type == SOCK_DGRAM &&
	  llc->laddr.lsap == laddr->lsap &&
	  llc_mac_match(llc->laddr.mac, laddr->mac);
}

static struct sock *llc_lookup_dgram(struct llc_sap *sap,
				     const struct llc_addr *laddr)
{
	struct sock *rc;
	struct hlist_nulls_node *node;
	int slot = llc_sk_laddr_hashfn(sap, laddr);
	struct hlist_nulls_head *laddr_hb = &sap->sk_laddr_hash[slot];

	rcu_read_lock_bh();
again:
	sk_nulls_for_each_rcu(rc, node, laddr_hb) {
		if (llc_dgram_match(sap, laddr, rc)) {
			
			if (unlikely(!atomic_inc_not_zero(&rc->sk_refcnt)))
				goto again;
			if (unlikely(llc_sk(rc)->sap != sap ||
				     !llc_dgram_match(sap, laddr, rc))) {
				sock_put(rc);
				continue;
			}
			goto found;
		}
	}
	rc = NULL;
	if (unlikely(get_nulls_value(node) != slot))
		goto again;
found:
	rcu_read_unlock_bh();
	return rc;
}

static inline bool llc_mcast_match(const struct llc_sap *sap,
				   const struct llc_addr *laddr,
				   const struct sk_buff *skb,
				   const struct sock *sk)
{
     struct llc_sock *llc = llc_sk(sk);

     return sk->sk_type == SOCK_DGRAM &&
	  llc->laddr.lsap == laddr->lsap &&
	  llc->dev == skb->dev;
}

static void llc_do_mcast(struct llc_sap *sap, struct sk_buff *skb,
			 struct sock **stack, int count)
{
	struct sk_buff *skb1;
	int i;

	for (i = 0; i < count; i++) {
		skb1 = skb_clone(skb, GFP_ATOMIC);
		if (!skb1) {
			sock_put(stack[i]);
			continue;
		}

		llc_sap_rcv(sap, skb1, stack[i]);
		sock_put(stack[i]);
	}
}

static void llc_sap_mcast(struct llc_sap *sap,
			  const struct llc_addr *laddr,
			  struct sk_buff *skb)
{
	int i = 0, count = 256 / sizeof(struct sock *);
	struct sock *sk, *stack[count];
	struct hlist_node *node;
	struct llc_sock *llc;
	struct hlist_head *dev_hb = llc_sk_dev_hash(sap, skb->dev->ifindex);

	spin_lock_bh(&sap->sk_lock);
	hlist_for_each_entry(llc, node, dev_hb, dev_hash_node) {

		sk = &llc->sk;

		if (!llc_mcast_match(sap, laddr, skb, sk))
			continue;

		sock_hold(sk);
		if (i < count)
			stack[i++] = sk;
		else {
			llc_do_mcast(sap, skb, stack, i);
			i = 0;
		}
	}
	spin_unlock_bh(&sap->sk_lock);

	llc_do_mcast(sap, skb, stack, i);
}


void llc_sap_handler(struct llc_sap *sap, struct sk_buff *skb)
{
	struct llc_addr laddr;

	llc_pdu_decode_da(skb, laddr.mac);
	llc_pdu_decode_dsap(skb, &laddr.lsap);

	if (llc_mac_multicast(laddr.mac)) {
		llc_sap_mcast(sap, &laddr, skb);
		kfree_skb(skb);
	} else {
		struct sock *sk = llc_lookup_dgram(sap, &laddr);
		if (sk) {
			llc_sap_rcv(sap, skb, sk);
			sock_put(sk);
		} else
			kfree_skb(skb);
	}
}
