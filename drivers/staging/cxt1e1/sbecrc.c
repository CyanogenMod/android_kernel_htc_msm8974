/*   Based on "File Verification Using CRC" by Mark R. Nelson in Dr. Dobbs'
 *   Journal, May 1992, pp. 64-67.  This algorithm generates the same CRC
 *   values as ZMODEM and PKZIP
 *
 * Copyright (C) 2002-2005  SBE, Inc.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

#include <linux/types.h>
#include "pmcc4_sysdep.h"
#include "sbecom_inline_linux.h"
#include "sbe_promformat.h"

#define CRC32_POLYNOMIAL                0xEDB88320L
#define CRC_TABLE_ENTRIES                       256



static      u_int32_t crcTableInit;

#ifdef STATIC_CRC_TABLE
static u_int32_t CRCTable[CRC_TABLE_ENTRIES];

#endif



static void
genCrcTable (u_int32_t *CRCTable)
{
    int         ii, jj;
    u_int32_t      crc;

    for (ii = 0; ii < CRC_TABLE_ENTRIES; ii++)
    {
        crc = ii;
        for (jj = 8; jj > 0; jj--)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ CRC32_POLYNOMIAL;
            else
                crc >>= 1;
        }
        CRCTable[ii] = crc;
    }

    crcTableInit++;
}



void
sbeCrc (u_int8_t *buffer,          
        u_int32_t count,           
        u_int32_t initialCrc,      
        u_int32_t *result)
{
    u_int32_t     *tbl = 0;
    u_int32_t      temp1, temp2, crc;

    if (!crcTableInit)
    {
#ifdef STATIC_CRC_TABLE
        tbl = &CRCTable;
        genCrcTable (tbl);
#else
        tbl = (u_int32_t *) OS_kmalloc (CRC_TABLE_ENTRIES * sizeof (u_int32_t));
        if (tbl == 0)
        {
            *result = 0;            
            return;
        }
        genCrcTable (tbl);
#endif
    }
    
    crc = initialCrc ^ 0xFFFFFFFFL;

    while (count-- != 0)
    {
        temp1 = (crc >> 8) & 0x00FFFFFFL;
        temp2 = tbl[((int) crc ^ *buffer++) & 0xff];
        crc = temp1 ^ temp2;
    }

    crc ^= 0xFFFFFFFFL;

    *result = crc;

#ifndef STATIC_CRC_TABLE
    crcTableInit = 0;
    OS_kfree (tbl);
#endif
}

