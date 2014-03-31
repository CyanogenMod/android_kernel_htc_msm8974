/*
 * Copyright (C) ST-Ericsson AB 2010
 * Author:	Sjur Brendeland/ sjur.brandeland@stericsson.com
 * License terms: GNU General Public License (GPL) version 2
 */

#ifndef CAIF_DEV_H_
#define CAIF_DEV_H_

#include <net/caif/caif_layer.h>
#include <net/caif/cfcnfg.h>
#include <net/caif/caif_device.h>
#include <linux/caif/caif_socket.h>
#include <linux/if.h>
#include <linux/net.h>

struct caif_param {
	u16  size;
	u8   data[256];
};

struct caif_connect_request {
	enum caif_protocol_type protocol;
	struct sockaddr_caif sockaddr;
	enum caif_channel_priority priority;
	enum caif_link_selector link_selector;
	int ifindex;
	struct caif_param param;
};

int caif_connect_client(struct net *net,
			struct caif_connect_request *conn_req,
			struct cflayer *client_layer, int *ifindex,
			int *headroom, int *tailroom);

int caif_disconnect_client(struct net *net, struct cflayer *client_layer);



void caif_client_register_refcnt(struct cflayer *adapt_layer,
					void (*hold)(struct cflayer *lyr),
					void (*put)(struct cflayer *lyr));
void caif_free_client(struct cflayer *adap_layer);

void caif_enroll_dev(struct net_device *dev, struct caif_dev_common *caifdev,
			struct cflayer *link_support, int head_room,
			struct cflayer **layer, int (**rcv_func)(
				struct sk_buff *, struct net_device *,
				struct packet_type *, struct net_device *));

#endif 
