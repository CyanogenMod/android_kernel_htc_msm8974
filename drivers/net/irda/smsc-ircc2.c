/*********************************************************************
 *
 * Description:   Driver for the SMC Infrared Communications Controller
 * Status:        Experimental.
 * Author:        Daniele Peri (peri@csai.unipa.it)
 * Created at:
 * Modified at:
 * Modified by:
 *
 *     Copyright (c) 2002      Daniele Peri
 *     All Rights Reserved.
 *     Copyright (c) 2002      Jean Tourrilhes
 *     Copyright (c) 2006      Linus Walleij
 *
 *
 * Based on smc-ircc.c:
 *
 *     Copyright (c) 2001      Stefani Seibold
 *     Copyright (c) 1999-2001 Dag Brattli
 *     Copyright (c) 1998-1999 Thomas Davis,
 *
 *	and irport.c:
 *
 *     Copyright (c) 1997, 1998, 1999-2000 Dag Brattli, All Rights Reserved.
 *
 *
 *     This program is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License as
 *     published by the Free Software Foundation; either version 2 of
 *     the License, or (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *     MA 02111-1307 USA
 *
 ********************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/rtnetlink.h>
#include <linux/serial_reg.h>
#include <linux/dma-mapping.h>
#include <linux/pnp.h>
#include <linux/platform_device.h>
#include <linux/gfp.h>

#include <asm/io.h>
#include <asm/dma.h>
#include <asm/byteorder.h>

#include <linux/spinlock.h>
#include <linux/pm.h>
#ifdef CONFIG_PCI
#include <linux/pci.h>
#endif

#include <net/irda/wrapper.h>
#include <net/irda/irda.h>
#include <net/irda/irda_device.h>

#include "smsc-ircc2.h"
#include "smsc-sio.h"


MODULE_AUTHOR("Daniele Peri <peri@csai.unipa.it>");
MODULE_DESCRIPTION("SMC IrCC SIR/FIR controller driver");
MODULE_LICENSE("GPL");

static bool smsc_nopnp = true;
module_param_named(nopnp, smsc_nopnp, bool, 0);
MODULE_PARM_DESC(nopnp, "Do not use PNP to detect controller settings, defaults to true");

#define DMA_INVAL 255
static int ircc_dma = DMA_INVAL;
module_param(ircc_dma, int, 0);
MODULE_PARM_DESC(ircc_dma, "DMA channel");

#define IRQ_INVAL 255
static int ircc_irq = IRQ_INVAL;
module_param(ircc_irq, int, 0);
MODULE_PARM_DESC(ircc_irq, "IRQ line");

static int ircc_fir;
module_param(ircc_fir, int, 0);
MODULE_PARM_DESC(ircc_fir, "FIR Base Address");

static int ircc_sir;
module_param(ircc_sir, int, 0);
MODULE_PARM_DESC(ircc_sir, "SIR Base Address");

static int ircc_cfg;
module_param(ircc_cfg, int, 0);
MODULE_PARM_DESC(ircc_cfg, "Configuration register base address");

static int ircc_transceiver;
module_param(ircc_transceiver, int, 0);
MODULE_PARM_DESC(ircc_transceiver, "Transceiver type");


#ifdef CONFIG_PCI
struct smsc_ircc_subsystem_configuration {
	unsigned short vendor; 
	unsigned short device; 
	unsigned short subvendor; 
	unsigned short subdevice; 
	unsigned short sir_io; 
	unsigned short fir_io; 
	unsigned char  fir_irq; 
	unsigned char  fir_dma; 
	unsigned short cfg_base; 
	int (*preconfigure)(struct pci_dev *dev, struct smsc_ircc_subsystem_configuration *conf); 
	const char *name;	
};
#endif

struct smsc_transceiver {
	char *name;
	void (*set_for_speed)(int fir_base, u32 speed);
	int  (*probe)(int fir_base);
};

struct smsc_chip {
	char *name;
	#if 0
	u8	type;
	#endif
	u16 flags;
	u8 devid;
	u8 rev;
};

struct smsc_chip_address {
	unsigned int cfg_base;
	unsigned int type;
};

struct smsc_ircc_cb {
	struct net_device *netdev;     
	struct irlap_cb    *irlap; 

	chipio_t io;               
	iobuff_t tx_buff;          
	iobuff_t rx_buff;          
	dma_addr_t tx_buff_dma;
	dma_addr_t rx_buff_dma;

	struct qos_info qos;       

	spinlock_t lock;           

	__u32 new_speed;
	__u32 flags;               

	int tx_buff_offsets[10];   
	int tx_len;                

	int transceiver;
	struct platform_device *pldev;
};


#define SMSC_IRCC2_DRIVER_NAME			"smsc-ircc2"

#define SMSC_IRCC2_C_IRDA_FALLBACK_SPEED	9600
#define SMSC_IRCC2_C_DEFAULT_TRANSCEIVER	1
#define SMSC_IRCC2_C_NET_TIMEOUT		0
#define SMSC_IRCC2_C_SIR_STOP			0

static const char *driver_name = SMSC_IRCC2_DRIVER_NAME;


static int smsc_ircc_open(unsigned int firbase, unsigned int sirbase, u8 dma, u8 irq);
static int smsc_ircc_present(unsigned int fir_base, unsigned int sir_base);
static void smsc_ircc_setup_io(struct smsc_ircc_cb *self, unsigned int fir_base, unsigned int sir_base, u8 dma, u8 irq);
static void smsc_ircc_setup_qos(struct smsc_ircc_cb *self);
static void smsc_ircc_init_chip(struct smsc_ircc_cb *self);
static int __exit smsc_ircc_close(struct smsc_ircc_cb *self);
static int  smsc_ircc_dma_receive(struct smsc_ircc_cb *self);
static void smsc_ircc_dma_receive_complete(struct smsc_ircc_cb *self);
static void smsc_ircc_sir_receive(struct smsc_ircc_cb *self);
static netdev_tx_t  smsc_ircc_hard_xmit_sir(struct sk_buff *skb,
						  struct net_device *dev);
static netdev_tx_t  smsc_ircc_hard_xmit_fir(struct sk_buff *skb,
						  struct net_device *dev);
static void smsc_ircc_dma_xmit(struct smsc_ircc_cb *self, int bofs);
static void smsc_ircc_dma_xmit_complete(struct smsc_ircc_cb *self);
static void smsc_ircc_change_speed(struct smsc_ircc_cb *self, u32 speed);
static void smsc_ircc_set_sir_speed(struct smsc_ircc_cb *self, u32 speed);
static irqreturn_t smsc_ircc_interrupt(int irq, void *dev_id);
static irqreturn_t smsc_ircc_interrupt_sir(struct net_device *dev);
static void smsc_ircc_sir_start(struct smsc_ircc_cb *self);
#if SMSC_IRCC2_C_SIR_STOP
static void smsc_ircc_sir_stop(struct smsc_ircc_cb *self);
#endif
static void smsc_ircc_sir_write_wakeup(struct smsc_ircc_cb *self);
static int  smsc_ircc_sir_write(int iobase, int fifo_size, __u8 *buf, int len);
static int  smsc_ircc_net_open(struct net_device *dev);
static int  smsc_ircc_net_close(struct net_device *dev);
static int  smsc_ircc_net_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
#if SMSC_IRCC2_C_NET_TIMEOUT
static void smsc_ircc_timeout(struct net_device *dev);
#endif
static int smsc_ircc_is_receiving(struct smsc_ircc_cb *self);
static void smsc_ircc_probe_transceiver(struct smsc_ircc_cb *self);
static void smsc_ircc_set_transceiver_for_speed(struct smsc_ircc_cb *self, u32 speed);
static void smsc_ircc_sir_wait_hw_transmitter_finish(struct smsc_ircc_cb *self);

static int __init smsc_ircc_look_for_chips(void);
static const struct smsc_chip * __init smsc_ircc_probe(unsigned short cfg_base, u8 reg, const struct smsc_chip *chip, char *type);
static int __init smsc_superio_flat(const struct smsc_chip *chips, unsigned short cfg_base, char *type);
static int __init smsc_superio_paged(const struct smsc_chip *chips, unsigned short cfg_base, char *type);
static int __init smsc_superio_fdc(unsigned short cfg_base);
static int __init smsc_superio_lpc(unsigned short cfg_base);
#ifdef CONFIG_PCI
static int __init preconfigure_smsc_chip(struct smsc_ircc_subsystem_configuration *conf);
static int __init preconfigure_through_82801(struct pci_dev *dev, struct smsc_ircc_subsystem_configuration *conf);
static void __init preconfigure_ali_port(struct pci_dev *dev,
					 unsigned short port);
static int __init preconfigure_through_ali(struct pci_dev *dev, struct smsc_ircc_subsystem_configuration *conf);
static int __init smsc_ircc_preconfigure_subsystems(unsigned short ircc_cfg,
						    unsigned short ircc_fir,
						    unsigned short ircc_sir,
						    unsigned char ircc_dma,
						    unsigned char ircc_irq);
#endif


static void smsc_ircc_set_transceiver_toshiba_sat1800(int fir_base, u32 speed);
static int  smsc_ircc_probe_transceiver_toshiba_sat1800(int fir_base);
static void smsc_ircc_set_transceiver_smsc_ircc_fast_pin_select(int fir_base, u32 speed);
static int  smsc_ircc_probe_transceiver_smsc_ircc_fast_pin_select(int fir_base);
static void smsc_ircc_set_transceiver_smsc_ircc_atc(int fir_base, u32 speed);
static int  smsc_ircc_probe_transceiver_smsc_ircc_atc(int fir_base);


static int smsc_ircc_suspend(struct platform_device *dev, pm_message_t state);
static int smsc_ircc_resume(struct platform_device *dev);

static struct platform_driver smsc_ircc_driver = {
	.suspend	= smsc_ircc_suspend,
	.resume		= smsc_ircc_resume,
	.driver		= {
		.name	= SMSC_IRCC2_DRIVER_NAME,
	},
};


static struct smsc_transceiver smsc_transceivers[] =
{
	{ "Toshiba Satellite 1800 (GP data pin select)", smsc_ircc_set_transceiver_toshiba_sat1800, smsc_ircc_probe_transceiver_toshiba_sat1800 },
	{ "Fast pin select", smsc_ircc_set_transceiver_smsc_ircc_fast_pin_select, smsc_ircc_probe_transceiver_smsc_ircc_fast_pin_select },
	{ "ATC IRMode", smsc_ircc_set_transceiver_smsc_ircc_atc, smsc_ircc_probe_transceiver_smsc_ircc_atc },
	{ NULL, NULL }
};
#define SMSC_IRCC2_C_NUMBER_OF_TRANSCEIVERS (ARRAY_SIZE(smsc_transceivers) - 1)


#define	KEY55_1	0	
#define	KEY55_2	1	
#define	NoIRDA	2	
#define	SIR	0	
#define	FIR	4	
#define	SERx4	8	

static struct smsc_chip __initdata fdc_chips_flat[] =
{
	
	{ "37C44",	KEY55_1|NoIRDA,		0x00, 0x00 }, 
	{ "37C665GT",	KEY55_2|NoIRDA,		0x65, 0x01 },
	{ "37C665GT",	KEY55_2|NoIRDA,		0x66, 0x01 },
	{ "37C669",	KEY55_2|SIR|SERx4,	0x03, 0x02 },
	{ "37C669",	KEY55_2|SIR|SERx4,	0x04, 0x02 }, 
	{ "37C78",	KEY55_2|NoIRDA,		0x78, 0x00 },
	{ "37N769",	KEY55_1|FIR|SERx4,	0x28, 0x00 },
	{ "37N869",	KEY55_1|FIR|SERx4,	0x29, 0x00 },
	{ NULL }
};

static struct smsc_chip __initdata fdc_chips_paged[] =
{
	
	{ "37B72X",	KEY55_1|SIR|SERx4,	0x4c, 0x00 },
	{ "37B77X",	KEY55_1|SIR|SERx4,	0x43, 0x00 },
	{ "37B78X",	KEY55_1|SIR|SERx4,	0x44, 0x00 },
	{ "37B80X",	KEY55_1|SIR|SERx4,	0x42, 0x00 },
	{ "37C67X",	KEY55_1|FIR|SERx4,	0x40, 0x00 },
	{ "37C93X",	KEY55_2|SIR|SERx4,	0x02, 0x01 },
	{ "37C93XAPM",	KEY55_1|SIR|SERx4,	0x30, 0x01 },
	{ "37C93XFR",	KEY55_2|FIR|SERx4,	0x03, 0x01 },
	{ "37M707",	KEY55_1|SIR|SERx4,	0x42, 0x00 },
	{ "37M81X",	KEY55_1|SIR|SERx4,	0x4d, 0x00 },
	{ "37N958FR",	KEY55_1|FIR|SERx4,	0x09, 0x04 },
	{ "37N971",	KEY55_1|FIR|SERx4,	0x0a, 0x00 },
	{ "37N972",	KEY55_1|FIR|SERx4,	0x0b, 0x00 },
	{ NULL }
};

static struct smsc_chip __initdata lpc_chips_flat[] =
{
	
	{ "47N227",	KEY55_1|FIR|SERx4,	0x5a, 0x00 },
	{ "47N227",	KEY55_1|FIR|SERx4,	0x7a, 0x00 },
	{ "47N267",	KEY55_1|FIR|SERx4,	0x5e, 0x00 },
	{ NULL }
};

static struct smsc_chip __initdata lpc_chips_paged[] =
{
	
	{ "47B27X",	KEY55_1|SIR|SERx4,	0x51, 0x00 },
	{ "47B37X",	KEY55_1|SIR|SERx4,	0x52, 0x00 },
	{ "47M10X",	KEY55_1|SIR|SERx4,	0x59, 0x00 },
	{ "47M120",	KEY55_1|NoIRDA|SERx4,	0x5c, 0x00 },
	{ "47M13X",	KEY55_1|SIR|SERx4,	0x59, 0x00 },
	{ "47M14X",	KEY55_1|SIR|SERx4,	0x5f, 0x00 },
	{ "47N252",	KEY55_1|FIR|SERx4,	0x0e, 0x00 },
	{ "47S42X",	KEY55_1|SIR|SERx4,	0x57, 0x00 },
	{ NULL }
};

#define SMSCSIO_TYPE_FDC	1
#define SMSCSIO_TYPE_LPC	2
#define SMSCSIO_TYPE_FLAT	4
#define SMSCSIO_TYPE_PAGED	8

static struct smsc_chip_address __initdata possible_addresses[] =
{
	{ 0x3f0, SMSCSIO_TYPE_FDC|SMSCSIO_TYPE_FLAT|SMSCSIO_TYPE_PAGED },
	{ 0x370, SMSCSIO_TYPE_FDC|SMSCSIO_TYPE_FLAT|SMSCSIO_TYPE_PAGED },
	{ 0xe0,  SMSCSIO_TYPE_FDC|SMSCSIO_TYPE_FLAT|SMSCSIO_TYPE_PAGED },
	{ 0x2e,  SMSCSIO_TYPE_LPC|SMSCSIO_TYPE_FLAT|SMSCSIO_TYPE_PAGED },
	{ 0x4e,  SMSCSIO_TYPE_LPC|SMSCSIO_TYPE_FLAT|SMSCSIO_TYPE_PAGED },
	{ 0, 0 }
};


static struct smsc_ircc_cb *dev_self[] = { NULL, NULL };
static unsigned short dev_count;

static inline void register_bank(int iobase, int bank)
{
        outb(((inb(iobase + IRCC_MASTER) & 0xf0) | (bank & 0x07)),
               iobase + IRCC_MASTER);
}

static const struct pnp_device_id smsc_ircc_pnp_table[] = {
	{ .id = "SMCf010", .driver_data = 0 },
	
	{ }
};
MODULE_DEVICE_TABLE(pnp, smsc_ircc_pnp_table);

static int pnp_driver_registered;

#ifdef CONFIG_PNP
static int __devinit smsc_ircc_pnp_probe(struct pnp_dev *dev,
				      const struct pnp_device_id *dev_id)
{
	unsigned int firbase, sirbase;
	u8 dma, irq;

	if (!(pnp_port_valid(dev, 0) && pnp_port_valid(dev, 1) &&
	      pnp_dma_valid(dev, 0) && pnp_irq_valid(dev, 0)))
		return -EINVAL;

	sirbase = pnp_port_start(dev, 0);
	firbase = pnp_port_start(dev, 1);
	dma = pnp_dma(dev, 0);
	irq = pnp_irq(dev, 0);

	if (smsc_ircc_open(firbase, sirbase, dma, irq))
		return -ENODEV;

	return 0;
}

static struct pnp_driver smsc_ircc_pnp_driver = {
	.name		= "smsc-ircc2",
	.id_table	= smsc_ircc_pnp_table,
	.probe		= smsc_ircc_pnp_probe,
};
#else 
static struct pnp_driver smsc_ircc_pnp_driver;
#endif


static int __init smsc_ircc_legacy_probe(void)
{
	int ret = 0;

#ifdef CONFIG_PCI
	if (smsc_ircc_preconfigure_subsystems(ircc_cfg, ircc_fir, ircc_sir, ircc_dma, ircc_irq) < 0) {
		
		IRDA_ERROR("%s, Preconfiguration failed !\n", driver_name);
	}
#endif

	if (ircc_fir > 0 && ircc_sir > 0) {
		IRDA_MESSAGE(" Overriding FIR address 0x%04x\n", ircc_fir);
		IRDA_MESSAGE(" Overriding SIR address 0x%04x\n", ircc_sir);

		if (smsc_ircc_open(ircc_fir, ircc_sir, ircc_dma, ircc_irq))
			ret = -ENODEV;
	} else {
		ret = -ENODEV;

		
		if (ircc_cfg > 0) {
			IRDA_MESSAGE(" Overriding configuration address "
				     "0x%04x\n", ircc_cfg);
			if (!smsc_superio_fdc(ircc_cfg))
				ret = 0;
			if (!smsc_superio_lpc(ircc_cfg))
				ret = 0;
		}

		if (smsc_ircc_look_for_chips() > 0)
			ret = 0;
	}
	return ret;
}

static int __init smsc_ircc_init(void)
{
	int ret;

	IRDA_DEBUG(1, "%s\n", __func__);

	ret = platform_driver_register(&smsc_ircc_driver);
	if (ret) {
		IRDA_ERROR("%s, Can't register driver!\n", driver_name);
		return ret;
	}

	dev_count = 0;

	if (smsc_nopnp || !pnp_platform_devices ||
	    ircc_cfg || ircc_fir || ircc_sir ||
	    ircc_dma != DMA_INVAL || ircc_irq != IRQ_INVAL) {
		ret = smsc_ircc_legacy_probe();
	} else {
		if (pnp_register_driver(&smsc_ircc_pnp_driver) == 0)
			pnp_driver_registered = 1;
	}

	if (ret) {
		if (pnp_driver_registered)
			pnp_unregister_driver(&smsc_ircc_pnp_driver);
		platform_driver_unregister(&smsc_ircc_driver);
	}

	return ret;
}

static netdev_tx_t smsc_ircc_net_xmit(struct sk_buff *skb,
					    struct net_device *dev)
{
	struct smsc_ircc_cb *self = netdev_priv(dev);

	if (self->io.speed > 115200)
		return 	smsc_ircc_hard_xmit_fir(skb, dev);
	else
		return 	smsc_ircc_hard_xmit_sir(skb, dev);
}

static const struct net_device_ops smsc_ircc_netdev_ops = {
	.ndo_open       = smsc_ircc_net_open,
	.ndo_stop       = smsc_ircc_net_close,
	.ndo_do_ioctl   = smsc_ircc_net_ioctl,
	.ndo_start_xmit = smsc_ircc_net_xmit,
#if SMSC_IRCC2_C_NET_TIMEOUT
	.ndo_tx_timeout	= smsc_ircc_timeout,
#endif
};

static int __devinit smsc_ircc_open(unsigned int fir_base, unsigned int sir_base, u8 dma, u8 irq)
{
	struct smsc_ircc_cb *self;
	struct net_device *dev;
	int err;

	IRDA_DEBUG(1, "%s\n", __func__);

	err = smsc_ircc_present(fir_base, sir_base);
	if (err)
		goto err_out;

	err = -ENOMEM;
	if (dev_count >= ARRAY_SIZE(dev_self)) {
	        IRDA_WARNING("%s(), too many devices!\n", __func__);
		goto err_out1;
	}

	dev = alloc_irdadev(sizeof(struct smsc_ircc_cb));
	if (!dev) {
		IRDA_WARNING("%s() can't allocate net device\n", __func__);
		goto err_out1;
	}

#if SMSC_IRCC2_C_NET_TIMEOUT
	dev->watchdog_timeo  = HZ * 2;  
#endif
	dev->netdev_ops = &smsc_ircc_netdev_ops;

	self = netdev_priv(dev);
	self->netdev = dev;

	
	dev->base_addr = self->io.fir_base = fir_base;
	dev->irq = self->io.irq = irq;

	
	dev_self[dev_count] = self;
	spin_lock_init(&self->lock);

	self->rx_buff.truesize = SMSC_IRCC2_RX_BUFF_TRUESIZE;
	self->tx_buff.truesize = SMSC_IRCC2_TX_BUFF_TRUESIZE;

	self->rx_buff.head =
		dma_alloc_coherent(NULL, self->rx_buff.truesize,
				   &self->rx_buff_dma, GFP_KERNEL);
	if (self->rx_buff.head == NULL) {
		IRDA_ERROR("%s, Can't allocate memory for receive buffer!\n",
			   driver_name);
		goto err_out2;
	}

	self->tx_buff.head =
		dma_alloc_coherent(NULL, self->tx_buff.truesize,
				   &self->tx_buff_dma, GFP_KERNEL);
	if (self->tx_buff.head == NULL) {
		IRDA_ERROR("%s, Can't allocate memory for transmit buffer!\n",
			   driver_name);
		goto err_out3;
	}

	memset(self->rx_buff.head, 0, self->rx_buff.truesize);
	memset(self->tx_buff.head, 0, self->tx_buff.truesize);

	self->rx_buff.in_frame = FALSE;
	self->rx_buff.state = OUTSIDE_FRAME;
	self->tx_buff.data = self->tx_buff.head;
	self->rx_buff.data = self->rx_buff.head;

	smsc_ircc_setup_io(self, fir_base, sir_base, dma, irq);
	smsc_ircc_setup_qos(self);
	smsc_ircc_init_chip(self);

	if (ircc_transceiver > 0  &&
	    ircc_transceiver < SMSC_IRCC2_C_NUMBER_OF_TRANSCEIVERS)
		self->transceiver = ircc_transceiver;
	else
		smsc_ircc_probe_transceiver(self);

	err = register_netdev(self->netdev);
	if (err) {
		IRDA_ERROR("%s, Network device registration failed!\n",
			   driver_name);
		goto err_out4;
	}

	self->pldev = platform_device_register_simple(SMSC_IRCC2_DRIVER_NAME,
						      dev_count, NULL, 0);
	if (IS_ERR(self->pldev)) {
		err = PTR_ERR(self->pldev);
		goto err_out5;
	}
	platform_set_drvdata(self->pldev, self);

	IRDA_MESSAGE("IrDA: Registered device %s\n", dev->name);
	dev_count++;

	return 0;

 err_out5:
	unregister_netdev(self->netdev);

 err_out4:
	dma_free_coherent(NULL, self->tx_buff.truesize,
			  self->tx_buff.head, self->tx_buff_dma);
 err_out3:
	dma_free_coherent(NULL, self->rx_buff.truesize,
			  self->rx_buff.head, self->rx_buff_dma);
 err_out2:
	free_netdev(self->netdev);
	dev_self[dev_count] = NULL;
 err_out1:
	release_region(fir_base, SMSC_IRCC2_FIR_CHIP_IO_EXTENT);
	release_region(sir_base, SMSC_IRCC2_SIR_CHIP_IO_EXTENT);
 err_out:
	return err;
}

static int smsc_ircc_present(unsigned int fir_base, unsigned int sir_base)
{
	unsigned char low, high, chip, config, dma, irq, version;

	if (!request_region(fir_base, SMSC_IRCC2_FIR_CHIP_IO_EXTENT,
			    driver_name)) {
		IRDA_WARNING("%s: can't get fir_base of 0x%03x\n",
			     __func__, fir_base);
		goto out1;
	}

	if (!request_region(sir_base, SMSC_IRCC2_SIR_CHIP_IO_EXTENT,
			    driver_name)) {
		IRDA_WARNING("%s: can't get sir_base of 0x%03x\n",
			     __func__, sir_base);
		goto out2;
	}

	register_bank(fir_base, 3);

	high    = inb(fir_base + IRCC_ID_HIGH);
	low     = inb(fir_base + IRCC_ID_LOW);
	chip    = inb(fir_base + IRCC_CHIP_ID);
	version = inb(fir_base + IRCC_VERSION);
	config  = inb(fir_base + IRCC_INTERFACE);
	dma     = config & IRCC_INTERFACE_DMA_MASK;
	irq     = (config & IRCC_INTERFACE_IRQ_MASK) >> 4;

	if (high != 0x10 || low != 0xb8 || (chip != 0xf1 && chip != 0xf2)) {
		IRDA_WARNING("%s(), addr 0x%04x - no device found!\n",
			     __func__, fir_base);
		goto out3;
	}
	IRDA_MESSAGE("SMsC IrDA Controller found\n IrCC version %d.%d, "
		     "firport 0x%03x, sirport 0x%03x dma=%d, irq=%d\n",
		     chip & 0x0f, version, fir_base, sir_base, dma, irq);

	return 0;

 out3:
	release_region(sir_base, SMSC_IRCC2_SIR_CHIP_IO_EXTENT);
 out2:
	release_region(fir_base, SMSC_IRCC2_FIR_CHIP_IO_EXTENT);
 out1:
	return -ENODEV;
}

static void smsc_ircc_setup_io(struct smsc_ircc_cb *self,
			       unsigned int fir_base, unsigned int sir_base,
			       u8 dma, u8 irq)
{
	unsigned char config, chip_dma, chip_irq;

	register_bank(fir_base, 3);
	config = inb(fir_base + IRCC_INTERFACE);
	chip_dma = config & IRCC_INTERFACE_DMA_MASK;
	chip_irq = (config & IRCC_INTERFACE_IRQ_MASK) >> 4;

	self->io.fir_base  = fir_base;
	self->io.sir_base  = sir_base;
	self->io.fir_ext   = SMSC_IRCC2_FIR_CHIP_IO_EXTENT;
	self->io.sir_ext   = SMSC_IRCC2_SIR_CHIP_IO_EXTENT;
	self->io.fifo_size = SMSC_IRCC2_FIFO_SIZE;
	self->io.speed = SMSC_IRCC2_C_IRDA_FALLBACK_SPEED;

	if (irq != IRQ_INVAL) {
		if (irq != chip_irq)
			IRDA_MESSAGE("%s, Overriding IRQ - chip says %d, using %d\n",
				     driver_name, chip_irq, irq);
		self->io.irq = irq;
	} else
		self->io.irq = chip_irq;

	if (dma != DMA_INVAL) {
		if (dma != chip_dma)
			IRDA_MESSAGE("%s, Overriding DMA - chip says %d, using %d\n",
				     driver_name, chip_dma, dma);
		self->io.dma = dma;
	} else
		self->io.dma = chip_dma;

}

static void smsc_ircc_setup_qos(struct smsc_ircc_cb *self)
{
	
	irda_init_max_qos_capabilies(&self->qos);

	self->qos.baud_rate.bits = IR_9600|IR_19200|IR_38400|IR_57600|
		IR_115200|IR_576000|IR_1152000|(IR_4000000 << 8);

	self->qos.min_turn_time.bits = SMSC_IRCC2_MIN_TURN_TIME;
	self->qos.window_size.bits = SMSC_IRCC2_WINDOW_SIZE;
	irda_qos_bits_to_value(&self->qos);
}

static void smsc_ircc_init_chip(struct smsc_ircc_cb *self)
{
	int iobase = self->io.fir_base;

	register_bank(iobase, 0);
	outb(IRCC_MASTER_RESET, iobase + IRCC_MASTER);
	outb(0x00, iobase + IRCC_MASTER);

	register_bank(iobase, 1);
	outb(((inb(iobase + IRCC_SCE_CFGA) & 0x87) | IRCC_CFGA_IRDA_SIR_A),
	     iobase + IRCC_SCE_CFGA);

#ifdef smsc_669 
	outb(((inb(iobase + IRCC_SCE_CFGB) & 0x3f) | IRCC_CFGB_MUX_COM),
	     iobase + IRCC_SCE_CFGB);
#else
	outb(((inb(iobase + IRCC_SCE_CFGB) & 0x3f) | IRCC_CFGB_MUX_IR),
	     iobase + IRCC_SCE_CFGB);
#endif
	(void) inb(iobase + IRCC_FIFO_THRESHOLD);
	outb(SMSC_IRCC2_FIFO_THRESHOLD, iobase + IRCC_FIFO_THRESHOLD);

	register_bank(iobase, 4);
	outb((inb(iobase + IRCC_CONTROL) & 0x30), iobase + IRCC_CONTROL);

	register_bank(iobase, 0);
	outb(0, iobase + IRCC_LCR_A);

	smsc_ircc_set_sir_speed(self, SMSC_IRCC2_C_IRDA_FALLBACK_SPEED);

	
	outb(0x00, iobase + IRCC_MASTER);
}

static int smsc_ircc_net_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct if_irda_req *irq = (struct if_irda_req *) rq;
	struct smsc_ircc_cb *self;
	unsigned long flags;
	int ret = 0;

	IRDA_ASSERT(dev != NULL, return -1;);

	self = netdev_priv(dev);

	IRDA_ASSERT(self != NULL, return -1;);

	IRDA_DEBUG(2, "%s(), %s, (cmd=0x%X)\n", __func__, dev->name, cmd);

	switch (cmd) {
	case SIOCSBANDWIDTH: 
		if (!capable(CAP_NET_ADMIN))
			ret = -EPERM;
                else {
			spin_lock_irqsave(&self->lock, flags);
			smsc_ircc_change_speed(self, irq->ifr_baudrate);
			spin_unlock_irqrestore(&self->lock, flags);
		}
		break;
	case SIOCSMEDIABUSY: 
		if (!capable(CAP_NET_ADMIN)) {
			ret = -EPERM;
			break;
		}

		irda_device_set_media_busy(self->netdev, TRUE);
		break;
	case SIOCGRECEIVING: 
		irq->ifr_receiving = smsc_ircc_is_receiving(self);
		break;
	#if 0
	case SIOCSDTRRTS:
		if (!capable(CAP_NET_ADMIN)) {
			ret = -EPERM;
			break;
		}
		smsc_ircc_sir_set_dtr_rts(dev, irq->ifr_dtr, irq->ifr_rts);
		break;
	#endif
	default:
		ret = -EOPNOTSUPP;
	}

	return ret;
}

#if SMSC_IRCC2_C_NET_TIMEOUT

static void smsc_ircc_timeout(struct net_device *dev)
{
	struct smsc_ircc_cb *self = netdev_priv(dev);
	unsigned long flags;

	IRDA_WARNING("%s: transmit timed out, changing speed to: %d\n",
		     dev->name, self->io.speed);
	spin_lock_irqsave(&self->lock, flags);
	smsc_ircc_sir_start(self);
	smsc_ircc_change_speed(self, self->io.speed);
	dev->trans_start = jiffies; 
	netif_wake_queue(dev);
	spin_unlock_irqrestore(&self->lock, flags);
}
#endif

static netdev_tx_t smsc_ircc_hard_xmit_sir(struct sk_buff *skb,
						 struct net_device *dev)
{
	struct smsc_ircc_cb *self;
	unsigned long flags;
	s32 speed;

	IRDA_DEBUG(1, "%s\n", __func__);

	IRDA_ASSERT(dev != NULL, return NETDEV_TX_OK;);

	self = netdev_priv(dev);
	IRDA_ASSERT(self != NULL, return NETDEV_TX_OK;);

	netif_stop_queue(dev);

	
	spin_lock_irqsave(&self->lock, flags);

	
	speed = irda_get_next_speed(skb);
	if (speed != self->io.speed && speed != -1) {
		
		if (!skb->len) {
			smsc_ircc_sir_wait_hw_transmitter_finish(self);
			smsc_ircc_change_speed(self, speed);
			spin_unlock_irqrestore(&self->lock, flags);
			dev_kfree_skb(skb);
			return NETDEV_TX_OK;
		}
		self->new_speed = speed;
	}

	
	self->tx_buff.data = self->tx_buff.head;

	
	self->tx_buff.len = async_wrap_skb(skb, self->tx_buff.data,
					   self->tx_buff.truesize);

	dev->stats.tx_bytes += self->tx_buff.len;

	
	outb(UART_IER_THRI, self->io.sir_base + UART_IER);

	spin_unlock_irqrestore(&self->lock, flags);

	dev_kfree_skb(skb);

	return NETDEV_TX_OK;
}

static void smsc_ircc_set_fir_speed(struct smsc_ircc_cb *self, u32 speed)
{
	int fir_base, ir_mode, ctrl, fast;

	IRDA_ASSERT(self != NULL, return;);
	fir_base = self->io.fir_base;

	self->io.speed = speed;

	switch (speed) {
	default:
	case 576000:
		ir_mode = IRCC_CFGA_IRDA_HDLC;
		ctrl = IRCC_CRC;
		fast = 0;
		IRDA_DEBUG(0, "%s(), handling baud of 576000\n", __func__);
		break;
	case 1152000:
		ir_mode = IRCC_CFGA_IRDA_HDLC;
		ctrl = IRCC_1152 | IRCC_CRC;
		fast = IRCC_LCR_A_FAST | IRCC_LCR_A_GP_DATA;
		IRDA_DEBUG(0, "%s(), handling baud of 1152000\n",
			   __func__);
		break;
	case 4000000:
		ir_mode = IRCC_CFGA_IRDA_4PPM;
		ctrl = IRCC_CRC;
		fast = IRCC_LCR_A_FAST;
		IRDA_DEBUG(0, "%s(), handling baud of 4000000\n",
			   __func__);
		break;
	}
	#if 0
	Now in tranceiver!
	
	register_bank(fir_base, 0);
	outb((inb(fir_base + IRCC_LCR_A) &  0xbf) | fast, fir_base + IRCC_LCR_A);
	#endif

	register_bank(fir_base, 1);
	outb(((inb(fir_base + IRCC_SCE_CFGA) & IRCC_SCE_CFGA_BLOCK_CTRL_BITS_MASK) | ir_mode), fir_base + IRCC_SCE_CFGA);

	register_bank(fir_base, 4);
	outb((inb(fir_base + IRCC_CONTROL) & 0x30) | ctrl, fir_base + IRCC_CONTROL);
}

static void smsc_ircc_fir_start(struct smsc_ircc_cb *self)
{
	struct net_device *dev;
	int fir_base;

	IRDA_DEBUG(1, "%s\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	dev = self->netdev;
	IRDA_ASSERT(dev != NULL, return;);

	fir_base = self->io.fir_base;

	

	
	outb(inb(fir_base + IRCC_LCR_A) | IRCC_LCR_A_FIFO_RESET, fir_base + IRCC_LCR_A);

	
	

	register_bank(fir_base, 1);

	
#ifdef SMSC_669 
	outb(((inb(fir_base + IRCC_SCE_CFGB) & 0x3f) | IRCC_CFGB_MUX_COM),
	     fir_base + IRCC_SCE_CFGB);
#else
	outb(((inb(fir_base + IRCC_SCE_CFGB) & 0x3f) | IRCC_CFGB_MUX_IR),
	     fir_base + IRCC_SCE_CFGB);
#endif
	(void) inb(fir_base + IRCC_FIFO_THRESHOLD);

	
	outb(0, fir_base + IRCC_MASTER);
	register_bank(fir_base, 0);
	outb(IRCC_IER_ACTIVE_FRAME | IRCC_IER_EOM, fir_base + IRCC_IER);
	outb(IRCC_MASTER_INT_EN, fir_base + IRCC_MASTER);
}

static void smsc_ircc_fir_stop(struct smsc_ircc_cb *self)
{
	int fir_base;

	IRDA_DEBUG(1, "%s\n", __func__);

	IRDA_ASSERT(self != NULL, return;);

	fir_base = self->io.fir_base;
	register_bank(fir_base, 0);
	
	outb(inb(fir_base + IRCC_LCR_B) & IRCC_LCR_B_SIP_ENABLE, fir_base + IRCC_LCR_B);
}


static void smsc_ircc_change_speed(struct smsc_ircc_cb *self, u32 speed)
{
	struct net_device *dev;
	int last_speed_was_sir;

	IRDA_DEBUG(0, "%s() changing speed to: %d\n", __func__, speed);

	IRDA_ASSERT(self != NULL, return;);
	dev = self->netdev;

	last_speed_was_sir = self->io.speed <= SMSC_IRCC2_MAX_SIR_SPEED;

	#if 0
	
	speed= 1152000;
	self->io.speed = speed;
	last_speed_was_sir = 0;
	smsc_ircc_fir_start(self);
	#endif

	if (self->io.speed == 0)
		smsc_ircc_sir_start(self);

	#if 0
	if (!last_speed_was_sir) speed = self->io.speed;
	#endif

	if (self->io.speed != speed)
		smsc_ircc_set_transceiver_for_speed(self, speed);

	self->io.speed = speed;

	if (speed <= SMSC_IRCC2_MAX_SIR_SPEED) {
		if (!last_speed_was_sir) {
			smsc_ircc_fir_stop(self);
			smsc_ircc_sir_start(self);
		}
		smsc_ircc_set_sir_speed(self, speed);
	} else {
		if (last_speed_was_sir) {
			#if SMSC_IRCC2_C_SIR_STOP
			smsc_ircc_sir_stop(self);
			#endif
			smsc_ircc_fir_start(self);
		}
		smsc_ircc_set_fir_speed(self, speed);

		#if 0
		self->tx_buff.len = 10;
		self->tx_buff.data = self->tx_buff.head;

		smsc_ircc_dma_xmit(self, 4000);
		#endif
		
		smsc_ircc_dma_receive(self);
	}

	netif_wake_queue(dev);
}

static void smsc_ircc_set_sir_speed(struct smsc_ircc_cb *self, __u32 speed)
{
	int iobase;
	int fcr;    
	int lcr;    
	int divisor;

	IRDA_DEBUG(0, "%s(), Setting speed to: %d\n", __func__, speed);

	IRDA_ASSERT(self != NULL, return;);
	iobase = self->io.sir_base;

	
	self->io.speed = speed;

	
	outb(0, iobase + UART_IER);

	divisor = SMSC_IRCC2_MAX_SIR_SPEED / speed;

	fcr = UART_FCR_ENABLE_FIFO;

	fcr |= self->io.speed < 38400 ?
		UART_FCR_TRIGGER_1 : UART_FCR_TRIGGER_14;

	
	lcr = UART_LCR_WLEN8;

	outb(UART_LCR_DLAB | lcr, iobase + UART_LCR); 
	outb(divisor & 0xff,      iobase + UART_DLL); 
	outb(divisor >> 8,	  iobase + UART_DLM);
	outb(lcr,		  iobase + UART_LCR); 
	outb(fcr,		  iobase + UART_FCR); 

	
	outb(UART_IER_RLSI | UART_IER_RDI | UART_IER_THRI, iobase + UART_IER);

	IRDA_DEBUG(2, "%s() speed changed to: %d\n", __func__, speed);
}


static netdev_tx_t smsc_ircc_hard_xmit_fir(struct sk_buff *skb,
						 struct net_device *dev)
{
	struct smsc_ircc_cb *self;
	unsigned long flags;
	s32 speed;
	int mtt;

	IRDA_ASSERT(dev != NULL, return NETDEV_TX_OK;);
	self = netdev_priv(dev);
	IRDA_ASSERT(self != NULL, return NETDEV_TX_OK;);

	netif_stop_queue(dev);

	
	spin_lock_irqsave(&self->lock, flags);

	
	speed = irda_get_next_speed(skb);
	if (speed != self->io.speed && speed != -1) {
		
		if (!skb->len) {
			smsc_ircc_change_speed(self, speed);
			spin_unlock_irqrestore(&self->lock, flags);
			dev_kfree_skb(skb);
			return NETDEV_TX_OK;
		}

		self->new_speed = speed;
	}

	skb_copy_from_linear_data(skb, self->tx_buff.head, skb->len);

	self->tx_buff.len = skb->len;
	self->tx_buff.data = self->tx_buff.head;

	mtt = irda_get_mtt(skb);
	if (mtt) {
		int bofs;

		bofs = mtt * (self->io.speed / 1000) / 8000;
		if (bofs > 4095)
			bofs = 4095;

		smsc_ircc_dma_xmit(self, bofs);
	} else {
		
		smsc_ircc_dma_xmit(self, 0);
	}

	spin_unlock_irqrestore(&self->lock, flags);
	dev_kfree_skb(skb);

	return NETDEV_TX_OK;
}

static void smsc_ircc_dma_xmit(struct smsc_ircc_cb *self, int bofs)
{
	int iobase = self->io.fir_base;
	u8 ctrl;

	IRDA_DEBUG(3, "%s\n", __func__);
#if 1
	
	register_bank(iobase, 0);
	outb(0x00, iobase + IRCC_LCR_B);
#endif
	register_bank(iobase, 1);
	outb(inb(iobase + IRCC_SCE_CFGB) & ~IRCC_CFGB_DMA_ENABLE,
	     iobase + IRCC_SCE_CFGB);

	self->io.direction = IO_XMIT;

	
	register_bank(iobase, 4);
	outb(bofs & 0xff, iobase + IRCC_BOF_COUNT_LO);
	ctrl = inb(iobase + IRCC_CONTROL) & 0xf0;
	outb(ctrl | ((bofs >> 8) & 0x0f), iobase + IRCC_BOF_COUNT_HI);

	
	outb(self->tx_buff.len >> 8, iobase + IRCC_TX_SIZE_HI);
	outb(self->tx_buff.len & 0xff, iobase + IRCC_TX_SIZE_LO);

	

	
	register_bank(iobase, 1);
	outb(inb(iobase + IRCC_SCE_CFGB) | IRCC_CFGB_DMA_ENABLE |
	     IRCC_CFGB_DMA_BURST, iobase + IRCC_SCE_CFGB);

	
	irda_setup_dma(self->io.dma, self->tx_buff_dma, self->tx_buff.len,
		       DMA_TX_MODE);

	

	register_bank(iobase, 0);
	outb(IRCC_IER_ACTIVE_FRAME | IRCC_IER_EOM, iobase + IRCC_IER);
	outb(IRCC_MASTER_INT_EN, iobase + IRCC_MASTER);

	
	outb(IRCC_LCR_B_SCE_TRANSMIT | IRCC_LCR_B_SIP_ENABLE, iobase + IRCC_LCR_B);
}

static void smsc_ircc_dma_xmit_complete(struct smsc_ircc_cb *self)
{
	int iobase = self->io.fir_base;

	IRDA_DEBUG(3, "%s\n", __func__);
#if 0
	
	register_bank(iobase, 0);
	outb(0x00, iobase + IRCC_LCR_B);
#endif
	register_bank(iobase, 1);
	outb(inb(iobase + IRCC_SCE_CFGB) & ~IRCC_CFGB_DMA_ENABLE,
	     iobase + IRCC_SCE_CFGB);

	
	register_bank(iobase, 0);
	if (inb(iobase + IRCC_LSR) & IRCC_LSR_UNDERRUN) {
		self->netdev->stats.tx_errors++;
		self->netdev->stats.tx_fifo_errors++;

		
		register_bank(iobase, 0);
		outb(IRCC_MASTER_ERROR_RESET, iobase + IRCC_MASTER);
		outb(0x00, iobase + IRCC_MASTER);
	} else {
		self->netdev->stats.tx_packets++;
		self->netdev->stats.tx_bytes += self->tx_buff.len;
	}

	
	if (self->new_speed) {
		smsc_ircc_change_speed(self, self->new_speed);
		self->new_speed = 0;
	}

	netif_wake_queue(self->netdev);
}

static int smsc_ircc_dma_receive(struct smsc_ircc_cb *self)
{
	int iobase = self->io.fir_base;
#if 0
	
	register_bank(iobase, 1);
	outb(inb(iobase + IRCC_SCE_CFGB) & ~IRCC_CFGB_DMA_ENABLE,
	     iobase + IRCC_SCE_CFGB);
#endif

	
	register_bank(iobase, 0);
	outb(0x00, iobase + IRCC_LCR_B);

	
	register_bank(iobase, 1);
	outb(inb(iobase + IRCC_SCE_CFGB) & ~IRCC_CFGB_DMA_ENABLE,
	     iobase + IRCC_SCE_CFGB);

	self->io.direction = IO_RECV;
	self->rx_buff.data = self->rx_buff.head;

	
	register_bank(iobase, 4);
	outb((2050 >> 8) & 0x0f, iobase + IRCC_RX_SIZE_HI);
	outb(2050 & 0xff, iobase + IRCC_RX_SIZE_LO);

	
	irda_setup_dma(self->io.dma, self->rx_buff_dma, self->rx_buff.truesize,
		       DMA_RX_MODE);

	
	register_bank(iobase, 1);
	outb(inb(iobase + IRCC_SCE_CFGB) | IRCC_CFGB_DMA_ENABLE |
	     IRCC_CFGB_DMA_BURST, iobase + IRCC_SCE_CFGB);

	
	register_bank(iobase, 0);
	outb(IRCC_IER_ACTIVE_FRAME | IRCC_IER_EOM, iobase + IRCC_IER);
	outb(IRCC_MASTER_INT_EN, iobase + IRCC_MASTER);

	
	register_bank(iobase, 0);
	outb(IRCC_LCR_B_SCE_RECEIVE | IRCC_LCR_B_SIP_ENABLE,
	     iobase + IRCC_LCR_B);

	return 0;
}

static void smsc_ircc_dma_receive_complete(struct smsc_ircc_cb *self)
{
	struct sk_buff *skb;
	int len, msgcnt, lsr;
	int iobase = self->io.fir_base;

	register_bank(iobase, 0);

	IRDA_DEBUG(3, "%s\n", __func__);
#if 0
	
	register_bank(iobase, 0);
	outb(0x00, iobase + IRCC_LCR_B);
#endif
	register_bank(iobase, 0);
	outb(inb(iobase + IRCC_LSAR) & ~IRCC_LSAR_ADDRESS_MASK, iobase + IRCC_LSAR);
	lsr= inb(iobase + IRCC_LSR);
	msgcnt = inb(iobase + IRCC_LCR_B) & 0x08;

	IRDA_DEBUG(2, "%s: dma count = %d\n", __func__,
		   get_dma_residue(self->io.dma));

	len = self->rx_buff.truesize - get_dma_residue(self->io.dma);

	
	if (lsr & (IRCC_LSR_FRAME_ERROR | IRCC_LSR_CRC_ERROR | IRCC_LSR_SIZE_ERROR)) {
		self->netdev->stats.rx_errors++;
		if (lsr & IRCC_LSR_FRAME_ERROR)
			self->netdev->stats.rx_frame_errors++;
		if (lsr & IRCC_LSR_CRC_ERROR)
			self->netdev->stats.rx_crc_errors++;
		if (lsr & IRCC_LSR_SIZE_ERROR)
			self->netdev->stats.rx_length_errors++;
		if (lsr & (IRCC_LSR_UNDERRUN | IRCC_LSR_OVERRUN))
			self->netdev->stats.rx_length_errors++;
		return;
	}

	
	len -= self->io.speed < 4000000 ? 2 : 4;

	if (len < 2 || len > 2050) {
		IRDA_WARNING("%s(), bogus len=%d\n", __func__, len);
		return;
	}
	IRDA_DEBUG(2, "%s: msgcnt = %d, len=%d\n", __func__, msgcnt, len);

	skb = dev_alloc_skb(len + 1);
	if (!skb) {
		IRDA_WARNING("%s(), memory squeeze, dropping frame.\n",
			     __func__);
		return;
	}
	
	skb_reserve(skb, 1);

	memcpy(skb_put(skb, len), self->rx_buff.data, len);
	self->netdev->stats.rx_packets++;
	self->netdev->stats.rx_bytes += len;

	skb->dev = self->netdev;
	skb_reset_mac_header(skb);
	skb->protocol = htons(ETH_P_IRDA);
	netif_rx(skb);
}

static void smsc_ircc_sir_receive(struct smsc_ircc_cb *self)
{
	int boguscount = 0;
	int iobase;

	IRDA_ASSERT(self != NULL, return;);

	iobase = self->io.sir_base;

	do {
		async_unwrap_char(self->netdev, &self->netdev->stats, &self->rx_buff,
				  inb(iobase + UART_RX));

		
		if (boguscount++ > 32) {
			IRDA_DEBUG(2, "%s(), breaking!\n", __func__);
			break;
		}
	} while (inb(iobase + UART_LSR) & UART_LSR_DR);
}


static irqreturn_t smsc_ircc_interrupt(int dummy, void *dev_id)
{
	struct net_device *dev = dev_id;
	struct smsc_ircc_cb *self = netdev_priv(dev);
	int iobase, iir, lcra, lsr;
	irqreturn_t ret = IRQ_NONE;

	
	spin_lock(&self->lock);

	
	if (self->io.speed <= SMSC_IRCC2_MAX_SIR_SPEED) {
		ret = smsc_ircc_interrupt_sir(dev);
		goto irq_ret_unlock;
	}

	iobase = self->io.fir_base;

	register_bank(iobase, 0);
	iir = inb(iobase + IRCC_IIR);
	if (iir == 0)
		goto irq_ret_unlock;
	ret = IRQ_HANDLED;

	
	outb(0, iobase + IRCC_IER);
	lcra = inb(iobase + IRCC_LCR_A);
	lsr = inb(iobase + IRCC_LSR);

	IRDA_DEBUG(2, "%s(), iir = 0x%02x\n", __func__, iir);

	if (iir & IRCC_IIR_EOM) {
		if (self->io.direction == IO_RECV)
			smsc_ircc_dma_receive_complete(self);
		else
			smsc_ircc_dma_xmit_complete(self);

		smsc_ircc_dma_receive(self);
	}

	if (iir & IRCC_IIR_ACTIVE_FRAME) {
		
	}

	

	register_bank(iobase, 0);
	outb(IRCC_IER_ACTIVE_FRAME | IRCC_IER_EOM, iobase + IRCC_IER);

 irq_ret_unlock:
	spin_unlock(&self->lock);

	return ret;
}

static irqreturn_t smsc_ircc_interrupt_sir(struct net_device *dev)
{
	struct smsc_ircc_cb *self = netdev_priv(dev);
	int boguscount = 0;
	int iobase;
	int iir, lsr;

	
	

	iobase = self->io.sir_base;

	iir = inb(iobase + UART_IIR) & UART_IIR_ID;
	if (iir == 0)
		return IRQ_NONE;
	while (iir) {
		
		lsr = inb(iobase + UART_LSR);

		IRDA_DEBUG(4, "%s(), iir=%02x, lsr=%02x, iobase=%#x\n",
			    __func__, iir, lsr, iobase);

		switch (iir) {
		case UART_IIR_RLSI:
			IRDA_DEBUG(2, "%s(), RLSI\n", __func__);
			break;
		case UART_IIR_RDI:
			
			smsc_ircc_sir_receive(self);
			break;
		case UART_IIR_THRI:
			if (lsr & UART_LSR_THRE)
				
				smsc_ircc_sir_write_wakeup(self);
			break;
		default:
			IRDA_DEBUG(0, "%s(), unhandled IIR=%#x\n",
				   __func__, iir);
			break;
		}

		
		if (boguscount++ > 100)
			break;

	        iir = inb(iobase + UART_IIR) & UART_IIR_ID;
	}
	
	return IRQ_HANDLED;
}


#if 0 
static int ircc_is_receiving(struct smsc_ircc_cb *self)
{
	int status = FALSE;
	

	IRDA_DEBUG(1, "%s\n", __func__);

	IRDA_ASSERT(self != NULL, return FALSE;);

	IRDA_DEBUG(0, "%s: dma count = %d\n", __func__,
		   get_dma_residue(self->io.dma));

	status = (self->rx_buff.state != OUTSIDE_FRAME);

	return status;
}
#endif 

static int smsc_ircc_request_irq(struct smsc_ircc_cb *self)
{
	int error;

	error = request_irq(self->io.irq, smsc_ircc_interrupt, 0,
			    self->netdev->name, self->netdev);
	if (error)
		IRDA_DEBUG(0, "%s(), unable to allocate irq=%d, err=%d\n",
			   __func__, self->io.irq, error);

	return error;
}

static void smsc_ircc_start_interrupts(struct smsc_ircc_cb *self)
{
	unsigned long flags;

	spin_lock_irqsave(&self->lock, flags);

	self->io.speed = 0;
	smsc_ircc_change_speed(self, SMSC_IRCC2_C_IRDA_FALLBACK_SPEED);

	spin_unlock_irqrestore(&self->lock, flags);
}

static void smsc_ircc_stop_interrupts(struct smsc_ircc_cb *self)
{
	int iobase = self->io.fir_base;
	unsigned long flags;

	spin_lock_irqsave(&self->lock, flags);

	register_bank(iobase, 0);
	outb(0, iobase + IRCC_IER);
	outb(IRCC_MASTER_RESET, iobase + IRCC_MASTER);
	outb(0x00, iobase + IRCC_MASTER);

	spin_unlock_irqrestore(&self->lock, flags);
}


static int smsc_ircc_net_open(struct net_device *dev)
{
	struct smsc_ircc_cb *self;
	char hwname[16];

	IRDA_DEBUG(1, "%s\n", __func__);

	IRDA_ASSERT(dev != NULL, return -1;);
	self = netdev_priv(dev);
	IRDA_ASSERT(self != NULL, return 0;);

	if (self->io.suspended) {
		IRDA_DEBUG(0, "%s(), device is suspended\n", __func__);
		return -EAGAIN;
	}

	if (request_irq(self->io.irq, smsc_ircc_interrupt, 0, dev->name,
			(void *) dev)) {
		IRDA_DEBUG(0, "%s(), unable to allocate irq=%d\n",
			   __func__, self->io.irq);
		return -EAGAIN;
	}

	smsc_ircc_start_interrupts(self);

	
	
	sprintf(hwname, "SMSC @ 0x%03x", self->io.fir_base);

	self->irlap = irlap_open(dev, &self->qos, hwname);

	if (request_dma(self->io.dma, dev->name)) {
		smsc_ircc_net_close(dev);

		IRDA_WARNING("%s(), unable to allocate DMA=%d\n",
			     __func__, self->io.dma);
		return -EAGAIN;
	}

	netif_start_queue(dev);

	return 0;
}

static int smsc_ircc_net_close(struct net_device *dev)
{
	struct smsc_ircc_cb *self;

	IRDA_DEBUG(1, "%s\n", __func__);

	IRDA_ASSERT(dev != NULL, return -1;);
	self = netdev_priv(dev);
	IRDA_ASSERT(self != NULL, return 0;);

	
	netif_stop_queue(dev);

	
	if (self->irlap)
		irlap_close(self->irlap);
	self->irlap = NULL;

	smsc_ircc_stop_interrupts(self);

	
	if (!self->io.suspended)
		free_irq(self->io.irq, dev);

	disable_dma(self->io.dma);
	free_dma(self->io.dma);

	return 0;
}

static int smsc_ircc_suspend(struct platform_device *dev, pm_message_t state)
{
	struct smsc_ircc_cb *self = platform_get_drvdata(dev);

	if (!self->io.suspended) {
		IRDA_DEBUG(1, "%s, Suspending\n", driver_name);

		rtnl_lock();
		if (netif_running(self->netdev)) {
			netif_device_detach(self->netdev);
			smsc_ircc_stop_interrupts(self);
			free_irq(self->io.irq, self->netdev);
			disable_dma(self->io.dma);
		}
		self->io.suspended = 1;
		rtnl_unlock();
	}

	return 0;
}

static int smsc_ircc_resume(struct platform_device *dev)
{
	struct smsc_ircc_cb *self = platform_get_drvdata(dev);

	if (self->io.suspended) {
		IRDA_DEBUG(1, "%s, Waking up\n", driver_name);

		rtnl_lock();
		smsc_ircc_init_chip(self);
		if (netif_running(self->netdev)) {
			if (smsc_ircc_request_irq(self)) {
				unregister_netdevice(self->netdev);
			} else {
				enable_dma(self->io.dma);
				smsc_ircc_start_interrupts(self);
				netif_device_attach(self->netdev);
			}
		}
		self->io.suspended = 0;
		rtnl_unlock();
	}
	return 0;
}

static int __exit smsc_ircc_close(struct smsc_ircc_cb *self)
{
	IRDA_DEBUG(1, "%s\n", __func__);

	IRDA_ASSERT(self != NULL, return -1;);

	platform_device_unregister(self->pldev);

	
	unregister_netdev(self->netdev);

	smsc_ircc_stop_interrupts(self);

	
	IRDA_DEBUG(0, "%s(), releasing 0x%03x\n",  __func__,
		   self->io.fir_base);

	release_region(self->io.fir_base, self->io.fir_ext);

	IRDA_DEBUG(0, "%s(), releasing 0x%03x\n", __func__,
		   self->io.sir_base);

	release_region(self->io.sir_base, self->io.sir_ext);

	if (self->tx_buff.head)
		dma_free_coherent(NULL, self->tx_buff.truesize,
				  self->tx_buff.head, self->tx_buff_dma);

	if (self->rx_buff.head)
		dma_free_coherent(NULL, self->rx_buff.truesize,
				  self->rx_buff.head, self->rx_buff_dma);

	free_netdev(self->netdev);

	return 0;
}

static void __exit smsc_ircc_cleanup(void)
{
	int i;

	IRDA_DEBUG(1, "%s\n", __func__);

	for (i = 0; i < 2; i++) {
		if (dev_self[i])
			smsc_ircc_close(dev_self[i]);
	}

	if (pnp_driver_registered)
		pnp_unregister_driver(&smsc_ircc_pnp_driver);

	platform_driver_unregister(&smsc_ircc_driver);
}

static void smsc_ircc_sir_start(struct smsc_ircc_cb *self)
{
	struct net_device *dev;
	int fir_base, sir_base;

	IRDA_DEBUG(3, "%s\n", __func__);

	IRDA_ASSERT(self != NULL, return;);
	dev = self->netdev;
	IRDA_ASSERT(dev != NULL, return;);

	fir_base = self->io.fir_base;
	sir_base = self->io.sir_base;

	
	outb(IRCC_MASTER_RESET, fir_base + IRCC_MASTER);

	#if SMSC_IRCC2_C_SIR_STOP
	
	#endif

	register_bank(fir_base, 1);
	outb(((inb(fir_base + IRCC_SCE_CFGA) & IRCC_SCE_CFGA_BLOCK_CTRL_BITS_MASK) | IRCC_CFGA_IRDA_SIR_A), fir_base + IRCC_SCE_CFGA);

	
	outb(UART_LCR_WLEN8, sir_base + UART_LCR);  
	outb((UART_MCR_DTR | UART_MCR_RTS | UART_MCR_OUT2), sir_base + UART_MCR);

	
	outb(UART_IER_RLSI | UART_IER_RDI |UART_IER_THRI, sir_base + UART_IER);

	IRDA_DEBUG(3, "%s() - exit\n", __func__);

	outb(0x00, fir_base + IRCC_MASTER);
}

#if SMSC_IRCC2_C_SIR_STOP
void smsc_ircc_sir_stop(struct smsc_ircc_cb *self)
{
	int iobase;

	IRDA_DEBUG(3, "%s\n", __func__);
	iobase = self->io.sir_base;

	
	outb(0, iobase + UART_MCR);

	
	outb(0, iobase + UART_IER);
}
#endif

static void smsc_ircc_sir_write_wakeup(struct smsc_ircc_cb *self)
{
	int actual = 0;
	int iobase;
	int fcr;

	IRDA_ASSERT(self != NULL, return;);

	IRDA_DEBUG(4, "%s\n", __func__);

	iobase = self->io.sir_base;

	
	if (self->tx_buff.len > 0)  {
		
		actual = smsc_ircc_sir_write(iobase, self->io.fifo_size,
				      self->tx_buff.data, self->tx_buff.len);
		self->tx_buff.data += actual;
		self->tx_buff.len  -= actual;
	} else {

	

		if (self->new_speed) {
			IRDA_DEBUG(5, "%s(), Changing speed to %d.\n",
				   __func__, self->new_speed);
			smsc_ircc_sir_wait_hw_transmitter_finish(self);
			smsc_ircc_change_speed(self, self->new_speed);
			self->new_speed = 0;
		} else {
			
			netif_wake_queue(self->netdev);
		}
		self->netdev->stats.tx_packets++;

		if (self->io.speed <= 115200) {
			fcr = UART_FCR_ENABLE_FIFO | UART_FCR_CLEAR_RCVR;
			fcr |= self->io.speed < 38400 ?
					UART_FCR_TRIGGER_1 : UART_FCR_TRIGGER_14;

			outb(fcr, iobase + UART_FCR);

			
			outb(UART_IER_RDI, iobase + UART_IER);
		}
	}
}

static int smsc_ircc_sir_write(int iobase, int fifo_size, __u8 *buf, int len)
{
	int actual = 0;

	
	if (!(inb(iobase + UART_LSR) & UART_LSR_THRE)) {
		IRDA_WARNING("%s(), failed, fifo not empty!\n", __func__);
		return 0;
	}

	
	while (fifo_size-- > 0 && actual < len) {
		
		outb(buf[actual], iobase + UART_TX);
		actual++;
	}
	return actual;
}

static int smsc_ircc_is_receiving(struct smsc_ircc_cb *self)
{
	return self->rx_buff.state != OUTSIDE_FRAME;
}


static void smsc_ircc_probe_transceiver(struct smsc_ircc_cb *self)
{
	unsigned int	i;

	IRDA_ASSERT(self != NULL, return;);

	for (i = 0; smsc_transceivers[i].name != NULL; i++)
		if (smsc_transceivers[i].probe(self->io.fir_base)) {
			IRDA_MESSAGE(" %s transceiver found\n",
				     smsc_transceivers[i].name);
			self->transceiver= i + 1;
			return;
		}

	IRDA_MESSAGE("No transceiver found. Defaulting to %s\n",
		     smsc_transceivers[SMSC_IRCC2_C_DEFAULT_TRANSCEIVER].name);

	self->transceiver = SMSC_IRCC2_C_DEFAULT_TRANSCEIVER;
}


static void smsc_ircc_set_transceiver_for_speed(struct smsc_ircc_cb *self, u32 speed)
{
	unsigned int trx;

	trx = self->transceiver;
	if (trx > 0)
		smsc_transceivers[trx - 1].set_for_speed(self->io.fir_base, speed);
}


static void smsc_ircc_sir_wait_hw_transmitter_finish(struct smsc_ircc_cb *self)
{
	int iobase = self->io.sir_base;
	int count = SMSC_IRCC2_HW_TRANSMITTER_TIMEOUT_US;

	
	while (count-- > 0 && !(inb(iobase + UART_LSR) & UART_LSR_TEMT))
		udelay(1);

	if (count < 0)
		IRDA_DEBUG(0, "%s(): stuck transmitter\n", __func__);
}



static int __init smsc_ircc_look_for_chips(void)
{
	struct smsc_chip_address *address;
	char *type;
	unsigned int cfg_base, found;

	found = 0;
	address = possible_addresses;

	while (address->cfg_base) {
		cfg_base = address->cfg_base;

		

		if (address->type & SMSCSIO_TYPE_FDC) {
			type = "FDC";
			if (address->type & SMSCSIO_TYPE_FLAT)
				if (!smsc_superio_flat(fdc_chips_flat, cfg_base, type))
					found++;

			if (address->type & SMSCSIO_TYPE_PAGED)
				if (!smsc_superio_paged(fdc_chips_paged, cfg_base, type))
					found++;
		}
		if (address->type & SMSCSIO_TYPE_LPC) {
			type = "LPC";
			if (address->type & SMSCSIO_TYPE_FLAT)
				if (!smsc_superio_flat(lpc_chips_flat, cfg_base, type))
					found++;

			if (address->type & SMSCSIO_TYPE_PAGED)
				if (!smsc_superio_paged(lpc_chips_paged, cfg_base, type))
					found++;
		}
		address++;
	}
	return found;
}

static int __init smsc_superio_flat(const struct smsc_chip *chips, unsigned short cfgbase, char *type)
{
	unsigned short firbase, sirbase;
	u8 mode, dma, irq;
	int ret = -ENODEV;

	IRDA_DEBUG(1, "%s\n", __func__);

	if (smsc_ircc_probe(cfgbase, SMSCSIOFLAT_DEVICEID_REG, chips, type) == NULL)
		return ret;

	outb(SMSCSIOFLAT_UARTMODE0C_REG, cfgbase);
	mode = inb(cfgbase + 1);

	

	if (!(mode & SMSCSIOFLAT_UART2MODE_VAL_IRDA))
		IRDA_WARNING("%s(): IrDA not enabled\n", __func__);

	outb(SMSCSIOFLAT_UART2BASEADDR_REG, cfgbase);
	sirbase = inb(cfgbase + 1) << 2;

	
	outb(SMSCSIOFLAT_FIRBASEADDR_REG, cfgbase);
	firbase = inb(cfgbase + 1) << 3;

	
	outb(SMSCSIOFLAT_FIRDMASELECT_REG, cfgbase);
	dma = inb(cfgbase + 1) & SMSCSIOFLAT_FIRDMASELECT_MASK;

	
	outb(SMSCSIOFLAT_UARTIRQSELECT_REG, cfgbase);
	irq = inb(cfgbase + 1) & SMSCSIOFLAT_UART2IRQSELECT_MASK;

	IRDA_MESSAGE("%s(): fir: 0x%02x, sir: 0x%02x, dma: %02d, irq: %d, mode: 0x%02x\n", __func__, firbase, sirbase, dma, irq, mode);

	if (firbase && smsc_ircc_open(firbase, sirbase, dma, irq) == 0)
		ret = 0;

	
	outb(SMSCSIO_CFGEXITKEY, cfgbase);

	return ret;
}

static int __init smsc_superio_paged(const struct smsc_chip *chips, unsigned short cfg_base, char *type)
{
	unsigned short fir_io, sir_io;
	int ret = -ENODEV;

	IRDA_DEBUG(1, "%s\n", __func__);

	if (smsc_ircc_probe(cfg_base, 0x20, chips, type) == NULL)
		return ret;

	
	outb(0x07, cfg_base);
	outb(0x05, cfg_base + 1);

	
	outb(0x60, cfg_base);
	sir_io = inb(cfg_base + 1) << 8;
	outb(0x61, cfg_base);
	sir_io |= inb(cfg_base + 1);

	
	outb(0x62, cfg_base);
	fir_io = inb(cfg_base + 1) << 8;
	outb(0x63, cfg_base);
	fir_io |= inb(cfg_base + 1);
	outb(0x2b, cfg_base); 

	if (fir_io && smsc_ircc_open(fir_io, sir_io, ircc_dma, ircc_irq) == 0)
		ret = 0;

	
	outb(SMSCSIO_CFGEXITKEY, cfg_base);

	return ret;
}


static int __init smsc_access(unsigned short cfg_base, unsigned char reg)
{
	IRDA_DEBUG(1, "%s\n", __func__);

	outb(reg, cfg_base);
	return inb(cfg_base) != reg ? -1 : 0;
}

static const struct smsc_chip * __init smsc_ircc_probe(unsigned short cfg_base, u8 reg, const struct smsc_chip *chip, char *type)
{
	u8 devid, xdevid, rev;

	IRDA_DEBUG(1, "%s\n", __func__);

	

	outb(SMSCSIO_CFGEXITKEY, cfg_base);

	if (inb(cfg_base) == SMSCSIO_CFGEXITKEY)	
		return NULL;

	outb(reg, cfg_base);

	xdevid = inb(cfg_base + 1);

	

	outb(SMSCSIO_CFGACCESSKEY, cfg_base);

	#if 0
	if (smsc_access(cfg_base,0x55))	
		return NULL;
	#endif

	

	if (smsc_access(cfg_base, reg))
		return NULL;

	devid = inb(cfg_base + 1);

	if (devid == 0 || devid == 0xff)	
		return NULL;

	

	if (smsc_access(cfg_base, reg + 1))
		return NULL;

	rev = inb(cfg_base + 1);

	if (rev >= 128)			
		return NULL;

	if (devid == xdevid)		
		return NULL;

	

	while (chip->devid != devid) {

		chip++;

		if (chip->name == NULL)
			return NULL;
	}

	IRDA_MESSAGE("found SMC SuperIO Chip (devid=0x%02x rev=%02X base=0x%04x): %s%s\n",
		     devid, rev, cfg_base, type, chip->name);

	if (chip->rev > rev) {
		IRDA_MESSAGE("Revision higher than expected\n");
		return NULL;
	}

	if (chip->flags & NoIRDA)
		IRDA_MESSAGE("chipset does not support IRDA\n");

	return chip;
}

static int __init smsc_superio_fdc(unsigned short cfg_base)
{
	int ret = -1;

	if (!request_region(cfg_base, 2, driver_name)) {
		IRDA_WARNING("%s: can't get cfg_base of 0x%03x\n",
			     __func__, cfg_base);
	} else {
		if (!smsc_superio_flat(fdc_chips_flat, cfg_base, "FDC") ||
		    !smsc_superio_paged(fdc_chips_paged, cfg_base, "FDC"))
			ret =  0;

		release_region(cfg_base, 2);
	}

	return ret;
}

static int __init smsc_superio_lpc(unsigned short cfg_base)
{
	int ret = -1;

	if (!request_region(cfg_base, 2, driver_name)) {
		IRDA_WARNING("%s: can't get cfg_base of 0x%03x\n",
			     __func__, cfg_base);
	} else {
		if (!smsc_superio_flat(lpc_chips_flat, cfg_base, "LPC") ||
		    !smsc_superio_paged(lpc_chips_paged, cfg_base, "LPC"))
			ret = 0;

		release_region(cfg_base, 2);
	}
	return ret;
}

#ifdef CONFIG_PCI
static struct smsc_ircc_subsystem_configuration subsystem_configurations[] __initdata = {
	{
		
		.vendor = PCI_VENDOR_ID_INTEL, 
		.device = 0x24cc,
		.subvendor = 0x103c,
		.subdevice = 0x08bc,
		.sir_io = 0x02f8,
		.fir_io = 0x0130,
		.fir_irq = 0x05,
		.fir_dma = 0x03,
		.cfg_base = 0x004e,
		.preconfigure = preconfigure_through_82801,
		.name = "HP nx5000 family",
	},
	{
		.vendor = PCI_VENDOR_ID_INTEL, 
		.device = 0x24cc,
		.subvendor = 0x103c,
		.subdevice = 0x088c,
		
		.sir_io = 0x02f8,
		.fir_io = 0x0130,
		.fir_irq = 0x05,
		.fir_dma = 0x03,
		.cfg_base = 0x004e,
		.preconfigure = preconfigure_through_82801,
		.name = "HP nc8000 family",
	},
	{
		.vendor = PCI_VENDOR_ID_INTEL, 
		.device = 0x24cc,
		.subvendor = 0x103c,
		.subdevice = 0x0890,
		.sir_io = 0x02f8,
		.fir_io = 0x0130,
		.fir_irq = 0x05,
		.fir_dma = 0x03,
		.cfg_base = 0x004e,
		.preconfigure = preconfigure_through_82801,
		.name = "HP nc6000 family",
	},
	{
		.vendor = PCI_VENDOR_ID_INTEL, 
		.device = 0x24cc,
		.subvendor = 0x0e11,
		.subdevice = 0x0860,
		
		.sir_io = 0x02e8,
		.fir_io = 0x02f8,
		.fir_irq = 0x07,
		.fir_dma = 0x03,
		.cfg_base = 0x002e,
		.preconfigure = preconfigure_through_82801,
		.name = "Compaq x1000 family",
	},
	{
		
		.vendor = PCI_VENDOR_ID_INTEL,
		.device = 0x24c0,
		.subvendor = 0x1179,
		.subdevice = 0xffff, 
		.sir_io = 0x03f8,
		.fir_io = 0x0130,
		.fir_irq = 0x07,
		.fir_dma = 0x01,
		.cfg_base = 0x002e,
		.preconfigure = preconfigure_through_82801,
		.name = "Toshiba laptop with Intel 82801DB/DBL LPC bridge",
	},
	{
		.vendor = PCI_VENDOR_ID_INTEL, 
		.device = 0x248c,
		.subvendor = 0x1179,
		.subdevice = 0xffff, 
		.sir_io = 0x03f8,
		.fir_io = 0x0130,
		.fir_irq = 0x03,
		.fir_dma = 0x03,
		.cfg_base = 0x002e,
		.preconfigure = preconfigure_through_82801,
		.name = "Toshiba laptop with Intel 82801CAM ISA bridge",
	},
	{
		
		.vendor = PCI_VENDOR_ID_INTEL,
		.device = 0x24cc,
		.subvendor = 0x1179,
		.subdevice = 0xffff, 
		.sir_io = 0x03f8,
		.fir_io = 0x0130,
		.fir_irq = 0x03,
		.fir_dma = 0x03,
		.cfg_base = 0x002e,
		.preconfigure = preconfigure_through_82801,
		.name = "Toshiba laptop with Intel 8281DBM LPC bridge",
	},
	{
		
		.vendor = PCI_VENDOR_ID_AL,
		.device = 0x1533,
		.subvendor = 0x1179,
		.subdevice = 0xffff, 
		.sir_io = 0x02e8,
		.fir_io = 0x02f8,
		.fir_irq = 0x07,
		.fir_dma = 0x03,
		.cfg_base = 0x002e,
		.preconfigure = preconfigure_through_ali,
		.name = "Toshiba laptop with ALi ISA bridge",
	},
	{ } 
};


static int __init preconfigure_smsc_chip(struct
					 smsc_ircc_subsystem_configuration
					 *conf)
{
	unsigned short iobase = conf->cfg_base;
	unsigned char tmpbyte;

	outb(LPC47N227_CFGACCESSKEY, iobase); 
	outb(SMSCSIOFLAT_DEVICEID_REG, iobase); 
	tmpbyte = inb(iobase +1); 
	IRDA_DEBUG(0,
		   "Detected Chip id: 0x%02x, setting up registers...\n",
		   tmpbyte);

	
	outb(0x24, iobase);  
	outb(0x00, iobase + 1); 
	outb(SMSCSIOFLAT_UART2BASEADDR_REG, iobase);  
	outb( (conf->sir_io >> 2), iobase + 1); 
	tmpbyte = inb(iobase + 1);
	if (tmpbyte != (conf->sir_io >> 2) ) {
		IRDA_WARNING("ERROR: could not configure SIR ioport.\n");
		IRDA_WARNING("Try to supply ircc_cfg argument.\n");
		return -ENXIO;
	}

	
	outb(SMSCSIOFLAT_UARTIRQSELECT_REG, iobase); 
	tmpbyte = inb(iobase + 1);
	tmpbyte &= SMSCSIOFLAT_UART1IRQSELECT_MASK; 
	tmpbyte |= (conf->fir_irq & SMSCSIOFLAT_UART2IRQSELECT_MASK);
	outb(tmpbyte, iobase + 1);
	tmpbyte = inb(iobase + 1) & SMSCSIOFLAT_UART2IRQSELECT_MASK;
	if (tmpbyte != conf->fir_irq) {
		IRDA_WARNING("ERROR: could not configure FIR IRQ channel.\n");
		return -ENXIO;
	}

	
	outb(SMSCSIOFLAT_FIRBASEADDR_REG, iobase);  
	outb((conf->fir_io >> 3), iobase + 1);
	tmpbyte = inb(iobase + 1);
	if (tmpbyte != (conf->fir_io >> 3) ) {
		IRDA_WARNING("ERROR: could not configure FIR I/O port.\n");
		return -ENXIO;
	}

	
	outb(SMSCSIOFLAT_FIRDMASELECT_REG, iobase);  
	outb((conf->fir_dma & LPC47N227_FIRDMASELECT_MASK), iobase + 1); 
	tmpbyte = inb(iobase + 1) & LPC47N227_FIRDMASELECT_MASK;
	if (tmpbyte != (conf->fir_dma & LPC47N227_FIRDMASELECT_MASK)) {
		IRDA_WARNING("ERROR: could not configure FIR DMA channel.\n");
		return -ENXIO;
	}

	outb(SMSCSIOFLAT_UARTMODE0C_REG, iobase);  
	tmpbyte = inb(iobase + 1);
	tmpbyte &= ~SMSCSIOFLAT_UART2MODE_MASK |
		SMSCSIOFLAT_UART2MODE_VAL_IRDA;
	outb(tmpbyte, iobase + 1); 

	outb(LPC47N227_APMBOOTDRIVE_REG, iobase);  
	tmpbyte = inb(iobase + 1);
	outb(tmpbyte | LPC47N227_UART2AUTOPWRDOWN_MASK, iobase + 1); 

	
	outb(0x0a, iobase);  
	tmpbyte = inb(iobase + 1);
	outb(tmpbyte | 0x40, iobase + 1); 

	outb(LPC47N227_UART12POWER_REG, iobase);  
	tmpbyte = inb(iobase + 1);
	outb(tmpbyte | LPC47N227_UART2POWERDOWN_MASK, iobase + 1); 

	outb(LPC47N227_FDCPOWERVALIDCONF_REG, iobase);  
	tmpbyte = inb(iobase + 1);
	outb(tmpbyte | LPC47N227_VALID_MASK, iobase + 1); 

	outb(LPC47N227_CFGEXITKEY, iobase);  

	return 0;
}

#define VID 0x00
#define DID 0x02
#define PIRQ_A_D_ROUT 0x60
#define SIRQ_CNTL 0x64
#define PIRQ_E_H_ROUT 0x68
#define PCI_DMA_C 0x90
#define COM_DEC 0xe0
#define GEN1_DEC 0xe4
#define LPC_EN 0xe6
#define GEN2_DEC 0xec
static int __init preconfigure_through_82801(struct pci_dev *dev,
					     struct
					     smsc_ircc_subsystem_configuration
					     *conf)
{
	unsigned short tmpword;
	unsigned char tmpbyte;

	IRDA_MESSAGE("Setting up Intel 82801 controller and SMSC device\n");
	pci_read_config_byte(dev, COM_DEC, &tmpbyte);
	tmpbyte &= 0xf8; 
	switch(conf->sir_io) {
	case 0x3f8:
		tmpbyte |= 0x00;
		break;
	case 0x2f8:
		tmpbyte |= 0x01;
		break;
	case 0x220:
		tmpbyte |= 0x02;
		break;
	case 0x228:
		tmpbyte |= 0x03;
		break;
	case 0x238:
		tmpbyte |= 0x04;
		break;
	case 0x2e8:
		tmpbyte |= 0x05;
		break;
	case 0x338:
		tmpbyte |= 0x06;
		break;
	case 0x3e8:
		tmpbyte |= 0x07;
		break;
	default:
		tmpbyte |= 0x01; 
	}
	IRDA_DEBUG(1, "COM_DEC (write): 0x%02x\n", tmpbyte);
	pci_write_config_byte(dev, COM_DEC, tmpbyte);

	
	pci_read_config_word(dev, LPC_EN, &tmpword);
	switch(conf->cfg_base) {
	case 0x04e:
		tmpword |= 0x2000;
		break;
	case 0x02e:
		tmpword |= 0x1000;
		break;
	case 0x062:
		tmpword |= 0x0800;
		break;
	case 0x060:
		tmpword |= 0x0400;
		break;
	default:
		IRDA_WARNING("Uncommon I/O base address: 0x%04x\n",
			     conf->cfg_base);
		break;
	}
	tmpword &= 0xfffd; 
	tmpword |= 0x0001; 
	IRDA_DEBUG(1, "LPC_EN (write): 0x%04x\n", tmpword);
	pci_write_config_word(dev, LPC_EN, tmpword);

	pci_read_config_word(dev, PCI_DMA_C, &tmpword);
	switch(conf->fir_dma) {
	case 0x07:
		tmpword |= 0xc000;
		break;
	case 0x06:
		tmpword |= 0x3000;
		break;
	case 0x05:
		tmpword |= 0x0c00;
		break;
	case 0x03:
		tmpword |= 0x00c0;
		break;
	case 0x02:
		tmpword |= 0x0030;
		break;
	case 0x01:
		tmpword |= 0x000c;
		break;
	case 0x00:
		tmpword |= 0x0003;
		break;
	default:
		break; 
	}
	IRDA_DEBUG(1, "PCI_DMA_C (write): 0x%04x\n", tmpword);
	pci_write_config_word(dev, PCI_DMA_C, tmpword);

	tmpword = conf->fir_io & 0xfff8;
	tmpword |= 0x0001;
	IRDA_DEBUG(1, "GEN2_DEC (write): 0x%04x\n", tmpword);
	pci_write_config_word(dev, GEN2_DEC, tmpword);

	
	return preconfigure_smsc_chip(conf);
}

static void __init preconfigure_ali_port(struct pci_dev *dev,
					 unsigned short port)
{
	unsigned char reg;
	
	unsigned char mask;
	unsigned char tmpbyte;

	switch(port) {
	case 0x0130:
	case 0x0178:
		reg = 0xb0;
		mask = 0x80;
		break;
	case 0x03f8:
		reg = 0xb4;
		mask = 0x80;
		break;
	case 0x02f8:
		reg = 0xb4;
		mask = 0x30;
		break;
	case 0x02e8:
		reg = 0xb4;
		mask = 0x08;
		break;
	default:
		IRDA_ERROR("Failed to configure unsupported port on ALi 1533 bridge: 0x%04x\n", port);
		return;
	}

	pci_read_config_byte(dev, reg, &tmpbyte);
	
	tmpbyte |= mask;
	pci_write_config_byte(dev, reg, tmpbyte);
	IRDA_MESSAGE("Activated ALi 1533 ISA bridge port 0x%04x.\n", port);
}

static int __init preconfigure_through_ali(struct pci_dev *dev,
					   struct
					   smsc_ircc_subsystem_configuration
					   *conf)
{
	
	preconfigure_ali_port(dev, conf->sir_io);
	preconfigure_ali_port(dev, conf->fir_io);

	
	return preconfigure_smsc_chip(conf);
}

static int __init smsc_ircc_preconfigure_subsystems(unsigned short ircc_cfg,
						    unsigned short ircc_fir,
						    unsigned short ircc_sir,
						    unsigned char ircc_dma,
						    unsigned char ircc_irq)
{
	struct pci_dev *dev = NULL;
	unsigned short ss_vendor = 0x0000;
	unsigned short ss_device = 0x0000;
	int ret = 0;

	for_each_pci_dev(dev) {
		struct smsc_ircc_subsystem_configuration *conf;

		if (dev->subsystem_vendor != 0x0000U) {
			ss_vendor = dev->subsystem_vendor;
			ss_device = dev->subsystem_device;
		}
		conf = subsystem_configurations;
		for( ; conf->subvendor; conf++) {
			if(conf->vendor == dev->vendor &&
			   conf->device == dev->device &&
			   conf->subvendor == ss_vendor &&
			   
			   (conf->subdevice == ss_device ||
			    conf->subdevice == 0xffff)) {
				struct smsc_ircc_subsystem_configuration
					tmpconf;

				memcpy(&tmpconf, conf,
				       sizeof(struct smsc_ircc_subsystem_configuration));

				if (ircc_cfg != 0)
					tmpconf.cfg_base = ircc_cfg;
				if (ircc_fir != 0)
					tmpconf.fir_io = ircc_fir;
				if (ircc_sir != 0)
					tmpconf.sir_io = ircc_sir;
				if (ircc_dma != DMA_INVAL)
					tmpconf.fir_dma = ircc_dma;
				if (ircc_irq != IRQ_INVAL)
					tmpconf.fir_irq = ircc_irq;

				IRDA_MESSAGE("Detected unconfigured %s SMSC IrDA chip, pre-configuring device.\n", conf->name);
				if (conf->preconfigure)
					ret = conf->preconfigure(dev, &tmpconf);
				else
					ret = -ENODEV;
			}
		}
	}

	return ret;
}
#endif 




static void smsc_ircc_set_transceiver_smsc_ircc_atc(int fir_base, u32 speed)
{
	unsigned long jiffies_now, jiffies_timeout;
	u8 val;

	jiffies_now = jiffies;
	jiffies_timeout = jiffies + SMSC_IRCC2_ATC_PROGRAMMING_TIMEOUT_JIFFIES;

	
	register_bank(fir_base, 4);
	outb((inb(fir_base + IRCC_ATC) & IRCC_ATC_MASK) | IRCC_ATC_nPROGREADY|IRCC_ATC_ENABLE,
	     fir_base + IRCC_ATC);

	while ((val = (inb(fir_base + IRCC_ATC) & IRCC_ATC_nPROGREADY)) &&
		!time_after(jiffies, jiffies_timeout))
		;

	if (val)
		IRDA_WARNING("%s(): ATC: 0x%02x\n", __func__,
			     inb(fir_base + IRCC_ATC));
}


static int smsc_ircc_probe_transceiver_smsc_ircc_atc(int fir_base)
{
	return 0;
}


static void smsc_ircc_set_transceiver_smsc_ircc_fast_pin_select(int fir_base, u32 speed)
{
	u8 fast_mode;

	switch (speed) {
	default:
	case 576000 :
		fast_mode = 0;
		break;
	case 1152000 :
	case 4000000 :
		fast_mode = IRCC_LCR_A_FAST;
		break;
	}
	register_bank(fir_base, 0);
	outb((inb(fir_base + IRCC_LCR_A) & 0xbf) | fast_mode, fir_base + IRCC_LCR_A);
}


static int smsc_ircc_probe_transceiver_smsc_ircc_fast_pin_select(int fir_base)
{
	return 0;
}


static void smsc_ircc_set_transceiver_toshiba_sat1800(int fir_base, u32 speed)
{
	u8 fast_mode;

	switch (speed) {
	default:
	case 576000 :
		fast_mode = 0;
		break;
	case 1152000 :
	case 4000000 :
		fast_mode =  IRCC_LCR_A_GP_DATA;
		break;

	}
	
	register_bank(fir_base, 0);
	outb((inb(fir_base + IRCC_LCR_A) &  0xbf) | fast_mode, fir_base + IRCC_LCR_A);
}


static int smsc_ircc_probe_transceiver_toshiba_sat1800(int fir_base)
{
	return 0;
}


module_init(smsc_ircc_init);
module_exit(smsc_ircc_cleanup);
