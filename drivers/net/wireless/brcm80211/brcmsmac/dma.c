/*
 * Copyright (c) 2010 Broadcom Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/pci.h>

#include <brcmu_utils.h>
#include <aiutils.h>
#include "types.h"
#include "dma.h"
#include "soc.h"

#define DMA64REGOFFS(field)		offsetof(struct dma64regs, field)
#define DMA64TXREGOFFS(di, field)	(di->d64txregbase + DMA64REGOFFS(field))
#define DMA64RXREGOFFS(di, field)	(di->d64rxregbase + DMA64REGOFFS(field))

#define D64RINGALIGN_BITS	13
#define	D64MAXRINGSZ		(1 << D64RINGALIGN_BITS)
#define	D64RINGALIGN		(1 << D64RINGALIGN_BITS)

#define	D64MAXDD	(D64MAXRINGSZ / sizeof(struct dma64desc))

#define	D64_XC_XE		0x00000001	
#define	D64_XC_SE		0x00000002	
#define	D64_XC_LE		0x00000004	
#define	D64_XC_FL		0x00000010	
#define	D64_XC_PD		0x00000800	
#define	D64_XC_AE		0x00030000	
#define	D64_XC_AE_SHIFT		16

#define	D64_XP_LD_MASK		0x00000fff	

#define	D64_XS0_CD_MASK		0x00001fff	
#define	D64_XS0_XS_MASK		0xf0000000	
#define	D64_XS0_XS_SHIFT		28
#define	D64_XS0_XS_DISABLED	0x00000000	
#define	D64_XS0_XS_ACTIVE	0x10000000	
#define	D64_XS0_XS_IDLE		0x20000000	
#define	D64_XS0_XS_STOPPED	0x30000000	
#define	D64_XS0_XS_SUSP		0x40000000	

#define	D64_XS1_AD_MASK		0x00001fff	
#define	D64_XS1_XE_MASK		0xf0000000	
#define	D64_XS1_XE_SHIFT		28
#define	D64_XS1_XE_NOERR	0x00000000	
#define	D64_XS1_XE_DPE		0x10000000	
#define	D64_XS1_XE_DFU		0x20000000	
#define	D64_XS1_XE_DTE		0x30000000	
#define	D64_XS1_XE_DESRE	0x40000000	
#define	D64_XS1_XE_COREE	0x50000000	

#define	D64_RC_RE		0x00000001
#define	D64_RC_RO_MASK		0x000000fe
#define	D64_RC_RO_SHIFT		1
#define	D64_RC_FM		0x00000100
#define	D64_RC_SH		0x00000200
#define	D64_RC_OC		0x00000400
#define	D64_RC_PD		0x00000800
#define	D64_RC_AE		0x00030000
#define	D64_RC_AE_SHIFT		16

#define DMA_CTRL_PEN		(1 << 0)
#define DMA_CTRL_ROC		(1 << 1)
#define DMA_CTRL_RXMULTI	(1 << 2)
#define DMA_CTRL_UNFRAMED	(1 << 3)

#define	D64_RP_LD_MASK		0x00000fff	

#define	D64_RS0_CD_MASK		0x00001fff	
#define	D64_RS0_RS_MASK		0xf0000000	
#define	D64_RS0_RS_SHIFT		28
#define	D64_RS0_RS_DISABLED	0x00000000	
#define	D64_RS0_RS_ACTIVE	0x10000000	
#define	D64_RS0_RS_IDLE		0x20000000	
#define	D64_RS0_RS_STOPPED	0x30000000	
#define	D64_RS0_RS_SUSP		0x40000000	

#define	D64_RS1_AD_MASK		0x0001ffff	
#define	D64_RS1_RE_MASK		0xf0000000	
#define	D64_RS1_RE_SHIFT		28
#define	D64_RS1_RE_NOERR	0x00000000	
#define	D64_RS1_RE_DPO		0x10000000	
#define	D64_RS1_RE_DFU		0x20000000	
#define	D64_RS1_RE_DTE		0x30000000	
#define	D64_RS1_RE_DESRE	0x40000000	
#define	D64_RS1_RE_COREE	0x50000000	

#define	D64_FA_OFF_MASK		0xffff	
#define	D64_FA_SEL_MASK		0xf0000	
#define	D64_FA_SEL_SHIFT	16
#define	D64_FA_SEL_XDD		0x00000	
#define	D64_FA_SEL_XDP		0x10000	
#define	D64_FA_SEL_RDD		0x40000	
#define	D64_FA_SEL_RDP		0x50000	
#define	D64_FA_SEL_XFD		0x80000	
#define	D64_FA_SEL_XFP		0x90000	
#define	D64_FA_SEL_RFD		0xc0000	
#define	D64_FA_SEL_RFP		0xd0000	
#define	D64_FA_SEL_RSD		0xe0000	
#define	D64_FA_SEL_RSP		0xf0000	

#define D64_CTRL_COREFLAGS	0x0ff00000	
#define	D64_CTRL1_EOT		((u32)1 << 28)	
#define	D64_CTRL1_IOC		((u32)1 << 29)	
#define	D64_CTRL1_EOF		((u32)1 << 30)	
#define	D64_CTRL1_SOF		((u32)1 << 31)	

#define	D64_CTRL2_BC_MASK	0x00007fff
#define	D64_CTRL2_AE		0x00030000
#define	D64_CTRL2_AE_SHIFT	16
#define D64_CTRL2_PARITY	0x00040000

#define	D64_CTRL_CORE_MASK	0x0ff00000

#define D64_RX_FRM_STS_LEN	0x0000ffff	
#define D64_RX_FRM_STS_OVFL	0x00800000	
#define D64_RX_FRM_STS_DSCRCNT	0x0f000000  
#define D64_RX_FRM_STS_DATATYPE	0xf0000000	


#define BCMEXTRAHDROOM 172

#ifdef DEBUG
#define	DMA_ERROR(fmt, ...)					\
do {								\
	if (*di->msg_level & 1)					\
		pr_debug("%s: " fmt, __func__, ##__VA_ARGS__);	\
} while (0)
#define	DMA_TRACE(fmt, ...)					\
do {								\
	if (*di->msg_level & 2)					\
		pr_debug("%s: " fmt, __func__, ##__VA_ARGS__);	\
} while (0)
#else
#define	DMA_ERROR(fmt, ...)			\
	no_printk(fmt, ##__VA_ARGS__)
#define	DMA_TRACE(fmt, ...)			\
	no_printk(fmt, ##__VA_ARGS__)
#endif				

#define	DMA_NONE(fmt, ...)			\
	no_printk(fmt, ##__VA_ARGS__)

#define	MAXNAMEL	8	

#define	B2I(bytes, type)	((bytes) / sizeof(type))
#define	I2B(index, type)	((index) * sizeof(type))

#define	PCI32ADDR_HIGH		0xc0000000	
#define	PCI32ADDR_HIGH_SHIFT	30	

#define	PCI64ADDR_HIGH		0x80000000	
#define	PCI64ADDR_HIGH_SHIFT	31	

/*
 * DMA Descriptor
 * Descriptors are only read by the hardware, never written back.
 */
struct dma64desc {
	__le32 ctrl1;	
	__le32 ctrl2;	
	__le32 addrlow;	
	__le32 addrhigh; 
};

struct dma_info {
	struct dma_pub dma; 
	uint *msg_level;	
	char name[MAXNAMEL];	

	struct bcma_device *core;
	struct device *dmadev;

	bool dma64;	
	bool addrext;	

	
	uint d64txregbase;
	
	uint d64rxregbase;
	
	struct dma64desc *txd64;
	
	struct dma64desc *rxd64;

	u16 dmadesc_align;	

	u16 ntxd;		
	u16 txin;		
	u16 txout;		
	
	struct sk_buff **txp;
	
	dma_addr_t txdpa;
	
	dma_addr_t txdpaorig;
	u16 txdalign;	
	u32 txdalloc;	
	u32 xmtptrbase;	

	u16 nrxd;	
	u16 rxin;	
	u16 rxout;	
	
	struct sk_buff **rxp;
	
	dma_addr_t rxdpa;
	
	dma_addr_t rxdpaorig;
	u16 rxdalign;	
	u32 rxdalloc;	
	u32 rcvptrbase;	

	
	unsigned int rxbufsize;	
	uint rxextrahdrroom;	
	uint nrxpost;		
	unsigned int rxoffset;	
	
	uint ddoffsetlow;
	
	uint ddoffsethigh;
	
	uint dataoffsetlow;
	
	uint dataoffsethigh;
	
	bool aligndesc_4k;
};

static uint dma_msg_level;

static u32 parity32(__le32 data)
{
	
	u32 par_data = *(u32 *)&data;

	par_data ^= par_data >> 16;
	par_data ^= par_data >> 8;
	par_data ^= par_data >> 4;
	par_data ^= par_data >> 2;
	par_data ^= par_data >> 1;

	return par_data & 1;
}

static bool dma64_dd_parity(struct dma64desc *dd)
{
	return parity32(dd->addrlow ^ dd->addrhigh ^ dd->ctrl1 ^ dd->ctrl2);
}


static uint xxd(uint x, uint n)
{
	return x & (n - 1); 
}

static uint txd(struct dma_info *di, uint x)
{
	return xxd(x, di->ntxd);
}

static uint rxd(struct dma_info *di, uint x)
{
	return xxd(x, di->nrxd);
}

static uint nexttxd(struct dma_info *di, uint i)
{
	return txd(di, i + 1);
}

static uint prevtxd(struct dma_info *di, uint i)
{
	return txd(di, i - 1);
}

static uint nextrxd(struct dma_info *di, uint i)
{
	return txd(di, i + 1);
}

static uint ntxdactive(struct dma_info *di, uint h, uint t)
{
	return txd(di, t-h);
}

static uint nrxdactive(struct dma_info *di, uint h, uint t)
{
	return rxd(di, t-h);
}

static uint _dma_ctrlflags(struct dma_info *di, uint mask, uint flags)
{
	uint dmactrlflags;

	if (di == NULL) {
		DMA_ERROR("NULL dma handle\n");
		return 0;
	}

	dmactrlflags = di->dma.dmactrlflags;
	dmactrlflags &= ~mask;
	dmactrlflags |= flags;

	
	if (dmactrlflags & DMA_CTRL_PEN) {
		u32 control;

		control = bcma_read32(di->core, DMA64TXREGOFFS(di, control));
		bcma_write32(di->core, DMA64TXREGOFFS(di, control),
		      control | D64_XC_PD);
		if (bcma_read32(di->core, DMA64TXREGOFFS(di, control)) &
		    D64_XC_PD)
			bcma_write32(di->core, DMA64TXREGOFFS(di, control),
				     control);
		else
			
			dmactrlflags &= ~DMA_CTRL_PEN;
	}

	di->dma.dmactrlflags = dmactrlflags;

	return dmactrlflags;
}

static bool _dma64_addrext(struct dma_info *di, uint ctrl_offset)
{
	u32 w;
	bcma_set32(di->core, ctrl_offset, D64_XC_AE);
	w = bcma_read32(di->core, ctrl_offset);
	bcma_mask32(di->core, ctrl_offset, ~D64_XC_AE);
	return (w & D64_XC_AE) == D64_XC_AE;
}

static bool _dma_isaddrext(struct dma_info *di)
{
	

	
	if (di->d64txregbase != 0) {
		if (!_dma64_addrext(di, DMA64TXREGOFFS(di, control)))
			DMA_ERROR("%s: DMA64 tx doesn't have AE set\n",
				  di->name);
		return true;
	} else if (di->d64rxregbase != 0) {
		if (!_dma64_addrext(di, DMA64RXREGOFFS(di, control)))
			DMA_ERROR("%s: DMA64 rx doesn't have AE set\n",
				  di->name);
		return true;
	}

	return false;
}

static bool _dma_descriptor_align(struct dma_info *di)
{
	u32 addrl;

	
	if (di->d64txregbase != 0) {
		bcma_write32(di->core, DMA64TXREGOFFS(di, addrlow), 0xff0);
		addrl = bcma_read32(di->core, DMA64TXREGOFFS(di, addrlow));
		if (addrl != 0)
			return false;
	} else if (di->d64rxregbase != 0) {
		bcma_write32(di->core, DMA64RXREGOFFS(di, addrlow), 0xff0);
		addrl = bcma_read32(di->core, DMA64RXREGOFFS(di, addrlow));
		if (addrl != 0)
			return false;
	}
	return true;
}

static void *dma_alloc_consistent(struct dma_info *di, uint size,
				  u16 align_bits, uint *alloced,
				  dma_addr_t *pap)
{
	if (align_bits) {
		u16 align = (1 << align_bits);
		if (!IS_ALIGNED(PAGE_SIZE, align))
			size += align;
		*alloced = size;
	}
	return dma_alloc_coherent(di->dmadev, size, pap, GFP_ATOMIC);
}

static
u8 dma_align_sizetobits(uint size)
{
	u8 bitpos = 0;
	while (size >>= 1)
		bitpos++;
	return bitpos;
}

static void *dma_ringalloc(struct dma_info *di, u32 boundary, uint size,
			   u16 *alignbits, uint *alloced,
			   dma_addr_t *descpa)
{
	void *va;
	u32 desc_strtaddr;
	u32 alignbytes = 1 << *alignbits;

	va = dma_alloc_consistent(di, size, *alignbits, alloced, descpa);

	if (NULL == va)
		return NULL;

	desc_strtaddr = (u32) roundup((unsigned long)va, alignbytes);
	if (((desc_strtaddr + size - 1) & boundary) != (desc_strtaddr
							& boundary)) {
		*alignbits = dma_align_sizetobits(size);
		dma_free_coherent(di->dmadev, size, va, *descpa);
		va = dma_alloc_consistent(di, size, *alignbits,
			alloced, descpa);
	}
	return va;
}

static bool dma64_alloc(struct dma_info *di, uint direction)
{
	u16 size;
	uint ddlen;
	void *va;
	uint alloced = 0;
	u16 align;
	u16 align_bits;

	ddlen = sizeof(struct dma64desc);

	size = (direction == DMA_TX) ? (di->ntxd * ddlen) : (di->nrxd * ddlen);
	align_bits = di->dmadesc_align;
	align = (1 << align_bits);

	if (direction == DMA_TX) {
		va = dma_ringalloc(di, D64RINGALIGN, size, &align_bits,
			&alloced, &di->txdpaorig);
		if (va == NULL) {
			DMA_ERROR("%s: DMA_ALLOC_CONSISTENT(ntxd) failed\n",
				  di->name);
			return false;
		}
		align = (1 << align_bits);
		di->txd64 = (struct dma64desc *)
					roundup((unsigned long)va, align);
		di->txdalign = (uint) ((s8 *)di->txd64 - (s8 *) va);
		di->txdpa = di->txdpaorig + di->txdalign;
		di->txdalloc = alloced;
	} else {
		va = dma_ringalloc(di, D64RINGALIGN, size, &align_bits,
			&alloced, &di->rxdpaorig);
		if (va == NULL) {
			DMA_ERROR("%s: DMA_ALLOC_CONSISTENT(nrxd) failed\n",
				  di->name);
			return false;
		}
		align = (1 << align_bits);
		di->rxd64 = (struct dma64desc *)
					roundup((unsigned long)va, align);
		di->rxdalign = (uint) ((s8 *)di->rxd64 - (s8 *) va);
		di->rxdpa = di->rxdpaorig + di->rxdalign;
		di->rxdalloc = alloced;
	}

	return true;
}

static bool _dma_alloc(struct dma_info *di, uint direction)
{
	return dma64_alloc(di, direction);
}

struct dma_pub *dma_attach(char *name, struct si_pub *sih,
			   struct bcma_device *core,
			   uint txregbase, uint rxregbase, uint ntxd, uint nrxd,
			   uint rxbufsize, int rxextheadroom,
			   uint nrxpost, uint rxoffset, uint *msg_level)
{
	struct dma_info *di;
	u8 rev = core->id.rev;
	uint size;

	
	di = kzalloc(sizeof(struct dma_info), GFP_ATOMIC);
	if (di == NULL)
		return NULL;

	di->msg_level = msg_level ? msg_level : &dma_msg_level;


	di->dma64 =
		((bcma_aread32(core, BCMA_IOST) & SISF_DMA64) == SISF_DMA64);

	
	di->core = core;
	di->d64txregbase = txregbase;
	di->d64rxregbase = rxregbase;

	_dma_ctrlflags(di, DMA_CTRL_ROC | DMA_CTRL_PEN, 0);

	DMA_TRACE("%s: %s flags 0x%x ntxd %d nrxd %d "
		  "rxbufsize %d rxextheadroom %d nrxpost %d rxoffset %d "
		  "txregbase %u rxregbase %u\n", name, "DMA64",
		  di->dma.dmactrlflags, ntxd, nrxd, rxbufsize,
		  rxextheadroom, nrxpost, rxoffset, txregbase, rxregbase);

	
	strncpy(di->name, name, MAXNAMEL);
	di->name[MAXNAMEL - 1] = '\0';

	di->dmadev = core->dma_dev;

	
	di->ntxd = (u16) ntxd;
	di->nrxd = (u16) nrxd;

	
	di->rxextrahdrroom =
	    (rxextheadroom == -1) ? BCMEXTRAHDROOM : rxextheadroom;
	if (rxbufsize > BCMEXTRAHDROOM)
		di->rxbufsize = (u16) (rxbufsize - di->rxextrahdrroom);
	else
		di->rxbufsize = (u16) rxbufsize;

	di->nrxpost = (u16) nrxpost;
	di->rxoffset = (u8) rxoffset;

	di->ddoffsetlow = 0;
	di->dataoffsetlow = 0;
	
	di->ddoffsetlow = 0;
	di->ddoffsethigh = SI_PCIE_DMA_H32;
	di->dataoffsetlow = di->ddoffsetlow;
	di->dataoffsethigh = di->ddoffsethigh;
	
	if ((core->id.id == SDIOD_CORE_ID)
	    && ((rev > 0) && (rev <= 2)))
		di->addrext = false;
	else if ((core->id.id == I2S_CORE_ID) &&
		 ((rev == 0) || (rev == 1)))
		di->addrext = false;
	else
		di->addrext = _dma_isaddrext(di);

	
	di->aligndesc_4k = _dma_descriptor_align(di);
	if (di->aligndesc_4k) {
		di->dmadesc_align = D64RINGALIGN_BITS;
		if ((ntxd < D64MAXDD / 2) && (nrxd < D64MAXDD / 2))
			
			di->dmadesc_align = D64RINGALIGN_BITS - 1;
	} else {
		di->dmadesc_align = 4;	
	}

	DMA_NONE("DMA descriptor align_needed %d, align %d\n",
		 di->aligndesc_4k, di->dmadesc_align);

	
	if (ntxd) {
		size = ntxd * sizeof(void *);
		di->txp = kzalloc(size, GFP_ATOMIC);
		if (di->txp == NULL)
			goto fail;
	}

	
	if (nrxd) {
		size = nrxd * sizeof(void *);
		di->rxp = kzalloc(size, GFP_ATOMIC);
		if (di->rxp == NULL)
			goto fail;
	}

	if (ntxd) {
		if (!_dma_alloc(di, DMA_TX))
			goto fail;
	}

	if (nrxd) {
		if (!_dma_alloc(di, DMA_RX))
			goto fail;
	}

	if ((di->ddoffsetlow != 0) && !di->addrext) {
		if (di->txdpa > SI_PCI_DMA_SZ) {
			DMA_ERROR("%s: txdpa 0x%x: addrext not supported\n",
				  di->name, (u32)di->txdpa);
			goto fail;
		}
		if (di->rxdpa > SI_PCI_DMA_SZ) {
			DMA_ERROR("%s: rxdpa 0x%x: addrext not supported\n",
				  di->name, (u32)di->rxdpa);
			goto fail;
		}
	}

	DMA_TRACE("ddoffsetlow 0x%x ddoffsethigh 0x%x dataoffsetlow 0x%x dataoffsethigh 0x%x addrext %d\n",
		  di->ddoffsetlow, di->ddoffsethigh,
		  di->dataoffsetlow, di->dataoffsethigh,
		  di->addrext);

	return (struct dma_pub *) di;

 fail:
	dma_detach((struct dma_pub *)di);
	return NULL;
}

static inline void
dma64_dd_upd(struct dma_info *di, struct dma64desc *ddring,
	     dma_addr_t pa, uint outidx, u32 *flags, u32 bufcount)
{
	u32 ctrl2 = bufcount & D64_CTRL2_BC_MASK;

	
	if ((di->dataoffsetlow == 0) || !(pa & PCI32ADDR_HIGH)) {
		ddring[outidx].addrlow = cpu_to_le32(pa + di->dataoffsetlow);
		ddring[outidx].addrhigh = cpu_to_le32(di->dataoffsethigh);
		ddring[outidx].ctrl1 = cpu_to_le32(*flags);
		ddring[outidx].ctrl2 = cpu_to_le32(ctrl2);
	} else {
		
		u32 ae;

		ae = (pa & PCI32ADDR_HIGH) >> PCI32ADDR_HIGH_SHIFT;
		pa &= ~PCI32ADDR_HIGH;

		ctrl2 |= (ae << D64_CTRL2_AE_SHIFT) & D64_CTRL2_AE;
		ddring[outidx].addrlow = cpu_to_le32(pa + di->dataoffsetlow);
		ddring[outidx].addrhigh = cpu_to_le32(di->dataoffsethigh);
		ddring[outidx].ctrl1 = cpu_to_le32(*flags);
		ddring[outidx].ctrl2 = cpu_to_le32(ctrl2);
	}
	if (di->dma.dmactrlflags & DMA_CTRL_PEN) {
		if (dma64_dd_parity(&ddring[outidx]))
			ddring[outidx].ctrl2 =
			     cpu_to_le32(ctrl2 | D64_CTRL2_PARITY);
	}
}

void dma_detach(struct dma_pub *pub)
{
	struct dma_info *di = (struct dma_info *)pub;

	DMA_TRACE("%s:\n", di->name);

	
	if (di->txd64)
		dma_free_coherent(di->dmadev, di->txdalloc,
				  ((s8 *)di->txd64 - di->txdalign),
				  (di->txdpaorig));
	if (di->rxd64)
		dma_free_coherent(di->dmadev, di->rxdalloc,
				  ((s8 *)di->rxd64 - di->rxdalign),
				  (di->rxdpaorig));

	
	kfree(di->txp);
	kfree(di->rxp);

	
	kfree(di);

}

static void
_dma_ddtable_init(struct dma_info *di, uint direction, dma_addr_t pa)
{
	if (!di->aligndesc_4k) {
		if (direction == DMA_TX)
			di->xmtptrbase = pa;
		else
			di->rcvptrbase = pa;
	}

	if ((di->ddoffsetlow == 0)
	    || !(pa & PCI32ADDR_HIGH)) {
		if (direction == DMA_TX) {
			bcma_write32(di->core, DMA64TXREGOFFS(di, addrlow),
				     pa + di->ddoffsetlow);
			bcma_write32(di->core, DMA64TXREGOFFS(di, addrhigh),
				     di->ddoffsethigh);
		} else {
			bcma_write32(di->core, DMA64RXREGOFFS(di, addrlow),
				     pa + di->ddoffsetlow);
			bcma_write32(di->core, DMA64RXREGOFFS(di, addrhigh),
				     di->ddoffsethigh);
		}
	} else {
		
		u32 ae;

		
		ae = (pa & PCI32ADDR_HIGH) >> PCI32ADDR_HIGH_SHIFT;
		pa &= ~PCI32ADDR_HIGH;

		if (direction == DMA_TX) {
			bcma_write32(di->core, DMA64TXREGOFFS(di, addrlow),
				     pa + di->ddoffsetlow);
			bcma_write32(di->core, DMA64TXREGOFFS(di, addrhigh),
				     di->ddoffsethigh);
			bcma_maskset32(di->core, DMA64TXREGOFFS(di, control),
				       D64_XC_AE, (ae << D64_XC_AE_SHIFT));
		} else {
			bcma_write32(di->core, DMA64RXREGOFFS(di, addrlow),
				     pa + di->ddoffsetlow);
			bcma_write32(di->core, DMA64RXREGOFFS(di, addrhigh),
				     di->ddoffsethigh);
			bcma_maskset32(di->core, DMA64RXREGOFFS(di, control),
				       D64_RC_AE, (ae << D64_RC_AE_SHIFT));
		}
	}
}

static void _dma_rxenable(struct dma_info *di)
{
	uint dmactrlflags = di->dma.dmactrlflags;
	u32 control;

	DMA_TRACE("%s:\n", di->name);

	control = D64_RC_RE | (bcma_read32(di->core,
					   DMA64RXREGOFFS(di, control)) &
			       D64_RC_AE);

	if ((dmactrlflags & DMA_CTRL_PEN) == 0)
		control |= D64_RC_PD;

	if (dmactrlflags & DMA_CTRL_ROC)
		control |= D64_RC_OC;

	bcma_write32(di->core, DMA64RXREGOFFS(di, control),
		((di->rxoffset << D64_RC_RO_SHIFT) | control));
}

void dma_rxinit(struct dma_pub *pub)
{
	struct dma_info *di = (struct dma_info *)pub;

	DMA_TRACE("%s:\n", di->name);

	if (di->nrxd == 0)
		return;

	di->rxin = di->rxout = 0;

	
	memset(di->rxd64, '\0', di->nrxd * sizeof(struct dma64desc));

	if (!di->aligndesc_4k)
		_dma_ddtable_init(di, DMA_RX, di->rxdpa);

	_dma_rxenable(di);

	if (di->aligndesc_4k)
		_dma_ddtable_init(di, DMA_RX, di->rxdpa);
}

static struct sk_buff *dma64_getnextrxp(struct dma_info *di, bool forceall)
{
	uint i, curr;
	struct sk_buff *rxp;
	dma_addr_t pa;

	i = di->rxin;

	
	if (i == di->rxout)
		return NULL;

	curr =
	    B2I(((bcma_read32(di->core,
			      DMA64RXREGOFFS(di, status0)) & D64_RS0_CD_MASK) -
		 di->rcvptrbase) & D64_RS0_CD_MASK, struct dma64desc);

	
	if (!forceall && (i == curr))
		return NULL;

	
	rxp = di->rxp[i];
	di->rxp[i] = NULL;

	pa = le32_to_cpu(di->rxd64[i].addrlow) - di->dataoffsetlow;

	
	dma_unmap_single(di->dmadev, pa, di->rxbufsize, DMA_FROM_DEVICE);

	di->rxd64[i].addrlow = cpu_to_le32(0xdeadbeef);
	di->rxd64[i].addrhigh = cpu_to_le32(0xdeadbeef);

	di->rxin = nextrxd(di, i);

	return rxp;
}

static struct sk_buff *_dma_getnextrxp(struct dma_info *di, bool forceall)
{
	if (di->nrxd == 0)
		return NULL;

	return dma64_getnextrxp(di, forceall);
}

int dma_rx(struct dma_pub *pub, struct sk_buff_head *skb_list)
{
	struct dma_info *di = (struct dma_info *)pub;
	struct sk_buff_head dma_frames;
	struct sk_buff *p, *next;
	uint len;
	uint pkt_len;
	int resid = 0;
	int pktcnt = 1;

	skb_queue_head_init(&dma_frames);
 next_frame:
	p = _dma_getnextrxp(di, false);
	if (p == NULL)
		return 0;

	len = le16_to_cpu(*(__le16 *) (p->data));
	DMA_TRACE("%s: dma_rx len %d\n", di->name, len);
	dma_spin_for_len(len, p);

	
	pkt_len = min((di->rxoffset + len), di->rxbufsize);
	__skb_trim(p, pkt_len);
	skb_queue_tail(&dma_frames, p);
	resid = len - (di->rxbufsize - di->rxoffset);

	
	if (resid > 0) {
		while ((resid > 0) && (p = _dma_getnextrxp(di, false))) {
			pkt_len = min_t(uint, resid, di->rxbufsize);
			__skb_trim(p, pkt_len);
			skb_queue_tail(&dma_frames, p);
			resid -= di->rxbufsize;
			pktcnt++;
		}

#ifdef DEBUG
		if (resid > 0) {
			uint cur;
			cur =
			    B2I(((bcma_read32(di->core,
					      DMA64RXREGOFFS(di, status0)) &
				  D64_RS0_CD_MASK) - di->rcvptrbase) &
				D64_RS0_CD_MASK, struct dma64desc);
			DMA_ERROR("rxin %d rxout %d, hw_curr %d\n",
				   di->rxin, di->rxout, cur);
		}
#endif				

		if ((di->dma.dmactrlflags & DMA_CTRL_RXMULTI) == 0) {
			DMA_ERROR("%s: bad frame length (%d)\n",
				  di->name, len);
			skb_queue_walk_safe(&dma_frames, p, next) {
				skb_unlink(p, &dma_frames);
				brcmu_pkt_buf_free_skb(p);
			}
			di->dma.rxgiants++;
			pktcnt = 1;
			goto next_frame;
		}
	}

	skb_queue_splice_tail(&dma_frames, skb_list);
	return pktcnt;
}

static bool dma64_rxidle(struct dma_info *di)
{
	DMA_TRACE("%s:\n", di->name);

	if (di->nrxd == 0)
		return true;

	return ((bcma_read32(di->core,
			     DMA64RXREGOFFS(di, status0)) & D64_RS0_CD_MASK) ==
		(bcma_read32(di->core, DMA64RXREGOFFS(di, ptr)) &
		 D64_RS0_CD_MASK));
}

bool dma_rxfill(struct dma_pub *pub)
{
	struct dma_info *di = (struct dma_info *)pub;
	struct sk_buff *p;
	u16 rxin, rxout;
	u32 flags = 0;
	uint n;
	uint i;
	dma_addr_t pa;
	uint extra_offset = 0;
	bool ring_empty;

	ring_empty = false;


	rxin = di->rxin;
	rxout = di->rxout;

	n = di->nrxpost - nrxdactive(di, rxin, rxout);

	DMA_TRACE("%s: post %d\n", di->name, n);

	if (di->rxbufsize > BCMEXTRAHDROOM)
		extra_offset = di->rxextrahdrroom;

	for (i = 0; i < n; i++) {
		p = brcmu_pkt_buf_get_skb(di->rxbufsize + extra_offset);

		if (p == NULL) {
			DMA_ERROR("%s: out of rxbufs\n", di->name);
			if (i == 0 && dma64_rxidle(di)) {
				DMA_ERROR("%s: ring is empty !\n", di->name);
				ring_empty = true;
			}
			di->dma.rxnobuf++;
			break;
		}
		
		if (extra_offset)
			skb_pull(p, extra_offset);

		*(u32 *) (p->data) = 0;

		pa = dma_map_single(di->dmadev, p->data, di->rxbufsize,
				    DMA_FROM_DEVICE);

		
		di->rxp[rxout] = p;

		
		flags = 0;
		if (rxout == (di->nrxd - 1))
			flags = D64_CTRL1_EOT;

		dma64_dd_upd(di, di->rxd64, pa, rxout, &flags,
			     di->rxbufsize);
		rxout = nextrxd(di, rxout);
	}

	di->rxout = rxout;

	
	bcma_write32(di->core, DMA64RXREGOFFS(di, ptr),
	      di->rcvptrbase + I2B(rxout, struct dma64desc));

	return ring_empty;
}

void dma_rxreclaim(struct dma_pub *pub)
{
	struct dma_info *di = (struct dma_info *)pub;
	struct sk_buff *p;

	DMA_TRACE("%s:\n", di->name);

	while ((p = _dma_getnextrxp(di, true)))
		brcmu_pkt_buf_free_skb(p);
}

void dma_counterreset(struct dma_pub *pub)
{
	
	pub->rxgiants = 0;
	pub->rxnobuf = 0;
	pub->txnobuf = 0;
}

unsigned long dma_getvar(struct dma_pub *pub, const char *name)
{
	struct dma_info *di = (struct dma_info *)pub;

	if (!strcmp(name, "&txavail"))
		return (unsigned long)&(di->dma.txavail);
	return 0;
}


void dma_txinit(struct dma_pub *pub)
{
	struct dma_info *di = (struct dma_info *)pub;
	u32 control = D64_XC_XE;

	DMA_TRACE("%s:\n", di->name);

	if (di->ntxd == 0)
		return;

	di->txin = di->txout = 0;
	di->dma.txavail = di->ntxd - 1;

	
	memset(di->txd64, '\0', (di->ntxd * sizeof(struct dma64desc)));

	if (!di->aligndesc_4k)
		_dma_ddtable_init(di, DMA_TX, di->txdpa);

	if ((di->dma.dmactrlflags & DMA_CTRL_PEN) == 0)
		control |= D64_XC_PD;
	bcma_set32(di->core, DMA64TXREGOFFS(di, control), control);

	if (di->aligndesc_4k)
		_dma_ddtable_init(di, DMA_TX, di->txdpa);
}

void dma_txsuspend(struct dma_pub *pub)
{
	struct dma_info *di = (struct dma_info *)pub;

	DMA_TRACE("%s:\n", di->name);

	if (di->ntxd == 0)
		return;

	bcma_set32(di->core, DMA64TXREGOFFS(di, control), D64_XC_SE);
}

void dma_txresume(struct dma_pub *pub)
{
	struct dma_info *di = (struct dma_info *)pub;

	DMA_TRACE("%s:\n", di->name);

	if (di->ntxd == 0)
		return;

	bcma_mask32(di->core, DMA64TXREGOFFS(di, control), ~D64_XC_SE);
}

bool dma_txsuspended(struct dma_pub *pub)
{
	struct dma_info *di = (struct dma_info *)pub;

	return (di->ntxd == 0) ||
	       ((bcma_read32(di->core,
			     DMA64TXREGOFFS(di, control)) & D64_XC_SE) ==
		D64_XC_SE);
}

void dma_txreclaim(struct dma_pub *pub, enum txd_range range)
{
	struct dma_info *di = (struct dma_info *)pub;
	struct sk_buff *p;

	DMA_TRACE("%s: %s\n",
		  di->name,
		  range == DMA_RANGE_ALL ? "all" :
		  range == DMA_RANGE_TRANSMITTED ? "transmitted" :
		  "transferred");

	if (di->txin == di->txout)
		return;

	while ((p = dma_getnexttxp(pub, range))) {
		
		if (!(di->dma.dmactrlflags & DMA_CTRL_UNFRAMED))
			brcmu_pkt_buf_free_skb(p);
	}
}

bool dma_txreset(struct dma_pub *pub)
{
	struct dma_info *di = (struct dma_info *)pub;
	u32 status;

	if (di->ntxd == 0)
		return true;

	
	bcma_write32(di->core, DMA64TXREGOFFS(di, control), D64_XC_SE);
	SPINWAIT(((status =
		   (bcma_read32(di->core, DMA64TXREGOFFS(di, status0)) &
		    D64_XS0_XS_MASK)) != D64_XS0_XS_DISABLED) &&
		  (status != D64_XS0_XS_IDLE) && (status != D64_XS0_XS_STOPPED),
		 10000);

	bcma_write32(di->core, DMA64TXREGOFFS(di, control), 0);
	SPINWAIT(((status =
		   (bcma_read32(di->core, DMA64TXREGOFFS(di, status0)) &
		    D64_XS0_XS_MASK)) != D64_XS0_XS_DISABLED), 10000);

	
	udelay(300);

	return status == D64_XS0_XS_DISABLED;
}

bool dma_rxreset(struct dma_pub *pub)
{
	struct dma_info *di = (struct dma_info *)pub;
	u32 status;

	if (di->nrxd == 0)
		return true;

	bcma_write32(di->core, DMA64RXREGOFFS(di, control), 0);
	SPINWAIT(((status =
		   (bcma_read32(di->core, DMA64RXREGOFFS(di, status0)) &
		    D64_RS0_RS_MASK)) != D64_RS0_RS_DISABLED), 10000);

	return status == D64_RS0_RS_DISABLED;
}

int dma_txfast(struct dma_pub *pub, struct sk_buff *p, bool commit)
{
	struct dma_info *di = (struct dma_info *)pub;
	unsigned char *data;
	uint len;
	u16 txout;
	u32 flags = 0;
	dma_addr_t pa;

	DMA_TRACE("%s:\n", di->name);

	txout = di->txout;

	data = p->data;
	len = p->len;

	
	if (len == 0)
		return 0;

	
	if (nexttxd(di, txout) == di->txin)
		goto outoftxd;

	
	pa = dma_map_single(di->dmadev, data, len, DMA_TO_DEVICE);

	flags = D64_CTRL1_SOF | D64_CTRL1_IOC | D64_CTRL1_EOF;
	if (txout == (di->ntxd - 1))
		flags |= D64_CTRL1_EOT;

	dma64_dd_upd(di, di->txd64, pa, txout, &flags, len);

	txout = nexttxd(di, txout);

	
	di->txp[prevtxd(di, txout)] = p;

	
	di->txout = txout;

	
	if (commit)
		bcma_write32(di->core, DMA64TXREGOFFS(di, ptr),
		      di->xmtptrbase + I2B(txout, struct dma64desc));

	
	di->dma.txavail = di->ntxd - ntxdactive(di, di->txin, di->txout) - 1;

	return 0;

 outoftxd:
	DMA_ERROR("%s: out of txds !!!\n", di->name);
	brcmu_pkt_buf_free_skb(p);
	di->dma.txavail = 0;
	di->dma.txnobuf++;
	return -1;
}

struct sk_buff *dma_getnexttxp(struct dma_pub *pub, enum txd_range range)
{
	struct dma_info *di = (struct dma_info *)pub;
	u16 start, end, i;
	u16 active_desc;
	struct sk_buff *txp;

	DMA_TRACE("%s: %s\n",
		  di->name,
		  range == DMA_RANGE_ALL ? "all" :
		  range == DMA_RANGE_TRANSMITTED ? "transmitted" :
		  "transferred");

	if (di->ntxd == 0)
		return NULL;

	txp = NULL;

	start = di->txin;
	if (range == DMA_RANGE_ALL)
		end = di->txout;
	else {
		end = (u16) (B2I(((bcma_read32(di->core,
					       DMA64TXREGOFFS(di, status0)) &
				   D64_XS0_CD_MASK) - di->xmtptrbase) &
				 D64_XS0_CD_MASK, struct dma64desc));

		if (range == DMA_RANGE_TRANSFERED) {
			active_desc =
				(u16)(bcma_read32(di->core,
						  DMA64TXREGOFFS(di, status1)) &
				      D64_XS1_AD_MASK);
			active_desc =
			    (active_desc - di->xmtptrbase) & D64_XS0_CD_MASK;
			active_desc = B2I(active_desc, struct dma64desc);
			if (end != active_desc)
				end = prevtxd(di, active_desc);
		}
	}

	if ((start == 0) && (end > di->txout))
		goto bogus;

	for (i = start; i != end && !txp; i = nexttxd(di, i)) {
		dma_addr_t pa;
		uint size;

		pa = le32_to_cpu(di->txd64[i].addrlow) - di->dataoffsetlow;

		size =
		    (le32_to_cpu(di->txd64[i].ctrl2) &
		     D64_CTRL2_BC_MASK);

		di->txd64[i].addrlow = cpu_to_le32(0xdeadbeef);
		di->txd64[i].addrhigh = cpu_to_le32(0xdeadbeef);

		txp = di->txp[i];
		di->txp[i] = NULL;

		dma_unmap_single(di->dmadev, pa, size, DMA_TO_DEVICE);
	}

	di->txin = i;

	
	di->dma.txavail = di->ntxd - ntxdactive(di, di->txin, di->txout) - 1;

	return txp;

 bogus:
	DMA_NONE("bogus curr: start %d end %d txout %d\n",
		 start, end, di->txout);
	return NULL;
}

void dma_walk_packets(struct dma_pub *dmah, void (*callback_fnc)
		      (void *pkt, void *arg_a), void *arg_a)
{
	struct dma_info *di = (struct dma_info *) dmah;
	uint i =   di->txin;
	uint end = di->txout;
	struct sk_buff *skb;
	struct ieee80211_tx_info *tx_info;

	while (i != end) {
		skb = (struct sk_buff *)di->txp[i];
		if (skb != NULL) {
			tx_info = (struct ieee80211_tx_info *)skb->cb;
			(callback_fnc)(tx_info, arg_a);
		}
		i = nexttxd(di, i);
	}
}
