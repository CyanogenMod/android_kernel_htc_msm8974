/*
 * Sound driver for Silicon Graphics 320 and 540 Visual Workstations'
 * onboard audio.  See notes in Documentation/sound/oss/vwsnd .
 *
 * Copyright 1999 Silicon Graphics, Inc.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#undef VWSND_DEBUG			






#include <linux/module.h>
#include <linux/init.h>

#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include <asm/visws/cobalt.h>

#include "sound_config.h"


#ifdef VWSND_DEBUG

static DEFINE_MUTEX(vwsnd_mutex);
static int shut_up = 1;


static void dbgassert(const char *fcn, int line, const char *expr)
{
	if (in_interrupt())
		panic("ASSERTION FAILED IN INTERRUPT, %s:%s:%d %s\n",
		      __FILE__, fcn, line, expr);
	else {
		int x;
		printk(KERN_ERR "ASSERTION FAILED, %s:%s:%d %s\n",
		       __FILE__, fcn, line, expr);
		x = * (volatile int *) 0; 
	}
}


#define ASSERT(e)      ((e) ? (void) 0 : dbgassert(__func__, __LINE__, #e))
#define DBGDO(x)            x
#define DBGX(fmt, args...)  (in_interrupt() ? 0 : printk(KERN_ERR fmt, ##args))
#define DBGP(fmt, args...)  (DBGX("%s: " fmt, __func__ , ##args))
#define DBGE(fmt, args...)  (DBGX("%s" fmt, __func__ , ##args))
#define DBGC(rtn)           (DBGP("calling %s\n", rtn))
#define DBGR()              (DBGP("returning\n"))
#define DBGXV(fmt, args...) (shut_up ? 0 : DBGX(fmt, ##args))
#define DBGPV(fmt, args...) (shut_up ? 0 : DBGP(fmt, ##args))
#define DBGEV(fmt, args...) (shut_up ? 0 : DBGE(fmt, ##args))
#define DBGCV(rtn)          (shut_up ? 0 : DBGC(rtn))
#define DBGRV()             (shut_up ? 0 : DBGR())

#else 

#define ASSERT(e)           ((void) 0)
#define DBGDO(x)            
#define DBGX(fmt, args...)  ((void) 0)
#define DBGP(fmt, args...)  ((void) 0)
#define DBGE(fmt, args...)  ((void) 0)
#define DBGC(rtn)           ((void) 0)
#define DBGR()              ((void) 0)
#define DBGPV(fmt, args...) ((void) 0)
#define DBGXV(fmt, args...) ((void) 0)
#define DBGEV(fmt, args...) ((void) 0)
#define DBGCV(rtn)          ((void) 0)
#define DBGRV()             ((void) 0)

#endif 



enum {
	LI_PAGE0_OFFSET = 0x01000 - 0x1000, 
	LI_PAGE1_OFFSET = 0x0F000 - 0x1000, 
	LI_PAGE2_OFFSET = 0x10000 - 0x1000, 
};


typedef struct lithium {
	void *		page0;		
	void *		page1;
	void *		page2;
	spinlock_t	lock;		
} lithium_t;


static void li_destroy(lithium_t *lith)
{
	if (lith->page0) {
		iounmap(lith->page0);
		lith->page0 = NULL;
	}
	if (lith->page1) {
		iounmap(lith->page1);
		lith->page1 = NULL;
	}
	if (lith->page2) {
		iounmap(lith->page2);
		lith->page2 = NULL;
	}
}


static int __init li_create(lithium_t *lith, unsigned long baseaddr)
{
	spin_lock_init(&lith->lock);
	lith->page0 = ioremap_nocache(baseaddr + LI_PAGE0_OFFSET, PAGE_SIZE);
	lith->page1 = ioremap_nocache(baseaddr + LI_PAGE1_OFFSET, PAGE_SIZE);
	lith->page2 = ioremap_nocache(baseaddr + LI_PAGE2_OFFSET, PAGE_SIZE);
	if (!lith->page0 || !lith->page1 || !lith->page2) {
		li_destroy(lith);
		return -ENOMEM;
	}
	return 0;
}


static __inline__ unsigned long li_readl(lithium_t *lith, int off)
{
	return * (volatile unsigned long *) (lith->page0 + off);
}

static __inline__ unsigned char li_readb(lithium_t *lith, int off)
{
	return * (volatile unsigned char *) (lith->page0 + off);
}

static __inline__ void li_writel(lithium_t *lith, int off, unsigned long val)
{
	* (volatile unsigned long *) (lith->page0 + off) = val;
}

static __inline__ void li_writeb(lithium_t *lith, int off, unsigned char val)
{
	* (volatile unsigned char *) (lith->page0 + off) = val;
}




#define LI_HOST_CONTROLLER	0x000
# define LI_HC_RESET		 0x00008000
# define LI_HC_LINK_ENABLE	 0x00004000
# define LI_HC_LINK_FAILURE	 0x00000004
# define LI_HC_LINK_CODEC	 0x00000002
# define LI_HC_LINK_READY	 0x00000001

#define LI_INTR_STATUS		0x010
#define LI_INTR_MASK		0x014
# define LI_INTR_LINK_ERR	 0x00008000
# define LI_INTR_COMM2_TRIG	 0x00000008
# define LI_INTR_COMM2_UNDERFLOW 0x00000004
# define LI_INTR_COMM1_TRIG	 0x00000002
# define LI_INTR_COMM1_OVERFLOW  0x00000001

#define LI_CODEC_COMMAND	0x018
# define LI_CC_BUSY		 0x00008000
# define LI_CC_DIR		 0x00000080
#  define LI_CC_DIR_RD		  LI_CC_DIR
#  define LI_CC_DIR_WR		(!LI_CC_DIR)
# define LI_CC_ADDR_MASK	 0x0000007F

#define LI_CODEC_DATA		0x01C

#define LI_COMM1_BASE		0x100
#define LI_COMM1_CTL		0x104
# define LI_CCTL_RESET		 0x80000000
# define LI_CCTL_SIZE		 0x70000000
# define LI_CCTL_DMA_ENABLE	 0x08000000
# define LI_CCTL_TMASK		 0x07000000 
# define LI_CCTL_TPTR		 0x00FF0000 
# define LI_CCTL_RPTR		 0x0000FF00
# define LI_CCTL_WPTR		 0x000000FF
#define LI_COMM1_CFG		0x108
# define LI_CCFG_LOCK		 0x00008000
# define LI_CCFG_SLOT		 0x00000070
# define LI_CCFG_DIRECTION	 0x00000008
#  define LI_CCFG_DIR_IN	(!LI_CCFG_DIRECTION)
#  define LI_CCFG_DIR_OUT	  LI_CCFG_DIRECTION
# define LI_CCFG_MODE		 0x00000004
#  define LI_CCFG_MODE_MONO	(!LI_CCFG_MODE)
#  define LI_CCFG_MODE_STEREO	  LI_CCFG_MODE
# define LI_CCFG_FORMAT		 0x00000003
#  define LI_CCFG_FMT_8BIT	  0x00000000
#  define LI_CCFG_FMT_16BIT	  0x00000001
#define LI_COMM2_BASE		0x10C
#define LI_COMM2_CTL		0x110
 
#define LI_COMM2_CFG		0x114
 

#define LI_UST_LOW		0x200	
#define LI_UST_HIGH		0x204	

#define LI_AUDIO1_UST		0x300	
#define LI_AUDIO1_MSC		0x304	
#define LI_AUDIO2_UST		0x308	
#define LI_AUDIO2_MSC		0x30C	


#define DMACHUNK_SHIFT 5
#define DMACHUNK_SIZE (1 << DMACHUNK_SHIFT)
#define BYTES_TO_CHUNKS(bytes) ((bytes) >> DMACHUNK_SHIFT)
#define CHUNKS_TO_BYTES(chunks) ((chunks) << DMACHUNK_SHIFT)


#define SHIFT_FIELD(val, mask) (((val) * ((mask) & -(mask))) & (mask))
#define UNSHIFT_FIELD(val, mask) (((val) & (mask)) / ((mask) & -(mask)))


typedef struct dma_chan_desc {
	int basereg;
	int cfgreg;
	int ctlreg;
	int hwptrreg;
	int swptrreg;
	int ustreg;
	int mscreg;
	unsigned long swptrmask;
	int ad1843_slot;
	int direction;			
} dma_chan_desc_t;

static const dma_chan_desc_t li_comm1 = {
	LI_COMM1_BASE,			
	LI_COMM1_CFG,			
	LI_COMM1_CTL,			
	LI_COMM1_CTL + 0,		
	LI_COMM1_CTL + 1,		
	LI_AUDIO1_UST,			
	LI_AUDIO1_MSC,			
	LI_CCTL_RPTR,			
	2,				
	LI_CCFG_DIR_IN			
};

static const dma_chan_desc_t li_comm2 = {
	LI_COMM2_BASE,			
	LI_COMM2_CFG,			
	LI_COMM2_CTL,			
	LI_COMM2_CTL + 1,		
	LI_COMM2_CTL + 0,		
	LI_AUDIO2_UST,			
	LI_AUDIO2_MSC,			
	LI_CCTL_WPTR,			
	2,				
	LI_CCFG_DIR_OUT			
};


typedef struct dma_chan {
	const dma_chan_desc_t *desc;
	lithium_t      *lith;
	unsigned long   baseval;
	unsigned long	cfgval;
	unsigned long	ctlval;
} dma_chan_t;


typedef struct ustmsc {
	unsigned long long ust;
	unsigned long msc;
} ustmsc_t;


static int li_ad1843_wait(lithium_t *lith)
{
	unsigned long later = jiffies + 2;
	while (li_readl(lith, LI_CODEC_COMMAND) & LI_CC_BUSY)
		if (time_after_eq(jiffies, later))
			return -EBUSY;
	return 0;
}


static int li_read_ad1843_reg(lithium_t *lith, int reg)
{
	int val;

	ASSERT(!in_interrupt());
	spin_lock(&lith->lock);
	{
		val = li_ad1843_wait(lith);
		if (val == 0) {
			li_writel(lith, LI_CODEC_COMMAND, LI_CC_DIR_RD | reg);
			val = li_ad1843_wait(lith);
		}
		if (val == 0)
			val = li_readl(lith, LI_CODEC_DATA);
	}
	spin_unlock(&lith->lock);

	DBGXV("li_read_ad1843_reg(lith=0x%p, reg=%d) returns 0x%04x\n",
	      lith, reg, val);

	return val;
}


static void li_write_ad1843_reg(lithium_t *lith, int reg, int newval)
{
	spin_lock(&lith->lock);
	{
		if (li_ad1843_wait(lith) == 0) {
			li_writel(lith, LI_CODEC_DATA, newval);
			li_writel(lith, LI_CODEC_COMMAND, LI_CC_DIR_WR | reg);
		}
	}
	spin_unlock(&lith->lock);
}


static void li_setup_dma(dma_chan_t *chan,
			 const dma_chan_desc_t *desc,
			 lithium_t *lith,
			 unsigned long buffer_paddr,
			 int bufshift,
			 int fragshift,
			 int channels,
			 int sampsize)
{
	unsigned long mode, format;
	unsigned long size, tmask;

	DBGEV("(chan=0x%p, desc=0x%p, lith=0x%p, buffer_paddr=0x%lx, "
	     "bufshift=%d, fragshift=%d, channels=%d, sampsize=%d)\n",
	     chan, desc, lith, buffer_paddr,
	     bufshift, fragshift, channels, sampsize);

	

	li_writel(lith, desc->ctlreg, LI_CCTL_RESET);

	ASSERT(channels == 1 || channels == 2);
	if (channels == 2)
		mode = LI_CCFG_MODE_STEREO;
	else
		mode = LI_CCFG_MODE_MONO;
	ASSERT(sampsize == 1 || sampsize == 2);
	if (sampsize == 2)
		format = LI_CCFG_FMT_16BIT;
	else
		format = LI_CCFG_FMT_8BIT;
	chan->desc = desc;
	chan->lith = lith;


	ASSERT(!(buffer_paddr & 0xFF));
	chan->baseval = (buffer_paddr >> 8) | 1 << (37 - 8);

	chan->cfgval = ((chan->cfgval & ~LI_CCFG_LOCK) |
			SHIFT_FIELD(desc->ad1843_slot, LI_CCFG_SLOT) |
			desc->direction |
			mode |
			format);

	size = bufshift - 6;
	tmask = 13 - fragshift;		
	ASSERT(size >= 2 && size <= 7);
	ASSERT(tmask >= 1 && tmask <= 7);
	chan->ctlval = ((chan->ctlval & ~LI_CCTL_RESET) |
			SHIFT_FIELD(size, LI_CCTL_SIZE) |
			(chan->ctlval & ~LI_CCTL_DMA_ENABLE) |
			SHIFT_FIELD(tmask, LI_CCTL_TMASK) |
			SHIFT_FIELD(0, LI_CCTL_TPTR));

	DBGPV("basereg 0x%x = 0x%lx\n", desc->basereg, chan->baseval);
	DBGPV("cfgreg 0x%x = 0x%lx\n", desc->cfgreg, chan->cfgval);
	DBGPV("ctlreg 0x%x = 0x%lx\n", desc->ctlreg, chan->ctlval);

	li_writel(lith, desc->basereg, chan->baseval);
	li_writel(lith, desc->cfgreg, chan->cfgval);
	li_writel(lith, desc->ctlreg, chan->ctlval);

	DBGRV();
}

static void li_shutdown_dma(dma_chan_t *chan)
{
	lithium_t *lith = chan->lith;
	void * lith1 = lith->page1;

	DBGEV("(chan=0x%p)\n", chan);
	
	chan->ctlval &= ~LI_CCTL_DMA_ENABLE;
	DBGPV("ctlreg 0x%x = 0x%lx\n", chan->desc->ctlreg, chan->ctlval);
	li_writel(lith, chan->desc->ctlreg, chan->ctlval);


	if (lith1 && chan->desc->direction == LI_CCFG_DIR_OUT)
		* (volatile unsigned long *) (lith1 + 0x500) = 0;
}


static __inline__ void li_activate_dma(dma_chan_t *chan)
{
	chan->ctlval |= LI_CCTL_DMA_ENABLE;
	DBGPV("ctlval = 0x%lx\n", chan->ctlval);
	li_writel(chan->lith, chan->desc->ctlreg, chan->ctlval);
}

static void li_deactivate_dma(dma_chan_t *chan)
{
	lithium_t *lith = chan->lith;
	void * lith2 = lith->page2;

	chan->ctlval &= ~(LI_CCTL_DMA_ENABLE | LI_CCTL_RPTR | LI_CCTL_WPTR);
	DBGPV("ctlval = 0x%lx\n", chan->ctlval);
	DBGPV("ctlreg 0x%x = 0x%lx\n", chan->desc->ctlreg, chan->ctlval);
	li_writel(lith, chan->desc->ctlreg, chan->ctlval);


	if (lith2 && chan->desc->direction == LI_CCFG_DIR_OUT) {
		* (volatile unsigned long *) (lith2 + 0x98) = 0;
		* (volatile unsigned long *) (lith2 + 0x9C) = 0;
	}
}


static __inline__ int li_read_swptr(dma_chan_t *chan)
{
	const unsigned long mask = chan->desc->swptrmask;

	return CHUNKS_TO_BYTES(UNSHIFT_FIELD(chan->ctlval, mask));
}

static __inline__ int li_read_hwptr(dma_chan_t *chan)
{
	return CHUNKS_TO_BYTES(li_readb(chan->lith, chan->desc->hwptrreg));
}

static __inline__ void li_write_swptr(dma_chan_t *chan, int val)
{
	const unsigned long mask = chan->desc->swptrmask;

	ASSERT(!(val & ~CHUNKS_TO_BYTES(0xFF)));
	val = BYTES_TO_CHUNKS(val);
	chan->ctlval = (chan->ctlval & ~mask) | SHIFT_FIELD(val, mask);
	li_writeb(chan->lith, chan->desc->swptrreg, val);
}


static void li_read_USTMSC(dma_chan_t *chan, ustmsc_t *ustmsc)
{
	lithium_t *lith = chan->lith;
	const dma_chan_desc_t *desc = chan->desc;
	unsigned long now_low, now_high0, now_high1, chan_ust;

	spin_lock(&lith->lock);
	{
		do {
			now_high0 = li_readl(lith, LI_UST_HIGH);
			now_low = li_readl(lith, LI_UST_LOW);


			ustmsc->msc = li_readl(lith, desc->mscreg);
			chan_ust = li_readl(lith, desc->ustreg);

			now_high1 = li_readl(lith, LI_UST_HIGH);
		} while (now_high0 != now_high1);
	}	
	spin_unlock(&lith->lock);
	ustmsc->ust = ((unsigned long long) now_high0 << 32 | chan_ust);
}

static void li_enable_interrupts(lithium_t *lith, unsigned int mask)
{
	DBGEV("(lith=0x%p, mask=0x%x)\n", lith, mask);

	

	li_writel(lith, LI_INTR_STATUS, mask);

	

	mask |= li_readl(lith, LI_INTR_MASK);
	li_writel(lith, LI_INTR_MASK, mask);
}

static void li_disable_interrupts(lithium_t *lith, unsigned int mask)
{
	unsigned int keepmask;

	DBGEV("(lith=0x%p, mask=0x%x)\n", lith, mask);

	

	keepmask = li_readl(lith, LI_INTR_MASK) & ~mask;
	li_writel(lith, LI_INTR_MASK, keepmask);

	

	li_writel(lith, LI_INTR_STATUS, mask);
}


static unsigned int li_get_clear_intr_status(lithium_t *lith)
{
	unsigned int status;

	status = li_readl(lith, LI_INTR_STATUS);
	li_writel(lith, LI_INTR_STATUS, ~0);
	return status & li_readl(lith, LI_INTR_MASK);
}

static int li_init(lithium_t *lith)
{
	

	

	li_writel(lith, LI_HOST_CONTROLLER, LI_HC_RESET);
	udelay(1);


	li_writel(lith, LI_HOST_CONTROLLER, LI_HC_LINK_ENABLE);
	udelay(1);

	return 0;
}



typedef struct ad1843_bitfield {
	char reg;
	char lo_bit;
	char nbits;
} ad1843_bitfield_t;

static const ad1843_bitfield_t
	ad1843_PDNO   = {  0, 14,  1 },	
	ad1843_INIT   = {  0, 15,  1 },	
	ad1843_RIG    = {  2,  0,  4 },	
	ad1843_RMGE   = {  2,  4,  1 },	
	ad1843_RSS    = {  2,  5,  3 },	
	ad1843_LIG    = {  2,  8,  4 },	
	ad1843_LMGE   = {  2, 12,  1 },	
	ad1843_LSS    = {  2, 13,  3 },	
	ad1843_RX1M   = {  4,  0,  5 },	
	ad1843_RX1MM  = {  4,  7,  1 },	
	ad1843_LX1M   = {  4,  8,  5 },	
	ad1843_LX1MM  = {  4, 15,  1 },	
	ad1843_RX2M   = {  5,  0,  5 },	
	ad1843_RX2MM  = {  5,  7,  1 },	
	ad1843_LX2M   = {  5,  8,  5 },	
	ad1843_LX2MM  = {  5, 15,  1 },	
	ad1843_RMCM   = {  7,  0,  5 },	
	ad1843_RMCMM  = {  7,  7,  1 },	
	ad1843_LMCM   = {  7,  8,  5 },	
	ad1843_LMCMM  = {  7, 15,  1 },	
	ad1843_HPOS   = {  8,  4,  1 },	
	ad1843_HPOM   = {  8,  5,  1 },	
	ad1843_RDA1G  = {  9,  0,  6 },	
	ad1843_RDA1GM = {  9,  7,  1 },	
	ad1843_LDA1G  = {  9,  8,  6 },	
	ad1843_LDA1GM = {  9, 15,  1 },	
	ad1843_RDA1AM = { 11,  7,  1 },	
	ad1843_LDA1AM = { 11, 15,  1 },	
	ad1843_ADLC   = { 15,  0,  2 },	
	ad1843_ADRC   = { 15,  2,  2 },	
	ad1843_DA1C   = { 15,  8,  2 },	
	ad1843_C1C    = { 17,  0, 16 },	
	ad1843_C2C    = { 20,  0, 16 },	
	ad1843_DAADL  = { 25,  4,  2 },	
	ad1843_DAADR  = { 25,  6,  2 },	
	ad1843_DRSFLT = { 25, 15,  1 },	
	ad1843_ADLF   = { 26,  0,  2 }, 
	ad1843_ADRF   = { 26,  2,  2 }, 
	ad1843_ADTLK  = { 26,  4,  1 },	
	ad1843_SCF    = { 26,  7,  1 },	
	ad1843_DA1F   = { 26,  8,  2 },	
	ad1843_DA1SM  = { 26, 14,  1 },	
	ad1843_ADLEN  = { 27,  0,  1 },	
	ad1843_ADREN  = { 27,  1,  1 },	
	ad1843_AAMEN  = { 27,  4,  1 },	
	ad1843_ANAEN  = { 27,  7,  1 },	
	ad1843_DA1EN  = { 27,  8,  1 },	
	ad1843_DA2EN  = { 27,  9,  1 },	
	ad1843_C1EN   = { 28, 11,  1 },	
	ad1843_C2EN   = { 28, 12,  1 },	
	ad1843_PDNI   = { 28, 15,  1 };	


typedef struct ad1843_gain {

	int	negative;		
	const ad1843_bitfield_t *lfield;
	const ad1843_bitfield_t *rfield;

} ad1843_gain_t;

static const ad1843_gain_t ad1843_gain_RECLEV
				= { 0, &ad1843_LIG,   &ad1843_RIG };
static const ad1843_gain_t ad1843_gain_LINE
				= { 1, &ad1843_LX1M,  &ad1843_RX1M };
static const ad1843_gain_t ad1843_gain_CD
				= { 1, &ad1843_LX2M,  &ad1843_RX2M };
static const ad1843_gain_t ad1843_gain_MIC
				= { 1, &ad1843_LMCM,  &ad1843_RMCM };
static const ad1843_gain_t ad1843_gain_PCM
				= { 1, &ad1843_LDA1G, &ad1843_RDA1G };


static int ad1843_read_bits(lithium_t *lith, const ad1843_bitfield_t *field)
{
	int w = li_read_ad1843_reg(lith, field->reg);
	int val = w >> field->lo_bit & ((1 << field->nbits) - 1);

	DBGXV("ad1843_read_bits(lith=0x%p, field->{%d %d %d}) returns 0x%x\n",
	      lith, field->reg, field->lo_bit, field->nbits, val);

	return val;
}


static int ad1843_write_bits(lithium_t *lith,
			     const ad1843_bitfield_t *field,
			     int newval)
{
	int w = li_read_ad1843_reg(lith, field->reg);
	int mask = ((1 << field->nbits) - 1) << field->lo_bit;
	int oldval = (w & mask) >> field->lo_bit;
	int newbits = (newval << field->lo_bit) & mask;
	w = (w & ~mask) | newbits;
	(void) li_write_ad1843_reg(lith, field->reg, w);

	DBGXV("ad1843_write_bits(lith=0x%p, field->{%d %d %d}, val=0x%x) "
	      "returns 0x%x\n",
	      lith, field->reg, field->lo_bit, field->nbits, newval,
	      oldval);

	return oldval;
}


static void ad1843_read_multi(lithium_t *lith, int argcount, ...)
{
	va_list ap;
	const ad1843_bitfield_t *fp;
	int w = 0, mask, *value, reg = -1;

	va_start(ap, argcount);
	while (--argcount >= 0) {
		fp = va_arg(ap, const ad1843_bitfield_t *);
		value = va_arg(ap, int *);
		if (reg == -1) {
			reg = fp->reg;
			w = li_read_ad1843_reg(lith, reg);
		}
		ASSERT(reg == fp->reg);
		mask = (1 << fp->nbits) - 1;
		*value = w >> fp->lo_bit & mask;
	}
	va_end(ap);
}


static void ad1843_write_multi(lithium_t *lith, int argcount, ...)
{
	va_list ap;
	int reg;
	const ad1843_bitfield_t *fp;
	int value;
	int w, m, mask, bits;

	mask = 0;
	bits = 0;
	reg = -1;

	va_start(ap, argcount);
	while (--argcount >= 0) {
		fp = va_arg(ap, const ad1843_bitfield_t *);
		value = va_arg(ap, int);
		if (reg == -1)
			reg = fp->reg;
		ASSERT(fp->reg == reg);
		m = ((1 << fp->nbits) - 1) << fp->lo_bit;
		mask |= m;
		bits |= (value << fp->lo_bit) & m;
	}
	va_end(ap);
	ASSERT(!(bits & ~mask));
	if (~mask & 0xFFFF)
		w = li_read_ad1843_reg(lith, reg);
	else
		w = 0;
	w = (w & ~mask) | bits;
	(void) li_write_ad1843_reg(lith, reg, w);
}


static int ad1843_get_gain(lithium_t *lith, const ad1843_gain_t *gp)
{
	int lg, rg;
	unsigned short mask = (1 << gp->lfield->nbits) - 1;

	ad1843_read_multi(lith, 2, gp->lfield, &lg, gp->rfield, &rg);
	if (gp->negative) {
		lg = mask - lg;
		rg = mask - rg;
	}
	lg = (lg * 100 + (mask >> 1)) / mask;
	rg = (rg * 100 + (mask >> 1)) / mask;
	return lg << 0 | rg << 8;
}


static int ad1843_set_gain(lithium_t *lith,
			   const ad1843_gain_t *gp,
			   int newval)
{
	unsigned short mask = (1 << gp->lfield->nbits) - 1;

	int lg = newval >> 0 & 0xFF;
	int rg = newval >> 8;
	if (lg < 0 || lg > 100 || rg < 0 || rg > 100)
		return -EINVAL;
	lg = (lg * mask + (mask >> 1)) / 100;
	rg = (rg * mask + (mask >> 1)) / 100;
	if (gp->negative) {
		lg = mask - lg;
		rg = mask - rg;
	}
	ad1843_write_multi(lith, 2, gp->lfield, lg, gp->rfield, rg);
	return ad1843_get_gain(lith, gp);
}


static int ad1843_get_recsrc(lithium_t *lith)
{
	int ls = ad1843_read_bits(lith, &ad1843_LSS);

	switch (ls) {
	case 1:
		return SOUND_MASK_MIC;
	case 2:
		return SOUND_MASK_LINE;
	case 3:
		return SOUND_MASK_CD;
	case 6:
		return SOUND_MASK_PCM;
	default:
		ASSERT(0);
		return -1;
	}
}

/*
 * Enable/disable digital resample mode in the AD1843.
 *
 * The AD1843 requires that ADL, ADR, DA1 and DA2 be powered down
 * while switching modes.  So we save DA1's state (DA2's state is not
 * interesting), power them down, switch into/out of resample mode,
 * power them up, and restore state.
 *
 * This will cause audible glitches if D/A or A/D is going on, so the
 * driver disallows that (in mixer_write_ioctl()).
 *
 * The open question is, is this worth doing?  I'm leaving it in,
 * because it's written, but...
 */

static void ad1843_set_resample_mode(lithium_t *lith, int onoff)
{
	
	int save_da1 = li_read_ad1843_reg(lith, 9);

	
	ad1843_write_multi(lith, 4,
			   &ad1843_DA1EN, 0,
			   &ad1843_DA2EN, 0,
			   &ad1843_ADLEN, 0,
			   &ad1843_ADREN, 0);

	
	ASSERT(onoff == 0 || onoff == 1);
	ad1843_write_bits(lith, &ad1843_DRSFLT, onoff);

 	
	ad1843_write_multi(lith, 3,
			   &ad1843_DA1EN, 1,
			   &ad1843_ADLEN, 1,
			   &ad1843_ADREN, 1);

	
	li_write_ad1843_reg(lith, 9, save_da1);
}


static int ad1843_set_recsrc(lithium_t *lith, int newsrc)
{
	int bits;
	int oldbits;

	switch (newsrc) {
	case SOUND_MASK_PCM:
		bits = 6;
		break;

	case SOUND_MASK_MIC:
		bits = 1;
		break;

	case SOUND_MASK_LINE:
		bits = 2;
		break;

	case SOUND_MASK_CD:
		bits = 3;
		break;

	default:
		return -EINVAL;
	}
	oldbits = ad1843_read_bits(lith, &ad1843_LSS);
	if (newsrc == SOUND_MASK_PCM && oldbits != 6) {
		DBGP("enabling digital resample mode\n");
		ad1843_set_resample_mode(lith, 1);
		ad1843_write_multi(lith, 2,
				   &ad1843_DAADL, 2,
				   &ad1843_DAADR, 2);
	} else if (newsrc != SOUND_MASK_PCM && oldbits == 6) {
		DBGP("disabling digital resample mode\n");
		ad1843_set_resample_mode(lith, 0);
		ad1843_write_multi(lith, 2,
				   &ad1843_DAADL, 0,
				   &ad1843_DAADR, 0);
	}
	ad1843_write_multi(lith, 2, &ad1843_LSS, bits, &ad1843_RSS, bits);
	return newsrc;
}


static int ad1843_get_outsrc(lithium_t *lith)
{
	int pcm, line, mic, cd;

	pcm  = ad1843_read_bits(lith, &ad1843_LDA1GM) ? 0 : SOUND_MASK_PCM;
	line = ad1843_read_bits(lith, &ad1843_LX1MM)  ? 0 : SOUND_MASK_LINE;
	cd   = ad1843_read_bits(lith, &ad1843_LX2MM)  ? 0 : SOUND_MASK_CD;
	mic  = ad1843_read_bits(lith, &ad1843_LMCMM)  ? 0 : SOUND_MASK_MIC;

	return pcm | line | cd | mic;
}


static int ad1843_set_outsrc(lithium_t *lith, int mask)
{
	int pcm, line, mic, cd;

	if (mask & ~(SOUND_MASK_PCM | SOUND_MASK_LINE |
		     SOUND_MASK_CD | SOUND_MASK_MIC))
		return -EINVAL;
	pcm  = (mask & SOUND_MASK_PCM)  ? 0 : 1;
	line = (mask & SOUND_MASK_LINE) ? 0 : 1;
	mic  = (mask & SOUND_MASK_MIC)  ? 0 : 1;
	cd   = (mask & SOUND_MASK_CD)   ? 0 : 1;

	ad1843_write_multi(lith, 2, &ad1843_LDA1GM, pcm, &ad1843_RDA1GM, pcm);
	ad1843_write_multi(lith, 2, &ad1843_LX1MM, line, &ad1843_RX1MM, line);
	ad1843_write_multi(lith, 2, &ad1843_LX2MM, cd,   &ad1843_RX2MM, cd);
	ad1843_write_multi(lith, 2, &ad1843_LMCMM, mic,  &ad1843_RMCMM, mic);

	return mask;
}


static void ad1843_setup_dac(lithium_t *lith,
			     int framerate,
			     int fmt,
			     int channels)
{
	int ad_fmt = 0, ad_mode = 0;

	DBGEV("(lith=0x%p, framerate=%d, fmt=%d, channels=%d)\n",
	      lith, framerate, fmt, channels);

	switch (fmt) {
	case AFMT_S8:		ad_fmt = 1; break;
	case AFMT_U8:		ad_fmt = 1; break;
	case AFMT_S16_LE:	ad_fmt = 1; break;
	case AFMT_MU_LAW:	ad_fmt = 2; break;
	case AFMT_A_LAW:	ad_fmt = 3; break;
	default:		ASSERT(0);
	}

	switch (channels) {
	case 2:			ad_mode = 0; break;
	case 1:			ad_mode = 1; break;
	default:		ASSERT(0);
	}
		
	DBGPV("ad_mode = %d, ad_fmt = %d\n", ad_mode, ad_fmt);
	ASSERT(framerate >= 4000 && framerate <= 49000);
	ad1843_write_bits(lith, &ad1843_C1C, framerate);
	ad1843_write_multi(lith, 2,
			   &ad1843_DA1SM, ad_mode, &ad1843_DA1F, ad_fmt);
}

static void ad1843_shutdown_dac(lithium_t *lith)
{
	ad1843_write_bits(lith, &ad1843_DA1F, 1);
}

static void ad1843_setup_adc(lithium_t *lith, int framerate, int fmt, int channels)
{
	int da_fmt = 0;

	DBGEV("(lith=0x%p, framerate=%d, fmt=%d, channels=%d)\n",
	      lith, framerate, fmt, channels);

	switch (fmt) {
	case AFMT_S8:		da_fmt = 1; break;
	case AFMT_U8:		da_fmt = 1; break;
	case AFMT_S16_LE:	da_fmt = 1; break;
	case AFMT_MU_LAW:	da_fmt = 2; break;
	case AFMT_A_LAW:	da_fmt = 3; break;
	default:		ASSERT(0);
	}

	DBGPV("da_fmt = %d\n", da_fmt);
	ASSERT(framerate >= 4000 && framerate <= 49000);
	ad1843_write_bits(lith, &ad1843_C2C, framerate);
	ad1843_write_multi(lith, 2,
			   &ad1843_ADLF, da_fmt, &ad1843_ADRF, da_fmt);
}

static void ad1843_shutdown_adc(lithium_t *lith)
{
	
}


static int __init ad1843_init(lithium_t *lith)
{
	unsigned long later;
	int err;

	err = li_init(lith);
	if (err)
		return err;

	if (ad1843_read_bits(lith, &ad1843_INIT) != 0) {
		printk(KERN_ERR "vwsnd sound: AD1843 won't initialize\n");
		return -EIO;
	}

	ad1843_write_bits(lith, &ad1843_SCF, 1);

	

	ad1843_write_bits(lith, &ad1843_PDNI, 0);
	later = jiffies + HZ / 2;	
	DBGDO(shut_up++);
	while (ad1843_read_bits(lith, &ad1843_PDNO)) {
		if (time_after(jiffies, later)) {
			printk(KERN_ERR
			       "vwsnd audio: AD1843 won't power up\n");
			return -EIO;
		}
		schedule();
	}
	DBGDO(shut_up--);

	

	ad1843_write_multi(lith, 2, &ad1843_C1EN, 1, &ad1843_C2EN, 1);

	

 	

	ad1843_write_multi(lith, 3,
			   &ad1843_DA1C, 1,
			   &ad1843_ADLC, 2,
			   &ad1843_ADRC, 2);

	

	ad1843_write_bits(lith, &ad1843_ADTLK, 1);
	ad1843_write_multi(lith, 5,
			   &ad1843_ANAEN, 1,
			   &ad1843_AAMEN, 1,
			   &ad1843_DA1EN, 1,
			   &ad1843_ADLEN, 1,
			   &ad1843_ADREN, 1);

	

	ad1843_write_bits(lith, &ad1843_DA1C, 1);

	

	ad1843_set_outsrc(lith,
			  (SOUND_MASK_PCM | SOUND_MASK_LINE |
			   SOUND_MASK_MIC | SOUND_MASK_CD));
	ad1843_write_multi(lith, 2, &ad1843_LDA1AM, 0, &ad1843_RDA1AM, 0);


	ad1843_set_recsrc(lith, SOUND_MASK_LINE);
	ad1843_write_multi(lith, 2, &ad1843_LMGE, 1, &ad1843_RMGE, 1);

	

	ad1843_write_multi(lith, 2, &ad1843_HPOS, 1, &ad1843_HPOM, 0);

	return 0;
}


#define READ_INTR_MASK  (LI_INTR_COMM1_TRIG | LI_INTR_COMM1_OVERFLOW)
#define WRITE_INTR_MASK (LI_INTR_COMM2_TRIG | LI_INTR_COMM2_UNDERFLOW)

typedef enum vwsnd_port_swstate {	
	SW_OFF,
	SW_INITIAL,
	SW_RUN,
	SW_DRAIN,
} vwsnd_port_swstate_t;

typedef enum vwsnd_port_hwstate {	
	HW_STOPPED,
	HW_RUNNING,
} vwsnd_port_hwstate_t;

/*
 * These flags are read by ISR, but only written at baseline.
 */

typedef enum vwsnd_port_flags {
	DISABLED = 1 << 0,
	ERFLOWN  = 1 << 1,		
	HW_BUSY  = 1 << 2,
} vwsnd_port_flags_t;

/*
 * vwsnd_port is the per-port data structure.  Each device has two
 * ports, one for input and one for output.
 *
 * Locking:
 *
 *	port->lock protects: hwstate, flags, swb_[iu]_avail.
 *
 *	devc->io_mutex protects: swstate, sw_*, swb_[iu]_idx.
 *
 *	everything else is only written by open/release or
 *	pcm_{setup,shutdown}(), which are serialized by a
 *	combination of devc->open_mutex and devc->io_mutex.
 */

typedef struct vwsnd_port {

	spinlock_t	lock;
	wait_queue_head_t queue;
	vwsnd_port_swstate_t swstate;
	vwsnd_port_hwstate_t hwstate;
	vwsnd_port_flags_t flags;

	int		sw_channels;
	int		sw_samplefmt;
	int		sw_framerate;
	int		sample_size;
	int		frame_size;
	unsigned int	zero_word;	

	int		sw_fragshift;
	int		sw_fragcount;
	int		sw_subdivshift;

	unsigned int	hw_fragshift;
	unsigned int	hw_fragsize;
	unsigned int	hw_fragcount;

	int		hwbuf_size;
	unsigned long	hwbuf_paddr;
	unsigned long	hwbuf_vaddr;
	void *		hwbuf;		
	int		hwbuf_max;	

	void *		swbuf;
	unsigned int	swbuf_size;	
	unsigned int	swb_u_idx;	
	unsigned int	swb_i_idx;	
	unsigned int	swb_u_avail;	
	unsigned int	swb_i_avail;	

	dma_chan_t	chan;

	

	int		byte_count;
	int		frag_count;
	int		MSC_offset;

} vwsnd_port_t;


typedef struct vwsnd_dev {
	struct vwsnd_dev *next_dev;
	int		audio_minor;	
	int		mixer_minor;	

	struct mutex open_mutex;
	struct mutex io_mutex;
	struct mutex mix_mutex;
	fmode_t		open_mode;
	wait_queue_head_t open_wait;

	lithium_t	lith;

	vwsnd_port_t	rport;
	vwsnd_port_t	wport;
} vwsnd_dev_t;

static vwsnd_dev_t *vwsnd_dev_list;	

static atomic_t vwsnd_use_count = ATOMIC_INIT(0);

# define INC_USE_COUNT (atomic_inc(&vwsnd_use_count))
# define DEC_USE_COUNT (atomic_dec(&vwsnd_use_count))
# define IN_USE        (atomic_read(&vwsnd_use_count) != 0)


#define HWBUF_SHIFT 13
#define HWBUF_SIZE (1 << HWBUF_SHIFT)
# define HBO         (HWBUF_SHIFT > PAGE_SHIFT ? HWBUF_SHIFT - PAGE_SHIFT : 0)
# define HWBUF_ORDER (HBO + 1)		
#define MIN_SPEED 4000
#define MAX_SPEED 49000

#define MIN_FRAGSHIFT			(DMACHUNK_SHIFT + 1)
#define MAX_FRAGSHIFT			(PAGE_SHIFT)
#define MIN_FRAGSIZE			(1 << MIN_FRAGSHIFT)
#define MAX_FRAGSIZE			(1 << MAX_FRAGSHIFT)
#define MIN_FRAGCOUNT(fragsize)		3
#define MAX_FRAGCOUNT(fragsize)		(32 * PAGE_SIZE / (fragsize))
#define DEFAULT_FRAGSHIFT		12
#define DEFAULT_FRAGCOUNT		16
#define DEFAULT_SUBDIVSHIFT		0


static __inline__ unsigned int __swb_inc_u(vwsnd_port_t *port, int inc)
{
	if (inc) {
		port->swb_u_idx += inc;
		port->swb_u_idx %= port->swbuf_size;
		port->swb_u_avail -= inc;
		port->swb_i_avail += inc;
	}
	return port->swb_u_avail;
}

static __inline__ unsigned int swb_inc_u(vwsnd_port_t *port, int inc)
{
	unsigned long flags;
	unsigned int ret;

	spin_lock_irqsave(&port->lock, flags);
	{
		ret = __swb_inc_u(port, inc);
	}
	spin_unlock_irqrestore(&port->lock, flags);
	return ret;
}

static __inline__ unsigned int __swb_inc_i(vwsnd_port_t *port, int inc)
{
	if (inc) {
		port->swb_i_idx += inc;
		port->swb_i_idx %= port->swbuf_size;
		port->swb_i_avail -= inc;
		port->swb_u_avail += inc;
	}
	return port->swb_i_avail;
}

static __inline__ unsigned int swb_inc_i(vwsnd_port_t *port, int inc)
{
	unsigned long flags;
	unsigned int ret;

	spin_lock_irqsave(&port->lock, flags);
	{
		ret = __swb_inc_i(port, inc);
	}
	spin_unlock_irqrestore(&port->lock, flags);
	return ret;
}


static int pcm_setup(vwsnd_dev_t *devc,
		     vwsnd_port_t *rport,
		     vwsnd_port_t *wport)
{
	vwsnd_port_t *aport = rport ? rport : wport;
	int sample_size;
	unsigned int zero_word;

	DBGEV("(devc=0x%p, rport=0x%p, wport=0x%p)\n", devc, rport, wport);

	ASSERT(aport != NULL);
	if (aport->swbuf != NULL)
		return 0;
	switch (aport->sw_samplefmt) {
	case AFMT_MU_LAW:
		sample_size = 1;
		zero_word = 0xFFFFFFFF ^ 0x80808080;
		break;

	case AFMT_A_LAW:
		sample_size = 1;
		zero_word = 0xD5D5D5D5 ^ 0x80808080;
		break;

	case AFMT_U8:
		sample_size = 1;
		zero_word = 0x80808080;
		break;

	case AFMT_S8:
		sample_size = 1;
		zero_word = 0x00000000;
		break;

	case AFMT_S16_LE:
		sample_size = 2;
		zero_word = 0x00000000;
		break;

	default:
		sample_size = 0;	
		zero_word = 0;
		ASSERT(0);
	}
	aport->sample_size  = sample_size;
	aport->zero_word    = zero_word;
	aport->frame_size   = aport->sw_channels * aport->sample_size;
	aport->hw_fragshift = aport->sw_fragshift - aport->sw_subdivshift;
	aport->hw_fragsize  = 1 << aport->hw_fragshift;
	aport->hw_fragcount = aport->sw_fragcount << aport->sw_subdivshift;
	ASSERT(aport->hw_fragsize >= MIN_FRAGSIZE);
	ASSERT(aport->hw_fragsize <= MAX_FRAGSIZE);
	ASSERT(aport->hw_fragcount >= MIN_FRAGCOUNT(aport->hw_fragsize));
	ASSERT(aport->hw_fragcount <= MAX_FRAGCOUNT(aport->hw_fragsize));
	if (rport) {
		int hwfrags, swfrags;
		rport->hwbuf_max = aport->hwbuf_size - DMACHUNK_SIZE;
		hwfrags = rport->hwbuf_max >> aport->hw_fragshift;
		swfrags = aport->hw_fragcount - hwfrags;
		if (swfrags < 2)
			swfrags = 2;
		rport->swbuf_size = swfrags * aport->hw_fragsize;
		DBGPV("hwfrags = %d, swfrags = %d\n", hwfrags, swfrags);
		DBGPV("read hwbuf_max = %d, swbuf_size = %d\n",
		     rport->hwbuf_max, rport->swbuf_size);
	}
	if (wport) {
		int hwfrags, swfrags;
		int total_bytes = aport->hw_fragcount * aport->hw_fragsize;
		wport->hwbuf_max = aport->hwbuf_size - DMACHUNK_SIZE;
		if (wport->hwbuf_max > total_bytes)
			wport->hwbuf_max = total_bytes;
		hwfrags = wport->hwbuf_max >> aport->hw_fragshift;
		DBGPV("hwfrags = %d\n", hwfrags);
		swfrags = aport->hw_fragcount - hwfrags;
		if (swfrags < 2)
			swfrags = 2;
		wport->swbuf_size = swfrags * aport->hw_fragsize;
		DBGPV("hwfrags = %d, swfrags = %d\n", hwfrags, swfrags);
		DBGPV("write hwbuf_max = %d, swbuf_size = %d\n",
		     wport->hwbuf_max, wport->swbuf_size);
	}

	aport->swb_u_idx    = 0;
	aport->swb_i_idx    = 0;
	aport->byte_count   = 0;


	aport->swbuf        = vmalloc(aport->swbuf_size + PAGE_SIZE);
	if (!aport->swbuf)
		return -ENOMEM;
	if (rport && wport) {
		ASSERT(aport == rport);
		ASSERT(wport->swbuf == NULL);
		
		wport->swbuf = vmalloc(aport->swbuf_size + PAGE_SIZE);
		if (!wport->swbuf) {
			vfree(aport->swbuf);
			aport->swbuf = NULL;
			return -ENOMEM;
		}
		wport->sample_size  = rport->sample_size;
		wport->zero_word    = rport->zero_word;
		wport->frame_size   = rport->frame_size;
		wport->hw_fragshift = rport->hw_fragshift;
		wport->hw_fragsize  = rport->hw_fragsize;
		wport->hw_fragcount = rport->hw_fragcount;
		wport->swbuf_size   = rport->swbuf_size;
		wport->hwbuf_max    = rport->hwbuf_max;
		wport->swb_u_idx    = rport->swb_u_idx;
		wport->swb_i_idx    = rport->swb_i_idx;
		wport->byte_count   = rport->byte_count;
	}
	if (rport) {
		rport->swb_u_avail = 0;
		rport->swb_i_avail = rport->swbuf_size;
		rport->swstate = SW_RUN;
		li_setup_dma(&rport->chan,
			     &li_comm1,
			     &devc->lith,
			     rport->hwbuf_paddr,
			     HWBUF_SHIFT,
			     rport->hw_fragshift,
			     rport->sw_channels,
			     rport->sample_size);
		ad1843_setup_adc(&devc->lith,
				 rport->sw_framerate,
				 rport->sw_samplefmt,
				 rport->sw_channels);
		li_enable_interrupts(&devc->lith, READ_INTR_MASK);
		if (!(rport->flags & DISABLED)) {
			ustmsc_t ustmsc;
			rport->hwstate = HW_RUNNING;
			li_activate_dma(&rport->chan);
			li_read_USTMSC(&rport->chan, &ustmsc);
			rport->MSC_offset = ustmsc.msc;
		}
	}
	if (wport) {
		if (wport->hwbuf_max > wport->swbuf_size)
			wport->hwbuf_max = wport->swbuf_size;
		wport->flags &= ~ERFLOWN;
		wport->swb_u_avail = wport->swbuf_size;
		wport->swb_i_avail = 0;
		wport->swstate = SW_RUN;
		li_setup_dma(&wport->chan,
			     &li_comm2,
			     &devc->lith,
			     wport->hwbuf_paddr,
			     HWBUF_SHIFT,
			     wport->hw_fragshift,
			     wport->sw_channels,
			     wport->sample_size);
		ad1843_setup_dac(&devc->lith,
				 wport->sw_framerate,
				 wport->sw_samplefmt,
				 wport->sw_channels);
		li_enable_interrupts(&devc->lith, WRITE_INTR_MASK);
	}
	DBGRV();
	return 0;
}


static void pcm_shutdown_port(vwsnd_dev_t *devc,
			      vwsnd_port_t *aport,
			      unsigned int mask)
{
	unsigned long flags;
	vwsnd_port_hwstate_t hwstate;
	DECLARE_WAITQUEUE(wait, current);

	aport->swstate = SW_INITIAL;
	add_wait_queue(&aport->queue, &wait);
	while (1) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		spin_lock_irqsave(&aport->lock, flags);
		{
			hwstate = aport->hwstate;
		}		
		spin_unlock_irqrestore(&aport->lock, flags);
		if (hwstate == HW_STOPPED)
			break;
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&aport->queue, &wait);
	li_disable_interrupts(&devc->lith, mask);
	if (aport == &devc->rport)
		ad1843_shutdown_adc(&devc->lith);
	else 
		ad1843_shutdown_dac(&devc->lith);
	li_shutdown_dma(&aport->chan);
	vfree(aport->swbuf);
	aport->swbuf = NULL;
	aport->byte_count = 0;
}


static void pcm_shutdown(vwsnd_dev_t *devc,
			 vwsnd_port_t *rport,
			 vwsnd_port_t *wport)
{
	DBGEV("(devc=0x%p, rport=0x%p, wport=0x%p)\n", devc, rport, wport);

	if (rport && rport->swbuf) {
		DBGPV("shutting down rport\n");
		pcm_shutdown_port(devc, rport, READ_INTR_MASK);
	}
	if (wport && wport->swbuf) {
		DBGPV("shutting down wport\n");
		pcm_shutdown_port(devc, wport, WRITE_INTR_MASK);
	}
	DBGRV();
}

static void pcm_copy_in(vwsnd_port_t *rport, int swidx, int hwidx, int nb)
{
	char *src = rport->hwbuf + hwidx;
	char *dst = rport->swbuf + swidx;
	int fmt = rport->sw_samplefmt;

	DBGPV("swidx = %d, hwidx = %d\n", swidx, hwidx);
	ASSERT(rport->hwbuf != NULL);
	ASSERT(rport->swbuf != NULL);
	ASSERT(nb > 0 && (nb % 32) == 0);
	ASSERT(swidx % 32 == 0 && hwidx % 32 == 0);
	ASSERT(swidx >= 0 && swidx + nb <= rport->swbuf_size);
	ASSERT(hwidx >= 0 && hwidx + nb <= rport->hwbuf_size);

	if (fmt == AFMT_MU_LAW || fmt == AFMT_A_LAW || fmt == AFMT_S8) {

		

		char *end = src + nb;
		while (src < end)
			*dst++ = *src++ ^ 0x80;
	} else
		memcpy(dst, src, nb);
}

static void pcm_copy_out(vwsnd_port_t *wport, int swidx, int hwidx, int nb)
{
	char *src = wport->swbuf + swidx;
	char *dst = wport->hwbuf + hwidx;
	int fmt = wport->sw_samplefmt;

	ASSERT(nb > 0 && (nb % 32) == 0);
	ASSERT(wport->hwbuf != NULL);
	ASSERT(wport->swbuf != NULL);
	ASSERT(swidx % 32 == 0 && hwidx % 32 == 0);
	ASSERT(swidx >= 0 && swidx + nb <= wport->swbuf_size);
	ASSERT(hwidx >= 0 && hwidx + nb <= wport->hwbuf_size);
	if (fmt == AFMT_MU_LAW || fmt == AFMT_A_LAW || fmt == AFMT_S8) {

		

		char *end = src + nb;
		while (src < end)
			*dst++ = *src++ ^ 0x80;
	} else
		memcpy(dst, src, nb);
}


static void pcm_output(vwsnd_dev_t *devc, int erflown, int nb)
{
	vwsnd_port_t *wport = &devc->wport;
	const int hwmax  = wport->hwbuf_max;
	const int hwsize = wport->hwbuf_size;
	const int swsize = wport->swbuf_size;
	const int fragsize = wport->hw_fragsize;
	unsigned long iflags;

	DBGEV("(devc=0x%p, erflown=%d, nb=%d)\n", devc, erflown, nb);
	spin_lock_irqsave(&wport->lock, iflags);
	if (erflown)
		wport->flags |= ERFLOWN;
	(void) __swb_inc_u(wport, nb);
	if (wport->flags & HW_BUSY) {
		spin_unlock_irqrestore(&wport->lock, iflags);
		DBGPV("returning: HW BUSY\n");
		return;
	}
	if (wport->flags & DISABLED) {
		spin_unlock_irqrestore(&wport->lock, iflags);
		DBGPV("returning: DISABLED\n");
		return;
	}
	wport->flags |= HW_BUSY;
	while (1) {
		int swptr, hwptr, hw_avail, sw_avail, swidx;
		vwsnd_port_hwstate_t hwstate = wport->hwstate;
		vwsnd_port_swstate_t swstate = wport->swstate;
		int hw_unavail;
		ustmsc_t ustmsc;

		hwptr = li_read_hwptr(&wport->chan);
		swptr = li_read_swptr(&wport->chan);
		hw_unavail = (swptr - hwptr + hwsize) % hwsize;
		hw_avail = (hwmax - hw_unavail) & -fragsize;
		sw_avail = wport->swb_i_avail & -fragsize;
		if (sw_avail && swstate == SW_RUN) {
			if (wport->flags & ERFLOWN) {
				wport->flags &= ~ERFLOWN;
			}
		} else if (swstate == SW_INITIAL ||
			 swstate == SW_OFF ||
			 (swstate == SW_DRAIN &&
			  !sw_avail &&
			  (wport->flags & ERFLOWN))) {
			DBGP("stopping.  hwstate = %d\n", hwstate);
			if (hwstate != HW_STOPPED) {
				li_deactivate_dma(&wport->chan);
				wport->hwstate = HW_STOPPED;
			}
			wake_up(&wport->queue);
			break;
		}
		if (!sw_avail || !hw_avail)
			break;
		spin_unlock_irqrestore(&wport->lock, iflags);


		swidx = wport->swb_i_idx;
		nb = hw_avail;
		if (nb > sw_avail)
			nb = sw_avail;
		if (nb > hwsize - swptr)
			nb = hwsize - swptr; 
		if (nb > swsize - swidx)
			nb = swsize - swidx; 
		ASSERT(nb > 0);
		if (nb % fragsize) {
			DBGP("nb = %d, fragsize = %d\n", nb, fragsize);
			DBGP("hw_avail = %d\n", hw_avail);
			DBGP("sw_avail = %d\n", sw_avail);
			DBGP("hwsize = %d, swptr = %d\n", hwsize, swptr);
			DBGP("swsize = %d, swidx = %d\n", swsize, swidx);
		}
		ASSERT(!(nb % fragsize));
		DBGPV("copying swb[%d..%d] to hwb[%d..%d]\n",
		      swidx, swidx + nb, swptr, swptr + nb);
		pcm_copy_out(wport, swidx, swptr, nb);
		li_write_swptr(&wport->chan, (swptr + nb) % hwsize);
		spin_lock_irqsave(&wport->lock, iflags);
		if (hwstate == HW_STOPPED) {
			DBGPV("starting\n");
			li_activate_dma(&wport->chan);
			wport->hwstate = HW_RUNNING;
			li_read_USTMSC(&wport->chan, &ustmsc);
			ASSERT(wport->byte_count % wport->frame_size == 0);
			wport->MSC_offset = ustmsc.msc - wport->byte_count / wport->frame_size;
		}
		__swb_inc_i(wport, nb);
		wport->byte_count += nb;
		wport->frag_count += nb / fragsize;
		ASSERT(nb % fragsize == 0);
		wake_up(&wport->queue);
	}
	wport->flags &= ~HW_BUSY;
	spin_unlock_irqrestore(&wport->lock, iflags);
	DBGRV();
}


static void pcm_input(vwsnd_dev_t *devc, int erflown, int nb)
{
	vwsnd_port_t *rport = &devc->rport;
	const int hwmax  = rport->hwbuf_max;
	const int hwsize = rport->hwbuf_size;
	const int swsize = rport->swbuf_size;
	const int fragsize = rport->hw_fragsize;
	unsigned long iflags;

	DBGEV("(devc=0x%p, erflown=%d, nb=%d)\n", devc, erflown, nb);

	spin_lock_irqsave(&rport->lock, iflags);
	if (erflown)
		rport->flags |= ERFLOWN;
	(void) __swb_inc_u(rport, nb);
	if (rport->flags & HW_BUSY || !rport->swbuf) {
		spin_unlock_irqrestore(&rport->lock, iflags);
		DBGPV("returning: HW BUSY or !swbuf\n");
		return;
	}
	if (rport->flags & DISABLED) {
		spin_unlock_irqrestore(&rport->lock, iflags);
		DBGPV("returning: DISABLED\n");
		return;
	}
	rport->flags |= HW_BUSY;
	while (1) {
		int swptr, hwptr, hw_avail, sw_avail, swidx;
		vwsnd_port_hwstate_t hwstate = rport->hwstate;
		vwsnd_port_swstate_t swstate = rport->swstate;

		hwptr = li_read_hwptr(&rport->chan);
		swptr = li_read_swptr(&rport->chan);
		hw_avail = (hwptr - swptr + hwsize) % hwsize & -fragsize;
		if (hw_avail > hwmax)
			hw_avail = hwmax;
		sw_avail = rport->swb_i_avail & -fragsize;
		if (swstate != SW_RUN) {
			DBGP("stopping.  hwstate = %d\n", hwstate);
			if (hwstate != HW_STOPPED) {
				li_deactivate_dma(&rport->chan);
				rport->hwstate = HW_STOPPED;
			}
			wake_up(&rport->queue);
			break;
		}
		if (!sw_avail || !hw_avail)
			break;
		spin_unlock_irqrestore(&rport->lock, iflags);


		swidx = rport->swb_i_idx;
		nb = hw_avail;
		if (nb > sw_avail)
			nb = sw_avail;
		if (nb > hwsize - swptr)
			nb = hwsize - swptr; 
		if (nb > swsize - swidx)
			nb = swsize - swidx; 
		ASSERT(nb > 0);
		if (nb % fragsize) {
			DBGP("nb = %d, fragsize = %d\n", nb, fragsize);
			DBGP("hw_avail = %d\n", hw_avail);
			DBGP("sw_avail = %d\n", sw_avail);
			DBGP("hwsize = %d, swptr = %d\n", hwsize, swptr);
			DBGP("swsize = %d, swidx = %d\n", swsize, swidx);
		}
		ASSERT(!(nb % fragsize));
		DBGPV("copying hwb[%d..%d] to swb[%d..%d]\n",
		      swptr, swptr + nb, swidx, swidx + nb);
		pcm_copy_in(rport, swidx, swptr, nb);
		li_write_swptr(&rport->chan, (swptr + nb) % hwsize);
		spin_lock_irqsave(&rport->lock, iflags);
		__swb_inc_i(rport, nb);
		rport->byte_count += nb;
		rport->frag_count += nb / fragsize;
		ASSERT(nb % fragsize == 0);
		wake_up(&rport->queue);
	}
	rport->flags &= ~HW_BUSY;
	spin_unlock_irqrestore(&rport->lock, iflags);
	DBGRV();
}


static void pcm_flush_frag(vwsnd_dev_t *devc)
{
	vwsnd_port_t *wport = &devc->wport;

	DBGPV("swstate = %d\n", wport->swstate);
	if (wport->swstate == SW_RUN) {
		int idx = wport->swb_u_idx;
		int end = (idx + wport->hw_fragsize - 1)
			>> wport->hw_fragshift
			<< wport->hw_fragshift;
		int nb = end - idx;
		DBGPV("clearing %d bytes\n", nb);
		if (nb)
			memset(wport->swbuf + idx,
			       (char) wport->zero_word,
			       nb);
		wport->swstate = SW_DRAIN;
		pcm_output(devc, 0, nb);
	}
	DBGRV();
}


static void pcm_write_sync(vwsnd_dev_t *devc)
{
	vwsnd_port_t *wport = &devc->wport;
	DECLARE_WAITQUEUE(wait, current);
	unsigned long flags;
	vwsnd_port_hwstate_t hwstate;

	DBGEV("(devc=0x%p)\n", devc);
	add_wait_queue(&wport->queue, &wait);
	while (1) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		spin_lock_irqsave(&wport->lock, flags);
		{
			hwstate = wport->hwstate;
		}
		spin_unlock_irqrestore(&wport->lock, flags);
		if (hwstate == HW_STOPPED)
			break;
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&wport->queue, &wait);
	DBGPV("swstate = %d, hwstate = %d\n", wport->swstate, wport->hwstate);
	DBGRV();
}



static void vwsnd_audio_read_intr(vwsnd_dev_t *devc, unsigned int status)
{
	int overflown = status & LI_INTR_COMM1_OVERFLOW;

	if (status & READ_INTR_MASK)
		pcm_input(devc, overflown, 0);
}

static void vwsnd_audio_write_intr(vwsnd_dev_t *devc, unsigned int status)
{
	int underflown = status & LI_INTR_COMM2_UNDERFLOW;

	if (status & WRITE_INTR_MASK)
		pcm_output(devc, underflown, 0);
}

static irqreturn_t vwsnd_audio_intr(int irq, void *dev_id)
{
	vwsnd_dev_t *devc = dev_id;
	unsigned int status;

	DBGEV("(irq=%d, dev_id=0x%p)\n", irq, dev_id);

	status = li_get_clear_intr_status(&devc->lith);
	vwsnd_audio_read_intr(devc, status);
	vwsnd_audio_write_intr(devc, status);
	return IRQ_HANDLED;
}

static ssize_t vwsnd_audio_do_read(struct file *file,
				   char *buffer,
				   size_t count,
				   loff_t *ppos)
{
	vwsnd_dev_t *devc = file->private_data;
	vwsnd_port_t *rport = ((file->f_mode & FMODE_READ) ?
			       &devc->rport : NULL);
	int ret, nb;

	DBGEV("(file=0x%p, buffer=0x%p, count=%d, ppos=0x%p)\n",
	     file, buffer, count, ppos);

	if (!rport)
		return -EINVAL;

	if (rport->swbuf == NULL) {
		vwsnd_port_t *wport = (file->f_mode & FMODE_WRITE) ?
			&devc->wport : NULL;
		ret = pcm_setup(devc, rport, wport);
		if (ret < 0)
			return ret;
	}

	if (!access_ok(VERIFY_READ, buffer, count))
		return -EFAULT;
	ret = 0;
	while (count) {
		DECLARE_WAITQUEUE(wait, current);
		add_wait_queue(&rport->queue, &wait);
		while ((nb = swb_inc_u(rport, 0)) == 0) {
			DBGPV("blocking\n");
			set_current_state(TASK_INTERRUPTIBLE);
			if (rport->flags & DISABLED ||
			    file->f_flags & O_NONBLOCK) {
				current->state = TASK_RUNNING;
				remove_wait_queue(&rport->queue, &wait);
				return ret ? ret : -EAGAIN;
			}
			schedule();
			if (signal_pending(current)) {
				current->state = TASK_RUNNING;
				remove_wait_queue(&rport->queue, &wait);
				return ret ? ret : -ERESTARTSYS;
			}
		}
		current->state = TASK_RUNNING;
		remove_wait_queue(&rport->queue, &wait);
		pcm_input(devc, 0, 0);
		
		if (nb > count)
			nb = count;
		DBGPV("nb = %d\n", nb);
		if (copy_to_user(buffer, rport->swbuf + rport->swb_u_idx, nb))
			return -EFAULT;
		(void) swb_inc_u(rport, nb);
		buffer += nb;
		count -= nb;
		ret += nb;
	}
	DBGPV("returning %d\n", ret);
	return ret;
}

static ssize_t vwsnd_audio_read(struct file *file,
				char *buffer,
				size_t count,
				loff_t *ppos)
{
	vwsnd_dev_t *devc = file->private_data;
	ssize_t ret;

	mutex_lock(&devc->io_mutex);
	ret = vwsnd_audio_do_read(file, buffer, count, ppos);
	mutex_unlock(&devc->io_mutex);
	return ret;
}

static ssize_t vwsnd_audio_do_write(struct file *file,
				    const char *buffer,
				    size_t count,
				    loff_t *ppos)
{
	vwsnd_dev_t *devc = file->private_data;
	vwsnd_port_t *wport = ((file->f_mode & FMODE_WRITE) ?
			       &devc->wport : NULL);
	int ret, nb;

	DBGEV("(file=0x%p, buffer=0x%p, count=%d, ppos=0x%p)\n",
	      file, buffer, count, ppos);

	if (!wport)
		return -EINVAL;

	if (wport->swbuf == NULL) {
		vwsnd_port_t *rport = (file->f_mode & FMODE_READ) ?
			&devc->rport : NULL;
		ret = pcm_setup(devc, rport, wport);
		if (ret < 0)
			return ret;
	}
	if (!access_ok(VERIFY_WRITE, buffer, count))
		return -EFAULT;
	ret = 0;
	while (count) {
		DECLARE_WAITQUEUE(wait, current);
		add_wait_queue(&wport->queue, &wait);
		while ((nb = swb_inc_u(wport, 0)) == 0) {
			set_current_state(TASK_INTERRUPTIBLE);
			if (wport->flags & DISABLED ||
			    file->f_flags & O_NONBLOCK) {
				current->state = TASK_RUNNING;
				remove_wait_queue(&wport->queue, &wait);
				return ret ? ret : -EAGAIN;
			}
			schedule();
			if (signal_pending(current)) {
				current->state = TASK_RUNNING;
				remove_wait_queue(&wport->queue, &wait);
				return ret ? ret : -ERESTARTSYS;
			}
		}
		current->state = TASK_RUNNING;
		remove_wait_queue(&wport->queue, &wait);
		
		if (nb > count)
			nb = count;
		DBGPV("nb = %d\n", nb);
		if (copy_from_user(wport->swbuf + wport->swb_u_idx, buffer, nb))
			return -EFAULT;
		pcm_output(devc, 0, nb);
		buffer += nb;
		count -= nb;
		ret += nb;
	}
	DBGPV("returning %d\n", ret);
	return ret;
}

static ssize_t vwsnd_audio_write(struct file *file,
				 const char *buffer,
				 size_t count,
				 loff_t *ppos)
{
	vwsnd_dev_t *devc = file->private_data;
	ssize_t ret;

	mutex_lock(&devc->io_mutex);
	ret = vwsnd_audio_do_write(file, buffer, count, ppos);
	mutex_unlock(&devc->io_mutex);
	return ret;
}

static unsigned int vwsnd_audio_poll(struct file *file,
				     struct poll_table_struct *wait)
{
	vwsnd_dev_t *devc = (vwsnd_dev_t *) file->private_data;
	vwsnd_port_t *rport = (file->f_mode & FMODE_READ) ?
		&devc->rport : NULL;
	vwsnd_port_t *wport = (file->f_mode & FMODE_WRITE) ?
		&devc->wport : NULL;
	unsigned int mask = 0;

	DBGEV("(file=0x%p, wait=0x%p)\n", file, wait);

	ASSERT(rport || wport);
	if (rport) {
		poll_wait(file, &rport->queue, wait);
		if (swb_inc_u(rport, 0))
			mask |= (POLLIN | POLLRDNORM);
	}
	if (wport) {
		poll_wait(file, &wport->queue, wait);
		if (wport->swbuf == NULL || swb_inc_u(wport, 0))
			mask |= (POLLOUT | POLLWRNORM);
	}

	DBGPV("returning 0x%x\n", mask);
	return mask;
}

static int vwsnd_audio_do_ioctl(struct file *file,
				unsigned int cmd,
				unsigned long arg)
{
	vwsnd_dev_t *devc = (vwsnd_dev_t *) file->private_data;
	vwsnd_port_t *rport = (file->f_mode & FMODE_READ) ?
		&devc->rport : NULL;
	vwsnd_port_t *wport = (file->f_mode & FMODE_WRITE) ?
		&devc->wport : NULL;
	vwsnd_port_t *aport = rport ? rport : wport;
	struct audio_buf_info buf_info;
	struct count_info info;
	unsigned long flags;
	int ival;

	
	DBGEV("(file=0x%p, cmd=0x%x, arg=0x%lx)\n",
	      file, cmd, arg);
	switch (cmd) {
	case OSS_GETVERSION:		
		DBGX("OSS_GETVERSION\n");
		ival = SOUND_VERSION;
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_GETCAPS:	
		DBGX("SNDCTL_DSP_GETCAPS\n");
		ival = DSP_CAP_DUPLEX | DSP_CAP_REALTIME | DSP_CAP_TRIGGER;
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_GETFMTS:	
		DBGX("SNDCTL_DSP_GETFMTS\n");
		ival = (AFMT_S16_LE | AFMT_MU_LAW | AFMT_A_LAW |
			AFMT_U8 | AFMT_S8);
		return put_user(ival, (int *) arg);
		break;

	case SOUND_PCM_READ_RATE:	
		DBGX("SOUND_PCM_READ_RATE\n");
		ival = aport->sw_framerate;
		return put_user(ival, (int *) arg);

	case SOUND_PCM_READ_CHANNELS:	
		DBGX("SOUND_PCM_READ_CHANNELS\n");
		ival = aport->sw_channels;
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_SPEED:		
		if (get_user(ival, (int *) arg))
			return -EFAULT;
		DBGX("SNDCTL_DSP_SPEED %d\n", ival);
		if (ival) {
			if (aport->swstate != SW_INITIAL) {
				DBGX("SNDCTL_DSP_SPEED failed: swstate = %d\n",
				     aport->swstate);
				return -EINVAL;
			}
			if (ival < MIN_SPEED)
				ival = MIN_SPEED;
			if (ival > MAX_SPEED)
				ival = MAX_SPEED;
			if (rport)
				rport->sw_framerate = ival;
			if (wport)
				wport->sw_framerate = ival;
		} else
			ival = aport->sw_framerate;
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_STEREO:		
		if (get_user(ival, (int *) arg))
			return -EFAULT;
		DBGX("SNDCTL_DSP_STEREO %d\n", ival);
		if (ival != 0 && ival != 1)
			return -EINVAL;
		if (aport->swstate != SW_INITIAL)
			return -EINVAL;
		if (rport)
			rport->sw_channels = ival + 1;
		if (wport)
			wport->sw_channels = ival + 1;
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_CHANNELS:	
		if (get_user(ival, (int *) arg))
			return -EFAULT;
		DBGX("SNDCTL_DSP_CHANNELS %d\n", ival);
		if (ival != 1 && ival != 2)
			return -EINVAL;
		if (aport->swstate != SW_INITIAL)
			return -EINVAL;
		if (rport)
			rport->sw_channels = ival;
		if (wport)
			wport->sw_channels = ival;
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_GETBLKSIZE:	
		ival = pcm_setup(devc, rport, wport);
		if (ival < 0) {
			DBGX("SNDCTL_DSP_GETBLKSIZE failed, errno %d\n", ival);
			return ival;
		}
		ival = 1 << aport->sw_fragshift;
		DBGX("SNDCTL_DSP_GETBLKSIZE returning %d\n", ival);
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_SETFRAGMENT:	
		if (get_user(ival, (int *) arg))
			return -EFAULT;
		DBGX("SNDCTL_DSP_SETFRAGMENT %d:%d\n",
		     ival >> 16, ival & 0xFFFF);
		if (aport->swstate != SW_INITIAL)
			return -EINVAL;
		{
			int sw_fragshift = ival & 0xFFFF;
			int sw_subdivshift = aport->sw_subdivshift;
			int hw_fragshift = sw_fragshift - sw_subdivshift;
			int sw_fragcount = (ival >> 16) & 0xFFFF;
			int hw_fragsize;
			if (hw_fragshift < MIN_FRAGSHIFT)
				hw_fragshift = MIN_FRAGSHIFT;
			if (hw_fragshift > MAX_FRAGSHIFT)
				hw_fragshift = MAX_FRAGSHIFT;
			sw_fragshift = hw_fragshift + aport->sw_subdivshift;
			hw_fragsize = 1 << hw_fragshift;
			if (sw_fragcount < MIN_FRAGCOUNT(hw_fragsize))
				sw_fragcount = MIN_FRAGCOUNT(hw_fragsize);
			if (sw_fragcount > MAX_FRAGCOUNT(hw_fragsize))
				sw_fragcount = MAX_FRAGCOUNT(hw_fragsize);
			DBGPV("sw_fragshift = %d\n", sw_fragshift);
			DBGPV("rport = 0x%p, wport = 0x%p\n", rport, wport);
			if (rport) {
				rport->sw_fragshift = sw_fragshift;
				rport->sw_fragcount = sw_fragcount;
			}
			if (wport) {
				wport->sw_fragshift = sw_fragshift;
				wport->sw_fragcount = sw_fragcount;
			}
			ival = sw_fragcount << 16 | sw_fragshift;
		}
		DBGX("SNDCTL_DSP_SETFRAGMENT returns %d:%d\n",
		      ival >> 16, ival & 0xFFFF);
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_SUBDIVIDE:	
                if (get_user(ival, (int *) arg))
			return -EFAULT;
		DBGX("SNDCTL_DSP_SUBDIVIDE %d\n", ival);
		if (aport->swstate != SW_INITIAL)
			return -EINVAL;
		{
			int subdivshift;
			int hw_fragshift, hw_fragsize, hw_fragcount;
			switch (ival) {
			case 1: subdivshift = 0; break;
			case 2: subdivshift = 1; break;
			case 4: subdivshift = 2; break;
			default: return -EINVAL;
			}
			hw_fragshift = aport->sw_fragshift - subdivshift;
			if (hw_fragshift < MIN_FRAGSHIFT ||
			    hw_fragshift > MAX_FRAGSHIFT)
				return -EINVAL;
			hw_fragsize = 1 << hw_fragshift;
			hw_fragcount = aport->sw_fragcount >> subdivshift;
			if (hw_fragcount < MIN_FRAGCOUNT(hw_fragsize) ||
			    hw_fragcount > MAX_FRAGCOUNT(hw_fragsize))
				return -EINVAL;
			if (rport)
				rport->sw_subdivshift = subdivshift;
			if (wport)
				wport->sw_subdivshift = subdivshift;
		}
		return 0;

	case SNDCTL_DSP_SETFMT:		
		if (get_user(ival, (int *) arg))
			return -EFAULT;
		DBGX("SNDCTL_DSP_SETFMT %d\n", ival);
		if (ival != AFMT_QUERY) {
			if (aport->swstate != SW_INITIAL) {
				DBGP("SETFMT failed, swstate = %d\n",
				     aport->swstate);
				return -EINVAL;
			}
			switch (ival) {
			case AFMT_MU_LAW:
			case AFMT_A_LAW:
			case AFMT_U8:
			case AFMT_S8:
			case AFMT_S16_LE:
				if (rport)
					rport->sw_samplefmt = ival;
				if (wport)
					wport->sw_samplefmt = ival;
				break;
			default:
				return -EINVAL;
			}
		}
		ival = aport->sw_samplefmt;
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_GETOSPACE:	
		DBGXV("SNDCTL_DSP_GETOSPACE\n");
		if (!wport)
			return -EINVAL;
		ival = pcm_setup(devc, rport, wport);
		if (ival < 0)
			return ival;
		ival = swb_inc_u(wport, 0);
		buf_info.fragments = ival >> wport->sw_fragshift;
		buf_info.fragstotal = wport->sw_fragcount;
		buf_info.fragsize = 1 << wport->sw_fragshift;
		buf_info.bytes = ival;
		DBGXV("SNDCTL_DSP_GETOSPACE returns { %d %d %d %d }\n",
		     buf_info.fragments, buf_info.fragstotal,
		     buf_info.fragsize, buf_info.bytes);
		if (copy_to_user((void *) arg, &buf_info, sizeof buf_info))
			return -EFAULT;
		return 0;

	case SNDCTL_DSP_GETISPACE:	
		DBGX("SNDCTL_DSP_GETISPACE\n");
		if (!rport)
			return -EINVAL;
		ival = pcm_setup(devc, rport, wport);
		if (ival < 0)
			return ival;
		ival = swb_inc_u(rport, 0);
		buf_info.fragments = ival >> rport->sw_fragshift;
		buf_info.fragstotal = rport->sw_fragcount;
		buf_info.fragsize = 1 << rport->sw_fragshift;
		buf_info.bytes = ival;
		DBGX("SNDCTL_DSP_GETISPACE returns { %d %d %d %d }\n",
		     buf_info.fragments, buf_info.fragstotal,
		     buf_info.fragsize, buf_info.bytes);
		if (copy_to_user((void *) arg, &buf_info, sizeof buf_info))
			return -EFAULT;
		return 0;

	case SNDCTL_DSP_NONBLOCK:	
		DBGX("SNDCTL_DSP_NONBLOCK\n");
		spin_lock(&file->f_lock);
		file->f_flags |= O_NONBLOCK;
		spin_unlock(&file->f_lock);
		return 0;

	case SNDCTL_DSP_RESET:		
		DBGX("SNDCTL_DSP_RESET\n");
		if (wport && wport->swbuf) {
			wport->swstate = SW_INITIAL;
			pcm_output(devc, 0, 0);
			pcm_write_sync(devc);
		}
		pcm_shutdown(devc, rport, wport);
		return 0;

	case SNDCTL_DSP_SYNC:		
		DBGX("SNDCTL_DSP_SYNC\n");
		if (wport) {
			pcm_flush_frag(devc);
			pcm_write_sync(devc);
		}
		pcm_shutdown(devc, rport, wport);
		return 0;

	case SNDCTL_DSP_POST:		
		DBGX("SNDCTL_DSP_POST\n");
		if (!wport)
			return -EINVAL;
		pcm_flush_frag(devc);
		return 0;

	case SNDCTL_DSP_GETIPTR:	
		DBGX("SNDCTL_DSP_GETIPTR\n");
		if (!rport)
			return -EINVAL;
		spin_lock_irqsave(&rport->lock, flags);
		{
			ustmsc_t ustmsc;
			if (rport->hwstate == HW_RUNNING) {
				ASSERT(rport->swstate == SW_RUN);
				li_read_USTMSC(&rport->chan, &ustmsc);
				info.bytes = ustmsc.msc - rport->MSC_offset;
				info.bytes *= rport->frame_size;
			} else {
				info.bytes = rport->byte_count;
			}
			info.blocks = rport->frag_count;
			info.ptr = 0;	
			rport->frag_count = 0;
		}
		spin_unlock_irqrestore(&rport->lock, flags);
		if (copy_to_user((void *) arg, &info, sizeof info))
			return -EFAULT;
		return 0;

	case SNDCTL_DSP_GETOPTR:	
		DBGX("SNDCTL_DSP_GETOPTR\n");
		if (!wport)
			return -EINVAL;
		spin_lock_irqsave(&wport->lock, flags);
		{
			ustmsc_t ustmsc;
			if (wport->hwstate == HW_RUNNING) {
				ASSERT(wport->swstate == SW_RUN);
				li_read_USTMSC(&wport->chan, &ustmsc);
				info.bytes = ustmsc.msc - wport->MSC_offset;
				info.bytes *= wport->frame_size;
			} else {
				info.bytes = wport->byte_count;
			}
			info.blocks = wport->frag_count;
			info.ptr = 0;	
			wport->frag_count = 0;
		}
		spin_unlock_irqrestore(&wport->lock, flags);
		if (copy_to_user((void *) arg, &info, sizeof info))
			return -EFAULT;
		return 0;

	case SNDCTL_DSP_GETODELAY:	
		DBGX("SNDCTL_DSP_GETODELAY\n");
		if (!wport)
			return -EINVAL;
		spin_lock_irqsave(&wport->lock, flags);
		{
			int fsize = wport->frame_size;
			ival = wport->swb_i_avail / fsize;
			if (wport->hwstate == HW_RUNNING) {
				int swptr, hwptr, hwframes, hwbytes, hwsize;
				int totalhwbytes;
				ustmsc_t ustmsc;

				hwsize = wport->hwbuf_size;
				swptr = li_read_swptr(&wport->chan);
				li_read_USTMSC(&wport->chan, &ustmsc);
				hwframes = ustmsc.msc - wport->MSC_offset;
				totalhwbytes = hwframes * fsize;
				hwptr = totalhwbytes % hwsize;
				hwbytes = (swptr - hwptr + hwsize) % hwsize;
				ival += hwbytes / fsize;
			}
		}
		spin_unlock_irqrestore(&wport->lock, flags);
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_PROFILE:	
		DBGX("SNDCTL_DSP_PROFILE\n");


		break;

	case SNDCTL_DSP_GETTRIGGER:	
		DBGX("SNDCTL_DSP_GETTRIGGER\n");
		ival = 0;
		if (rport) {
			spin_lock_irqsave(&rport->lock, flags);
			{
				if (!(rport->flags & DISABLED))
					ival |= PCM_ENABLE_INPUT;
			}
			spin_unlock_irqrestore(&rport->lock, flags);
		}
		if (wport) {
			spin_lock_irqsave(&wport->lock, flags);
			{
				if (!(wport->flags & DISABLED))
					ival |= PCM_ENABLE_OUTPUT;
			}
			spin_unlock_irqrestore(&wport->lock, flags);
		}
		return put_user(ival, (int *) arg);

	case SNDCTL_DSP_SETTRIGGER:	
		if (get_user(ival, (int *) arg))
			return -EFAULT;
		DBGX("SNDCTL_DSP_SETTRIGGER %d\n", ival);


		if (((rport && !(ival & PCM_ENABLE_INPUT)) ||
		     (wport && !(ival & PCM_ENABLE_OUTPUT))) &&
		    aport->swstate != SW_INITIAL)
			return -EINVAL;

		if (rport) {
			vwsnd_port_hwstate_t hwstate;
			spin_lock_irqsave(&rport->lock, flags);
			{
				hwstate = rport->hwstate;
				if (ival & PCM_ENABLE_INPUT)
					rport->flags &= ~DISABLED;
				else
					rport->flags |= DISABLED;
			}
			spin_unlock_irqrestore(&rport->lock, flags);
			if (hwstate != HW_RUNNING && ival & PCM_ENABLE_INPUT) {

				if (rport->swstate == SW_INITIAL)
					pcm_setup(devc, rport, wport);
				else
					li_activate_dma(&rport->chan);
			}
		}
		if (wport) {
			vwsnd_port_flags_t pflags;
			spin_lock_irqsave(&wport->lock, flags);
			{
				pflags = wport->flags;
				if (ival & PCM_ENABLE_OUTPUT)
					wport->flags &= ~DISABLED;
				else
					wport->flags |= DISABLED;
			}
			spin_unlock_irqrestore(&wport->lock, flags);
			if (pflags & DISABLED && ival & PCM_ENABLE_OUTPUT) {
				if (wport->swstate == SW_RUN)
					pcm_output(devc, 0, 0);
			}
		}
		return 0;

	default:
		DBGP("unknown ioctl 0x%x\n", cmd);
		return -EINVAL;
	}
	DBGP("unimplemented ioctl 0x%x\n", cmd);
	return -EINVAL;
}

static long vwsnd_audio_ioctl(struct file *file,
				unsigned int cmd,
				unsigned long arg)
{
	vwsnd_dev_t *devc = (vwsnd_dev_t *) file->private_data;
	int ret;

	mutex_lock(&vwsnd_mutex);
	mutex_lock(&devc->io_mutex);
	ret = vwsnd_audio_do_ioctl(file, cmd, arg);
	mutex_unlock(&devc->io_mutex);
	mutex_unlock(&vwsnd_mutex);

	return ret;
}


static int vwsnd_audio_mmap(struct file *file, struct vm_area_struct *vma)
{
	DBGE("(file=0x%p, vma=0x%p)\n", file, vma);
	return -ENODEV;
}


static int vwsnd_audio_open(struct inode *inode, struct file *file)
{
	vwsnd_dev_t *devc;
	int minor = iminor(inode);
	int sw_samplefmt;

	DBGE("(inode=0x%p, file=0x%p)\n", inode, file);

	mutex_lock(&vwsnd_mutex);
	INC_USE_COUNT;
	for (devc = vwsnd_dev_list; devc; devc = devc->next_dev)
		if ((devc->audio_minor & ~0x0F) == (minor & ~0x0F))
			break;

	if (devc == NULL) {
		DEC_USE_COUNT;
		mutex_unlock(&vwsnd_mutex);
		return -ENODEV;
	}

	mutex_lock(&devc->open_mutex);
	while (devc->open_mode & file->f_mode) {
		mutex_unlock(&devc->open_mutex);
		if (file->f_flags & O_NONBLOCK) {
			DEC_USE_COUNT;
			mutex_unlock(&vwsnd_mutex);
			return -EBUSY;
		}
		interruptible_sleep_on(&devc->open_wait);
		if (signal_pending(current)) {
			DEC_USE_COUNT;
			mutex_unlock(&vwsnd_mutex);
			return -ERESTARTSYS;
		}
		mutex_lock(&devc->open_mutex);
	}
	devc->open_mode |= file->f_mode & (FMODE_READ | FMODE_WRITE);
	mutex_unlock(&devc->open_mutex);

	

	sw_samplefmt = 0;
	if ((minor & 0xF) == SND_DEV_DSP)
		sw_samplefmt = AFMT_U8;
	else if ((minor & 0xF) == SND_DEV_AUDIO)
		sw_samplefmt = AFMT_MU_LAW;
	else if ((minor & 0xF) == SND_DEV_DSP16)
		sw_samplefmt = AFMT_S16_LE;
	else
		ASSERT(0);

	

	mutex_lock(&devc->io_mutex);
	{
		if (file->f_mode & FMODE_READ) {
			devc->rport.swstate        = SW_INITIAL;
			devc->rport.flags          = 0;
			devc->rport.sw_channels    = 1;
			devc->rport.sw_samplefmt   = sw_samplefmt;
			devc->rport.sw_framerate   = 8000;
			devc->rport.sw_fragshift   = DEFAULT_FRAGSHIFT;
			devc->rport.sw_fragcount   = DEFAULT_FRAGCOUNT;
			devc->rport.sw_subdivshift = DEFAULT_SUBDIVSHIFT;
			devc->rport.byte_count     = 0;
			devc->rport.frag_count     = 0;
		}
		if (file->f_mode & FMODE_WRITE) {
			devc->wport.swstate        = SW_INITIAL;
			devc->wport.flags          = 0;
			devc->wport.sw_channels    = 1;
			devc->wport.sw_samplefmt   = sw_samplefmt;
			devc->wport.sw_framerate   = 8000;
			devc->wport.sw_fragshift   = DEFAULT_FRAGSHIFT;
			devc->wport.sw_fragcount   = DEFAULT_FRAGCOUNT;
			devc->wport.sw_subdivshift = DEFAULT_SUBDIVSHIFT;
			devc->wport.byte_count     = 0;
			devc->wport.frag_count     = 0;
		}
	}
	mutex_unlock(&devc->io_mutex);

	file->private_data = devc;
	DBGRV();
	mutex_unlock(&vwsnd_mutex);
	return 0;
}


static int vwsnd_audio_release(struct inode *inode, struct file *file)
{
	vwsnd_dev_t *devc = (vwsnd_dev_t *) file->private_data;
	vwsnd_port_t *wport = NULL, *rport = NULL;
	int err = 0;

	mutex_lock(&vwsnd_mutex);
	mutex_lock(&devc->io_mutex);
	{
		DBGEV("(inode=0x%p, file=0x%p)\n", inode, file);

		if (file->f_mode & FMODE_READ)
			rport = &devc->rport;
		if (file->f_mode & FMODE_WRITE) {
			wport = &devc->wport;
			pcm_flush_frag(devc);
			pcm_write_sync(devc);
		}
		pcm_shutdown(devc, rport, wport);
		if (rport)
			rport->swstate = SW_OFF;
		if (wport)
			wport->swstate = SW_OFF;
	}
	mutex_unlock(&devc->io_mutex);

	mutex_lock(&devc->open_mutex);
	{
		devc->open_mode &= ~file->f_mode;
	}
	mutex_unlock(&devc->open_mutex);
	wake_up(&devc->open_wait);
	DEC_USE_COUNT;
	DBGR();
	mutex_unlock(&vwsnd_mutex);
	return err;
}

static const struct file_operations vwsnd_audio_fops = {
	.owner =	THIS_MODULE,
	.llseek =	no_llseek,
	.read =		vwsnd_audio_read,
	.write =	vwsnd_audio_write,
	.poll =		vwsnd_audio_poll,
	.unlocked_ioctl = vwsnd_audio_ioctl,
	.mmap =		vwsnd_audio_mmap,
	.open =		vwsnd_audio_open,
	.release =	vwsnd_audio_release,
};



static int vwsnd_mixer_open(struct inode *inode, struct file *file)
{
	vwsnd_dev_t *devc;

	DBGEV("(inode=0x%p, file=0x%p)\n", inode, file);

	INC_USE_COUNT;
	mutex_lock(&vwsnd_mutex);
	for (devc = vwsnd_dev_list; devc; devc = devc->next_dev)
		if (devc->mixer_minor == iminor(inode))
			break;

	if (devc == NULL) {
		DEC_USE_COUNT;
		mutex_unlock(&vwsnd_mutex);
		return -ENODEV;
	}
	file->private_data = devc;
	mutex_unlock(&vwsnd_mutex);
	return 0;
}


static int vwsnd_mixer_release(struct inode *inode, struct file *file)
{
	DBGEV("(inode=0x%p, file=0x%p)\n", inode, file);
	DEC_USE_COUNT;
	return 0;
}


static int mixer_read_ioctl(vwsnd_dev_t *devc, unsigned int nr, void __user *arg)
{
	int val = -1;

	DBGEV("(devc=0x%p, nr=0x%x, arg=0x%p)\n", devc, nr, arg);

	switch (nr) {
	case SOUND_MIXER_CAPS:
		val = SOUND_CAP_EXCL_INPUT;
		break;

	case SOUND_MIXER_DEVMASK:
		val = (SOUND_MASK_PCM | SOUND_MASK_LINE |
		       SOUND_MASK_MIC | SOUND_MASK_CD | SOUND_MASK_RECLEV);
		break;

	case SOUND_MIXER_STEREODEVS:
		val = (SOUND_MASK_PCM | SOUND_MASK_LINE |
		       SOUND_MASK_MIC | SOUND_MASK_CD | SOUND_MASK_RECLEV);
		break;

	case SOUND_MIXER_OUTMASK:
		val = (SOUND_MASK_PCM | SOUND_MASK_LINE |
		       SOUND_MASK_MIC | SOUND_MASK_CD);
		break;

	case SOUND_MIXER_RECMASK:
		val = (SOUND_MASK_PCM | SOUND_MASK_LINE |
		       SOUND_MASK_MIC | SOUND_MASK_CD);
		break;

	case SOUND_MIXER_PCM:
		val = ad1843_get_gain(&devc->lith, &ad1843_gain_PCM);
		break;

	case SOUND_MIXER_LINE:
		val = ad1843_get_gain(&devc->lith, &ad1843_gain_LINE);
		break;

	case SOUND_MIXER_MIC:
		val = ad1843_get_gain(&devc->lith, &ad1843_gain_MIC);
		break;

	case SOUND_MIXER_CD:
		val = ad1843_get_gain(&devc->lith, &ad1843_gain_CD);
		break;

	case SOUND_MIXER_RECLEV:
		val = ad1843_get_gain(&devc->lith, &ad1843_gain_RECLEV);
		break;

	case SOUND_MIXER_RECSRC:
		val = ad1843_get_recsrc(&devc->lith);
		break;

	case SOUND_MIXER_OUTSRC:
		val = ad1843_get_outsrc(&devc->lith);
		break;

	default:
		return -EINVAL;
	}
	return put_user(val, (int __user *) arg);
}


static int mixer_write_ioctl(vwsnd_dev_t *devc, unsigned int nr, void __user *arg)
{
	int val;
	int err;

	DBGEV("(devc=0x%p, nr=0x%x, arg=0x%p)\n", devc, nr, arg);

	err = get_user(val, (int __user *) arg);
	if (err)
		return -EFAULT;
	switch (nr) {
	case SOUND_MIXER_PCM:
		val = ad1843_set_gain(&devc->lith, &ad1843_gain_PCM, val);
		break;

	case SOUND_MIXER_LINE:
		val = ad1843_set_gain(&devc->lith, &ad1843_gain_LINE, val);
		break;

	case SOUND_MIXER_MIC:
		val = ad1843_set_gain(&devc->lith, &ad1843_gain_MIC, val);
		break;

	case SOUND_MIXER_CD:
		val = ad1843_set_gain(&devc->lith, &ad1843_gain_CD, val);
		break;

	case SOUND_MIXER_RECLEV:
		val = ad1843_set_gain(&devc->lith, &ad1843_gain_RECLEV, val);
		break;

	case SOUND_MIXER_RECSRC:
		if (devc->rport.swbuf || devc->wport.swbuf)
			return -EBUSY;	
		val = ad1843_set_recsrc(&devc->lith, val);
		break;

	case SOUND_MIXER_OUTSRC:
		val = ad1843_set_outsrc(&devc->lith, val);
		break;

	default:
		return -EINVAL;
	}
	if (val < 0)
		return val;
	return put_user(val, (int __user *) arg);
}


static long vwsnd_mixer_ioctl(struct file *file,
			      unsigned int cmd,
			      unsigned long arg)
{
	vwsnd_dev_t *devc = (vwsnd_dev_t *) file->private_data;
	const unsigned int nrmask = _IOC_NRMASK << _IOC_NRSHIFT;
	const unsigned int nr = (cmd & nrmask) >> _IOC_NRSHIFT;
	int retval;

	DBGEV("(devc=0x%p, cmd=0x%x, arg=0x%lx)\n", devc, cmd, arg);

	mutex_lock(&vwsnd_mutex);
	mutex_lock(&devc->mix_mutex);
	{
		if ((cmd & ~nrmask) == MIXER_READ(0))
			retval = mixer_read_ioctl(devc, nr, (void __user *) arg);
		else if ((cmd & ~nrmask) == MIXER_WRITE(0))
			retval = mixer_write_ioctl(devc, nr, (void __user *) arg);
		else
			retval = -EINVAL;
	}
	mutex_unlock(&devc->mix_mutex);
	mutex_unlock(&vwsnd_mutex);
	return retval;
}

static const struct file_operations vwsnd_mixer_fops = {
	.owner =	THIS_MODULE,
	.llseek =	no_llseek,
	.unlocked_ioctl = vwsnd_mixer_ioctl,
	.open =		vwsnd_mixer_open,
	.release =	vwsnd_mixer_release,
};



static int __init probe_vwsnd(struct address_info *hw_config)
{
	lithium_t lith;
	int w;
	unsigned long later;

	DBGEV("(hw_config=0x%p)\n", hw_config);

	

	if (li_create(&lith, hw_config->io_base) != 0) {
		printk(KERN_WARNING "probe_vwsnd: can't map lithium\n");
		return 0;
	}
	later = jiffies + 2;
	li_writel(&lith, LI_HOST_CONTROLLER, LI_HC_LINK_ENABLE);
	do {
		w = li_readl(&lith, LI_HOST_CONTROLLER);
	} while (w == LI_HC_LINK_ENABLE && time_before(jiffies, later));
	
	li_destroy(&lith);

	DBGPV("HC = 0x%04x\n", w);

	if ((w == LI_HC_LINK_ENABLE) || (w & LI_HC_LINK_CODEC)) {


		printk(KERN_WARNING "probe_vwsnd: audio codec not found\n");
		return 0;
	}

	if (w & LI_HC_LINK_FAILURE) {
		printk(KERN_WARNING "probe_vwsnd: can't init audio codec\n");
		return 0;
	}

	printk(KERN_INFO "vwsnd: lithium audio at mmio %#x irq %d\n",
		hw_config->io_base, hw_config->irq);

	return 1;
}


static int __init attach_vwsnd(struct address_info *hw_config)
{
	vwsnd_dev_t *devc = NULL;
	int err = -ENOMEM;

	DBGEV("(hw_config=0x%p)\n", hw_config);

	devc = kmalloc(sizeof (vwsnd_dev_t), GFP_KERNEL);
	if (devc == NULL)
		goto fail0;

	err = li_create(&devc->lith, hw_config->io_base);
	if (err)
		goto fail1;

	init_waitqueue_head(&devc->open_wait);

	devc->rport.hwbuf_size = HWBUF_SIZE;
	devc->rport.hwbuf_vaddr = __get_free_pages(GFP_KERNEL, HWBUF_ORDER);
	if (!devc->rport.hwbuf_vaddr)
		goto fail2;
	devc->rport.hwbuf = (void *) devc->rport.hwbuf_vaddr;
	devc->rport.hwbuf_paddr = virt_to_phys(devc->rport.hwbuf);


	li_writel(&devc->lith, LI_COMM1_BASE,
		  devc->rport.hwbuf_paddr >> 8 | 1 << (37 - 8));

	devc->wport.hwbuf_size = HWBUF_SIZE;
	devc->wport.hwbuf_vaddr = __get_free_pages(GFP_KERNEL, HWBUF_ORDER);
	if (!devc->wport.hwbuf_vaddr)
		goto fail3;
	devc->wport.hwbuf = (void *) devc->wport.hwbuf_vaddr;
	devc->wport.hwbuf_paddr = virt_to_phys(devc->wport.hwbuf);
	DBGP("wport hwbuf = 0x%p\n", devc->wport.hwbuf);

	DBGDO(shut_up++);
	err = ad1843_init(&devc->lith);
	DBGDO(shut_up--);
	if (err)
		goto fail4;

	

	err = request_irq(hw_config->irq, vwsnd_audio_intr, 0, "vwsnd", devc);
	if (err)
		goto fail5;

	

	devc->audio_minor = register_sound_dsp(&vwsnd_audio_fops, -1);
	if ((err = devc->audio_minor) < 0) {
		DBGDO(printk(KERN_WARNING
			     "attach_vwsnd: register_sound_dsp error %d\n",
			     err));
		goto fail6;
	}
	devc->mixer_minor = register_sound_mixer(&vwsnd_mixer_fops,
						 devc->audio_minor >> 4);
	if ((err = devc->mixer_minor) < 0) {
		DBGDO(printk(KERN_WARNING
			     "attach_vwsnd: register_sound_mixer error %d\n",
			     err));
		goto fail7;
	}

	

	hw_config->slots[0] = devc->audio_minor;

	

	mutex_init(&devc->open_mutex);
	mutex_init(&devc->io_mutex);
	mutex_init(&devc->mix_mutex);
	devc->open_mode = 0;
	spin_lock_init(&devc->rport.lock);
	init_waitqueue_head(&devc->rport.queue);
	devc->rport.swstate = SW_OFF;
	devc->rport.hwstate = HW_STOPPED;
	devc->rport.flags = 0;
	devc->rport.swbuf = NULL;
	spin_lock_init(&devc->wport.lock);
	init_waitqueue_head(&devc->wport.queue);
	devc->wport.swstate = SW_OFF;
	devc->wport.hwstate = HW_STOPPED;
	devc->wport.flags = 0;
	devc->wport.swbuf = NULL;

	

	devc->next_dev = vwsnd_dev_list;
	vwsnd_dev_list = devc;
	return devc->audio_minor;

	

 fail7:
	unregister_sound_dsp(devc->audio_minor);
 fail6:
	free_irq(hw_config->irq, devc);
 fail5:
 fail4:
	free_pages(devc->wport.hwbuf_vaddr, HWBUF_ORDER);
 fail3:
	free_pages(devc->rport.hwbuf_vaddr, HWBUF_ORDER);
 fail2:
	li_destroy(&devc->lith);
 fail1:
	kfree(devc);
 fail0:
	return err;
}

static int __exit unload_vwsnd(struct address_info *hw_config)
{
	vwsnd_dev_t *devc, **devcp;

	DBGE("()\n");

	devcp = &vwsnd_dev_list;
	while ((devc = *devcp)) {
		if (devc->audio_minor == hw_config->slots[0]) {
			*devcp = devc->next_dev;
			break;
		}
		devcp = &devc->next_dev;
	}

	if (!devc)
		return -ENODEV;

	unregister_sound_mixer(devc->mixer_minor);
	unregister_sound_dsp(devc->audio_minor);
	free_irq(hw_config->irq, devc);
	free_pages(devc->wport.hwbuf_vaddr, HWBUF_ORDER);
	free_pages(devc->rport.hwbuf_vaddr, HWBUF_ORDER);
	li_destroy(&devc->lith);
	kfree(devc);

	return 0;
}


static struct address_info the_hw_config = {
	0xFF001000,			
	CO_IRQ(CO_APIC_LI_AUDIO)	
};

MODULE_DESCRIPTION("SGI Visual Workstation sound module");
MODULE_AUTHOR("Bob Miller <kbob@sgi.com>");
MODULE_LICENSE("GPL");

static int __init init_vwsnd(void)
{
	int err;

	DBGXV("\n");
	DBGXV("sound::vwsnd::init_module()\n");

	if (!probe_vwsnd(&the_hw_config))
		return -ENODEV;

	err = attach_vwsnd(&the_hw_config);
	if (err < 0)
		return err;
	return 0;
}

static void __exit cleanup_vwsnd(void)
{
	DBGX("sound::vwsnd::cleanup_module()\n");

	unload_vwsnd(&the_hw_config);
}

module_init(init_vwsnd);
module_exit(cleanup_vwsnd);
