/*
 * Copyright 2011, Siemens AG
 * written by Alexander Smirnov <alex.bluesman.smirnov@gmail.com>
 */

/*
 * Based on patches from Jon Smirl <jonsmirl@gmail.com>
 * Copyright (c) 2011 Jon Smirl <jonsmirl@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* Jon's code is based on 6lowpan implementation for Contiki which is:
 * Copyright (c) 2008, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <linux/bitops.h>
#include <linux/if_arp.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <net/af_ieee802154.h>
#include <net/ieee802154.h>
#include <net/ieee802154_netdev.h>
#include <net/ipv6.h>

#include "6lowpan.h"

static const u8 lowpan_ttl_values[] = {0, 1, 64, 255};

static LIST_HEAD(lowpan_devices);

static const u8 lowpan_unc_llconf[] = {0x0f, 0x28, 0x22, 0x20};

static const u8 lowpan_unc_ctxconf[] = {0x00, 0x88, 0x82, 0x80};

static const u8 lowpan_unc_mxconf[] = {0x0f, 0x25, 0x23, 0x21};

static const u8 lowpan_llprefix[] = {0xfe, 0x80};

struct lowpan_dev_info {
	struct net_device	*real_dev; 
	struct mutex		dev_list_mtx; 
};

struct lowpan_dev_record {
	struct net_device *ldev;
	struct list_head list;
};

struct lowpan_fragment {
	struct sk_buff		*skb;		
	spinlock_t		lock;		
	u16			length;		
	u32			bytes_rcv;	
	u16			tag;		
	struct timer_list	timer;		
	struct list_head	list;		
};

static unsigned short fragment_tag;
static LIST_HEAD(lowpan_fragments);
spinlock_t flist_lock;

static inline struct
lowpan_dev_info *lowpan_dev_info(const struct net_device *dev)
{
	return netdev_priv(dev);
}

static inline void lowpan_address_flip(u8 *src, u8 *dest)
{
	int i;
	for (i = 0; i < IEEE802154_ADDR_LEN; i++)
		(dest)[IEEE802154_ADDR_LEN - i - 1] = (src)[i];
}

static inline void lowpan_raw_dump_inline(const char *caller, char *msg,
				   unsigned char *buf, int len)
{
#ifdef DEBUG
	if (msg)
		pr_debug("(%s) %s: ", caller, msg);
	print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_NONE,
		       16, 1, buf, len, false);
#endif 
}

static inline void lowpan_raw_dump_table(const char *caller, char *msg,
				   unsigned char *buf, int len)
{
#ifdef DEBUG
	if (msg)
		pr_debug("(%s) %s:\n", caller, msg);
	print_hex_dump(KERN_DEBUG, "\t", DUMP_PREFIX_OFFSET,
		       16, 1, buf, len, false);
#endif 
}

static u8
lowpan_compress_addr_64(u8 **hc06_ptr, u8 shift, const struct in6_addr *ipaddr,
		 const unsigned char *lladdr)
{
	u8 val = 0;

	if (is_addr_mac_addr_based(ipaddr, lladdr))
		val = 3; 
	else if (lowpan_is_iid_16_bit_compressable(ipaddr)) {
		
		memcpy(*hc06_ptr, &ipaddr->s6_addr16[7], 2);
		*hc06_ptr += 2;
		val = 2; 
	} else {
		
		memcpy(*hc06_ptr, &ipaddr->s6_addr16[4], 8);
		*hc06_ptr += 8;
		val = 1; 
	}

	return rol8(val, shift);
}

static void
lowpan_uip_ds6_set_addr_iid(struct in6_addr *ipaddr, unsigned char *lladdr)
{
	memcpy(&ipaddr->s6_addr[8], lladdr, IEEE802154_ALEN);
	
	ipaddr->s6_addr[8] ^= 0x02;
}

static int
lowpan_uncompress_addr(struct sk_buff *skb, struct in6_addr *ipaddr,
	u8 const *prefix, u8 pref_post_count, unsigned char *lladdr)
{
	u8 prefcount = pref_post_count >> 4;
	u8 postcount = pref_post_count & 0x0f;

	
	prefcount = (prefcount == 15 ? 16 : prefcount);
	postcount = (postcount == 15 ? 16 : postcount);

	if (lladdr)
		lowpan_raw_dump_inline(__func__, "linklocal address",
						lladdr,	IEEE802154_ALEN);
	if (prefcount > 0)
		memcpy(ipaddr, prefix, prefcount);

	if (prefcount + postcount < 16)
		memset(&ipaddr->s6_addr[prefcount], 0,
					16 - (prefcount + postcount));

	if (postcount > 0) {
		memcpy(&ipaddr->s6_addr[16 - postcount], skb->data, postcount);
		skb_pull(skb, postcount);
	} else if (prefcount > 0) {
		if (lladdr == NULL)
			return -EINVAL;

		
		lowpan_uip_ds6_set_addr_iid(ipaddr, lladdr);
	}

	pr_debug("(%s): uncompressing %d + %d => ", __func__, prefcount,
								postcount);
	lowpan_raw_dump_inline(NULL, NULL, ipaddr->s6_addr, 16);

	return 0;
}

static void
lowpan_compress_udp_header(u8 **hc06_ptr, struct sk_buff *skb)
{
	struct udphdr *uh = udp_hdr(skb);

	pr_debug("(%s): UDP header compression\n", __func__);

	if (((uh->source & LOWPAN_NHC_UDP_4BIT_MASK) ==
				LOWPAN_NHC_UDP_4BIT_PORT) &&
	    ((uh->dest & LOWPAN_NHC_UDP_4BIT_MASK) ==
				LOWPAN_NHC_UDP_4BIT_PORT)) {
		pr_debug("(%s): both ports compression to 4 bits\n", __func__);
		**hc06_ptr = LOWPAN_NHC_UDP_CS_P_11;
		**(hc06_ptr + 1) = 
		   (u8)((uh->dest - LOWPAN_NHC_UDP_4BIT_PORT) +
		       ((uh->source & LOWPAN_NHC_UDP_4BIT_PORT) << 4));
		*hc06_ptr += 2;
	} else if ((uh->dest & LOWPAN_NHC_UDP_8BIT_MASK) ==
			LOWPAN_NHC_UDP_8BIT_PORT) {
		pr_debug("(%s): remove 8 bits of dest\n", __func__);
		**hc06_ptr = LOWPAN_NHC_UDP_CS_P_01;
		memcpy(*hc06_ptr + 1, &uh->source, 2);
		**(hc06_ptr + 3) = (u8)(uh->dest - LOWPAN_NHC_UDP_8BIT_PORT);
		*hc06_ptr += 4;
	} else if ((uh->source & LOWPAN_NHC_UDP_8BIT_MASK) ==
			LOWPAN_NHC_UDP_8BIT_PORT) {
		pr_debug("(%s): remove 8 bits of source\n", __func__);
		**hc06_ptr = LOWPAN_NHC_UDP_CS_P_10;
		memcpy(*hc06_ptr + 1, &uh->dest, 2);
		**(hc06_ptr + 3) = (u8)(uh->source - LOWPAN_NHC_UDP_8BIT_PORT);
		*hc06_ptr += 4;
	} else {
		pr_debug("(%s): can't compress header\n", __func__);
		**hc06_ptr = LOWPAN_NHC_UDP_CS_P_00;
		memcpy(*hc06_ptr + 1, &uh->source, 2);
		memcpy(*hc06_ptr + 3, &uh->dest, 2);
		*hc06_ptr += 5;
	}

	
	memcpy(*hc06_ptr, &uh->check, 2);
	*hc06_ptr += 2;
}

static u8 lowpan_fetch_skb_u8(struct sk_buff *skb)
{
	u8 ret;

	ret = skb->data[0];
	skb_pull(skb, 1);

	return ret;
}

static u16 lowpan_fetch_skb_u16(struct sk_buff *skb)
{
	u16 ret;

	BUG_ON(!pskb_may_pull(skb, 2));

	ret = skb->data[0] | (skb->data[1] << 8);
	skb_pull(skb, 2);
	return ret;
}

static int
lowpan_uncompress_udp_header(struct sk_buff *skb)
{
	struct udphdr *uh = udp_hdr(skb);
	u8 tmp;

	tmp = lowpan_fetch_skb_u8(skb);

	if ((tmp & LOWPAN_NHC_UDP_MASK) == LOWPAN_NHC_UDP_ID) {
		pr_debug("(%s): UDP header uncompression\n", __func__);
		switch (tmp & LOWPAN_NHC_UDP_CS_P_11) {
		case LOWPAN_NHC_UDP_CS_P_00:
			memcpy(&uh->source, &skb->data[0], 2);
			memcpy(&uh->dest, &skb->data[2], 2);
			skb_pull(skb, 4);
			break;
		case LOWPAN_NHC_UDP_CS_P_01:
			memcpy(&uh->source, &skb->data[0], 2);
			uh->dest =
			   skb->data[2] + LOWPAN_NHC_UDP_8BIT_PORT;
			skb_pull(skb, 3);
			break;
		case LOWPAN_NHC_UDP_CS_P_10:
			uh->source = skb->data[0] + LOWPAN_NHC_UDP_8BIT_PORT;
			memcpy(&uh->dest, &skb->data[1], 2);
			skb_pull(skb, 3);
			break;
		case LOWPAN_NHC_UDP_CS_P_11:
			uh->source =
			   LOWPAN_NHC_UDP_4BIT_PORT + (skb->data[0] >> 4);
			uh->dest =
			   LOWPAN_NHC_UDP_4BIT_PORT + (skb->data[0] & 0x0f);
			skb_pull(skb, 1);
			break;
		default:
			pr_debug("(%s) ERROR: unknown UDP format\n", __func__);
			goto err;
			break;
		}

		pr_debug("(%s): uncompressed UDP ports: src = %d, dst = %d\n",
					__func__, uh->source, uh->dest);

		
		memcpy(&uh->check, &skb->data[0], 2);
		skb_pull(skb, 2);
	} else {
		pr_debug("(%s): ERROR: unsupported NH format\n", __func__);
		goto err;
	}

	return 0;
err:
	return -EINVAL;
}

static int lowpan_header_create(struct sk_buff *skb,
			   struct net_device *dev,
			   unsigned short type, const void *_daddr,
			   const void *_saddr, unsigned len)
{
	u8 tmp, iphc0, iphc1, *hc06_ptr;
	struct ipv6hdr *hdr;
	const u8 *saddr = _saddr;
	const u8 *daddr = _daddr;
	u8 *head;
	struct ieee802154_addr sa, da;

	if (type != ETH_P_IPV6)
		return 0;
	head = kzalloc(100, GFP_KERNEL);
	if (head == NULL)
		return -ENOMEM;

	hdr = ipv6_hdr(skb);
	hc06_ptr = head + 2;

	pr_debug("(%s): IPv6 header dump:\n\tversion = %d\n\tlength  = %d\n"
		 "\tnexthdr = 0x%02x\n\thop_lim = %d\n", __func__,
		hdr->version, ntohs(hdr->payload_len), hdr->nexthdr,
		hdr->hop_limit);

	lowpan_raw_dump_table(__func__, "raw skb network header dump",
		skb_network_header(skb), sizeof(struct ipv6hdr));

	if (!saddr)
		saddr = dev->dev_addr;

	lowpan_raw_dump_inline(__func__, "saddr", (unsigned char *)saddr, 8);

	iphc0 = LOWPAN_DISPATCH_IPHC;
	iphc1 = 0;

	

	lowpan_raw_dump_inline(__func__, "daddr", (unsigned char *)daddr, 8);


	
	tmp = (hdr->priority << 4) | (hdr->flow_lbl[0] >> 4);
	tmp = ((tmp & 0x03) << 6) | (tmp >> 2);

	if (((hdr->flow_lbl[0] & 0x0F) == 0) &&
	     (hdr->flow_lbl[1] == 0) && (hdr->flow_lbl[2] == 0)) {
		
		iphc0 |= LOWPAN_IPHC_FL_C;
		if ((hdr->priority == 0) &&
		   ((hdr->flow_lbl[0] & 0xF0) == 0)) {
			
			iphc0 |= LOWPAN_IPHC_TC_C;
		} else {
			
			*hc06_ptr = tmp;
			hc06_ptr += 1;
		}
	} else {
		
		if ((hdr->priority == 0) &&
		   ((hdr->flow_lbl[0] & 0xF0) == 0)) {
			
			iphc0 |= LOWPAN_IPHC_TC_C;
			*hc06_ptr = (tmp & 0xc0) | (hdr->flow_lbl[0] & 0x0F);
			memcpy(hc06_ptr + 1, &hdr->flow_lbl[1], 2);
			hc06_ptr += 3;
		} else {
			
			memcpy(hc06_ptr, &hdr, 4);
			
			*hc06_ptr = tmp;
			hc06_ptr += 4;
		}
	}

	

	
	if (hdr->nexthdr == UIP_PROTO_UDP)
		iphc0 |= LOWPAN_IPHC_NH_C;

	if ((iphc0 & LOWPAN_IPHC_NH_C) == 0) {
		*hc06_ptr = hdr->nexthdr;
		hc06_ptr += 1;
	}

	switch (hdr->hop_limit) {
	case 1:
		iphc0 |= LOWPAN_IPHC_TTL_1;
		break;
	case 64:
		iphc0 |= LOWPAN_IPHC_TTL_64;
		break;
	case 255:
		iphc0 |= LOWPAN_IPHC_TTL_255;
		break;
	default:
		*hc06_ptr = hdr->hop_limit;
		break;
	}

	
	if (is_addr_unspecified(&hdr->saddr)) {
		pr_debug("(%s): source address is unspecified, setting SAC\n",
								__func__);
		iphc1 |= LOWPAN_IPHC_SAC;
	
	} else if (is_addr_link_local(&hdr->saddr)) {
		pr_debug("(%s): source address is link-local\n", __func__);
		iphc1 |= lowpan_compress_addr_64(&hc06_ptr,
				LOWPAN_IPHC_SAM_BIT, &hdr->saddr, saddr);
	} else {
		pr_debug("(%s): send the full source address\n", __func__);
		memcpy(hc06_ptr, &hdr->saddr.s6_addr16[0], 16);
		hc06_ptr += 16;
	}

	
	if (is_addr_mcast(&hdr->daddr)) {
		pr_debug("(%s): destination address is multicast", __func__);
		iphc1 |= LOWPAN_IPHC_M;
		if (lowpan_is_mcast_addr_compressable8(&hdr->daddr)) {
			pr_debug("compressed to 1 octet\n");
			iphc1 |= LOWPAN_IPHC_DAM_11;
			
			*hc06_ptr = hdr->daddr.s6_addr[15];
			hc06_ptr += 1;
		} else if (lowpan_is_mcast_addr_compressable32(&hdr->daddr)) {
			pr_debug("compressed to 4 octets\n");
			iphc1 |= LOWPAN_IPHC_DAM_10;
			
			*hc06_ptr = hdr->daddr.s6_addr[1];
			memcpy(hc06_ptr + 1, &hdr->daddr.s6_addr[13], 3);
			hc06_ptr += 4;
		} else if (lowpan_is_mcast_addr_compressable48(&hdr->daddr)) {
			pr_debug("compressed to 6 octets\n");
			iphc1 |= LOWPAN_IPHC_DAM_01;
			
			*hc06_ptr = hdr->daddr.s6_addr[1];
			memcpy(hc06_ptr + 1, &hdr->daddr.s6_addr[11], 5);
			hc06_ptr += 6;
		} else {
			pr_debug("using full address\n");
			iphc1 |= LOWPAN_IPHC_DAM_00;
			memcpy(hc06_ptr, &hdr->daddr.s6_addr[0], 16);
			hc06_ptr += 16;
		}
	} else {
		pr_debug("(%s): destination address is unicast: ", __func__);
		
		if (is_addr_link_local(&hdr->daddr)) {
			pr_debug("destination address is link-local\n");
			iphc1 |= lowpan_compress_addr_64(&hc06_ptr,
				LOWPAN_IPHC_DAM_BIT, &hdr->daddr, daddr);
		} else {
			pr_debug("using full address\n");
			memcpy(hc06_ptr, &hdr->daddr.s6_addr16[0], 16);
			hc06_ptr += 16;
		}
	}

	
	if (hdr->nexthdr == UIP_PROTO_UDP)
		lowpan_compress_udp_header(&hc06_ptr, skb);

	head[0] = iphc0;
	head[1] = iphc1;

	skb_pull(skb, sizeof(struct ipv6hdr));
	memcpy(skb_push(skb, hc06_ptr - head), head, hc06_ptr - head);

	kfree(head);

	lowpan_raw_dump_table(__func__, "raw skb data dump", skb->data,
				skb->len);

	{
		
		sa.addr_type = IEEE802154_ADDR_LONG;
		sa.pan_id = 0xff;

		da.addr_type = IEEE802154_ADDR_LONG;
		da.pan_id = 0xff;

		memcpy(&(da.hwaddr), daddr, 8);
		memcpy(&(sa.hwaddr), saddr, 8);

		mac_cb(skb)->flags = IEEE802154_FC_TYPE_DATA;

		return dev_hard_header(skb, lowpan_dev_info(dev)->real_dev,
				type, (void *)&da, (void *)&sa, skb->len);
	}
}

static int lowpan_skb_deliver(struct sk_buff *skb, struct ipv6hdr *hdr)
{
	struct sk_buff *new;
	struct lowpan_dev_record *entry;
	int stat = NET_RX_SUCCESS;

	new = skb_copy_expand(skb, sizeof(struct ipv6hdr), skb_tailroom(skb),
								GFP_ATOMIC);
	kfree_skb(skb);

	if (!new)
		return -ENOMEM;

	skb_push(new, sizeof(struct ipv6hdr));
	skb_reset_network_header(new);
	skb_copy_to_linear_data(new, hdr, sizeof(struct ipv6hdr));

	new->protocol = htons(ETH_P_IPV6);
	new->pkt_type = PACKET_HOST;

	rcu_read_lock();
	list_for_each_entry_rcu(entry, &lowpan_devices, list)
		if (lowpan_dev_info(entry->ldev)->real_dev == new->dev) {
			skb = skb_copy(new, GFP_ATOMIC);
			if (!skb) {
				stat = -ENOMEM;
				break;
			}

			skb->dev = entry->ldev;
			stat = netif_rx(skb);
		}
	rcu_read_unlock();

	kfree_skb(new);

	return stat;
}

static void lowpan_fragment_timer_expired(unsigned long entry_addr)
{
	struct lowpan_fragment *entry = (struct lowpan_fragment *)entry_addr;

	pr_debug("%s: timer expired for frame with tag %d\n", __func__,
								entry->tag);

	spin_lock(&flist_lock);
	list_del(&entry->list);
	spin_unlock(&flist_lock);

	dev_kfree_skb(entry->skb);
	kfree(entry);
}

static int
lowpan_process_data(struct sk_buff *skb)
{
	struct ipv6hdr hdr;
	u8 tmp, iphc0, iphc1, num_context = 0;
	u8 *_saddr, *_daddr;
	int err;

	lowpan_raw_dump_table(__func__, "raw skb data dump", skb->data,
				skb->len);
	
	if (skb->len < 2)
		goto drop;
	iphc0 = lowpan_fetch_skb_u8(skb);

	
	switch (iphc0 & LOWPAN_DISPATCH_MASK) {
	case LOWPAN_DISPATCH_FRAG1:
	case LOWPAN_DISPATCH_FRAGN:
	{
		struct lowpan_fragment *frame;
		u8 len, offset;
		u16 tag;
		bool found = false;

		len = lowpan_fetch_skb_u8(skb); 
		tag = lowpan_fetch_skb_u16(skb);

		spin_lock(&flist_lock);

		list_for_each_entry(frame, &lowpan_fragments, list)
			if (frame->tag == tag) {
				found = true;
				break;
			}

		
		if (!found) {
			frame = kzalloc(sizeof(struct lowpan_fragment),
								GFP_ATOMIC);
			if (!frame)
				goto unlock_and_drop;

			INIT_LIST_HEAD(&frame->list);

			frame->length = (iphc0 & 7) | (len << 3);
			frame->tag = tag;

			
			frame->skb = alloc_skb(frame->length +
					sizeof(struct ipv6hdr), GFP_ATOMIC);

			if (!frame->skb) {
				kfree(frame);
				goto unlock_and_drop;
			}

			frame->skb->priority = skb->priority;
			frame->skb->dev = skb->dev;

			
			skb_reserve(frame->skb, sizeof(struct ipv6hdr));
			skb_put(frame->skb, frame->length);

			init_timer(&frame->timer);
			
			frame->timer.expires = jiffies + LOWPAN_FRAG_TIMEOUT;
			frame->timer.data = (unsigned long)frame;
			frame->timer.function = lowpan_fragment_timer_expired;

			add_timer(&frame->timer);

			list_add_tail(&frame->list, &lowpan_fragments);
		}

		if ((iphc0 & LOWPAN_DISPATCH_MASK) == LOWPAN_DISPATCH_FRAG1)
			goto unlock_and_drop;

		offset = lowpan_fetch_skb_u8(skb); 

		
		if (likely((offset * 8 + skb->len) <= frame->length))
			skb_copy_to_linear_data_offset(frame->skb, offset * 8,
							skb->data, skb->len);
		else
			goto unlock_and_drop;

		frame->bytes_rcv += skb->len;

		
		if ((frame->bytes_rcv == frame->length) &&
		     frame->timer.expires > jiffies) {
			
			del_timer(&frame->timer);
			list_del(&frame->list);
			spin_unlock(&flist_lock);

			dev_kfree_skb(skb);
			skb = frame->skb;
			kfree(frame);
			iphc0 = lowpan_fetch_skb_u8(skb);
			break;
		}
		spin_unlock(&flist_lock);

		return kfree_skb(skb), 0;
	}
	default:
		break;
	}

	iphc1 = lowpan_fetch_skb_u8(skb);

	_saddr = mac_cb(skb)->sa.hwaddr;
	_daddr = mac_cb(skb)->da.hwaddr;

	pr_debug("(%s): iphc0 = %02x, iphc1 = %02x\n", __func__, iphc0, iphc1);

	
	if (iphc1 & LOWPAN_IPHC_CID) {
		pr_debug("(%s): CID flag is set, increase header with one\n",
								__func__);
		if (!skb->len)
			goto drop;
		num_context = lowpan_fetch_skb_u8(skb);
	}

	hdr.version = 6;

	
	switch ((iphc0 & LOWPAN_IPHC_TF) >> 3) {
	case 0: 
		if (!skb->len)
			goto drop;
		tmp = lowpan_fetch_skb_u8(skb);
		memcpy(&hdr.flow_lbl, &skb->data[0], 3);
		skb_pull(skb, 3);
		hdr.priority = ((tmp >> 2) & 0x0f);
		hdr.flow_lbl[0] = ((tmp >> 2) & 0x30) | (tmp << 6) |
					(hdr.flow_lbl[0] & 0x0f);
		break;
	case 1: 
		if (!skb->len)
			goto drop;
		tmp = lowpan_fetch_skb_u8(skb);
		hdr.priority = ((tmp >> 2) & 0x0f);
		hdr.flow_lbl[0] = ((tmp << 6) & 0xC0) | ((tmp >> 2) & 0x30);
		hdr.flow_lbl[1] = 0;
		hdr.flow_lbl[2] = 0;
		break;
	case 2: 
		if (!skb->len)
			goto drop;
		tmp = lowpan_fetch_skb_u8(skb);
		hdr.flow_lbl[0] = (skb->data[0] & 0x0F) | ((tmp >> 2) & 0x30);
		memcpy(&hdr.flow_lbl[1], &skb->data[0], 2);
		skb_pull(skb, 2);
		break;
	
	case 3: 
		hdr.priority = 0;
		hdr.flow_lbl[0] = 0;
		hdr.flow_lbl[1] = 0;
		hdr.flow_lbl[2] = 0;
		break;
	default:
		break;
	}

	
	if ((iphc0 & LOWPAN_IPHC_NH_C) == 0) {
		
		if (!skb->len)
			goto drop;
		hdr.nexthdr = lowpan_fetch_skb_u8(skb);
		pr_debug("(%s): NH flag is set, next header is carried "
			 "inline: %02x\n", __func__, hdr.nexthdr);
	}

	
	if ((iphc0 & 0x03) != LOWPAN_IPHC_TTL_I)
		hdr.hop_limit = lowpan_ttl_values[iphc0 & 0x03];
	else {
		if (!skb->len)
			goto drop;
		hdr.hop_limit = lowpan_fetch_skb_u8(skb);
	}

	
	tmp = ((iphc1 & LOWPAN_IPHC_SAM) >> LOWPAN_IPHC_SAM_BIT) & 0x03;

	
	pr_debug("(%s): source address stateless compression\n", __func__);
	err = lowpan_uncompress_addr(skb, &hdr.saddr, lowpan_llprefix,
				lowpan_unc_llconf[tmp], skb->data);
	if (err)
		goto drop;

	
	tmp = ((iphc1 & LOWPAN_IPHC_DAM_11) >> LOWPAN_IPHC_DAM_BIT) & 0x03;

	
	if (iphc1 & LOWPAN_IPHC_M) {
		if (iphc1 & LOWPAN_IPHC_DAC) {
			pr_debug("(%s): destination address context-based "
				 "multicast compression\n", __func__);
			
		} else {
			u8 prefix[] = {0xff, 0x02};

			pr_debug("(%s): destination address non-context-based"
				 " multicast compression\n", __func__);
			if (0 < tmp && tmp < 3) {
				if (!skb->len)
					goto drop;
				else
					prefix[1] = lowpan_fetch_skb_u8(skb);
			}

			err = lowpan_uncompress_addr(skb, &hdr.daddr, prefix,
					lowpan_unc_mxconf[tmp], NULL);
			if (err)
				goto drop;
		}
	} else {
		pr_debug("(%s): destination address stateless compression\n",
								__func__);
		err = lowpan_uncompress_addr(skb, &hdr.daddr, lowpan_llprefix,
				lowpan_unc_llconf[tmp], skb->data);
		if (err)
			goto drop;
	}

	
	if (iphc0 & LOWPAN_IPHC_NH_C)
		if (lowpan_uncompress_udp_header(skb))
			goto drop;

	
	hdr.payload_len = htons(skb->len);

	pr_debug("(%s): skb headroom size = %d, data length = %d\n", __func__,
						skb_headroom(skb), skb->len);

	pr_debug("(%s): IPv6 header dump:\n\tversion = %d\n\tlength  = %d\n\t"
		 "nexthdr = 0x%02x\n\thop_lim = %d\n", __func__, hdr.version,
		 ntohs(hdr.payload_len), hdr.nexthdr, hdr.hop_limit);

	lowpan_raw_dump_table(__func__, "raw header dump", (u8 *)&hdr,
							sizeof(hdr));
	return lowpan_skb_deliver(skb, &hdr);

unlock_and_drop:
	spin_unlock(&flist_lock);
drop:
	kfree_skb(skb);
	return -EINVAL;
}

static int lowpan_get_mac_header_length(struct sk_buff *skb)
{
	return 21;
}

static int
lowpan_fragment_xmit(struct sk_buff *skb, u8 *head,
			int mlen, int plen, int offset)
{
	struct sk_buff *frag;
	int hlen, ret;

	
	hlen = (plen == 0 ? LOWPAN_FRAG1_HEAD_SIZE :  LOWPAN_FRAGN_HEAD_SIZE);

	lowpan_raw_dump_inline(__func__, "6lowpan fragment header", head, hlen);

	frag = dev_alloc_skb(hlen + mlen + plen + IEEE802154_MFR_SIZE);
	if (!frag)
		return -ENOMEM;

	frag->priority = skb->priority;
	frag->dev = skb->dev;

	
	memcpy(skb_put(frag, mlen), skb->data, mlen);
	memcpy(skb_put(frag, hlen), head, hlen);

	if (plen)
		skb_copy_from_linear_data_offset(skb, offset + mlen,
					skb_put(frag, plen), plen);

	lowpan_raw_dump_table(__func__, " raw fragment dump", frag->data,
								frag->len);

	ret = dev_queue_xmit(frag);

	return ret;
}

static int
lowpan_skb_fragmentation(struct sk_buff *skb)
{
	int  err, header_length, payload_length, tag, offset = 0;
	u8 head[5];

	header_length = lowpan_get_mac_header_length(skb);
	payload_length = skb->len - header_length;
	tag = fragment_tag++;

	
	head[0] = LOWPAN_DISPATCH_FRAG1 | (payload_length & 0x7);
	head[1] = (payload_length >> 3) & 0xff;
	head[2] = tag & 0xff;
	head[3] = tag >> 8;

	err = lowpan_fragment_xmit(skb, head, header_length, 0, 0);

	
	head[0] &= ~LOWPAN_DISPATCH_FRAG1;
	head[0] |= LOWPAN_DISPATCH_FRAGN;

	while ((payload_length - offset > 0) && (err >= 0)) {
		int len = LOWPAN_FRAG_SIZE;

		head[4] = offset / 8;

		if (payload_length - offset < len)
			len = payload_length - offset;

		err = lowpan_fragment_xmit(skb, head, header_length,
							len, offset);
		offset += len;
	}

	return err;
}

static netdev_tx_t lowpan_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int err = -1;

	pr_debug("(%s): package xmit\n", __func__);

	skb->dev = lowpan_dev_info(dev)->real_dev;
	if (skb->dev == NULL) {
		pr_debug("(%s) ERROR: no real wpan device found\n", __func__);
		goto error;
	}

	if (skb->len <= IEEE802154_MTU) {
		err = dev_queue_xmit(skb);
		goto out;
	}

	pr_debug("(%s): frame is too big, fragmentation is needed\n",
								__func__);
	err = lowpan_skb_fragmentation(skb);
error:
	dev_kfree_skb(skb);
out:
	if (err < 0)
		pr_debug("(%s): ERROR: xmit failed\n", __func__);

	return (err < 0 ? NETDEV_TX_BUSY : NETDEV_TX_OK);
}

static void lowpan_dev_free(struct net_device *dev)
{
	dev_put(lowpan_dev_info(dev)->real_dev);
	free_netdev(dev);
}

static struct wpan_phy *lowpan_get_phy(const struct net_device *dev)
{
	struct net_device *real_dev = lowpan_dev_info(dev)->real_dev;
	return ieee802154_mlme_ops(real_dev)->get_phy(real_dev);
}

static u16 lowpan_get_pan_id(const struct net_device *dev)
{
	struct net_device *real_dev = lowpan_dev_info(dev)->real_dev;
	return ieee802154_mlme_ops(real_dev)->get_pan_id(real_dev);
}

static u16 lowpan_get_short_addr(const struct net_device *dev)
{
	struct net_device *real_dev = lowpan_dev_info(dev)->real_dev;
	return ieee802154_mlme_ops(real_dev)->get_short_addr(real_dev);
}

static struct header_ops lowpan_header_ops = {
	.create	= lowpan_header_create,
};

static const struct net_device_ops lowpan_netdev_ops = {
	.ndo_start_xmit		= lowpan_xmit,
	.ndo_set_mac_address	= eth_mac_addr,
};

static struct ieee802154_mlme_ops lowpan_mlme = {
	.get_pan_id = lowpan_get_pan_id,
	.get_phy = lowpan_get_phy,
	.get_short_addr = lowpan_get_short_addr,
};

static void lowpan_setup(struct net_device *dev)
{
	pr_debug("(%s)\n", __func__);

	dev->addr_len		= IEEE802154_ADDR_LEN;
	memset(dev->broadcast, 0xff, IEEE802154_ADDR_LEN);
	dev->type		= ARPHRD_IEEE802154;
	
	dev->hard_header_len	= 2 + 1 + 20 + 14;
	dev->needed_tailroom	= 2; 
	dev->mtu		= 1281;
	dev->tx_queue_len	= 0;
	dev->flags		= IFF_BROADCAST | IFF_MULTICAST;
	dev->watchdog_timeo	= 0;

	dev->netdev_ops		= &lowpan_netdev_ops;
	dev->header_ops		= &lowpan_header_ops;
	dev->ml_priv		= &lowpan_mlme;
	dev->destructor		= lowpan_dev_free;
}

static int lowpan_validate(struct nlattr *tb[], struct nlattr *data[])
{
	pr_debug("(%s)\n", __func__);

	if (tb[IFLA_ADDRESS]) {
		if (nla_len(tb[IFLA_ADDRESS]) != IEEE802154_ADDR_LEN)
			return -EINVAL;
	}
	return 0;
}

static int lowpan_rcv(struct sk_buff *skb, struct net_device *dev,
	struct packet_type *pt, struct net_device *orig_dev)
{
	if (!netif_running(dev))
		goto drop;

	if (dev->type != ARPHRD_IEEE802154)
		goto drop;

	
	switch (skb->data[0] & 0xe0) {
	case LOWPAN_DISPATCH_IPHC:	
	case LOWPAN_DISPATCH_FRAG1:	
	case LOWPAN_DISPATCH_FRAGN:	
		lowpan_process_data(skb);
		break;
	default:
		break;
	}

	return NET_RX_SUCCESS;

drop:
	kfree_skb(skb);
	return NET_RX_DROP;
}

static int lowpan_newlink(struct net *src_net, struct net_device *dev,
			  struct nlattr *tb[], struct nlattr *data[])
{
	struct net_device *real_dev;
	struct lowpan_dev_record *entry;

	pr_debug("(%s)\n", __func__);

	if (!tb[IFLA_LINK])
		return -EINVAL;
	
	real_dev = dev_get_by_index(src_net, nla_get_u32(tb[IFLA_LINK]));
	if (!real_dev)
		return -ENODEV;

	lowpan_dev_info(dev)->real_dev = real_dev;
	mutex_init(&lowpan_dev_info(dev)->dev_list_mtx);

	entry = kzalloc(sizeof(struct lowpan_dev_record), GFP_KERNEL);
	if (!entry) {
		dev_put(real_dev);
		lowpan_dev_info(dev)->real_dev = NULL;
		return -ENOMEM;
	}

	entry->ldev = dev;

	mutex_lock(&lowpan_dev_info(dev)->dev_list_mtx);
	INIT_LIST_HEAD(&entry->list);
	list_add_tail(&entry->list, &lowpan_devices);
	mutex_unlock(&lowpan_dev_info(dev)->dev_list_mtx);

	spin_lock_init(&flist_lock);

	register_netdevice(dev);

	return 0;
}

static void lowpan_dellink(struct net_device *dev, struct list_head *head)
{
	struct lowpan_dev_info *lowpan_dev = lowpan_dev_info(dev);
	struct net_device *real_dev = lowpan_dev->real_dev;
	struct lowpan_dev_record *entry, *tmp;
	struct lowpan_fragment *frame, *tframe;

	ASSERT_RTNL();

	spin_lock(&flist_lock);
	list_for_each_entry_safe(frame, tframe, &lowpan_fragments, list) {
		del_timer(&frame->timer);
		list_del(&frame->list);
		dev_kfree_skb(frame->skb);
		kfree(frame);
	}
	spin_unlock(&flist_lock);

	mutex_lock(&lowpan_dev_info(dev)->dev_list_mtx);
	list_for_each_entry_safe(entry, tmp, &lowpan_devices, list) {
		if (entry->ldev == dev) {
			list_del(&entry->list);
			kfree(entry);
		}
	}
	mutex_unlock(&lowpan_dev_info(dev)->dev_list_mtx);

	mutex_destroy(&lowpan_dev_info(dev)->dev_list_mtx);

	unregister_netdevice_queue(dev, head);

	dev_put(real_dev);
}

static struct rtnl_link_ops lowpan_link_ops __read_mostly = {
	.kind		= "lowpan",
	.priv_size	= sizeof(struct lowpan_dev_info),
	.setup		= lowpan_setup,
	.newlink	= lowpan_newlink,
	.dellink	= lowpan_dellink,
	.validate	= lowpan_validate,
};

static inline int __init lowpan_netlink_init(void)
{
	return rtnl_link_register(&lowpan_link_ops);
}

static inline void __init lowpan_netlink_fini(void)
{
	rtnl_link_unregister(&lowpan_link_ops);
}

static struct packet_type lowpan_packet_type = {
	.type = __constant_htons(ETH_P_IEEE802154),
	.func = lowpan_rcv,
};

static int __init lowpan_init_module(void)
{
	int err = 0;

	pr_debug("(%s)\n", __func__);

	err = lowpan_netlink_init();
	if (err < 0)
		goto out;

	dev_add_pack(&lowpan_packet_type);
out:
	return err;
}

static void __exit lowpan_cleanup_module(void)
{
	pr_debug("(%s)\n", __func__);

	lowpan_netlink_fini();

	dev_remove_pack(&lowpan_packet_type);
}

module_init(lowpan_init_module);
module_exit(lowpan_cleanup_module);
MODULE_LICENSE("GPL");
MODULE_ALIAS_RTNL_LINK("lowpan");
