/***************************************************************************
                          dpti_ioctl.h  -  description
                             -------------------
    begin                : Thu Sep 7 2000
    copyright            : (C) 2001 by Adaptec

    See Documentation/scsi/dpti.txt for history, notes, license info
    and credits
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#ifndef _dpti_ioctl_h
#define _dpti_ioctl_h


#ifndef _IOWR
# define _IOWR(x,y,z)	(((x)<<8)|y)
#endif
#ifndef _IOW
# define _IOW(x,y,z)	(((x)<<8)|y)
#endif
#ifndef _IOR
# define _IOR(x,y,z)	(((x)<<8)|y)
#endif
#ifndef _IO
# define _IO(x,y)	(((x)<<8)|y)
#endif
#define EATAUSRCMD      _IOWR('D',65,EATA_CP)
#define DPT_DEBUG       _IOW('D',66,int)
#define DPT_SIGNATURE   _IOR('D',67,dpt_sig_S)
#if defined __bsdi__
#define DPT_SIGNATURE_PACKED   _IOR('D',67,dpt_sig_S_Packed)
#endif
#define DPT_NUMCTRLS    _IOR('D',68,int)
#define DPT_CTRLINFO    _IOR('D',69,CtrlInfo)
#define DPT_STATINFO    _IO('D',70)
#define DPT_CLRSTAT     _IO('D',71)
#define DPT_SYSINFO     _IOR('D',72,sysInfo_S)
#define DPT_TIMEOUT     _IO('D',73)
#define DPT_CONFIG      _IO('D',74)
#define DPT_BLINKLED    _IOR('D',75,int)
#define DPT_STATS_INFO        _IOR('D',80,STATS_DATA)
#define DPT_STATS_CLEAR       _IO('D',81)
#define DPT_PERF_INFO        _IOR('D',82,dpt_perf_t)
#define I2OUSRCMD	_IO('D',76)
#define I2ORESCANCMD	_IO('D',77)
#define I2ORESETCMD	_IO('D',78)
#define DPT_TARGET_BUSY	_IOR('D',79, TARGET_BUSY_T)


  

typedef struct {
	uCHAR    state;            
	uCHAR    id;               
	int      vect;             
	int      base;             
	int      njobs;            
	int      qdepth;           
	int      wakebase;         
	uINT     SGsize;           
	unsigned heads;            
	unsigned sectors;          
	uCHAR    do_drive32;       
	uCHAR    BusQuiet;         
	char     idPAL[4];         
	uCHAR    primary;          
	uCHAR    eataVersion;      
	uINT     cpLength;         
	uINT     spLength;         
	uCHAR    drqNum;           
	uCHAR    flag1;            
	uCHAR    flag2;            
} CtrlInfo;

typedef struct {
	uSHORT length;		
	uSHORT drvrHBAnum;	
	uINT baseAddr;		
	uSHORT blinkState;	
	uCHAR pciBusNum;	
	uCHAR pciDeviceNum;	
	uSHORT hbaFlags;	
	uSHORT Interrupt;	
#   if (defined(_DPT_ARC))
	uINT baseLength;
	ADAPTER_OBJECT *AdapterObject;
	LARGE_INTEGER DmaLogicalAddress;
	PVOID DmaVirtualAddress;
	LARGE_INTEGER ReplyLogicalAddress;
	PVOID ReplyVirtualAddress;
#   else
	uINT reserved1;		
	uINT reserved2;		
	uINT reserved3;		
#   endif
} drvrHBAinfo_S;

typedef struct TARGET_BUSY
{
  uLONG channel;
  uLONG id;
  uLONG lun;
  uLONG isBusy;
} TARGET_BUSY_T;

#endif

