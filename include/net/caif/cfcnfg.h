/*
 * Copyright (C) ST-Ericsson AB 2010
 * Author:	Sjur Brendeland/sjur.brandeland@stericsson.com
 * License terms: GNU General Public License (GPL) version 2
 */

#ifndef CFCNFG_H_
#define CFCNFG_H_
#include <linux/spinlock.h>
#include <linux/netdevice.h>
#include <net/caif/caif_layer.h>
#include <net/caif/cfctrl.h>

struct cfcnfg;

enum cfcnfg_phy_preference {
	CFPHYPREF_UNSPECIFIED,
	CFPHYPREF_LOW_LAT,
	CFPHYPREF_HIGH_BW,
	CFPHYPREF_LOOP
};

struct cfcnfg *get_cfcnfg(struct net *net);

struct cfcnfg *cfcnfg_create(void);

void cfcnfg_remove(struct cfcnfg *cfg);

void
cfcnfg_add_phy_layer(struct cfcnfg *cnfg,
		     struct net_device *dev, struct cflayer *phy_layer,
		     enum cfcnfg_phy_preference pref,
		     struct cflayer *link_support,
		     bool fcs, int head_room);

int cfcnfg_del_phy_layer(struct cfcnfg *cnfg, struct cflayer *phy_layer);

int cfcnfg_set_phy_state(struct cfcnfg *cnfg, struct cflayer *phy_layer,
				bool up);

#endif				
