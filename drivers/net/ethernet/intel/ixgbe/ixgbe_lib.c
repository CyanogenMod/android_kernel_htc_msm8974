/*******************************************************************************

  Intel 10 Gigabit PCI Express Linux driver
  Copyright(c) 1999 - 2012 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#include "ixgbe.h"
#include "ixgbe_sriov.h"

static inline bool ixgbe_cache_ring_rss(struct ixgbe_adapter *adapter)
{
	int i;

	if (!(adapter->flags & IXGBE_FLAG_RSS_ENABLED))
		return false;

	for (i = 0; i < adapter->num_rx_queues; i++)
		adapter->rx_ring[i]->reg_idx = i;
	for (i = 0; i < adapter->num_tx_queues; i++)
		adapter->tx_ring[i]->reg_idx = i;

	return true;
}
#ifdef CONFIG_IXGBE_DCB

static void ixgbe_get_first_reg_idx(struct ixgbe_adapter *adapter, u8 tc,
				    unsigned int *tx, unsigned int *rx)
{
	struct net_device *dev = adapter->netdev;
	struct ixgbe_hw *hw = &adapter->hw;
	u8 num_tcs = netdev_get_num_tc(dev);

	*tx = 0;
	*rx = 0;

	switch (hw->mac.type) {
	case ixgbe_mac_82598EB:
		*tx = tc << 2;
		*rx = tc << 3;
		break;
	case ixgbe_mac_82599EB:
	case ixgbe_mac_X540:
		if (num_tcs > 4) {
			if (tc < 3) {
				*tx = tc << 5;
				*rx = tc << 4;
			} else if (tc <  5) {
				*tx = ((tc + 2) << 4);
				*rx = tc << 4;
			} else if (tc < num_tcs) {
				*tx = ((tc + 8) << 3);
				*rx = tc << 4;
			}
		} else {
			*rx =  tc << 5;
			switch (tc) {
			case 0:
				*tx =  0;
				break;
			case 1:
				*tx = 64;
				break;
			case 2:
				*tx = 96;
				break;
			case 3:
				*tx = 112;
				break;
			default:
				break;
			}
		}
		break;
	default:
		break;
	}
}

static inline bool ixgbe_cache_ring_dcb(struct ixgbe_adapter *adapter)
{
	struct net_device *dev = adapter->netdev;
	int i, j, k;
	u8 num_tcs = netdev_get_num_tc(dev);

	if (!num_tcs)
		return false;

	for (i = 0, k = 0; i < num_tcs; i++) {
		unsigned int tx_s, rx_s;
		u16 count = dev->tc_to_txq[i].count;

		ixgbe_get_first_reg_idx(adapter, i, &tx_s, &rx_s);
		for (j = 0; j < count; j++, k++) {
			adapter->tx_ring[k]->reg_idx = tx_s + j;
			adapter->rx_ring[k]->reg_idx = rx_s + j;
			adapter->tx_ring[k]->dcb_tc = i;
			adapter->rx_ring[k]->dcb_tc = i;
		}
	}

	return true;
}
#endif

static inline bool ixgbe_cache_ring_fdir(struct ixgbe_adapter *adapter)
{
	int i;
	bool ret = false;

	if ((adapter->flags & IXGBE_FLAG_RSS_ENABLED) &&
	    (adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE)) {
		for (i = 0; i < adapter->num_rx_queues; i++)
			adapter->rx_ring[i]->reg_idx = i;
		for (i = 0; i < adapter->num_tx_queues; i++)
			adapter->tx_ring[i]->reg_idx = i;
		ret = true;
	}

	return ret;
}

#ifdef IXGBE_FCOE
static inline bool ixgbe_cache_ring_fcoe(struct ixgbe_adapter *adapter)
{
	struct ixgbe_ring_feature *f = &adapter->ring_feature[RING_F_FCOE];
	int i;
	u8 fcoe_rx_i = 0, fcoe_tx_i = 0;

	if (!(adapter->flags & IXGBE_FLAG_FCOE_ENABLED))
		return false;

	if (adapter->flags & IXGBE_FLAG_RSS_ENABLED) {
		if (adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE)
			ixgbe_cache_ring_fdir(adapter);
		else
			ixgbe_cache_ring_rss(adapter);

		fcoe_rx_i = f->mask;
		fcoe_tx_i = f->mask;
	}
	for (i = 0; i < f->indices; i++, fcoe_rx_i++, fcoe_tx_i++) {
		adapter->rx_ring[f->mask + i]->reg_idx = fcoe_rx_i;
		adapter->tx_ring[f->mask + i]->reg_idx = fcoe_tx_i;
	}
	return true;
}

#endif 
static inline bool ixgbe_cache_ring_sriov(struct ixgbe_adapter *adapter)
{
	adapter->rx_ring[0]->reg_idx = adapter->num_vfs * 2;
	adapter->tx_ring[0]->reg_idx = adapter->num_vfs * 2;
	if (adapter->num_vfs)
		return true;
	else
		return false;
}

static void ixgbe_cache_ring_register(struct ixgbe_adapter *adapter)
{
	
	adapter->rx_ring[0]->reg_idx = 0;
	adapter->tx_ring[0]->reg_idx = 0;

	if (ixgbe_cache_ring_sriov(adapter))
		return;

#ifdef CONFIG_IXGBE_DCB
	if (ixgbe_cache_ring_dcb(adapter))
		return;
#endif

#ifdef IXGBE_FCOE
	if (ixgbe_cache_ring_fcoe(adapter))
		return;
#endif 

	if (ixgbe_cache_ring_fdir(adapter))
		return;

	if (ixgbe_cache_ring_rss(adapter))
		return;
}

static inline bool ixgbe_set_sriov_queues(struct ixgbe_adapter *adapter)
{
	return false;
}

static inline bool ixgbe_set_rss_queues(struct ixgbe_adapter *adapter)
{
	bool ret = false;
	struct ixgbe_ring_feature *f = &adapter->ring_feature[RING_F_RSS];

	if (adapter->flags & IXGBE_FLAG_RSS_ENABLED) {
		f->mask = 0xF;
		adapter->num_rx_queues = f->indices;
		adapter->num_tx_queues = f->indices;
		ret = true;
	}

	return ret;
}

static inline bool ixgbe_set_fdir_queues(struct ixgbe_adapter *adapter)
{
	bool ret = false;
	struct ixgbe_ring_feature *f_fdir = &adapter->ring_feature[RING_F_FDIR];

	f_fdir->indices = min_t(int, num_online_cpus(), f_fdir->indices);
	f_fdir->mask = 0;

	if ((adapter->flags & IXGBE_FLAG_RSS_ENABLED) &&
	    (adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE)) {
		adapter->num_tx_queues = f_fdir->indices;
		adapter->num_rx_queues = f_fdir->indices;
		ret = true;
	} else {
		adapter->flags &= ~IXGBE_FLAG_FDIR_HASH_CAPABLE;
	}
	return ret;
}

#ifdef IXGBE_FCOE
static inline bool ixgbe_set_fcoe_queues(struct ixgbe_adapter *adapter)
{
	struct ixgbe_ring_feature *f = &adapter->ring_feature[RING_F_FCOE];

	if (!(adapter->flags & IXGBE_FLAG_FCOE_ENABLED))
		return false;

	f->indices = min_t(int, num_online_cpus(), f->indices);

	adapter->num_rx_queues = 1;
	adapter->num_tx_queues = 1;

	if (adapter->flags & IXGBE_FLAG_RSS_ENABLED) {
		e_info(probe, "FCoE enabled with RSS\n");
		if (adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE)
			ixgbe_set_fdir_queues(adapter);
		else
			ixgbe_set_rss_queues(adapter);
	}

	
	f->mask = adapter->num_rx_queues;
	adapter->num_rx_queues += f->indices;
	adapter->num_tx_queues += f->indices;

	return true;
}
#endif 

#define DCB_QUEUE_CAP 8

#ifdef CONFIG_IXGBE_DCB
static inline bool ixgbe_set_dcb_queues(struct ixgbe_adapter *adapter)
{
	int per_tc_q, q, i, offset = 0;
	struct net_device *dev = adapter->netdev;
	int tcs = netdev_get_num_tc(dev);

	if (!tcs)
		return false;

	
	per_tc_q = min_t(unsigned int, dev->num_tx_queues / tcs, DCB_QUEUE_CAP);
	q = min_t(int, num_online_cpus(), per_tc_q);

	for (i = 0; i < tcs; i++) {
		netdev_set_tc_queue(dev, i, q, offset);
		offset += q;
	}

	adapter->num_tx_queues = q * tcs;
	adapter->num_rx_queues = q * tcs;

#ifdef IXGBE_FCOE
	if (adapter->flags & IXGBE_FLAG_FCOE_ENABLED) {
		u8 prio_tc[MAX_USER_PRIORITY] = {0};
		int tc;
		struct ixgbe_ring_feature *f =
					&adapter->ring_feature[RING_F_FCOE];

		ixgbe_dcb_unpack_map(&adapter->dcb_cfg, DCB_TX_CONFIG, prio_tc);
		tc = prio_tc[adapter->fcoe.up];
		f->indices = dev->tc_to_txq[tc].count;
		f->mask = dev->tc_to_txq[tc].offset;
	}
#endif

	return true;
}
#endif

static int ixgbe_set_num_queues(struct ixgbe_adapter *adapter)
{
	
	adapter->num_rx_queues = 1;
	adapter->num_tx_queues = 1;
	adapter->num_rx_pools = adapter->num_rx_queues;
	adapter->num_rx_queues_per_pool = 1;

	if (ixgbe_set_sriov_queues(adapter))
		goto done;

#ifdef CONFIG_IXGBE_DCB
	if (ixgbe_set_dcb_queues(adapter))
		goto done;

#endif
#ifdef IXGBE_FCOE
	if (ixgbe_set_fcoe_queues(adapter))
		goto done;

#endif 
	if (ixgbe_set_fdir_queues(adapter))
		goto done;

	if (ixgbe_set_rss_queues(adapter))
		goto done;

	
	adapter->num_rx_queues = 1;
	adapter->num_tx_queues = 1;

done:
	if ((adapter->netdev->reg_state == NETREG_UNREGISTERED) ||
	    (adapter->netdev->reg_state == NETREG_UNREGISTERING))
		return 0;

	
	netif_set_real_num_tx_queues(adapter->netdev, adapter->num_tx_queues);
	return netif_set_real_num_rx_queues(adapter->netdev,
					    adapter->num_rx_queues);
}

static void ixgbe_acquire_msix_vectors(struct ixgbe_adapter *adapter,
				       int vectors)
{
	int err, vector_threshold;

	vector_threshold = MIN_MSIX_COUNT;

	while (vectors >= vector_threshold) {
		err = pci_enable_msix(adapter->pdev, adapter->msix_entries,
				      vectors);
		if (!err) 
			break;
		else if (err < 0)
			vectors = 0; 
		else 
			vectors = err;
	}

	if (vectors < vector_threshold) {
		netif_printk(adapter, hw, KERN_DEBUG, adapter->netdev,
			     "Unable to allocate MSI-X interrupts\n");
		adapter->flags &= ~IXGBE_FLAG_MSIX_ENABLED;
		kfree(adapter->msix_entries);
		adapter->msix_entries = NULL;
	} else {
		adapter->flags |= IXGBE_FLAG_MSIX_ENABLED; 
		adapter->num_msix_vectors = min(vectors,
				   adapter->max_msix_q_vectors + NON_Q_VECTORS);
	}
}

static void ixgbe_add_ring(struct ixgbe_ring *ring,
			   struct ixgbe_ring_container *head)
{
	ring->next = head->ring;
	head->ring = ring;
	head->count++;
}

static int ixgbe_alloc_q_vector(struct ixgbe_adapter *adapter, int v_idx,
				int txr_count, int txr_idx,
				int rxr_count, int rxr_idx)
{
	struct ixgbe_q_vector *q_vector;
	struct ixgbe_ring *ring;
	int node = -1;
	int cpu = -1;
	int ring_count, size;

	ring_count = txr_count + rxr_count;
	size = sizeof(struct ixgbe_q_vector) +
	       (sizeof(struct ixgbe_ring) * ring_count);

	
	if (adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE) {
		if (cpu_online(v_idx)) {
			cpu = v_idx;
			node = cpu_to_node(cpu);
		}
	}

	
	q_vector = kzalloc_node(size, GFP_KERNEL, node);
	if (!q_vector)
		q_vector = kzalloc(size, GFP_KERNEL);
	if (!q_vector)
		return -ENOMEM;

	
	if (cpu != -1)
		cpumask_set_cpu(cpu, &q_vector->affinity_mask);
	else
		cpumask_copy(&q_vector->affinity_mask, cpu_online_mask);
	q_vector->numa_node = node;

	
	netif_napi_add(adapter->netdev, &q_vector->napi,
		       ixgbe_poll, 64);

	
	adapter->q_vector[v_idx] = q_vector;
	q_vector->adapter = adapter;
	q_vector->v_idx = v_idx;

	
	q_vector->tx.work_limit = adapter->tx_work_limit;

	
	ring = q_vector->ring;

	while (txr_count) {
		
		ring->dev = &adapter->pdev->dev;
		ring->netdev = adapter->netdev;

		
		ring->q_vector = q_vector;

		
		ixgbe_add_ring(ring, &q_vector->tx);

		
		ring->count = adapter->tx_ring_count;
		ring->queue_index = txr_idx;

		
		adapter->tx_ring[txr_idx] = ring;

		
		txr_count--;
		txr_idx++;

		
		ring++;
	}

	while (rxr_count) {
		
		ring->dev = &adapter->pdev->dev;
		ring->netdev = adapter->netdev;

		
		ring->q_vector = q_vector;

		
		ixgbe_add_ring(ring, &q_vector->rx);

		if (adapter->hw.mac.type == ixgbe_mac_82599EB)
			set_bit(__IXGBE_RX_CSUM_UDP_ZERO_ERR, &ring->state);

#ifdef IXGBE_FCOE
		if (adapter->netdev->features & NETIF_F_FCOE_MTU) {
			struct ixgbe_ring_feature *f;
			f = &adapter->ring_feature[RING_F_FCOE];
			if ((rxr_idx >= f->mask) &&
			    (rxr_idx < f->mask + f->indices))
				set_bit(__IXGBE_RX_FCOE_BUFSZ, &ring->state);
		}

#endif 
		
		ring->count = adapter->rx_ring_count;
		ring->queue_index = rxr_idx;

		
		adapter->rx_ring[rxr_idx] = ring;

		
		rxr_count--;
		rxr_idx++;

		
		ring++;
	}

	return 0;
}

static void ixgbe_free_q_vector(struct ixgbe_adapter *adapter, int v_idx)
{
	struct ixgbe_q_vector *q_vector = adapter->q_vector[v_idx];
	struct ixgbe_ring *ring;

	ixgbe_for_each_ring(ring, q_vector->tx)
		adapter->tx_ring[ring->queue_index] = NULL;

	ixgbe_for_each_ring(ring, q_vector->rx)
		adapter->rx_ring[ring->queue_index] = NULL;

	adapter->q_vector[v_idx] = NULL;
	netif_napi_del(&q_vector->napi);

	kfree_rcu(q_vector, rcu);
}

static int ixgbe_alloc_q_vectors(struct ixgbe_adapter *adapter)
{
	int q_vectors = adapter->num_msix_vectors - NON_Q_VECTORS;
	int rxr_remaining = adapter->num_rx_queues;
	int txr_remaining = adapter->num_tx_queues;
	int rxr_idx = 0, txr_idx = 0, v_idx = 0;
	int err;

	
	if (!(adapter->flags & IXGBE_FLAG_MSIX_ENABLED))
		q_vectors = 1;

	if (q_vectors >= (rxr_remaining + txr_remaining)) {
		for (; rxr_remaining; v_idx++, q_vectors--) {
			int rqpv = DIV_ROUND_UP(rxr_remaining, q_vectors);
			err = ixgbe_alloc_q_vector(adapter, v_idx,
						   0, 0, rqpv, rxr_idx);

			if (err)
				goto err_out;

			
			rxr_remaining -= rqpv;
			rxr_idx += rqpv;
		}
	}

	for (; q_vectors; v_idx++, q_vectors--) {
		int rqpv = DIV_ROUND_UP(rxr_remaining, q_vectors);
		int tqpv = DIV_ROUND_UP(txr_remaining, q_vectors);
		err = ixgbe_alloc_q_vector(adapter, v_idx,
					   tqpv, txr_idx,
					   rqpv, rxr_idx);

		if (err)
			goto err_out;

		
		rxr_remaining -= rqpv;
		rxr_idx += rqpv;
		txr_remaining -= tqpv;
		txr_idx += tqpv;
	}

	return 0;

err_out:
	while (v_idx) {
		v_idx--;
		ixgbe_free_q_vector(adapter, v_idx);
	}

	return -ENOMEM;
}

static void ixgbe_free_q_vectors(struct ixgbe_adapter *adapter)
{
	int v_idx, q_vectors;

	if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED)
		q_vectors = adapter->num_msix_vectors - NON_Q_VECTORS;
	else
		q_vectors = 1;

	for (v_idx = 0; v_idx < q_vectors; v_idx++)
		ixgbe_free_q_vector(adapter, v_idx);
}

static void ixgbe_reset_interrupt_capability(struct ixgbe_adapter *adapter)
{
	if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED) {
		adapter->flags &= ~IXGBE_FLAG_MSIX_ENABLED;
		pci_disable_msix(adapter->pdev);
		kfree(adapter->msix_entries);
		adapter->msix_entries = NULL;
	} else if (adapter->flags & IXGBE_FLAG_MSI_ENABLED) {
		adapter->flags &= ~IXGBE_FLAG_MSI_ENABLED;
		pci_disable_msi(adapter->pdev);
	}
}

static int ixgbe_set_interrupt_capability(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	int err = 0;
	int vector, v_budget;

	v_budget = max(adapter->num_rx_queues, adapter->num_tx_queues);
	v_budget = min_t(int, v_budget, num_online_cpus());
	v_budget += NON_Q_VECTORS;

	v_budget = min_t(int, v_budget, hw->mac.max_msix_vectors);

	adapter->msix_entries = kcalloc(v_budget,
					sizeof(struct msix_entry), GFP_KERNEL);
	if (adapter->msix_entries) {
		for (vector = 0; vector < v_budget; vector++)
			adapter->msix_entries[vector].entry = vector;

		ixgbe_acquire_msix_vectors(adapter, v_budget);

		if (adapter->flags & IXGBE_FLAG_MSIX_ENABLED)
			goto out;
	}

	adapter->flags &= ~IXGBE_FLAG_DCB_ENABLED;
	adapter->flags &= ~IXGBE_FLAG_RSS_ENABLED;
	if (adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE) {
		e_err(probe,
		      "ATR is not supported while multiple "
		      "queues are disabled.  Disabling Flow Director\n");
	}
	adapter->flags &= ~IXGBE_FLAG_FDIR_HASH_CAPABLE;
	adapter->atr_sample_rate = 0;
	if (adapter->flags & IXGBE_FLAG_SRIOV_ENABLED)
		ixgbe_disable_sriov(adapter);

	err = ixgbe_set_num_queues(adapter);
	if (err)
		return err;

	err = pci_enable_msi(adapter->pdev);
	if (!err) {
		adapter->flags |= IXGBE_FLAG_MSI_ENABLED;
	} else {
		netif_printk(adapter, hw, KERN_DEBUG, adapter->netdev,
			     "Unable to allocate MSI interrupt, "
			     "falling back to legacy.  Error: %d\n", err);
		
		err = 0;
	}

out:
	return err;
}

int ixgbe_init_interrupt_scheme(struct ixgbe_adapter *adapter)
{
	int err;

	
	err = ixgbe_set_num_queues(adapter);
	if (err)
		return err;

	err = ixgbe_set_interrupt_capability(adapter);
	if (err) {
		e_dev_err("Unable to setup interrupt capabilities\n");
		goto err_set_interrupt;
	}

	err = ixgbe_alloc_q_vectors(adapter);
	if (err) {
		e_dev_err("Unable to allocate memory for queue vectors\n");
		goto err_alloc_q_vectors;
	}

	ixgbe_cache_ring_register(adapter);

	e_dev_info("Multiqueue %s: Rx Queue count = %u, Tx Queue count = %u\n",
		   (adapter->num_rx_queues > 1) ? "Enabled" : "Disabled",
		   adapter->num_rx_queues, adapter->num_tx_queues);

	set_bit(__IXGBE_DOWN, &adapter->state);

	return 0;

err_alloc_q_vectors:
	ixgbe_reset_interrupt_capability(adapter);
err_set_interrupt:
	return err;
}

void ixgbe_clear_interrupt_scheme(struct ixgbe_adapter *adapter)
{
	adapter->num_tx_queues = 0;
	adapter->num_rx_queues = 0;

	ixgbe_free_q_vectors(adapter);
	ixgbe_reset_interrupt_capability(adapter);
}

void ixgbe_tx_ctxtdesc(struct ixgbe_ring *tx_ring, u32 vlan_macip_lens,
		       u32 fcoe_sof_eof, u32 type_tucmd, u32 mss_l4len_idx)
{
	struct ixgbe_adv_tx_context_desc *context_desc;
	u16 i = tx_ring->next_to_use;

	context_desc = IXGBE_TX_CTXTDESC(tx_ring, i);

	i++;
	tx_ring->next_to_use = (i < tx_ring->count) ? i : 0;

	
	type_tucmd |= IXGBE_TXD_CMD_DEXT | IXGBE_ADVTXD_DTYP_CTXT;

	context_desc->vlan_macip_lens	= cpu_to_le32(vlan_macip_lens);
	context_desc->seqnum_seed	= cpu_to_le32(fcoe_sof_eof);
	context_desc->type_tucmd_mlhl	= cpu_to_le32(type_tucmd);
	context_desc->mss_l4len_idx	= cpu_to_le32(mss_l4len_idx);
}

