/*
 * llc_s_ac.c - actions performed during sap state transition.
 *
 * Description :
 *   Functions in this module are implementation of sap component actions.
 *   Details of actions can be found in IEEE-802.2 standard document.
 *   All functions have one sap and one event as input argument. All of
 *   them return 0 On success and 1 otherwise.
 *
 * Copyright (c) 1997 by Procom Technology, Inc.
 *		 2001-2003 by Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *
 * This program can be redistributed or modified under the terms of the
 * GNU General Public License as published by the Free Software Foundation.
 * This program is distributed without any warranty or implied warranty
 * of merchantability or fitness for a particular purpose.
 *
 * See the GNU General Public License for more details.
 */

#include <linux/netdevice.h>
#include <net/llc.h>
#include <net/llc_pdu.h>
#include <net/llc_s_ac.h>
#include <net/llc_s_ev.h>
#include <net/llc_sap.h>


int llc_sap_action_unitdata_ind(struct llc_sap *sap, struct sk_buff *skb)
{
	llc_sap_rtn_pdu(sap, skb);
	return 0;
}

int llc_sap_action_send_ui(struct llc_sap *sap, struct sk_buff *skb)
{
	struct llc_sap_state_ev *ev = llc_sap_ev(skb);
	int rc;

	llc_pdu_header_init(skb, LLC_PDU_TYPE_U, ev->saddr.lsap,
			    ev->daddr.lsap, LLC_PDU_CMD);
	llc_pdu_init_as_ui_cmd(skb);
	rc = llc_mac_hdr_init(skb, ev->saddr.mac, ev->daddr.mac);
	if (likely(!rc))
		rc = dev_queue_xmit(skb);
	return rc;
}

int llc_sap_action_send_xid_c(struct llc_sap *sap, struct sk_buff *skb)
{
	struct llc_sap_state_ev *ev = llc_sap_ev(skb);
	int rc;

	llc_pdu_header_init(skb, LLC_PDU_TYPE_U, ev->saddr.lsap,
			    ev->daddr.lsap, LLC_PDU_CMD);
	llc_pdu_init_as_xid_cmd(skb, LLC_XID_NULL_CLASS_2, 0);
	rc = llc_mac_hdr_init(skb, ev->saddr.mac, ev->daddr.mac);
	if (likely(!rc))
		rc = dev_queue_xmit(skb);
	return rc;
}

int llc_sap_action_send_xid_r(struct llc_sap *sap, struct sk_buff *skb)
{
	u8 mac_da[ETH_ALEN], mac_sa[ETH_ALEN], dsap;
	int rc = 1;
	struct sk_buff *nskb;

	llc_pdu_decode_sa(skb, mac_da);
	llc_pdu_decode_da(skb, mac_sa);
	llc_pdu_decode_ssap(skb, &dsap);
	nskb = llc_alloc_frame(NULL, skb->dev, LLC_PDU_TYPE_U,
			       sizeof(struct llc_xid_info));
	if (!nskb)
		goto out;
	llc_pdu_header_init(nskb, LLC_PDU_TYPE_U, sap->laddr.lsap, dsap,
			    LLC_PDU_RSP);
	llc_pdu_init_as_xid_rsp(nskb, LLC_XID_NULL_CLASS_2, 0);
	rc = llc_mac_hdr_init(nskb, mac_sa, mac_da);
	if (likely(!rc))
		rc = dev_queue_xmit(nskb);
out:
	return rc;
}

int llc_sap_action_send_test_c(struct llc_sap *sap, struct sk_buff *skb)
{
	struct llc_sap_state_ev *ev = llc_sap_ev(skb);
	int rc;

	llc_pdu_header_init(skb, LLC_PDU_TYPE_U, ev->saddr.lsap,
			    ev->daddr.lsap, LLC_PDU_CMD);
	llc_pdu_init_as_test_cmd(skb);
	rc = llc_mac_hdr_init(skb, ev->saddr.mac, ev->daddr.mac);
	if (likely(!rc))
		rc = dev_queue_xmit(skb);
	return rc;
}

int llc_sap_action_send_test_r(struct llc_sap *sap, struct sk_buff *skb)
{
	u8 mac_da[ETH_ALEN], mac_sa[ETH_ALEN], dsap;
	struct sk_buff *nskb;
	int rc = 1;
	u32 data_size;

	llc_pdu_decode_sa(skb, mac_da);
	llc_pdu_decode_da(skb, mac_sa);
	llc_pdu_decode_ssap(skb, &dsap);

	
	data_size = ntohs(eth_hdr(skb)->h_proto) - 3;
	nskb = llc_alloc_frame(NULL, skb->dev, LLC_PDU_TYPE_U, data_size);
	if (!nskb)
		goto out;
	llc_pdu_header_init(nskb, LLC_PDU_TYPE_U, sap->laddr.lsap, dsap,
			    LLC_PDU_RSP);
	llc_pdu_init_as_test_rsp(nskb, skb);
	rc = llc_mac_hdr_init(nskb, mac_sa, mac_da);
	if (likely(!rc))
		rc = dev_queue_xmit(nskb);
out:
	return rc;
}

int llc_sap_action_report_status(struct llc_sap *sap, struct sk_buff *skb)
{
	return 0;
}

int llc_sap_action_xid_ind(struct llc_sap *sap, struct sk_buff *skb)
{
	llc_sap_rtn_pdu(sap, skb);
	return 0;
}

int llc_sap_action_test_ind(struct llc_sap *sap, struct sk_buff *skb)
{
	llc_sap_rtn_pdu(sap, skb);
	return 0;
}
