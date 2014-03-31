
#define MAX_USB_RX_BUFFER	4096	

#define MAX_USB_RX_BUFFER_NUMBER	ETHERNET_RX_DESCRIPTORS		
#define RX_INTERFACE				0	
#define RX_PIPE						2	
#define MAX_PACKET_SIZE				1600 
#define RX_END_TAG					0x0badbeef


struct wb35_rx {
	u32			ByteReceived;
	atomic_t		RxFireCounter;

	u8	RxBuffer[ MAX_USB_RX_BUFFER_NUMBER ][ ((MAX_USB_RX_BUFFER+3) & ~0x03 ) ];
	u16	RxBufferSize[ ((MAX_USB_RX_BUFFER_NUMBER+1) & ~0x01) ];
	u8	RxOwner[ ((MAX_USB_RX_BUFFER_NUMBER+3) & ~0x03 ) ];

	u32	RxProcessIndex;
	u32	RxBufferId;
	u32	EP3vm_state;

	u32	rx_halt; 

	u16	MoreDataSize;
	u16	PacketSize;

	u32	CurrentRxBufferId; 
	u32	Rx3UrbCancel;

	u32	LastR1; 
	struct urb *				RxUrb;
	u32		Ep3ErrorCount2; 

	int		EP3VM_status;
	u8 *	pDRx;
};
