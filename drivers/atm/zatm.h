
/* Written 1995-1998 by Werner Almesberger, EPFL LRC/ICA */


#ifndef DRIVER_ATM_ZATM_H
#define DRIVER_ATM_ZATM_H

#include <linux/skbuff.h>
#include <linux/atm.h>
#include <linux/atmdev.h>
#include <linux/sonet.h>
#include <linux/pci.h>


#define DEV_LABEL	"zatm"

#define MAX_AAL5_PDU	10240	
#define MAX_RX_SIZE_LD	14	

#define LOW_MARK	12	
#define HIGH_MARK	30	
#define OFF_CNG_THRES	5	

#define RX_SIZE		2	
#define NR_POOLS	32	
#define POOL_SIZE	8	
#define NR_SHAPERS	16	
#define SHAPER_SIZE	4	
#define VC_SIZE		32	

#define RING_ENTRIES	32	
#define RING_WORDS	4	
#define RING_SIZE	(sizeof(unsigned long)*(RING_ENTRIES+1)*RING_WORDS)

#define NR_MBX		4	
#define MBX_RX_0	0	
#define MBX_RX_1	1
#define MBX_TX_0	2
#define MBX_TX_1	3

struct zatm_vcc {
	
	int rx_chan;			
	int pool;			
	
	int tx_chan;			
	int shaper;			
	struct sk_buff_head tx_queue;	
	wait_queue_head_t tx_wait;	
	u32 *ring;			
	int ring_curr;			
	int txing;			
	struct sk_buff_head backlog;	
};

struct zatm_dev {
	
	int tx_bw;			
	u32 free_shapers;		
	int ubr;			
	int ubr_ref_cnt;		
	
	int pool_ref[NR_POOLS];		
	volatile struct sk_buff *last_free[NR_POOLS];
					
	struct sk_buff_head pool[NR_POOLS];
	struct zatm_pool_info pool_info[NR_POOLS]; 
	
	struct atm_vcc **tx_map;	
	struct atm_vcc **rx_map;	
	int chans;			
	
	unsigned long mbx_start[NR_MBX];
	dma_addr_t mbx_dma[NR_MBX];
	u16 mbx_end[NR_MBX];		
	
	u32 pool_base;			
	
	struct atm_dev *more;		
	
	int mem;			
	int khz;			
	int copper;			
	unsigned char irq;		
	unsigned int base;		
	struct pci_dev *pci_dev;	
	spinlock_t lock;
};


#define ZATM_DEV(d) ((struct zatm_dev *) (d)->dev_data)
#define ZATM_VCC(d) ((struct zatm_vcc *) (d)->dev_data)


struct zatm_skb_prv {
	struct atm_skb_data _;		
	u32 *dsc;			
};

#define ZATM_PRV_DSC(skb) (((struct zatm_skb_prv *) (skb)->cb)->dsc)

#endif
