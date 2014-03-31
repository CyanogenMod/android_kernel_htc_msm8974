/*
 * 7990.h -- LANCE ethernet IC generic routines.
 * This is an attempt to separate out the bits of various ethernet
 * drivers that are common because they all use the AMD 7990 LANCE
 * (Local Area Network Controller for Ethernet) chip.
 *
 * Copyright (C) 05/1998 Peter Maydell <pmaydell@chiark.greenend.org.uk>
 *
 * Most of this stuff was obtained by looking at other LANCE drivers,
 * in particular a2065.[ch]. The AMD C-LANCE datasheet was also helpful.
 */

#ifndef _7990_H
#define _7990_H

#define LANCE_RDP	0	
#define LANCE_RAP	2	


#ifndef LANCE_LOG_TX_BUFFERS
#define LANCE_LOG_TX_BUFFERS 1
#define LANCE_LOG_RX_BUFFERS 3
#endif

#define TX_RING_SIZE (1<<LANCE_LOG_TX_BUFFERS)
#define RX_RING_SIZE (1<<LANCE_LOG_RX_BUFFERS)
#define TX_RING_MOD_MASK (TX_RING_SIZE - 1)
#define RX_RING_MOD_MASK (RX_RING_SIZE - 1)
#define TX_RING_LEN_BITS ((LANCE_LOG_TX_BUFFERS) << 29)
#define RX_RING_LEN_BITS ((LANCE_LOG_RX_BUFFERS) << 29)
#define PKT_BUFF_SIZE (1544)
#define RX_BUFF_SIZE PKT_BUFF_SIZE
#define TX_BUFF_SIZE PKT_BUFF_SIZE

struct lance_rx_desc {
	volatile unsigned short rmd0;        
	volatile unsigned char  rmd1_bits;   
	volatile unsigned char  rmd1_hadr;   
	volatile short    length;    	    
	volatile unsigned short mblength;    
};

struct lance_tx_desc {
	volatile unsigned short tmd0;        
	volatile unsigned char  tmd1_bits;   
	volatile unsigned char  tmd1_hadr;   
	volatile short    length;       	    
	volatile unsigned short misc;
};

struct lance_init_block {
        volatile unsigned short mode;            
        volatile unsigned char phys_addr[6];     
        volatile unsigned filter[2];             

        
        volatile unsigned short rx_ptr;          
        volatile unsigned short rx_len;          
        volatile unsigned short tx_ptr;          
        volatile unsigned short tx_len;          

        volatile struct lance_tx_desc btx_ring[TX_RING_SIZE];
        volatile struct lance_rx_desc brx_ring[RX_RING_SIZE];

        volatile char   tx_buf [TX_RING_SIZE][TX_BUFF_SIZE];
        volatile char   rx_buf [RX_RING_SIZE][RX_BUFF_SIZE];
};

struct lance_private
{
        char *name;
	unsigned long base;
        volatile struct lance_init_block *init_block; 
        volatile struct lance_init_block *lance_init_block; 

        int rx_new, tx_new;
        int rx_old, tx_old;

        int lance_log_rx_bufs, lance_log_tx_bufs;
        int rx_ring_mod_mask, tx_ring_mod_mask;

        int tpe;                                  
        int auto_select;                          
        unsigned short busmaster_regval;

        unsigned int irq;                         

        void (*writerap)(void *, unsigned short);
        void (*writerdp)(void *, unsigned short);
        unsigned short (*readrdp)(void *);
	spinlock_t devlock;
	char tx_full;
};

#define LE_CSR0         0x0000          
#define LE_CSR1         0x0001          
#define LE_CSR2         0x0002          
#define LE_CSR3         0x0003          

#define LE_C0_ERR	0x8000		
#define LE_C0_BABL	0x4000		
#define LE_C0_CERR	0x2000		
#define LE_C0_MISS	0x1000		
#define LE_C0_MERR	0x0800		
#define LE_C0_RINT	0x0400		
#define LE_C0_TINT	0x0200		
#define LE_C0_IDON	0x0100		
#define LE_C0_INTR	0x0080		
#define LE_C0_INEA	0x0040		
#define LE_C0_RXON	0x0020		
#define LE_C0_TXON	0x0010		
#define LE_C0_TDMD	0x0008		
#define LE_C0_STOP	0x0004		
#define LE_C0_STRT	0x0002		
#define LE_C0_INIT	0x0001		


#define LE_C3_BSWP	0x0004		
#define LE_C3_ACON	0x0002		
#define LE_C3_BCON	0x0001		


#define LE_MO_PROM	0x8000		
#define LE_MO_DRCVBC  0x4000          
#define LE_MO_DRCVPA  0x2000          
#define LE_MO_DLNKTST 0x1000          
#define LE_MO_DAPC    0x0800          
#define LE_MO_MENDECL 0x0400          
#define LE_MO_LRTTSEL 0x0200          
#define LE_MO_PSEL1   0x0100          
#define LE_MO_PSEL0   0x0080          
#define LE_MO_EMBA      0x0080          
#define LE_MO_INTL	0x0040		
#define LE_MO_DRTY	0x0020		
#define LE_MO_FCOLL	0x0010		
#define LE_MO_DXMTFCS	0x0008		
#define LE_MO_LOOP	0x0004		
#define LE_MO_DTX	0x0002		
#define LE_MO_DRX	0x0001		


#define LE_R1_OWN	0x80		
#define LE_R1_ERR	0x40		
#define LE_R1_FRA	0x20		
#define LE_R1_OFL	0x10		
#define LE_R1_CRC	0x08		
#define LE_R1_BUF	0x04		
#define LE_R1_SOP	0x02		
#define LE_R1_EOP	0x01		
#define LE_R1_POK       0x03		


#define LE_T1_OWN	0x80		
#define LE_T1_ERR	0x40		
#define LE_T1_RES	0x20		
#define LE_T1_EMORE	0x10		
#define LE_T1_EONE	0x08		
#define LE_T1_EDEF	0x04		
#define LE_T1_SOP	0x02		
#define LE_T1_EOP	0x01		
#define LE_T1_POK	0x03		

#define LE_T3_BUF 	0x8000		
#define LE_T3_UFL 	0x4000		
#define LE_T3_LCOL 	0x1000		
#define LE_T3_CLOS 	0x0800		
#define LE_T3_RTY 	0x0400		
#define LE_T3_TDR	0x03ff		


#define TX_BUFFS_AVAIL ((lp->tx_old<=lp->tx_new)?\
                        lp->tx_old+lp->tx_ring_mod_mask-lp->tx_new:\
                        lp->tx_old - lp->tx_new-1)

#define LANCE_ADDR(x) ((int)(x) & ~0xff000000)

extern int lance_open(struct net_device *dev);
extern int lance_close (struct net_device *dev);
extern int lance_start_xmit (struct sk_buff *skb, struct net_device *dev);
extern void lance_set_multicast (struct net_device *dev);
extern void lance_tx_timeout(struct net_device *dev);
#ifdef CONFIG_NET_POLL_CONTROLLER
extern void lance_poll(struct net_device *dev);
#endif

#endif 
