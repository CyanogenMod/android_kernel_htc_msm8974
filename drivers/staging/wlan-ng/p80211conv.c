/* src/p80211/p80211conv.c
*
* Ether/802.11 conversions and packet buffer routines
*
* Copyright (C) 1999 AbsoluteValue Systems, Inc.  All Rights Reserved.
* --------------------------------------------------------------------
*
* linux-wlan
*
*   The contents of this file are subject to the Mozilla Public
*   License Version 1.1 (the "License"); you may not use this file
*   except in compliance with the License. You may obtain a copy of
*   the License at http://www.mozilla.org/MPL/
*
*   Software distributed under the License is distributed on an "AS
*   IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
*   implied. See the License for the specific language governing
*   rights and limitations under the License.
*
*   Alternatively, the contents of this file may be used under the
*   terms of the GNU Public License version 2 (the "GPL"), in which
*   case the provisions of the GPL are applicable instead of the
*   above.  If you wish to allow the use of your version of this file
*   only under the terms of the GPL and not to allow others to use
*   your version of this file under the MPL, indicate your decision
*   by deleting the provisions above and replace them with the notice
*   and other provisions required by the GPL.  If you do not delete
*   the provisions above, a recipient may use your version of this
*   file under either the MPL or the GPL.
*
* --------------------------------------------------------------------
*
* Inquiries regarding the linux-wlan Open Source project can be
* made directly to:
*
* AbsoluteValue Systems Inc.
* info@linux-wlan.com
* http://www.linux-wlan.com
*
* --------------------------------------------------------------------
*
* Portions of the development of this software were funded by
* Intersil Corporation as part of PRISM(R) chipset product development.
*
* --------------------------------------------------------------------
*
* This file defines the functions that perform Ethernet to/from
* 802.11 frame conversions.
*
* --------------------------------------------------------------------
*
*================================================================ */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/wireless.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/byteorder/generic.h>

#include <asm/byteorder.h>

#include "p80211types.h"
#include "p80211hdr.h"
#include "p80211conv.h"
#include "p80211mgmt.h"
#include "p80211msg.h"
#include "p80211netdev.h"
#include "p80211ioctl.h"
#include "p80211req.h"

static u8 oui_rfc1042[] = { 0x00, 0x00, 0x00 };
static u8 oui_8021h[] = { 0x00, 0x00, 0xf8 };

int skb_ether_to_p80211(wlandevice_t *wlandev, u32 ethconv,
			struct sk_buff *skb, union p80211_hdr *p80211_hdr,
			struct p80211_metawep *p80211_wep)
{

	u16 fc;
	u16 proto;
	struct wlan_ethhdr e_hdr;
	struct wlan_llc *e_llc;
	struct wlan_snap *e_snap;
	int foo;

	memcpy(&e_hdr, skb->data, sizeof(e_hdr));

	if (skb->len <= 0) {
		pr_debug("zero-length skb!\n");
		return 1;
	}

	if (ethconv == WLAN_ETHCONV_ENCAP) {	
		pr_debug("ENCAP len: %d\n", skb->len);
		
		
		
	} else {
		
		proto = ntohs(e_hdr.type);
		if (proto <= 1500) {
			pr_debug("802.3 len: %d\n", skb->len);
			
			

			
			skb_pull(skb, WLAN_ETHHDR_LEN);

			
			skb_trim(skb, proto);
		} else {
			pr_debug("DIXII len: %d\n", skb->len);
			

			
			skb_pull(skb, WLAN_ETHHDR_LEN);

			
			e_snap =
			    (struct wlan_snap *) skb_push(skb,
				sizeof(struct wlan_snap));
			e_snap->type = htons(proto);
			if (ethconv == WLAN_ETHCONV_8021h
			    && p80211_stt_findproto(proto)) {
				memcpy(e_snap->oui, oui_8021h,
				       WLAN_IEEE_OUI_LEN);
			} else {
				memcpy(e_snap->oui, oui_rfc1042,
				       WLAN_IEEE_OUI_LEN);
			}

			
			e_llc =
			    (struct wlan_llc *) skb_push(skb,
				sizeof(struct wlan_llc));
			e_llc->dsap = 0xAA;	
			e_llc->ssap = 0xAA;
			e_llc->ctl = 0x03;

		}
	}

	
	
	fc = cpu_to_le16(WLAN_SET_FC_FTYPE(WLAN_FTYPE_DATA) |
			 WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_DATAONLY));

	switch (wlandev->macmode) {
	case WLAN_MACMODE_IBSS_STA:
		memcpy(p80211_hdr->a3.a1, &e_hdr.daddr, ETH_ALEN);
		memcpy(p80211_hdr->a3.a2, wlandev->netdev->dev_addr, ETH_ALEN);
		memcpy(p80211_hdr->a3.a3, wlandev->bssid, ETH_ALEN);
		break;
	case WLAN_MACMODE_ESS_STA:
		fc |= cpu_to_le16(WLAN_SET_FC_TODS(1));
		memcpy(p80211_hdr->a3.a1, wlandev->bssid, ETH_ALEN);
		memcpy(p80211_hdr->a3.a2, wlandev->netdev->dev_addr, ETH_ALEN);
		memcpy(p80211_hdr->a3.a3, &e_hdr.daddr, ETH_ALEN);
		break;
	case WLAN_MACMODE_ESS_AP:
		fc |= cpu_to_le16(WLAN_SET_FC_FROMDS(1));
		memcpy(p80211_hdr->a3.a1, &e_hdr.daddr, ETH_ALEN);
		memcpy(p80211_hdr->a3.a2, wlandev->bssid, ETH_ALEN);
		memcpy(p80211_hdr->a3.a3, &e_hdr.saddr, ETH_ALEN);
		break;
	default:
		printk(KERN_ERR
		       "Error: Converting eth to wlan in unknown mode.\n");
		return 1;
		break;
	}

	p80211_wep->data = NULL;

	if ((wlandev->hostwep & HOSTWEP_PRIVACYINVOKED)
	    && (wlandev->hostwep & HOSTWEP_ENCRYPT)) {
		

		p80211_wep->data = kmalloc(skb->len, GFP_ATOMIC);
		foo = wep_encrypt(wlandev, skb->data, p80211_wep->data,
				  skb->len,
				  (wlandev->hostwep & HOSTWEP_DEFAULTKEY_MASK),
				  p80211_wep->iv, p80211_wep->icv);
		if (foo) {
			printk(KERN_WARNING
			       "Host en-WEP failed, dropping frame (%d).\n",
			       foo);
			return 2;
		}
		fc |= cpu_to_le16(WLAN_SET_FC_ISWEP(1));
	}

	

	p80211_hdr->a3.fc = fc;
	p80211_hdr->a3.dur = 0;
	p80211_hdr->a3.seq = 0;

	return 0;
}

static void orinoco_spy_gather(wlandevice_t *wlandev, char *mac,
			       struct p80211_rxmeta *rxmeta)
{
	int i;


	for (i = 0; i < wlandev->spy_number; i++) {

		if (!memcmp(wlandev->spy_address[i], mac, ETH_ALEN)) {
			memcpy(wlandev->spy_address[i], mac, ETH_ALEN);
			wlandev->spy_stat[i].level = rxmeta->signal;
			wlandev->spy_stat[i].noise = rxmeta->noise;
			wlandev->spy_stat[i].qual =
			    (rxmeta->signal >
			     rxmeta->noise) ? (rxmeta->signal -
					       rxmeta->noise) : 0;
			wlandev->spy_stat[i].updated = 0x7;
		}
	}
}

int skb_p80211_to_ether(wlandevice_t *wlandev, u32 ethconv,
			struct sk_buff *skb)
{
	netdevice_t *netdev = wlandev->netdev;
	u16 fc;
	unsigned int payload_length;
	unsigned int payload_offset;
	u8 daddr[WLAN_ETHADDR_LEN];
	u8 saddr[WLAN_ETHADDR_LEN];
	union p80211_hdr *w_hdr;
	struct wlan_ethhdr *e_hdr;
	struct wlan_llc *e_llc;
	struct wlan_snap *e_snap;

	int foo;

	payload_length = skb->len - WLAN_HDR_A3_LEN - WLAN_CRC_LEN;
	payload_offset = WLAN_HDR_A3_LEN;

	w_hdr = (union p80211_hdr *) skb->data;

	
	fc = le16_to_cpu(w_hdr->a3.fc);
	if ((WLAN_GET_FC_TODS(fc) == 0) && (WLAN_GET_FC_FROMDS(fc) == 0)) {
		memcpy(daddr, w_hdr->a3.a1, WLAN_ETHADDR_LEN);
		memcpy(saddr, w_hdr->a3.a2, WLAN_ETHADDR_LEN);
	} else if ((WLAN_GET_FC_TODS(fc) == 0)
			&& (WLAN_GET_FC_FROMDS(fc) == 1)) {
		memcpy(daddr, w_hdr->a3.a1, WLAN_ETHADDR_LEN);
		memcpy(saddr, w_hdr->a3.a3, WLAN_ETHADDR_LEN);
	} else if ((WLAN_GET_FC_TODS(fc) == 1)
			&& (WLAN_GET_FC_FROMDS(fc) == 0)) {
		memcpy(daddr, w_hdr->a3.a3, WLAN_ETHADDR_LEN);
		memcpy(saddr, w_hdr->a3.a2, WLAN_ETHADDR_LEN);
	} else {
		payload_offset = WLAN_HDR_A4_LEN;
		if (payload_length < WLAN_HDR_A4_LEN - WLAN_HDR_A3_LEN) {
			printk(KERN_ERR "A4 frame too short!\n");
			return 1;
		}
		payload_length -= (WLAN_HDR_A4_LEN - WLAN_HDR_A3_LEN);
		memcpy(daddr, w_hdr->a4.a3, WLAN_ETHADDR_LEN);
		memcpy(saddr, w_hdr->a4.a4, WLAN_ETHADDR_LEN);
	}

	
	if ((wlandev->hostwep & HOSTWEP_PRIVACYINVOKED) && WLAN_GET_FC_ISWEP(fc)
	    && (wlandev->hostwep & HOSTWEP_DECRYPT)) {
		if (payload_length <= 8) {
			printk(KERN_ERR "WEP frame too short (%u).\n",
			       skb->len);
			return 1;
		}
		foo = wep_decrypt(wlandev, skb->data + payload_offset + 4,
				       payload_length - 8, -1,
				       skb->data + payload_offset,
				       skb->data + payload_offset +
				       payload_length - 4);
		if (foo) {
			
			pr_debug("Host de-WEP failed, dropping frame (%d).\n",
				 foo);
			wlandev->rx.decrypt_err++;
			return 2;
		}

		
		payload_length -= 8;
		
		skb_pull(skb, 4);
		
		skb_trim(skb, skb->len - 4);

		wlandev->rx.decrypt++;
	}

	e_hdr = (struct wlan_ethhdr *) (skb->data + payload_offset);

	e_llc = (struct wlan_llc *) (skb->data + payload_offset);
	e_snap =
	    (struct wlan_snap *) (skb->data + payload_offset +
		sizeof(struct wlan_llc));

	
	if ((payload_length >= sizeof(struct wlan_ethhdr)) &&
	    (e_llc->dsap != 0xaa || e_llc->ssap != 0xaa) &&
	    ((memcmp(daddr, e_hdr->daddr, WLAN_ETHADDR_LEN) == 0) ||
	     (memcmp(saddr, e_hdr->saddr, WLAN_ETHADDR_LEN) == 0))) {
		pr_debug("802.3 ENCAP len: %d\n", payload_length);
		
		
		if (payload_length > (netdev->mtu + WLAN_ETHHDR_LEN)) {
			
			
			printk(KERN_ERR "ENCAP frame too large (%d > %d)\n",
			       payload_length, netdev->mtu + WLAN_ETHHDR_LEN);
			return 1;
		}

		
		skb_pull(skb, payload_offset);
		
		skb_trim(skb, skb->len - WLAN_CRC_LEN);

	} else if ((payload_length >= sizeof(struct wlan_llc) +
		sizeof(struct wlan_snap))
		&& (e_llc->dsap == 0xaa)
		&& (e_llc->ssap == 0xaa)
		&& (e_llc->ctl == 0x03)
		   &&
		   (((memcmp(e_snap->oui, oui_rfc1042, WLAN_IEEE_OUI_LEN) == 0)
		     && (ethconv == WLAN_ETHCONV_8021h)
		     && (p80211_stt_findproto(le16_to_cpu(e_snap->type))))
		    || (memcmp(e_snap->oui, oui_rfc1042, WLAN_IEEE_OUI_LEN) !=
			0))) {
		pr_debug("SNAP+RFC1042 len: %d\n", payload_length);
		
		

		
		if (payload_length > netdev->mtu) {
			
			
			printk(KERN_ERR "SNAP frame too large (%d > %d)\n",
			       payload_length, netdev->mtu);
			return 1;
		}

		
		skb_pull(skb, payload_offset);

		
		e_hdr = (struct wlan_ethhdr *) skb_push(skb, WLAN_ETHHDR_LEN);
		memcpy(e_hdr->daddr, daddr, WLAN_ETHADDR_LEN);
		memcpy(e_hdr->saddr, saddr, WLAN_ETHADDR_LEN);
		e_hdr->type = htons(payload_length);

		
		skb_trim(skb, skb->len - WLAN_CRC_LEN);

	} else if ((payload_length >= sizeof(struct wlan_llc) +
		sizeof(struct wlan_snap))
		&& (e_llc->dsap == 0xaa)
		&& (e_llc->ssap == 0xaa)
		&& (e_llc->ctl == 0x03)) {
		pr_debug("802.1h/RFC1042 len: %d\n", payload_length);

		
		if ((payload_length - sizeof(struct wlan_llc) -
			sizeof(struct wlan_snap))
			> netdev->mtu) {
			
			
			printk(KERN_ERR "DIXII frame too large (%ld > %d)\n",
			       (long int)(payload_length -
					sizeof(struct wlan_llc) -
					sizeof(struct wlan_snap)), netdev->mtu);
			return 1;
		}

		
		skb_pull(skb, payload_offset);

		
		skb_pull(skb, sizeof(struct wlan_llc));

		
		skb_pull(skb, sizeof(struct wlan_snap));

		
		e_hdr = (struct wlan_ethhdr *) skb_push(skb, WLAN_ETHHDR_LEN);
		e_hdr->type = e_snap->type;
		memcpy(e_hdr->daddr, daddr, WLAN_ETHADDR_LEN);
		memcpy(e_hdr->saddr, saddr, WLAN_ETHADDR_LEN);

		
		skb_trim(skb, skb->len - WLAN_CRC_LEN);
	} else {
		pr_debug("NON-ENCAP len: %d\n", payload_length);
		
		
		
		

		
		if (payload_length > netdev->mtu) {
			
			
			printk(KERN_ERR "OTHER frame too large (%d > %d)\n",
			       payload_length, netdev->mtu);
			return 1;
		}

		
		skb_pull(skb, payload_offset);

		
		e_hdr = (struct wlan_ethhdr *) skb_push(skb, WLAN_ETHHDR_LEN);
		memcpy(e_hdr->daddr, daddr, WLAN_ETHADDR_LEN);
		memcpy(e_hdr->saddr, saddr, WLAN_ETHADDR_LEN);
		e_hdr->type = htons(payload_length);

		
		skb_trim(skb, skb->len - WLAN_CRC_LEN);

	}

	skb->protocol = eth_type_trans(skb, netdev);

	
	
	if (wlandev->spy_number)
		orinoco_spy_gather(wlandev, eth_hdr(skb)->h_source,
				   P80211SKB_RXMETA(skb));

	
	p80211skb_rxmeta_detach(skb);

	return 0;
}

int p80211_stt_findproto(u16 proto)
{
	
	

	if (proto == 0x80f3)	
		return 1;

	return 0;
}

void p80211skb_rxmeta_detach(struct sk_buff *skb)
{
	struct p80211_rxmeta *rxmeta;
	struct p80211_frmmeta *frmmeta;

	
	if (skb == NULL) {	
		pr_debug("Called w/ null skb.\n");
		goto exit;
	}
	frmmeta = P80211SKB_FRMMETA(skb);
	if (frmmeta == NULL) {	
		pr_debug("Called w/ bad frmmeta magic.\n");
		goto exit;
	}
	rxmeta = frmmeta->rx;
	if (rxmeta == NULL) {	
		pr_debug("Called w/ bad rxmeta ptr.\n");
		goto exit;
	}

	
	kfree(rxmeta);

	
	memset(skb->cb, 0, sizeof(skb->cb));
exit:
	return;
}

int p80211skb_rxmeta_attach(struct wlandevice *wlandev, struct sk_buff *skb)
{
	int result = 0;
	struct p80211_rxmeta *rxmeta;
	struct p80211_frmmeta *frmmeta;

	
	if (P80211SKB_RXMETA(skb) != NULL) {
		printk(KERN_ERR "%s: RXmeta already attached!\n",
		       wlandev->name);
		result = 0;
		goto exit;
	}

	
	rxmeta = kzalloc(sizeof(struct p80211_rxmeta), GFP_ATOMIC);

	if (rxmeta == NULL) {
		printk(KERN_ERR "%s: Failed to allocate rxmeta.\n",
		       wlandev->name);
		result = 1;
		goto exit;
	}

	
	rxmeta->wlandev = wlandev;
	rxmeta->hosttime = jiffies;

	
	memset(skb->cb, 0, sizeof(struct p80211_frmmeta));
	frmmeta = (struct p80211_frmmeta *) (skb->cb);
	frmmeta->magic = P80211_FRMMETA_MAGIC;
	frmmeta->rx = rxmeta;
exit:
	return result;
}

void p80211skb_free(struct wlandevice *wlandev, struct sk_buff *skb)
{
	struct p80211_frmmeta *meta;

	meta = P80211SKB_FRMMETA(skb);
	if (meta && meta->rx)
		p80211skb_rxmeta_detach(skb);
	else
		printk(KERN_ERR "Freeing an skb (%p) w/ no frmmeta.\n", skb);
	dev_kfree_skb(skb);
	return;
}
