/*
 * Copyright (C) ST-Ericsson AB 2010
 * Author:	Sjur Brendeland/sjur.brandeland@stericsson.com
 * License terms: GNU General Public License (GPL) version 2
 */

#define pr_fmt(fmt) KBUILD_MODNAME ":%s(): " fmt, __func__

#include <linux/stddef.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <net/caif/caif_layer.h>
#include <net/caif/cfpkt.h>
#include <net/caif/cfserl.h>

#define container_obj(layr) ((struct cfserl *) layr)

#define CFSERL_STX 0x02
#define SERIAL_MINIUM_PACKET_SIZE 4
#define SERIAL_MAX_FRAMESIZE 4096
struct cfserl {
	struct cflayer layer;
	struct cfpkt *incomplete_frm;
	
	spinlock_t sync;
	bool usestx;
};

static int cfserl_receive(struct cflayer *layr, struct cfpkt *pkt);
static int cfserl_transmit(struct cflayer *layr, struct cfpkt *pkt);
static void cfserl_ctrlcmd(struct cflayer *layr, enum caif_ctrlcmd ctrl,
				int phyid);

struct cflayer *cfserl_create(int instance, bool use_stx)
{
	struct cfserl *this = kzalloc(sizeof(struct cfserl), GFP_ATOMIC);
	if (!this)
		return NULL;
	caif_assert(offsetof(struct cfserl, layer) == 0);
	this->layer.receive = cfserl_receive;
	this->layer.transmit = cfserl_transmit;
	this->layer.ctrlcmd = cfserl_ctrlcmd;
	this->usestx = use_stx;
	spin_lock_init(&this->sync);
	snprintf(this->layer.name, CAIF_LAYER_NAME_SZ, "ser1");
	return &this->layer;
}

static int cfserl_receive(struct cflayer *l, struct cfpkt *newpkt)
{
	struct cfserl *layr = container_obj(l);
	u16 pkt_len;
	struct cfpkt *pkt = NULL;
	struct cfpkt *tail_pkt = NULL;
	u8 tmp8;
	u16 tmp;
	u8 stx = CFSERL_STX;
	int ret;
	u16 expectlen = 0;

	caif_assert(newpkt != NULL);
	spin_lock(&layr->sync);

	if (layr->incomplete_frm != NULL) {
		layr->incomplete_frm =
		    cfpkt_append(layr->incomplete_frm, newpkt, expectlen);
		pkt = layr->incomplete_frm;
		if (pkt == NULL) {
			spin_unlock(&layr->sync);
			return -ENOMEM;
		}
	} else {
		pkt = newpkt;
	}
	layr->incomplete_frm = NULL;

	do {
		
		if (layr->usestx) {
			cfpkt_extr_head(pkt, &tmp8, 1);
			if (tmp8 != CFSERL_STX) {
				while (cfpkt_more(pkt)
				       && tmp8 != CFSERL_STX) {
					cfpkt_extr_head(pkt, &tmp8, 1);
				}
				if (!cfpkt_more(pkt)) {
					cfpkt_destroy(pkt);
					layr->incomplete_frm = NULL;
					spin_unlock(&layr->sync);
					return -EPROTO;
				}
			}
		}

		pkt_len = cfpkt_getlen(pkt);


		if (pkt_len < 2) {
			if (layr->usestx)
				cfpkt_add_head(pkt, &stx, 1);
			layr->incomplete_frm = pkt;
			spin_unlock(&layr->sync);
			return 0;
		}

		cfpkt_peek_head(pkt, &tmp, 2);
		expectlen = le16_to_cpu(tmp) + 2;
		if (expectlen < SERIAL_MINIUM_PACKET_SIZE
		    || expectlen > SERIAL_MAX_FRAMESIZE) {
			if (!layr->usestx) {
				if (pkt != NULL)
					cfpkt_destroy(pkt);
				layr->incomplete_frm = NULL;
				expectlen = 0;
				spin_unlock(&layr->sync);
				return -EPROTO;
			}
			continue;
		}

		if (pkt_len < expectlen) {
			
			if (layr->usestx)
				cfpkt_add_head(pkt, &stx, 1);
			layr->incomplete_frm = pkt;
			spin_unlock(&layr->sync);
			return 0;
		}

		if (pkt_len > expectlen)
			tail_pkt = cfpkt_split(pkt, expectlen);
		else
			tail_pkt = NULL;

		
		spin_unlock(&layr->sync);
		ret = layr->layer.up->receive(layr->layer.up, pkt);
		spin_lock(&layr->sync);
		if (ret == -EILSEQ) {
			if (layr->usestx) {
				if (tail_pkt != NULL)
					pkt = cfpkt_append(pkt, tail_pkt, 0);
				
				continue;
			} else {
				cfpkt_destroy(pkt);
				pkt = NULL;
			}
		}

		pkt = tail_pkt;

	} while (pkt != NULL);

	spin_unlock(&layr->sync);
	return 0;
}

static int cfserl_transmit(struct cflayer *layer, struct cfpkt *newpkt)
{
	struct cfserl *layr = container_obj(layer);
	u8 tmp8 = CFSERL_STX;
	if (layr->usestx)
		cfpkt_add_head(newpkt, &tmp8, 1);
	return layer->dn->transmit(layer->dn, newpkt);
}

static void cfserl_ctrlcmd(struct cflayer *layr, enum caif_ctrlcmd ctrl,
				int phyid)
{
	layr->up->ctrlcmd(layr->up, ctrl, phyid);
}
