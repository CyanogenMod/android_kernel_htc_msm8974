
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

#ifndef         __OSD_UTIL_H
#define         __OSD_UTIL_H

/*File - OSD_UTIL.H
 ****************************************************************************
 *
 *Description:
 *
 *      This file contains defines and function prototypes that are
 *operating system dependent.  The resources defined in this file
 *are not specific to any particular application.
 *
 *Copyright Distributed Processing Technology, Corp.
 *        140 Candace Dr.
 *        Maitland, Fl. 32751   USA
 *        Phone: (407) 830-5522  Fax: (407) 260-5366
 *        All Rights Reserved
 *
 *Author:       Doug Anderson
 *Date:         1/7/94
 *
 *Editors:
 *
 *Remarks:
 *
 *
 *****************************************************************************/







#if (defined(KERNEL) && (defined(__FreeBSD__) || defined(__bsdi__)))
# include        "i386/isa/dpt_osd_defs.h"
#else
# include        "osd_defs.h"
#endif

#ifndef DPT_UNALIGNED
   #define      DPT_UNALIGNED
#endif

#ifndef DPT_EXPORT
   #define      DPT_EXPORT
#endif

#ifndef DPT_IMPORT
   #define      DPT_IMPORT
#endif

#ifndef DPT_RUNTIME_IMPORT
   #define      DPT_RUNTIME_IMPORT  DPT_IMPORT
#endif


#if defined (_DPT_MSDOS) || defined (_DPT_WIN_3X)
   #define      _DPT_16_BIT
#else
   #define      _DPT_32_BIT
#endif

#if defined (_DPT_SCO) || defined (_DPT_UNIXWARE) || defined (_DPT_SOLARIS) || defined (_DPT_AIX) || defined (SNI_MIPS) || defined (_DPT_BSDI) || defined (_DPT_FREE_BSD) || defined(_DPT_LINUX)
   #define      _DPT_UNIX
#endif

#if defined (_DPT_WIN_3x) || defined (_DPT_WIN_4X) || defined (_DPT_WIN_NT) \
	    || defined (_DPT_OS2)
   #define      _DPT_DLL_SUPPORT
#endif

#if !defined (_DPT_MSDOS) && !defined (_DPT_WIN_3X) && !defined (_DPT_NETWARE)
   #define      _DPT_PREEMPTIVE
#endif

#if !defined (_DPT_MSDOS) && !defined (_DPT_WIN_3X)
   #define      _DPT_MULTI_THREADED
#endif

#if !defined (_DPT_MSDOS)
   #define      _DPT_MULTI_TASKING
#endif

  
  
  
#if defined (SNI_MIPS) || defined (_DPT_SOLARIS)
   #if defined (_DPT_BIG_ENDIAN)
	#if !defined (_DPT_STRICT_ALIGN)
            #define _DPT_STRICT_ALIGN
	#endif
   #endif
#endif

  
#ifdef  __cplusplus
   #define      _DPT_CPP
#else
   #define      _DPT_C
#endif

#if !defined (DPTSQO)
   #if defined (_DPT_SOLARIS)
      #define DPTSQO
      #define DPTSQC
   #else
      #define DPTSQO {
      #define DPTSQC }
   #endif  
#endif  



#if defined (_DPT_MSDOS) || defined (_DPT_SCO)
   #define BYTE unsigned char
   #define WORD unsigned short
#endif

#ifndef _DPT_TYPEDEFS
   #define _DPT_TYPEDEFS
   typedef unsigned char   uCHAR;
   typedef unsigned short  uSHORT;
   typedef unsigned int    uINT;
   typedef unsigned long   uLONG;

   typedef union {
	 uCHAR        u8[4];
	 uSHORT       u16[2];
	 uLONG        u32;
   } access_U;
#endif

#if !defined (NULL)
   #define      NULL    0
#endif



#ifdef  __cplusplus
   extern "C" {         
#endif


  
#if (!defined(osdSwap2))
 uSHORT       osdSwap2(DPT_UNALIGNED uSHORT *);
#endif  

  
#if (!defined(osdSwap3))
 uLONG        osdSwap3(DPT_UNALIGNED uLONG *);
#endif  


#ifdef  _DPT_NETWARE
   #include "novpass.h" 
	
   #ifdef __cplusplus
	 inline uLONG osdSwap4(uLONG *inLong) {
	 return *inLong = DPT_Bswapl(*inLong);
	 }
   #else
	 #define osdSwap4(inLong)       DPT_Bswapl(inLong)
   #endif  
#else
	
# if (!defined(osdSwap4))
   uLONG        osdSwap4(DPT_UNALIGNED uLONG *);
# endif  


   uSHORT       trueSwap2(DPT_UNALIGNED uSHORT *);
   uLONG        trueSwap4(DPT_UNALIGNED uLONG *);

#endif  


uLONG	netSwap4(uLONG val);

#if defined (_DPT_BIG_ENDIAN)


#ifndef NET_SWAP_2
#define NET_SWAP_2(x) (((x) >> 8) | ((x) << 8))
#endif  

#ifndef NET_SWAP_4
#define NET_SWAP_4(x) netSwap4((x))
#endif  

#else


#ifndef NET_SWAP_2
#define NET_SWAP_2(x) (x)
#endif  

#ifndef NET_SWAP_4
#define NET_SWAP_4(x) (x)
#endif  

#endif  




  
DLL_HANDLE_T    osdLoadModule(uCHAR *);
  
uSHORT          osdUnloadModule(DLL_HANDLE_T);
  
void *          osdGetFnAddr(DLL_HANDLE_T,uCHAR *);


  
SEMAPHORE_T     osdCreateNamedSemaphore(char *);
  
SEMAPHORE_T     osdCreateSemaphore(void);
	
SEMAPHORE_T              osdCreateEventSemaphore(void);
	
SEMAPHORE_T             osdCreateNamedEventSemaphore(char *);

  
uSHORT          osdDestroySemaphore(SEMAPHORE_T);
  
uLONG           osdRequestSemaphore(SEMAPHORE_T,uLONG);
  
uSHORT          osdReleaseSemaphore(SEMAPHORE_T);
	
uLONG                            osdWaitForEventSemaphore(SEMAPHORE_T, uLONG);
	
uLONG                            osdSignalEventSemaphore(SEMAPHORE_T);
	
uLONG                            osdResetEventSemaphore(SEMAPHORE_T);


  
  
void            osdSwitchThreads(void);

  
uLONG   osdStartThread(void *,void *);

uLONG osdGetThreadID(void);

void osdWakeThread(uLONG);

void osdSleep(uLONG);

#define DPT_THREAD_PRIORITY_LOWEST 0x00
#define DPT_THREAD_PRIORITY_NORMAL 0x01
#define DPT_THREAD_PRIORITY_HIGHEST 0x02

uCHAR osdSetThreadPriority(uLONG tid, uCHAR priority);

#ifdef __cplusplus
   }    
#endif

#endif  
