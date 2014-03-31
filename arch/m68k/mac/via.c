
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/irq.h>

#include <asm/bootinfo.h>
#include <asm/macintosh.h>
#include <asm/macints.h>
#include <asm/mac_via.h>
#include <asm/mac_psc.h>
#include <asm/mac_oss.h>

volatile __u8 *via1, *via2;
int rbv_present;
int via_alt_mapping;
EXPORT_SYMBOL(via_alt_mapping);
static __u8 rbv_clear;


static int gIER,gIFR,gBufA,gBufB;


#define TICK_SIZE		10000
#define MAC_CLOCK_TICK		(783300/HZ)		
#define MAC_CLOCK_LOW		(MAC_CLOCK_TICK&0xFF)
#define MAC_CLOCK_HIGH		(MAC_CLOCK_TICK>>8)


/*
 * On Macs with a genuine VIA chip there is no way to mask an individual slot
 * interrupt. This limitation also seems to apply to VIA clone logic cores in
 * Quadra-like ASICs. (RBV and OSS machines don't have this limitation.)
 *
 * We used to fake it by configuring the relevent VIA pin as an output
 * (to mask the interrupt) or input (to unmask). That scheme did not work on
 * (at least) the Quadra 700. A NuBus card's /NMRQ signal is an open-collector
 * circuit (see Designing Cards and Drivers for Macintosh II and Macintosh SE,
 * p. 10-11 etc) but VIA outputs are not (see datasheet).
 *
 * Driving these outputs high must cause the VIA to source current and the
 * card to sink current when it asserts /NMRQ. Current will flow but the pin
 * voltage is uncertain and so the /NMRQ condition may still cause a transition
 * at the VIA2 CA1 input (which explains the lost interrupts). A side effect
 * is that a disabled slot IRQ can never be tested as pending or not.
 *
 * Driving these outputs low doesn't work either. All the slot /NMRQ lines are
 * (active low) OR'd together to generate the CA1 (aka "SLOTS") interrupt (see
 * The Guide To Macintosh Family Hardware, 2nd edition p. 167). If we drive a
 * disabled /NMRQ line low, the falling edge immediately triggers a CA1
 * interrupt and all slot interrupts after that will generate no transition
 * and therefore no interrupt, even after being re-enabled.
 *
 * So we make the VIA port A I/O lines inputs and use nubus_disabled to keep
 * track of their states. When any slot IRQ becomes disabled we mask the CA1
 * umbrella interrupt. Only when all slot IRQs become enabled do we unmask
 * the CA1 interrupt. It must remain enabled even when cards have no interrupt
 * handler registered. Drivers must therefore disable a slot interrupt at the
 * device before they call free_irq (like shared and autovector interrupts).
 *
 * There is also a related problem when MacOS is used to boot Linux. A network
 * card brought up by a MacOS driver may raise an interrupt while Linux boots.
 * This can be fatal since it can't be handled until the right driver loads
 * (if such a driver exists at all). Apparently related to this hardware
 * limitation, "Designing Cards and Drivers", p. 9-8, says that a slot
 * interrupt with no driver would crash MacOS (the book was written before
 * the appearance of Macs with RBV or OSS).
 */

static u8 nubus_disabled;

void via_debug_dump(void);


void __init via_init(void)
{
	switch(macintosh_config->via_type) {

		

		case MAC_VIA_IICI:
			via1 = (void *) VIA1_BASE;
			if (macintosh_config->ident == MAC_MODEL_IIFX) {
				via2 = NULL;
				rbv_present = 0;
				oss_present = 1;
			} else {
				via2 = (void *) RBV_BASE;
				rbv_present = 1;
				oss_present = 0;
			}
			if (macintosh_config->ident == MAC_MODEL_LCIII) {
				rbv_clear = 0x00;
			} else {
				
				
				
				rbv_clear = 0x80;
			}
			gIER = rIER;
			gIFR = rIFR;
			gBufA = rSIFR;
			gBufB = rBufB;
			break;

		

		case MAC_VIA_QUADRA:
		case MAC_VIA_II:
			via1 = (void *) VIA1_BASE;
			via2 = (void *) VIA2_BASE;
			rbv_present = 0;
			oss_present = 0;
			rbv_clear = 0x00;
			gIER = vIER;
			gIFR = vIFR;
			gBufA = vBufA;
			gBufB = vBufB;
			break;
		default:
			panic("UNKNOWN VIA TYPE");
	}

	printk(KERN_INFO "VIA1 at %p is a 6522 or clone\n", via1);

	printk(KERN_INFO "VIA2 at %p is ", via2);
	if (rbv_present) {
		printk("an RBV\n");
	} else if (oss_present) {
		printk("an OSS\n");
	} else {
		printk("a 6522 or clone\n");
	}

#ifdef DEBUG_VIA
	via_debug_dump();
#endif


	via1[vIER] = 0x7F;
	via1[vIFR] = 0x7F;
	via1[vT1LL] = 0;
	via1[vT1LH] = 0;
	via1[vT1CL] = 0;
	via1[vT1CH] = 0;
	via1[vT2CL] = 0;
	via1[vT2CH] = 0;
	via1[vACR] &= ~0xC0; 
	via1[vACR] &= ~0x03; 


	if (macintosh_config->ident == MAC_MODEL_SE30) {
		via1[vDirB] |= 0x40;
		via1[vBufB] |= 0x40;
	}


	via1[vDirB] |= (VIA1B_vRTCEnb | VIA1B_vRTCClk | VIA1B_vRTCData);
	via1[vBufB] |= (VIA1B_vRTCEnb | VIA1B_vRTCClk);

	

	if (oss_present)
		return;

	if ((macintosh_config->via_type == MAC_VIA_QUADRA) &&
	    (macintosh_config->adb_type != MAC_ADB_PB1) &&
	    (macintosh_config->adb_type != MAC_ADB_PB2) &&
	    (macintosh_config->ident    != MAC_MODEL_C660) &&
	    (macintosh_config->ident    != MAC_MODEL_Q840)) {
		via_alt_mapping = 1;
		via1[vDirB] |= 0x40;
		via1[vBufB] &= ~0x40;
	} else {
		via_alt_mapping = 0;
	}


	via2[gIER] = 0x7F;
	via2[gIFR] = 0x7F | rbv_clear;
	if (!rbv_present) {
		via2[vT1LL] = 0;
		via2[vT1LH] = 0;
		via2[vT1CL] = 0;
		via2[vT1CH] = 0;
		via2[vT2CL] = 0;
		via2[vT2CH] = 0;
		via2[vACR] &= ~0xC0; 
		via2[vACR] &= ~0x03; 
	}

	

	if (rbv_present)
		return;


	pr_debug("VIA2 vPCR is 0x%02X\n", via2[vPCR]);
	if (macintosh_config->via_type == MAC_VIA_II) {
		
		via2[vPCR] = 0x66;
	} else {
		
		via2[vPCR] = 0x22;
	}
}


void __init via_init_clock(irq_handler_t func)
{
	via1[vACR] |= 0x40;
	via1[vT1LL] = MAC_CLOCK_LOW;
	via1[vT1LH] = MAC_CLOCK_HIGH;
	via1[vT1CL] = MAC_CLOCK_LOW;
	via1[vT1CH] = MAC_CLOCK_HIGH;

	if (request_irq(IRQ_MAC_TIMER_1, func, 0, "timer", func))
		pr_err("Couldn't register %s interrupt\n", "timer");
}


void via_debug_dump(void)
{
	printk(KERN_DEBUG "VIA1: DDRA = 0x%02X DDRB = 0x%02X ACR = 0x%02X\n",
		(uint) via1[vDirA], (uint) via1[vDirB], (uint) via1[vACR]);
	printk(KERN_DEBUG "         PCR = 0x%02X  IFR = 0x%02X IER = 0x%02X\n",
		(uint) via1[vPCR], (uint) via1[vIFR], (uint) via1[vIER]);
	if (oss_present) {
		printk(KERN_DEBUG "VIA2: <OSS>\n");
	} else if (rbv_present) {
		printk(KERN_DEBUG "VIA2:  IFR = 0x%02X  IER = 0x%02X\n",
			(uint) via2[rIFR], (uint) via2[rIER]);
		printk(KERN_DEBUG "      SIFR = 0x%02X SIER = 0x%02X\n",
			(uint) via2[rSIFR], (uint) via2[rSIER]);
	} else {
		printk(KERN_DEBUG "VIA2: DDRA = 0x%02X DDRB = 0x%02X ACR = 0x%02X\n",
			(uint) via2[vDirA], (uint) via2[vDirB],
			(uint) via2[vACR]);
		printk(KERN_DEBUG "         PCR = 0x%02X  IFR = 0x%02X IER = 0x%02X\n",
			(uint) via2[vPCR],
			(uint) via2[vIFR], (uint) via2[vIER]);
	}
}


unsigned long mac_gettimeoffset (void)
{
	unsigned long ticks, offset = 0;

	
	ticks = via1[vT1CL] | (via1[vT1CH] << 8);
	
	if (ticks > MAC_CLOCK_TICK - MAC_CLOCK_TICK / 50)
		
		if (via1[vIFR] & 0x40) offset = TICK_SIZE;

	ticks = MAC_CLOCK_TICK - ticks;
	ticks = ticks * 10000L / MAC_CLOCK_TICK;

	return ticks + offset;
}


void via_flush_cache(void)
{
	via2[gBufB] &= ~VIA2B_vMode32;
	via2[gBufB] |= VIA2B_vMode32;
}


int via_get_cache_disable(void)
{
	
	if (!via2) {
		printk(KERN_ERR "via_get_cache_disable called on a non-VIA machine!\n");
		return 1;
	}

	return (int) via2[gBufB] & VIA2B_vCDis;
}


void __init via_nubus_init(void)
{
	

	if ((macintosh_config->adb_type != MAC_ADB_PB1) &&
	    (macintosh_config->adb_type != MAC_ADB_PB2)) {
		
		if (!rbv_present)
			via2[vDirB] |= 0x02;

		
		
		via2[gBufB] |= 0x02;
	}


	switch (macintosh_config->via_type) {
	case MAC_VIA_II:
	case MAC_VIA_QUADRA:
		pr_debug("VIA2 vDirA is 0x%02X\n", via2[vDirA]);
		break;
	case MAC_VIA_IICI:
		
		via2[rSIER] = 0x7F;
		break;
	}
}

void via_nubus_irq_startup(int irq)
{
	int irq_idx = IRQ_IDX(irq);

	switch (macintosh_config->via_type) {
	case MAC_VIA_II:
	case MAC_VIA_QUADRA:
		
		if (macintosh_config->via_type == MAC_VIA_II) {
			
			via2[vDirA] &= 0xC0 | ~(1 << irq_idx);
		} else {
			
			via2[vDirA] &= 0x80 | ~(1 << irq_idx);
		}
		
	case MAC_VIA_IICI:
		via_irq_enable(irq);
		break;
	}
}

void via_nubus_irq_shutdown(int irq)
{
	switch (macintosh_config->via_type) {
	case MAC_VIA_II:
	case MAC_VIA_QUADRA:
		
		via_irq_enable(irq);
		break;
	case MAC_VIA_IICI:
		via_irq_disable(irq);
		break;
	}
}


void via1_irq(unsigned int irq, struct irq_desc *desc)
{
	int irq_num;
	unsigned char irq_bit, events;

	events = via1[vIFR] & via1[vIER] & 0x7F;
	if (!events)
		return;

	irq_num = VIA1_SOURCE_BASE;
	irq_bit = 1;
	do {
		if (events & irq_bit) {
			via1[vIFR] = irq_bit;
			generic_handle_irq(irq_num);
		}
		++irq_num;
		irq_bit <<= 1;
	} while (events >= irq_bit);
}

static void via2_irq(unsigned int irq, struct irq_desc *desc)
{
	int irq_num;
	unsigned char irq_bit, events;

	events = via2[gIFR] & via2[gIER] & 0x7F;
	if (!events)
		return;

	irq_num = VIA2_SOURCE_BASE;
	irq_bit = 1;
	do {
		if (events & irq_bit) {
			via2[gIFR] = irq_bit | rbv_clear;
			generic_handle_irq(irq_num);
		}
		++irq_num;
		irq_bit <<= 1;
	} while (events >= irq_bit);
}


void via_nubus_irq(unsigned int irq, struct irq_desc *desc)
{
	int slot_irq;
	unsigned char slot_bit, events;

	events = ~via2[gBufA] & 0x7F;
	if (rbv_present)
		events &= via2[rSIER];
	else
		events &= ~via2[vDirA];
	if (!events)
		return;

	do {
		slot_irq = IRQ_NUBUS_F;
		slot_bit = 0x40;
		do {
			if (events & slot_bit) {
				events &= ~slot_bit;
				generic_handle_irq(slot_irq);
			}
			--slot_irq;
			slot_bit >>= 1;
		} while (events);

 		
		via2[gIFR] = 0x02 | rbv_clear;
		events = ~via2[gBufA] & 0x7F;
		if (rbv_present)
			events &= via2[rSIER];
		else
			events &= ~via2[vDirA];
	} while (events);
}


void __init via_register_interrupts(void)
{
	if (via_alt_mapping) {
		
		irq_set_chained_handler(IRQ_AUTO_1, via1_irq);
		
		irq_set_chained_handler(IRQ_AUTO_6, via1_irq);
	} else {
		irq_set_chained_handler(IRQ_AUTO_1, via1_irq);
	}
	irq_set_chained_handler(IRQ_AUTO_2, via2_irq);
	irq_set_chained_handler(IRQ_MAC_NUBUS, via_nubus_irq);
}

void via_irq_enable(int irq) {
	int irq_src	= IRQ_SRC(irq);
	int irq_idx	= IRQ_IDX(irq);

#ifdef DEBUG_IRQUSE
	printk(KERN_DEBUG "via_irq_enable(%d)\n", irq);
#endif

	if (irq_src == 1) {
		via1[vIER] = IER_SET_BIT(irq_idx);
	} else if (irq_src == 2) {
		if (irq != IRQ_MAC_NUBUS || nubus_disabled == 0)
			via2[gIER] = IER_SET_BIT(irq_idx);
	} else if (irq_src == 7) {
		switch (macintosh_config->via_type) {
		case MAC_VIA_II:
		case MAC_VIA_QUADRA:
			nubus_disabled &= ~(1 << irq_idx);
			
			if (!nubus_disabled)
				via2[gIER] = IER_SET_BIT(1);
			break;
		case MAC_VIA_IICI:
			via2[rSIER] = IER_SET_BIT(irq_idx);
			break;
		}
	}
}

void via_irq_disable(int irq) {
	int irq_src	= IRQ_SRC(irq);
	int irq_idx	= IRQ_IDX(irq);

#ifdef DEBUG_IRQUSE
	printk(KERN_DEBUG "via_irq_disable(%d)\n", irq);
#endif

	if (irq_src == 1) {
		via1[vIER] = IER_CLR_BIT(irq_idx);
	} else if (irq_src == 2) {
		via2[gIER] = IER_CLR_BIT(irq_idx);
	} else if (irq_src == 7) {
		switch (macintosh_config->via_type) {
		case MAC_VIA_II:
		case MAC_VIA_QUADRA:
			nubus_disabled |= 1 << irq_idx;
			if (nubus_disabled)
				via2[gIER] = IER_CLR_BIT(1);
			break;
		case MAC_VIA_IICI:
			via2[rSIER] = IER_CLR_BIT(irq_idx);
			break;
		}
	}
}

void via1_set_head(int head)
{
	if (head == 0)
		via1[vBufA] &= ~VIA1A_vHeadSel;
	else
		via1[vBufA] |= VIA1A_vHeadSel;
}
EXPORT_SYMBOL(via1_set_head);

int via2_scsi_drq_pending(void)
{
	return via2[gIFR] & (1 << IRQ_IDX(IRQ_MAC_SCSIDRQ));
}
EXPORT_SYMBOL(via2_scsi_drq_pending);
