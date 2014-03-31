/*
 *	FILE    	SA-1100.h
 *
 *	Version 	1.2
 *	Author  	Copyright (c) Marc A. Viredaz, 1998
 *	        	DEC Western Research Laboratory, Palo Alto, CA
 *	Date    	January 1998 (April 1997)
 *	System  	StrongARM SA-1100
 *	Language	C or ARM Assembly
 *	Purpose 	Definition of constants related to the StrongARM
 *	        	SA-1100 microprocessor (Advanced RISC Machine (ARM)
 *	        	architecture version 4). This file is based on the
 *	        	StrongARM SA-1100 data sheet version 2.2.
 *
 */


#ifndef __ASM_ARCH_HARDWARE_H
#error You must include hardware.h not SA-1100.h
#endif

#include "bitfield.h"


#define SA1100_CS0_PHYS	0x00000000
#define SA1100_CS1_PHYS	0x08000000
#define SA1100_CS2_PHYS	0x10000000
#define SA1100_CS3_PHYS	0x18000000
#define SA1100_CS4_PHYS	0x40000000
#define SA1100_CS5_PHYS	0x48000000


#define PCMCIAPrtSp	0x04000000	
#define PCMCIASp	(4*PCMCIAPrtSp)	
#define PCMCIAIOSp	PCMCIAPrtSp	
#define PCMCIAAttrSp	PCMCIAPrtSp	
#define PCMCIAMemSp	PCMCIAPrtSp	

#define PCMCIA0Sp	PCMCIASp	
#define PCMCIA0IOSp	PCMCIAIOSp	
#define PCMCIA0AttrSp	PCMCIAAttrSp	
#define PCMCIA0MemSp	PCMCIAMemSp	

#define PCMCIA1Sp	PCMCIASp	
#define PCMCIA1IOSp	PCMCIAIOSp	
#define PCMCIA1AttrSp	PCMCIAAttrSp	
#define PCMCIA1MemSp	PCMCIAMemSp	

#define _PCMCIA(Nb)	        	 \
                	(0x20000000 + (Nb)*PCMCIASp)
#define _PCMCIAIO(Nb)	_PCMCIA (Nb)	
#define _PCMCIAAttr(Nb)	        	 \
                	(_PCMCIA (Nb) + 2*PCMCIAPrtSp)
#define _PCMCIAMem(Nb)	        	 \
                	(_PCMCIA (Nb) + 3*PCMCIAPrtSp)

#define _PCMCIA0	_PCMCIA (0)	
#define _PCMCIA0IO	_PCMCIAIO (0)	
#define _PCMCIA0Attr	_PCMCIAAttr (0)	
#define _PCMCIA0Mem	_PCMCIAMem (0)	

#define _PCMCIA1	_PCMCIA (1)	
#define _PCMCIA1IO	_PCMCIAIO (1)	
#define _PCMCIA1Attr	_PCMCIAAttr (1)	
#define _PCMCIA1Mem	_PCMCIAMem (1)	



#define Ser0UDCCR	__REG(0x80000000)  
#define Ser0UDCAR	__REG(0x80000004)  
#define Ser0UDCOMP	__REG(0x80000008)  
#define Ser0UDCIMP	__REG(0x8000000C)  
#define Ser0UDCCS0	__REG(0x80000010)  
#define Ser0UDCCS1	__REG(0x80000014)  
#define Ser0UDCCS2	__REG(0x80000018)  
#define Ser0UDCD0	__REG(0x8000001C)  
#define Ser0UDCWC	__REG(0x80000020)  
#define Ser0UDCDR	__REG(0x80000028)  
#define Ser0UDCSR	__REG(0x80000030)  

#define UDCCR_UDD	0x00000001	
#define UDCCR_UDA	0x00000002	
#define UDCCR_RESIM	0x00000004	
#define UDCCR_EIM	0x00000008	
                	        	
#define UDCCR_RIM	0x00000010	
                	        	
#define UDCCR_TIM	0x00000020	
                	        	
#define UDCCR_SRM	0x00000040	
                	        	
#define UDCCR_SUSIM	UDCCR_SRM	
#define UDCCR_REM	0x00000080	

#define UDCAR_ADD	Fld (7, 0)	

#define UDCOMP_OUTMAXP	Fld (8, 0)	
                	        	
#define UDCOMP_OutMaxPkt(Size)  	 \
                	        	 \
                	(((Size) - 1) << FShft (UDCOMP_OUTMAXP))

#define UDCIMP_INMAXP	Fld (8, 0)	
                	        	
#define UDCIMP_InMaxPkt(Size)   	 \
                	        	 \
                	(((Size) - 1) << FShft (UDCIMP_INMAXP))

#define UDCCS0_OPR	0x00000001	
#define UDCCS0_IPR	0x00000002	
#define UDCCS0_SST	0x00000004	
#define UDCCS0_FST	0x00000008	
#define UDCCS0_DE	0x00000010	
#define UDCCS0_SE	0x00000020	
#define UDCCS0_SO	0x00000040	
                	        	
#define UDCCS0_SSE	0x00000080	

#define UDCCS1_RFS	0x00000001	
                	        	
#define UDCCS1_RPC	0x00000002	
#define UDCCS1_RPE	0x00000004	
#define UDCCS1_SST	0x00000008	
#define UDCCS1_FST	0x00000010	
#define UDCCS1_RNE	0x00000020	

#define UDCCS2_TFS	0x00000001	
                	        	
#define UDCCS2_TPC	0x00000002	
#define UDCCS2_TPE	0x00000004	
#define UDCCS2_TUR	0x00000008	
#define UDCCS2_SST	0x00000010	
#define UDCCS2_FST	0x00000020	

#define UDCD0_DATA	Fld (8, 0)	

#define UDCWC_WC	Fld (4, 0)	

#define UDCDR_DATA	Fld (8, 0)	

#define UDCSR_EIR	0x00000001	
#define UDCSR_RIR	0x00000002	
#define UDCSR_TIR	0x00000004	
#define UDCSR_SUSIR	0x00000008	
#define UDCSR_RESIR	0x00000010	
#define UDCSR_RSTIR	0x00000020	



#define _UTCR0(Nb)	__REG(0x80010000 + ((Nb) - 1)*0x00020000)  
#define _UTCR1(Nb)	__REG(0x80010004 + ((Nb) - 1)*0x00020000)  
#define _UTCR2(Nb)	__REG(0x80010008 + ((Nb) - 1)*0x00020000)  
#define _UTCR3(Nb)	__REG(0x8001000C + ((Nb) - 1)*0x00020000)  
#define _UTCR4(Nb)	__REG(0x80010010 + ((Nb) - 1)*0x00020000)  
#define _UTDR(Nb)	__REG(0x80010014 + ((Nb) - 1)*0x00020000)  
#define _UTSR0(Nb)	__REG(0x8001001C + ((Nb) - 1)*0x00020000)  
#define _UTSR1(Nb)	__REG(0x80010020 + ((Nb) - 1)*0x00020000)  

#define Ser1UTCR0	_UTCR0 (1)	
#define Ser1UTCR1	_UTCR1 (1)	
#define Ser1UTCR2	_UTCR2 (1)	
#define Ser1UTCR3	_UTCR3 (1)	
#define Ser1UTDR	_UTDR (1)	
#define Ser1UTSR0	_UTSR0 (1)	
#define Ser1UTSR1	_UTSR1 (1)	

#define Ser2UTCR0	_UTCR0 (2)	
#define Ser2UTCR1	_UTCR1 (2)	
#define Ser2UTCR2	_UTCR2 (2)	
#define Ser2UTCR3	_UTCR3 (2)	
#define Ser2UTCR4	_UTCR4 (2)	
#define Ser2UTDR	_UTDR (2)	
#define Ser2UTSR0	_UTSR0 (2)	
#define Ser2UTSR1	_UTSR1 (2)	

#define Ser3UTCR0	_UTCR0 (3)	
#define Ser3UTCR1	_UTCR1 (3)	
#define Ser3UTCR2	_UTCR2 (3)	
#define Ser3UTCR3	_UTCR3 (3)	
#define Ser3UTDR	_UTDR (3)	
#define Ser3UTSR0	_UTSR0 (3)	
#define Ser3UTSR1	_UTSR1 (3)	

#define _Ser1UTCR0	__PREG(Ser1UTCR0)
#define _Ser2UTCR0	__PREG(Ser2UTCR0)
#define _Ser3UTCR0	__PREG(Ser3UTCR0)

#define UTCR0		0x00
#define UTCR1		0x04
#define UTCR2		0x08
#define UTCR3		0x0c
#define UTDR		0x14
#define UTSR0		0x1c
#define UTSR1		0x20

#define UTCR0_PE	0x00000001	
#define UTCR0_OES	0x00000002	
#define UTCR0_OddPar	(UTCR0_OES*0)	
#define UTCR0_EvenPar	(UTCR0_OES*1)	
#define UTCR0_SBS	0x00000004	
#define UTCR0_1StpBit	(UTCR0_SBS*0)	
#define UTCR0_2StpBit	(UTCR0_SBS*1)	
#define UTCR0_DSS	0x00000008	
#define UTCR0_7BitData	(UTCR0_DSS*0)	
#define UTCR0_8BitData	(UTCR0_DSS*1)	
#define UTCR0_SCE	0x00000010	
                	        	
                	        	
#define UTCR0_RCE	0x00000020	
#define UTCR0_RcRsEdg	(UTCR0_RCE*0)	
#define UTCR0_RcFlEdg	(UTCR0_RCE*1)	
#define UTCR0_TCE	0x00000040	
#define UTCR0_TrRsEdg	(UTCR0_TCE*0)	
#define UTCR0_TrFlEdg	(UTCR0_TCE*1)	
#define UTCR0_Ser2IrDA	        	 \
                	(UTCR0_1StpBit + UTCR0_8BitData)

#define UTCR1_BRD	Fld (4, 0)	
#define UTCR2_BRD	Fld (8, 0)	
                	        	
                	        	
#define UTCR1_BdRtDiv(Div)      	 \
                	(((Div) - 16)/16 >> FSize (UTCR2_BRD) << \
                	 FShft (UTCR1_BRD))
#define UTCR2_BdRtDiv(Div)      	 \
                	(((Div) - 16)/16 & FAlnMsk (UTCR2_BRD) << \
                	 FShft (UTCR2_BRD))
                	        	
                	        	
#define UTCR1_CeilBdRtDiv(Div)  	 \
                	(((Div) - 1)/16 >> FSize (UTCR2_BRD) << \
                	 FShft (UTCR1_BRD))
#define UTCR2_CeilBdRtDiv(Div)  	 \
                	(((Div) - 1)/16 & FAlnMsk (UTCR2_BRD) << \
                	 FShft (UTCR2_BRD))
                	        	
                	        	

#define UTCR3_RXE	0x00000001	
#define UTCR3_TXE	0x00000002	
#define UTCR3_BRK	0x00000004	
#define UTCR3_RIE	0x00000008	
                	        	
#define UTCR3_TIE	0x00000010	
                	        	
#define UTCR3_LBM	0x00000020	
#define UTCR3_Ser2IrDA	        	 \
                	        	 \
                	(UTCR3_RXE + UTCR3_TXE)

#define UTCR4_HSE	0x00000001	
                	        	
#define UTCR4_NRZ	(UTCR4_HSE*0)	
#define UTCR4_HPSIR	(UTCR4_HSE*1)	
#define UTCR4_LPM	0x00000002	
#define UTCR4_Z3_16Bit	(UTCR4_LPM*0)	
#define UTCR4_Z1_6us	(UTCR4_LPM*1)	

#define UTDR_DATA	Fld (8, 0)	
#if 0           	        	
#define UTDR_PRE	0x00000100	
#define UTDR_FRE	0x00000200	
#define UTDR_ROR	0x00000400	
#endif 

#define UTSR0_TFS	0x00000001	
                	        	
#define UTSR0_RFS	0x00000002	
                	        	
#define UTSR0_RID	0x00000004	
#define UTSR0_RBB	0x00000008	
#define UTSR0_REB	0x00000010	
#define UTSR0_EIF	0x00000020	

#define UTSR1_TBY	0x00000001	
#define UTSR1_RNE	0x00000002	
#define UTSR1_TNF	0x00000004	
#define UTSR1_PRE	0x00000008	
#define UTSR1_FRE	0x00000010	
#define UTSR1_ROR	0x00000020	



#define Ser1SDCR0	__REG(0x80020060)  
#define Ser1SDCR1	__REG(0x80020064)  
#define Ser1SDCR2	__REG(0x80020068)  
#define Ser1SDCR3	__REG(0x8002006C)  
#define Ser1SDCR4	__REG(0x80020070)  
#define Ser1SDDR	__REG(0x80020078)  
#define Ser1SDSR0	__REG(0x80020080)  
#define Ser1SDSR1	__REG(0x80020084)  

#define SDCR0_SUS	0x00000001	
#define SDCR0_SDLC	(SDCR0_SUS*0)	
#define SDCR0_UART	(SDCR0_SUS*1)	
#define SDCR0_SDF	0x00000002	
#define SDCR0_SglFlg	(SDCR0_SDF*0)	
#define SDCR0_DblFlg	(SDCR0_SDF*1)	
#define SDCR0_LBM	0x00000004	
#define SDCR0_BMS	0x00000008	
#define SDCR0_FM0	(SDCR0_BMS*0)	
#define SDCR0_NRZ	(SDCR0_BMS*1)	
#define SDCR0_SCE	0x00000010	
#define SDCR0_SCD	0x00000020	
                	        	
#define SDCR0_SClkIn	(SDCR0_SCD*0)	
#define SDCR0_SClkOut	(SDCR0_SCD*1)	
#define SDCR0_RCE	0x00000040	
#define SDCR0_RcRsEdg	(SDCR0_RCE*0)	
#define SDCR0_RcFlEdg	(SDCR0_RCE*1)	
#define SDCR0_TCE	0x00000080	
#define SDCR0_TrRsEdg	(SDCR0_TCE*0)	
#define SDCR0_TrFlEdg	(SDCR0_TCE*1)	

#define SDCR1_AAF	0x00000001	
                	        	
#define SDCR1_TXE	0x00000002	
#define SDCR1_RXE	0x00000004	
#define SDCR1_RIE	0x00000008	
                	        	
#define SDCR1_TIE	0x00000010	
                	        	
#define SDCR1_AME	0x00000020	
#define SDCR1_TUS	0x00000040	
#define SDCR1_EFrmURn	(SDCR1_TUS*0)	
#define SDCR1_AbortURn	(SDCR1_TUS*1)	
#define SDCR1_RAE	0x00000080	

#define SDCR2_AMV	Fld (8, 0)	

#define SDCR3_BRD	Fld (4, 0)	
#define SDCR4_BRD	Fld (8, 0)	
                	        	
                	        	
#define SDCR3_BdRtDiv(Div)      	 \
                	(((Div) - 16)/16 >> FSize (SDCR4_BRD) << \
                	 FShft (SDCR3_BRD))
#define SDCR4_BdRtDiv(Div)      	 \
                	(((Div) - 16)/16 & FAlnMsk (SDCR4_BRD) << \
                	 FShft (SDCR4_BRD))
                	        	
                	        	
#define SDCR3_CeilBdRtDiv(Div)  	 \
                	(((Div) - 1)/16 >> FSize (SDCR4_BRD) << \
                	 FShft (SDCR3_BRD))
#define SDCR4_CeilBdRtDiv(Div)  	 \
                	(((Div) - 1)/16 & FAlnMsk (SDCR4_BRD) << \
                	 FShft (SDCR4_BRD))
                	        	
                	        	

#define SDDR_DATA	Fld (8, 0)	
#if 0           	        	
#define SDDR_EOF	0x00000100	
#define SDDR_CRE	0x00000200	
#define SDDR_ROR	0x00000400	
#endif 

#define SDSR0_EIF	0x00000001	
#define SDSR0_TUR	0x00000002	
#define SDSR0_RAB	0x00000004	
#define SDSR0_TFS	0x00000008	
                	        	
#define SDSR0_RFS	0x00000010	
                	        	

#define SDSR1_RSY	0x00000001	
#define SDSR1_TBY	0x00000002	
#define SDSR1_RNE	0x00000004	
#define SDSR1_TNF	0x00000008	
#define SDSR1_RTD	0x00000010	
#define SDSR1_EOF	0x00000020	
#define SDSR1_CRE	0x00000040	
#define SDSR1_ROR	0x00000080	



#define Ser2HSCR0	__REG(0x80040060)  
#define Ser2HSCR1	__REG(0x80040064)  
#define Ser2HSDR	__REG(0x8004006C)  
#define Ser2HSSR0	__REG(0x80040074)  
#define Ser2HSSR1	__REG(0x80040078)  
#define Ser2HSCR2	__REG(0x90060028)  

#define HSCR0_ITR	0x00000001	
#define HSCR0_UART	(HSCR0_ITR*0)	
#define HSCR0_HSSP	(HSCR0_ITR*1)	
#define HSCR0_LBM	0x00000002	
#define HSCR0_TUS	0x00000004	
#define HSCR0_EFrmURn	(HSCR0_TUS*0)	
#define HSCR0_AbortURn	(HSCR0_TUS*1)	
#define HSCR0_TXE	0x00000008	
#define HSCR0_RXE	0x00000010	
#define HSCR0_RIE	0x00000020	
                	        	
#define HSCR0_TIE	0x00000040	
                	        	
#define HSCR0_AME	0x00000080	

#define HSCR1_AMV	Fld (8, 0)	

#define HSDR_DATA	Fld (8, 0)	
#if 0           	        	
#define HSDR_EOF	0x00000100	
#define HSDR_CRE	0x00000200	
#define HSDR_ROR	0x00000400	
#endif 

#define HSSR0_EIF	0x00000001	
#define HSSR0_TUR	0x00000002	
#define HSSR0_RAB	0x00000004	
#define HSSR0_TFS	0x00000008	
                	        	
#define HSSR0_RFS	0x00000010	
                	        	
#define HSSR0_FRE	0x00000020	

#define HSSR1_RSY	0x00000001	
#define HSSR1_TBY	0x00000002	
#define HSSR1_RNE	0x00000004	
#define HSSR1_TNF	0x00000008	
#define HSSR1_EOF	0x00000010	
#define HSSR1_CRE	0x00000020	
#define HSSR1_ROR	0x00000040	

#define HSCR2_TXP	0x00040000	
#define HSCR2_TrDataL	(HSCR2_TXP*0)	
                	        	
#define HSCR2_TrDataH	(HSCR2_TXP*1)	
                	        	
#define HSCR2_RXP	0x00080000	
#define HSCR2_RcDataL	(HSCR2_RXP*0)	
                	        	
#define HSCR2_RcDataH	(HSCR2_RXP*1)	
                	        	



#define Ser4MCCR0	__REG(0x80060000)  
#define Ser4MCDR0	__REG(0x80060008)  
#define Ser4MCDR1	__REG(0x8006000C)  
#define Ser4MCDR2	__REG(0x80060010)  
#define Ser4MCSR	__REG(0x80060018)  
#define Ser4MCCR1	__REG(0x90060030)  

#define MCCR0_ASD	Fld (7, 0)	
                	        	
                	        	
                	        	
#define MCCR0_AudSmpDiv(Div)    	 \
                	        	 \
                	((Div)/32 << FShft (MCCR0_ASD))
                	        	
                	        	
#define MCCR0_CeilAudSmpDiv(Div)	 \
                	(((Div) + 31)/32 << FShft (MCCR0_ASD))
                	        	
                	        	
#define MCCR0_TSD	Fld (7, 8)	
                	        	
                	        	
                	        	
#define MCCR0_TcmSmpDiv(Div)    	 \
                	        	 \
                	((Div)/32 << FShft (MCCR0_TSD))
                	        	
                	        	
#define MCCR0_CeilTcmSmpDiv(Div)	 \
                	(((Div) + 31)/32 << FShft (MCCR0_TSD))
                	        	
                	        	
#define MCCR0_MCE	0x00010000	
#define MCCR0_ECS	0x00020000	
#define MCCR0_IntClk	(MCCR0_ECS*0)	
#define MCCR0_ExtClk	(MCCR0_ECS*1)	
#define MCCR0_ADM	0x00040000	
                	        	
#define MCCR0_VldBit	(MCCR0_ADM*0)	
#define MCCR0_SmpCnt	(MCCR0_ADM*1)	
#define MCCR0_TTE	0x00080000	
                	        	
#define MCCR0_TRE	0x00100000	
                	        	
#define MCCR0_ATE	0x00200000	
                	        	
#define MCCR0_ARE	0x00400000	
                	        	
#define MCCR0_LBM	0x00800000	
#define MCCR0_ECP	Fld (2, 24)	
#define MCCR0_ExtClkDiv(Div)    	 \
                	(((Div) - 1) << FShft (MCCR0_ECP))

#define MCDR0_DATA	Fld (12, 4)	
                	        	

#define MCDR1_DATA	Fld (14, 2)	
                	        	

                	        	
                	        	
#define MCDR2_DATA	Fld (16, 0)	
#define MCDR2_RW	0x00010000	
#define MCDR2_Rd	(MCDR2_RW*0)	
#define MCDR2_Wr	(MCDR2_RW*1)	
#define MCDR2_ADD	Fld (4, 17)	

#define MCSR_ATS	0x00000001	
                	        	
#define MCSR_ARS	0x00000002	
                	        	
#define MCSR_TTS	0x00000004	
                	        	
#define MCSR_TRS	0x00000008	
                	        	
#define MCSR_ATU	0x00000010	
#define MCSR_ARO	0x00000020	
#define MCSR_TTU	0x00000040	
#define MCSR_TRO	0x00000080	
#define MCSR_ANF	0x00000100	
                	        	
#define MCSR_ANE	0x00000200	
                	        	
#define MCSR_TNF	0x00000400	
                	        	
#define MCSR_TNE	0x00000800	
                	        	
#define MCSR_CWC	0x00001000	
                	        	
#define MCSR_CRC	0x00002000	
                	        	
#define MCSR_ACE	0x00004000	
#define MCSR_TCE	0x00008000	

#define MCCR1_CFS	0x00100000	
#define MCCR1_F12MHz	(MCCR1_CFS*0)	
                	        	
#define MCCR1_F10MHz	(MCCR1_CFS*1)	
                	        	



#define Ser4SSCR0	__REG(0x80070060)  
#define Ser4SSCR1	__REG(0x80070064)  
#define Ser4SSDR	__REG(0x8007006C)  
#define Ser4SSSR	__REG(0x80070074)  

#define SSCR0_DSS	Fld (4, 0)	
#define SSCR0_DataSize(Size)    	 \
                	(((Size) - 1) << FShft (SSCR0_DSS))
#define SSCR0_FRF	Fld (2, 4)	
#define SSCR0_Motorola	        	 \
                	        	 \
                	(0 << FShft (SSCR0_FRF))
#define SSCR0_TI	        	 \
                	        	 \
                	(1 << FShft (SSCR0_FRF))
#define SSCR0_National	        	 \
                	(2 << FShft (SSCR0_FRF))
#define SSCR0_SSE	0x00000080	
#define SSCR0_SCR	Fld (8, 8)	
                	        	
                	        	
#define SSCR0_SerClkDiv(Div)    	 \
                	(((Div) - 2)/2 << FShft (SSCR0_SCR))
                	        	
                	        	
#define SSCR0_CeilSerClkDiv(Div)	 \
                	(((Div) - 1)/2 << FShft (SSCR0_SCR))
                	        	
                	        	

#define SSCR1_RIE	0x00000001	
                	        	
#define SSCR1_TIE	0x00000002	
                	        	
#define SSCR1_LBM	0x00000004	
#define SSCR1_SPO	0x00000008	
#define SSCR1_SClkIactL	(SSCR1_SPO*0)	
#define SSCR1_SClkIactH	(SSCR1_SPO*1)	
#define SSCR1_SP	0x00000010	
#define SSCR1_SClk1P	(SSCR1_SP*0)	
                	        	
#define SSCR1_SClk1_2P	(SSCR1_SP*1)	
                	        	
#define SSCR1_ECS	0x00000020	
#define SSCR1_IntClk	(SSCR1_ECS*0)	
#define SSCR1_ExtClk	(SSCR1_ECS*1)	

#define SSDR_DATA	Fld (16, 0)	

#define SSSR_TNF	0x00000002	
#define SSSR_RNE	0x00000004	
#define SSSR_BSY	0x00000008	
#define SSSR_TFS	0x00000010	
                	        	
#define SSSR_RFS	0x00000020	
                	        	
#define SSSR_ROR	0x00000040	



#define OSMR0  		__REG(0x90000000)  
#define OSMR1  		__REG(0x90000004)  
#define OSMR2  		__REG(0x90000008)  
#define OSMR3  		__REG(0x9000000c)  
#define OSCR   	__REG(0x90000010)  
#define OSSR   	__REG(0x90000014	)  
#define OWER   	__REG(0x90000018	)  
#define OIER   	__REG(0x9000001C	)  

#define OSSR_M(Nb)	        	 \
                	(0x00000001 << (Nb))
#define OSSR_M0 	OSSR_M (0)	
#define OSSR_M1 	OSSR_M (1)	
#define OSSR_M2 	OSSR_M (2)	
#define OSSR_M3 	OSSR_M (3)	

#define OWER_WME	0x00000001	
                	        	

#define OIER_E(Nb)	        	 \
                	(0x00000001 << (Nb))
#define OIER_E0 	OIER_E (0)	
#define OIER_E1 	OIER_E (1)	
#define OIER_E2 	OIER_E (2)	
#define OIER_E3 	OIER_E (3)	



#define RTAR		__REG(0x90010000)  
#define RCNR		__REG(0x90010004)  
#define RTTR		__REG(0x90010008)  
#define RTSR		__REG(0x90010010)  

#define RTTR_C  	Fld (16, 0)	
#define RTTR_D  	Fld (10, 16)	
                	        	
                	        	
                	        	
                	        	

#define RTSR_AL 	0x00000001	
#define RTSR_HZ 	0x00000002	
#define RTSR_ALE	0x00000004	
#define RTSR_HZE	0x00000008	



#define PMCR		__REG(0x90020000)  
#define PSSR		__REG(0x90020004)  
#define PSPR		__REG(0x90020008)  
#define PWER		__REG(0x9002000C)  
#define PCFR		__REG(0x90020010)  
#define PPCR		__REG(0x90020014)  
#define PGSR		__REG(0x90020018)  
#define POSR		__REG(0x9002001C)  

#define PMCR_SF 	0x00000001	

#define PSSR_SS 	0x00000001	
#define PSSR_BFS	0x00000002	
                	        	
#define PSSR_VFS	0x00000004	
#define PSSR_DH 	0x00000008	
#define PSSR_PH 	0x00000010	

#define PWER_GPIO(Nb)	GPIO_GPIO (Nb)	
#define PWER_GPIO0	PWER_GPIO (0)	
#define PWER_GPIO1	PWER_GPIO (1)	
#define PWER_GPIO2	PWER_GPIO (2)	
#define PWER_GPIO3	PWER_GPIO (3)	
#define PWER_GPIO4	PWER_GPIO (4)	
#define PWER_GPIO5	PWER_GPIO (5)	
#define PWER_GPIO6	PWER_GPIO (6)	
#define PWER_GPIO7	PWER_GPIO (7)	
#define PWER_GPIO8	PWER_GPIO (8)	
#define PWER_GPIO9	PWER_GPIO (9)	
#define PWER_GPIO10	PWER_GPIO (10)	
#define PWER_GPIO11	PWER_GPIO (11)	
#define PWER_GPIO12	PWER_GPIO (12)	
#define PWER_GPIO13	PWER_GPIO (13)	
#define PWER_GPIO14	PWER_GPIO (14)	
#define PWER_GPIO15	PWER_GPIO (15)	
#define PWER_GPIO16	PWER_GPIO (16)	
#define PWER_GPIO17	PWER_GPIO (17)	
#define PWER_GPIO18	PWER_GPIO (18)	
#define PWER_GPIO19	PWER_GPIO (19)	
#define PWER_GPIO20	PWER_GPIO (20)	
#define PWER_GPIO21	PWER_GPIO (21)	
#define PWER_GPIO22	PWER_GPIO (22)	
#define PWER_GPIO23	PWER_GPIO (23)	
#define PWER_GPIO24	PWER_GPIO (24)	
#define PWER_GPIO25	PWER_GPIO (25)	
#define PWER_GPIO26	PWER_GPIO (26)	
#define PWER_GPIO27	PWER_GPIO (27)	
#define PWER_RTC	0x80000000	

#define PCFR_OPDE	0x00000001	
#define PCFR_ClkRun	(PCFR_OPDE*0)	
#define PCFR_ClkStp	(PCFR_OPDE*1)	
#define PCFR_FP 	0x00000002	
#define PCFR_PCMCIANeg	(PCFR_FP*0)	
#define PCFR_PCMCIAFlt	(PCFR_FP*1)	
#define PCFR_FS 	0x00000004	
#define PCFR_StMemNeg	(PCFR_FS*0)	
#define PCFR_StMemFlt	(PCFR_FS*1)	
#define PCFR_FO 	0x00000008	
                	        	

#define PPCR_CCF	Fld (5, 0)	
#define PPCR_Fx16	        	 \
                	(0x00 << FShft (PPCR_CCF))
#define PPCR_Fx20	        	 \
                	(0x01 << FShft (PPCR_CCF))
#define PPCR_Fx24	        	 \
                	(0x02 << FShft (PPCR_CCF))
#define PPCR_Fx28	        	 \
                	(0x03 << FShft (PPCR_CCF))
#define PPCR_Fx32	        	 \
                	(0x04 << FShft (PPCR_CCF))
#define PPCR_Fx36	        	 \
                	(0x05 << FShft (PPCR_CCF))
#define PPCR_Fx40	        	 \
                	(0x06 << FShft (PPCR_CCF))
#define PPCR_Fx44	        	 \
                	(0x07 << FShft (PPCR_CCF))
#define PPCR_Fx48	        	 \
                	(0x08 << FShft (PPCR_CCF))
#define PPCR_Fx52	        	 \
                	(0x09 << FShft (PPCR_CCF))
#define PPCR_Fx56	        	 \
                	(0x0A << FShft (PPCR_CCF))
#define PPCR_Fx60	        	 \
                	(0x0B << FShft (PPCR_CCF))
#define PPCR_Fx64	        	 \
                	(0x0C << FShft (PPCR_CCF))
#define PPCR_Fx68	        	 \
                	(0x0D << FShft (PPCR_CCF))
#define PPCR_Fx72	        	 \
                	(0x0E << FShft (PPCR_CCF))
#define PPCR_Fx76	        	 \
                	(0x0F << FShft (PPCR_CCF))
                	        	
#define PPCR_F59_0MHz	PPCR_Fx16	
#define PPCR_F73_7MHz	PPCR_Fx20	
#define PPCR_F88_5MHz	PPCR_Fx24	
#define PPCR_F103_2MHz	PPCR_Fx28	
#define PPCR_F118_0MHz	PPCR_Fx32	
#define PPCR_F132_7MHz	PPCR_Fx36	
#define PPCR_F147_5MHz	PPCR_Fx40	
#define PPCR_F162_2MHz	PPCR_Fx44	
#define PPCR_F176_9MHz	PPCR_Fx48	
#define PPCR_F191_7MHz	PPCR_Fx52	
#define PPCR_F206_4MHz	PPCR_Fx56	
#define PPCR_F221_2MHz	PPCR_Fx60	
#define PPCR_F239_6MHz	PPCR_Fx64	
#define PPCR_F250_7MHz	PPCR_Fx68	
#define PPCR_F265_4MHz	PPCR_Fx72	
#define PPCR_F280_2MHz	PPCR_Fx76	
                	        	
#define PPCR_F57_3MHz	PPCR_Fx16	
#define PPCR_F71_6MHz	PPCR_Fx20	
#define PPCR_F85_9MHz	PPCR_Fx24	
#define PPCR_F100_2MHz	PPCR_Fx28	
#define PPCR_F114_5MHz	PPCR_Fx32	
#define PPCR_F128_9MHz	PPCR_Fx36	
#define PPCR_F143_2MHz	PPCR_Fx40	
#define PPCR_F157_5MHz	PPCR_Fx44	
#define PPCR_F171_8MHz	PPCR_Fx48	
#define PPCR_F186_1MHz	PPCR_Fx52	
#define PPCR_F200_5MHz	PPCR_Fx56	
#define PPCR_F214_8MHz	PPCR_Fx60	
#define PPCR_F229_1MHz	PPCR_Fx64	
#define PPCR_F243_4MHz	PPCR_Fx68	
#define PPCR_F257_7MHz	PPCR_Fx72	
#define PPCR_F272_0MHz	PPCR_Fx76	

#define POSR_OOK	0x00000001	



#define RSRR		__REG(0x90030000)  
#define RCSR		__REG(0x90030004)  

#define RSRR_SWR	0x00000001	

#define RCSR_HWR	0x00000001	
#define RCSR_SWR	0x00000002	
#define RCSR_WDR	0x00000004	
#define RCSR_SMR	0x00000008	



#define TUCR		__REG(0x90030008)  

#define TUCR_TIC	0x00000040	
#define TUCR_TTST	0x00000080	
#define TUCR_RCRC	0x00000100	
                	        	
#define TUCR_PMD	0x00000200	
#define TUCR_MR 	0x00000400	
#define TUCR_NoMB	(TUCR_MR*0)	
#define TUCR_MBGPIO	(TUCR_MR*1)	
                	        	
#define TUCR_CTB	Fld (3, 20)	
#define TUCR_FDC	0x00800000	
#define TUCR_FMC	0x01000000	
#define TUCR_TMC	0x02000000	
#define TUCR_DPS	0x04000000	
#define TUCR_TSEL	Fld (3, 29)	
#define TUCR_32_768kHz	        	 \
                	(0 << FShft (TUCR_TSEL))
#define TUCR_3_6864MHz	        	 \
                	(1 << FShft (TUCR_TSEL))
#define TUCR_VDD	        	 \
                	(2 << FShft (TUCR_TSEL))
#define TUCR_96MHzPLL	        	 \
                	(3 << FShft (TUCR_TSEL))
#define TUCR_Clock	        	 \
                	        	 \
                	(4 << FShft (TUCR_TSEL))
#define TUCR_3_6864MHzA	        	 \
                	        	 \
                	(5 << FShft (TUCR_TSEL))
#define TUCR_MainPLL	        	 \
                	(6 << FShft (TUCR_TSEL))
#define TUCR_VDDL	        	 \
                	(7 << FShft (TUCR_TSEL))


/*
 * General-Purpose Input/Output (GPIO) control registers
 *
 * Registers
 *    GPLR      	General-Purpose Input/Output (GPIO) Pin Level
 *              	Register (read).
 *    GPDR      	General-Purpose Input/Output (GPIO) Pin Direction
 *              	Register (read/write).
 *    GPSR      	General-Purpose Input/Output (GPIO) Pin output Set
 *              	Register (write).
 *    GPCR      	General-Purpose Input/Output (GPIO) Pin output Clear
 *              	Register (write).
 *    GRER      	General-Purpose Input/Output (GPIO) Rising-Edge
 *              	detect Register (read/write).
 *    GFER      	General-Purpose Input/Output (GPIO) Falling-Edge
 *              	detect Register (read/write).
 *    GEDR      	General-Purpose Input/Output (GPIO) Edge Detect
 *              	status Register (read/write).
 *    GAFR      	General-Purpose Input/Output (GPIO) Alternate
 *              	Function Register (read/write).
 *
 * Clock
 *    fcpu, Tcpu	Frequency, period of the CPU core clock (CCLK).
 */

#define GPLR		__REG(0x90040000)  
#define GPDR		__REG(0x90040004)  
#define GPSR		__REG(0x90040008)  
#define GPCR		__REG(0x9004000C)  
#define GRER		__REG(0x90040010)  
#define GFER		__REG(0x90040014)  
#define GEDR		__REG(0x90040018)  
#define GAFR		__REG(0x9004001C)  

#define GPIO_MIN	(0)
#define GPIO_MAX	(27)

#define GPIO_GPIO(Nb)	        	 \
                	(0x00000001 << (Nb))
#define GPIO_GPIO0	GPIO_GPIO (0)	
#define GPIO_GPIO1	GPIO_GPIO (1)	
#define GPIO_GPIO2	GPIO_GPIO (2)	
#define GPIO_GPIO3	GPIO_GPIO (3)	
#define GPIO_GPIO4	GPIO_GPIO (4)	
#define GPIO_GPIO5	GPIO_GPIO (5)	
#define GPIO_GPIO6	GPIO_GPIO (6)	
#define GPIO_GPIO7	GPIO_GPIO (7)	
#define GPIO_GPIO8	GPIO_GPIO (8)	
#define GPIO_GPIO9	GPIO_GPIO (9)	
#define GPIO_GPIO10	GPIO_GPIO (10)	
#define GPIO_GPIO11	GPIO_GPIO (11)	
#define GPIO_GPIO12	GPIO_GPIO (12)	
#define GPIO_GPIO13	GPIO_GPIO (13)	
#define GPIO_GPIO14	GPIO_GPIO (14)	
#define GPIO_GPIO15	GPIO_GPIO (15)	
#define GPIO_GPIO16	GPIO_GPIO (16)	
#define GPIO_GPIO17	GPIO_GPIO (17)	
#define GPIO_GPIO18	GPIO_GPIO (18)	
#define GPIO_GPIO19	GPIO_GPIO (19)	
#define GPIO_GPIO20	GPIO_GPIO (20)	
#define GPIO_GPIO21	GPIO_GPIO (21)	
#define GPIO_GPIO22	GPIO_GPIO (22)	
#define GPIO_GPIO23	GPIO_GPIO (23)	
#define GPIO_GPIO24	GPIO_GPIO (24)	
#define GPIO_GPIO25	GPIO_GPIO (25)	
#define GPIO_GPIO26	GPIO_GPIO (26)	
#define GPIO_GPIO27	GPIO_GPIO (27)	

#define GPIO_LDD(Nb)	        	 \
                	GPIO_GPIO ((Nb) - 6)
#define GPIO_LDD8	GPIO_LDD (8)	
#define GPIO_LDD9	GPIO_LDD (9)	
#define GPIO_LDD10	GPIO_LDD (10)	
#define GPIO_LDD11	GPIO_LDD (11)	
#define GPIO_LDD12	GPIO_LDD (12)	
#define GPIO_LDD13	GPIO_LDD (13)	
#define GPIO_LDD14	GPIO_LDD (14)	
#define GPIO_LDD15	GPIO_LDD (15)	
                	        	
#define GPIO_SSP_TXD	GPIO_GPIO (10)	
#define GPIO_SSP_RXD	GPIO_GPIO (11)	
#define GPIO_SSP_SCLK	GPIO_GPIO (12)	
#define GPIO_SSP_SFRM	GPIO_GPIO (13)	
                	        	
#define GPIO_UART_TXD	GPIO_GPIO (14)	
#define GPIO_UART_RXD	GPIO_GPIO (15)	
#define GPIO_SDLC_SCLK	GPIO_GPIO (16)	
#define GPIO_SDLC_AAF	GPIO_GPIO (17)	
#define GPIO_UART_SCLK1	GPIO_GPIO (18)	
                	        	
#define GPIO_SSP_CLK	GPIO_GPIO (19)	
                	        	
#define GPIO_UART_SCLK3	GPIO_GPIO (20)	
                	        	
#define GPIO_MCP_CLK	GPIO_GPIO (21)	
                	        	
#define GPIO_TIC_ACK	GPIO_GPIO (21)	
#define GPIO_MBGNT	GPIO_GPIO (21)	
#define GPIO_TREQA	GPIO_GPIO (22)	
#define GPIO_MBREQ	GPIO_GPIO (22)	
#define GPIO_TREQB	GPIO_GPIO (23)	
#define GPIO_1Hz	GPIO_GPIO (25)	
#define GPIO_RCLK	GPIO_GPIO (26)	
#define GPIO_32_768kHz	GPIO_GPIO (27)	

#define GPDR_In 	0       	
#define GPDR_Out	1       	



#define ICIP		__REG(0x90050000)  
#define ICMR		__REG(0x90050004)  
#define ICLR		__REG(0x90050008)  
#define ICCR		__REG(0x9005000C)  
#define ICFP		__REG(0x90050010)  
#define ICPR		__REG(0x90050020)  

#define IC_GPIO(Nb)	        	 \
                	(0x00000001 << (Nb))
#define IC_GPIO0	IC_GPIO (0)	
#define IC_GPIO1	IC_GPIO (1)	
#define IC_GPIO2	IC_GPIO (2)	
#define IC_GPIO3	IC_GPIO (3)	
#define IC_GPIO4	IC_GPIO (4)	
#define IC_GPIO5	IC_GPIO (5)	
#define IC_GPIO6	IC_GPIO (6)	
#define IC_GPIO7	IC_GPIO (7)	
#define IC_GPIO8	IC_GPIO (8)	
#define IC_GPIO9	IC_GPIO (9)	
#define IC_GPIO10	IC_GPIO (10)	
#define IC_GPIO11_27	0x00000800	
#define IC_LCD  	0x00001000	
#define IC_Ser0UDC	0x00002000	
#define IC_Ser1SDLC	0x00004000	
#define IC_Ser1UART	0x00008000	
#define IC_Ser2ICP	0x00010000	
#define IC_Ser3UART	0x00020000	
#define IC_Ser4MCP	0x00040000	
#define IC_Ser4SSP	0x00080000	
#define IC_DMA(Nb)	        	 \
                	(0x00100000 << (Nb))
#define IC_DMA0 	IC_DMA (0)	
#define IC_DMA1 	IC_DMA (1)	
#define IC_DMA2 	IC_DMA (2)	
#define IC_DMA3 	IC_DMA (3)	
#define IC_DMA4 	IC_DMA (4)	
#define IC_DMA5 	IC_DMA (5)	
#define IC_OST(Nb)	        	 \
                	(0x04000000 << (Nb))
#define IC_OST0 	IC_OST (0)	
#define IC_OST1 	IC_OST (1)	
#define IC_OST2 	IC_OST (2)	
#define IC_OST3 	IC_OST (3)	
#define IC_RTC1Hz	0x40000000	
#define IC_RTCAlrm	0x80000000	

#define ICLR_IRQ	0       	
#define ICLR_FIQ	1       	

#define ICCR_DIM	0x00000001	
                	        	
#define ICCR_IdleAllInt	(ICCR_DIM*0)	
                	        	
#define ICCR_IdleMskInt	(ICCR_DIM*1)	
                	        	



#define PPDR		__REG(0x90060000)  
#define PPSR		__REG(0x90060004)  
#define PPAR		__REG(0x90060008)  
#define PSDR		__REG(0x9006000C)  
#define PPFR		__REG(0x90060010)  

#define PPC_LDD(Nb)	        	 \
                	(0x00000001 << (Nb))
#define PPC_LDD0	PPC_LDD (0)	
#define PPC_LDD1	PPC_LDD (1)	
#define PPC_LDD2	PPC_LDD (2)	
#define PPC_LDD3	PPC_LDD (3)	
#define PPC_LDD4	PPC_LDD (4)	
#define PPC_LDD5	PPC_LDD (5)	
#define PPC_LDD6	PPC_LDD (6)	
#define PPC_LDD7	PPC_LDD (7)	
#define PPC_L_PCLK	0x00000100	
#define PPC_L_LCLK	0x00000200	
#define PPC_L_FCLK	0x00000400	
#define PPC_L_BIAS	0x00000800	
                	        	
#define PPC_TXD1	0x00001000	
#define PPC_RXD1	0x00002000	
                	        	
#define PPC_TXD2	0x00004000	
#define PPC_RXD2	0x00008000	
                	        	
#define PPC_TXD3	0x00010000	
#define PPC_RXD3	0x00020000	
                	        	
#define PPC_TXD4	0x00040000	
#define PPC_RXD4	0x00080000	
#define PPC_SCLK	0x00100000	
#define PPC_SFRM	0x00200000	

#define PPDR_In 	0       	
#define PPDR_Out	1       	

                	        	
#define PPAR_UPR	0x00001000	
#define PPAR_UARTTR	(PPAR_UPR*0)	
#define PPAR_UARTGPIO	(PPAR_UPR*1)	
                	        	
#define PPAR_SPR	0x00040000	
#define PPAR_SSPTRSS	(PPAR_SPR*0)	
                	        	
#define PPAR_SSPGPIO	(PPAR_SPR*1)	

#define PSDR_OutL	0       	
#define PSDR_Flt	1       	

#define PPFR_LCD	0x00000001	
#define PPFR_SP1TX	0x00001000	
#define PPFR_SP1RX	0x00002000	
#define PPFR_SP2TX	0x00004000	
#define PPFR_SP2RX	0x00008000	
#define PPFR_SP3TX	0x00010000	
#define PPFR_SP3RX	0x00020000	
#define PPFR_SP4	0x00040000	
#define PPFR_PerEn	0       	
#define PPFR_PPCEn	1       	



#define MDCNFG		__REG(0xA0000000)  
#define MDCAS0		__REG(0xA0000004)  
#define MDCAS1		__REG(0xA0000008)  
#define MDCAS2		__REG(0xA000000c)  

#define MDCNFG_DE(Nb)	        	 \
                	(0x00000001 << (Nb))
#define MDCNFG_DE0	MDCNFG_DE (0)	
#define MDCNFG_DE1	MDCNFG_DE (1)	
#define MDCNFG_DE2	MDCNFG_DE (2)	
#define MDCNFG_DE3	MDCNFG_DE (3)	
#define MDCNFG_DRAC	Fld (2, 4)	
#define MDCNFG_RowAdd(Add)      	 \
                	(((Add) - 9) << FShft (MDCNFG_DRAC))
#define MDCNFG_CDB2	0x00000040	
                	        	
#define MDCNFG_TRP	Fld (4, 7)	
#define MDCNFG_PrChrg(Tcpu)     	 \
                	(((Tcpu) - 2)/2 << FShft (MDCNFG_TRP))
#define MDCNFG_CeilPrChrg(Tcpu) 	 \
                	(((Tcpu) - 1)/2 << FShft (MDCNFG_TRP))
#define MDCNFG_TRASR	Fld (4, 11)	
#define MDCNFG_Ref(Tcpu)        	 \
                	(((Tcpu) - 2)/2 << FShft (MDCNFG_TRASR))
#define MDCNFG_CeilRef(Tcpu)    	 \
                	(((Tcpu) - 1)/2 << FShft (MDCNFG_TRASR))
#define MDCNFG_TDL	Fld (2, 15)	
#define MDCNFG_DataLtch(Tcpu)   	 \
                	((Tcpu) << FShft (MDCNFG_TDL))
#define MDCNFG_DRI	Fld (15, 17)	
                	        	
#define MDCNFG_RefInt(Tcpu)     	 \
                	        	 \
                	((Tcpu)/8 << FShft (MDCNFG_DRI))

#define MDCNFG_SA1110_DE0	0x00000001	
#define MDCNFG_SA1110_DE1	0x00000002 	
#define MDCNFG_SA1110_DTIM0	0x00000004	
#define MDCNFG_SA1110_DWID0	0x00000008	
#define MDCNFG_SA1110_DRAC0	Fld(3, 4)	
                	        		
#define MDCNFG_SA1110_CDB20	0x00000080	
#define MDCNFG_SA1110_TRP0	Fld(3, 8)	
#define MDCNFG_SA1110_TDL0	Fld(2, 12)	
                	        		
#define MDCNFG_SA1110_TWR0	Fld(2, 14)	
#define MDCNFG_SA1110_DE2	0x00010000	
#define MDCNFG_SA1110_DE3	0x00020000 	
#define MDCNFG_SA1110_DTIM2	0x00040000	
#define MDCNFG_SA1110_DWID2	0x00080000	
#define MDCNFG_SA1110_DRAC2	Fld(3, 20)	
                	        		
#define MDCNFG_SA1110_CDB22	0x00800000	
#define MDCNFG_SA1110_TRP2	Fld(3, 24)	
#define MDCNFG_SA1110_TDL2	Fld(2, 28)	
                	        		
#define MDCNFG_SA1110_TWR2	Fld(2, 30)	



#define MSC0		__REG(0xa0000010)  
#define MSC1		__REG(0xa0000014)  
#define MSC2		__REG(0xa000002c)  

#define MSC_Bnk(Nb)	        	 \
                	Fld (16, ((Nb) Modulo 2)*16)
#define MSC0_Bnk0	MSC_Bnk (0)	
#define MSC0_Bnk1	MSC_Bnk (1)	
#define MSC1_Bnk2	MSC_Bnk (2)	
#define MSC1_Bnk3	MSC_Bnk (3)	

#define MSC_RT  	Fld (2, 0)	
#define MSC_NonBrst	        	 \
                	(0 << FShft (MSC_RT))
#define MSC_SRAM	        	 \
                	(1 << FShft (MSC_RT))
#define MSC_Brst4	        	 \
                	(2 << FShft (MSC_RT))
#define MSC_Brst8	        	 \
                	(3 << FShft (MSC_RT))
#define MSC_RBW 	0x0004  	
#define MSC_32BitStMem	(MSC_RBW*0)	
#define MSC_16BitStMem	(MSC_RBW*1)	
#define MSC_RDF 	Fld (5, 3)	
                	        	
#define MSC_1stRdAcc(Tcpu)      	 \
                	        	 \
                	((((Tcpu) - 3)/2) << FShft (MSC_RDF))
#define MSC_Ceil1stRdAcc(Tcpu)  	 \
                	((((Tcpu) - 2)/2) << FShft (MSC_RDF))
#define MSC_RdAcc(Tcpu)	        	 \
                	        	 \
                	((((Tcpu) - 2)/2) << FShft (MSC_RDF))
#define MSC_CeilRdAcc(Tcpu)     	 \
                	((((Tcpu) - 1)/2) << FShft (MSC_RDF))
#define MSC_RDN 	Fld (5, 8)	
                	        	
#define MSC_NxtRdAcc(Tcpu)      	 \
                	        	 \
                	((((Tcpu) - 2)/2) << FShft (MSC_RDN))
#define MSC_CeilNxtRdAcc(Tcpu)  	 \
                	((((Tcpu) - 1)/2) << FShft (MSC_RDN))
#define MSC_WrAcc(Tcpu)	        	 \
                	        	 \
                	((((Tcpu) - 2)/2) << FShft (MSC_RDN))
#define MSC_CeilWrAcc(Tcpu)     	 \
                	((((Tcpu) - 1)/2) << FShft (MSC_RDN))
#define MSC_RRR 	Fld (3, 13)	
                	        	
#define MSC_Rec(Tcpu)	        	 \
                	(((Tcpu)/4) << FShft (MSC_RRR))
#define MSC_CeilRec(Tcpu)       	 \
                	((((Tcpu) + 3)/4) << FShft (MSC_RRR))



                	        	
#define MECR		__REG(0xA0000018)  

#define MECR_PCMCIA(Nb)	        	 \
                	Fld (15, (Nb)*16)
#define MECR_PCMCIA0	MECR_PCMCIA (0)	
#define MECR_PCMCIA1	MECR_PCMCIA (1)	

#define MECR_BSIO	Fld (5, 0)	
#define MECR_IOClk(Tcpu)        	 \
                	((((Tcpu) - 2)/2) << FShft (MECR_BSIO))
#define MECR_CeilIOClk(Tcpu)    	 \
                	((((Tcpu) - 1)/2) << FShft (MECR_BSIO))
#define MECR_BSA	Fld (5, 5)	
                	        	
#define MECR_AttrClk(Tcpu)      	 \
                	((((Tcpu) - 2)/2) << FShft (MECR_BSA))
#define MECR_CeilAttrClk(Tcpu)  	 \
                	((((Tcpu) - 1)/2) << FShft (MECR_BSA))
#define MECR_BSM	Fld (5, 10)	
#define MECR_MemClk(Tcpu)       	 \
                	((((Tcpu) - 2)/2) << FShft (MECR_BSM))
#define MECR_CeilMemClk(Tcpu)   	 \
                	((((Tcpu) - 1)/2) << FShft (MECR_BSM))


#define MDREFR		__REG(0xA000001C)

#define MDREFR_TRASR		Fld (4, 0)
#define MDREFR_DRI		Fld (12, 4)
#define MDREFR_E0PIN		(1 << 16)
#define MDREFR_K0RUN		(1 << 17)
#define MDREFR_K0DB2		(1 << 18)
#define MDREFR_E1PIN		(1 << 20)
#define MDREFR_K1RUN		(1 << 21)
#define MDREFR_K1DB2		(1 << 22)
#define MDREFR_K2RUN		(1 << 25)
#define MDREFR_K2DB2		(1 << 26)
#define MDREFR_EAPD		(1 << 28)
#define MDREFR_KAPD		(1 << 29)
#define MDREFR_SLFRSH		(1 << 31)


#define DMA_SIZE	(6 * 0x20)
#define DMA_PHYS	0xb0000000


/*
 * Liquid Crystal Display (LCD) control registers
 *
 * Registers
 *    LCCR0     	Liquid Crystal Display (LCD) Control Register 0
 *              	(read/write).
 *              	[Bits LDM, BAM, and ERM are only implemented in
 *              	versions 2.0 (rev. = 8) and higher of the StrongARM
 *              	SA-1100.]
 *    LCSR      	Liquid Crystal Display (LCD) Status Register
 *              	(read/write).
 *              	[Bit LDD can be only read in versions 1.0 (rev. = 1)
 *              	and 1.1 (rev. = 2) of the StrongARM SA-1100, it can be
 *              	read and written (cleared) in versions 2.0 (rev. = 8)
 *              	and higher.]
 *    DBAR1     	Liquid Crystal Display (LCD) Direct Memory Access
 *              	(DMA) Base Address Register channel 1 (read/write).
 *    DCAR1     	Liquid Crystal Display (LCD) Direct Memory Access
 *              	(DMA) Current Address Register channel 1 (read).
 *    DBAR2     	Liquid Crystal Display (LCD) Direct Memory Access
 *              	(DMA) Base Address Register channel 2 (read/write).
 *    DCAR2     	Liquid Crystal Display (LCD) Direct Memory Access
 *              	(DMA) Current Address Register channel 2 (read).
 *    LCCR1     	Liquid Crystal Display (LCD) Control Register 1
 *              	(read/write).
 *              	[The LCCR1 register can be only written in
 *              	versions 1.0 (rev. = 1) and 1.1 (rev. = 2) of the
 *              	StrongARM SA-1100, it can be written and read in
 *              	versions 2.0 (rev. = 8) and higher.]
 *    LCCR2     	Liquid Crystal Display (LCD) Control Register 2
 *              	(read/write).
 *              	[The LCCR1 register can be only written in
 *              	versions 1.0 (rev. = 1) and 1.1 (rev. = 2) of the
 *              	StrongARM SA-1100, it can be written and read in
 *              	versions 2.0 (rev. = 8) and higher.]
 *    LCCR3     	Liquid Crystal Display (LCD) Control Register 3
 *              	(read/write).
 *              	[The LCCR1 register can be only written in
 *              	versions 1.0 (rev. = 1) and 1.1 (rev. = 2) of the
 *              	StrongARM SA-1100, it can be written and read in
 *              	versions 2.0 (rev. = 8) and higher. Bit PCP is only
 *              	implemented in versions 2.0 (rev. = 8) and higher of
 *              	the StrongARM SA-1100.]
 *
 * Clocks
 *    fcpu, Tcpu	Frequency, period of the CPU core clock (CCLK).
 *    fmem, Tmem	Frequency, period of the memory clock (fmem = fcpu/2).
 *    fpix, Tpix	Frequency, period of the pixel clock.
 *    fln, Tln  	Frequency, period of the line clock.
 *    fac, Tac  	Frequency, period of the AC bias clock.
 */

#define LCD_PEntrySp	2       	
#define LCD_4BitPSp	        	 \
                	        	 \
                	(16*LCD_PEntrySp)
#define LCD_8BitPSp	        	 \
                	        	 \
                	(256*LCD_PEntrySp)
#define LCD_12_16BitPSp	        	 \
                	        	 \
                	(16*LCD_PEntrySp)

#define LCD_PGrey	Fld (4, 0)	
#define LCD_PBlue	Fld (4, 0)	
#define LCD_PGreen	Fld (4, 4)	
#define LCD_PRed	Fld (4, 8)	
#define LCD_PBS 	Fld (2, 12)	
#define LCD_4Bit	        	 \
                	(0 << FShft (LCD_PBS))
#define LCD_8Bit	        	 \
                	(1 << FShft (LCD_PBS))
#define LCD_12_16Bit	        	 \
                	(2 << FShft (LCD_PBS))

#define LCD_Int0_0	0x0     	
#define LCD_Int11_1	0x1     	
#define LCD_Int20_0	0x2     	
#define LCD_Int26_7	0x3     	
#define LCD_Int33_3	0x4     	
#define LCD_Int40_0	0x5     	
#define LCD_Int44_4	0x6     	
#define LCD_Int50_0	0x7     	
#define LCD_Int55_6	0x8     	
#define LCD_Int60_0	0x9     	
#define LCD_Int66_7	0xA     	
#define LCD_Int73_3	0xB     	
#define LCD_Int80_0	0xC     	
#define LCD_Int88_9	0xD     	
#define LCD_Int100_0	0xE     	
#define LCD_Int100_0A	0xF     	
                	        	

#define LCCR0_LEN	0x00000001	
#define LCCR0_CMS	0x00000002	
#define LCCR0_Color	(LCCR0_CMS*0)	
#define LCCR0_Mono	(LCCR0_CMS*1)	
#define LCCR0_SDS	0x00000004	
                	        	
#define LCCR0_Sngl	(LCCR0_SDS*0)	
#define LCCR0_Dual	(LCCR0_SDS*1)	
#define LCCR0_LDM	0x00000008	
                	        	
#define LCCR0_BAM	0x00000010	
                	        	
#define LCCR0_ERM	0x00000020	
                	        	
                	        	
#define LCCR0_PAS	0x00000080	
#define LCCR0_Pas	(LCCR0_PAS*0)	
#define LCCR0_Act	(LCCR0_PAS*1)	
#define LCCR0_BLE	0x00000100	
#define LCCR0_LtlEnd	(LCCR0_BLE*0)	
#define LCCR0_BigEnd	(LCCR0_BLE*1)	
#define LCCR0_DPD	0x00000200	
                	        	
#define LCCR0_4PixMono	(LCCR0_DPD*0)	
                	        	
#define LCCR0_8PixMono	(LCCR0_DPD*1)	
                	        	
#define LCCR0_PDD	Fld (8, 12)	
                	        	
#define LCCR0_DMADel(Tcpu)      	 \
                	        	 \
                	((Tcpu)/2 << FShft (LCCR0_PDD))

#define LCSR_LDD	0x00000001	
#define LCSR_BAU	0x00000002	
#define LCSR_BER	0x00000004	
#define LCSR_ABC	0x00000008	
#define LCSR_IOL	0x00000010	
                	        	
#define LCSR_IUL	0x00000020	
                	        	
#define LCSR_IOU	0x00000040	
                	        	
#define LCSR_IUU	0x00000080	
                	        	
#define LCSR_OOL	0x00000100	
                	        	
#define LCSR_OUL	0x00000200	
                	        	
#define LCSR_OOU	0x00000400	
                	        	
#define LCSR_OUU	0x00000800	
                	        	

#define LCCR1_PPL	Fld (6, 4)	
#define LCCR1_DisWdth(Pixel)    	 \
                	(((Pixel) - 16)/16 << FShft (LCCR1_PPL))
#define LCCR1_HSW	Fld (6, 10)	
                	        	
#define LCCR1_HorSnchWdth(Tpix) 	 \
                	        	 \
                	(((Tpix) - 1) << FShft (LCCR1_HSW))
#define LCCR1_ELW	Fld (8, 16)	
                	        	
#define LCCR1_EndLnDel(Tpix)    	 \
                	        	 \
                	(((Tpix) - 1) << FShft (LCCR1_ELW))
#define LCCR1_BLW	Fld (8, 24)	
                	        	
#define LCCR1_BegLnDel(Tpix)    	 \
                	        	 \
                	(((Tpix) - 1) << FShft (LCCR1_BLW))

#define LCCR2_LPP	Fld (10, 0)	
#define LCCR2_DisHght(Line)     	 \
                	(((Line) - 1) << FShft (LCCR2_LPP))
#define LCCR2_VSW	Fld (6, 10)	
                	        	
#define LCCR2_VrtSnchWdth(Tln)  	 \
                	        	 \
                	(((Tln) - 1) << FShft (LCCR2_VSW))
#define LCCR2_EFW	Fld (8, 16)	
                	        	
#define LCCR2_EndFrmDel(Tln)    	 \
                	        	 \
                	((Tln) << FShft (LCCR2_EFW))
#define LCCR2_BFW	Fld (8, 24)	
                	        	
#define LCCR2_BegFrmDel(Tln)    	 \
                	        	 \
                	((Tln) << FShft (LCCR2_BFW))

#define LCCR3_PCD	Fld (8, 0)	
                	        	
                	        	
                	        	
#define LCCR3_PixClkDiv(Div)    	 \
                	(((Div) - 4)/2 << FShft (LCCR3_PCD))
                	        	
                	        	
#define LCCR3_CeilPixClkDiv(Div)	 \
                	(((Div) - 3)/2 << FShft (LCCR3_PCD))
                	        	
                	        	
#define LCCR3_ACB	Fld (8, 8)	
                	        	
#define LCCR3_ACBsDiv(Div)      	 \
                	(((Div) - 2)/2 << FShft (LCCR3_ACB))
                	        	
                	        	
#define LCCR3_CeilACBsDiv(Div)  	 \
                	(((Div) - 1)/2 << FShft (LCCR3_ACB))
                	        	
                	        	
#define LCCR3_API	Fld (4, 16)	
                	        	
#define LCCR3_ACBsCntOff        	 \
                	        	 \
                	(0 << FShft (LCCR3_API))
#define LCCR3_ACBsCnt(Trans)    	 \
                	        	 \
                	((Trans) << FShft (LCCR3_API))
#define LCCR3_VSP	0x00100000	
                	        	
#define LCCR3_VrtSnchH	(LCCR3_VSP*0)	
                	        	
#define LCCR3_VrtSnchL	(LCCR3_VSP*1)	
                	        	
#define LCCR3_HSP	0x00200000	
                	        	
#define LCCR3_HorSnchH	(LCCR3_HSP*0)	
                	        	
#define LCCR3_HorSnchL	(LCCR3_HSP*1)	
                	        	
#define LCCR3_PCP	0x00400000	
#define LCCR3_PixRsEdg	(LCCR3_PCP*0)	
#define LCCR3_PixFlEdg	(LCCR3_PCP*1)	
#define LCCR3_OEP	0x00800000	
                	        	
#define LCCR3_OutEnH	(LCCR3_OEP*0)	
#define LCCR3_OutEnL	(LCCR3_OEP*1)	
