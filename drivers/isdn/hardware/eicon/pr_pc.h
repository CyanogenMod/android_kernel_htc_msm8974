
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
struct pr_ram {
	word NextReq;         
	word NextRc;          
	word NextInd;         
	byte ReqInput;        
	byte ReqOutput;       
	byte ReqReserved;     
	byte Int;             
	byte XLock;           
	byte RcOutput;        
	byte IndOutput;       
	byte IMask;           
	byte Reserved1[2];    
	byte ReadyInt;        
	byte Reserved2[12];   
	byte InterfaceType;   
	word Signature;       
	byte B[1];            
};
typedef struct {
	word next;
	byte Req;
	byte ReqId;
	byte ReqCh;
	byte Reserved1;
	word Reference;
	byte Reserved[8];
	PBUFFER XBuffer;
} REQ;
typedef struct {
	word next;
	byte Rc;
	byte RcId;
	byte RcCh;
	byte Reserved1;
	word Reference;
	byte Reserved2[8];
} RC;
typedef struct {
	word next;
	byte Ind;
	byte IndId;
	byte IndCh;
	byte MInd;
	word MLength;
	word Reference;
	byte RNR;
	byte Reserved;
	dword Ack;
	PBUFFER RBuffer;
} IND;
