#include <linux/hardirq.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/sched.h>
#include <linux/export.h>
#include <net/cfg80211.h>

#include "host.h"
#include "radiotap.h"
#include "decl.h"
#include "defs.h"
#include "dev.h"
#include "mesh.h"

static u32 convert_radiotap_rate_to_mv(u8 rate)
{
	switch (rate) {
	case 2:		
		return 0 | (1 << 4);
	case 4:		
		return 1 | (1 << 4);
	case 11:		
		return 2 | (1 << 4);
	case 22:		
		return 3 | (1 << 4);
	case 12:		
		return 4 | (1 << 4);
	case 18:		
		return 5 | (1 << 4);
	case 24:		
		return 6 | (1 << 4);
	case 36:		
		return 7 | (1 << 4);
	case 48:		
		return 8 | (1 << 4);
	case 72:		
		return 9 | (1 << 4);
	case 96:		
		return 10 | (1 << 4);
	case 108:		
		return 11 | (1 << 4);
	}
	return 0;
}

netdev_tx_t lbs_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	unsigned long flags;
	struct lbs_private *priv = dev->ml_priv;
	struct txpd *txpd;
	char *p802x_hdr;
	uint16_t pkt_len;
	netdev_tx_t ret = NETDEV_TX_OK;

	lbs_deb_enter(LBS_DEB_TX);

	spin_lock_irqsave(&priv->driver_lock, flags);

	if (priv->surpriseremoved)
		goto free;

	if (!skb->len || (skb->len > MRVDRV_ETH_TX_PACKET_BUFFER_SIZE)) {
		lbs_deb_tx("tx err: skb length %d 0 or > %zd\n",
		       skb->len, MRVDRV_ETH_TX_PACKET_BUFFER_SIZE);
		

		dev->stats.tx_dropped++;
		dev->stats.tx_errors++;
		goto free;
	}


	netif_stop_queue(priv->dev);
	if (priv->mesh_dev)
		netif_stop_queue(priv->mesh_dev);

	if (priv->tx_pending_len) {
		lbs_deb_tx("Packet on %s while busy\n", dev->name);
		ret = NETDEV_TX_BUSY;
		goto unlock;
	}

	priv->tx_pending_len = -1;
	spin_unlock_irqrestore(&priv->driver_lock, flags);

	lbs_deb_hex(LBS_DEB_TX, "TX Data", skb->data, min_t(unsigned int, skb->len, 100));

	txpd = (void *)priv->tx_pending_buf;
	memset(txpd, 0, sizeof(struct txpd));

	p802x_hdr = skb->data;
	pkt_len = skb->len;

	if (priv->wdev->iftype == NL80211_IFTYPE_MONITOR) {
		struct tx_radiotap_hdr *rtap_hdr = (void *)skb->data;

		
		txpd->tx_control = cpu_to_le32(convert_radiotap_rate_to_mv(rtap_hdr->rate));

		
		p802x_hdr += sizeof(*rtap_hdr);
		pkt_len -= sizeof(*rtap_hdr);

		
		memcpy(txpd->tx_dest_addr_high, p802x_hdr + 4, ETH_ALEN);
	} else {
		
		memcpy(txpd->tx_dest_addr_high, p802x_hdr, ETH_ALEN);
	}

	txpd->tx_packet_length = cpu_to_le16(pkt_len);
	txpd->tx_packet_location = cpu_to_le32(sizeof(struct txpd));

	lbs_mesh_set_txpd(priv, dev, txpd);

	lbs_deb_hex(LBS_DEB_TX, "txpd", (u8 *) &txpd, sizeof(struct txpd));

	lbs_deb_hex(LBS_DEB_TX, "Tx Data", (u8 *) p802x_hdr, le16_to_cpu(txpd->tx_packet_length));

	memcpy(&txpd[1], p802x_hdr, le16_to_cpu(txpd->tx_packet_length));

	spin_lock_irqsave(&priv->driver_lock, flags);
	priv->tx_pending_len = pkt_len + sizeof(struct txpd);

	lbs_deb_tx("%s lined up packet\n", __func__);

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;

	if (priv->wdev->iftype == NL80211_IFTYPE_MONITOR) {
		skb_orphan(skb);

		
		priv->currenttxskb = skb;
	} else {
 free:
		dev_kfree_skb_any(skb);
	}

 unlock:
	spin_unlock_irqrestore(&priv->driver_lock, flags);
	wake_up(&priv->waitq);

	lbs_deb_leave_args(LBS_DEB_TX, "ret %d", ret);
	return ret;
}

void lbs_send_tx_feedback(struct lbs_private *priv, u32 try_count)
{
	struct tx_radiotap_hdr *radiotap_hdr;

	if (priv->wdev->iftype != NL80211_IFTYPE_MONITOR ||
	    priv->currenttxskb == NULL)
		return;

	radiotap_hdr = (struct tx_radiotap_hdr *)priv->currenttxskb->data;

	radiotap_hdr->data_retries = try_count ?
		(1 + priv->txretrycount - try_count) : 0;

	priv->currenttxskb->protocol = eth_type_trans(priv->currenttxskb,
						      priv->dev);
	netif_rx(priv->currenttxskb);

	priv->currenttxskb = NULL;

	if (priv->connect_status == LBS_CONNECTED)
		netif_wake_queue(priv->dev);

	if (priv->mesh_dev && netif_running(priv->mesh_dev))
		netif_wake_queue(priv->mesh_dev);
}
EXPORT_SYMBOL_GPL(lbs_send_tx_feedback);
