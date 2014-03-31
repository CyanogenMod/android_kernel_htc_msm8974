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
 * File: rf.c
 *
 * Purpose: rf function code
 *
 * Author: Jerry Chen
 *
 * Date: Feb. 19, 2004
 *
 * Functions:
 *      IFRFbWriteEmbeded      - Embeded write RF register via MAC
 *
 * Revision History:
 *
 */

#include "mac.h"
#include "rf.h"
#include "baseband.h"
#include "control.h"
#include "rndis.h"
#include "datarate.h"

static int          msglevel                =MSG_LEVEL_INFO;
#define BY_AL2230_REG_LEN     23 
#define CB_AL2230_INIT_SEQ    15
#define AL2230_PWR_IDX_LEN    64

#define BY_AL7230_REG_LEN     23 
#define CB_AL7230_INIT_SEQ    16
#define AL7230_PWR_IDX_LEN    64

#define BY_VT3226_REG_LEN     23
#define CB_VT3226_INIT_SEQ    11
#define VT3226_PWR_IDX_LEN    64

#define BY_VT3342_REG_LEN     23
#define CB_VT3342_INIT_SEQ    13
#define VT3342_PWR_IDX_LEN    64






BYTE abyAL2230InitTable[CB_AL2230_INIT_SEQ][3] = {
    {0x03, 0xF7, 0x90},
    {0x03, 0x33, 0x31},
    {0x01, 0xB8, 0x02},
    {0x00, 0xFF, 0xF3},
    {0x00, 0x05, 0xA4},
    {0x0F, 0x4D, 0xC5},   
    {0x08, 0x05, 0xB6},
    {0x01, 0x47, 0xC7},
    {0x00, 0x06, 0x88},
    {0x04, 0x03, 0xB9},
    {0x00, 0xDB, 0xBA},
    {0x00, 0x09, 0x9B},
    {0x0B, 0xDF, 0xFC},
    {0x00, 0x00, 0x0D},
    {0x00, 0x58, 0x0F}
    };

BYTE abyAL2230ChannelTable0[CB_MAX_CHANNEL_24G][3] = {
    {0x03, 0xF7, 0x90}, 
    {0x03, 0xF7, 0x90}, 
    {0x03, 0xE7, 0x90}, 
    {0x03, 0xE7, 0x90}, 
    {0x03, 0xF7, 0xA0}, 
    {0x03, 0xF7, 0xA0}, 
    {0x03, 0xE7, 0xA0}, 
    {0x03, 0xE7, 0xA0}, 
    {0x03, 0xF7, 0xB0}, 
    {0x03, 0xF7, 0xB0}, 
    {0x03, 0xE7, 0xB0}, 
    {0x03, 0xE7, 0xB0}, 
    {0x03, 0xF7, 0xC0}, 
    {0x03, 0xE7, 0xC0}  
    };

BYTE abyAL2230ChannelTable1[CB_MAX_CHANNEL_24G][3] = {
    {0x03, 0x33, 0x31}, 
    {0x0B, 0x33, 0x31}, 
    {0x03, 0x33, 0x31}, 
    {0x0B, 0x33, 0x31}, 
    {0x03, 0x33, 0x31}, 
    {0x0B, 0x33, 0x31}, 
    {0x03, 0x33, 0x31}, 
    {0x0B, 0x33, 0x31}, 
    {0x03, 0x33, 0x31}, 
    {0x0B, 0x33, 0x31}, 
    {0x03, 0x33, 0x31}, 
    {0x0B, 0x33, 0x31}, 
    {0x03, 0x33, 0x31}, 
    {0x06, 0x66, 0x61}  
    };

BYTE abyAL7230InitTable[CB_AL7230_INIT_SEQ][3] = {
    {0x20, 0x37, 0x90}, 
    {0x13, 0x33, 0x31}, 
    {0x84, 0x1F, 0xF2}, 
    {0x3F, 0xDF, 0xA3}, 
    {0x7F, 0xD7, 0x84}, 
    
    
    {0x80, 0x2B, 0x55}, 
    {0x56, 0xAF, 0x36},
    {0xCE, 0x02, 0x07}, 
    {0x6E, 0xBC, 0x98},
    {0x22, 0x1B, 0xB9},
    {0xE0, 0x00, 0x0A}, 
    {0x08, 0x03, 0x1B}, 
    
    
    {0x00, 0x0A, 0x3C}, 
    {0xFF, 0xFF, 0xFD},
    {0x00, 0x00, 0x0E},
    {0x1A, 0xBA, 0x8F} 
    };

BYTE abyAL7230InitTableAMode[CB_AL7230_INIT_SEQ][3] = {
    {0x2F, 0xF5, 0x20}, 
    {0x00, 0x00, 0x01}, 
    {0x45, 0x1F, 0xE2}, 
    {0x5F, 0xDF, 0xA3}, 
    {0x6F, 0xD7, 0x84}, 
    {0x85, 0x3F, 0x55}, 
    {0x56, 0xAF, 0x36},
    {0xCE, 0x02, 0x07}, 
    {0x6E, 0xBC, 0x98},
    {0x22, 0x1B, 0xB9},
    {0xE0, 0x60, 0x0A}, 
    {0x08, 0x03, 0x1B}, 
    {0x00, 0x14, 0x7C}, 
    {0xFF, 0xFF, 0xFD},
    {0x00, 0x00, 0x0E},
    {0x12, 0xBA, 0xCF} 
    };

BYTE abyAL7230ChannelTable0[CB_MAX_CHANNEL][3] = {
    {0x20, 0x37, 0x90}, 
    {0x20, 0x37, 0x90}, 
    {0x20, 0x37, 0x90}, 
    {0x20, 0x37, 0x90}, 
    {0x20, 0x37, 0xA0}, 
    {0x20, 0x37, 0xA0}, 
    {0x20, 0x37, 0xA0}, 
    {0x20, 0x37, 0xA0}, 
    {0x20, 0x37, 0xB0}, 
    {0x20, 0x37, 0xB0}, 
    {0x20, 0x37, 0xB0}, 
    {0x20, 0x37, 0xB0}, 
    {0x20, 0x37, 0xC0}, 
    {0x20, 0x37, 0xC0}, 

    
    {0x0F, 0xF5, 0x20}, 
    {0x2F, 0xF5, 0x20}, 
    {0x0F, 0xF5, 0x20}, 
    {0x0F, 0xF5, 0x20}, 
    {0x2F, 0xF5, 0x20}, 
    {0x0F, 0xF5, 0x20}, 
    {0x2F, 0xF5, 0x30}, 
    {0x2F, 0xF5, 0x30}, 

    
    

    {0x0F, 0xF5, 0x40}, 
    {0x2F, 0xF5, 0x40}, 
    {0x0F, 0xF5, 0x40}, 
    {0x0F, 0xF5, 0x40}, 
    {0x2F, 0xF5, 0x40}, 
    {0x2F, 0xF5, 0x50}, 
    {0x2F, 0xF5, 0x60}, 
    {0x2F, 0xF5, 0x60}, 
    {0x2F, 0xF5, 0x70}, 
    {0x2F, 0xF5, 0x70}, 
    {0x2F, 0xF5, 0x70}, 
    {0x2F, 0xF5, 0x70}, 
    {0x2F, 0xF5, 0x70}, 
    {0x2F, 0xF5, 0x70}, 
    {0x2F, 0xF5, 0x80}, 
    {0x2F, 0xF5, 0x80}, 
    {0x2F, 0xF5, 0x80}, 
    {0x2F, 0xF5, 0x90}, 

    {0x2F, 0xF5, 0xC0}, 
    {0x2F, 0xF5, 0xC0}, 
    {0x2F, 0xF5, 0xC0}, 
    {0x2F, 0xF5, 0xD0}, 
    {0x2F, 0xF5, 0xD0}, 
    {0x2F, 0xF5, 0xD0}, 
    {0x2F, 0xF5, 0xE0}, 
    {0x2F, 0xF5, 0xE0}, 
    {0x2F, 0xF5, 0xE0}, 
    {0x2F, 0xF5, 0xF0}, 
    {0x2F, 0xF5, 0xF0}, 
    {0x2F, 0xF6, 0x00}, 
    {0x2F, 0xF6, 0x00}, 
    {0x2F, 0xF6, 0x00}, 
    {0x2F, 0xF6, 0x10}, 
    {0x2F, 0xF6, 0x10} 
    };

BYTE abyAL7230ChannelTable1[CB_MAX_CHANNEL][3] = {
    {0x13, 0x33, 0x31}, 
    {0x1B, 0x33, 0x31}, 
    {0x03, 0x33, 0x31}, 
    {0x0B, 0x33, 0x31}, 
    {0x13, 0x33, 0x31}, 
    {0x1B, 0x33, 0x31}, 
    {0x03, 0x33, 0x31}, 
    {0x0B, 0x33, 0x31}, 
    {0x13, 0x33, 0x31}, 
    {0x1B, 0x33, 0x31}, 
    {0x03, 0x33, 0x31}, 
    {0x0B, 0x33, 0x31}, 
    {0x13, 0x33, 0x31}, 
    {0x06, 0x66, 0x61}, 

    
    {0x1D, 0x55, 0x51}, 
    {0x00, 0x00, 0x01}, 
    {0x02, 0xAA, 0xA1}, 
    {0x08, 0x00, 0x01}, 
    {0x0A, 0xAA, 0xA1}, 
    {0x0D, 0x55, 0x51}, 
    {0x15, 0x55, 0x51}, 
    {0x00, 0x00, 0x01}, 

    
    
    {0x1D, 0x55, 0x51}, 
    {0x00, 0x00, 0x01}, 
    {0x02, 0xAA, 0xA1}, 
    {0x08, 0x00, 0x01}, 
    {0x0A, 0xAA, 0xA1}, 
    {0x15, 0x55, 0x51}, 
    {0x05, 0x55, 0x51}, 
    {0x0A, 0xAA, 0xA1}, 
    {0x10, 0x00, 0x01}, 
    {0x15, 0x55, 0x51}, 
    {0x1A, 0xAA, 0xA1}, 
    {0x00, 0x00, 0x01}, 
    {0x05, 0x55, 0x51}, 
    {0x0A, 0xAA, 0xA1}, 
    {0x15, 0x55, 0x51}, 
    {0x00, 0x00, 0x01}, 
    {0x0A, 0xAA, 0xA1}, 
    {0x15, 0x55, 0x51}, 
    {0x15, 0x55, 0x51}, 
    {0x00, 0x00, 0x01}, 
    {0x0A, 0xAA, 0xA1}, 
    {0x15, 0x55, 0x51}, 
    {0x00, 0x00, 0x01}, 
    {0x0A, 0xAA, 0xA1}, 
    {0x15, 0x55, 0x51}, 
    {0x00, 0x00, 0x01}, 
    {0x0A, 0xAA, 0xA1}, 
    {0x15, 0x55, 0x51}, 
    {0x00, 0x00, 0x01}, 
    {0x18, 0x00, 0x01}, 
    {0x02, 0xAA, 0xA1}, 
    {0x0D, 0x55, 0x51}, 
    {0x18, 0x00, 0x01}, 
    {0x02, 0xAA, 0xB1}  
    };

BYTE abyAL7230ChannelTable2[CB_MAX_CHANNEL][3] = {
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 

    
    {0x7F, 0xD7, 0x84}, 
    {0x6F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x6F, 0xD7, 0x84}, 

    
    
    {0x7F, 0xD7, 0x84}, 
    {0x6F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x6F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x6F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x6F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x6F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x6F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x6F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}, 
    {0x7F, 0xD7, 0x84}  
    };

BYTE abyVT3226_InitTable[CB_VT3226_INIT_SEQ][3] = {
    {0x03, 0xFF, 0x80},
    {0x02, 0x82, 0xA1},
    {0x03, 0xC6, 0xA2},
    {0x01, 0x97, 0x93},
    {0x03, 0x66, 0x64},
    {0x00, 0x61, 0xA5},
    {0x01, 0x7B, 0xD6},
    {0x00, 0x80, 0x17},
    {0x03, 0xF8, 0x08},
    {0x00, 0x02, 0x39},   
    {0x02, 0x00, 0x2A}
    };

BYTE abyVT3226D0_InitTable[CB_VT3226_INIT_SEQ][3] = {
    {0x03, 0xFF, 0x80},
    {0x03, 0x02, 0x21}, 
    {0x03, 0xC6, 0xA2},
    {0x01, 0x97, 0x93},
    {0x03, 0x66, 0x64},
    {0x00, 0x71, 0xA5}, 
    {0x01, 0x15, 0xC6}, 
    {0x01, 0x2E, 0x07}, 
    {0x00, 0x58, 0x08}, 
    {0x00, 0x02, 0x79}, 
    {0x02, 0x01, 0xAA}  
    };


BYTE abyVT3226_ChannelTable0[CB_MAX_CHANNEL_24G][3] = {
    {0x01, 0x97, 0x83}, 
    {0x01, 0x97, 0x83}, 
    {0x01, 0x97, 0x93}, 
    {0x01, 0x97, 0x93}, 
    {0x01, 0x97, 0x93}, 
    {0x01, 0x97, 0x93}, 
    {0x01, 0x97, 0xA3}, 
    {0x01, 0x97, 0xA3}, 
    {0x01, 0x97, 0xA3}, 
    {0x01, 0x97, 0xA3}, 
    {0x01, 0x97, 0xB3}, 
    {0x01, 0x97, 0xB3}, 
    {0x01, 0x97, 0xB3}, 
    {0x03, 0x37, 0xC3}  
    };

BYTE abyVT3226_ChannelTable1[CB_MAX_CHANNEL_24G][3] = {
    {0x02, 0x66, 0x64}, 
    {0x03, 0x66, 0x64}, 
    {0x00, 0x66, 0x64}, 
    {0x01, 0x66, 0x64}, 
    {0x02, 0x66, 0x64}, 
    {0x03, 0x66, 0x64}, 
    {0x00, 0x66, 0x64}, 
    {0x01, 0x66, 0x64}, 
    {0x02, 0x66, 0x64}, 
    {0x03, 0x66, 0x64}, 
    {0x00, 0x66, 0x64}, 
    {0x01, 0x66, 0x64}, 
    {0x02, 0x66, 0x64}, 
    {0x00, 0xCC, 0xC4}  
    };


DWORD dwVT3226D0LoCurrentTable[CB_MAX_CHANNEL_24G] = {
    0x0135C600+(BY_VT3226_REG_LEN<<3)+IFREGCTL_REGW, 
    0x0135C600+(BY_VT3226_REG_LEN<<3)+IFREGCTL_REGW, 
    0x0235C600+(BY_VT3226_REG_LEN<<3)+IFREGCTL_REGW, 
    0x0235C600+(BY_VT3226_REG_LEN<<3)+IFREGCTL_REGW, 
    0x0235C600+(BY_VT3226_REG_LEN<<3)+IFREGCTL_REGW, 
    0x0335C600+(BY_VT3226_REG_LEN<<3)+IFREGCTL_REGW, 
    0x0335C600+(BY_VT3226_REG_LEN<<3)+IFREGCTL_REGW, 
    0x0335C600+(BY_VT3226_REG_LEN<<3)+IFREGCTL_REGW, 
    0x0335C600+(BY_VT3226_REG_LEN<<3)+IFREGCTL_REGW, 
    0x0335C600+(BY_VT3226_REG_LEN<<3)+IFREGCTL_REGW, 
    0x0335C600+(BY_VT3226_REG_LEN<<3)+IFREGCTL_REGW, 
    0x0335C600+(BY_VT3226_REG_LEN<<3)+IFREGCTL_REGW, 
    0x0335C600+(BY_VT3226_REG_LEN<<3)+IFREGCTL_REGW, 
    0x0135C600+(BY_VT3226_REG_LEN<<3)+IFREGCTL_REGW  
};


BYTE abyVT3342A0_InitTable[CB_VT3342_INIT_SEQ][3] = { 
    {0x03, 0xFF, 0x80}, 
    {0x02, 0x08, 0x81},
    {0x00, 0xC6, 0x02},
    {0x03, 0xC5, 0x13}, 
    {0x00, 0xEE, 0xE4}, 
    {0x00, 0x71, 0xA5},
    {0x01, 0x75, 0x46},
    {0x01, 0x40, 0x27},
    {0x01, 0x54, 0x08},
    {0x00, 0x01, 0x69},
    {0x02, 0x00, 0xAA},
    {0x00, 0x08, 0xCB},
    {0x01, 0x70, 0x0C}
    };

 
 

 
 
 

BYTE abyVT3342_ChannelTable0[CB_MAX_CHANNEL][3] = {
    {0x02, 0x05, 0x03}, 
    {0x01, 0x15, 0x03}, 
    {0x03, 0xC5, 0x03}, 
    {0x02, 0x65, 0x03}, 
    {0x01, 0x15, 0x13}, 
    {0x03, 0xC5, 0x13}, 
    {0x02, 0x05, 0x13}, 
    {0x01, 0x15, 0x13}, 
    {0x03, 0xC5, 0x13}, 
    {0x02, 0x65, 0x13}, 
    {0x01, 0x15, 0x23}, 
    {0x03, 0xC5, 0x23}, 
    {0x02, 0x05, 0x23}, 
    {0x00, 0xD5, 0x23}, 

    
    {0x01, 0x15, 0x13}, 
    {0x01, 0x15, 0x13}, 
    {0x01, 0x15, 0x13}, 
    {0x01, 0x15, 0x13}, 
    {0x01, 0x15, 0x13}, 
    {0x01, 0x15, 0x13}, 
    {0x01, 0x15, 0x13}, 
    {0x01, 0x15, 0x13}, 

    
    
    {0x01, 0x15, 0x13}, 
    {0x01, 0x15, 0x13}, 
    {0x01, 0x15, 0x13}, 
    {0x01, 0x15, 0x13}, 
    {0x01, 0x15, 0x13}, 
    {0x01, 0x15, 0x13}, 
    {0x01, 0x15, 0x13}, 
    {0x01, 0x55, 0x63}, 
    {0x01, 0x55, 0x63}, 
    {0x02, 0xA5, 0x63}, 
    {0x02, 0xA5, 0x63}, 
    {0x00, 0x05, 0x73}, 
    {0x00, 0x05, 0x73}, 
    {0x01, 0x55, 0x73}, 
    {0x02, 0xA5, 0x73}, 
    {0x00, 0x05, 0x83}, 
    {0x01, 0x55, 0x83}, 
    {0x02, 0xA5, 0x83}, 

    {0x02, 0xA5, 0x83}, 
    {0x02, 0xA5, 0x83}, 
    {0x02, 0xA5, 0x83}, 
    {0x02, 0xA5, 0x83}, 
    {0x02, 0xA5, 0x83}, 
    {0x02, 0xA5, 0x83}, 
    {0x02, 0xA5, 0x83}, 
    {0x02, 0xA5, 0x83}, 
    {0x02, 0xA5, 0x83}, 
    {0x02, 0xA5, 0x83}, 
    {0x02, 0xA5, 0x83}, 

    {0x00, 0x05, 0xF3}, 
    {0x01, 0x56, 0x03}, 
    {0x02, 0xA6, 0x03}, 
    {0x00, 0x06, 0x03}, 
    {0x00, 0x06, 0x03}  
    };

BYTE abyVT3342_ChannelTable1[CB_MAX_CHANNEL][3] = {
    {0x01, 0x99, 0x94}, 
    {0x02, 0x44, 0x44}, 
    {0x02, 0xEE, 0xE4}, 
    {0x03, 0x99, 0x94}, 
    {0x00, 0x44, 0x44}, 
    {0x00, 0xEE, 0xE4}, 
    {0x01, 0x99, 0x94}, 
    {0x02, 0x44, 0x44}, 
    {0x02, 0xEE, 0xE4}, 
    {0x03, 0x99, 0x94}, 
    {0x00, 0x44, 0x44}, 
    {0x00, 0xEE, 0xE4}, 
    {0x01, 0x99, 0x94}, 
    {0x03, 0x33, 0x34}, 

    
    {0x00, 0x44, 0x44}, 
    {0x00, 0x44, 0x44}, 
    {0x00, 0x44, 0x44}, 
    {0x00, 0x44, 0x44}, 
    {0x00, 0x44, 0x44}, 
    {0x00, 0x44, 0x44}, 
    {0x00, 0x44, 0x44}, 
    {0x00, 0x44, 0x44}, 

    
    
    {0x00, 0x44, 0x44}, 
    {0x00, 0x44, 0x44}, 
    {0x00, 0x44, 0x44}, 
    {0x00, 0x44, 0x44}, 
    {0x00, 0x44, 0x44}, 
    {0x00, 0x44, 0x44}, 
    {0x00, 0x44, 0x44}, 
    {0x01, 0x55, 0x54}, 
    {0x01, 0x55, 0x54}, 
    {0x02, 0xAA, 0xA4}, 
    {0x02, 0xAA, 0xA4}, 
    {0x00, 0x00, 0x04}, 
    {0x00, 0x00, 0x04}, 
    {0x01, 0x55, 0x54}, 
    {0x02, 0xAA, 0xA4}, 
    {0x00, 0x00, 0x04}, 
    {0x01, 0x55, 0x54}, 
    {0x02, 0xAA, 0xA4}, 
    {0x02, 0xAA, 0xA4}, 
    {0x02, 0xAA, 0xA4}, 
    {0x02, 0xAA, 0xA4}, 
    {0x02, 0xAA, 0xA4}, 
    {0x02, 0xAA, 0xA4}, 
    {0x02, 0xAA, 0xA4}, 
    {0x02, 0xAA, 0xA4}, 
    {0x02, 0xAA, 0xA4}, 
    {0x02, 0xAA, 0xA4}, 
    {0x02, 0xAA, 0xA4}, 
    {0x02, 0xAA, 0xA4}, 
    {0x03, 0x00, 0x04}, 
    {0x00, 0x55, 0x54}, 
    {0x01, 0xAA, 0xA4}, 
    {0x03, 0x00, 0x04}, 
    {0x03, 0x00, 0x04}  
    };



const DWORD dwAL2230PowerTable[AL2230_PWR_IDX_LEN] = {
    0x04040900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04041900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04042900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04043900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04044900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04045900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04046900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04047900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04048900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04049900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0404A900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0404B900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0404C900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0404D900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0404E900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0404F900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04050900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04051900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04052900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04053900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04054900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04055900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04056900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04057900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04058900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04059900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0405A900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0405B900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0405C900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0405D900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0405E900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0405F900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04060900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04061900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04062900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04063900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04064900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04065900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04066900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04067900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04068900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04069900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0406A900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0406B900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0406C900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0406D900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0406E900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0406F900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04070900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04071900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04072900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04073900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04074900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04075900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04076900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04077900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04078900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x04079900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0407A900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0407B900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0407C900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0407D900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0407E900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW,
    0x0407F900+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW
    };




const BYTE RFaby11aChannelIndex[200] = {
  
    00, 00, 00, 00, 00, 00, 23, 24, 25, 00,  
    26, 27, 00, 00, 00, 28, 00, 00, 00, 00,  
    00, 00, 00, 00, 00, 00, 00, 00, 00, 00,  
    00, 00, 00, 29, 00, 30, 00, 31, 00, 32,  
    00, 33, 00, 34, 00, 35, 00, 36, 00, 00,  
    00, 37, 00, 00, 00, 38, 00, 00, 00, 39,  
    00, 00, 00, 40, 00, 00, 00, 00, 00, 00,  
    00, 00, 00, 00, 00, 00, 00, 00, 00, 00,  
    00, 00, 00, 00, 00, 00, 00, 00, 00, 00,  
    00, 00, 00, 00, 00, 00, 00, 00, 00, 41,  

    00, 00, 00, 42, 00, 00, 00, 43, 00, 00,  
    00, 44, 00, 00, 00, 45, 00, 00, 00, 46,  
    00, 00, 00, 47, 00, 00, 00, 48, 00, 00,  
    00, 49, 00, 00, 00, 50, 00, 00, 00, 51,  
    00, 00, 00, 00, 00, 00, 00, 00, 52, 00,  
    00, 00, 53, 00, 00, 00, 54, 00, 00, 00,  
    55, 00, 00, 00, 56, 00, 00, 00, 00, 00,  
    00, 00, 00, 00, 00, 00, 00, 00, 00, 00,  
    00, 00, 15, 16, 17, 00, 18, 19, 20, 00,  
    00, 21, 00, 00, 00, 22, 00, 00, 00, 00   
};


BOOL IFRFbWriteEmbeded (PSDevice pDevice, DWORD dwData)
{
    BYTE        pbyData[4];

    pbyData[0] = (BYTE)dwData;
    pbyData[1] = (BYTE)(dwData>>8);
    pbyData[2] = (BYTE)(dwData>>16);
    pbyData[3] = (BYTE)(dwData>>24);
    CONTROLnsRequestOut(pDevice,
                    MESSAGE_TYPE_WRITE_IFRF,
                    0,
                    0,
                    4,
                    pbyData
                        );


    return TRUE;
}


BOOL RFbSetPower (
      PSDevice  pDevice,
      unsigned int      uRATE,
      unsigned int      uCH
    )
{
BOOL    bResult = TRUE;
BYTE    byPwr = pDevice->byCCKPwr;

    if (pDevice->dwDiagRefCount != 0) {
        return TRUE;
    }

    switch (uRATE) {
    case RATE_1M:
    case RATE_2M:
    case RATE_5M:
    case RATE_11M:
        byPwr = pDevice->abyCCKPwrTbl[uCH-1];
        break;
    case RATE_6M:
    case RATE_9M:
    case RATE_18M:
    case RATE_24M:
    case RATE_36M:
    case RATE_48M:
    case RATE_54M:
        if (uCH > CB_MAX_CHANNEL_24G) {
            byPwr = pDevice->abyOFDMAPwrTbl[uCH-15];
        } else {
            byPwr = pDevice->abyOFDMPwrTbl[uCH-1];
        }
        break;
    }

    bResult = RFbRawSetPower(pDevice, byPwr, uRATE);

    return bResult;
}


BOOL RFbRawSetPower (
      PSDevice  pDevice,
      BYTE      byPwr,
      unsigned int      uRATE
    )
{
BOOL        bResult = TRUE;

    if (pDevice->byCurPwr == byPwr)
        return TRUE;

    pDevice->byCurPwr = byPwr;

    switch (pDevice->byRFType) {

        case RF_AL2230 :
            if (pDevice->byCurPwr >= AL2230_PWR_IDX_LEN)
                return FALSE;
            bResult &= IFRFbWriteEmbeded(pDevice, dwAL2230PowerTable[pDevice->byCurPwr]);
            if (uRATE <= RATE_11M)
                bResult &= IFRFbWriteEmbeded(pDevice, 0x0001B400+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW);
            else
                bResult &= IFRFbWriteEmbeded(pDevice, 0x0005A400+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW);
            break;

        case RF_AL2230S :
            if (pDevice->byCurPwr >= AL2230_PWR_IDX_LEN)
                return FALSE;
            bResult &= IFRFbWriteEmbeded(pDevice, dwAL2230PowerTable[pDevice->byCurPwr]);
            if (uRATE <= RATE_11M) {
                bResult &= IFRFbWriteEmbeded(pDevice, 0x040C1400+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW);
                bResult &= IFRFbWriteEmbeded(pDevice, 0x00299B00+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW);
            }else {
                bResult &= IFRFbWriteEmbeded(pDevice, 0x0005A400+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW);
                bResult &= IFRFbWriteEmbeded(pDevice, 0x00099B00+(BY_AL2230_REG_LEN<<3)+IFREGCTL_REGW);
            }
            break;


        case RF_AIROHA7230:
            {
                DWORD       dwMax7230Pwr;

                if (uRATE <= RATE_11M) { 
                    bResult &= IFRFbWriteEmbeded(pDevice, 0x111BB900+(BY_AL7230_REG_LEN<<3)+IFREGCTL_REGW);
                }
                else {
                    bResult &= IFRFbWriteEmbeded(pDevice, 0x221BB900+(BY_AL7230_REG_LEN<<3)+IFREGCTL_REGW);
                }

                if (pDevice->byCurPwr > AL7230_PWR_IDX_LEN) return FALSE;

                
                dwMax7230Pwr = 0x080C0B00 | ( (pDevice->byCurPwr) << 12 ) |
                                 (BY_AL7230_REG_LEN << 3 )  | IFREGCTL_REGW;

                bResult &= IFRFbWriteEmbeded(pDevice, dwMax7230Pwr);
                break;
            }
            break;

        case RF_VT3226: 
        {
            DWORD       dwVT3226Pwr;

            if (pDevice->byCurPwr >= VT3226_PWR_IDX_LEN)
                return FALSE;
            dwVT3226Pwr = ((0x3F-pDevice->byCurPwr) << 20 ) | ( 0x17 << 8 )  |
                           (BY_VT3226_REG_LEN << 3 )  | IFREGCTL_REGW;
            bResult &= IFRFbWriteEmbeded(pDevice, dwVT3226Pwr);
            break;
        }

        case RF_VT3226D0: 
        {
            DWORD       dwVT3226Pwr;

            if (pDevice->byCurPwr >= VT3226_PWR_IDX_LEN)
                return FALSE;

            if (uRATE <= RATE_11M) {

                dwVT3226Pwr = ((0x3F-pDevice->byCurPwr) << 20 ) | ( 0xE07 << 8 )  |   
                               (BY_VT3226_REG_LEN << 3 )  | IFREGCTL_REGW;
                bResult &= IFRFbWriteEmbeded(pDevice, dwVT3226Pwr);

                bResult &= IFRFbWriteEmbeded(pDevice, 0x03C6A200+(BY_VT3226_REG_LEN<<3)+IFREGCTL_REGW);
                if (pDevice->sMgmtObj.eScanState != WMAC_NO_SCANNING) {
                    
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"@@@@ RFbRawSetPower> 11B mode uCurrChannel[%d]\n", pDevice->sMgmtObj.uScanChannel);
                    bResult &= IFRFbWriteEmbeded(pDevice, dwVT3226D0LoCurrentTable[pDevice->sMgmtObj.uScanChannel-1]); 
                } else {
                    DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"@@@@ RFbRawSetPower> 11B mode uCurrChannel[%d]\n", pDevice->sMgmtObj.uCurrChannel);
                    bResult &= IFRFbWriteEmbeded(pDevice, dwVT3226D0LoCurrentTable[pDevice->sMgmtObj.uCurrChannel-1]); 
                }

                bResult &= IFRFbWriteEmbeded(pDevice, 0x015C0800+(BY_VT3226_REG_LEN<<3)+IFREGCTL_REGW); 
            } else {
                DBG_PRT(MSG_LEVEL_DEBUG, KERN_INFO"@@@@ RFbRawSetPower> 11G mode\n");
                dwVT3226Pwr = ((0x3F-pDevice->byCurPwr) << 20 ) | ( 0x7 << 8 )  |   
                               (BY_VT3226_REG_LEN << 3 )  | IFREGCTL_REGW;
                bResult &= IFRFbWriteEmbeded(pDevice, dwVT3226Pwr);
                bResult &= IFRFbWriteEmbeded(pDevice, 0x00C6A200+(BY_VT3226_REG_LEN<<3)+IFREGCTL_REGW); 
                bResult &= IFRFbWriteEmbeded(pDevice, 0x016BC600+(BY_VT3226_REG_LEN<<3)+IFREGCTL_REGW); 
                bResult &= IFRFbWriteEmbeded(pDevice, 0x00900800+(BY_VT3226_REG_LEN<<3)+IFREGCTL_REGW); 
            }
            break;
        }

        
        case RF_VT3342A0:
        {
            DWORD       dwVT3342Pwr;

            if (pDevice->byCurPwr >= VT3342_PWR_IDX_LEN)
                return FALSE;

            dwVT3342Pwr =  ((0x3F-pDevice->byCurPwr) << 20 ) | ( 0x27 << 8 )  |
                            (BY_VT3342_REG_LEN << 3 )  | IFREGCTL_REGW;
            bResult &= IFRFbWriteEmbeded(pDevice, dwVT3342Pwr);
            break;
        }

        default :
            break;
    }
    return bResult;
}

void
RFvRSSITodBm (
      PSDevice pDevice,
      BYTE     byCurrRSSI,
    long *    pldBm
    )
{
    BYTE byIdx = (((byCurrRSSI & 0xC0) >> 6) & 0x03);
    signed long b = (byCurrRSSI & 0x3F);
    signed long a = 0;
    BYTE abyAIROHARF[4] = {0, 18, 0, 40};

    switch (pDevice->byRFType) {
        case RF_AL2230:
        case RF_AL2230S:
        case RF_AIROHA7230:
        case RF_VT3226: 
        case RF_VT3226D0:
        case RF_VT3342A0:   
            a = abyAIROHARF[byIdx];
            break;
        default:
            break;
    }

    *pldBm = -1 * (a + b * 2);
}



void
RFbRFTableDownload (
      PSDevice pDevice
    )
{
WORD    wLength1 = 0,wLength2 = 0 ,wLength3 = 0;
PBYTE   pbyAddr1 = NULL,pbyAddr2 = NULL,pbyAddr3 = NULL;
WORD    wLength,wValue;
BYTE    abyArray[256];

    switch ( pDevice->byRFType ) {
        case RF_AL2230:
        case RF_AL2230S:
            wLength1 = CB_AL2230_INIT_SEQ * 3;
            wLength2 = CB_MAX_CHANNEL_24G * 3;
            wLength3 = CB_MAX_CHANNEL_24G * 3;
            pbyAddr1 = &(abyAL2230InitTable[0][0]);
            pbyAddr2 = &(abyAL2230ChannelTable0[0][0]);
            pbyAddr3 = &(abyAL2230ChannelTable1[0][0]);
            break;
        case RF_AIROHA7230:
            wLength1 = CB_AL7230_INIT_SEQ * 3;
            wLength2 = CB_MAX_CHANNEL * 3;
            wLength3 = CB_MAX_CHANNEL * 3;
            pbyAddr1 = &(abyAL7230InitTable[0][0]);
            pbyAddr2 = &(abyAL7230ChannelTable0[0][0]);
            pbyAddr3 = &(abyAL7230ChannelTable1[0][0]);
            break;
        case RF_VT3226: 
            wLength1 = CB_VT3226_INIT_SEQ * 3;
            wLength2 = CB_MAX_CHANNEL_24G * 3;
            wLength3 = CB_MAX_CHANNEL_24G * 3;
            pbyAddr1 = &(abyVT3226_InitTable[0][0]);
            pbyAddr2 = &(abyVT3226_ChannelTable0[0][0]);
            pbyAddr3 = &(abyVT3226_ChannelTable1[0][0]);
            break;
        case RF_VT3226D0: 
            wLength1 = CB_VT3226_INIT_SEQ * 3;
            wLength2 = CB_MAX_CHANNEL_24G * 3;
            wLength3 = CB_MAX_CHANNEL_24G * 3;
            pbyAddr1 = &(abyVT3226D0_InitTable[0][0]);
            pbyAddr2 = &(abyVT3226_ChannelTable0[0][0]);
            pbyAddr3 = &(abyVT3226_ChannelTable1[0][0]);
            break;
        case RF_VT3342A0: 
            wLength1 = CB_VT3342_INIT_SEQ * 3;
            wLength2 = CB_MAX_CHANNEL * 3;
            wLength3 = CB_MAX_CHANNEL * 3;
            pbyAddr1 = &(abyVT3342A0_InitTable[0][0]);
            pbyAddr2 = &(abyVT3342_ChannelTable0[0][0]);
            pbyAddr3 = &(abyVT3342_ChannelTable1[0][0]);
            break;

    }
    

    memcpy(abyArray, pbyAddr1, wLength1);
    CONTROLnsRequestOut(pDevice,
                    MESSAGE_TYPE_WRITE,
                    0,
                    MESSAGE_REQUEST_RF_INIT,
                    wLength1,
                    abyArray
                    );
    
    wValue = 0;
    while ( wLength2 > 0 ) {

        if ( wLength2 >= 64 ) {
            wLength = 64;
        } else {
            wLength = wLength2;
        }
        memcpy(abyArray, pbyAddr2, wLength);
        CONTROLnsRequestOut(pDevice,
                        MESSAGE_TYPE_WRITE,
                        wValue,
                        MESSAGE_REQUEST_RF_CH0,
                        wLength,
                        abyArray);

        wLength2 -= wLength;
        wValue += wLength;
        pbyAddr2 += wLength;
    }
    
    wValue = 0;
    while ( wLength3 > 0 ) {

        if ( wLength3 >= 64 ) {
            wLength = 64;
        } else {
            wLength = wLength3;
        }
        memcpy(abyArray, pbyAddr3, wLength);
        CONTROLnsRequestOut(pDevice,
                        MESSAGE_TYPE_WRITE,
                        wValue,
                        MESSAGE_REQUEST_RF_CH1,
                        wLength,
                        abyArray);

        wLength3 -= wLength;
        wValue += wLength;
        pbyAddr3 += wLength;
    }

    
    if ( pDevice->byRFType == RF_AIROHA7230 ) {
        wLength1 = CB_AL7230_INIT_SEQ * 3;
        wLength2 = CB_MAX_CHANNEL * 3;
        pbyAddr1 = &(abyAL7230InitTableAMode[0][0]);
        pbyAddr2 = &(abyAL7230ChannelTable2[0][0]);
        memcpy(abyArray, pbyAddr1, wLength1);
        
        CONTROLnsRequestOut(pDevice,
                    MESSAGE_TYPE_WRITE,
                    0,
                    MESSAGE_REQUEST_RF_INIT2,
                    wLength1,
                    abyArray);

        
        wValue = 0;
        while ( wLength2 > 0 ) {

            if ( wLength2 >= 64 ) {
                wLength = 64;
            } else {
                wLength = wLength2;
            }
            memcpy(abyArray, pbyAddr2, wLength);
            CONTROLnsRequestOut(pDevice,
                            MESSAGE_TYPE_WRITE,
                            wValue,
                            MESSAGE_REQUEST_RF_CH2,
                            wLength,
                            abyArray);

            wLength2 -= wLength;
            wValue += wLength;
            pbyAddr2 += wLength;
        }
    }

}

BOOL s_bVT3226D0_11bLoCurrentAdjust(
      PSDevice    pDevice,
      BYTE        byChannel,
      BOOL        b11bMode)
{
    BOOL    bResult;

    bResult = TRUE;
    if( b11bMode )
        bResult &= IFRFbWriteEmbeded(pDevice, dwVT3226D0LoCurrentTable[byChannel-1]);
    else
        bResult &= IFRFbWriteEmbeded(pDevice, 0x016BC600+(BY_VT3226_REG_LEN<<3)+IFREGCTL_REGW); 

    return bResult;
}


