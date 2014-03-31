
#ifndef HCF_H
#define HCF_H 1

/************************************************************************************************************
*
* FILE	 : hcf.h
*
* DATE   : $Date: 2004/08/05 11:47:10 $   $Revision: 1.7 $
* Original: 2004/05/19 07:26:01    Revision: 1.56      Tag: hcf7_t20040602_01
* Original: 2004/05/12 08:47:23    Revision: 1.53      Tag: hcf7_t7_20040513_01
* Original: 2004/04/15 09:24:42    Revision: 1.46      Tag: hcf7_t7_20040415_01
* Original: 2004/04/08 15:18:16    Revision: 1.45      Tag: t7_20040413_01
* Original: 2004/04/01 15:32:55    Revision: 1.43      Tag: t7_20040401_01
* Original: 2004/03/10 15:39:28    Revision: 1.39      Tag: t20040310_01
* Original: 2004/03/04 11:03:38    Revision: 1.37      Tag: t20040304_01
* Original: 2004/03/02 14:51:21    Revision: 1.35      Tag: t20040302_03
* Original: 2004/02/24 13:00:28    Revision: 1.28      Tag: t20040224_01
* Original: 2004/02/09 14:50:14    Revision: 1.26      Tag: t20040219_01
*
* AUTHOR : Nico Valster
*
* SPECIFICATION: ..........
*
* DESC   : Definitions and Prototypes for MSF as well as HCF sources
*
*		   Customizable via HCFCFG.H
*
*
**************************************************************************************************************

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


#include "hcfcfg.h"	
#include "mdd.h"	



#define LOF(x) 			(sizeof(x)/sizeof(hcf_16)-1)



#define CNV_END_SHORT(w)  (hcf_16)( ((hcf_16)(w) & 0x00FF) << 8 | ((hcf_16)(w) & 0xFF00) >> 8 )
#define CNV_END_LONG(dw)  (hcf_32)( (dw >> 24) | ((dw >> 8) & 0xff00) | ((dw << 8) & 0xff0000) | (dw << 24) )

#if HCF_BIG_ENDIAN
#define CNV_LITTLE_TO_SHORT(w)	CNV_END_SHORT(w)	
#define CNV_BIG_TO_SHORT(w)		(w)				
#define CNV_LITTLE_TO_LONG(dw)	CNV_END_LONG(dw)
#define CNV_LONG_TO_LITTLE(dw)	CNV_END_LONG(dw)
#else
#define CNV_LITTLE_TO_SHORT(w) 	(w)				
#define CNV_BIG_TO_SHORT(w)		CNV_END_SHORT(w)	
#define CNV_LITTLE_TO_LONG(dw)	(dw)
#define CNV_LONG_TO_LITTLE(dw)	(dw)

#if defined HCF_ALIGN && HCF_ALIGN > 1
#define CNV_SHORTP_TO_LITTLE(pw)	((hcf_16)(*(hcf_8 *)pw)) 			| ((hcf_16)(*((hcf_8 *)pw+1)) << 8)
#define CNV_LONGP_TO_LITTLE(pdw)	((hcf_32)(*(hcf_8 *)pdw)) 			| ((hcf_32)(*((hcf_8 *)pdw+1)) << 8) 	| \
									((hcf_32)(*((hcf_8 *)pdw+2)) << 16) | ((hcf_32)(*((hcf_8 *)pdw+3)) << 24)
#else
#define CNV_LONGP_TO_LITTLE(pdw)	(*(hcf_32 *)pdw)
#define CNV_SHORTP_TO_LITTLE(pw)	(*(hcf_16 *)pw)
#endif

#endif 

#define CNV_SHORT_TO_LITTLE(w)	CNV_LITTLE_TO_SHORT(w)
#define CNV_SHORT_TO_BIG(w)		CNV_BIG_TO_SHORT(w)



#define HFS_STAT				0x0000
#define HFS_SWSUP				0x0006					
#define HFS_Q_INFO				0x0006					
#define HFS_RATE				0x0008					
#define 	HFS_STAT_ERR		RX_STAT_ERR				
#define HFS_TX_CNTL				0x0036
														
#define HFS_DAT_LEN				(HFS_ADDR_DEST - 2)		
#define HFS_ADDR_DEST			0x003A					
#define HFS_ADDR_SRC			(HFS_ADDR_DEST + 6)		
#define HFS_LEN					(HFS_ADDR_SRC  + 6)		
#define HFS_DAT					(HFS_LEN       + 2)		
#define HFS_TYPE				(HFS_DAT       + 6)		



#if HCF_BIG_ENDIAN
#define DESC_STRCT_CNT		0
#define DESC_STRCT_SIZE		1
#else
#define DESC_STRCT_CNT		1
#define DESC_STRCT_SIZE		0
#endif 

#define	BUF_CNT				buf_dim[DESC_STRCT_CNT]
#define	BUF_SIZE			buf_dim[DESC_STRCT_SIZE]

typedef struct DESC_STRCT {
	hcf_16					buf_dim[2];
	hcf_32					buf_phys_addr;
	hcf_32					next_desc_phys_addr;	
	hcf_32					desc_phys_addr;			
	struct DESC_STRCT 		*next_desc_addr;
	hcf_8	FAR				*buf_addr;
#if (HCF_EXT) & HCF_EXT_DESC_STRCT
	void FAR			   *DESC_MSFSup;			
#endif 
} DESC_STRCT;

#define HCF_DASA_SIZE			12							

#define DESC_CNT_MASK 			0x0FFF

#define GET_BUF_SIZE(descp)       ((descp)->BUF_SIZE)
#define GET_BUF_CNT(descp)        ((descp)->BUF_CNT)
#define SET_BUF_SIZE(descp, size) (descp)->BUF_SIZE = size;
#define SET_BUF_CNT(descp, count) (descp)->BUF_CNT = count;


typedef struct  {  
  hcf_32	TxUnicastFrames;
  hcf_32	TxMulticastFrames;
  hcf_32	TxFragments;
  hcf_32	TxUnicastOctets;
  hcf_32	TxMulticastOctets;
  hcf_32	TxDeferredTransmissions;
  hcf_32	TxSingleRetryFrames;
  hcf_32	TxMultipleRetryFrames;
  hcf_32	TxRetryLimitExceeded;
  hcf_32	TxDiscards;
  hcf_32	RxUnicastFrames;
  hcf_32	RxMulticastFrames;
  hcf_32	RxFragments;
  hcf_32	RxUnicastOctets;
  hcf_32	RxMulticastOctets;
  hcf_32	RxFCSErrors;
  hcf_32	RxDiscardsNoBuffer;
  hcf_32	TxDiscardsWrongSA;
  hcf_32	RxWEPUndecryptable;
  hcf_32	RxMsgInMsgFragments;
  hcf_32	RxMsgInBadMsgFragments;
  hcf_32	RxDiscardsWEPICVError;
  hcf_32	RxDiscardsWEPExcluded;
#if (HCF_EXT) & HCF_EXT_TALLIES_FW
  hcf_32	TalliesExtra[32];
#endif 
} CFG_HERMES_TALLIES_STRCT;

typedef struct  {  
  hcf_32	NoBufInfo;  				
  hcf_32	NoBufMB;					
  hcf_32	MiscErr;					
#if (HCF_EXT) & HCF_EXT_TALLIES_FW
  hcf_32	EngCnt[8];
#endif 
} CFG_HCF_TALLIES_STRCT;

#if (HCF_TALLIES) & ( HCF_TALLIES_NIC | HCF_TALLIES_HCF )
#if (HCF_TALLIES) & HCF_TALLIES_NIC	
#define		HCF_NIC_TAL_CNT	(sizeof(CFG_HERMES_TALLIES_STRCT)/ sizeof(hcf_32))
#else
#define		HCF_NIC_TAL_CNT	0
#endif 
#if (HCF_TALLIES) & HCF_TALLIES_HCF	
#define		HCF_HCF_TAL_CNT	(sizeof(CFG_HCF_TALLIES_STRCT)   / sizeof(hcf_32))
#else
#define		HCF_HCF_TAL_CNT	0
#endif 
#define HCF_TOT_TAL_CNT ( HCF_NIC_TAL_CNT + HCF_NIC_TAL_CNT )
#endif 



#define IFB_VERSION 0x0E	 			

typedef struct  {
  hcf_io		IFB_IOBase;				
  hcf_16		IFB_IORange;			
  hcf_16		IFB_DLMode;				
  hcf_16		IFB_Cmd;				
  hcf_16		IFB_RxFID;				
  hcf_16		IFB_TxFID;				
  hcf_16		IFB_RxLen;				
  hcf_16		IFB_DefunctStat;		
  hcf_16		IFB_ErrCmd;				
  hcf_16		IFB_ErrQualifier;		
  hcf_16		IFB_lal;				
  wci_bufp		IFB_lap;				
  hcf_16		IFB_LinkStat;			
  hcf_16		IFB_DSLinkStat;			
  hcf_16		IFB_CarryIn;			
  hcf_16		IFB_CarryOut;			
  hcf_16		IFB_Version;			
  hcf_16		IFB_CardStat;			
  hcf_16  		IFB_RscInd;				
  hcf_16		IFB_CntlOpt;			
  hcf_16		IFB_BusType;			
  CFG_FW_IDENTITY_STRCT	 IFB_FWIdentity; 
#if defined MSF_COMPONENT_ID
  CFG_SUP_RANGE_STRCT	 IFB_FWSup;
  CFG_PRI_IDENTITY_STRCT IFB_PRIIdentity;
  CFG_SUP_RANGE_STRCT	 IFB_PRISup;
  CFG_SUP_RANGE_STRCT	 IFB_HSISup;
#endif 
#if (HCF_EXT) & HCF_EXT_INFO_LOG
  RID_LOGP		IFB_RIDLogp;			
#endif 
#if HCF_PROT_TIME
  hcf_32	 	IFB_TickIni;			
#endif 
#if (HCF_EXT) & HCF_EXT_INT_TICK
  int			IFB_TickCnt;			
#endif 
  hcf_16 	   *IFB_MBp;				
  hcf_16		IFB_MBSize;				
  hcf_16		IFB_MBWp;				
  hcf_16		IFB_MBRp;				
  hcf_16		IFB_MBInfoLen;			
#if (HCF_TYPE) & HCF_TYPE_WPA
  hcf_16		IFB_MICTxCntl;			
  hcf_32		IFB_MICTxKey[2];		
  hcf_32		IFB_MICTx[2];   	  	
  hcf_16		IFB_MICTxCarry;			
  hcf_16		IFB_MICRxCarry;			
  hcf_32		IFB_MICRxKey[4*2];		
  hcf_32		IFB_MICRx[2]; 	 		
#endif 
#if HCF_ASSERT
#if (HCF_ASSERT) & HCF_ASSERT_MB
  CFG_MB_INFO_RANGE1_STRCT	IFB_AssertStrct; 
#endif 
  										
  hcf_16		IFB_AssertLine;			
  hcf_16		IFB_AssertTrace;		
  hcf_32		IFB_AssertQualifier;	
  hcf_16		IFB_AssertLvl;			
  hcf_16		IFB_AssertWhere;		
#if (HCF_ASSERT) & ( HCF_ASSERT_LNK_MSF_RTN | HCF_ASSERT_RT_MSF_RTN )
  MSF_ASSERT_RTNP	IFB_AssertRtn;		
#endif 
#if (HCF_ASSERT) & HCF_ASSERT_PRINTF	
   hcf_16       IFB_DbgPrintF_Cnt;
   CFG_FW_PRINTF_BUFFER_LOCATION_STRCT IFB_FwPfBuff;
#endif 
#endif 
  hcf_16 volatile IFB_IntOffCnt;		
#if (HCF_TALLIES) & ( HCF_TALLIES_NIC | HCF_TALLIES_HCF )	
  hcf_32		IFB_Silly_you_should_align;	
  hcf_16		IFB_TallyLen;			
  hcf_16		IFB_TallyTyp;			
#endif 
#if (HCF_TALLIES) & HCF_TALLIES_NIC		
  CFG_HERMES_TALLIES_STRCT	IFB_NIC_Tallies;
#endif 
#if (HCF_TALLIES) & HCF_TALLIES_HCF		
  CFG_HCF_TALLIES_STRCT		IFB_HCF_Tallies;
#endif 
#if HCF_DMA
  
  
  DESC_STRCT	*IFB_FirstDesc[2];
  DESC_STRCT	*IFB_LastDesc[2];
  DESC_STRCT	*IFB_ConfinedDesc[2];	
  hcf_16		IFB_DmaPackets;			
#endif 
#if (HCF_EXT) & HCF_EXT_INT_TX_EX
  hcf_16		IFB_TxFsStat;			
  hcf_16		IFB_TxFsGap[2];			
  hcf_16		IFB_TxFsSwSup;
#endif 
  hcf_16		IFB_Magic;				
#if (HCF_EXT) & HCF_EXT_IFB_STRCT		
  void FAR	   *IFB_MSFSup;				
#endif 
} IFB_STRCT;

typedef IFB_STRCT*	IFBP;



EXTERN_C int		 hcf_action			(IFBP ifbp, hcf_16 cmd );
EXTERN_C int		 hcf_connect		(IFBP ifbp, hcf_io io_base );
EXTERN_C int		 hcf_get_info		(IFBP ifbp, LTVP ltvp );
EXTERN_C int		 hcf_service_nic	(IFBP ifbp, wci_bufp bufp, unsigned int len );
EXTERN_C int		 hcf_cntl			(IFBP ifbp, hcf_16 cmd );
EXTERN_C int		 hcf_put_info		(IFBP ifbp, LTVP ltvp );
EXTERN_C int		 hcf_rcv_msg		(IFBP ifbp, DESC_STRCT *descp, unsigned int offset );
EXTERN_C int		 hcf_send_msg       (IFBP ifbp, DESC_STRCT *dp, hcf_16 tx_cntl );
#if HCF_DMA
EXTERN_C void		 hcf_dma_tx_put 	(IFBP ifbp, DESC_STRCT *d, hcf_16 tx_cntl );
EXTERN_C DESC_STRCT* hcf_dma_tx_get		(IFBP ifbp );
EXTERN_C DESC_STRCT* hcf_dma_rx_get		(IFBP ifbp );
EXTERN_C void		 hcf_dma_rx_put		(IFBP ifbp, DESC_STRCT *d );
#endif 
#if (HCF_ASSERT) & HCF_ASSERT_LNK_MSF_RTN
EXTERN_C void		 msf_assert	 		(unsigned int line_number, hcf_16 trace, hcf_32 qual );
#endif 

#endif  

