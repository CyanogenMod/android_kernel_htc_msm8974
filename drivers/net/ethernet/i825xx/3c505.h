
#define ELP_DMA       6
#define ELP_RX_PCBS   4
#define ELP_MAX_CARDS 4

#define	PORT_COMMAND	0x00	
#define	PORT_STATUS	0x02	
#define	PORT_AUXDMA	0x02	
#define	PORT_DATA	0x04	
#define	PORT_CONTROL	0x06	

#define ELP_IO_EXTENT	0x10	

#define	ATTN	0x80	
#define	FLSH	0x40	
#define DMAE	0x20	
#define DIR	0x10	
#define	TCEN	0x08	
#define	CMDE	0x04	
#define	HSF2	0x02	
#define	HSF1	0x01	

#define	HSF_PCB_ACK	HSF1
#define	HSF_PCB_NAK	HSF2
#define	HSF_PCB_END	(HSF2|HSF1)
#define	HSF_PCB_MASK	(HSF2|HSF1)

#define	HRDY	0x80	
#define	HCRE	0x40	
#define	ACRF	0x20	
#define	DONE	0x08	
#define	ASF3	0x04	
#define	ASF2	0x02	
#define	ASF1	0x01	

#define	ASF_PCB_ACK	ASF1
#define	ASF_PCB_NAK	ASF2
#define	ASF_PCB_END	(ASF2|ASF1)
#define	ASF_PCB_MASK	(ASF2|ASF1)

#define	DMA_BRST	0x01	

#define	MAX_PCB_DATA	62


#define	TIMEOUT	300


enum {
  CMD_CONFIGURE_ADAPTER_MEMORY	= 0x01,
  CMD_CONFIGURE_82586		= 0x02,
  CMD_STATION_ADDRESS		= 0x03,
  CMD_DMA_DOWNLOAD		= 0x04,
  CMD_DMA_UPLOAD		= 0x05,
  CMD_PIO_DOWNLOAD		= 0x06,
  CMD_PIO_UPLOAD		= 0x07,
  CMD_RECEIVE_PACKET		= 0x08,
  CMD_TRANSMIT_PACKET		= 0x09,
  CMD_NETWORK_STATISTICS	= 0x0a,
  CMD_LOAD_MULTICAST_LIST	= 0x0b,
  CMD_CLEAR_PROGRAM		= 0x0c,
  CMD_DOWNLOAD_PROGRAM		= 0x0d,
  CMD_EXECUTE_PROGRAM		= 0x0e,
  CMD_SELF_TEST			= 0x0f,
  CMD_SET_STATION_ADDRESS	= 0x10,
  CMD_ADAPTER_INFO		= 0x11,
  NUM_TRANSMIT_CMDS,

  CMD_CONFIGURE_ADAPTER_RESPONSE	= 0x31,
  CMD_CONFIGURE_82586_RESPONSE		= 0x32,
  CMD_ADDRESS_RESPONSE			= 0x33,
  CMD_DOWNLOAD_DATA_REQUEST		= 0x34,
  CMD_UPLOAD_DATA_REQUEST		= 0x35,
  CMD_RECEIVE_PACKET_COMPLETE		= 0x38,
  CMD_TRANSMIT_PACKET_COMPLETE		= 0x39,
  CMD_NETWORK_STATISTICS_RESPONSE	= 0x3a,
  CMD_LOAD_MULTICAST_RESPONSE		= 0x3b,
  CMD_CLEAR_PROGRAM_RESPONSE		= 0x3c,
  CMD_DOWNLOAD_PROGRAM_RESPONSE		= 0x3d,
  CMD_EXECUTE_RESPONSE			= 0x3e,
  CMD_SELF_TEST_RESPONSE		= 0x3f,
  CMD_SET_ADDRESS_RESPONSE		= 0x40,
  CMD_ADAPTER_INFO_RESPONSE		= 0x41
};


typedef unsigned char         byte;
typedef unsigned short int    word;
typedef unsigned long int     dword;

struct Memconf {
	word	cmd_q,
		rcv_q,
		mcast,
		frame,
		rcv_b,
		progs;
};

struct Rcv_pkt {
	word	buf_ofs,
		buf_seg,
		buf_len,
		timeout;
};

struct Xmit_pkt {
	word	buf_ofs,
		buf_seg,
		pkt_len;
};

struct Rcv_resp {
	word	buf_ofs,
		buf_seg,
		buf_len,
		pkt_len,
		timeout,
		status;
	dword	timetag;
};

struct Xmit_resp {
	word	buf_ofs,
		buf_seg,
		c_stat,
		status;
};


struct Netstat {
	dword	tot_recv,
		tot_xmit;
	word	err_CRC,
		err_align,
		err_res,
		err_ovrrun;
};


struct Selftest {
	word	error;
	union {
		word ROM_cksum;
		struct {
			word ofs, seg;
		} RAM;
		word i82586;
	} failure;
};

struct Info {
	byte	minor_vers,
		major_vers;
	word	ROM_cksum,
		RAM_sz,
		free_ofs,
		free_seg;
};

struct Memdump {
       word size,
            off,
            seg;
};

typedef struct {
	byte	command;
	byte	length;
	union	{
		struct Memconf		memconf;
		word			configure;
		struct Rcv_pkt		rcv_pkt;
		struct Xmit_pkt		xmit_pkt;
		byte			multicast[10][6];
		byte			eth_addr[6];
		byte			failed;
		struct Rcv_resp		rcv_resp;
		struct Xmit_resp	xmit_resp;
		struct Netstat		netstat;
		struct Selftest		selftest;
		struct Info		info;
		struct Memdump    	memdump;
		byte			raw[62];
	} data;
} pcb_struct;

#define RECV_STATION	0x00
#define RECV_BROAD	0x01
#define RECV_MULTI	0x02
#define RECV_PROMISC	0x04
#define NO_LOOPBACK	0x00
#define INT_LOOPBACK	0x08
#define EXT_LOOPBACK	0x10


#define DMA_BUFFER_SIZE  1600
#define BACKLOG_SIZE      4

typedef struct {
	volatile short got[NUM_TRANSMIT_CMDS];	
	pcb_struct tx_pcb;	
	pcb_struct rx_pcb;	
	pcb_struct itx_pcb;	
	pcb_struct irx_pcb;	

	void *dma_buffer;

	struct {
		unsigned int length[BACKLOG_SIZE];
		unsigned int in;
		unsigned int out;
	} rx_backlog;

	struct {
		unsigned int direction;
		unsigned int length;
		struct sk_buff *skb;
	        void *target;
		unsigned long start_time;
	} current_dma;

	
	unsigned long send_pcb_semaphore;
	unsigned long dmaing;
	unsigned long busy;

	unsigned int rx_active;  
        volatile unsigned char hcr_val;  
        spinlock_t lock;	
} elp_device;
