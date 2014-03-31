#ifndef __LINUX_IBMTR_H__
#define __LINUX_IBMTR_H__

/* This file is distributed under the GNU GPL   */


#define TR_RETRY_INTERVAL	(30*HZ)	
#define TR_RST_TIME		(msecs_to_jiffies(50))	
#define TR_BUSY_INTERVAL	(msecs_to_jiffies(200))	
#define TR_SPIN_INTERVAL	(3*HZ)	

#define TR_ISA 1
#define TR_MCA 2
#define TR_ISAPNP 3
#define NOTOK 0

#define IBMTR_SHARED_RAM_SIZE 0x10000
#define IBMTR_IO_EXTENT 4
#define IBMTR_MAX_ADAPTERS 4

#define CHANNEL_ID      0X1F30
#define AIP             0X1F00
#define AIPADAPTYPE     0X1FA0
#define AIPDATARATE     0X1FA2
#define AIPEARLYTOKEN   0X1FA4
#define AIPAVAILSHRAM   0X1FA6
#define AIPSHRAMPAGE    0X1FA8
#define AIP4MBDHB       0X1FAA
#define AIP16MBDHB      0X1FAC
#define AIPFID		0X1FBA

#define ADAPTRESET      0x1     
#define ADAPTRESETREL   0x2     
#define ADAPTINTREL	0x3 	

#define GLOBAL_INT_ENABLE 0x02f0

#define RRR_EVEN       0x00 
#define RRR_ODD         0x01
#define WRBR_EVEN       0x02    
#define WRBR_ODD        0x03
#define WWOR_EVEN       0x04    
#define WWOR_ODD        0x05
#define WWCR_EVEN       0x06   
#define WWCR_ODD        0x07

#define ISRP_EVEN       0x08

#define TCR_INT    0x10    
#define ERR_INT	   0x08    
#define ACCESS_INT 0x04    
#define INT_ENABLE 0x40 


#define ISRP_ODD        0x09

#define ADAP_CHK_INT 0x40 
#define SRB_RESP_INT 0x20 
#define ASB_FREE_INT 0x10 
#define ARB_CMD_INT  0x08 
#define SSB_RESP_INT 0x04 



#define ISRA_EVEN 0x0A 

#define ISRA_ODD        0x0B
#define CMD_IN_SRB  0x20 
#define RESP_IN_ASB 0x10 
#define ARB_FREE 0x2
#define SSB_FREE 0x1

#define TCR_EVEN        0x0C    
#define TCR_ODD         0x0D
#define TVR_EVEN        0x0E    
#define TVR_ODD         0x0F
#define SRPR_EVEN       0x18    
#define SRPR_ENABLE_PAGING 0xc0
#define SRPR_ODD        0x19	
#define TOKREAD         0x60
#define TOKOR           0x40
#define TOKAND          0x20
#define TOKWRITE        0x00



#define PCCHANNELID 5049434F3631313039393020
#define MCCHANNELID 4D4152533633583435313820

#define ACA_OFFSET 0x1e00
#define ACA_SET 0x40
#define ACA_RESET 0x20
#define ACA_RW 0x00

#ifdef ENABLE_PAGING
#define SET_PAGE(x) (writeb((x), ti->mmio + ACA_OFFSET+ ACA_RW + SRPR_EVEN))
#else
#define SET_PAGE(x)
#endif

#define FIRST_INT 1
#define NOT_FIRST 2

typedef enum {	CLOSED,	OPEN } open_state;

struct tok_info {
	unsigned char irq;
	void __iomem *mmio;
	unsigned char hw_address[32];
	unsigned char adapter_type;
	unsigned char data_rate;
	unsigned char token_release;
	unsigned char avail_shared_ram;
	unsigned char shared_ram_paging;
        unsigned char turbo;
	unsigned short dhb_size4mb;
	unsigned short rbuf_len4;
	unsigned short rbuf_cnt4;
	unsigned short maxmtu4;
	unsigned short dhb_size16mb;
	unsigned short rbuf_len16;
	unsigned short rbuf_cnt16;
	unsigned short maxmtu16;
	
	unsigned char do_tok_int;
	wait_queue_head_t wait_for_reset;
	unsigned char sram_base;
	
	unsigned char page_mask;          
	unsigned char mapped_ram_size;    
	__u32 sram_phys;          
	void __iomem *sram_virt;          
	void __iomem *init_srb;   
	void __iomem *srb;                
	void __iomem *ssb;                
	void __iomem *arb;                
	void __iomem *asb;                
        __u8  init_srb_page;
        __u8  srb_page;
        __u8  ssb_page;
        __u8  arb_page;
        __u8  asb_page;
	unsigned short exsap_station_id;
	unsigned short global_int_enable;
	struct sk_buff *current_skb;

	unsigned char auto_speedsave;
	open_state			open_status, sap_status;
	enum {MANUAL, AUTOMATIC}	open_mode;
	enum {FAIL, RESTART, REOPEN}	open_action;
	enum {NO, YES}			open_failure;
	unsigned char readlog_pending;
	unsigned short adapter_int_enable; 
        struct timer_list tr_timer;
	unsigned char ring_speed;
	spinlock_t lock;		
};

#define DIR_INTERRUPT 		0x00 
#define DIR_MOD_OPEN_PARAMS 	0x01
#define DIR_OPEN_ADAPTER 	0x03 
#define DIR_CLOSE_ADAPTER   	0x04
#define DIR_SET_GRP_ADDR    	0x06
#define DIR_SET_FUNC_ADDR   	0x07 
#define DIR_READ_LOG 		0x08 
#define DLC_OPEN_SAP 		0x15 
#define DLC_CLOSE_SAP       	0x16
#define DATA_LOST 		0x20 
#define REC_DATA 		0x81 
#define XMIT_DATA_REQ 		0x82 
#define DLC_STATUS 		0x83 
#define RING_STAT_CHANGE    	0x84 

#define OPEN_PASS_BCON_MAC 0x0100
#define NUM_RCV_BUF 2
#define RCV_BUF_LEN 1024
#define DHB_LENGTH 2048
#define NUM_DHB 2
#define DLC_MAX_SAP 2
#define DLC_MAX_STA 1

#define MAX_I_FIELD 0x0088
#define SAP_OPEN_IND_SAP 0x04
#define SAP_OPEN_PRIORITY 0x20
#define SAP_OPEN_STATION_CNT 0x1
#define XMIT_DIR_FRAME 0x0A
#define XMIT_UI_FRAME  0x0d
#define XMIT_XID_CMD   0x0e
#define XMIT_TEST_CMD  0x11

#define SIGNAL_LOSS  0x8000
#define HARD_ERROR   0x4000
#define XMIT_BEACON  0x1000
#define LOBE_FAULT   0x0800
#define AUTO_REMOVAL 0x0400
#define REMOVE_RECV  0x0100
#define LOG_OVERFLOW 0x0080
#define RING_RECOVER 0x0020

struct srb_init_response {
	unsigned char command;
	unsigned char init_status;
	unsigned char init_status_2;
	unsigned char reserved[3];
	__u16 bring_up_code;
	__u16 encoded_address;
	__u16 level_address;
	__u16 adapter_address;
	__u16 parms_address;
	__u16 mac_address;
};

struct dir_open_adapter {
	unsigned char command;
	char reserved[7];
	__u16 open_options;
	unsigned char node_address[6];
	unsigned char group_address[4];
	unsigned char funct_address[4];
	__u16 num_rcv_buf;
	__u16 rcv_buf_len;
	__u16 dhb_length;
	unsigned char num_dhb;
	char reserved2;
	unsigned char dlc_max_sap;
	unsigned char dlc_max_sta;
	unsigned char dlc_max_gsap;
	unsigned char dlc_max_gmem;
	unsigned char dlc_t1_tick_1;
	unsigned char dlc_t2_tick_1;
	unsigned char dlc_ti_tick_1;
	unsigned char dlc_t1_tick_2;
	unsigned char dlc_t2_tick_2;
	unsigned char dlc_ti_tick_2;
	unsigned char product_id[18];
};

struct dlc_open_sap {
	unsigned char command;
	unsigned char reserved1;
	unsigned char ret_code;
	unsigned char reserved2;
	__u16 station_id;
	unsigned char timer_t1;
	unsigned char timer_t2;
	unsigned char timer_ti;
	unsigned char maxout;
	unsigned char maxin;
	unsigned char maxout_incr;
	unsigned char max_retry_count;
	unsigned char gsap_max_mem;
	__u16 max_i_field;
	unsigned char sap_value;
	unsigned char sap_options;
	unsigned char station_count;
	unsigned char sap_gsap_mem;
	unsigned char gsap[0];
};

struct srb_xmit {
	unsigned char command;
	unsigned char cmd_corr;
	unsigned char ret_code;
	unsigned char reserved1;
	__u16 station_id;
};

struct arb_rec_req {
	unsigned char command;
	unsigned char reserved1[3];
	__u16 station_id;
	__u16 rec_buf_addr;
	unsigned char lan_hdr_len;
	unsigned char dlc_hdr_len;
	__u16 frame_len;
	unsigned char msg_type;
};

struct asb_rec {
	unsigned char command;
	unsigned char reserved1;
	unsigned char ret_code;
	unsigned char reserved2;
	__u16 station_id;
	__u16 rec_buf_addr;
};

struct rec_buf {
  	unsigned char reserved1[2];
	__u16 buf_ptr;
	unsigned char reserved2;
	unsigned char receive_fs;
	__u16 buf_len;
	unsigned char data[0];
};

struct srb_set_funct_addr {
	unsigned char command;
	unsigned char reserved1;
	unsigned char ret_code;
	unsigned char reserved2[3];
	unsigned char funct_address[4];
};

#endif
