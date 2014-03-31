/*********************************************************************
 *                
 * Filename:      irlap_frame.h
 * Version:       0.9
 * Description:   IrLAP frame declarations
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Tue Aug 19 10:27:26 1997
 * Modified at:   Sat Dec 25 21:07:26 1999
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 *     Copyright (c) 1997-1999 Dag Brattli <dagb@cs.uit.no>,
 *     All Rights Reserved.
 *     Copyright (c) 2000-2002 Jean Tourrilhes <jt@hpl.hp.com>
 *     
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License 
 *     along with this program; if not, write to the Free Software 
 *     Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 *     MA 02111-1307 USA
 *     
 ********************************************************************/

#ifndef IRLAP_FRAME_H
#define IRLAP_FRAME_H

#include <linux/skbuff.h>

#include <net/irda/irda.h>

struct irlap_cb;
struct discovery_t;

#define INVALID   0xff

#define SNRM_CMD  0x83 
#define DISC_CMD  0x43 
#define XID_CMD   0x2f 
#define TEST_CMD  0xe3 

#define RNRM_RSP  0x83 
#define UA_RSP    0x63 
#define FRMR_RSP  0x87 
#define DM_RSP    0x0f 
#define RD_RSP    0x43 
#define XID_RSP   0xaf 
#define TEST_RSP  0xe3 

#define RR        0x01 
#define REJ       0x09 
#define RNR       0x05 
#define SREJ      0x0d 

#define I_FRAME   0x00 
#define UI_FRAME  0x03 

#define CMD_FRAME 0x01
#define RSP_FRAME 0x00

#define PF_BIT    0x10 

#define IRLAP_NEGOCIATION_PARAMS_LEN 25
#define IRLAP_DISCOVERY_INFO_LEN     32

struct disc_frame {
	__u8 caddr;          
	__u8 control;
} __packed;

struct xid_frame {
	__u8  caddr; 
	__u8  control;
	__u8  ident;  
	__le32 saddr; 
	__le32 daddr; 
	__u8  flags; 
	__u8  slotnr;
	__u8  version;
} __packed;

struct test_frame {
	__u8 caddr;          
	__u8 control;
	__le32 saddr;         
	__le32 daddr;         
} __packed;

struct ua_frame {
	__u8 caddr;
	__u8 control;
	__le32 saddr; 
	__le32 daddr; 
} __packed;

struct dm_frame {
	__u8 caddr;          
	__u8 control;
} __packed;

struct rd_frame {
	__u8 caddr;          
	__u8 control;
} __packed;

struct rr_frame {
	__u8 caddr;          
	__u8 control;
} __packed;

struct i_frame {
	__u8 caddr;
	__u8 control;
} __packed;

struct snrm_frame {
	__u8  caddr;
	__u8  control;
	__le32 saddr;
	__le32 daddr;
	__u8  ncaddr;
} __packed;

void irlap_queue_xmit(struct irlap_cb *self, struct sk_buff *skb);
void irlap_send_discovery_xid_frame(struct irlap_cb *, int S, __u8 s, 
				    __u8 command,
				    struct discovery_t *discovery);
void irlap_send_snrm_frame(struct irlap_cb *, struct qos_info *);
void irlap_send_test_frame(struct irlap_cb *self, __u8 caddr, __u32 daddr, 
			   struct sk_buff *cmd);
void irlap_send_ua_response_frame(struct irlap_cb *, struct qos_info *);
void irlap_send_dm_frame(struct irlap_cb *self);
void irlap_send_rd_frame(struct irlap_cb *self);
void irlap_send_disc_frame(struct irlap_cb *self);
void irlap_send_rr_frame(struct irlap_cb *self, int command);

void irlap_send_data_primary(struct irlap_cb *, struct sk_buff *);
void irlap_send_data_primary_poll(struct irlap_cb *, struct sk_buff *);
void irlap_send_data_secondary(struct irlap_cb *, struct sk_buff *);
void irlap_send_data_secondary_final(struct irlap_cb *, struct sk_buff *);
void irlap_resend_rejected_frames(struct irlap_cb *, int command);
void irlap_resend_rejected_frame(struct irlap_cb *self, int command);

void irlap_send_ui_frame(struct irlap_cb *self, struct sk_buff *skb,
			 __u8 caddr, int command);

extern int irlap_insert_qos_negotiation_params(struct irlap_cb *self, 
					       struct sk_buff *skb);

#endif
