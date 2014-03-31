/* $Id: isdnif.h,v 1.43.2.2 2004/01/12 23:08:35 keil Exp $
 *
 * Linux ISDN subsystem
 * Definition of the interface between the subsystem and its low-level drivers.
 *
 * Copyright 1994,95,96 by Fritz Elfert (fritz@isdn4linux.de)
 * Copyright 1995,96    Thinking Objects Software GmbH Wuerzburg
 * 
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#ifndef __ISDNIF_H__
#define __ISDNIF_H__


#define ISDN_PTYPE_UNKNOWN   0   
#define ISDN_PTYPE_1TR6      1   
#define ISDN_PTYPE_EURO      2   
#define ISDN_PTYPE_LEASED    3   
#define ISDN_PTYPE_NI1       4   
#define ISDN_PTYPE_MAX       7   

#define ISDN_PROTO_L2_X75I   0   
#define ISDN_PROTO_L2_X75UI  1   
#define ISDN_PROTO_L2_X75BUI 2   
#define ISDN_PROTO_L2_HDLC   3   
#define ISDN_PROTO_L2_TRANS  4   
#define ISDN_PROTO_L2_X25DTE 5   
#define ISDN_PROTO_L2_X25DCE 6   
#define ISDN_PROTO_L2_V11096 7   
#define ISDN_PROTO_L2_V11019 8   
#define ISDN_PROTO_L2_V11038 9   
#define ISDN_PROTO_L2_MODEM  10  
#define ISDN_PROTO_L2_FAX    11  
#define ISDN_PROTO_L2_HDLC_56K 12   
#define ISDN_PROTO_L2_MAX    15  

#define ISDN_PROTO_L3_TRANS	0	
#define ISDN_PROTO_L3_TRANSDSP	1	
#define ISDN_PROTO_L3_FCLASS2	2	
#define ISDN_PROTO_L3_FCLASS1	3	
#define ISDN_PROTO_L3_MAX	7	

#ifdef __KERNEL__

#include <linux/skbuff.h>

 
 
    
                    

 
#define DSS1_CMD_INVOKE       ((0x00 << 8) | ISDN_PTYPE_EURO)   
#define DSS1_CMD_INVOKE_ABORT ((0x01 << 8) | ISDN_PTYPE_EURO)   

#define DSS1_STAT_INVOKE_RES  ((0x80 << 8) | ISDN_PTYPE_EURO)   
#define DSS1_STAT_INVOKE_ERR  ((0x81 << 8) | ISDN_PTYPE_EURO)   
#define DSS1_STAT_INVOKE_BRD  ((0x82 << 8) | ISDN_PTYPE_EURO)   


   

 
#define NI1_CMD_INVOKE       ((0x00 << 8) | ISDN_PTYPE_NI1)   
#define NI1_CMD_INVOKE_ABORT ((0x01 << 8) | ISDN_PTYPE_NI1)   

#define NI1_STAT_INVOKE_RES  ((0x80 << 8) | ISDN_PTYPE_NI1)   
#define NI1_STAT_INVOKE_ERR  ((0x81 << 8) | ISDN_PTYPE_NI1)   
#define NI1_STAT_INVOKE_BRD  ((0x82 << 8) | ISDN_PTYPE_NI1)   

typedef struct
  { ulong ll_id; 
		 
                 
    int hl_id;   
                 
                 
                   
    int proc;    
                  
    int timeout; 
                 
                 
    int datalen; 
    u_char *data;
  } isdn_cmd_stat;

#define ISDN_CMD_IOCTL    0       
#define ISDN_CMD_DIAL     1       
#define ISDN_CMD_ACCEPTD  2       
#define ISDN_CMD_ACCEPTB  3       
#define ISDN_CMD_HANGUP   4       
#define ISDN_CMD_CLREAZ   5       
#define ISDN_CMD_SETEAZ   6       
#define ISDN_CMD_GETEAZ   7       
#define ISDN_CMD_SETSIL   8       
#define ISDN_CMD_GETSIL   9       
#define ISDN_CMD_SETL2   10       
#define ISDN_CMD_GETL2   11       
#define ISDN_CMD_SETL3   12       
#define ISDN_CMD_GETL3   13       
#define ISDN_CMD_SUSPEND 16       
#define ISDN_CMD_RESUME  17       
#define ISDN_CMD_PROCEED 18       
#define ISDN_CMD_ALERT   19       
#define ISDN_CMD_REDIR   20       
#define ISDN_CMD_PROT_IO 21       
#define CAPI_PUT_MESSAGE 22       
#define ISDN_CMD_FAXCMD  23       
#define ISDN_CMD_AUDIO   24       

#define ISDN_STAT_STAVAIL 256    
#define ISDN_STAT_ICALL   257    
#define ISDN_STAT_RUN     258    
#define ISDN_STAT_STOP    259    
#define ISDN_STAT_DCONN   260    
#define ISDN_STAT_BCONN   261    
#define ISDN_STAT_DHUP    262    
#define ISDN_STAT_BHUP    263    
#define ISDN_STAT_CINF    264    
#define ISDN_STAT_LOAD    265    
#define ISDN_STAT_UNLOAD  266    
#define ISDN_STAT_BSENT   267    
#define ISDN_STAT_NODCH   268    
#define ISDN_STAT_ADDCH   269    
#define ISDN_STAT_CAUSE   270    
#define ISDN_STAT_ICALLW  271    
#define ISDN_STAT_REDIR   272    
#define ISDN_STAT_PROT    273    
#define ISDN_STAT_DISPLAY 274    
#define ISDN_STAT_L1ERR   275    
#define ISDN_STAT_FAXIND  276    
#define ISDN_STAT_AUDIO   277    
#define ISDN_STAT_DISCH   278    

#define ISDN_AUDIO_SETDD	0	
#define ISDN_AUDIO_DTMF		1	

#define ISDN_STAT_L1ERR_SEND 1
#define ISDN_STAT_L1ERR_RECV 2

#define ISDN_FEATURE_L2_X75I    (0x0001 << ISDN_PROTO_L2_X75I)
#define ISDN_FEATURE_L2_X75UI   (0x0001 << ISDN_PROTO_L2_X75UI)
#define ISDN_FEATURE_L2_X75BUI  (0x0001 << ISDN_PROTO_L2_X75BUI)
#define ISDN_FEATURE_L2_HDLC    (0x0001 << ISDN_PROTO_L2_HDLC)
#define ISDN_FEATURE_L2_TRANS   (0x0001 << ISDN_PROTO_L2_TRANS)
#define ISDN_FEATURE_L2_X25DTE  (0x0001 << ISDN_PROTO_L2_X25DTE)
#define ISDN_FEATURE_L2_X25DCE  (0x0001 << ISDN_PROTO_L2_X25DCE)
#define ISDN_FEATURE_L2_V11096  (0x0001 << ISDN_PROTO_L2_V11096)
#define ISDN_FEATURE_L2_V11019  (0x0001 << ISDN_PROTO_L2_V11019)
#define ISDN_FEATURE_L2_V11038  (0x0001 << ISDN_PROTO_L2_V11038)
#define ISDN_FEATURE_L2_MODEM   (0x0001 << ISDN_PROTO_L2_MODEM)
#define ISDN_FEATURE_L2_FAX	(0x0001 << ISDN_PROTO_L2_FAX)
#define ISDN_FEATURE_L2_HDLC_56K (0x0001 << ISDN_PROTO_L2_HDLC_56K)

#define ISDN_FEATURE_L2_MASK    (0x0FFFF) 
#define ISDN_FEATURE_L2_SHIFT   (0)

#define ISDN_FEATURE_L3_TRANS   (0x10000 << ISDN_PROTO_L3_TRANS)
#define ISDN_FEATURE_L3_TRANSDSP (0x10000 << ISDN_PROTO_L3_TRANSDSP)
#define ISDN_FEATURE_L3_FCLASS2	(0x10000 << ISDN_PROTO_L3_FCLASS2)
#define ISDN_FEATURE_L3_FCLASS1	(0x10000 << ISDN_PROTO_L3_FCLASS1)

#define ISDN_FEATURE_L3_MASK    (0x0FF0000) 
#define ISDN_FEATURE_L3_SHIFT   (16)

#define ISDN_FEATURE_P_UNKNOWN  (0x1000000 << ISDN_PTYPE_UNKNOWN)
#define ISDN_FEATURE_P_1TR6     (0x1000000 << ISDN_PTYPE_1TR6)
#define ISDN_FEATURE_P_EURO     (0x1000000 << ISDN_PTYPE_EURO)
#define ISDN_FEATURE_P_NI1      (0x1000000 << ISDN_PTYPE_NI1)

#define ISDN_FEATURE_P_MASK     (0x0FF000000) 
#define ISDN_FEATURE_P_SHIFT    (24)

typedef struct setup_parm {
    unsigned char phone[32];	
    unsigned char eazmsn[32];	
    unsigned char si1;      
    unsigned char si2;      
    unsigned char plan;     
    unsigned char screen;   
} setup_parm;


#ifdef CONFIG_ISDN_TTY_FAX

#define FAXIDLEN 21

typedef struct T30_s {
	
	__u8 resolution;
	__u8 rate;
	__u8 width;
	__u8 length;
	__u8 compression;
	__u8 ecm;
	__u8 binary;
	__u8 scantime;
	__u8 id[FAXIDLEN];
	
	__u8 phase;
	__u8 direction;
	__u8 code;
	__u8 badlin;
	__u8 badmul;
	__u8 bor;
	__u8 fet;
	__u8 pollid[FAXIDLEN];
	__u8 cq;
	__u8 cr;
	__u8 ctcrty;
	__u8 minsp;
	__u8 phcto;
	__u8 rel;
	__u8 nbc;
	
	__u8 r_resolution;
	__u8 r_rate;
	__u8 r_width;
	__u8 r_length;
	__u8 r_compression;
	__u8 r_ecm;
	__u8 r_binary;
	__u8 r_scantime;
	__u8 r_id[FAXIDLEN];
	__u8 r_code;
} __packed T30_s;

#define ISDN_TTY_FAX_CONN_IN	0
#define ISDN_TTY_FAX_CONN_OUT	1

#define ISDN_TTY_FAX_FCON	0
#define ISDN_TTY_FAX_DIS 	1
#define ISDN_TTY_FAX_FTT 	2
#define ISDN_TTY_FAX_MCF 	3
#define ISDN_TTY_FAX_DCS 	4
#define ISDN_TTY_FAX_TRAIN_OK	5
#define ISDN_TTY_FAX_EOP 	6
#define ISDN_TTY_FAX_EOM 	7
#define ISDN_TTY_FAX_MPS 	8
#define ISDN_TTY_FAX_DTC 	9
#define ISDN_TTY_FAX_RID 	10
#define ISDN_TTY_FAX_HNG 	11
#define ISDN_TTY_FAX_DT  	12
#define ISDN_TTY_FAX_FCON_I	13
#define ISDN_TTY_FAX_DR  	14
#define ISDN_TTY_FAX_ET  	15
#define ISDN_TTY_FAX_CFR 	16
#define ISDN_TTY_FAX_PTS 	17
#define ISDN_TTY_FAX_SENT	18

#define ISDN_FAX_PHASE_IDLE	0
#define ISDN_FAX_PHASE_A	1
#define ISDN_FAX_PHASE_B   	2
#define ISDN_FAX_PHASE_C   	3
#define ISDN_FAX_PHASE_D   	4
#define ISDN_FAX_PHASE_E   	5

#endif 

#define ISDN_FAX_CLASS1_FAE	0
#define ISDN_FAX_CLASS1_FTS	1
#define ISDN_FAX_CLASS1_FRS	2
#define ISDN_FAX_CLASS1_FTM	3
#define ISDN_FAX_CLASS1_FRM	4
#define ISDN_FAX_CLASS1_FTH	5
#define ISDN_FAX_CLASS1_FRH	6
#define ISDN_FAX_CLASS1_CTRL	7

#define ISDN_FAX_CLASS1_OK	0
#define ISDN_FAX_CLASS1_CONNECT	1
#define ISDN_FAX_CLASS1_NOCARR	2
#define ISDN_FAX_CLASS1_ERROR	3
#define ISDN_FAX_CLASS1_FCERROR	4
#define ISDN_FAX_CLASS1_QUERY	5

typedef struct {
	__u8	cmd;
	__u8	subcmd;
	__u8	para[50];
} aux_s;

#define AT_COMMAND	0
#define AT_EQ_VALUE	1
#define AT_QUERY	2
#define AT_EQ_QUERY	3


#define MAX_CAPI_PARA_LEN 50

typedef struct {
	
	__u16 Length;
	__u16 ApplId;
	__u8 Command;
	__u8 Subcommand;
	__u16 Messagenumber;

	
	union {
		__u32 Controller;
		__u32 PLCI;
		__u32 NCCI;
	} adr;
	__u8 para[MAX_CAPI_PARA_LEN];
} capi_msg;

typedef struct {
	int   driver;		
	int   command;		
	ulong arg;		
	union {
		ulong errcode;	
		int length;	
		u_char num[50];	
		setup_parm setup;
		capi_msg cmsg;	
		char display[85]; 
		isdn_cmd_stat isdn_io; 
		aux_s aux;	
#ifdef CONFIG_ISDN_TTY_FAX
		T30_s	*fax;	
#endif
		ulong userdata;	
	} parm;
} isdn_ctrl;

#define dss1_io    isdn_io
#define ni1_io     isdn_io

typedef struct {
  struct module *owner;

  int channels;

  int maxbufsize;

  unsigned long features;

  unsigned short hl_hdrlen;

  void (*rcvcallb_skb)(int, int, struct sk_buff *);

  int (*statcallb)(isdn_ctrl*);

  int (*command)(isdn_ctrl*);

  int (*writebuf_skb) (int, int, int, struct sk_buff *);

  int (*writecmd)(const u_char __user *, int, int, int);

  int (*readstat)(u_char __user *, int, int, int);

  char id[20];
} isdn_if;

extern int register_isdn(isdn_if*);
#include <asm/uaccess.h>

#endif 

#endif 
