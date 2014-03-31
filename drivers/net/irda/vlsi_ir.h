
/*********************************************************************
 *
 *	vlsi_ir.h:	VLSI82C147 PCI IrDA controller driver for Linux
 *
 *	Version:	0.5
 *
 *	Copyright (c) 2001-2003 Martin Diehl
 *
 *	This program is free software; you can redistribute it and/or 
 *	modify it under the terms of the GNU General Public License as 
 *	published by the Free Software Foundation; either version 2 of 
 *	the License, or (at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License 
 *	along with this program; if not, write to the Free Software 
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 *	MA 02111-1307 USA
 *
 ********************************************************************/

#ifndef IRDA_VLSI_FIR_H
#define IRDA_VLSI_FIR_H



#ifndef PCI_CLASS_WIRELESS_IRDA
#define PCI_CLASS_WIRELESS_IRDA		0x0d00
#endif

#ifndef PCI_CLASS_SUBCLASS_MASK
#define PCI_CLASS_SUBCLASS_MASK		0xffff
#endif



enum vlsi_pci_regs {
	VLSI_PCI_CLKCTL		= 0x40,		
	VLSI_PCI_MSTRPAGE	= 0x41,		
	VLSI_PCI_IRMISC		= 0x42		
};




enum vlsi_pci_clkctl {

	

	CLKCTL_PD_INV		= 0x04,		
	CLKCTL_LOCK		= 0x40,		

	

	CLKCTL_EXTCLK		= 0x20,		
	CLKCTL_XCKSEL		= 0x10,		

	

	CLKCTL_CLKSTP		= 0x80,		
	CLKCTL_WAKE		= 0x08		
};



#define DMA_MASK_USED_BY_HW	0xffffffff
#define DMA_MASK_MSTRPAGE	0x00ffffff
#define MSTRPAGE_VALUE		(DMA_MASK_MSTRPAGE >> 24)





enum vlsi_pci_irmisc {

	

	IRMISC_IRRAIL		= 0x40,		
	IRMISC_IRPD		= 0x08,		

	

	IRMISC_UARTTST		= 0x80,		
	IRMISC_UARTEN		= 0x04,		

	

	IRMISC_UARTSEL_3f8	= 0x00,
	IRMISC_UARTSEL_2f8	= 0x01,
	IRMISC_UARTSEL_3e8	= 0x02,
	IRMISC_UARTSEL_2e8	= 0x03
};




enum vlsi_pio_regs {
	VLSI_PIO_IRINTR		= 0x00,		
	VLSI_PIO_RINGPTR	= 0x02,		
	VLSI_PIO_RINGBASE	= 0x04,		
	VLSI_PIO_RINGSIZE	= 0x06,		
	VLSI_PIO_PROMPT		= 0x08, 	
	
	VLSI_PIO_IRCFG		= 0x10,		
	VLSI_PIO_SIRFLAG	= 0x12,		
	VLSI_PIO_IRENABLE	= 0x14,		
	VLSI_PIO_PHYCTL		= 0x16,		
	VLSI_PIO_NPHYCTL	= 0x18,		
	VLSI_PIO_MAXPKT		= 0x1a,		
	VLSI_PIO_RCVBCNT	= 0x1c		
	
};




enum vlsi_pio_irintr {
	IRINTR_ACTEN	= 0x80,	
	IRINTR_ACTIVITY	= 0x40,	
	IRINTR_RPKTEN	= 0x20,	
	IRINTR_RPKTINT	= 0x10,	
	IRINTR_TPKTEN	= 0x08,	
	IRINTR_TPKTINT	= 0x04,	
	IRINTR_OE_EN	= 0x02,	
	IRINTR_OE_INT	= 0x01	
};


#define IRINTR_INT_MASK		(IRINTR_ACTIVITY|IRINTR_RPKTINT|IRINTR_TPKTINT)




#define MAX_RING_DESCR		64	

#define RINGPTR_RX_MASK		(MAX_RING_DESCR-1)
#define RINGPTR_TX_MASK		((MAX_RING_DESCR-1)<<8)

#define RINGPTR_GET_RX(p)	((p)&RINGPTR_RX_MASK)
#define RINGPTR_GET_TX(p)	(((p)&RINGPTR_TX_MASK)>>8)




#define BUS_TO_RINGBASE(p)	(((p)>>10)&0x3fff)




#define SIZE_TO_BITS(num)		((((num)-1)>>2)&0x0f)
#define TX_RX_TO_RINGSIZE(tx,rx)	((SIZE_TO_BITS(tx)<<12)|(SIZE_TO_BITS(rx)<<8))
#define RINGSIZE_TO_RXSIZE(rs)		((((rs)&0x0f00)>>6)+4)
#define RINGSIZE_TO_TXSIZE(rs)		((((rs)&0xf000)>>10)+4)








enum vlsi_pio_ircfg {
	IRCFG_LOOP	= 0x4000,	
	IRCFG_ENTX	= 0x1000,	
	IRCFG_ENRX	= 0x0800,	
	IRCFG_MSTR	= 0x0400,	
	IRCFG_RXANY	= 0x0200,	
	IRCFG_CRC16	= 0x0080,	
	IRCFG_FIR	= 0x0040,	
	IRCFG_MIR	= 0x0020,	
	IRCFG_SIR	= 0x0010,	
	IRCFG_SIRFILT	= 0x0008,	
	IRCFG_SIRTEST	= 0x0004,	
	IRCFG_TXPOL	= 0x0002,	
	IRCFG_RXPOL	= 0x0001	
};







enum vlsi_pio_irenable {
	IRENABLE_PHYANDCLOCK	= 0x8000,  
	IRENABLE_CFGER		= 0x4000,  
	IRENABLE_FIR_ON		= 0x2000,  
	IRENABLE_MIR_ON		= 0x1000,  
	IRENABLE_SIR_ON		= 0x0800,  
	IRENABLE_ENTXST		= 0x0400,  
	IRENABLE_ENRXST		= 0x0200,  
	IRENABLE_CRC16_ON	= 0x0100   
};

#define	  IRENABLE_MASK	    0xff00  







#define PHYCTL_BAUD_SHIFT	10
#define PHYCTL_BAUD_MASK	0xfc00
#define PHYCTL_PLSWID_SHIFT	5
#define PHYCTL_PLSWID_MASK	0x03e0
#define PHYCTL_PREAMB_SHIFT	0
#define PHYCTL_PREAMB_MASK	0x001f

#define PHYCTL_TO_BAUD(bwp)	(((bwp)&PHYCTL_BAUD_MASK)>>PHYCTL_BAUD_SHIFT)
#define PHYCTL_TO_PLSWID(bwp)	(((bwp)&PHYCTL_PLSWID_MASK)>>PHYCTL_PLSWID_SHIFT)
#define PHYCTL_TO_PREAMB(bwp)	(((bwp)&PHYCTL_PREAMB_MASK)>>PHYCTL_PREAMB_SHIFT)

#define BWP_TO_PHYCTL(b,w,p)	((((b)<<PHYCTL_BAUD_SHIFT)&PHYCTL_BAUD_MASK) \
				 | (((w)<<PHYCTL_PLSWID_SHIFT)&PHYCTL_PLSWID_MASK) \
				 | (((p)<<PHYCTL_PREAMB_SHIFT)&PHYCTL_PREAMB_MASK))

#define BAUD_BITS(br)		((115200/(br))-1)

static inline unsigned
calc_width_bits(unsigned baudrate, unsigned widthselect, unsigned clockselect)
{
	unsigned	tmp;

	if (widthselect)	
		return (clockselect) ? 12 : 24;

	tmp = ((clockselect) ? 12 : 24) / (BAUD_BITS(baudrate)+1);

	

	return (tmp>0) ? (tmp-1) : 0;
}

#define PHYCTL_SIR(br,ws,cs)	BWP_TO_PHYCTL(BAUD_BITS(br),calc_width_bits((br),(ws),(cs)),0)
#define PHYCTL_MIR(cs)		BWP_TO_PHYCTL(0,((cs)?9:10),1)
#define PHYCTL_FIR		BWP_TO_PHYCTL(0,0,15)






#define MAX_PACKET_LENGTH	0x0fff

#define IRDA_MTU		2048

#define IRLAP_SKB_ALLOCSIZE	(1+1+IRDA_MTU)


#define XFER_BUF_SIZE		MAX_PACKET_LENGTH




#define RCVBCNT_MASK	0x0fff



struct ring_descr_hw {
	volatile __le16	rd_count;	
	__le16		reserved;
	union {
		__le32	addr;		
		struct {
			u8		addr_res[3];
			volatile u8	status;		
		} __packed rd_s;
	} __packed rd_u;
} __packed;

#define rd_addr		rd_u.addr
#define rd_status	rd_u.rd_s.status


#define RD_ACTIVE		0x80	


#define	RD_TX_DISCRC		0x40	
#define	RD_TX_BADCRC		0x20	
#define	RD_TX_PULSE		0x10	
#define	RD_TX_FRCEUND		0x08	
#define	RD_TX_CLRENTX		0x04	
#define	RD_TX_UNDRN		0x01	


#define RD_RX_PHYERR		0x40	
#define RD_RX_CRCERR		0x20	
#define RD_RX_LENGTH		0x10	
#define RD_RX_OVER		0x08	
#define RD_RX_SIRBAD		0x04	

#define RD_RX_ERROR		0x7c	

#define HW_RING_AREA_SIZE	(2 * MAX_RING_DESCR * sizeof(struct ring_descr_hw))



struct ring_descr {
	struct ring_descr_hw	*hw;
	struct sk_buff		*skb;
	void			*buf;
};


static inline int rd_is_active(struct ring_descr *rd)
{
	return (rd->hw->rd_status & RD_ACTIVE) != 0;
}

static inline void rd_activate(struct ring_descr *rd)
{
	rd->hw->rd_status |= RD_ACTIVE;
}

static inline void rd_set_status(struct ring_descr *rd, u8 s)
{
	rd->hw->rd_status = s;	 
}

static inline void rd_set_addr_status(struct ring_descr *rd, dma_addr_t a, u8 s)
{

	if ((a & ~DMA_MASK_MSTRPAGE)>>24 != MSTRPAGE_VALUE) {
		IRDA_ERROR("%s: pci busaddr inconsistency!\n", __func__);
		dump_stack();
		return;
	}

	a &= DMA_MASK_MSTRPAGE;  
	rd->hw->rd_addr = cpu_to_le32(a);
	wmb();
	rd_set_status(rd, s);	 
}

static inline void rd_set_count(struct ring_descr *rd, u16 c)
{
	rd->hw->rd_count = cpu_to_le16(c);
}

static inline u8 rd_get_status(struct ring_descr *rd)
{
	return rd->hw->rd_status;
}

static inline dma_addr_t rd_get_addr(struct ring_descr *rd)
{
	dma_addr_t	a;

	a = le32_to_cpu(rd->hw->rd_addr);
	return (a & DMA_MASK_MSTRPAGE) | (MSTRPAGE_VALUE << 24);
}

static inline u16 rd_get_count(struct ring_descr *rd)
{
	return le16_to_cpu(rd->hw->rd_count);
}



struct vlsi_ring {
	struct pci_dev		*pdev;
	int			dir;
	unsigned		len;
	unsigned		size;
	unsigned		mask;
	atomic_t		head, tail;
	struct ring_descr	*rd;
};


static inline struct ring_descr *ring_last(struct vlsi_ring *r)
{
	int t;

	t = atomic_read(&r->tail) & r->mask;
	return (((t+1) & r->mask) == (atomic_read(&r->head) & r->mask)) ? NULL : &r->rd[t];
}

static inline struct ring_descr *ring_put(struct vlsi_ring *r)
{
	atomic_inc(&r->tail);
	return ring_last(r);
}

static inline struct ring_descr *ring_first(struct vlsi_ring *r)
{
	int h;

	h = atomic_read(&r->head) & r->mask;
	return (h == (atomic_read(&r->tail) & r->mask)) ? NULL : &r->rd[h];
}

static inline struct ring_descr *ring_get(struct vlsi_ring *r)
{
	atomic_inc(&r->head);
	return ring_first(r);
}



typedef struct vlsi_irda_dev {
	struct pci_dev		*pdev;

	struct irlap_cb		*irlap;

	struct qos_info		qos;

	unsigned		mode;
	int			baud, new_baud;

	dma_addr_t		busaddr;
	void			*virtaddr;
	struct vlsi_ring	*tx_ring, *rx_ring;

	struct timeval		last_rx;

	spinlock_t		lock;
	struct mutex		mtx;

	u8			resume_ok;	
	struct proc_dir_entry	*proc_entry;

} vlsi_irda_dev_t;



#define VLSI_TX_DROP		0x0001
#define VLSI_TX_FIFO		0x0002

#define VLSI_RX_DROP		0x0100
#define VLSI_RX_OVER		0x0200
#define VLSI_RX_LENGTH  	0x0400
#define VLSI_RX_FRAME		0x0800
#define VLSI_RX_CRC		0x1000


#endif 

