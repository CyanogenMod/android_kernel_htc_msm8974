/*
 * net/dsa/mv88e6131.c - Marvell 88e6095/6095f/6131 switch chip support
 * Copyright (c) 2008-2009 Marvell Semiconductor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/list.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/phy.h>
#include <net/dsa.h>
#include "mv88e6xxx.h"

#define ID_6085		0x04a0
#define ID_6095		0x0950
#define ID_6131		0x1060

static char *mv88e6131_probe(struct mii_bus *bus, int sw_addr)
{
	int ret;

	ret = __mv88e6xxx_reg_read(bus, sw_addr, REG_PORT(0), 0x03);
	if (ret >= 0) {
		ret &= 0xfff0;
		if (ret == ID_6085)
			return "Marvell 88E6085";
		if (ret == ID_6095)
			return "Marvell 88E6095/88E6095F";
		if (ret == ID_6131)
			return "Marvell 88E6131";
	}

	return NULL;
}

static int mv88e6131_switch_reset(struct dsa_switch *ds)
{
	int i;
	int ret;

	for (i = 0; i < 11; i++) {
		ret = REG_READ(REG_PORT(i), 0x04);
		REG_WRITE(REG_PORT(i), 0x04, ret & 0xfffc);
	}

	msleep(2);

	REG_WRITE(REG_GLOBAL, 0x04, 0xc400);

	for (i = 0; i < 1000; i++) {
		ret = REG_READ(REG_GLOBAL, 0x00);
		if ((ret & 0xc800) == 0xc800)
			break;

		msleep(1);
	}
	if (i == 1000)
		return -ETIMEDOUT;

	return 0;
}

static int mv88e6131_setup_global(struct dsa_switch *ds)
{
	int ret;
	int i;

	REG_WRITE(REG_GLOBAL, 0x04, 0x4400);

	REG_WRITE(REG_GLOBAL, 0x0a, 0x0148);

	ret = mv88e6xxx_config_prio(ds);
	if (ret < 0)
		return ret;

	REG_WRITE(REG_GLOBAL, 0x19, 0x8100);

	REG_WRITE(REG_GLOBAL, 0x1a, (dsa_upstream_port(ds) * 0x1100) | 0x00f0);

	if (ds->dst->pd->nr_chips > 1)
		REG_WRITE(REG_GLOBAL, 0x1c, 0xf000 | (ds->index & 0x1f));
	else
		REG_WRITE(REG_GLOBAL, 0x1c, 0xe000 | (ds->index & 0x1f));

	REG_WRITE(REG_GLOBAL2, 0x03, 0xffff);

	REG_WRITE(REG_GLOBAL2, 0x05, 0x00ff);

	for (i = 0; i < 32; i++) {
		int nexthop;

		nexthop = 0x1f;
		if (i != ds->index && i < ds->dst->pd->nr_chips)
			nexthop = ds->pd->rtable[i] & 0x1f;

		REG_WRITE(REG_GLOBAL2, 0x06, 0x8000 | (i << 8) | nexthop);
	}

	for (i = 0; i < 8; i++)
		REG_WRITE(REG_GLOBAL2, 0x07, 0x8000 | (i << 12) | 0x7ff);

	for (i = 0; i < 16; i++)
		REG_WRITE(REG_GLOBAL2, 0x08, 0x8000 | (i << 11));

	REG_WRITE(REG_GLOBAL2, 0x0f, 0x00ff);

	return 0;
}

static int mv88e6131_setup_port(struct dsa_switch *ds, int p)
{
	struct mv88e6xxx_priv_state *ps = (void *)(ds + 1);
	int addr = REG_PORT(p);
	u16 val;

	if (dsa_is_cpu_port(ds, p) || ds->dsa_port_mask & (1 << p))
		if (ps->id == ID_6085)
			REG_WRITE(addr, 0x01, 0x003d); 
		else
			REG_WRITE(addr, 0x01, 0x003e); 
	else
		REG_WRITE(addr, 0x01, 0x0003);

	val = 0x0433;
	if (p == dsa_upstream_port(ds)) {
		val |= 0x0104;
		if (ps->id == ID_6085)
			val |= 0x0008;
	}
	if (ds->dsa_port_mask & (1 << p))
		val |= 0x0100;
	REG_WRITE(addr, 0x04, val);

	REG_WRITE(addr, 0x05, dsa_is_cpu_port(ds, p) ? 0x8000 : 0x0000);

	val = (p & 0xf) << 12;
	if (dsa_is_cpu_port(ds, p))
		val |= ds->phys_port_mask;
	else
		val |= 1 << dsa_upstream_port(ds);
	REG_WRITE(addr, 0x06, val);

	REG_WRITE(addr, 0x07, 0x0000);

	if (ps->id == ID_6085)
		REG_WRITE(addr, 0x08, 0x0080);
	else {
		val = 0x0080 | dsa_upstream_port(ds);
		if (p == dsa_upstream_port(ds))
			val |= 0x0040;
		REG_WRITE(addr, 0x08, val);
	}

	REG_WRITE(addr, 0x09, 0x0000);

	REG_WRITE(addr, 0x0a, 0x0000);

	REG_WRITE(addr, 0x0b, 1 << p);

	REG_WRITE(addr, 0x18, 0x3210);

	REG_WRITE(addr, 0x19, 0x7654);

	return 0;
}

static int mv88e6131_setup(struct dsa_switch *ds)
{
	struct mv88e6xxx_priv_state *ps = (void *)(ds + 1);
	int i;
	int ret;

	mutex_init(&ps->smi_mutex);
	mv88e6xxx_ppu_state_init(ds);
	mutex_init(&ps->stats_mutex);

	ps->id = REG_READ(REG_PORT(0), 0x03) & 0xfff0;

	ret = mv88e6131_switch_reset(ds);
	if (ret < 0)
		return ret;

	

	ret = mv88e6131_setup_global(ds);
	if (ret < 0)
		return ret;

	for (i = 0; i < 11; i++) {
		ret = mv88e6131_setup_port(ds, i);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int mv88e6131_port_to_phy_addr(int port)
{
	if (port >= 0 && port <= 11)
		return port;
	return -1;
}

static int
mv88e6131_phy_read(struct dsa_switch *ds, int port, int regnum)
{
	int addr = mv88e6131_port_to_phy_addr(port);
	return mv88e6xxx_phy_read_ppu(ds, addr, regnum);
}

static int
mv88e6131_phy_write(struct dsa_switch *ds,
			      int port, int regnum, u16 val)
{
	int addr = mv88e6131_port_to_phy_addr(port);
	return mv88e6xxx_phy_write_ppu(ds, addr, regnum, val);
}

static struct mv88e6xxx_hw_stat mv88e6131_hw_stats[] = {
	{ "in_good_octets", 8, 0x00, },
	{ "in_bad_octets", 4, 0x02, },
	{ "in_unicast", 4, 0x04, },
	{ "in_broadcasts", 4, 0x06, },
	{ "in_multicasts", 4, 0x07, },
	{ "in_pause", 4, 0x16, },
	{ "in_undersize", 4, 0x18, },
	{ "in_fragments", 4, 0x19, },
	{ "in_oversize", 4, 0x1a, },
	{ "in_jabber", 4, 0x1b, },
	{ "in_rx_error", 4, 0x1c, },
	{ "in_fcs_error", 4, 0x1d, },
	{ "out_octets", 8, 0x0e, },
	{ "out_unicast", 4, 0x10, },
	{ "out_broadcasts", 4, 0x13, },
	{ "out_multicasts", 4, 0x12, },
	{ "out_pause", 4, 0x15, },
	{ "excessive", 4, 0x11, },
	{ "collisions", 4, 0x1e, },
	{ "deferred", 4, 0x05, },
	{ "single", 4, 0x14, },
	{ "multiple", 4, 0x17, },
	{ "out_fcs_error", 4, 0x03, },
	{ "late", 4, 0x1f, },
	{ "hist_64bytes", 4, 0x08, },
	{ "hist_65_127bytes", 4, 0x09, },
	{ "hist_128_255bytes", 4, 0x0a, },
	{ "hist_256_511bytes", 4, 0x0b, },
	{ "hist_512_1023bytes", 4, 0x0c, },
	{ "hist_1024_max_bytes", 4, 0x0d, },
};

static void
mv88e6131_get_strings(struct dsa_switch *ds, int port, uint8_t *data)
{
	mv88e6xxx_get_strings(ds, ARRAY_SIZE(mv88e6131_hw_stats),
			      mv88e6131_hw_stats, port, data);
}

static void
mv88e6131_get_ethtool_stats(struct dsa_switch *ds,
				  int port, uint64_t *data)
{
	mv88e6xxx_get_ethtool_stats(ds, ARRAY_SIZE(mv88e6131_hw_stats),
				    mv88e6131_hw_stats, port, data);
}

static int mv88e6131_get_sset_count(struct dsa_switch *ds)
{
	return ARRAY_SIZE(mv88e6131_hw_stats);
}

struct dsa_switch_driver mv88e6131_switch_driver = {
	.tag_protocol		= cpu_to_be16(ETH_P_DSA),
	.priv_size		= sizeof(struct mv88e6xxx_priv_state),
	.probe			= mv88e6131_probe,
	.setup			= mv88e6131_setup,
	.set_addr		= mv88e6xxx_set_addr_direct,
	.phy_read		= mv88e6131_phy_read,
	.phy_write		= mv88e6131_phy_write,
	.poll_link		= mv88e6xxx_poll_link,
	.get_strings		= mv88e6131_get_strings,
	.get_ethtool_stats	= mv88e6131_get_ethtool_stats,
	.get_sset_count		= mv88e6131_get_sset_count,
};

MODULE_ALIAS("platform:mv88e6085");
MODULE_ALIAS("platform:mv88e6095");
MODULE_ALIAS("platform:mv88e6095f");
MODULE_ALIAS("platform:mv88e6131");
