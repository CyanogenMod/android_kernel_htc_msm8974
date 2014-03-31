/*
 * QLogic qlge NIC HBA Driver
 * Copyright (c)  2003-2008 QLogic Corporation
 * See LICENSE.qlge for copyright and licensing details.
 * Author:     Linux qlge network device driver by
 *                      Ron Mercer <ron.mercer@qlogic.com>
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/pagemap.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/dmapool.h>
#include <linux/mempool.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <net/ipv6.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/prefetch.h>
#include <net/ip6_checksum.h>

#include "qlge.h"

char qlge_driver_name[] = DRV_NAME;
const char qlge_driver_version[] = DRV_VERSION;

MODULE_AUTHOR("Ron Mercer <ron.mercer@qlogic.com>");
MODULE_DESCRIPTION(DRV_STRING " ");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

static const u32 default_msg =
    NETIF_MSG_DRV | NETIF_MSG_PROBE | NETIF_MSG_LINK |
    NETIF_MSG_IFDOWN |
    NETIF_MSG_IFUP |
    NETIF_MSG_RX_ERR |
    NETIF_MSG_TX_ERR |
    NETIF_MSG_HW | NETIF_MSG_WOL | 0;

static int debug = -1;	
module_param(debug, int, 0664);
MODULE_PARM_DESC(debug, "Debug level (0=none,...,16=all)");

#define MSIX_IRQ 0
#define MSI_IRQ 1
#define LEG_IRQ 2
static int qlge_irq_type = MSIX_IRQ;
module_param(qlge_irq_type, int, 0664);
MODULE_PARM_DESC(qlge_irq_type, "0 = MSI-X, 1 = MSI, 2 = Legacy.");

static int qlge_mpi_coredump;
module_param(qlge_mpi_coredump, int, 0);
MODULE_PARM_DESC(qlge_mpi_coredump,
		"Option to enable MPI firmware dump. "
		"Default is OFF - Do Not allocate memory. ");

static int qlge_force_coredump;
module_param(qlge_force_coredump, int, 0);
MODULE_PARM_DESC(qlge_force_coredump,
		"Option to allow force of firmware core dump. "
		"Default is OFF - Do not allow.");

static DEFINE_PCI_DEVICE_TABLE(qlge_pci_tbl) = {
	{PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, QLGE_DEVICE_ID_8012)},
	{PCI_DEVICE(PCI_VENDOR_ID_QLOGIC, QLGE_DEVICE_ID_8000)},
	
	{0,}
};

MODULE_DEVICE_TABLE(pci, qlge_pci_tbl);

static int ql_wol(struct ql_adapter *qdev);
static void qlge_set_multicast_list(struct net_device *ndev);

static int ql_sem_trylock(struct ql_adapter *qdev, u32 sem_mask)
{
	u32 sem_bits = 0;

	switch (sem_mask) {
	case SEM_XGMAC0_MASK:
		sem_bits = SEM_SET << SEM_XGMAC0_SHIFT;
		break;
	case SEM_XGMAC1_MASK:
		sem_bits = SEM_SET << SEM_XGMAC1_SHIFT;
		break;
	case SEM_ICB_MASK:
		sem_bits = SEM_SET << SEM_ICB_SHIFT;
		break;
	case SEM_MAC_ADDR_MASK:
		sem_bits = SEM_SET << SEM_MAC_ADDR_SHIFT;
		break;
	case SEM_FLASH_MASK:
		sem_bits = SEM_SET << SEM_FLASH_SHIFT;
		break;
	case SEM_PROBE_MASK:
		sem_bits = SEM_SET << SEM_PROBE_SHIFT;
		break;
	case SEM_RT_IDX_MASK:
		sem_bits = SEM_SET << SEM_RT_IDX_SHIFT;
		break;
	case SEM_PROC_REG_MASK:
		sem_bits = SEM_SET << SEM_PROC_REG_SHIFT;
		break;
	default:
		netif_alert(qdev, probe, qdev->ndev, "bad Semaphore mask!.\n");
		return -EINVAL;
	}

	ql_write32(qdev, SEM, sem_bits | sem_mask);
	return !(ql_read32(qdev, SEM) & sem_bits);
}

int ql_sem_spinlock(struct ql_adapter *qdev, u32 sem_mask)
{
	unsigned int wait_count = 30;
	do {
		if (!ql_sem_trylock(qdev, sem_mask))
			return 0;
		udelay(100);
	} while (--wait_count);
	return -ETIMEDOUT;
}

void ql_sem_unlock(struct ql_adapter *qdev, u32 sem_mask)
{
	ql_write32(qdev, SEM, sem_mask);
	ql_read32(qdev, SEM);	
}

int ql_wait_reg_rdy(struct ql_adapter *qdev, u32 reg, u32 bit, u32 err_bit)
{
	u32 temp;
	int count = UDELAY_COUNT;

	while (count) {
		temp = ql_read32(qdev, reg);

		
		if (temp & err_bit) {
			netif_alert(qdev, probe, qdev->ndev,
				    "register 0x%.08x access error, value = 0x%.08x!.\n",
				    reg, temp);
			return -EIO;
		} else if (temp & bit)
			return 0;
		udelay(UDELAY_DELAY);
		count--;
	}
	netif_alert(qdev, probe, qdev->ndev,
		    "Timed out waiting for reg %x to come ready.\n", reg);
	return -ETIMEDOUT;
}

static int ql_wait_cfg(struct ql_adapter *qdev, u32 bit)
{
	int count = UDELAY_COUNT;
	u32 temp;

	while (count) {
		temp = ql_read32(qdev, CFG);
		if (temp & CFG_LE)
			return -EIO;
		if (!(temp & bit))
			return 0;
		udelay(UDELAY_DELAY);
		count--;
	}
	return -ETIMEDOUT;
}


int ql_write_cfg(struct ql_adapter *qdev, void *ptr, int size, u32 bit,
		 u16 q_id)
{
	u64 map;
	int status = 0;
	int direction;
	u32 mask;
	u32 value;

	direction =
	    (bit & (CFG_LRQ | CFG_LR | CFG_LCQ)) ? PCI_DMA_TODEVICE :
	    PCI_DMA_FROMDEVICE;

	map = pci_map_single(qdev->pdev, ptr, size, direction);
	if (pci_dma_mapping_error(qdev->pdev, map)) {
		netif_err(qdev, ifup, qdev->ndev, "Couldn't map DMA area.\n");
		return -ENOMEM;
	}

	status = ql_sem_spinlock(qdev, SEM_ICB_MASK);
	if (status)
		return status;

	status = ql_wait_cfg(qdev, bit);
	if (status) {
		netif_err(qdev, ifup, qdev->ndev,
			  "Timed out waiting for CFG to come ready.\n");
		goto exit;
	}

	ql_write32(qdev, ICB_L, (u32) map);
	ql_write32(qdev, ICB_H, (u32) (map >> 32));

	mask = CFG_Q_MASK | (bit << 16);
	value = bit | (q_id << CFG_Q_SHIFT);
	ql_write32(qdev, CFG, (mask | value));

	status = ql_wait_cfg(qdev, bit);
exit:
	ql_sem_unlock(qdev, SEM_ICB_MASK);	
	pci_unmap_single(qdev->pdev, map, size, direction);
	return status;
}

int ql_get_mac_addr_reg(struct ql_adapter *qdev, u32 type, u16 index,
			u32 *value)
{
	u32 offset = 0;
	int status;

	switch (type) {
	case MAC_ADDR_TYPE_MULTI_MAC:
	case MAC_ADDR_TYPE_CAM_MAC:
		{
			status =
			    ql_wait_reg_rdy(qdev,
				MAC_ADDR_IDX, MAC_ADDR_MW, 0);
			if (status)
				goto exit;
			ql_write32(qdev, MAC_ADDR_IDX, (offset++) | 
				   (index << MAC_ADDR_IDX_SHIFT) | 
				   MAC_ADDR_ADR | MAC_ADDR_RS | type); 
			status =
			    ql_wait_reg_rdy(qdev,
				MAC_ADDR_IDX, MAC_ADDR_MR, 0);
			if (status)
				goto exit;
			*value++ = ql_read32(qdev, MAC_ADDR_DATA);
			status =
			    ql_wait_reg_rdy(qdev,
				MAC_ADDR_IDX, MAC_ADDR_MW, 0);
			if (status)
				goto exit;
			ql_write32(qdev, MAC_ADDR_IDX, (offset++) | 
				   (index << MAC_ADDR_IDX_SHIFT) | 
				   MAC_ADDR_ADR | MAC_ADDR_RS | type); 
			status =
			    ql_wait_reg_rdy(qdev,
				MAC_ADDR_IDX, MAC_ADDR_MR, 0);
			if (status)
				goto exit;
			*value++ = ql_read32(qdev, MAC_ADDR_DATA);
			if (type == MAC_ADDR_TYPE_CAM_MAC) {
				status =
				    ql_wait_reg_rdy(qdev,
					MAC_ADDR_IDX, MAC_ADDR_MW, 0);
				if (status)
					goto exit;
				ql_write32(qdev, MAC_ADDR_IDX, (offset++) | 
					   (index << MAC_ADDR_IDX_SHIFT) | 
					   MAC_ADDR_ADR | MAC_ADDR_RS | type); 
				status =
				    ql_wait_reg_rdy(qdev, MAC_ADDR_IDX,
						    MAC_ADDR_MR, 0);
				if (status)
					goto exit;
				*value++ = ql_read32(qdev, MAC_ADDR_DATA);
			}
			break;
		}
	case MAC_ADDR_TYPE_VLAN:
	case MAC_ADDR_TYPE_MULTI_FLTR:
	default:
		netif_crit(qdev, ifup, qdev->ndev,
			   "Address type %d not yet supported.\n", type);
		status = -EPERM;
	}
exit:
	return status;
}

static int ql_set_mac_addr_reg(struct ql_adapter *qdev, u8 *addr, u32 type,
			       u16 index)
{
	u32 offset = 0;
	int status = 0;

	switch (type) {
	case MAC_ADDR_TYPE_MULTI_MAC:
		{
			u32 upper = (addr[0] << 8) | addr[1];
			u32 lower = (addr[2] << 24) | (addr[3] << 16) |
					(addr[4] << 8) | (addr[5]);

			status =
				ql_wait_reg_rdy(qdev,
				MAC_ADDR_IDX, MAC_ADDR_MW, 0);
			if (status)
				goto exit;
			ql_write32(qdev, MAC_ADDR_IDX, (offset++) |
				(index << MAC_ADDR_IDX_SHIFT) |
				type | MAC_ADDR_E);
			ql_write32(qdev, MAC_ADDR_DATA, lower);
			status =
				ql_wait_reg_rdy(qdev,
				MAC_ADDR_IDX, MAC_ADDR_MW, 0);
			if (status)
				goto exit;
			ql_write32(qdev, MAC_ADDR_IDX, (offset++) |
				(index << MAC_ADDR_IDX_SHIFT) |
				type | MAC_ADDR_E);

			ql_write32(qdev, MAC_ADDR_DATA, upper);
			status =
				ql_wait_reg_rdy(qdev,
				MAC_ADDR_IDX, MAC_ADDR_MW, 0);
			if (status)
				goto exit;
			break;
		}
	case MAC_ADDR_TYPE_CAM_MAC:
		{
			u32 cam_output;
			u32 upper = (addr[0] << 8) | addr[1];
			u32 lower =
			    (addr[2] << 24) | (addr[3] << 16) | (addr[4] << 8) |
			    (addr[5]);
			status =
			    ql_wait_reg_rdy(qdev,
				MAC_ADDR_IDX, MAC_ADDR_MW, 0);
			if (status)
				goto exit;
			ql_write32(qdev, MAC_ADDR_IDX, (offset++) | 
				   (index << MAC_ADDR_IDX_SHIFT) | 
				   type);	
			ql_write32(qdev, MAC_ADDR_DATA, lower);
			status =
			    ql_wait_reg_rdy(qdev,
				MAC_ADDR_IDX, MAC_ADDR_MW, 0);
			if (status)
				goto exit;
			ql_write32(qdev, MAC_ADDR_IDX, (offset++) | 
				   (index << MAC_ADDR_IDX_SHIFT) | 
				   type);	
			ql_write32(qdev, MAC_ADDR_DATA, upper);
			status =
			    ql_wait_reg_rdy(qdev,
				MAC_ADDR_IDX, MAC_ADDR_MW, 0);
			if (status)
				goto exit;
			ql_write32(qdev, MAC_ADDR_IDX, (offset) |	
				   (index << MAC_ADDR_IDX_SHIFT) |	
				   type);	
			cam_output = (CAM_OUT_ROUTE_NIC |
				      (qdev->
				       func << CAM_OUT_FUNC_SHIFT) |
					(0 << CAM_OUT_CQ_ID_SHIFT));
			if (qdev->ndev->features & NETIF_F_HW_VLAN_RX)
				cam_output |= CAM_OUT_RV;
			
			ql_write32(qdev, MAC_ADDR_DATA, cam_output);
			break;
		}
	case MAC_ADDR_TYPE_VLAN:
		{
			u32 enable_bit = *((u32 *) &addr[0]);
			status =
			    ql_wait_reg_rdy(qdev,
				MAC_ADDR_IDX, MAC_ADDR_MW, 0);
			if (status)
				goto exit;
			ql_write32(qdev, MAC_ADDR_IDX, offset |	
				   (index << MAC_ADDR_IDX_SHIFT) |	
				   type |	
				   enable_bit);	
			break;
		}
	case MAC_ADDR_TYPE_MULTI_FLTR:
	default:
		netif_crit(qdev, ifup, qdev->ndev,
			   "Address type %d not yet supported.\n", type);
		status = -EPERM;
	}
exit:
	return status;
}

static int ql_set_mac_addr(struct ql_adapter *qdev, int set)
{
	int status;
	char zero_mac_addr[ETH_ALEN];
	char *addr;

	if (set) {
		addr = &qdev->current_mac_addr[0];
		netif_printk(qdev, ifup, KERN_DEBUG, qdev->ndev,
			     "Set Mac addr %pM\n", addr);
	} else {
		memset(zero_mac_addr, 0, ETH_ALEN);
		addr = &zero_mac_addr[0];
		netif_printk(qdev, ifup, KERN_DEBUG, qdev->ndev,
			     "Clearing MAC address\n");
	}
	status = ql_sem_spinlock(qdev, SEM_MAC_ADDR_MASK);
	if (status)
		return status;
	status = ql_set_mac_addr_reg(qdev, (u8 *) addr,
			MAC_ADDR_TYPE_CAM_MAC, qdev->func * MAX_CQ);
	ql_sem_unlock(qdev, SEM_MAC_ADDR_MASK);
	if (status)
		netif_err(qdev, ifup, qdev->ndev,
			  "Failed to init mac address.\n");
	return status;
}

void ql_link_on(struct ql_adapter *qdev)
{
	netif_err(qdev, link, qdev->ndev, "Link is up.\n");
	netif_carrier_on(qdev->ndev);
	ql_set_mac_addr(qdev, 1);
}

void ql_link_off(struct ql_adapter *qdev)
{
	netif_err(qdev, link, qdev->ndev, "Link is down.\n");
	netif_carrier_off(qdev->ndev);
	ql_set_mac_addr(qdev, 0);
}

int ql_get_routing_reg(struct ql_adapter *qdev, u32 index, u32 *value)
{
	int status = 0;

	status = ql_wait_reg_rdy(qdev, RT_IDX, RT_IDX_MW, 0);
	if (status)
		goto exit;

	ql_write32(qdev, RT_IDX,
		   RT_IDX_TYPE_NICQ | RT_IDX_RS | (index << RT_IDX_IDX_SHIFT));
	status = ql_wait_reg_rdy(qdev, RT_IDX, RT_IDX_MR, 0);
	if (status)
		goto exit;
	*value = ql_read32(qdev, RT_DATA);
exit:
	return status;
}

static int ql_set_routing_reg(struct ql_adapter *qdev, u32 index, u32 mask,
			      int enable)
{
	int status = -EINVAL; 
	u32 value = 0;

	switch (mask) {
	case RT_IDX_CAM_HIT:
		{
			value = RT_IDX_DST_CAM_Q |	
			    RT_IDX_TYPE_NICQ |	
			    (RT_IDX_CAM_HIT_SLOT << RT_IDX_IDX_SHIFT);
			break;
		}
	case RT_IDX_VALID:	
		{
			value = RT_IDX_DST_DFLT_Q |	
			    RT_IDX_TYPE_NICQ |	
			    (RT_IDX_PROMISCUOUS_SLOT << RT_IDX_IDX_SHIFT);
			break;
		}
	case RT_IDX_ERR:	
		{
			value = RT_IDX_DST_DFLT_Q |	
			    RT_IDX_TYPE_NICQ |	
			    (RT_IDX_ALL_ERR_SLOT << RT_IDX_IDX_SHIFT);
			break;
		}
	case RT_IDX_IP_CSUM_ERR: 
		{
			value = RT_IDX_DST_DFLT_Q | 
				RT_IDX_TYPE_NICQ | 
				(RT_IDX_IP_CSUM_ERR_SLOT <<
				RT_IDX_IDX_SHIFT); 
			break;
		}
	case RT_IDX_TU_CSUM_ERR: 
		{
			value = RT_IDX_DST_DFLT_Q | 
				RT_IDX_TYPE_NICQ | 
				(RT_IDX_TCP_UDP_CSUM_ERR_SLOT <<
				RT_IDX_IDX_SHIFT); 
			break;
		}
	case RT_IDX_BCAST:	
		{
			value = RT_IDX_DST_DFLT_Q |	
			    RT_IDX_TYPE_NICQ |	
			    (RT_IDX_BCAST_SLOT << RT_IDX_IDX_SHIFT);
			break;
		}
	case RT_IDX_MCAST:	
		{
			value = RT_IDX_DST_DFLT_Q |	
			    RT_IDX_TYPE_NICQ |	
			    (RT_IDX_ALLMULTI_SLOT << RT_IDX_IDX_SHIFT);
			break;
		}
	case RT_IDX_MCAST_MATCH:	
		{
			value = RT_IDX_DST_DFLT_Q |	
			    RT_IDX_TYPE_NICQ |	
			    (RT_IDX_MCAST_MATCH_SLOT << RT_IDX_IDX_SHIFT);
			break;
		}
	case RT_IDX_RSS_MATCH:	
		{
			value = RT_IDX_DST_RSS |	
			    RT_IDX_TYPE_NICQ |	
			    (RT_IDX_RSS_MATCH_SLOT << RT_IDX_IDX_SHIFT);
			break;
		}
	case 0:		
		{
			value = RT_IDX_DST_DFLT_Q |	
			    RT_IDX_TYPE_NICQ |	
			    (index << RT_IDX_IDX_SHIFT);
			break;
		}
	default:
		netif_err(qdev, ifup, qdev->ndev,
			  "Mask type %d not yet supported.\n", mask);
		status = -EPERM;
		goto exit;
	}

	if (value) {
		status = ql_wait_reg_rdy(qdev, RT_IDX, RT_IDX_MW, 0);
		if (status)
			goto exit;
		value |= (enable ? RT_IDX_E : 0);
		ql_write32(qdev, RT_IDX, value);
		ql_write32(qdev, RT_DATA, enable ? mask : 0);
	}
exit:
	return status;
}

static void ql_enable_interrupts(struct ql_adapter *qdev)
{
	ql_write32(qdev, INTR_EN, (INTR_EN_EI << 16) | INTR_EN_EI);
}

static void ql_disable_interrupts(struct ql_adapter *qdev)
{
	ql_write32(qdev, INTR_EN, (INTR_EN_EI << 16));
}

u32 ql_enable_completion_interrupt(struct ql_adapter *qdev, u32 intr)
{
	u32 var = 0;
	unsigned long hw_flags = 0;
	struct intr_context *ctx = qdev->intr_context + intr;

	if (likely(test_bit(QL_MSIX_ENABLED, &qdev->flags) && intr)) {
		ql_write32(qdev, INTR_EN,
			   ctx->intr_en_mask);
		var = ql_read32(qdev, STS);
		return var;
	}

	spin_lock_irqsave(&qdev->hw_lock, hw_flags);
	if (atomic_dec_and_test(&ctx->irq_cnt)) {
		ql_write32(qdev, INTR_EN,
			   ctx->intr_en_mask);
		var = ql_read32(qdev, STS);
	}
	spin_unlock_irqrestore(&qdev->hw_lock, hw_flags);
	return var;
}

static u32 ql_disable_completion_interrupt(struct ql_adapter *qdev, u32 intr)
{
	u32 var = 0;
	struct intr_context *ctx;

	if (likely(test_bit(QL_MSIX_ENABLED, &qdev->flags) && intr))
		return 0;

	ctx = qdev->intr_context + intr;
	spin_lock(&qdev->hw_lock);
	if (!atomic_read(&ctx->irq_cnt)) {
		ql_write32(qdev, INTR_EN,
		ctx->intr_dis_mask);
		var = ql_read32(qdev, STS);
	}
	atomic_inc(&ctx->irq_cnt);
	spin_unlock(&qdev->hw_lock);
	return var;
}

static void ql_enable_all_completion_interrupts(struct ql_adapter *qdev)
{
	int i;
	for (i = 0; i < qdev->intr_count; i++) {
		if (unlikely(!test_bit(QL_MSIX_ENABLED, &qdev->flags) ||
			i == 0))
			atomic_set(&qdev->intr_context[i].irq_cnt, 1);
		ql_enable_completion_interrupt(qdev, i);
	}

}

static int ql_validate_flash(struct ql_adapter *qdev, u32 size, const char *str)
{
	int status, i;
	u16 csum = 0;
	__le16 *flash = (__le16 *)&qdev->flash;

	status = strncmp((char *)&qdev->flash, str, 4);
	if (status) {
		netif_err(qdev, ifup, qdev->ndev, "Invalid flash signature.\n");
		return	status;
	}

	for (i = 0; i < size; i++)
		csum += le16_to_cpu(*flash++);

	if (csum)
		netif_err(qdev, ifup, qdev->ndev,
			  "Invalid flash checksum, csum = 0x%.04x.\n", csum);

	return csum;
}

static int ql_read_flash_word(struct ql_adapter *qdev, int offset, __le32 *data)
{
	int status = 0;
	
	status = ql_wait_reg_rdy(qdev,
			FLASH_ADDR, FLASH_ADDR_RDY, FLASH_ADDR_ERR);
	if (status)
		goto exit;
	
	ql_write32(qdev, FLASH_ADDR, FLASH_ADDR_R | offset);
	
	status = ql_wait_reg_rdy(qdev,
			FLASH_ADDR, FLASH_ADDR_RDY, FLASH_ADDR_ERR);
	if (status)
		goto exit;
	*data = cpu_to_le32(ql_read32(qdev, FLASH_DATA));
exit:
	return status;
}

static int ql_get_8000_flash_params(struct ql_adapter *qdev)
{
	u32 i, size;
	int status;
	__le32 *p = (__le32 *)&qdev->flash;
	u32 offset;
	u8 mac_addr[6];

	if (!qdev->port)
		offset = FUNC0_FLASH_OFFSET / sizeof(u32);
	else
		offset = FUNC1_FLASH_OFFSET / sizeof(u32);

	if (ql_sem_spinlock(qdev, SEM_FLASH_MASK))
		return -ETIMEDOUT;

	size = sizeof(struct flash_params_8000) / sizeof(u32);
	for (i = 0; i < size; i++, p++) {
		status = ql_read_flash_word(qdev, i+offset, p);
		if (status) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Error reading flash.\n");
			goto exit;
		}
	}

	status = ql_validate_flash(qdev,
			sizeof(struct flash_params_8000) / sizeof(u16),
			"8000");
	if (status) {
		netif_err(qdev, ifup, qdev->ndev, "Invalid flash.\n");
		status = -EINVAL;
		goto exit;
	}

	if (qdev->flash.flash_params_8000.data_type1 == 2)
		memcpy(mac_addr,
			qdev->flash.flash_params_8000.mac_addr1,
			qdev->ndev->addr_len);
	else
		memcpy(mac_addr,
			qdev->flash.flash_params_8000.mac_addr,
			qdev->ndev->addr_len);

	if (!is_valid_ether_addr(mac_addr)) {
		netif_err(qdev, ifup, qdev->ndev, "Invalid MAC address.\n");
		status = -EINVAL;
		goto exit;
	}

	memcpy(qdev->ndev->dev_addr,
		mac_addr,
		qdev->ndev->addr_len);

exit:
	ql_sem_unlock(qdev, SEM_FLASH_MASK);
	return status;
}

static int ql_get_8012_flash_params(struct ql_adapter *qdev)
{
	int i;
	int status;
	__le32 *p = (__le32 *)&qdev->flash;
	u32 offset = 0;
	u32 size = sizeof(struct flash_params_8012) / sizeof(u32);

	if (qdev->port)
		offset = size;

	if (ql_sem_spinlock(qdev, SEM_FLASH_MASK))
		return -ETIMEDOUT;

	for (i = 0; i < size; i++, p++) {
		status = ql_read_flash_word(qdev, i+offset, p);
		if (status) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Error reading flash.\n");
			goto exit;
		}

	}

	status = ql_validate_flash(qdev,
			sizeof(struct flash_params_8012) / sizeof(u16),
			"8012");
	if (status) {
		netif_err(qdev, ifup, qdev->ndev, "Invalid flash.\n");
		status = -EINVAL;
		goto exit;
	}

	if (!is_valid_ether_addr(qdev->flash.flash_params_8012.mac_addr)) {
		status = -EINVAL;
		goto exit;
	}

	memcpy(qdev->ndev->dev_addr,
		qdev->flash.flash_params_8012.mac_addr,
		qdev->ndev->addr_len);

exit:
	ql_sem_unlock(qdev, SEM_FLASH_MASK);
	return status;
}

static int ql_write_xgmac_reg(struct ql_adapter *qdev, u32 reg, u32 data)
{
	int status;
	
	status = ql_wait_reg_rdy(qdev,
			XGMAC_ADDR, XGMAC_ADDR_RDY, XGMAC_ADDR_XME);
	if (status)
		return status;
	
	ql_write32(qdev, XGMAC_DATA, data);
	
	ql_write32(qdev, XGMAC_ADDR, reg);
	return status;
}

int ql_read_xgmac_reg(struct ql_adapter *qdev, u32 reg, u32 *data)
{
	int status = 0;
	
	status = ql_wait_reg_rdy(qdev,
			XGMAC_ADDR, XGMAC_ADDR_RDY, XGMAC_ADDR_XME);
	if (status)
		goto exit;
	
	ql_write32(qdev, XGMAC_ADDR, reg | XGMAC_ADDR_R);
	
	status = ql_wait_reg_rdy(qdev,
			XGMAC_ADDR, XGMAC_ADDR_RDY, XGMAC_ADDR_XME);
	if (status)
		goto exit;
	
	*data = ql_read32(qdev, XGMAC_DATA);
exit:
	return status;
}

int ql_read_xgmac_reg64(struct ql_adapter *qdev, u32 reg, u64 *data)
{
	int status = 0;
	u32 hi = 0;
	u32 lo = 0;

	status = ql_read_xgmac_reg(qdev, reg, &lo);
	if (status)
		goto exit;

	status = ql_read_xgmac_reg(qdev, reg + 4, &hi);
	if (status)
		goto exit;

	*data = (u64) lo | ((u64) hi << 32);

exit:
	return status;
}

static int ql_8000_port_initialize(struct ql_adapter *qdev)
{
	int status;
	status = ql_mb_about_fw(qdev);
	if (status)
		goto exit;
	status = ql_mb_get_fw_state(qdev);
	if (status)
		goto exit;
	
	queue_delayed_work(qdev->workqueue, &qdev->mpi_port_cfg_work, 0);
exit:
	return status;
}

static int ql_8012_port_initialize(struct ql_adapter *qdev)
{
	int status = 0;
	u32 data;

	if (ql_sem_trylock(qdev, qdev->xg_sem_mask)) {
		netif_info(qdev, link, qdev->ndev,
			   "Another function has the semaphore, so wait for the port init bit to come ready.\n");
		status = ql_wait_reg_rdy(qdev, STS, qdev->port_init, 0);
		if (status) {
			netif_crit(qdev, link, qdev->ndev,
				   "Port initialize timed out.\n");
		}
		return status;
	}

	netif_info(qdev, link, qdev->ndev, "Got xgmac semaphore!.\n");
	
	status = ql_read_xgmac_reg(qdev, GLOBAL_CFG, &data);
	if (status)
		goto end;
	data |= GLOBAL_CFG_RESET;
	status = ql_write_xgmac_reg(qdev, GLOBAL_CFG, data);
	if (status)
		goto end;

	
	data &= ~GLOBAL_CFG_RESET;	
	data |= GLOBAL_CFG_JUMBO;	
	data |= GLOBAL_CFG_TX_STAT_EN;
	data |= GLOBAL_CFG_RX_STAT_EN;
	status = ql_write_xgmac_reg(qdev, GLOBAL_CFG, data);
	if (status)
		goto end;

	
	status = ql_read_xgmac_reg(qdev, TX_CFG, &data);
	if (status)
		goto end;
	data &= ~TX_CFG_RESET;	
	data |= TX_CFG_EN;	
	status = ql_write_xgmac_reg(qdev, TX_CFG, data);
	if (status)
		goto end;

	
	status = ql_read_xgmac_reg(qdev, RX_CFG, &data);
	if (status)
		goto end;
	data &= ~RX_CFG_RESET;	
	data |= RX_CFG_EN;	
	status = ql_write_xgmac_reg(qdev, RX_CFG, data);
	if (status)
		goto end;

	
	status =
	    ql_write_xgmac_reg(qdev, MAC_TX_PARAMS, MAC_TX_PARAMS_JUMBO | (0x2580 << 16));
	if (status)
		goto end;
	status =
	    ql_write_xgmac_reg(qdev, MAC_RX_PARAMS, 0x2580);
	if (status)
		goto end;

	
	ql_write32(qdev, STS, ((qdev->port_init << 16) | qdev->port_init));
end:
	ql_sem_unlock(qdev, qdev->xg_sem_mask);
	return status;
}

static inline unsigned int ql_lbq_block_size(struct ql_adapter *qdev)
{
	return PAGE_SIZE << qdev->lbq_buf_order;
}

static struct bq_desc *ql_get_curr_lbuf(struct rx_ring *rx_ring)
{
	struct bq_desc *lbq_desc = &rx_ring->lbq[rx_ring->lbq_curr_idx];
	rx_ring->lbq_curr_idx++;
	if (rx_ring->lbq_curr_idx == rx_ring->lbq_len)
		rx_ring->lbq_curr_idx = 0;
	rx_ring->lbq_free_cnt++;
	return lbq_desc;
}

static struct bq_desc *ql_get_curr_lchunk(struct ql_adapter *qdev,
		struct rx_ring *rx_ring)
{
	struct bq_desc *lbq_desc = ql_get_curr_lbuf(rx_ring);

	pci_dma_sync_single_for_cpu(qdev->pdev,
					dma_unmap_addr(lbq_desc, mapaddr),
				    rx_ring->lbq_buf_size,
					PCI_DMA_FROMDEVICE);

	if ((lbq_desc->p.pg_chunk.offset + rx_ring->lbq_buf_size)
					== ql_lbq_block_size(qdev))
		pci_unmap_page(qdev->pdev,
				lbq_desc->p.pg_chunk.map,
				ql_lbq_block_size(qdev),
				PCI_DMA_FROMDEVICE);
	return lbq_desc;
}

static struct bq_desc *ql_get_curr_sbuf(struct rx_ring *rx_ring)
{
	struct bq_desc *sbq_desc = &rx_ring->sbq[rx_ring->sbq_curr_idx];
	rx_ring->sbq_curr_idx++;
	if (rx_ring->sbq_curr_idx == rx_ring->sbq_len)
		rx_ring->sbq_curr_idx = 0;
	rx_ring->sbq_free_cnt++;
	return sbq_desc;
}

static void ql_update_cq(struct rx_ring *rx_ring)
{
	rx_ring->cnsmr_idx++;
	rx_ring->curr_entry++;
	if (unlikely(rx_ring->cnsmr_idx == rx_ring->cq_len)) {
		rx_ring->cnsmr_idx = 0;
		rx_ring->curr_entry = rx_ring->cq_base;
	}
}

static void ql_write_cq_idx(struct rx_ring *rx_ring)
{
	ql_write_db_reg(rx_ring->cnsmr_idx, rx_ring->cnsmr_idx_db_reg);
}

static int ql_get_next_chunk(struct ql_adapter *qdev, struct rx_ring *rx_ring,
						struct bq_desc *lbq_desc)
{
	if (!rx_ring->pg_chunk.page) {
		u64 map;
		rx_ring->pg_chunk.page = alloc_pages(__GFP_COLD | __GFP_COMP |
						GFP_ATOMIC,
						qdev->lbq_buf_order);
		if (unlikely(!rx_ring->pg_chunk.page)) {
			netif_err(qdev, drv, qdev->ndev,
				  "page allocation failed.\n");
			return -ENOMEM;
		}
		rx_ring->pg_chunk.offset = 0;
		map = pci_map_page(qdev->pdev, rx_ring->pg_chunk.page,
					0, ql_lbq_block_size(qdev),
					PCI_DMA_FROMDEVICE);
		if (pci_dma_mapping_error(qdev->pdev, map)) {
			__free_pages(rx_ring->pg_chunk.page,
					qdev->lbq_buf_order);
			netif_err(qdev, drv, qdev->ndev,
				  "PCI mapping failed.\n");
			return -ENOMEM;
		}
		rx_ring->pg_chunk.map = map;
		rx_ring->pg_chunk.va = page_address(rx_ring->pg_chunk.page);
	}

	lbq_desc->p.pg_chunk = rx_ring->pg_chunk;

	rx_ring->pg_chunk.offset += rx_ring->lbq_buf_size;
	if (rx_ring->pg_chunk.offset == ql_lbq_block_size(qdev)) {
		rx_ring->pg_chunk.page = NULL;
		lbq_desc->p.pg_chunk.last_flag = 1;
	} else {
		rx_ring->pg_chunk.va += rx_ring->lbq_buf_size;
		get_page(rx_ring->pg_chunk.page);
		lbq_desc->p.pg_chunk.last_flag = 0;
	}
	return 0;
}
static void ql_update_lbq(struct ql_adapter *qdev, struct rx_ring *rx_ring)
{
	u32 clean_idx = rx_ring->lbq_clean_idx;
	u32 start_idx = clean_idx;
	struct bq_desc *lbq_desc;
	u64 map;
	int i;

	while (rx_ring->lbq_free_cnt > 32) {
		for (i = (rx_ring->lbq_clean_idx % 16); i < 16; i++) {
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "lbq: try cleaning clean_idx = %d.\n",
				     clean_idx);
			lbq_desc = &rx_ring->lbq[clean_idx];
			if (ql_get_next_chunk(qdev, rx_ring, lbq_desc)) {
				rx_ring->lbq_clean_idx = clean_idx;
				netif_err(qdev, ifup, qdev->ndev,
						"Could not get a page chunk, i=%d, clean_idx =%d .\n",
						i, clean_idx);
				return;
			}

			map = lbq_desc->p.pg_chunk.map +
				lbq_desc->p.pg_chunk.offset;
				dma_unmap_addr_set(lbq_desc, mapaddr, map);
			dma_unmap_len_set(lbq_desc, maplen,
					rx_ring->lbq_buf_size);
				*lbq_desc->addr = cpu_to_le64(map);

			pci_dma_sync_single_for_device(qdev->pdev, map,
						rx_ring->lbq_buf_size,
						PCI_DMA_FROMDEVICE);
			clean_idx++;
			if (clean_idx == rx_ring->lbq_len)
				clean_idx = 0;
		}

		rx_ring->lbq_clean_idx = clean_idx;
		rx_ring->lbq_prod_idx += 16;
		if (rx_ring->lbq_prod_idx == rx_ring->lbq_len)
			rx_ring->lbq_prod_idx = 0;
		rx_ring->lbq_free_cnt -= 16;
	}

	if (start_idx != clean_idx) {
		netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
			     "lbq: updating prod idx = %d.\n",
			     rx_ring->lbq_prod_idx);
		ql_write_db_reg(rx_ring->lbq_prod_idx,
				rx_ring->lbq_prod_idx_db_reg);
	}
}

static void ql_update_sbq(struct ql_adapter *qdev, struct rx_ring *rx_ring)
{
	u32 clean_idx = rx_ring->sbq_clean_idx;
	u32 start_idx = clean_idx;
	struct bq_desc *sbq_desc;
	u64 map;
	int i;

	while (rx_ring->sbq_free_cnt > 16) {
		for (i = (rx_ring->sbq_clean_idx % 16); i < 16; i++) {
			sbq_desc = &rx_ring->sbq[clean_idx];
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "sbq: try cleaning clean_idx = %d.\n",
				     clean_idx);
			if (sbq_desc->p.skb == NULL) {
				netif_printk(qdev, rx_status, KERN_DEBUG,
					     qdev->ndev,
					     "sbq: getting new skb for index %d.\n",
					     sbq_desc->index);
				sbq_desc->p.skb =
				    netdev_alloc_skb(qdev->ndev,
						     SMALL_BUFFER_SIZE);
				if (sbq_desc->p.skb == NULL) {
					netif_err(qdev, probe, qdev->ndev,
						  "Couldn't get an skb.\n");
					rx_ring->sbq_clean_idx = clean_idx;
					return;
				}
				skb_reserve(sbq_desc->p.skb, QLGE_SB_PAD);
				map = pci_map_single(qdev->pdev,
						     sbq_desc->p.skb->data,
						     rx_ring->sbq_buf_size,
						     PCI_DMA_FROMDEVICE);
				if (pci_dma_mapping_error(qdev->pdev, map)) {
					netif_err(qdev, ifup, qdev->ndev,
						  "PCI mapping failed.\n");
					rx_ring->sbq_clean_idx = clean_idx;
					dev_kfree_skb_any(sbq_desc->p.skb);
					sbq_desc->p.skb = NULL;
					return;
				}
				dma_unmap_addr_set(sbq_desc, mapaddr, map);
				dma_unmap_len_set(sbq_desc, maplen,
						  rx_ring->sbq_buf_size);
				*sbq_desc->addr = cpu_to_le64(map);
			}

			clean_idx++;
			if (clean_idx == rx_ring->sbq_len)
				clean_idx = 0;
		}
		rx_ring->sbq_clean_idx = clean_idx;
		rx_ring->sbq_prod_idx += 16;
		if (rx_ring->sbq_prod_idx == rx_ring->sbq_len)
			rx_ring->sbq_prod_idx = 0;
		rx_ring->sbq_free_cnt -= 16;
	}

	if (start_idx != clean_idx) {
		netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
			     "sbq: updating prod idx = %d.\n",
			     rx_ring->sbq_prod_idx);
		ql_write_db_reg(rx_ring->sbq_prod_idx,
				rx_ring->sbq_prod_idx_db_reg);
	}
}

static void ql_update_buffer_queues(struct ql_adapter *qdev,
				    struct rx_ring *rx_ring)
{
	ql_update_sbq(qdev, rx_ring);
	ql_update_lbq(qdev, rx_ring);
}

static void ql_unmap_send(struct ql_adapter *qdev,
			  struct tx_ring_desc *tx_ring_desc, int mapped)
{
	int i;
	for (i = 0; i < mapped; i++) {
		if (i == 0 || (i == 7 && mapped > 7)) {
			if (i == 7) {
				netif_printk(qdev, tx_done, KERN_DEBUG,
					     qdev->ndev,
					     "unmapping OAL area.\n");
			}
			pci_unmap_single(qdev->pdev,
					 dma_unmap_addr(&tx_ring_desc->map[i],
							mapaddr),
					 dma_unmap_len(&tx_ring_desc->map[i],
						       maplen),
					 PCI_DMA_TODEVICE);
		} else {
			netif_printk(qdev, tx_done, KERN_DEBUG, qdev->ndev,
				     "unmapping frag %d.\n", i);
			pci_unmap_page(qdev->pdev,
				       dma_unmap_addr(&tx_ring_desc->map[i],
						      mapaddr),
				       dma_unmap_len(&tx_ring_desc->map[i],
						     maplen), PCI_DMA_TODEVICE);
		}
	}

}

static int ql_map_send(struct ql_adapter *qdev,
		       struct ob_mac_iocb_req *mac_iocb_ptr,
		       struct sk_buff *skb, struct tx_ring_desc *tx_ring_desc)
{
	int len = skb_headlen(skb);
	dma_addr_t map;
	int frag_idx, err, map_idx = 0;
	struct tx_buf_desc *tbd = mac_iocb_ptr->tbd;
	int frag_cnt = skb_shinfo(skb)->nr_frags;

	if (frag_cnt) {
		netif_printk(qdev, tx_queued, KERN_DEBUG, qdev->ndev,
			     "frag_cnt = %d.\n", frag_cnt);
	}
	map = pci_map_single(qdev->pdev, skb->data, len, PCI_DMA_TODEVICE);

	err = pci_dma_mapping_error(qdev->pdev, map);
	if (err) {
		netif_err(qdev, tx_queued, qdev->ndev,
			  "PCI mapping failed with error: %d\n", err);

		return NETDEV_TX_BUSY;
	}

	tbd->len = cpu_to_le32(len);
	tbd->addr = cpu_to_le64(map);
	dma_unmap_addr_set(&tx_ring_desc->map[map_idx], mapaddr, map);
	dma_unmap_len_set(&tx_ring_desc->map[map_idx], maplen, len);
	map_idx++;

	for (frag_idx = 0; frag_idx < frag_cnt; frag_idx++, map_idx++) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[frag_idx];
		tbd++;
		if (frag_idx == 6 && frag_cnt > 7) {
			
			map = pci_map_single(qdev->pdev, &tx_ring_desc->oal,
					     sizeof(struct oal),
					     PCI_DMA_TODEVICE);
			err = pci_dma_mapping_error(qdev->pdev, map);
			if (err) {
				netif_err(qdev, tx_queued, qdev->ndev,
					  "PCI mapping outbound address list with error: %d\n",
					  err);
				goto map_error;
			}

			tbd->addr = cpu_to_le64(map);
			tbd->len =
			    cpu_to_le32((sizeof(struct tx_buf_desc) *
					 (frag_cnt - frag_idx)) | TX_DESC_C);
			dma_unmap_addr_set(&tx_ring_desc->map[map_idx], mapaddr,
					   map);
			dma_unmap_len_set(&tx_ring_desc->map[map_idx], maplen,
					  sizeof(struct oal));
			tbd = (struct tx_buf_desc *)&tx_ring_desc->oal;
			map_idx++;
		}

		map = skb_frag_dma_map(&qdev->pdev->dev, frag, 0, skb_frag_size(frag),
				       DMA_TO_DEVICE);

		err = dma_mapping_error(&qdev->pdev->dev, map);
		if (err) {
			netif_err(qdev, tx_queued, qdev->ndev,
				  "PCI mapping frags failed with error: %d.\n",
				  err);
			goto map_error;
		}

		tbd->addr = cpu_to_le64(map);
		tbd->len = cpu_to_le32(skb_frag_size(frag));
		dma_unmap_addr_set(&tx_ring_desc->map[map_idx], mapaddr, map);
		dma_unmap_len_set(&tx_ring_desc->map[map_idx], maplen,
				  skb_frag_size(frag));

	}
	
	tx_ring_desc->map_cnt = map_idx;
	
	tbd->len = cpu_to_le32(le32_to_cpu(tbd->len) | TX_DESC_E);
	return NETDEV_TX_OK;

map_error:
	ql_unmap_send(qdev, tx_ring_desc, map_idx);
	return NETDEV_TX_BUSY;
}

static void ql_process_mac_rx_gro_page(struct ql_adapter *qdev,
					struct rx_ring *rx_ring,
					struct ib_mac_iocb_rsp *ib_mac_rsp,
					u32 length,
					u16 vlan_id)
{
	struct sk_buff *skb;
	struct bq_desc *lbq_desc = ql_get_curr_lchunk(qdev, rx_ring);
	struct napi_struct *napi = &rx_ring->napi;

	napi->dev = qdev->ndev;

	skb = napi_get_frags(napi);
	if (!skb) {
		netif_err(qdev, drv, qdev->ndev,
			  "Couldn't get an skb, exiting.\n");
		rx_ring->rx_dropped++;
		put_page(lbq_desc->p.pg_chunk.page);
		return;
	}
	prefetch(lbq_desc->p.pg_chunk.va);
	__skb_fill_page_desc(skb, skb_shinfo(skb)->nr_frags,
			     lbq_desc->p.pg_chunk.page,
			     lbq_desc->p.pg_chunk.offset,
			     length);

	skb->len += length;
	skb->data_len += length;
	skb->truesize += length;
	skb_shinfo(skb)->nr_frags++;

	rx_ring->rx_packets++;
	rx_ring->rx_bytes += length;
	skb->ip_summed = CHECKSUM_UNNECESSARY;
	skb_record_rx_queue(skb, rx_ring->cq_id);
	if (vlan_id != 0xffff)
		__vlan_hwaccel_put_tag(skb, vlan_id);
	napi_gro_frags(napi);
}

static void ql_process_mac_rx_page(struct ql_adapter *qdev,
					struct rx_ring *rx_ring,
					struct ib_mac_iocb_rsp *ib_mac_rsp,
					u32 length,
					u16 vlan_id)
{
	struct net_device *ndev = qdev->ndev;
	struct sk_buff *skb = NULL;
	void *addr;
	struct bq_desc *lbq_desc = ql_get_curr_lchunk(qdev, rx_ring);
	struct napi_struct *napi = &rx_ring->napi;

	skb = netdev_alloc_skb(ndev, length);
	if (!skb) {
		netif_err(qdev, drv, qdev->ndev,
			  "Couldn't get an skb, need to unwind!.\n");
		rx_ring->rx_dropped++;
		put_page(lbq_desc->p.pg_chunk.page);
		return;
	}

	addr = lbq_desc->p.pg_chunk.va;
	prefetch(addr);


	
	if (ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_ERR_MASK) {
		netif_info(qdev, drv, qdev->ndev,
			  "Receive error, flags2 = 0x%x\n", ib_mac_rsp->flags2);
		rx_ring->rx_errors++;
		goto err_out;
	}

	if (skb->len > ndev->mtu + ETH_HLEN) {
		netif_err(qdev, drv, qdev->ndev,
			  "Segment too small, dropping.\n");
		rx_ring->rx_dropped++;
		goto err_out;
	}
	memcpy(skb_put(skb, ETH_HLEN), addr, ETH_HLEN);
	netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
		     "%d bytes of headers and data in large. Chain page to new skb and pull tail.\n",
		     length);
	skb_fill_page_desc(skb, 0, lbq_desc->p.pg_chunk.page,
				lbq_desc->p.pg_chunk.offset+ETH_HLEN,
				length-ETH_HLEN);
	skb->len += length-ETH_HLEN;
	skb->data_len += length-ETH_HLEN;
	skb->truesize += length-ETH_HLEN;

	rx_ring->rx_packets++;
	rx_ring->rx_bytes += skb->len;
	skb->protocol = eth_type_trans(skb, ndev);
	skb_checksum_none_assert(skb);

	if ((ndev->features & NETIF_F_RXCSUM) &&
		!(ib_mac_rsp->flags1 & IB_MAC_CSUM_ERR_MASK)) {
		
		if (ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_T) {
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "TCP checksum done!\n");
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		} else if ((ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_U) &&
				(ib_mac_rsp->flags3 & IB_MAC_IOCB_RSP_V4)) {
			
			struct iphdr *iph =
				(struct iphdr *) ((u8 *)addr + ETH_HLEN);
			if (!(iph->frag_off &
				cpu_to_be16(IP_MF|IP_OFFSET))) {
				skb->ip_summed = CHECKSUM_UNNECESSARY;
				netif_printk(qdev, rx_status, KERN_DEBUG,
					     qdev->ndev,
					     "UDP checksum done!\n");
			}
		}
	}

	skb_record_rx_queue(skb, rx_ring->cq_id);
	if (vlan_id != 0xffff)
		__vlan_hwaccel_put_tag(skb, vlan_id);
	if (skb->ip_summed == CHECKSUM_UNNECESSARY)
		napi_gro_receive(napi, skb);
	else
		netif_receive_skb(skb);
	return;
err_out:
	dev_kfree_skb_any(skb);
	put_page(lbq_desc->p.pg_chunk.page);
}

static void ql_process_mac_rx_skb(struct ql_adapter *qdev,
					struct rx_ring *rx_ring,
					struct ib_mac_iocb_rsp *ib_mac_rsp,
					u32 length,
					u16 vlan_id)
{
	struct net_device *ndev = qdev->ndev;
	struct sk_buff *skb = NULL;
	struct sk_buff *new_skb = NULL;
	struct bq_desc *sbq_desc = ql_get_curr_sbuf(rx_ring);

	skb = sbq_desc->p.skb;
	
	new_skb = netdev_alloc_skb(qdev->ndev, length + NET_IP_ALIGN);
	if (new_skb == NULL) {
		netif_err(qdev, probe, qdev->ndev,
			  "No skb available, drop the packet.\n");
		rx_ring->rx_dropped++;
		return;
	}
	skb_reserve(new_skb, NET_IP_ALIGN);
	memcpy(skb_put(new_skb, length), skb->data, length);
	skb = new_skb;

	
	if (ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_ERR_MASK) {
		netif_info(qdev, drv, qdev->ndev,
			  "Receive error, flags2 = 0x%x\n", ib_mac_rsp->flags2);
		dev_kfree_skb_any(skb);
		rx_ring->rx_errors++;
		return;
	}

	
	if (test_bit(QL_SELFTEST, &qdev->flags)) {
		ql_check_lb_frame(qdev, skb);
		dev_kfree_skb_any(skb);
		return;
	}

	if (skb->len > ndev->mtu + ETH_HLEN) {
		dev_kfree_skb_any(skb);
		rx_ring->rx_dropped++;
		return;
	}

	prefetch(skb->data);
	skb->dev = ndev;
	if (ib_mac_rsp->flags1 & IB_MAC_IOCB_RSP_M_MASK) {
		netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
			     "%s Multicast.\n",
			     (ib_mac_rsp->flags1 & IB_MAC_IOCB_RSP_M_MASK) ==
			     IB_MAC_IOCB_RSP_M_HASH ? "Hash" :
			     (ib_mac_rsp->flags1 & IB_MAC_IOCB_RSP_M_MASK) ==
			     IB_MAC_IOCB_RSP_M_REG ? "Registered" :
			     (ib_mac_rsp->flags1 & IB_MAC_IOCB_RSP_M_MASK) ==
			     IB_MAC_IOCB_RSP_M_PROM ? "Promiscuous" : "");
	}
	if (ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_P)
		netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
			     "Promiscuous Packet.\n");

	rx_ring->rx_packets++;
	rx_ring->rx_bytes += skb->len;
	skb->protocol = eth_type_trans(skb, ndev);
	skb_checksum_none_assert(skb);

	if ((ndev->features & NETIF_F_RXCSUM) &&
		!(ib_mac_rsp->flags1 & IB_MAC_CSUM_ERR_MASK)) {
		
		if (ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_T) {
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "TCP checksum done!\n");
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		} else if ((ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_U) &&
				(ib_mac_rsp->flags3 & IB_MAC_IOCB_RSP_V4)) {
			
			struct iphdr *iph = (struct iphdr *) skb->data;
			if (!(iph->frag_off &
				ntohs(IP_MF|IP_OFFSET))) {
				skb->ip_summed = CHECKSUM_UNNECESSARY;
				netif_printk(qdev, rx_status, KERN_DEBUG,
					     qdev->ndev,
					     "UDP checksum done!\n");
			}
		}
	}

	skb_record_rx_queue(skb, rx_ring->cq_id);
	if (vlan_id != 0xffff)
		__vlan_hwaccel_put_tag(skb, vlan_id);
	if (skb->ip_summed == CHECKSUM_UNNECESSARY)
		napi_gro_receive(&rx_ring->napi, skb);
	else
		netif_receive_skb(skb);
}

static void ql_realign_skb(struct sk_buff *skb, int len)
{
	void *temp_addr = skb->data;

	skb->data -= QLGE_SB_PAD - NET_IP_ALIGN;
	skb->tail -= QLGE_SB_PAD - NET_IP_ALIGN;
	skb_copy_to_linear_data(skb, temp_addr,
		(unsigned int)len);
}

/*
 * This function builds an skb for the given inbound
 * completion.  It will be rewritten for readability in the near
 * future, but for not it works well.
 */
static struct sk_buff *ql_build_rx_skb(struct ql_adapter *qdev,
				       struct rx_ring *rx_ring,
				       struct ib_mac_iocb_rsp *ib_mac_rsp)
{
	struct bq_desc *lbq_desc;
	struct bq_desc *sbq_desc;
	struct sk_buff *skb = NULL;
	u32 length = le32_to_cpu(ib_mac_rsp->data_len);
       u32 hdr_len = le32_to_cpu(ib_mac_rsp->hdr_len);

	if (ib_mac_rsp->flags4 & IB_MAC_IOCB_RSP_HV &&
	    ib_mac_rsp->flags4 & IB_MAC_IOCB_RSP_HS) {
		netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
			     "Header of %d bytes in small buffer.\n", hdr_len);
		sbq_desc = ql_get_curr_sbuf(rx_ring);
		pci_unmap_single(qdev->pdev,
				dma_unmap_addr(sbq_desc, mapaddr),
				dma_unmap_len(sbq_desc, maplen),
				PCI_DMA_FROMDEVICE);
		skb = sbq_desc->p.skb;
		ql_realign_skb(skb, hdr_len);
		skb_put(skb, hdr_len);
		sbq_desc->p.skb = NULL;
	}

	if (unlikely(!length)) {	
		netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
			     "No Data buffer in this packet.\n");
		return skb;
	}

	if (ib_mac_rsp->flags3 & IB_MAC_IOCB_RSP_DS) {
		if (ib_mac_rsp->flags4 & IB_MAC_IOCB_RSP_HS) {
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "Headers in small, data of %d bytes in small, combine them.\n",
				     length);
			sbq_desc = ql_get_curr_sbuf(rx_ring);
			pci_dma_sync_single_for_cpu(qdev->pdev,
						    dma_unmap_addr
						    (sbq_desc, mapaddr),
						    dma_unmap_len
						    (sbq_desc, maplen),
						    PCI_DMA_FROMDEVICE);
			memcpy(skb_put(skb, length),
			       sbq_desc->p.skb->data, length);
			pci_dma_sync_single_for_device(qdev->pdev,
						       dma_unmap_addr
						       (sbq_desc,
							mapaddr),
						       dma_unmap_len
						       (sbq_desc,
							maplen),
						       PCI_DMA_FROMDEVICE);
		} else {
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "%d bytes in a single small buffer.\n",
				     length);
			sbq_desc = ql_get_curr_sbuf(rx_ring);
			skb = sbq_desc->p.skb;
			ql_realign_skb(skb, length);
			skb_put(skb, length);
			pci_unmap_single(qdev->pdev,
					 dma_unmap_addr(sbq_desc,
							mapaddr),
					 dma_unmap_len(sbq_desc,
						       maplen),
					 PCI_DMA_FROMDEVICE);
			sbq_desc->p.skb = NULL;
		}
	} else if (ib_mac_rsp->flags3 & IB_MAC_IOCB_RSP_DL) {
		if (ib_mac_rsp->flags4 & IB_MAC_IOCB_RSP_HS) {
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "Header in small, %d bytes in large. Chain large to small!\n",
				     length);
			lbq_desc = ql_get_curr_lchunk(qdev, rx_ring);
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "Chaining page at offset = %d, for %d bytes  to skb.\n",
				     lbq_desc->p.pg_chunk.offset, length);
			skb_fill_page_desc(skb, 0, lbq_desc->p.pg_chunk.page,
						lbq_desc->p.pg_chunk.offset,
						length);
			skb->len += length;
			skb->data_len += length;
			skb->truesize += length;
		} else {
			lbq_desc = ql_get_curr_lchunk(qdev, rx_ring);
			skb = netdev_alloc_skb(qdev->ndev, length);
			if (skb == NULL) {
				netif_printk(qdev, probe, KERN_DEBUG, qdev->ndev,
					     "No skb available, drop the packet.\n");
				return NULL;
			}
			pci_unmap_page(qdev->pdev,
				       dma_unmap_addr(lbq_desc,
						      mapaddr),
				       dma_unmap_len(lbq_desc, maplen),
				       PCI_DMA_FROMDEVICE);
			skb_reserve(skb, NET_IP_ALIGN);
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "%d bytes of headers and data in large. Chain page to new skb and pull tail.\n",
				     length);
			skb_fill_page_desc(skb, 0,
						lbq_desc->p.pg_chunk.page,
						lbq_desc->p.pg_chunk.offset,
						length);
			skb->len += length;
			skb->data_len += length;
			skb->truesize += length;
			length -= length;
			__pskb_pull_tail(skb,
				(ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_V) ?
				VLAN_ETH_HLEN : ETH_HLEN);
		}
	} else {
		int size, i = 0;
		sbq_desc = ql_get_curr_sbuf(rx_ring);
		pci_unmap_single(qdev->pdev,
				 dma_unmap_addr(sbq_desc, mapaddr),
				 dma_unmap_len(sbq_desc, maplen),
				 PCI_DMA_FROMDEVICE);
		if (!(ib_mac_rsp->flags4 & IB_MAC_IOCB_RSP_HS)) {
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "%d bytes of headers & data in chain of large.\n",
				     length);
			skb = sbq_desc->p.skb;
			sbq_desc->p.skb = NULL;
			skb_reserve(skb, NET_IP_ALIGN);
		}
		while (length > 0) {
			lbq_desc = ql_get_curr_lchunk(qdev, rx_ring);
			size = (length < rx_ring->lbq_buf_size) ? length :
				rx_ring->lbq_buf_size;

			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "Adding page %d to skb for %d bytes.\n",
				     i, size);
			skb_fill_page_desc(skb, i,
						lbq_desc->p.pg_chunk.page,
						lbq_desc->p.pg_chunk.offset,
						size);
			skb->len += size;
			skb->data_len += size;
			skb->truesize += size;
			length -= size;
			i++;
		}
		__pskb_pull_tail(skb, (ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_V) ?
				VLAN_ETH_HLEN : ETH_HLEN);
	}
	return skb;
}

static void ql_process_mac_split_rx_intr(struct ql_adapter *qdev,
				   struct rx_ring *rx_ring,
				   struct ib_mac_iocb_rsp *ib_mac_rsp,
				   u16 vlan_id)
{
	struct net_device *ndev = qdev->ndev;
	struct sk_buff *skb = NULL;

	QL_DUMP_IB_MAC_RSP(ib_mac_rsp);

	skb = ql_build_rx_skb(qdev, rx_ring, ib_mac_rsp);
	if (unlikely(!skb)) {
		netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
			     "No skb available, drop packet.\n");
		rx_ring->rx_dropped++;
		return;
	}

	
	if (ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_ERR_MASK) {
		netif_info(qdev, drv, qdev->ndev,
			  "Receive error, flags2 = 0x%x\n", ib_mac_rsp->flags2);
		dev_kfree_skb_any(skb);
		rx_ring->rx_errors++;
		return;
	}

	if (skb->len > ndev->mtu + ETH_HLEN) {
		dev_kfree_skb_any(skb);
		rx_ring->rx_dropped++;
		return;
	}

	
	if (test_bit(QL_SELFTEST, &qdev->flags)) {
		ql_check_lb_frame(qdev, skb);
		dev_kfree_skb_any(skb);
		return;
	}

	prefetch(skb->data);
	skb->dev = ndev;
	if (ib_mac_rsp->flags1 & IB_MAC_IOCB_RSP_M_MASK) {
		netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev, "%s Multicast.\n",
			     (ib_mac_rsp->flags1 & IB_MAC_IOCB_RSP_M_MASK) ==
			     IB_MAC_IOCB_RSP_M_HASH ? "Hash" :
			     (ib_mac_rsp->flags1 & IB_MAC_IOCB_RSP_M_MASK) ==
			     IB_MAC_IOCB_RSP_M_REG ? "Registered" :
			     (ib_mac_rsp->flags1 & IB_MAC_IOCB_RSP_M_MASK) ==
			     IB_MAC_IOCB_RSP_M_PROM ? "Promiscuous" : "");
		rx_ring->rx_multicast++;
	}
	if (ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_P) {
		netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
			     "Promiscuous Packet.\n");
	}

	skb->protocol = eth_type_trans(skb, ndev);
	skb_checksum_none_assert(skb);

	if ((ndev->features & NETIF_F_RXCSUM) &&
		!(ib_mac_rsp->flags1 & IB_MAC_CSUM_ERR_MASK)) {
		
		if (ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_T) {
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "TCP checksum done!\n");
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		} else if ((ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_U) &&
				(ib_mac_rsp->flags3 & IB_MAC_IOCB_RSP_V4)) {
		
			struct iphdr *iph = (struct iphdr *) skb->data;
			if (!(iph->frag_off &
				ntohs(IP_MF|IP_OFFSET))) {
				skb->ip_summed = CHECKSUM_UNNECESSARY;
				netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
					     "TCP checksum done!\n");
			}
		}
	}

	rx_ring->rx_packets++;
	rx_ring->rx_bytes += skb->len;
	skb_record_rx_queue(skb, rx_ring->cq_id);
	if ((ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_V) && (vlan_id != 0))
		__vlan_hwaccel_put_tag(skb, vlan_id);
	if (skb->ip_summed == CHECKSUM_UNNECESSARY)
		napi_gro_receive(&rx_ring->napi, skb);
	else
		netif_receive_skb(skb);
}

static unsigned long ql_process_mac_rx_intr(struct ql_adapter *qdev,
					struct rx_ring *rx_ring,
					struct ib_mac_iocb_rsp *ib_mac_rsp)
{
	u32 length = le32_to_cpu(ib_mac_rsp->data_len);
	u16 vlan_id = (ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_V) ?
			((le16_to_cpu(ib_mac_rsp->vlan_id) &
			IB_MAC_IOCB_RSP_VLAN_MASK)) : 0xffff;

	QL_DUMP_IB_MAC_RSP(ib_mac_rsp);

	if (ib_mac_rsp->flags4 & IB_MAC_IOCB_RSP_HV) {
		ql_process_mac_split_rx_intr(qdev, rx_ring, ib_mac_rsp,
						vlan_id);
	} else if (ib_mac_rsp->flags3 & IB_MAC_IOCB_RSP_DS) {
		ql_process_mac_rx_skb(qdev, rx_ring, ib_mac_rsp,
						length, vlan_id);
	} else if ((ib_mac_rsp->flags3 & IB_MAC_IOCB_RSP_DL) &&
		!(ib_mac_rsp->flags1 & IB_MAC_CSUM_ERR_MASK) &&
		(ib_mac_rsp->flags2 & IB_MAC_IOCB_RSP_T)) {
		ql_process_mac_rx_gro_page(qdev, rx_ring, ib_mac_rsp,
						length, vlan_id);
	} else if (ib_mac_rsp->flags3 & IB_MAC_IOCB_RSP_DL) {
		ql_process_mac_rx_page(qdev, rx_ring, ib_mac_rsp,
						length, vlan_id);
	} else {
		ql_process_mac_split_rx_intr(qdev, rx_ring, ib_mac_rsp,
						vlan_id);
	}

	return (unsigned long)length;
}

static void ql_process_mac_tx_intr(struct ql_adapter *qdev,
				   struct ob_mac_iocb_rsp *mac_rsp)
{
	struct tx_ring *tx_ring;
	struct tx_ring_desc *tx_ring_desc;

	QL_DUMP_OB_MAC_RSP(mac_rsp);
	tx_ring = &qdev->tx_ring[mac_rsp->txq_idx];
	tx_ring_desc = &tx_ring->q[mac_rsp->tid];
	ql_unmap_send(qdev, tx_ring_desc, tx_ring_desc->map_cnt);
	tx_ring->tx_bytes += (tx_ring_desc->skb)->len;
	tx_ring->tx_packets++;
	dev_kfree_skb(tx_ring_desc->skb);
	tx_ring_desc->skb = NULL;

	if (unlikely(mac_rsp->flags1 & (OB_MAC_IOCB_RSP_E |
					OB_MAC_IOCB_RSP_S |
					OB_MAC_IOCB_RSP_L |
					OB_MAC_IOCB_RSP_P | OB_MAC_IOCB_RSP_B))) {
		if (mac_rsp->flags1 & OB_MAC_IOCB_RSP_E) {
			netif_warn(qdev, tx_done, qdev->ndev,
				   "Total descriptor length did not match transfer length.\n");
		}
		if (mac_rsp->flags1 & OB_MAC_IOCB_RSP_S) {
			netif_warn(qdev, tx_done, qdev->ndev,
				   "Frame too short to be valid, not sent.\n");
		}
		if (mac_rsp->flags1 & OB_MAC_IOCB_RSP_L) {
			netif_warn(qdev, tx_done, qdev->ndev,
				   "Frame too long, but sent anyway.\n");
		}
		if (mac_rsp->flags1 & OB_MAC_IOCB_RSP_B) {
			netif_warn(qdev, tx_done, qdev->ndev,
				   "PCI backplane error. Frame not sent.\n");
		}
	}
	atomic_inc(&tx_ring->tx_count);
}

void ql_queue_fw_error(struct ql_adapter *qdev)
{
	ql_link_off(qdev);
	queue_delayed_work(qdev->workqueue, &qdev->mpi_reset_work, 0);
}

void ql_queue_asic_error(struct ql_adapter *qdev)
{
	ql_link_off(qdev);
	ql_disable_interrupts(qdev);
	clear_bit(QL_ADAPTER_UP, &qdev->flags);
	set_bit(QL_ASIC_RECOVERY, &qdev->flags);
	queue_delayed_work(qdev->workqueue, &qdev->asic_reset_work, 0);
}

static void ql_process_chip_ae_intr(struct ql_adapter *qdev,
				    struct ib_ae_iocb_rsp *ib_ae_rsp)
{
	switch (ib_ae_rsp->event) {
	case MGMT_ERR_EVENT:
		netif_err(qdev, rx_err, qdev->ndev,
			  "Management Processor Fatal Error.\n");
		ql_queue_fw_error(qdev);
		return;

	case CAM_LOOKUP_ERR_EVENT:
		netdev_err(qdev->ndev, "Multiple CAM hits lookup occurred.\n");
		netdev_err(qdev->ndev, "This event shouldn't occur.\n");
		ql_queue_asic_error(qdev);
		return;

	case SOFT_ECC_ERROR_EVENT:
		netdev_err(qdev->ndev, "Soft ECC error detected.\n");
		ql_queue_asic_error(qdev);
		break;

	case PCI_ERR_ANON_BUF_RD:
		netdev_err(qdev->ndev, "PCI error occurred when reading "
					"anonymous buffers from rx_ring %d.\n",
					ib_ae_rsp->q_id);
		ql_queue_asic_error(qdev);
		break;

	default:
		netif_err(qdev, drv, qdev->ndev, "Unexpected event %d.\n",
			  ib_ae_rsp->event);
		ql_queue_asic_error(qdev);
		break;
	}
}

static int ql_clean_outbound_rx_ring(struct rx_ring *rx_ring)
{
	struct ql_adapter *qdev = rx_ring->qdev;
	u32 prod = ql_read_sh_reg(rx_ring->prod_idx_sh_reg);
	struct ob_mac_iocb_rsp *net_rsp = NULL;
	int count = 0;

	struct tx_ring *tx_ring;
	
	while (prod != rx_ring->cnsmr_idx) {

		netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
			     "cq_id = %d, prod = %d, cnsmr = %d.\n.",
			     rx_ring->cq_id, prod, rx_ring->cnsmr_idx);

		net_rsp = (struct ob_mac_iocb_rsp *)rx_ring->curr_entry;
		rmb();
		switch (net_rsp->opcode) {

		case OPCODE_OB_MAC_TSO_IOCB:
		case OPCODE_OB_MAC_IOCB:
			ql_process_mac_tx_intr(qdev, net_rsp);
			break;
		default:
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "Hit default case, not handled! dropping the packet, opcode = %x.\n",
				     net_rsp->opcode);
		}
		count++;
		ql_update_cq(rx_ring);
		prod = ql_read_sh_reg(rx_ring->prod_idx_sh_reg);
	}
	if (!net_rsp)
		return 0;
	ql_write_cq_idx(rx_ring);
	tx_ring = &qdev->tx_ring[net_rsp->txq_idx];
	if (__netif_subqueue_stopped(qdev->ndev, tx_ring->wq_id)) {
		if (atomic_read(&tx_ring->queue_stopped) &&
		    (atomic_read(&tx_ring->tx_count) > (tx_ring->wq_len / 4)))
			netif_wake_subqueue(qdev->ndev, tx_ring->wq_id);
	}

	return count;
}

static int ql_clean_inbound_rx_ring(struct rx_ring *rx_ring, int budget)
{
	struct ql_adapter *qdev = rx_ring->qdev;
	u32 prod = ql_read_sh_reg(rx_ring->prod_idx_sh_reg);
	struct ql_net_rsp_iocb *net_rsp;
	int count = 0;

	
	while (prod != rx_ring->cnsmr_idx) {

		netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
			     "cq_id = %d, prod = %d, cnsmr = %d.\n.",
			     rx_ring->cq_id, prod, rx_ring->cnsmr_idx);

		net_rsp = rx_ring->curr_entry;
		rmb();
		switch (net_rsp->opcode) {
		case OPCODE_IB_MAC_IOCB:
			ql_process_mac_rx_intr(qdev, rx_ring,
					       (struct ib_mac_iocb_rsp *)
					       net_rsp);
			break;

		case OPCODE_IB_AE_IOCB:
			ql_process_chip_ae_intr(qdev, (struct ib_ae_iocb_rsp *)
						net_rsp);
			break;
		default:
			netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
				     "Hit default case, not handled! dropping the packet, opcode = %x.\n",
				     net_rsp->opcode);
			break;
		}
		count++;
		ql_update_cq(rx_ring);
		prod = ql_read_sh_reg(rx_ring->prod_idx_sh_reg);
		if (count == budget)
			break;
	}
	ql_update_buffer_queues(qdev, rx_ring);
	ql_write_cq_idx(rx_ring);
	return count;
}

static int ql_napi_poll_msix(struct napi_struct *napi, int budget)
{
	struct rx_ring *rx_ring = container_of(napi, struct rx_ring, napi);
	struct ql_adapter *qdev = rx_ring->qdev;
	struct rx_ring *trx_ring;
	int i, work_done = 0;
	struct intr_context *ctx = &qdev->intr_context[rx_ring->cq_id];

	netif_printk(qdev, rx_status, KERN_DEBUG, qdev->ndev,
		     "Enter, NAPI POLL cq_id = %d.\n", rx_ring->cq_id);

	for (i = qdev->rss_ring_count; i < qdev->rx_ring_count; i++) {
		trx_ring = &qdev->rx_ring[i];
		if ((ctx->irq_mask & (1 << trx_ring->cq_id)) &&
			(ql_read_sh_reg(trx_ring->prod_idx_sh_reg) !=
					trx_ring->cnsmr_idx)) {
			netif_printk(qdev, intr, KERN_DEBUG, qdev->ndev,
				     "%s: Servicing TX completion ring %d.\n",
				     __func__, trx_ring->cq_id);
			ql_clean_outbound_rx_ring(trx_ring);
		}
	}

	if (ql_read_sh_reg(rx_ring->prod_idx_sh_reg) !=
					rx_ring->cnsmr_idx) {
		netif_printk(qdev, intr, KERN_DEBUG, qdev->ndev,
			     "%s: Servicing RX completion ring %d.\n",
			     __func__, rx_ring->cq_id);
		work_done = ql_clean_inbound_rx_ring(rx_ring, budget);
	}

	if (work_done < budget) {
		napi_complete(napi);
		ql_enable_completion_interrupt(qdev, rx_ring->irq);
	}
	return work_done;
}

static void qlge_vlan_mode(struct net_device *ndev, netdev_features_t features)
{
	struct ql_adapter *qdev = netdev_priv(ndev);

	if (features & NETIF_F_HW_VLAN_RX) {
		ql_write32(qdev, NIC_RCV_CFG, NIC_RCV_CFG_VLAN_MASK |
				 NIC_RCV_CFG_VLAN_MATCH_AND_NON);
	} else {
		ql_write32(qdev, NIC_RCV_CFG, NIC_RCV_CFG_VLAN_MASK);
	}
}

static netdev_features_t qlge_fix_features(struct net_device *ndev,
	netdev_features_t features)
{
	if (features & NETIF_F_HW_VLAN_RX)
		features |= NETIF_F_HW_VLAN_TX;
	else
		features &= ~NETIF_F_HW_VLAN_TX;

	return features;
}

static int qlge_set_features(struct net_device *ndev,
	netdev_features_t features)
{
	netdev_features_t changed = ndev->features ^ features;

	if (changed & NETIF_F_HW_VLAN_RX)
		qlge_vlan_mode(ndev, features);

	return 0;
}

static int __qlge_vlan_rx_add_vid(struct ql_adapter *qdev, u16 vid)
{
	u32 enable_bit = MAC_ADDR_E;
	int err;

	err = ql_set_mac_addr_reg(qdev, (u8 *) &enable_bit,
				  MAC_ADDR_TYPE_VLAN, vid);
	if (err)
		netif_err(qdev, ifup, qdev->ndev,
			  "Failed to init vlan address.\n");
	return err;
}

static int qlge_vlan_rx_add_vid(struct net_device *ndev, u16 vid)
{
	struct ql_adapter *qdev = netdev_priv(ndev);
	int status;
	int err;

	status = ql_sem_spinlock(qdev, SEM_MAC_ADDR_MASK);
	if (status)
		return status;

	err = __qlge_vlan_rx_add_vid(qdev, vid);
	set_bit(vid, qdev->active_vlans);

	ql_sem_unlock(qdev, SEM_MAC_ADDR_MASK);

	return err;
}

static int __qlge_vlan_rx_kill_vid(struct ql_adapter *qdev, u16 vid)
{
	u32 enable_bit = 0;
	int err;

	err = ql_set_mac_addr_reg(qdev, (u8 *) &enable_bit,
				  MAC_ADDR_TYPE_VLAN, vid);
	if (err)
		netif_err(qdev, ifup, qdev->ndev,
			  "Failed to clear vlan address.\n");
	return err;
}

static int qlge_vlan_rx_kill_vid(struct net_device *ndev, u16 vid)
{
	struct ql_adapter *qdev = netdev_priv(ndev);
	int status;
	int err;

	status = ql_sem_spinlock(qdev, SEM_MAC_ADDR_MASK);
	if (status)
		return status;

	err = __qlge_vlan_rx_kill_vid(qdev, vid);
	clear_bit(vid, qdev->active_vlans);

	ql_sem_unlock(qdev, SEM_MAC_ADDR_MASK);

	return err;
}

static void qlge_restore_vlan(struct ql_adapter *qdev)
{
	int status;
	u16 vid;

	status = ql_sem_spinlock(qdev, SEM_MAC_ADDR_MASK);
	if (status)
		return;

	for_each_set_bit(vid, qdev->active_vlans, VLAN_N_VID)
		__qlge_vlan_rx_add_vid(qdev, vid);

	ql_sem_unlock(qdev, SEM_MAC_ADDR_MASK);
}

static irqreturn_t qlge_msix_rx_isr(int irq, void *dev_id)
{
	struct rx_ring *rx_ring = dev_id;
	napi_schedule(&rx_ring->napi);
	return IRQ_HANDLED;
}

static irqreturn_t qlge_isr(int irq, void *dev_id)
{
	struct rx_ring *rx_ring = dev_id;
	struct ql_adapter *qdev = rx_ring->qdev;
	struct intr_context *intr_context = &qdev->intr_context[0];
	u32 var;
	int work_done = 0;

	spin_lock(&qdev->hw_lock);
	if (atomic_read(&qdev->intr_context[0].irq_cnt)) {
		netif_printk(qdev, intr, KERN_DEBUG, qdev->ndev,
			     "Shared Interrupt, Not ours!\n");
		spin_unlock(&qdev->hw_lock);
		return IRQ_NONE;
	}
	spin_unlock(&qdev->hw_lock);

	var = ql_disable_completion_interrupt(qdev, intr_context->intr);

	if (var & STS_FE) {
		ql_queue_asic_error(qdev);
		netdev_err(qdev->ndev, "Got fatal error, STS = %x.\n", var);
		var = ql_read32(qdev, ERR_STS);
		netdev_err(qdev->ndev, "Resetting chip. "
					"Error Status Register = 0x%x\n", var);
		return IRQ_HANDLED;
	}

	if ((var & STS_PI) &&
		(ql_read32(qdev, INTR_MASK) & INTR_MASK_PI)) {
		netif_err(qdev, intr, qdev->ndev,
			  "Got MPI processor interrupt.\n");
		ql_disable_completion_interrupt(qdev, intr_context->intr);
		ql_write32(qdev, INTR_MASK, (INTR_MASK_PI << 16));
		queue_delayed_work_on(smp_processor_id(),
				qdev->workqueue, &qdev->mpi_work, 0);
		work_done++;
	}

	var = ql_read32(qdev, ISR1);
	if (var & intr_context->irq_mask) {
		netif_info(qdev, intr, qdev->ndev,
			   "Waking handler for rx_ring[0].\n");
		ql_disable_completion_interrupt(qdev, intr_context->intr);
		napi_schedule(&rx_ring->napi);
		work_done++;
	}
	ql_enable_completion_interrupt(qdev, intr_context->intr);
	return work_done ? IRQ_HANDLED : IRQ_NONE;
}

static int ql_tso(struct sk_buff *skb, struct ob_mac_tso_iocb_req *mac_iocb_ptr)
{

	if (skb_is_gso(skb)) {
		int err;
		if (skb_header_cloned(skb)) {
			err = pskb_expand_head(skb, 0, 0, GFP_ATOMIC);
			if (err)
				return err;
		}

		mac_iocb_ptr->opcode = OPCODE_OB_MAC_TSO_IOCB;
		mac_iocb_ptr->flags3 |= OB_MAC_TSO_IOCB_IC;
		mac_iocb_ptr->frame_len = cpu_to_le32((u32) skb->len);
		mac_iocb_ptr->total_hdrs_len =
		    cpu_to_le16(skb_transport_offset(skb) + tcp_hdrlen(skb));
		mac_iocb_ptr->net_trans_offset =
		    cpu_to_le16(skb_network_offset(skb) |
				skb_transport_offset(skb)
				<< OB_MAC_TRANSPORT_HDR_SHIFT);
		mac_iocb_ptr->mss = cpu_to_le16(skb_shinfo(skb)->gso_size);
		mac_iocb_ptr->flags2 |= OB_MAC_TSO_IOCB_LSO;
		if (likely(skb->protocol == htons(ETH_P_IP))) {
			struct iphdr *iph = ip_hdr(skb);
			iph->check = 0;
			mac_iocb_ptr->flags1 |= OB_MAC_TSO_IOCB_IP4;
			tcp_hdr(skb)->check = ~csum_tcpudp_magic(iph->saddr,
								 iph->daddr, 0,
								 IPPROTO_TCP,
								 0);
		} else if (skb->protocol == htons(ETH_P_IPV6)) {
			mac_iocb_ptr->flags1 |= OB_MAC_TSO_IOCB_IP6;
			tcp_hdr(skb)->check =
			    ~csum_ipv6_magic(&ipv6_hdr(skb)->saddr,
					     &ipv6_hdr(skb)->daddr,
					     0, IPPROTO_TCP, 0);
		}
		return 1;
	}
	return 0;
}

static void ql_hw_csum_setup(struct sk_buff *skb,
			     struct ob_mac_tso_iocb_req *mac_iocb_ptr)
{
	int len;
	struct iphdr *iph = ip_hdr(skb);
	__sum16 *check;
	mac_iocb_ptr->opcode = OPCODE_OB_MAC_TSO_IOCB;
	mac_iocb_ptr->frame_len = cpu_to_le32((u32) skb->len);
	mac_iocb_ptr->net_trans_offset =
		cpu_to_le16(skb_network_offset(skb) |
		skb_transport_offset(skb) << OB_MAC_TRANSPORT_HDR_SHIFT);

	mac_iocb_ptr->flags1 |= OB_MAC_TSO_IOCB_IP4;
	len = (ntohs(iph->tot_len) - (iph->ihl << 2));
	if (likely(iph->protocol == IPPROTO_TCP)) {
		check = &(tcp_hdr(skb)->check);
		mac_iocb_ptr->flags2 |= OB_MAC_TSO_IOCB_TC;
		mac_iocb_ptr->total_hdrs_len =
		    cpu_to_le16(skb_transport_offset(skb) +
				(tcp_hdr(skb)->doff << 2));
	} else {
		check = &(udp_hdr(skb)->check);
		mac_iocb_ptr->flags2 |= OB_MAC_TSO_IOCB_UC;
		mac_iocb_ptr->total_hdrs_len =
		    cpu_to_le16(skb_transport_offset(skb) +
				sizeof(struct udphdr));
	}
	*check = ~csum_tcpudp_magic(iph->saddr,
				    iph->daddr, len, iph->protocol, 0);
}

static netdev_tx_t qlge_send(struct sk_buff *skb, struct net_device *ndev)
{
	struct tx_ring_desc *tx_ring_desc;
	struct ob_mac_iocb_req *mac_iocb_ptr;
	struct ql_adapter *qdev = netdev_priv(ndev);
	int tso;
	struct tx_ring *tx_ring;
	u32 tx_ring_idx = (u32) skb->queue_mapping;

	tx_ring = &qdev->tx_ring[tx_ring_idx];

	if (skb_padto(skb, ETH_ZLEN))
		return NETDEV_TX_OK;

	if (unlikely(atomic_read(&tx_ring->tx_count) < 2)) {
		netif_info(qdev, tx_queued, qdev->ndev,
			   "%s: shutting down tx queue %d du to lack of resources.\n",
			   __func__, tx_ring_idx);
		netif_stop_subqueue(ndev, tx_ring->wq_id);
		atomic_inc(&tx_ring->queue_stopped);
		tx_ring->tx_errors++;
		return NETDEV_TX_BUSY;
	}
	tx_ring_desc = &tx_ring->q[tx_ring->prod_idx];
	mac_iocb_ptr = tx_ring_desc->queue_entry;
	memset((void *)mac_iocb_ptr, 0, sizeof(*mac_iocb_ptr));

	mac_iocb_ptr->opcode = OPCODE_OB_MAC_IOCB;
	mac_iocb_ptr->tid = tx_ring_desc->index;
	mac_iocb_ptr->txq_idx = tx_ring_idx;
	tx_ring_desc->skb = skb;

	mac_iocb_ptr->frame_len = cpu_to_le16((u16) skb->len);

	if (vlan_tx_tag_present(skb)) {
		netif_printk(qdev, tx_queued, KERN_DEBUG, qdev->ndev,
			     "Adding a vlan tag %d.\n", vlan_tx_tag_get(skb));
		mac_iocb_ptr->flags3 |= OB_MAC_IOCB_V;
		mac_iocb_ptr->vlan_tci = cpu_to_le16(vlan_tx_tag_get(skb));
	}
	tso = ql_tso(skb, (struct ob_mac_tso_iocb_req *)mac_iocb_ptr);
	if (tso < 0) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	} else if (unlikely(!tso) && (skb->ip_summed == CHECKSUM_PARTIAL)) {
		ql_hw_csum_setup(skb,
				 (struct ob_mac_tso_iocb_req *)mac_iocb_ptr);
	}
	if (ql_map_send(qdev, mac_iocb_ptr, skb, tx_ring_desc) !=
			NETDEV_TX_OK) {
		netif_err(qdev, tx_queued, qdev->ndev,
			  "Could not map the segments.\n");
		tx_ring->tx_errors++;
		return NETDEV_TX_BUSY;
	}
	QL_DUMP_OB_MAC_IOCB(mac_iocb_ptr);
	tx_ring->prod_idx++;
	if (tx_ring->prod_idx == tx_ring->wq_len)
		tx_ring->prod_idx = 0;
	wmb();

	ql_write_db_reg(tx_ring->prod_idx, tx_ring->prod_idx_db_reg);
	netif_printk(qdev, tx_queued, KERN_DEBUG, qdev->ndev,
		     "tx queued, slot %d, len %d\n",
		     tx_ring->prod_idx, skb->len);

	atomic_dec(&tx_ring->tx_count);
	return NETDEV_TX_OK;
}


static void ql_free_shadow_space(struct ql_adapter *qdev)
{
	if (qdev->rx_ring_shadow_reg_area) {
		pci_free_consistent(qdev->pdev,
				    PAGE_SIZE,
				    qdev->rx_ring_shadow_reg_area,
				    qdev->rx_ring_shadow_reg_dma);
		qdev->rx_ring_shadow_reg_area = NULL;
	}
	if (qdev->tx_ring_shadow_reg_area) {
		pci_free_consistent(qdev->pdev,
				    PAGE_SIZE,
				    qdev->tx_ring_shadow_reg_area,
				    qdev->tx_ring_shadow_reg_dma);
		qdev->tx_ring_shadow_reg_area = NULL;
	}
}

static int ql_alloc_shadow_space(struct ql_adapter *qdev)
{
	qdev->rx_ring_shadow_reg_area =
	    pci_alloc_consistent(qdev->pdev,
				 PAGE_SIZE, &qdev->rx_ring_shadow_reg_dma);
	if (qdev->rx_ring_shadow_reg_area == NULL) {
		netif_err(qdev, ifup, qdev->ndev,
			  "Allocation of RX shadow space failed.\n");
		return -ENOMEM;
	}
	memset(qdev->rx_ring_shadow_reg_area, 0, PAGE_SIZE);
	qdev->tx_ring_shadow_reg_area =
	    pci_alloc_consistent(qdev->pdev, PAGE_SIZE,
				 &qdev->tx_ring_shadow_reg_dma);
	if (qdev->tx_ring_shadow_reg_area == NULL) {
		netif_err(qdev, ifup, qdev->ndev,
			  "Allocation of TX shadow space failed.\n");
		goto err_wqp_sh_area;
	}
	memset(qdev->tx_ring_shadow_reg_area, 0, PAGE_SIZE);
	return 0;

err_wqp_sh_area:
	pci_free_consistent(qdev->pdev,
			    PAGE_SIZE,
			    qdev->rx_ring_shadow_reg_area,
			    qdev->rx_ring_shadow_reg_dma);
	return -ENOMEM;
}

static void ql_init_tx_ring(struct ql_adapter *qdev, struct tx_ring *tx_ring)
{
	struct tx_ring_desc *tx_ring_desc;
	int i;
	struct ob_mac_iocb_req *mac_iocb_ptr;

	mac_iocb_ptr = tx_ring->wq_base;
	tx_ring_desc = tx_ring->q;
	for (i = 0; i < tx_ring->wq_len; i++) {
		tx_ring_desc->index = i;
		tx_ring_desc->skb = NULL;
		tx_ring_desc->queue_entry = mac_iocb_ptr;
		mac_iocb_ptr++;
		tx_ring_desc++;
	}
	atomic_set(&tx_ring->tx_count, tx_ring->wq_len);
	atomic_set(&tx_ring->queue_stopped, 0);
}

static void ql_free_tx_resources(struct ql_adapter *qdev,
				 struct tx_ring *tx_ring)
{
	if (tx_ring->wq_base) {
		pci_free_consistent(qdev->pdev, tx_ring->wq_size,
				    tx_ring->wq_base, tx_ring->wq_base_dma);
		tx_ring->wq_base = NULL;
	}
	kfree(tx_ring->q);
	tx_ring->q = NULL;
}

static int ql_alloc_tx_resources(struct ql_adapter *qdev,
				 struct tx_ring *tx_ring)
{
	tx_ring->wq_base =
	    pci_alloc_consistent(qdev->pdev, tx_ring->wq_size,
				 &tx_ring->wq_base_dma);

	if ((tx_ring->wq_base == NULL) ||
	    tx_ring->wq_base_dma & WQ_ADDR_ALIGN) {
		netif_err(qdev, ifup, qdev->ndev, "tx_ring alloc failed.\n");
		return -ENOMEM;
	}
	tx_ring->q =
	    kmalloc(tx_ring->wq_len * sizeof(struct tx_ring_desc), GFP_KERNEL);
	if (tx_ring->q == NULL)
		goto err;

	return 0;
err:
	pci_free_consistent(qdev->pdev, tx_ring->wq_size,
			    tx_ring->wq_base, tx_ring->wq_base_dma);
	return -ENOMEM;
}

static void ql_free_lbq_buffers(struct ql_adapter *qdev, struct rx_ring *rx_ring)
{
	struct bq_desc *lbq_desc;

	uint32_t  curr_idx, clean_idx;

	curr_idx = rx_ring->lbq_curr_idx;
	clean_idx = rx_ring->lbq_clean_idx;
	while (curr_idx != clean_idx) {
		lbq_desc = &rx_ring->lbq[curr_idx];

		if (lbq_desc->p.pg_chunk.last_flag) {
			pci_unmap_page(qdev->pdev,
				lbq_desc->p.pg_chunk.map,
				ql_lbq_block_size(qdev),
				       PCI_DMA_FROMDEVICE);
			lbq_desc->p.pg_chunk.last_flag = 0;
		}

		put_page(lbq_desc->p.pg_chunk.page);
		lbq_desc->p.pg_chunk.page = NULL;

		if (++curr_idx == rx_ring->lbq_len)
			curr_idx = 0;

	}
}

static void ql_free_sbq_buffers(struct ql_adapter *qdev, struct rx_ring *rx_ring)
{
	int i;
	struct bq_desc *sbq_desc;

	for (i = 0; i < rx_ring->sbq_len; i++) {
		sbq_desc = &rx_ring->sbq[i];
		if (sbq_desc == NULL) {
			netif_err(qdev, ifup, qdev->ndev,
				  "sbq_desc %d is NULL.\n", i);
			return;
		}
		if (sbq_desc->p.skb) {
			pci_unmap_single(qdev->pdev,
					 dma_unmap_addr(sbq_desc, mapaddr),
					 dma_unmap_len(sbq_desc, maplen),
					 PCI_DMA_FROMDEVICE);
			dev_kfree_skb(sbq_desc->p.skb);
			sbq_desc->p.skb = NULL;
		}
	}
}

static void ql_free_rx_buffers(struct ql_adapter *qdev)
{
	int i;
	struct rx_ring *rx_ring;

	for (i = 0; i < qdev->rx_ring_count; i++) {
		rx_ring = &qdev->rx_ring[i];
		if (rx_ring->lbq)
			ql_free_lbq_buffers(qdev, rx_ring);
		if (rx_ring->sbq)
			ql_free_sbq_buffers(qdev, rx_ring);
	}
}

static void ql_alloc_rx_buffers(struct ql_adapter *qdev)
{
	struct rx_ring *rx_ring;
	int i;

	for (i = 0; i < qdev->rx_ring_count; i++) {
		rx_ring = &qdev->rx_ring[i];
		if (rx_ring->type != TX_Q)
			ql_update_buffer_queues(qdev, rx_ring);
	}
}

static void ql_init_lbq_ring(struct ql_adapter *qdev,
				struct rx_ring *rx_ring)
{
	int i;
	struct bq_desc *lbq_desc;
	__le64 *bq = rx_ring->lbq_base;

	memset(rx_ring->lbq, 0, rx_ring->lbq_len * sizeof(struct bq_desc));
	for (i = 0; i < rx_ring->lbq_len; i++) {
		lbq_desc = &rx_ring->lbq[i];
		memset(lbq_desc, 0, sizeof(*lbq_desc));
		lbq_desc->index = i;
		lbq_desc->addr = bq;
		bq++;
	}
}

static void ql_init_sbq_ring(struct ql_adapter *qdev,
				struct rx_ring *rx_ring)
{
	int i;
	struct bq_desc *sbq_desc;
	__le64 *bq = rx_ring->sbq_base;

	memset(rx_ring->sbq, 0, rx_ring->sbq_len * sizeof(struct bq_desc));
	for (i = 0; i < rx_ring->sbq_len; i++) {
		sbq_desc = &rx_ring->sbq[i];
		memset(sbq_desc, 0, sizeof(*sbq_desc));
		sbq_desc->index = i;
		sbq_desc->addr = bq;
		bq++;
	}
}

static void ql_free_rx_resources(struct ql_adapter *qdev,
				 struct rx_ring *rx_ring)
{
	
	if (rx_ring->sbq_base) {
		pci_free_consistent(qdev->pdev,
				    rx_ring->sbq_size,
				    rx_ring->sbq_base, rx_ring->sbq_base_dma);
		rx_ring->sbq_base = NULL;
	}

	
	kfree(rx_ring->sbq);
	rx_ring->sbq = NULL;

	
	if (rx_ring->lbq_base) {
		pci_free_consistent(qdev->pdev,
				    rx_ring->lbq_size,
				    rx_ring->lbq_base, rx_ring->lbq_base_dma);
		rx_ring->lbq_base = NULL;
	}

	
	kfree(rx_ring->lbq);
	rx_ring->lbq = NULL;

	
	if (rx_ring->cq_base) {
		pci_free_consistent(qdev->pdev,
				    rx_ring->cq_size,
				    rx_ring->cq_base, rx_ring->cq_base_dma);
		rx_ring->cq_base = NULL;
	}
}

static int ql_alloc_rx_resources(struct ql_adapter *qdev,
				 struct rx_ring *rx_ring)
{

	rx_ring->cq_base =
	    pci_alloc_consistent(qdev->pdev, rx_ring->cq_size,
				 &rx_ring->cq_base_dma);

	if (rx_ring->cq_base == NULL) {
		netif_err(qdev, ifup, qdev->ndev, "rx_ring alloc failed.\n");
		return -ENOMEM;
	}

	if (rx_ring->sbq_len) {
		rx_ring->sbq_base =
		    pci_alloc_consistent(qdev->pdev, rx_ring->sbq_size,
					 &rx_ring->sbq_base_dma);

		if (rx_ring->sbq_base == NULL) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Small buffer queue allocation failed.\n");
			goto err_mem;
		}

		rx_ring->sbq =
		    kmalloc(rx_ring->sbq_len * sizeof(struct bq_desc),
			    GFP_KERNEL);
		if (rx_ring->sbq == NULL) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Small buffer queue control block allocation failed.\n");
			goto err_mem;
		}

		ql_init_sbq_ring(qdev, rx_ring);
	}

	if (rx_ring->lbq_len) {
		rx_ring->lbq_base =
		    pci_alloc_consistent(qdev->pdev, rx_ring->lbq_size,
					 &rx_ring->lbq_base_dma);

		if (rx_ring->lbq_base == NULL) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Large buffer queue allocation failed.\n");
			goto err_mem;
		}
		rx_ring->lbq =
		    kmalloc(rx_ring->lbq_len * sizeof(struct bq_desc),
			    GFP_KERNEL);
		if (rx_ring->lbq == NULL) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Large buffer queue control block allocation failed.\n");
			goto err_mem;
		}

		ql_init_lbq_ring(qdev, rx_ring);
	}

	return 0;

err_mem:
	ql_free_rx_resources(qdev, rx_ring);
	return -ENOMEM;
}

static void ql_tx_ring_clean(struct ql_adapter *qdev)
{
	struct tx_ring *tx_ring;
	struct tx_ring_desc *tx_ring_desc;
	int i, j;

	for (j = 0; j < qdev->tx_ring_count; j++) {
		tx_ring = &qdev->tx_ring[j];
		for (i = 0; i < tx_ring->wq_len; i++) {
			tx_ring_desc = &tx_ring->q[i];
			if (tx_ring_desc && tx_ring_desc->skb) {
				netif_err(qdev, ifdown, qdev->ndev,
					  "Freeing lost SKB %p, from queue %d, index %d.\n",
					  tx_ring_desc->skb, j,
					  tx_ring_desc->index);
				ql_unmap_send(qdev, tx_ring_desc,
					      tx_ring_desc->map_cnt);
				dev_kfree_skb(tx_ring_desc->skb);
				tx_ring_desc->skb = NULL;
			}
		}
	}
}

static void ql_free_mem_resources(struct ql_adapter *qdev)
{
	int i;

	for (i = 0; i < qdev->tx_ring_count; i++)
		ql_free_tx_resources(qdev, &qdev->tx_ring[i]);
	for (i = 0; i < qdev->rx_ring_count; i++)
		ql_free_rx_resources(qdev, &qdev->rx_ring[i]);
	ql_free_shadow_space(qdev);
}

static int ql_alloc_mem_resources(struct ql_adapter *qdev)
{
	int i;

	
	if (ql_alloc_shadow_space(qdev))
		return -ENOMEM;

	for (i = 0; i < qdev->rx_ring_count; i++) {
		if (ql_alloc_rx_resources(qdev, &qdev->rx_ring[i]) != 0) {
			netif_err(qdev, ifup, qdev->ndev,
				  "RX resource allocation failed.\n");
			goto err_mem;
		}
	}
	
	for (i = 0; i < qdev->tx_ring_count; i++) {
		if (ql_alloc_tx_resources(qdev, &qdev->tx_ring[i]) != 0) {
			netif_err(qdev, ifup, qdev->ndev,
				  "TX resource allocation failed.\n");
			goto err_mem;
		}
	}
	return 0;

err_mem:
	ql_free_mem_resources(qdev);
	return -ENOMEM;
}

static int ql_start_rx_ring(struct ql_adapter *qdev, struct rx_ring *rx_ring)
{
	struct cqicb *cqicb = &rx_ring->cqicb;
	void *shadow_reg = qdev->rx_ring_shadow_reg_area +
		(rx_ring->cq_id * RX_RING_SHADOW_SPACE);
	u64 shadow_reg_dma = qdev->rx_ring_shadow_reg_dma +
		(rx_ring->cq_id * RX_RING_SHADOW_SPACE);
	void __iomem *doorbell_area =
	    qdev->doorbell_area + (DB_PAGE_SIZE * (128 + rx_ring->cq_id));
	int err = 0;
	u16 bq_len;
	u64 tmp;
	__le64 *base_indirect_ptr;
	int page_entries;

	
	rx_ring->prod_idx_sh_reg = shadow_reg;
	rx_ring->prod_idx_sh_reg_dma = shadow_reg_dma;
	*rx_ring->prod_idx_sh_reg = 0;
	shadow_reg += sizeof(u64);
	shadow_reg_dma += sizeof(u64);
	rx_ring->lbq_base_indirect = shadow_reg;
	rx_ring->lbq_base_indirect_dma = shadow_reg_dma;
	shadow_reg += (sizeof(u64) * MAX_DB_PAGES_PER_BQ(rx_ring->lbq_len));
	shadow_reg_dma += (sizeof(u64) * MAX_DB_PAGES_PER_BQ(rx_ring->lbq_len));
	rx_ring->sbq_base_indirect = shadow_reg;
	rx_ring->sbq_base_indirect_dma = shadow_reg_dma;

	
	rx_ring->cnsmr_idx_db_reg = (u32 __iomem *) doorbell_area;
	rx_ring->cnsmr_idx = 0;
	rx_ring->curr_entry = rx_ring->cq_base;

	
	rx_ring->valid_db_reg = doorbell_area + 0x04;

	
	rx_ring->lbq_prod_idx_db_reg = (u32 __iomem *) (doorbell_area + 0x18);

	
	rx_ring->sbq_prod_idx_db_reg = (u32 __iomem *) (doorbell_area + 0x1c);

	memset((void *)cqicb, 0, sizeof(struct cqicb));
	cqicb->msix_vect = rx_ring->irq;

	bq_len = (rx_ring->cq_len == 65536) ? 0 : (u16) rx_ring->cq_len;
	cqicb->len = cpu_to_le16(bq_len | LEN_V | LEN_CPP_CONT);

	cqicb->addr = cpu_to_le64(rx_ring->cq_base_dma);

	cqicb->prod_idx_addr = cpu_to_le64(rx_ring->prod_idx_sh_reg_dma);

	cqicb->flags = FLAGS_LC |	
	    FLAGS_LV |		
	    FLAGS_LI;		
	if (rx_ring->lbq_len) {
		cqicb->flags |= FLAGS_LL;	
		tmp = (u64)rx_ring->lbq_base_dma;
		base_indirect_ptr = rx_ring->lbq_base_indirect;
		page_entries = 0;
		do {
			*base_indirect_ptr = cpu_to_le64(tmp);
			tmp += DB_PAGE_SIZE;
			base_indirect_ptr++;
			page_entries++;
		} while (page_entries < MAX_DB_PAGES_PER_BQ(rx_ring->lbq_len));
		cqicb->lbq_addr =
		    cpu_to_le64(rx_ring->lbq_base_indirect_dma);
		bq_len = (rx_ring->lbq_buf_size == 65536) ? 0 :
			(u16) rx_ring->lbq_buf_size;
		cqicb->lbq_buf_size = cpu_to_le16(bq_len);
		bq_len = (rx_ring->lbq_len == 65536) ? 0 :
			(u16) rx_ring->lbq_len;
		cqicb->lbq_len = cpu_to_le16(bq_len);
		rx_ring->lbq_prod_idx = 0;
		rx_ring->lbq_curr_idx = 0;
		rx_ring->lbq_clean_idx = 0;
		rx_ring->lbq_free_cnt = rx_ring->lbq_len;
	}
	if (rx_ring->sbq_len) {
		cqicb->flags |= FLAGS_LS;	
		tmp = (u64)rx_ring->sbq_base_dma;
		base_indirect_ptr = rx_ring->sbq_base_indirect;
		page_entries = 0;
		do {
			*base_indirect_ptr = cpu_to_le64(tmp);
			tmp += DB_PAGE_SIZE;
			base_indirect_ptr++;
			page_entries++;
		} while (page_entries < MAX_DB_PAGES_PER_BQ(rx_ring->sbq_len));
		cqicb->sbq_addr =
		    cpu_to_le64(rx_ring->sbq_base_indirect_dma);
		cqicb->sbq_buf_size =
		    cpu_to_le16((u16)(rx_ring->sbq_buf_size));
		bq_len = (rx_ring->sbq_len == 65536) ? 0 :
			(u16) rx_ring->sbq_len;
		cqicb->sbq_len = cpu_to_le16(bq_len);
		rx_ring->sbq_prod_idx = 0;
		rx_ring->sbq_curr_idx = 0;
		rx_ring->sbq_clean_idx = 0;
		rx_ring->sbq_free_cnt = rx_ring->sbq_len;
	}
	switch (rx_ring->type) {
	case TX_Q:
		cqicb->irq_delay = cpu_to_le16(qdev->tx_coalesce_usecs);
		cqicb->pkt_delay = cpu_to_le16(qdev->tx_max_coalesced_frames);
		break;
	case RX_Q:
		netif_napi_add(qdev->ndev, &rx_ring->napi, ql_napi_poll_msix,
			       64);
		cqicb->irq_delay = cpu_to_le16(qdev->rx_coalesce_usecs);
		cqicb->pkt_delay = cpu_to_le16(qdev->rx_max_coalesced_frames);
		break;
	default:
		netif_printk(qdev, ifup, KERN_DEBUG, qdev->ndev,
			     "Invalid rx_ring->type = %d.\n", rx_ring->type);
	}
	err = ql_write_cfg(qdev, cqicb, sizeof(struct cqicb),
			   CFG_LCQ, rx_ring->cq_id);
	if (err) {
		netif_err(qdev, ifup, qdev->ndev, "Failed to load CQICB.\n");
		return err;
	}
	return err;
}

static int ql_start_tx_ring(struct ql_adapter *qdev, struct tx_ring *tx_ring)
{
	struct wqicb *wqicb = (struct wqicb *)tx_ring;
	void __iomem *doorbell_area =
	    qdev->doorbell_area + (DB_PAGE_SIZE * tx_ring->wq_id);
	void *shadow_reg = qdev->tx_ring_shadow_reg_area +
	    (tx_ring->wq_id * sizeof(u64));
	u64 shadow_reg_dma = qdev->tx_ring_shadow_reg_dma +
	    (tx_ring->wq_id * sizeof(u64));
	int err = 0;

	
	tx_ring->prod_idx_db_reg = (u32 __iomem *) doorbell_area;
	tx_ring->prod_idx = 0;
	
	tx_ring->valid_db_reg = doorbell_area + 0x04;

	tx_ring->cnsmr_idx_sh_reg = shadow_reg;
	tx_ring->cnsmr_idx_sh_reg_dma = shadow_reg_dma;

	wqicb->len = cpu_to_le16(tx_ring->wq_len | Q_LEN_V | Q_LEN_CPP_CONT);
	wqicb->flags = cpu_to_le16(Q_FLAGS_LC |
				   Q_FLAGS_LB | Q_FLAGS_LI | Q_FLAGS_LO);
	wqicb->cq_id_rss = cpu_to_le16(tx_ring->cq_id);
	wqicb->rid = 0;
	wqicb->addr = cpu_to_le64(tx_ring->wq_base_dma);

	wqicb->cnsmr_idx_addr = cpu_to_le64(tx_ring->cnsmr_idx_sh_reg_dma);

	ql_init_tx_ring(qdev, tx_ring);

	err = ql_write_cfg(qdev, wqicb, sizeof(*wqicb), CFG_LRQ,
			   (u16) tx_ring->wq_id);
	if (err) {
		netif_err(qdev, ifup, qdev->ndev, "Failed to load tx_ring.\n");
		return err;
	}
	return err;
}

static void ql_disable_msix(struct ql_adapter *qdev)
{
	if (test_bit(QL_MSIX_ENABLED, &qdev->flags)) {
		pci_disable_msix(qdev->pdev);
		clear_bit(QL_MSIX_ENABLED, &qdev->flags);
		kfree(qdev->msi_x_entry);
		qdev->msi_x_entry = NULL;
	} else if (test_bit(QL_MSI_ENABLED, &qdev->flags)) {
		pci_disable_msi(qdev->pdev);
		clear_bit(QL_MSI_ENABLED, &qdev->flags);
	}
}

static void ql_enable_msix(struct ql_adapter *qdev)
{
	int i, err;

	
	if (qlge_irq_type == MSIX_IRQ) {
		qdev->msi_x_entry = kcalloc(qdev->intr_count,
					    sizeof(struct msix_entry),
					    GFP_KERNEL);
		if (!qdev->msi_x_entry) {
			qlge_irq_type = MSI_IRQ;
			goto msi;
		}

		for (i = 0; i < qdev->intr_count; i++)
			qdev->msi_x_entry[i].entry = i;

		do {
			err = pci_enable_msix(qdev->pdev,
				qdev->msi_x_entry, qdev->intr_count);
			if (err > 0)
				qdev->intr_count = err;
		} while (err > 0);

		if (err < 0) {
			kfree(qdev->msi_x_entry);
			qdev->msi_x_entry = NULL;
			netif_warn(qdev, ifup, qdev->ndev,
				   "MSI-X Enable failed, trying MSI.\n");
			qdev->intr_count = 1;
			qlge_irq_type = MSI_IRQ;
		} else if (err == 0) {
			set_bit(QL_MSIX_ENABLED, &qdev->flags);
			netif_info(qdev, ifup, qdev->ndev,
				   "MSI-X Enabled, got %d vectors.\n",
				   qdev->intr_count);
			return;
		}
	}
msi:
	qdev->intr_count = 1;
	if (qlge_irq_type == MSI_IRQ) {
		if (!pci_enable_msi(qdev->pdev)) {
			set_bit(QL_MSI_ENABLED, &qdev->flags);
			netif_info(qdev, ifup, qdev->ndev,
				   "Running with MSI interrupts.\n");
			return;
		}
	}
	qlge_irq_type = LEG_IRQ;
	netif_printk(qdev, ifup, KERN_DEBUG, qdev->ndev,
		     "Running with legacy interrupts.\n");
}

static void ql_set_tx_vect(struct ql_adapter *qdev)
{
	int i, j, vect;
	u32 tx_rings_per_vector = qdev->tx_ring_count / qdev->intr_count;

	if (likely(test_bit(QL_MSIX_ENABLED, &qdev->flags))) {
		
		for (vect = 0, j = 0, i = qdev->rss_ring_count;
					 i < qdev->rx_ring_count; i++) {
			if (j == tx_rings_per_vector) {
				vect++;
				j = 0;
			}
			qdev->rx_ring[i].irq = vect;
			j++;
		}
	} else {
		for (i = 0; i < qdev->rx_ring_count; i++)
			qdev->rx_ring[i].irq = 0;
	}
}

static void ql_set_irq_mask(struct ql_adapter *qdev, struct intr_context *ctx)
{
	int j, vect = ctx->intr;
	u32 tx_rings_per_vector = qdev->tx_ring_count / qdev->intr_count;

	if (likely(test_bit(QL_MSIX_ENABLED, &qdev->flags))) {
		ctx->irq_mask = (1 << qdev->rx_ring[vect].cq_id);
		for (j = 0; j < tx_rings_per_vector; j++) {
			ctx->irq_mask |=
			(1 << qdev->rx_ring[qdev->rss_ring_count +
			(vect * tx_rings_per_vector) + j].cq_id);
		}
	} else {
		for (j = 0; j < qdev->rx_ring_count; j++)
			ctx->irq_mask |= (1 << qdev->rx_ring[j].cq_id);
	}
}

static void ql_resolve_queues_to_irqs(struct ql_adapter *qdev)
{
	int i = 0;
	struct intr_context *intr_context = &qdev->intr_context[0];

	if (likely(test_bit(QL_MSIX_ENABLED, &qdev->flags))) {
		for (i = 0; i < qdev->intr_count; i++, intr_context++) {
			qdev->rx_ring[i].irq = i;
			intr_context->intr = i;
			intr_context->qdev = qdev;
			ql_set_irq_mask(qdev, intr_context);
			intr_context->intr_en_mask =
			    INTR_EN_TYPE_MASK | INTR_EN_INTR_MASK |
			    INTR_EN_TYPE_ENABLE | INTR_EN_IHD_MASK | INTR_EN_IHD
			    | i;
			intr_context->intr_dis_mask =
			    INTR_EN_TYPE_MASK | INTR_EN_INTR_MASK |
			    INTR_EN_TYPE_DISABLE | INTR_EN_IHD_MASK |
			    INTR_EN_IHD | i;
			intr_context->intr_read_mask =
			    INTR_EN_TYPE_MASK | INTR_EN_INTR_MASK |
			    INTR_EN_TYPE_READ | INTR_EN_IHD_MASK | INTR_EN_IHD |
			    i;
			if (i == 0) {
				intr_context->handler = qlge_isr;
				sprintf(intr_context->name, "%s-rx-%d",
					qdev->ndev->name, i);
			} else {
				intr_context->handler = qlge_msix_rx_isr;
				sprintf(intr_context->name, "%s-rx-%d",
					qdev->ndev->name, i);
			}
		}
	} else {
		intr_context->intr = 0;
		intr_context->qdev = qdev;
		intr_context->intr_en_mask =
		    INTR_EN_TYPE_MASK | INTR_EN_INTR_MASK | INTR_EN_TYPE_ENABLE;
		intr_context->intr_dis_mask =
		    INTR_EN_TYPE_MASK | INTR_EN_INTR_MASK |
		    INTR_EN_TYPE_DISABLE;
		intr_context->intr_read_mask =
		    INTR_EN_TYPE_MASK | INTR_EN_INTR_MASK | INTR_EN_TYPE_READ;
		intr_context->handler = qlge_isr;
		sprintf(intr_context->name, "%s-single_irq", qdev->ndev->name);
		ql_set_irq_mask(qdev, intr_context);
	}
	ql_set_tx_vect(qdev);
}

static void ql_free_irq(struct ql_adapter *qdev)
{
	int i;
	struct intr_context *intr_context = &qdev->intr_context[0];

	for (i = 0; i < qdev->intr_count; i++, intr_context++) {
		if (intr_context->hooked) {
			if (test_bit(QL_MSIX_ENABLED, &qdev->flags)) {
				free_irq(qdev->msi_x_entry[i].vector,
					 &qdev->rx_ring[i]);
			} else {
				free_irq(qdev->pdev->irq, &qdev->rx_ring[0]);
			}
		}
	}
	ql_disable_msix(qdev);
}

static int ql_request_irq(struct ql_adapter *qdev)
{
	int i;
	int status = 0;
	struct pci_dev *pdev = qdev->pdev;
	struct intr_context *intr_context = &qdev->intr_context[0];

	ql_resolve_queues_to_irqs(qdev);

	for (i = 0; i < qdev->intr_count; i++, intr_context++) {
		atomic_set(&intr_context->irq_cnt, 0);
		if (test_bit(QL_MSIX_ENABLED, &qdev->flags)) {
			status = request_irq(qdev->msi_x_entry[i].vector,
					     intr_context->handler,
					     0,
					     intr_context->name,
					     &qdev->rx_ring[i]);
			if (status) {
				netif_err(qdev, ifup, qdev->ndev,
					  "Failed request for MSIX interrupt %d.\n",
					  i);
				goto err_irq;
			}
		} else {
			netif_printk(qdev, ifup, KERN_DEBUG, qdev->ndev,
				     "trying msi or legacy interrupts.\n");
			netif_printk(qdev, ifup, KERN_DEBUG, qdev->ndev,
				     "%s: irq = %d.\n", __func__, pdev->irq);
			netif_printk(qdev, ifup, KERN_DEBUG, qdev->ndev,
				     "%s: context->name = %s.\n", __func__,
				     intr_context->name);
			netif_printk(qdev, ifup, KERN_DEBUG, qdev->ndev,
				     "%s: dev_id = 0x%p.\n", __func__,
				     &qdev->rx_ring[0]);
			status =
			    request_irq(pdev->irq, qlge_isr,
					test_bit(QL_MSI_ENABLED,
						 &qdev->
						 flags) ? 0 : IRQF_SHARED,
					intr_context->name, &qdev->rx_ring[0]);
			if (status)
				goto err_irq;

			netif_err(qdev, ifup, qdev->ndev,
				  "Hooked intr %d, queue type %s, with name %s.\n",
				  i,
				  qdev->rx_ring[0].type == DEFAULT_Q ?
				  "DEFAULT_Q" :
				  qdev->rx_ring[0].type == TX_Q ? "TX_Q" :
				  qdev->rx_ring[0].type == RX_Q ? "RX_Q" : "",
				  intr_context->name);
		}
		intr_context->hooked = 1;
	}
	return status;
err_irq:
	netif_err(qdev, ifup, qdev->ndev, "Failed to get the interrupts!!!/n");
	ql_free_irq(qdev);
	return status;
}

static int ql_start_rss(struct ql_adapter *qdev)
{
	static const u8 init_hash_seed[] = {
		0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2,
		0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0,
		0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4,
		0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c,
		0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa
	};
	struct ricb *ricb = &qdev->ricb;
	int status = 0;
	int i;
	u8 *hash_id = (u8 *) ricb->hash_cq_id;

	memset((void *)ricb, 0, sizeof(*ricb));

	ricb->base_cq = RSS_L4K;
	ricb->flags =
		(RSS_L6K | RSS_LI | RSS_LB | RSS_LM | RSS_RT4 | RSS_RT6);
	ricb->mask = cpu_to_le16((u16)(0x3ff));

	for (i = 0; i < 1024; i++)
		hash_id[i] = (i & (qdev->rss_ring_count - 1));

	memcpy((void *)&ricb->ipv6_hash_key[0], init_hash_seed, 40);
	memcpy((void *)&ricb->ipv4_hash_key[0], init_hash_seed, 16);

	status = ql_write_cfg(qdev, ricb, sizeof(*ricb), CFG_LR, 0);
	if (status) {
		netif_err(qdev, ifup, qdev->ndev, "Failed to load RICB.\n");
		return status;
	}
	return status;
}

static int ql_clear_routing_entries(struct ql_adapter *qdev)
{
	int i, status = 0;

	status = ql_sem_spinlock(qdev, SEM_RT_IDX_MASK);
	if (status)
		return status;
	
	for (i = 0; i < 16; i++) {
		status = ql_set_routing_reg(qdev, i, 0, 0);
		if (status) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Failed to init routing register for CAM packets.\n");
			break;
		}
	}
	ql_sem_unlock(qdev, SEM_RT_IDX_MASK);
	return status;
}

static int ql_route_initialize(struct ql_adapter *qdev)
{
	int status = 0;

	
	status = ql_clear_routing_entries(qdev);
	if (status)
		return status;

	status = ql_sem_spinlock(qdev, SEM_RT_IDX_MASK);
	if (status)
		return status;

	status = ql_set_routing_reg(qdev, RT_IDX_IP_CSUM_ERR_SLOT,
						RT_IDX_IP_CSUM_ERR, 1);
	if (status) {
		netif_err(qdev, ifup, qdev->ndev,
			"Failed to init routing register "
			"for IP CSUM error packets.\n");
		goto exit;
	}
	status = ql_set_routing_reg(qdev, RT_IDX_TCP_UDP_CSUM_ERR_SLOT,
						RT_IDX_TU_CSUM_ERR, 1);
	if (status) {
		netif_err(qdev, ifup, qdev->ndev,
			"Failed to init routing register "
			"for TCP/UDP CSUM error packets.\n");
		goto exit;
	}
	status = ql_set_routing_reg(qdev, RT_IDX_BCAST_SLOT, RT_IDX_BCAST, 1);
	if (status) {
		netif_err(qdev, ifup, qdev->ndev,
			  "Failed to init routing register for broadcast packets.\n");
		goto exit;
	}
	if (qdev->rss_ring_count > 1) {
		status = ql_set_routing_reg(qdev, RT_IDX_RSS_MATCH_SLOT,
					RT_IDX_RSS_MATCH, 1);
		if (status) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Failed to init routing register for MATCH RSS packets.\n");
			goto exit;
		}
	}

	status = ql_set_routing_reg(qdev, RT_IDX_CAM_HIT_SLOT,
				    RT_IDX_CAM_HIT, 1);
	if (status)
		netif_err(qdev, ifup, qdev->ndev,
			  "Failed to init routing register for CAM packets.\n");
exit:
	ql_sem_unlock(qdev, SEM_RT_IDX_MASK);
	return status;
}

int ql_cam_route_initialize(struct ql_adapter *qdev)
{
	int status, set;

	set = ql_read32(qdev, STS);
	set &= qdev->port_link_up;
	status = ql_set_mac_addr(qdev, set);
	if (status) {
		netif_err(qdev, ifup, qdev->ndev, "Failed to init mac address.\n");
		return status;
	}

	status = ql_route_initialize(qdev);
	if (status)
		netif_err(qdev, ifup, qdev->ndev, "Failed to init routing table.\n");

	return status;
}

static int ql_adapter_initialize(struct ql_adapter *qdev)
{
	u32 value, mask;
	int i;
	int status = 0;

	value = SYS_EFE | SYS_FAE;
	mask = value << 16;
	ql_write32(qdev, SYS, mask | value);

	
	value = NIC_RCV_CFG_DFQ | NIC_RCV_CFG_RV;
	mask = NIC_RCV_CFG_DFQ_MASK | (NIC_RCV_CFG_RV << 16);
	ql_write32(qdev, NIC_RCV_CFG, (mask | value));

	
	ql_write32(qdev, INTR_MASK, (INTR_MASK_PI << 16) | INTR_MASK_PI);

	
	value = FSC_FE | FSC_EPC_INBOUND | FSC_EPC_OUTBOUND |
	    FSC_EC | FSC_VM_PAGE_4K;
	value |= SPLT_SETTING;

	
	mask = FSC_VM_PAGESIZE_MASK |
	    FSC_DBL_MASK | FSC_DBRST_MASK | (value << 16);
	ql_write32(qdev, FSC, mask | value);

	ql_write32(qdev, SPLT_HDR, SPLT_LEN);

	ql_write32(qdev, RST_FO, RST_FO_RR_MASK | RST_FO_RR_RCV_FUNC_CQ);
	value = ql_read32(qdev, MGMT_RCV_CFG);
	value &= ~MGMT_RCV_CFG_RM;
	mask = 0xffff0000;

	
	ql_write32(qdev, MGMT_RCV_CFG, mask);
	ql_write32(qdev, MGMT_RCV_CFG, mask | value);

	
	if (qdev->pdev->subsystem_device == 0x0068 ||
			qdev->pdev->subsystem_device == 0x0180)
		qdev->wol = WAKE_MAGIC;

	
	for (i = 0; i < qdev->rx_ring_count; i++) {
		status = ql_start_rx_ring(qdev, &qdev->rx_ring[i]);
		if (status) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Failed to start rx ring[%d].\n", i);
			return status;
		}
	}

	if (qdev->rss_ring_count > 1) {
		status = ql_start_rss(qdev);
		if (status) {
			netif_err(qdev, ifup, qdev->ndev, "Failed to start RSS.\n");
			return status;
		}
	}

	
	for (i = 0; i < qdev->tx_ring_count; i++) {
		status = ql_start_tx_ring(qdev, &qdev->tx_ring[i]);
		if (status) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Failed to start tx ring[%d].\n", i);
			return status;
		}
	}

	
	status = qdev->nic_ops->port_initialize(qdev);
	if (status)
		netif_err(qdev, ifup, qdev->ndev, "Failed to start port.\n");

	
	status = ql_cam_route_initialize(qdev);
	if (status) {
		netif_err(qdev, ifup, qdev->ndev,
			  "Failed to init CAM/Routing tables.\n");
		return status;
	}

	
	for (i = 0; i < qdev->rss_ring_count; i++)
		napi_enable(&qdev->rx_ring[i].napi);

	return status;
}

static int ql_adapter_reset(struct ql_adapter *qdev)
{
	u32 value;
	int status = 0;
	unsigned long end_jiffies;

	
	status = ql_clear_routing_entries(qdev);
	if (status) {
		netif_err(qdev, ifup, qdev->ndev, "Failed to clear routing bits.\n");
		return status;
	}

	end_jiffies = jiffies +
		max((unsigned long)1, usecs_to_jiffies(30));

	if (!test_bit(QL_ASIC_RECOVERY, &qdev->flags)) {
		
		ql_mb_set_mgmnt_traffic_ctl(qdev, MB_SET_MPI_TFK_STOP);

		
		ql_wait_fifo_empty(qdev);
	} else
		clear_bit(QL_ASIC_RECOVERY, &qdev->flags);

	ql_write32(qdev, RST_FO, (RST_FO_FR << 16) | RST_FO_FR);

	do {
		value = ql_read32(qdev, RST_FO);
		if ((value & RST_FO_FR) == 0)
			break;
		cpu_relax();
	} while (time_before(jiffies, end_jiffies));

	if (value & RST_FO_FR) {
		netif_err(qdev, ifdown, qdev->ndev,
			  "ETIMEDOUT!!! errored out of resetting the chip!\n");
		status = -ETIMEDOUT;
	}

	
	ql_mb_set_mgmnt_traffic_ctl(qdev, MB_SET_MPI_TFK_RESUME);
	return status;
}

static void ql_display_dev_info(struct net_device *ndev)
{
	struct ql_adapter *qdev = netdev_priv(ndev);

	netif_info(qdev, probe, qdev->ndev,
		   "Function #%d, Port %d, NIC Roll %d, NIC Rev = %d, "
		   "XG Roll = %d, XG Rev = %d.\n",
		   qdev->func,
		   qdev->port,
		   qdev->chip_rev_id & 0x0000000f,
		   qdev->chip_rev_id >> 4 & 0x0000000f,
		   qdev->chip_rev_id >> 8 & 0x0000000f,
		   qdev->chip_rev_id >> 12 & 0x0000000f);
	netif_info(qdev, probe, qdev->ndev,
		   "MAC address %pM\n", ndev->dev_addr);
}

static int ql_wol(struct ql_adapter *qdev)
{
	int status = 0;
	u32 wol = MB_WOL_DISABLE;


	if (qdev->wol & (WAKE_ARP | WAKE_MAGICSECURE | WAKE_PHY | WAKE_UCAST |
			WAKE_MCAST | WAKE_BCAST)) {
		netif_err(qdev, ifdown, qdev->ndev,
			  "Unsupported WOL paramter. qdev->wol = 0x%x.\n",
			  qdev->wol);
		return -EINVAL;
	}

	if (qdev->wol & WAKE_MAGIC) {
		status = ql_mb_wol_set_magic(qdev, 1);
		if (status) {
			netif_err(qdev, ifdown, qdev->ndev,
				  "Failed to set magic packet on %s.\n",
				  qdev->ndev->name);
			return status;
		} else
			netif_info(qdev, drv, qdev->ndev,
				   "Enabled magic packet successfully on %s.\n",
				   qdev->ndev->name);

		wol |= MB_WOL_MAGIC_PKT;
	}

	if (qdev->wol) {
		wol |= MB_WOL_MODE_ON;
		status = ql_mb_wol_mode(qdev, wol);
		netif_err(qdev, drv, qdev->ndev,
			  "WOL %s (wol code 0x%x) on %s\n",
			  (status == 0) ? "Successfully set" : "Failed",
			  wol, qdev->ndev->name);
	}

	return status;
}

static void ql_cancel_all_work_sync(struct ql_adapter *qdev)
{

	if (test_bit(QL_ADAPTER_UP, &qdev->flags))
		cancel_delayed_work_sync(&qdev->asic_reset_work);
	cancel_delayed_work_sync(&qdev->mpi_reset_work);
	cancel_delayed_work_sync(&qdev->mpi_work);
	cancel_delayed_work_sync(&qdev->mpi_idc_work);
	cancel_delayed_work_sync(&qdev->mpi_core_to_log);
	cancel_delayed_work_sync(&qdev->mpi_port_cfg_work);
}

static int ql_adapter_down(struct ql_adapter *qdev)
{
	int i, status = 0;

	ql_link_off(qdev);

	ql_cancel_all_work_sync(qdev);

	for (i = 0; i < qdev->rss_ring_count; i++)
		napi_disable(&qdev->rx_ring[i].napi);

	clear_bit(QL_ADAPTER_UP, &qdev->flags);

	ql_disable_interrupts(qdev);

	ql_tx_ring_clean(qdev);

	for (i = 0; i < qdev->rss_ring_count; i++)
		netif_napi_del(&qdev->rx_ring[i].napi);

	status = ql_adapter_reset(qdev);
	if (status)
		netif_err(qdev, ifdown, qdev->ndev, "reset(func #%d) FAILED!\n",
			  qdev->func);
	ql_free_rx_buffers(qdev);

	return status;
}

static int ql_adapter_up(struct ql_adapter *qdev)
{
	int err = 0;

	err = ql_adapter_initialize(qdev);
	if (err) {
		netif_info(qdev, ifup, qdev->ndev, "Unable to initialize adapter.\n");
		goto err_init;
	}
	set_bit(QL_ADAPTER_UP, &qdev->flags);
	ql_alloc_rx_buffers(qdev);
	if ((ql_read32(qdev, STS) & qdev->port_init) &&
			(ql_read32(qdev, STS) & qdev->port_link_up))
		ql_link_on(qdev);
	
	clear_bit(QL_ALLMULTI, &qdev->flags);
	clear_bit(QL_PROMISCUOUS, &qdev->flags);
	qlge_set_multicast_list(qdev->ndev);

	
	qlge_restore_vlan(qdev);

	ql_enable_interrupts(qdev);
	ql_enable_all_completion_interrupts(qdev);
	netif_tx_start_all_queues(qdev->ndev);

	return 0;
err_init:
	ql_adapter_reset(qdev);
	return err;
}

static void ql_release_adapter_resources(struct ql_adapter *qdev)
{
	ql_free_mem_resources(qdev);
	ql_free_irq(qdev);
}

static int ql_get_adapter_resources(struct ql_adapter *qdev)
{
	int status = 0;

	if (ql_alloc_mem_resources(qdev)) {
		netif_err(qdev, ifup, qdev->ndev, "Unable to  allocate memory.\n");
		return -ENOMEM;
	}
	status = ql_request_irq(qdev);
	return status;
}

static int qlge_close(struct net_device *ndev)
{
	struct ql_adapter *qdev = netdev_priv(ndev);

	if (test_bit(QL_EEH_FATAL, &qdev->flags)) {
		netif_err(qdev, drv, qdev->ndev, "EEH fatal did unload.\n");
		clear_bit(QL_EEH_FATAL, &qdev->flags);
		return 0;
	}

	while (!test_bit(QL_ADAPTER_UP, &qdev->flags))
		msleep(1);
	ql_adapter_down(qdev);
	ql_release_adapter_resources(qdev);
	return 0;
}

static int ql_configure_rings(struct ql_adapter *qdev)
{
	int i;
	struct rx_ring *rx_ring;
	struct tx_ring *tx_ring;
	int cpu_cnt = min(MAX_CPUS, (int)num_online_cpus());
	unsigned int lbq_buf_len = (qdev->ndev->mtu > 1500) ?
		LARGE_BUFFER_MAX_SIZE : LARGE_BUFFER_MIN_SIZE;

	qdev->lbq_buf_order = get_order(lbq_buf_len);

	qdev->intr_count = cpu_cnt;
	ql_enable_msix(qdev);
	
	qdev->rss_ring_count = qdev->intr_count;
	qdev->tx_ring_count = cpu_cnt;
	qdev->rx_ring_count = qdev->tx_ring_count + qdev->rss_ring_count;

	for (i = 0; i < qdev->tx_ring_count; i++) {
		tx_ring = &qdev->tx_ring[i];
		memset((void *)tx_ring, 0, sizeof(*tx_ring));
		tx_ring->qdev = qdev;
		tx_ring->wq_id = i;
		tx_ring->wq_len = qdev->tx_ring_size;
		tx_ring->wq_size =
		    tx_ring->wq_len * sizeof(struct ob_mac_iocb_req);

		tx_ring->cq_id = qdev->rss_ring_count + i;
	}

	for (i = 0; i < qdev->rx_ring_count; i++) {
		rx_ring = &qdev->rx_ring[i];
		memset((void *)rx_ring, 0, sizeof(*rx_ring));
		rx_ring->qdev = qdev;
		rx_ring->cq_id = i;
		rx_ring->cpu = i % cpu_cnt;	
		if (i < qdev->rss_ring_count) {
			rx_ring->cq_len = qdev->rx_ring_size;
			rx_ring->cq_size =
			    rx_ring->cq_len * sizeof(struct ql_net_rsp_iocb);
			rx_ring->lbq_len = NUM_LARGE_BUFFERS;
			rx_ring->lbq_size =
			    rx_ring->lbq_len * sizeof(__le64);
			rx_ring->lbq_buf_size = (u16)lbq_buf_len;
			rx_ring->sbq_len = NUM_SMALL_BUFFERS;
			rx_ring->sbq_size =
			    rx_ring->sbq_len * sizeof(__le64);
			rx_ring->sbq_buf_size = SMALL_BUF_MAP_SIZE;
			rx_ring->type = RX_Q;
		} else {
			
			rx_ring->cq_len = qdev->tx_ring_size;
			rx_ring->cq_size =
			    rx_ring->cq_len * sizeof(struct ql_net_rsp_iocb);
			rx_ring->lbq_len = 0;
			rx_ring->lbq_size = 0;
			rx_ring->lbq_buf_size = 0;
			rx_ring->sbq_len = 0;
			rx_ring->sbq_size = 0;
			rx_ring->sbq_buf_size = 0;
			rx_ring->type = TX_Q;
		}
	}
	return 0;
}

static int qlge_open(struct net_device *ndev)
{
	int err = 0;
	struct ql_adapter *qdev = netdev_priv(ndev);

	err = ql_adapter_reset(qdev);
	if (err)
		return err;

	err = ql_configure_rings(qdev);
	if (err)
		return err;

	err = ql_get_adapter_resources(qdev);
	if (err)
		goto error_up;

	err = ql_adapter_up(qdev);
	if (err)
		goto error_up;

	return err;

error_up:
	ql_release_adapter_resources(qdev);
	return err;
}

static int ql_change_rx_buffers(struct ql_adapter *qdev)
{
	struct rx_ring *rx_ring;
	int i, status;
	u32 lbq_buf_len;

	
	if (!test_bit(QL_ADAPTER_UP, &qdev->flags)) {
		int i = 3;
		while (i-- && !test_bit(QL_ADAPTER_UP, &qdev->flags)) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Waiting for adapter UP...\n");
			ssleep(1);
		}

		if (!i) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Timed out waiting for adapter UP\n");
			return -ETIMEDOUT;
		}
	}

	status = ql_adapter_down(qdev);
	if (status)
		goto error;

	
	lbq_buf_len = (qdev->ndev->mtu > 1500) ?
		LARGE_BUFFER_MAX_SIZE : LARGE_BUFFER_MIN_SIZE;
	qdev->lbq_buf_order = get_order(lbq_buf_len);

	for (i = 0; i < qdev->rss_ring_count; i++) {
		rx_ring = &qdev->rx_ring[i];
		
		rx_ring->lbq_buf_size = lbq_buf_len;
	}

	status = ql_adapter_up(qdev);
	if (status)
		goto error;

	return status;
error:
	netif_alert(qdev, ifup, qdev->ndev,
		    "Driver up/down cycle failed, closing device.\n");
	set_bit(QL_ADAPTER_UP, &qdev->flags);
	dev_close(qdev->ndev);
	return status;
}

static int qlge_change_mtu(struct net_device *ndev, int new_mtu)
{
	struct ql_adapter *qdev = netdev_priv(ndev);
	int status;

	if (ndev->mtu == 1500 && new_mtu == 9000) {
		netif_err(qdev, ifup, qdev->ndev, "Changing to jumbo MTU.\n");
	} else if (ndev->mtu == 9000 && new_mtu == 1500) {
		netif_err(qdev, ifup, qdev->ndev, "Changing to normal MTU.\n");
	} else
		return -EINVAL;

	queue_delayed_work(qdev->workqueue,
			&qdev->mpi_port_cfg_work, 3*HZ);

	ndev->mtu = new_mtu;

	if (!netif_running(qdev->ndev)) {
		return 0;
	}

	status = ql_change_rx_buffers(qdev);
	if (status) {
		netif_err(qdev, ifup, qdev->ndev,
			  "Changing MTU failed.\n");
	}

	return status;
}

static struct net_device_stats *qlge_get_stats(struct net_device
					       *ndev)
{
	struct ql_adapter *qdev = netdev_priv(ndev);
	struct rx_ring *rx_ring = &qdev->rx_ring[0];
	struct tx_ring *tx_ring = &qdev->tx_ring[0];
	unsigned long pkts, mcast, dropped, errors, bytes;
	int i;

	
	pkts = mcast = dropped = errors = bytes = 0;
	for (i = 0; i < qdev->rss_ring_count; i++, rx_ring++) {
			pkts += rx_ring->rx_packets;
			bytes += rx_ring->rx_bytes;
			dropped += rx_ring->rx_dropped;
			errors += rx_ring->rx_errors;
			mcast += rx_ring->rx_multicast;
	}
	ndev->stats.rx_packets = pkts;
	ndev->stats.rx_bytes = bytes;
	ndev->stats.rx_dropped = dropped;
	ndev->stats.rx_errors = errors;
	ndev->stats.multicast = mcast;

	
	pkts = errors = bytes = 0;
	for (i = 0; i < qdev->tx_ring_count; i++, tx_ring++) {
			pkts += tx_ring->tx_packets;
			bytes += tx_ring->tx_bytes;
			errors += tx_ring->tx_errors;
	}
	ndev->stats.tx_packets = pkts;
	ndev->stats.tx_bytes = bytes;
	ndev->stats.tx_errors = errors;
	return &ndev->stats;
}

static void qlge_set_multicast_list(struct net_device *ndev)
{
	struct ql_adapter *qdev = netdev_priv(ndev);
	struct netdev_hw_addr *ha;
	int i, status;

	status = ql_sem_spinlock(qdev, SEM_RT_IDX_MASK);
	if (status)
		return;
	if (ndev->flags & IFF_PROMISC) {
		if (!test_bit(QL_PROMISCUOUS, &qdev->flags)) {
			if (ql_set_routing_reg
			    (qdev, RT_IDX_PROMISCUOUS_SLOT, RT_IDX_VALID, 1)) {
				netif_err(qdev, hw, qdev->ndev,
					  "Failed to set promiscuous mode.\n");
			} else {
				set_bit(QL_PROMISCUOUS, &qdev->flags);
			}
		}
	} else {
		if (test_bit(QL_PROMISCUOUS, &qdev->flags)) {
			if (ql_set_routing_reg
			    (qdev, RT_IDX_PROMISCUOUS_SLOT, RT_IDX_VALID, 0)) {
				netif_err(qdev, hw, qdev->ndev,
					  "Failed to clear promiscuous mode.\n");
			} else {
				clear_bit(QL_PROMISCUOUS, &qdev->flags);
			}
		}
	}

	if ((ndev->flags & IFF_ALLMULTI) ||
	    (netdev_mc_count(ndev) > MAX_MULTICAST_ENTRIES)) {
		if (!test_bit(QL_ALLMULTI, &qdev->flags)) {
			if (ql_set_routing_reg
			    (qdev, RT_IDX_ALLMULTI_SLOT, RT_IDX_MCAST, 1)) {
				netif_err(qdev, hw, qdev->ndev,
					  "Failed to set all-multi mode.\n");
			} else {
				set_bit(QL_ALLMULTI, &qdev->flags);
			}
		}
	} else {
		if (test_bit(QL_ALLMULTI, &qdev->flags)) {
			if (ql_set_routing_reg
			    (qdev, RT_IDX_ALLMULTI_SLOT, RT_IDX_MCAST, 0)) {
				netif_err(qdev, hw, qdev->ndev,
					  "Failed to clear all-multi mode.\n");
			} else {
				clear_bit(QL_ALLMULTI, &qdev->flags);
			}
		}
	}

	if (!netdev_mc_empty(ndev)) {
		status = ql_sem_spinlock(qdev, SEM_MAC_ADDR_MASK);
		if (status)
			goto exit;
		i = 0;
		netdev_for_each_mc_addr(ha, ndev) {
			if (ql_set_mac_addr_reg(qdev, (u8 *) ha->addr,
						MAC_ADDR_TYPE_MULTI_MAC, i)) {
				netif_err(qdev, hw, qdev->ndev,
					  "Failed to loadmulticast address.\n");
				ql_sem_unlock(qdev, SEM_MAC_ADDR_MASK);
				goto exit;
			}
			i++;
		}
		ql_sem_unlock(qdev, SEM_MAC_ADDR_MASK);
		if (ql_set_routing_reg
		    (qdev, RT_IDX_MCAST_MATCH_SLOT, RT_IDX_MCAST_MATCH, 1)) {
			netif_err(qdev, hw, qdev->ndev,
				  "Failed to set multicast match mode.\n");
		} else {
			set_bit(QL_ALLMULTI, &qdev->flags);
		}
	}
exit:
	ql_sem_unlock(qdev, SEM_RT_IDX_MASK);
}

static int qlge_set_mac_address(struct net_device *ndev, void *p)
{
	struct ql_adapter *qdev = netdev_priv(ndev);
	struct sockaddr *addr = p;
	int status;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;
	memcpy(ndev->dev_addr, addr->sa_data, ndev->addr_len);
	
	memcpy(qdev->current_mac_addr, ndev->dev_addr, ndev->addr_len);

	status = ql_sem_spinlock(qdev, SEM_MAC_ADDR_MASK);
	if (status)
		return status;
	status = ql_set_mac_addr_reg(qdev, (u8 *) ndev->dev_addr,
			MAC_ADDR_TYPE_CAM_MAC, qdev->func * MAX_CQ);
	if (status)
		netif_err(qdev, hw, qdev->ndev, "Failed to load MAC address.\n");
	ql_sem_unlock(qdev, SEM_MAC_ADDR_MASK);
	return status;
}

static void qlge_tx_timeout(struct net_device *ndev)
{
	struct ql_adapter *qdev = netdev_priv(ndev);
	ql_queue_asic_error(qdev);
}

static void ql_asic_reset_work(struct work_struct *work)
{
	struct ql_adapter *qdev =
	    container_of(work, struct ql_adapter, asic_reset_work.work);
	int status;
	rtnl_lock();
	status = ql_adapter_down(qdev);
	if (status)
		goto error;

	status = ql_adapter_up(qdev);
	if (status)
		goto error;

	
	clear_bit(QL_ALLMULTI, &qdev->flags);
	clear_bit(QL_PROMISCUOUS, &qdev->flags);
	qlge_set_multicast_list(qdev->ndev);

	rtnl_unlock();
	return;
error:
	netif_alert(qdev, ifup, qdev->ndev,
		    "Driver up/down cycle failed, closing device\n");

	set_bit(QL_ADAPTER_UP, &qdev->flags);
	dev_close(qdev->ndev);
	rtnl_unlock();
}

static const struct nic_operations qla8012_nic_ops = {
	.get_flash		= ql_get_8012_flash_params,
	.port_initialize	= ql_8012_port_initialize,
};

static const struct nic_operations qla8000_nic_ops = {
	.get_flash		= ql_get_8000_flash_params,
	.port_initialize	= ql_8000_port_initialize,
};

static int ql_get_alt_pcie_func(struct ql_adapter *qdev)
{
	int status = 0;
	u32 temp;
	u32 nic_func1, nic_func2;

	status = ql_read_mpi_reg(qdev, MPI_TEST_FUNC_PORT_CFG,
			&temp);
	if (status)
		return status;

	nic_func1 = ((temp >> MPI_TEST_NIC1_FUNC_SHIFT) &
			MPI_TEST_NIC_FUNC_MASK);
	nic_func2 = ((temp >> MPI_TEST_NIC2_FUNC_SHIFT) &
			MPI_TEST_NIC_FUNC_MASK);

	if (qdev->func == nic_func1)
		qdev->alt_func = nic_func2;
	else if (qdev->func == nic_func2)
		qdev->alt_func = nic_func1;
	else
		status = -EIO;

	return status;
}

static int ql_get_board_info(struct ql_adapter *qdev)
{
	int status;
	qdev->func =
	    (ql_read32(qdev, STS) & STS_FUNC_ID_MASK) >> STS_FUNC_ID_SHIFT;
	if (qdev->func > 3)
		return -EIO;

	status = ql_get_alt_pcie_func(qdev);
	if (status)
		return status;

	qdev->port = (qdev->func < qdev->alt_func) ? 0 : 1;
	if (qdev->port) {
		qdev->xg_sem_mask = SEM_XGMAC1_MASK;
		qdev->port_link_up = STS_PL1;
		qdev->port_init = STS_PI1;
		qdev->mailbox_in = PROC_ADDR_MPI_RISC | PROC_ADDR_FUNC2_MBI;
		qdev->mailbox_out = PROC_ADDR_MPI_RISC | PROC_ADDR_FUNC2_MBO;
	} else {
		qdev->xg_sem_mask = SEM_XGMAC0_MASK;
		qdev->port_link_up = STS_PL0;
		qdev->port_init = STS_PI0;
		qdev->mailbox_in = PROC_ADDR_MPI_RISC | PROC_ADDR_FUNC0_MBI;
		qdev->mailbox_out = PROC_ADDR_MPI_RISC | PROC_ADDR_FUNC0_MBO;
	}
	qdev->chip_rev_id = ql_read32(qdev, REV_ID);
	qdev->device_id = qdev->pdev->device;
	if (qdev->device_id == QLGE_DEVICE_ID_8012)
		qdev->nic_ops = &qla8012_nic_ops;
	else if (qdev->device_id == QLGE_DEVICE_ID_8000)
		qdev->nic_ops = &qla8000_nic_ops;
	return status;
}

static void ql_release_all(struct pci_dev *pdev)
{
	struct net_device *ndev = pci_get_drvdata(pdev);
	struct ql_adapter *qdev = netdev_priv(ndev);

	if (qdev->workqueue) {
		destroy_workqueue(qdev->workqueue);
		qdev->workqueue = NULL;
	}

	if (qdev->reg_base)
		iounmap(qdev->reg_base);
	if (qdev->doorbell_area)
		iounmap(qdev->doorbell_area);
	vfree(qdev->mpi_coredump);
	pci_release_regions(pdev);
	pci_set_drvdata(pdev, NULL);
}

static int __devinit ql_init_device(struct pci_dev *pdev,
				    struct net_device *ndev, int cards_found)
{
	struct ql_adapter *qdev = netdev_priv(ndev);
	int err = 0;

	memset((void *)qdev, 0, sizeof(*qdev));
	err = pci_enable_device(pdev);
	if (err) {
		dev_err(&pdev->dev, "PCI device enable failed.\n");
		return err;
	}

	qdev->ndev = ndev;
	qdev->pdev = pdev;
	pci_set_drvdata(pdev, ndev);

	
	err = pcie_set_readrq(pdev, 4096);
	if (err) {
		dev_err(&pdev->dev, "Set readrq failed.\n");
		goto err_out1;
	}

	err = pci_request_regions(pdev, DRV_NAME);
	if (err) {
		dev_err(&pdev->dev, "PCI region request failed.\n");
		return err;
	}

	pci_set_master(pdev);
	if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(64))) {
		set_bit(QL_DMA64, &qdev->flags);
		err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64));
	} else {
		err = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
		if (!err)
		       err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
	}

	if (err) {
		dev_err(&pdev->dev, "No usable DMA configuration.\n");
		goto err_out2;
	}

	
	pdev->needs_freset = 1;
	pci_save_state(pdev);
	qdev->reg_base =
	    ioremap_nocache(pci_resource_start(pdev, 1),
			    pci_resource_len(pdev, 1));
	if (!qdev->reg_base) {
		dev_err(&pdev->dev, "Register mapping failed.\n");
		err = -ENOMEM;
		goto err_out2;
	}

	qdev->doorbell_area_size = pci_resource_len(pdev, 3);
	qdev->doorbell_area =
	    ioremap_nocache(pci_resource_start(pdev, 3),
			    pci_resource_len(pdev, 3));
	if (!qdev->doorbell_area) {
		dev_err(&pdev->dev, "Doorbell register mapping failed.\n");
		err = -ENOMEM;
		goto err_out2;
	}

	err = ql_get_board_info(qdev);
	if (err) {
		dev_err(&pdev->dev, "Register access failed.\n");
		err = -EIO;
		goto err_out2;
	}
	qdev->msg_enable = netif_msg_init(debug, default_msg);
	spin_lock_init(&qdev->hw_lock);
	spin_lock_init(&qdev->stats_lock);

	if (qlge_mpi_coredump) {
		qdev->mpi_coredump =
			vmalloc(sizeof(struct ql_mpi_coredump));
		if (qdev->mpi_coredump == NULL) {
			dev_err(&pdev->dev, "Coredump alloc failed.\n");
			err = -ENOMEM;
			goto err_out2;
		}
		if (qlge_force_coredump)
			set_bit(QL_FRC_COREDUMP, &qdev->flags);
	}
	
	err = qdev->nic_ops->get_flash(qdev);
	if (err) {
		dev_err(&pdev->dev, "Invalid FLASH.\n");
		goto err_out2;
	}

	memcpy(ndev->perm_addr, ndev->dev_addr, ndev->addr_len);
	
	memcpy(qdev->current_mac_addr, ndev->dev_addr, ndev->addr_len);

	
	qdev->tx_ring_size = NUM_TX_RING_ENTRIES;
	qdev->rx_ring_size = NUM_RX_RING_ENTRIES;

	
	qdev->rx_coalesce_usecs = DFLT_COALESCE_WAIT;
	qdev->tx_coalesce_usecs = DFLT_COALESCE_WAIT;
	qdev->rx_max_coalesced_frames = DFLT_INTER_FRAME_WAIT;
	qdev->tx_max_coalesced_frames = DFLT_INTER_FRAME_WAIT;

	qdev->workqueue = create_singlethread_workqueue(ndev->name);
	INIT_DELAYED_WORK(&qdev->asic_reset_work, ql_asic_reset_work);
	INIT_DELAYED_WORK(&qdev->mpi_reset_work, ql_mpi_reset_work);
	INIT_DELAYED_WORK(&qdev->mpi_work, ql_mpi_work);
	INIT_DELAYED_WORK(&qdev->mpi_port_cfg_work, ql_mpi_port_cfg_work);
	INIT_DELAYED_WORK(&qdev->mpi_idc_work, ql_mpi_idc_work);
	INIT_DELAYED_WORK(&qdev->mpi_core_to_log, ql_mpi_core_to_log);
	init_completion(&qdev->ide_completion);
	mutex_init(&qdev->mpi_mutex);

	if (!cards_found) {
		dev_info(&pdev->dev, "%s\n", DRV_STRING);
		dev_info(&pdev->dev, "Driver name: %s, Version: %s.\n",
			 DRV_NAME, DRV_VERSION);
	}
	return 0;
err_out2:
	ql_release_all(pdev);
err_out1:
	pci_disable_device(pdev);
	return err;
}

static const struct net_device_ops qlge_netdev_ops = {
	.ndo_open		= qlge_open,
	.ndo_stop		= qlge_close,
	.ndo_start_xmit		= qlge_send,
	.ndo_change_mtu		= qlge_change_mtu,
	.ndo_get_stats		= qlge_get_stats,
	.ndo_set_rx_mode	= qlge_set_multicast_list,
	.ndo_set_mac_address	= qlge_set_mac_address,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_tx_timeout		= qlge_tx_timeout,
	.ndo_fix_features	= qlge_fix_features,
	.ndo_set_features	= qlge_set_features,
	.ndo_vlan_rx_add_vid	= qlge_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid	= qlge_vlan_rx_kill_vid,
};

static void ql_timer(unsigned long data)
{
	struct ql_adapter *qdev = (struct ql_adapter *)data;
	u32 var = 0;

	var = ql_read32(qdev, STS);
	if (pci_channel_offline(qdev->pdev)) {
		netif_err(qdev, ifup, qdev->ndev, "EEH STS = 0x%.08x.\n", var);
		return;
	}

	mod_timer(&qdev->timer, jiffies + (5*HZ));
}

static int __devinit qlge_probe(struct pci_dev *pdev,
				const struct pci_device_id *pci_entry)
{
	struct net_device *ndev = NULL;
	struct ql_adapter *qdev = NULL;
	static int cards_found = 0;
	int err = 0;

	ndev = alloc_etherdev_mq(sizeof(struct ql_adapter),
			min(MAX_CPUS, (int)num_online_cpus()));
	if (!ndev)
		return -ENOMEM;

	err = ql_init_device(pdev, ndev, cards_found);
	if (err < 0) {
		free_netdev(ndev);
		return err;
	}

	qdev = netdev_priv(ndev);
	SET_NETDEV_DEV(ndev, &pdev->dev);
	ndev->hw_features = NETIF_F_SG | NETIF_F_IP_CSUM |
		NETIF_F_TSO | NETIF_F_TSO6 | NETIF_F_TSO_ECN |
		NETIF_F_HW_VLAN_TX | NETIF_F_RXCSUM;
	ndev->features = ndev->hw_features |
		NETIF_F_HW_VLAN_RX | NETIF_F_HW_VLAN_FILTER;

	if (test_bit(QL_DMA64, &qdev->flags))
		ndev->features |= NETIF_F_HIGHDMA;

	ndev->tx_queue_len = qdev->tx_ring_size;
	ndev->irq = pdev->irq;

	ndev->netdev_ops = &qlge_netdev_ops;
	SET_ETHTOOL_OPS(ndev, &qlge_ethtool_ops);
	ndev->watchdog_timeo = 10 * HZ;

	err = register_netdev(ndev);
	if (err) {
		dev_err(&pdev->dev, "net device registration failed.\n");
		ql_release_all(pdev);
		pci_disable_device(pdev);
		return err;
	}
	init_timer_deferrable(&qdev->timer);
	qdev->timer.data = (unsigned long)qdev;
	qdev->timer.function = ql_timer;
	qdev->timer.expires = jiffies + (5*HZ);
	add_timer(&qdev->timer);
	ql_link_off(qdev);
	ql_display_dev_info(ndev);
	atomic_set(&qdev->lb_count, 0);
	cards_found++;
	return 0;
}

netdev_tx_t ql_lb_send(struct sk_buff *skb, struct net_device *ndev)
{
	return qlge_send(skb, ndev);
}

int ql_clean_lb_rx_ring(struct rx_ring *rx_ring, int budget)
{
	return ql_clean_inbound_rx_ring(rx_ring, budget);
}

static void __devexit qlge_remove(struct pci_dev *pdev)
{
	struct net_device *ndev = pci_get_drvdata(pdev);
	struct ql_adapter *qdev = netdev_priv(ndev);
	del_timer_sync(&qdev->timer);
	ql_cancel_all_work_sync(qdev);
	unregister_netdev(ndev);
	ql_release_all(pdev);
	pci_disable_device(pdev);
	free_netdev(ndev);
}

static void ql_eeh_close(struct net_device *ndev)
{
	int i;
	struct ql_adapter *qdev = netdev_priv(ndev);

	if (netif_carrier_ok(ndev)) {
		netif_carrier_off(ndev);
		netif_stop_queue(ndev);
	}

	
	del_timer_sync(&qdev->timer);
	ql_cancel_all_work_sync(qdev);

	for (i = 0; i < qdev->rss_ring_count; i++)
		netif_napi_del(&qdev->rx_ring[i].napi);

	clear_bit(QL_ADAPTER_UP, &qdev->flags);
	ql_tx_ring_clean(qdev);
	ql_free_rx_buffers(qdev);
	ql_release_adapter_resources(qdev);
}

static pci_ers_result_t qlge_io_error_detected(struct pci_dev *pdev,
					       enum pci_channel_state state)
{
	struct net_device *ndev = pci_get_drvdata(pdev);
	struct ql_adapter *qdev = netdev_priv(ndev);

	switch (state) {
	case pci_channel_io_normal:
		return PCI_ERS_RESULT_CAN_RECOVER;
	case pci_channel_io_frozen:
		netif_device_detach(ndev);
		if (netif_running(ndev))
			ql_eeh_close(ndev);
		pci_disable_device(pdev);
		return PCI_ERS_RESULT_NEED_RESET;
	case pci_channel_io_perm_failure:
		dev_err(&pdev->dev,
			"%s: pci_channel_io_perm_failure.\n", __func__);
		ql_eeh_close(ndev);
		set_bit(QL_EEH_FATAL, &qdev->flags);
		return PCI_ERS_RESULT_DISCONNECT;
	}

	
	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t qlge_io_slot_reset(struct pci_dev *pdev)
{
	struct net_device *ndev = pci_get_drvdata(pdev);
	struct ql_adapter *qdev = netdev_priv(ndev);

	pdev->error_state = pci_channel_io_normal;

	pci_restore_state(pdev);
	if (pci_enable_device(pdev)) {
		netif_err(qdev, ifup, qdev->ndev,
			  "Cannot re-enable PCI device after reset.\n");
		return PCI_ERS_RESULT_DISCONNECT;
	}
	pci_set_master(pdev);

	if (ql_adapter_reset(qdev)) {
		netif_err(qdev, drv, qdev->ndev, "reset FAILED!\n");
		set_bit(QL_EEH_FATAL, &qdev->flags);
		return PCI_ERS_RESULT_DISCONNECT;
	}

	return PCI_ERS_RESULT_RECOVERED;
}

static void qlge_io_resume(struct pci_dev *pdev)
{
	struct net_device *ndev = pci_get_drvdata(pdev);
	struct ql_adapter *qdev = netdev_priv(ndev);
	int err = 0;

	if (netif_running(ndev)) {
		err = qlge_open(ndev);
		if (err) {
			netif_err(qdev, ifup, qdev->ndev,
				  "Device initialization failed after reset.\n");
			return;
		}
	} else {
		netif_err(qdev, ifup, qdev->ndev,
			  "Device was not running prior to EEH.\n");
	}
	mod_timer(&qdev->timer, jiffies + (5*HZ));
	netif_device_attach(ndev);
}

static struct pci_error_handlers qlge_err_handler = {
	.error_detected = qlge_io_error_detected,
	.slot_reset = qlge_io_slot_reset,
	.resume = qlge_io_resume,
};

static int qlge_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct net_device *ndev = pci_get_drvdata(pdev);
	struct ql_adapter *qdev = netdev_priv(ndev);
	int err;

	netif_device_detach(ndev);
	del_timer_sync(&qdev->timer);

	if (netif_running(ndev)) {
		err = ql_adapter_down(qdev);
		if (!err)
			return err;
	}

	ql_wol(qdev);
	err = pci_save_state(pdev);
	if (err)
		return err;

	pci_disable_device(pdev);

	pci_set_power_state(pdev, pci_choose_state(pdev, state));

	return 0;
}

#ifdef CONFIG_PM
static int qlge_resume(struct pci_dev *pdev)
{
	struct net_device *ndev = pci_get_drvdata(pdev);
	struct ql_adapter *qdev = netdev_priv(ndev);
	int err;

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);
	err = pci_enable_device(pdev);
	if (err) {
		netif_err(qdev, ifup, qdev->ndev, "Cannot enable PCI device from suspend\n");
		return err;
	}
	pci_set_master(pdev);

	pci_enable_wake(pdev, PCI_D3hot, 0);
	pci_enable_wake(pdev, PCI_D3cold, 0);

	if (netif_running(ndev)) {
		err = ql_adapter_up(qdev);
		if (err)
			return err;
	}

	mod_timer(&qdev->timer, jiffies + (5*HZ));
	netif_device_attach(ndev);

	return 0;
}
#endif 

static void qlge_shutdown(struct pci_dev *pdev)
{
	qlge_suspend(pdev, PMSG_SUSPEND);
}

static struct pci_driver qlge_driver = {
	.name = DRV_NAME,
	.id_table = qlge_pci_tbl,
	.probe = qlge_probe,
	.remove = __devexit_p(qlge_remove),
#ifdef CONFIG_PM
	.suspend = qlge_suspend,
	.resume = qlge_resume,
#endif
	.shutdown = qlge_shutdown,
	.err_handler = &qlge_err_handler
};

static int __init qlge_init_module(void)
{
	return pci_register_driver(&qlge_driver);
}

static void __exit qlge_exit(void)
{
	pci_unregister_driver(&qlge_driver);
}

module_init(qlge_init_module);
module_exit(qlge_exit);
