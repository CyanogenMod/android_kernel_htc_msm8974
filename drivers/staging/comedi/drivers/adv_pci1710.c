
#include <linux/interrupt.h>

#include "../comedidev.h"

#include "comedi_pci.h"

#include "8253.h"
#include "amcc_s5933.h"

#define PCI171x_PARANOIDCHECK	

#undef PCI171X_EXTDEBUG

#define DRV_NAME "adv_pci1710"

#undef DPRINTK
#ifdef PCI171X_EXTDEBUG
#define DPRINTK(fmt, args...) printk(fmt, ## args)
#else
#define DPRINTK(fmt, args...)
#endif

#define PCI_VENDOR_ID_ADVANTECH		0x13fe

#define TYPE_PCI171X	0
#define TYPE_PCI1713	2
#define TYPE_PCI1720	3

#define IORANGE_171x	32
#define IORANGE_1720	16

#define PCI171x_AD_DATA	 0	
#define PCI171x_SOFTTRG	 0	
#define PCI171x_RANGE	 2	
#define PCI171x_MUX	 4	
#define PCI171x_STATUS	 6	
#define PCI171x_CONTROL	 6	
#define PCI171x_CLRINT	 8	
#define PCI171x_CLRFIFO	 9	
#define PCI171x_DA1	10	
#define PCI171x_DA2	12	
#define PCI171x_DAREF	14	
#define PCI171x_DI	16	
#define PCI171x_DO	16	
#define PCI171x_CNT0	24	
#define PCI171x_CNT1	26	
#define PCI171x_CNT2	28	
#define PCI171x_CNTCTRL	30	

#define	Status_FE	0x0100	
#define Status_FH	0x0200	
#define Status_FF	0x0400	
#define Status_IRQ	0x0800	
#define Control_CNT0	0x0040	
#define Control_ONEFH	0x0020	
#define Control_IRQEN	0x0010	
#define Control_GATE	0x0008	
#define Control_EXT	0x0004	
#define Control_PACER	0x0002	
#define Control_SW	0x0001	
#define Counter_BCD     0x0001	
#define Counter_M0      0x0002	
#define Counter_M1      0x0004	
#define Counter_M2      0x0008
#define Counter_RW0     0x0010	
#define Counter_RW1     0x0020
#define Counter_SC0     0x0040	
#define Counter_SC1     0x0080	

#define PCI1720_DA0	 0	
#define PCI1720_DA1	 2	
#define PCI1720_DA2	 4	
#define PCI1720_DA3	 6	
#define PCI1720_RANGE	 8	
#define PCI1720_SYNCOUT	 9	
#define PCI1720_SYNCONT	15	

#define Syncont_SC0	 1	

static const struct comedi_lrange range_pci1710_3 = { 9, {
							  BIP_RANGE(5),
							  BIP_RANGE(2.5),
							  BIP_RANGE(1.25),
							  BIP_RANGE(0.625),
							  BIP_RANGE(10),
							  UNI_RANGE(10),
							  UNI_RANGE(5),
							  UNI_RANGE(2.5),
							  UNI_RANGE(1.25)
							  }
};

static const char range_codes_pci1710_3[] = { 0x00, 0x01, 0x02, 0x03, 0x04,
					      0x10, 0x11, 0x12, 0x13 };

static const struct comedi_lrange range_pci1710hg = { 12, {
							   BIP_RANGE(5),
							   BIP_RANGE(0.5),
							   BIP_RANGE(0.05),
							   BIP_RANGE(0.005),
							   BIP_RANGE(10),
							   BIP_RANGE(1),
							   BIP_RANGE(0.1),
							   BIP_RANGE(0.01),
							   UNI_RANGE(10),
							   UNI_RANGE(1),
							   UNI_RANGE(0.1),
							   UNI_RANGE(0.01)
							   }
};

static const char range_codes_pci1710hg[] = { 0x00, 0x01, 0x02, 0x03, 0x04,
					      0x05, 0x06, 0x07, 0x10, 0x11,
					      0x12, 0x13 };

static const struct comedi_lrange range_pci17x1 = { 5, {
							BIP_RANGE(10),
							BIP_RANGE(5),
							BIP_RANGE(2.5),
							BIP_RANGE(1.25),
							BIP_RANGE(0.625)
							}
};

static const char range_codes_pci17x1[] = { 0x00, 0x01, 0x02, 0x03, 0x04 };

static const struct comedi_lrange range_pci1720 = { 4, {
							UNI_RANGE(5),
							UNI_RANGE(10),
							BIP_RANGE(5),
							BIP_RANGE(10)
							}
};

static const struct comedi_lrange range_pci171x_da = { 2, {
							   UNI_RANGE(5),
							   UNI_RANGE(10),
							   }
};

static int pci1710_attach(struct comedi_device *dev,
			  struct comedi_devconfig *it);
static int pci1710_detach(struct comedi_device *dev);

struct boardtype {
	const char *name;	
	int device_id;
	int iorange;		
	char have_irq;		
	char cardtype;		
	int n_aichan;		
	int n_aichand;		
	int n_aochan;		
	int n_dichan;		
	int n_dochan;		
	int n_counter;		
	int ai_maxdata;		
	int ao_maxdata;		
	const struct comedi_lrange *rangelist_ai;	
	const char *rangecode_ai;	
	const struct comedi_lrange *rangelist_ao;	
	unsigned int ai_ns_min;	
	unsigned int fifo_half_size;	
};

static DEFINE_PCI_DEVICE_TABLE(pci1710_pci_table) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1710) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1711) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1713) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1720) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1731) },
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, pci1710_pci_table);

static const struct boardtype boardtypes[] = {
	{"pci1710", 0x1710,
	 IORANGE_171x, 1, TYPE_PCI171X,
	 16, 8, 2, 16, 16, 1, 0x0fff, 0x0fff,
	 &range_pci1710_3, range_codes_pci1710_3,
	 &range_pci171x_da,
	 10000, 2048},
	{"pci1710hg", 0x1710,
	 IORANGE_171x, 1, TYPE_PCI171X,
	 16, 8, 2, 16, 16, 1, 0x0fff, 0x0fff,
	 &range_pci1710hg, range_codes_pci1710hg,
	 &range_pci171x_da,
	 10000, 2048},
	{"pci1711", 0x1711,
	 IORANGE_171x, 1, TYPE_PCI171X,
	 16, 0, 2, 16, 16, 1, 0x0fff, 0x0fff,
	 &range_pci17x1, range_codes_pci17x1, &range_pci171x_da,
	 10000, 512},
	{"pci1713", 0x1713,
	 IORANGE_171x, 1, TYPE_PCI1713,
	 32, 16, 0, 0, 0, 0, 0x0fff, 0x0000,
	 &range_pci1710_3, range_codes_pci1710_3, NULL,
	 10000, 2048},
	{"pci1720", 0x1720,
	 IORANGE_1720, 0, TYPE_PCI1720,
	 0, 0, 4, 0, 0, 0, 0x0000, 0x0fff,
	 NULL, NULL, &range_pci1720,
	 0, 0},
	{"pci1731", 0x1731,
	 IORANGE_171x, 1, TYPE_PCI171X,
	 16, 0, 0, 16, 16, 0, 0x0fff, 0x0000,
	 &range_pci17x1, range_codes_pci17x1, NULL,
	 10000, 512},
	
	{.name = DRV_NAME},
};

#define n_boardtypes (sizeof(boardtypes)/sizeof(struct boardtype))

static struct comedi_driver driver_pci1710 = {
	.driver_name = DRV_NAME,
	.module = THIS_MODULE,
	.attach = pci1710_attach,
	.detach = pci1710_detach,
	.num_names = n_boardtypes,
	.board_name = &boardtypes[0].name,
	.offset = sizeof(struct boardtype),
};

struct pci1710_private {
	struct pci_dev *pcidev;	
	char valid;		
	char neverending_ai;	
	unsigned int CntrlReg;	
	unsigned int i8254_osc_base;	
	unsigned int ai_do;	
	unsigned int ai_act_scan;	
	unsigned int ai_act_chan;	
	unsigned int ai_buf_ptr;	
	unsigned char ai_eos;	
	unsigned char ai_et;
	unsigned int ai_et_CntrlReg;
	unsigned int ai_et_MuxVal;
	unsigned int ai_et_div1, ai_et_div2;
	unsigned int act_chanlist[32];	
	unsigned char act_chanlist_len;	
	unsigned char act_chanlist_pos;	
	unsigned char da_ranges;	
	unsigned int ai_scans;	
	unsigned int ai_n_chan;	
	unsigned int *ai_chanlist;	
	unsigned int ai_flags;	
	unsigned int ai_data_len;	
	short *ai_data;		
	unsigned int ai_timer1;	
	unsigned int ai_timer2;
	short ao_data[4];	
	unsigned int cnt0_write_wait;	
};

#define devpriv ((struct pci1710_private *)dev->private)
#define this_board ((const struct boardtype *)dev->board_ptr)


static int check_channel_list(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      unsigned int *chanlist, unsigned int n_chan);
static void setup_channel_list(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       unsigned int *chanlist, unsigned int n_chan,
			       unsigned int seglen);
static void start_pacer(struct comedi_device *dev, int mode,
			unsigned int divisor1, unsigned int divisor2);
static int pci1710_reset(struct comedi_device *dev);
static int pci171x_ai_cancel(struct comedi_device *dev,
			     struct comedi_subdevice *s);

static const unsigned int muxonechan[] = {
	0x0000, 0x0101, 0x0202, 0x0303, 0x0404, 0x0505, 0x0606, 0x0707,
	0x0808, 0x0909, 0x0a0a, 0x0b0b, 0x0c0c, 0x0d0d, 0x0e0e, 0x0f0f,
	0x1010, 0x1111, 0x1212, 0x1313, 0x1414, 0x1515, 0x1616, 0x1717,
	0x1818, 0x1919, 0x1a1a, 0x1b1b, 0x1c1c, 0x1d1d, 0x1e1e, 0x1f1f
};

static int pci171x_insn_read_ai(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data)
{
	int n, timeout;
#ifdef PCI171x_PARANOIDCHECK
	unsigned int idata;
#endif

	DPRINTK("adv_pci1710 EDBG: BGN: pci171x_insn_read_ai(...)\n");
	devpriv->CntrlReg &= Control_CNT0;
	devpriv->CntrlReg |= Control_SW;	
	outw(devpriv->CntrlReg, dev->iobase + PCI171x_CONTROL);
	outb(0, dev->iobase + PCI171x_CLRFIFO);
	outb(0, dev->iobase + PCI171x_CLRINT);

	setup_channel_list(dev, s, &insn->chanspec, 1, 1);

	DPRINTK("adv_pci1710 A ST=%4x IO=%x\n",
		inw(dev->iobase + PCI171x_STATUS),
		dev->iobase + PCI171x_STATUS);
	for (n = 0; n < insn->n; n++) {
		outw(0, dev->iobase + PCI171x_SOFTTRG);	
		DPRINTK("adv_pci1710 B n=%d ST=%4x\n", n,
			inw(dev->iobase + PCI171x_STATUS));
		
		DPRINTK("adv_pci1710 C n=%d ST=%4x\n", n,
			inw(dev->iobase + PCI171x_STATUS));
		timeout = 100;
		while (timeout--) {
			if (!(inw(dev->iobase + PCI171x_STATUS) & Status_FE))
				goto conv_finish;
			if (!(timeout % 10))
				DPRINTK("adv_pci1710 D n=%d tm=%d ST=%4x\n", n,
					timeout,
					inw(dev->iobase + PCI171x_STATUS));
		}
		comedi_error(dev, "A/D insn timeout");
		outb(0, dev->iobase + PCI171x_CLRFIFO);
		outb(0, dev->iobase + PCI171x_CLRINT);
		data[n] = 0;
		DPRINTK
		    ("adv_pci1710 EDBG: END: pci171x_insn_read_ai(...) n=%d\n",
		     n);
		return -ETIME;

conv_finish:
#ifdef PCI171x_PARANOIDCHECK
		idata = inw(dev->iobase + PCI171x_AD_DATA);
		if (this_board->cardtype != TYPE_PCI1713)
			if ((idata & 0xf000) != devpriv->act_chanlist[0]) {
				comedi_error(dev, "A/D insn data droput!");
				return -ETIME;
			}
		data[n] = idata & 0x0fff;
#else
		data[n] = inw(dev->iobase + PCI171x_AD_DATA) & 0x0fff;
#endif

	}

	outb(0, dev->iobase + PCI171x_CLRFIFO);
	outb(0, dev->iobase + PCI171x_CLRINT);

	DPRINTK("adv_pci1710 EDBG: END: pci171x_insn_read_ai(...) n=%d\n", n);
	return n;
}

static int pci171x_insn_write_ao(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 struct comedi_insn *insn, unsigned int *data)
{
	int n, chan, range, ofs;

	chan = CR_CHAN(insn->chanspec);
	range = CR_RANGE(insn->chanspec);
	if (chan) {
		devpriv->da_ranges &= 0xfb;
		devpriv->da_ranges |= (range << 2);
		outw(devpriv->da_ranges, dev->iobase + PCI171x_DAREF);
		ofs = PCI171x_DA2;
	} else {
		devpriv->da_ranges &= 0xfe;
		devpriv->da_ranges |= range;
		outw(devpriv->da_ranges, dev->iobase + PCI171x_DAREF);
		ofs = PCI171x_DA1;
	}

	for (n = 0; n < insn->n; n++)
		outw(data[n], dev->iobase + ofs);

	devpriv->ao_data[chan] = data[n];

	return n;

}

static int pci171x_insn_read_ao(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data)
{
	int n, chan;

	chan = CR_CHAN(insn->chanspec);
	for (n = 0; n < insn->n; n++)
		data[n] = devpriv->ao_data[chan];

	return n;
}

static int pci171x_insn_bits_di(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data)
{
	data[1] = inw(dev->iobase + PCI171x_DI);

	return 2;
}

static int pci171x_insn_bits_do(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data)
{
	if (data[0]) {
		s->state &= ~data[0];
		s->state |= (data[0] & data[1]);
		outw(s->state, dev->iobase + PCI171x_DO);
	}
	data[1] = s->state;

	return 2;
}

static int pci171x_insn_counter_read(struct comedi_device *dev,
				     struct comedi_subdevice *s,
				     struct comedi_insn *insn,
				     unsigned int *data)
{
	unsigned int msb, lsb, ccntrl;
	int i;

	ccntrl = 0xD2;		
	for (i = 0; i < insn->n; i++) {
		outw(ccntrl, dev->iobase + PCI171x_CNTCTRL);

		lsb = inw(dev->iobase + PCI171x_CNT0) & 0xFF;
		msb = inw(dev->iobase + PCI171x_CNT0) & 0xFF;

		data[0] = lsb | (msb << 8);
	}

	return insn->n;
}

static int pci171x_insn_counter_write(struct comedi_device *dev,
				      struct comedi_subdevice *s,
				      struct comedi_insn *insn,
				      unsigned int *data)
{
	uint msb, lsb, ccntrl, status;

	lsb = data[0] & 0x00FF;
	msb = (data[0] & 0xFF00) >> 8;

	
	outw(lsb, dev->iobase + PCI171x_CNT0);
	outw(msb, dev->iobase + PCI171x_CNT0);

	if (devpriv->cnt0_write_wait) {
		
		ccntrl = 0xE2;
		do {
			outw(ccntrl, dev->iobase + PCI171x_CNTCTRL);
			status = inw(dev->iobase + PCI171x_CNT0) & 0xFF;
		} while (status & 0x40);
	}

	return insn->n;
}

static int pci171x_insn_counter_config(struct comedi_device *dev,
				       struct comedi_subdevice *s,
				       struct comedi_insn *insn,
				       unsigned int *data)
{
#ifdef unused
	
	uint ccntrl = 0;

	devpriv->cnt0_write_wait = data[0] & 0x20;

	
	if (!(data[0] & 0x10)) {	
		devpriv->CntrlReg &= ~Control_CNT0;
	} else {
		devpriv->CntrlReg |= Control_CNT0;
	}
	outw(devpriv->CntrlReg, dev->iobase + PCI171x_CONTROL);

	if (data[0] & 0x01)
		ccntrl |= Counter_M0;
	if (data[0] & 0x02)
		ccntrl |= Counter_M1;
	if (data[0] & 0x04)
		ccntrl |= Counter_M2;
	if (data[0] & 0x08)
		ccntrl |= Counter_BCD;
	ccntrl |= Counter_RW0;	
	ccntrl |= Counter_RW1;
	outw(ccntrl, dev->iobase + PCI171x_CNTCTRL);
#endif

	return 1;
}

static int pci1720_insn_write_ao(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 struct comedi_insn *insn, unsigned int *data)
{
	int n, rangereg, chan;

	chan = CR_CHAN(insn->chanspec);
	rangereg = devpriv->da_ranges & (~(0x03 << (chan << 1)));
	rangereg |= (CR_RANGE(insn->chanspec) << (chan << 1));
	if (rangereg != devpriv->da_ranges) {
		outb(rangereg, dev->iobase + PCI1720_RANGE);
		devpriv->da_ranges = rangereg;
	}

	for (n = 0; n < insn->n; n++) {
		outw(data[n], dev->iobase + PCI1720_DA0 + (chan << 1));
		outb(0, dev->iobase + PCI1720_SYNCOUT);	
	}

	devpriv->ao_data[chan] = data[n];

	return n;
}

static void interrupt_pci1710_every_sample(void *d)
{
	struct comedi_device *dev = d;
	struct comedi_subdevice *s = dev->subdevices + 0;
	int m;
#ifdef PCI171x_PARANOIDCHECK
	short sampl;
#endif

	DPRINTK("adv_pci1710 EDBG: BGN: interrupt_pci1710_every_sample(...)\n");
	m = inw(dev->iobase + PCI171x_STATUS);
	if (m & Status_FE) {
		printk("comedi%d: A/D FIFO empty (%4x)\n", dev->minor, m);
		pci171x_ai_cancel(dev, s);
		s->async->events |= COMEDI_CB_EOA | COMEDI_CB_ERROR;
		comedi_event(dev, s);
		return;
	}
	if (m & Status_FF) {
		printk
		    ("comedi%d: A/D FIFO Full status (Fatal Error!) (%4x)\n",
		     dev->minor, m);
		pci171x_ai_cancel(dev, s);
		s->async->events |= COMEDI_CB_EOA | COMEDI_CB_ERROR;
		comedi_event(dev, s);
		return;
	}

	outb(0, dev->iobase + PCI171x_CLRINT);	

	DPRINTK("FOR ");
	for (; !(inw(dev->iobase + PCI171x_STATUS) & Status_FE);) {
#ifdef PCI171x_PARANOIDCHECK
		sampl = inw(dev->iobase + PCI171x_AD_DATA);
		DPRINTK("%04x:", sampl);
		if (this_board->cardtype != TYPE_PCI1713)
			if ((sampl & 0xf000) !=
			    devpriv->act_chanlist[s->async->cur_chan]) {
				printk
				    ("comedi: A/D data dropout: received data from channel %d, expected %d!\n",
				     (sampl & 0xf000) >> 12,
				     (devpriv->
				      act_chanlist[s->
						   async->cur_chan] & 0xf000) >>
				     12);
				pci171x_ai_cancel(dev, s);
				s->async->events |=
				    COMEDI_CB_EOA | COMEDI_CB_ERROR;
				comedi_event(dev, s);
				return;
			}
		DPRINTK("%8d %2d %8d~", s->async->buf_int_ptr,
			s->async->cur_chan, s->async->buf_int_count);
		comedi_buf_put(s->async, sampl & 0x0fff);
#else
		comedi_buf_put(s->async,
			       inw(dev->iobase + PCI171x_AD_DATA) & 0x0fff);
#endif
		++s->async->cur_chan;

		if (s->async->cur_chan >= devpriv->ai_n_chan)
			s->async->cur_chan = 0;


		if (s->async->cur_chan == 0) {	
			devpriv->ai_act_scan++;
			DPRINTK
			    ("adv_pci1710 EDBG: EOS1 bic %d bip %d buc %d bup %d\n",
			     s->async->buf_int_count, s->async->buf_int_ptr,
			     s->async->buf_user_count, s->async->buf_user_ptr);
			DPRINTK("adv_pci1710 EDBG: EOS2\n");
			if ((!devpriv->neverending_ai) && (devpriv->ai_act_scan >= devpriv->ai_scans)) {	
				pci171x_ai_cancel(dev, s);
				s->async->events |= COMEDI_CB_EOA;
				comedi_event(dev, s);
				return;
			}
		}
	}

	outb(0, dev->iobase + PCI171x_CLRINT);	
	DPRINTK("adv_pci1710 EDBG: END: interrupt_pci1710_every_sample(...)\n");

	comedi_event(dev, s);
}

static int move_block_from_fifo(struct comedi_device *dev,
				struct comedi_subdevice *s, int n, int turn)
{
	int i, j;
#ifdef PCI171x_PARANOIDCHECK
	int sampl;
#endif
	DPRINTK("adv_pci1710 EDBG: BGN: move_block_from_fifo(...,%d,%d)\n", n,
		turn);
	j = s->async->cur_chan;
	for (i = 0; i < n; i++) {
#ifdef PCI171x_PARANOIDCHECK
		sampl = inw(dev->iobase + PCI171x_AD_DATA);
		if (this_board->cardtype != TYPE_PCI1713)
			if ((sampl & 0xf000) != devpriv->act_chanlist[j]) {
				printk
				    ("comedi%d: A/D  FIFO data dropout: received data from channel %d, expected %d! (%d/%d/%d/%d/%d/%4x)\n",
				     dev->minor, (sampl & 0xf000) >> 12,
				     (devpriv->act_chanlist[j] & 0xf000) >> 12,
				     i, j, devpriv->ai_act_scan, n, turn,
				     sampl);
				pci171x_ai_cancel(dev, s);
				s->async->events |=
				    COMEDI_CB_EOA | COMEDI_CB_ERROR;
				comedi_event(dev, s);
				return 1;
			}
		comedi_buf_put(s->async, sampl & 0x0fff);
#else
		comedi_buf_put(s->async,
			       inw(dev->iobase + PCI171x_AD_DATA) & 0x0fff);
#endif
		j++;
		if (j >= devpriv->ai_n_chan) {
			j = 0;
			devpriv->ai_act_scan++;
		}
	}
	s->async->cur_chan = j;
	DPRINTK("adv_pci1710 EDBG: END: move_block_from_fifo(...)\n");
	return 0;
}

static void interrupt_pci1710_half_fifo(void *d)
{
	struct comedi_device *dev = d;
	struct comedi_subdevice *s = dev->subdevices + 0;
	int m, samplesinbuf;

	DPRINTK("adv_pci1710 EDBG: BGN: interrupt_pci1710_half_fifo(...)\n");
	m = inw(dev->iobase + PCI171x_STATUS);
	if (!(m & Status_FH)) {
		printk("comedi%d: A/D FIFO not half full! (%4x)\n",
		       dev->minor, m);
		pci171x_ai_cancel(dev, s);
		s->async->events |= COMEDI_CB_EOA | COMEDI_CB_ERROR;
		comedi_event(dev, s);
		return;
	}
	if (m & Status_FF) {
		printk
		    ("comedi%d: A/D FIFO Full status (Fatal Error!) (%4x)\n",
		     dev->minor, m);
		pci171x_ai_cancel(dev, s);
		s->async->events |= COMEDI_CB_EOA | COMEDI_CB_ERROR;
		comedi_event(dev, s);
		return;
	}

	samplesinbuf = this_board->fifo_half_size;
	if (samplesinbuf * sizeof(short) >= devpriv->ai_data_len) {
		m = devpriv->ai_data_len / sizeof(short);
		if (move_block_from_fifo(dev, s, m, 0))
			return;
		samplesinbuf -= m;
	}

	if (samplesinbuf) {
		if (move_block_from_fifo(dev, s, samplesinbuf, 1))
			return;
	}

	if (!devpriv->neverending_ai)
		if (devpriv->ai_act_scan >= devpriv->ai_scans) { 
			pci171x_ai_cancel(dev, s);
			s->async->events |= COMEDI_CB_EOA;
			comedi_event(dev, s);
			return;
		}
	outb(0, dev->iobase + PCI171x_CLRINT);	
	DPRINTK("adv_pci1710 EDBG: END: interrupt_pci1710_half_fifo(...)\n");

	comedi_event(dev, s);
}

static irqreturn_t interrupt_service_pci1710(int irq, void *d)
{
	struct comedi_device *dev = d;

	DPRINTK("adv_pci1710 EDBG: BGN: interrupt_service_pci1710(%d,...)\n",
		irq);
	if (!dev->attached)	
		return IRQ_NONE;	

	if (!(inw(dev->iobase + PCI171x_STATUS) & Status_IRQ))	
		return IRQ_NONE;	

	DPRINTK("adv_pci1710 EDBG: interrupt_service_pci1710() ST: %4x\n",
		inw(dev->iobase + PCI171x_STATUS));

	if (devpriv->ai_et) {	
		devpriv->ai_et = 0;
		devpriv->CntrlReg &= Control_CNT0;
		devpriv->CntrlReg |= Control_SW;	
		outw(devpriv->CntrlReg, dev->iobase + PCI171x_CONTROL);
		devpriv->CntrlReg = devpriv->ai_et_CntrlReg;
		outb(0, dev->iobase + PCI171x_CLRFIFO);
		outb(0, dev->iobase + PCI171x_CLRINT);
		outw(devpriv->ai_et_MuxVal, dev->iobase + PCI171x_MUX);
		outw(devpriv->CntrlReg, dev->iobase + PCI171x_CONTROL);
		
		start_pacer(dev, 1, devpriv->ai_et_div1, devpriv->ai_et_div2);
		return IRQ_HANDLED;
	}
	if (devpriv->ai_eos) {	
		interrupt_pci1710_every_sample(d);
	} else {
		interrupt_pci1710_half_fifo(d);
	}
	DPRINTK("adv_pci1710 EDBG: END: interrupt_service_pci1710(...)\n");
	return IRQ_HANDLED;
}

static int pci171x_ai_docmd_and_mode(int mode, struct comedi_device *dev,
				     struct comedi_subdevice *s)
{
	unsigned int divisor1 = 0, divisor2 = 0;
	unsigned int seglen;

	DPRINTK("adv_pci1710 EDBG: BGN: pci171x_ai_docmd_and_mode(%d,...)\n",
		mode);
	start_pacer(dev, -1, 0, 0);	

	seglen = check_channel_list(dev, s, devpriv->ai_chanlist,
				    devpriv->ai_n_chan);
	if (seglen < 1)
		return -EINVAL;
	setup_channel_list(dev, s, devpriv->ai_chanlist,
			   devpriv->ai_n_chan, seglen);

	outb(0, dev->iobase + PCI171x_CLRFIFO);
	outb(0, dev->iobase + PCI171x_CLRINT);

	devpriv->ai_do = mode;

	devpriv->ai_act_scan = 0;
	s->async->cur_chan = 0;
	devpriv->ai_buf_ptr = 0;
	devpriv->neverending_ai = 0;

	devpriv->CntrlReg &= Control_CNT0;
	if ((devpriv->ai_flags & TRIG_WAKE_EOS)) {	
		devpriv->ai_eos = 1;
	} else {
		devpriv->CntrlReg |= Control_ONEFH;
		devpriv->ai_eos = 0;
	}

	if ((devpriv->ai_scans == 0) || (devpriv->ai_scans == -1))
		devpriv->neverending_ai = 1;
	
	else
		devpriv->neverending_ai = 0;

	switch (mode) {
	case 1:
	case 2:
		if (devpriv->ai_timer1 < this_board->ai_ns_min)
			devpriv->ai_timer1 = this_board->ai_ns_min;
		devpriv->CntrlReg |= Control_PACER | Control_IRQEN;
		if (mode == 2) {
			devpriv->ai_et_CntrlReg = devpriv->CntrlReg;
			devpriv->CntrlReg &=
			    ~(Control_PACER | Control_ONEFH | Control_GATE);
			devpriv->CntrlReg |= Control_EXT;
			devpriv->ai_et = 1;
		} else {
			devpriv->ai_et = 0;
		}
		i8253_cascade_ns_to_timer(devpriv->i8254_osc_base, &divisor1,
					  &divisor2, &devpriv->ai_timer1,
					  devpriv->ai_flags & TRIG_ROUND_MASK);
		DPRINTK
		    ("adv_pci1710 EDBG: OSC base=%u div1=%u div2=%u timer=%u\n",
		     devpriv->i8254_osc_base, divisor1, divisor2,
		     devpriv->ai_timer1);
		outw(devpriv->CntrlReg, dev->iobase + PCI171x_CONTROL);
		if (mode != 2) {
			
			start_pacer(dev, mode, divisor1, divisor2);
		} else {
			devpriv->ai_et_div1 = divisor1;
			devpriv->ai_et_div2 = divisor2;
		}
		break;
	case 3:
		devpriv->CntrlReg |= Control_EXT | Control_IRQEN;
		outw(devpriv->CntrlReg, dev->iobase + PCI171x_CONTROL);
		break;
	}

	DPRINTK("adv_pci1710 EDBG: END: pci171x_ai_docmd_and_mode(...)\n");
	return 0;
}

#ifdef PCI171X_EXTDEBUG
static void pci171x_cmdtest_out(int e, struct comedi_cmd *cmd)
{
	printk("adv_pci1710 e=%d startsrc=%x scansrc=%x convsrc=%x\n", e,
	       cmd->start_src, cmd->scan_begin_src, cmd->convert_src);
	printk("adv_pci1710 e=%d startarg=%d scanarg=%d convarg=%d\n", e,
	       cmd->start_arg, cmd->scan_begin_arg, cmd->convert_arg);
	printk("adv_pci1710 e=%d stopsrc=%x scanend=%x\n", e, cmd->stop_src,
	       cmd->scan_end_src);
	printk("adv_pci1710 e=%d stoparg=%d scanendarg=%d chanlistlen=%d\n",
	       e, cmd->stop_arg, cmd->scan_end_arg, cmd->chanlist_len);
}
#endif

static int pci171x_ai_cmdtest(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_cmd *cmd)
{
	int err = 0;
	int tmp;
	unsigned int divisor1 = 0, divisor2 = 0;

	DPRINTK("adv_pci1710 EDBG: BGN: pci171x_ai_cmdtest(...)\n");
#ifdef PCI171X_EXTDEBUG
	pci171x_cmdtest_out(-1, cmd);
#endif
	

	tmp = cmd->start_src;
	cmd->start_src &= TRIG_NOW | TRIG_EXT;
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

	if (err) {
#ifdef PCI171X_EXTDEBUG
		pci171x_cmdtest_out(1, cmd);
#endif
		DPRINTK
		    ("adv_pci1710 EDBG: BGN: pci171x_ai_cmdtest(...) err=%d ret=1\n",
		     err);
		return 1;
	}

	

	if (cmd->start_src != TRIG_NOW && cmd->start_src != TRIG_EXT) {
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

	if (err) {
#ifdef PCI171X_EXTDEBUG
		pci171x_cmdtest_out(2, cmd);
#endif
		DPRINTK
		    ("adv_pci1710 EDBG: BGN: pci171x_ai_cmdtest(...) err=%d ret=2\n",
		     err);
		return 2;
	}

	

	if (cmd->start_arg != 0) {
		cmd->start_arg = 0;
		err++;
	}

	if (cmd->scan_begin_arg != 0) {
		cmd->scan_begin_arg = 0;
		err++;
	}

	if (cmd->convert_src == TRIG_TIMER) {
		if (cmd->convert_arg < this_board->ai_ns_min) {
			cmd->convert_arg = this_board->ai_ns_min;
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

	if (err) {
#ifdef PCI171X_EXTDEBUG
		pci171x_cmdtest_out(3, cmd);
#endif
		DPRINTK
		    ("adv_pci1710 EDBG: BGN: pci171x_ai_cmdtest(...) err=%d ret=3\n",
		     err);
		return 3;
	}

	

	if (cmd->convert_src == TRIG_TIMER) {
		tmp = cmd->convert_arg;
		i8253_cascade_ns_to_timer(devpriv->i8254_osc_base, &divisor1,
					  &divisor2, &cmd->convert_arg,
					  cmd->flags & TRIG_ROUND_MASK);
		if (cmd->convert_arg < this_board->ai_ns_min)
			cmd->convert_arg = this_board->ai_ns_min;
		if (tmp != cmd->convert_arg)
			err++;
	}

	if (err) {
		DPRINTK
		    ("adv_pci1710 EDBG: BGN: pci171x_ai_cmdtest(...) err=%d ret=4\n",
		     err);
		return 4;
	}

	

	if (cmd->chanlist) {
		if (!check_channel_list(dev, s, cmd->chanlist,
					cmd->chanlist_len))
			return 5;	
	}

	DPRINTK("adv_pci1710 EDBG: BGN: pci171x_ai_cmdtest(...) ret=0\n");
	return 0;
}

static int pci171x_ai_cmd(struct comedi_device *dev, struct comedi_subdevice *s)
{
	struct comedi_cmd *cmd = &s->async->cmd;

	DPRINTK("adv_pci1710 EDBG: BGN: pci171x_ai_cmd(...)\n");
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
			return pci171x_ai_docmd_and_mode(cmd->start_src ==
							 TRIG_EXT ? 2 : 1, dev,
							 s);
		}
		if (cmd->convert_src == TRIG_EXT) {	
			return pci171x_ai_docmd_and_mode(3, dev, s);
		}
	}

	return -1;
}

static int check_channel_list(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      unsigned int *chanlist, unsigned int n_chan)
{
	unsigned int chansegment[32];
	unsigned int i, nowmustbechan, seglen, segpos;

	DPRINTK("adv_pci1710 EDBG:  check_channel_list(...,%d)\n", n_chan);
	
	if (n_chan < 1) {
		comedi_error(dev, "range/channel list is empty!");
		return 0;
	}

	if (n_chan > 1) {
		chansegment[0] = chanlist[0];	
		for (i = 1, seglen = 1; i < n_chan; i++, seglen++) {	
			
			if (chanlist[0] == chanlist[i])
				break;	
			if (CR_CHAN(chanlist[i]) & 1)	
				if (CR_AREF(chanlist[i]) == AREF_DIFF) {
					comedi_error(dev,
						     "Odd channel can't be differential input!\n");
					return 0;
				}
			nowmustbechan =
			    (CR_CHAN(chansegment[i - 1]) + 1) % s->n_chan;
			if (CR_AREF(chansegment[i - 1]) == AREF_DIFF)
				nowmustbechan = (nowmustbechan + 1) % s->n_chan;
			if (nowmustbechan != CR_CHAN(chanlist[i])) {	
				printk
				    ("channel list must be continuous! chanlist[%i]=%d but must be %d or %d!\n",
				     i, CR_CHAN(chanlist[i]), nowmustbechan,
				     CR_CHAN(chanlist[0]));
				return 0;
			}
			chansegment[i] = chanlist[i];	
		}

		for (i = 0, segpos = 0; i < n_chan; i++) {	
			
			if (chanlist[i] != chansegment[i % seglen]) {
				printk
				    ("bad channel, reference or range number! chanlist[%i]=%d,%d,%d and not %d,%d,%d!\n",
				     i, CR_CHAN(chansegment[i]),
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
	return seglen;
}

static void setup_channel_list(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       unsigned int *chanlist, unsigned int n_chan,
			       unsigned int seglen)
{
	unsigned int i, range, chanprog;

	DPRINTK("adv_pci1710 EDBG:  setup_channel_list(...,%d,%d)\n", n_chan,
		seglen);
	devpriv->act_chanlist_len = seglen;
	devpriv->act_chanlist_pos = 0;

	DPRINTK("SegLen: %d\n", seglen);
	for (i = 0; i < seglen; i++) {	
		chanprog = muxonechan[CR_CHAN(chanlist[i])];
		outw(chanprog, dev->iobase + PCI171x_MUX);	
		range = this_board->rangecode_ai[CR_RANGE(chanlist[i])];
		if (CR_AREF(chanlist[i]) == AREF_DIFF)
			range |= 0x0020;
		outw(range, dev->iobase + PCI171x_RANGE);	
#ifdef PCI171x_PARANOIDCHECK
		devpriv->act_chanlist[i] =
		    (CR_CHAN(chanlist[i]) << 12) & 0xf000;
#endif
		DPRINTK("GS: %2d. [%4x]=%4x %4x\n", i, chanprog, range,
			devpriv->act_chanlist[i]);
	}
#ifdef PCI171x_PARANOIDCHECK
	for ( ; i < n_chan; i++) { 
		devpriv->act_chanlist[i] =
		    (CR_CHAN(chanlist[i]) << 12) & 0xf000;
	}
#endif

	devpriv->ai_et_MuxVal =
	    CR_CHAN(chanlist[0]) | (CR_CHAN(chanlist[seglen - 1]) << 8);
	outw(devpriv->ai_et_MuxVal, dev->iobase + PCI171x_MUX);	
	DPRINTK("MUX: %4x L%4x.H%4x\n",
		CR_CHAN(chanlist[0]) | (CR_CHAN(chanlist[seglen - 1]) << 8),
		CR_CHAN(chanlist[0]), CR_CHAN(chanlist[seglen - 1]));
}

static void start_pacer(struct comedi_device *dev, int mode,
			unsigned int divisor1, unsigned int divisor2)
{
	DPRINTK("adv_pci1710 EDBG: BGN: start_pacer(%d,%u,%u)\n", mode,
		divisor1, divisor2);
	outw(0xb4, dev->iobase + PCI171x_CNTCTRL);
	outw(0x74, dev->iobase + PCI171x_CNTCTRL);

	if (mode == 1) {
		outw(divisor2 & 0xff, dev->iobase + PCI171x_CNT2);
		outw((divisor2 >> 8) & 0xff, dev->iobase + PCI171x_CNT2);
		outw(divisor1 & 0xff, dev->iobase + PCI171x_CNT1);
		outw((divisor1 >> 8) & 0xff, dev->iobase + PCI171x_CNT1);
	}
	DPRINTK("adv_pci1710 EDBG: END: start_pacer(...)\n");
}

static int pci171x_ai_cancel(struct comedi_device *dev,
			     struct comedi_subdevice *s)
{
	DPRINTK("adv_pci1710 EDBG: BGN: pci171x_ai_cancel(...)\n");

	switch (this_board->cardtype) {
	default:
		devpriv->CntrlReg &= Control_CNT0;
		devpriv->CntrlReg |= Control_SW;

		outw(devpriv->CntrlReg, dev->iobase + PCI171x_CONTROL);	
		start_pacer(dev, -1, 0, 0);
		outb(0, dev->iobase + PCI171x_CLRFIFO);
		outb(0, dev->iobase + PCI171x_CLRINT);
		break;
	}

	devpriv->ai_do = 0;
	devpriv->ai_act_scan = 0;
	s->async->cur_chan = 0;
	devpriv->ai_buf_ptr = 0;
	devpriv->neverending_ai = 0;

	DPRINTK("adv_pci1710 EDBG: END: pci171x_ai_cancel(...)\n");
	return 0;
}

static int pci171x_reset(struct comedi_device *dev)
{
	DPRINTK("adv_pci1710 EDBG: BGN: pci171x_reset(...)\n");
	outw(0x30, dev->iobase + PCI171x_CNTCTRL);
	devpriv->CntrlReg = Control_SW | Control_CNT0;	
	outw(devpriv->CntrlReg, dev->iobase + PCI171x_CONTROL);	
	outb(0, dev->iobase + PCI171x_CLRFIFO);	
	outb(0, dev->iobase + PCI171x_CLRINT);	
	start_pacer(dev, -1, 0, 0);	
	devpriv->da_ranges = 0;
	if (this_board->n_aochan) {
		outb(devpriv->da_ranges, dev->iobase + PCI171x_DAREF);	
		outw(0, dev->iobase + PCI171x_DA1);	
		devpriv->ao_data[0] = 0x0000;
		if (this_board->n_aochan > 1) {
			outw(0, dev->iobase + PCI171x_DA2);
			devpriv->ao_data[1] = 0x0000;
		}
	}
	outw(0, dev->iobase + PCI171x_DO);	
	outb(0, dev->iobase + PCI171x_CLRFIFO);	
	outb(0, dev->iobase + PCI171x_CLRINT);	

	DPRINTK("adv_pci1710 EDBG: END: pci171x_reset(...)\n");
	return 0;
}

static int pci1720_reset(struct comedi_device *dev)
{
	DPRINTK("adv_pci1710 EDBG: BGN: pci1720_reset(...)\n");
	outb(Syncont_SC0, dev->iobase + PCI1720_SYNCONT);	
	devpriv->da_ranges = 0xAA;
	outb(devpriv->da_ranges, dev->iobase + PCI1720_RANGE);	
	outw(0x0800, dev->iobase + PCI1720_DA0);	
	outw(0x0800, dev->iobase + PCI1720_DA1);
	outw(0x0800, dev->iobase + PCI1720_DA2);
	outw(0x0800, dev->iobase + PCI1720_DA3);
	outb(0, dev->iobase + PCI1720_SYNCOUT);	
	devpriv->ao_data[0] = 0x0800;
	devpriv->ao_data[1] = 0x0800;
	devpriv->ao_data[2] = 0x0800;
	devpriv->ao_data[3] = 0x0800;
	DPRINTK("adv_pci1710 EDBG: END: pci1720_reset(...)\n");
	return 0;
}

static int pci1710_reset(struct comedi_device *dev)
{
	DPRINTK("adv_pci1710 EDBG: BGN: pci1710_reset(...)\n");
	switch (this_board->cardtype) {
	case TYPE_PCI1720:
		return pci1720_reset(dev);
	default:
		return pci171x_reset(dev);
	}
	DPRINTK("adv_pci1710 EDBG: END: pci1710_reset(...)\n");
}

static int pci1710_attach(struct comedi_device *dev,
			  struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	int ret, subdev, n_subdevices;
	unsigned int irq;
	unsigned long iobase;
	struct pci_dev *pcidev;
	int opt_bus, opt_slot;
	const char *errstr;
	unsigned char pci_bus, pci_slot, pci_func;
	int i;
	int board_index;

	dev_info(dev->hw_dev, "comedi%d: adv_pci1710:\n", dev->minor);

	opt_bus = it->options[0];
	opt_slot = it->options[1];

	ret = alloc_private(dev, sizeof(struct pci1710_private));
	if (ret < 0)
		return -ENOMEM;

	
	errstr = "not found!";
	pcidev = NULL;
	board_index = this_board - boardtypes;
	while (NULL != (pcidev = pci_get_device(PCI_VENDOR_ID_ADVANTECH,
						PCI_ANY_ID, pcidev))) {
		if (strcmp(this_board->name, DRV_NAME) == 0) {
			for (i = 0; i < n_boardtypes; ++i) {
				if (pcidev->device == boardtypes[i].device_id) {
					board_index = i;
					break;
				}
			}
			if (i == n_boardtypes)
				continue;
		} else {
			if (pcidev->device != boardtypes[board_index].device_id)
				continue;
		}

		
		if (opt_bus || opt_slot) {
			
			if (opt_bus != pcidev->bus->number
			    || opt_slot != PCI_SLOT(pcidev->devfn))
				continue;	
		}
		if (comedi_pci_enable(pcidev, DRV_NAME)) {
			errstr =
			    "failed to enable PCI device and request regions!";
			continue;
		}
		
		dev->board_ptr = &boardtypes[board_index];
		break;
	}

	if (!pcidev) {
		if (opt_bus || opt_slot) {
			dev_err(dev->hw_dev, "- Card at b:s %d:%d %s\n",
				opt_bus, opt_slot, errstr);
		} else {
			dev_err(dev->hw_dev, "- Card %s\n", errstr);
		}
		return -EIO;
	}

	pci_bus = pcidev->bus->number;
	pci_slot = PCI_SLOT(pcidev->devfn);
	pci_func = PCI_FUNC(pcidev->devfn);
	irq = pcidev->irq;
	iobase = pci_resource_start(pcidev, 2);

	dev_dbg(dev->hw_dev, "b:s:f=%d:%d:%d, io=0x%4lx\n", pci_bus, pci_slot,
		pci_func, iobase);

	dev->iobase = iobase;

	dev->board_name = this_board->name;
	devpriv->pcidev = pcidev;

	n_subdevices = 0;
	if (this_board->n_aichan)
		n_subdevices++;
	if (this_board->n_aochan)
		n_subdevices++;
	if (this_board->n_dichan)
		n_subdevices++;
	if (this_board->n_dochan)
		n_subdevices++;
	if (this_board->n_counter)
		n_subdevices++;

	ret = alloc_subdevices(dev, n_subdevices);
	if (ret < 0)
		return ret;

	pci1710_reset(dev);

	if (this_board->have_irq) {
		if (irq) {
			if (request_irq(irq, interrupt_service_pci1710,
					IRQF_SHARED, "Advantech PCI-1710",
					dev)) {
				dev_dbg(dev->hw_dev, "unable to allocate IRQ %d, DISABLING IT",
					irq);
				irq = 0;	
			} else {
				dev_dbg(dev->hw_dev, "irq=%u", irq);
			}
		} else {
			dev_dbg(dev->hw_dev, "IRQ disabled");
		}
	} else {
		irq = 0;
	}

	dev->irq = irq;
	subdev = 0;

	if (this_board->n_aichan) {
		s = dev->subdevices + subdev;
		dev->read_subdev = s;
		s->type = COMEDI_SUBD_AI;
		s->subdev_flags = SDF_READABLE | SDF_COMMON | SDF_GROUND;
		if (this_board->n_aichand)
			s->subdev_flags |= SDF_DIFF;
		s->n_chan = this_board->n_aichan;
		s->maxdata = this_board->ai_maxdata;
		s->len_chanlist = this_board->n_aichan;
		s->range_table = this_board->rangelist_ai;
		s->cancel = pci171x_ai_cancel;
		s->insn_read = pci171x_insn_read_ai;
		if (irq) {
			s->subdev_flags |= SDF_CMD_READ;
			s->do_cmdtest = pci171x_ai_cmdtest;
			s->do_cmd = pci171x_ai_cmd;
		}
		devpriv->i8254_osc_base = 100;	
		subdev++;
	}

	if (this_board->n_aochan) {
		s = dev->subdevices + subdev;
		s->type = COMEDI_SUBD_AO;
		s->subdev_flags = SDF_WRITABLE | SDF_GROUND | SDF_COMMON;
		s->n_chan = this_board->n_aochan;
		s->maxdata = this_board->ao_maxdata;
		s->len_chanlist = this_board->n_aochan;
		s->range_table = this_board->rangelist_ao;
		switch (this_board->cardtype) {
		case TYPE_PCI1720:
			s->insn_write = pci1720_insn_write_ao;
			break;
		default:
			s->insn_write = pci171x_insn_write_ao;
			break;
		}
		s->insn_read = pci171x_insn_read_ao;
		subdev++;
	}

	if (this_board->n_dichan) {
		s = dev->subdevices + subdev;
		s->type = COMEDI_SUBD_DI;
		s->subdev_flags = SDF_READABLE | SDF_GROUND | SDF_COMMON;
		s->n_chan = this_board->n_dichan;
		s->maxdata = 1;
		s->len_chanlist = this_board->n_dichan;
		s->range_table = &range_digital;
		s->io_bits = 0;	
		s->insn_bits = pci171x_insn_bits_di;
		subdev++;
	}

	if (this_board->n_dochan) {
		s = dev->subdevices + subdev;
		s->type = COMEDI_SUBD_DO;
		s->subdev_flags = SDF_WRITABLE | SDF_GROUND | SDF_COMMON;
		s->n_chan = this_board->n_dochan;
		s->maxdata = 1;
		s->len_chanlist = this_board->n_dochan;
		s->range_table = &range_digital;
		
		s->io_bits = (1 << this_board->n_dochan) - 1;
		s->state = 0;
		s->insn_bits = pci171x_insn_bits_do;
		subdev++;
	}

	if (this_board->n_counter) {
		s = dev->subdevices + subdev;
		s->type = COMEDI_SUBD_COUNTER;
		s->subdev_flags = SDF_READABLE | SDF_WRITABLE;
		s->n_chan = this_board->n_counter;
		s->len_chanlist = this_board->n_counter;
		s->maxdata = 0xffff;
		s->range_table = &range_unknown;
		s->insn_read = pci171x_insn_counter_read;
		s->insn_write = pci171x_insn_counter_write;
		s->insn_config = pci171x_insn_counter_config;
		subdev++;
	}

	devpriv->valid = 1;

	return 0;
}

static int pci1710_detach(struct comedi_device *dev)
{

	if (dev->private) {
		if (devpriv->valid)
			pci1710_reset(dev);
		if (dev->irq)
			free_irq(dev->irq, dev);
		if (devpriv->pcidev) {
			if (dev->iobase)
				comedi_pci_disable(devpriv->pcidev);

			pci_dev_put(devpriv->pcidev);
		}
	}

	return 0;
}

static int __devinit driver_pci1710_pci_probe(struct pci_dev *dev,
					      const struct pci_device_id *ent)
{
	return comedi_pci_auto_config(dev, driver_pci1710.driver_name);
}

static void __devexit driver_pci1710_pci_remove(struct pci_dev *dev)
{
	comedi_pci_auto_unconfig(dev);
}

static struct pci_driver driver_pci1710_pci_driver = {
	.id_table = pci1710_pci_table,
	.probe = &driver_pci1710_pci_probe,
	.remove = __devexit_p(&driver_pci1710_pci_remove)
};

static int __init driver_pci1710_init_module(void)
{
	int retval;

	retval = comedi_driver_register(&driver_pci1710);
	if (retval < 0)
		return retval;

	driver_pci1710_pci_driver.name = (char *)driver_pci1710.driver_name;
	return pci_register_driver(&driver_pci1710_pci_driver);
}

static void __exit driver_pci1710_cleanup_module(void)
{
	pci_unregister_driver(&driver_pci1710_pci_driver);
	comedi_driver_unregister(&driver_pci1710);
}

module_init(driver_pci1710_init_module);
module_exit(driver_pci1710_cleanup_module);

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
