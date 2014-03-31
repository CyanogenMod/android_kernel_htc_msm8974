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
 * File: srom.h
 *
 * Purpose: Implement functions to access eeprom
 *
 * Author: Jerry Chen
 *
 * Date: Jan 29, 2003
 *
 */

#ifndef __SROM_H__
#define __SROM_H__

#include "ttype.h"


#define EEP_MAX_CONTEXT_SIZE    256

#define CB_EEPROM_READBYTE_WAIT 900     

#define W_MAX_I2CRETRY          0x0fff

#define EEP_OFS_PAR         0x00        
#define EEP_OFS_ANTENNA     0x16
#define EEP_OFS_RADIOCTL    0x17
#define EEP_OFS_RFTYPE      0x1B        
#define EEP_OFS_MINCHANNEL  0x1C        
#define EEP_OFS_MAXCHANNEL  0x1D        
#define EEP_OFS_SIGNATURE   0x1E        
#define EEP_OFS_ZONETYPE    0x1F        
#define EEP_OFS_RFTABLE     0x20        
#define EEP_OFS_PWR_CCK     0x20
#define EEP_OFS_SETPT_CCK   0x21
#define EEP_OFS_PWR_OFDMG   0x23
#define EEP_OFS_SETPT_OFDMG 0x24
#define EEP_OFS_PWR_FORMULA_OST  0x26   
#define EEP_OFS_MAJOR_VER 0x2E
#define EEP_OFS_MINOR_VER 0x2F
#define EEP_OFS_CCK_PWR_TBL     0x30
#define EEP_OFS_CCK_PWR_dBm     0x3F
#define EEP_OFS_OFDM_PWR_TBL    0x40
#define EEP_OFS_OFDM_PWR_dBm    0x4F
#define EEP_OFS_SETPT_OFDMA         0x4E
#define EEP_OFS_OFDMA_PWR_TBL       0x50
#define EEP_OFS_OFDMA_PWR_dBm       0xD2


#define EEP_OFS_BBTAB_LEN   0x70        
#define EEP_OFS_BBTAB_ADR   0x71        
#define EEP_OFS_CHECKSUM    0xFF        

#define EEP_I2C_DEV_ID      0x50        


#define EEP_ANTENNA_MAIN    0x01
#define EEP_ANTENNA_AUX     0x02
#define EEP_ANTINV          0x04

#define EEP_RADIOCTL_ENABLE 0x80
#define EEP_RADIOCTL_INV    0x01


typedef struct tagSSromReg {
    unsigned char abyPAR[6];                  

    unsigned short wSUB_VID;                   
    unsigned short wSUB_SID;

    unsigned char byBCFG0;                    
    unsigned char byBCFG1;

    unsigned char byFCR0;                     
    unsigned char byFCR1;
    unsigned char byPMC0;                     
    unsigned char byPMC1;
    unsigned char byMAXLAT;                   
    unsigned char byMINGNT;
    unsigned char byCFG0;                     
    unsigned char byCFG1;
    unsigned short wCISPTR;                    
    unsigned short wRsv0;                      
    unsigned short wRsv1;                      
    unsigned char byBBPAIR;                   
    unsigned char byRFTYPE;
    unsigned char byMinChannel;               
    unsigned char byMaxChannel;
    unsigned char bySignature;                
    unsigned char byCheckSum;

    unsigned char abyReserved0[96];           
    unsigned char abyCIS[128];                
} SSromReg, *PSSromReg;





unsigned char SROMbyReadEmbedded(unsigned long dwIoBase, unsigned char byContntOffset);
bool SROMbWriteEmbedded(unsigned long dwIoBase, unsigned char byContntOffset, unsigned char byData);

void SROMvRegBitsOn(unsigned long dwIoBase, unsigned char byContntOffset, unsigned char byBits);
void SROMvRegBitsOff(unsigned long dwIoBase, unsigned char byContntOffset, unsigned char byBits);

bool SROMbIsRegBitsOn(unsigned long dwIoBase, unsigned char byContntOffset, unsigned char byTestBits);
bool SROMbIsRegBitsOff(unsigned long dwIoBase, unsigned char byContntOffset, unsigned char byTestBits);

void SROMvReadAllContents(unsigned long dwIoBase, unsigned char *pbyEepromRegs);
void SROMvWriteAllContents(unsigned long dwIoBase, unsigned char *pbyEepromRegs);

void SROMvReadEtherAddress(unsigned long dwIoBase, unsigned char *pbyEtherAddress);
void SROMvWriteEtherAddress(unsigned long dwIoBase, unsigned char *pbyEtherAddress);

void SROMvReadSubSysVenId(unsigned long dwIoBase, unsigned long *pdwSubSysVenId);

bool SROMbAutoLoad (unsigned long dwIoBase);

#endif 
