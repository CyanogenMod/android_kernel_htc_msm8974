/* $Id: capi.h,v 1.6.6.2 2001/09/23 22:24:32 kai Exp $
 *
 * ISDN lowlevel-module for the IBM ISDN-S0 Active 2000.
 *
 * Author       Fritz Elfert
 * Copyright    by Fritz Elfert      <fritz@isdn4linux.de>
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 * Thanks to Friedemann Baitinger and IBM Germany
 *
 */

#ifndef CAPI_H
#define CAPI_H

typedef struct actcapi_msgcmd {
	__u8 cmd;
	__u8 subcmd;
} actcapi_msgcmd;

typedef struct actcapi_msghdr {
	__u16 len;
	__u16 applicationID;
	actcapi_msgcmd cmd;
	__u16 msgnum;
} actcapi_msghdr;

typedef struct actcapi_msgdsc {
	actcapi_msgcmd cmd;
	char *description;
} actcapi_msgdsc;

typedef struct actcapi_addr {
	__u8 len;                            
	__u8 tnp;                            
	__u8 num[20];                        
} actcapi_addr;

typedef  union actcapi_infonr {              
	__u16 mask;                          
	struct bmask {                       
		unsigned  codes:3;           
		unsigned  rsvd:5;            
		unsigned  svind:1;           
		unsigned  wtype:7;           
	} bmask;
} actcapi_infonr;

typedef union  actcapi_infoel {              
	__u8 len;                            
	__u8 display[40];                    
	__u8 uuinfo[40];                     
	struct cause {                       
		unsigned ext2:1;             
		unsigned cod:2;              
		unsigned spare:1;            
		unsigned loc:4;              
		unsigned ext1:1;             
		unsigned cval:7;             
	} cause;
	struct charge {                      
		__u8 toc;                    
		__u8 unit[10];               
	} charge;
	__u8 date[20];                       
	__u8 stat;                           
} actcapi_infoel;

typedef struct actcapi_msn {
	__u8 eaz;
	__u8 len;                            
	__u8 msn[15];
}  __attribute__((packed)) actcapi_msn;

typedef struct actcapi_dlpd {
	__u8 len;                            
	__u16 dlen;                          
	__u8 laa;                            
	__u8 lab;                            
	__u8 modulo;                         
	__u8 win;                            
	__u8 xid[100];                       
} __attribute__((packed)) actcapi_dlpd;

typedef struct actcapi_ncpd {
	__u8   len;                          
	__u16  lic;
	__u16  hic;
	__u16  ltc;
	__u16  htc;
	__u16  loc;
	__u16  hoc;
	__u8   modulo;
} __attribute__((packed)) actcapi_ncpd;
#define actcapi_ncpi actcapi_ncpd

#define MAKE_NCCI(plci, contr, ncci)					\
	((plci & 0x1f) | ((contr & 0x7) << 5) | ((ncci & 0xff) << 8))

#define EVAL_NCCI(fakencci, plci, contr, ncci) {	\
		plci  = fakencci & 0x1f;		\
		contr = (fakencci >> 5) & 0x7;		\
		ncci  = (fakencci >> 8) & 0xff;		\
	}

#define MAKE_PLCI(plci, contr)			\
	((plci & 0x1f) | ((contr & 0x7) << 5))

#define EVAL_PLCI(fakeplci, plci, contr) {	\
		plci  = fakeplci & 0x1f;	\
		contr = (fakeplci >> 5) & 0x7;	\
	}

typedef struct actcapi_msg {
	actcapi_msghdr hdr;
	union {
		__u16 manuf_msg;
		struct manufacturer_req_net {
			__u16 manuf_msg;
			__u16 controller;
			__u8  nettype;
		} manufacturer_req_net;
		struct manufacturer_req_v42 {
			__u16 manuf_msg;
			__u16 controller;
			__u32 v42control;
		} manufacturer_req_v42;
		struct manufacturer_conf_v42 {
			__u16 manuf_msg;
			__u16 controller;
		} manufacturer_conf_v42;
		struct manufacturer_req_err {
			__u16 manuf_msg;
			__u16 controller;
		} manufacturer_req_err;
		struct manufacturer_ind_err {
			__u16 manuf_msg;
			__u16 controller;
			__u32 errcode;
			__u8  errstring; 
		} manufacturer_ind_err;
		struct manufacturer_req_msn {
			__u16 manuf_msg;
			__u16 controller;
			actcapi_msn msnmap;
		} __attribute ((packed)) manufacturer_req_msn;
		struct connect_req {
			__u8  controller;
			__u8  bchan;
			__u32 infomask;
			__u8  si1;
			__u8  si2;
			__u8  eaz;
			actcapi_addr addr;
		} __attribute__ ((packed)) connect_req;
		struct connect_conf {
			__u16 plci;
			__u16 info;
		} connect_conf;
		struct connect_ind {
			__u16 plci;
			__u8  controller;
			__u8  si1;
			__u8  si2;
			__u8  eaz;
			actcapi_addr addr;
		} __attribute__ ((packed)) connect_ind;
		struct connect_resp {
			__u16 plci;
			__u8  rejectcause;
		} connect_resp;
		struct connect_active_ind {
			__u16 plci;
			actcapi_addr addr;
		} __attribute__ ((packed)) connect_active_ind;
		struct connect_active_resp {
			__u16 plci;
		} connect_active_resp;
		struct connect_b3_req {
			__u16 plci;
			actcapi_ncpi ncpi;
		} __attribute__ ((packed)) connect_b3_req;
		struct connect_b3_conf {
			__u16 plci;
			__u16 ncci;
			__u16 info;
		} connect_b3_conf;
		struct connect_b3_ind {
			__u16 ncci;
			__u16 plci;
			actcapi_ncpi ncpi;
		} __attribute__ ((packed)) connect_b3_ind;
		struct connect_b3_resp {
			__u16 ncci;
			__u8  rejectcause;
			actcapi_ncpi ncpi;
		} __attribute__ ((packed)) connect_b3_resp;
		struct disconnect_req {
			__u16 plci;
			__u8  cause;
		} disconnect_req;
		struct disconnect_conf {
			__u16 plci;
			__u16 info;
		} disconnect_conf;
		struct disconnect_ind {
			__u16 plci;
			__u16 info;
		} disconnect_ind;
		struct disconnect_resp {
			__u16 plci;
		} disconnect_resp;
		struct connect_b3_active_ind {
			__u16 ncci;
			actcapi_ncpi ncpi;
		} __attribute__ ((packed)) connect_b3_active_ind;
		struct connect_b3_active_resp {
			__u16 ncci;
		} connect_b3_active_resp;
		struct disconnect_b3_req {
			__u16 ncci;
			actcapi_ncpi ncpi;
		} __attribute__ ((packed)) disconnect_b3_req;
		struct disconnect_b3_conf {
			__u16 ncci;
			__u16 info;
		} disconnect_b3_conf;
		struct disconnect_b3_ind {
			__u16 ncci;
			__u16 info;
			actcapi_ncpi ncpi;
		} __attribute__ ((packed)) disconnect_b3_ind;
		struct disconnect_b3_resp {
			__u16 ncci;
		} disconnect_b3_resp;
		struct info_ind {
			__u16 plci;
			actcapi_infonr nr;
			actcapi_infoel el;
		} __attribute__ ((packed)) info_ind;
		struct info_resp {
			__u16 plci;
		} info_resp;
		struct listen_b3_req {
			__u16 plci;
		} listen_b3_req;
		struct listen_b3_conf {
			__u16 plci;
			__u16 info;
		} listen_b3_conf;
		struct select_b2_protocol_req {
			__u16 plci;
			__u8  protocol;
			actcapi_dlpd dlpd;
		} __attribute__ ((packed)) select_b2_protocol_req;
		struct select_b2_protocol_conf {
			__u16 plci;
			__u16 info;
		} select_b2_protocol_conf;
		struct select_b3_protocol_req {
			__u16 plci;
			__u8  protocol;
			actcapi_ncpd ncpd;
		} __attribute__ ((packed)) select_b3_protocol_req;
		struct select_b3_protocol_conf {
			__u16 plci;
			__u16 info;
		} select_b3_protocol_conf;
		struct listen_req {
			__u8  controller;
			__u32 infomask;
			__u16 eazmask;
			__u16 simask;
		} __attribute__ ((packed)) listen_req;
		struct listen_conf {
			__u8  controller;
			__u16 info;
		} __attribute__ ((packed)) listen_conf;
		struct data_b3_req {
			__u16 fakencci;
			__u16 datalen;
			__u32 unused;
			__u8  blocknr;
			__u16 flags;
		} __attribute ((packed)) data_b3_req;
		struct data_b3_ind {
			__u16 fakencci;
			__u16 datalen;
			__u32 unused;
			__u8  blocknr;
			__u16 flags;
		} __attribute__ ((packed)) data_b3_ind;
		struct data_b3_resp {
			__u16 ncci;
			__u8  blocknr;
		} __attribute__ ((packed)) data_b3_resp;
		struct data_b3_conf {
			__u16 ncci;
			__u8  blocknr;
			__u16 info;
		} __attribute__ ((packed)) data_b3_conf;
	} msg;
} __attribute__ ((packed)) actcapi_msg;

static inline unsigned short
actcapi_nextsmsg(act2000_card *card)
{
	unsigned long flags;
	unsigned short n;

	spin_lock_irqsave(&card->mnlock, flags);
	n = card->msgnum;
	card->msgnum++;
	card->msgnum &= 0x7fff;
	spin_unlock_irqrestore(&card->mnlock, flags);
	return n;
}
#define DEBUG_MSG
#undef DEBUG_DATA_MSG
#undef DEBUG_DUMP_SKB

extern int actcapi_chkhdr(act2000_card *, actcapi_msghdr *);
extern int actcapi_listen_req(act2000_card *);
extern int actcapi_manufacturer_req_net(act2000_card *);
extern int actcapi_manufacturer_req_errh(act2000_card *);
extern int actcapi_manufacturer_req_msn(act2000_card *);
extern int actcapi_connect_req(act2000_card *, act2000_chan *, char *, char, int, int);
extern void actcapi_select_b2_protocol_req(act2000_card *, act2000_chan *);
extern void actcapi_disconnect_b3_req(act2000_card *, act2000_chan *);
extern void actcapi_connect_resp(act2000_card *, act2000_chan *, __u8);
extern void actcapi_dispatch(struct work_struct *);
#ifdef DEBUG_MSG
extern void actcapi_debug_msg(struct sk_buff *skb, int);
#else
#define actcapi_debug_msg(skb, len)
#endif
#endif
