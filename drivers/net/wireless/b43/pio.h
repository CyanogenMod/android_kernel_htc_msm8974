#ifndef B43_PIO_H_
#define B43_PIO_H_

#include "b43.h"

#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/skbuff.h>


#define B43_PIO_TXCTL			0x00
#define  B43_PIO_TXCTL_WRITELO		0x0001
#define  B43_PIO_TXCTL_WRITEHI		0x0002
#define  B43_PIO_TXCTL_EOF		0x0004
#define  B43_PIO_TXCTL_FREADY		0x0008
#define  B43_PIO_TXCTL_FLUSHREQ		0x0020
#define  B43_PIO_TXCTL_FLUSHPEND	0x0040
#define  B43_PIO_TXCTL_SUSPREQ		0x0080
#define  B43_PIO_TXCTL_QSUSP		0x0100
#define  B43_PIO_TXCTL_COMMCNT		0xFC00
#define  B43_PIO_TXCTL_COMMCNT_SHIFT	10
#define B43_PIO_TXDATA			0x02
#define B43_PIO_TXQBUFSIZE		0x04
#define B43_PIO_RXCTL			0x00
#define  B43_PIO_RXCTL_FRAMERDY		0x0001
#define  B43_PIO_RXCTL_DATARDY		0x0002
#define B43_PIO_RXDATA			0x02

#define B43_PIO8_TXCTL			0x00
#define  B43_PIO8_TXCTL_0_7		0x00000001
#define  B43_PIO8_TXCTL_8_15		0x00000002
#define  B43_PIO8_TXCTL_16_23		0x00000004
#define  B43_PIO8_TXCTL_24_31		0x00000008
#define  B43_PIO8_TXCTL_EOF		0x00000010
#define  B43_PIO8_TXCTL_FREADY		0x00000080
#define  B43_PIO8_TXCTL_SUSPREQ		0x00000100
#define  B43_PIO8_TXCTL_QSUSP		0x00000200
#define  B43_PIO8_TXCTL_FLUSHREQ	0x00000400
#define  B43_PIO8_TXCTL_FLUSHPEND	0x00000800
#define B43_PIO8_TXDATA			0x04
#define B43_PIO8_RXCTL			0x00
#define  B43_PIO8_RXCTL_FRAMERDY	0x00000001
#define  B43_PIO8_RXCTL_DATARDY		0x00000002
#define B43_PIO8_RXDATA			0x04


#define B43_PIO_MAX_NR_TXPACKETS	32


struct b43_pio_txpacket {
	
	struct b43_pio_txqueue *queue;
	
	struct sk_buff *skb;
	
	u8 index;

	struct list_head list;
};

struct b43_pio_txqueue {
	struct b43_wldev *dev;
	u16 mmio_base;

	
	u16 buffer_size;
	
	u16 buffer_used;
	u16 free_packet_slots;

	
	bool stopped;
	
	u8 index;
	
	u8 queue_prio;

	
	struct b43_pio_txpacket packets[B43_PIO_MAX_NR_TXPACKETS];
	struct list_head packets_list;

	u8 rev;
};

struct b43_pio_rxqueue {
	struct b43_wldev *dev;
	u16 mmio_base;

	u8 rev;
};


static inline u16 b43_piotx_read16(struct b43_pio_txqueue *q, u16 offset)
{
	return b43_read16(q->dev, q->mmio_base + offset);
}

static inline u32 b43_piotx_read32(struct b43_pio_txqueue *q, u16 offset)
{
	return b43_read32(q->dev, q->mmio_base + offset);
}

static inline void b43_piotx_write16(struct b43_pio_txqueue *q,
				     u16 offset, u16 value)
{
	b43_write16(q->dev, q->mmio_base + offset, value);
}

static inline void b43_piotx_write32(struct b43_pio_txqueue *q,
				     u16 offset, u32 value)
{
	b43_write32(q->dev, q->mmio_base + offset, value);
}


static inline u16 b43_piorx_read16(struct b43_pio_rxqueue *q, u16 offset)
{
	return b43_read16(q->dev, q->mmio_base + offset);
}

static inline u32 b43_piorx_read32(struct b43_pio_rxqueue *q, u16 offset)
{
	return b43_read32(q->dev, q->mmio_base + offset);
}

static inline void b43_piorx_write16(struct b43_pio_rxqueue *q,
				     u16 offset, u16 value)
{
	b43_write16(q->dev, q->mmio_base + offset, value);
}

static inline void b43_piorx_write32(struct b43_pio_rxqueue *q,
				     u16 offset, u32 value)
{
	b43_write32(q->dev, q->mmio_base + offset, value);
}


int b43_pio_init(struct b43_wldev *dev);
void b43_pio_free(struct b43_wldev *dev);

int b43_pio_tx(struct b43_wldev *dev, struct sk_buff *skb);
void b43_pio_handle_txstatus(struct b43_wldev *dev,
			     const struct b43_txstatus *status);
void b43_pio_rx(struct b43_pio_rxqueue *q);

void b43_pio_tx_suspend(struct b43_wldev *dev);
void b43_pio_tx_resume(struct b43_wldev *dev);

#endif 
