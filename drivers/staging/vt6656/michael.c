/*
 * Copyright (c) 1996, 2003 VIA Networking Technologies, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *
 * File: michael.cpp
 *
 * Purpose: The implementation of LIST data structure.
 *
 * Author: Kyle Hsu
 *
 * Date: Sep 4, 2002
 *
 * Functions:
 *      s_dwGetUINT32 - Convert from BYTE[] to DWORD in a portable way
 *      s_vPutUINT32 - Convert from DWORD to BYTE[] in a portable way
 *      s_vClear - Reset the state to the empty message.
 *      s_vSetKey - Set the key.
 *      MIC_vInit - Set the key.
 *      s_vAppendByte - Append the byte to our word-sized buffer.
 *      MIC_vAppend - call s_vAppendByte.
 *      MIC_vGetMIC - Append the minimum padding and call s_vAppendByte.
 *
 * Revision History:
 *
 */

#include "tmacro.h"
#include "michael.h"



static void s_vClear(void);		
static void s_vSetKey(DWORD dwK0, DWORD dwK1);
static void s_vAppendByte(BYTE b);	

static DWORD  L, R;		
static DWORD  K0, K1;		
static DWORD  M;		
static unsigned int   nBytesInM;	



static void s_vClear(void)
{
	
	L = K0;
	R = K1;
	nBytesInM = 0;
	M = 0;
}

static void s_vSetKey(DWORD dwK0, DWORD dwK1)
{
	
	K0 = dwK0;
	K1 = dwK1;
	
	s_vClear();
}

static void s_vAppendByte(BYTE b)
{
	
	M |= b << (8*nBytesInM);
	nBytesInM++;
	
	if (nBytesInM >= 4) {
		L ^= M;
		R ^= ROL32(L, 17);
		L += R;
		R ^= ((L & 0xff00ff00) >> 8) | ((L & 0x00ff00ff) << 8);
		L += R;
		R ^= ROL32(L, 3);
		L += R;
		R ^= ROR32(L, 2);
		L += R;
		
		M = 0;
		nBytesInM = 0;
	}
}

void MIC_vInit(DWORD dwK0, DWORD dwK1)
{
	
	s_vSetKey(dwK0, dwK1);
}


void MIC_vUnInit(void)
{
	
	K0 = 0;
	K1 = 0;

	
	
	s_vClear();
}

void MIC_vAppend(PBYTE src, unsigned int nBytes)
{
    
	while (nBytes > 0) {
		s_vAppendByte(*src++);
		nBytes--;
	}
}

void MIC_vGetMIC(PDWORD pdwL, PDWORD pdwR)
{
	
	s_vAppendByte(0x5a);
	s_vAppendByte(0);
	s_vAppendByte(0);
	s_vAppendByte(0);
	s_vAppendByte(0);
	
	while (nBytesInM != 0)
		s_vAppendByte(0);
	
	*pdwL = L;
	*pdwR = R;
	
	s_vClear();
}
