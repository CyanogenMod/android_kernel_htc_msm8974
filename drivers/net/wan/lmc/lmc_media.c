
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/if_arp.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/inet.h>
#include <linux/bitops.h>

#include <asm/processor.h>             
#include <asm/io.h>
#include <asm/dma.h>

#include <asm/uaccess.h>

#include "lmc.h"
#include "lmc_var.h"
#include "lmc_ioctl.h"
#include "lmc_debug.h"

#define CONFIG_LMC_IGNORE_HARDWARE_HANDSHAKE 1

 /*
  * Copyright (c) 1997-2000 LAN Media Corporation (LMC)
  * All rights reserved.  www.lanmedia.com
  *
  * This code is written by:
  * Andrew Stanley-Jones (asj@cban.com)
  * Rob Braun (bbraun@vix.com),
  * Michael Graff (explorer@vix.com) and
  * Matt Thomas (matt@3am-software.com).
  *
  * This software may be used and distributed according to the terms
  * of the GNU General Public License version 2, incorporated herein by reference.
  */

static void lmc_set_protocol (lmc_softc_t * const, lmc_ctl_t *);

static void lmc_ds3_init (lmc_softc_t * const);
static void lmc_ds3_default (lmc_softc_t * const);
static void lmc_ds3_set_status (lmc_softc_t * const, lmc_ctl_t *);
static void lmc_ds3_set_100ft (lmc_softc_t * const, int);
static int lmc_ds3_get_link_status (lmc_softc_t * const);
static void lmc_ds3_set_crc_length (lmc_softc_t * const, int);
static void lmc_ds3_set_scram (lmc_softc_t * const, int);
static void lmc_ds3_watchdog (lmc_softc_t * const);

static void lmc_hssi_init (lmc_softc_t * const);
static void lmc_hssi_default (lmc_softc_t * const);
static void lmc_hssi_set_status (lmc_softc_t * const, lmc_ctl_t *);
static void lmc_hssi_set_clock (lmc_softc_t * const, int);
static int lmc_hssi_get_link_status (lmc_softc_t * const);
static void lmc_hssi_set_link_status (lmc_softc_t * const, int);
static void lmc_hssi_set_crc_length (lmc_softc_t * const, int);
static void lmc_hssi_watchdog (lmc_softc_t * const);

static void lmc_ssi_init (lmc_softc_t * const);
static void lmc_ssi_default (lmc_softc_t * const);
static void lmc_ssi_set_status (lmc_softc_t * const, lmc_ctl_t *);
static void lmc_ssi_set_clock (lmc_softc_t * const, int);
static void lmc_ssi_set_speed (lmc_softc_t * const, lmc_ctl_t *);
static int lmc_ssi_get_link_status (lmc_softc_t * const);
static void lmc_ssi_set_link_status (lmc_softc_t * const, int);
static void lmc_ssi_set_crc_length (lmc_softc_t * const, int);
static void lmc_ssi_watchdog (lmc_softc_t * const);

static void lmc_t1_init (lmc_softc_t * const);
static void lmc_t1_default (lmc_softc_t * const);
static void lmc_t1_set_status (lmc_softc_t * const, lmc_ctl_t *);
static int lmc_t1_get_link_status (lmc_softc_t * const);
static void lmc_t1_set_circuit_type (lmc_softc_t * const, int);
static void lmc_t1_set_crc_length (lmc_softc_t * const, int);
static void lmc_t1_set_clock (lmc_softc_t * const, int);
static void lmc_t1_watchdog (lmc_softc_t * const);

static void lmc_dummy_set_1 (lmc_softc_t * const, int);
static void lmc_dummy_set2_1 (lmc_softc_t * const, lmc_ctl_t *);

static inline void write_av9110_bit (lmc_softc_t *, int);
static void write_av9110(lmc_softc_t *, u32, u32, u32, u32, u32);

lmc_media_t lmc_ds3_media = {
  lmc_ds3_init,			
  lmc_ds3_default,		
  lmc_ds3_set_status,		
  lmc_dummy_set_1,		
  lmc_dummy_set2_1,		
  lmc_ds3_set_100ft,		
  lmc_ds3_set_scram,		
  lmc_ds3_get_link_status,	
  lmc_dummy_set_1,		
  lmc_ds3_set_crc_length,	
  lmc_dummy_set_1,		
  lmc_ds3_watchdog
};

lmc_media_t lmc_hssi_media = {
  lmc_hssi_init,		
  lmc_hssi_default,		
  lmc_hssi_set_status,		
  lmc_hssi_set_clock,		
  lmc_dummy_set2_1,		
  lmc_dummy_set_1,		
  lmc_dummy_set_1,		
  lmc_hssi_get_link_status,	
  lmc_hssi_set_link_status,	
  lmc_hssi_set_crc_length,	
  lmc_dummy_set_1,		
  lmc_hssi_watchdog
};

lmc_media_t lmc_ssi_media = { lmc_ssi_init,	
  lmc_ssi_default,		
  lmc_ssi_set_status,		
  lmc_ssi_set_clock,		
  lmc_ssi_set_speed,		
  lmc_dummy_set_1,		
  lmc_dummy_set_1,		
  lmc_ssi_get_link_status,	
  lmc_ssi_set_link_status,	
  lmc_ssi_set_crc_length,	
  lmc_dummy_set_1,		
  lmc_ssi_watchdog
};

lmc_media_t lmc_t1_media = {
  lmc_t1_init,			
  lmc_t1_default,		
  lmc_t1_set_status,		
  lmc_t1_set_clock,		
  lmc_dummy_set2_1,		
  lmc_dummy_set_1,		
  lmc_dummy_set_1,		
  lmc_t1_get_link_status,	
  lmc_dummy_set_1,		
  lmc_t1_set_crc_length,	
  lmc_t1_set_circuit_type,	
  lmc_t1_watchdog
};

static void
lmc_dummy_set_1 (lmc_softc_t * const sc, int a)
{
}

static void
lmc_dummy_set2_1 (lmc_softc_t * const sc, lmc_ctl_t * a)
{
}


static void
lmc_hssi_init (lmc_softc_t * const sc)
{
  sc->ictl.cardtype = LMC_CTL_CARDTYPE_LMC5200;

  lmc_gpio_mkoutput (sc, LMC_GEP_HSSI_CLOCK);
}

static void
lmc_hssi_default (lmc_softc_t * const sc)
{
  sc->lmc_miireg16 = LMC_MII16_LED_ALL;

  sc->lmc_media->set_link_status (sc, LMC_LINK_DOWN);
  sc->lmc_media->set_clock_source (sc, LMC_CTL_CLOCK_SOURCE_EXT);
  sc->lmc_media->set_crc_length (sc, LMC_CTL_CRC_LENGTH_16);
}

static void
lmc_hssi_set_status (lmc_softc_t * const sc, lmc_ctl_t * ctl)
{
  if (ctl == NULL)
    {
      sc->lmc_media->set_clock_source (sc, sc->ictl.clock_source);
      lmc_set_protocol (sc, NULL);

      return;
    }

  if (ctl->clock_source && !sc->ictl.clock_source)
    {
      sc->lmc_media->set_clock_source (sc, LMC_CTL_CLOCK_SOURCE_INT);
      sc->lmc_timing = LMC_CTL_CLOCK_SOURCE_INT;
    }
  else if (!ctl->clock_source && sc->ictl.clock_source)
    {
      sc->lmc_timing = LMC_CTL_CLOCK_SOURCE_EXT;
      sc->lmc_media->set_clock_source (sc, LMC_CTL_CLOCK_SOURCE_EXT);
    }

  lmc_set_protocol (sc, ctl);
}

static void
lmc_hssi_set_clock (lmc_softc_t * const sc, int ie)
{
  int old;
  old = sc->ictl.clock_source;
  if (ie == LMC_CTL_CLOCK_SOURCE_EXT)
    {
      sc->lmc_gpio |= LMC_GEP_HSSI_CLOCK;
      LMC_CSR_WRITE (sc, csr_gp, sc->lmc_gpio);
      sc->ictl.clock_source = LMC_CTL_CLOCK_SOURCE_EXT;
      if(old != ie)
        printk (LMC_PRINTF_FMT ": clock external\n", LMC_PRINTF_ARGS);
    }
  else
    {
      sc->lmc_gpio &= ~(LMC_GEP_HSSI_CLOCK);
      LMC_CSR_WRITE (sc, csr_gp, sc->lmc_gpio);
      sc->ictl.clock_source = LMC_CTL_CLOCK_SOURCE_INT;
      if(old != ie)
        printk (LMC_PRINTF_FMT ": clock internal\n", LMC_PRINTF_ARGS);
    }
}

static int
lmc_hssi_get_link_status (lmc_softc_t * const sc)
{
    return lmc_ssi_get_link_status(sc);
}

static void
lmc_hssi_set_link_status (lmc_softc_t * const sc, int state)
{
  if (state == LMC_LINK_UP)
    sc->lmc_miireg16 |= LMC_MII16_HSSI_TA;
  else
    sc->lmc_miireg16 &= ~LMC_MII16_HSSI_TA;

  lmc_mii_writereg (sc, 0, 16, sc->lmc_miireg16);
}

static void
lmc_hssi_set_crc_length (lmc_softc_t * const sc, int state)
{
  if (state == LMC_CTL_CRC_LENGTH_32)
    {
      
      sc->lmc_miireg16 |= LMC_MII16_HSSI_CRC;
      sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_32;
    }
  else
    {
      
      sc->lmc_miireg16 &= ~LMC_MII16_HSSI_CRC;
      sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_16;
    }

  lmc_mii_writereg (sc, 0, 16, sc->lmc_miireg16);
}

static void
lmc_hssi_watchdog (lmc_softc_t * const sc)
{
  
}


static void
lmc_ds3_set_100ft (lmc_softc_t * const sc, int ie)
{
  if (ie == LMC_CTL_CABLE_LENGTH_GT_100FT)
    {
      sc->lmc_miireg16 &= ~LMC_MII16_DS3_ZERO;
      sc->ictl.cable_length = LMC_CTL_CABLE_LENGTH_GT_100FT;
    }
  else if (ie == LMC_CTL_CABLE_LENGTH_LT_100FT)
    {
      sc->lmc_miireg16 |= LMC_MII16_DS3_ZERO;
      sc->ictl.cable_length = LMC_CTL_CABLE_LENGTH_LT_100FT;
    }
  lmc_mii_writereg (sc, 0, 16, sc->lmc_miireg16);
}

static void
lmc_ds3_default (lmc_softc_t * const sc)
{
  sc->lmc_miireg16 = LMC_MII16_LED_ALL;

  sc->lmc_media->set_link_status (sc, LMC_LINK_DOWN);
  sc->lmc_media->set_cable_length (sc, LMC_CTL_CABLE_LENGTH_LT_100FT);
  sc->lmc_media->set_scrambler (sc, LMC_CTL_OFF);
  sc->lmc_media->set_crc_length (sc, LMC_CTL_CRC_LENGTH_16);
}

static void
lmc_ds3_set_status (lmc_softc_t * const sc, lmc_ctl_t * ctl)
{
  if (ctl == NULL)
    {
      sc->lmc_media->set_cable_length (sc, sc->ictl.cable_length);
      sc->lmc_media->set_scrambler (sc, sc->ictl.scrambler_onoff);
      lmc_set_protocol (sc, NULL);

      return;
    }

  if (ctl->cable_length && !sc->ictl.cable_length)
    lmc_ds3_set_100ft (sc, LMC_CTL_CABLE_LENGTH_GT_100FT);
  else if (!ctl->cable_length && sc->ictl.cable_length)
    lmc_ds3_set_100ft (sc, LMC_CTL_CABLE_LENGTH_LT_100FT);

  if (ctl->scrambler_onoff && !sc->ictl.scrambler_onoff)
    lmc_ds3_set_scram (sc, LMC_CTL_ON);
  else if (!ctl->scrambler_onoff && sc->ictl.scrambler_onoff)
    lmc_ds3_set_scram (sc, LMC_CTL_OFF);

  lmc_set_protocol (sc, ctl);
}

static void
lmc_ds3_init (lmc_softc_t * const sc)
{
  int i;

  sc->ictl.cardtype = LMC_CTL_CARDTYPE_LMC5245;

  
  for (i = 0; i < 21; i++)
    {
      lmc_mii_writereg (sc, 0, 17, i);
      lmc_mii_writereg (sc, 0, 18, 0);
    }

  
  lmc_mii_writereg (sc, 0, 17, 1);
  lmc_mii_writereg (sc, 0, 18, 0x25);	

  lmc_mii_writereg (sc, 0, 17, 5);
  lmc_mii_writereg (sc, 0, 18, 0x80);	

  lmc_mii_writereg (sc, 0, 17, 14);
  lmc_mii_writereg (sc, 0, 18, 0x30);	

  
  for (i = 0; i < 21; i++)
    {
      lmc_mii_writereg (sc, 0, 17, i);
      lmc_mii_readreg (sc, 0, 18);
    }
}

static void
lmc_ds3_set_scram (lmc_softc_t * const sc, int ie)
{
  if (ie == LMC_CTL_ON)
    {
      sc->lmc_miireg16 |= LMC_MII16_DS3_SCRAM;
      sc->ictl.scrambler_onoff = LMC_CTL_ON;
    }
  else
    {
      sc->lmc_miireg16 &= ~LMC_MII16_DS3_SCRAM;
      sc->ictl.scrambler_onoff = LMC_CTL_OFF;
    }
  lmc_mii_writereg (sc, 0, 16, sc->lmc_miireg16);
}

static int
lmc_ds3_get_link_status (lmc_softc_t * const sc)
{
    u16 link_status, link_status_11;
    int ret = 1;

    lmc_mii_writereg (sc, 0, 17, 7);
    link_status = lmc_mii_readreg (sc, 0, 18);


    lmc_led_on(sc, LMC_DS3_LED2);

    if ((link_status & LMC_FRAMER_REG0_DLOS) ||
        (link_status & LMC_FRAMER_REG0_OOFS)){
        ret = 0;
        if(sc->last_led_err[3] != 1){
	    u16 r1;
            lmc_mii_writereg (sc, 0, 17, 01); 
            r1 = lmc_mii_readreg (sc, 0, 18);
            r1 &= 0xfe;
            lmc_mii_writereg(sc, 0, 18, r1);
            printk(KERN_WARNING "%s: Red Alarm - Loss of Signal or Loss of Framing\n", sc->name);
        }
        lmc_led_on(sc, LMC_DS3_LED3);	
        sc->last_led_err[3] = 1;
    }
    else {
        lmc_led_off(sc, LMC_DS3_LED3);	
        if(sc->last_led_err[3] == 1){
	    u16 r1;
            lmc_mii_writereg (sc, 0, 17, 01); 
            r1 = lmc_mii_readreg (sc, 0, 18);
            r1 |= 0x01;
            lmc_mii_writereg(sc, 0, 18, r1);
        }
        sc->last_led_err[3] = 0;
    }

    lmc_mii_writereg(sc, 0, 17, 0x10);
    link_status_11 = lmc_mii_readreg(sc, 0, 18);
    if((link_status & LMC_FRAMER_REG0_AIS) ||
       (link_status_11 & LMC_FRAMER_REG10_XBIT)) {
        ret = 0;
        if(sc->last_led_err[0] != 1){
            printk(KERN_WARNING "%s: AIS Alarm or XBit Error\n", sc->name);
            printk(KERN_WARNING "%s: Remote end has loss of signal or framing\n", sc->name);
        }
        lmc_led_on(sc, LMC_DS3_LED0);
        sc->last_led_err[0] = 1;
    }
    else {
        lmc_led_off(sc, LMC_DS3_LED0);
        sc->last_led_err[0] = 0;
    }

    lmc_mii_writereg (sc, 0, 17, 9);
    link_status = lmc_mii_readreg (sc, 0, 18);
    
    if(link_status & LMC_FRAMER_REG9_RBLUE){
        ret = 0;
        if(sc->last_led_err[1] != 1){
            printk(KERN_WARNING "%s: Blue Alarm - Receiving all 1's\n", sc->name);
        }
        lmc_led_on(sc, LMC_DS3_LED1);
        sc->last_led_err[1] = 1;
    }
    else {
        lmc_led_off(sc, LMC_DS3_LED1);
        sc->last_led_err[1] = 0;
    }

    return ret;
}

static void
lmc_ds3_set_crc_length (lmc_softc_t * const sc, int state)
{
  if (state == LMC_CTL_CRC_LENGTH_32)
    {
      
      sc->lmc_miireg16 |= LMC_MII16_DS3_CRC;
      sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_32;
    }
  else
    {
      
      sc->lmc_miireg16 &= ~LMC_MII16_DS3_CRC;
      sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_16;
    }

  lmc_mii_writereg (sc, 0, 16, sc->lmc_miireg16);
}

static void
lmc_ds3_watchdog (lmc_softc_t * const sc)
{
    
}



static void lmc_ssi_init(lmc_softc_t * const sc)
{
	u16 mii17;
	int cable;

	sc->ictl.cardtype = LMC_CTL_CARDTYPE_LMC1000;

	mii17 = lmc_mii_readreg(sc, 0, 17);

	cable = (mii17 & LMC_MII17_SSI_CABLE_MASK) >> LMC_MII17_SSI_CABLE_SHIFT;
	sc->ictl.cable_type = cable;

	lmc_gpio_mkoutput(sc, LMC_GEP_SSI_TXCLOCK);
}

static void
lmc_ssi_default (lmc_softc_t * const sc)
{
  sc->lmc_miireg16 = LMC_MII16_LED_ALL;

  lmc_gpio_mkoutput (sc, LMC_GEP_SSI_TXCLOCK);

  sc->lmc_media->set_link_status (sc, LMC_LINK_DOWN);
  sc->lmc_media->set_clock_source (sc, LMC_CTL_CLOCK_SOURCE_EXT);
  sc->lmc_media->set_speed (sc, NULL);
  sc->lmc_media->set_crc_length (sc, LMC_CTL_CRC_LENGTH_16);
}

static void
lmc_ssi_set_status (lmc_softc_t * const sc, lmc_ctl_t * ctl)
{
  if (ctl == NULL)
    {
      sc->lmc_media->set_clock_source (sc, sc->ictl.clock_source);
      sc->lmc_media->set_speed (sc, &sc->ictl);
      lmc_set_protocol (sc, NULL);

      return;
    }

  if (ctl->clock_source == LMC_CTL_CLOCK_SOURCE_INT
      && sc->ictl.clock_source == LMC_CTL_CLOCK_SOURCE_EXT)
    {
      sc->lmc_media->set_clock_source (sc, LMC_CTL_CLOCK_SOURCE_INT);
      sc->lmc_timing = LMC_CTL_CLOCK_SOURCE_INT;
    }
  else if (ctl->clock_source == LMC_CTL_CLOCK_SOURCE_EXT
	   && sc->ictl.clock_source == LMC_CTL_CLOCK_SOURCE_INT)
    {
      sc->lmc_media->set_clock_source (sc, LMC_CTL_CLOCK_SOURCE_EXT);
      sc->lmc_timing = LMC_CTL_CLOCK_SOURCE_EXT;
    }

  if (ctl->clock_rate != sc->ictl.clock_rate)
    sc->lmc_media->set_speed (sc, ctl);

  lmc_set_protocol (sc, ctl);
}

static void
lmc_ssi_set_clock (lmc_softc_t * const sc, int ie)
{
  int old;
  old = ie;
  if (ie == LMC_CTL_CLOCK_SOURCE_EXT)
    {
      sc->lmc_gpio &= ~(LMC_GEP_SSI_TXCLOCK);
      LMC_CSR_WRITE (sc, csr_gp, sc->lmc_gpio);
      sc->ictl.clock_source = LMC_CTL_CLOCK_SOURCE_EXT;
      if(ie != old)
        printk (LMC_PRINTF_FMT ": clock external\n", LMC_PRINTF_ARGS);
    }
  else
    {
      sc->lmc_gpio |= LMC_GEP_SSI_TXCLOCK;
      LMC_CSR_WRITE (sc, csr_gp, sc->lmc_gpio);
      sc->ictl.clock_source = LMC_CTL_CLOCK_SOURCE_INT;
      if(ie != old)
        printk (LMC_PRINTF_FMT ": clock internal\n", LMC_PRINTF_ARGS);
    }
}

static void
lmc_ssi_set_speed (lmc_softc_t * const sc, lmc_ctl_t * ctl)
{
  lmc_ctl_t *ictl = &sc->ictl;
  lmc_av9110_t *av;

  if (ctl == NULL)
    {
      av = &ictl->cardspec.ssi;
      ictl->clock_rate = 1500000;
      av->f = ictl->clock_rate;
      av->n = 120;
      av->m = 100;
      av->v = 1;
      av->x = 1;
      av->r = 2;

      write_av9110 (sc, av->n, av->m, av->v, av->x, av->r);
      return;
    }

  av = &ctl->cardspec.ssi;

  if (av->f == 0)
    return;

  ictl->clock_rate = av->f;	
  ictl->cardspec.ssi = *av;

  write_av9110 (sc, av->n, av->m, av->v, av->x, av->r);
}

static int
lmc_ssi_get_link_status (lmc_softc_t * const sc)
{
  u16 link_status;
  u32 ticks;
  int ret = 1;
  int hw_hdsk = 1;



  link_status = lmc_mii_readreg (sc, 0, 16);

  
  ticks = LMC_CSR_READ (sc, csr_gp_timer);
  ticks = 0x0000ffff - (ticks & 0x0000ffff);

  lmc_led_on (sc, LMC_MII16_LED0);

  
  if (sc->lmc_timing == LMC_CTL_CLOCK_SOURCE_INT) {
      lmc_led_off(sc, LMC_MII16_LED3);
  }
  else if (ticks == 0 ) {				
      ret = 0;
      if (sc->last_led_err[3] != 1) {
	      sc->extra_stats.tx_lossOfClockCnt++;
	      printk(KERN_WARNING "%s: Lost Clock, Link Down\n", sc->name);
      }
      sc->last_led_err[3] = 1;
      lmc_led_on (sc, LMC_MII16_LED3);	
  }
  else {
      if(sc->last_led_err[3] == 1)
          printk(KERN_WARNING "%s: Clock Returned\n", sc->name);
      sc->last_led_err[3] = 0;
      lmc_led_off (sc, LMC_MII16_LED3);		
  }

  if ((link_status & LMC_MII16_SSI_DSR) == 0) { 
      ret = 0;
      hw_hdsk = 0;
  }

#ifdef CONFIG_LMC_IGNORE_HARDWARE_HANDSHAKE
  if ((link_status & (LMC_MII16_SSI_CTS | LMC_MII16_SSI_DCD)) == 0){
      ret = 0;
      hw_hdsk = 0;
  }
#endif

  if(hw_hdsk == 0){
      if(sc->last_led_err[1] != 1)
          printk(KERN_WARNING "%s: DSR not asserted\n", sc->name);
      sc->last_led_err[1] = 1;
      lmc_led_off(sc, LMC_MII16_LED1);
  }
  else {
      if(sc->last_led_err[1] != 0)
          printk(KERN_WARNING "%s: DSR now asserted\n", sc->name);
      sc->last_led_err[1] = 0;
      lmc_led_on(sc, LMC_MII16_LED1);
  }

  if(ret == 1) {
      lmc_led_on(sc, LMC_MII16_LED2); 
  }
  
  return ret;
}

static void
lmc_ssi_set_link_status (lmc_softc_t * const sc, int state)
{
  if (state == LMC_LINK_UP)
    {
      sc->lmc_miireg16 |= (LMC_MII16_SSI_DTR | LMC_MII16_SSI_RTS);
      printk (LMC_PRINTF_FMT ": asserting DTR and RTS\n", LMC_PRINTF_ARGS);
    }
  else
    {
      sc->lmc_miireg16 &= ~(LMC_MII16_SSI_DTR | LMC_MII16_SSI_RTS);
      printk (LMC_PRINTF_FMT ": deasserting DTR and RTS\n", LMC_PRINTF_ARGS);
    }

  lmc_mii_writereg (sc, 0, 16, sc->lmc_miireg16);

}

static void
lmc_ssi_set_crc_length (lmc_softc_t * const sc, int state)
{
  if (state == LMC_CTL_CRC_LENGTH_32)
    {
      
      sc->lmc_miireg16 |= LMC_MII16_SSI_CRC;
      sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_32;
      sc->lmc_crcSize = LMC_CTL_CRC_BYTESIZE_4;

    }
  else
    {
      
      sc->lmc_miireg16 &= ~LMC_MII16_SSI_CRC;
      sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_16;
      sc->lmc_crcSize = LMC_CTL_CRC_BYTESIZE_2;
    }

  lmc_mii_writereg (sc, 0, 16, sc->lmc_miireg16);
}

static inline void
write_av9110_bit (lmc_softc_t * sc, int c)
{
  sc->lmc_gpio &= ~(LMC_GEP_CLK);
  if (c & 0x01)
    sc->lmc_gpio |= LMC_GEP_DATA;
  else
    sc->lmc_gpio &= ~(LMC_GEP_DATA);
  LMC_CSR_WRITE (sc, csr_gp, sc->lmc_gpio);

  sc->lmc_gpio |= LMC_GEP_CLK;
  LMC_CSR_WRITE (sc, csr_gp, sc->lmc_gpio);

  sc->lmc_gpio &= ~(LMC_GEP_CLK);
  LMC_CSR_WRITE (sc, csr_gp, sc->lmc_gpio);
}

static void write_av9110(lmc_softc_t *sc, u32 n, u32 m, u32 v, u32 x, u32 r)
{
  int i;

#if 0
  printk (LMC_PRINTF_FMT ": speed %u, %d %d %d %d %d\n",
	  LMC_PRINTF_ARGS, sc->ictl.clock_rate, n, m, v, x, r);
#endif

  sc->lmc_gpio |= LMC_GEP_SSI_GENERATOR;
  sc->lmc_gpio &= ~(LMC_GEP_DATA | LMC_GEP_CLK);
  LMC_CSR_WRITE (sc, csr_gp, sc->lmc_gpio);

  lmc_gpio_mkoutput (sc, (LMC_GEP_DATA | LMC_GEP_CLK
			  | LMC_GEP_SSI_GENERATOR));

  sc->lmc_gpio &= ~(LMC_GEP_SSI_GENERATOR);
  LMC_CSR_WRITE (sc, csr_gp, sc->lmc_gpio);

  for (i = 0; i < 7; i++)
    write_av9110_bit (sc, n >> i);
  for (i = 0; i < 7; i++)
    write_av9110_bit (sc, m >> i);
  for (i = 0; i < 1; i++)
    write_av9110_bit (sc, v >> i);
  for (i = 0; i < 2; i++)
    write_av9110_bit (sc, x >> i);
  for (i = 0; i < 2; i++)
    write_av9110_bit (sc, r >> i);
  for (i = 0; i < 5; i++)
    write_av9110_bit (sc, 0x17 >> i);

  lmc_gpio_mkinput (sc,
		    (LMC_GEP_DATA | LMC_GEP_CLK
		     | LMC_GEP_SSI_GENERATOR));
}

static void lmc_ssi_watchdog(lmc_softc_t * const sc)
{
	u16 mii17 = lmc_mii_readreg(sc, 0, 17);
	if (((mii17 >> 3) & 7) == 7)
		lmc_led_off(sc, LMC_MII16_LED2);
	else
		lmc_led_on(sc, LMC_MII16_LED2);
}


static void
lmc_t1_write (lmc_softc_t * const sc, int a, int d)
{
  lmc_mii_writereg (sc, 0, 17, a);
  lmc_mii_writereg (sc, 0, 18, d);
}



static void
lmc_t1_init (lmc_softc_t * const sc)
{
  u16 mii16;
  int i;

  sc->ictl.cardtype = LMC_CTL_CARDTYPE_LMC1200;
  mii16 = lmc_mii_readreg (sc, 0, 16);

  
  mii16 &= ~LMC_MII16_T1_RST;
  lmc_mii_writereg (sc, 0, 16, mii16 | LMC_MII16_T1_RST);
  lmc_mii_writereg (sc, 0, 16, mii16);

  
  sc->lmc_miireg16 = mii16;
  lmc_t1_set_circuit_type(sc, LMC_CTL_CIRCUIT_TYPE_T1);
  mii16 = sc->lmc_miireg16;

  lmc_t1_write (sc, 0x01, 0x1B);	
  lmc_t1_write (sc, 0x02, 0x42);	
  lmc_t1_write (sc, 0x14, 0x00);	
  lmc_t1_write (sc, 0x15, 0x00);	
  lmc_t1_write (sc, 0x18, 0xFF);	
  lmc_t1_write (sc, 0x19, 0x30);	
  lmc_t1_write (sc, 0x1A, 0x0F);	
  lmc_t1_write (sc, 0x20, 0x41);	
  lmc_t1_write (sc, 0x22, 0x76);	
  lmc_t1_write (sc, 0x40, 0x03);	
  lmc_t1_write (sc, 0x45, 0x00);	
  lmc_t1_write (sc, 0x46, 0x05);	
  lmc_t1_write (sc, 0x68, 0x40);	
  lmc_t1_write (sc, 0x70, 0x0D);	
  lmc_t1_write (sc, 0x71, 0x05);	
  lmc_t1_write (sc, 0x72, 0x0B);	
  lmc_t1_write (sc, 0x73, 0x00);	
  lmc_t1_write (sc, 0x74, 0x00);	
  lmc_t1_write (sc, 0x75, 0x00);	
  lmc_t1_write (sc, 0x76, 0x00);	
  lmc_t1_write (sc, 0x77, 0x00);	
  lmc_t1_write (sc, 0x90, 0x05);	
  lmc_t1_write (sc, 0x91, 0x05);	
  lmc_t1_write (sc, 0xA6, 0x00);	
  lmc_t1_write (sc, 0xB1, 0x00);	
  lmc_t1_write (sc, 0xD0, 0x47);	
  lmc_t1_write (sc, 0xD1, 0x70);	
  lmc_t1_write (sc, 0xD4, 0x30);	
  for (i = 0; i < 32; i++)
    {
      lmc_t1_write (sc, 0x0E0 + i, 0x00);	
      lmc_t1_write (sc, 0x100 + i, 0x00);	
      lmc_t1_write (sc, 0x180 + i, 0x00);	
    }
  for (i = 1; i < 25; i++)
    {
      lmc_t1_write (sc, 0x0E0 + i, 0x0D);	
    }

  mii16 |= LMC_MII16_T1_XOE;
  lmc_mii_writereg (sc, 0, 16, mii16);
  sc->lmc_miireg16 = mii16;
}

static void
lmc_t1_default (lmc_softc_t * const sc)
{
  sc->lmc_miireg16 = LMC_MII16_LED_ALL;
  sc->lmc_media->set_link_status (sc, LMC_LINK_DOWN);
  sc->lmc_media->set_circuit_type (sc, LMC_CTL_CIRCUIT_TYPE_T1);
  sc->lmc_media->set_crc_length (sc, LMC_CTL_CRC_LENGTH_16);
  
  sc->ictl.clock_source = LMC_CTL_CLOCK_SOURCE_INT;
}
static void
lmc_t1_set_status (lmc_softc_t * const sc, lmc_ctl_t * ctl)
{
  if (ctl == NULL)
    {
      sc->lmc_media->set_circuit_type (sc, sc->ictl.circuit_type);
      lmc_set_protocol (sc, NULL);

      return;
    }
  if (ctl->circuit_type == LMC_CTL_CIRCUIT_TYPE_T1
      && sc->ictl.circuit_type ==
      LMC_CTL_CIRCUIT_TYPE_E1) sc->lmc_media->set_circuit_type (sc,
								LMC_CTL_CIRCUIT_TYPE_E1);
  else if (ctl->circuit_type == LMC_CTL_CIRCUIT_TYPE_E1
	   && sc->ictl.circuit_type == LMC_CTL_CIRCUIT_TYPE_T1)
    sc->lmc_media->set_circuit_type (sc, LMC_CTL_CIRCUIT_TYPE_T1);
  lmc_set_protocol (sc, ctl);
}
 static int
lmc_t1_get_link_status (lmc_softc_t * const sc)
{
    u16 link_status;
    int ret = 1;

    lmc_trace(sc->lmc_device, "lmc_t1_get_link_status in");
    lmc_led_on(sc, LMC_DS3_LED2);

    lmc_mii_writereg (sc, 0, 17, T1FRAMER_ALARM1_STATUS);
    link_status = lmc_mii_readreg (sc, 0, 18);


    if (link_status & T1F_RAIS) {			
        ret = 0;
        if(sc->last_led_err[1] != 1){
            printk(KERN_WARNING "%s: Receive AIS/Blue Alarm. Far end in RED alarm\n", sc->name);
        }
        lmc_led_on(sc, LMC_DS3_LED1);
        sc->last_led_err[1] = 1;
    }
    else {
        if(sc->last_led_err[1] != 0){
            printk(KERN_WARNING "%s: End AIS/Blue Alarm\n", sc->name);
        }
        lmc_led_off (sc, LMC_DS3_LED1);
        sc->last_led_err[1] = 0;
    }


    if (link_status & T1F_RMYEL) {
        ret = 0;
        if(sc->last_led_err[0] != 1){
            printk(KERN_WARNING "%s: Receive Yellow AIS Alarm\n", sc->name);
        }
        lmc_led_on(sc, LMC_DS3_LED0);
        sc->last_led_err[0] = 1;
    }
    else {
        if(sc->last_led_err[0] != 0){
            printk(KERN_WARNING "%s: End of Yellow AIS Alarm\n", sc->name);
        }
        lmc_led_off(sc, LMC_DS3_LED0);
        sc->last_led_err[0] = 0;
    }

    if(link_status & T1F_RLOF){
        ret = 0;
        if(sc->last_led_err[3] != 1){
            printk(KERN_WARNING "%s: Local Red Alarm: Loss of Framing\n", sc->name);
        }
        lmc_led_on(sc, LMC_DS3_LED3);
        sc->last_led_err[3] = 1;

    }
    else {
        if(sc->last_led_err[3] != 0){
            printk(KERN_WARNING "%s: End Red Alarm (LOF)\n", sc->name);
        }
        if( ! (link_status & T1F_RLOS))
            lmc_led_off(sc, LMC_DS3_LED3);
        sc->last_led_err[3] = 0;
    }
    
    if(link_status & T1F_RLOS){
        ret = 0;
        if(sc->last_led_err[2] != 1){
            printk(KERN_WARNING "%s: Local Red Alarm: Loss of Signal\n", sc->name);
        }
        lmc_led_on(sc, LMC_DS3_LED3);
        sc->last_led_err[2] = 1;

    }
    else {
        if(sc->last_led_err[2] != 0){
            printk(KERN_WARNING "%s: End Red Alarm (LOS)\n", sc->name);
        }
        if( ! (link_status & T1F_RLOF))
            lmc_led_off(sc, LMC_DS3_LED3);
        sc->last_led_err[2] = 0;
    }

    sc->lmc_xinfo.t1_alarm1_status = link_status;

    lmc_mii_writereg (sc, 0, 17, T1FRAMER_ALARM2_STATUS);
    sc->lmc_xinfo.t1_alarm2_status = lmc_mii_readreg (sc, 0, 18);

    
    lmc_trace(sc->lmc_device, "lmc_t1_get_link_status out");

    return ret;
}

static void
lmc_t1_set_circuit_type (lmc_softc_t * const sc, int ie)
{
  if (ie == LMC_CTL_CIRCUIT_TYPE_T1) {
      sc->lmc_miireg16 |= LMC_MII16_T1_Z;
      sc->ictl.circuit_type = LMC_CTL_CIRCUIT_TYPE_T1;
      printk(KERN_INFO "%s: In T1 Mode\n", sc->name);
  }
  else {
      sc->lmc_miireg16 &= ~LMC_MII16_T1_Z;
      sc->ictl.circuit_type = LMC_CTL_CIRCUIT_TYPE_E1;
      printk(KERN_INFO "%s: In E1 Mode\n", sc->name);
  }

  lmc_mii_writereg (sc, 0, 16, sc->lmc_miireg16);
  
}

static void
lmc_t1_set_crc_length (lmc_softc_t * const sc, int state)
{
  if (state == LMC_CTL_CRC_LENGTH_32)
    {
      
      sc->lmc_miireg16 |= LMC_MII16_T1_CRC;
      sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_32;
      sc->lmc_crcSize = LMC_CTL_CRC_BYTESIZE_4;

    }
  else
    {
       sc->lmc_miireg16 &= ~LMC_MII16_T1_CRC;
      sc->ictl.crc_length = LMC_CTL_CRC_LENGTH_16;
      sc->lmc_crcSize = LMC_CTL_CRC_BYTESIZE_2;

    }

  lmc_mii_writereg (sc, 0, 16, sc->lmc_miireg16);
}

static void
lmc_t1_set_clock (lmc_softc_t * const sc, int ie)
{
  int old;
  old = ie;
  if (ie == LMC_CTL_CLOCK_SOURCE_EXT)
    {
      sc->lmc_gpio &= ~(LMC_GEP_SSI_TXCLOCK);
      LMC_CSR_WRITE (sc, csr_gp, sc->lmc_gpio);
      sc->ictl.clock_source = LMC_CTL_CLOCK_SOURCE_EXT;
      if(old != ie)
        printk (LMC_PRINTF_FMT ": clock external\n", LMC_PRINTF_ARGS);
    }
  else
    {
      sc->lmc_gpio |= LMC_GEP_SSI_TXCLOCK;
      LMC_CSR_WRITE (sc, csr_gp, sc->lmc_gpio);
      sc->ictl.clock_source = LMC_CTL_CLOCK_SOURCE_INT;
      if(old != ie)
        printk (LMC_PRINTF_FMT ": clock internal\n", LMC_PRINTF_ARGS);
    }
}

static void
lmc_t1_watchdog (lmc_softc_t * const sc)
{
}

static void
lmc_set_protocol (lmc_softc_t * const sc, lmc_ctl_t * ctl)
{
	if (!ctl)
		sc->ictl.keepalive_onoff = LMC_CTL_ON;
}
