/*
 * Copyright (c) 2009, Microsoft Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307 USA.
 *
 * Authors:
 *   Haiyang Zhang <haiyangz@microsoft.com>
 *   Hank Janssen  <hjanssen@microsoft.com>
 */
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/if_ether.h>

#include "hyperv_net.h"


static struct netvsc_device *alloc_net_device(struct hv_device *device)
{
	struct netvsc_device *net_device;
	struct net_device *ndev = hv_get_drvdata(device);

	net_device = kzalloc(sizeof(struct netvsc_device), GFP_KERNEL);
	if (!net_device)
		return NULL;

	net_device->start_remove = false;
	net_device->destroy = false;
	net_device->dev = device;
	net_device->ndev = ndev;

	hv_set_drvdata(device, net_device);
	return net_device;
}

static struct netvsc_device *get_outbound_net_device(struct hv_device *device)
{
	struct netvsc_device *net_device;

	net_device = hv_get_drvdata(device);
	if (net_device && net_device->destroy)
		net_device = NULL;

	return net_device;
}

static struct netvsc_device *get_inbound_net_device(struct hv_device *device)
{
	struct netvsc_device *net_device;

	net_device = hv_get_drvdata(device);

	if (!net_device)
		goto get_in_err;

	if (net_device->destroy &&
		atomic_read(&net_device->num_outstanding_sends) == 0)
		net_device = NULL;

get_in_err:
	return net_device;
}


static int netvsc_destroy_recv_buf(struct netvsc_device *net_device)
{
	struct nvsp_message *revoke_packet;
	int ret = 0;
	struct net_device *ndev = net_device->ndev;

	if (net_device->recv_section_cnt) {
		
		revoke_packet = &net_device->revoke_packet;
		memset(revoke_packet, 0, sizeof(struct nvsp_message));

		revoke_packet->hdr.msg_type =
			NVSP_MSG1_TYPE_REVOKE_RECV_BUF;
		revoke_packet->msg.v1_msg.
		revoke_recv_buf.id = NETVSC_RECEIVE_BUFFER_ID;

		ret = vmbus_sendpacket(net_device->dev->channel,
				       revoke_packet,
				       sizeof(struct nvsp_message),
				       (unsigned long)revoke_packet,
				       VM_PKT_DATA_INBAND, 0);
		if (ret != 0) {
			netdev_err(ndev, "unable to send "
				"revoke receive buffer to netvsp\n");
			return ret;
		}
	}

	
	if (net_device->recv_buf_gpadl_handle) {
		ret = vmbus_teardown_gpadl(net_device->dev->channel,
			   net_device->recv_buf_gpadl_handle);

		if (ret != 0) {
			netdev_err(ndev,
				   "unable to teardown receive buffer's gpadl\n");
			return ret;
		}
		net_device->recv_buf_gpadl_handle = 0;
	}

	if (net_device->recv_buf) {
		
		free_pages((unsigned long)net_device->recv_buf,
			get_order(net_device->recv_buf_size));
		net_device->recv_buf = NULL;
	}

	if (net_device->recv_section) {
		net_device->recv_section_cnt = 0;
		kfree(net_device->recv_section);
		net_device->recv_section = NULL;
	}

	return ret;
}

static int netvsc_init_recv_buf(struct hv_device *device)
{
	int ret = 0;
	int t;
	struct netvsc_device *net_device;
	struct nvsp_message *init_packet;
	struct net_device *ndev;

	net_device = get_outbound_net_device(device);
	if (!net_device)
		return -ENODEV;
	ndev = net_device->ndev;

	net_device->recv_buf =
		(void *)__get_free_pages(GFP_KERNEL|__GFP_ZERO,
				get_order(net_device->recv_buf_size));
	if (!net_device->recv_buf) {
		netdev_err(ndev, "unable to allocate receive "
			"buffer of size %d\n", net_device->recv_buf_size);
		ret = -ENOMEM;
		goto cleanup;
	}

	ret = vmbus_establish_gpadl(device->channel, net_device->recv_buf,
				    net_device->recv_buf_size,
				    &net_device->recv_buf_gpadl_handle);
	if (ret != 0) {
		netdev_err(ndev,
			"unable to establish receive buffer's gpadl\n");
		goto cleanup;
	}


	
	init_packet = &net_device->channel_init_pkt;

	memset(init_packet, 0, sizeof(struct nvsp_message));

	init_packet->hdr.msg_type = NVSP_MSG1_TYPE_SEND_RECV_BUF;
	init_packet->msg.v1_msg.send_recv_buf.
		gpadl_handle = net_device->recv_buf_gpadl_handle;
	init_packet->msg.v1_msg.
		send_recv_buf.id = NETVSC_RECEIVE_BUFFER_ID;

	
	ret = vmbus_sendpacket(device->channel, init_packet,
			       sizeof(struct nvsp_message),
			       (unsigned long)init_packet,
			       VM_PKT_DATA_INBAND,
			       VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);
	if (ret != 0) {
		netdev_err(ndev,
			"unable to send receive buffer's gpadl to netvsp\n");
		goto cleanup;
	}

	t = wait_for_completion_timeout(&net_device->channel_init_wait, 5*HZ);
	BUG_ON(t == 0);


	
	if (init_packet->msg.v1_msg.
	    send_recv_buf_complete.status != NVSP_STAT_SUCCESS) {
		netdev_err(ndev, "Unable to complete receive buffer "
			   "initialization with NetVsp - status %d\n",
			   init_packet->msg.v1_msg.
			   send_recv_buf_complete.status);
		ret = -EINVAL;
		goto cleanup;
	}

	

	net_device->recv_section_cnt = init_packet->msg.
		v1_msg.send_recv_buf_complete.num_sections;

	net_device->recv_section = kmemdup(
		init_packet->msg.v1_msg.send_recv_buf_complete.sections,
		net_device->recv_section_cnt *
		sizeof(struct nvsp_1_receive_buffer_section),
		GFP_KERNEL);
	if (net_device->recv_section == NULL) {
		ret = -EINVAL;
		goto cleanup;
	}

	if (net_device->recv_section_cnt != 1 ||
	    net_device->recv_section->offset != 0) {
		ret = -EINVAL;
		goto cleanup;
	}

	goto exit;

cleanup:
	netvsc_destroy_recv_buf(net_device);

exit:
	return ret;
}


static int negotiate_nvsp_ver(struct hv_device *device,
			      struct netvsc_device *net_device,
			      struct nvsp_message *init_packet,
			      u32 nvsp_ver)
{
	int ret, t;

	memset(init_packet, 0, sizeof(struct nvsp_message));
	init_packet->hdr.msg_type = NVSP_MSG_TYPE_INIT;
	init_packet->msg.init_msg.init.min_protocol_ver = nvsp_ver;
	init_packet->msg.init_msg.init.max_protocol_ver = nvsp_ver;

	
	ret = vmbus_sendpacket(device->channel, init_packet,
			       sizeof(struct nvsp_message),
			       (unsigned long)init_packet,
			       VM_PKT_DATA_INBAND,
			       VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);

	if (ret != 0)
		return ret;

	t = wait_for_completion_timeout(&net_device->channel_init_wait, 5*HZ);

	if (t == 0)
		return -ETIMEDOUT;

	if (init_packet->msg.init_msg.init_complete.status !=
	    NVSP_STAT_SUCCESS)
		return -EINVAL;

	if (nvsp_ver != NVSP_PROTOCOL_VERSION_2)
		return 0;

	
	memset(init_packet, 0, sizeof(struct nvsp_message));
	init_packet->hdr.msg_type = NVSP_MSG2_TYPE_SEND_NDIS_CONFIG;
	init_packet->msg.v2_msg.send_ndis_config.mtu = net_device->ndev->mtu;
	init_packet->msg.v2_msg.send_ndis_config.capability.ieee8021q = 1;

	ret = vmbus_sendpacket(device->channel, init_packet,
				sizeof(struct nvsp_message),
				(unsigned long)init_packet,
				VM_PKT_DATA_INBAND, 0);

	return ret;
}

static int netvsc_connect_vsp(struct hv_device *device)
{
	int ret;
	struct netvsc_device *net_device;
	struct nvsp_message *init_packet;
	int ndis_version;
	struct net_device *ndev;

	net_device = get_outbound_net_device(device);
	if (!net_device)
		return -ENODEV;
	ndev = net_device->ndev;

	init_packet = &net_device->channel_init_pkt;

	
	if (negotiate_nvsp_ver(device, net_device, init_packet,
			       NVSP_PROTOCOL_VERSION_2) == 0) {
		net_device->nvsp_version = NVSP_PROTOCOL_VERSION_2;
	} else if (negotiate_nvsp_ver(device, net_device, init_packet,
				    NVSP_PROTOCOL_VERSION_1) == 0) {
		net_device->nvsp_version = NVSP_PROTOCOL_VERSION_1;
	} else {
		ret = -EPROTO;
		goto cleanup;
	}

	pr_debug("Negotiated NVSP version:%x\n", net_device->nvsp_version);

	
	memset(init_packet, 0, sizeof(struct nvsp_message));

	ndis_version = 0x00050001;

	init_packet->hdr.msg_type = NVSP_MSG1_TYPE_SEND_NDIS_VER;
	init_packet->msg.v1_msg.
		send_ndis_ver.ndis_major_ver =
				(ndis_version & 0xFFFF0000) >> 16;
	init_packet->msg.v1_msg.
		send_ndis_ver.ndis_minor_ver =
				ndis_version & 0xFFFF;

	
	ret = vmbus_sendpacket(device->channel, init_packet,
				sizeof(struct nvsp_message),
				(unsigned long)init_packet,
				VM_PKT_DATA_INBAND, 0);
	if (ret != 0)
		goto cleanup;

	
	ret = netvsc_init_recv_buf(device);

cleanup:
	return ret;
}

static void netvsc_disconnect_vsp(struct netvsc_device *net_device)
{
	netvsc_destroy_recv_buf(net_device);
}

int netvsc_device_remove(struct hv_device *device)
{
	struct netvsc_device *net_device;
	struct hv_netvsc_packet *netvsc_packet, *pos;
	unsigned long flags;

	net_device = hv_get_drvdata(device);
	spin_lock_irqsave(&device->channel->inbound_lock, flags);
	net_device->destroy = true;
	spin_unlock_irqrestore(&device->channel->inbound_lock, flags);

	
	while (atomic_read(&net_device->num_outstanding_sends)) {
		dev_info(&device->device,
			"waiting for %d requests to complete...\n",
			atomic_read(&net_device->num_outstanding_sends));
		udelay(100);
	}

	netvsc_disconnect_vsp(net_device);


	spin_lock_irqsave(&device->channel->inbound_lock, flags);
	hv_set_drvdata(device, NULL);
	spin_unlock_irqrestore(&device->channel->inbound_lock, flags);

	dev_notice(&device->device, "net device safe to remove\n");

	
	vmbus_close(device->channel);

	
	list_for_each_entry_safe(netvsc_packet, pos,
				 &net_device->recv_pkt_list, list_ent) {
		list_del(&netvsc_packet->list_ent);
		kfree(netvsc_packet);
	}

	kfree(net_device);
	return 0;
}

static void netvsc_send_completion(struct hv_device *device,
				   struct vmpacket_descriptor *packet)
{
	struct netvsc_device *net_device;
	struct nvsp_message *nvsp_packet;
	struct hv_netvsc_packet *nvsc_packet;
	struct net_device *ndev;

	net_device = get_inbound_net_device(device);
	if (!net_device)
		return;
	ndev = net_device->ndev;

	nvsp_packet = (struct nvsp_message *)((unsigned long)packet +
			(packet->offset8 << 3));

	if ((nvsp_packet->hdr.msg_type == NVSP_MSG_TYPE_INIT_COMPLETE) ||
	    (nvsp_packet->hdr.msg_type ==
	     NVSP_MSG1_TYPE_SEND_RECV_BUF_COMPLETE) ||
	    (nvsp_packet->hdr.msg_type ==
	     NVSP_MSG1_TYPE_SEND_SEND_BUF_COMPLETE)) {
		
		memcpy(&net_device->channel_init_pkt, nvsp_packet,
		       sizeof(struct nvsp_message));
		complete(&net_device->channel_init_wait);
	} else if (nvsp_packet->hdr.msg_type ==
		   NVSP_MSG1_TYPE_SEND_RNDIS_PKT_COMPLETE) {
		
		nvsc_packet = (struct hv_netvsc_packet *)(unsigned long)
			packet->trans_id;

		
		nvsc_packet->completion.send.send_completion(
			nvsc_packet->completion.send.send_completion_ctx);

		atomic_dec(&net_device->num_outstanding_sends);

		if (netif_queue_stopped(ndev) && !net_device->start_remove)
			netif_wake_queue(ndev);
	} else {
		netdev_err(ndev, "Unknown send completion packet type- "
			   "%d received!!\n", nvsp_packet->hdr.msg_type);
	}

}

int netvsc_send(struct hv_device *device,
			struct hv_netvsc_packet *packet)
{
	struct netvsc_device *net_device;
	int ret = 0;
	struct nvsp_message sendMessage;
	struct net_device *ndev;

	net_device = get_outbound_net_device(device);
	if (!net_device)
		return -ENODEV;
	ndev = net_device->ndev;

	sendMessage.hdr.msg_type = NVSP_MSG1_TYPE_SEND_RNDIS_PKT;
	if (packet->is_data_pkt) {
		
		sendMessage.msg.v1_msg.send_rndis_pkt.channel_type = 0;
	} else {
		
		sendMessage.msg.v1_msg.send_rndis_pkt.channel_type = 1;
	}

	
	sendMessage.msg.v1_msg.send_rndis_pkt.send_buf_section_index =
		0xFFFFFFFF;
	sendMessage.msg.v1_msg.send_rndis_pkt.send_buf_section_size = 0;

	if (packet->page_buf_cnt) {
		ret = vmbus_sendpacket_pagebuffer(device->channel,
						  packet->page_buf,
						  packet->page_buf_cnt,
						  &sendMessage,
						  sizeof(struct nvsp_message),
						  (unsigned long)packet);
	} else {
		ret = vmbus_sendpacket(device->channel, &sendMessage,
				sizeof(struct nvsp_message),
				(unsigned long)packet,
				VM_PKT_DATA_INBAND,
				VMBUS_DATA_PACKET_FLAG_COMPLETION_REQUESTED);

	}

	if (ret == 0) {
		atomic_inc(&net_device->num_outstanding_sends);
	} else if (ret == -EAGAIN) {
		netif_stop_queue(ndev);
		if (atomic_read(&net_device->num_outstanding_sends) < 1)
			netif_wake_queue(ndev);
	} else {
		netdev_err(ndev, "Unable to send packet %p ret %d\n",
			   packet, ret);
	}

	return ret;
}

static void netvsc_send_recv_completion(struct hv_device *device,
					u64 transaction_id)
{
	struct nvsp_message recvcompMessage;
	int retries = 0;
	int ret;
	struct net_device *ndev;
	struct netvsc_device *net_device = hv_get_drvdata(device);

	ndev = net_device->ndev;

	recvcompMessage.hdr.msg_type =
				NVSP_MSG1_TYPE_SEND_RNDIS_PKT_COMPLETE;

	
	recvcompMessage.msg.v1_msg.send_rndis_pkt_complete.status =
		NVSP_STAT_SUCCESS;

retry_send_cmplt:
	
	ret = vmbus_sendpacket(device->channel, &recvcompMessage,
			       sizeof(struct nvsp_message), transaction_id,
			       VM_PKT_COMP, 0);
	if (ret == 0) {
		
		
	} else if (ret == -EAGAIN) {
		
		retries++;
		netdev_err(ndev, "unable to send receive completion pkt"
			" (tid %llx)...retrying %d\n", transaction_id, retries);

		if (retries < 4) {
			udelay(100);
			goto retry_send_cmplt;
		} else {
			netdev_err(ndev, "unable to send receive "
				"completion pkt (tid %llx)...give up retrying\n",
				transaction_id);
		}
	} else {
		netdev_err(ndev, "unable to send receive "
			"completion pkt - %llx\n", transaction_id);
	}
}

static void netvsc_receive_completion(void *context)
{
	struct hv_netvsc_packet *packet = context;
	struct hv_device *device = (struct hv_device *)packet->device;
	struct netvsc_device *net_device;
	u64 transaction_id = 0;
	bool fsend_receive_comp = false;
	unsigned long flags;
	struct net_device *ndev;

	net_device = get_inbound_net_device(device);
	if (!net_device)
		return;
	ndev = net_device->ndev;

	
	spin_lock_irqsave(&net_device->recv_pkt_list_lock, flags);

	packet->xfer_page_pkt->count--;

	if (packet->xfer_page_pkt->count == 0) {
		fsend_receive_comp = true;
		transaction_id = packet->completion.recv.recv_completion_tid;
		list_add_tail(&packet->xfer_page_pkt->list_ent,
			      &net_device->recv_pkt_list);

	}

	
	list_add_tail(&packet->list_ent, &net_device->recv_pkt_list);
	spin_unlock_irqrestore(&net_device->recv_pkt_list_lock, flags);

	
	if (fsend_receive_comp)
		netvsc_send_recv_completion(device, transaction_id);

}

static void netvsc_receive(struct hv_device *device,
			    struct vmpacket_descriptor *packet)
{
	struct netvsc_device *net_device;
	struct vmtransfer_page_packet_header *vmxferpage_packet;
	struct nvsp_message *nvsp_packet;
	struct hv_netvsc_packet *netvsc_packet = NULL;
	
	struct xferpage_packet *xferpage_packet = NULL;
	int i;
	int count = 0;
	unsigned long flags;
	struct net_device *ndev;

	LIST_HEAD(listHead);

	net_device = get_inbound_net_device(device);
	if (!net_device)
		return;
	ndev = net_device->ndev;

	if (packet->type != VM_PKT_DATA_USING_XFER_PAGES) {
		netdev_err(ndev, "Unknown packet type received - %d\n",
			   packet->type);
		return;
	}

	nvsp_packet = (struct nvsp_message *)((unsigned long)packet +
			(packet->offset8 << 3));

	
	if (nvsp_packet->hdr.msg_type !=
	    NVSP_MSG1_TYPE_SEND_RNDIS_PKT) {
		netdev_err(ndev, "Unknown nvsp packet type received-"
			" %d\n", nvsp_packet->hdr.msg_type);
		return;
	}

	vmxferpage_packet = (struct vmtransfer_page_packet_header *)packet;

	if (vmxferpage_packet->xfer_pageset_id != NETVSC_RECEIVE_BUFFER_ID) {
		netdev_err(ndev, "Invalid xfer page set id - "
			   "expecting %x got %x\n", NETVSC_RECEIVE_BUFFER_ID,
			   vmxferpage_packet->xfer_pageset_id);
		return;
	}

	spin_lock_irqsave(&net_device->recv_pkt_list_lock, flags);
	while (!list_empty(&net_device->recv_pkt_list)) {
		list_move_tail(net_device->recv_pkt_list.next, &listHead);
		if (++count == vmxferpage_packet->range_cnt + 1)
			break;
	}
	spin_unlock_irqrestore(&net_device->recv_pkt_list_lock, flags);

	if (count < 2) {
		netdev_err(ndev, "Got only %d netvsc pkt...needed "
			"%d pkts. Dropping this xfer page packet completely!\n",
			count, vmxferpage_packet->range_cnt + 1);

		
		spin_lock_irqsave(&net_device->recv_pkt_list_lock, flags);
		for (i = count; i != 0; i--) {
			list_move_tail(listHead.next,
				       &net_device->recv_pkt_list);
		}
		spin_unlock_irqrestore(&net_device->recv_pkt_list_lock,
				       flags);

		netvsc_send_recv_completion(device,
					    vmxferpage_packet->d.trans_id);

		return;
	}

	
	xferpage_packet = (struct xferpage_packet *)listHead.next;
	list_del(&xferpage_packet->list_ent);

	
	xferpage_packet->count = count - 1;

	if (xferpage_packet->count != vmxferpage_packet->range_cnt) {
		netdev_err(ndev, "Needed %d netvsc pkts to satisfy "
			"this xfer page...got %d\n",
			vmxferpage_packet->range_cnt, xferpage_packet->count);
	}

	
	for (i = 0; i < (count - 1); i++) {
		netvsc_packet = (struct hv_netvsc_packet *)listHead.next;
		list_del(&netvsc_packet->list_ent);

		
		netvsc_packet->xfer_page_pkt = xferpage_packet;
		netvsc_packet->completion.recv.recv_completion =
					netvsc_receive_completion;
		netvsc_packet->completion.recv.recv_completion_ctx =
					netvsc_packet;
		netvsc_packet->device = device;
		
		netvsc_packet->completion.recv.recv_completion_tid =
					vmxferpage_packet->d.trans_id;

		netvsc_packet->data = (void *)((unsigned long)net_device->
			recv_buf + vmxferpage_packet->ranges[i].byte_offset);
		netvsc_packet->total_data_buflen =
					vmxferpage_packet->ranges[i].byte_count;

		
		rndis_filter_receive(device, netvsc_packet);

		netvsc_receive_completion(netvsc_packet->
				completion.recv.recv_completion_ctx);
	}

}

static void netvsc_channel_cb(void *context)
{
	int ret;
	struct hv_device *device = context;
	struct netvsc_device *net_device;
	u32 bytes_recvd;
	u64 request_id;
	unsigned char *packet;
	struct vmpacket_descriptor *desc;
	unsigned char *buffer;
	int bufferlen = NETVSC_PACKET_SIZE;
	struct net_device *ndev;

	packet = kzalloc(NETVSC_PACKET_SIZE * sizeof(unsigned char),
			 GFP_ATOMIC);
	if (!packet)
		return;
	buffer = packet;

	net_device = get_inbound_net_device(device);
	if (!net_device)
		goto out;
	ndev = net_device->ndev;

	do {
		ret = vmbus_recvpacket_raw(device->channel, buffer, bufferlen,
					   &bytes_recvd, &request_id);
		if (ret == 0) {
			if (bytes_recvd > 0) {
				desc = (struct vmpacket_descriptor *)buffer;
				switch (desc->type) {
				case VM_PKT_COMP:
					netvsc_send_completion(device, desc);
					break;

				case VM_PKT_DATA_USING_XFER_PAGES:
					netvsc_receive(device, desc);
					break;

				default:
					netdev_err(ndev,
						   "unhandled packet type %d, "
						   "tid %llx len %d\n",
						   desc->type, request_id,
						   bytes_recvd);
					break;
				}

				
				if (bufferlen > NETVSC_PACKET_SIZE) {
					kfree(buffer);
					buffer = packet;
					bufferlen = NETVSC_PACKET_SIZE;
				}
			} else {
				
				if (bufferlen > NETVSC_PACKET_SIZE) {
					kfree(buffer);
					buffer = packet;
					bufferlen = NETVSC_PACKET_SIZE;
				}

				break;
			}
		} else if (ret == -ENOBUFS) {
			
			buffer = kmalloc(bytes_recvd, GFP_ATOMIC);
			if (buffer == NULL) {
				
				netdev_err(ndev,
					   "unable to allocate buffer of size "
					   "(%d)!!\n", bytes_recvd);
				break;
			}

			bufferlen = bytes_recvd;
		}
	} while (1);

out:
	kfree(buffer);
	return;
}

int netvsc_device_add(struct hv_device *device, void *additional_info)
{
	int ret = 0;
	int i;
	int ring_size =
	((struct netvsc_device_info *)additional_info)->ring_size;
	struct netvsc_device *net_device;
	struct hv_netvsc_packet *packet, *pos;
	struct net_device *ndev;

	net_device = alloc_net_device(device);
	if (!net_device) {
		ret = -ENOMEM;
		goto cleanup;
	}

	ndev = net_device->ndev;

	
	net_device->recv_buf_size = NETVSC_RECEIVE_BUFFER_SIZE;
	spin_lock_init(&net_device->recv_pkt_list_lock);

	INIT_LIST_HEAD(&net_device->recv_pkt_list);

	for (i = 0; i < NETVSC_RECEIVE_PACKETLIST_COUNT; i++) {
		packet = kzalloc(sizeof(struct hv_netvsc_packet) +
				 (NETVSC_RECEIVE_SG_COUNT *
				  sizeof(struct hv_page_buffer)), GFP_KERNEL);
		if (!packet)
			break;

		list_add_tail(&packet->list_ent,
			      &net_device->recv_pkt_list);
	}
	init_completion(&net_device->channel_init_wait);

	
	ret = vmbus_open(device->channel, ring_size * PAGE_SIZE,
			 ring_size * PAGE_SIZE, NULL, 0,
			 netvsc_channel_cb, device);

	if (ret != 0) {
		netdev_err(ndev, "unable to open channel: %d\n", ret);
		goto cleanup;
	}

	
	pr_info("hv_netvsc channel opened successfully\n");

	
	ret = netvsc_connect_vsp(device);
	if (ret != 0) {
		netdev_err(ndev,
			"unable to connect to NetVSP - %d\n", ret);
		goto close;
	}

	return ret;

close:
	
	vmbus_close(device->channel);

cleanup:

	if (net_device) {
		list_for_each_entry_safe(packet, pos,
					 &net_device->recv_pkt_list,
					 list_ent) {
			list_del(&packet->list_ent);
			kfree(packet);
		}

		kfree(net_device);
	}

	return ret;
}
