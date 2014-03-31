/*
 * Audio crossconnecting/conferrencing (hardware level).
 *
 * Copyright 2002 by Andreas Eversberg (jolly@eversberg.eu)
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */




#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mISDNif.h>
#include <linux/mISDNdsp.h>
#include "core.h"
#include "dsp.h"



static inline int
count_list_member(struct list_head *head)
{
	int			cnt = 0;
	struct list_head	*m;

	list_for_each(m, head)
		cnt++;
	return cnt;
}

void
dsp_cmx_debug(struct dsp *dsp)
{
	struct dsp_conf	*conf;
	struct dsp_conf_member	*member;
	struct dsp		*odsp;

	printk(KERN_DEBUG "-----Current DSP\n");
	list_for_each_entry(odsp, &dsp_ilist, list) {
		printk(KERN_DEBUG "* %s hardecho=%d softecho=%d txmix=%d",
		       odsp->name, odsp->echo.hardware, odsp->echo.software,
		       odsp->tx_mix);
		if (odsp->conf)
			printk(" (Conf %d)", odsp->conf->id);
		if (dsp == odsp)
			printk(" *this*");
		printk("\n");
	}
	printk(KERN_DEBUG "-----Current Conf:\n");
	list_for_each_entry(conf, &conf_ilist, list) {
		printk(KERN_DEBUG "* Conf %d (%p)\n", conf->id, conf);
		list_for_each_entry(member, &conf->mlist, list) {
			printk(KERN_DEBUG
			       "  - member = %s (slot_tx %d, bank_tx %d, "
			       "slot_rx %d, bank_rx %d hfc_conf %d "
			       "tx_data %d rx_is_off %d)%s\n",
			       member->dsp->name, member->dsp->pcm_slot_tx,
			       member->dsp->pcm_bank_tx, member->dsp->pcm_slot_rx,
			       member->dsp->pcm_bank_rx, member->dsp->hfc_conf,
			       member->dsp->tx_data, member->dsp->rx_is_off,
			       (member->dsp == dsp) ? " *this*" : "");
		}
	}
	printk(KERN_DEBUG "-----end\n");
}

static struct dsp_conf *
dsp_cmx_search_conf(u32 id)
{
	struct dsp_conf *conf;

	if (!id) {
		printk(KERN_WARNING "%s: conference ID is 0.\n", __func__);
		return NULL;
	}

	
	list_for_each_entry(conf, &conf_ilist, list)
		if (conf->id == id)
			return conf;

	return NULL;
}


static int
dsp_cmx_add_conf_member(struct dsp *dsp, struct dsp_conf *conf)
{
	struct dsp_conf_member *member;

	if (!conf || !dsp) {
		printk(KERN_WARNING "%s: conf or dsp is 0.\n", __func__);
		return -EINVAL;
	}
	if (dsp->member) {
		printk(KERN_WARNING "%s: dsp is already member in a conf.\n",
		       __func__);
		return -EINVAL;
	}

	if (dsp->conf) {
		printk(KERN_WARNING "%s: dsp is already in a conf.\n",
		       __func__);
		return -EINVAL;
	}

	member = kzalloc(sizeof(struct dsp_conf_member), GFP_ATOMIC);
	if (!member) {
		printk(KERN_ERR "kzalloc struct dsp_conf_member failed\n");
		return -ENOMEM;
	}
	member->dsp = dsp;
	
	memset(dsp->rx_buff, dsp_silence, sizeof(dsp->rx_buff));
	dsp->rx_init = 1; 
	dsp->rx_W = 0;
	dsp->rx_R = 0;

	list_add_tail(&member->list, &conf->mlist);

	dsp->conf = conf;
	dsp->member = member;

	return 0;
}


int
dsp_cmx_del_conf_member(struct dsp *dsp)
{
	struct dsp_conf_member *member;

	if (!dsp) {
		printk(KERN_WARNING "%s: dsp is 0.\n",
		       __func__);
		return -EINVAL;
	}

	if (!dsp->conf) {
		printk(KERN_WARNING "%s: dsp is not in a conf.\n",
		       __func__);
		return -EINVAL;
	}

	if (list_empty(&dsp->conf->mlist)) {
		printk(KERN_WARNING "%s: dsp has linked an empty conf.\n",
		       __func__);
		return -EINVAL;
	}

	
	list_for_each_entry(member, &dsp->conf->mlist, list) {
		if (member->dsp == dsp) {
			list_del(&member->list);
			dsp->conf = NULL;
			dsp->member = NULL;
			kfree(member);
			return 0;
		}
	}
	printk(KERN_WARNING
	       "%s: dsp is not present in its own conf_meber list.\n",
	       __func__);

	return -EINVAL;
}


static struct dsp_conf
*dsp_cmx_new_conf(u32 id)
{
	struct dsp_conf *conf;

	if (!id) {
		printk(KERN_WARNING "%s: id is 0.\n",
		       __func__);
		return NULL;
	}

	conf = kzalloc(sizeof(struct dsp_conf), GFP_ATOMIC);
	if (!conf) {
		printk(KERN_ERR "kzalloc struct dsp_conf failed\n");
		return NULL;
	}
	INIT_LIST_HEAD(&conf->mlist);
	conf->id = id;

	list_add_tail(&conf->list, &conf_ilist);

	return conf;
}


int
dsp_cmx_del_conf(struct dsp_conf *conf)
{
	if (!conf) {
		printk(KERN_WARNING "%s: conf is null.\n",
		       __func__);
		return -EINVAL;
	}

	if (!list_empty(&conf->mlist)) {
		printk(KERN_WARNING "%s: conf not empty.\n",
		       __func__);
		return -EINVAL;
	}
	list_del(&conf->list);
	kfree(conf);

	return 0;
}


static void
dsp_cmx_hw_message(struct dsp *dsp, u32 message, u32 param1, u32 param2,
		   u32 param3, u32 param4)
{
	struct mISDN_ctrl_req cq;

	memset(&cq, 0, sizeof(cq));
	cq.op = message;
	cq.p1 = param1 | (param2 << 8);
	cq.p2 = param3 | (param4 << 8);
	if (dsp->ch.peer)
		dsp->ch.peer->ctrl(dsp->ch.peer, CONTROL_CHANNEL, &cq);
}


void
dsp_cmx_hardware(struct dsp_conf *conf, struct dsp *dsp)
{
	struct dsp_conf_member	*member, *nextm;
	struct dsp		*finddsp;
	int		memb = 0, i, ii, i1, i2;
	int		freeunits[8];
	u_char		freeslots[256];
	int		same_hfc = -1, same_pcm = -1, current_conf = -1,
		all_conf = 1, tx_data = 0;

	
	if (!conf) {
		if (!dsp)
			return;
		if (dsp_debug & DEBUG_DSP_CMX)
			printk(KERN_DEBUG "%s checking dsp %s\n",
			       __func__, dsp->name);
	one_member:
		
		if (dsp->hfc_conf >= 0) {
			if (dsp_debug & DEBUG_DSP_CMX)
				printk(KERN_DEBUG
				       "%s removing %s from HFC conf %d "
				       "because dsp is split\n", __func__,
				       dsp->name, dsp->hfc_conf);
			dsp_cmx_hw_message(dsp, MISDN_CTRL_HFC_CONF_SPLIT,
					   0, 0, 0, 0);
			dsp->hfc_conf = -1;
		}
		
		if (dsp->features.pcm_banks < 1)
			return;
		if (!dsp->echo.software && !dsp->echo.hardware) {
			
			if (dsp->pcm_slot_tx >= 0 || dsp->pcm_slot_rx >= 0) {
				if (dsp_debug & DEBUG_DSP_CMX)
					printk(KERN_DEBUG "%s removing %s from"
					       " PCM slot %d (TX) %d (RX) because"
					       " dsp is split (no echo)\n",
					       __func__, dsp->name,
					       dsp->pcm_slot_tx, dsp->pcm_slot_rx);
				dsp_cmx_hw_message(dsp, MISDN_CTRL_HFC_PCM_DISC,
						   0, 0, 0, 0);
				dsp->pcm_slot_tx = -1;
				dsp->pcm_bank_tx = -1;
				dsp->pcm_slot_rx = -1;
				dsp->pcm_bank_rx = -1;
			}
			return;
		}
		
		dsp->echo.software = dsp->tx_data;
		dsp->echo.hardware = 0;
		
		if (dsp->pcm_slot_tx >= 0 && dsp->pcm_slot_rx < 0 &&
		    dsp->pcm_bank_tx == 2 && dsp->pcm_bank_rx == 2) {
			dsp->echo.hardware = 1;
			return;
		}
		
		if (dsp->pcm_slot_tx >= 0) {
			dsp->pcm_slot_rx = dsp->pcm_slot_tx;
			dsp->pcm_bank_tx = 2; 
			dsp->pcm_bank_rx = 2;
			if (dsp_debug & DEBUG_DSP_CMX)
				printk(KERN_DEBUG
				       "%s refresh %s for echo using slot %d\n",
				       __func__, dsp->name,
				       dsp->pcm_slot_tx);
			dsp_cmx_hw_message(dsp, MISDN_CTRL_HFC_PCM_CONN,
					   dsp->pcm_slot_tx, 2, dsp->pcm_slot_rx, 2);
			dsp->echo.hardware = 1;
			return;
		}
		
		dsp->pcm_slot_tx = -1;
		dsp->pcm_slot_rx = -1;
		memset(freeslots, 1, sizeof(freeslots));
		list_for_each_entry(finddsp, &dsp_ilist, list) {
			if (finddsp->features.pcm_id == dsp->features.pcm_id) {
				if (finddsp->pcm_slot_rx >= 0 &&
				    finddsp->pcm_slot_rx < sizeof(freeslots))
					freeslots[finddsp->pcm_slot_rx] = 0;
				if (finddsp->pcm_slot_tx >= 0 &&
				    finddsp->pcm_slot_tx < sizeof(freeslots))
					freeslots[finddsp->pcm_slot_tx] = 0;
			}
		}
		i = 0;
		ii = dsp->features.pcm_slots;
		while (i < ii) {
			if (freeslots[i])
				break;
			i++;
		}
		if (i == ii) {
			if (dsp_debug & DEBUG_DSP_CMX)
				printk(KERN_DEBUG
				       "%s no slot available for echo\n",
				       __func__);
			
			dsp->echo.software = 1;
			return;
		}
		
		dsp->pcm_slot_tx = i;
		dsp->pcm_slot_rx = i;
		dsp->pcm_bank_tx = 2; 
		dsp->pcm_bank_rx = 2;
		if (dsp_debug & DEBUG_DSP_CMX)
			printk(KERN_DEBUG
			       "%s assign echo for %s using slot %d\n",
			       __func__, dsp->name, dsp->pcm_slot_tx);
		dsp_cmx_hw_message(dsp, MISDN_CTRL_HFC_PCM_CONN,
				   dsp->pcm_slot_tx, 2, dsp->pcm_slot_rx, 2);
		dsp->echo.hardware = 1;
		return;
	}

	
	if (dsp_debug & DEBUG_DSP_CMX)
		printk(KERN_DEBUG "%s checking conference %d\n",
		       __func__, conf->id);

	if (list_empty(&conf->mlist)) {
		printk(KERN_ERR "%s: conference whithout members\n",
		       __func__);
		return;
	}
	member = list_entry(conf->mlist.next, struct dsp_conf_member, list);
	same_hfc = member->dsp->features.hfc_id;
	same_pcm = member->dsp->features.pcm_id;
	
	list_for_each_entry(member, &conf->mlist, list) {
		
		if (member->dsp->tx_mix) {
			if (dsp_debug & DEBUG_DSP_CMX)
				printk(KERN_DEBUG
				       "%s dsp %s cannot form a conf, because "
				       "tx_mix is turned on\n", __func__,
				       member->dsp->name);
		conf_software:
			list_for_each_entry(member, &conf->mlist, list) {
				dsp = member->dsp;
				
				if (dsp->hfc_conf >= 0) {
					if (dsp_debug & DEBUG_DSP_CMX)
						printk(KERN_DEBUG
						       "%s removing %s from HFC "
						       "conf %d because not "
						       "possible with hardware\n",
						       __func__,
						       dsp->name,
						       dsp->hfc_conf);
					dsp_cmx_hw_message(dsp,
							   MISDN_CTRL_HFC_CONF_SPLIT,
							   0, 0, 0, 0);
					dsp->hfc_conf = -1;
				}
				
				if (dsp->pcm_slot_tx >= 0 ||
				    dsp->pcm_slot_rx >= 0) {
					if (dsp_debug & DEBUG_DSP_CMX)
						printk(KERN_DEBUG "%s removing "
						       "%s from PCM slot %d (TX)"
						       " slot %d (RX) because not"
						       " possible with hardware\n",
						       __func__,
						       dsp->name,
						       dsp->pcm_slot_tx,
						       dsp->pcm_slot_rx);
					dsp_cmx_hw_message(dsp,
							   MISDN_CTRL_HFC_PCM_DISC,
							   0, 0, 0, 0);
					dsp->pcm_slot_tx = -1;
					dsp->pcm_bank_tx = -1;
					dsp->pcm_slot_rx = -1;
					dsp->pcm_bank_rx = -1;
				}
			}
			conf->hardware = 0;
			conf->software = 1;
			return;
		}
		
		if (member->dsp->echo.hardware || member->dsp->echo.software) {
			if (dsp_debug & DEBUG_DSP_CMX)
				printk(KERN_DEBUG
				       "%s dsp %s cannot form a conf, because "
				       "echo is turned on\n", __func__,
				       member->dsp->name);
			goto conf_software;
		}
		
		if (member->dsp->tx_mix) {
			if (dsp_debug & DEBUG_DSP_CMX)
				printk(KERN_DEBUG
				       "%s dsp %s cannot form a conf, because "
				       "tx_mix is turned on\n",
				       __func__, member->dsp->name);
			goto conf_software;
		}
		
		if (member->dsp->tx_volume) {
			if (dsp_debug & DEBUG_DSP_CMX)
				printk(KERN_DEBUG
				       "%s dsp %s cannot form a conf, because "
				       "tx_volume is changed\n",
				       __func__, member->dsp->name);
			goto conf_software;
		}
		if (member->dsp->rx_volume) {
			if (dsp_debug & DEBUG_DSP_CMX)
				printk(KERN_DEBUG
				       "%s dsp %s cannot form a conf, because "
				       "rx_volume is changed\n",
				       __func__, member->dsp->name);
			goto conf_software;
		}
		
		if (member->dsp->tx_data) {
			if (dsp_debug & DEBUG_DSP_CMX)
				printk(KERN_DEBUG
				       "%s dsp %s tx_data is turned on\n",
				       __func__, member->dsp->name);
			tx_data = 1;
		}
		
		if (member->dsp->pipeline.inuse) {
			if (dsp_debug & DEBUG_DSP_CMX)
				printk(KERN_DEBUG
				       "%s dsp %s cannot form a conf, because "
				       "pipeline exists\n", __func__,
				       member->dsp->name);
			goto conf_software;
		}
		
		if (member->dsp->bf_enable) {
			if (dsp_debug & DEBUG_DSP_CMX)
				printk(KERN_DEBUG "%s dsp %s cannot form a "
				       "conf, because encryption is enabled\n",
				       __func__, member->dsp->name);
			goto conf_software;
		}
		
		if (member->dsp->features.pcm_id < 0) {
			if (dsp_debug & DEBUG_DSP_CMX)
				printk(KERN_DEBUG
				       "%s dsp %s cannot form a conf, because "
				       "dsp has no PCM bus\n",
				       __func__, member->dsp->name);
			goto conf_software;
		}
		
		if (member->dsp->features.pcm_id != same_pcm) {
			if (dsp_debug & DEBUG_DSP_CMX)
				printk(KERN_DEBUG
				       "%s dsp %s cannot form a conf, because "
				       "dsp is on a different PCM bus than the "
				       "first dsp\n",
				       __func__, member->dsp->name);
			goto conf_software;
		}
		
		if (same_hfc != member->dsp->features.hfc_id)
			same_hfc = -1;
		
		if (current_conf < 0 && member->dsp->hfc_conf >= 0)
			current_conf = member->dsp->hfc_conf;
		
		if (member->dsp->hfc_conf < 0)
			all_conf = 0;

		memb++;
	}

	
	if (memb < 1)
		return;

	
	if (memb == 1) {
		if (dsp_debug & DEBUG_DSP_CMX)
			printk(KERN_DEBUG
			       "%s conf %d cannot form a HW conference, "
			       "because dsp is alone\n", __func__, conf->id);
		conf->hardware = 0;
		conf->software = 0;
		member = list_entry(conf->mlist.next, struct dsp_conf_member,
				    list);
		dsp = member->dsp;
		goto one_member;
	}


	
	if (memb == 2) {
		member = list_entry(conf->mlist.next, struct dsp_conf_member,
				    list);
		nextm = list_entry(member->list.next, struct dsp_conf_member,
				   list);
		
		if (member->dsp->hfc_conf >= 0) {
			if (dsp_debug & DEBUG_DSP_CMX)
				printk(KERN_DEBUG
				       "%s removing %s from HFC conf %d because "
				       "two parties require only a PCM slot\n",
				       __func__, member->dsp->name,
				       member->dsp->hfc_conf);
			dsp_cmx_hw_message(member->dsp,
					   MISDN_CTRL_HFC_CONF_SPLIT, 0, 0, 0, 0);
			member->dsp->hfc_conf = -1;
		}
		if (nextm->dsp->hfc_conf >= 0) {
			if (dsp_debug & DEBUG_DSP_CMX)
				printk(KERN_DEBUG
				       "%s removing %s from HFC conf %d because "
				       "two parties require only a PCM slot\n",
				       __func__, nextm->dsp->name,
				       nextm->dsp->hfc_conf);
			dsp_cmx_hw_message(nextm->dsp,
					   MISDN_CTRL_HFC_CONF_SPLIT, 0, 0, 0, 0);
			nextm->dsp->hfc_conf = -1;
		}
		
		if (member->dsp->features.pcm_banks > 1 &&
		    nextm->dsp->features.pcm_banks > 1 &&
		    member->dsp->features.hfc_id !=
		    nextm->dsp->features.hfc_id) {
			
			if (member->dsp->pcm_slot_tx >= 0 &&
			    member->dsp->pcm_slot_rx >= 0 &&
			    nextm->dsp->pcm_slot_tx >= 0 &&
			    nextm->dsp->pcm_slot_rx >= 0 &&
			    nextm->dsp->pcm_slot_tx ==
			    member->dsp->pcm_slot_rx &&
			    nextm->dsp->pcm_slot_rx ==
			    member->dsp->pcm_slot_tx &&
			    nextm->dsp->pcm_slot_tx ==
			    member->dsp->pcm_slot_tx &&
			    member->dsp->pcm_bank_tx !=
			    member->dsp->pcm_bank_rx &&
			    nextm->dsp->pcm_bank_tx !=
			    nextm->dsp->pcm_bank_rx) {
				
				if (dsp_debug & DEBUG_DSP_CMX)
					printk(KERN_DEBUG
					       "%s dsp %s & %s stay joined on "
					       "PCM slot %d bank %d (TX) bank %d "
					       "(RX) (on different chips)\n",
					       __func__,
					       member->dsp->name,
					       nextm->dsp->name,
					       member->dsp->pcm_slot_tx,
					       member->dsp->pcm_bank_tx,
					       member->dsp->pcm_bank_rx);
				conf->hardware = 0;
				conf->software = 1;
				return;
			}
			
			memset(freeslots, 1, sizeof(freeslots));
			list_for_each_entry(dsp, &dsp_ilist, list) {
				if (dsp != member->dsp &&
				    dsp != nextm->dsp &&
				    member->dsp->features.pcm_id ==
				    dsp->features.pcm_id) {
					if (dsp->pcm_slot_rx >= 0 &&
					    dsp->pcm_slot_rx <
					    sizeof(freeslots))
						freeslots[dsp->pcm_slot_rx] = 0;
					if (dsp->pcm_slot_tx >= 0 &&
					    dsp->pcm_slot_tx <
					    sizeof(freeslots))
						freeslots[dsp->pcm_slot_tx] = 0;
				}
			}
			i = 0;
			ii = member->dsp->features.pcm_slots;
			while (i < ii) {
				if (freeslots[i])
					break;
				i++;
			}
			if (i == ii) {
				if (dsp_debug & DEBUG_DSP_CMX)
					printk(KERN_DEBUG
					       "%s no slot available for "
					       "%s & %s\n", __func__,
					       member->dsp->name,
					       nextm->dsp->name);
				
				goto conf_software;
			}
			
			member->dsp->pcm_slot_tx = i;
			member->dsp->pcm_slot_rx = i;
			nextm->dsp->pcm_slot_tx = i;
			nextm->dsp->pcm_slot_rx = i;
			member->dsp->pcm_bank_rx = 0;
			member->dsp->pcm_bank_tx = 1;
			nextm->dsp->pcm_bank_rx = 1;
			nextm->dsp->pcm_bank_tx = 0;
			if (dsp_debug & DEBUG_DSP_CMX)
				printk(KERN_DEBUG
				       "%s adding %s & %s to new PCM slot %d "
				       "(TX and RX on different chips) because "
				       "both members have not same slots\n",
				       __func__,
				       member->dsp->name,
				       nextm->dsp->name,
				       member->dsp->pcm_slot_tx);
			dsp_cmx_hw_message(member->dsp, MISDN_CTRL_HFC_PCM_CONN,
					   member->dsp->pcm_slot_tx, member->dsp->pcm_bank_tx,
					   member->dsp->pcm_slot_rx, member->dsp->pcm_bank_rx);
			dsp_cmx_hw_message(nextm->dsp, MISDN_CTRL_HFC_PCM_CONN,
					   nextm->dsp->pcm_slot_tx, nextm->dsp->pcm_bank_tx,
					   nextm->dsp->pcm_slot_rx, nextm->dsp->pcm_bank_rx);
			conf->hardware = 1;
			conf->software = tx_data;
			return;
			
		} else {
			
			if (member->dsp->pcm_slot_tx >= 0 &&
			    member->dsp->pcm_slot_rx >= 0 &&
			    nextm->dsp->pcm_slot_tx >= 0 &&
			    nextm->dsp->pcm_slot_rx >= 0 &&
			    nextm->dsp->pcm_slot_tx ==
			    member->dsp->pcm_slot_rx &&
			    nextm->dsp->pcm_slot_rx ==
			    member->dsp->pcm_slot_tx &&
			    member->dsp->pcm_slot_tx !=
			    member->dsp->pcm_slot_rx &&
			    member->dsp->pcm_bank_tx == 0 &&
			    member->dsp->pcm_bank_rx == 0 &&
			    nextm->dsp->pcm_bank_tx == 0 &&
			    nextm->dsp->pcm_bank_rx == 0) {
				
				if (dsp_debug & DEBUG_DSP_CMX)
					printk(KERN_DEBUG
					       "%s dsp %s & %s stay joined on PCM "
					       "slot %d (TX) %d (RX) on same chip "
					       "or one bank PCM)\n", __func__,
					       member->dsp->name,
					       nextm->dsp->name,
					       member->dsp->pcm_slot_tx,
					       member->dsp->pcm_slot_rx);
				conf->hardware = 0;
				conf->software = 1;
				return;
			}
			
			memset(freeslots, 1, sizeof(freeslots));
			list_for_each_entry(dsp, &dsp_ilist, list) {
				if (dsp != member->dsp &&
				    dsp != nextm->dsp &&
				    member->dsp->features.pcm_id ==
				    dsp->features.pcm_id) {
					if (dsp->pcm_slot_rx >= 0 &&
					    dsp->pcm_slot_rx <
					    sizeof(freeslots))
						freeslots[dsp->pcm_slot_rx] = 0;
					if (dsp->pcm_slot_tx >= 0 &&
					    dsp->pcm_slot_tx <
					    sizeof(freeslots))
						freeslots[dsp->pcm_slot_tx] = 0;
				}
			}
			i1 = 0;
			ii = member->dsp->features.pcm_slots;
			while (i1 < ii) {
				if (freeslots[i1])
					break;
				i1++;
			}
			if (i1 == ii) {
				if (dsp_debug & DEBUG_DSP_CMX)
					printk(KERN_DEBUG
					       "%s no slot available "
					       "for %s & %s\n", __func__,
					       member->dsp->name,
					       nextm->dsp->name);
				
				goto conf_software;
			}
			i2 = i1 + 1;
			while (i2 < ii) {
				if (freeslots[i2])
					break;
				i2++;
			}
			if (i2 == ii) {
				if (dsp_debug & DEBUG_DSP_CMX)
					printk(KERN_DEBUG
					       "%s no slot available "
					       "for %s & %s\n",
					       __func__,
					       member->dsp->name,
					       nextm->dsp->name);
				
				goto conf_software;
			}
			
			member->dsp->pcm_slot_tx = i1;
			member->dsp->pcm_slot_rx = i2;
			nextm->dsp->pcm_slot_tx = i2;
			nextm->dsp->pcm_slot_rx = i1;
			member->dsp->pcm_bank_rx = 0;
			member->dsp->pcm_bank_tx = 0;
			nextm->dsp->pcm_bank_rx = 0;
			nextm->dsp->pcm_bank_tx = 0;
			if (dsp_debug & DEBUG_DSP_CMX)
				printk(KERN_DEBUG
				       "%s adding %s & %s to new PCM slot %d "
				       "(TX) %d (RX) on same chip or one bank "
				       "PCM, because both members have not "
				       "crossed slots\n", __func__,
				       member->dsp->name,
				       nextm->dsp->name,
				       member->dsp->pcm_slot_tx,
				       member->dsp->pcm_slot_rx);
			dsp_cmx_hw_message(member->dsp, MISDN_CTRL_HFC_PCM_CONN,
					   member->dsp->pcm_slot_tx, member->dsp->pcm_bank_tx,
					   member->dsp->pcm_slot_rx, member->dsp->pcm_bank_rx);
			dsp_cmx_hw_message(nextm->dsp, MISDN_CTRL_HFC_PCM_CONN,
					   nextm->dsp->pcm_slot_tx, nextm->dsp->pcm_bank_tx,
					   nextm->dsp->pcm_slot_rx, nextm->dsp->pcm_bank_rx);
			conf->hardware = 1;
			conf->software = tx_data;
			return;
		}
	}


	
	if (same_hfc < 0) {
		if (dsp_debug & DEBUG_DSP_CMX)
			printk(KERN_DEBUG
			       "%s conference %d cannot be formed, because "
			       "members are on different chips or not "
			       "on HFC chip\n",
			       __func__, conf->id);
		goto conf_software;
	}

	

	
	if (all_conf)
		return;

	if (current_conf >= 0) {
	join_members:
		list_for_each_entry(member, &conf->mlist, list) {
			if (!member->dsp->features.hfc_conf)
				goto conf_software;
			
			if (member->dsp->hdlc)
				goto conf_software;
			
			if (member->dsp->hfc_conf == current_conf)
				continue;
			
			memset(freeslots, 1, sizeof(freeslots));
			list_for_each_entry(dsp, &dsp_ilist, list) {
				/*
				 * not checking current member, because
				 * slot will be overwritten.
				 */
				if (
					dsp != member->dsp &&
					
					member->dsp->features.pcm_id ==
					dsp->features.pcm_id) {
					
					if (dsp->pcm_slot_tx >= 0 &&
					    dsp->pcm_slot_tx <
					    sizeof(freeslots))
						freeslots[dsp->pcm_slot_tx] = 0;
					if (dsp->pcm_slot_rx >= 0 &&
					    dsp->pcm_slot_rx <
					    sizeof(freeslots))
						freeslots[dsp->pcm_slot_rx] = 0;
				}
			}
			i = 0;
			ii = member->dsp->features.pcm_slots;
			while (i < ii) {
				if (freeslots[i])
					break;
				i++;
			}
			if (i == ii) {
				
				if (dsp_debug & DEBUG_DSP_CMX)
					printk(KERN_DEBUG
					       "%s conference %d cannot be formed,"
					       " because no slot free\n",
					       __func__, conf->id);
				goto conf_software;
			}
			if (dsp_debug & DEBUG_DSP_CMX)
				printk(KERN_DEBUG
				       "%s changing dsp %s to HW conference "
				       "%d slot %d\n", __func__,
				       member->dsp->name, current_conf, i);
			
			member->dsp->pcm_slot_tx = i;
			member->dsp->pcm_slot_rx = i;
			member->dsp->pcm_bank_tx = 2; 
			member->dsp->pcm_bank_rx = 2;
			member->dsp->hfc_conf = current_conf;
			dsp_cmx_hw_message(member->dsp, MISDN_CTRL_HFC_PCM_CONN,
					   i, 2, i, 2);
			dsp_cmx_hw_message(member->dsp,
					   MISDN_CTRL_HFC_CONF_JOIN, current_conf, 0, 0, 0);
		}
		return;
	}

	memset(freeunits, 1, sizeof(freeunits));
	list_for_each_entry(dsp, &dsp_ilist, list) {
		
		if (dsp->features.hfc_id == same_hfc &&
		    
		    dsp->hfc_conf >= 0 &&
		    
		    dsp->hfc_conf < 8)
			freeunits[dsp->hfc_conf] = 0;
	}
	i = 0;
	ii = 8;
	while (i < ii) {
		if (freeunits[i])
			break;
		i++;
	}
	if (i == ii) {
		
		if (dsp_debug & DEBUG_DSP_CMX)
			printk(KERN_DEBUG
			       "%s conference %d cannot be formed, because "
			       "no conference number free\n",
			       __func__, conf->id);
		goto conf_software;
	}
	
	current_conf = i;
	goto join_members;
}


int
dsp_cmx_conf(struct dsp *dsp, u32 conf_id)
{
	int err;
	struct dsp_conf *conf;
	struct dsp_conf_member	*member;

	
	if (dsp->conf_id == conf_id)
		return 0;

	
	if (dsp->conf_id) {
		if (dsp_debug & DEBUG_DSP_CMX)
			printk(KERN_DEBUG "removing us from conference %d\n",
			       dsp->conf->id);
		
		conf = dsp->conf;
		err = dsp_cmx_del_conf_member(dsp);
		if (err)
			return err;
		dsp->conf_id = 0;

		
		dsp_cmx_hardware(NULL, dsp);

		
		if (list_empty(&conf->mlist)) {
			if (dsp_debug & DEBUG_DSP_CMX)
				printk(KERN_DEBUG
				       "conference is empty, so we remove it.\n");
			err = dsp_cmx_del_conf(conf);
			if (err)
				return err;
		} else {
			
			dsp_cmx_hardware(conf, NULL);
		}
	}

	
	if (!conf_id)
		return 0;

	
	if (dsp_debug & DEBUG_DSP_CMX)
		printk(KERN_DEBUG "searching conference %d\n",
		       conf_id);
	conf = dsp_cmx_search_conf(conf_id);
	if (!conf) {
		if (dsp_debug & DEBUG_DSP_CMX)
			printk(KERN_DEBUG
			       "conference doesn't exist yet, creating.\n");
		
		conf = dsp_cmx_new_conf(conf_id);
		if (!conf)
			return -EINVAL;
	} else if (!list_empty(&conf->mlist)) {
		member = list_entry(conf->mlist.next, struct dsp_conf_member,
				    list);
		if (dsp->hdlc && !member->dsp->hdlc) {
			if (dsp_debug & DEBUG_DSP_CMX)
				printk(KERN_DEBUG
				       "cannot join transparent conference.\n");
			return -EINVAL;
		}
		if (!dsp->hdlc && member->dsp->hdlc) {
			if (dsp_debug & DEBUG_DSP_CMX)
				printk(KERN_DEBUG
				       "cannot join hdlc conference.\n");
			return -EINVAL;
		}
	}
	
	err = dsp_cmx_add_conf_member(dsp, conf);
	if (err)
		return err;
	dsp->conf_id = conf_id;

	
	if (list_empty(&conf->mlist)) {
		if (dsp_debug & DEBUG_DSP_CMX)
			printk(KERN_DEBUG
			       "we are alone in this conference, so exit.\n");
		
		dsp_cmx_hardware(NULL, dsp);
		return 0;
	}

	
	dsp_cmx_hardware(conf, NULL);

	return 0;
}

#ifdef CMX_DELAY_DEBUG
int delaycount;
static void
showdelay(struct dsp *dsp, int samples, int delay)
{
	char bar[] = "--------------------------------------------------|";
	int sdelay;

	delaycount += samples;
	if (delaycount < 8000)
		return;
	delaycount = 0;

	sdelay = delay * 50 / (dsp_poll << 2);

	printk(KERN_DEBUG "DELAY (%s) %3d >%s\n", dsp->name, delay,
	       sdelay > 50 ? "..." : bar + 50 - sdelay);
}
#endif

void
dsp_cmx_receive(struct dsp *dsp, struct sk_buff *skb)
{
	u8 *d, *p;
	int len = skb->len;
	struct mISDNhead *hh = mISDN_HEAD_P(skb);
	int w, i, ii;

	
	if (len < 1)
		return;

	
	if (len >= CMX_BUFF_HALF) {
		printk(KERN_ERR
		       "%s line %d: packet from card is too large (%d bytes). "
		       "please make card send smaller packets OR increase "
		       "CMX_BUFF_SIZE\n", __FILE__, __LINE__, len);
		return;
	}

	if (dsp->rx_init) {
		dsp->rx_init = 0;
		if (dsp->features.unordered) {
			dsp->rx_R = (hh->id & CMX_BUFF_MASK);
			if (dsp->cmx_delay)
				dsp->rx_W = (dsp->rx_R + dsp->cmx_delay)
					& CMX_BUFF_MASK;
			else
				dsp->rx_W = (dsp->rx_R + (dsp_poll >> 1))
					& CMX_BUFF_MASK;
		} else {
			dsp->rx_R = 0;
			if (dsp->cmx_delay)
				dsp->rx_W = dsp->cmx_delay;
			else
				dsp->rx_W = dsp_poll >> 1;
		}
	}
	
	if (dsp->features.unordered) {
		dsp->rx_W = (hh->id & CMX_BUFF_MASK);
		
	}
	if (((dsp->rx_W-dsp->rx_R) & CMX_BUFF_MASK) >= CMX_BUFF_HALF) {
		if (dsp_debug & DEBUG_DSP_CLOCK)
			printk(KERN_DEBUG
			       "cmx_receive(dsp=%lx): UNDERRUN (or overrun the "
			       "maximum delay), adjusting read pointer! "
			       "(inst %s)\n", (u_long)dsp, dsp->name);
		
		if (dsp->features.unordered) {
			dsp->rx_R = (hh->id & CMX_BUFF_MASK);
			if (dsp->cmx_delay)
				dsp->rx_W = (dsp->rx_R + dsp->cmx_delay)
					& CMX_BUFF_MASK;
			else
				dsp->rx_W = (dsp->rx_R + (dsp_poll >> 1))
					& CMX_BUFF_MASK;
		} else {
			dsp->rx_R = 0;
			if (dsp->cmx_delay)
				dsp->rx_W = dsp->cmx_delay;
			else
				dsp->rx_W = dsp_poll >> 1;
		}
		memset(dsp->rx_buff, dsp_silence, sizeof(dsp->rx_buff));
	}
	
	if (dsp->cmx_delay)
		if (((dsp->rx_W - dsp->rx_R) & CMX_BUFF_MASK) >=
		    (dsp->cmx_delay << 1)) {
			if (dsp_debug & DEBUG_DSP_CLOCK)
				printk(KERN_DEBUG
				       "cmx_receive(dsp=%lx): OVERRUN (because "
				       "twice the delay is reached), adjusting "
				       "read pointer! (inst %s)\n",
				       (u_long)dsp, dsp->name);
			
			if (dsp->features.unordered) {
				dsp->rx_R = (hh->id & CMX_BUFF_MASK);
				dsp->rx_W = (dsp->rx_R + dsp->cmx_delay)
					& CMX_BUFF_MASK;
			} else {
				dsp->rx_R = 0;
				dsp->rx_W = dsp->cmx_delay;
			}
			memset(dsp->rx_buff, dsp_silence, sizeof(dsp->rx_buff));
		}

	
#ifdef CMX_DEBUG
	printk(KERN_DEBUG
	       "cmx_receive(dsp=%lx): rx_R(dsp)=%05x rx_W(dsp)=%05x len=%d %s\n",
	       (u_long)dsp, dsp->rx_R, dsp->rx_W, len, dsp->name);
#endif

	
	p = skb->data;
	d = dsp->rx_buff;
	w = dsp->rx_W;
	i = 0;
	ii = len;
	while (i < ii) {
		d[w++ & CMX_BUFF_MASK] = *p++;
		i++;
	}

	
	dsp->rx_W = ((dsp->rx_W + len) & CMX_BUFF_MASK);
#ifdef CMX_DELAY_DEBUG
	showdelay(dsp, len, (dsp->rx_W-dsp->rx_R) & CMX_BUFF_MASK);
#endif
}


static void
dsp_cmx_send_member(struct dsp *dsp, int len, s32 *c, int members)
{
	struct dsp_conf *conf = dsp->conf;
	struct dsp *member, *other;
	register s32 sample;
	u8 *d, *p, *q, *o_q;
	struct sk_buff *nskb, *txskb;
	int r, rr, t, tt, o_r, o_rr;
	int preload = 0;
	struct mISDNhead *hh, *thh;
	int tx_data_only = 0;

	
	if (!dsp->b_active) { 
		dsp->last_tx = 0;
		return;
	}
	if (((dsp->conf && dsp->conf->hardware) || 
	     dsp->echo.hardware) && 
	    dsp->tx_R == dsp->tx_W && 
	    !(dsp->tone.tone && dsp->tone.software)) { 
		if (!dsp->tx_data) { 
			dsp->last_tx = 0;
			return;
		}
		if (dsp->conf && dsp->conf->software && dsp->conf->hardware)
			tx_data_only = 1;
		if (dsp->conf->software && dsp->echo.hardware)
			tx_data_only = 1;
	}

#ifdef CMX_DEBUG
	printk(KERN_DEBUG
	       "SEND members=%d dsp=%s, conf=%p, rx_R=%05x rx_W=%05x\n",
	       members, dsp->name, conf, dsp->rx_R, dsp->rx_W);
#endif

	
	if (dsp->cmx_delay && !dsp->last_tx) {
		preload = len;
		if (preload < 128)
			preload = 128;
	}

	
	nskb = mI_alloc_skb(len + preload, GFP_ATOMIC);
	if (!nskb) {
		printk(KERN_ERR
		       "FATAL ERROR in mISDN_dsp.o: cannot alloc %d bytes\n",
		       len + preload);
		return;
	}
	hh = mISDN_HEAD_P(nskb);
	hh->prim = PH_DATA_REQ;
	hh->id = 0;
	dsp->last_tx = 1;

	
	member = dsp;
	p = dsp->tx_buff; 
	q = dsp->rx_buff; 
	d = skb_put(nskb, preload + len); 
	t = dsp->tx_R; 
	tt = dsp->tx_W;
	r = dsp->rx_R; 
	rr = (r + len) & CMX_BUFF_MASK;

	
	if (preload) {
		memset(d, dsp_silence, preload);
		d += preload;
	}

	
	if (dsp->tone.tone && dsp->tone.software) {
		
		dsp_tone_copy(dsp, d, len);
		dsp->tx_R = 0; 
		dsp->tx_W = 0;
		goto send_packet;
	}
	
	if (!dsp->tx_mix && t != tt) {
		
#ifdef CMX_TX_DEBUG
		sprintf(debugbuf, "TX sending (%04x-%04x)%p: ", t, tt, p);
#endif
		while (r != rr && t != tt) {
#ifdef CMX_TX_DEBUG
			if (strlen(debugbuf) < 48)
				sprintf(debugbuf + strlen(debugbuf), " %02x",
					p[t]);
#endif
			*d++ = p[t]; 
			t = (t + 1) & CMX_BUFF_MASK;
			r = (r + 1) & CMX_BUFF_MASK;
		}
		if (r == rr) {
			dsp->tx_R = t;
#ifdef CMX_TX_DEBUG
			printk(KERN_DEBUG "%s\n", debugbuf);
#endif
			goto send_packet;
		}
	}
#ifdef CMX_TX_DEBUG
	printk(KERN_DEBUG "%s\n", debugbuf);
#endif

	
	if (!conf || members <= 1) {
		
		if (!dsp->echo.software) {
			
			while (r != rr && t != tt) {
				*d++ = p[t]; 
				t = (t + 1) & CMX_BUFF_MASK;
				r = (r + 1) & CMX_BUFF_MASK;
			}
			if (r != rr) {
				if (dsp_debug & DEBUG_DSP_CLOCK)
					printk(KERN_DEBUG "%s: RX empty\n",
					       __func__);
				memset(d, dsp_silence, (rr - r) & CMX_BUFF_MASK);
			}
			
		} else {
			while (r != rr && t != tt) {
				*d++ = dsp_audio_mix_law[(p[t] << 8) | q[r]];
				t = (t + 1) & CMX_BUFF_MASK;
				r = (r + 1) & CMX_BUFF_MASK;
			}
			while (r != rr) {
				*d++ = q[r]; 
				r = (r + 1) & CMX_BUFF_MASK;
			}
		}
		dsp->tx_R = t;
		goto send_packet;
	}
	
#ifdef CMX_CONF_DEBUG
	if (0) {
#else
		if (members == 2) {
#endif
			
			other = (list_entry(conf->mlist.next,
					    struct dsp_conf_member, list))->dsp;
			if (other == member)
				other = (list_entry(conf->mlist.prev,
						    struct dsp_conf_member, list))->dsp;
			o_q = other->rx_buff; 
			o_rr = (other->rx_R + len) & CMX_BUFF_MASK;
			
			o_r = (o_rr - rr + r) & CMX_BUFF_MASK;
			
			
			if (!dsp->echo.software) {
				while (o_r != o_rr && t != tt) {
					*d++ = dsp_audio_mix_law[(p[t] << 8) | o_q[o_r]];
					t = (t + 1) & CMX_BUFF_MASK;
					o_r = (o_r + 1) & CMX_BUFF_MASK;
				}
				while (o_r != o_rr) {
					*d++ = o_q[o_r];
					o_r = (o_r + 1) & CMX_BUFF_MASK;
				}
				
			} else {
				while (r != rr && t != tt) {
					sample = dsp_audio_law_to_s32[p[t]] +
						dsp_audio_law_to_s32[q[r]] +
						dsp_audio_law_to_s32[o_q[o_r]];
					if (sample < -32768)
						sample = -32768;
					else if (sample > 32767)
						sample = 32767;
					*d++ = dsp_audio_s16_to_law[sample & 0xffff];
					
					t = (t + 1) & CMX_BUFF_MASK;
					r = (r + 1) & CMX_BUFF_MASK;
					o_r = (o_r + 1) & CMX_BUFF_MASK;
				}
				while (r != rr) {
					*d++ = dsp_audio_mix_law[(q[r] << 8) | o_q[o_r]];
					r = (r + 1) & CMX_BUFF_MASK;
					o_r = (o_r + 1) & CMX_BUFF_MASK;
				}
			}
			dsp->tx_R = t;
			goto send_packet;
		}
#ifdef DSP_NEVER_DEFINED
	}
#endif
	
	
	if (!dsp->echo.software) {
		while (r != rr && t != tt) {
			sample = dsp_audio_law_to_s32[p[t]] + *c++ -
				dsp_audio_law_to_s32[q[r]];
			if (sample < -32768)
				sample = -32768;
			else if (sample > 32767)
				sample = 32767;
			*d++ = dsp_audio_s16_to_law[sample & 0xffff];
			
			r = (r + 1) & CMX_BUFF_MASK;
			t = (t + 1) & CMX_BUFF_MASK;
		}
		while (r != rr) {
			sample = *c++ - dsp_audio_law_to_s32[q[r]];
			if (sample < -32768)
				sample = -32768;
			else if (sample > 32767)
				sample = 32767;
			*d++ = dsp_audio_s16_to_law[sample & 0xffff];
			
			r = (r + 1) & CMX_BUFF_MASK;
		}
		
	} else {
		while (r != rr && t != tt) {
			sample = dsp_audio_law_to_s32[p[t]] + *c++;
			if (sample < -32768)
				sample = -32768;
			else if (sample > 32767)
				sample = 32767;
			*d++ = dsp_audio_s16_to_law[sample & 0xffff];
			
			t = (t + 1) & CMX_BUFF_MASK;
			r = (r + 1) & CMX_BUFF_MASK;
		}
		while (r != rr) {
			sample = *c++;
			if (sample < -32768)
				sample = -32768;
			else if (sample > 32767)
				sample = 32767;
			*d++ = dsp_audio_s16_to_law[sample & 0xffff];
			
			r = (r + 1) & CMX_BUFF_MASK;
		}
	}
	dsp->tx_R = t;
	goto send_packet;

send_packet:
	if (dsp->tx_data) {
		if (tx_data_only) {
			hh->prim = DL_DATA_REQ;
			hh->id = 0;
			
			skb_queue_tail(&dsp->sendq, nskb);
			schedule_work(&dsp->workq);
			
			return;
		} else {
			txskb = mI_alloc_skb(len, GFP_ATOMIC);
			if (!txskb) {
				printk(KERN_ERR
				       "FATAL ERROR in mISDN_dsp.o: "
				       "cannot alloc %d bytes\n", len);
			} else {
				thh = mISDN_HEAD_P(txskb);
				thh->prim = DL_DATA_REQ;
				thh->id = 0;
				memcpy(skb_put(txskb, len), nskb->data + preload,
				       len);
				
				skb_queue_tail(&dsp->sendq, txskb);
			}
		}
	}

	
	
	if (dsp->tx_volume)
		dsp_change_volume(nskb, dsp->tx_volume);
	
	if (dsp->pipeline.inuse)
		dsp_pipeline_process_tx(&dsp->pipeline, nskb->data,
					nskb->len);
	
	if (dsp->bf_enable)
		dsp_bf_encrypt(dsp, nskb->data, nskb->len);
	
	skb_queue_tail(&dsp->sendq, nskb);
	schedule_work(&dsp->workq);
}

static u32	jittercount; 
struct timer_list dsp_spl_tl;
u32	dsp_spl_jiffies; 
static u16	dsp_count; 
static int	dsp_count_valid; 

void
dsp_cmx_send(void *arg)
{
	struct dsp_conf *conf;
	struct dsp_conf_member *member;
	struct dsp *dsp;
	int mustmix, members;
	static s32 mixbuffer[MAX_POLL + 100];
	s32 *c;
	u8 *p, *q;
	int r, rr;
	int jittercheck = 0, delay, i;
	u_long flags;
	u16 length, count;

	
	spin_lock_irqsave(&dsp_lock, flags);

	if (!dsp_count_valid) {
		dsp_count = mISDN_clock_get();
		length = dsp_poll;
		dsp_count_valid = 1;
	} else {
		count = mISDN_clock_get();
		length = count - dsp_count;
		dsp_count = count;
	}
	if (length > MAX_POLL + 100)
		length = MAX_POLL + 100;
	

	jittercount += length;
	if (jittercount >= 8000) {
		jittercount -= 8000;
		jittercheck = 1;
	}

	
	list_for_each_entry(dsp, &dsp_ilist, list) {
		if (dsp->hdlc)
			continue;
		conf = dsp->conf;
		mustmix = 0;
		members = 0;
		if (conf) {
			members = count_list_member(&conf->mlist);
#ifdef CMX_CONF_DEBUG
			if (conf->software && members > 1)
#else
				if (conf->software && members > 2)
#endif
					mustmix = 1;
		}

		
		if (!mustmix) {
			dsp_cmx_send_member(dsp, length, mixbuffer, members);

		}
	}

	
	list_for_each_entry(conf, &conf_ilist, list) {
		
		members = count_list_member(&conf->mlist);
#ifdef CMX_CONF_DEBUG
		if (conf->software && members > 1) {
#else
			if (conf->software && members > 2) {
#endif
				
				member = list_entry(conf->mlist.next,
						    struct dsp_conf_member, list);
				if (member->dsp->hdlc)
					continue;
				
				memset(mixbuffer, 0, length * sizeof(s32));
				list_for_each_entry(member, &conf->mlist, list) {
					dsp = member->dsp;
					
					c = mixbuffer;
					q = dsp->rx_buff;
					r = dsp->rx_R;
					rr = (r + length) & CMX_BUFF_MASK;
					
					while (r != rr) {
						*c++ += dsp_audio_law_to_s32[q[r]];
						r = (r + 1) & CMX_BUFF_MASK;
					}
				}

				
				list_for_each_entry(member, &conf->mlist, list) {
					
					dsp_cmx_send_member(member->dsp, length,
							    mixbuffer, members);
				}
			}
		}

		
		list_for_each_entry(dsp, &dsp_ilist, list) {
			if (dsp->hdlc)
				continue;
			p = dsp->rx_buff;
			q = dsp->tx_buff;
			r = dsp->rx_R;
			
			if (!dsp->rx_is_off) {
				rr = (r + length) & CMX_BUFF_MASK;
				
				while (r != rr) {
					p[r] = dsp_silence;
					r = (r + 1) & CMX_BUFF_MASK;
				}
				
				dsp->rx_R = r; 
			}

			
			delay = (dsp->rx_W-dsp->rx_R) & CMX_BUFF_MASK;
			if (delay >= CMX_BUFF_HALF)
				delay = 0; 
			
			if (delay < dsp->rx_delay[0])
				dsp->rx_delay[0] = delay;
			
			delay = (dsp->tx_W-dsp->tx_R) & CMX_BUFF_MASK;
			if (delay >= CMX_BUFF_HALF)
				delay = 0; 
			
			if (delay < dsp->tx_delay[0])
				dsp->tx_delay[0] = delay;
			if (jittercheck) {
				
				delay = dsp->rx_delay[0];
				i = 1;
				while (i < MAX_SECONDS_JITTER_CHECK) {
					if (delay > dsp->rx_delay[i])
						delay = dsp->rx_delay[i];
					i++;
				}
				if (delay > dsp_poll && !dsp->cmx_delay) {
					if (dsp_debug & DEBUG_DSP_CLOCK)
						printk(KERN_DEBUG
						       "%s lowest rx_delay of %d bytes for"
						       " dsp %s are now removed.\n",
						       __func__, delay,
						       dsp->name);
					r = dsp->rx_R;
					rr = (r + delay - (dsp_poll >> 1))
						& CMX_BUFF_MASK;
					
					while (r != rr) {
						p[r] = dsp_silence;
						r = (r + 1) & CMX_BUFF_MASK;
					}
					
					dsp->rx_R = r;
					
				}
				
				delay = dsp->tx_delay[0];
				i = 1;
				while (i < MAX_SECONDS_JITTER_CHECK) {
					if (delay > dsp->tx_delay[i])
						delay = dsp->tx_delay[i];
					i++;
				}
				if (delay > dsp_poll && dsp->tx_dejitter) {
					if (dsp_debug & DEBUG_DSP_CLOCK)
						printk(KERN_DEBUG
						       "%s lowest tx_delay of %d bytes for"
						       " dsp %s are now removed.\n",
						       __func__, delay,
						       dsp->name);
					r = dsp->tx_R;
					rr = (r + delay - (dsp_poll >> 1))
						& CMX_BUFF_MASK;
					
					while (r != rr) {
						q[r] = dsp_silence;
						r = (r + 1) & CMX_BUFF_MASK;
					}
					
					dsp->tx_R = r;
					
				}
				
				i = MAX_SECONDS_JITTER_CHECK - 1;
				while (i) {
					dsp->rx_delay[i] = dsp->rx_delay[i - 1];
					dsp->tx_delay[i] = dsp->tx_delay[i - 1];
					i--;
				}
				dsp->tx_delay[0] = CMX_BUFF_HALF; 
				dsp->rx_delay[0] = CMX_BUFF_HALF; 
			}
		}

		
		if ((s32)(dsp_spl_jiffies + dsp_tics-jiffies) <= 0)
			dsp_spl_jiffies = jiffies + 1;
		else
			dsp_spl_jiffies += dsp_tics;

		dsp_spl_tl.expires = dsp_spl_jiffies;
		add_timer(&dsp_spl_tl);

		
		spin_unlock_irqrestore(&dsp_lock, flags);
	}

	void
		dsp_cmx_transmit(struct dsp *dsp, struct sk_buff *skb)
	{
		u_int w, ww;
		u8 *d, *p;
		int space; 
#ifdef CMX_TX_DEBUG
		char debugbuf[256] = "";
#endif

		
		w = dsp->tx_W;
		ww = dsp->tx_R;
		p = dsp->tx_buff;
		d = skb->data;
		space = (ww - w - 1) & CMX_BUFF_MASK;
		
		if (space < skb->len) {
			
			ww = (ww - 1) & CMX_BUFF_MASK; 
			if (dsp_debug & DEBUG_DSP_CLOCK)
				printk(KERN_DEBUG "%s: TX overflow space=%d skb->len="
				       "%d, w=0x%04x, ww=0x%04x\n", __func__, space,
				       skb->len, w, ww);
		} else
			
			ww = (w + skb->len) & CMX_BUFF_MASK;
		dsp->tx_W = ww;

		
#ifdef CMX_DEBUG
		printk(KERN_DEBUG
		       "cmx_transmit(dsp=%lx) %d bytes to 0x%x-0x%x. %s\n",
		       (u_long)dsp, (ww - w) & CMX_BUFF_MASK, w, ww, dsp->name);
#endif

		
#ifdef CMX_TX_DEBUG
		sprintf(debugbuf, "TX getting (%04x-%04x)%p: ", w, ww, p);
#endif
		while (w != ww) {
#ifdef CMX_TX_DEBUG
			if (strlen(debugbuf) < 48)
				sprintf(debugbuf + strlen(debugbuf), " %02x", *d);
#endif
			p[w] = *d++;
			w = (w + 1) & CMX_BUFF_MASK;
		}
#ifdef CMX_TX_DEBUG
		printk(KERN_DEBUG "%s\n", debugbuf);
#endif

	}

	void
		dsp_cmx_hdlc(struct dsp *dsp, struct sk_buff *skb)
	{
		struct sk_buff *nskb = NULL;
		struct dsp_conf_member *member;
		struct mISDNhead *hh;

		
		if (!dsp->b_active)
			return;

		
		if (skb->len < 1)
			return;

		
		if (!dsp->conf) {
			
			if (dsp->echo.software) {
				nskb = skb_clone(skb, GFP_ATOMIC);
				if (nskb) {
					hh = mISDN_HEAD_P(nskb);
					hh->prim = PH_DATA_REQ;
					hh->id = 0;
					skb_queue_tail(&dsp->sendq, nskb);
					schedule_work(&dsp->workq);
				}
			}
			return;
		}
		
		if (dsp->conf->hardware)
			return;
		list_for_each_entry(member, &dsp->conf->mlist, list) {
			if (dsp->echo.software || member->dsp != dsp) {
				nskb = skb_clone(skb, GFP_ATOMIC);
				if (nskb) {
					hh = mISDN_HEAD_P(nskb);
					hh->prim = PH_DATA_REQ;
					hh->id = 0;
					skb_queue_tail(&member->dsp->sendq, nskb);
					schedule_work(&member->dsp->workq);
				}
			}
		}
	}
