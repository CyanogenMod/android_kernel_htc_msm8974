/******************************************************************************

  Copyright(c) 2003 - 2004 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.

  Contact Information:
  James P. Ketrenos <ipw2100-admin@linux.intel.com>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

******************************************************************************

  Few modifications for Realtek's Wi-Fi drivers by
  Andrea Merello <andreamrl@tiscali.it>

  A special thanks goes to Realtek for their support !

******************************************************************************/

#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/if_arp.h>
#include <linux/in6.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/tcp.h>
#include <linux/types.h>
#include <linux/wireless.h>
#include <linux/etherdevice.h>
#include <asm/uaccess.h>
#include <linux/if_vlan.h>

#include "ieee80211.h"



static u8 P802_1H_OUI[P80211_OUI_LEN] = { 0x00, 0x00, 0xf8 };
static u8 RFC1042_OUI[P80211_OUI_LEN] = { 0x00, 0x00, 0x00 };

static inline int ieee80211_put_snap(u8 *data, u16 h_proto)
{
	struct ieee80211_snap_hdr *snap;
	u8 *oui;

	snap = (struct ieee80211_snap_hdr *)data;
	snap->dsap = 0xaa;
	snap->ssap = 0xaa;
	snap->ctrl = 0x03;

	if (h_proto == 0x8137 || h_proto == 0x80f3)
		oui = P802_1H_OUI;
	else
		oui = RFC1042_OUI;
	snap->oui[0] = oui[0];
	snap->oui[1] = oui[1];
	snap->oui[2] = oui[2];

	*(u16 *)(data + SNAP_SIZE) = htons(h_proto);

	return SNAP_SIZE + sizeof(u16);
}

int ieee80211_encrypt_fragment(
	struct ieee80211_device *ieee,
	struct sk_buff *frag,
	int hdr_len)
{
	struct ieee80211_crypt_data* crypt = ieee->crypt[ieee->tx_keyidx];
	int res;

 
        if (!crypt || !crypt->ops)
        return -1;

#ifdef CONFIG_IEEE80211_CRYPT_TKIP
	struct ieee80211_hdr_4addr *header;

	if (ieee->tkip_countermeasures &&
	    crypt && crypt->ops && strcmp(crypt->ops->name, "TKIP") == 0) {
		header = (struct ieee80211_hdr_4addr *)frag->data;
		if (net_ratelimit()) {
			printk(KERN_DEBUG "%s: TKIP countermeasures: dropped "
			       "TX packet to %pM\n",
			       ieee->dev->name, header->addr1);
		}
		return -1;
	}
#endif

	
	atomic_inc(&crypt->refcnt);
	res = 0;
	if (crypt->ops->encrypt_msdu)
		res = crypt->ops->encrypt_msdu(frag, hdr_len, crypt->priv);
	if (res == 0 && crypt->ops->encrypt_mpdu)
		res = crypt->ops->encrypt_mpdu(frag, hdr_len, crypt->priv);

	atomic_dec(&crypt->refcnt);
	if (res < 0) {
		printk(KERN_INFO "%s: Encryption failed: len=%d.\n",
		       ieee->dev->name, frag->len);
		ieee->ieee_stats.tx_discards++;
		return -1;
	}

	return 0;
}


void ieee80211_txb_free(struct ieee80211_txb *txb) {
	int i;
	if (unlikely(!txb))
		return;
	for (i = 0; i < txb->nr_frags; i++)
		if (txb->fragments[i])
			dev_kfree_skb_any(txb->fragments[i]);
	kfree(txb);
}

struct ieee80211_txb *ieee80211_alloc_txb(int nr_frags, int txb_size,
					  int gfp_mask)
{
	struct ieee80211_txb *txb;
	int i;
	txb = kmalloc(
		sizeof(struct ieee80211_txb) + (sizeof(u8*) * nr_frags),
		gfp_mask);
	if (!txb)
		return NULL;

	memset(txb, 0, sizeof(struct ieee80211_txb));
	txb->nr_frags = nr_frags;
	txb->frag_size = txb_size;

	for (i = 0; i < nr_frags; i++) {
		txb->fragments[i] = dev_alloc_skb(txb_size);
		if (unlikely(!txb->fragments[i])) {
			i--;
			break;
		}
	}
	if (unlikely(i != nr_frags)) {
		while (i >= 0)
			dev_kfree_skb_any(txb->fragments[i--]);
		kfree(txb);
		return NULL;
	}
	return txb;
}

static int
ieee80211_classify(struct sk_buff *skb, struct ieee80211_network *network)
{
  struct ether_header *eh = (struct ether_header*)skb->data;
  unsigned int wme_UP = 0;

  if(!network->QoS_Enable) {
     skb->priority = 0;
     return(wme_UP);
  }

  if(eh->ether_type == __constant_htons(ETHERTYPE_IP)) {
    const struct iphdr *ih = (struct iphdr*)(skb->data + \
		    sizeof(struct ether_header));
    wme_UP = (ih->tos >> 5)&0x07;
  } else if (vlan_tx_tag_present(skb)) {
#ifndef VLAN_PRI_SHIFT
#define VLAN_PRI_SHIFT  13              
#define VLAN_PRI_MASK   7               
#endif
	u32 tag = vlan_tx_tag_get(skb);
	wme_UP = (tag >> VLAN_PRI_SHIFT) & VLAN_PRI_MASK;
  } else if(ETH_P_PAE ==  ntohs(((struct ethhdr *)skb->data)->h_proto)) {
    
    wme_UP = 7;
  }

  skb->priority = wme_UP;
  return(wme_UP);
}

int ieee80211_rtl_xmit(struct sk_buff *skb,
		   struct net_device *dev)
{
	struct ieee80211_device *ieee = netdev_priv(dev);
	struct ieee80211_txb *txb = NULL;
	struct ieee80211_hdr_3addrqos *frag_hdr;
	int i, bytes_per_frag, nr_frags, bytes_last_frag, frag_size;
	unsigned long flags;
	struct net_device_stats *stats = &ieee->stats;
	int ether_type, encrypt;
	int bytes, fc, qos_ctl, hdr_len;
	struct sk_buff *skb_frag;
	struct ieee80211_hdr_3addrqos header = { 
		.duration_id = 0,
		.seq_ctl = 0,
		.qos_ctl = 0
	};
	u8 dest[ETH_ALEN], src[ETH_ALEN];

	struct ieee80211_crypt_data* crypt;

	
	spin_lock_irqsave(&ieee->lock, flags);

	if ((!ieee->hard_start_xmit && !(ieee->softmac_features & IEEE_SOFTMAC_TX_QUEUE))||
	   ((!ieee->softmac_data_hard_start_xmit && (ieee->softmac_features & IEEE_SOFTMAC_TX_QUEUE)))) {
		printk(KERN_WARNING "%s: No xmit handler.\n",
		       ieee->dev->name);
		goto success;
	}

	ieee80211_classify(skb,&ieee->current_network);
	if(likely(ieee->raw_tx == 0)){

		if (unlikely(skb->len < SNAP_SIZE + sizeof(u16))) {
			printk(KERN_WARNING "%s: skb too small (%d).\n",
			ieee->dev->name, skb->len);
			goto success;
		}

		ether_type = ntohs(((struct ethhdr *)skb->data)->h_proto);

		crypt = ieee->crypt[ieee->tx_keyidx];

		encrypt = !(ether_type == ETH_P_PAE && ieee->ieee802_1x) &&
			ieee->host_encrypt && crypt && crypt->ops;

		if (!encrypt && ieee->ieee802_1x &&
		ieee->drop_unencrypted && ether_type != ETH_P_PAE) {
			stats->tx_dropped++;
			goto success;
		}

	#ifdef CONFIG_IEEE80211_DEBUG
		if (crypt && !encrypt && ether_type == ETH_P_PAE) {
			struct eapol *eap = (struct eapol *)(skb->data +
				sizeof(struct ethhdr) - SNAP_SIZE - sizeof(u16));
			IEEE80211_DEBUG_EAP("TX: IEEE 802.11 EAPOL frame: %s\n",
				eap_get_type(eap->type));
		}
	#endif

		
		memcpy(&dest, skb->data, ETH_ALEN);
		memcpy(&src, skb->data+ETH_ALEN, ETH_ALEN);

		
		skb_pull(skb, sizeof(struct ethhdr));

		
		bytes = skb->len + SNAP_SIZE + sizeof(u16);

		if(ieee->current_network.QoS_Enable) {
			if (encrypt)
				fc = IEEE80211_FTYPE_DATA | IEEE80211_STYPE_QOS_DATA |
					IEEE80211_FCTL_WEP;
			else
				fc = IEEE80211_FTYPE_DATA | IEEE80211_STYPE_QOS_DATA;

		} else {
			if (encrypt)
				fc = IEEE80211_FTYPE_DATA | IEEE80211_STYPE_DATA |
					IEEE80211_FCTL_WEP;
			else
				fc = IEEE80211_FTYPE_DATA | IEEE80211_STYPE_DATA;
		}

		if (ieee->iw_mode == IW_MODE_INFRA) {
			fc |= IEEE80211_FCTL_TODS;
			memcpy(&header.addr1, ieee->current_network.bssid, ETH_ALEN);
			memcpy(&header.addr2, &src, ETH_ALEN);
			memcpy(&header.addr3, &dest, ETH_ALEN);
		} else if (ieee->iw_mode == IW_MODE_ADHOC) {
			memcpy(&header.addr1, dest, ETH_ALEN);
			memcpy(&header.addr2, src, ETH_ALEN);
			memcpy(&header.addr3, ieee->current_network.bssid, ETH_ALEN);
		}
	
		header.frame_ctl = cpu_to_le16(fc);
		

		if (is_multicast_ether_addr(header.addr1) ||
		is_broadcast_ether_addr(header.addr1)) {
			frag_size = MAX_FRAG_THRESHOLD;
			qos_ctl = QOS_CTL_NOTCONTAIN_ACK;
		}
		else {
			
			frag_size = ieee->fts;
			qos_ctl = 0;
		}

		if (ieee->current_network.QoS_Enable)	{
			hdr_len = IEEE80211_3ADDR_LEN + 2;
			
			qos_ctl |= skb->priority;
			header.qos_ctl = cpu_to_le16(qos_ctl);
		} else {
			hdr_len = IEEE80211_3ADDR_LEN;
		}

		
		bytes_per_frag = frag_size - hdr_len;
		if (ieee->config &
		(CFG_IEEE80211_COMPUTE_FCS | CFG_IEEE80211_RESERVE_FCS))
			bytes_per_frag -= IEEE80211_FCS_LEN;

		
		if (encrypt)
			bytes_per_frag -= crypt->ops->extra_prefix_len +
				crypt->ops->extra_postfix_len;

		nr_frags = bytes / bytes_per_frag;
		bytes_last_frag = bytes % bytes_per_frag;
		if (bytes_last_frag)
			nr_frags++;
		else
			bytes_last_frag = bytes_per_frag;

		txb = ieee80211_alloc_txb(nr_frags, frag_size, GFP_ATOMIC);
		if (unlikely(!txb)) {
			printk(KERN_WARNING "%s: Could not allocate TXB\n",
			ieee->dev->name);
			goto failed;
		}
		txb->encrypted = encrypt;
		txb->payload_size = bytes;

		for (i = 0; i < nr_frags; i++) {
			skb_frag = txb->fragments[i];
			skb_frag->priority = UP2AC(skb->priority);
			if (encrypt)
				skb_reserve(skb_frag, crypt->ops->extra_prefix_len);

			frag_hdr = (struct ieee80211_hdr_3addrqos *)skb_put(skb_frag, hdr_len);
			memcpy(frag_hdr, &header, hdr_len);

			if (i != nr_frags - 1) {
				frag_hdr->frame_ctl = cpu_to_le16(
					fc | IEEE80211_FCTL_MOREFRAGS);
				bytes = bytes_per_frag;

			} else {
				
				bytes = bytes_last_frag;
			}
			if(ieee->current_network.QoS_Enable) {
			  
			  frag_hdr->seq_ctl = cpu_to_le16(ieee->seq_ctrl[UP2AC(skb->priority)+1]<<4 | i);
			  
			  
			} else {
			  frag_hdr->seq_ctl = cpu_to_le16(ieee->seq_ctrl[0]<<4 | i);
			}
			
			

			
			if (i == 0) {
				ieee80211_put_snap(
					skb_put(skb_frag, SNAP_SIZE + sizeof(u16)),
					ether_type);
				bytes -= SNAP_SIZE + sizeof(u16);
			}

			memcpy(skb_put(skb_frag, bytes), skb->data, bytes);

			
			skb_pull(skb, bytes);

			if (encrypt)
				ieee80211_encrypt_fragment(ieee, skb_frag, hdr_len);
			if (ieee->config &
			(CFG_IEEE80211_COMPUTE_FCS | CFG_IEEE80211_RESERVE_FCS))
				skb_put(skb_frag, 4);
		}
		
		
		if (ieee->current_network.QoS_Enable) {
		  if (ieee->seq_ctrl[UP2AC(skb->priority) + 1] == 0xFFF)
			ieee->seq_ctrl[UP2AC(skb->priority) + 1] = 0;
		  else
			ieee->seq_ctrl[UP2AC(skb->priority) + 1]++;
		} else {
  		  if (ieee->seq_ctrl[0] == 0xFFF)
			ieee->seq_ctrl[0] = 0;
		  else
			ieee->seq_ctrl[0]++;
		}
		
	}else{
		if (unlikely(skb->len < sizeof(struct ieee80211_hdr_3addr))) {
			printk(KERN_WARNING "%s: skb too small (%d).\n",
			ieee->dev->name, skb->len);
			goto success;
		}

		txb = ieee80211_alloc_txb(1, skb->len, GFP_ATOMIC);
		if(!txb){
			printk(KERN_WARNING "%s: Could not allocate TXB\n",
			ieee->dev->name);
			goto failed;
		}

		txb->encrypted = 0;
		txb->payload_size = skb->len;
		memcpy(skb_put(txb->fragments[0],skb->len), skb->data, skb->len);
	}

 success:
	spin_unlock_irqrestore(&ieee->lock, flags);
		dev_kfree_skb_any(skb);
	if (txb) {
		if (ieee->softmac_features & IEEE_SOFTMAC_TX_QUEUE){
			ieee80211_softmac_xmit(txb, ieee);
		}else{
			if ((*ieee->hard_start_xmit)(txb, dev) == 0) {
				stats->tx_packets++;
				stats->tx_bytes += txb->payload_size;
				return NETDEV_TX_OK;
			}
			ieee80211_txb_free(txb);
		}
	}

	return NETDEV_TX_OK;

 failed:
	spin_unlock_irqrestore(&ieee->lock, flags);
	netif_stop_queue(dev);
	stats->tx_errors++;
	return NETDEV_TX_BUSY;

}
