/*
 * Kernel CAPI interface for the Gigaset driver
 *
 * Copyright (c) 2009 by Tilman Schmidt <tilman@imap.cc>.
 *
 * =====================================================================
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation; either version 2 of
 *	the License, or (at your option) any later version.
 * =====================================================================
 */

#include "gigaset.h"
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/isdn/capilli.h>
#include <linux/isdn/capicmd.h>
#include <linux/isdn/capiutil.h>
#include <linux/export.h>

#define CapiNcpiNotSupportedByProtocol	0x0001
#define CapiFlagsNotSupportedByProtocol	0x0002
#define CapiAlertAlreadySent		0x0003
#define CapiFacilitySpecificFunctionNotSupported	0x3011

#define CAPI_CONNECT_IND_BASELEN	(CAPI_MSG_BASELEN + 4 + 2 + 8 * 1)
#define CAPI_CONNECT_ACTIVE_IND_BASELEN	(CAPI_MSG_BASELEN + 4 + 3 * 1)
#define CAPI_CONNECT_B3_IND_BASELEN	(CAPI_MSG_BASELEN + 4 + 1)
#define CAPI_CONNECT_B3_ACTIVE_IND_BASELEN	(CAPI_MSG_BASELEN + 4 + 1)
#define CAPI_DATA_B3_REQ_LEN64		(CAPI_MSG_BASELEN + 4 + 4 + 2 + 2 + 2 + 8)
#define CAPI_DATA_B3_CONF_LEN		(CAPI_MSG_BASELEN + 4 + 2 + 2)
#define CAPI_DISCONNECT_IND_LEN		(CAPI_MSG_BASELEN + 4 + 2)
#define CAPI_DISCONNECT_B3_IND_BASELEN	(CAPI_MSG_BASELEN + 4 + 2 + 1)
#define CAPI_FACILITY_CONF_BASELEN	(CAPI_MSG_BASELEN + 4 + 2 + 2 + 1)
#define CAPI_STDCONF_LEN		(CAPI_MSG_BASELEN + 4 + 2)

#define CAPI_FACILITY_HANDSET	0x0000
#define CAPI_FACILITY_DTMF	0x0001
#define CAPI_FACILITY_V42BIS	0x0002
#define CAPI_FACILITY_SUPPSVC	0x0003
#define CAPI_FACILITY_WAKEUP	0x0004
#define CAPI_FACILITY_LI	0x0005

#define CAPI_SUPPSVC_GETSUPPORTED	0x0000
#define CAPI_SUPPSVC_LISTEN		0x0001

#define CAPIMSG_PLCI_PART(m)	CAPIMSG_U8(m, 9)
#define CAPIMSG_NCCI_PART(m)	CAPIMSG_U16(m, 10)
#define CAPIMSG_HANDLE_REQ(m)	CAPIMSG_U16(m, 18) 
#define CAPIMSG_FLAGS(m)	CAPIMSG_U16(m, 20)
#define CAPIMSG_SETCONTROLLER(m, contr)	capimsg_setu8(m, 8, contr)
#define CAPIMSG_SETPLCI_PART(m, plci)	capimsg_setu8(m, 9, plci)
#define CAPIMSG_SETNCCI_PART(m, ncci)	capimsg_setu16(m, 10, ncci)
#define CAPIMSG_SETFLAGS(m, flags)	capimsg_setu16(m, 20, flags)

#define CAPIMSG_SETHANDLE_CONF(m, handle)	capimsg_setu16(m, 12, handle)
#define	CAPIMSG_SETINFO_CONF(m, info)		capimsg_setu16(m, 14, info)

#define CAPI_FLAGS_DELIVERY_CONFIRMATION	0x04
#define CAPI_FLAGS_RESERVED			(~0x1f)

#define MAX_BC_OCTETS 11
#define MAX_HLC_OCTETS 3
#define MAX_NUMBER_DIGITS 20
#define MAX_FMT_IE_LEN 20

#define APCONN_NONE	0	
#define APCONN_SETUP	1	
#define APCONN_ACTIVE	2	

struct gigaset_capi_appl {
	struct list_head ctrlist;
	struct gigaset_capi_appl *bcnext;
	u16 id;
	struct capi_register_params rp;
	u16 nextMessageNumber;
	u32 listenInfoMask;
	u32 listenCIPmask;
};

struct gigaset_capi_ctr {
	struct capi_ctr ctr;
	struct list_head appls;
	struct sk_buff_head sendqueue;
	atomic_t sendqlen;
	
	_cmsg hcmsg;	
	_cmsg acmsg;	
	u8 bc_buf[MAX_BC_OCTETS + 1];
	u8 hlc_buf[MAX_HLC_OCTETS + 1];
	u8 cgpty_buf[MAX_NUMBER_DIGITS + 3];
	u8 cdpty_buf[MAX_NUMBER_DIGITS + 2];
};

static struct {
	u8 *bc;
	u8 *hlc;
} cip2bchlc[] = {
	[1] = { "8090A3", NULL },
	
	[2] = { "8890", NULL },
	
	[3] = { "8990", NULL },
	
	[4] = { "9090A3", NULL },
	
	[5] = { "9190", NULL },
	
	[6] = { "9890", NULL },
	
	[7] = { "88C0C6E6", NULL },
	
	[8] = { "8890218F", NULL },
	
	[9] = { "9190A5", NULL },
	
	[16] = { "8090A3", "9181" },
	
	[17] = { "9090A3", "9184" },
	
	[18] = { "8890", "91A1" },
	
	[19] = { "8890", "91A4" },
	[20] = { "8890", "91A8" },
	
	[21] = { "8890", "91B1" },
	
	[22] = { "8890", "91B2" },
	
	[23] = { "8890", "91B5" },
	
	[24] = { "8890", "91B8" },
	
	[25] = { "8890", "91C1" },
	
	[26] = { "9190A5", "9181" },
	
	[27] = { "9190A5", "916001" },
	
	[28] = { "8890", "916002" },
	
};


static inline void ignore_cstruct_param(struct cardstate *cs, _cstruct param,
					char *msgname, char *paramname)
{
	if (param && *param)
		dev_warn(cs->dev, "%s: ignoring unsupported parameter: %s\n",
			 msgname, paramname);
}

static int encode_ie(char *in, u8 *out, int maxlen)
{
	int l = 0;
	while (*in) {
		if (!isxdigit(in[0]) || !isxdigit(in[1]) || l >= maxlen)
			return -1;
		out[++l] = (hex_to_bin(in[0]) << 4) + hex_to_bin(in[1]);
		in += 2;
	}
	out[0] = l;
	return l;
}

static void decode_ie(u8 *in, char *out)
{
	int i = *in;
	while (i-- > 0) {
		
		*out++ = toupper(hex_asc_hi(*++in));
		*out++ = toupper(hex_asc_lo(*in));
	}
}

static inline struct gigaset_capi_appl *
get_appl(struct gigaset_capi_ctr *iif, u16 appl)
{
	struct gigaset_capi_appl *ap;

	list_for_each_entry(ap, &iif->appls, ctrlist)
		if (ap->id == appl)
			return ap;
	return NULL;
}

static inline void dump_cmsg(enum debuglevel level, const char *tag, _cmsg *p)
{
#ifdef CONFIG_GIGASET_DEBUG
	_cdebbuf *cdb;

	if (!(gigaset_debuglevel & level))
		return;

	cdb = capi_cmsg2str(p);
	if (cdb) {
		gig_dbg(level, "%s: [%d] %s", tag, p->ApplId, cdb->buf);
		cdebbuf_free(cdb);
	} else {
		gig_dbg(level, "%s: [%d] %s", tag, p->ApplId,
			capi_cmd2str(p->Command, p->Subcommand));
	}
#endif
}

static inline void dump_rawmsg(enum debuglevel level, const char *tag,
			       unsigned char *data)
{
#ifdef CONFIG_GIGASET_DEBUG
	char *dbgline;
	int i, l;

	if (!(gigaset_debuglevel & level))
		return;

	l = CAPIMSG_LEN(data);
	if (l < 12) {
		gig_dbg(level, "%s: ??? LEN=%04d", tag, l);
		return;
	}
	gig_dbg(level, "%s: 0x%02x:0x%02x: ID=%03d #0x%04x LEN=%04d NCCI=0x%x",
		tag, CAPIMSG_COMMAND(data), CAPIMSG_SUBCOMMAND(data),
		CAPIMSG_APPID(data), CAPIMSG_MSGID(data), l,
		CAPIMSG_CONTROL(data));
	l -= 12;
	dbgline = kmalloc(3 * l, GFP_ATOMIC);
	if (!dbgline)
		return;
	for (i = 0; i < l; i++) {
		dbgline[3 * i] = hex_asc_hi(data[12 + i]);
		dbgline[3 * i + 1] = hex_asc_lo(data[12 + i]);
		dbgline[3 * i + 2] = ' ';
	}
	dbgline[3 * l - 1] = '\0';
	gig_dbg(level, "  %s", dbgline);
	kfree(dbgline);
	if (CAPIMSG_COMMAND(data) == CAPI_DATA_B3 &&
	    (CAPIMSG_SUBCOMMAND(data) == CAPI_REQ ||
	     CAPIMSG_SUBCOMMAND(data) == CAPI_IND)) {
		l = CAPIMSG_DATALEN(data);
		gig_dbg(level, "   DataLength=%d", l);
		if (l <= 0 || !(gigaset_debuglevel & DEBUG_LLDATA))
			return;
		if (l > 64)
			l = 64; 
		dbgline = kmalloc(3 * l, GFP_ATOMIC);
		if (!dbgline)
			return;
		data += CAPIMSG_LEN(data);
		for (i = 0; i < l; i++) {
			dbgline[3 * i] = hex_asc_hi(data[i]);
			dbgline[3 * i + 1] = hex_asc_lo(data[i]);
			dbgline[3 * i + 2] = ' ';
		}
		dbgline[3 * l - 1] = '\0';
		gig_dbg(level, "  %s", dbgline);
		kfree(dbgline);
	}
#endif
}


static const char *format_ie(const char *ie)
{
	static char result[3 * MAX_FMT_IE_LEN];
	int len, count;
	char *pout = result;

	if (!ie)
		return "NULL";

	count = len = ie[0];
	if (count > MAX_FMT_IE_LEN)
		count = MAX_FMT_IE_LEN - 1;
	while (count--) {
		*pout++ = hex_asc_hi(*++ie);
		*pout++ = hex_asc_lo(*ie);
		*pout++ = ' ';
	}
	if (len > MAX_FMT_IE_LEN) {
		*pout++ = '.';
		*pout++ = '.';
		*pout++ = '.';
	}
	*--pout = 0;
	return result;
}

static void send_data_b3_conf(struct cardstate *cs, struct capi_ctr *ctr,
			      u16 appl, u16 msgid, int channel,
			      u16 handle, u16 info)
{
	struct sk_buff *cskb;
	u8 *msg;

	cskb = alloc_skb(CAPI_DATA_B3_CONF_LEN, GFP_ATOMIC);
	if (!cskb) {
		dev_err(cs->dev, "%s: out of memory\n", __func__);
		return;
	}
	
	msg = __skb_put(cskb, CAPI_DATA_B3_CONF_LEN);
	CAPIMSG_SETLEN(msg, CAPI_DATA_B3_CONF_LEN);
	CAPIMSG_SETAPPID(msg, appl);
	CAPIMSG_SETCOMMAND(msg, CAPI_DATA_B3);
	CAPIMSG_SETSUBCOMMAND(msg,  CAPI_CONF);
	CAPIMSG_SETMSGID(msg, msgid);
	CAPIMSG_SETCONTROLLER(msg, ctr->cnr);
	CAPIMSG_SETPLCI_PART(msg, channel);
	CAPIMSG_SETNCCI_PART(msg, 1);
	CAPIMSG_SETHANDLE_CONF(msg, handle);
	CAPIMSG_SETINFO_CONF(msg, info);

	
	dump_rawmsg(DEBUG_MCMD, __func__, msg);
	capi_ctr_handle_message(ctr, appl, cskb);
}



void gigaset_skb_sent(struct bc_state *bcs, struct sk_buff *dskb)
{
	struct cardstate *cs = bcs->cs;
	struct gigaset_capi_ctr *iif = cs->iif;
	struct gigaset_capi_appl *ap = bcs->ap;
	unsigned char *req = skb_mac_header(dskb);
	u16 flags;

	
	++bcs->trans_up;

	if (!ap) {
		gig_dbg(DEBUG_MCMD, "%s: application gone", __func__);
		return;
	}

	
	if (bcs->apconnstate < APCONN_ACTIVE) {
		gig_dbg(DEBUG_MCMD, "%s: disconnected", __func__);
		return;
	}

	flags = CAPIMSG_FLAGS(req);
	if (flags & CAPI_FLAGS_DELIVERY_CONFIRMATION)
		send_data_b3_conf(cs, &iif->ctr, ap->id, CAPIMSG_MSGID(req),
				  bcs->channel + 1, CAPIMSG_HANDLE_REQ(req),
				  (flags & ~CAPI_FLAGS_DELIVERY_CONFIRMATION) ?
				  CapiFlagsNotSupportedByProtocol :
				  CAPI_NOERROR);
}
EXPORT_SYMBOL_GPL(gigaset_skb_sent);

void gigaset_skb_rcvd(struct bc_state *bcs, struct sk_buff *skb)
{
	struct cardstate *cs = bcs->cs;
	struct gigaset_capi_ctr *iif = cs->iif;
	struct gigaset_capi_appl *ap = bcs->ap;
	int len = skb->len;

	
	bcs->trans_down++;

	if (!ap) {
		gig_dbg(DEBUG_MCMD, "%s: application gone", __func__);
		dev_kfree_skb_any(skb);
		return;
	}

	
	if (bcs->apconnstate < APCONN_ACTIVE) {
		gig_dbg(DEBUG_MCMD, "%s: disconnected", __func__);
		dev_kfree_skb_any(skb);
		return;
	}

	skb_push(skb, CAPI_DATA_B3_REQ_LEN);
	CAPIMSG_SETLEN(skb->data, CAPI_DATA_B3_REQ_LEN);
	CAPIMSG_SETAPPID(skb->data, ap->id);
	CAPIMSG_SETCOMMAND(skb->data, CAPI_DATA_B3);
	CAPIMSG_SETSUBCOMMAND(skb->data,  CAPI_IND);
	CAPIMSG_SETMSGID(skb->data, ap->nextMessageNumber++);
	CAPIMSG_SETCONTROLLER(skb->data, iif->ctr.cnr);
	CAPIMSG_SETPLCI_PART(skb->data, bcs->channel + 1);
	CAPIMSG_SETNCCI_PART(skb->data, 1);
	
	CAPIMSG_SETDATALEN(skb->data, len);
	
	CAPIMSG_SETFLAGS(skb->data, 0);
	

	
	dump_rawmsg(DEBUG_MCMD, __func__, skb->data);
	capi_ctr_handle_message(&iif->ctr, ap->id, skb);
}
EXPORT_SYMBOL_GPL(gigaset_skb_rcvd);

void gigaset_isdn_rcv_err(struct bc_state *bcs)
{
	
	if (bcs->ignore) {
		bcs->ignore--;
		return;
	}

	
	bcs->corrupted++;

	
}
EXPORT_SYMBOL_GPL(gigaset_isdn_rcv_err);

int gigaset_isdn_icall(struct at_state_t *at_state)
{
	struct cardstate *cs = at_state->cs;
	struct bc_state *bcs = at_state->bcs;
	struct gigaset_capi_ctr *iif = cs->iif;
	struct gigaset_capi_appl *ap;
	u32 actCIPmask;
	struct sk_buff *skb;
	unsigned int msgsize;
	unsigned long flags;
	int i;

	if (!bcs)
		return ICALL_IGNORE;

	
	capi_cmsg_header(&iif->hcmsg, 0, CAPI_CONNECT, CAPI_IND, 0,
			 iif->ctr.cnr | ((bcs->channel + 1) << 8));

	
	msgsize = CAPI_CONNECT_IND_BASELEN;

	
	if (at_state->str_var[STR_ZBC]) {
		
		if (encode_ie(at_state->str_var[STR_ZBC], iif->bc_buf,
			      MAX_BC_OCTETS) < 0) {
			dev_warn(cs->dev, "RING ignored - bad BC %s\n",
				 at_state->str_var[STR_ZBC]);
			return ICALL_IGNORE;
		}

		
		iif->hcmsg.CIPValue = 0;	
		for (i = 0; i < ARRAY_SIZE(cip2bchlc); i++)
			if (cip2bchlc[i].bc != NULL &&
			    cip2bchlc[i].hlc == NULL &&
			    !strcmp(cip2bchlc[i].bc,
				    at_state->str_var[STR_ZBC])) {
				iif->hcmsg.CIPValue = i;
				break;
			}
	} else {
		
		iif->hcmsg.CIPValue = 1;
		encode_ie(cip2bchlc[1].bc, iif->bc_buf, MAX_BC_OCTETS);
	}
	iif->hcmsg.BC = iif->bc_buf;
	msgsize += iif->hcmsg.BC[0];

	
	if (at_state->str_var[STR_ZHLC]) {
		
		if (encode_ie(at_state->str_var[STR_ZHLC], iif->hlc_buf,
			      MAX_HLC_OCTETS) < 0) {
			dev_warn(cs->dev, "RING ignored - bad HLC %s\n",
				 at_state->str_var[STR_ZHLC]);
			return ICALL_IGNORE;
		}
		iif->hcmsg.HLC = iif->hlc_buf;
		msgsize += iif->hcmsg.HLC[0];

		
		
		if (at_state->str_var[STR_ZBC])
			for (i = 0; i < ARRAY_SIZE(cip2bchlc); i++)
				if (cip2bchlc[i].hlc != NULL &&
				    !strcmp(cip2bchlc[i].hlc,
					    at_state->str_var[STR_ZHLC]) &&
				    !strcmp(cip2bchlc[i].bc,
					    at_state->str_var[STR_ZBC])) {
					iif->hcmsg.CIPValue = i;
					break;
				}
	}

	
	if (at_state->str_var[STR_ZCPN]) {
		i = strlen(at_state->str_var[STR_ZCPN]);
		if (i > MAX_NUMBER_DIGITS) {
			dev_warn(cs->dev, "RING ignored - bad number %s\n",
				 at_state->str_var[STR_ZBC]);
			return ICALL_IGNORE;
		}
		iif->cdpty_buf[0] = i + 1;
		iif->cdpty_buf[1] = 0x80; 
		memcpy(iif->cdpty_buf + 2, at_state->str_var[STR_ZCPN], i);
		iif->hcmsg.CalledPartyNumber = iif->cdpty_buf;
		msgsize += iif->hcmsg.CalledPartyNumber[0];
	}

	
	if (at_state->str_var[STR_NMBR]) {
		i = strlen(at_state->str_var[STR_NMBR]);
		if (i > MAX_NUMBER_DIGITS) {
			dev_warn(cs->dev, "RING ignored - bad number %s\n",
				 at_state->str_var[STR_ZBC]);
			return ICALL_IGNORE;
		}
		iif->cgpty_buf[0] = i + 2;
		iif->cgpty_buf[1] = 0x00; 
		iif->cgpty_buf[2] = 0x80; 
		memcpy(iif->cgpty_buf + 3, at_state->str_var[STR_NMBR], i);
		iif->hcmsg.CallingPartyNumber = iif->cgpty_buf;
		msgsize += iif->hcmsg.CallingPartyNumber[0];
	}


	gig_dbg(DEBUG_CMD, "icall: PLCI %x CIP %d BC %s",
		iif->hcmsg.adr.adrPLCI, iif->hcmsg.CIPValue,
		format_ie(iif->hcmsg.BC));
	gig_dbg(DEBUG_CMD, "icall: HLC %s",
		format_ie(iif->hcmsg.HLC));
	gig_dbg(DEBUG_CMD, "icall: CgPty %s",
		format_ie(iif->hcmsg.CallingPartyNumber));
	gig_dbg(DEBUG_CMD, "icall: CdPty %s",
		format_ie(iif->hcmsg.CalledPartyNumber));

	
	spin_lock_irqsave(&bcs->aplock, flags);
	if (bcs->ap != NULL || bcs->apconnstate != APCONN_NONE) {
		dev_warn(cs->dev, "%s: channel not properly cleared (%p/%d)\n",
			 __func__, bcs->ap, bcs->apconnstate);
		bcs->ap = NULL;
		bcs->apconnstate = APCONN_NONE;
	}
	spin_unlock_irqrestore(&bcs->aplock, flags);
	actCIPmask = 1 | (1 << iif->hcmsg.CIPValue);
	list_for_each_entry(ap, &iif->appls, ctrlist)
		if (actCIPmask & ap->listenCIPmask) {
			
			iif->hcmsg.ApplId = ap->id;
			iif->hcmsg.Messagenumber = ap->nextMessageNumber++;

			skb = alloc_skb(msgsize, GFP_ATOMIC);
			if (!skb) {
				dev_err(cs->dev, "%s: out of memory\n",
					__func__);
				break;
			}
			capi_cmsg2message(&iif->hcmsg, __skb_put(skb, msgsize));
			dump_cmsg(DEBUG_CMD, __func__, &iif->hcmsg);

			
			spin_lock_irqsave(&bcs->aplock, flags);
			ap->bcnext = bcs->ap;
			bcs->ap = ap;
			bcs->chstate |= CHS_NOTIFY_LL;
			bcs->apconnstate = APCONN_SETUP;
			spin_unlock_irqrestore(&bcs->aplock, flags);

			
			capi_ctr_handle_message(&iif->ctr, ap->id, skb);
		}

	return bcs->ap ? ICALL_ACCEPT : ICALL_IGNORE;
}

static void send_disconnect_ind(struct bc_state *bcs,
				struct gigaset_capi_appl *ap, u16 reason)
{
	struct cardstate *cs = bcs->cs;
	struct gigaset_capi_ctr *iif = cs->iif;
	struct sk_buff *skb;

	if (bcs->apconnstate == APCONN_NONE)
		return;

	capi_cmsg_header(&iif->hcmsg, ap->id, CAPI_DISCONNECT, CAPI_IND,
			 ap->nextMessageNumber++,
			 iif->ctr.cnr | ((bcs->channel + 1) << 8));
	iif->hcmsg.Reason = reason;
	skb = alloc_skb(CAPI_DISCONNECT_IND_LEN, GFP_ATOMIC);
	if (!skb) {
		dev_err(cs->dev, "%s: out of memory\n", __func__);
		return;
	}
	capi_cmsg2message(&iif->hcmsg, __skb_put(skb, CAPI_DISCONNECT_IND_LEN));
	dump_cmsg(DEBUG_CMD, __func__, &iif->hcmsg);
	capi_ctr_handle_message(&iif->ctr, ap->id, skb);
}

static void send_disconnect_b3_ind(struct bc_state *bcs,
				   struct gigaset_capi_appl *ap)
{
	struct cardstate *cs = bcs->cs;
	struct gigaset_capi_ctr *iif = cs->iif;
	struct sk_buff *skb;

	
	if (bcs->apconnstate < APCONN_ACTIVE)
		return;
	bcs->apconnstate = APCONN_SETUP;

	capi_cmsg_header(&iif->hcmsg, ap->id, CAPI_DISCONNECT_B3, CAPI_IND,
			 ap->nextMessageNumber++,
			 iif->ctr.cnr | ((bcs->channel + 1) << 8) | (1 << 16));
	skb = alloc_skb(CAPI_DISCONNECT_B3_IND_BASELEN, GFP_ATOMIC);
	if (!skb) {
		dev_err(cs->dev, "%s: out of memory\n", __func__);
		return;
	}
	capi_cmsg2message(&iif->hcmsg,
			  __skb_put(skb, CAPI_DISCONNECT_B3_IND_BASELEN));
	dump_cmsg(DEBUG_CMD, __func__, &iif->hcmsg);
	capi_ctr_handle_message(&iif->ctr, ap->id, skb);
}

void gigaset_isdn_connD(struct bc_state *bcs)
{
	struct cardstate *cs = bcs->cs;
	struct gigaset_capi_ctr *iif = cs->iif;
	struct gigaset_capi_appl *ap;
	struct sk_buff *skb;
	unsigned int msgsize;
	unsigned long flags;

	spin_lock_irqsave(&bcs->aplock, flags);
	ap = bcs->ap;
	if (!ap) {
		spin_unlock_irqrestore(&bcs->aplock, flags);
		gig_dbg(DEBUG_CMD, "%s: application gone", __func__);
		return;
	}
	if (bcs->apconnstate == APCONN_NONE) {
		spin_unlock_irqrestore(&bcs->aplock, flags);
		dev_warn(cs->dev, "%s: application %u not connected\n",
			 __func__, ap->id);
		return;
	}
	spin_unlock_irqrestore(&bcs->aplock, flags);
	while (ap->bcnext) {
		
		dev_warn(cs->dev, "%s: dropping extra application %u\n",
			 __func__, ap->bcnext->id);
		send_disconnect_ind(bcs, ap->bcnext,
				    CapiCallGivenToOtherApplication);
		ap->bcnext = ap->bcnext->bcnext;
	}

	capi_cmsg_header(&iif->hcmsg, ap->id, CAPI_CONNECT_ACTIVE, CAPI_IND,
			 ap->nextMessageNumber++,
			 iif->ctr.cnr | ((bcs->channel + 1) << 8));

	
	msgsize = CAPI_CONNECT_ACTIVE_IND_BASELEN;


	
	skb = alloc_skb(msgsize, GFP_ATOMIC);
	if (!skb) {
		dev_err(cs->dev, "%s: out of memory\n", __func__);
		return;
	}
	capi_cmsg2message(&iif->hcmsg, __skb_put(skb, msgsize));
	dump_cmsg(DEBUG_CMD, __func__, &iif->hcmsg);
	capi_ctr_handle_message(&iif->ctr, ap->id, skb);
}

void gigaset_isdn_hupD(struct bc_state *bcs)
{
	struct gigaset_capi_appl *ap;
	unsigned long flags;

	spin_lock_irqsave(&bcs->aplock, flags);
	while (bcs->ap != NULL) {
		ap = bcs->ap;
		bcs->ap = ap->bcnext;
		spin_unlock_irqrestore(&bcs->aplock, flags);
		send_disconnect_b3_ind(bcs, ap);
		send_disconnect_ind(bcs, ap, 0);
		spin_lock_irqsave(&bcs->aplock, flags);
	}
	bcs->apconnstate = APCONN_NONE;
	spin_unlock_irqrestore(&bcs->aplock, flags);
}

void gigaset_isdn_connB(struct bc_state *bcs)
{
	struct cardstate *cs = bcs->cs;
	struct gigaset_capi_ctr *iif = cs->iif;
	struct gigaset_capi_appl *ap;
	struct sk_buff *skb;
	unsigned long flags;
	unsigned int msgsize;
	u8 command;

	spin_lock_irqsave(&bcs->aplock, flags);
	ap = bcs->ap;
	if (!ap) {
		spin_unlock_irqrestore(&bcs->aplock, flags);
		gig_dbg(DEBUG_CMD, "%s: application gone", __func__);
		return;
	}
	if (!bcs->apconnstate) {
		spin_unlock_irqrestore(&bcs->aplock, flags);
		dev_warn(cs->dev, "%s: application %u not connected\n",
			 __func__, ap->id);
		return;
	}

	if (bcs->apconnstate >= APCONN_ACTIVE) {
		command = CAPI_CONNECT_B3_ACTIVE;
		msgsize = CAPI_CONNECT_B3_ACTIVE_IND_BASELEN;
	} else {
		command = CAPI_CONNECT_B3;
		msgsize = CAPI_CONNECT_B3_IND_BASELEN;
	}
	bcs->apconnstate = APCONN_ACTIVE;

	spin_unlock_irqrestore(&bcs->aplock, flags);

	while (ap->bcnext) {
		
		dev_warn(cs->dev, "%s: dropping extra application %u\n",
			 __func__, ap->bcnext->id);
		send_disconnect_ind(bcs, ap->bcnext,
				    CapiCallGivenToOtherApplication);
		ap->bcnext = ap->bcnext->bcnext;
	}

	capi_cmsg_header(&iif->hcmsg, ap->id, command, CAPI_IND,
			 ap->nextMessageNumber++,
			 iif->ctr.cnr | ((bcs->channel + 1) << 8) | (1 << 16));
	skb = alloc_skb(msgsize, GFP_ATOMIC);
	if (!skb) {
		dev_err(cs->dev, "%s: out of memory\n", __func__);
		return;
	}
	capi_cmsg2message(&iif->hcmsg, __skb_put(skb, msgsize));
	dump_cmsg(DEBUG_CMD, __func__, &iif->hcmsg);
	capi_ctr_handle_message(&iif->ctr, ap->id, skb);
}

void gigaset_isdn_hupB(struct bc_state *bcs)
{
	struct gigaset_capi_appl *ap = bcs->ap;

	

	if (!ap) {
		gig_dbg(DEBUG_CMD, "%s: application gone", __func__);
		return;
	}

	send_disconnect_b3_ind(bcs, ap);
}

void gigaset_isdn_start(struct cardstate *cs)
{
	struct gigaset_capi_ctr *iif = cs->iif;

	
	strcpy(iif->ctr.manu, "Siemens");
	
	iif->ctr.version.majorversion = 2;		
	iif->ctr.version.minorversion = 0;
	
	iif->ctr.version.majormanuversion = cs->fwver[0];
	iif->ctr.version.minormanuversion = cs->fwver[1];
	
	iif->ctr.profile.nbchannel = cs->channels;
	
	iif->ctr.profile.goptions = 0x11;
	
	iif->ctr.profile.support1 =  0x03;
	
	
	iif->ctr.profile.support2 =  0x02;
	
	iif->ctr.profile.support3 =  0x01;
	
	strcpy(iif->ctr.serial, "0");
	capi_ctr_ready(&iif->ctr);
}

void gigaset_isdn_stop(struct cardstate *cs)
{
	struct gigaset_capi_ctr *iif = cs->iif;
	capi_ctr_down(&iif->ctr);
}


static void gigaset_register_appl(struct capi_ctr *ctr, u16 appl,
				  capi_register_params *rp)
{
	struct gigaset_capi_ctr *iif
		= container_of(ctr, struct gigaset_capi_ctr, ctr);
	struct cardstate *cs = ctr->driverdata;
	struct gigaset_capi_appl *ap;

	gig_dbg(DEBUG_CMD, "%s [%u] l3cnt=%u blkcnt=%u blklen=%u",
		__func__, appl, rp->level3cnt, rp->datablkcnt, rp->datablklen);

	list_for_each_entry(ap, &iif->appls, ctrlist)
		if (ap->id == appl) {
			dev_notice(cs->dev,
				   "application %u already registered\n", appl);
			return;
		}

	ap = kzalloc(sizeof(*ap), GFP_KERNEL);
	if (!ap) {
		dev_err(cs->dev, "%s: out of memory\n", __func__);
		return;
	}
	ap->id = appl;
	ap->rp = *rp;

	list_add(&ap->ctrlist, &iif->appls);
	dev_info(cs->dev, "application %u registered\n", ap->id);
}


static inline void remove_appl_from_channel(struct bc_state *bcs,
					    struct gigaset_capi_appl *ap)
{
	struct cardstate *cs = bcs->cs;
	struct gigaset_capi_appl *bcap;
	unsigned long flags;
	int prevconnstate;

	spin_lock_irqsave(&bcs->aplock, flags);
	bcap = bcs->ap;
	if (bcap == NULL) {
		spin_unlock_irqrestore(&bcs->aplock, flags);
		return;
	}

	
	if (bcap == ap) {
		bcs->ap = ap->bcnext;
		if (bcs->ap != NULL) {
			spin_unlock_irqrestore(&bcs->aplock, flags);
			return;
		}

		
		prevconnstate = bcs->apconnstate;
		bcs->apconnstate = APCONN_NONE;
		spin_unlock_irqrestore(&bcs->aplock, flags);

		if (prevconnstate == APCONN_ACTIVE) {
			dev_notice(cs->dev, "%s: hanging up channel %u\n",
				   __func__, bcs->channel);
			gigaset_add_event(cs, &bcs->at_state,
					  EV_HUP, NULL, 0, NULL);
			gigaset_schedule_event(cs);
		}
		return;
	}

	
	do {
		if (bcap->bcnext == ap) {
			bcap->bcnext = bcap->bcnext->bcnext;
			spin_unlock_irqrestore(&bcs->aplock, flags);
			return;
		}
		bcap = bcap->bcnext;
	} while (bcap != NULL);
	spin_unlock_irqrestore(&bcs->aplock, flags);
}

static void gigaset_release_appl(struct capi_ctr *ctr, u16 appl)
{
	struct gigaset_capi_ctr *iif
		= container_of(ctr, struct gigaset_capi_ctr, ctr);
	struct cardstate *cs = iif->ctr.driverdata;
	struct gigaset_capi_appl *ap, *tmp;
	unsigned ch;

	gig_dbg(DEBUG_CMD, "%s [%u]", __func__, appl);

	list_for_each_entry_safe(ap, tmp, &iif->appls, ctrlist)
		if (ap->id == appl) {
			
			for (ch = 0; ch < cs->channels; ch++)
				remove_appl_from_channel(&cs->bcs[ch], ap);

			
			list_del(&ap->ctrlist);
			kfree(ap);
			dev_info(cs->dev, "application %u released\n", appl);
		}
}


static void send_conf(struct gigaset_capi_ctr *iif,
		      struct gigaset_capi_appl *ap,
		      struct sk_buff *skb,
		      u16 info)
{
	capi_cmsg_answer(&iif->acmsg);
	iif->acmsg.Info = info;
	capi_cmsg2message(&iif->acmsg, skb->data);
	__skb_trim(skb, CAPI_STDCONF_LEN);
	dump_cmsg(DEBUG_CMD, __func__, &iif->acmsg);
	capi_ctr_handle_message(&iif->ctr, ap->id, skb);
}

static void do_facility_req(struct gigaset_capi_ctr *iif,
			    struct gigaset_capi_appl *ap,
			    struct sk_buff *skb)
{
	struct cardstate *cs = iif->ctr.driverdata;
	_cmsg *cmsg = &iif->acmsg;
	struct sk_buff *cskb;
	u8 *pparam;
	unsigned int msgsize = CAPI_FACILITY_CONF_BASELEN;
	u16 function, info;
	static u8 confparam[10];	

	
	capi_message2cmsg(cmsg, skb->data);
	dump_cmsg(DEBUG_CMD, __func__, cmsg);

	switch (cmsg->FacilitySelector) {
	case CAPI_FACILITY_DTMF:	
		info = CapiFacilityNotSupported;
		confparam[0] = 2;	
		
		capimsg_setu16(confparam, 1, 2);
		break;

	case CAPI_FACILITY_V42BIS:	
		info = CapiFacilityNotSupported;
		confparam[0] = 2;	
		
		capimsg_setu16(confparam, 1, 1);
		break;

	case CAPI_FACILITY_SUPPSVC:
		
		pparam = cmsg->FacilityRequestParameter;
		if (pparam == NULL || pparam[0] < 2) {
			dev_notice(cs->dev, "%s: %s missing\n", "FACILITY_REQ",
				   "Facility Request Parameter");
			send_conf(iif, ap, skb, CapiIllMessageParmCoding);
			return;
		}
		function = CAPIMSG_U16(pparam, 1);
		switch (function) {
		case CAPI_SUPPSVC_GETSUPPORTED:
			info = CapiSuccess;
			
			confparam[3] = 6;	
			
			capimsg_setu16(confparam, 4, CapiSuccess);
			
			capimsg_setu32(confparam, 6, 0);
			break;
		case CAPI_SUPPSVC_LISTEN:
			if (pparam[0] < 7 || pparam[3] < 4) {
				dev_notice(cs->dev, "%s: %s missing\n",
					   "FACILITY_REQ", "Notification Mask");
				send_conf(iif, ap, skb,
					  CapiIllMessageParmCoding);
				return;
			}
			if (CAPIMSG_U32(pparam, 4) != 0) {
				dev_notice(cs->dev,
					   "%s: unsupported supplementary service notification mask 0x%x\n",
					   "FACILITY_REQ", CAPIMSG_U32(pparam, 4));
				info = CapiFacilitySpecificFunctionNotSupported;
				confparam[3] = 2;	
				capimsg_setu16(confparam, 4,
					       CapiSupplementaryServiceNotSupported);
			}
			info = CapiSuccess;
			confparam[3] = 2;	
			capimsg_setu16(confparam, 4, CapiSuccess);
			break;
			
		default:
			dev_notice(cs->dev,
				   "%s: unsupported supplementary service function 0x%04x\n",
				   "FACILITY_REQ", function);
			info = CapiFacilitySpecificFunctionNotSupported;
			
			confparam[3] = 2;	
			
			capimsg_setu16(confparam, 4,
				       CapiSupplementaryServiceNotSupported);
		}

		
		confparam[0] = confparam[3] + 3;	
		
		capimsg_setu16(confparam, 1, function);
		
		break;

	case CAPI_FACILITY_WAKEUP:	
		info = CapiFacilityNotSupported;
		confparam[0] = 2;	
		
		capimsg_setu16(confparam, 1, 0);
		break;

	default:
		info = CapiFacilityNotSupported;
		confparam[0] = 0;	
	}

	
	capi_cmsg_answer(cmsg);
	cmsg->Info = info;
	cmsg->FacilityConfirmationParameter = confparam;
	msgsize += confparam[0];	
	cskb = alloc_skb(msgsize, GFP_ATOMIC);
	if (!cskb) {
		dev_err(cs->dev, "%s: out of memory\n", __func__);
		return;
	}
	capi_cmsg2message(cmsg, __skb_put(cskb, msgsize));
	dump_cmsg(DEBUG_CMD, __func__, cmsg);
	capi_ctr_handle_message(&iif->ctr, ap->id, cskb);
}


static void do_listen_req(struct gigaset_capi_ctr *iif,
			  struct gigaset_capi_appl *ap,
			  struct sk_buff *skb)
{
	
	capi_message2cmsg(&iif->acmsg, skb->data);
	dump_cmsg(DEBUG_CMD, __func__, &iif->acmsg);

	
	ap->listenInfoMask = iif->acmsg.InfoMask;
	ap->listenCIPmask = iif->acmsg.CIPmask;
	send_conf(iif, ap, skb, CapiSuccess);
}

static void do_alert_req(struct gigaset_capi_ctr *iif,
			 struct gigaset_capi_appl *ap,
			 struct sk_buff *skb)
{
	
	capi_message2cmsg(&iif->acmsg, skb->data);
	dump_cmsg(DEBUG_CMD, __func__, &iif->acmsg);
	send_conf(iif, ap, skb, CapiAlertAlreadySent);
}

static void do_connect_req(struct gigaset_capi_ctr *iif,
			   struct gigaset_capi_appl *ap,
			   struct sk_buff *skb)
{
	struct cardstate *cs = iif->ctr.driverdata;
	_cmsg *cmsg = &iif->acmsg;
	struct bc_state *bcs;
	char **commands;
	char *s;
	u8 *pp;
	unsigned long flags;
	int i, l, lbc, lhlc;
	u16 info;

	
	capi_message2cmsg(cmsg, skb->data);
	dump_cmsg(DEBUG_CMD, __func__, cmsg);

	
	bcs = gigaset_get_free_channel(cs);
	if (!bcs) {
		dev_notice(cs->dev, "%s: no B channel available\n",
			   "CONNECT_REQ");
		send_conf(iif, ap, skb, CapiNoPlciAvailable);
		return;
	}
	spin_lock_irqsave(&bcs->aplock, flags);
	if (bcs->ap != NULL || bcs->apconnstate != APCONN_NONE)
		dev_warn(cs->dev, "%s: channel not properly cleared (%p/%d)\n",
			 __func__, bcs->ap, bcs->apconnstate);
	ap->bcnext = NULL;
	bcs->ap = ap;
	bcs->apconnstate = APCONN_SETUP;
	spin_unlock_irqrestore(&bcs->aplock, flags);

	bcs->rx_bufsize = ap->rp.datablklen;
	dev_kfree_skb(bcs->rx_skb);
	gigaset_new_rx_skb(bcs);
	cmsg->adr.adrPLCI |= (bcs->channel + 1) << 8;

	
	commands = kzalloc(AT_NUM * (sizeof *commands), GFP_KERNEL);
	if (!commands)
		goto oom;

	
	pp = cmsg->CalledPartyNumber;
	if (pp == NULL || *pp == 0) {
		dev_notice(cs->dev, "%s: %s missing\n",
			   "CONNECT_REQ", "Called party number");
		info = CapiIllMessageParmCoding;
		goto error;
	}
	l = *pp++;
	
	switch (*pp) {
	case 0x80:	
	case 0x81:	
		break;
	default:	
		dev_notice(cs->dev, "%s: %s type/plan 0x%02x unsupported\n",
			   "CONNECT_REQ", "Called party number", *pp);
	}
	pp++;
	l--;
	
	if (l >= 2 && pp[0] == '*' && pp[1] == '*') {
		s = "^SCTP=0\r";
		pp += 2;
		l -= 2;
	} else {
		s = "^SCTP=1\r";
	}
	commands[AT_TYPE] = kstrdup(s, GFP_KERNEL);
	if (!commands[AT_TYPE])
		goto oom;
	commands[AT_DIAL] = kmalloc(l + 3, GFP_KERNEL);
	if (!commands[AT_DIAL])
		goto oom;
	snprintf(commands[AT_DIAL], l + 3, "D%.*s\r", l, pp);

	
	pp = cmsg->CallingPartyNumber;
	if (pp != NULL && *pp > 0) {
		l = *pp++;

		
		
		switch (*pp) {
		case 0x00:	
		case 0x01:	
			break;
		default:
			dev_notice(cs->dev,
				   "%s: %s type/plan 0x%02x unsupported\n",
				   "CONNECT_REQ", "Calling party number", *pp);
		}
		pp++;
		l--;

		
		if (!l) {
			dev_notice(cs->dev, "%s: %s IE truncated\n",
				   "CONNECT_REQ", "Calling party number");
			info = CapiIllMessageParmCoding;
			goto error;
		}
		switch (*pp & 0xfc) { 
		case 0x80:	
			s = "^SCLIP=1\r";
			break;
		case 0xa0:	
			s = "^SCLIP=0\r";
			break;
		default:
			dev_notice(cs->dev, "%s: invalid %s 0x%02x\n",
				   "CONNECT_REQ",
				   "Presentation/Screening indicator",
				   *pp);
			s = "^SCLIP=1\r";
		}
		commands[AT_CLIP] = kstrdup(s, GFP_KERNEL);
		if (!commands[AT_CLIP])
			goto oom;
		pp++;
		l--;

		if (l) {
			
			commands[AT_MSN] = kmalloc(l + 8, GFP_KERNEL);
			if (!commands[AT_MSN])
				goto oom;
			snprintf(commands[AT_MSN], l + 8, "^SMSN=%*s\r", l, pp);
		}
	}

	
	if (cmsg->CIPValue >= ARRAY_SIZE(cip2bchlc) ||
	    (cmsg->CIPValue > 0 && cip2bchlc[cmsg->CIPValue].bc == NULL)) {
		dev_notice(cs->dev, "%s: unknown CIP value %d\n",
			   "CONNECT_REQ", cmsg->CIPValue);
		info = CapiCipValueUnknown;
		goto error;
	}


	
	if (cmsg->BC && cmsg->BC[0])		
		lbc = 2 * cmsg->BC[0];
	else if (cip2bchlc[cmsg->CIPValue].bc)	
		lbc = strlen(cip2bchlc[cmsg->CIPValue].bc);
	else					
		lbc = 0;
	if (cmsg->HLC && cmsg->HLC[0])		
		lhlc = 2 * cmsg->HLC[0];
	else if (cip2bchlc[cmsg->CIPValue].hlc)	
		lhlc = strlen(cip2bchlc[cmsg->CIPValue].hlc);
	else					
		lhlc = 0;

	if (lbc) {
		
		l = lbc + 7;		
		if (lhlc)
			l += lhlc + 7;	
		commands[AT_BC] = kmalloc(l, GFP_KERNEL);
		if (!commands[AT_BC])
			goto oom;
		strcpy(commands[AT_BC], "^SBC=");
		if (cmsg->BC && cmsg->BC[0])	
			decode_ie(cmsg->BC, commands[AT_BC] + 5);
		else				
			strcpy(commands[AT_BC] + 5,
			       cip2bchlc[cmsg->CIPValue].bc);
		if (lhlc) {
			strcpy(commands[AT_BC] + lbc + 5, ";^SHLC=");
			if (cmsg->HLC && cmsg->HLC[0])
				
				decode_ie(cmsg->HLC,
					  commands[AT_BC] + lbc + 12);
			else	
				strcpy(commands[AT_BC] + lbc + 12,
				       cip2bchlc[cmsg->CIPValue].hlc);
		}
		strcpy(commands[AT_BC] + l - 2, "\r");
	} else {
		
		if (lhlc) {
			dev_notice(cs->dev, "%s: cannot set HLC without BC\n",
				   "CONNECT_REQ");
			info = CapiIllMessageParmCoding; 
			goto error;
		}
	}

	
	if (cmsg->BProtocol == CAPI_DEFAULT) {
		bcs->proto2 = L2_HDLC;
		dev_warn(cs->dev,
			 "B2 Protocol X.75 SLP unsupported, using Transparent\n");
	} else {
		switch (cmsg->B1protocol) {
		case 0:
			bcs->proto2 = L2_HDLC;
			break;
		case 1:
			bcs->proto2 = L2_VOICE;
			break;
		default:
			dev_warn(cs->dev,
				 "B1 Protocol %u unsupported, using Transparent\n",
				 cmsg->B1protocol);
			bcs->proto2 = L2_VOICE;
		}
		if (cmsg->B2protocol != 1)
			dev_warn(cs->dev,
				 "B2 Protocol %u unsupported, using Transparent\n",
				 cmsg->B2protocol);
		if (cmsg->B3protocol != 0)
			dev_warn(cs->dev,
				 "B3 Protocol %u unsupported, using Transparent\n",
				 cmsg->B3protocol);
		ignore_cstruct_param(cs, cmsg->B1configuration,
				     "CONNECT_REQ", "B1 Configuration");
		ignore_cstruct_param(cs, cmsg->B2configuration,
				     "CONNECT_REQ", "B2 Configuration");
		ignore_cstruct_param(cs, cmsg->B3configuration,
				     "CONNECT_REQ", "B3 Configuration");
	}
	commands[AT_PROTO] = kmalloc(9, GFP_KERNEL);
	if (!commands[AT_PROTO])
		goto oom;
	snprintf(commands[AT_PROTO], 9, "^SBPR=%u\r", bcs->proto2);

	
	ignore_cstruct_param(cs, cmsg->CalledPartySubaddress,
			     "CONNECT_REQ", "Called pty subaddr");
	ignore_cstruct_param(cs, cmsg->CallingPartySubaddress,
			     "CONNECT_REQ", "Calling pty subaddr");
	ignore_cstruct_param(cs, cmsg->LLC,
			     "CONNECT_REQ", "LLC");
	if (cmsg->AdditionalInfo != CAPI_DEFAULT) {
		ignore_cstruct_param(cs, cmsg->BChannelinformation,
				     "CONNECT_REQ", "B Channel Information");
		ignore_cstruct_param(cs, cmsg->Keypadfacility,
				     "CONNECT_REQ", "Keypad Facility");
		ignore_cstruct_param(cs, cmsg->Useruserdata,
				     "CONNECT_REQ", "User-User Data");
		ignore_cstruct_param(cs, cmsg->Facilitydataarray,
				     "CONNECT_REQ", "Facility Data Array");
	}

	
	commands[AT_ISO] = kmalloc(9, GFP_KERNEL);
	if (!commands[AT_ISO])
		goto oom;
	snprintf(commands[AT_ISO], 9, "^SISO=%u\r",
		 (unsigned) bcs->channel + 1);

	
	if (!gigaset_add_event(cs, &bcs->at_state, EV_DIAL, commands,
			       bcs->at_state.seq_index, NULL)) {
		info = CAPI_MSGOSRESOURCEERR;
		goto error;
	}
	gigaset_schedule_event(cs);
	send_conf(iif, ap, skb, CapiSuccess);
	return;

oom:
	dev_err(cs->dev, "%s: out of memory\n", __func__);
	info = CAPI_MSGOSRESOURCEERR;
error:
	if (commands)
		for (i = 0; i < AT_NUM; i++)
			kfree(commands[i]);
	kfree(commands);
	gigaset_free_channel(bcs);
	send_conf(iif, ap, skb, info);
}

static void do_connect_resp(struct gigaset_capi_ctr *iif,
			    struct gigaset_capi_appl *ap,
			    struct sk_buff *skb)
{
	struct cardstate *cs = iif->ctr.driverdata;
	_cmsg *cmsg = &iif->acmsg;
	struct bc_state *bcs;
	struct gigaset_capi_appl *oap;
	unsigned long flags;
	int channel;

	
	capi_message2cmsg(cmsg, skb->data);
	dump_cmsg(DEBUG_CMD, __func__, cmsg);
	dev_kfree_skb_any(skb);

	
	channel = (cmsg->adr.adrPLCI >> 8) & 0xff;
	if (!channel || channel > cs->channels) {
		dev_notice(cs->dev, "%s: invalid %s 0x%02x\n",
			   "CONNECT_RESP", "PLCI", cmsg->adr.adrPLCI);
		return;
	}
	bcs = cs->bcs + channel - 1;

	switch (cmsg->Reject) {
	case 0:		
		
		spin_lock_irqsave(&bcs->aplock, flags);
		while (bcs->ap != NULL) {
			oap = bcs->ap;
			bcs->ap = oap->bcnext;
			if (oap != ap) {
				spin_unlock_irqrestore(&bcs->aplock, flags);
				send_disconnect_ind(bcs, oap,
						    CapiCallGivenToOtherApplication);
				spin_lock_irqsave(&bcs->aplock, flags);
			}
		}
		ap->bcnext = NULL;
		bcs->ap = ap;
		spin_unlock_irqrestore(&bcs->aplock, flags);

		bcs->rx_bufsize = ap->rp.datablklen;
		dev_kfree_skb(bcs->rx_skb);
		gigaset_new_rx_skb(bcs);
		bcs->chstate |= CHS_NOTIFY_LL;

		
		if (cmsg->BProtocol == CAPI_DEFAULT) {
			bcs->proto2 = L2_HDLC;
			dev_warn(cs->dev,
				 "B2 Protocol X.75 SLP unsupported, using Transparent\n");
		} else {
			switch (cmsg->B1protocol) {
			case 0:
				bcs->proto2 = L2_HDLC;
				break;
			case 1:
				bcs->proto2 = L2_VOICE;
				break;
			default:
				dev_warn(cs->dev,
					 "B1 Protocol %u unsupported, using Transparent\n",
					 cmsg->B1protocol);
				bcs->proto2 = L2_VOICE;
			}
			if (cmsg->B2protocol != 1)
				dev_warn(cs->dev,
					 "B2 Protocol %u unsupported, using Transparent\n",
					 cmsg->B2protocol);
			if (cmsg->B3protocol != 0)
				dev_warn(cs->dev,
					 "B3 Protocol %u unsupported, using Transparent\n",
					 cmsg->B3protocol);
			ignore_cstruct_param(cs, cmsg->B1configuration,
					     "CONNECT_RESP", "B1 Configuration");
			ignore_cstruct_param(cs, cmsg->B2configuration,
					     "CONNECT_RESP", "B2 Configuration");
			ignore_cstruct_param(cs, cmsg->B3configuration,
					     "CONNECT_RESP", "B3 Configuration");
		}

		
		ignore_cstruct_param(cs, cmsg->ConnectedNumber,
				     "CONNECT_RESP", "Connected Number");
		ignore_cstruct_param(cs, cmsg->ConnectedSubaddress,
				     "CONNECT_RESP", "Connected Subaddress");
		ignore_cstruct_param(cs, cmsg->LLC,
				     "CONNECT_RESP", "LLC");
		if (cmsg->AdditionalInfo != CAPI_DEFAULT) {
			ignore_cstruct_param(cs, cmsg->BChannelinformation,
					     "CONNECT_RESP", "BChannel Information");
			ignore_cstruct_param(cs, cmsg->Keypadfacility,
					     "CONNECT_RESP", "Keypad Facility");
			ignore_cstruct_param(cs, cmsg->Useruserdata,
					     "CONNECT_RESP", "User-User Data");
			ignore_cstruct_param(cs, cmsg->Facilitydataarray,
					     "CONNECT_RESP", "Facility Data Array");
		}

		
		if (!gigaset_add_event(cs, &cs->bcs[channel - 1].at_state,
				       EV_ACCEPT, NULL, 0, NULL))
			return;
		gigaset_schedule_event(cs);
		return;

	case 1:			
		
		send_disconnect_ind(bcs, ap, 0);

		
		spin_lock_irqsave(&bcs->aplock, flags);
		if (bcs->ap == ap) {
			bcs->ap = ap->bcnext;
			if (bcs->ap == NULL) {
				
				bcs->apconnstate = APCONN_NONE;
				bcs->chstate &= ~CHS_NOTIFY_LL;
			}
			spin_unlock_irqrestore(&bcs->aplock, flags);
			return;
		}
		for (oap = bcs->ap; oap != NULL; oap = oap->bcnext) {
			if (oap->bcnext == ap) {
				oap->bcnext = oap->bcnext->bcnext;
				spin_unlock_irqrestore(&bcs->aplock, flags);
				return;
			}
		}
		spin_unlock_irqrestore(&bcs->aplock, flags);
		dev_err(cs->dev, "%s: application %u not found\n",
			__func__, ap->id);
		return;

	default:		
		
		spin_lock_irqsave(&bcs->aplock, flags);
		while (bcs->ap != NULL) {
			oap = bcs->ap;
			bcs->ap = oap->bcnext;
			if (oap != ap) {
				spin_unlock_irqrestore(&bcs->aplock, flags);
				send_disconnect_ind(bcs, oap,
						    CapiCallGivenToOtherApplication);
				spin_lock_irqsave(&bcs->aplock, flags);
			}
		}
		ap->bcnext = NULL;
		bcs->ap = ap;
		spin_unlock_irqrestore(&bcs->aplock, flags);

		
		dev_info(cs->dev, "%s: Reject=%x\n",
			 "CONNECT_RESP", cmsg->Reject);
		if (!gigaset_add_event(cs, &cs->bcs[channel - 1].at_state,
				       EV_HUP, NULL, 0, NULL))
			return;
		gigaset_schedule_event(cs);
		return;
	}
}

static void do_connect_b3_req(struct gigaset_capi_ctr *iif,
			      struct gigaset_capi_appl *ap,
			      struct sk_buff *skb)
{
	struct cardstate *cs = iif->ctr.driverdata;
	_cmsg *cmsg = &iif->acmsg;
	struct bc_state *bcs;
	int channel;

	
	capi_message2cmsg(cmsg, skb->data);
	dump_cmsg(DEBUG_CMD, __func__, cmsg);

	
	channel = (cmsg->adr.adrPLCI >> 8) & 0xff;
	if (!channel || channel > cs->channels) {
		dev_notice(cs->dev, "%s: invalid %s 0x%02x\n",
			   "CONNECT_B3_REQ", "PLCI", cmsg->adr.adrPLCI);
		send_conf(iif, ap, skb, CapiIllContrPlciNcci);
		return;
	}
	bcs = &cs->bcs[channel - 1];

	
	bcs->apconnstate = APCONN_ACTIVE;

	
	cmsg->adr.adrNCCI |= 1 << 16;

	
	ignore_cstruct_param(cs, cmsg->NCPI, "CONNECT_B3_REQ", "NCPI");
	send_conf(iif, ap, skb, (cmsg->NCPI && cmsg->NCPI[0]) ?
		  CapiNcpiNotSupportedByProtocol : CapiSuccess);
}

static void do_connect_b3_resp(struct gigaset_capi_ctr *iif,
			       struct gigaset_capi_appl *ap,
			       struct sk_buff *skb)
{
	struct cardstate *cs = iif->ctr.driverdata;
	_cmsg *cmsg = &iif->acmsg;
	struct bc_state *bcs;
	int channel;
	unsigned int msgsize;
	u8 command;

	
	capi_message2cmsg(cmsg, skb->data);
	dump_cmsg(DEBUG_CMD, __func__, cmsg);

	
	channel = (cmsg->adr.adrNCCI >> 8) & 0xff;
	if (!channel || channel > cs->channels ||
	    ((cmsg->adr.adrNCCI >> 16) & 0xffff) != 1) {
		dev_notice(cs->dev, "%s: invalid %s 0x%02x\n",
			   "CONNECT_B3_RESP", "NCCI", cmsg->adr.adrNCCI);
		dev_kfree_skb_any(skb);
		return;
	}
	bcs = &cs->bcs[channel - 1];

	if (cmsg->Reject) {
		
		bcs->apconnstate = APCONN_SETUP;

		
		if (!gigaset_add_event(cs, &bcs->at_state,
				       EV_HUP, NULL, 0, NULL)) {
			dev_kfree_skb_any(skb);
			return;
		}
		gigaset_schedule_event(cs);

		
		command = CAPI_DISCONNECT_B3;
		msgsize = CAPI_DISCONNECT_B3_IND_BASELEN;
	} else {
		command = CAPI_CONNECT_B3_ACTIVE;
		msgsize = CAPI_CONNECT_B3_ACTIVE_IND_BASELEN;
	}
	capi_cmsg_header(cmsg, ap->id, command, CAPI_IND,
			 ap->nextMessageNumber++, cmsg->adr.adrNCCI);
	__skb_trim(skb, msgsize);
	capi_cmsg2message(cmsg, skb->data);
	dump_cmsg(DEBUG_CMD, __func__, cmsg);
	capi_ctr_handle_message(&iif->ctr, ap->id, skb);
}

static void do_disconnect_req(struct gigaset_capi_ctr *iif,
			      struct gigaset_capi_appl *ap,
			      struct sk_buff *skb)
{
	struct cardstate *cs = iif->ctr.driverdata;
	_cmsg *cmsg = &iif->acmsg;
	struct bc_state *bcs;
	_cmsg *b3cmsg;
	struct sk_buff *b3skb;
	int channel;

	
	capi_message2cmsg(cmsg, skb->data);
	dump_cmsg(DEBUG_CMD, __func__, cmsg);

	
	channel = (cmsg->adr.adrPLCI >> 8) & 0xff;
	if (!channel || channel > cs->channels) {
		dev_notice(cs->dev, "%s: invalid %s 0x%02x\n",
			   "DISCONNECT_REQ", "PLCI", cmsg->adr.adrPLCI);
		send_conf(iif, ap, skb, CapiIllContrPlciNcci);
		return;
	}
	bcs = cs->bcs + channel - 1;

	
	if (cmsg->AdditionalInfo != CAPI_DEFAULT) {
		ignore_cstruct_param(cs, cmsg->BChannelinformation,
				     "DISCONNECT_REQ", "B Channel Information");
		ignore_cstruct_param(cs, cmsg->Keypadfacility,
				     "DISCONNECT_REQ", "Keypad Facility");
		ignore_cstruct_param(cs, cmsg->Useruserdata,
				     "DISCONNECT_REQ", "User-User Data");
		ignore_cstruct_param(cs, cmsg->Facilitydataarray,
				     "DISCONNECT_REQ", "Facility Data Array");
	}

	
	if (!bcs->apconnstate)
		return;

	
	if (bcs->apconnstate >= APCONN_ACTIVE) {
		b3cmsg = kmalloc(sizeof(*b3cmsg), GFP_KERNEL);
		if (!b3cmsg) {
			dev_err(cs->dev, "%s: out of memory\n", __func__);
			send_conf(iif, ap, skb, CAPI_MSGOSRESOURCEERR);
			return;
		}
		capi_cmsg_header(b3cmsg, ap->id, CAPI_DISCONNECT_B3, CAPI_IND,
				 ap->nextMessageNumber++,
				 cmsg->adr.adrPLCI | (1 << 16));
		b3cmsg->Reason_B3 = CapiProtocolErrorLayer1;
		b3skb = alloc_skb(CAPI_DISCONNECT_B3_IND_BASELEN, GFP_KERNEL);
		if (b3skb == NULL) {
			dev_err(cs->dev, "%s: out of memory\n", __func__);
			send_conf(iif, ap, skb, CAPI_MSGOSRESOURCEERR);
			kfree(b3cmsg);
			return;
		}
		capi_cmsg2message(b3cmsg,
				  __skb_put(b3skb, CAPI_DISCONNECT_B3_IND_BASELEN));
		kfree(b3cmsg);
		capi_ctr_handle_message(&iif->ctr, ap->id, b3skb);
	}

	
	if (!gigaset_add_event(cs, &bcs->at_state, EV_HUP, NULL, 0, NULL)) {
		send_conf(iif, ap, skb, CAPI_MSGOSRESOURCEERR);
		return;
	}
	gigaset_schedule_event(cs);

	
	send_conf(iif, ap, skb, CapiSuccess);
}

static void do_disconnect_b3_req(struct gigaset_capi_ctr *iif,
				 struct gigaset_capi_appl *ap,
				 struct sk_buff *skb)
{
	struct cardstate *cs = iif->ctr.driverdata;
	_cmsg *cmsg = &iif->acmsg;
	struct bc_state *bcs;
	int channel;

	
	capi_message2cmsg(cmsg, skb->data);
	dump_cmsg(DEBUG_CMD, __func__, cmsg);

	
	channel = (cmsg->adr.adrNCCI >> 8) & 0xff;
	if (!channel || channel > cs->channels ||
	    ((cmsg->adr.adrNCCI >> 16) & 0xffff) != 1) {
		dev_notice(cs->dev, "%s: invalid %s 0x%02x\n",
			   "DISCONNECT_B3_REQ", "NCCI", cmsg->adr.adrNCCI);
		send_conf(iif, ap, skb, CapiIllContrPlciNcci);
		return;
	}
	bcs = &cs->bcs[channel - 1];

	
	if (bcs->apconnstate < APCONN_ACTIVE) {
		send_conf(iif, ap, skb,
			  CapiMessageNotSupportedInCurrentState);
		return;
	}

	
	if (!gigaset_add_event(cs, &bcs->at_state, EV_HUP, NULL, 0, NULL)) {
		send_conf(iif, ap, skb, CAPI_MSGOSRESOURCEERR);
		return;
	}
	gigaset_schedule_event(cs);

	
	ignore_cstruct_param(cs, cmsg->NCPI,
			     "DISCONNECT_B3_REQ", "NCPI");
	send_conf(iif, ap, skb, (cmsg->NCPI && cmsg->NCPI[0]) ?
		  CapiNcpiNotSupportedByProtocol : CapiSuccess);
}

static void do_data_b3_req(struct gigaset_capi_ctr *iif,
			   struct gigaset_capi_appl *ap,
			   struct sk_buff *skb)
{
	struct cardstate *cs = iif->ctr.driverdata;
	struct bc_state *bcs;
	int channel = CAPIMSG_PLCI_PART(skb->data);
	u16 ncci = CAPIMSG_NCCI_PART(skb->data);
	u16 msglen = CAPIMSG_LEN(skb->data);
	u16 datalen = CAPIMSG_DATALEN(skb->data);
	u16 flags = CAPIMSG_FLAGS(skb->data);
	u16 msgid = CAPIMSG_MSGID(skb->data);
	u16 handle = CAPIMSG_HANDLE_REQ(skb->data);

	
	dump_rawmsg(DEBUG_MCMD, __func__, skb->data);

	
	if (channel == 0 || channel > cs->channels || ncci != 1) {
		dev_notice(cs->dev, "%s: invalid %s 0x%02x\n",
			   "DATA_B3_REQ", "NCCI", CAPIMSG_NCCI(skb->data));
		send_conf(iif, ap, skb, CapiIllContrPlciNcci);
		return;
	}
	bcs = &cs->bcs[channel - 1];
	if (msglen != CAPI_DATA_B3_REQ_LEN && msglen != CAPI_DATA_B3_REQ_LEN64)
		dev_notice(cs->dev, "%s: unexpected length %d\n",
			   "DATA_B3_REQ", msglen);
	if (msglen + datalen != skb->len)
		dev_notice(cs->dev, "%s: length mismatch (%d+%d!=%d)\n",
			   "DATA_B3_REQ", msglen, datalen, skb->len);
	if (msglen + datalen > skb->len) {
		
		send_conf(iif, ap, skb, CapiIllMessageParmCoding); 
		return;
	}
	if (flags & CAPI_FLAGS_RESERVED) {
		dev_notice(cs->dev, "%s: reserved flags set (%x)\n",
			   "DATA_B3_REQ", flags);
		send_conf(iif, ap, skb, CapiIllMessageParmCoding);
		return;
	}

	
	if (bcs->apconnstate < APCONN_ACTIVE) {
		send_conf(iif, ap, skb, CapiMessageNotSupportedInCurrentState);
		return;
	}

	
	skb_reset_mac_header(skb);
	skb->mac_len = msglen;
	skb_pull(skb, msglen);

	
	if (cs->ops->send_skb(bcs, skb) < 0) {
		send_conf(iif, ap, skb, CAPI_MSGOSRESOURCEERR);
		return;
	}

	if (!(flags & CAPI_FLAGS_DELIVERY_CONFIRMATION))
		send_data_b3_conf(cs, &iif->ctr, ap->id, msgid, channel, handle,
				  flags ? CapiFlagsNotSupportedByProtocol
				  : CAPI_NOERROR);
}

static void do_reset_b3_req(struct gigaset_capi_ctr *iif,
			    struct gigaset_capi_appl *ap,
			    struct sk_buff *skb)
{
	
	capi_message2cmsg(&iif->acmsg, skb->data);
	dump_cmsg(DEBUG_CMD, __func__, &iif->acmsg);
	send_conf(iif, ap, skb,
		  CapiResetProcedureNotSupportedByCurrentProtocol);
}

static unsigned long ignored_msg_dump_time;

static void do_unsupported(struct gigaset_capi_ctr *iif,
			   struct gigaset_capi_appl *ap,
			   struct sk_buff *skb)
{
	
	capi_message2cmsg(&iif->acmsg, skb->data);
	if (printk_timed_ratelimit(&ignored_msg_dump_time, 30 * 1000))
		dump_cmsg(DEBUG_CMD, __func__, &iif->acmsg);
	send_conf(iif, ap, skb, CapiMessageNotSupportedInCurrentState);
}

static void do_nothing(struct gigaset_capi_ctr *iif,
		       struct gigaset_capi_appl *ap,
		       struct sk_buff *skb)
{
	if (printk_timed_ratelimit(&ignored_msg_dump_time, 30 * 1000)) {
		
		capi_message2cmsg(&iif->acmsg, skb->data);
		dump_cmsg(DEBUG_CMD, __func__, &iif->acmsg);
	}
	dev_kfree_skb_any(skb);
}

static void do_data_b3_resp(struct gigaset_capi_ctr *iif,
			    struct gigaset_capi_appl *ap,
			    struct sk_buff *skb)
{
	dump_rawmsg(DEBUG_MCMD, __func__, skb->data);
	dev_kfree_skb_any(skb);
}

typedef void (*capi_send_handler_t)(struct gigaset_capi_ctr *,
				    struct gigaset_capi_appl *,
				    struct sk_buff *);

static struct {
	u16 cmd;
	capi_send_handler_t handler;
} capi_send_handler_table[] = {
	
	{ CAPI_DATA_B3_REQ, do_data_b3_req },
	{ CAPI_DATA_B3_RESP, do_data_b3_resp },

	{ CAPI_ALERT_REQ, do_alert_req },
	{ CAPI_CONNECT_ACTIVE_RESP, do_nothing },
	{ CAPI_CONNECT_B3_ACTIVE_RESP, do_nothing },
	{ CAPI_CONNECT_B3_REQ, do_connect_b3_req },
	{ CAPI_CONNECT_B3_RESP, do_connect_b3_resp },
	{ CAPI_CONNECT_B3_T90_ACTIVE_RESP, do_nothing },
	{ CAPI_CONNECT_REQ, do_connect_req },
	{ CAPI_CONNECT_RESP, do_connect_resp },
	{ CAPI_DISCONNECT_B3_REQ, do_disconnect_b3_req },
	{ CAPI_DISCONNECT_B3_RESP, do_nothing },
	{ CAPI_DISCONNECT_REQ, do_disconnect_req },
	{ CAPI_DISCONNECT_RESP, do_nothing },
	{ CAPI_FACILITY_REQ, do_facility_req },
	{ CAPI_FACILITY_RESP, do_nothing },
	{ CAPI_LISTEN_REQ, do_listen_req },
	{ CAPI_SELECT_B_PROTOCOL_REQ, do_unsupported },
	{ CAPI_RESET_B3_REQ, do_reset_b3_req },
	{ CAPI_RESET_B3_RESP, do_nothing },

	{ CAPI_INFO_REQ, do_unsupported },
	{ CAPI_INFO_RESP, do_nothing },

	{ CAPI_MANUFACTURER_REQ, do_nothing },
	{ CAPI_MANUFACTURER_RESP, do_nothing },
};

static inline capi_send_handler_t lookup_capi_send_handler(const u16 cmd)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(capi_send_handler_table); i++)
		if (capi_send_handler_table[i].cmd == cmd)
			return capi_send_handler_table[i].handler;
	return NULL;
}


static u16 gigaset_send_message(struct capi_ctr *ctr, struct sk_buff *skb)
{
	struct gigaset_capi_ctr *iif
		= container_of(ctr, struct gigaset_capi_ctr, ctr);
	struct cardstate *cs = ctr->driverdata;
	struct gigaset_capi_appl *ap;
	capi_send_handler_t handler;

	
	if (skb_linearize(skb) < 0) {
		dev_warn(cs->dev, "%s: skb_linearize failed\n", __func__);
		return CAPI_MSGOSRESOURCEERR;
	}

	
	ap = get_appl(iif, CAPIMSG_APPID(skb->data));
	if (!ap) {
		dev_notice(cs->dev, "%s: application %u not registered\n",
			   __func__, CAPIMSG_APPID(skb->data));
		return CAPI_ILLAPPNR;
	}

	
	handler = lookup_capi_send_handler(CAPIMSG_CMD(skb->data));
	if (!handler) {
		
		if (printk_ratelimit())
			dev_notice(cs->dev, "%s: unsupported message %u\n",
				   __func__, CAPIMSG_CMD(skb->data));
		return CAPI_ILLCMDORSUBCMDORMSGTOSMALL;
	}

	
	if (atomic_add_return(1, &iif->sendqlen) > 1) {
		
		skb_queue_tail(&iif->sendqueue, skb);
		return CAPI_NOERROR;
	}

	
	handler(iif, ap, skb);

	
	while (atomic_sub_return(1, &iif->sendqlen) > 0) {
		skb = skb_dequeue(&iif->sendqueue);
		if (!skb) {
			
			dev_err(cs->dev, "%s: send queue empty\n", __func__);
			continue;
		}
		ap = get_appl(iif, CAPIMSG_APPID(skb->data));
		if (!ap) {
			
			dev_warn(cs->dev, "%s: application %u vanished\n",
				 __func__, CAPIMSG_APPID(skb->data));
			continue;
		}
		handler = lookup_capi_send_handler(CAPIMSG_CMD(skb->data));
		if (!handler) {
			
			dev_err(cs->dev, "%s: handler %x vanished\n",
				__func__, CAPIMSG_CMD(skb->data));
			continue;
		}
		handler(iif, ap, skb);
	}

	return CAPI_NOERROR;
}

static char *gigaset_procinfo(struct capi_ctr *ctr)
{
	return ctr->name;	
}

static int gigaset_proc_show(struct seq_file *m, void *v)
{
	struct capi_ctr *ctr = m->private;
	struct cardstate *cs = ctr->driverdata;
	char *s;
	int i;

	seq_printf(m, "%-16s %s\n", "name", ctr->name);
	seq_printf(m, "%-16s %s %s\n", "dev",
		   dev_driver_string(cs->dev), dev_name(cs->dev));
	seq_printf(m, "%-16s %d\n", "id", cs->myid);
	if (cs->gotfwver)
		seq_printf(m, "%-16s %d.%d.%d.%d\n", "firmware",
			   cs->fwver[0], cs->fwver[1], cs->fwver[2], cs->fwver[3]);
	seq_printf(m, "%-16s %d\n", "channels", cs->channels);
	seq_printf(m, "%-16s %s\n", "onechannel", cs->onechannel ? "yes" : "no");

	switch (cs->mode) {
	case M_UNKNOWN:
		s = "unknown";
		break;
	case M_CONFIG:
		s = "config";
		break;
	case M_UNIMODEM:
		s = "Unimodem";
		break;
	case M_CID:
		s = "CID";
		break;
	default:
		s = "??";
	}
	seq_printf(m, "%-16s %s\n", "mode", s);

	switch (cs->mstate) {
	case MS_UNINITIALIZED:
		s = "uninitialized";
		break;
	case MS_INIT:
		s = "init";
		break;
	case MS_LOCKED:
		s = "locked";
		break;
	case MS_SHUTDOWN:
		s = "shutdown";
		break;
	case MS_RECOVER:
		s = "recover";
		break;
	case MS_READY:
		s = "ready";
		break;
	default:
		s = "??";
	}
	seq_printf(m, "%-16s %s\n", "mstate", s);

	seq_printf(m, "%-16s %s\n", "running", cs->running ? "yes" : "no");
	seq_printf(m, "%-16s %s\n", "connected", cs->connected ? "yes" : "no");
	seq_printf(m, "%-16s %s\n", "isdn_up", cs->isdn_up ? "yes" : "no");
	seq_printf(m, "%-16s %s\n", "cidmode", cs->cidmode ? "yes" : "no");

	for (i = 0; i < cs->channels; i++) {
		seq_printf(m, "[%d]%-13s %d\n", i, "corrupted",
			   cs->bcs[i].corrupted);
		seq_printf(m, "[%d]%-13s %d\n", i, "trans_down",
			   cs->bcs[i].trans_down);
		seq_printf(m, "[%d]%-13s %d\n", i, "trans_up",
			   cs->bcs[i].trans_up);
		seq_printf(m, "[%d]%-13s %d\n", i, "chstate",
			   cs->bcs[i].chstate);
		switch (cs->bcs[i].proto2) {
		case L2_BITSYNC:
			s = "bitsync";
			break;
		case L2_HDLC:
			s = "HDLC";
			break;
		case L2_VOICE:
			s = "voice";
			break;
		default:
			s = "??";
		}
		seq_printf(m, "[%d]%-13s %s\n", i, "proto2", s);
	}
	return 0;
}

static int gigaset_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, gigaset_proc_show, PDE(inode)->data);
}

static const struct file_operations gigaset_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= gigaset_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int gigaset_isdn_regdev(struct cardstate *cs, const char *isdnid)
{
	struct gigaset_capi_ctr *iif;
	int rc;

	iif = kmalloc(sizeof(*iif), GFP_KERNEL);
	if (!iif) {
		pr_err("%s: out of memory\n", __func__);
		return 0;
	}

	
	iif->ctr.owner         = THIS_MODULE;
	iif->ctr.driverdata    = cs;
	strncpy(iif->ctr.name, isdnid, sizeof(iif->ctr.name));
	iif->ctr.driver_name   = "gigaset";
	iif->ctr.load_firmware = NULL;
	iif->ctr.reset_ctr     = NULL;
	iif->ctr.register_appl = gigaset_register_appl;
	iif->ctr.release_appl  = gigaset_release_appl;
	iif->ctr.send_message  = gigaset_send_message;
	iif->ctr.procinfo      = gigaset_procinfo;
	iif->ctr.proc_fops = &gigaset_proc_fops;
	INIT_LIST_HEAD(&iif->appls);
	skb_queue_head_init(&iif->sendqueue);
	atomic_set(&iif->sendqlen, 0);

	
	rc = attach_capi_ctr(&iif->ctr);
	if (rc) {
		pr_err("attach_capi_ctr failed (%d)\n", rc);
		kfree(iif);
		return 0;
	}

	cs->iif = iif;
	cs->hw_hdr_len = CAPI_DATA_B3_REQ_LEN;
	return 1;
}

void gigaset_isdn_unregdev(struct cardstate *cs)
{
	struct gigaset_capi_ctr *iif = cs->iif;

	detach_capi_ctr(&iif->ctr);
	kfree(iif);
	cs->iif = NULL;
}

static struct capi_driver capi_driver_gigaset = {
	.name		= "gigaset",
	.revision	= "1.0",
};

void gigaset_isdn_regdrv(void)
{
	pr_info("Kernel CAPI interface\n");
	register_capi_driver(&capi_driver_gigaset);
}

void gigaset_isdn_unregdrv(void)
{
	unregister_capi_driver(&capi_driver_gigaset);
}
