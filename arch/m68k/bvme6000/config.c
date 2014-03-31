/*
 *  arch/m68k/bvme6000/config.c
 *
 *  Copyright (C) 1997 Richard Hirst [richard@sleepie.demon.co.uk]
 *
 * Based on:
 *
 *  linux/amiga/config.c
 *
 *  Copyright (C) 1993 Hamish Macdonald
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file README.legal in the main directory of this archive
 * for more details.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/linkage.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/genhd.h>
#include <linux/rtc.h>
#include <linux/interrupt.h>
#include <linux/bcd.h>

#include <asm/bootinfo.h>
#include <asm/pgtable.h>
#include <asm/setup.h>
#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/rtc.h>
#include <asm/machdep.h>
#include <asm/bvme6000hw.h>

static void bvme6000_get_model(char *model);
extern void bvme6000_sched_init(irq_handler_t handler);
extern unsigned long bvme6000_gettimeoffset (void);
extern int bvme6000_hwclk (int, struct rtc_time *);
extern int bvme6000_set_clock_mmss (unsigned long);
extern void bvme6000_reset (void);
void bvme6000_set_vectors (void);


static irq_handler_t tick_handler;


int bvme6000_parse_bootinfo(const struct bi_record *bi)
{
	if (bi->tag == BI_VME_TYPE)
		return 0;
	else
		return 1;
}

void bvme6000_reset(void)
{
	volatile PitRegsPtr pit = (PitRegsPtr)BVME_PIT_BASE;

	printk ("\r\n\nCalled bvme6000_reset\r\n"
			"\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r");
	

	pit->pcddr	|= 0x10;	

	while(1)
		;
}

static void bvme6000_get_model(char *model)
{
    sprintf(model, "BVME%d000", m68k_cputype == CPU_68060 ? 6 : 4);
}

static void __init bvme6000_init_IRQ(void)
{
	m68k_setup_user_interrupt(VEC_USER, 192);
}

void __init config_bvme6000(void)
{
    volatile PitRegsPtr pit = (PitRegsPtr)BVME_PIT_BASE;

    
    if (!vme_brdtype) {
	if (m68k_cputype == CPU_68060)
	    vme_brdtype = VME_TYPE_BVME6000;
	else
	    vme_brdtype = VME_TYPE_BVME4000;
    }
#if 0

    bvme6000_set_vectors();
#endif

    mach_max_dma_address = 0xffffffff;
    mach_sched_init      = bvme6000_sched_init;
    mach_init_IRQ        = bvme6000_init_IRQ;
    mach_gettimeoffset   = bvme6000_gettimeoffset;
    mach_hwclk           = bvme6000_hwclk;
    mach_set_clock_mmss	 = bvme6000_set_clock_mmss;
    mach_reset		 = bvme6000_reset;
    mach_get_model       = bvme6000_get_model;

    printk ("Board is %sconfigured as a System Controller\n",
		*config_reg_ptr & BVME_CONFIG_SW1 ? "" : "not ");

    

    pit->pgcr	= 0x00;	
    pit->psrr	= 0x18;	
    pit->pacr	= 0x00;	
    pit->padr	= 0x00;	
    pit->paddr	= 0x00;	
    pit->pbcr	= 0x80;	
    pit->pbdr	= 0xbc | (*config_reg_ptr & BVME_CONFIG_SW1 ? 0 : 0x40);
			
    pit->pbddr	= 0xf3;	
    pit->pcdr	= 0x01;	
    pit->pcddr	= 0x03;	

    

    bvme_acr_addrctl = 0;
}


irqreturn_t bvme6000_abort_int (int irq, void *dev_id)
{
        unsigned long *new = (unsigned long *)vectors;
        unsigned long *old = (unsigned long *)0xf8000000;

        
        while (*(volatile unsigned char *)BVME_LOCAL_IRQ_STAT & BVME_ABORT_STATUS)
                ;

        *(new+4) = *(old+4);            
        *(new+9) = *(old+9);            
        *(new+47) = *(old+47);          
        *(new+0x1f) = *(old+0x1f);      
	return IRQ_HANDLED;
}


static irqreturn_t bvme6000_timer_int (int irq, void *dev_id)
{
    volatile RtcPtr_t rtc = (RtcPtr_t)BVME_RTC_BASE;
    unsigned char msr = rtc->msr & 0xc0;

    rtc->msr = msr | 0x20;		

    return tick_handler(irq, dev_id);
}


void bvme6000_sched_init (irq_handler_t timer_routine)
{
    volatile RtcPtr_t rtc = (RtcPtr_t)BVME_RTC_BASE;
    unsigned char msr = rtc->msr & 0xc0;

    rtc->msr = 0;	

    tick_handler = timer_routine;
    if (request_irq(BVME_IRQ_RTC, bvme6000_timer_int, 0,
				"timer", bvme6000_timer_int))
	panic ("Couldn't register timer int");

    rtc->t1cr_omr = 0x04;	
    rtc->t1msb = 39999 >> 8;
    rtc->t1lsb = 39999 & 0xff;
    rtc->irr_icr1 &= 0xef;	
    rtc->msr = 0x40;		
    rtc->pfr_icr0 = 0x80;	
    rtc->irr_icr1 = 0;
    rtc->t1cr_omr = 0x0a;	
    rtc->t0cr_rtmr &= 0xdf;	
    rtc->msr = 0;		
    rtc->t1cr_omr = 0x05;	

    rtc->msr = msr;

    if (request_irq(BVME_IRQ_ABORT, bvme6000_abort_int, 0,
				"abort", bvme6000_abort_int))
	panic ("Couldn't register abort int");
}




unsigned long bvme6000_gettimeoffset (void)
{
    volatile RtcPtr_t rtc = (RtcPtr_t)BVME_RTC_BASE;
    volatile PitRegsPtr pit = (PitRegsPtr)BVME_PIT_BASE;
    unsigned char msr = rtc->msr & 0xc0;
    unsigned char t1int, t1op;
    unsigned long v = 800000, ov;

    rtc->msr = 0;	

    do {
	ov = v;
	t1int = rtc->msr & 0x20;
	t1op  = pit->pcdr & 0x04;
	rtc->t1cr_omr |= 0x40;		
	v = rtc->t1msb << 8;		
	v |= rtc->t1lsb;		
    } while (t1int != (rtc->msr & 0x20) ||
		t1op != (pit->pcdr & 0x04) ||
			abs(ov-v) > 80 ||
				v > 39960);

    v = 39999 - v;
    if (!t1op)				
	v += 40000;
    v /= 8;				
    if (t1int)
	v += 10000;			
    rtc->msr = msr;

    return v;
}


int bvme6000_hwclk(int op, struct rtc_time *t)
{
	volatile RtcPtr_t rtc = (RtcPtr_t)BVME_RTC_BASE;
	unsigned char msr = rtc->msr & 0xc0;

	rtc->msr = 0x40;	
	if (op)
	{	
		rtc->t0cr_rtmr = t->tm_year%4;
		rtc->bcd_tenms = 0;
		rtc->bcd_sec = bin2bcd(t->tm_sec);
		rtc->bcd_min = bin2bcd(t->tm_min);
		rtc->bcd_hr  = bin2bcd(t->tm_hour);
		rtc->bcd_dom = bin2bcd(t->tm_mday);
		rtc->bcd_mth = bin2bcd(t->tm_mon + 1);
		rtc->bcd_year = bin2bcd(t->tm_year%100);
		if (t->tm_wday >= 0)
			rtc->bcd_dow = bin2bcd(t->tm_wday+1);
		rtc->t0cr_rtmr = t->tm_year%4 | 0x08;
	}
	else
	{	
		do {
			t->tm_sec  = bcd2bin(rtc->bcd_sec);
			t->tm_min  = bcd2bin(rtc->bcd_min);
			t->tm_hour = bcd2bin(rtc->bcd_hr);
			t->tm_mday = bcd2bin(rtc->bcd_dom);
			t->tm_mon  = bcd2bin(rtc->bcd_mth)-1;
			t->tm_year = bcd2bin(rtc->bcd_year);
			if (t->tm_year < 70)
				t->tm_year += 100;
			t->tm_wday = bcd2bin(rtc->bcd_dow)-1;
		} while (t->tm_sec != bcd2bin(rtc->bcd_sec));
	}

	rtc->msr = msr;

	return 0;
}


int bvme6000_set_clock_mmss (unsigned long nowtime)
{
	int retval = 0;
	short real_seconds = nowtime % 60, real_minutes = (nowtime / 60) % 60;
	unsigned char rtc_minutes, rtc_tenms;
	volatile RtcPtr_t rtc = (RtcPtr_t)BVME_RTC_BASE;
	unsigned char msr = rtc->msr & 0xc0;
	unsigned long flags;
	volatile int i;

	rtc->msr = 0;		
	rtc_minutes = bcd2bin (rtc->bcd_min);

	if ((rtc_minutes < real_minutes
		? real_minutes - rtc_minutes
			: rtc_minutes - real_minutes) < 30)
	{
		local_irq_save(flags);
		rtc_tenms = rtc->bcd_tenms;
		while (rtc_tenms == rtc->bcd_tenms)
			;
		for (i = 0; i < 1000; i++)
			;
		rtc->bcd_min = bin2bcd(real_minutes);
		rtc->bcd_sec = bin2bcd(real_seconds);
		local_irq_restore(flags);
	}
	else
		retval = -1;

	rtc->msr = msr;

	return retval;
}

