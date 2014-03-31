#undef	BLOCKMOVE
#define	Z_WAKE
#undef	Z_EXT_CHARS_IN_BUFFER

/*
 * This file contains the driver for the Cyclades async multiport
 * serial boards.
 *
 * Initially written by Randolph Bentson <bentson@grieg.seaslug.org>.
 * Modified and maintained by Marcio Saito <marcio@cyclades.com>.
 *
 * Copyright (C) 2007-2009 Jiri Slaby <jirislaby@gmail.com>
 *
 * Much of the design and some of the code came from serial.c
 * which was copyright (C) 1991, 1992  Linus Torvalds.  It was
 * extensively rewritten by Theodore Ts'o, 8/16/92 -- 9/14/92,
 * and then fixed as suggested by Michael K. Johnson 12/12/92.
 * Converted to pci probing and cleaned up by Jiri Slaby.
 *
 */

#define CY_VERSION	"2.6"


#define NR_CARDS	4


#define NR_PORTS	256

#define ZO_V1	0
#define ZO_V2	1
#define ZE_V1	2

#define	SERIAL_PARANOIA_CHECK
#undef	CY_DEBUG_OPEN
#undef	CY_DEBUG_THROTTLE
#undef	CY_DEBUG_OTHER
#undef	CY_DEBUG_IO
#undef	CY_DEBUG_COUNT
#undef	CY_DEBUG_DTR
#undef	CY_DEBUG_INTERRUPTS
#undef	CY_16Y_HACK
#undef	CY_ENABLE_MONITORING
#undef	CY_PCI_DEBUG

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/cyclades.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/bitops.h>
#include <linux/firmware.h>
#include <linux/device.h>
#include <linux/slab.h>

#include <linux/io.h>
#include <linux/uaccess.h>

#include <linux/kernel.h>
#include <linux/pci.h>

#include <linux/stat.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static void cy_send_xchar(struct tty_struct *tty, char ch);

#ifndef SERIAL_XMIT_SIZE
#define	SERIAL_XMIT_SIZE	(min(PAGE_SIZE, 4096))
#endif

#define STD_COM_FLAGS (0)

#define ZL_MAX_BLOCKS	16
#define DRIVER_VERSION	0x02010203
#define RAM_SIZE 0x80000

enum zblock_type {
	ZBLOCK_PRG = 0,
	ZBLOCK_FPGA = 1
};

struct zfile_header {
	char name[64];
	char date[32];
	char aux[32];
	u32 n_config;
	u32 config_offset;
	u32 n_blocks;
	u32 block_offset;
	u32 reserved[9];
} __attribute__ ((packed));

struct zfile_config {
	char name[64];
	u32 mailbox;
	u32 function;
	u32 n_blocks;
	u32 block_list[ZL_MAX_BLOCKS];
} __attribute__ ((packed));

struct zfile_block {
	u32 type;
	u32 file_offset;
	u32 ram_offset;
	u32 size;
} __attribute__ ((packed));

static struct tty_driver *cy_serial_driver;

#ifdef CONFIG_ISA

static unsigned int cy_isa_addresses[] = {
	0xD0000,
	0xD2000,
	0xD4000,
	0xD6000,
	0xD8000,
	0xDA000,
	0xDC000,
	0xDE000,
	0, 0, 0, 0, 0, 0, 0, 0
};

#define NR_ISA_ADDRS ARRAY_SIZE(cy_isa_addresses)

static long maddr[NR_CARDS];
static int irq[NR_CARDS];

module_param_array(maddr, long, NULL, 0);
module_param_array(irq, int, NULL, 0);

#endif				

static struct cyclades_card cy_card[NR_CARDS];

static int cy_next_channel;	

static const int baud_table[] = {
	0, 50, 75, 110, 134, 150, 200, 300, 600, 1200,
	1800, 2400, 4800, 9600, 19200, 38400, 57600, 76800, 115200, 150000,
	230400, 0
};

static const char baud_co_25[] = {	
	
	
	0x00, 0x04, 0x04, 0x04, 0x04, 0x04, 0x03, 0x03, 0x03, 0x02,
	0x02, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const char baud_bpr_25[] = {	
	0x00, 0xf5, 0xa3, 0x6f, 0x5c, 0x51, 0xf5, 0xa3, 0x51, 0xa3,
	0x6d, 0x51, 0xa3, 0x51, 0xa3, 0x51, 0x36, 0x29, 0x1b, 0x15
};

static const char baud_co_60[] = {	
	
	
	0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04, 0x04, 0x03, 0x03,
	0x03, 0x02, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00
};

static const char baud_bpr_60[] = {	
	0x00, 0x82, 0x21, 0xff, 0xdb, 0xc3, 0x92, 0x62, 0xc3, 0x62,
	0x41, 0xc3, 0x62, 0xc3, 0x62, 0xc3, 0x82, 0x62, 0x41, 0x32,
	0x21
};

static const char baud_cor3[] = {	
	0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a, 0x0a, 0x0a, 0x09, 0x09, 0x08, 0x08, 0x08, 0x08, 0x07,
	0x07
};


static const char rflow_thr[] = {	
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a,
	0x0a
};

static const unsigned int cy_chip_offset[] = { 0x0000,
	0x0400,
	0x0800,
	0x0C00,
	0x0200,
	0x0600,
	0x0A00,
	0x0E00
};


#ifdef CONFIG_PCI
static const struct pci_device_id cy_pci_dev_id[] = {
	
	{ PCI_DEVICE(PCI_VENDOR_ID_CYCLADES, PCI_DEVICE_ID_CYCLOM_Y_Lo) },
	
	{ PCI_DEVICE(PCI_VENDOR_ID_CYCLADES, PCI_DEVICE_ID_CYCLOM_Y_Hi) },
	
	{ PCI_DEVICE(PCI_VENDOR_ID_CYCLADES, PCI_DEVICE_ID_CYCLOM_4Y_Lo) },
	
	{ PCI_DEVICE(PCI_VENDOR_ID_CYCLADES, PCI_DEVICE_ID_CYCLOM_4Y_Hi) },
	
	{ PCI_DEVICE(PCI_VENDOR_ID_CYCLADES, PCI_DEVICE_ID_CYCLOM_8Y_Lo) },
	
	{ PCI_DEVICE(PCI_VENDOR_ID_CYCLADES, PCI_DEVICE_ID_CYCLOM_8Y_Hi) },
	
	{ PCI_DEVICE(PCI_VENDOR_ID_CYCLADES, PCI_DEVICE_ID_CYCLOM_Z_Lo) },
	
	{ PCI_DEVICE(PCI_VENDOR_ID_CYCLADES, PCI_DEVICE_ID_CYCLOM_Z_Hi) },
	{ }			
};
MODULE_DEVICE_TABLE(pci, cy_pci_dev_id);
#endif

static void cy_start(struct tty_struct *);
static void cy_set_line_char(struct cyclades_port *, struct tty_struct *);
static int cyz_issue_cmd(struct cyclades_card *, __u32, __u8, __u32);
#ifdef CONFIG_ISA
static unsigned detect_isa_irq(void __iomem *);
#endif				

#ifndef CONFIG_CYZ_INTR
static void cyz_poll(unsigned long);

static long cyz_polling_cycle = CZ_DEF_POLL;

static DEFINE_TIMER(cyz_timerlist, cyz_poll, 0, 0);

#else				
static void cyz_rx_restart(unsigned long);
static struct timer_list cyz_rx_full_timer[NR_PORTS];
#endif				

static inline void cyy_writeb(struct cyclades_port *port, u32 reg, u8 val)
{
	struct cyclades_card *card = port->card;

	cy_writeb(port->u.cyy.base_addr + (reg << card->bus_index), val);
}

static inline u8 cyy_readb(struct cyclades_port *port, u32 reg)
{
	struct cyclades_card *card = port->card;

	return readb(port->u.cyy.base_addr + (reg << card->bus_index));
}

static inline bool cy_is_Z(struct cyclades_card *card)
{
	return card->num_chips == (unsigned int)-1;
}

static inline bool __cyz_fpga_loaded(struct RUNTIME_9060 __iomem *ctl_addr)
{
	return readl(&ctl_addr->init_ctrl) & (1 << 17);
}

static inline bool cyz_fpga_loaded(struct cyclades_card *card)
{
	return __cyz_fpga_loaded(card->ctl_addr.p9060);
}

static inline bool cyz_is_loaded(struct cyclades_card *card)
{
	struct FIRM_ID __iomem *fw_id = card->base_addr + ID_ADDRESS;

	return (card->hw_ver == ZO_V1 || cyz_fpga_loaded(card)) &&
			readl(&fw_id->signature) == ZFIRM_ID;
}

static inline int serial_paranoia_check(struct cyclades_port *info,
		const char *name, const char *routine)
{
#ifdef SERIAL_PARANOIA_CHECK
	if (!info) {
		printk(KERN_WARNING "cyc Warning: null cyclades_port for (%s) "
				"in %s\n", name, routine);
		return 1;
	}

	if (info->magic != CYCLADES_MAGIC) {
		printk(KERN_WARNING "cyc Warning: bad magic number for serial "
				"struct (%s) in %s\n", name, routine);
		return 1;
	}
#endif
	return 0;
}


static int __cyy_issue_cmd(void __iomem *base_addr, u8 cmd, int index)
{
	void __iomem *ccr = base_addr + (CyCCR << index);
	unsigned int i;

	
	for (i = 0; i < 100; i++) {
		if (readb(ccr) == 0)
			break;
		udelay(10L);
	}
	if (i == 100)
		return -1;

	
	cy_writeb(ccr, cmd);

	return 0;
}

static inline int cyy_issue_cmd(struct cyclades_port *port, u8 cmd)
{
	return __cyy_issue_cmd(port->u.cyy.base_addr, cmd,
			port->card->bus_index);
}

#ifdef CONFIG_ISA
static unsigned detect_isa_irq(void __iomem *address)
{
	int irq;
	unsigned long irqs, flags;
	int save_xir, save_car;
	int index = 0;		

	
	irq = probe_irq_off(probe_irq_on());

	
	cy_writeb(address + (Cy_ClrIntr << index), 0);
	

	irqs = probe_irq_on();
	
	msleep(5);

	
	local_irq_save(flags);
	cy_writeb(address + (CyCAR << index), 0);
	__cyy_issue_cmd(address, CyCHAN_CTL | CyENB_XMTR, index);

	cy_writeb(address + (CyCAR << index), 0);
	cy_writeb(address + (CySRER << index),
		  readb(address + (CySRER << index)) | CyTxRdy);
	local_irq_restore(flags);

	
	msleep(5);

	
	irq = probe_irq_off(irqs);

	
	save_xir = (u_char) readb(address + (CyTIR << index));
	save_car = readb(address + (CyCAR << index));
	cy_writeb(address + (CyCAR << index), (save_xir & 0x3));
	cy_writeb(address + (CySRER << index),
		  readb(address + (CySRER << index)) & ~CyTxRdy);
	cy_writeb(address + (CyTIR << index), (save_xir & 0x3f));
	cy_writeb(address + (CyCAR << index), (save_car));
	cy_writeb(address + (Cy_ClrIntr << index), 0);
	

	return (irq > 0) ? irq : 0;
}
#endif				

static void cyy_chip_rx(struct cyclades_card *cinfo, int chip,
		void __iomem *base_addr)
{
	struct cyclades_port *info;
	struct tty_struct *tty;
	int len, index = cinfo->bus_index;
	u8 ivr, save_xir, channel, save_car, data, char_count;

#ifdef CY_DEBUG_INTERRUPTS
	printk(KERN_DEBUG "cyy_interrupt: rcvd intr, chip %d\n", chip);
#endif
	
	save_xir = readb(base_addr + (CyRIR << index));
	channel = save_xir & CyIRChannel;
	info = &cinfo->ports[channel + chip * 4];
	save_car = cyy_readb(info, CyCAR);
	cyy_writeb(info, CyCAR, save_xir);
	ivr = cyy_readb(info, CyRIVR) & CyIVRMask;

	tty = tty_port_tty_get(&info->port);
	
	if (tty == NULL) {
		if (ivr == CyIVRRxEx) {	
			data = cyy_readb(info, CyRDSR);
		} else {	
			char_count = cyy_readb(info, CyRDCR);
			while (char_count--)
				data = cyy_readb(info, CyRDSR);
		}
		goto end;
	}
	
	if (ivr == CyIVRRxEx) {	
		data = cyy_readb(info, CyRDSR);

		
		if (data & CyBREAK)
			info->icount.brk++;
		else if (data & CyFRAME)
			info->icount.frame++;
		else if (data & CyPARITY)
			info->icount.parity++;
		else if (data & CyOVERRUN)
			info->icount.overrun++;

		if (data & info->ignore_status_mask) {
			info->icount.rx++;
			tty_kref_put(tty);
			return;
		}
		if (tty_buffer_request_room(tty, 1)) {
			if (data & info->read_status_mask) {
				if (data & CyBREAK) {
					tty_insert_flip_char(tty,
						cyy_readb(info, CyRDSR),
						TTY_BREAK);
					info->icount.rx++;
					if (info->port.flags & ASYNC_SAK)
						do_SAK(tty);
				} else if (data & CyFRAME) {
					tty_insert_flip_char(tty,
						cyy_readb(info, CyRDSR),
						TTY_FRAME);
					info->icount.rx++;
					info->idle_stats.frame_errs++;
				} else if (data & CyPARITY) {
					
					tty_insert_flip_char(tty,
						cyy_readb(info, CyRDSR),
						TTY_PARITY);
					info->icount.rx++;
					info->idle_stats.parity_errs++;
				} else if (data & CyOVERRUN) {
					tty_insert_flip_char(tty, 0,
							TTY_OVERRUN);
					info->icount.rx++;
					tty_insert_flip_char(tty,
						cyy_readb(info, CyRDSR),
						TTY_FRAME);
					info->icount.rx++;
					info->idle_stats.overruns++;
				
				
				
				
				} else {
					tty_insert_flip_char(tty, 0,
							TTY_NORMAL);
					info->icount.rx++;
				}
			} else {
				tty_insert_flip_char(tty, 0, TTY_NORMAL);
				info->icount.rx++;
			}
		} else {
			info->icount.buf_overrun++;
			info->idle_stats.overruns++;
		}
	} else {	
		
		char_count = cyy_readb(info, CyRDCR);

#ifdef CY_ENABLE_MONITORING
		++info->mon.int_count;
		info->mon.char_count += char_count;
		if (char_count > info->mon.char_max)
			info->mon.char_max = char_count;
		info->mon.char_last = char_count;
#endif
		len = tty_buffer_request_room(tty, char_count);
		while (len--) {
			data = cyy_readb(info, CyRDSR);
			tty_insert_flip_char(tty, data, TTY_NORMAL);
			info->idle_stats.recv_bytes++;
			info->icount.rx++;
#ifdef CY_16Y_HACK
			udelay(10L);
#endif
		}
		info->idle_stats.recv_idle = jiffies;
	}
	tty_schedule_flip(tty);
	tty_kref_put(tty);
end:
	
	cyy_writeb(info, CyRIR, save_xir & 0x3f);
	cyy_writeb(info, CyCAR, save_car);
}

static void cyy_chip_tx(struct cyclades_card *cinfo, unsigned int chip,
		void __iomem *base_addr)
{
	struct cyclades_port *info;
	struct tty_struct *tty;
	int char_count, index = cinfo->bus_index;
	u8 save_xir, channel, save_car, outch;

#ifdef CY_DEBUG_INTERRUPTS
	printk(KERN_DEBUG "cyy_interrupt: xmit intr, chip %d\n", chip);
#endif

	
	save_xir = readb(base_addr + (CyTIR << index));
	channel = save_xir & CyIRChannel;
	save_car = readb(base_addr + (CyCAR << index));
	cy_writeb(base_addr + (CyCAR << index), save_xir);

	info = &cinfo->ports[channel + chip * 4];
	tty = tty_port_tty_get(&info->port);
	if (tty == NULL) {
		cyy_writeb(info, CySRER, cyy_readb(info, CySRER) & ~CyTxRdy);
		goto end;
	}

	
	char_count = info->xmit_fifo_size;

	if (info->x_char) {	
		outch = info->x_char;
		cyy_writeb(info, CyTDR, outch);
		char_count--;
		info->icount.tx++;
		info->x_char = 0;
	}

	if (info->breakon || info->breakoff) {
		if (info->breakon) {
			cyy_writeb(info, CyTDR, 0);
			cyy_writeb(info, CyTDR, 0x81);
			info->breakon = 0;
			char_count -= 2;
		}
		if (info->breakoff) {
			cyy_writeb(info, CyTDR, 0);
			cyy_writeb(info, CyTDR, 0x83);
			info->breakoff = 0;
			char_count -= 2;
		}
	}

	while (char_count-- > 0) {
		if (!info->xmit_cnt) {
			if (cyy_readb(info, CySRER) & CyTxMpty) {
				cyy_writeb(info, CySRER,
					cyy_readb(info, CySRER) & ~CyTxMpty);
			} else {
				cyy_writeb(info, CySRER, CyTxMpty |
					(cyy_readb(info, CySRER) & ~CyTxRdy));
			}
			goto done;
		}
		if (info->port.xmit_buf == NULL) {
			cyy_writeb(info, CySRER,
				cyy_readb(info, CySRER) & ~CyTxRdy);
			goto done;
		}
		if (tty->stopped || tty->hw_stopped) {
			cyy_writeb(info, CySRER,
				cyy_readb(info, CySRER) & ~CyTxRdy);
			goto done;
		}
		outch = info->port.xmit_buf[info->xmit_tail];
		if (outch) {
			info->xmit_cnt--;
			info->xmit_tail = (info->xmit_tail + 1) &
					(SERIAL_XMIT_SIZE - 1);
			cyy_writeb(info, CyTDR, outch);
			info->icount.tx++;
		} else {
			if (char_count > 1) {
				info->xmit_cnt--;
				info->xmit_tail = (info->xmit_tail + 1) &
					(SERIAL_XMIT_SIZE - 1);
				cyy_writeb(info, CyTDR, outch);
				cyy_writeb(info, CyTDR, 0);
				info->icount.tx++;
				char_count--;
			}
		}
	}

done:
	tty_wakeup(tty);
	tty_kref_put(tty);
end:
	
	cyy_writeb(info, CyTIR, save_xir & 0x3f);
	cyy_writeb(info, CyCAR, save_car);
}

static void cyy_chip_modem(struct cyclades_card *cinfo, int chip,
		void __iomem *base_addr)
{
	struct cyclades_port *info;
	struct tty_struct *tty;
	int index = cinfo->bus_index;
	u8 save_xir, channel, save_car, mdm_change, mdm_status;

	
	save_xir = readb(base_addr + (CyMIR << index));
	channel = save_xir & CyIRChannel;
	info = &cinfo->ports[channel + chip * 4];
	save_car = cyy_readb(info, CyCAR);
	cyy_writeb(info, CyCAR, save_xir);

	mdm_change = cyy_readb(info, CyMISR);
	mdm_status = cyy_readb(info, CyMSVR1);

	tty = tty_port_tty_get(&info->port);
	if (!tty)
		goto end;

	if (mdm_change & CyANY_DELTA) {
		
		if (mdm_change & CyDCD)
			info->icount.dcd++;
		if (mdm_change & CyCTS)
			info->icount.cts++;
		if (mdm_change & CyDSR)
			info->icount.dsr++;
		if (mdm_change & CyRI)
			info->icount.rng++;

		wake_up_interruptible(&info->port.delta_msr_wait);
	}

	if ((mdm_change & CyDCD) && (info->port.flags & ASYNC_CHECK_CD)) {
		if (mdm_status & CyDCD)
			wake_up_interruptible(&info->port.open_wait);
		else
			tty_hangup(tty);
	}
	if ((mdm_change & CyCTS) && (info->port.flags & ASYNC_CTS_FLOW)) {
		if (tty->hw_stopped) {
			if (mdm_status & CyCTS) {
				tty->hw_stopped = 0;
				cyy_writeb(info, CySRER,
					cyy_readb(info, CySRER) | CyTxRdy);
				tty_wakeup(tty);
			}
		} else {
			if (!(mdm_status & CyCTS)) {
				tty->hw_stopped = 1;
				cyy_writeb(info, CySRER,
					cyy_readb(info, CySRER) & ~CyTxRdy);
			}
		}
	}
	tty_kref_put(tty);
end:
	
	cyy_writeb(info, CyMIR, save_xir & 0x3f);
	cyy_writeb(info, CyCAR, save_car);
}

static irqreturn_t cyy_interrupt(int irq, void *dev_id)
{
	int status;
	struct cyclades_card *cinfo = dev_id;
	void __iomem *base_addr, *card_base_addr;
	unsigned int chip, too_many, had_work;
	int index;

	if (unlikely(cinfo == NULL)) {
#ifdef CY_DEBUG_INTERRUPTS
		printk(KERN_DEBUG "cyy_interrupt: spurious interrupt %d\n",
				irq);
#endif
		return IRQ_NONE;	
	}

	card_base_addr = cinfo->base_addr;
	index = cinfo->bus_index;

	
	if (unlikely(card_base_addr == NULL))
		return IRQ_HANDLED;

	do {
		had_work = 0;
		for (chip = 0; chip < cinfo->num_chips; chip++) {
			base_addr = cinfo->base_addr +
					(cy_chip_offset[chip] << index);
			too_many = 0;
			while ((status = readb(base_addr +
						(CySVRR << index))) != 0x00) {
				had_work++;
				if (1000 < too_many++)
					break;
				spin_lock(&cinfo->card_lock);
				if (status & CySRReceive) 
					cyy_chip_rx(cinfo, chip, base_addr);
				if (status & CySRTransmit) 
					cyy_chip_tx(cinfo, chip, base_addr);
				if (status & CySRModem) 
					cyy_chip_modem(cinfo, chip, base_addr);
				spin_unlock(&cinfo->card_lock);
			}
		}
	} while (had_work);

	
	spin_lock(&cinfo->card_lock);
	cy_writeb(card_base_addr + (Cy_ClrIntr << index), 0);
	
	spin_unlock(&cinfo->card_lock);
	return IRQ_HANDLED;
}				

static void cyy_change_rts_dtr(struct cyclades_port *info, unsigned int set,
		unsigned int clear)
{
	struct cyclades_card *card = info->card;
	int channel = info->line - card->first_line;
	u32 rts, dtr, msvrr, msvrd;

	channel &= 0x03;

	if (info->rtsdtr_inv) {
		msvrr = CyMSVR2;
		msvrd = CyMSVR1;
		rts = CyDTR;
		dtr = CyRTS;
	} else {
		msvrr = CyMSVR1;
		msvrd = CyMSVR2;
		rts = CyRTS;
		dtr = CyDTR;
	}
	if (set & TIOCM_RTS) {
		cyy_writeb(info, CyCAR, channel);
		cyy_writeb(info, msvrr, rts);
	}
	if (clear & TIOCM_RTS) {
		cyy_writeb(info, CyCAR, channel);
		cyy_writeb(info, msvrr, ~rts);
	}
	if (set & TIOCM_DTR) {
		cyy_writeb(info, CyCAR, channel);
		cyy_writeb(info, msvrd, dtr);
#ifdef CY_DEBUG_DTR
		printk(KERN_DEBUG "cyc:set_modem_info raising DTR\n");
		printk(KERN_DEBUG "     status: 0x%x, 0x%x\n",
			cyy_readb(info, CyMSVR1),
			cyy_readb(info, CyMSVR2));
#endif
	}
	if (clear & TIOCM_DTR) {
		cyy_writeb(info, CyCAR, channel);
		cyy_writeb(info, msvrd, ~dtr);
#ifdef CY_DEBUG_DTR
		printk(KERN_DEBUG "cyc:set_modem_info dropping DTR\n");
		printk(KERN_DEBUG "     status: 0x%x, 0x%x\n",
			cyy_readb(info, CyMSVR1),
			cyy_readb(info, CyMSVR2));
#endif
	}
}


static int
cyz_fetch_msg(struct cyclades_card *cinfo,
		__u32 *channel, __u8 *cmd, __u32 *param)
{
	struct BOARD_CTRL __iomem *board_ctrl = cinfo->board_ctrl;
	unsigned long loc_doorbell;

	loc_doorbell = readl(&cinfo->ctl_addr.p9060->loc_doorbell);
	if (loc_doorbell) {
		*cmd = (char)(0xff & loc_doorbell);
		*channel = readl(&board_ctrl->fwcmd_channel);
		*param = (__u32) readl(&board_ctrl->fwcmd_param);
		cy_writel(&cinfo->ctl_addr.p9060->loc_doorbell, 0xffffffff);
		return 1;
	}
	return 0;
}				

static int
cyz_issue_cmd(struct cyclades_card *cinfo,
		__u32 channel, __u8 cmd, __u32 param)
{
	struct BOARD_CTRL __iomem *board_ctrl = cinfo->board_ctrl;
	__u32 __iomem *pci_doorbell;
	unsigned int index;

	if (!cyz_is_loaded(cinfo))
		return -1;

	index = 0;
	pci_doorbell = &cinfo->ctl_addr.p9060->pci_doorbell;
	while ((readl(pci_doorbell) & 0xff) != 0) {
		if (index++ == 1000)
			return (int)(readl(pci_doorbell) & 0xff);
		udelay(50L);
	}
	cy_writel(&board_ctrl->hcmd_channel, channel);
	cy_writel(&board_ctrl->hcmd_param, param);
	cy_writel(pci_doorbell, (long)cmd);

	return 0;
}				

static void cyz_handle_rx(struct cyclades_port *info, struct tty_struct *tty)
{
	struct BUF_CTRL __iomem *buf_ctrl = info->u.cyz.buf_ctrl;
	struct cyclades_card *cinfo = info->card;
	unsigned int char_count;
	int len;
#ifdef BLOCKMOVE
	unsigned char *buf;
#else
	char data;
#endif
	__u32 rx_put, rx_get, new_rx_get, rx_bufsize, rx_bufaddr;

	rx_get = new_rx_get = readl(&buf_ctrl->rx_get);
	rx_put = readl(&buf_ctrl->rx_put);
	rx_bufsize = readl(&buf_ctrl->rx_bufsize);
	rx_bufaddr = readl(&buf_ctrl->rx_bufaddr);
	if (rx_put >= rx_get)
		char_count = rx_put - rx_get;
	else
		char_count = rx_put - rx_get + rx_bufsize;

	if (char_count) {
#ifdef CY_ENABLE_MONITORING
		info->mon.int_count++;
		info->mon.char_count += char_count;
		if (char_count > info->mon.char_max)
			info->mon.char_max = char_count;
		info->mon.char_last = char_count;
#endif
		if (tty == NULL) {
			
			new_rx_get = (new_rx_get + char_count) &
					(rx_bufsize - 1);
			info->rflush_count++;
		} else {
#ifdef BLOCKMOVE
			while (1) {
				len = tty_prepare_flip_string(tty, &buf,
						char_count);
				if (!len)
					break;

				len = min_t(unsigned int, min(len, char_count),
						rx_bufsize - new_rx_get);

				memcpy_fromio(buf, cinfo->base_addr +
						rx_bufaddr + new_rx_get, len);

				new_rx_get = (new_rx_get + len) &
						(rx_bufsize - 1);
				char_count -= len;
				info->icount.rx += len;
				info->idle_stats.recv_bytes += len;
			}
#else
			len = tty_buffer_request_room(tty, char_count);
			while (len--) {
				data = readb(cinfo->base_addr + rx_bufaddr +
						new_rx_get);
				new_rx_get = (new_rx_get + 1) &
							(rx_bufsize - 1);
				tty_insert_flip_char(tty, data, TTY_NORMAL);
				info->idle_stats.recv_bytes++;
				info->icount.rx++;
			}
#endif
#ifdef CONFIG_CYZ_INTR
			rx_put = readl(&buf_ctrl->rx_put);
			if (rx_put >= rx_get)
				char_count = rx_put - rx_get;
			else
				char_count = rx_put - rx_get + rx_bufsize;
			if (char_count >= readl(&buf_ctrl->rx_threshold) &&
					!timer_pending(&cyz_rx_full_timer[
							info->line]))
				mod_timer(&cyz_rx_full_timer[info->line],
						jiffies + 1);
#endif
			info->idle_stats.recv_idle = jiffies;
			tty_schedule_flip(tty);
		}
		
		cy_writel(&buf_ctrl->rx_get, new_rx_get);
	}
}

static void cyz_handle_tx(struct cyclades_port *info, struct tty_struct *tty)
{
	struct BUF_CTRL __iomem *buf_ctrl = info->u.cyz.buf_ctrl;
	struct cyclades_card *cinfo = info->card;
	u8 data;
	unsigned int char_count;
#ifdef BLOCKMOVE
	int small_count;
#endif
	__u32 tx_put, tx_get, tx_bufsize, tx_bufaddr;

	if (info->xmit_cnt <= 0)	
		return;

	tx_get = readl(&buf_ctrl->tx_get);
	tx_put = readl(&buf_ctrl->tx_put);
	tx_bufsize = readl(&buf_ctrl->tx_bufsize);
	tx_bufaddr = readl(&buf_ctrl->tx_bufaddr);
	if (tx_put >= tx_get)
		char_count = tx_get - tx_put - 1 + tx_bufsize;
	else
		char_count = tx_get - tx_put - 1;

	if (char_count) {

		if (tty == NULL)
			goto ztxdone;

		if (info->x_char) {	
			data = info->x_char;

			cy_writeb(cinfo->base_addr + tx_bufaddr + tx_put, data);
			tx_put = (tx_put + 1) & (tx_bufsize - 1);
			info->x_char = 0;
			char_count--;
			info->icount.tx++;
		}
#ifdef BLOCKMOVE
		while (0 < (small_count = min_t(unsigned int,
				tx_bufsize - tx_put, min_t(unsigned int,
					(SERIAL_XMIT_SIZE - info->xmit_tail),
					min_t(unsigned int, info->xmit_cnt,
						char_count))))) {

			memcpy_toio((char *)(cinfo->base_addr + tx_bufaddr +
					tx_put),
					&info->port.xmit_buf[info->xmit_tail],
					small_count);

			tx_put = (tx_put + small_count) & (tx_bufsize - 1);
			char_count -= small_count;
			info->icount.tx += small_count;
			info->xmit_cnt -= small_count;
			info->xmit_tail = (info->xmit_tail + small_count) &
					(SERIAL_XMIT_SIZE - 1);
		}
#else
		while (info->xmit_cnt && char_count) {
			data = info->port.xmit_buf[info->xmit_tail];
			info->xmit_cnt--;
			info->xmit_tail = (info->xmit_tail + 1) &
					(SERIAL_XMIT_SIZE - 1);

			cy_writeb(cinfo->base_addr + tx_bufaddr + tx_put, data);
			tx_put = (tx_put + 1) & (tx_bufsize - 1);
			char_count--;
			info->icount.tx++;
		}
#endif
		tty_wakeup(tty);
ztxdone:
		
		cy_writel(&buf_ctrl->tx_put, tx_put);
	}
}

static void cyz_handle_cmd(struct cyclades_card *cinfo)
{
	struct BOARD_CTRL __iomem *board_ctrl = cinfo->board_ctrl;
	struct tty_struct *tty;
	struct cyclades_port *info;
	__u32 channel, param, fw_ver;
	__u8 cmd;
	int special_count;
	int delta_count;

	fw_ver = readl(&board_ctrl->fw_version);

	while (cyz_fetch_msg(cinfo, &channel, &cmd, &param) == 1) {
		special_count = 0;
		delta_count = 0;
		info = &cinfo->ports[channel];
		tty = tty_port_tty_get(&info->port);
		if (tty == NULL)
			continue;

		switch (cmd) {
		case C_CM_PR_ERROR:
			tty_insert_flip_char(tty, 0, TTY_PARITY);
			info->icount.rx++;
			special_count++;
			break;
		case C_CM_FR_ERROR:
			tty_insert_flip_char(tty, 0, TTY_FRAME);
			info->icount.rx++;
			special_count++;
			break;
		case C_CM_RXBRK:
			tty_insert_flip_char(tty, 0, TTY_BREAK);
			info->icount.rx++;
			special_count++;
			break;
		case C_CM_MDCD:
			info->icount.dcd++;
			delta_count++;
			if (info->port.flags & ASYNC_CHECK_CD) {
				u32 dcd = fw_ver > 241 ? param :
					readl(&info->u.cyz.ch_ctrl->rs_status);
				if (dcd & C_RS_DCD)
					wake_up_interruptible(&info->port.open_wait);
				else
					tty_hangup(tty);
			}
			break;
		case C_CM_MCTS:
			info->icount.cts++;
			delta_count++;
			break;
		case C_CM_MRI:
			info->icount.rng++;
			delta_count++;
			break;
		case C_CM_MDSR:
			info->icount.dsr++;
			delta_count++;
			break;
#ifdef Z_WAKE
		case C_CM_IOCTLW:
			complete(&info->shutdown_wait);
			break;
#endif
#ifdef CONFIG_CYZ_INTR
		case C_CM_RXHIWM:
		case C_CM_RXNNDT:
		case C_CM_INTBACK2:
			
#ifdef CY_DEBUG_INTERRUPTS
			printk(KERN_DEBUG "cyz_interrupt: rcvd intr, card %d, "
					"port %ld\n", info->card, channel);
#endif
			cyz_handle_rx(info, tty);
			break;
		case C_CM_TXBEMPTY:
		case C_CM_TXLOWWM:
		case C_CM_INTBACK:
			
#ifdef CY_DEBUG_INTERRUPTS
			printk(KERN_DEBUG "cyz_interrupt: xmit intr, card %d, "
					"port %ld\n", info->card, channel);
#endif
			cyz_handle_tx(info, tty);
			break;
#endif				
		case C_CM_FATAL:
			
			break;
		default:
			break;
		}
		if (delta_count)
			wake_up_interruptible(&info->port.delta_msr_wait);
		if (special_count)
			tty_schedule_flip(tty);
		tty_kref_put(tty);
	}
}

#ifdef CONFIG_CYZ_INTR
static irqreturn_t cyz_interrupt(int irq, void *dev_id)
{
	struct cyclades_card *cinfo = dev_id;

	if (unlikely(!cyz_is_loaded(cinfo))) {
#ifdef CY_DEBUG_INTERRUPTS
		printk(KERN_DEBUG "cyz_interrupt: board not yet loaded "
				"(IRQ%d).\n", irq);
#endif
		return IRQ_NONE;
	}

	
	cyz_handle_cmd(cinfo);

	return IRQ_HANDLED;
}				

static void cyz_rx_restart(unsigned long arg)
{
	struct cyclades_port *info = (struct cyclades_port *)arg;
	struct cyclades_card *card = info->card;
	int retval;
	__u32 channel = info->line - card->first_line;
	unsigned long flags;

	spin_lock_irqsave(&card->card_lock, flags);
	retval = cyz_issue_cmd(card, channel, C_CM_INTBACK2, 0L);
	if (retval != 0) {
		printk(KERN_ERR "cyc:cyz_rx_restart retval on ttyC%d was %x\n",
			info->line, retval);
	}
	spin_unlock_irqrestore(&card->card_lock, flags);
}

#else				

static void cyz_poll(unsigned long arg)
{
	struct cyclades_card *cinfo;
	struct cyclades_port *info;
	unsigned long expires = jiffies + HZ;
	unsigned int port, card;

	for (card = 0; card < NR_CARDS; card++) {
		cinfo = &cy_card[card];

		if (!cy_is_Z(cinfo))
			continue;
		if (!cyz_is_loaded(cinfo))
			continue;

	
		if (!cinfo->intr_enabled) {
			cinfo->intr_enabled = 1;
			continue;
		}

		cyz_handle_cmd(cinfo);

		for (port = 0; port < cinfo->nports; port++) {
			struct tty_struct *tty;

			info = &cinfo->ports[port];
			tty = tty_port_tty_get(&info->port);

			if (!info->throttle)
				cyz_handle_rx(info, tty);
			cyz_handle_tx(info, tty);
			tty_kref_put(tty);
		}
		
		expires = jiffies + cyz_polling_cycle;
	}
	mod_timer(&cyz_timerlist, expires);
}				

#endif				


static int cy_startup(struct cyclades_port *info, struct tty_struct *tty)
{
	struct cyclades_card *card;
	unsigned long flags;
	int retval = 0;
	int channel;
	unsigned long page;

	card = info->card;
	channel = info->line - card->first_line;

	page = get_zeroed_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	spin_lock_irqsave(&card->card_lock, flags);

	if (info->port.flags & ASYNC_INITIALIZED)
		goto errout;

	if (!info->type) {
		set_bit(TTY_IO_ERROR, &tty->flags);
		goto errout;
	}

	if (info->port.xmit_buf)
		free_page(page);
	else
		info->port.xmit_buf = (unsigned char *)page;

	spin_unlock_irqrestore(&card->card_lock, flags);

	cy_set_line_char(info, tty);

	if (!cy_is_Z(card)) {
		channel &= 0x03;

		spin_lock_irqsave(&card->card_lock, flags);

		cyy_writeb(info, CyCAR, channel);

		cyy_writeb(info, CyRTPR,
			(info->default_timeout ? info->default_timeout : 0x02));
		

		cyy_issue_cmd(info, CyCHAN_CTL | CyENB_RCVR | CyENB_XMTR);

		cyy_change_rts_dtr(info, TIOCM_RTS | TIOCM_DTR, 0);

		cyy_writeb(info, CySRER, cyy_readb(info, CySRER) | CyRxData);
	} else {
		struct CH_CTRL __iomem *ch_ctrl = info->u.cyz.ch_ctrl;

		if (!cyz_is_loaded(card))
			return -ENODEV;

#ifdef CY_DEBUG_OPEN
		printk(KERN_DEBUG "cyc startup Z card %d, channel %d, "
			"base_addr %p\n", card, channel, card->base_addr);
#endif
		spin_lock_irqsave(&card->card_lock, flags);

		cy_writel(&ch_ctrl->op_mode, C_CH_ENABLE);
#ifdef Z_WAKE
#ifdef CONFIG_CYZ_INTR
		cy_writel(&ch_ctrl->intr_enable,
			  C_IN_TXBEMPTY | C_IN_TXLOWWM | C_IN_RXHIWM |
			  C_IN_RXNNDT | C_IN_IOCTLW | C_IN_MDCD);
#else
		cy_writel(&ch_ctrl->intr_enable,
			  C_IN_IOCTLW | C_IN_MDCD);
#endif				
#else
#ifdef CONFIG_CYZ_INTR
		cy_writel(&ch_ctrl->intr_enable,
			  C_IN_TXBEMPTY | C_IN_TXLOWWM | C_IN_RXHIWM |
			  C_IN_RXNNDT | C_IN_MDCD);
#else
		cy_writel(&ch_ctrl->intr_enable, C_IN_MDCD);
#endif				
#endif				

		retval = cyz_issue_cmd(card, channel, C_CM_IOCTL, 0L);
		if (retval != 0) {
			printk(KERN_ERR "cyc:startup(1) retval on ttyC%d was "
				"%x\n", info->line, retval);
		}

		
		retval = cyz_issue_cmd(card, channel, C_CM_FLUSH_RX, 0L);
		if (retval != 0) {
			printk(KERN_ERR "cyc:startup(2) retval on ttyC%d was "
				"%x\n", info->line, retval);
		}

		
		
		tty_port_raise_dtr_rts(&info->port);

		
	}

	info->port.flags |= ASYNC_INITIALIZED;

	clear_bit(TTY_IO_ERROR, &tty->flags);
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
	info->breakon = info->breakoff = 0;
	memset((char *)&info->idle_stats, 0, sizeof(info->idle_stats));
	info->idle_stats.in_use =
	info->idle_stats.recv_idle =
	info->idle_stats.xmit_idle = jiffies;

	spin_unlock_irqrestore(&card->card_lock, flags);

#ifdef CY_DEBUG_OPEN
	printk(KERN_DEBUG "cyc startup done\n");
#endif
	return 0;

errout:
	spin_unlock_irqrestore(&card->card_lock, flags);
	free_page(page);
	return retval;
}				

static void start_xmit(struct cyclades_port *info)
{
	struct cyclades_card *card = info->card;
	unsigned long flags;
	int channel = info->line - card->first_line;

	if (!cy_is_Z(card)) {
		spin_lock_irqsave(&card->card_lock, flags);
		cyy_writeb(info, CyCAR, channel & 0x03);
		cyy_writeb(info, CySRER, cyy_readb(info, CySRER) | CyTxRdy);
		spin_unlock_irqrestore(&card->card_lock, flags);
	} else {
#ifdef CONFIG_CYZ_INTR
		int retval;

		spin_lock_irqsave(&card->card_lock, flags);
		retval = cyz_issue_cmd(card, channel, C_CM_INTBACK, 0L);
		if (retval != 0) {
			printk(KERN_ERR "cyc:start_xmit retval on ttyC%d was "
				"%x\n", info->line, retval);
		}
		spin_unlock_irqrestore(&card->card_lock, flags);
#else				
		
#endif				
	}
}				

static void cy_shutdown(struct cyclades_port *info, struct tty_struct *tty)
{
	struct cyclades_card *card;
	unsigned long flags;

	if (!(info->port.flags & ASYNC_INITIALIZED))
		return;

	card = info->card;
	if (!cy_is_Z(card)) {
		spin_lock_irqsave(&card->card_lock, flags);

		
		wake_up_interruptible(&info->port.delta_msr_wait);

		if (info->port.xmit_buf) {
			unsigned char *temp;
			temp = info->port.xmit_buf;
			info->port.xmit_buf = NULL;
			free_page((unsigned long)temp);
		}
		if (tty->termios->c_cflag & HUPCL)
			cyy_change_rts_dtr(info, 0, TIOCM_RTS | TIOCM_DTR);

		cyy_issue_cmd(info, CyCHAN_CTL | CyDIS_RCVR);

		set_bit(TTY_IO_ERROR, &tty->flags);
		info->port.flags &= ~ASYNC_INITIALIZED;
		spin_unlock_irqrestore(&card->card_lock, flags);
	} else {
#ifdef CY_DEBUG_OPEN
		int channel = info->line - card->first_line;
		printk(KERN_DEBUG "cyc shutdown Z card %d, channel %d, "
			"base_addr %p\n", card, channel, card->base_addr);
#endif

		if (!cyz_is_loaded(card))
			return;

		spin_lock_irqsave(&card->card_lock, flags);

		if (info->port.xmit_buf) {
			unsigned char *temp;
			temp = info->port.xmit_buf;
			info->port.xmit_buf = NULL;
			free_page((unsigned long)temp);
		}

		if (tty->termios->c_cflag & HUPCL)
			tty_port_lower_dtr_rts(&info->port);

		set_bit(TTY_IO_ERROR, &tty->flags);
		info->port.flags &= ~ASYNC_INITIALIZED;

		spin_unlock_irqrestore(&card->card_lock, flags);
	}

#ifdef CY_DEBUG_OPEN
	printk(KERN_DEBUG "cyc shutdown done\n");
#endif
}				


static int cy_open(struct tty_struct *tty, struct file *filp)
{
	struct cyclades_port *info;
	unsigned int i, line = tty->index;
	int retval;

	for (i = 0; i < NR_CARDS; i++)
		if (line < cy_card[i].first_line + cy_card[i].nports &&
				line >= cy_card[i].first_line)
			break;
	if (i >= NR_CARDS)
		return -ENODEV;
	info = &cy_card[i].ports[line - cy_card[i].first_line];
	if (info->line < 0)
		return -ENODEV;

	if (cy_is_Z(info->card)) {
		struct cyclades_card *cinfo = info->card;
		struct FIRM_ID __iomem *firm_id = cinfo->base_addr + ID_ADDRESS;

		if (!cyz_is_loaded(cinfo)) {
			if (cinfo->hw_ver == ZE_V1 && cyz_fpga_loaded(cinfo) &&
					readl(&firm_id->signature) ==
					ZFIRM_HLT) {
				printk(KERN_ERR "cyc:Cyclades-Z Error: you "
					"need an external power supply for "
					"this number of ports.\nFirmware "
					"halted.\n");
			} else {
				printk(KERN_ERR "cyc:Cyclades-Z firmware not "
					"yet loaded\n");
			}
			return -ENODEV;
		}
#ifdef CONFIG_CYZ_INTR
		else {
			if (!cinfo->intr_enabled) {
				u16 intr;

				
				intr = readw(&cinfo->ctl_addr.p9060->
						intr_ctrl_stat) | 0x0900;
				cy_writew(&cinfo->ctl_addr.p9060->
						intr_ctrl_stat, intr);
				
				retval = cyz_issue_cmd(cinfo, 0,
						C_CM_IRQ_ENBL, 0L);
				if (retval != 0) {
					printk(KERN_ERR "cyc:IRQ enable retval "
						"was %x\n", retval);
				}
				cinfo->intr_enabled = 1;
			}
		}
#endif				
		
		if (info->line > (cinfo->first_line + cinfo->nports - 1))
			return -ENODEV;
	}
#ifdef CY_DEBUG_OTHER
	printk(KERN_DEBUG "cyc:cy_open ttyC%d\n", info->line);
#endif
	tty->driver_data = info;
	if (serial_paranoia_check(info, tty->name, "cy_open"))
		return -ENODEV;

#ifdef CY_DEBUG_OPEN
	printk(KERN_DEBUG "cyc:cy_open ttyC%d, count = %d\n", info->line,
			info->port.count);
#endif
	info->port.count++;
#ifdef CY_DEBUG_COUNT
	printk(KERN_DEBUG "cyc:cy_open (%d): incrementing count to %d\n",
		current->pid, info->port.count);
#endif

	if (tty_hung_up_p(filp) || (info->port.flags & ASYNC_CLOSING)) {
		wait_event_interruptible_tty(info->port.close_wait,
				!(info->port.flags & ASYNC_CLOSING));
		return (info->port.flags & ASYNC_HUP_NOTIFY) ? -EAGAIN: -ERESTARTSYS;
	}

	retval = cy_startup(info, tty);
	if (retval)
		return retval;

	retval = tty_port_block_til_ready(&info->port, tty, filp);
	if (retval) {
#ifdef CY_DEBUG_OPEN
		printk(KERN_DEBUG "cyc:cy_open returning after block_til_ready "
			"with %d\n", retval);
#endif
		return retval;
	}

	info->throttle = 0;
	tty_port_tty_set(&info->port, tty);

#ifdef CY_DEBUG_OPEN
	printk(KERN_DEBUG "cyc:cy_open done\n");
#endif
	return 0;
}				

static void cy_wait_until_sent(struct tty_struct *tty, int timeout)
{
	struct cyclades_card *card;
	struct cyclades_port *info = tty->driver_data;
	unsigned long orig_jiffies;
	int char_time;

	if (serial_paranoia_check(info, tty->name, "cy_wait_until_sent"))
		return;

	if (info->xmit_fifo_size == 0)
		return;		

	orig_jiffies = jiffies;
	char_time = (info->timeout - HZ / 50) / info->xmit_fifo_size;
	char_time = char_time / 5;
	if (char_time <= 0)
		char_time = 1;
	if (timeout < 0)
		timeout = 0;
	if (timeout)
		char_time = min(char_time, timeout);
	if (!timeout || timeout > 2 * info->timeout)
		timeout = 2 * info->timeout;

	card = info->card;
	if (!cy_is_Z(card)) {
		while (cyy_readb(info, CySRER) & CyTxRdy) {
			if (msleep_interruptible(jiffies_to_msecs(char_time)))
				break;
			if (timeout && time_after(jiffies, orig_jiffies +
					timeout))
				break;
		}
	}
	
	msleep_interruptible(jiffies_to_msecs(char_time * 5));
}

static void cy_flush_buffer(struct tty_struct *tty)
{
	struct cyclades_port *info = tty->driver_data;
	struct cyclades_card *card;
	int channel, retval;
	unsigned long flags;

#ifdef CY_DEBUG_IO
	printk(KERN_DEBUG "cyc:cy_flush_buffer ttyC%d\n", info->line);
#endif

	if (serial_paranoia_check(info, tty->name, "cy_flush_buffer"))
		return;

	card = info->card;
	channel = info->line - card->first_line;

	spin_lock_irqsave(&card->card_lock, flags);
	info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
	spin_unlock_irqrestore(&card->card_lock, flags);

	if (cy_is_Z(card)) {	
		spin_lock_irqsave(&card->card_lock, flags);
		retval = cyz_issue_cmd(card, channel, C_CM_FLUSH_TX, 0L);
		if (retval != 0) {
			printk(KERN_ERR "cyc: flush_buffer retval on ttyC%d "
				"was %x\n", info->line, retval);
		}
		spin_unlock_irqrestore(&card->card_lock, flags);
	}
	tty_wakeup(tty);
}				


static void cy_do_close(struct tty_port *port)
{
	struct cyclades_port *info = container_of(port, struct cyclades_port,
								port);
	struct cyclades_card *card;
	unsigned long flags;
	int channel;

	card = info->card;
	channel = info->line - card->first_line;
	spin_lock_irqsave(&card->card_lock, flags);

	if (!cy_is_Z(card)) {
		
		cyy_writeb(info, CyCAR, channel & 0x03);
		cyy_writeb(info, CySRER, cyy_readb(info, CySRER) & ~CyRxData);
		if (info->port.flags & ASYNC_INITIALIZED) {
			spin_unlock_irqrestore(&card->card_lock, flags);
			cy_wait_until_sent(port->tty, info->timeout);
			spin_lock_irqsave(&card->card_lock, flags);
		}
	} else {
#ifdef Z_WAKE
		struct CH_CTRL __iomem *ch_ctrl = info->u.cyz.ch_ctrl;
		int retval;

		if (readl(&ch_ctrl->flow_status) != C_FS_TXIDLE) {
			retval = cyz_issue_cmd(card, channel, C_CM_IOCTLW, 0L);
			if (retval != 0) {
				printk(KERN_DEBUG "cyc:cy_close retval on "
					"ttyC%d was %x\n", info->line, retval);
			}
			spin_unlock_irqrestore(&card->card_lock, flags);
			wait_for_completion_interruptible(&info->shutdown_wait);
			spin_lock_irqsave(&card->card_lock, flags);
		}
#endif
	}
	spin_unlock_irqrestore(&card->card_lock, flags);
	cy_shutdown(info, port->tty);
}

static void cy_close(struct tty_struct *tty, struct file *filp)
{
	struct cyclades_port *info = tty->driver_data;
	if (!info || serial_paranoia_check(info, tty->name, "cy_close"))
		return;
	tty_port_close(&info->port, tty, filp);
}				

static int cy_write(struct tty_struct *tty, const unsigned char *buf, int count)
{
	struct cyclades_port *info = tty->driver_data;
	unsigned long flags;
	int c, ret = 0;

#ifdef CY_DEBUG_IO
	printk(KERN_DEBUG "cyc:cy_write ttyC%d\n", info->line);
#endif

	if (serial_paranoia_check(info, tty->name, "cy_write"))
		return 0;

	if (!info->port.xmit_buf)
		return 0;

	spin_lock_irqsave(&info->card->card_lock, flags);
	while (1) {
		c = min(count, (int)(SERIAL_XMIT_SIZE - info->xmit_cnt - 1));
		c = min(c, (int)(SERIAL_XMIT_SIZE - info->xmit_head));

		if (c <= 0)
			break;

		memcpy(info->port.xmit_buf + info->xmit_head, buf, c);
		info->xmit_head = (info->xmit_head + c) &
			(SERIAL_XMIT_SIZE - 1);
		info->xmit_cnt += c;
		buf += c;
		count -= c;
		ret += c;
	}
	spin_unlock_irqrestore(&info->card->card_lock, flags);

	info->idle_stats.xmit_bytes += ret;
	info->idle_stats.xmit_idle = jiffies;

	if (info->xmit_cnt && !tty->stopped && !tty->hw_stopped)
		start_xmit(info);

	return ret;
}				

static int cy_put_char(struct tty_struct *tty, unsigned char ch)
{
	struct cyclades_port *info = tty->driver_data;
	unsigned long flags;

#ifdef CY_DEBUG_IO
	printk(KERN_DEBUG "cyc:cy_put_char ttyC%d\n", info->line);
#endif

	if (serial_paranoia_check(info, tty->name, "cy_put_char"))
		return 0;

	if (!info->port.xmit_buf)
		return 0;

	spin_lock_irqsave(&info->card->card_lock, flags);
	if (info->xmit_cnt >= (int)(SERIAL_XMIT_SIZE - 1)) {
		spin_unlock_irqrestore(&info->card->card_lock, flags);
		return 0;
	}

	info->port.xmit_buf[info->xmit_head++] = ch;
	info->xmit_head &= SERIAL_XMIT_SIZE - 1;
	info->xmit_cnt++;
	info->idle_stats.xmit_bytes++;
	info->idle_stats.xmit_idle = jiffies;
	spin_unlock_irqrestore(&info->card->card_lock, flags);
	return 1;
}				

/*
 * This routine is called by the kernel after it has written a
 * series of characters to the tty device using put_char().
 */
static void cy_flush_chars(struct tty_struct *tty)
{
	struct cyclades_port *info = tty->driver_data;

#ifdef CY_DEBUG_IO
	printk(KERN_DEBUG "cyc:cy_flush_chars ttyC%d\n", info->line);
#endif

	if (serial_paranoia_check(info, tty->name, "cy_flush_chars"))
		return;

	if (info->xmit_cnt <= 0 || tty->stopped || tty->hw_stopped ||
			!info->port.xmit_buf)
		return;

	start_xmit(info);
}				

/*
 * This routine returns the numbers of characters the tty driver
 * will accept for queuing to be written.  This number is subject
 * to change as output buffers get emptied, or if the output flow
 * control is activated.
 */
static int cy_write_room(struct tty_struct *tty)
{
	struct cyclades_port *info = tty->driver_data;
	int ret;

#ifdef CY_DEBUG_IO
	printk(KERN_DEBUG "cyc:cy_write_room ttyC%d\n", info->line);
#endif

	if (serial_paranoia_check(info, tty->name, "cy_write_room"))
		return 0;
	ret = SERIAL_XMIT_SIZE - info->xmit_cnt - 1;
	if (ret < 0)
		ret = 0;
	return ret;
}				

static int cy_chars_in_buffer(struct tty_struct *tty)
{
	struct cyclades_port *info = tty->driver_data;

	if (serial_paranoia_check(info, tty->name, "cy_chars_in_buffer"))
		return 0;

#ifdef Z_EXT_CHARS_IN_BUFFER
	if (!cy_is_Z(info->card)) {
#endif				
#ifdef CY_DEBUG_IO
		printk(KERN_DEBUG "cyc:cy_chars_in_buffer ttyC%d %d\n",
			info->line, info->xmit_cnt);
#endif
		return info->xmit_cnt;
#ifdef Z_EXT_CHARS_IN_BUFFER
	} else {
		struct BUF_CTRL __iomem *buf_ctrl = info->u.cyz.buf_ctrl;
		int char_count;
		__u32 tx_put, tx_get, tx_bufsize;

		tx_get = readl(&buf_ctrl->tx_get);
		tx_put = readl(&buf_ctrl->tx_put);
		tx_bufsize = readl(&buf_ctrl->tx_bufsize);
		if (tx_put >= tx_get)
			char_count = tx_put - tx_get;
		else
			char_count = tx_put - tx_get + tx_bufsize;
#ifdef CY_DEBUG_IO
		printk(KERN_DEBUG "cyc:cy_chars_in_buffer ttyC%d %d\n",
			info->line, info->xmit_cnt + char_count);
#endif
		return info->xmit_cnt + char_count;
	}
#endif				
}				


static void cyy_baud_calc(struct cyclades_port *info, __u32 baud)
{
	int co, co_val, bpr;
	__u32 cy_clock = ((info->chip_rev >= CD1400_REV_J) ? 60000000 :
			25000000);

	if (baud == 0) {
		info->tbpr = info->tco = info->rbpr = info->rco = 0;
		return;
	}

	
	for (co = 4, co_val = 2048; co; co--, co_val >>= 2) {
		if (cy_clock / co_val / baud > 63)
			break;
	}

	bpr = (cy_clock / co_val * 2 / baud + 1) / 2;
	if (bpr > 255)
		bpr = 255;

	info->tbpr = info->rbpr = bpr;
	info->tco = info->rco = co;
}

static void cy_set_line_char(struct cyclades_port *info, struct tty_struct *tty)
{
	struct cyclades_card *card;
	unsigned long flags;
	int channel;
	unsigned cflag, iflag;
	int baud, baud_rate = 0;
	int i;

	if (!tty->termios) 
		return;

	if (info->line == -1)
		return;

	cflag = tty->termios->c_cflag;
	iflag = tty->termios->c_iflag;

	if ((info->port.flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
		tty->alt_speed = 57600;
	if ((info->port.flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
		tty->alt_speed = 115200;
	if ((info->port.flags & ASYNC_SPD_MASK) == ASYNC_SPD_SHI)
		tty->alt_speed = 230400;
	if ((info->port.flags & ASYNC_SPD_MASK) == ASYNC_SPD_WARP)
		tty->alt_speed = 460800;

	card = info->card;
	channel = info->line - card->first_line;

	if (!cy_is_Z(card)) {
		u32 cflags;

		
		baud = tty_get_baud_rate(tty);
		if (baud == 38400 && (info->port.flags & ASYNC_SPD_MASK) ==
				ASYNC_SPD_CUST) {
			if (info->custom_divisor)
				baud_rate = info->baud / info->custom_divisor;
			else
				baud_rate = info->baud;
		} else if (baud > CD1400_MAX_SPEED) {
			baud = CD1400_MAX_SPEED;
		}
		
		for (i = 0; i < 20; i++) {
			if (baud == baud_table[i])
				break;
		}
		if (i == 20)
			i = 19;	

		if (baud == 38400 && (info->port.flags & ASYNC_SPD_MASK) ==
				ASYNC_SPD_CUST) {
			cyy_baud_calc(info, baud_rate);
		} else {
			if (info->chip_rev >= CD1400_REV_J) {
				
				info->tbpr = baud_bpr_60[i];	
				info->tco = baud_co_60[i];	
				info->rbpr = baud_bpr_60[i];	
				info->rco = baud_co_60[i];	
			} else {
				info->tbpr = baud_bpr_25[i];	
				info->tco = baud_co_25[i];	
				info->rbpr = baud_bpr_25[i];	
				info->rco = baud_co_25[i];	
			}
		}
		if (baud_table[i] == 134) {
			
			info->timeout = (info->xmit_fifo_size * HZ * 30 / 269) +
					2;
		} else if (baud == 38400 && (info->port.flags & ASYNC_SPD_MASK) ==
				ASYNC_SPD_CUST) {
			info->timeout = (info->xmit_fifo_size * HZ * 15 /
					baud_rate) + 2;
		} else if (baud_table[i]) {
			info->timeout = (info->xmit_fifo_size * HZ * 15 /
					baud_table[i]) + 2;
			
		} else {
			info->timeout = 0;
		}

		
		info->cor5 = 0;
		info->cor4 = 0;
		
		info->cor3 = (info->default_threshold ?
				info->default_threshold : baud_cor3[i]);
		info->cor2 = CyETC;
		switch (cflag & CSIZE) {
		case CS5:
			info->cor1 = Cy_5_BITS;
			break;
		case CS6:
			info->cor1 = Cy_6_BITS;
			break;
		case CS7:
			info->cor1 = Cy_7_BITS;
			break;
		case CS8:
			info->cor1 = Cy_8_BITS;
			break;
		}
		if (cflag & CSTOPB)
			info->cor1 |= Cy_2_STOP;

		if (cflag & PARENB) {
			if (cflag & PARODD)
				info->cor1 |= CyPARITY_O;
			else
				info->cor1 |= CyPARITY_E;
		} else
			info->cor1 |= CyPARITY_NONE;

		
		if (cflag & CRTSCTS) {
			info->port.flags |= ASYNC_CTS_FLOW;
			info->cor2 |= CyCtsAE;
		} else {
			info->port.flags &= ~ASYNC_CTS_FLOW;
			info->cor2 &= ~CyCtsAE;
		}
		if (cflag & CLOCAL)
			info->port.flags &= ~ASYNC_CHECK_CD;
		else
			info->port.flags |= ASYNC_CHECK_CD;


		channel &= 0x03;

		spin_lock_irqsave(&card->card_lock, flags);
		cyy_writeb(info, CyCAR, channel);

		

		cyy_writeb(info, CyTCOR, info->tco);
		cyy_writeb(info, CyTBPR, info->tbpr);
		cyy_writeb(info, CyRCOR, info->rco);
		cyy_writeb(info, CyRBPR, info->rbpr);

		

		cyy_writeb(info, CySCHR1, START_CHAR(tty));
		cyy_writeb(info, CySCHR2, STOP_CHAR(tty));
		cyy_writeb(info, CyCOR1, info->cor1);
		cyy_writeb(info, CyCOR2, info->cor2);
		cyy_writeb(info, CyCOR3, info->cor3);
		cyy_writeb(info, CyCOR4, info->cor4);
		cyy_writeb(info, CyCOR5, info->cor5);

		cyy_issue_cmd(info, CyCOR_CHANGE | CyCOR1ch | CyCOR2ch |
				CyCOR3ch);

		
		cyy_writeb(info, CyCAR, channel);
		cyy_writeb(info, CyRTPR,
			(info->default_timeout ? info->default_timeout : 0x02));
		

		cflags = CyCTS;
		if (!C_CLOCAL(tty))
			cflags |= CyDSR | CyRI | CyDCD;
		
		cyy_writeb(info, CySRER, cyy_readb(info, CySRER) | CyMdmCh);
		
		if ((cflag & CRTSCTS) && info->rflow)
			cyy_writeb(info, CyMCOR1, cflags | rflow_thr[i]);
		else
			cyy_writeb(info, CyMCOR1, cflags);
		
		cyy_writeb(info, CyMCOR2, cflags);

		if (i == 0)	
			cyy_change_rts_dtr(info, 0, TIOCM_DTR);
		else
			cyy_change_rts_dtr(info, TIOCM_DTR, 0);

		clear_bit(TTY_IO_ERROR, &tty->flags);
		spin_unlock_irqrestore(&card->card_lock, flags);

	} else {
		struct CH_CTRL __iomem *ch_ctrl = info->u.cyz.ch_ctrl;
		__u32 sw_flow;
		int retval;

		if (!cyz_is_loaded(card))
			return;

		
		baud = tty_get_baud_rate(tty);
		if (baud == 38400 && (info->port.flags & ASYNC_SPD_MASK) ==
				ASYNC_SPD_CUST) {
			if (info->custom_divisor)
				baud_rate = info->baud / info->custom_divisor;
			else
				baud_rate = info->baud;
		} else if (baud > CYZ_MAX_SPEED) {
			baud = CYZ_MAX_SPEED;
		}
		cy_writel(&ch_ctrl->comm_baud, baud);

		if (baud == 134) {
			
			info->timeout = (info->xmit_fifo_size * HZ * 30 / 269) +
					2;
		} else if (baud == 38400 && (info->port.flags & ASYNC_SPD_MASK) ==
				ASYNC_SPD_CUST) {
			info->timeout = (info->xmit_fifo_size * HZ * 15 /
					baud_rate) + 2;
		} else if (baud) {
			info->timeout = (info->xmit_fifo_size * HZ * 15 /
					baud) + 2;
			
		} else {
			info->timeout = 0;
		}

		
		switch (cflag & CSIZE) {
		case CS5:
			cy_writel(&ch_ctrl->comm_data_l, C_DL_CS5);
			break;
		case CS6:
			cy_writel(&ch_ctrl->comm_data_l, C_DL_CS6);
			break;
		case CS7:
			cy_writel(&ch_ctrl->comm_data_l, C_DL_CS7);
			break;
		case CS8:
			cy_writel(&ch_ctrl->comm_data_l, C_DL_CS8);
			break;
		}
		if (cflag & CSTOPB) {
			cy_writel(&ch_ctrl->comm_data_l,
				  readl(&ch_ctrl->comm_data_l) | C_DL_2STOP);
		} else {
			cy_writel(&ch_ctrl->comm_data_l,
				  readl(&ch_ctrl->comm_data_l) | C_DL_1STOP);
		}
		if (cflag & PARENB) {
			if (cflag & PARODD)
				cy_writel(&ch_ctrl->comm_parity, C_PR_ODD);
			else
				cy_writel(&ch_ctrl->comm_parity, C_PR_EVEN);
		} else
			cy_writel(&ch_ctrl->comm_parity, C_PR_NONE);

		
		if (cflag & CRTSCTS) {
			cy_writel(&ch_ctrl->hw_flow,
				readl(&ch_ctrl->hw_flow) | C_RS_CTS | C_RS_RTS);
		} else {
			cy_writel(&ch_ctrl->hw_flow, readl(&ch_ctrl->hw_flow) &
					~(C_RS_CTS | C_RS_RTS));
		}
		info->port.flags &= ~ASYNC_CTS_FLOW;

		
		sw_flow = 0;
		if (iflag & IXON) {
			sw_flow |= C_FL_OXX;
			if (iflag & IXANY)
				sw_flow |= C_FL_OIXANY;
		}
		cy_writel(&ch_ctrl->sw_flow, sw_flow);

		retval = cyz_issue_cmd(card, channel, C_CM_IOCTL, 0L);
		if (retval != 0) {
			printk(KERN_ERR "cyc:set_line_char retval on ttyC%d "
				"was %x\n", info->line, retval);
		}

		
		if (cflag & CLOCAL)
			info->port.flags &= ~ASYNC_CHECK_CD;
		else
			info->port.flags |= ASYNC_CHECK_CD;

		if (baud == 0) {	
			cy_writel(&ch_ctrl->rs_control,
				  readl(&ch_ctrl->rs_control) & ~C_RS_DTR);
#ifdef CY_DEBUG_DTR
			printk(KERN_DEBUG "cyc:set_line_char dropping Z DTR\n");
#endif
		} else {
			cy_writel(&ch_ctrl->rs_control,
				  readl(&ch_ctrl->rs_control) | C_RS_DTR);
#ifdef CY_DEBUG_DTR
			printk(KERN_DEBUG "cyc:set_line_char raising Z DTR\n");
#endif
		}

		retval = cyz_issue_cmd(card, channel, C_CM_IOCTLM, 0L);
		if (retval != 0) {
			printk(KERN_ERR "cyc:set_line_char(2) retval on ttyC%d "
				"was %x\n", info->line, retval);
		}

		clear_bit(TTY_IO_ERROR, &tty->flags);
	}
}				

static int cy_get_serial_info(struct cyclades_port *info,
		struct serial_struct __user *retinfo)
{
	struct cyclades_card *cinfo = info->card;
	struct serial_struct tmp = {
		.type = info->type,
		.line = info->line,
		.port = (info->card - cy_card) * 0x100 + info->line -
			cinfo->first_line,
		.irq = cinfo->irq,
		.flags = info->port.flags,
		.close_delay = info->port.close_delay,
		.closing_wait = info->port.closing_wait,
		.baud_base = info->baud,
		.custom_divisor = info->custom_divisor,
		.hub6 = 0,		
	};
	return copy_to_user(retinfo, &tmp, sizeof(*retinfo)) ? -EFAULT : 0;
}

static int
cy_set_serial_info(struct cyclades_port *info, struct tty_struct *tty,
		struct serial_struct __user *new_info)
{
	struct serial_struct new_serial;
	int ret;

	if (copy_from_user(&new_serial, new_info, sizeof(new_serial)))
		return -EFAULT;

	mutex_lock(&info->port.mutex);
	if (!capable(CAP_SYS_ADMIN)) {
		if (new_serial.close_delay != info->port.close_delay ||
				new_serial.baud_base != info->baud ||
				(new_serial.flags & ASYNC_FLAGS &
					~ASYNC_USR_MASK) !=
				(info->port.flags & ASYNC_FLAGS & ~ASYNC_USR_MASK))
		{
			mutex_unlock(&info->port.mutex);
			return -EPERM;
		}
		info->port.flags = (info->port.flags & ~ASYNC_USR_MASK) |
				(new_serial.flags & ASYNC_USR_MASK);
		info->baud = new_serial.baud_base;
		info->custom_divisor = new_serial.custom_divisor;
		goto check_and_exit;
	}


	info->baud = new_serial.baud_base;
	info->custom_divisor = new_serial.custom_divisor;
	info->port.flags = (info->port.flags & ~ASYNC_FLAGS) |
			(new_serial.flags & ASYNC_FLAGS);
	info->port.close_delay = new_serial.close_delay * HZ / 100;
	info->port.closing_wait = new_serial.closing_wait * HZ / 100;

check_and_exit:
	if (info->port.flags & ASYNC_INITIALIZED) {
		cy_set_line_char(info, tty);
		ret = 0;
	} else {
		ret = cy_startup(info, tty);
	}
	mutex_unlock(&info->port.mutex);
	return ret;
}				

/*
 * get_lsr_info - get line status register info
 *
 * Purpose: Let user call ioctl() to get info when the UART physically
 *	    is emptied.  On bus types like RS485, the transmitter must
 *	    release the bus after transmitting. This must be done when
 *	    the transmit shift register is empty, not be done when the
 *	    transmit holding register is empty.  This functionality
 *	    allows an RS485 driver to be written in user space.
 */
static int get_lsr_info(struct cyclades_port *info, unsigned int __user *value)
{
	struct cyclades_card *card = info->card;
	unsigned int result;
	unsigned long flags;
	u8 status;

	if (!cy_is_Z(card)) {
		spin_lock_irqsave(&card->card_lock, flags);
		status = cyy_readb(info, CySRER) & (CyTxRdy | CyTxMpty);
		spin_unlock_irqrestore(&card->card_lock, flags);
		result = (status ? 0 : TIOCSER_TEMT);
	} else {
		
		return -EINVAL;
	}
	return put_user(result, value);
}

static int cy_tiocmget(struct tty_struct *tty)
{
	struct cyclades_port *info = tty->driver_data;
	struct cyclades_card *card;
	int result;

	if (serial_paranoia_check(info, tty->name, __func__))
		return -ENODEV;

	card = info->card;

	if (!cy_is_Z(card)) {
		unsigned long flags;
		int channel = info->line - card->first_line;
		u8 status;

		spin_lock_irqsave(&card->card_lock, flags);
		cyy_writeb(info, CyCAR, channel & 0x03);
		status = cyy_readb(info, CyMSVR1);
		status |= cyy_readb(info, CyMSVR2);
		spin_unlock_irqrestore(&card->card_lock, flags);

		if (info->rtsdtr_inv) {
			result = ((status & CyRTS) ? TIOCM_DTR : 0) |
				((status & CyDTR) ? TIOCM_RTS : 0);
		} else {
			result = ((status & CyRTS) ? TIOCM_RTS : 0) |
				((status & CyDTR) ? TIOCM_DTR : 0);
		}
		result |= ((status & CyDCD) ? TIOCM_CAR : 0) |
			((status & CyRI) ? TIOCM_RNG : 0) |
			((status & CyDSR) ? TIOCM_DSR : 0) |
			((status & CyCTS) ? TIOCM_CTS : 0);
	} else {
		u32 lstatus;

		if (!cyz_is_loaded(card)) {
			result = -ENODEV;
			goto end;
		}

		lstatus = readl(&info->u.cyz.ch_ctrl->rs_status);
		result = ((lstatus & C_RS_RTS) ? TIOCM_RTS : 0) |
			((lstatus & C_RS_DTR) ? TIOCM_DTR : 0) |
			((lstatus & C_RS_DCD) ? TIOCM_CAR : 0) |
			((lstatus & C_RS_RI) ? TIOCM_RNG : 0) |
			((lstatus & C_RS_DSR) ? TIOCM_DSR : 0) |
			((lstatus & C_RS_CTS) ? TIOCM_CTS : 0);
	}
end:
	return result;
}				

static int
cy_tiocmset(struct tty_struct *tty,
		unsigned int set, unsigned int clear)
{
	struct cyclades_port *info = tty->driver_data;
	struct cyclades_card *card;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->name, __func__))
		return -ENODEV;

	card = info->card;
	if (!cy_is_Z(card)) {
		spin_lock_irqsave(&card->card_lock, flags);
		cyy_change_rts_dtr(info, set, clear);
		spin_unlock_irqrestore(&card->card_lock, flags);
	} else {
		struct CH_CTRL __iomem *ch_ctrl = info->u.cyz.ch_ctrl;
		int retval, channel = info->line - card->first_line;
		u32 rs;

		if (!cyz_is_loaded(card))
			return -ENODEV;

		spin_lock_irqsave(&card->card_lock, flags);
		rs = readl(&ch_ctrl->rs_control);
		if (set & TIOCM_RTS)
			rs |= C_RS_RTS;
		if (clear & TIOCM_RTS)
			rs &= ~C_RS_RTS;
		if (set & TIOCM_DTR) {
			rs |= C_RS_DTR;
#ifdef CY_DEBUG_DTR
			printk(KERN_DEBUG "cyc:set_modem_info raising Z DTR\n");
#endif
		}
		if (clear & TIOCM_DTR) {
			rs &= ~C_RS_DTR;
#ifdef CY_DEBUG_DTR
			printk(KERN_DEBUG "cyc:set_modem_info clearing "
				"Z DTR\n");
#endif
		}
		cy_writel(&ch_ctrl->rs_control, rs);
		retval = cyz_issue_cmd(card, channel, C_CM_IOCTLM, 0L);
		spin_unlock_irqrestore(&card->card_lock, flags);
		if (retval != 0) {
			printk(KERN_ERR "cyc:set_modem_info retval on ttyC%d "
				"was %x\n", info->line, retval);
		}
	}
	return 0;
}

static int cy_break(struct tty_struct *tty, int break_state)
{
	struct cyclades_port *info = tty->driver_data;
	struct cyclades_card *card;
	unsigned long flags;
	int retval = 0;

	if (serial_paranoia_check(info, tty->name, "cy_break"))
		return -EINVAL;

	card = info->card;

	spin_lock_irqsave(&card->card_lock, flags);
	if (!cy_is_Z(card)) {
		if (break_state == -1) {
			if (!info->breakon) {
				info->breakon = 1;
				if (!info->xmit_cnt) {
					spin_unlock_irqrestore(&card->card_lock, flags);
					start_xmit(info);
					spin_lock_irqsave(&card->card_lock, flags);
				}
			}
		} else {
			if (!info->breakoff) {
				info->breakoff = 1;
				if (!info->xmit_cnt) {
					spin_unlock_irqrestore(&card->card_lock, flags);
					start_xmit(info);
					spin_lock_irqsave(&card->card_lock, flags);
				}
			}
		}
	} else {
		if (break_state == -1) {
			retval = cyz_issue_cmd(card,
				info->line - card->first_line,
				C_CM_SET_BREAK, 0L);
			if (retval != 0) {
				printk(KERN_ERR "cyc:cy_break (set) retval on "
					"ttyC%d was %x\n", info->line, retval);
			}
		} else {
			retval = cyz_issue_cmd(card,
				info->line - card->first_line,
				C_CM_CLR_BREAK, 0L);
			if (retval != 0) {
				printk(KERN_DEBUG "cyc:cy_break (clr) retval "
					"on ttyC%d was %x\n", info->line,
					retval);
			}
		}
	}
	spin_unlock_irqrestore(&card->card_lock, flags);
	return retval;
}				

static int set_threshold(struct cyclades_port *info, unsigned long value)
{
	struct cyclades_card *card = info->card;
	unsigned long flags;

	if (!cy_is_Z(card)) {
		info->cor3 &= ~CyREC_FIFO;
		info->cor3 |= value & CyREC_FIFO;

		spin_lock_irqsave(&card->card_lock, flags);
		cyy_writeb(info, CyCOR3, info->cor3);
		cyy_issue_cmd(info, CyCOR_CHANGE | CyCOR3ch);
		spin_unlock_irqrestore(&card->card_lock, flags);
	}
	return 0;
}				

static int get_threshold(struct cyclades_port *info,
						unsigned long __user *value)
{
	struct cyclades_card *card = info->card;

	if (!cy_is_Z(card)) {
		u8 tmp = cyy_readb(info, CyCOR3) & CyREC_FIFO;
		return put_user(tmp, value);
	}
	return 0;
}				

static int set_timeout(struct cyclades_port *info, unsigned long value)
{
	struct cyclades_card *card = info->card;
	unsigned long flags;

	if (!cy_is_Z(card)) {
		spin_lock_irqsave(&card->card_lock, flags);
		cyy_writeb(info, CyRTPR, value & 0xff);
		spin_unlock_irqrestore(&card->card_lock, flags);
	}
	return 0;
}				

static int get_timeout(struct cyclades_port *info,
						unsigned long __user *value)
{
	struct cyclades_card *card = info->card;

	if (!cy_is_Z(card)) {
		u8 tmp = cyy_readb(info, CyRTPR);
		return put_user(tmp, value);
	}
	return 0;
}				

static int cy_cflags_changed(struct cyclades_port *info, unsigned long arg,
		struct cyclades_icount *cprev)
{
	struct cyclades_icount cnow;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&info->card->card_lock, flags);
	cnow = info->icount;	
	spin_unlock_irqrestore(&info->card->card_lock, flags);

	ret =	((arg & TIOCM_RNG) && (cnow.rng != cprev->rng)) ||
		((arg & TIOCM_DSR) && (cnow.dsr != cprev->dsr)) ||
		((arg & TIOCM_CD)  && (cnow.dcd != cprev->dcd)) ||
		((arg & TIOCM_CTS) && (cnow.cts != cprev->cts));

	*cprev = cnow;

	return ret;
}

static int
cy_ioctl(struct tty_struct *tty,
	 unsigned int cmd, unsigned long arg)
{
	struct cyclades_port *info = tty->driver_data;
	struct cyclades_icount cnow;	
	int ret_val = 0;
	unsigned long flags;
	void __user *argp = (void __user *)arg;

	if (serial_paranoia_check(info, tty->name, "cy_ioctl"))
		return -ENODEV;

#ifdef CY_DEBUG_OTHER
	printk(KERN_DEBUG "cyc:cy_ioctl ttyC%d, cmd = %x arg = %lx\n",
		info->line, cmd, arg);
#endif

	switch (cmd) {
	case CYGETMON:
		if (copy_to_user(argp, &info->mon, sizeof(info->mon))) {
			ret_val = -EFAULT;
			break;
		}
		memset(&info->mon, 0, sizeof(info->mon));
		break;
	case CYGETTHRESH:
		ret_val = get_threshold(info, argp);
		break;
	case CYSETTHRESH:
		ret_val = set_threshold(info, arg);
		break;
	case CYGETDEFTHRESH:
		ret_val = put_user(info->default_threshold,
				(unsigned long __user *)argp);
		break;
	case CYSETDEFTHRESH:
		info->default_threshold = arg & 0x0f;
		break;
	case CYGETTIMEOUT:
		ret_val = get_timeout(info, argp);
		break;
	case CYSETTIMEOUT:
		ret_val = set_timeout(info, arg);
		break;
	case CYGETDEFTIMEOUT:
		ret_val = put_user(info->default_timeout,
				(unsigned long __user *)argp);
		break;
	case CYSETDEFTIMEOUT:
		info->default_timeout = arg & 0xff;
		break;
	case CYSETRFLOW:
		info->rflow = (int)arg;
		break;
	case CYGETRFLOW:
		ret_val = info->rflow;
		break;
	case CYSETRTSDTR_INV:
		info->rtsdtr_inv = (int)arg;
		break;
	case CYGETRTSDTR_INV:
		ret_val = info->rtsdtr_inv;
		break;
	case CYGETCD1400VER:
		ret_val = info->chip_rev;
		break;
#ifndef CONFIG_CYZ_INTR
	case CYZSETPOLLCYCLE:
		cyz_polling_cycle = (arg * HZ) / 1000;
		break;
	case CYZGETPOLLCYCLE:
		ret_val = (cyz_polling_cycle * 1000) / HZ;
		break;
#endif				
	case CYSETWAIT:
		info->port.closing_wait = (unsigned short)arg * HZ / 100;
		break;
	case CYGETWAIT:
		ret_val = info->port.closing_wait / (HZ / 100);
		break;
	case TIOCGSERIAL:
		ret_val = cy_get_serial_info(info, argp);
		break;
	case TIOCSSERIAL:
		ret_val = cy_set_serial_info(info, tty, argp);
		break;
	case TIOCSERGETLSR:	
		ret_val = get_lsr_info(info, argp);
		break;
	case TIOCMIWAIT:
		spin_lock_irqsave(&info->card->card_lock, flags);
		
		cnow = info->icount;
		spin_unlock_irqrestore(&info->card->card_lock, flags);
		ret_val = wait_event_interruptible(info->port.delta_msr_wait,
				cy_cflags_changed(info, arg, &cnow));
		break;

	default:
		ret_val = -ENOIOCTLCMD;
	}

#ifdef CY_DEBUG_OTHER
	printk(KERN_DEBUG "cyc:cy_ioctl done\n");
#endif
	return ret_val;
}				

static int cy_get_icount(struct tty_struct *tty,
				struct serial_icounter_struct *sic)
{
	struct cyclades_port *info = tty->driver_data;
	struct cyclades_icount cnow;	
	unsigned long flags;

	spin_lock_irqsave(&info->card->card_lock, flags);
	cnow = info->icount;
	spin_unlock_irqrestore(&info->card->card_lock, flags);

	sic->cts = cnow.cts;
	sic->dsr = cnow.dsr;
	sic->rng = cnow.rng;
	sic->dcd = cnow.dcd;
	sic->rx = cnow.rx;
	sic->tx = cnow.tx;
	sic->frame = cnow.frame;
	sic->overrun = cnow.overrun;
	sic->parity = cnow.parity;
	sic->brk = cnow.brk;
	sic->buf_overrun = cnow.buf_overrun;
	return 0;
}

static void cy_set_termios(struct tty_struct *tty, struct ktermios *old_termios)
{
	struct cyclades_port *info = tty->driver_data;

#ifdef CY_DEBUG_OTHER
	printk(KERN_DEBUG "cyc:cy_set_termios ttyC%d\n", info->line);
#endif

	cy_set_line_char(info, tty);

	if ((old_termios->c_cflag & CRTSCTS) &&
			!(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		cy_start(tty);
	}
#if 0
	if (!(old_termios->c_cflag & CLOCAL) &&
	    (tty->termios->c_cflag & CLOCAL))
		wake_up_interruptible(&info->port.open_wait);
#endif
}				

static void cy_send_xchar(struct tty_struct *tty, char ch)
{
	struct cyclades_port *info = tty->driver_data;
	struct cyclades_card *card;
	int channel;

	if (serial_paranoia_check(info, tty->name, "cy_send_xchar"))
		return;

	info->x_char = ch;

	if (ch)
		cy_start(tty);

	card = info->card;
	channel = info->line - card->first_line;

	if (cy_is_Z(card)) {
		if (ch == STOP_CHAR(tty))
			cyz_issue_cmd(card, channel, C_CM_SENDXOFF, 0L);
		else if (ch == START_CHAR(tty))
			cyz_issue_cmd(card, channel, C_CM_SENDXON, 0L);
	}
}

static void cy_throttle(struct tty_struct *tty)
{
	struct cyclades_port *info = tty->driver_data;
	struct cyclades_card *card;
	unsigned long flags;

#ifdef CY_DEBUG_THROTTLE
	char buf[64];

	printk(KERN_DEBUG "cyc:throttle %s: %ld...ttyC%d\n", tty_name(tty, buf),
			tty->ldisc.chars_in_buffer(tty), info->line);
#endif

	if (serial_paranoia_check(info, tty->name, "cy_throttle"))
		return;

	card = info->card;

	if (I_IXOFF(tty)) {
		if (!cy_is_Z(card))
			cy_send_xchar(tty, STOP_CHAR(tty));
		else
			info->throttle = 1;
	}

	if (tty->termios->c_cflag & CRTSCTS) {
		if (!cy_is_Z(card)) {
			spin_lock_irqsave(&card->card_lock, flags);
			cyy_change_rts_dtr(info, 0, TIOCM_RTS);
			spin_unlock_irqrestore(&card->card_lock, flags);
		} else {
			info->throttle = 1;
		}
	}
}				

static void cy_unthrottle(struct tty_struct *tty)
{
	struct cyclades_port *info = tty->driver_data;
	struct cyclades_card *card;
	unsigned long flags;

#ifdef CY_DEBUG_THROTTLE
	char buf[64];

	printk(KERN_DEBUG "cyc:unthrottle %s: %ld...ttyC%d\n",
		tty_name(tty, buf), tty_chars_in_buffer(tty), info->line);
#endif

	if (serial_paranoia_check(info, tty->name, "cy_unthrottle"))
		return;

	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else
			cy_send_xchar(tty, START_CHAR(tty));
	}

	if (tty->termios->c_cflag & CRTSCTS) {
		card = info->card;
		if (!cy_is_Z(card)) {
			spin_lock_irqsave(&card->card_lock, flags);
			cyy_change_rts_dtr(info, TIOCM_RTS, 0);
			spin_unlock_irqrestore(&card->card_lock, flags);
		} else {
			info->throttle = 0;
		}
	}
}				

static void cy_stop(struct tty_struct *tty)
{
	struct cyclades_card *cinfo;
	struct cyclades_port *info = tty->driver_data;
	int channel;
	unsigned long flags;

#ifdef CY_DEBUG_OTHER
	printk(KERN_DEBUG "cyc:cy_stop ttyC%d\n", info->line);
#endif

	if (serial_paranoia_check(info, tty->name, "cy_stop"))
		return;

	cinfo = info->card;
	channel = info->line - cinfo->first_line;
	if (!cy_is_Z(cinfo)) {
		spin_lock_irqsave(&cinfo->card_lock, flags);
		cyy_writeb(info, CyCAR, channel & 0x03);
		cyy_writeb(info, CySRER, cyy_readb(info, CySRER) & ~CyTxRdy);
		spin_unlock_irqrestore(&cinfo->card_lock, flags);
	}
}				

static void cy_start(struct tty_struct *tty)
{
	struct cyclades_card *cinfo;
	struct cyclades_port *info = tty->driver_data;
	int channel;
	unsigned long flags;

#ifdef CY_DEBUG_OTHER
	printk(KERN_DEBUG "cyc:cy_start ttyC%d\n", info->line);
#endif

	if (serial_paranoia_check(info, tty->name, "cy_start"))
		return;

	cinfo = info->card;
	channel = info->line - cinfo->first_line;
	if (!cy_is_Z(cinfo)) {
		spin_lock_irqsave(&cinfo->card_lock, flags);
		cyy_writeb(info, CyCAR, channel & 0x03);
		cyy_writeb(info, CySRER, cyy_readb(info, CySRER) | CyTxRdy);
		spin_unlock_irqrestore(&cinfo->card_lock, flags);
	}
}				

static void cy_hangup(struct tty_struct *tty)
{
	struct cyclades_port *info = tty->driver_data;

#ifdef CY_DEBUG_OTHER
	printk(KERN_DEBUG "cyc:cy_hangup ttyC%d\n", info->line);
#endif

	if (serial_paranoia_check(info, tty->name, "cy_hangup"))
		return;

	cy_flush_buffer(tty);
	cy_shutdown(info, tty);
	tty_port_hangup(&info->port);
}				

static int cyy_carrier_raised(struct tty_port *port)
{
	struct cyclades_port *info = container_of(port, struct cyclades_port,
			port);
	struct cyclades_card *cinfo = info->card;
	unsigned long flags;
	int channel = info->line - cinfo->first_line;
	u32 cd;

	spin_lock_irqsave(&cinfo->card_lock, flags);
	cyy_writeb(info, CyCAR, channel & 0x03);
	cd = cyy_readb(info, CyMSVR1) & CyDCD;
	spin_unlock_irqrestore(&cinfo->card_lock, flags);

	return cd;
}

static void cyy_dtr_rts(struct tty_port *port, int raise)
{
	struct cyclades_port *info = container_of(port, struct cyclades_port,
			port);
	struct cyclades_card *cinfo = info->card;
	unsigned long flags;

	spin_lock_irqsave(&cinfo->card_lock, flags);
	cyy_change_rts_dtr(info, raise ? TIOCM_RTS | TIOCM_DTR : 0,
			raise ? 0 : TIOCM_RTS | TIOCM_DTR);
	spin_unlock_irqrestore(&cinfo->card_lock, flags);
}

static int cyz_carrier_raised(struct tty_port *port)
{
	struct cyclades_port *info = container_of(port, struct cyclades_port,
			port);

	return readl(&info->u.cyz.ch_ctrl->rs_status) & C_RS_DCD;
}

static void cyz_dtr_rts(struct tty_port *port, int raise)
{
	struct cyclades_port *info = container_of(port, struct cyclades_port,
			port);
	struct cyclades_card *cinfo = info->card;
	struct CH_CTRL __iomem *ch_ctrl = info->u.cyz.ch_ctrl;
	int ret, channel = info->line - cinfo->first_line;
	u32 rs;

	rs = readl(&ch_ctrl->rs_control);
	if (raise)
		rs |= C_RS_RTS | C_RS_DTR;
	else
		rs &= ~(C_RS_RTS | C_RS_DTR);
	cy_writel(&ch_ctrl->rs_control, rs);
	ret = cyz_issue_cmd(cinfo, channel, C_CM_IOCTLM, 0L);
	if (ret != 0)
		printk(KERN_ERR "%s: retval on ttyC%d was %x\n",
				__func__, info->line, ret);
#ifdef CY_DEBUG_DTR
	printk(KERN_DEBUG "%s: raising Z DTR\n", __func__);
#endif
}

static const struct tty_port_operations cyy_port_ops = {
	.carrier_raised = cyy_carrier_raised,
	.dtr_rts = cyy_dtr_rts,
	.shutdown = cy_do_close,
};

static const struct tty_port_operations cyz_port_ops = {
	.carrier_raised = cyz_carrier_raised,
	.dtr_rts = cyz_dtr_rts,
	.shutdown = cy_do_close,
};


static int __devinit cy_init_card(struct cyclades_card *cinfo)
{
	struct cyclades_port *info;
	unsigned int channel, port;

	spin_lock_init(&cinfo->card_lock);
	cinfo->intr_enabled = 0;

	cinfo->ports = kcalloc(cinfo->nports, sizeof(*cinfo->ports),
			GFP_KERNEL);
	if (cinfo->ports == NULL) {
		printk(KERN_ERR "Cyclades: cannot allocate ports\n");
		return -ENOMEM;
	}

	for (channel = 0, port = cinfo->first_line; channel < cinfo->nports;
			channel++, port++) {
		info = &cinfo->ports[channel];
		tty_port_init(&info->port);
		info->magic = CYCLADES_MAGIC;
		info->card = cinfo;
		info->line = port;

		info->port.closing_wait = CLOSING_WAIT_DELAY;
		info->port.close_delay = 5 * HZ / 10;
		info->port.flags = STD_COM_FLAGS;
		init_completion(&info->shutdown_wait);

		if (cy_is_Z(cinfo)) {
			struct FIRM_ID *firm_id = cinfo->base_addr + ID_ADDRESS;
			struct ZFW_CTRL *zfw_ctrl;

			info->port.ops = &cyz_port_ops;
			info->type = PORT_STARTECH;

			zfw_ctrl = cinfo->base_addr +
				(readl(&firm_id->zfwctrl_addr) & 0xfffff);
			info->u.cyz.ch_ctrl = &zfw_ctrl->ch_ctrl[channel];
			info->u.cyz.buf_ctrl = &zfw_ctrl->buf_ctrl[channel];

			if (cinfo->hw_ver == ZO_V1)
				info->xmit_fifo_size = CYZ_FIFO_SIZE;
			else
				info->xmit_fifo_size = 4 * CYZ_FIFO_SIZE;
#ifdef CONFIG_CYZ_INTR
			setup_timer(&cyz_rx_full_timer[port],
				cyz_rx_restart, (unsigned long)info);
#endif
		} else {
			unsigned short chip_number;
			int index = cinfo->bus_index;

			info->port.ops = &cyy_port_ops;
			info->type = PORT_CIRRUS;
			info->xmit_fifo_size = CyMAX_CHAR_FIFO;
			info->cor1 = CyPARITY_NONE | Cy_1_STOP | Cy_8_BITS;
			info->cor2 = CyETC;
			info->cor3 = 0x08;	

			chip_number = channel / CyPORTS_PER_CHIP;
			info->u.cyy.base_addr = cinfo->base_addr +
				(cy_chip_offset[chip_number] << index);
			info->chip_rev = cyy_readb(info, CyGFRCR);

			if (info->chip_rev >= CD1400_REV_J) {
				
				info->tbpr = baud_bpr_60[13];	
				info->tco = baud_co_60[13];	
				info->rbpr = baud_bpr_60[13];	
				info->rco = baud_co_60[13];	
				info->rtsdtr_inv = 1;
			} else {
				info->tbpr = baud_bpr_25[13];	
				info->tco = baud_co_25[13];	
				info->rbpr = baud_bpr_25[13];	
				info->rco = baud_co_25[13];	
				info->rtsdtr_inv = 0;
			}
			info->read_status_mask = CyTIMEOUT | CySPECHAR |
				CyBREAK | CyPARITY | CyFRAME | CyOVERRUN;
		}

	}

#ifndef CONFIG_CYZ_INTR
	if (cy_is_Z(cinfo) && !timer_pending(&cyz_timerlist)) {
		mod_timer(&cyz_timerlist, jiffies + 1);
#ifdef CY_PCI_DEBUG
		printk(KERN_DEBUG "Cyclades-Z polling initialized\n");
#endif
	}
#endif
	return 0;
}

static unsigned short __devinit cyy_init_card(void __iomem *true_base_addr,
		int index)
{
	unsigned int chip_number;
	void __iomem *base_addr;

	cy_writeb(true_base_addr + (Cy_HwReset << index), 0);
	
	cy_writeb(true_base_addr + (Cy_ClrIntr << index), 0);
	
	udelay(500L);

	for (chip_number = 0; chip_number < CyMAX_CHIPS_PER_CARD;
							chip_number++) {
		base_addr =
		    true_base_addr + (cy_chip_offset[chip_number] << index);
		mdelay(1);
		if (readb(base_addr + (CyCCR << index)) != 0x00) {
			return chip_number;
		}

		cy_writeb(base_addr + (CyGFRCR << index), 0);
		udelay(10L);

		if (chip_number == 4 && readb(true_base_addr +
				(cy_chip_offset[0] << index) +
				(CyGFRCR << index)) == 0) {
			return chip_number;
		}

		cy_writeb(base_addr + (CyCCR << index), CyCHIP_RESET);
		mdelay(1);

		if (readb(base_addr + (CyGFRCR << index)) == 0x00) {
			return chip_number;
		}
		if ((0xf0 & (readb(base_addr + (CyGFRCR << index)))) !=
				0x40) {
			return chip_number;
		}
		cy_writeb(base_addr + (CyGCR << index), CyCH0_SERIAL);
		if (readb(base_addr + (CyGFRCR << index)) >= CD1400_REV_J) {
			
			cy_writeb(base_addr + (CyPPR << index), CyCLOCK_60_2MS);
		} else {
			
			cy_writeb(base_addr + (CyPPR << index), CyCLOCK_25_5MS);
		}

	}
	return chip_number;
}				

static int __init cy_detect_isa(void)
{
#ifdef CONFIG_ISA
	unsigned short cy_isa_irq, nboard;
	void __iomem *cy_isa_address;
	unsigned short i, j, cy_isa_nchan;
	int isparam = 0;

	nboard = 0;

	
	for (i = 0; i < NR_CARDS; i++) {
		if (maddr[i] || i) {
			isparam = 1;
			cy_isa_addresses[i] = maddr[i];
		}
		if (!maddr[i])
			break;
	}

	
	for (i = 0; i < NR_ISA_ADDRS; i++) {
		unsigned int isa_address = cy_isa_addresses[i];
		if (isa_address == 0x0000)
			return nboard;

		
		cy_isa_address = ioremap_nocache(isa_address, CyISA_Ywin);
		if (cy_isa_address == NULL) {
			printk(KERN_ERR "Cyclom-Y/ISA: can't remap base "
					"address\n");
			continue;
		}
		cy_isa_nchan = CyPORTS_PER_CHIP *
			cyy_init_card(cy_isa_address, 0);
		if (cy_isa_nchan == 0) {
			iounmap(cy_isa_address);
			continue;
		}

		if (isparam && i < NR_CARDS && irq[i])
			cy_isa_irq = irq[i];
		else
			
			cy_isa_irq = detect_isa_irq(cy_isa_address);
		if (cy_isa_irq == 0) {
			printk(KERN_ERR "Cyclom-Y/ISA found at 0x%lx, but the "
				"IRQ could not be detected.\n",
				(unsigned long)cy_isa_address);
			iounmap(cy_isa_address);
			continue;
		}

		if ((cy_next_channel + cy_isa_nchan) > NR_PORTS) {
			printk(KERN_ERR "Cyclom-Y/ISA found at 0x%lx, but no "
				"more channels are available. Change NR_PORTS "
				"in cyclades.c and recompile kernel.\n",
				(unsigned long)cy_isa_address);
			iounmap(cy_isa_address);
			return nboard;
		}
		
		for (j = 0; j < NR_CARDS; j++) {
			if (cy_card[j].base_addr == NULL)
				break;
		}
		if (j == NR_CARDS) {	
			printk(KERN_ERR "Cyclom-Y/ISA found at 0x%lx, but no "
				"more cards can be used. Change NR_CARDS in "
				"cyclades.c and recompile kernel.\n",
				(unsigned long)cy_isa_address);
			iounmap(cy_isa_address);
			return nboard;
		}

		
		if (request_irq(cy_isa_irq, cyy_interrupt,
				0, "Cyclom-Y", &cy_card[j])) {
			printk(KERN_ERR "Cyclom-Y/ISA found at 0x%lx, but "
				"could not allocate IRQ#%d.\n",
				(unsigned long)cy_isa_address, cy_isa_irq);
			iounmap(cy_isa_address);
			return nboard;
		}

		
		cy_card[j].base_addr = cy_isa_address;
		cy_card[j].ctl_addr.p9050 = NULL;
		cy_card[j].irq = (int)cy_isa_irq;
		cy_card[j].bus_index = 0;
		cy_card[j].first_line = cy_next_channel;
		cy_card[j].num_chips = cy_isa_nchan / CyPORTS_PER_CHIP;
		cy_card[j].nports = cy_isa_nchan;
		if (cy_init_card(&cy_card[j])) {
			cy_card[j].base_addr = NULL;
			free_irq(cy_isa_irq, &cy_card[j]);
			iounmap(cy_isa_address);
			continue;
		}
		nboard++;

		printk(KERN_INFO "Cyclom-Y/ISA #%d: 0x%lx-0x%lx, IRQ%d found: "
			"%d channels starting from port %d\n",
			j + 1, (unsigned long)cy_isa_address,
			(unsigned long)(cy_isa_address + (CyISA_Ywin - 1)),
			cy_isa_irq, cy_isa_nchan, cy_next_channel);

		for (j = cy_next_channel;
				j < cy_next_channel + cy_isa_nchan; j++)
			tty_register_device(cy_serial_driver, j, NULL);
		cy_next_channel += cy_isa_nchan;
	}
	return nboard;
#else
	return 0;
#endif				
}				

#ifdef CONFIG_PCI
static inline int __devinit cyc_isfwstr(const char *str, unsigned int size)
{
	unsigned int a;

	for (a = 0; a < size && *str; a++, str++)
		if (*str & 0x80)
			return -EINVAL;

	for (; a < size; a++, str++)
		if (*str)
			return -EINVAL;

	return 0;
}

static inline void __devinit cyz_fpga_copy(void __iomem *fpga, const u8 *data,
		unsigned int size)
{
	for (; size > 0; size--) {
		cy_writel(fpga, *data++);
		udelay(10);
	}
}

static void __devinit plx_init(struct pci_dev *pdev, int irq,
		struct RUNTIME_9060 __iomem *addr)
{
	
	cy_writel(&addr->init_ctrl, readl(&addr->init_ctrl) | 0x40000000);
	udelay(100L);
	cy_writel(&addr->init_ctrl, readl(&addr->init_ctrl) & ~0x40000000);

	
	cy_writel(&addr->init_ctrl, readl(&addr->init_ctrl) | 0x20000000);
	udelay(100L);
	cy_writel(&addr->init_ctrl, readl(&addr->init_ctrl) & ~0x20000000);

	pci_write_config_byte(pdev, PCI_INTERRUPT_LINE, irq);
}

static int __devinit __cyz_load_fw(const struct firmware *fw,
		const char *name, const u32 mailbox, void __iomem *base,
		void __iomem *fpga)
{
	const void *ptr = fw->data;
	const struct zfile_header *h = ptr;
	const struct zfile_config *c, *cs;
	const struct zfile_block *b, *bs;
	unsigned int a, tmp, len = fw->size;
#define BAD_FW KERN_ERR "Bad firmware: "
	if (len < sizeof(*h)) {
		printk(BAD_FW "too short: %u<%zu\n", len, sizeof(*h));
		return -EINVAL;
	}

	cs = ptr + h->config_offset;
	bs = ptr + h->block_offset;

	if ((void *)(cs + h->n_config) > ptr + len ||
			(void *)(bs + h->n_blocks) > ptr + len) {
		printk(BAD_FW "too short");
		return  -EINVAL;
	}

	if (cyc_isfwstr(h->name, sizeof(h->name)) ||
			cyc_isfwstr(h->date, sizeof(h->date))) {
		printk(BAD_FW "bad formatted header string\n");
		return -EINVAL;
	}

	if (strncmp(name, h->name, sizeof(h->name))) {
		printk(BAD_FW "bad name '%s' (expected '%s')\n", h->name, name);
		return -EINVAL;
	}

	tmp = 0;
	for (c = cs; c < cs + h->n_config; c++) {
		for (a = 0; a < c->n_blocks; a++)
			if (c->block_list[a] > h->n_blocks) {
				printk(BAD_FW "bad block ref number in cfgs\n");
				return -EINVAL;
			}
		if (c->mailbox == mailbox && c->function == 0) 
			tmp++;
	}
	if (!tmp) {
		printk(BAD_FW "nothing appropriate\n");
		return -EINVAL;
	}

	for (b = bs; b < bs + h->n_blocks; b++)
		if (b->file_offset + b->size > len) {
			printk(BAD_FW "bad block data offset\n");
			return -EINVAL;
		}

	
	for (c = cs; c < cs + h->n_config; c++)
		if (c->mailbox == mailbox && c->function == 0)
			break;

	for (a = 0; a < c->n_blocks; a++) {
		b = &bs[c->block_list[a]];
		if (b->type == ZBLOCK_FPGA) {
			if (fpga != NULL)
				cyz_fpga_copy(fpga, ptr + b->file_offset,
						b->size);
		} else {
			if (base != NULL)
				memcpy_toio(base + b->ram_offset,
					       ptr + b->file_offset, b->size);
		}
	}
#undef BAD_FW
	return 0;
}

static int __devinit cyz_load_fw(struct pci_dev *pdev, void __iomem *base_addr,
		struct RUNTIME_9060 __iomem *ctl_addr, int irq)
{
	const struct firmware *fw;
	struct FIRM_ID __iomem *fid = base_addr + ID_ADDRESS;
	struct CUSTOM_REG __iomem *cust = base_addr;
	struct ZFW_CTRL __iomem *pt_zfwctrl;
	void __iomem *tmp;
	u32 mailbox, status, nchan;
	unsigned int i;
	int retval;

	retval = request_firmware(&fw, "cyzfirm.bin", &pdev->dev);
	if (retval) {
		dev_err(&pdev->dev, "can't get firmware\n");
		goto err;
	}

	if (__cyz_fpga_loaded(ctl_addr) && readl(&fid->signature) == ZFIRM_ID) {
		u32 cntval = readl(base_addr + 0x190);

		udelay(100);
		if (cntval != readl(base_addr + 0x190)) {
			
			dev_dbg(&pdev->dev, "Cyclades-Z FW already loaded. "
					"Skipping board.\n");
			retval = 0;
			goto err_rel;
		}
	}

	
	cy_writel(&ctl_addr->intr_ctrl_stat, readl(&ctl_addr->intr_ctrl_stat) &
			~0x00030800UL);

	mailbox = readl(&ctl_addr->mail_box_0);

	if (mailbox == 0 || __cyz_fpga_loaded(ctl_addr)) {
		
		cy_writel(&ctl_addr->loc_addr_base, WIN_CREG);
		cy_writel(&cust->cpu_stop, 0);
		cy_writel(&ctl_addr->loc_addr_base, WIN_RAM);
		udelay(100);
	}

	plx_init(pdev, irq, ctl_addr);

	if (mailbox != 0) {
		
		retval = __cyz_load_fw(fw, "Cyclom-Z", mailbox, NULL,
				base_addr);
		if (retval)
			goto err_rel;
		if (!__cyz_fpga_loaded(ctl_addr)) {
			dev_err(&pdev->dev, "fw upload successful, but fw is "
					"not loaded\n");
			goto err_rel;
		}
	}

	
	cy_writel(&ctl_addr->loc_addr_base, WIN_CREG);
	cy_writel(&cust->cpu_stop, 0);
	cy_writel(&ctl_addr->loc_addr_base, WIN_RAM);
	udelay(100);

	
	for (tmp = base_addr; tmp < base_addr + RAM_SIZE; tmp++)
		cy_writeb(tmp, 255);
	if (mailbox != 0) {
		
		cy_writel(&ctl_addr->loc_addr_base, WIN_RAM + RAM_SIZE);
		for (tmp = base_addr; tmp < base_addr + RAM_SIZE; tmp++)
			cy_writeb(tmp, 255);
		
		cy_writel(&ctl_addr->loc_addr_base, WIN_RAM);
	}

	retval = __cyz_load_fw(fw, "Cyclom-Z", mailbox, base_addr, NULL);
	release_firmware(fw);
	if (retval)
		goto err;

	
	cy_writel(&ctl_addr->loc_addr_base, WIN_CREG);
	cy_writel(&cust->cpu_start, 0);
	cy_writel(&ctl_addr->loc_addr_base, WIN_RAM);
	i = 0;
	while ((status = readl(&fid->signature)) != ZFIRM_ID && i++ < 40)
		msleep(100);
	if (status != ZFIRM_ID) {
		if (status == ZFIRM_HLT) {
			dev_err(&pdev->dev, "you need an external power supply "
				"for this number of ports. Firmware halted and "
				"board reset.\n");
			retval = -EIO;
			goto err;
		}
		dev_warn(&pdev->dev, "fid->signature = 0x%x... Waiting "
				"some more time\n", status);
		while ((status = readl(&fid->signature)) != ZFIRM_ID &&
				i++ < 200)
			msleep(100);
		if (status != ZFIRM_ID) {
			dev_err(&pdev->dev, "Board not started in 20 seconds! "
					"Giving up. (fid->signature = 0x%x)\n",
					status);
			dev_info(&pdev->dev, "*** Warning ***: if you are "
				"upgrading the FW, please power cycle the "
				"system before loading the new FW to the "
				"Cyclades-Z.\n");

			if (__cyz_fpga_loaded(ctl_addr))
				plx_init(pdev, irq, ctl_addr);

			retval = -EIO;
			goto err;
		}
		dev_dbg(&pdev->dev, "Firmware started after %d seconds.\n",
				i / 10);
	}
	pt_zfwctrl = base_addr + readl(&fid->zfwctrl_addr);

	dev_dbg(&pdev->dev, "fid=> %p, zfwctrl_addr=> %x, npt_zfwctrl=> %p\n",
			base_addr + ID_ADDRESS, readl(&fid->zfwctrl_addr),
			base_addr + readl(&fid->zfwctrl_addr));

	nchan = readl(&pt_zfwctrl->board_ctrl.n_channel);
	dev_info(&pdev->dev, "Cyclades-Z FW loaded: version = %x, ports = %u\n",
		readl(&pt_zfwctrl->board_ctrl.fw_version), nchan);

	if (nchan == 0) {
		dev_warn(&pdev->dev, "no Cyclades-Z ports were found. Please "
			"check the connection between the Z host card and the "
			"serial expanders.\n");

		if (__cyz_fpga_loaded(ctl_addr))
			plx_init(pdev, irq, ctl_addr);

		dev_info(&pdev->dev, "Null number of ports detected. Board "
				"reset.\n");
		retval = 0;
		goto err;
	}

	cy_writel(&pt_zfwctrl->board_ctrl.op_system, C_OS_LINUX);
	cy_writel(&pt_zfwctrl->board_ctrl.dr_version, DRIVER_VERSION);

	cy_writel(&ctl_addr->intr_ctrl_stat, readl(&ctl_addr->intr_ctrl_stat) |
			(1 << 17));
	cy_writel(&ctl_addr->intr_ctrl_stat, readl(&ctl_addr->intr_ctrl_stat) |
			0x00030800UL);

	return nchan;
err_rel:
	release_firmware(fw);
err:
	return retval;
}

static int __devinit cy_pci_probe(struct pci_dev *pdev,
		const struct pci_device_id *ent)
{
	void __iomem *addr0 = NULL, *addr2 = NULL;
	char *card_name = NULL;
	u32 uninitialized_var(mailbox);
	unsigned int device_id, nchan = 0, card_no, i;
	unsigned char plx_ver;
	int retval, irq;

	retval = pci_enable_device(pdev);
	if (retval) {
		dev_err(&pdev->dev, "cannot enable device\n");
		goto err;
	}

	
	irq = pdev->irq;
	device_id = pdev->device & ~PCI_DEVICE_ID_MASK;

#if defined(__alpha__)
	if (device_id == PCI_DEVICE_ID_CYCLOM_Y_Lo) {	
		dev_err(&pdev->dev, "Cyclom-Y/PCI not supported for low "
			"addresses on Alpha systems.\n");
		retval = -EIO;
		goto err_dis;
	}
#endif
	if (device_id == PCI_DEVICE_ID_CYCLOM_Z_Lo) {
		dev_err(&pdev->dev, "Cyclades-Z/PCI not supported for low "
			"addresses\n");
		retval = -EIO;
		goto err_dis;
	}

	if (pci_resource_flags(pdev, 2) & IORESOURCE_IO) {
		dev_warn(&pdev->dev, "PCI I/O bit incorrectly set. Ignoring "
				"it...\n");
		pdev->resource[2].flags &= ~IORESOURCE_IO;
	}

	retval = pci_request_regions(pdev, "cyclades");
	if (retval) {
		dev_err(&pdev->dev, "failed to reserve resources\n");
		goto err_dis;
	}

	retval = -EIO;
	if (device_id == PCI_DEVICE_ID_CYCLOM_Y_Lo ||
			device_id == PCI_DEVICE_ID_CYCLOM_Y_Hi) {
		card_name = "Cyclom-Y";

		addr0 = ioremap_nocache(pci_resource_start(pdev, 0),
				CyPCI_Yctl);
		if (addr0 == NULL) {
			dev_err(&pdev->dev, "can't remap ctl region\n");
			goto err_reg;
		}
		addr2 = ioremap_nocache(pci_resource_start(pdev, 2),
				CyPCI_Ywin);
		if (addr2 == NULL) {
			dev_err(&pdev->dev, "can't remap base region\n");
			goto err_unmap;
		}

		nchan = CyPORTS_PER_CHIP * cyy_init_card(addr2, 1);
		if (nchan == 0) {
			dev_err(&pdev->dev, "Cyclom-Y PCI host card with no "
					"Serial-Modules\n");
			goto err_unmap;
		}
	} else if (device_id == PCI_DEVICE_ID_CYCLOM_Z_Hi) {
		struct RUNTIME_9060 __iomem *ctl_addr;

		ctl_addr = addr0 = ioremap_nocache(pci_resource_start(pdev, 0),
				CyPCI_Zctl);
		if (addr0 == NULL) {
			dev_err(&pdev->dev, "can't remap ctl region\n");
			goto err_reg;
		}

		
		cy_writew(&ctl_addr->intr_ctrl_stat,
				readw(&ctl_addr->intr_ctrl_stat) & ~0x0900);

		plx_init(pdev, irq, addr0);

		mailbox = readl(&ctl_addr->mail_box_0);

		addr2 = ioremap_nocache(pci_resource_start(pdev, 2),
				mailbox == ZE_V1 ? CyPCI_Ze_win : CyPCI_Zwin);
		if (addr2 == NULL) {
			dev_err(&pdev->dev, "can't remap base region\n");
			goto err_unmap;
		}

		if (mailbox == ZE_V1) {
			card_name = "Cyclades-Ze";
		} else {
			card_name = "Cyclades-8Zo";
#ifdef CY_PCI_DEBUG
			if (mailbox == ZO_V1) {
				cy_writel(&ctl_addr->loc_addr_base, WIN_CREG);
				dev_info(&pdev->dev, "Cyclades-8Zo/PCI: FPGA "
					"id %lx, ver %lx\n", (ulong)(0xff &
					readl(&((struct CUSTOM_REG *)addr2)->
						fpga_id)), (ulong)(0xff &
					readl(&((struct CUSTOM_REG *)addr2)->
						fpga_version)));
				cy_writel(&ctl_addr->loc_addr_base, WIN_RAM);
			} else {
				dev_info(&pdev->dev, "Cyclades-Z/PCI: New "
					"Cyclades-Z board.  FPGA not loaded\n");
			}
#endif
			if ((mailbox == ZO_V1) || (mailbox == ZO_V2))
				cy_writel(addr2 + ID_ADDRESS, 0L);
		}

		retval = cyz_load_fw(pdev, addr2, addr0, irq);
		if (retval <= 0)
			goto err_unmap;
		nchan = retval;
	}

	if ((cy_next_channel + nchan) > NR_PORTS) {
		dev_err(&pdev->dev, "Cyclades-8Zo/PCI found, but no "
			"channels are available. Change NR_PORTS in "
			"cyclades.c and recompile kernel.\n");
		goto err_unmap;
	}
	
	for (card_no = 0; card_no < NR_CARDS; card_no++) {
		if (cy_card[card_no].base_addr == NULL)
			break;
	}
	if (card_no == NR_CARDS) {	
		dev_err(&pdev->dev, "Cyclades-8Zo/PCI found, but no "
			"more cards can be used. Change NR_CARDS in "
			"cyclades.c and recompile kernel.\n");
		goto err_unmap;
	}

	if (device_id == PCI_DEVICE_ID_CYCLOM_Y_Lo ||
			device_id == PCI_DEVICE_ID_CYCLOM_Y_Hi) {
		
		retval = request_irq(irq, cyy_interrupt,
				IRQF_SHARED, "Cyclom-Y", &cy_card[card_no]);
		if (retval) {
			dev_err(&pdev->dev, "could not allocate IRQ\n");
			goto err_unmap;
		}
		cy_card[card_no].num_chips = nchan / CyPORTS_PER_CHIP;
	} else {
		struct FIRM_ID __iomem *firm_id = addr2 + ID_ADDRESS;
		struct ZFW_CTRL __iomem *zfw_ctrl;

		zfw_ctrl = addr2 + (readl(&firm_id->zfwctrl_addr) & 0xfffff);

		cy_card[card_no].hw_ver = mailbox;
		cy_card[card_no].num_chips = (unsigned int)-1;
		cy_card[card_no].board_ctrl = &zfw_ctrl->board_ctrl;
#ifdef CONFIG_CYZ_INTR
		
		if (irq != 0 && irq != 255) {
			retval = request_irq(irq, cyz_interrupt,
					IRQF_SHARED, "Cyclades-Z",
					&cy_card[card_no]);
			if (retval) {
				dev_err(&pdev->dev, "could not allocate IRQ\n");
				goto err_unmap;
			}
		}
#endif				
	}

	
	cy_card[card_no].base_addr = addr2;
	cy_card[card_no].ctl_addr.p9050 = addr0;
	cy_card[card_no].irq = irq;
	cy_card[card_no].bus_index = 1;
	cy_card[card_no].first_line = cy_next_channel;
	cy_card[card_no].nports = nchan;
	retval = cy_init_card(&cy_card[card_no]);
	if (retval)
		goto err_null;

	pci_set_drvdata(pdev, &cy_card[card_no]);

	if (device_id == PCI_DEVICE_ID_CYCLOM_Y_Lo ||
			device_id == PCI_DEVICE_ID_CYCLOM_Y_Hi) {
		
		plx_ver = readb(addr2 + CyPLX_VER) & 0x0f;
		switch (plx_ver) {
		case PLX_9050:
			cy_writeb(addr0 + 0x4c, 0x43);
			break;

		case PLX_9060:
		case PLX_9080:
		default:	
		{
			struct RUNTIME_9060 __iomem *ctl_addr = addr0;
			plx_init(pdev, irq, ctl_addr);
			cy_writew(&ctl_addr->intr_ctrl_stat,
				readw(&ctl_addr->intr_ctrl_stat) | 0x0900);
			break;
		}
		}
	}

	dev_info(&pdev->dev, "%s/PCI #%d found: %d channels starting from "
		"port %d.\n", card_name, card_no + 1, nchan, cy_next_channel);
	for (i = cy_next_channel; i < cy_next_channel + nchan; i++)
		tty_register_device(cy_serial_driver, i, &pdev->dev);
	cy_next_channel += nchan;

	return 0;
err_null:
	cy_card[card_no].base_addr = NULL;
	free_irq(irq, &cy_card[card_no]);
err_unmap:
	iounmap(addr0);
	if (addr2)
		iounmap(addr2);
err_reg:
	pci_release_regions(pdev);
err_dis:
	pci_disable_device(pdev);
err:
	return retval;
}

static void __devexit cy_pci_remove(struct pci_dev *pdev)
{
	struct cyclades_card *cinfo = pci_get_drvdata(pdev);
	unsigned int i;

	
	if (!cy_is_Z(cinfo) && (readb(cinfo->base_addr + CyPLX_VER) & 0x0f) ==
			PLX_9050)
		cy_writeb(cinfo->ctl_addr.p9050 + 0x4c, 0);
	else
#ifndef CONFIG_CYZ_INTR
		if (!cy_is_Z(cinfo))
#endif
		cy_writew(&cinfo->ctl_addr.p9060->intr_ctrl_stat,
			readw(&cinfo->ctl_addr.p9060->intr_ctrl_stat) &
			~0x0900);

	iounmap(cinfo->base_addr);
	if (cinfo->ctl_addr.p9050)
		iounmap(cinfo->ctl_addr.p9050);
	if (cinfo->irq
#ifndef CONFIG_CYZ_INTR
		&& !cy_is_Z(cinfo)
#endif 
		)
		free_irq(cinfo->irq, cinfo);
	pci_release_regions(pdev);

	cinfo->base_addr = NULL;
	for (i = cinfo->first_line; i < cinfo->first_line +
			cinfo->nports; i++)
		tty_unregister_device(cy_serial_driver, i);
	cinfo->nports = 0;
	kfree(cinfo->ports);
}

static struct pci_driver cy_pci_driver = {
	.name = "cyclades",
	.id_table = cy_pci_dev_id,
	.probe = cy_pci_probe,
	.remove = __devexit_p(cy_pci_remove)
};
#endif

static int cyclades_proc_show(struct seq_file *m, void *v)
{
	struct cyclades_port *info;
	unsigned int i, j;
	__u32 cur_jifs = jiffies;

	seq_puts(m, "Dev TimeOpen   BytesOut  IdleOut    BytesIn   "
			"IdleIn  Overruns  Ldisc\n");

	
	for (i = 0; i < NR_CARDS; i++)
		for (j = 0; j < cy_card[i].nports; j++) {
			info = &cy_card[i].ports[j];

			if (info->port.count) {
				
				struct tty_struct *tty;
				struct tty_ldisc *ld;
				int num = 0;
				tty = tty_port_tty_get(&info->port);
				if (tty) {
					ld = tty_ldisc_ref(tty);
					if (ld) {
						num = ld->ops->num;
						tty_ldisc_deref(ld);
					}
					tty_kref_put(tty);
				}
				seq_printf(m, "%3d %8lu %10lu %8lu "
					"%10lu %8lu %9lu %6d\n", info->line,
					(cur_jifs - info->idle_stats.in_use) /
					HZ, info->idle_stats.xmit_bytes,
					(cur_jifs - info->idle_stats.xmit_idle)/
					HZ, info->idle_stats.recv_bytes,
					(cur_jifs - info->idle_stats.recv_idle)/
					HZ, info->idle_stats.overruns,
					num);
			} else
				seq_printf(m, "%3d %8lu %10lu %8lu "
					"%10lu %8lu %9lu %6ld\n",
					info->line, 0L, 0L, 0L, 0L, 0L, 0L, 0L);
		}
	return 0;
}

static int cyclades_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, cyclades_proc_show, NULL);
}

static const struct file_operations cyclades_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= cyclades_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


static const struct tty_operations cy_ops = {
	.open = cy_open,
	.close = cy_close,
	.write = cy_write,
	.put_char = cy_put_char,
	.flush_chars = cy_flush_chars,
	.write_room = cy_write_room,
	.chars_in_buffer = cy_chars_in_buffer,
	.flush_buffer = cy_flush_buffer,
	.ioctl = cy_ioctl,
	.throttle = cy_throttle,
	.unthrottle = cy_unthrottle,
	.set_termios = cy_set_termios,
	.stop = cy_stop,
	.start = cy_start,
	.hangup = cy_hangup,
	.break_ctl = cy_break,
	.wait_until_sent = cy_wait_until_sent,
	.tiocmget = cy_tiocmget,
	.tiocmset = cy_tiocmset,
	.get_icount = cy_get_icount,
	.proc_fops = &cyclades_proc_fops,
};

static int __init cy_init(void)
{
	unsigned int nboards;
	int retval = -ENOMEM;

	cy_serial_driver = alloc_tty_driver(NR_PORTS);
	if (!cy_serial_driver)
		goto err;

	printk(KERN_INFO "Cyclades driver " CY_VERSION "\n");

	

	cy_serial_driver->driver_name = "cyclades";
	cy_serial_driver->name = "ttyC";
	cy_serial_driver->major = CYCLADES_MAJOR;
	cy_serial_driver->minor_start = 0;
	cy_serial_driver->type = TTY_DRIVER_TYPE_SERIAL;
	cy_serial_driver->subtype = SERIAL_TYPE_NORMAL;
	cy_serial_driver->init_termios = tty_std_termios;
	cy_serial_driver->init_termios.c_cflag =
	    B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	cy_serial_driver->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	tty_set_operations(cy_serial_driver, &cy_ops);

	retval = tty_register_driver(cy_serial_driver);
	if (retval) {
		printk(KERN_ERR "Couldn't register Cyclades serial driver\n");
		goto err_frtty;
	}


	
	nboards = cy_detect_isa();

#ifdef CONFIG_PCI
	
	retval = pci_register_driver(&cy_pci_driver);
	if (retval && !nboards) {
		tty_unregister_driver(cy_serial_driver);
		goto err_frtty;
	}
#endif

	return 0;
err_frtty:
	put_tty_driver(cy_serial_driver);
err:
	return retval;
}				

static void __exit cy_cleanup_module(void)
{
	struct cyclades_card *card;
	unsigned int i, e1;

#ifndef CONFIG_CYZ_INTR
	del_timer_sync(&cyz_timerlist);
#endif 

	e1 = tty_unregister_driver(cy_serial_driver);
	if (e1)
		printk(KERN_ERR "failed to unregister Cyclades serial "
				"driver(%d)\n", e1);

#ifdef CONFIG_PCI
	pci_unregister_driver(&cy_pci_driver);
#endif

	for (i = 0; i < NR_CARDS; i++) {
		card = &cy_card[i];
		if (card->base_addr) {
			
			cy_writeb(card->base_addr + Cy_ClrIntr, 0);
			iounmap(card->base_addr);
			if (card->ctl_addr.p9050)
				iounmap(card->ctl_addr.p9050);
			if (card->irq
#ifndef CONFIG_CYZ_INTR
				&& !cy_is_Z(card)
#endif 
				)
				free_irq(card->irq, card);
			for (e1 = card->first_line; e1 < card->first_line +
					card->nports; e1++)
				tty_unregister_device(cy_serial_driver, e1);
			kfree(card->ports);
		}
	}

	put_tty_driver(cy_serial_driver);
} 

module_init(cy_init);
module_exit(cy_cleanup_module);

MODULE_LICENSE("GPL");
MODULE_VERSION(CY_VERSION);
MODULE_ALIAS_CHARDEV_MAJOR(CYCLADES_MAJOR);
MODULE_FIRMWARE("cyzfirm.bin");
