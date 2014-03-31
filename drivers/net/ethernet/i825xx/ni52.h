/*
 * Intel i82586 Ethernet definitions
 *
 * This is an extension to the Linux operating system, and is covered by the
 * same GNU General Public License that covers that work.
 *
 * copyrights (c) 1994 by Michael Hipp (hippm@informatik.uni-tuebingen.de)
 *
 * I have done a look in the following sources:
 *   crynwr-packet-driver by Russ Nelson
 *   Garret A. Wollman's i82586-driver for BSD
 */


#define NI52_RESET     0  
#define NI52_ATTENTION 1  
#define NI52_TENA      3  
#define NI52_TDIS      2  
#define NI52_INTENA    5  
#define NI52_INTDIS    4  
#define NI52_MAGIC1    6  
#define NI52_MAGIC2    7  

#define NI52_MAGICVAL1 0x00  
#define NI52_MAGICVAL2 0x55

#define SCP_DEFAULT_ADDRESS 0xfffff4



struct scp_struct
{
	u16 zero_dum0;	
	u8 sysbus;	
	u8 zero_dum1;	
	u16 zero_dum2;
	u16 zero_dum3;
	u32 iscp;		
};


struct iscp_struct
{
	u8 busy;          
	u8 zero_dummy;    
	u16 scb_offset;    
	u32 scb_base;      
};

struct scb_struct
{
	u8 rus;
	u8 cus;
	u8 cmd_ruc;        
	u8 cmd_cuc;        
	u16 cbl_offset;    
	u16 rfa_offset;    
	u16 crc_errs;      
	u16 aln_errs;      
	u16 rsc_errs;      
	u16 ovrn_errs;     
};

#define RUC_MASK	0x0070	
#define RUC_NOP		0x0000	
#define RUC_START	0x0010	
#define RUC_RESUME	0x0020	
#define RUC_SUSPEND	0x0030	
#define RUC_ABORT	0x0040	

#define CUC_MASK        0x07  
#define CUC_NOP         0x00  
#define CUC_START       0x01  
#define CUC_RESUME      0x02  
#define CUC_SUSPEND     0x03  
#define CUC_ABORT       0x04  

#define ACK_MASK        0xf0  
#define ACK_CX          0x80  
#define ACK_FR          0x40  
#define ACK_CNA         0x20  
#define ACK_RNR         0x10  

#define STAT_MASK       0xf0  
#define STAT_CX         0x80  
#define STAT_FR         0x40  
#define STAT_CNA        0x20  
#define STAT_RNR        0x10  

#define CU_STATUS       0x7   
#define CU_SUSPEND      0x1   
#define CU_ACTIVE       0x2   

#define RU_STATUS	0x70	
#define RU_SUSPEND	0x10	
#define RU_NOSPACE	0x20	
#define RU_READY	0x40	

struct rfd_struct
{
	u8  stat_low;	
	u8  stat_high;	
	u8  rfd_sf;	
	u8  last;		
	u16 next;		
	u16 rbd_offset;	
	u8  dest[6];	
	u8  source[6];	
	u16 length;	
	u16 zero_dummy;	
};

#define RFD_LAST     0x80	
#define RFD_SUSP     0x40	
#define RFD_COMPL    0x80
#define RFD_OK       0x20
#define RFD_BUSY     0x40
#define RFD_ERR_LEN  0x10     
#define RFD_ERR_CRC  0x08     
#define RFD_ERR_ALGN 0x04     
#define RFD_ERR_RNR  0x02     
#define RFD_ERR_OVR  0x01     

#define RFD_ERR_FTS  0x0080	
#define RFD_ERR_NEOP 0x0040	
#define RFD_ERR_TRUN 0x0020	
#define RFD_MATCHADD 0x0002     
#define RFD_COLLDET  0x0001	

struct rbd_struct
{
	u16 status;	
	u16 next;		
	u32 buffer;	
	u16 size;		
	u16 zero_dummy;    
};

#define RBD_LAST	0x8000	
#define RBD_USED	0x4000	
#define RBD_MASK	0x3fff	

#define STAT_COMPL   0x8000	
#define STAT_BUSY    0x4000	
#define STAT_OK      0x2000	

#define CMD_NOP		0x0000	
#define CMD_IASETUP	0x0001	
#define CMD_CONFIGURE	0x0002	
#define CMD_MCSETUP	0x0003	
#define CMD_XMIT	0x0004	
#define CMD_TDR		0x0005	
#define CMD_DUMP	0x0006	
#define CMD_DIAGNOSE	0x0007	

#define CMD_LAST	0x8000	
#define CMD_SUSPEND	0x4000	
#define CMD_INT		0x2000	

struct nop_cmd_struct
{
	u16 cmd_status;	
	u16 cmd_cmd;       
	u16 cmd_link;      
};

struct iasetup_cmd_struct
{
	u16 cmd_status;
	u16 cmd_cmd;
	u16 cmd_link;
	u8  iaddr[6];
};

struct configure_cmd_struct
{
	u16 cmd_status;
	u16 cmd_cmd;
	u16 cmd_link;
	u8  byte_cnt;   
	u8  fifo;       
	u8  sav_bf;     
	u8  adr_len;    
	u8  priority;   
	u8  ifs;        
	u8  time_low;   
	u8  time_high;  
	u8  promisc;    
	u8  carr_coll;  
	u8  fram_len;   
	u8  dummy;	     
};

struct mcsetup_cmd_struct
{
	u16 cmd_status;
	u16 cmd_cmd;
	u16 cmd_link;
	u16 mc_cnt;		
	u8  mc_list[0][6];  	
};

struct dump_cmd_struct
{
	u16 cmd_status;
	u16 cmd_cmd;
	u16 cmd_link;
	u16 dump_offset;    
};

struct transmit_cmd_struct
{
	u16 cmd_status;
	u16 cmd_cmd;
	u16 cmd_link;
	u16 tbd_offset;	
	u8  dest[6];       
	u16 length;	
};

#define TCMD_ERRMASK     0x0fa0
#define TCMD_MAXCOLLMASK 0x000f
#define TCMD_MAXCOLL     0x0020
#define TCMD_HEARTBEAT   0x0040
#define TCMD_DEFERRED    0x0080
#define TCMD_UNDERRUN    0x0100
#define TCMD_LOSTCTS     0x0200
#define TCMD_NOCARRIER   0x0400
#define TCMD_LATECOLL    0x0800

struct tdr_cmd_struct
{
	u16 cmd_status;
	u16 cmd_cmd;
	u16 cmd_link;
	u16 status;
};

#define TDR_LNK_OK	0x8000	
#define TDR_XCVR_PRB	0x4000	
#define TDR_ET_OPN	0x2000	
#define TDR_ET_SRT	0x1000	
#define TDR_TIMEMASK	0x07ff	

struct tbd_struct
{
	u16 size;		
	u16 next;          
	u32 buffer;        
};

#define TBD_LAST 0x8000         




