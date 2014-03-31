
#ifndef HCFCFG_H
#define HCFCFG_H 1

/*************************************************************************************************************
*
* FILE	 : hcfcfg.tpl // hcfcfg.h
*
* DATE   : $Date: 2004/08/05 11:47:10 $   $Revision: 1.6 $
* Original: 2004/04/08 15:18:16    Revision: 1.40      Tag: t20040408_01
* Original: 2004/04/01 15:32:55    Revision: 1.38      Tag: t7_20040401_01
* Original: 2004/03/10 15:39:28    Revision: 1.34      Tag: t20040310_01
* Original: 2004/03/03 14:10:12    Revision: 1.32      Tag: t20040304_01
* Original: 2004/03/02 09:27:12    Revision: 1.30      Tag: t20040302_03
* Original: 2004/02/24 13:00:28    Revision: 1.25      Tag: t20040224_01
* Original: 2004/02/18 17:13:57    Revision: 1.23      Tag: t20040219_01
*
* AUTHOR : Nico Valster
*
* DESC   : HCF Customization Macros
* hcfcfg.tpl list all #defines which must be specified to:
*   adjust the HCF functions defined in HCF.C to the characteristics of a specific environment
*		o maximum sizes for messages
*		o Endianess
*	Compiler specific macros
*		o port I/O macros
*		o type definitions
*
* By copying HCFCFG.TPL to HCFCFG.H and -if needed- modifying the #defines the WCI functionality can be
* tailored
*
* Supported environments:
* WVLAN_41	Miniport								NDIS 3.1
* WVLAN_42	Packet									Microsoft Visual C 1.5
* WVLAN_43	16 bits DOS ODI							Microsoft Visual C 1.5
* WVLAN_44	32 bits ODI (__NETWARE_386__)			WATCOM
* WVLAN_45	MAC_OS									MPW?, Symantec?
* WVLAN_46	Windows CE (_WIN32_WCE)					Microsoft ?
* WVLAN_47	LINUX  (__LINUX__)						GCC, discarded, based on GPL'ed HCF-light
* WVLAN_48	Miniport								NDIS 5
* WVLAN_49	LINUX  (__LINUX__)						GCC, originally based on pre-compiled HCF_library
* 													migrated to use the HCF sources when Lucent Technologies
* 													brought the HCF module under GPL
* WVLAN_51	Miniport USB							NDIS 5
* WVLAN_52	Miniport								NDIS 4
* WVLAN_53	VxWorks END Station driver
* WVLAN_54	VxWorks END Access Point driver
* WVLAN_81	WavePoint								BORLAND C
* WCITST	Inhouse test tool						Microsoft Visual C 1.5
* WSU		WaveLAN Station Update					Microsoft Visual C ??
* SCO UNIX	not yet actually used ?					?
* __ppc		OEM supplied							?
* _AM29K	OEM supplied							?
* ?			OEM supplied							Microtec Research 80X86 Compiler
*
**************************************************************************************************************
*
*
* SOFTWARE LICENSE
*
* This software is provided subject to the following terms and conditions,
* which you should read carefully before using the software.  Using this
* software indicates your acceptance of these terms and conditions.  If you do
* not agree with these terms and conditions, do not use the software.
*
* COPYRIGHT © 1994 - 1995	by AT&T.				All Rights Reserved
* COPYRIGHT © 1996 - 2000 by Lucent Technologies.	All Rights Reserved
* COPYRIGHT © 2001 - 2004	by Agere Systems Inc.	All Rights Reserved
* All rights reserved.
*
* Redistribution and use in source or binary forms, with or without
* modifications, are permitted provided that the following conditions are met:
*
* . Redistributions of source code must retain the above copyright notice, this
*    list of conditions and the following Disclaimer as comments in the code as
*    well as in the documentation and/or other materials provided with the
*    distribution.
*
* . Redistributions in binary form must reproduce the above copyright notice,
*    this list of conditions and the following Disclaimer in the documentation
*    and/or other materials provided with the distribution.
*
* . Neither the name of Agere Systems Inc. nor the names of the contributors
*    may be used to endorse or promote products derived from this software
*    without specific prior written permission.
*
* Disclaimer
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
* INCLUDING, BUT NOT LIMITED TO, INFRINGEMENT AND THE IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  ANY
* USE, MODIFICATION OR DISTRIBUTION OF THIS SOFTWARE IS SOLELY AT THE USERS OWN
* RISK. IN NO EVENT SHALL AGERE SYSTEMS INC. OR CONTRIBUTORS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, INCLUDING, BUT NOT LIMITED TO, CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
* OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
* DAMAGE.
*
*
*************************************************************************************************************/





typedef unsigned char			hcf_8;
typedef unsigned short			hcf_16;
typedef unsigned long			hcf_32;

#define HCF_IO_MEM     			0x0001	
#define HCF_IO_32BITS   	  	0x0002 	

#define HCF_TYPE_NONE			0x0000	
#define HCF_TYPE_WPA			0x0001	
#define HCF_TYPE_USB			0x0002	
#define HCF_TYPE_WARP			0x0008	
#define HCF_TYPE_PRELOADED		0x0040	
#define HCF_TYPE_HII5			0x0080	
#define HCF_TYPE_CCX			0x0100	
#define HCF_TYPE_BEAGLE_HII5	0x0200	
#define HCF_TYPE_TX_DELAY		0x4000	

#define HCF_ASSERT_NONE			0x0000	
#define HCF_ASSERT_PRINTF		0x0001	
#define HCF_ASSERT_SW_SUP		0x0002	
#define HCF_ASSERT_MB			0x0004	
#define HCF_ASSERT_RT_MSF_RTN	0x4000	
#define HCF_ASSERT_LNK_MSF_RTN	0x8000	

#define HCF_ENC_NONE			0x0000	
#define HCF_ENC					0x0001	
#define HCF_ENC_SUP				0x0002	

#define HCF_EXT_NONE			0x0000	
#define HCF_EXT_INFO_LOG		0x0001	
#define HCF_EXT_INT_TX_EX		0x0004	
#define HCF_EXT_TALLIES_FW		0x0010	
#define HCF_EXT_TALLIES_HCF		0x0020	
#define HCF_EXT_NIC_ACCESS		0x0040	
#define HCF_EXT_MB				0x0080	
#define HCF_EXT_IFB_STRCT 		0x0100	
#define HCF_EXT_DESC_STRCT 		0x0200	
#define HCF_EXT_TX_CONT			0x4000	
#define HCF_EXT_INT_TICK		0x8000	

#define HCF_DDS					0x0001	
#define HCF_CDS					0x0002	

#define HCF_TALLIES_NONE		0x0000	
#define HCF_TALLIES_NIC			0x0001	
#define HCF_TALLIES_HCF			0x0002	
#define HCF_TALLIES_RESET		0x8000	


#ifdef WVLAN_49
#include <asm/io.h>
#include <wl_version.h>


#define HCF_SLEEP (HCF_CDS)


#ifdef HERMES25
#ifdef WARP
#define HCF_TYPE    ( HCF_TYPE_WARP | HCF_TYPE_HII5 )
#else
#define HCF_TYPE    (HCF_TYPE_HII5 | HCF_TYPE_WPA)
#endif 
#else
#define HCF_TYPE    HCF_TYPE_WPA
#endif 

#ifdef ENABLE_DMA
#define HCF_DMA		1
#endif 

#define HCF_EXT ( HCF_EXT_MB | HCF_EXT_INFO_LOG | HCF_EXT_INT_TICK )

#ifndef HCF_TYPE_HII
#define HCF_TYPE_HII 0x0004
#endif

#ifndef HCF_TYPE_AP
#define HCF_TYPE_AP  0x0010
#endif

#ifndef HCF_TYPE_STA
#define HCF_TYPE_STA 0x0020
#endif  

#define HCF_ALIGN		2

#ifndef CNV_INT_TO_LITTLE
#define CNV_INT_TO_LITTLE   CNV_SHORT_TO_LITTLE
#endif

#ifndef CNV_LITTLE_TO_INT
#define CNV_LITTLE_TO_INT   CNV_LITTLE_TO_SHORT
#endif

#define	HCF_ERR_BUSY			0x06

#define UIL_SUCCESS					HCF_SUCCESS
#define UIL_ERR_TIME_OUT			HCF_ERR_TIME_OUT
#define UIL_ERR_NO_NIC				HCF_ERR_NO_NIC
#define UIL_ERR_LEN					HCF_ERR_LEN
#define UIL_ERR_MIN					HCF_ERR_MAX	
#define UIL_ERR_IN_USE				0x44
#define UIL_ERR_WRONG_IFB			0x46
#define UIL_ERR_MAX					0x7F		
#define UIL_ERR_BUSY			    HCF_ERR_BUSY
#define UIL_ERR_DIAG_1			    HCF_ERR_DIAG_1
#define UIL_FAILURE					0xFF	
#define UIL_ERR_PIF_CONFLICT		0x40	
#define UIL_ERR_INCOMP_DRV			0x41	
#define UIL_ERR_DOS_CALL			0x43	
#define UIL_ERR_NO_DRV				0x42	
#define UIL_ERR_NSTL				0x45	



#if 0  
#define HCF_IO              HCF_IO_32BITS
#define HCF_DMA             1
#define HCF_DESC_STRCT_EXT  4

#ifndef BUS_PCMCIA
#define ENABLE_DMA
#endif  

#endif  


#ifdef ENABLE_DMA
#define HCF_MAX_PACKET_SIZE 2304
#else
#define HCF_MAX_PACKET_SIZE 1514
#endif  

#define	MSF_COMPONENT_ID	COMP_ID_LINUX

#define	MSF_COMPONENT_VAR			DRV_VARIANT
#define MSF_COMPONENT_MAJOR_VER     DRV_MAJOR_VERSION
#define MSF_COMPONENT_MINOR_VER     DRV_MINOR_VERSION

#define HCF_ASSERT			HCF_ASSERT_LNK_MSF_RTN	

#ifdef USE_BIG_ENDIAN
#define HCF_BIG_ENDIAN  1
#else
#define HCF_BIG_ENDIAN  0
#endif  


#define IN_PORT_BYTE(port)			((hcf_8)inb( (hcf_io)(port) ))
#define IN_PORT_WORD(port)			((hcf_16)inw( (hcf_io)(port) ))
#define OUT_PORT_BYTE(port, value)	(outb( (hcf_8) (value), (hcf_io)(port) ))
#define OUT_PORT_WORD(port, value)	(outw((hcf_16) (value), (hcf_io)(port) ))

#define IN_PORT_STRING_16(port, dst, n)    insw((hcf_io)(port), dst, n)
#define OUT_PORT_STRING_16(port, src, n)   outsw((hcf_io)(port), src, n)
#define IN_PORT_STRING_32(port, dst, n)   insl((port), (dst), (n))
#define OUT_PORT_STRING_32(port, src, n)  outsl((port), (src), (n))
#define IN_PORT_HCF32(port)          inl( (hcf_io)(port) )
#define OUT_PORT_HCF32(port, value)  outl((hcf_32)(value), (hcf_io)(port) )

#define IN_PORT_DWORD(port)          IN_PORT_HCF32(port)
#define OUT_PORT_DWORD(port, value)  OUT_PORT_HCF32(port, value)

#define  IN_PORT_STRING_8_16(port, addr, len)	IN_PORT_STRING_16(port, addr, len)
#define  OUT_PORT_STRING_8_16(port, addr, len)	OUT_PORT_STRING_16(port, addr, len)

#ifndef CFG_SCAN_CHANNELS_2GHZ
#define CFG_SCAN_CHANNELS_2GHZ 0xFCC2
#endif 

#define HCF_MAX_MSG 1600 
#endif	

#if ! defined 	HCF_ALIGN
#define 		HCF_ALIGN			1		
#endif 

#if ! defined 	HCF_ASSERT
#define 		HCF_ASSERT			0
#endif 

#if ! defined	HCF_BIG_ENDIAN
#define 		HCF_BIG_ENDIAN		0
#endif 

#if ! defined	HCF_DMA
#define 		HCF_DMA				0
#endif 

#if ! defined	HCF_ENCAP
#define			HCF_ENCAP			HCF_ENC
#endif 

#if ! defined	HCF_EXT
#define			HCF_EXT				0
#endif 

#if ! defined	HCF_INT_ON
#define			HCF_INT_ON			1
#endif 

#if ! defined	HCF_IO
#define			HCF_IO				0		
#endif 

#if ! defined	HCF_LEGACY
#define			HCF_LEGACY			0
#endif 

#if ! defined	HCF_MAX_LTV
#define 		HCF_MAX_LTV			1200	
#endif 

#if ! defined	HCF_PROT_TIME
#define 		HCF_PROT_TIME		100		
#endif 

#if ! defined	HCF_SLEEP
#define			HCF_SLEEP			0
#endif 

#if ! defined	HCF_TALLIES
#define			HCF_TALLIES			( HCF_TALLIES_NIC | HCF_TALLIES_HCF )
#endif 

#if ! defined 	HCF_TYPE
#define 		HCF_TYPE 			0
#endif 

#if				HCF_BIG_ENDIAN
#undef			HCF_BIG_ENDIAN
#define			HCF_BIG_ENDIAN		1		
#endif 

#if				HCF_DMA
#undef			HCF_DMA
#define			HCF_DMA				1		
#endif 

#if				HCF_INT_ON
#undef			HCF_INT_ON
#define			HCF_INT_ON			1		
#endif 


#if ! defined IN_PORT_STRING_8_16
#define  IN_PORT_STRING_8_16(port, addr, len)	IN_PORT_STRING_16(port, addr, len)
#define  OUT_PORT_STRING_8_16(port, addr, len)	OUT_PORT_STRING_16(port, addr, len)
#endif 


#if ! defined	FAR
#define 		FAR						
#endif 

typedef hcf_8  FAR *wci_bufp;			
typedef hcf_16 FAR *wci_recordp;		


#if HCF_IO & HCF_IO_MEM
typedef hcf_32 hcf_io;
#else
typedef hcf_16 hcf_io;
#endif 

#if 	HCF_PROT_TIME > 128
#define HCF_PROT_TIME_SHFT	3
#define HCF_PROT_TIME_DIV	8
#elif 	HCF_PROT_TIME > 64
#define HCF_PROT_TIME_SHFT	2
#define HCF_PROT_TIME_DIV	4
#elif 	HCF_PROT_TIME > 32
#define HCF_PROT_TIME_SHFT	1
#define HCF_PROT_TIME_DIV	2
#else 
#define HCF_PROT_TIME_SHFT	0
#define HCF_PROT_TIME_DIV	1
#endif

#define HCF_PROT_TIME_CNT (HCF_PROT_TIME / HCF_PROT_TIME_DIV)




#if defined	MSF_COMPONENT_ID

#if ! defined	DUI_COMPAT_VAR
#define			DUI_COMPAT_VAR		MSF_COMPONENT_ID
#endif 

#if ! defined	DUI_COMPAT_BOT		
#define			DUI_COMPAT_BOT		8
#endif 

#if ! defined	DUI_COMPAT_TOP		
#define			DUI_COMPAT_TOP       8
#endif 

#endif 

#if (HCF_TYPE) & HCF_TYPE_HII5

#if ! defined	HCF_HSI_VAR_5
#define			HCF_HSI_VAR_5
#endif 

#if ! defined	HCF_APF_VAR_4
#define			HCF_APF_VAR_4
#endif 

#if (HCF_TYPE) & HCF_TYPE_WARP
#if ! defined	HCF_STA_VAR_4
#define			HCF_STA_VAR_4
#endif 
#else
#if ! defined	HCF_STA_VAR_2
#define			HCF_STA_VAR_2
#endif 
#endif

#if defined HCF_HSI_VAR_4
err: HSI variants 4 correspond with HII;
#endif 

#else

#if ! defined	HCF_HSI_VAR_4
#define			HCF_HSI_VAR_4		
#endif 

#if ! defined	HCF_APF_VAR_2
#define 		HCF_APF_VAR_2
#endif 

#if ! defined	HCF_STA_VAR_2
#define			HCF_STA_VAR_2
#endif 

#endif 

#if ! defined	HCF_PRI_VAR_3
#define		HCF_PRI_VAR_3
#endif 

#if defined HCF_HSI_VAR_1 || defined HCF_HSI_VAR_2 || defined HCF_HSI_VAR_3
err: HSI variants 1, 2 and 3 correspond with H-I only;
#endif 

#if defined HCF_PRI_VAR_1 || defined HCF_PRI_VAR_2
err: primary variants 1 and 2 correspond with H-I only;
#endif 





#if ! defined	BASED
#define 		BASED
#endif 

#if ! defined	EXTERN_C
#ifdef __cplusplus
#define			EXTERN_C extern "C"
#else
#define			EXTERN_C
#endif 
#endif 

#if ! defined	NULL
#define			NULL	((void *) 0)
#endif 

#if ! defined	TEXT
#define			TEXT(x)	x
#endif 

#if HCF_ALIGN != 1 && HCF_ALIGN != 2 && HCF_ALIGN != 4 && HCF_ALIGN != 8
err: invalid value for HCF_ALIGN;
#endif 

#if (HCF_ASSERT) & ~( HCF_ASSERT_PRINTF | HCF_ASSERT_SW_SUP | HCF_ASSERT_MB | HCF_ASSERT_RT_MSF_RTN | \
					  HCF_ASSERT_LNK_MSF_RTN )
err: invalid value for HCF_ASSERT;
#endif 

#if (HCF_ASSERT) & HCF_ASSERT_MB && ! ( (HCF_EXT) & HCF_EXT_MB )		
err: these macros are not used consistently;
#endif 

#if HCF_BIG_ENDIAN != 0 && HCF_BIG_ENDIAN != 1
err: invalid value for HCF_BIG_ENDIAN;
#endif 

#if HCF_DMA != 0 && HCF_DMA != 1
err: invalid value for HCF_DMA;
#endif 

#if (HCF_ENCAP) & ~( HCF_ENC | HCF_ENC_SUP )
err: invalid value for HCF_ENCAP;
#endif 

#if (HCF_EXT) & ~( HCF_EXT_INFO_LOG | HCF_EXT_INT_TX_EX | HCF_EXT_TALLIES_FW | HCF_EXT_TALLIES_HCF	| \
				   HCF_EXT_NIC_ACCESS | HCF_EXT_MB | HCF_EXT_INT_TICK | \
				   HCF_EXT_IFB_STRCT | HCF_EXT_DESC_STRCT | HCF_EXT_TX_CONT )
err: invalid value for HCF_EXT;
#endif 

#if HCF_INT_ON != 0 && HCF_INT_ON != 1
err: invalid value for HCF_INT_ON;
#endif 

#if (HCF_IO) & ~( HCF_IO_MEM | HCF_IO_32BITS )
err: invalid value for HCF_IO;
#endif 

#if HCF_LEGACY != 0 && HCF_LEGACY != 1
err: invalid value for HCF_LEGACY;
#endif 

#if HCF_MAX_LTV < 16 || HCF_MAX_LTV > 2304
err: invalid value for HCF_MAX_LTV;
#endif 

#if HCF_PROT_TIME != 0 && ( HCF_PROT_TIME < 19 || 256 < HCF_PROT_TIME )
err: below minimum .08 second required by Hermes or possibly above hcf_32 capacity;
#endif 

#if (HCF_SLEEP) & ~( HCF_CDS | HCF_DDS )
err: invalid value for HCF_SLEEP;
#endif 

#if (HCF_SLEEP) && ! (HCF_INT_ON)
err: these macros are not used consistently;
#endif 

#if (HCF_SLEEP) && ! ( (HCF_EXT) & HCF_EXT_INT_TICK )
#endif 

#if (HCF_TALLIES) & ~( HCF_TALLIES_HCF | HCF_TALLIES_NIC | HCF_TALLIES_RESET ) || \
	(HCF_TALLIES) == HCF_TALLIES_RESET
err: invalid value for HCF_TALLIES;
#endif 

#if (HCF_TYPE) & ~(HCF_TYPE_WPA | HCF_TYPE_USB | HCF_TYPE_PRELOADED | HCF_TYPE_HII5 | HCF_TYPE_WARP | \
		HCF_TYPE_CCX  )
err: invalid value for HCF_TYPE;
#endif 

#if (HCF_TYPE) & HCF_TYPE_WARP && (HCF_TYPE) & HCF_TYPE_WPA
err: at most 1 of these macros should be defined;
#endif 

#endif 

