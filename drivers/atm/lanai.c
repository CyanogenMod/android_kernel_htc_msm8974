/* lanai.c -- Copyright 1999-2003 by Mitchell Blank Jr <mitch@sfgoth.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 * This driver supports ATM cards based on the Efficient "Lanai"
 * chipset such as the Speedstream 3010 and the ENI-25p.  The
 * Speedstream 3060 is currently not supported since we don't
 * have the code to drive the on-board Alcatel DSL chipset (yet).
 *
 * Thanks to Efficient for supporting this project with hardware,
 * documentation, and by answering my questions.
 *
 * Things not working yet:
 *
 * o  We don't support the Speedstream 3060 yet - this card has
 *    an on-board DSL modem chip by Alcatel and the driver will
 *    need some extra code added to handle it
 *
 * o  Note that due to limitations of the Lanai only one VCC can be
 *    in CBR at once
 *
 * o We don't currently parse the EEPROM at all.  The code is all
 *   there as per the spec, but it doesn't actually work.  I think
 *   there may be some issues with the docs.  Anyway, do NOT
 *   enable it yet - bugs in that code may actually damage your
 *   hardware!  Because of this you should hardware an ESI before
 *   trying to use this in a LANE or MPOA environment.
 *
 * o  AAL0 is stubbed in but the actual rx/tx path isn't written yet:
 *	vcc_tx_aal0() needs to send or queue a SKB
 *	vcc_tx_unqueue_aal0() needs to attempt to send queued SKBs
 *	vcc_rx_aal0() needs to handle AAL0 interrupts
 *    This isn't too much work - I just wanted to get other things
 *    done first.
 *
 * o  lanai_change_qos() isn't written yet
 *
 * o  There aren't any ioctl's yet -- I'd like to eventually support
 *    setting loopback and LED modes that way.
 *
 * o  If the segmentation engine or DMA gets shut down we should restart
 *    card as per section 17.0i.  (see lanai_reset)
 *
 * o setsockopt(SO_CIRANGE) isn't done (although despite what the
 *   API says it isn't exactly commonly implemented)
 */


#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/atmdev.h>
#include <asm/io.h>
#include <asm/byteorder.h>
#include <linux/spinlock.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>


#define NUM_VCI			(1024)

#define DEBUG
#undef DEBUG_RW

#define FULL_MEMORY_TEST

#define SERVICE_ENTRIES		(1024)

 

#define TX_FIFO_DEPTH		(7)

#define LANAI_POLL_PERIOD	(10*HZ)

#define AAL5_RX_MULTIPLIER	(3)

#define AAL5_TX_MULTIPLIER	(3)

#define AAL0_TX_MULTIPLIER	(40)

#define AAL0_RX_BUFFER_SIZE	(PAGE_SIZE)



#define DEV_LABEL "lanai"

#ifdef DEBUG

#define DPRINTK(format, args...) \
	printk(KERN_DEBUG DEV_LABEL ": " format, ##args)
#define APRINTK(truth, format, args...) \
	do { \
		if (unlikely(!(truth))) \
			printk(KERN_ERR DEV_LABEL ": " format, ##args); \
	} while (0)

#else 

#define DPRINTK(format, args...)
#define APRINTK(truth, format, args...)

#endif 

#ifdef DEBUG_RW
#define RWDEBUG(format, args...) \
	printk(KERN_DEBUG DEV_LABEL ": " format, ##args)
#else 
#define RWDEBUG(format, args...)
#endif


#define LANAI_MAPPING_SIZE	(0x40000)
#define LANAI_EEPROM_SIZE	(128)

typedef int vci_t;
typedef void __iomem *bus_addr_t;

struct lanai_buffer {
	u32 *start;	
	u32 *end;	
	u32 *ptr;	
	dma_addr_t dmaaddr;
};

struct lanai_vcc_stats {
	unsigned rx_nomem;
	union {
		struct {
			unsigned rx_badlen;
			unsigned service_trash;
			unsigned service_stream;
			unsigned service_rxcrc;
		} aal5;
		struct {
		} aal0;
	} x;
};

struct lanai_dev;			

struct lanai_vcc {
	bus_addr_t vbase;		
	struct lanai_vcc_stats stats;
	int nref;			
	vci_t vci;
	struct {
		struct lanai_buffer buf;
		struct atm_vcc *atmvcc;	
	} rx;
	struct {
		struct lanai_buffer buf;
		struct atm_vcc *atmvcc;	
		int endptr;		
		struct sk_buff_head backlog;
		void (*unqueue)(struct lanai_dev *, struct lanai_vcc *, int);
	} tx;
};

enum lanai_type {
	lanai2	= PCI_DEVICE_ID_EF_ATM_LANAI2,
	lanaihb	= PCI_DEVICE_ID_EF_ATM_LANAIHB
};

struct lanai_dev_stats {
	unsigned ovfl_trash;	
	unsigned vci_trash;	
	unsigned hec_err;	
	unsigned atm_ovfl;	
	unsigned pcierr_parity_detect;
	unsigned pcierr_serr_set;
	unsigned pcierr_master_abort;
	unsigned pcierr_m_target_abort;
	unsigned pcierr_s_target_abort;
	unsigned pcierr_master_parity;
	unsigned service_notx;
	unsigned service_norx;
	unsigned service_rxnotaal5;
	unsigned dma_reenable;
	unsigned card_reset;
};

struct lanai_dev {
	bus_addr_t base;
	struct lanai_dev_stats stats;
	struct lanai_buffer service;
	struct lanai_vcc **vccs;
#ifdef USE_POWERDOWN
	int nbound;			
#endif
	enum lanai_type type;
	vci_t num_vci;			
	u8 eeprom[LANAI_EEPROM_SIZE];
	u32 serialno, magicno;
	struct pci_dev *pci;
	DECLARE_BITMAP(backlog_vccs, NUM_VCI);   
	DECLARE_BITMAP(transmit_ready, NUM_VCI); 
	struct timer_list timer;
	int naal0;
	struct lanai_buffer aal0buf;	
	u32 conf1, conf2;		
	u32 status;			
	spinlock_t endtxlock;
	spinlock_t servicelock;
	struct atm_vcc *cbrvcc;
	int number;
	int board_rev;
};

static void vci_bitfield_iterate(struct lanai_dev *lanai,
	const unsigned long *lp,
	void (*func)(struct lanai_dev *,vci_t vci))
{
	vci_t vci;

	for_each_set_bit(vci, lp, NUM_VCI)
		func(lanai, vci);
}


#define LANAI_PAGE_SIZE   ((PAGE_SIZE >= 1024) ? PAGE_SIZE : 1024)

static void lanai_buf_allocate(struct lanai_buffer *buf,
	size_t bytes, size_t minbytes, struct pci_dev *pci)
{
	int size;

	if (bytes > (128 * 1024))	
		bytes = 128 * 1024;
	for (size = LANAI_PAGE_SIZE; size < bytes; size *= 2)
		;
	if (minbytes < LANAI_PAGE_SIZE)
		minbytes = LANAI_PAGE_SIZE;
	do {
		buf->start = pci_alloc_consistent(pci, size, &buf->dmaaddr);
		if (buf->start != NULL) {	
			
			APRINTK((buf->dmaaddr & ~0xFFFFFF00) == 0,
			    "bad dmaaddr: 0x%lx\n",
			    (unsigned long) buf->dmaaddr);
			buf->ptr = buf->start;
			buf->end = (u32 *)
			    (&((unsigned char *) buf->start)[size]);
			memset(buf->start, 0, size);
			break;
		}
		size /= 2;
	} while (size >= minbytes);
}

static inline size_t lanai_buf_size(const struct lanai_buffer *buf)
{
	return ((unsigned long) buf->end) - ((unsigned long) buf->start);
}

static void lanai_buf_deallocate(struct lanai_buffer *buf,
	struct pci_dev *pci)
{
	if (buf->start != NULL) {
		pci_free_consistent(pci, lanai_buf_size(buf),
		    buf->start, buf->dmaaddr);
		buf->start = buf->end = buf->ptr = NULL;
	}
}

static int lanai_buf_size_cardorder(const struct lanai_buffer *buf)
{
	int order = get_order(lanai_buf_size(buf)) + (PAGE_SHIFT - 10);

	
	if (order > 7)
		order = 7;
	return order;
}


enum lanai_register {
	Reset_Reg		= 0x00,	
#define   RESET_GET_BOARD_REV(x)    (((x)>> 0)&0x03)	
#define   RESET_GET_BOARD_ID(x)	    (((x)>> 2)&0x03)	
#define     BOARD_ID_LANAI256		(0)	
	Endian_Reg		= 0x04,	
	IntStatus_Reg		= 0x08,	
	IntStatusMasked_Reg	= 0x0C,	
	IntAck_Reg		= 0x10,	
	IntAckMasked_Reg	= 0x14,	
	IntStatusSet_Reg	= 0x18,	
	IntStatusSetMasked_Reg	= 0x1C,	
	IntControlEna_Reg	= 0x20,	
	IntControlDis_Reg	= 0x24,	
	Status_Reg		= 0x28,	
#define   STATUS_PROMDATA	 (0x00000001)	
#define   STATUS_WAITING	 (0x00000002)	
#define	  STATUS_SOOL		 (0x00000004)	
#define   STATUS_LOCD		 (0x00000008)	
#define	  STATUS_LED		 (0x00000010)	
#define   STATUS_GPIN		 (0x00000020)	
#define   STATUS_BUTTBUSY	 (0x00000040)	
	Config1_Reg		= 0x2C,	
#define   CONFIG1_PROMDATA	 (0x00000001)	
#define   CONFIG1_PROMCLK	 (0x00000002)	
#define   CONFIG1_SET_READMODE(x) ((x)*0x004)	
#define     READMODE_PLAIN	    (0)		
#define     READMODE_LINE	    (2)		
#define     READMODE_MULTIPLE	    (3)		
#define   CONFIG1_DMA_ENABLE	 (0x00000010)	
#define   CONFIG1_POWERDOWN	 (0x00000020)	
#define   CONFIG1_SET_LOOPMODE(x) ((x)*0x080)	
#define     LOOPMODE_NORMAL	    (0)		
#define     LOOPMODE_TIME	    (1)
#define     LOOPMODE_DIAG	    (2)
#define     LOOPMODE_LINE	    (3)
#define   CONFIG1_MASK_LOOPMODE  (0x00000180)
#define   CONFIG1_SET_LEDMODE(x) ((x)*0x0200)	
#define     LEDMODE_NOT_SOOL	    (0)		
#define	    LEDMODE_OFF		    (1)		
#define	    LEDMODE_ON		    (2)		
#define	    LEDMODE_NOT_LOCD	    (3)		
#define	    LEDMORE_GPIN	    (4)		
#define     LEDMODE_NOT_GPIN	    (7)		
#define   CONFIG1_MASK_LEDMODE	 (0x00000E00)
#define   CONFIG1_GPOUT1	 (0x00001000)	
#define   CONFIG1_GPOUT2	 (0x00002000)	
#define   CONFIG1_GPOUT3	 (0x00004000)	
	Config2_Reg		= 0x30,	
#define   CONFIG2_HOWMANY	 (0x00000001)	
#define   CONFIG2_PTI7_MODE	 (0x00000002)	
#define   CONFIG2_VPI_CHK_DIS	 (0x00000004)	
#define   CONFIG2_HEC_DROP	 (0x00000008)	
#define   CONFIG2_VCI0_NORMAL	 (0x00000010)	
#define   CONFIG2_CBR_ENABLE	 (0x00000020)	
#define   CONFIG2_TRASH_ALL	 (0x00000040)	
#define   CONFIG2_TX_DISABLE	 (0x00000080)	
#define   CONFIG2_SET_TRASH	 (0x00000100)	
	Statistics_Reg		= 0x34,	
#define   STATS_GET_FIFO_OVFL(x)    (((x)>> 0)&0xFF)	
#define   STATS_GET_HEC_ERR(x)      (((x)>> 8)&0xFF)	
#define   STATS_GET_BAD_VCI(x)      (((x)>>16)&0xFF)	
#define   STATS_GET_BUF_OVFL(x)     (((x)>>24)&0xFF)	
	ServiceStuff_Reg	= 0x38,	
#define   SSTUFF_SET_SIZE(x) ((x)*0x20000000)	
#define   SSTUFF_SET_ADDR(x)	    ((x)>>8)	
	ServWrite_Reg		= 0x3C,	
	ServRead_Reg		= 0x40,	
	TxDepth_Reg		= 0x44,	
	Butt_Reg		= 0x48,	
	CBR_ICG_Reg		= 0x50,
	CBR_PTR_Reg		= 0x54,
	PingCount_Reg		= 0x58,	
	DMA_Addr_Reg		= 0x5C	
};

static inline bus_addr_t reg_addr(const struct lanai_dev *lanai,
	enum lanai_register reg)
{
	return lanai->base + reg;
}

static inline u32 reg_read(const struct lanai_dev *lanai,
	enum lanai_register reg)
{
	u32 t;
	t = readl(reg_addr(lanai, reg));
	RWDEBUG("R [0x%08X] 0x%02X = 0x%08X\n", (unsigned int) lanai->base,
	    (int) reg, t);
	return t;
}

static inline void reg_write(const struct lanai_dev *lanai, u32 val,
	enum lanai_register reg)
{
	RWDEBUG("W [0x%08X] 0x%02X < 0x%08X\n", (unsigned int) lanai->base,
	    (int) reg, val);
	writel(val, reg_addr(lanai, reg));
}

static inline void conf1_write(const struct lanai_dev *lanai)
{
	reg_write(lanai, lanai->conf1, Config1_Reg);
}

static inline void conf2_write(const struct lanai_dev *lanai)
{
	reg_write(lanai, lanai->conf2, Config2_Reg);
}

static inline void conf2_write_if_powerup(const struct lanai_dev *lanai)
{
#ifdef USE_POWERDOWN
	if (unlikely((lanai->conf1 & CONFIG1_POWERDOWN) != 0))
		return;
#endif 
	conf2_write(lanai);
}

static inline void reset_board(const struct lanai_dev *lanai)
{
	DPRINTK("about to reset board\n");
	reg_write(lanai, 0, Reset_Reg);
	udelay(5);
}


#define SRAM_START (0x20000)
#define SRAM_BYTES (0x20000)	

static inline bus_addr_t sram_addr(const struct lanai_dev *lanai, int offset)
{
	return lanai->base + SRAM_START + offset;
}

static inline u32 sram_read(const struct lanai_dev *lanai, int offset)
{
	return readl(sram_addr(lanai, offset));
}

static inline void sram_write(const struct lanai_dev *lanai,
	u32 val, int offset)
{
	writel(val, sram_addr(lanai, offset));
}

static int __devinit sram_test_word(const struct lanai_dev *lanai,
				    int offset, u32 pattern)
{
	u32 readback;
	sram_write(lanai, pattern, offset);
	readback = sram_read(lanai, offset);
	if (likely(readback == pattern))
		return 0;
	printk(KERN_ERR DEV_LABEL
	    "(itf %d): SRAM word at %d bad: wrote 0x%X, read 0x%X\n",
	    lanai->number, offset,
	    (unsigned int) pattern, (unsigned int) readback);
	return -EIO;
}

static int __devinit sram_test_pass(const struct lanai_dev *lanai, u32 pattern)
{
	int offset, result = 0;
	for (offset = 0; offset < SRAM_BYTES && result == 0; offset += 4)
		result = sram_test_word(lanai, offset, pattern);
	return result;
}

static int __devinit sram_test_and_clear(const struct lanai_dev *lanai)
{
#ifdef FULL_MEMORY_TEST
	int result;
	DPRINTK("testing SRAM\n");
	if ((result = sram_test_pass(lanai, 0x5555)) != 0)
		return result;
	if ((result = sram_test_pass(lanai, 0xAAAA)) != 0)
		return result;
#endif
	DPRINTK("clearing SRAM\n");
	return sram_test_pass(lanai, 0x0000);
}


enum lanai_vcc_offset {
	vcc_rxaddr1		= 0x00,	
#define   RXADDR1_SET_SIZE(x) ((x)*0x0000100)	
#define   RXADDR1_SET_RMMODE(x) ((x)*0x00800)	
#define     RMMODE_TRASH	  (0)		
#define     RMMODE_PRESERVE	  (1)		
#define     RMMODE_PIPE		  (2)		
#define     RMMODE_PIPEALL	  (3)		
#define   RXADDR1_OAM_PRESERVE	 (0x00002000)	
#define   RXADDR1_SET_MODE(x) ((x)*0x0004000)	
#define     RXMODE_TRASH	  (0)		
#define     RXMODE_AAL0		  (1)		
#define     RXMODE_AAL5		  (2)		
#define     RXMODE_AAL5_STREAM	  (3)		
	vcc_rxaddr2		= 0x04,	
	vcc_rxcrc1		= 0x08,	
	vcc_rxcrc2		= 0x0C,
	vcc_rxwriteptr		= 0x10, 
#define   RXWRITEPTR_LASTEFCI	 (0x00002000)	
#define   RXWRITEPTR_DROPPING	 (0x00004000)	
#define   RXWRITEPTR_TRASHING	 (0x00008000)	
	vcc_rxbufstart		= 0x14,	
#define   RXBUFSTART_CLP	 (0x00004000)
#define   RXBUFSTART_CI		 (0x00008000)
	vcc_rxreadptr		= 0x18,	
	vcc_txicg		= 0x1C, 
	vcc_txaddr1		= 0x20,	
#define   TXADDR1_SET_SIZE(x) ((x)*0x0000100)	
#define   TXADDR1_ABR		 (0x00008000)	
	vcc_txaddr2		= 0x24,	
	vcc_txcrc1		= 0x28,	
	vcc_txcrc2		= 0x2C,
	vcc_txreadptr		= 0x30, 
#define   TXREADPTR_GET_PTR(x) ((x)&0x01FFF)
#define   TXREADPTR_MASK_DELTA	(0x0000E000)	
	vcc_txendptr		= 0x34, 
#define   TXENDPTR_CLP		(0x00002000)
#define   TXENDPTR_MASK_PDUMODE	(0x0000C000)	
#define     PDUMODE_AAL0	 (0*0x04000)
#define     PDUMODE_AAL5	 (2*0x04000)
#define     PDUMODE_AAL5STREAM	 (3*0x04000)
	vcc_txwriteptr		= 0x38,	
#define   TXWRITEPTR_GET_PTR(x) ((x)&0x1FFF)
	vcc_txcbr_next		= 0x3C	
#define   TXCBR_NEXT_BOZO	(0x00008000)	
};

#define CARDVCC_SIZE	(0x40)

static inline bus_addr_t cardvcc_addr(const struct lanai_dev *lanai,
	vci_t vci)
{
	return sram_addr(lanai, vci * CARDVCC_SIZE);
}

static inline u32 cardvcc_read(const struct lanai_vcc *lvcc,
	enum lanai_vcc_offset offset)
{
	u32 val;
	APRINTK(lvcc->vbase != NULL, "cardvcc_read: unbound vcc!\n");
	val= readl(lvcc->vbase + offset);
	RWDEBUG("VR vci=%04d 0x%02X = 0x%08X\n",
	    lvcc->vci, (int) offset, val);
	return val;
}

static inline void cardvcc_write(const struct lanai_vcc *lvcc,
	u32 val, enum lanai_vcc_offset offset)
{
	APRINTK(lvcc->vbase != NULL, "cardvcc_write: unbound vcc!\n");
	APRINTK((val & ~0xFFFF) == 0,
	    "cardvcc_write: bad val 0x%X (vci=%d, addr=0x%02X)\n",
	    (unsigned int) val, lvcc->vci, (unsigned int) offset);
	RWDEBUG("VW vci=%04d 0x%02X > 0x%08X\n",
	    lvcc->vci, (unsigned int) offset, (unsigned int) val);
	writel(val, lvcc->vbase + offset);
}


static inline int aal5_size(int size)
{
	int cells = (size + 8 + 47) / 48;
	return cells * 48;
}

static inline int aal5_spacefor(int space)
{
	int cells = space / 48;
	return cells * 48;
}


static inline void lanai_free_skb(struct atm_vcc *atmvcc, struct sk_buff *skb)
{
	if (atmvcc->pop != NULL)
		atmvcc->pop(atmvcc, skb);
	else
		dev_kfree_skb_any(skb);
}


static void host_vcc_start_rx(const struct lanai_vcc *lvcc)
{
	u32 addr1;
	if (lvcc->rx.atmvcc->qos.aal == ATM_AAL5) {
		dma_addr_t dmaaddr = lvcc->rx.buf.dmaaddr;
		cardvcc_write(lvcc, 0xFFFF, vcc_rxcrc1);
		cardvcc_write(lvcc, 0xFFFF, vcc_rxcrc2);
		cardvcc_write(lvcc, 0, vcc_rxwriteptr);
		cardvcc_write(lvcc, 0, vcc_rxbufstart);
		cardvcc_write(lvcc, 0, vcc_rxreadptr);
		cardvcc_write(lvcc, (dmaaddr >> 16) & 0xFFFF, vcc_rxaddr2);
		addr1 = ((dmaaddr >> 8) & 0xFF) |
		    RXADDR1_SET_SIZE(lanai_buf_size_cardorder(&lvcc->rx.buf))|
		    RXADDR1_SET_RMMODE(RMMODE_TRASH) |	
		 
		    RXADDR1_SET_MODE(RXMODE_AAL5);
	} else
		addr1 = RXADDR1_SET_RMMODE(RMMODE_PRESERVE) | 
		    RXADDR1_OAM_PRESERVE |		      
		    RXADDR1_SET_MODE(RXMODE_AAL0);
	
	cardvcc_write(lvcc, addr1, vcc_rxaddr1);
}

static void host_vcc_start_tx(const struct lanai_vcc *lvcc)
{
	dma_addr_t dmaaddr = lvcc->tx.buf.dmaaddr;
	cardvcc_write(lvcc, 0, vcc_txicg);
	cardvcc_write(lvcc, 0xFFFF, vcc_txcrc1);
	cardvcc_write(lvcc, 0xFFFF, vcc_txcrc2);
	cardvcc_write(lvcc, 0, vcc_txreadptr);
	cardvcc_write(lvcc, 0, vcc_txendptr);
	cardvcc_write(lvcc, 0, vcc_txwriteptr);
	cardvcc_write(lvcc,
		(lvcc->tx.atmvcc->qos.txtp.traffic_class == ATM_CBR) ?
		TXCBR_NEXT_BOZO | lvcc->vci : 0, vcc_txcbr_next);
	cardvcc_write(lvcc, (dmaaddr >> 16) & 0xFFFF, vcc_txaddr2);
	cardvcc_write(lvcc,
	    ((dmaaddr >> 8) & 0xFF) |
	    TXADDR1_SET_SIZE(lanai_buf_size_cardorder(&lvcc->tx.buf)),
	    vcc_txaddr1);
}

static void lanai_shutdown_rx_vci(const struct lanai_vcc *lvcc)
{
	if (lvcc->vbase == NULL)	
		return;
	
	cardvcc_write(lvcc,
	    RXADDR1_SET_RMMODE(RMMODE_TRASH) |
	    RXADDR1_SET_MODE(RXMODE_TRASH), vcc_rxaddr1);
	udelay(15);
	
	cardvcc_write(lvcc, 0, vcc_rxaddr2);
	cardvcc_write(lvcc, 0, vcc_rxcrc1);
	cardvcc_write(lvcc, 0, vcc_rxcrc2);
	cardvcc_write(lvcc, 0, vcc_rxwriteptr);
	cardvcc_write(lvcc, 0, vcc_rxbufstart);
	cardvcc_write(lvcc, 0, vcc_rxreadptr);
}

static void lanai_shutdown_tx_vci(struct lanai_dev *lanai,
	struct lanai_vcc *lvcc)
{
	struct sk_buff *skb;
	unsigned long flags, timeout;
	int read, write, lastread = -1;
	APRINTK(!in_interrupt(),
	    "lanai_shutdown_tx_vci called w/o process context!\n");
	if (lvcc->vbase == NULL)	
		return;
	
	while ((skb = skb_dequeue(&lvcc->tx.backlog)) != NULL)
		lanai_free_skb(lvcc->tx.atmvcc, skb);
	read_lock_irqsave(&vcc_sklist_lock, flags);
	__clear_bit(lvcc->vci, lanai->backlog_vccs);
	read_unlock_irqrestore(&vcc_sklist_lock, flags);
	timeout = jiffies +
	    (((lanai_buf_size(&lvcc->tx.buf) / 1024) * HZ) >> 7);
	write = TXWRITEPTR_GET_PTR(cardvcc_read(lvcc, vcc_txwriteptr));
	for (;;) {
		read = TXREADPTR_GET_PTR(cardvcc_read(lvcc, vcc_txreadptr));
		if (read == write &&	   
		    (lvcc->tx.atmvcc->qos.txtp.traffic_class != ATM_CBR ||
		    (cardvcc_read(lvcc, vcc_txcbr_next) &
		    TXCBR_NEXT_BOZO) == 0))
			break;
		if (read != lastread) {	   
			lastread = read;
			timeout += HZ / 10;
		}
		if (unlikely(time_after(jiffies, timeout))) {
			printk(KERN_ERR DEV_LABEL "(itf %d): Timed out on "
			    "backlog closing vci %d\n",
			    lvcc->tx.atmvcc->dev->number, lvcc->vci);
			DPRINTK("read, write = %d, %d\n", read, write);
			break;
		}
		msleep(40);
	}
	
	cardvcc_write(lvcc, 0, vcc_txreadptr);
	cardvcc_write(lvcc, 0, vcc_txwriteptr);
	cardvcc_write(lvcc, 0, vcc_txendptr);
	cardvcc_write(lvcc, 0, vcc_txcrc1);
	cardvcc_write(lvcc, 0, vcc_txcrc2);
	cardvcc_write(lvcc, 0, vcc_txaddr2);
	cardvcc_write(lvcc, 0, vcc_txaddr1);
}


static inline int aal0_buffer_allocate(struct lanai_dev *lanai)
{
	DPRINTK("aal0_buffer_allocate: allocating AAL0 RX buffer\n");
	lanai_buf_allocate(&lanai->aal0buf, AAL0_RX_BUFFER_SIZE, 80,
			   lanai->pci);
	return (lanai->aal0buf.start == NULL) ? -ENOMEM : 0;
}

static inline void aal0_buffer_free(struct lanai_dev *lanai)
{
	DPRINTK("aal0_buffer_allocate: freeing AAL0 RX buffer\n");
	lanai_buf_deallocate(&lanai->aal0buf, lanai->pci);
}


#define EEPROM_COPYRIGHT	(0)
#define EEPROM_COPYRIGHT_LEN	(44)
#define EEPROM_CHECKSUM		(62)
#define EEPROM_CHECKSUM_REV	(63)
#define EEPROM_MAC		(64)
#define EEPROM_MAC_REV		(70)
#define EEPROM_SERIAL		(112)
#define EEPROM_SERIAL_REV	(116)
#define EEPROM_MAGIC		(120)
#define EEPROM_MAGIC_REV	(124)

#define EEPROM_MAGIC_VALUE	(0x5AB478D2)

#ifndef READ_EEPROM

static int __devinit eeprom_read(struct lanai_dev *lanai)
{
	printk(KERN_INFO DEV_LABEL "(itf %d): *NOT* reading EEPROM\n",
	    lanai->number);
	memset(&lanai->eeprom[EEPROM_MAC], 0, 6);
	return 0;
}

static int __devinit eeprom_validate(struct lanai_dev *lanai)
{
	lanai->serialno = 0;
	lanai->magicno = EEPROM_MAGIC_VALUE;
	return 0;
}

#else 

static int __devinit eeprom_read(struct lanai_dev *lanai)
{
	int i, address;
	u8 data;
	u32 tmp;
#define set_config1(x)   do { lanai->conf1 = x; conf1_write(lanai); \
			    } while (0)
#define clock_h()	 set_config1(lanai->conf1 | CONFIG1_PROMCLK)
#define clock_l()	 set_config1(lanai->conf1 &~ CONFIG1_PROMCLK)
#define data_h()	 set_config1(lanai->conf1 | CONFIG1_PROMDATA)
#define data_l()	 set_config1(lanai->conf1 &~ CONFIG1_PROMDATA)
#define pre_read()	 do { data_h(); clock_h(); udelay(5); } while (0)
#define read_pin()	 (reg_read(lanai, Status_Reg) & STATUS_PROMDATA)
#define send_stop()	 do { data_l(); udelay(5); clock_h(); udelay(5); \
			      data_h(); udelay(5); } while (0)
	
	data_h(); clock_h(); udelay(5);
	for (address = 0; address < LANAI_EEPROM_SIZE; address++) {
		data = (address << 1) | 1;	
		
		data_l(); udelay(5);
		clock_l(); udelay(5);
		for (i = 128; i != 0; i >>= 1) {   
			tmp = (lanai->conf1 & ~CONFIG1_PROMDATA) |
			    ((data & i) ? CONFIG1_PROMDATA : 0);
			if (lanai->conf1 != tmp) {
				set_config1(tmp);
				udelay(5);	
			}
			clock_h(); udelay(5); clock_l(); udelay(5);
		}
		
		data_h(); clock_h(); udelay(5);
		if (read_pin() != 0)
			goto error;	
		clock_l(); udelay(5);
		
		for (data = 0, i = 7; i >= 0; i--) {
			data_h(); clock_h(); udelay(5);
			data = (data << 1) | !!read_pin();
			clock_l(); udelay(5);
		}
		
		data_h(); clock_h(); udelay(5);
		if (read_pin() == 0)
			goto error;	
		clock_l(); udelay(5);
		send_stop();
		lanai->eeprom[address] = data;
		DPRINTK("EEPROM 0x%04X %02X\n",
		    (unsigned int) address, (unsigned int) data);
	}
	return 0;
    error:
	clock_l(); udelay(5);		
	send_stop();
	printk(KERN_ERR DEV_LABEL "(itf %d): error reading EEPROM byte %d\n",
	    lanai->number, address);
	return -EIO;
#undef set_config1
#undef clock_h
#undef clock_l
#undef data_h
#undef data_l
#undef pre_read
#undef read_pin
#undef send_stop
}

static inline u32 eeprom_be4(const struct lanai_dev *lanai, int address)
{
	return be32_to_cpup((const u32 *) &lanai->eeprom[address]);
}

static int __devinit eeprom_validate(struct lanai_dev *lanai)
{
	int i, s;
	u32 v;
	const u8 *e = lanai->eeprom;
#ifdef DEBUG
	/* First, see if we can get an ASCIIZ string out of the copyright */
	for (i = EEPROM_COPYRIGHT;
	    i < (EEPROM_COPYRIGHT + EEPROM_COPYRIGHT_LEN); i++)
		if (e[i] < 0x20 || e[i] > 0x7E)
			break;
	if ( i != EEPROM_COPYRIGHT &&
	    i != EEPROM_COPYRIGHT + EEPROM_COPYRIGHT_LEN && e[i] == '\0')
		DPRINTK("eeprom: copyright = \"%s\"\n",
		    (char *) &e[EEPROM_COPYRIGHT]);
	else
		DPRINTK("eeprom: copyright not found\n");
#endif
	
	for (i = s = 0; i < EEPROM_CHECKSUM; i++)
		s += e[i];
	s &= 0xFF;
	if (s != e[EEPROM_CHECKSUM]) {
		printk(KERN_ERR DEV_LABEL "(itf %d): EEPROM checksum bad "
		    "(wanted 0x%02X, got 0x%02X)\n", lanai->number,
		    (unsigned int) s, (unsigned int) e[EEPROM_CHECKSUM]);
		return -EIO;
	}
	s ^= 0xFF;
	if (s != e[EEPROM_CHECKSUM_REV]) {
		printk(KERN_ERR DEV_LABEL "(itf %d): EEPROM inverse checksum "
		    "bad (wanted 0x%02X, got 0x%02X)\n", lanai->number,
		    (unsigned int) s, (unsigned int) e[EEPROM_CHECKSUM_REV]);
		return -EIO;
	}
	
	for (i = 0; i < 6; i++)
		if ((e[EEPROM_MAC + i] ^ e[EEPROM_MAC_REV + i]) != 0xFF) {
			printk(KERN_ERR DEV_LABEL
			    "(itf %d) : EEPROM MAC addresses don't match "
			    "(0x%02X, inverse 0x%02X)\n", lanai->number,
			    (unsigned int) e[EEPROM_MAC + i],
			    (unsigned int) e[EEPROM_MAC_REV + i]);
			return -EIO;
		}
	DPRINTK("eeprom: MAC address = %pM\n", &e[EEPROM_MAC]);
	
	lanai->serialno = eeprom_be4(lanai, EEPROM_SERIAL);
	v = eeprom_be4(lanai, EEPROM_SERIAL_REV);
	if ((lanai->serialno ^ v) != 0xFFFFFFFF) {
		printk(KERN_ERR DEV_LABEL "(itf %d): EEPROM serial numbers "
		    "don't match (0x%08X, inverse 0x%08X)\n", lanai->number,
		    (unsigned int) lanai->serialno, (unsigned int) v);
		return -EIO;
	}
	DPRINTK("eeprom: Serial number = %d\n", (unsigned int) lanai->serialno);
	
	lanai->magicno = eeprom_be4(lanai, EEPROM_MAGIC);
	v = eeprom_be4(lanai, EEPROM_MAGIC_REV);
	if ((lanai->magicno ^ v) != 0xFFFFFFFF) {
		printk(KERN_ERR DEV_LABEL "(itf %d): EEPROM magic numbers "
		    "don't match (0x%08X, inverse 0x%08X)\n", lanai->number,
		    lanai->magicno, v);
		return -EIO;
	}
	DPRINTK("eeprom: Magic number = 0x%08X\n", lanai->magicno);
	if (lanai->magicno != EEPROM_MAGIC_VALUE)
		printk(KERN_WARNING DEV_LABEL "(itf %d): warning - EEPROM "
		    "magic not what expected (got 0x%08X, not 0x%08X)\n",
		    lanai->number, (unsigned int) lanai->magicno,
		    (unsigned int) EEPROM_MAGIC_VALUE);
	return 0;
}

#endif 

static inline const u8 *eeprom_mac(const struct lanai_dev *lanai)
{
	return &lanai->eeprom[EEPROM_MAC];
}


#define INT_STATS	(0x00000002)	
#define INT_SOOL	(0x00000004)	
#define INT_LOCD	(0x00000008)	
#define INT_LED		(0x00000010)	
#define INT_GPIN	(0x00000020)	
#define INT_PING	(0x00000040)	
#define INT_WAKE	(0x00000080)	
#define INT_CBR0	(0x00000100)	
#define INT_LOCK	(0x00000200)	
#define INT_MISMATCH	(0x00000400)	
#define INT_AAL0_STR	(0x00000800)	
#define INT_AAL0	(0x00001000)	
#define INT_SERVICE	(0x00002000)	
#define INT_TABORTSENT	(0x00004000)	
#define INT_TABORTBM	(0x00008000)	
#define INT_TIMEOUTBM	(0x00010000)	
#define INT_PCIPARITY	(0x00020000)	

#define INT_ALL		(0x0003FFFE)	
#define INT_STATUS	(0x0000003C)	
#define INT_DMASHUT	(0x00038000)	
#define INT_SEGSHUT	(0x00000700)	

static inline u32 intr_pending(const struct lanai_dev *lanai)
{
	return reg_read(lanai, IntStatusMasked_Reg);
}

static inline void intr_enable(const struct lanai_dev *lanai, u32 i)
{
	reg_write(lanai, i, IntControlEna_Reg);
}

static inline void intr_disable(const struct lanai_dev *lanai, u32 i)
{
	reg_write(lanai, i, IntControlDis_Reg);
}


static void status_message(int itf, const char *name, int status)
{
	static const char *onoff[2] = { "off to on", "on to off" };
	printk(KERN_INFO DEV_LABEL "(itf %d): %s changed from %s\n",
	    itf, name, onoff[!status]);
}

static void lanai_check_status(struct lanai_dev *lanai)
{
	u32 new = reg_read(lanai, Status_Reg);
	u32 changes = new ^ lanai->status;
	lanai->status = new;
#define e(flag, name) \
		if (changes & flag) \
			status_message(lanai->number, name, new & flag)
	e(STATUS_SOOL, "SOOL");
	e(STATUS_LOCD, "LOCD");
	e(STATUS_LED, "LED");
	e(STATUS_GPIN, "GPIN");
#undef e
}

static void pcistatus_got(int itf, const char *name)
{
	printk(KERN_INFO DEV_LABEL "(itf %d): PCI got %s error\n", itf, name);
}

static void pcistatus_check(struct lanai_dev *lanai, int clearonly)
{
	u16 s;
	int result;
	result = pci_read_config_word(lanai->pci, PCI_STATUS, &s);
	if (result != PCIBIOS_SUCCESSFUL) {
		printk(KERN_ERR DEV_LABEL "(itf %d): can't read PCI_STATUS: "
		    "%d\n", lanai->number, result);
		return;
	}
	s &= PCI_STATUS_DETECTED_PARITY | PCI_STATUS_SIG_SYSTEM_ERROR |
	    PCI_STATUS_REC_MASTER_ABORT | PCI_STATUS_REC_TARGET_ABORT |
	    PCI_STATUS_SIG_TARGET_ABORT | PCI_STATUS_PARITY;
	if (s == 0)
		return;
	result = pci_write_config_word(lanai->pci, PCI_STATUS, s);
	if (result != PCIBIOS_SUCCESSFUL)
		printk(KERN_ERR DEV_LABEL "(itf %d): can't write PCI_STATUS: "
		    "%d\n", lanai->number, result);
	if (clearonly)
		return;
#define e(flag, name, stat) \
		if (s & flag) { \
			pcistatus_got(lanai->number, name); \
			++lanai->stats.pcierr_##stat; \
		}
	e(PCI_STATUS_DETECTED_PARITY, "parity", parity_detect);
	e(PCI_STATUS_SIG_SYSTEM_ERROR, "signalled system", serr_set);
	e(PCI_STATUS_REC_MASTER_ABORT, "master", master_abort);
	e(PCI_STATUS_REC_TARGET_ABORT, "master target", m_target_abort);
	e(PCI_STATUS_SIG_TARGET_ABORT, "slave", s_target_abort);
	e(PCI_STATUS_PARITY, "master parity", master_parity);
#undef e
}


static inline int vcc_tx_space(const struct lanai_vcc *lvcc, int endptr)
{
	int r;
	r = endptr * 16;
	r -= ((unsigned long) lvcc->tx.buf.ptr) -
	    ((unsigned long) lvcc->tx.buf.start);
	r -= 16;	
	if (r < 0)
		r += lanai_buf_size(&lvcc->tx.buf);
	return r;
}

static inline int vcc_is_backlogged(const struct lanai_vcc *lvcc)
{
	return !skb_queue_empty(&lvcc->tx.backlog);
}

#define DESCRIPTOR_MAGIC	(0xD0000000)
#define DESCRIPTOR_AAL5		(0x00008000)
#define DESCRIPTOR_AAL5_STREAM	(0x00004000)
#define DESCRIPTOR_CLP		(0x00002000)

static inline void vcc_tx_add_aal5_descriptor(struct lanai_vcc *lvcc,
	u32 flags, int len)
{
	int pos;
	APRINTK((((unsigned long) lvcc->tx.buf.ptr) & 15) == 0,
	    "vcc_tx_add_aal5_descriptor: bad ptr=%p\n", lvcc->tx.buf.ptr);
	lvcc->tx.buf.ptr += 4;	
	pos = ((unsigned char *) lvcc->tx.buf.ptr) -
	    (unsigned char *) lvcc->tx.buf.start;
	APRINTK((pos & ~0x0001FFF0) == 0,
	    "vcc_tx_add_aal5_descriptor: bad pos (%d) before, vci=%d, "
	    "start,ptr,end=%p,%p,%p\n", pos, lvcc->vci,
	    lvcc->tx.buf.start, lvcc->tx.buf.ptr, lvcc->tx.buf.end);
	pos = (pos + len) & (lanai_buf_size(&lvcc->tx.buf) - 1);
	APRINTK((pos & ~0x0001FFF0) == 0,
	    "vcc_tx_add_aal5_descriptor: bad pos (%d) after, vci=%d, "
	    "start,ptr,end=%p,%p,%p\n", pos, lvcc->vci,
	    lvcc->tx.buf.start, lvcc->tx.buf.ptr, lvcc->tx.buf.end);
	lvcc->tx.buf.ptr[-1] =
	    cpu_to_le32(DESCRIPTOR_MAGIC | DESCRIPTOR_AAL5 |
	    ((lvcc->tx.atmvcc->atm_options & ATM_ATMOPT_CLP) ?
	    DESCRIPTOR_CLP : 0) | flags | pos >> 4);
	if (lvcc->tx.buf.ptr >= lvcc->tx.buf.end)
		lvcc->tx.buf.ptr = lvcc->tx.buf.start;
}

static inline void vcc_tx_add_aal5_trailer(struct lanai_vcc *lvcc,
	int len, int cpi, int uu)
{
	APRINTK((((unsigned long) lvcc->tx.buf.ptr) & 15) == 8,
	    "vcc_tx_add_aal5_trailer: bad ptr=%p\n", lvcc->tx.buf.ptr);
	lvcc->tx.buf.ptr += 2;
	lvcc->tx.buf.ptr[-2] = cpu_to_be32((uu << 24) | (cpi << 16) | len);
	if (lvcc->tx.buf.ptr >= lvcc->tx.buf.end)
		lvcc->tx.buf.ptr = lvcc->tx.buf.start;
}

static inline void vcc_tx_memcpy(struct lanai_vcc *lvcc,
	const unsigned char *src, int n)
{
	unsigned char *e;
	int m;
	e = ((unsigned char *) lvcc->tx.buf.ptr) + n;
	m = e - (unsigned char *) lvcc->tx.buf.end;
	if (m < 0)
		m = 0;
	memcpy(lvcc->tx.buf.ptr, src, n - m);
	if (m != 0) {
		memcpy(lvcc->tx.buf.start, src + n - m, m);
		e = ((unsigned char *) lvcc->tx.buf.start) + m;
	}
	lvcc->tx.buf.ptr = (u32 *) e;
}

static inline void vcc_tx_memzero(struct lanai_vcc *lvcc, int n)
{
	unsigned char *e;
	int m;
	if (n == 0)
		return;
	e = ((unsigned char *) lvcc->tx.buf.ptr) + n;
	m = e - (unsigned char *) lvcc->tx.buf.end;
	if (m < 0)
		m = 0;
	memset(lvcc->tx.buf.ptr, 0, n - m);
	if (m != 0) {
		memset(lvcc->tx.buf.start, 0, m);
		e = ((unsigned char *) lvcc->tx.buf.start) + m;
	}
	lvcc->tx.buf.ptr = (u32 *) e;
}

static inline void lanai_endtx(struct lanai_dev *lanai,
	const struct lanai_vcc *lvcc)
{
	int i, ptr = ((unsigned char *) lvcc->tx.buf.ptr) -
	    (unsigned char *) lvcc->tx.buf.start;
	APRINTK((ptr & ~0x0001FFF0) == 0,
	    "lanai_endtx: bad ptr (%d), vci=%d, start,ptr,end=%p,%p,%p\n",
	    ptr, lvcc->vci, lvcc->tx.buf.start, lvcc->tx.buf.ptr,
	    lvcc->tx.buf.end);

	spin_lock(&lanai->endtxlock);
	for (i = 0; reg_read(lanai, Status_Reg) & STATUS_BUTTBUSY; i++) {
		if (unlikely(i > 50)) {
			printk(KERN_ERR DEV_LABEL "(itf %d): butt register "
			    "always busy!\n", lanai->number);
			break;
		}
		udelay(5);
	}
	/*
	 * Before we tall the card to start work we need to be sure 100% of
	 * the info in the service buffer has been written before we tell
	 * the card about it
	 */
	wmb();
	reg_write(lanai, (ptr << 12) | lvcc->vci, Butt_Reg);
	spin_unlock(&lanai->endtxlock);
}

static void lanai_send_one_aal5(struct lanai_dev *lanai,
	struct lanai_vcc *lvcc, struct sk_buff *skb, int pdusize)
{
	int pad;
	APRINTK(pdusize == aal5_size(skb->len),
	    "lanai_send_one_aal5: wrong size packet (%d != %d)\n",
	    pdusize, aal5_size(skb->len));
	vcc_tx_add_aal5_descriptor(lvcc, 0, pdusize);
	pad = pdusize - skb->len - 8;
	APRINTK(pad >= 0, "pad is negative (%d)\n", pad);
	APRINTK(pad < 48, "pad is too big (%d)\n", pad);
	vcc_tx_memcpy(lvcc, skb->data, skb->len);
	vcc_tx_memzero(lvcc, pad);
	vcc_tx_add_aal5_trailer(lvcc, skb->len, 0, 0);
	lanai_endtx(lanai, lvcc);
	lanai_free_skb(lvcc->tx.atmvcc, skb);
	atomic_inc(&lvcc->tx.atmvcc->stats->tx);
}

static void vcc_tx_unqueue_aal5(struct lanai_dev *lanai,
	struct lanai_vcc *lvcc, int endptr)
{
	int n;
	struct sk_buff *skb;
	int space = vcc_tx_space(lvcc, endptr);
	APRINTK(vcc_is_backlogged(lvcc),
	    "vcc_tx_unqueue() called with empty backlog (vci=%d)\n",
	    lvcc->vci);
	while (space >= 64) {
		skb = skb_dequeue(&lvcc->tx.backlog);
		if (skb == NULL)
			goto no_backlog;
		n = aal5_size(skb->len);
		if (n + 16 > space) {
			
			skb_queue_head(&lvcc->tx.backlog, skb);
			return;
		}
		lanai_send_one_aal5(lanai, lvcc, skb, n);
		space -= n + 16;
	}
	if (!vcc_is_backlogged(lvcc)) {
	    no_backlog:
		__clear_bit(lvcc->vci, lanai->backlog_vccs);
	}
}

static void vcc_tx_aal5(struct lanai_dev *lanai, struct lanai_vcc *lvcc,
	struct sk_buff *skb)
{
	int space, n;
	if (vcc_is_backlogged(lvcc))		
		goto queue_it;
	space = vcc_tx_space(lvcc,
		    TXREADPTR_GET_PTR(cardvcc_read(lvcc, vcc_txreadptr)));
	n = aal5_size(skb->len);
	APRINTK(n + 16 >= 64, "vcc_tx_aal5: n too small (%d)\n", n);
	if (space < n + 16) {			
		__set_bit(lvcc->vci, lanai->backlog_vccs);
	    queue_it:
		skb_queue_tail(&lvcc->tx.backlog, skb);
		return;
	}
	lanai_send_one_aal5(lanai, lvcc, skb, n);
}

static void vcc_tx_unqueue_aal0(struct lanai_dev *lanai,
	struct lanai_vcc *lvcc, int endptr)
{
	printk(KERN_INFO DEV_LABEL
	    ": vcc_tx_unqueue_aal0: not implemented\n");
}

static void vcc_tx_aal0(struct lanai_dev *lanai, struct lanai_vcc *lvcc,
	struct sk_buff *skb)
{
	printk(KERN_INFO DEV_LABEL ": vcc_tx_aal0: not implemented\n");
	
	lanai_free_skb(lvcc->tx.atmvcc, skb);
}


static inline void vcc_rx_memcpy(unsigned char *dest,
	const struct lanai_vcc *lvcc, int n)
{
	int m = ((const unsigned char *) lvcc->rx.buf.ptr) + n -
	    ((const unsigned char *) (lvcc->rx.buf.end));
	if (m < 0)
		m = 0;
	memcpy(dest, lvcc->rx.buf.ptr, n - m);
	memcpy(dest + n - m, lvcc->rx.buf.start, m);
	
	barrier();
}

static void vcc_rx_aal5(struct lanai_vcc *lvcc, int endptr)
{
	int size;
	struct sk_buff *skb;
	const u32 *x;
	u32 *end = &lvcc->rx.buf.start[endptr * 4];
	int n = ((unsigned long) end) - ((unsigned long) lvcc->rx.buf.ptr);
	if (n < 0)
		n += lanai_buf_size(&lvcc->rx.buf);
	APRINTK(n >= 0 && n < lanai_buf_size(&lvcc->rx.buf) && !(n & 15),
	    "vcc_rx_aal5: n out of range (%d/%Zu)\n",
	    n, lanai_buf_size(&lvcc->rx.buf));
	
	if ((x = &end[-2]) < lvcc->rx.buf.start)
		x = &lvcc->rx.buf.end[-2];
	rmb();
	size = be32_to_cpup(x) & 0xffff;
	if (unlikely(n != aal5_size(size))) {
		
		printk(KERN_INFO DEV_LABEL "(itf %d): Got bad AAL5 length "
		    "on vci=%d - size=%d n=%d\n",
		    lvcc->rx.atmvcc->dev->number, lvcc->vci, size, n);
		lvcc->stats.x.aal5.rx_badlen++;
		goto out;
	}
	skb = atm_alloc_charge(lvcc->rx.atmvcc, size, GFP_ATOMIC);
	if (unlikely(skb == NULL)) {
		lvcc->stats.rx_nomem++;
		goto out;
	}
	skb_put(skb, size);
	vcc_rx_memcpy(skb->data, lvcc, size);
	ATM_SKB(skb)->vcc = lvcc->rx.atmvcc;
	__net_timestamp(skb);
	lvcc->rx.atmvcc->push(lvcc->rx.atmvcc, skb);
	atomic_inc(&lvcc->rx.atmvcc->stats->rx);
    out:
	lvcc->rx.buf.ptr = end;
	cardvcc_write(lvcc, endptr, vcc_rxreadptr);
}

static void vcc_rx_aal0(struct lanai_dev *lanai)
{
	printk(KERN_INFO DEV_LABEL ": vcc_rx_aal0: not implemented\n");
	
	
}


#if (NUM_VCI * BITS_PER_LONG) <= PAGE_SIZE
#define VCCTABLE_GETFREEPAGE
#else
#include <linux/vmalloc.h>
#endif

static int __devinit vcc_table_allocate(struct lanai_dev *lanai)
{
#ifdef VCCTABLE_GETFREEPAGE
	APRINTK((lanai->num_vci) * sizeof(struct lanai_vcc *) <= PAGE_SIZE,
	    "vcc table > PAGE_SIZE!");
	lanai->vccs = (struct lanai_vcc **) get_zeroed_page(GFP_KERNEL);
	return (lanai->vccs == NULL) ? -ENOMEM : 0;
#else
	int bytes = (lanai->num_vci) * sizeof(struct lanai_vcc *);
	lanai->vccs = vzalloc(bytes);
	if (unlikely(lanai->vccs == NULL))
		return -ENOMEM;
	return 0;
#endif
}

static inline void vcc_table_deallocate(const struct lanai_dev *lanai)
{
#ifdef VCCTABLE_GETFREEPAGE
	free_page((unsigned long) lanai->vccs);
#else
	vfree(lanai->vccs);
#endif
}

static inline struct lanai_vcc *new_lanai_vcc(void)
{
	struct lanai_vcc *lvcc;
	lvcc =  kzalloc(sizeof(*lvcc), GFP_KERNEL);
	if (likely(lvcc != NULL)) {
		skb_queue_head_init(&lvcc->tx.backlog);
#ifdef DEBUG
		lvcc->vci = -1;
#endif
	}
	return lvcc;
}

static int lanai_get_sized_buffer(struct lanai_dev *lanai,
	struct lanai_buffer *buf, int max_sdu, int multiplier,
	const char *name)
{
	int size;
	if (unlikely(max_sdu < 1))
		max_sdu = 1;
	max_sdu = aal5_size(max_sdu);
	size = (max_sdu + 16) * multiplier + 16;
	lanai_buf_allocate(buf, size, max_sdu + 32, lanai->pci);
	if (unlikely(buf->start == NULL))
		return -ENOMEM;
	if (unlikely(lanai_buf_size(buf) < size))
		printk(KERN_WARNING DEV_LABEL "(itf %d): wanted %d bytes "
		    "for %s buffer, got only %Zu\n", lanai->number, size,
		    name, lanai_buf_size(buf));
	DPRINTK("Allocated %Zu byte %s buffer\n", lanai_buf_size(buf), name);
	return 0;
}

static inline int lanai_setup_rx_vci_aal5(struct lanai_dev *lanai,
	struct lanai_vcc *lvcc, const struct atm_qos *qos)
{
	return lanai_get_sized_buffer(lanai, &lvcc->rx.buf,
	    qos->rxtp.max_sdu, AAL5_RX_MULTIPLIER, "RX");
}

static int lanai_setup_tx_vci(struct lanai_dev *lanai, struct lanai_vcc *lvcc,
	const struct atm_qos *qos)
{
	int max_sdu, multiplier;
	if (qos->aal == ATM_AAL0) {
		lvcc->tx.unqueue = vcc_tx_unqueue_aal0;
		max_sdu = ATM_CELL_SIZE - 1;
		multiplier = AAL0_TX_MULTIPLIER;
	} else {
		lvcc->tx.unqueue = vcc_tx_unqueue_aal5;
		max_sdu = qos->txtp.max_sdu;
		multiplier = AAL5_TX_MULTIPLIER;
	}
	return lanai_get_sized_buffer(lanai, &lvcc->tx.buf, max_sdu,
	    multiplier, "TX");
}

static inline void host_vcc_bind(struct lanai_dev *lanai,
	struct lanai_vcc *lvcc, vci_t vci)
{
	if (lvcc->vbase != NULL)
		return;    
	DPRINTK("Binding vci %d\n", vci);
#ifdef USE_POWERDOWN
	if (lanai->nbound++ == 0) {
		DPRINTK("Coming out of powerdown\n");
		lanai->conf1 &= ~CONFIG1_POWERDOWN;
		conf1_write(lanai);
		conf2_write(lanai);
	}
#endif
	lvcc->vbase = cardvcc_addr(lanai, vci);
	lanai->vccs[lvcc->vci = vci] = lvcc;
}

static inline void host_vcc_unbind(struct lanai_dev *lanai,
	struct lanai_vcc *lvcc)
{
	if (lvcc->vbase == NULL)
		return;	
	DPRINTK("Unbinding vci %d\n", lvcc->vci);
	lvcc->vbase = NULL;
	lanai->vccs[lvcc->vci] = NULL;
#ifdef USE_POWERDOWN
	if (--lanai->nbound == 0) {
		DPRINTK("Going into powerdown\n");
		lanai->conf1 |= CONFIG1_POWERDOWN;
		conf1_write(lanai);
	}
#endif
}


static void lanai_reset(struct lanai_dev *lanai)
{
	printk(KERN_CRIT DEV_LABEL "(itf %d): *NOT* resetting - not "
	    "implemented\n", lanai->number);
	
	reg_write(lanai, INT_ALL, IntAck_Reg);
	lanai->stats.card_reset++;
}


static int __devinit service_buffer_allocate(struct lanai_dev *lanai)
{
	lanai_buf_allocate(&lanai->service, SERVICE_ENTRIES * 4, 8,
	    lanai->pci);
	if (unlikely(lanai->service.start == NULL))
		return -ENOMEM;
	DPRINTK("allocated service buffer at 0x%08lX, size %Zu(%d)\n",
	    (unsigned long) lanai->service.start,
	    lanai_buf_size(&lanai->service),
	    lanai_buf_size_cardorder(&lanai->service));
	
	reg_write(lanai, 0, ServWrite_Reg);
	
	reg_write(lanai,
	    SSTUFF_SET_SIZE(lanai_buf_size_cardorder(&lanai->service)) |
	    SSTUFF_SET_ADDR(lanai->service.dmaaddr),
	    ServiceStuff_Reg);
	return 0;
}

static inline void service_buffer_deallocate(struct lanai_dev *lanai)
{
	lanai_buf_deallocate(&lanai->service, lanai->pci);
}

#define SERVICE_TX	(0x80000000)	
#define SERVICE_TRASH	(0x40000000)	
#define SERVICE_CRCERR	(0x20000000)	
#define SERVICE_CI	(0x10000000)	
#define SERVICE_CLP	(0x08000000)	
#define SERVICE_STREAM	(0x04000000)	
#define SERVICE_GET_VCI(x) (((x)>>16)&0x3FF)
#define SERVICE_GET_END(x) ((x)&0x1FFF)

static int handle_service(struct lanai_dev *lanai, u32 s)
{
	vci_t vci = SERVICE_GET_VCI(s);
	struct lanai_vcc *lvcc;
	read_lock(&vcc_sklist_lock);
	lvcc = lanai->vccs[vci];
	if (unlikely(lvcc == NULL)) {
		read_unlock(&vcc_sklist_lock);
		DPRINTK("(itf %d) got service entry 0x%X for nonexistent "
		    "vcc %d\n", lanai->number, (unsigned int) s, vci);
		if (s & SERVICE_TX)
			lanai->stats.service_notx++;
		else
			lanai->stats.service_norx++;
		return 0;
	}
	if (s & SERVICE_TX) {			
		if (unlikely(lvcc->tx.atmvcc == NULL)) {
			read_unlock(&vcc_sklist_lock);
			DPRINTK("(itf %d) got service entry 0x%X for non-TX "
			    "vcc %d\n", lanai->number, (unsigned int) s, vci);
			lanai->stats.service_notx++;
			return 0;
		}
		__set_bit(vci, lanai->transmit_ready);
		lvcc->tx.endptr = SERVICE_GET_END(s);
		read_unlock(&vcc_sklist_lock);
		return 1;
	}
	if (unlikely(lvcc->rx.atmvcc == NULL)) {
		read_unlock(&vcc_sklist_lock);
		DPRINTK("(itf %d) got service entry 0x%X for non-RX "
		    "vcc %d\n", lanai->number, (unsigned int) s, vci);
		lanai->stats.service_norx++;
		return 0;
	}
	if (unlikely(lvcc->rx.atmvcc->qos.aal != ATM_AAL5)) {
		read_unlock(&vcc_sklist_lock);
		DPRINTK("(itf %d) got RX service entry 0x%X for non-AAL5 "
		    "vcc %d\n", lanai->number, (unsigned int) s, vci);
		lanai->stats.service_rxnotaal5++;
		atomic_inc(&lvcc->rx.atmvcc->stats->rx_err);
		return 0;
	}
	if (likely(!(s & (SERVICE_TRASH | SERVICE_STREAM | SERVICE_CRCERR)))) {
		vcc_rx_aal5(lvcc, SERVICE_GET_END(s));
		read_unlock(&vcc_sklist_lock);
		return 0;
	}
	if (s & SERVICE_TRASH) {
		int bytes;
		read_unlock(&vcc_sklist_lock);
		DPRINTK("got trashed rx pdu on vci %d\n", vci);
		atomic_inc(&lvcc->rx.atmvcc->stats->rx_err);
		lvcc->stats.x.aal5.service_trash++;
		bytes = (SERVICE_GET_END(s) * 16) -
		    (((unsigned long) lvcc->rx.buf.ptr) -
		    ((unsigned long) lvcc->rx.buf.start)) + 47;
		if (bytes < 0)
			bytes += lanai_buf_size(&lvcc->rx.buf);
		lanai->stats.ovfl_trash += (bytes / 48);
		return 0;
	}
	if (s & SERVICE_STREAM) {
		read_unlock(&vcc_sklist_lock);
		atomic_inc(&lvcc->rx.atmvcc->stats->rx_err);
		lvcc->stats.x.aal5.service_stream++;
		printk(KERN_ERR DEV_LABEL "(itf %d): Got AAL5 stream "
		    "PDU on VCI %d!\n", lanai->number, vci);
		lanai_reset(lanai);
		return 0;
	}
	DPRINTK("got rx crc error on vci %d\n", vci);
	atomic_inc(&lvcc->rx.atmvcc->stats->rx_err);
	lvcc->stats.x.aal5.service_rxcrc++;
	lvcc->rx.buf.ptr = &lvcc->rx.buf.start[SERVICE_GET_END(s) * 4];
	cardvcc_write(lvcc, SERVICE_GET_END(s), vcc_rxreadptr);
	read_unlock(&vcc_sklist_lock);
	return 0;
}

static void iter_transmit(struct lanai_dev *lanai, vci_t vci)
{
	struct lanai_vcc *lvcc = lanai->vccs[vci];
	if (vcc_is_backlogged(lvcc))
		lvcc->tx.unqueue(lanai, lvcc, lvcc->tx.endptr);
}

static void run_service(struct lanai_dev *lanai)
{
	int ntx = 0;
	u32 wreg = reg_read(lanai, ServWrite_Reg);
	const u32 *end = lanai->service.start + wreg;
	while (lanai->service.ptr != end) {
		ntx += handle_service(lanai,
		    le32_to_cpup(lanai->service.ptr++));
		if (lanai->service.ptr >= lanai->service.end)
			lanai->service.ptr = lanai->service.start;
	}
	reg_write(lanai, wreg, ServRead_Reg);
	if (ntx != 0) {
		read_lock(&vcc_sklist_lock);
		vci_bitfield_iterate(lanai, lanai->transmit_ready,
		    iter_transmit);
		bitmap_zero(lanai->transmit_ready, NUM_VCI);
		read_unlock(&vcc_sklist_lock);
	}
}


static void get_statistics(struct lanai_dev *lanai)
{
	u32 statreg = reg_read(lanai, Statistics_Reg);
	lanai->stats.atm_ovfl += STATS_GET_FIFO_OVFL(statreg);
	lanai->stats.hec_err += STATS_GET_HEC_ERR(statreg);
	lanai->stats.vci_trash += STATS_GET_BAD_VCI(statreg);
	lanai->stats.ovfl_trash += STATS_GET_BUF_OVFL(statreg);
}


#ifndef DEBUG_RW
static void iter_dequeue(struct lanai_dev *lanai, vci_t vci)
{
	struct lanai_vcc *lvcc = lanai->vccs[vci];
	int endptr;
	if (lvcc == NULL || lvcc->tx.atmvcc == NULL ||
	    !vcc_is_backlogged(lvcc)) {
		__clear_bit(vci, lanai->backlog_vccs);
		return;
	}
	endptr = TXREADPTR_GET_PTR(cardvcc_read(lvcc, vcc_txreadptr));
	lvcc->tx.unqueue(lanai, lvcc, endptr);
}
#endif 

static void lanai_timed_poll(unsigned long arg)
{
	struct lanai_dev *lanai = (struct lanai_dev *) arg;
#ifndef DEBUG_RW
	unsigned long flags;
#ifdef USE_POWERDOWN
	if (lanai->conf1 & CONFIG1_POWERDOWN)
		return;
#endif 
	local_irq_save(flags);
	
	if (spin_trylock(&lanai->servicelock)) {
		run_service(lanai);
		spin_unlock(&lanai->servicelock);
	}
	
	
	read_lock(&vcc_sklist_lock);
	vci_bitfield_iterate(lanai, lanai->backlog_vccs, iter_dequeue);
	read_unlock(&vcc_sklist_lock);
	local_irq_restore(flags);

	get_statistics(lanai);
#endif 
	mod_timer(&lanai->timer, jiffies + LANAI_POLL_PERIOD);
}

static inline void lanai_timed_poll_start(struct lanai_dev *lanai)
{
	init_timer(&lanai->timer);
	lanai->timer.expires = jiffies + LANAI_POLL_PERIOD;
	lanai->timer.data = (unsigned long) lanai;
	lanai->timer.function = lanai_timed_poll;
	add_timer(&lanai->timer);
}

static inline void lanai_timed_poll_stop(struct lanai_dev *lanai)
{
	del_timer_sync(&lanai->timer);
}


static inline void lanai_int_1(struct lanai_dev *lanai, u32 reason)
{
	u32 ack = 0;
	if (reason & INT_SERVICE) {
		ack = INT_SERVICE;
		spin_lock(&lanai->servicelock);
		run_service(lanai);
		spin_unlock(&lanai->servicelock);
	}
	if (reason & (INT_AAL0_STR | INT_AAL0)) {
		ack |= reason & (INT_AAL0_STR | INT_AAL0);
		vcc_rx_aal0(lanai);
	}
	
	if (ack == reason)
		goto done;
	if (reason & INT_STATS) {
		reason &= ~INT_STATS;	
		get_statistics(lanai);
	}
	if (reason & INT_STATUS) {
		ack |= reason & INT_STATUS;
		lanai_check_status(lanai);
	}
	if (unlikely(reason & INT_DMASHUT)) {
		printk(KERN_ERR DEV_LABEL "(itf %d): driver error - DMA "
		    "shutdown, reason=0x%08X, address=0x%08X\n",
		    lanai->number, (unsigned int) (reason & INT_DMASHUT),
		    (unsigned int) reg_read(lanai, DMA_Addr_Reg));
		if (reason & INT_TABORTBM) {
			lanai_reset(lanai);
			return;
		}
		ack |= (reason & INT_DMASHUT);
		printk(KERN_ERR DEV_LABEL "(itf %d): re-enabling DMA\n",
		    lanai->number);
		conf1_write(lanai);
		lanai->stats.dma_reenable++;
		pcistatus_check(lanai, 0);
	}
	if (unlikely(reason & INT_TABORTSENT)) {
		ack |= (reason & INT_TABORTSENT);
		printk(KERN_ERR DEV_LABEL "(itf %d): sent PCI target abort\n",
		    lanai->number);
		pcistatus_check(lanai, 0);
	}
	if (unlikely(reason & INT_SEGSHUT)) {
		printk(KERN_ERR DEV_LABEL "(itf %d): driver error - "
		    "segmentation shutdown, reason=0x%08X\n", lanai->number,
		    (unsigned int) (reason & INT_SEGSHUT));
		lanai_reset(lanai);
		return;
	}
	if (unlikely(reason & (INT_PING | INT_WAKE))) {
		printk(KERN_ERR DEV_LABEL "(itf %d): driver error - "
		    "unexpected interrupt 0x%08X, resetting\n",
		    lanai->number,
		    (unsigned int) (reason & (INT_PING | INT_WAKE)));
		lanai_reset(lanai);
		return;
	}
#ifdef DEBUG
	if (unlikely(ack != reason)) {
		DPRINTK("unacked ints: 0x%08X\n",
		    (unsigned int) (reason & ~ack));
		ack = reason;
	}
#endif
   done:
	if (ack != 0)
		reg_write(lanai, ack, IntAck_Reg);
}

static irqreturn_t lanai_int(int irq, void *devid)
{
	struct lanai_dev *lanai = devid;
	u32 reason;

#ifdef USE_POWERDOWN
	if (unlikely(lanai->conf1 & CONFIG1_POWERDOWN))
		return IRQ_NONE;
#endif

	reason = intr_pending(lanai);
	if (reason == 0)
		return IRQ_NONE;	

	do {
		if (unlikely(reason == 0xFFFFFFFF))
			break;		
		lanai_int_1(lanai, reason);
		reason = intr_pending(lanai);
	} while (reason != 0);

	return IRQ_HANDLED;
}



static int check_board_id_and_rev(const char *name, u32 val, int *revp)
{
	DPRINTK("%s says board_id=%d, board_rev=%d\n", name,
		(int) RESET_GET_BOARD_ID(val),
		(int) RESET_GET_BOARD_REV(val));
	if (RESET_GET_BOARD_ID(val) != BOARD_ID_LANAI256) {
		printk(KERN_ERR DEV_LABEL ": Found %s board-id %d -- not a "
		    "Lanai 25.6\n", name, (int) RESET_GET_BOARD_ID(val));
		return -ENODEV;
	}
	if (revp != NULL)
		*revp = RESET_GET_BOARD_REV(val);
	return 0;
}


static int __devinit lanai_pci_start(struct lanai_dev *lanai)
{
	struct pci_dev *pci = lanai->pci;
	int result;

	if (pci_enable_device(pci) != 0) {
		printk(KERN_ERR DEV_LABEL "(itf %d): can't enable "
		    "PCI device", lanai->number);
		return -ENXIO;
	}
	pci_set_master(pci);
	if (pci_set_dma_mask(pci, DMA_BIT_MASK(32)) != 0) {
		printk(KERN_WARNING DEV_LABEL
		    "(itf %d): No suitable DMA available.\n", lanai->number);
		return -EBUSY;
	}
	if (pci_set_consistent_dma_mask(pci, DMA_BIT_MASK(32)) != 0) {
		printk(KERN_WARNING DEV_LABEL
		    "(itf %d): No suitable DMA available.\n", lanai->number);
		return -EBUSY;
	}
	result = check_board_id_and_rev("PCI", pci->subsystem_device, NULL);
	if (result != 0)
		return result;
	
	result = pci_write_config_byte(pci, PCI_LATENCY_TIMER, 0);
	if (result != PCIBIOS_SUCCESSFUL) {
		printk(KERN_ERR DEV_LABEL "(itf %d): can't write "
		    "PCI_LATENCY_TIMER: %d\n", lanai->number, result);
		return -EINVAL;
	}
	pcistatus_check(lanai, 1);
	pcistatus_check(lanai, 0);
	return 0;
}


static inline int vci0_is_ok(struct lanai_dev *lanai,
	const struct atm_qos *qos)
{
	if (qos->txtp.traffic_class == ATM_CBR || qos->aal == ATM_AAL0)
		return 0;
	if (qos->rxtp.traffic_class != ATM_NONE) {
		if (lanai->naal0 != 0)
			return 0;
		lanai->conf2 |= CONFIG2_VCI0_NORMAL;
		conf2_write_if_powerup(lanai);
	}
	return 1;
}

static int vci_is_ok(struct lanai_dev *lanai, vci_t vci,
	const struct atm_vcc *atmvcc)
{
	const struct atm_qos *qos = &atmvcc->qos;
	const struct lanai_vcc *lvcc = lanai->vccs[vci];
	if (vci == 0 && !vci0_is_ok(lanai, qos))
		return 0;
	if (unlikely(lvcc != NULL)) {
		if (qos->rxtp.traffic_class != ATM_NONE &&
		    lvcc->rx.atmvcc != NULL && lvcc->rx.atmvcc != atmvcc)
			return 0;
		if (qos->txtp.traffic_class != ATM_NONE &&
		    lvcc->tx.atmvcc != NULL && lvcc->tx.atmvcc != atmvcc)
			return 0;
		if (qos->txtp.traffic_class == ATM_CBR &&
		    lanai->cbrvcc != NULL && lanai->cbrvcc != atmvcc)
			return 0;
	}
	if (qos->aal == ATM_AAL0 && lanai->naal0 == 0 &&
	    qos->rxtp.traffic_class != ATM_NONE) {
		const struct lanai_vcc *vci0 = lanai->vccs[0];
		if (vci0 != NULL && vci0->rx.atmvcc != NULL)
			return 0;
		lanai->conf2 &= ~CONFIG2_VCI0_NORMAL;
		conf2_write_if_powerup(lanai);
	}
	return 1;
}

static int lanai_normalize_ci(struct lanai_dev *lanai,
	const struct atm_vcc *atmvcc, short *vpip, vci_t *vcip)
{
	switch (*vpip) {
		case ATM_VPI_ANY:
			*vpip = 0;
			
		case 0:
			break;
		default:
			return -EADDRINUSE;
	}
	switch (*vcip) {
		case ATM_VCI_ANY:
			for (*vcip = ATM_NOT_RSV_VCI; *vcip < lanai->num_vci;
			    (*vcip)++)
				if (vci_is_ok(lanai, *vcip, atmvcc))
					return 0;
			return -EADDRINUSE;
		default:
			if (*vcip >= lanai->num_vci || *vcip < 0 ||
			    !vci_is_ok(lanai, *vcip, atmvcc))
				return -EADDRINUSE;
	}
	return 0;
}


#define CBRICG_FRAC_BITS	(4)
#define CBRICG_MAX		(2046 << CBRICG_FRAC_BITS)

static int pcr_to_cbricg(const struct atm_qos *qos)
{
	int rounddown = 0;	
	int x, icg, pcr = atm_pcr_goal(&qos->txtp);
	if (pcr == 0)		
		return 0;
	if (pcr < 0) {
		rounddown = 1;
		pcr = -pcr;
	}
	x = pcr * 27;
	icg = (3125 << (9 + CBRICG_FRAC_BITS)) - (x << CBRICG_FRAC_BITS);
	if (rounddown)
		icg += x - 1;
	icg /= x;
	if (icg > CBRICG_MAX)
		icg = CBRICG_MAX;
	DPRINTK("pcr_to_cbricg: pcr=%d rounddown=%c icg=%d\n",
	    pcr, rounddown ? 'Y' : 'N', icg);
	return icg;
}

static inline void lanai_cbr_setup(struct lanai_dev *lanai)
{
	reg_write(lanai, pcr_to_cbricg(&lanai->cbrvcc->qos), CBR_ICG_Reg);
	reg_write(lanai, lanai->cbrvcc->vci, CBR_PTR_Reg);
	lanai->conf2 |= CONFIG2_CBR_ENABLE;
	conf2_write(lanai);
}

static inline void lanai_cbr_shutdown(struct lanai_dev *lanai)
{
	lanai->conf2 &= ~CONFIG2_CBR_ENABLE;
	conf2_write(lanai);
}


static int __devinit lanai_dev_open(struct atm_dev *atmdev)
{
	struct lanai_dev *lanai = (struct lanai_dev *) atmdev->dev_data;
	unsigned long raw_base;
	int result;

	DPRINTK("In lanai_dev_open()\n");
	
	lanai->number = atmdev->number;
	lanai->num_vci = NUM_VCI;
	bitmap_zero(lanai->backlog_vccs, NUM_VCI);
	bitmap_zero(lanai->transmit_ready, NUM_VCI);
	lanai->naal0 = 0;
#ifdef USE_POWERDOWN
	lanai->nbound = 0;
#endif
	lanai->cbrvcc = NULL;
	memset(&lanai->stats, 0, sizeof lanai->stats);
	spin_lock_init(&lanai->endtxlock);
	spin_lock_init(&lanai->servicelock);
	atmdev->ci_range.vpi_bits = 0;
	atmdev->ci_range.vci_bits = 0;
	while (1 << atmdev->ci_range.vci_bits < lanai->num_vci)
		atmdev->ci_range.vci_bits++;
	atmdev->link_rate = ATM_25_PCR;

	
	if ((result = lanai_pci_start(lanai)) != 0)
		goto error;
	raw_base = lanai->pci->resource[0].start;
	lanai->base = (bus_addr_t) ioremap(raw_base, LANAI_MAPPING_SIZE);
	if (lanai->base == NULL) {
		printk(KERN_ERR DEV_LABEL ": couldn't remap I/O space\n");
		goto error_pci;
	}
	
	reset_board(lanai);
	lanai->conf1 = reg_read(lanai, Config1_Reg);
	lanai->conf1 &= ~(CONFIG1_GPOUT1 | CONFIG1_POWERDOWN |
	    CONFIG1_MASK_LEDMODE);
	lanai->conf1 |= CONFIG1_SET_LEDMODE(LEDMODE_NOT_SOOL);
	reg_write(lanai, lanai->conf1 | CONFIG1_GPOUT1, Config1_Reg);
	udelay(1000);
	conf1_write(lanai);

	result = check_board_id_and_rev("register",
	    reg_read(lanai, Reset_Reg), &lanai->board_rev);
	if (result != 0)
		goto error_unmap;

	
	if ((result = eeprom_read(lanai)) != 0)
		goto error_unmap;
	if ((result = eeprom_validate(lanai)) != 0)
		goto error_unmap;

	
	reg_write(lanai, lanai->conf1 | CONFIG1_GPOUT1, Config1_Reg);
	udelay(1000);
	conf1_write(lanai);
	
	lanai->conf1 |= (CONFIG1_GPOUT2 | CONFIG1_GPOUT3 | CONFIG1_DMA_ENABLE);
	conf1_write(lanai);

	
	if ((result = sram_test_and_clear(lanai)) != 0)
		goto error_unmap;

	
	lanai->conf1 |= CONFIG1_DMA_ENABLE;
	conf1_write(lanai);
	if ((result = service_buffer_allocate(lanai)) != 0)
		goto error_unmap;
	if ((result = vcc_table_allocate(lanai)) != 0)
		goto error_service;
	lanai->conf2 = (lanai->num_vci >= 512 ? CONFIG2_HOWMANY : 0) |
	    CONFIG2_HEC_DROP |	 CONFIG2_PTI7_MODE;
	conf2_write(lanai);
	reg_write(lanai, TX_FIFO_DEPTH, TxDepth_Reg);
	reg_write(lanai, 0, CBR_ICG_Reg);	
	if ((result = request_irq(lanai->pci->irq, lanai_int, IRQF_SHARED,
	    DEV_LABEL, lanai)) != 0) {
		printk(KERN_ERR DEV_LABEL ": can't allocate interrupt\n");
		goto error_vcctable;
	}
	mb();				
	intr_enable(lanai, INT_ALL & ~(INT_PING | INT_WAKE));
	
	lanai->conf1 = (lanai->conf1 & ~CONFIG1_MASK_LOOPMODE) |
	    CONFIG1_SET_LOOPMODE(LOOPMODE_NORMAL) |
	    CONFIG1_GPOUT2 | CONFIG1_GPOUT3;
	conf1_write(lanai);
	lanai->status = reg_read(lanai, Status_Reg);
	
#ifdef USE_POWERDOWN
	lanai->conf1 |= CONFIG1_POWERDOWN;
	conf1_write(lanai);
#endif
	memcpy(atmdev->esi, eeprom_mac(lanai), ESI_LEN);
	lanai_timed_poll_start(lanai);
	printk(KERN_NOTICE DEV_LABEL "(itf %d): rev.%d, base=0x%lx, irq=%u "
		"(%pMF)\n", lanai->number, (int) lanai->pci->revision,
		(unsigned long) lanai->base, lanai->pci->irq, atmdev->esi);
	printk(KERN_NOTICE DEV_LABEL "(itf %d): LANAI%s, serialno=%u(0x%X), "
	    "board_rev=%d\n", lanai->number,
	    lanai->type==lanai2 ? "2" : "HB", (unsigned int) lanai->serialno,
	    (unsigned int) lanai->serialno, lanai->board_rev);
	return 0;

    error_vcctable:
	vcc_table_deallocate(lanai);
    error_service:
	service_buffer_deallocate(lanai);
    error_unmap:
	reset_board(lanai);
#ifdef USE_POWERDOWN
	lanai->conf1 = reg_read(lanai, Config1_Reg) | CONFIG1_POWERDOWN;
	conf1_write(lanai);
#endif
	iounmap(lanai->base);
    error_pci:
	pci_disable_device(lanai->pci);
    error:
	return result;
}

static void lanai_dev_close(struct atm_dev *atmdev)
{
	struct lanai_dev *lanai = (struct lanai_dev *) atmdev->dev_data;
	printk(KERN_INFO DEV_LABEL "(itf %d): shutting down interface\n",
	    lanai->number);
	lanai_timed_poll_stop(lanai);
#ifdef USE_POWERDOWN
	lanai->conf1 = reg_read(lanai, Config1_Reg) & ~CONFIG1_POWERDOWN;
	conf1_write(lanai);
#endif
	intr_disable(lanai, INT_ALL);
	free_irq(lanai->pci->irq, lanai);
	reset_board(lanai);
#ifdef USE_POWERDOWN
	lanai->conf1 |= CONFIG1_POWERDOWN;
	conf1_write(lanai);
#endif
	pci_disable_device(lanai->pci);
	vcc_table_deallocate(lanai);
	service_buffer_deallocate(lanai);
	iounmap(lanai->base);
	kfree(lanai);
}

static void lanai_close(struct atm_vcc *atmvcc)
{
	struct lanai_vcc *lvcc = (struct lanai_vcc *) atmvcc->dev_data;
	struct lanai_dev *lanai = (struct lanai_dev *) atmvcc->dev->dev_data;
	if (lvcc == NULL)
		return;
	clear_bit(ATM_VF_READY, &atmvcc->flags);
	clear_bit(ATM_VF_PARTIAL, &atmvcc->flags);
	if (lvcc->rx.atmvcc == atmvcc) {
		lanai_shutdown_rx_vci(lvcc);
		if (atmvcc->qos.aal == ATM_AAL0) {
			if (--lanai->naal0 <= 0)
				aal0_buffer_free(lanai);
		} else
			lanai_buf_deallocate(&lvcc->rx.buf, lanai->pci);
		lvcc->rx.atmvcc = NULL;
	}
	if (lvcc->tx.atmvcc == atmvcc) {
		if (atmvcc == lanai->cbrvcc) {
			if (lvcc->vbase != NULL)
				lanai_cbr_shutdown(lanai);
			lanai->cbrvcc = NULL;
		}
		lanai_shutdown_tx_vci(lanai, lvcc);
		lanai_buf_deallocate(&lvcc->tx.buf, lanai->pci);
		lvcc->tx.atmvcc = NULL;
	}
	if (--lvcc->nref == 0) {
		host_vcc_unbind(lanai, lvcc);
		kfree(lvcc);
	}
	atmvcc->dev_data = NULL;
	clear_bit(ATM_VF_ADDR, &atmvcc->flags);
}

static int lanai_open(struct atm_vcc *atmvcc)
{
	struct lanai_dev *lanai;
	struct lanai_vcc *lvcc;
	int result = 0;
	int vci = atmvcc->vci;
	short vpi = atmvcc->vpi;
	
	if ((test_bit(ATM_VF_PARTIAL, &atmvcc->flags)) ||
	    (vpi == ATM_VPI_UNSPEC) || (vci == ATM_VCI_UNSPEC))
		return -EINVAL;
	lanai = (struct lanai_dev *) atmvcc->dev->dev_data;
	result = lanai_normalize_ci(lanai, atmvcc, &vpi, &vci);
	if (unlikely(result != 0))
		goto out;
	set_bit(ATM_VF_ADDR, &atmvcc->flags);
	if (atmvcc->qos.aal != ATM_AAL0 && atmvcc->qos.aal != ATM_AAL5)
		return -EINVAL;
	DPRINTK(DEV_LABEL "(itf %d): open %d.%d\n", lanai->number,
	    (int) vpi, vci);
	lvcc = lanai->vccs[vci];
	if (lvcc == NULL) {
		lvcc = new_lanai_vcc();
		if (unlikely(lvcc == NULL))
			return -ENOMEM;
		atmvcc->dev_data = lvcc;
	}
	lvcc->nref++;
	if (atmvcc->qos.rxtp.traffic_class != ATM_NONE) {
		APRINTK(lvcc->rx.atmvcc == NULL, "rx.atmvcc!=NULL, vci=%d\n",
		    vci);
		if (atmvcc->qos.aal == ATM_AAL0) {
			if (lanai->naal0 == 0)
				result = aal0_buffer_allocate(lanai);
		} else
			result = lanai_setup_rx_vci_aal5(
			    lanai, lvcc, &atmvcc->qos);
		if (unlikely(result != 0))
			goto out_free;
		lvcc->rx.atmvcc = atmvcc;
		lvcc->stats.rx_nomem = 0;
		lvcc->stats.x.aal5.rx_badlen = 0;
		lvcc->stats.x.aal5.service_trash = 0;
		lvcc->stats.x.aal5.service_stream = 0;
		lvcc->stats.x.aal5.service_rxcrc = 0;
		if (atmvcc->qos.aal == ATM_AAL0)
			lanai->naal0++;
	}
	if (atmvcc->qos.txtp.traffic_class != ATM_NONE) {
		APRINTK(lvcc->tx.atmvcc == NULL, "tx.atmvcc!=NULL, vci=%d\n",
		    vci);
		result = lanai_setup_tx_vci(lanai, lvcc, &atmvcc->qos);
		if (unlikely(result != 0))
			goto out_free;
		lvcc->tx.atmvcc = atmvcc;
		if (atmvcc->qos.txtp.traffic_class == ATM_CBR) {
			APRINTK(lanai->cbrvcc == NULL,
			    "cbrvcc!=NULL, vci=%d\n", vci);
			lanai->cbrvcc = atmvcc;
		}
	}
	host_vcc_bind(lanai, lvcc, vci);
	wmb();
	if (atmvcc == lvcc->rx.atmvcc)
		host_vcc_start_rx(lvcc);
	if (atmvcc == lvcc->tx.atmvcc) {
		host_vcc_start_tx(lvcc);
		if (lanai->cbrvcc == atmvcc)
			lanai_cbr_setup(lanai);
	}
	set_bit(ATM_VF_READY, &atmvcc->flags);
	return 0;
    out_free:
	lanai_close(atmvcc);
    out:
	return result;
}

static int lanai_send(struct atm_vcc *atmvcc, struct sk_buff *skb)
{
	struct lanai_vcc *lvcc = (struct lanai_vcc *) atmvcc->dev_data;
	struct lanai_dev *lanai = (struct lanai_dev *) atmvcc->dev->dev_data;
	unsigned long flags;
	if (unlikely(lvcc == NULL || lvcc->vbase == NULL ||
	      lvcc->tx.atmvcc != atmvcc))
		goto einval;
#ifdef DEBUG
	if (unlikely(skb == NULL)) {
		DPRINTK("lanai_send: skb==NULL for vci=%d\n", atmvcc->vci);
		goto einval;
	}
	if (unlikely(lanai == NULL)) {
		DPRINTK("lanai_send: lanai==NULL for vci=%d\n", atmvcc->vci);
		goto einval;
	}
#endif
	ATM_SKB(skb)->vcc = atmvcc;
	switch (atmvcc->qos.aal) {
		case ATM_AAL5:
			read_lock_irqsave(&vcc_sklist_lock, flags);
			vcc_tx_aal5(lanai, lvcc, skb);
			read_unlock_irqrestore(&vcc_sklist_lock, flags);
			return 0;
		case ATM_AAL0:
			if (unlikely(skb->len != ATM_CELL_SIZE-1))
				goto einval;
  
			cpu_to_be32s((u32 *) skb->data);
			read_lock_irqsave(&vcc_sklist_lock, flags);
			vcc_tx_aal0(lanai, lvcc, skb);
			read_unlock_irqrestore(&vcc_sklist_lock, flags);
			return 0;
	}
	DPRINTK("lanai_send: bad aal=%d on vci=%d\n", (int) atmvcc->qos.aal,
	    atmvcc->vci);
    einval:
	lanai_free_skb(atmvcc, skb);
	return -EINVAL;
}

static int lanai_change_qos(struct atm_vcc *atmvcc,
	 struct atm_qos *qos, int flags)
{
	return -EBUSY;		
}

#ifndef CONFIG_PROC_FS
#define lanai_proc_read NULL
#else
static int lanai_proc_read(struct atm_dev *atmdev, loff_t *pos, char *page)
{
	struct lanai_dev *lanai = (struct lanai_dev *) atmdev->dev_data;
	loff_t left = *pos;
	struct lanai_vcc *lvcc;
	if (left-- == 0)
		return sprintf(page, DEV_LABEL "(itf %d): chip=LANAI%s, "
		    "serial=%u, magic=0x%08X, num_vci=%d\n",
		    atmdev->number, lanai->type==lanai2 ? "2" : "HB",
		    (unsigned int) lanai->serialno,
		    (unsigned int) lanai->magicno, lanai->num_vci);
	if (left-- == 0)
		return sprintf(page, "revision: board=%d, pci_if=%d\n",
		    lanai->board_rev, (int) lanai->pci->revision);
	if (left-- == 0)
		return sprintf(page, "EEPROM ESI: %pM\n",
		    &lanai->eeprom[EEPROM_MAC]);
	if (left-- == 0)
		return sprintf(page, "status: SOOL=%d, LOCD=%d, LED=%d, "
		    "GPIN=%d\n", (lanai->status & STATUS_SOOL) ? 1 : 0,
		    (lanai->status & STATUS_LOCD) ? 1 : 0,
		    (lanai->status & STATUS_LED) ? 1 : 0,
		    (lanai->status & STATUS_GPIN) ? 1 : 0);
	if (left-- == 0)
		return sprintf(page, "global buffer sizes: service=%Zu, "
		    "aal0_rx=%Zu\n", lanai_buf_size(&lanai->service),
		    lanai->naal0 ? lanai_buf_size(&lanai->aal0buf) : 0);
	if (left-- == 0) {
		get_statistics(lanai);
		return sprintf(page, "cells in error: overflow=%u, "
		    "closed_vci=%u, bad_HEC=%u, rx_fifo=%u\n",
		    lanai->stats.ovfl_trash, lanai->stats.vci_trash,
		    lanai->stats.hec_err, lanai->stats.atm_ovfl);
	}
	if (left-- == 0)
		return sprintf(page, "PCI errors: parity_detect=%u, "
		    "master_abort=%u, master_target_abort=%u,\n",
		    lanai->stats.pcierr_parity_detect,
		    lanai->stats.pcierr_serr_set,
		    lanai->stats.pcierr_m_target_abort);
	if (left-- == 0)
		return sprintf(page, "            slave_target_abort=%u, "
		    "master_parity=%u\n", lanai->stats.pcierr_s_target_abort,
		    lanai->stats.pcierr_master_parity);
	if (left-- == 0)
		return sprintf(page, "                     no_tx=%u, "
		    "no_rx=%u, bad_rx_aal=%u\n", lanai->stats.service_norx,
		    lanai->stats.service_notx,
		    lanai->stats.service_rxnotaal5);
	if (left-- == 0)
		return sprintf(page, "resets: dma=%u, card=%u\n",
		    lanai->stats.dma_reenable, lanai->stats.card_reset);
	
	read_lock(&vcc_sklist_lock);
	for (; ; left++) {
		if (left >= NUM_VCI) {
			left = 0;
			goto out;
		}
		if ((lvcc = lanai->vccs[left]) != NULL)
			break;
		(*pos)++;
	}
	
	left = sprintf(page, "VCI %4d: nref=%d, rx_nomem=%u",  (vci_t) left,
	    lvcc->nref, lvcc->stats.rx_nomem);
	if (lvcc->rx.atmvcc != NULL) {
		left += sprintf(&page[left], ",\n          rx_AAL=%d",
		    lvcc->rx.atmvcc->qos.aal == ATM_AAL5 ? 5 : 0);
		if (lvcc->rx.atmvcc->qos.aal == ATM_AAL5)
			left += sprintf(&page[left], ", rx_buf_size=%Zu, "
			    "rx_bad_len=%u,\n          rx_service_trash=%u, "
			    "rx_service_stream=%u, rx_bad_crc=%u",
			    lanai_buf_size(&lvcc->rx.buf),
			    lvcc->stats.x.aal5.rx_badlen,
			    lvcc->stats.x.aal5.service_trash,
			    lvcc->stats.x.aal5.service_stream,
			    lvcc->stats.x.aal5.service_rxcrc);
	}
	if (lvcc->tx.atmvcc != NULL)
		left += sprintf(&page[left], ",\n          tx_AAL=%d, "
		    "tx_buf_size=%Zu, tx_qos=%cBR, tx_backlogged=%c",
		    lvcc->tx.atmvcc->qos.aal == ATM_AAL5 ? 5 : 0,
		    lanai_buf_size(&lvcc->tx.buf),
		    lvcc->tx.atmvcc == lanai->cbrvcc ? 'C' : 'U',
		    vcc_is_backlogged(lvcc) ? 'Y' : 'N');
	page[left++] = '\n';
	page[left] = '\0';
    out:
	read_unlock(&vcc_sklist_lock);
	return left;
}
#endif 


static const struct atmdev_ops ops = {
	.dev_close	= lanai_dev_close,
	.open		= lanai_open,
	.close		= lanai_close,
	.getsockopt	= NULL,
	.setsockopt	= NULL,
	.send		= lanai_send,
	.phy_put	= NULL,
	.phy_get	= NULL,
	.change_qos	= lanai_change_qos,
	.proc_read	= lanai_proc_read,
	.owner		= THIS_MODULE
};

static int __devinit lanai_init_one(struct pci_dev *pci,
				    const struct pci_device_id *ident)
{
	struct lanai_dev *lanai;
	struct atm_dev *atmdev;
	int result;

	lanai = kmalloc(sizeof(*lanai), GFP_KERNEL);
	if (lanai == NULL) {
		printk(KERN_ERR DEV_LABEL
		       ": couldn't allocate dev_data structure!\n");
		return -ENOMEM;
	}

	atmdev = atm_dev_register(DEV_LABEL, &pci->dev, &ops, -1, NULL);
	if (atmdev == NULL) {
		printk(KERN_ERR DEV_LABEL
		    ": couldn't register atm device!\n");
		kfree(lanai);
		return -EBUSY;
	}

	atmdev->dev_data = lanai;
	lanai->pci = pci;
	lanai->type = (enum lanai_type) ident->device;

	result = lanai_dev_open(atmdev);
	if (result != 0) {
		DPRINTK("lanai_start() failed, err=%d\n", -result);
		atm_dev_deregister(atmdev);
		kfree(lanai);
	}
	return result;
}

static struct pci_device_id lanai_pci_tbl[] = {
	{ PCI_VDEVICE(EF, PCI_DEVICE_ID_EF_ATM_LANAI2) },
	{ PCI_VDEVICE(EF, PCI_DEVICE_ID_EF_ATM_LANAIHB) },
	{ 0, }	
};
MODULE_DEVICE_TABLE(pci, lanai_pci_tbl);

static struct pci_driver lanai_driver = {
	.name     = DEV_LABEL,
	.id_table = lanai_pci_tbl,
	.probe    = lanai_init_one,
};

static int __init lanai_module_init(void)
{
	int x;

	x = pci_register_driver(&lanai_driver);
	if (x != 0)
		printk(KERN_ERR DEV_LABEL ": no adapter found\n");
	return x;
}

static void __exit lanai_module_exit(void)
{
	DPRINTK("cleanup_module()\n");
	pci_unregister_driver(&lanai_driver);
}

module_init(lanai_module_init);
module_exit(lanai_module_exit);

MODULE_AUTHOR("Mitchell Blank Jr <mitch@sfgoth.com>");
MODULE_DESCRIPTION("Efficient Networks Speedstream 3010 driver");
MODULE_LICENSE("GPL");
