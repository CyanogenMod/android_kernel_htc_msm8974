/*
 *  madgemc.c: Driver for the Madge Smart 16/4 MC16 MCA token ring card.
 *
 *  Written 2000 by Adam Fritzler
 *
 *  This software may be used and distributed according to the terms
 *  of the GNU General Public License, incorporated herein by reference.
 *
 *  This driver module supports the following cards:
 *      - Madge Smart 16/4 Ringnode MC16
 *	- Madge Smart 16/4 Ringnode MC32 (??)
 *
 *  Maintainer(s):
 *    AF	Adam Fritzler
 *
 *  Modification History:
 *	16-Jan-00	AF	Created
 *
 */
static const char version[] = "madgemc.c: v0.91 23/01/2000 by Adam Fritzler\n";

#include <linux/module.h>
#include <linux/mca.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/trdevice.h>

#include <asm/io.h>
#include <asm/irq.h>

#include "tms380tr.h"
#include "madgemc.h"            

#define MADGEMC_IO_EXTENT 32
#define MADGEMC_SIF_OFFSET 0x08

struct card_info {
	unsigned int manid;
	unsigned int cardtype;
	unsigned int cardrev;
	unsigned int ramsize;
	
	unsigned int burstmode:2;
	unsigned int fairness:1; 
	unsigned int arblevel:4;
	unsigned int ringspeed:2; 
	unsigned int cabletype:1; 
};

static int madgemc_open(struct net_device *dev);
static int madgemc_close(struct net_device *dev);
static int madgemc_chipset_init(struct net_device *dev);
static void madgemc_read_rom(struct net_device *dev, struct card_info *card);
static unsigned short madgemc_setnselout_pins(struct net_device *dev);
static void madgemc_setcabletype(struct net_device *dev, int type);

static int madgemc_mcaproc(char *buf, int slot, void *d);

static void madgemc_setregpage(struct net_device *dev, int page);
static void madgemc_setsifsel(struct net_device *dev, int val);
static void madgemc_setint(struct net_device *dev, int val);

static irqreturn_t madgemc_interrupt(int irq, void *dev_id);

#define SIFREADB(reg) (inb(dev->base_addr + ((reg<0x8)?reg:reg-0x8)))
#define SIFWRITEB(val, reg) (outb(val, dev->base_addr + ((reg<0x8)?reg:reg-0x8)))
#define SIFREADW(reg) (inw(dev->base_addr + ((reg<0x8)?reg:reg-0x8)))
#define SIFWRITEW(val, reg) (outw(val, dev->base_addr + ((reg<0x8)?reg:reg-0x8)))

static unsigned short madgemc_sifreadb(struct net_device *dev, unsigned short reg)
{
	unsigned short ret;
	if (reg<0x8)	
		ret = SIFREADB(reg);
	else {
		madgemc_setregpage(dev, 1);	
		ret = SIFREADB(reg);
		madgemc_setregpage(dev, 0);
	}
	return ret;
}

static void madgemc_sifwriteb(struct net_device *dev, unsigned short val, unsigned short reg)
{
	if (reg<0x8)
		SIFWRITEB(val, reg);
	else {
		madgemc_setregpage(dev, 1);
		SIFWRITEB(val, reg);
		madgemc_setregpage(dev, 0);
	}
}

static unsigned short madgemc_sifreadw(struct net_device *dev, unsigned short reg)
{
	unsigned short ret;
	if (reg<0x8)	
		ret = SIFREADW(reg);
	else {
		madgemc_setregpage(dev, 1);	
		ret = SIFREADW(reg);
		madgemc_setregpage(dev, 0);
	}
	return ret;
}

static void madgemc_sifwritew(struct net_device *dev, unsigned short val, unsigned short reg)
{
	if (reg<0x8)
		SIFWRITEW(val, reg);
	else {
		madgemc_setregpage(dev, 1);
		SIFWRITEW(val, reg);
		madgemc_setregpage(dev, 0);
	}
}

static struct net_device_ops madgemc_netdev_ops __read_mostly;

static int __devinit madgemc_probe(struct device *device)
{	
	static int versionprinted;
	struct net_device *dev;
	struct net_local *tp;
	struct card_info *card;
	struct mca_device *mdev = to_mca_device(device);
	int ret = 0;

	if (versionprinted++ == 0)
		printk("%s", version);

	if(mca_device_claimed(mdev))
		return -EBUSY;
	mca_device_set_claim(mdev, 1);

	dev = alloc_trdev(sizeof(struct net_local));
	if (!dev) {
		printk("madgemc: unable to allocate dev space\n");
		mca_device_set_claim(mdev, 0);
		ret = -ENOMEM;
		goto getout;
	}

	dev->netdev_ops = &madgemc_netdev_ops;

	card = kmalloc(sizeof(struct card_info), GFP_KERNEL);
	if (card==NULL) {
		ret = -ENOMEM;
		goto getout1;
	}


	dev->base_addr = 0x0a20 + 
		((mdev->pos[2] & MC16_POS2_ADDR2)?0x0400:0) +
		((mdev->pos[0] & MC16_POS0_ADDR1)?0x1000:0) +
		((mdev->pos[3] & MC16_POS3_ADDR3)?0x2000:0);

	switch(mdev->pos[0] >> 6) { 
		case 0x1: dev->irq = 3; break;
		case 0x2: dev->irq = 9; break; 
		case 0x3: dev->irq = 10; break;
		default: dev->irq = 0; break;
	}

	if (dev->irq == 0) {
		printk("%s: invalid IRQ\n", dev->name);
		ret = -EBUSY;
		goto getout2;
	}

	if (!request_region(dev->base_addr, MADGEMC_IO_EXTENT, 
			   "madgemc")) {
		printk(KERN_INFO "madgemc: unable to setup Smart MC in slot %d because of I/O base conflict at 0x%04lx\n", mdev->slot, dev->base_addr);
		dev->base_addr += MADGEMC_SIF_OFFSET;
		ret = -EBUSY;
		goto getout2;
	}
	dev->base_addr += MADGEMC_SIF_OFFSET;
	
	card->arblevel = ((mdev->pos[0] >> 1) & 0x7) + 8;

	card->burstmode = ((mdev->pos[2] >> 6) & 0x3);
	card->fairness = ((mdev->pos[2] >> 4) & 0x1);

	if ((mdev->pos[1] >> 2)&0x1)
		card->ringspeed = 2; 
	else if ((mdev->pos[2] >> 5) & 0x1)
		card->ringspeed = 1; 
	else
		card->ringspeed = 0; 

	if ((mdev->pos[1] >> 6)&0x1)
		card->cabletype = 1; 
	else
		card->cabletype = 0; 


	madgemc_read_rom(dev, card);
		
	if (card->manid != 0x4d) { 
		printk(KERN_INFO "%s: Madge MC ROM read failed (unknown manufacturer ID %02x)\n", dev->name, card->manid);
		goto getout3;
	}
		
	if ((card->cardtype != 0x08) && (card->cardtype != 0x0d)) {
		printk(KERN_INFO "%s: Madge MC ROM read failed (unknown card ID %02x)\n", dev->name, card->cardtype);
		ret = -EIO;
		goto getout3;
	}
	       
	
	if ((card->cardtype == 0x08) && (card->cardrev <= 0x01))
		card->ramsize = 128;
	else
		card->ramsize = 256;

	printk("%s: %s Rev %d at 0x%04lx IRQ %d\n", 
	       dev->name, 
	       (card->cardtype == 0x08)?MADGEMC16_CARDNAME:
	       MADGEMC32_CARDNAME, card->cardrev, 
	       dev->base_addr, dev->irq);

	if (card->cardtype == 0x0d)
		printk("%s:     Warning: MC32 support is experimental and highly untested\n", dev->name);
	
	if (card->ringspeed==2) { 
		printk("%s:     Warning: Ring speed not set in POS -- Please run the reference disk and set it!\n", dev->name);
		card->ringspeed = 1; 
	}
		
	printk("%s:     RAM Size: %dKB\n", dev->name, card->ramsize);

	printk("%s:     Ring Speed: %dMb/sec on %s\n", dev->name, 
	       (card->ringspeed)?16:4, 
	       card->cabletype?"STP/DB9":"UTP/RJ-45");
	printk("%s:     Arbitration Level: %d\n", dev->name, 
	       card->arblevel);

	printk("%s:     Burst Mode: ", dev->name);
	switch(card->burstmode) {
		case 0: printk("Cycle steal"); break;
		case 1: printk("Limited burst"); break;
		case 2: printk("Delayed release"); break;
		case 3: printk("Immediate release"); break;
	}
	printk(" (%s)\n", (card->fairness)?"Unfair":"Fair");


 
	outb(0, dev->base_addr + MC_CONTROL_REG0); 
	madgemc_setsifsel(dev, 1);
	if (request_irq(dev->irq, madgemc_interrupt, IRQF_SHARED,
		       "madgemc", dev)) {
		ret = -EBUSY;
		goto getout3;
	}

	madgemc_chipset_init(dev); 
	madgemc_setcabletype(dev, card->cabletype);

	
	mca_device_set_name(mdev, (card->cardtype == 0x08)?MADGEMC16_CARDNAME:MADGEMC32_CARDNAME);
	mca_set_adapter_procfn(mdev->slot, madgemc_mcaproc, dev);

	printk("%s:     Ring Station Address: %pM\n",
	       dev->name, dev->dev_addr);

	if (tmsdev_init(dev, device)) {
		printk("%s: unable to get memory for dev->priv.\n", 
		       dev->name);
		ret = -ENOMEM;
		goto getout4;
	}
	tp = netdev_priv(dev);

	tp->setnselout = madgemc_setnselout_pins;
	tp->sifwriteb = madgemc_sifwriteb;
	tp->sifreadb = madgemc_sifreadb;
	tp->sifwritew = madgemc_sifwritew;
	tp->sifreadw = madgemc_sifreadw;
	tp->DataRate = (card->ringspeed)?SPEED_16:SPEED_4;

	memcpy(tp->ProductID, "Madge MCA 16/4    ", PROD_ID_SIZE + 1);

	tp->tmspriv = card;
	dev_set_drvdata(device, dev);

	if (register_netdev(dev) == 0)
		return 0;

	dev_set_drvdata(device, NULL);
	ret = -ENOMEM;
getout4:
	free_irq(dev->irq, dev);
getout3:
	release_region(dev->base_addr-MADGEMC_SIF_OFFSET, 
		       MADGEMC_IO_EXTENT); 
getout2:
	kfree(card);
getout1:
	free_netdev(dev);
getout:
	mca_device_set_claim(mdev, 0);
	return ret;
}

static irqreturn_t madgemc_interrupt(int irq, void *dev_id)
{
	int pending,reg1;
	struct net_device *dev;

	if (!dev_id) {
		printk("madgemc_interrupt: was not passed a dev_id!\n");
		return IRQ_NONE;
	}

	dev = dev_id;

	
	pending = inb(dev->base_addr + MC_CONTROL_REG0);
	if (!(pending & MC_CONTROL_REG0_SINTR))
		return IRQ_NONE; 

	pending = STS_SYSTEM_IRQ;
	do {
		if (pending & STS_SYSTEM_IRQ) {

			
			reg1 = inb(dev->base_addr + MC_CONTROL_REG1);
			outb(reg1 ^ MC_CONTROL_REG1_SINTEN, 
			     dev->base_addr + MC_CONTROL_REG1);
			outb(reg1, dev->base_addr + MC_CONTROL_REG1);

			
			tms380tr_interrupt(irq, dev_id);

			pending = SIFREADW(SIFSTS); 

		} else
			return IRQ_HANDLED; 
	} while (1);

	return IRQ_HANDLED; 
}

static unsigned short madgemc_setnselout_pins(struct net_device *dev)
{
	unsigned char reg1;
	struct net_local *tp = netdev_priv(dev);
	
	reg1 = inb(dev->base_addr + MC_CONTROL_REG1);

	if(tp->DataRate == SPEED_16)
		reg1 |= MC_CONTROL_REG1_SPEED_SEL; 
	else if (reg1 & MC_CONTROL_REG1_SPEED_SEL)
		reg1 ^= MC_CONTROL_REG1_SPEED_SEL; 
	outb(reg1, dev->base_addr + MC_CONTROL_REG1);

	return 0; 
}

static void madgemc_setregpage(struct net_device *dev, int page)
{	
	static int reg1;

	reg1 = inb(dev->base_addr + MC_CONTROL_REG1);
	if ((page == 0) && (reg1 & MC_CONTROL_REG1_SRSX)) {
		outb(reg1 ^ MC_CONTROL_REG1_SRSX, 
		     dev->base_addr + MC_CONTROL_REG1);
	}
	else if (page == 1) {
		outb(reg1 | MC_CONTROL_REG1_SRSX, 
		     dev->base_addr + MC_CONTROL_REG1);
	}
	reg1 = inb(dev->base_addr + MC_CONTROL_REG1);
}

static void madgemc_setsifsel(struct net_device *dev, int val)
{
	unsigned int reg0;

	reg0 = inb(dev->base_addr + MC_CONTROL_REG0);
	if ((val == 0) && (reg0 & MC_CONTROL_REG0_SIFSEL)) {
		outb(reg0 ^ MC_CONTROL_REG0_SIFSEL, 
		     dev->base_addr + MC_CONTROL_REG0);
	} else if (val == 1) {
		outb(reg0 | MC_CONTROL_REG0_SIFSEL, 
		     dev->base_addr + MC_CONTROL_REG0);
	}	
	reg0 = inb(dev->base_addr + MC_CONTROL_REG0);
}

static void madgemc_setint(struct net_device *dev, int val)
{
	unsigned int reg1;

	reg1 = inb(dev->base_addr + MC_CONTROL_REG1);
	if ((val == 0) && (reg1 & MC_CONTROL_REG1_SINTEN)) {
		outb(reg1 ^ MC_CONTROL_REG1_SINTEN, 
		     dev->base_addr + MC_CONTROL_REG1);
	} else if (val == 1) {
		outb(reg1 | MC_CONTROL_REG1_SINTEN, 
		     dev->base_addr + MC_CONTROL_REG1);
	}
}

static void madgemc_setcabletype(struct net_device *dev, int type)
{
	outb((type==0)?MC_CONTROL_REG7_CABLEUTP:MC_CONTROL_REG7_CABLESTP,
	     dev->base_addr + MC_CONTROL_REG7);
}

static int madgemc_chipset_init(struct net_device *dev)
{
	outb(0, dev->base_addr + MC_CONTROL_REG1); 
	tms380tr_wait(100); 

	
	outb(MC_CONTROL_REG1_NSRESET, dev->base_addr + MC_CONTROL_REG1);

	
	madgemc_setsifsel(dev, 1);

	
	madgemc_setint(dev, 1); 

	return 0;
}

static void madgemc_chipset_close(struct net_device *dev)
{
	
	madgemc_setint(dev, 0);
	
	madgemc_setsifsel(dev, 0);
}

static void madgemc_read_rom(struct net_device *dev, struct card_info *card)
{
	unsigned long ioaddr;
	unsigned char reg0, reg1, tmpreg0, i;

	ioaddr = dev->base_addr;

	reg0 = inb(ioaddr + MC_CONTROL_REG0);
	reg1 = inb(ioaddr + MC_CONTROL_REG1);

	
	tmpreg0 = reg0 & ~(MC_CONTROL_REG0_PAGE + MC_CONTROL_REG0_SIFSEL);
	outb(tmpreg0, ioaddr + MC_CONTROL_REG0);
	
	card->manid = inb(ioaddr + MC_ROM_MANUFACTURERID);
	card->cardtype = inb(ioaddr + MC_ROM_ADAPTERID);
	card->cardrev = inb(ioaddr + MC_ROM_REVISION);

	
	outb(tmpreg0 | MC_CONTROL_REG0_PAGE, ioaddr + MC_CONTROL_REG0);

	
	dev->addr_len = 6;
	for (i = 0; i < 6; i++)
		dev->dev_addr[i] = inb(ioaddr + MC_ROM_BIA_START + i);
	
	
	outb(reg0, ioaddr + MC_CONTROL_REG0);
	outb(reg1, ioaddr + MC_CONTROL_REG1);
}

static int madgemc_open(struct net_device *dev)
{  
	madgemc_chipset_init(dev);
	tms380tr_open(dev);
	return 0;
}

static int madgemc_close(struct net_device *dev)
{
	tms380tr_close(dev);
	madgemc_chipset_close(dev);
	return 0;
}

static int madgemc_mcaproc(char *buf, int slot, void *d) 
{	
	struct net_device *dev = (struct net_device *)d;
	struct net_local *tp = netdev_priv(dev);
	struct card_info *curcard = tp->tmspriv;
	int len = 0;
	
	len += sprintf(buf+len, "-------\n");
	if (curcard) {
		len += sprintf(buf+len, "Card Revision: %d\n", curcard->cardrev);
		len += sprintf(buf+len, "RAM Size: %dkb\n", curcard->ramsize);
		len += sprintf(buf+len, "Cable type: %s\n", (curcard->cabletype)?"STP/DB9":"UTP/RJ-45");
		len += sprintf(buf+len, "Configured ring speed: %dMb/sec\n", (curcard->ringspeed)?16:4);
		len += sprintf(buf+len, "Running ring speed: %dMb/sec\n", (tp->DataRate==SPEED_16)?16:4);
		len += sprintf(buf+len, "Device: %s\n", dev->name);
		len += sprintf(buf+len, "IO Port: 0x%04lx\n", dev->base_addr);
		len += sprintf(buf+len, "IRQ: %d\n", dev->irq);
		len += sprintf(buf+len, "Arbitration Level: %d\n", curcard->arblevel);
		len += sprintf(buf+len, "Burst Mode: ");
		switch(curcard->burstmode) {
		case 0: len += sprintf(buf+len, "Cycle steal"); break;
		case 1: len += sprintf(buf+len, "Limited burst"); break;
		case 2: len += sprintf(buf+len, "Delayed release"); break;
		case 3: len += sprintf(buf+len, "Immediate release"); break;
		}
		len += sprintf(buf+len, " (%s)\n", (curcard->fairness)?"Unfair":"Fair");
		
		len += sprintf(buf+len, "Ring Station Address: %pM\n",
			       dev->dev_addr);
	} else 
		len += sprintf(buf+len, "Card not configured\n");

	return len;
}

static int __devexit madgemc_remove(struct device *device)
{
	struct net_device *dev = dev_get_drvdata(device);
	struct net_local *tp;
        struct card_info *card;

	BUG_ON(!dev);

	tp = netdev_priv(dev);
	card = tp->tmspriv;
	kfree(card);
	tp->tmspriv = NULL;

	unregister_netdev(dev);
	release_region(dev->base_addr-MADGEMC_SIF_OFFSET, MADGEMC_IO_EXTENT);
	free_irq(dev->irq, dev);
	tmsdev_term(dev);
	free_netdev(dev);
	dev_set_drvdata(device, NULL);

	return 0;
}

static short madgemc_adapter_ids[] __initdata = {
	0x002d,
	0x0000
};

static struct mca_driver madgemc_driver = {
	.id_table = madgemc_adapter_ids,
	.driver = {
		.name = "madgemc",
		.bus = &mca_bus_type,
		.probe = madgemc_probe,
		.remove = __devexit_p(madgemc_remove),
	},
};

static int __init madgemc_init (void)
{
	madgemc_netdev_ops = tms380tr_netdev_ops;
	madgemc_netdev_ops.ndo_open = madgemc_open;
	madgemc_netdev_ops.ndo_stop = madgemc_close;

	return mca_register_driver (&madgemc_driver);
}

static void __exit madgemc_exit (void)
{
	mca_unregister_driver (&madgemc_driver);
}

module_init(madgemc_init);
module_exit(madgemc_exit);

MODULE_LICENSE("GPL");

