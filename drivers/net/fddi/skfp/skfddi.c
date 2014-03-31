/*
 * File Name:
 *   skfddi.c
 *
 * Copyright Information:
 *   Copyright SysKonnect 1998,1999.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The information in this file is provided "AS IS" without warranty.
 *
 * Abstract:
 *   A Linux device driver supporting the SysKonnect FDDI PCI controller
 *   familie.
 *
 * Maintainers:
 *   CG    Christoph Goos (cgoos@syskonnect.de)
 *
 * Contributors:
 *   DM    David S. Miller
 *
 * Address all question to:
 *   linux@syskonnect.de
 *
 * The technical manual for the adapters is available from SysKonnect's
 * web pages: www.syskonnect.com
 * Goto "Support" and search Knowledge Base for "manual".
 *
 * Driver Architecture:
 *   The driver architecture is based on the DEC FDDI driver by
 *   Lawrence V. Stefani and several ethernet drivers.
 *   I also used an existing Windows NT miniport driver.
 *   All hardware dependent functions are handled by the SysKonnect
 *   Hardware Module.
 *   The only headerfiles that are directly related to this source
 *   are skfddi.c, h/types.h, h/osdef1st.h, h/targetos.h.
 *   The others belong to the SysKonnect FDDI Hardware Module and
 *   should better not be changed.
 *
 * Modification History:
 *              Date            Name    Description
 *              02-Mar-98       CG	Created.
 *
 *		10-Mar-99	CG	Support for 2.2.x added.
 *		25-Mar-99	CG	Corrected IRQ routing for SMP (APIC)
 *		26-Oct-99	CG	Fixed compilation error on 2.2.13
 *		12-Nov-99	CG	Source code release
 *		22-Nov-99	CG	Included in kernel source.
 *		07-May-00	DM	64 bit fixes, new dma interface
 *		31-Jul-03	DB	Audit copy_*_user in skfp_ioctl
 *					  Daniele Bellucci <bellucda@tiscali.it>
 *		03-Dec-03	SH	Convert to PCI device model
 *
 * Compilation options (-Dxxx):
 *              DRIVERDEBUG     print lots of messages to log file
 *              DUMPPACKETS     print received/transmitted packets to logfile
 * 
 * Tested cpu architectures:
 *	- i386
 *	- sparc64
 */

#define VERSION		"2.07"

static const char * const boot_msg = 
	"SysKonnect FDDI PCI Adapter driver v" VERSION " for\n"
	"  SK-55xx/SK-58xx adapters (SK-NET FDDI-FP/UP/LP)";


#include <linux/capability.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/fddidevice.h>
#include <linux/skbuff.h>
#include <linux/bitops.h>
#include <linux/gfp.h>

#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include	"h/types.h"
#undef ADDR			
#include	"h/skfbi.h"
#include	"h/fddi.h"
#include	"h/smc.h"
#include	"h/smtstate.h"


static int skfp_driver_init(struct net_device *dev);
static int skfp_open(struct net_device *dev);
static int skfp_close(struct net_device *dev);
static irqreturn_t skfp_interrupt(int irq, void *dev_id);
static struct net_device_stats *skfp_ctl_get_stats(struct net_device *dev);
static void skfp_ctl_set_multicast_list(struct net_device *dev);
static void skfp_ctl_set_multicast_list_wo_lock(struct net_device *dev);
static int skfp_ctl_set_mac_address(struct net_device *dev, void *addr);
static int skfp_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static netdev_tx_t skfp_send_pkt(struct sk_buff *skb,
				       struct net_device *dev);
static void send_queued_packets(struct s_smc *smc);
static void CheckSourceAddress(unsigned char *frame, unsigned char *hw_addr);
static void ResetAdapter(struct s_smc *smc);


void *mac_drv_get_space(struct s_smc *smc, u_int size);
void *mac_drv_get_desc_mem(struct s_smc *smc, u_int size);
unsigned long mac_drv_virt2phys(struct s_smc *smc, void *virt);
unsigned long dma_master(struct s_smc *smc, void *virt, int len, int flag);
void dma_complete(struct s_smc *smc, volatile union s_fp_descr *descr,
		  int flag);
void mac_drv_tx_complete(struct s_smc *smc, volatile struct s_smt_fp_txd *txd);
void llc_restart_tx(struct s_smc *smc);
void mac_drv_rx_complete(struct s_smc *smc, volatile struct s_smt_fp_rxd *rxd,
			 int frag_count, int len);
void mac_drv_requeue_rxd(struct s_smc *smc, volatile struct s_smt_fp_rxd *rxd,
			 int frag_count);
void mac_drv_fill_rxd(struct s_smc *smc);
void mac_drv_clear_rxd(struct s_smc *smc, volatile struct s_smt_fp_rxd *rxd,
		       int frag_count);
int mac_drv_rx_init(struct s_smc *smc, int len, int fc, char *look_ahead,
		    int la_len);
void dump_data(unsigned char *Data, int length);

extern u_int mac_drv_check_space(void);
extern int mac_drv_init(struct s_smc *smc);
extern void hwm_tx_frag(struct s_smc *smc, char far * virt, u_long phys,
			int len, int frame_status);
extern int hwm_tx_init(struct s_smc *smc, u_char fc, int frag_count,
		       int frame_len, int frame_status);
extern void fddi_isr(struct s_smc *smc);
extern void hwm_rx_frag(struct s_smc *smc, char far * virt, u_long phys,
			int len, int frame_status);
extern void mac_drv_rx_mode(struct s_smc *smc, int mode);
extern void mac_drv_clear_rx_queue(struct s_smc *smc);
extern void enable_tx_irq(struct s_smc *smc, u_short queue);

static DEFINE_PCI_DEVICE_TABLE(skfddi_pci_tbl) = {
	{ PCI_VENDOR_ID_SK, PCI_DEVICE_ID_SK_FP, PCI_ANY_ID, PCI_ANY_ID, },
	{ }			
};
MODULE_DEVICE_TABLE(pci, skfddi_pci_tbl);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mirko Lindner <mlindner@syskonnect.de>");


static int num_boards;	

static const struct net_device_ops skfp_netdev_ops = {
	.ndo_open		= skfp_open,
	.ndo_stop		= skfp_close,
	.ndo_start_xmit		= skfp_send_pkt,
	.ndo_get_stats		= skfp_ctl_get_stats,
	.ndo_change_mtu		= fddi_change_mtu,
	.ndo_set_rx_mode	= skfp_ctl_set_multicast_list,
	.ndo_set_mac_address	= skfp_ctl_set_mac_address,
	.ndo_do_ioctl		= skfp_ioctl,
};

static int skfp_init_one(struct pci_dev *pdev,
				const struct pci_device_id *ent)
{
	struct net_device *dev;
	struct s_smc *smc;	
	void __iomem *mem;
	int err;

	pr_debug("entering skfp_init_one\n");

	if (num_boards == 0) 
		printk("%s\n", boot_msg);

	err = pci_enable_device(pdev);
	if (err)
		return err;

	err = pci_request_regions(pdev, "skfddi");
	if (err)
		goto err_out1;

	pci_set_master(pdev);

#ifdef MEM_MAPPED_IO
	if (!(pci_resource_flags(pdev, 0) & IORESOURCE_MEM)) {
		printk(KERN_ERR "skfp: region is not an MMIO resource\n");
		err = -EIO;
		goto err_out2;
	}

	mem = ioremap(pci_resource_start(pdev, 0), 0x4000);
#else
	if (!(pci_resource_flags(pdev, 1) & IO_RESOURCE_IO)) {
		printk(KERN_ERR "skfp: region is not PIO resource\n");
		err = -EIO;
		goto err_out2;
	}

	mem = ioport_map(pci_resource_start(pdev, 1), FP_IO_LEN);
#endif
	if (!mem) {
		printk(KERN_ERR "skfp:  Unable to map register, "
				"FDDI adapter will be disabled.\n");
		err = -EIO;
		goto err_out2;
	}

	dev = alloc_fddidev(sizeof(struct s_smc));
	if (!dev) {
		printk(KERN_ERR "skfp: Unable to allocate fddi device, "
				"FDDI adapter will be disabled.\n");
		err = -ENOMEM;
		goto err_out3;
	}

	dev->irq = pdev->irq;
	dev->netdev_ops = &skfp_netdev_ops;

	SET_NETDEV_DEV(dev, &pdev->dev);

	
	smc = netdev_priv(dev);
	smc->os.dev = dev;
	smc->os.bus_type = SK_BUS_TYPE_PCI;
	smc->os.pdev = *pdev;
	smc->os.QueueSkb = MAX_TX_QUEUE_LEN;
	smc->os.MaxFrameSize = MAX_FRAME_SIZE;
	smc->os.dev = dev;
	smc->hw.slot = -1;
	smc->hw.iop = mem;
	smc->os.ResetRequested = FALSE;
	skb_queue_head_init(&smc->os.SendSkbQueue);

	dev->base_addr = (unsigned long)mem;

	err = skfp_driver_init(dev);
	if (err)
		goto err_out4;

	err = register_netdev(dev);
	if (err)
		goto err_out5;

	++num_boards;
	pci_set_drvdata(pdev, dev);

	if ((pdev->subsystem_device & 0xff00) == 0x5500 ||
	    (pdev->subsystem_device & 0xff00) == 0x5800) 
		printk("%s: SysKonnect FDDI PCI adapter"
		       " found (SK-%04X)\n", dev->name,	
		       pdev->subsystem_device);
	else
		printk("%s: FDDI PCI adapter found\n", dev->name);

	return 0;
err_out5:
	if (smc->os.SharedMemAddr) 
		pci_free_consistent(pdev, smc->os.SharedMemSize,
				    smc->os.SharedMemAddr, 
				    smc->os.SharedMemDMA);
	pci_free_consistent(pdev, MAX_FRAME_SIZE,
			    smc->os.LocalRxBuffer, smc->os.LocalRxBufferDMA);
err_out4:
	free_netdev(dev);
err_out3:
#ifdef MEM_MAPPED_IO
	iounmap(mem);
#else
	ioport_unmap(mem);
#endif
err_out2:
	pci_release_regions(pdev);
err_out1:
	pci_disable_device(pdev);
	return err;
}

static void __devexit skfp_remove_one(struct pci_dev *pdev)
{
	struct net_device *p = pci_get_drvdata(pdev);
	struct s_smc *lp = netdev_priv(p);

	unregister_netdev(p);

	if (lp->os.SharedMemAddr) {
		pci_free_consistent(&lp->os.pdev,
				    lp->os.SharedMemSize,
				    lp->os.SharedMemAddr,
				    lp->os.SharedMemDMA);
		lp->os.SharedMemAddr = NULL;
	}
	if (lp->os.LocalRxBuffer) {
		pci_free_consistent(&lp->os.pdev,
				    MAX_FRAME_SIZE,
				    lp->os.LocalRxBuffer,
				    lp->os.LocalRxBufferDMA);
		lp->os.LocalRxBuffer = NULL;
	}
#ifdef MEM_MAPPED_IO
	iounmap(lp->hw.iop);
#else
	ioport_unmap(lp->hw.iop);
#endif
	pci_release_regions(pdev);
	free_netdev(p);

	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
}

static  int skfp_driver_init(struct net_device *dev)
{
	struct s_smc *smc = netdev_priv(dev);
	skfddi_priv *bp = &smc->os;
	int err = -EIO;

	pr_debug("entering skfp_driver_init\n");

	
	bp->base_addr = dev->base_addr;

	
	smc->hw.irq = dev->irq;

	spin_lock_init(&bp->DriverLock);
	
	
	bp->LocalRxBuffer = pci_alloc_consistent(&bp->pdev, MAX_FRAME_SIZE, &bp->LocalRxBufferDMA);
	if (!bp->LocalRxBuffer) {
		printk("could not allocate mem for ");
		printk("LocalRxBuffer: %d byte\n", MAX_FRAME_SIZE);
		goto fail;
	}

	
	bp->SharedMemSize = mac_drv_check_space();
	pr_debug("Memory for HWM: %ld\n", bp->SharedMemSize);
	if (bp->SharedMemSize > 0) {
		bp->SharedMemSize += 16;	

		bp->SharedMemAddr = pci_alloc_consistent(&bp->pdev,
							 bp->SharedMemSize,
							 &bp->SharedMemDMA);
		if (!bp->SharedMemAddr) {
			printk("could not allocate mem for ");
			printk("hardware module: %ld byte\n",
			       bp->SharedMemSize);
			goto fail;
		}
		bp->SharedMemHeap = 0;	

	} else {
		bp->SharedMemAddr = NULL;
		bp->SharedMemHeap = 0;
	}			

	memset(bp->SharedMemAddr, 0, bp->SharedMemSize);

	card_stop(smc);		

	pr_debug("mac_drv_init()..\n");
	if (mac_drv_init(smc) != 0) {
		pr_debug("mac_drv_init() failed\n");
		goto fail;
	}
	read_address(smc, NULL);
	pr_debug("HW-Addr: %pMF\n", smc->hw.fddi_canon_addr.a);
	memcpy(dev->dev_addr, smc->hw.fddi_canon_addr.a, 6);

	smt_reset_defaults(smc, 0);

	return 0;

fail:
	if (bp->SharedMemAddr) {
		pci_free_consistent(&bp->pdev,
				    bp->SharedMemSize,
				    bp->SharedMemAddr,
				    bp->SharedMemDMA);
		bp->SharedMemAddr = NULL;
	}
	if (bp->LocalRxBuffer) {
		pci_free_consistent(&bp->pdev, MAX_FRAME_SIZE,
				    bp->LocalRxBuffer, bp->LocalRxBufferDMA);
		bp->LocalRxBuffer = NULL;
	}
	return err;
}				


static int skfp_open(struct net_device *dev)
{
	struct s_smc *smc = netdev_priv(dev);
	int err;

	pr_debug("entering skfp_open\n");
	
	err = request_irq(dev->irq, skfp_interrupt, IRQF_SHARED,
			  dev->name, dev);
	if (err)
		return err;

	read_address(smc, NULL);
	memcpy(dev->dev_addr, smc->hw.fddi_canon_addr.a, 6);

	init_smt(smc, NULL);
	smt_online(smc, 1);
	STI_FBI();

	
	mac_clear_multicast(smc);

	
	mac_drv_rx_mode(smc, RX_DISABLE_PROMISC);

	netif_start_queue(dev);
	return 0;
}				


static int skfp_close(struct net_device *dev)
{
	struct s_smc *smc = netdev_priv(dev);
	skfddi_priv *bp = &smc->os;

	CLI_FBI();
	smt_reset_defaults(smc, 1);
	card_stop(smc);
	mac_drv_clear_tx_queue(smc);
	mac_drv_clear_rx_queue(smc);

	netif_stop_queue(dev);
	
	free_irq(dev->irq, dev);

	skb_queue_purge(&bp->SendSkbQueue);
	bp->QueueSkb = MAX_TX_QUEUE_LEN;

	return 0;
}				



static irqreturn_t skfp_interrupt(int irq, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct s_smc *smc;	
	skfddi_priv *bp;

	smc = netdev_priv(dev);
	bp = &smc->os;

	
	if (inpd(ADDR(B0_IMSK)) == 0) {
		
		return IRQ_NONE;
	}
	
	if ((inpd(ISR_A) & smc->hw.is_imask) == 0) {	
		
		return IRQ_NONE;
	}
	CLI_FBI();		
	spin_lock(&bp->DriverLock);

	
	fddi_isr(smc);

	if (smc->os.ResetRequested) {
		ResetAdapter(smc);
		smc->os.ResetRequested = FALSE;
	}
	spin_unlock(&bp->DriverLock);
	STI_FBI();		

	return IRQ_HANDLED;
}				


static struct net_device_stats *skfp_ctl_get_stats(struct net_device *dev)
{
	struct s_smc *bp = netdev_priv(dev);

	

	bp->os.MacStat.port_bs_flag[0] = 0x1234;
	bp->os.MacStat.port_bs_flag[1] = 0x5678;
#if 0
	


	memcpy(bp->stats.smt_station_id, &bp->cmd_rsp_virt->smt_mib_get.smt_station_id, sizeof(bp->cmd_rsp_virt->smt_mib_get.smt_station_id));
	bp->stats.smt_op_version_id = bp->cmd_rsp_virt->smt_mib_get.smt_op_version_id;
	bp->stats.smt_hi_version_id = bp->cmd_rsp_virt->smt_mib_get.smt_hi_version_id;
	bp->stats.smt_lo_version_id = bp->cmd_rsp_virt->smt_mib_get.smt_lo_version_id;
	memcpy(bp->stats.smt_user_data, &bp->cmd_rsp_virt->smt_mib_get.smt_user_data, sizeof(bp->cmd_rsp_virt->smt_mib_get.smt_user_data));
	bp->stats.smt_mib_version_id = bp->cmd_rsp_virt->smt_mib_get.smt_mib_version_id;
	bp->stats.smt_mac_cts = bp->cmd_rsp_virt->smt_mib_get.smt_mac_ct;
	bp->stats.smt_non_master_cts = bp->cmd_rsp_virt->smt_mib_get.smt_non_master_ct;
	bp->stats.smt_master_cts = bp->cmd_rsp_virt->smt_mib_get.smt_master_ct;
	bp->stats.smt_available_paths = bp->cmd_rsp_virt->smt_mib_get.smt_available_paths;
	bp->stats.smt_config_capabilities = bp->cmd_rsp_virt->smt_mib_get.smt_config_capabilities;
	bp->stats.smt_config_policy = bp->cmd_rsp_virt->smt_mib_get.smt_config_policy;
	bp->stats.smt_connection_policy = bp->cmd_rsp_virt->smt_mib_get.smt_connection_policy;
	bp->stats.smt_t_notify = bp->cmd_rsp_virt->smt_mib_get.smt_t_notify;
	bp->stats.smt_stat_rpt_policy = bp->cmd_rsp_virt->smt_mib_get.smt_stat_rpt_policy;
	bp->stats.smt_trace_max_expiration = bp->cmd_rsp_virt->smt_mib_get.smt_trace_max_expiration;
	bp->stats.smt_bypass_present = bp->cmd_rsp_virt->smt_mib_get.smt_bypass_present;
	bp->stats.smt_ecm_state = bp->cmd_rsp_virt->smt_mib_get.smt_ecm_state;
	bp->stats.smt_cf_state = bp->cmd_rsp_virt->smt_mib_get.smt_cf_state;
	bp->stats.smt_remote_disconnect_flag = bp->cmd_rsp_virt->smt_mib_get.smt_remote_disconnect_flag;
	bp->stats.smt_station_status = bp->cmd_rsp_virt->smt_mib_get.smt_station_status;
	bp->stats.smt_peer_wrap_flag = bp->cmd_rsp_virt->smt_mib_get.smt_peer_wrap_flag;
	bp->stats.smt_time_stamp = bp->cmd_rsp_virt->smt_mib_get.smt_msg_time_stamp.ls;
	bp->stats.smt_transition_time_stamp = bp->cmd_rsp_virt->smt_mib_get.smt_transition_time_stamp.ls;
	bp->stats.mac_frame_status_functions = bp->cmd_rsp_virt->smt_mib_get.mac_frame_status_functions;
	bp->stats.mac_t_max_capability = bp->cmd_rsp_virt->smt_mib_get.mac_t_max_capability;
	bp->stats.mac_tvx_capability = bp->cmd_rsp_virt->smt_mib_get.mac_tvx_capability;
	bp->stats.mac_available_paths = bp->cmd_rsp_virt->smt_mib_get.mac_available_paths;
	bp->stats.mac_current_path = bp->cmd_rsp_virt->smt_mib_get.mac_current_path;
	memcpy(bp->stats.mac_upstream_nbr, &bp->cmd_rsp_virt->smt_mib_get.mac_upstream_nbr, FDDI_K_ALEN);
	memcpy(bp->stats.mac_downstream_nbr, &bp->cmd_rsp_virt->smt_mib_get.mac_downstream_nbr, FDDI_K_ALEN);
	memcpy(bp->stats.mac_old_upstream_nbr, &bp->cmd_rsp_virt->smt_mib_get.mac_old_upstream_nbr, FDDI_K_ALEN);
	memcpy(bp->stats.mac_old_downstream_nbr, &bp->cmd_rsp_virt->smt_mib_get.mac_old_downstream_nbr, FDDI_K_ALEN);
	bp->stats.mac_dup_address_test = bp->cmd_rsp_virt->smt_mib_get.mac_dup_address_test;
	bp->stats.mac_requested_paths = bp->cmd_rsp_virt->smt_mib_get.mac_requested_paths;
	bp->stats.mac_downstream_port_type = bp->cmd_rsp_virt->smt_mib_get.mac_downstream_port_type;
	memcpy(bp->stats.mac_smt_address, &bp->cmd_rsp_virt->smt_mib_get.mac_smt_address, FDDI_K_ALEN);
	bp->stats.mac_t_req = bp->cmd_rsp_virt->smt_mib_get.mac_t_req;
	bp->stats.mac_t_neg = bp->cmd_rsp_virt->smt_mib_get.mac_t_neg;
	bp->stats.mac_t_max = bp->cmd_rsp_virt->smt_mib_get.mac_t_max;
	bp->stats.mac_tvx_value = bp->cmd_rsp_virt->smt_mib_get.mac_tvx_value;
	bp->stats.mac_frame_error_threshold = bp->cmd_rsp_virt->smt_mib_get.mac_frame_error_threshold;
	bp->stats.mac_frame_error_ratio = bp->cmd_rsp_virt->smt_mib_get.mac_frame_error_ratio;
	bp->stats.mac_rmt_state = bp->cmd_rsp_virt->smt_mib_get.mac_rmt_state;
	bp->stats.mac_da_flag = bp->cmd_rsp_virt->smt_mib_get.mac_da_flag;
	bp->stats.mac_una_da_flag = bp->cmd_rsp_virt->smt_mib_get.mac_unda_flag;
	bp->stats.mac_frame_error_flag = bp->cmd_rsp_virt->smt_mib_get.mac_frame_error_flag;
	bp->stats.mac_ma_unitdata_available = bp->cmd_rsp_virt->smt_mib_get.mac_ma_unitdata_available;
	bp->stats.mac_hardware_present = bp->cmd_rsp_virt->smt_mib_get.mac_hardware_present;
	bp->stats.mac_ma_unitdata_enable = bp->cmd_rsp_virt->smt_mib_get.mac_ma_unitdata_enable;
	bp->stats.path_tvx_lower_bound = bp->cmd_rsp_virt->smt_mib_get.path_tvx_lower_bound;
	bp->stats.path_t_max_lower_bound = bp->cmd_rsp_virt->smt_mib_get.path_t_max_lower_bound;
	bp->stats.path_max_t_req = bp->cmd_rsp_virt->smt_mib_get.path_max_t_req;
	memcpy(bp->stats.path_configuration, &bp->cmd_rsp_virt->smt_mib_get.path_configuration, sizeof(bp->cmd_rsp_virt->smt_mib_get.path_configuration));
	bp->stats.port_my_type[0] = bp->cmd_rsp_virt->smt_mib_get.port_my_type[0];
	bp->stats.port_my_type[1] = bp->cmd_rsp_virt->smt_mib_get.port_my_type[1];
	bp->stats.port_neighbor_type[0] = bp->cmd_rsp_virt->smt_mib_get.port_neighbor_type[0];
	bp->stats.port_neighbor_type[1] = bp->cmd_rsp_virt->smt_mib_get.port_neighbor_type[1];
	bp->stats.port_connection_policies[0] = bp->cmd_rsp_virt->smt_mib_get.port_connection_policies[0];
	bp->stats.port_connection_policies[1] = bp->cmd_rsp_virt->smt_mib_get.port_connection_policies[1];
	bp->stats.port_mac_indicated[0] = bp->cmd_rsp_virt->smt_mib_get.port_mac_indicated[0];
	bp->stats.port_mac_indicated[1] = bp->cmd_rsp_virt->smt_mib_get.port_mac_indicated[1];
	bp->stats.port_current_path[0] = bp->cmd_rsp_virt->smt_mib_get.port_current_path[0];
	bp->stats.port_current_path[1] = bp->cmd_rsp_virt->smt_mib_get.port_current_path[1];
	memcpy(&bp->stats.port_requested_paths[0 * 3], &bp->cmd_rsp_virt->smt_mib_get.port_requested_paths[0], 3);
	memcpy(&bp->stats.port_requested_paths[1 * 3], &bp->cmd_rsp_virt->smt_mib_get.port_requested_paths[1], 3);
	bp->stats.port_mac_placement[0] = bp->cmd_rsp_virt->smt_mib_get.port_mac_placement[0];
	bp->stats.port_mac_placement[1] = bp->cmd_rsp_virt->smt_mib_get.port_mac_placement[1];
	bp->stats.port_available_paths[0] = bp->cmd_rsp_virt->smt_mib_get.port_available_paths[0];
	bp->stats.port_available_paths[1] = bp->cmd_rsp_virt->smt_mib_get.port_available_paths[1];
	bp->stats.port_pmd_class[0] = bp->cmd_rsp_virt->smt_mib_get.port_pmd_class[0];
	bp->stats.port_pmd_class[1] = bp->cmd_rsp_virt->smt_mib_get.port_pmd_class[1];
	bp->stats.port_connection_capabilities[0] = bp->cmd_rsp_virt->smt_mib_get.port_connection_capabilities[0];
	bp->stats.port_connection_capabilities[1] = bp->cmd_rsp_virt->smt_mib_get.port_connection_capabilities[1];
	bp->stats.port_bs_flag[0] = bp->cmd_rsp_virt->smt_mib_get.port_bs_flag[0];
	bp->stats.port_bs_flag[1] = bp->cmd_rsp_virt->smt_mib_get.port_bs_flag[1];
	bp->stats.port_ler_estimate[0] = bp->cmd_rsp_virt->smt_mib_get.port_ler_estimate[0];
	bp->stats.port_ler_estimate[1] = bp->cmd_rsp_virt->smt_mib_get.port_ler_estimate[1];
	bp->stats.port_ler_cutoff[0] = bp->cmd_rsp_virt->smt_mib_get.port_ler_cutoff[0];
	bp->stats.port_ler_cutoff[1] = bp->cmd_rsp_virt->smt_mib_get.port_ler_cutoff[1];
	bp->stats.port_ler_alarm[0] = bp->cmd_rsp_virt->smt_mib_get.port_ler_alarm[0];
	bp->stats.port_ler_alarm[1] = bp->cmd_rsp_virt->smt_mib_get.port_ler_alarm[1];
	bp->stats.port_connect_state[0] = bp->cmd_rsp_virt->smt_mib_get.port_connect_state[0];
	bp->stats.port_connect_state[1] = bp->cmd_rsp_virt->smt_mib_get.port_connect_state[1];
	bp->stats.port_pcm_state[0] = bp->cmd_rsp_virt->smt_mib_get.port_pcm_state[0];
	bp->stats.port_pcm_state[1] = bp->cmd_rsp_virt->smt_mib_get.port_pcm_state[1];
	bp->stats.port_pc_withhold[0] = bp->cmd_rsp_virt->smt_mib_get.port_pc_withhold[0];
	bp->stats.port_pc_withhold[1] = bp->cmd_rsp_virt->smt_mib_get.port_pc_withhold[1];
	bp->stats.port_ler_flag[0] = bp->cmd_rsp_virt->smt_mib_get.port_ler_flag[0];
	bp->stats.port_ler_flag[1] = bp->cmd_rsp_virt->smt_mib_get.port_ler_flag[1];
	bp->stats.port_hardware_present[0] = bp->cmd_rsp_virt->smt_mib_get.port_hardware_present[0];
	bp->stats.port_hardware_present[1] = bp->cmd_rsp_virt->smt_mib_get.port_hardware_present[1];


	

	bp->stats.mac_frame_cts = bp->cmd_rsp_virt->cntrs_get.cntrs.frame_cnt.ls;
	bp->stats.mac_copied_cts = bp->cmd_rsp_virt->cntrs_get.cntrs.copied_cnt.ls;
	bp->stats.mac_transmit_cts = bp->cmd_rsp_virt->cntrs_get.cntrs.transmit_cnt.ls;
	bp->stats.mac_error_cts = bp->cmd_rsp_virt->cntrs_get.cntrs.error_cnt.ls;
	bp->stats.mac_lost_cts = bp->cmd_rsp_virt->cntrs_get.cntrs.lost_cnt.ls;
	bp->stats.port_lct_fail_cts[0] = bp->cmd_rsp_virt->cntrs_get.cntrs.lct_rejects[0].ls;
	bp->stats.port_lct_fail_cts[1] = bp->cmd_rsp_virt->cntrs_get.cntrs.lct_rejects[1].ls;
	bp->stats.port_lem_reject_cts[0] = bp->cmd_rsp_virt->cntrs_get.cntrs.lem_rejects[0].ls;
	bp->stats.port_lem_reject_cts[1] = bp->cmd_rsp_virt->cntrs_get.cntrs.lem_rejects[1].ls;
	bp->stats.port_lem_cts[0] = bp->cmd_rsp_virt->cntrs_get.cntrs.link_errors[0].ls;
	bp->stats.port_lem_cts[1] = bp->cmd_rsp_virt->cntrs_get.cntrs.link_errors[1].ls;

#endif
	return (struct net_device_stats *)&bp->os.MacStat;
}				


static void skfp_ctl_set_multicast_list(struct net_device *dev)
{
	struct s_smc *smc = netdev_priv(dev);
	skfddi_priv *bp = &smc->os;
	unsigned long Flags;

	spin_lock_irqsave(&bp->DriverLock, Flags);
	skfp_ctl_set_multicast_list_wo_lock(dev);
	spin_unlock_irqrestore(&bp->DriverLock, Flags);
}				



static void skfp_ctl_set_multicast_list_wo_lock(struct net_device *dev)
{
	struct s_smc *smc = netdev_priv(dev);
	struct netdev_hw_addr *ha;

	
	if (dev->flags & IFF_PROMISC) {
		mac_drv_rx_mode(smc, RX_ENABLE_PROMISC);
		pr_debug("PROMISCUOUS MODE ENABLED\n");
	}
	
	else {
		mac_drv_rx_mode(smc, RX_DISABLE_PROMISC);
		pr_debug("PROMISCUOUS MODE DISABLED\n");

		
		mac_clear_multicast(smc);
		mac_drv_rx_mode(smc, RX_DISABLE_ALLMULTI);

		if (dev->flags & IFF_ALLMULTI) {
			mac_drv_rx_mode(smc, RX_ENABLE_ALLMULTI);
			pr_debug("ENABLE ALL MC ADDRESSES\n");
		} else if (!netdev_mc_empty(dev)) {
			if (netdev_mc_count(dev) <= FPMAX_MULTICAST) {
				

				
				netdev_for_each_mc_addr(ha, dev) {
					mac_add_multicast(smc,
						(struct fddi_addr *)ha->addr,
						1);

					pr_debug("ENABLE MC ADDRESS: %pMF\n",
						 ha->addr);
				}

			} else {	

				mac_drv_rx_mode(smc, RX_ENABLE_ALLMULTI);
				pr_debug("ENABLE ALL MC ADDRESSES\n");
			}
		} else {	

			pr_debug("DISABLE ALL MC ADDRESSES\n");
		}

		
		mac_update_multicast(smc);
	}
}				


static int skfp_ctl_set_mac_address(struct net_device *dev, void *addr)
{
	struct s_smc *smc = netdev_priv(dev);
	struct sockaddr *p_sockaddr = (struct sockaddr *) addr;
	skfddi_priv *bp = &smc->os;
	unsigned long Flags;


	memcpy(dev->dev_addr, p_sockaddr->sa_data, FDDI_K_ALEN);
	spin_lock_irqsave(&bp->DriverLock, Flags);
	ResetAdapter(smc);
	spin_unlock_irqrestore(&bp->DriverLock, Flags);

	return 0;		
}				




static int skfp_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct s_smc *smc = netdev_priv(dev);
	skfddi_priv *lp = &smc->os;
	struct s_skfp_ioctl ioc;
	int status = 0;

	if (copy_from_user(&ioc, rq->ifr_data, sizeof(struct s_skfp_ioctl)))
		return -EFAULT;

	switch (ioc.cmd) {
	case SKFP_GET_STATS:	
		ioc.len = sizeof(lp->MacStat);
		status = copy_to_user(ioc.data, skfp_ctl_get_stats(dev), ioc.len)
				? -EFAULT : 0;
		break;
	case SKFP_CLR_STATS:	
		if (!capable(CAP_NET_ADMIN)) {
			status = -EPERM;
		} else {
			memset(&lp->MacStat, 0, sizeof(lp->MacStat));
		}
		break;
	default:
		printk("ioctl for %s: unknown cmd: %04x\n", dev->name, ioc.cmd);
		status = -EOPNOTSUPP;

	}			

	return status;
}				


static netdev_tx_t skfp_send_pkt(struct sk_buff *skb,
				       struct net_device *dev)
{
	struct s_smc *smc = netdev_priv(dev);
	skfddi_priv *bp = &smc->os;

	pr_debug("skfp_send_pkt\n");


	if (!(skb->len >= FDDI_K_LLC_ZLEN && skb->len <= FDDI_K_LLC_LEN)) {
		bp->MacStat.gen.tx_errors++;	
		
		netif_start_queue(dev);
		dev_kfree_skb(skb);
		return NETDEV_TX_OK;	
	}
	if (bp->QueueSkb == 0) {	

		netif_stop_queue(dev);
		return NETDEV_TX_BUSY;
	}
	bp->QueueSkb--;
	skb_queue_tail(&bp->SendSkbQueue, skb);
	send_queued_packets(netdev_priv(dev));
	if (bp->QueueSkb == 0) {
		netif_stop_queue(dev);
	}
	return NETDEV_TX_OK;

}				


static void send_queued_packets(struct s_smc *smc)
{
	skfddi_priv *bp = &smc->os;
	struct sk_buff *skb;
	unsigned char fc;
	int queue;
	struct s_smt_fp_txd *txd;	
	dma_addr_t dma_address;
	unsigned long Flags;

	int frame_status;	

	pr_debug("send queued packets\n");
	for (;;) {
		
		skb = skb_dequeue(&bp->SendSkbQueue);

		if (!skb) {
			pr_debug("queue empty\n");
			return;
		}		

		spin_lock_irqsave(&bp->DriverLock, Flags);
		fc = skb->data[0];
		queue = (fc & FC_SYNC_BIT) ? QUEUE_S : QUEUE_A0;
#ifdef ESS
		

		if ((fc & ~(FC_SYNC_BIT | FC_LLC_PRIOR)) == FC_ASYNC_LLC) {
			
			if (!smc->ess.sync_bw_available)
				fc &= ~FC_SYNC_BIT; 

			else {	

				if (smc->mib.fddiESSSynchTxMode) {
					
					fc |= FC_SYNC_BIT;
				}
			}
		}
#endif				
		frame_status = hwm_tx_init(smc, fc, 1, skb->len, queue);

		if ((frame_status & (LOC_TX | LAN_TX)) == 0) {
			

			if ((frame_status & RING_DOWN) != 0) {
				
				pr_debug("Tx attempt while ring down.\n");
			} else if ((frame_status & OUT_OF_TXD) != 0) {
				pr_debug("%s: out of TXDs.\n", bp->dev->name);
			} else {
				pr_debug("%s: out of transmit resources",
					bp->dev->name);
			}

			
			
			skb_queue_head(&bp->SendSkbQueue, skb);
			spin_unlock_irqrestore(&bp->DriverLock, Flags);
			return;	

		}		

		bp->QueueSkb++;	

		
		CheckSourceAddress(skb->data, smc->hw.fddi_canon_addr.a);

		txd = (struct s_smt_fp_txd *) HWM_GET_CURR_TXD(smc, queue);

		dma_address = pci_map_single(&bp->pdev, skb->data,
					     skb->len, PCI_DMA_TODEVICE);
		if (frame_status & LAN_TX) {
			txd->txd_os.skb = skb;			
			txd->txd_os.dma_addr = dma_address;	
		}
		hwm_tx_frag(smc, skb->data, dma_address, skb->len,
                      frame_status | FIRST_FRAG | LAST_FRAG | EN_IRQ_EOF);

		if (!(frame_status & LAN_TX)) {		
			pci_unmap_single(&bp->pdev, dma_address,
					 skb->len, PCI_DMA_TODEVICE);
			dev_kfree_skb_irq(skb);
		}
		spin_unlock_irqrestore(&bp->DriverLock, Flags);
	}			

	return;			

}				


static void CheckSourceAddress(unsigned char *frame, unsigned char *hw_addr)
{
	unsigned char SRBit;

	if ((((unsigned long) frame[1 + 6]) & ~0x01) != 0) 

		return;
	if ((unsigned short) frame[1 + 10] != 0)
		return;
	SRBit = frame[1 + 6] & 0x01;
	memcpy(&frame[1 + 6], hw_addr, 6);
	frame[8] |= SRBit;
}				


static void ResetAdapter(struct s_smc *smc)
{

	pr_debug("[fddi: ResetAdapter]\n");

	

	card_stop(smc);		

	
	mac_drv_clear_tx_queue(smc);
	mac_drv_clear_rx_queue(smc);

	

	smt_reset_defaults(smc, 1);	

	init_smt(smc, (smc->os.dev)->dev_addr);	

	smt_online(smc, 1);	
	STI_FBI();

	
	skfp_ctl_set_multicast_list_wo_lock(smc->os.dev);
}				



void llc_restart_tx(struct s_smc *smc)
{
	skfddi_priv *bp = &smc->os;

	pr_debug("[llc_restart_tx]\n");

	
	spin_unlock(&bp->DriverLock);
	send_queued_packets(smc);
	spin_lock(&bp->DriverLock);
	netif_start_queue(bp->dev);

}				


void *mac_drv_get_space(struct s_smc *smc, unsigned int size)
{
	void *virt;

	pr_debug("mac_drv_get_space (%d bytes), ", size);
	virt = (void *) (smc->os.SharedMemAddr + smc->os.SharedMemHeap);

	if ((smc->os.SharedMemHeap + size) > smc->os.SharedMemSize) {
		printk("Unexpected SMT memory size requested: %d\n", size);
		return NULL;
	}
	smc->os.SharedMemHeap += size;	

	pr_debug("mac_drv_get_space end\n");
	pr_debug("virt addr: %lx\n", (ulong) virt);
	pr_debug("bus  addr: %lx\n", (ulong)
	       (smc->os.SharedMemDMA +
		((char *) virt - (char *)smc->os.SharedMemAddr)));
	return virt;
}				


void *mac_drv_get_desc_mem(struct s_smc *smc, unsigned int size)
{

	char *virt;

	pr_debug("mac_drv_get_desc_mem\n");

	

	virt = mac_drv_get_space(smc, size);

	size = (u_int) (16 - (((unsigned long) virt) & 15UL));
	size = size % 16;

	pr_debug("Allocate %u bytes alignment gap ", size);
	pr_debug("for descriptor memory.\n");

	if (!mac_drv_get_space(smc, size)) {
		printk("fddi: Unable to align descriptor memory.\n");
		return NULL;
	}
	return virt + size;
}				


unsigned long mac_drv_virt2phys(struct s_smc *smc, void *virt)
{
	return smc->os.SharedMemDMA +
		((char *) virt - (char *)smc->os.SharedMemAddr);
}				


u_long dma_master(struct s_smc * smc, void *virt, int len, int flag)
{
	return smc->os.SharedMemDMA +
		((char *) virt - (char *)smc->os.SharedMemAddr);
}				


void dma_complete(struct s_smc *smc, volatile union s_fp_descr *descr, int flag)
{
	if (flag & DMA_WR) {
		skfddi_priv *bp = &smc->os;
		volatile struct s_smt_fp_rxd *r = &descr->r;

		
		if (r->rxd_os.skb && r->rxd_os.dma_addr) {
			int MaxFrameSize = bp->MaxFrameSize;

			pci_unmap_single(&bp->pdev, r->rxd_os.dma_addr,
					 MaxFrameSize, PCI_DMA_FROMDEVICE);
			r->rxd_os.dma_addr = 0;
		}
	}
}				


void mac_drv_tx_complete(struct s_smc *smc, volatile struct s_smt_fp_txd *txd)
{
	struct sk_buff *skb;

	pr_debug("entering mac_drv_tx_complete\n");
	

	if (!(skb = txd->txd_os.skb)) {
		pr_debug("TXD with no skb assigned.\n");
		return;
	}
	txd->txd_os.skb = NULL;

	
	pci_unmap_single(&smc->os.pdev, txd->txd_os.dma_addr,
			 skb->len, PCI_DMA_TODEVICE);
	txd->txd_os.dma_addr = 0;

	smc->os.MacStat.gen.tx_packets++;	
	smc->os.MacStat.gen.tx_bytes+=skb->len;	

	
	dev_kfree_skb_irq(skb);

	pr_debug("leaving mac_drv_tx_complete\n");
}				


#ifdef DUMPPACKETS
void dump_data(unsigned char *Data, int length)
{
	int i, j;
	unsigned char s[255], sh[10];
	if (length > 64) {
		length = 64;
	}
	printk(KERN_INFO "---Packet start---\n");
	for (i = 0, j = 0; i < length / 8; i++, j += 8)
		printk(KERN_INFO "%02x %02x %02x %02x %02x %02x %02x %02x\n",
		       Data[j + 0], Data[j + 1], Data[j + 2], Data[j + 3],
		       Data[j + 4], Data[j + 5], Data[j + 6], Data[j + 7]);
	strcpy(s, "");
	for (i = 0; i < length % 8; i++) {
		sprintf(sh, "%02x ", Data[j + i]);
		strcat(s, sh);
	}
	printk(KERN_INFO "%s\n", s);
	printk(KERN_INFO "------------------\n");
}				
#else
#define dump_data(data,len)
#endif				

void mac_drv_rx_complete(struct s_smc *smc, volatile struct s_smt_fp_rxd *rxd,
			 int frag_count, int len)
{
	skfddi_priv *bp = &smc->os;
	struct sk_buff *skb;
	unsigned char *virt, *cp;
	unsigned short ri;
	u_int RifLength;

	pr_debug("entering mac_drv_rx_complete (len=%d)\n", len);
	if (frag_count != 1) {	

		printk("fddi: Multi-fragment receive!\n");
		goto RequeueRxd;	

	}
	skb = rxd->rxd_os.skb;
	if (!skb) {
		pr_debug("No skb in rxd\n");
		smc->os.MacStat.gen.rx_errors++;
		goto RequeueRxd;
	}
	virt = skb->data;

	

	dump_data(skb->data, len);


	

	if ((virt[1 + 6] & FDDI_RII) == 0)
		RifLength = 0;
	else {
		int n;
		pr_debug("RIF found\n");
		
		cp = virt + FDDI_MAC_HDR_LEN;	

		ri = ntohs(*((__be16 *) cp));
		RifLength = ri & FDDI_RCF_LEN_MASK;
		if (len < (int) (FDDI_MAC_HDR_LEN + RifLength)) {
			printk("fddi: Invalid RIF.\n");
			goto RequeueRxd;	

		}
		virt[1 + 6] &= ~FDDI_RII;	
		

		virt = cp + RifLength;
		for (n = FDDI_MAC_HDR_LEN; n; n--)
			*--virt = *--cp;
		
		skb_pull(skb, RifLength);
		len -= RifLength;
		RifLength = 0;
	}

	
	smc->os.MacStat.gen.rx_packets++;	
						
	smc->os.MacStat.gen.rx_bytes+=len;	

	
	if (virt[1] & 0x01) {	

		smc->os.MacStat.gen.multicast++;
	}

	
	rxd->rxd_os.skb = NULL;
	skb_trim(skb, len);
	skb->protocol = fddi_type_trans(skb, bp->dev);

	netif_rx(skb);

	HWM_RX_CHECK(smc, RX_LOW_WATERMARK);
	return;

      RequeueRxd:
	pr_debug("Rx: re-queue RXD.\n");
	mac_drv_requeue_rxd(smc, rxd, frag_count);
	smc->os.MacStat.gen.rx_errors++;	
						

}				


void mac_drv_requeue_rxd(struct s_smc *smc, volatile struct s_smt_fp_rxd *rxd,
			 int frag_count)
{
	volatile struct s_smt_fp_rxd *next_rxd;
	volatile struct s_smt_fp_rxd *src_rxd;
	struct sk_buff *skb;
	int MaxFrameSize;
	unsigned char *v_addr;
	dma_addr_t b_addr;

	if (frag_count != 1)	

		printk("fddi: Multi-fragment requeue!\n");

	MaxFrameSize = smc->os.MaxFrameSize;
	src_rxd = rxd;
	for (; frag_count > 0; frag_count--) {
		next_rxd = src_rxd->rxd_next;
		rxd = HWM_GET_CURR_RXD(smc);

		skb = src_rxd->rxd_os.skb;
		if (skb == NULL) {	

			pr_debug("Requeue with no skb in rxd!\n");
			skb = alloc_skb(MaxFrameSize + 3, GFP_ATOMIC);
			if (skb) {
				
				rxd->rxd_os.skb = skb;
				skb_reserve(skb, 3);
				skb_put(skb, MaxFrameSize);
				v_addr = skb->data;
				b_addr = pci_map_single(&smc->os.pdev,
							v_addr,
							MaxFrameSize,
							PCI_DMA_FROMDEVICE);
				rxd->rxd_os.dma_addr = b_addr;
			} else {
				
				pr_debug("Queueing invalid buffer!\n");
				rxd->rxd_os.skb = NULL;
				v_addr = smc->os.LocalRxBuffer;
				b_addr = smc->os.LocalRxBufferDMA;
			}
		} else {
			
			rxd->rxd_os.skb = skb;
			v_addr = skb->data;
			b_addr = pci_map_single(&smc->os.pdev,
						v_addr,
						MaxFrameSize,
						PCI_DMA_FROMDEVICE);
			rxd->rxd_os.dma_addr = b_addr;
		}
		hwm_rx_frag(smc, v_addr, b_addr, MaxFrameSize,
			    FIRST_FRAG | LAST_FRAG);

		src_rxd = next_rxd;
	}
}				


void mac_drv_fill_rxd(struct s_smc *smc)
{
	int MaxFrameSize;
	unsigned char *v_addr;
	unsigned long b_addr;
	struct sk_buff *skb;
	volatile struct s_smt_fp_rxd *rxd;

	pr_debug("entering mac_drv_fill_rxd\n");

	
	

	MaxFrameSize = smc->os.MaxFrameSize;
	
	while (HWM_GET_RX_FREE(smc) > 0) {
		pr_debug(".\n");

		rxd = HWM_GET_CURR_RXD(smc);
		skb = alloc_skb(MaxFrameSize + 3, GFP_ATOMIC);
		if (skb) {
			
			skb_reserve(skb, 3);
			skb_put(skb, MaxFrameSize);
			v_addr = skb->data;
			b_addr = pci_map_single(&smc->os.pdev,
						v_addr,
						MaxFrameSize,
						PCI_DMA_FROMDEVICE);
			rxd->rxd_os.dma_addr = b_addr;
		} else {
			
			
			
			
			
			pr_debug("Queueing invalid buffer!\n");
			v_addr = smc->os.LocalRxBuffer;
			b_addr = smc->os.LocalRxBufferDMA;
		}

		rxd->rxd_os.skb = skb;

		
		hwm_rx_frag(smc, v_addr, b_addr, MaxFrameSize,
			    FIRST_FRAG | LAST_FRAG);
	}
	pr_debug("leaving mac_drv_fill_rxd\n");
}				


void mac_drv_clear_rxd(struct s_smc *smc, volatile struct s_smt_fp_rxd *rxd,
		       int frag_count)
{

	struct sk_buff *skb;

	pr_debug("entering mac_drv_clear_rxd\n");

	if (frag_count != 1)	

		printk("fddi: Multi-fragment clear!\n");

	for (; frag_count > 0; frag_count--) {
		skb = rxd->rxd_os.skb;
		if (skb != NULL) {
			skfddi_priv *bp = &smc->os;
			int MaxFrameSize = bp->MaxFrameSize;

			pci_unmap_single(&bp->pdev, rxd->rxd_os.dma_addr,
					 MaxFrameSize, PCI_DMA_FROMDEVICE);

			dev_kfree_skb(skb);
			rxd->rxd_os.skb = NULL;
		}
		rxd = rxd->rxd_next;	

	}
}				


int mac_drv_rx_init(struct s_smc *smc, int len, int fc,
		    char *look_ahead, int la_len)
{
	struct sk_buff *skb;

	pr_debug("entering mac_drv_rx_init(len=%d)\n", len);

	

	if (len != la_len || len < FDDI_MAC_HDR_LEN || !look_ahead) {
		pr_debug("fddi: Discard invalid local SMT frame\n");
		pr_debug("  len=%d, la_len=%d, (ULONG) look_ahead=%08lXh.\n",
		       len, la_len, (unsigned long) look_ahead);
		return 0;
	}
	skb = alloc_skb(len + 3, GFP_ATOMIC);
	if (!skb) {
		pr_debug("fddi: Local SMT: skb memory exhausted.\n");
		return 0;
	}
	skb_reserve(skb, 3);
	skb_put(skb, len);
	skb_copy_to_linear_data(skb, look_ahead, len);

	
	skb->protocol = fddi_type_trans(skb, smc->os.dev);
	netif_rx(skb);

	return 0;
}				


void smt_timer_poll(struct s_smc *smc)
{
}				


void ring_status_indication(struct s_smc *smc, u_long status)
{
	pr_debug("ring_status_indication( ");
	if (status & RS_RES15)
		pr_debug("RS_RES15 ");
	if (status & RS_HARDERROR)
		pr_debug("RS_HARDERROR ");
	if (status & RS_SOFTERROR)
		pr_debug("RS_SOFTERROR ");
	if (status & RS_BEACON)
		pr_debug("RS_BEACON ");
	if (status & RS_PATHTEST)
		pr_debug("RS_PATHTEST ");
	if (status & RS_SELFTEST)
		pr_debug("RS_SELFTEST ");
	if (status & RS_RES9)
		pr_debug("RS_RES9 ");
	if (status & RS_DISCONNECT)
		pr_debug("RS_DISCONNECT ");
	if (status & RS_RES7)
		pr_debug("RS_RES7 ");
	if (status & RS_DUPADDR)
		pr_debug("RS_DUPADDR ");
	if (status & RS_NORINGOP)
		pr_debug("RS_NORINGOP ");
	if (status & RS_VERSION)
		pr_debug("RS_VERSION ");
	if (status & RS_STUCKBYPASSS)
		pr_debug("RS_STUCKBYPASSS ");
	if (status & RS_EVENT)
		pr_debug("RS_EVENT ");
	if (status & RS_RINGOPCHANGE)
		pr_debug("RS_RINGOPCHANGE ");
	if (status & RS_RES0)
		pr_debug("RS_RES0 ");
	pr_debug("]\n");
}				


unsigned long smt_get_time(void)
{
	return jiffies;
}				


void smt_stat_counter(struct s_smc *smc, int stat)
{

	pr_debug("smt_stat_counter\n");
	switch (stat) {
	case 0:
		pr_debug("Ring operational change.\n");
		break;
	case 1:
		pr_debug("Receive fifo overflow.\n");
		smc->os.MacStat.gen.rx_errors++;
		break;
	default:
		pr_debug("Unknown status (%d).\n", stat);
		break;
	}
}				


void cfm_state_change(struct s_smc *smc, int c_state)
{
#ifdef DRIVERDEBUG
	char *s;

	switch (c_state) {
	case SC0_ISOLATED:
		s = "SC0_ISOLATED";
		break;
	case SC1_WRAP_A:
		s = "SC1_WRAP_A";
		break;
	case SC2_WRAP_B:
		s = "SC2_WRAP_B";
		break;
	case SC4_THRU_A:
		s = "SC4_THRU_A";
		break;
	case SC5_THRU_B:
		s = "SC5_THRU_B";
		break;
	case SC7_WRAP_S:
		s = "SC7_WRAP_S";
		break;
	case SC9_C_WRAP_A:
		s = "SC9_C_WRAP_A";
		break;
	case SC10_C_WRAP_B:
		s = "SC10_C_WRAP_B";
		break;
	case SC11_C_WRAP_S:
		s = "SC11_C_WRAP_S";
		break;
	default:
		pr_debug("cfm_state_change: unknown %d\n", c_state);
		return;
	}
	pr_debug("cfm_state_change: %s\n", s);
#endif				
}				


void ecm_state_change(struct s_smc *smc, int e_state)
{
#ifdef DRIVERDEBUG
	char *s;

	switch (e_state) {
	case EC0_OUT:
		s = "EC0_OUT";
		break;
	case EC1_IN:
		s = "EC1_IN";
		break;
	case EC2_TRACE:
		s = "EC2_TRACE";
		break;
	case EC3_LEAVE:
		s = "EC3_LEAVE";
		break;
	case EC4_PATH_TEST:
		s = "EC4_PATH_TEST";
		break;
	case EC5_INSERT:
		s = "EC5_INSERT";
		break;
	case EC6_CHECK:
		s = "EC6_CHECK";
		break;
	case EC7_DEINSERT:
		s = "EC7_DEINSERT";
		break;
	default:
		s = "unknown";
		break;
	}
	pr_debug("ecm_state_change: %s\n", s);
#endif				
}				


void rmt_state_change(struct s_smc *smc, int r_state)
{
#ifdef DRIVERDEBUG
	char *s;

	switch (r_state) {
	case RM0_ISOLATED:
		s = "RM0_ISOLATED";
		break;
	case RM1_NON_OP:
		s = "RM1_NON_OP - not operational";
		break;
	case RM2_RING_OP:
		s = "RM2_RING_OP - ring operational";
		break;
	case RM3_DETECT:
		s = "RM3_DETECT - detect dupl addresses";
		break;
	case RM4_NON_OP_DUP:
		s = "RM4_NON_OP_DUP - dupl. addr detected";
		break;
	case RM5_RING_OP_DUP:
		s = "RM5_RING_OP_DUP - ring oper. with dupl. addr";
		break;
	case RM6_DIRECTED:
		s = "RM6_DIRECTED - sending directed beacons";
		break;
	case RM7_TRACE:
		s = "RM7_TRACE - trace initiated";
		break;
	default:
		s = "unknown";
		break;
	}
	pr_debug("[rmt_state_change: %s]\n", s);
#endif				
}				


void drv_reset_indication(struct s_smc *smc)
{
	pr_debug("entering drv_reset_indication\n");

	smc->os.ResetRequested = TRUE;	

}				

static struct pci_driver skfddi_pci_driver = {
	.name		= "skfddi",
	.id_table	= skfddi_pci_tbl,
	.probe		= skfp_init_one,
	.remove		= __devexit_p(skfp_remove_one),
};

static int __init skfd_init(void)
{
	return pci_register_driver(&skfddi_pci_driver);
}

static void __exit skfd_exit(void)
{
	pci_unregister_driver(&skfddi_pci_driver);
}

module_init(skfd_init);
module_exit(skfd_exit);
