/*
 * Copyright (C) ST-Ericsson SA 2012
 * Author: Johan Gardsmark <johan.gardsmark@stericsson.com> for ST-Ericsson.
 * License terms:  GNU General Public License (GPL), version 2
 */

#ifndef _UX500_CHARGALG_H
#define _UX500_CHARGALG_H

#include <linux/power_supply.h>

#define psy_to_ux500_charger(x) container_of((x), \
		struct ux500_charger, psy)

struct ux500_charger;

struct ux500_charger_ops {
	int (*enable) (struct ux500_charger *, int, int, int);
	int (*kick_wd) (struct ux500_charger *);
	int (*update_curr) (struct ux500_charger *, int);
};

struct ux500_charger {
	struct power_supply psy;
	struct ux500_charger_ops ops;
	int max_out_volt;
	int max_out_curr;
};

#endif
