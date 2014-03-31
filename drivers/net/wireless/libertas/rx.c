
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/etherdevice.h>
#include <linux/hardirq.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/export.h>
#include <net/cfg80211.h>

#include "defs.h"
#include "host.h"
#include "radiotap.h"
#include "decl.h"
#include "dev.h"
#include "mesh.h"

struct eth803hdr {
	u8 dest_addr[6];
	u8 src_addr[6];
	u16 h803_len;
} __packed;

struct rfc1042hdr {
	u8 llc_dsap;
	u8 llc_ssap;
	u8 llc_ctrl;
	u8 snap_oui[3];
	u16 snap_type;
} __packed;

struct rxpackethdr {
	struct eth803hdr eth803_hdr;
	struct rfc1042hdr rfc1042_hdr;
} __packed;

struct rx80211packethdr {
	struct rxpd rx_pd;
	void *eth80211_hdr;
} __packed;

static int process_rxed_802_11_packet(struct lbs_private *priv,
	struct sk_buff *skb);

int lbs_process_rxed_packet(struct lbs_private *priv, struct sk_buff *skb)
{
	int ret = 0;
	struct net_device *dev = priv->dev;
	struct rxpackethdr *p_rx_pkt;
	struct rxpd *p_rx_pd;
	int hdrchop;
	struct ethhdr *p_ethhdr;
	static const u8 rfc1042_eth_hdr[] = {
		0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00
	};

	lbs_deb_enter(LBS_DEB_RX);

	BUG_ON(!skb);

	skb->ip_summed = CHECKSUM_NONE;

	if (priv->wdev->iftype == NL80211_IFTYPE_MONITOR)
		return process_rxed_802_11_packet(priv, skb);

	p_rx_pd = (struct rxpd *) skb->data;
	p_rx_pkt = (struct rxpackethdr *) ((u8 *)p_rx_pd +
		le32_to_cpu(p_rx_pd->pkt_ptr));

	dev = lbs_mesh_set_dev(priv, dev, p_rx_pd);

	lbs_deb_hex(LBS_DEB_RX, "RX Data: Before chop rxpd", skb->data,
		 min_t(unsigned int, skb->len, 100));

	if (skb->len < (ETH_HLEN + 8 + sizeof(struct rxpd))) {
		lbs_deb_rx("rx err: frame received with bad length\n");
		dev->stats.rx_length_errors++;
		ret = 0;
		dev_kfree_skb(skb);
		goto done;
	}

	lbs_deb_rx("rx data: skb->len - pkt_ptr = %d-%zd = %zd\n",
		skb->len, (size_t)le32_to_cpu(p_rx_pd->pkt_ptr),
		skb->len - (size_t)le32_to_cpu(p_rx_pd->pkt_ptr));

	lbs_deb_hex(LBS_DEB_RX, "RX Data: Dest", p_rx_pkt->eth803_hdr.dest_addr,
		sizeof(p_rx_pkt->eth803_hdr.dest_addr));
	lbs_deb_hex(LBS_DEB_RX, "RX Data: Src", p_rx_pkt->eth803_hdr.src_addr,
		sizeof(p_rx_pkt->eth803_hdr.src_addr));

	if (memcmp(&p_rx_pkt->rfc1042_hdr,
		   rfc1042_eth_hdr, sizeof(rfc1042_eth_hdr)) == 0) {
		p_ethhdr = (struct ethhdr *)
		    ((u8 *) &p_rx_pkt->eth803_hdr
		     + sizeof(p_rx_pkt->eth803_hdr) + sizeof(p_rx_pkt->rfc1042_hdr)
		     - sizeof(p_rx_pkt->eth803_hdr.dest_addr)
		     - sizeof(p_rx_pkt->eth803_hdr.src_addr)
		     - sizeof(p_rx_pkt->rfc1042_hdr.snap_type));

		memcpy(p_ethhdr->h_source, p_rx_pkt->eth803_hdr.src_addr,
		       sizeof(p_ethhdr->h_source));
		memcpy(p_ethhdr->h_dest, p_rx_pkt->eth803_hdr.dest_addr,
		       sizeof(p_ethhdr->h_dest));

		hdrchop = (u8 *)p_ethhdr - (u8 *)p_rx_pd;
	} else {
		lbs_deb_hex(LBS_DEB_RX, "RX Data: LLC/SNAP",
			(u8 *) &p_rx_pkt->rfc1042_hdr,
			sizeof(p_rx_pkt->rfc1042_hdr));

		
		hdrchop = (u8 *)&p_rx_pkt->eth803_hdr - (u8 *)p_rx_pd;
	}

	skb_pull(skb, hdrchop);

	priv->cur_rate = lbs_fw_index_to_data_rate(p_rx_pd->rx_rate);

	lbs_deb_rx("rx data: size of actual packet %d\n", skb->len);
	dev->stats.rx_bytes += skb->len;
	dev->stats.rx_packets++;

	skb->protocol = eth_type_trans(skb, dev);
	if (in_interrupt())
		netif_rx(skb);
	else
		netif_rx_ni(skb);

	ret = 0;
done:
	lbs_deb_leave_args(LBS_DEB_RX, "ret %d", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(lbs_process_rxed_packet);

static u8 convert_mv_rate_to_radiotap(u8 rate)
{
	switch (rate) {
	case 0:		
		return 2;
	case 1:		
		return 4;
	case 2:		
		return 11;
	case 3:		
		return 22;
	
	case 5:		
		return 12;
	case 6:		
		return 18;
	case 7:		
		return 24;
	case 8:		
		return 36;
	case 9:		
		return 48;
	case 10:		
		return 72;
	case 11:		
		return 96;
	case 12:		
		return 108;
	}
	pr_alert("Invalid Marvell WLAN rate %i\n", rate);
	return 0;
}

static int process_rxed_802_11_packet(struct lbs_private *priv,
	struct sk_buff *skb)
{
	int ret = 0;
	struct net_device *dev = priv->dev;
	struct rx80211packethdr *p_rx_pkt;
	struct rxpd *prxpd;
	struct rx_radiotap_hdr radiotap_hdr;
	struct rx_radiotap_hdr *pradiotap_hdr;

	lbs_deb_enter(LBS_DEB_RX);

	p_rx_pkt = (struct rx80211packethdr *) skb->data;
	prxpd = &p_rx_pkt->rx_pd;

	

	if (skb->len < (ETH_HLEN + 8 + sizeof(struct rxpd))) {
		lbs_deb_rx("rx err: frame received with bad length\n");
		dev->stats.rx_length_errors++;
		ret = -EINVAL;
		kfree_skb(skb);
		goto done;
	}

	lbs_deb_rx("rx data: skb->len-sizeof(RxPd) = %d-%zd = %zd\n",
	       skb->len, sizeof(struct rxpd), skb->len - sizeof(struct rxpd));

	

	
	memset(&radiotap_hdr, 0, sizeof(radiotap_hdr));
	
	radiotap_hdr.hdr.it_len = cpu_to_le16 (sizeof(struct rx_radiotap_hdr));
	radiotap_hdr.hdr.it_present = cpu_to_le32 (RX_RADIOTAP_PRESENT);
	radiotap_hdr.rate = convert_mv_rate_to_radiotap(prxpd->rx_rate);
	
	radiotap_hdr.antsignal = prxpd->snr + prxpd->nf;

	
	skb_pull(skb, sizeof(struct rxpd));

	
	if ((skb_headroom(skb) < sizeof(struct rx_radiotap_hdr)) &&
	    pskb_expand_head(skb, sizeof(struct rx_radiotap_hdr), 0, GFP_ATOMIC)) {
		netdev_alert(dev, "%s: couldn't pskb_expand_head\n", __func__);
		ret = -ENOMEM;
		kfree_skb(skb);
		goto done;
	}

	pradiotap_hdr = (void *)skb_push(skb, sizeof(struct rx_radiotap_hdr));
	memcpy(pradiotap_hdr, &radiotap_hdr, sizeof(struct rx_radiotap_hdr));

	priv->cur_rate = lbs_fw_index_to_data_rate(prxpd->rx_rate);

	lbs_deb_rx("rx data: size of actual packet %d\n", skb->len);
	dev->stats.rx_bytes += skb->len;
	dev->stats.rx_packets++;

	skb->protocol = eth_type_trans(skb, priv->dev);

	if (in_interrupt())
		netif_rx(skb);
	else
		netif_rx_ni(skb);

	ret = 0;

done:
	lbs_deb_leave_args(LBS_DEB_RX, "ret %d", ret);
	return ret;
}
