/*
  comedi/drivers/s626.c
  Sensoray s626 Comedi driver

  COMEDI - Linux Control and Measurement Device Interface
  Copyright (C) 2000 David A. Schleef <ds@schleef.org>

  Based on Sensoray Model 626 Linux driver Version 0.2
  Copyright (C) 2002-2004 Sensoray Co., Inc.

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
#include <linux/kernel.h>
#include <linux/types.h>

#include "../comedidev.h"

#include "comedi_pci.h"

#include "comedi_fc.h"
#include "s626.h"

MODULE_AUTHOR("Gianluca Palli <gpalli@deis.unibo.it>");
MODULE_DESCRIPTION("Sensoray 626 Comedi driver module");
MODULE_LICENSE("GPL");

struct s626_board {
	const char *name;
	int ai_chans;
	int ai_bits;
	int ao_chans;
	int ao_bits;
	int dio_chans;
	int dio_banks;
	int enc_chans;
};

static const struct s626_board s626_boards[] = {
	{
	 .name = "s626",
	 .ai_chans = S626_ADC_CHANNELS,
	 .ai_bits = 14,
	 .ao_chans = S626_DAC_CHANNELS,
	 .ao_bits = 13,
	 .dio_chans = S626_DIO_CHANNELS,
	 .dio_banks = S626_DIO_BANKS,
	 .enc_chans = S626_ENCODER_CHANNELS,
	 }
};

#define thisboard ((const struct s626_board *)dev->board_ptr)
#define PCI_VENDOR_ID_S626 0x1131
#define PCI_DEVICE_ID_S626 0x7146

static DEFINE_PCI_DEVICE_TABLE(s626_pci_table) = {
	{PCI_VENDOR_ID_S626, PCI_DEVICE_ID_S626, 0x6000, 0x0272, 0, 0, 0},
	{0}
};

MODULE_DEVICE_TABLE(pci, s626_pci_table);

static int s626_attach(struct comedi_device *dev, struct comedi_devconfig *it);
static int s626_detach(struct comedi_device *dev);

static struct comedi_driver driver_s626 = {
	.driver_name = "s626",
	.module = THIS_MODULE,
	.attach = s626_attach,
	.detach = s626_detach,
};

struct s626_private {
	struct pci_dev *pdev;
	void *base_addr;
	int got_regions;
	short allocatedBuf;
	uint8_t ai_cmd_running;	
	uint8_t ai_continous;	
	int ai_sample_count;	
	unsigned int ai_sample_timer;
	
	int ai_convert_count;	
	unsigned int ai_convert_timer;
	
	uint16_t CounterIntEnabs;
	
	uint8_t AdcItems;	
	struct bufferDMA RPSBuf;	
	struct bufferDMA ANABuf;
	
	uint32_t *pDacWBuf;
	
	uint16_t Dacpol;	
	uint8_t TrimSetpoint[12];	
	uint16_t ChargeEnabled;	
	
	uint16_t WDInterval;	
	uint32_t I2CAdrs;
	
	
	unsigned int ao_readback[S626_DAC_CHANNELS];
};

struct dio_private {
	uint16_t RDDIn;
	uint16_t WRDOut;
	uint16_t RDEdgSel;
	uint16_t WREdgSel;
	uint16_t RDCapSel;
	uint16_t WRCapSel;
	uint16_t RDCapFlg;
	uint16_t RDIntSel;
	uint16_t WRIntSel;
};

static struct dio_private dio_private_A = {
	.RDDIn = LP_RDDINA,
	.WRDOut = LP_WRDOUTA,
	.RDEdgSel = LP_RDEDGSELA,
	.WREdgSel = LP_WREDGSELA,
	.RDCapSel = LP_RDCAPSELA,
	.WRCapSel = LP_WRCAPSELA,
	.RDCapFlg = LP_RDCAPFLGA,
	.RDIntSel = LP_RDINTSELA,
	.WRIntSel = LP_WRINTSELA,
};

static struct dio_private dio_private_B = {
	.RDDIn = LP_RDDINB,
	.WRDOut = LP_WRDOUTB,
	.RDEdgSel = LP_RDEDGSELB,
	.WREdgSel = LP_WREDGSELB,
	.RDCapSel = LP_RDCAPSELB,
	.WRCapSel = LP_WRCAPSELB,
	.RDCapFlg = LP_RDCAPFLGB,
	.RDIntSel = LP_RDINTSELB,
	.WRIntSel = LP_WRINTSELB,
};

static struct dio_private dio_private_C = {
	.RDDIn = LP_RDDINC,
	.WRDOut = LP_WRDOUTC,
	.RDEdgSel = LP_RDEDGSELC,
	.WREdgSel = LP_WREDGSELC,
	.RDCapSel = LP_RDCAPSELC,
	.WRCapSel = LP_WRCAPSELC,
	.RDCapFlg = LP_RDCAPFLGC,
	.RDIntSel = LP_RDINTSELC,
	.WRIntSel = LP_WRINTSELC,
};


#define devpriv ((struct s626_private *)dev->private)
#define diopriv ((struct dio_private *)s->private)

static int __devinit driver_s626_pci_probe(struct pci_dev *dev,
					   const struct pci_device_id *ent)
{
	return comedi_pci_auto_config(dev, driver_s626.driver_name);
}

static void __devexit driver_s626_pci_remove(struct pci_dev *dev)
{
	comedi_pci_auto_unconfig(dev);
}

static struct pci_driver driver_s626_pci_driver = {
	.id_table = s626_pci_table,
	.probe = &driver_s626_pci_probe,
	.remove = __devexit_p(&driver_s626_pci_remove)
};

static int __init driver_s626_init_module(void)
{
	int retval;

	retval = comedi_driver_register(&driver_s626);
	if (retval < 0)
		return retval;

	driver_s626_pci_driver.name = (char *)driver_s626.driver_name;
	return pci_register_driver(&driver_s626_pci_driver);
}

static void __exit driver_s626_cleanup_module(void)
{
	pci_unregister_driver(&driver_s626_pci_driver);
	comedi_driver_unregister(&driver_s626);
}

module_init(driver_s626_init_module);
module_exit(driver_s626_cleanup_module);

static int s626_ai_insn_config(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data);
static int s626_ai_insn_read(struct comedi_device *dev,
			     struct comedi_subdevice *s,
			     struct comedi_insn *insn, unsigned int *data);
static int s626_ai_cmd(struct comedi_device *dev, struct comedi_subdevice *s);
static int s626_ai_cmdtest(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_cmd *cmd);
static int s626_ai_cancel(struct comedi_device *dev,
			  struct comedi_subdevice *s);
static int s626_ao_winsn(struct comedi_device *dev, struct comedi_subdevice *s,
			 struct comedi_insn *insn, unsigned int *data);
static int s626_ao_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
			 struct comedi_insn *insn, unsigned int *data);
static int s626_dio_insn_bits(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data);
static int s626_dio_insn_config(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data);
static int s626_dio_set_irq(struct comedi_device *dev, unsigned int chan);
static int s626_dio_reset_irq(struct comedi_device *dev, unsigned int gruop,
			      unsigned int mask);
static int s626_dio_clear_irq(struct comedi_device *dev);
static int s626_enc_insn_config(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data);
static int s626_enc_insn_read(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data);
static int s626_enc_insn_write(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data);
static int s626_ns_to_timer(int *nanosec, int round_mode);
static int s626_ai_load_polllist(uint8_t *ppl, struct comedi_cmd *cmd);
static int s626_ai_inttrig(struct comedi_device *dev,
			   struct comedi_subdevice *s, unsigned int trignum);
static irqreturn_t s626_irq_handler(int irq, void *d);
static unsigned int s626_ai_reg_to_uint(int data);


static void s626_dio_init(struct comedi_device *dev);
static void ResetADC(struct comedi_device *dev, uint8_t * ppl);
static void LoadTrimDACs(struct comedi_device *dev);
static void WriteTrimDAC(struct comedi_device *dev, uint8_t LogicalChan,
			 uint8_t DacData);
static uint8_t I2Cread(struct comedi_device *dev, uint8_t addr);
static uint32_t I2Chandshake(struct comedi_device *dev, uint32_t val);
static void SetDAC(struct comedi_device *dev, uint16_t chan, short dacdata);
static void SendDAC(struct comedi_device *dev, uint32_t val);
static void WriteMISC2(struct comedi_device *dev, uint16_t NewImage);
static void DEBItransfer(struct comedi_device *dev);
static uint16_t DEBIread(struct comedi_device *dev, uint16_t addr);
static void DEBIwrite(struct comedi_device *dev, uint16_t addr, uint16_t wdata);
static void DEBIreplace(struct comedi_device *dev, uint16_t addr, uint16_t mask,
			uint16_t wdata);
static void CloseDMAB(struct comedi_device *dev, struct bufferDMA *pdma,
		      size_t bsize);

struct enc_private {
	
	uint16_t(*GetEnable) (struct comedi_device *dev, struct enc_private *);	
	uint16_t(*GetIntSrc) (struct comedi_device *dev, struct enc_private *);	
	uint16_t(*GetLoadTrig) (struct comedi_device *dev, struct enc_private *);	
	uint16_t(*GetMode) (struct comedi_device *dev, struct enc_private *);	
	void (*PulseIndex) (struct comedi_device *dev, struct enc_private *);	
	void (*SetEnable) (struct comedi_device *dev, struct enc_private *, uint16_t enab);	
	void (*SetIntSrc) (struct comedi_device *dev, struct enc_private *, uint16_t IntSource);	
	void (*SetLoadTrig) (struct comedi_device *dev, struct enc_private *, uint16_t Trig);	
	void (*SetMode) (struct comedi_device *dev, struct enc_private *, uint16_t Setup, uint16_t DisableIntSrc);	
	void (*ResetCapFlags) (struct comedi_device *dev, struct enc_private *);	

	uint16_t MyCRA;		
	uint16_t MyCRB;		
	uint16_t MyLatchLsw;	
	
	uint16_t MyEventBits[4];	
};

#define encpriv ((struct enc_private *)(dev->subdevices+5)->private)

static void s626_timer_load(struct comedi_device *dev, struct enc_private *k,
			    int tick);
static uint32_t ReadLatch(struct comedi_device *dev, struct enc_private *k);
static void ResetCapFlags_A(struct comedi_device *dev, struct enc_private *k);
static void ResetCapFlags_B(struct comedi_device *dev, struct enc_private *k);
static uint16_t GetMode_A(struct comedi_device *dev, struct enc_private *k);
static uint16_t GetMode_B(struct comedi_device *dev, struct enc_private *k);
static void SetMode_A(struct comedi_device *dev, struct enc_private *k,
		      uint16_t Setup, uint16_t DisableIntSrc);
static void SetMode_B(struct comedi_device *dev, struct enc_private *k,
		      uint16_t Setup, uint16_t DisableIntSrc);
static void SetEnable_A(struct comedi_device *dev, struct enc_private *k,
			uint16_t enab);
static void SetEnable_B(struct comedi_device *dev, struct enc_private *k,
			uint16_t enab);
static uint16_t GetEnable_A(struct comedi_device *dev, struct enc_private *k);
static uint16_t GetEnable_B(struct comedi_device *dev, struct enc_private *k);
static void SetLatchSource(struct comedi_device *dev, struct enc_private *k,
			   uint16_t value);
static void SetLoadTrig_A(struct comedi_device *dev, struct enc_private *k,
			  uint16_t Trig);
static void SetLoadTrig_B(struct comedi_device *dev, struct enc_private *k,
			  uint16_t Trig);
static uint16_t GetLoadTrig_A(struct comedi_device *dev, struct enc_private *k);
static uint16_t GetLoadTrig_B(struct comedi_device *dev, struct enc_private *k);
static void SetIntSrc_B(struct comedi_device *dev, struct enc_private *k,
			uint16_t IntSource);
static void SetIntSrc_A(struct comedi_device *dev, struct enc_private *k,
			uint16_t IntSource);
static uint16_t GetIntSrc_A(struct comedi_device *dev, struct enc_private *k);
static uint16_t GetIntSrc_B(struct comedi_device *dev, struct enc_private *k);
static void PulseIndex_A(struct comedi_device *dev, struct enc_private *k);
static void PulseIndex_B(struct comedi_device *dev, struct enc_private *k);
static void Preload(struct comedi_device *dev, struct enc_private *k,
		    uint32_t value);
static void CountersInit(struct comedi_device *dev);


#define INDXMASK(C)		(1 << (((C) > 2) ? ((C) * 2 - 1) : ((C) * 2 +  4)))
#define OVERMASK(C)		(1 << (((C) > 2) ? ((C) * 2 + 5) : ((C) * 2 + 10)))
#define EVBITS(C)		{ 0, OVERMASK(C), INDXMASK(C), OVERMASK(C) | INDXMASK(C) }


static struct enc_private enc_private_data[] = {
	{
	 .GetEnable = GetEnable_A,
	 .GetIntSrc = GetIntSrc_A,
	 .GetLoadTrig = GetLoadTrig_A,
	 .GetMode = GetMode_A,
	 .PulseIndex = PulseIndex_A,
	 .SetEnable = SetEnable_A,
	 .SetIntSrc = SetIntSrc_A,
	 .SetLoadTrig = SetLoadTrig_A,
	 .SetMode = SetMode_A,
	 .ResetCapFlags = ResetCapFlags_A,
	 .MyCRA = LP_CR0A,
	 .MyCRB = LP_CR0B,
	 .MyLatchLsw = LP_CNTR0ALSW,
	 .MyEventBits = EVBITS(0),
	 },
	{
	 .GetEnable = GetEnable_A,
	 .GetIntSrc = GetIntSrc_A,
	 .GetLoadTrig = GetLoadTrig_A,
	 .GetMode = GetMode_A,
	 .PulseIndex = PulseIndex_A,
	 .SetEnable = SetEnable_A,
	 .SetIntSrc = SetIntSrc_A,
	 .SetLoadTrig = SetLoadTrig_A,
	 .SetMode = SetMode_A,
	 .ResetCapFlags = ResetCapFlags_A,
	 .MyCRA = LP_CR1A,
	 .MyCRB = LP_CR1B,
	 .MyLatchLsw = LP_CNTR1ALSW,
	 .MyEventBits = EVBITS(1),
	 },
	{
	 .GetEnable = GetEnable_A,
	 .GetIntSrc = GetIntSrc_A,
	 .GetLoadTrig = GetLoadTrig_A,
	 .GetMode = GetMode_A,
	 .PulseIndex = PulseIndex_A,
	 .SetEnable = SetEnable_A,
	 .SetIntSrc = SetIntSrc_A,
	 .SetLoadTrig = SetLoadTrig_A,
	 .SetMode = SetMode_A,
	 .ResetCapFlags = ResetCapFlags_A,
	 .MyCRA = LP_CR2A,
	 .MyCRB = LP_CR2B,
	 .MyLatchLsw = LP_CNTR2ALSW,
	 .MyEventBits = EVBITS(2),
	 },
	{
	 .GetEnable = GetEnable_B,
	 .GetIntSrc = GetIntSrc_B,
	 .GetLoadTrig = GetLoadTrig_B,
	 .GetMode = GetMode_B,
	 .PulseIndex = PulseIndex_B,
	 .SetEnable = SetEnable_B,
	 .SetIntSrc = SetIntSrc_B,
	 .SetLoadTrig = SetLoadTrig_B,
	 .SetMode = SetMode_B,
	 .ResetCapFlags = ResetCapFlags_B,
	 .MyCRA = LP_CR0A,
	 .MyCRB = LP_CR0B,
	 .MyLatchLsw = LP_CNTR0BLSW,
	 .MyEventBits = EVBITS(3),
	 },
	{
	 .GetEnable = GetEnable_B,
	 .GetIntSrc = GetIntSrc_B,
	 .GetLoadTrig = GetLoadTrig_B,
	 .GetMode = GetMode_B,
	 .PulseIndex = PulseIndex_B,
	 .SetEnable = SetEnable_B,
	 .SetIntSrc = SetIntSrc_B,
	 .SetLoadTrig = SetLoadTrig_B,
	 .SetMode = SetMode_B,
	 .ResetCapFlags = ResetCapFlags_B,
	 .MyCRA = LP_CR1A,
	 .MyCRB = LP_CR1B,
	 .MyLatchLsw = LP_CNTR1BLSW,
	 .MyEventBits = EVBITS(4),
	 },
	{
	 .GetEnable = GetEnable_B,
	 .GetIntSrc = GetIntSrc_B,
	 .GetLoadTrig = GetLoadTrig_B,
	 .GetMode = GetMode_B,
	 .PulseIndex = PulseIndex_B,
	 .SetEnable = SetEnable_B,
	 .SetIntSrc = SetIntSrc_B,
	 .SetLoadTrig = SetLoadTrig_B,
	 .SetMode = SetMode_B,
	 .ResetCapFlags = ResetCapFlags_B,
	 .MyCRA = LP_CR2A,
	 .MyCRB = LP_CR2B,
	 .MyLatchLsw = LP_CNTR2BLSW,
	 .MyEventBits = EVBITS(5),
	 },
};

#define MC_ENABLE(REGADRS, CTRLWORD)	writel(((uint32_t)(CTRLWORD) << 16) | (uint32_t)(CTRLWORD), devpriv->base_addr+(REGADRS))

#define MC_DISABLE(REGADRS, CTRLWORD)	writel((uint32_t)(CTRLWORD) << 16 , devpriv->base_addr+(REGADRS))

#define MC_TEST(REGADRS, CTRLWORD)	((readl(devpriv->base_addr+(REGADRS)) & CTRLWORD) != 0)

#define WR7146(REGARDS, CTRLWORD) writel(CTRLWORD, devpriv->base_addr+(REGARDS))

#define RR7146(REGARDS)		readl(devpriv->base_addr+(REGARDS))

#define BUGFIX_STREG(REGADRS)   (REGADRS - 4)

#define VECTPORT(VECTNUM)		(P_TSL2 + ((VECTNUM) << 2))
#define SETVECT(VECTNUM, VECTVAL)	WR7146(VECTPORT(VECTNUM), (VECTVAL))

#define I2C_B2(ATTR, VAL)	(((ATTR) << 6) | ((VAL) << 24))
#define I2C_B1(ATTR, VAL)	(((ATTR) << 4) | ((VAL) << 16))
#define I2C_B0(ATTR, VAL)	(((ATTR) << 2) | ((VAL) <<  8))

static const struct comedi_lrange s626_range_table = { 2, {
							   RANGE(-5, 5),
							   RANGE(-10, 10),
							   }
};

static int s626_attach(struct comedi_device *dev, struct comedi_devconfig *it)
{
	int result;
	int i;
	int ret;
	resource_size_t resourceStart;
	dma_addr_t appdma;
	struct comedi_subdevice *s;
	const struct pci_device_id *ids;
	struct pci_dev *pdev = NULL;

	if (alloc_private(dev, sizeof(struct s626_private)) < 0)
		return -ENOMEM;

	for (i = 0; i < (ARRAY_SIZE(s626_pci_table) - 1) && !pdev; i++) {
		ids = &s626_pci_table[i];
		do {
			pdev = pci_get_subsys(ids->vendor, ids->device,
					      ids->subvendor, ids->subdevice,
					      pdev);

			if ((it->options[0] || it->options[1]) && pdev) {
				
				if (pdev->bus->number == it->options[0] &&
				    PCI_SLOT(pdev->devfn) == it->options[1])
					break;
			} else
				break;
		} while (1);
	}
	devpriv->pdev = pdev;

	if (pdev == NULL) {
		printk(KERN_ERR "s626_attach: Board not present!!!\n");
		return -ENODEV;
	}

	result = comedi_pci_enable(pdev, "s626");
	if (result < 0) {
		printk(KERN_ERR "s626_attach: comedi_pci_enable fails\n");
		return -ENODEV;
	}
	devpriv->got_regions = 1;

	resourceStart = pci_resource_start(devpriv->pdev, 0);

	devpriv->base_addr = ioremap(resourceStart, SIZEOF_ADDRESS_SPACE);
	if (devpriv->base_addr == NULL) {
		printk(KERN_ERR "s626_attach: IOREMAP failed\n");
		return -ENODEV;
	}

	if (devpriv->base_addr) {
		
		writel(0, devpriv->base_addr + P_IER);

		
		writel(MC1_SOFT_RESET, devpriv->base_addr + P_MC1);

		
		DEBUG("s626_attach: DMA ALLOCATION\n");

		
		devpriv->allocatedBuf = 0;

		devpriv->ANABuf.LogicalBase =
		    pci_alloc_consistent(devpriv->pdev, DMABUF_SIZE, &appdma);

		if (devpriv->ANABuf.LogicalBase == NULL) {
			printk(KERN_ERR "s626_attach: DMA Memory mapping error\n");
			return -ENOMEM;
		}

		devpriv->ANABuf.PhysicalBase = appdma;

		DEBUG
		    ("s626_attach: AllocDMAB ADC Logical=%p, bsize=%d, Physical=0x%x\n",
		     devpriv->ANABuf.LogicalBase, DMABUF_SIZE,
		     (uint32_t) devpriv->ANABuf.PhysicalBase);

		devpriv->allocatedBuf++;

		devpriv->RPSBuf.LogicalBase =
		    pci_alloc_consistent(devpriv->pdev, DMABUF_SIZE, &appdma);

		if (devpriv->RPSBuf.LogicalBase == NULL) {
			printk(KERN_ERR "s626_attach: DMA Memory mapping error\n");
			return -ENOMEM;
		}

		devpriv->RPSBuf.PhysicalBase = appdma;

		DEBUG
		    ("s626_attach: AllocDMAB RPS Logical=%p, bsize=%d, Physical=0x%x\n",
		     devpriv->RPSBuf.LogicalBase, DMABUF_SIZE,
		     (uint32_t) devpriv->RPSBuf.PhysicalBase);

		devpriv->allocatedBuf++;

	}

	dev->board_ptr = s626_boards;
	dev->board_name = thisboard->name;

	if (alloc_subdevices(dev, 6) < 0)
		return -ENOMEM;

	dev->iobase = (unsigned long)devpriv->base_addr;
	dev->irq = devpriv->pdev->irq;

	
	if (dev->irq == 0) {
		printk(KERN_ERR " unknown irq (bad)\n");
	} else {
		ret = request_irq(dev->irq, s626_irq_handler, IRQF_SHARED,
				  "s626", dev);

		if (ret < 0) {
			printk(KERN_ERR " irq not available\n");
			dev->irq = 0;
		}
	}

	DEBUG("s626_attach: -- it opts  %d,%d --\n",
	      it->options[0], it->options[1]);

	s = dev->subdevices + 0;
	
	dev->read_subdev = s;
	
	s->type = COMEDI_SUBD_AI;
	s->subdev_flags = SDF_READABLE | SDF_DIFF | SDF_CMD_READ;
	s->n_chan = thisboard->ai_chans;
	s->maxdata = (0xffff >> 2);
	s->range_table = &s626_range_table;
	s->len_chanlist = thisboard->ai_chans;	
	s->insn_config = s626_ai_insn_config;
	s->insn_read = s626_ai_insn_read;
	s->do_cmd = s626_ai_cmd;
	s->do_cmdtest = s626_ai_cmdtest;
	s->cancel = s626_ai_cancel;

	s = dev->subdevices + 1;
	
	s->type = COMEDI_SUBD_AO;
	s->subdev_flags = SDF_WRITABLE | SDF_READABLE;
	s->n_chan = thisboard->ao_chans;
	s->maxdata = (0x3fff);
	s->range_table = &range_bipolar10;
	s->insn_write = s626_ao_winsn;
	s->insn_read = s626_ao_rinsn;

	s = dev->subdevices + 2;
	
	s->type = COMEDI_SUBD_DIO;
	s->subdev_flags = SDF_WRITABLE | SDF_READABLE;
	s->n_chan = S626_DIO_CHANNELS;
	s->maxdata = 1;
	s->io_bits = 0xffff;
	s->private = &dio_private_A;
	s->range_table = &range_digital;
	s->insn_config = s626_dio_insn_config;
	s->insn_bits = s626_dio_insn_bits;

	s = dev->subdevices + 3;
	
	s->type = COMEDI_SUBD_DIO;
	s->subdev_flags = SDF_WRITABLE | SDF_READABLE;
	s->n_chan = 16;
	s->maxdata = 1;
	s->io_bits = 0xffff;
	s->private = &dio_private_B;
	s->range_table = &range_digital;
	s->insn_config = s626_dio_insn_config;
	s->insn_bits = s626_dio_insn_bits;

	s = dev->subdevices + 4;
	
	s->type = COMEDI_SUBD_DIO;
	s->subdev_flags = SDF_WRITABLE | SDF_READABLE;
	s->n_chan = 16;
	s->maxdata = 1;
	s->io_bits = 0xffff;
	s->private = &dio_private_C;
	s->range_table = &range_digital;
	s->insn_config = s626_dio_insn_config;
	s->insn_bits = s626_dio_insn_bits;

	s = dev->subdevices + 5;
	
	s->type = COMEDI_SUBD_COUNTER;
	s->subdev_flags = SDF_WRITABLE | SDF_READABLE | SDF_LSAMPL;
	s->n_chan = thisboard->enc_chans;
	s->private = enc_private_data;
	s->insn_config = s626_enc_insn_config;
	s->insn_read = s626_enc_insn_read;
	s->insn_write = s626_enc_insn_write;
	s->maxdata = 0xffffff;
	s->range_table = &range_unknown;

	
	devpriv->ai_cmd_running = 0;

	if (devpriv->base_addr && (devpriv->allocatedBuf == 2)) {
		dma_addr_t pPhysBuf;
		uint16_t chan;

		
		MC_ENABLE(P_MC1, MC1_DEBI | MC1_AUDIO | MC1_I2C);
		
		WR7146(P_DEBICFG, DEBI_CFG_SLAVE16	
		       
		       | (DEBI_TOUT << DEBI_CFG_TOUT_BIT)

		       
		       
		       
		       |DEBI_SWAP	
		       
		       | DEBI_CFG_INTEL);	
		
		
		DEBUG("s626_attach: %d debi init -- %d\n",
		      DEBI_CFG_SLAVE16 | (DEBI_TOUT << DEBI_CFG_TOUT_BIT) |
		      DEBI_SWAP | DEBI_CFG_INTEL,
		      DEBI_CFG_INTEL | DEBI_CFG_TOQ | DEBI_CFG_INCQ |
		      DEBI_CFG_16Q);

		
		

		
		WR7146(P_DEBIPAGE, DEBI_PAGE_DISABLE);	

		
		WR7146(P_GPIO, GPIO_BASE | GPIO1_HI);


		
		
		

		devpriv->I2CAdrs = 0xA0;	
		

		
		
		WR7146(P_I2CSTAT, I2C_CLKSEL | I2C_ABORT);
		
		MC_ENABLE(P_MC2, MC2_UPLD_IIC);
		
		while ((RR7146(P_MC2) & MC2_UPLD_IIC) == 0)
			;
		

		for (i = 0; i < 2; i++) {
			WR7146(P_I2CSTAT, I2C_CLKSEL);
			
			MC_ENABLE(P_MC2, MC2_UPLD_IIC);	
			while (!MC_TEST(P_MC2, MC2_UPLD_IIC))
				;
			
		}


		WR7146(P_ACON2, ACON2_INIT);

		WR7146(P_TSL1, RSD1 | SIB_A1);
		
		WR7146(P_TSL1 + 4, RSD1 | SIB_A1 | EOS);
		

		
		WR7146(P_ACON1, ACON1_ADCSTART);

		

		
		WR7146(P_RPSADDR1, (uint32_t) devpriv->RPSBuf.PhysicalBase);

		WR7146(P_RPSPAGE1, 0);
		
		WR7146(P_RPS1_TOUT, 0);	







		

		

		WR7146(P_PCI_BT_A, 0);


		pPhysBuf =
		    devpriv->ANABuf.PhysicalBase +
		    (DAC_WDMABUF_OS * sizeof(uint32_t));

		WR7146(P_BASEA2_OUT, (uint32_t) pPhysBuf);	
		WR7146(P_PROTA2_OUT, (uint32_t) (pPhysBuf + sizeof(uint32_t)));	

		devpriv->pDacWBuf =
		    (uint32_t *) devpriv->ANABuf.LogicalBase + DAC_WDMABUF_OS;


		WR7146(P_PAGEA2_OUT, 8);


		SETVECT(0, XSD2 | RSD3 | SIB_A2 | EOS);
		

		SETVECT(1, LF_A2);
		

		
		WR7146(P_ACON1, ACON1_DACSTART);

		


		LoadTrimDACs(dev);
		LoadTrimDACs(dev);	


		for (chan = 0; chan < S626_DAC_CHANNELS; chan++)
			SetDAC(dev, chan, 0);

		devpriv->ChargeEnabled = 0;

		devpriv->WDInterval = 0;

		devpriv->CounterIntEnabs = 0;

		
		CountersInit(dev);

		WriteMISC2(dev, (uint16_t) (DEBIread(dev,
						     LP_RDMISC2) &
					    MISC2_BATT_ENABLE));

		
		s626_dio_init(dev);

		
		
	}

	DEBUG("s626_attach: comedi%d s626 attached %04x\n", dev->minor,
	      (uint32_t) devpriv->base_addr);

	return 1;
}

static unsigned int s626_ai_reg_to_uint(int data)
{
	unsigned int tempdata;

	tempdata = (data >> 18);
	if (tempdata & 0x2000)
		tempdata &= 0x1fff;
	else
		tempdata += (1 << 13);

	return tempdata;
}


static irqreturn_t s626_irq_handler(int irq, void *d)
{
	struct comedi_device *dev = d;
	struct comedi_subdevice *s;
	struct comedi_cmd *cmd;
	struct enc_private *k;
	unsigned long flags;
	int32_t *readaddr;
	uint32_t irqtype, irqstatus;
	int i = 0;
	short tempdata;
	uint8_t group;
	uint16_t irqbit;

	DEBUG("s626_irq_handler: interrupt request received!!!\n");

	if (dev->attached == 0)
		return IRQ_NONE;
	
	spin_lock_irqsave(&dev->spinlock, flags);

	
	irqstatus = readl(devpriv->base_addr + P_IER);

	
	irqtype = readl(devpriv->base_addr + P_ISR);

	
	writel(0, devpriv->base_addr + P_IER);

	
	writel(irqtype, devpriv->base_addr + P_ISR);

	
	DEBUG("s626_irq_handler: interrupt type %d\n", irqtype);

	switch (irqtype) {
	case IRQ_RPS1:		

		DEBUG("s626_irq_handler: RPS1 irq detected\n");

		
		s = dev->subdevices;
		cmd = &(s->async->cmd);

		readaddr = (int32_t *) devpriv->ANABuf.LogicalBase + 1;

		
		for (i = 0; i < (s->async->cmd.chanlist_len); i++) {
			
			
			tempdata = s626_ai_reg_to_uint((int)*readaddr);
			readaddr++;

			
			
			if (cfc_write_to_buffer(s, tempdata) == 0)
				printk
				    ("s626_irq_handler: cfc_write_to_buffer error!\n");

			DEBUG("s626_irq_handler: ai channel %d acquired: %d\n",
			      i, tempdata);
		}

		
		s->async->events |= COMEDI_CB_EOS;

		if (!(devpriv->ai_continous))
			devpriv->ai_sample_count--;
		if (devpriv->ai_sample_count <= 0) {
			devpriv->ai_cmd_running = 0;

			
			MC_DISABLE(P_MC1, MC1_ERPS1);

			
			s->async->events |= COMEDI_CB_EOA;

			
			irqstatus = 0;
		}

		if (devpriv->ai_cmd_running && cmd->scan_begin_src == TRIG_EXT) {
			DEBUG
			    ("s626_irq_handler: enable interrupt on dio channel %d\n",
			     cmd->scan_begin_arg);

			s626_dio_set_irq(dev, cmd->scan_begin_arg);

			DEBUG("s626_irq_handler: External trigger is set!!!\n");
		}
		
		DEBUG("s626_irq_handler: events %d\n", s->async->events);
		comedi_event(dev, s);
		break;
	case IRQ_GPIO3:	

		DEBUG("s626_irq_handler: GPIO3 irq detected\n");

		
		s = dev->subdevices;
		cmd = &(s->async->cmd);

		

		for (group = 0; group < S626_DIO_BANKS; group++) {
			irqbit = 0;
			
			irqbit = DEBIread(dev,
					  ((struct dio_private *)(dev->
								  subdevices +
								  2 +
								  group)->
					   private)->RDCapFlg);

			
			if (irqbit) {
				s626_dio_reset_irq(dev, group, irqbit);
				DEBUG
				    ("s626_irq_handler: check interrupt on dio group %d %d\n",
				     group, i);
				if (devpriv->ai_cmd_running) {
					
					if ((irqbit >> (cmd->start_arg -
							(16 * group)))
					    == 1 && cmd->start_src == TRIG_EXT) {
						DEBUG
						    ("s626_irq_handler: Edge capture interrupt received from channel %d\n",
						     cmd->start_arg);

						
						MC_ENABLE(P_MC1, MC1_ERPS1);

						DEBUG
						    ("s626_irq_handler: acquisition start triggered!!!\n");

						if (cmd->scan_begin_src ==
						    TRIG_EXT) {
							DEBUG
							    ("s626_ai_cmd: enable interrupt on dio channel %d\n",
							     cmd->
							     scan_begin_arg);

							s626_dio_set_irq(dev,
									 cmd->scan_begin_arg);

							DEBUG
							    ("s626_irq_handler: External scan trigger is set!!!\n");
						}
					}
					if ((irqbit >> (cmd->scan_begin_arg -
							(16 * group)))
					    == 1
					    && cmd->scan_begin_src ==
					    TRIG_EXT) {
						DEBUG
						    ("s626_irq_handler: Edge capture interrupt received from channel %d\n",
						     cmd->scan_begin_arg);

						
						MC_ENABLE(P_MC2, MC2_ADC_RPS);

						DEBUG
						    ("s626_irq_handler: scan triggered!!! %d\n",
						     devpriv->ai_sample_count);
						if (cmd->convert_src ==
						    TRIG_EXT) {

							DEBUG
							    ("s626_ai_cmd: enable interrupt on dio channel %d group %d\n",
							     cmd->convert_arg -
							     (16 * group),
							     group);

							devpriv->ai_convert_count
							    = cmd->chanlist_len;

							s626_dio_set_irq(dev,
									 cmd->convert_arg);

							DEBUG
							    ("s626_irq_handler: External convert trigger is set!!!\n");
						}

						if (cmd->convert_src ==
						    TRIG_TIMER) {
							k = &encpriv[5];
							devpriv->ai_convert_count
							    = cmd->chanlist_len;
							k->SetEnable(dev, k,
								     CLKENAB_ALWAYS);
						}
					}
					if ((irqbit >> (cmd->convert_arg -
							(16 * group)))
					    == 1
					    && cmd->convert_src == TRIG_EXT) {
						DEBUG
						    ("s626_irq_handler: Edge capture interrupt received from channel %d\n",
						     cmd->convert_arg);

						
						MC_ENABLE(P_MC2, MC2_ADC_RPS);

						DEBUG
						    ("s626_irq_handler: adc convert triggered!!!\n");

						devpriv->ai_convert_count--;

						if (devpriv->ai_convert_count >
						    0) {

							DEBUG
							    ("s626_ai_cmd: enable interrupt on dio channel %d group %d\n",
							     cmd->convert_arg -
							     (16 * group),
							     group);

							s626_dio_set_irq(dev,
									 cmd->convert_arg);

							DEBUG
							    ("s626_irq_handler: External trigger is set!!!\n");
						}
					}
				}
				break;
			}
		}

		
		irqbit = DEBIread(dev, LP_RDMISC2);

		
		DEBUG("s626_irq_handler: check counters interrupt %d\n",
		      irqbit);

		if (irqbit & IRQ_COINT1A) {
			DEBUG
			    ("s626_irq_handler: interrupt on counter 1A overflow\n");
			k = &encpriv[0];

			
			k->ResetCapFlags(dev, k);
		}
		if (irqbit & IRQ_COINT2A) {
			DEBUG
			    ("s626_irq_handler: interrupt on counter 2A overflow\n");
			k = &encpriv[1];

			
			k->ResetCapFlags(dev, k);
		}
		if (irqbit & IRQ_COINT3A) {
			DEBUG
			    ("s626_irq_handler: interrupt on counter 3A overflow\n");
			k = &encpriv[2];

			
			k->ResetCapFlags(dev, k);
		}
		if (irqbit & IRQ_COINT1B) {
			DEBUG
			    ("s626_irq_handler: interrupt on counter 1B overflow\n");
			k = &encpriv[3];

			
			k->ResetCapFlags(dev, k);
		}
		if (irqbit & IRQ_COINT2B) {
			DEBUG
			    ("s626_irq_handler: interrupt on counter 2B overflow\n");
			k = &encpriv[4];

			
			k->ResetCapFlags(dev, k);

			if (devpriv->ai_convert_count > 0) {
				devpriv->ai_convert_count--;
				if (devpriv->ai_convert_count == 0)
					k->SetEnable(dev, k, CLKENAB_INDEX);

				if (cmd->convert_src == TRIG_TIMER) {
					DEBUG
					    ("s626_irq_handler: conver timer trigger!!! %d\n",
					     devpriv->ai_convert_count);

					
					MC_ENABLE(P_MC2, MC2_ADC_RPS);
				}
			}
		}
		if (irqbit & IRQ_COINT3B) {
			DEBUG
			    ("s626_irq_handler: interrupt on counter 3B overflow\n");
			k = &encpriv[5];

			
			k->ResetCapFlags(dev, k);

			if (cmd->scan_begin_src == TRIG_TIMER) {
				DEBUG
				    ("s626_irq_handler: scan timer trigger!!!\n");

				
				MC_ENABLE(P_MC2, MC2_ADC_RPS);
			}

			if (cmd->convert_src == TRIG_TIMER) {
				DEBUG
				    ("s626_irq_handler: convert timer trigger is set\n");
				k = &encpriv[4];
				devpriv->ai_convert_count = cmd->chanlist_len;
				k->SetEnable(dev, k, CLKENAB_ALWAYS);
			}
		}
	}

	
	writel(irqstatus, devpriv->base_addr + P_IER);

	DEBUG("s626_irq_handler: exit interrupt service routine.\n");

	spin_unlock_irqrestore(&dev->spinlock, flags);
	return IRQ_HANDLED;
}

static int s626_detach(struct comedi_device *dev)
{
	if (devpriv) {
		
		devpriv->ai_cmd_running = 0;

		if (devpriv->base_addr) {
			
			WR7146(P_IER, 0);	
			WR7146(P_ISR, IRQ_GPIO3 | IRQ_RPS1);	

			
			WriteMISC2(dev, 0);

			
			WR7146(P_MC1, MC1_SHUTDOWN);
			WR7146(P_ACON1, ACON1_BASE);

			CloseDMAB(dev, &devpriv->RPSBuf, DMABUF_SIZE);
			CloseDMAB(dev, &devpriv->ANABuf, DMABUF_SIZE);
		}

		if (dev->irq)
			free_irq(dev->irq, dev);

		if (devpriv->base_addr)
			iounmap(devpriv->base_addr);

		if (devpriv->pdev) {
			if (devpriv->got_regions)
				comedi_pci_disable(devpriv->pdev);
			pci_dev_put(devpriv->pdev);
		}
	}

	DEBUG("s626_detach: S626 detached!\n");

	return 0;
}

void ResetADC(struct comedi_device *dev, uint8_t * ppl)
{
	register uint32_t *pRPS;
	uint32_t JmpAdrs;
	uint16_t i;
	uint16_t n;
	uint32_t LocalPPL;
	struct comedi_cmd *cmd = &(dev->subdevices->async->cmd);

	
	MC_DISABLE(P_MC1, MC1_ERPS1);

	
	pRPS = (uint32_t *) devpriv->RPSBuf.LogicalBase;

	
	WR7146(P_RPSADDR1, (uint32_t) devpriv->RPSBuf.PhysicalBase);

	

	if (cmd != NULL && cmd->scan_begin_src != TRIG_FOLLOW) {
		DEBUG("ResetADC: scan_begin pause inserted\n");
		
		*pRPS++ = RPS_PAUSE | RPS_SIGADC;
		*pRPS++ = RPS_CLRSIGNAL | RPS_SIGADC;
	}

	*pRPS++ = RPS_LDREG | (P_DEBICMD >> 2);
	

	*pRPS++ = DEBI_CMD_WRWORD | LP_GSEL;
	*pRPS++ = RPS_LDREG | (P_DEBIAD >> 2);
	

	*pRPS++ = GSEL_BIPOLAR5V;
	

	*pRPS++ = RPS_CLRSIGNAL | RPS_DEBI;
	
	*pRPS++ = RPS_UPLOAD | RPS_DEBI;	
	*pRPS++ = RPS_PAUSE | RPS_DEBI;	

	for (devpriv->AdcItems = 0; devpriv->AdcItems < 16; devpriv->AdcItems++) {
		LocalPPL =
		    (*ppl << 8) | (*ppl & 0x10 ? GSEL_BIPOLAR5V :
				   GSEL_BIPOLAR10V);

		
		*pRPS++ = RPS_LDREG | (P_DEBICMD >> 2);	
		
		
		*pRPS++ = DEBI_CMD_WRWORD | LP_GSEL;
		*pRPS++ = RPS_LDREG | (P_DEBIAD >> 2);	
		
		
		*pRPS++ = LocalPPL;
		*pRPS++ = RPS_CLRSIGNAL | RPS_DEBI;	
		
		*pRPS++ = RPS_UPLOAD | RPS_DEBI;	
		*pRPS++ = RPS_PAUSE | RPS_DEBI;	
		

		
		*pRPS++ = RPS_LDREG | (P_DEBICMD >> 2);
		
		*pRPS++ = DEBI_CMD_WRWORD | LP_ISEL;
		*pRPS++ = RPS_LDREG | (P_DEBIAD >> 2);
		
		*pRPS++ = LocalPPL;
		*pRPS++ = RPS_CLRSIGNAL | RPS_DEBI;
		

		*pRPS++ = RPS_UPLOAD | RPS_DEBI;
		

		*pRPS++ = RPS_PAUSE | RPS_DEBI;
		

		JmpAdrs =
		    (uint32_t) devpriv->RPSBuf.PhysicalBase +
		    (uint32_t) ((unsigned long)pRPS -
				(unsigned long)devpriv->RPSBuf.LogicalBase);
		for (i = 0; i < (10 * RPSCLK_PER_US / 2); i++) {
			JmpAdrs += 8;	
			*pRPS++ = RPS_JUMP;	
			*pRPS++ = JmpAdrs;
		}

		if (cmd != NULL && cmd->convert_src != TRIG_NOW) {
			DEBUG("ResetADC: convert pause inserted\n");
			
			*pRPS++ = RPS_PAUSE | RPS_SIGADC;
			*pRPS++ = RPS_CLRSIGNAL | RPS_SIGADC;
		}
		
		*pRPS++ = RPS_LDREG | (P_GPIO >> 2);	
		*pRPS++ = GPIO_BASE | GPIO1_LO;
		*pRPS++ = RPS_NOP;
		
		*pRPS++ = RPS_LDREG | (P_GPIO >> 2);	
		*pRPS++ = GPIO_BASE | GPIO1_HI;

		*pRPS++ = RPS_PAUSE | RPS_GPIO2;	

		
		*pRPS++ = RPS_STREG | (BUGFIX_STREG(P_FB_BUFFER1) >> 2);
		*pRPS++ =
		    (uint32_t) devpriv->ANABuf.PhysicalBase +
		    (devpriv->AdcItems << 2);

		
		
		if (*ppl++ & EOPL) {
			devpriv->AdcItems++;	
			break;	
		}
	}
	DEBUG("ResetADC: ADC items %d\n", devpriv->AdcItems);

	for (n = 0; n < (2 * RPSCLK_PER_US); n++)
		*pRPS++ = RPS_NOP;

	*pRPS++ = RPS_LDREG | (P_GPIO >> 2);	
	*pRPS++ = GPIO_BASE | GPIO1_LO;
	*pRPS++ = RPS_NOP;
	
	*pRPS++ = RPS_LDREG | (P_GPIO >> 2);	
	*pRPS++ = GPIO_BASE | GPIO1_HI;

	*pRPS++ = RPS_PAUSE | RPS_GPIO2;	

	
	*pRPS++ = RPS_STREG | (BUGFIX_STREG(P_FB_BUFFER1) >> 2);	
	*pRPS++ =
	    (uint32_t) devpriv->ANABuf.PhysicalBase + (devpriv->AdcItems << 2);

	
	

	
	if (devpriv->ai_cmd_running == 1) {
		DEBUG("ResetADC: insert irq in ADC RPS task\n");
		*pRPS++ = RPS_IRQ;
	}
	
	*pRPS++ = RPS_JUMP;	
	*pRPS++ = (uint32_t) devpriv->RPSBuf.PhysicalBase;

	
}

static int s626_ai_insn_config(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{

	return -EINVAL;
}








static int s626_ai_insn_read(struct comedi_device *dev,
			     struct comedi_subdevice *s,
			     struct comedi_insn *insn, unsigned int *data)
{
	uint16_t chan = CR_CHAN(insn->chanspec);
	uint16_t range = CR_RANGE(insn->chanspec);
	uint16_t AdcSpec = 0;
	uint32_t GpioImage;
	int n;

	

	DEBUG("s626_ai_insn_read: entering\n");

	if (range == 0)
		AdcSpec = (chan << 8) | (GSEL_BIPOLAR5V);
	else
		AdcSpec = (chan << 8) | (GSEL_BIPOLAR10V);

	
	DEBIwrite(dev, LP_GSEL, AdcSpec);	

	
	DEBIwrite(dev, LP_ISEL, AdcSpec);	

	for (n = 0; n < insn->n; n++) {

		
		udelay(10);

		
		GpioImage = RR7146(P_GPIO);
		
		WR7146(P_GPIO, GpioImage & ~GPIO1_HI);
		
		WR7146(P_GPIO, GpioImage & ~GPIO1_HI);
		WR7146(P_GPIO, GpioImage & ~GPIO1_HI);
		
		WR7146(P_GPIO, GpioImage | GPIO1_HI);

		
		
		

		
		while (!(RR7146(P_PSR) & PSR_GPIO2))
			;

		
		if (n != 0)
			data[n - 1] = s626_ai_reg_to_uint(RR7146(P_FB_BUFFER1));

		udelay(4);
	}

	GpioImage = RR7146(P_GPIO);

	
	WR7146(P_GPIO, GpioImage & ~GPIO1_HI);
	
	WR7146(P_GPIO, GpioImage & ~GPIO1_HI);
	WR7146(P_GPIO, GpioImage & ~GPIO1_HI);
	
	WR7146(P_GPIO, GpioImage | GPIO1_HI);

	

	
	while (!(RR7146(P_PSR) & PSR_GPIO2))
		;

	

	
	if (n != 0)
		data[n - 1] = s626_ai_reg_to_uint(RR7146(P_FB_BUFFER1));

	DEBUG("s626_ai_insn_read: samples %d, data %d\n", n, data[n - 1]);

	return n;
}

static int s626_ai_load_polllist(uint8_t *ppl, struct comedi_cmd *cmd)
{

	int n;

	for (n = 0; n < cmd->chanlist_len; n++) {
		if (CR_RANGE((cmd->chanlist)[n]) == 0)
			ppl[n] = (CR_CHAN((cmd->chanlist)[n])) | (RANGE_5V);
		else
			ppl[n] = (CR_CHAN((cmd->chanlist)[n])) | (RANGE_10V);
	}
	if (n != 0)
		ppl[n - 1] |= EOPL;

	return n;
}

static int s626_ai_inttrig(struct comedi_device *dev,
			   struct comedi_subdevice *s, unsigned int trignum)
{
	if (trignum != 0)
		return -EINVAL;

	DEBUG("s626_ai_inttrig: trigger adc start...");

	
	MC_ENABLE(P_MC1, MC1_ERPS1);

	s->async->inttrig = NULL;

	DEBUG(" done\n");

	return 1;
}

static int s626_ai_cmd(struct comedi_device *dev, struct comedi_subdevice *s)
{

	uint8_t ppl[16];
	struct comedi_cmd *cmd = &s->async->cmd;
	struct enc_private *k;
	int tick;

	DEBUG("s626_ai_cmd: entering command function\n");

	if (devpriv->ai_cmd_running) {
		printk(KERN_ERR "s626_ai_cmd: Another ai_cmd is running %d\n",
		       dev->minor);
		return -EBUSY;
	}
	
	writel(0, devpriv->base_addr + P_IER);

	
	writel(IRQ_RPS1 | IRQ_GPIO3, devpriv->base_addr + P_ISR);

	
	s626_dio_clear_irq(dev);
	

	
	devpriv->ai_cmd_running = 0;

	
	if (cmd == NULL) {
		DEBUG("s626_ai_cmd: NULL command\n");
		return -EINVAL;
	} else {
		DEBUG("s626_ai_cmd: command received!!!\n");
	}

	if (dev->irq == 0) {
		comedi_error(dev,
			     "s626_ai_cmd: cannot run command without an irq");
		return -EIO;
	}

	s626_ai_load_polllist(ppl, cmd);
	devpriv->ai_cmd_running = 1;
	devpriv->ai_convert_count = 0;

	switch (cmd->scan_begin_src) {
	case TRIG_FOLLOW:
		break;
	case TRIG_TIMER:
		
		k = &encpriv[5];
		tick = s626_ns_to_timer((int *)&cmd->scan_begin_arg,
					cmd->flags & TRIG_ROUND_MASK);

		
		s626_timer_load(dev, k, tick);
		k->SetEnable(dev, k, CLKENAB_ALWAYS);

		DEBUG("s626_ai_cmd: scan trigger timer is set with value %d\n",
		      tick);

		break;
	case TRIG_EXT:
		
		if (cmd->start_src != TRIG_EXT)
			s626_dio_set_irq(dev, cmd->scan_begin_arg);

		DEBUG("s626_ai_cmd: External scan trigger is set!!!\n");

		break;
	}

	switch (cmd->convert_src) {
	case TRIG_NOW:
		break;
	case TRIG_TIMER:
		
		k = &encpriv[4];
		tick = s626_ns_to_timer((int *)&cmd->convert_arg,
					cmd->flags & TRIG_ROUND_MASK);

		
		s626_timer_load(dev, k, tick);
		k->SetEnable(dev, k, CLKENAB_INDEX);

		DEBUG
		    ("s626_ai_cmd: convert trigger timer is set with value %d\n",
		     tick);
		break;
	case TRIG_EXT:
		
		if (cmd->scan_begin_src != TRIG_EXT
		    && cmd->start_src == TRIG_EXT)
			s626_dio_set_irq(dev, cmd->convert_arg);

		DEBUG("s626_ai_cmd: External convert trigger is set!!!\n");

		break;
	}

	switch (cmd->stop_src) {
	case TRIG_COUNT:
		
		devpriv->ai_sample_count = cmd->stop_arg;
		devpriv->ai_continous = 0;
		break;
	case TRIG_NONE:
		
		devpriv->ai_continous = 1;
		devpriv->ai_sample_count = 0;
		break;
	}

	ResetADC(dev, ppl);

	switch (cmd->start_src) {
	case TRIG_NOW:
		
		

		
		MC_ENABLE(P_MC1, MC1_ERPS1);

		DEBUG("s626_ai_cmd: ADC triggered\n");
		s->async->inttrig = NULL;
		break;
	case TRIG_EXT:
		
		s626_dio_set_irq(dev, cmd->start_arg);

		DEBUG("s626_ai_cmd: External start trigger is set!!!\n");

		s->async->inttrig = NULL;
		break;
	case TRIG_INT:
		s->async->inttrig = s626_ai_inttrig;
		break;
	}

	
	writel(IRQ_GPIO3 | IRQ_RPS1, devpriv->base_addr + P_IER);

	DEBUG("s626_ai_cmd: command function terminated\n");

	return 0;
}

static int s626_ai_cmdtest(struct comedi_device *dev,
			   struct comedi_subdevice *s, struct comedi_cmd *cmd)
{
	int err = 0;
	int tmp;


	

	tmp = cmd->start_src;
	cmd->start_src &= TRIG_NOW | TRIG_INT | TRIG_EXT;
	if (!cmd->start_src || tmp != cmd->start_src)
		err++;

	tmp = cmd->scan_begin_src;
	cmd->scan_begin_src &= TRIG_TIMER | TRIG_EXT | TRIG_FOLLOW;
	if (!cmd->scan_begin_src || tmp != cmd->scan_begin_src)
		err++;

	tmp = cmd->convert_src;
	cmd->convert_src &= TRIG_TIMER | TRIG_EXT | TRIG_NOW;
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
	    cmd->scan_begin_src != TRIG_EXT
	    && cmd->scan_begin_src != TRIG_FOLLOW)
		err++;
	if (cmd->convert_src != TRIG_TIMER &&
	    cmd->convert_src != TRIG_EXT && cmd->convert_src != TRIG_NOW)
		err++;
	if (cmd->stop_src != TRIG_COUNT && cmd->stop_src != TRIG_NONE)
		err++;

	if (err)
		return 2;

	

	if (cmd->start_src != TRIG_EXT && cmd->start_arg != 0) {
		cmd->start_arg = 0;
		err++;
	}

	if (cmd->start_src == TRIG_EXT && cmd->start_arg > 39) {
		cmd->start_arg = 39;
		err++;
	}

	if (cmd->scan_begin_src == TRIG_EXT && cmd->scan_begin_arg > 39) {
		cmd->scan_begin_arg = 39;
		err++;
	}

	if (cmd->convert_src == TRIG_EXT && cmd->convert_arg > 39) {
		cmd->convert_arg = 39;
		err++;
	}
#define MAX_SPEED	200000	
#define MIN_SPEED	2000000000	

	if (cmd->scan_begin_src == TRIG_TIMER) {
		if (cmd->scan_begin_arg < MAX_SPEED) {
			cmd->scan_begin_arg = MAX_SPEED;
			err++;
		}
		if (cmd->scan_begin_arg > MIN_SPEED) {
			cmd->scan_begin_arg = MIN_SPEED;
			err++;
		}
	} else {
		
		
		
	}
	if (cmd->convert_src == TRIG_TIMER) {
		if (cmd->convert_arg < MAX_SPEED) {
			cmd->convert_arg = MAX_SPEED;
			err++;
		}
		if (cmd->convert_arg > MIN_SPEED) {
			cmd->convert_arg = MIN_SPEED;
			err++;
		}
	} else {
		
		
	}

	if (cmd->scan_end_arg != cmd->chanlist_len) {
		cmd->scan_end_arg = cmd->chanlist_len;
		err++;
	}
	if (cmd->stop_src == TRIG_COUNT) {
		if (cmd->stop_arg > 0x00ffffff) {
			cmd->stop_arg = 0x00ffffff;
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

	

	if (cmd->scan_begin_src == TRIG_TIMER) {
		tmp = cmd->scan_begin_arg;
		s626_ns_to_timer((int *)&cmd->scan_begin_arg,
				 cmd->flags & TRIG_ROUND_MASK);
		if (tmp != cmd->scan_begin_arg)
			err++;
	}
	if (cmd->convert_src == TRIG_TIMER) {
		tmp = cmd->convert_arg;
		s626_ns_to_timer((int *)&cmd->convert_arg,
				 cmd->flags & TRIG_ROUND_MASK);
		if (tmp != cmd->convert_arg)
			err++;
		if (cmd->scan_begin_src == TRIG_TIMER &&
		    cmd->scan_begin_arg <
		    cmd->convert_arg * cmd->scan_end_arg) {
			cmd->scan_begin_arg =
			    cmd->convert_arg * cmd->scan_end_arg;
			err++;
		}
	}

	if (err)
		return 4;

	return 0;
}

static int s626_ai_cancel(struct comedi_device *dev, struct comedi_subdevice *s)
{
	
	MC_DISABLE(P_MC1, MC1_ERPS1);

	
	writel(0, devpriv->base_addr + P_IER);

	devpriv->ai_cmd_running = 0;

	return 0;
}

static int s626_ns_to_timer(int *nanosec, int round_mode)
{
	int divider, base;

	base = 500;		

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

	*nanosec = base * divider;
	return divider - 1;
}

static int s626_ao_winsn(struct comedi_device *dev, struct comedi_subdevice *s,
			 struct comedi_insn *insn, unsigned int *data)
{

	int i;
	uint16_t chan = CR_CHAN(insn->chanspec);
	int16_t dacdata;

	for (i = 0; i < insn->n; i++) {
		dacdata = (int16_t) data[i];
		devpriv->ao_readback[CR_CHAN(insn->chanspec)] = data[i];
		dacdata -= (0x1fff);

		SetDAC(dev, chan, dacdata);
	}

	return i;
}

static int s626_ao_rinsn(struct comedi_device *dev, struct comedi_subdevice *s,
			 struct comedi_insn *insn, unsigned int *data)
{
	int i;

	for (i = 0; i < insn->n; i++)
		data[i] = devpriv->ao_readback[CR_CHAN(insn->chanspec)];

	return i;
}


static void s626_dio_init(struct comedi_device *dev)
{
	uint16_t group;
	struct comedi_subdevice *s;

	
	DEBIwrite(dev, LP_MISC1, MISC1_NOEDCAP);

	
	for (group = 0; group < S626_DIO_BANKS; group++) {
		s = dev->subdevices + 2 + group;
		DEBIwrite(dev, diopriv->WRIntSel, 0);	
		DEBIwrite(dev, diopriv->WRCapSel, 0xFFFF);	
		
		DEBIwrite(dev, diopriv->WREdgSel, 0);	
		
		
		DEBIwrite(dev, diopriv->WRDOut, 0);	
		
	}
	DEBUG("s626_dio_init: DIO initialized\n");
}


static int s626_dio_insn_bits(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data)
{

	
	if (insn->n == 0)
		return 0;

	if (insn->n != 2) {
		printk
		    ("comedi%d: s626: s626_dio_insn_bits(): Invalid instruction length\n",
		     dev->minor);
		return -EINVAL;
	}

	if (data[0]) {
		
		if ((s->io_bits & data[0]) != data[0])
			return -EIO;

		s->state &= ~data[0];
		s->state |= data[0] & data[1];

		

		DEBIwrite(dev, diopriv->WRDOut, s->state);
	}
	data[1] = DEBIread(dev, diopriv->RDDIn);

	return 2;
}

static int s626_dio_insn_config(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data)
{

	switch (data[0]) {
	case INSN_CONFIG_DIO_QUERY:
		data[1] =
		    (s->
		     io_bits & (1 << CR_CHAN(insn->chanspec))) ? COMEDI_OUTPUT :
		    COMEDI_INPUT;
		return insn->n;
		break;
	case COMEDI_INPUT:
		s->io_bits &= ~(1 << CR_CHAN(insn->chanspec));
		break;
	case COMEDI_OUTPUT:
		s->io_bits |= 1 << CR_CHAN(insn->chanspec);
		break;
	default:
		return -EINVAL;
		break;
	}
	DEBIwrite(dev, diopriv->WRDOut, s->io_bits);

	return 1;
}

static int s626_dio_set_irq(struct comedi_device *dev, unsigned int chan)
{
	unsigned int group;
	unsigned int bitmask;
	unsigned int status;

	
	group = chan / 16;
	bitmask = 1 << (chan - (16 * group));
	DEBUG("s626_dio_set_irq: enable interrupt on dio channel %d group %d\n",
	      chan - (16 * group), group);

	
	status = DEBIread(dev,
			  ((struct dio_private *)(dev->subdevices + 2 +
						  group)->private)->RDEdgSel);
	DEBIwrite(dev,
		  ((struct dio_private *)(dev->subdevices + 2 +
					  group)->private)->WREdgSel,
		  bitmask | status);

	
	status = DEBIread(dev,
			  ((struct dio_private *)(dev->subdevices + 2 +
						  group)->private)->RDIntSel);
	DEBIwrite(dev,
		  ((struct dio_private *)(dev->subdevices + 2 +
					  group)->private)->WRIntSel,
		  bitmask | status);

	
	DEBIwrite(dev, LP_MISC1, MISC1_EDCAP);

	
	status = DEBIread(dev,
			  ((struct dio_private *)(dev->subdevices + 2 +
						  group)->private)->RDCapSel);
	DEBIwrite(dev,
		  ((struct dio_private *)(dev->subdevices + 2 +
					  group)->private)->WRCapSel,
		  bitmask | status);

	return 0;
}

static int s626_dio_reset_irq(struct comedi_device *dev, unsigned int group,
			      unsigned int mask)
{
	DEBUG
	    ("s626_dio_reset_irq: disable  interrupt on dio channel %d group %d\n",
	     mask, group);

	
	DEBIwrite(dev, LP_MISC1, MISC1_NOEDCAP);

	
	DEBIwrite(dev,
		  ((struct dio_private *)(dev->subdevices + 2 +
					  group)->private)->WRCapSel, mask);

	return 0;
}

static int s626_dio_clear_irq(struct comedi_device *dev)
{
	unsigned int group;

	
	DEBIwrite(dev, LP_MISC1, MISC1_NOEDCAP);

	for (group = 0; group < S626_DIO_BANKS; group++) {
		
		DEBIwrite(dev,
			  ((struct dio_private *)(dev->subdevices + 2 +
						  group)->private)->WRCapSel,
			  0xffff);
	}

	return 0;
}

static int s626_enc_insn_config(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data)
{
	uint16_t Setup = (LOADSRC_INDX << BF_LOADSRC) |	
	    
	    (INDXSRC_SOFT << BF_INDXSRC) |	
	    (CLKSRC_COUNTER << BF_CLKSRC) |	
	    (CLKPOL_POS << BF_CLKPOL) |	
	    
	    (CLKMULT_1X << BF_CLKMULT) |	
	    (CLKENAB_INDEX << BF_CLKENAB);
	
	
	uint16_t valueSrclatch = LATCHSRC_AB_READ;
	uint16_t enab = CLKENAB_ALWAYS;
	struct enc_private *k = &encpriv[CR_CHAN(insn->chanspec)];

	DEBUG("s626_enc_insn_config: encoder config\n");

	

	k->SetMode(dev, k, Setup, TRUE);
	Preload(dev, k, *(insn->data));
	k->PulseIndex(dev, k);
	SetLatchSource(dev, k, valueSrclatch);
	k->SetEnable(dev, k, (uint16_t) (enab != 0));

	return insn->n;
}

static int s626_enc_insn_read(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data)
{

	int n;
	struct enc_private *k = &encpriv[CR_CHAN(insn->chanspec)];

	DEBUG("s626_enc_insn_read: encoder read channel %d\n",
	      CR_CHAN(insn->chanspec));

	for (n = 0; n < insn->n; n++)
		data[n] = ReadLatch(dev, k);

	DEBUG("s626_enc_insn_read: encoder sample %d\n", data[n]);

	return n;
}

static int s626_enc_insn_write(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{

	struct enc_private *k = &encpriv[CR_CHAN(insn->chanspec)];

	DEBUG("s626_enc_insn_write: encoder write channel %d\n",
	      CR_CHAN(insn->chanspec));

	
	Preload(dev, k, data[0]);

	
	
	k->SetLoadTrig(dev, k, 0);
	k->PulseIndex(dev, k);
	k->SetLoadTrig(dev, k, 2);

	DEBUG("s626_enc_insn_write: End encoder write\n");

	return 1;
}

static void s626_timer_load(struct comedi_device *dev, struct enc_private *k,
			    int tick)
{
	uint16_t Setup = (LOADSRC_INDX << BF_LOADSRC) |	
	    
	    (INDXSRC_SOFT << BF_INDXSRC) |	
	    (CLKSRC_TIMER << BF_CLKSRC) |	
	    (CLKPOL_POS << BF_CLKPOL) |	
	    (CNTDIR_DOWN << BF_CLKPOL) |	
	    (CLKMULT_1X << BF_CLKMULT) |	
	    (CLKENAB_INDEX << BF_CLKENAB);
	uint16_t valueSrclatch = LATCHSRC_A_INDXA;
	

	k->SetMode(dev, k, Setup, FALSE);

	
	Preload(dev, k, tick);

	
	
	k->SetLoadTrig(dev, k, 0);
	k->PulseIndex(dev, k);

	
	k->SetLoadTrig(dev, k, 1);

	
	k->SetIntSrc(dev, k, INTSRC_OVER);

	SetLatchSource(dev, k, valueSrclatch);
	
}


#define VECT0	(XSD2 | RSD3 | SIB_A2)

static uint8_t trimchan[] = { 10, 9, 8, 3, 2, 7, 6, 1, 0, 5, 4 };

static uint8_t trimadrs[] = { 0x40, 0x41, 0x42, 0x50, 0x51, 0x52, 0x53, 0x60, 0x61, 0x62, 0x63 };

static void LoadTrimDACs(struct comedi_device *dev)
{
	register uint8_t i;

	
	for (i = 0; i < ARRAY_SIZE(trimchan); i++)
		WriteTrimDAC(dev, i, I2Cread(dev, trimadrs[i]));
}

static void WriteTrimDAC(struct comedi_device *dev, uint8_t LogicalChan,
			 uint8_t DacData)
{
	uint32_t chan;

	
	devpriv->TrimSetpoint[LogicalChan] = (uint8_t) DacData;

	
	chan = (uint32_t) trimchan[LogicalChan];


	SETVECT(2, XSD2 | XFIFO_1 | WS3);
	
	SETVECT(3, XSD2 | XFIFO_0 | WS3);
	
	SETVECT(4, XSD2 | XFIFO_3 | WS1);
	
	SETVECT(5, XSD2 | XFIFO_2 | WS1 | EOS);
	


	
	SendDAC(dev, ((uint32_t) chan << 8)
		| (uint32_t) DacData);	
}


static uint8_t I2Cread(struct comedi_device *dev, uint8_t addr)
{
	uint8_t rtnval;

	
	if (I2Chandshake(dev, I2C_B2(I2C_ATTRSTART, I2CW)
			 
			 | I2C_B1(I2C_ATTRSTOP, addr)
			 
			 | I2C_B0(I2C_ATTRNOP, 0))) {	
		
		DEBUG("I2Cread: error handshake I2Cread  a\n");
		return 0;
	}
	
	if (I2Chandshake(dev, I2C_B2(I2C_ATTRSTART, I2CR)

			 
			 
			 
			 
			 |I2C_B1(I2C_ATTRSTOP, 0)

			 
			 
			 
			 |I2C_B0(I2C_ATTRNOP, 0))) {	

		
		DEBUG("I2Cread: error handshake I2Cread b\n");
		return 0;
	}
	
	rtnval = (uint8_t) (RR7146(P_I2CCTRL) >> 16);
	return rtnval;
}

static uint32_t I2Chandshake(struct comedi_device *dev, uint32_t val)
{
	
	WR7146(P_I2CCTRL, val);

	
	

	MC_ENABLE(P_MC2, MC2_UPLD_IIC);
	while (!MC_TEST(P_MC2, MC2_UPLD_IIC))
		;

	
	while ((RR7146(P_I2CCTRL) & (I2C_BUSY | I2C_ERR)) == I2C_BUSY)
		;

	
	return RR7146(P_I2CCTRL) & I2C_ERR;

}


static void SetDAC(struct comedi_device *dev, uint16_t chan, short dacdata)
{
	register uint16_t signmask;
	register uint32_t WSImage;

	
	
	signmask = 1 << chan;
	if (dacdata < 0) {
		dacdata = -dacdata;
		devpriv->Dacpol |= signmask;
	} else
		devpriv->Dacpol &= ~signmask;

	
	if ((uint16_t) dacdata > 0x1FFF)
		dacdata = 0x1FFF;


	WSImage = (chan & 2) ? WS1 : WS2;
	
	SETVECT(2, XSD2 | XFIFO_1 | WSImage);
	
	SETVECT(3, XSD2 | XFIFO_0 | WSImage);
	
	SETVECT(4, XSD2 | XFIFO_3 | WS3);
	
	SETVECT(5, XSD2 | XFIFO_2 | WS3 | EOS);
	

	SendDAC(dev, 0x0F000000
		
		| 0x00004000
		| ((uint32_t) (chan & 1) << 15)
		
		| (uint32_t) dacdata);	

}


static void SendDAC(struct comedi_device *dev, uint32_t val)
{

	

	DEBIwrite(dev, LP_DACPOL, devpriv->Dacpol);

	

	

	
	*devpriv->pDacWBuf = val;

	MC_ENABLE(P_MC1, MC1_A2OUT);

	

	WR7146(P_ISR, ISR_AFOU);

	while ((RR7146(P_MC1) & MC1_A2OUT) != 0)
		;

	

	SETVECT(0, XSD2 | RSD3 | SIB_A2);

	while ((RR7146(P_SSR) & SSR_AF2_OUT) == 0)
		;

	SETVECT(0, XSD2 | XFIFO_2 | RSD2 | SIB_A2 | EOS);

	

	if ((RR7146(P_FB_BUFFER2) & 0xFF000000) != 0) {
		while ((RR7146(P_FB_BUFFER2) & 0xFF000000) != 0)
			;
	}
	SETVECT(0, RSD3 | SIB_A2 | EOS);

	while ((RR7146(P_FB_BUFFER2) & 0xFF000000) == 0)
		;
}

static void WriteMISC2(struct comedi_device *dev, uint16_t NewImage)
{
	DEBIwrite(dev, LP_MISC1, MISC1_WENABLE);	
	
	DEBIwrite(dev, LP_WRMISC2, NewImage);	
	DEBIwrite(dev, LP_MISC1, MISC1_WDISABLE);	
}


static uint16_t DEBIread(struct comedi_device *dev, uint16_t addr)
{
	uint16_t retval;

	
	WR7146(P_DEBICMD, DEBI_CMD_RDWORD | addr);

	
	DEBItransfer(dev);

	
	retval = (uint16_t) RR7146(P_DEBIAD);

	
	return retval;
}

static void DEBItransfer(struct comedi_device *dev)
{
	
	MC_ENABLE(P_MC2, MC2_UPLD_DEBI);

	
	
	while (!MC_TEST(P_MC2, MC2_UPLD_DEBI))
		;

	
	while (RR7146(P_PSR) & PSR_DEBI_S)
		;
}

static void DEBIwrite(struct comedi_device *dev, uint16_t addr, uint16_t wdata)
{

	
	WR7146(P_DEBICMD, DEBI_CMD_WRWORD | addr);
	WR7146(P_DEBIAD, wdata);

	
	DEBItransfer(dev);
}

static void DEBIreplace(struct comedi_device *dev, uint16_t addr, uint16_t mask,
			uint16_t wdata)
{

	
	WR7146(P_DEBICMD, DEBI_CMD_RDWORD | addr);
	
	DEBItransfer(dev);	

	
	WR7146(P_DEBICMD, DEBI_CMD_WRWORD | addr);
	

	WR7146(P_DEBIAD, wdata | ((uint16_t) RR7146(P_DEBIAD) & mask));
	
	DEBItransfer(dev);	
}

static void CloseDMAB(struct comedi_device *dev, struct bufferDMA *pdma,
		      size_t bsize)
{
	void *vbptr;
	dma_addr_t vpptr;

	DEBUG("CloseDMAB: Entering S626DRV_CloseDMAB():\n");
	if (pdma == NULL)
		return;
	

	vbptr = pdma->LogicalBase;
	vpptr = pdma->PhysicalBase;
	if (vbptr) {
		pci_free_consistent(devpriv->pdev, bsize, vbptr, vpptr);
		pdma->LogicalBase = 0;
		pdma->PhysicalBase = 0;

		DEBUG("CloseDMAB(): Logical=%p, bsize=%d, Physical=0x%x\n",
		      vbptr, bsize, (uint32_t) vpptr);
	}
}





static uint32_t ReadLatch(struct comedi_device *dev, struct enc_private *k)
{
	register uint32_t value;
	

	
	value = (uint32_t) DEBIread(dev, k->MyLatchLsw);

	
	value |= ((uint32_t) DEBIread(dev, k->MyLatchLsw + 2) << 16);

	

	
	return value;
}


static void ResetCapFlags_A(struct comedi_device *dev, struct enc_private *k)
{
	DEBIreplace(dev, k->MyCRB, (uint16_t) (~CRBMSK_INTCTRL),
		    CRBMSK_INTRESETCMD | CRBMSK_INTRESET_A);
}

static void ResetCapFlags_B(struct comedi_device *dev, struct enc_private *k)
{
	DEBIreplace(dev, k->MyCRB, (uint16_t) (~CRBMSK_INTCTRL),
		    CRBMSK_INTRESETCMD | CRBMSK_INTRESET_B);
}


static uint16_t GetMode_A(struct comedi_device *dev, struct enc_private *k)
{
	register uint16_t cra;
	register uint16_t crb;
	register uint16_t setup;

	
	cra = DEBIread(dev, k->MyCRA);
	crb = DEBIread(dev, k->MyCRB);

	
	
	setup = ((cra & STDMSK_LOADSRC)	
		 |((crb << (STDBIT_LATCHSRC - CRBBIT_LATCHSRC)) & STDMSK_LATCHSRC)	
		 |((cra << (STDBIT_INTSRC - CRABIT_INTSRC_A)) & STDMSK_INTSRC)	
		 |((cra << (STDBIT_INDXSRC - (CRABIT_INDXSRC_A + 1))) & STDMSK_INDXSRC)	
		 |((cra >> (CRABIT_INDXPOL_A - STDBIT_INDXPOL)) & STDMSK_INDXPOL)	
		 |((crb >> (CRBBIT_CLKENAB_A - STDBIT_CLKENAB)) & STDMSK_CLKENAB));	

	
	if (cra & (2 << CRABIT_CLKSRC_A))	
		setup |= ((CLKSRC_TIMER << STDBIT_CLKSRC)	
			  |((cra << (STDBIT_CLKPOL - CRABIT_CLKSRC_A)) & STDMSK_CLKPOL)	
			  |(MULT_X1 << STDBIT_CLKMULT));	

	else			
		setup |= ((CLKSRC_COUNTER << STDBIT_CLKSRC)	
			  |((cra >> (CRABIT_CLKPOL_A - STDBIT_CLKPOL)) & STDMSK_CLKPOL)	
			  |(((cra & CRAMSK_CLKMULT_A) == (MULT_X0 << CRABIT_CLKMULT_A)) ?	
			    (MULT_X1 << STDBIT_CLKMULT) :
			    ((cra >> (CRABIT_CLKMULT_A -
				      STDBIT_CLKMULT)) & STDMSK_CLKMULT)));

	
	return setup;
}

static uint16_t GetMode_B(struct comedi_device *dev, struct enc_private *k)
{
	register uint16_t cra;
	register uint16_t crb;
	register uint16_t setup;

	
	cra = DEBIread(dev, k->MyCRA);
	crb = DEBIread(dev, k->MyCRB);

	
	
	setup = (((crb << (STDBIT_INTSRC - CRBBIT_INTSRC_B)) & STDMSK_INTSRC)	
		 |((crb << (STDBIT_LATCHSRC - CRBBIT_LATCHSRC)) & STDMSK_LATCHSRC)	
		 |((crb << (STDBIT_LOADSRC - CRBBIT_LOADSRC_B)) & STDMSK_LOADSRC)	
		 |((crb << (STDBIT_INDXPOL - CRBBIT_INDXPOL_B)) & STDMSK_INDXPOL)	
		 |((crb >> (CRBBIT_CLKENAB_B - STDBIT_CLKENAB)) & STDMSK_CLKENAB)	
		 |((cra >> ((CRABIT_INDXSRC_B + 1) - STDBIT_INDXSRC)) & STDMSK_INDXSRC));	

	
	if ((crb & CRBMSK_CLKMULT_B) == (MULT_X0 << CRBBIT_CLKMULT_B))	
		setup |= ((CLKSRC_EXTENDER << STDBIT_CLKSRC)	
			  |(MULT_X1 << STDBIT_CLKMULT)	
			  |((cra >> (CRABIT_CLKSRC_B - STDBIT_CLKPOL)) & STDMSK_CLKPOL));	

	else if (cra & (2 << CRABIT_CLKSRC_B))	
		setup |= ((CLKSRC_TIMER << STDBIT_CLKSRC)	
			  |(MULT_X1 << STDBIT_CLKMULT)	
			  |((cra >> (CRABIT_CLKSRC_B - STDBIT_CLKPOL)) & STDMSK_CLKPOL));	

	else			
		setup |= ((CLKSRC_COUNTER << STDBIT_CLKSRC)	
			  |((crb >> (CRBBIT_CLKMULT_B - STDBIT_CLKMULT)) & STDMSK_CLKMULT)	
			  |((crb << (STDBIT_CLKPOL - CRBBIT_CLKPOL_B)) & STDMSK_CLKPOL));	

	
	return setup;
}


static void SetMode_A(struct comedi_device *dev, struct enc_private *k,
		      uint16_t Setup, uint16_t DisableIntSrc)
{
	register uint16_t cra;
	register uint16_t crb;
	register uint16_t setup = Setup;	

	
	cra = ((setup & CRAMSK_LOADSRC_A)	
	       |((setup & STDMSK_INDXSRC) >> (STDBIT_INDXSRC - (CRABIT_INDXSRC_A + 1))));	

	crb = (CRBMSK_INTRESETCMD | CRBMSK_INTRESET_A	
	       | ((setup & STDMSK_CLKENAB) << (CRBBIT_CLKENAB_A - STDBIT_CLKENAB)));	

	
	if (!DisableIntSrc)
		cra |= ((setup & STDMSK_INTSRC) >> (STDBIT_INTSRC -
						    CRABIT_INTSRC_A));

	
	switch ((setup & STDMSK_CLKSRC) >> STDBIT_CLKSRC) {
	case CLKSRC_EXTENDER:	
		

	case CLKSRC_TIMER:	
		cra |= ((2 << CRABIT_CLKSRC_A)	
			|((setup & STDMSK_CLKPOL) >> (STDBIT_CLKPOL - CRABIT_CLKSRC_A))	
			|(1 << CRABIT_CLKPOL_A)	
			|(MULT_X1 << CRABIT_CLKMULT_A));	
		break;

	default:		
		cra |= (CLKSRC_COUNTER	
			| ((setup & STDMSK_CLKPOL) << (CRABIT_CLKPOL_A - STDBIT_CLKPOL))	
			|(((setup & STDMSK_CLKMULT) == (MULT_X0 << STDBIT_CLKMULT)) ?	
			  (MULT_X1 << CRABIT_CLKMULT_A) :
			  ((setup & STDMSK_CLKMULT) << (CRABIT_CLKMULT_A -
							STDBIT_CLKMULT))));
	}

	
	
	if (~setup & STDMSK_INDXSRC)
		cra |= ((setup & STDMSK_INDXPOL) << (CRABIT_INDXPOL_A -
						     STDBIT_INDXPOL));

	
	
	if (DisableIntSrc)
		devpriv->CounterIntEnabs &= ~k->MyEventBits[3];

	
	
	DEBIreplace(dev, k->MyCRA, CRAMSK_INDXSRC_B | CRAMSK_CLKSRC_B, cra);
	DEBIreplace(dev, k->MyCRB,
		    (uint16_t) (~(CRBMSK_INTCTRL | CRBMSK_CLKENAB_A)), crb);
}

static void SetMode_B(struct comedi_device *dev, struct enc_private *k,
		      uint16_t Setup, uint16_t DisableIntSrc)
{
	register uint16_t cra;
	register uint16_t crb;
	register uint16_t setup = Setup;	

	
	cra = ((setup & STDMSK_INDXSRC) << ((CRABIT_INDXSRC_B + 1) - STDBIT_INDXSRC));	

	crb = (CRBMSK_INTRESETCMD | CRBMSK_INTRESET_B	
	       | ((setup & STDMSK_CLKENAB) << (CRBBIT_CLKENAB_B - STDBIT_CLKENAB))	
	       |((setup & STDMSK_LOADSRC) >> (STDBIT_LOADSRC - CRBBIT_LOADSRC_B)));	

	
	if (!DisableIntSrc)
		crb |= ((setup & STDMSK_INTSRC) >> (STDBIT_INTSRC -
						    CRBBIT_INTSRC_B));

	
	switch ((setup & STDMSK_CLKSRC) >> STDBIT_CLKSRC) {
	case CLKSRC_TIMER:	
		cra |= ((2 << CRABIT_CLKSRC_B)	
			|((setup & STDMSK_CLKPOL) << (CRABIT_CLKSRC_B - STDBIT_CLKPOL)));	
		crb |= ((1 << CRBBIT_CLKPOL_B)	
			|(MULT_X1 << CRBBIT_CLKMULT_B));	
		break;

	case CLKSRC_EXTENDER:	
		cra |= ((2 << CRABIT_CLKSRC_B)	
			|((setup & STDMSK_CLKPOL) << (CRABIT_CLKSRC_B - STDBIT_CLKPOL)));	
		crb |= ((1 << CRBBIT_CLKPOL_B)	
			|(MULT_X0 << CRBBIT_CLKMULT_B));	
		break;

	default:		
		cra |= (CLKSRC_COUNTER << CRABIT_CLKSRC_B);	
		crb |= (((setup & STDMSK_CLKPOL) >> (STDBIT_CLKPOL - CRBBIT_CLKPOL_B))	
			|(((setup & STDMSK_CLKMULT) == (MULT_X0 << STDBIT_CLKMULT)) ?	
			  (MULT_X1 << CRBBIT_CLKMULT_B) :
			  ((setup & STDMSK_CLKMULT) << (CRBBIT_CLKMULT_B -
							STDBIT_CLKMULT))));
	}

	
	
	if (~setup & STDMSK_INDXSRC)
		crb |= ((setup & STDMSK_INDXPOL) >> (STDBIT_INDXPOL -
						     CRBBIT_INDXPOL_B));

	
	
	if (DisableIntSrc)
		devpriv->CounterIntEnabs &= ~k->MyEventBits[3];

	
	
	DEBIreplace(dev, k->MyCRA,
		    (uint16_t) (~(CRAMSK_INDXSRC_B | CRAMSK_CLKSRC_B)), cra);
	DEBIreplace(dev, k->MyCRB, CRBMSK_CLKENAB_A | CRBMSK_LATCHSRC, crb);
}


static void SetEnable_A(struct comedi_device *dev, struct enc_private *k,
			uint16_t enab)
{
	DEBUG("SetEnable_A: SetEnable_A enter 3541\n");
	DEBIreplace(dev, k->MyCRB,
		    (uint16_t) (~(CRBMSK_INTCTRL | CRBMSK_CLKENAB_A)),
		    (uint16_t) (enab << CRBBIT_CLKENAB_A));
}

static void SetEnable_B(struct comedi_device *dev, struct enc_private *k,
			uint16_t enab)
{
	DEBIreplace(dev, k->MyCRB,
		    (uint16_t) (~(CRBMSK_INTCTRL | CRBMSK_CLKENAB_B)),
		    (uint16_t) (enab << CRBBIT_CLKENAB_B));
}

static uint16_t GetEnable_A(struct comedi_device *dev, struct enc_private *k)
{
	return (DEBIread(dev, k->MyCRB) >> CRBBIT_CLKENAB_A) & 1;
}

static uint16_t GetEnable_B(struct comedi_device *dev, struct enc_private *k)
{
	return (DEBIread(dev, k->MyCRB) >> CRBBIT_CLKENAB_B) & 1;
}


static void SetLatchSource(struct comedi_device *dev, struct enc_private *k,
			   uint16_t value)
{
	DEBUG("SetLatchSource: SetLatchSource enter 3550\n");
	DEBIreplace(dev, k->MyCRB,
		    (uint16_t) (~(CRBMSK_INTCTRL | CRBMSK_LATCHSRC)),
		    (uint16_t) (value << CRBBIT_LATCHSRC));

	DEBUG("SetLatchSource: SetLatchSource exit\n");
}



static void SetLoadTrig_A(struct comedi_device *dev, struct enc_private *k,
			  uint16_t Trig)
{
	DEBIreplace(dev, k->MyCRA, (uint16_t) (~CRAMSK_LOADSRC_A),
		    (uint16_t) (Trig << CRABIT_LOADSRC_A));
}

static void SetLoadTrig_B(struct comedi_device *dev, struct enc_private *k,
			  uint16_t Trig)
{
	DEBIreplace(dev, k->MyCRB,
		    (uint16_t) (~(CRBMSK_LOADSRC_B | CRBMSK_INTCTRL)),
		    (uint16_t) (Trig << CRBBIT_LOADSRC_B));
}

static uint16_t GetLoadTrig_A(struct comedi_device *dev, struct enc_private *k)
{
	return (DEBIread(dev, k->MyCRA) >> CRABIT_LOADSRC_A) & 3;
}

static uint16_t GetLoadTrig_B(struct comedi_device *dev, struct enc_private *k)
{
	return (DEBIread(dev, k->MyCRB) >> CRBBIT_LOADSRC_B) & 3;
}


static void SetIntSrc_A(struct comedi_device *dev, struct enc_private *k,
			uint16_t IntSource)
{
	
	DEBIreplace(dev, k->MyCRB, (uint16_t) (~CRBMSK_INTCTRL),
		    CRBMSK_INTRESETCMD | CRBMSK_INTRESET_A);

	
	DEBIreplace(dev, k->MyCRA, ~CRAMSK_INTSRC_A,
		    (uint16_t) (IntSource << CRABIT_INTSRC_A));

	
	devpriv->CounterIntEnabs =
	    (devpriv->CounterIntEnabs & ~k->
	     MyEventBits[3]) | k->MyEventBits[IntSource];
}

static void SetIntSrc_B(struct comedi_device *dev, struct enc_private *k,
			uint16_t IntSource)
{
	uint16_t crb;

	
	crb = DEBIread(dev, k->MyCRB) & ~CRBMSK_INTCTRL;

	
	DEBIwrite(dev, k->MyCRB,
		  (uint16_t) (crb | CRBMSK_INTRESETCMD | CRBMSK_INTRESET_B));

	
	DEBIwrite(dev, k->MyCRB,
		  (uint16_t) ((crb & ~CRBMSK_INTSRC_B) | (IntSource <<
							  CRBBIT_INTSRC_B)));

	
	devpriv->CounterIntEnabs =
	    (devpriv->CounterIntEnabs & ~k->
	     MyEventBits[3]) | k->MyEventBits[IntSource];
}

static uint16_t GetIntSrc_A(struct comedi_device *dev, struct enc_private *k)
{
	return (DEBIread(dev, k->MyCRA) >> CRABIT_INTSRC_A) & 3;
}

static uint16_t GetIntSrc_B(struct comedi_device *dev, struct enc_private *k)
{
	return (DEBIread(dev, k->MyCRB) >> CRBBIT_INTSRC_B) & 3;
}

















static void PulseIndex_A(struct comedi_device *dev, struct enc_private *k)
{
	register uint16_t cra;

	DEBUG("PulseIndex_A: pulse index enter\n");

	cra = DEBIread(dev, k->MyCRA);	
	DEBIwrite(dev, k->MyCRA, (uint16_t) (cra ^ CRAMSK_INDXPOL_A));
	DEBUG("PulseIndex_A: pulse index step1\n");
	DEBIwrite(dev, k->MyCRA, cra);
}

static void PulseIndex_B(struct comedi_device *dev, struct enc_private *k)
{
	register uint16_t crb;

	crb = DEBIread(dev, k->MyCRB) & ~CRBMSK_INTCTRL;	
	DEBIwrite(dev, k->MyCRB, (uint16_t) (crb ^ CRBMSK_INDXPOL_B));
	DEBIwrite(dev, k->MyCRB, crb);
}


static void Preload(struct comedi_device *dev, struct enc_private *k,
		    uint32_t value)
{
	DEBUG("Preload: preload enter\n");
	DEBIwrite(dev, (uint16_t) (k->MyLatchLsw), (uint16_t) value);	
	DEBUG("Preload: preload step 1\n");
	DEBIwrite(dev, (uint16_t) (k->MyLatchLsw + 2),
		  (uint16_t) (value >> 16));
}

static void CountersInit(struct comedi_device *dev)
{
	int chan;
	struct enc_private *k;
	uint16_t Setup = (LOADSRC_INDX << BF_LOADSRC) |	
	    
	    (INDXSRC_SOFT << BF_INDXSRC) |	
	    (CLKSRC_COUNTER << BF_CLKSRC) |	
	    (CLKPOL_POS << BF_CLKPOL) |	
	    (CNTDIR_UP << BF_CLKPOL) |	
	    (CLKMULT_1X << BF_CLKMULT) |	
	    (CLKENAB_INDEX << BF_CLKENAB);	

	
	for (chan = 0; chan < S626_ENCODER_CHANNELS; chan++) {
		k = &encpriv[chan];
		k->SetMode(dev, k, Setup, TRUE);
		k->SetIntSrc(dev, k, 0);
		k->ResetCapFlags(dev, k);
		k->SetEnable(dev, k, CLKENAB_ALWAYS);
	}
	DEBUG("CountersInit: counters initialized\n");

}
