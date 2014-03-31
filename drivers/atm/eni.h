 
/* Written 1995-2000 by Werner Almesberger, EPFL LRC/ICA */
 
 
#ifndef DRIVER_ATM_ENI_H
#define DRIVER_ATM_ENI_H

#include <linux/atm.h>
#include <linux/atmdev.h>
#include <linux/interrupt.h>
#include <linux/sonet.h>
#include <linux/skbuff.h>
#include <linux/time.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>

#include "midway.h"


#define DEV_LABEL	"eni"

#define UBR_BUFFER	(128*1024)	

#define RX_DMA_BUF	  8		
#define TX_DMA_BUF	100		

#define DEFAULT_RX_MULT	300		
#define DEFAULT_TX_MULT	300		

#define ENI_ZEROES_SIZE	  4		


struct eni_free {
	void __iomem *start;		
	int order;
};

struct eni_tx {
	void __iomem *send;		
	int prescaler;			
	int resolution;			
	unsigned long tx_pos;		
	unsigned long words;		
	int index;			
	int reserved;			
	int shaping;			
	struct sk_buff_head backlog;	
};

struct eni_vcc {
	int (*rx)(struct atm_vcc *vcc);	
	void __iomem *recv;		
	unsigned long words;		
	unsigned long descr;		
	unsigned long rx_pos;		
	struct eni_tx *tx;		
	int rxing;			
	int servicing;			
	int txing;			
	ktime_t timestamp;		
	struct atm_vcc *next;		
	struct sk_buff *last;		
};

struct eni_dev {
	
	spinlock_t lock;		
	struct tasklet_struct task;	
	u32 events;			
	void __iomem *ioaddr;
	void __iomem *phy;		
	void __iomem *reg;		
	void __iomem *ram;		
	void __iomem *vci;		
	void __iomem *rx_dma;		
	void __iomem *tx_dma;		
	void __iomem *service;		
	
	struct eni_tx tx[NR_CHAN];	
	struct eni_tx *ubr;		
	struct sk_buff_head tx_queue;	
	wait_queue_head_t tx_wait;	
	int tx_bw;			
	u32 dma[TX_DMA_BUF*2];		
	struct eni_zero {		
		u32 *addr;
		dma_addr_t dma;
	} zero;
	int tx_mult;			
	
	u32 serv_read;			
	struct atm_vcc *fast,*last_fast;
	struct atm_vcc *slow,*last_slow;
	struct atm_vcc **rx_map;	
	struct sk_buff_head rx_queue;	
	wait_queue_head_t rx_wait;	
	int rx_mult;			
	
	unsigned long lost;		
	
	unsigned long base_diff;	
	int free_len;			
	struct eni_free *free_list;	
	int free_list_size;		
	
	struct atm_dev *more;		
	
	int mem;			
	int asic;			
	unsigned int irq;		
	struct pci_dev *pci_dev;	
};


#define ENI_DEV(d) ((struct eni_dev *) (d)->dev_data)
#define ENI_VCC(d) ((struct eni_vcc *) (d)->dev_data)


struct eni_skb_prv {
	struct atm_skb_data _;		
	unsigned long pos;		
	int size;			
	dma_addr_t paddr;		
};

#define ENI_PRV_SIZE(skb) (((struct eni_skb_prv *) (skb)->cb)->size)
#define ENI_PRV_POS(skb) (((struct eni_skb_prv *) (skb)->cb)->pos)
#define ENI_PRV_PADDR(skb) (((struct eni_skb_prv *) (skb)->cb)->paddr)

#endif
