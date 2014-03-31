
#include "../comedidev.h"

#include <linux/ioport.h>
#include <linux/mc146818rtc.h>
#include <linux/gfp.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <asm/dma.h>

#include "8253.h"



#define boardPCL818L 0
#define boardPCL818H 1
#define boardPCL818HD 2
#define boardPCL818HG 3
#define boardPCL818 4
#define boardPCL718 5

#define PCLx1x_RANGE 16
#define PCLx1xFIFO_RANGE 32

#define PCL818_CLRINT 8
#define PCL818_STATUS 8
#define PCL818_RANGE 1
#define PCL818_MUX 2
#define PCL818_CONTROL 9
#define PCL818_CNTENABLE 10

#define PCL818_AD_LO 0
#define PCL818_AD_HI 1
#define PCL818_DA_LO 4
#define PCL818_DA_HI 5
#define PCL818_DI_LO 3
#define PCL818_DI_HI 11
#define PCL818_DO_LO 3
#define PCL818_DO_HI 11
#define PCL718_DA2_LO 6
#define PCL718_DA2_HI 7
#define PCL818_CTR0 12
#define PCL818_CTR1 13
#define PCL818_CTR2 14
#define PCL818_CTRCTL 15

#define PCL818_FI_ENABLE 6
#define PCL818_FI_INTCLR 20
#define PCL818_FI_FLUSH 25
#define PCL818_FI_STATUS 25
#define PCL818_FI_DATALO 23
#define PCL818_FI_DATAHI 23

#define INT_TYPE_AI1_INT 1
#define INT_TYPE_AI1_DMA 2
#define INT_TYPE_AI1_FIFO 3
#define INT_TYPE_AI3_INT 4
#define INT_TYPE_AI3_DMA 5
#define INT_TYPE_AI3_FIFO 6
#ifdef PCL818_MODE13_AO
#define INT_TYPE_AO1_INT 7
#define INT_TYPE_AO3_INT 8
#endif

#ifdef unused
#define INT_TYPE_AI1_DMA_RTC 9
#define INT_TYPE_AI3_DMA_RTC 10

#define RTC_IRQ 	8
#define RTC_IO_EXTENT	0x10
#endif

#define MAGIC_DMA_WORD 0x5a5a

static const struct comedi_lrange range_pcl818h_ai = { 9, {
							   BIP_RANGE(5),
							   BIP_RANGE(2.5),
							   BIP_RANGE(1.25),
							   BIP_RANGE(0.625),
							   UNI_RANGE(10),
							   UNI_RANGE(5),
							   UNI_RANGE(2.5),
							   UNI_RANGE(1.25),
							   BIP_RANGE(10),
							   }
};

static const struct comedi_lrange range_pcl818hg_ai = { 10, {
							     BIP_RANGE(5),
							     BIP_RANGE(0.5),
							     BIP_RANGE(0.05),
							     BIP_RANGE(0.005),
							     UNI_RANGE(10),
							     UNI_RANGE(1),
							     UNI_RANGE(0.1),
							     UNI_RANGE(0.01),
							     BIP_RANGE(10),
							     BIP_RANGE(1),
							     BIP_RANGE(0.1),
							     BIP_RANGE(0.01),
							     }
};

static const struct comedi_lrange range_pcl818l_l_ai = { 4, {
							     BIP_RANGE(5),
							     BIP_RANGE(2.5),
							     BIP_RANGE(1.25),
							     BIP_RANGE(0.625),
							     }
};

static const struct comedi_lrange range_pcl818l_h_ai = { 4, {
							     BIP_RANGE(10),
							     BIP_RANGE(5),
							     BIP_RANGE(2.5),
							     BIP_RANGE(1.25),
							     }
};

static const struct comedi_lrange range718_bipolar1 = { 1, {BIP_RANGE(1),} };
static const struct comedi_lrange range718_bipolar0_5 =
    { 1, {BIP_RANGE(0.5),} };
static const struct comedi_lrange range718_unipolar2 = { 1, {UNI_RANGE(2),} };
static const struct comedi_lrange range718_unipolar1 = { 1, {BIP_RANGE(1),} };

static int pcl818_attach(struct comedi_device *dev,
			 struct comedi_devconfig *it);
static int pcl818_detach(struct comedi_device *dev);

#ifdef unused
static int RTC_lock;	
static int RTC_timer_lock;	
#endif

struct pcl818_board {

	const char *name;	
	int n_ranges;		
	int n_aichan_se;	
	int n_aichan_diff;	
	unsigned int ns_min;	
	int n_aochan;		
	int n_dichan;		
	int n_dochan;		
	const struct comedi_lrange *ai_range_type;	
	const struct comedi_lrange *ao_range_type;	
	unsigned int io_range;	
	unsigned int IRQbits;	
	unsigned int DMAbits;	
	int ai_maxdata;		
	int ao_maxdata;		
	unsigned char fifo;	
	int is_818;
};

static const struct pcl818_board boardtypes[] = {
	{"pcl818l", 4, 16, 8, 25000, 1, 16, 16, &range_pcl818l_l_ai,
	 &range_unipolar5, PCLx1x_RANGE, 0x00fc,
	 0x0a, 0xfff, 0xfff, 0, 1},
	{"pcl818h", 9, 16, 8, 10000, 1, 16, 16, &range_pcl818h_ai,
	 &range_unipolar5, PCLx1x_RANGE, 0x00fc,
	 0x0a, 0xfff, 0xfff, 0, 1},
	{"pcl818hd", 9, 16, 8, 10000, 1, 16, 16, &range_pcl818h_ai,
	 &range_unipolar5, PCLx1x_RANGE, 0x00fc,
	 0x0a, 0xfff, 0xfff, 1, 1},
	{"pcl818hg", 12, 16, 8, 10000, 1, 16, 16, &range_pcl818hg_ai,
	 &range_unipolar5, PCLx1x_RANGE, 0x00fc,
	 0x0a, 0xfff, 0xfff, 1, 1},
	{"pcl818", 9, 16, 8, 10000, 2, 16, 16, &range_pcl818h_ai,
	 &range_unipolar5, PCLx1x_RANGE, 0x00fc,
	 0x0a, 0xfff, 0xfff, 0, 1},
	{"pcl718", 1, 16, 8, 16000, 2, 16, 16, &range_unipolar5,
	 &range_unipolar5, PCLx1x_RANGE, 0x00fc,
	 0x0a, 0xfff, 0xfff, 0, 0},
	
	{"pcm3718", 9, 16, 8, 10000, 0, 16, 16, &range_pcl818h_ai,
	 &range_unipolar5, PCLx1x_RANGE, 0x00fc,
	 0x0a, 0xfff, 0xfff, 0, 1  },
};

#define n_boardtypes (sizeof(boardtypes)/sizeof(struct pcl818_board))

static struct comedi_driver driver_pcl818 = {
	.driver_name = "pcl818",
	.module = THIS_MODULE,
	.attach = pcl818_attach,
	.detach = pcl818_detach,
	.board_name = &boardtypes[0].name,
	.num_names = n_boardtypes,
	.offset = sizeof(struct pcl818_board),
};

static int __init driver_pcl818_init_module(void)
{
	return comedi_driver_register(&driver_pcl818);
}

static void __exit driver_pcl818_cleanup_module(void)
{
	comedi_driver_unregister(&driver_pcl818);
}

module_init(driver_pcl818_init_module);
module_exit(driver_pcl818_cleanup_module);

struct pcl818_private {

	unsigned int dma;	
	int dma_rtc;		
	unsigned int io_range;
#ifdef unused
	unsigned long rtc_iobase;	
	unsigned int rtc_iosize;
	unsigned int rtc_irq;
	struct timer_list rtc_irq_timer;	
	unsigned long rtc_freq;	
	int rtc_irq_blocked;	
#endif
	unsigned long dmabuf[2];	
	unsigned int dmapages[2];	
	unsigned int hwdmaptr[2];	
	unsigned int hwdmasize[2];	
	unsigned int dmasamplsize;	
	unsigned int last_top_dma;	
	int next_dma_buf;	
	long dma_runs_to_end;	
	unsigned long last_dma_run;	
	unsigned char neverending_ai;	
	unsigned int ns_min;	
	int i8253_osc_base;	
	int irq_free;		
	int irq_blocked;	
	int irq_was_now_closed;	
	int ai_mode;		
	struct comedi_subdevice *last_int_sub;	
	int ai_act_scan;	
	int ai_act_chan;	
	unsigned int act_chanlist[16];	
	unsigned int act_chanlist_len;	
	unsigned int act_chanlist_pos;	
	unsigned int ai_scans;	
	unsigned int ai_n_chan;	
	unsigned int *ai_chanlist;	
	unsigned int ai_flags;	
	unsigned int ai_data_len;	
	short *ai_data;		
	unsigned int ai_timer1;	
	unsigned int ai_timer2;
	struct comedi_subdevice *sub_ai;	
	unsigned char usefifo;	
	unsigned int ao_readback[2];
};

static const unsigned int muxonechan[] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,	
	0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
};

#define devpriv ((struct pcl818_private *)dev->private)
#define this_board ((const struct pcl818_board *)dev->board_ptr)

static void setup_channel_list(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       unsigned int *chanlist, unsigned int n_chan,
			       unsigned int seglen);
static int check_channel_list(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      unsigned int *chanlist, unsigned int n_chan);

static int pcl818_ai_cancel(struct comedi_device *dev,
			    struct comedi_subdevice *s);
static void start_pacer(struct comedi_device *dev, int mode,
			unsigned int divisor1, unsigned int divisor2);

#ifdef unused
static int set_rtc_irq_bit(unsigned char bit);
static void rtc_dropped_irq(unsigned long data);
static int rtc_setfreq_irq(int freq);
#endif

static int pcl818_ai_insn_read(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{
	int n;
	int timeout;

	
	outb(0, dev->iobase + PCL818_CONTROL);

	
	outb(muxonechan[CR_CHAN(insn->chanspec)], dev->iobase + PCL818_MUX);

	
	outb(CR_RANGE(insn->chanspec), dev->iobase + PCL818_RANGE);

	for (n = 0; n < insn->n; n++) {

		
		outb(0, dev->iobase + PCL818_CLRINT);

		
		outb(0, dev->iobase + PCL818_AD_LO);

		timeout = 100;
		while (timeout--) {
			if (inb(dev->iobase + PCL818_STATUS) & 0x10)
				goto conv_finish;
			udelay(1);
		}
		comedi_error(dev, "A/D insn timeout");
		
		outb(0, dev->iobase + PCL818_CLRINT);
		return -EIO;

conv_finish:
		data[n] = ((inb(dev->iobase + PCL818_AD_HI) << 4) |
			   (inb(dev->iobase + PCL818_AD_LO) >> 4));
	}

	return n;
}

static int pcl818_ao_insn_read(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{
	int n;
	int chan = CR_CHAN(insn->chanspec);

	for (n = 0; n < insn->n; n++)
		data[n] = devpriv->ao_readback[chan];

	return n;
}

static int pcl818_ao_insn_write(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data)
{
	int n;
	int chan = CR_CHAN(insn->chanspec);

	for (n = 0; n < insn->n; n++) {
		devpriv->ao_readback[chan] = data[n];
		outb((data[n] & 0x000f) << 4, dev->iobase +
		     (chan ? PCL718_DA2_LO : PCL818_DA_LO));
		outb((data[n] & 0x0ff0) >> 4, dev->iobase +
		     (chan ? PCL718_DA2_HI : PCL818_DA_HI));
	}

	return n;
}

static int pcl818_di_insn_bits(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{
	if (insn->n != 2)
		return -EINVAL;

	data[1] = inb(dev->iobase + PCL818_DI_LO) |
	    (inb(dev->iobase + PCL818_DI_HI) << 8);

	return 2;
}

static int pcl818_do_insn_bits(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{
	if (insn->n != 2)
		return -EINVAL;

	s->state &= ~data[0];
	s->state |= (data[0] & data[1]);

	outb(s->state & 0xff, dev->iobase + PCL818_DO_LO);
	outb((s->state >> 8), dev->iobase + PCL818_DO_HI);

	data[1] = s->state;

	return 2;
}

static irqreturn_t interrupt_pcl818_ai_mode13_int(int irq, void *d)
{
	struct comedi_device *dev = d;
	struct comedi_subdevice *s = dev->subdevices + 0;
	int low;
	int timeout = 50;	

	while (timeout--) {
		if (inb(dev->iobase + PCL818_STATUS) & 0x10)
			goto conv_finish;
		udelay(1);
	}
	outb(0, dev->iobase + PCL818_STATUS);	
	comedi_error(dev, "A/D mode1/3 IRQ without DRDY!");
	pcl818_ai_cancel(dev, s);
	s->async->events |= COMEDI_CB_EOA | COMEDI_CB_ERROR;
	comedi_event(dev, s);
	return IRQ_HANDLED;

conv_finish:
	low = inb(dev->iobase + PCL818_AD_LO);
	comedi_buf_put(s->async, ((inb(dev->iobase + PCL818_AD_HI) << 4) | (low >> 4)));	
	outb(0, dev->iobase + PCL818_CLRINT);	

	if ((low & 0xf) != devpriv->act_chanlist[devpriv->act_chanlist_pos]) {	
		printk
		    ("comedi: A/D mode1/3 IRQ - channel dropout %x!=%x !\n",
		     (low & 0xf),
		     devpriv->act_chanlist[devpriv->act_chanlist_pos]);
		pcl818_ai_cancel(dev, s);
		s->async->events |= COMEDI_CB_EOA | COMEDI_CB_ERROR;
		comedi_event(dev, s);
		return IRQ_HANDLED;
	}
	devpriv->act_chanlist_pos++;
	if (devpriv->act_chanlist_pos >= devpriv->act_chanlist_len)
		devpriv->act_chanlist_pos = 0;

	s->async->cur_chan++;
	if (s->async->cur_chan >= devpriv->ai_n_chan) {
		
		s->async->cur_chan = 0;
		devpriv->ai_act_scan--;
	}

	if (!devpriv->neverending_ai) {
		if (devpriv->ai_act_scan == 0) {	
			pcl818_ai_cancel(dev, s);
			s->async->events |= COMEDI_CB_EOA;
		}
	}
	comedi_event(dev, s);
	return IRQ_HANDLED;
}

static irqreturn_t interrupt_pcl818_ai_mode13_dma(int irq, void *d)
{
	struct comedi_device *dev = d;
	struct comedi_subdevice *s = dev->subdevices + 0;
	int i, len, bufptr;
	unsigned long flags;
	short *ptr;

	disable_dma(devpriv->dma);
	devpriv->next_dma_buf = 1 - devpriv->next_dma_buf;
	if ((devpriv->dma_runs_to_end) > -1 || devpriv->neverending_ai) {	
		set_dma_mode(devpriv->dma, DMA_MODE_READ);
		flags = claim_dma_lock();
		set_dma_addr(devpriv->dma,
			     devpriv->hwdmaptr[devpriv->next_dma_buf]);
		if (devpriv->dma_runs_to_end || devpriv->neverending_ai) {
			set_dma_count(devpriv->dma,
				      devpriv->hwdmasize[devpriv->
							 next_dma_buf]);
		} else {
			set_dma_count(devpriv->dma, devpriv->last_dma_run);
		}
		release_dma_lock(flags);
		enable_dma(devpriv->dma);
	}
	printk("comedi: A/D mode1/3 IRQ \n");

	devpriv->dma_runs_to_end--;
	outb(0, dev->iobase + PCL818_CLRINT);	
	ptr = (short *)devpriv->dmabuf[1 - devpriv->next_dma_buf];

	len = devpriv->hwdmasize[0] >> 1;
	bufptr = 0;

	for (i = 0; i < len; i++) {
		if ((ptr[bufptr] & 0xf) != devpriv->act_chanlist[devpriv->act_chanlist_pos]) {	
			printk
			    ("comedi: A/D mode1/3 DMA - channel dropout %d(card)!=%d(chanlist) at %d !\n",
			     (ptr[bufptr] & 0xf),
			     devpriv->act_chanlist[devpriv->act_chanlist_pos],
			     devpriv->act_chanlist_pos);
			pcl818_ai_cancel(dev, s);
			s->async->events |= COMEDI_CB_EOA | COMEDI_CB_ERROR;
			comedi_event(dev, s);
			return IRQ_HANDLED;
		}

		comedi_buf_put(s->async, ptr[bufptr++] >> 4);	

		devpriv->act_chanlist_pos++;
		if (devpriv->act_chanlist_pos >= devpriv->act_chanlist_len)
			devpriv->act_chanlist_pos = 0;

		s->async->cur_chan++;
		if (s->async->cur_chan >= devpriv->ai_n_chan) {
			s->async->cur_chan = 0;
			devpriv->ai_act_scan--;
		}

		if (!devpriv->neverending_ai)
			if (devpriv->ai_act_scan == 0) {	
				pcl818_ai_cancel(dev, s);
				s->async->events |= COMEDI_CB_EOA;
				comedi_event(dev, s);
				
				return IRQ_HANDLED;
			}
	}

	if (len > 0)
		comedi_event(dev, s);
	return IRQ_HANDLED;
}

#ifdef unused
static irqreturn_t interrupt_pcl818_ai_mode13_dma_rtc(int irq, void *d)
{
	struct comedi_device *dev = d;
	struct comedi_subdevice *s = dev->subdevices + 0;
	unsigned long tmp;
	unsigned int top1, top2, i, bufptr;
	long ofs_dats;
	short *dmabuf = (short *)devpriv->dmabuf[0];

	
	switch (devpriv->ai_mode) {
	case INT_TYPE_AI1_DMA_RTC:
	case INT_TYPE_AI3_DMA_RTC:
		tmp = (CMOS_READ(RTC_INTR_FLAGS) & 0xF0);
		mod_timer(&devpriv->rtc_irq_timer,
			  jiffies + HZ / devpriv->rtc_freq + 2 * HZ / 100);

		for (i = 0; i < 10; i++) {
			top1 = get_dma_residue(devpriv->dma);
			top2 = get_dma_residue(devpriv->dma);
			if (top1 == top2)
				break;
		}

		if (top1 != top2)
			return IRQ_HANDLED;
		top1 = devpriv->hwdmasize[0] - top1;	
		top1 >>= 1;
		ofs_dats = top1 - devpriv->last_top_dma;	
		if (ofs_dats < 0)
			ofs_dats = (devpriv->dmasamplsize) + ofs_dats;
		if (!ofs_dats)
			return IRQ_HANDLED;	
		
		i = devpriv->last_top_dma - 1;
		i &= (devpriv->dmasamplsize - 1);

		if (dmabuf[i] != MAGIC_DMA_WORD) {	
			comedi_error(dev, "A/D mode1/3 DMA buffer overflow!");
			
			pcl818_ai_cancel(dev, s);
			s->async->events |= COMEDI_CB_EOA | COMEDI_CB_ERROR;
			comedi_event(dev, s);
			return IRQ_HANDLED;
		}
		

		bufptr = devpriv->last_top_dma;

		for (i = 0; i < ofs_dats; i++) {
			if ((dmabuf[bufptr] & 0xf) != devpriv->act_chanlist[devpriv->act_chanlist_pos]) {	
				printk
				    ("comedi: A/D mode1/3 DMA - channel dropout %d!=%d !\n",
				     (dmabuf[bufptr] & 0xf),
				     devpriv->
				     act_chanlist[devpriv->act_chanlist_pos]);
				pcl818_ai_cancel(dev, s);
				s->async->events |=
				    COMEDI_CB_EOA | COMEDI_CB_ERROR;
				comedi_event(dev, s);
				return IRQ_HANDLED;
			}

			comedi_buf_put(s->async, dmabuf[bufptr++] >> 4);	
			bufptr &= (devpriv->dmasamplsize - 1);

			devpriv->act_chanlist_pos++;
			if (devpriv->act_chanlist_pos >=
					devpriv->act_chanlist_len) {
				devpriv->act_chanlist_pos = 0;
			}
			s->async->cur_chan++;
			if (s->async->cur_chan >= devpriv->ai_n_chan) {
				s->async->cur_chan = 0;
				devpriv->ai_act_scan--;
			}

			if (!devpriv->neverending_ai)
				if (devpriv->ai_act_scan == 0) {	
					pcl818_ai_cancel(dev, s);
					s->async->events |= COMEDI_CB_EOA;
					comedi_event(dev, s);
					
					return IRQ_HANDLED;
				}
		}

		devpriv->last_top_dma = bufptr;
		bufptr--;
		bufptr &= (devpriv->dmasamplsize - 1);
		dmabuf[bufptr] = MAGIC_DMA_WORD;
		comedi_event(dev, s);
		
		return IRQ_HANDLED;
	}

	
	return IRQ_HANDLED;
}
#endif

static irqreturn_t interrupt_pcl818_ai_mode13_fifo(int irq, void *d)
{
	struct comedi_device *dev = d;
	struct comedi_subdevice *s = dev->subdevices + 0;
	int i, len, lo;

	outb(0, dev->iobase + PCL818_FI_INTCLR);	

	lo = inb(dev->iobase + PCL818_FI_STATUS);

	if (lo & 4) {
		comedi_error(dev, "A/D mode1/3 FIFO overflow!");
		pcl818_ai_cancel(dev, s);
		s->async->events |= COMEDI_CB_EOA | COMEDI_CB_ERROR;
		comedi_event(dev, s);
		return IRQ_HANDLED;
	}

	if (lo & 1) {
		comedi_error(dev, "A/D mode1/3 FIFO interrupt without data!");
		pcl818_ai_cancel(dev, s);
		s->async->events |= COMEDI_CB_EOA | COMEDI_CB_ERROR;
		comedi_event(dev, s);
		return IRQ_HANDLED;
	}

	if (lo & 2)
		len = 512;
	else
		len = 0;

	for (i = 0; i < len; i++) {
		lo = inb(dev->iobase + PCL818_FI_DATALO);
		if ((lo & 0xf) != devpriv->act_chanlist[devpriv->act_chanlist_pos]) {	
			printk
			    ("comedi: A/D mode1/3 FIFO - channel dropout %d!=%d !\n",
			     (lo & 0xf),
			     devpriv->act_chanlist[devpriv->act_chanlist_pos]);
			pcl818_ai_cancel(dev, s);
			s->async->events |= COMEDI_CB_EOA | COMEDI_CB_ERROR;
			comedi_event(dev, s);
			return IRQ_HANDLED;
		}

		comedi_buf_put(s->async, (lo >> 4) | (inb(dev->iobase + PCL818_FI_DATAHI) << 4));	

		devpriv->act_chanlist_pos++;
		if (devpriv->act_chanlist_pos >= devpriv->act_chanlist_len)
			devpriv->act_chanlist_pos = 0;

		s->async->cur_chan++;
		if (s->async->cur_chan >= devpriv->ai_n_chan) {
			s->async->cur_chan = 0;
			devpriv->ai_act_scan--;
		}

		if (!devpriv->neverending_ai)
			if (devpriv->ai_act_scan == 0) {	
				pcl818_ai_cancel(dev, s);
				s->async->events |= COMEDI_CB_EOA;
				comedi_event(dev, s);
				return IRQ_HANDLED;
			}
	}

	if (len > 0)
		comedi_event(dev, s);
	return IRQ_HANDLED;
}

static irqreturn_t interrupt_pcl818(int irq, void *d)
{
	struct comedi_device *dev = d;

	if (!dev->attached) {
		comedi_error(dev, "premature interrupt");
		return IRQ_HANDLED;
	}
	

	if (devpriv->irq_blocked && devpriv->irq_was_now_closed) {
		if ((devpriv->neverending_ai || (!devpriv->neverending_ai &&
						 devpriv->ai_act_scan > 0)) &&
		    (devpriv->ai_mode == INT_TYPE_AI1_DMA ||
		     devpriv->ai_mode == INT_TYPE_AI3_DMA)) {
			struct comedi_subdevice *s = dev->subdevices + 0;
			devpriv->ai_act_scan = 0;
			devpriv->neverending_ai = 0;
			pcl818_ai_cancel(dev, s);
		}

		outb(0, dev->iobase + PCL818_CLRINT);	

		return IRQ_HANDLED;
	}

	switch (devpriv->ai_mode) {
	case INT_TYPE_AI1_DMA:
	case INT_TYPE_AI3_DMA:
		return interrupt_pcl818_ai_mode13_dma(irq, d);
	case INT_TYPE_AI1_INT:
	case INT_TYPE_AI3_INT:
		return interrupt_pcl818_ai_mode13_int(irq, d);
	case INT_TYPE_AI1_FIFO:
	case INT_TYPE_AI3_FIFO:
		return interrupt_pcl818_ai_mode13_fifo(irq, d);
#ifdef PCL818_MODE13_AO
	case INT_TYPE_AO1_INT:
	case INT_TYPE_AO3_INT:
		return interrupt_pcl818_ao_mode13_int(irq, d);
#endif
	default:
		break;
	}

	outb(0, dev->iobase + PCL818_CLRINT);	

	if ((!dev->irq) || (!devpriv->irq_free) || (!devpriv->irq_blocked)
	    || (!devpriv->ai_mode)) {
		comedi_error(dev, "bad IRQ!");
		return IRQ_NONE;
	}

	comedi_error(dev, "IRQ from unknown source!");
	return IRQ_NONE;
}

static void pcl818_ai_mode13dma_int(int mode, struct comedi_device *dev,
				    struct comedi_subdevice *s)
{
	unsigned int flags;
	unsigned int bytes;

	printk("mode13dma_int, mode: %d\n", mode);
	disable_dma(devpriv->dma);	
	bytes = devpriv->hwdmasize[0];
	if (!devpriv->neverending_ai) {
		bytes = devpriv->ai_n_chan * devpriv->ai_scans * sizeof(short);	
		devpriv->dma_runs_to_end = bytes / devpriv->hwdmasize[0];	
		devpriv->last_dma_run = bytes % devpriv->hwdmasize[0];	
		devpriv->dma_runs_to_end--;
		if (devpriv->dma_runs_to_end >= 0)
			bytes = devpriv->hwdmasize[0];
	}

	devpriv->next_dma_buf = 0;
	set_dma_mode(devpriv->dma, DMA_MODE_READ);
	flags = claim_dma_lock();
	clear_dma_ff(devpriv->dma);
	set_dma_addr(devpriv->dma, devpriv->hwdmaptr[0]);
	set_dma_count(devpriv->dma, bytes);
	release_dma_lock(flags);
	enable_dma(devpriv->dma);

	if (mode == 1) {
		devpriv->ai_mode = INT_TYPE_AI1_DMA;
		outb(0x87 | (dev->irq << 4), dev->iobase + PCL818_CONTROL);	
	} else {
		devpriv->ai_mode = INT_TYPE_AI3_DMA;
		outb(0x86 | (dev->irq << 4), dev->iobase + PCL818_CONTROL);	
	};
}

#ifdef unused
static void pcl818_ai_mode13dma_rtc(int mode, struct comedi_device *dev,
				    struct comedi_subdevice *s)
{
	unsigned int flags;
	short *pole;

	set_dma_mode(devpriv->dma, DMA_MODE_READ | DMA_AUTOINIT);
	flags = claim_dma_lock();
	clear_dma_ff(devpriv->dma);
	set_dma_addr(devpriv->dma, devpriv->hwdmaptr[0]);
	set_dma_count(devpriv->dma, devpriv->hwdmasize[0]);
	release_dma_lock(flags);
	enable_dma(devpriv->dma);
	devpriv->last_top_dma = 0;	
	pole = (short *)devpriv->dmabuf[0];
	devpriv->dmasamplsize = devpriv->hwdmasize[0] / 2;
	pole[devpriv->dmasamplsize - 1] = MAGIC_DMA_WORD;
#ifdef unused
	devpriv->rtc_freq = rtc_setfreq_irq(2048);
	devpriv->rtc_irq_timer.expires =
	    jiffies + HZ / devpriv->rtc_freq + 2 * HZ / 100;
	devpriv->rtc_irq_timer.data = (unsigned long)dev;
	devpriv->rtc_irq_timer.function = rtc_dropped_irq;

	add_timer(&devpriv->rtc_irq_timer);
#endif

	if (mode == 1) {
		devpriv->int818_mode = INT_TYPE_AI1_DMA_RTC;
		outb(0x07 | (dev->irq << 4), dev->iobase + PCL818_CONTROL);	
	} else {
		devpriv->int818_mode = INT_TYPE_AI3_DMA_RTC;
		outb(0x06 | (dev->irq << 4), dev->iobase + PCL818_CONTROL);	
	};
}
#endif

static int pcl818_ai_cmd_mode(int mode, struct comedi_device *dev,
			      struct comedi_subdevice *s)
{
	struct comedi_cmd *cmd = &s->async->cmd;
	int divisor1 = 0, divisor2 = 0;
	unsigned int seglen;

	dev_dbg(dev->hw_dev, "pcl818_ai_cmd_mode()\n");
	if ((!dev->irq) && (!devpriv->dma_rtc)) {
		comedi_error(dev, "IRQ not defined!");
		return -EINVAL;
	}

	if (devpriv->irq_blocked)
		return -EBUSY;

	start_pacer(dev, -1, 0, 0);	

	seglen = check_channel_list(dev, s, devpriv->ai_chanlist,
				    devpriv->ai_n_chan);
	if (seglen < 1)
		return -EINVAL;
	setup_channel_list(dev, s, devpriv->ai_chanlist,
			   devpriv->ai_n_chan, seglen);

	udelay(1);

	devpriv->ai_act_scan = devpriv->ai_scans;
	devpriv->ai_act_chan = 0;
	devpriv->irq_blocked = 1;
	devpriv->irq_was_now_closed = 0;
	devpriv->neverending_ai = 0;
	devpriv->act_chanlist_pos = 0;
	devpriv->dma_runs_to_end = 0;

	if ((devpriv->ai_scans == 0) || (devpriv->ai_scans == -1))
		devpriv->neverending_ai = 1;	

	if (mode == 1) {
		i8253_cascade_ns_to_timer(devpriv->i8253_osc_base, &divisor1,
					  &divisor2, &cmd->convert_arg,
					  TRIG_ROUND_NEAREST);
		if (divisor1 == 1) {	
			divisor1 = 2;
			divisor2 /= 2;
		}
		if (divisor2 == 1) {
			divisor2 = 2;
			divisor1 /= 2;
		}
	}

	outb(0, dev->iobase + PCL818_CNTENABLE);	

	switch (devpriv->dma) {
	case 1:		
	case 3:
		if (devpriv->dma_rtc == 0) {
			pcl818_ai_mode13dma_int(mode, dev, s);
		}
#ifdef unused
		else {
			pcl818_ai_mode13dma_rtc(mode, dev, s);
		}
#else
		else {
			return -EINVAL;
		}
#endif
		break;
	case 0:
		if (!devpriv->usefifo) {
			
			
			if (mode == 1) {
				devpriv->ai_mode = INT_TYPE_AI1_INT;
				
				outb(0x83 | (dev->irq << 4),
				     dev->iobase + PCL818_CONTROL);
			} else {
				devpriv->ai_mode = INT_TYPE_AI3_INT;
				
				outb(0x82 | (dev->irq << 4),
				     dev->iobase + PCL818_CONTROL);
			}
		} else {
			
			
			outb(1, dev->iobase + PCL818_FI_ENABLE);
			if (mode == 1) {
				devpriv->ai_mode = INT_TYPE_AI1_FIFO;
				
				outb(0x03, dev->iobase + PCL818_CONTROL);
			} else {
				devpriv->ai_mode = INT_TYPE_AI3_FIFO;
				outb(0x02, dev->iobase + PCL818_CONTROL);
			}
		}
	}

	start_pacer(dev, mode, divisor1, divisor2);

#ifdef unused
	switch (devpriv->ai_mode) {
	case INT_TYPE_AI1_DMA_RTC:
	case INT_TYPE_AI3_DMA_RTC:
		set_rtc_irq_bit(1);	
		break;
	}
#endif
	dev_dbg(dev->hw_dev, "pcl818_ai_cmd_mode() end\n");
	return 0;
}

#ifdef unused
#ifdef PCL818_MODE13_AO
static int pcl818_ao_mode13(int mode, struct comedi_device *dev,
			    struct comedi_subdevice *s, comedi_trig * it)
{
	int divisor1 = 0, divisor2 = 0;

	if (!dev->irq) {
		comedi_error(dev, "IRQ not defined!");
		return -EINVAL;
	}

	if (devpriv->irq_blocked)
		return -EBUSY;

	start_pacer(dev, -1, 0, 0);	

	devpriv->int13_act_scan = it->n;
	devpriv->int13_act_chan = 0;
	devpriv->irq_blocked = 1;
	devpriv->irq_was_now_closed = 0;
	devpriv->neverending_ai = 0;
	devpriv->act_chanlist_pos = 0;

	if (mode == 1) {
		i8253_cascade_ns_to_timer(devpriv->i8253_osc_base, &divisor1,
					  &divisor2, &it->trigvar,
					  TRIG_ROUND_NEAREST);
		if (divisor1 == 1) {	
			divisor1 = 2;
			divisor2 /= 2;
		}
		if (divisor2 == 1) {
			divisor2 = 2;
			divisor1 /= 2;
		}
	}

	outb(0, dev->iobase + PCL818_CNTENABLE);	
	if (mode == 1) {
		devpriv->int818_mode = INT_TYPE_AO1_INT;
		outb(0x83 | (dev->irq << 4), dev->iobase + PCL818_CONTROL);	
	} else {
		devpriv->int818_mode = INT_TYPE_AO3_INT;
		outb(0x82 | (dev->irq << 4), dev->iobase + PCL818_CONTROL);	
	};

	start_pacer(dev, mode, divisor1, divisor2);

	return 0;
}

static int pcl818_ao_mode1(struct comedi_device *dev,
			   struct comedi_subdevice *s, comedi_trig * it)
{
	return pcl818_ao_mode13(1, dev, s, it);
}

static int pcl818_ao_mode3(struct comedi_device *dev,
			   struct comedi_subdevice *s, comedi_trig * it)
{
	return pcl818_ao_mode13(3, dev, s, it);
}
#endif
#endif

static void start_pacer(struct comedi_device *dev, int mode,
			unsigned int divisor1, unsigned int divisor2)
{
	outb(0xb4, dev->iobase + PCL818_CTRCTL);
	outb(0x74, dev->iobase + PCL818_CTRCTL);
	udelay(1);

	if (mode == 1) {
		outb(divisor2 & 0xff, dev->iobase + PCL818_CTR2);
		outb((divisor2 >> 8) & 0xff, dev->iobase + PCL818_CTR2);
		outb(divisor1 & 0xff, dev->iobase + PCL818_CTR1);
		outb((divisor1 >> 8) & 0xff, dev->iobase + PCL818_CTR1);
	}
}

static int check_channel_list(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      unsigned int *chanlist, unsigned int n_chan)
{
	unsigned int chansegment[16];
	unsigned int i, nowmustbechan, seglen, segpos;

	
	if (n_chan < 1) {
		comedi_error(dev, "range/channel list is empty!");
		return 0;
	}

	if (n_chan > 1) {
		
		chansegment[0] = chanlist[0];
		
		for (i = 1, seglen = 1; i < n_chan; i++, seglen++) {


			

			if (chanlist[0] == chanlist[i])
				break;
			nowmustbechan =
			    (CR_CHAN(chansegment[i - 1]) + 1) % s->n_chan;
			if (nowmustbechan != CR_CHAN(chanlist[i])) {	
				printk
				    ("comedi%d: pcl818: channel list must be continuous! chanlist[%i]=%d but must be %d or %d!\n",
				     dev->minor, i, CR_CHAN(chanlist[i]),
				     nowmustbechan, CR_CHAN(chanlist[0]));
				return 0;
			}
			
			chansegment[i] = chanlist[i];
		}

		
		for (i = 0, segpos = 0; i < n_chan; i++) {
			
			if (chanlist[i] != chansegment[i % seglen]) {
				printk
				    ("comedi%d: pcl818: bad channel or range number! chanlist[%i]=%d,%d,%d and not %d,%d,%d!\n",
				     dev->minor, i, CR_CHAN(chansegment[i]),
				     CR_RANGE(chansegment[i]),
				     CR_AREF(chansegment[i]),
				     CR_CHAN(chanlist[i % seglen]),
				     CR_RANGE(chanlist[i % seglen]),
				     CR_AREF(chansegment[i % seglen]));
				return 0;	
			}
		}
	} else {
		seglen = 1;
	}
	printk("check_channel_list: seglen %d\n", seglen);
	return seglen;
}

static void setup_channel_list(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       unsigned int *chanlist, unsigned int n_chan,
			       unsigned int seglen)
{
	int i;

	devpriv->act_chanlist_len = seglen;
	devpriv->act_chanlist_pos = 0;

	for (i = 0; i < seglen; i++) {	
		devpriv->act_chanlist[i] = CR_CHAN(chanlist[i]);
		outb(muxonechan[CR_CHAN(chanlist[i])], dev->iobase + PCL818_MUX);	
		outb(CR_RANGE(chanlist[i]), dev->iobase + PCL818_RANGE);	
	}

	udelay(1);

	
	outb(devpriv->act_chanlist[0] | (devpriv->act_chanlist[seglen -
							       1] << 4),
	     dev->iobase + PCL818_MUX);
}

static int check_single_ended(unsigned int port)
{
	if (inb(port + PCL818_STATUS) & 0x20)
		return 1;
	return 0;
}

static int ai_cmdtest(struct comedi_device *dev, struct comedi_subdevice *s,
		      struct comedi_cmd *cmd)
{
	int err = 0;
	int tmp, divisor1 = 0, divisor2 = 0;

	

	tmp = cmd->start_src;
	cmd->start_src &= TRIG_NOW;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	tmp = cmd->scan_begin_src;
	cmd->scan_begin_src &= TRIG_FOLLOW;
	if (!cmd->scan_begin_src || tmp != cmd->scan_begin_src)
		err++;

	tmp = cmd->convert_src;
	cmd->convert_src &= TRIG_TIMER | TRIG_EXT;
	if (!cmd->convert_src || tmp != cmd->convert_src)
		err++;

	tmp = cmd->scan_end_src;
	cmd->scan_end_src &= TRIG_COUNT;
	if (!cmd->scan_end_src || tmp != cmd->scan_end_src)
		err++;

	tmp = cmd->stop_src;
	cmd->stop_src &= TRIG_COUNT | TRIG_NONE;
	if (!cmd->stop_src || tmp != cmd->stop_src)
		err++;

	if (err)
		return 1;

	

	if (cmd->start_src != TRIG_NOW) {
		cmd->start_src = TRIG_NOW;
		err++;
	}
	if (cmd->scan_begin_src != TRIG_FOLLOW) {
		cmd->scan_begin_src = TRIG_FOLLOW;
		err++;
	}
	if (cmd->convert_src != TRIG_TIMER && cmd->convert_src != TRIG_EXT)
		err++;

	if (cmd->scan_end_src != TRIG_COUNT) {
		cmd->scan_end_src = TRIG_COUNT;
		err++;
	}

	if (cmd->stop_src != TRIG_NONE && cmd->stop_src != TRIG_COUNT)
		err++;

	if (err)
		return 2;

	

	if (cmd->start_arg != 0) {
		cmd->start_arg = 0;
		err++;
	}

	if (cmd->scan_begin_arg != 0) {
		cmd->scan_begin_arg = 0;
		err++;
	}

	if (cmd->convert_src == TRIG_TIMER) {
		if (cmd->convert_arg < this_board->ns_min) {
			cmd->convert_arg = this_board->ns_min;
			err++;
		}
	} else {		
		if (cmd->convert_arg != 0) {
			cmd->convert_arg = 0;
			err++;
		}
	}

	if (cmd->scan_end_arg != cmd->chanlist_len) {
		cmd->scan_end_arg = cmd->chanlist_len;
		err++;
	}
	if (cmd->stop_src == TRIG_COUNT) {
		if (!cmd->stop_arg) {
			cmd->stop_arg = 1;
			err++;
		}
	} else {		
		if (cmd->stop_arg != 0) {
			cmd->stop_arg = 0;
			err++;
		}
	}

	if (err)
		return 3;

	

	if (cmd->convert_src == TRIG_TIMER) {
		tmp = cmd->convert_arg;
		i8253_cascade_ns_to_timer(devpriv->i8253_osc_base, &divisor1,
					  &divisor2, &cmd->convert_arg,
					  cmd->flags & TRIG_ROUND_MASK);
		if (cmd->convert_arg < this_board->ns_min)
			cmd->convert_arg = this_board->ns_min;
		if (tmp != cmd->convert_arg)
			err++;
	}

	if (err)
		return 4;

	

	if (cmd->chanlist) {
		if (!check_channel_list(dev, s, cmd->chanlist,
					cmd->chanlist_len))
			return 5;	
	}

	return 0;
}

static int ai_cmd(struct comedi_device *dev, struct comedi_subdevice *s)
{
	struct comedi_cmd *cmd = &s->async->cmd;
	int retval;

	dev_dbg(dev->hw_dev, "pcl818_ai_cmd()\n");
	devpriv->ai_n_chan = cmd->chanlist_len;
	devpriv->ai_chanlist = cmd->chanlist;
	devpriv->ai_flags = cmd->flags;
	devpriv->ai_data_len = s->async->prealloc_bufsz;
	devpriv->ai_data = s->async->prealloc_buf;
	devpriv->ai_timer1 = 0;
	devpriv->ai_timer2 = 0;

	if (cmd->stop_src == TRIG_COUNT)
		devpriv->ai_scans = cmd->stop_arg;
	else
		devpriv->ai_scans = 0;

	if (cmd->scan_begin_src == TRIG_FOLLOW) {	
		if (cmd->convert_src == TRIG_TIMER) {	
			devpriv->ai_timer1 = cmd->convert_arg;
			retval = pcl818_ai_cmd_mode(1, dev, s);
			dev_dbg(dev->hw_dev, "pcl818_ai_cmd() end\n");
			return retval;
		}
		if (cmd->convert_src == TRIG_EXT) {	
			return pcl818_ai_cmd_mode(3, dev, s);
		}
	}

	return -1;
}

static int pcl818_ai_cancel(struct comedi_device *dev,
			    struct comedi_subdevice *s)
{
	if (devpriv->irq_blocked > 0) {
		dev_dbg(dev->hw_dev, "pcl818_ai_cancel()\n");
		devpriv->irq_was_now_closed = 1;

		switch (devpriv->ai_mode) {
#ifdef unused
		case INT_TYPE_AI1_DMA_RTC:
		case INT_TYPE_AI3_DMA_RTC:
			set_rtc_irq_bit(0);	
			del_timer(&devpriv->rtc_irq_timer);
#endif
		case INT_TYPE_AI1_DMA:
		case INT_TYPE_AI3_DMA:
			if (devpriv->neverending_ai ||
			    (!devpriv->neverending_ai &&
			     devpriv->ai_act_scan > 0)) {
				
				goto end;
			}
			disable_dma(devpriv->dma);
		case INT_TYPE_AI1_INT:
		case INT_TYPE_AI3_INT:
		case INT_TYPE_AI1_FIFO:
		case INT_TYPE_AI3_FIFO:
#ifdef PCL818_MODE13_AO
		case INT_TYPE_AO1_INT:
		case INT_TYPE_AO3_INT:
#endif
			outb(inb(dev->iobase + PCL818_CONTROL) & 0x73, dev->iobase + PCL818_CONTROL);	
			udelay(1);
			start_pacer(dev, -1, 0, 0);
			outb(0, dev->iobase + PCL818_AD_LO);
			inb(dev->iobase + PCL818_AD_LO);
			inb(dev->iobase + PCL818_AD_HI);
			outb(0, dev->iobase + PCL818_CLRINT);	
			outb(0, dev->iobase + PCL818_CONTROL);	
			if (devpriv->usefifo) {	
				outb(0, dev->iobase + PCL818_FI_INTCLR);
				outb(0, dev->iobase + PCL818_FI_FLUSH);
				outb(0, dev->iobase + PCL818_FI_ENABLE);
			}
			devpriv->irq_blocked = 0;
			devpriv->last_int_sub = s;
			devpriv->neverending_ai = 0;
			devpriv->ai_mode = 0;
			devpriv->irq_was_now_closed = 0;
			break;
		}
	}

end:
	dev_dbg(dev->hw_dev, "pcl818_ai_cancel() end\n");
	return 0;
}

static int pcl818_check(unsigned long iobase)
{
	outb(0x00, iobase + PCL818_MUX);
	udelay(1);
	if (inb(iobase + PCL818_MUX) != 0x00)
		return 1;	
	outb(0x55, iobase + PCL818_MUX);
	udelay(1);
	if (inb(iobase + PCL818_MUX) != 0x55)
		return 1;	
	outb(0x00, iobase + PCL818_MUX);
	udelay(1);
	outb(0x18, iobase + PCL818_CONTROL);
	udelay(1);
	if (inb(iobase + PCL818_CONTROL) != 0x18)
		return 1;	
	return 0;		
}

static void pcl818_reset(struct comedi_device *dev)
{
	if (devpriv->usefifo) {	
		outb(0, dev->iobase + PCL818_FI_INTCLR);
		outb(0, dev->iobase + PCL818_FI_FLUSH);
		outb(0, dev->iobase + PCL818_FI_ENABLE);
	}
	outb(0, dev->iobase + PCL818_DA_LO);	
	outb(0, dev->iobase + PCL818_DA_HI);
	udelay(1);
	outb(0, dev->iobase + PCL818_DO_HI);	
	outb(0, dev->iobase + PCL818_DO_LO);
	udelay(1);
	outb(0, dev->iobase + PCL818_CONTROL);
	outb(0, dev->iobase + PCL818_CNTENABLE);
	outb(0, dev->iobase + PCL818_MUX);
	outb(0, dev->iobase + PCL818_CLRINT);
	outb(0xb0, dev->iobase + PCL818_CTRCTL);	
	outb(0x70, dev->iobase + PCL818_CTRCTL);
	outb(0x30, dev->iobase + PCL818_CTRCTL);
	if (this_board->is_818) {
		outb(0, dev->iobase + PCL818_RANGE);
	} else {
		outb(0, dev->iobase + PCL718_DA2_LO);
		outb(0, dev->iobase + PCL718_DA2_HI);
	}
}

#ifdef unused
static int set_rtc_irq_bit(unsigned char bit)
{
	unsigned char val;
	unsigned long flags;

	if (bit == 1) {
		RTC_timer_lock++;
		if (RTC_timer_lock > 1)
			return 0;
	} else {
		RTC_timer_lock--;
		if (RTC_timer_lock < 0)
			RTC_timer_lock = 0;
		if (RTC_timer_lock > 0)
			return 0;
	}

	save_flags(flags);
	cli();
	val = CMOS_READ(RTC_CONTROL);
	if (bit)
		val |= RTC_PIE;
	else
		val &= ~RTC_PIE;

	CMOS_WRITE(val, RTC_CONTROL);
	CMOS_READ(RTC_INTR_FLAGS);
	restore_flags(flags);
	return 0;
}

static void rtc_dropped_irq(unsigned long data)
{
	struct comedi_device *dev = (void *)data;
	unsigned long flags, tmp;

	switch (devpriv->int818_mode) {
	case INT_TYPE_AI1_DMA_RTC:
	case INT_TYPE_AI3_DMA_RTC:
		mod_timer(&devpriv->rtc_irq_timer,
			  jiffies + HZ / devpriv->rtc_freq + 2 * HZ / 100);
		save_flags(flags);
		cli();
		tmp = (CMOS_READ(RTC_INTR_FLAGS) & 0xF0);	
		restore_flags(flags);
		break;
	}
}

static int rtc_setfreq_irq(int freq)
{
	int tmp = 0;
	int rtc_freq;
	unsigned char val;
	unsigned long flags;

	if (freq < 2)
		freq = 2;
	if (freq > 8192)
		freq = 8192;

	while (freq > (1 << tmp))
		tmp++;

	rtc_freq = 1 << tmp;

	save_flags(flags);
	cli();
	val = CMOS_READ(RTC_FREQ_SELECT) & 0xf0;
	val |= (16 - tmp);
	CMOS_WRITE(val, RTC_FREQ_SELECT);
	restore_flags(flags);
	return rtc_freq;
}
#endif

static void free_resources(struct comedi_device *dev)
{
	
	if (dev->private) {
		pcl818_ai_cancel(dev, devpriv->sub_ai);
		pcl818_reset(dev);
		if (devpriv->dma)
			free_dma(devpriv->dma);
		if (devpriv->dmabuf[0])
			free_pages(devpriv->dmabuf[0], devpriv->dmapages[0]);
		if (devpriv->dmabuf[1])
			free_pages(devpriv->dmabuf[1], devpriv->dmapages[1]);
#ifdef unused
		if (devpriv->rtc_irq)
			free_irq(devpriv->rtc_irq, dev);
		if ((devpriv->dma_rtc) && (RTC_lock == 1)) {
			if (devpriv->rtc_iobase)
				release_region(devpriv->rtc_iobase,
					       devpriv->rtc_iosize);
		}
		if (devpriv->dma_rtc)
			RTC_lock--;
#endif
	}

	if (dev->irq)
		free_irq(dev->irq, dev);
	if (dev->iobase)
		release_region(dev->iobase, devpriv->io_range);
	
}

static int pcl818_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	int ret;
	unsigned long iobase;
	unsigned int irq;
	int dma;
	unsigned long pages;
	struct comedi_subdevice *s;

	ret = alloc_private(dev, sizeof(struct pcl818_private));
	if (ret < 0)
		return ret;	

	
	iobase = it->options[0];
	printk
	    ("comedi%d: pcl818:  board=%s, ioport=0x%03lx",
	     dev->minor, this_board->name, iobase);
	devpriv->io_range = this_board->io_range;
	if ((this_board->fifo) && (it->options[2] == -1)) {	
		devpriv->io_range = PCLx1xFIFO_RANGE;
		devpriv->usefifo = 1;
	}
	if (!request_region(iobase, devpriv->io_range, "pcl818")) {
		comedi_error(dev, "I/O port conflict\n");
		return -EIO;
	}

	dev->iobase = iobase;

	if (pcl818_check(iobase)) {
		comedi_error(dev, "I can't detect board. FAIL!\n");
		return -EIO;
	}

	
	dev->board_name = this_board->name;
	
	irq = 0;
	if (this_board->IRQbits != 0) {	
		irq = it->options[1];
		if (irq) {	
			if (((1 << irq) & this_board->IRQbits) == 0) {
				printk
				    (", IRQ %u is out of allowed range, DISABLING IT",
				     irq);
				irq = 0;	
			} else {
				if (request_irq
				    (irq, interrupt_pcl818, 0, "pcl818", dev)) {
					printk
					    (", unable to allocate IRQ %u, DISABLING IT",
					     irq);
					irq = 0;	
				} else {
					printk(KERN_DEBUG "irq=%u", irq);
				}
			}
		}
	}

	dev->irq = irq;
	if (irq)
		devpriv->irq_free = 1;   
	else
		devpriv->irq_free = 0;

	devpriv->irq_blocked = 0;	
	devpriv->ai_mode = 0;	

#ifdef unused
	
	devpriv->dma_rtc = 0;
	if (it->options[2] > 0) {	
		if (RTC_lock == 0) {
			if (!request_region(RTC_PORT(0), RTC_IO_EXTENT,
					    "pcl818 (RTC)"))
				goto no_rtc;
		}
		devpriv->rtc_iobase = RTC_PORT(0);
		devpriv->rtc_iosize = RTC_IO_EXTENT;
		RTC_lock++;
		if (!request_irq(RTC_IRQ, interrupt_pcl818_ai_mode13_dma_rtc, 0,
				 "pcl818 DMA (RTC)", dev)) {
			devpriv->dma_rtc = 1;
			devpriv->rtc_irq = RTC_IRQ;
			printk(KERN_DEBUG "dma_irq=%u", devpriv->rtc_irq);
		} else {
			RTC_lock--;
			if (RTC_lock == 0) {
				if (devpriv->rtc_iobase)
					release_region(devpriv->rtc_iobase,
						       devpriv->rtc_iosize);
			}
			devpriv->rtc_iobase = 0;
			devpriv->rtc_iosize = 0;
		}
	}

no_rtc:
#endif
	
	dma = 0;
	devpriv->dma = dma;
	if ((devpriv->irq_free == 0) && (devpriv->dma_rtc == 0))
		goto no_dma;	
	if (this_board->DMAbits != 0) {	
		dma = it->options[2];
		if (dma < 1)
			goto no_dma;	
		if (((1 << dma) & this_board->DMAbits) == 0) {
			printk(KERN_ERR "DMA is out of allowed range, FAIL!\n");
			return -EINVAL;	
		}
		ret = request_dma(dma, "pcl818");
		if (ret)
			return -EBUSY;	
		devpriv->dma = dma;
		pages = 2;	
		devpriv->dmabuf[0] = __get_dma_pages(GFP_KERNEL, pages);
		if (!devpriv->dmabuf[0])
			
			return -EBUSY;	
		devpriv->dmapages[0] = pages;
		devpriv->hwdmaptr[0] = virt_to_bus((void *)devpriv->dmabuf[0]);
		devpriv->hwdmasize[0] = (1 << pages) * PAGE_SIZE;
		
		if (devpriv->dma_rtc == 0) {	
			devpriv->dmabuf[1] = __get_dma_pages(GFP_KERNEL, pages);
			if (!devpriv->dmabuf[1])
				return -EBUSY;
			devpriv->dmapages[1] = pages;
			devpriv->hwdmaptr[1] =
			    virt_to_bus((void *)devpriv->dmabuf[1]);
			devpriv->hwdmasize[1] = (1 << pages) * PAGE_SIZE;
		}
	}

no_dma:

	ret = alloc_subdevices(dev, 4);
	if (ret < 0)
		return ret;

	s = dev->subdevices + 0;
	if (!this_board->n_aichan_se) {
		s->type = COMEDI_SUBD_UNUSED;
	} else {
		s->type = COMEDI_SUBD_AI;
		devpriv->sub_ai = s;
		s->subdev_flags = SDF_READABLE;
		if (check_single_ended(dev->iobase)) {
			s->n_chan = this_board->n_aichan_se;
			s->subdev_flags |= SDF_COMMON | SDF_GROUND;
			printk(", %dchans S.E. DAC", s->n_chan);
		} else {
			s->n_chan = this_board->n_aichan_diff;
			s->subdev_flags |= SDF_DIFF;
			printk(", %dchans DIFF DAC", s->n_chan);
		}
		s->maxdata = this_board->ai_maxdata;
		s->len_chanlist = s->n_chan;
		s->range_table = this_board->ai_range_type;
		s->cancel = pcl818_ai_cancel;
		s->insn_read = pcl818_ai_insn_read;
		if ((irq) || (devpriv->dma_rtc)) {
			dev->read_subdev = s;
			s->subdev_flags |= SDF_CMD_READ;
			s->do_cmdtest = ai_cmdtest;
			s->do_cmd = ai_cmd;
		}
		if (this_board->is_818) {
			if ((it->options[4] == 1) || (it->options[4] == 10))
				s->range_table = &range_pcl818l_h_ai;	
		} else {
			switch (it->options[4]) {
			case 0:
				s->range_table = &range_bipolar10;
				break;
			case 1:
				s->range_table = &range_bipolar5;
				break;
			case 2:
				s->range_table = &range_bipolar2_5;
				break;
			case 3:
				s->range_table = &range718_bipolar1;
				break;
			case 4:
				s->range_table = &range718_bipolar0_5;
				break;
			case 6:
				s->range_table = &range_unipolar10;
				break;
			case 7:
				s->range_table = &range_unipolar5;
				break;
			case 8:
				s->range_table = &range718_unipolar2;
				break;
			case 9:
				s->range_table = &range718_unipolar1;
				break;
			default:
				s->range_table = &range_unknown;
				break;
			}
		}
	}

	s = dev->subdevices + 1;
	if (!this_board->n_aochan) {
		s->type = COMEDI_SUBD_UNUSED;
	} else {
		s->type = COMEDI_SUBD_AO;
		s->subdev_flags = SDF_WRITABLE | SDF_GROUND;
		s->n_chan = this_board->n_aochan;
		s->maxdata = this_board->ao_maxdata;
		s->len_chanlist = this_board->n_aochan;
		s->range_table = this_board->ao_range_type;
		s->insn_read = pcl818_ao_insn_read;
		s->insn_write = pcl818_ao_insn_write;
#ifdef unused
#ifdef PCL818_MODE13_AO
		if (irq) {
			s->trig[1] = pcl818_ao_mode1;
			s->trig[3] = pcl818_ao_mode3;
		}
#endif
#endif
		if (this_board->is_818) {
			if ((it->options[4] == 1) || (it->options[4] == 10))
				s->range_table = &range_unipolar10;
			if (it->options[4] == 2)
				s->range_table = &range_unknown;
		} else {
			if ((it->options[5] == 1) || (it->options[5] == 10))
				s->range_table = &range_unipolar10;
			if (it->options[5] == 2)
				s->range_table = &range_unknown;
		}
	}

	s = dev->subdevices + 2;
	if (!this_board->n_dichan) {
		s->type = COMEDI_SUBD_UNUSED;
	} else {
		s->type = COMEDI_SUBD_DI;
		s->subdev_flags = SDF_READABLE;
		s->n_chan = this_board->n_dichan;
		s->maxdata = 1;
		s->len_chanlist = this_board->n_dichan;
		s->range_table = &range_digital;
		s->insn_bits = pcl818_di_insn_bits;
	}

	s = dev->subdevices + 3;
	if (!this_board->n_dochan) {
		s->type = COMEDI_SUBD_UNUSED;
	} else {
		s->type = COMEDI_SUBD_DO;
		s->subdev_flags = SDF_WRITABLE;
		s->n_chan = this_board->n_dochan;
		s->maxdata = 1;
		s->len_chanlist = this_board->n_dochan;
		s->range_table = &range_digital;
		s->insn_bits = pcl818_do_insn_bits;
	}

	
	if ((it->options[3] == 0) || (it->options[3] == 10))
		devpriv->i8253_osc_base = 100;
	else
		devpriv->i8253_osc_base = 1000;

	
	devpriv->ns_min = this_board->ns_min;

	if (!this_board->is_818) {
		if ((it->options[6] == 1) || (it->options[6] == 100))
			devpriv->ns_min = 10000;	
	}

	pcl818_reset(dev);

	printk("\n");

	return 0;
}

static int pcl818_detach(struct comedi_device *dev)
{
	
	free_resources(dev);
	return 0;
}

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
