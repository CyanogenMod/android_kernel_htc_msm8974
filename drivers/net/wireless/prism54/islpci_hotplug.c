/*
 *  Copyright (C) 2002 Intersil Americas Inc.
 *  Copyright (C) 2003 Herbert Valerio Riedel <hvr@gnu.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/init.h> 
#include <linux/dma-mapping.h>

#include "prismcompat.h"
#include "islpci_dev.h"
#include "islpci_mgt.h"		
#include "isl_oid.h"

MODULE_AUTHOR("[Intersil] R.Bastings and W.Termorshuizen, The prism54.org Development Team <prism54-devel@prism54.org>");
MODULE_DESCRIPTION("The Prism54 802.11 Wireless LAN adapter");
MODULE_LICENSE("GPL");

static int	init_pcitm = 0;
module_param(init_pcitm, int, 0);

static DEFINE_PCI_DEVICE_TABLE(prism54_id_tbl) = {
	
	{
	 0x1260, 0x3890,
	 PCI_ANY_ID, PCI_ANY_ID,
	 0, 0, 0
	},

	
	{
	 PCI_VDEVICE(3COM, 0x6001), 0
	},

	
	{
	 0x1260, 0x3877,
	 PCI_ANY_ID, PCI_ANY_ID,
	 0, 0, 0
	},

	
	{
	 0x1260, 0x3886,
	 PCI_ANY_ID, PCI_ANY_ID,
	 0, 0, 0
	},

	
	{0,0,0,0,0,0,0}
};

MODULE_DEVICE_TABLE(pci, prism54_id_tbl);

static int prism54_probe(struct pci_dev *, const struct pci_device_id *);
static void prism54_remove(struct pci_dev *);
static int prism54_suspend(struct pci_dev *, pm_message_t state);
static int prism54_resume(struct pci_dev *);

static struct pci_driver prism54_driver = {
	.name = DRV_NAME,
	.id_table = prism54_id_tbl,
	.probe = prism54_probe,
	.remove = prism54_remove,
	.suspend = prism54_suspend,
	.resume = prism54_resume,
};


static int
prism54_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct net_device *ndev;
	u8 latency_tmr;
	u32 mem_addr;
	islpci_private *priv;
	int rvalue;

	
	if (pci_enable_device(pdev)) {
		printk(KERN_ERR "%s: pci_enable_device() failed.\n", DRV_NAME);
		return -ENODEV;
	}

	
	pci_read_config_byte(pdev, PCI_LATENCY_TIMER, &latency_tmr);
#if VERBOSE > SHOW_ERROR_MESSAGES
	DEBUG(SHOW_TRACING, "latency timer: %x\n", latency_tmr);
#endif
	if (latency_tmr < PCIDEVICE_LATENCY_TIMER_MIN) {
		
		pci_write_config_byte(pdev, PCI_LATENCY_TIMER,
				      PCIDEVICE_LATENCY_TIMER_VAL);
	}

	
	if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32))) {
		printk(KERN_ERR "%s: 32-bit PCI DMA not supported", DRV_NAME);
		goto do_pci_disable_device;
        }

	if ( init_pcitm >= 0 ) {
		pci_write_config_byte(pdev, 0x40, (u8)init_pcitm);
		pci_write_config_byte(pdev, 0x41, (u8)init_pcitm);
	} else {
		printk(KERN_INFO "PCI TRDY/RETRY unchanged\n");
	}

	
	rvalue = pci_request_regions(pdev, DRV_NAME);
	if (rvalue) {
		printk(KERN_ERR "%s: pci_request_regions failure (rc=%d)\n",
		       DRV_NAME, rvalue);
		goto do_pci_disable_device;
	}

	
	rvalue = pci_read_config_dword(pdev, PCI_BASE_ADDRESS_0, &mem_addr);
	if (rvalue || !mem_addr) {
		printk(KERN_ERR "%s: PCI device memory region not configured; fix your BIOS or CardBus bridge/drivers\n",
		       DRV_NAME);
		goto do_pci_release_regions;
	}

	
	DEBUG(SHOW_TRACING, "%s: pci_set_master(pdev)\n", DRV_NAME);
	pci_set_master(pdev);

	
	pci_try_set_mwi(pdev);

	
	if (!(ndev = islpci_setup(pdev))) {
		
		printk(KERN_ERR "%s: could not configure network device\n",
		       DRV_NAME);
		goto do_pci_clear_mwi;
	}

	priv = netdev_priv(ndev);
	islpci_set_state(priv, PRV_STATE_PREBOOT); 

	
	isl38xx_disable_interrupts(priv->device_base);

	
	rvalue = request_irq(pdev->irq, islpci_interrupt,
			     IRQF_SHARED, ndev->name, priv);

	if (rvalue) {
		
		printk(KERN_ERR "%s: could not install IRQ handler\n",
		       ndev->name);
		goto do_unregister_netdev;
	}

	

	return 0;

      do_unregister_netdev:
	unregister_netdev(ndev);
	islpci_free_memory(priv);
	pci_set_drvdata(pdev, NULL);
	free_netdev(ndev);
	priv = NULL;
      do_pci_clear_mwi:
	pci_clear_mwi(pdev);
      do_pci_release_regions:
	pci_release_regions(pdev);
      do_pci_disable_device:
	pci_disable_device(pdev);
	return -EIO;
}

static volatile int __in_cleanup_module = 0;

static void
prism54_remove(struct pci_dev *pdev)
{
	struct net_device *ndev = pci_get_drvdata(pdev);
	islpci_private *priv = ndev ? netdev_priv(ndev) : NULL;
	BUG_ON(!priv);

	if (!__in_cleanup_module) {
		printk(KERN_DEBUG "%s: hot unplug detected\n", ndev->name);
		islpci_set_state(priv, PRV_STATE_OFF);
	}

	printk(KERN_DEBUG "%s: removing device\n", ndev->name);

	unregister_netdev(ndev);

	

	if (islpci_get_state(priv) != PRV_STATE_OFF) {
		isl38xx_disable_interrupts(priv->device_base);
		islpci_set_state(priv, PRV_STATE_OFF);
			
	}

	free_irq(pdev->irq, priv);

	
	islpci_free_memory(priv);

	pci_set_drvdata(pdev, NULL);
	free_netdev(ndev);
	priv = NULL;

	pci_clear_mwi(pdev);

	pci_release_regions(pdev);

	pci_disable_device(pdev);
}

static int
prism54_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct net_device *ndev = pci_get_drvdata(pdev);
	islpci_private *priv = ndev ? netdev_priv(ndev) : NULL;
	BUG_ON(!priv);


	pci_save_state(pdev);

	
	isl38xx_disable_interrupts(priv->device_base);

	islpci_set_state(priv, PRV_STATE_OFF);

	netif_stop_queue(ndev);
	netif_device_detach(ndev);

	return 0;
}

static int
prism54_resume(struct pci_dev *pdev)
{
	struct net_device *ndev = pci_get_drvdata(pdev);
	islpci_private *priv = ndev ? netdev_priv(ndev) : NULL;
	int err;

	BUG_ON(!priv);

	printk(KERN_NOTICE "%s: got resume request\n", ndev->name);

	err = pci_enable_device(pdev);
	if (err) {
		printk(KERN_ERR "%s: pci_enable_device failed on resume\n",
		       ndev->name);
		return err;
	}

	pci_restore_state(pdev);

	
	islpci_reset(priv, 1);

	netif_device_attach(ndev);
	netif_start_queue(ndev);

	return 0;
}

static int __init
prism54_module_init(void)
{
	printk(KERN_INFO "Loaded %s driver, version %s\n",
	       DRV_NAME, DRV_VERSION);

	__bug_on_wrong_struct_sizes ();

	return pci_register_driver(&prism54_driver);
}

static void __exit
prism54_module_exit(void)
{
	__in_cleanup_module = 1;

	pci_unregister_driver(&prism54_driver);

	printk(KERN_INFO "Unloaded %s driver\n", DRV_NAME);

	__in_cleanup_module = 0;
}

module_init(prism54_module_init);
module_exit(prism54_module_exit);
