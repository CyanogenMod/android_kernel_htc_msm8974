/******************************************************************************
             Device driver for Interphase ATM PCI adapter cards 
                    Author: Peter Wang  <pwang@iphase.com>            
                   Interphase Corporation  <www.iphase.com>           
                               Version: 1.0   
               iphase.h:  This is the header file for iphase.c. 
*******************************************************************************
      
      This software may be used and distributed according to the terms
      of the GNU General Public License (GPL), incorporated herein by reference.
      Drivers based on this skeleton fall under the GPL and must retain
      the authorship (implicit copyright) notice.

      This program is distributed in the hope that it will be useful, but
      WITHOUT ANY WARRANTY; without even the implied warranty of
      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
      General Public License for more details.
      
      Modified from an incomplete driver for Interphase 5575 1KVC 1M card which 
      was originally written by Monalisa Agrawal at UNH. Now this driver 
      supports a variety of varients of Interphase ATM PCI (i)Chip adapter 
      card family (See www.iphase.com/products/ClassSheet.cfm?ClassID=ATM) 
      in terms of PHY type, the size of control memory and the size of 
      packet memory. The followings are the change log and history:
     
          Bugfix the Mona's UBR driver.
          Modify the basic memory allocation and dma logic.
          Port the driver to the latest kernel from 2.0.46.
          Complete the ABR logic of the driver, and added the ABR work-
              around for the hardware anormalies.
          Add the CBR support.
	  Add the flow control logic to the driver to allow rate-limit VC.
          Add 4K VC support to the board with 512K control memory.
          Add the support of all the variants of the Interphase ATM PCI 
          (i)Chip adapter cards including x575 (155M OC3 and UTP155), x525
          (25M UTP25) and x531 (DS3 and E3).
          Add SMP support.

      Support and updates available at: ftp://ftp.iphase.com/pub/atm

*******************************************************************************/
  
#ifndef IPHASE_H  
#define IPHASE_H  


 
#define IF_IADBG_INIT_ADAPTER   0x00000001        
#define IF_IADBG_TX             0x00000002        
#define IF_IADBG_RX             0x00000004        
#define IF_IADBG_QUERY_INFO     0x00000008        
#define IF_IADBG_SHUTDOWN       0x00000010        
#define IF_IADBG_INTR           0x00000020        
#define IF_IADBG_TXPKT          0x00000040  	  
#define IF_IADBG_RXPKT          0x00000080  	  
#define IF_IADBG_ERR            0x00000100        
#define IF_IADBG_EVENT          0x00000200        
#define IF_IADBG_DIS_INTR       0x00001000        
#define IF_IADBG_EN_INTR        0x00002000        
#define IF_IADBG_LOUD           0x00004000        
#define IF_IADBG_VERY_LOUD      0x00008000        
#define IF_IADBG_CBR            0x00100000  	  
#define IF_IADBG_UBR            0x00200000  	  
#define IF_IADBG_ABR            0x00400000        
#define IF_IADBG_DESC           0x01000000        
#define IF_IADBG_SUNI_STAT      0x02000000        
#define IF_IADBG_RESET          0x04000000        

#define IF_IADBG(f) if (IADebugFlag & (f))

#ifdef  CONFIG_ATM_IA_DEBUG   

#define IF_LOUD(A) IF_IADBG(IF_IADBG_LOUD) { A }
#define IF_ERR(A) IF_IADBG(IF_IADBG_ERR) { A }
#define IF_VERY_LOUD(A) IF_IADBG( IF_IADBG_VERY_LOUD ) { A }

#define IF_INIT_ADAPTER(A) IF_IADBG( IF_IADBG_INIT_ADAPTER ) { A }
#define IF_INIT(A) IF_IADBG( IF_IADBG_INIT_ADAPTER ) { A }
#define IF_SUNI_STAT(A) IF_IADBG( IF_IADBG_SUNI_STAT ) { A }
#define IF_QUERY_INFO(A) IF_IADBG( IF_IADBG_QUERY_INFO ) { A }
#define IF_COPY_OVER(A) IF_IADBG( IF_IADBG_COPY_OVER ) { A }

#define IF_INTR(A) IF_IADBG( IF_IADBG_INTR ) { A }
#define IF_DIS_INTR(A) IF_IADBG( IF_IADBG_DIS_INTR ) { A }
#define IF_EN_INTR(A) IF_IADBG( IF_IADBG_EN_INTR ) { A }

#define IF_TX(A) IF_IADBG( IF_IADBG_TX ) { A }
#define IF_RX(A) IF_IADBG( IF_IADBG_RX ) { A }
#define IF_TXPKT(A) IF_IADBG( IF_IADBG_TXPKT ) { A }
#define IF_RXPKT(A) IF_IADBG( IF_IADBG_RXPKT ) { A }

#define IF_SHUTDOWN(A) IF_IADBG(IF_IADBG_SHUTDOWN) { A }
#define IF_CBR(A) IF_IADBG( IF_IADBG_CBR ) { A }
#define IF_UBR(A) IF_IADBG( IF_IADBG_UBR ) { A }
#define IF_ABR(A) IF_IADBG( IF_IADBG_ABR ) { A }
#define IF_EVENT(A) IF_IADBG( IF_IADBG_EVENT) { A }

#else 
#define IF_LOUD(A)
#define IF_VERY_LOUD(A)
#define IF_INIT_ADAPTER(A)
#define IF_INIT(A)
#define IF_SUNI_STAT(A)
#define IF_PVC_CHKPKT(A)
#define IF_QUERY_INFO(A)
#define IF_COPY_OVER(A)
#define IF_HANG(A)
#define IF_INTR(A)
#define IF_DIS_INTR(A)
#define IF_EN_INTR(A)
#define IF_TX(A)
#define IF_RX(A)
#define IF_TXDEBUG(A)
#define IF_VC(A)
#define IF_ERR(A) 
#define IF_CBR(A)
#define IF_UBR(A)
#define IF_ABR(A)
#define IF_SHUTDOWN(A)
#define DbgPrint(A)
#define IF_EVENT(A)
#define IF_TXPKT(A) 
#define IF_RXPKT(A)
#endif  

#define isprint(a) ((a >=' ')&&(a <= '~'))  
#define ATM_DESC(skb) (skb->protocol)
#define IA_SKB_STATE(skb) (skb->protocol)
#define IA_DLED   1
#define IA_TX_DONE 2

#define IA_CMD   0x7749
typedef struct {
	int cmd;
        int sub_cmd;
        int len;
        u32 maddr;
        int status;
        void __user *buf;
} IA_CMDBUF, *PIA_CMDBUF;

#define MEMDUMP     		0x01

#define MEMDUMP_SEGREG          0x2
#define MEMDUMP_DEV  		0x1
#define MEMDUMP_REASSREG        0x3
#define MEMDUMP_FFL             0x4
#define READ_REG                0x5
#define WAKE_DBG_WAIT           0x6


#define Boolean(x)    	((x) ? 1 : 0)
#define NR_VCI 1024		  
#define NR_VCI_LD 10		  
#define NR_VCI_4K 4096 		  
#define NR_VCI_4K_LD 12		  
#define MEM_VALID 0xfffffff0	  
  
#ifndef PCI_VENDOR_ID_IPHASE  
#define PCI_VENDOR_ID_IPHASE 0x107e  
#endif  
#ifndef PCI_DEVICE_ID_IPHASE_5575  
#define PCI_DEVICE_ID_IPHASE_5575 0x0008  
#endif  
#define DEV_LABEL 	"ia"  
#define PCR	207692  
#define ICR	100000  
#define MCR	0  
#define TBE	1000  
#define FRTT	1  
#define RIF	2		  
#define RDF	4  
#define NRMCODE 5	  
#define TRMCODE	3	  
#define CDFCODE	6  
#define ATDFCODE 2	  
  
  
#define TX_PACKET_RAM 	0x00000   
#define DFL_TX_BUF_SZ	10240	  
#define DFL_TX_BUFFERS     50 	
  
#define REASS_RAM_SIZE 0x10000    
#define RX_PACKET_RAM 	0x80000   
#define DFL_RX_BUF_SZ	10240	  
#define DFL_RX_BUFFERS      50	
  
  
struct cpcs_trailer 
{  
	u_short control;  
	u_short length;  
	u_int	crc32;  
};  

struct cpcs_trailer_desc
{
	struct cpcs_trailer *cpcs;
	dma_addr_t dma_addr;
};

struct ia_vcc 
{ 
	int rxing;	 
	int txing;		 
        int NumCbrEntry;
        u32 pcr;
        u32 saved_tx_quota;
        int flow_inc;
        struct sk_buff_head txing_skb; 
        int  ltimeout;                  
        u8  vc_desc_cnt;                
                
};  
  
struct abr_vc_table 
{  
	u_char status;  
	u_char rdf;  
	u_short air;  
	u_int res[3];  
	u_int req_rm_cell_data1;  
	u_int req_rm_cell_data2;  
	u_int add_rm_cell_data1;  
	u_int add_rm_cell_data2;  
};  
    
  
struct main_vc 
{  
	u_short 	type;  
#define ABR	0x8000  
#define UBR 	0xc000  
#define CBR	0x0000  
	  
	u_short 	nrm;	 
 	u_short 	trm;	   
	u_short 	rm_timestamp_hi;  
	u_short 	rm_timestamp_lo:8,  
			crm:8;		  
	u_short 	remainder; 	  
	u_short 	next_vc_sched;  
	u_short 	present_desc;	  
	u_short 	last_cell_slot;	  
	u_short 	pcr;  
	u_short 	fraction;  
	u_short 	icr;  
	u_short 	atdf;  
	u_short 	mcr;  
	u_short 	acr;		 
	u_short 	unack:8,  
			status:8;	  
#define UIOLI 0x80  
#define CRC_APPEND 0x40			  
#define ABR_STATE 0x02  
  
};  
  
  
  
struct ext_vc 
{  
	u_short 	atm_hdr1;  
	u_short 	atm_hdr2;  
	u_short 	last_desc;  
      	u_short 	out_of_rate_link;     
};  
  
  
#define DLE_ENTRIES 256  
#define DMA_INT_ENABLE 0x0002	  
#define TX_DLE_PSI 0x0001  
#define DLE_TOTAL_SIZE (sizeof(struct dle)*DLE_ENTRIES)
  
  
struct dle 
{  
	u32 	sys_pkt_addr;  
	u32 	local_pkt_addr;  
	u32 	bytes;  
	u16 	prq_wr_ptr_data;  
	u16 	mode;  
};  
  
struct dle_q 
{  
	struct dle 	*start;  
	struct dle 	*end;  
	struct dle 	*read;  
	struct dle 	*write;  
};  
  
struct free_desc_q 
{  
	int 	desc;	  
	struct free_desc_q *next;  
};  
  
struct tx_buf_desc {  
	unsigned short desc_mode;  
	unsigned short vc_index;  
	unsigned short res1;		  
	unsigned short bytes;  
	unsigned short buf_start_hi;  
	unsigned short buf_start_lo;  
	unsigned short res2[10];	  
};  
	  
  
struct rx_buf_desc { 
	unsigned short desc_mode;
	unsigned short vc_index;
	unsigned short vpi; 
	unsigned short bytes; 
	unsigned short buf_start_hi;  
	unsigned short buf_start_lo;  
	unsigned short dma_start_hi;  
	unsigned short dma_start_lo;  
	unsigned short crc_upper;  
	unsigned short crc_lower;  
	unsigned short res:8, timeout:8;  
	unsigned short res2[5];	  
};  
  
  
  
#define EPROM_SIZE 0x40000	  
#define MAC1_LEN	4	   					  
#define MAC2_LEN	2  
   
  
#define IPHASE5575_PCI_CONFIG_REG_BASE	0x0000  
#define IPHASE5575_BUS_CONTROL_REG_BASE 0x1000	  
#define IPHASE5575_FRAG_CONTROL_REG_BASE 0x2000  
#define IPHASE5575_REASS_CONTROL_REG_BASE 0x3000  
#define IPHASE5575_DMA_CONTROL_REG_BASE	0x4000  
#define IPHASE5575_FRONT_END_REG_BASE IPHASE5575_DMA_CONTROL_REG_BASE  
#define IPHASE5575_FRAG_CONTROL_RAM_BASE 0x10000  
#define IPHASE5575_REASS_CONTROL_RAM_BASE 0x20000  
  
  
#define IPHASE5575_BUS_CONTROL_REG	0x00  
#define IPHASE5575_BUS_STATUS_REG	0x01	  
#define IPHASE5575_MAC1			0x02  
#define IPHASE5575_REV			0x03  
#define IPHASE5575_MAC2			0x03	  
#define IPHASE5575_EXT_RESET		0x04  
#define IPHASE5575_INT_RESET		0x05	  
#define IPHASE5575_PCI_ADDR_PAGE	0x07	  
#define IPHASE5575_EEPROM_ACCESS	0x0a	  
#define IPHASE5575_CELL_FIFO_QUEUE_SZ	0x0b  
#define IPHASE5575_CELL_FIFO_MARK_STATE	0x0c  
#define IPHASE5575_CELL_FIFO_READ_PTR	0x0d  
#define IPHASE5575_CELL_FIFO_WRITE_PTR	0x0e  
#define IPHASE5575_CELL_FIFO_CELLS_AVL	0x0f	  
  
  
#define CTRL_FE_RST	0x80000000  
#define CTRL_LED	0x40000000  
#define CTRL_25MBPHY	0x10000000  
#define CTRL_ENCMBMEM	0x08000000  
#define CTRL_ENOFFSEG	0x01000000  
#define CTRL_ERRMASK	0x00400000  
#define CTRL_DLETMASK	0x00100000  
#define CTRL_DLERMASK	0x00080000  
#define CTRL_FEMASK	0x00040000  
#define CTRL_SEGMASK	0x00020000  
#define CTRL_REASSMASK	0x00010000  
#define CTRL_CSPREEMPT	0x00002000  
#define CTRL_B128	0x00000200  
#define CTRL_B64	0x00000100  
#define CTRL_B48	0x00000080  
#define CTRL_B32	0x00000040  
#define CTRL_B16	0x00000020  
#define CTRL_B8		0x00000010  
  
  
#define STAT_CMEMSIZ	0xc0000000  
#define STAT_ADPARCK	0x20000000  
#define STAT_RESVD	0x1fffff80  
#define STAT_ERRINT	0x00000040  
#define STAT_MARKINT	0x00000020  
#define STAT_DLETINT	0x00000010  
#define STAT_DLERINT	0x00000008  
#define STAT_FEINT	0x00000004  
#define STAT_SEGINT	0x00000002  
#define STAT_REASSINT	0x00000001  
  
  
  
  
#define IDLEHEADHI	0x00  
#define IDLEHEADLO	0x01  
#define MAXRATE		0x02  
  
#define RATE155		0x64b1 
#define MAX_ATM_155     352768 
#define RATE25		0x5f9d  
  
#define STPARMS		0x03  
#define STPARMS_1K	0x008c  
#define STPARMS_2K	0x0049  
#define STPARMS_4K	0x0026  
#define COMP_EN		0x4000  
#define CBR_EN		0x2000  
#define ABR_EN		0x0800  
#define UBR_EN		0x0400  
  
#define ABRUBR_ARB	0x04  
#define RM_TYPE		0x05  
  
#define RM_TYPE_4_0	0x0100  
  
#define SEG_COMMAND_REG		0x17  
  
#define RESET_SEG 0x0055  
#define RESET_SEG_STATE	0x00aa  
#define RESET_TX_CELL_CTR 0x00cc  
  
#define CBR_PTR_BASE	0x20  
#define ABR_SBPTR_BASE	0x22  
#define UBR_SBPTR_BASE  0x23  
#define ABRWQ_BASE	0x26  
#define UBRWQ_BASE	0x27  
#define VCT_BASE	0x28  
#define VCTE_BASE	0x29  
#define CBR_TAB_BEG	0x2c  
#define CBR_TAB_END	0x2d  
#define PRQ_ST_ADR	0x30  
#define PRQ_ED_ADR	0x31  
#define PRQ_RD_PTR	0x32  
#define PRQ_WR_PTR	0x33  
#define TCQ_ST_ADR	0x34  
#define TCQ_ED_ADR 	0x35  
#define TCQ_RD_PTR	0x36  
#define TCQ_WR_PTR	0x37  
#define SEG_QUEUE_BASE	0x40  
#define SEG_DESC_BASE	0x41  
#define MODE_REG_0	0x45  
#define T_ONLINE	0x0002		  
  
#define MODE_REG_1	0x46  
#define MODE_REG_1_VAL	0x0400		  
  
#define SEG_INTR_STATUS_REG 0x47  
#define SEG_MASK_REG	0x48  
#define TRANSMIT_DONE 0x0200
#define TCQ_NOT_EMPTY 0x1000	
  
  
#define CELL_CTR_HIGH_AUTO 0x49  
#define CELL_CTR_HIGH_NOAUTO 0xc9  
#define CELL_CTR_LO_AUTO 0x4a  
#define CELL_CTR_LO_NOAUTO 0xca  
  
  
#define NEXTDESC 	0x59  
#define NEXTVC		0x5a  
#define PSLOTCNT	0x5d  
#define NEWDN		0x6a  
#define NEWVC		0x6b  
#define SBPTR		0x6c  
#define ABRWQ_WRPTR	0x6f  
#define ABRWQ_RDPTR	0x70  
#define UBRWQ_WRPTR	0x71  
#define UBRWQ_RDPTR	0x72  
#define CBR_VC		0x73  
#define ABR_SBVC	0x75  
#define UBR_SBVC	0x76  
#define ABRNEXTLINK	0x78  
#define UBRNEXTLINK	0x79  
  
  
  
  
#define MODE_REG	0x00  
#define R_ONLINE	0x0002		  
#define IGN_RAW_FL     	0x0004
  
#define PROTOCOL_ID	0x01  
#define REASS_MASK_REG	0x02  
#define REASS_INTR_STATUS_REG	0x03  
  
#define RX_PKT_CTR_OF	0x8000  
#define RX_ERR_CTR_OF	0x4000  
#define RX_CELL_CTR_OF	0x1000  
#define RX_FREEQ_EMPT	0x0200  
#define RX_EXCPQ_FL	0x0080  
#define	RX_RAWQ_FL	0x0010  
#define RX_EXCP_RCVD	0x0008  
#define RX_PKT_RCVD	0x0004  
#define RX_RAW_RCVD	0x0001  
  
#define DRP_PKT_CNTR	0x04  
#define ERR_CNTR	0x05  
#define RAW_BASE_ADR	0x08  
#define CELL_CTR0	0x0c  
#define CELL_CTR1	0x0d  
#define REASS_COMMAND_REG	0x0f  
  
#define RESET_REASS	0x0055  
#define RESET_REASS_STATE 0x00aa  
#define RESET_DRP_PKT_CNTR 0x00f1  
#define RESET_ERR_CNTR	0x00f2  
#define RESET_CELL_CNTR 0x00f8  
#define RESET_REASS_ALL_REGS 0x00ff  
  
#define REASS_DESC_BASE	0x10  
#define VC_LKUP_BASE	0x11  
#define REASS_TABLE_BASE 0x12  
#define REASS_QUEUE_BASE 0x13  
#define PKT_TM_CNT	0x16  
#define TMOUT_RANGE	0x17  
#define INTRVL_CNTR	0x18  
#define TMOUT_INDX	0x19  
#define VP_LKUP_BASE	0x1c  
#define VP_FILTER	0x1d  
#define ABR_LKUP_BASE	0x1e  
#define FREEQ_ST_ADR	0x24  
#define FREEQ_ED_ADR	0x25  
#define FREEQ_RD_PTR	0x26  
#define FREEQ_WR_PTR	0x27  
#define PCQ_ST_ADR	0x28  
#define PCQ_ED_ADR	0x29  
#define PCQ_RD_PTR	0x2a  
#define PCQ_WR_PTR	0x2b  
#define EXCP_Q_ST_ADR	0x2c  
#define EXCP_Q_ED_ADR	0x2d  
#define EXCP_Q_RD_PTR	0x2e  
#define EXCP_Q_WR_PTR	0x2f  
#define CC_FIFO_ST_ADR	0x34  
#define CC_FIFO_ED_ADR	0x35  
#define CC_FIFO_RD_PTR	0x36  
#define CC_FIFO_WR_PTR	0x37  
#define STATE_REG	0x38  
#define BUF_SIZE	0x42  
#define XTRA_RM_OFFSET	0x44  
#define DRP_PKT_CNTR_NC	0x84  
#define ERR_CNTR_NC	0x85  
#define CELL_CNTR0_NC	0x8c  
#define CELL_CNTR1_NC	0x8d  
  
  
#define EXCPQ_EMPTY	0x0040  
#define PCQ_EMPTY	0x0010  
#define FREEQ_EMPTY	0x0004  
  
  
  
  
#define IPHASE5575_TX_COUNTER		0x200	  
#define IPHASE5575_RX_COUNTER		0x280	  
#define IPHASE5575_TX_LIST_ADDR		0x300	  
#define IPHASE5575_RX_LIST_ADDR		0x380	  
  
  
  
  
  
#define TX_DESC_BASE	0x0000	  
#define TX_COMP_Q	0x1000	  
#define PKT_RDY_Q	0x1400	  
#define CBR_SCHED_TABLE	0x1800	  
#define UBR_SCHED_TABLE	0x3000	  
#define UBR_WAIT_Q	0x4000	  
#define ABR_SCHED_TABLE	0x5000	  
#define ABR_WAIT_Q	0x5800	  
#define EXT_VC_TABLE	0x6000	  
#define MAIN_VC_TABLE	0x8000	  
#define SCHEDSZ		1024	  
#define TX_DESC_TABLE_SZ 128	
  
  
  
#define DESC_MODE	0x0  
#define VC_INDEX	0x1  
#define BYTE_CNT	0x3  
#define PKT_START_HI	0x4  
#define PKT_START_LO	0x5  
  
  
#define EOM_EN	0x0800  
#define AAL5	0x0100  
#define APP_CRC32 0x0400  
#define CMPL_INT  0x1000
  
#define TABLE_ADDRESS(db, dn, to) \
	(((unsigned long)(db & 0x04)) << 16) | (dn << 5) | (to << 1)  
  
  
#define RX_DESC_BASE	0x0000	  
#define VP_TABLE	0x5c00	  
#define EXCEPTION_Q	0x5e00	  
#define FREE_BUF_DESC_Q	0x6000	  
#define PKT_COMP_Q	0x6800	  
#define REASS_TABLE	0x7000	  
#define RX_VC_TABLE	0x7800	  
#define ABR_VC_TABLE	0x8000	  
#define RX_DESC_TABLE_SZ 736	
  
#define VP_TABLE_SZ	256	    
#define RX_VC_TABLE_SZ 	1024	   
#define REASS_TABLE_SZ 	1024	  
   
#define RX_ACT	0x8000  
#define RX_VPVC	0x4000  
#define RX_CNG	0x0040  
#define RX_CER	0x0008  
#define RX_PTE	0x0004  
#define RX_OFL	0x0002  
#define NUM_RX_EXCP   32

  
#define NO_AAL5_PKT	0x0000  
#define AAL5_PKT_REASSEMBLED 0x4000  
#define AAL5_PKT_TERMINATED 0x8000  
#define RAW_PKT		0xc000  
#define REASS_ABR	0x2000  
  
  
#define REG_BASE IPHASE5575_BUS_CONTROL_REG_BASE  
#define RAM_BASE IPHASE5575_FRAG_CONTROL_RAM_BASE  
#define PHY_BASE IPHASE5575_FRONT_END_REG_BASE  
#define SEG_BASE IPHASE5575_FRAG_CONTROL_REG_BASE  
#define REASS_BASE IPHASE5575_REASS_CONTROL_REG_BASE  

typedef volatile u_int  freg_t;
typedef u_int   rreg_t;

typedef struct _ffredn_t {
        freg_t  idlehead_high;  
        freg_t  idlehead_low;   
        freg_t  maxrate;        
        freg_t  stparms;        
        freg_t  abrubr_abr;     
        freg_t  rm_type;        
        u_int   filler5[0x17 - 0x06];
        freg_t  cmd_reg;        
        u_int   filler18[0x20 - 0x18];
        freg_t  cbr_base;       
        freg_t  vbr_base;       
        freg_t  abr_base;       
        freg_t  ubr_base;       
        u_int   filler24;
        freg_t  vbrwq_base;     
        freg_t  abrwq_base;     
        freg_t  ubrwq_base;     
        freg_t  vct_base;       
        freg_t  vcte_base;      
        u_int   filler2a[0x2C - 0x2A];
        freg_t  cbr_tab_beg;    
        freg_t  cbr_tab_end;    
        freg_t  cbr_pointer;    
        u_int   filler2f[0x30 - 0x2F];
        freg_t  prq_st_adr;     
        freg_t  prq_ed_adr;     
        freg_t  prq_rd_ptr;     
        freg_t  prq_wr_ptr;     
        freg_t  tcq_st_adr;     
        freg_t  tcq_ed_adr;     
        freg_t  tcq_rd_ptr;     
        freg_t  tcq_wr_ptr;     
        u_int   filler38[0x40 - 0x38];
        freg_t  queue_base;     
        freg_t  desc_base;      
        u_int   filler42[0x45 - 0x42];
        freg_t  mode_reg_0;     
        freg_t  mode_reg_1;     
        freg_t  intr_status_reg;
        freg_t  mask_reg;       
        freg_t  cell_ctr_high1; 
        freg_t  cell_ctr_lo1;   
        freg_t  state_reg;      
        u_int   filler4c[0x58 - 0x4c];
        freg_t  curr_desc_num;  
        freg_t  next_desc;      
        freg_t  next_vc;        
        u_int   filler5b[0x5d - 0x5b];
        freg_t  present_slot_cnt;
        u_int   filler5e[0x6a - 0x5e];
        freg_t  new_desc_num;   
        freg_t  new_vc;         
        freg_t  sched_tbl_ptr;  
        freg_t  vbrwq_wptr;     
        freg_t  vbrwq_rptr;     
        freg_t  abrwq_wptr;     
        freg_t  abrwq_rptr;     
        freg_t  ubrwq_wptr;     
        freg_t  ubrwq_rptr;     
        freg_t  cbr_vc;         
        freg_t  vbr_sb_vc;      
        freg_t  abr_sb_vc;      
        freg_t  ubr_sb_vc;      
        freg_t  vbr_next_link;  
        freg_t  abr_next_link;  
        freg_t  ubr_next_link;  
        u_int   filler7a[0x7c-0x7a];
        freg_t  out_rate_head;  
        u_int   filler7d[0xca-0x7d]; 
        freg_t  cell_ctr_high1_nc;
        freg_t  cell_ctr_lo1_nc;
        u_int   fillercc[0x100-0xcc]; 
} ffredn_t;

typedef struct _rfredn_t {
        rreg_t  mode_reg_0;     
        rreg_t  protocol_id;    
        rreg_t  mask_reg;       
        rreg_t  intr_status_reg;
        rreg_t  drp_pkt_cntr;   
        rreg_t  err_cntr;       
        u_int   filler6[0x08 - 0x06];
        rreg_t  raw_base_adr;   
        u_int   filler2[0x0c - 0x09];
        rreg_t  cell_ctr0;      
        rreg_t  cell_ctr1;      
        u_int   filler3[0x0f - 0x0e];
        rreg_t  cmd_reg;        
        rreg_t  desc_base;      
        rreg_t  vc_lkup_base;   
        rreg_t  reass_base;     
        rreg_t  queue_base;     
        u_int   filler14[0x16 - 0x14];
        rreg_t  pkt_tm_cnt;     
        rreg_t  tmout_range;    
        rreg_t  intrvl_cntr;    
        rreg_t  tmout_indx;     
        u_int   filler1a[0x1c - 0x1a];
        rreg_t  vp_lkup_base;   
        rreg_t  vp_filter;      
        rreg_t  abr_lkup_base;  
        u_int   filler1f[0x24 - 0x1f];
        rreg_t  fdq_st_adr;     
        rreg_t  fdq_ed_adr;     
        rreg_t  fdq_rd_ptr;     
        rreg_t  fdq_wr_ptr;     
        rreg_t  pcq_st_adr;     
        rreg_t  pcq_ed_adr;     
        rreg_t  pcq_rd_ptr;     
        rreg_t  pcq_wr_ptr;     
        rreg_t  excp_st_adr;    
        rreg_t  excp_ed_adr;    
        rreg_t  excp_rd_ptr;    
        rreg_t  excp_wr_ptr;    
        u_int   filler30[0x34 - 0x30];
        rreg_t  raw_st_adr;     
        rreg_t  raw_ed_adr;     
        rreg_t  raw_rd_ptr;     
        rreg_t  raw_wr_ptr;     
        rreg_t  state_reg;      
        u_int   filler39[0x42 - 0x39];
        rreg_t  buf_size;       
        u_int   filler43;
        rreg_t  xtra_rm_offset; 
        u_int   filler45[0x84 - 0x45];
        rreg_t  drp_pkt_cntr_nc;
        rreg_t  err_cntr_nc;    
        u_int   filler86[0x8c - 0x86];
        rreg_t  cell_ctr0_nc;   
        rreg_t  cell_ctr1_nc;   
        u_int   filler8e[0x100-0x8e]; 
} rfredn_t;

typedef struct {
        
        ffredn_t        ffredn;         
        rfredn_t        rfredn;         
} ia_regs_t;

typedef struct {
	u_short		f_vc_type;	
	u_short		f_nrm;		
	u_short		f_nrmexp;	
	u_short		reserved6;	
	u_short		f_crm;		
	u_short		reserved10;	
	u_short		reserved12;	
	u_short		reserved14;	
	u_short		last_cell_slot;	
	u_short		f_pcr;		
	u_short		fraction;	
	u_short		f_icr;		
	u_short		f_cdf;		
	u_short		f_mcr;		
	u_short		f_acr;		
	u_short		f_status;	
} f_vc_abr_entry;

typedef struct {
        u_short         r_status_rdf;   
        u_short         r_air;          
        u_short         reserved4[14];  
} r_vc_abr_entry;   

#define MRM 3

typedef struct srv_cls_param {
        u32 class_type;         
        u32 pcr;                 
        
        u32 scr;                
        u32 max_burst_size;     
 
        
        u32 mcr;                
        u32 icr;                
        u32 tbe;                
        u32 frtt;               
 
#if 0   
bits  31          30           29          28       27-25 24-22 21-19  18-9
-----------------------------------------------------------------------------
| NRM present | TRM prsnt | CDF prsnt | ADTF prsnt | NRM | TRM | CDF | ADTF |
-----------------------------------------------------------------------------
#endif 
 
        u8 nrm;                 
        u8 trm;                 
        u16 adtf;               
        u8 cdf;                 
        u8 rif;                 
        u8 rdf;                 
        u8 reserved;            
} srv_cls_param_t;

struct testTable_t {
	u16 lastTime; 
	u16 fract; 
	u8 vc_status;
}; 

typedef struct {
	u16 vci;
	u16 error;
} RX_ERROR_Q;

typedef struct {
	u8 active: 1; 
	u8 abr: 1; 
	u8 ubr: 1; 
	u8 cnt: 5;
#define VC_ACTIVE 	0x01
#define VC_ABR		0x02
#define VC_UBR		0x04
} vcstatus_t;
  
struct ia_rfL_t {
    	u32  fdq_st;     
        u32  fdq_ed;     
        u32  fdq_rd;     
        u32  fdq_wr;     
        u32  pcq_st;     
        u32  pcq_ed;     
        u32  pcq_rd;     
        u32  pcq_wr;      
};

struct ia_ffL_t {
	u32  prq_st;     
        u32  prq_ed;     
        u32  prq_wr;     
        u32  tcq_st;     
        u32  tcq_ed;     
        u32  tcq_rd;     
};

struct desc_tbl_t {
        u32 timestamp;
        struct ia_vcc *iavcc;
        struct sk_buff *txskb;
}; 

typedef struct ia_rtn_q {
   struct desc_tbl_t data;
   struct ia_rtn_q *next, *tail;
} IARTN_Q;

#define SUNI_LOSV   	0x04
enum ia_suni {
	SUNI_MASTER_RESET	= 0x000, 
	SUNI_MASTER_CONFIG	= 0x004, 
	SUNI_MASTER_INTR_STAT	= 0x008, 
	SUNI_RESERVED1		= 0x00c, 
	SUNI_MASTER_CLK_MONITOR	= 0x010, 
	SUNI_MASTER_CONTROL	= 0x014, 
					 
	SUNI_RSOP_CONTROL	= 0x040, 
	SUNI_RSOP_STATUS	= 0x044, 
	SUNI_RSOP_SECTION_BIP8L	= 0x048, 
	SUNI_RSOP_SECTION_BIP8M	= 0x04c, 

	SUNI_TSOP_CONTROL	= 0x050, 
	SUNI_TSOP_DIAG		= 0x054, 
					 
	SUNI_RLOP_CS		= 0x060, 
	SUNI_RLOP_INTR		= 0x064, 
	SUNI_RLOP_LINE_BIP24L	= 0x068, 
	SUNI_RLOP_LINE_BIP24	= 0x06c, 
	SUNI_RLOP_LINE_BIP24M	= 0x070, 
	SUNI_RLOP_LINE_FEBEL	= 0x074, 
	SUNI_RLOP_LINE_FEBE	= 0x078, 
	SUNI_RLOP_LINE_FEBEM	= 0x07c, 

	SUNI_TLOP_CONTROL	= 0x080, 
	SUNI_TLOP_DISG		= 0x084, 
					 
	SUNI_RPOP_CS		= 0x0c0, 
	SUNI_RPOP_INTR		= 0x0c4, 
	SUNI_RPOP_RESERVED	= 0x0c8, 
	SUNI_RPOP_INTR_ENA	= 0x0cc, 
					 
	SUNI_RPOP_PATH_SIG	= 0x0dc, 
	SUNI_RPOP_BIP8L		= 0x0e0, 
	SUNI_RPOP_BIP8M		= 0x0e4, 
	SUNI_RPOP_FEBEL		= 0x0e8, 
	SUNI_RPOP_FEBEM		= 0x0ec, 
					 
	SUNI_TPOP_CNTRL_DAIG	= 0x100, 
	SUNI_TPOP_POINTER_CTRL	= 0x104, 
	SUNI_TPOP_SOURCER_CTRL	= 0x108, 
					 
	SUNI_TPOP_ARB_PRTL	= 0x114, 
	SUNI_TPOP_ARB_PRTM	= 0x118, 
	SUNI_TPOP_RESERVED2	= 0x11c, 
	SUNI_TPOP_PATH_SIG	= 0x120, 
	SUNI_TPOP_PATH_STATUS	= 0x124, 
					 
	SUNI_RACP_CS		= 0x140, 
	SUNI_RACP_INTR		= 0x144, 
	SUNI_RACP_HDR_PATTERN	= 0x148, 
	SUNI_RACP_HDR_MASK	= 0x14c, 
	SUNI_RACP_CORR_HCS	= 0x150, 
	SUNI_RACP_UNCORR_HCS	= 0x154, 
					 
	SUNI_TACP_CONTROL	= 0x180, 
	SUNI_TACP_IDLE_HDR_PAT	= 0x184, 
	SUNI_TACP_IDLE_PAY_PAY	= 0x188, 
					 
					 
	SUNI_RESERVED_TEST	= 0x204  
};

typedef struct _SUNI_STATS_
{
   u32 valid;                       
   u32 carrier_detect;              
   
   u16 rsop_oof_state;              
   u16 rsop_lof_state;              
   u16 rsop_los_state;              
   u32 rsop_los_count;              
   u32 rsop_bse_count;              
   
   u16 rlop_ferf_state;             
   u16 rlop_lais_state;             
   u32 rlop_lbe_count;              
   u32 rlop_febe_count;             
   
   u16 rpop_lop_state;              
   u16 rpop_pais_state;             
   u16 rpop_pyel_state;             
   u32 rpop_bip_count;              
   u32 rpop_febe_count;             
   u16 rpop_psig;                   
   
   u16 racp_hp_state;               
   u32 racp_fu_count;               
   u32 racp_fo_count;               
   u32 racp_chcs_count;             
   u32 racp_uchcs_count;            
} IA_SUNI_STATS; 

typedef struct iadev_priv {
	   
	u32 __iomem *phy;	
	u32 __iomem *dma;	
	u32 __iomem *reg;	
	u32 __iomem *seg_reg;		
  
	u32 __iomem *reass_reg;		
  
	u32 __iomem *ram;		  
	void __iomem *seg_ram;  
	void __iomem *reass_ram;  
	struct dle_q tx_dle_q;  
	struct free_desc_q *tx_free_desc_qhead;  
	struct sk_buff_head tx_dma_q, tx_backlog;  
        spinlock_t            tx_lock;
        IARTN_Q               tx_return_q;
        u32                   close_pending;
        wait_queue_head_t    close_wait;
        wait_queue_head_t    timeout_wait;
	struct cpcs_trailer_desc *tx_buf;
        u16 num_tx_desc, tx_buf_sz, rate_limit;
        u32 tx_cell_cnt, tx_pkt_cnt;
        void __iomem *MAIN_VC_TABLE_ADDR, *EXT_VC_TABLE_ADDR, *ABR_SCHED_TABLE_ADDR;
	struct dle_q rx_dle_q;  
	struct free_desc_q *rx_free_desc_qhead;  
	struct sk_buff_head rx_dma_q;  
	spinlock_t rx_lock;
	struct atm_vcc **rx_open;	  
        u16 num_rx_desc, rx_buf_sz, rxing;
        u32 rx_pkt_ram, rx_tmp_cnt;
        unsigned long rx_tmp_jif;
        void __iomem *RX_DESC_BASE_ADDR;
        u32 drop_rxpkt, drop_rxcell, rx_cell_cnt, rx_pkt_cnt;
	struct atm_dev *next_board;	  
	struct pci_dev *pci;  
	int mem;  
	unsigned int real_base;	  
	void __iomem *base;
	unsigned int pci_map_size;	  
	unsigned char irq;  
	unsigned char bus;  
	unsigned char dev_fn;  
        u_short  phy_type;
        u_short  num_vc, memSize, memType;
        struct ia_ffL_t ffL;
        struct ia_rfL_t rfL;
        
        
        unsigned char carrier_detect;
        
        
        unsigned int tx_dma_cnt;     
        unsigned int rx_dma_cnt;     
        unsigned int NumEnabledCBR;  
        
        unsigned int rx_mark_cnt;    
        unsigned int CbrTotEntries;  
        unsigned int CbrRemEntries;  
        unsigned int CbrEntryPt;     
        unsigned int Granularity;    
        
	unsigned int  sum_mcr, sum_cbr, LineRate;
	unsigned int  n_abr;
        struct desc_tbl_t *desc_tbl;
        u_short host_tcq_wr;
        struct testTable_t **testTable;
	dma_addr_t tx_dle_dma;
	dma_addr_t rx_dle_dma;
} IADEV;
  
  
#define INPH_IA_DEV(d) ((IADEV *) (d)->dev_data)  
#define INPH_IA_VCC(v) ((struct ia_vcc *) (v)->dev_data)  

enum ia_mb25 {
	MB25_MASTER_CTRL	= 0x00, 
	MB25_INTR_STATUS	= 0x04,	
	MB25_DIAG_CONTROL	= 0x08,	
	MB25_LED_HEC		= 0x0c,	
	MB25_LOW_BYTE_COUNTER	= 0x10,
	MB25_HIGH_BYTE_COUNTER	= 0x14
};

#define	MB25_MC_UPLO	0x80		
#define	MB25_MC_DREC	0x40		
#define	MB25_MC_ECEIO	0x20		
#define	MB25_MC_TDPC	0x10		
#define	MB25_MC_DRIC	0x08		
#define	MB25_MC_HALTTX	0x04		
#define	MB25_MC_UMS	0x02		
#define	MB25_MC_ENABLED	0x01		

#define	MB25_IS_GSB	0x40			
#define	MB25_IS_HECECR	0x20		
#define	MB25_IS_SCR	0x10		
#define	MB25_IS_TPE	0x08		
#define	MB25_IS_RSCC	0x04		
#define	MB25_IS_RCSE	0x02		
#define	MB25_IS_RFIFOO	0x01		

#define	MB25_DC_FTXCD	0x80			
#define	MB25_DC_RXCOS	0x40		
#define	MB25_DC_ECEIO	0x20		
#define	MB25_DC_RLFLUSH	0x10		
#define	MB25_DC_IXPE	0x08		
#define	MB25_DC_IXHECE	0x04		
#define	MB25_DC_LB_MASK	0x03		

#define	MB25_DC_LL	0x03		
#define	MB25_DC_PL	0x02		
#define	MB25_DC_NM	0x00		

#define FE_MASK 	0x00F0
#define FE_MULTI_MODE	0x0000
#define FE_SINGLE_MODE  0x0010 
#define FE_UTP_OPTION  	0x0020
#define FE_25MBIT_PHY	0x0040
#define FE_DS3_PHY      0x0080          
#define FE_E3_PHY       0x0090          
		     
enum suni_pm7345 {
	SUNI_CONFIG			= 0x000, 
	SUNI_INTR_ENBL			= 0x004, 
	SUNI_INTR_STAT			= 0x008, 
	SUNI_CONTROL			= 0x00c, 
	SUNI_ID_RESET			= 0x010, 
	SUNI_DATA_LINK_CTRL		= 0x014,
	SUNI_RBOC_CONF_INTR_ENBL	= 0x018,
	SUNI_RBOC_STAT			= 0x01c,
	SUNI_DS3_FRM_CFG		= 0x020,
	SUNI_DS3_FRM_INTR_ENBL		= 0x024,
	SUNI_DS3_FRM_INTR_STAT		= 0x028,
	SUNI_DS3_FRM_STAT		= 0x02c,
	SUNI_RFDL_CFG			= 0x030,
	SUNI_RFDL_ENBL_STAT		= 0x034,
	SUNI_RFDL_STAT			= 0x038,
	SUNI_RFDL_DATA			= 0x03c,
	SUNI_PMON_CHNG			= 0x040,
	SUNI_PMON_INTR_ENBL_STAT	= 0x044,
	
	SUNI_PMON_LCV_EVT_CNT_LSB	= 0x050,
	SUNI_PMON_LCV_EVT_CNT_MSB	= 0x054,
	SUNI_PMON_FBE_EVT_CNT_LSB	= 0x058,
	SUNI_PMON_FBE_EVT_CNT_MSB	= 0x05c,
	SUNI_PMON_SEZ_DET_CNT_LSB	= 0x060,
	SUNI_PMON_SEZ_DET_CNT_MSB	= 0x064,
	SUNI_PMON_PE_EVT_CNT_LSB	= 0x068,
	SUNI_PMON_PE_EVT_CNT_MSB	= 0x06c,
	SUNI_PMON_PPE_EVT_CNT_LSB	= 0x070,
	SUNI_PMON_PPE_EVT_CNT_MSB	= 0x074,
	SUNI_PMON_FEBE_EVT_CNT_LSB	= 0x078,
	SUNI_PMON_FEBE_EVT_CNT_MSB	= 0x07c,
	SUNI_DS3_TRAN_CFG		= 0x080,
	SUNI_DS3_TRAN_DIAG		= 0x084,
	
	SUNI_XFDL_CFG			= 0x090,
	SUNI_XFDL_INTR_ST		= 0x094,
	SUNI_XFDL_XMIT_DATA		= 0x098,
	SUNI_XBOC_CODE			= 0x09c,
	SUNI_SPLR_CFG			= 0x0a0,
	SUNI_SPLR_INTR_EN		= 0x0a4,
	SUNI_SPLR_INTR_ST		= 0x0a8,
	SUNI_SPLR_STATUS		= 0x0ac,
	SUNI_SPLT_CFG			= 0x0b0,
	SUNI_SPLT_CNTL			= 0x0b4,
	SUNI_SPLT_DIAG_G1		= 0x0b8,
	SUNI_SPLT_F1			= 0x0bc,
	SUNI_CPPM_LOC_METERS		= 0x0c0,
	SUNI_CPPM_CHG_OF_CPPM_PERF_METR	= 0x0c4,
	SUNI_CPPM_B1_ERR_CNT_LSB	= 0x0c8,
	SUNI_CPPM_B1_ERR_CNT_MSB	= 0x0cc,
	SUNI_CPPM_FRAMING_ERR_CNT_LSB	= 0x0d0,
	SUNI_CPPM_FRAMING_ERR_CNT_MSB	= 0x0d4,
	SUNI_CPPM_FEBE_CNT_LSB		= 0x0d8,
	SUNI_CPPM_FEBE_CNT_MSB		= 0x0dc,
	SUNI_CPPM_HCS_ERR_CNT_LSB	= 0x0e0,
	SUNI_CPPM_HCS_ERR_CNT_MSB	= 0x0e4,
	SUNI_CPPM_IDLE_UN_CELL_CNT_LSB	= 0x0e8,
	SUNI_CPPM_IDLE_UN_CELL_CNT_MSB	= 0x0ec,
	SUNI_CPPM_RCV_CELL_CNT_LSB	= 0x0f0,
	SUNI_CPPM_RCV_CELL_CNT_MSB	= 0x0f4,
	SUNI_CPPM_XMIT_CELL_CNT_LSB	= 0x0f8,
	SUNI_CPPM_XMIT_CELL_CNT_MSB	= 0x0fc,
	SUNI_RXCP_CTRL			= 0x100,
	SUNI_RXCP_FCTRL			= 0x104,
	SUNI_RXCP_INTR_EN_STS		= 0x108,
	SUNI_RXCP_IDLE_PAT_H1		= 0x10c,
	SUNI_RXCP_IDLE_PAT_H2		= 0x110,
	SUNI_RXCP_IDLE_PAT_H3		= 0x114,
	SUNI_RXCP_IDLE_PAT_H4		= 0x118,
	SUNI_RXCP_IDLE_MASK_H1		= 0x11c,
	SUNI_RXCP_IDLE_MASK_H2		= 0x120,
	SUNI_RXCP_IDLE_MASK_H3		= 0x124,
	SUNI_RXCP_IDLE_MASK_H4		= 0x128,
	SUNI_RXCP_CELL_PAT_H1		= 0x12c,
	SUNI_RXCP_CELL_PAT_H2		= 0x130,
	SUNI_RXCP_CELL_PAT_H3		= 0x134,
	SUNI_RXCP_CELL_PAT_H4		= 0x138,
	SUNI_RXCP_CELL_MASK_H1		= 0x13c,
	SUNI_RXCP_CELL_MASK_H2		= 0x140,
	SUNI_RXCP_CELL_MASK_H3		= 0x144,
	SUNI_RXCP_CELL_MASK_H4		= 0x148,
	SUNI_RXCP_HCS_CS		= 0x14c,
	SUNI_RXCP_LCD_CNT_THRESHOLD	= 0x150,
	
	SUNI_TXCP_CTRL			= 0x160,
	SUNI_TXCP_INTR_EN_STS		= 0x164,
	SUNI_TXCP_IDLE_PAT_H1		= 0x168,
	SUNI_TXCP_IDLE_PAT_H2		= 0x16c,
	SUNI_TXCP_IDLE_PAT_H3		= 0x170,
	SUNI_TXCP_IDLE_PAT_H4		= 0x174,
	SUNI_TXCP_IDLE_PAT_H5		= 0x178,
	SUNI_TXCP_IDLE_PAYLOAD		= 0x17c,
	SUNI_E3_FRM_FRAM_OPTIONS	= 0x180,
	SUNI_E3_FRM_MAINT_OPTIONS	= 0x184,
	SUNI_E3_FRM_FRAM_INTR_ENBL	= 0x188,
	SUNI_E3_FRM_FRAM_INTR_IND_STAT	= 0x18c,
	SUNI_E3_FRM_MAINT_INTR_ENBL	= 0x190,
	SUNI_E3_FRM_MAINT_INTR_IND	= 0x194,
	SUNI_E3_FRM_MAINT_STAT		= 0x198,
	SUNI_RESERVED4			= 0x19c,
	SUNI_E3_TRAN_FRAM_OPTIONS	= 0x1a0,
	SUNI_E3_TRAN_STAT_DIAG_OPTIONS	= 0x1a4,
	SUNI_E3_TRAN_BIP_8_ERR_MASK	= 0x1a8,
	SUNI_E3_TRAN_MAINT_ADAPT_OPTS	= 0x1ac,
	SUNI_TTB_CTRL			= 0x1b0,
	SUNI_TTB_TRAIL_TRACE_ID_STAT	= 0x1b4,
	SUNI_TTB_IND_ADDR		= 0x1b8,
	SUNI_TTB_IND_DATA		= 0x1bc,
	SUNI_TTB_EXP_PAYLOAD_TYPE	= 0x1c0,
	SUNI_TTB_PAYLOAD_TYPE_CTRL_STAT	= 0x1c4,
	
	SUNI_MASTER_TEST		= 0x200,
	
};

#define SUNI_PM7345_T suni_pm7345_t
#define SUNI_PM7345     0x20            
#define SUNI_PM5346     0x30            
#define SUNI_PM7345_CLB         0x01    
#define SUNI_PM7345_PLB         0x02    
#define SUNI_PM7345_DLB         0x04    
#define SUNI_PM7345_LLB         0x80    
#define SUNI_PM7345_E3ENBL      0x40    
#define SUNI_PM7345_LOOPT       0x10    
#define SUNI_PM7345_FIFOBP      0x20    
#define SUNI_PM7345_FRMRBP      0x08    
#define SUNI_DS3_COFAE  0x80            
#define SUNI_DS3_REDE   0x40            
#define SUNI_DS3_CBITE  0x20            
#define SUNI_DS3_FERFE  0x10            
#define SUNI_DS3_IDLE   0x08            
#define SUNI_DS3_AISE   0x04            
#define SUNI_DS3_OOFE   0x02            
#define SUNI_DS3_LOSE   0x01            
 
#define SUNI_DS3_ACE    0x80            
#define SUNI_DS3_REDV   0x40            
#define SUNI_DS3_CBITV  0x20            
#define SUNI_DS3_FERFV  0x10            
#define SUNI_DS3_IDLV   0x08            
#define SUNI_DS3_AISV   0x04            
#define SUNI_DS3_OOFV   0x02            
#define SUNI_DS3_LOSV   0x01            

#define SUNI_E3_CZDI    0x40            
#define SUNI_E3_LOSI    0x20            
#define SUNI_E3_LCVI    0x10            
#define SUNI_E3_COFAI   0x08            
#define SUNI_E3_OOFI    0x04            
#define SUNI_E3_LOS     0x02            
#define SUNI_E3_OOF     0x01            

#define SUNI_E3_AISD    0x80            
#define SUNI_E3_FERF_RAI        0x40    
#define SUNI_E3_FEBE    0x20            

#define SUNI_DS3_HCSPASS        0x80    
#define SUNI_DS3_HCSDQDB        0x40    
#define SUNI_DS3_HCSADD         0x20    
#define SUNI_DS3_HCK            0x10    
#define SUNI_DS3_BLOCK          0x08    
#define SUNI_DS3_DSCR           0x04    
#define SUNI_DS3_OOCDV          0x02    
#define SUNI_DS3_FIFORST        0x01    

#define SUNI_DS3_OOCDE  0x80            
#define SUNI_DS3_HCSE   0x40            
#define SUNI_DS3_FIFOE  0x20            
#define SUNI_DS3_OOCDI  0x10            
#define SUNI_DS3_UHCSI  0x08            
#define SUNI_DS3_COCAI  0x04            
#define SUNI_DS3_FOVRI  0x02            
#define SUNI_DS3_FUDRI  0x01            


#define MEM_SIZE_MASK   0x000F          
#define MEM_SIZE_128K   0x0000          
#define MEM_SIZE_512K   0x0001          
#define MEM_SIZE_1M     0x0002          
                                        

#define FE_MASK         0x00F0          
#define FE_MULTI_MODE   0x0000          
#define FE_SINGLE_MODE  0x0010          
#define FE_UTP_OPTION   0x0020          

#define	NOVRAM_SIZE	64
#define	CMD_LEN		10



#define	EXTEND	0x100
#define	IAWRITE	0x140
#define	IAREAD	0x180
#define	ERASE	0x1c0

#define	EWDS	0x00
#define	WRAL	0x10
#define	ERAL	0x20
#define	EWEN	0x30


#define	NVCE	0x02
#define	NVSK	0x01
#define	NVDO	0x08	
#define NVDI	0x04

#define	CFG_AND(val) { \
		u32 t; \
		t = readl(iadev->reg+IPHASE5575_EEPROM_ACCESS); \
		t &= (val); \
		writel(t, iadev->reg+IPHASE5575_EEPROM_ACCESS); \
	}


#define	CFG_OR(val) { \
		u32 t; \
		t =  readl(iadev->reg+IPHASE5575_EEPROM_ACCESS); \
		t |= (val); \
		writel(t, iadev->reg+IPHASE5575_EEPROM_ACCESS); \
	}


#define	NVRAM_CMD(cmd) { \
		int	i; \
		u_short c = cmd; \
		CFG_AND(~(NVCE|NVSK)); \
		CFG_OR(NVCE); \
		for (i=0; i<CMD_LEN; i++) { \
			NVRAM_CLKOUT((c & (1 << (CMD_LEN - 1))) ? 1 : 0); \
			c <<= 1; \
		} \
	}


#define	NVRAM_CLR_CE	{CFG_AND(~NVCE)}


#define	NVRAM_CLKOUT(bitval) { \
		CFG_AND(~NVDI); \
		CFG_OR((bitval) ? NVDI : 0); \
		CFG_OR(NVSK); \
		CFG_AND( ~NVSK); \
	}


#define	NVRAM_CLKIN(value) { \
		u32 _t; \
		CFG_OR(NVSK); \
		CFG_AND(~NVSK); \
		_t = readl(iadev->reg+IPHASE5575_EEPROM_ACCESS); \
		value = (_t & NVDO) ? 1 : 0; \
	}


#endif 
