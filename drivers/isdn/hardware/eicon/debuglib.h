
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
#if !defined(__DEBUGLIB_H__)
#define __DEBUGLIB_H__
#include <stdarg.h>
#define DL_LOG  0x00000001 
#define DL_FTL  0x00000002 
#define DL_ERR  0x00000004 
#define DL_TRC  0x00000008 
#define DL_XLOG  0x00000010 
#define DL_MXLOG 0x00000020 
#define DL_FTL_MXLOG 0x00000021 
#define DL_EVL  0x00000080 
#define DL_COMPAT (DL_MXLOG | DL_XLOG)
#define DL_PRIOR_MASK (DL_EVL | DL_COMPAT | DL_TRC | DL_ERR | DL_FTL | DL_LOG)
#define DLI_LOG  0x0100
#define DLI_FTL  0x0200
#define DLI_ERR  0x0300
#define DLI_TRC  0x0400
#define DLI_XLOG 0x0500
#define DLI_MXLOG 0x0600
#define DLI_FTL_MXLOG 0x0600
#define DLI_EVL  0x0800
#define DL_REG  0x00000100 
#define DL_MEM  0x00000200 
#define DL_SPL  0x00000400 
#define DL_IRP  0x00000800 
#define DL_TIM  0x00001000 
#define DL_BLK  0x00002000 
#define DL_OS_MASK (DL_BLK | DL_TIM | DL_IRP | DL_SPL | DL_MEM | DL_REG)
#define DLI_REG  0x0900
#define DLI_MEM  0x0A00
#define DLI_SPL  0x0B00
#define DLI_IRP  0x0C00
#define DLI_TIM  0x0D00
#define DLI_BLK  0x0E00
#define DL_TAPI  0x00010000 
#define DL_NDIS  0x00020000 
#define DL_CONN  0x00040000 
#define DL_STAT  0x00080000 
#define DL_SEND  0x00100000 
#define DL_RECV  0x00200000 
#define DL_DATA  (DL_SEND | DL_RECV)
#define DL_ISDN_MASK (DL_DATA | DL_STAT | DL_CONN | DL_NDIS | DL_TAPI)
#define DLI_TAPI 0x1100
#define DLI_NDIS 0x1200
#define DLI_CONN 0x1300
#define DLI_STAT 0x1400
#define DLI_SEND 0x1500
#define DLI_RECV 0x1600
#define DL_PRV0  0x01000000
#define DL_PRV1  0x02000000
#define DL_PRV2  0x04000000
#define DL_PRV3  0x08000000
#define DL_PRIV_MASK (DL_PRV0 | DL_PRV1 | DL_PRV2 | DL_PRV3)
#define DLI_PRV0 0x1900
#define DLI_PRV1 0x1A00
#define DLI_PRV2 0x1B00
#define DLI_PRV3 0x1C00
#define DT_INDEX(x)  ((x) & 0x000F)
#define DL_INDEX(x)  ((((x) >> 8) & 0x00FF) - 1)
#define DLI_NAME(x)  ((x) & 0xFF00)
#define DL_TO_KERNEL    0x40000000

#ifdef DIVA_NO_DEBUGLIB
#define myDbgPrint_LOG(x...) do { } while (0);
#define myDbgPrint_FTL(x...) do { } while (0);
#define myDbgPrint_ERR(x...) do { } while (0);
#define myDbgPrint_TRC(x...) do { } while (0);
#define myDbgPrint_MXLOG(x...) do { } while (0);
#define myDbgPrint_EVL(x...) do { } while (0);
#define myDbgPrint_REG(x...) do { } while (0);
#define myDbgPrint_MEM(x...) do { } while (0);
#define myDbgPrint_SPL(x...) do { } while (0);
#define myDbgPrint_IRP(x...) do { } while (0);
#define myDbgPrint_TIM(x...) do { } while (0);
#define myDbgPrint_BLK(x...) do { } while (0);
#define myDbgPrint_TAPI(x...) do { } while (0);
#define myDbgPrint_NDIS(x...) do { } while (0);
#define myDbgPrint_CONN(x...) do { } while (0);
#define myDbgPrint_STAT(x...) do { } while (0);
#define myDbgPrint_SEND(x...) do { } while (0);
#define myDbgPrint_RECV(x...) do { } while (0);
#define myDbgPrint_PRV0(x...) do { } while (0);
#define myDbgPrint_PRV1(x...) do { } while (0);
#define myDbgPrint_PRV2(x...) do { } while (0);
#define myDbgPrint_PRV3(x...) do { } while (0);
#define DBG_TEST(func, args) do { } while (0);
#define DBG_EVL_ID(args) do { } while (0);

#else 
#define DBG_DECL(func) extern void  myDbgPrint_##func(char *, ...);
DBG_DECL(LOG)
DBG_DECL(FTL)
DBG_DECL(ERR)
DBG_DECL(TRC)
DBG_DECL(MXLOG)
DBG_DECL(FTL_MXLOG)
extern void  myDbgPrint_EVL(long, ...);
DBG_DECL(REG)
DBG_DECL(MEM)
DBG_DECL(SPL)
DBG_DECL(IRP)
DBG_DECL(TIM)
DBG_DECL(BLK)
DBG_DECL(TAPI)
DBG_DECL(NDIS)
DBG_DECL(CONN)
DBG_DECL(STAT)
DBG_DECL(SEND)
DBG_DECL(RECV)
DBG_DECL(PRV0)
DBG_DECL(PRV1)
DBG_DECL(PRV2)
DBG_DECL(PRV3)
#ifdef _KERNEL_DBG_PRINT_
#define DBG_TEST(func, args)						\
	{ if ((myDriverDebugHandle.dbgMask) & (unsigned long)DL_##func) \
		{							\
			if ((myDriverDebugHandle.dbgMask) & DL_TO_KERNEL) \
			{ DbgPrint args; DbgPrint("\r\n"); }		\
			myDbgPrint_##func args;			\
		} }
#else
#define DBG_TEST(func, args)						\
	{ if ((myDriverDebugHandle.dbgMask) & (unsigned long)DL_##func) \
		{ myDbgPrint_##func args;				\
		} }
#endif
#define DBG_EVL_ID(args)						\
	{ if ((myDriverDebugHandle.dbgMask) & (unsigned long)DL_EVL)	\
		{ myDbgPrint_EVL args;					\
		} }

#endif 

#define DBG_LOG(args)  DBG_TEST(LOG, args)
#define DBG_FTL(args)  DBG_TEST(FTL, args)
#define DBG_ERR(args)  DBG_TEST(ERR, args)
#define DBG_TRC(args)  DBG_TEST(TRC, args)
#define DBG_MXLOG(args)  DBG_TEST(MXLOG, args)
#define DBG_FTL_MXLOG(args) DBG_TEST(FTL_MXLOG, args)
#define DBG_EVL(args)  DBG_EVL_ID(args)
#define DBG_REG(args)  DBG_TEST(REG, args)
#define DBG_MEM(args)  DBG_TEST(MEM, args)
#define DBG_SPL(args)  DBG_TEST(SPL, args)
#define DBG_IRP(args)  DBG_TEST(IRP, args)
#define DBG_TIM(args)  DBG_TEST(TIM, args)
#define DBG_BLK(args)  DBG_TEST(BLK, args)
#define DBG_TAPI(args)  DBG_TEST(TAPI, args)
#define DBG_NDIS(args)  DBG_TEST(NDIS, args)
#define DBG_CONN(args)  DBG_TEST(CONN, args)
#define DBG_STAT(args)  DBG_TEST(STAT, args)
#define DBG_SEND(args)  DBG_TEST(SEND, args)
#define DBG_RECV(args)  DBG_TEST(RECV, args)
#define DBG_PRV0(args)  DBG_TEST(PRV0, args)
#define DBG_PRV1(args)  DBG_TEST(PRV1, args)
#define DBG_PRV2(args)  DBG_TEST(PRV2, args)
#define DBG_PRV3(args)  DBG_TEST(PRV3, args)
#ifdef DIVA_NO_DEBUGLIB
#define DbgRegister(name, tag, mask) do { } while (0)
#define DbgDeregister() do { } while (0)
#define DbgSetLevel(mask) do { } while (0)
#else
extern DIVA_DI_PRINTF dprintf;
extern int  DbgRegister(char *drvName, char *drvTag, unsigned long dbgMask);
extern void DbgDeregister(void);
extern void DbgSetLevel(unsigned long dbgMask);
#endif
typedef struct _DbgHandle_ *pDbgHandle;
typedef void (*DbgEnd)(pDbgHandle);
typedef void (*DbgLog)(unsigned short, int, char *, va_list);
typedef void (*DbgOld)(unsigned short, char *, va_list);
typedef void (*DbgEv)(unsigned short, unsigned long, va_list);
typedef void (*DbgIrq)(unsigned short, int, char *, va_list);
typedef struct _DbgHandle_
{ char    Registered; 
#define DBG_HANDLE_REG_NEW 0x01  
#define DBG_HANDLE_REG_OLD 0x7f  
	char    Version;  
#define DBG_HANDLE_VERSION 1   
#define DBG_HANDLE_VER_EXT  2           
	short               id;   
	struct _DbgHandle_ *next;   
	struct  {
		unsigned long LowPart;
		long          HighPart;
	}     regTime;  
	void               *pIrp;   
	unsigned long       dbgMask;  
	char                drvName[128]; 
	char                drvTag[64]; 
	DbgEnd              dbg_end;  
	DbgLog              dbg_prt;  
	DbgOld              dbg_old;  
	DbgEv       dbg_ev;  
	DbgIrq    dbg_irq;  
	void      *pReserved3;
} _DbgHandle_;
extern _DbgHandle_ myDriverDebugHandle;
typedef struct _OldDbgHandle_
{ struct _OldDbgHandle_ *next;
	void                *pIrp;
	long    regTime[2];
	unsigned long       dbgMask;
	short               id;
	char                drvName[78];
	DbgEnd              dbg_end;
	DbgLog              dbg_prt;
} _OldDbgHandle_;
#define DBG_EXT_TYPE_CARD_TRACE     0x00000001
typedef struct
{
	unsigned long ExtendedType;
	union
	{
		
		struct
		{
			void (*MaskChangedNotify)(void *pContext);
			unsigned long ModuleTxtMask;
			unsigned long DebugLevel;
			unsigned long B_ChannelMask;
			unsigned long LogBufferSize;
		} CardTrace;
	} Data;
} _DbgExtendedInfo_;
#ifndef DIVA_NO_DEBUGLIB
#define XDI_USE_XLOG 1
void xdi_dbg_xlog(char *x, ...);
#endif 
#endif 
