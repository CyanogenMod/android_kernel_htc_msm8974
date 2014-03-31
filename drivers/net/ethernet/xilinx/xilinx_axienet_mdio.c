/*
 * MDIO bus driver for the Xilinx Axi Ethernet device
 *
 * Copyright (c) 2009 Secret Lab Technologies, Ltd.
 * Copyright (c) 2010 - 2011 Michal Simek <monstr@monstr.eu>
 * Copyright (c) 2010 - 2011 PetaLogix
 * Copyright (c) 2010 - 2012 Xilinx, Inc. All rights reserved.
 */

#include <linux/of_address.h>
#include <linux/of_mdio.h>
#include <linux/jiffies.h>

#include "xilinx_axienet.h"

#define MAX_MDIO_FREQ		2500000 
#define DEFAULT_CLOCK_DIVISOR	XAE_MDIO_DIV_DFT

int axienet_mdio_wait_until_ready(struct axienet_local *lp)
{
	long end = jiffies + 2;
	while (!(axienet_ior(lp, XAE_MDIO_MCR_OFFSET) &
		 XAE_MDIO_MCR_READY_MASK)) {
		if (end - jiffies <= 0) {
			WARN_ON(1);
			return -ETIMEDOUT;
		}
		udelay(1);
	}
	return 0;
}

static int axienet_mdio_read(struct mii_bus *bus, int phy_id, int reg)
{
	u32 rc;
	int ret;
	struct axienet_local *lp = bus->priv;

	ret = axienet_mdio_wait_until_ready(lp);
	if (ret < 0)
		return ret;

	axienet_iow(lp, XAE_MDIO_MCR_OFFSET,
		    (((phy_id << XAE_MDIO_MCR_PHYAD_SHIFT) &
		      XAE_MDIO_MCR_PHYAD_MASK) |
		     ((reg << XAE_MDIO_MCR_REGAD_SHIFT) &
		      XAE_MDIO_MCR_REGAD_MASK) |
		     XAE_MDIO_MCR_INITIATE_MASK |
		     XAE_MDIO_MCR_OP_READ_MASK));

	ret = axienet_mdio_wait_until_ready(lp);
	if (ret < 0)
		return ret;

	rc = axienet_ior(lp, XAE_MDIO_MRD_OFFSET) & 0x0000FFFF;

	dev_dbg(lp->dev, "axienet_mdio_read(phy_id=%i, reg=%x) == %x\n",
		phy_id, reg, rc);

	return rc;
}

/**
 * axienet_mdio_write - MDIO interface write function
 * @bus:	Pointer to mii bus structure
 * @phy_id:	Address of the PHY device
 * @reg:	PHY register to write to
 * @val:	Value to be written into the register
 *
 * returns:	0 on success, -ETIMEDOUT on a timeout
 *
 * Writes the value to the requested register by first writing the value
 * into MWD register. The the MCR register is then appropriately setup
 * to finish the write operation.
 */
static int axienet_mdio_write(struct mii_bus *bus, int phy_id, int reg,
			      u16 val)
{
	int ret;
	struct axienet_local *lp = bus->priv;

	dev_dbg(lp->dev, "axienet_mdio_write(phy_id=%i, reg=%x, val=%x)\n",
		phy_id, reg, val);

	ret = axienet_mdio_wait_until_ready(lp);
	if (ret < 0)
		return ret;

	axienet_iow(lp, XAE_MDIO_MWD_OFFSET, (u32) val);
	axienet_iow(lp, XAE_MDIO_MCR_OFFSET,
		    (((phy_id << XAE_MDIO_MCR_PHYAD_SHIFT) &
		      XAE_MDIO_MCR_PHYAD_MASK) |
		     ((reg << XAE_MDIO_MCR_REGAD_SHIFT) &
		      XAE_MDIO_MCR_REGAD_MASK) |
		     XAE_MDIO_MCR_INITIATE_MASK |
		     XAE_MDIO_MCR_OP_WRITE_MASK));

	ret = axienet_mdio_wait_until_ready(lp);
	if (ret < 0)
		return ret;
	return 0;
}

int axienet_mdio_setup(struct axienet_local *lp, struct device_node *np)
{
	int ret;
	u32 clk_div, host_clock;
	u32 *property_p;
	struct mii_bus *bus;
	struct resource res;
	struct device_node *np1;


	np1 = of_find_node_by_name(NULL, "cpu");
	if (!np1) {
		printk(KERN_WARNING "%s(): Could not find CPU device node.",
		       __func__);
		printk(KERN_WARNING "Setting MDIO clock divisor to "
		       "default %d\n", DEFAULT_CLOCK_DIVISOR);
		clk_div = DEFAULT_CLOCK_DIVISOR;
		goto issue;
	}
	property_p = (u32 *) of_get_property(np1, "clock-frequency", NULL);
	if (!property_p) {
		printk(KERN_WARNING "%s(): Could not find CPU property: "
		       "clock-frequency.", __func__);
		printk(KERN_WARNING "Setting MDIO clock divisor to "
		       "default %d\n", DEFAULT_CLOCK_DIVISOR);
		clk_div = DEFAULT_CLOCK_DIVISOR;
		goto issue;
	}

	host_clock = be32_to_cpup(property_p);
	clk_div = (host_clock / (MAX_MDIO_FREQ * 2)) - 1;
	if (host_clock % (MAX_MDIO_FREQ * 2))
		clk_div++;

	printk(KERN_DEBUG "%s(): Setting MDIO clock divisor to %u based "
	       "on %u Hz host clock.\n", __func__, clk_div, host_clock);

	of_node_put(np1);
issue:
	axienet_iow(lp, XAE_MDIO_MC_OFFSET,
		    (((u32) clk_div) | XAE_MDIO_MC_MDIOEN_MASK));

	ret = axienet_mdio_wait_until_ready(lp);
	if (ret < 0)
		return ret;

	bus = mdiobus_alloc();
	if (!bus)
		return -ENOMEM;

	np1 = of_get_parent(lp->phy_node);
	of_address_to_resource(np1, 0, &res);
	snprintf(bus->id, MII_BUS_ID_SIZE, "%.8llx",
		 (unsigned long long) res.start);

	bus->priv = lp;
	bus->name = "Xilinx Axi Ethernet MDIO";
	bus->read = axienet_mdio_read;
	bus->write = axienet_mdio_write;
	bus->parent = lp->dev;
	bus->irq = lp->mdio_irqs; 
	lp->mii_bus = bus;

	ret = of_mdiobus_register(bus, np1);
	if (ret) {
		mdiobus_free(bus);
		return ret;
	}
	return 0;
}

void axienet_mdio_teardown(struct axienet_local *lp)
{
	mdiobus_unregister(lp->mii_bus);
	kfree(lp->mii_bus->irq);
	mdiobus_free(lp->mii_bus);
	lp->mii_bus = NULL;
}
