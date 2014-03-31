/* Copyright (C) 2003-2005  SBE, Inc.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/slab.h>
#include <asm/io.h>
#include <asm/byteorder.h>
#include <linux/netdevice.h>
#include <linux/delay.h>
#include <linux/hdlc.h>
#include "pmcc4_sysdep.h"
#include "sbecom_inline_linux.h"
#include "libsbew.h"
#include "pmcc4.h"


#ifdef SBE_INCLUDE_SYMBOLS
#define STATIC
#else
#define STATIC  static
#endif

#if defined(CONFIG_SBE_HDLC_V7) || defined(CONFIG_SBE_WAN256T3_HDLC_V7) || \
    defined(CONFIG_SBE_HDLC_V7_MODULE) || defined(CONFIG_SBE_WAN256T3_HDLC_V7_MODULE)
#define _v7_hdlc_  1
#else
#define _v7_hdlc_  0
#endif

#if _v7_hdlc_
#define V7(x) (x ## _v7)
extern int  hdlc_netif_rx_v7 (hdlc_device *, struct sk_buff *);
extern int  register_hdlc_device_v7 (hdlc_device *);
extern int  unregister_hdlc_device_v7 (hdlc_device *);

#else
#define V7(x) x
#endif


#ifndef USE_MAX_INT_DELAY
static int  dummy = 0;

#endif

extern int  cxt1e1_log_level;
extern int  drvr_state;


#if 1
u_int32_t
pci_read_32 (u_int32_t *p)
{
#ifdef FLOW_DEBUG
    u_int32_t   v;

    FLUSH_PCI_READ ();
    v = le32_to_cpu (*p);
    if (cxt1e1_log_level >= LOG_DEBUG)
        pr_info("pci_read : %x = %x\n", (u_int32_t) p, v);
    return v;
#else
    FLUSH_PCI_READ ();              
    return le32_to_cpu (*p);
#endif
}

void
pci_write_32 (u_int32_t *p, u_int32_t v)
{
#ifdef FLOW_DEBUG
    if (cxt1e1_log_level >= LOG_DEBUG)
        pr_info("pci_write: %x = %x\n", (u_int32_t) p, v);
#endif
    *p = cpu_to_le32 (v);
    FLUSH_PCI_WRITE ();             
}
#endif


void
pci_flush_write (ci_t * ci)
{
    volatile u_int32_t v;

    
    v = *(u_int32_t *) &ci->reg->glcd;  

    /*
     * return nothing, this just reads PCI bridge interface to flush
     * previously written data
     */
}


STATIC void
watchdog_func (unsigned long arg)
{
    struct watchdog *wd = (void *) arg;

    if (drvr_state != SBE_DRVR_AVAILABLE)
    {
        if (cxt1e1_log_level >= LOG_MONITOR)
            pr_warning("%s: drvr not available (%x)\n", __func__, drvr_state);
        return;
    }
    schedule_work (&wd->work);
    mod_timer (&wd->h, jiffies + wd->ticks);
}

int OS_init_watchdog(struct watchdog *wdp, void (*f) (void *), void *c, int usec)
{
    wdp->func = f;
    wdp->softc = c;
    wdp->ticks = (HZ) * (usec / 1000) / 1000;
    INIT_WORK(&wdp->work, (void *)f);
    init_timer (&wdp->h);
    {
        ci_t       *ci = (ci_t *) c;

        wdp->h.data = (unsigned long) &ci->wd;
    }
    wdp->h.function = watchdog_func;
    return 0;
}

void
OS_uwait (int usec, char *description)
{
    int         tmp;

    if (usec >= 1000)
    {
        mdelay (usec / 1000);
        
        tmp = (usec / 1000) * 1000; 
        tmp = usec - tmp;           
        if (tmp)
        {                           
            udelay (tmp);
        }
    } else
    {
        udelay (usec);
    }
}


void
OS_uwait_dummy (void)
{
#ifndef USE_MAX_INT_DELAY
    dummy++;
#else
    udelay (1);
#endif
}


void
OS_sem_init (void *sem, int state)
{
    switch (state)
    {
        case SEM_TAKEN:
		sema_init((struct semaphore *) sem, 0);
        break;
    case SEM_AVAILABLE:
	    sema_init((struct semaphore *) sem, 1);
        break;
    default:                        
        sema_init (sem, state);
        break;
    }
}


int
sd_line_is_ok (void *user)
{
    struct net_device *ndev = (struct net_device *) user;

    return (netif_carrier_ok (ndev));
}

void
sd_line_is_up (void *user)
{
    struct net_device *ndev = (struct net_device *) user;

    netif_carrier_on (ndev);
    return;
}

void
sd_line_is_down (void *user)
{
    struct net_device *ndev = (struct net_device *) user;

    netif_carrier_off (ndev);
    return;
}

void
sd_disable_xmit (void *user)
{
    struct net_device *dev = (struct net_device *) user;

    netif_stop_queue (dev);
    return;
}

void
sd_enable_xmit (void *user)
{
    struct net_device *dev = (struct net_device *) user;

    netif_wake_queue (dev);
    return;
}

int
sd_queue_stopped (void *user)
{
    struct net_device *ndev = (struct net_device *) user;

    return (netif_queue_stopped (ndev));
}

void sd_recv_consume(void *token, size_t len, void *user)
{
    struct net_device *ndev = user;
    struct sk_buff *skb = token;

    skb->dev = ndev;
    skb_put (skb, len);
    skb->protocol = hdlc_type_trans(skb, ndev);
    netif_rx(skb);
}



#include "comet.h"

extern ci_t *CI;                
void
VMETRO_TRACE (void *x)
{
    u_int32_t   y = (u_int32_t) x;

    pci_write_32 ((u_int32_t *) &CI->cpldbase->leds, y);
}


void
VMETRO_TRIGGER (ci_t * ci, int x)
{
    comet_t    *comet;
    volatile u_int32_t data;

    comet = ci->port[0].cometbase;  

    switch (x)
    {
    default:
    case 0:
        data = pci_read_32 ((u_int32_t *) &comet->__res24);     
        break;
    case 1:
        data = pci_read_32 ((u_int32_t *) &comet->__res25);     
        break;
    case 2:
        data = pci_read_32 ((u_int32_t *) &comet->__res26);     
        break;
    case 3:
        data = pci_read_32 ((u_int32_t *) &comet->__res27);     
        break;
    case 4:
        data = pci_read_32 ((u_int32_t *) &comet->__res88);     
        break;
    case 5:
        data = pci_read_32 ((u_int32_t *) &comet->__res89);     
        break;
    case 6:
        data = pci_read_32 ((u_int32_t *) &comet->__res8A);     
        break;
    case 7:
        data = pci_read_32 ((u_int32_t *) &comet->__res8B);     
        break;
    case 8:
        data = pci_read_32 ((u_int32_t *) &comet->__resA0);     
        break;
    case 9:
        data = pci_read_32 ((u_int32_t *) &comet->__resA1);     
        break;
    case 10:
        data = pci_read_32 ((u_int32_t *) &comet->__resA2);     
        break;
    case 11:
        data = pci_read_32 ((u_int32_t *) &comet->__resA3);     
        break;
    case 12:
        data = pci_read_32 ((u_int32_t *) &comet->__resA4);     
        break;
    case 13:
        data = pci_read_32 ((u_int32_t *) &comet->__resA5);     
        break;
    case 14:
        data = pci_read_32 ((u_int32_t *) &comet->__resA6);     
        break;
    case 15:
        data = pci_read_32 ((u_int32_t *) &comet->__resA7);     
        break;
    case 16:
        data = pci_read_32 ((u_int32_t *) &comet->__res74);     
        break;
    case 17:
        data = pci_read_32 ((u_int32_t *) &comet->__res75);     
        break;
    case 18:
        data = pci_read_32 ((u_int32_t *) &comet->__res76);     
        break;
    case 19:
        data = pci_read_32 ((u_int32_t *) &comet->__res77);     
        break;
    }
}


