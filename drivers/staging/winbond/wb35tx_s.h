#ifndef __WINBOND_WB35_TX_S_H
#define __WINBOND_WB35_TX_S_H

#include "mds_s.h"

#define TX_INTERFACE			0	
#define TX_PIPE					3	
#define TX_INTERRUPT			1	
#define MAX_INTERRUPT_LENGTH	64	





struct wb35_tx {
	
	u8	TxBuffer[ MAX_USB_TX_BUFFER_NUMBER ][ MAX_USB_TX_BUFFER ];

	
	u8	EP2_buf[MAX_INTERRUPT_LENGTH];

	atomic_t	TxResultCount;
	atomic_t	TxFireCounter;
	u32			ByteTransfer;

	u32	    TxSendIndex;
	u32	    EP2vm_state; 
	u32	    EP4vm_state; 
	u32	    tx_halt; 

	struct urb *				Tx4Urb;
	struct urb *				Tx2Urb;

	int		EP2VM_status;
	int		EP4VM_status;

	u32	TxFillCount; 
	u32	TxTimer; 
};

#endif
