/*
 * Tehuti Networks(R) Network Driver
 * ethtool interface implementation
 * Copyright (C) 2007 Tehuti Networks Ltd. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include "tehuti.h"

static DEFINE_PCI_DEVICE_TABLE(bdx_pci_tbl) = {
	{ PCI_VDEVICE(TEHUTI, 0x3009), },
	{ PCI_VDEVICE(TEHUTI, 0x3010), },
	{ PCI_VDEVICE(TEHUTI, 0x3014), },
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, bdx_pci_tbl);

static void bdx_rx_alloc_skbs(struct bdx_priv *priv, struct rxf_fifo *f);
static void bdx_tx_cleanup(struct bdx_priv *priv);
static int bdx_rx_receive(struct bdx_priv *priv, struct rxd_fifo *f, int budget);

static void bdx_tx_push_desc_safe(struct bdx_priv *priv, void *data, int size);

static int bdx_tx_init(struct bdx_priv *priv);
static int bdx_rx_init(struct bdx_priv *priv);

static void bdx_rx_free(struct bdx_priv *priv);
static void bdx_tx_free(struct bdx_priv *priv);

static void bdx_set_ethtool_ops(struct net_device *netdev);


static void print_hw_id(struct pci_dev *pdev)
{
	struct pci_nic *nic = pci_get_drvdata(pdev);
	u16 pci_link_status = 0;
	u16 pci_ctrl = 0;

	pci_read_config_word(pdev, PCI_LINK_STATUS_REG, &pci_link_status);
	pci_read_config_word(pdev, PCI_DEV_CTRL_REG, &pci_ctrl);

	pr_info("%s%s\n", BDX_NIC_NAME,
		nic->port_num == 1 ? "" : ", 2-Port");
	pr_info("srom 0x%x fpga %d build %u lane# %d max_pl 0x%x mrrs 0x%x\n",
		readl(nic->regs + SROM_VER), readl(nic->regs + FPGA_VER) & 0xFFF,
		readl(nic->regs + FPGA_SEED),
		GET_LINK_STATUS_LANES(pci_link_status),
		GET_DEV_CTRL_MAXPL(pci_ctrl), GET_DEV_CTRL_MRRS(pci_ctrl));
}

static void print_fw_id(struct pci_nic *nic)
{
	pr_info("fw 0x%x\n", readl(nic->regs + FW_VER));
}

static void print_eth_id(struct net_device *ndev)
{
	netdev_info(ndev, "%s, Port %c\n",
		    BDX_NIC_NAME, (ndev->if_port == 0) ? 'A' : 'B');

}


#define bdx_enable_interrupts(priv)	\
	do { WRITE_REG(priv, regIMR, IR_RUN); } while (0)
#define bdx_disable_interrupts(priv)	\
	do { WRITE_REG(priv, regIMR, 0); } while (0)

static int
bdx_fifo_init(struct bdx_priv *priv, struct fifo *f, int fsz_type,
	      u16 reg_CFG0, u16 reg_CFG1, u16 reg_RPTR, u16 reg_WPTR)
{
	u16 memsz = FIFO_SIZE * (1 << fsz_type);

	memset(f, 0, sizeof(struct fifo));
	
	f->va = pci_alloc_consistent(priv->pdev,
				     memsz + FIFO_EXTRA_SPACE, &f->da);
	if (!f->va) {
		pr_err("pci_alloc_consistent failed\n");
		RET(-ENOMEM);
	}
	f->reg_CFG0 = reg_CFG0;
	f->reg_CFG1 = reg_CFG1;
	f->reg_RPTR = reg_RPTR;
	f->reg_WPTR = reg_WPTR;
	f->rptr = 0;
	f->wptr = 0;
	f->memsz = memsz;
	f->size_mask = memsz - 1;
	WRITE_REG(priv, reg_CFG0, (u32) ((f->da & TX_RX_CFG0_BASE) | fsz_type));
	WRITE_REG(priv, reg_CFG1, H32_64(f->da));

	RET(0);
}

static void bdx_fifo_free(struct bdx_priv *priv, struct fifo *f)
{
	ENTER;
	if (f->va) {
		pci_free_consistent(priv->pdev,
				    f->memsz + FIFO_EXTRA_SPACE, f->va, f->da);
		f->va = NULL;
	}
	RET();
}

static void bdx_link_changed(struct bdx_priv *priv)
{
	u32 link = READ_REG(priv, regMAC_LNK_STAT) & MAC_LINK_STAT;

	if (!link) {
		if (netif_carrier_ok(priv->ndev)) {
			netif_stop_queue(priv->ndev);
			netif_carrier_off(priv->ndev);
			netdev_err(priv->ndev, "Link Down\n");
		}
	} else {
		if (!netif_carrier_ok(priv->ndev)) {
			netif_wake_queue(priv->ndev);
			netif_carrier_on(priv->ndev);
			netdev_err(priv->ndev, "Link Up\n");
		}
	}
}

static void bdx_isr_extra(struct bdx_priv *priv, u32 isr)
{
	if (isr & IR_RX_FREE_0) {
		bdx_rx_alloc_skbs(priv, &priv->rxf_fifo0);
		DBG("RX_FREE_0\n");
	}

	if (isr & IR_LNKCHG0)
		bdx_link_changed(priv);

	if (isr & IR_PCIE_LINK)
		netdev_err(priv->ndev, "PCI-E Link Fault\n");

	if (isr & IR_PCIE_TOUT)
		netdev_err(priv->ndev, "PCI-E Time Out\n");

}


static irqreturn_t bdx_isr_napi(int irq, void *dev)
{
	struct net_device *ndev = dev;
	struct bdx_priv *priv = netdev_priv(ndev);
	u32 isr;

	ENTER;
	isr = (READ_REG(priv, regISR) & IR_RUN);
	if (unlikely(!isr)) {
		bdx_enable_interrupts(priv);
		return IRQ_NONE;	
	}

	if (isr & IR_EXTRA)
		bdx_isr_extra(priv, isr);

	if (isr & (IR_RX_DESC_0 | IR_TX_FREE_0)) {
		if (likely(napi_schedule_prep(&priv->napi))) {
			__napi_schedule(&priv->napi);
			RET(IRQ_HANDLED);
		} else {
			READ_REG(priv, regTXF_WPTR_0);
			READ_REG(priv, regRXD_WPTR_0);
		}
	}

	bdx_enable_interrupts(priv);
	RET(IRQ_HANDLED);
}

static int bdx_poll(struct napi_struct *napi, int budget)
{
	struct bdx_priv *priv = container_of(napi, struct bdx_priv, napi);
	int work_done;

	ENTER;
	bdx_tx_cleanup(priv);
	work_done = bdx_rx_receive(priv, &priv->rxd_fifo0, budget);
	if ((work_done < budget) ||
	    (priv->napi_stop++ >= 30)) {
		DBG("rx poll is done. backing to isr-driven\n");

		priv->napi_stop = 0;

		napi_complete(napi);
		bdx_enable_interrupts(priv);
	}
	return work_done;
}


static int bdx_fw_load(struct bdx_priv *priv)
{
	const struct firmware *fw = NULL;
	int master, i;
	int rc;

	ENTER;
	master = READ_REG(priv, regINIT_SEMAPHORE);
	if (!READ_REG(priv, regINIT_STATUS) && master) {
		rc = request_firmware(&fw, "tehuti/bdx.bin", &priv->pdev->dev);
		if (rc)
			goto out;
		bdx_tx_push_desc_safe(priv, (char *)fw->data, fw->size);
		mdelay(100);
	}
	for (i = 0; i < 200; i++) {
		if (READ_REG(priv, regINIT_STATUS)) {
			rc = 0;
			goto out;
		}
		mdelay(2);
	}
	rc = -EIO;
out:
	if (master)
		WRITE_REG(priv, regINIT_SEMAPHORE, 1);
	if (fw)
		release_firmware(fw);

	if (rc) {
		netdev_err(priv->ndev, "firmware loading failed\n");
		if (rc == -EIO)
			DBG("VPC = 0x%x VIC = 0x%x INIT_STATUS = 0x%x i=%d\n",
			    READ_REG(priv, regVPC),
			    READ_REG(priv, regVIC),
			    READ_REG(priv, regINIT_STATUS), i);
		RET(rc);
	} else {
		DBG("%s: firmware loading success\n", priv->ndev->name);
		RET(0);
	}
}

static void bdx_restore_mac(struct net_device *ndev, struct bdx_priv *priv)
{
	u32 val;

	ENTER;
	DBG("mac0=%x mac1=%x mac2=%x\n",
	    READ_REG(priv, regUNC_MAC0_A),
	    READ_REG(priv, regUNC_MAC1_A), READ_REG(priv, regUNC_MAC2_A));

	val = (ndev->dev_addr[0] << 8) | (ndev->dev_addr[1]);
	WRITE_REG(priv, regUNC_MAC2_A, val);
	val = (ndev->dev_addr[2] << 8) | (ndev->dev_addr[3]);
	WRITE_REG(priv, regUNC_MAC1_A, val);
	val = (ndev->dev_addr[4] << 8) | (ndev->dev_addr[5]);
	WRITE_REG(priv, regUNC_MAC0_A, val);

	DBG("mac0=%x mac1=%x mac2=%x\n",
	    READ_REG(priv, regUNC_MAC0_A),
	    READ_REG(priv, regUNC_MAC1_A), READ_REG(priv, regUNC_MAC2_A));
	RET();
}

static int bdx_hw_start(struct bdx_priv *priv)
{
	int rc = -EIO;
	struct net_device *ndev = priv->ndev;

	ENTER;
	bdx_link_changed(priv);

	
	WRITE_REG(priv, regFRM_LENGTH, 0X3FE0);
	WRITE_REG(priv, regPAUSE_QUANT, 0x96);
	WRITE_REG(priv, regRX_FIFO_SECTION, 0x800010);
	WRITE_REG(priv, regTX_FIFO_SECTION, 0xE00010);
	WRITE_REG(priv, regRX_FULLNESS, 0);
	WRITE_REG(priv, regTX_FULLNESS, 0);
	WRITE_REG(priv, regCTRLST,
		  regCTRLST_BASE | regCTRLST_RX_ENA | regCTRLST_TX_ENA);

	WRITE_REG(priv, regVGLB, 0);
	WRITE_REG(priv, regMAX_FRAME_A,
		  priv->rxf_fifo0.m.pktsz & MAX_FRAME_AB_VAL);

	DBG("RDINTCM=%08x\n", priv->rdintcm);	
	WRITE_REG(priv, regRDINTCM0, priv->rdintcm);
	WRITE_REG(priv, regRDINTCM2, 0);	

	DBG("TDINTCM=%08x\n", priv->tdintcm);	
	WRITE_REG(priv, regTDINTCM0, priv->tdintcm);	

	
	
	bdx_restore_mac(priv->ndev, priv);

	WRITE_REG(priv, regGMAC_RXF_A, GMAC_RX_FILTER_OSEN |
		  GMAC_RX_FILTER_AM | GMAC_RX_FILTER_AB);

#define BDX_IRQ_TYPE	((priv->nic->irq_type == IRQ_MSI) ? 0 : IRQF_SHARED)

	rc = request_irq(priv->pdev->irq, bdx_isr_napi, BDX_IRQ_TYPE,
			 ndev->name, ndev);
	if (rc)
		goto err_irq;
	bdx_enable_interrupts(priv);

	RET(0);

err_irq:
	RET(rc);
}

static void bdx_hw_stop(struct bdx_priv *priv)
{
	ENTER;
	bdx_disable_interrupts(priv);
	free_irq(priv->pdev->irq, priv->ndev);

	netif_carrier_off(priv->ndev);
	netif_stop_queue(priv->ndev);

	RET();
}

static int bdx_hw_reset_direct(void __iomem *regs)
{
	u32 val, i;
	ENTER;

	
	val = readl(regs + regCLKPLL);
	writel((val | CLKPLL_SFTRST) + 0x8, regs + regCLKPLL);
	udelay(50);
	val = readl(regs + regCLKPLL);
	writel(val & ~CLKPLL_SFTRST, regs + regCLKPLL);

	
	for (i = 0; i < 70; i++, mdelay(10))
		if ((readl(regs + regCLKPLL) & CLKPLL_LKD) == CLKPLL_LKD) {
			
			readl(regs + regRXD_CFG0_0);
			return 0;
		}
	pr_err("HW reset failed\n");
	return 1;		
}

static int bdx_hw_reset(struct bdx_priv *priv)
{
	u32 val, i;
	ENTER;

	if (priv->port == 0) {
		
		val = READ_REG(priv, regCLKPLL);
		WRITE_REG(priv, regCLKPLL, (val | CLKPLL_SFTRST) + 0x8);
		udelay(50);
		val = READ_REG(priv, regCLKPLL);
		WRITE_REG(priv, regCLKPLL, val & ~CLKPLL_SFTRST);
	}
	
	for (i = 0; i < 70; i++, mdelay(10))
		if ((READ_REG(priv, regCLKPLL) & CLKPLL_LKD) == CLKPLL_LKD) {
			
			READ_REG(priv, regRXD_CFG0_0);
			return 0;
		}
	pr_err("HW reset failed\n");
	return 1;		
}

static int bdx_sw_reset(struct bdx_priv *priv)
{
	int i;

	ENTER;
	
	
	WRITE_REG(priv, regGMAC_RXF_A, 0);
	mdelay(100);
	
	WRITE_REG(priv, regDIS_PORT, 1);
	
	WRITE_REG(priv, regDIS_QU, 1);
	
	for (i = 0; i < 50; i++) {
		if (READ_REG(priv, regRST_PORT) & 1)
			break;
		mdelay(10);
	}
	if (i == 50)
		netdev_err(priv->ndev, "SW reset timeout. continuing anyway\n");

	
	WRITE_REG(priv, regRDINTCM0, 0);
	WRITE_REG(priv, regTDINTCM0, 0);
	WRITE_REG(priv, regIMR, 0);
	READ_REG(priv, regISR);

	
	WRITE_REG(priv, regRST_QU, 1);
	
	WRITE_REG(priv, regRST_PORT, 1);
	
	for (i = regTXD_WPTR_0; i <= regTXF_RPTR_3; i += 0x10)
		DBG("%x = %x\n", i, READ_REG(priv, i) & TXF_WPTR_WR_PTR);
	for (i = regTXD_WPTR_0; i <= regTXF_RPTR_3; i += 0x10)
		WRITE_REG(priv, i, 0);
	
	WRITE_REG(priv, regDIS_PORT, 0);
	
	WRITE_REG(priv, regDIS_QU, 0);
	
	WRITE_REG(priv, regRST_QU, 0);
	
	WRITE_REG(priv, regRST_PORT, 0);
	
	
	
	for (i = regTXD_WPTR_0; i <= regTXF_RPTR_3; i += 0x10)
		DBG("%x = %x\n", i, READ_REG(priv, i) & TXF_WPTR_WR_PTR);

	RET(0);
}

static int bdx_reset(struct bdx_priv *priv)
{
	ENTER;
	RET((priv->pdev->device == 0x3009)
	    ? bdx_hw_reset(priv)
	    : bdx_sw_reset(priv));
}

static int bdx_close(struct net_device *ndev)
{
	struct bdx_priv *priv = NULL;

	ENTER;
	priv = netdev_priv(ndev);

	napi_disable(&priv->napi);

	bdx_reset(priv);
	bdx_hw_stop(priv);
	bdx_rx_free(priv);
	bdx_tx_free(priv);
	RET(0);
}

static int bdx_open(struct net_device *ndev)
{
	struct bdx_priv *priv;
	int rc;

	ENTER;
	priv = netdev_priv(ndev);
	bdx_reset(priv);
	if (netif_running(ndev))
		netif_stop_queue(priv->ndev);

	if ((rc = bdx_tx_init(priv)) ||
	    (rc = bdx_rx_init(priv)) ||
	    (rc = bdx_fw_load(priv)))
		goto err;

	bdx_rx_alloc_skbs(priv, &priv->rxf_fifo0);

	rc = bdx_hw_start(priv);
	if (rc)
		goto err;

	napi_enable(&priv->napi);

	print_fw_id(priv->nic);

	RET(0);

err:
	bdx_close(ndev);
	RET(rc);
}

static int bdx_range_check(struct bdx_priv *priv, u32 offset)
{
	return (offset > (u32) (BDX_REGS_SIZE / priv->nic->port_num)) ?
		-EINVAL : 0;
}

static int bdx_ioctl_priv(struct net_device *ndev, struct ifreq *ifr, int cmd)
{
	struct bdx_priv *priv = netdev_priv(ndev);
	u32 data[3];
	int error;

	ENTER;

	DBG("jiffies=%ld cmd=%d\n", jiffies, cmd);
	if (cmd != SIOCDEVPRIVATE) {
		error = copy_from_user(data, ifr->ifr_data, sizeof(data));
		if (error) {
			pr_err("can't copy from user\n");
			RET(-EFAULT);
		}
		DBG("%d 0x%x 0x%x\n", data[0], data[1], data[2]);
	}

	if (!capable(CAP_SYS_RAWIO))
		return -EPERM;

	switch (data[0]) {

	case BDX_OP_READ:
		error = bdx_range_check(priv, data[1]);
		if (error < 0)
			return error;
		data[2] = READ_REG(priv, data[1]);
		DBG("read_reg(0x%x)=0x%x (dec %d)\n", data[1], data[2],
		    data[2]);
		error = copy_to_user(ifr->ifr_data, data, sizeof(data));
		if (error)
			RET(-EFAULT);
		break;

	case BDX_OP_WRITE:
		error = bdx_range_check(priv, data[1]);
		if (error < 0)
			return error;
		WRITE_REG(priv, data[1], data[2]);
		DBG("write_reg(0x%x, 0x%x)\n", data[1], data[2]);
		break;

	default:
		RET(-EOPNOTSUPP);
	}
	return 0;
}

static int bdx_ioctl(struct net_device *ndev, struct ifreq *ifr, int cmd)
{
	ENTER;
	if (cmd >= SIOCDEVPRIVATE && cmd <= (SIOCDEVPRIVATE + 15))
		RET(bdx_ioctl_priv(ndev, ifr, cmd));
	else
		RET(-EOPNOTSUPP);
}

static void __bdx_vlan_rx_vid(struct net_device *ndev, uint16_t vid, int enable)
{
	struct bdx_priv *priv = netdev_priv(ndev);
	u32 reg, bit, val;

	ENTER;
	DBG2("vid=%d value=%d\n", (int)vid, enable);
	if (unlikely(vid >= 4096)) {
		pr_err("invalid VID: %u (> 4096)\n", vid);
		RET();
	}
	reg = regVLAN_0 + (vid / 32) * 4;
	bit = 1 << vid % 32;
	val = READ_REG(priv, reg);
	DBG2("reg=%x, val=%x, bit=%d\n", reg, val, bit);
	if (enable)
		val |= bit;
	else
		val &= ~bit;
	DBG2("new val %x\n", val);
	WRITE_REG(priv, reg, val);
	RET();
}

static int bdx_vlan_rx_add_vid(struct net_device *ndev, uint16_t vid)
{
	__bdx_vlan_rx_vid(ndev, vid, 1);
	return 0;
}

static int bdx_vlan_rx_kill_vid(struct net_device *ndev, unsigned short vid)
{
	__bdx_vlan_rx_vid(ndev, vid, 0);
	return 0;
}

static int bdx_change_mtu(struct net_device *ndev, int new_mtu)
{
	ENTER;

	if (new_mtu == ndev->mtu)
		RET(0);

	
	if (new_mtu < ETH_ZLEN) {
		netdev_err(ndev, "mtu %d is less then minimal %d\n",
			   new_mtu, ETH_ZLEN);
		RET(-EINVAL);
	}

	ndev->mtu = new_mtu;
	if (netif_running(ndev)) {
		bdx_close(ndev);
		bdx_open(ndev);
	}
	RET(0);
}

static void bdx_setmulti(struct net_device *ndev)
{
	struct bdx_priv *priv = netdev_priv(ndev);

	u32 rxf_val =
	    GMAC_RX_FILTER_AM | GMAC_RX_FILTER_AB | GMAC_RX_FILTER_OSEN;
	int i;

	ENTER;
	
	

	
	if (ndev->flags & IFF_PROMISC) {
		rxf_val |= GMAC_RX_FILTER_PRM;
	} else if (ndev->flags & IFF_ALLMULTI) {
		
		for (i = 0; i < MAC_MCST_HASH_NUM; i++)
			WRITE_REG(priv, regRX_MCST_HASH0 + i * 4, ~0);
	} else if (!netdev_mc_empty(ndev)) {
		u8 hash;
		struct netdev_hw_addr *ha;
		u32 reg, val;

		
		for (i = 0; i < MAC_MCST_HASH_NUM; i++)
			WRITE_REG(priv, regRX_MCST_HASH0 + i * 4, 0);
		
		for (i = 0; i < MAC_MCST_NUM; i++) {
			WRITE_REG(priv, regRX_MAC_MCST0 + i * 8, 0);
			WRITE_REG(priv, regRX_MAC_MCST1 + i * 8, 0);
		}

		
		
		netdev_for_each_mc_addr(ha, ndev) {
			hash = 0;
			for (i = 0; i < ETH_ALEN; i++)
				hash ^= ha->addr[i];
			reg = regRX_MCST_HASH0 + ((hash >> 5) << 2);
			val = READ_REG(priv, reg);
			val |= (1 << (hash % 32));
			WRITE_REG(priv, reg, val);
		}

	} else {
		DBG("only own mac %d\n", netdev_mc_count(ndev));
		rxf_val |= GMAC_RX_FILTER_AB;
	}
	WRITE_REG(priv, regGMAC_RXF_A, rxf_val);
	
	
	RET();
}

static int bdx_set_mac(struct net_device *ndev, void *p)
{
	struct bdx_priv *priv = netdev_priv(ndev);
	struct sockaddr *addr = p;

	ENTER;
	memcpy(ndev->dev_addr, addr->sa_data, ndev->addr_len);
	bdx_restore_mac(ndev, priv);
	RET(0);
}

static int bdx_read_mac(struct bdx_priv *priv)
{
	u16 macAddress[3], i;
	ENTER;

	macAddress[2] = READ_REG(priv, regUNC_MAC0_A);
	macAddress[2] = READ_REG(priv, regUNC_MAC0_A);
	macAddress[1] = READ_REG(priv, regUNC_MAC1_A);
	macAddress[1] = READ_REG(priv, regUNC_MAC1_A);
	macAddress[0] = READ_REG(priv, regUNC_MAC2_A);
	macAddress[0] = READ_REG(priv, regUNC_MAC2_A);
	for (i = 0; i < 3; i++) {
		priv->ndev->dev_addr[i * 2 + 1] = macAddress[i];
		priv->ndev->dev_addr[i * 2] = macAddress[i] >> 8;
	}
	RET(0);
}

static u64 bdx_read_l2stat(struct bdx_priv *priv, int reg)
{
	u64 val;

	val = READ_REG(priv, reg);
	val |= ((u64) READ_REG(priv, reg + 8)) << 32;
	return val;
}

static void bdx_update_stats(struct bdx_priv *priv)
{
	struct bdx_stats *stats = &priv->hw_stats;
	u64 *stats_vector = (u64 *) stats;
	int i;
	int addr;

	
	addr = 0x7200;
	
	for (i = 0; i < 12; i++) {
		stats_vector[i] = bdx_read_l2stat(priv, addr);
		addr += 0x10;
	}
	BDX_ASSERT(addr != 0x72C0);
	
	addr = 0x72F0;
	for (; i < 16; i++) {
		stats_vector[i] = bdx_read_l2stat(priv, addr);
		addr += 0x10;
	}
	BDX_ASSERT(addr != 0x7330);
	
	addr = 0x7370;
	for (; i < 19; i++) {
		stats_vector[i] = bdx_read_l2stat(priv, addr);
		addr += 0x10;
	}
	BDX_ASSERT(addr != 0x73A0);
	
	addr = 0x73C0;
	for (; i < 23; i++) {
		stats_vector[i] = bdx_read_l2stat(priv, addr);
		addr += 0x10;
	}
	BDX_ASSERT(addr != 0x7400);
	BDX_ASSERT((sizeof(struct bdx_stats) / sizeof(u64)) != i);
}

static void print_rxdd(struct rxd_desc *rxdd, u32 rxd_val1, u16 len,
		       u16 rxd_vlan);
static void print_rxfd(struct rxf_desc *rxfd);


static void bdx_rxdb_destroy(struct rxdb *db)
{
	vfree(db);
}

static struct rxdb *bdx_rxdb_create(int nelem)
{
	struct rxdb *db;
	int i;

	db = vmalloc(sizeof(struct rxdb)
		     + (nelem * sizeof(int))
		     + (nelem * sizeof(struct rx_map)));
	if (likely(db != NULL)) {
		db->stack = (int *)(db + 1);
		db->elems = (void *)(db->stack + nelem);
		db->nelem = nelem;
		db->top = nelem;
		for (i = 0; i < nelem; i++)
			db->stack[i] = nelem - i - 1;	
	}

	return db;
}

static inline int bdx_rxdb_alloc_elem(struct rxdb *db)
{
	BDX_ASSERT(db->top <= 0);
	return db->stack[--(db->top)];
}

static inline void *bdx_rxdb_addr_elem(struct rxdb *db, int n)
{
	BDX_ASSERT((n < 0) || (n >= db->nelem));
	return db->elems + n;
}

static inline int bdx_rxdb_available(struct rxdb *db)
{
	return db->top;
}

static inline void bdx_rxdb_free_elem(struct rxdb *db, int n)
{
	BDX_ASSERT((n >= db->nelem) || (n < 0));
	db->stack[(db->top)++] = n;
}




static int bdx_rx_init(struct bdx_priv *priv)
{
	ENTER;

	if (bdx_fifo_init(priv, &priv->rxd_fifo0.m, priv->rxd_size,
			  regRXD_CFG0_0, regRXD_CFG1_0,
			  regRXD_RPTR_0, regRXD_WPTR_0))
		goto err_mem;
	if (bdx_fifo_init(priv, &priv->rxf_fifo0.m, priv->rxf_size,
			  regRXF_CFG0_0, regRXF_CFG1_0,
			  regRXF_RPTR_0, regRXF_WPTR_0))
		goto err_mem;
	priv->rxdb = bdx_rxdb_create(priv->rxf_fifo0.m.memsz /
				     sizeof(struct rxf_desc));
	if (!priv->rxdb)
		goto err_mem;

	priv->rxf_fifo0.m.pktsz = priv->ndev->mtu + VLAN_ETH_HLEN;
	return 0;

err_mem:
	netdev_err(priv->ndev, "Rx init failed\n");
	return -ENOMEM;
}

static void bdx_rx_free_skbs(struct bdx_priv *priv, struct rxf_fifo *f)
{
	struct rx_map *dm;
	struct rxdb *db = priv->rxdb;
	u16 i;

	ENTER;
	DBG("total=%d free=%d busy=%d\n", db->nelem, bdx_rxdb_available(db),
	    db->nelem - bdx_rxdb_available(db));
	while (bdx_rxdb_available(db) > 0) {
		i = bdx_rxdb_alloc_elem(db);
		dm = bdx_rxdb_addr_elem(db, i);
		dm->dma = 0;
	}
	for (i = 0; i < db->nelem; i++) {
		dm = bdx_rxdb_addr_elem(db, i);
		if (dm->dma) {
			pci_unmap_single(priv->pdev,
					 dm->dma, f->m.pktsz,
					 PCI_DMA_FROMDEVICE);
			dev_kfree_skb(dm->skb);
		}
	}
}

static void bdx_rx_free(struct bdx_priv *priv)
{
	ENTER;
	if (priv->rxdb) {
		bdx_rx_free_skbs(priv, &priv->rxf_fifo0);
		bdx_rxdb_destroy(priv->rxdb);
		priv->rxdb = NULL;
	}
	bdx_fifo_free(priv, &priv->rxf_fifo0.m);
	bdx_fifo_free(priv, &priv->rxd_fifo0.m);

	RET();
}



/* TBD: do not update WPTR if no desc were written */

static void bdx_rx_alloc_skbs(struct bdx_priv *priv, struct rxf_fifo *f)
{
	struct sk_buff *skb;
	struct rxf_desc *rxfd;
	struct rx_map *dm;
	int dno, delta, idx;
	struct rxdb *db = priv->rxdb;

	ENTER;
	dno = bdx_rxdb_available(db) - 1;
	while (dno > 0) {
		skb = netdev_alloc_skb(priv->ndev, f->m.pktsz + NET_IP_ALIGN);
		if (!skb) {
			pr_err("NO MEM: netdev_alloc_skb failed\n");
			break;
		}
		skb_reserve(skb, NET_IP_ALIGN);

		idx = bdx_rxdb_alloc_elem(db);
		dm = bdx_rxdb_addr_elem(db, idx);
		dm->dma = pci_map_single(priv->pdev,
					 skb->data, f->m.pktsz,
					 PCI_DMA_FROMDEVICE);
		dm->skb = skb;
		rxfd = (struct rxf_desc *)(f->m.va + f->m.wptr);
		rxfd->info = CPU_CHIP_SWAP32(0x10003);	
		rxfd->va_lo = idx;
		rxfd->pa_lo = CPU_CHIP_SWAP32(L32_64(dm->dma));
		rxfd->pa_hi = CPU_CHIP_SWAP32(H32_64(dm->dma));
		rxfd->len = CPU_CHIP_SWAP32(f->m.pktsz);
		print_rxfd(rxfd);

		f->m.wptr += sizeof(struct rxf_desc);
		delta = f->m.wptr - f->m.memsz;
		if (unlikely(delta >= 0)) {
			f->m.wptr = delta;
			if (delta > 0) {
				memcpy(f->m.va, f->m.va + f->m.memsz, delta);
				DBG("wrapped descriptor\n");
			}
		}
		dno--;
	}
	
	WRITE_REG(priv, f->m.reg_WPTR, f->m.wptr & TXF_WPTR_WR_PTR);
	RET();
}

static inline void
NETIF_RX_MUX(struct bdx_priv *priv, u32 rxd_val1, u16 rxd_vlan,
	     struct sk_buff *skb)
{
	ENTER;
	DBG("rxdd->flags.bits.vtag=%d\n", GET_RXD_VTAG(rxd_val1));
	if (GET_RXD_VTAG(rxd_val1)) {
		DBG("%s: vlan rcv vlan '%x' vtag '%x'\n",
		    priv->ndev->name,
		    GET_RXD_VLAN_ID(rxd_vlan),
		    GET_RXD_VTAG(rxd_val1));
		__vlan_hwaccel_put_tag(skb, GET_RXD_VLAN_TCI(rxd_vlan));
	}
	netif_receive_skb(skb);
}

static void bdx_recycle_skb(struct bdx_priv *priv, struct rxd_desc *rxdd)
{
	struct rxf_desc *rxfd;
	struct rx_map *dm;
	struct rxf_fifo *f;
	struct rxdb *db;
	struct sk_buff *skb;
	int delta;

	ENTER;
	DBG("priv=%p rxdd=%p\n", priv, rxdd);
	f = &priv->rxf_fifo0;
	db = priv->rxdb;
	DBG("db=%p f=%p\n", db, f);
	dm = bdx_rxdb_addr_elem(db, rxdd->va_lo);
	DBG("dm=%p\n", dm);
	skb = dm->skb;
	rxfd = (struct rxf_desc *)(f->m.va + f->m.wptr);
	rxfd->info = CPU_CHIP_SWAP32(0x10003);	
	rxfd->va_lo = rxdd->va_lo;
	rxfd->pa_lo = CPU_CHIP_SWAP32(L32_64(dm->dma));
	rxfd->pa_hi = CPU_CHIP_SWAP32(H32_64(dm->dma));
	rxfd->len = CPU_CHIP_SWAP32(f->m.pktsz);
	print_rxfd(rxfd);

	f->m.wptr += sizeof(struct rxf_desc);
	delta = f->m.wptr - f->m.memsz;
	if (unlikely(delta >= 0)) {
		f->m.wptr = delta;
		if (delta > 0) {
			memcpy(f->m.va, f->m.va + f->m.memsz, delta);
			DBG("wrapped descriptor\n");
		}
	}
	RET();
}



static int bdx_rx_receive(struct bdx_priv *priv, struct rxd_fifo *f, int budget)
{
	struct net_device *ndev = priv->ndev;
	struct sk_buff *skb, *skb2;
	struct rxd_desc *rxdd;
	struct rx_map *dm;
	struct rxf_fifo *rxf_fifo;
	int tmp_len, size;
	int done = 0;
	int max_done = BDX_MAX_RX_DONE;
	struct rxdb *db = NULL;
	
	u32 rxd_val1;
	u16 len;
	u16 rxd_vlan;

	ENTER;
	max_done = budget;

	f->m.wptr = READ_REG(priv, f->m.reg_WPTR) & TXF_WPTR_WR_PTR;

	size = f->m.wptr - f->m.rptr;
	if (size < 0)
		size = f->m.memsz + size;	

	while (size > 0) {

		rxdd = (struct rxd_desc *)(f->m.va + f->m.rptr);
		rxd_val1 = CPU_CHIP_SWAP32(rxdd->rxd_val1);

		len = CPU_CHIP_SWAP16(rxdd->len);

		rxd_vlan = CPU_CHIP_SWAP16(rxdd->rxd_vlan);

		print_rxdd(rxdd, rxd_val1, len, rxd_vlan);

		tmp_len = GET_RXD_BC(rxd_val1) << 3;
		BDX_ASSERT(tmp_len <= 0);
		size -= tmp_len;
		if (size < 0)	
			break;

		f->m.rptr += tmp_len;

		tmp_len = f->m.rptr - f->m.memsz;
		if (unlikely(tmp_len >= 0)) {
			f->m.rptr = tmp_len;
			if (tmp_len > 0) {
				DBG("wrapped desc rptr=%d tmp_len=%d\n",
				    f->m.rptr, tmp_len);
				memcpy(f->m.va + f->m.memsz, f->m.va, tmp_len);
			}
		}

		if (unlikely(GET_RXD_ERR(rxd_val1))) {
			DBG("rxd_err = 0x%x\n", GET_RXD_ERR(rxd_val1));
			ndev->stats.rx_errors++;
			bdx_recycle_skb(priv, rxdd);
			continue;
		}

		rxf_fifo = &priv->rxf_fifo0;
		db = priv->rxdb;
		dm = bdx_rxdb_addr_elem(db, rxdd->va_lo);
		skb = dm->skb;

		if (len < BDX_COPYBREAK &&
		    (skb2 = netdev_alloc_skb(priv->ndev, len + NET_IP_ALIGN))) {
			skb_reserve(skb2, NET_IP_ALIGN);
			
			pci_dma_sync_single_for_cpu(priv->pdev,
						    dm->dma, rxf_fifo->m.pktsz,
						    PCI_DMA_FROMDEVICE);
			memcpy(skb2->data, skb->data, len);
			bdx_recycle_skb(priv, rxdd);
			skb = skb2;
		} else {
			pci_unmap_single(priv->pdev,
					 dm->dma, rxf_fifo->m.pktsz,
					 PCI_DMA_FROMDEVICE);
			bdx_rxdb_free_elem(db, rxdd->va_lo);
		}

		ndev->stats.rx_bytes += len;

		skb_put(skb, len);
		skb->protocol = eth_type_trans(skb, ndev);

		
		if (GET_RXD_PKT_ID(rxd_val1) == 0)
			skb_checksum_none_assert(skb);
		else
			skb->ip_summed = CHECKSUM_UNNECESSARY;

		NETIF_RX_MUX(priv, rxd_val1, rxd_vlan, skb);

		if (++done >= max_done)
			break;
	}

	ndev->stats.rx_packets += done;

	
	WRITE_REG(priv, f->m.reg_RPTR, f->m.rptr & TXF_WPTR_WR_PTR);

	bdx_rx_alloc_skbs(priv, &priv->rxf_fifo0);

	RET(done);
}

static void print_rxdd(struct rxd_desc *rxdd, u32 rxd_val1, u16 len,
		       u16 rxd_vlan)
{
	DBG("ERROR: rxdd bc %d rxfq %d to %d type %d err %d rxp %d pkt_id %d vtag %d len %d vlan_id %d cfi %d prio %d va_lo %d va_hi %d\n",
	    GET_RXD_BC(rxd_val1), GET_RXD_RXFQ(rxd_val1), GET_RXD_TO(rxd_val1),
	    GET_RXD_TYPE(rxd_val1), GET_RXD_ERR(rxd_val1),
	    GET_RXD_RXP(rxd_val1), GET_RXD_PKT_ID(rxd_val1),
	    GET_RXD_VTAG(rxd_val1), len, GET_RXD_VLAN_ID(rxd_vlan),
	    GET_RXD_CFI(rxd_vlan), GET_RXD_PRIO(rxd_vlan), rxdd->va_lo,
	    rxdd->va_hi);
}

static void print_rxfd(struct rxf_desc *rxfd)
{
	DBG("=== RxF desc CHIP ORDER/ENDIANESS =============\n"
	    "info 0x%x va_lo %u pa_lo 0x%x pa_hi 0x%x len 0x%x\n",
	    rxfd->info, rxfd->va_lo, rxfd->pa_lo, rxfd->pa_hi, rxfd->len);
}


static inline int bdx_tx_db_size(struct txdb *db)
{
	int taken = db->wptr - db->rptr;
	if (taken < 0)
		taken = db->size + 1 + taken;	

	return db->size - taken;
}

static inline void __bdx_tx_db_ptr_next(struct txdb *db, struct tx_map **pptr)
{
	BDX_ASSERT(db == NULL || pptr == NULL);	

	BDX_ASSERT(*pptr != db->rptr &&	
		   *pptr != db->wptr);	

	BDX_ASSERT(*pptr < db->start ||	
		   *pptr >= db->end);	

	++*pptr;
	if (unlikely(*pptr == db->end))
		*pptr = db->start;
}

static inline void bdx_tx_db_inc_rptr(struct txdb *db)
{
	BDX_ASSERT(db->rptr == db->wptr);	
	__bdx_tx_db_ptr_next(db, &db->rptr);
}

static inline void bdx_tx_db_inc_wptr(struct txdb *db)
{
	__bdx_tx_db_ptr_next(db, &db->wptr);
	BDX_ASSERT(db->rptr == db->wptr);	
}

static int bdx_tx_db_init(struct txdb *d, int sz_type)
{
	int memsz = FIFO_SIZE * (1 << (sz_type + 1));

	d->start = vmalloc(memsz);
	if (!d->start)
		return -ENOMEM;

	d->size = memsz / sizeof(struct tx_map) - 1;
	d->end = d->start + d->size + 1;	

	
	d->rptr = d->start;
	d->wptr = d->start;

	return 0;
}

static void bdx_tx_db_close(struct txdb *d)
{
	BDX_ASSERT(d == NULL);

	vfree(d->start);
	d->start = NULL;
}


static struct {
	u16 bytes;
	u16 qwords;		
} txd_sizes[MAX_SKB_FRAGS + 1];

static inline void
bdx_tx_map_skb(struct bdx_priv *priv, struct sk_buff *skb,
	       struct txd_desc *txdd)
{
	struct txdb *db = &priv->txdb;
	struct pbl *pbl = &txdd->pbl[0];
	int nr_frags = skb_shinfo(skb)->nr_frags;
	int i;

	db->wptr->len = skb_headlen(skb);
	db->wptr->addr.dma = pci_map_single(priv->pdev, skb->data,
					    db->wptr->len, PCI_DMA_TODEVICE);
	pbl->len = CPU_CHIP_SWAP32(db->wptr->len);
	pbl->pa_lo = CPU_CHIP_SWAP32(L32_64(db->wptr->addr.dma));
	pbl->pa_hi = CPU_CHIP_SWAP32(H32_64(db->wptr->addr.dma));
	DBG("=== pbl   len: 0x%x ================\n", pbl->len);
	DBG("=== pbl pa_lo: 0x%x ================\n", pbl->pa_lo);
	DBG("=== pbl pa_hi: 0x%x ================\n", pbl->pa_hi);
	bdx_tx_db_inc_wptr(db);

	for (i = 0; i < nr_frags; i++) {
		const struct skb_frag_struct *frag;

		frag = &skb_shinfo(skb)->frags[i];
		db->wptr->len = skb_frag_size(frag);
		db->wptr->addr.dma = skb_frag_dma_map(&priv->pdev->dev, frag,
						      0, skb_frag_size(frag),
						      DMA_TO_DEVICE);

		pbl++;
		pbl->len = CPU_CHIP_SWAP32(db->wptr->len);
		pbl->pa_lo = CPU_CHIP_SWAP32(L32_64(db->wptr->addr.dma));
		pbl->pa_hi = CPU_CHIP_SWAP32(H32_64(db->wptr->addr.dma));
		bdx_tx_db_inc_wptr(db);
	}

	
	db->wptr->len = -txd_sizes[nr_frags].bytes;
	db->wptr->addr.skb = skb;
	bdx_tx_db_inc_wptr(db);
}

static void __init init_txd_sizes(void)
{
	int i, lwords;

	for (i = 0; i < MAX_SKB_FRAGS + 1; i++) {
		lwords = 7 + (i * 3);
		if (lwords & 1)
			lwords++;	
		txd_sizes[i].qwords = lwords >> 1;
		txd_sizes[i].bytes = lwords << 2;
	}
}

static int bdx_tx_init(struct bdx_priv *priv)
{
	if (bdx_fifo_init(priv, &priv->txd_fifo0.m, priv->txd_size,
			  regTXD_CFG0_0,
			  regTXD_CFG1_0, regTXD_RPTR_0, regTXD_WPTR_0))
		goto err_mem;
	if (bdx_fifo_init(priv, &priv->txf_fifo0.m, priv->txf_size,
			  regTXF_CFG0_0,
			  regTXF_CFG1_0, regTXF_RPTR_0, regTXF_WPTR_0))
		goto err_mem;

	if (bdx_tx_db_init(&priv->txdb, max(priv->txd_size, priv->txf_size)))
		goto err_mem;

	priv->tx_level = BDX_MAX_TX_LEVEL;
#ifdef BDX_DELAY_WPTR
	priv->tx_update_mark = priv->tx_level - 1024;
#endif
	return 0;

err_mem:
	netdev_err(priv->ndev, "Tx init failed\n");
	return -ENOMEM;
}

static inline int bdx_tx_space(struct bdx_priv *priv)
{
	struct txd_fifo *f = &priv->txd_fifo0;
	int fsize;

	f->m.rptr = READ_REG(priv, f->m.reg_RPTR) & TXF_WPTR_WR_PTR;
	fsize = f->m.rptr - f->m.wptr;
	if (fsize <= 0)
		fsize = f->m.memsz + fsize;
	return fsize;
}

static netdev_tx_t bdx_tx_transmit(struct sk_buff *skb,
				   struct net_device *ndev)
{
	struct bdx_priv *priv = netdev_priv(ndev);
	struct txd_fifo *f = &priv->txd_fifo0;
	int txd_checksum = 7;	
	int txd_lgsnd = 0;
	int txd_vlan_id = 0;
	int txd_vtag = 0;
	int txd_mss = 0;

	int nr_frags = skb_shinfo(skb)->nr_frags;
	struct txd_desc *txdd;
	int len;
	unsigned long flags;

	ENTER;
	local_irq_save(flags);
	if (!spin_trylock(&priv->tx_lock)) {
		local_irq_restore(flags);
		DBG("%s[%s]: TX locked, returning NETDEV_TX_LOCKED\n",
		    BDX_DRV_NAME, ndev->name);
		return NETDEV_TX_LOCKED;
	}

	
	BDX_ASSERT(f->m.wptr >= f->m.memsz);	
	txdd = (struct txd_desc *)(f->m.va + f->m.wptr);
	if (unlikely(skb->ip_summed != CHECKSUM_PARTIAL))
		txd_checksum = 0;

	if (skb_shinfo(skb)->gso_size) {
		txd_mss = skb_shinfo(skb)->gso_size;
		txd_lgsnd = 1;
		DBG("skb %p skb len %d gso size = %d\n", skb, skb->len,
		    txd_mss);
	}

	if (vlan_tx_tag_present(skb)) {
		
		txd_vlan_id = vlan_tx_tag_get(skb) & BITS_MASK(12);
		txd_vtag = 1;
	}

	txdd->length = CPU_CHIP_SWAP16(skb->len);
	txdd->mss = CPU_CHIP_SWAP16(txd_mss);
	txdd->txd_val1 =
	    CPU_CHIP_SWAP32(TXD_W1_VAL
			    (txd_sizes[nr_frags].qwords, txd_checksum, txd_vtag,
			     txd_lgsnd, txd_vlan_id));
	DBG("=== TxD desc =====================\n");
	DBG("=== w1: 0x%x ================\n", txdd->txd_val1);
	DBG("=== w2: mss 0x%x len 0x%x\n", txdd->mss, txdd->length);

	bdx_tx_map_skb(priv, skb, txdd);

	f->m.wptr += txd_sizes[nr_frags].bytes;
	len = f->m.wptr - f->m.memsz;
	if (unlikely(len >= 0)) {
		f->m.wptr = len;
		if (len > 0) {
			BDX_ASSERT(len > f->m.memsz);
			memcpy(f->m.va, f->m.va + f->m.memsz, len);
		}
	}
	BDX_ASSERT(f->m.wptr >= f->m.memsz);	

	priv->tx_level -= txd_sizes[nr_frags].bytes;
	BDX_ASSERT(priv->tx_level <= 0 || priv->tx_level > BDX_MAX_TX_LEVEL);
#ifdef BDX_DELAY_WPTR
	if (priv->tx_level > priv->tx_update_mark) {
		WRITE_REG(priv, f->m.reg_WPTR, f->m.wptr & TXF_WPTR_WR_PTR);
	} else {
		if (priv->tx_noupd++ > BDX_NO_UPD_PACKETS) {
			priv->tx_noupd = 0;
			WRITE_REG(priv, f->m.reg_WPTR,
				  f->m.wptr & TXF_WPTR_WR_PTR);
		}
	}
#else
	WRITE_REG(priv, f->m.reg_WPTR, f->m.wptr & TXF_WPTR_WR_PTR);

#endif
#ifdef BDX_LLTX
	ndev->trans_start = jiffies; 
#endif
	ndev->stats.tx_packets++;
	ndev->stats.tx_bytes += skb->len;

	if (priv->tx_level < BDX_MIN_TX_LEVEL) {
		DBG("%s: %s: TX Q STOP level %d\n",
		    BDX_DRV_NAME, ndev->name, priv->tx_level);
		netif_stop_queue(ndev);
	}

	spin_unlock_irqrestore(&priv->tx_lock, flags);
	return NETDEV_TX_OK;
}

static void bdx_tx_cleanup(struct bdx_priv *priv)
{
	struct txf_fifo *f = &priv->txf_fifo0;
	struct txdb *db = &priv->txdb;
	int tx_level = 0;

	ENTER;
	f->m.wptr = READ_REG(priv, f->m.reg_WPTR) & TXF_WPTR_MASK;
	BDX_ASSERT(f->m.rptr >= f->m.memsz);	

	while (f->m.wptr != f->m.rptr) {
		f->m.rptr += BDX_TXF_DESC_SZ;
		f->m.rptr &= f->m.size_mask;

		
		
		BDX_ASSERT(db->rptr->len == 0);
		do {
			BDX_ASSERT(db->rptr->addr.dma == 0);
			pci_unmap_page(priv->pdev, db->rptr->addr.dma,
				       db->rptr->len, PCI_DMA_TODEVICE);
			bdx_tx_db_inc_rptr(db);
		} while (db->rptr->len > 0);
		tx_level -= db->rptr->len;	

		
		dev_kfree_skb_irq(db->rptr->addr.skb);
		bdx_tx_db_inc_rptr(db);
	}

	
	BDX_ASSERT((f->m.wptr & TXF_WPTR_WR_PTR) >= f->m.memsz);
	WRITE_REG(priv, f->m.reg_RPTR, f->m.rptr & TXF_WPTR_WR_PTR);

	spin_lock(&priv->tx_lock);
	priv->tx_level += tx_level;
	BDX_ASSERT(priv->tx_level <= 0 || priv->tx_level > BDX_MAX_TX_LEVEL);
#ifdef BDX_DELAY_WPTR
	if (priv->tx_noupd) {
		priv->tx_noupd = 0;
		WRITE_REG(priv, priv->txd_fifo0.m.reg_WPTR,
			  priv->txd_fifo0.m.wptr & TXF_WPTR_WR_PTR);
	}
#endif

	if (unlikely(netif_queue_stopped(priv->ndev) &&
		     netif_carrier_ok(priv->ndev) &&
		     (priv->tx_level >= BDX_MIN_TX_LEVEL))) {
		DBG("%s: %s: TX Q WAKE level %d\n",
		    BDX_DRV_NAME, priv->ndev->name, priv->tx_level);
		netif_wake_queue(priv->ndev);
	}
	spin_unlock(&priv->tx_lock);
}

static void bdx_tx_free_skbs(struct bdx_priv *priv)
{
	struct txdb *db = &priv->txdb;

	ENTER;
	while (db->rptr != db->wptr) {
		if (likely(db->rptr->len))
			pci_unmap_page(priv->pdev, db->rptr->addr.dma,
				       db->rptr->len, PCI_DMA_TODEVICE);
		else
			dev_kfree_skb(db->rptr->addr.skb);
		bdx_tx_db_inc_rptr(db);
	}
	RET();
}

static void bdx_tx_free(struct bdx_priv *priv)
{
	ENTER;
	bdx_tx_free_skbs(priv);
	bdx_fifo_free(priv, &priv->txd_fifo0.m);
	bdx_fifo_free(priv, &priv->txf_fifo0.m);
	bdx_tx_db_close(&priv->txdb);
}

static void bdx_tx_push_desc(struct bdx_priv *priv, void *data, int size)
{
	struct txd_fifo *f = &priv->txd_fifo0;
	int i = f->m.memsz - f->m.wptr;

	if (size == 0)
		return;

	if (i > size) {
		memcpy(f->m.va + f->m.wptr, data, size);
		f->m.wptr += size;
	} else {
		memcpy(f->m.va + f->m.wptr, data, i);
		f->m.wptr = size - i;
		memcpy(f->m.va, data + i, f->m.wptr);
	}
	WRITE_REG(priv, f->m.reg_WPTR, f->m.wptr & TXF_WPTR_WR_PTR);
}

static void bdx_tx_push_desc_safe(struct bdx_priv *priv, void *data, int size)
{
	int timer = 0;
	ENTER;

	while (size > 0) {
		int avail = bdx_tx_space(priv) - 8;
		if (avail <= 0) {
			if (timer++ > 300) {	
				DBG("timeout while writing desc to TxD fifo\n");
				break;
			}
			udelay(50);	
			continue;
		}
		avail = min(avail, size);
		DBG("about to push  %d bytes starting %p size %d\n", avail,
		    data, size);
		bdx_tx_push_desc(priv, data, avail);
		size -= avail;
		data += avail;
	}
	RET();
}

static const struct net_device_ops bdx_netdev_ops = {
	.ndo_open		= bdx_open,
	.ndo_stop		= bdx_close,
	.ndo_start_xmit		= bdx_tx_transmit,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_do_ioctl		= bdx_ioctl,
	.ndo_set_rx_mode	= bdx_setmulti,
	.ndo_change_mtu		= bdx_change_mtu,
	.ndo_set_mac_address	= bdx_set_mac,
	.ndo_vlan_rx_add_vid	= bdx_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid	= bdx_vlan_rx_kill_vid,
};


static int __devinit
bdx_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct net_device *ndev;
	struct bdx_priv *priv;
	int err, pci_using_dac, port;
	unsigned long pciaddr;
	u32 regionSize;
	struct pci_nic *nic;

	ENTER;

	nic = vmalloc(sizeof(*nic));
	if (!nic)
		RET(-ENOMEM);

    
	err = pci_enable_device(pdev);
	if (err)			
		goto err_pci;		

	if (!(err = pci_set_dma_mask(pdev, DMA_BIT_MASK(64))) &&
	    !(err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64)))) {
		pci_using_dac = 1;
	} else {
		if ((err = pci_set_dma_mask(pdev, DMA_BIT_MASK(32))) ||
		    (err = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32)))) {
			pr_err("No usable DMA configuration, aborting\n");
			goto err_dma;
		}
		pci_using_dac = 0;
	}

	err = pci_request_regions(pdev, BDX_DRV_NAME);
	if (err)
		goto err_dma;

	pci_set_master(pdev);

	pciaddr = pci_resource_start(pdev, 0);
	if (!pciaddr) {
		err = -EIO;
		pr_err("no MMIO resource\n");
		goto err_out_res;
	}
	regionSize = pci_resource_len(pdev, 0);
	if (regionSize < BDX_REGS_SIZE) {
		err = -EIO;
		pr_err("MMIO resource (%x) too small\n", regionSize);
		goto err_out_res;
	}

	nic->regs = ioremap(pciaddr, regionSize);
	if (!nic->regs) {
		err = -EIO;
		pr_err("ioremap failed\n");
		goto err_out_res;
	}

	if (pdev->irq < 2) {
		err = -EIO;
		pr_err("invalid irq (%d)\n", pdev->irq);
		goto err_out_iomap;
	}
	pci_set_drvdata(pdev, nic);

	if (pdev->device == 0x3014)
		nic->port_num = 2;
	else
		nic->port_num = 1;

	print_hw_id(pdev);

	bdx_hw_reset_direct(nic->regs);

	nic->irq_type = IRQ_INTX;
#ifdef BDX_MSI
	if ((readl(nic->regs + FPGA_VER) & 0xFFF) >= 378) {
		err = pci_enable_msi(pdev);
		if (err)
			pr_err("Can't eneble msi. error is %d\n", err);
		else
			nic->irq_type = IRQ_MSI;
	} else
		DBG("HW does not support MSI\n");
#endif

    
	for (port = 0; port < nic->port_num; port++) {
		ndev = alloc_etherdev(sizeof(struct bdx_priv));
		if (!ndev) {
			err = -ENOMEM;
			goto err_out_iomap;
		}

		ndev->netdev_ops = &bdx_netdev_ops;
		ndev->tx_queue_len = BDX_NDEV_TXQ_LEN;

		bdx_set_ethtool_ops(ndev);	

		ndev->if_port = port;
		ndev->base_addr = pciaddr;
		ndev->mem_start = pciaddr;
		ndev->mem_end = pciaddr + regionSize;
		ndev->irq = pdev->irq;
		ndev->features = NETIF_F_IP_CSUM | NETIF_F_SG | NETIF_F_TSO
		    | NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX |
		    NETIF_F_HW_VLAN_FILTER | NETIF_F_RXCSUM
		    
		    ;
		ndev->hw_features = NETIF_F_IP_CSUM | NETIF_F_SG |
			NETIF_F_TSO | NETIF_F_HW_VLAN_TX;

		if (pci_using_dac)
			ndev->features |= NETIF_F_HIGHDMA;

	
		priv = nic->priv[port] = netdev_priv(ndev);

		priv->pBdxRegs = nic->regs + port * 0x8000;
		priv->port = port;
		priv->pdev = pdev;
		priv->ndev = ndev;
		priv->nic = nic;
		priv->msg_enable = BDX_DEF_MSG_ENABLE;

		netif_napi_add(ndev, &priv->napi, bdx_poll, 64);

		if ((readl(nic->regs + FPGA_VER) & 0xFFF) == 308) {
			DBG("HW statistics not supported\n");
			priv->stats_flag = 0;
		} else {
			priv->stats_flag = 1;
		}

		
		priv->txd_size = 2;
		priv->txf_size = 2;
		priv->rxd_size = 2;
		priv->rxf_size = 3;

		
		priv->rdintcm = INT_REG_VAL(0x20, 1, 4, 12);
		priv->tdintcm = INT_REG_VAL(0x20, 1, 0, 12);

#ifdef BDX_LLTX
		ndev->features |= NETIF_F_LLTX;
#endif
		spin_lock_init(&priv->tx_lock);

		
		if (bdx_read_mac(priv)) {
			pr_err("load MAC address failed\n");
			goto err_out_iomap;
		}
		SET_NETDEV_DEV(ndev, &pdev->dev);
		err = register_netdev(ndev);
		if (err) {
			pr_err("register_netdev failed\n");
			goto err_out_free;
		}
		netif_carrier_off(ndev);
		netif_stop_queue(ndev);

		print_eth_id(ndev);
	}
	RET(0);

err_out_free:
	free_netdev(ndev);
err_out_iomap:
	iounmap(nic->regs);
err_out_res:
	pci_release_regions(pdev);
err_dma:
	pci_disable_device(pdev);
err_pci:
	vfree(nic);

	RET(err);
}

static const char
 bdx_stat_names[][ETH_GSTRING_LEN] = {
	"InUCast",		
	"InMCast",		
	"InBCast",		
	"InPkts",		
	"InErrors",		
	"InDropped",		
	"FrameTooLong",		
	"FrameSequenceErrors",	
	"InVLAN",		
	"InDroppedDFE",		
	"InDroppedIntFull",	
	"InFrameAlignErrors",	

	

	"OutUCast",		
	"OutMCast",		
	"OutBCast",		
	"OutPkts",		

	

	"OutVLAN",		
	"InUCastOctects",	
	"OutUCastOctects",	

	

	"InBCastOctects",	
	"OutBCastOctects",	
	"InOctects",		
	"OutOctects",		
};

static int bdx_get_settings(struct net_device *netdev, struct ethtool_cmd *ecmd)
{
	u32 rdintcm;
	u32 tdintcm;
	struct bdx_priv *priv = netdev_priv(netdev);

	rdintcm = priv->rdintcm;
	tdintcm = priv->tdintcm;

	ecmd->supported = (SUPPORTED_10000baseT_Full | SUPPORTED_FIBRE);
	ecmd->advertising = (ADVERTISED_10000baseT_Full | ADVERTISED_FIBRE);
	ethtool_cmd_speed_set(ecmd, SPEED_10000);
	ecmd->duplex = DUPLEX_FULL;
	ecmd->port = PORT_FIBRE;
	ecmd->transceiver = XCVR_EXTERNAL;	
	ecmd->autoneg = AUTONEG_DISABLE;

	ecmd->maxtxpkt =
	    ((GET_PCK_TH(tdintcm) * PCK_TH_MULT) / BDX_TXF_DESC_SZ);
	ecmd->maxrxpkt =
	    ((GET_PCK_TH(rdintcm) * PCK_TH_MULT) / sizeof(struct rxf_desc));

	return 0;
}

static void
bdx_get_drvinfo(struct net_device *netdev, struct ethtool_drvinfo *drvinfo)
{
	struct bdx_priv *priv = netdev_priv(netdev);

	strlcat(drvinfo->driver, BDX_DRV_NAME, sizeof(drvinfo->driver));
	strlcat(drvinfo->version, BDX_DRV_VERSION, sizeof(drvinfo->version));
	strlcat(drvinfo->fw_version, "N/A", sizeof(drvinfo->fw_version));
	strlcat(drvinfo->bus_info, pci_name(priv->pdev),
		sizeof(drvinfo->bus_info));

	drvinfo->n_stats = ((priv->stats_flag) ? ARRAY_SIZE(bdx_stat_names) : 0);
	drvinfo->testinfo_len = 0;
	drvinfo->regdump_len = 0;
	drvinfo->eedump_len = 0;
}

static int
bdx_get_coalesce(struct net_device *netdev, struct ethtool_coalesce *ecoal)
{
	u32 rdintcm;
	u32 tdintcm;
	struct bdx_priv *priv = netdev_priv(netdev);

	rdintcm = priv->rdintcm;
	tdintcm = priv->tdintcm;

	ecoal->rx_coalesce_usecs = GET_INT_COAL(rdintcm) * INT_COAL_MULT;
	ecoal->rx_max_coalesced_frames =
	    ((GET_PCK_TH(rdintcm) * PCK_TH_MULT) / sizeof(struct rxf_desc));

	ecoal->tx_coalesce_usecs = GET_INT_COAL(tdintcm) * INT_COAL_MULT;
	ecoal->tx_max_coalesced_frames =
	    ((GET_PCK_TH(tdintcm) * PCK_TH_MULT) / BDX_TXF_DESC_SZ);

	
	return 0;
}

static int
bdx_set_coalesce(struct net_device *netdev, struct ethtool_coalesce *ecoal)
{
	u32 rdintcm;
	u32 tdintcm;
	struct bdx_priv *priv = netdev_priv(netdev);
	int rx_coal;
	int tx_coal;
	int rx_max_coal;
	int tx_max_coal;

	
	rx_coal = ecoal->rx_coalesce_usecs / INT_COAL_MULT;
	tx_coal = ecoal->tx_coalesce_usecs / INT_COAL_MULT;
	rx_max_coal = ecoal->rx_max_coalesced_frames;
	tx_max_coal = ecoal->tx_max_coalesced_frames;

	
	rx_max_coal =
	    (((rx_max_coal * sizeof(struct rxf_desc)) + PCK_TH_MULT - 1)
	     / PCK_TH_MULT);
	tx_max_coal =
	    (((tx_max_coal * BDX_TXF_DESC_SZ) + PCK_TH_MULT - 1)
	     / PCK_TH_MULT);

	if ((rx_coal > 0x7FFF) || (tx_coal > 0x7FFF) ||
	    (rx_max_coal > 0xF) || (tx_max_coal > 0xF))
		return -EINVAL;

	rdintcm = INT_REG_VAL(rx_coal, GET_INT_COAL_RC(priv->rdintcm),
			      GET_RXF_TH(priv->rdintcm), rx_max_coal);
	tdintcm = INT_REG_VAL(tx_coal, GET_INT_COAL_RC(priv->tdintcm), 0,
			      tx_max_coal);

	priv->rdintcm = rdintcm;
	priv->tdintcm = tdintcm;

	WRITE_REG(priv, regRDINTCM0, rdintcm);
	WRITE_REG(priv, regTDINTCM0, tdintcm);

	return 0;
}

static inline int bdx_rx_fifo_size_to_packets(int rx_size)
{
	return (FIFO_SIZE * (1 << rx_size)) / sizeof(struct rxf_desc);
}

static inline int bdx_tx_fifo_size_to_packets(int tx_size)
{
	return (FIFO_SIZE * (1 << tx_size)) / BDX_TXF_DESC_SZ;
}

static void
bdx_get_ringparam(struct net_device *netdev, struct ethtool_ringparam *ring)
{
	struct bdx_priv *priv = netdev_priv(netdev);

	
	ring->rx_max_pending = bdx_rx_fifo_size_to_packets(3);
	ring->tx_max_pending = bdx_tx_fifo_size_to_packets(3);
	ring->rx_pending = bdx_rx_fifo_size_to_packets(priv->rxf_size);
	ring->tx_pending = bdx_tx_fifo_size_to_packets(priv->txd_size);
}

static int
bdx_set_ringparam(struct net_device *netdev, struct ethtool_ringparam *ring)
{
	struct bdx_priv *priv = netdev_priv(netdev);
	int rx_size = 0;
	int tx_size = 0;

	for (; rx_size < 4; rx_size++) {
		if (bdx_rx_fifo_size_to_packets(rx_size) >= ring->rx_pending)
			break;
	}
	if (rx_size == 4)
		rx_size = 3;

	for (; tx_size < 4; tx_size++) {
		if (bdx_tx_fifo_size_to_packets(tx_size) >= ring->tx_pending)
			break;
	}
	if (tx_size == 4)
		tx_size = 3;

	
	if ((rx_size == priv->rxf_size) &&
	    (tx_size == priv->txd_size))
		return 0;

	priv->rxf_size = rx_size;
	if (rx_size > 1)
		priv->rxd_size = rx_size - 1;
	else
		priv->rxd_size = rx_size;

	priv->txf_size = priv->txd_size = tx_size;

	if (netif_running(netdev)) {
		bdx_close(netdev);
		bdx_open(netdev);
	}
	return 0;
}

static void bdx_get_strings(struct net_device *netdev, u32 stringset, u8 *data)
{
	switch (stringset) {
	case ETH_SS_STATS:
		memcpy(data, *bdx_stat_names, sizeof(bdx_stat_names));
		break;
	}
}

static int bdx_get_sset_count(struct net_device *netdev, int stringset)
{
	struct bdx_priv *priv = netdev_priv(netdev);

	switch (stringset) {
	case ETH_SS_STATS:
		BDX_ASSERT(ARRAY_SIZE(bdx_stat_names)
			   != sizeof(struct bdx_stats) / sizeof(u64));
		return (priv->stats_flag) ? ARRAY_SIZE(bdx_stat_names)	: 0;
	}

	return -EINVAL;
}

static void bdx_get_ethtool_stats(struct net_device *netdev,
				  struct ethtool_stats *stats, u64 *data)
{
	struct bdx_priv *priv = netdev_priv(netdev);

	if (priv->stats_flag) {

		
		bdx_update_stats(priv);

		
		memcpy(data, &priv->hw_stats, sizeof(priv->hw_stats));
	}
}

static void bdx_set_ethtool_ops(struct net_device *netdev)
{
	static const struct ethtool_ops bdx_ethtool_ops = {
		.get_settings = bdx_get_settings,
		.get_drvinfo = bdx_get_drvinfo,
		.get_link = ethtool_op_get_link,
		.get_coalesce = bdx_get_coalesce,
		.set_coalesce = bdx_set_coalesce,
		.get_ringparam = bdx_get_ringparam,
		.set_ringparam = bdx_set_ringparam,
		.get_strings = bdx_get_strings,
		.get_sset_count = bdx_get_sset_count,
		.get_ethtool_stats = bdx_get_ethtool_stats,
	};

	SET_ETHTOOL_OPS(netdev, &bdx_ethtool_ops);
}

static void __devexit bdx_remove(struct pci_dev *pdev)
{
	struct pci_nic *nic = pci_get_drvdata(pdev);
	struct net_device *ndev;
	int port;

	for (port = 0; port < nic->port_num; port++) {
		ndev = nic->priv[port]->ndev;
		unregister_netdev(ndev);
		free_netdev(ndev);
	}

	
#ifdef BDX_MSI
	if (nic->irq_type == IRQ_MSI)
		pci_disable_msi(pdev);
#endif

	iounmap(nic->regs);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
	vfree(nic);

	RET();
}

static struct pci_driver bdx_pci_driver = {
	.name = BDX_DRV_NAME,
	.id_table = bdx_pci_tbl,
	.probe = bdx_probe,
	.remove = __devexit_p(bdx_remove),
};

static void __init print_driver_id(void)
{
	pr_info("%s, %s\n", BDX_DRV_DESC, BDX_DRV_VERSION);
	pr_info("Options: hw_csum %s\n", BDX_MSI_STRING);
}

static int __init bdx_module_init(void)
{
	ENTER;
	init_txd_sizes();
	print_driver_id();
	RET(pci_register_driver(&bdx_pci_driver));
}

module_init(bdx_module_init);

static void __exit bdx_module_exit(void)
{
	ENTER;
	pci_unregister_driver(&bdx_pci_driver);
	RET();
}

module_exit(bdx_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(BDX_DRV_DESC);
MODULE_FIRMWARE("tehuti/bdx.bin");
