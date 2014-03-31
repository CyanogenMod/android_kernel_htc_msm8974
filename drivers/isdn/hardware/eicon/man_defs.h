/*
 *
 Copyright (c) Eicon Networks, 2002.
 *
 This source file is supplied for the use with
 Eicon Networks range of DIVA Server Adapters.
 *
 Eicon File Revision :    1.9
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


#define MI_DIR          0x01  
#define MI_EXECUTE      0x02  
#define MI_ASCIIZ       0x03  
#define MI_ASCII        0x04  
#define MI_NUMBER       0x05  
#define MI_TRACE        0x06  

#define MI_FIXED_LENGTH 0x80  
#define MI_INT          0x81  
#define MI_UINT         0x82  
#define MI_HINT         0x83  
#define MI_HSTR         0x84  
#define MI_BOOLEAN      0x85  
#define MI_IP_ADDRESS   0x86  
#define MI_BITFLD       0x87  
#define MI_SPID_STATE   0x88  

#define MI_WRITE        0x01  
#define MI_EVENT        0x02  

#define MI_LOCKED       0x01  
#define MI_EVENT_ON     0x02  
#define MI_PROTECTED    0x04  

typedef struct mi_xlog_hdr_s MI_XLOG_HDR;
struct mi_xlog_hdr_s
{
	unsigned long  time;   
	unsigned short size;   
	unsigned short code;   
};                       

#define TM_D_CHAN   0x0001  
#define TM_L_LAYER  0x0002  
#define TM_N_LAYER  0x0004  
#define TM_DL_ERR   0x0008  
#define TM_LAYER1   0x0010  
#define TM_C_COMM   0x0020  
#define TM_M_DATA   0x0040  
#define TM_STRING   0x0080  
#define TM_N_USED2  0x0100  
#define TM_N_USED3  0x0200  
#define TM_N_USED4  0x0400  
#define TM_N_USED5  0x0800  
#define TM_N_USED6  0x1000  
#define TM_N_USED7  0x2000  
#define TM_N_USED8  0x4000  
#define TM_REST     0x8000  

