#ifndef _DEBUG_H
#define _DEBUG_H
#include <linux/string.h>
#define NONE 0xFFFF



#define DBG_TYPE_INITEXIT		(1 << 0)	
#define DBG_TYPE_TX				(1 << 1)	
#define DBG_TYPE_RX				(1 << 2)	
#define DBG_TYPE_OTHERS			(1 << 3)	
#define NUMTYPES			4		


#define TX 			1
#define MP_SEND  	(TX<<0)
#define NEXT_SEND   (TX<<1)
#define TX_FIFO  	(TX<<2)
#define TX_CONTROL 	(TX<<3)

#define IP_ADDR  	(TX<<4)
#define ARP_REQ  	(TX<<5)
#define ARP_RESP 	(TX<<6)


#define TOKEN_COUNTS (TX<<8)
#define CHECK_TOKENS (TX<<9)
#define TX_PACKETS   (TX<<10)
#define TIMER  		 (TX<<11)

#define QOS TX
#define QUEUE_INDEX (QOS<<12)
#define IPV4_DBG 	(QOS<<13)
#define IPV6_DBG 	(QOS<<14)
#define PRUNE_QUEUE (QOS<<15)
#define SEND_QUEUE 	(QOS<<16)

#define TX_OSAL_DBG (TX<<17)


#define MP 1
#define DRV_ENTRY 	(MP<<0)
#define MP_INIT  	(MP<<1)
#define READ_REG 	(MP<<3)
#define DISPATCH 	(MP<<2)
#define CLAIM_ADAP 	(MP<<4)
#define REG_IO_PORT (MP<<5)
#define INIT_DISP 	(MP<<6)
#define RX_INIT  	(MP<<7)


#define RX 1
#define RX_DPC  	(RX<<0)
#define RX_CTRL 	(RX<<3)
#define RX_DATA 	(RX<<4)
#define MP_RETURN 	(RX<<1)
#define LINK_MSG 	(RX<<2)


#define OTHERS 1

#define ISR OTHERS
#define MP_DPC  (ISR<<0)

#define HALT OTHERS
#define MP_HALT  		(HALT<<1)
#define CHECK_HANG 		(HALT<<2)
#define MP_RESET 		(HALT<<3)
#define MP_SHUTDOWN 	(HALT<<4)

#define PNP OTHERS
#define MP_PNP  		(PNP<<5)

#define MISC OTHERS
#define DUMP_INFO 		(MISC<<6)
#define CLASSIFY 		(MISC<<7)
#define LINK_UP_MSG 	(MISC<<8)
#define CP_CTRL_PKT 	(MISC<<9)
#define DUMP_CONTROL 	(MISC<<10)
#define LED_DUMP_INFO 	(MISC<<11)

#define CMHOST OTHERS


#define SERIAL  		(OTHERS<<12)
#define IDLE_MODE 		(OTHERS<<13)

#define WRM   			(OTHERS<<14)
#define RDM   			(OTHERS<<15)

#define PHS_SEND    	(OTHERS<<16)
#define PHS_RECEIVE 	(OTHERS<<17)
#define PHS_MODULE 	    (OTHERS<<18)

#define INTF_INIT    	(OTHERS<<19)
#define INTF_ERR     	(OTHERS<<20)
#define INTF_WARN    	(OTHERS<<21)
#define INTF_NORM 		(OTHERS<<22)

#define IRP_COMPLETION 	(OTHERS<<23)
#define SF_DESCRIPTOR_CNTS (OTHERS<<24)
#define PHS_DISPATCH 	(OTHERS << 25)
#define OSAL_DBG 		(OTHERS << 26)
#define NVM_RW      	(OTHERS << 27)

#define HOST_MIBS   	(OTHERS << 28)
#define CONN_MSG    	(CMHOST << 29)


#define BCM_ALL			7
#define	BCM_LOW			6
#define	BCM_PRINT		5
#define	BCM_NORMAL		4
#define	BCM_MEDIUM		3
#define	BCM_SCREAM		2
#define	BCM_ERR			1
#define	BCM_NONE		0


#define DBG_LVL_CURR	(BCM_ALL)
#define DBG_LVL_ALL		BCM_ALL

typedef struct
{
	unsigned int Subtype, Type;
	unsigned int OnOff;
} __attribute__((packed)) USER_BCM_DBG_STATE;

typedef struct _S_BCM_DEBUG_STATE {
	UINT type;
	UINT subtype[(NUMTYPES*2)+1];
	UINT debug_level;
} S_BCM_DEBUG_STATE;
#define DBG_NO_FUNC_PRINT	1 << 31
#define DBG_LVL_BITMASK		0xFF

#define DBG_TYPE_PRINTK		3

#define BCM_DEBUG_PRINT(Adapter, Type, SubType, dbg_level, string, args...) \
	do {								\
		if (DBG_TYPE_PRINTK == Type)				\
			pr_info("%s:" string, __func__, ##args);	\
		else if (Adapter &&					\
			 (dbg_level & DBG_LVL_BITMASK) <= Adapter->stDebugState.debug_level && \
			 (Type & Adapter->stDebugState.type) &&		\
			 (SubType & Adapter->stDebugState.subtype[Type])) { \
			if (dbg_level & DBG_NO_FUNC_PRINT)		\
				printk(KERN_DEBUG string, ##args);	\
			else						\
				printk(KERN_DEBUG "%s:" string, __func__, ##args);	\
		}							\
	} while (0)

#define BCM_DEBUG_PRINT_BUFFER(Adapter, Type, SubType, dbg_level,  buffer, bufferlen) do { \
	if (DBG_TYPE_PRINTK == Type ||					\
	    (Adapter &&							\
	     (dbg_level & DBG_LVL_BITMASK) <= Adapter->stDebugState.debug_level  && \
	     (Type & Adapter->stDebugState.type) &&			\
	     (SubType & Adapter->stDebugState.subtype[Type]))) {	\
		printk(KERN_DEBUG "%s:\n", __func__);			\
		print_hex_dump(KERN_DEBUG, " ", DUMP_PREFIX_OFFSET,	\
			       16, 1, buffer, bufferlen, false);	\
	}								\
} while(0)


#define BCM_SHOW_DEBUG_BITMAP(Adapter)	do { \
	int i;									\
	for (i=0; i<(NUMTYPES*2)+1; i++) {		\
		if ((i == 1) || (i == 2) || (i == 4) || (i == 8)) {		\
 \
		BCM_DEBUG_PRINT (Adapter, DBG_TYPE_PRINTK, 0, 0, "subtype[%d] = 0x%08x\n", 	\
		i, Adapter->stDebugState.subtype[i]);	\
		}	\
	}		\
} while (0)

#endif

