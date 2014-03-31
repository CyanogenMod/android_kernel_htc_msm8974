/*
    Copyright 1994 Digital Equipment Corporation.

    This software may be used and distributed according to  the terms of the
    GNU General Public License, incorporated herein by reference.

    The author may    be  reached as davies@wanton.lkg.dec.com  or   Digital
    Equipment Corporation, 550 King Street, Littleton MA 01460.

    =========================================================================
*/

#define DE4X5_BMR    iobase+(0x000 << lp->bus)  
#define DE4X5_TPD    iobase+(0x008 << lp->bus)  
#define DE4X5_RPD    iobase+(0x010 << lp->bus)  
#define DE4X5_RRBA   iobase+(0x018 << lp->bus)  
#define DE4X5_TRBA   iobase+(0x020 << lp->bus)  
#define DE4X5_STS    iobase+(0x028 << lp->bus)  
#define DE4X5_OMR    iobase+(0x030 << lp->bus)  
#define DE4X5_IMR    iobase+(0x038 << lp->bus)  
#define DE4X5_MFC    iobase+(0x040 << lp->bus)  
#define DE4X5_APROM  iobase+(0x048 << lp->bus)  
#define DE4X5_BROM   iobase+(0x048 << lp->bus)  
#define DE4X5_SROM   iobase+(0x048 << lp->bus)  
#define DE4X5_MII    iobase+(0x048 << lp->bus)  
#define DE4X5_DDR    iobase+(0x050 << lp->bus)  
#define DE4X5_FDR    iobase+(0x058 << lp->bus)  
#define DE4X5_GPT    iobase+(0x058 << lp->bus)  
#define DE4X5_GEP    iobase+(0x060 << lp->bus)  
#define DE4X5_SISR   iobase+(0x060 << lp->bus)  
#define DE4X5_SICR   iobase+(0x068 << lp->bus)  
#define DE4X5_STRR   iobase+(0x070 << lp->bus)  
#define DE4X5_SIGR   iobase+(0x078 << lp->bus)  

#define EISA_ID      iobase+0x0c80   
#define EISA_ID0     iobase+0x0c80   
#define EISA_ID1     iobase+0x0c81   
#define EISA_ID2     iobase+0x0c82   
#define EISA_ID3     iobase+0x0c83   
#define EISA_CR      iobase+0x0c84   
#define EISA_REG0    iobase+0x0c88   
#define EISA_REG1    iobase+0x0c89   
#define EISA_REG2    iobase+0x0c8a   
#define EISA_REG3    iobase+0x0c8f   
#define EISA_APROM   iobase+0x0c90   

#define PCI_CFID     iobase+0x0008   
#define PCI_CFCS     iobase+0x000c   
#define PCI_CFRV     iobase+0x0018   
#define PCI_CFLT     iobase+0x001c   
#define PCI_CBIO     iobase+0x0028   
#define PCI_CBMA     iobase+0x002c   
#define PCI_CBER     iobase+0x0030   
#define PCI_CFIT     iobase+0x003c   
#define PCI_CFDA     iobase+0x0040   
#define PCI_CFDD     iobase+0x0041   
#define PCI_CFPM     iobase+0x0043   

#define ER0_BSW       0x80           
#define ER0_BMW       0x40           
#define ER0_EPT       0x20           
#define ER0_ISTS      0x10           
#define ER0_LI        0x08           
#define ER0_INTL      0x06           
#define ER0_INTT      0x01           

#define ER1_IAM       0xe0           
#define ER1_IAE       0x10           
#define ER1_UPIN      0x0f           

#define ER2_BRS       0xc0           
#define ER2_BRA       0x3c           

#define ER3_BWE       0x40           
#define ER3_BRE       0x04           
#define ER3_LSR       0x02           

#define CFID_DID    0xff00           
#define CFID_VID    0x00ff           
#define DC21040_DID 0x0200           
#define DC21040_VID 0x1011           
#define DC21041_DID 0x1400           
#define DC21041_VID 0x1011           
#define DC21140_DID 0x0900           
#define DC21140_VID 0x1011           
#define DC2114x_DID 0x1900           
#define DC2114x_VID 0x1011           

#define DC21040     DC21040_DID
#define DC21041     DC21041_DID
#define DC21140     DC21140_DID
#define DC2114x     DC2114x_DID
#define DC21142     (DC2114x_DID | 0x0010)
#define DC21143     (DC2114x_DID | 0x0030)
#define DC2114x_BRK 0x0020           

#define is_DC21040 ((vendor == DC21040_VID) && (device == DC21040_DID))
#define is_DC21041 ((vendor == DC21041_VID) && (device == DC21041_DID))
#define is_DC21140 ((vendor == DC21140_VID) && (device == DC21140_DID))
#define is_DC2114x ((vendor == DC2114x_VID) && (device == DC2114x_DID))
#define is_DC21142 ((vendor == DC2114x_VID) && (device == DC21142))
#define is_DC21143 ((vendor == DC2114x_VID) && (device == DC21143))

#define CFCS_DPE    0x80000000       
#define CFCS_SSE    0x40000000       
#define CFCS_RMA    0x20000000       
#define CFCS_RTA    0x10000000       
#define CFCS_DST    0x06000000       
#define CFCS_DPR    0x01000000       
#define CFCS_FBB    0x00800000       
#define CFCS_SEE    0x00000100       
#define CFCS_PER    0x00000040       
#define CFCS_MO     0x00000004       
#define CFCS_MSA    0x00000002       
#define CFCS_IOSA   0x00000001       

#define CFRV_BC     0xff000000       
#define CFRV_SC     0x00ff0000       
#define CFRV_RN     0x000000f0       
#define CFRV_SN     0x0000000f       
#define BASE_CLASS  0x02000000       
#define SUB_CLASS   0x00000000       
#define STEP_NUMBER 0x00000020       
#define REV_NUMBER  0x00000003       
#define CFRV_MASK   0xffff0000       

#define CFLT_BC     0x0000ff00       

#define CBIO_MASK   -128             
#define CBIO_IOSI   0x00000001       

#define CCIS_ROMI   0xf0000000       
#define CCIS_ASO    0x0ffffff8       
#define CCIS_ASI    0x00000007       

#define SSID_SSID   0xffff0000       
#define SSID_SVID   0x0000ffff       

#define CBER_MASK   0xfffffc00       
#define CBER_ROME   0x00000001       

#define CFIT_MXLT   0xff000000       
#define CFIT_MNGT   0x00ff0000       
#define CFIT_IRQP   0x0000ff00       
#define CFIT_IRQL   0x000000ff       

#define SLEEP       0x80             
#define SNOOZE      0x40             
#define WAKEUP      0x00             

#define PCI_CFDA_DSU 0x41            
#define PCI_CFDA_PSM 0x43            

#define BMR_RML    0x00200000       
#define BMR_DBO    0x00100000       
#define BMR_TAP    0x000e0000       
#define BMR_DAS    0x00010000       
#define BMR_CAL    0x0000c000       
#define BMR_PBL    0x00003f00       
#define BMR_BLE    0x00000080       
#define BMR_DSL    0x0000007c       
#define BMR_BAR    0x00000002       
#define BMR_SWR    0x00000001       

                                    
#define TAP_NOPOLL 0x00000000       
#define TAP_200US  0x00020000       
#define TAP_800US  0x00040000       
#define TAP_1_6MS  0x00060000       
#define TAP_12_8US 0x00080000       
#define TAP_25_6US 0x000a0000       
#define TAP_51_2US 0x000c0000       
#define TAP_102_4US 0x000e0000      

#define CAL_NOUSE  0x00000000       
#define CAL_8LONG  0x00004000       
#define CAL_16LONG 0x00008000       
#define CAL_32LONG 0x0000c000       

#define PBL_0      0x00000000       
#define PBL_1      0x00000100       
#define PBL_2      0x00000200       
#define PBL_4      0x00000400       
#define PBL_8      0x00000800       
#define PBL_16     0x00001000       
#define PBL_32     0x00002000       

#define DSL_0      0x00000000       
#define DSL_1      0x00000004       
#define DSL_2      0x00000008       
#define DSL_4      0x00000010       
#define DSL_8      0x00000020       
#define DSL_16     0x00000040       
#define DSL_32     0x00000080       

#define TPD        0x00000001       

#define RPD        0x00000001       

#define RRBA       0xfffffffc       

#define TRBA       0xfffffffc       

#define STS_GPI    0x04000000       
#define STS_BE     0x03800000       
#define STS_TS     0x00700000       
#define STS_RS     0x000e0000       
#define STS_NIS    0x00010000       
#define STS_AIS    0x00008000       
#define STS_ER     0x00004000       
#define STS_FBE    0x00002000       
#define STS_SE     0x00002000       
#define STS_LNF    0x00001000       
#define STS_FD     0x00000800       
#define STS_TM     0x00000800       
#define STS_ETI    0x00000400       
#define STS_AT     0x00000400       
#define STS_RWT    0x00000200       
#define STS_RPS    0x00000100       
#define STS_RU     0x00000080       
#define STS_RI     0x00000040       
#define STS_UNF    0x00000020       
#define STS_LNP    0x00000010       
#define STS_ANC    0x00000010       
#define STS_TJT    0x00000008       
#define STS_TU     0x00000004       
#define STS_TPS    0x00000002       
#define STS_TI     0x00000001       

#define EB_PAR     0x00000000       
#define EB_MA      0x00800000       
#define EB_TA      0x01000000       
#define EB_RES0    0x01800000       
#define EB_RES1    0x02000000       

#define TS_STOP    0x00000000       
#define TS_FTD     0x00100000       
#define TS_WEOT    0x00200000       
#define TS_QDAT    0x00300000       
#define TS_RES     0x00400000       
#define TS_SPKT    0x00500000       
#define TS_SUSP    0x00600000       
#define TS_CLTD    0x00700000       

#define RS_STOP    0x00000000       
#define RS_FRD     0x00020000       
#define RS_CEOR    0x00040000       
#define RS_WFRP    0x00060000       
#define RS_SUSP    0x00080000       
#define RS_CLRD    0x000a0000       
#define RS_FLUSH   0x000c0000       
#define RS_QRFS    0x000e0000       

#define INT_CANCEL 0x0001ffff       

#define OMR_SC     0x80000000       
#define OMR_RA     0x40000000       
#define OMR_SDP    0x02000000       
#define OMR_SCR    0x01000000       
#define OMR_PCS    0x00800000       
#define OMR_TTM    0x00400000       
#define OMR_SF     0x00200000       
#define OMR_HBD    0x00080000       
#define OMR_PS     0x00040000       
#define OMR_CA     0x00020000       
#define OMR_BP     0x00010000       
#define OMR_TR     0x0000c000       
#define OMR_ST     0x00002000       
#define OMR_FC     0x00001000       
#define OMR_OM     0x00000c00       
#define OMR_FDX    0x00000200       
#define OMR_FKD    0x00000100       
#define OMR_PM     0x00000080       
#define OMR_PR     0x00000040       
#define OMR_SB     0x00000020       
#define OMR_IF     0x00000010       
#define OMR_PB     0x00000008       
#define OMR_HO     0x00000004       
#define OMR_SR     0x00000002       
#define OMR_HP     0x00000001       

#define TR_72      0x00000000       
#define TR_96      0x00004000       
#define TR_128     0x00008000       
#define TR_160     0x0000c000       

#define OMR_DEF     (OMR_SDP)
#define OMR_SIA     (OMR_SDP | OMR_TTM)
#define OMR_SYM     (OMR_SDP | OMR_SCR | OMR_PCS | OMR_HBD | OMR_PS)
#define OMR_MII_10  (OMR_SDP | OMR_TTM | OMR_PS)
#define OMR_MII_100 (OMR_SDP | OMR_HBD | OMR_PS)

#define IMR_GPM    0x04000000       
#define IMR_NIM    0x00010000       
#define IMR_AIM    0x00008000       
#define IMR_ERM    0x00004000       
#define IMR_FBM    0x00002000       
#define IMR_SEM    0x00002000       
#define IMR_LFM    0x00001000       
#define IMR_FDM    0x00000800       
#define IMR_TMM    0x00000800       
#define IMR_ETM    0x00000400       
#define IMR_ATM    0x00000400       
#define IMR_RWM    0x00000200       
#define IMR_RSM    0x00000100       
#define IMR_RUM    0x00000080       
#define IMR_RIM    0x00000040       
#define IMR_UNM    0x00000020       
#define IMR_ANM    0x00000010       
#define IMR_LPM    0x00000010       
#define IMR_TJM    0x00000008       
#define IMR_TUM    0x00000004       
#define IMR_TSM    0x00000002       
#define IMR_TIM    0x00000001       

#define MFC_FOCO   0x10000000       
#define MFC_FOC    0x0ffe0000       
#define MFC_OVFL   0x00010000       
#define MFC_CNTR   0x0000ffff       
#define MFC_FOCM   0x1ffe0000       

#define APROM_DN   0x80000000       
#define APROM_DT   0x000000ff       

#define BROM_MODE 0x00008000       
#define BROM_RD   0x00004000       
#define BROM_WR   0x00002000       
#define BROM_BR   0x00001000       
#define BROM_SR   0x00000800       
#define BROM_REG  0x00000400       
#define BROM_DT   0x000000ff       

#define MII_MDI   0x00080000       
#define MII_MDO   0x00060000       
#define MII_MRD   0x00040000       
#define MII_MWR   0x00000000       
#define MII_MDT   0x00020000       
#define MII_MDC   0x00010000       
#define MII_RD    0x00004000       
#define MII_WR    0x00002000       
#define MII_SEL   0x00000800       

#define SROM_MODE 0x00008000       
#define SROM_RD   0x00004000       
#define SROM_WR   0x00002000       
#define SROM_BR   0x00001000       
#define SROM_SR   0x00000800       
#define SROM_REG  0x00000400       
#define SROM_DT   0x000000ff       

#define DT_OUT    0x00000008       
#define DT_IN     0x00000004       
#define DT_CLK    0x00000002       
#define DT_CS     0x00000001       

#define MII_PREAMBLE 0xffffffff    
#define MII_TEST     0xaaaaaaaa    
#define MII_STRD     0x06          
#define MII_STWR     0x0a          

#define MII_CR       0x00          
#define MII_SR       0x01          
#define MII_ID0      0x02          
#define MII_ID1      0x03          
#define MII_ANA      0x04          
#define MII_ANLPA    0x05          
#define MII_ANE      0x06          
#define MII_ANP      0x07          

#define DE4X5_MAX_MII 32           

#define MII_CR_RST  0x8000         
#define MII_CR_LPBK 0x4000         
#define MII_CR_SPD  0x2000         
#define MII_CR_10   0x0000         
#define MII_CR_100  0x2000         
#define MII_CR_ASSE 0x1000         
#define MII_CR_PD   0x0800         
#define MII_CR_ISOL 0x0400         
#define MII_CR_RAN  0x0200         
#define MII_CR_FDM  0x0100         
#define MII_CR_CTE  0x0080         

#define MII_SR_T4C  0x8000         
#define MII_SR_TXFD 0x4000         
#define MII_SR_TXHD 0x2000         
#define MII_SR_TFD  0x1000         
#define MII_SR_THD  0x0800         
#define MII_SR_ASSC 0x0020         
#define MII_SR_RFD  0x0010         
#define MII_SR_ANC  0x0008         
#define MII_SR_LKS  0x0004         
#define MII_SR_JABD 0x0002         
#define MII_SR_XC   0x0001         

#define MII_ANA_TAF  0x03e0        
#define MII_ANA_T4AM 0x0200        
#define MII_ANA_TXAM 0x0180        
#define MII_ANA_FDAM 0x0140        
#define MII_ANA_HDAM 0x02a0        
#define MII_ANA_100M 0x0380        
#define MII_ANA_10M  0x0060        
#define MII_ANA_CSMA 0x0001        

#define MII_ANLPA_NP   0x8000      
#define MII_ANLPA_ACK  0x4000      
#define MII_ANLPA_RF   0x2000      
#define MII_ANLPA_TAF  0x03e0      
#define MII_ANLPA_T4AM 0x0200      
#define MII_ANLPA_TXAM 0x0180      
#define MII_ANLPA_FDAM 0x0140      
#define MII_ANLPA_HDAM 0x02a0      
#define MII_ANLPA_100M 0x0380      
#define MII_ANLPA_10M  0x0060      
#define MII_ANLPA_CSMA 0x0001      

#define MEDIA_NWAY     0x0080      
#define MEDIA_MII      0x0040      
#define MEDIA_FIBRE    0x0008      
#define MEDIA_AUI      0x0004      
#define MEDIA_TP       0x0002      
#define MEDIA_BNC      0x0001      

#define SROM_SSVID     0x0000      
#define SROM_SSID      0x0002      
#define SROM_CISPL     0x0004      
#define SROM_CISPH     0x0006      
#define SROM_IDCRC     0x0010      
#define SROM_RSVD2     0x0011      
#define SROM_SFV       0x0012      
#define SROM_CCNT      0x0013      
#define SROM_HWADD     0x0014      
#define SROM_MRSVD     0x007c      
#define SROM_CRC       0x007e      

#define SROM_10BT      0x0000      
#define SROM_10BTN     0x0100      
#define SROM_10BTF     0x0204      
#define SROM_10BTNLP   0x0400      
#define SROM_10B2      0x0001      
#define SROM_10B5      0x0002      
#define SROM_100BTH    0x0003      
#define SROM_100BTF    0x0205      
#define SROM_100BT4    0x0006      
#define SROM_100BFX    0x0007      
#define SROM_M10BT     0x0009      
#define SROM_M10BTF    0x020a      
#define SROM_M100BT    0x000d      
#define SROM_M100BTF   0x020e      
#define SROM_M100BT4   0x000f      
#define SROM_M100BF    0x0010      
#define SROM_M100BFF   0x0211      
#define SROM_PDA       0x0800      
#define SROM_PAO       0x8800      
#define SROM_NSMI      0xffff      

#define SROM_10BASET   0x0000      
#define SROM_10BASE2   0x0001      
#define SROM_10BASE5   0x0002      
#define SROM_100BASET  0x0003      
#define SROM_10BASETF  0x0004      
#define SROM_100BASETF 0x0005      
#define SROM_100BASET4 0x0006      
#define SROM_100BASEF  0x0007      
#define SROM_100BASEFF 0x0008      

#define BLOCK_LEN      0x7f        
#define EXT_FIELD      0x40        
#define MEDIA_CODE     0x3f        

#define COMPACT_FI      0x80       
#define COMPACT_LEN     0x04       
#define COMPACT_MC      0x3f       

#define BLOCK0_FI      0x80        
#define BLOCK0_MCS     0x80        
#define BLOCK0_MC      0x3f        

#define FDR_FDACV  0x0000ffff      

#define GPT_CON  0x00010000        
#define GPT_VAL  0x0000ffff        

#define GEP_LNP  0x00000080        
#define GEP_SLNK 0x00000040        
#define GEP_SDET 0x00000020        
#define GEP_HRST 0x00000010        
#define GEP_FDXD 0x00000008        
#define GEP_PHYL 0x00000004        
#define GEP_FLED 0x00000002        
#define GEP_MODE 0x00000001        
#define GEP_INIT 0x0000011f        
#define GEP_CTRL 0x00000100        

#define CSR13 0x00000001
#define CSR14 0x0003ff7f           
#define CSR15 0x00000008

#define SISR_LPC   0xffff0000      
#define SISR_LPN   0x00008000      
#define SISR_ANS   0x00007000      
#define SISR_NSN   0x00000800      
#define SISR_TRF   0x00000800      
#define SISR_NSND  0x00000400      
#define SISR_ANR_FDS 0x00000400    
#define SISR_TRA   0x00000200      
#define SISR_NRA   0x00000200      
#define SISR_ARA   0x00000100      
#define SISR_SRA   0x00000100      
#define SISR_DAO   0x00000080      
#define SISR_DAZ   0x00000040      
#define SISR_DSP   0x00000020      
#define SISR_DSD   0x00000010      
#define SISR_APS   0x00000008      
#define SISR_LKF   0x00000004      
#define SISR_LS10  0x00000004      
#define SISR_NCR   0x00000002      
#define SISR_LS100 0x00000002      
#define SISR_PAUI  0x00000001      
#define SISR_MRA   0x00000001      

#define ANS_NDIS   0x00000000      
#define ANS_TDIS   0x00001000      
#define ANS_ADET   0x00002000      
#define ANS_ACK    0x00003000      
#define ANS_CACK   0x00004000      
#define ANS_NWOK   0x00005000      
#define ANS_LCHK   0x00006000      

#define SISR_RST   0x00000301      
#define SISR_ANR   0x00001301      

#define SICR_SDM   0xffff0000       
#define SICR_OE57  0x00008000       
#define SICR_OE24  0x00004000       
#define SICR_OE13  0x00002000       
#define SICR_IE    0x00001000       
#define SICR_EXT   0x00000000       
#define SICR_D_SIA 0x00000400       
#define SICR_DPLL  0x00000800       
#define SICR_APLL  0x00000a00       
#define SICR_D_RxM 0x00000c00       
#define SICR_M_RxM 0x00000d00       
#define SICR_LNKT  0x00000e00       
#define SICR_SEL   0x00000f00       
#define SICR_ASE   0x00000080       
#define SICR_SIM   0x00000040       
#define SICR_ENI   0x00000020       
#define SICR_EDP   0x00000010       
#define SICR_AUI   0x00000008       
#define SICR_CAC   0x00000004       
#define SICR_PS    0x00000002       
#define SICR_SRL   0x00000001       
#define SIA_RESET  0x00000000       

#define STRR_TAS   0x00008000       
#define STRR_SPP   0x00004000       
#define STRR_APE   0x00002000       
#define STRR_LTE   0x00001000       
#define STRR_SQE   0x00000800       
#define STRR_CLD   0x00000400       
#define STRR_CSQ   0x00000200       
#define STRR_RSQ   0x00000100       
#define STRR_ANE   0x00000080       
#define STRR_HDE   0x00000040       
#define STRR_CPEN  0x00000030       
#define STRR_LSE   0x00000008       
#define STRR_DREN  0x00000004       
#define STRR_LBK   0x00000002       
#define STRR_ECEN  0x00000001       
#define STRR_RESET 0xffffffff       

#define SIGR_RMI   0x40000000       
#define SIGR_GI1   0x20000000       
#define SIGR_GI0   0x10000000       
#define SIGR_CWE   0x08000000       
#define SIGR_RME   0x04000000       
#define SIGR_GEI1  0x02000000       
#define SIGR_GEI0  0x01000000       
#define SIGR_LGS3  0x00800000       
#define SIGR_LGS2  0x00400000       
#define SIGR_LGS1  0x00200000       
#define SIGR_LGS0  0x00100000       
#define SIGR_MD    0x000f0000       
#define SIGR_LV2   0x00008000       
#define SIGR_LE2   0x00004000       
#define SIGR_FRL   0x00002000       
#define SIGR_DPST  0x00001000       
#define SIGR_LSD   0x00000800       
#define SIGR_FLF   0x00000400       
#define SIGR_FUSQ  0x00000200       
#define SIGR_TSCK  0x00000100       
#define SIGR_LV1   0x00000080       
#define SIGR_LE1   0x00000040       
#define SIGR_RWR   0x00000020       
#define SIGR_RWD   0x00000010       
#define SIGR_ABM   0x00000008       
#define SIGR_JCK   0x00000004       
#define SIGR_HUJ   0x00000002       
#define SIGR_JBD   0x00000001       
#define SIGR_RESET 0xffff0000       

#define R_OWN      0x80000000       
#define RD_FF      0x40000000       
#define RD_FL      0x3fff0000       
#define RD_ES      0x00008000       
#define RD_LE      0x00004000       
#define RD_DT      0x00003000       
#define RD_RF      0x00000800       
#define RD_MF      0x00000400       
#define RD_FS      0x00000200       
#define RD_LS      0x00000100       
#define RD_TL      0x00000080       
#define RD_CS      0x00000040       
#define RD_FT      0x00000020       
#define RD_RJ      0x00000010       
#define RD_RE      0x00000008       
#define RD_DB      0x00000004       
#define RD_CE      0x00000002       
#define RD_OF      0x00000001       

#define RD_RER     0x02000000       
#define RD_RCH     0x01000000       
#define RD_RBS2    0x003ff800       
#define RD_RBS1    0x000007ff       

#define T_OWN      0x80000000       
#define TD_ES      0x00008000       
#define TD_TO      0x00004000       
#define TD_LO      0x00000800       
#define TD_NC      0x00000400       
#define TD_LC      0x00000200       
#define TD_EC      0x00000100       
#define TD_HF      0x00000080       
#define TD_CC      0x00000078       
#define TD_LF      0x00000004       
#define TD_UF      0x00000002       
#define TD_DE      0x00000001       

#define TD_IC      0x80000000       
#define TD_LS      0x40000000       
#define TD_FS      0x20000000       
#define TD_FT1     0x10000000       
#define TD_SET     0x08000000       
#define TD_AC      0x04000000       
#define TD_TER     0x02000000       
#define TD_TCH     0x01000000       
#define TD_DPD     0x00800000       
#define TD_FT0     0x00400000       
#define TD_TBS2    0x003ff800       
#define TD_TBS1    0x000007ff       

#define PERFECT_F  0x00000000
#define HASH_F     TD_FT0
#define INVERSE_F  TD_FT1
#define HASH_O_F   (TD_FT1 | TD_F0)

#define TP              0x0040     
#define TP_NW           0x0002     
#define BNC             0x0004     
#define AUI             0x0008     
#define BNC_AUI         0x0010     
#define _10Mb           0x0040     
#define _100Mb          0x0080     
#define AUTO            0x4000     

#define NC              0x0000     
#define ANS             0x0020     
#define SPD_DET         0x0100     
#define INIT            0x0200     
#define EXT_SIA         0x0400     
#define ANS_SUSPECT     0x0802     
#define TP_SUSPECT      0x0803     
#define BNC_AUI_SUSPECT 0x0804     
#define EXT_SIA_SUSPECT 0x0805     
#define BNC_SUSPECT     0x0806     
#define AUI_SUSPECT     0x0807     
#define MII             0x1000     

#define TIMER_CB        0x80000000 

#define DEBUG_NONE      0x0000     
#define DEBUG_VERSION   0x0001     
#define DEBUG_MEDIA     0x0002     
#define DEBUG_TX        0x0004     
#define DEBUG_RX        0x0008     
#define DEBUG_SROM      0x0010     
#define DEBUG_MII       0x0020     
#define DEBUG_OPEN      0x0040     
#define DEBUG_CLOSE     0x0080     
#define DEBUG_PCICFG    0x0100
#define DEBUG_ALL       0x01ff

#define PCI  0
#define EISA 1

#define HASH_TABLE_LEN   512       
#define HASH_BITS        0x01ff    

#define SETUP_FRAME_LEN  192       
#define IMPERF_PA_OFFSET 156       

#define POLL_DEMAND          1

#define LOST_MEDIA_THRESHOLD 3

#define MASK_INTERRUPTS      1
#define UNMASK_INTERRUPTS    0

#define DE4X5_STRLEN         8

#define DE4X5_INIT           0     
#define DE4X5_RUN            1     

#define DE4X5_SAVE_STATE     0
#define DE4X5_RESTORE_STATE  1

#define PERFECT              0     
#define HASH_PERF            1     
#define PERFECT_REJ          2     
#define ALL_HASH             3     

#define ALL                  0     
#define PHYS_ADDR_ONLY       1     

#define INITIALISED          0     
#define CLOSED               1     
#define OPEN                 2     

#define PDET_LINK_WAIT    1200    
#define ANS_FINISH_WAIT   1000    

#define NATIONAL_TX 0x2000
#define BROADCOM_T4 0x03e0
#define SEEQ_T4     0x0016
#define CYPRESS_T4  0x0014

#define SET_10Mb {\
  if ((lp->phy[lp->active].id) && (!lp->useSROM || lp->useMII)) {\
    omr = inl(DE4X5_OMR) & ~(OMR_TTM | OMR_PCS | OMR_SCR | OMR_FDX);\
    if ((lp->tmp != MII_SR_ASSC) || (lp->autosense != AUTO)) {\
      mii_wr(MII_CR_10|(lp->fdx?MII_CR_FDM:0), MII_CR, lp->phy[lp->active].addr, DE4X5_MII);\
    }\
    omr |= ((lp->fdx ? OMR_FDX : 0) | OMR_TTM);\
    outl(omr, DE4X5_OMR);\
    if (!lp->useSROM) lp->cache.gep = 0;\
  } else if (lp->useSROM && !lp->useMII) {\
    omr = (inl(DE4X5_OMR) & ~(OMR_PS | OMR_HBD | OMR_TTM | OMR_PCS | OMR_SCR | OMR_FDX));\
    omr |= (lp->fdx ? OMR_FDX : 0);\
    outl(omr | (lp->infoblock_csr6 & ~(OMR_SCR | OMR_HBD)), DE4X5_OMR);\
  } else {\
    omr = (inl(DE4X5_OMR) & ~(OMR_PS | OMR_HBD | OMR_TTM | OMR_PCS | OMR_SCR | OMR_FDX));\
    omr |= (lp->fdx ? OMR_FDX : 0);\
    outl(omr | OMR_SDP | OMR_TTM, DE4X5_OMR);\
    lp->cache.gep = (lp->fdx ? 0 : GEP_FDXD);\
    gep_wr(lp->cache.gep, dev);\
  }\
}

#define SET_100Mb {\
  if ((lp->phy[lp->active].id) && (!lp->useSROM || lp->useMII)) {\
    int fdx=0;\
    if (lp->phy[lp->active].id == NATIONAL_TX) {\
        mii_wr(mii_rd(0x18, lp->phy[lp->active].addr, DE4X5_MII) & ~0x2000,\
                      0x18, lp->phy[lp->active].addr, DE4X5_MII);\
    }\
    omr = inl(DE4X5_OMR) & ~(OMR_TTM | OMR_PCS | OMR_SCR | OMR_FDX);\
    sr = mii_rd(MII_SR, lp->phy[lp->active].addr, DE4X5_MII);\
    if (!(sr & MII_ANA_T4AM) && lp->fdx) fdx=1;\
    if ((lp->tmp != MII_SR_ASSC) || (lp->autosense != AUTO)) {\
      mii_wr(MII_CR_100|(fdx?MII_CR_FDM:0), MII_CR, lp->phy[lp->active].addr, DE4X5_MII);\
    }\
    if (fdx) omr |= OMR_FDX;\
    outl(omr, DE4X5_OMR);\
    if (!lp->useSROM) lp->cache.gep = 0;\
  } else if (lp->useSROM && !lp->useMII) {\
    omr = (inl(DE4X5_OMR) & ~(OMR_PS | OMR_HBD | OMR_TTM | OMR_PCS | OMR_SCR | OMR_FDX));\
    omr |= (lp->fdx ? OMR_FDX : 0);\
    outl(omr | lp->infoblock_csr6, DE4X5_OMR);\
  } else {\
    omr = (inl(DE4X5_OMR) & ~(OMR_PS | OMR_HBD | OMR_TTM | OMR_PCS | OMR_SCR | OMR_FDX));\
    omr |= (lp->fdx ? OMR_FDX : 0);\
    outl(omr | OMR_SDP | OMR_PS | OMR_HBD | OMR_PCS | OMR_SCR, DE4X5_OMR);\
    lp->cache.gep = (lp->fdx ? 0 : GEP_FDXD) | GEP_MODE;\
    gep_wr(lp->cache.gep, dev);\
  }\
}

#define SET_100Mb_PDET {\
  if ((lp->phy[lp->active].id) && (!lp->useSROM || lp->useMII)) {\
    mii_wr(MII_CR_100|MII_CR_ASSE, MII_CR, lp->phy[lp->active].addr, DE4X5_MII);\
    omr = (inl(DE4X5_OMR) & ~(OMR_TTM | OMR_PCS | OMR_SCR | OMR_FDX));\
    outl(omr, DE4X5_OMR);\
  } else if (lp->useSROM && !lp->useMII) {\
    omr = (inl(DE4X5_OMR) & ~(OMR_TTM | OMR_PCS | OMR_SCR | OMR_FDX));\
    outl(omr, DE4X5_OMR);\
  } else {\
    omr = (inl(DE4X5_OMR) & ~(OMR_PS | OMR_HBD | OMR_TTM | OMR_PCS | OMR_SCR | OMR_FDX));\
    outl(omr | OMR_SDP | OMR_PS | OMR_HBD | OMR_PCS, DE4X5_OMR);\
    lp->cache.gep = (GEP_FDXD | GEP_MODE);\
    gep_wr(lp->cache.gep, dev);\
  }\
}

#include <linux/sockios.h>

struct de4x5_ioctl {
	unsigned short cmd;                
	unsigned short len;                
	unsigned char  __user *data;       
};

#define DE4X5_GET_HWADDR	0x01 
#define DE4X5_SET_HWADDR	0x02 
#define DE4X5_SAY_BOO	        0x05 
#define DE4X5_GET_MCA   	0x06 
#define DE4X5_SET_MCA   	0x07 
#define DE4X5_CLR_MCA    	0x08 
#define DE4X5_MCA_EN    	0x09 
#define DE4X5_GET_STATS  	0x0a 
#define DE4X5_CLR_STATS 	0x0b 
#define DE4X5_GET_OMR           0x0c 
#define DE4X5_SET_OMR           0x0d 
#define DE4X5_GET_REG           0x0e 

#define MOTO_SROM_BUG    (lp->active == 8 && (get_unaligned_le32(dev->dev_addr) & 0x00ffffff) == 0x3e0008)
