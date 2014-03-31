/*
    comedi/drivers/rtd520.c
    Comedi driver for Real Time Devices (RTD) PCI4520/DM7520

    COMEDI - Linux Control and Measurement Device Interface
    Copyright (C) 2001 David A. Schleef <ds@schleef.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#include <linux/interrupt.h>
#include <linux/delay.h>

#include "../comedidev.h"
#include "comedi_pci.h"

#define DRV_NAME "rtd520"


#define DMA_CHAIN_COUNT 2	

#define TRANS_TARGET_PERIOD 10000000	

#define RTD_MAX_CHANLIST	128	

#ifdef FAST_SPIN
#define WAIT_QUIETLY		
#define RTD_ADC_TIMEOUT	66000	
#define RTD_DAC_TIMEOUT	66000
#define RTD_DMA_TIMEOUT	33000	
#else
#define WAIT_QUIETLY	udelay(1)
#define RTD_ADC_TIMEOUT	2000	
#define RTD_DAC_TIMEOUT	2000	
#define RTD_DMA_TIMEOUT	1000	
#endif


#define PCI_VENDOR_ID_RTD	0x1435
#define LCFG_PCIINDEX	0
#define LAS0_PCIINDEX	2	
#define LAS1_PCIINDEX	3
#define LCFG_PCISIZE	0x100
#define LAS0_PCISIZE	0x200
#define LAS1_PCISIZE	0x10

#define RTD_CLOCK_RATE	8000000	
#define RTD_CLOCK_BASE	125	

#define RTD_MAX_SPEED	1625	
#define RTD_MAX_SPEED_1	875	

#define RTD_MIN_SPEED	2097151875	
#define RTD_MIN_SPEED_1	5000000	

#include "rtd520.h"
#include "plx9080.h"

#define DMA_MODE_BITS (\
		       PLX_LOCAL_BUS_16_WIDE_BITS \
		       | PLX_DMA_EN_READYIN_BIT \
		       | PLX_DMA_LOCAL_BURST_EN_BIT \
		       | PLX_EN_CHAIN_BIT \
		       | PLX_DMA_INTR_PCI_BIT \
		       | PLX_LOCAL_ADDR_CONST_BIT \
		       | PLX_DEMAND_MODE_BIT)

#define DMA_TRANSFER_BITS (\
  PLX_DESC_IN_PCI_BIT \
 | PLX_INTR_TERM_COUNT \
		| PLX_XFER_LOCAL_TO_PCI)


static const struct comedi_lrange rtd_ai_7520_range = { 18, {
							     
							     BIP_RANGE(5.0),
							     BIP_RANGE(5.0 / 2),
							     BIP_RANGE(5.0 / 4),
							     BIP_RANGE(5.0 / 8),
							     BIP_RANGE(5.0 /
								       16),
							     BIP_RANGE(5.0 /
								       32),
							     
							     BIP_RANGE(10.0),
							     BIP_RANGE(10.0 /
								       2),
							     BIP_RANGE(10.0 /
								       4),
							     BIP_RANGE(10.0 /
								       8),
							     BIP_RANGE(10.0 /
								       16),
							     BIP_RANGE(10.0 /
								       32),
							     
							     UNI_RANGE(10.0),
							     UNI_RANGE(10.0 /
								       2),
							     UNI_RANGE(10.0 /
								       4),
							     UNI_RANGE(10.0 /
								       8),
							     UNI_RANGE(10.0 /
								       16),
							     UNI_RANGE(10.0 /
								       32),

							     }
};

static const struct comedi_lrange rtd_ai_4520_range = { 24, {
							     
							     BIP_RANGE(5.0),
							     BIP_RANGE(5.0 / 2),
							     BIP_RANGE(5.0 / 4),
							     BIP_RANGE(5.0 / 8),
							     BIP_RANGE(5.0 /
								       16),
							     BIP_RANGE(5.0 /
								       32),
							     BIP_RANGE(5.0 /
								       64),
							     BIP_RANGE(5.0 /
								       128),
							     
							     BIP_RANGE(10.0),
							     BIP_RANGE(10.0 /
								       2),
							     BIP_RANGE(10.0 /
								       4),
							     BIP_RANGE(10.0 /
								       8),
							     BIP_RANGE(10.0 /
								       16),
							     BIP_RANGE(10.0 /
								       32),
							     BIP_RANGE(10.0 /
								       64),
							     BIP_RANGE(10.0 /
								       128),
							     
							     UNI_RANGE(10.0),
							     UNI_RANGE(10.0 /
								       2),
							     UNI_RANGE(10.0 /
								       4),
							     UNI_RANGE(10.0 /
								       8),
							     UNI_RANGE(10.0 /
								       16),
							     UNI_RANGE(10.0 /
								       32),
							     UNI_RANGE(10.0 /
								       64),
							     UNI_RANGE(10.0 /
								       128),
							     }
};

static const struct comedi_lrange rtd_ao_range = { 4, {
						       RANGE(0, 5),
						       RANGE(0, 10),
						       RANGE(-5, 5),
						       RANGE(-10, 10),
						       }
};

struct rtdBoard {
	const char *name;	
	int device_id;
	int aiChans;
	int aiBits;
	int aiMaxGain;
	int range10Start;	
	int rangeUniStart;	
};

static const struct rtdBoard rtd520Boards[] = {
	{
	 .name = "DM7520",
	 .device_id = 0x7520,
	 .aiChans = 16,
	 .aiBits = 12,
	 .aiMaxGain = 32,
	 .range10Start = 6,
	 .rangeUniStart = 12,
	 },
	{
	 .name = "PCI4520",
	 .device_id = 0x4520,
	 .aiChans = 16,
	 .aiBits = 12,
	 .aiMaxGain = 128,
	 .range10Start = 8,
	 .rangeUniStart = 16,
	 },
};

static DEFINE_PCI_DEVICE_TABLE(rtd520_pci_table) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_RTD, 0x7520) },
	{ PCI_DEVICE(PCI_VENDOR_ID_RTD, 0x4520) },
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, rtd520_pci_table);

#define thisboard ((const struct rtdBoard *)dev->board_ptr)

struct rtdPrivate {
	
	void *las0;
	void *las1;
	void *lcfg;

	unsigned long intCount;	
	long aiCount;		
	int transCount;		
	int flags;		

	
	struct pci_dev *pci_dev;
	int got_regions;	

	
	
	unsigned char chanBipolar[RTD_MAX_CHANLIST / 8];	

	
	unsigned int aoValue[2];	

	
	u8 utcGate[4];		

	
	
	u16 intMask;		
	u16 intClearMask;	
	u8 utcCtrl[4];		
	u8 dioStatus;		
#ifdef USE_DMA
	s16 dma0Offset;		
	uint16_t *dma0Buff[DMA_CHAIN_COUNT];	
	dma_addr_t dma0BuffPhysAddr[DMA_CHAIN_COUNT];	
	struct plx_dma_desc *dma0Chain;	
	dma_addr_t dma0ChainPhysAddr;	
	
	u8 dma0Control;
	u8 dma1Control;
#endif				
	unsigned fifoLen;
};

#define SEND_EOS	0x01	
#define DMA0_ACTIVE	0x02	
#define DMA1_ACTIVE	0x04	

#define CHAN_ARRAY_TEST(array, index) \
	(((array)[(index)/8] >> ((index) & 0x7)) & 0x1)
#define CHAN_ARRAY_SET(array, index) \
	(((array)[(index)/8] |= 1 << ((index) & 0x7)))
#define CHAN_ARRAY_CLEAR(array, index) \
	(((array)[(index)/8] &= ~(1 << ((index) & 0x7))))

#define devpriv ((struct rtdPrivate *)dev->private)


#define RtdResetBoard(dev) \
	writel(0, devpriv->las0+LAS0_BOARD_RESET)

#define RtdResetCGT(dev) \
	writel(0, devpriv->las0+LAS0_CGT_RESET)

#define RtdClearCGT(dev) \
	writel(0, devpriv->las0+LAS0_CGT_CLEAR)

#define RtdEnableCGT(dev, v) \
	writel((v > 0) ? 1 : 0, devpriv->las0+LAS0_CGT_ENABLE)

#define RtdWriteCGTable(dev, v) \
	writel(v, devpriv->las0+LAS0_CGT_WRITE)

#define RtdWriteCGLatch(dev, v) \
	writel(v, devpriv->las0+LAS0_CGL_WRITE)

#define RtdAdcClearFifo(dev) \
	writel(0, devpriv->las0+LAS0_ADC_FIFO_CLEAR)

#define RtdAdcConversionSource(dev, v) \
	writel(v, devpriv->las0+LAS0_ADC_CONVERSION)

#define RtdBurstStartSource(dev, v) \
	writel(v, devpriv->las0+LAS0_BURST_START)

#define RtdPacerStartSource(dev, v) \
	writel(v, devpriv->las0+LAS0_PACER_START)

#define RtdPacerStopSource(dev, v) \
	writel(v, devpriv->las0+LAS0_PACER_STOP)

#define RtdPacerClockSource(dev, v) \
	writel((v > 0) ? 1 : 0, devpriv->las0+LAS0_PACER_SELECT)

#define RtdAdcSampleCounterSource(dev, v) \
	writel(v, devpriv->las0+LAS0_ADC_SCNT_SRC)

#define RtdPacerTriggerMode(dev, v) \
	writel((v > 0) ? 1 : 0, devpriv->las0+LAS0_PACER_REPEAT)

#define RtdAboutStopEnable(dev, v) \
	writel((v > 0) ? 1 : 0, devpriv->las0+LAS0_ACNT_STOP_ENABLE)

#define RtdTriggerPolarity(dev, v) \
	writel((v > 0) ? 1 : 0, devpriv->las0+LAS0_ETRG_POLARITY)

#define RtdAdcStart(dev) \
	writew(0, devpriv->las0+LAS0_ADC)

#define RtdAdcFifoGet(dev) \
	readw(devpriv->las1+LAS1_ADC_FIFO)

#define RtdAdcFifoGet2(dev) \
	readl(devpriv->las1+LAS1_ADC_FIFO)

#define RtdFifoStatus(dev) \
	readl(devpriv->las0+LAS0_ADC)

#define RtdPacerStart(dev) \
	readl(devpriv->las0+LAS0_PACER)
#define RtdPacerStop(dev) \
	writel(0, devpriv->las0+LAS0_PACER)

#define RtdInterruptStatus(dev) \
	readw(devpriv->las0+LAS0_IT)

#define RtdInterruptMask(dev, v) \
	writew((devpriv->intMask = (v)), devpriv->las0+LAS0_IT)

#define RtdInterruptClear(dev) \
	readw(devpriv->las0+LAS0_CLEAR)

#define RtdInterruptClearMask(dev, v) \
	writew((devpriv->intClearMask = (v)), devpriv->las0+LAS0_CLEAR)

#define RtdInterruptOverrunStatus(dev) \
	readl(devpriv->las0+LAS0_OVERRUN)

#define RtdInterruptOverrunClear(dev) \
	writel(0, devpriv->las0+LAS0_OVERRUN)

#define RtdPacerCount(dev) \
	readl(devpriv->las0+LAS0_PCLK)
#define RtdPacerCounter(dev, v) \
	writel((v) & 0xffffff, devpriv->las0+LAS0_PCLK)

#define RtdBurstCount(dev) \
	readl(devpriv->las0+LAS0_BCLK)
#define RtdBurstCounter(dev, v) \
	writel((v) & 0x3ff, devpriv->las0+LAS0_BCLK)

#define RtdDelayCount(dev) \
	readl(devpriv->las0+LAS0_DCLK)
#define RtdDelayCounter(dev, v) \
	writel((v) & 0xffff, devpriv->las0+LAS0_DCLK)

#define RtdAboutCount(dev) \
	readl(devpriv->las0+LAS0_ACNT)
#define RtdAboutCounter(dev, v) \
	writel((v) & 0xffff, devpriv->las0+LAS0_ACNT)

#define RtdAdcSampleCount(dev) \
	readl(devpriv->las0+LAS0_ADC_SCNT)
#define RtdAdcSampleCounter(dev, v) \
	writel((v) & 0x3ff, devpriv->las0+LAS0_ADC_SCNT)

#define RtdUtcCounterGet(dev, n) \
	readb(devpriv->las0 \
		+ ((n <= 0) ? LAS0_UTC0 : ((1 == n) ? LAS0_UTC1 : LAS0_UTC2)))

#define RtdUtcCounterPut(dev, n, v) \
	writeb((v) & 0xff, devpriv->las0 \
		+ ((n <= 0) ? LAS0_UTC0 : ((1 == n) ? LAS0_UTC1 : LAS0_UTC2)))

#define RtdUtcCtrlPut(dev, n, v) \
	writeb(devpriv->utcCtrl[(n) & 3] = (((n) & 3) << 6) | ((v) & 0x3f), \
		devpriv->las0 + LAS0_UTC_CTRL)

#define RtdUtcClockSource(dev, n, v) \
	writew(v, devpriv->las0 \
		+ ((n <= 0) ? LAS0_UTC0_CLOCK : \
			((1 == n) ? LAS0_UTC1_CLOCK : LAS0_UTC2_CLOCK)))

#define RtdUtcGateSource(dev, n, v) \
	writew(v, devpriv->las0 \
		+ ((n <= 0) ? LAS0_UTC0_GATE : \
			((1 == n) ? LAS0_UTC1_GATE : LAS0_UTC2_GATE)))

#define RtdUsrOutSource(dev, n, v) \
	writel(v, devpriv->las0+((n <= 0) ? LAS0_UOUT0_SELECT : LAS0_UOUT1_SELECT))

#define RtdDio0Read(dev) \
	(readw(devpriv->las0+LAS0_DIO0) & 0xff)
#define RtdDio0Write(dev, v) \
	writew((v) & 0xff, devpriv->las0+LAS0_DIO0)

#define RtdDio1Read(dev) \
	(readw(devpriv->las0+LAS0_DIO1) & 0xff)
#define RtdDio1Write(dev, v) \
	writew((v) & 0xff, devpriv->las0+LAS0_DIO1)

#define RtdDioStatusRead(dev) \
	(readw(devpriv->las0+LAS0_DIO_STATUS) & 0xff)
#define RtdDioStatusWrite(dev, v) \
	writew((devpriv->dioStatus = (v)), devpriv->las0+LAS0_DIO_STATUS)

#define RtdDio0CtrlRead(dev) \
	(readw(devpriv->las0+LAS0_DIO0_CTRL) & 0xff)
#define RtdDio0CtrlWrite(dev, v) \
	writew((v) & 0xff, devpriv->las0+LAS0_DIO0_CTRL)

#define RtdDacFifoPut(dev, n, v) \
	writew((v), devpriv->las1 + (((n) == 0) ? LAS1_DAC1_FIFO : LAS1_DAC2_FIFO))

#define RtdDacUpdate(dev, n) \
	writew(0, devpriv->las0 + (((n) == 0) ? LAS0_DAC1 : LAS0_DAC2))

#define RtdDacBothUpdate(dev) \
	writew(0, devpriv->las0+LAS0_DAC)

#define RtdDacRange(dev, n, v) \
	writew((v) & 7, devpriv->las0 \
		+(((n) == 0) ? LAS0_DAC1_CTRL : LAS0_DAC2_CTRL))

#define RtdDacClearFifo(dev, n) \
	writel(0, devpriv->las0+(((n) == 0) ? LAS0_DAC1_RESET : LAS0_DAC2_RESET))

#define RtdDma0Source(dev, n) \
	writel((n) & 0xf, devpriv->las0+LAS0_DMA0_SRC)

#define RtdDma1Source(dev, n) \
	writel((n) & 0xf, devpriv->las0+LAS0_DMA1_SRC)

#define RtdDma0Reset(dev) \
	writel(0, devpriv->las0+LAS0_DMA0_RESET)

#define RtdDma1Reset(dev) \
	writel(0, devpriv->las0+LAS0_DMA1_SRC)

#define RtdPlxInterruptRead(dev) \
	readl(devpriv->lcfg+LCFG_ITCSR)
#define RtdPlxInterruptWrite(dev, v) \
	writel(v, devpriv->lcfg+LCFG_ITCSR)

#define RtdDma0Mode(dev, m) \
	writel((m), devpriv->lcfg+LCFG_DMAMODE0)

#define RtdDma0PciAddr(dev, a) \
	writel((a), devpriv->lcfg+LCFG_DMAPADR0)

#define RtdDma0LocalAddr(dev, a) \
	writel((a), devpriv->lcfg+LCFG_DMALADR0)

#define RtdDma0Count(dev, c) \
	writel((c), devpriv->lcfg+LCFG_DMASIZ0)

#define RtdDma0Next(dev, a) \
	writel((a), devpriv->lcfg+LCFG_DMADPR0)

#define RtdDma1Mode(dev, m) \
	writel((m), devpriv->lcfg+LCFG_DMAMODE1)

#define RtdDma1PciAddr(dev, a) \
	writel((a), devpriv->lcfg+LCFG_DMAADR1)

#define RtdDma1LocalAddr(dev, a) \
	writel((a), devpriv->lcfg+LCFG_DMALADR1)

#define RtdDma1Count(dev, c) \
	writel((c), devpriv->lcfg+LCFG_DMASIZ1)

#define RtdDma1Next(dev, a) \
	writel((a), devpriv->lcfg+LCFG_DMADPR1)

#define RtdDma0Control(dev, n) \
	writeb(devpriv->dma0Control = (n), devpriv->lcfg+LCFG_DMACSR0)

#define RtdDma0Status(dev) \
	readb(devpriv->lcfg+LCFG_DMACSR0)

#define RtdDma1Control(dev, n) \
	writeb(devpriv->dma1Control = (n), devpriv->lcfg+LCFG_DMACSR1)

#define RtdDma1Status(dev) \
	readb(devpriv->lcfg+LCFG_DMACSR1)

static int rtd_attach(struct comedi_device *dev, struct comedi_devconfig *it);
static int rtd_detach(struct comedi_device *dev);

static struct comedi_driver rtd520Driver = {
	.driver_name = DRV_NAME,
	.module = THIS_MODULE,
	.attach = rtd_attach,
	.detach = rtd_detach,
};

static int rtd_ai_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
			struct comedi_insn *insn, unsigned int *data);
static int rtd_ao_winsn(struct comedi_device *dev, struct comedi_subdevice *s,
			struct comedi_insn *insn, unsigned int *data);
static int rtd_ao_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
			struct comedi_insn *insn, unsigned int *data);
static int rtd_dio_insn_bits(struct comedi_device *dev,
			     struct comedi_subdevice *s,
			     struct comedi_insn *insn, unsigned int *data);
static int rtd_dio_insn_config(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data);
static int rtd_ai_cmdtest(struct comedi_device *dev, struct comedi_subdevice *s,
			  struct comedi_cmd *cmd);
static int rtd_ai_cmd(struct comedi_device *dev, struct comedi_subdevice *s);
static int rtd_ai_cancel(struct comedi_device *dev, struct comedi_subdevice *s);
static int rtd_ns_to_timer(unsigned int *ns, int roundMode);
static irqreturn_t rtd_interrupt(int irq, void *d);
static int rtd520_probe_fifo_depth(struct comedi_device *dev);

static int rtd_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{				
	struct comedi_subdevice *s;
	struct pci_dev *pcidev;
	int ret;
	resource_size_t physLas0;	
	resource_size_t physLas1;	
	resource_size_t physLcfg;	
#ifdef USE_DMA
	int index;
#endif

	printk(KERN_INFO "comedi%d: rtd520 attaching.\n", dev->minor);

#if defined(CONFIG_COMEDI_DEBUG) && defined(USE_DMA)
	
	if (0 == comedi_debug)	
		comedi_debug = 1;
#endif

	if (alloc_private(dev, sizeof(struct rtdPrivate)) < 0)
		return -ENOMEM;

	for (pcidev = pci_get_device(PCI_VENDOR_ID_RTD, PCI_ANY_ID, NULL);
	     pcidev != NULL;
	     pcidev = pci_get_device(PCI_VENDOR_ID_RTD, PCI_ANY_ID, pcidev)) {
		int i;

		if (it->options[0] || it->options[1]) {
			if (pcidev->bus->number != it->options[0]
			    || PCI_SLOT(pcidev->devfn) != it->options[1]) {
				continue;
			}
		}
		for (i = 0; i < ARRAY_SIZE(rtd520Boards); ++i) {
			if (pcidev->device == rtd520Boards[i].device_id) {
				dev->board_ptr = &rtd520Boards[i];
				break;
			}
		}
		if (dev->board_ptr)
			break;	
	}
	if (!pcidev) {
		if (it->options[0] && it->options[1]) {
			printk(KERN_INFO "No RTD card at bus=%d slot=%d.\n",
			       it->options[0], it->options[1]);
		} else {
			printk(KERN_INFO "No RTD card found.\n");
		}
		return -EIO;
	}
	devpriv->pci_dev = pcidev;
	dev->board_name = thisboard->name;

	ret = comedi_pci_enable(pcidev, DRV_NAME);
	if (ret < 0) {
		printk(KERN_INFO "Failed to enable PCI device and request regions.\n");
		return ret;
	}
	devpriv->got_regions = 1;

	
	physLas0 = pci_resource_start(devpriv->pci_dev, LAS0_PCIINDEX);
	physLas1 = pci_resource_start(devpriv->pci_dev, LAS1_PCIINDEX);
	physLcfg = pci_resource_start(devpriv->pci_dev, LCFG_PCIINDEX);
	
	
	devpriv->las0 = ioremap_nocache(physLas0, LAS0_PCISIZE);
	devpriv->las1 = ioremap_nocache(physLas1, LAS1_PCISIZE);
	devpriv->lcfg = ioremap_nocache(physLcfg, LCFG_PCISIZE);

	if (!devpriv->las0 || !devpriv->las1 || !devpriv->lcfg)
		return -ENOMEM;


	DPRINTK("%s: LAS0=%llx, LAS1=%llx, CFG=%llx.\n", dev->board_name,
		(unsigned long long)physLas0, (unsigned long long)physLas1,
		(unsigned long long)physLcfg);
	{			
		unsigned char pci_latency;
		u16 revision;
		

		pci_read_config_word(devpriv->pci_dev, PCI_REVISION_ID,
				     &revision);
		DPRINTK("%s: PCI revision %d.\n", dev->board_name, revision);

		pci_read_config_byte(devpriv->pci_dev,
				     PCI_LATENCY_TIMER, &pci_latency);
		if (pci_latency < 32) {
			printk(KERN_INFO "%s: PCI latency changed from %d to %d\n",
			       dev->board_name, pci_latency, 32);
			pci_write_config_byte(devpriv->pci_dev,
					      PCI_LATENCY_TIMER, 32);
		} else {
			DPRINTK("rtd520: PCI latency = %d\n", pci_latency);
		}

		
	}

	
	printk(KERN_INFO "%s:", dev->board_name);

	if (alloc_subdevices(dev, 4) < 0)
		return -ENOMEM;


	s = dev->subdevices + 0;
	dev->read_subdev = s;
	
	s->type = COMEDI_SUBD_AI;
	s->subdev_flags =
	    SDF_READABLE | SDF_GROUND | SDF_COMMON | SDF_DIFF | SDF_CMD_READ;
	s->n_chan = thisboard->aiChans;
	s->maxdata = (1 << thisboard->aiBits) - 1;
	if (thisboard->aiMaxGain <= 32)
		s->range_table = &rtd_ai_7520_range;
	else
		s->range_table = &rtd_ai_4520_range;

	s->len_chanlist = RTD_MAX_CHANLIST;	
	s->insn_read = rtd_ai_rinsn;
	s->do_cmd = rtd_ai_cmd;
	s->do_cmdtest = rtd_ai_cmdtest;
	s->cancel = rtd_ai_cancel;
	

	s = dev->subdevices + 1;
	
	s->type = COMEDI_SUBD_AO;
	s->subdev_flags = SDF_WRITABLE;
	s->n_chan = 2;
	s->maxdata = (1 << thisboard->aiBits) - 1;
	s->range_table = &rtd_ao_range;
	s->insn_write = rtd_ao_winsn;
	s->insn_read = rtd_ao_rinsn;

	s = dev->subdevices + 2;
	
	s->type = COMEDI_SUBD_DIO;
	s->subdev_flags = SDF_READABLE | SDF_WRITABLE;
	
	s->n_chan = 8;
	s->maxdata = 1;
	s->range_table = &range_digital;
	s->insn_bits = rtd_dio_insn_bits;
	s->insn_config = rtd_dio_insn_config;

	
	s = dev->subdevices + 3;
	s->type = COMEDI_SUBD_COUNTER;
	s->subdev_flags = SDF_READABLE | SDF_WRITABLE;
	s->n_chan = 3;
	s->maxdata = 0xffff;

	
	
	RtdResetBoard(dev);
	udelay(100);		
	RtdPlxInterruptWrite(dev, 0);
	RtdInterruptMask(dev, 0);	
	RtdInterruptClearMask(dev, ~0);	
	RtdInterruptClear(dev);	
	RtdInterruptOverrunClear(dev);
	RtdClearCGT(dev);
	RtdAdcClearFifo(dev);
	RtdDacClearFifo(dev, 0);
	RtdDacClearFifo(dev, 1);
	
	RtdDioStatusWrite(dev, 0);	
	RtdUtcCtrlPut(dev, 0, 0x30);	
	RtdUtcCtrlPut(dev, 1, 0x30);	
	RtdUtcCtrlPut(dev, 2, 0x30);	
	RtdUtcCtrlPut(dev, 3, 0);	
	

	
	ret = request_irq(devpriv->pci_dev->irq, rtd_interrupt,
			  IRQF_SHARED, DRV_NAME, dev);

	if (ret < 0) {
		printk("Could not get interrupt! (%u)\n",
		       devpriv->pci_dev->irq);
		return ret;
	}
	dev->irq = devpriv->pci_dev->irq;
	printk(KERN_INFO "( irq=%u )", dev->irq);

	ret = rtd520_probe_fifo_depth(dev);
	if (ret < 0)
		return ret;

	devpriv->fifoLen = ret;
	printk("( fifoLen=%d )", devpriv->fifoLen);

#ifdef USE_DMA
	if (dev->irq > 0) {
		printk("( DMA buff=%d )\n", DMA_CHAIN_COUNT);
		devpriv->dma0Offset = 0;

		for (index = 0; index < DMA_CHAIN_COUNT; index++) {
			devpriv->dma0Buff[index] =
			    pci_alloc_consistent(devpriv->pci_dev,
						 sizeof(u16) *
						 devpriv->fifoLen / 2,
						 &devpriv->
						 dma0BuffPhysAddr[index]);
			if (devpriv->dma0Buff[index] == NULL) {
				ret = -ENOMEM;
				goto rtd_attach_die_error;
			}
		}

		
		devpriv->dma0Chain =
		    pci_alloc_consistent(devpriv->pci_dev,
					 sizeof(struct plx_dma_desc) *
					 DMA_CHAIN_COUNT,
					 &devpriv->dma0ChainPhysAddr);
		for (index = 0; index < DMA_CHAIN_COUNT; index++) {
			devpriv->dma0Chain[index].pci_start_addr =
			    devpriv->dma0BuffPhysAddr[index];
			devpriv->dma0Chain[index].local_start_addr =
			    DMALADDR_ADC;
			devpriv->dma0Chain[index].transfer_size =
			    sizeof(u16) * devpriv->fifoLen / 2;
			devpriv->dma0Chain[index].next =
			    (devpriv->dma0ChainPhysAddr + ((index +
							    1) %
							   (DMA_CHAIN_COUNT))
			     * sizeof(devpriv->dma0Chain[0]))
			    | DMA_TRANSFER_BITS;
		}

		if (devpriv->dma0Chain == NULL) {
			ret = -ENOMEM;
			goto rtd_attach_die_error;
		}

		RtdDma0Mode(dev, DMA_MODE_BITS);
		
		RtdDma0Source(dev, DMAS_ADFIFO_HALF_FULL);
	} else {
		printk(KERN_INFO "( no IRQ->no DMA )");
	}
#endif 

	if (dev->irq) {		
		RtdPlxInterruptWrite(dev, ICS_PIE | ICS_PLIE);
	}

	printk("\ncomedi%d: rtd520 driver attached.\n", dev->minor);

	return 1;

#if 0
	
#ifdef USE_DMA
	for (index = 0; index < DMA_CHAIN_COUNT; index++) {
		if (NULL != devpriv->dma0Buff[index]) {	
			pci_free_consistent(devpriv->pci_dev,
					    sizeof(u16) * devpriv->fifoLen / 2,
					    devpriv->dma0Buff[index],
					    devpriv->dma0BuffPhysAddr[index]);
			devpriv->dma0Buff[index] = NULL;
		}
	}
	if (NULL != devpriv->dma0Chain) {
		pci_free_consistent(devpriv->pci_dev,
				    sizeof(struct plx_dma_desc)
				    * DMA_CHAIN_COUNT,
				    devpriv->dma0Chain,
				    devpriv->dma0ChainPhysAddr);
		devpriv->dma0Chain = NULL;
	}
#endif 
	
	if (dev->irq) {
		
		RtdPlxInterruptWrite(dev, RtdPlxInterruptRead(dev)
				     & ~(ICS_PLIE | ICS_DMA0_E | ICS_DMA1_E));
		free_irq(dev->irq, dev);
	}

	
	if (devpriv->las0)
		iounmap(devpriv->las0);

	if (devpriv->las1)
		iounmap(devpriv->las1);

	if (devpriv->lcfg)
		iounmap(devpriv->lcfg);

	if (devpriv->pci_dev)
		pci_dev_put(devpriv->pci_dev);

	return ret;
#endif
}

static int rtd_detach(struct comedi_device *dev)
{
#ifdef USE_DMA
	int index;
#endif

	DPRINTK("comedi%d: rtd520: removing (%ld ints)\n",
		dev->minor, (devpriv ? devpriv->intCount : 0L));
	if (devpriv && devpriv->lcfg) {
		DPRINTK
		    ("(int status 0x%x, overrun status 0x%x, fifo status 0x%x)...\n",
		     0xffff & RtdInterruptStatus(dev),
		     0xffff & RtdInterruptOverrunStatus(dev),
		     (0xffff & RtdFifoStatus(dev)) ^ 0x6666);
	}

	if (devpriv) {
		
#ifdef USE_DMA
		if (devpriv->lcfg) {
			RtdDma0Control(dev, 0);	
			RtdDma1Control(dev, 0);	
			RtdPlxInterruptWrite(dev, ICS_PIE | ICS_PLIE);
		}
#endif 
		if (devpriv->las0) {
			RtdResetBoard(dev);
			RtdInterruptMask(dev, 0);
			RtdInterruptClearMask(dev, ~0);
			RtdInterruptClear(dev);	
		}
#ifdef USE_DMA
		
		for (index = 0; index < DMA_CHAIN_COUNT; index++) {
			if (NULL != devpriv->dma0Buff[index]) {
				pci_free_consistent(devpriv->pci_dev,
						    sizeof(u16) *
						    devpriv->fifoLen / 2,
						    devpriv->dma0Buff[index],
						    devpriv->
						    dma0BuffPhysAddr[index]);
				devpriv->dma0Buff[index] = NULL;
			}
		}
		if (NULL != devpriv->dma0Chain) {
			pci_free_consistent(devpriv->pci_dev,
					    sizeof(struct plx_dma_desc) *
					    DMA_CHAIN_COUNT, devpriv->dma0Chain,
					    devpriv->dma0ChainPhysAddr);
			devpriv->dma0Chain = NULL;
		}
#endif 

		
		if (dev->irq) {
			
			RtdPlxInterruptWrite(dev, RtdPlxInterruptRead(dev)
					     & ~(ICS_PLIE | ICS_DMA0_E |
						 ICS_DMA1_E));
			free_irq(dev->irq, dev);
		}

		
		if (devpriv->las0)
			iounmap(devpriv->las0);

		if (devpriv->las1)
			iounmap(devpriv->las1);

		if (devpriv->lcfg)
			iounmap(devpriv->lcfg);

		if (devpriv->pci_dev) {
			if (devpriv->got_regions)
				comedi_pci_disable(devpriv->pci_dev);

			pci_dev_put(devpriv->pci_dev);
		}
	}

	printk(KERN_INFO "comedi%d: rtd520: removed.\n", dev->minor);

	return 0;
}

static unsigned short rtdConvertChanGain(struct comedi_device *dev,
					 unsigned int comediChan, int chanIndex)
{				
	unsigned int chan, range, aref;
	unsigned short r = 0;

	chan = CR_CHAN(comediChan);
	range = CR_RANGE(comediChan);
	aref = CR_AREF(comediChan);

	r |= chan & 0xf;

	
	if (range < thisboard->range10Start) {	
		r |= 0x000;	
		r |= (range & 0x7) << 4;	
		CHAN_ARRAY_SET(devpriv->chanBipolar, chanIndex);
	} else if (range < thisboard->rangeUniStart) {	
		r |= 0x100;	
		
		r |= ((range - thisboard->range10Start) & 0x7) << 4;
		CHAN_ARRAY_SET(devpriv->chanBipolar, chanIndex);
	} else {		
		r |= 0x200;	
		
		r |= ((range - thisboard->rangeUniStart) & 0x7) << 4;
		CHAN_ARRAY_CLEAR(devpriv->chanBipolar, chanIndex);
	}

	switch (aref) {
	case AREF_GROUND:	
		break;

	case AREF_COMMON:
		r |= 0x80;	
		break;

	case AREF_DIFF:
		r |= 0x400;	
		break;

	case AREF_OTHER:	
		break;
	}
	return r;
}

static void rtd_load_channelgain_list(struct comedi_device *dev,
				      unsigned int n_chan, unsigned int *list)
{
	if (n_chan > 1) {	
		int ii;
		RtdClearCGT(dev);
		RtdEnableCGT(dev, 1);	
		for (ii = 0; ii < n_chan; ii++) {
			RtdWriteCGTable(dev, rtdConvertChanGain(dev, list[ii],
								ii));
		}
	} else {		
		RtdEnableCGT(dev, 0);	
		RtdWriteCGLatch(dev, rtdConvertChanGain(dev, list[0], 0));
	}
}

static int rtd520_probe_fifo_depth(struct comedi_device *dev)
{
	unsigned int chanspec = CR_PACK(0, 0, AREF_GROUND);
	unsigned i;
	static const unsigned limit = 0x2000;
	unsigned fifo_size = 0;

	RtdAdcClearFifo(dev);
	rtd_load_channelgain_list(dev, 1, &chanspec);
	RtdAdcConversionSource(dev, 0);	
	
	for (i = 0; i < limit; ++i) {
		unsigned fifo_status;
		
		RtdAdcStart(dev);
		udelay(1);
		fifo_status = RtdFifoStatus(dev);
		if ((fifo_status & FS_ADC_HEMPTY) == 0) {
			fifo_size = 2 * i;
			break;
		}
	}
	if (i == limit) {
		printk(KERN_INFO "\ncomedi: %s: failed to probe fifo size.\n", DRV_NAME);
		return -EIO;
	}
	RtdAdcClearFifo(dev);
	if (fifo_size != 0x400 && fifo_size != 0x2000) {
		printk
		    (KERN_INFO "\ncomedi: %s: unexpected fifo size of %i, expected 1024 or 8192.\n",
		     DRV_NAME, fifo_size);
		return -EIO;
	}
	return fifo_size;
}

static int rtd_ai_rinsn(struct comedi_device *dev,
			struct comedi_subdevice *s, struct comedi_insn *insn,
			unsigned int *data)
{
	int n, ii;
	int stat;

	
	RtdAdcClearFifo(dev);

	
	rtd_load_channelgain_list(dev, 1, &insn->chanspec);

	
	RtdAdcConversionSource(dev, 0);	

	
	for (n = 0; n < insn->n; n++) {
		s16 d;
		
		RtdAdcStart(dev);

		for (ii = 0; ii < RTD_ADC_TIMEOUT; ++ii) {
			stat = RtdFifoStatus(dev);
			if (stat & FS_ADC_NOT_EMPTY)	
				break;
			WAIT_QUIETLY;
		}
		if (ii >= RTD_ADC_TIMEOUT) {
			DPRINTK
			    ("rtd520: Error: ADC never finished! FifoStatus=0x%x\n",
			     stat ^ 0x6666);
			return -ETIMEDOUT;
		}

		
		d = RtdAdcFifoGet(dev);	
		
		d = d >> 3;	
		if (CHAN_ARRAY_TEST(devpriv->chanBipolar, 0))
			
			data[n] = d + 2048;
		else
			data[n] = d;
	}

	/* return the number of samples read/written */
	return n;
}

static int ai_read_n(struct comedi_device *dev, struct comedi_subdevice *s,
		     int count)
{
	int ii;

	for (ii = 0; ii < count; ii++) {
		short sample;
		s16 d;

		if (0 == devpriv->aiCount) {	
			d = RtdAdcFifoGet(dev);	
			continue;
		}
#if 0
		if (0 == (RtdFifoStatus(dev) & FS_ADC_NOT_EMPTY)) {	
			DPRINTK("comedi: READ OOPS on %d of %d\n", ii + 1,
				count);
			break;
		}
#endif
		d = RtdAdcFifoGet(dev);	

		d = d >> 3;	
		if (CHAN_ARRAY_TEST(devpriv->chanBipolar, s->async->cur_chan))
			sample = d + 2048;	
		else
			sample = d;

		if (!comedi_buf_put(s->async, sample))
			return -1;

		if (devpriv->aiCount > 0)	
			devpriv->aiCount--;
	}
	return 0;
}

static int ai_read_dregs(struct comedi_device *dev, struct comedi_subdevice *s)
{
	while (RtdFifoStatus(dev) & FS_ADC_NOT_EMPTY) {	
		short sample;
		s16 d = RtdAdcFifoGet(dev);	

		if (0 == devpriv->aiCount) {	
			continue;	
		}

		d = d >> 3;	
		if (CHAN_ARRAY_TEST(devpriv->chanBipolar, s->async->cur_chan))
			sample = d + 2048;	
		else
			sample = d;

		if (!comedi_buf_put(s->async, sample))
			return -1;

		if (devpriv->aiCount > 0)	
			devpriv->aiCount--;
	}
	return 0;
}

#ifdef USE_DMA
void abort_dma(struct comedi_device *dev, unsigned int channel)
{				
	unsigned long dma_cs_addr;	
	uint8_t status;
	unsigned int ii;
	

	dma_cs_addr = (unsigned long)devpriv->lcfg
	    + ((channel == 0) ? LCFG_DMACSR0 : LCFG_DMACSR1);

	
	

	
	status = readb(dma_cs_addr);
	if ((status & PLX_DMA_EN_BIT) == 0) {	
		DPRINTK("rtd520: AbortDma on non-active channel %d (0x%x)\n",
			channel, status);
		goto abortDmaExit;
	}

	
	for (ii = 0; (status & PLX_DMA_DONE_BIT) && ii < RTD_DMA_TIMEOUT; ii++) {
		WAIT_QUIETLY;
		status = readb(dma_cs_addr);
	}
	if (status & PLX_DMA_DONE_BIT) {
		printk("rtd520: Timeout waiting for dma %i done clear\n",
		       channel);
		goto abortDmaExit;
	}

	
	writeb(0, dma_cs_addr);
	udelay(1);		
	
	writeb(PLX_DMA_ABORT_BIT, dma_cs_addr);

	
	status = readb(dma_cs_addr);
	for (ii = 0;
	     (status & PLX_DMA_DONE_BIT) == 0 && ii < RTD_DMA_TIMEOUT; ii++) {
		status = readb(dma_cs_addr);
		WAIT_QUIETLY;
	}
	if ((status & PLX_DMA_DONE_BIT) == 0) {
		printk("rtd520: Timeout waiting for dma %i done set\n",
		       channel);
	}

abortDmaExit:
	
}

static int ai_process_dma(struct comedi_device *dev, struct comedi_subdevice *s)
{
	int ii, n;
	s16 *dp;

	if (devpriv->aiCount == 0)	
		return 0;

	dp = devpriv->dma0Buff[devpriv->dma0Offset];
	for (ii = 0; ii < devpriv->fifoLen / 2;) {	
		short sample;

		if (CHAN_ARRAY_TEST(devpriv->chanBipolar, s->async->cur_chan)) {
			sample = (*dp >> 3) + 2048;	
		else
			sample = *dp >> 3;	

		*dp++ = sample;	

		if (++s->async->cur_chan >= s->async->cmd.chanlist_len)
			s->async->cur_chan = 0;

		++ii;		
		if (devpriv->aiCount > 0) {	
			if (--devpriv->aiCount == 0) {	
				
				break;
			}
		}
	}

	
	dp = devpriv->dma0Buff[devpriv->dma0Offset];
	n = comedi_buf_write_alloc(s->async, ii * sizeof(s16));
	if (n < (ii * sizeof(s16))) {	
		DPRINTK("rtd520:ai_process_dma buffer overflow %d samples!\n",
			ii - (n / sizeof(s16)));
		s->async->events |= COMEDI_CB_ERROR;
		return -1;
	}
	comedi_buf_memcpy_to(s->async, 0, dp, n);
	comedi_buf_write_free(s->async, n);

	
	s->async->events |= COMEDI_CB_BLOCK | COMEDI_CB_EOS;

	if (++devpriv->dma0Offset >= DMA_CHAIN_COUNT) {	
		devpriv->dma0Offset = 0;
	}
	return 0;
}
#endif 

static irqreturn_t rtd_interrupt(int irq,	
				 void *d)
{				
	struct comedi_device *dev = d;	
	u16 status;
	u16 fifoStatus;
	struct comedi_subdevice *s = dev->subdevices + 0;	

	if (!dev->attached)
		return IRQ_NONE;

	devpriv->intCount++;	

	fifoStatus = RtdFifoStatus(dev);
	
	if (!(fifoStatus & FS_ADC_NOT_FULL)) {	
		DPRINTK("rtd520: FIFO full! fifo_status=0x%x\n", (fifoStatus ^ 0x6666) & 0x7777);	
		goto abortTransfer;
	}
#ifdef USE_DMA
	if (devpriv->flags & DMA0_ACTIVE) {	
		u32 istatus = RtdPlxInterruptRead(dev);

		if (istatus & ICS_DMA0_A) {
			if (ai_process_dma(dev, s) < 0) {
				DPRINTK
				    ("rtd520: comedi read buffer overflow (DMA) with %ld to go!\n",
				     devpriv->aiCount);
				RtdDma0Control(dev,
					       (devpriv->dma0Control &
						~PLX_DMA_START_BIT)
					       | PLX_CLEAR_DMA_INTR_BIT);
				goto abortTransfer;
			}

			RtdDma0Control(dev,
				       (devpriv->
					dma0Control & ~PLX_DMA_START_BIT)
				       | PLX_CLEAR_DMA_INTR_BIT);
			if (0 == devpriv->aiCount) {	
				DPRINTK("rtd520: Samples Done (DMA).\n");
				goto transferDone;
			}
			comedi_event(dev, s);
		} else {
			
		}
	}
	
#endif 

	status = RtdInterruptStatus(dev);
	
	if (0 == status)
		return IRQ_HANDLED;

	if (status & IRQM_ADC_ABOUT_CNT) {	
		if (!(fifoStatus & FS_ADC_HEMPTY)) {	
			if (ai_read_n(dev, s, devpriv->fifoLen / 2) < 0) {
				DPRINTK
				    ("rtd520: comedi read buffer overflow (1/2FIFO) with %ld to go!\n",
				     devpriv->aiCount);
				goto abortTransfer;
			}
			if (0 == devpriv->aiCount) {	
				DPRINTK("rtd520: Samples Done (1/2). fifo_status was 0x%x\n", (fifoStatus ^ 0x6666) & 0x7777);	
				goto transferDone;
			}
			comedi_event(dev, s);
		} else if (devpriv->transCount > 0) {	
			if (fifoStatus & FS_ADC_NOT_EMPTY) {	
				if (ai_read_n(dev, s, devpriv->transCount) < 0) {
					DPRINTK
					    ("rtd520: comedi read buffer overflow (N) with %ld to go!\n",
					     devpriv->aiCount);
					goto abortTransfer;
				}
				if (0 == devpriv->aiCount) {	
					DPRINTK
					    ("rtd520: Samples Done (N). fifo_status was 0x%x\n",
					     (fifoStatus ^ 0x6666) & 0x7777);
					goto transferDone;
				}
				comedi_event(dev, s);
			}
		} else {	
			DPRINTK
			    ("rtd520: Sample int.  Wait for 1/2. fifo_status 0x%x\n",
			     (fifoStatus ^ 0x6666) & 0x7777);
		}
	} else {
		DPRINTK("rtd520: unknown interrupt source!\n");
	}

	if (0xffff & RtdInterruptOverrunStatus(dev)) {	
		DPRINTK
		    ("rtd520: Interrupt overrun with %ld to go! over_status=0x%x\n",
		     devpriv->aiCount, 0xffff & RtdInterruptOverrunStatus(dev));
		goto abortTransfer;
	}

	
	RtdInterruptClearMask(dev, status);
	RtdInterruptClear(dev);
	return IRQ_HANDLED;

abortTransfer:
	RtdAdcClearFifo(dev);	
	s->async->events |= COMEDI_CB_ERROR;
	devpriv->aiCount = 0;	
	

transferDone:
	RtdPacerStopSource(dev, 0);	
	RtdPacerStop(dev);	
	RtdAdcConversionSource(dev, 0);	
	RtdInterruptMask(dev, 0);	
#ifdef USE_DMA
	if (devpriv->flags & DMA0_ACTIVE) {
		RtdPlxInterruptWrite(dev,	
				     RtdPlxInterruptRead(dev) & ~ICS_DMA0_E);
		abort_dma(dev, 0);
		devpriv->flags &= ~DMA0_ACTIVE;
		
		if (devpriv->aiCount > 0) {
			DPRINTK("rtd520: Lost DMA data! %ld remain\n",
				devpriv->aiCount);
		}
	}
#endif 

	if (devpriv->aiCount > 0) {	
		fifoStatus = RtdFifoStatus(dev);
		DPRINTK("rtd520: Finishing up. %ld remain, fifoStat=%x\n", devpriv->aiCount, (fifoStatus ^ 0x6666) & 0x7777);	
		ai_read_dregs(dev, s);	
	}

	s->async->events |= COMEDI_CB_EOA;	
	comedi_event(dev, s);

	
	status = RtdInterruptStatus(dev);
	RtdInterruptClearMask(dev, status);
	RtdInterruptClear(dev);

	fifoStatus = RtdFifoStatus(dev);	
	DPRINTK
	    ("rtd520: Acquisition complete. %ld ints, intStat=%x, overStat=%x\n",
	     devpriv->intCount, status,
	     0xffff & RtdInterruptOverrunStatus(dev));

	return IRQ_HANDLED;
}

#if 0
static int rtd_ai_poll(struct comedi_device *dev, struct comedi_subdevice *s)
{
	
	
	return s->async->buf_write_count - s->async->buf_read_count;
}
#endif


static int rtd_ai_cmdtest(struct comedi_device *dev,
			  struct comedi_subdevice *s, struct comedi_cmd *cmd)
{
	int err = 0;
	int tmp;

	

	tmp = cmd->start_src;
	cmd->start_src &= TRIG_NOW;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	tmp = cmd->scan_begin_src;
	cmd->scan_begin_src &= TRIG_TIMER | TRIG_EXT;
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

	
	if (cmd->scan_begin_src != TRIG_TIMER &&
	    cmd->scan_begin_src != TRIG_EXT) {
		err++;
	}
	if (cmd->convert_src != TRIG_TIMER && cmd->convert_src != TRIG_EXT)
		err++;

	if (cmd->stop_src != TRIG_COUNT && cmd->stop_src != TRIG_NONE)
		err++;

	if (err)
		return 2;

	

	if (cmd->start_arg != 0) {
		cmd->start_arg = 0;
		err++;
	}

	if (cmd->scan_begin_src == TRIG_TIMER) {
		
		if (1 == cmd->chanlist_len) {	
			if (cmd->scan_begin_arg < RTD_MAX_SPEED_1) {
				cmd->scan_begin_arg = RTD_MAX_SPEED_1;
				rtd_ns_to_timer(&cmd->scan_begin_arg,
						TRIG_ROUND_UP);
				err++;
			}
			if (cmd->scan_begin_arg > RTD_MIN_SPEED_1) {
				cmd->scan_begin_arg = RTD_MIN_SPEED_1;
				rtd_ns_to_timer(&cmd->scan_begin_arg,
						TRIG_ROUND_DOWN);
				err++;
			}
		} else {
			if (cmd->scan_begin_arg < RTD_MAX_SPEED) {
				cmd->scan_begin_arg = RTD_MAX_SPEED;
				rtd_ns_to_timer(&cmd->scan_begin_arg,
						TRIG_ROUND_UP);
				err++;
			}
			if (cmd->scan_begin_arg > RTD_MIN_SPEED) {
				cmd->scan_begin_arg = RTD_MIN_SPEED;
				rtd_ns_to_timer(&cmd->scan_begin_arg,
						TRIG_ROUND_DOWN);
				err++;
			}
		}
	} else {
		
		
		
		if (cmd->scan_begin_arg > 9) {
			cmd->scan_begin_arg = 9;
			err++;
		}
	}
	if (cmd->convert_src == TRIG_TIMER) {
		if (1 == cmd->chanlist_len) {	
			if (cmd->convert_arg < RTD_MAX_SPEED_1) {
				cmd->convert_arg = RTD_MAX_SPEED_1;
				rtd_ns_to_timer(&cmd->convert_arg,
						TRIG_ROUND_UP);
				err++;
			}
			if (cmd->convert_arg > RTD_MIN_SPEED_1) {
				cmd->convert_arg = RTD_MIN_SPEED_1;
				rtd_ns_to_timer(&cmd->convert_arg,
						TRIG_ROUND_DOWN);
				err++;
			}
		} else {
			if (cmd->convert_arg < RTD_MAX_SPEED) {
				cmd->convert_arg = RTD_MAX_SPEED;
				rtd_ns_to_timer(&cmd->convert_arg,
						TRIG_ROUND_UP);
				err++;
			}
			if (cmd->convert_arg > RTD_MIN_SPEED) {
				cmd->convert_arg = RTD_MIN_SPEED;
				rtd_ns_to_timer(&cmd->convert_arg,
						TRIG_ROUND_DOWN);
				err++;
			}
		}
	} else {
		
		
		if (cmd->convert_arg > 9) {
			cmd->convert_arg = 9;
			err++;
		}
	}

#if 0
	if (cmd->scan_end_arg != cmd->chanlist_len) {
		cmd->scan_end_arg = cmd->chanlist_len;
		err++;
	}
#endif
	if (cmd->stop_src == TRIG_COUNT) {
		

	} else {
		
		if (cmd->stop_arg != 0) {
			cmd->stop_arg = 0;
			err++;
		}
	}

	if (err)
		return 3;


	

	if (cmd->chanlist_len > RTD_MAX_CHANLIST) {
		cmd->chanlist_len = RTD_MAX_CHANLIST;
		err++;
	}
	if (cmd->scan_begin_src == TRIG_TIMER) {
		tmp = cmd->scan_begin_arg;
		rtd_ns_to_timer(&cmd->scan_begin_arg,
				cmd->flags & TRIG_ROUND_MASK);
		if (tmp != cmd->scan_begin_arg)
			err++;

	}
	if (cmd->convert_src == TRIG_TIMER) {
		tmp = cmd->convert_arg;
		rtd_ns_to_timer(&cmd->convert_arg,
				cmd->flags & TRIG_ROUND_MASK);
		if (tmp != cmd->convert_arg)
			err++;

		if (cmd->scan_begin_src == TRIG_TIMER
		    && (cmd->scan_begin_arg
			< (cmd->convert_arg * cmd->scan_end_arg))) {
			cmd->scan_begin_arg =
			    cmd->convert_arg * cmd->scan_end_arg;
			err++;
		}
	}

	if (err)
		return 4;

	return 0;
}

static int rtd_ai_cmd(struct comedi_device *dev, struct comedi_subdevice *s)
{
	struct comedi_cmd *cmd = &s->async->cmd;
	int timer;

	
	RtdPacerStopSource(dev, 0);	
	RtdPacerStop(dev);	
	RtdAdcConversionSource(dev, 0);	
	RtdInterruptMask(dev, 0);
#ifdef USE_DMA
	if (devpriv->flags & DMA0_ACTIVE) {	
		RtdPlxInterruptWrite(dev,	
				     RtdPlxInterruptRead(dev) & ~ICS_DMA0_E);
		abort_dma(dev, 0);
		devpriv->flags &= ~DMA0_ACTIVE;
		if (RtdPlxInterruptRead(dev) & ICS_DMA0_A) {	
			RtdDma0Control(dev, PLX_CLEAR_DMA_INTR_BIT);
		}
	}
	RtdDma0Reset(dev);	
#endif 
	RtdAdcClearFifo(dev);	
	RtdInterruptOverrunClear(dev);
	devpriv->intCount = 0;

	if (!dev->irq) {	
		DPRINTK("rtd520: ERROR! No interrupt available!\n");
		return -ENXIO;
	}

	
	
	rtd_load_channelgain_list(dev, cmd->chanlist_len, cmd->chanlist);

	
	if (cmd->chanlist_len > 1) {
		
		RtdPacerStartSource(dev, 0);	
		RtdBurstStartSource(dev, 1);	
		RtdAdcConversionSource(dev, 2);	
	} else {		
		
		RtdPacerStartSource(dev, 0);	
		RtdAdcConversionSource(dev, 1);	
	}
	RtdAboutCounter(dev, devpriv->fifoLen / 2 - 1);	

	if (TRIG_TIMER == cmd->scan_begin_src) {
		
		
		if (cmd->flags & TRIG_WAKE_EOS) {
			
			
			devpriv->transCount = cmd->chanlist_len;
			devpriv->flags |= SEND_EOS;
		} else {
			
			devpriv->transCount
			    =
			    (TRANS_TARGET_PERIOD * cmd->chanlist_len) /
			    cmd->scan_begin_arg;
			if (devpriv->transCount < cmd->chanlist_len) {
				
				devpriv->transCount = cmd->chanlist_len;
			} else {	
				devpriv->transCount =
				    (devpriv->transCount +
				     cmd->chanlist_len - 1)
				    / cmd->chanlist_len;
				devpriv->transCount *= cmd->chanlist_len;
			}
			devpriv->flags |= SEND_EOS;
		}
		if (devpriv->transCount >= (devpriv->fifoLen / 2)) {
			
			devpriv->transCount = 0;
			devpriv->flags &= ~SEND_EOS;
		} else {
			
			RtdAboutCounter(dev, devpriv->transCount - 1);
		}

		DPRINTK
		    ("rtd520: scanLen=%d tranferCount=%d fifoLen=%d\n  scanTime(ns)=%d flags=0x%x\n",
		     cmd->chanlist_len, devpriv->transCount, devpriv->fifoLen,
		     cmd->scan_begin_arg, devpriv->flags);
	} else {		
		devpriv->transCount = 0;
		devpriv->flags &= ~SEND_EOS;
	}
	RtdPacerClockSource(dev, 1);	
	RtdAboutStopEnable(dev, 1);	

	

	
	switch (cmd->stop_src) {
	case TRIG_COUNT:	
		devpriv->aiCount = cmd->stop_arg * cmd->chanlist_len;
		if ((devpriv->transCount > 0)
		    && (devpriv->transCount > devpriv->aiCount)) {
			devpriv->transCount = devpriv->aiCount;
		}
		break;

	case TRIG_NONE:	
		devpriv->aiCount = -1;	
		break;

	default:
		DPRINTK("rtd520: Warning! ignoring stop_src mode %d\n",
			cmd->stop_src);
	}

	
	switch (cmd->scan_begin_src) {
	case TRIG_TIMER:	
		timer = rtd_ns_to_timer(&cmd->scan_begin_arg,
					TRIG_ROUND_NEAREST);
		
		
		RtdPacerCounter(dev, timer);

		break;

	case TRIG_EXT:
		RtdPacerStartSource(dev, 1);	
		break;

	default:
		DPRINTK("rtd520: Warning! ignoring scan_begin_src mode %d\n",
			cmd->scan_begin_src);
	}

	
	switch (cmd->convert_src) {
	case TRIG_TIMER:	
		if (cmd->chanlist_len > 1) {	
			timer = rtd_ns_to_timer(&cmd->convert_arg,
						TRIG_ROUND_NEAREST);
			
			
			RtdBurstCounter(dev, timer);
		}

		break;

	case TRIG_EXT:		
		RtdBurstStartSource(dev, 2);	
		break;

	default:
		DPRINTK("rtd520: Warning! ignoring convert_src mode %d\n",
			cmd->convert_src);
	}
	

	RtdInterruptClearMask(dev, ~0);	
	RtdInterruptClear(dev);

	
	if (devpriv->transCount > 0) {	
		RtdInterruptMask(dev, IRQM_ADC_ABOUT_CNT);
		DPRINTK("rtd520: Transferring every %d\n", devpriv->transCount);
	} else {		
#ifdef USE_DMA
		devpriv->flags |= DMA0_ACTIVE;

		
		devpriv->dma0Offset = 0;
		RtdDma0Mode(dev, DMA_MODE_BITS);
		RtdDma0Next(dev,	
			    devpriv->dma0Chain[DMA_CHAIN_COUNT - 1].next);
		RtdDma0Source(dev, DMAS_ADFIFO_HALF_FULL);	

		RtdPlxInterruptWrite(dev,	
				     RtdPlxInterruptRead(dev) | ICS_DMA0_E);
		
		RtdDma0Control(dev, PLX_DMA_EN_BIT);	
		RtdDma0Control(dev, PLX_DMA_EN_BIT | PLX_DMA_START_BIT);	
		DPRINTK("rtd520: Using DMA0 transfers. plxInt %x RtdInt %x\n",
			RtdPlxInterruptRead(dev), devpriv->intMask);
#else 
		RtdInterruptMask(dev, IRQM_ADC_ABOUT_CNT);
		DPRINTK("rtd520: Transferring every 1/2 FIFO\n");
#endif 
	}

	
	
	RtdPacerStart(dev);	
	return 0;
}

static int rtd_ai_cancel(struct comedi_device *dev, struct comedi_subdevice *s)
{
	u16 status;

	RtdPacerStopSource(dev, 0);	
	RtdPacerStop(dev);	
	RtdAdcConversionSource(dev, 0);	
	RtdInterruptMask(dev, 0);
	devpriv->aiCount = 0;	
#ifdef USE_DMA
	if (devpriv->flags & DMA0_ACTIVE) {
		RtdPlxInterruptWrite(dev,	
				     RtdPlxInterruptRead(dev) & ~ICS_DMA0_E);
		abort_dma(dev, 0);
		devpriv->flags &= ~DMA0_ACTIVE;
	}
#endif 
	status = RtdInterruptStatus(dev);
	DPRINTK
	    ("rtd520: Acquisition canceled. %ld ints, intStat=%x, overStat=%x\n",
	     devpriv->intCount, status,
	     0xffff & RtdInterruptOverrunStatus(dev));
	return 0;
}

static int rtd_ns_to_timer_base(unsigned int *nanosec,	
				int round_mode, int base)
{				
	int divider;

	switch (round_mode) {
	case TRIG_ROUND_NEAREST:
	default:
		divider = (*nanosec + base / 2) / base;
		break;
	case TRIG_ROUND_DOWN:
		divider = (*nanosec) / base;
		break;
	case TRIG_ROUND_UP:
		divider = (*nanosec + base - 1) / base;
		break;
	}
	if (divider < 2)
		divider = 2;	


	*nanosec = base * divider;
	return divider - 1;	
}

static int rtd_ns_to_timer(unsigned int *ns, int round_mode)
{
	return rtd_ns_to_timer_base(ns, round_mode, RTD_CLOCK_BASE);
}

static int rtd_ao_winsn(struct comedi_device *dev,
			struct comedi_subdevice *s, struct comedi_insn *insn,
			unsigned int *data)
{
	int i;
	int chan = CR_CHAN(insn->chanspec);
	int range = CR_RANGE(insn->chanspec);

	
	RtdDacRange(dev, chan, range);

	for (i = 0; i < insn->n; ++i) {
		int val = data[i] << 3;
		int stat = 0;	
		int ii;

		

		if ((range > 1)	
		    && (data[i] < 2048)) {
			
			val = (((int)data[i]) - 2048) << 3;
		} else {	
			val = data[i] << 3;
		}

		DPRINTK
		    ("comedi: rtd520 DAC chan=%d range=%d writing %d as 0x%x\n",
		     chan, range, data[i], val);

		
		RtdDacFifoPut(dev, chan, val);	
		RtdDacUpdate(dev, chan);	

		devpriv->aoValue[chan] = data[i];	

		for (ii = 0; ii < RTD_DAC_TIMEOUT; ++ii) {
			stat = RtdFifoStatus(dev);
			
			if (stat & ((0 == chan) ? FS_DAC1_NOT_EMPTY :
				    FS_DAC2_NOT_EMPTY))
				break;
			WAIT_QUIETLY;
		}
		if (ii >= RTD_DAC_TIMEOUT) {
			DPRINTK
			    ("rtd520: Error: DAC never finished! FifoStatus=0x%x\n",
			     stat ^ 0x6666);
			return -ETIMEDOUT;
		}
	}

	/* return the number of samples read/written */
	return i;
}

static int rtd_ao_rinsn(struct comedi_device *dev,
			struct comedi_subdevice *s, struct comedi_insn *insn,
			unsigned int *data)
{
	int i;
	int chan = CR_CHAN(insn->chanspec);

	for (i = 0; i < insn->n; i++)
		data[i] = devpriv->aoValue[chan];


	return i;
}

static int rtd_dio_insn_bits(struct comedi_device *dev,
			     struct comedi_subdevice *s,
			     struct comedi_insn *insn, unsigned int *data)
{
	if (insn->n != 2)
		return -EINVAL;

	if (data[0]) {
		s->state &= ~data[0];
		s->state |= data[0] & data[1];

		
		RtdDio0Write(dev, s->state);
	}
	data[1] = RtdDio0Read(dev);

	

	return 2;
}

static int rtd_dio_insn_config(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{
	int chan = CR_CHAN(insn->chanspec);

	switch (data[0]) {
	case INSN_CONFIG_DIO_OUTPUT:
		s->io_bits |= 1 << chan;	
		break;
	case INSN_CONFIG_DIO_INPUT:
		s->io_bits &= ~(1 << chan);
		break;
	case INSN_CONFIG_DIO_QUERY:
		data[1] =
		    (s->io_bits & (1 << chan)) ? COMEDI_OUTPUT : COMEDI_INPUT;
		return insn->n;
		break;
	default:
		return -EINVAL;
	}

	DPRINTK("rtd520: port_0_direction=0x%x (1 means out)\n", s->io_bits);
	
	RtdDioStatusWrite(dev, 0x01);	
	RtdDio0CtrlWrite(dev, s->io_bits);	
	RtdDioStatusWrite(dev, 0);	

	

	

	return 1;
}

static int __devinit rtd520Driver_pci_probe(struct pci_dev *dev,
					    const struct pci_device_id *ent)
{
	return comedi_pci_auto_config(dev, rtd520Driver.driver_name);
}

static void __devexit rtd520Driver_pci_remove(struct pci_dev *dev)
{
	comedi_pci_auto_unconfig(dev);
}

static struct pci_driver rtd520Driver_pci_driver = {
	.id_table = rtd520_pci_table,
	.probe = &rtd520Driver_pci_probe,
	.remove = __devexit_p(&rtd520Driver_pci_remove)
};

static int __init rtd520Driver_init_module(void)
{
	int retval;

	retval = comedi_driver_register(&rtd520Driver);
	if (retval < 0)
		return retval;

	rtd520Driver_pci_driver.name = (char *)rtd520Driver.driver_name;
	return pci_register_driver(&rtd520Driver_pci_driver);
}

static void __exit rtd520Driver_cleanup_module(void)
{
	pci_unregister_driver(&rtd520Driver_pci_driver);
	comedi_driver_unregister(&rtd520Driver);
}

module_init(rtd520Driver_init_module);
module_exit(rtd520Driver_cleanup_module);

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
