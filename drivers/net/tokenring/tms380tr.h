
#ifndef __LINUX_TMS380TR_H
#define __LINUX_TMS380TR_H

#ifdef __KERNEL__

#include <linux/interrupt.h>

extern const struct net_device_ops tms380tr_netdev_ops;
int tms380tr_open(struct net_device *dev);
int tms380tr_close(struct net_device *dev);
irqreturn_t tms380tr_interrupt(int irq, void *dev_id);
int tmsdev_init(struct net_device *dev, struct device *pdev);
void tmsdev_term(struct net_device *dev);
void tms380tr_wait(unsigned long time);

#define TMS380TR_MAX_ADAPTERS 7

#define SEND_TIMEOUT 10*HZ

#define TR_RCF_LONGEST_FRAME_MASK 0x0070
#define TR_RCF_FRAME4K 0x0030


#define SWAPW(x) (((x) << 16) | ((x) >> 16))

#define LOBYTE(w)       ((unsigned char)(w))

#define HIBYTE(w)       ((unsigned char)((unsigned short)(w) >> 8))

#define LOWORD(l)       ((unsigned short)(l))

#define HIWORD(l)       ((unsigned short)((unsigned long)(l) >> 16))




#define SIFDAT      		0x00	
#define SIFINC      		0x02  	
#define SIFINH      		0x03  	
#define SIFADR      		0x04  	
#define SIFCMD      		0x06  	
#define SIFSTS      		0x06  	

#define SIFACL      		0x08  	
#define SIFADD      		0x0a 	
#define SIFADX      		0x0c     
#define DMALEN      		0x0e 	

#define POSREG      		0x10 	
#define POSREG_2    		24L 	

#define CMD_INTERRUPT_ADAPTER   0x8000  
#define CMD_ADAPTER_RESET   	0x4000  
#define CMD_SSB_CLEAR		0x2000  
#define CMD_EXECUTE		0x1000	
#define CMD_SCB_REQUEST		0x0800  
#define CMD_RX_CONTINUE		0x0400  
#define CMD_RX_VALID		0x0200  
#define CMD_TX_VALID		0x0100  
#define CMD_SYSTEM_IRQ		0x0080  
#define CMD_CLEAR_SYSTEM_IRQ	0x0080	
#define EXEC_SOFT_RESET		0xFF00  


#define ACL_SWHLDA		0x0800  
#define ACL_SWDDIR		0x0400  
#define ACL_SWHRQ		0x0200  
#define ACL_PSDMAEN		0x0100  
#define ACL_ARESET		0x0080  
#define ACL_CPHALT		0x0040  
#define ACL_BOOT		0x0020
#define ACL_SINTEN		0x0008  /* System interrupt enable/disable
					 * (1/0): can be written if ACL_ARESET
					 * is zero.
					 */
#define ACL_PEN                 0x0004

#define ACL_NSELOUT0            0x0002 
#define ACL_NSELOUT1            0x0001	

#define PS_DMA_MASK		(ACL_SWHRQ | ACL_PSDMAEN)


#define STS_SYSTEM_IRQ		0x0080	
#define STS_INITIALIZE		0x0040  
#define STS_TEST		0x0020  
#define STS_ERROR		0x0010  
#define STS_MASK		0x00F0  
#define STS_ERROR_MASK		0x000F  
#define ADAPTER_INT_PTRS	0x0A00  


#define STS_IRQ_ADAPTER_CHECK	0x0000	
 
#define STS_IRQ_RING_STATUS	0x0004  
#define STS_IRQ_LLC_STATUS	0x0005	
#define STS_IRQ_SCB_CLEAR	0x0006	
#define STS_IRQ_TIMER		0x0007	
#define STS_IRQ_COMMAND_STATUS	0x0008	
 
#define STS_IRQ_RECEIVE_STATUS	0x000A	
#define STS_IRQ_TRANSMIT_STATUS	0x000C	
#define STS_IRQ_RECEIVE_PENDING	0x000E	
#define STS_IRQ_MASK		0x000F	


#define COMMAND_COMPLETE	0x0080	
#define FRAME_COMPLETE		0x0040	
#define LIST_ERROR		0x0020	
#define FRAME_SIZE_ERROR	0x8000	
#define TX_THRESHOLD		0x4000	
#define ODD_ADDRESS		0x2000	
#define FRAME_ERROR		0x1000	
#define ACCESS_PRIORITY_ERROR	0x0800	
#define UNENABLED_MAC_FRAME	0x0400	
#define ILLEGAL_FRAME_FORMAT	0x0200	


#define OC_OPEN			0x0001	
#define OC_TRANSMIT		0x0002	
#define OC_TRANSMIT_HALT	0x0004	
#define OC_RECEIVE		0x0008	
#define OC_CLOSE		0x0010	
#define OC_SET_GROUP_ADDR	0x0020	
#define OC_SET_FUNCT_ADDR	0x0040	
#define OC_READ_ERROR_LOG	0x0080	
#define OC_READ_ADAPTER		0x0100	
#define OC_MODIFY_OPEN_PARMS	0x0400	
#define OC_RESTORE_OPEN_PARMS	0x0800	
#define OC_SET_FIRST_16_GROUP	0x1000	
#define OC_SET_BRIDGE_PARMS	0x2000	
#define OC_CONFIG_BRIDGE_PARMS	0x4000	

#define OPEN			0x0300	
#define TRANSMIT		0x0400	
#define TRANSMIT_HALT		0x0500	
#define RECEIVE			0x0600	
#define CLOSE			0x0700	
#define SET_GROUP_ADDR		0x0800	
#define SET_FUNCT_ADDR		0x0900	
#define READ_ERROR_LOG		0x0A00	
#define READ_ADAPTER		0x0B00	
#define MODIFY_OPEN_PARMS	0x0D00	
#define RESTORE_OPEN_PARMS	0x0E00	
#define SET_FIRST_16_GROUP	0x0F00	
#define SET_BRIDGE_PARMS	0x1000	
#define CONFIG_BRIDGE_PARMS	0x1100	

#define SPEED_4			4
#define SPEED_16		16	


#define BURST_SIZE	0x0018	
#define BURST_MODE	0x9F00	
#define DMA_RETRIES	0x0505	

#define CYCLE_TIME	3	
#define LINE_SPEED_BIT	0x80

#define ONE_SECOND_TICKS	1000000
#define HALF_SECOND		(ONE_SECOND_TICKS / 2)
#define ONE_SECOND		(ONE_SECOND_TICKS)
#define TWO_SECONDS		(ONE_SECOND_TICKS * 2)
#define THREE_SECONDS		(ONE_SECOND_TICKS * 3)
#define FOUR_SECONDS		(ONE_SECOND_TICKS * 4)
#define FIVE_SECONDS		(ONE_SECOND_TICKS * 5)

#define BUFFER_SIZE 		2048	

#pragma pack(1)
typedef struct {
	unsigned short Init_Options;	

	
	u_int8_t  CMD_Status_IV;    
	u_int8_t  TX_IV;	    
	u_int8_t  RX_IV;	    
	u_int8_t  Ring_Status_IV;   
	u_int8_t  SCB_Clear_IV;	    
	u_int8_t  Adapter_CHK_IV;   

	u_int16_t RX_Burst_Size;    
	u_int16_t TX_Burst_Size;    
	u_int16_t DMA_Abort_Thrhld; 

	u_int32_t SCB_Addr;   
	u_int32_t SSB_Addr;   
} IPB, *IPB_Ptr;
#pragma pack()

#define BUFFER_SIZE	2048		
#define TPL_SIZE	8+6*TX_FRAG_NUM 
#define RPL_SIZE	14		
#define TX_BUF_MIN	20		
#define TX_BUF_MAX	40		
#define DISABLE_EARLY_TOKEN_RELEASE 	0x1000

#define WRAP_INTERFACE		0x0080	
#define DISABLE_HARD_ERROR	0x0040	
#define DISABLE_SOFT_ERROR	0x0020	
#define PASS_ADAPTER_MAC_FRAMES	0x0010	
#define PASS_ATTENTION_FRAMES	0x0008	
#define PAD_ROUTING_FIELD	0x0004	
#define FRAME_HOLD		0x0002	
#define CONTENDER		0x0001	
#define PASS_BEACON_MAC_FRAMES	0x8000	
#define EARLY_TOKEN_RELEASE 	0x1000	
#define COPY_ALL_MAC_FRAMES	0x0400	
#define COPY_ALL_NON_MAC_FRAMES	0x0200	
#define PASS_FIRST_BUF_ONLY	0x0100	
#define ENABLE_FULL_DUPLEX_SELECTION	0x2000 

#define OPEN_FULL_DUPLEX_OFF	0x0000
#define OPEN_FULL_DUPLEX_ON	0x00c0
#define OPEN_FULL_DUPLEX_AUTO	0x0080

#define PROD_ID_SIZE	18	

#define TX_FRAG_NUM	3	 
#define TX_MORE_FRAGMENTS 0x8000 

#define ISA_MAX_ADDRESS 	0x00ffffff
#define PCI_MAX_ADDRESS		0xffffffff

#pragma pack(1)
typedef struct {
	u_int16_t OPENOptions;
	u_int8_t  NodeAddr[6];	
	u_int32_t GroupAddr;	
	u_int32_t FunctAddr;	
	__be16 RxListSize;	
	__be16 TxListSize;	
	__be16 BufSize;		
	u_int16_t FullDuplex;
	u_int16_t Reserved;
	u_int8_t  TXBufMin;	
	u_int8_t  TXBufMax;	
	u_int16_t ProdIDAddr[2];
} OPB, *OPB_Ptr;
#pragma pack()

#pragma pack(1)
typedef struct {
	u_int16_t CMD;		
	u_int16_t Parm[2];	
} SCB;	
#pragma pack()

#pragma pack(1)
typedef struct {
	u_int16_t STS;		
	u_int16_t Parm[3];	
} SSB;	
#pragma pack()

typedef struct {
	unsigned short BurnedInAddrPtr;	
	unsigned short SoftwareLevelPtr;
	unsigned short AdapterAddrPtr;	
	unsigned short AdapterParmsPtr;	
	unsigned short MACBufferPtr;	
	unsigned short LLCCountersPtr;	
	unsigned short SpeedFlagPtr;	
	unsigned short AdapterRAMPtr;	
} INTPTRS;	

#pragma pack(1)
typedef struct {
	u_int8_t  Line_Error;		
	u_int8_t  Internal_Error;	
	u_int8_t  Burst_Error;
	u_int8_t  ARI_FCI_Error;	
	u_int8_t  AbortDelimeters;	
	u_int8_t  Reserved_3;
	u_int8_t  Lost_Frame_Error;	
	u_int8_t  Rx_Congest_Error;	
	u_int8_t  Frame_Copied_Error;	
	u_int8_t  Frequency_Error;	
	u_int8_t  Token_Error;		
	u_int8_t  Reserved_5;
	u_int8_t  DMA_Bus_Error;	
	u_int8_t  DMA_Parity_Error;	
} ERRORTAB;	
#pragma pack()


#pragma pack(1)
typedef struct {
	__be16 DataCount;	
	__be32 DataAddr;	
} Fragment;
#pragma pack()

#define MAX_FRAG_NUMBERS    9	

#define HEADER_SIZE		(1 + 1 + 6 + 6)
#define SRC_SIZE		18
#define MIN_DATA_SIZE		516
#define DEFAULT_DATA_SIZE	4472
#define MAX_DATA_SIZE		17800

#define DEFAULT_PACKET_SIZE (HEADER_SIZE + SRC_SIZE + DEFAULT_DATA_SIZE)
#define MIN_PACKET_SIZE     (HEADER_SIZE + SRC_SIZE + MIN_DATA_SIZE)
#define MAX_PACKET_SIZE     (HEADER_SIZE + SRC_SIZE + MAX_DATA_SIZE)

#define AC_NOT_RECOGNIZED	0x00
#define GROUP_BIT		0x80
#define GET_TRANSMIT_STATUS_HIGH_BYTE(Ts) ((unsigned char)((Ts) >> 8))
#define GET_FRAME_STATUS_HIGH_AC(Fs)	  ((unsigned char)(((Fs) & 0xC0) >> 6))
#define GET_FRAME_STATUS_LOW_AC(Fs)       ((unsigned char)(((Fs) & 0x0C) >> 2))
#define DIRECTED_FRAME(Context)           (!((Context)->MData[2] & GROUP_BIT))



#define TX_VALID		0x0080	
#define TX_FRAME_COMPLETE	0x0040	
#define TX_START_FRAME		0x0020  
#define TX_END_FRAME		0x0010  
#define TX_FRAME_IRQ		0x0008  
#define TX_ERROR		0x0004  
#define TX_INTERFRAME_WAIT	0x0004
#define TX_PASS_CRC		0x0002  
#define TX_PASS_SRC_ADDR	0x0001  
#define TX_STRIP_FS		0xFF00  

#define TPL_NUM		3	

#pragma pack(1)
typedef struct s_TPL TPL;

struct s_TPL {	
	__be32 NextTPLAddr;		
	volatile u_int16_t Status;	
	__be16 FrameSize;		

	Fragment FragList[TX_FRAG_NUM];	
#pragma pack()

	

	TPL *NextTPLPtr;		
	unsigned char *MData;
	struct sk_buff *Skb;
	unsigned char TPLIndex;
	volatile unsigned char BusyFlag;
	dma_addr_t DMABuff;		
};

#define RX_VALID		0x0080	
#define RX_FRAME_COMPLETE	0x0040  
#define RX_START_FRAME		0x0020  
#define RX_END_FRAME		0x0010  
#define RX_FRAME_IRQ		0x0008  
#define RX_INTERFRAME_WAIT	0x0004  
#define RX_PASS_CRC		0x0002  
#define RX_PASS_SRC_ADDR	0x0001  
#define RX_RECEIVE_FS		0xFC00  
#define RX_ADDR_MATCH		0x0300  
 
#define RX_STATUS_MASK		0x00FF  

#define RX_INTERN_ADDR_MATCH    0x0100  
#define RX_EXTERN_ADDR_MATCH    0x0200  
#define RX_INTEXT_ADDR_MATCH    0x0300  
#define RX_READY (RX_VALID | RX_FRAME_IRQ) 

#define ILLEGAL_COMMAND		0x0080	
#define ADDRESS_ERROR		0x0040  
#define ADAPTER_OPEN		0x0020  
#define ADAPTER_CLOSE		0x0010  
#define SAME_COMMAND		0x0008  

#define NODE_ADDR_ERROR		0x0040  
#define LIST_SIZE_ERROR		0x0020  
#define BUF_SIZE_ERROR		0x0010  
#define TX_BUF_COUNT_ERROR	0x0004  
#define OPEN_ERROR		0x0002	

#define GOOD_COMPLETION		0x0080  
#define INVALID_OPEN_OPTION	0x0001  

#define OPEN_PHASES_MASK            0xF000  
#define LOBE_MEDIA_TEST             0x1000
#define PHYSICAL_INSERTION          0x2000
#define ADDRESS_VERIFICATION        0x3000
#define PARTICIPATION_IN_RING_POLL  0x4000
#define REQUEST_INITIALISATION      0x5000
#define FULLDUPLEX_CHECK            0x6000

#define OPEN_ERROR_CODES_MASK	0x0F00  
#define OPEN_FUNCTION_FAILURE   0x0100  
#define OPEN_SIGNAL_LOSS	0x0200	
#define OPEN_TIMEOUT		0x0500	
#define OPEN_RING_FAILURE	0x0600	
#define OPEN_RING_BEACONING	0x0700	
#define OPEN_DUPLICATE_NODEADDR	0x0800  
#define OPEN_REQUEST_INIT	0x0900	
#define OPEN_REMOVE_RECEIVED    0x0A00  
#define OPEN_FULLDUPLEX_SET	0x0D00	

#define BRIDGE_INVALID_MAX_LEN  0x4000  
#define BRIDGE_INVALID_SRC_RING 0x2000  
#define BRIDGE_INVALID_TRG_RING 0x1000  
#define BRIDGE_INVALID_BRDGE_NO 0x0800  
#define BRIDGE_INVALID_OPTIONS  0x0400  
#define BRIDGE_DIAGS_FAILED     0x0200  
#define BRIDGE_NO_SRA           0x0100  

#define BUD_INITIAL_ERROR       0x0
#define BUD_CHECKSUM_ERROR      0x1
#define BUD_ADAPTER_RAM_ERROR   0x2
#define BUD_INSTRUCTION_ERROR   0x3
#define BUD_CONTEXT_ERROR       0x4
#define BUD_PROTOCOL_ERROR      0x5
#define BUD_INTERFACE_ERROR	0x6

#define BUD_MAX_RETRIES         3
#define BUD_MAX_LOOPCNT         6
#define BUD_TIMEOUT             3000

#define INIT_MAX_RETRIES        3	
#define INIT_MAX_LOOPCNT        22      

#define SIGNAL_LOSS             0x0080  
#define HARD_ERROR              0x0040  
#define SOFT_ERROR              0x0020  
#define TRANSMIT_BEACON         0x0010  
#define LOBE_WIRE_FAULT         0x0008  
#define AUTO_REMOVAL_ERROR      0x0004  
#define REMOVE_RECEIVED         0x0001  
#define COUNTER_OVERFLOW        0x8000  
#define SINGLE_STATION          0x4000  
#define RING_RECOVERY           0x2000  

#define ADAPTER_CLOSED (LOBE_WIRE_FAULT | AUTO_REMOVAL_ERROR |\
                        REMOVE_RECEIVED)

#define DIO_PARITY              0x8000  
#define DMA_READ_ABORT          0x4000  
#define DMA_WRITE_ABORT         0x2000  
#define ILLEGAL_OP_CODE         0x1000  
#define PARITY_ERRORS           0x0800  
#define RAM_DATA_ERROR          0x0080  
#define RAM_PARITY_ERROR        0x0040  
#define RING_UNDERRUN           0x0020  
#define INVALID_IRQ             0x0008  
#define INVALID_ERROR_IRQ       0x0004  
#define INVALID_XOP             0x0002  
#define CHECKADDR               0x05E0  
#define ROM_PAGE_0              0x0000  

#define RX_COMPLETE             0x0080  
#define RX_SUSPENDED            0x0040  

#define RX_FRAME_CONTROL_BITS (RX_VALID | RX_START_FRAME | RX_END_FRAME | \
			       RX_FRAME_COMPLETE)
#define VALID_SINGLE_BUFFER_FRAME (RX_START_FRAME | RX_END_FRAME | \
				   RX_FRAME_COMPLETE)

typedef enum SKB_STAT SKB_STAT;
enum SKB_STAT {
	SKB_UNAVAILABLE,
	SKB_DMA_DIRECT,
	SKB_DATA_COPY
};

#define RPL_NUM		3

#define RX_FRAG_NUM     1	

#pragma pack(1)
typedef struct s_RPL RPL;
struct s_RPL {	
	__be32 NextRPLAddr;		
	volatile u_int16_t Status;	
	volatile __be16 FrameSize;	 

	Fragment FragList[RX_FRAG_NUM];	
#pragma pack()

	
	RPL *NextRPLPtr;	
	unsigned char *MData;
	struct sk_buff *Skb;
	SKB_STAT SkbStat;
	int RPLIndex;
	dma_addr_t DMABuff;		
};

typedef struct net_local {
#pragma pack(1)
	IPB ipb;	
	SCB scb;	
	SSB ssb;	
	OPB ocpl;	

	ERRORTAB errorlogtable;	
	unsigned char ProductID[PROD_ID_SIZE + 1]; 
#pragma pack()

	TPL Tpl[TPL_NUM];
	TPL *TplFree;
	TPL *TplBusy;
	unsigned char LocalTxBuffers[TPL_NUM][DEFAULT_PACKET_SIZE];

	RPL Rpl[RPL_NUM];
	RPL *RplHead;
	RPL *RplTail;
	unsigned char LocalRxBuffers[RPL_NUM][DEFAULT_PACKET_SIZE];

	struct device *pdev;
	int DataRate;
	unsigned char ScbInUse;
	unsigned short CMDqueue;

	unsigned long AdapterOpenFlag:1;
	unsigned long AdapterVirtOpenFlag:1;
	unsigned long OpenCommandIssued:1;
	unsigned long TransmitCommandActive:1;
	unsigned long TransmitHaltScheduled:1;
	unsigned long HaltInProgress:1;
	unsigned long LobeWireFaultLogged:1;
	unsigned long ReOpenInProgress:1;
	unsigned long Sleeping:1;

	unsigned long LastOpenStatus;
	unsigned short CurrentRingStatus;
	unsigned long MaxPacketSize;
	
	unsigned long StartTime;
	unsigned long LastSendTime;

	struct tr_statistics MacStat;	

	unsigned long dmalimit; 
	dma_addr_t    dmabuffer; 

	struct timer_list timer;

	wait_queue_head_t  wait_for_tok_int;

	INTPTRS intptrs;	
	unsigned short (*setnselout)(struct net_device *);
	unsigned short (*sifreadb)(struct net_device *, unsigned short);
	void (*sifwriteb)(struct net_device *, unsigned short, unsigned short);
	unsigned short (*sifreadw)(struct net_device *, unsigned short);
	void (*sifwritew)(struct net_device *, unsigned short, unsigned short);

	spinlock_t lock;                
	void *tmspriv;
} NET_LOCAL;

#endif	
#endif	
