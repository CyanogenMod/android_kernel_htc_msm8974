
/*
 *
 Copyright (c) Eicon Networks, 2002.
 *
 This source file is supplied for the use with
 Eicon Networks range of DIVA Server Adapters.
 *
 Eicon File Revision :    2.1
 *
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.
 *
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY OF ANY KIND WHATSOEVER INCLUDING ANY
 implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 See the GNU General Public License for more details.
 *
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#ifndef _DI_DEFS_
#define _DI_DEFS_
typedef struct get_name_s GET_NAME;
typedef struct entity_s ENTITY;
typedef struct buffers_s BUFFERS;
typedef struct postcall_s POSTCALL;
typedef struct get_para_s GET_PARA;
#define BOARD_NAME_LENGTH 9
#define IDI_CALL_LINK_T
#define IDI_CALL_ENTITY_T
typedef void (IDI_CALL_LINK_T *IDI_CALL)(ENTITY IDI_CALL_ENTITY_T *);
typedef struct {
	word length;          
	byte P[270];          
} DBUFFER;
struct get_name_s {
	word command;         
	byte name[BOARD_NAME_LENGTH];
};
struct postcall_s {
	word      command;                           
	word      dummy;                             
	void      (*callback)(void *);      
	void      *context;                          
};
#define REQ_PARA            0x0600   
#define REQ_PARA_LEN             1   
#define L1_STARTUP_DOWN_POS      0   
#define L1_STARTUP_DOWN_MSK   0x01   
struct get_para_s {
	word  command;            
	byte  len;                
	byte  para[REQ_PARA_LEN]; 
};
struct buffers_s {
	word PLength;
	byte *P;
};
struct entity_s {
	byte                  Req;            
	byte                  Rc;             
	byte                  Ind;            
	byte                  ReqCh;          
	byte                  RcCh;           
	byte                  IndCh;          
	byte                  Id;             
	byte                  GlobalId;       
	byte                  XNum;           
	byte                  RNum;           
	BUFFERS               *X;             
	BUFFERS               *R;             
	word                  RLength;        
	DBUFFER               *RBuffer;       
	byte                  RNR;            
	byte                  complete;       
	IDI_CALL              callback;
	word                  user[2];
	
	byte                  No;             
	byte                  reserved2;      
	byte                  More;           
	byte                  MInd;           
	byte                  XCurrent;       
	byte                  RCurrent;       
	word                  XOffset;        
	word                  ROffset;        
};
typedef struct {
	byte                  type;
	byte                  channels;
	word                  features;
	IDI_CALL              request;
} DESCRIPTOR;
#define IDI_ADAPTER_S           1
#define IDI_ADAPTER_PR          2
#define IDI_ADAPTER_DIVA        3
#define IDI_ADAPTER_MAESTRA     4
#define IDI_VADAPTER            0x40
#define IDI_DRIVER              0x80
#define IDI_DADAPTER            0xfd
#define IDI_DIDDPNP             0xfe
#define IDI_DIMAINT             0xff
#define HW_ID_DIVA_PRO     3    
#define HW_ID_MAESTRA      4    
#define HW_ID_PICCOLA      5
#define HW_ID_DIVA_PRO20   6
#define HW_ID_DIVA20       7
#define HW_ID_DIVA_PRO20_U 8
#define HW_ID_DIVA20_U     9
#define HW_ID_DIVA30       10
#define HW_ID_DIVA30_U     11
#define HW_ID_EICON_PCI              0x1133
#define HW_ID_SIEMENS_PCI            0x8001 
#define HW_ID_PROTTYPE_CORNETN       0x0014 
#define HW_ID_FUJITSU_SIEMENS_PCI    0x110A 
#define HW_ID_GS03_PCI               0x0021 
#define HW_ID_DIVA_PRO20_PCI         0xe001
#define HW_ID_DIVA20_PCI             0xe002
#define HW_ID_DIVA_PRO20_PCI_U       0xe003
#define HW_ID_DIVA20_PCI_U           0xe004
#define HW_ID_DIVA201_PCI            0xe005
#define HW_ID_DIVA_CT_ST             0xe006
#define HW_ID_DIVA_CT_U              0xe007
#define HW_ID_DIVA_CTL_ST            0xe008
#define HW_ID_DIVA_CTL_U             0xe009
#define HW_ID_DIVA_ISDN_V90_PCI      0xe00a
#define HW_ID_DIVA202_PCI_ST         0xe00b
#define HW_ID_DIVA202_PCI_U          0xe00c
#define HW_ID_DIVA_PRO30_PCI         0xe00d
#define HW_ID_MAESTRA_PCI            0xe010
#define HW_ID_MAESTRAQ_PCI           0xe012
#define HW_ID_DSRV_Q8M_V2_PCI        0xe013
#define HW_ID_MAESTRAP_PCI           0xe014
#define HW_ID_DSRV_P30M_V2_PCI       0xe015
#define HW_ID_DSRV_VOICE_Q8M_PCI     0xe016
#define HW_ID_DSRV_VOICE_Q8M_V2_PCI  0xe017
#define HW_ID_DSRV_B2M_V2_PCI        0xe018
#define HW_ID_DSRV_VOICE_P30M_V2_PCI 0xe019
#define HW_ID_DSRV_B2F_PCI           0xe01a
#define HW_ID_DSRV_VOICE_B2M_V2_PCI  0xe01b
#define EICON_USB_VENDOR_ID          0x071D
#define HW_ID_DIVA_USB_REV1          0x1000
#define HW_ID_DIVA_USB_REV2          0x1003
#define HW_ID_TELEDAT_SURF_USB_REV2  0x1004
#define HW_ID_TELEDAT_SURF_USB_REV1  0x2000
typedef void (IDI_CALL_LINK_T *didd_adapter_change_callback_t)(void IDI_CALL_ENTITY_T *context, DESCRIPTOR *adapter, int removal);
#define DI_VOICE          0x0 
#define DI_FAX3           0x1
#define DI_MODEM          0x2
#define DI_POST           0x4
#define DI_V110           0x8
#define DI_V120           0x10
#define DI_POTS           0x20
#define DI_CODEC          0x40
#define DI_MANAGE         0x80
#define DI_V_42           0x0100
#define DI_EXTD_FAX       0x0200 
#define DI_AT_PARSER      0x0400 
#define DI_VOICE_OVER_IP  0x0800 
typedef void (IDI_CALL_LINK_T *_IDI_CALL)(void *, ENTITY *);
#endif
