/*
 * Broadcom USB remote download definitions
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: usbrdl.h 296577 2011-11-16 03:09:51Z $
 */

#ifndef _USB_RDL_H
#define _USB_RDL_H

#define DL_GETSTATE		0	
#define DL_CHECK_CRC		1	
#define DL_GO			2	
#define DL_START		3	
#define DL_REBOOT		4	
#define DL_GETVER		5	
#define DL_GO_PROTECTED		6	
#define DL_EXEC			7	
#define DL_RESETCFG		8	
#define DL_DEFER_RESP_OK	9	

#define	DL_HWCMD_MASK		0xfc	
#define	DL_RDHW			0x10	
#define	DL_RDHW32		0x10	
#define	DL_RDHW16		0x11	
#define	DL_RDHW8		0x12	
#define	DL_WRHW			0x14	
#define DL_WRHW_BLK 	0x13	

#define DL_CMD_RDHW		1	
#define DL_CMD_WRHW		2	

#ifndef LINUX_POSTMOGRIFY_REMOVAL
#define	DL_JTCONF		0x15	
#define	DL_JTON			0x16	
#define	DL_JTOFF		0x17	
#define	DL_RDRJT		0x18	
#define	DL_WRJT			0x19	
#define	DL_WRRJT		0x1a	
#define	DL_JTRST		0x1b	

#define	DL_RDJT			0x1c	
#define	DL_RDJT32		0x1c	
#define	DL_RDJT16		0x1e	
#define	DL_RDJT8		0x1f	

#define	DL_MRDJT		0x20	
#define	DL_MRDJT32		0x20	
#define	DL_MRDJT16		0x22	
#define	DL_MRDJT6		0x23	
#define	DL_MRDIJT		0x24	
#define	DL_MRDIJT32		0x24	
#define	DL_MRDIJT16		0x26	
#define	DL_MRDIJT8		0x27	
#define	DL_MRDDJT		0x28	
#define	DL_MRDDJT32		0x28	
#define	DL_MRDDJT16		0x2a	
#define	DL_MRDDJT8		0x2b	
#define	DL_MWRJT		0x2c	
#define	DL_MWRIJT		0x2d	
#define	DL_MWRDJT		0x2e	
#define	DL_VRDJT		0x2f	
#define	DL_VWRJT		0x30	
#define	DL_SCJT			0x31	

#define	DL_CFRD			0x33	
#define	DL_CFWR			0x34	
#define DL_GET_NVRAM            0x35    

#define	DL_DBGTRIG		0xFF	

#define	DL_JTERROR		0x80000000
#endif 

#define DL_WAITING	0	
#define DL_READY	1	
#define DL_BAD_HDR	2	
#define DL_BAD_CRC	3	
#define DL_RUNNABLE	4	
#define DL_START_FAIL	5	
#define DL_NVRAM_TOOBIG	6	
#define DL_IMAGE_TOOBIG	7	

#define TIMEOUT		5000	

struct bcm_device_id {
	char	*name;
	uint32	vend;
	uint32	prod;
};

typedef struct {
	uint32	state;
	uint32	bytes;
} rdl_state_t;

typedef struct {
	uint32	chip;		
	uint32	chiprev;	
	uint32  ramsize;    
	uint32  remapbase;   
	uint32  boardtype;   
	uint32  boardrev;    
} bootrom_id_t;

typedef struct {
	uint32	cmd;		
	uint32	addr;		
	uint32	len;		
	uint32	data;		
} hwacc_t;

typedef struct {
	uint32  cmd;            
	uint32  addr;           
	uint32  len;            
	uint8   data[1];                
} hwacc_blk_t;

#ifndef LINUX_POSTMOGRIFY_REMOVAL
typedef struct {
	uint32  chip;           
	uint32  chiprev;        
	uint32  ccrev;          
	uint32  siclock;        
} jtagd_id_t;

typedef struct {
	uint32	cmd;		
	uint8	clkd;		
	uint8	disgpio;	
	uint8	irsz;		
	uint8	drsz;		

	uint8	bigend;		
	uint8	mode;		
	uint16	delay;		

	uint32	retries;	
	uint32	ctrl;		
	uint32	ir_lvbase;	
	uint32	dretries;	
} jtagconf_t;

#define MAX_USB_IR_BITS	256
#define MAX_USB_DR_BITS	3072
#define USB_IR_WORDS	(MAX_USB_IR_BITS / 32)
#define USB_DR_WORDS	(MAX_USB_DR_BITS / 32)
typedef struct {
	uint32	cmd;		
	uint32	irsz;		
	uint32	drsz;		
	uint32	ts;		
	uint32	data[USB_IR_WORDS + USB_DR_WORDS];	
} scjt_t;
#endif 

#define QUERY_STRING_MAX 32
typedef struct {
	uint32  cmd;                    
	char    var[QUERY_STRING_MAX];  
} nvparam_t;

typedef void (*exec_fn_t)(void *sih);

#define USB_CTRL_IN (USB_TYPE_VENDOR | 0x80 | USB_RECIP_INTERFACE)
#define USB_CTRL_OUT (USB_TYPE_VENDOR | 0 | USB_RECIP_INTERFACE)

#define USB_CTRL_EP_TIMEOUT 500 

#define RDL_CHUNK	1500  

#define TRX_OFFSETS_DLFWLEN_IDX	0	
#define TRX_OFFSETS_JUMPTO_IDX	1	
#define TRX_OFFSETS_NVM_LEN_IDX	2	

#define TRX_OFFSETS_DLBASE_IDX  0       

#endif  
