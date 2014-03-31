
/*
 * Copyright (c) 1996-1999 Distributed Processing Technology Corporation
 * All rights reserved.
 *
 * Redistribution and use in source form, with or without modification, are
 * permitted provided that redistributions of source code must retain the
 * above copyright notice, this list of conditions and the following disclaimer.
 *
 * This software is provided `as is' by Distributed Processing Technology and
 * any express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular purpose,
 * are disclaimed. In no event shall Distributed Processing Technology be
 * liable for any direct, indirect, incidental, special, exemplary or
 * consequential damages (including, but not limited to, procurement of
 * substitute goods or services; loss of use, data, or profits; or business
 * interruptions) however caused and on any theory of liability, whether in
 * contract, strict liability, or tort (including negligence or otherwise)
 * arising in any way out of the use of this driver software, even if advised
 * of the possibility of such damage.
 *
 */

#ifndef         __SYS_INFO_H
#define         __SYS_INFO_H

/*File - SYS_INFO.H
 ****************************************************************************
 *
 *Description:
 *
 *      This file contains structure definitions for the OS dependent
 *layer system information buffers.
 *
 *Copyright Distributed Processing Technology, Corp.
 *        140 Candace Dr.
 *        Maitland, Fl. 32751   USA
 *        Phone: (407) 830-5522  Fax: (407) 260-5366
 *        All Rights Reserved
 *
 *Author:       Don Kemper
 *Date:         5/10/94
 *
 *Editors:
 *
 *Remarks:
 *
 *
 *****************************************************************************/



#include        "osd_util.h"

#ifndef NO_PACK
#if defined (_DPT_AIX)
#pragma options align=packed
#else
#pragma pack(1)
#endif  
#endif  



#ifdef  __cplusplus
   struct driveParam_S {
#else
   typedef struct  {
#endif

   uSHORT       cylinders;      
   uCHAR        heads;          
   uCHAR        sectors;        

#ifdef  __cplusplus

#ifdef DPT_PORTABLE
	uSHORT		netInsert(dptBuffer_S *buffer);
	uSHORT		netExtract(dptBuffer_S *buffer);
#endif 

   };
#else
   } driveParam_S;
#endif



#define SI_CMOS_Valid           0x0001
#define SI_NumDrivesValid       0x0002
#define SI_ProcessorValid       0x0004
#define SI_MemorySizeValid      0x0008
#define SI_DriveParamsValid     0x0010
#define SI_SmartROMverValid     0x0020
#define SI_OSversionValid       0x0040
#define SI_OSspecificValid      0x0080  
#define SI_BusTypeValid         0x0100

#define SI_ALL_VALID            0x0FFF  
#define SI_NO_SmartROM          0x8000

#define SI_ISA_BUS      0x00
#define SI_MCA_BUS      0x01
#define SI_EISA_BUS     0x02
#define SI_PCI_BUS      0x04

#ifdef  __cplusplus
   struct sysInfo_S {
#else
   typedef struct  {
#endif

   uCHAR        drive0CMOS;             
   uCHAR        drive1CMOS;             
   uCHAR        numDrives;              
   uCHAR        processorFamily;        
   uCHAR        processorType;          
   uCHAR        smartROMMajorVersion;
   uCHAR        smartROMMinorVersion;   
   uCHAR        smartROMRevision;
   uSHORT       flags;                  
   uSHORT       conventionalMemSize;    
   uINT         extendedMemSize;        
   uINT         osType;                 
   uCHAR        osMajorVersion;
   uCHAR        osMinorVersion;         
   uCHAR        osRevision;
#ifdef _SINIX_ADDON
   uCHAR        busType;                
   uSHORT       osSubRevision;
   uCHAR        pad[2];                 
#else
   uCHAR        osSubRevision;
   uCHAR        busType;                
   uCHAR        pad[3];                 
#endif
   driveParam_S drives[16];             

#ifdef  __cplusplus

#ifdef DPT_PORTABLE
	uSHORT		netInsert(dptBuffer_S *buffer);
	uSHORT		netExtract(dptBuffer_S *buffer);
#endif 

   };
#else
   } sysInfo_S;
#endif



#define DI_DOS_HIGH             0x01    
#define DI_DPMI_VALID           0x02    

#ifdef  __cplusplus
   struct DOS_Info_S {
#else
   typedef struct {
#endif

   uCHAR        flags;          
   uSHORT       driverLocation; 
   uSHORT       DOS_version;
   uSHORT       DPMI_version;

#ifdef  __cplusplus

#ifdef DPT_PORTABLE
	uSHORT		netInsert(dptBuffer_S *buffer);
	uSHORT		netExtract(dptBuffer_S *buffer);
#endif 

   };
#else
   } DOS_Info_S;
#endif



#ifdef  __cplusplus
   struct Netware_Info_S {
#else
   typedef struct {
#endif

   uCHAR        driverName[13];         
   uCHAR        serverName[48];
   uCHAR        netwareVersion;         
   uCHAR        netwareSubVersion;
   uCHAR        netwareRevision;
   uSHORT       maxConnections;         
   uSHORT       connectionsInUse;
   uSHORT       maxVolumes;
   uCHAR        unused;
   uCHAR        SFTlevel;
   uCHAR        TTSlevel;

   uCHAR        clibMajorVersion;       
   uCHAR        clibMinorVersion;
   uCHAR        clibRevision;

#ifdef  __cplusplus

#ifdef DPT_PORTABLE
	uSHORT		netInsert(dptBuffer_S *buffer);
	uSHORT		netExtract(dptBuffer_S *buffer);
#endif 

   };
#else
   } Netware_Info_S;
#endif



#ifdef  __cplusplus
   struct OS2_Info_S {
#else
   typedef struct {
#endif

   uCHAR        something;

#ifdef  __cplusplus

#ifdef DPT_PORTABLE
	uSHORT		netInsert(dptBuffer_S *buffer);
	uSHORT		netExtract(dptBuffer_S *buffer);
#endif 

   };
#else
   } OS2_Info_S;
#endif



#ifdef  __cplusplus
   struct WinNT_Info_S {
#else
   typedef struct {
#endif

   uCHAR        something;

#ifdef  __cplusplus

#ifdef DPT_PORTABLE
	uSHORT		netInsert(dptBuffer_S *buffer);
	uSHORT		netExtract(dptBuffer_S *buffer);
#endif 

   };
#else
   } WinNT_Info_S;
#endif



#ifdef  __cplusplus
   struct SCO_Info_S {
#else
   typedef struct {
#endif

   uCHAR        something;

#ifdef  __cplusplus

#ifdef DPT_PORTABLE
	uSHORT		netInsert(dptBuffer_S *buffer);
	uSHORT		netExtract(dptBuffer_S *buffer);
#endif 

   };
#else
   } SCO_Info_S;
#endif



#ifdef  __cplusplus
   struct USL_Info_S {
#else
   typedef struct {
#endif

   uCHAR        something;

#ifdef  __cplusplus

#ifdef DPT_PORTABLE
	uSHORT		netInsert(dptBuffer_S *buffer);
	uSHORT		netExtract(dptBuffer_S *buffer);
#endif 

   };
#else
   } USL_Info_S;
#endif


  
#ifndef NO_UNPACK
#if defined (_DPT_AIX)
#pragma options align=reset
#elif defined (UNPACK_FOUR)
#pragma pack(4)
#else
#pragma pack()
#endif  
#endif  

#endif  

