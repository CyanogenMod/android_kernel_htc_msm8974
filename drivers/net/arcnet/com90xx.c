/*
 * Linux ARCnet driver - COM90xx chipset (memory-mapped buffers)
 * 
 * Written 1994-1999 by Avery Pennarun.
 * Written 1999 by Martin Mares <mj@ucw.cz>.
 * Derived from skeleton.c by Donald Becker.
 *
 * Special thanks to Contemporary Controls, Inc. (www.ccontrols.com)
 *  for sponsoring the further development of this driver.
 *
 * **********************
 *
 * The original copyright of skeleton.c was as follows:
 *
 * skeleton.c Written 1993 by Donald Becker.
 * Copyright 1993 United States Government as represented by the
 * Director, National Security Agency.  This software may only be used
 * and distributed according to the terms of the GNU General Public License as
 * modified by SRC, incorporated herein by reference.
 *
 * **********************
 *
 * For more details, see drivers/net/arcnet.c
 *
 * **********************
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <linux/arcdevice.h>


#define VERSION "arcnet: COM90xx chipset support\n"


#undef FAST_PROBE


static int com90xx_found(int ioaddr, int airq, u_long shmem, void __iomem *);
static void com90xx_command(struct net_device *dev, int command);
static int com90xx_status(struct net_device *dev);
static void com90xx_setmask(struct net_device *dev, int mask);
static int com90xx_reset(struct net_device *dev, int really_reset);
static void com90xx_copy_to_card(struct net_device *dev, int bufnum, int offset,
				 void *buf, int count);
static void com90xx_copy_from_card(struct net_device *dev, int bufnum, int offset,
				   void *buf, int count);


static struct net_device *cards[16];
static int numcards;


#define ARCNET_TOTAL_SIZE	16

#define BUFFER_SIZE (512)
#define MIRROR_SIZE (BUFFER_SIZE*4)

#define _INTMASK (ioaddr+0)	
#define _STATUS  (ioaddr+0)	
#define _COMMAND (ioaddr+1)	
#define _CONFIG  (ioaddr+2)	
#define _RESET   (ioaddr+8)	
#define _MEMDATA (ioaddr+12)	
#define _ADDR_HI (ioaddr+15)	
#define _ADDR_LO (ioaddr+14)

#undef ASTATUS
#undef ACOMMAND
#undef AINTMASK

#define ASTATUS()	inb(_STATUS)
#define ACOMMAND(cmd) 	outb((cmd),_COMMAND)
#define AINTMASK(msk)	outb((msk),_INTMASK)


static int com90xx_skip_probe __initdata = 0;


static int io;			
static int irq;
static int shmem;
static char device[9];		

module_param(io, int, 0);
module_param(irq, int, 0);
module_param(shmem, int, 0);
module_param_string(device, device, sizeof(device), 0);

static void __init com90xx_probe(void)
{
	int count, status, ioaddr, numprint, airq, openparen = 0;
	unsigned long airqmask;
	int ports[(0x3f0 - 0x200) / 16 + 1] =
	{0};
	unsigned long *shmems;
	void __iomem **iomem;
	int numports, numshmems, *port;
	u_long *p;
	int index;

	if (!io && !irq && !shmem && !*device && com90xx_skip_probe)
		return;

	shmems = kzalloc(((0x100000-0xa0000) / 0x800) * sizeof(unsigned long),
			 GFP_KERNEL);
	if (!shmems)
		return;
	iomem = kzalloc(((0x100000-0xa0000) / 0x800) * sizeof(void __iomem *),
			 GFP_KERNEL);
	if (!iomem) {
		kfree(shmems);
		return;
	}

	BUGLVL(D_NORMAL) printk(VERSION);

	
	numports = numshmems = 0;
	if (io)
		ports[numports++] = io;
	else
		for (count = 0x200; count <= 0x3f0; count += 16)
			ports[numports++] = count;
	if (shmem)
		shmems[numshmems++] = shmem;
	else
		for (count = 0xA0000; count <= 0xFF800; count += 2048)
			shmems[numshmems++] = count;

	numprint = -1;
	for (port = &ports[0]; port - ports < numports; port++) {
		numprint++;
		numprint %= 8;
		if (!numprint) {
			BUGMSG2(D_INIT, "\n");
			BUGMSG2(D_INIT, "S1: ");
		}
		BUGMSG2(D_INIT, "%Xh ", *port);

		ioaddr = *port;

		if (!request_region(*port, ARCNET_TOTAL_SIZE, "arcnet (90xx)")) {
			BUGMSG2(D_INIT_REASONS, "(request_region)\n");
			BUGMSG2(D_INIT_REASONS, "S1: ");
			BUGLVL(D_INIT_REASONS) numprint = 0;
			*port-- = ports[--numports];
			continue;
		}
		if (ASTATUS() == 0xFF) {
			BUGMSG2(D_INIT_REASONS, "(empty)\n");
			BUGMSG2(D_INIT_REASONS, "S1: ");
			BUGLVL(D_INIT_REASONS) numprint = 0;
			release_region(*port, ARCNET_TOTAL_SIZE);
			*port-- = ports[--numports];
			continue;
		}
		inb(_RESET);	

		BUGMSG2(D_INIT_REASONS, "\n");
		BUGMSG2(D_INIT_REASONS, "S1: ");
		BUGLVL(D_INIT_REASONS) numprint = 0;
	}
	BUGMSG2(D_INIT, "\n");

	if (!numports) {
		BUGMSG2(D_NORMAL, "S1: No ARCnet cards found.\n");
		kfree(shmems);
		kfree(iomem);
		return;
	}
	numprint = -1;
	for (port = &ports[0]; port < ports + numports; port++) {
		numprint++;
		numprint %= 8;
		if (!numprint) {
			BUGMSG2(D_INIT, "\n");
			BUGMSG2(D_INIT, "S2: ");
		}
		BUGMSG2(D_INIT, "%Xh ", *port);
	}
	BUGMSG2(D_INIT, "\n");
	mdelay(RESETtime);

	numprint = -1;
	for (index = 0, p = &shmems[0]; index < numshmems; p++, index++) {
		void __iomem *base;

		numprint++;
		numprint %= 8;
		if (!numprint) {
			BUGMSG2(D_INIT, "\n");
			BUGMSG2(D_INIT, "S3: ");
		}
		BUGMSG2(D_INIT, "%lXh ", *p);

		if (!request_mem_region(*p, MIRROR_SIZE, "arcnet (90xx)")) {
			BUGMSG2(D_INIT_REASONS, "(request_mem_region)\n");
			BUGMSG2(D_INIT_REASONS, "Stage 3: ");
			BUGLVL(D_INIT_REASONS) numprint = 0;
			goto out;
		}
		base = ioremap(*p, MIRROR_SIZE);
		if (!base) {
			BUGMSG2(D_INIT_REASONS, "(ioremap)\n");
			BUGMSG2(D_INIT_REASONS, "Stage 3: ");
			BUGLVL(D_INIT_REASONS) numprint = 0;
			goto out1;
		}
		if (readb(base) != TESTvalue) {
			BUGMSG2(D_INIT_REASONS, "(%02Xh != %02Xh)\n",
				readb(base), TESTvalue);
			BUGMSG2(D_INIT_REASONS, "S3: ");
			BUGLVL(D_INIT_REASONS) numprint = 0;
			goto out2;
		}
		writeb(0x42, base);
		if (readb(base) != 0x42) {
			BUGMSG2(D_INIT_REASONS, "(read only)\n");
			BUGMSG2(D_INIT_REASONS, "S3: ");
			goto out2;
		}
		BUGMSG2(D_INIT_REASONS, "\n");
		BUGMSG2(D_INIT_REASONS, "S3: ");
		BUGLVL(D_INIT_REASONS) numprint = 0;
		iomem[index] = base;
		continue;
	out2:
		iounmap(base);
	out1:
		release_mem_region(*p, MIRROR_SIZE);
	out:
		*p-- = shmems[--numshmems];
		index--;
	}
	BUGMSG2(D_INIT, "\n");

	if (!numshmems) {
		BUGMSG2(D_NORMAL, "S3: No ARCnet cards found.\n");
		for (port = &ports[0]; port < ports + numports; port++)
			release_region(*port, ARCNET_TOTAL_SIZE);
		kfree(shmems);
		kfree(iomem);
		return;
	}
	numprint = -1;
	for (p = &shmems[0]; p < shmems + numshmems; p++) {
		numprint++;
		numprint %= 8;
		if (!numprint) {
			BUGMSG2(D_INIT, "\n");
			BUGMSG2(D_INIT, "S4: ");
		}
		BUGMSG2(D_INIT, "%lXh ", *p);
	}
	BUGMSG2(D_INIT, "\n");

	numprint = -1;
	for (port = &ports[0]; port < ports + numports; port++) {
		int found = 0;
		numprint++;
		numprint %= 8;
		if (!numprint) {
			BUGMSG2(D_INIT, "\n");
			BUGMSG2(D_INIT, "S5: ");
		}
		BUGMSG2(D_INIT, "%Xh ", *port);

		ioaddr = *port;
		status = ASTATUS();

		if ((status & 0x9D)
		    != (NORXflag | RECONflag | TXFREEflag | RESETflag)) {
			BUGMSG2(D_INIT_REASONS, "(status=%Xh)\n", status);
			BUGMSG2(D_INIT_REASONS, "S5: ");
			BUGLVL(D_INIT_REASONS) numprint = 0;
			release_region(*port, ARCNET_TOTAL_SIZE);
			*port-- = ports[--numports];
			continue;
		}
		ACOMMAND(CFLAGScmd | RESETclear | CONFIGclear);
		status = ASTATUS();
		if (status & RESETflag) {
			BUGMSG2(D_INIT_REASONS, " (eternal reset, status=%Xh)\n",
				status);
			BUGMSG2(D_INIT_REASONS, "S5: ");
			BUGLVL(D_INIT_REASONS) numprint = 0;
			release_region(*port, ARCNET_TOTAL_SIZE);
			*port-- = ports[--numports];
			continue;
		}
		if (!irq) {
			airqmask = probe_irq_on();
			AINTMASK(NORXflag);
			udelay(1);
			AINTMASK(0);
			airq = probe_irq_off(airqmask);

			if (airq <= 0) {
				BUGMSG2(D_INIT_REASONS, "(airq=%d)\n", airq);
				BUGMSG2(D_INIT_REASONS, "S5: ");
				BUGLVL(D_INIT_REASONS) numprint = 0;
				release_region(*port, ARCNET_TOTAL_SIZE);
				*port-- = ports[--numports];
				continue;
			}
		} else {
			airq = irq;
		}

		BUGMSG2(D_INIT, "(%d,", airq);
		openparen = 1;

#ifdef FAST_PROBE
		if (numports > 1 || numshmems > 1) {
			inb(_RESET);
			mdelay(RESETtime);
		} else {
			
			writeb(TESTvalue, iomem[0]);
		}
#else
		inb(_RESET);
		mdelay(RESETtime);
#endif

		for (index = 0; index < numshmems; index++) {
			u_long ptr = shmems[index];
			void __iomem *base = iomem[index];

			if (readb(base) == TESTvalue) {	
				BUGMSG2(D_INIT, "%lXh)\n", *p);
				openparen = 0;

				
				if (com90xx_found(*port, airq, ptr, base) == 0)
					found = 1;
				numprint = -1;

				
				shmems[index] = shmems[--numshmems];
				iomem[index] = iomem[numshmems];
				break;	
			} else {
				BUGMSG2(D_INIT_REASONS, "%Xh-", readb(base));
			}
		}

		if (openparen) {
			BUGLVL(D_INIT) printk("no matching shmem)\n");
			BUGLVL(D_INIT_REASONS) printk("S5: ");
			BUGLVL(D_INIT_REASONS) numprint = 0;
		}
		if (!found)
			release_region(*port, ARCNET_TOTAL_SIZE);
		*port-- = ports[--numports];
	}

	BUGLVL(D_INIT_REASONS) printk("\n");

	
	for (index = 0; index < numshmems; index++) {
		writeb(TESTvalue, iomem[index]);
		iounmap(iomem[index]);
		release_mem_region(shmems[index], MIRROR_SIZE);
	}
	kfree(shmems);
	kfree(iomem);
}

static int check_mirror(unsigned long addr, size_t size)
{
	void __iomem *p;
	int res = -1;

	if (!request_mem_region(addr, size, "arcnet (90xx)"))
		return -1;

	p = ioremap(addr, size);
	if (p) {
		if (readb(p) == TESTvalue)
			res = 1;
		else
			res = 0;
		iounmap(p);
	}

	release_mem_region(addr, size);
	return res;
}

static int __init com90xx_found(int ioaddr, int airq, u_long shmem, void __iomem *p)
{
	struct net_device *dev = NULL;
	struct arcnet_local *lp;
	u_long first_mirror, last_mirror;
	int mirror_size;

	
	dev = alloc_arcdev(device);
	if (!dev) {
		BUGMSG2(D_NORMAL, "com90xx: Can't allocate device!\n");
		iounmap(p);
		release_mem_region(shmem, MIRROR_SIZE);
		return -ENOMEM;
	}
	lp = netdev_priv(dev);
	

	mirror_size = MIRROR_SIZE;
	if (readb(p) == TESTvalue &&
	    check_mirror(shmem - MIRROR_SIZE, MIRROR_SIZE) == 0 &&
	    check_mirror(shmem - 2 * MIRROR_SIZE, MIRROR_SIZE) == 1)
		mirror_size = 2 * MIRROR_SIZE;

	first_mirror = shmem - mirror_size;
	while (check_mirror(first_mirror, mirror_size) == 1)
		first_mirror -= mirror_size;
	first_mirror += mirror_size;

	last_mirror = shmem + mirror_size;
	while (check_mirror(last_mirror, mirror_size) == 1)
		last_mirror += mirror_size;
	last_mirror -= mirror_size;

	dev->mem_start = first_mirror;
	dev->mem_end = last_mirror + MIRROR_SIZE - 1;

	iounmap(p);
	release_mem_region(shmem, MIRROR_SIZE);

	if (!request_mem_region(dev->mem_start, dev->mem_end - dev->mem_start + 1, "arcnet (90xx)"))
		goto err_free_dev;

	
	if (request_irq(airq, arcnet_interrupt, 0, "arcnet (90xx)", dev)) {
		BUGMSG(D_NORMAL, "Can't get IRQ %d!\n", airq);
		goto err_release_mem;
	}
	dev->irq = airq;

	
	lp->card_name = "COM90xx";
	lp->hw.command = com90xx_command;
	lp->hw.status = com90xx_status;
	lp->hw.intmask = com90xx_setmask;
	lp->hw.reset = com90xx_reset;
	lp->hw.owner = THIS_MODULE;
	lp->hw.copy_to_card = com90xx_copy_to_card;
	lp->hw.copy_from_card = com90xx_copy_from_card;
	lp->mem_start = ioremap(dev->mem_start, dev->mem_end - dev->mem_start + 1);
	if (!lp->mem_start) {
		BUGMSG(D_NORMAL, "Can't remap device memory!\n");
		goto err_free_irq;
	}

	
	dev->dev_addr[0] = readb(lp->mem_start + 1);

	dev->base_addr = ioaddr;

	BUGMSG(D_NORMAL, "COM90xx station %02Xh found at %03lXh, IRQ %d, "
	       "ShMem %lXh (%ld*%xh).\n",
	       dev->dev_addr[0],
	       dev->base_addr, dev->irq, dev->mem_start,
	 (dev->mem_end - dev->mem_start + 1) / mirror_size, mirror_size);

	if (register_netdev(dev))
		goto err_unmap;

	cards[numcards++] = dev;
	return 0;

err_unmap:
	iounmap(lp->mem_start);
err_free_irq:
	free_irq(dev->irq, dev);
err_release_mem:
	release_mem_region(dev->mem_start, dev->mem_end - dev->mem_start + 1);
err_free_dev:
	free_netdev(dev);
	return -EIO;
}


static void com90xx_command(struct net_device *dev, int cmd)
{
	short ioaddr = dev->base_addr;

	ACOMMAND(cmd);
}


static int com90xx_status(struct net_device *dev)
{
	short ioaddr = dev->base_addr;

	return ASTATUS();
}


static void com90xx_setmask(struct net_device *dev, int mask)
{
	short ioaddr = dev->base_addr;

	AINTMASK(mask);
}


static int com90xx_reset(struct net_device *dev, int really_reset)
{
	struct arcnet_local *lp = netdev_priv(dev);
	short ioaddr = dev->base_addr;

	BUGMSG(D_INIT, "Resetting (status=%02Xh)\n", ASTATUS());

	if (really_reset) {
		
		inb(_RESET);
		mdelay(RESETtime);
	}
	ACOMMAND(CFLAGScmd | RESETclear);	
	ACOMMAND(CFLAGScmd | CONFIGclear);

	
	

	
	if (readb(lp->mem_start) != TESTvalue) {
		if (really_reset)
			BUGMSG(D_NORMAL, "reset failed: TESTvalue not present.\n");
		return 1;
	}
	
	ACOMMAND(CONFIGcmd | EXTconf);

	
	BUGLVL(D_DURING)
	    memset_io(lp->mem_start, 0x42, 2048);

	
	return 0;
}

static void com90xx_copy_to_card(struct net_device *dev, int bufnum, int offset,
				 void *buf, int count)
{
	struct arcnet_local *lp = netdev_priv(dev);
	void __iomem *memaddr = lp->mem_start + bufnum * 512 + offset;
	TIME("memcpy_toio", count, memcpy_toio(memaddr, buf, count));
}


static void com90xx_copy_from_card(struct net_device *dev, int bufnum, int offset,
				   void *buf, int count)
{
	struct arcnet_local *lp = netdev_priv(dev);
	void __iomem *memaddr = lp->mem_start + bufnum * 512 + offset;
	TIME("memcpy_fromio", count, memcpy_fromio(buf, memaddr, count));
}


MODULE_LICENSE("GPL");

static int __init com90xx_init(void)
{
	if (irq == 2)
		irq = 9;
	com90xx_probe();
	if (!numcards)
		return -EIO;
	return 0;
}

static void __exit com90xx_exit(void)
{
	struct net_device *dev;
	struct arcnet_local *lp;
	int count;

	for (count = 0; count < numcards; count++) {
		dev = cards[count];
		lp = netdev_priv(dev);

		unregister_netdev(dev);
		free_irq(dev->irq, dev);
		iounmap(lp->mem_start);
		release_region(dev->base_addr, ARCNET_TOTAL_SIZE);
		release_mem_region(dev->mem_start, dev->mem_end - dev->mem_start + 1);
		free_netdev(dev);
	}
}

module_init(com90xx_init);
module_exit(com90xx_exit);

#ifndef MODULE
static int __init com90xx_setup(char *s)
{
	int ints[8];

	s = get_options(s, 8, ints);
	if (!ints[0] && !*s) {
		printk("com90xx: Disabled.\n");
		return 1;
	}

	switch (ints[0]) {
	default:		
		printk("com90xx: Too many arguments.\n");
	case 3:		
		shmem = ints[3];
	case 2:		
		irq = ints[2];
	case 1:		
		io = ints[1];
	}

	if (*s)
		snprintf(device, sizeof(device), "%s", s);

	return 1;
}

__setup("com90xx=", com90xx_setup);
#endif
