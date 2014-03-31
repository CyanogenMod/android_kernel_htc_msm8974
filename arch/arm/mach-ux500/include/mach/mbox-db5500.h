/*
 * Copyright (C) ST-Ericsson SA 2010
 * Author: Stefan Nilsson <stefan.xk.nilsson@stericsson.com> for ST-Ericsson.
 * Author: Martin Persson <martin.persson@stericsson.com> for ST-Ericsson.
 * License terms: GNU General Public License (GPL), version 2.
 */

#ifndef __INC_STE_MBOX_H
#define __INC_STE_MBOX_H

#define MBOX_BUF_SIZE 16
#define MBOX_NAME_SIZE 8

typedef void mbox_recv_cb_t (u32 mbox_msg, void *priv);

struct mbox {
	struct list_head list;
	struct platform_device *pdev;
	mbox_recv_cb_t *cb;
	void *client_data;
	void __iomem *virtbase_peer;
	void __iomem *virtbase_local;
	u32 buffer[MBOX_BUF_SIZE];
	char name[MBOX_NAME_SIZE];
	struct completion buffer_available;
	u8 client_blocked;
	spinlock_t lock;
	u8 write_index;
	u8 read_index;
	bool allocated;
};

struct mbox *mbox_setup(u8 mbox_id, mbox_recv_cb_t *mbox_cb, void *priv);

int mbox_send(struct mbox *mbox, u32 mbox_msg, bool block);

#endif 
