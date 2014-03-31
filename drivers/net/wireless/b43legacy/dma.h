#ifndef B43legacy_DMA_H_
#define B43legacy_DMA_H_

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/linkage.h>
#include <linux/atomic.h>

#include "b43legacy.h"


#define B43legacy_DMAIRQ_FATALMASK	((1 << 10) | (1 << 11) | (1 << 12) \
					 | (1 << 14) | (1 << 15))
#define B43legacy_DMAIRQ_NONFATALMASK	(1 << 13)
#define B43legacy_DMAIRQ_RX_DONE		(1 << 16)



#define B43legacy_DMA32_TXCTL				0x00
#define		B43legacy_DMA32_TXENABLE		0x00000001
#define		B43legacy_DMA32_TXSUSPEND		0x00000002
#define		B43legacy_DMA32_TXLOOPBACK		0x00000004
#define		B43legacy_DMA32_TXFLUSH			0x00000010
#define		B43legacy_DMA32_TXADDREXT_MASK		0x00030000
#define		B43legacy_DMA32_TXADDREXT_SHIFT		16
#define B43legacy_DMA32_TXRING				0x04
#define B43legacy_DMA32_TXINDEX				0x08
#define B43legacy_DMA32_TXSTATUS			0x0C
#define		B43legacy_DMA32_TXDPTR			0x00000FFF
#define		B43legacy_DMA32_TXSTATE			0x0000F000
#define			B43legacy_DMA32_TXSTAT_DISABLED	0x00000000
#define			B43legacy_DMA32_TXSTAT_ACTIVE	0x00001000
#define			B43legacy_DMA32_TXSTAT_IDLEWAIT	0x00002000
#define			B43legacy_DMA32_TXSTAT_STOPPED	0x00003000
#define			B43legacy_DMA32_TXSTAT_SUSP	0x00004000
#define		B43legacy_DMA32_TXERROR			0x000F0000
#define			B43legacy_DMA32_TXERR_NOERR	0x00000000
#define			B43legacy_DMA32_TXERR_PROT	0x00010000
#define			B43legacy_DMA32_TXERR_UNDERRUN	0x00020000
#define			B43legacy_DMA32_TXERR_BUFREAD	0x00030000
#define			B43legacy_DMA32_TXERR_DESCREAD	0x00040000
#define		B43legacy_DMA32_TXACTIVE		0xFFF00000
#define B43legacy_DMA32_RXCTL				0x10
#define		B43legacy_DMA32_RXENABLE		0x00000001
#define		B43legacy_DMA32_RXFROFF_MASK		0x000000FE
#define		B43legacy_DMA32_RXFROFF_SHIFT		1
#define		B43legacy_DMA32_RXDIRECTFIFO		0x00000100
#define		B43legacy_DMA32_RXADDREXT_MASK		0x00030000
#define		B43legacy_DMA32_RXADDREXT_SHIFT		16
#define B43legacy_DMA32_RXRING				0x14
#define B43legacy_DMA32_RXINDEX				0x18
#define B43legacy_DMA32_RXSTATUS			0x1C
#define		B43legacy_DMA32_RXDPTR			0x00000FFF
#define		B43legacy_DMA32_RXSTATE			0x0000F000
#define			B43legacy_DMA32_RXSTAT_DISABLED	0x00000000
#define			B43legacy_DMA32_RXSTAT_ACTIVE	0x00001000
#define			B43legacy_DMA32_RXSTAT_IDLEWAIT	0x00002000
#define			B43legacy_DMA32_RXSTAT_STOPPED	0x00003000
#define		B43legacy_DMA32_RXERROR			0x000F0000
#define			B43legacy_DMA32_RXERR_NOERR	0x00000000
#define			B43legacy_DMA32_RXERR_PROT	0x00010000
#define			B43legacy_DMA32_RXERR_OVERFLOW	0x00020000
#define			B43legacy_DMA32_RXERR_BUFWRITE	0x00030000
#define			B43legacy_DMA32_RXERR_DESCREAD	0x00040000
#define		B43legacy_DMA32_RXACTIVE		0xFFF00000

struct b43legacy_dmadesc32 {
	__le32 control;
	__le32 address;
} __packed;
#define B43legacy_DMA32_DCTL_BYTECNT		0x00001FFF
#define B43legacy_DMA32_DCTL_ADDREXT_MASK	0x00030000
#define B43legacy_DMA32_DCTL_ADDREXT_SHIFT	16
#define B43legacy_DMA32_DCTL_DTABLEEND		0x10000000
#define B43legacy_DMA32_DCTL_IRQ		0x20000000
#define B43legacy_DMA32_DCTL_FRAMEEND		0x40000000
#define B43legacy_DMA32_DCTL_FRAMESTART		0x80000000


#define B43legacy_DMA_RINGMEMSIZE	PAGE_SIZE
#define B43legacy_DMA0_RX_FRAMEOFFSET	30
#define B43legacy_DMA3_RX_FRAMEOFFSET	0


#define B43legacy_TXRING_SLOTS		128
#define B43legacy_RXRING_SLOTS		64
#define B43legacy_DMA0_RX_BUFFERSIZE	(2304 + 100)
#define B43legacy_DMA3_RX_BUFFERSIZE	16



#ifdef CONFIG_B43LEGACY_DMA


struct sk_buff;
struct b43legacy_private;
struct b43legacy_txstatus;


struct b43legacy_dmadesc_meta {
	
	struct sk_buff *skb;
	
	dma_addr_t dmaaddr;
	
	bool is_last_fragment;
};

enum b43legacy_dmatype {
	B43legacy_DMA_30BIT = 30,
	B43legacy_DMA_32BIT = 32,
};

struct b43legacy_dmaring {
	
	void *descbase;
	
	struct b43legacy_dmadesc_meta *meta;
	u8 *txhdr_cache;
	
	dma_addr_t dmabase;
	
	int nr_slots;
	
	int used_slots;
	
	int current_slot;
	
	u32 frameoffset;
	
	u16 rx_buffersize;
	
	u16 mmio_base;
	
	int index;
	
	bool tx;
	
	enum b43legacy_dmatype type;
	
	bool stopped;
	u8 queue_prio;
	struct b43legacy_wldev *dev;
#ifdef CONFIG_B43LEGACY_DEBUG
	
	int max_used_slots;
	
	unsigned long last_injected_overflow;
#endif 
};


static inline
u32 b43legacy_dma_read(struct b43legacy_dmaring *ring,
		       u16 offset)
{
	return b43legacy_read32(ring->dev, ring->mmio_base + offset);
}

static inline
void b43legacy_dma_write(struct b43legacy_dmaring *ring,
			 u16 offset, u32 value)
{
	b43legacy_write32(ring->dev, ring->mmio_base + offset, value);
}


int b43legacy_dma_init(struct b43legacy_wldev *dev);
void b43legacy_dma_free(struct b43legacy_wldev *dev);

void b43legacy_dma_tx_suspend(struct b43legacy_wldev *dev);
void b43legacy_dma_tx_resume(struct b43legacy_wldev *dev);

int b43legacy_dma_tx(struct b43legacy_wldev *dev,
		     struct sk_buff *skb);
void b43legacy_dma_handle_txstatus(struct b43legacy_wldev *dev,
				   const struct b43legacy_txstatus *status);

void b43legacy_dma_rx(struct b43legacy_dmaring *ring);

#else 


static inline
int b43legacy_dma_init(struct b43legacy_wldev *dev)
{
	return 0;
}
static inline
void b43legacy_dma_free(struct b43legacy_wldev *dev)
{
}
static inline
int b43legacy_dma_tx(struct b43legacy_wldev *dev,
		     struct sk_buff *skb)
{
	return 0;
}
static inline
void b43legacy_dma_handle_txstatus(struct b43legacy_wldev *dev,
				   const struct b43legacy_txstatus *status)
{
}
static inline
void b43legacy_dma_rx(struct b43legacy_dmaring *ring)
{
}
static inline
void b43legacy_dma_tx_suspend(struct b43legacy_wldev *dev)
{
}
static inline
void b43legacy_dma_tx_resume(struct b43legacy_wldev *dev)
{
}

#endif 
#endif 
