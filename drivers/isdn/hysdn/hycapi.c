/* $Id: hycapi.c,v 1.8.6.4 2001/09/23 22:24:54 kai Exp $
 *
 * Linux driver for HYSDN cards, CAPI2.0-Interface.
 *
 * Author    Ulrich Albrecht <u.albrecht@hypercope.de> for Hypercope GmbH
 * Copyright 2000 by Hypercope GmbH
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/signal.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/slab.h>

#define	VER_DRIVER	0
#define	VER_CARDTYPE	1
#define	VER_HWID	2
#define	VER_SERIAL	3
#define	VER_OPTION	4
#define	VER_PROTO	5
#define	VER_PROFILE	6
#define	VER_CAPI	7

#include "hysdn_defs.h"
#include <linux/kernelcapi.h>

static char hycapi_revision[] = "$Revision: 1.8.6.4 $";

unsigned int hycapi_enable = 0xffffffff;
module_param(hycapi_enable, uint, 0);

typedef struct _hycapi_appl {
	unsigned int ctrl_mask;
	capi_register_params rp;
	struct sk_buff *listen_req[CAPI_MAXCONTR];
} hycapi_appl;

static hycapi_appl hycapi_applications[CAPI_MAXAPPL];

static u16 hycapi_send_message(struct capi_ctr *ctrl, struct sk_buff *skb);

static inline int _hycapi_appCheck(int app_id, int ctrl_no)
{
	if ((ctrl_no <= 0) || (ctrl_no > CAPI_MAXCONTR) || (app_id <= 0) ||
	   (app_id > CAPI_MAXAPPL))
	{
		printk(KERN_ERR "HYCAPI: Invalid request app_id %d for controller %d", app_id, ctrl_no);
		return -1;
	}
	return ((hycapi_applications[app_id - 1].ctrl_mask & (1 << (ctrl_no-1))) != 0);
}


static void
hycapi_reset_ctr(struct capi_ctr *ctrl)
{
	hycapictrl_info *cinfo = ctrl->driverdata;

#ifdef HYCAPI_PRINTFNAMES
	printk(KERN_NOTICE "HYCAPI hycapi_reset_ctr\n");
#endif
	capilib_release(&cinfo->ncci_head);
	capi_ctr_down(ctrl);
}


static void
hycapi_remove_ctr(struct capi_ctr *ctrl)
{
	int i;
	hycapictrl_info *cinfo = NULL;
	hysdn_card *card = NULL;
#ifdef HYCAPI_PRINTFNAMES
	printk(KERN_NOTICE "HYCAPI hycapi_remove_ctr\n");
#endif
	cinfo = (hycapictrl_info *)(ctrl->driverdata);
	if (!cinfo) {
		printk(KERN_ERR "No hycapictrl_info set!");
		return;
	}
	card = cinfo->card;
	capi_ctr_suspend_output(ctrl);
	for (i = 0; i < CAPI_MAXAPPL; i++) {
		if (hycapi_applications[i].listen_req[ctrl->cnr - 1]) {
			kfree_skb(hycapi_applications[i].listen_req[ctrl->cnr - 1]);
			hycapi_applications[i].listen_req[ctrl->cnr - 1] = NULL;
		}
	}
	detach_capi_ctr(ctrl);
	ctrl->driverdata = NULL;
	kfree(card->hyctrlinfo);


	card->hyctrlinfo = NULL;
}


static void
hycapi_sendmsg_internal(struct capi_ctr *ctrl, struct sk_buff *skb)
{
	hycapictrl_info *cinfo = (hycapictrl_info *)(ctrl->driverdata);
	hysdn_card *card = cinfo->card;

	spin_lock_irq(&cinfo->lock);
#ifdef HYCAPI_PRINTFNAMES
	printk(KERN_NOTICE "hycapi_send_message\n");
#endif
	cinfo->skbs[cinfo->in_idx++] = skb;	
	if (cinfo->in_idx >= HYSDN_MAX_CAPI_SKB)
		cinfo->in_idx = 0;	
	cinfo->sk_count++;		
	if (cinfo->sk_count >= HYSDN_MAX_CAPI_SKB) {
		
		printk(KERN_ERR "HYSDN Card%d: CAPI-buffer overrun!\n",
		       card->myid);
		capi_ctr_suspend_output(ctrl);
	}
	cinfo->tx_skb = skb;
	spin_unlock_irq(&cinfo->lock);
	schedule_work(&card->irq_queue);
}


static void
hycapi_register_internal(struct capi_ctr *ctrl, __u16 appl,
			 capi_register_params *rp)
{
	char ExtFeatureDefaults[] = "49  /0/0/0/0,*/1,*/2,*/3,*/4,*/5,*/6,*/7,*/8,*/9,*";
	hycapictrl_info *cinfo = (hycapictrl_info *)(ctrl->driverdata);
	hysdn_card *card = cinfo->card;
	struct sk_buff *skb;
	__u16 len;
	__u8 _command = 0xa0, _subcommand = 0x80;
	__u16 MessageNumber = 0x0000;
	__u16 MessageBufferSize = 0;
	int slen = strlen(ExtFeatureDefaults);
#ifdef HYCAPI_PRINTFNAMES
	printk(KERN_NOTICE "hycapi_register_appl\n");
#endif
	MessageBufferSize = rp->level3cnt * rp->datablkcnt * rp->datablklen;

	len = CAPI_MSG_BASELEN + 8 + slen + 1;
	if (!(skb = alloc_skb(len, GFP_ATOMIC))) {
		printk(KERN_ERR "HYSDN card%d: memory squeeze in hycapi_register_appl\n",
		       card->myid);
		return;
	}
	memcpy(skb_put(skb, sizeof(__u16)), &len, sizeof(__u16));
	memcpy(skb_put(skb, sizeof(__u16)), &appl, sizeof(__u16));
	memcpy(skb_put(skb, sizeof(__u8)), &_command, sizeof(_command));
	memcpy(skb_put(skb, sizeof(__u8)), &_subcommand, sizeof(_subcommand));
	memcpy(skb_put(skb, sizeof(__u16)), &MessageNumber, sizeof(__u16));
	memcpy(skb_put(skb, sizeof(__u16)), &MessageBufferSize, sizeof(__u16));
	memcpy(skb_put(skb, sizeof(__u16)), &(rp->level3cnt), sizeof(__u16));
	memcpy(skb_put(skb, sizeof(__u16)), &(rp->datablkcnt), sizeof(__u16));
	memcpy(skb_put(skb, sizeof(__u16)), &(rp->datablklen), sizeof(__u16));
	memcpy(skb_put(skb, slen), ExtFeatureDefaults, slen);
	hycapi_applications[appl - 1].ctrl_mask |= (1 << (ctrl->cnr - 1));
	hycapi_send_message(ctrl, skb);
}


static void hycapi_restart_internal(struct capi_ctr *ctrl)
{
	int i;
	struct sk_buff *skb;
#ifdef HYCAPI_PRINTFNAMES
	printk(KERN_WARNING "HYSDN: hycapi_restart_internal");
#endif
	for (i = 0; i < CAPI_MAXAPPL; i++) {
		if (_hycapi_appCheck(i + 1, ctrl->cnr) == 1) {
			hycapi_register_internal(ctrl, i + 1,
						 &hycapi_applications[i].rp);
			if (hycapi_applications[i].listen_req[ctrl->cnr - 1]) {
				skb = skb_copy(hycapi_applications[i].listen_req[ctrl->cnr - 1], GFP_ATOMIC);
				hycapi_sendmsg_internal(ctrl, skb);
			}
		}
	}
}


static void
hycapi_register_appl(struct capi_ctr *ctrl, __u16 appl,
		     capi_register_params *rp)
{
	int MaxLogicalConnections = 0, MaxBDataBlocks = 0, MaxBDataLen = 0;
	hycapictrl_info *cinfo = (hycapictrl_info *)(ctrl->driverdata);
	hysdn_card *card = cinfo->card;
	int chk = _hycapi_appCheck(appl, ctrl->cnr);
	if (chk < 0) {
		return;
	}
	if (chk == 1) {
		printk(KERN_INFO "HYSDN: apl %d already registered\n", appl);
		return;
	}
	MaxBDataBlocks = rp->datablkcnt > CAPI_MAXDATAWINDOW ? CAPI_MAXDATAWINDOW : rp->datablkcnt;
	rp->datablkcnt = MaxBDataBlocks;
	MaxBDataLen = rp->datablklen < 1024 ? 1024 : rp->datablklen;
	rp->datablklen = MaxBDataLen;

	MaxLogicalConnections = rp->level3cnt;
	if (MaxLogicalConnections < 0) {
		MaxLogicalConnections = card->bchans * -MaxLogicalConnections;
	}
	if (MaxLogicalConnections == 0) {
		MaxLogicalConnections = card->bchans;
	}

	rp->level3cnt = MaxLogicalConnections;
	memcpy(&hycapi_applications[appl - 1].rp,
	       rp, sizeof(capi_register_params));
}


static void hycapi_release_internal(struct capi_ctr *ctrl, __u16 appl)
{
	hycapictrl_info *cinfo = (hycapictrl_info *)(ctrl->driverdata);
	hysdn_card *card = cinfo->card;
	struct sk_buff *skb;
	__u16 len;
	__u8 _command = 0xa1, _subcommand = 0x80;
	__u16 MessageNumber = 0x0000;

	capilib_release_appl(&cinfo->ncci_head, appl);

#ifdef HYCAPI_PRINTFNAMES
	printk(KERN_NOTICE "hycapi_release_appl\n");
#endif
	len = CAPI_MSG_BASELEN;
	if (!(skb = alloc_skb(len, GFP_ATOMIC))) {
		printk(KERN_ERR "HYSDN card%d: memory squeeze in hycapi_register_appl\n",
		       card->myid);
		return;
	}
	memcpy(skb_put(skb, sizeof(__u16)), &len, sizeof(__u16));
	memcpy(skb_put(skb, sizeof(__u16)), &appl, sizeof(__u16));
	memcpy(skb_put(skb, sizeof(__u8)), &_command, sizeof(_command));
	memcpy(skb_put(skb, sizeof(__u8)), &_subcommand, sizeof(_subcommand));
	memcpy(skb_put(skb, sizeof(__u16)), &MessageNumber, sizeof(__u16));
	hycapi_send_message(ctrl, skb);
	hycapi_applications[appl - 1].ctrl_mask &= ~(1 << (ctrl->cnr - 1));
}


static void
hycapi_release_appl(struct capi_ctr *ctrl, __u16 appl)
{
	int chk;

	chk = _hycapi_appCheck(appl, ctrl->cnr);
	if (chk < 0) {
		printk(KERN_ERR "HYCAPI: Releasing invalid appl %d on controller %d\n", appl, ctrl->cnr);
		return;
	}
	if (hycapi_applications[appl - 1].listen_req[ctrl->cnr - 1]) {
		kfree_skb(hycapi_applications[appl - 1].listen_req[ctrl->cnr - 1]);
		hycapi_applications[appl - 1].listen_req[ctrl->cnr - 1] = NULL;
	}
	if (chk == 1)
	{
		hycapi_release_internal(ctrl, appl);
	}
}



int hycapi_capi_release(hysdn_card *card)
{
	hycapictrl_info *cinfo = card->hyctrlinfo;
	struct capi_ctr *ctrl;
#ifdef HYCAPI_PRINTFNAMES
	printk(KERN_NOTICE "hycapi_capi_release\n");
#endif
	if (cinfo) {
		ctrl = &cinfo->capi_ctrl;
		hycapi_remove_ctr(ctrl);
	}
	return 0;
}


int hycapi_capi_stop(hysdn_card *card)
{
	hycapictrl_info *cinfo = card->hyctrlinfo;
	struct capi_ctr *ctrl;
#ifdef HYCAPI_PRINTFNAMES
	printk(KERN_NOTICE "hycapi_capi_stop\n");
#endif
	if (cinfo) {
		ctrl = &cinfo->capi_ctrl;
		capi_ctr_down(ctrl);
	}
	return 0;
}


static u16 hycapi_send_message(struct capi_ctr *ctrl, struct sk_buff *skb)
{
	__u16 appl_id;
	int _len, _len2;
	__u8 msghead[64];
	hycapictrl_info *cinfo = ctrl->driverdata;
	u16 retval = CAPI_NOERROR;

	appl_id = CAPIMSG_APPID(skb->data);
	switch (_hycapi_appCheck(appl_id, ctrl->cnr))
	{
	case 0:
		hycapi_register_internal(ctrl,
					 appl_id,
					 &(hycapi_applications[appl_id - 1].rp));
		break;
	case 1:
		break;
	default:
		printk(KERN_ERR "HYCAPI: Controller mixup!\n");
		retval = CAPI_ILLAPPNR;
		goto out;
	}
	switch (CAPIMSG_CMD(skb->data)) {
	case CAPI_DISCONNECT_B3_RESP:
		capilib_free_ncci(&cinfo->ncci_head, appl_id,
				  CAPIMSG_NCCI(skb->data));
		break;
	case CAPI_DATA_B3_REQ:
		_len = CAPIMSG_LEN(skb->data);
		if (_len > 22) {
			_len2 = _len - 22;
			skb_copy_from_linear_data(skb, msghead, 22);
			skb_copy_to_linear_data_offset(skb, _len2,
						       msghead, 22);
			skb_pull(skb, _len2);
			CAPIMSG_SETLEN(skb->data, 22);
			retval = capilib_data_b3_req(&cinfo->ncci_head,
						     CAPIMSG_APPID(skb->data),
						     CAPIMSG_NCCI(skb->data),
						     CAPIMSG_MSGID(skb->data));
		}
		break;
	case CAPI_LISTEN_REQ:
		if (hycapi_applications[appl_id - 1].listen_req[ctrl->cnr - 1])
		{
			kfree_skb(hycapi_applications[appl_id - 1].listen_req[ctrl->cnr - 1]);
			hycapi_applications[appl_id - 1].listen_req[ctrl->cnr - 1] = NULL;
		}
		if (!(hycapi_applications[appl_id  -1].listen_req[ctrl->cnr - 1] = skb_copy(skb, GFP_ATOMIC)))
		{
			printk(KERN_ERR "HYSDN: memory squeeze in private_listen\n");
		}
		break;
	default:
		break;
	}
out:
	if (retval == CAPI_NOERROR)
		hycapi_sendmsg_internal(ctrl, skb);
	else
		dev_kfree_skb_any(skb);

	return retval;
}

static int hycapi_proc_show(struct seq_file *m, void *v)
{
	struct capi_ctr *ctrl = m->private;
	hycapictrl_info *cinfo = (hycapictrl_info *)(ctrl->driverdata);
	hysdn_card *card = cinfo->card;
	char *s;

	seq_printf(m, "%-16s %s\n", "name", cinfo->cardname);
	seq_printf(m, "%-16s 0x%x\n", "io", card->iobase);
	seq_printf(m, "%-16s %d\n", "irq", card->irq);

	switch (card->brdtype) {
	case BD_PCCARD:  s = "HYSDN Hycard"; break;
	case BD_ERGO: s = "HYSDN Ergo2"; break;
	case BD_METRO: s = "HYSDN Metro4"; break;
	case BD_CHAMP2: s = "HYSDN Champ2";	break;
	case BD_PLEXUS: s = "HYSDN Plexus30"; break;
	default: s = "???"; break;
	}
	seq_printf(m, "%-16s %s\n", "type", s);
	if ((s = cinfo->version[VER_DRIVER]) != NULL)
		seq_printf(m, "%-16s %s\n", "ver_driver", s);
	if ((s = cinfo->version[VER_CARDTYPE]) != NULL)
		seq_printf(m, "%-16s %s\n", "ver_cardtype", s);
	if ((s = cinfo->version[VER_SERIAL]) != NULL)
		seq_printf(m, "%-16s %s\n", "ver_serial", s);

	seq_printf(m, "%-16s %s\n", "cardname", cinfo->cardname);

	return 0;
}

static int hycapi_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, hycapi_proc_show, PDE(inode)->data);
}

static const struct file_operations hycapi_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= hycapi_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


static int hycapi_load_firmware(struct capi_ctr *ctrl, capiloaddata *data)
{
#ifdef HYCAPI_PRINTFNAMES
	printk(KERN_NOTICE "hycapi_load_firmware\n");
#endif
	return 0;
}


static char *hycapi_procinfo(struct capi_ctr *ctrl)
{
	hycapictrl_info *cinfo = (hycapictrl_info *)(ctrl->driverdata);
#ifdef HYCAPI_PRINTFNAMES
	printk(KERN_NOTICE "hycapi_proc_info\n");
#endif
	if (!cinfo)
		return "";
	sprintf(cinfo->infobuf, "%s %s 0x%x %d %s",
		cinfo->cardname[0] ? cinfo->cardname : "-",
		cinfo->version[VER_DRIVER] ? cinfo->version[VER_DRIVER] : "-",
		cinfo->card ? cinfo->card->iobase : 0x0,
		cinfo->card ? cinfo->card->irq : 0,
		hycapi_revision
		);
	return cinfo->infobuf;
}


void
hycapi_rx_capipkt(hysdn_card *card, unsigned char *buf, unsigned short len)
{
	struct sk_buff *skb;
	hycapictrl_info *cinfo = card->hyctrlinfo;
	struct capi_ctr *ctrl;
	__u16 ApplId;
	__u16 MsgLen, info;
	__u16 len2, CapiCmd;
	__u32 CP64[2] = {0, 0};
#ifdef HYCAPI_PRINTFNAMES
	printk(KERN_NOTICE "hycapi_rx_capipkt\n");
#endif
	if (!cinfo) {
		return;
	}
	ctrl = &cinfo->capi_ctrl;
	if (len < CAPI_MSG_BASELEN) {
		printk(KERN_ERR "HYSDN Card%d: invalid CAPI-message, length %d!\n",
		       card->myid, len);
		return;
	}
	MsgLen = CAPIMSG_LEN(buf);
	ApplId = CAPIMSG_APPID(buf);
	CapiCmd = CAPIMSG_CMD(buf);

	if ((CapiCmd == CAPI_DATA_B3_IND) && (MsgLen < 30)) {
		len2 = len + (30 - MsgLen);
		if (!(skb = alloc_skb(len2, GFP_ATOMIC))) {
			printk(KERN_ERR "HYSDN Card%d: incoming packet dropped\n",
			       card->myid);
			return;
		}
		memcpy(skb_put(skb, MsgLen), buf, MsgLen);
		memcpy(skb_put(skb, 2 * sizeof(__u32)), CP64, 2 * sizeof(__u32));
		memcpy(skb_put(skb, len - MsgLen), buf + MsgLen,
		       len - MsgLen);
		CAPIMSG_SETLEN(skb->data, 30);
	} else {
		if (!(skb = alloc_skb(len, GFP_ATOMIC))) {
			printk(KERN_ERR "HYSDN Card%d: incoming packet dropped\n",
			       card->myid);
			return;
		}
		memcpy(skb_put(skb, len), buf, len);
	}
	switch (CAPIMSG_CMD(skb->data))
	{
	case CAPI_CONNECT_B3_CONF:
		info = CAPIMSG_U16(skb->data, 12);
		switch (info)
		{
		case 0:
			capilib_new_ncci(&cinfo->ncci_head, ApplId, CAPIMSG_NCCI(skb->data),
					 hycapi_applications[ApplId - 1].rp.datablkcnt);

			break;
		case 0x0001:
			printk(KERN_ERR "HYSDN Card%d: NCPI not supported by current "
			       "protocol. NCPI ignored.\n", card->myid);
			break;
		case 0x2001:
			printk(KERN_ERR "HYSDN Card%d: Message not supported in"
			       " current state\n", card->myid);
			break;
		case 0x2002:
			printk(KERN_ERR "HYSDN Card%d: invalid PLCI\n", card->myid);
			break;
		case 0x2004:
			printk(KERN_ERR "HYSDN Card%d: out of NCCI\n", card->myid);
			break;
		case 0x3008:
			printk(KERN_ERR "HYSDN Card%d: NCPI not supported\n",
			       card->myid);
			break;
		default:
			printk(KERN_ERR "HYSDN Card%d: Info in CONNECT_B3_CONF: %d\n",
			       card->myid, info);
			break;
		}
		break;
	case CAPI_CONNECT_B3_IND:
		capilib_new_ncci(&cinfo->ncci_head, ApplId,
				 CAPIMSG_NCCI(skb->data),
				 hycapi_applications[ApplId - 1].rp.datablkcnt);
		break;
	case CAPI_DATA_B3_CONF:
		capilib_data_b3_conf(&cinfo->ncci_head, ApplId,
				     CAPIMSG_NCCI(skb->data),
				     CAPIMSG_MSGID(skb->data));
		break;
	default:
		break;
	}
	capi_ctr_handle_message(ctrl, ApplId, skb);
}


void hycapi_tx_capiack(hysdn_card *card)
{
	hycapictrl_info *cinfo = card->hyctrlinfo;
#ifdef HYCAPI_PRINTFNAMES
	printk(KERN_NOTICE "hycapi_tx_capiack\n");
#endif
	if (!cinfo) {
		return;
	}
	spin_lock_irq(&cinfo->lock);
	kfree_skb(cinfo->skbs[cinfo->out_idx]);		
	cinfo->skbs[cinfo->out_idx++] = NULL;
	if (cinfo->out_idx >= HYSDN_MAX_CAPI_SKB)
		cinfo->out_idx = 0;	

	if (cinfo->sk_count-- == HYSDN_MAX_CAPI_SKB)	
		capi_ctr_resume_output(&cinfo->capi_ctrl);
	spin_unlock_irq(&cinfo->lock);
}


struct sk_buff *
hycapi_tx_capiget(hysdn_card *card)
{
	hycapictrl_info *cinfo = card->hyctrlinfo;
	if (!cinfo) {
		return (struct sk_buff *)NULL;
	}
	if (!cinfo->sk_count)
		return (struct sk_buff *)NULL;	

	return (cinfo->skbs[cinfo->out_idx]);		
}



int hycapi_init(void)
{
	int i;
	for (i = 0; i < CAPI_MAXAPPL; i++) {
		memset(&(hycapi_applications[i]), 0, sizeof(hycapi_appl));
	}
	return (0);
}


void
hycapi_cleanup(void)
{
}


static void hycapi_fill_profile(hysdn_card *card)
{
	hycapictrl_info *cinfo = NULL;
	struct capi_ctr *ctrl = NULL;
	cinfo = card->hyctrlinfo;
	if (!cinfo) return;
	ctrl = &cinfo->capi_ctrl;
	strcpy(ctrl->manu, "Hypercope");
	ctrl->version.majorversion = 2;
	ctrl->version.minorversion = 0;
	ctrl->version.majormanuversion = 3;
	ctrl->version.minormanuversion = 2;
	ctrl->profile.ncontroller = card->myid;
	ctrl->profile.nbchannel = card->bchans;
	ctrl->profile.goptions = GLOBAL_OPTION_INTERNAL_CONTROLLER |
		GLOBAL_OPTION_B_CHANNEL_OPERATION;
	ctrl->profile.support1 =  B1_PROT_64KBIT_HDLC |
		(card->faxchans ? B1_PROT_T30 : 0) |
		B1_PROT_64KBIT_TRANSPARENT;
	ctrl->profile.support2 = B2_PROT_ISO7776 |
		(card->faxchans ? B2_PROT_T30 : 0) |
		B2_PROT_TRANSPARENT;
	ctrl->profile.support3 = B3_PROT_TRANSPARENT |
		B3_PROT_T90NL |
		(card->faxchans ? B3_PROT_T30 : 0) |
		(card->faxchans ? B3_PROT_T30EXT : 0) |
		B3_PROT_ISO8208;
}

int
hycapi_capi_create(hysdn_card *card)
{
	hycapictrl_info *cinfo = NULL;
	struct capi_ctr *ctrl = NULL;
	int retval;
#ifdef HYCAPI_PRINTFNAMES
	printk(KERN_NOTICE "hycapi_capi_create\n");
#endif
	if ((hycapi_enable & (1 << card->myid)) == 0) {
		return 1;
	}
	if (!card->hyctrlinfo) {
		cinfo = kzalloc(sizeof(hycapictrl_info), GFP_ATOMIC);
		if (!cinfo) {
			printk(KERN_WARNING "HYSDN: no memory for capi-ctrl.\n");
			return -ENOMEM;
		}
		card->hyctrlinfo = cinfo;
		cinfo->card = card;
		spin_lock_init(&cinfo->lock);
		INIT_LIST_HEAD(&cinfo->ncci_head);

		switch (card->brdtype) {
		case BD_PCCARD:  strcpy(cinfo->cardname, "HYSDN Hycard"); break;
		case BD_ERGO: strcpy(cinfo->cardname, "HYSDN Ergo2"); break;
		case BD_METRO: strcpy(cinfo->cardname, "HYSDN Metro4"); break;
		case BD_CHAMP2: strcpy(cinfo->cardname, "HYSDN Champ2"); break;
		case BD_PLEXUS: strcpy(cinfo->cardname, "HYSDN Plexus30"); break;
		default: strcpy(cinfo->cardname, "HYSDN ???"); break;
		}

		ctrl = &cinfo->capi_ctrl;
		ctrl->driver_name   = "hycapi";
		ctrl->driverdata    = cinfo;
		ctrl->register_appl = hycapi_register_appl;
		ctrl->release_appl  = hycapi_release_appl;
		ctrl->send_message  = hycapi_send_message;
		ctrl->load_firmware = hycapi_load_firmware;
		ctrl->reset_ctr     = hycapi_reset_ctr;
		ctrl->procinfo      = hycapi_procinfo;
		ctrl->proc_fops = &hycapi_proc_fops;
		strcpy(ctrl->name, cinfo->cardname);
		ctrl->owner = THIS_MODULE;

		retval = attach_capi_ctr(ctrl);
		if (retval) {
			printk(KERN_ERR "hycapi: attach controller failed.\n");
			return -EBUSY;
		}
		
		hycapi_fill_profile(card);
		capi_ctr_ready(ctrl);
	} else {
		
		ctrl = &card->hyctrlinfo->capi_ctrl;
		hycapi_fill_profile(card);
		capi_ctr_ready(ctrl);
		hycapi_restart_internal(ctrl);
	}
	return 0;
}
