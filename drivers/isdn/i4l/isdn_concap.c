/* $Id: isdn_concap.c,v 1.1.2.2 2004/01/12 22:37:19 keil Exp $
 *
 * Linux ISDN subsystem, protocol encapsulation
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */



#include <linux/isdn.h>
#include "isdn_x25iface.h"
#include "isdn_net.h"
#include <linux/concap.h>
#include "isdn_concap.h"




static int isdn_concap_dl_data_req(struct concap_proto *concap, struct sk_buff *skb)
{
	struct net_device *ndev = concap->net_dev;
	isdn_net_dev *nd = ((isdn_net_local *) netdev_priv(ndev))->netdev;
	isdn_net_local *lp = isdn_net_get_locked_lp(nd);

	IX25DEBUG("isdn_concap_dl_data_req: %s \n", concap->net_dev->name);
	if (!lp) {
		IX25DEBUG("isdn_concap_dl_data_req: %s : isdn_net_send_skb returned %d\n", concap->net_dev->name, 1);
		return 1;
	}
	lp->huptimer = 0;
	isdn_net_writebuf_skb(lp, skb);
	spin_unlock_bh(&lp->xmit_lock);
	IX25DEBUG("isdn_concap_dl_data_req: %s : isdn_net_send_skb returned %d\n", concap->net_dev->name, 0);
	return 0;
}


static int isdn_concap_dl_connect_req(struct concap_proto *concap)
{
	struct net_device *ndev = concap->net_dev;
	isdn_net_local *lp = netdev_priv(ndev);
	int ret;
	IX25DEBUG("isdn_concap_dl_connect_req: %s \n", ndev->name);

	
	ret = isdn_net_dial_req(lp);
	if (ret) IX25DEBUG("dialing failed\n");
	return ret;
}

static int isdn_concap_dl_disconn_req(struct concap_proto *concap)
{
	IX25DEBUG("isdn_concap_dl_disconn_req: %s \n", concap->net_dev->name);

	isdn_net_hangup(concap->net_dev);
	return 0;
}

struct concap_device_ops isdn_concap_reliable_dl_dops = {
	&isdn_concap_dl_data_req,
	&isdn_concap_dl_connect_req,
	&isdn_concap_dl_disconn_req
};

struct concap_proto *isdn_concap_new(int encap)
{
	switch (encap) {
	case ISDN_NET_ENCAP_X25IFACE:
		return isdn_x25iface_proto_new();
	}
	return NULL;
}
