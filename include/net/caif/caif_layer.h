/*
 * Copyright (C) ST-Ericsson AB 2010
 * Author:	Sjur Brendeland / sjur.brandeland@stericsson.com
 * License terms: GNU General Public License (GPL) version 2
 */

#ifndef CAIF_LAYER_H_
#define CAIF_LAYER_H_

#include <linux/list.h>

struct cflayer;
struct cfpkt;
struct cfpktq;
struct caif_payload_info;
struct caif_packet_funcs;

#define CAIF_LAYER_NAME_SZ 16

#define caif_assert(assert)					\
do {								\
	if (!(assert)) {					\
		pr_err("caif:Assert detected:'%s'\n", #assert); \
		WARN_ON(!(assert));				\
	}							\
} while (0)

enum caif_ctrlcmd {
	CAIF_CTRLCMD_FLOW_OFF_IND,
	CAIF_CTRLCMD_FLOW_ON_IND,
	CAIF_CTRLCMD_REMOTE_SHUTDOWN_IND,
	CAIF_CTRLCMD_INIT_RSP,
	CAIF_CTRLCMD_DEINIT_RSP,
	CAIF_CTRLCMD_INIT_FAIL_RSP,
	_CAIF_CTRLCMD_PHYIF_FLOW_OFF_IND,
	_CAIF_CTRLCMD_PHYIF_FLOW_ON_IND,
	_CAIF_CTRLCMD_PHYIF_DOWN_IND,
};

enum caif_modemcmd {
	CAIF_MODEMCMD_FLOW_ON_REQ = 0,
	CAIF_MODEMCMD_FLOW_OFF_REQ = 1,
	_CAIF_MODEMCMD_PHYIF_USEFULL = 3,
	_CAIF_MODEMCMD_PHYIF_USELESS = 4
};

enum caif_direction {
	CAIF_DIR_IN = 0,
	CAIF_DIR_OUT = 1
};

struct cflayer {
	struct cflayer *up;
	struct cflayer *dn;
	struct list_head node;

	int (*receive)(struct cflayer *layr, struct cfpkt *cfpkt);

	int (*transmit) (struct cflayer *layr, struct cfpkt *cfpkt);

	void (*ctrlcmd) (struct cflayer *layr, enum caif_ctrlcmd ctrl,
			 int phyid);

	int (*modemcmd) (struct cflayer *layr, enum caif_modemcmd ctrl);

	unsigned int id;
	char name[CAIF_LAYER_NAME_SZ];
};

#define layer_set_up(layr, above) ((layr)->up = (struct cflayer *)(above))

#define layer_set_dn(layr, below) ((layr)->dn = (struct cflayer *)(below))

struct dev_info {
	void *dev;
	unsigned int id;
};

struct caif_payload_info {
	struct dev_info *dev_info;
	unsigned short hdr_len;
	unsigned short channel_id;
};

#endif	
