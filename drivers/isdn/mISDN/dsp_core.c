/*
 * Author       Andreas Eversberg (jolly@eversberg.eu)
 * Based on source code structure by
 *		Karsten Keil (keil@isdn4linux.de)
 *
 *		This file is (c) under GNU PUBLIC LICENSE
 *		For changes and modifications please read
 *		../../../Documentation/isdn/mISDN.cert
 *
 * Thanks to    Karsten Keil (great drivers)
 *              Cologne Chip (great chips)
 *
 * This module does:
 *		Real-time tone generation
 *		DTMF detection
 *		Real-time cross-connection and conferrence
 *		Compensate jitter due to system load and hardware fault.
 *		All features are done in kernel space and will be realized
 *		using hardware, if available and supported by chip set.
 *		Blowfish encryption/decryption
 */


#include <linux/delay.h>
#include <linux/gfp.h>
#include <linux/mISDNif.h>
#include <linux/mISDNdsp.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include "core.h"
#include "dsp.h"

static const char *mISDN_dsp_revision = "2.0";

static int debug;
static int options;
static int poll;
static int dtmfthreshold = 100;

MODULE_AUTHOR("Andreas Eversberg");
module_param(debug, uint, S_IRUGO | S_IWUSR);
module_param(options, uint, S_IRUGO | S_IWUSR);
module_param(poll, uint, S_IRUGO | S_IWUSR);
module_param(dtmfthreshold, uint, S_IRUGO | S_IWUSR);
MODULE_LICENSE("GPL");


spinlock_t dsp_lock; 
struct list_head dsp_ilist;
struct list_head conf_ilist;
int dsp_debug;
int dsp_options;
int dsp_poll, dsp_tics;

static void
dsp_rx_off_member(struct dsp *dsp)
{
	struct mISDN_ctrl_req	cq;
	int rx_off = 1;

	memset(&cq, 0, sizeof(cq));

	if (!dsp->features_rx_off)
		return;

	
	if (!dsp->rx_disabled)
		rx_off = 0;
	
	else if (dsp->dtmf.software)
		rx_off = 0;
	
	else if (dsp->echo.software)
		rx_off = 0;
	
	else if (dsp->conf && dsp->conf->software)
		rx_off = 0;

	if (rx_off == dsp->rx_is_off)
		return;

	if (!dsp->ch.peer) {
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: no peer, no rx_off\n",
			       __func__);
		return;
	}
	cq.op = MISDN_CTRL_RX_OFF;
	cq.p1 = rx_off;
	if (dsp->ch.peer->ctrl(dsp->ch.peer, CONTROL_CHANNEL, &cq)) {
		printk(KERN_DEBUG "%s: 2nd CONTROL_CHANNEL failed\n",
		       __func__);
		return;
	}
	dsp->rx_is_off = rx_off;
	if (dsp_debug & DEBUG_DSP_CORE)
		printk(KERN_DEBUG "%s: %s set rx_off = %d\n",
		       __func__, dsp->name, rx_off);
}
static void
dsp_rx_off(struct dsp *dsp)
{
	struct dsp_conf_member	*member;

	if (dsp_options & DSP_OPT_NOHARDWARE)
		return;

	
	if (!dsp->conf) {
		dsp_rx_off_member(dsp);
		return;
	}
	
	list_for_each_entry(member, &dsp->conf->mlist, list) {
		dsp_rx_off_member(member->dsp);
	}
}

static void
dsp_fill_empty(struct dsp *dsp)
{
	struct mISDN_ctrl_req	cq;

	memset(&cq, 0, sizeof(cq));

	if (!dsp->ch.peer) {
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: no peer, no fill_empty\n",
			       __func__);
		return;
	}
	cq.op = MISDN_CTRL_FILL_EMPTY;
	cq.p1 = 1;
	if (dsp->ch.peer->ctrl(dsp->ch.peer, CONTROL_CHANNEL, &cq)) {
		printk(KERN_DEBUG "%s: CONTROL_CHANNEL failed\n",
		       __func__);
		return;
	}
	if (dsp_debug & DEBUG_DSP_CORE)
		printk(KERN_DEBUG "%s: %s set fill_empty = 1\n",
		       __func__, dsp->name);
}

static int
dsp_control_req(struct dsp *dsp, struct mISDNhead *hh, struct sk_buff *skb)
{
	struct sk_buff	*nskb;
	int ret = 0;
	int cont;
	u8 *data;
	int len;

	if (skb->len < sizeof(int))
		printk(KERN_ERR "%s: PH_CONTROL message too short\n", __func__);
	cont = *((int *)skb->data);
	len = skb->len - sizeof(int);
	data = skb->data + sizeof(int);

	switch (cont) {
	case DTMF_TONE_START: 
		if (dsp->hdlc) {
			ret = -EINVAL;
			break;
		}
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: start dtmf\n", __func__);
		if (len == sizeof(int)) {
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_NOTICE "changing DTMF Threshold "
				       "to %d\n", *((int *)data));
			dsp->dtmf.treshold = (*(int *)data) * 10000;
		}
		dsp->dtmf.enable = 1;
		
		dsp_dtmf_goertzel_init(dsp);

		
		dsp_dtmf_hardware(dsp);
		dsp_rx_off(dsp);
		break;
	case DTMF_TONE_STOP: 
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: stop dtmf\n", __func__);
		dsp->dtmf.enable = 0;
		dsp->dtmf.hardware = 0;
		dsp->dtmf.software = 0;
		break;
	case DSP_CONF_JOIN: 
		if (len < sizeof(int)) {
			ret = -EINVAL;
			break;
		}
		if (*((u32 *)data) == 0)
			goto conf_split;
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: join conference %d\n",
			       __func__, *((u32 *)data));
		ret = dsp_cmx_conf(dsp, *((u32 *)data));
		
		dsp_rx_off(dsp);
		if (dsp_debug & DEBUG_DSP_CMX)
			dsp_cmx_debug(dsp);
		break;
	case DSP_CONF_SPLIT: 
	conf_split:
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: release conference\n", __func__);
		ret = dsp_cmx_conf(dsp, 0);
		
		if (dsp_debug & DEBUG_DSP_CMX)
			dsp_cmx_debug(dsp);
		dsp_rx_off(dsp);
		break;
	case DSP_TONE_PATT_ON: 
		if (dsp->hdlc) {
			ret = -EINVAL;
			break;
		}
		if (len < sizeof(int)) {
			ret = -EINVAL;
			break;
		}
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: turn tone 0x%x on\n",
			       __func__, *((int *)skb->data));
		ret = dsp_tone(dsp, *((int *)data));
		if (!ret) {
			dsp_cmx_hardware(dsp->conf, dsp);
			dsp_rx_off(dsp);
		}
		if (!dsp->tone.tone)
			goto tone_off;
		break;
	case DSP_TONE_PATT_OFF: 
		if (dsp->hdlc) {
			ret = -EINVAL;
			break;
		}
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: turn tone off\n", __func__);
		dsp_tone(dsp, 0);
		dsp_cmx_hardware(dsp->conf, dsp);
		dsp_rx_off(dsp);
		
	tone_off:
		dsp->rx_W = 0;
		dsp->rx_R = 0;
		break;
	case DSP_VOL_CHANGE_TX: 
		if (dsp->hdlc) {
			ret = -EINVAL;
			break;
		}
		if (len < sizeof(int)) {
			ret = -EINVAL;
			break;
		}
		dsp->tx_volume = *((int *)data);
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: change tx vol to %d\n",
			       __func__, dsp->tx_volume);
		dsp_cmx_hardware(dsp->conf, dsp);
		dsp_dtmf_hardware(dsp);
		dsp_rx_off(dsp);
		break;
	case DSP_VOL_CHANGE_RX: 
		if (dsp->hdlc) {
			ret = -EINVAL;
			break;
		}
		if (len < sizeof(int)) {
			ret = -EINVAL;
			break;
		}
		dsp->rx_volume = *((int *)data);
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: change rx vol to %d\n",
			       __func__, dsp->tx_volume);
		dsp_cmx_hardware(dsp->conf, dsp);
		dsp_dtmf_hardware(dsp);
		dsp_rx_off(dsp);
		break;
	case DSP_ECHO_ON: 
		dsp->echo.software = 1; 
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: enable cmx-echo\n", __func__);
		dsp_cmx_hardware(dsp->conf, dsp);
		dsp_rx_off(dsp);
		if (dsp_debug & DEBUG_DSP_CMX)
			dsp_cmx_debug(dsp);
		break;
	case DSP_ECHO_OFF: 
		dsp->echo.software = 0;
		dsp->echo.hardware = 0;
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: disable cmx-echo\n", __func__);
		dsp_cmx_hardware(dsp->conf, dsp);
		dsp_rx_off(dsp);
		if (dsp_debug & DEBUG_DSP_CMX)
			dsp_cmx_debug(dsp);
		break;
	case DSP_RECEIVE_ON: 
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: enable receive to user "
			       "space\n", __func__);
		dsp->rx_disabled = 0;
		dsp_rx_off(dsp);
		break;
	case DSP_RECEIVE_OFF: 
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: disable receive to "
			       "user space\n", __func__);
		dsp->rx_disabled = 1;
		dsp_rx_off(dsp);
		break;
	case DSP_MIX_ON: 
		if (dsp->hdlc) {
			ret = -EINVAL;
			break;
		}
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: enable mixing of "
			       "tx-data with conf mebers\n", __func__);
		dsp->tx_mix = 1;
		dsp_cmx_hardware(dsp->conf, dsp);
		dsp_rx_off(dsp);
		if (dsp_debug & DEBUG_DSP_CMX)
			dsp_cmx_debug(dsp);
		break;
	case DSP_MIX_OFF: 
		if (dsp->hdlc) {
			ret = -EINVAL;
			break;
		}
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: disable mixing of "
			       "tx-data with conf mebers\n", __func__);
		dsp->tx_mix = 0;
		dsp_cmx_hardware(dsp->conf, dsp);
		dsp_rx_off(dsp);
		if (dsp_debug & DEBUG_DSP_CMX)
			dsp_cmx_debug(dsp);
		break;
	case DSP_TXDATA_ON: 
		dsp->tx_data = 1;
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: enable tx-data\n", __func__);
		dsp_cmx_hardware(dsp->conf, dsp);
		dsp_rx_off(dsp);
		if (dsp_debug & DEBUG_DSP_CMX)
			dsp_cmx_debug(dsp);
		break;
	case DSP_TXDATA_OFF: 
		dsp->tx_data = 0;
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: disable tx-data\n", __func__);
		dsp_cmx_hardware(dsp->conf, dsp);
		dsp_rx_off(dsp);
		if (dsp_debug & DEBUG_DSP_CMX)
			dsp_cmx_debug(dsp);
		break;
	case DSP_DELAY: 
		if (dsp->hdlc) {
			ret = -EINVAL;
			break;
		}
		if (len < sizeof(int)) {
			ret = -EINVAL;
			break;
		}
		dsp->cmx_delay = (*((int *)data)) << 3;
		
		if (dsp->cmx_delay >= (CMX_BUFF_HALF >> 1))
			dsp->cmx_delay = (CMX_BUFF_HALF >> 1) - 1;
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: use delay algorithm to "
			       "compensate jitter (%d samples)\n",
			       __func__, dsp->cmx_delay);
		break;
	case DSP_JITTER: 
		if (dsp->hdlc) {
			ret = -EINVAL;
			break;
		}
		dsp->cmx_delay = 0;
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: use jitter algorithm to "
			       "compensate jitter\n", __func__);
		break;
	case DSP_TX_DEJITTER: 
		if (dsp->hdlc) {
			ret = -EINVAL;
			break;
		}
		dsp->tx_dejitter = 1;
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: use dejitter on TX "
			       "buffer\n", __func__);
		break;
	case DSP_TX_DEJ_OFF: 
		if (dsp->hdlc) {
			ret = -EINVAL;
			break;
		}
		dsp->tx_dejitter = 0;
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: use TX buffer without "
			       "dejittering\n", __func__);
		break;
	case DSP_PIPELINE_CFG:
		if (dsp->hdlc) {
			ret = -EINVAL;
			break;
		}
		if (len > 0 && ((char *)data)[len - 1]) {
			printk(KERN_DEBUG "%s: pipeline config string "
			       "is not NULL terminated!\n", __func__);
			ret = -EINVAL;
		} else {
			dsp->pipeline.inuse = 1;
			dsp_cmx_hardware(dsp->conf, dsp);
			ret = dsp_pipeline_build(&dsp->pipeline,
						 len > 0 ? data : NULL);
			dsp_cmx_hardware(dsp->conf, dsp);
			dsp_rx_off(dsp);
		}
		break;
	case DSP_BF_ENABLE_KEY: 
		if (dsp->hdlc) {
			ret = -EINVAL;
			break;
		}
		if (len < 4 || len > 56) {
			ret = -EINVAL;
			break;
		}
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: turn blowfish on (key "
			       "not shown)\n", __func__);
		ret = dsp_bf_init(dsp, (u8 *)data, len);
		
		if (!ret)
			cont = DSP_BF_ACCEPT;
		else
			cont = DSP_BF_REJECT;
		
		nskb = _alloc_mISDN_skb(PH_CONTROL_IND, MISDN_ID_ANY,
					sizeof(int), &cont, GFP_ATOMIC);
		if (nskb) {
			if (dsp->up) {
				if (dsp->up->send(dsp->up, nskb))
					dev_kfree_skb(nskb);
			} else
				dev_kfree_skb(nskb);
		}
		if (!ret) {
			dsp_cmx_hardware(dsp->conf, dsp);
			dsp_dtmf_hardware(dsp);
			dsp_rx_off(dsp);
		}
		break;
	case DSP_BF_DISABLE: 
		if (dsp->hdlc) {
			ret = -EINVAL;
			break;
		}
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: turn blowfish off\n", __func__);
		dsp_bf_cleanup(dsp);
		dsp_cmx_hardware(dsp->conf, dsp);
		dsp_dtmf_hardware(dsp);
		dsp_rx_off(dsp);
		break;
	default:
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: ctrl req %x unhandled\n",
			       __func__, cont);
		ret = -EINVAL;
	}
	return ret;
}

static void
get_features(struct mISDNchannel *ch)
{
	struct dsp		*dsp = container_of(ch, struct dsp, ch);
	struct mISDN_ctrl_req	cq;

	if (!ch->peer) {
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: no peer, no features\n",
			       __func__);
		return;
	}
	memset(&cq, 0, sizeof(cq));
	cq.op = MISDN_CTRL_GETOP;
	if (ch->peer->ctrl(ch->peer, CONTROL_CHANNEL, &cq) < 0) {
		printk(KERN_DEBUG "%s: CONTROL_CHANNEL failed\n",
		       __func__);
		return;
	}
	if (cq.op & MISDN_CTRL_RX_OFF)
		dsp->features_rx_off = 1;
	if (cq.op & MISDN_CTRL_FILL_EMPTY)
		dsp->features_fill_empty = 1;
	if (dsp_options & DSP_OPT_NOHARDWARE)
		return;
	if ((cq.op & MISDN_CTRL_HW_FEATURES_OP)) {
		cq.op = MISDN_CTRL_HW_FEATURES;
		*((u_long *)&cq.p1) = (u_long)&dsp->features;
		if (ch->peer->ctrl(ch->peer, CONTROL_CHANNEL, &cq)) {
			printk(KERN_DEBUG "%s: 2nd CONTROL_CHANNEL failed\n",
			       __func__);
		}
	} else
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: features not supported for %s\n",
			       __func__, dsp->name);
}

static int
dsp_function(struct mISDNchannel *ch,  struct sk_buff *skb)
{
	struct dsp		*dsp = container_of(ch, struct dsp, ch);
	struct mISDNhead	*hh;
	int			ret = 0;
	u8			*digits = NULL;
	u_long			flags;

	hh = mISDN_HEAD_P(skb);
	switch (hh->prim) {
		
	case (PH_DATA_CNF):
		dsp->data_pending = 0;
		
		if (dsp->hdlc) {
			spin_lock_irqsave(&dsp_lock, flags);
			if (dsp->b_active)
				schedule_work(&dsp->workq);
			spin_unlock_irqrestore(&dsp_lock, flags);
		}
		break;
	case (PH_DATA_IND):
	case (DL_DATA_IND):
		if (skb->len < 1) {
			ret = -EINVAL;
			break;
		}
		if (dsp->rx_is_off) {
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: rx-data during rx_off"
				       " for %s\n",
				       __func__, dsp->name);
		}
		if (dsp->hdlc) {
			
			spin_lock_irqsave(&dsp_lock, flags);
			dsp_cmx_hdlc(dsp, skb);
			spin_unlock_irqrestore(&dsp_lock, flags);
			if (dsp->rx_disabled) {
				
				break;
			}
			hh->prim = DL_DATA_IND;
			if (dsp->up)
				return dsp->up->send(dsp->up, skb);
			break;
		}

		spin_lock_irqsave(&dsp_lock, flags);

		
		if (dsp->bf_enable)
			dsp_bf_decrypt(dsp, skb->data, skb->len);
		
		if (dsp->pipeline.inuse)
			dsp_pipeline_process_rx(&dsp->pipeline, skb->data,
						skb->len, hh->id);
		
		if (dsp->rx_volume)
			dsp_change_volume(skb, dsp->rx_volume);
		
		if (dsp->dtmf.software) {
			digits = dsp_dtmf_goertzel_decode(dsp, skb->data,
							  skb->len, (dsp_options & DSP_OPT_ULAW) ? 1 : 0);
		}
		
		if (dsp->conf && dsp->conf->software) {
			
			dsp_cmx_receive(dsp, skb);
		}

		spin_unlock_irqrestore(&dsp_lock, flags);

		
		if (digits) {
			while (*digits) {
				int k;
				struct sk_buff *nskb;
				if (dsp_debug & DEBUG_DSP_DTMF)
					printk(KERN_DEBUG "%s: digit"
					       "(%c) to layer %s\n",
					       __func__, *digits, dsp->name);
				k = *digits | DTMF_TONE_VAL;
				nskb = _alloc_mISDN_skb(PH_CONTROL_IND,
							MISDN_ID_ANY, sizeof(int), &k,
							GFP_ATOMIC);
				if (nskb) {
					if (dsp->up) {
						if (dsp->up->send(
							    dsp->up, nskb))
							dev_kfree_skb(nskb);
					} else
						dev_kfree_skb(nskb);
				}
				digits++;
			}
		}
		if (dsp->rx_disabled) {
			
			break;
		}
		hh->prim = DL_DATA_IND;
		if (dsp->up)
			return dsp->up->send(dsp->up, skb);
		break;
	case (PH_CONTROL_IND):
		if (dsp_debug & DEBUG_DSP_DTMFCOEFF)
			printk(KERN_DEBUG "%s: PH_CONTROL INDICATION "
			       "received: %x (len %d) %s\n", __func__,
			       hh->id, skb->len, dsp->name);
		switch (hh->id) {
		case (DTMF_HFC_COEF): 
			if (!dsp->dtmf.hardware) {
				if (dsp_debug & DEBUG_DSP_DTMFCOEFF)
					printk(KERN_DEBUG "%s: ignoring DTMF "
					       "coefficients from HFC\n",
					       __func__);
				break;
			}
			digits = dsp_dtmf_goertzel_decode(dsp, skb->data,
							  skb->len, 2);
			while (*digits) {
				int k;
				struct sk_buff *nskb;
				if (dsp_debug & DEBUG_DSP_DTMF)
					printk(KERN_DEBUG "%s: digit"
					       "(%c) to layer %s\n",
					       __func__, *digits, dsp->name);
				k = *digits | DTMF_TONE_VAL;
				nskb = _alloc_mISDN_skb(PH_CONTROL_IND,
							MISDN_ID_ANY, sizeof(int), &k,
							GFP_ATOMIC);
				if (nskb) {
					if (dsp->up) {
						if (dsp->up->send(
							    dsp->up, nskb))
							dev_kfree_skb(nskb);
					} else
						dev_kfree_skb(nskb);
				}
				digits++;
			}
			break;
		case (HFC_VOL_CHANGE_TX): 
			if (skb->len != sizeof(int)) {
				ret = -EINVAL;
				break;
			}
			spin_lock_irqsave(&dsp_lock, flags);
			dsp->tx_volume = *((int *)skb->data);
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: change tx volume to "
				       "%d\n", __func__, dsp->tx_volume);
			dsp_cmx_hardware(dsp->conf, dsp);
			dsp_dtmf_hardware(dsp);
			dsp_rx_off(dsp);
			spin_unlock_irqrestore(&dsp_lock, flags);
			break;
		default:
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: ctrl ind %x unhandled "
				       "%s\n", __func__, hh->id, dsp->name);
			ret = -EINVAL;
		}
		break;
	case (PH_ACTIVATE_IND):
	case (PH_ACTIVATE_CNF):
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: b_channel is now active %s\n",
			       __func__, dsp->name);
		
		spin_lock_irqsave(&dsp_lock, flags);
		dsp->b_active = 1;
		dsp->data_pending = 0;
		dsp->rx_init = 1;
		
		dsp->rx_W = 0;
		dsp->rx_R = 0;
		memset(dsp->rx_buff, 0, sizeof(dsp->rx_buff));
		dsp_cmx_hardware(dsp->conf, dsp);
		dsp_dtmf_hardware(dsp);
		dsp_rx_off(dsp);
		spin_unlock_irqrestore(&dsp_lock, flags);
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: done with activation, sending "
			       "confirm to user space. %s\n", __func__,
			       dsp->name);
		
		hh->prim = DL_ESTABLISH_CNF;
		if (dsp->up)
			return dsp->up->send(dsp->up, skb);
		break;
	case (PH_DEACTIVATE_IND):
	case (PH_DEACTIVATE_CNF):
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: b_channel is now inactive %s\n",
			       __func__, dsp->name);
		
		spin_lock_irqsave(&dsp_lock, flags);
		dsp->b_active = 0;
		dsp->data_pending = 0;
		dsp_cmx_hardware(dsp->conf, dsp);
		dsp_rx_off(dsp);
		spin_unlock_irqrestore(&dsp_lock, flags);
		hh->prim = DL_RELEASE_CNF;
		if (dsp->up)
			return dsp->up->send(dsp->up, skb);
		break;
		
	case (DL_DATA_REQ):
	case (PH_DATA_REQ):
		if (skb->len < 1) {
			ret = -EINVAL;
			break;
		}
		if (dsp->hdlc) {
			
			if (!dsp->b_active) {
				ret = -EIO;
				break;
			}
			hh->prim = PH_DATA_REQ;
			spin_lock_irqsave(&dsp_lock, flags);
			skb_queue_tail(&dsp->sendq, skb);
			schedule_work(&dsp->workq);
			spin_unlock_irqrestore(&dsp_lock, flags);
			return 0;
		}
		
		if (!dsp->tone.tone) {
			spin_lock_irqsave(&dsp_lock, flags);
			dsp_cmx_transmit(dsp, skb);
			spin_unlock_irqrestore(&dsp_lock, flags);
		}
		break;
	case (PH_CONTROL_REQ):
		spin_lock_irqsave(&dsp_lock, flags);
		ret = dsp_control_req(dsp, hh, skb);
		spin_unlock_irqrestore(&dsp_lock, flags);
		break;
	case (DL_ESTABLISH_REQ):
	case (PH_ACTIVATE_REQ):
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: activating b_channel %s\n",
			       __func__, dsp->name);
		if (dsp->dtmf.hardware || dsp->dtmf.software)
			dsp_dtmf_goertzel_init(dsp);
		get_features(ch);
		
		if (dsp->features_fill_empty)
			dsp_fill_empty(dsp);
		
		hh->prim = PH_ACTIVATE_REQ;
		if (ch->peer)
			return ch->recv(ch->peer, skb);
		break;
	case (DL_RELEASE_REQ):
	case (PH_DEACTIVATE_REQ):
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: releasing b_channel %s\n",
			       __func__, dsp->name);
		spin_lock_irqsave(&dsp_lock, flags);
		dsp->tone.tone = 0;
		dsp->tone.hardware = 0;
		dsp->tone.software = 0;
		if (timer_pending(&dsp->tone.tl))
			del_timer(&dsp->tone.tl);
		if (dsp->conf)
			dsp_cmx_conf(dsp, 0); 
		skb_queue_purge(&dsp->sendq);
		spin_unlock_irqrestore(&dsp_lock, flags);
		hh->prim = PH_DEACTIVATE_REQ;
		if (ch->peer)
			return ch->recv(ch->peer, skb);
		break;
	default:
		if (dsp_debug & DEBUG_DSP_CORE)
			printk(KERN_DEBUG "%s: msg %x unhandled %s\n",
			       __func__, hh->prim, dsp->name);
		ret = -EINVAL;
	}
	if (!ret)
		dev_kfree_skb(skb);
	return ret;
}

static int
dsp_ctrl(struct mISDNchannel *ch, u_int cmd, void *arg)
{
	struct dsp		*dsp = container_of(ch, struct dsp, ch);
	u_long		flags;
	int		err = 0;

	if (debug & DEBUG_DSP_CTRL)
		printk(KERN_DEBUG "%s:(%x)\n", __func__, cmd);

	switch (cmd) {
	case OPEN_CHANNEL:
		break;
	case CLOSE_CHANNEL:
		if (dsp->ch.peer)
			dsp->ch.peer->ctrl(dsp->ch.peer, CLOSE_CHANNEL, NULL);

		spin_lock_irqsave(&dsp_lock, flags);
		dsp->b_active = 0;
		spin_unlock_irqrestore(&dsp_lock, flags);
		
		cancel_work_sync(&dsp->workq);
		spin_lock_irqsave(&dsp_lock, flags);
		if (timer_pending(&dsp->tone.tl))
			del_timer(&dsp->tone.tl);
		skb_queue_purge(&dsp->sendq);
		if (dsp_debug & DEBUG_DSP_CTRL)
			printk(KERN_DEBUG "%s: releasing member %s\n",
			       __func__, dsp->name);
		dsp->b_active = 0;
		dsp_cmx_conf(dsp, 0); 
		dsp_pipeline_destroy(&dsp->pipeline);

		if (dsp_debug & DEBUG_DSP_CTRL)
			printk(KERN_DEBUG "%s: remove & destroy object %s\n",
			       __func__, dsp->name);
		list_del(&dsp->list);
		spin_unlock_irqrestore(&dsp_lock, flags);

		if (dsp_debug & DEBUG_DSP_CTRL)
			printk(KERN_DEBUG "%s: dsp instance released\n",
			       __func__);
		vfree(dsp);
		module_put(THIS_MODULE);
		break;
	}
	return err;
}

static void
dsp_send_bh(struct work_struct *work)
{
	struct dsp *dsp = container_of(work, struct dsp, workq);
	struct sk_buff *skb;
	struct mISDNhead	*hh;

	if (dsp->hdlc && dsp->data_pending)
		return; 

	
	while ((skb = skb_dequeue(&dsp->sendq))) {
		
		if (dsp->data_pending) {
			if (dsp_debug & DEBUG_DSP_CORE)
				printk(KERN_DEBUG "%s: fifo full %s, this is "
				       "no bug!\n", __func__, dsp->name);
			
			dev_kfree_skb(skb);
			continue;
		}
		hh = mISDN_HEAD_P(skb);
		if (hh->prim == DL_DATA_REQ) {
			
			if (dsp->up) {
				if (dsp->up->send(dsp->up, skb))
					dev_kfree_skb(skb);
			} else
				dev_kfree_skb(skb);
		} else {
			
			if (dsp->ch.peer) {
				dsp->data_pending = 1;
				if (dsp->ch.recv(dsp->ch.peer, skb)) {
					dev_kfree_skb(skb);
					dsp->data_pending = 0;
				}
			} else
				dev_kfree_skb(skb);
		}
	}
}

static int
dspcreate(struct channel_req *crq)
{
	struct dsp		*ndsp;
	u_long		flags;

	if (crq->protocol != ISDN_P_B_L2DSP
	    && crq->protocol != ISDN_P_B_L2DSPHDLC)
		return -EPROTONOSUPPORT;
	ndsp = vzalloc(sizeof(struct dsp));
	if (!ndsp) {
		printk(KERN_ERR "%s: vmalloc struct dsp failed\n", __func__);
		return -ENOMEM;
	}
	if (dsp_debug & DEBUG_DSP_CTRL)
		printk(KERN_DEBUG "%s: creating new dsp instance\n", __func__);

	
	INIT_WORK(&ndsp->workq, (void *)dsp_send_bh);
	skb_queue_head_init(&ndsp->sendq);
	ndsp->ch.send = dsp_function;
	ndsp->ch.ctrl = dsp_ctrl;
	ndsp->up = crq->ch;
	crq->ch = &ndsp->ch;
	if (crq->protocol == ISDN_P_B_L2DSP) {
		crq->protocol = ISDN_P_B_RAW;
		ndsp->hdlc = 0;
	} else {
		crq->protocol = ISDN_P_B_HDLC;
		ndsp->hdlc = 1;
	}
	if (!try_module_get(THIS_MODULE))
		printk(KERN_WARNING "%s:cannot get module\n",
		       __func__);

	sprintf(ndsp->name, "DSP_C%x(0x%p)",
		ndsp->up->st->dev->id + 1, ndsp);
	
	ndsp->features.hfc_id = -1; 
	ndsp->features.pcm_id = -1; 
	ndsp->pcm_slot_rx = -1; 
	ndsp->pcm_slot_tx = -1;
	ndsp->pcm_bank_rx = -1;
	ndsp->pcm_bank_tx = -1;
	ndsp->hfc_conf = -1; 
	
	ndsp->tone.tl.function = (void *)dsp_tone_timeout;
	ndsp->tone.tl.data = (long) ndsp;
	init_timer(&ndsp->tone.tl);

	if (dtmfthreshold < 20 || dtmfthreshold > 500)
		dtmfthreshold = 200;
	ndsp->dtmf.treshold = dtmfthreshold * 10000;

	
	spin_lock_irqsave(&dsp_lock, flags);
	dsp_pipeline_init(&ndsp->pipeline);
	list_add_tail(&ndsp->list, &dsp_ilist);
	spin_unlock_irqrestore(&dsp_lock, flags);

	return 0;
}


static struct Bprotocol DSP = {
	.Bprotocols = (1 << (ISDN_P_B_L2DSP & ISDN_P_B_MASK))
	| (1 << (ISDN_P_B_L2DSPHDLC & ISDN_P_B_MASK)),
	.name = "dsp",
	.create = dspcreate
};

static int __init dsp_init(void)
{
	int err;
	int tics;

	printk(KERN_INFO "DSP module %s\n", mISDN_dsp_revision);

	dsp_options = options;
	dsp_debug = debug;

	
	dsp_poll = poll;
	if (dsp_poll) {
		if (dsp_poll > MAX_POLL) {
			printk(KERN_ERR "%s: Wrong poll value (%d), use %d "
			       "maximum.\n", __func__, poll, MAX_POLL);
			err = -EINVAL;
			return err;
		}
		if (dsp_poll < 8) {
			printk(KERN_ERR "%s: Wrong poll value (%d), use 8 "
			       "minimum.\n", __func__, dsp_poll);
			err = -EINVAL;
			return err;
		}
		dsp_tics = poll * HZ / 8000;
		if (dsp_tics * 8000 != poll * HZ) {
			printk(KERN_INFO "mISDN_dsp: Cannot clock every %d "
			       "samples (0,125 ms). It is not a multiple of "
			       "%d HZ.\n", poll, HZ);
			err = -EINVAL;
			return err;
		}
	} else {
		poll = 8;
		while (poll <= MAX_POLL) {
			tics = (poll * HZ) / 8000;
			if (tics * 8000 == poll * HZ) {
				dsp_tics = tics;
				dsp_poll = poll;
				if (poll >= 64)
					break;
			}
			poll++;
		}
	}
	if (dsp_poll == 0) {
		printk(KERN_INFO "mISDN_dsp: There is no multiple of kernel "
		       "clock that equals exactly the duration of 8-256 "
		       "samples. (Choose kernel clock speed like 100, 250, "
		       "300, 1000)\n");
		err = -EINVAL;
		return err;
	}
	printk(KERN_INFO "mISDN_dsp: DSP clocks every %d samples. This equals "
	       "%d jiffies.\n", dsp_poll, dsp_tics);

	spin_lock_init(&dsp_lock);
	INIT_LIST_HEAD(&dsp_ilist);
	INIT_LIST_HEAD(&conf_ilist);

	
	dsp_audio_generate_law_tables();
	dsp_silence = (dsp_options & DSP_OPT_ULAW) ? 0xff : 0x2a;
	dsp_audio_law_to_s32 = (dsp_options & DSP_OPT_ULAW) ?
		dsp_audio_ulaw_to_s32 : dsp_audio_alaw_to_s32;
	dsp_audio_generate_s2law_table();
	dsp_audio_generate_seven();
	dsp_audio_generate_mix_table();
	if (dsp_options & DSP_OPT_ULAW)
		dsp_audio_generate_ulaw_samples();
	dsp_audio_generate_volume_changes();

	err = dsp_pipeline_module_init();
	if (err) {
		printk(KERN_ERR "mISDN_dsp: Can't initialize pipeline, "
		       "error(%d)\n", err);
		return err;
	}

	err = mISDN_register_Bprotocol(&DSP);
	if (err) {
		printk(KERN_ERR "Can't register %s error(%d)\n", DSP.name, err);
		return err;
	}

	
	dsp_spl_tl.function = (void *)dsp_cmx_send;
	dsp_spl_tl.data = 0;
	init_timer(&dsp_spl_tl);
	dsp_spl_tl.expires = jiffies + dsp_tics;
	dsp_spl_jiffies = dsp_spl_tl.expires;
	add_timer(&dsp_spl_tl);

	return 0;
}


static void __exit dsp_cleanup(void)
{
	mISDN_unregister_Bprotocol(&DSP);

	if (timer_pending(&dsp_spl_tl))
		del_timer(&dsp_spl_tl);

	if (!list_empty(&dsp_ilist)) {
		printk(KERN_ERR "mISDN_dsp: Audio DSP object inst list not "
		       "empty.\n");
	}
	if (!list_empty(&conf_ilist)) {
		printk(KERN_ERR "mISDN_dsp: Conference list not empty. Not "
		       "all memory freed.\n");
	}

	dsp_pipeline_module_exit();
}

module_init(dsp_init);
module_exit(dsp_cleanup);
