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
 * RMNET Data ingress/egress handler
 *
 */

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/rmnet_data.h>
#include "rmnet_data_private.h"
#include "rmnet_data_config.h"
#include "rmnet_data_vnd.h"
#include "rmnet_map.h"

void rmnet_egress_handler(struct sk_buff *skb,
			  struct rmnet_logical_ep_conf_s *ep);

#ifdef CONFIG_RMNET_DATA_DEBUG_PKT
unsigned int dump_pkt_rx;
module_param(dump_pkt_rx, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dump_pkt_rx, "Dump packets entering ingress handler");

unsigned int dump_pkt_tx;
module_param(dump_pkt_tx, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dump_pkt_tx, "Dump packets exiting egress handler");
#endif 


static inline void __rmnet_data_set_skb_proto(struct sk_buff *skb)
{
	switch (skb->data[0] & 0xF0) {
	case 0x40: 
		skb->protocol = htons(ETH_P_IP);
		break;
	case 0x60: 
		skb->protocol = htons(ETH_P_IPV6);
		break;
	default:
		skb->protocol = htons(ETH_P_MAP);
		break;
	}
}

#ifdef CONFIG_RMNET_DATA_DEBUG_PKT
void rmnet_print_packet(const struct sk_buff *skb, const char *dev, char dir)
{
	char buffer[200];
	unsigned int len, printlen;
	int i, buffloc = 0;

	switch (dir) {
	case 'r':
		printlen = dump_pkt_rx;
		break;

	case 't':
		printlen = dump_pkt_tx;
		break;

	default:
		printlen = 0;
		break;
	}

	if (!printlen)
		return;

	pr_err("[%s][%c] - PKT skb->len=%d skb->head=%p skb->data=%p skb->tail=%p skb->end=%p",
		dev, dir, skb->len, skb->head, skb->data, skb->tail, skb->end);

	if (skb->len > 0)
		len = skb->len;
	else
		len = ((unsigned int)skb->end) - ((unsigned int)skb->data);

	pr_err("[%s][%c] - PKT len: %d, printing first %d bytes",
		dev, dir, len, printlen);

	memset(buffer, 0, sizeof(buffer));
	for (i = 0; (i < printlen) && (i < len); i++) {
		if ((i%16) == 0) {
			pr_err("[%s][%c] - PKT%s", dev, dir, buffer);
			memset(buffer, 0, sizeof(buffer));
			buffloc = 0;
			buffloc += snprintf(&buffer[buffloc],
					sizeof(buffer)-buffloc, "%04X:",
					i);
		}

		buffloc += snprintf(&buffer[buffloc], sizeof(buffer)-buffloc,
					" %02x", skb->data[i]);

	}
	pr_err("[%s][%c] - PKT%s", dev, dir, buffer);
}
#else
void rmnet_print_packet(const struct sk_buff *skb, const char *dev, char dir)
{
	return;
}
#endif 


static rx_handler_result_t rmnet_bridge_handler(struct sk_buff *skb,
					struct rmnet_logical_ep_conf_s *ep)
{
	if (!ep->egress_dev) {
		LOGD("%s(): Missing egress device for packet arriving on %s",
		      __func__, skb->dev->name);
		kfree_skb(skb);
	} else {
		rmnet_egress_handler(skb, ep);
	}

	return RX_HANDLER_CONSUMED;
}

static rx_handler_result_t __rmnet_deliver_skb(struct sk_buff *skb,
					 struct rmnet_logical_ep_conf_s *ep)
{
	switch (ep->rmnet_mode) {
	case RMNET_EPMODE_NONE:
		return RX_HANDLER_PASS;

	case RMNET_EPMODE_BRIDGE:
		return rmnet_bridge_handler(skb, ep);

	case RMNET_EPMODE_VND:
		skb_reset_transport_header(skb);
		skb_reset_network_header(skb);
		switch (rmnet_vnd_rx_fixup(skb, skb->dev)) {
		case RX_HANDLER_CONSUMED:
			return RX_HANDLER_CONSUMED;

		case RX_HANDLER_PASS:
			skb->pkt_type = PACKET_HOST;
			return  RX_HANDLER_ANOTHER;
		}
		return RX_HANDLER_PASS;

	default:
		LOGD("%s() unkown ep mode %d", __func__,
			ep->rmnet_mode);
		kfree_skb(skb);
		return RX_HANDLER_CONSUMED;
	}
}

static rx_handler_result_t rmnet_ip_ingress_handler(struct sk_buff *skb,
					    struct rmnet_phys_ep_conf_s *config)
{
	if (!config->local_ep.refcount) {
		LOGD("%s(): Packet on %s has no local endpoint configuration\n",
			__func__, skb->dev->name);
		kfree_skb(skb);
		return RX_HANDLER_CONSUMED;
	}

	skb->dev = config->local_ep.egress_dev;

	return __rmnet_deliver_skb(skb, &config->local_ep);
}


static rx_handler_result_t _rmnet_map_ingress_handler(struct sk_buff *skb,
					    struct rmnet_phys_ep_conf_s *config)
{
	struct rmnet_logical_ep_conf_s *ep;
	uint8_t mux_id;
	uint16_t len;

	mux_id = RMNET_MAP_GET_MUX_ID(skb);
	len = RMNET_MAP_GET_LENGTH(skb) - RMNET_MAP_GET_PAD(skb);

	if (mux_id >= RMNET_DATA_MAX_LOGICAL_EP) {
		LOGD("%s(): Got packet on %s with bad mux id %d\n",
		     __func__, skb->dev->name, mux_id);
		kfree_skb(skb);
			return RX_HANDLER_CONSUMED;
	}

	ep = &(config->muxed_ep[mux_id]);

	if (!ep->refcount) {
		LOGD("%s(): Packet on %s:%d; has no logical endpoint config\n",
		     __func__, skb->dev->name, mux_id);

		kfree_skb(skb);
			return RX_HANDLER_CONSUMED;
	}

	if (config->ingress_data_format & RMNET_INGRESS_FORMAT_DEMUXING)
		skb->dev = ep->egress_dev;

	
	skb_pull(skb, sizeof(struct rmnet_map_header_s));
	skb_trim(skb, len);
	__rmnet_data_set_skb_proto(skb);

	return __rmnet_deliver_skb(skb, ep);
}

static rx_handler_result_t rmnet_map_ingress_handler(struct sk_buff *skb,
					    struct rmnet_phys_ep_conf_s *config)
{
	struct sk_buff *skbn;
	int rc, co = 0;

	if (config->ingress_data_format & RMNET_INGRESS_FORMAT_DEAGGREGATION) {
		while ((skbn = rmnet_map_deaggregate(skb, config)) != 0) {
			LOGD("co=%d\n", co);
			_rmnet_map_ingress_handler(skbn, config);
			co++;
		}
		kfree_skb(skb);
		rc = RX_HANDLER_CONSUMED;
	} else {
		rc = _rmnet_map_ingress_handler(skb, config);
	}

	return rc;
}

static int rmnet_map_egress_handler(struct sk_buff *skb,
				    struct rmnet_phys_ep_conf_s *config,
				    struct rmnet_logical_ep_conf_s *ep)
{
	int required_headroom, additional_header_length;
	struct rmnet_map_header_s *map_header;

	additional_header_length = 0;

	required_headroom = sizeof(struct rmnet_map_header_s);

	LOGD("%s(): headroom of %d bytes\n", __func__, required_headroom);

	if (skb_headroom(skb) < required_headroom) {
		if (pskb_expand_head(skb, required_headroom, 0, GFP_KERNEL)) {
			LOGD("%s(): Failed to add headroom of %d bytes\n",
			     __func__, required_headroom);
			return 1;
		}
	}

	map_header = rmnet_map_add_map_header(skb, additional_header_length);

	if (!map_header) {
		LOGD("%s(): Failed to add MAP header to egress packet",
		     __func__);
		return 1;
	}

	if (config->egress_data_format & RMNET_EGRESS_FORMAT_MUXING) {
		if (ep->mux_id == 0xff)
			map_header->mux_id = 0;
		else
			map_header->mux_id = ep->mux_id;
	}

	if (config->egress_data_format & RMNET_EGRESS_FORMAT_AGGREGATION) {
		rmnet_map_aggregate(skb, config);
		return RMNET_MAP_CONSUMED;
	}

	return RMNET_MAP_SUCCESS;
}

rx_handler_result_t rmnet_ingress_handler(struct sk_buff *skb)
{
	struct rmnet_phys_ep_conf_s *config;
	struct net_device *dev;
	int rc;

	if (!skb)
		BUG();

	dev = skb->dev;
	rmnet_print_packet(skb, dev->name, 'r');

	config = (struct rmnet_phys_ep_conf_s *)
		rcu_dereference(skb->dev->rx_handler_data);

	if (config->ingress_data_format & RMNET_INGRESS_FIX_ETHERNET) {
		skb_push(skb, RMNET_ETHERNET_HEADER_LENGTH);
		__rmnet_data_set_skb_proto(skb);
	}

	if (config->ingress_data_format & RMNET_INGRESS_FORMAT_MAP) {
		if (RMNET_MAP_GET_CD_BIT(skb)) {
			if (config->ingress_data_format
			    & RMNET_INGRESS_FORMAT_MAP_COMMANDS) {
				rc = rmnet_map_command(skb, config);
			} else {
				LOGM("%s(): MAP command packet on %s; %s\n",
				     __func__, dev->name,
				     "Not configured for MAP commands");
				kfree_skb(skb);
				return RX_HANDLER_CONSUMED;
			}
		} else {
			rc = rmnet_map_ingress_handler(skb, config);
		}
	} else {
		switch (ntohs(skb->protocol)) {
		case ETH_P_MAP:
			LOGD("%s(): MAP packet on %s; not configured for MAP\n",
				__func__, dev->name);
			kfree_skb(skb);
			rc = RX_HANDLER_CONSUMED;
			break;

		case ETH_P_ARP:
		case ETH_P_IP:
		case ETH_P_IPV6:
			rc = rmnet_ip_ingress_handler(skb, config);
			break;

		default:
			LOGD("%s(): Unknown skb->proto 0x%04X\n",
				__func__, ntohs(skb->protocol) & 0xFFFF);
			rc = RX_HANDLER_PASS;
		}
	}

	return rc;
}

rx_handler_result_t rmnet_rx_handler(struct sk_buff **pskb)
{
	return rmnet_ingress_handler(*pskb);
}

void rmnet_egress_handler(struct sk_buff *skb,
			  struct rmnet_logical_ep_conf_s *ep)
{
	struct rmnet_phys_ep_conf_s *config;
	struct net_device *orig_dev;
	orig_dev = skb->dev;
	skb->dev = ep->egress_dev;

	config = (struct rmnet_phys_ep_conf_s *)
		rcu_dereference(skb->dev->rx_handler_data);

	LOGD("%s(): Packet going out on %s with egress format 0x%08X\n",
	      __func__, skb->dev->name, config->egress_data_format);

	if (config->egress_data_format & RMNET_EGRESS_FORMAT_MAP) {
		switch (rmnet_map_egress_handler(skb, config, ep)) {
		case RMNET_MAP_CONSUMED:
			LOGD("%s(): MAP process consumed packet\n", __func__);
			return;

		case RMNET_MAP_SUCCESS:
			break;

		default:
			LOGD("%s(): MAP egress failed on packet on %s",
			      __func__, skb->dev->name);
			kfree_skb(skb);
			return;
		}
	}

	if (ep->rmnet_mode == RMNET_EPMODE_VND)
		rmnet_vnd_tx_fixup(skb, orig_dev);

	rmnet_print_packet(skb, skb->dev->name, 't');
	if (dev_queue_xmit(skb) != 0) {
		LOGD("%s(): Failed to queue packet for transmission on [%s]\n",
		      __func__, skb->dev->name);
	}
}
