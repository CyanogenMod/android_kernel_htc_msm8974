// Copyright (C) 2002 Flarion Technologies, All rights reserved.
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option) any
// or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// License along with this program; if not, write to the
#ifndef _FT1000IOCTLH_
#define _FT1000IOCTLH_

typedef struct _IOCTL_GET_VER
{
    unsigned long drv_ver;
} __attribute__ ((packed)) IOCTL_GET_VER, *PIOCTL_GET_VER;

typedef struct _IOCTL_GET_DSP_STAT
{
    unsigned char DspVer[DSPVERSZ];        
    unsigned char HwSerNum[HWSERNUMSZ];    
    unsigned char Sku[SKUSZ];              
    unsigned char eui64[EUISZ];            
    unsigned short ConStat;                
                                
                                
                                
                                
                                
                                
    unsigned short LedStat;                
                                
                                
                                
                                
                                
                                
                                
                                
                                
                                
                                
                                
                                
                                
    unsigned long nTxPkts;                
    unsigned long nRxPkts;                
    unsigned long nTxBytes;               
    unsigned long nRxBytes;               
    unsigned long ConTm;                  
    unsigned char CalVer[CALVERSZ];       
    unsigned char CalDate[CALDATESZ];     
} __attribute__ ((packed)) IOCTL_GET_DSP_STAT, *PIOCTL_GET_DSP_STAT;

typedef struct _IOCTL_DPRAM_BLK
{
    unsigned short total_len;
	struct pseudo_hdr pseudohdr;
    unsigned char buffer[1780];
} __attribute__ ((packed)) IOCTL_DPRAM_BLK, *PIOCTL_DPRAM_BLK;

typedef struct _IOCTL_DPRAM_COMMAND
{
    unsigned short extra;
    IOCTL_DPRAM_BLK dpram_blk;
} __attribute__ ((packed)) IOCTL_DPRAM_COMMAND, *PIOCTL_DPRAM_COMMAND;

#define FT1000_MAGIC_CODE      'F'

#define IOCTL_REGISTER_CMD					0
#define IOCTL_SET_DPRAM_CMD					3
#define IOCTL_GET_DPRAM_CMD					4
#define IOCTL_GET_DSP_STAT_CMD      6
#define IOCTL_GET_VER_CMD           7
#define IOCTL_CONNECT               10
#define IOCTL_DISCONNECT            11

#define IOCTL_FT1000_GET_DSP_STAT _IOR (FT1000_MAGIC_CODE, IOCTL_GET_DSP_STAT_CMD, sizeof(IOCTL_GET_DSP_STAT) )
#define IOCTL_FT1000_GET_VER _IOR (FT1000_MAGIC_CODE, IOCTL_GET_VER_CMD, sizeof(IOCTL_GET_VER) )
#define IOCTL_FT1000_CONNECT _IOW (FT1000_MAGIC_CODE, IOCTL_CONNECT, 0 )
#define IOCTL_FT1000_DISCONNECT _IOW (FT1000_MAGIC_CODE, IOCTL_DISCONNECT, 0 )
#define IOCTL_FT1000_SET_DPRAM _IOW (FT1000_MAGIC_CODE, IOCTL_SET_DPRAM_CMD, sizeof(IOCTL_DPRAM_BLK) )
#define IOCTL_FT1000_GET_DPRAM _IOR (FT1000_MAGIC_CODE, IOCTL_GET_DPRAM_CMD, sizeof(IOCTL_DPRAM_BLK) )
#define IOCTL_FT1000_REGISTER  _IOW (FT1000_MAGIC_CODE, IOCTL_REGISTER_CMD, sizeof(unsigned short *) )
#endif 

