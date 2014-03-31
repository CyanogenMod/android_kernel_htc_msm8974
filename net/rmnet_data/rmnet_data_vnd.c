/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * RMNET Data virtual network driver
 *
 */

#include <linux/types.h>
#include <linux/rmnet_data.h>
#include <linux/msm_rmnet.h>
#include <linux/etherdevice.h>
#include <linux/if_arp.h>
#include <linux/spinlock.h>
#include <net/pkt_sched.h>
#include "rmnet_data_config.h"
#include "rmnet_data_handlers.h"
#include "rmnet_data_private.h"
#include "rmnet_map.h"

struct net_device *rmnet_devices[RMNET_DATA_MAX_VND];

struct rmnet_vnd_private_s {
	uint8_t qos_mode:1;
	uint8_t reserved:7;
	struct rmnet_logical_ep_conf_s local_ep;
	struct rmnet_map_flow_control_s flows;
};


static void rmnet_vnd_add_qos_header(struct sk_buff *skb,
				     struct net_device *dev)
{
	struct QMI_QOS_HDR_S *qmih;

	qmih = (struct QMI_QOS_HDR_S *)
		skb_push(skb, sizeof(struct QMI_QOS_HDR_S));
	qmih->version = 1;
	qmih->flags = 0;
	qmih->flow_id = skb->mark;
}


int rmnet_vnd_rx_fixup(struct sk_buff *skb, struct net_device *dev)
{
	if (unlikely(!dev || !skb))
		BUG();

	dev->stats.rx_packets++;
	dev->stats.rx_bytes += skb->len;

	return RX_HANDLER_PASS;
}

int rmnet_vnd_tx_fixup(struct sk_buff *skb, struct net_device *dev)
{
	struct rmnet_vnd_private_s *dev_conf;
	dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);

	if (unlikely(!dev || !skb))
		BUG();

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;

	return RX_HANDLER_PASS;
}


static netdev_tx_t rmnet_vnd_start_xmit(struct sk_buff *skb,
					struct net_device *dev)
{
	struct rmnet_vnd_private_s *dev_conf;
	dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);
	if (dev_conf->local_ep.egress_dev) {
		
		if (dev_conf->qos_mode)
			rmnet_vnd_add_qos_header(skb, dev);
		rmnet_egress_handler(skb, &dev_conf->local_ep);
	} else {
		dev->stats.tx_dropped++;
		kfree_skb(skb);
	}
	return NETDEV_TX_OK;
}

static int rmnet_vnd_change_mtu(struct net_device *dev, int new_mtu)
{
	if (new_mtu < 0 || new_mtu > RMNET_DATA_MAX_PACKET_SIZE)
		return -EINVAL;

	dev->mtu = new_mtu;
	return 0;
}

#ifdef CONFIG_RMNET_DATA_FC
static int _rmnet_vnd_do_qos_ioctl(struct net_device *dev,
				   struct ifreq *ifr,
				   int cmd)
{
	struct rmnet_vnd_private_s *dev_conf;
	int rc;
	rc = 0;
	dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);

	switch (cmd) {

	case RMNET_IOCTL_SET_QOS_ENABLE:
		LOGM("%s(): RMNET_IOCTL_SET_QOS_ENABLE on %s\n",
		     __func__, dev->name);
		dev_conf->qos_mode = 1;
		break;

	case RMNET_IOCTL_SET_QOS_DISABLE:
		LOGM("%s(): RMNET_IOCTL_SET_QOS_DISABLE on %s\n",
		     __func__, dev->name);
		dev_conf->qos_mode = 0;
		break;

	case RMNET_IOCTL_FLOW_ENABLE:
		LOGL("%s(): RMNET_IOCTL_FLOW_ENABLE on %s\n",
		     __func__, dev->name);
		tc_qdisc_flow_control(dev, (u32)ifr->ifr_data, 1);
		break;

	case RMNET_IOCTL_FLOW_DISABLE:
		LOGL("%s(): RMNET_IOCTL_FLOW_DISABLE on %s\n",
		     __func__, dev->name);
		tc_qdisc_flow_control(dev, (u32)ifr->ifr_data, 0);
		break;

	case RMNET_IOCTL_GET_QOS:           
		LOGM("%s(): RMNET_IOCTL_GET_QOS on %s\n",
		     __func__, dev->name);
		ifr->ifr_ifru.ifru_data =
			(void *)(dev_conf->qos_mode == 1);
		break;

	default:
		rc = -EINVAL;
	}

	return rc;
}
#else
static int _rmnet_vnd_do_qos_ioctl(struct net_device *dev,
				   struct ifreq *ifr,
				   int cmd)
{
	return -EINVAL;
}
#endif 

static int rmnet_vnd_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct rmnet_vnd_private_s *dev_conf;
	int rc;
	rc = 0;
	dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);

	rc = _rmnet_vnd_do_qos_ioctl(dev, ifr, cmd);
	if (rc != -EINVAL)
		return rc;
	rc = 0; 

	switch (cmd) {

	case RMNET_IOCTL_OPEN: 
		LOGM("%s(): RMNET_IOCTL_OPEN on %s (ignored)\n",
		     __func__, dev->name);
		break;

	case RMNET_IOCTL_CLOSE: 
		LOGM("%s(): RMNET_IOCTL_CLOSE on %s (ignored)\n",
		     __func__, dev->name);
		break;

	case RMNET_IOCTL_SET_LLP_ETHERNET:
		LOGM("%s(): RMNET_IOCTL_SET_LLP_ETHERNET on %s (no support)\n",
		     __func__, dev->name);
		rc = -EINVAL;
		break;

	case RMNET_IOCTL_SET_LLP_IP: 
		LOGM("%s(): RMNET_IOCTL_SET_LLP_IP on %s  (ignored)\n",
		     __func__, dev->name);
		break;

	case RMNET_IOCTL_GET_LLP: 
		LOGM("%s(): RMNET_IOCTL_GET_LLP on %s\n",
		     __func__, dev->name);
		ifr->ifr_ifru.ifru_data = (void *)(RMNET_MODE_LLP_IP);
		break;

	default:
		LOGH("%s(): Unkown IOCTL 0x%08X\n", __func__, cmd);
		rc = -EINVAL;
	}

	return rc;
}

static const struct net_device_ops rmnet_data_vnd_ops = {
	.ndo_init = 0,
	.ndo_start_xmit = rmnet_vnd_start_xmit,
	.ndo_do_ioctl = rmnet_vnd_ioctl,
	.ndo_change_mtu = rmnet_vnd_change_mtu,
	.ndo_set_mac_address = 0,
	.ndo_validate_addr = 0,
};

static void rmnet_vnd_setup(struct net_device *dev)
{
	struct rmnet_vnd_private_s *dev_conf;
	LOGM("%s(): Setting up device %s\n", __func__, dev->name);

	
	dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);
	memset(dev_conf, 0, sizeof(struct rmnet_vnd_private_s));

	
	dev->flags |= IFF_NOARP;
	dev->netdev_ops = &rmnet_data_vnd_ops;
	dev->mtu = RMNET_DATA_DFLT_PACKET_SIZE;
	dev->needed_headroom = RMNET_DATA_NEEDED_HEADROOM;
	random_ether_addr(dev->dev_addr);
	dev->watchdog_timeo = 1000;

	
	dev->header_ops = 0;  
	dev->type = ARPHRD_RAWIP;
	dev->hard_header_len = 0;
	dev->flags &= ~(IFF_BROADCAST | IFF_MULTICAST);

	
	rwlock_init(&dev_conf->flows.flow_map_lock);
}


void rmnet_vnd_exit(void)
{
	int i;
	for (i = 0; i < RMNET_DATA_MAX_VND; i++)
		if (rmnet_devices[i]) {
			unregister_netdev(rmnet_devices[i]);
			free_netdev(rmnet_devices[i]);
	}
}

int rmnet_vnd_init(void)
{
	memset(rmnet_devices, 0,
	       sizeof(struct net_device *) * RMNET_DATA_MAX_VND);
	return 0;
}

int rmnet_vnd_create_dev(int id, struct net_device **new_device)
{
	struct net_device *dev;
	int rc = 0;

	if (id < 0 || id > RMNET_DATA_MAX_VND || rmnet_devices[id] != 0) {
		*new_device = 0;
		return -EINVAL;
	}

	dev = alloc_netdev(sizeof(struct rmnet_vnd_private_s),
			   RMNET_DATA_DEV_NAME_STR,
			   rmnet_vnd_setup);
	if (!dev) {
		LOGE("%s(): Failed to to allocate netdev for id %d",
		      __func__, id);
		*new_device = 0;
		return -EINVAL;
	}

	rc = register_netdevice(dev);
	if (rc != 0) {
		LOGE("%s(): Failed to to register netdev [%s]",
		      __func__, dev->name);
		free_netdev(dev);
		*new_device = 0;
	} else {
		rmnet_devices[id] = dev;
		*new_device = dev;
	}

	LOGM("%s(): Registered device %s\n", __func__, dev->name);
	return rc;
}

int rmnet_vnd_is_vnd(struct net_device *dev)
{
	int i;

	if (!dev)
		BUG();

	for (i = 0; i < RMNET_DATA_MAX_VND; i++)
		if (dev == rmnet_devices[i])
			return i+1;

	return 0;
}

struct rmnet_logical_ep_conf_s *rmnet_vnd_get_le_config(struct net_device *dev)
{
	struct rmnet_vnd_private_s *dev_conf;
	if (!dev)
		return 0;

	dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);
	if (!dev_conf)
		BUG();

	return &dev_conf->local_ep;
}

int rmnet_vnd_get_flow_mapping(struct net_device *dev,
			       uint32_t map_flow_id,
			       uint32_t *tc_handle,
			       uint64_t **v4_seq,
			       uint64_t **v6_seq)
{
	struct rmnet_vnd_private_s *dev_conf;
	struct rmnet_map_flow_mapping_s *flowmap;
	int i;
	int error = 0;

	if (!dev || !tc_handle)
		BUG();

	if (!rmnet_vnd_is_vnd(dev)) {
		*tc_handle = 0;
		return 2;
	} else {
		dev_conf = (struct rmnet_vnd_private_s *) netdev_priv(dev);
	}

	if (!dev_conf)
		BUG();

	if (map_flow_id == 0xFFFFFFFF) {
		*tc_handle = dev_conf->flows.default_tc_handle;
		*v4_seq = &dev_conf->flows.default_v4_seq;
		*v6_seq = &dev_conf->flows.default_v6_seq;
		if (*tc_handle == 0)
			error = 1;
	} else {
		flowmap = &dev_conf->flows.flowmap[0];
		for (i = 0; i < RMNET_MAP_MAX_FLOWS; i++) {
			if ((flowmap[i].flow_id != 0)
			     && (flowmap[i].flow_id == map_flow_id)) {

				*tc_handle = flowmap[i].tc_handle;
				*v4_seq = &flowmap[i].v4_seq;
				*v6_seq = &flowmap[i].v6_seq;
				error = 0;
				break;
			}
		}
		*v4_seq = 0;
		*v6_seq = 0;
		*tc_handle = 0;
		error = 1;
	}

	return error;
}
