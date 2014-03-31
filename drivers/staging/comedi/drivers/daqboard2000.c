/*
   comedi/drivers/daqboard2000.c
   hardware driver for IOtech DAQboard/2000

   COMEDI - Linux Control and Measurement Device Interface
   Copyright (C) 1999 Anders Blomdell <anders.blomdell@control.lth.se>

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

#include "../comedidev.h"

#include <linux/delay.h>
#include <linux/interrupt.h>

#include "comedi_pci.h"
#include "8255.h"

#define DAQBOARD2000_SUBSYSTEM_IDS2 	0x00021616	
#define DAQBOARD2000_SUBSYSTEM_IDS4 	0x00041616	

#define DAQBOARD2000_DAQ_SIZE 		0x1002
#define DAQBOARD2000_PLX_SIZE 		0x100

#define DAQBOARD2000_SECRProgPinHi      0x8001767e
#define DAQBOARD2000_SECRProgPinLo      0x8000767e
#define DAQBOARD2000_SECRLocalBusHi     0xc000767e
#define DAQBOARD2000_SECRLocalBusLo     0x8000767e
#define DAQBOARD2000_SECRReloadHi       0xa000767e
#define DAQBOARD2000_SECRReloadLo       0x8000767e

#define DAQBOARD2000_EEPROM_PRESENT     0x10000000

#define DAQBOARD2000_CPLD_INIT 		0x0002
#define DAQBOARD2000_CPLD_DONE 		0x0004

static const struct comedi_lrange range_daqboard2000_ai = { 13, {
								 RANGE(-10, 10),
								 RANGE(-5, 5),
								 RANGE(-2.5,
								       2.5),
								 RANGE(-1.25,
								       1.25),
								 RANGE(-0.625,
								       0.625),
								 RANGE(-0.3125,
								       0.3125),
								 RANGE(-0.156,
								       0.156),
								 RANGE(0, 10),
								 RANGE(0, 5),
								 RANGE(0, 2.5),
								 RANGE(0, 1.25),
								 RANGE(0,
								       0.625),
								 RANGE(0,
								       0.3125)
								 }
};

static const struct comedi_lrange range_daqboard2000_ao = { 1, {
								RANGE(-10, 10)
								}
};

struct daqboard2000_hw {
	volatile u16 acqControl;	
	volatile u16 acqScanListFIFO;	
	volatile u32 acqPacerClockDivLow;	

	volatile u16 acqScanCounter;	
	volatile u16 acqPacerClockDivHigh;	
	volatile u16 acqTriggerCount;	
	volatile u16 fill2;	
	volatile u16 acqResultsFIFO;	
	volatile u16 fill3;	
	volatile u16 acqResultsShadow;	
	volatile u16 fill4;	
	volatile u16 acqAdcResult;	
	volatile u16 fill5;	
	volatile u16 dacScanCounter;	
	volatile u16 fill6;	

	volatile u16 dacControl;	
	volatile u16 fill7;	
	volatile s16 dacFIFO;	
	volatile u16 fill8[2];	
	volatile u16 dacPacerClockDiv;	
	volatile u16 refDacs;	
	volatile u16 fill9;	

	volatile u16 dioControl;	
	volatile s16 dioP3hsioData;	
	volatile u16 dioP3Control;	
	volatile u16 calEepromControl;	
	volatile s16 dacSetting[4];	
	volatile s16 dioP2ExpansionIO8Bit[32];	

	volatile u16 ctrTmrControl;	
	volatile u16 fill10[3];	
	volatile s16 ctrInput[4];	
	volatile u16 fill11[8];	
	volatile u16 timerDivisor[2];	
	volatile u16 fill12[6];	

	volatile u16 dmaControl;	
	volatile u16 trigControl;	
	volatile u16 fill13[2];	
	volatile u16 calEeprom;	
	volatile u16 acqDigitalMark;	
	volatile u16 trigDacs;	
	volatile u16 fill14;	
	volatile s16 dioP2ExpansionIO16Bit[32];	
};

#define DAQBOARD2000_SeqStartScanList            0x0011
#define DAQBOARD2000_SeqStopScanList             0x0010

#define DAQBOARD2000_AcqResetScanListFifo        0x0004
#define DAQBOARD2000_AcqResetResultsFifo         0x0002
#define DAQBOARD2000_AcqResetConfigPipe          0x0001

#define DAQBOARD2000_AcqResultsFIFOMore1Sample   0x0001
#define DAQBOARD2000_AcqResultsFIFOHasValidData  0x0002
#define DAQBOARD2000_AcqResultsFIFOOverrun       0x0004
#define DAQBOARD2000_AcqLogicScanning            0x0008
#define DAQBOARD2000_AcqConfigPipeFull           0x0010
#define DAQBOARD2000_AcqScanListFIFOEmpty        0x0020
#define DAQBOARD2000_AcqAdcNotReady              0x0040
#define DAQBOARD2000_ArbitrationFailure          0x0080
#define DAQBOARD2000_AcqPacerOverrun             0x0100
#define DAQBOARD2000_DacPacerOverrun             0x0200
#define DAQBOARD2000_AcqHardwareError            0x01c0

#define DAQBOARD2000_SeqStartScanList            0x0011
#define DAQBOARD2000_SeqStopScanList             0x0010

#define DAQBOARD2000_AdcPacerInternal            0x0030
#define DAQBOARD2000_AdcPacerExternal            0x0032
#define DAQBOARD2000_AdcPacerEnable              0x0031
#define DAQBOARD2000_AdcPacerEnableDacPacer      0x0034
#define DAQBOARD2000_AdcPacerDisable             0x0030
#define DAQBOARD2000_AdcPacerNormalMode          0x0060
#define DAQBOARD2000_AdcPacerCompatibilityMode   0x0061
#define DAQBOARD2000_AdcPacerInternalOutEnable   0x0008
#define DAQBOARD2000_AdcPacerExternalRising      0x0100

#define DAQBOARD2000_DacFull                     0x0001
#define DAQBOARD2000_RefBusy                     0x0002
#define DAQBOARD2000_TrgBusy                     0x0004
#define DAQBOARD2000_CalBusy                     0x0008
#define DAQBOARD2000_Dac0Busy                    0x0010
#define DAQBOARD2000_Dac1Busy                    0x0020
#define DAQBOARD2000_Dac2Busy                    0x0040
#define DAQBOARD2000_Dac3Busy                    0x0080

#define DAQBOARD2000_Dac0Enable                  0x0021
#define DAQBOARD2000_Dac1Enable                  0x0031
#define DAQBOARD2000_Dac2Enable                  0x0041
#define DAQBOARD2000_Dac3Enable                  0x0051
#define DAQBOARD2000_DacEnableBit                0x0001
#define DAQBOARD2000_Dac0Disable                 0x0020
#define DAQBOARD2000_Dac1Disable                 0x0030
#define DAQBOARD2000_Dac2Disable                 0x0040
#define DAQBOARD2000_Dac3Disable                 0x0050
#define DAQBOARD2000_DacResetFifo                0x0004
#define DAQBOARD2000_DacPatternDisable           0x0060
#define DAQBOARD2000_DacPatternEnable            0x0061
#define DAQBOARD2000_DacSelectSignedData         0x0002
#define DAQBOARD2000_DacSelectUnsignedData       0x0000

#define DAQBOARD2000_TrigAnalog                  0x0000
#define DAQBOARD2000_TrigTTL                     0x0010
#define DAQBOARD2000_TrigTransHiLo               0x0004
#define DAQBOARD2000_TrigTransLoHi               0x0000
#define DAQBOARD2000_TrigAbove                   0x0000
#define DAQBOARD2000_TrigBelow                   0x0004
#define DAQBOARD2000_TrigLevelSense              0x0002
#define DAQBOARD2000_TrigEdgeSense               0x0000
#define DAQBOARD2000_TrigEnable                  0x0001
#define DAQBOARD2000_TrigDisable                 0x0000

#define DAQBOARD2000_PosRefDacSelect             0x0100
#define DAQBOARD2000_NegRefDacSelect             0x0000

static int daqboard2000_attach(struct comedi_device *dev,
			       struct comedi_devconfig *it);
static int daqboard2000_detach(struct comedi_device *dev);

static struct comedi_driver driver_daqboard2000 = {
	.driver_name = "daqboard2000",
	.module = THIS_MODULE,
	.attach = daqboard2000_attach,
	.detach = daqboard2000_detach,
};

struct daq200_boardtype {
	const char *name;
	int id;
};
static const struct daq200_boardtype boardtypes[] = {
	{"ids2", DAQBOARD2000_SUBSYSTEM_IDS2},
	{"ids4", DAQBOARD2000_SUBSYSTEM_IDS4},
};

#define n_boardtypes (sizeof(boardtypes)/sizeof(struct daq200_boardtype))
#define this_board ((const struct daq200_boardtype *)dev->board_ptr)

static DEFINE_PCI_DEVICE_TABLE(daqboard2000_pci_table) = {
	{ PCI_DEVICE(0x1616, 0x0409) },
	{0}
};

MODULE_DEVICE_TABLE(pci, daqboard2000_pci_table);

struct daqboard2000_private {
	enum {
		card_daqboard_2000
	} card;
	struct pci_dev *pci_dev;
	void *daq;
	void *plx;
	int got_regions;
	unsigned int ao_readback[2];
};

#define devpriv ((struct daqboard2000_private *)dev->private)

static void writeAcqScanListEntry(struct comedi_device *dev, u16 entry)
{
	struct daqboard2000_hw *fpga = devpriv->daq;

	fpga->acqScanListFIFO = entry & 0x00ff;
	fpga->acqScanListFIFO = (entry >> 8) & 0x00ff;
}

static void setup_sampling(struct comedi_device *dev, int chan, int gain)
{
	u16 word0, word1, word2, word3;

	
	word0 = 0;
	word1 = 0x0004;		
	word2 = (chan << 6) & 0x00c0;
	switch (chan / 4) {
	case 0:
		word3 = 0x0001;
		break;
	case 1:
		word3 = 0x0002;
		break;
	case 2:
		word3 = 0x0005;
		break;
	case 3:
		word3 = 0x0006;
		break;
	case 4:
		word3 = 0x0041;
		break;
	case 5:
		word3 = 0x0042;
		break;
	default:
		word3 = 0;
		break;
	}
	
	word2 |= 0x0800;
	word3 |= 0xc000;
	writeAcqScanListEntry(dev, word0);
	writeAcqScanListEntry(dev, word1);
	writeAcqScanListEntry(dev, word2);
	writeAcqScanListEntry(dev, word3);
}

static int daqboard2000_ai_insn_read(struct comedi_device *dev,
				     struct comedi_subdevice *s,
				     struct comedi_insn *insn,
				     unsigned int *data)
{
	int i;
	struct daqboard2000_hw *fpga = devpriv->daq;
	int gain, chan, timeout;

	fpga->acqControl =
	    DAQBOARD2000_AcqResetScanListFifo |
	    DAQBOARD2000_AcqResetResultsFifo | DAQBOARD2000_AcqResetConfigPipe;

	fpga->acqPacerClockDivLow = 1000000;	
	fpga->acqPacerClockDivHigh = 0;

	gain = CR_RANGE(insn->chanspec);
	chan = CR_CHAN(insn->chanspec);

	for (i = 0; i < insn->n; i++) {
		setup_sampling(dev, chan, gain);
		
		fpga->acqControl = DAQBOARD2000_SeqStartScanList;
		for (timeout = 0; timeout < 20; timeout++) {
			if (fpga->acqControl & DAQBOARD2000_AcqConfigPipeFull)
				break;
			
		}
		fpga->acqControl = DAQBOARD2000_AdcPacerEnable;
		for (timeout = 0; timeout < 20; timeout++) {
			if (fpga->acqControl & DAQBOARD2000_AcqLogicScanning)
				break;
			
		}
		for (timeout = 0; timeout < 20; timeout++) {
			if (fpga->acqControl &
			    DAQBOARD2000_AcqResultsFIFOHasValidData) {
				break;
			}
			
		}
		data[i] = fpga->acqResultsFIFO;
		fpga->acqControl = DAQBOARD2000_AdcPacerDisable;
		fpga->acqControl = DAQBOARD2000_SeqStopScanList;
	}

	return i;
}

static int daqboard2000_ao_insn_read(struct comedi_device *dev,
				     struct comedi_subdevice *s,
				     struct comedi_insn *insn,
				     unsigned int *data)
{
	int i;
	int chan = CR_CHAN(insn->chanspec);

	for (i = 0; i < insn->n; i++)
		data[i] = devpriv->ao_readback[chan];

	return i;
}

static int daqboard2000_ao_insn_write(struct comedi_device *dev,
				      struct comedi_subdevice *s,
				      struct comedi_insn *insn,
				      unsigned int *data)
{
	int i;
	int chan = CR_CHAN(insn->chanspec);
	struct daqboard2000_hw *fpga = devpriv->daq;
	int timeout;

	for (i = 0; i < insn->n; i++) {
		
		fpga->dacSetting[chan] = data[i];
		for (timeout = 0; timeout < 20; timeout++) {
			if ((fpga->dacControl & ((chan + 1) * 0x0010)) == 0)
				break;
			
		}
		devpriv->ao_readback[chan] = data[i];
	}

	return i;
}

static void daqboard2000_resetLocalBus(struct comedi_device *dev)
{
	dev_dbg(dev->hw_dev, "daqboard2000_resetLocalBus\n");
	writel(DAQBOARD2000_SECRLocalBusHi, devpriv->plx + 0x6c);
	udelay(10000);
	writel(DAQBOARD2000_SECRLocalBusLo, devpriv->plx + 0x6c);
	udelay(10000);
}

static void daqboard2000_reloadPLX(struct comedi_device *dev)
{
	dev_dbg(dev->hw_dev, "daqboard2000_reloadPLX\n");
	writel(DAQBOARD2000_SECRReloadLo, devpriv->plx + 0x6c);
	udelay(10000);
	writel(DAQBOARD2000_SECRReloadHi, devpriv->plx + 0x6c);
	udelay(10000);
	writel(DAQBOARD2000_SECRReloadLo, devpriv->plx + 0x6c);
	udelay(10000);
}

static void daqboard2000_pulseProgPin(struct comedi_device *dev)
{
	dev_dbg(dev->hw_dev, "daqboard2000_pulseProgPin 1\n");
	writel(DAQBOARD2000_SECRProgPinHi, devpriv->plx + 0x6c);
	udelay(10000);
	writel(DAQBOARD2000_SECRProgPinLo, devpriv->plx + 0x6c);
	udelay(10000);		
}

static int daqboard2000_pollCPLD(struct comedi_device *dev, int mask)
{
	int result = 0;
	int i;
	int cpld;

	
	for (i = 0; i < 50; i++) {
		cpld = readw(devpriv->daq + 0x1000);
		if ((cpld & mask) == mask) {
			result = 1;
			break;
		}
		udelay(100);
	}
	udelay(5);
	return result;
}

static int daqboard2000_writeCPLD(struct comedi_device *dev, int data)
{
	int result = 0;

	udelay(10);
	writew(data, devpriv->daq + 0x1000);
	if ((readw(devpriv->daq + 0x1000) & DAQBOARD2000_CPLD_INIT) ==
	    DAQBOARD2000_CPLD_INIT) {
		result = 1;
	}
	return result;
}

static int initialize_daqboard2000(struct comedi_device *dev,
				   unsigned char *cpld_array, int len)
{
	int result = -EIO;
	
	int secr;
	int retry;
	int i;

	
	secr = readl(devpriv->plx + 0x6c);
	if (!(secr & DAQBOARD2000_EEPROM_PRESENT)) {
#ifdef DEBUG_EEPROM
		dev_dbg(dev->hw_dev, "no serial eeprom\n");
#endif
		return -EIO;
	}

	for (retry = 0; retry < 3; retry++) {
#ifdef DEBUG_EEPROM
		dev_dbg(dev->hw_dev, "Programming EEPROM try %x\n", retry);
#endif

		daqboard2000_resetLocalBus(dev);
		daqboard2000_reloadPLX(dev);
		daqboard2000_pulseProgPin(dev);
		if (daqboard2000_pollCPLD(dev, DAQBOARD2000_CPLD_INIT)) {
			for (i = 0; i < len; i++) {
				if (cpld_array[i] == 0xff
				    && cpld_array[i + 1] == 0x20) {
#ifdef DEBUG_EEPROM
					dev_dbg(dev->hw_dev, "Preamble found at %d\n",
						i);
#endif
					break;
				}
			}
			for (; i < len; i += 2) {
				int data =
				    (cpld_array[i] << 8) + cpld_array[i + 1];
				if (!daqboard2000_writeCPLD(dev, data))
					break;
			}
			if (i >= len) {
#ifdef DEBUG_EEPROM
				dev_dbg(dev->hw_dev, "Programmed\n");
#endif
				daqboard2000_resetLocalBus(dev);
				daqboard2000_reloadPLX(dev);
				result = 0;
				break;
			}
		}
	}
	return result;
}

static void daqboard2000_adcStopDmaTransfer(struct comedi_device *dev)
{
}

static void daqboard2000_adcDisarm(struct comedi_device *dev)
{
	struct daqboard2000_hw *fpga = devpriv->daq;

	
	udelay(2);
	fpga->trigControl = DAQBOARD2000_TrigAnalog | DAQBOARD2000_TrigDisable;
	udelay(2);
	fpga->trigControl = DAQBOARD2000_TrigTTL | DAQBOARD2000_TrigDisable;

	
	udelay(2);
	fpga->acqControl = DAQBOARD2000_SeqStopScanList;

	
	udelay(2);
	fpga->acqControl = DAQBOARD2000_AdcPacerDisable;

	
	daqboard2000_adcStopDmaTransfer(dev);
}

static void daqboard2000_activateReferenceDacs(struct comedi_device *dev)
{
	struct daqboard2000_hw *fpga = devpriv->daq;
	int timeout;

	
	fpga->refDacs = 0x80 | DAQBOARD2000_PosRefDacSelect;
	for (timeout = 0; timeout < 20; timeout++) {
		if ((fpga->dacControl & DAQBOARD2000_RefBusy) == 0)
			break;
		udelay(2);
	}

	
	fpga->refDacs = 0x80 | DAQBOARD2000_NegRefDacSelect;
	for (timeout = 0; timeout < 20; timeout++) {
		if ((fpga->dacControl & DAQBOARD2000_RefBusy) == 0)
			break;
		udelay(2);
	}
}

static void daqboard2000_initializeCtrs(struct comedi_device *dev)
{
}

static void daqboard2000_initializeTmrs(struct comedi_device *dev)
{
}

static void daqboard2000_dacDisarm(struct comedi_device *dev)
{
}

static void daqboard2000_initializeAdc(struct comedi_device *dev)
{
	daqboard2000_adcDisarm(dev);
	daqboard2000_activateReferenceDacs(dev);
	daqboard2000_initializeCtrs(dev);
	daqboard2000_initializeTmrs(dev);
}

static void daqboard2000_initializeDac(struct comedi_device *dev)
{
	daqboard2000_dacDisarm(dev);
}


static int daqboard2000_8255_cb(int dir, int port, int data,
				unsigned long ioaddr)
{
	int result = 0;
	if (dir) {
		writew(data, ((void *)ioaddr) + port * 2);
		result = 0;
	} else {
		result = readw(((void *)ioaddr) + port * 2);
	}
	return result;
}

static int daqboard2000_attach(struct comedi_device *dev,
			       struct comedi_devconfig *it)
{
	int result = 0;
	struct comedi_subdevice *s;
	struct pci_dev *card = NULL;
	void *aux_data;
	unsigned int aux_len;
	int bus, slot;

	bus = it->options[0];
	slot = it->options[1];

	result = alloc_private(dev, sizeof(struct daqboard2000_private));
	if (result < 0)
		return -ENOMEM;

	for (card = pci_get_device(0x1616, 0x0409, NULL);
	     card != NULL; card = pci_get_device(0x1616, 0x0409, card)) {
		if (bus || slot) {
			
			if (card->bus->number != bus ||
			    PCI_SLOT(card->devfn) != slot) {
				continue;
			}
		}
		break;		
	}
	if (!card) {
		if (bus || slot)
			dev_err(dev->hw_dev, "no daqboard2000 found at bus/slot: %d/%d\n",
				bus, slot);
		else
			dev_err(dev->hw_dev, "no daqboard2000 found\n");
		return -EIO;
	} else {
		u32 id;
		int i;
		devpriv->pci_dev = card;
		id = ((u32) card->
		      subsystem_device << 16) | card->subsystem_vendor;
		for (i = 0; i < n_boardtypes; i++) {
			if (boardtypes[i].id == id) {
				dev_dbg(dev->hw_dev, "%s\n",
					boardtypes[i].name);
				dev->board_ptr = boardtypes + i;
			}
		}
		if (!dev->board_ptr) {
			printk
			    (" unknown subsystem id %08x (pretend it is an ids2)",
			     id);
			dev->board_ptr = boardtypes;
		}
	}

	result = comedi_pci_enable(card, "daqboard2000");
	if (result < 0) {
		dev_err(dev->hw_dev, "failed to enable PCI device and request regions\n");
		return -EIO;
	}
	devpriv->got_regions = 1;
	devpriv->plx =
	    ioremap(pci_resource_start(card, 0), DAQBOARD2000_PLX_SIZE);
	devpriv->daq =
	    ioremap(pci_resource_start(card, 2), DAQBOARD2000_DAQ_SIZE);
	if (!devpriv->plx || !devpriv->daq)
		return -ENOMEM;

	result = alloc_subdevices(dev, 3);
	if (result < 0)
		goto out;

	readl(devpriv->plx + 0x6c);


	aux_data = comedi_aux_data(it->options, 0);
	aux_len = it->options[COMEDI_DEVCONF_AUX_DATA_LENGTH];

	if (aux_data && aux_len) {
		result = initialize_daqboard2000(dev, aux_data, aux_len);
	} else {
		dev_dbg(dev->hw_dev, "no FPGA initialization code, aborting\n");
		result = -EIO;
	}
	if (result < 0)
		goto out;
	daqboard2000_initializeAdc(dev);
	daqboard2000_initializeDac(dev);

	dev->iobase = (unsigned long)devpriv->daq;

	dev->board_name = this_board->name;

	s = dev->subdevices + 0;
	
	s->type = COMEDI_SUBD_AI;
	s->subdev_flags = SDF_READABLE | SDF_GROUND;
	s->n_chan = 24;
	s->maxdata = 0xffff;
	s->insn_read = daqboard2000_ai_insn_read;
	s->range_table = &range_daqboard2000_ai;

	s = dev->subdevices + 1;
	
	s->type = COMEDI_SUBD_AO;
	s->subdev_flags = SDF_WRITABLE;
	s->n_chan = 2;
	s->maxdata = 0xffff;
	s->insn_read = daqboard2000_ao_insn_read;
	s->insn_write = daqboard2000_ao_insn_write;
	s->range_table = &range_daqboard2000_ao;

	s = dev->subdevices + 2;
	result = subdev_8255_init(dev, s, daqboard2000_8255_cb,
				  (unsigned long)(dev->iobase + 0x40));

out:
	return result;
}

static int daqboard2000_detach(struct comedi_device *dev)
{
	if (dev->subdevices)
		subdev_8255_cleanup(dev, dev->subdevices + 2);

	if (dev->irq)
		free_irq(dev->irq, dev);

	if (devpriv) {
		if (devpriv->daq)
			iounmap(devpriv->daq);
		if (devpriv->plx)
			iounmap(devpriv->plx);
		if (devpriv->pci_dev) {
			if (devpriv->got_regions)
				comedi_pci_disable(devpriv->pci_dev);
			pci_dev_put(devpriv->pci_dev);
		}
	}
	return 0;
}

static int __devinit driver_daqboard2000_pci_probe(struct pci_dev *dev,
						   const struct pci_device_id
						   *ent)
{
	return comedi_pci_auto_config(dev, driver_daqboard2000.driver_name);
}

static void __devexit driver_daqboard2000_pci_remove(struct pci_dev *dev)
{
	comedi_pci_auto_unconfig(dev);
}

static struct pci_driver driver_daqboard2000_pci_driver = {
	.id_table = daqboard2000_pci_table,
	.probe = &driver_daqboard2000_pci_probe,
	.remove = __devexit_p(&driver_daqboard2000_pci_remove)
};

static int __init driver_daqboard2000_init_module(void)
{
	int retval;

	retval = comedi_driver_register(&driver_daqboard2000);
	if (retval < 0)
		return retval;

	driver_daqboard2000_pci_driver.name =
	    (char *)driver_daqboard2000.driver_name;
	return pci_register_driver(&driver_daqboard2000_pci_driver);
}

static void __exit driver_daqboard2000_cleanup_module(void)
{
	pci_unregister_driver(&driver_daqboard2000_pci_driver);
	comedi_driver_unregister(&driver_daqboard2000);
}

module_init(driver_daqboard2000_init_module);
module_exit(driver_daqboard2000_cleanup_module);

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
