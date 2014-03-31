/*
 * rocket_int.h --- internal header file for rocket.c
 *
 * Written by Theodore Ts'o, Copyright 1997.
 * Copyright 1997 Comtrol Corporation.  
 * 
 */

#define ROCKET_TYPE_NORMAL	0
#define ROCKET_TYPE_MODEM	1
#define ROCKET_TYPE_MODEMII	2
#define ROCKET_TYPE_MODEMIII	3
#define ROCKET_TYPE_PC104       4

#include <linux/mutex.h>

#include <asm/io.h>
#include <asm/byteorder.h>

typedef unsigned char Byte_t;
typedef unsigned int ByteIO_t;

typedef unsigned int Word_t;
typedef unsigned int WordIO_t;

typedef unsigned int DWordIO_t;


static inline void sOutB(unsigned short port, unsigned char value)
{
#ifdef ROCKET_DEBUG_IO
	printk(KERN_DEBUG "sOutB(%x, %x)...\n", port, value);
#endif
	outb_p(value, port);
}

static inline void sOutW(unsigned short port, unsigned short value)
{
#ifdef ROCKET_DEBUG_IO
	printk(KERN_DEBUG "sOutW(%x, %x)...\n", port, value);
#endif
	outw_p(value, port);
}

static inline void out32(unsigned short port, Byte_t *p)
{
	u32 value = get_unaligned_le32(p);
#ifdef ROCKET_DEBUG_IO
	printk(KERN_DEBUG "out32(%x, %lx)...\n", port, value);
#endif
	outl_p(value, port);
}

static inline unsigned char sInB(unsigned short port)
{
	return inb_p(port);
}

static inline unsigned short sInW(unsigned short port)
{
	return inw_p(port);
}

#define sOutStrW(port, addr, count) if (count) outsw(port, addr, count)
#define sInStrW(port, addr, count) if (count) insw(port, addr, count)

#define CTL_SIZE 8
#define AIOP_CTL_SIZE 4
#define CHAN_AIOP_SIZE 8
#define MAX_PORTS_PER_AIOP 8
#define MAX_AIOPS_PER_BOARD 4
#define MAX_PORTS_PER_BOARD 32

#define	isISA	0
#define	isPCI	1
#define	isMC	2

#define CTLID_NULL  -1		
#define CTLID_0001  0x0001	

#define AIOPID_NULL -1		
#define AIOPID_0001 0x0001	


#define _CMD_REG   0x38		
#define _INT_CHAN  0x39		
#define _INT_MASK  0x3A		
#define _UNUSED    0x3B		
#define _INDX_ADDR 0x3C		
#define _INDX_DATA 0x3E		

#define _TD0       0x00		
#define _RD0       0x00		
#define _CHN_STAT0 0x20		
#define _FIFO_CNT0 0x10		
#define _INT_ID0   0x30		

#define _TX_ENBLS  0x980	
#define _TXCMP1    0x988	
#define _TXCMP2    0x989	
#define _TXREP1B1  0x98A	
#define _TXREP1B2  0x98B	
#define _TXREP2    0x98C	

#define _RX_FIFO    0x000	
#define _TX_FIFO    0x800	
#define _RXF_OUTP   0x990	
#define _RXF_INP    0x992	
#define _TXF_OUTP   0x994	
#define _TXF_INP    0x995	
#define _TXP_CNT    0x996	
#define _TXP_PNTR   0x997	

#define PRI_PEND    0x80	
#define TXFIFO_SIZE 255		
#define RXFIFO_SIZE 1023	

#define _TXP_BUF    0x9C0	
#define TXP_SIZE    0x20	


#define _TX_CTRL    0xFF0	
#define _RX_CTRL    0xFF2	
#define _BAUD       0xFF4	
#define _CLK_PRE    0xFF6	

#define STMBREAK   0x08		
#define STMFRAME   0x04		
#define STMRCVROVR 0x02		
#define STMPARITY  0x01		
#define STMERROR   (STMBREAK | STMFRAME | STMPARITY)
#define STMBREAKH   0x800	
#define STMFRAMEH   0x400	
#define STMRCVROVRH 0x200	
#define STMPARITYH  0x100	
#define STMERRORH   (STMBREAKH | STMFRAMEH | STMPARITYH)

#define CTS_ACT   0x20		
#define DSR_ACT   0x10		
#define CD_ACT    0x08		
#define TXFIFOMT  0x04		
#define TXSHRMT   0x02		
#define RDA       0x01		
#define DRAINED (TXFIFOMT | TXSHRMT)	

#define STATMODE  0x8000	
#define RXFOVERFL 0x2000	
#define RX2MATCH  0x1000	
#define RX1MATCH  0x0800	
#define RXBREAK   0x0400	
#define RXFRAME   0x0200	
#define RXPARITY  0x0100	
#define STATERROR (RXBREAK | RXFRAME | RXPARITY)

#define CTSFC_EN  0x80		
#define RTSTOG_EN 0x40		
#define TXINT_EN  0x10		
#define STOP2     0x08		
#define PARITY_EN 0x04		
#define EVEN_PAR  0x02		
#define DATA8BIT  0x01		

#define SETBREAK  0x10		
#define LOCALLOOP 0x08		
#define SET_DTR   0x04		
#define SET_RTS   0x02		
#define TX_ENABLE 0x01		

#define RTSFC_EN  0x40		
#define RXPROC_EN 0x20		
#define TRIG_NO   0x00		
#define TRIG_1    0x08		
#define TRIG_1_2  0x10		
#define TRIG_7_8  0x18		
#define TRIG_MASK 0x18		
#define SRCINT_EN 0x04		
#define RXINT_EN  0x02		
#define MCINT_EN  0x01		

#define RXF_TRIG  0x20		
#define TXFIFO_MT 0x10		
#define SRC_INT   0x08		
#define DELTA_CD  0x04		
#define DELTA_CTS 0x02		
#define DELTA_DSR 0x01		

#define REP1W2_EN 0x10		
#define IGN2_EN   0x08		
#define IGN1_EN   0x04		
#define COMP2_EN  0x02		
#define COMP1_EN  0x01		

#define RESET_ALL 0x80		
#define TXOVERIDE 0x40		
#define RESETUART 0x20		
#define RESTXFCNT 0x10		
#define RESRXFCNT 0x08		

#define INTSTAT0  0x01		
#define INTSTAT1  0x02		
#define INTSTAT2  0x04		
#define INTSTAT3  0x08		

#define INTR_EN   0x08		
#define INT_STROB 0x04		


#define _CFG_INT_PCI  0x40
#define _PCI_INT_FUNC 0x3A

#define PCI_STROB 0x2000	
#define INTR_EN_PCI   0x0010	

#define _PCI_9030_INT_CTRL	0x4c          
#define _PCI_9030_GPIO_CTRL	0x54
#define PCI_INT_CTRL_AIOP	0x0001
#define PCI_GPIO_CTRL_8PORT	0x4000
#define _PCI_9030_RING_IND	0xc0          

#define CHAN3_EN  0x08		
#define CHAN2_EN  0x04		
#define CHAN1_EN  0x02		
#define CHAN0_EN  0x01		
#define FREQ_DIS  0x00
#define FREQ_274HZ 0x60
#define FREQ_137HZ 0x50
#define FREQ_69HZ  0x40
#define FREQ_34HZ  0x30
#define FREQ_17HZ  0x20
#define FREQ_9HZ   0x10
#define PERIODIC_ONLY 0x80	

#define CHANINT_EN 0x0100	

#define RDATASIZE 72
#define RREGDATASIZE 52

#define AIOP_INTR_BIT_0		0x0001
#define AIOP_INTR_BIT_1		0x0002
#define AIOP_INTR_BIT_2		0x0004
#define AIOP_INTR_BIT_3		0x0008

#define AIOP_INTR_BITS ( \
	AIOP_INTR_BIT_0 \
	| AIOP_INTR_BIT_1 \
	| AIOP_INTR_BIT_2 \
	| AIOP_INTR_BIT_3)

#define UPCI_AIOP_INTR_BIT_0	0x0004
#define UPCI_AIOP_INTR_BIT_1	0x0020
#define UPCI_AIOP_INTR_BIT_2	0x0100
#define UPCI_AIOP_INTR_BIT_3	0x0800

#define UPCI_AIOP_INTR_BITS ( \
	UPCI_AIOP_INTR_BIT_0 \
	| UPCI_AIOP_INTR_BIT_1 \
	| UPCI_AIOP_INTR_BIT_2 \
	| UPCI_AIOP_INTR_BIT_3)

typedef struct {
	int CtlID;
	int CtlNum;
	int BusType;
	int boardType;
	int isUPCI;
	WordIO_t PCIIO;
	WordIO_t PCIIO2;
	ByteIO_t MBaseIO;
	ByteIO_t MReg1IO;
	ByteIO_t MReg2IO;
	ByteIO_t MReg3IO;
	Byte_t MReg2;
	Byte_t MReg3;
	int NumAiop;
	int AltChanRingIndicator;
	ByteIO_t UPCIRingInd;
	WordIO_t AiopIO[AIOP_CTL_SIZE];
	ByteIO_t AiopIntChanIO[AIOP_CTL_SIZE];
	int AiopID[AIOP_CTL_SIZE];
	int AiopNumChan[AIOP_CTL_SIZE];
	Word_t *AiopIntrBits;
} CONTROLLER_T;

typedef CONTROLLER_T CONTROLLER_t;

typedef struct {
	CONTROLLER_T *CtlP;
	int AiopNum;
	int ChanID;
	int ChanNum;
	int rtsToggle;

	ByteIO_t Cmd;
	ByteIO_t IntChan;
	ByteIO_t IntMask;
	DWordIO_t IndexAddr;
	WordIO_t IndexData;

	WordIO_t TxRxData;
	WordIO_t ChanStat;
	WordIO_t TxRxCount;
	ByteIO_t IntID;

	Word_t TxFIFO;
	Word_t TxFIFOPtrs;
	Word_t RxFIFO;
	Word_t RxFIFOPtrs;
	Word_t TxPrioCnt;
	Word_t TxPrioPtr;
	Word_t TxPrioBuf;

	Byte_t R[RREGDATASIZE];

	Byte_t BaudDiv[4];
	Byte_t TxControl[4];
	Byte_t RxControl[4];
	Byte_t TxEnables[4];
	Byte_t TxCompare[4];
	Byte_t TxReplace1[4];
	Byte_t TxReplace2[4];
} CHANNEL_T;

typedef CHANNEL_T CHANNEL_t;
typedef CHANNEL_T *CHANPTR_T;

#define InterfaceModeRS232  0x00
#define InterfaceModeRS422  0x08
#define InterfaceModeRS485  0x10
#define InterfaceModeRS232T 0x18

#define sClrBreak(ChP) \
do { \
   (ChP)->TxControl[3] &= ~SETBREAK; \
   out32((ChP)->IndexAddr,(ChP)->TxControl); \
} while (0)

#define sClrDTR(ChP) \
do { \
   (ChP)->TxControl[3] &= ~SET_DTR; \
   out32((ChP)->IndexAddr,(ChP)->TxControl); \
} while (0)

#define sClrRTS(ChP) \
do { \
   if ((ChP)->rtsToggle) break; \
   (ChP)->TxControl[3] &= ~SET_RTS; \
   out32((ChP)->IndexAddr,(ChP)->TxControl); \
} while (0)

#define sClrTxXOFF(ChP) \
do { \
   sOutB((ChP)->Cmd,TXOVERIDE | (Byte_t)(ChP)->ChanNum); \
   sOutB((ChP)->Cmd,(Byte_t)(ChP)->ChanNum); \
} while (0)

#define sCtlNumToCtlPtr(CTLNUM) &sController[CTLNUM]

#define sControllerEOI(CTLP) sOutB((CTLP)->MReg2IO,(CTLP)->MReg2 | INT_STROB)

#define sPCIControllerEOI(CTLP) \
do { \
    if ((CTLP)->isUPCI) { \
	Word_t w = sInW((CTLP)->PCIIO); \
	sOutW((CTLP)->PCIIO, (w ^ PCI_INT_CTRL_AIOP)); \
	sOutW((CTLP)->PCIIO, w); \
    } \
    else { \
	sOutW((CTLP)->PCIIO, PCI_STROB); \
    } \
} while (0)

#define sDisAiop(CTLP,AIOPNUM) \
do { \
   (CTLP)->MReg3 &= sBitMapClrTbl[AIOPNUM]; \
   sOutB((CTLP)->MReg3IO,(CTLP)->MReg3); \
} while (0)

#define sDisCTSFlowCtl(ChP) \
do { \
   (ChP)->TxControl[2] &= ~CTSFC_EN; \
   out32((ChP)->IndexAddr,(ChP)->TxControl); \
} while (0)

#define sDisIXANY(ChP) \
do { \
   (ChP)->R[0x0e] = 0x86; \
   out32((ChP)->IndexAddr,&(ChP)->R[0x0c]); \
} while (0)

#define sDisParity(ChP) \
do { \
   (ChP)->TxControl[2] &= ~PARITY_EN; \
   out32((ChP)->IndexAddr,(ChP)->TxControl); \
} while (0)

#define sDisRTSToggle(ChP) \
do { \
   (ChP)->TxControl[2] &= ~RTSTOG_EN; \
   out32((ChP)->IndexAddr,(ChP)->TxControl); \
   (ChP)->rtsToggle = 0; \
} while (0)

#define sDisRxFIFO(ChP) \
do { \
   (ChP)->R[0x32] = 0x0a; \
   out32((ChP)->IndexAddr,&(ChP)->R[0x30]); \
} while (0)

#define sDisRxStatusMode(ChP) sOutW((ChP)->ChanStat,0)

#define sDisTransmit(ChP) \
do { \
   (ChP)->TxControl[3] &= ~TX_ENABLE; \
   out32((ChP)->IndexAddr,(ChP)->TxControl); \
} while (0)

#define sDisTxSoftFlowCtl(ChP) \
do { \
   (ChP)->R[0x06] = 0x8a; \
   out32((ChP)->IndexAddr,&(ChP)->R[0x04]); \
} while (0)

#define sEnAiop(CTLP,AIOPNUM) \
do { \
   (CTLP)->MReg3 |= sBitMapSetTbl[AIOPNUM]; \
   sOutB((CTLP)->MReg3IO,(CTLP)->MReg3); \
} while (0)

#define sEnCTSFlowCtl(ChP) \
do { \
   (ChP)->TxControl[2] |= CTSFC_EN; \
   out32((ChP)->IndexAddr,(ChP)->TxControl); \
} while (0)

#define sEnIXANY(ChP) \
do { \
   (ChP)->R[0x0e] = 0x21; \
   out32((ChP)->IndexAddr,&(ChP)->R[0x0c]); \
} while (0)

#define sEnParity(ChP) \
do { \
   (ChP)->TxControl[2] |= PARITY_EN; \
   out32((ChP)->IndexAddr,(ChP)->TxControl); \
} while (0)

#define sEnRTSToggle(ChP) \
do { \
   (ChP)->RxControl[2] &= ~RTSFC_EN; \
   out32((ChP)->IndexAddr,(ChP)->RxControl); \
   (ChP)->TxControl[2] |= RTSTOG_EN; \
   (ChP)->TxControl[3] &= ~SET_RTS; \
   out32((ChP)->IndexAddr,(ChP)->TxControl); \
   (ChP)->rtsToggle = 1; \
} while (0)

#define sEnRxFIFO(ChP) \
do { \
   (ChP)->R[0x32] = 0x08; \
   out32((ChP)->IndexAddr,&(ChP)->R[0x30]); \
} while (0)

#define sEnRxProcessor(ChP) \
do { \
   (ChP)->RxControl[2] |= RXPROC_EN; \
   out32((ChP)->IndexAddr,(ChP)->RxControl); \
} while (0)

#define sEnRxStatusMode(ChP) sOutW((ChP)->ChanStat,STATMODE)

#define sEnTransmit(ChP) \
do { \
   (ChP)->TxControl[3] |= TX_ENABLE; \
   out32((ChP)->IndexAddr,(ChP)->TxControl); \
} while (0)

#define sEnTxSoftFlowCtl(ChP) \
do { \
   (ChP)->R[0x06] = 0xc5; \
   out32((ChP)->IndexAddr,&(ChP)->R[0x04]); \
} while (0)

#define sGetAiopIntStatus(CTLP,AIOPNUM) sInB((CTLP)->AiopIntChanIO[AIOPNUM])

#define sGetAiopNumChan(CTLP,AIOPNUM) (CTLP)->AiopNumChan[AIOPNUM]

#define sGetChanIntID(ChP) (sInB((ChP)->IntID) & (RXF_TRIG | TXFIFO_MT | SRC_INT | DELTA_CD | DELTA_CTS | DELTA_DSR))

#define sGetChanNum(ChP) (ChP)->ChanNum

#define sGetChanStatus(ChP) sInW((ChP)->ChanStat)

#define sGetChanStatusLo(ChP) sInB((ByteIO_t)(ChP)->ChanStat)

#if 0
#define sGetChanRI(ChP) ((ChP)->CtlP->AltChanRingIndicator ? \
                          (sInB((ByteIO_t)((ChP)->ChanStat+8)) & DSR_ACT) : \
                            (((ChP)->CtlP->boardType == ROCKET_TYPE_PC104) ? \
                               (!(sInB((ChP)->CtlP->AiopIO[3]) & sBitMapSetTbl[(ChP)->ChanNum])) : \
                             0))
#endif

#define sGetControllerIntStatus(CTLP) (sInB((CTLP)->MReg1IO) & 0x0f)

#define sPCIGetControllerIntStatus(CTLP) \
	((CTLP)->isUPCI ? \
	  (sInW((CTLP)->PCIIO2) & UPCI_AIOP_INTR_BITS) : \
	  ((sInW((CTLP)->PCIIO) >> 8) & AIOP_INTR_BITS))

#define sGetRxCnt(ChP) sInW((ChP)->TxRxCount)

#define sGetTxCnt(ChP) sInB((ByteIO_t)(ChP)->TxRxCount)

#define sGetTxRxDataIO(ChP) (ChP)->TxRxData

#define sInitChanDefaults(ChP) \
do { \
   (ChP)->CtlP = NULLCTLPTR; \
   (ChP)->AiopNum = NULLAIOP; \
   (ChP)->ChanID = AIOPID_NULL; \
   (ChP)->ChanNum = NULLCHAN; \
} while (0)

#define sResetAiopByNum(CTLP,AIOPNUM) \
do { \
   sOutB((CTLP)->AiopIO[(AIOPNUM)]+_CMD_REG,RESET_ALL); \
   sOutB((CTLP)->AiopIO[(AIOPNUM)]+_CMD_REG,0x0); \
} while (0)

#define sSendBreak(ChP) \
do { \
   (ChP)->TxControl[3] |= SETBREAK; \
   out32((ChP)->IndexAddr,(ChP)->TxControl); \
} while (0)

#define sSetBaud(ChP,DIVISOR) \
do { \
   (ChP)->BaudDiv[2] = (Byte_t)(DIVISOR); \
   (ChP)->BaudDiv[3] = (Byte_t)((DIVISOR) >> 8); \
   out32((ChP)->IndexAddr,(ChP)->BaudDiv); \
} while (0)

#define sSetData7(ChP) \
do { \
   (ChP)->TxControl[2] &= ~DATA8BIT; \
   out32((ChP)->IndexAddr,(ChP)->TxControl); \
} while (0)

#define sSetData8(ChP) \
do { \
   (ChP)->TxControl[2] |= DATA8BIT; \
   out32((ChP)->IndexAddr,(ChP)->TxControl); \
} while (0)

#define sSetDTR(ChP) \
do { \
   (ChP)->TxControl[3] |= SET_DTR; \
   out32((ChP)->IndexAddr,(ChP)->TxControl); \
} while (0)

#define sSetEvenParity(ChP) \
do { \
   (ChP)->TxControl[2] |= EVEN_PAR; \
   out32((ChP)->IndexAddr,(ChP)->TxControl); \
} while (0)

#define sSetOddParity(ChP) \
do { \
   (ChP)->TxControl[2] &= ~EVEN_PAR; \
   out32((ChP)->IndexAddr,(ChP)->TxControl); \
} while (0)

#define sSetRTS(ChP) \
do { \
   if ((ChP)->rtsToggle) break; \
   (ChP)->TxControl[3] |= SET_RTS; \
   out32((ChP)->IndexAddr,(ChP)->TxControl); \
} while (0)

#define sSetRxTrigger(ChP,LEVEL) \
do { \
   (ChP)->RxControl[2] &= ~TRIG_MASK; \
   (ChP)->RxControl[2] |= LEVEL; \
   out32((ChP)->IndexAddr,(ChP)->RxControl); \
} while (0)

#define sSetStop1(ChP) \
do { \
   (ChP)->TxControl[2] &= ~STOP2; \
   out32((ChP)->IndexAddr,(ChP)->TxControl); \
} while (0)

#define sSetStop2(ChP) \
do { \
   (ChP)->TxControl[2] |= STOP2; \
   out32((ChP)->IndexAddr,(ChP)->TxControl); \
} while (0)

#define sSetTxXOFFChar(ChP,CH) \
do { \
   (ChP)->R[0x07] = (CH); \
   out32((ChP)->IndexAddr,&(ChP)->R[0x04]); \
} while (0)

#define sSetTxXONChar(ChP,CH) \
do { \
   (ChP)->R[0x0b] = (CH); \
   out32((ChP)->IndexAddr,&(ChP)->R[0x08]); \
} while (0)

#define sStartRxProcessor(ChP) out32((ChP)->IndexAddr,&(ChP)->R[0])

#define sWriteTxByte(IO,DATA) sOutB(IO,DATA)

/*
 * Begin Linux specific definitions for the Rocketport driver
 *
 * This code is Copyright Theodore Ts'o, 1995-1997
 */

struct r_port {
	int magic;
	struct tty_port port;
	int line;
	int flags;		
	unsigned int board:3;
	unsigned int aiop:2;
	unsigned int chan:3;
	CONTROLLER_t *ctlp;
	CHANNEL_t channel;
	int intmask;
	int xmit_fifo_room;	
	unsigned char *xmit_buf;
	int xmit_head;
	int xmit_tail;
	int xmit_cnt;
	int cd_status;
	int ignore_status_mask;
	int read_status_mask;
	int cps;

	struct completion close_wait;	
	spinlock_t slock;
	struct mutex write_mtx;
};

#define RPORT_MAGIC 0x525001

#define NUM_BOARDS 8
#define MAX_RP_PORTS (32*NUM_BOARDS)

#define XMIT_BUF_SIZE 4096

#define WAKEUP_CHARS 256

#define TTY_ROCKET_MAJOR	46
#define CUA_ROCKET_MAJOR	47

#ifdef PCI_VENDOR_ID_RP
#undef PCI_VENDOR_ID_RP
#undef PCI_DEVICE_ID_RP8OCTA
#undef PCI_DEVICE_ID_RP8INTF
#undef PCI_DEVICE_ID_RP16INTF
#undef PCI_DEVICE_ID_RP32INTF
#undef PCI_DEVICE_ID_URP8OCTA
#undef PCI_DEVICE_ID_URP8INTF
#undef PCI_DEVICE_ID_URP16INTF
#undef PCI_DEVICE_ID_CRP16INTF
#undef PCI_DEVICE_ID_URP32INTF
#endif

#define PCI_VENDOR_ID_RP		0x11fe

#define PCI_DEVICE_ID_RP32INTF		0x0001	
#define PCI_DEVICE_ID_RP8INTF		0x0002	
#define PCI_DEVICE_ID_RP16INTF		0x0003	
#define PCI_DEVICE_ID_RP4QUAD		0x0004	
#define PCI_DEVICE_ID_RP8OCTA		0x0005	
#define PCI_DEVICE_ID_RP8J		0x0006	
#define PCI_DEVICE_ID_RP4J		0x0007	
#define PCI_DEVICE_ID_RP8SNI		0x0008	
#define PCI_DEVICE_ID_RP16SNI		0x0009	
#define PCI_DEVICE_ID_RPP4		0x000A	
#define PCI_DEVICE_ID_RPP8		0x000B	
#define PCI_DEVICE_ID_RP6M		0x000C	
#define PCI_DEVICE_ID_RP4M		0x000D	
#define PCI_DEVICE_ID_RP2_232           0x000E	
#define PCI_DEVICE_ID_RP2_422           0x000F	 

#define PCI_DEVICE_ID_URP32INTF		0x0801	 
#define PCI_DEVICE_ID_URP8INTF		0x0802	
#define PCI_DEVICE_ID_URP16INTF		0x0803	
#define PCI_DEVICE_ID_URP8OCTA		0x0805	
#define PCI_DEVICE_ID_UPCI_RM3_8PORT    0x080C	
#define PCI_DEVICE_ID_UPCI_RM3_4PORT    0x080D	

 
#define PCI_DEVICE_ID_CRP16INTF		0x0903	

