/*
 * $Id: synclink.c,v 4.38 2005/11/07 16:30:34 paulkf Exp $
 *
 * Device driver for Microgate SyncLink ISA and PCI
 * high speed multiprotocol serial adapters.
 *
 * written by Paul Fulghum for Microgate Corporation
 * paulkf@microgate.com
 *
 * Microgate and SyncLink are trademarks of Microgate Corporation
 *
 * Derived from serial.c written by Theodore Ts'o and Linus Torvalds
 *
 * Original release 01/11/99
 *
 * This code is released under the GNU General Public License (GPL)
 *
 * This driver is primarily intended for use in synchronous
 * HDLC mode. Asynchronous mode is also provided.
 *
 * When operating in synchronous mode, each call to mgsl_write()
 * contains exactly one complete HDLC frame. Calling mgsl_put_char
 * will start assembling an HDLC frame that will not be sent until
 * mgsl_flush_chars or mgsl_write is called.
 * 
 * Synchronous receive data is reported as complete frames. To accomplish
 * this, the TTY flip buffer is bypassed (too small to hold largest
 * frame and may fragment frames) and the line discipline
 * receive entry point is called directly.
 *
 * This driver has been tested with a slightly modified ppp.c driver
 * for synchronous PPP.
 *
 * 2000/02/16
 * Added interface for syncppp.c driver (an alternate synchronous PPP
 * implementation that also supports Cisco HDLC). Each device instance
 * registers as a tty device AND a network device (if dosyncppp option
 * is set for the device). The functionality is determined by which
 * device interface is opened.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(__i386__)
#  define BREAKPOINT() asm("   int $3");
#else
#  define BREAKPOINT() { }
#endif

#define MAX_ISA_DEVICES 10
#define MAX_PCI_DEVICES 10
#define MAX_TOTAL_DEVICES 20

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/synclink.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/dma.h>
#include <linux/bitops.h>
#include <asm/types.h>
#include <linux/termios.h>
#include <linux/workqueue.h>
#include <linux/hdlc.h>
#include <linux/dma-mapping.h>

#if defined(CONFIG_HDLC) || (defined(CONFIG_HDLC_MODULE) && defined(CONFIG_SYNCLINK_MODULE))
#define SYNCLINK_GENERIC_HDLC 1
#else
#define SYNCLINK_GENERIC_HDLC 0
#endif

#define GET_USER(error,value,addr) error = get_user(value,addr)
#define COPY_FROM_USER(error,dest,src,size) error = copy_from_user(dest,src,size) ? -EFAULT : 0
#define PUT_USER(error,value,addr) error = put_user(value,addr)
#define COPY_TO_USER(error,dest,src,size) error = copy_to_user(dest,src,size) ? -EFAULT : 0

#include <asm/uaccess.h>

#define RCLRVALUE 0xffff

static MGSL_PARAMS default_params = {
	MGSL_MODE_HDLC,			
	0,				
	HDLC_FLAG_UNDERRUN_ABORT15,	
	HDLC_ENCODING_NRZI_SPACE,	
	0,				
	0xff,				
	HDLC_CRC_16_CCITT,		
	HDLC_PREAMBLE_LENGTH_8BITS,	
	HDLC_PREAMBLE_PATTERN_NONE,	
	9600,				
	8,				
	1,				
	ASYNC_PARITY_NONE		
};

#define SHARED_MEM_ADDRESS_SIZE 0x40000
#define BUFFERLISTSIZE 4096
#define DMABUFFERSIZE 4096
#define MAXRXFRAMES 7

typedef struct _DMABUFFERENTRY
{
	u32 phys_addr;	
	volatile u16 count;	
	volatile u16 status;	
	volatile u16 rcc;	
	u16 reserved;	
	u32 link;	
	char *virt_addr;	
	u32 phys_entry;	
	dma_addr_t dma_addr;
} DMABUFFERENTRY, *DMAPBUFFERENTRY;


#define BH_RECEIVE  1
#define BH_TRANSMIT 2
#define BH_STATUS   4

#define IO_PIN_SHUTDOWN_LIMIT 100

struct	_input_signal_events {
	int	ri_up;	
	int	ri_down;
	int	dsr_up;
	int	dsr_down;
	int	dcd_up;
	int	dcd_down;
	int	cts_up;
	int	cts_down;
};

#define MAX_TX_HOLDING_BUFFERS 5
struct tx_holding_buffer {
	int	buffer_size;
	unsigned char *	buffer;
};


 
struct mgsl_struct {
	int			magic;
	struct tty_port		port;
	int			line;
	int                     hw_version;
	
	struct mgsl_icount	icount;
	
	int			timeout;
	int			x_char;		
	u16			read_status_mask;
	u16			ignore_status_mask;	
	unsigned char 		*xmit_buf;
	int			xmit_head;
	int			xmit_tail;
	int			xmit_cnt;
	
	wait_queue_head_t	status_event_wait_q;
	wait_queue_head_t	event_wait_q;
	struct timer_list	tx_timer;	
	struct mgsl_struct	*next_device;	
	
	spinlock_t irq_spinlock;		
	struct work_struct task;		

	u32 EventMask;			
	u32 RecordedEvents;		

	u32 max_frame_size;		

	u32 pending_bh;

	bool bh_running;		
	int isr_overflow;
	bool bh_requested;
	
	int dcd_chkcount;		
	int cts_chkcount;		
	int dsr_chkcount;		
	int ri_chkcount;

	char *buffer_list;		
	u32 buffer_list_phys;
	dma_addr_t buffer_list_dma_addr;

	unsigned int rx_buffer_count;	
	DMABUFFERENTRY *rx_buffer_list;	
	unsigned int current_rx_buffer;

	int num_tx_dma_buffers;		
 	int tx_dma_buffers_used;
	unsigned int tx_buffer_count;	
	DMABUFFERENTRY *tx_buffer_list;	
	int start_tx_dma_buffer;	
	int current_tx_buffer;          
	
	unsigned char *intermediate_rxbuffer;

	int num_tx_holding_buffers;	
	int get_tx_holding_index;  	
	int put_tx_holding_index;  	
	int tx_holding_count;		
	struct tx_holding_buffer tx_holding_buffers[MAX_TX_HOLDING_BUFFERS];

	bool rx_enabled;
	bool rx_overflow;
	bool rx_rcc_underrun;

	bool tx_enabled;
	bool tx_active;
	u32 idle_mode;

	u16 cmr_value;
	u16 tcsr_value;

	char device_name[25];		

	unsigned int bus_type;	
	unsigned char bus;		
	unsigned char function;		

	unsigned int io_base;		
	unsigned int io_addr_size;	
	bool io_addr_requested;		
	
	unsigned int irq_level;		
	unsigned long irq_flags;
	bool irq_requested;		
	
	unsigned int dma_level;		
	bool dma_requested;		

	u16 mbre_bit;
	u16 loopback_bits;
	u16 usc_idle_mode;

	MGSL_PARAMS params;		

	unsigned char serial_signals;	

	bool irq_occurred;		
	unsigned int init_error;	
	int	fDiagnosticsmode;	

	u32 last_mem_alloc;
	unsigned char* memory_base;	
	u32 phys_memory_base;
	bool shared_mem_requested;

	unsigned char* lcr_base;	
	u32 phys_lcr_base;
	u32 lcr_offset;
	bool lcr_mem_requested;

	u32 misc_ctrl_value;
	char flag_buf[MAX_ASYNC_BUFFER_SIZE];
	char char_buf[MAX_ASYNC_BUFFER_SIZE];	
	bool drop_rts_on_tx_done;

	bool loopmode_insert_requested;
	bool loopmode_send_done_requested;
	
	struct	_input_signal_events	input_signal_events;

	
	int netcount;
	spinlock_t netlock;

#if SYNCLINK_GENERIC_HDLC
	struct net_device *netdev;
#endif
};

#define MGSL_MAGIC 0x5401

#ifndef SERIAL_XMIT_SIZE
#define SERIAL_XMIT_SIZE 4096
#endif



#define DCPIN 2		
#define SDPIN 4		

#define DCAR 0		
#define CCAR SDPIN		
#define DATAREG DCPIN + SDPIN	
#define MSBONLY 0x41
#define LSBONLY 0x40


#define CMR	0x02	
#define CCSR	0x04	
#define CCR	0x06	
#define PSR	0x08	
#define PCR	0x0a	
#define TMDR	0x0c	
#define TMCR	0x0e	
#define CMCR	0x10	
#define HCR	0x12	
#define IVR	0x14	
#define IOCR	0x16	
#define ICR	0x18	
#define DCCR	0x1a	
#define MISR	0x1c	
#define SICR	0x1e	
#define RDR	0x20	
#define RMR	0x22	
#define RCSR	0x24	
#define RICR	0x26	
#define RSR	0x28	
#define RCLR	0x2a	
#define RCCR	0x2c	
#define TC0R	0x2e	
#define TDR	0x30	
#define TMR	0x32	
#define TCSR	0x34	
#define TICR	0x36	
#define TSR	0x38	
#define TCLR	0x3a	
#define TCCR	0x3c	
#define TC1R	0x3e	



#define DCR	0x06	
#define DACR	0x08	
#define BDCR	0x12	
#define DIVR	0x14		
#define DICR	0x18	
#define CDIR	0x1a	
#define SDIR	0x1c	

#define TDMR	0x02	
#define TDIAR	0x1e	
#define TBCR	0x2a	
#define TARL	0x2c	
#define TARU	0x2e	
#define NTBCR	0x3a	
#define NTARL	0x3c	
#define NTARU	0x3e	

#define RDMR	0x82	
#define RDIAR	0x9e	
#define RBCR	0xaa	
#define RARL	0xac	
#define RARU	0xae	
#define NRBCR	0xba	
#define NRARL	0xbc	
#define NRARU	0xbe	



#define MODEMSTATUS_DTR 0x80
#define MODEMSTATUS_DSR 0x40
#define MODEMSTATUS_RTS 0x20
#define MODEMSTATUS_CTS 0x10
#define MODEMSTATUS_RI  0x04
#define MODEMSTATUS_DCD 0x01



#define RTCmd_Null			0x0000
#define RTCmd_ResetHighestIus		0x1000
#define RTCmd_TriggerChannelLoadDma	0x2000
#define RTCmd_TriggerRxDma		0x2800
#define RTCmd_TriggerTxDma		0x3000
#define RTCmd_TriggerRxAndTxDma		0x3800
#define RTCmd_PurgeRxFifo		0x4800
#define RTCmd_PurgeTxFifo		0x5000
#define RTCmd_PurgeRxAndTxFifo		0x5800
#define RTCmd_LoadRcc			0x6800
#define RTCmd_LoadTcc			0x7000
#define RTCmd_LoadRccAndTcc		0x7800
#define RTCmd_LoadTC0			0x8800
#define RTCmd_LoadTC1			0x9000
#define RTCmd_LoadTC0AndTC1		0x9800
#define RTCmd_SerialDataLSBFirst	0xa000
#define RTCmd_SerialDataMSBFirst	0xa800
#define RTCmd_SelectBigEndian		0xb000
#define RTCmd_SelectLittleEndian	0xb800



#define DmaCmd_Null			0x0000
#define DmaCmd_ResetTxChannel		0x1000
#define DmaCmd_ResetRxChannel		0x1200
#define DmaCmd_StartTxChannel		0x2000
#define DmaCmd_StartRxChannel		0x2200
#define DmaCmd_ContinueTxChannel	0x3000
#define DmaCmd_ContinueRxChannel	0x3200
#define DmaCmd_PauseTxChannel		0x4000
#define DmaCmd_PauseRxChannel		0x4200
#define DmaCmd_AbortTxChannel		0x5000
#define DmaCmd_AbortRxChannel		0x5200
#define DmaCmd_InitTxChannel		0x7000
#define DmaCmd_InitRxChannel		0x7200
#define DmaCmd_ResetHighestDmaIus	0x8000
#define DmaCmd_ResetAllChannels		0x9000
#define DmaCmd_StartAllChannels		0xa000
#define DmaCmd_ContinueAllChannels	0xb000
#define DmaCmd_PauseAllChannels		0xc000
#define DmaCmd_AbortAllChannels		0xd000
#define DmaCmd_InitAllChannels		0xf000

#define TCmd_Null			0x0000
#define TCmd_ClearTxCRC			0x2000
#define TCmd_SelectTicrTtsaData		0x4000
#define TCmd_SelectTicrTxFifostatus	0x5000
#define TCmd_SelectTicrIntLevel		0x6000
#define TCmd_SelectTicrdma_level		0x7000
#define TCmd_SendFrame			0x8000
#define TCmd_SendAbort			0x9000
#define TCmd_EnableDleInsertion		0xc000
#define TCmd_DisableDleInsertion	0xd000
#define TCmd_ClearEofEom		0xe000
#define TCmd_SetEofEom			0xf000

#define RCmd_Null			0x0000
#define RCmd_ClearRxCRC			0x2000
#define RCmd_EnterHuntmode		0x3000
#define RCmd_SelectRicrRtsaData		0x4000
#define RCmd_SelectRicrRxFifostatus	0x5000
#define RCmd_SelectRicrIntLevel		0x6000
#define RCmd_SelectRicrdma_level		0x7000

 
#define RECEIVE_STATUS		BIT5
#define RECEIVE_DATA		BIT4
#define TRANSMIT_STATUS		BIT3
#define TRANSMIT_DATA		BIT2
#define IO_PIN			BIT1
#define MISC			BIT0



#define RXSTATUS_SHORT_FRAME		BIT8
#define RXSTATUS_CODE_VIOLATION		BIT8
#define RXSTATUS_EXITED_HUNT		BIT7
#define RXSTATUS_IDLE_RECEIVED		BIT6
#define RXSTATUS_BREAK_RECEIVED		BIT5
#define RXSTATUS_ABORT_RECEIVED		BIT5
#define RXSTATUS_RXBOUND		BIT4
#define RXSTATUS_CRC_ERROR		BIT3
#define RXSTATUS_FRAMING_ERROR		BIT3
#define RXSTATUS_ABORT			BIT2
#define RXSTATUS_PARITY_ERROR		BIT2
#define RXSTATUS_OVERRUN		BIT1
#define RXSTATUS_DATA_AVAILABLE		BIT0
#define RXSTATUS_ALL			0x01f6
#define usc_UnlatchRxstatusBits(a,b) usc_OutReg( (a), RCSR, (u16)((b) & RXSTATUS_ALL) )

#define IDLEMODE_FLAGS			0x0000
#define IDLEMODE_ALT_ONE_ZERO		0x0100
#define IDLEMODE_ZERO			0x0200
#define IDLEMODE_ONE			0x0300
#define IDLEMODE_ALT_MARK_SPACE		0x0500
#define IDLEMODE_SPACE			0x0600
#define IDLEMODE_MARK			0x0700
#define IDLEMODE_MASK			0x0700

#define	IUSC_SL1660			0x4d44
#define IUSC_PRE_SL1660			0x4553


#define TCSR_PRESERVE			0x0F00

#define TCSR_UNDERWAIT			BIT11
#define TXSTATUS_PREAMBLE_SENT		BIT7
#define TXSTATUS_IDLE_SENT		BIT6
#define TXSTATUS_ABORT_SENT		BIT5
#define TXSTATUS_EOF_SENT		BIT4
#define TXSTATUS_EOM_SENT		BIT4
#define TXSTATUS_CRC_SENT		BIT3
#define TXSTATUS_ALL_SENT		BIT2
#define TXSTATUS_UNDERRUN		BIT1
#define TXSTATUS_FIFO_EMPTY		BIT0
#define TXSTATUS_ALL			0x00fa
#define usc_UnlatchTxstatusBits(a,b) usc_OutReg( (a), TCSR, (u16)((a)->tcsr_value + ((b) & 0x00FF)) )
				

#define MISCSTATUS_RXC_LATCHED		BIT15
#define MISCSTATUS_RXC			BIT14
#define MISCSTATUS_TXC_LATCHED		BIT13
#define MISCSTATUS_TXC			BIT12
#define MISCSTATUS_RI_LATCHED		BIT11
#define MISCSTATUS_RI			BIT10
#define MISCSTATUS_DSR_LATCHED		BIT9
#define MISCSTATUS_DSR			BIT8
#define MISCSTATUS_DCD_LATCHED		BIT7
#define MISCSTATUS_DCD			BIT6
#define MISCSTATUS_CTS_LATCHED		BIT5
#define MISCSTATUS_CTS			BIT4
#define MISCSTATUS_RCC_UNDERRUN		BIT3
#define MISCSTATUS_DPLL_NO_SYNC		BIT2
#define MISCSTATUS_BRG1_ZERO		BIT1
#define MISCSTATUS_BRG0_ZERO		BIT0

#define usc_UnlatchIostatusBits(a,b) usc_OutReg((a),MISR,(u16)((b) & 0xaaa0))
#define usc_UnlatchMiscstatusBits(a,b) usc_OutReg((a),MISR,(u16)((b) & 0x000f))

#define SICR_RXC_ACTIVE			BIT15
#define SICR_RXC_INACTIVE		BIT14
#define SICR_RXC			(BIT15+BIT14)
#define SICR_TXC_ACTIVE			BIT13
#define SICR_TXC_INACTIVE		BIT12
#define SICR_TXC			(BIT13+BIT12)
#define SICR_RI_ACTIVE			BIT11
#define SICR_RI_INACTIVE		BIT10
#define SICR_RI				(BIT11+BIT10)
#define SICR_DSR_ACTIVE			BIT9
#define SICR_DSR_INACTIVE		BIT8
#define SICR_DSR			(BIT9+BIT8)
#define SICR_DCD_ACTIVE			BIT7
#define SICR_DCD_INACTIVE		BIT6
#define SICR_DCD			(BIT7+BIT6)
#define SICR_CTS_ACTIVE			BIT5
#define SICR_CTS_INACTIVE		BIT4
#define SICR_CTS			(BIT5+BIT4)
#define SICR_RCC_UNDERFLOW		BIT3
#define SICR_DPLL_NO_SYNC		BIT2
#define SICR_BRG1_ZERO			BIT1
#define SICR_BRG0_ZERO			BIT0

void usc_DisableMasterIrqBit( struct mgsl_struct *info );
void usc_EnableMasterIrqBit( struct mgsl_struct *info );
void usc_EnableInterrupts( struct mgsl_struct *info, u16 IrqMask );
void usc_DisableInterrupts( struct mgsl_struct *info, u16 IrqMask );
void usc_ClearIrqPendingBits( struct mgsl_struct *info, u16 IrqMask );

#define usc_EnableInterrupts( a, b ) \
	usc_OutReg( (a), ICR, (u16)((usc_InReg((a),ICR) & 0xff00) + 0xc0 + (b)) )

#define usc_DisableInterrupts( a, b ) \
	usc_OutReg( (a), ICR, (u16)((usc_InReg((a),ICR) & 0xff00) + 0x80 + (b)) )

#define usc_EnableMasterIrqBit(a) \
	usc_OutReg( (a), ICR, (u16)((usc_InReg((a),ICR) & 0x0f00) + 0xb000) )

#define usc_DisableMasterIrqBit(a) \
	usc_OutReg( (a), ICR, (u16)(usc_InReg((a),ICR) & 0x7f00) )

#define usc_ClearIrqPendingBits( a, b ) usc_OutReg( (a), DCCR, 0x40 + (b) )


#define TXSTATUS_PREAMBLE_SENT	BIT7
#define TXSTATUS_IDLE_SENT	BIT6
#define TXSTATUS_ABORT_SENT	BIT5
#define TXSTATUS_EOF		BIT4
#define TXSTATUS_CRC_SENT	BIT3
#define TXSTATUS_ALL_SENT	BIT2
#define TXSTATUS_UNDERRUN	BIT1
#define TXSTATUS_FIFO_EMPTY	BIT0

#define DICR_MASTER		BIT15
#define DICR_TRANSMIT		BIT0
#define DICR_RECEIVE		BIT1

#define usc_EnableDmaInterrupts(a,b) \
	usc_OutDmaReg( (a), DICR, (u16)(usc_InDmaReg((a),DICR) | (b)) )

#define usc_DisableDmaInterrupts(a,b) \
	usc_OutDmaReg( (a), DICR, (u16)(usc_InDmaReg((a),DICR) & ~(b)) )

#define usc_EnableStatusIrqs(a,b) \
	usc_OutReg( (a), SICR, (u16)(usc_InReg((a),SICR) | (b)) )

#define usc_DisablestatusIrqs(a,b) \
	usc_OutReg( (a), SICR, (u16)(usc_InReg((a),SICR) & ~(b)) )



#define DISABLE_UNCONDITIONAL    0
#define DISABLE_END_OF_FRAME     1
#define ENABLE_UNCONDITIONAL     2
#define ENABLE_AUTO_CTS          3
#define ENABLE_AUTO_DCD          3
#define usc_EnableTransmitter(a,b) \
	usc_OutReg( (a), TMR, (u16)((usc_InReg((a),TMR) & 0xfffc) | (b)) )
#define usc_EnableReceiver(a,b) \
	usc_OutReg( (a), RMR, (u16)((usc_InReg((a),RMR) & 0xfffc) | (b)) )

static u16  usc_InDmaReg( struct mgsl_struct *info, u16 Port );
static void usc_OutDmaReg( struct mgsl_struct *info, u16 Port, u16 Value );
static void usc_DmaCmd( struct mgsl_struct *info, u16 Cmd );

static u16  usc_InReg( struct mgsl_struct *info, u16 Port );
static void usc_OutReg( struct mgsl_struct *info, u16 Port, u16 Value );
static void usc_RTCmd( struct mgsl_struct *info, u16 Cmd );
void usc_RCmd( struct mgsl_struct *info, u16 Cmd );
void usc_TCmd( struct mgsl_struct *info, u16 Cmd );

#define usc_TCmd(a,b) usc_OutReg((a), TCSR, (u16)((a)->tcsr_value + (b)))
#define usc_RCmd(a,b) usc_OutReg((a), RCSR, (b))

#define usc_SetTransmitSyncChars(a,s0,s1) usc_OutReg((a), TSR, (u16)(((u16)s0<<8)|(u16)s1))

static void usc_process_rxoverrun_sync( struct mgsl_struct *info );
static void usc_start_receiver( struct mgsl_struct *info );
static void usc_stop_receiver( struct mgsl_struct *info );

static void usc_start_transmitter( struct mgsl_struct *info );
static void usc_stop_transmitter( struct mgsl_struct *info );
static void usc_set_txidle( struct mgsl_struct *info );
static void usc_load_txfifo( struct mgsl_struct *info );

static void usc_enable_aux_clock( struct mgsl_struct *info, u32 DataRate );
static void usc_enable_loopback( struct mgsl_struct *info, int enable );

static void usc_get_serial_signals( struct mgsl_struct *info );
static void usc_set_serial_signals( struct mgsl_struct *info );

static void usc_reset( struct mgsl_struct *info );

static void usc_set_sync_mode( struct mgsl_struct *info );
static void usc_set_sdlc_mode( struct mgsl_struct *info );
static void usc_set_async_mode( struct mgsl_struct *info );
static void usc_enable_async_clock( struct mgsl_struct *info, u32 DataRate );

static void usc_loopback_frame( struct mgsl_struct *info );

static void mgsl_tx_timeout(unsigned long context);


static void usc_loopmode_cancel_transmit( struct mgsl_struct * info );
static void usc_loopmode_insert_request( struct mgsl_struct * info );
static int usc_loopmode_active( struct mgsl_struct * info);
static void usc_loopmode_send_done( struct mgsl_struct * info );

static int mgsl_ioctl_common(struct mgsl_struct *info, unsigned int cmd, unsigned long arg);

#if SYNCLINK_GENERIC_HDLC
#define dev_to_port(D) (dev_to_hdlc(D)->priv)
static void hdlcdev_tx_done(struct mgsl_struct *info);
static void hdlcdev_rx(struct mgsl_struct *info, char *buf, int size);
static int  hdlcdev_init(struct mgsl_struct *info);
static void hdlcdev_exit(struct mgsl_struct *info);
#endif


#define BUS_DESCRIPTOR( WrHold, WrDly, RdDly, Nwdd, Nwad, Nxda, Nrdd, Nrad ) \
(0x00400020 + \
((WrHold) << 30) + \
((WrDly)  << 28) + \
((RdDly)  << 26) + \
((Nwdd)   << 20) + \
((Nwad)   << 15) + \
((Nxda)   << 13) + \
((Nrdd)   << 11) + \
((Nrad)   <<  6) )

static void mgsl_trace_block(struct mgsl_struct *info,const char* data, int count, int xmit);

static bool mgsl_register_test( struct mgsl_struct *info );
static bool mgsl_irq_test( struct mgsl_struct *info );
static bool mgsl_dma_test( struct mgsl_struct *info );
static bool mgsl_memory_test( struct mgsl_struct *info );
static int mgsl_adapter_test( struct mgsl_struct *info );

static int mgsl_claim_resources(struct mgsl_struct *info);
static void mgsl_release_resources(struct mgsl_struct *info);
static void mgsl_add_device(struct mgsl_struct *info);
static struct mgsl_struct* mgsl_allocate_device(void);

static void mgsl_free_rx_frame_buffers( struct mgsl_struct *info, unsigned int StartIndex, unsigned int EndIndex );
static bool mgsl_get_rx_frame( struct mgsl_struct *info );
static bool mgsl_get_raw_rx_frame( struct mgsl_struct *info );
static void mgsl_reset_rx_dma_buffers( struct mgsl_struct *info );
static void mgsl_reset_tx_dma_buffers( struct mgsl_struct *info );
static int num_free_tx_dma_buffers(struct mgsl_struct *info);
static void mgsl_load_tx_dma_buffer( struct mgsl_struct *info, const char *Buffer, unsigned int BufferSize);
static void mgsl_load_pci_memory(char* TargetPtr, const char* SourcePtr, unsigned short count);

static int  mgsl_allocate_dma_buffers(struct mgsl_struct *info);
static void mgsl_free_dma_buffers(struct mgsl_struct *info);
static int  mgsl_alloc_frame_memory(struct mgsl_struct *info, DMABUFFERENTRY *BufferList,int Buffercount);
static void mgsl_free_frame_memory(struct mgsl_struct *info, DMABUFFERENTRY *BufferList,int Buffercount);
static int  mgsl_alloc_buffer_list_memory(struct mgsl_struct *info);
static void mgsl_free_buffer_list_memory(struct mgsl_struct *info);
static int mgsl_alloc_intermediate_rxbuffer_memory(struct mgsl_struct *info);
static void mgsl_free_intermediate_rxbuffer_memory(struct mgsl_struct *info);
static int mgsl_alloc_intermediate_txbuffer_memory(struct mgsl_struct *info);
static void mgsl_free_intermediate_txbuffer_memory(struct mgsl_struct *info);
static bool load_next_tx_holding_buffer(struct mgsl_struct *info);
static int save_tx_buffer_request(struct mgsl_struct *info,const char *Buffer, unsigned int BufferSize);

static void mgsl_bh_handler(struct work_struct *work);
static void mgsl_bh_receive(struct mgsl_struct *info);
static void mgsl_bh_transmit(struct mgsl_struct *info);
static void mgsl_bh_status(struct mgsl_struct *info);

static void mgsl_isr_null( struct mgsl_struct *info );
static void mgsl_isr_transmit_data( struct mgsl_struct *info );
static void mgsl_isr_receive_data( struct mgsl_struct *info );
static void mgsl_isr_receive_status( struct mgsl_struct *info );
static void mgsl_isr_transmit_status( struct mgsl_struct *info );
static void mgsl_isr_io_pin( struct mgsl_struct *info );
static void mgsl_isr_misc( struct mgsl_struct *info );
static void mgsl_isr_receive_dma( struct mgsl_struct *info );
static void mgsl_isr_transmit_dma( struct mgsl_struct *info );

typedef void (*isr_dispatch_func)(struct mgsl_struct *);

static isr_dispatch_func UscIsrTable[7] =
{
	mgsl_isr_null,
	mgsl_isr_misc,
	mgsl_isr_io_pin,
	mgsl_isr_transmit_data,
	mgsl_isr_transmit_status,
	mgsl_isr_receive_data,
	mgsl_isr_receive_status
};

static int tiocmget(struct tty_struct *tty);
static int tiocmset(struct tty_struct *tty,
		    unsigned int set, unsigned int clear);
static int mgsl_get_stats(struct mgsl_struct * info, struct mgsl_icount
	__user *user_icount);
static int mgsl_get_params(struct mgsl_struct * info, MGSL_PARAMS  __user *user_params);
static int mgsl_set_params(struct mgsl_struct * info, MGSL_PARAMS  __user *new_params);
static int mgsl_get_txidle(struct mgsl_struct * info, int __user *idle_mode);
static int mgsl_set_txidle(struct mgsl_struct * info, int idle_mode);
static int mgsl_txenable(struct mgsl_struct * info, int enable);
static int mgsl_txabort(struct mgsl_struct * info);
static int mgsl_rxenable(struct mgsl_struct * info, int enable);
static int mgsl_wait_event(struct mgsl_struct * info, int __user *mask);
static int mgsl_loopmode_send_done( struct mgsl_struct * info );

static bool pci_registered;

static struct mgsl_struct *mgsl_device_list;
static int mgsl_device_count;

static bool break_on_load;

static int ttymajor;

static int io[MAX_ISA_DEVICES];
static int irq[MAX_ISA_DEVICES];
static int dma[MAX_ISA_DEVICES];
static int debug_level;
static int maxframe[MAX_TOTAL_DEVICES];
static int txdmabufs[MAX_TOTAL_DEVICES];
static int txholdbufs[MAX_TOTAL_DEVICES];
	
module_param(break_on_load, bool, 0);
module_param(ttymajor, int, 0);
module_param_array(io, int, NULL, 0);
module_param_array(irq, int, NULL, 0);
module_param_array(dma, int, NULL, 0);
module_param(debug_level, int, 0);
module_param_array(maxframe, int, NULL, 0);
module_param_array(txdmabufs, int, NULL, 0);
module_param_array(txholdbufs, int, NULL, 0);

static char *driver_name = "SyncLink serial driver";
static char *driver_version = "$Revision: 4.38 $";

static int synclink_init_one (struct pci_dev *dev,
				     const struct pci_device_id *ent);
static void synclink_remove_one (struct pci_dev *dev);

static struct pci_device_id synclink_pci_tbl[] = {
	{ PCI_VENDOR_ID_MICROGATE, PCI_DEVICE_ID_MICROGATE_USC, PCI_ANY_ID, PCI_ANY_ID, },
	{ PCI_VENDOR_ID_MICROGATE, 0x0210, PCI_ANY_ID, PCI_ANY_ID, },
	{ 0, }, 
};
MODULE_DEVICE_TABLE(pci, synclink_pci_tbl);

MODULE_LICENSE("GPL");

static struct pci_driver synclink_pci_driver = {
	.name		= "synclink",
	.id_table	= synclink_pci_tbl,
	.probe		= synclink_init_one,
	.remove		= __devexit_p(synclink_remove_one),
};

static struct tty_driver *serial_driver;

#define WAKEUP_CHARS 256


static void mgsl_change_params(struct mgsl_struct *info);
static void mgsl_wait_until_sent(struct tty_struct *tty, int timeout);

static void* mgsl_get_text_ptr(void)
{
	return mgsl_get_text_ptr;
}

static inline int mgsl_paranoia_check(struct mgsl_struct *info,
					char *name, const char *routine)
{
#ifdef MGSL_PARANOIA_CHECK
	static const char *badmagic =
		"Warning: bad magic number for mgsl struct (%s) in %s\n";
	static const char *badinfo =
		"Warning: null mgsl_struct for (%s) in %s\n";

	if (!info) {
		printk(badinfo, name, routine);
		return 1;
	}
	if (info->magic != MGSL_MAGIC) {
		printk(badmagic, name, routine);
		return 1;
	}
#else
	if (!info)
		return 1;
#endif
	return 0;
}


static void ldisc_receive_buf(struct tty_struct *tty,
			      const __u8 *data, char *flags, int count)
{
	struct tty_ldisc *ld;
	if (!tty)
		return;
	ld = tty_ldisc_ref(tty);
	if (ld) {
		if (ld->ops->receive_buf)
			ld->ops->receive_buf(tty, data, flags, count);
		tty_ldisc_deref(ld);
	}
}

static void mgsl_stop(struct tty_struct *tty)
{
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;
	
	if (mgsl_paranoia_check(info, tty->name, "mgsl_stop"))
		return;
	
	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk("mgsl_stop(%s)\n",info->device_name);	
		
	spin_lock_irqsave(&info->irq_spinlock,flags);
	if (info->tx_enabled)
	 	usc_stop_transmitter(info);
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	
}	

static void mgsl_start(struct tty_struct *tty)
{
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;
	
	if (mgsl_paranoia_check(info, tty->name, "mgsl_start"))
		return;
	
	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk("mgsl_start(%s)\n",info->device_name);	
		
	spin_lock_irqsave(&info->irq_spinlock,flags);
	if (!info->tx_enabled)
	 	usc_start_transmitter(info);
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	
}	


static int mgsl_bh_action(struct mgsl_struct *info)
{
	unsigned long flags;
	int rc = 0;
	
	spin_lock_irqsave(&info->irq_spinlock,flags);

	if (info->pending_bh & BH_RECEIVE) {
		info->pending_bh &= ~BH_RECEIVE;
		rc = BH_RECEIVE;
	} else if (info->pending_bh & BH_TRANSMIT) {
		info->pending_bh &= ~BH_TRANSMIT;
		rc = BH_TRANSMIT;
	} else if (info->pending_bh & BH_STATUS) {
		info->pending_bh &= ~BH_STATUS;
		rc = BH_STATUS;
	}

	if (!rc) {
		
		info->bh_running = false;
		info->bh_requested = false;
	}
	
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	
	return rc;
}

static void mgsl_bh_handler(struct work_struct *work)
{
	struct mgsl_struct *info =
		container_of(work, struct mgsl_struct, task);
	int action;

	if (!info)
		return;
		
	if ( debug_level >= DEBUG_LEVEL_BH )
		printk( "%s(%d):mgsl_bh_handler(%s) entry\n",
			__FILE__,__LINE__,info->device_name);
	
	info->bh_running = true;

	while((action = mgsl_bh_action(info)) != 0) {
	
		
		if ( debug_level >= DEBUG_LEVEL_BH )
			printk( "%s(%d):mgsl_bh_handler() work item action=%d\n",
				__FILE__,__LINE__,action);

		switch (action) {
		
		case BH_RECEIVE:
			mgsl_bh_receive(info);
			break;
		case BH_TRANSMIT:
			mgsl_bh_transmit(info);
			break;
		case BH_STATUS:
			mgsl_bh_status(info);
			break;
		default:
			
			printk("Unknown work item ID=%08X!\n", action);
			break;
		}
	}

	if ( debug_level >= DEBUG_LEVEL_BH )
		printk( "%s(%d):mgsl_bh_handler(%s) exit\n",
			__FILE__,__LINE__,info->device_name);
}

static void mgsl_bh_receive(struct mgsl_struct *info)
{
	bool (*get_rx_frame)(struct mgsl_struct *info) =
		(info->params.mode == MGSL_MODE_HDLC ? mgsl_get_rx_frame : mgsl_get_raw_rx_frame);

	if ( debug_level >= DEBUG_LEVEL_BH )
		printk( "%s(%d):mgsl_bh_receive(%s)\n",
			__FILE__,__LINE__,info->device_name);
	
	do
	{
		if (info->rx_rcc_underrun) {
			unsigned long flags;
			spin_lock_irqsave(&info->irq_spinlock,flags);
			usc_start_receiver(info);
			spin_unlock_irqrestore(&info->irq_spinlock,flags);
			return;
		}
	} while(get_rx_frame(info));
}

static void mgsl_bh_transmit(struct mgsl_struct *info)
{
	struct tty_struct *tty = info->port.tty;
	unsigned long flags;
	
	if ( debug_level >= DEBUG_LEVEL_BH )
		printk( "%s(%d):mgsl_bh_transmit() entry on %s\n",
			__FILE__,__LINE__,info->device_name);

	if (tty)
		tty_wakeup(tty);

	spin_lock_irqsave(&info->irq_spinlock,flags);
 	if ( !info->tx_active && info->loopmode_send_done_requested )
 		usc_loopmode_send_done( info );
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
}

static void mgsl_bh_status(struct mgsl_struct *info)
{
	if ( debug_level >= DEBUG_LEVEL_BH )
		printk( "%s(%d):mgsl_bh_status() entry on %s\n",
			__FILE__,__LINE__,info->device_name);

	info->ri_chkcount = 0;
	info->dsr_chkcount = 0;
	info->dcd_chkcount = 0;
	info->cts_chkcount = 0;
}

static void mgsl_isr_receive_status( struct mgsl_struct *info )
{
	u16 status = usc_InReg( info, RCSR );

	if ( debug_level >= DEBUG_LEVEL_ISR )	
		printk("%s(%d):mgsl_isr_receive_status status=%04X\n",
			__FILE__,__LINE__,status);
			
 	if ( (status & RXSTATUS_ABORT_RECEIVED) && 
		info->loopmode_insert_requested &&
 		usc_loopmode_active(info) )
 	{
		++info->icount.rxabort;
	 	info->loopmode_insert_requested = false;
 
 		
		info->cmr_value &= ~BIT13;
 		usc_OutReg(info, CMR, info->cmr_value);
 
		
	 	usc_OutReg(info, RICR,
 			(usc_InReg(info, RICR) & ~RXSTATUS_ABORT_RECEIVED));
 	}

	if (status & (RXSTATUS_EXITED_HUNT + RXSTATUS_IDLE_RECEIVED)) {
		if (status & RXSTATUS_EXITED_HUNT)
			info->icount.exithunt++;
		if (status & RXSTATUS_IDLE_RECEIVED)
			info->icount.rxidle++;
		wake_up_interruptible(&info->event_wait_q);
	}

	if (status & RXSTATUS_OVERRUN){
		info->icount.rxover++;
		usc_process_rxoverrun_sync( info );
	}

	usc_ClearIrqPendingBits( info, RECEIVE_STATUS );
	usc_UnlatchRxstatusBits( info, status );

}	

static void mgsl_isr_transmit_status( struct mgsl_struct *info )
{
	u16 status = usc_InReg( info, TCSR );

	if ( debug_level >= DEBUG_LEVEL_ISR )	
		printk("%s(%d):mgsl_isr_transmit_status status=%04X\n",
			__FILE__,__LINE__,status);
	
	usc_ClearIrqPendingBits( info, TRANSMIT_STATUS );
	usc_UnlatchTxstatusBits( info, status );
	
	if ( status & (TXSTATUS_UNDERRUN | TXSTATUS_ABORT_SENT) )
	{
		
		
		
		
		
 		usc_DmaCmd( info, DmaCmd_ResetTxChannel );
 		usc_RTCmd( info, RTCmd_PurgeTxFifo );
	}
 
	if ( status & TXSTATUS_EOF_SENT )
		info->icount.txok++;
	else if ( status & TXSTATUS_UNDERRUN )
		info->icount.txunder++;
	else if ( status & TXSTATUS_ABORT_SENT )
		info->icount.txabort++;
	else
		info->icount.txunder++;
			
	info->tx_active = false;
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
	del_timer(&info->tx_timer);	
	
	if ( info->drop_rts_on_tx_done ) {
		usc_get_serial_signals( info );
		if ( info->serial_signals & SerialSignal_RTS ) {
			info->serial_signals &= ~SerialSignal_RTS;
			usc_set_serial_signals( info );
		}
		info->drop_rts_on_tx_done = false;
	}

#if SYNCLINK_GENERIC_HDLC
	if (info->netcount)
		hdlcdev_tx_done(info);
	else 
#endif
	{
		if (info->port.tty->stopped || info->port.tty->hw_stopped) {
			usc_stop_transmitter(info);
			return;
		}
		info->pending_bh |= BH_TRANSMIT;
	}

}	

static void mgsl_isr_io_pin( struct mgsl_struct *info )
{
 	struct	mgsl_icount *icount;
	u16 status = usc_InReg( info, MISR );

	if ( debug_level >= DEBUG_LEVEL_ISR )	
		printk("%s(%d):mgsl_isr_io_pin status=%04X\n",
			__FILE__,__LINE__,status);
			
	usc_ClearIrqPendingBits( info, IO_PIN );
	usc_UnlatchIostatusBits( info, status );

	if (status & (MISCSTATUS_CTS_LATCHED | MISCSTATUS_DCD_LATCHED |
	              MISCSTATUS_DSR_LATCHED | MISCSTATUS_RI_LATCHED) ) {
		icount = &info->icount;
		
		if (status & MISCSTATUS_RI_LATCHED) {
			if ((info->ri_chkcount)++ >= IO_PIN_SHUTDOWN_LIMIT)
				usc_DisablestatusIrqs(info,SICR_RI);
			icount->rng++;
			if ( status & MISCSTATUS_RI )
				info->input_signal_events.ri_up++;	
			else
				info->input_signal_events.ri_down++;	
		}
		if (status & MISCSTATUS_DSR_LATCHED) {
			if ((info->dsr_chkcount)++ >= IO_PIN_SHUTDOWN_LIMIT)
				usc_DisablestatusIrqs(info,SICR_DSR);
			icount->dsr++;
			if ( status & MISCSTATUS_DSR )
				info->input_signal_events.dsr_up++;
			else
				info->input_signal_events.dsr_down++;
		}
		if (status & MISCSTATUS_DCD_LATCHED) {
			if ((info->dcd_chkcount)++ >= IO_PIN_SHUTDOWN_LIMIT)
				usc_DisablestatusIrqs(info,SICR_DCD);
			icount->dcd++;
			if (status & MISCSTATUS_DCD) {
				info->input_signal_events.dcd_up++;
			} else
				info->input_signal_events.dcd_down++;
#if SYNCLINK_GENERIC_HDLC
			if (info->netcount) {
				if (status & MISCSTATUS_DCD)
					netif_carrier_on(info->netdev);
				else
					netif_carrier_off(info->netdev);
			}
#endif
		}
		if (status & MISCSTATUS_CTS_LATCHED)
		{
			if ((info->cts_chkcount)++ >= IO_PIN_SHUTDOWN_LIMIT)
				usc_DisablestatusIrqs(info,SICR_CTS);
			icount->cts++;
			if ( status & MISCSTATUS_CTS )
				info->input_signal_events.cts_up++;
			else
				info->input_signal_events.cts_down++;
		}
		wake_up_interruptible(&info->status_event_wait_q);
		wake_up_interruptible(&info->event_wait_q);

		if ( (info->port.flags & ASYNC_CHECK_CD) && 
		     (status & MISCSTATUS_DCD_LATCHED) ) {
			if ( debug_level >= DEBUG_LEVEL_ISR )
				printk("%s CD now %s...", info->device_name,
				       (status & MISCSTATUS_DCD) ? "on" : "off");
			if (status & MISCSTATUS_DCD)
				wake_up_interruptible(&info->port.open_wait);
			else {
				if ( debug_level >= DEBUG_LEVEL_ISR )
					printk("doing serial hangup...");
				if (info->port.tty)
					tty_hangup(info->port.tty);
			}
		}
	
		if ( (info->port.flags & ASYNC_CTS_FLOW) && 
		     (status & MISCSTATUS_CTS_LATCHED) ) {
			if (info->port.tty->hw_stopped) {
				if (status & MISCSTATUS_CTS) {
					if ( debug_level >= DEBUG_LEVEL_ISR )
						printk("CTS tx start...");
					if (info->port.tty)
						info->port.tty->hw_stopped = 0;
					usc_start_transmitter(info);
					info->pending_bh |= BH_TRANSMIT;
					return;
				}
			} else {
				if (!(status & MISCSTATUS_CTS)) {
					if ( debug_level >= DEBUG_LEVEL_ISR )
						printk("CTS tx stop...");
					if (info->port.tty)
						info->port.tty->hw_stopped = 1;
					usc_stop_transmitter(info);
				}
			}
		}
	}

	info->pending_bh |= BH_STATUS;
	
	
	if ( status & MISCSTATUS_TXC_LATCHED ){
		usc_OutReg( info, SICR,
			(unsigned short)(usc_InReg(info,SICR) & ~(SICR_TXC_ACTIVE+SICR_TXC_INACTIVE)) );
		usc_UnlatchIostatusBits( info, MISCSTATUS_TXC_LATCHED );
		info->irq_occurred = true;
	}

}	

static void mgsl_isr_transmit_data( struct mgsl_struct *info )
{
	if ( debug_level >= DEBUG_LEVEL_ISR )	
		printk("%s(%d):mgsl_isr_transmit_data xmit_cnt=%d\n",
			__FILE__,__LINE__,info->xmit_cnt);
			
	usc_ClearIrqPendingBits( info, TRANSMIT_DATA );
	
	if (info->port.tty->stopped || info->port.tty->hw_stopped) {
		usc_stop_transmitter(info);
		return;
	}
	
	if ( info->xmit_cnt )
		usc_load_txfifo( info );
	else
		info->tx_active = false;
		
	if (info->xmit_cnt < WAKEUP_CHARS)
		info->pending_bh |= BH_TRANSMIT;

}	

static void mgsl_isr_receive_data( struct mgsl_struct *info )
{
	int Fifocount;
	u16 status;
	int work = 0;
	unsigned char DataByte;
 	struct tty_struct *tty = info->port.tty;
 	struct	mgsl_icount *icount = &info->icount;
	
	if ( debug_level >= DEBUG_LEVEL_ISR )	
		printk("%s(%d):mgsl_isr_receive_data\n",
			__FILE__,__LINE__);

	usc_ClearIrqPendingBits( info, RECEIVE_DATA );
	
	
	usc_RCmd( info, RCmd_SelectRicrRxFifostatus );

	
	
	usc_OutReg( info, RICR+LSBONLY, (u16)(usc_InReg(info, RICR+LSBONLY) & ~BIT3 ));

	

	while( (Fifocount = (usc_InReg(info,RICR) >> 8)) ) {
		int flag;

		
		outw( (inw(info->io_base + CCAR) & 0x0780) | (RDR+LSBONLY),
		      info->io_base + CCAR );
		DataByte = inb( info->io_base + CCAR );

		
		status = usc_InReg(info, RCSR);
		if ( status & (RXSTATUS_FRAMING_ERROR + RXSTATUS_PARITY_ERROR +
				RXSTATUS_OVERRUN + RXSTATUS_BREAK_RECEIVED) )
			usc_UnlatchRxstatusBits(info,RXSTATUS_ALL);
		
		icount->rx++;
		
		flag = 0;
		if ( status & (RXSTATUS_FRAMING_ERROR + RXSTATUS_PARITY_ERROR +
				RXSTATUS_OVERRUN + RXSTATUS_BREAK_RECEIVED) ) {
			printk("rxerr=%04X\n",status);					
			
			if ( status & RXSTATUS_BREAK_RECEIVED ) {
				status &= ~(RXSTATUS_FRAMING_ERROR + RXSTATUS_PARITY_ERROR);
				icount->brk++;
			} else if (status & RXSTATUS_PARITY_ERROR) 
				icount->parity++;
			else if (status & RXSTATUS_FRAMING_ERROR)
				icount->frame++;
			else if (status & RXSTATUS_OVERRUN) {
				
				
				usc_RTCmd(info,RTCmd_PurgeRxFifo);
				icount->overrun++;
			}

								
			if (status & info->ignore_status_mask)
				continue;
				
			status &= info->read_status_mask;
		
			if (status & RXSTATUS_BREAK_RECEIVED) {
				flag = TTY_BREAK;
				if (info->port.flags & ASYNC_SAK)
					do_SAK(tty);
			} else if (status & RXSTATUS_PARITY_ERROR)
				flag = TTY_PARITY;
			else if (status & RXSTATUS_FRAMING_ERROR)
				flag = TTY_FRAME;
		}	
		tty_insert_flip_char(tty, DataByte, flag);
		if (status & RXSTATUS_OVERRUN) {
			work += tty_insert_flip_char(tty, 0, TTY_OVERRUN);
		}
	}

	if ( debug_level >= DEBUG_LEVEL_ISR ) {
		printk("%s(%d):rx=%d brk=%d parity=%d frame=%d overrun=%d\n",
			__FILE__,__LINE__,icount->rx,icount->brk,
			icount->parity,icount->frame,icount->overrun);
	}
			
	if(work)
		tty_flip_buffer_push(tty);
}

static void mgsl_isr_misc( struct mgsl_struct *info )
{
	u16 status = usc_InReg( info, MISR );

	if ( debug_level >= DEBUG_LEVEL_ISR )	
		printk("%s(%d):mgsl_isr_misc status=%04X\n",
			__FILE__,__LINE__,status);
			
	if ((status & MISCSTATUS_RCC_UNDERRUN) &&
	    (info->params.mode == MGSL_MODE_HDLC)) {

		
		usc_EnableReceiver(info,DISABLE_UNCONDITIONAL);
		usc_DmaCmd(info, DmaCmd_ResetRxChannel);
		usc_UnlatchRxstatusBits(info, RXSTATUS_ALL);
		usc_ClearIrqPendingBits(info, RECEIVE_DATA + RECEIVE_STATUS);
		usc_DisableInterrupts(info, RECEIVE_DATA + RECEIVE_STATUS);

		
		info->pending_bh |= BH_RECEIVE;
		info->rx_rcc_underrun = true;
	}

	usc_ClearIrqPendingBits( info, MISC );
	usc_UnlatchMiscstatusBits( info, status );

}	

static void mgsl_isr_null( struct mgsl_struct *info )
{

}	

static void mgsl_isr_receive_dma( struct mgsl_struct *info )
{
	u16 status;
	
	
	usc_OutDmaReg( info, CDIR, BIT9+BIT1 );

	
	
	status = usc_InDmaReg( info, RDMR );

	if ( debug_level >= DEBUG_LEVEL_ISR )	
		printk("%s(%d):mgsl_isr_receive_dma(%s) status=%04X\n",
			__FILE__,__LINE__,info->device_name,status);
			
	info->pending_bh |= BH_RECEIVE;
	
	if ( status & BIT3 ) {
		info->rx_overflow = true;
		info->icount.buf_overrun++;
	}

}	

static void mgsl_isr_transmit_dma( struct mgsl_struct *info )
{
	u16 status;

	
	usc_OutDmaReg(info, CDIR, BIT8+BIT0 );

	
	

	status = usc_InDmaReg( info, TDMR );

	if ( debug_level >= DEBUG_LEVEL_ISR )
		printk("%s(%d):mgsl_isr_transmit_dma(%s) status=%04X\n",
			__FILE__,__LINE__,info->device_name,status);

	if ( status & BIT2 ) {
		--info->tx_dma_buffers_used;

		if ( load_next_tx_holding_buffer(info) ) {
			info->pending_bh |= BH_TRANSMIT;
		}
	}

}	

static irqreturn_t mgsl_interrupt(int dummy, void *dev_id)
{
	struct mgsl_struct *info = dev_id;
	u16 UscVector;
	u16 DmaVector;

	if ( debug_level >= DEBUG_LEVEL_ISR )	
		printk(KERN_DEBUG "%s(%d):mgsl_interrupt(%d)entry.\n",
			__FILE__, __LINE__, info->irq_level);

	spin_lock(&info->irq_spinlock);

	for(;;) {
		
		UscVector = usc_InReg(info, IVR) >> 9;
		DmaVector = usc_InDmaReg(info, DIVR);
		
		if ( debug_level >= DEBUG_LEVEL_ISR )	
			printk("%s(%d):%s UscVector=%08X DmaVector=%08X\n",
				__FILE__,__LINE__,info->device_name,UscVector,DmaVector);
			
		if ( !UscVector && !DmaVector )
			break;
			
		
		if ( UscVector )
			(*UscIsrTable[UscVector])(info);
		else if ( (DmaVector&(BIT10|BIT9)) == BIT10)
			mgsl_isr_transmit_dma(info);
		else
			mgsl_isr_receive_dma(info);

		if ( info->isr_overflow ) {
			printk(KERN_ERR "%s(%d):%s isr overflow irq=%d\n",
				__FILE__, __LINE__, info->device_name, info->irq_level);
			usc_DisableMasterIrqBit(info);
			usc_DisableDmaInterrupts(info,DICR_MASTER);
			break;
		}
	}
	

	if ( info->pending_bh && !info->bh_running && !info->bh_requested ) {
		if ( debug_level >= DEBUG_LEVEL_ISR )	
			printk("%s(%d):%s queueing bh task.\n",
				__FILE__,__LINE__,info->device_name);
		schedule_work(&info->task);
		info->bh_requested = true;
	}

	spin_unlock(&info->irq_spinlock);
	
	if ( debug_level >= DEBUG_LEVEL_ISR )	
		printk(KERN_DEBUG "%s(%d):mgsl_interrupt(%d)exit.\n",
			__FILE__, __LINE__, info->irq_level);

	return IRQ_HANDLED;
}	

static int startup(struct mgsl_struct * info)
{
	int retval = 0;
	
	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk("%s(%d):mgsl_startup(%s)\n",__FILE__,__LINE__,info->device_name);
		
	if (info->port.flags & ASYNC_INITIALIZED)
		return 0;
	
	if (!info->xmit_buf) {
		
		info->xmit_buf = (unsigned char *)get_zeroed_page(GFP_KERNEL);
		if (!info->xmit_buf) {
			printk(KERN_ERR"%s(%d):%s can't allocate transmit buffer\n",
				__FILE__,__LINE__,info->device_name);
			return -ENOMEM;
		}
	}

	info->pending_bh = 0;
	
	memset(&info->icount, 0, sizeof(info->icount));

	setup_timer(&info->tx_timer, mgsl_tx_timeout, (unsigned long)info);
	
	
	retval = mgsl_claim_resources(info);
	
	
	if ( !retval )
		retval = mgsl_adapter_test(info);
		
	if ( retval ) {
  		if (capable(CAP_SYS_ADMIN) && info->port.tty)
			set_bit(TTY_IO_ERROR, &info->port.tty->flags);
		mgsl_release_resources(info);
  		return retval;
  	}

	
	mgsl_change_params(info);
	
	if (info->port.tty)
		clear_bit(TTY_IO_ERROR, &info->port.tty->flags);

	info->port.flags |= ASYNC_INITIALIZED;
	
	return 0;
	
}	

static void shutdown(struct mgsl_struct * info)
{
	unsigned long flags;
	
	if (!(info->port.flags & ASYNC_INITIALIZED))
		return;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_shutdown(%s)\n",
			 __FILE__,__LINE__, info->device_name );

	
	
	wake_up_interruptible(&info->status_event_wait_q);
	wake_up_interruptible(&info->event_wait_q);

	del_timer_sync(&info->tx_timer);

	if (info->xmit_buf) {
		free_page((unsigned long) info->xmit_buf);
		info->xmit_buf = NULL;
	}

	spin_lock_irqsave(&info->irq_spinlock,flags);
	usc_DisableMasterIrqBit(info);
	usc_stop_receiver(info);
	usc_stop_transmitter(info);
	usc_DisableInterrupts(info,RECEIVE_DATA + RECEIVE_STATUS +
		TRANSMIT_DATA + TRANSMIT_STATUS + IO_PIN + MISC );
	usc_DisableDmaInterrupts(info,DICR_MASTER + DICR_TRANSMIT + DICR_RECEIVE);
	
	
	
	
	usc_OutReg(info, PCR, (u16)((usc_InReg(info, PCR) | BIT15) | BIT14));
	
	
	
	
	usc_OutReg(info, PCR, (u16)((usc_InReg(info, PCR) | BIT13) | BIT12));
	
 	if (!info->port.tty || info->port.tty->termios->c_cflag & HUPCL) {
 		info->serial_signals &= ~(SerialSignal_DTR + SerialSignal_RTS);
		usc_set_serial_signals(info);
	}
	
	spin_unlock_irqrestore(&info->irq_spinlock,flags);

	mgsl_release_resources(info);	
	
	if (info->port.tty)
		set_bit(TTY_IO_ERROR, &info->port.tty->flags);

	info->port.flags &= ~ASYNC_INITIALIZED;
	
}	

static void mgsl_program_hw(struct mgsl_struct *info)
{
	unsigned long flags;

	spin_lock_irqsave(&info->irq_spinlock,flags);
	
	usc_stop_receiver(info);
	usc_stop_transmitter(info);
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
	
	if (info->params.mode == MGSL_MODE_HDLC ||
	    info->params.mode == MGSL_MODE_RAW ||
	    info->netcount)
		usc_set_sync_mode(info);
	else
		usc_set_async_mode(info);
		
	usc_set_serial_signals(info);
	
	info->dcd_chkcount = 0;
	info->cts_chkcount = 0;
	info->ri_chkcount = 0;
	info->dsr_chkcount = 0;

	usc_EnableStatusIrqs(info,SICR_CTS+SICR_DSR+SICR_DCD+SICR_RI);		
	usc_EnableInterrupts(info, IO_PIN);
	usc_get_serial_signals(info);
		
	if (info->netcount || info->port.tty->termios->c_cflag & CREAD)
		usc_start_receiver(info);
		
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
}

static void mgsl_change_params(struct mgsl_struct *info)
{
	unsigned cflag;
	int bits_per_char;

	if (!info->port.tty || !info->port.tty->termios)
		return;
		
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_change_params(%s)\n",
			 __FILE__,__LINE__, info->device_name );
			 
	cflag = info->port.tty->termios->c_cflag;

	
	
 	if (cflag & CBAUD)
		info->serial_signals |= SerialSignal_RTS + SerialSignal_DTR;
	else
		info->serial_signals &= ~(SerialSignal_RTS + SerialSignal_DTR);
	
	
	
	switch (cflag & CSIZE) {
	      case CS5: info->params.data_bits = 5; break;
	      case CS6: info->params.data_bits = 6; break;
	      case CS7: info->params.data_bits = 7; break;
	      case CS8: info->params.data_bits = 8; break;
	      
	      default:  info->params.data_bits = 7; break;
	      }
	      
	if (cflag & CSTOPB)
		info->params.stop_bits = 2;
	else
		info->params.stop_bits = 1;

	info->params.parity = ASYNC_PARITY_NONE;
	if (cflag & PARENB) {
		if (cflag & PARODD)
			info->params.parity = ASYNC_PARITY_ODD;
		else
			info->params.parity = ASYNC_PARITY_EVEN;
#ifdef CMSPAR
		if (cflag & CMSPAR)
			info->params.parity = ASYNC_PARITY_SPACE;
#endif
	}

	bits_per_char = info->params.data_bits + 
			info->params.stop_bits + 1;

	if (info->params.data_rate <= 460800)
		info->params.data_rate = tty_get_baud_rate(info->port.tty);
	
	if ( info->params.data_rate ) {
		info->timeout = (32*HZ*bits_per_char) / 
				info->params.data_rate;
	}
	info->timeout += HZ/50;		

	if (cflag & CRTSCTS)
		info->port.flags |= ASYNC_CTS_FLOW;
	else
		info->port.flags &= ~ASYNC_CTS_FLOW;
		
	if (cflag & CLOCAL)
		info->port.flags &= ~ASYNC_CHECK_CD;
	else
		info->port.flags |= ASYNC_CHECK_CD;

	
	
	info->read_status_mask = RXSTATUS_OVERRUN;
	if (I_INPCK(info->port.tty))
		info->read_status_mask |= RXSTATUS_PARITY_ERROR | RXSTATUS_FRAMING_ERROR;
 	if (I_BRKINT(info->port.tty) || I_PARMRK(info->port.tty))
 		info->read_status_mask |= RXSTATUS_BREAK_RECEIVED;
	
	if (I_IGNPAR(info->port.tty))
		info->ignore_status_mask |= RXSTATUS_PARITY_ERROR | RXSTATUS_FRAMING_ERROR;
	if (I_IGNBRK(info->port.tty)) {
		info->ignore_status_mask |= RXSTATUS_BREAK_RECEIVED;
		if (I_IGNPAR(info->port.tty))
			info->ignore_status_mask |= RXSTATUS_OVERRUN;
	}

	mgsl_program_hw(info);

}	

static int mgsl_put_char(struct tty_struct *tty, unsigned char ch)
{
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;
	int ret = 0;

	if (debug_level >= DEBUG_LEVEL_INFO) {
		printk(KERN_DEBUG "%s(%d):mgsl_put_char(%d) on %s\n",
			__FILE__, __LINE__, ch, info->device_name);
	}		
	
	if (mgsl_paranoia_check(info, tty->name, "mgsl_put_char"))
		return 0;

	if (!info->xmit_buf)
		return 0;

	spin_lock_irqsave(&info->irq_spinlock, flags);
	
	if ((info->params.mode == MGSL_MODE_ASYNC ) || !info->tx_active) {
		if (info->xmit_cnt < SERIAL_XMIT_SIZE - 1) {
			info->xmit_buf[info->xmit_head++] = ch;
			info->xmit_head &= SERIAL_XMIT_SIZE-1;
			info->xmit_cnt++;
			ret = 1;
		}
	}
	spin_unlock_irqrestore(&info->irq_spinlock, flags);
	return ret;
	
}	

static void mgsl_flush_chars(struct tty_struct *tty)
{
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;
				
	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk( "%s(%d):mgsl_flush_chars() entry on %s xmit_cnt=%d\n",
			__FILE__,__LINE__,info->device_name,info->xmit_cnt);
	
	if (mgsl_paranoia_check(info, tty->name, "mgsl_flush_chars"))
		return;

	if (info->xmit_cnt <= 0 || tty->stopped || tty->hw_stopped ||
	    !info->xmit_buf)
		return;

	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk( "%s(%d):mgsl_flush_chars() entry on %s starting transmitter\n",
			__FILE__,__LINE__,info->device_name );

	spin_lock_irqsave(&info->irq_spinlock,flags);
	
	if (!info->tx_active) {
		if ( (info->params.mode == MGSL_MODE_HDLC ||
			info->params.mode == MGSL_MODE_RAW) && info->xmit_cnt ) {
			
			
			
			mgsl_load_tx_dma_buffer(info,
				 info->xmit_buf,info->xmit_cnt);
		}
	 	usc_start_transmitter(info);
	}
	
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	
}	

/* mgsl_write()
 * 
 * 	Send a block of data
 * 	
 * Arguments:
 * 
 * 	tty		pointer to tty information structure
 * 	buf		pointer to buffer containing send data
 * 	count		size of send data in bytes
 * 	
 * Return Value:	number of characters written
 */
static int mgsl_write(struct tty_struct * tty,
		    const unsigned char *buf, int count)
{
	int	c, ret = 0;
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;
	
	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk( "%s(%d):mgsl_write(%s) count=%d\n",
			__FILE__,__LINE__,info->device_name,count);
	
	if (mgsl_paranoia_check(info, tty->name, "mgsl_write"))
		goto cleanup;

	if (!info->xmit_buf)
		goto cleanup;

	if ( info->params.mode == MGSL_MODE_HDLC ||
			info->params.mode == MGSL_MODE_RAW ) {
		
		if (info->tx_active) {

			if ( info->params.mode == MGSL_MODE_HDLC ) {
				ret = 0;
				goto cleanup;
			}
			if (info->tx_holding_count >= info->num_tx_holding_buffers ) {
				
				ret = 0;
				goto cleanup;
			}

			
			ret = count;
			save_tx_buffer_request(info,buf,count);

			spin_lock_irqsave(&info->irq_spinlock,flags);
			load_next_tx_holding_buffer(info);
			spin_unlock_irqrestore(&info->irq_spinlock,flags);
			goto cleanup;
		}
	
		
		
		

		if ( (info->params.flags & HDLC_FLAG_HDLC_LOOPMODE) &&
			!usc_loopmode_active(info) )
		{
			ret = 0;
			goto cleanup;
		}

		if ( info->xmit_cnt ) {
			
			
			ret = 0;
			
			
			
			mgsl_load_tx_dma_buffer(info,
				info->xmit_buf,info->xmit_cnt);
			if ( debug_level >= DEBUG_LEVEL_INFO )
				printk( "%s(%d):mgsl_write(%s) sync xmit_cnt flushing\n",
					__FILE__,__LINE__,info->device_name);
		} else {
			if ( debug_level >= DEBUG_LEVEL_INFO )
				printk( "%s(%d):mgsl_write(%s) sync transmit accepted\n",
					__FILE__,__LINE__,info->device_name);
			ret = count;
			info->xmit_cnt = count;
			mgsl_load_tx_dma_buffer(info,buf,count);
		}
	} else {
		while (1) {
			spin_lock_irqsave(&info->irq_spinlock,flags);
			c = min_t(int, count,
				min(SERIAL_XMIT_SIZE - info->xmit_cnt - 1,
				    SERIAL_XMIT_SIZE - info->xmit_head));
			if (c <= 0) {
				spin_unlock_irqrestore(&info->irq_spinlock,flags);
				break;
			}
			memcpy(info->xmit_buf + info->xmit_head, buf, c);
			info->xmit_head = ((info->xmit_head + c) &
					   (SERIAL_XMIT_SIZE-1));
			info->xmit_cnt += c;
			spin_unlock_irqrestore(&info->irq_spinlock,flags);
			buf += c;
			count -= c;
			ret += c;
		}
	}	
	
 	if (info->xmit_cnt && !tty->stopped && !tty->hw_stopped) {
		spin_lock_irqsave(&info->irq_spinlock,flags);
		if (!info->tx_active)
		 	usc_start_transmitter(info);
		spin_unlock_irqrestore(&info->irq_spinlock,flags);
 	}
cleanup:	
	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk( "%s(%d):mgsl_write(%s) returning=%d\n",
			__FILE__,__LINE__,info->device_name,ret);
			
	return ret;
	
}	

static int mgsl_write_room(struct tty_struct *tty)
{
	struct mgsl_struct *info = tty->driver_data;
	int	ret;
				
	if (mgsl_paranoia_check(info, tty->name, "mgsl_write_room"))
		return 0;
	ret = SERIAL_XMIT_SIZE - info->xmit_cnt - 1;
	if (ret < 0)
		ret = 0;
		
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_write_room(%s)=%d\n",
			 __FILE__,__LINE__, info->device_name,ret );
			 
	if ( info->params.mode == MGSL_MODE_HDLC ||
		info->params.mode == MGSL_MODE_RAW ) {
		
		if ( info->tx_active )
			return 0;
		else
			return HDLC_MAX_FRAME_SIZE;
	}
	
	return ret;
	
}	

static int mgsl_chars_in_buffer(struct tty_struct *tty)
{
	struct mgsl_struct *info = tty->driver_data;
			 
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_chars_in_buffer(%s)\n",
			 __FILE__,__LINE__, info->device_name );
			 
	if (mgsl_paranoia_check(info, tty->name, "mgsl_chars_in_buffer"))
		return 0;
		
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_chars_in_buffer(%s)=%d\n",
			 __FILE__,__LINE__, info->device_name,info->xmit_cnt );
			 
	if ( info->params.mode == MGSL_MODE_HDLC ||
		info->params.mode == MGSL_MODE_RAW ) {
		
		if ( info->tx_active )
			return info->max_frame_size;
		else
			return 0;
	}
			 
	return info->xmit_cnt;
}	

static void mgsl_flush_buffer(struct tty_struct *tty)
{
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_flush_buffer(%s) entry\n",
			 __FILE__,__LINE__, info->device_name );
	
	if (mgsl_paranoia_check(info, tty->name, "mgsl_flush_buffer"))
		return;
		
	spin_lock_irqsave(&info->irq_spinlock,flags); 
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
	del_timer(&info->tx_timer);	
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	
	tty_wakeup(tty);
}

static void mgsl_send_xchar(struct tty_struct *tty, char ch)
{
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_send_xchar(%s,%d)\n",
			 __FILE__,__LINE__, info->device_name, ch );
			 
	if (mgsl_paranoia_check(info, tty->name, "mgsl_send_xchar"))
		return;

	info->x_char = ch;
	if (ch) {
		
		spin_lock_irqsave(&info->irq_spinlock,flags);
		if (!info->tx_enabled)
		 	usc_start_transmitter(info);
		spin_unlock_irqrestore(&info->irq_spinlock,flags);
	}
}	

static void mgsl_throttle(struct tty_struct * tty)
{
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_throttle(%s) entry\n",
			 __FILE__,__LINE__, info->device_name );

	if (mgsl_paranoia_check(info, tty->name, "mgsl_throttle"))
		return;
	
	if (I_IXOFF(tty))
		mgsl_send_xchar(tty, STOP_CHAR(tty));
 
 	if (tty->termios->c_cflag & CRTSCTS) {
		spin_lock_irqsave(&info->irq_spinlock,flags);
		info->serial_signals &= ~SerialSignal_RTS;
	 	usc_set_serial_signals(info);
		spin_unlock_irqrestore(&info->irq_spinlock,flags);
	}
}	

static void mgsl_unthrottle(struct tty_struct * tty)
{
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_unthrottle(%s) entry\n",
			 __FILE__,__LINE__, info->device_name );

	if (mgsl_paranoia_check(info, tty->name, "mgsl_unthrottle"))
		return;
	
	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else
			mgsl_send_xchar(tty, START_CHAR(tty));
	}
	
 	if (tty->termios->c_cflag & CRTSCTS) {
		spin_lock_irqsave(&info->irq_spinlock,flags);
		info->serial_signals |= SerialSignal_RTS;
	 	usc_set_serial_signals(info);
		spin_unlock_irqrestore(&info->irq_spinlock,flags);
	}
	
}	

static int mgsl_get_stats(struct mgsl_struct * info, struct mgsl_icount __user *user_icount)
{
	int err;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_get_params(%s)\n",
			 __FILE__,__LINE__, info->device_name);
			
	if (!user_icount) {
		memset(&info->icount, 0, sizeof(info->icount));
	} else {
		mutex_lock(&info->port.mutex);
		COPY_TO_USER(err, user_icount, &info->icount, sizeof(struct mgsl_icount));
		mutex_unlock(&info->port.mutex);
		if (err)
			return -EFAULT;
	}
	
	return 0;
	
}	

static int mgsl_get_params(struct mgsl_struct * info, MGSL_PARAMS __user *user_params)
{
	int err;
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_get_params(%s)\n",
			 __FILE__,__LINE__, info->device_name);
			
	mutex_lock(&info->port.mutex);
	COPY_TO_USER(err,user_params, &info->params, sizeof(MGSL_PARAMS));
	mutex_unlock(&info->port.mutex);
	if (err) {
		if ( debug_level >= DEBUG_LEVEL_INFO )
			printk( "%s(%d):mgsl_get_params(%s) user buffer copy failed\n",
				__FILE__,__LINE__,info->device_name);
		return -EFAULT;
	}
	
	return 0;
	
}	

static int mgsl_set_params(struct mgsl_struct * info, MGSL_PARAMS __user *new_params)
{
 	unsigned long flags;
	MGSL_PARAMS tmp_params;
	int err;
 
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_set_params %s\n", __FILE__,__LINE__,
			info->device_name );
	COPY_FROM_USER(err,&tmp_params, new_params, sizeof(MGSL_PARAMS));
	if (err) {
		if ( debug_level >= DEBUG_LEVEL_INFO )
			printk( "%s(%d):mgsl_set_params(%s) user buffer copy failed\n",
				__FILE__,__LINE__,info->device_name);
		return -EFAULT;
	}
	
	mutex_lock(&info->port.mutex);
	spin_lock_irqsave(&info->irq_spinlock,flags);
	memcpy(&info->params,&tmp_params,sizeof(MGSL_PARAMS));
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	
 	mgsl_change_params(info);
	mutex_unlock(&info->port.mutex);
	
	return 0;
	
}	

static int mgsl_get_txidle(struct mgsl_struct * info, int __user *idle_mode)
{
	int err;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_get_txidle(%s)=%d\n",
			 __FILE__,__LINE__, info->device_name, info->idle_mode);
			
	COPY_TO_USER(err,idle_mode, &info->idle_mode, sizeof(int));
	if (err) {
		if ( debug_level >= DEBUG_LEVEL_INFO )
			printk( "%s(%d):mgsl_get_txidle(%s) user buffer copy failed\n",
				__FILE__,__LINE__,info->device_name);
		return -EFAULT;
	}
	
	return 0;
	
}	

static int mgsl_set_txidle(struct mgsl_struct * info, int idle_mode)
{
 	unsigned long flags;
 
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_set_txidle(%s,%d)\n", __FILE__,__LINE__,
			info->device_name, idle_mode );
			
	spin_lock_irqsave(&info->irq_spinlock,flags);
	info->idle_mode = idle_mode;
	usc_set_txidle( info );
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	return 0;
	
}	

static int mgsl_txenable(struct mgsl_struct * info, int enable)
{
 	unsigned long flags;
 
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_txenable(%s,%d)\n", __FILE__,__LINE__,
			info->device_name, enable);
			
	spin_lock_irqsave(&info->irq_spinlock,flags);
	if ( enable ) {
		if ( !info->tx_enabled ) {

			usc_start_transmitter(info);
			if ( info->params.flags & HDLC_FLAG_HDLC_LOOPMODE )
				usc_loopmode_insert_request( info );
		}
	} else {
		if ( info->tx_enabled )
			usc_stop_transmitter(info);
	}
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	return 0;
	
}	

static int mgsl_txabort(struct mgsl_struct * info)
{
 	unsigned long flags;
 
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_txabort(%s)\n", __FILE__,__LINE__,
			info->device_name);
			
	spin_lock_irqsave(&info->irq_spinlock,flags);
	if ( info->tx_active && info->params.mode == MGSL_MODE_HDLC )
	{
		if ( info->params.flags & HDLC_FLAG_HDLC_LOOPMODE )
			usc_loopmode_cancel_transmit( info );
		else
			usc_TCmd(info,TCmd_SendAbort);
	}
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	return 0;
	
}	

static int mgsl_rxenable(struct mgsl_struct * info, int enable)
{
 	unsigned long flags;
 
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_rxenable(%s,%d)\n", __FILE__,__LINE__,
			info->device_name, enable);
			
	spin_lock_irqsave(&info->irq_spinlock,flags);
	if ( enable ) {
		if ( !info->rx_enabled )
			usc_start_receiver(info);
	} else {
		if ( info->rx_enabled )
			usc_stop_receiver(info);
	}
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	return 0;
	
}	

static int mgsl_wait_event(struct mgsl_struct * info, int __user * mask_ptr)
{
 	unsigned long flags;
	int s;
	int rc=0;
	struct mgsl_icount cprev, cnow;
	int events;
	int mask;
	struct	_input_signal_events oldsigs, newsigs;
	DECLARE_WAITQUEUE(wait, current);

	COPY_FROM_USER(rc,&mask, mask_ptr, sizeof(int));
	if (rc) {
		return  -EFAULT;
	}
		 
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_wait_event(%s,%d)\n", __FILE__,__LINE__,
			info->device_name, mask);

	spin_lock_irqsave(&info->irq_spinlock,flags);

	
	usc_get_serial_signals(info);
	s = info->serial_signals;
	events = mask &
		( ((s & SerialSignal_DSR) ? MgslEvent_DsrActive:MgslEvent_DsrInactive) +
 		  ((s & SerialSignal_DCD) ? MgslEvent_DcdActive:MgslEvent_DcdInactive) +
		  ((s & SerialSignal_CTS) ? MgslEvent_CtsActive:MgslEvent_CtsInactive) +
		  ((s & SerialSignal_RI)  ? MgslEvent_RiActive :MgslEvent_RiInactive) );
	if (events) {
		spin_unlock_irqrestore(&info->irq_spinlock,flags);
		goto exit;
	}

	
	cprev = info->icount;
	oldsigs = info->input_signal_events;
	
	
	if (mask & (MgslEvent_ExitHuntMode + MgslEvent_IdleReceived)) {
		u16 oldreg = usc_InReg(info,RICR);
		u16 newreg = oldreg +
			 (mask & MgslEvent_ExitHuntMode ? RXSTATUS_EXITED_HUNT:0) +
			 (mask & MgslEvent_IdleReceived ? RXSTATUS_IDLE_RECEIVED:0);
		if (oldreg != newreg)
			usc_OutReg(info, RICR, newreg);
	}
	
	set_current_state(TASK_INTERRUPTIBLE);
	add_wait_queue(&info->event_wait_q, &wait);
	
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	

	for(;;) {
		schedule();
		if (signal_pending(current)) {
			rc = -ERESTARTSYS;
			break;
		}
			
		
		spin_lock_irqsave(&info->irq_spinlock,flags);
		cnow = info->icount;
		newsigs = info->input_signal_events;
		set_current_state(TASK_INTERRUPTIBLE);
		spin_unlock_irqrestore(&info->irq_spinlock,flags);

		
		if (newsigs.dsr_up   == oldsigs.dsr_up   &&
		    newsigs.dsr_down == oldsigs.dsr_down &&
		    newsigs.dcd_up   == oldsigs.dcd_up   &&
		    newsigs.dcd_down == oldsigs.dcd_down &&
		    newsigs.cts_up   == oldsigs.cts_up   &&
		    newsigs.cts_down == oldsigs.cts_down &&
		    newsigs.ri_up    == oldsigs.ri_up    &&
		    newsigs.ri_down  == oldsigs.ri_down  &&
		    cnow.exithunt    == cprev.exithunt   &&
		    cnow.rxidle      == cprev.rxidle) {
			rc = -EIO;
			break;
		}

		events = mask &
			( (newsigs.dsr_up   != oldsigs.dsr_up   ? MgslEvent_DsrActive:0)   +
			(newsigs.dsr_down != oldsigs.dsr_down ? MgslEvent_DsrInactive:0) +
			(newsigs.dcd_up   != oldsigs.dcd_up   ? MgslEvent_DcdActive:0)   +
			(newsigs.dcd_down != oldsigs.dcd_down ? MgslEvent_DcdInactive:0) +
			(newsigs.cts_up   != oldsigs.cts_up   ? MgslEvent_CtsActive:0)   +
			(newsigs.cts_down != oldsigs.cts_down ? MgslEvent_CtsInactive:0) +
			(newsigs.ri_up    != oldsigs.ri_up    ? MgslEvent_RiActive:0)    +
			(newsigs.ri_down  != oldsigs.ri_down  ? MgslEvent_RiInactive:0)  +
			(cnow.exithunt    != cprev.exithunt   ? MgslEvent_ExitHuntMode:0) +
			  (cnow.rxidle      != cprev.rxidle     ? MgslEvent_IdleReceived:0) );
		if (events)
			break;
		
		cprev = cnow;
		oldsigs = newsigs;
	}
	
	remove_wait_queue(&info->event_wait_q, &wait);
	set_current_state(TASK_RUNNING);

	if (mask & (MgslEvent_ExitHuntMode + MgslEvent_IdleReceived)) {
		spin_lock_irqsave(&info->irq_spinlock,flags);
		if (!waitqueue_active(&info->event_wait_q)) {
			
			usc_OutReg(info, RICR, usc_InReg(info,RICR) &
				~(RXSTATUS_EXITED_HUNT + RXSTATUS_IDLE_RECEIVED));
		}
		spin_unlock_irqrestore(&info->irq_spinlock,flags);
	}
exit:
	if ( rc == 0 )
		PUT_USER(rc, events, mask_ptr);
		
	return rc;
	
}	

static int modem_input_wait(struct mgsl_struct *info,int arg)
{
 	unsigned long flags;
	int rc;
	struct mgsl_icount cprev, cnow;
	DECLARE_WAITQUEUE(wait, current);

	
	spin_lock_irqsave(&info->irq_spinlock,flags);
	cprev = info->icount;
	add_wait_queue(&info->status_event_wait_q, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	spin_unlock_irqrestore(&info->irq_spinlock,flags);

	for(;;) {
		schedule();
		if (signal_pending(current)) {
			rc = -ERESTARTSYS;
			break;
		}

		
		spin_lock_irqsave(&info->irq_spinlock,flags);
		cnow = info->icount;
		set_current_state(TASK_INTERRUPTIBLE);
		spin_unlock_irqrestore(&info->irq_spinlock,flags);

		
		if (cnow.rng == cprev.rng && cnow.dsr == cprev.dsr &&
		    cnow.dcd == cprev.dcd && cnow.cts == cprev.cts) {
			rc = -EIO;
			break;
		}

		
		if ((arg & TIOCM_RNG && cnow.rng != cprev.rng) ||
		    (arg & TIOCM_DSR && cnow.dsr != cprev.dsr) ||
		    (arg & TIOCM_CD  && cnow.dcd != cprev.dcd) ||
		    (arg & TIOCM_CTS && cnow.cts != cprev.cts)) {
			rc = 0;
			break;
		}

		cprev = cnow;
	}
	remove_wait_queue(&info->status_event_wait_q, &wait);
	set_current_state(TASK_RUNNING);
	return rc;
}

static int tiocmget(struct tty_struct *tty)
{
	struct mgsl_struct *info = tty->driver_data;
	unsigned int result;
 	unsigned long flags;

	spin_lock_irqsave(&info->irq_spinlock,flags);
 	usc_get_serial_signals(info);
	spin_unlock_irqrestore(&info->irq_spinlock,flags);

	result = ((info->serial_signals & SerialSignal_RTS) ? TIOCM_RTS:0) +
		((info->serial_signals & SerialSignal_DTR) ? TIOCM_DTR:0) +
		((info->serial_signals & SerialSignal_DCD) ? TIOCM_CAR:0) +
		((info->serial_signals & SerialSignal_RI)  ? TIOCM_RNG:0) +
		((info->serial_signals & SerialSignal_DSR) ? TIOCM_DSR:0) +
		((info->serial_signals & SerialSignal_CTS) ? TIOCM_CTS:0);

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s tiocmget() value=%08X\n",
			 __FILE__,__LINE__, info->device_name, result );
	return result;
}

static int tiocmset(struct tty_struct *tty,
				    unsigned int set, unsigned int clear)
{
	struct mgsl_struct *info = tty->driver_data;
 	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):%s tiocmset(%x,%x)\n",
			__FILE__,__LINE__,info->device_name, set, clear);

	if (set & TIOCM_RTS)
		info->serial_signals |= SerialSignal_RTS;
	if (set & TIOCM_DTR)
		info->serial_signals |= SerialSignal_DTR;
	if (clear & TIOCM_RTS)
		info->serial_signals &= ~SerialSignal_RTS;
	if (clear & TIOCM_DTR)
		info->serial_signals &= ~SerialSignal_DTR;

	spin_lock_irqsave(&info->irq_spinlock,flags);
 	usc_set_serial_signals(info);
	spin_unlock_irqrestore(&info->irq_spinlock,flags);

	return 0;
}

static int mgsl_break(struct tty_struct *tty, int break_state)
{
	struct mgsl_struct * info = tty->driver_data;
	unsigned long flags;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_break(%s,%d)\n",
			 __FILE__,__LINE__, info->device_name, break_state);
			 
	if (mgsl_paranoia_check(info, tty->name, "mgsl_break"))
		return -EINVAL;

	spin_lock_irqsave(&info->irq_spinlock,flags);
 	if (break_state == -1)
		usc_OutReg(info,IOCR,(u16)(usc_InReg(info,IOCR) | BIT7));
	else 
		usc_OutReg(info,IOCR,(u16)(usc_InReg(info,IOCR) & ~BIT7));
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	return 0;
	
}	

static int msgl_get_icount(struct tty_struct *tty,
				struct serial_icounter_struct *icount)

{
	struct mgsl_struct * info = tty->driver_data;
	struct mgsl_icount cnow;	
	unsigned long flags;

	spin_lock_irqsave(&info->irq_spinlock,flags);
	cnow = info->icount;
	spin_unlock_irqrestore(&info->irq_spinlock,flags);

	icount->cts = cnow.cts;
	icount->dsr = cnow.dsr;
	icount->rng = cnow.rng;
	icount->dcd = cnow.dcd;
	icount->rx = cnow.rx;
	icount->tx = cnow.tx;
	icount->frame = cnow.frame;
	icount->overrun = cnow.overrun;
	icount->parity = cnow.parity;
	icount->brk = cnow.brk;
	icount->buf_overrun = cnow.buf_overrun;
	return 0;
}

static int mgsl_ioctl(struct tty_struct *tty,
		    unsigned int cmd, unsigned long arg)
{
	struct mgsl_struct * info = tty->driver_data;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_ioctl %s cmd=%08X\n", __FILE__,__LINE__,
			info->device_name, cmd );
	
	if (mgsl_paranoia_check(info, tty->name, "mgsl_ioctl"))
		return -ENODEV;

	if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
	    (cmd != TIOCMIWAIT)) {
		if (tty->flags & (1 << TTY_IO_ERROR))
		    return -EIO;
	}

	return mgsl_ioctl_common(info, cmd, arg);
}

static int mgsl_ioctl_common(struct mgsl_struct *info, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	
	switch (cmd) {
		case MGSL_IOCGPARAMS:
			return mgsl_get_params(info, argp);
		case MGSL_IOCSPARAMS:
			return mgsl_set_params(info, argp);
		case MGSL_IOCGTXIDLE:
			return mgsl_get_txidle(info, argp);
		case MGSL_IOCSTXIDLE:
			return mgsl_set_txidle(info,(int)arg);
		case MGSL_IOCTXENABLE:
			return mgsl_txenable(info,(int)arg);
		case MGSL_IOCRXENABLE:
			return mgsl_rxenable(info,(int)arg);
		case MGSL_IOCTXABORT:
			return mgsl_txabort(info);
		case MGSL_IOCGSTATS:
			return mgsl_get_stats(info, argp);
		case MGSL_IOCWAITEVENT:
			return mgsl_wait_event(info, argp);
		case MGSL_IOCLOOPTXDONE:
			return mgsl_loopmode_send_done(info);
		case TIOCMIWAIT:
			return modem_input_wait(info,(int)arg);

		default:
			return -ENOIOCTLCMD;
	}
	return 0;
}

static void mgsl_set_termios(struct tty_struct *tty, struct ktermios *old_termios)
{
	struct mgsl_struct *info = tty->driver_data;
	unsigned long flags;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_set_termios %s\n", __FILE__,__LINE__,
			tty->driver->name );
	
	mgsl_change_params(info);

	
	if (old_termios->c_cflag & CBAUD &&
	    !(tty->termios->c_cflag & CBAUD)) {
		info->serial_signals &= ~(SerialSignal_RTS + SerialSignal_DTR);
		spin_lock_irqsave(&info->irq_spinlock,flags);
	 	usc_set_serial_signals(info);
		spin_unlock_irqrestore(&info->irq_spinlock,flags);
	}
	
	
	if (!(old_termios->c_cflag & CBAUD) &&
	    tty->termios->c_cflag & CBAUD) {
		info->serial_signals |= SerialSignal_DTR;
 		if (!(tty->termios->c_cflag & CRTSCTS) || 
 		    !test_bit(TTY_THROTTLED, &tty->flags)) {
			info->serial_signals |= SerialSignal_RTS;
 		}
		spin_lock_irqsave(&info->irq_spinlock,flags);
	 	usc_set_serial_signals(info);
		spin_unlock_irqrestore(&info->irq_spinlock,flags);
	}
	
	
	if (old_termios->c_cflag & CRTSCTS &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		mgsl_start(tty);
	}

}	

static void mgsl_close(struct tty_struct *tty, struct file * filp)
{
	struct mgsl_struct * info = tty->driver_data;

	if (mgsl_paranoia_check(info, tty->name, "mgsl_close"))
		return;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_close(%s) entry, count=%d\n",
			 __FILE__,__LINE__, info->device_name, info->port.count);

	if (tty_port_close_start(&info->port, tty, filp) == 0)			 
		goto cleanup;

	mutex_lock(&info->port.mutex);
 	if (info->port.flags & ASYNC_INITIALIZED)
 		mgsl_wait_until_sent(tty, info->timeout);
	mgsl_flush_buffer(tty);
	tty_ldisc_flush(tty);
	shutdown(info);
	mutex_unlock(&info->port.mutex);

	tty_port_close_end(&info->port, tty);	
	info->port.tty = NULL;
cleanup:			
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_close(%s) exit, count=%d\n", __FILE__,__LINE__,
			tty->driver->name, info->port.count);
			
}	

static void mgsl_wait_until_sent(struct tty_struct *tty, int timeout)
{
	struct mgsl_struct * info = tty->driver_data;
	unsigned long orig_jiffies, char_time;

	if (!info )
		return;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_wait_until_sent(%s) entry\n",
			 __FILE__,__LINE__, info->device_name );
      
	if (mgsl_paranoia_check(info, tty->name, "mgsl_wait_until_sent"))
		return;

	if (!(info->port.flags & ASYNC_INITIALIZED))
		goto exit;
	 
	orig_jiffies = jiffies;
      
 

	if ( info->params.data_rate ) {
	       	char_time = info->timeout/(32 * 5);
		if (!char_time)
			char_time++;
	} else
		char_time = 1;
		
	if (timeout)
		char_time = min_t(unsigned long, char_time, timeout);
		
	if ( info->params.mode == MGSL_MODE_HDLC ||
		info->params.mode == MGSL_MODE_RAW ) {
		while (info->tx_active) {
			msleep_interruptible(jiffies_to_msecs(char_time));
			if (signal_pending(current))
				break;
			if (timeout && time_after(jiffies, orig_jiffies + timeout))
				break;
		}
	} else {
		while (!(usc_InReg(info,TCSR) & TXSTATUS_ALL_SENT) &&
			info->tx_enabled) {
			msleep_interruptible(jiffies_to_msecs(char_time));
			if (signal_pending(current))
				break;
			if (timeout && time_after(jiffies, orig_jiffies + timeout))
				break;
		}
	}
      
exit:
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_wait_until_sent(%s) exit\n",
			 __FILE__,__LINE__, info->device_name );
			 
}	

static void mgsl_hangup(struct tty_struct *tty)
{
	struct mgsl_struct * info = tty->driver_data;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_hangup(%s)\n",
			 __FILE__,__LINE__, info->device_name );
			 
	if (mgsl_paranoia_check(info, tty->name, "mgsl_hangup"))
		return;

	mgsl_flush_buffer(tty);
	shutdown(info);
	
	info->port.count = 0;	
	info->port.flags &= ~ASYNC_NORMAL_ACTIVE;
	info->port.tty = NULL;

	wake_up_interruptible(&info->port.open_wait);
	
}	


static int carrier_raised(struct tty_port *port)
{
	unsigned long flags;
	struct mgsl_struct *info = container_of(port, struct mgsl_struct, port);
	
	spin_lock_irqsave(&info->irq_spinlock, flags);
 	usc_get_serial_signals(info);
	spin_unlock_irqrestore(&info->irq_spinlock, flags);
	return (info->serial_signals & SerialSignal_DCD) ? 1 : 0;
}

static void dtr_rts(struct tty_port *port, int on)
{
	struct mgsl_struct *info = container_of(port, struct mgsl_struct, port);
	unsigned long flags;

	spin_lock_irqsave(&info->irq_spinlock,flags);
	if (on)
		info->serial_signals |= SerialSignal_RTS + SerialSignal_DTR;
	else
		info->serial_signals &= ~(SerialSignal_RTS + SerialSignal_DTR);
 	usc_set_serial_signals(info);
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
}


static int block_til_ready(struct tty_struct *tty, struct file * filp,
			   struct mgsl_struct *info)
{
	DECLARE_WAITQUEUE(wait, current);
	int		retval;
	bool		do_clocal = false;
	bool		extra_count = false;
	unsigned long	flags;
	int		dcd;
	struct tty_port *port = &info->port;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):block_til_ready on %s\n",
			 __FILE__,__LINE__, tty->driver->name );

	if (filp->f_flags & O_NONBLOCK || tty->flags & (1 << TTY_IO_ERROR)){
		
		port->flags |= ASYNC_NORMAL_ACTIVE;
		return 0;
	}

	if (tty->termios->c_cflag & CLOCAL)
		do_clocal = true;

	 
	retval = 0;
	add_wait_queue(&port->open_wait, &wait);
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):block_til_ready before block on %s count=%d\n",
			 __FILE__,__LINE__, tty->driver->name, port->count );

	spin_lock_irqsave(&info->irq_spinlock, flags);
	if (!tty_hung_up_p(filp)) {
		extra_count = true;
		port->count--;
	}
	spin_unlock_irqrestore(&info->irq_spinlock, flags);
	port->blocked_open++;
	
	while (1) {
		if (tty->termios->c_cflag & CBAUD)
			tty_port_raise_dtr_rts(port);
		
		set_current_state(TASK_INTERRUPTIBLE);
		
		if (tty_hung_up_p(filp) || !(port->flags & ASYNC_INITIALIZED)){
			retval = (port->flags & ASYNC_HUP_NOTIFY) ?
					-EAGAIN : -ERESTARTSYS;
			break;
		}
		
		dcd = tty_port_carrier_raised(&info->port);
		
 		if (!(port->flags & ASYNC_CLOSING) && (do_clocal || dcd))
 			break;
			
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
		
		if (debug_level >= DEBUG_LEVEL_INFO)
			printk("%s(%d):block_til_ready blocking on %s count=%d\n",
				 __FILE__,__LINE__, tty->driver->name, port->count );
				 
		tty_unlock();
		schedule();
		tty_lock();
	}
	
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&port->open_wait, &wait);
	
	
	if (extra_count)
		port->count++;
	port->blocked_open--;
	
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):block_til_ready after blocking on %s count=%d\n",
			 __FILE__,__LINE__, tty->driver->name, port->count );
			 
	if (!retval)
		port->flags |= ASYNC_NORMAL_ACTIVE;
		
	return retval;
	
}	

static int mgsl_open(struct tty_struct *tty, struct file * filp)
{
	struct mgsl_struct	*info;
	int 			retval, line;
	unsigned long flags;

		
	line = tty->index;
	if (line >= mgsl_device_count) {
		printk("%s(%d):mgsl_open with invalid line #%d.\n",
			__FILE__,__LINE__,line);
		return -ENODEV;
	}

	
	info = mgsl_device_list;
	while(info && info->line != line)
		info = info->next_device;
	if (mgsl_paranoia_check(info, tty->name, "mgsl_open"))
		return -ENODEV;
	
	tty->driver_data = info;
	info->port.tty = tty;
		
	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_open(%s), old ref count = %d\n",
			 __FILE__,__LINE__,tty->driver->name, info->port.count);

	
	if (tty_hung_up_p(filp) || info->port.flags & ASYNC_CLOSING){
		if (info->port.flags & ASYNC_CLOSING)
			interruptible_sleep_on(&info->port.close_wait);
		retval = ((info->port.flags & ASYNC_HUP_NOTIFY) ?
			-EAGAIN : -ERESTARTSYS);
		goto cleanup;
	}
	
	info->port.tty->low_latency = (info->port.flags & ASYNC_LOW_LATENCY) ? 1 : 0;

	spin_lock_irqsave(&info->netlock, flags);
	if (info->netcount) {
		retval = -EBUSY;
		spin_unlock_irqrestore(&info->netlock, flags);
		goto cleanup;
	}
	info->port.count++;
	spin_unlock_irqrestore(&info->netlock, flags);

	if (info->port.count == 1) {
		
		retval = startup(info);
		if (retval < 0)
			goto cleanup;
	}

	retval = block_til_ready(tty, filp, info);
	if (retval) {
		if (debug_level >= DEBUG_LEVEL_INFO)
			printk("%s(%d):block_til_ready(%s) returned %d\n",
				 __FILE__,__LINE__, info->device_name, retval);
		goto cleanup;
	}

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s(%d):mgsl_open(%s) success\n",
			 __FILE__,__LINE__, info->device_name);
	retval = 0;
	
cleanup:			
	if (retval) {
		if (tty->count == 1)
			info->port.tty = NULL; 
		if(info->port.count)
			info->port.count--;
	}
	
	return retval;
	
}	


static inline void line_info(struct seq_file *m, struct mgsl_struct *info)
{
	char	stat_buf[30];
	unsigned long flags;

	if (info->bus_type == MGSL_BUS_TYPE_PCI) {
		seq_printf(m, "%s:PCI io:%04X irq:%d mem:%08X lcr:%08X",
			info->device_name, info->io_base, info->irq_level,
			info->phys_memory_base, info->phys_lcr_base);
	} else {
		seq_printf(m, "%s:(E)ISA io:%04X irq:%d dma:%d",
			info->device_name, info->io_base, 
			info->irq_level, info->dma_level);
	}

	
	spin_lock_irqsave(&info->irq_spinlock,flags);
 	usc_get_serial_signals(info);
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	
	stat_buf[0] = 0;
	stat_buf[1] = 0;
	if (info->serial_signals & SerialSignal_RTS)
		strcat(stat_buf, "|RTS");
	if (info->serial_signals & SerialSignal_CTS)
		strcat(stat_buf, "|CTS");
	if (info->serial_signals & SerialSignal_DTR)
		strcat(stat_buf, "|DTR");
	if (info->serial_signals & SerialSignal_DSR)
		strcat(stat_buf, "|DSR");
	if (info->serial_signals & SerialSignal_DCD)
		strcat(stat_buf, "|CD");
	if (info->serial_signals & SerialSignal_RI)
		strcat(stat_buf, "|RI");

	if (info->params.mode == MGSL_MODE_HDLC ||
	    info->params.mode == MGSL_MODE_RAW ) {
		seq_printf(m, " HDLC txok:%d rxok:%d",
			      info->icount.txok, info->icount.rxok);
		if (info->icount.txunder)
			seq_printf(m, " txunder:%d", info->icount.txunder);
		if (info->icount.txabort)
			seq_printf(m, " txabort:%d", info->icount.txabort);
		if (info->icount.rxshort)
			seq_printf(m, " rxshort:%d", info->icount.rxshort);
		if (info->icount.rxlong)
			seq_printf(m, " rxlong:%d", info->icount.rxlong);
		if (info->icount.rxover)
			seq_printf(m, " rxover:%d", info->icount.rxover);
		if (info->icount.rxcrc)
			seq_printf(m, " rxcrc:%d", info->icount.rxcrc);
	} else {
		seq_printf(m, " ASYNC tx:%d rx:%d",
			      info->icount.tx, info->icount.rx);
		if (info->icount.frame)
			seq_printf(m, " fe:%d", info->icount.frame);
		if (info->icount.parity)
			seq_printf(m, " pe:%d", info->icount.parity);
		if (info->icount.brk)
			seq_printf(m, " brk:%d", info->icount.brk);
		if (info->icount.overrun)
			seq_printf(m, " oe:%d", info->icount.overrun);
	}
	
	
	seq_printf(m, " %s\n", stat_buf+1);
	
	seq_printf(m, "txactive=%d bh_req=%d bh_run=%d pending_bh=%x\n",
	 info->tx_active,info->bh_requested,info->bh_running,
	 info->pending_bh);
	 
	spin_lock_irqsave(&info->irq_spinlock,flags);
	{	
	u16 Tcsr = usc_InReg( info, TCSR );
	u16 Tdmr = usc_InDmaReg( info, TDMR );
	u16 Ticr = usc_InReg( info, TICR );
	u16 Rscr = usc_InReg( info, RCSR );
	u16 Rdmr = usc_InDmaReg( info, RDMR );
	u16 Ricr = usc_InReg( info, RICR );
	u16 Icr = usc_InReg( info, ICR );
	u16 Dccr = usc_InReg( info, DCCR );
	u16 Tmr = usc_InReg( info, TMR );
	u16 Tccr = usc_InReg( info, TCCR );
	u16 Ccar = inw( info->io_base + CCAR );
	seq_printf(m, "tcsr=%04X tdmr=%04X ticr=%04X rcsr=%04X rdmr=%04X\n"
                        "ricr=%04X icr =%04X dccr=%04X tmr=%04X tccr=%04X ccar=%04X\n",
	 		Tcsr,Tdmr,Ticr,Rscr,Rdmr,Ricr,Icr,Dccr,Tmr,Tccr,Ccar );
	}
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
}

static int mgsl_proc_show(struct seq_file *m, void *v)
{
	struct mgsl_struct *info;
	
	seq_printf(m, "synclink driver:%s\n", driver_version);
	
	info = mgsl_device_list;
	while( info ) {
		line_info(m, info);
		info = info->next_device;
	}
	return 0;
}

static int mgsl_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mgsl_proc_show, NULL);
}

static const struct file_operations mgsl_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= mgsl_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int mgsl_allocate_dma_buffers(struct mgsl_struct *info)
{
	unsigned short BuffersPerFrame;

	info->last_mem_alloc = 0;

	
	
	
	

	BuffersPerFrame = (unsigned short)(info->max_frame_size/DMABUFFERSIZE);
	if ( info->max_frame_size % DMABUFFERSIZE )
		BuffersPerFrame++;

	if ( info->bus_type == MGSL_BUS_TYPE_PCI ) {
		info->tx_buffer_count = info->num_tx_dma_buffers * BuffersPerFrame;
		info->rx_buffer_count = 62 - info->tx_buffer_count;
	} else {
		
		


		
		
		
		
		

		info->tx_buffer_count = info->num_tx_dma_buffers * BuffersPerFrame;
		info->rx_buffer_count = (BuffersPerFrame * MAXRXFRAMES) + 6;
		
		
		if ( (info->tx_buffer_count + info->rx_buffer_count) > 62 )
			info->rx_buffer_count = 62 - info->tx_buffer_count;

	}

	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk("%s(%d):Allocating %d TX and %d RX DMA buffers.\n",
			__FILE__,__LINE__, info->tx_buffer_count,info->rx_buffer_count);
	
	if ( mgsl_alloc_buffer_list_memory( info ) < 0 ||
		  mgsl_alloc_frame_memory(info, info->rx_buffer_list, info->rx_buffer_count) < 0 || 
		  mgsl_alloc_frame_memory(info, info->tx_buffer_list, info->tx_buffer_count) < 0 || 
		  mgsl_alloc_intermediate_rxbuffer_memory(info) < 0  ||
		  mgsl_alloc_intermediate_txbuffer_memory(info) < 0 ) {
		printk("%s(%d):Can't allocate DMA buffer memory\n",__FILE__,__LINE__);
		return -ENOMEM;
	}
	
	mgsl_reset_rx_dma_buffers( info );
  	mgsl_reset_tx_dma_buffers( info );

	return 0;

}	

static int mgsl_alloc_buffer_list_memory( struct mgsl_struct *info )
{
	unsigned int i;

	if ( info->bus_type == MGSL_BUS_TYPE_PCI ) {
		
		info->buffer_list = info->memory_base + info->last_mem_alloc;
		info->buffer_list_phys = info->last_mem_alloc;
		info->last_mem_alloc += BUFFERLISTSIZE;
	} else {
		
		
		
		
		

		info->buffer_list = dma_alloc_coherent(NULL, BUFFERLISTSIZE, &info->buffer_list_dma_addr, GFP_KERNEL);
		if (info->buffer_list == NULL)
			return -ENOMEM;
		info->buffer_list_phys = (u32)(info->buffer_list_dma_addr);
	}

	
	
	memset( info->buffer_list, 0, BUFFERLISTSIZE );

	
	
	
	info->rx_buffer_list = (DMABUFFERENTRY *)info->buffer_list;
	info->tx_buffer_list = (DMABUFFERENTRY *)info->buffer_list;
	info->tx_buffer_list += info->rx_buffer_count;


	for ( i = 0; i < info->rx_buffer_count; i++ ) {
		
		info->rx_buffer_list[i].phys_entry =
			info->buffer_list_phys + (i * sizeof(DMABUFFERENTRY));

		
		

		info->rx_buffer_list[i].link = info->buffer_list_phys;

		if ( i < info->rx_buffer_count - 1 )
			info->rx_buffer_list[i].link += (i + 1) * sizeof(DMABUFFERENTRY);
	}

	for ( i = 0; i < info->tx_buffer_count; i++ ) {
		
		info->tx_buffer_list[i].phys_entry = info->buffer_list_phys +
			((info->rx_buffer_count + i) * sizeof(DMABUFFERENTRY));

		
		

		info->tx_buffer_list[i].link = info->buffer_list_phys +
			info->rx_buffer_count * sizeof(DMABUFFERENTRY);

		if ( i < info->tx_buffer_count - 1 )
			info->tx_buffer_list[i].link += (i + 1) * sizeof(DMABUFFERENTRY);
	}

	return 0;

}	

static void mgsl_free_buffer_list_memory( struct mgsl_struct *info )
{
	if (info->buffer_list && info->bus_type != MGSL_BUS_TYPE_PCI)
		dma_free_coherent(NULL, BUFFERLISTSIZE, info->buffer_list, info->buffer_list_dma_addr);
		
	info->buffer_list = NULL;
	info->rx_buffer_list = NULL;
	info->tx_buffer_list = NULL;

}	

static int mgsl_alloc_frame_memory(struct mgsl_struct *info,DMABUFFERENTRY *BufferList,int Buffercount)
{
	int i;
	u32 phys_addr;

	

	for ( i = 0; i < Buffercount; i++ ) {
		if ( info->bus_type == MGSL_BUS_TYPE_PCI ) {
			
			BufferList[i].virt_addr = info->memory_base + info->last_mem_alloc;
			phys_addr = info->last_mem_alloc;
			info->last_mem_alloc += DMABUFFERSIZE;
		} else {
			
			BufferList[i].virt_addr = dma_alloc_coherent(NULL, DMABUFFERSIZE, &BufferList[i].dma_addr, GFP_KERNEL);
			if (BufferList[i].virt_addr == NULL)
				return -ENOMEM;
			phys_addr = (u32)(BufferList[i].dma_addr);
		}
		BufferList[i].phys_addr = phys_addr;
	}

	return 0;

}	

static void mgsl_free_frame_memory(struct mgsl_struct *info, DMABUFFERENTRY *BufferList, int Buffercount)
{
	int i;

	if ( BufferList ) {
		for ( i = 0 ; i < Buffercount ; i++ ) {
			if ( BufferList[i].virt_addr ) {
				if ( info->bus_type != MGSL_BUS_TYPE_PCI )
					dma_free_coherent(NULL, DMABUFFERSIZE, BufferList[i].virt_addr, BufferList[i].dma_addr);
				BufferList[i].virt_addr = NULL;
			}
		}
	}

}	

static void mgsl_free_dma_buffers( struct mgsl_struct *info )
{
	mgsl_free_frame_memory( info, info->rx_buffer_list, info->rx_buffer_count );
	mgsl_free_frame_memory( info, info->tx_buffer_list, info->tx_buffer_count );
	mgsl_free_buffer_list_memory( info );

}	


static int mgsl_alloc_intermediate_rxbuffer_memory(struct mgsl_struct *info)
{
	info->intermediate_rxbuffer = kmalloc(info->max_frame_size, GFP_KERNEL | GFP_DMA);
	if ( info->intermediate_rxbuffer == NULL )
		return -ENOMEM;

	return 0;

}	

static void mgsl_free_intermediate_rxbuffer_memory(struct mgsl_struct *info)
{
	kfree(info->intermediate_rxbuffer);
	info->intermediate_rxbuffer = NULL;

}	

static int mgsl_alloc_intermediate_txbuffer_memory(struct mgsl_struct *info)
{
	int i;

	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk("%s %s(%d)  allocating %d tx holding buffers\n",
				info->device_name, __FILE__,__LINE__,info->num_tx_holding_buffers);

	memset(info->tx_holding_buffers,0,sizeof(info->tx_holding_buffers));

	for ( i=0; i<info->num_tx_holding_buffers; ++i) {
		info->tx_holding_buffers[i].buffer =
			kmalloc(info->max_frame_size, GFP_KERNEL);
		if (info->tx_holding_buffers[i].buffer == NULL) {
			for (--i; i >= 0; i--) {
				kfree(info->tx_holding_buffers[i].buffer);
				info->tx_holding_buffers[i].buffer = NULL;
			}
			return -ENOMEM;
		}
	}

	return 0;

}	

static void mgsl_free_intermediate_txbuffer_memory(struct mgsl_struct *info)
{
	int i;

	for ( i=0; i<info->num_tx_holding_buffers; ++i ) {
		kfree(info->tx_holding_buffers[i].buffer);
		info->tx_holding_buffers[i].buffer = NULL;
	}

	info->get_tx_holding_index = 0;
	info->put_tx_holding_index = 0;
	info->tx_holding_count = 0;

}	


static bool load_next_tx_holding_buffer(struct mgsl_struct *info)
{
	bool ret = false;

	if ( info->tx_holding_count ) {
		struct tx_holding_buffer *ptx =
			&info->tx_holding_buffers[info->get_tx_holding_index];
		int num_free = num_free_tx_dma_buffers(info);
		int num_needed = ptx->buffer_size / DMABUFFERSIZE;
		if ( ptx->buffer_size % DMABUFFERSIZE )
			++num_needed;

		if (num_needed <= num_free) {
			info->xmit_cnt = ptx->buffer_size;
			mgsl_load_tx_dma_buffer(info,ptx->buffer,ptx->buffer_size);

			--info->tx_holding_count;
			if ( ++info->get_tx_holding_index >= info->num_tx_holding_buffers)
				info->get_tx_holding_index=0;

			
			mod_timer(&info->tx_timer, jiffies + msecs_to_jiffies(5000));

			ret = true;
		}
	}

	return ret;
}

static int save_tx_buffer_request(struct mgsl_struct *info,const char *Buffer, unsigned int BufferSize)
{
	struct tx_holding_buffer *ptx;

	if ( info->tx_holding_count >= info->num_tx_holding_buffers ) {
		return 0;	        
	}

	ptx = &info->tx_holding_buffers[info->put_tx_holding_index];
	ptx->buffer_size = BufferSize;
	memcpy( ptx->buffer, Buffer, BufferSize);

	++info->tx_holding_count;
	if ( ++info->put_tx_holding_index >= info->num_tx_holding_buffers)
		info->put_tx_holding_index=0;

	return 1;
}

static int mgsl_claim_resources(struct mgsl_struct *info)
{
	if (request_region(info->io_base,info->io_addr_size,"synclink") == NULL) {
		printk( "%s(%d):I/O address conflict on device %s Addr=%08X\n",
			__FILE__,__LINE__,info->device_name, info->io_base);
		return -ENODEV;
	}
	info->io_addr_requested = true;
	
	if ( request_irq(info->irq_level,mgsl_interrupt,info->irq_flags,
		info->device_name, info ) < 0 ) {
		printk( "%s(%d):Can't request interrupt on device %s IRQ=%d\n",
			__FILE__,__LINE__,info->device_name, info->irq_level );
		goto errout;
	}
	info->irq_requested = true;
	
	if ( info->bus_type == MGSL_BUS_TYPE_PCI ) {
		if (request_mem_region(info->phys_memory_base,0x40000,"synclink") == NULL) {
			printk( "%s(%d):mem addr conflict device %s Addr=%08X\n",
				__FILE__,__LINE__,info->device_name, info->phys_memory_base);
			goto errout;
		}
		info->shared_mem_requested = true;
		if (request_mem_region(info->phys_lcr_base + info->lcr_offset,128,"synclink") == NULL) {
			printk( "%s(%d):lcr mem addr conflict device %s Addr=%08X\n",
				__FILE__,__LINE__,info->device_name, info->phys_lcr_base + info->lcr_offset);
			goto errout;
		}
		info->lcr_mem_requested = true;

		info->memory_base = ioremap_nocache(info->phys_memory_base,
								0x40000);
		if (!info->memory_base) {
			printk( "%s(%d):Can't map shared memory on device %s MemAddr=%08X\n",
				__FILE__,__LINE__,info->device_name, info->phys_memory_base );
			goto errout;
		}
		
		if ( !mgsl_memory_test(info) ) {
			printk( "%s(%d):Failed shared memory test %s MemAddr=%08X\n",
				__FILE__,__LINE__,info->device_name, info->phys_memory_base );
			goto errout;
		}
		
		info->lcr_base = ioremap_nocache(info->phys_lcr_base,
								PAGE_SIZE);
		if (!info->lcr_base) {
			printk( "%s(%d):Can't map LCR memory on device %s MemAddr=%08X\n",
				__FILE__,__LINE__,info->device_name, info->phys_lcr_base );
			goto errout;
		}
		info->lcr_base += info->lcr_offset;
		
	} else {
		
		
		if (request_dma(info->dma_level,info->device_name) < 0){
			printk( "%s(%d):Can't request DMA channel on device %s DMA=%d\n",
				__FILE__,__LINE__,info->device_name, info->dma_level );
			mgsl_release_resources( info );
			return -ENODEV;
		}
		info->dma_requested = true;

				
		set_dma_mode(info->dma_level,DMA_MODE_CASCADE);
		enable_dma(info->dma_level);
	}
	
	if ( mgsl_allocate_dma_buffers(info) < 0 ) {
		printk( "%s(%d):Can't allocate DMA buffers on device %s DMA=%d\n",
			__FILE__,__LINE__,info->device_name, info->dma_level );
		goto errout;
	}	
	
	return 0;
errout:
	mgsl_release_resources(info);
	return -ENODEV;

}	

static void mgsl_release_resources(struct mgsl_struct *info)
{
	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk( "%s(%d):mgsl_release_resources(%s) entry\n",
			__FILE__,__LINE__,info->device_name );
			
	if ( info->irq_requested ) {
		free_irq(info->irq_level, info);
		info->irq_requested = false;
	}
	if ( info->dma_requested ) {
		disable_dma(info->dma_level);
		free_dma(info->dma_level);
		info->dma_requested = false;
	}
	mgsl_free_dma_buffers(info);
	mgsl_free_intermediate_rxbuffer_memory(info);
     	mgsl_free_intermediate_txbuffer_memory(info);
	
	if ( info->io_addr_requested ) {
		release_region(info->io_base,info->io_addr_size);
		info->io_addr_requested = false;
	}
	if ( info->shared_mem_requested ) {
		release_mem_region(info->phys_memory_base,0x40000);
		info->shared_mem_requested = false;
	}
	if ( info->lcr_mem_requested ) {
		release_mem_region(info->phys_lcr_base + info->lcr_offset,128);
		info->lcr_mem_requested = false;
	}
	if (info->memory_base){
		iounmap(info->memory_base);
		info->memory_base = NULL;
	}
	if (info->lcr_base){
		iounmap(info->lcr_base - info->lcr_offset);
		info->lcr_base = NULL;
	}
	
	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk( "%s(%d):mgsl_release_resources(%s) exit\n",
			__FILE__,__LINE__,info->device_name );
			
}	

static void mgsl_add_device( struct mgsl_struct *info )
{
	info->next_device = NULL;
	info->line = mgsl_device_count;
	sprintf(info->device_name,"ttySL%d",info->line);
	
	if (info->line < MAX_TOTAL_DEVICES) {
		if (maxframe[info->line])
			info->max_frame_size = maxframe[info->line];

		if (txdmabufs[info->line]) {
			info->num_tx_dma_buffers = txdmabufs[info->line];
			if (info->num_tx_dma_buffers < 1)
				info->num_tx_dma_buffers = 1;
		}

		if (txholdbufs[info->line]) {
			info->num_tx_holding_buffers = txholdbufs[info->line];
			if (info->num_tx_holding_buffers < 1)
				info->num_tx_holding_buffers = 1;
			else if (info->num_tx_holding_buffers > MAX_TX_HOLDING_BUFFERS)
				info->num_tx_holding_buffers = MAX_TX_HOLDING_BUFFERS;
		}
	}

	mgsl_device_count++;
	
	if ( !mgsl_device_list )
		mgsl_device_list = info;
	else {	
		struct mgsl_struct *current_dev = mgsl_device_list;
		while( current_dev->next_device )
			current_dev = current_dev->next_device;
		current_dev->next_device = info;
	}
	
	if ( info->max_frame_size < 4096 )
		info->max_frame_size = 4096;
	else if ( info->max_frame_size > 65535 )
		info->max_frame_size = 65535;
	
	if ( info->bus_type == MGSL_BUS_TYPE_PCI ) {
		printk( "SyncLink PCI v%d %s: IO=%04X IRQ=%d Mem=%08X,%08X MaxFrameSize=%u\n",
			info->hw_version + 1, info->device_name, info->io_base, info->irq_level,
			info->phys_memory_base, info->phys_lcr_base,
		     	info->max_frame_size );
	} else {
		printk( "SyncLink ISA %s: IO=%04X IRQ=%d DMA=%d MaxFrameSize=%u\n",
			info->device_name, info->io_base, info->irq_level, info->dma_level,
		     	info->max_frame_size );
	}

#if SYNCLINK_GENERIC_HDLC
	hdlcdev_init(info);
#endif

}	

static const struct tty_port_operations mgsl_port_ops = {
	.carrier_raised = carrier_raised,
	.dtr_rts = dtr_rts,
};


static struct mgsl_struct* mgsl_allocate_device(void)
{
	struct mgsl_struct *info;
	
	info = kzalloc(sizeof(struct mgsl_struct),
		 GFP_KERNEL);
		 
	if (!info) {
		printk("Error can't allocate device instance data\n");
	} else {
		tty_port_init(&info->port);
		info->port.ops = &mgsl_port_ops;
		info->magic = MGSL_MAGIC;
		INIT_WORK(&info->task, mgsl_bh_handler);
		info->max_frame_size = 4096;
		info->port.close_delay = 5*HZ/10;
		info->port.closing_wait = 30*HZ;
		init_waitqueue_head(&info->status_event_wait_q);
		init_waitqueue_head(&info->event_wait_q);
		spin_lock_init(&info->irq_spinlock);
		spin_lock_init(&info->netlock);
		memcpy(&info->params,&default_params,sizeof(MGSL_PARAMS));
		info->idle_mode = HDLC_TXIDLE_FLAGS;		
		info->num_tx_dma_buffers = 1;
		info->num_tx_holding_buffers = 0;
	}
	
	return info;

}	

static const struct tty_operations mgsl_ops = {
	.open = mgsl_open,
	.close = mgsl_close,
	.write = mgsl_write,
	.put_char = mgsl_put_char,
	.flush_chars = mgsl_flush_chars,
	.write_room = mgsl_write_room,
	.chars_in_buffer = mgsl_chars_in_buffer,
	.flush_buffer = mgsl_flush_buffer,
	.ioctl = mgsl_ioctl,
	.throttle = mgsl_throttle,
	.unthrottle = mgsl_unthrottle,
	.send_xchar = mgsl_send_xchar,
	.break_ctl = mgsl_break,
	.wait_until_sent = mgsl_wait_until_sent,
	.set_termios = mgsl_set_termios,
	.stop = mgsl_stop,
	.start = mgsl_start,
	.hangup = mgsl_hangup,
	.tiocmget = tiocmget,
	.tiocmset = tiocmset,
	.get_icount = msgl_get_icount,
	.proc_fops = &mgsl_proc_fops,
};

static int mgsl_init_tty(void)
{
	int rc;

	serial_driver = alloc_tty_driver(128);
	if (!serial_driver)
		return -ENOMEM;
	
	serial_driver->driver_name = "synclink";
	serial_driver->name = "ttySL";
	serial_driver->major = ttymajor;
	serial_driver->minor_start = 64;
	serial_driver->type = TTY_DRIVER_TYPE_SERIAL;
	serial_driver->subtype = SERIAL_TYPE_NORMAL;
	serial_driver->init_termios = tty_std_termios;
	serial_driver->init_termios.c_cflag =
		B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	serial_driver->init_termios.c_ispeed = 9600;
	serial_driver->init_termios.c_ospeed = 9600;
	serial_driver->flags = TTY_DRIVER_REAL_RAW;
	tty_set_operations(serial_driver, &mgsl_ops);
	if ((rc = tty_register_driver(serial_driver)) < 0) {
		printk("%s(%d):Couldn't register serial driver\n",
			__FILE__,__LINE__);
		put_tty_driver(serial_driver);
		serial_driver = NULL;
		return rc;
	}
			
 	printk("%s %s, tty major#%d\n",
		driver_name, driver_version,
		serial_driver->major);
	return 0;
}

static void mgsl_enum_isa_devices(void)
{
	struct mgsl_struct *info;
	int i;
		
	
	
	for (i=0 ;(i < MAX_ISA_DEVICES) && io[i] && irq[i]; i++){
		if ( debug_level >= DEBUG_LEVEL_INFO )
			printk("ISA device specified io=%04X,irq=%d,dma=%d\n",
				io[i], irq[i], dma[i] );
		
		info = mgsl_allocate_device();
		if ( !info ) {
			
			if ( debug_level >= DEBUG_LEVEL_ERROR )
				printk( "can't allocate device instance data.\n");
			continue;
		}
		
		
		info->io_base = (unsigned int)io[i];
		info->irq_level = (unsigned int)irq[i];
		info->irq_level = irq_canonicalize(info->irq_level);
		info->dma_level = (unsigned int)dma[i];
		info->bus_type = MGSL_BUS_TYPE_ISA;
		info->io_addr_size = 16;
		info->irq_flags = 0;
		
		mgsl_add_device( info );
	}
}

static void synclink_cleanup(void)
{
	int rc;
	struct mgsl_struct *info;
	struct mgsl_struct *tmp;

	printk("Unloading %s: %s\n", driver_name, driver_version);

	if (serial_driver) {
		if ((rc = tty_unregister_driver(serial_driver)))
			printk("%s(%d) failed to unregister tty driver err=%d\n",
			       __FILE__,__LINE__,rc);
		put_tty_driver(serial_driver);
	}

	info = mgsl_device_list;
	while(info) {
#if SYNCLINK_GENERIC_HDLC
		hdlcdev_exit(info);
#endif
		mgsl_release_resources(info);
		tmp = info;
		info = info->next_device;
		kfree(tmp);
	}
	
	if (pci_registered)
		pci_unregister_driver(&synclink_pci_driver);
}

static int __init synclink_init(void)
{
	int rc;

	if (break_on_load) {
	 	mgsl_get_text_ptr();
  		BREAKPOINT();
	}

 	printk("%s %s\n", driver_name, driver_version);

	mgsl_enum_isa_devices();
	if ((rc = pci_register_driver(&synclink_pci_driver)) < 0)
		printk("%s:failed to register PCI driver, error=%d\n",__FILE__,rc);
	else
		pci_registered = true;

	if ((rc = mgsl_init_tty()) < 0)
		goto error;

	return 0;

error:
	synclink_cleanup();
	return rc;
}

static void __exit synclink_exit(void)
{
	synclink_cleanup();
}

module_init(synclink_init);
module_exit(synclink_exit);

/*
 * usc_RTCmd()
 *
 * Issue a USC Receive/Transmit command to the
 * Channel Command/Address Register (CCAR).
 *
 * Notes:
 *
 *    The command is encoded in the most significant 5 bits <15..11>
 *    of the CCAR value. Bits <10..7> of the CCAR must be preserved
 *    and Bits <6..0> must be written as zeros.
 *
 * Arguments:
 *
 *    info   pointer to device information structure
 *    Cmd    command mask (use symbolic macros)
 *
 * Return Value:
 *
 *    None
 */
static void usc_RTCmd( struct mgsl_struct *info, u16 Cmd )
{
	
	

	outw( Cmd + info->loopback_bits, info->io_base + CCAR );

	
	if ( info->bus_type == MGSL_BUS_TYPE_PCI )
		inw( info->io_base + CCAR );

}	

static void usc_DmaCmd( struct mgsl_struct *info, u16 Cmd )
{
	
	outw( Cmd + info->mbre_bit, info->io_base );

	
	if ( info->bus_type == MGSL_BUS_TYPE_PCI )
		inw( info->io_base );

}	

static void usc_OutDmaReg( struct mgsl_struct *info, u16 RegAddr, u16 RegValue )
{
	
	

	outw( RegAddr + info->mbre_bit, info->io_base );
	outw( RegValue, info->io_base );

	
	if ( info->bus_type == MGSL_BUS_TYPE_PCI )
		inw( info->io_base );

}	
 
static u16 usc_InDmaReg( struct mgsl_struct *info, u16 RegAddr )
{
	
	

	outw( RegAddr + info->mbre_bit, info->io_base );
	return inw( info->io_base );

}	

static void usc_OutReg( struct mgsl_struct *info, u16 RegAddr, u16 RegValue )
{
	outw( RegAddr + info->loopback_bits, info->io_base + CCAR );
	outw( RegValue, info->io_base + CCAR );

	
	if ( info->bus_type == MGSL_BUS_TYPE_PCI )
		inw( info->io_base + CCAR );

}	

static u16 usc_InReg( struct mgsl_struct *info, u16 RegAddr )
{
	outw( RegAddr + info->loopback_bits, info->io_base + CCAR );
	return inw( info->io_base + CCAR );

}	

static void usc_set_sdlc_mode( struct mgsl_struct *info )
{
	u16 RegValue;
	bool PreSL1660;
	
	usc_OutReg(info,TMCR,0x1f);
	RegValue=usc_InReg(info,TMDR);
	PreSL1660 = (RegValue == IUSC_PRE_SL1660);

 	if ( info->params.flags & HDLC_FLAG_HDLC_LOOPMODE )
 	{
 	   RegValue = 0x8e06;
 
 	}
 	else
 	{	
		if (info->params.mode == MGSL_MODE_RAW) {
			RegValue = 0x0001;		

			usc_OutReg( info, IOCR,		
				(unsigned short)((usc_InReg(info, IOCR) & ~(BIT13|BIT12)) | BIT12));

			RegValue |= 0x0400;
		}
		else {

		RegValue = 0x0606;

		if ( info->params.flags & HDLC_FLAG_UNDERRUN_ABORT15 )
			RegValue |= BIT14;
		else if ( info->params.flags & HDLC_FLAG_UNDERRUN_FLAG )
			RegValue |= BIT15;
		else if ( info->params.flags & HDLC_FLAG_UNDERRUN_CRC )
			RegValue |= BIT15 + BIT14;
		}

		if ( info->params.preamble != HDLC_PREAMBLE_PATTERN_NONE )
			RegValue |= BIT13;
	}

	if ( info->params.mode == MGSL_MODE_HDLC &&
		(info->params.flags & HDLC_FLAG_SHARE_ZERO) )
		RegValue |= BIT12;

	if ( info->params.addr_filter != 0xff )
	{
		
		usc_OutReg( info, RSR, info->params.addr_filter );
		RegValue |= BIT4;
	}

	usc_OutReg( info, CMR, RegValue );
	info->cmr_value = RegValue;


	RegValue = 0x0500;

	switch ( info->params.encoding ) {
	case HDLC_ENCODING_NRZB:               RegValue |= BIT13; break;
	case HDLC_ENCODING_NRZI_MARK:          RegValue |= BIT14; break;
	case HDLC_ENCODING_NRZI_SPACE:	       RegValue |= BIT14 + BIT13; break;
	case HDLC_ENCODING_BIPHASE_MARK:       RegValue |= BIT15; break;
	case HDLC_ENCODING_BIPHASE_SPACE:      RegValue |= BIT15 + BIT13; break;
	case HDLC_ENCODING_BIPHASE_LEVEL:      RegValue |= BIT15 + BIT14; break;
	case HDLC_ENCODING_DIFF_BIPHASE_LEVEL: RegValue |= BIT15 + BIT14 + BIT13; break;
	}

	if ( (info->params.crc_type & HDLC_CRC_MASK) == HDLC_CRC_16_CCITT )
		RegValue |= BIT9;
	else if ( (info->params.crc_type & HDLC_CRC_MASK) == HDLC_CRC_32_CCITT )
		RegValue |= ( BIT12 | BIT10 | BIT9 );

	usc_OutReg( info, RMR, RegValue );

	
	
	
	
	
	

	usc_OutReg( info, RCLR, RCLRVALUE );

	usc_RCmd( info, RCmd_SelectRicrdma_level );


	
	

	RegValue = usc_InReg( info, RICR ) & 0xc0;

	if ( info->bus_type == MGSL_BUS_TYPE_PCI )
		usc_OutReg( info, RICR, (u16)(0x030a | RegValue) );
	else
		usc_OutReg( info, RICR, (u16)(0x140a | RegValue) );

	

	usc_UnlatchRxstatusBits( info, RXSTATUS_ALL );
	usc_ClearIrqPendingBits( info, RECEIVE_STATUS );


	RegValue = 0x0400;

	switch ( info->params.encoding ) {
	case HDLC_ENCODING_NRZB:               RegValue |= BIT13; break;
	case HDLC_ENCODING_NRZI_MARK:          RegValue |= BIT14; break;
	case HDLC_ENCODING_NRZI_SPACE:         RegValue |= BIT14 + BIT13; break;
	case HDLC_ENCODING_BIPHASE_MARK:       RegValue |= BIT15; break;
	case HDLC_ENCODING_BIPHASE_SPACE:      RegValue |= BIT15 + BIT13; break;
	case HDLC_ENCODING_BIPHASE_LEVEL:      RegValue |= BIT15 + BIT14; break;
	case HDLC_ENCODING_DIFF_BIPHASE_LEVEL: RegValue |= BIT15 + BIT14 + BIT13; break;
	}

	if ( (info->params.crc_type & HDLC_CRC_MASK) == HDLC_CRC_16_CCITT )
		RegValue |= BIT9 + BIT8;
	else if ( (info->params.crc_type & HDLC_CRC_MASK) == HDLC_CRC_32_CCITT )
		RegValue |= ( BIT12 | BIT10 | BIT9 | BIT8);

	usc_OutReg( info, TMR, RegValue );

	usc_set_txidle( info );


	usc_TCmd( info, TCmd_SelectTicrdma_level );


	if ( info->bus_type == MGSL_BUS_TYPE_PCI )
		usc_OutReg( info, TICR, 0x0736 );
	else								
		usc_OutReg( info, TICR, 0x1436 );

	usc_UnlatchTxstatusBits( info, TXSTATUS_ALL );
	usc_ClearIrqPendingBits( info, TRANSMIT_STATUS );

	info->tcsr_value = 0;

	if ( !PreSL1660 )
		info->tcsr_value |= TCSR_UNDERWAIT;
		
	usc_OutReg( info, TCSR, info->tcsr_value );


	RegValue = 0x0f40;

	if ( info->params.flags & HDLC_FLAG_RXC_DPLL )
		RegValue |= 0x0003;	
	else if ( info->params.flags & HDLC_FLAG_RXC_BRG )
		RegValue |= 0x0004;	
 	else if ( info->params.flags & HDLC_FLAG_RXC_TXCPIN)
 		RegValue |= 0x0006;	
	else
		RegValue |= 0x0007;	

	if ( info->params.flags & HDLC_FLAG_TXC_DPLL )
		RegValue |= 0x0018;	
	else if ( info->params.flags & HDLC_FLAG_TXC_BRG )
		RegValue |= 0x0020;	
 	else if ( info->params.flags & HDLC_FLAG_TXC_RXCPIN)
 		RegValue |= 0x0038;	
	else
		RegValue |= 0x0030;	

	usc_OutReg( info, CMCR, RegValue );



	RegValue = 0x0000;

	if ( info->params.flags & (HDLC_FLAG_RXC_DPLL + HDLC_FLAG_TXC_DPLL) ) {
		u32 XtalSpeed;
		u32 DpllDivisor;
		u16 Tc;

		
		

		if ( info->bus_type == MGSL_BUS_TYPE_PCI )
			XtalSpeed = 11059200;
		else
			XtalSpeed = 14745600;

		if ( info->params.flags & HDLC_FLAG_DPLL_DIV16 ) {
			DpllDivisor = 16;
			RegValue |= BIT10;
		}
		else if ( info->params.flags & HDLC_FLAG_DPLL_DIV8 ) {
			DpllDivisor = 8;
			RegValue |= BIT11;
		}
		else
			DpllDivisor = 32;

		
		
		
		
		

 		if ( info->params.clock_speed )
 		{
			Tc = (u16)((XtalSpeed/DpllDivisor)/info->params.clock_speed);
			if ( !((((XtalSpeed/DpllDivisor) % info->params.clock_speed) * 2)
			       / info->params.clock_speed) )
				Tc--;
 		}
 		else
 			Tc = -1;
 				  

		
		usc_OutReg( info, TC1R, Tc );

		RegValue |= BIT4;		

		switch ( info->params.encoding ) {
		case HDLC_ENCODING_NRZ:
		case HDLC_ENCODING_NRZB:
		case HDLC_ENCODING_NRZI_MARK:
		case HDLC_ENCODING_NRZI_SPACE: RegValue |= BIT8; break;
		case HDLC_ENCODING_BIPHASE_MARK:
		case HDLC_ENCODING_BIPHASE_SPACE: RegValue |= BIT9; break;
		case HDLC_ENCODING_BIPHASE_LEVEL:
		case HDLC_ENCODING_DIFF_BIPHASE_LEVEL: RegValue |= BIT9 + BIT8; break;
		}
	}

	usc_OutReg( info, HCR, RegValue );



	usc_OutReg( info, CCSR, 0x1020 );


	if ( info->params.flags & HDLC_FLAG_AUTO_CTS ) {
		usc_OutReg( info, SICR,
			    (u16)(usc_InReg(info,SICR) | SICR_CTS_INACTIVE) );
	}
	

	
	usc_EnableMasterIrqBit( info );

	usc_ClearIrqPendingBits( info, RECEIVE_STATUS + RECEIVE_DATA +
				TRANSMIT_STATUS + TRANSMIT_DATA + MISC);

	
	usc_OutReg(info, SICR, (u16)(usc_InReg(info,SICR) | BIT3));
	usc_EnableInterrupts(info, MISC);

	info->mbre_bit = 0;
	outw( 0, info->io_base ); 			
	usc_DmaCmd( info, DmaCmd_ResetAllChannels );	
	info->mbre_bit = BIT8;
	outw( BIT8, info->io_base );			

	if (info->bus_type == MGSL_BUS_TYPE_ISA) {
		
		
		usc_OutReg(info, PCR, (u16)((usc_InReg(info, PCR) | BIT15) & ~BIT14));
	}


	if ( info->bus_type == MGSL_BUS_TYPE_PCI ) {
		
		usc_OutDmaReg( info, DCR, 0xa00b );
	}
	else
		usc_OutDmaReg( info, DCR, 0x800b );



	usc_OutDmaReg( info, RDMR, 0xf200 );



	usc_OutDmaReg( info, TDMR, 0xf200 );



	usc_OutDmaReg( info, DICR, 0x9000 );

	usc_InDmaReg( info, RDMR );		
	usc_InDmaReg( info, TDMR );		
	usc_OutDmaReg( info, CDIR, 0x0303 );	


	RegValue = 0x8080;

	switch ( info->params.preamble_length ) {
	case HDLC_PREAMBLE_LENGTH_16BITS: RegValue |= BIT10; break;
	case HDLC_PREAMBLE_LENGTH_32BITS: RegValue |= BIT11; break;
	case HDLC_PREAMBLE_LENGTH_64BITS: RegValue |= BIT11 + BIT10; break;
	}

	switch ( info->params.preamble ) {
	case HDLC_PREAMBLE_PATTERN_FLAGS: RegValue |= BIT8 + BIT12; break;
	case HDLC_PREAMBLE_PATTERN_ONES:  RegValue |= BIT8; break;
	case HDLC_PREAMBLE_PATTERN_10:    RegValue |= BIT9; break;
	case HDLC_PREAMBLE_PATTERN_01:    RegValue |= BIT9 + BIT8; break;
	}

	usc_OutReg( info, CCR, RegValue );



	if ( info->bus_type == MGSL_BUS_TYPE_PCI ) {
		
		usc_OutDmaReg( info, BDCR, 0x0000 );
	}
	else
		usc_OutDmaReg( info, BDCR, 0x2000 );

	usc_stop_transmitter(info);
	usc_stop_receiver(info);
	
}	

static void usc_enable_loopback(struct mgsl_struct *info, int enable)
{
	if (enable) {
		
		usc_OutReg(info,IOCR,usc_InReg(info,IOCR) | (BIT7+BIT6));
	

		usc_OutReg( info, CMCR, 0x0f64 );

		
		
		if (info->params.clock_speed) {
			if (info->bus_type == MGSL_BUS_TYPE_PCI)
				usc_OutReg(info, TC0R, (u16)((11059200/info->params.clock_speed)-1));
			else
				usc_OutReg(info, TC0R, (u16)((14745600/info->params.clock_speed)-1));
		} else
			usc_OutReg(info, TC0R, (u16)8);

		usc_OutReg( info, HCR, (u16)((usc_InReg( info, HCR ) & ~BIT1) | BIT0) );

		
		usc_OutReg(info, IOCR, (u16)((usc_InReg(info, IOCR) & 0xfff8) | 0x0004));

		
		info->loopback_bits = 0x300;
		outw( 0x0300, info->io_base + CCAR );
	} else {
		
		usc_OutReg(info,IOCR,usc_InReg(info,IOCR) & ~(BIT7+BIT6));
	
		
		info->loopback_bits = 0;
		outw( 0,info->io_base + CCAR );
	}
	
}	

static void usc_enable_aux_clock( struct mgsl_struct *info, u32 data_rate )
{
	u32 XtalSpeed;
	u16 Tc;

	if ( data_rate ) {
		if ( info->bus_type == MGSL_BUS_TYPE_PCI )
			XtalSpeed = 11059200;
		else
			XtalSpeed = 14745600;


		
		
		
		
		


		Tc = (u16)(XtalSpeed/data_rate);
		if ( !(((XtalSpeed % data_rate) * 2) / data_rate) )
			Tc--;

		
		usc_OutReg( info, TC0R, Tc );


		usc_OutReg( info, HCR, (u16)((usc_InReg( info, HCR ) & ~BIT1) | BIT0) );

		
		usc_OutReg( info, IOCR, (u16)((usc_InReg(info, IOCR) & 0xfff8) | 0x0004) );
	} else {
		
		usc_OutReg( info, HCR, (u16)(usc_InReg( info, HCR ) & ~BIT0) );
	}

}	

static void usc_process_rxoverrun_sync( struct mgsl_struct *info )
{
	int start_index;
	int end_index;
	int frame_start_index;
	bool start_of_frame_found = false;
	bool end_of_frame_found = false;
	bool reprogram_dma = false;

	DMABUFFERENTRY *buffer_list = info->rx_buffer_list;
	u32 phys_addr;

	usc_DmaCmd( info, DmaCmd_PauseRxChannel );
	usc_RCmd( info, RCmd_EnterHuntmode );
	usc_RTCmd( info, RTCmd_PurgeRxFifo );

	
	
	
	frame_start_index = start_index = end_index = info->current_rx_buffer;

	
	
	
	

	while( !buffer_list[end_index].count )
	{
		
		

		if ( !start_of_frame_found )
		{
			start_of_frame_found = true;
			frame_start_index = end_index;
			end_of_frame_found = false;
		}

		if ( buffer_list[end_index].status )
		{
			
			

			
			

			start_of_frame_found = false;
			end_of_frame_found = true;
		}

  		
  		end_index++;
  		if ( end_index == info->rx_buffer_count )
  			end_index = 0;

		if ( start_index == end_index )
		{
			
			
			
			mgsl_reset_rx_dma_buffers( info );
			frame_start_index = 0;
			start_of_frame_found = false;
			reprogram_dma = true;
			break;
		}
	}

	if ( start_of_frame_found && !end_of_frame_found )
	{
		
		

		
		
		

		start_index = frame_start_index;

		do
		{
			*((unsigned long *)&(info->rx_buffer_list[start_index++].count)) = DMABUFFERSIZE;

  			
  			if ( start_index == info->rx_buffer_count )
  				start_index = 0;

		} while( start_index != end_index );

		reprogram_dma = true;
	}

	if ( reprogram_dma )
	{
		usc_UnlatchRxstatusBits(info,RXSTATUS_ALL);
		usc_ClearIrqPendingBits(info, RECEIVE_DATA|RECEIVE_STATUS);
		usc_UnlatchRxstatusBits(info, RECEIVE_DATA|RECEIVE_STATUS);
		
		usc_EnableReceiver(info,DISABLE_UNCONDITIONAL);
		
		
		usc_OutReg( info, CCSR, (u16)(usc_InReg(info,CCSR) | BIT13) );

		
		phys_addr = info->rx_buffer_list[frame_start_index].phys_entry;
		usc_OutDmaReg( info, NRARL, (u16)phys_addr );
		usc_OutDmaReg( info, NRARU, (u16)(phys_addr >> 16) );

		usc_UnlatchRxstatusBits( info, RXSTATUS_ALL );
		usc_ClearIrqPendingBits( info, RECEIVE_DATA + RECEIVE_STATUS );
		usc_EnableInterrupts( info, RECEIVE_STATUS );

		
		

		usc_OutDmaReg( info, RDIAR, BIT3 + BIT2 );
		usc_OutDmaReg( info, DICR, (u16)(usc_InDmaReg(info,DICR) | BIT1) );
		usc_DmaCmd( info, DmaCmd_InitRxChannel );
		if ( info->params.flags & HDLC_FLAG_AUTO_DCD )
			usc_EnableReceiver(info,ENABLE_AUTO_DCD);
		else
			usc_EnableReceiver(info,ENABLE_UNCONDITIONAL);
	}
	else
	{
		
		usc_OutReg( info, CCSR, (u16)(usc_InReg(info,CCSR) | BIT13) );
		usc_RTCmd( info, RTCmd_PurgeRxFifo );
	}

}	

static void usc_stop_receiver( struct mgsl_struct *info )
{
	if (debug_level >= DEBUG_LEVEL_ISR)
		printk("%s(%d):usc_stop_receiver(%s)\n",
			 __FILE__,__LINE__, info->device_name );
			 
	
	
	usc_DmaCmd( info, DmaCmd_ResetRxChannel );

	usc_UnlatchRxstatusBits( info, RXSTATUS_ALL );
	usc_ClearIrqPendingBits( info, RECEIVE_DATA + RECEIVE_STATUS );
	usc_DisableInterrupts( info, RECEIVE_DATA + RECEIVE_STATUS );

	usc_EnableReceiver(info,DISABLE_UNCONDITIONAL);

	
	usc_OutReg( info, CCSR, (u16)(usc_InReg(info,CCSR) | BIT13) );
	usc_RTCmd( info, RTCmd_PurgeRxFifo );

	info->rx_enabled = false;
	info->rx_overflow = false;
	info->rx_rcc_underrun = false;
	
}	

static void usc_start_receiver( struct mgsl_struct *info )
{
	u32 phys_addr;
	
	if (debug_level >= DEBUG_LEVEL_ISR)
		printk("%s(%d):usc_start_receiver(%s)\n",
			 __FILE__,__LINE__, info->device_name );

	mgsl_reset_rx_dma_buffers( info );
	usc_stop_receiver( info );

	usc_OutReg( info, CCSR, (u16)(usc_InReg(info,CCSR) | BIT13) );
	usc_RTCmd( info, RTCmd_PurgeRxFifo );

	if ( info->params.mode == MGSL_MODE_HDLC ||
		info->params.mode == MGSL_MODE_RAW ) {
		
		
		

		
		phys_addr = info->rx_buffer_list[0].phys_entry;
		usc_OutDmaReg( info, NRARL, (u16)phys_addr );
		usc_OutDmaReg( info, NRARU, (u16)(phys_addr >> 16) );

		usc_UnlatchRxstatusBits( info, RXSTATUS_ALL );
		usc_ClearIrqPendingBits( info, RECEIVE_DATA + RECEIVE_STATUS );
		usc_EnableInterrupts( info, RECEIVE_STATUS );

		
		

		usc_OutDmaReg( info, RDIAR, BIT3 + BIT2 );
		usc_OutDmaReg( info, DICR, (u16)(usc_InDmaReg(info,DICR) | BIT1) );
		usc_DmaCmd( info, DmaCmd_InitRxChannel );
		if ( info->params.flags & HDLC_FLAG_AUTO_DCD )
			usc_EnableReceiver(info,ENABLE_AUTO_DCD);
		else
			usc_EnableReceiver(info,ENABLE_UNCONDITIONAL);
	} else {
		usc_UnlatchRxstatusBits(info, RXSTATUS_ALL);
		usc_ClearIrqPendingBits(info, RECEIVE_DATA + RECEIVE_STATUS);
		usc_EnableInterrupts(info, RECEIVE_DATA);

		usc_RTCmd( info, RTCmd_PurgeRxFifo );
		usc_RCmd( info, RCmd_EnterHuntmode );

		usc_EnableReceiver(info,ENABLE_UNCONDITIONAL);
	}

	usc_OutReg( info, CCSR, 0x1020 );

	info->rx_enabled = true;

}	

static void usc_start_transmitter( struct mgsl_struct *info )
{
	u32 phys_addr;
	unsigned int FrameSize;

	if (debug_level >= DEBUG_LEVEL_ISR)
		printk("%s(%d):usc_start_transmitter(%s)\n",
			 __FILE__,__LINE__, info->device_name );
			 
	if ( info->xmit_cnt ) {

		
		
		

		info->drop_rts_on_tx_done = false;

		if ( info->params.flags & HDLC_FLAG_AUTO_RTS ) {
			usc_get_serial_signals( info );
			if ( !(info->serial_signals & SerialSignal_RTS) ) {
				info->serial_signals |= SerialSignal_RTS;
				usc_set_serial_signals( info );
				info->drop_rts_on_tx_done = true;
			}
		}


		if ( info->params.mode == MGSL_MODE_ASYNC ) {
			if ( !info->tx_active ) {
				usc_UnlatchTxstatusBits(info, TXSTATUS_ALL);
				usc_ClearIrqPendingBits(info, TRANSMIT_STATUS + TRANSMIT_DATA);
				usc_EnableInterrupts(info, TRANSMIT_DATA);
				usc_load_txfifo(info);
			}
		} else {
			
			usc_DmaCmd( info, DmaCmd_ResetTxChannel );
			
			
			

			FrameSize = info->tx_buffer_list[info->start_tx_dma_buffer].rcc;

	    		if ( info->params.mode == MGSL_MODE_RAW )
				info->tx_buffer_list[info->start_tx_dma_buffer].rcc = 0;

			
			
			usc_OutReg( info, TCLR, (u16)FrameSize );

			usc_RTCmd( info, RTCmd_PurgeTxFifo );

			
			phys_addr = info->tx_buffer_list[info->start_tx_dma_buffer].phys_entry;
			usc_OutDmaReg( info, NTARL, (u16)phys_addr );
			usc_OutDmaReg( info, NTARU, (u16)(phys_addr >> 16) );

			usc_UnlatchTxstatusBits( info, TXSTATUS_ALL );
			usc_ClearIrqPendingBits( info, TRANSMIT_STATUS );
			usc_EnableInterrupts( info, TRANSMIT_STATUS );

			if ( info->params.mode == MGSL_MODE_RAW &&
					info->num_tx_dma_buffers > 1 ) {
			   
			   
			   
			   
			   
			   

			   usc_OutDmaReg( info, TDIAR, BIT2|BIT3 );
			   usc_OutDmaReg( info, DICR, (u16)(usc_InDmaReg(info,DICR) | BIT0) );
			}

			
			usc_DmaCmd( info, DmaCmd_InitTxChannel );
			
			usc_TCmd( info, TCmd_SendFrame );
			
			mod_timer(&info->tx_timer, jiffies +
					msecs_to_jiffies(5000));
		}
		info->tx_active = true;
	}

	if ( !info->tx_enabled ) {
		info->tx_enabled = true;
		if ( info->params.flags & HDLC_FLAG_AUTO_CTS )
			usc_EnableTransmitter(info,ENABLE_AUTO_CTS);
		else
			usc_EnableTransmitter(info,ENABLE_UNCONDITIONAL);
	}

}	

static void usc_stop_transmitter( struct mgsl_struct *info )
{
	if (debug_level >= DEBUG_LEVEL_ISR)
		printk("%s(%d):usc_stop_transmitter(%s)\n",
			 __FILE__,__LINE__, info->device_name );
			 
	del_timer(&info->tx_timer);	
			 
	usc_UnlatchTxstatusBits( info, TXSTATUS_ALL );
	usc_ClearIrqPendingBits( info, TRANSMIT_STATUS + TRANSMIT_DATA );
	usc_DisableInterrupts( info, TRANSMIT_STATUS + TRANSMIT_DATA );

	usc_EnableTransmitter(info,DISABLE_UNCONDITIONAL);
	usc_DmaCmd( info, DmaCmd_ResetTxChannel );
	usc_RTCmd( info, RTCmd_PurgeTxFifo );

	info->tx_enabled = false;
	info->tx_active = false;

}	

static void usc_load_txfifo( struct mgsl_struct *info )
{
	int Fifocount;
	u8 TwoBytes[2];
	
	if ( !info->xmit_cnt && !info->x_char )
		return; 
		
	
	usc_TCmd( info, TCmd_SelectTicrTxFifostatus );

	

	while( (Fifocount = usc_InReg(info, TICR) >> 8) && info->xmit_cnt ) {
		
		

		if ( (info->xmit_cnt > 1) && (Fifocount > 1) && !info->x_char ) {
 			
				
			TwoBytes[0] = info->xmit_buf[info->xmit_tail++];
			info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
			TwoBytes[1] = info->xmit_buf[info->xmit_tail++];
			info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
			
			outw( *((u16 *)TwoBytes), info->io_base + DATAREG);
				
			info->xmit_cnt -= 2;
			info->icount.tx += 2;
		} else {
			
			
			outw( (inw( info->io_base + CCAR) & 0x0780) | (TDR+LSBONLY),
				info->io_base + CCAR );
			
			if (info->x_char) {
				
				outw( info->x_char,info->io_base + CCAR );
				info->x_char = 0;
			} else {
				outw( info->xmit_buf[info->xmit_tail++],info->io_base + CCAR );
				info->xmit_tail = info->xmit_tail & (SERIAL_XMIT_SIZE-1);
				info->xmit_cnt--;
			}
			info->icount.tx++;
		}
	}

}	

static void usc_reset( struct mgsl_struct *info )
{
	if ( info->bus_type == MGSL_BUS_TYPE_PCI ) {
		int i;
		u32 readval;

		
		

		volatile u32 *MiscCtrl = (u32 *)(info->lcr_base + 0x50);
		u32 *LCR0BRDR = (u32 *)(info->lcr_base + 0x28);

		info->misc_ctrl_value |= BIT30;
		*MiscCtrl = info->misc_ctrl_value;

		for(i=0;i<10;i++)
			readval = *MiscCtrl;

		info->misc_ctrl_value &= ~BIT30;
		*MiscCtrl = info->misc_ctrl_value;

		*LCR0BRDR = BUS_DESCRIPTOR(
			1,		
			2,		
			2,		
			0,		
			4,		
			0,		
			0,		
			5		
			);
	} else {
		
		outb( 0,info->io_base + 8 );
	}

	info->mbre_bit = 0;
	info->loopback_bits = 0;
	info->usc_idle_mode = 0;

	
	outw( 0x000c,info->io_base + SDPIN );


	outw( 0,info->io_base );
	outw( 0,info->io_base + CCAR );

	
	usc_RTCmd( info, RTCmd_SelectLittleEndian );



	usc_OutReg( info, PCR, 0xf0f5 );



	usc_OutReg( info, IOCR, 0x0004 );

}	

static void usc_set_async_mode( struct mgsl_struct *info )
{
	u16 RegValue;

	
	usc_DisableMasterIrqBit( info );

	outw( 0, info->io_base ); 			
	usc_DmaCmd( info, DmaCmd_ResetAllChannels );	

	usc_loopback_frame( info );


	RegValue = 0;
	if ( info->params.stop_bits != 1 )
		RegValue |= BIT14;
	usc_OutReg( info, CMR, RegValue );

	

	RegValue = 0;

	if ( info->params.data_bits != 8 )
		RegValue |= BIT4+BIT3+BIT2;

	if ( info->params.parity != ASYNC_PARITY_NONE ) {
		RegValue |= BIT5;
		if ( info->params.parity != ASYNC_PARITY_ODD )
			RegValue |= BIT6;
	}

	usc_OutReg( info, RMR, RegValue );


	

	usc_RCmd( info, RCmd_SelectRicrIntLevel );

	
	
	usc_OutReg( info, RICR, 0x0000 );

	usc_UnlatchRxstatusBits( info, RXSTATUS_ALL );
	usc_ClearIrqPendingBits( info, RECEIVE_STATUS );

	

	RegValue = 0;

	if ( info->params.data_bits != 8 )
		RegValue |= BIT4+BIT3+BIT2;

	if ( info->params.parity != ASYNC_PARITY_NONE ) {
		RegValue |= BIT5;
		if ( info->params.parity != ASYNC_PARITY_ODD )
			RegValue |= BIT6;
	}

	usc_OutReg( info, TMR, RegValue );

	usc_set_txidle( info );


	

	usc_TCmd( info, TCmd_SelectTicrIntLevel );

	

	usc_OutReg( info, TICR, 0x1f40 );

	usc_UnlatchTxstatusBits( info, TXSTATUS_ALL );
	usc_ClearIrqPendingBits( info, TRANSMIT_STATUS );

	usc_enable_async_clock( info, info->params.data_rate );

	
	
	usc_OutReg( info, CCSR, 0x0020 );

	usc_DisableInterrupts( info, TRANSMIT_STATUS + TRANSMIT_DATA +
			      RECEIVE_DATA + RECEIVE_STATUS );

	usc_ClearIrqPendingBits( info, TRANSMIT_STATUS + TRANSMIT_DATA +
				RECEIVE_DATA + RECEIVE_STATUS );

	usc_EnableMasterIrqBit( info );

	if (info->bus_type == MGSL_BUS_TYPE_ISA) {
		
		
		usc_OutReg(info, PCR, (u16)((usc_InReg(info, PCR) | BIT13) & ~BIT12));
	}

	if (info->params.loopback) {
		info->loopback_bits = 0x300;
		outw(0x0300, info->io_base + CCAR);
	}

}	

static void usc_loopback_frame( struct mgsl_struct *info )
{
	int i;
	unsigned long oldmode = info->params.mode;

	info->params.mode = MGSL_MODE_HDLC;
	
	usc_DisableMasterIrqBit( info );

	usc_set_sdlc_mode( info );
	usc_enable_loopback( info, 1 );

	
	usc_OutReg( info, TC0R, 0 );
	

	usc_OutReg( info, CCR, 0x0100 );

	
	usc_RTCmd( info, RTCmd_PurgeRxFifo );
	usc_EnableReceiver(info,ENABLE_UNCONDITIONAL);

	
	
	
	usc_OutReg( info, TCLR, 2 );
	usc_RTCmd( info, RTCmd_PurgeTxFifo );

	
	usc_UnlatchTxstatusBits(info,TXSTATUS_ALL);
	outw(0,info->io_base + DATAREG);

	
	usc_TCmd( info, TCmd_SendFrame );
	usc_EnableTransmitter(info,ENABLE_UNCONDITIONAL);
							
	
	for (i=0 ; i<1000 ; i++)
		if (usc_InReg( info, RCSR ) & (BIT8 + BIT4 + BIT3 + BIT1))
			break;

	
	usc_enable_loopback(info, 0);

	usc_EnableMasterIrqBit(info);

	info->params.mode = oldmode;

}	

static void usc_set_sync_mode( struct mgsl_struct *info )
{
	usc_loopback_frame( info );
	usc_set_sdlc_mode( info );

	if (info->bus_type == MGSL_BUS_TYPE_ISA) {
		
		
		usc_OutReg(info, PCR, (u16)((usc_InReg(info, PCR) | BIT13) & ~BIT12));
	}

	usc_enable_aux_clock(info, info->params.clock_speed);

	if (info->params.loopback)
		usc_enable_loopback(info,1);

}	

static void usc_set_txidle( struct mgsl_struct *info )
{
	u16 usc_idle_mode = IDLEMODE_FLAGS;

	

	switch( info->idle_mode ){
	case HDLC_TXIDLE_FLAGS:			usc_idle_mode = IDLEMODE_FLAGS; break;
	case HDLC_TXIDLE_ALT_ZEROS_ONES:	usc_idle_mode = IDLEMODE_ALT_ONE_ZERO; break;
	case HDLC_TXIDLE_ZEROS:			usc_idle_mode = IDLEMODE_ZERO; break;
	case HDLC_TXIDLE_ONES:			usc_idle_mode = IDLEMODE_ONE; break;
	case HDLC_TXIDLE_ALT_MARK_SPACE:	usc_idle_mode = IDLEMODE_ALT_MARK_SPACE; break;
	case HDLC_TXIDLE_SPACE:			usc_idle_mode = IDLEMODE_SPACE; break;
	case HDLC_TXIDLE_MARK:			usc_idle_mode = IDLEMODE_MARK; break;
	}

	info->usc_idle_mode = usc_idle_mode;
	
	info->tcsr_value &= ~IDLEMODE_MASK;	
	info->tcsr_value += usc_idle_mode;
	usc_OutReg(info, TCSR, info->tcsr_value);

	if ( info->params.mode == MGSL_MODE_RAW ) {
		unsigned char syncpat = 0;
		switch( info->idle_mode ) {
		case HDLC_TXIDLE_FLAGS:
			syncpat = 0x7e;
			break;
		case HDLC_TXIDLE_ALT_ZEROS_ONES:
			syncpat = 0x55;
			break;
		case HDLC_TXIDLE_ZEROS:
		case HDLC_TXIDLE_SPACE:
			syncpat = 0x00;
			break;
		case HDLC_TXIDLE_ONES:
		case HDLC_TXIDLE_MARK:
			syncpat = 0xff;
			break;
		case HDLC_TXIDLE_ALT_MARK_SPACE:
			syncpat = 0xaa;
			break;
		}

		usc_SetTransmitSyncChars(info,syncpat,syncpat);
	}

}	

static void usc_get_serial_signals( struct mgsl_struct *info )
{
	u16 status;

	
	info->serial_signals &= SerialSignal_DTR + SerialSignal_RTS;

	
	

	status = usc_InReg( info, MISR );

	

	if ( status & MISCSTATUS_CTS )
		info->serial_signals |= SerialSignal_CTS;

	if ( status & MISCSTATUS_DCD )
		info->serial_signals |= SerialSignal_DCD;

	if ( status & MISCSTATUS_RI )
		info->serial_signals |= SerialSignal_RI;

	if ( status & MISCSTATUS_DSR )
		info->serial_signals |= SerialSignal_DSR;

}	

static void usc_set_serial_signals( struct mgsl_struct *info )
{
	u16 Control;
	unsigned char V24Out = info->serial_signals;

	

	Control = usc_InReg( info, PCR );

	if ( V24Out & SerialSignal_RTS )
		Control &= ~(BIT6);
	else
		Control |= BIT6;

	if ( V24Out & SerialSignal_DTR )
		Control &= ~(BIT4);
	else
		Control |= BIT4;

	usc_OutReg( info, PCR, Control );

}	

static void usc_enable_async_clock( struct mgsl_struct *info, u32 data_rate )
{
	if ( data_rate )	{
		
		usc_OutReg( info, CMCR, 0x0f64 );



		if ( info->bus_type == MGSL_BUS_TYPE_PCI )
			usc_OutReg( info, TC0R, (u16)((691200/data_rate) - 1) );
		else
			usc_OutReg( info, TC0R, (u16)((921600/data_rate) - 1) );

		

		usc_OutReg( info, HCR,
			    (u16)((usc_InReg( info, HCR ) & ~BIT1) | BIT0) );


		

		usc_OutReg( info, IOCR,
			    (u16)((usc_InReg(info, IOCR) & 0xfff8) | 0x0004) );
	} else {
		
		usc_OutReg( info, HCR, (u16)(usc_InReg( info, HCR ) & ~BIT0) );
	}

}	

/*
 * Buffer Structures:
 *
 * Normal memory access uses virtual addresses that can make discontiguous
 * physical memory pages appear to be contiguous in the virtual address
 * space (the processors memory mapping handles the conversions).
 *
 * DMA transfers require physically contiguous memory. This is because
 * the DMA system controller and DMA bus masters deal with memory using
 * only physical addresses.
 *
 * This causes a problem under Windows NT when large DMA buffers are
 * needed. Fragmentation of the nonpaged pool prevents allocations of
 * physically contiguous buffers larger than the PAGE_SIZE.
 *
 * However the 16C32 supports Bus Master Scatter/Gather DMA which
 * allows DMA transfers to physically discontiguous buffers. Information
 * about each data transfer buffer is contained in a memory structure
 * called a 'buffer entry'. A list of buffer entries is maintained
 * to track and control the use of the data transfer buffers.
 *
 * To support this strategy we will allocate sufficient PAGE_SIZE
 * contiguous memory buffers to allow for the total required buffer
 * space.
 *
 * The 16C32 accesses the list of buffer entries using Bus Master
 * DMA. Control information is read from the buffer entries by the
 * 16C32 to control data transfers. status information is written to
 * the buffer entries by the 16C32 to indicate the status of completed
 * transfers.
 *
 * The CPU writes control information to the buffer entries to control
 * the 16C32 and reads status information from the buffer entries to
 * determine information about received and transmitted frames.
 *
 * Because the CPU and 16C32 (adapter) both need simultaneous access
 * to the buffer entries, the buffer entry memory is allocated with
 * HalAllocateCommonBuffer(). This restricts the size of the buffer
 * entry list to PAGE_SIZE.
 *
 * The actual data buffers on the other hand will only be accessed
 * by the CPU or the adapter but not by both simultaneously. This allows
 * Scatter/Gather packet based DMA procedures for using physically
 * discontiguous pages.
 */

static void mgsl_reset_tx_dma_buffers( struct mgsl_struct *info )
{
	unsigned int i;

	for ( i = 0; i < info->tx_buffer_count; i++ ) {
		*((unsigned long *)&(info->tx_buffer_list[i].count)) = 0;
	}

	info->current_tx_buffer = 0;
	info->start_tx_dma_buffer = 0;
	info->tx_dma_buffers_used = 0;

	info->get_tx_holding_index = 0;
	info->put_tx_holding_index = 0;
	info->tx_holding_count = 0;

}	

static int num_free_tx_dma_buffers(struct mgsl_struct *info)
{
	return info->tx_buffer_count - info->tx_dma_buffers_used;
}

static void mgsl_reset_rx_dma_buffers( struct mgsl_struct *info )
{
	unsigned int i;

	for ( i = 0; i < info->rx_buffer_count; i++ ) {
		*((unsigned long *)&(info->rx_buffer_list[i].count)) = DMABUFFERSIZE;
	}

	info->current_rx_buffer = 0;

}	

static void mgsl_free_rx_frame_buffers( struct mgsl_struct *info, unsigned int StartIndex, unsigned int EndIndex )
{
	bool Done = false;
	DMABUFFERENTRY *pBufEntry;
	unsigned int Index;

	
	

	Index = StartIndex;

	while( !Done ) {
		pBufEntry = &(info->rx_buffer_list[Index]);

		if ( Index == EndIndex ) {
			
			Done = true;
		}

		
		*((unsigned long *)&(pBufEntry->count)) = DMABUFFERSIZE;

		
		Index++;
		if ( Index == info->rx_buffer_count )
			Index = 0;
	}

	
	info->current_rx_buffer = Index;

}	

static bool mgsl_get_rx_frame(struct mgsl_struct *info)
{
	unsigned int StartIndex, EndIndex;	
	unsigned short status;
	DMABUFFERENTRY *pBufEntry;
	unsigned int framesize = 0;
	bool ReturnCode = false;
	unsigned long flags;
	struct tty_struct *tty = info->port.tty;
	bool return_frame = false;
	

	StartIndex = EndIndex = info->current_rx_buffer;

	while( !info->rx_buffer_list[EndIndex].status ) {

		if ( info->rx_buffer_list[EndIndex].count )
			goto Cleanup;

		
		EndIndex++;
		if ( EndIndex == info->rx_buffer_count )
			EndIndex = 0;

		
		if ( EndIndex == StartIndex ) {

			if ( info->rx_enabled ){
				spin_lock_irqsave(&info->irq_spinlock,flags);
				usc_start_receiver(info);
				spin_unlock_irqrestore(&info->irq_spinlock,flags);
			}
			goto Cleanup;
		}
	}


	
	
	status = info->rx_buffer_list[EndIndex].status;

	if ( status & (RXSTATUS_SHORT_FRAME + RXSTATUS_OVERRUN +
			RXSTATUS_CRC_ERROR + RXSTATUS_ABORT) ) {
		if ( status & RXSTATUS_SHORT_FRAME )
			info->icount.rxshort++;
		else if ( status & RXSTATUS_ABORT )
			info->icount.rxabort++;
		else if ( status & RXSTATUS_OVERRUN )
			info->icount.rxover++;
		else {
			info->icount.rxcrc++;
			if ( info->params.crc_type & HDLC_CRC_RETURN_EX )
				return_frame = true;
		}
		framesize = 0;
#if SYNCLINK_GENERIC_HDLC
		{
			info->netdev->stats.rx_errors++;
			info->netdev->stats.rx_frame_errors++;
		}
#endif
	} else
		return_frame = true;

	if ( return_frame ) {

		framesize = RCLRVALUE - info->rx_buffer_list[EndIndex].rcc;

		
		if ( info->params.crc_type == HDLC_CRC_16_CCITT )
			framesize -= 2;
		else if ( info->params.crc_type == HDLC_CRC_32_CCITT )
			framesize -= 4;		
	}

	if ( debug_level >= DEBUG_LEVEL_BH )
		printk("%s(%d):mgsl_get_rx_frame(%s) status=%04X size=%d\n",
			__FILE__,__LINE__,info->device_name,status,framesize);
			
	if ( debug_level >= DEBUG_LEVEL_DATA )
		mgsl_trace_block(info,info->rx_buffer_list[StartIndex].virt_addr,
			min_t(int, framesize, DMABUFFERSIZE),0);
		
	if (framesize) {
		if ( ( (info->params.crc_type & HDLC_CRC_RETURN_EX) &&
				((framesize+1) > info->max_frame_size) ) ||
			(framesize > info->max_frame_size) )
			info->icount.rxlong++;
		else {
			
			int copy_count = framesize;
			int index = StartIndex;
			unsigned char *ptmp = info->intermediate_rxbuffer;

			if ( !(status & RXSTATUS_CRC_ERROR))
			info->icount.rxok++;
			
			while(copy_count) {
				int partial_count;
				if ( copy_count > DMABUFFERSIZE )
					partial_count = DMABUFFERSIZE;
				else
					partial_count = copy_count;
			
				pBufEntry = &(info->rx_buffer_list[index]);
				memcpy( ptmp, pBufEntry->virt_addr, partial_count );
				ptmp += partial_count;
				copy_count -= partial_count;
				
				if ( ++index == info->rx_buffer_count )
					index = 0;
			}

			if ( info->params.crc_type & HDLC_CRC_RETURN_EX ) {
				++framesize;
				*ptmp = (status & RXSTATUS_CRC_ERROR ?
						RX_CRC_ERROR :
						RX_OK);

				if ( debug_level >= DEBUG_LEVEL_DATA )
					printk("%s(%d):mgsl_get_rx_frame(%s) rx frame status=%d\n",
						__FILE__,__LINE__,info->device_name,
						*ptmp);
			}

#if SYNCLINK_GENERIC_HDLC
			if (info->netcount)
				hdlcdev_rx(info,info->intermediate_rxbuffer,framesize);
			else
#endif
				ldisc_receive_buf(tty, info->intermediate_rxbuffer, info->flag_buf, framesize);
		}
	}
	
	mgsl_free_rx_frame_buffers( info, StartIndex, EndIndex );

	ReturnCode = true;

Cleanup:

	if ( info->rx_enabled && info->rx_overflow ) {

		if ( !info->rx_buffer_list[EndIndex].status &&
			info->rx_buffer_list[EndIndex].count ) {
			spin_lock_irqsave(&info->irq_spinlock,flags);
			usc_start_receiver(info);
			spin_unlock_irqrestore(&info->irq_spinlock,flags);
		}
	}

	return ReturnCode;

}	

static bool mgsl_get_raw_rx_frame(struct mgsl_struct *info)
{
	unsigned int CurrentIndex, NextIndex;
	unsigned short status;
	DMABUFFERENTRY *pBufEntry;
	unsigned int framesize = 0;
	bool ReturnCode = false;
	unsigned long flags;
	struct tty_struct *tty = info->port.tty;

	CurrentIndex = NextIndex = info->current_rx_buffer;
	++NextIndex;
	if ( NextIndex == info->rx_buffer_count )
		NextIndex = 0;

	if ( info->rx_buffer_list[CurrentIndex].status != 0 ||
		(info->rx_buffer_list[CurrentIndex].count == 0 &&
			info->rx_buffer_list[NextIndex].count == 0)) {

		status = info->rx_buffer_list[CurrentIndex].status;

		if ( status & (RXSTATUS_SHORT_FRAME + RXSTATUS_OVERRUN +
				RXSTATUS_CRC_ERROR + RXSTATUS_ABORT) ) {
			if ( status & RXSTATUS_SHORT_FRAME )
				info->icount.rxshort++;
			else if ( status & RXSTATUS_ABORT )
				info->icount.rxabort++;
			else if ( status & RXSTATUS_OVERRUN )
				info->icount.rxover++;
			else
				info->icount.rxcrc++;
			framesize = 0;
		} else {
			if ( status ) {
				if ( info->rx_buffer_list[CurrentIndex].rcc )
					framesize = RCLRVALUE - info->rx_buffer_list[CurrentIndex].rcc;
				else
					framesize = DMABUFFERSIZE;
			}
			else
				framesize = DMABUFFERSIZE;
		}

		if ( framesize > DMABUFFERSIZE ) {
			framesize = framesize % DMABUFFERSIZE;
		}


		if ( debug_level >= DEBUG_LEVEL_BH )
			printk("%s(%d):mgsl_get_raw_rx_frame(%s) status=%04X size=%d\n",
				__FILE__,__LINE__,info->device_name,status,framesize);

		if ( debug_level >= DEBUG_LEVEL_DATA )
			mgsl_trace_block(info,info->rx_buffer_list[CurrentIndex].virt_addr,
				min_t(int, framesize, DMABUFFERSIZE),0);

		if (framesize) {
			
			

			pBufEntry = &(info->rx_buffer_list[CurrentIndex]);
			memcpy( info->intermediate_rxbuffer, pBufEntry->virt_addr, framesize);
			info->icount.rxok++;

			ldisc_receive_buf(tty, info->intermediate_rxbuffer, info->flag_buf, framesize);
		}

		
		mgsl_free_rx_frame_buffers( info, CurrentIndex, CurrentIndex );

		ReturnCode = true;
	}


	if ( info->rx_enabled && info->rx_overflow ) {

		if ( !info->rx_buffer_list[CurrentIndex].status &&
			info->rx_buffer_list[CurrentIndex].count ) {
			spin_lock_irqsave(&info->irq_spinlock,flags);
			usc_start_receiver(info);
			spin_unlock_irqrestore(&info->irq_spinlock,flags);
		}
	}

	return ReturnCode;

}	

static void mgsl_load_tx_dma_buffer(struct mgsl_struct *info,
		const char *Buffer, unsigned int BufferSize)
{
	unsigned short Copycount;
	unsigned int i = 0;
	DMABUFFERENTRY *pBufEntry;
	
	if ( debug_level >= DEBUG_LEVEL_DATA )
		mgsl_trace_block(info,Buffer, min_t(int, BufferSize, DMABUFFERSIZE), 1);

	if (info->params.flags & HDLC_FLAG_HDLC_LOOPMODE) {
	 	info->cmr_value |= BIT13;			  
	}
		
	i = info->current_tx_buffer;
	info->start_tx_dma_buffer = i;

	
	

	info->tx_buffer_list[i].status = info->cmr_value & 0xf000;
	info->tx_buffer_list[i].rcc    = BufferSize;
	info->tx_buffer_list[i].count  = BufferSize;

	
	

	while( BufferSize ){
		
		pBufEntry = &info->tx_buffer_list[i++];
			
		if ( i == info->tx_buffer_count )
			i=0;

		
		
		if ( BufferSize > DMABUFFERSIZE )
			Copycount = DMABUFFERSIZE;
		else
			Copycount = BufferSize;

		
		
		if ( info->bus_type == MGSL_BUS_TYPE_PCI )
			mgsl_load_pci_memory(pBufEntry->virt_addr, Buffer,Copycount);
		else
			memcpy(pBufEntry->virt_addr, Buffer, Copycount);

		pBufEntry->count = Copycount;

		
		Buffer += Copycount;
		BufferSize -= Copycount;

		++info->tx_dma_buffers_used;
	}

	
	info->current_tx_buffer = i;

}	

static bool mgsl_register_test( struct mgsl_struct *info )
{
	static unsigned short BitPatterns[] =
		{ 0x0000, 0xffff, 0xaaaa, 0x5555, 0x1234, 0x6969, 0x9696, 0x0f0f };
	static unsigned int Patterncount = ARRAY_SIZE(BitPatterns);
	unsigned int i;
	bool rc = true;
	unsigned long flags;

	spin_lock_irqsave(&info->irq_spinlock,flags);
	usc_reset(info);

	

	if ( (usc_InReg( info, SICR ) != 0) ||
		  (usc_InReg( info, IVR  ) != 0) ||
		  (usc_InDmaReg( info, DIVR ) != 0) ){
		rc = false;
	}

	if ( rc ){
		
		

		for ( i = 0 ; i < Patterncount ; i++ ) {
			usc_OutReg( info, TC0R, BitPatterns[i] );
			usc_OutReg( info, TC1R, BitPatterns[(i+1)%Patterncount] );
			usc_OutReg( info, TCLR, BitPatterns[(i+2)%Patterncount] );
			usc_OutReg( info, RCLR, BitPatterns[(i+3)%Patterncount] );
			usc_OutReg( info, RSR,  BitPatterns[(i+4)%Patterncount] );
			usc_OutDmaReg( info, TBCR, BitPatterns[(i+5)%Patterncount] );

			if ( (usc_InReg( info, TC0R ) != BitPatterns[i]) ||
				  (usc_InReg( info, TC1R ) != BitPatterns[(i+1)%Patterncount]) ||
				  (usc_InReg( info, TCLR ) != BitPatterns[(i+2)%Patterncount]) ||
				  (usc_InReg( info, RCLR ) != BitPatterns[(i+3)%Patterncount]) ||
				  (usc_InReg( info, RSR )  != BitPatterns[(i+4)%Patterncount]) ||
				  (usc_InDmaReg( info, TBCR ) != BitPatterns[(i+5)%Patterncount]) ){
				rc = false;
				break;
			}
		}
	}

	usc_reset(info);
	spin_unlock_irqrestore(&info->irq_spinlock,flags);

	return rc;

}	

static bool mgsl_irq_test( struct mgsl_struct *info )
{
	unsigned long EndTime;
	unsigned long flags;

	spin_lock_irqsave(&info->irq_spinlock,flags);
	usc_reset(info);


	info->irq_occurred = false;

	
	
	
	
	usc_OutReg( info, PCR, (unsigned short)((usc_InReg(info, PCR) | BIT13) & ~BIT12) );

	usc_EnableMasterIrqBit(info);
	usc_EnableInterrupts(info, IO_PIN);
	usc_ClearIrqPendingBits(info, IO_PIN);
	
	usc_UnlatchIostatusBits(info, MISCSTATUS_TXC_LATCHED);
	usc_EnableStatusIrqs(info, SICR_TXC_ACTIVE + SICR_TXC_INACTIVE);

	spin_unlock_irqrestore(&info->irq_spinlock,flags);

	EndTime=100;
	while( EndTime-- && !info->irq_occurred ) {
		msleep_interruptible(10);
	}
	
	spin_lock_irqsave(&info->irq_spinlock,flags);
	usc_reset(info);
	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	
	return info->irq_occurred;

}	

static bool mgsl_dma_test( struct mgsl_struct *info )
{
	unsigned short FifoLevel;
	unsigned long phys_addr;
	unsigned int FrameSize;
	unsigned int i;
	char *TmpPtr;
	bool rc = true;
	unsigned short status=0;
	unsigned long EndTime;
	unsigned long flags;
	MGSL_PARAMS tmp_params;

	
	memcpy(&tmp_params,&info->params,sizeof(MGSL_PARAMS));
	
	memcpy(&info->params,&default_params,sizeof(MGSL_PARAMS));
	
#define TESTFRAMESIZE 40

	spin_lock_irqsave(&info->irq_spinlock,flags);
	
	

	usc_reset(info);
	usc_set_sdlc_mode(info);
	usc_enable_loopback(info,1);
	


	usc_OutDmaReg( info, RDMR, 0xe200 );
	
	spin_unlock_irqrestore(&info->irq_spinlock,flags);


	

	FrameSize = TESTFRAMESIZE;

	
	

	info->tx_buffer_list[0].count  = FrameSize;
	info->tx_buffer_list[0].rcc    = FrameSize;
	info->tx_buffer_list[0].status = 0x4000;

	

	TmpPtr = info->tx_buffer_list[0].virt_addr;
	for (i = 0; i < FrameSize; i++ )
		*TmpPtr++ = i;

	
	

	info->rx_buffer_list[0].status = 0;
	info->rx_buffer_list[0].count = FrameSize + 4;

	

	memset( info->rx_buffer_list[0].virt_addr, 0, FrameSize + 4 );

	
	

	info->tx_buffer_list[1].count = 0;
	info->rx_buffer_list[1].count = 0;
	

	
	
	
	
	spin_lock_irqsave(&info->irq_spinlock,flags);

	
	usc_RTCmd( info, RTCmd_PurgeRxFifo );

	
	phys_addr = info->rx_buffer_list[0].phys_entry;
	usc_OutDmaReg( info, NRARL, (unsigned short)phys_addr );
	usc_OutDmaReg( info, NRARU, (unsigned short)(phys_addr >> 16) );

	
	usc_InDmaReg( info, RDMR );
	usc_DmaCmd( info, DmaCmd_InitRxChannel );

	
	usc_OutReg( info, RMR, (unsigned short)((usc_InReg(info, RMR) & 0xfffc) | 0x0002) );
	
	spin_unlock_irqrestore(&info->irq_spinlock,flags);


	
	
	

	
	EndTime = jiffies + msecs_to_jiffies(100);

	for(;;) {
		if (time_after(jiffies, EndTime)) {
			rc = false;
			break;
		}

		spin_lock_irqsave(&info->irq_spinlock,flags);
		status = usc_InDmaReg( info, RDMR );
		spin_unlock_irqrestore(&info->irq_spinlock,flags);

		if ( !(status & BIT4) && (status & BIT5) ) {
			
			
			
			break;
		}
	}


	
	
	
	
	spin_lock_irqsave(&info->irq_spinlock,flags);

	
	

	usc_OutReg( info, TCLR, (unsigned short)info->tx_buffer_list[0].count );
	usc_RTCmd( info, RTCmd_PurgeTxFifo );

	

	phys_addr = info->tx_buffer_list[0].phys_entry;
	usc_OutDmaReg( info, NTARL, (unsigned short)phys_addr );
	usc_OutDmaReg( info, NTARU, (unsigned short)(phys_addr >> 16) );

	

	usc_OutReg( info, TCSR, (unsigned short)(( usc_InReg(info, TCSR) & 0x0f00) | 0xfa) );
	usc_DmaCmd( info, DmaCmd_InitTxChannel );

	

	usc_TCmd( info, TCmd_SelectTicrTxFifostatus );
	
	spin_unlock_irqrestore(&info->irq_spinlock,flags);


	
	
	
	
	
	EndTime = jiffies + msecs_to_jiffies(100);

	for(;;) {
		if (time_after(jiffies, EndTime)) {
			rc = false;
			break;
		}

		spin_lock_irqsave(&info->irq_spinlock,flags);
		FifoLevel = usc_InReg(info, TICR) >> 8;
		spin_unlock_irqrestore(&info->irq_spinlock,flags);
			
		if ( FifoLevel < 16 )
			break;
		else
			if ( FrameSize < 32 ) {
				
				
				if ( FifoLevel <= (32 - FrameSize) )
					break;
			}
	}


	if ( rc )
	{
		

		spin_lock_irqsave(&info->irq_spinlock,flags);
		
		
		usc_TCmd( info, TCmd_SendFrame );
		usc_OutReg( info, TMR, (unsigned short)((usc_InReg(info, TMR) & 0xfffc) | 0x0002) );
		
		spin_unlock_irqrestore(&info->irq_spinlock,flags);

						
		
		
		

		
		EndTime = jiffies + msecs_to_jiffies(100);

		

		spin_lock_irqsave(&info->irq_spinlock,flags);
		status = usc_InReg( info, TCSR );
		spin_unlock_irqrestore(&info->irq_spinlock,flags);

		while ( !(status & (BIT6+BIT5+BIT4+BIT2+BIT1)) ) {
			if (time_after(jiffies, EndTime)) {
				rc = false;
				break;
			}

			spin_lock_irqsave(&info->irq_spinlock,flags);
			status = usc_InReg( info, TCSR );
			spin_unlock_irqrestore(&info->irq_spinlock,flags);
		}
	}


	if ( rc ){
		
		if ( status & (BIT5 + BIT1) ) 
			rc = false;
	}

	if ( rc ) {
		

		
		EndTime = jiffies + msecs_to_jiffies(100);

		
		status=info->rx_buffer_list[0].status;
		while ( status == 0 ) {
			if (time_after(jiffies, EndTime)) {
				rc = false;
				break;
			}
			status=info->rx_buffer_list[0].status;
		}
	}


	if ( rc ) {
		
		status = info->rx_buffer_list[0].status;

		if ( status & (BIT8 + BIT3 + BIT1) ) {
			
			rc = false;
		} else {
			if ( memcmp( info->tx_buffer_list[0].virt_addr ,
				info->rx_buffer_list[0].virt_addr, FrameSize ) ){
				rc = false;
			}
		}
	}

	spin_lock_irqsave(&info->irq_spinlock,flags);
	usc_reset( info );
	spin_unlock_irqrestore(&info->irq_spinlock,flags);

	
	memcpy(&info->params,&tmp_params,sizeof(MGSL_PARAMS));
	
	return rc;

}	

static int mgsl_adapter_test( struct mgsl_struct *info )
{
	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk( "%s(%d):Testing device %s\n",
			__FILE__,__LINE__,info->device_name );
			
	if ( !mgsl_register_test( info ) ) {
		info->init_error = DiagStatus_AddressFailure;
		printk( "%s(%d):Register test failure for device %s Addr=%04X\n",
			__FILE__,__LINE__,info->device_name, (unsigned short)(info->io_base) );
		return -ENODEV;
	}

	if ( !mgsl_irq_test( info ) ) {
		info->init_error = DiagStatus_IrqFailure;
		printk( "%s(%d):Interrupt test failure for device %s IRQ=%d\n",
			__FILE__,__LINE__,info->device_name, (unsigned short)(info->irq_level) );
		return -ENODEV;
	}

	if ( !mgsl_dma_test( info ) ) {
		info->init_error = DiagStatus_DmaFailure;
		printk( "%s(%d):DMA test failure for device %s DMA=%d\n",
			__FILE__,__LINE__,info->device_name, (unsigned short)(info->dma_level) );
		return -ENODEV;
	}

	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk( "%s(%d):device %s passed diagnostics\n",
			__FILE__,__LINE__,info->device_name );
			
	return 0;

}	

static bool mgsl_memory_test( struct mgsl_struct *info )
{
	static unsigned long BitPatterns[] =
		{ 0x0, 0x55555555, 0xaaaaaaaa, 0x66666666, 0x99999999, 0xffffffff, 0x12345678 };
	unsigned long Patterncount = ARRAY_SIZE(BitPatterns);
	unsigned long i;
	unsigned long TestLimit = SHARED_MEM_ADDRESS_SIZE/sizeof(unsigned long);
	unsigned long * TestAddr;

	if ( info->bus_type != MGSL_BUS_TYPE_PCI )
		return true;

	TestAddr = (unsigned long *)info->memory_base;

	

	for ( i = 0 ; i < Patterncount ; i++ ) {
		*TestAddr = BitPatterns[i];
		if ( *TestAddr != BitPatterns[i] )
			return false;
	}

	
	

	for ( i = 0 ; i < TestLimit ; i++ ) {
		*TestAddr = i * 4;
		TestAddr++;
	}

	TestAddr = (unsigned long *)info->memory_base;

	for ( i = 0 ; i < TestLimit ; i++ ) {
		if ( *TestAddr != i * 4 )
			return false;
		TestAddr++;
	}

	memset( info->memory_base, 0, SHARED_MEM_ADDRESS_SIZE );

	return true;

}	


static void mgsl_load_pci_memory( char* TargetPtr, const char* SourcePtr,
	unsigned short count )
{
	
#define PCI_LOAD_INTERVAL 64

	unsigned short Intervalcount = count / PCI_LOAD_INTERVAL;
	unsigned short Index;
	unsigned long Dummy;

	for ( Index = 0 ; Index < Intervalcount ; Index++ )
	{
		memcpy(TargetPtr, SourcePtr, PCI_LOAD_INTERVAL);
		Dummy = *((volatile unsigned long *)TargetPtr);
		TargetPtr += PCI_LOAD_INTERVAL;
		SourcePtr += PCI_LOAD_INTERVAL;
	}

	memcpy( TargetPtr, SourcePtr, count % PCI_LOAD_INTERVAL );

}	

static void mgsl_trace_block(struct mgsl_struct *info,const char* data, int count, int xmit)
{
	int i;
	int linecount;
	if (xmit)
		printk("%s tx data:\n",info->device_name);
	else
		printk("%s rx data:\n",info->device_name);
		
	while(count) {
		if (count > 16)
			linecount = 16;
		else
			linecount = count;
			
		for(i=0;i<linecount;i++)
			printk("%02X ",(unsigned char)data[i]);
		for(;i<17;i++)
			printk("   ");
		for(i=0;i<linecount;i++) {
			if (data[i]>=040 && data[i]<=0176)
				printk("%c",data[i]);
			else
				printk(".");
		}
		printk("\n");
		
		data  += linecount;
		count -= linecount;
	}
}	

static void mgsl_tx_timeout(unsigned long context)
{
	struct mgsl_struct *info = (struct mgsl_struct*)context;
	unsigned long flags;
	
	if ( debug_level >= DEBUG_LEVEL_INFO )
		printk( "%s(%d):mgsl_tx_timeout(%s)\n",
			__FILE__,__LINE__,info->device_name);
	if(info->tx_active &&
	   (info->params.mode == MGSL_MODE_HDLC ||
	    info->params.mode == MGSL_MODE_RAW) ) {
		info->icount.txtimeout++;
	}
	spin_lock_irqsave(&info->irq_spinlock,flags);
	info->tx_active = false;
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;

	if ( info->params.flags & HDLC_FLAG_HDLC_LOOPMODE )
		usc_loopmode_cancel_transmit( info );

	spin_unlock_irqrestore(&info->irq_spinlock,flags);
	
#if SYNCLINK_GENERIC_HDLC
	if (info->netcount)
		hdlcdev_tx_done(info);
	else
#endif
		mgsl_bh_transmit(info);
	
}	

static int mgsl_loopmode_send_done( struct mgsl_struct * info )
{
	unsigned long flags;
	
	spin_lock_irqsave(&info->irq_spinlock,flags);
	if (info->params.flags & HDLC_FLAG_HDLC_LOOPMODE) {
		if (info->tx_active)
			info->loopmode_send_done_requested = true;
		else
			usc_loopmode_send_done(info);
	}
	spin_unlock_irqrestore(&info->irq_spinlock,flags);

	return 0;
}

static void usc_loopmode_send_done( struct mgsl_struct * info )
{
 	info->loopmode_send_done_requested = false;
 	
 	info->cmr_value &= ~BIT13;			  
 	usc_OutReg(info, CMR, info->cmr_value);
}

static void usc_loopmode_cancel_transmit( struct mgsl_struct * info )
{
 	
 	usc_RTCmd( info, RTCmd_PurgeTxFifo );
 	usc_DmaCmd( info, DmaCmd_ResetTxChannel );
  	usc_loopmode_send_done( info );
}

static void usc_loopmode_insert_request( struct mgsl_struct * info )
{
 	info->loopmode_insert_requested = true;
 
 	usc_OutReg( info, RICR, 
		(usc_InReg( info, RICR ) | RXSTATUS_ABORT_RECEIVED ) );
		
	
	info->cmr_value |= BIT13;
 	usc_OutReg(info, CMR, info->cmr_value);
}

static int usc_loopmode_active( struct mgsl_struct * info)
{
 	return usc_InReg( info, CCSR ) & BIT7 ? 1 : 0 ;
}

#if SYNCLINK_GENERIC_HDLC

static int hdlcdev_attach(struct net_device *dev, unsigned short encoding,
			  unsigned short parity)
{
	struct mgsl_struct *info = dev_to_port(dev);
	unsigned char  new_encoding;
	unsigned short new_crctype;

	
	if (info->port.count)
		return -EBUSY;

	switch (encoding)
	{
	case ENCODING_NRZ:        new_encoding = HDLC_ENCODING_NRZ; break;
	case ENCODING_NRZI:       new_encoding = HDLC_ENCODING_NRZI_SPACE; break;
	case ENCODING_FM_MARK:    new_encoding = HDLC_ENCODING_BIPHASE_MARK; break;
	case ENCODING_FM_SPACE:   new_encoding = HDLC_ENCODING_BIPHASE_SPACE; break;
	case ENCODING_MANCHESTER: new_encoding = HDLC_ENCODING_BIPHASE_LEVEL; break;
	default: return -EINVAL;
	}

	switch (parity)
	{
	case PARITY_NONE:            new_crctype = HDLC_CRC_NONE; break;
	case PARITY_CRC16_PR1_CCITT: new_crctype = HDLC_CRC_16_CCITT; break;
	case PARITY_CRC32_PR1_CCITT: new_crctype = HDLC_CRC_32_CCITT; break;
	default: return -EINVAL;
	}

	info->params.encoding = new_encoding;
	info->params.crc_type = new_crctype;

	
	if (info->netcount)
		mgsl_program_hw(info);

	return 0;
}

static netdev_tx_t hdlcdev_xmit(struct sk_buff *skb,
				      struct net_device *dev)
{
	struct mgsl_struct *info = dev_to_port(dev);
	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk(KERN_INFO "%s:hdlc_xmit(%s)\n",__FILE__,dev->name);

	
	netif_stop_queue(dev);

	
	info->xmit_cnt = skb->len;
	mgsl_load_tx_dma_buffer(info, skb->data, skb->len);

	
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;

	
	dev_kfree_skb(skb);

	
	dev->trans_start = jiffies;

	
	spin_lock_irqsave(&info->irq_spinlock,flags);
	if (!info->tx_active)
	 	usc_start_transmitter(info);
	spin_unlock_irqrestore(&info->irq_spinlock,flags);

	return NETDEV_TX_OK;
}

static int hdlcdev_open(struct net_device *dev)
{
	struct mgsl_struct *info = dev_to_port(dev);
	int rc;
	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s:hdlcdev_open(%s)\n",__FILE__,dev->name);

	
	if ((rc = hdlc_open(dev)))
		return rc;

	
	spin_lock_irqsave(&info->netlock, flags);
	if (info->port.count != 0 || info->netcount != 0) {
		printk(KERN_WARNING "%s: hdlc_open returning busy\n", dev->name);
		spin_unlock_irqrestore(&info->netlock, flags);
		return -EBUSY;
	}
	info->netcount=1;
	spin_unlock_irqrestore(&info->netlock, flags);

	
	if ((rc = startup(info)) != 0) {
		spin_lock_irqsave(&info->netlock, flags);
		info->netcount=0;
		spin_unlock_irqrestore(&info->netlock, flags);
		return rc;
	}

	
	info->serial_signals |= SerialSignal_RTS + SerialSignal_DTR;
	mgsl_program_hw(info);

	
	dev->trans_start = jiffies;
	netif_start_queue(dev);

	
	spin_lock_irqsave(&info->irq_spinlock, flags);
	usc_get_serial_signals(info);
	spin_unlock_irqrestore(&info->irq_spinlock, flags);
	if (info->serial_signals & SerialSignal_DCD)
		netif_carrier_on(dev);
	else
		netif_carrier_off(dev);
	return 0;
}

static int hdlcdev_close(struct net_device *dev)
{
	struct mgsl_struct *info = dev_to_port(dev);
	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s:hdlcdev_close(%s)\n",__FILE__,dev->name);

	netif_stop_queue(dev);

	
	shutdown(info);

	hdlc_close(dev);

	spin_lock_irqsave(&info->netlock, flags);
	info->netcount=0;
	spin_unlock_irqrestore(&info->netlock, flags);

	return 0;
}

static int hdlcdev_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	const size_t size = sizeof(sync_serial_settings);
	sync_serial_settings new_line;
	sync_serial_settings __user *line = ifr->ifr_settings.ifs_ifsu.sync;
	struct mgsl_struct *info = dev_to_port(dev);
	unsigned int flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("%s:hdlcdev_ioctl(%s)\n",__FILE__,dev->name);

	
	if (info->port.count)
		return -EBUSY;

	if (cmd != SIOCWANDEV)
		return hdlc_ioctl(dev, ifr, cmd);

	switch(ifr->ifr_settings.type) {
	case IF_GET_IFACE: 

		ifr->ifr_settings.type = IF_IFACE_SYNC_SERIAL;
		if (ifr->ifr_settings.size < size) {
			ifr->ifr_settings.size = size; 
			return -ENOBUFS;
		}

		flags = info->params.flags & (HDLC_FLAG_RXC_RXCPIN | HDLC_FLAG_RXC_DPLL |
					      HDLC_FLAG_RXC_BRG    | HDLC_FLAG_RXC_TXCPIN |
					      HDLC_FLAG_TXC_TXCPIN | HDLC_FLAG_TXC_DPLL |
					      HDLC_FLAG_TXC_BRG    | HDLC_FLAG_TXC_RXCPIN);

		switch (flags){
		case (HDLC_FLAG_RXC_RXCPIN | HDLC_FLAG_TXC_TXCPIN): new_line.clock_type = CLOCK_EXT; break;
		case (HDLC_FLAG_RXC_BRG    | HDLC_FLAG_TXC_BRG):    new_line.clock_type = CLOCK_INT; break;
		case (HDLC_FLAG_RXC_RXCPIN | HDLC_FLAG_TXC_BRG):    new_line.clock_type = CLOCK_TXINT; break;
		case (HDLC_FLAG_RXC_RXCPIN | HDLC_FLAG_TXC_RXCPIN): new_line.clock_type = CLOCK_TXFROMRX; break;
		default: new_line.clock_type = CLOCK_DEFAULT;
		}

		new_line.clock_rate = info->params.clock_speed;
		new_line.loopback   = info->params.loopback ? 1:0;

		if (copy_to_user(line, &new_line, size))
			return -EFAULT;
		return 0;

	case IF_IFACE_SYNC_SERIAL: 

		if(!capable(CAP_NET_ADMIN))
			return -EPERM;
		if (copy_from_user(&new_line, line, size))
			return -EFAULT;

		switch (new_line.clock_type)
		{
		case CLOCK_EXT:      flags = HDLC_FLAG_RXC_RXCPIN | HDLC_FLAG_TXC_TXCPIN; break;
		case CLOCK_TXFROMRX: flags = HDLC_FLAG_RXC_RXCPIN | HDLC_FLAG_TXC_RXCPIN; break;
		case CLOCK_INT:      flags = HDLC_FLAG_RXC_BRG    | HDLC_FLAG_TXC_BRG;    break;
		case CLOCK_TXINT:    flags = HDLC_FLAG_RXC_RXCPIN | HDLC_FLAG_TXC_BRG;    break;
		case CLOCK_DEFAULT:  flags = info->params.flags &
					     (HDLC_FLAG_RXC_RXCPIN | HDLC_FLAG_RXC_DPLL |
					      HDLC_FLAG_RXC_BRG    | HDLC_FLAG_RXC_TXCPIN |
					      HDLC_FLAG_TXC_TXCPIN | HDLC_FLAG_TXC_DPLL |
					      HDLC_FLAG_TXC_BRG    | HDLC_FLAG_TXC_RXCPIN); break;
		default: return -EINVAL;
		}

		if (new_line.loopback != 0 && new_line.loopback != 1)
			return -EINVAL;

		info->params.flags &= ~(HDLC_FLAG_RXC_RXCPIN | HDLC_FLAG_RXC_DPLL |
					HDLC_FLAG_RXC_BRG    | HDLC_FLAG_RXC_TXCPIN |
					HDLC_FLAG_TXC_TXCPIN | HDLC_FLAG_TXC_DPLL |
					HDLC_FLAG_TXC_BRG    | HDLC_FLAG_TXC_RXCPIN);
		info->params.flags |= flags;

		info->params.loopback = new_line.loopback;

		if (flags & (HDLC_FLAG_RXC_BRG | HDLC_FLAG_TXC_BRG))
			info->params.clock_speed = new_line.clock_rate;
		else
			info->params.clock_speed = 0;

		
		if (info->netcount)
			mgsl_program_hw(info);
		return 0;

	default:
		return hdlc_ioctl(dev, ifr, cmd);
	}
}

static void hdlcdev_tx_timeout(struct net_device *dev)
{
	struct mgsl_struct *info = dev_to_port(dev);
	unsigned long flags;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("hdlcdev_tx_timeout(%s)\n",dev->name);

	dev->stats.tx_errors++;
	dev->stats.tx_aborted_errors++;

	spin_lock_irqsave(&info->irq_spinlock,flags);
	usc_stop_transmitter(info);
	spin_unlock_irqrestore(&info->irq_spinlock,flags);

	netif_wake_queue(dev);
}

static void hdlcdev_tx_done(struct mgsl_struct *info)
{
	if (netif_queue_stopped(info->netdev))
		netif_wake_queue(info->netdev);
}

static void hdlcdev_rx(struct mgsl_struct *info, char *buf, int size)
{
	struct sk_buff *skb = dev_alloc_skb(size);
	struct net_device *dev = info->netdev;

	if (debug_level >= DEBUG_LEVEL_INFO)
		printk("hdlcdev_rx(%s)\n", dev->name);

	if (skb == NULL) {
		printk(KERN_NOTICE "%s: can't alloc skb, dropping packet\n",
		       dev->name);
		dev->stats.rx_dropped++;
		return;
	}

	memcpy(skb_put(skb, size), buf, size);

	skb->protocol = hdlc_type_trans(skb, dev);

	dev->stats.rx_packets++;
	dev->stats.rx_bytes += size;

	netif_rx(skb);
}

static const struct net_device_ops hdlcdev_ops = {
	.ndo_open       = hdlcdev_open,
	.ndo_stop       = hdlcdev_close,
	.ndo_change_mtu = hdlc_change_mtu,
	.ndo_start_xmit = hdlc_start_xmit,
	.ndo_do_ioctl   = hdlcdev_ioctl,
	.ndo_tx_timeout = hdlcdev_tx_timeout,
};

static int hdlcdev_init(struct mgsl_struct *info)
{
	int rc;
	struct net_device *dev;
	hdlc_device *hdlc;

	

	if (!(dev = alloc_hdlcdev(info))) {
		printk(KERN_ERR "%s:hdlc device allocation failure\n",__FILE__);
		return -ENOMEM;
	}

	
	dev->base_addr = info->io_base;
	dev->irq       = info->irq_level;
	dev->dma       = info->dma_level;

	
	dev->netdev_ops     = &hdlcdev_ops;
	dev->watchdog_timeo = 10 * HZ;
	dev->tx_queue_len   = 50;

	
	hdlc         = dev_to_hdlc(dev);
	hdlc->attach = hdlcdev_attach;
	hdlc->xmit   = hdlcdev_xmit;

	
	if ((rc = register_hdlc_device(dev))) {
		printk(KERN_WARNING "%s:unable to register hdlc device\n",__FILE__);
		free_netdev(dev);
		return rc;
	}

	info->netdev = dev;
	return 0;
}

static void hdlcdev_exit(struct mgsl_struct *info)
{
	unregister_hdlc_device(info->netdev);
	free_netdev(info->netdev);
	info->netdev = NULL;
}

#endif 


static int __devinit synclink_init_one (struct pci_dev *dev,
					const struct pci_device_id *ent)
{
	struct mgsl_struct *info;

	if (pci_enable_device(dev)) {
		printk("error enabling pci device %p\n", dev);
		return -EIO;
	}

	if (!(info = mgsl_allocate_device())) {
		printk("can't allocate device instance data.\n");
		return -EIO;
	}

        
		
	info->io_base = pci_resource_start(dev, 2);
	info->irq_level = dev->irq;
	info->phys_memory_base = pci_resource_start(dev, 3);
				
	info->phys_lcr_base = pci_resource_start(dev, 0);
	info->lcr_offset    = info->phys_lcr_base & (PAGE_SIZE-1);
	info->phys_lcr_base &= ~(PAGE_SIZE-1);
				
	info->bus_type = MGSL_BUS_TYPE_PCI;
	info->io_addr_size = 8;
	info->irq_flags = IRQF_SHARED;

	if (dev->device == 0x0210) {
		
		info->misc_ctrl_value = 0x007c4080;
		info->hw_version = 1;
	} else {
		info->misc_ctrl_value = 0x087e4546;
		info->hw_version = 0;
	}
				
	mgsl_add_device(info);

	return 0;
}

static void __devexit synclink_remove_one (struct pci_dev *dev)
{
}

