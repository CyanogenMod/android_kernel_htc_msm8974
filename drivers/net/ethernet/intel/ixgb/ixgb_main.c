/*******************************************************************************

  Intel PRO/10GbE Linux driver
  Copyright(c) 1999 - 2008 Intel Corporation.

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
  Linux NICS <linux.nics@intel.com>
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/prefetch.h>
#include "ixgb.h"

char ixgb_driver_name[] = "ixgb";
static char ixgb_driver_string[] = "Intel(R) PRO/10GbE Network Driver";

#define DRIVERNAPI "-NAPI"
#define DRV_VERSION "1.0.135-k2" DRIVERNAPI
const char ixgb_driver_version[] = DRV_VERSION;
static const char ixgb_copyright[] = "Copyright (c) 1999-2008 Intel Corporation.";

#define IXGB_CB_LENGTH 256
static unsigned int copybreak __read_mostly = IXGB_CB_LENGTH;
module_param(copybreak, uint, 0644);
MODULE_PARM_DESC(copybreak,
	"Maximum size of packet that is copied to a new buffer on receive");

static DEFINE_PCI_DEVICE_TABLE(ixgb_pci_tbl) = {
	{INTEL_VENDOR_ID, IXGB_DEVICE_ID_82597EX,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{INTEL_VENDOR_ID, IXGB_DEVICE_ID_82597EX_CX4,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{INTEL_VENDOR_ID, IXGB_DEVICE_ID_82597EX_SR,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{INTEL_VENDOR_ID, IXGB_DEVICE_ID_82597EX_LR,
	 PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},

	
	{0,}
};

MODULE_DEVICE_TABLE(pci, ixgb_pci_tbl);

static int ixgb_init_module(void);
static void ixgb_exit_module(void);
static int ixgb_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static void __devexit ixgb_remove(struct pci_dev *pdev);
static int ixgb_sw_init(struct ixgb_adapter *adapter);
static int ixgb_open(struct net_device *netdev);
static int ixgb_close(struct net_device *netdev);
static void ixgb_configure_tx(struct ixgb_adapter *adapter);
static void ixgb_configure_rx(struct ixgb_adapter *adapter);
static void ixgb_setup_rctl(struct ixgb_adapter *adapter);
static void ixgb_clean_tx_ring(struct ixgb_adapter *adapter);
static void ixgb_clean_rx_ring(struct ixgb_adapter *adapter);
static void ixgb_set_multi(struct net_device *netdev);
static void ixgb_watchdog(unsigned long data);
static netdev_tx_t ixgb_xmit_frame(struct sk_buff *skb,
				   struct net_device *netdev);
static struct net_device_stats *ixgb_get_stats(struct net_device *netdev);
static int ixgb_change_mtu(struct net_device *netdev, int new_mtu);
static int ixgb_set_mac(struct net_device *netdev, void *p);
static irqreturn_t ixgb_intr(int irq, void *data);
static bool ixgb_clean_tx_irq(struct ixgb_adapter *adapter);

static int ixgb_clean(struct napi_struct *, int);
static bool ixgb_clean_rx_irq(struct ixgb_adapter *, int *, int);
static void ixgb_alloc_rx_buffers(struct ixgb_adapter *, int);

static void ixgb_tx_timeout(struct net_device *dev);
static void ixgb_tx_timeout_task(struct work_struct *work);

static void ixgb_vlan_strip_enable(struct ixgb_adapter *adapter);
static void ixgb_vlan_strip_disable(struct ixgb_adapter *adapter);
static int ixgb_vlan_rx_add_vid(struct net_device *netdev, u16 vid);
static int ixgb_vlan_rx_kill_vid(struct net_device *netdev, u16 vid);
static void ixgb_restore_vlan(struct ixgb_adapter *adapter);

#ifdef CONFIG_NET_POLL_CONTROLLER
static void ixgb_netpoll(struct net_device *dev);
#endif

static pci_ers_result_t ixgb_io_error_detected (struct pci_dev *pdev,
                             enum pci_channel_state state);
static pci_ers_result_t ixgb_io_slot_reset (struct pci_dev *pdev);
static void ixgb_io_resume (struct pci_dev *pdev);

static struct pci_error_handlers ixgb_err_handler = {
	.error_detected = ixgb_io_error_detected,
	.slot_reset = ixgb_io_slot_reset,
	.resume = ixgb_io_resume,
};

static struct pci_driver ixgb_driver = {
	.name     = ixgb_driver_name,
	.id_table = ixgb_pci_tbl,
	.probe    = ixgb_probe,
	.remove   = __devexit_p(ixgb_remove),
	.err_handler = &ixgb_err_handler
};

MODULE_AUTHOR("Intel Corporation, <linux.nics@intel.com>");
MODULE_DESCRIPTION("Intel(R) PRO/10GbE Network Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

#define DEFAULT_MSG_ENABLE (NETIF_MSG_DRV|NETIF_MSG_PROBE|NETIF_MSG_LINK)
static int debug = -1;
module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "Debug level (0=none,...,16=all)");


static int __init
ixgb_init_module(void)
{
	pr_info("%s - version %s\n", ixgb_driver_string, ixgb_driver_version);
	pr_info("%s\n", ixgb_copyright);

	return pci_register_driver(&ixgb_driver);
}

module_init(ixgb_init_module);


static void __exit
ixgb_exit_module(void)
{
	pci_unregister_driver(&ixgb_driver);
}

module_exit(ixgb_exit_module);


static void
ixgb_irq_disable(struct ixgb_adapter *adapter)
{
	IXGB_WRITE_REG(&adapter->hw, IMC, ~0);
	IXGB_WRITE_FLUSH(&adapter->hw);
	synchronize_irq(adapter->pdev->irq);
}


static void
ixgb_irq_enable(struct ixgb_adapter *adapter)
{
	u32 val = IXGB_INT_RXT0 | IXGB_INT_RXDMT0 |
		  IXGB_INT_TXDW | IXGB_INT_LSC;
	if (adapter->hw.subsystem_vendor_id == SUN_SUBVENDOR_ID)
		val |= IXGB_INT_GPI0;
	IXGB_WRITE_REG(&adapter->hw, IMS, val);
	IXGB_WRITE_FLUSH(&adapter->hw);
}

int
ixgb_up(struct ixgb_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	int err, irq_flags = IRQF_SHARED;
	int max_frame = netdev->mtu + ENET_HEADER_SIZE + ENET_FCS_LENGTH;
	struct ixgb_hw *hw = &adapter->hw;

	

	ixgb_rar_set(hw, netdev->dev_addr, 0);
	ixgb_set_multi(netdev);

	ixgb_restore_vlan(adapter);

	ixgb_configure_tx(adapter);
	ixgb_setup_rctl(adapter);
	ixgb_configure_rx(adapter);
	ixgb_alloc_rx_buffers(adapter, IXGB_DESC_UNUSED(&adapter->rx_ring));

	
	IXGB_WRITE_REG(&adapter->hw, IMC, 0xffffffff);

	
	if (IXGB_READ_REG(&adapter->hw, STATUS) & IXGB_STATUS_PCIX_MODE) {
		err = pci_enable_msi(adapter->pdev);
		if (!err) {
			adapter->have_msi = true;
			irq_flags = 0;
		}
		
	}

	err = request_irq(adapter->pdev->irq, ixgb_intr, irq_flags,
	                  netdev->name, netdev);
	if (err) {
		if (adapter->have_msi)
			pci_disable_msi(adapter->pdev);
		netif_err(adapter, probe, adapter->netdev,
			  "Unable to allocate interrupt Error: %d\n", err);
		return err;
	}

	if ((hw->max_frame_size != max_frame) ||
		(hw->max_frame_size !=
		(IXGB_READ_REG(hw, MFS) >> IXGB_MFS_SHIFT))) {

		hw->max_frame_size = max_frame;

		IXGB_WRITE_REG(hw, MFS, hw->max_frame_size << IXGB_MFS_SHIFT);

		if (hw->max_frame_size >
		   IXGB_MAX_ENET_FRAME_SIZE_WITHOUT_FCS + ENET_FCS_LENGTH) {
			u32 ctrl0 = IXGB_READ_REG(hw, CTRL0);

			if (!(ctrl0 & IXGB_CTRL0_JFE)) {
				ctrl0 |= IXGB_CTRL0_JFE;
				IXGB_WRITE_REG(hw, CTRL0, ctrl0);
			}
		}
	}

	clear_bit(__IXGB_DOWN, &adapter->flags);

	napi_enable(&adapter->napi);
	ixgb_irq_enable(adapter);

	netif_wake_queue(netdev);

	mod_timer(&adapter->watchdog_timer, jiffies);

	return 0;
}

void
ixgb_down(struct ixgb_adapter *adapter, bool kill_watchdog)
{
	struct net_device *netdev = adapter->netdev;

	
	set_bit(__IXGB_DOWN, &adapter->flags);

	napi_disable(&adapter->napi);
	
	ixgb_irq_disable(adapter);
	free_irq(adapter->pdev->irq, netdev);

	if (adapter->have_msi)
		pci_disable_msi(adapter->pdev);

	if (kill_watchdog)
		del_timer_sync(&adapter->watchdog_timer);

	adapter->link_speed = 0;
	adapter->link_duplex = 0;
	netif_carrier_off(netdev);
	netif_stop_queue(netdev);

	ixgb_reset(adapter);
	ixgb_clean_tx_ring(adapter);
	ixgb_clean_rx_ring(adapter);
}

void
ixgb_reset(struct ixgb_adapter *adapter)
{
	struct ixgb_hw *hw = &adapter->hw;

	ixgb_adapter_stop(hw);
	if (!ixgb_init_hw(hw))
		netif_err(adapter, probe, adapter->netdev, "ixgb_init_hw failed\n");

	
	IXGB_WRITE_REG(hw, MFS, hw->max_frame_size << IXGB_MFS_SHIFT);
	if (hw->max_frame_size >
	    IXGB_MAX_ENET_FRAME_SIZE_WITHOUT_FCS + ENET_FCS_LENGTH) {
		u32 ctrl0 = IXGB_READ_REG(hw, CTRL0);
		if (!(ctrl0 & IXGB_CTRL0_JFE)) {
			ctrl0 |= IXGB_CTRL0_JFE;
			IXGB_WRITE_REG(hw, CTRL0, ctrl0);
		}
	}
}

static netdev_features_t
ixgb_fix_features(struct net_device *netdev, netdev_features_t features)
{
	if (!(features & NETIF_F_HW_VLAN_RX))
		features &= ~NETIF_F_HW_VLAN_TX;

	return features;
}

static int
ixgb_set_features(struct net_device *netdev, netdev_features_t features)
{
	struct ixgb_adapter *adapter = netdev_priv(netdev);
	netdev_features_t changed = features ^ netdev->features;

	if (!(changed & (NETIF_F_RXCSUM|NETIF_F_HW_VLAN_RX)))
		return 0;

	adapter->rx_csum = !!(features & NETIF_F_RXCSUM);

	if (netif_running(netdev)) {
		ixgb_down(adapter, true);
		ixgb_up(adapter);
		ixgb_set_speed_duplex(netdev);
	} else
		ixgb_reset(adapter);

	return 0;
}


static const struct net_device_ops ixgb_netdev_ops = {
	.ndo_open 		= ixgb_open,
	.ndo_stop		= ixgb_close,
	.ndo_start_xmit		= ixgb_xmit_frame,
	.ndo_get_stats		= ixgb_get_stats,
	.ndo_set_rx_mode	= ixgb_set_multi,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address	= ixgb_set_mac,
	.ndo_change_mtu		= ixgb_change_mtu,
	.ndo_tx_timeout		= ixgb_tx_timeout,
	.ndo_vlan_rx_add_vid	= ixgb_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid	= ixgb_vlan_rx_kill_vid,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller	= ixgb_netpoll,
#endif
	.ndo_fix_features       = ixgb_fix_features,
	.ndo_set_features       = ixgb_set_features,
};


static int __devinit
ixgb_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct net_device *netdev = NULL;
	struct ixgb_adapter *adapter;
	static int cards_found = 0;
	int pci_using_dac;
	int i;
	int err;

	err = pci_enable_device(pdev);
	if (err)
		return err;

	pci_using_dac = 0;
	err = dma_set_mask(&pdev->dev, DMA_BIT_MASK(64));
	if (!err) {
		err = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(64));
		if (!err)
			pci_using_dac = 1;
	} else {
		err = dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));
		if (err) {
			err = dma_set_coherent_mask(&pdev->dev,
						    DMA_BIT_MASK(32));
			if (err) {
				pr_err("No usable DMA configuration, aborting\n");
				goto err_dma_mask;
			}
		}
	}

	err = pci_request_regions(pdev, ixgb_driver_name);
	if (err)
		goto err_request_regions;

	pci_set_master(pdev);

	netdev = alloc_etherdev(sizeof(struct ixgb_adapter));
	if (!netdev) {
		err = -ENOMEM;
		goto err_alloc_etherdev;
	}

	SET_NETDEV_DEV(netdev, &pdev->dev);

	pci_set_drvdata(pdev, netdev);
	adapter = netdev_priv(netdev);
	adapter->netdev = netdev;
	adapter->pdev = pdev;
	adapter->hw.back = adapter;
	adapter->msg_enable = netif_msg_init(debug, DEFAULT_MSG_ENABLE);

	adapter->hw.hw_addr = pci_ioremap_bar(pdev, BAR_0);
	if (!adapter->hw.hw_addr) {
		err = -EIO;
		goto err_ioremap;
	}

	for (i = BAR_1; i <= BAR_5; i++) {
		if (pci_resource_len(pdev, i) == 0)
			continue;
		if (pci_resource_flags(pdev, i) & IORESOURCE_IO) {
			adapter->hw.io_base = pci_resource_start(pdev, i);
			break;
		}
	}

	netdev->netdev_ops = &ixgb_netdev_ops;
	ixgb_set_ethtool_ops(netdev);
	netdev->watchdog_timeo = 5 * HZ;
	netif_napi_add(netdev, &adapter->napi, ixgb_clean, 64);

	strncpy(netdev->name, pci_name(pdev), sizeof(netdev->name) - 1);

	adapter->bd_number = cards_found;
	adapter->link_speed = 0;
	adapter->link_duplex = 0;

	

	err = ixgb_sw_init(adapter);
	if (err)
		goto err_sw_init;

	netdev->hw_features = NETIF_F_SG |
			   NETIF_F_TSO |
			   NETIF_F_HW_CSUM |
			   NETIF_F_HW_VLAN_TX |
			   NETIF_F_HW_VLAN_RX;
	netdev->features = netdev->hw_features |
			   NETIF_F_HW_VLAN_FILTER;
	netdev->hw_features |= NETIF_F_RXCSUM;

	if (pci_using_dac) {
		netdev->features |= NETIF_F_HIGHDMA;
		netdev->vlan_features |= NETIF_F_HIGHDMA;
	}

	

	if (!ixgb_validate_eeprom_checksum(&adapter->hw)) {
		netif_err(adapter, probe, adapter->netdev,
			  "The EEPROM Checksum Is Not Valid\n");
		err = -EIO;
		goto err_eeprom;
	}

	ixgb_get_ee_mac_addr(&adapter->hw, netdev->dev_addr);
	memcpy(netdev->perm_addr, netdev->dev_addr, netdev->addr_len);

	if (!is_valid_ether_addr(netdev->perm_addr)) {
		netif_err(adapter, probe, adapter->netdev, "Invalid MAC Address\n");
		err = -EIO;
		goto err_eeprom;
	}

	adapter->part_num = ixgb_get_ee_pba_number(&adapter->hw);

	init_timer(&adapter->watchdog_timer);
	adapter->watchdog_timer.function = ixgb_watchdog;
	adapter->watchdog_timer.data = (unsigned long)adapter;

	INIT_WORK(&adapter->tx_timeout_task, ixgb_tx_timeout_task);

	strcpy(netdev->name, "eth%d");
	err = register_netdev(netdev);
	if (err)
		goto err_register;

	
	netif_carrier_off(netdev);

	netif_info(adapter, probe, adapter->netdev,
		   "Intel(R) PRO/10GbE Network Connection\n");
	ixgb_check_options(adapter);
	

	ixgb_reset(adapter);

	cards_found++;
	return 0;

err_register:
err_sw_init:
err_eeprom:
	iounmap(adapter->hw.hw_addr);
err_ioremap:
	free_netdev(netdev);
err_alloc_etherdev:
	pci_release_regions(pdev);
err_request_regions:
err_dma_mask:
	pci_disable_device(pdev);
	return err;
}


static void __devexit
ixgb_remove(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct ixgb_adapter *adapter = netdev_priv(netdev);

	cancel_work_sync(&adapter->tx_timeout_task);

	unregister_netdev(netdev);

	iounmap(adapter->hw.hw_addr);
	pci_release_regions(pdev);

	free_netdev(netdev);
	pci_disable_device(pdev);
}


static int __devinit
ixgb_sw_init(struct ixgb_adapter *adapter)
{
	struct ixgb_hw *hw = &adapter->hw;
	struct net_device *netdev = adapter->netdev;
	struct pci_dev *pdev = adapter->pdev;

	

	hw->vendor_id = pdev->vendor;
	hw->device_id = pdev->device;
	hw->subsystem_vendor_id = pdev->subsystem_vendor;
	hw->subsystem_id = pdev->subsystem_device;

	hw->max_frame_size = netdev->mtu + ENET_HEADER_SIZE + ENET_FCS_LENGTH;
	adapter->rx_buffer_len = hw->max_frame_size + 8; 

	if ((hw->device_id == IXGB_DEVICE_ID_82597EX) ||
	    (hw->device_id == IXGB_DEVICE_ID_82597EX_CX4) ||
	    (hw->device_id == IXGB_DEVICE_ID_82597EX_LR) ||
	    (hw->device_id == IXGB_DEVICE_ID_82597EX_SR))
		hw->mac_type = ixgb_82597;
	else {
		
		netif_err(adapter, probe, adapter->netdev, "unsupported device id\n");
	}

	
	hw->fc.send_xon = 1;

	set_bit(__IXGB_DOWN, &adapter->flags);
	return 0;
}


static int
ixgb_open(struct net_device *netdev)
{
	struct ixgb_adapter *adapter = netdev_priv(netdev);
	int err;

	
	err = ixgb_setup_tx_resources(adapter);
	if (err)
		goto err_setup_tx;

	netif_carrier_off(netdev);

	

	err = ixgb_setup_rx_resources(adapter);
	if (err)
		goto err_setup_rx;

	err = ixgb_up(adapter);
	if (err)
		goto err_up;

	netif_start_queue(netdev);

	return 0;

err_up:
	ixgb_free_rx_resources(adapter);
err_setup_rx:
	ixgb_free_tx_resources(adapter);
err_setup_tx:
	ixgb_reset(adapter);

	return err;
}


static int
ixgb_close(struct net_device *netdev)
{
	struct ixgb_adapter *adapter = netdev_priv(netdev);

	ixgb_down(adapter, true);

	ixgb_free_tx_resources(adapter);
	ixgb_free_rx_resources(adapter);

	return 0;
}


int
ixgb_setup_tx_resources(struct ixgb_adapter *adapter)
{
	struct ixgb_desc_ring *txdr = &adapter->tx_ring;
	struct pci_dev *pdev = adapter->pdev;
	int size;

	size = sizeof(struct ixgb_buffer) * txdr->count;
	txdr->buffer_info = vzalloc(size);
	if (!txdr->buffer_info) {
		netif_err(adapter, probe, adapter->netdev,
			  "Unable to allocate transmit descriptor ring memory\n");
		return -ENOMEM;
	}

	

	txdr->size = txdr->count * sizeof(struct ixgb_tx_desc);
	txdr->size = ALIGN(txdr->size, 4096);

	txdr->desc = dma_alloc_coherent(&pdev->dev, txdr->size, &txdr->dma,
					GFP_KERNEL);
	if (!txdr->desc) {
		vfree(txdr->buffer_info);
		netif_err(adapter, probe, adapter->netdev,
			  "Unable to allocate transmit descriptor memory\n");
		return -ENOMEM;
	}
	memset(txdr->desc, 0, txdr->size);

	txdr->next_to_use = 0;
	txdr->next_to_clean = 0;

	return 0;
}


static void
ixgb_configure_tx(struct ixgb_adapter *adapter)
{
	u64 tdba = adapter->tx_ring.dma;
	u32 tdlen = adapter->tx_ring.count * sizeof(struct ixgb_tx_desc);
	u32 tctl;
	struct ixgb_hw *hw = &adapter->hw;


	IXGB_WRITE_REG(hw, TDBAL, (tdba & 0x00000000ffffffffULL));
	IXGB_WRITE_REG(hw, TDBAH, (tdba >> 32));

	IXGB_WRITE_REG(hw, TDLEN, tdlen);

	

	IXGB_WRITE_REG(hw, TDH, 0);
	IXGB_WRITE_REG(hw, TDT, 0);

	

	IXGB_WRITE_REG(hw, TIDV, adapter->tx_int_delay);

	

	tctl = IXGB_TCTL_TCE | IXGB_TCTL_TXEN | IXGB_TCTL_TPDE;
	IXGB_WRITE_REG(hw, TCTL, tctl);

	
	adapter->tx_cmd_type =
		IXGB_TX_DESC_TYPE |
		(adapter->tx_int_delay_enable ? IXGB_TX_DESC_CMD_IDE : 0);
}


int
ixgb_setup_rx_resources(struct ixgb_adapter *adapter)
{
	struct ixgb_desc_ring *rxdr = &adapter->rx_ring;
	struct pci_dev *pdev = adapter->pdev;
	int size;

	size = sizeof(struct ixgb_buffer) * rxdr->count;
	rxdr->buffer_info = vzalloc(size);
	if (!rxdr->buffer_info) {
		netif_err(adapter, probe, adapter->netdev,
			  "Unable to allocate receive descriptor ring\n");
		return -ENOMEM;
	}

	

	rxdr->size = rxdr->count * sizeof(struct ixgb_rx_desc);
	rxdr->size = ALIGN(rxdr->size, 4096);

	rxdr->desc = dma_alloc_coherent(&pdev->dev, rxdr->size, &rxdr->dma,
					GFP_KERNEL);

	if (!rxdr->desc) {
		vfree(rxdr->buffer_info);
		netif_err(adapter, probe, adapter->netdev,
			  "Unable to allocate receive descriptors\n");
		return -ENOMEM;
	}
	memset(rxdr->desc, 0, rxdr->size);

	rxdr->next_to_clean = 0;
	rxdr->next_to_use = 0;

	return 0;
}


static void
ixgb_setup_rctl(struct ixgb_adapter *adapter)
{
	u32 rctl;

	rctl = IXGB_READ_REG(&adapter->hw, RCTL);

	rctl &= ~(3 << IXGB_RCTL_MO_SHIFT);

	rctl |=
		IXGB_RCTL_BAM | IXGB_RCTL_RDMTS_1_2 |
		IXGB_RCTL_RXEN | IXGB_RCTL_CFF |
		(adapter->hw.mc_filter_type << IXGB_RCTL_MO_SHIFT);

	rctl |= IXGB_RCTL_SECRC;

	if (adapter->rx_buffer_len <= IXGB_RXBUFFER_2048)
		rctl |= IXGB_RCTL_BSIZE_2048;
	else if (adapter->rx_buffer_len <= IXGB_RXBUFFER_4096)
		rctl |= IXGB_RCTL_BSIZE_4096;
	else if (adapter->rx_buffer_len <= IXGB_RXBUFFER_8192)
		rctl |= IXGB_RCTL_BSIZE_8192;
	else if (adapter->rx_buffer_len <= IXGB_RXBUFFER_16384)
		rctl |= IXGB_RCTL_BSIZE_16384;

	IXGB_WRITE_REG(&adapter->hw, RCTL, rctl);
}


static void
ixgb_configure_rx(struct ixgb_adapter *adapter)
{
	u64 rdba = adapter->rx_ring.dma;
	u32 rdlen = adapter->rx_ring.count * sizeof(struct ixgb_rx_desc);
	struct ixgb_hw *hw = &adapter->hw;
	u32 rctl;
	u32 rxcsum;

	

	rctl = IXGB_READ_REG(hw, RCTL);
	IXGB_WRITE_REG(hw, RCTL, rctl & ~IXGB_RCTL_RXEN);

	

	IXGB_WRITE_REG(hw, RDTR, adapter->rx_int_delay);

	

	IXGB_WRITE_REG(hw, RDBAL, (rdba & 0x00000000ffffffffULL));
	IXGB_WRITE_REG(hw, RDBAH, (rdba >> 32));

	IXGB_WRITE_REG(hw, RDLEN, rdlen);

	
	IXGB_WRITE_REG(hw, RDH, 0);
	IXGB_WRITE_REG(hw, RDT, 0);

	IXGB_WRITE_REG(hw, RXDCTL, 0);

	
	if (adapter->rx_csum) {
		rxcsum = IXGB_READ_REG(hw, RXCSUM);
		rxcsum |= IXGB_RXCSUM_TUOFL;
		IXGB_WRITE_REG(hw, RXCSUM, rxcsum);
	}

	

	IXGB_WRITE_REG(hw, RCTL, rctl);
}


void
ixgb_free_tx_resources(struct ixgb_adapter *adapter)
{
	struct pci_dev *pdev = adapter->pdev;

	ixgb_clean_tx_ring(adapter);

	vfree(adapter->tx_ring.buffer_info);
	adapter->tx_ring.buffer_info = NULL;

	dma_free_coherent(&pdev->dev, adapter->tx_ring.size,
			  adapter->tx_ring.desc, adapter->tx_ring.dma);

	adapter->tx_ring.desc = NULL;
}

static void
ixgb_unmap_and_free_tx_resource(struct ixgb_adapter *adapter,
                                struct ixgb_buffer *buffer_info)
{
	if (buffer_info->dma) {
		if (buffer_info->mapped_as_page)
			dma_unmap_page(&adapter->pdev->dev, buffer_info->dma,
				       buffer_info->length, DMA_TO_DEVICE);
		else
			dma_unmap_single(&adapter->pdev->dev, buffer_info->dma,
					 buffer_info->length, DMA_TO_DEVICE);
		buffer_info->dma = 0;
	}

	if (buffer_info->skb) {
		dev_kfree_skb_any(buffer_info->skb);
		buffer_info->skb = NULL;
	}
	buffer_info->time_stamp = 0;
}


static void
ixgb_clean_tx_ring(struct ixgb_adapter *adapter)
{
	struct ixgb_desc_ring *tx_ring = &adapter->tx_ring;
	struct ixgb_buffer *buffer_info;
	unsigned long size;
	unsigned int i;

	

	for (i = 0; i < tx_ring->count; i++) {
		buffer_info = &tx_ring->buffer_info[i];
		ixgb_unmap_and_free_tx_resource(adapter, buffer_info);
	}

	size = sizeof(struct ixgb_buffer) * tx_ring->count;
	memset(tx_ring->buffer_info, 0, size);

	

	memset(tx_ring->desc, 0, tx_ring->size);

	tx_ring->next_to_use = 0;
	tx_ring->next_to_clean = 0;

	IXGB_WRITE_REG(&adapter->hw, TDH, 0);
	IXGB_WRITE_REG(&adapter->hw, TDT, 0);
}


void
ixgb_free_rx_resources(struct ixgb_adapter *adapter)
{
	struct ixgb_desc_ring *rx_ring = &adapter->rx_ring;
	struct pci_dev *pdev = adapter->pdev;

	ixgb_clean_rx_ring(adapter);

	vfree(rx_ring->buffer_info);
	rx_ring->buffer_info = NULL;

	dma_free_coherent(&pdev->dev, rx_ring->size, rx_ring->desc,
			  rx_ring->dma);

	rx_ring->desc = NULL;
}


static void
ixgb_clean_rx_ring(struct ixgb_adapter *adapter)
{
	struct ixgb_desc_ring *rx_ring = &adapter->rx_ring;
	struct ixgb_buffer *buffer_info;
	struct pci_dev *pdev = adapter->pdev;
	unsigned long size;
	unsigned int i;

	

	for (i = 0; i < rx_ring->count; i++) {
		buffer_info = &rx_ring->buffer_info[i];
		if (buffer_info->dma) {
			dma_unmap_single(&pdev->dev,
					 buffer_info->dma,
					 buffer_info->length,
					 DMA_FROM_DEVICE);
			buffer_info->dma = 0;
			buffer_info->length = 0;
		}

		if (buffer_info->skb) {
			dev_kfree_skb(buffer_info->skb);
			buffer_info->skb = NULL;
		}
	}

	size = sizeof(struct ixgb_buffer) * rx_ring->count;
	memset(rx_ring->buffer_info, 0, size);

	

	memset(rx_ring->desc, 0, rx_ring->size);

	rx_ring->next_to_clean = 0;
	rx_ring->next_to_use = 0;

	IXGB_WRITE_REG(&adapter->hw, RDH, 0);
	IXGB_WRITE_REG(&adapter->hw, RDT, 0);
}


static int
ixgb_set_mac(struct net_device *netdev, void *p)
{
	struct ixgb_adapter *adapter = netdev_priv(netdev);
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);

	ixgb_rar_set(&adapter->hw, addr->sa_data, 0);

	return 0;
}


static void
ixgb_set_multi(struct net_device *netdev)
{
	struct ixgb_adapter *adapter = netdev_priv(netdev);
	struct ixgb_hw *hw = &adapter->hw;
	struct netdev_hw_addr *ha;
	u32 rctl;

	

	rctl = IXGB_READ_REG(hw, RCTL);

	if (netdev->flags & IFF_PROMISC) {
		rctl |= (IXGB_RCTL_UPE | IXGB_RCTL_MPE);
		
		rctl &= ~IXGB_RCTL_CFIEN;
		rctl &= ~IXGB_RCTL_VFE;
	} else {
		if (netdev->flags & IFF_ALLMULTI) {
			rctl |= IXGB_RCTL_MPE;
			rctl &= ~IXGB_RCTL_UPE;
		} else {
			rctl &= ~(IXGB_RCTL_UPE | IXGB_RCTL_MPE);
		}
		
		rctl |= IXGB_RCTL_VFE;
		rctl &= ~IXGB_RCTL_CFIEN;
	}

	if (netdev_mc_count(netdev) > IXGB_MAX_NUM_MULTICAST_ADDRESSES) {
		rctl |= IXGB_RCTL_MPE;
		IXGB_WRITE_REG(hw, RCTL, rctl);
	} else {
		u8 *mta = kmalloc(IXGB_MAX_NUM_MULTICAST_ADDRESSES *
			      ETH_ALEN, GFP_ATOMIC);
		u8 *addr;
		if (!mta)
			goto alloc_failed;

		IXGB_WRITE_REG(hw, RCTL, rctl);

		addr = mta;
		netdev_for_each_mc_addr(ha, netdev) {
			memcpy(addr, ha->addr, ETH_ALEN);
			addr += ETH_ALEN;
		}

		ixgb_mc_addr_list_update(hw, mta, netdev_mc_count(netdev), 0);
		kfree(mta);
	}

alloc_failed:
	if (netdev->features & NETIF_F_HW_VLAN_RX)
		ixgb_vlan_strip_enable(adapter);
	else
		ixgb_vlan_strip_disable(adapter);

}


static void
ixgb_watchdog(unsigned long data)
{
	struct ixgb_adapter *adapter = (struct ixgb_adapter *)data;
	struct net_device *netdev = adapter->netdev;
	struct ixgb_desc_ring *txdr = &adapter->tx_ring;

	ixgb_check_for_link(&adapter->hw);

	if (ixgb_check_for_bad_link(&adapter->hw)) {
		
		netif_stop_queue(netdev);
	}

	if (adapter->hw.link_up) {
		if (!netif_carrier_ok(netdev)) {
			netdev_info(netdev,
				    "NIC Link is Up 10 Gbps Full Duplex, Flow Control: %s\n",
				    (adapter->hw.fc.type == ixgb_fc_full) ?
				    "RX/TX" :
				    (adapter->hw.fc.type == ixgb_fc_rx_pause) ?
				     "RX" :
				    (adapter->hw.fc.type == ixgb_fc_tx_pause) ?
				    "TX" : "None");
			adapter->link_speed = 10000;
			adapter->link_duplex = FULL_DUPLEX;
			netif_carrier_on(netdev);
		}
	} else {
		if (netif_carrier_ok(netdev)) {
			adapter->link_speed = 0;
			adapter->link_duplex = 0;
			netdev_info(netdev, "NIC Link is Down\n");
			netif_carrier_off(netdev);
		}
	}

	ixgb_update_stats(adapter);

	if (!netif_carrier_ok(netdev)) {
		if (IXGB_DESC_UNUSED(txdr) + 1 < txdr->count) {
			schedule_work(&adapter->tx_timeout_task);
			
			return;
		}
	}

	
	adapter->detect_tx_hung = true;

	
	IXGB_WRITE_REG(&adapter->hw, ICS, IXGB_INT_TXDW);

	
	mod_timer(&adapter->watchdog_timer, jiffies + 2 * HZ);
}

#define IXGB_TX_FLAGS_CSUM		0x00000001
#define IXGB_TX_FLAGS_VLAN		0x00000002
#define IXGB_TX_FLAGS_TSO		0x00000004

static int
ixgb_tso(struct ixgb_adapter *adapter, struct sk_buff *skb)
{
	struct ixgb_context_desc *context_desc;
	unsigned int i;
	u8 ipcss, ipcso, tucss, tucso, hdr_len;
	u16 ipcse, tucse, mss;
	int err;

	if (likely(skb_is_gso(skb))) {
		struct ixgb_buffer *buffer_info;
		struct iphdr *iph;

		if (skb_header_cloned(skb)) {
			err = pskb_expand_head(skb, 0, 0, GFP_ATOMIC);
			if (err)
				return err;
		}

		hdr_len = skb_transport_offset(skb) + tcp_hdrlen(skb);
		mss = skb_shinfo(skb)->gso_size;
		iph = ip_hdr(skb);
		iph->tot_len = 0;
		iph->check = 0;
		tcp_hdr(skb)->check = ~csum_tcpudp_magic(iph->saddr,
							 iph->daddr, 0,
							 IPPROTO_TCP, 0);
		ipcss = skb_network_offset(skb);
		ipcso = (void *)&(iph->check) - (void *)skb->data;
		ipcse = skb_transport_offset(skb) - 1;
		tucss = skb_transport_offset(skb);
		tucso = (void *)&(tcp_hdr(skb)->check) - (void *)skb->data;
		tucse = 0;

		i = adapter->tx_ring.next_to_use;
		context_desc = IXGB_CONTEXT_DESC(adapter->tx_ring, i);
		buffer_info = &adapter->tx_ring.buffer_info[i];
		WARN_ON(buffer_info->dma != 0);

		context_desc->ipcss = ipcss;
		context_desc->ipcso = ipcso;
		context_desc->ipcse = cpu_to_le16(ipcse);
		context_desc->tucss = tucss;
		context_desc->tucso = tucso;
		context_desc->tucse = cpu_to_le16(tucse);
		context_desc->mss = cpu_to_le16(mss);
		context_desc->hdr_len = hdr_len;
		context_desc->status = 0;
		context_desc->cmd_type_len = cpu_to_le32(
						  IXGB_CONTEXT_DESC_TYPE
						| IXGB_CONTEXT_DESC_CMD_TSE
						| IXGB_CONTEXT_DESC_CMD_IP
						| IXGB_CONTEXT_DESC_CMD_TCP
						| IXGB_CONTEXT_DESC_CMD_IDE
						| (skb->len - (hdr_len)));


		if (++i == adapter->tx_ring.count) i = 0;
		adapter->tx_ring.next_to_use = i;

		return 1;
	}

	return 0;
}

static bool
ixgb_tx_csum(struct ixgb_adapter *adapter, struct sk_buff *skb)
{
	struct ixgb_context_desc *context_desc;
	unsigned int i;
	u8 css, cso;

	if (likely(skb->ip_summed == CHECKSUM_PARTIAL)) {
		struct ixgb_buffer *buffer_info;
		css = skb_checksum_start_offset(skb);
		cso = css + skb->csum_offset;

		i = adapter->tx_ring.next_to_use;
		context_desc = IXGB_CONTEXT_DESC(adapter->tx_ring, i);
		buffer_info = &adapter->tx_ring.buffer_info[i];
		WARN_ON(buffer_info->dma != 0);

		context_desc->tucss = css;
		context_desc->tucso = cso;
		context_desc->tucse = 0;
		
		*(u32 *)&(context_desc->ipcss) = 0;
		context_desc->status = 0;
		context_desc->hdr_len = 0;
		context_desc->mss = 0;
		context_desc->cmd_type_len =
			cpu_to_le32(IXGB_CONTEXT_DESC_TYPE
				    | IXGB_TX_DESC_CMD_IDE);

		if (++i == adapter->tx_ring.count) i = 0;
		adapter->tx_ring.next_to_use = i;

		return true;
	}

	return false;
}

#define IXGB_MAX_TXD_PWR	14
#define IXGB_MAX_DATA_PER_TXD	(1<<IXGB_MAX_TXD_PWR)

static int
ixgb_tx_map(struct ixgb_adapter *adapter, struct sk_buff *skb,
	    unsigned int first)
{
	struct ixgb_desc_ring *tx_ring = &adapter->tx_ring;
	struct pci_dev *pdev = adapter->pdev;
	struct ixgb_buffer *buffer_info;
	int len = skb_headlen(skb);
	unsigned int offset = 0, size, count = 0, i;
	unsigned int mss = skb_shinfo(skb)->gso_size;
	unsigned int nr_frags = skb_shinfo(skb)->nr_frags;
	unsigned int f;

	i = tx_ring->next_to_use;

	while (len) {
		buffer_info = &tx_ring->buffer_info[i];
		size = min(len, IXGB_MAX_DATA_PER_TXD);
		if (unlikely(mss && !nr_frags && size == len && size > 8))
			size -= 4;

		buffer_info->length = size;
		WARN_ON(buffer_info->dma != 0);
		buffer_info->time_stamp = jiffies;
		buffer_info->mapped_as_page = false;
		buffer_info->dma = dma_map_single(&pdev->dev,
						  skb->data + offset,
						  size, DMA_TO_DEVICE);
		if (dma_mapping_error(&pdev->dev, buffer_info->dma))
			goto dma_error;
		buffer_info->next_to_watch = 0;

		len -= size;
		offset += size;
		count++;
		if (len) {
			i++;
			if (i == tx_ring->count)
				i = 0;
		}
	}

	for (f = 0; f < nr_frags; f++) {
		const struct skb_frag_struct *frag;

		frag = &skb_shinfo(skb)->frags[f];
		len = skb_frag_size(frag);
		offset = 0;

		while (len) {
			i++;
			if (i == tx_ring->count)
				i = 0;

			buffer_info = &tx_ring->buffer_info[i];
			size = min(len, IXGB_MAX_DATA_PER_TXD);

			if (unlikely(mss && (f == (nr_frags - 1))
				     && size == len && size > 8))
				size -= 4;

			buffer_info->length = size;
			buffer_info->time_stamp = jiffies;
			buffer_info->mapped_as_page = true;
			buffer_info->dma =
				skb_frag_dma_map(&pdev->dev, frag, offset, size,
						 DMA_TO_DEVICE);
			if (dma_mapping_error(&pdev->dev, buffer_info->dma))
				goto dma_error;
			buffer_info->next_to_watch = 0;

			len -= size;
			offset += size;
			count++;
		}
	}
	tx_ring->buffer_info[i].skb = skb;
	tx_ring->buffer_info[first].next_to_watch = i;

	return count;

dma_error:
	dev_err(&pdev->dev, "TX DMA map failed\n");
	buffer_info->dma = 0;
	if (count)
		count--;

	while (count--) {
		if (i==0)
			i += tx_ring->count;
		i--;
		buffer_info = &tx_ring->buffer_info[i];
		ixgb_unmap_and_free_tx_resource(adapter, buffer_info);
	}

	return 0;
}

static void
ixgb_tx_queue(struct ixgb_adapter *adapter, int count, int vlan_id,int tx_flags)
{
	struct ixgb_desc_ring *tx_ring = &adapter->tx_ring;
	struct ixgb_tx_desc *tx_desc = NULL;
	struct ixgb_buffer *buffer_info;
	u32 cmd_type_len = adapter->tx_cmd_type;
	u8 status = 0;
	u8 popts = 0;
	unsigned int i;

	if (tx_flags & IXGB_TX_FLAGS_TSO) {
		cmd_type_len |= IXGB_TX_DESC_CMD_TSE;
		popts |= (IXGB_TX_DESC_POPTS_IXSM | IXGB_TX_DESC_POPTS_TXSM);
	}

	if (tx_flags & IXGB_TX_FLAGS_CSUM)
		popts |= IXGB_TX_DESC_POPTS_TXSM;

	if (tx_flags & IXGB_TX_FLAGS_VLAN)
		cmd_type_len |= IXGB_TX_DESC_CMD_VLE;

	i = tx_ring->next_to_use;

	while (count--) {
		buffer_info = &tx_ring->buffer_info[i];
		tx_desc = IXGB_TX_DESC(*tx_ring, i);
		tx_desc->buff_addr = cpu_to_le64(buffer_info->dma);
		tx_desc->cmd_type_len =
			cpu_to_le32(cmd_type_len | buffer_info->length);
		tx_desc->status = status;
		tx_desc->popts = popts;
		tx_desc->vlan = cpu_to_le16(vlan_id);

		if (++i == tx_ring->count) i = 0;
	}

	tx_desc->cmd_type_len |=
		cpu_to_le32(IXGB_TX_DESC_CMD_EOP | IXGB_TX_DESC_CMD_RS);

	wmb();

	tx_ring->next_to_use = i;
	IXGB_WRITE_REG(&adapter->hw, TDT, i);
}

static int __ixgb_maybe_stop_tx(struct net_device *netdev, int size)
{
	struct ixgb_adapter *adapter = netdev_priv(netdev);
	struct ixgb_desc_ring *tx_ring = &adapter->tx_ring;

	netif_stop_queue(netdev);
	smp_mb();

	if (likely(IXGB_DESC_UNUSED(tx_ring) < size))
		return -EBUSY;

	
	netif_start_queue(netdev);
	++adapter->restart_queue;
	return 0;
}

static int ixgb_maybe_stop_tx(struct net_device *netdev,
                              struct ixgb_desc_ring *tx_ring, int size)
{
	if (likely(IXGB_DESC_UNUSED(tx_ring) >= size))
		return 0;
	return __ixgb_maybe_stop_tx(netdev, size);
}


#define TXD_USE_COUNT(S) (((S) >> IXGB_MAX_TXD_PWR) + \
			 (((S) & (IXGB_MAX_DATA_PER_TXD - 1)) ? 1 : 0))
#define DESC_NEEDED TXD_USE_COUNT(IXGB_MAX_DATA_PER_TXD)  + \
	MAX_SKB_FRAGS * TXD_USE_COUNT(PAGE_SIZE) + 1  \
	+ 1 

static netdev_tx_t
ixgb_xmit_frame(struct sk_buff *skb, struct net_device *netdev)
{
	struct ixgb_adapter *adapter = netdev_priv(netdev);
	unsigned int first;
	unsigned int tx_flags = 0;
	int vlan_id = 0;
	int count = 0;
	int tso;

	if (test_bit(__IXGB_DOWN, &adapter->flags)) {
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	if (skb->len <= 0) {
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	if (unlikely(ixgb_maybe_stop_tx(netdev, &adapter->tx_ring,
                     DESC_NEEDED)))
		return NETDEV_TX_BUSY;

	if (vlan_tx_tag_present(skb)) {
		tx_flags |= IXGB_TX_FLAGS_VLAN;
		vlan_id = vlan_tx_tag_get(skb);
	}

	first = adapter->tx_ring.next_to_use;

	tso = ixgb_tso(adapter, skb);
	if (tso < 0) {
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	if (likely(tso))
		tx_flags |= IXGB_TX_FLAGS_TSO;
	else if (ixgb_tx_csum(adapter, skb))
		tx_flags |= IXGB_TX_FLAGS_CSUM;

	count = ixgb_tx_map(adapter, skb, first);

	if (count) {
		ixgb_tx_queue(adapter, count, vlan_id, tx_flags);
		
		ixgb_maybe_stop_tx(netdev, &adapter->tx_ring, DESC_NEEDED);

	} else {
		dev_kfree_skb_any(skb);
		adapter->tx_ring.buffer_info[first].time_stamp = 0;
		adapter->tx_ring.next_to_use = first;
	}

	return NETDEV_TX_OK;
}


static void
ixgb_tx_timeout(struct net_device *netdev)
{
	struct ixgb_adapter *adapter = netdev_priv(netdev);

	
	schedule_work(&adapter->tx_timeout_task);
}

static void
ixgb_tx_timeout_task(struct work_struct *work)
{
	struct ixgb_adapter *adapter =
		container_of(work, struct ixgb_adapter, tx_timeout_task);

	adapter->tx_timeout_count++;
	ixgb_down(adapter, true);
	ixgb_up(adapter);
}


static struct net_device_stats *
ixgb_get_stats(struct net_device *netdev)
{
	return &netdev->stats;
}


static int
ixgb_change_mtu(struct net_device *netdev, int new_mtu)
{
	struct ixgb_adapter *adapter = netdev_priv(netdev);
	int max_frame = new_mtu + ENET_HEADER_SIZE + ENET_FCS_LENGTH;
	int old_max_frame = netdev->mtu + ENET_HEADER_SIZE + ENET_FCS_LENGTH;

	
	if ((new_mtu < 68) ||
	    (max_frame > IXGB_MAX_JUMBO_FRAME_SIZE + ENET_FCS_LENGTH)) {
		netif_err(adapter, probe, adapter->netdev,
			  "Invalid MTU setting %d\n", new_mtu);
		return -EINVAL;
	}

	if (old_max_frame == max_frame)
		return 0;

	if (netif_running(netdev))
		ixgb_down(adapter, true);

	adapter->rx_buffer_len = max_frame + 8; 

	netdev->mtu = new_mtu;

	if (netif_running(netdev))
		ixgb_up(adapter);

	return 0;
}


void
ixgb_update_stats(struct ixgb_adapter *adapter)
{
	struct net_device *netdev = adapter->netdev;
	struct pci_dev *pdev = adapter->pdev;

	
	if (pci_channel_offline(pdev))
		return;

	if ((netdev->flags & IFF_PROMISC) || (netdev->flags & IFF_ALLMULTI) ||
	   (netdev_mc_count(netdev) > IXGB_MAX_NUM_MULTICAST_ADDRESSES)) {
		u64 multi = IXGB_READ_REG(&adapter->hw, MPRCL);
		u32 bcast_l = IXGB_READ_REG(&adapter->hw, BPRCL);
		u32 bcast_h = IXGB_READ_REG(&adapter->hw, BPRCH);
		u64 bcast = ((u64)bcast_h << 32) | bcast_l;

		multi |= ((u64)IXGB_READ_REG(&adapter->hw, MPRCH) << 32);
		
		if (multi >= bcast)
			multi -= bcast;

		adapter->stats.mprcl += (multi & 0xFFFFFFFF);
		adapter->stats.mprch += (multi >> 32);
		adapter->stats.bprcl += bcast_l;
		adapter->stats.bprch += bcast_h;
	} else {
		adapter->stats.mprcl += IXGB_READ_REG(&adapter->hw, MPRCL);
		adapter->stats.mprch += IXGB_READ_REG(&adapter->hw, MPRCH);
		adapter->stats.bprcl += IXGB_READ_REG(&adapter->hw, BPRCL);
		adapter->stats.bprch += IXGB_READ_REG(&adapter->hw, BPRCH);
	}
	adapter->stats.tprl += IXGB_READ_REG(&adapter->hw, TPRL);
	adapter->stats.tprh += IXGB_READ_REG(&adapter->hw, TPRH);
	adapter->stats.gprcl += IXGB_READ_REG(&adapter->hw, GPRCL);
	adapter->stats.gprch += IXGB_READ_REG(&adapter->hw, GPRCH);
	adapter->stats.uprcl += IXGB_READ_REG(&adapter->hw, UPRCL);
	adapter->stats.uprch += IXGB_READ_REG(&adapter->hw, UPRCH);
	adapter->stats.vprcl += IXGB_READ_REG(&adapter->hw, VPRCL);
	adapter->stats.vprch += IXGB_READ_REG(&adapter->hw, VPRCH);
	adapter->stats.jprcl += IXGB_READ_REG(&adapter->hw, JPRCL);
	adapter->stats.jprch += IXGB_READ_REG(&adapter->hw, JPRCH);
	adapter->stats.gorcl += IXGB_READ_REG(&adapter->hw, GORCL);
	adapter->stats.gorch += IXGB_READ_REG(&adapter->hw, GORCH);
	adapter->stats.torl += IXGB_READ_REG(&adapter->hw, TORL);
	adapter->stats.torh += IXGB_READ_REG(&adapter->hw, TORH);
	adapter->stats.rnbc += IXGB_READ_REG(&adapter->hw, RNBC);
	adapter->stats.ruc += IXGB_READ_REG(&adapter->hw, RUC);
	adapter->stats.roc += IXGB_READ_REG(&adapter->hw, ROC);
	adapter->stats.rlec += IXGB_READ_REG(&adapter->hw, RLEC);
	adapter->stats.crcerrs += IXGB_READ_REG(&adapter->hw, CRCERRS);
	adapter->stats.icbc += IXGB_READ_REG(&adapter->hw, ICBC);
	adapter->stats.ecbc += IXGB_READ_REG(&adapter->hw, ECBC);
	adapter->stats.mpc += IXGB_READ_REG(&adapter->hw, MPC);
	adapter->stats.tptl += IXGB_READ_REG(&adapter->hw, TPTL);
	adapter->stats.tpth += IXGB_READ_REG(&adapter->hw, TPTH);
	adapter->stats.gptcl += IXGB_READ_REG(&adapter->hw, GPTCL);
	adapter->stats.gptch += IXGB_READ_REG(&adapter->hw, GPTCH);
	adapter->stats.bptcl += IXGB_READ_REG(&adapter->hw, BPTCL);
	adapter->stats.bptch += IXGB_READ_REG(&adapter->hw, BPTCH);
	adapter->stats.mptcl += IXGB_READ_REG(&adapter->hw, MPTCL);
	adapter->stats.mptch += IXGB_READ_REG(&adapter->hw, MPTCH);
	adapter->stats.uptcl += IXGB_READ_REG(&adapter->hw, UPTCL);
	adapter->stats.uptch += IXGB_READ_REG(&adapter->hw, UPTCH);
	adapter->stats.vptcl += IXGB_READ_REG(&adapter->hw, VPTCL);
	adapter->stats.vptch += IXGB_READ_REG(&adapter->hw, VPTCH);
	adapter->stats.jptcl += IXGB_READ_REG(&adapter->hw, JPTCL);
	adapter->stats.jptch += IXGB_READ_REG(&adapter->hw, JPTCH);
	adapter->stats.gotcl += IXGB_READ_REG(&adapter->hw, GOTCL);
	adapter->stats.gotch += IXGB_READ_REG(&adapter->hw, GOTCH);
	adapter->stats.totl += IXGB_READ_REG(&adapter->hw, TOTL);
	adapter->stats.toth += IXGB_READ_REG(&adapter->hw, TOTH);
	adapter->stats.dc += IXGB_READ_REG(&adapter->hw, DC);
	adapter->stats.plt64c += IXGB_READ_REG(&adapter->hw, PLT64C);
	adapter->stats.tsctc += IXGB_READ_REG(&adapter->hw, TSCTC);
	adapter->stats.tsctfc += IXGB_READ_REG(&adapter->hw, TSCTFC);
	adapter->stats.ibic += IXGB_READ_REG(&adapter->hw, IBIC);
	adapter->stats.rfc += IXGB_READ_REG(&adapter->hw, RFC);
	adapter->stats.lfc += IXGB_READ_REG(&adapter->hw, LFC);
	adapter->stats.pfrc += IXGB_READ_REG(&adapter->hw, PFRC);
	adapter->stats.pftc += IXGB_READ_REG(&adapter->hw, PFTC);
	adapter->stats.mcfrc += IXGB_READ_REG(&adapter->hw, MCFRC);
	adapter->stats.mcftc += IXGB_READ_REG(&adapter->hw, MCFTC);
	adapter->stats.xonrxc += IXGB_READ_REG(&adapter->hw, XONRXC);
	adapter->stats.xontxc += IXGB_READ_REG(&adapter->hw, XONTXC);
	adapter->stats.xoffrxc += IXGB_READ_REG(&adapter->hw, XOFFRXC);
	adapter->stats.xofftxc += IXGB_READ_REG(&adapter->hw, XOFFTXC);
	adapter->stats.rjc += IXGB_READ_REG(&adapter->hw, RJC);

	

	netdev->stats.rx_packets = adapter->stats.gprcl;
	netdev->stats.tx_packets = adapter->stats.gptcl;
	netdev->stats.rx_bytes = adapter->stats.gorcl;
	netdev->stats.tx_bytes = adapter->stats.gotcl;
	netdev->stats.multicast = adapter->stats.mprcl;
	netdev->stats.collisions = 0;

	netdev->stats.rx_errors =
	     adapter->stats.crcerrs +
	    adapter->stats.ruc +
	    adapter->stats.roc   +
	    adapter->stats.icbc +
	    adapter->stats.ecbc + adapter->stats.mpc;


	netdev->stats.rx_crc_errors = adapter->stats.crcerrs;
	netdev->stats.rx_fifo_errors = adapter->stats.mpc;
	netdev->stats.rx_missed_errors = adapter->stats.mpc;
	netdev->stats.rx_over_errors = adapter->stats.mpc;

	netdev->stats.tx_errors = 0;
	netdev->stats.rx_frame_errors = 0;
	netdev->stats.tx_aborted_errors = 0;
	netdev->stats.tx_carrier_errors = 0;
	netdev->stats.tx_fifo_errors = 0;
	netdev->stats.tx_heartbeat_errors = 0;
	netdev->stats.tx_window_errors = 0;
}

#define IXGB_MAX_INTR 10

static irqreturn_t
ixgb_intr(int irq, void *data)
{
	struct net_device *netdev = data;
	struct ixgb_adapter *adapter = netdev_priv(netdev);
	struct ixgb_hw *hw = &adapter->hw;
	u32 icr = IXGB_READ_REG(hw, ICR);

	if (unlikely(!icr))
		return IRQ_NONE;  

	if (unlikely(icr & (IXGB_INT_RXSEQ | IXGB_INT_LSC)))
		if (!test_bit(__IXGB_DOWN, &adapter->flags))
			mod_timer(&adapter->watchdog_timer, jiffies);

	if (napi_schedule_prep(&adapter->napi)) {


		IXGB_WRITE_REG(&adapter->hw, IMC, ~0);
		__napi_schedule(&adapter->napi);
	}
	return IRQ_HANDLED;
}


static int
ixgb_clean(struct napi_struct *napi, int budget)
{
	struct ixgb_adapter *adapter = container_of(napi, struct ixgb_adapter, napi);
	int work_done = 0;

	ixgb_clean_tx_irq(adapter);
	ixgb_clean_rx_irq(adapter, &work_done, budget);

	
	if (work_done < budget) {
		napi_complete(napi);
		if (!test_bit(__IXGB_DOWN, &adapter->flags))
			ixgb_irq_enable(adapter);
	}

	return work_done;
}


static bool
ixgb_clean_tx_irq(struct ixgb_adapter *adapter)
{
	struct ixgb_desc_ring *tx_ring = &adapter->tx_ring;
	struct net_device *netdev = adapter->netdev;
	struct ixgb_tx_desc *tx_desc, *eop_desc;
	struct ixgb_buffer *buffer_info;
	unsigned int i, eop;
	bool cleaned = false;

	i = tx_ring->next_to_clean;
	eop = tx_ring->buffer_info[i].next_to_watch;
	eop_desc = IXGB_TX_DESC(*tx_ring, eop);

	while (eop_desc->status & IXGB_TX_DESC_STATUS_DD) {

		rmb(); 
		for (cleaned = false; !cleaned; ) {
			tx_desc = IXGB_TX_DESC(*tx_ring, i);
			buffer_info = &tx_ring->buffer_info[i];

			if (tx_desc->popts &
			   (IXGB_TX_DESC_POPTS_TXSM |
			    IXGB_TX_DESC_POPTS_IXSM))
				adapter->hw_csum_tx_good++;

			ixgb_unmap_and_free_tx_resource(adapter, buffer_info);

			*(u32 *)&(tx_desc->status) = 0;

			cleaned = (i == eop);
			if (++i == tx_ring->count) i = 0;
		}

		eop = tx_ring->buffer_info[i].next_to_watch;
		eop_desc = IXGB_TX_DESC(*tx_ring, eop);
	}

	tx_ring->next_to_clean = i;

	if (unlikely(cleaned && netif_carrier_ok(netdev) &&
		     IXGB_DESC_UNUSED(tx_ring) >= DESC_NEEDED)) {
		smp_mb();

		if (netif_queue_stopped(netdev) &&
		    !(test_bit(__IXGB_DOWN, &adapter->flags))) {
			netif_wake_queue(netdev);
			++adapter->restart_queue;
		}
	}

	if (adapter->detect_tx_hung) {
		adapter->detect_tx_hung = false;
		if (tx_ring->buffer_info[eop].time_stamp &&
		   time_after(jiffies, tx_ring->buffer_info[eop].time_stamp + HZ)
		   && !(IXGB_READ_REG(&adapter->hw, STATUS) &
		        IXGB_STATUS_TXOFF)) {
			
			netif_err(adapter, drv, adapter->netdev,
				  "Detected Tx Unit Hang\n"
				  "  TDH                  <%x>\n"
				  "  TDT                  <%x>\n"
				  "  next_to_use          <%x>\n"
				  "  next_to_clean        <%x>\n"
				  "buffer_info[next_to_clean]\n"
				  "  time_stamp           <%lx>\n"
				  "  next_to_watch        <%x>\n"
				  "  jiffies              <%lx>\n"
				  "  next_to_watch.status <%x>\n",
				  IXGB_READ_REG(&adapter->hw, TDH),
				  IXGB_READ_REG(&adapter->hw, TDT),
				  tx_ring->next_to_use,
				  tx_ring->next_to_clean,
				  tx_ring->buffer_info[eop].time_stamp,
				  eop,
				  jiffies,
				  eop_desc->status);
			netif_stop_queue(netdev);
		}
	}

	return cleaned;
}


static void
ixgb_rx_checksum(struct ixgb_adapter *adapter,
                 struct ixgb_rx_desc *rx_desc,
                 struct sk_buff *skb)
{
	if ((rx_desc->status & IXGB_RX_DESC_STATUS_IXSM) ||
	   (!(rx_desc->status & IXGB_RX_DESC_STATUS_TCPCS))) {
		skb_checksum_none_assert(skb);
		return;
	}

	
	
	if (rx_desc->errors & IXGB_RX_DESC_ERRORS_TCPE) {
		
		skb_checksum_none_assert(skb);
		adapter->hw_csum_rx_error++;
	} else {
		
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		adapter->hw_csum_rx_good++;
	}
}

static void ixgb_check_copybreak(struct net_device *netdev,
				 struct ixgb_buffer *buffer_info,
				 u32 length, struct sk_buff **skb)
{
	struct sk_buff *new_skb;

	if (length > copybreak)
		return;

	new_skb = netdev_alloc_skb_ip_align(netdev, length);
	if (!new_skb)
		return;

	skb_copy_to_linear_data_offset(new_skb, -NET_IP_ALIGN,
				       (*skb)->data - NET_IP_ALIGN,
				       length + NET_IP_ALIGN);
	
	buffer_info->skb = *skb;
	*skb = new_skb;
}


static bool
ixgb_clean_rx_irq(struct ixgb_adapter *adapter, int *work_done, int work_to_do)
{
	struct ixgb_desc_ring *rx_ring = &adapter->rx_ring;
	struct net_device *netdev = adapter->netdev;
	struct pci_dev *pdev = adapter->pdev;
	struct ixgb_rx_desc *rx_desc, *next_rxd;
	struct ixgb_buffer *buffer_info, *next_buffer, *next2_buffer;
	u32 length;
	unsigned int i, j;
	int cleaned_count = 0;
	bool cleaned = false;

	i = rx_ring->next_to_clean;
	rx_desc = IXGB_RX_DESC(*rx_ring, i);
	buffer_info = &rx_ring->buffer_info[i];

	while (rx_desc->status & IXGB_RX_DESC_STATUS_DD) {
		struct sk_buff *skb;
		u8 status;

		if (*work_done >= work_to_do)
			break;

		(*work_done)++;
		rmb();	
		status = rx_desc->status;
		skb = buffer_info->skb;
		buffer_info->skb = NULL;

		prefetch(skb->data - NET_IP_ALIGN);

		if (++i == rx_ring->count)
			i = 0;
		next_rxd = IXGB_RX_DESC(*rx_ring, i);
		prefetch(next_rxd);

		j = i + 1;
		if (j == rx_ring->count)
			j = 0;
		next2_buffer = &rx_ring->buffer_info[j];
		prefetch(next2_buffer);

		next_buffer = &rx_ring->buffer_info[i];

		cleaned = true;
		cleaned_count++;

		dma_unmap_single(&pdev->dev,
				 buffer_info->dma,
				 buffer_info->length,
				 DMA_FROM_DEVICE);
		buffer_info->dma = 0;

		length = le16_to_cpu(rx_desc->length);
		rx_desc->length = 0;

		if (unlikely(!(status & IXGB_RX_DESC_STATUS_EOP))) {

			

			pr_debug("Receive packet consumed multiple buffers length<%x>\n",
				 length);

			dev_kfree_skb_irq(skb);
			goto rxdesc_done;
		}

		if (unlikely(rx_desc->errors &
		    (IXGB_RX_DESC_ERRORS_CE | IXGB_RX_DESC_ERRORS_SE |
		     IXGB_RX_DESC_ERRORS_P | IXGB_RX_DESC_ERRORS_RXE))) {
			dev_kfree_skb_irq(skb);
			goto rxdesc_done;
		}

		ixgb_check_copybreak(netdev, buffer_info, length, &skb);

		
		skb_put(skb, length);

		
		ixgb_rx_checksum(adapter, rx_desc, skb);

		skb->protocol = eth_type_trans(skb, netdev);
		if (status & IXGB_RX_DESC_STATUS_VP)
			__vlan_hwaccel_put_tag(skb,
					       le16_to_cpu(rx_desc->special));

		netif_receive_skb(skb);

rxdesc_done:
		/* clean up descriptor, might be written over by hw */
		rx_desc->status = 0;

		
		if (unlikely(cleaned_count >= IXGB_RX_BUFFER_WRITE)) {
			ixgb_alloc_rx_buffers(adapter, cleaned_count);
			cleaned_count = 0;
		}

		
		rx_desc = next_rxd;
		buffer_info = next_buffer;
	}

	rx_ring->next_to_clean = i;

	cleaned_count = IXGB_DESC_UNUSED(rx_ring);
	if (cleaned_count)
		ixgb_alloc_rx_buffers(adapter, cleaned_count);

	return cleaned;
}


static void
ixgb_alloc_rx_buffers(struct ixgb_adapter *adapter, int cleaned_count)
{
	struct ixgb_desc_ring *rx_ring = &adapter->rx_ring;
	struct net_device *netdev = adapter->netdev;
	struct pci_dev *pdev = adapter->pdev;
	struct ixgb_rx_desc *rx_desc;
	struct ixgb_buffer *buffer_info;
	struct sk_buff *skb;
	unsigned int i;
	long cleancount;

	i = rx_ring->next_to_use;
	buffer_info = &rx_ring->buffer_info[i];
	cleancount = IXGB_DESC_UNUSED(rx_ring);


	
	while (--cleancount > 2 && cleaned_count--) {
		
		skb = buffer_info->skb;
		if (skb) {
			skb_trim(skb, 0);
			goto map_skb;
		}

		skb = netdev_alloc_skb_ip_align(netdev, adapter->rx_buffer_len);
		if (unlikely(!skb)) {
			
			adapter->alloc_rx_buff_failed++;
			break;
		}

		buffer_info->skb = skb;
		buffer_info->length = adapter->rx_buffer_len;
map_skb:
		buffer_info->dma = dma_map_single(&pdev->dev,
		                                  skb->data,
		                                  adapter->rx_buffer_len,
						  DMA_FROM_DEVICE);

		rx_desc = IXGB_RX_DESC(*rx_ring, i);
		rx_desc->buff_addr = cpu_to_le64(buffer_info->dma);
		rx_desc->status = 0;


		if (++i == rx_ring->count) i = 0;
		buffer_info = &rx_ring->buffer_info[i];
	}

	if (likely(rx_ring->next_to_use != i)) {
		rx_ring->next_to_use = i;
		if (unlikely(i-- == 0))
			i = (rx_ring->count - 1);

		wmb();
		IXGB_WRITE_REG(&adapter->hw, RDT, i);
	}
}

static void
ixgb_vlan_strip_enable(struct ixgb_adapter *adapter)
{
	u32 ctrl;

	
	ctrl = IXGB_READ_REG(&adapter->hw, CTRL0);
	ctrl |= IXGB_CTRL0_VME;
	IXGB_WRITE_REG(&adapter->hw, CTRL0, ctrl);
}

static void
ixgb_vlan_strip_disable(struct ixgb_adapter *adapter)
{
	u32 ctrl;

	
	ctrl = IXGB_READ_REG(&adapter->hw, CTRL0);
	ctrl &= ~IXGB_CTRL0_VME;
	IXGB_WRITE_REG(&adapter->hw, CTRL0, ctrl);
}

static int
ixgb_vlan_rx_add_vid(struct net_device *netdev, u16 vid)
{
	struct ixgb_adapter *adapter = netdev_priv(netdev);
	u32 vfta, index;

	

	index = (vid >> 5) & 0x7F;
	vfta = IXGB_READ_REG_ARRAY(&adapter->hw, VFTA, index);
	vfta |= (1 << (vid & 0x1F));
	ixgb_write_vfta(&adapter->hw, index, vfta);
	set_bit(vid, adapter->active_vlans);

	return 0;
}

static int
ixgb_vlan_rx_kill_vid(struct net_device *netdev, u16 vid)
{
	struct ixgb_adapter *adapter = netdev_priv(netdev);
	u32 vfta, index;

	

	index = (vid >> 5) & 0x7F;
	vfta = IXGB_READ_REG_ARRAY(&adapter->hw, VFTA, index);
	vfta &= ~(1 << (vid & 0x1F));
	ixgb_write_vfta(&adapter->hw, index, vfta);
	clear_bit(vid, adapter->active_vlans);

	return 0;
}

static void
ixgb_restore_vlan(struct ixgb_adapter *adapter)
{
	u16 vid;

	for_each_set_bit(vid, adapter->active_vlans, VLAN_N_VID)
		ixgb_vlan_rx_add_vid(adapter->netdev, vid);
}

#ifdef CONFIG_NET_POLL_CONTROLLER

static void ixgb_netpoll(struct net_device *dev)
{
	struct ixgb_adapter *adapter = netdev_priv(dev);

	disable_irq(adapter->pdev->irq);
	ixgb_intr(adapter->pdev->irq, dev);
	enable_irq(adapter->pdev->irq);
}
#endif

static pci_ers_result_t ixgb_io_error_detected(struct pci_dev *pdev,
                                               enum pci_channel_state state)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct ixgb_adapter *adapter = netdev_priv(netdev);

	netif_device_detach(netdev);

	if (state == pci_channel_io_perm_failure)
		return PCI_ERS_RESULT_DISCONNECT;

	if (netif_running(netdev))
		ixgb_down(adapter, true);

	pci_disable_device(pdev);

	
	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t ixgb_io_slot_reset(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct ixgb_adapter *adapter = netdev_priv(netdev);

	if (pci_enable_device(pdev)) {
		netif_err(adapter, probe, adapter->netdev,
			  "Cannot re-enable PCI device after reset\n");
		return PCI_ERS_RESULT_DISCONNECT;
	}

	
	if (0 != PCI_FUNC (pdev->devfn))
		return PCI_ERS_RESULT_RECOVERED;

	pci_set_master(pdev);

	netif_carrier_off(netdev);
	netif_stop_queue(netdev);
	ixgb_reset(adapter);

	
	if (!ixgb_validate_eeprom_checksum(&adapter->hw)) {
		netif_err(adapter, probe, adapter->netdev,
			  "After reset, the EEPROM checksum is not valid\n");
		return PCI_ERS_RESULT_DISCONNECT;
	}
	ixgb_get_ee_mac_addr(&adapter->hw, netdev->dev_addr);
	memcpy(netdev->perm_addr, netdev->dev_addr, netdev->addr_len);

	if (!is_valid_ether_addr(netdev->perm_addr)) {
		netif_err(adapter, probe, adapter->netdev,
			  "After reset, invalid MAC address\n");
		return PCI_ERS_RESULT_DISCONNECT;
	}

	return PCI_ERS_RESULT_RECOVERED;
}

static void ixgb_io_resume(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct ixgb_adapter *adapter = netdev_priv(netdev);

	pci_set_master(pdev);

	if (netif_running(netdev)) {
		if (ixgb_up(adapter)) {
			pr_err("can't bring device back up after reset\n");
			return;
		}
	}

	netif_device_attach(netdev);
	mod_timer(&adapter->watchdog_timer, jiffies);
}

