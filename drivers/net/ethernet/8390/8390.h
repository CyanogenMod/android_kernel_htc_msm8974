/* This file is part of Donald Becker's 8390 drivers, and is distributed
   under the same license. Auto-loading of 8390.o only in v2.2 - Paul G.
   Some of these names and comments originated from the Crynwr
   packet drivers, which are distributed under the GPL. */

#ifndef _8390_h
#define _8390_h

#include <linux/if_ether.h>
#include <linux/ioport.h>
#include <linux/irqreturn.h>
#include <linux/skbuff.h>

#define TX_PAGES 12	

struct e8390_pkt_hdr {
  unsigned char status; 
  unsigned char next;   
  unsigned short count; 
};

#ifdef notdef
extern int ei_debug;
#else
#define ei_debug 1
#endif

#ifdef CONFIG_NET_POLL_CONTROLLER
extern void ei_poll(struct net_device *dev);
extern void eip_poll(struct net_device *dev);
#endif


extern void NS8390_init(struct net_device *dev, int startp);
extern int ei_open(struct net_device *dev);
extern int ei_close(struct net_device *dev);
extern irqreturn_t ei_interrupt(int irq, void *dev_id);
extern void ei_tx_timeout(struct net_device *dev);
extern netdev_tx_t ei_start_xmit(struct sk_buff *skb, struct net_device *dev);
extern void ei_set_multicast_list(struct net_device *dev);
extern struct net_device_stats *ei_get_stats(struct net_device *dev);

extern const struct net_device_ops ei_netdev_ops;

extern struct net_device *__alloc_ei_netdev(int size);
static inline struct net_device *alloc_ei_netdev(void)
{
	return __alloc_ei_netdev(0);
}

extern void NS8390p_init(struct net_device *dev, int startp);
extern int eip_open(struct net_device *dev);
extern int eip_close(struct net_device *dev);
extern irqreturn_t eip_interrupt(int irq, void *dev_id);
extern void eip_tx_timeout(struct net_device *dev);
extern netdev_tx_t eip_start_xmit(struct sk_buff *skb, struct net_device *dev);
extern void eip_set_multicast_list(struct net_device *dev);
extern struct net_device_stats *eip_get_stats(struct net_device *dev);

extern const struct net_device_ops eip_netdev_ops;

extern struct net_device *__alloc_eip_netdev(int size);
static inline struct net_device *alloc_eip_netdev(void)
{
	return __alloc_eip_netdev(0);
}

struct ei_device {
	const char *name;
	void (*reset_8390)(struct net_device *);
	void (*get_8390_hdr)(struct net_device *, struct e8390_pkt_hdr *, int);
	void (*block_output)(struct net_device *, int, const unsigned char *, int);
	void (*block_input)(struct net_device *, int, struct sk_buff *, int);
	unsigned long rmem_start;
	unsigned long rmem_end;
	void __iomem *mem;
	unsigned char mcfilter[8];
	unsigned open:1;
	unsigned word16:1;  		
	unsigned bigendian:1;		
					
	unsigned txing:1;		
	unsigned irqlock:1;		
	unsigned dmaing:1;		
	unsigned char tx_start_page, rx_start_page, stop_page;
	unsigned char current_page;	
	unsigned char interface_num;	
	unsigned char txqueue;		
	short tx1, tx2;			
	short lasttx;			
	unsigned char reg0;		
	unsigned char reg5;		
	unsigned char saved_irq;	
	u32 *reg_offset;		
	spinlock_t page_lock;		
	unsigned long priv;		
#ifdef AX88796_PLATFORM
	unsigned char rxcr_base;	
#endif
};

#define MAX_SERVICE 12

#define TX_TIMEOUT (20*HZ/100)

#define ei_status (*(struct ei_device *)netdev_priv(dev))

#define E8390_TX_IRQ_MASK	0xa	
#define E8390_RX_IRQ_MASK	0x5

#ifdef AX88796_PLATFORM
#define E8390_RXCONFIG		(ei_status.rxcr_base | 0x04)
#define E8390_RXOFF		(ei_status.rxcr_base | 0x20)
#else
#define E8390_RXCONFIG		0x4	
#define E8390_RXOFF		0x20	
#endif

#define E8390_TXCONFIG		0x00	
#define E8390_TXOFF		0x02	


#define E8390_STOP	0x01	
#define E8390_START	0x02	
#define E8390_TRANS	0x04	
#define E8390_RREAD	0x08	
#define E8390_RWRITE	0x10	
#define E8390_NODMA	0x20	
#define E8390_PAGE0	0x00	
#define E8390_PAGE1	0x40	
#define E8390_PAGE2	0x80	


#ifndef ei_inb
#define ei_inb(_p)	inb(_p)
#define ei_outb(_v,_p)	outb(_v,_p)
#define ei_inb_p(_p)	inb(_p)
#define ei_outb_p(_v,_p) outb(_v,_p)
#endif

#ifndef EI_SHIFT
#define EI_SHIFT(x)	(x)
#endif

#define E8390_CMD	EI_SHIFT(0x00)  
#define EN0_CLDALO	EI_SHIFT(0x01)	
#define EN0_STARTPG	EI_SHIFT(0x01)	
#define EN0_CLDAHI	EI_SHIFT(0x02)	
#define EN0_STOPPG	EI_SHIFT(0x02)	
#define EN0_BOUNDARY	EI_SHIFT(0x03)	
#define EN0_TSR		EI_SHIFT(0x04)	
#define EN0_TPSR	EI_SHIFT(0x04)	
#define EN0_NCR		EI_SHIFT(0x05)	
#define EN0_TCNTLO	EI_SHIFT(0x05)	
#define EN0_FIFO	EI_SHIFT(0x06)	
#define EN0_TCNTHI	EI_SHIFT(0x06)	
#define EN0_ISR		EI_SHIFT(0x07)	
#define EN0_CRDALO	EI_SHIFT(0x08)	
#define EN0_RSARLO	EI_SHIFT(0x08)	
#define EN0_CRDAHI	EI_SHIFT(0x09)	
#define EN0_RSARHI	EI_SHIFT(0x09)	
#define EN0_RCNTLO	EI_SHIFT(0x0a)	
#define EN0_RCNTHI	EI_SHIFT(0x0b)	
#define EN0_RSR		EI_SHIFT(0x0c)	
#define EN0_RXCR	EI_SHIFT(0x0c)	
#define EN0_TXCR	EI_SHIFT(0x0d)	
#define EN0_COUNTER0	EI_SHIFT(0x0d)	
#define EN0_DCFG	EI_SHIFT(0x0e)	
#define EN0_COUNTER1	EI_SHIFT(0x0e)	
#define EN0_IMR		EI_SHIFT(0x0f)	
#define EN0_COUNTER2	EI_SHIFT(0x0f)	

#define ENISR_RX	0x01	
#define ENISR_TX	0x02	
#define ENISR_RX_ERR	0x04	
#define ENISR_TX_ERR	0x08	
#define ENISR_OVER	0x10	
#define ENISR_COUNTERS	0x20	
#define ENISR_RDC	0x40	
#define ENISR_RESET	0x80	
#define ENISR_ALL	0x3f	

#define ENDCFG_WTS	0x01	
#define ENDCFG_BOS	0x02	

#define EN1_PHYS   EI_SHIFT(0x01)	
#define EN1_PHYS_SHIFT(i)  EI_SHIFT(i+1) 
#define EN1_CURPAG EI_SHIFT(0x07)	
#define EN1_MULT   EI_SHIFT(0x08)	
#define EN1_MULT_SHIFT(i)  EI_SHIFT(8+i) 

#define ENRSR_RXOK	0x01	
#define ENRSR_CRC	0x02	
#define ENRSR_FAE	0x04	
#define ENRSR_FO	0x08	
#define ENRSR_MPA	0x10	
#define ENRSR_PHY	0x20	
#define ENRSR_DIS	0x40	
#define ENRSR_DEF	0x80	

#define ENTSR_PTX 0x01	
#define ENTSR_ND  0x02	
#define ENTSR_COL 0x04	
#define ENTSR_ABT 0x08  
#define ENTSR_CRS 0x10	
#define ENTSR_FU  0x20  
#define ENTSR_CDH 0x40	
#define ENTSR_OWC 0x80  

#endif 
