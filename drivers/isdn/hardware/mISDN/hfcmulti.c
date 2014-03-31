/*
 * hfcmulti.c  low level driver for hfc-4s/hfc-8s/hfc-e1 based cards
 *
 * Author	Andreas Eversberg (jolly@eversberg.eu)
 * ported to mqueue mechanism:
 *		Peter Sprenger (sprengermoving-bytes.de)
 *
 * inspired by existing hfc-pci driver:
 * Copyright 1999  by Werner Cornelius (werner@isdn-development.de)
 * Copyright 2008  by Karsten Keil (kkeil@suse.de)
 * Copyright 2008  by Andreas Eversberg (jolly@eversberg.eu)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * Thanks to Cologne Chip AG for this great controller!
 */



#define HFC_MULTI_VERSION	"2.03"

#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/mISDNhw.h>
#include <linux/mISDNdsp.h>


#include "hfc_multi.h"
#ifdef ECHOPREP
#include "gaintab.h"
#endif

#define	MAX_CARDS	8
#define	MAX_PORTS	(8 * MAX_CARDS)

static LIST_HEAD(HFClist);
static spinlock_t HFClock; 

static void ph_state_change(struct dchannel *);

static struct hfc_multi *syncmaster;
static int plxsd_master; 
static spinlock_t plx_lock; 

#define	TYP_E1		1
#define	TYP_4S		4
#define TYP_8S		8

static int poll_timer = 6;	
static int nt_t1_count[] = { 3840, 1920, 960, 480, 240, 120, 60, 30  };
#define	CLKDEL_TE	0x0f	
#define	CLKDEL_NT	0x6c	

#define	DIP_4S	0x1		
#define	DIP_8S	0x2		
#define	DIP_E1	0x3		


static uint	type[MAX_CARDS];
static int	pcm[MAX_CARDS];
static int	dslot[MAX_CARDS];
static uint	iomode[MAX_CARDS];
static uint	port[MAX_PORTS];
static uint	debug;
static uint	poll;
static int	clock;
static uint	timer;
static uint	clockdelay_te = CLKDEL_TE;
static uint	clockdelay_nt = CLKDEL_NT;
#define HWID_NONE	0
#define HWID_MINIP4	1
#define HWID_MINIP8	2
#define HWID_MINIP16	3
static uint	hwid = HWID_NONE;

static int	HFC_cnt, Port_cnt, PCM_cnt = 99;

MODULE_AUTHOR("Andreas Eversberg");
MODULE_LICENSE("GPL");
MODULE_VERSION(HFC_MULTI_VERSION);
module_param(debug, uint, S_IRUGO | S_IWUSR);
module_param(poll, uint, S_IRUGO | S_IWUSR);
module_param(clock, int, S_IRUGO | S_IWUSR);
module_param(timer, uint, S_IRUGO | S_IWUSR);
module_param(clockdelay_te, uint, S_IRUGO | S_IWUSR);
module_param(clockdelay_nt, uint, S_IRUGO | S_IWUSR);
module_param_array(type, uint, NULL, S_IRUGO | S_IWUSR);
module_param_array(pcm, int, NULL, S_IRUGO | S_IWUSR);
module_param_array(dslot, int, NULL, S_IRUGO | S_IWUSR);
module_param_array(iomode, uint, NULL, S_IRUGO | S_IWUSR);
module_param_array(port, uint, NULL, S_IRUGO | S_IWUSR);
module_param(hwid, uint, S_IRUGO | S_IWUSR); 

#ifdef HFC_REGISTER_DEBUG
#define HFC_outb(hc, reg, val)					\
	(hc->HFC_outb(hc, reg, val, __func__, __LINE__))
#define HFC_outb_nodebug(hc, reg, val)					\
	(hc->HFC_outb_nodebug(hc, reg, val, __func__, __LINE__))
#define HFC_inb(hc, reg)				\
	(hc->HFC_inb(hc, reg, __func__, __LINE__))
#define HFC_inb_nodebug(hc, reg)				\
	(hc->HFC_inb_nodebug(hc, reg, __func__, __LINE__))
#define HFC_inw(hc, reg)				\
	(hc->HFC_inw(hc, reg, __func__, __LINE__))
#define HFC_inw_nodebug(hc, reg)				\
	(hc->HFC_inw_nodebug(hc, reg, __func__, __LINE__))
#define HFC_wait(hc)				\
	(hc->HFC_wait(hc, __func__, __LINE__))
#define HFC_wait_nodebug(hc)				\
	(hc->HFC_wait_nodebug(hc, __func__, __LINE__))
#else
#define HFC_outb(hc, reg, val)		(hc->HFC_outb(hc, reg, val))
#define HFC_outb_nodebug(hc, reg, val)	(hc->HFC_outb_nodebug(hc, reg, val))
#define HFC_inb(hc, reg)		(hc->HFC_inb(hc, reg))
#define HFC_inb_nodebug(hc, reg)	(hc->HFC_inb_nodebug(hc, reg))
#define HFC_inw(hc, reg)		(hc->HFC_inw(hc, reg))
#define HFC_inw_nodebug(hc, reg)	(hc->HFC_inw_nodebug(hc, reg))
#define HFC_wait(hc)			(hc->HFC_wait(hc))
#define HFC_wait_nodebug(hc)		(hc->HFC_wait_nodebug(hc))
#endif

#ifdef CONFIG_MISDN_HFCMULTI_8xx
#include "hfc_multi_8xx.h"
#endif

static void
#ifdef HFC_REGISTER_DEBUG
HFC_outb_pcimem(struct hfc_multi *hc, u_char reg, u_char val,
		const char *function, int line)
#else
	HFC_outb_pcimem(struct hfc_multi *hc, u_char reg, u_char val)
#endif
{
	writeb(val, hc->pci_membase + reg);
}
static u_char
#ifdef HFC_REGISTER_DEBUG
HFC_inb_pcimem(struct hfc_multi *hc, u_char reg, const char *function, int line)
#else
	HFC_inb_pcimem(struct hfc_multi *hc, u_char reg)
#endif
{
	return readb(hc->pci_membase + reg);
}
static u_short
#ifdef HFC_REGISTER_DEBUG
HFC_inw_pcimem(struct hfc_multi *hc, u_char reg, const char *function, int line)
#else
	HFC_inw_pcimem(struct hfc_multi *hc, u_char reg)
#endif
{
	return readw(hc->pci_membase + reg);
}
static void
#ifdef HFC_REGISTER_DEBUG
HFC_wait_pcimem(struct hfc_multi *hc, const char *function, int line)
#else
	HFC_wait_pcimem(struct hfc_multi *hc)
#endif
{
	while (readb(hc->pci_membase + R_STATUS) & V_BUSY)
		cpu_relax();
}

static void
#ifdef HFC_REGISTER_DEBUG
HFC_outb_regio(struct hfc_multi *hc, u_char reg, u_char val,
	       const char *function, int line)
#else
	HFC_outb_regio(struct hfc_multi *hc, u_char reg, u_char val)
#endif
{
	outb(reg, hc->pci_iobase + 4);
	outb(val, hc->pci_iobase);
}
static u_char
#ifdef HFC_REGISTER_DEBUG
HFC_inb_regio(struct hfc_multi *hc, u_char reg, const char *function, int line)
#else
	HFC_inb_regio(struct hfc_multi *hc, u_char reg)
#endif
{
	outb(reg, hc->pci_iobase + 4);
	return inb(hc->pci_iobase);
}
static u_short
#ifdef HFC_REGISTER_DEBUG
HFC_inw_regio(struct hfc_multi *hc, u_char reg, const char *function, int line)
#else
	HFC_inw_regio(struct hfc_multi *hc, u_char reg)
#endif
{
	outb(reg, hc->pci_iobase + 4);
	return inw(hc->pci_iobase);
}
static void
#ifdef HFC_REGISTER_DEBUG
HFC_wait_regio(struct hfc_multi *hc, const char *function, int line)
#else
	HFC_wait_regio(struct hfc_multi *hc)
#endif
{
	outb(R_STATUS, hc->pci_iobase + 4);
	while (inb(hc->pci_iobase) & V_BUSY)
		cpu_relax();
}

#ifdef HFC_REGISTER_DEBUG
static void
HFC_outb_debug(struct hfc_multi *hc, u_char reg, u_char val,
	       const char *function, int line)
{
	char regname[256] = "", bits[9] = "xxxxxxxx";
	int i;

	i = -1;
	while (hfc_register_names[++i].name) {
		if (hfc_register_names[i].reg == reg)
			strcat(regname, hfc_register_names[i].name);
	}
	if (regname[0] == '\0')
		strcpy(regname, "register");

	bits[7] = '0' + (!!(val & 1));
	bits[6] = '0' + (!!(val & 2));
	bits[5] = '0' + (!!(val & 4));
	bits[4] = '0' + (!!(val & 8));
	bits[3] = '0' + (!!(val & 16));
	bits[2] = '0' + (!!(val & 32));
	bits[1] = '0' + (!!(val & 64));
	bits[0] = '0' + (!!(val & 128));
	printk(KERN_DEBUG
	       "HFC_outb(chip %d, %02x=%s, 0x%02x=%s); in %s() line %d\n",
	       hc->id, reg, regname, val, bits, function, line);
	HFC_outb_nodebug(hc, reg, val);
}
static u_char
HFC_inb_debug(struct hfc_multi *hc, u_char reg, const char *function, int line)
{
	char regname[256] = "", bits[9] = "xxxxxxxx";
	u_char val = HFC_inb_nodebug(hc, reg);
	int i;

	i = 0;
	while (hfc_register_names[i++].name)
		;
	while (hfc_register_names[++i].name) {
		if (hfc_register_names[i].reg == reg)
			strcat(regname, hfc_register_names[i].name);
	}
	if (regname[0] == '\0')
		strcpy(regname, "register");

	bits[7] = '0' + (!!(val & 1));
	bits[6] = '0' + (!!(val & 2));
	bits[5] = '0' + (!!(val & 4));
	bits[4] = '0' + (!!(val & 8));
	bits[3] = '0' + (!!(val & 16));
	bits[2] = '0' + (!!(val & 32));
	bits[1] = '0' + (!!(val & 64));
	bits[0] = '0' + (!!(val & 128));
	printk(KERN_DEBUG
	       "HFC_inb(chip %d, %02x=%s) = 0x%02x=%s; in %s() line %d\n",
	       hc->id, reg, regname, val, bits, function, line);
	return val;
}
static u_short
HFC_inw_debug(struct hfc_multi *hc, u_char reg, const char *function, int line)
{
	char regname[256] = "";
	u_short val = HFC_inw_nodebug(hc, reg);
	int i;

	i = 0;
	while (hfc_register_names[i++].name)
		;
	while (hfc_register_names[++i].name) {
		if (hfc_register_names[i].reg == reg)
			strcat(regname, hfc_register_names[i].name);
	}
	if (regname[0] == '\0')
		strcpy(regname, "register");

	printk(KERN_DEBUG
	       "HFC_inw(chip %d, %02x=%s) = 0x%04x; in %s() line %d\n",
	       hc->id, reg, regname, val, function, line);
	return val;
}
static void
HFC_wait_debug(struct hfc_multi *hc, const char *function, int line)
{
	printk(KERN_DEBUG "HFC_wait(chip %d); in %s() line %d\n",
	       hc->id, function, line);
	HFC_wait_nodebug(hc);
}
#endif

static void
write_fifo_regio(struct hfc_multi *hc, u_char *data, int len)
{
	outb(A_FIFO_DATA0, (hc->pci_iobase) + 4);
	while (len >> 2) {
		outl(cpu_to_le32(*(u32 *)data), hc->pci_iobase);
		data += 4;
		len -= 4;
	}
	while (len >> 1) {
		outw(cpu_to_le16(*(u16 *)data), hc->pci_iobase);
		data += 2;
		len -= 2;
	}
	while (len) {
		outb(*data, hc->pci_iobase);
		data++;
		len--;
	}
}
static void
write_fifo_pcimem(struct hfc_multi *hc, u_char *data, int len)
{
	while (len >> 2) {
		writel(cpu_to_le32(*(u32 *)data),
		       hc->pci_membase + A_FIFO_DATA0);
		data += 4;
		len -= 4;
	}
	while (len >> 1) {
		writew(cpu_to_le16(*(u16 *)data),
		       hc->pci_membase + A_FIFO_DATA0);
		data += 2;
		len -= 2;
	}
	while (len) {
		writeb(*data, hc->pci_membase + A_FIFO_DATA0);
		data++;
		len--;
	}
}

static void
read_fifo_regio(struct hfc_multi *hc, u_char *data, int len)
{
	outb(A_FIFO_DATA0, (hc->pci_iobase) + 4);
	while (len >> 2) {
		*(u32 *)data = le32_to_cpu(inl(hc->pci_iobase));
		data += 4;
		len -= 4;
	}
	while (len >> 1) {
		*(u16 *)data = le16_to_cpu(inw(hc->pci_iobase));
		data += 2;
		len -= 2;
	}
	while (len) {
		*data = inb(hc->pci_iobase);
		data++;
		len--;
	}
}

static void
read_fifo_pcimem(struct hfc_multi *hc, u_char *data, int len)
{
	while (len >> 2) {
		*(u32 *)data =
			le32_to_cpu(readl(hc->pci_membase + A_FIFO_DATA0));
		data += 4;
		len -= 4;
	}
	while (len >> 1) {
		*(u16 *)data =
			le16_to_cpu(readw(hc->pci_membase + A_FIFO_DATA0));
		data += 2;
		len -= 2;
	}
	while (len) {
		*data = readb(hc->pci_membase + A_FIFO_DATA0);
		data++;
		len--;
	}
}

static void
enable_hwirq(struct hfc_multi *hc)
{
	hc->hw.r_irq_ctrl |= V_GLOB_IRQ_EN;
	HFC_outb(hc, R_IRQ_CTRL, hc->hw.r_irq_ctrl);
}

static void
disable_hwirq(struct hfc_multi *hc)
{
	hc->hw.r_irq_ctrl &= ~((u_char)V_GLOB_IRQ_EN);
	HFC_outb(hc, R_IRQ_CTRL, hc->hw.r_irq_ctrl);
}

#define	NUM_EC 2
#define	MAX_TDM_CHAN 32


inline void
enablepcibridge(struct hfc_multi *c)
{
	HFC_outb(c, R_BRG_PCM_CFG, (0x0 << 6) | 0x3); 
}

inline void
disablepcibridge(struct hfc_multi *c)
{
	HFC_outb(c, R_BRG_PCM_CFG, (0x0 << 6) | 0x2); 
}

inline unsigned char
readpcibridge(struct hfc_multi *hc, unsigned char address)
{
	unsigned short cipv;
	unsigned char data;

	if (!hc->pci_iobase)
		return 0;

	
	HFC_outb(hc, R_CTRL, 0x4); 

	if (address == 0)
		cipv = 0x4000;
	else
		cipv = 0x5800;

	
	
	outw(cipv, hc->pci_iobase + 4);
	data = inb(hc->pci_iobase);

	
	HFC_outb(hc, R_CTRL, 0x0); 

	return data;
}

inline void
writepcibridge(struct hfc_multi *hc, unsigned char address, unsigned char data)
{
	unsigned short cipv;
	unsigned int datav;

	if (!hc->pci_iobase)
		return;

	if (address == 0)
		cipv = 0x4000;
	else
		cipv = 0x5800;

	
	outw(cipv, hc->pci_iobase + 4);
	
	datav = data | ((__u32) data << 8) | ((__u32) data << 16) |
		((__u32) data << 24);

	outl(datav, hc->pci_iobase);
}

inline void
cpld_set_reg(struct hfc_multi *hc, unsigned char reg)
{
	
	HFC_outb(hc, R_GPIO_OUT1, reg);
}

inline void
cpld_write_reg(struct hfc_multi *hc, unsigned char reg, unsigned char val)
{
	cpld_set_reg(hc, reg);

	enablepcibridge(hc);
	writepcibridge(hc, 1, val);
	disablepcibridge(hc);

	return;
}

inline unsigned char
cpld_read_reg(struct hfc_multi *hc, unsigned char reg)
{
	unsigned char bytein;

	cpld_set_reg(hc, reg);

	
	HFC_outb(hc, R_GPIO_OUT1, reg);

	enablepcibridge(hc);
	bytein = readpcibridge(hc, 1);
	disablepcibridge(hc);

	return bytein;
}

inline void
vpm_write_address(struct hfc_multi *hc, unsigned short addr)
{
	cpld_write_reg(hc, 0, 0xff & addr);
	cpld_write_reg(hc, 1, 0x01 & (addr >> 8));
}

inline unsigned short
vpm_read_address(struct hfc_multi *c)
{
	unsigned short addr;
	unsigned short highbit;

	addr = cpld_read_reg(c, 0);
	highbit = cpld_read_reg(c, 1);

	addr = addr | (highbit << 8);

	return addr & 0x1ff;
}

inline unsigned char
vpm_in(struct hfc_multi *c, int which, unsigned short addr)
{
	unsigned char res;

	vpm_write_address(c, addr);

	if (!which)
		cpld_set_reg(c, 2);
	else
		cpld_set_reg(c, 3);

	enablepcibridge(c);
	res = readpcibridge(c, 1);
	disablepcibridge(c);

	cpld_set_reg(c, 0);

	return res;
}

inline void
vpm_out(struct hfc_multi *c, int which, unsigned short addr,
	unsigned char data)
{
	vpm_write_address(c, addr);

	enablepcibridge(c);

	if (!which)
		cpld_set_reg(c, 2);
	else
		cpld_set_reg(c, 3);

	writepcibridge(c, 1, data);

	cpld_set_reg(c, 0);

	disablepcibridge(c);

	{
		unsigned char regin;
		regin = vpm_in(c, which, addr);
		if (regin != data)
			printk(KERN_DEBUG "Wrote 0x%x to register 0x%x but got back "
			       "0x%x\n", data, addr, regin);
	}

}


static void
vpm_init(struct hfc_multi *wc)
{
	unsigned char reg;
	unsigned int mask;
	unsigned int i, x, y;
	unsigned int ver;

	for (x = 0; x < NUM_EC; x++) {
		
		if (!x) {
			ver = vpm_in(wc, x, 0x1a0);
			printk(KERN_DEBUG "VPM: Chip %d: ver %02x\n", x, ver);
		}

		for (y = 0; y < 4; y++) {
			vpm_out(wc, x, 0x1a8 + y, 0x00); 
			vpm_out(wc, x, 0x1ac + y, 0x00); 
			vpm_out(wc, x, 0x1b0 + y, 0x00); 
		}

		
		reg = vpm_in(wc, x, 0x1a3); 
		vpm_out(wc, x, 0x1a3, reg & ~2);

		
		vpm_out(wc, x, 0x022, 1);
		vpm_out(wc, x, 0x023, 0xff);

		
		vpm_out(wc, x, 0x02f, 0x00);
		mask = 0x02020202 << (x * 4);

		
		for (i = 0; i < 4; i++)
			vpm_out(wc, x, 0x33 - i, (mask >> (i << 3)) & 0xff);

		
		printk(KERN_DEBUG "VPM: A-law mode\n");
		reg = 0x00 | 0x10 | 0x01;
		vpm_out(wc, x, 0x20, reg);
		printk(KERN_DEBUG "VPM reg 0x20 is %x\n", reg);
		

		vpm_out(wc, x, 0x24, 0x02);
		reg = vpm_in(wc, x, 0x24);
		printk(KERN_DEBUG "NLP Thresh is set to %d (0x%x)\n", reg, reg);

		
		for (i = 0; i < MAX_TDM_CHAN; i++) {
			if (mask & (0x00000001 << i))
				vpm_out(wc, x, i, 0x00);
		}


		udelay(2000);
		udelay(2000);
		udelay(2000);
		udelay(2000);
		udelay(2000);

		
		for (i = 0; i < MAX_TDM_CHAN; i++) {
			if (mask & (0x00000001 << i))
				vpm_out(wc, x, i, 0x01);
		}

		
		for (i = 0; i < MAX_TDM_CHAN; i++) {
			if (mask & (0x00000001 << i))
				vpm_out(wc, x, 0x78 + i, 0x01);
		}

	}
}

#ifdef UNUSED
static void
vpm_check(struct hfc_multi *hctmp)
{
	unsigned char gpi2;

	gpi2 = HFC_inb(hctmp, R_GPI_IN2);

	if ((gpi2 & 0x3) != 0x3)
		printk(KERN_DEBUG "Got interrupt 0x%x from VPM!\n", gpi2);
}
#endif 



static void
vpm_echocan_on(struct hfc_multi *hc, int ch, int taps)
{
	unsigned int timeslot;
	unsigned int unit;
	struct bchannel *bch = hc->chan[ch].bch;
#ifdef TXADJ
	int txadj = -4;
	struct sk_buff *skb;
#endif
	if (hc->chan[ch].protocol != ISDN_P_B_RAW)
		return;

	if (!bch)
		return;

#ifdef TXADJ
	skb = _alloc_mISDN_skb(PH_CONTROL_IND, HFC_VOL_CHANGE_TX,
			       sizeof(int), &txadj, GFP_ATOMIC);
	if (skb)
		recv_Bchannel_skb(bch, skb);
#endif

	timeslot = ((ch / 4) * 8) + ((ch % 4) * 4) + 1;
	unit = ch % 4;

	printk(KERN_NOTICE "vpm_echocan_on called taps [%d] on timeslot %d\n",
	       taps, timeslot);

	vpm_out(hc, unit, timeslot, 0x7e);
}

static void
vpm_echocan_off(struct hfc_multi *hc, int ch)
{
	unsigned int timeslot;
	unsigned int unit;
	struct bchannel *bch = hc->chan[ch].bch;
#ifdef TXADJ
	int txadj = 0;
	struct sk_buff *skb;
#endif

	if (hc->chan[ch].protocol != ISDN_P_B_RAW)
		return;

	if (!bch)
		return;

#ifdef TXADJ
	skb = _alloc_mISDN_skb(PH_CONTROL_IND, HFC_VOL_CHANGE_TX,
			       sizeof(int), &txadj, GFP_ATOMIC);
	if (skb)
		recv_Bchannel_skb(bch, skb);
#endif

	timeslot = ((ch / 4) * 8) + ((ch % 4) * 4) + 1;
	unit = ch % 4;

	printk(KERN_NOTICE "vpm_echocan_off called on timeslot %d\n",
	       timeslot);
	
	vpm_out(hc, unit, timeslot, 0x01);
}


static inline void
hfcmulti_resync(struct hfc_multi *locked, struct hfc_multi *newmaster, int rm)
{
	struct hfc_multi *hc, *next, *pcmmaster = NULL;
	void __iomem *plx_acc_32;
	u_int pv;
	u_long flags;

	spin_lock_irqsave(&HFClock, flags);
	spin_lock(&plx_lock); 

	if (debug & DEBUG_HFCMULTI_PLXSD)
		printk(KERN_DEBUG "%s: RESYNC(syncmaster=0x%p)\n",
		       __func__, syncmaster);

	
	if (newmaster) {
		if (debug & DEBUG_HFCMULTI_PLXSD)
			printk(KERN_DEBUG "using provided controller\n");
	} else {
		list_for_each_entry_safe(hc, next, &HFClist, list) {
			if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
				if (hc->syncronized) {
					newmaster = hc;
					break;
				}
			}
		}
	}

	
	list_for_each_entry_safe(hc, next, &HFClist, list) {
		if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
			plx_acc_32 = hc->plx_membase + PLX_GPIOC;
			pv = readl(plx_acc_32);
			pv &= ~PLX_SYNC_O_EN;
			writel(pv, plx_acc_32);
			if (test_bit(HFC_CHIP_PCM_MASTER, &hc->chip)) {
				pcmmaster = hc;
				if (hc->ctype == HFC_TYPE_E1) {
					if (debug & DEBUG_HFCMULTI_PLXSD)
						printk(KERN_DEBUG
						       "Schedule SYNC_I\n");
					hc->e1_resync |= 1; 
				}
			}
		}
	}

	if (newmaster) {
		hc = newmaster;
		if (debug & DEBUG_HFCMULTI_PLXSD)
			printk(KERN_DEBUG "id=%d (0x%p) = syncronized with "
			       "interface.\n", hc->id, hc);
		
		plx_acc_32 = hc->plx_membase + PLX_GPIOC;
		pv = readl(plx_acc_32);
		pv |= PLX_SYNC_O_EN;
		writel(pv, plx_acc_32);
		
		if (hc->ctype == HFC_TYPE_E1
		    && !test_bit(HFC_CHIP_RX_SYNC, &hc->chip)) {
			if (debug & DEBUG_HFCMULTI_PLXSD)
				printk(KERN_DEBUG "Schedule jatt PLL\n");
			hc->e1_resync |= 2; 
		}
	} else {
		if (pcmmaster) {
			hc = pcmmaster;
			if (debug & DEBUG_HFCMULTI_PLXSD)
				printk(KERN_DEBUG
				       "id=%d (0x%p) = PCM master syncronized "
				       "with QUARTZ\n", hc->id, hc);
			if (hc->ctype == HFC_TYPE_E1) {
				if (debug & DEBUG_HFCMULTI_PLXSD)
					printk(KERN_DEBUG
					       "Schedule QUARTZ for HFC-E1\n");
				hc->e1_resync |= 4; 
			} else {
				if (debug & DEBUG_HFCMULTI_PLXSD)
					printk(KERN_DEBUG
					       "QUARTZ is automatically "
					       "enabled by HFC-%dS\n", hc->ctype);
			}
			plx_acc_32 = hc->plx_membase + PLX_GPIOC;
			pv = readl(plx_acc_32);
			pv |= PLX_SYNC_O_EN;
			writel(pv, plx_acc_32);
		} else
			if (!rm)
				printk(KERN_ERR "%s no pcm master, this MUST "
				       "not happen!\n", __func__);
	}
	syncmaster = newmaster;

	spin_unlock(&plx_lock);
	spin_unlock_irqrestore(&HFClock, flags);
}

inline void
plxsd_checksync(struct hfc_multi *hc, int rm)
{
	if (hc->syncronized) {
		if (syncmaster == NULL) {
			if (debug & DEBUG_HFCMULTI_PLXSD)
				printk(KERN_DEBUG "%s: GOT sync on card %d"
				       " (id=%d)\n", __func__, hc->id + 1,
				       hc->id);
			hfcmulti_resync(hc, hc, rm);
		}
	} else {
		if (syncmaster == hc) {
			if (debug & DEBUG_HFCMULTI_PLXSD)
				printk(KERN_DEBUG "%s: LOST sync on card %d"
				       " (id=%d)\n", __func__, hc->id + 1,
				       hc->id);
			hfcmulti_resync(hc, NULL, rm);
		}
	}
}


static void
release_io_hfcmulti(struct hfc_multi *hc)
{
	void __iomem *plx_acc_32;
	u_int	pv;
	u_long	plx_flags;

	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: entered\n", __func__);

	
	hc->hw.r_cirm |= V_SRES;
	HFC_outb(hc, R_CIRM, hc->hw.r_cirm);
	udelay(1000);
	hc->hw.r_cirm &= ~V_SRES;
	HFC_outb(hc, R_CIRM, hc->hw.r_cirm);
	udelay(1000); 

	
	if (test_bit(HFC_CHIP_PLXSD, &hc->chip) && hc->plx_membase) {
		if (debug & DEBUG_HFCMULTI_PLXSD)
			printk(KERN_DEBUG "%s: release PLXSD card %d\n",
			       __func__, hc->id + 1);
		spin_lock_irqsave(&plx_lock, plx_flags);
		plx_acc_32 = hc->plx_membase + PLX_GPIOC;
		writel(PLX_GPIOC_INIT, plx_acc_32);
		pv = readl(plx_acc_32);
		
		pv &= ~PLX_TERM_ON;
		
		pv |= PLX_SLAVE_EN_N;
		pv &= ~PLX_MASTER_EN;
		pv &= ~PLX_SYNC_O_EN;
		
		pv &= ~PLX_DSP_RES_N;
		writel(pv, plx_acc_32);
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: PCM off: PLX_GPIO=%x\n",
			       __func__, pv);
		spin_unlock_irqrestore(&plx_lock, plx_flags);
	}

	
	test_and_clear_bit(HFC_CHIP_PLXSD, &hc->chip); 
	if (hc->pci_dev)
		pci_write_config_word(hc->pci_dev, PCI_COMMAND, 0);
	if (hc->pci_membase)
		iounmap(hc->pci_membase);
	if (hc->plx_membase)
		iounmap(hc->plx_membase);
	if (hc->pci_iobase)
		release_region(hc->pci_iobase, 8);
	if (hc->xhfc_membase)
		iounmap((void *)hc->xhfc_membase);

	if (hc->pci_dev) {
		pci_disable_device(hc->pci_dev);
		pci_set_drvdata(hc->pci_dev, NULL);
	}
	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: done\n", __func__);
}


static int
init_chip(struct hfc_multi *hc)
{
	u_long			flags, val, val2 = 0, rev;
	int			i, err = 0;
	u_char			r_conf_en, rval;
	void __iomem		*plx_acc_32;
	u_int			pv;
	u_long			plx_flags, hfc_flags;
	int			plx_count;
	struct hfc_multi	*pos, *next, *plx_last_hc;

	spin_lock_irqsave(&hc->lock, flags);
	
	memset(&hc->hw, 0, sizeof(struct hfcm_hw));

	
	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: entered\n", __func__);
	val = HFC_inb(hc, R_CHIP_ID);
	if ((val >> 4) != 0x8 && (val >> 4) != 0xc && (val >> 4) != 0xe &&
	    (val >> 1) != 0x31) {
		printk(KERN_INFO "HFC_multi: unknown CHIP_ID:%x\n", (u_int)val);
		err = -EIO;
		goto out;
	}
	rev = HFC_inb(hc, R_CHIP_RV);
	printk(KERN_INFO
	       "HFC_multi: detected HFC with chip ID=0x%lx revision=%ld%s\n",
	       val, rev, (rev == 0 && (hc->ctype != HFC_TYPE_XHFC)) ?
	       " (old FIFO handling)" : "");
	if (hc->ctype != HFC_TYPE_XHFC && rev == 0) {
		test_and_set_bit(HFC_CHIP_REVISION0, &hc->chip);
		printk(KERN_WARNING
		       "HFC_multi: NOTE: Your chip is revision 0, "
		       "ask Cologne Chip for update. Newer chips "
		       "have a better FIFO handling. Old chips "
		       "still work but may have slightly lower "
		       "HDLC transmit performance.\n");
	}
	if (rev > 1) {
		printk(KERN_WARNING "HFC_multi: WARNING: This driver doesn't "
		       "consider chip revision = %ld. The chip / "
		       "bridge may not work.\n", rev);
	}

	
	hc->Flen = 0x10;
	hc->Zmin = 0x80;
	hc->Zlen = 384;
	hc->DTMFbase = 0x1000;
	if (test_bit(HFC_CHIP_EXRAM_128, &hc->chip)) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: changing to 128K extenal RAM\n",
			       __func__);
		hc->hw.r_ctrl |= V_EXT_RAM;
		hc->hw.r_ram_sz = 1;
		hc->Flen = 0x20;
		hc->Zmin = 0xc0;
		hc->Zlen = 1856;
		hc->DTMFbase = 0x2000;
	}
	if (test_bit(HFC_CHIP_EXRAM_512, &hc->chip)) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: changing to 512K extenal RAM\n",
			       __func__);
		hc->hw.r_ctrl |= V_EXT_RAM;
		hc->hw.r_ram_sz = 2;
		hc->Flen = 0x20;
		hc->Zmin = 0xc0;
		hc->Zlen = 8000;
		hc->DTMFbase = 0x2000;
	}
	if (hc->ctype == HFC_TYPE_XHFC) {
		hc->Flen = 0x8;
		hc->Zmin = 0x0;
		hc->Zlen = 64;
		hc->DTMFbase = 0x0;
	}
	hc->max_trans = poll << 1;
	if (hc->max_trans > hc->Zlen)
		hc->max_trans = hc->Zlen;

	
	if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
		if (debug & DEBUG_HFCMULTI_PLXSD)
			printk(KERN_DEBUG "%s: initializing PLXSD card %d\n",
			       __func__, hc->id + 1);
		spin_lock_irqsave(&plx_lock, plx_flags);
		plx_acc_32 = hc->plx_membase + PLX_GPIOC;
		writel(PLX_GPIOC_INIT, plx_acc_32);
		pv = readl(plx_acc_32);
		
		pv |= PLX_TERM_ON; 
		
		pv |= PLX_SLAVE_EN_N;
		pv &= ~PLX_MASTER_EN;
		pv &= ~PLX_SYNC_O_EN;
		
		pv &= ~PLX_DSP_RES_N;
		writel(pv, plx_acc_32);
		spin_unlock_irqrestore(&plx_lock, plx_flags);
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: slave/term: PLX_GPIO=%x\n",
			       __func__, pv);
		spin_lock_irqsave(&HFClock, hfc_flags);
		plx_count = 0;
		plx_last_hc = NULL;
		list_for_each_entry_safe(pos, next, &HFClist, list) {
			if (test_bit(HFC_CHIP_PLXSD, &pos->chip)) {
				plx_count++;
				if (pos != hc)
					plx_last_hc = pos;
			}
		}
		if (plx_count >= 3) {
			if (debug & DEBUG_HFCMULTI_PLXSD)
				printk(KERN_DEBUG "%s: card %d is between, so "
				       "we disable termination\n",
				       __func__, plx_last_hc->id + 1);
			spin_lock_irqsave(&plx_lock, plx_flags);
			plx_acc_32 = plx_last_hc->plx_membase + PLX_GPIOC;
			pv = readl(plx_acc_32);
			pv &= ~PLX_TERM_ON;
			writel(pv, plx_acc_32);
			spin_unlock_irqrestore(&plx_lock, plx_flags);
			if (debug & DEBUG_HFCMULTI_INIT)
				printk(KERN_DEBUG
				       "%s: term off: PLX_GPIO=%x\n",
				       __func__, pv);
		}
		spin_unlock_irqrestore(&HFClock, hfc_flags);
		hc->hw.r_pcm_md0 = V_F0_LEN; 
	}

	if (test_bit(HFC_CHIP_EMBSD, &hc->chip))
		hc->hw.r_pcm_md0 = V_F0_LEN; 

	
	if (!test_bit(HFC_CHIP_REVISION0, &hc->chip))
		hc->hw.r_ram_sz |= V_FZ_MD;

	
	if (test_bit(HFC_CHIP_PCM_SLAVE, &hc->chip)) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: setting PCM into slave mode\n",
			       __func__);
	} else
		if (test_bit(HFC_CHIP_PCM_MASTER, &hc->chip) && !plxsd_master) {
			if (debug & DEBUG_HFCMULTI_INIT)
				printk(KERN_DEBUG "%s: setting PCM into master mode\n",
				       __func__);
			hc->hw.r_pcm_md0 |= V_PCM_MD;
		} else {
			if (debug & DEBUG_HFCMULTI_INIT)
				printk(KERN_DEBUG "%s: performing PCM auto detect\n",
				       __func__);
		}

	
	HFC_outb(hc, R_CTRL, hc->hw.r_ctrl);
	if (hc->ctype == HFC_TYPE_XHFC)
		HFC_outb(hc, 0x0C ,
			 0x11 );
	else
		HFC_outb(hc, R_RAM_SZ, hc->hw.r_ram_sz);
	HFC_outb(hc, R_FIFO_MD, 0);
	if (hc->ctype == HFC_TYPE_XHFC)
		hc->hw.r_cirm = V_SRES | V_HFCRES | V_PCMRES | V_STRES;
	else
		hc->hw.r_cirm = V_SRES | V_HFCRES | V_PCMRES | V_STRES
			| V_RLD_EPR;
	HFC_outb(hc, R_CIRM, hc->hw.r_cirm);
	udelay(100);
	hc->hw.r_cirm = 0;
	HFC_outb(hc, R_CIRM, hc->hw.r_cirm);
	udelay(100);
	if (hc->ctype != HFC_TYPE_XHFC)
		HFC_outb(hc, R_RAM_SZ, hc->hw.r_ram_sz);

	
	if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
		spin_lock_irqsave(&plx_lock, plx_flags);
		plx_acc_32 = hc->plx_membase + PLX_GPIOC;
		pv = readl(plx_acc_32);
		
		if (hc->hw.r_pcm_md0 & V_PCM_MD) {
			pv |= PLX_MASTER_EN | PLX_SLAVE_EN_N;
			pv |= PLX_SYNC_O_EN;
			if (debug & DEBUG_HFCMULTI_INIT)
				printk(KERN_DEBUG "%s: master: PLX_GPIO=%x\n",
				       __func__, pv);
		} else {
			pv &= ~(PLX_MASTER_EN | PLX_SLAVE_EN_N);
			pv &= ~PLX_SYNC_O_EN;
			if (debug & DEBUG_HFCMULTI_INIT)
				printk(KERN_DEBUG "%s: slave: PLX_GPIO=%x\n",
				       __func__, pv);
		}
		writel(pv, plx_acc_32);
		spin_unlock_irqrestore(&plx_lock, plx_flags);
	}

	
	HFC_outb(hc, R_PCM_MD0, hc->hw.r_pcm_md0 | 0x90);
	if (hc->slots == 32)
		HFC_outb(hc, R_PCM_MD1, 0x00);
	if (hc->slots == 64)
		HFC_outb(hc, R_PCM_MD1, 0x10);
	if (hc->slots == 128)
		HFC_outb(hc, R_PCM_MD1, 0x20);
	HFC_outb(hc, R_PCM_MD0, hc->hw.r_pcm_md0 | 0xa0);
	if (test_bit(HFC_CHIP_PLXSD, &hc->chip))
		HFC_outb(hc, R_PCM_MD2, V_SYNC_SRC); 
	else if (test_bit(HFC_CHIP_EMBSD, &hc->chip))
		HFC_outb(hc, R_PCM_MD2, 0x10); 
	else
		HFC_outb(hc, R_PCM_MD2, 0x00); 
	HFC_outb(hc, R_PCM_MD0, hc->hw.r_pcm_md0 | 0x00);
	for (i = 0; i < 256; i++) {
		HFC_outb_nodebug(hc, R_SLOT, i);
		HFC_outb_nodebug(hc, A_SL_CFG, 0);
		if (hc->ctype != HFC_TYPE_XHFC)
			HFC_outb_nodebug(hc, A_CONF, 0);
		hc->slot_owner[i] = -1;
	}

	
	if (test_bit(HFC_CHIP_CLOCK2, &hc->chip)) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG
			       "%s: setting double clock\n", __func__);
		HFC_outb(hc, R_BRG_PCM_CFG, V_PCM_CLK);
	}

	if (test_bit(HFC_CHIP_EMBSD, &hc->chip))
		HFC_outb(hc, 0x02 , 0x40 );

	
	if (test_bit(HFC_CHIP_B410P, &hc->chip)) {
		printk(KERN_NOTICE "Setting GPIOs\n");
		HFC_outb(hc, R_GPIO_SEL, 0x30);
		HFC_outb(hc, R_GPIO_EN1, 0x3);
		udelay(1000);
		printk(KERN_NOTICE "calling vpm_init\n");
		vpm_init(hc);
	}

	
	val = HFC_inb(hc, R_F0_CNTL);
	val += HFC_inb(hc, R_F0_CNTH) << 8;
	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG
		       "HFC_multi F0_CNT %ld after reset\n", val);
	spin_unlock_irqrestore(&hc->lock, flags);
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout((HZ / 100) ? : 1); 
	spin_lock_irqsave(&hc->lock, flags);
	val2 = HFC_inb(hc, R_F0_CNTL);
	val2 += HFC_inb(hc, R_F0_CNTH) << 8;
	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG
		       "HFC_multi F0_CNT %ld after 10 ms (1st try)\n",
		       val2);
	if (val2 >= val + 8) { 
		
		if (test_bit(HFC_CHIP_PCM_MASTER, &hc->chip))
			printk(KERN_INFO "controller is PCM bus MASTER\n");
		else
			if (test_bit(HFC_CHIP_PCM_SLAVE, &hc->chip))
				printk(KERN_INFO "controller is PCM bus SLAVE\n");
			else {
				test_and_set_bit(HFC_CHIP_PCM_SLAVE, &hc->chip);
				printk(KERN_INFO "controller is PCM bus SLAVE "
				       "(auto detected)\n");
			}
	} else {
		
		if (test_bit(HFC_CHIP_PCM_MASTER, &hc->chip)) {
		controller_fail:
			printk(KERN_ERR "HFC_multi ERROR, getting no 125us "
			       "pulse. Seems that controller fails.\n");
			err = -EIO;
			goto out;
		}
		if (test_bit(HFC_CHIP_PCM_SLAVE, &hc->chip)) {
			printk(KERN_INFO "controller is PCM bus SLAVE "
			       "(ignoring missing PCM clock)\n");
		} else {
			
			if (test_bit(HFC_CHIP_PLXSD, &hc->chip)
			    && plxsd_master) {
				printk(KERN_ERR "HFC_multi ERROR, no clock "
				       "on another Speech Design card found. "
				       "Please be sure to connect PCM cable.\n");
				err = -EIO;
				goto out;
			}
			
			if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
				spin_lock_irqsave(&plx_lock, plx_flags);
				plx_acc_32 = hc->plx_membase + PLX_GPIOC;
				pv = readl(plx_acc_32);
				pv |= PLX_MASTER_EN | PLX_SLAVE_EN_N;
				pv |= PLX_SYNC_O_EN;
				writel(pv, plx_acc_32);
				spin_unlock_irqrestore(&plx_lock, plx_flags);
				if (debug & DEBUG_HFCMULTI_INIT)
					printk(KERN_DEBUG "%s: master: "
					       "PLX_GPIO=%x\n", __func__, pv);
			}
			hc->hw.r_pcm_md0 |= V_PCM_MD;
			HFC_outb(hc, R_PCM_MD0, hc->hw.r_pcm_md0 | 0x00);
			spin_unlock_irqrestore(&hc->lock, flags);
			set_current_state(TASK_UNINTERRUPTIBLE);
			schedule_timeout((HZ / 100) ?: 1); 
			spin_lock_irqsave(&hc->lock, flags);
			val2 = HFC_inb(hc, R_F0_CNTL);
			val2 += HFC_inb(hc, R_F0_CNTH) << 8;
			if (debug & DEBUG_HFCMULTI_INIT)
				printk(KERN_DEBUG "HFC_multi F0_CNT %ld after "
				       "10 ms (2nd try)\n", val2);
			if (val2 >= val + 8) { 
				test_and_set_bit(HFC_CHIP_PCM_MASTER,
						 &hc->chip);
				printk(KERN_INFO "controller is PCM bus MASTER "
				       "(auto detected)\n");
			} else
				goto controller_fail;
		}
	}

	
	if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
		if (test_bit(HFC_CHIP_PCM_MASTER, &hc->chip))
			plxsd_master = 1;
		spin_lock_irqsave(&plx_lock, plx_flags);
		plx_acc_32 = hc->plx_membase + PLX_GPIOC;
		pv = readl(plx_acc_32);
		pv |=  PLX_DSP_RES_N;
		writel(pv, plx_acc_32);
		spin_unlock_irqrestore(&plx_lock, plx_flags);
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: reset off: PLX_GPIO=%x\n",
			       __func__, pv);
	}

	
	if (hc->pcm)
		printk(KERN_INFO "controller has given PCM BUS ID %d\n",
		       hc->pcm);
	else {
		if (test_bit(HFC_CHIP_PCM_MASTER, &hc->chip)
		    || test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
			PCM_cnt++; 
		}
		hc->pcm = PCM_cnt;
		printk(KERN_INFO "controller has PCM BUS ID %d "
		       "(auto selected)\n", hc->pcm);
	}

	
	HFC_outb(hc, R_TI_WD, poll_timer);
	hc->hw.r_irqmsk_misc |= V_TI_IRQMSK;

	
	if (hc->ctype == HFC_TYPE_E1)
		hc->hw.r_irqmsk_misc |= V_STA_IRQMSK;

	
	if (test_bit(HFC_CHIP_DTMF, &hc->chip)) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: enabling DTMF detection "
			       "for all B-channel\n", __func__);
		hc->hw.r_dtmf = V_DTMF_EN | V_DTMF_STOP;
		if (test_bit(HFC_CHIP_ULAW, &hc->chip))
			hc->hw.r_dtmf |= V_ULAW_SEL;
		HFC_outb(hc, R_DTMF_N, 102 - 1);
		hc->hw.r_irqmsk_misc |= V_DTMF_IRQMSK;
	}

	
	if (test_bit(HFC_CHIP_ULAW, &hc->chip))
		r_conf_en = V_CONF_EN | V_ULAW;
	else
		r_conf_en = V_CONF_EN;
	if (hc->ctype != HFC_TYPE_XHFC)
		HFC_outb(hc, R_CONF_EN, r_conf_en);

	
	switch (hc->leds) {
	case 1: 
		if (test_bit(HFC_CHIP_WATCHDOG, &hc->chip))
			HFC_outb(hc, R_GPIO_SEL, 0x32);
		else
			HFC_outb(hc, R_GPIO_SEL, 0x30);

		HFC_outb(hc, R_GPIO_EN1, 0x0f);
		HFC_outb(hc, R_GPIO_OUT1, 0x00);

		HFC_outb(hc, R_GPIO_EN0, V_GPIO_EN2 | V_GPIO_EN3);
		break;

	case 2: 
	case 3:
		HFC_outb(hc, R_GPIO_SEL, 0xf0);
		HFC_outb(hc, R_GPIO_EN1, 0xff);
		HFC_outb(hc, R_GPIO_OUT1, 0x00);
		break;
	}

	if (test_bit(HFC_CHIP_EMBSD, &hc->chip)) {
		hc->hw.r_st_sync = 0x10; 
		HFC_outb(hc, R_ST_SYNC, hc->hw.r_st_sync);
	}

	
	if (hc->masterclk >= 0) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: setting ST master clock "
			       "to port %d (0..%d)\n",
			       __func__, hc->masterclk, hc->ports - 1);
		hc->hw.r_st_sync |= (hc->masterclk | V_AUTO_SYNC);
		HFC_outb(hc, R_ST_SYNC, hc->hw.r_st_sync);
	}



	
	HFC_outb(hc, R_IRQMSK_MISC, hc->hw.r_irqmsk_misc);
	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "r_irqmsk_misc.2: 0x%x\n",
		       hc->hw.r_irqmsk_misc);

	
	HFC_outb(hc, R_RAM_ADDR0, 0);
	HFC_outb(hc, R_RAM_ADDR1, 0);
	HFC_outb(hc, R_RAM_ADDR2, 0);
	for (i = 0; i < 256; i++) {
		HFC_outb_nodebug(hc, R_RAM_ADDR0, i);
		HFC_outb_nodebug(hc, R_RAM_DATA, ((i * 3) & 0xff));
	}
	for (i = 0; i < 256; i++) {
		HFC_outb_nodebug(hc, R_RAM_ADDR0, i);
		HFC_inb_nodebug(hc, R_RAM_DATA);
		rval = HFC_inb_nodebug(hc, R_INT_DATA);
		if (rval != ((i * 3) & 0xff)) {
			printk(KERN_DEBUG
			       "addr:%x val:%x should:%x\n", i, rval,
			       (i * 3) & 0xff);
			err++;
		}
	}
	if (err) {
		printk(KERN_DEBUG "aborting - %d RAM access errors\n", err);
		err = -EIO;
		goto out;
	}

	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: done\n", __func__);
out:
	spin_unlock_irqrestore(&hc->lock, flags);
	return err;
}


static void
hfcmulti_watchdog(struct hfc_multi *hc)
{
	hc->wdcount++;

	if (hc->wdcount > 10) {
		hc->wdcount = 0;
		hc->wdbyte = hc->wdbyte == V_GPIO_OUT2 ?
			V_GPIO_OUT3 : V_GPIO_OUT2;

		
		HFC_outb(hc, R_GPIO_EN0, V_GPIO_EN2 | V_GPIO_EN3);
		HFC_outb(hc, R_GPIO_OUT0, hc->wdbyte);
	}
}



static void
hfcmulti_leds(struct hfc_multi *hc)
{
	unsigned long lled;
	unsigned long leddw;
	int i, state, active, leds;
	struct dchannel *dch;
	int led[4];

	hc->ledcount += poll;
	if (hc->ledcount > 4096) {
		hc->ledcount -= 4096;
		hc->ledstate = 0xAFFEAFFE;
	}

	switch (hc->leds) {
	case 1: 
		if (hc->chan[hc->dslot].sync != 2) { 
			if (hc->chan[hc->dslot].dch->dev.D.protocol
			    != ISDN_P_NT_E1) {
				led[0] = 1;
				led[1] = 1;
			} else if (hc->ledcount >> 11) {
				led[0] = 1;
				led[1] = 1;
			} else {
				led[0] = 0;
				led[1] = 0;
			}
			led[2] = 0;
			led[3] = 0;
		} else { 
			
			led[0] = 0;
			led[1] = 0;
			led[2] = 0;
			led[3] = 1;
		}
		leds = (led[0] | (led[1]<<2) | (led[2]<<1) | (led[3]<<3))^0xF;
		
		if (leds != (int)hc->ledstate) {
			HFC_outb_nodebug(hc, R_GPIO_OUT1, leds);
			hc->ledstate = leds;
		}
		break;

	case 2: 
		for (i = 0; i < 4; i++) {
			state = 0;
			active = -1;
			dch = hc->chan[(i << 2) | 2].dch;
			if (dch) {
				state = dch->state;
				if (dch->dev.D.protocol == ISDN_P_NT_S0)
					active = 3;
				else
					active = 7;
			}
			if (state) {
				if (state == active) {
					led[i] = 1; 
				} else
					if (dch->dev.D.protocol == ISDN_P_TE_S0)
						
						led[i] = 2;
					else
						if (hc->ledcount >> 11)
							
							led[i] = 2;
						else
							
							led[i] = 0;
			} else
				led[i] = 0; 
		}
		if (test_bit(HFC_CHIP_B410P, &hc->chip)) {
			leds = 0;
			for (i = 0; i < 4; i++) {
				if (led[i] == 1) {
					
					leds |= (0x2 << (i * 2));
				} else if (led[i] == 2) {
					
					leds |= (0x1 << (i * 2));
				}
			}
			if (leds != (int)hc->ledstate) {
				vpm_out(hc, 0, 0x1a8 + 3, leds);
				hc->ledstate = leds;
			}
		} else {
			leds = ((led[3] > 0) << 0) | ((led[1] > 0) << 1) |
				((led[0] > 0) << 2) | ((led[2] > 0) << 3) |
				((led[3] & 1) << 4) | ((led[1] & 1) << 5) |
				((led[0] & 1) << 6) | ((led[2] & 1) << 7);
			if (leds != (int)hc->ledstate) {
				HFC_outb_nodebug(hc, R_GPIO_EN1, leds & 0x0F);
				HFC_outb_nodebug(hc, R_GPIO_OUT1, leds >> 4);
				hc->ledstate = leds;
			}
		}
		break;

	case 3: 
		for (i = 0; i < 2; i++) {
			state = 0;
			active = -1;
			dch = hc->chan[(i << 2) | 2].dch;
			if (dch) {
				state = dch->state;
				if (dch->dev.D.protocol == ISDN_P_NT_S0)
					active = 3;
				else
					active = 7;
			}
			if (state) {
				if (state == active) {
					led[i] = 1; 
				} else
					if (dch->dev.D.protocol == ISDN_P_TE_S0)
						
						led[i] = 2;
					else
						if (hc->ledcount >> 11)
							
							led[i] = 2;
						else
							
							led[i] = 0;
			} else
				led[i] = 0; 
		}


		leds = (led[0] > 0) | ((led[1] > 0) << 1) | ((led[0]&1) << 2)
			| ((led[1]&1) << 3);
		if (leds != (int)hc->ledstate) {
			HFC_outb_nodebug(hc, R_GPIO_EN1,
					 ((led[0] > 0) << 2) | ((led[1] > 0) << 3));
			HFC_outb_nodebug(hc, R_GPIO_OUT1,
					 ((led[0] & 1) << 2) | ((led[1] & 1) << 3));
			hc->ledstate = leds;
		}
		break;
	case 8: 
		lled = 0;

		for (i = 0; i < 8; i++) {
			state = 0;
			active = -1;
			dch = hc->chan[(i << 2) | 2].dch;
			if (dch) {
				state = dch->state;
				if (dch->dev.D.protocol == ISDN_P_NT_S0)
					active = 3;
				else
					active = 7;
			}
			if (state) {
				if (state == active) {
					lled |= 0 << i;
				} else
					if (hc->ledcount >> 11)
						lled |= 0 << i;
					else
						lled |= 1 << i;
			} else
				lled |= 1 << i;
		}
		leddw = lled << 24 | lled << 16 | lled << 8 | lled;
		if (leddw != hc->ledstate) {
			
			HFC_outb_nodebug(hc, R_BRG_PCM_CFG, 1 | V_PCM_CLK);
			outw(0x4000, hc->pci_iobase + 4);
			outl(leddw, hc->pci_iobase);
			HFC_outb_nodebug(hc, R_BRG_PCM_CFG, V_PCM_CLK);
			hc->ledstate = leddw;
		}
		break;
	}
}

static void
hfcmulti_dtmf(struct hfc_multi *hc)
{
	s32		*coeff;
	u_int		mantissa;
	int		co, ch;
	struct bchannel	*bch = NULL;
	u8		exponent;
	int		dtmf = 0;
	int		addr;
	u16		w_float;
	struct sk_buff	*skb;
	struct mISDNhead *hh;

	if (debug & DEBUG_HFCMULTI_DTMF)
		printk(KERN_DEBUG "%s: dtmf detection irq\n", __func__);
	for (ch = 0; ch <= 31; ch++) {
		
		bch = hc->chan[ch].bch;
		if (!bch)
			continue;
		if (!hc->created[hc->chan[ch].port])
			continue;
		if (!test_bit(FLG_TRANSPARENT, &bch->Flags))
			continue;
		if (debug & DEBUG_HFCMULTI_DTMF)
			printk(KERN_DEBUG "%s: dtmf channel %d:",
			       __func__, ch);
		coeff = &(hc->chan[ch].coeff[hc->chan[ch].coeff_count * 16]);
		dtmf = 1;
		for (co = 0; co < 8; co++) {
			
			addr = hc->DTMFbase + ((co << 7) | (ch << 2));
			HFC_outb_nodebug(hc, R_RAM_ADDR0, addr);
			HFC_outb_nodebug(hc, R_RAM_ADDR1, addr >> 8);
			HFC_outb_nodebug(hc, R_RAM_ADDR2, (addr >> 16)
					 | V_ADDR_INC);
			w_float = HFC_inb_nodebug(hc, R_RAM_DATA);
			w_float |= (HFC_inb_nodebug(hc, R_RAM_DATA) << 8);
			if (debug & DEBUG_HFCMULTI_DTMF)
				printk(" %04x", w_float);

			
			mantissa = w_float & 0x0fff;
			if (w_float & 0x8000)
				mantissa |= 0xfffff000;
			exponent = (w_float >> 12) & 0x7;
			if (exponent) {
				mantissa ^= 0x1000;
				mantissa <<= (exponent - 1);
			}

			
			coeff[co << 1] = mantissa;

			
			w_float = HFC_inb_nodebug(hc, R_RAM_DATA);
			w_float |= (HFC_inb_nodebug(hc, R_RAM_DATA) << 8);
			if (debug & DEBUG_HFCMULTI_DTMF)
				printk(" %04x", w_float);

			
			mantissa = w_float & 0x0fff;
			if (w_float & 0x8000)
				mantissa |= 0xfffff000;
			exponent = (w_float >> 12) & 0x7;
			if (exponent) {
				mantissa ^= 0x1000;
				mantissa <<= (exponent - 1);
			}

			
			coeff[(co << 1) | 1] = mantissa;
		}
		if (debug & DEBUG_HFCMULTI_DTMF)
			printk(" DTMF ready %08x %08x %08x %08x "
			       "%08x %08x %08x %08x\n",
			       coeff[0], coeff[1], coeff[2], coeff[3],
			       coeff[4], coeff[5], coeff[6], coeff[7]);
		hc->chan[ch].coeff_count++;
		if (hc->chan[ch].coeff_count == 8) {
			hc->chan[ch].coeff_count = 0;
			skb = mI_alloc_skb(512, GFP_ATOMIC);
			if (!skb) {
				printk(KERN_DEBUG "%s: No memory for skb\n",
				       __func__);
				continue;
			}
			hh = mISDN_HEAD_P(skb);
			hh->prim = PH_CONTROL_IND;
			hh->id = DTMF_HFC_COEF;
			memcpy(skb_put(skb, 512), hc->chan[ch].coeff, 512);
			recv_Bchannel_skb(bch, skb);
		}
	}

	
	hc->dtmf = dtmf;
	if (dtmf)
		HFC_outb_nodebug(hc, R_DTMF, hc->hw.r_dtmf | V_RST_DTMF);
}



static void
hfcmulti_tx(struct hfc_multi *hc, int ch)
{
	int i, ii, temp, len = 0;
	int Zspace, z1, z2; 
	int Fspace, f1, f2;
	u_char *d;
	int *txpending, slot_tx;
	struct	bchannel *bch;
	struct  dchannel *dch;
	struct  sk_buff **sp = NULL;
	int *idxp;

	bch = hc->chan[ch].bch;
	dch = hc->chan[ch].dch;
	if ((!dch) && (!bch))
		return;

	txpending = &hc->chan[ch].txpending;
	slot_tx = hc->chan[ch].slot_tx;
	if (dch) {
		if (!test_bit(FLG_ACTIVE, &dch->Flags))
			return;
		sp = &dch->tx_skb;
		idxp = &dch->tx_idx;
	} else {
		if (!test_bit(FLG_ACTIVE, &bch->Flags))
			return;
		sp = &bch->tx_skb;
		idxp = &bch->tx_idx;
	}
	if (*sp)
		len = (*sp)->len;

	if ((!len) && *txpending != 1)
		return; 

	if (test_bit(HFC_CHIP_B410P, &hc->chip) &&
	    (hc->chan[ch].protocol == ISDN_P_B_RAW) &&
	    (hc->chan[ch].slot_rx < 0) &&
	    (hc->chan[ch].slot_tx < 0))
		HFC_outb_nodebug(hc, R_FIFO, 0x20 | (ch << 1));
	else
		HFC_outb_nodebug(hc, R_FIFO, ch << 1);
	HFC_wait_nodebug(hc);

	if (*txpending == 2) {
		
		HFC_outb_nodebug(hc, R_INC_RES_FIFO, V_RES_F);
		HFC_wait_nodebug(hc);
		HFC_outb(hc, A_SUBCH_CFG, 0);
		*txpending = 1;
	}
next_frame:
	if (dch || test_bit(FLG_HDLC, &bch->Flags)) {
		f1 = HFC_inb_nodebug(hc, A_F1);
		f2 = HFC_inb_nodebug(hc, A_F2);
		while (f2 != (temp = HFC_inb_nodebug(hc, A_F2))) {
			if (debug & DEBUG_HFCMULTI_FIFO)
				printk(KERN_DEBUG
				       "%s(card %d): reread f2 because %d!=%d\n",
				       __func__, hc->id + 1, temp, f2);
			f2 = temp; 
		}
		Fspace = f2 - f1 - 1;
		if (Fspace < 0)
			Fspace += hc->Flen;
		if (test_bit(HFC_CHIP_REVISION0, &hc->chip)) {
			if (f1 != f2)
				Fspace = 0;
			else
				Fspace = 1;
		}
		
		if (hc->ctype != HFC_TYPE_E1 && dch) {
			if (f1 != f2)
				Fspace = 0;
		}
		
		if (Fspace == 0)
			return;
	}
	z1 = HFC_inw_nodebug(hc, A_Z1) - hc->Zmin;
	z2 = HFC_inw_nodebug(hc, A_Z2) - hc->Zmin;
	while (z2 != (temp = (HFC_inw_nodebug(hc, A_Z2) - hc->Zmin))) {
		if (debug & DEBUG_HFCMULTI_FIFO)
			printk(KERN_DEBUG "%s(card %d): reread z2 because "
			       "%d!=%d\n", __func__, hc->id + 1, temp, z2);
		z2 = temp; 
	}
	hc->chan[ch].Zfill = z1 - z2;
	if (hc->chan[ch].Zfill < 0)
		hc->chan[ch].Zfill += hc->Zlen;
	Zspace = z2 - z1;
	if (Zspace <= 0)
		Zspace += hc->Zlen;
	Zspace -= 4; 
	
	if (bch && test_bit(FLG_TRANSPARENT, &bch->Flags))
		Zspace = Zspace - hc->Zlen + hc->max_trans;
	if (Zspace <= 0) 
		return;

	
	if (!len) {
		if (z1 == z2) { 
			
			if (bch && (!test_bit(FLG_HDLC, &bch->Flags)) &&
			    *txpending && slot_tx >= 0) {
				if (debug & DEBUG_HFCMULTI_MODE)
					printk(KERN_DEBUG
					       "%s: reconnecting PCM due to no "
					       "more FIFO data: channel %d "
					       "slot_tx %d\n",
					       __func__, ch, slot_tx);
				
				if (hc->ctype == HFC_TYPE_XHFC)
					HFC_outb(hc, A_CON_HDLC, 0xc0
						 | 0x07 << 2 | V_HDLC_TRP | V_IFF);
				
				else
					HFC_outb(hc, A_CON_HDLC, 0xc0 | 0x00 |
						 V_HDLC_TRP | V_IFF);
				HFC_outb_nodebug(hc, R_FIFO, ch << 1 | 1);
				HFC_wait_nodebug(hc);
				if (hc->ctype == HFC_TYPE_XHFC)
					HFC_outb(hc, A_CON_HDLC, 0xc0
						 | 0x07 << 2 | V_HDLC_TRP | V_IFF);
				
				else
					HFC_outb(hc, A_CON_HDLC, 0xc0 | 0x00 |
						 V_HDLC_TRP | V_IFF);
				HFC_outb_nodebug(hc, R_FIFO, ch << 1);
				HFC_wait_nodebug(hc);
			}
			*txpending = 0;
		}
		return; 
	}

	
	if (bch && test_bit(FLG_FILLEMPTY, &bch->Flags)
	    && !test_bit(FLG_HDLC, &bch->Flags) && z2 == z1) {
		if (debug & DEBUG_HFCMULTI_FILL)
			printk(KERN_DEBUG "%s: buffer empty, so we have "
			       "underrun\n", __func__);
		
		hc->write_fifo(hc, hc->silence_data, poll >> 1);
		Zspace -= (poll >> 1);
	}

	
	if (bch && (!test_bit(FLG_HDLC, &bch->Flags)) && (!*txpending)
	    && slot_tx >= 0) {
		if (debug & DEBUG_HFCMULTI_MODE)
			printk(KERN_DEBUG "%s: disconnecting PCM due to "
			       "FIFO data: channel %d slot_tx %d\n",
			       __func__, ch, slot_tx);
		
		if (hc->ctype == HFC_TYPE_XHFC)
			HFC_outb(hc, A_CON_HDLC, 0x80
				 | 0x07 << 2 | V_HDLC_TRP | V_IFF);
		
		else
			HFC_outb(hc, A_CON_HDLC, 0x80 | 0x00 |
				 V_HDLC_TRP | V_IFF);
		HFC_outb_nodebug(hc, R_FIFO, ch << 1 | 1);
		HFC_wait_nodebug(hc);
		if (hc->ctype == HFC_TYPE_XHFC)
			HFC_outb(hc, A_CON_HDLC, 0x80
				 | 0x07 << 2 | V_HDLC_TRP | V_IFF);
		
		else
			HFC_outb(hc, A_CON_HDLC, 0x80 | 0x00 |
				 V_HDLC_TRP | V_IFF);
		HFC_outb_nodebug(hc, R_FIFO, ch << 1);
		HFC_wait_nodebug(hc);
	}
	*txpending = 1;

	
	hc->activity[hc->chan[ch].port] = 1;

	
	ii = len;
	if (dch || test_bit(FLG_HDLC, &bch->Flags))
		temp = 1;
	else
		temp = 0;
	i = *idxp;
	d = (*sp)->data + i;
	if (ii - i > Zspace)
		ii = Zspace + i;
	if (debug & DEBUG_HFCMULTI_FIFO)
		printk(KERN_DEBUG "%s(card %d): fifo(%d) has %d bytes space "
		       "left (z1=%04x, z2=%04x) sending %d of %d bytes %s\n",
		       __func__, hc->id + 1, ch, Zspace, z1, z2, ii-i, len-i,
		       temp ? "HDLC" : "TRANS");

	
	hc->write_fifo(hc, d, ii - i);
	hc->chan[ch].Zfill += ii - i;
	*idxp = ii;

	/* if not all data has been written */
	if (ii != len) {
		
		return;
	}

	/* if all data has been written, terminate frame */
	if (dch || test_bit(FLG_HDLC, &bch->Flags)) {
		
		HFC_outb_nodebug(hc, R_INC_RES_FIFO, V_INC_F);
		HFC_wait_nodebug(hc);
	}

	
	if (bch && test_bit(FLG_TRANSPARENT, &bch->Flags))
		confirm_Bsend(bch);

	
	dev_kfree_skb(*sp);
	if (bch && get_next_bframe(bch)) { 
		len = (*sp)->len;
		goto next_frame;
	}
	if (dch && get_next_dframe(dch)) {
		len = (*sp)->len;
		goto next_frame;
	}

	if (bch && test_bit(FLG_TRANSPARENT, &bch->Flags))
		HFC_outb_nodebug(hc, A_FIFO_DATA0_NOINC, hc->silence);
}


static void
hfcmulti_rx(struct hfc_multi *hc, int ch)
{
	int temp;
	int Zsize, z1, z2 = 0; 
	int f1 = 0, f2 = 0; 
	int again = 0;
	struct	bchannel *bch;
	struct  dchannel *dch;
	struct sk_buff	*skb, **sp = NULL;
	int	maxlen;

	bch = hc->chan[ch].bch;
	dch = hc->chan[ch].dch;
	if ((!dch) && (!bch))
		return;
	if (dch) {
		if (!test_bit(FLG_ACTIVE, &dch->Flags))
			return;
		sp = &dch->rx_skb;
		maxlen = dch->maxlen;
	} else {
		if (!test_bit(FLG_ACTIVE, &bch->Flags))
			return;
		sp = &bch->rx_skb;
		maxlen = bch->maxlen;
	}
next_frame:
	/* on first AND before getting next valid frame, R_FIFO must be written
	   to. */
	if (test_bit(HFC_CHIP_B410P, &hc->chip) &&
	    (hc->chan[ch].protocol == ISDN_P_B_RAW) &&
	    (hc->chan[ch].slot_rx < 0) &&
	    (hc->chan[ch].slot_tx < 0))
		HFC_outb_nodebug(hc, R_FIFO, 0x20 | (ch << 1) | 1);
	else
		HFC_outb_nodebug(hc, R_FIFO, (ch << 1) | 1);
	HFC_wait_nodebug(hc);

	
	if (hc->chan[ch].rx_off)
		return;

	if (dch || test_bit(FLG_HDLC, &bch->Flags)) {
		f1 = HFC_inb_nodebug(hc, A_F1);
		while (f1 != (temp = HFC_inb_nodebug(hc, A_F1))) {
			if (debug & DEBUG_HFCMULTI_FIFO)
				printk(KERN_DEBUG
				       "%s(card %d): reread f1 because %d!=%d\n",
				       __func__, hc->id + 1, temp, f1);
			f1 = temp; 
		}
		f2 = HFC_inb_nodebug(hc, A_F2);
	}
	z1 = HFC_inw_nodebug(hc, A_Z1) - hc->Zmin;
	while (z1 != (temp = (HFC_inw_nodebug(hc, A_Z1) - hc->Zmin))) {
		if (debug & DEBUG_HFCMULTI_FIFO)
			printk(KERN_DEBUG "%s(card %d): reread z2 because "
			       "%d!=%d\n", __func__, hc->id + 1, temp, z2);
		z1 = temp; 
	}
	z2 = HFC_inw_nodebug(hc, A_Z2) - hc->Zmin;
	Zsize = z1 - z2;
	if ((dch || test_bit(FLG_HDLC, &bch->Flags)) && f1 != f2)
		
		Zsize++;
	if (Zsize < 0)
		Zsize += hc->Zlen;
	
	if (Zsize <= 0)
		return;

	if (*sp == NULL) {
		*sp = mI_alloc_skb(maxlen + 3, GFP_ATOMIC);
		if (*sp == NULL) {
			printk(KERN_DEBUG "%s: No mem for rx_skb\n",
			       __func__);
			return;
		}
	}
	
	hc->activity[hc->chan[ch].port] = 1;

	
	if (dch || test_bit(FLG_HDLC, &bch->Flags)) {
		if (debug & DEBUG_HFCMULTI_FIFO)
			printk(KERN_DEBUG "%s(card %d): fifo(%d) reading %d "
			       "bytes (z1=%04x, z2=%04x) HDLC %s (f1=%d, f2=%d) "
			       "got=%d (again %d)\n", __func__, hc->id + 1, ch,
			       Zsize, z1, z2, (f1 == f2) ? "fragment" : "COMPLETE",
			       f1, f2, Zsize + (*sp)->len, again);
		
		if ((Zsize + (*sp)->len) > (maxlen + 3)) {
			if (debug & DEBUG_HFCMULTI_FIFO)
				printk(KERN_DEBUG
				       "%s(card %d): hdlc-frame too large.\n",
				       __func__, hc->id + 1);
			skb_trim(*sp, 0);
			HFC_outb_nodebug(hc, R_INC_RES_FIFO, V_RES_F);
			HFC_wait_nodebug(hc);
			return;
		}

		hc->read_fifo(hc, skb_put(*sp, Zsize), Zsize);

		if (f1 != f2) {
			
			HFC_outb_nodebug(hc, R_INC_RES_FIFO, V_INC_F);
			HFC_wait_nodebug(hc);
			
			if ((*sp)->len < 4) {
				if (debug & DEBUG_HFCMULTI_FIFO)
					printk(KERN_DEBUG
					       "%s(card %d): Frame below minimum "
					       "size\n", __func__, hc->id + 1);
				skb_trim(*sp, 0);
				goto next_frame;
			}
			
			if ((*sp)->data[(*sp)->len - 1]) {
				if (debug & DEBUG_HFCMULTI_CRC)
					printk(KERN_DEBUG
					       "%s: CRC-error\n", __func__);
				skb_trim(*sp, 0);
				goto next_frame;
			}
			skb_trim(*sp, (*sp)->len - 3);
			if ((*sp)->len < MISDN_COPY_SIZE) {
				skb = *sp;
				*sp = mI_alloc_skb(skb->len, GFP_ATOMIC);
				if (*sp) {
					memcpy(skb_put(*sp, skb->len),
					       skb->data, skb->len);
					skb_trim(skb, 0);
				} else {
					printk(KERN_DEBUG "%s: No mem\n",
					       __func__);
					*sp = skb;
					skb = NULL;
				}
			} else {
				skb = NULL;
			}
			if (debug & DEBUG_HFCMULTI_FIFO) {
				printk(KERN_DEBUG "%s(card %d):",
				       __func__, hc->id + 1);
				temp = 0;
				while (temp < (*sp)->len)
					printk(" %02x", (*sp)->data[temp++]);
				printk("\n");
			}
			if (dch)
				recv_Dchannel(dch);
			else
				recv_Bchannel(bch, MISDN_ID_ANY);
			*sp = skb;
			again++;
			goto next_frame;
		}
		
	} else {
		
		if (Zsize > skb_tailroom(*sp))
			Zsize = skb_tailroom(*sp);
		hc->read_fifo(hc, skb_put(*sp, Zsize), Zsize);
		if (((*sp)->len) < MISDN_COPY_SIZE) {
			skb = *sp;
			*sp = mI_alloc_skb(skb->len, GFP_ATOMIC);
			if (*sp) {
				memcpy(skb_put(*sp, skb->len),
				       skb->data, skb->len);
				skb_trim(skb, 0);
			} else {
				printk(KERN_DEBUG "%s: No mem\n", __func__);
				*sp = skb;
				skb = NULL;
			}
		} else {
			skb = NULL;
		}
		if (debug & DEBUG_HFCMULTI_FIFO)
			printk(KERN_DEBUG
			       "%s(card %d): fifo(%d) reading %d bytes "
			       "(z1=%04x, z2=%04x) TRANS\n",
			       __func__, hc->id + 1, ch, Zsize, z1, z2);
		
		recv_Bchannel(bch, hc->chan[ch].Zfill);
		*sp = skb;
	}
}


static void
signal_state_up(struct dchannel *dch, int info, char *msg)
{
	struct sk_buff	*skb;
	int		id, data = info;

	if (debug & DEBUG_HFCMULTI_STATE)
		printk(KERN_DEBUG "%s: %s\n", __func__, msg);

	id = TEI_SAPI | (GROUP_TEI << 8); 

	skb = _alloc_mISDN_skb(MPH_INFORMATION_IND, id, sizeof(data), &data,
			       GFP_ATOMIC);
	if (!skb)
		return;
	recv_Dchannel_skb(dch, skb);
}

static inline void
handle_timer_irq(struct hfc_multi *hc)
{
	int		ch, temp;
	struct dchannel	*dch;
	u_long		flags;

	
	if (hc->e1_resync) {
		
		spin_lock_irqsave(&HFClock, flags);
		if (hc->e1_resync & 1) {
			if (debug & DEBUG_HFCMULTI_PLXSD)
				printk(KERN_DEBUG "Enable SYNC_I\n");
			HFC_outb(hc, R_SYNC_CTRL, V_EXT_CLK_SYNC);
			
			if (test_bit(HFC_CHIP_RX_SYNC, &hc->chip))
				HFC_outb(hc, R_SYNC_OUT, V_SYNC_E1_RX);
		}
		if (hc->e1_resync & 2) {
			if (debug & DEBUG_HFCMULTI_PLXSD)
				printk(KERN_DEBUG "Enable jatt PLL\n");
			HFC_outb(hc, R_SYNC_CTRL, V_SYNC_OFFS);
		}
		if (hc->e1_resync & 4) {
			if (debug & DEBUG_HFCMULTI_PLXSD)
				printk(KERN_DEBUG
				       "Enable QUARTZ for HFC-E1\n");
			
			HFC_outb(hc, R_SYNC_CTRL, V_EXT_CLK_SYNC
				 | V_JATT_OFF);
			
			HFC_outb(hc, R_SYNC_OUT, 0);
		}
		hc->e1_resync = 0;
		spin_unlock_irqrestore(&HFClock, flags);
	}

	if (hc->ctype != HFC_TYPE_E1 || hc->e1_state == 1)
		for (ch = 0; ch <= 31; ch++) {
			if (hc->created[hc->chan[ch].port]) {
				hfcmulti_tx(hc, ch);
				
				hfcmulti_rx(hc, ch);
				if (hc->chan[ch].dch &&
				    hc->chan[ch].nt_timer > -1) {
					dch = hc->chan[ch].dch;
					if (!(--hc->chan[ch].nt_timer)) {
						schedule_event(dch,
							       FLG_PHCHANGE);
						if (debug &
						    DEBUG_HFCMULTI_STATE)
							printk(KERN_DEBUG
							       "%s: nt_timer at "
							       "state %x\n",
							       __func__,
							       dch->state);
					}
				}
			}
		}
	if (hc->ctype == HFC_TYPE_E1 && hc->created[0]) {
		dch = hc->chan[hc->dslot].dch;
		if (test_bit(HFC_CFG_REPORT_LOS, &hc->chan[hc->dslot].cfg)) {
			
			temp = HFC_inb_nodebug(hc, R_SYNC_STA) & V_SIG_LOS;
			if (!temp && hc->chan[hc->dslot].los)
				signal_state_up(dch, L1_SIGNAL_LOS_ON,
						"LOS detected");
			if (temp && !hc->chan[hc->dslot].los)
				signal_state_up(dch, L1_SIGNAL_LOS_OFF,
						"LOS gone");
			hc->chan[hc->dslot].los = temp;
		}
		if (test_bit(HFC_CFG_REPORT_AIS, &hc->chan[hc->dslot].cfg)) {
			
			temp = HFC_inb_nodebug(hc, R_SYNC_STA) & V_AIS;
			if (!temp && hc->chan[hc->dslot].ais)
				signal_state_up(dch, L1_SIGNAL_AIS_ON,
						"AIS detected");
			if (temp && !hc->chan[hc->dslot].ais)
				signal_state_up(dch, L1_SIGNAL_AIS_OFF,
						"AIS gone");
			hc->chan[hc->dslot].ais = temp;
		}
		if (test_bit(HFC_CFG_REPORT_SLIP, &hc->chan[hc->dslot].cfg)) {
			
			temp = HFC_inb_nodebug(hc, R_SLIP) & V_FOSLIP_RX;
			if (!temp && hc->chan[hc->dslot].slip_rx)
				signal_state_up(dch, L1_SIGNAL_SLIP_RX,
						" bit SLIP detected RX");
			hc->chan[hc->dslot].slip_rx = temp;
			temp = HFC_inb_nodebug(hc, R_SLIP) & V_FOSLIP_TX;
			if (!temp && hc->chan[hc->dslot].slip_tx)
				signal_state_up(dch, L1_SIGNAL_SLIP_TX,
						" bit SLIP detected TX");
			hc->chan[hc->dslot].slip_tx = temp;
		}
		if (test_bit(HFC_CFG_REPORT_RDI, &hc->chan[hc->dslot].cfg)) {
			
			temp = HFC_inb_nodebug(hc, R_RX_SL0_0) & V_A;
			if (!temp && hc->chan[hc->dslot].rdi)
				signal_state_up(dch, L1_SIGNAL_RDI_ON,
						"RDI detected");
			if (temp && !hc->chan[hc->dslot].rdi)
				signal_state_up(dch, L1_SIGNAL_RDI_OFF,
						"RDI gone");
			hc->chan[hc->dslot].rdi = temp;
		}
		temp = HFC_inb_nodebug(hc, R_JATT_DIR);
		switch (hc->chan[hc->dslot].sync) {
		case 0:
			if ((temp & 0x60) == 0x60) {
				if (debug & DEBUG_HFCMULTI_SYNC)
					printk(KERN_DEBUG
					       "%s: (id=%d) E1 now "
					       "in clock sync\n",
					       __func__, hc->id);
				HFC_outb(hc, R_RX_OFF,
					 hc->chan[hc->dslot].jitter | V_RX_INIT);
				HFC_outb(hc, R_TX_OFF,
					 hc->chan[hc->dslot].jitter | V_RX_INIT);
				hc->chan[hc->dslot].sync = 1;
				goto check_framesync;
			}
			break;
		case 1:
			if ((temp & 0x60) != 0x60) {
				if (debug & DEBUG_HFCMULTI_SYNC)
					printk(KERN_DEBUG
					       "%s: (id=%d) E1 "
					       "lost clock sync\n",
					       __func__, hc->id);
				hc->chan[hc->dslot].sync = 0;
				break;
			}
		check_framesync:
			temp = HFC_inb_nodebug(hc, R_SYNC_STA);
			if (temp == 0x27) {
				if (debug & DEBUG_HFCMULTI_SYNC)
					printk(KERN_DEBUG
					       "%s: (id=%d) E1 "
					       "now in frame sync\n",
					       __func__, hc->id);
				hc->chan[hc->dslot].sync = 2;
			}
			break;
		case 2:
			if ((temp & 0x60) != 0x60) {
				if (debug & DEBUG_HFCMULTI_SYNC)
					printk(KERN_DEBUG
					       "%s: (id=%d) E1 lost "
					       "clock & frame sync\n",
					       __func__, hc->id);
				hc->chan[hc->dslot].sync = 0;
				break;
			}
			temp = HFC_inb_nodebug(hc, R_SYNC_STA);
			if (temp != 0x27) {
				if (debug & DEBUG_HFCMULTI_SYNC)
					printk(KERN_DEBUG
					       "%s: (id=%d) E1 "
					       "lost frame sync\n",
					       __func__, hc->id);
				hc->chan[hc->dslot].sync = 1;
			}
			break;
		}
	}

	if (test_bit(HFC_CHIP_WATCHDOG, &hc->chip))
		hfcmulti_watchdog(hc);

	if (hc->leds)
		hfcmulti_leds(hc);
}

static void
ph_state_irq(struct hfc_multi *hc, u_char r_irq_statech)
{
	struct dchannel	*dch;
	int		ch;
	int		active;
	u_char		st_status, temp;

	
	for (ch = 0; ch <= 31; ch++) {
		if (hc->chan[ch].dch) {
			dch = hc->chan[ch].dch;
			if (r_irq_statech & 1) {
				HFC_outb_nodebug(hc, R_ST_SEL,
						 hc->chan[ch].port);
				
				udelay(1);
				
				st_status = HFC_inb_nodebug(hc, A_ST_RD_STATE);
				while (st_status != (temp =
						     HFC_inb_nodebug(hc, A_ST_RD_STATE))) {
					if (debug & DEBUG_HFCMULTI_STATE)
						printk(KERN_DEBUG "%s: reread "
						       "STATE because %d!=%d\n",
						       __func__, temp,
						       st_status);
					st_status = temp; 
				}

				
				if (test_bit(HFC_CHIP_PLXSD, &hc->chip) &&
				    dch->dev.D.protocol == ISDN_P_TE_S0) {
					if (st_status & V_FR_SYNC_ST)
						hc->syncronized |=
							(1 << hc->chan[ch].port);
					else
						hc->syncronized &=
							~(1 << hc->chan[ch].port);
				}
				dch->state = st_status & 0x0f;
				if (dch->dev.D.protocol == ISDN_P_NT_S0)
					active = 3;
				else
					active = 7;
				if (dch->state == active) {
					HFC_outb_nodebug(hc, R_FIFO,
							 (ch << 1) | 1);
					HFC_wait_nodebug(hc);
					HFC_outb_nodebug(hc,
							 R_INC_RES_FIFO, V_RES_F);
					HFC_wait_nodebug(hc);
					dch->tx_idx = 0;
				}
				schedule_event(dch, FLG_PHCHANGE);
				if (debug & DEBUG_HFCMULTI_STATE)
					printk(KERN_DEBUG
					       "%s: S/T newstate %x port %d\n",
					       __func__, dch->state,
					       hc->chan[ch].port);
			}
			r_irq_statech >>= 1;
		}
	}
	if (test_bit(HFC_CHIP_PLXSD, &hc->chip))
		plxsd_checksync(hc, 0);
}

static void
fifo_irq(struct hfc_multi *hc, int block)
{
	int	ch, j;
	struct dchannel	*dch;
	struct bchannel	*bch;
	u_char r_irq_fifo_bl;

	r_irq_fifo_bl = HFC_inb_nodebug(hc, R_IRQ_FIFO_BL0 + block);
	j = 0;
	while (j < 8) {
		ch = (block << 2) + (j >> 1);
		dch = hc->chan[ch].dch;
		bch = hc->chan[ch].bch;
		if (((!dch) && (!bch)) || (!hc->created[hc->chan[ch].port])) {
			j += 2;
			continue;
		}
		if (dch && (r_irq_fifo_bl & (1 << j)) &&
		    test_bit(FLG_ACTIVE, &dch->Flags)) {
			hfcmulti_tx(hc, ch);
			
			HFC_outb_nodebug(hc, R_FIFO, 0);
			HFC_wait_nodebug(hc);
		}
		if (bch && (r_irq_fifo_bl & (1 << j)) &&
		    test_bit(FLG_ACTIVE, &bch->Flags)) {
			hfcmulti_tx(hc, ch);
			
			HFC_outb_nodebug(hc, R_FIFO, 0);
			HFC_wait_nodebug(hc);
		}
		j++;
		if (dch && (r_irq_fifo_bl & (1 << j)) &&
		    test_bit(FLG_ACTIVE, &dch->Flags)) {
			hfcmulti_rx(hc, ch);
		}
		if (bch && (r_irq_fifo_bl & (1 << j)) &&
		    test_bit(FLG_ACTIVE, &bch->Flags)) {
			hfcmulti_rx(hc, ch);
		}
		j++;
	}
}

#ifdef IRQ_DEBUG
int irqsem;
#endif
static irqreturn_t
hfcmulti_interrupt(int intno, void *dev_id)
{
#ifdef IRQCOUNT_DEBUG
	static int iq1 = 0, iq2 = 0, iq3 = 0, iq4 = 0,
		iq5 = 0, iq6 = 0, iqcnt = 0;
#endif
	struct hfc_multi	*hc = dev_id;
	struct dchannel		*dch;
	u_char			r_irq_statech, status, r_irq_misc, r_irq_oview;
	int			i;
	void __iomem		*plx_acc;
	u_short			wval;
	u_char			e1_syncsta, temp;
	u_long			flags;

	if (!hc) {
		printk(KERN_ERR "HFC-multi: Spurious interrupt!\n");
		return IRQ_NONE;
	}

	spin_lock(&hc->lock);

#ifdef IRQ_DEBUG
	if (irqsem)
		printk(KERN_ERR "irq for card %d during irq from "
		       "card %d, this is no bug.\n", hc->id + 1, irqsem);
	irqsem = hc->id + 1;
#endif
#ifdef CONFIG_MISDN_HFCMULTI_8xx
	if (hc->immap->im_cpm.cp_pbdat & hc->pb_irqmsk)
		goto irq_notforus;
#endif
	if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
		spin_lock_irqsave(&plx_lock, flags);
		plx_acc = hc->plx_membase + PLX_INTCSR;
		wval = readw(plx_acc);
		spin_unlock_irqrestore(&plx_lock, flags);
		if (!(wval & PLX_INTCSR_LINTI1_STATUS))
			goto irq_notforus;
	}

	status = HFC_inb_nodebug(hc, R_STATUS);
	r_irq_statech = HFC_inb_nodebug(hc, R_IRQ_STATECH);
#ifdef IRQCOUNT_DEBUG
	if (r_irq_statech)
		iq1++;
	if (status & V_DTMF_STA)
		iq2++;
	if (status & V_LOST_STA)
		iq3++;
	if (status & V_EXT_IRQSTA)
		iq4++;
	if (status & V_MISC_IRQSTA)
		iq5++;
	if (status & V_FR_IRQSTA)
		iq6++;
	if (iqcnt++ > 5000) {
		printk(KERN_ERR "iq1:%x iq2:%x iq3:%x iq4:%x iq5:%x iq6:%x\n",
		       iq1, iq2, iq3, iq4, iq5, iq6);
		iqcnt = 0;
	}
#endif

	if (!r_irq_statech &&
	    !(status & (V_DTMF_STA | V_LOST_STA | V_EXT_IRQSTA |
			V_MISC_IRQSTA | V_FR_IRQSTA))) {
		
		goto irq_notforus;
	}
	hc->irqcnt++;
	if (r_irq_statech) {
		if (hc->ctype != HFC_TYPE_E1)
			ph_state_irq(hc, r_irq_statech);
	}
	if (status & V_EXT_IRQSTA)
		; 
	if (status & V_LOST_STA) {
		
		HFC_outb(hc, R_INC_RES_FIFO, V_RES_LOST); 
	}
	if (status & V_MISC_IRQSTA) {
		
		r_irq_misc = HFC_inb_nodebug(hc, R_IRQ_MISC);
		r_irq_misc &= hc->hw.r_irqmsk_misc; 
		if (r_irq_misc & V_STA_IRQ) {
			if (hc->ctype == HFC_TYPE_E1) {
				
				dch = hc->chan[hc->dslot].dch;
				e1_syncsta = HFC_inb_nodebug(hc, R_SYNC_STA);
				if (test_bit(HFC_CHIP_PLXSD, &hc->chip)
				    && hc->e1_getclock) {
					if (e1_syncsta & V_FR_SYNC_E1)
						hc->syncronized = 1;
					else
						hc->syncronized = 0;
				}
				
				dch->state = HFC_inb_nodebug(hc, R_E1_RD_STA);
				while (dch->state != (temp =
						      HFC_inb_nodebug(hc, R_E1_RD_STA))) {
					if (debug & DEBUG_HFCMULTI_STATE)
						printk(KERN_DEBUG "%s: reread "
						       "STATE because %d!=%d\n",
						       __func__, temp,
						       dch->state);
					dch->state = temp; 
				}
				dch->state = HFC_inb_nodebug(hc, R_E1_RD_STA)
					& 0x7;
				schedule_event(dch, FLG_PHCHANGE);
				if (debug & DEBUG_HFCMULTI_STATE)
					printk(KERN_DEBUG
					       "%s: E1 (id=%d) newstate %x\n",
					       __func__, hc->id, dch->state);
				if (test_bit(HFC_CHIP_PLXSD, &hc->chip))
					plxsd_checksync(hc, 0);
			}
		}
		if (r_irq_misc & V_TI_IRQ) {
			if (hc->iclock_on)
				mISDN_clock_update(hc->iclock, poll, NULL);
			handle_timer_irq(hc);
		}

		if (r_irq_misc & V_DTMF_IRQ)
			hfcmulti_dtmf(hc);

		if (r_irq_misc & V_IRQ_PROC) {
			static int irq_proc_cnt;
			if (!irq_proc_cnt++)
				printk(KERN_DEBUG "%s: got V_IRQ_PROC -"
				       " this should not happen\n", __func__);
		}

	}
	if (status & V_FR_IRQSTA) {
		
		r_irq_oview = HFC_inb_nodebug(hc, R_IRQ_OVIEW);
		for (i = 0; i < 8; i++) {
			if (r_irq_oview & (1 << i))
				fifo_irq(hc, i);
		}
	}

#ifdef IRQ_DEBUG
	irqsem = 0;
#endif
	spin_unlock(&hc->lock);
	return IRQ_HANDLED;

irq_notforus:
#ifdef IRQ_DEBUG
	irqsem = 0;
#endif
	spin_unlock(&hc->lock);
	return IRQ_NONE;
}



static void
hfcmulti_dbusy_timer(struct hfc_multi *hc)
{
}


static int
mode_hfcmulti(struct hfc_multi *hc, int ch, int protocol, int slot_tx,
	      int bank_tx, int slot_rx, int bank_rx)
{
	int flow_tx = 0, flow_rx = 0, routing = 0;
	int oslot_tx, oslot_rx;
	int conf;

	if (ch < 0 || ch > 31)
		return -EINVAL;
	oslot_tx = hc->chan[ch].slot_tx;
	oslot_rx = hc->chan[ch].slot_rx;
	conf = hc->chan[ch].conf;

	if (debug & DEBUG_HFCMULTI_MODE)
		printk(KERN_DEBUG
		       "%s: card %d channel %d protocol %x slot old=%d new=%d "
		       "bank new=%d (TX) slot old=%d new=%d bank new=%d (RX)\n",
		       __func__, hc->id, ch, protocol, oslot_tx, slot_tx,
		       bank_tx, oslot_rx, slot_rx, bank_rx);

	if (oslot_tx >= 0 && slot_tx != oslot_tx) {
		
		if (debug & DEBUG_HFCMULTI_MODE)
			printk(KERN_DEBUG "%s: remove from slot %d (TX)\n",
			       __func__, oslot_tx);
		if (hc->slot_owner[oslot_tx << 1] == ch) {
			HFC_outb(hc, R_SLOT, oslot_tx << 1);
			HFC_outb(hc, A_SL_CFG, 0);
			if (hc->ctype != HFC_TYPE_XHFC)
				HFC_outb(hc, A_CONF, 0);
			hc->slot_owner[oslot_tx << 1] = -1;
		} else {
			if (debug & DEBUG_HFCMULTI_MODE)
				printk(KERN_DEBUG
				       "%s: we are not owner of this tx slot "
				       "anymore, channel %d is.\n",
				       __func__, hc->slot_owner[oslot_tx << 1]);
		}
	}

	if (oslot_rx >= 0 && slot_rx != oslot_rx) {
		
		if (debug & DEBUG_HFCMULTI_MODE)
			printk(KERN_DEBUG
			       "%s: remove from slot %d (RX)\n",
			       __func__, oslot_rx);
		if (hc->slot_owner[(oslot_rx << 1) | 1] == ch) {
			HFC_outb(hc, R_SLOT, (oslot_rx << 1) | V_SL_DIR);
			HFC_outb(hc, A_SL_CFG, 0);
			hc->slot_owner[(oslot_rx << 1) | 1] = -1;
		} else {
			if (debug & DEBUG_HFCMULTI_MODE)
				printk(KERN_DEBUG
				       "%s: we are not owner of this rx slot "
				       "anymore, channel %d is.\n",
				       __func__,
				       hc->slot_owner[(oslot_rx << 1) | 1]);
		}
	}

	if (slot_tx < 0) {
		flow_tx = 0x80; 
		
		hc->chan[ch].slot_tx = -1;
		hc->chan[ch].bank_tx = 0;
	} else {
		
		if (hc->chan[ch].txpending)
			flow_tx = 0x80; 
		else
			flow_tx = 0xc0; 
		
		routing = bank_tx ? 0xc0 : 0x80;
		if (conf >= 0 || bank_tx > 1)
			routing = 0x40; 
		if (debug & DEBUG_HFCMULTI_MODE)
			printk(KERN_DEBUG "%s: put channel %d to slot %d bank"
			       " %d flow %02x routing %02x conf %d (TX)\n",
			       __func__, ch, slot_tx, bank_tx,
			       flow_tx, routing, conf);
		HFC_outb(hc, R_SLOT, slot_tx << 1);
		HFC_outb(hc, A_SL_CFG, (ch << 1) | routing);
		if (hc->ctype != HFC_TYPE_XHFC)
			HFC_outb(hc, A_CONF,
				 (conf < 0) ? 0 : (conf | V_CONF_SL));
		hc->slot_owner[slot_tx << 1] = ch;
		hc->chan[ch].slot_tx = slot_tx;
		hc->chan[ch].bank_tx = bank_tx;
	}
	if (slot_rx < 0) {
		
		flow_rx = 0x80; 
		hc->chan[ch].slot_rx = -1;
		hc->chan[ch].bank_rx = 0;
	} else {
		
		if (hc->chan[ch].txpending)
			flow_rx = 0x80; 
		else
			flow_rx = 0xc0; 
		
		routing = bank_rx ? 0x80 : 0xc0; 
		if (conf >= 0 || bank_rx > 1)
			routing = 0x40; 
		if (debug & DEBUG_HFCMULTI_MODE)
			printk(KERN_DEBUG "%s: put channel %d to slot %d bank"
			       " %d flow %02x routing %02x conf %d (RX)\n",
			       __func__, ch, slot_rx, bank_rx,
			       flow_rx, routing, conf);
		HFC_outb(hc, R_SLOT, (slot_rx << 1) | V_SL_DIR);
		HFC_outb(hc, A_SL_CFG, (ch << 1) | V_CH_DIR | routing);
		hc->slot_owner[(slot_rx << 1) | 1] = ch;
		hc->chan[ch].slot_rx = slot_rx;
		hc->chan[ch].bank_rx = bank_rx;
	}

	switch (protocol) {
	case (ISDN_P_NONE):
		
		HFC_outb(hc, R_FIFO, ch << 1);
		HFC_wait(hc);
		HFC_outb(hc, A_CON_HDLC, flow_tx | 0x00 | V_IFF);
		HFC_outb(hc, A_SUBCH_CFG, 0);
		HFC_outb(hc, A_IRQ_MSK, 0);
		HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
		HFC_wait(hc);
		
		HFC_outb(hc, R_FIFO, (ch << 1) | 1);
		HFC_wait(hc);
		HFC_outb(hc, A_CON_HDLC, flow_rx | 0x00);
		HFC_outb(hc, A_SUBCH_CFG, 0);
		HFC_outb(hc, A_IRQ_MSK, 0);
		HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
		HFC_wait(hc);
		if (hc->chan[ch].bch && hc->ctype != HFC_TYPE_E1) {
			hc->hw.a_st_ctrl0[hc->chan[ch].port] &=
				((ch & 0x3) == 0) ? ~V_B1_EN : ~V_B2_EN;
			HFC_outb(hc, R_ST_SEL, hc->chan[ch].port);
			
			udelay(1);
			HFC_outb(hc, A_ST_CTRL0,
				 hc->hw.a_st_ctrl0[hc->chan[ch].port]);
		}
		if (hc->chan[ch].bch) {
			test_and_clear_bit(FLG_HDLC, &hc->chan[ch].bch->Flags);
			test_and_clear_bit(FLG_TRANSPARENT,
					   &hc->chan[ch].bch->Flags);
		}
		break;
	case (ISDN_P_B_RAW): 

		if (test_bit(HFC_CHIP_B410P, &hc->chip) &&
		    (hc->chan[ch].slot_rx < 0) &&
		    (hc->chan[ch].slot_tx < 0)) {

			printk(KERN_DEBUG
			       "Setting B-channel %d to echo cancelable "
			       "state on PCM slot %d\n", ch,
			       ((ch / 4) * 8) + ((ch % 4) * 4) + 1);
			printk(KERN_DEBUG
			       "Enabling pass through for channel\n");
			vpm_out(hc, ch, ((ch / 4) * 8) +
				((ch % 4) * 4) + 1, 0x01);
			
			
			HFC_outb(hc, R_FIFO, (ch << 1));
			HFC_wait(hc);
			HFC_outb(hc, A_CON_HDLC, 0xc0 | V_HDLC_TRP | V_IFF);
			HFC_outb(hc, R_SLOT, (((ch / 4) * 8) +
					      ((ch % 4) * 4) + 1) << 1);
			HFC_outb(hc, A_SL_CFG, 0x80 | (ch << 1));

			
			HFC_outb(hc, R_FIFO, 0x20 | (ch << 1) | 1);
			HFC_wait(hc);
			HFC_outb(hc, A_CON_HDLC, 0x20 | V_HDLC_TRP | V_IFF);
			HFC_outb(hc, A_SUBCH_CFG, 0);
			HFC_outb(hc, A_IRQ_MSK, 0);
			HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
			HFC_wait(hc);
			HFC_outb(hc, R_SLOT, ((((ch / 4) * 8) +
					       ((ch % 4) * 4) + 1) << 1) | 1);
			HFC_outb(hc, A_SL_CFG, 0x80 | 0x20 | (ch << 1) | 1);

			
			
			HFC_outb(hc, R_FIFO, (ch << 1) | 1);
			HFC_wait(hc);
			HFC_outb(hc, A_CON_HDLC, 0xc0 | V_HDLC_TRP | V_IFF);
			HFC_outb(hc, R_SLOT, ((((ch / 4) * 8) +
					       ((ch % 4) * 4)) << 1) | 1);
			HFC_outb(hc, A_SL_CFG, 0x80 | 0x40 | (ch << 1) | 1);

			
			HFC_outb(hc, R_FIFO, 0x20 | (ch << 1));
			HFC_wait(hc);
			HFC_outb(hc, A_CON_HDLC, 0x20 | V_HDLC_TRP | V_IFF);
			HFC_outb(hc, A_SUBCH_CFG, 0);
			HFC_outb(hc, A_IRQ_MSK, 0);
			HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
			HFC_wait(hc);
			
			HFC_outb_nodebug(hc, A_FIFO_DATA0_NOINC, hc->silence);
			HFC_outb(hc, R_SLOT, (((ch / 4) * 8) +
					      ((ch % 4) * 4)) << 1);
			HFC_outb(hc, A_SL_CFG, 0x80 | 0x20 | (ch << 1));
		} else {
			
			HFC_outb(hc, R_FIFO, ch << 1);
			HFC_wait(hc);
			if (hc->ctype == HFC_TYPE_XHFC)
				HFC_outb(hc, A_CON_HDLC, flow_tx | 0x07 << 2 |
					 V_HDLC_TRP | V_IFF);
			
			else
				HFC_outb(hc, A_CON_HDLC, flow_tx | 0x00 |
					 V_HDLC_TRP | V_IFF);
			HFC_outb(hc, A_SUBCH_CFG, 0);
			HFC_outb(hc, A_IRQ_MSK, 0);
			HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
			HFC_wait(hc);
			
			HFC_outb_nodebug(hc, A_FIFO_DATA0_NOINC, hc->silence);
			
			HFC_outb(hc, R_FIFO, (ch << 1) | 1);
			HFC_wait(hc);
			if (hc->ctype == HFC_TYPE_XHFC)
				HFC_outb(hc, A_CON_HDLC, flow_rx | 0x07 << 2 |
					 V_HDLC_TRP);
			
			else
				HFC_outb(hc, A_CON_HDLC, flow_rx | 0x00 |
					 V_HDLC_TRP);
			HFC_outb(hc, A_SUBCH_CFG, 0);
			HFC_outb(hc, A_IRQ_MSK, 0);
			HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
			HFC_wait(hc);
		}
		if (hc->ctype != HFC_TYPE_E1) {
			hc->hw.a_st_ctrl0[hc->chan[ch].port] |=
				((ch & 0x3) == 0) ? V_B1_EN : V_B2_EN;
			HFC_outb(hc, R_ST_SEL, hc->chan[ch].port);
			
			udelay(1);
			HFC_outb(hc, A_ST_CTRL0,
				 hc->hw.a_st_ctrl0[hc->chan[ch].port]);
		}
		if (hc->chan[ch].bch)
			test_and_set_bit(FLG_TRANSPARENT,
					 &hc->chan[ch].bch->Flags);
		break;
	case (ISDN_P_B_HDLC): 
	case (ISDN_P_TE_S0): 
	case (ISDN_P_NT_S0):
	case (ISDN_P_TE_E1):
	case (ISDN_P_NT_E1):
		
		HFC_outb(hc, R_FIFO, ch << 1);
		HFC_wait(hc);
		if (hc->ctype == HFC_TYPE_E1 || hc->chan[ch].bch) {
			
			HFC_outb(hc, A_CON_HDLC, flow_tx | 0x04);
			HFC_outb(hc, A_SUBCH_CFG, 0);
		} else {
			
			HFC_outb(hc, A_CON_HDLC, flow_tx | 0x04 | V_IFF);
			HFC_outb(hc, A_SUBCH_CFG, 2);
		}
		HFC_outb(hc, A_IRQ_MSK, V_IRQ);
		HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
		HFC_wait(hc);
		
		HFC_outb(hc, R_FIFO, (ch << 1) | 1);
		HFC_wait(hc);
		HFC_outb(hc, A_CON_HDLC, flow_rx | 0x04);
		if (hc->ctype == HFC_TYPE_E1 || hc->chan[ch].bch)
			HFC_outb(hc, A_SUBCH_CFG, 0); 
		else
			HFC_outb(hc, A_SUBCH_CFG, 2); 
		HFC_outb(hc, A_IRQ_MSK, V_IRQ);
		HFC_outb(hc, R_INC_RES_FIFO, V_RES_F);
		HFC_wait(hc);
		if (hc->chan[ch].bch) {
			test_and_set_bit(FLG_HDLC, &hc->chan[ch].bch->Flags);
			if (hc->ctype != HFC_TYPE_E1) {
				hc->hw.a_st_ctrl0[hc->chan[ch].port] |=
					((ch & 0x3) == 0) ? V_B1_EN : V_B2_EN;
				HFC_outb(hc, R_ST_SEL, hc->chan[ch].port);
				
				udelay(1);
				HFC_outb(hc, A_ST_CTRL0,
					 hc->hw.a_st_ctrl0[hc->chan[ch].port]);
			}
		}
		break;
	default:
		printk(KERN_DEBUG "%s: protocol not known %x\n",
		       __func__, protocol);
		hc->chan[ch].protocol = ISDN_P_NONE;
		return -ENOPROTOOPT;
	}
	hc->chan[ch].protocol = protocol;
	return 0;
}



static void
hfcmulti_pcm(struct hfc_multi *hc, int ch, int slot_tx, int bank_tx,
	     int slot_rx, int bank_rx)
{
	if (slot_tx < 0 || slot_rx < 0 || bank_tx < 0 || bank_rx < 0) {
		
		mode_hfcmulti(hc, ch, hc->chan[ch].protocol, -1, 0, -1, 0);
		return;
	}

	
	mode_hfcmulti(hc, ch, hc->chan[ch].protocol, slot_tx, bank_tx,
		      slot_rx, bank_rx);
}


static void
hfcmulti_conf(struct hfc_multi *hc, int ch, int num)
{
	if (num >= 0 && num <= 7)
		hc->chan[ch].conf = num;
	else
		hc->chan[ch].conf = -1;
	mode_hfcmulti(hc, ch, hc->chan[ch].protocol, hc->chan[ch].slot_tx,
		      hc->chan[ch].bank_tx, hc->chan[ch].slot_rx,
		      hc->chan[ch].bank_rx);
}




static int
hfcm_l1callback(struct dchannel *dch, u_int cmd)
{
	struct hfc_multi	*hc = dch->hw;
	u_long	flags;

	switch (cmd) {
	case INFO3_P8:
	case INFO3_P10:
		break;
	case HW_RESET_REQ:
		
		spin_lock_irqsave(&hc->lock, flags);
		if (hc->ctype == HFC_TYPE_E1) {
			if (debug & DEBUG_HFCMULTI_MSG)
				printk(KERN_DEBUG
				       "%s: HW_RESET_REQ no BRI\n",
				       __func__);
		} else {
			HFC_outb(hc, R_ST_SEL, hc->chan[dch->slot].port);
			
			udelay(1);
			HFC_outb(hc, A_ST_WR_STATE, V_ST_LD_STA | 3); 
			udelay(6); 
			HFC_outb(hc, A_ST_WR_STATE, 3);
			HFC_outb(hc, A_ST_WR_STATE, 3 | (V_ST_ACT * 3));
			
		}
		spin_unlock_irqrestore(&hc->lock, flags);
		l1_event(dch->l1, HW_POWERUP_IND);
		break;
	case HW_DEACT_REQ:
		
		spin_lock_irqsave(&hc->lock, flags);
		if (hc->ctype == HFC_TYPE_E1) {
			if (debug & DEBUG_HFCMULTI_MSG)
				printk(KERN_DEBUG
				       "%s: HW_DEACT_REQ no BRI\n",
				       __func__);
		} else {
			HFC_outb(hc, R_ST_SEL, hc->chan[dch->slot].port);
			
			udelay(1);
			HFC_outb(hc, A_ST_WR_STATE, V_ST_ACT * 2);
			
			if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
				hc->syncronized &=
					~(1 << hc->chan[dch->slot].port);
				plxsd_checksync(hc, 0);
			}
		}
		skb_queue_purge(&dch->squeue);
		if (dch->tx_skb) {
			dev_kfree_skb(dch->tx_skb);
			dch->tx_skb = NULL;
		}
		dch->tx_idx = 0;
		if (dch->rx_skb) {
			dev_kfree_skb(dch->rx_skb);
			dch->rx_skb = NULL;
		}
		test_and_clear_bit(FLG_TX_BUSY, &dch->Flags);
		if (test_and_clear_bit(FLG_BUSY_TIMER, &dch->Flags))
			del_timer(&dch->timer);
		spin_unlock_irqrestore(&hc->lock, flags);
		break;
	case HW_POWERUP_REQ:
		spin_lock_irqsave(&hc->lock, flags);
		if (hc->ctype == HFC_TYPE_E1) {
			if (debug & DEBUG_HFCMULTI_MSG)
				printk(KERN_DEBUG
				       "%s: HW_POWERUP_REQ no BRI\n",
				       __func__);
		} else {
			HFC_outb(hc, R_ST_SEL, hc->chan[dch->slot].port);
			
			udelay(1);
			HFC_outb(hc, A_ST_WR_STATE, 3 | 0x10); 
			udelay(6); 
			HFC_outb(hc, A_ST_WR_STATE, 3); 
		}
		spin_unlock_irqrestore(&hc->lock, flags);
		break;
	case PH_ACTIVATE_IND:
		test_and_set_bit(FLG_ACTIVE, &dch->Flags);
		_queue_data(&dch->dev.D, cmd, MISDN_ID_ANY, 0, NULL,
			    GFP_ATOMIC);
		break;
	case PH_DEACTIVATE_IND:
		test_and_clear_bit(FLG_ACTIVE, &dch->Flags);
		_queue_data(&dch->dev.D, cmd, MISDN_ID_ANY, 0, NULL,
			    GFP_ATOMIC);
		break;
	default:
		if (dch->debug & DEBUG_HW)
			printk(KERN_DEBUG "%s: unknown command %x\n",
			       __func__, cmd);
		return -1;
	}
	return 0;
}


static int
handle_dmsg(struct mISDNchannel *ch, struct sk_buff *skb)
{
	struct mISDNdevice	*dev = container_of(ch, struct mISDNdevice, D);
	struct dchannel		*dch = container_of(dev, struct dchannel, dev);
	struct hfc_multi	*hc = dch->hw;
	struct mISDNhead	*hh = mISDN_HEAD_P(skb);
	int			ret = -EINVAL;
	unsigned int		id;
	u_long			flags;

	switch (hh->prim) {
	case PH_DATA_REQ:
		if (skb->len < 1)
			break;
		spin_lock_irqsave(&hc->lock, flags);
		ret = dchannel_senddata(dch, skb);
		if (ret > 0) { 
			id = hh->id; 
			hfcmulti_tx(hc, dch->slot);
			ret = 0;
			
			HFC_outb(hc, R_FIFO, 0);
			HFC_wait(hc);
			spin_unlock_irqrestore(&hc->lock, flags);
			queue_ch_frame(ch, PH_DATA_CNF, id, NULL);
		} else
			spin_unlock_irqrestore(&hc->lock, flags);
		return ret;
	case PH_ACTIVATE_REQ:
		if (dch->dev.D.protocol != ISDN_P_TE_S0) {
			spin_lock_irqsave(&hc->lock, flags);
			ret = 0;
			if (debug & DEBUG_HFCMULTI_MSG)
				printk(KERN_DEBUG
				       "%s: PH_ACTIVATE port %d (0..%d)\n",
				       __func__, hc->chan[dch->slot].port,
				       hc->ports - 1);
			
			if (hc->ctype == HFC_TYPE_E1) {
				ph_state_change(dch);
				if (debug & DEBUG_HFCMULTI_STATE)
					printk(KERN_DEBUG
					       "%s: E1 report state %x \n",
					       __func__, dch->state);
			} else {
				HFC_outb(hc, R_ST_SEL,
					 hc->chan[dch->slot].port);
				
				udelay(1);
				HFC_outb(hc, A_ST_WR_STATE, V_ST_LD_STA | 1);
				
				udelay(6); 
				HFC_outb(hc, A_ST_WR_STATE, 1);
				HFC_outb(hc, A_ST_WR_STATE, 1 |
					 (V_ST_ACT * 3)); 
				dch->state = 1;
			}
			spin_unlock_irqrestore(&hc->lock, flags);
		} else
			ret = l1_event(dch->l1, hh->prim);
		break;
	case PH_DEACTIVATE_REQ:
		test_and_clear_bit(FLG_L2_ACTIVATED, &dch->Flags);
		if (dch->dev.D.protocol != ISDN_P_TE_S0) {
			spin_lock_irqsave(&hc->lock, flags);
			if (debug & DEBUG_HFCMULTI_MSG)
				printk(KERN_DEBUG
				       "%s: PH_DEACTIVATE port %d (0..%d)\n",
				       __func__, hc->chan[dch->slot].port,
				       hc->ports - 1);
			
			if (hc->ctype == HFC_TYPE_E1) {
				if (debug & DEBUG_HFCMULTI_MSG)
					printk(KERN_DEBUG
					       "%s: PH_DEACTIVATE no BRI\n",
					       __func__);
			} else {
				HFC_outb(hc, R_ST_SEL,
					 hc->chan[dch->slot].port);
				
				udelay(1);
				HFC_outb(hc, A_ST_WR_STATE, V_ST_ACT * 2);
				
				dch->state = 1;
			}
			skb_queue_purge(&dch->squeue);
			if (dch->tx_skb) {
				dev_kfree_skb(dch->tx_skb);
				dch->tx_skb = NULL;
			}
			dch->tx_idx = 0;
			if (dch->rx_skb) {
				dev_kfree_skb(dch->rx_skb);
				dch->rx_skb = NULL;
			}
			test_and_clear_bit(FLG_TX_BUSY, &dch->Flags);
			if (test_and_clear_bit(FLG_BUSY_TIMER, &dch->Flags))
				del_timer(&dch->timer);
#ifdef FIXME
			if (test_and_clear_bit(FLG_L1_BUSY, &dch->Flags))
				dchannel_sched_event(&hc->dch, D_CLEARBUSY);
#endif
			ret = 0;
			spin_unlock_irqrestore(&hc->lock, flags);
		} else
			ret = l1_event(dch->l1, hh->prim);
		break;
	}
	if (!ret)
		dev_kfree_skb(skb);
	return ret;
}

static void
deactivate_bchannel(struct bchannel *bch)
{
	struct hfc_multi	*hc = bch->hw;
	u_long			flags;

	spin_lock_irqsave(&hc->lock, flags);
	mISDN_clear_bchannel(bch);
	hc->chan[bch->slot].coeff_count = 0;
	hc->chan[bch->slot].rx_off = 0;
	hc->chan[bch->slot].conf = -1;
	mode_hfcmulti(hc, bch->slot, ISDN_P_NONE, -1, 0, -1, 0);
	spin_unlock_irqrestore(&hc->lock, flags);
}

static int
handle_bmsg(struct mISDNchannel *ch, struct sk_buff *skb)
{
	struct bchannel		*bch = container_of(ch, struct bchannel, ch);
	struct hfc_multi	*hc = bch->hw;
	int			ret = -EINVAL;
	struct mISDNhead	*hh = mISDN_HEAD_P(skb);
	unsigned int		id;
	u_long			flags;

	switch (hh->prim) {
	case PH_DATA_REQ:
		if (!skb->len)
			break;
		spin_lock_irqsave(&hc->lock, flags);
		ret = bchannel_senddata(bch, skb);
		if (ret > 0) { 
			id = hh->id; 
			hfcmulti_tx(hc, bch->slot);
			ret = 0;
			
			HFC_outb_nodebug(hc, R_FIFO, 0);
			HFC_wait_nodebug(hc);
			if (!test_bit(FLG_TRANSPARENT, &bch->Flags)) {
				spin_unlock_irqrestore(&hc->lock, flags);
				queue_ch_frame(ch, PH_DATA_CNF, id, NULL);
			} else
				spin_unlock_irqrestore(&hc->lock, flags);
		} else
			spin_unlock_irqrestore(&hc->lock, flags);
		return ret;
	case PH_ACTIVATE_REQ:
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG "%s: PH_ACTIVATE ch %d (0..32)\n",
			       __func__, bch->slot);
		spin_lock_irqsave(&hc->lock, flags);
		
		if (!test_and_set_bit(FLG_ACTIVE, &bch->Flags)) {
			hc->chan[bch->slot].txpending = 0;
			ret = mode_hfcmulti(hc, bch->slot,
					    ch->protocol,
					    hc->chan[bch->slot].slot_tx,
					    hc->chan[bch->slot].bank_tx,
					    hc->chan[bch->slot].slot_rx,
					    hc->chan[bch->slot].bank_rx);
			if (!ret) {
				if (ch->protocol == ISDN_P_B_RAW && !hc->dtmf
				    && test_bit(HFC_CHIP_DTMF, &hc->chip)) {
					
					hc->dtmf = 1;
					if (debug & DEBUG_HFCMULTI_DTMF)
						printk(KERN_DEBUG
						       "%s: start dtmf decoder\n",
						       __func__);
					HFC_outb(hc, R_DTMF, hc->hw.r_dtmf |
						 V_RST_DTMF);
				}
			}
		} else
			ret = 0;
		spin_unlock_irqrestore(&hc->lock, flags);
		if (!ret)
			_queue_data(ch, PH_ACTIVATE_IND, MISDN_ID_ANY, 0, NULL,
				    GFP_KERNEL);
		break;
	case PH_CONTROL_REQ:
		spin_lock_irqsave(&hc->lock, flags);
		switch (hh->id) {
		case HFC_SPL_LOOP_ON: 
			if (debug & DEBUG_HFCMULTI_MSG)
				printk(KERN_DEBUG
				       "%s: HFC_SPL_LOOP_ON (len = %d)\n",
				       __func__, skb->len);
			ret = 0;
			break;
		case HFC_SPL_LOOP_OFF: 
			if (debug & DEBUG_HFCMULTI_MSG)
				printk(KERN_DEBUG "%s: HFC_SPL_LOOP_OFF\n",
				       __func__);
			ret = 0;
			break;
		default:
			printk(KERN_ERR
			       "%s: unknown PH_CONTROL_REQ info %x\n",
			       __func__, hh->id);
			ret = -EINVAL;
		}
		spin_unlock_irqrestore(&hc->lock, flags);
		break;
	case PH_DEACTIVATE_REQ:
		deactivate_bchannel(bch); 
		_queue_data(ch, PH_DEACTIVATE_IND, MISDN_ID_ANY, 0, NULL,
			    GFP_KERNEL);
		ret = 0;
		break;
	}
	if (!ret)
		dev_kfree_skb(skb);
	return ret;
}

static int
channel_bctrl(struct bchannel *bch, struct mISDN_ctrl_req *cq)
{
	int			ret = 0;
	struct dsp_features	*features =
		(struct dsp_features *)(*((u_long *)&cq->p1));
	struct hfc_multi	*hc = bch->hw;
	int			slot_tx;
	int			bank_tx;
	int			slot_rx;
	int			bank_rx;
	int			num;

	switch (cq->op) {
	case MISDN_CTRL_GETOP:
		cq->op = MISDN_CTRL_HFC_OP | MISDN_CTRL_HW_FEATURES_OP
			| MISDN_CTRL_RX_OFF | MISDN_CTRL_FILL_EMPTY;
		break;
	case MISDN_CTRL_RX_OFF: 
		hc->chan[bch->slot].rx_off = !!cq->p1;
		if (!hc->chan[bch->slot].rx_off) {
			
			HFC_outb_nodebug(hc, R_FIFO, (bch->slot << 1) | 1);
			HFC_wait_nodebug(hc);
			HFC_outb_nodebug(hc, R_INC_RES_FIFO, V_RES_F);
			HFC_wait_nodebug(hc);
		}
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG "%s: RX_OFF request (nr=%d off=%d)\n",
			       __func__, bch->nr, hc->chan[bch->slot].rx_off);
		break;
	case MISDN_CTRL_FILL_EMPTY: 
		test_and_set_bit(FLG_FILLEMPTY, &bch->Flags);
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG "%s: FILL_EMPTY request (nr=%d "
			       "off=%d)\n", __func__, bch->nr, !!cq->p1);
		break;
	case MISDN_CTRL_HW_FEATURES: 
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG "%s: HW_FEATURE request\n",
			       __func__);
		
		features->hfc_id = hc->id;
		if (test_bit(HFC_CHIP_DTMF, &hc->chip))
			features->hfc_dtmf = 1;
		if (test_bit(HFC_CHIP_CONF, &hc->chip))
			features->hfc_conf = 1;
		features->hfc_loops = 0;
		if (test_bit(HFC_CHIP_B410P, &hc->chip)) {
			features->hfc_echocanhw = 1;
		} else {
			features->pcm_id = hc->pcm;
			features->pcm_slots = hc->slots;
			features->pcm_banks = 2;
		}
		break;
	case MISDN_CTRL_HFC_PCM_CONN: 
		slot_tx = cq->p1 & 0xff;
		bank_tx = cq->p1 >> 8;
		slot_rx = cq->p2 & 0xff;
		bank_rx = cq->p2 >> 8;
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG
			       "%s: HFC_PCM_CONN slot %d bank %d (TX) "
			       "slot %d bank %d (RX)\n",
			       __func__, slot_tx, bank_tx,
			       slot_rx, bank_rx);
		if (slot_tx < hc->slots && bank_tx <= 2 &&
		    slot_rx < hc->slots && bank_rx <= 2)
			hfcmulti_pcm(hc, bch->slot,
				     slot_tx, bank_tx, slot_rx, bank_rx);
		else {
			printk(KERN_WARNING
			       "%s: HFC_PCM_CONN slot %d bank %d (TX) "
			       "slot %d bank %d (RX) out of range\n",
			       __func__, slot_tx, bank_tx,
			       slot_rx, bank_rx);
			ret = -EINVAL;
		}
		break;
	case MISDN_CTRL_HFC_PCM_DISC: 
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG "%s: HFC_PCM_DISC\n",
			       __func__);
		hfcmulti_pcm(hc, bch->slot, -1, 0, -1, 0);
		break;
	case MISDN_CTRL_HFC_CONF_JOIN: 
		num = cq->p1 & 0xff;
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG "%s: HFC_CONF_JOIN conf %d\n",
			       __func__, num);
		if (num <= 7)
			hfcmulti_conf(hc, bch->slot, num);
		else {
			printk(KERN_WARNING
			       "%s: HW_CONF_JOIN conf %d out of range\n",
			       __func__, num);
			ret = -EINVAL;
		}
		break;
	case MISDN_CTRL_HFC_CONF_SPLIT: 
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG "%s: HFC_CONF_SPLIT\n", __func__);
		hfcmulti_conf(hc, bch->slot, -1);
		break;
	case MISDN_CTRL_HFC_ECHOCAN_ON:
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG "%s: HFC_ECHOCAN_ON\n", __func__);
		if (test_bit(HFC_CHIP_B410P, &hc->chip))
			vpm_echocan_on(hc, bch->slot, cq->p1);
		else
			ret = -EINVAL;
		break;

	case MISDN_CTRL_HFC_ECHOCAN_OFF:
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG "%s: HFC_ECHOCAN_OFF\n",
			       __func__);
		if (test_bit(HFC_CHIP_B410P, &hc->chip))
			vpm_echocan_off(hc, bch->slot);
		else
			ret = -EINVAL;
		break;
	default:
		printk(KERN_WARNING "%s: unknown Op %x\n",
		       __func__, cq->op);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int
hfcm_bctrl(struct mISDNchannel *ch, u_int cmd, void *arg)
{
	struct bchannel		*bch = container_of(ch, struct bchannel, ch);
	struct hfc_multi	*hc = bch->hw;
	int			err = -EINVAL;
	u_long	flags;

	if (bch->debug & DEBUG_HW)
		printk(KERN_DEBUG "%s: cmd:%x %p\n",
		       __func__, cmd, arg);
	switch (cmd) {
	case CLOSE_CHANNEL:
		test_and_clear_bit(FLG_OPEN, &bch->Flags);
		if (test_bit(FLG_ACTIVE, &bch->Flags))
			deactivate_bchannel(bch); 
		ch->protocol = ISDN_P_NONE;
		ch->peer = NULL;
		module_put(THIS_MODULE);
		err = 0;
		break;
	case CONTROL_CHANNEL:
		spin_lock_irqsave(&hc->lock, flags);
		err = channel_bctrl(bch, arg);
		spin_unlock_irqrestore(&hc->lock, flags);
		break;
	default:
		printk(KERN_WARNING "%s: unknown prim(%x)\n",
		       __func__, cmd);
	}
	return err;
}

static void
ph_state_change(struct dchannel *dch)
{
	struct hfc_multi *hc;
	int ch, i;

	if (!dch) {
		printk(KERN_WARNING "%s: ERROR given dch is NULL\n", __func__);
		return;
	}
	hc = dch->hw;
	ch = dch->slot;

	if (hc->ctype == HFC_TYPE_E1) {
		if (dch->dev.D.protocol == ISDN_P_TE_E1) {
			if (debug & DEBUG_HFCMULTI_STATE)
				printk(KERN_DEBUG
				       "%s: E1 TE (id=%d) newstate %x\n",
				       __func__, hc->id, dch->state);
		} else {
			if (debug & DEBUG_HFCMULTI_STATE)
				printk(KERN_DEBUG
				       "%s: E1 NT (id=%d) newstate %x\n",
				       __func__, hc->id, dch->state);
		}
		switch (dch->state) {
		case (1):
			if (hc->e1_state != 1) {
				for (i = 1; i <= 31; i++) {
					
					HFC_outb_nodebug(hc, R_FIFO,
							 (i << 1) | 1);
					HFC_wait_nodebug(hc);
					HFC_outb_nodebug(hc, R_INC_RES_FIFO,
							 V_RES_F);
					HFC_wait_nodebug(hc);
				}
			}
			test_and_set_bit(FLG_ACTIVE, &dch->Flags);
			_queue_data(&dch->dev.D, PH_ACTIVATE_IND,
				    MISDN_ID_ANY, 0, NULL, GFP_ATOMIC);
			break;

		default:
			if (hc->e1_state != 1)
				return;
			test_and_clear_bit(FLG_ACTIVE, &dch->Flags);
			_queue_data(&dch->dev.D, PH_DEACTIVATE_IND,
				    MISDN_ID_ANY, 0, NULL, GFP_ATOMIC);
		}
		hc->e1_state = dch->state;
	} else {
		if (dch->dev.D.protocol == ISDN_P_TE_S0) {
			if (debug & DEBUG_HFCMULTI_STATE)
				printk(KERN_DEBUG
				       "%s: S/T TE newstate %x\n",
				       __func__, dch->state);
			switch (dch->state) {
			case (0):
				l1_event(dch->l1, HW_RESET_IND);
				break;
			case (3):
				l1_event(dch->l1, HW_DEACT_IND);
				break;
			case (5):
			case (8):
				l1_event(dch->l1, ANYSIGNAL);
				break;
			case (6):
				l1_event(dch->l1, INFO2);
				break;
			case (7):
				l1_event(dch->l1, INFO4_P8);
				break;
			}
		} else {
			if (debug & DEBUG_HFCMULTI_STATE)
				printk(KERN_DEBUG "%s: S/T NT newstate %x\n",
				       __func__, dch->state);
			switch (dch->state) {
			case (2):
				if (hc->chan[ch].nt_timer == 0) {
					hc->chan[ch].nt_timer = -1;
					HFC_outb(hc, R_ST_SEL,
						 hc->chan[ch].port);
					
					udelay(1);
					HFC_outb(hc, A_ST_WR_STATE, 4 |
						 V_ST_LD_STA); 
					udelay(6); 
					HFC_outb(hc, A_ST_WR_STATE, 4);
					dch->state = 4;
				} else {
					
					hc->chan[ch].nt_timer =
						nt_t1_count[poll_timer] + 1;
					HFC_outb(hc, R_ST_SEL,
						 hc->chan[ch].port);
					
					udelay(1);
					
					HFC_outb(hc, A_ST_WR_STATE, 2 |
						 V_SET_G2_G3);
				}
				break;
			case (1):
				hc->chan[ch].nt_timer = -1;
				test_and_clear_bit(FLG_ACTIVE, &dch->Flags);
				_queue_data(&dch->dev.D, PH_DEACTIVATE_IND,
					    MISDN_ID_ANY, 0, NULL, GFP_ATOMIC);
				break;
			case (4):
				hc->chan[ch].nt_timer = -1;
				break;
			case (3):
				hc->chan[ch].nt_timer = -1;
				test_and_set_bit(FLG_ACTIVE, &dch->Flags);
				_queue_data(&dch->dev.D, PH_ACTIVATE_IND,
					    MISDN_ID_ANY, 0, NULL, GFP_ATOMIC);
				break;
			}
		}
	}
}


static void
hfcmulti_initmode(struct dchannel *dch)
{
	struct hfc_multi *hc = dch->hw;
	u_char		a_st_wr_state, r_e1_wr_sta;
	int		i, pt;

	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: entered\n", __func__);

	if (hc->ctype == HFC_TYPE_E1) {
		hc->chan[hc->dslot].slot_tx = -1;
		hc->chan[hc->dslot].slot_rx = -1;
		hc->chan[hc->dslot].conf = -1;
		if (hc->dslot) {
			mode_hfcmulti(hc, hc->dslot, dch->dev.D.protocol,
				      -1, 0, -1, 0);
			dch->timer.function = (void *) hfcmulti_dbusy_timer;
			dch->timer.data = (long) dch;
			init_timer(&dch->timer);
		}
		for (i = 1; i <= 31; i++) {
			if (i == hc->dslot)
				continue;
			hc->chan[i].slot_tx = -1;
			hc->chan[i].slot_rx = -1;
			hc->chan[i].conf = -1;
			mode_hfcmulti(hc, i, ISDN_P_NONE, -1, 0, -1, 0);
		}
		
		if (test_bit(HFC_CFG_REPORT_LOS, &hc->chan[hc->dslot].cfg)) {
			HFC_outb(hc, R_LOS0, 255); 
			HFC_outb(hc, R_LOS1, 255); 
		}
		if (test_bit(HFC_CFG_OPTICAL, &hc->chan[hc->dslot].cfg)) {
			HFC_outb(hc, R_RX0, 0);
			hc->hw.r_tx0 = 0 | V_OUT_EN;
		} else {
			HFC_outb(hc, R_RX0, 1);
			hc->hw.r_tx0 = 1 | V_OUT_EN;
		}
		hc->hw.r_tx1 = V_ATX | V_NTRI;
		HFC_outb(hc, R_TX0, hc->hw.r_tx0);
		HFC_outb(hc, R_TX1, hc->hw.r_tx1);
		HFC_outb(hc, R_TX_FR0, 0x00);
		HFC_outb(hc, R_TX_FR1, 0xf8);

		if (test_bit(HFC_CFG_CRC4, &hc->chan[hc->dslot].cfg))
			HFC_outb(hc, R_TX_FR2, V_TX_MF | V_TX_E | V_NEG_E);

		HFC_outb(hc, R_RX_FR0, V_AUTO_RESYNC | V_AUTO_RECO | 0);

		if (test_bit(HFC_CFG_CRC4, &hc->chan[hc->dslot].cfg))
			HFC_outb(hc, R_RX_FR1, V_RX_MF | V_RX_MF_SYNC);

		if (dch->dev.D.protocol == ISDN_P_NT_E1) {
			if (debug & DEBUG_HFCMULTI_INIT)
				printk(KERN_DEBUG "%s: E1 port is NT-mode\n",
				       __func__);
			r_e1_wr_sta = 0; 
			hc->e1_getclock = 0;
		} else {
			if (debug & DEBUG_HFCMULTI_INIT)
				printk(KERN_DEBUG "%s: E1 port is TE-mode\n",
				       __func__);
			r_e1_wr_sta = 0; 
			hc->e1_getclock = 1;
		}
		if (test_bit(HFC_CHIP_RX_SYNC, &hc->chip))
			HFC_outb(hc, R_SYNC_OUT, V_SYNC_E1_RX);
		else
			HFC_outb(hc, R_SYNC_OUT, 0);
		if (test_bit(HFC_CHIP_E1CLOCK_GET, &hc->chip))
			hc->e1_getclock = 1;
		if (test_bit(HFC_CHIP_E1CLOCK_PUT, &hc->chip))
			hc->e1_getclock = 0;
		if (test_bit(HFC_CHIP_PCM_SLAVE, &hc->chip)) {
			
			if (debug & DEBUG_HFCMULTI_INIT)
				printk(KERN_DEBUG
				       "%s: E1 port is clock master "
				       "(clock from PCM)\n", __func__);
			HFC_outb(hc, R_SYNC_CTRL, V_EXT_CLK_SYNC | V_PCM_SYNC);
		} else {
			if (hc->e1_getclock) {
				
				if (debug & DEBUG_HFCMULTI_INIT)
					printk(KERN_DEBUG
					       "%s: E1 port is clock slave "
					       "(clock to PCM)\n", __func__);
				HFC_outb(hc, R_SYNC_CTRL, V_SYNC_OFFS);
			} else {
				
				if (debug & DEBUG_HFCMULTI_INIT)
					printk(KERN_DEBUG "%s: E1 port is "
					       "clock master "
					       "(clock from QUARTZ)\n",
					       __func__);
				HFC_outb(hc, R_SYNC_CTRL, V_EXT_CLK_SYNC |
					 V_PCM_SYNC | V_JATT_OFF);
				HFC_outb(hc, R_SYNC_OUT, 0);
			}
		}
		HFC_outb(hc, R_JATT_ATT, 0x9c); 
		HFC_outb(hc, R_PWM_MD, V_PWM0_MD);
		HFC_outb(hc, R_PWM0, 0x50);
		HFC_outb(hc, R_PWM1, 0xff);
		
		HFC_outb(hc, R_E1_WR_STA, r_e1_wr_sta | V_E1_LD_STA);
		udelay(6); 
		HFC_outb(hc, R_E1_WR_STA, r_e1_wr_sta);
		if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
			hc->syncronized = 0;
			plxsd_checksync(hc, 0);
		}
	} else {
		i = dch->slot;
		hc->chan[i].slot_tx = -1;
		hc->chan[i].slot_rx = -1;
		hc->chan[i].conf = -1;
		mode_hfcmulti(hc, i, dch->dev.D.protocol, -1, 0, -1, 0);
		dch->timer.function = (void *)hfcmulti_dbusy_timer;
		dch->timer.data = (long) dch;
		init_timer(&dch->timer);
		hc->chan[i - 2].slot_tx = -1;
		hc->chan[i - 2].slot_rx = -1;
		hc->chan[i - 2].conf = -1;
		mode_hfcmulti(hc, i - 2, ISDN_P_NONE, -1, 0, -1, 0);
		hc->chan[i - 1].slot_tx = -1;
		hc->chan[i - 1].slot_rx = -1;
		hc->chan[i - 1].conf = -1;
		mode_hfcmulti(hc, i - 1, ISDN_P_NONE, -1, 0, -1, 0);
		
		pt = hc->chan[i].port;
		
		HFC_outb(hc, R_ST_SEL, pt);
		
		udelay(1);
		if (dch->dev.D.protocol == ISDN_P_NT_S0) {
			if (debug & DEBUG_HFCMULTI_INIT)
				printk(KERN_DEBUG
				       "%s: ST port %d is NT-mode\n",
				       __func__, pt);
			
			HFC_outb(hc, A_ST_CLK_DLY, clockdelay_nt);
			a_st_wr_state = 1; 
			hc->hw.a_st_ctrl0[pt] = V_ST_MD;
		} else {
			if (debug & DEBUG_HFCMULTI_INIT)
				printk(KERN_DEBUG
				       "%s: ST port %d is TE-mode\n",
				       __func__, pt);
			
			HFC_outb(hc, A_ST_CLK_DLY, clockdelay_te);
			a_st_wr_state = 2; 
			hc->hw.a_st_ctrl0[pt] = 0;
		}
		if (!test_bit(HFC_CFG_NONCAP_TX, &hc->chan[i].cfg))
			hc->hw.a_st_ctrl0[pt] |= V_TX_LI;
		if (hc->ctype == HFC_TYPE_XHFC) {
			hc->hw.a_st_ctrl0[pt] |= 0x40 ;
			HFC_outb(hc, 0x35 ,
				 0x7c << 1 );
		}
		
		HFC_outb(hc, A_ST_CTRL0,  hc->hw.a_st_ctrl0[pt]);
		
		if ((dch->dev.D.protocol == ISDN_P_NT_S0) ||
		    test_bit(HFC_CFG_DIS_ECHANNEL, &hc->chan[i].cfg))
			HFC_outb(hc, A_ST_CTRL1, V_E_IGNO);
		else
			HFC_outb(hc, A_ST_CTRL1, 0);
		
		HFC_outb(hc, A_ST_CTRL2,  V_B1_RX_EN | V_B2_RX_EN);
		
		HFC_outb(hc, A_ST_WR_STATE, a_st_wr_state | V_ST_LD_STA);
		udelay(6); 
		HFC_outb(hc, A_ST_WR_STATE, a_st_wr_state);
		hc->hw.r_sci_msk |= 1 << pt;
		
		HFC_outb(hc, R_SCI_MSK, hc->hw.r_sci_msk);
		
		if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
			hc->syncronized &=
				~(1 << hc->chan[dch->slot].port);
			plxsd_checksync(hc, 0);
		}
	}
	if (debug & DEBUG_HFCMULTI_INIT)
		printk("%s: done\n", __func__);
}


static int
open_dchannel(struct hfc_multi *hc, struct dchannel *dch,
	      struct channel_req *rq)
{
	int	err = 0;
	u_long	flags;

	if (debug & DEBUG_HW_OPEN)
		printk(KERN_DEBUG "%s: dev(%d) open from %p\n", __func__,
		       dch->dev.id, __builtin_return_address(0));
	if (rq->protocol == ISDN_P_NONE)
		return -EINVAL;
	if ((dch->dev.D.protocol != ISDN_P_NONE) &&
	    (dch->dev.D.protocol != rq->protocol)) {
		if (debug & DEBUG_HFCMULTI_MODE)
			printk(KERN_DEBUG "%s: change protocol %x to %x\n",
			       __func__, dch->dev.D.protocol, rq->protocol);
	}
	if ((dch->dev.D.protocol == ISDN_P_TE_S0) &&
	    (rq->protocol != ISDN_P_TE_S0))
		l1_event(dch->l1, CLOSE_CHANNEL);
	if (dch->dev.D.protocol != rq->protocol) {
		if (rq->protocol == ISDN_P_TE_S0) {
			err = create_l1(dch, hfcm_l1callback);
			if (err)
				return err;
		}
		dch->dev.D.protocol = rq->protocol;
		spin_lock_irqsave(&hc->lock, flags);
		hfcmulti_initmode(dch);
		spin_unlock_irqrestore(&hc->lock, flags);
	}

	if (((rq->protocol == ISDN_P_NT_S0) && (dch->state == 3)) ||
	    ((rq->protocol == ISDN_P_TE_S0) && (dch->state == 7)) ||
	    ((rq->protocol == ISDN_P_NT_E1) && (dch->state == 1)) ||
	    ((rq->protocol == ISDN_P_TE_E1) && (dch->state == 1))) {
		_queue_data(&dch->dev.D, PH_ACTIVATE_IND, MISDN_ID_ANY,
			    0, NULL, GFP_KERNEL);
	}
	rq->ch = &dch->dev.D;
	if (!try_module_get(THIS_MODULE))
		printk(KERN_WARNING "%s:cannot get module\n", __func__);
	return 0;
}

static int
open_bchannel(struct hfc_multi *hc, struct dchannel *dch,
	      struct channel_req *rq)
{
	struct bchannel	*bch;
	int		ch;

	if (!test_channelmap(rq->adr.channel, dch->dev.channelmap))
		return -EINVAL;
	if (rq->protocol == ISDN_P_NONE)
		return -EINVAL;
	if (hc->ctype == HFC_TYPE_E1)
		ch = rq->adr.channel;
	else
		ch = (rq->adr.channel - 1) + (dch->slot - 2);
	bch = hc->chan[ch].bch;
	if (!bch) {
		printk(KERN_ERR "%s:internal error ch %d has no bch\n",
		       __func__, ch);
		return -EINVAL;
	}
	if (test_and_set_bit(FLG_OPEN, &bch->Flags))
		return -EBUSY; 
	test_and_clear_bit(FLG_FILLEMPTY, &bch->Flags);
	bch->ch.protocol = rq->protocol;
	hc->chan[ch].rx_off = 0;
	rq->ch = &bch->ch;
	if (!try_module_get(THIS_MODULE))
		printk(KERN_WARNING "%s:cannot get module\n", __func__);
	return 0;
}

static int
channel_dctrl(struct dchannel *dch, struct mISDN_ctrl_req *cq)
{
	struct hfc_multi	*hc = dch->hw;
	int	ret = 0;
	int	wd_mode, wd_cnt;

	switch (cq->op) {
	case MISDN_CTRL_GETOP:
		cq->op = MISDN_CTRL_HFC_OP;
		break;
	case MISDN_CTRL_HFC_WD_INIT: 
		wd_cnt = cq->p1 & 0xf;
		wd_mode = !!(cq->p1 >> 4);
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG "%s: MISDN_CTRL_HFC_WD_INIT mode %s"
			       ", counter 0x%x\n", __func__,
			       wd_mode ? "AUTO" : "MANUAL", wd_cnt);
		
		HFC_outb(hc, R_TI_WD, poll_timer | (wd_cnt << 4));
		hc->hw.r_bert_wd_md = (wd_mode ? V_AUTO_WD_RES : 0);
		if (hc->ctype == HFC_TYPE_XHFC)
			hc->hw.r_bert_wd_md |= 0x40 ;
		
		HFC_outb(hc, R_BERT_WD_MD, hc->hw.r_bert_wd_md | V_WD_RES);
		if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
			
			HFC_outb(hc, R_GPIO_SEL,  V_GPIO_SEL7);
			HFC_outb(hc, R_GPIO_EN1,  V_GPIO_EN15);
			HFC_outb(hc, R_GPIO_OUT1, 0);
			HFC_outb(hc, R_GPIO_OUT1, V_GPIO_OUT15);
		}
		break;
	case MISDN_CTRL_HFC_WD_RESET: 
		if (debug & DEBUG_HFCMULTI_MSG)
			printk(KERN_DEBUG "%s: MISDN_CTRL_HFC_WD_RESET\n",
			       __func__);
		HFC_outb(hc, R_BERT_WD_MD, hc->hw.r_bert_wd_md | V_WD_RES);
		break;
	default:
		printk(KERN_WARNING "%s: unknown Op %x\n",
		       __func__, cq->op);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int
hfcm_dctrl(struct mISDNchannel *ch, u_int cmd, void *arg)
{
	struct mISDNdevice	*dev = container_of(ch, struct mISDNdevice, D);
	struct dchannel		*dch = container_of(dev, struct dchannel, dev);
	struct hfc_multi	*hc = dch->hw;
	struct channel_req	*rq;
	int			err = 0;
	u_long			flags;

	if (dch->debug & DEBUG_HW)
		printk(KERN_DEBUG "%s: cmd:%x %p\n",
		       __func__, cmd, arg);
	switch (cmd) {
	case OPEN_CHANNEL:
		rq = arg;
		switch (rq->protocol) {
		case ISDN_P_TE_S0:
		case ISDN_P_NT_S0:
			if (hc->ctype == HFC_TYPE_E1) {
				err = -EINVAL;
				break;
			}
			err = open_dchannel(hc, dch, rq); 
			break;
		case ISDN_P_TE_E1:
		case ISDN_P_NT_E1:
			if (hc->ctype != HFC_TYPE_E1) {
				err = -EINVAL;
				break;
			}
			err = open_dchannel(hc, dch, rq); 
			break;
		default:
			spin_lock_irqsave(&hc->lock, flags);
			err = open_bchannel(hc, dch, rq);
			spin_unlock_irqrestore(&hc->lock, flags);
		}
		break;
	case CLOSE_CHANNEL:
		if (debug & DEBUG_HW_OPEN)
			printk(KERN_DEBUG "%s: dev(%d) close from %p\n",
			       __func__, dch->dev.id,
			       __builtin_return_address(0));
		module_put(THIS_MODULE);
		break;
	case CONTROL_CHANNEL:
		spin_lock_irqsave(&hc->lock, flags);
		err = channel_dctrl(dch, arg);
		spin_unlock_irqrestore(&hc->lock, flags);
		break;
	default:
		if (dch->debug & DEBUG_HW)
			printk(KERN_DEBUG "%s: unknown command %x\n",
			       __func__, cmd);
		err = -EINVAL;
	}
	return err;
}

static int
clockctl(void *priv, int enable)
{
	struct hfc_multi *hc = priv;

	hc->iclock_on = enable;
	return 0;
}


static int
init_card(struct hfc_multi *hc)
{
	int	err = -EIO;
	u_long	flags;
	void	__iomem *plx_acc;
	u_long	plx_flags;

	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: entered\n", __func__);

	spin_lock_irqsave(&hc->lock, flags);
	
	hc->hw.r_irq_ctrl = V_FIFO_IRQ;
	disable_hwirq(hc);
	spin_unlock_irqrestore(&hc->lock, flags);

	if (request_irq(hc->irq, hfcmulti_interrupt, IRQF_SHARED,
			"HFC-multi", hc)) {
		printk(KERN_WARNING "mISDN: Could not get interrupt %d.\n",
		       hc->irq);
		hc->irq = 0;
		return -EIO;
	}

	if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
		spin_lock_irqsave(&plx_lock, plx_flags);
		plx_acc = hc->plx_membase + PLX_INTCSR;
		writew((PLX_INTCSR_PCIINT_ENABLE | PLX_INTCSR_LINTI1_ENABLE),
		       plx_acc); 
		spin_unlock_irqrestore(&plx_lock, plx_flags);
	}

	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: IRQ %d count %d\n",
		       __func__, hc->irq, hc->irqcnt);
	err = init_chip(hc);
	if (err)
		goto error;
	spin_lock_irqsave(&hc->lock, flags);
	enable_hwirq(hc);
	spin_unlock_irqrestore(&hc->lock, flags);
	
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout((100 * HZ) / 1000); 
	
	spin_lock_irqsave(&hc->lock, flags);
	disable_hwirq(hc);
	spin_unlock_irqrestore(&hc->lock, flags);
	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: IRQ %d count %d\n",
		       __func__, hc->irq, hc->irqcnt);
	if (hc->irqcnt) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: done\n", __func__);

		return 0;
	}
	if (test_bit(HFC_CHIP_PCM_SLAVE, &hc->chip)) {
		printk(KERN_INFO "ignoring missing interrupts\n");
		return 0;
	}

	printk(KERN_ERR "HFC PCI: IRQ(%d) getting no interrupts during init.\n",
	       hc->irq);

	err = -EIO;

error:
	if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
		spin_lock_irqsave(&plx_lock, plx_flags);
		plx_acc = hc->plx_membase + PLX_INTCSR;
		writew(0x00, plx_acc); 
		spin_unlock_irqrestore(&plx_lock, plx_flags);
	}

	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: free irq %d\n", __func__, hc->irq);
	if (hc->irq) {
		free_irq(hc->irq, hc);
		hc->irq = 0;
	}

	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: done (err=%d)\n", __func__, err);
	return err;
}


static int
setup_pci(struct hfc_multi *hc, struct pci_dev *pdev,
	  const struct pci_device_id *ent)
{
	struct hm_map	*m = (struct hm_map *)ent->driver_data;

	printk(KERN_INFO
	       "HFC-multi: card manufacturer: '%s' card name: '%s' clock: %s\n",
	       m->vendor_name, m->card_name, m->clock2 ? "double" : "normal");

	hc->pci_dev = pdev;
	if (m->clock2)
		test_and_set_bit(HFC_CHIP_CLOCK2, &hc->chip);

	if (ent->device == 0xB410) {
		test_and_set_bit(HFC_CHIP_B410P, &hc->chip);
		test_and_set_bit(HFC_CHIP_PCM_MASTER, &hc->chip);
		test_and_clear_bit(HFC_CHIP_PCM_SLAVE, &hc->chip);
		hc->slots = 32;
	}

	if (hc->pci_dev->irq <= 0) {
		printk(KERN_WARNING "HFC-multi: No IRQ for PCI card found.\n");
		return -EIO;
	}
	if (pci_enable_device(hc->pci_dev)) {
		printk(KERN_WARNING "HFC-multi: Error enabling PCI card.\n");
		return -EIO;
	}
	hc->leds = m->leds;
	hc->ledstate = 0xAFFEAFFE;
	hc->opticalsupport = m->opticalsupport;

	hc->pci_iobase = 0;
	hc->pci_membase = NULL;
	hc->plx_membase = NULL;

	
	if (m->io_mode) 
		hc->io_mode = m->io_mode;
	switch (hc->io_mode) {
	case HFC_IO_MODE_PLXSD:
		test_and_set_bit(HFC_CHIP_PLXSD, &hc->chip);
		hc->slots = 128; 
		hc->HFC_outb = HFC_outb_pcimem;
		hc->HFC_inb = HFC_inb_pcimem;
		hc->HFC_inw = HFC_inw_pcimem;
		hc->HFC_wait = HFC_wait_pcimem;
		hc->read_fifo = read_fifo_pcimem;
		hc->write_fifo = write_fifo_pcimem;
		hc->plx_origmembase =  hc->pci_dev->resource[0].start;
		

		if (!hc->plx_origmembase) {
			printk(KERN_WARNING
			       "HFC-multi: No IO-Memory for PCI PLX bridge found\n");
			pci_disable_device(hc->pci_dev);
			return -EIO;
		}

		hc->plx_membase = ioremap(hc->plx_origmembase, 0x80);
		if (!hc->plx_membase) {
			printk(KERN_WARNING
			       "HFC-multi: failed to remap plx address space. "
			       "(internal error)\n");
			pci_disable_device(hc->pci_dev);
			return -EIO;
		}
		printk(KERN_INFO
		       "HFC-multi: plx_membase:%#lx plx_origmembase:%#lx\n",
		       (u_long)hc->plx_membase, hc->plx_origmembase);

		hc->pci_origmembase =  hc->pci_dev->resource[2].start;
		
		if (!hc->pci_origmembase) {
			printk(KERN_WARNING
			       "HFC-multi: No IO-Memory for PCI card found\n");
			pci_disable_device(hc->pci_dev);
			return -EIO;
		}

		hc->pci_membase = ioremap(hc->pci_origmembase, 0x400);
		if (!hc->pci_membase) {
			printk(KERN_WARNING "HFC-multi: failed to remap io "
			       "address space. (internal error)\n");
			pci_disable_device(hc->pci_dev);
			return -EIO;
		}

		printk(KERN_INFO
		       "card %d: defined at MEMBASE %#lx (%#lx) IRQ %d HZ %d "
		       "leds-type %d\n",
		       hc->id, (u_long)hc->pci_membase, hc->pci_origmembase,
		       hc->pci_dev->irq, HZ, hc->leds);
		pci_write_config_word(hc->pci_dev, PCI_COMMAND, PCI_ENA_MEMIO);
		break;
	case HFC_IO_MODE_PCIMEM:
		hc->HFC_outb = HFC_outb_pcimem;
		hc->HFC_inb = HFC_inb_pcimem;
		hc->HFC_inw = HFC_inw_pcimem;
		hc->HFC_wait = HFC_wait_pcimem;
		hc->read_fifo = read_fifo_pcimem;
		hc->write_fifo = write_fifo_pcimem;
		hc->pci_origmembase = hc->pci_dev->resource[1].start;
		if (!hc->pci_origmembase) {
			printk(KERN_WARNING
			       "HFC-multi: No IO-Memory for PCI card found\n");
			pci_disable_device(hc->pci_dev);
			return -EIO;
		}

		hc->pci_membase = ioremap(hc->pci_origmembase, 256);
		if (!hc->pci_membase) {
			printk(KERN_WARNING
			       "HFC-multi: failed to remap io address space. "
			       "(internal error)\n");
			pci_disable_device(hc->pci_dev);
			return -EIO;
		}
		printk(KERN_INFO "card %d: defined at MEMBASE %#lx (%#lx) IRQ "
		       "%d HZ %d leds-type %d\n", hc->id, (u_long)hc->pci_membase,
		       hc->pci_origmembase, hc->pci_dev->irq, HZ, hc->leds);
		pci_write_config_word(hc->pci_dev, PCI_COMMAND, PCI_ENA_MEMIO);
		break;
	case HFC_IO_MODE_REGIO:
		hc->HFC_outb = HFC_outb_regio;
		hc->HFC_inb = HFC_inb_regio;
		hc->HFC_inw = HFC_inw_regio;
		hc->HFC_wait = HFC_wait_regio;
		hc->read_fifo = read_fifo_regio;
		hc->write_fifo = write_fifo_regio;
		hc->pci_iobase = (u_int) hc->pci_dev->resource[0].start;
		if (!hc->pci_iobase) {
			printk(KERN_WARNING
			       "HFC-multi: No IO for PCI card found\n");
			pci_disable_device(hc->pci_dev);
			return -EIO;
		}

		if (!request_region(hc->pci_iobase, 8, "hfcmulti")) {
			printk(KERN_WARNING "HFC-multi: failed to request "
			       "address space at 0x%08lx (internal error)\n",
			       hc->pci_iobase);
			pci_disable_device(hc->pci_dev);
			return -EIO;
		}

		printk(KERN_INFO
		       "%s %s: defined at IOBASE %#x IRQ %d HZ %d leds-type %d\n",
		       m->vendor_name, m->card_name, (u_int) hc->pci_iobase,
		       hc->pci_dev->irq, HZ, hc->leds);
		pci_write_config_word(hc->pci_dev, PCI_COMMAND, PCI_ENA_REGIO);
		break;
	default:
		printk(KERN_WARNING "HFC-multi: Invalid IO mode.\n");
		pci_disable_device(hc->pci_dev);
		return -EIO;
	}

	pci_set_drvdata(hc->pci_dev, hc);

	
	
	return 0;
}



static void
release_port(struct hfc_multi *hc, struct dchannel *dch)
{
	int	pt, ci, i = 0;
	u_long	flags;
	struct bchannel *pb;

	ci = dch->slot;
	pt = hc->chan[ci].port;

	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: entered for port %d\n",
		       __func__, pt + 1);

	if (pt >= hc->ports) {
		printk(KERN_WARNING "%s: ERROR port out of range (%d).\n",
		       __func__, pt + 1);
		return;
	}

	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: releasing port=%d\n",
		       __func__, pt + 1);

	if (dch->dev.D.protocol == ISDN_P_TE_S0)
		l1_event(dch->l1, CLOSE_CHANNEL);

	hc->chan[ci].dch = NULL;

	if (hc->created[pt]) {
		hc->created[pt] = 0;
		mISDN_unregister_device(&dch->dev);
	}

	spin_lock_irqsave(&hc->lock, flags);

	if (dch->timer.function) {
		del_timer(&dch->timer);
		dch->timer.function = NULL;
	}

	if (hc->ctype == HFC_TYPE_E1) { 
		
		if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
			hc->syncronized = 0;
			plxsd_checksync(hc, 1);
		}
		
		for (i = 0; i <= 31; i++) {
			if (hc->chan[i].bch) {
				if (debug & DEBUG_HFCMULTI_INIT)
					printk(KERN_DEBUG
					       "%s: free port %d channel %d\n",
					       __func__, hc->chan[i].port + 1, i);
				pb = hc->chan[i].bch;
				hc->chan[i].bch = NULL;
				spin_unlock_irqrestore(&hc->lock, flags);
				mISDN_freebchannel(pb);
				kfree(pb);
				kfree(hc->chan[i].coeff);
				spin_lock_irqsave(&hc->lock, flags);
			}
		}
	} else {
		
		if (test_bit(HFC_CHIP_PLXSD, &hc->chip)) {
			hc->syncronized &=
				~(1 << hc->chan[ci].port);
			plxsd_checksync(hc, 1);
		}
		
		if (hc->chan[ci - 2].bch) {
			if (debug & DEBUG_HFCMULTI_INIT)
				printk(KERN_DEBUG
				       "%s: free port %d channel %d\n",
				       __func__, hc->chan[ci - 2].port + 1,
				       ci - 2);
			pb = hc->chan[ci - 2].bch;
			hc->chan[ci - 2].bch = NULL;
			spin_unlock_irqrestore(&hc->lock, flags);
			mISDN_freebchannel(pb);
			kfree(pb);
			kfree(hc->chan[ci - 2].coeff);
			spin_lock_irqsave(&hc->lock, flags);
		}
		if (hc->chan[ci - 1].bch) {
			if (debug & DEBUG_HFCMULTI_INIT)
				printk(KERN_DEBUG
				       "%s: free port %d channel %d\n",
				       __func__, hc->chan[ci - 1].port + 1,
				       ci - 1);
			pb = hc->chan[ci - 1].bch;
			hc->chan[ci - 1].bch = NULL;
			spin_unlock_irqrestore(&hc->lock, flags);
			mISDN_freebchannel(pb);
			kfree(pb);
			kfree(hc->chan[ci - 1].coeff);
			spin_lock_irqsave(&hc->lock, flags);
		}
	}

	spin_unlock_irqrestore(&hc->lock, flags);

	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: free port %d channel D\n", __func__, pt);
	mISDN_freedchannel(dch);
	kfree(dch);

	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: done!\n", __func__);
}

static void
release_card(struct hfc_multi *hc)
{
	u_long	flags;
	int	ch;

	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: release card (%d) entered\n",
		       __func__, hc->id);

	
	if (hc->iclock)
		mISDN_unregister_clock(hc->iclock);

	
	spin_lock_irqsave(&hc->lock, flags);
	disable_hwirq(hc);
	spin_unlock_irqrestore(&hc->lock, flags);
	udelay(1000);

	
	if (hc->leds)
		hfcmulti_leds(hc);

	
	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: disable all channels (d and b)\n",
		       __func__);
	for (ch = 0; ch <= 31; ch++) {
		if (hc->chan[ch].dch)
			release_port(hc, hc->chan[ch].dch);
	}

	
	if (hc->irq) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: free irq %d\n",
			       __func__, hc->irq);
		free_irq(hc->irq, hc);
		hc->irq = 0;

	}
	release_io_hfcmulti(hc);

	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: remove instance from list\n",
		       __func__);
	list_del(&hc->list);

	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: delete instance\n", __func__);
	if (hc == syncmaster)
		syncmaster = NULL;
	kfree(hc);
	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: card successfully removed\n",
		       __func__);
}

static int
init_e1_port(struct hfc_multi *hc, struct hm_map *m)
{
	struct dchannel	*dch;
	struct bchannel	*bch;
	int		ch, ret = 0;
	char		name[MISDN_MAX_IDLEN];

	dch = kzalloc(sizeof(struct dchannel), GFP_KERNEL);
	if (!dch)
		return -ENOMEM;
	dch->debug = debug;
	mISDN_initdchannel(dch, MAX_DFRAME_LEN_L1, ph_state_change);
	dch->hw = hc;
	dch->dev.Dprotocols = (1 << ISDN_P_TE_E1) | (1 << ISDN_P_NT_E1);
	dch->dev.Bprotocols = (1 << (ISDN_P_B_RAW & ISDN_P_B_MASK)) |
		(1 << (ISDN_P_B_HDLC & ISDN_P_B_MASK));
	dch->dev.D.send = handle_dmsg;
	dch->dev.D.ctrl = hfcm_dctrl;
	dch->dev.nrbchan = (hc->dslot) ? 30 : 31;
	dch->slot = hc->dslot;
	hc->chan[hc->dslot].dch = dch;
	hc->chan[hc->dslot].port = 0;
	hc->chan[hc->dslot].nt_timer = -1;
	for (ch = 1; ch <= 31; ch++) {
		if (ch == hc->dslot) 
			continue;
		bch = kzalloc(sizeof(struct bchannel), GFP_KERNEL);
		if (!bch) {
			printk(KERN_ERR "%s: no memory for bchannel\n",
			       __func__);
			ret = -ENOMEM;
			goto free_chan;
		}
		hc->chan[ch].coeff = kzalloc(512, GFP_KERNEL);
		if (!hc->chan[ch].coeff) {
			printk(KERN_ERR "%s: no memory for coeffs\n",
			       __func__);
			ret = -ENOMEM;
			kfree(bch);
			goto free_chan;
		}
		bch->nr = ch;
		bch->slot = ch;
		bch->debug = debug;
		mISDN_initbchannel(bch, MAX_DATA_MEM);
		bch->hw = hc;
		bch->ch.send = handle_bmsg;
		bch->ch.ctrl = hfcm_bctrl;
		bch->ch.nr = ch;
		list_add(&bch->ch.list, &dch->dev.bchannels);
		hc->chan[ch].bch = bch;
		hc->chan[ch].port = 0;
		set_channelmap(bch->nr, dch->dev.channelmap);
	}
	
	if (port[Port_cnt] & 0x001) {
		if (!m->opticalsupport)  {
			printk(KERN_INFO
			       "This board has no optical "
			       "support\n");
		} else {
			if (debug & DEBUG_HFCMULTI_INIT)
				printk(KERN_DEBUG
				       "%s: PORT set optical "
				       "interfacs: card(%d) "
				       "port(%d)\n",
				       __func__,
				       HFC_cnt + 1, 1);
			test_and_set_bit(HFC_CFG_OPTICAL,
					 &hc->chan[hc->dslot].cfg);
		}
	}
	
	if (port[Port_cnt] & 0x004) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: PORT set "
			       "LOS report: card(%d) port(%d)\n",
			       __func__, HFC_cnt + 1, 1);
		test_and_set_bit(HFC_CFG_REPORT_LOS,
				 &hc->chan[hc->dslot].cfg);
	}
	
	if (port[Port_cnt] & 0x008) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: PORT set "
			       "AIS report: card(%d) port(%d)\n",
			       __func__, HFC_cnt + 1, 1);
		test_and_set_bit(HFC_CFG_REPORT_AIS,
				 &hc->chan[hc->dslot].cfg);
	}
	
	if (port[Port_cnt] & 0x010) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG
			       "%s: PORT set SLIP report: "
			       "card(%d) port(%d)\n",
			       __func__, HFC_cnt + 1, 1);
		test_and_set_bit(HFC_CFG_REPORT_SLIP,
				 &hc->chan[hc->dslot].cfg);
	}
	
	if (port[Port_cnt] & 0x020) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG
			       "%s: PORT set RDI report: "
			       "card(%d) port(%d)\n",
			       __func__, HFC_cnt + 1, 1);
		test_and_set_bit(HFC_CFG_REPORT_RDI,
				 &hc->chan[hc->dslot].cfg);
	}
	
	if (!(port[Port_cnt] & 0x100)) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: PORT turn on CRC4 report:"
			       " card(%d) port(%d)\n",
			       __func__, HFC_cnt + 1, 1);
		test_and_set_bit(HFC_CFG_CRC4,
				 &hc->chan[hc->dslot].cfg);
	} else {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: PORT turn off CRC4"
			       " report: card(%d) port(%d)\n",
			       __func__, HFC_cnt + 1, 1);
	}
	
	if (port[Port_cnt] & 0x0200) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: PORT force getting clock from "
			       "E1: card(%d) port(%d)\n",
			       __func__, HFC_cnt + 1, 1);
		test_and_set_bit(HFC_CHIP_E1CLOCK_GET, &hc->chip);
	} else
		if (port[Port_cnt] & 0x0400) {
			if (debug & DEBUG_HFCMULTI_INIT)
				printk(KERN_DEBUG "%s: PORT force putting clock to "
				       "E1: card(%d) port(%d)\n",
				       __func__, HFC_cnt + 1, 1);
			test_and_set_bit(HFC_CHIP_E1CLOCK_PUT, &hc->chip);
		}
	
	if (port[Port_cnt] & 0x0800) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG "%s: PORT disable JATT PLL on "
			       "E1: card(%d) port(%d)\n",
			       __func__, HFC_cnt + 1, 1);
		test_and_set_bit(HFC_CHIP_RX_SYNC, &hc->chip);
	}
	
	if (port[Port_cnt] & 0x3000) {
		hc->chan[hc->dslot].jitter = (port[Port_cnt]>>12) & 0x3;
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG
			       "%s: PORT set elastic "
			       "buffer to %d: card(%d) port(%d)\n",
			       __func__, hc->chan[hc->dslot].jitter,
			       HFC_cnt + 1, 1);
	} else
		hc->chan[hc->dslot].jitter = 2; 
	snprintf(name, MISDN_MAX_IDLEN - 1, "hfc-e1.%d", HFC_cnt + 1);
	ret = mISDN_register_device(&dch->dev, &hc->pci_dev->dev, name);
	if (ret)
		goto free_chan;
	hc->created[0] = 1;
	return ret;
free_chan:
	release_port(hc, dch);
	return ret;
}

static int
init_multi_port(struct hfc_multi *hc, int pt)
{
	struct dchannel	*dch;
	struct bchannel	*bch;
	int		ch, i, ret = 0;
	char		name[MISDN_MAX_IDLEN];

	dch = kzalloc(sizeof(struct dchannel), GFP_KERNEL);
	if (!dch)
		return -ENOMEM;
	dch->debug = debug;
	mISDN_initdchannel(dch, MAX_DFRAME_LEN_L1, ph_state_change);
	dch->hw = hc;
	dch->dev.Dprotocols = (1 << ISDN_P_TE_S0) | (1 << ISDN_P_NT_S0);
	dch->dev.Bprotocols = (1 << (ISDN_P_B_RAW & ISDN_P_B_MASK)) |
		(1 << (ISDN_P_B_HDLC & ISDN_P_B_MASK));
	dch->dev.D.send = handle_dmsg;
	dch->dev.D.ctrl = hfcm_dctrl;
	dch->dev.nrbchan = 2;
	i = pt << 2;
	dch->slot = i + 2;
	hc->chan[i + 2].dch = dch;
	hc->chan[i + 2].port = pt;
	hc->chan[i + 2].nt_timer = -1;
	for (ch = 0; ch < dch->dev.nrbchan; ch++) {
		bch = kzalloc(sizeof(struct bchannel), GFP_KERNEL);
		if (!bch) {
			printk(KERN_ERR "%s: no memory for bchannel\n",
			       __func__);
			ret = -ENOMEM;
			goto free_chan;
		}
		hc->chan[i + ch].coeff = kzalloc(512, GFP_KERNEL);
		if (!hc->chan[i + ch].coeff) {
			printk(KERN_ERR "%s: no memory for coeffs\n",
			       __func__);
			ret = -ENOMEM;
			kfree(bch);
			goto free_chan;
		}
		bch->nr = ch + 1;
		bch->slot = i + ch;
		bch->debug = debug;
		mISDN_initbchannel(bch, MAX_DATA_MEM);
		bch->hw = hc;
		bch->ch.send = handle_bmsg;
		bch->ch.ctrl = hfcm_bctrl;
		bch->ch.nr = ch + 1;
		list_add(&bch->ch.list, &dch->dev.bchannels);
		hc->chan[i + ch].bch = bch;
		hc->chan[i + ch].port = pt;
		set_channelmap(bch->nr, dch->dev.channelmap);
	}
	
	if (port[Port_cnt] & 0x001) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG
			       "%s: PROTOCOL set master clock: "
			       "card(%d) port(%d)\n",
			       __func__, HFC_cnt + 1, pt + 1);
		if (dch->dev.D.protocol != ISDN_P_TE_S0) {
			printk(KERN_ERR "Error: Master clock "
			       "for port(%d) of card(%d) is only"
			       " possible with TE-mode\n",
			       pt + 1, HFC_cnt + 1);
			ret = -EINVAL;
			goto free_chan;
		}
		if (hc->masterclk >= 0) {
			printk(KERN_ERR "Error: Master clock "
			       "for port(%d) of card(%d) already "
			       "defined for port(%d)\n",
			       pt + 1, HFC_cnt + 1, hc->masterclk + 1);
			ret = -EINVAL;
			goto free_chan;
		}
		hc->masterclk = pt;
	}
	
	if (port[Port_cnt] & 0x002) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG
			       "%s: PROTOCOL set non capacitive "
			       "transmitter: card(%d) port(%d)\n",
			       __func__, HFC_cnt + 1, pt + 1);
		test_and_set_bit(HFC_CFG_NONCAP_TX,
				 &hc->chan[i + 2].cfg);
	}
	
	if (port[Port_cnt] & 0x004) {
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG
			       "%s: PROTOCOL disable E-channel: "
			       "card(%d) port(%d)\n",
			       __func__, HFC_cnt + 1, pt + 1);
		test_and_set_bit(HFC_CFG_DIS_ECHANNEL,
				 &hc->chan[i + 2].cfg);
	}
	if (hc->ctype == HFC_TYPE_XHFC) {
		snprintf(name, MISDN_MAX_IDLEN - 1, "xhfc.%d-%d",
			 HFC_cnt + 1, pt + 1);
		ret = mISDN_register_device(&dch->dev, NULL, name);
	} else {
		snprintf(name, MISDN_MAX_IDLEN - 1, "hfc-%ds.%d-%d",
			 hc->ctype, HFC_cnt + 1, pt + 1);
		ret = mISDN_register_device(&dch->dev, &hc->pci_dev->dev, name);
	}
	if (ret)
		goto free_chan;
	hc->created[pt] = 1;
	return ret;
free_chan:
	release_port(hc, dch);
	return ret;
}

static int
hfcmulti_init(struct hm_map *m, struct pci_dev *pdev,
	      const struct pci_device_id *ent)
{
	int		ret_err = 0;
	int		pt;
	struct hfc_multi	*hc;
	u_long		flags;
	u_char		dips = 0, pmj = 0; 
	int		i;

	if (HFC_cnt >= MAX_CARDS) {
		printk(KERN_ERR "too many cards (max=%d).\n",
		       MAX_CARDS);
		return -EINVAL;
	}
	if ((type[HFC_cnt] & 0xff) && (type[HFC_cnt] & 0xff) != m->type) {
		printk(KERN_WARNING "HFC-MULTI: Card '%s:%s' type %d found but "
		       "type[%d] %d was supplied as module parameter\n",
		       m->vendor_name, m->card_name, m->type, HFC_cnt,
		       type[HFC_cnt] & 0xff);
		printk(KERN_WARNING "HFC-MULTI: Load module without parameters "
		       "first, to see cards and their types.");
		return -EINVAL;
	}
	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: Registering %s:%s chip type %d (0x%x)\n",
		       __func__, m->vendor_name, m->card_name, m->type,
		       type[HFC_cnt]);

	
	hc = kzalloc(sizeof(struct hfc_multi), GFP_KERNEL);
	if (!hc) {
		printk(KERN_ERR "No kmem for HFC-Multi card\n");
		return -ENOMEM;
	}
	spin_lock_init(&hc->lock);
	hc->mtyp = m;
	hc->ctype =  m->type;
	hc->ports = m->ports;
	hc->id = HFC_cnt;
	hc->pcm = pcm[HFC_cnt];
	hc->io_mode = iomode[HFC_cnt];
	if (dslot[HFC_cnt] < 0 && hc->ctype == HFC_TYPE_E1) {
		hc->dslot = 0;
		printk(KERN_INFO "HFC-E1 card has disabled D-channel, but "
		       "31 B-channels\n");
	}
	if (dslot[HFC_cnt] > 0 && dslot[HFC_cnt] < 32
	    && hc->ctype == HFC_TYPE_E1) {
		hc->dslot = dslot[HFC_cnt];
		printk(KERN_INFO "HFC-E1 card has alternating D-channel on "
		       "time slot %d\n", dslot[HFC_cnt]);
	} else
		hc->dslot = 16;

	
	hc->masterclk = -1;
	if (type[HFC_cnt] & 0x100) {
		test_and_set_bit(HFC_CHIP_ULAW, &hc->chip);
		hc->silence = 0xff; 
	} else
		hc->silence = 0x2a; 
	if ((poll >> 1) > sizeof(hc->silence_data)) {
		printk(KERN_ERR "HFCMULTI error: silence_data too small, "
		       "please fix\n");
		return -EINVAL;
	}
	for (i = 0; i < (poll >> 1); i++)
		hc->silence_data[i] = hc->silence;

	if (hc->ctype != HFC_TYPE_XHFC) {
		if (!(type[HFC_cnt] & 0x200))
			test_and_set_bit(HFC_CHIP_DTMF, &hc->chip);
		test_and_set_bit(HFC_CHIP_CONF, &hc->chip);
	}

	if (type[HFC_cnt] & 0x800)
		test_and_set_bit(HFC_CHIP_PCM_SLAVE, &hc->chip);
	if (type[HFC_cnt] & 0x1000) {
		test_and_set_bit(HFC_CHIP_PCM_MASTER, &hc->chip);
		test_and_clear_bit(HFC_CHIP_PCM_SLAVE, &hc->chip);
	}
	if (type[HFC_cnt] & 0x4000)
		test_and_set_bit(HFC_CHIP_EXRAM_128, &hc->chip);
	if (type[HFC_cnt] & 0x8000)
		test_and_set_bit(HFC_CHIP_EXRAM_512, &hc->chip);
	hc->slots = 32;
	if (type[HFC_cnt] & 0x10000)
		hc->slots = 64;
	if (type[HFC_cnt] & 0x20000)
		hc->slots = 128;
	if (type[HFC_cnt] & 0x80000) {
		test_and_set_bit(HFC_CHIP_WATCHDOG, &hc->chip);
		hc->wdcount = 0;
		hc->wdbyte = V_GPIO_OUT2;
		printk(KERN_NOTICE "Watchdog enabled\n");
	}

	if (pdev && ent)
		
		ret_err = setup_pci(hc, pdev, ent);
	else
#ifdef CONFIG_MISDN_HFCMULTI_8xx
		ret_err = setup_embedded(hc, m);
#else
	{
		printk(KERN_WARNING "Embedded IO Mode not selected\n");
		ret_err = -EIO;
	}
#endif
	if (ret_err) {
		if (hc == syncmaster)
			syncmaster = NULL;
		kfree(hc);
		return ret_err;
	}

	hc->HFC_outb_nodebug = hc->HFC_outb;
	hc->HFC_inb_nodebug = hc->HFC_inb;
	hc->HFC_inw_nodebug = hc->HFC_inw;
	hc->HFC_wait_nodebug = hc->HFC_wait;
#ifdef HFC_REGISTER_DEBUG
	hc->HFC_outb = HFC_outb_debug;
	hc->HFC_inb = HFC_inb_debug;
	hc->HFC_inw = HFC_inw_debug;
	hc->HFC_wait = HFC_wait_debug;
#endif
	
	for (pt = 0; pt < hc->ports; pt++) {
		if (Port_cnt >= MAX_PORTS) {
			printk(KERN_ERR "too many ports (max=%d).\n",
			       MAX_PORTS);
			ret_err = -EINVAL;
			goto free_card;
		}
		if (hc->ctype == HFC_TYPE_E1)
			ret_err = init_e1_port(hc, m);
		else
			ret_err = init_multi_port(hc, pt);
		if (debug & DEBUG_HFCMULTI_INIT)
			printk(KERN_DEBUG
			       "%s: Registering D-channel, card(%d) port(%d)"
			       "result %d\n",
			       __func__, HFC_cnt + 1, pt, ret_err);

		if (ret_err) {
			while (pt) { 
				pt--;
				release_port(hc, hc->chan[(pt << 2) + 2].dch);
			}
			goto free_card;
		}
		Port_cnt++;
	}

	
	switch (m->dip_type) {
	case DIP_4S:
		dips = ((~HFC_inb(hc, R_GPIO_IN1) & 0xE0) >> 5) |
			((~HFC_inb(hc, R_GPI_IN2) & 0x80) >> 3) |
			(~HFC_inb(hc, R_GPI_IN2) & 0x08);

		
		pmj = ((HFC_inb(hc, R_GPI_IN3) >> 4)  & 0xf);

		if (test_bit(HFC_CHIP_B410P, &hc->chip))
			pmj = ~pmj & 0xf;

		printk(KERN_INFO "%s: %s DIPs(0x%x) jumpers(0x%x)\n",
		       m->vendor_name, m->card_name, dips, pmj);
		break;
	case DIP_8S:
		HFC_outb(hc, R_BRG_PCM_CFG, 1 | V_PCM_CLK);
		
		outw(0x4000, hc->pci_iobase + 4);
		dips = inb(hc->pci_iobase);
		dips = inb(hc->pci_iobase);
		dips = inb(hc->pci_iobase);
		dips = ~inb(hc->pci_iobase) & 0x3F;
		outw(0x0, hc->pci_iobase + 4);
		
		HFC_outb(hc, R_BRG_PCM_CFG, V_PCM_CLK);
		printk(KERN_INFO "%s: %s DIPs(0x%x)\n",
		       m->vendor_name, m->card_name, dips);
		break;
	case DIP_E1:
		dips = (~HFC_inb(hc, R_GPI_IN0) & 0xF0) >> 4;
		printk(KERN_INFO "%s: %s DIPs(0x%x)\n",
		       m->vendor_name, m->card_name, dips);
		break;
	}

	
	spin_lock_irqsave(&HFClock, flags);
	list_add_tail(&hc->list, &HFClist);
	spin_unlock_irqrestore(&HFClock, flags);

	
	if (clock == HFC_cnt + 1)
		hc->iclock = mISDN_register_clock("HFCMulti", 0, clockctl, hc);

	
	hc->irq = (m->irq) ? : hc->pci_dev->irq;
	ret_err = init_card(hc);
	if (ret_err) {
		printk(KERN_ERR "init card returns %d\n", ret_err);
		release_card(hc);
		return ret_err;
	}

	
	spin_lock_irqsave(&hc->lock, flags);
	enable_hwirq(hc);
	spin_unlock_irqrestore(&hc->lock, flags);
	return 0;

free_card:
	release_io_hfcmulti(hc);
	if (hc == syncmaster)
		syncmaster = NULL;
	kfree(hc);
	return ret_err;
}

static void __devexit hfc_remove_pci(struct pci_dev *pdev)
{
	struct hfc_multi	*card = pci_get_drvdata(pdev);
	u_long			flags;

	if (debug)
		printk(KERN_INFO "removing hfc_multi card vendor:%x "
		       "device:%x subvendor:%x subdevice:%x\n",
		       pdev->vendor, pdev->device,
		       pdev->subsystem_vendor, pdev->subsystem_device);

	if (card) {
		spin_lock_irqsave(&HFClock, flags);
		release_card(card);
		spin_unlock_irqrestore(&HFClock, flags);
	}  else {
		if (debug)
			printk(KERN_DEBUG "%s: drvdata already removed\n",
			       __func__);
	}
}

#define	VENDOR_CCD	"Cologne Chip AG"
#define	VENDOR_BN	"beroNet GmbH"
#define	VENDOR_DIG	"Digium Inc."
#define VENDOR_JH	"Junghanns.NET GmbH"
#define VENDOR_PRIM	"PrimuX"

static const struct hm_map hfcm_map[] = {
		{VENDOR_BN, "HFC-1S Card (mini PCI)", 4, 1, 1, 3, 0, DIP_4S, 0, 0},
		{VENDOR_BN, "HFC-2S Card", 4, 2, 1, 3, 0, DIP_4S, 0, 0},
		{VENDOR_BN, "HFC-2S Card (mini PCI)", 4, 2, 1, 3, 0, DIP_4S, 0, 0},
		{VENDOR_BN, "HFC-4S Card", 4, 4, 1, 2, 0, DIP_4S, 0, 0},
		{VENDOR_BN, "HFC-4S Card (mini PCI)", 4, 4, 1, 2, 0, 0, 0, 0},
		{VENDOR_CCD, "HFC-4S Eval (old)", 4, 4, 0, 0, 0, 0, 0, 0},
		{VENDOR_CCD, "HFC-4S IOB4ST", 4, 4, 1, 2, 0, DIP_4S, 0, 0},
		{VENDOR_CCD, "HFC-4S", 4, 4, 1, 2, 0, 0, 0, 0},
		{VENDOR_DIG, "HFC-4S Card", 4, 4, 0, 2, 0, 0, HFC_IO_MODE_REGIO, 0},
		{VENDOR_CCD, "HFC-4S Swyx 4xS0 SX2 QuadBri", 4, 4, 1, 2, 0, 0, 0, 0},
		{VENDOR_JH, "HFC-4S (junghanns 2.0)", 4, 4, 1, 2, 0, 0, 0, 0},
		{VENDOR_PRIM, "HFC-2S Primux Card", 4, 2, 0, 0, 0, 0, 0, 0},

		{VENDOR_BN, "HFC-8S Card", 8, 8, 1, 0, 0, 0, 0, 0},
		{VENDOR_BN, "HFC-8S Card (+)", 8, 8, 1, 8, 0, DIP_8S,
		 HFC_IO_MODE_REGIO, 0},
		{VENDOR_CCD, "HFC-8S Eval (old)", 8, 8, 0, 0, 0, 0, 0, 0},
		{VENDOR_CCD, "HFC-8S IOB4ST Recording", 8, 8, 1, 0, 0, 0, 0, 0},

		{VENDOR_CCD, "HFC-8S IOB8ST", 8, 8, 1, 0, 0, 0, 0, 0},
		{VENDOR_CCD, "HFC-8S", 8, 8, 1, 0, 0, 0, 0, 0},
		{VENDOR_CCD, "HFC-8S", 8, 8, 1, 0, 0, 0, 0, 0},

		{VENDOR_BN, "HFC-E1 Card", 1, 1, 0, 1, 0, DIP_E1, 0, 0},
		{VENDOR_BN, "HFC-E1 Card (mini PCI)", 1, 1, 0, 1, 0, 0, 0, 0},
		{VENDOR_BN, "HFC-E1+ Card (Dual)", 1, 1, 0, 1, 0, DIP_E1, 0, 0},
		{VENDOR_BN, "HFC-E1 Card (Dual)", 1, 1, 0, 1, 0, DIP_E1, 0, 0},

		{VENDOR_CCD, "HFC-E1 Eval (old)", 1, 1, 0, 0, 0, 0, 0, 0},
		{VENDOR_CCD, "HFC-E1 IOB1E1", 1, 1, 0, 1, 0, 0, 0, 0},
		{VENDOR_CCD, "HFC-E1", 1, 1, 0, 1, 0, 0, 0, 0},

		{VENDOR_CCD, "HFC-4S Speech Design", 4, 4, 0, 0, 0, 0,
		 HFC_IO_MODE_PLXSD, 0},
		{VENDOR_CCD, "HFC-E1 Speech Design", 1, 1, 0, 0, 0, 0,
		 HFC_IO_MODE_PLXSD, 0},
		{VENDOR_CCD, "HFC-4S OpenVox", 4, 4, 1, 0, 0, 0, 0, 0},
		{VENDOR_CCD, "HFC-2S OpenVox", 4, 2, 1, 0, 0, 0, 0, 0},
		{VENDOR_CCD, "HFC-8S OpenVox", 8, 8, 1, 0, 0, 0, 0, 0},
		{VENDOR_CCD, "XHFC-4S Speech Design", 5, 4, 0, 0, 0, 0,
		 HFC_IO_MODE_EMBSD, XHFC_IRQ},
		{VENDOR_JH, "HFC-8S (junghanns)", 8, 8, 1, 0, 0, 0, 0, 0},
		{VENDOR_BN, "HFC-2S Beronet Card PCIe", 4, 2, 1, 3, 0, DIP_4S, 0, 0},
		{VENDOR_BN, "HFC-4S Beronet Card PCIe", 4, 4, 1, 2, 0, DIP_4S, 0, 0},
};

#undef H
#define H(x)	((unsigned long)&hfcm_map[x])
static struct pci_device_id hfmultipci_ids[] __devinitdata = {

	
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC4S, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_BN1SM, 0, 0, H(0)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC4S, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_BN2S, 0, 0, H(1)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC4S, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_BN2SM, 0, 0, H(2)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC4S, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_BN4S, 0, 0, H(3)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC4S, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_BN4SM, 0, 0, H(4)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC4S, PCI_VENDOR_ID_CCD,
	  PCI_DEVICE_ID_CCD_HFC4S, 0, 0, H(5)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC4S, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_IOB4ST, 0, 0, H(6)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC4S, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_HFC4S, 0, 0, H(7)}, 
	{ PCI_VENDOR_ID_DIGIUM, PCI_DEVICE_ID_DIGIUM_HFC4S,
	  PCI_VENDOR_ID_DIGIUM, PCI_DEVICE_ID_DIGIUM_HFC4S, 0, 0, H(8)},
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC4S, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_SWYX4S, 0, 0, H(9)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC4S, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_JH4S20, 0, 0, H(10)},
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC4S, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_PMX2S, 0, 0, H(11)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC4S, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_OV4S, 0, 0, H(28)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC4S, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_OV2S, 0, 0, H(29)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC4S, PCI_VENDOR_ID_CCD,
	  0xb761, 0, 0, H(33)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC4S, PCI_VENDOR_ID_CCD,
	  0xb762, 0, 0, H(34)}, 

	
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC8S, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_BN8S, 0, 0, H(12)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC8S, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_BN8SP, 0, 0, H(13)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC8S, PCI_VENDOR_ID_CCD,
	  PCI_DEVICE_ID_CCD_HFC8S, 0, 0, H(14)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC8S, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_IOB8STR, 0, 0, H(15)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC8S, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_IOB8ST, 0, 0, H(16)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC8S, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_IOB8ST_1, 0, 0, H(17)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC8S, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_HFC8S, 0, 0, H(18)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC8S, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_OV8S, 0, 0, H(30)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFC8S, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_JH8S, 0, 0, H(32)}, 


	
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFCE1, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_BNE1, 0, 0, H(19)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFCE1, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_BNE1M, 0, 0, H(20)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFCE1, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_BNE1DP, 0, 0, H(21)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFCE1, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_BNE1D, 0, 0, H(22)}, 

	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFCE1, PCI_VENDOR_ID_CCD,
	  PCI_DEVICE_ID_CCD_HFCE1, 0, 0, H(23)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFCE1, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_IOB1E1, 0, 0, H(24)}, 
	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFCE1, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_HFCE1, 0, 0, H(25)}, 

	{ PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9030, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_SPD4S, 0, 0, H(26)}, 
	{ PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_9030, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_SPDE1, 0, 0, H(27)}, 

	{ PCI_VENDOR_ID_CCD, PCI_DEVICE_ID_CCD_HFCE1, PCI_VENDOR_ID_CCD,
	  PCI_SUBDEVICE_ID_CCD_JHSE1, 0, 0, H(25)}, 

	{ PCI_VDEVICE(CCD, PCI_DEVICE_ID_CCD_HFC4S), 0 },
	{ PCI_VDEVICE(CCD, PCI_DEVICE_ID_CCD_HFC8S), 0 },
	{ PCI_VDEVICE(CCD, PCI_DEVICE_ID_CCD_HFCE1), 0 },
	{0, }
};
#undef H

MODULE_DEVICE_TABLE(pci, hfmultipci_ids);

static int
hfcmulti_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct hm_map	*m = (struct hm_map *)ent->driver_data;
	int		ret;

	if (m == NULL && ent->vendor == PCI_VENDOR_ID_CCD && (
		    ent->device == PCI_DEVICE_ID_CCD_HFC4S ||
		    ent->device == PCI_DEVICE_ID_CCD_HFC8S ||
		    ent->device == PCI_DEVICE_ID_CCD_HFCE1)) {
		printk(KERN_ERR
		       "Unknown HFC multiport controller (vendor:%04x device:%04x "
		       "subvendor:%04x subdevice:%04x)\n", pdev->vendor,
		       pdev->device, pdev->subsystem_vendor,
		       pdev->subsystem_device);
		printk(KERN_ERR
		       "Please contact the driver maintainer for support.\n");
		return -ENODEV;
	}
	ret = hfcmulti_init(m, pdev, ent);
	if (ret)
		return ret;
	HFC_cnt++;
	printk(KERN_INFO "%d devices registered\n", HFC_cnt);
	return 0;
}

static struct pci_driver hfcmultipci_driver = {
	.name		= "hfc_multi",
	.probe		= hfcmulti_probe,
	.remove		= __devexit_p(hfc_remove_pci),
	.id_table	= hfmultipci_ids,
};

static void __exit
HFCmulti_cleanup(void)
{
	struct hfc_multi *card, *next;

	
	list_for_each_entry_safe(card, next, &HFClist, list)
		release_card(card);
	pci_unregister_driver(&hfcmultipci_driver);
}

static int __init
HFCmulti_init(void)
{
	int err;
	int i, xhfc = 0;
	struct hm_map m;

	printk(KERN_INFO "mISDN: HFC-multi driver %s\n", HFC_MULTI_VERSION);

#ifdef IRQ_DEBUG
	printk(KERN_DEBUG "%s: IRQ_DEBUG IS ENABLED!\n", __func__);
#endif

	spin_lock_init(&HFClock);
	spin_lock_init(&plx_lock);

	if (debug & DEBUG_HFCMULTI_INIT)
		printk(KERN_DEBUG "%s: init entered\n", __func__);

	switch (poll) {
	case 0:
		poll_timer = 6;
		poll = 128;
		break;
	case 8:
		poll_timer = 2;
		break;
	case 16:
		poll_timer = 3;
		break;
	case 32:
		poll_timer = 4;
		break;
	case 64:
		poll_timer = 5;
		break;
	case 128:
		poll_timer = 6;
		break;
	case 256:
		poll_timer = 7;
		break;
	default:
		printk(KERN_ERR
		       "%s: Wrong poll value (%d).\n", __func__, poll);
		err = -EINVAL;
		return err;

	}

	if (!clock)
		clock = 1;

	switch (hwid) {
	case HWID_MINIP4:
		xhfc = 1;
		m = hfcm_map[31];
		break;
	case HWID_MINIP8:
		xhfc = 2;
		m = hfcm_map[31];
		break;
	case HWID_MINIP16:
		xhfc = 4;
		m = hfcm_map[31];
		break;
	default:
		xhfc = 0;
	}

	for (i = 0; i < xhfc; ++i) {
		err = hfcmulti_init(&m, NULL, NULL);
		if (err) {
			printk(KERN_ERR "error registering embedded driver: "
			       "%x\n", err);
			return err;
		}
		HFC_cnt++;
		printk(KERN_INFO "%d devices registered\n", HFC_cnt);
	}

	
	err = pci_register_driver(&hfcmultipci_driver);
	if (err < 0) {
		printk(KERN_ERR "error registering pci driver: %x\n", err);
		return err;
	}

	return 0;
}


module_init(HFCmulti_init);
module_exit(HFCmulti_cleanup);
