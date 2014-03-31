/*
 *      FarSync WAN driver for Linux (2.6.x kernel version)
 *
 *      Actually sync driver for X.21, V.35 and V.24 on FarSync T-series cards
 *
 *      Copyright (C) 2001-2004 FarSite Communications Ltd.
 *      www.farsite.co.uk
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *      Author:      R.J.Dunlop    <bob.dunlop@farsite.co.uk>
 *      Maintainer:  Kevin Curtis  <kevin.curtis@farsite.co.uk>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/if.h>
#include <linux/hdlc.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include "farsync.h"

MODULE_AUTHOR("R.J.Dunlop <bob.dunlop@farsite.co.uk>");
MODULE_DESCRIPTION("FarSync T-Series WAN driver. FarSite Communications Ltd.");
MODULE_LICENSE("GPL");


#define FST_MAX_PORTS           4
#define FST_MAX_CARDS           32

#define FST_TX_QUEUE_LEN        100	
#define FST_TXQ_DEPTH           16	
#define FST_HIGH_WATER_MARK     12	
#define FST_LOW_WATER_MARK      8	
#define FST_MAX_MTU             8000	
#define FST_DEF_MTU             1500	

#define FST_TX_TIMEOUT          (2*HZ)

#ifdef ARPHRD_RAWHDLC
#define ARPHRD_MYTYPE   ARPHRD_RAWHDLC	
#else
#define ARPHRD_MYTYPE   ARPHRD_HDLC	
#endif

static int fst_txq_low = FST_LOW_WATER_MARK;
static int fst_txq_high = FST_HIGH_WATER_MARK;
static int fst_max_reads = 7;
static int fst_excluded_cards = 0;
static int fst_excluded_list[FST_MAX_CARDS];

module_param(fst_txq_low, int, 0);
module_param(fst_txq_high, int, 0);
module_param(fst_max_reads, int, 0);
module_param(fst_excluded_cards, int, 0);
module_param_array(fst_excluded_list, int, NULL, 0);

#pragma pack(1)

#define SMC_VERSION 24

#define FST_MEMSIZE 0x100000	

#define SMC_BASE 0x00002000L	
#define BFM_BASE 0x00010000L	

#define LEN_TX_BUFFER 8192	
#define LEN_RX_BUFFER 8192

#define LEN_SMALL_TX_BUFFER 256	
#define LEN_SMALL_RX_BUFFER 256

#define NUM_TX_BUFFER 2		
#define NUM_RX_BUFFER 8

#define INT_RETRY_TIME 2

struct txdesc {			
	volatile u16 ladr;	
	volatile u8 hadr;	
	volatile u8 bits;	
	volatile u16 bcnt;	
	u16 unused;		
};

struct rxdesc {			
	volatile u16 ladr;	
	volatile u8 hadr;	
	volatile u8 bits;	
	volatile u16 bcnt;	
	volatile u16 mcnt;	
};

#define cnv_bcnt(len)   (-(len))

#define DMA_OWN         0x80	
#define TX_STP          0x02	
#define TX_ENP          0x01	
#define RX_ERR          0x40	
#define RX_FRAM         0x20	
#define RX_OFLO         0x10	
#define RX_CRC          0x08	
#define RX_HBUF         0x04	
#define RX_STP          0x02	
#define RX_ENP          0x01	

#define MAX_CIRBUFF     32

struct cirbuff {
	u8 rdindex;		
	u8 wrindex;		
	u8 evntbuff[MAX_CIRBUFF];
};

#define CTLA_CHG        0x18	
#define CTLB_CHG        0x19
#define CTLC_CHG        0x1A
#define CTLD_CHG        0x1B

#define INIT_CPLT       0x20	
#define INIT_FAIL       0x21	

#define ABTA_SENT       0x24	
#define ABTB_SENT       0x25
#define ABTC_SENT       0x26
#define ABTD_SENT       0x27

#define TXA_UNDF        0x28	
#define TXB_UNDF        0x29
#define TXC_UNDF        0x2A
#define TXD_UNDF        0x2B

#define F56_INT         0x2C
#define M32_INT         0x2D

#define TE1_ALMA        0x30

struct port_cfg {
	u16 lineInterface;	
	u8 x25op;		
	u8 internalClock;	
	u8 transparentMode;	
	u8 invertClock;		
	u8 padBytes[6];		
	u32 lineSpeed;		
};

struct su_config {
	u32 dataRate;
	u8 clocking;
	u8 framing;
	u8 structure;
	u8 interface;
	u8 coding;
	u8 lineBuildOut;
	u8 equalizer;
	u8 transparentMode;
	u8 loopMode;
	u8 range;
	u8 txBufferMode;
	u8 rxBufferMode;
	u8 startingSlot;
	u8 losThreshold;
	u8 enableIdleCode;
	u8 idleCode;
	u8 spare[44];
};

struct su_status {
	u32 receiveBufferDelay;
	u32 framingErrorCount;
	u32 codeViolationCount;
	u32 crcErrorCount;
	u32 lineAttenuation;
	u8 portStarted;
	u8 lossOfSignal;
	u8 receiveRemoteAlarm;
	u8 alarmIndicationSignal;
	u8 spare[40];
};

struct fst_shared {
	
	struct rxdesc rxDescrRing[FST_MAX_PORTS][NUM_RX_BUFFER];
	struct txdesc txDescrRing[FST_MAX_PORTS][NUM_TX_BUFFER];

	
	u8 smallRxBuffer[FST_MAX_PORTS][NUM_RX_BUFFER][LEN_SMALL_RX_BUFFER];
	u8 smallTxBuffer[FST_MAX_PORTS][NUM_TX_BUFFER][LEN_SMALL_TX_BUFFER];

	u8 taskStatus;		

	u8 interruptHandshake;	

	u16 smcVersion;		

	u32 smcFirmwareVersion;	

	u16 txa_done;		
	u16 rxa_done;
	u16 txb_done;
	u16 rxb_done;
	u16 txc_done;
	u16 rxc_done;
	u16 txd_done;
	u16 rxd_done;

	u16 mailbox[4];		

	struct cirbuff interruptEvent;	

	u32 v24IpSts[FST_MAX_PORTS];	
	u32 v24OpSts[FST_MAX_PORTS];	

	struct port_cfg portConfig[FST_MAX_PORTS];

	u16 clockStatus[FST_MAX_PORTS];	

	u16 cableStatus;	

	u16 txDescrIndex[FST_MAX_PORTS];	
	u16 rxDescrIndex[FST_MAX_PORTS];	

	u16 portMailbox[FST_MAX_PORTS][2];	
	u16 cardMailbox[4];	

	u32 interruptRetryCount;

	u32 portHandle[FST_MAX_PORTS];

	
	u32 transmitBufferUnderflow[FST_MAX_PORTS];

	
	u32 v24DebouncedSts[FST_MAX_PORTS];

	
	u32 ctsTimer[FST_MAX_PORTS];
	u32 ctsTimerRun[FST_MAX_PORTS];
	u32 dcdTimer[FST_MAX_PORTS];
	u32 dcdTimerRun[FST_MAX_PORTS];

	u32 numberOfPorts;	

	u16 _reserved[64];

	u16 cardMode;		

	u16 portScheduleOffset;

	struct su_config suConfig;	
	struct su_status suStatus;

	u32 endOfSmcSignature;	
};

#define END_SIG                 0x12345678

#define NOP             0	
#define ACK             1	
#define NAK             2	
#define STARTPORT       3	
#define STOPPORT        4	
#define ABORTTX         5	
#define SETV24O         6	

#define CNTRL_9052      0x50	
#define CNTRL_9054      0x6c	

#define INTCSR_9052     0x4c	
#define INTCSR_9054     0x68	

#define DMAMODE0        0x80
#define DMAPADR0        0x84
#define DMALADR0        0x88
#define DMASIZ0         0x8c
#define DMADPR0         0x90
#define DMAMODE1        0x94
#define DMAPADR1        0x98
#define DMALADR1        0x9c
#define DMASIZ1         0xa0
#define DMADPR1         0xa4
#define DMACSR0         0xa8
#define DMACSR1         0xa9
#define DMAARB          0xac
#define DMATHR          0xb0
#define DMADAC0         0xb4
#define DMADAC1         0xb8
#define DMAMARBR        0xac

#define FST_MIN_DMA_LEN 64
#define FST_RX_DMA_INT  0x01
#define FST_TX_DMA_INT  0x02
#define FST_CARD_INT    0x04

struct buf_window {
	u8 txBuffer[FST_MAX_PORTS][NUM_TX_BUFFER][LEN_TX_BUFFER];
	u8 rxBuffer[FST_MAX_PORTS][NUM_RX_BUFFER][LEN_RX_BUFFER];
};

#define BUF_OFFSET(X)   (BFM_BASE + offsetof(struct buf_window, X))

#pragma pack()

struct fst_port_info {
        struct net_device *dev; 
	struct fst_card_info *card;	
	int index;		
	int hwif;		
	int run;		
	int mode;		
	int rxpos;		
	int txpos;		
	int txipos;		
	int start;		
	int txqs;		
	int txqe;		
	struct sk_buff *txq[FST_TXQ_DEPTH];	
	int rxqdepth;
};

struct fst_card_info {
	char __iomem *mem;	
	char __iomem *ctlmem;	
	unsigned int phys_mem;	
	unsigned int phys_ctlmem;	
	unsigned int irq;	
	unsigned int nports;	
	unsigned int type;	
	unsigned int state;	
	spinlock_t card_lock;	
	unsigned short pci_conf;	
	
	struct fst_port_info ports[FST_MAX_PORTS];
	struct pci_dev *device;	
	int card_no;		
	int family;		
	int dmarx_in_progress;
	int dmatx_in_progress;
	unsigned long int_count;
	unsigned long int_time_ave;
	void *rx_dma_handle_host;
	dma_addr_t rx_dma_handle_card;
	void *tx_dma_handle_host;
	dma_addr_t tx_dma_handle_card;
	struct sk_buff *dma_skb_rx;
	struct fst_port_info *dma_port_rx;
	struct fst_port_info *dma_port_tx;
	int dma_len_rx;
	int dma_len_tx;
	int dma_txpos;
	int dma_rxpos;
};

#define dev_to_port(D)  (dev_to_hdlc(D)->priv)
#define port_to_dev(P)  ((P)->dev)


#define WIN_OFFSET(X)   ((long)&(((struct fst_shared *)SMC_BASE)->X))

#define FST_RDB(C,E)    readb ((C)->mem + WIN_OFFSET(E))
#define FST_RDW(C,E)    readw ((C)->mem + WIN_OFFSET(E))
#define FST_RDL(C,E)    readl ((C)->mem + WIN_OFFSET(E))

#define FST_WRB(C,E,B)  writeb ((B), (C)->mem + WIN_OFFSET(E))
#define FST_WRW(C,E,W)  writew ((W), (C)->mem + WIN_OFFSET(E))
#define FST_WRL(C,E,L)  writel ((L), (C)->mem + WIN_OFFSET(E))

#if FST_DEBUG

static int fst_debug_mask = { FST_DEBUG };

#define dbg(F, fmt, args...)					\
do {								\
	if (fst_debug_mask & (F))				\
		printk(KERN_DEBUG pr_fmt(fmt), ##args);		\
} while (0)
#else
#define dbg(F, fmt, args...)					\
do {								\
	if (0)							\
		printk(KERN_DEBUG pr_fmt(fmt), ##args);		\
} while (0)
#endif

static DEFINE_PCI_DEVICE_TABLE(fst_pci_dev_id) = {
	{PCI_VENDOR_ID_FARSITE, PCI_DEVICE_ID_FARSITE_T2P, PCI_ANY_ID, 
	 PCI_ANY_ID, 0, 0, FST_TYPE_T2P},

	{PCI_VENDOR_ID_FARSITE, PCI_DEVICE_ID_FARSITE_T4P, PCI_ANY_ID, 
	 PCI_ANY_ID, 0, 0, FST_TYPE_T4P},

	{PCI_VENDOR_ID_FARSITE, PCI_DEVICE_ID_FARSITE_T1U, PCI_ANY_ID, 
	 PCI_ANY_ID, 0, 0, FST_TYPE_T1U},

	{PCI_VENDOR_ID_FARSITE, PCI_DEVICE_ID_FARSITE_T2U, PCI_ANY_ID, 
	 PCI_ANY_ID, 0, 0, FST_TYPE_T2U},

	{PCI_VENDOR_ID_FARSITE, PCI_DEVICE_ID_FARSITE_T4U, PCI_ANY_ID, 
	 PCI_ANY_ID, 0, 0, FST_TYPE_T4U},

	{PCI_VENDOR_ID_FARSITE, PCI_DEVICE_ID_FARSITE_TE1, PCI_ANY_ID, 
	 PCI_ANY_ID, 0, 0, FST_TYPE_TE1},

	{PCI_VENDOR_ID_FARSITE, PCI_DEVICE_ID_FARSITE_TE1C, PCI_ANY_ID, 
	 PCI_ANY_ID, 0, 0, FST_TYPE_TE1},
	{0,}			
};

MODULE_DEVICE_TABLE(pci, fst_pci_dev_id);


static void do_bottom_half_tx(struct fst_card_info *card);
static void do_bottom_half_rx(struct fst_card_info *card);
static void fst_process_tx_work_q(unsigned long work_q);
static void fst_process_int_work_q(unsigned long work_q);

static DECLARE_TASKLET(fst_tx_task, fst_process_tx_work_q, 0);
static DECLARE_TASKLET(fst_int_task, fst_process_int_work_q, 0);

static struct fst_card_info *fst_card_array[FST_MAX_CARDS];
static spinlock_t fst_work_q_lock;
static u64 fst_work_txq;
static u64 fst_work_intq;

static void
fst_q_work_item(u64 * queue, int card_index)
{
	unsigned long flags;
	u64 mask;

	spin_lock_irqsave(&fst_work_q_lock, flags);

	mask = 1 << card_index;
	*queue |= mask;
	spin_unlock_irqrestore(&fst_work_q_lock, flags);
}

static void
fst_process_tx_work_q(unsigned long work_q)
{
	unsigned long flags;
	u64 work_txq;
	int i;

	dbg(DBG_TX, "fst_process_tx_work_q\n");
	spin_lock_irqsave(&fst_work_q_lock, flags);
	work_txq = fst_work_txq;
	fst_work_txq = 0;
	spin_unlock_irqrestore(&fst_work_q_lock, flags);

	for (i = 0; i < FST_MAX_CARDS; i++) {
		if (work_txq & 0x01) {
			if (fst_card_array[i] != NULL) {
				dbg(DBG_TX, "Calling tx bh for card %d\n", i);
				do_bottom_half_tx(fst_card_array[i]);
			}
		}
		work_txq = work_txq >> 1;
	}
}

static void
fst_process_int_work_q(unsigned long work_q)
{
	unsigned long flags;
	u64 work_intq;
	int i;

	dbg(DBG_INTR, "fst_process_int_work_q\n");
	spin_lock_irqsave(&fst_work_q_lock, flags);
	work_intq = fst_work_intq;
	fst_work_intq = 0;
	spin_unlock_irqrestore(&fst_work_q_lock, flags);

	for (i = 0; i < FST_MAX_CARDS; i++) {
		if (work_intq & 0x01) {
			if (fst_card_array[i] != NULL) {
				dbg(DBG_INTR,
				    "Calling rx & tx bh for card %d\n", i);
				do_bottom_half_rx(fst_card_array[i]);
				do_bottom_half_tx(fst_card_array[i]);
			}
		}
		work_intq = work_intq >> 1;
	}
}

static inline void
fst_cpureset(struct fst_card_info *card)
{
	unsigned char interrupt_line_register;
	unsigned long j = jiffies + 1;
	unsigned int regval;

	if (card->family == FST_FAMILY_TXU) {
		if (pci_read_config_byte
		    (card->device, PCI_INTERRUPT_LINE, &interrupt_line_register)) {
			dbg(DBG_ASS,
			    "Error in reading interrupt line register\n");
		}
		outw(0x440f, card->pci_conf + CNTRL_9054 + 2);
		outw(0x040f, card->pci_conf + CNTRL_9054 + 2);
		j = jiffies + 1;
		while (jiffies < j)
			 ;
		outw(0x240f, card->pci_conf + CNTRL_9054 + 2);
		j = jiffies + 1;
		while (jiffies < j)
			 ;
		outw(0x040f, card->pci_conf + CNTRL_9054 + 2);

		if (pci_write_config_byte
		    (card->device, PCI_INTERRUPT_LINE, interrupt_line_register)) {
			dbg(DBG_ASS,
			    "Error in writing interrupt line register\n");
		}

	} else {
		regval = inl(card->pci_conf + CNTRL_9052);

		outl(regval | 0x40000000, card->pci_conf + CNTRL_9052);
		outl(regval & ~0x40000000, card->pci_conf + CNTRL_9052);
	}
}

static inline void
fst_cpurelease(struct fst_card_info *card)
{
	if (card->family == FST_FAMILY_TXU) {
		(void) readb(card->mem);

		outw(0x040e, card->pci_conf + CNTRL_9054 + 2);
		outw(0x040f, card->pci_conf + CNTRL_9054 + 2);
	} else {
		(void) readb(card->ctlmem);
	}
}

static inline void
fst_clear_intr(struct fst_card_info *card)
{
	if (card->family == FST_FAMILY_TXU) {
		(void) readb(card->ctlmem);
	} else {
		outw(0x0543, card->pci_conf + INTCSR_9052);
	}
}

static inline void
fst_enable_intr(struct fst_card_info *card)
{
	if (card->family == FST_FAMILY_TXU) {
		outl(0x0f0c0900, card->pci_conf + INTCSR_9054);
	} else {
		outw(0x0543, card->pci_conf + INTCSR_9052);
	}
}

static inline void
fst_disable_intr(struct fst_card_info *card)
{
	if (card->family == FST_FAMILY_TXU) {
		outl(0x00000000, card->pci_conf + INTCSR_9054);
	} else {
		outw(0x0000, card->pci_conf + INTCSR_9052);
	}
}

static void
fst_process_rx_status(int rx_status, char *name)
{
	switch (rx_status) {
	case NET_RX_SUCCESS:
		{
			break;
		}
	case NET_RX_DROP:
		{
			dbg(DBG_ASS, "%s: Received packet dropped\n", name);
			break;
		}
	}
}

static inline void
fst_init_dma(struct fst_card_info *card)
{
	if (card->family == FST_FAMILY_TXU) {
	        pci_set_master(card->device);
		outl(0x00020441, card->pci_conf + DMAMODE0);
		outl(0x00020441, card->pci_conf + DMAMODE1);
		outl(0x0, card->pci_conf + DMATHR);
	}
}

static void
fst_tx_dma_complete(struct fst_card_info *card, struct fst_port_info *port,
		    int len, int txpos)
{
	struct net_device *dev = port_to_dev(port);

	dbg(DBG_TX, "fst_tx_dma_complete\n");
	FST_WRB(card, txDescrRing[port->index][txpos].bits,
		DMA_OWN | TX_STP | TX_ENP);
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += len;
	dev->trans_start = jiffies;
}

static __be16 farsync_type_trans(struct sk_buff *skb, struct net_device *dev)
{
	skb->dev = dev;
	skb_reset_mac_header(skb);
	skb->pkt_type = PACKET_HOST;
	return htons(ETH_P_CUST);
}

static void
fst_rx_dma_complete(struct fst_card_info *card, struct fst_port_info *port,
		    int len, struct sk_buff *skb, int rxp)
{
	struct net_device *dev = port_to_dev(port);
	int pi;
	int rx_status;

	dbg(DBG_TX, "fst_rx_dma_complete\n");
	pi = port->index;
	memcpy(skb_put(skb, len), card->rx_dma_handle_host, len);

	
	FST_WRB(card, rxDescrRing[pi][rxp].bits, DMA_OWN);

	
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += len;

	
	dbg(DBG_RX, "Pushing the frame up the stack\n");
	if (port->mode == FST_RAW)
		skb->protocol = farsync_type_trans(skb, dev);
	else
		skb->protocol = hdlc_type_trans(skb, dev);
	rx_status = netif_rx(skb);
	fst_process_rx_status(rx_status, port_to_dev(port)->name);
	if (rx_status == NET_RX_DROP)
		dev->stats.rx_dropped++;
}

static inline void
fst_rx_dma(struct fst_card_info *card, dma_addr_t skb,
	   dma_addr_t mem, int len)
{

	dbg(DBG_RX, "In fst_rx_dma %lx %lx %d\n",
	    (unsigned long) skb, (unsigned long) mem, len);
	if (card->dmarx_in_progress) {
		dbg(DBG_ASS, "In fst_rx_dma while dma in progress\n");
	}

	outl(skb, card->pci_conf + DMAPADR0);	
	outl(mem, card->pci_conf + DMALADR0);	
	outl(len, card->pci_conf + DMASIZ0);	
	outl(0x00000000c, card->pci_conf + DMADPR0);	

	card->dmarx_in_progress = 1;
	outb(0x03, card->pci_conf + DMACSR0);	
}

static inline void
fst_tx_dma(struct fst_card_info *card, unsigned char *skb,
	   unsigned char *mem, int len)
{

	dbg(DBG_TX, "In fst_tx_dma %p %p %d\n", skb, mem, len);
	if (card->dmatx_in_progress) {
		dbg(DBG_ASS, "In fst_tx_dma while dma in progress\n");
	}

	outl((unsigned long) skb, card->pci_conf + DMAPADR1);	
	outl((unsigned long) mem, card->pci_conf + DMALADR1);	
	outl(len, card->pci_conf + DMASIZ1);	
	outl(0x000000004, card->pci_conf + DMADPR1);	

	card->dmatx_in_progress = 1;
	outb(0x03, card->pci_conf + DMACSR1);	
}

static void
fst_issue_cmd(struct fst_port_info *port, unsigned short cmd)
{
	struct fst_card_info *card;
	unsigned short mbval;
	unsigned long flags;
	int safety;

	card = port->card;
	spin_lock_irqsave(&card->card_lock, flags);
	mbval = FST_RDW(card, portMailbox[port->index][0]);

	safety = 0;
	
	while (mbval > NAK) {
		spin_unlock_irqrestore(&card->card_lock, flags);
		schedule_timeout_uninterruptible(1);
		spin_lock_irqsave(&card->card_lock, flags);

		if (++safety > 2000) {
			pr_err("Mailbox safety timeout\n");
			break;
		}

		mbval = FST_RDW(card, portMailbox[port->index][0]);
	}
	if (safety > 0) {
		dbg(DBG_CMD, "Mailbox clear after %d jiffies\n", safety);
	}
	if (mbval == NAK) {
		dbg(DBG_CMD, "issue_cmd: previous command was NAK'd\n");
	}

	FST_WRW(card, portMailbox[port->index][0], cmd);

	if (cmd == ABORTTX || cmd == STARTPORT) {
		port->txpos = 0;
		port->txipos = 0;
		port->start = 0;
	}

	spin_unlock_irqrestore(&card->card_lock, flags);
}

static inline void
fst_op_raise(struct fst_port_info *port, unsigned int outputs)
{
	outputs |= FST_RDL(port->card, v24OpSts[port->index]);
	FST_WRL(port->card, v24OpSts[port->index], outputs);

	if (port->run)
		fst_issue_cmd(port, SETV24O);
}

static inline void
fst_op_lower(struct fst_port_info *port, unsigned int outputs)
{
	outputs = ~outputs & FST_RDL(port->card, v24OpSts[port->index]);
	FST_WRL(port->card, v24OpSts[port->index], outputs);

	if (port->run)
		fst_issue_cmd(port, SETV24O);
}

static void
fst_rx_config(struct fst_port_info *port)
{
	int i;
	int pi;
	unsigned int offset;
	unsigned long flags;
	struct fst_card_info *card;

	pi = port->index;
	card = port->card;
	spin_lock_irqsave(&card->card_lock, flags);
	for (i = 0; i < NUM_RX_BUFFER; i++) {
		offset = BUF_OFFSET(rxBuffer[pi][i][0]);

		FST_WRW(card, rxDescrRing[pi][i].ladr, (u16) offset);
		FST_WRB(card, rxDescrRing[pi][i].hadr, (u8) (offset >> 16));
		FST_WRW(card, rxDescrRing[pi][i].bcnt, cnv_bcnt(LEN_RX_BUFFER));
		FST_WRW(card, rxDescrRing[pi][i].mcnt, LEN_RX_BUFFER);
		FST_WRB(card, rxDescrRing[pi][i].bits, DMA_OWN);
	}
	port->rxpos = 0;
	spin_unlock_irqrestore(&card->card_lock, flags);
}

static void
fst_tx_config(struct fst_port_info *port)
{
	int i;
	int pi;
	unsigned int offset;
	unsigned long flags;
	struct fst_card_info *card;

	pi = port->index;
	card = port->card;
	spin_lock_irqsave(&card->card_lock, flags);
	for (i = 0; i < NUM_TX_BUFFER; i++) {
		offset = BUF_OFFSET(txBuffer[pi][i][0]);

		FST_WRW(card, txDescrRing[pi][i].ladr, (u16) offset);
		FST_WRB(card, txDescrRing[pi][i].hadr, (u8) (offset >> 16));
		FST_WRW(card, txDescrRing[pi][i].bcnt, 0);
		FST_WRB(card, txDescrRing[pi][i].bits, 0);
	}
	port->txpos = 0;
	port->txipos = 0;
	port->start = 0;
	spin_unlock_irqrestore(&card->card_lock, flags);
}

static void
fst_intr_te1_alarm(struct fst_card_info *card, struct fst_port_info *port)
{
	u8 los;
	u8 rra;
	u8 ais;

	los = FST_RDB(card, suStatus.lossOfSignal);
	rra = FST_RDB(card, suStatus.receiveRemoteAlarm);
	ais = FST_RDB(card, suStatus.alarmIndicationSignal);

	if (los) {
		if (netif_carrier_ok(port_to_dev(port))) {
			dbg(DBG_INTR, "Net carrier off\n");
			netif_carrier_off(port_to_dev(port));
		}
	} else {
		if (!netif_carrier_ok(port_to_dev(port))) {
			dbg(DBG_INTR, "Net carrier on\n");
			netif_carrier_on(port_to_dev(port));
		}
	}

	if (los)
		dbg(DBG_INTR, "Assert LOS Alarm\n");
	else
		dbg(DBG_INTR, "De-assert LOS Alarm\n");
	if (rra)
		dbg(DBG_INTR, "Assert RRA Alarm\n");
	else
		dbg(DBG_INTR, "De-assert RRA Alarm\n");

	if (ais)
		dbg(DBG_INTR, "Assert AIS Alarm\n");
	else
		dbg(DBG_INTR, "De-assert AIS Alarm\n");
}

static void
fst_intr_ctlchg(struct fst_card_info *card, struct fst_port_info *port)
{
	int signals;

	signals = FST_RDL(card, v24DebouncedSts[port->index]);

	if (signals & (((port->hwif == X21) || (port->hwif == X21D))
		       ? IPSTS_INDICATE : IPSTS_DCD)) {
		if (!netif_carrier_ok(port_to_dev(port))) {
			dbg(DBG_INTR, "DCD active\n");
			netif_carrier_on(port_to_dev(port));
		}
	} else {
		if (netif_carrier_ok(port_to_dev(port))) {
			dbg(DBG_INTR, "DCD lost\n");
			netif_carrier_off(port_to_dev(port));
		}
	}
}

static void
fst_log_rx_error(struct fst_card_info *card, struct fst_port_info *port,
		 unsigned char dmabits, int rxp, unsigned short len)
{
	struct net_device *dev = port_to_dev(port);

	dev->stats.rx_errors++;
	if (dmabits & RX_OFLO) {
		dev->stats.rx_fifo_errors++;
		dbg(DBG_ASS, "Rx fifo error on card %d port %d buffer %d\n",
		    card->card_no, port->index, rxp);
	}
	if (dmabits & RX_CRC) {
		dev->stats.rx_crc_errors++;
		dbg(DBG_ASS, "Rx crc error on card %d port %d\n",
		    card->card_no, port->index);
	}
	if (dmabits & RX_FRAM) {
		dev->stats.rx_frame_errors++;
		dbg(DBG_ASS, "Rx frame error on card %d port %d\n",
		    card->card_no, port->index);
	}
	if (dmabits == (RX_STP | RX_ENP)) {
		dev->stats.rx_length_errors++;
		dbg(DBG_ASS, "Rx length error (%d) on card %d port %d\n",
		    len, card->card_no, port->index);
	}
}

static void
fst_recover_rx_error(struct fst_card_info *card, struct fst_port_info *port,
		     unsigned char dmabits, int rxp, unsigned short len)
{
	int i;
	int pi;

	pi = port->index;
	i = 0;
	while ((dmabits & (DMA_OWN | RX_STP)) == 0) {
		FST_WRB(card, rxDescrRing[pi][rxp].bits, DMA_OWN);
		rxp = (rxp+1) % NUM_RX_BUFFER;
		if (++i > NUM_RX_BUFFER) {
			dbg(DBG_ASS, "intr_rx: Discarding more bufs"
			    " than we have\n");
			break;
		}
		dmabits = FST_RDB(card, rxDescrRing[pi][rxp].bits);
		dbg(DBG_ASS, "DMA Bits of next buffer was %x\n", dmabits);
	}
	dbg(DBG_ASS, "There were %d subsequent buffers in error\n", i);

	
	if (!(dmabits & DMA_OWN)) {
		FST_WRB(card, rxDescrRing[pi][rxp].bits, DMA_OWN);
		rxp = (rxp+1) % NUM_RX_BUFFER;
	}
	port->rxpos = rxp;
	return;

}

static void
fst_intr_rx(struct fst_card_info *card, struct fst_port_info *port)
{
	unsigned char dmabits;
	int pi;
	int rxp;
	int rx_status;
	unsigned short len;
	struct sk_buff *skb;
	struct net_device *dev = port_to_dev(port);

	
	pi = port->index;
	rxp = port->rxpos;
	dmabits = FST_RDB(card, rxDescrRing[pi][rxp].bits);
	if (dmabits & DMA_OWN) {
		dbg(DBG_RX | DBG_INTR, "intr_rx: No buffer port %d pos %d\n",
		    pi, rxp);
		return;
	}
	if (card->dmarx_in_progress) {
		return;
	}

	
	len = FST_RDW(card, rxDescrRing[pi][rxp].mcnt);
	
	len -= 2;
	if (len == 0) {
		pr_err("Frame received with 0 length. Card %d Port %d\n",
		       card->card_no, port->index);
		
		FST_WRB(card, rxDescrRing[pi][rxp].bits, DMA_OWN);

		rxp = (rxp+1) % NUM_RX_BUFFER;
		port->rxpos = rxp;
		return;
	}

	dbg(DBG_RX, "intr_rx: %d,%d: flags %x len %d\n", pi, rxp, dmabits, len);
	if (dmabits != (RX_STP | RX_ENP) || len > LEN_RX_BUFFER - 2) {
		fst_log_rx_error(card, port, dmabits, rxp, len);
		fst_recover_rx_error(card, port, dmabits, rxp, len);
		return;
	}

	
	if ((skb = dev_alloc_skb(len)) == NULL) {
		dbg(DBG_RX, "intr_rx: can't allocate buffer\n");

		dev->stats.rx_dropped++;

		
		FST_WRB(card, rxDescrRing[pi][rxp].bits, DMA_OWN);

		rxp = (rxp+1) % NUM_RX_BUFFER;
		port->rxpos = rxp;
		return;
	}


	if ((len < FST_MIN_DMA_LEN) || (card->family == FST_FAMILY_TXP)) {
		memcpy_fromio(skb_put(skb, len),
			      card->mem + BUF_OFFSET(rxBuffer[pi][rxp][0]),
			      len);

		
		FST_WRB(card, rxDescrRing[pi][rxp].bits, DMA_OWN);

		
		dev->stats.rx_packets++;
		dev->stats.rx_bytes += len;

		
		dbg(DBG_RX, "Pushing frame up the stack\n");
		if (port->mode == FST_RAW)
			skb->protocol = farsync_type_trans(skb, dev);
		else
			skb->protocol = hdlc_type_trans(skb, dev);
		rx_status = netif_rx(skb);
		fst_process_rx_status(rx_status, port_to_dev(port)->name);
		if (rx_status == NET_RX_DROP)
			dev->stats.rx_dropped++;
	} else {
		card->dma_skb_rx = skb;
		card->dma_port_rx = port;
		card->dma_len_rx = len;
		card->dma_rxpos = rxp;
		fst_rx_dma(card, card->rx_dma_handle_card,
			   BUF_OFFSET(rxBuffer[pi][rxp][0]), len);
	}
	if (rxp != port->rxpos) {
		dbg(DBG_ASS, "About to increment rxpos by more than 1\n");
		dbg(DBG_ASS, "rxp = %d rxpos = %d\n", rxp, port->rxpos);
	}
	rxp = (rxp+1) % NUM_RX_BUFFER;
	port->rxpos = rxp;
}


static void
do_bottom_half_tx(struct fst_card_info *card)
{
	struct fst_port_info *port;
	int pi;
	int txq_length;
	struct sk_buff *skb;
	unsigned long flags;
	struct net_device *dev;


	dbg(DBG_TX, "do_bottom_half_tx\n");
	for (pi = 0, port = card->ports; pi < card->nports; pi++, port++) {
		if (!port->run)
			continue;

		dev = port_to_dev(port);
		while (!(FST_RDB(card, txDescrRing[pi][port->txpos].bits) &
			 DMA_OWN) &&
		       !(card->dmatx_in_progress)) {
			spin_lock_irqsave(&card->card_lock, flags);
			if ((txq_length = port->txqe - port->txqs) < 0) {
				txq_length = txq_length + FST_TXQ_DEPTH;
			}
			spin_unlock_irqrestore(&card->card_lock, flags);
			if (txq_length > 0) {
				spin_lock_irqsave(&card->card_lock, flags);
				skb = port->txq[port->txqs];
				port->txqs++;
				if (port->txqs == FST_TXQ_DEPTH) {
					port->txqs = 0;
				}
				spin_unlock_irqrestore(&card->card_lock, flags);
				FST_WRW(card, txDescrRing[pi][port->txpos].bcnt,
					cnv_bcnt(skb->len));
				if ((skb->len < FST_MIN_DMA_LEN) ||
				    (card->family == FST_FAMILY_TXP)) {
					
					memcpy_toio(card->mem +
						    BUF_OFFSET(txBuffer[pi]
							       [port->
								txpos][0]),
						    skb->data, skb->len);
					FST_WRB(card,
						txDescrRing[pi][port->txpos].
						bits,
						DMA_OWN | TX_STP | TX_ENP);
					dev->stats.tx_packets++;
					dev->stats.tx_bytes += skb->len;
					dev->trans_start = jiffies;
				} else {
					
					memcpy(card->tx_dma_handle_host,
					       skb->data, skb->len);
					card->dma_port_tx = port;
					card->dma_len_tx = skb->len;
					card->dma_txpos = port->txpos;
					fst_tx_dma(card,
						   (char *) card->
						   tx_dma_handle_card,
						   (char *)
						   BUF_OFFSET(txBuffer[pi]
							      [port->txpos][0]),
						   skb->len);
				}
				if (++port->txpos >= NUM_TX_BUFFER)
					port->txpos = 0;
				if (port->start) {
					if (txq_length < fst_txq_low) {
						netif_wake_queue(port_to_dev
								 (port));
						port->start = 0;
					}
				}
				dev_kfree_skb(skb);
			} else {
				break;
			}
		}
	}
}

static void
do_bottom_half_rx(struct fst_card_info *card)
{
	struct fst_port_info *port;
	int pi;
	int rx_count = 0;

	
	dbg(DBG_RX, "do_bottom_half_rx\n");
	for (pi = 0, port = card->ports; pi < card->nports; pi++, port++) {
		if (!port->run)
			continue;

		while (!(FST_RDB(card, rxDescrRing[pi][port->rxpos].bits)
			 & DMA_OWN) && !(card->dmarx_in_progress)) {
			if (rx_count > fst_max_reads) {
				fst_q_work_item(&fst_work_intq, card->card_no);
				tasklet_schedule(&fst_int_task);
				break;	
			}
			fst_intr_rx(card, port);
			rx_count++;
		}
	}
}

static irqreturn_t
fst_intr(int dummy, void *dev_id)
{
	struct fst_card_info *card = dev_id;
	struct fst_port_info *port;
	int rdidx;		
	int wridx;
	int event;		
	unsigned int dma_intcsr = 0;
	unsigned int do_card_interrupt;
	unsigned int int_retry_count;

	dbg(DBG_INTR, "intr: %d %p\n", card->irq, card);
	if (card->state != FST_RUNNING) {
		pr_err("Interrupt received for card %d in a non running state (%d)\n",
		       card->card_no, card->state);

		fst_clear_intr(card);
		return IRQ_HANDLED;
	}

	
	fst_clear_intr(card);

	do_card_interrupt = 0;
	if (FST_RDB(card, interruptHandshake) == 1) {
		do_card_interrupt += FST_CARD_INT;
		
		FST_WRB(card, interruptHandshake, 0xEE);
	}
	if (card->family == FST_FAMILY_TXU) {
		dma_intcsr = inl(card->pci_conf + INTCSR_9054);
		if (dma_intcsr & 0x00200000) {
			dbg(DBG_RX, "DMA Rx xfer complete\n");
			outb(0x8, card->pci_conf + DMACSR0);
			fst_rx_dma_complete(card, card->dma_port_rx,
					    card->dma_len_rx, card->dma_skb_rx,
					    card->dma_rxpos);
			card->dmarx_in_progress = 0;
			do_card_interrupt += FST_RX_DMA_INT;
		}
		if (dma_intcsr & 0x00400000) {
			dbg(DBG_TX, "DMA Tx xfer complete\n");
			outb(0x8, card->pci_conf + DMACSR1);
			fst_tx_dma_complete(card, card->dma_port_tx,
					    card->dma_len_tx, card->dma_txpos);
			card->dmatx_in_progress = 0;
			do_card_interrupt += FST_TX_DMA_INT;
		}
	}

	int_retry_count = FST_RDL(card, interruptRetryCount);
	if (int_retry_count) {
		dbg(DBG_ASS, "Card %d int_retry_count is  %d\n",
		    card->card_no, int_retry_count);
		FST_WRL(card, interruptRetryCount, 0);
	}

	if (!do_card_interrupt) {
		return IRQ_HANDLED;
	}

	
	fst_q_work_item(&fst_work_intq, card->card_no);
	tasklet_schedule(&fst_int_task);

	
	rdidx = FST_RDB(card, interruptEvent.rdindex) & 0x1f;
	wridx = FST_RDB(card, interruptEvent.wrindex) & 0x1f;
	while (rdidx != wridx) {
		event = FST_RDB(card, interruptEvent.evntbuff[rdidx]);
		port = &card->ports[event & 0x03];

		dbg(DBG_INTR, "Processing Interrupt event: %x\n", event);

		switch (event) {
		case TE1_ALMA:
			dbg(DBG_INTR, "TE1 Alarm intr\n");
			if (port->run)
				fst_intr_te1_alarm(card, port);
			break;

		case CTLA_CHG:
		case CTLB_CHG:
		case CTLC_CHG:
		case CTLD_CHG:
			if (port->run)
				fst_intr_ctlchg(card, port);
			break;

		case ABTA_SENT:
		case ABTB_SENT:
		case ABTC_SENT:
		case ABTD_SENT:
			dbg(DBG_TX, "Abort complete port %d\n", port->index);
			break;

		case TXA_UNDF:
		case TXB_UNDF:
		case TXC_UNDF:
		case TXD_UNDF:
			dbg(DBG_TX, "Tx underflow port %d\n", port->index);
			port_to_dev(port)->stats.tx_errors++;
			port_to_dev(port)->stats.tx_fifo_errors++;
			dbg(DBG_ASS, "Tx underflow on card %d port %d\n",
			    card->card_no, port->index);
			break;

		case INIT_CPLT:
			dbg(DBG_INIT, "Card init OK intr\n");
			break;

		case INIT_FAIL:
			dbg(DBG_INIT, "Card init FAILED intr\n");
			card->state = FST_IFAILED;
			break;

		default:
			pr_err("intr: unknown card event %d. ignored\n", event);
			break;
		}

		
		if (++rdidx >= MAX_CIRBUFF)
			rdidx = 0;
	}
	FST_WRB(card, interruptEvent.rdindex, rdidx);
        return IRQ_HANDLED;
}

static void
check_started_ok(struct fst_card_info *card)
{
	int i;

	
	if (FST_RDW(card, smcVersion) != SMC_VERSION) {
		pr_err("Bad shared memory version %d expected %d\n",
		       FST_RDW(card, smcVersion), SMC_VERSION);
		card->state = FST_BADVERSION;
		return;
	}
	if (FST_RDL(card, endOfSmcSignature) != END_SIG) {
		pr_err("Missing shared memory signature\n");
		card->state = FST_BADVERSION;
		return;
	}
	
	if ((i = FST_RDB(card, taskStatus)) == 0x01) {
		card->state = FST_RUNNING;
	} else if (i == 0xFF) {
		pr_err("Firmware initialisation failed. Card halted\n");
		card->state = FST_HALTED;
		return;
	} else if (i != 0x00) {
		pr_err("Unknown firmware status 0x%x\n", i);
		card->state = FST_HALTED;
		return;
	}

	if (FST_RDL(card, numberOfPorts) != card->nports) {
		pr_warn("Port count mismatch on card %d.  Firmware thinks %d we say %d\n",
			card->card_no,
			FST_RDL(card, numberOfPorts), card->nports);
	}
}

static int
set_conf_from_info(struct fst_card_info *card, struct fst_port_info *port,
		   struct fstioc_info *info)
{
	int err;
	unsigned char my_framing;

	err = 0;
	if (info->valid & FSTVAL_PROTO) {
		if (info->proto == FST_RAW)
			port->mode = FST_RAW;
		else
			port->mode = FST_GEN_HDLC;
	}

	if (info->valid & FSTVAL_CABLE)
		err = -EINVAL;

	if (info->valid & FSTVAL_SPEED)
		err = -EINVAL;

	if (info->valid & FSTVAL_PHASE)
		FST_WRB(card, portConfig[port->index].invertClock,
			info->invertClock);
	if (info->valid & FSTVAL_MODE)
		FST_WRW(card, cardMode, info->cardMode);
	if (info->valid & FSTVAL_TE1) {
		FST_WRL(card, suConfig.dataRate, info->lineSpeed);
		FST_WRB(card, suConfig.clocking, info->clockSource);
		my_framing = FRAMING_E1;
		if (info->framing == E1)
			my_framing = FRAMING_E1;
		if (info->framing == T1)
			my_framing = FRAMING_T1;
		if (info->framing == J1)
			my_framing = FRAMING_J1;
		FST_WRB(card, suConfig.framing, my_framing);
		FST_WRB(card, suConfig.structure, info->structure);
		FST_WRB(card, suConfig.interface, info->interface);
		FST_WRB(card, suConfig.coding, info->coding);
		FST_WRB(card, suConfig.lineBuildOut, info->lineBuildOut);
		FST_WRB(card, suConfig.equalizer, info->equalizer);
		FST_WRB(card, suConfig.transparentMode, info->transparentMode);
		FST_WRB(card, suConfig.loopMode, info->loopMode);
		FST_WRB(card, suConfig.range, info->range);
		FST_WRB(card, suConfig.txBufferMode, info->txBufferMode);
		FST_WRB(card, suConfig.rxBufferMode, info->rxBufferMode);
		FST_WRB(card, suConfig.startingSlot, info->startingSlot);
		FST_WRB(card, suConfig.losThreshold, info->losThreshold);
		if (info->idleCode)
			FST_WRB(card, suConfig.enableIdleCode, 1);
		else
			FST_WRB(card, suConfig.enableIdleCode, 0);
		FST_WRB(card, suConfig.idleCode, info->idleCode);
#if FST_DEBUG
		if (info->valid & FSTVAL_TE1) {
			printk("Setting TE1 data\n");
			printk("Line Speed = %d\n", info->lineSpeed);
			printk("Start slot = %d\n", info->startingSlot);
			printk("Clock source = %d\n", info->clockSource);
			printk("Framing = %d\n", my_framing);
			printk("Structure = %d\n", info->structure);
			printk("interface = %d\n", info->interface);
			printk("Coding = %d\n", info->coding);
			printk("Line build out = %d\n", info->lineBuildOut);
			printk("Equaliser = %d\n", info->equalizer);
			printk("Transparent mode = %d\n",
			       info->transparentMode);
			printk("Loop mode = %d\n", info->loopMode);
			printk("Range = %d\n", info->range);
			printk("Tx Buffer mode = %d\n", info->txBufferMode);
			printk("Rx Buffer mode = %d\n", info->rxBufferMode);
			printk("LOS Threshold = %d\n", info->losThreshold);
			printk("Idle Code = %d\n", info->idleCode);
		}
#endif
	}
#if FST_DEBUG
	if (info->valid & FSTVAL_DEBUG) {
		fst_debug_mask = info->debug;
	}
#endif

	return err;
}

static void
gather_conf_info(struct fst_card_info *card, struct fst_port_info *port,
		 struct fstioc_info *info)
{
	int i;

	memset(info, 0, sizeof (struct fstioc_info));

	i = port->index;
	info->kernelVersion = LINUX_VERSION_CODE;
	info->nports = card->nports;
	info->type = card->type;
	info->state = card->state;
	info->proto = FST_GEN_HDLC;
	info->index = i;
#if FST_DEBUG
	info->debug = fst_debug_mask;
#endif

	info->valid = ((card->state == FST_RUNNING) ? FSTVAL_ALL : FSTVAL_CARD)
#if FST_DEBUG
	    | FSTVAL_DEBUG
#endif
	    ;

	info->lineInterface = FST_RDW(card, portConfig[i].lineInterface);
	info->internalClock = FST_RDB(card, portConfig[i].internalClock);
	info->lineSpeed = FST_RDL(card, portConfig[i].lineSpeed);
	info->invertClock = FST_RDB(card, portConfig[i].invertClock);
	info->v24IpSts = FST_RDL(card, v24IpSts[i]);
	info->v24OpSts = FST_RDL(card, v24OpSts[i]);
	info->clockStatus = FST_RDW(card, clockStatus[i]);
	info->cableStatus = FST_RDW(card, cableStatus);
	info->cardMode = FST_RDW(card, cardMode);
	info->smcFirmwareVersion = FST_RDL(card, smcFirmwareVersion);

	if (card->family == FST_FAMILY_TXU) {
		if (port->index == 0) {
			info->cableStatus = info->cableStatus & 1;
		} else {
			info->cableStatus = info->cableStatus >> 1;
			info->cableStatus = info->cableStatus & 1;
		}
	}
	if (card->type == FST_TYPE_TE1) {
		info->lineSpeed = FST_RDL(card, suConfig.dataRate);
		info->clockSource = FST_RDB(card, suConfig.clocking);
		info->framing = FST_RDB(card, suConfig.framing);
		info->structure = FST_RDB(card, suConfig.structure);
		info->interface = FST_RDB(card, suConfig.interface);
		info->coding = FST_RDB(card, suConfig.coding);
		info->lineBuildOut = FST_RDB(card, suConfig.lineBuildOut);
		info->equalizer = FST_RDB(card, suConfig.equalizer);
		info->loopMode = FST_RDB(card, suConfig.loopMode);
		info->range = FST_RDB(card, suConfig.range);
		info->txBufferMode = FST_RDB(card, suConfig.txBufferMode);
		info->rxBufferMode = FST_RDB(card, suConfig.rxBufferMode);
		info->startingSlot = FST_RDB(card, suConfig.startingSlot);
		info->losThreshold = FST_RDB(card, suConfig.losThreshold);
		if (FST_RDB(card, suConfig.enableIdleCode))
			info->idleCode = FST_RDB(card, suConfig.idleCode);
		else
			info->idleCode = 0;
		info->receiveBufferDelay =
		    FST_RDL(card, suStatus.receiveBufferDelay);
		info->framingErrorCount =
		    FST_RDL(card, suStatus.framingErrorCount);
		info->codeViolationCount =
		    FST_RDL(card, suStatus.codeViolationCount);
		info->crcErrorCount = FST_RDL(card, suStatus.crcErrorCount);
		info->lineAttenuation = FST_RDL(card, suStatus.lineAttenuation);
		info->lossOfSignal = FST_RDB(card, suStatus.lossOfSignal);
		info->receiveRemoteAlarm =
		    FST_RDB(card, suStatus.receiveRemoteAlarm);
		info->alarmIndicationSignal =
		    FST_RDB(card, suStatus.alarmIndicationSignal);
	}
}

static int
fst_set_iface(struct fst_card_info *card, struct fst_port_info *port,
	      struct ifreq *ifr)
{
	sync_serial_settings sync;
	int i;

	if (ifr->ifr_settings.size != sizeof (sync)) {
		return -ENOMEM;
	}

	if (copy_from_user
	    (&sync, ifr->ifr_settings.ifs_ifsu.sync, sizeof (sync))) {
		return -EFAULT;
	}

	if (sync.loopback)
		return -EINVAL;

	i = port->index;

	switch (ifr->ifr_settings.type) {
	case IF_IFACE_V35:
		FST_WRW(card, portConfig[i].lineInterface, V35);
		port->hwif = V35;
		break;

	case IF_IFACE_V24:
		FST_WRW(card, portConfig[i].lineInterface, V24);
		port->hwif = V24;
		break;

	case IF_IFACE_X21:
		FST_WRW(card, portConfig[i].lineInterface, X21);
		port->hwif = X21;
		break;

	case IF_IFACE_X21D:
		FST_WRW(card, portConfig[i].lineInterface, X21D);
		port->hwif = X21D;
		break;

	case IF_IFACE_T1:
		FST_WRW(card, portConfig[i].lineInterface, T1);
		port->hwif = T1;
		break;

	case IF_IFACE_E1:
		FST_WRW(card, portConfig[i].lineInterface, E1);
		port->hwif = E1;
		break;

	case IF_IFACE_SYNC_SERIAL:
		break;

	default:
		return -EINVAL;
	}

	switch (sync.clock_type) {
	case CLOCK_EXT:
		FST_WRB(card, portConfig[i].internalClock, EXTCLK);
		break;

	case CLOCK_INT:
		FST_WRB(card, portConfig[i].internalClock, INTCLK);
		break;

	default:
		return -EINVAL;
	}
	FST_WRL(card, portConfig[i].lineSpeed, sync.clock_rate);
	return 0;
}

static int
fst_get_iface(struct fst_card_info *card, struct fst_port_info *port,
	      struct ifreq *ifr)
{
	sync_serial_settings sync;
	int i;

	switch (port->hwif) {
	case E1:
		ifr->ifr_settings.type = IF_IFACE_E1;
		break;
	case T1:
		ifr->ifr_settings.type = IF_IFACE_T1;
		break;
	case V35:
		ifr->ifr_settings.type = IF_IFACE_V35;
		break;
	case V24:
		ifr->ifr_settings.type = IF_IFACE_V24;
		break;
	case X21D:
		ifr->ifr_settings.type = IF_IFACE_X21D;
		break;
	case X21:
	default:
		ifr->ifr_settings.type = IF_IFACE_X21;
		break;
	}
	if (ifr->ifr_settings.size == 0) {
		return 0;	
	}
	if (ifr->ifr_settings.size < sizeof (sync)) {
		return -ENOMEM;
	}

	i = port->index;
	sync.clock_rate = FST_RDL(card, portConfig[i].lineSpeed);
	
	sync.clock_type = FST_RDB(card, portConfig[i].internalClock) ==
	    INTCLK ? CLOCK_INT : CLOCK_EXT;
	sync.loopback = 0;

	if (copy_to_user(ifr->ifr_settings.ifs_ifsu.sync, &sync, sizeof (sync))) {
		return -EFAULT;
	}

	ifr->ifr_settings.size = sizeof (sync);
	return 0;
}

static int
fst_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct fst_card_info *card;
	struct fst_port_info *port;
	struct fstioc_write wrthdr;
	struct fstioc_info info;
	unsigned long flags;
	void *buf;

	dbg(DBG_IOCTL, "ioctl: %x, %p\n", cmd, ifr->ifr_data);

	port = dev_to_port(dev);
	card = port->card;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	switch (cmd) {
	case FSTCPURESET:
		fst_cpureset(card);
		card->state = FST_RESET;
		return 0;

	case FSTCPURELEASE:
		fst_cpurelease(card);
		card->state = FST_STARTING;
		return 0;

	case FSTWRITE:		

		if (ifr->ifr_data == NULL) {
			return -EINVAL;
		}
		if (copy_from_user(&wrthdr, ifr->ifr_data,
				   sizeof (struct fstioc_write))) {
			return -EFAULT;
		}

		if (wrthdr.size > FST_MEMSIZE || wrthdr.offset > FST_MEMSIZE ||
		    wrthdr.size + wrthdr.offset > FST_MEMSIZE) {
			return -ENXIO;
		}

		

		buf = memdup_user(ifr->ifr_data + sizeof(struct fstioc_write),
				  wrthdr.size);
		if (IS_ERR(buf))
			return PTR_ERR(buf);

		memcpy_toio(card->mem + wrthdr.offset, buf, wrthdr.size);
		kfree(buf);

		if (card->state == FST_RESET) {
			card->state = FST_DOWNLOAD;
		}
		return 0;

	case FSTGETCONF:

		if (card->state == FST_STARTING) {
			check_started_ok(card);

			
			if (card->state == FST_RUNNING) {
				spin_lock_irqsave(&card->card_lock, flags);
				fst_enable_intr(card);
				FST_WRB(card, interruptHandshake, 0xEE);
				spin_unlock_irqrestore(&card->card_lock, flags);
			}
		}

		if (ifr->ifr_data == NULL) {
			return -EINVAL;
		}

		gather_conf_info(card, port, &info);

		if (copy_to_user(ifr->ifr_data, &info, sizeof (info))) {
			return -EFAULT;
		}
		return 0;

	case FSTSETCONF:


		if (card->state != FST_RUNNING) {
			pr_err("Attempt to configure card %d in non-running state (%d)\n",
			       card->card_no, card->state);
			return -EIO;
		}
		if (copy_from_user(&info, ifr->ifr_data, sizeof (info))) {
			return -EFAULT;
		}

		return set_conf_from_info(card, port, &info);

	case SIOCWANDEV:
		switch (ifr->ifr_settings.type) {
		case IF_GET_IFACE:
			return fst_get_iface(card, port, ifr);

		case IF_IFACE_SYNC_SERIAL:
		case IF_IFACE_V35:
		case IF_IFACE_V24:
		case IF_IFACE_X21:
		case IF_IFACE_X21D:
		case IF_IFACE_T1:
		case IF_IFACE_E1:
			return fst_set_iface(card, port, ifr);

		case IF_PROTO_RAW:
			port->mode = FST_RAW;
			return 0;

		case IF_GET_PROTO:
			if (port->mode == FST_RAW) {
				ifr->ifr_settings.type = IF_PROTO_RAW;
				return 0;
			}
			return hdlc_ioctl(dev, ifr, cmd);

		default:
			port->mode = FST_GEN_HDLC;
			dbg(DBG_IOCTL, "Passing this type to hdlc %x\n",
			    ifr->ifr_settings.type);
			return hdlc_ioctl(dev, ifr, cmd);
		}

	default:
		
		return hdlc_ioctl(dev, ifr, cmd);
	}
}

static void
fst_openport(struct fst_port_info *port)
{
	int signals;
	int txq_length;

	if (port->card->state == FST_RUNNING) {
		if (port->run) {
			dbg(DBG_OPEN, "open: found port already running\n");

			fst_issue_cmd(port, STOPPORT);
			port->run = 0;
		}

		fst_rx_config(port);
		fst_tx_config(port);
		fst_op_raise(port, OPSTS_RTS | OPSTS_DTR);

		fst_issue_cmd(port, STARTPORT);
		port->run = 1;

		signals = FST_RDL(port->card, v24DebouncedSts[port->index]);
		if (signals & (((port->hwif == X21) || (port->hwif == X21D))
			       ? IPSTS_INDICATE : IPSTS_DCD))
			netif_carrier_on(port_to_dev(port));
		else
			netif_carrier_off(port_to_dev(port));

		txq_length = port->txqe - port->txqs;
		port->txqe = 0;
		port->txqs = 0;
	}

}

static void
fst_closeport(struct fst_port_info *port)
{
	if (port->card->state == FST_RUNNING) {
		if (port->run) {
			port->run = 0;
			fst_op_lower(port, OPSTS_RTS | OPSTS_DTR);

			fst_issue_cmd(port, STOPPORT);
		} else {
			dbg(DBG_OPEN, "close: port not running\n");
		}
	}
}

static int
fst_open(struct net_device *dev)
{
	int err;
	struct fst_port_info *port;

	port = dev_to_port(dev);
	if (!try_module_get(THIS_MODULE))
          return -EBUSY;

	if (port->mode != FST_RAW) {
		err = hdlc_open(dev);
		if (err) {
			module_put(THIS_MODULE);
			return err;
		}
	}

	fst_openport(port);
	netif_wake_queue(dev);
	return 0;
}

static int
fst_close(struct net_device *dev)
{
	struct fst_port_info *port;
	struct fst_card_info *card;
	unsigned char tx_dma_done;
	unsigned char rx_dma_done;

	port = dev_to_port(dev);
	card = port->card;

	tx_dma_done = inb(card->pci_conf + DMACSR1);
	rx_dma_done = inb(card->pci_conf + DMACSR0);
	dbg(DBG_OPEN,
	    "Port Close: tx_dma_in_progress = %d (%x) rx_dma_in_progress = %d (%x)\n",
	    card->dmatx_in_progress, tx_dma_done, card->dmarx_in_progress,
	    rx_dma_done);

	netif_stop_queue(dev);
	fst_closeport(dev_to_port(dev));
	if (port->mode != FST_RAW) {
		hdlc_close(dev);
	}
	module_put(THIS_MODULE);
	return 0;
}

static int
fst_attach(struct net_device *dev, unsigned short encoding, unsigned short parity)
{
	if (encoding != ENCODING_NRZ || parity != PARITY_CRC16_PR1_CCITT)
		return -EINVAL;
	return 0;
}

static void
fst_tx_timeout(struct net_device *dev)
{
	struct fst_port_info *port;
	struct fst_card_info *card;

	port = dev_to_port(dev);
	card = port->card;
	dev->stats.tx_errors++;
	dev->stats.tx_aborted_errors++;
	dbg(DBG_ASS, "Tx timeout card %d port %d\n",
	    card->card_no, port->index);
	fst_issue_cmd(port, ABORTTX);

	dev->trans_start = jiffies;
	netif_wake_queue(dev);
	port->start = 0;
}

static netdev_tx_t
fst_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct fst_card_info *card;
	struct fst_port_info *port;
	unsigned long flags;
	int txq_length;

	port = dev_to_port(dev);
	card = port->card;
	dbg(DBG_TX, "fst_start_xmit: length = %d\n", skb->len);

	
	if (!netif_carrier_ok(dev)) {
		dev_kfree_skb(skb);
		dev->stats.tx_errors++;
		dev->stats.tx_carrier_errors++;
		dbg(DBG_ASS,
		    "Tried to transmit but no carrier on card %d port %d\n",
		    card->card_no, port->index);
		return NETDEV_TX_OK;
	}

	
	if (skb->len > LEN_TX_BUFFER) {
		dbg(DBG_ASS, "Packet too large %d vs %d\n", skb->len,
		    LEN_TX_BUFFER);
		dev_kfree_skb(skb);
		dev->stats.tx_errors++;
		return NETDEV_TX_OK;
	}

	spin_lock_irqsave(&card->card_lock, flags);
	if ((txq_length = port->txqe - port->txqs) < 0) {
		txq_length = txq_length + FST_TXQ_DEPTH;
	}
	spin_unlock_irqrestore(&card->card_lock, flags);
	if (txq_length > fst_txq_high) {
		netif_stop_queue(dev);
		port->start = 1;	
	}

	if (txq_length == FST_TXQ_DEPTH - 1) {
		dev_kfree_skb(skb);
		dev->stats.tx_errors++;
		dbg(DBG_ASS, "Tx queue overflow card %d port %d\n",
		    card->card_no, port->index);
		return NETDEV_TX_OK;
	}

	spin_lock_irqsave(&card->card_lock, flags);
	port->txq[port->txqe] = skb;
	port->txqe++;
	if (port->txqe == FST_TXQ_DEPTH)
		port->txqe = 0;
	spin_unlock_irqrestore(&card->card_lock, flags);

	
	fst_q_work_item(&fst_work_txq, card->card_no);
	tasklet_schedule(&fst_tx_task);

	return NETDEV_TX_OK;
}

static char *type_strings[] __devinitdata = {
	"no hardware",		
	"FarSync T2P",
	"FarSync T4P",
	"FarSync T1U",
	"FarSync T2U",
	"FarSync T4U",
	"FarSync TE1"
};

static void __devinit
fst_init_card(struct fst_card_info *card)
{
	int i;
	int err;

	for (i = 0; i < card->nports; i++) {
                err = register_hdlc_device(card->ports[i].dev);
                if (err < 0) {
			int j;
			pr_err("Cannot register HDLC device for port %d (errno %d)\n",
			       i, -err);
			for (j = i; j < card->nports; j++) {
				free_netdev(card->ports[j].dev);
				card->ports[j].dev = NULL;
			}
                        card->nports = i;
                        break;
                }
	}

	pr_info("%s-%s: %s IRQ%d, %d ports\n",
		port_to_dev(&card->ports[0])->name,
		port_to_dev(&card->ports[card->nports - 1])->name,
		type_strings[card->type], card->irq, card->nports);
}

static const struct net_device_ops fst_ops = {
	.ndo_open       = fst_open,
	.ndo_stop       = fst_close,
	.ndo_change_mtu = hdlc_change_mtu,
	.ndo_start_xmit = hdlc_start_xmit,
	.ndo_do_ioctl   = fst_ioctl,
	.ndo_tx_timeout = fst_tx_timeout,
};

static int __devinit
fst_add_one(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	static int no_of_cards_added = 0;
	struct fst_card_info *card;
	int err = 0;
	int i;

	printk_once(KERN_INFO
		    pr_fmt("FarSync WAN driver " FST_USER_VERSION
			   " (c) 2001-2004 FarSite Communications Ltd.\n"));
#if FST_DEBUG
	dbg(DBG_ASS, "The value of debug mask is %x\n", fst_debug_mask);
#endif
	if (fst_excluded_cards != 0) {
		for (i = 0; i < fst_excluded_cards; i++) {
			if ((pdev->devfn) >> 3 == fst_excluded_list[i]) {
				pr_info("FarSync PCI device %d not assigned\n",
					(pdev->devfn) >> 3);
				return -EBUSY;
			}
		}
	}

	
	card = kzalloc(sizeof (struct fst_card_info), GFP_KERNEL);
	if (card == NULL) {
		pr_err("FarSync card found but insufficient memory for driver storage\n");
		return -ENOMEM;
	}

	
	if ((err = pci_enable_device(pdev)) != 0) {
		pr_err("Failed to enable card. Err %d\n", -err);
		kfree(card);
		return err;
	}

	if ((err = pci_request_regions(pdev, "FarSync")) !=0) {
		pr_err("Failed to allocate regions. Err %d\n", -err);
		pci_disable_device(pdev);
		kfree(card);
	        return err;
	}

	
	card->pci_conf = pci_resource_start(pdev, 1);
	card->phys_mem = pci_resource_start(pdev, 2);
	card->phys_ctlmem = pci_resource_start(pdev, 3);
	if ((card->mem = ioremap(card->phys_mem, FST_MEMSIZE)) == NULL) {
		pr_err("Physical memory remap failed\n");
		pci_release_regions(pdev);
		pci_disable_device(pdev);
		kfree(card);
		return -ENODEV;
	}
	if ((card->ctlmem = ioremap(card->phys_ctlmem, 0x10)) == NULL) {
		pr_err("Control memory remap failed\n");
		pci_release_regions(pdev);
		pci_disable_device(pdev);
		iounmap(card->mem);
		kfree(card);
		return -ENODEV;
	}
	dbg(DBG_PCI, "kernel mem %p, ctlmem %p\n", card->mem, card->ctlmem);

	
	if (request_irq(pdev->irq, fst_intr, IRQF_SHARED, FST_DEV_NAME, card)) {
		pr_err("Unable to register interrupt %d\n", card->irq);
		pci_release_regions(pdev);
		pci_disable_device(pdev);
		iounmap(card->ctlmem);
		iounmap(card->mem);
		kfree(card);
		return -ENODEV;
	}

	
	card->irq = pdev->irq;
	card->type = ent->driver_data;
	card->family = ((ent->driver_data == FST_TYPE_T2P) ||
			(ent->driver_data == FST_TYPE_T4P))
	    ? FST_FAMILY_TXP : FST_FAMILY_TXU;
	if ((ent->driver_data == FST_TYPE_T1U) ||
	    (ent->driver_data == FST_TYPE_TE1))
		card->nports = 1;
	else
		card->nports = ((ent->driver_data == FST_TYPE_T2P) ||
				(ent->driver_data == FST_TYPE_T2U)) ? 2 : 4;

	card->state = FST_UNINIT;
        spin_lock_init ( &card->card_lock );

        for ( i = 0 ; i < card->nports ; i++ ) {
		struct net_device *dev = alloc_hdlcdev(&card->ports[i]);
		hdlc_device *hdlc;
		if (!dev) {
			while (i--)
				free_netdev(card->ports[i].dev);
			pr_err("FarSync: out of memory\n");
                        free_irq(card->irq, card);
                        pci_release_regions(pdev);
                        pci_disable_device(pdev);
                        iounmap(card->ctlmem);
                        iounmap(card->mem);
                        kfree(card);
                        return -ENODEV;
		}
		card->ports[i].dev    = dev;
                card->ports[i].card   = card;
                card->ports[i].index  = i;
                card->ports[i].run    = 0;

		hdlc = dev_to_hdlc(dev);

                
                dev->mem_start   = card->phys_mem
                                 + BUF_OFFSET ( txBuffer[i][0][0]);
                dev->mem_end     = card->phys_mem
                                 + BUF_OFFSET ( txBuffer[i][NUM_TX_BUFFER][0]);
                dev->base_addr   = card->pci_conf;
                dev->irq         = card->irq;

		dev->netdev_ops = &fst_ops;
		dev->tx_queue_len = FST_TX_QUEUE_LEN;
		dev->watchdog_timeo = FST_TX_TIMEOUT;
                hdlc->attach = fst_attach;
                hdlc->xmit   = fst_start_xmit;
	}

	card->device = pdev;

	dbg(DBG_PCI, "type %d nports %d irq %d\n", card->type,
	    card->nports, card->irq);
	dbg(DBG_PCI, "conf %04x mem %08x ctlmem %08x\n",
	    card->pci_conf, card->phys_mem, card->phys_ctlmem);

	
	fst_cpureset(card);
	card->state = FST_RESET;

	
	fst_init_dma(card);

	
	pci_set_drvdata(pdev, card);

	
	fst_card_array[no_of_cards_added] = card;
	card->card_no = no_of_cards_added++;	
	fst_init_card(card);
	if (card->family == FST_FAMILY_TXU) {
		card->rx_dma_handle_host =
		    pci_alloc_consistent(card->device, FST_MAX_MTU,
					 &card->rx_dma_handle_card);
		if (card->rx_dma_handle_host == NULL) {
			pr_err("Could not allocate rx dma buffer\n");
			fst_disable_intr(card);
			pci_release_regions(pdev);
			pci_disable_device(pdev);
			iounmap(card->ctlmem);
			iounmap(card->mem);
			kfree(card);
			return -ENOMEM;
		}
		card->tx_dma_handle_host =
		    pci_alloc_consistent(card->device, FST_MAX_MTU,
					 &card->tx_dma_handle_card);
		if (card->tx_dma_handle_host == NULL) {
			pr_err("Could not allocate tx dma buffer\n");
			fst_disable_intr(card);
			pci_release_regions(pdev);
			pci_disable_device(pdev);
			iounmap(card->ctlmem);
			iounmap(card->mem);
			kfree(card);
			return -ENOMEM;
		}
	}
	return 0;		
}

static void __devexit
fst_remove_one(struct pci_dev *pdev)
{
	struct fst_card_info *card;
	int i;

	card = pci_get_drvdata(pdev);

	for (i = 0; i < card->nports; i++) {
		struct net_device *dev = port_to_dev(&card->ports[i]);
		unregister_hdlc_device(dev);
	}

	fst_disable_intr(card);
	free_irq(card->irq, card);

	iounmap(card->ctlmem);
	iounmap(card->mem);
	pci_release_regions(pdev);
	if (card->family == FST_FAMILY_TXU) {
		pci_free_consistent(card->device, FST_MAX_MTU,
				    card->rx_dma_handle_host,
				    card->rx_dma_handle_card);
		pci_free_consistent(card->device, FST_MAX_MTU,
				    card->tx_dma_handle_host,
				    card->tx_dma_handle_card);
	}
	fst_card_array[card->card_no] = NULL;
}

static struct pci_driver fst_driver = {
        .name		= FST_NAME,
        .id_table	= fst_pci_dev_id,
        .probe		= fst_add_one,
        .remove	= __devexit_p(fst_remove_one),
        .suspend	= NULL,
        .resume	= NULL,
};

static int __init
fst_init(void)
{
	int i;

	for (i = 0; i < FST_MAX_CARDS; i++)
		fst_card_array[i] = NULL;
	spin_lock_init(&fst_work_q_lock);
	return pci_register_driver(&fst_driver);
}

static void __exit
fst_cleanup_module(void)
{
	pr_info("FarSync WAN driver unloading\n");
	pci_unregister_driver(&fst_driver);
}

module_init(fst_init);
module_exit(fst_cleanup_module);
