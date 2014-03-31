/****************************************************************************
 * Driver for Solarflare Solarstorm network controllers and boards
 * Copyright 2005-2006 Fen Systems Ltd.
 * Copyright 2005-2010 Solarflare Communications Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, incorporated herein by reference.
 */

#include <linux/pci.h>
#include <linux/tcp.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/ipv6.h>
#include <linux/slab.h>
#include <net/ipv6.h>
#include <linux/if_ether.h>
#include <linux/highmem.h>
#include "net_driver.h"
#include "efx.h"
#include "nic.h"
#include "workarounds.h"

#define EFX_TXQ_THRESHOLD(_efx) ((_efx)->txq_entries / 2u)

static void efx_dequeue_buffer(struct efx_tx_queue *tx_queue,
			       struct efx_tx_buffer *buffer,
			       unsigned int *pkts_compl,
			       unsigned int *bytes_compl)
{
	if (buffer->unmap_len) {
		struct pci_dev *pci_dev = tx_queue->efx->pci_dev;
		dma_addr_t unmap_addr = (buffer->dma_addr + buffer->len -
					 buffer->unmap_len);
		if (buffer->unmap_single)
			pci_unmap_single(pci_dev, unmap_addr, buffer->unmap_len,
					 PCI_DMA_TODEVICE);
		else
			pci_unmap_page(pci_dev, unmap_addr, buffer->unmap_len,
				       PCI_DMA_TODEVICE);
		buffer->unmap_len = 0;
		buffer->unmap_single = false;
	}

	if (buffer->skb) {
		(*pkts_compl)++;
		(*bytes_compl) += buffer->skb->len;
		dev_kfree_skb_any((struct sk_buff *) buffer->skb);
		buffer->skb = NULL;
		netif_vdbg(tx_queue->efx, tx_done, tx_queue->efx->net_dev,
			   "TX queue %d transmission id %x complete\n",
			   tx_queue->queue, tx_queue->read_count);
	}
}

struct efx_tso_header {
	union {
		struct efx_tso_header *next;
		size_t unmap_len;
	};
	dma_addr_t dma_addr;
};

static int efx_enqueue_skb_tso(struct efx_tx_queue *tx_queue,
			       struct sk_buff *skb);
static void efx_fini_tso(struct efx_tx_queue *tx_queue);
static void efx_tsoh_heap_free(struct efx_tx_queue *tx_queue,
			       struct efx_tso_header *tsoh);

static void efx_tsoh_free(struct efx_tx_queue *tx_queue,
			  struct efx_tx_buffer *buffer)
{
	if (buffer->tsoh) {
		if (likely(!buffer->tsoh->unmap_len)) {
			buffer->tsoh->next = tx_queue->tso_headers_free;
			tx_queue->tso_headers_free = buffer->tsoh;
		} else {
			efx_tsoh_heap_free(tx_queue, buffer->tsoh);
		}
		buffer->tsoh = NULL;
	}
}


static inline unsigned
efx_max_tx_len(struct efx_nic *efx, dma_addr_t dma_addr)
{
	unsigned len = (~dma_addr & (EFX_PAGE_SIZE - 1)) + 1;

	
	if (EFX_WORKAROUND_5391(efx) && (dma_addr & 0xf))
		len = min_t(unsigned, len, 512 - (dma_addr & 0xf));

	return len;
}

netdev_tx_t efx_enqueue_skb(struct efx_tx_queue *tx_queue, struct sk_buff *skb)
{
	struct efx_nic *efx = tx_queue->efx;
	struct pci_dev *pci_dev = efx->pci_dev;
	struct efx_tx_buffer *buffer;
	skb_frag_t *fragment;
	unsigned int len, unmap_len = 0, fill_level, insert_ptr;
	dma_addr_t dma_addr, unmap_addr = 0;
	unsigned int dma_len;
	bool unmap_single;
	int q_space, i = 0;
	netdev_tx_t rc = NETDEV_TX_OK;

	EFX_BUG_ON_PARANOID(tx_queue->write_count != tx_queue->insert_count);

	if (skb_shinfo(skb)->gso_size)
		return efx_enqueue_skb_tso(tx_queue, skb);

	
	len = skb_headlen(skb);

	
	if (EFX_WORKAROUND_15592(efx) && skb->len <= 32) {
		EFX_BUG_ON_PARANOID(skb->data_len);
		len = 32 + 1;
		if (skb_pad(skb, len - skb->len))
			return NETDEV_TX_OK;
	}

	fill_level = tx_queue->insert_count - tx_queue->old_read_count;
	q_space = efx->txq_entries - 1 - fill_level;

	unmap_single = true;
	dma_addr = pci_map_single(pci_dev, skb->data, len, PCI_DMA_TODEVICE);

	
	while (1) {
		if (unlikely(pci_dma_mapping_error(pci_dev, dma_addr)))
			goto pci_err;

		unmap_len = len;
		unmap_addr = dma_addr;

		
		do {
			if (unlikely(q_space-- <= 0)) {
				netif_tx_stop_queue(tx_queue->core_txq);
				smp_mb();
				tx_queue->old_read_count =
					ACCESS_ONCE(tx_queue->read_count);
				fill_level = (tx_queue->insert_count
					      - tx_queue->old_read_count);
				q_space = efx->txq_entries - 1 - fill_level;
				if (unlikely(q_space-- <= 0)) {
					rc = NETDEV_TX_BUSY;
					goto unwind;
				}
				smp_mb();
				if (likely(!efx->loopback_selftest))
					netif_tx_start_queue(
						tx_queue->core_txq);
			}

			insert_ptr = tx_queue->insert_count & tx_queue->ptr_mask;
			buffer = &tx_queue->buffer[insert_ptr];
			efx_tsoh_free(tx_queue, buffer);
			EFX_BUG_ON_PARANOID(buffer->tsoh);
			EFX_BUG_ON_PARANOID(buffer->skb);
			EFX_BUG_ON_PARANOID(buffer->len);
			EFX_BUG_ON_PARANOID(!buffer->continuation);
			EFX_BUG_ON_PARANOID(buffer->unmap_len);

			dma_len = efx_max_tx_len(efx, dma_addr);
			if (likely(dma_len >= len))
				dma_len = len;

			
			buffer->len = dma_len;
			buffer->dma_addr = dma_addr;
			len -= dma_len;
			dma_addr += dma_len;
			++tx_queue->insert_count;
		} while (len);

		
		buffer->unmap_single = unmap_single;
		buffer->unmap_len = unmap_len;
		unmap_len = 0;

		
		if (i >= skb_shinfo(skb)->nr_frags)
			break;
		fragment = &skb_shinfo(skb)->frags[i];
		len = skb_frag_size(fragment);
		i++;
		
		unmap_single = false;
		dma_addr = skb_frag_dma_map(&pci_dev->dev, fragment, 0, len,
					    DMA_TO_DEVICE);
	}

	
	buffer->skb = skb;
	buffer->continuation = false;

	netdev_tx_sent_queue(tx_queue->core_txq, skb->len);

	
	efx_nic_push_buffers(tx_queue);

	return NETDEV_TX_OK;

 pci_err:
	netif_err(efx, tx_err, efx->net_dev,
		  " TX queue %d could not map skb with %d bytes %d "
		  "fragments for DMA\n", tx_queue->queue, skb->len,
		  skb_shinfo(skb)->nr_frags + 1);

	
	dev_kfree_skb_any(skb);

 unwind:
	
	while (tx_queue->insert_count != tx_queue->write_count) {
		unsigned int pkts_compl = 0, bytes_compl = 0;
		--tx_queue->insert_count;
		insert_ptr = tx_queue->insert_count & tx_queue->ptr_mask;
		buffer = &tx_queue->buffer[insert_ptr];
		efx_dequeue_buffer(tx_queue, buffer, &pkts_compl, &bytes_compl);
		buffer->len = 0;
	}

	
	if (unmap_len) {
		if (unmap_single)
			pci_unmap_single(pci_dev, unmap_addr, unmap_len,
					 PCI_DMA_TODEVICE);
		else
			pci_unmap_page(pci_dev, unmap_addr, unmap_len,
				       PCI_DMA_TODEVICE);
	}

	return rc;
}

static void efx_dequeue_buffers(struct efx_tx_queue *tx_queue,
				unsigned int index,
				unsigned int *pkts_compl,
				unsigned int *bytes_compl)
{
	struct efx_nic *efx = tx_queue->efx;
	unsigned int stop_index, read_ptr;

	stop_index = (index + 1) & tx_queue->ptr_mask;
	read_ptr = tx_queue->read_count & tx_queue->ptr_mask;

	while (read_ptr != stop_index) {
		struct efx_tx_buffer *buffer = &tx_queue->buffer[read_ptr];
		if (unlikely(buffer->len == 0)) {
			netif_err(efx, tx_err, efx->net_dev,
				  "TX queue %d spurious TX completion id %x\n",
				  tx_queue->queue, read_ptr);
			efx_schedule_reset(efx, RESET_TYPE_TX_SKIP);
			return;
		}

		efx_dequeue_buffer(tx_queue, buffer, pkts_compl, bytes_compl);
		buffer->continuation = true;
		buffer->len = 0;

		++tx_queue->read_count;
		read_ptr = tx_queue->read_count & tx_queue->ptr_mask;
	}
}

netdev_tx_t efx_hard_start_xmit(struct sk_buff *skb,
				struct net_device *net_dev)
{
	struct efx_nic *efx = netdev_priv(net_dev);
	struct efx_tx_queue *tx_queue;
	unsigned index, type;

	EFX_WARN_ON_PARANOID(!netif_device_present(net_dev));

	index = skb_get_queue_mapping(skb);
	type = skb->ip_summed == CHECKSUM_PARTIAL ? EFX_TXQ_TYPE_OFFLOAD : 0;
	if (index >= efx->n_tx_channels) {
		index -= efx->n_tx_channels;
		type |= EFX_TXQ_TYPE_HIGHPRI;
	}
	tx_queue = efx_get_tx_queue(efx, index, type);

	return efx_enqueue_skb(tx_queue, skb);
}

void efx_init_tx_queue_core_txq(struct efx_tx_queue *tx_queue)
{
	struct efx_nic *efx = tx_queue->efx;

	
	tx_queue->core_txq =
		netdev_get_tx_queue(efx->net_dev,
				    tx_queue->queue / EFX_TXQ_TYPES +
				    ((tx_queue->queue & EFX_TXQ_TYPE_HIGHPRI) ?
				     efx->n_tx_channels : 0));
}

int efx_setup_tc(struct net_device *net_dev, u8 num_tc)
{
	struct efx_nic *efx = netdev_priv(net_dev);
	struct efx_channel *channel;
	struct efx_tx_queue *tx_queue;
	unsigned tc;
	int rc;

	if (efx_nic_rev(efx) < EFX_REV_FALCON_B0 || num_tc > EFX_MAX_TX_TC)
		return -EINVAL;

	if (num_tc == net_dev->num_tc)
		return 0;

	for (tc = 0; tc < num_tc; tc++) {
		net_dev->tc_to_txq[tc].offset = tc * efx->n_tx_channels;
		net_dev->tc_to_txq[tc].count = efx->n_tx_channels;
	}

	if (num_tc > net_dev->num_tc) {
		
		efx_for_each_channel(channel, efx) {
			efx_for_each_possible_channel_tx_queue(tx_queue,
							       channel) {
				if (!(tx_queue->queue & EFX_TXQ_TYPE_HIGHPRI))
					continue;
				if (!tx_queue->buffer) {
					rc = efx_probe_tx_queue(tx_queue);
					if (rc)
						return rc;
				}
				if (!tx_queue->initialised)
					efx_init_tx_queue(tx_queue);
				efx_init_tx_queue_core_txq(tx_queue);
			}
		}
	} else {
		
		net_dev->num_tc = num_tc;
	}

	rc = netif_set_real_num_tx_queues(net_dev,
					  max_t(int, num_tc, 1) *
					  efx->n_tx_channels);
	if (rc)
		return rc;


	net_dev->num_tc = num_tc;
	return 0;
}

void efx_xmit_done(struct efx_tx_queue *tx_queue, unsigned int index)
{
	unsigned fill_level;
	struct efx_nic *efx = tx_queue->efx;
	unsigned int pkts_compl = 0, bytes_compl = 0;

	EFX_BUG_ON_PARANOID(index > tx_queue->ptr_mask);

	efx_dequeue_buffers(tx_queue, index, &pkts_compl, &bytes_compl);
	netdev_tx_completed_queue(tx_queue->core_txq, pkts_compl, bytes_compl);

	smp_mb();
	if (unlikely(netif_tx_queue_stopped(tx_queue->core_txq)) &&
	    likely(efx->port_enabled) &&
	    likely(netif_device_present(efx->net_dev))) {
		fill_level = tx_queue->insert_count - tx_queue->read_count;
		if (fill_level < EFX_TXQ_THRESHOLD(efx))
			netif_tx_wake_queue(tx_queue->core_txq);
	}

	
	if ((int)(tx_queue->read_count - tx_queue->old_write_count) >= 0) {
		tx_queue->old_write_count = ACCESS_ONCE(tx_queue->write_count);
		if (tx_queue->read_count == tx_queue->old_write_count) {
			smp_mb();
			tx_queue->empty_read_count =
				tx_queue->read_count | EFX_EMPTY_COUNT_VALID;
		}
	}
}

int efx_probe_tx_queue(struct efx_tx_queue *tx_queue)
{
	struct efx_nic *efx = tx_queue->efx;
	unsigned int entries;
	int i, rc;

	
	entries = max(roundup_pow_of_two(efx->txq_entries), EFX_MIN_DMAQ_SIZE);
	EFX_BUG_ON_PARANOID(entries > EFX_MAX_DMAQ_SIZE);
	tx_queue->ptr_mask = entries - 1;

	netif_dbg(efx, probe, efx->net_dev,
		  "creating TX queue %d size %#x mask %#x\n",
		  tx_queue->queue, efx->txq_entries, tx_queue->ptr_mask);

	
	tx_queue->buffer = kcalloc(entries, sizeof(*tx_queue->buffer),
				   GFP_KERNEL);
	if (!tx_queue->buffer)
		return -ENOMEM;
	for (i = 0; i <= tx_queue->ptr_mask; ++i)
		tx_queue->buffer[i].continuation = true;

	
	rc = efx_nic_probe_tx(tx_queue);
	if (rc)
		goto fail;

	return 0;

 fail:
	kfree(tx_queue->buffer);
	tx_queue->buffer = NULL;
	return rc;
}

void efx_init_tx_queue(struct efx_tx_queue *tx_queue)
{
	netif_dbg(tx_queue->efx, drv, tx_queue->efx->net_dev,
		  "initialising TX queue %d\n", tx_queue->queue);

	tx_queue->insert_count = 0;
	tx_queue->write_count = 0;
	tx_queue->old_write_count = 0;
	tx_queue->read_count = 0;
	tx_queue->old_read_count = 0;
	tx_queue->empty_read_count = 0 | EFX_EMPTY_COUNT_VALID;

	
	efx_nic_init_tx(tx_queue);

	tx_queue->initialised = true;
}

void efx_release_tx_buffers(struct efx_tx_queue *tx_queue)
{
	struct efx_tx_buffer *buffer;

	if (!tx_queue->buffer)
		return;

	
	while (tx_queue->read_count != tx_queue->write_count) {
		unsigned int pkts_compl = 0, bytes_compl = 0;
		buffer = &tx_queue->buffer[tx_queue->read_count & tx_queue->ptr_mask];
		efx_dequeue_buffer(tx_queue, buffer, &pkts_compl, &bytes_compl);
		buffer->continuation = true;
		buffer->len = 0;

		++tx_queue->read_count;
	}
	netdev_tx_reset_queue(tx_queue->core_txq);
}

void efx_fini_tx_queue(struct efx_tx_queue *tx_queue)
{
	if (!tx_queue->initialised)
		return;

	netif_dbg(tx_queue->efx, drv, tx_queue->efx->net_dev,
		  "shutting down TX queue %d\n", tx_queue->queue);

	tx_queue->initialised = false;

	
	efx_nic_fini_tx(tx_queue);

	efx_release_tx_buffers(tx_queue);

	
	efx_fini_tso(tx_queue);
}

void efx_remove_tx_queue(struct efx_tx_queue *tx_queue)
{
	if (!tx_queue->buffer)
		return;

	netif_dbg(tx_queue->efx, drv, tx_queue->efx->net_dev,
		  "destroying TX queue %d\n", tx_queue->queue);
	efx_nic_remove_tx(tx_queue);

	kfree(tx_queue->buffer);
	tx_queue->buffer = NULL;
}



#ifdef CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS
#define TSOH_OFFSET	0
#else
#define TSOH_OFFSET	NET_IP_ALIGN
#endif

#define TSOH_BUFFER(tsoh)	((u8 *)(tsoh + 1) + TSOH_OFFSET)

#define TSOH_SIZE(hdr_len)					\
	(sizeof(struct efx_tso_header) + TSOH_OFFSET + hdr_len)

#define TSOH_STD_SIZE		128

#define PTR_DIFF(p1, p2)  ((u8 *)(p1) - (u8 *)(p2))
#define ETH_HDR_LEN(skb)  (skb_network_header(skb) - (skb)->data)
#define SKB_TCP_OFF(skb)  PTR_DIFF(tcp_hdr(skb), (skb)->data)
#define SKB_IPV4_OFF(skb) PTR_DIFF(ip_hdr(skb), (skb)->data)
#define SKB_IPV6_OFF(skb) PTR_DIFF(ipv6_hdr(skb), (skb)->data)

struct tso_state {
	
	unsigned out_len;
	unsigned seqnum;
	unsigned ipv4_id;
	unsigned packet_space;

	
	dma_addr_t dma_addr;
	unsigned in_len;
	unsigned unmap_len;
	dma_addr_t unmap_addr;
	bool unmap_single;

	__be16 protocol;
	unsigned header_len;
	int full_packet_size;
};


static __be16 efx_tso_check_protocol(struct sk_buff *skb)
{
	__be16 protocol = skb->protocol;

	EFX_BUG_ON_PARANOID(((struct ethhdr *)skb->data)->h_proto !=
			    protocol);
	if (protocol == htons(ETH_P_8021Q)) {
		struct vlan_ethhdr *veh = (struct vlan_ethhdr *)skb->data;
		protocol = veh->h_vlan_encapsulated_proto;
		skb_set_network_header(skb, sizeof(*veh));
		if (protocol == htons(ETH_P_IP))
			skb_set_transport_header(skb, sizeof(*veh) +
						 4 * ip_hdr(skb)->ihl);
		else if (protocol == htons(ETH_P_IPV6))
			skb_set_transport_header(skb, sizeof(*veh) +
						 sizeof(struct ipv6hdr));
	}

	if (protocol == htons(ETH_P_IP)) {
		EFX_BUG_ON_PARANOID(ip_hdr(skb)->protocol != IPPROTO_TCP);
	} else {
		EFX_BUG_ON_PARANOID(protocol != htons(ETH_P_IPV6));
		EFX_BUG_ON_PARANOID(ipv6_hdr(skb)->nexthdr != NEXTHDR_TCP);
	}
	EFX_BUG_ON_PARANOID((PTR_DIFF(tcp_hdr(skb), skb->data)
			     + (tcp_hdr(skb)->doff << 2u)) >
			    skb_headlen(skb));

	return protocol;
}


static int efx_tsoh_block_alloc(struct efx_tx_queue *tx_queue)
{

	struct pci_dev *pci_dev = tx_queue->efx->pci_dev;
	struct efx_tso_header *tsoh;
	dma_addr_t dma_addr;
	u8 *base_kva, *kva;

	base_kva = pci_alloc_consistent(pci_dev, PAGE_SIZE, &dma_addr);
	if (base_kva == NULL) {
		netif_err(tx_queue->efx, tx_err, tx_queue->efx->net_dev,
			  "Unable to allocate page for TSO headers\n");
		return -ENOMEM;
	}

	
	EFX_BUG_ON_PARANOID(dma_addr & (PAGE_SIZE - 1u));

	for (kva = base_kva; kva < base_kva + PAGE_SIZE; kva += TSOH_STD_SIZE) {
		tsoh = (struct efx_tso_header *)kva;
		tsoh->dma_addr = dma_addr + (TSOH_BUFFER(tsoh) - base_kva);
		tsoh->next = tx_queue->tso_headers_free;
		tx_queue->tso_headers_free = tsoh;
	}

	return 0;
}


static void efx_tsoh_block_free(struct efx_tx_queue *tx_queue,
				struct efx_tso_header *tsoh,
				struct pci_dev *pci_dev)
{
	struct efx_tso_header **p;
	unsigned long base_kva;
	dma_addr_t base_dma;

	base_kva = (unsigned long)tsoh & PAGE_MASK;
	base_dma = tsoh->dma_addr & PAGE_MASK;

	p = &tx_queue->tso_headers_free;
	while (*p != NULL) {
		if (((unsigned long)*p & PAGE_MASK) == base_kva)
			*p = (*p)->next;
		else
			p = &(*p)->next;
	}

	pci_free_consistent(pci_dev, PAGE_SIZE, (void *)base_kva, base_dma);
}

static struct efx_tso_header *
efx_tsoh_heap_alloc(struct efx_tx_queue *tx_queue, size_t header_len)
{
	struct efx_tso_header *tsoh;

	tsoh = kmalloc(TSOH_SIZE(header_len), GFP_ATOMIC | GFP_DMA);
	if (unlikely(!tsoh))
		return NULL;

	tsoh->dma_addr = pci_map_single(tx_queue->efx->pci_dev,
					TSOH_BUFFER(tsoh), header_len,
					PCI_DMA_TODEVICE);
	if (unlikely(pci_dma_mapping_error(tx_queue->efx->pci_dev,
					   tsoh->dma_addr))) {
		kfree(tsoh);
		return NULL;
	}

	tsoh->unmap_len = header_len;
	return tsoh;
}

static void
efx_tsoh_heap_free(struct efx_tx_queue *tx_queue, struct efx_tso_header *tsoh)
{
	pci_unmap_single(tx_queue->efx->pci_dev,
			 tsoh->dma_addr, tsoh->unmap_len,
			 PCI_DMA_TODEVICE);
	kfree(tsoh);
}

static int efx_tx_queue_insert(struct efx_tx_queue *tx_queue,
			       dma_addr_t dma_addr, unsigned len,
			       struct efx_tx_buffer **final_buffer)
{
	struct efx_tx_buffer *buffer;
	struct efx_nic *efx = tx_queue->efx;
	unsigned dma_len, fill_level, insert_ptr;
	int q_space;

	EFX_BUG_ON_PARANOID(len <= 0);

	fill_level = tx_queue->insert_count - tx_queue->old_read_count;
	
	q_space = efx->txq_entries - 1 - fill_level;

	while (1) {
		if (unlikely(q_space-- <= 0)) {
			netif_tx_stop_queue(tx_queue->core_txq);
			smp_mb();
			tx_queue->old_read_count =
				ACCESS_ONCE(tx_queue->read_count);
			fill_level = (tx_queue->insert_count
				      - tx_queue->old_read_count);
			q_space = efx->txq_entries - 1 - fill_level;
			if (unlikely(q_space-- <= 0)) {
				*final_buffer = NULL;
				return 1;
			}
			smp_mb();
			netif_tx_start_queue(tx_queue->core_txq);
		}

		insert_ptr = tx_queue->insert_count & tx_queue->ptr_mask;
		buffer = &tx_queue->buffer[insert_ptr];
		++tx_queue->insert_count;

		EFX_BUG_ON_PARANOID(tx_queue->insert_count -
				    tx_queue->read_count >=
				    efx->txq_entries);

		efx_tsoh_free(tx_queue, buffer);
		EFX_BUG_ON_PARANOID(buffer->len);
		EFX_BUG_ON_PARANOID(buffer->unmap_len);
		EFX_BUG_ON_PARANOID(buffer->skb);
		EFX_BUG_ON_PARANOID(!buffer->continuation);
		EFX_BUG_ON_PARANOID(buffer->tsoh);

		buffer->dma_addr = dma_addr;

		dma_len = efx_max_tx_len(efx, dma_addr);

		
		if (dma_len >= len)
			break;

		buffer->len = dma_len; 
		dma_addr += dma_len;
		len -= dma_len;
	}

	EFX_BUG_ON_PARANOID(!len);
	buffer->len = len;
	*final_buffer = buffer;
	return 0;
}


static void efx_tso_put_header(struct efx_tx_queue *tx_queue,
			       struct efx_tso_header *tsoh, unsigned len)
{
	struct efx_tx_buffer *buffer;

	buffer = &tx_queue->buffer[tx_queue->insert_count & tx_queue->ptr_mask];
	efx_tsoh_free(tx_queue, buffer);
	EFX_BUG_ON_PARANOID(buffer->len);
	EFX_BUG_ON_PARANOID(buffer->unmap_len);
	EFX_BUG_ON_PARANOID(buffer->skb);
	EFX_BUG_ON_PARANOID(!buffer->continuation);
	EFX_BUG_ON_PARANOID(buffer->tsoh);
	buffer->len = len;
	buffer->dma_addr = tsoh->dma_addr;
	buffer->tsoh = tsoh;

	++tx_queue->insert_count;
}


static void efx_enqueue_unwind(struct efx_tx_queue *tx_queue)
{
	struct efx_tx_buffer *buffer;
	dma_addr_t unmap_addr;

	
	while (tx_queue->insert_count != tx_queue->write_count) {
		--tx_queue->insert_count;
		buffer = &tx_queue->buffer[tx_queue->insert_count &
					   tx_queue->ptr_mask];
		efx_tsoh_free(tx_queue, buffer);
		EFX_BUG_ON_PARANOID(buffer->skb);
		if (buffer->unmap_len) {
			unmap_addr = (buffer->dma_addr + buffer->len -
				      buffer->unmap_len);
			if (buffer->unmap_single)
				pci_unmap_single(tx_queue->efx->pci_dev,
						 unmap_addr, buffer->unmap_len,
						 PCI_DMA_TODEVICE);
			else
				pci_unmap_page(tx_queue->efx->pci_dev,
					       unmap_addr, buffer->unmap_len,
					       PCI_DMA_TODEVICE);
			buffer->unmap_len = 0;
		}
		buffer->len = 0;
		buffer->continuation = true;
	}
}


static void tso_start(struct tso_state *st, const struct sk_buff *skb)
{
	st->header_len = ((tcp_hdr(skb)->doff << 2u)
			  + PTR_DIFF(tcp_hdr(skb), skb->data));
	st->full_packet_size = st->header_len + skb_shinfo(skb)->gso_size;

	if (st->protocol == htons(ETH_P_IP))
		st->ipv4_id = ntohs(ip_hdr(skb)->id);
	else
		st->ipv4_id = 0;
	st->seqnum = ntohl(tcp_hdr(skb)->seq);

	EFX_BUG_ON_PARANOID(tcp_hdr(skb)->urg);
	EFX_BUG_ON_PARANOID(tcp_hdr(skb)->syn);
	EFX_BUG_ON_PARANOID(tcp_hdr(skb)->rst);

	st->packet_space = st->full_packet_size;
	st->out_len = skb->len - st->header_len;
	st->unmap_len = 0;
	st->unmap_single = false;
}

static int tso_get_fragment(struct tso_state *st, struct efx_nic *efx,
			    skb_frag_t *frag)
{
	st->unmap_addr = skb_frag_dma_map(&efx->pci_dev->dev, frag, 0,
					  skb_frag_size(frag), DMA_TO_DEVICE);
	if (likely(!dma_mapping_error(&efx->pci_dev->dev, st->unmap_addr))) {
		st->unmap_single = false;
		st->unmap_len = skb_frag_size(frag);
		st->in_len = skb_frag_size(frag);
		st->dma_addr = st->unmap_addr;
		return 0;
	}
	return -ENOMEM;
}

static int tso_get_head_fragment(struct tso_state *st, struct efx_nic *efx,
				 const struct sk_buff *skb)
{
	int hl = st->header_len;
	int len = skb_headlen(skb) - hl;

	st->unmap_addr = pci_map_single(efx->pci_dev, skb->data + hl,
					len, PCI_DMA_TODEVICE);
	if (likely(!pci_dma_mapping_error(efx->pci_dev, st->unmap_addr))) {
		st->unmap_single = true;
		st->unmap_len = len;
		st->in_len = len;
		st->dma_addr = st->unmap_addr;
		return 0;
	}
	return -ENOMEM;
}


static int tso_fill_packet_with_fragment(struct efx_tx_queue *tx_queue,
					 const struct sk_buff *skb,
					 struct tso_state *st)
{
	struct efx_tx_buffer *buffer;
	int n, end_of_packet, rc;

	if (st->in_len == 0)
		return 0;
	if (st->packet_space == 0)
		return 0;

	EFX_BUG_ON_PARANOID(st->in_len <= 0);
	EFX_BUG_ON_PARANOID(st->packet_space <= 0);

	n = min(st->in_len, st->packet_space);

	st->packet_space -= n;
	st->out_len -= n;
	st->in_len -= n;

	rc = efx_tx_queue_insert(tx_queue, st->dma_addr, n, &buffer);
	if (likely(rc == 0)) {
		if (st->out_len == 0)
			
			buffer->skb = skb;

		end_of_packet = st->out_len == 0 || st->packet_space == 0;
		buffer->continuation = !end_of_packet;

		if (st->in_len == 0) {
			
			buffer->unmap_len = st->unmap_len;
			buffer->unmap_single = st->unmap_single;
			st->unmap_len = 0;
		}
	}

	st->dma_addr += n;
	return rc;
}


static int tso_start_new_packet(struct efx_tx_queue *tx_queue,
				const struct sk_buff *skb,
				struct tso_state *st)
{
	struct efx_tso_header *tsoh;
	struct tcphdr *tsoh_th;
	unsigned ip_length;
	u8 *header;

	
	if (likely(TSOH_SIZE(st->header_len) <= TSOH_STD_SIZE)) {
		if (tx_queue->tso_headers_free == NULL) {
			if (efx_tsoh_block_alloc(tx_queue))
				return -1;
		}
		EFX_BUG_ON_PARANOID(!tx_queue->tso_headers_free);
		tsoh = tx_queue->tso_headers_free;
		tx_queue->tso_headers_free = tsoh->next;
		tsoh->unmap_len = 0;
	} else {
		tx_queue->tso_long_headers++;
		tsoh = efx_tsoh_heap_alloc(tx_queue, st->header_len);
		if (unlikely(!tsoh))
			return -1;
	}

	header = TSOH_BUFFER(tsoh);
	tsoh_th = (struct tcphdr *)(header + SKB_TCP_OFF(skb));

	
	memcpy(header, skb->data, st->header_len);

	tsoh_th->seq = htonl(st->seqnum);
	st->seqnum += skb_shinfo(skb)->gso_size;
	if (st->out_len > skb_shinfo(skb)->gso_size) {
		
		ip_length = st->full_packet_size - ETH_HDR_LEN(skb);
		tsoh_th->fin = 0;
		tsoh_th->psh = 0;
	} else {
		
		ip_length = st->header_len - ETH_HDR_LEN(skb) + st->out_len;
		tsoh_th->fin = tcp_hdr(skb)->fin;
		tsoh_th->psh = tcp_hdr(skb)->psh;
	}

	if (st->protocol == htons(ETH_P_IP)) {
		struct iphdr *tsoh_iph =
			(struct iphdr *)(header + SKB_IPV4_OFF(skb));

		tsoh_iph->tot_len = htons(ip_length);

		
		tsoh_iph->id = htons(st->ipv4_id);
		st->ipv4_id++;
	} else {
		struct ipv6hdr *tsoh_iph =
			(struct ipv6hdr *)(header + SKB_IPV6_OFF(skb));

		tsoh_iph->payload_len = htons(ip_length - sizeof(*tsoh_iph));
	}

	st->packet_space = skb_shinfo(skb)->gso_size;
	++tx_queue->tso_packets;

	
	efx_tso_put_header(tx_queue, tsoh, st->header_len);

	return 0;
}


static int efx_enqueue_skb_tso(struct efx_tx_queue *tx_queue,
			       struct sk_buff *skb)
{
	struct efx_nic *efx = tx_queue->efx;
	int frag_i, rc, rc2 = NETDEV_TX_OK;
	struct tso_state state;

	
	state.protocol = efx_tso_check_protocol(skb);

	EFX_BUG_ON_PARANOID(tx_queue->write_count != tx_queue->insert_count);

	tso_start(&state, skb);

	if (skb_headlen(skb) == state.header_len) {
		
		EFX_BUG_ON_PARANOID(skb_shinfo(skb)->nr_frags < 1);
		frag_i = 0;
		rc = tso_get_fragment(&state, efx,
				      skb_shinfo(skb)->frags + frag_i);
		if (rc)
			goto mem_err;
	} else {
		rc = tso_get_head_fragment(&state, efx, skb);
		if (rc)
			goto mem_err;
		frag_i = -1;
	}

	if (tso_start_new_packet(tx_queue, skb, &state) < 0)
		goto mem_err;

	while (1) {
		rc = tso_fill_packet_with_fragment(tx_queue, skb, &state);
		if (unlikely(rc)) {
			rc2 = NETDEV_TX_BUSY;
			goto unwind;
		}

		
		if (state.in_len == 0) {
			if (++frag_i >= skb_shinfo(skb)->nr_frags)
				
				break;
			rc = tso_get_fragment(&state, efx,
					      skb_shinfo(skb)->frags + frag_i);
			if (rc)
				goto mem_err;
		}

		
		if (state.packet_space == 0 &&
		    tso_start_new_packet(tx_queue, skb, &state) < 0)
			goto mem_err;
	}

	netdev_tx_sent_queue(tx_queue->core_txq, skb->len);

	
	efx_nic_push_buffers(tx_queue);

	tx_queue->tso_bursts++;
	return NETDEV_TX_OK;

 mem_err:
	netif_err(efx, tx_err, efx->net_dev,
		  "Out of memory for TSO headers, or PCI mapping error\n");
	dev_kfree_skb_any(skb);

 unwind:
	
	if (state.unmap_len) {
		if (state.unmap_single)
			pci_unmap_single(efx->pci_dev, state.unmap_addr,
					 state.unmap_len, PCI_DMA_TODEVICE);
		else
			pci_unmap_page(efx->pci_dev, state.unmap_addr,
				       state.unmap_len, PCI_DMA_TODEVICE);
	}

	efx_enqueue_unwind(tx_queue);
	return rc2;
}


static void efx_fini_tso(struct efx_tx_queue *tx_queue)
{
	unsigned i;

	if (tx_queue->buffer) {
		for (i = 0; i <= tx_queue->ptr_mask; ++i)
			efx_tsoh_free(tx_queue, &tx_queue->buffer[i]);
	}

	while (tx_queue->tso_headers_free != NULL)
		efx_tsoh_block_free(tx_queue, tx_queue->tso_headers_free,
				    tx_queue->efx->pci_dev);
}
