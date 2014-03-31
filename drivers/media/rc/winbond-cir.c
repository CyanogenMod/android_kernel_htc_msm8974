/*
 *  winbond-cir.c - Driver for the Consumer IR functionality of Winbond
 *                  SuperI/O chips.
 *
 *  Currently supports the Winbond WPCD376i chip (PNP id WEC1022), but
 *  could probably support others (Winbond WEC102X, NatSemi, etc)
 *  with minor modifications.
 *
 *  Original Author: David Härdeman <david@hardeman.nu>
 *     Copyright (C) 2009 - 2011 David Härdeman <david@hardeman.nu>
 *
 *  Dedicated to my daughter Matilda, without whose loving attention this
 *  driver would have been finished in half the time and with a fraction
 *  of the bugs.
 *
 *  Written using:
 *    o Winbond WPCD376I datasheet helpfully provided by Jesse Barnes at Intel
 *    o NatSemi PC87338/PC97338 datasheet (for the serial port stuff)
 *    o DSDT dumps
 *
 *  Supported features:
 *    o IR Receive
 *    o IR Transmit
 *    o Wake-On-CIR functionality
 *
 *  To do:
 *    o Learning
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/pnp.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/leds.h>
#include <linux/spinlock.h>
#include <linux/pci_ids.h>
#include <linux/io.h>
#include <linux/bitrev.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <media/rc-core.h>

#define DRVNAME "winbond-cir"

#define WBCIR_REG_WCEIR_CTL	0x03 
#define WBCIR_REG_WCEIR_STS	0x04 
#define WBCIR_REG_WCEIR_EV_EN	0x05 
#define WBCIR_REG_WCEIR_CNTL	0x06 
#define WBCIR_REG_WCEIR_CNTH	0x07 
#define WBCIR_REG_WCEIR_INDEX	0x08 
#define WBCIR_REG_WCEIR_DATA	0x09 
#define WBCIR_REG_WCEIR_CSL	0x0A 
#define WBCIR_REG_WCEIR_CFG1	0x0B 
#define WBCIR_REG_WCEIR_CFG2	0x0C 

#define WBCIR_REG_ECEIR_CTS	0x00 
#define WBCIR_REG_ECEIR_CCTL	0x01 
#define WBCIR_REG_ECEIR_CNT_LO	0x02 
#define WBCIR_REG_ECEIR_CNT_HI	0x03 
#define WBCIR_REG_ECEIR_IREM	0x04 

#define WBCIR_REG_SP3_BSR	0x03 
				      
#define WBCIR_REG_SP3_RXDATA	0x00 
#define WBCIR_REG_SP3_TXDATA	0x00 
#define WBCIR_REG_SP3_IER	0x01 
#define WBCIR_REG_SP3_EIR	0x02 
#define WBCIR_REG_SP3_FCR	0x02 
#define WBCIR_REG_SP3_MCR	0x04 
#define WBCIR_REG_SP3_LSR	0x05 
#define WBCIR_REG_SP3_MSR	0x06 
#define WBCIR_REG_SP3_ASCR	0x07 
				      
#define WBCIR_REG_SP3_BGDL	0x00 
#define WBCIR_REG_SP3_BGDH	0x01 
#define WBCIR_REG_SP3_EXCR1	0x02 
#define WBCIR_REG_SP3_EXCR2	0x04 
#define WBCIR_REG_SP3_TXFLV	0x06 
#define WBCIR_REG_SP3_RXFLV	0x07 
				      
#define WBCIR_REG_SP3_MRID	0x00 
#define WBCIR_REG_SP3_SH_LCR	0x01 
#define WBCIR_REG_SP3_SH_FCR	0x02 
				      
#define WBCIR_REG_SP3_IRCR1	0x02 
				      
#define WBCIR_REG_SP3_IRCR2	0x04 
				      
#define WBCIR_REG_SP3_IRCR3	0x00 
#define WBCIR_REG_SP3_SIR_PW	0x02 
				      
#define WBCIR_REG_SP3_IRRXDC	0x00 
#define WBCIR_REG_SP3_IRTXMC	0x01 
#define WBCIR_REG_SP3_RCCFG	0x02 
#define WBCIR_REG_SP3_IRCFG1	0x04 
#define WBCIR_REG_SP3_IRCFG4	0x07 


#define WBCIR_IRQ_NONE		0x00
#define WBCIR_IRQ_RX		0x01
#define WBCIR_IRQ_TX_LOW	0x02
#define WBCIR_IRQ_ERR		0x04
#define WBCIR_IRQ_TX_EMPTY	0x20
#define WBCIR_LED_ENABLE	0x80
#define WBCIR_RX_AVAIL		0x01
#define WBCIR_RX_OVERRUN	0x02
#define WBCIR_TX_EOT		0x04
#define WBCIR_RX_DISABLE	0x20
#define WBCIR_TX_UNDERRUN	0x40
#define WBCIR_EXT_ENABLE	0x01
#define WBCIR_REGSEL_COMPARE	0x10
#define WBCIR_REGSEL_MASK	0x20
#define WBCIR_REG_ADDR0		0x00

enum wbcir_bank {
	WBCIR_BANK_0          = 0x00,
	WBCIR_BANK_1          = 0x80,
	WBCIR_BANK_2          = 0xE0,
	WBCIR_BANK_3          = 0xE4,
	WBCIR_BANK_4          = 0xE8,
	WBCIR_BANK_5          = 0xEC,
	WBCIR_BANK_6          = 0xF0,
	WBCIR_BANK_7          = 0xF4,
};

enum wbcir_protocol {
	IR_PROTOCOL_RC5          = 0x0,
	IR_PROTOCOL_NEC          = 0x1,
	IR_PROTOCOL_RC6          = 0x2,
};

enum wbcir_rxstate {
	WBCIR_RXSTATE_INACTIVE = 0,
	WBCIR_RXSTATE_ACTIVE,
	WBCIR_RXSTATE_ERROR
};

enum wbcir_txstate {
	WBCIR_TXSTATE_INACTIVE = 0,
	WBCIR_TXSTATE_ACTIVE,
	WBCIR_TXSTATE_DONE,
	WBCIR_TXSTATE_ERROR
};

#define WBCIR_NAME	"Winbond CIR"
#define WBCIR_ID_FAMILY          0xF1 
#define	WBCIR_ID_CHIP            0x04 
#define INVALID_SCANCODE   0x7FFFFFFF 
#define WAKEUP_IOMEM_LEN         0x10 
#define EHFUNC_IOMEM_LEN         0x10 
#define SP_IOMEM_LEN             0x08 

struct wbcir_data {
	spinlock_t spinlock;
	struct rc_dev *dev;
	struct led_classdev led;

	unsigned long wbase;        
	unsigned long ebase;        
	unsigned long sbase;        
	unsigned int  irq;          
	u8 irqmask;

	
	enum wbcir_rxstate rxstate;
	struct led_trigger *rxtrigger;
	struct ir_raw_event rxev;

	
	enum wbcir_txstate txstate;
	struct led_trigger *txtrigger;
	u32 txlen;
	u32 txoff;
	u32 *txbuf;
	wait_queue_head_t txwaitq;
	u8 txmask;
	u32 txcarrier;
};

static enum wbcir_protocol protocol = IR_PROTOCOL_RC6;
module_param(protocol, uint, 0444);
MODULE_PARM_DESC(protocol, "IR protocol to use for the power-on command "
		 "(0 = RC5, 1 = NEC, 2 = RC6A, default)");

static bool invert; 
module_param(invert, bool, 0444);
MODULE_PARM_DESC(invert, "Invert the signal from the IR receiver");

static bool txandrx; 
module_param(txandrx, bool, 0444);
MODULE_PARM_DESC(invert, "Allow simultaneous TX and RX");

static unsigned int wake_sc = 0x800F040C;
module_param(wake_sc, uint, 0644);
MODULE_PARM_DESC(wake_sc, "Scancode of the power-on IR command");

static unsigned int wake_rc6mode = 6;
module_param(wake_rc6mode, uint, 0644);
MODULE_PARM_DESC(wake_rc6mode, "RC6 mode for the power-on command "
		 "(0 = 0, 6 = 6A, default)");




static void
wbcir_set_bits(unsigned long addr, u8 bits, u8 mask)
{
	u8 val;

	val = inb(addr);
	val = ((val & ~mask) | (bits & mask));
	outb(val, addr);
}

static inline void
wbcir_select_bank(struct wbcir_data *data, enum wbcir_bank bank)
{
	outb(bank, data->sbase + WBCIR_REG_SP3_BSR);
}

static inline void
wbcir_set_irqmask(struct wbcir_data *data, u8 irqmask)
{
	if (data->irqmask == irqmask)
		return;

	wbcir_select_bank(data, WBCIR_BANK_0);
	outb(irqmask, data->sbase + WBCIR_REG_SP3_IER);
	data->irqmask = irqmask;
}

static enum led_brightness
wbcir_led_brightness_get(struct led_classdev *led_cdev)
{
	struct wbcir_data *data = container_of(led_cdev,
					       struct wbcir_data,
					       led);

	if (inb(data->ebase + WBCIR_REG_ECEIR_CTS) & WBCIR_LED_ENABLE)
		return LED_FULL;
	else
		return LED_OFF;
}

static void
wbcir_led_brightness_set(struct led_classdev *led_cdev,
			 enum led_brightness brightness)
{
	struct wbcir_data *data = container_of(led_cdev,
					       struct wbcir_data,
					       led);

	wbcir_set_bits(data->ebase + WBCIR_REG_ECEIR_CTS,
		       brightness == LED_OFF ? 0x00 : WBCIR_LED_ENABLE,
		       WBCIR_LED_ENABLE);
}

static u8
wbcir_to_rc6cells(u8 val)
{
	u8 coded = 0x00;
	int i;

	val &= 0x0F;
	for (i = 0; i < 4; i++) {
		if (val & 0x01)
			coded |= 0x02 << (i * 2);
		else
			coded |= 0x01 << (i * 2);
		val >>= 1;
	}

	return coded;
}


static void
wbcir_idle_rx(struct rc_dev *dev, bool idle)
{
	struct wbcir_data *data = dev->priv;

	if (!idle && data->rxstate == WBCIR_RXSTATE_INACTIVE) {
		data->rxstate = WBCIR_RXSTATE_ACTIVE;
		led_trigger_event(data->rxtrigger, LED_FULL);
	}

	if (idle && data->rxstate != WBCIR_RXSTATE_INACTIVE)
		
		outb(WBCIR_RX_DISABLE, data->sbase + WBCIR_REG_SP3_ASCR);
}

static void
wbcir_irq_rx(struct wbcir_data *data, struct pnp_dev *device)
{
	u8 irdata;
	DEFINE_IR_RAW_EVENT(rawir);

	
	while (inb(data->sbase + WBCIR_REG_SP3_LSR) & WBCIR_RX_AVAIL) {
		irdata = inb(data->sbase + WBCIR_REG_SP3_RXDATA);
		if (data->rxstate == WBCIR_RXSTATE_ERROR)
			continue;
		rawir.pulse = irdata & 0x80 ? false : true;
		rawir.duration = US_TO_NS((irdata & 0x7F) * 10);
		ir_raw_event_store_with_filter(data->dev, &rawir);
	}

	
	if (data->dev->idle) {
		led_trigger_event(data->rxtrigger, LED_OFF);
		data->rxstate = WBCIR_RXSTATE_INACTIVE;
	}

	ir_raw_event_handle(data->dev);
}

static void
wbcir_irq_tx(struct wbcir_data *data)
{
	unsigned int space;
	unsigned int used;
	u8 bytes[16];
	u8 byte;

	if (!data->txbuf)
		return;

	switch (data->txstate) {
	case WBCIR_TXSTATE_INACTIVE:
		
		space = 16;
		led_trigger_event(data->txtrigger, LED_FULL);
		break;
	case WBCIR_TXSTATE_ACTIVE:
		
		space = 13;
		break;
	case WBCIR_TXSTATE_ERROR:
		space = 0;
		break;
	default:
		return;
	}

	for (used = 0; used < space && data->txoff != data->txlen; used++) {
		if (data->txbuf[data->txoff] == 0) {
			data->txoff++;
			continue;
		}
		byte = min((u32)0x80, data->txbuf[data->txoff]);
		data->txbuf[data->txoff] -= byte;
		byte--;
		byte |= (data->txoff % 2 ? 0x80 : 0x00); 
		bytes[used] = byte;
	}

	while (data->txbuf[data->txoff] == 0 && data->txoff != data->txlen)
		data->txoff++;

	if (used == 0) {
		
		if (data->txstate == WBCIR_TXSTATE_ERROR)
			
			outb(WBCIR_TX_UNDERRUN, data->sbase + WBCIR_REG_SP3_ASCR);
		else
			data->txstate = WBCIR_TXSTATE_DONE;
		wbcir_set_irqmask(data, WBCIR_IRQ_RX | WBCIR_IRQ_ERR);
		led_trigger_event(data->txtrigger, LED_OFF);
		wake_up(&data->txwaitq);
	} else if (data->txoff == data->txlen) {
		
		outsb(data->sbase + WBCIR_REG_SP3_TXDATA, bytes, used - 1);
		outb(WBCIR_TX_EOT, data->sbase + WBCIR_REG_SP3_ASCR);
		outb(bytes[used - 1], data->sbase + WBCIR_REG_SP3_TXDATA);
		wbcir_set_irqmask(data, WBCIR_IRQ_RX | WBCIR_IRQ_ERR |
				  WBCIR_IRQ_TX_EMPTY);
	} else {
		
		outsb(data->sbase + WBCIR_REG_SP3_RXDATA, bytes, used);
		if (data->txstate == WBCIR_TXSTATE_INACTIVE) {
			wbcir_set_irqmask(data, WBCIR_IRQ_RX | WBCIR_IRQ_ERR |
					  WBCIR_IRQ_TX_LOW);
			data->txstate = WBCIR_TXSTATE_ACTIVE;
		}
	}
}

static irqreturn_t
wbcir_irq_handler(int irqno, void *cookie)
{
	struct pnp_dev *device = cookie;
	struct wbcir_data *data = pnp_get_drvdata(device);
	unsigned long flags;
	u8 status;

	spin_lock_irqsave(&data->spinlock, flags);
	wbcir_select_bank(data, WBCIR_BANK_0);
	status = inb(data->sbase + WBCIR_REG_SP3_EIR);
	status &= data->irqmask;

	if (!status) {
		spin_unlock_irqrestore(&data->spinlock, flags);
		return IRQ_NONE;
	}

	if (status & WBCIR_IRQ_ERR) {
		
		if (inb(data->sbase + WBCIR_REG_SP3_LSR) & WBCIR_RX_OVERRUN) {
			data->rxstate = WBCIR_RXSTATE_ERROR;
			ir_raw_event_reset(data->dev);
		}

		
		if (inb(data->sbase + WBCIR_REG_SP3_ASCR) & WBCIR_TX_UNDERRUN)
			data->txstate = WBCIR_TXSTATE_ERROR;
	}

	if (status & WBCIR_IRQ_RX)
		wbcir_irq_rx(data, device);

	if (status & (WBCIR_IRQ_TX_LOW | WBCIR_IRQ_TX_EMPTY))
		wbcir_irq_tx(data);

	spin_unlock_irqrestore(&data->spinlock, flags);
	return IRQ_HANDLED;
}


static int
wbcir_txcarrier(struct rc_dev *dev, u32 carrier)
{
	struct wbcir_data *data = dev->priv;
	unsigned long flags;
	u8 val;
	u32 freq;

	freq = DIV_ROUND_CLOSEST(carrier, 1000);
	if (freq < 30 || freq > 60)
		return -EINVAL;

	switch (freq) {
	case 58:
	case 59:
	case 60:
		val = freq - 58;
		freq *= 1000;
		break;
	case 57:
		val = freq - 27;
		freq = 56900;
		break;
	default:
		val = freq - 27;
		freq *= 1000;
		break;
	}

	spin_lock_irqsave(&data->spinlock, flags);
	if (data->txstate != WBCIR_TXSTATE_INACTIVE) {
		spin_unlock_irqrestore(&data->spinlock, flags);
		return -EBUSY;
	}

	if (data->txcarrier != freq) {
		wbcir_select_bank(data, WBCIR_BANK_7);
		wbcir_set_bits(data->sbase + WBCIR_REG_SP3_IRTXMC, val, 0x1F);
		data->txcarrier = freq;
	}

	spin_unlock_irqrestore(&data->spinlock, flags);
	return 0;
}

static int
wbcir_txmask(struct rc_dev *dev, u32 mask)
{
	struct wbcir_data *data = dev->priv;
	unsigned long flags;
	u8 val;

	
	switch (mask) {
	case 0x1:
		val = 0x0;
		break;
	case 0x2:
		val = 0x1;
		break;
	case 0x4:
		val = 0x2;
		break;
	case 0x8:
		val = 0x3;
		break;
	default:
		return -EINVAL;
	}

	spin_lock_irqsave(&data->spinlock, flags);
	if (data->txstate != WBCIR_TXSTATE_INACTIVE) {
		spin_unlock_irqrestore(&data->spinlock, flags);
		return -EBUSY;
	}

	if (data->txmask != mask) {
		wbcir_set_bits(data->ebase + WBCIR_REG_ECEIR_CTS, val, 0x0c);
		data->txmask = mask;
	}

	spin_unlock_irqrestore(&data->spinlock, flags);
	return 0;
}

static int
wbcir_tx(struct rc_dev *dev, unsigned *buf, unsigned count)
{
	struct wbcir_data *data = dev->priv;
	unsigned i;
	unsigned long flags;

	
	spin_lock_irqsave(&data->spinlock, flags);
	if (data->txstate != WBCIR_TXSTATE_INACTIVE) {
		spin_unlock_irqrestore(&data->spinlock, flags);
		return -EBUSY;
	}

	
	for (i = 0; i < count; i++)
		buf[i] = DIV_ROUND_CLOSEST(buf[i], 10);

	
	data->txbuf = buf;
	data->txlen = count;
	data->txoff = 0;
	wbcir_irq_tx(data);

	
	while (data->txstate == WBCIR_TXSTATE_ACTIVE) {
		spin_unlock_irqrestore(&data->spinlock, flags);
		wait_event(data->txwaitq, data->txstate != WBCIR_TXSTATE_ACTIVE);
		spin_lock_irqsave(&data->spinlock, flags);
	}

	
	if (data->txstate == WBCIR_TXSTATE_ERROR)
		count = -EAGAIN;
	data->txstate = WBCIR_TXSTATE_INACTIVE;
	data->txbuf = NULL;
	spin_unlock_irqrestore(&data->spinlock, flags);

	return count;
}


static void
wbcir_shutdown(struct pnp_dev *device)
{
	struct device *dev = &device->dev;
	struct wbcir_data *data = pnp_get_drvdata(device);
	bool do_wake = true;
	u8 match[11];
	u8 mask[11];
	u8 rc6_csl = 0;
	int i;

	memset(match, 0, sizeof(match));
	memset(mask, 0, sizeof(mask));

	if (wake_sc == INVALID_SCANCODE || !device_may_wakeup(dev)) {
		do_wake = false;
		goto finish;
	}

	switch (protocol) {
	case IR_PROTOCOL_RC5:
		if (wake_sc > 0xFFF) {
			do_wake = false;
			dev_err(dev, "RC5 - Invalid wake scancode\n");
			break;
		}

		
		mask[0] = 0xFF;
		mask[1] = 0x17;

		match[0]  = (wake_sc & 0x003F);      
		match[0] |= (wake_sc & 0x0180) >> 1; 
		match[1]  = (wake_sc & 0x0E00) >> 9; 
		if (!(wake_sc & 0x0040))             
			match[1] |= 0x10;

		break;

	case IR_PROTOCOL_NEC:
		if (wake_sc > 0xFFFFFF) {
			do_wake = false;
			dev_err(dev, "NEC - Invalid wake scancode\n");
			break;
		}

		mask[0] = mask[1] = mask[2] = mask[3] = 0xFF;

		match[1] = bitrev8((wake_sc & 0xFF));
		match[0] = ~match[1];

		match[3] = bitrev8((wake_sc & 0xFF00) >> 8);
		if (wake_sc > 0xFFFF)
			match[2] = bitrev8((wake_sc & 0xFF0000) >> 16);
		else
			match[2] = ~match[3];

		break;

	case IR_PROTOCOL_RC6:

		if (wake_rc6mode == 0) {
			if (wake_sc > 0xFFFF) {
				do_wake = false;
				dev_err(dev, "RC6 - Invalid wake scancode\n");
				break;
			}

			
			match[0] = wbcir_to_rc6cells(wake_sc >>  0);
			mask[0]  = 0xFF;
			match[1] = wbcir_to_rc6cells(wake_sc >>  4);
			mask[1]  = 0xFF;

			
			match[2] = wbcir_to_rc6cells(wake_sc >>  8);
			mask[2]  = 0xFF;
			match[3] = wbcir_to_rc6cells(wake_sc >> 12);
			mask[3]  = 0xFF;

			
			match[4] = 0x50; 
			mask[4]  = 0xF0;
			match[5] = 0x09; 
			mask[5]  = 0x0F;

			rc6_csl = 44;

		} else if (wake_rc6mode == 6) {
			i = 0;

			
			match[i]  = wbcir_to_rc6cells(wake_sc >>  0);
			mask[i++] = 0xFF;
			match[i]  = wbcir_to_rc6cells(wake_sc >>  4);
			mask[i++] = 0xFF;

			
			match[i]  = wbcir_to_rc6cells(wake_sc >>  8);
			mask[i++] = 0xFF;
			match[i]  = wbcir_to_rc6cells(wake_sc >> 12);
			mask[i++] = 0x3F;

			
			match[i]  = wbcir_to_rc6cells(wake_sc >> 16);
			mask[i++] = 0xFF;
			match[i]  = wbcir_to_rc6cells(wake_sc >> 20);
			mask[i++] = 0xFF;

			if (wake_sc & 0x80000000) {
				
				match[i]  = wbcir_to_rc6cells(wake_sc >> 24);
				mask[i++] = 0xFF;
				match[i]  = wbcir_to_rc6cells(wake_sc >> 28);
				mask[i++] = 0xFF;
				rc6_csl = 76;
			} else if (wake_sc <= 0x007FFFFF) {
				rc6_csl = 60;
			} else {
				do_wake = false;
				dev_err(dev, "RC6 - Invalid wake scancode\n");
				break;
			}

			
			match[i]  = 0x93; 
			mask[i++] = 0xFF;
			match[i]  = 0x0A; 
			mask[i++] = 0x0F;

		} else {
			do_wake = false;
			dev_err(dev, "RC6 - Invalid wake mode\n");
		}

		break;

	default:
		do_wake = false;
		break;
	}

finish:
	if (do_wake) {
		
		wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_INDEX,
			       WBCIR_REGSEL_COMPARE | WBCIR_REG_ADDR0,
			       0x3F);
		outsb(data->wbase + WBCIR_REG_WCEIR_DATA, match, 11);
		wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_INDEX,
			       WBCIR_REGSEL_MASK | WBCIR_REG_ADDR0,
			       0x3F);
		outsb(data->wbase + WBCIR_REG_WCEIR_DATA, mask, 11);

		
		outb(rc6_csl, data->wbase + WBCIR_REG_WCEIR_CSL);

		
		wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_STS, 0x17, 0x17);

		
		wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_EV_EN, 0x01, 0x07);

		
		wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_CTL, 0x01, 0x01);

	} else {
		
		wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_EV_EN, 0x00, 0x07);

		
		wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_CTL, 0x00, 0x01);
	}

	wbcir_set_irqmask(data, WBCIR_IRQ_NONE);
	disable_irq(data->irq);

	
	led_trigger_event(data->rxtrigger, LED_OFF);
	led_trigger_event(data->txtrigger, LED_OFF);
}

static int
wbcir_suspend(struct pnp_dev *device, pm_message_t state)
{
	wbcir_shutdown(device);
	return 0;
}

static void
wbcir_init_hw(struct wbcir_data *data)
{
	u8 tmp;

	
	wbcir_set_irqmask(data, WBCIR_IRQ_NONE);

	
	tmp = protocol << 4;
	if (invert)
		tmp |= 0x08;
	outb(tmp, data->wbase + WBCIR_REG_WCEIR_CTL);

	
	wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_STS, 0x17, 0x17);

	
	wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_EV_EN, 0x00, 0x07);

	
	wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_CFG1, 0x4A, 0x7F);

	
	if (invert)
		outb(0x04, data->ebase + WBCIR_REG_ECEIR_CCTL);
	else
		outb(0x00, data->ebase + WBCIR_REG_ECEIR_CCTL);

	outb(0x10, data->ebase + WBCIR_REG_ECEIR_CTS);
	data->txmask = 0x1;

	
	wbcir_select_bank(data, WBCIR_BANK_2);
	outb(WBCIR_EXT_ENABLE, data->sbase + WBCIR_REG_SP3_EXCR1);


	
	outb(0x30, data->sbase + WBCIR_REG_SP3_EXCR2);

	
	outb(0x0F, data->sbase + WBCIR_REG_SP3_BGDL);
	outb(0x00, data->sbase + WBCIR_REG_SP3_BGDH);

	
	wbcir_select_bank(data, WBCIR_BANK_0);
	outb(0xC0, data->sbase + WBCIR_REG_SP3_MCR);
	inb(data->sbase + WBCIR_REG_SP3_LSR); 
	inb(data->sbase + WBCIR_REG_SP3_MSR); 

	
	wbcir_select_bank(data, WBCIR_BANK_7);
	outb(0x90, data->sbase + WBCIR_REG_SP3_RCCFG);

	
	wbcir_select_bank(data, WBCIR_BANK_4);
	outb(0x00, data->sbase + WBCIR_REG_SP3_IRCR1);

	
	wbcir_select_bank(data, WBCIR_BANK_5);
	outb(txandrx ? 0x03 : 0x02, data->sbase + WBCIR_REG_SP3_IRCR2);

	
	wbcir_select_bank(data, WBCIR_BANK_6);
	outb(0x20, data->sbase + WBCIR_REG_SP3_IRCR3);

	
	wbcir_select_bank(data, WBCIR_BANK_7);
	outb(0xF2, data->sbase + WBCIR_REG_SP3_IRRXDC);

	
	outb(0x69, data->sbase + WBCIR_REG_SP3_IRTXMC);
	data->txcarrier = 36000;

	
	if (invert)
		outb(0x10, data->sbase + WBCIR_REG_SP3_IRCFG4);
	else
		outb(0x00, data->sbase + WBCIR_REG_SP3_IRCFG4);

	
	wbcir_select_bank(data, WBCIR_BANK_0);
	outb(0x97, data->sbase + WBCIR_REG_SP3_FCR);

	
	outb(0xE0, data->sbase + WBCIR_REG_SP3_ASCR);

	
	data->rxstate = WBCIR_RXSTATE_INACTIVE;
	data->rxev.duration = 0;
	ir_raw_event_reset(data->dev);
	ir_raw_event_handle(data->dev);

	if (data->txstate == WBCIR_TXSTATE_ACTIVE) {
		data->txstate = WBCIR_TXSTATE_ERROR;
		wake_up(&data->txwaitq);
	}

	
	wbcir_set_irqmask(data, WBCIR_IRQ_RX | WBCIR_IRQ_ERR);
}

static int
wbcir_resume(struct pnp_dev *device)
{
	struct wbcir_data *data = pnp_get_drvdata(device);

	wbcir_init_hw(data);
	enable_irq(data->irq);

	return 0;
}

static int __devinit
wbcir_probe(struct pnp_dev *device, const struct pnp_device_id *dev_id)
{
	struct device *dev = &device->dev;
	struct wbcir_data *data;
	int err;

	if (!(pnp_port_len(device, 0) == EHFUNC_IOMEM_LEN &&
	      pnp_port_len(device, 1) == WAKEUP_IOMEM_LEN &&
	      pnp_port_len(device, 2) == SP_IOMEM_LEN)) {
		dev_err(dev, "Invalid resources\n");
		return -ENODEV;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}

	pnp_set_drvdata(device, data);

	spin_lock_init(&data->spinlock);
	init_waitqueue_head(&data->txwaitq);
	data->ebase = pnp_port_start(device, 0);
	data->wbase = pnp_port_start(device, 1);
	data->sbase = pnp_port_start(device, 2);
	data->irq = pnp_irq(device, 0);

	if (data->wbase == 0 || data->ebase == 0 ||
	    data->sbase == 0 || data->irq == 0) {
		err = -ENODEV;
		dev_err(dev, "Invalid resources\n");
		goto exit_free_data;
	}

	dev_dbg(&device->dev, "Found device "
		"(w: 0x%lX, e: 0x%lX, s: 0x%lX, i: %u)\n",
		data->wbase, data->ebase, data->sbase, data->irq);

	led_trigger_register_simple("cir-tx", &data->txtrigger);
	if (!data->txtrigger) {
		err = -ENOMEM;
		goto exit_free_data;
	}

	led_trigger_register_simple("cir-rx", &data->rxtrigger);
	if (!data->rxtrigger) {
		err = -ENOMEM;
		goto exit_unregister_txtrigger;
	}

	data->led.name = "cir::activity";
	data->led.default_trigger = "cir-rx";
	data->led.brightness_set = wbcir_led_brightness_set;
	data->led.brightness_get = wbcir_led_brightness_get;
	err = led_classdev_register(&device->dev, &data->led);
	if (err)
		goto exit_unregister_rxtrigger;

	data->dev = rc_allocate_device();
	if (!data->dev) {
		err = -ENOMEM;
		goto exit_unregister_led;
	}

	data->dev->driver_type = RC_DRIVER_IR_RAW;
	data->dev->driver_name = WBCIR_NAME;
	data->dev->input_name = WBCIR_NAME;
	data->dev->input_phys = "wbcir/cir0";
	data->dev->input_id.bustype = BUS_HOST;
	data->dev->input_id.vendor = PCI_VENDOR_ID_WINBOND;
	data->dev->input_id.product = WBCIR_ID_FAMILY;
	data->dev->input_id.version = WBCIR_ID_CHIP;
	data->dev->map_name = RC_MAP_RC6_MCE;
	data->dev->s_idle = wbcir_idle_rx;
	data->dev->s_tx_mask = wbcir_txmask;
	data->dev->s_tx_carrier = wbcir_txcarrier;
	data->dev->tx_ir = wbcir_tx;
	data->dev->priv = data;
	data->dev->dev.parent = &device->dev;

	if (!request_region(data->wbase, WAKEUP_IOMEM_LEN, DRVNAME)) {
		dev_err(dev, "Region 0x%lx-0x%lx already in use!\n",
			data->wbase, data->wbase + WAKEUP_IOMEM_LEN - 1);
		err = -EBUSY;
		goto exit_free_rc;
	}

	if (!request_region(data->ebase, EHFUNC_IOMEM_LEN, DRVNAME)) {
		dev_err(dev, "Region 0x%lx-0x%lx already in use!\n",
			data->ebase, data->ebase + EHFUNC_IOMEM_LEN - 1);
		err = -EBUSY;
		goto exit_release_wbase;
	}

	if (!request_region(data->sbase, SP_IOMEM_LEN, DRVNAME)) {
		dev_err(dev, "Region 0x%lx-0x%lx already in use!\n",
			data->sbase, data->sbase + SP_IOMEM_LEN - 1);
		err = -EBUSY;
		goto exit_release_ebase;
	}

	err = request_irq(data->irq, wbcir_irq_handler,
			  IRQF_DISABLED, DRVNAME, device);
	if (err) {
		dev_err(dev, "Failed to claim IRQ %u\n", data->irq);
		err = -EBUSY;
		goto exit_release_sbase;
	}

	err = rc_register_device(data->dev);
	if (err)
		goto exit_free_irq;

	device_init_wakeup(&device->dev, 1);

	wbcir_init_hw(data);

	return 0;

exit_free_irq:
	free_irq(data->irq, device);
exit_release_sbase:
	release_region(data->sbase, SP_IOMEM_LEN);
exit_release_ebase:
	release_region(data->ebase, EHFUNC_IOMEM_LEN);
exit_release_wbase:
	release_region(data->wbase, WAKEUP_IOMEM_LEN);
exit_free_rc:
	rc_free_device(data->dev);
exit_unregister_led:
	led_classdev_unregister(&data->led);
exit_unregister_rxtrigger:
	led_trigger_unregister_simple(data->rxtrigger);
exit_unregister_txtrigger:
	led_trigger_unregister_simple(data->txtrigger);
exit_free_data:
	kfree(data);
	pnp_set_drvdata(device, NULL);
exit:
	return err;
}

static void __devexit
wbcir_remove(struct pnp_dev *device)
{
	struct wbcir_data *data = pnp_get_drvdata(device);

	
	wbcir_set_irqmask(data, WBCIR_IRQ_NONE);
	free_irq(data->irq, device);

	
	wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_STS, 0x17, 0x17);

	
	wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_CTL, 0x00, 0x01);

	
	wbcir_set_bits(data->wbase + WBCIR_REG_WCEIR_EV_EN, 0x00, 0x07);

	rc_unregister_device(data->dev);

	led_trigger_unregister_simple(data->rxtrigger);
	led_trigger_unregister_simple(data->txtrigger);
	led_classdev_unregister(&data->led);

	
	wbcir_led_brightness_set(&data->led, LED_OFF);

	release_region(data->wbase, WAKEUP_IOMEM_LEN);
	release_region(data->ebase, EHFUNC_IOMEM_LEN);
	release_region(data->sbase, SP_IOMEM_LEN);

	kfree(data);

	pnp_set_drvdata(device, NULL);
}

static const struct pnp_device_id wbcir_ids[] = {
	{ "WEC1022", 0 },
	{ "", 0 }
};
MODULE_DEVICE_TABLE(pnp, wbcir_ids);

static struct pnp_driver wbcir_driver = {
	.name     = WBCIR_NAME,
	.id_table = wbcir_ids,
	.probe    = wbcir_probe,
	.remove   = __devexit_p(wbcir_remove),
	.suspend  = wbcir_suspend,
	.resume   = wbcir_resume,
	.shutdown = wbcir_shutdown
};

static int __init
wbcir_init(void)
{
	int ret;

	switch (protocol) {
	case IR_PROTOCOL_RC5:
	case IR_PROTOCOL_NEC:
	case IR_PROTOCOL_RC6:
		break;
	default:
		pr_err("Invalid power-on protocol\n");
	}

	ret = pnp_register_driver(&wbcir_driver);
	if (ret)
		pr_err("Unable to register driver\n");

	return ret;
}

static void __exit
wbcir_exit(void)
{
	pnp_unregister_driver(&wbcir_driver);
}

module_init(wbcir_init);
module_exit(wbcir_exit);

MODULE_AUTHOR("David Härdeman <david@hardeman.nu>");
MODULE_DESCRIPTION("Winbond SuperI/O Consumer IR Driver");
MODULE_LICENSE("GPL");
