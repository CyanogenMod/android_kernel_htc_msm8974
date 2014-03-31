
/*
 * snull.h -- definitions for the network module
 *
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 */


#define TX_RING_ENTRIES 64	

#define RX_RING_ENTRIES 16 
#define TX_RING_BUFFER_SIZE	(TX_RING_ENTRIES*sizeof(tx_packet))
#define RX_BUFFER_SIZE 1546 
#define METH_RX_BUFF_SIZE 4096
#define METH_RX_HEAD 34 
#define RX_BUFFER_OFFSET (sizeof(rx_status_vector)+2) 
#define RX_BUCKET_SIZE 256


/* tx status vector is written over tx command header upon
   dma completion. */

typedef struct tx_status_vector {
	u64		sent:1; 
	u64		pad0:34;
	u64		flags:9;			
	u64		col_retry_cnt:4;	
	u64		len:16;				
} tx_status_vector;

typedef struct tx_packet_hdr {
	u64		pad1:36; 
	u64		cat_ptr3_valid:1,	
			cat_ptr2_valid:1,
			cat_ptr1_valid:1;
	u64		tx_int_flag:1;		
	u64		term_dma_flag:1;	
	u64		data_offset:7;		
	u64		data_len:16;		
} tx_packet_hdr;
typedef union tx_cat_ptr {
	struct {
		u64		pad2:16; 
		u64		len:16;				
		u64		start_addr:29;		
		u64		pad1:3; 
	} form;
	u64 raw;
} tx_cat_ptr;

typedef struct tx_packet {
	union {
		tx_packet_hdr header;
		tx_status_vector res;
		u64 raw;
	}header;
	union {
		tx_cat_ptr cat_buf[3];
		char dt[120];
	} data;
} tx_packet;

typedef union rx_status_vector {
	volatile struct {
		u64		pad1:1;
		u64		pad2:15;
		u64		ip_chk_sum:16;
		u64		seq_num:5;
		u64		mac_addr_match:1;
		u64		mcast_addr_match:1;
		u64		carrier_event_seen:1;
		u64		bad_packet:1;
		u64		long_event_seen:1;
		u64		invalid_preamble:1;
		u64		broadcast:1;
		u64		multicast:1;
		u64		crc_error:1;
		u64		huh:1;
		u64		rx_code_violation:1;
		u64		rx_len:16;
	} parsed;
	volatile u64 raw;
} rx_status_vector;

typedef struct rx_packet {
	rx_status_vector status;
        u64 pad[3]; 
        u16 pad2;
	char buf[METH_RX_BUFF_SIZE-sizeof(rx_status_vector)-3*sizeof(u64)-sizeof(u16)];
} rx_packet;

#define TX_INFO_RPTR    0x00FF0000
#define TX_INFO_WPTR    0x000000FF

	

#define SGI_MAC_RESET		BIT(0)	
#define METH_PHY_FDX		BIT(1) 
#define METH_PHY_LOOP	BIT(2) 
				       
#define METH_100MBIT		BIT(3) 
#define METH_PHY_MII		BIT(4) 
				       
				       

				       
#define METH_ACCEPT_MY 0			
#define METH_ACCEPT_MCAST 0x20	
#define METH_ACCEPT_AMCAST 0x40	
#define METH_PROMISC 0x60		

#define METH_PHY_LINK_FAIL	BIT(7) 

#define METH_MAC_IPG	0x1ffff00

#define METH_DEFAULT_IPG ((17<<15) | (11<<22) | (21<<8))
						  
				       
				       
				       

				       

				       

#define METH_REV_SHIFT 29       
				       
				       


#define METH_RX_OFFSET_SHIFT 12 
#define METH_RX_DEPTH_SHIFT 4 

#define METH_DMA_TX_EN BIT(1) 
#define METH_DMA_TX_INT_EN BIT(0) 
#define METH_DMA_RX_EN BIT(15) 
#define METH_DMA_RX_INT_EN BIT(9) 

#define METH_RX_FIFO_WPTR(x)   (((x)>>16)&0xf)
#define METH_RX_FIFO_RPTR(x)   (((x)>>8)&0xf)
#define METH_RX_FIFO_DEPTH(x)  ((x)&0x1f)


#define METH_RX_ST_VALID BIT(63)
#define METH_RX_ST_RCV_CODE_VIOLATION BIT(16)
#define METH_RX_ST_DRBL_NBL BIT(17)
#define METH_RX_ST_CRC_ERR BIT(18)
#define METH_RX_ST_MCAST_PKT BIT(19)
#define METH_RX_ST_BCAST_PKT BIT(20)
#define METH_RX_ST_INV_PREAMBLE_CTX BIT(21)
#define METH_RX_ST_LONG_EVT_SEEN BIT(22)
#define METH_RX_ST_BAD_PACKET BIT(23)
#define METH_RX_ST_CARRIER_EVT_SEEN BIT(24)
#define METH_RX_ST_MCAST_FILTER_MATCH BIT(25)
#define METH_RX_ST_PHYS_ADDR_MATCH BIT(26)

#define METH_RX_STATUS_ERRORS \
	( \
	METH_RX_ST_RCV_CODE_VIOLATION| \
	METH_RX_ST_CRC_ERR| \
	METH_RX_ST_INV_PREAMBLE_CTX| \
	METH_RX_ST_LONG_EVT_SEEN| \
	METH_RX_ST_BAD_PACKET| \
	METH_RX_ST_CARRIER_EVT_SEEN \
	)
	
	
#define METH_INT_TX_EMPTY	BIT(0)	
#define METH_INT_TX_PKT		BIT(1)	
					      	
#define METH_INT_TX_LINK_FAIL	BIT(2)	
#define METH_INT_MEM_ERROR	BIT(3)	
						
#define METH_INT_TX_ABORT		BIT(4)	
#define METH_INT_RX_THRESHOLD	BIT(5)	
#define METH_INT_RX_UNDERFLOW	BIT(6)	
#define METH_INT_RX_OVERFLOW		BIT(7)	

		
#define METH_INT_RX_RPTR_MASK 0x0000F00		

						

#define METH_INT_TX_RPTR_MASK	0x1FF0000        

#define METH_INT_RX_SEQ_MASK	0x2E000000	

						

#define METH_INT_ERROR	(METH_INT_TX_LINK_FAIL| \
			METH_INT_MEM_ERROR| \
			METH_INT_TX_ABORT| \
			METH_INT_RX_OVERFLOW| \
			METH_INT_RX_UNDERFLOW)

#define METH_INT_MCAST_HASH		BIT(30) 

#define METH_TX_ST_DONE      BIT(63) 
#define METH_TX_ST_SUCCESS   BIT(23) 
#define METH_TX_ST_TOOLONG   BIT(24) 
#define METH_TX_ST_UNDERRUN  BIT(25) 
#define METH_TX_ST_EXCCOLL   BIT(26) 
#define METH_TX_ST_DEFER     BIT(27) 
#define METH_TX_ST_LATECOLL  BIT(28) 


#define METH_TX_CMD_INT_EN BIT(24) 

#define MDIO_BUSY    BIT(16)
#define MDIO_DATA_MASK 0xFFFF
#define PHY_QS6612X    0x0181441    
#define PHY_ICS1889    0x0015F41    
#define PHY_ICS1890    0x0015F42    
#define PHY_DP83840    0x20005C0    

#define ADVANCE_RX_PTR(x)  x=(x+1)&(RX_RING_ENTRIES-1)
