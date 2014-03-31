/*
 * Copyright 2008-2009 Analog Devices Inc.
 *
 * Licensed under the ADI BSD license or the GPL-2 (or later)
 */

#ifndef _DEF_BF516_H
#define _DEF_BF516_H

#include "defBF514.h"


#define EMAC_OPMODE             0xFFC03000       
#define EMAC_ADDRLO             0xFFC03004       
#define EMAC_ADDRHI             0xFFC03008       
#define EMAC_HASHLO             0xFFC0300C       
#define EMAC_HASHHI             0xFFC03010       
#define EMAC_STAADD             0xFFC03014       
#define EMAC_STADAT             0xFFC03018       
#define EMAC_FLC                0xFFC0301C       
#define EMAC_VLAN1              0xFFC03020       
#define EMAC_VLAN2              0xFFC03024       
#define EMAC_WKUP_CTL           0xFFC0302C       
#define EMAC_WKUP_FFMSK0        0xFFC03030       
#define EMAC_WKUP_FFMSK1        0xFFC03034       
#define EMAC_WKUP_FFMSK2        0xFFC03038       
#define EMAC_WKUP_FFMSK3        0xFFC0303C       
#define EMAC_WKUP_FFCMD         0xFFC03040       
#define EMAC_WKUP_FFOFF         0xFFC03044       
#define EMAC_WKUP_FFCRC0        0xFFC03048       
#define EMAC_WKUP_FFCRC1        0xFFC0304C       

#define EMAC_SYSCTL             0xFFC03060       
#define EMAC_SYSTAT             0xFFC03064       
#define EMAC_RX_STAT            0xFFC03068       
#define EMAC_RX_STKY            0xFFC0306C       
#define EMAC_RX_IRQE            0xFFC03070       
#define EMAC_TX_STAT            0xFFC03074       
#define EMAC_TX_STKY            0xFFC03078       
#define EMAC_TX_IRQE            0xFFC0307C       

#define EMAC_MMC_CTL            0xFFC03080       
#define EMAC_MMC_RIRQS          0xFFC03084       
#define EMAC_MMC_RIRQE          0xFFC03088       
#define EMAC_MMC_TIRQS          0xFFC0308C       
#define EMAC_MMC_TIRQE          0xFFC03090       

#define EMAC_RXC_OK             0xFFC03100       
#define EMAC_RXC_FCS            0xFFC03104       
#define EMAC_RXC_ALIGN          0xFFC03108       
#define EMAC_RXC_OCTET          0xFFC0310C       
#define EMAC_RXC_DMAOVF         0xFFC03110       
#define EMAC_RXC_UNICST         0xFFC03114       
#define EMAC_RXC_MULTI          0xFFC03118       
#define EMAC_RXC_BROAD          0xFFC0311C       
#define EMAC_RXC_LNERRI         0xFFC03120       
#define EMAC_RXC_LNERRO         0xFFC03124       
#define EMAC_RXC_LONG           0xFFC03128       
#define EMAC_RXC_MACCTL         0xFFC0312C       
#define EMAC_RXC_OPCODE         0xFFC03130       
#define EMAC_RXC_PAUSE          0xFFC03134       
#define EMAC_RXC_ALLFRM         0xFFC03138       
#define EMAC_RXC_ALLOCT         0xFFC0313C       
#define EMAC_RXC_TYPED          0xFFC03140       
#define EMAC_RXC_SHORT          0xFFC03144       
#define EMAC_RXC_EQ64           0xFFC03148       
#define EMAC_RXC_LT128          0xFFC0314C       
#define EMAC_RXC_LT256          0xFFC03150       
#define EMAC_RXC_LT512          0xFFC03154       
#define EMAC_RXC_LT1024         0xFFC03158       
#define EMAC_RXC_GE1024         0xFFC0315C       

#define EMAC_TXC_OK             0xFFC03180       
#define EMAC_TXC_1COL           0xFFC03184       
#define EMAC_TXC_GT1COL         0xFFC03188       
#define EMAC_TXC_OCTET          0xFFC0318C       
#define EMAC_TXC_DEFER          0xFFC03190       
#define EMAC_TXC_LATECL         0xFFC03194       
#define EMAC_TXC_XS_COL         0xFFC03198       
#define EMAC_TXC_DMAUND         0xFFC0319C       
#define EMAC_TXC_CRSERR         0xFFC031A0       
#define EMAC_TXC_UNICST         0xFFC031A4       
#define EMAC_TXC_MULTI          0xFFC031A8       
#define EMAC_TXC_BROAD          0xFFC031AC       
#define EMAC_TXC_XS_DFR         0xFFC031B0       
#define EMAC_TXC_MACCTL         0xFFC031B4       
#define EMAC_TXC_ALLFRM         0xFFC031B8       
#define EMAC_TXC_ALLOCT         0xFFC031BC       
#define EMAC_TXC_EQ64           0xFFC031C0       
#define EMAC_TXC_LT128          0xFFC031C4       
#define EMAC_TXC_LT256          0xFFC031C8       
#define EMAC_TXC_LT512          0xFFC031CC       
#define EMAC_TXC_LT1024         0xFFC031D0       
#define EMAC_TXC_GE1024         0xFFC031D4       
#define EMAC_TXC_ABORT          0xFFC031D8       


#define FramesReceivedOK                EMAC_RXC_OK        
#define FrameCheckSequenceErrors        EMAC_RXC_FCS       
#define AlignmentErrors                 EMAC_RXC_ALIGN     
#define OctetsReceivedOK                EMAC_RXC_OCTET     
#define FramesLostDueToIntMACRcvError   EMAC_RXC_DMAOVF    
#define UnicastFramesReceivedOK         EMAC_RXC_UNICST    
#define MulticastFramesReceivedOK       EMAC_RXC_MULTI     
#define BroadcastFramesReceivedOK       EMAC_RXC_BROAD     
#define InRangeLengthErrors             EMAC_RXC_LNERRI    
#define OutOfRangeLengthField           EMAC_RXC_LNERRO    
#define FrameTooLongErrors              EMAC_RXC_LONG      
#define MACControlFramesReceived        EMAC_RXC_MACCTL    
#define UnsupportedOpcodesReceived      EMAC_RXC_OPCODE    
#define PAUSEMACCtrlFramesReceived      EMAC_RXC_PAUSE     
#define FramesReceivedAll               EMAC_RXC_ALLFRM    
#define OctetsReceivedAll               EMAC_RXC_ALLOCT    
#define TypedFramesReceived             EMAC_RXC_TYPED     
#define FramesLenLt64Received           EMAC_RXC_SHORT     
#define FramesLenEq64Received           EMAC_RXC_EQ64      
#define FramesLen65_127Received         EMAC_RXC_LT128     
#define FramesLen128_255Received        EMAC_RXC_LT256     
#define FramesLen256_511Received        EMAC_RXC_LT512     
#define FramesLen512_1023Received       EMAC_RXC_LT1024    
#define FramesLen1024_MaxReceived       EMAC_RXC_GE1024    

#define FramesTransmittedOK             EMAC_TXC_OK        
#define SingleCollisionFrames           EMAC_TXC_1COL      
#define MultipleCollisionFrames         EMAC_TXC_GT1COL    
#define OctetsTransmittedOK             EMAC_TXC_OCTET     
#define FramesWithDeferredXmissions     EMAC_TXC_DEFER     
#define LateCollisions                  EMAC_TXC_LATECL    
#define FramesAbortedDueToXSColls       EMAC_TXC_XS_COL    
#define FramesLostDueToIntMacXmitError  EMAC_TXC_DMAUND    
#define CarrierSenseErrors              EMAC_TXC_CRSERR    
#define UnicastFramesXmittedOK          EMAC_TXC_UNICST    
#define MulticastFramesXmittedOK        EMAC_TXC_MULTI     
#define BroadcastFramesXmittedOK        EMAC_TXC_BROAD     
#define FramesWithExcessiveDeferral     EMAC_TXC_XS_DFR    
#define MACControlFramesTransmitted     EMAC_TXC_MACCTL    
#define FramesTransmittedAll            EMAC_TXC_ALLFRM    
#define OctetsTransmittedAll            EMAC_TXC_ALLOCT    
#define FramesLenEq64Transmitted        EMAC_TXC_EQ64      
#define FramesLen65_127Transmitted      EMAC_TXC_LT128     
#define FramesLen128_255Transmitted     EMAC_TXC_LT256     
#define FramesLen256_511Transmitted     EMAC_TXC_LT512     
#define FramesLen512_1023Transmitted    EMAC_TXC_LT1024    
#define FramesLen1024_MaxTransmitted    EMAC_TXC_GE1024    
#define TxAbortedFrames                 EMAC_TXC_ABORT     




#define	RE                 0x00000001     
#define	ASTP               0x00000002     
#define	HU                 0x00000010     
#define	HM                 0x00000020     
#define	PAM                0x00000040     
#define	PR                 0x00000080     
#define	IFE                0x00000100     
#define	DBF                0x00000200     
#define	PBF                0x00000400     
#define	PSF                0x00000800     
#define	RAF                0x00001000     
#define	TE                 0x00010000     
#define	DTXPAD             0x00020000     
#define	DTXCRC             0x00040000     
#define	DC                 0x00080000     
#define	BOLMT              0x00300000     
#define	BOLMT_10           0x00000000     
#define	BOLMT_8            0x00100000     
#define	BOLMT_4            0x00200000     
#define	BOLMT_1            0x00300000     
#define	DRTY               0x00400000     
#define	LCTRE              0x00800000     
#define	RMII               0x01000000     
#define	RMII_10            0x02000000     
#define	FDMODE             0x04000000     
#define	LB                 0x08000000     
#define	DRO                0x10000000     


#define	STABUSY            0x00000001     
#define	STAOP              0x00000002     
#define	STADISPRE          0x00000004     
#define	STAIE              0x00000008     
#define	REGAD              0x000007C0     
#define	PHYAD              0x0000F800     

#define	SET_REGAD(x) (((x)&0x1F)<<  6 )   
#define	SET_PHYAD(x) (((x)&0x1F)<< 11 )   


#define	STADATA            0x0000FFFF     


#define	FLCBUSY            0x00000001     
#define	FLCE               0x00000002     
#define	PCF                0x00000004     
#define	BKPRSEN            0x00000008     
#define	FLCPAUSE           0xFFFF0000     

#define	SET_FLCPAUSE(x) (((x)&0xFFFF)<< 16) 


#define	CAPWKFRM           0x00000001    
#define	MPKE               0x00000002    
#define	RWKE               0x00000004    
#define	GUWKE              0x00000008    
#define	MPKS               0x00000020    
#define	RWKS               0x00000F00    


#define	WF0_E              0x00000001    
#define	WF0_T              0x00000008    
#define	WF1_E              0x00000100    
#define	WF1_T              0x00000800    
#define	WF2_E              0x00010000    
#define	WF2_T              0x00080000    
#define	WF3_E              0x01000000    
#define	WF3_T              0x08000000    


#define	WF0_OFF            0x000000FF    
#define	WF1_OFF            0x0000FF00    
#define	WF2_OFF            0x00FF0000    
#define	WF3_OFF            0xFF000000    

#define	SET_WF0_OFF(x) (((x)&0xFF)<<  0 ) 
#define	SET_WF1_OFF(x) (((x)&0xFF)<<  8 ) 
#define	SET_WF2_OFF(x) (((x)&0xFF)<< 16 ) 
#define	SET_WF3_OFF(x) (((x)&0xFF)<< 24 ) 
#define	SET_WF_OFFS(x0,x1,x2,x3) (SET_WF0_OFF((x0))|SET_WF1_OFF((x1))|SET_WF2_OFF((x2))|SET_WF3_OFF((x3)))


#define	WF0_CRC           0x0000FFFF    
#define	WF1_CRC           0xFFFF0000    

#define	SET_WF0_CRC(x) (((x)&0xFFFF)<<   0 ) 
#define	SET_WF1_CRC(x) (((x)&0xFFFF)<<  16 ) 


#define	WF2_CRC           0x0000FFFF    
#define	WF3_CRC           0xFFFF0000    

#define	SET_WF2_CRC(x) (((x)&0xFFFF)<<   0 ) 
#define	SET_WF3_CRC(x) (((x)&0xFFFF)<<  16 ) 


#define	PHYIE             0x00000001    
#define	RXDWA             0x00000002    
#define	RXCKS             0x00000004    
#define	TXDWA             0x00000010    
#define	MDCDIV            0x00003F00    

#define	SET_MDCDIV(x) (((x)&0x3F)<< 8)   


#define	PHYINT            0x00000001    
#define	MMCINT            0x00000002    
#define	RXFSINT           0x00000004    
#define	TXFSINT           0x00000008    
#define	WAKEDET           0x00000010    
#define	RXDMAERR          0x00000020    
#define	TXDMAERR          0x00000040    
#define	STMDONE           0x00000080    


#define	RX_FRLEN          0x000007FF    
#define	RX_COMP           0x00001000    
#define	RX_OK             0x00002000    
#define	RX_LONG           0x00004000    
#define	RX_ALIGN          0x00008000    
#define	RX_CRC            0x00010000    
#define	RX_LEN            0x00020000    
#define	RX_FRAG           0x00040000    
#define	RX_ADDR           0x00080000    
#define	RX_DMAO           0x00100000    
#define	RX_PHY            0x00200000    
#define	RX_LATE           0x00400000    
#define	RX_RANGE          0x00800000    
#define	RX_MULTI          0x01000000    
#define	RX_BROAD          0x02000000    
#define	RX_CTL            0x04000000    
#define	RX_UCTL           0x08000000    
#define	RX_TYPE           0x10000000    
#define	RX_VLAN1          0x20000000    
#define	RX_VLAN2          0x40000000    
#define	RX_ACCEPT         0x80000000    


#define	TX_COMP           0x00000001    
#define	TX_OK             0x00000002    
#define	TX_ECOLL          0x00000004    
#define	TX_LATE           0x00000008    
#define	TX_DMAU           0x00000010    
#define	TX_MACE           0x00000010    
#define	TX_EDEFER         0x00000020    
#define	TX_BROAD          0x00000040    
#define	TX_MULTI          0x00000080    
#define	TX_CCNT           0x00000F00    
#define	TX_DEFER          0x00001000    
#define	TX_CRS            0x00002000    
#define	TX_LOSS           0x00004000    
#define	TX_RETRY          0x00008000    
#define	TX_FRLEN          0x07FF0000    

#define	RSTC              0x00000001    
#define	CROLL             0x00000002    
#define	CCOR              0x00000004    
#define	MMCE              0x00000008    

#define	RX_OK_CNT         0x00000001    
#define	RX_FCS_CNT        0x00000002    
#define	RX_ALIGN_CNT      0x00000004    
#define	RX_OCTET_CNT      0x00000008    
#define	RX_LOST_CNT       0x00000010    
#define	RX_UNI_CNT        0x00000020    
#define	RX_MULTI_CNT      0x00000040    
#define	RX_BROAD_CNT      0x00000080    
#define	RX_IRL_CNT        0x00000100    
#define	RX_ORL_CNT        0x00000200    
#define	RX_LONG_CNT       0x00000400    
#define	RX_MACCTL_CNT     0x00000800    
#define	RX_OPCODE_CTL     0x00001000    
#define	RX_PAUSE_CNT      0x00002000    
#define	RX_ALLF_CNT       0x00004000    
#define	RX_ALLO_CNT       0x00008000    
#define	RX_TYPED_CNT      0x00010000    
#define	RX_SHORT_CNT      0x00020000    
#define	RX_EQ64_CNT       0x00040000    
#define	RX_LT128_CNT      0x00080000    
#define	RX_LT256_CNT      0x00100000    
#define	RX_LT512_CNT      0x00200000    
#define	RX_LT1024_CNT     0x00400000    
#define	RX_GE1024_CNT     0x00800000    


#define	TX_OK_CNT         0x00000001    
#define	TX_SCOLL_CNT      0x00000002    
#define	TX_MCOLL_CNT      0x00000004    
#define	TX_OCTET_CNT      0x00000008    
#define	TX_DEFER_CNT      0x00000010    
#define	TX_LATE_CNT       0x00000020    
#define	TX_ABORTC_CNT     0x00000040    
#define	TX_LOST_CNT       0x00000080    
#define	TX_CRS_CNT        0x00000100    
#define	TX_UNI_CNT        0x00000200    
#define	TX_MULTI_CNT      0x00000400    
#define	TX_BROAD_CNT      0x00000800    
#define	TX_EXDEF_CTL      0x00001000    
#define	TX_MACCTL_CNT     0x00002000    
#define	TX_ALLF_CNT       0x00004000    
#define	TX_ALLO_CNT       0x00008000    
#define	TX_EQ64_CNT       0x00010000    
#define	TX_LT128_CNT      0x00020000    
#define	TX_LT256_CNT      0x00040000    
#define	TX_LT512_CNT      0x00080000    
#define	TX_LT1024_CNT     0x00100000    
#define	TX_GE1024_CNT     0x00200000    
#define	TX_ABORT_CNT      0x00400000    

#endif 
