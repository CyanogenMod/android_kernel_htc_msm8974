/*
 * Stuff used by all variants of the driver
 *
 * Copyright (c) 2001 by Stefan Eilers,
 *                       Hansjoerg Lipp <hjlipp@web.de>,
 *                       Tilman Schmidt <tilman@imap.cc>.
 *
 * =====================================================================
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation; either version 2 of
 *	the License, or (at your option) any later version.
 * =====================================================================
 */

#include "gigaset.h"
#include <linux/isdnif.h>
#include <linux/export.h>

#define SBUFSIZE	4096	
#define TRANSBUFSIZE	768	
#define HW_HDR_LEN	2	
#define MAX_BUF_SIZE	(SBUFSIZE - HW_HDR_LEN)	


static int writebuf_from_LL(int driverID, int channel, int ack,
			    struct sk_buff *skb)
{
	struct cardstate *cs = gigaset_get_cs_by_id(driverID);
	struct bc_state *bcs;
	unsigned char *ack_header;
	unsigned len;

	if (!cs) {
		pr_err("%s: invalid driver ID (%d)\n", __func__, driverID);
		return -ENODEV;
	}
	if (channel < 0 || channel >= cs->channels) {
		dev_err(cs->dev, "%s: invalid channel ID (%d)\n",
			__func__, channel);
		return -ENODEV;
	}
	bcs = &cs->bcs[channel];

	
	if (skb_linearize(skb) < 0) {
		dev_err(cs->dev, "%s: skb_linearize failed\n", __func__);
		return -ENOMEM;
	}
	len = skb->len;

	gig_dbg(DEBUG_LLDATA,
		"Receiving data from LL (id: %d, ch: %d, ack: %d, sz: %d)",
		driverID, channel, ack, len);

	if (!len) {
		if (ack)
			dev_notice(cs->dev, "%s: not ACKing empty packet\n",
				   __func__);
		return 0;
	}
	if (len > MAX_BUF_SIZE) {
		dev_err(cs->dev, "%s: packet too large (%d bytes)\n",
			__func__, len);
		return -EINVAL;
	}

	
	if (skb_headroom(skb) < HW_HDR_LEN) {
		
		dev_err(cs->dev, "%s: insufficient skb headroom\n", __func__);
		return -ENOMEM;
	}
	skb_set_mac_header(skb, -HW_HDR_LEN);
	skb->mac_len = HW_HDR_LEN;
	ack_header = skb_mac_header(skb);
	if (ack) {
		ack_header[0] = len & 0xff;
		ack_header[1] = len >> 8;
	} else {
		ack_header[0] = ack_header[1] = 0;
	}
	gig_dbg(DEBUG_MCMD, "skb: len=%u, ack=%d: %02x %02x",
		len, ack, ack_header[0], ack_header[1]);

	
	return cs->ops->send_skb(bcs, skb);
}

void gigaset_skb_sent(struct bc_state *bcs, struct sk_buff *skb)
{
	isdn_if *iif = bcs->cs->iif;
	unsigned char *ack_header = skb_mac_header(skb);
	unsigned len;
	isdn_ctrl response;

	++bcs->trans_up;

	if (skb->len)
		dev_warn(bcs->cs->dev, "%s: skb->len==%d\n",
			 __func__, skb->len);

	len = ack_header[0] + ((unsigned) ack_header[1] << 8);
	if (len) {
		gig_dbg(DEBUG_MCMD, "ACKing to LL (id: %d, ch: %d, sz: %u)",
			bcs->cs->myid, bcs->channel, len);

		response.driver = bcs->cs->myid;
		response.command = ISDN_STAT_BSENT;
		response.arg = bcs->channel;
		response.parm.length = len;
		iif->statcallb(&response);
	}
}
EXPORT_SYMBOL_GPL(gigaset_skb_sent);

void gigaset_skb_rcvd(struct bc_state *bcs, struct sk_buff *skb)
{
	isdn_if *iif = bcs->cs->iif;

	iif->rcvcallb_skb(bcs->cs->myid, bcs->channel, skb);
	bcs->trans_down++;
}
EXPORT_SYMBOL_GPL(gigaset_skb_rcvd);

void gigaset_isdn_rcv_err(struct bc_state *bcs)
{
	isdn_if *iif = bcs->cs->iif;
	isdn_ctrl response;

	
	if (bcs->ignore) {
		bcs->ignore--;
		return;
	}

	
	bcs->corrupted++;

	
	gig_dbg(DEBUG_CMD, "sending L1ERR");
	response.driver = bcs->cs->myid;
	response.command = ISDN_STAT_L1ERR;
	response.arg = bcs->channel;
	response.parm.errcode = ISDN_STAT_L1ERR_RECV;
	iif->statcallb(&response);
}
EXPORT_SYMBOL_GPL(gigaset_isdn_rcv_err);

static int command_from_LL(isdn_ctrl *cntrl)
{
	struct cardstate *cs;
	struct bc_state *bcs;
	int retval = 0;
	char **commands;
	int ch;
	int i;
	size_t l;

	gig_dbg(DEBUG_CMD, "driver: %d, command: %d, arg: 0x%lx",
		cntrl->driver, cntrl->command, cntrl->arg);

	cs = gigaset_get_cs_by_id(cntrl->driver);
	if (cs == NULL) {
		pr_err("%s: invalid driver ID (%d)\n", __func__, cntrl->driver);
		return -ENODEV;
	}
	ch = cntrl->arg & 0xff;

	switch (cntrl->command) {
	case ISDN_CMD_IOCTL:
		dev_warn(cs->dev, "ISDN_CMD_IOCTL not supported\n");
		return -EINVAL;

	case ISDN_CMD_DIAL:
		gig_dbg(DEBUG_CMD,
			"ISDN_CMD_DIAL (phone: %s, msn: %s, si1: %d, si2: %d)",
			cntrl->parm.setup.phone, cntrl->parm.setup.eazmsn,
			cntrl->parm.setup.si1, cntrl->parm.setup.si2);

		if (ch >= cs->channels) {
			dev_err(cs->dev,
				"ISDN_CMD_DIAL: invalid channel (%d)\n", ch);
			return -EINVAL;
		}
		bcs = cs->bcs + ch;
		if (!gigaset_get_channel(bcs)) {
			dev_err(cs->dev, "ISDN_CMD_DIAL: channel not free\n");
			return -EBUSY;
		}
		switch (bcs->proto2) {
		case L2_HDLC:
			bcs->rx_bufsize = SBUFSIZE;
			break;
		default:			
			bcs->rx_bufsize = TRANSBUFSIZE;
		}
		dev_kfree_skb(bcs->rx_skb);
		gigaset_new_rx_skb(bcs);

		commands = kzalloc(AT_NUM * (sizeof *commands), GFP_ATOMIC);
		if (!commands) {
			gigaset_free_channel(bcs);
			dev_err(cs->dev, "ISDN_CMD_DIAL: out of memory\n");
			return -ENOMEM;
		}

		l = 3 + strlen(cntrl->parm.setup.phone);
		commands[AT_DIAL] = kmalloc(l, GFP_ATOMIC);
		if (!commands[AT_DIAL])
			goto oom;
		if (cntrl->parm.setup.phone[0] == '*' &&
		    cntrl->parm.setup.phone[1] == '*') {
			
			commands[AT_TYPE] = kstrdup("^SCTP=0\r", GFP_ATOMIC);
			if (!commands[AT_TYPE])
				goto oom;
			snprintf(commands[AT_DIAL], l,
				 "D%s\r", cntrl->parm.setup.phone + 2);
		} else {
			commands[AT_TYPE] = kstrdup("^SCTP=1\r", GFP_ATOMIC);
			if (!commands[AT_TYPE])
				goto oom;
			snprintf(commands[AT_DIAL], l,
				 "D%s\r", cntrl->parm.setup.phone);
		}

		l = strlen(cntrl->parm.setup.eazmsn);
		if (l) {
			l += 8;
			commands[AT_MSN] = kmalloc(l, GFP_ATOMIC);
			if (!commands[AT_MSN])
				goto oom;
			snprintf(commands[AT_MSN], l, "^SMSN=%s\r",
				 cntrl->parm.setup.eazmsn);
		}

		switch (cntrl->parm.setup.si1) {
		case 1:		
			
			commands[AT_BC] = kstrdup("^SBC=9090A3\r", GFP_ATOMIC);
			if (!commands[AT_BC])
				goto oom;
			break;
		case 7:		
		default:	
			
			commands[AT_BC] = kstrdup("^SBC=8890\r", GFP_ATOMIC);
			if (!commands[AT_BC])
				goto oom;
		}
		

		commands[AT_PROTO] = kmalloc(9, GFP_ATOMIC);
		if (!commands[AT_PROTO])
			goto oom;
		snprintf(commands[AT_PROTO], 9, "^SBPR=%u\r", bcs->proto2);

		commands[AT_ISO] = kmalloc(9, GFP_ATOMIC);
		if (!commands[AT_ISO])
			goto oom;
		snprintf(commands[AT_ISO], 9, "^SISO=%u\r",
			 (unsigned) bcs->channel + 1);

		if (!gigaset_add_event(cs, &bcs->at_state, EV_DIAL, commands,
				       bcs->at_state.seq_index, NULL)) {
			for (i = 0; i < AT_NUM; ++i)
				kfree(commands[i]);
			kfree(commands);
			gigaset_free_channel(bcs);
			return -ENOMEM;
		}
		gigaset_schedule_event(cs);
		break;
	case ISDN_CMD_ACCEPTD:
		gig_dbg(DEBUG_CMD, "ISDN_CMD_ACCEPTD");
		if (ch >= cs->channels) {
			dev_err(cs->dev,
				"ISDN_CMD_ACCEPTD: invalid channel (%d)\n", ch);
			return -EINVAL;
		}
		bcs = cs->bcs + ch;
		switch (bcs->proto2) {
		case L2_HDLC:
			bcs->rx_bufsize = SBUFSIZE;
			break;
		default:			
			bcs->rx_bufsize = TRANSBUFSIZE;
		}
		dev_kfree_skb(bcs->rx_skb);
		gigaset_new_rx_skb(bcs);
		if (!gigaset_add_event(cs, &bcs->at_state,
				       EV_ACCEPT, NULL, 0, NULL))
			return -ENOMEM;
		gigaset_schedule_event(cs);

		break;
	case ISDN_CMD_HANGUP:
		gig_dbg(DEBUG_CMD, "ISDN_CMD_HANGUP");
		if (ch >= cs->channels) {
			dev_err(cs->dev,
				"ISDN_CMD_HANGUP: invalid channel (%d)\n", ch);
			return -EINVAL;
		}
		bcs = cs->bcs + ch;
		if (!gigaset_add_event(cs, &bcs->at_state,
				       EV_HUP, NULL, 0, NULL))
			return -ENOMEM;
		gigaset_schedule_event(cs);

		break;
	case ISDN_CMD_CLREAZ: 
		dev_info(cs->dev, "ignoring ISDN_CMD_CLREAZ\n");
		break;
	case ISDN_CMD_SETEAZ: 
		dev_info(cs->dev, "ignoring ISDN_CMD_SETEAZ (%s)\n",
			 cntrl->parm.num);
		break;
	case ISDN_CMD_SETL2: 
		if (ch >= cs->channels) {
			dev_err(cs->dev,
				"ISDN_CMD_SETL2: invalid channel (%d)\n", ch);
			return -EINVAL;
		}
		bcs = cs->bcs + ch;
		if (bcs->chstate & CHS_D_UP) {
			dev_err(cs->dev,
				"ISDN_CMD_SETL2: channel active (%d)\n", ch);
			return -EINVAL;
		}
		switch (cntrl->arg >> 8) {
		case ISDN_PROTO_L2_HDLC:
			gig_dbg(DEBUG_CMD, "ISDN_CMD_SETL2: setting L2_HDLC");
			bcs->proto2 = L2_HDLC;
			break;
		case ISDN_PROTO_L2_TRANS:
			gig_dbg(DEBUG_CMD, "ISDN_CMD_SETL2: setting L2_VOICE");
			bcs->proto2 = L2_VOICE;
			break;
		default:
			dev_err(cs->dev,
				"ISDN_CMD_SETL2: unsupported protocol (%lu)\n",
				cntrl->arg >> 8);
			return -EINVAL;
		}
		break;
	case ISDN_CMD_SETL3: 
		gig_dbg(DEBUG_CMD, "ISDN_CMD_SETL3");
		if (ch >= cs->channels) {
			dev_err(cs->dev,
				"ISDN_CMD_SETL3: invalid channel (%d)\n", ch);
			return -EINVAL;
		}

		if (cntrl->arg >> 8 != ISDN_PROTO_L3_TRANS) {
			dev_err(cs->dev,
				"ISDN_CMD_SETL3: unsupported protocol (%lu)\n",
				cntrl->arg >> 8);
			return -EINVAL;
		}

		break;

	default:
		gig_dbg(DEBUG_CMD, "unknown command %d from LL",
			cntrl->command);
		return -EINVAL;
	}

	return retval;

oom:
	dev_err(bcs->cs->dev, "out of memory\n");
	for (i = 0; i < AT_NUM; ++i)
		kfree(commands[i]);
	kfree(commands);
	gigaset_free_channel(bcs);
	return -ENOMEM;
}

static void gigaset_i4l_cmd(struct cardstate *cs, int cmd)
{
	isdn_if *iif = cs->iif;
	isdn_ctrl command;

	command.driver = cs->myid;
	command.command = cmd;
	command.arg = 0;
	iif->statcallb(&command);
}

static void gigaset_i4l_channel_cmd(struct bc_state *bcs, int cmd)
{
	isdn_if *iif = bcs->cs->iif;
	isdn_ctrl command;

	command.driver = bcs->cs->myid;
	command.command = cmd;
	command.arg = bcs->channel;
	iif->statcallb(&command);
}

int gigaset_isdn_icall(struct at_state_t *at_state)
{
	struct cardstate *cs = at_state->cs;
	struct bc_state *bcs = at_state->bcs;
	isdn_if *iif = cs->iif;
	isdn_ctrl response;
	int retval;

	
	response.parm.setup.si1 = 0;	
	response.parm.setup.si2 = 0;
	response.parm.setup.screen = 0;
	response.parm.setup.plan = 0;
	if (!at_state->str_var[STR_ZBC]) {
		
		response.parm.setup.si1 = 1;
	} else if (!strcmp(at_state->str_var[STR_ZBC], "8890")) {
		
		response.parm.setup.si1 = 7;
	} else if (!strcmp(at_state->str_var[STR_ZBC], "8090A3")) {
		
		response.parm.setup.si1 = 1;
	} else if (!strcmp(at_state->str_var[STR_ZBC], "9090A3")) {
		
		response.parm.setup.si1 = 1;
		response.parm.setup.si2 = 2;
	} else {
		dev_warn(cs->dev, "RING ignored - unsupported BC %s\n",
			 at_state->str_var[STR_ZBC]);
		return ICALL_IGNORE;
	}
	if (at_state->str_var[STR_NMBR]) {
		strlcpy(response.parm.setup.phone, at_state->str_var[STR_NMBR],
			sizeof response.parm.setup.phone);
	} else
		response.parm.setup.phone[0] = 0;
	if (at_state->str_var[STR_ZCPN]) {
		strlcpy(response.parm.setup.eazmsn, at_state->str_var[STR_ZCPN],
			sizeof response.parm.setup.eazmsn);
	} else
		response.parm.setup.eazmsn[0] = 0;

	if (!bcs) {
		dev_notice(cs->dev, "no channel for incoming call\n");
		response.command = ISDN_STAT_ICALLW;
		response.arg = 0;
	} else {
		gig_dbg(DEBUG_CMD, "Sending ICALL");
		response.command = ISDN_STAT_ICALL;
		response.arg = bcs->channel;
	}
	response.driver = cs->myid;
	retval = iif->statcallb(&response);
	gig_dbg(DEBUG_CMD, "Response: %d", retval);
	switch (retval) {
	case 0:	
		return ICALL_IGNORE;
	case 1:	
		bcs->chstate |= CHS_NOTIFY_LL;
		return ICALL_ACCEPT;
	case 2:	
		return ICALL_REJECT;
	case 3:	
		dev_warn(cs->dev,
			 "LL requested unsupported feature: Incomplete Number\n");
		return ICALL_IGNORE;
	case 4:	
		return ICALL_ACCEPT;
	case 5:	
		dev_warn(cs->dev,
			 "LL requested unsupported feature: Call Deflection\n");
		return ICALL_IGNORE;
	default:
		dev_err(cs->dev, "LL error %d on ICALL\n", retval);
		return ICALL_IGNORE;
	}
}

void gigaset_isdn_connD(struct bc_state *bcs)
{
	gig_dbg(DEBUG_CMD, "sending DCONN");
	gigaset_i4l_channel_cmd(bcs, ISDN_STAT_DCONN);
}

void gigaset_isdn_hupD(struct bc_state *bcs)
{
	gig_dbg(DEBUG_CMD, "sending DHUP");
	gigaset_i4l_channel_cmd(bcs, ISDN_STAT_DHUP);
}

void gigaset_isdn_connB(struct bc_state *bcs)
{
	gig_dbg(DEBUG_CMD, "sending BCONN");
	gigaset_i4l_channel_cmd(bcs, ISDN_STAT_BCONN);
}

void gigaset_isdn_hupB(struct bc_state *bcs)
{
	gig_dbg(DEBUG_CMD, "sending BHUP");
	gigaset_i4l_channel_cmd(bcs, ISDN_STAT_BHUP);
}

void gigaset_isdn_start(struct cardstate *cs)
{
	gig_dbg(DEBUG_CMD, "sending RUN");
	gigaset_i4l_cmd(cs, ISDN_STAT_RUN);
}

void gigaset_isdn_stop(struct cardstate *cs)
{
	gig_dbg(DEBUG_CMD, "sending STOP");
	gigaset_i4l_cmd(cs, ISDN_STAT_STOP);
}

int gigaset_isdn_regdev(struct cardstate *cs, const char *isdnid)
{
	isdn_if *iif;

	iif = kmalloc(sizeof *iif, GFP_KERNEL);
	if (!iif) {
		pr_err("out of memory\n");
		return 0;
	}

	if (snprintf(iif->id, sizeof iif->id, "%s_%u", isdnid, cs->minor_index)
	    >= sizeof iif->id) {
		pr_err("ID too long: %s\n", isdnid);
		kfree(iif);
		return 0;
	}

	iif->owner = THIS_MODULE;
	iif->channels = cs->channels;
	iif->maxbufsize = MAX_BUF_SIZE;
	iif->features = ISDN_FEATURE_L2_TRANS |
		ISDN_FEATURE_L2_HDLC |
		ISDN_FEATURE_L2_X75I |
		ISDN_FEATURE_L3_TRANS |
		ISDN_FEATURE_P_EURO;
	iif->hl_hdrlen = HW_HDR_LEN;		
	iif->command = command_from_LL;
	iif->writebuf_skb = writebuf_from_LL;
	iif->writecmd = NULL;			
	iif->readstat = NULL;			
	iif->rcvcallb_skb = NULL;		
	iif->statcallb = NULL;			

	if (!register_isdn(iif)) {
		pr_err("register_isdn failed\n");
		kfree(iif);
		return 0;
	}

	cs->iif = iif;
	cs->myid = iif->channels;		
	cs->hw_hdr_len = HW_HDR_LEN;
	return 1;
}

void gigaset_isdn_unregdev(struct cardstate *cs)
{
	gig_dbg(DEBUG_CMD, "sending UNLOAD");
	gigaset_i4l_cmd(cs, ISDN_STAT_UNLOAD);
	kfree(cs->iif);
	cs->iif = NULL;
}

void gigaset_isdn_regdrv(void)
{
	pr_info("ISDN4Linux interface\n");
	
}

void gigaset_isdn_unregdrv(void)
{
	
}
