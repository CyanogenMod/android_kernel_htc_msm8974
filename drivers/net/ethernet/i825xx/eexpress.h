

#define DATAPORT      0x0000
#define WRITE_PTR     0x0002
#define READ_PTR      0x0004
#define SIGNAL_CA     0x0006
#define SET_IRQ       0x0007
#define SM_PTR        0x0008
#define	MEM_Dec	      0x000a
#define MEM_Ctrl      0x000b
#define MEM_Page_Ctrl 0x000c
#define Config        0x000d
#define EEPROM_Ctrl   0x000e
#define ID_PORT       0x000f
#define	MEM_ECtrl     0x000f


#define SIRQ_en       0x08
#define SIRQ_dis      0x00

#define EC_Clk        0x01
#define EC_CS         0x02
#define EC_Wr         0x04
#define EC_Rd         0x08
#define ASIC_RST      0x40
#define i586_RST      0x80

#define eeprom_delay() { udelay(40); }


#define SCP_START 0xfff6

#define ISCP_START 0x0000

#define SCB_START 0x0008


#define TX_BUF_START 0x0100

#define TX_BUF_SIZE ((24+ETH_FRAME_LEN+31)&~0x1f)
#define RX_BUF_SIZE ((32+ETH_FRAME_LEN+31)&~0x1f)


#define SCB_complete(s) (((s) & 0x8000) != 0)
#define SCB_rxdframe(s) (((s) & 0x4000) != 0)
#define SCB_CUdead(s)   (((s) & 0x2000) != 0)
#define SCB_RUdead(s)   (((s) & 0x1000) != 0)
#define SCB_ack(s)      ((s) & 0xf000)

#define SCB_CUstat(s)   (((s)&0x0300)>>8)

#define SCB_RUstat(s)   (((s)&0x0070)>>4)

#define SCB_CUnop       0x0000
#define SCB_CUstart     0x0100
#define SCB_CUresume    0x0200
#define SCB_CUsuspend   0x0300
#define SCB_CUabort     0x0400
#define SCB_resetchip   0x0080

#define SCB_RUnop       0x0000
#define SCB_RUstart     0x0010
#define SCB_RUresume    0x0020
#define SCB_RUsuspend   0x0030
#define SCB_RUabort     0x0040


#define Stat_Done(s)    (((s) & 0x8000) != 0)
#define Stat_Busy(s)    (((s) & 0x4000) != 0)
#define Stat_OK(s)      (((s) & 0x2000) != 0)
#define Stat_Abort(s)   (((s) & 0x1000) != 0)
#define Stat_STFail     (((s) & 0x0800) != 0)
#define Stat_TNoCar(s)  (((s) & 0x0400) != 0)
#define Stat_TNoCTS(s)  (((s) & 0x0200) != 0)
#define Stat_TNoDMA(s)  (((s) & 0x0100) != 0)
#define Stat_TDefer(s)  (((s) & 0x0080) != 0)
#define Stat_TColl(s)   (((s) & 0x0040) != 0)
#define Stat_TXColl(s)  (((s) & 0x0020) != 0)
#define Stat_NoColl(s)  ((s) & 0x000f)


#define Cmd_END     0x8000
#define Cmd_SUS     0x4000
#define Cmd_INT     0x2000

#define Cmd_Nop     0x0000
#define Cmd_SetAddr 0x0001
#define Cmd_Config  0x0002
#define Cmd_MCast   0x0003
#define Cmd_Xmit    0x0004
#define Cmd_TDR     0x0005
#define Cmd_Dump    0x0006
#define Cmd_Diag    0x0007



#define FD_Done(s)  (((s) & 0x8000) != 0)
#define FD_Busy(s)  (((s) & 0x4000) != 0)
#define FD_OK(s)    (((s) & 0x2000) != 0)

#define FD_CRC(s)   (((s) & 0x0800) != 0)
#define FD_Align(s) (((s) & 0x0400) != 0)
#define FD_Resrc(s) (((s) & 0x0200) != 0)
#define FD_DMA(s)   (((s) & 0x0100) != 0)
#define FD_Short(s) (((s) & 0x0080) != 0)
#define FD_NoEOF(s) (((s) & 0x0040) != 0)

struct rfd_header {
	volatile unsigned long flags;
	volatile unsigned short link;
	volatile unsigned short rbd_offset;
	volatile unsigned short dstaddr1;
	volatile unsigned short dstaddr2;
	volatile unsigned short dstaddr3;
	volatile unsigned short srcaddr1;
	volatile unsigned short srcaddr2;
	volatile unsigned short srcaddr3;
	volatile unsigned short length;

	volatile unsigned short actual_count;
	volatile unsigned short next_rbd;
	volatile unsigned short buf_addr1;
	volatile unsigned short buf_addr2;
	volatile unsigned short size;
};


#define TDR_LINKOK       (1<<15)
#define TDR_XCVRPROBLEM  (1<<14)
#define TDR_OPEN         (1<<13)
#define TDR_SHORT        (1<<12)
#define TDR_TIME         0x7ff
