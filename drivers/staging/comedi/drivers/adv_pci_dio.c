
#include "../comedidev.h"

#include <linux/delay.h>

#include "comedi_pci.h"
#include "8255.h"
#include "8253.h"

#undef PCI_DIO_EXTDEBUG		

#undef DPRINTK
#ifdef PCI_DIO_EXTDEBUG
#define DPRINTK(fmt, args...) printk(fmt, ## args)
#else
#define DPRINTK(fmt, args...)
#endif

#define PCI_VENDOR_ID_ADVANTECH		0x13fe

enum hw_cards_id {
	TYPE_PCI1730, TYPE_PCI1733, TYPE_PCI1734, TYPE_PCI1735, TYPE_PCI1736,
	TYPE_PCI1739,
	TYPE_PCI1750,
	TYPE_PCI1751,
	TYPE_PCI1752,
	TYPE_PCI1753, TYPE_PCI1753E,
	TYPE_PCI1754, TYPE_PCI1756,
	TYPE_PCI1760,
	TYPE_PCI1762
};

enum hw_io_access {
	IO_8b, IO_16b
};

#define MAX_DI_SUBDEVS	2	
#define MAX_DO_SUBDEVS	2	
#define MAX_DIO_SUBDEVG	2	
#define MAX_8254_SUBDEVS   1	

#define SIZE_8254	   4	
#define SIZE_8255	   4	

#define PCIDIO_MAINREG	   2	

#define PCI1730_IDI	   0	
#define PCI1730_IDO	   0	
#define PCI1730_DI	   2	
#define PCI1730_DO	   2	
#define PCI1733_IDI	   0	
#define	PCI1730_3_INT_EN	0x08	
#define	PCI1730_3_INT_RF	0x0c	
#define	PCI1730_3_INT_CLR	0x10	
#define PCI1734_IDO	   0	
#define PCI173x_BOARDID	   4	

#define PCI1735_DI	   0	
#define PCI1735_DO	   0	
#define PCI1735_C8254	   4	
#define PCI1735_BOARDID	   8    

#define PCI1736_IDI        0	
#define PCI1736_IDO        0	
#define PCI1736_3_INT_EN        0x08	
#define PCI1736_3_INT_RF        0x0c	
#define PCI1736_3_INT_CLR       0x10	
#define PCI1736_BOARDID    4	
#define PCI1736_MAINREG    0	

#define PCI1739_DIO	   0	
#define PCI1739_ICR	  32	
#define PCI1739_ISR	  32	
#define PCI1739_BOARDID	   8    

#define PCI1750_IDI	   0	
#define PCI1750_IDO	   0	
#define PCI1750_ICR	  32	
#define PCI1750_ISR	  32	

#define PCI1751_DIO	   0	
#define PCI1751_CNT	  24	
#define PCI1751_ICR	  32	
#define PCI1751_ISR	  32	
#define PCI1753_DIO	   0	
#define PCI1753_ICR0	  16	
#define PCI1753_ICR1	  17	
#define PCI1753_ICR2	  18	
#define PCI1753_ICR3	  19	
#define PCI1753E_DIO	  32	
#define PCI1753E_ICR0	  48	
#define PCI1753E_ICR1	  49	
#define PCI1753E_ICR2	  50	
#define PCI1753E_ICR3	  51	

#define PCI1752_IDO	   0	
#define PCI1752_IDO2	   4	
#define PCI1754_IDI	   0	
#define PCI1754_IDI2	   4	
#define PCI1756_IDI	   0	
#define PCI1756_IDO	   4	
#define PCI1754_6_ICR0	0x08	
#define PCI1754_6_ICR1	0x0a	
#define PCI1754_ICR2	0x0c	
#define PCI1754_ICR3	0x0e	
#define PCI1752_6_CFC	0x12	
#define PCI175x_BOARDID	0x10	

#define PCI1762_RO	   0	
#define PCI1762_IDI	   2	
#define PCI1762_BOARDID	   4	
#define PCI1762_ICR	   6	
#define PCI1762_ISR	   6	

#define OMB0		0x0c	
#define OMB1		0x0d
#define OMB2		0x0e
#define OMB3		0x0f
#define IMB0		0x1c	
#define IMB1		0x1d
#define IMB2		0x1e
#define IMB3		0x1f
#define INTCSR0		0x38	
#define INTCSR1		0x39
#define INTCSR2		0x3a
#define INTCSR3		0x3b

#define CMD_ClearIMB2		0x00	
#define CMD_SetRelaysOutput	0x01	
#define CMD_GetRelaysStatus	0x02	
#define CMD_ReadCurrentStatus	0x07	
#define CMD_ReadFirmwareVersion	0x0e	
#define CMD_ReadHardwareVersion	0x0f	
#define CMD_EnableIDIFilters	0x20	
#define CMD_EnableIDIPatternMatch 0x21	
#define CMD_SetIDIPatternMatch	0x22	
#define CMD_EnableIDICounters	0x28	
#define CMD_ResetIDICounters	0x29	
#define CMD_OverflowIDICounters	0x2a	
#define CMD_MatchIntIDICounters	0x2b	
#define CMD_EdgeIDICounters	0x2c	
#define CMD_GetIDICntCurValue	0x2f	
#define CMD_SetIDI0CntResetValue 0x40	
#define CMD_SetIDI1CntResetValue 0x41	
#define CMD_SetIDI2CntResetValue 0x42	
#define CMD_SetIDI3CntResetValue 0x43	
#define CMD_SetIDI4CntResetValue 0x44	
#define CMD_SetIDI5CntResetValue 0x45	
#define CMD_SetIDI6CntResetValue 0x46	
#define CMD_SetIDI7CntResetValue 0x47	
#define CMD_SetIDI0CntMatchValue 0x48	
#define CMD_SetIDI1CntMatchValue 0x49	
#define CMD_SetIDI2CntMatchValue 0x4a	
#define CMD_SetIDI3CntMatchValue 0x4b	
#define CMD_SetIDI4CntMatchValue 0x4c	
#define CMD_SetIDI5CntMatchValue 0x4d	
#define CMD_SetIDI6CntMatchValue 0x4e	
#define CMD_SetIDI7CntMatchValue 0x4f	

#define OMBCMD_RETRY	0x03	

static int pci_dio_attach(struct comedi_device *dev,
			  struct comedi_devconfig *it);
static int pci_dio_detach(struct comedi_device *dev);

struct diosubd_data {
	int chans;		
	int addr;		
	int regs;		
	unsigned int specflags;	
};

struct dio_boardtype {
	const char *name;	
	int vendor_id;		
	int device_id;
	int main_pci_region;	
	enum hw_cards_id cardtype;
	struct diosubd_data sdi[MAX_DI_SUBDEVS];	
	struct diosubd_data sdo[MAX_DO_SUBDEVS];	
	struct diosubd_data sdio[MAX_DIO_SUBDEVG];	
	struct diosubd_data boardid;	
	struct diosubd_data s8254[MAX_8254_SUBDEVS];	
	enum hw_io_access io_access;
};

static DEFINE_PCI_DEVICE_TABLE(pci_dio_pci_table) = {
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1730) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1733) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1734) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1735) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1736) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1739) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1750) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1751) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1752) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1753) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1754) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1756) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1760) },
	{ PCI_DEVICE(PCI_VENDOR_ID_ADVANTECH, 0x1762) },
	{ 0 }
};

MODULE_DEVICE_TABLE(pci, pci_dio_pci_table);

static const struct dio_boardtype boardtypes[] = {
	{"pci1730", PCI_VENDOR_ID_ADVANTECH, 0x1730, PCIDIO_MAINREG,
	 TYPE_PCI1730,
	 { {16, PCI1730_DI, 2, 0}, {16, PCI1730_IDI, 2, 0} },
	 { {16, PCI1730_DO, 2, 0}, {16, PCI1730_IDO, 2, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 {4, PCI173x_BOARDID, 1, SDF_INTERNAL},
	 { {0, 0, 0, 0} },
	 IO_8b},
	{"pci1733", PCI_VENDOR_ID_ADVANTECH, 0x1733, PCIDIO_MAINREG,
	 TYPE_PCI1733,
	 { {0, 0, 0, 0}, {32, PCI1733_IDI, 4, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 {4, PCI173x_BOARDID, 1, SDF_INTERNAL},
	 { {0, 0, 0, 0} },
	 IO_8b},
	{"pci1734", PCI_VENDOR_ID_ADVANTECH, 0x1734, PCIDIO_MAINREG,
	 TYPE_PCI1734,
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {0, 0, 0, 0}, {32, PCI1734_IDO, 4, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 {4, PCI173x_BOARDID, 1, SDF_INTERNAL},
	 { {0, 0, 0, 0} },
	 IO_8b},
	{"pci1735", PCI_VENDOR_ID_ADVANTECH, 0x1735, PCIDIO_MAINREG,
	 TYPE_PCI1735,
	 { {32, PCI1735_DI, 4, 0}, {0, 0, 0, 0} },
	 { {32, PCI1735_DO, 4, 0}, {0, 0, 0, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { 4, PCI1735_BOARDID, 1, SDF_INTERNAL},
	 { {3, PCI1735_C8254, 1, 0} },
	 IO_8b},
	{"pci1736", PCI_VENDOR_ID_ADVANTECH, 0x1736, PCI1736_MAINREG,
	 TYPE_PCI1736,
	 { {0, 0, 0, 0}, {16, PCI1736_IDI, 2, 0} },
	 { {0, 0, 0, 0}, {16, PCI1736_IDO, 2, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 {4, PCI1736_BOARDID, 1, SDF_INTERNAL},
	 { {0, 0, 0, 0} },
	 IO_8b},
	{"pci1739", PCI_VENDOR_ID_ADVANTECH, 0x1739, PCIDIO_MAINREG,
	 TYPE_PCI1739,
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {48, PCI1739_DIO, 2, 0}, {0, 0, 0, 0} },
	 {0, 0, 0, 0},
	 { {0, 0, 0, 0} },
	 IO_8b},
	{"pci1750", PCI_VENDOR_ID_ADVANTECH, 0x1750, PCIDIO_MAINREG,
	 TYPE_PCI1750,
	 { {0, 0, 0, 0}, {16, PCI1750_IDI, 2, 0} },
	 { {0, 0, 0, 0}, {16, PCI1750_IDO, 2, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 {0, 0, 0, 0},
	 { {0, 0, 0, 0} },
	 IO_8b},
	{"pci1751", PCI_VENDOR_ID_ADVANTECH, 0x1751, PCIDIO_MAINREG,
	 TYPE_PCI1751,
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {48, PCI1751_DIO, 2, 0}, {0, 0, 0, 0} },
	 {0, 0, 0, 0},
	 { {3, PCI1751_CNT, 1, 0} },
	 IO_8b},
	{"pci1752", PCI_VENDOR_ID_ADVANTECH, 0x1752, PCIDIO_MAINREG,
	 TYPE_PCI1752,
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {32, PCI1752_IDO, 2, 0}, {32, PCI1752_IDO2, 2, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 {4, PCI175x_BOARDID, 1, SDF_INTERNAL},
	 { {0, 0, 0, 0} },
	 IO_16b},
	{"pci1753", PCI_VENDOR_ID_ADVANTECH, 0x1753, PCIDIO_MAINREG,
	 TYPE_PCI1753,
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {96, PCI1753_DIO, 4, 0}, {0, 0, 0, 0} },
	 {0, 0, 0, 0},
	 { {0, 0, 0, 0} },
	 IO_8b},
	{"pci1753e", PCI_VENDOR_ID_ADVANTECH, 0x1753, PCIDIO_MAINREG,
	 TYPE_PCI1753E,
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {96, PCI1753_DIO, 4, 0}, {96, PCI1753E_DIO, 4, 0} },
	 {0, 0, 0, 0},
	 { {0, 0, 0, 0} },
	 IO_8b},
	{"pci1754", PCI_VENDOR_ID_ADVANTECH, 0x1754, PCIDIO_MAINREG,
	 TYPE_PCI1754,
	 { {32, PCI1754_IDI, 2, 0}, {32, PCI1754_IDI2, 2, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 {4, PCI175x_BOARDID, 1, SDF_INTERNAL},
	 { {0, 0, 0, 0} },
	 IO_16b},
	{"pci1756", PCI_VENDOR_ID_ADVANTECH, 0x1756, PCIDIO_MAINREG,
	 TYPE_PCI1756,
	 { {0, 0, 0, 0}, {32, PCI1756_IDI, 2, 0} },
	 { {0, 0, 0, 0}, {32, PCI1756_IDO, 2, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 {4, PCI175x_BOARDID, 1, SDF_INTERNAL},
	 { {0, 0, 0, 0} },
	 IO_16b},
	{"pci1760", PCI_VENDOR_ID_ADVANTECH, 0x1760, 0,
	 TYPE_PCI1760,
	 { {0, 0, 0, 0}, {0, 0, 0, 0} }, 
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 {0, 0, 0, 0},
	 { {0, 0, 0, 0} },
	 IO_8b},
	{"pci1762", PCI_VENDOR_ID_ADVANTECH, 0x1762, PCIDIO_MAINREG,
	 TYPE_PCI1762,
	 { {0, 0, 0, 0}, {16, PCI1762_IDI, 1, 0} },
	 { {0, 0, 0, 0}, {16, PCI1762_RO, 1, 0} },
	 { {0, 0, 0, 0}, {0, 0, 0, 0} },
	 {4, PCI1762_BOARDID, 1, SDF_INTERNAL},
	 { {0, 0, 0, 0} },
	 IO_16b}
};

#define n_boardtypes (sizeof(boardtypes)/sizeof(struct dio_boardtype))

static struct comedi_driver driver_pci_dio = {
	.driver_name = "adv_pci_dio",
	.module = THIS_MODULE,
	.attach = pci_dio_attach,
	.detach = pci_dio_detach
};

struct pci_dio_private {
	struct pci_dio_private *prev;	
	struct pci_dio_private *next;	
	struct pci_dev *pcidev;	
	char valid;		
	char GlobalIrqEnabled;	
	
	unsigned char IDICntEnable;	
	unsigned char IDICntOverEnable;	
	unsigned char IDICntMatchEnable;	
	unsigned char IDICntEdge;	
	unsigned short CntResValue[8];	
	unsigned short CntMatchValue[8]; 
	unsigned char IDIFiltersEn; 
	unsigned char IDIPatMatchEn;	
	unsigned char IDIPatMatchValue;	
	unsigned short IDIFiltrLow[8];	
	unsigned short IDIFiltrHigh[8];	
};

static struct pci_dio_private *pci_priv;	

#define devpriv ((struct pci_dio_private *)dev->private)
#define this_board ((const struct dio_boardtype *)dev->board_ptr)

static int pci_dio_insn_bits_di_b(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn, unsigned int *data)
{
	const struct diosubd_data *d = (const struct diosubd_data *)s->private;
	int i;

	data[1] = 0;
	for (i = 0; i < d->regs; i++)
		data[1] |= inb(dev->iobase + d->addr + i) << (8 * i);


	return 2;
}

static int pci_dio_insn_bits_di_w(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn, unsigned int *data)
{
	const struct diosubd_data *d = (const struct diosubd_data *)s->private;
	int i;

	data[1] = 0;
	for (i = 0; i < d->regs; i++)
		data[1] |= inw(dev->iobase + d->addr + 2 * i) << (16 * i);

	return 2;
}

static int pci_dio_insn_bits_do_b(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn, unsigned int *data)
{
	const struct diosubd_data *d = (const struct diosubd_data *)s->private;
	int i;

	if (data[0]) {
		s->state &= ~data[0];
		s->state |= (data[0] & data[1]);
		for (i = 0; i < d->regs; i++)
			outb((s->state >> (8 * i)) & 0xff,
			     dev->iobase + d->addr + i);
	}
	data[1] = s->state;

	return 2;
}

static int pci_dio_insn_bits_do_w(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn, unsigned int *data)
{
	const struct diosubd_data *d = (const struct diosubd_data *)s->private;
	int i;

	if (data[0]) {
		s->state &= ~data[0];
		s->state |= (data[0] & data[1]);
		for (i = 0; i < d->regs; i++)
			outw((s->state >> (16 * i)) & 0xffff,
			     dev->iobase + d->addr + 2 * i);
	}
	data[1] = s->state;

	return 2;
}

static int pci_8254_insn_read(struct comedi_device *dev,
			      struct comedi_subdevice *s,
			      struct comedi_insn *insn, unsigned int *data)
{
	const struct diosubd_data *d = (const struct diosubd_data *)s->private;
	unsigned int chan, chip, chipchan;
	unsigned long flags;

	chan = CR_CHAN(insn->chanspec);	
	chip = chan / 3;		
	chipchan = chan - (3 * chip);	
	spin_lock_irqsave(&s->spin_lock, flags);
	data[0] = i8254_read(dev->iobase + d->addr + (SIZE_8254 * chip),
			0, chipchan);
	spin_unlock_irqrestore(&s->spin_lock, flags);
	return 1;
}

static int pci_8254_insn_write(struct comedi_device *dev,
			       struct comedi_subdevice *s,
			       struct comedi_insn *insn, unsigned int *data)
{
	const struct diosubd_data *d = (const struct diosubd_data *)s->private;
	unsigned int chan, chip, chipchan;
	unsigned long flags;

	chan = CR_CHAN(insn->chanspec);	
	chip = chan / 3;		
	chipchan = chan - (3 * chip);	
	spin_lock_irqsave(&s->spin_lock, flags);
	i8254_write(dev->iobase + d->addr + (SIZE_8254 * chip),
			0, chipchan, data[0]);
	spin_unlock_irqrestore(&s->spin_lock, flags);
	return 1;
}

static int pci_8254_insn_config(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data)
{
	const struct diosubd_data *d = (const struct diosubd_data *)s->private;
	unsigned int chan, chip, chipchan;
	unsigned long iobase;
	int ret = 0;
	unsigned long flags;

	chan = CR_CHAN(insn->chanspec);	
	chip = chan / 3;		
	chipchan = chan - (3 * chip);	
	iobase = dev->iobase + d->addr + (SIZE_8254 * chip);
	spin_lock_irqsave(&s->spin_lock, flags);
	switch (data[0]) {
	case INSN_CONFIG_SET_COUNTER_MODE:
		ret = i8254_set_mode(iobase, 0, chipchan, data[1]);
		if (ret < 0)
			ret = -EINVAL;
		break;
	case INSN_CONFIG_8254_READ_STATUS:
		data[1] = i8254_status(iobase, 0, chipchan);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	spin_unlock_irqrestore(&s->spin_lock, flags);
	return ret < 0 ? ret : insn->n;
}

static int pci1760_unchecked_mbxrequest(struct comedi_device *dev,
					unsigned char *omb, unsigned char *imb,
					int repeats)
{
	int cnt, tout, ok = 0;

	for (cnt = 0; cnt < repeats; cnt++) {
		outb(omb[0], dev->iobase + OMB0);
		outb(omb[1], dev->iobase + OMB1);
		outb(omb[2], dev->iobase + OMB2);
		outb(omb[3], dev->iobase + OMB3);
		for (tout = 0; tout < 251; tout++) {
			imb[2] = inb(dev->iobase + IMB2);
			if (imb[2] == omb[2]) {
				imb[0] = inb(dev->iobase + IMB0);
				imb[1] = inb(dev->iobase + IMB1);
				imb[3] = inb(dev->iobase + IMB3);
				ok = 1;
				break;
			}
			udelay(1);
		}
		if (ok)
			return 0;
	}

	comedi_error(dev, "PCI-1760 mailbox request timeout!");
	return -ETIME;
}

static int pci1760_clear_imb2(struct comedi_device *dev)
{
	unsigned char omb[4] = { 0x0, 0x0, CMD_ClearIMB2, 0x0 };
	unsigned char imb[4];
	
	if (inb(dev->iobase + IMB2) == CMD_ClearIMB2)
		return 0;
	return pci1760_unchecked_mbxrequest(dev, omb, imb, OMBCMD_RETRY);
}

static int pci1760_mbxrequest(struct comedi_device *dev,
			      unsigned char *omb, unsigned char *imb)
{
	if (omb[2] == CMD_ClearIMB2) {
		comedi_error(dev,
			     "bug! this function should not be used for CMD_ClearIMB2 command");
		return -EINVAL;
	}
	if (inb(dev->iobase + IMB2) == omb[2]) {
		int retval;
		retval = pci1760_clear_imb2(dev);
		if (retval < 0)
			return retval;
	}
	return pci1760_unchecked_mbxrequest(dev, omb, imb, OMBCMD_RETRY);
}

static int pci1760_insn_bits_di(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data)
{
	data[1] = inb(dev->iobase + IMB3);

	return 2;
}

static int pci1760_insn_bits_do(struct comedi_device *dev,
				struct comedi_subdevice *s,
				struct comedi_insn *insn, unsigned int *data)
{
	int ret;
	unsigned char omb[4] = {
		0x00,
		0x00,
		CMD_SetRelaysOutput,
		0x00
	};
	unsigned char imb[4];

	if (data[0]) {
		s->state &= ~data[0];
		s->state |= (data[0] & data[1]);
		omb[0] = s->state;
		ret = pci1760_mbxrequest(dev, omb, imb);
		if (!ret)
			return ret;
	}
	data[1] = s->state;

	return 2;
}

static int pci1760_insn_cnt_read(struct comedi_device *dev,
				 struct comedi_subdevice *s,
				 struct comedi_insn *insn, unsigned int *data)
{
	int ret, n;
	unsigned char omb[4] = {
		CR_CHAN(insn->chanspec) & 0x07,
		0x00,
		CMD_GetIDICntCurValue,
		0x00
	};
	unsigned char imb[4];

	for (n = 0; n < insn->n; n++) {
		ret = pci1760_mbxrequest(dev, omb, imb);
		if (!ret)
			return ret;
		data[n] = (imb[1] << 8) + imb[0];
	}

	return n;
}

static int pci1760_insn_cnt_write(struct comedi_device *dev,
				  struct comedi_subdevice *s,
				  struct comedi_insn *insn, unsigned int *data)
{
	int ret;
	unsigned char chan = CR_CHAN(insn->chanspec) & 0x07;
	unsigned char bitmask = 1 << chan;
	unsigned char omb[4] = {
		data[0] & 0xff,
		(data[0] >> 8) & 0xff,
		CMD_SetIDI0CntResetValue + chan,
		0x00
	};
	unsigned char imb[4];

	
	if (devpriv->CntResValue[chan] != (data[0] & 0xffff)) {
		ret = pci1760_mbxrequest(dev, omb, imb);
		if (!ret)
			return ret;
		devpriv->CntResValue[chan] = data[0] & 0xffff;
	}

	omb[0] = bitmask;	
	omb[2] = CMD_ResetIDICounters;
	ret = pci1760_mbxrequest(dev, omb, imb);
	if (!ret)
		return ret;

	
	if (!(bitmask & devpriv->IDICntEnable)) {
		omb[0] = bitmask;
		omb[2] = CMD_EnableIDICounters;
		ret = pci1760_mbxrequest(dev, omb, imb);
		if (!ret)
			return ret;
		devpriv->IDICntEnable |= bitmask;
	}
	return 1;
}

static int pci1760_reset(struct comedi_device *dev)
{
	int i;
	unsigned char omb[4] = { 0x00, 0x00, 0x00, 0x00 };
	unsigned char imb[4];

	outb(0, dev->iobase + INTCSR0);	
	outb(0, dev->iobase + INTCSR1);
	outb(0, dev->iobase + INTCSR2);
	outb(0, dev->iobase + INTCSR3);
	devpriv->GlobalIrqEnabled = 0;

	omb[0] = 0x00;
	omb[2] = CMD_SetRelaysOutput;	
	pci1760_mbxrequest(dev, omb, imb);

	omb[0] = 0x00;
	omb[2] = CMD_EnableIDICounters;	
	pci1760_mbxrequest(dev, omb, imb);
	devpriv->IDICntEnable = 0;

	omb[0] = 0x00;
	omb[2] = CMD_OverflowIDICounters; 
	pci1760_mbxrequest(dev, omb, imb);
	devpriv->IDICntOverEnable = 0;

	omb[0] = 0x00;
	omb[2] = CMD_MatchIntIDICounters; 
	pci1760_mbxrequest(dev, omb, imb);
	devpriv->IDICntMatchEnable = 0;

	omb[0] = 0x00;
	omb[1] = 0x80;
	for (i = 0; i < 8; i++) {	
		omb[2] = CMD_SetIDI0CntMatchValue + i;
		pci1760_mbxrequest(dev, omb, imb);
		devpriv->CntMatchValue[i] = 0x8000;
	}

	omb[0] = 0x00;
	omb[1] = 0x00;
	for (i = 0; i < 8; i++) {	
		omb[2] = CMD_SetIDI0CntResetValue + i;
		pci1760_mbxrequest(dev, omb, imb);
		devpriv->CntResValue[i] = 0x0000;
	}

	omb[0] = 0xff;
	omb[2] = CMD_ResetIDICounters; 
	pci1760_mbxrequest(dev, omb, imb);

	omb[0] = 0x00;
	omb[2] = CMD_EdgeIDICounters;	
	pci1760_mbxrequest(dev, omb, imb);
	devpriv->IDICntEdge = 0x00;

	omb[0] = 0x00;
	omb[2] = CMD_EnableIDIFilters;	
	pci1760_mbxrequest(dev, omb, imb);
	devpriv->IDIFiltersEn = 0x00;

	omb[0] = 0x00;
	omb[2] = CMD_EnableIDIPatternMatch;	
	pci1760_mbxrequest(dev, omb, imb);
	devpriv->IDIPatMatchEn = 0x00;

	omb[0] = 0x00;
	omb[2] = CMD_SetIDIPatternMatch;	
	pci1760_mbxrequest(dev, omb, imb);
	devpriv->IDIPatMatchValue = 0x00;

	return 0;
}

static int pci_dio_reset(struct comedi_device *dev)
{
	DPRINTK("adv_pci_dio EDBG: BGN: pci171x_reset(...)\n");

	switch (this_board->cardtype) {
	case TYPE_PCI1730:
		outb(0, dev->iobase + PCI1730_DO);	
		outb(0, dev->iobase + PCI1730_DO + 1);
		outb(0, dev->iobase + PCI1730_IDO);
		outb(0, dev->iobase + PCI1730_IDO + 1);
		
	case TYPE_PCI1733:
		
		outb(0, dev->iobase + PCI1730_3_INT_EN);
		
		outb(0x0f, dev->iobase + PCI1730_3_INT_CLR);
		
		outb(0, dev->iobase + PCI1730_3_INT_RF);
		break;
	case TYPE_PCI1734:
		outb(0, dev->iobase + PCI1734_IDO);	
		outb(0, dev->iobase + PCI1734_IDO + 1);
		outb(0, dev->iobase + PCI1734_IDO + 2);
		outb(0, dev->iobase + PCI1734_IDO + 3);
		break;
	case TYPE_PCI1735:
		outb(0, dev->iobase + PCI1735_DO);	
		outb(0, dev->iobase + PCI1735_DO + 1);
		outb(0, dev->iobase + PCI1735_DO + 2);
		outb(0, dev->iobase + PCI1735_DO + 3);
		i8254_set_mode(dev->iobase + PCI1735_C8254, 0, 0, I8254_MODE0);
		i8254_set_mode(dev->iobase + PCI1735_C8254, 0, 1, I8254_MODE0);
		i8254_set_mode(dev->iobase + PCI1735_C8254, 0, 2, I8254_MODE0);
		break;

	case TYPE_PCI1736:
		outb(0, dev->iobase + PCI1736_IDO);
		outb(0, dev->iobase + PCI1736_IDO + 1);
		
		outb(0, dev->iobase + PCI1736_3_INT_EN);
		
		outb(0x0f, dev->iobase + PCI1736_3_INT_CLR);
		
		outb(0, dev->iobase + PCI1736_3_INT_RF);
		break;

	case TYPE_PCI1739:
		
		outb(0x88, dev->iobase + PCI1739_ICR);
		break;

	case TYPE_PCI1750:
	case TYPE_PCI1751:
		
		outb(0x88, dev->iobase + PCI1750_ICR);
		break;
	case TYPE_PCI1752:
		outw(0, dev->iobase + PCI1752_6_CFC); 
		outw(0, dev->iobase + PCI1752_IDO);	
		outw(0, dev->iobase + PCI1752_IDO + 2);
		outw(0, dev->iobase + PCI1752_IDO2);
		outw(0, dev->iobase + PCI1752_IDO2 + 2);
		break;
	case TYPE_PCI1753E:
		outb(0x88, dev->iobase + PCI1753E_ICR0); 
		outb(0x80, dev->iobase + PCI1753E_ICR1);
		outb(0x80, dev->iobase + PCI1753E_ICR2);
		outb(0x80, dev->iobase + PCI1753E_ICR3);
		
	case TYPE_PCI1753:
		outb(0x88, dev->iobase + PCI1753_ICR0); 
		outb(0x80, dev->iobase + PCI1753_ICR1);
		outb(0x80, dev->iobase + PCI1753_ICR2);
		outb(0x80, dev->iobase + PCI1753_ICR3);
		break;
	case TYPE_PCI1754:
		outw(0x08, dev->iobase + PCI1754_6_ICR0); 
		outw(0x08, dev->iobase + PCI1754_6_ICR1);
		outw(0x08, dev->iobase + PCI1754_ICR2);
		outw(0x08, dev->iobase + PCI1754_ICR3);
		break;
	case TYPE_PCI1756:
		outw(0, dev->iobase + PCI1752_6_CFC); 
		outw(0x08, dev->iobase + PCI1754_6_ICR0); 
		outw(0x08, dev->iobase + PCI1754_6_ICR1);
		outw(0, dev->iobase + PCI1756_IDO);	
		outw(0, dev->iobase + PCI1756_IDO + 2);
		break;
	case TYPE_PCI1760:
		pci1760_reset(dev);
		break;
	case TYPE_PCI1762:
		outw(0x0101, dev->iobase + PCI1762_ICR); 
		break;
	}

	DPRINTK("adv_pci_dio EDBG: END: pci171x_reset(...)\n");

	return 0;
}

static int pci1760_attach(struct comedi_device *dev,
			  struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	int subdev = 0;

	s = dev->subdevices + subdev;
	s->type = COMEDI_SUBD_DI;
	s->subdev_flags = SDF_READABLE | SDF_GROUND | SDF_COMMON;
	s->n_chan = 8;
	s->maxdata = 1;
	s->len_chanlist = 8;
	s->range_table = &range_digital;
	s->insn_bits = pci1760_insn_bits_di;
	subdev++;

	s = dev->subdevices + subdev;
	s->type = COMEDI_SUBD_DO;
	s->subdev_flags = SDF_WRITABLE | SDF_GROUND | SDF_COMMON;
	s->n_chan = 8;
	s->maxdata = 1;
	s->len_chanlist = 8;
	s->range_table = &range_digital;
	s->state = 0;
	s->insn_bits = pci1760_insn_bits_do;
	subdev++;

	s = dev->subdevices + subdev;
	s->type = COMEDI_SUBD_TIMER;
	s->subdev_flags = SDF_WRITABLE | SDF_LSAMPL;
	s->n_chan = 2;
	s->maxdata = 0xffffffff;
	s->len_chanlist = 2;
	subdev++;

	s = dev->subdevices + subdev;
	s->type = COMEDI_SUBD_COUNTER;
	s->subdev_flags = SDF_READABLE | SDF_WRITABLE;
	s->n_chan = 8;
	s->maxdata = 0xffff;
	s->len_chanlist = 8;
	s->insn_read = pci1760_insn_cnt_read;
	s->insn_write = pci1760_insn_cnt_write;
	subdev++;

	return 0;
}

static int pci_dio_add_di(struct comedi_device *dev, struct comedi_subdevice *s,
			  const struct diosubd_data *d, int subdev)
{
	s->type = COMEDI_SUBD_DI;
	s->subdev_flags = SDF_READABLE | SDF_GROUND | SDF_COMMON | d->specflags;
	if (d->chans > 16)
		s->subdev_flags |= SDF_LSAMPL;
	s->n_chan = d->chans;
	s->maxdata = 1;
	s->len_chanlist = d->chans;
	s->range_table = &range_digital;
	switch (this_board->io_access) {
	case IO_8b:
		s->insn_bits = pci_dio_insn_bits_di_b;
		break;
	case IO_16b:
		s->insn_bits = pci_dio_insn_bits_di_w;
		break;
	}
	s->private = (void *)d;

	return 0;
}

static int pci_dio_add_do(struct comedi_device *dev, struct comedi_subdevice *s,
			  const struct diosubd_data *d, int subdev)
{
	s->type = COMEDI_SUBD_DO;
	s->subdev_flags = SDF_WRITABLE | SDF_GROUND | SDF_COMMON;
	if (d->chans > 16)
		s->subdev_flags |= SDF_LSAMPL;
	s->n_chan = d->chans;
	s->maxdata = 1;
	s->len_chanlist = d->chans;
	s->range_table = &range_digital;
	s->state = 0;
	switch (this_board->io_access) {
	case IO_8b:
		s->insn_bits = pci_dio_insn_bits_do_b;
		break;
	case IO_16b:
		s->insn_bits = pci_dio_insn_bits_do_w;
		break;
	}
	s->private = (void *)d;

	return 0;
}

static int pci_dio_add_8254(struct comedi_device *dev,
			    struct comedi_subdevice *s,
			    const struct diosubd_data *d, int subdev)
{
	s->type = COMEDI_SUBD_COUNTER;
	s->subdev_flags = SDF_WRITABLE | SDF_READABLE;
	s->n_chan = d->chans;
	s->maxdata = 65535;
	s->len_chanlist = d->chans;
	s->insn_read = pci_8254_insn_read;
	s->insn_write = pci_8254_insn_write;
	s->insn_config = pci_8254_insn_config;
	s->private = (void *)d;

	return 0;
}

static int CheckAndAllocCard(struct comedi_device *dev,
			     struct comedi_devconfig *it,
			     struct pci_dev *pcidev)
{
	struct pci_dio_private *pr, *prev;

	for (pr = pci_priv, prev = NULL; pr != NULL; prev = pr, pr = pr->next) {
		if (pr->pcidev == pcidev)
			return 0; 

	}

	if (prev) {
		devpriv->prev = prev;
		prev->next = devpriv;
	} else {
		pci_priv = devpriv;
	}

	devpriv->pcidev = pcidev;

	return 1;
}

static int pci_dio_attach(struct comedi_device *dev,
			  struct comedi_devconfig *it)
{
	struct comedi_subdevice *s;
	int ret, subdev, n_subdevices, i, j;
	unsigned long iobase;
	struct pci_dev *pcidev = NULL;


	ret = alloc_private(dev, sizeof(struct pci_dio_private));
	if (ret < 0)
		return -ENOMEM;

	for_each_pci_dev(pcidev) {
		
		for (i = 0; i < n_boardtypes; ++i) {
			if (boardtypes[i].vendor_id != pcidev->vendor)
				continue;
			if (boardtypes[i].device_id != pcidev->device)
				continue;
			
			if (it->options[0] || it->options[1]) {
				
				if (pcidev->bus->number != it->options[0] ||
				    PCI_SLOT(pcidev->devfn) != it->options[1]) {
					continue;
				}
			}
			ret = CheckAndAllocCard(dev, it, pcidev);
			if (ret != 1)
				continue;
			dev->board_ptr = boardtypes + i;
			break;
		}
		if (dev->board_ptr)
			break;
	}

	if (!dev->board_ptr) {
		dev_err(dev->hw_dev, "Error: Requested type of the card was not found!\n");
		return -EIO;
	}

	if (comedi_pci_enable(pcidev, driver_pci_dio.driver_name)) {
		dev_err(dev->hw_dev, "Error: Can't enable PCI device and request regions!\n");
		return -EIO;
	}
	iobase = pci_resource_start(pcidev, this_board->main_pci_region);
	dev_dbg(dev->hw_dev, "b:s:f=%d:%d:%d, io=0x%4lx\n",
		pcidev->bus->number, PCI_SLOT(pcidev->devfn),
		PCI_FUNC(pcidev->devfn), iobase);

	dev->iobase = iobase;
	dev->board_name = this_board->name;

	if (this_board->cardtype == TYPE_PCI1760) {
		n_subdevices = 4;	
	} else {
		n_subdevices = 0;
		for (i = 0; i < MAX_DI_SUBDEVS; i++)
			if (this_board->sdi[i].chans)
				n_subdevices++;
		for (i = 0; i < MAX_DO_SUBDEVS; i++)
			if (this_board->sdo[i].chans)
				n_subdevices++;
		for (i = 0; i < MAX_DIO_SUBDEVG; i++)
			n_subdevices += this_board->sdio[i].regs;
		if (this_board->boardid.chans)
			n_subdevices++;
		for (i = 0; i < MAX_8254_SUBDEVS; i++)
			if (this_board->s8254[i].chans)
				n_subdevices++;
	}

	ret = alloc_subdevices(dev, n_subdevices);
	if (ret < 0)
		return ret;

	subdev = 0;
	for (i = 0; i < MAX_DI_SUBDEVS; i++)
		if (this_board->sdi[i].chans) {
			s = dev->subdevices + subdev;
			pci_dio_add_di(dev, s, &this_board->sdi[i], subdev);
			subdev++;
		}

	for (i = 0; i < MAX_DO_SUBDEVS; i++)
		if (this_board->sdo[i].chans) {
			s = dev->subdevices + subdev;
			pci_dio_add_do(dev, s, &this_board->sdo[i], subdev);
			subdev++;
		}

	for (i = 0; i < MAX_DIO_SUBDEVG; i++)
		for (j = 0; j < this_board->sdio[i].regs; j++) {
			s = dev->subdevices + subdev;
			subdev_8255_init(dev, s, NULL,
					 dev->iobase +
					 this_board->sdio[i].addr +
					 SIZE_8255 * j);
			subdev++;
		}

	if (this_board->boardid.chans) {
		s = dev->subdevices + subdev;
		s->type = COMEDI_SUBD_DI;
		pci_dio_add_di(dev, s, &this_board->boardid, subdev);
		subdev++;
	}

	for (i = 0; i < MAX_8254_SUBDEVS; i++)
		if (this_board->s8254[i].chans) {
			s = dev->subdevices + subdev;
			pci_dio_add_8254(dev, s, &this_board->s8254[i], subdev);
			subdev++;
		}

	if (this_board->cardtype == TYPE_PCI1760)
		pci1760_attach(dev, it);

	devpriv->valid = 1;

	pci_dio_reset(dev);

	return 0;
}

static int pci_dio_detach(struct comedi_device *dev)
{
	int i, j;
	struct comedi_subdevice *s;
	int subdev;

	if (dev->private) {
		if (devpriv->valid)
			pci_dio_reset(dev);


		subdev = 0;
		for (i = 0; i < MAX_DI_SUBDEVS; i++) {
			if (this_board->sdi[i].chans)
				subdev++;

		}
		for (i = 0; i < MAX_DO_SUBDEVS; i++) {
			if (this_board->sdo[i].chans)
				subdev++;

		}
		for (i = 0; i < MAX_DIO_SUBDEVG; i++) {
			for (j = 0; j < this_board->sdio[i].regs; j++) {
				s = dev->subdevices + subdev;
				subdev_8255_cleanup(dev, s);
				subdev++;
			}
		}

		if (this_board->boardid.chans)
			subdev++;

		for (i = 0; i < MAX_8254_SUBDEVS; i++)
			if (this_board->s8254[i].chans)
				subdev++;

		for (i = 0; i < dev->n_subdevices; i++) {
			s = dev->subdevices + i;
			s->private = NULL;
		}

		if (devpriv->pcidev) {
			if (dev->iobase)
				comedi_pci_disable(devpriv->pcidev);

			pci_dev_put(devpriv->pcidev);
		}

		if (devpriv->prev)
			devpriv->prev->next = devpriv->next;
		else
			pci_priv = devpriv->next;

		if (devpriv->next)
			devpriv->next->prev = devpriv->prev;

	}

	return 0;
}

static int __devinit driver_pci_dio_pci_probe(struct pci_dev *dev,
					      const struct pci_device_id *ent)
{
	return comedi_pci_auto_config(dev, driver_pci_dio.driver_name);
}

static void __devexit driver_pci_dio_pci_remove(struct pci_dev *dev)
{
	comedi_pci_auto_unconfig(dev);
}

static struct pci_driver driver_pci_dio_pci_driver = {
	.id_table = pci_dio_pci_table,
	.probe = &driver_pci_dio_pci_probe,
	.remove = __devexit_p(&driver_pci_dio_pci_remove)
};

static int __init driver_pci_dio_init_module(void)
{
	int retval;

	retval = comedi_driver_register(&driver_pci_dio);
	if (retval < 0)
		return retval;

	driver_pci_dio_pci_driver.name = (char *)driver_pci_dio.driver_name;
	return pci_register_driver(&driver_pci_dio_pci_driver);
}

static void __exit driver_pci_dio_cleanup_module(void)
{
	pci_unregister_driver(&driver_pci_dio_pci_driver);
	comedi_driver_unregister(&driver_pci_dio);
}

module_init(driver_pci_dio_init_module);
module_exit(driver_pci_dio_cleanup_module);

MODULE_AUTHOR("Comedi http://www.comedi.org");
MODULE_DESCRIPTION("Comedi low-level driver");
MODULE_LICENSE("GPL");
