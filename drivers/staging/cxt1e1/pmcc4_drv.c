/*-----------------------------------------------------------------------------
 * pmcc4_drv.c -
 *
 * Copyright (C) 2007  One Stop Systems, Inc.
 * Copyright (C) 2002-2006  SBE, Inc.
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
 *
 * For further information, contact via email: support@onestopsystems.com
 * One Stop Systems, Inc.  Escondido, California  U.S.A.
 *-----------------------------------------------------------------------------
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/types.h>
#include "pmcc4_sysdep.h"
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sched.h>        
#include <linux/timer.h>        
#include <linux/hdlc.h>
#include <asm/io.h>

#include "sbecom_inline_linux.h"
#include "libsbew.h"
#include "pmcc4_private.h"
#include "pmcc4.h"
#include "pmcc4_ioctls.h"
#include "musycc.h"
#include "comet.h"
#include "sbe_bid.h"

#ifdef SBE_INCLUDE_SYMBOLS
#define STATIC
#else
#define STATIC  static
#endif


#define KERN_WARN KERN_WARNING

status_t    c4_wk_chan_init (mpi_t *, mch_t *);
void        c4_wq_port_cleanup (mpi_t *);
status_t    c4_wq_port_init (mpi_t *);

int         c4_loop_port (ci_t *, int, u_int8_t);
status_t    c4_set_port (ci_t *, int);
status_t    musycc_chan_down (ci_t *, int);

u_int32_t musycc_chan_proto (int);
status_t    musycc_dump_ring (ci_t *, unsigned int);
status_t __init musycc_init (ci_t *);
void        musycc_init_mdt (mpi_t *);
void        musycc_serv_req (mpi_t *, u_int32_t);
void        musycc_update_timeslots (mpi_t *);

extern void musycc_update_tx_thp (mch_t *);
extern int  cxt1e1_log_level;
extern int  cxt1e1_max_mru;
extern int  cxt1e1_max_mtu;
extern int  max_rxdesc_used, max_rxdesc_default;
extern int  max_txdesc_used, max_txdesc_default;

#if defined (__powerpc__)
extern void *memset (void *s, int c, size_t n);

#endif

int         drvr_state = SBE_DRVR_INIT;
ci_t       *c4_list = 0;
ci_t       *CI;                 


void
sbecom_set_loglevel (int d)
{
    if (d > LOG_DEBUG)
    {
        unsigned int channum = d - (LOG_DEBUG + 1);     

        (void) musycc_dump_ring ((ci_t *) CI, channum); 
    } else
    {
        if (cxt1e1_log_level != d)
        {
            pr_info("log level changed from %d to %d\n", cxt1e1_log_level, d);
            cxt1e1_log_level = d;          
        } else
            pr_info("log level is %d\n", cxt1e1_log_level);
    }
}


mch_t      *
c4_find_chan (int channum)
{
    ci_t       *ci;
    mch_t      *ch;
    int         portnum, gchan;

    for (ci = c4_list; ci; ci = ci->next)
        for (portnum = 0; portnum < ci->max_port; portnum++)
            for (gchan = 0; gchan < MUSYCC_NCHANS; gchan++)
            {
                if ((ch = ci->port[portnum].chan[gchan]))
                {
                    if ((ch->state != UNASSIGNED) &&
                        (ch->channum == channum))
                        return (ch);
                }
            }
    return 0;
}


ci_t       *__init
c4_new (void *hi)
{
    ci_t       *ci;

#ifdef SBE_MAP_DEBUG
    pr_warning("c4_new() entered, ci needs %u.\n",
               (unsigned int) sizeof (ci_t));
#endif

    ci = (ci_t *) OS_kmalloc (sizeof (ci_t));
    if (ci)
    {
        ci->hdw_info = hi;
        ci->state = C_INIT;         
        ci->next = c4_list;
        c4_list = ci;
        ci->brdno = ci->next ? ci->next->brdno + 1 : 0;
    } else
        pr_warning("failed CI malloc, size %u.\n",
                   (unsigned int) sizeof (ci_t));

    if (CI == 0)
        CI = ci;                    
    return ci;
}



#define sbeLinkMask       0x41  
#define sbeLinkChange     0x40
#define sbeLinkDown       0x01
#define sbeAlarmsMask     0x07  
#define sbeE1AlarmsMask   0x107 

#define COMET_LBCMD_READ  0x80  

void
checkPorts (ci_t * ci)
{
#ifndef CONFIG_SBE_PMCC4_NCOMM

    comet_t    *comet;
    volatile u_int32_t value;
    u_int32_t   copyVal, LEDval;

    u_int8_t portnum;

    LEDval = 0;
    for (portnum = 0; portnum < ci->max_port; portnum++)
    {
        copyVal = 0x12f & (ci->alarmed[portnum]);       
        comet = ci->port[portnum].cometbase;
        value = pci_read_32 ((u_int32_t *) &comet->cdrc_ists) & sbeLinkMask;    

        if (value & sbeLinkChange)  
        {
            
            if (!(((copyVal >> 3) & sbeLinkDown) ^ (value & sbeLinkDown)))
            {
                if (value & sbeLinkDown)
                    pr_warning("%s: Port %d momentarily recovered.\n",
                               ci->devname, portnum);
                else
                    pr_warning("%s: Warning: Port %d link was briefly down.\n",
                               ci->devname, portnum);
            } else if (value & sbeLinkDown)
                pr_warning("%s: Warning: Port %d link is down.\n",
                           ci->devname, portnum);
            else
            {
                pr_warning("%s: Port %d link has recovered.\n",
                           ci->devname, portnum);
                copyVal |= 0x20;    
            }
            copyVal |= 0x10;        
        }
        copyVal &= 0x137;           
        if (value & sbeLinkDown)
            copyVal |= 0x08;        
        else
        {                           
            copyVal |= 0x40;        
            
            if (IS_FRAME_ANY_E1 (ci->port[portnum].p.port_mode))
            {
                value = pci_read_32 ((u_int32_t *) &comet->e1_frmr_nat_ists);   
                if (value & 0x1f)
                {                   
                    if (value & 0x10)
                        pr_warning("%s: E1 Port %d Codeword Sa4 change detected.\n",
                                   ci->devname, portnum);
                    if (value & 0x08)
                        pr_warning("%s: E1 Port %d Codeword Sa5 change detected.\n",
                                   ci->devname, portnum);
                    if (value & 0x04)
                        pr_warning("%s: E1 Port %d Codeword Sa6 change detected.\n",
                                   ci->devname, portnum);
                    if (value & 0x02)
                        pr_warning("%s: E1 Port %d Codeword Sa7 change detected.\n",
                                   ci->devname, portnum);
                    if (value & 0x01)
                        pr_warning("%s: E1 Port %d Codeword Sa8 change detected.\n",
                                   ci->devname, portnum);
                }
                value = pci_read_32 ((u_int32_t *) &comet->e1_frmr_mists);      
                if (value & 0x3)
                {                   
                    if (value & sbeE1CRC)
                        pr_warning("%s: E1 Port %d CRC-4 error(s) detected.\n",
                                   ci->devname, portnum);
                    if (value & sbeE1errSMF)    
                        pr_warning("%s: E1 Port %d received errored SMF.\n",
                                   ci->devname, portnum);
                }
                value = pci_read_32 ((u_int32_t *) &comet->e1_frmr_masts) & 0xcc; 
                
                
                value = (value >> 2);
                if (value & 0x30)
                {
                    if (value & 0x20)
                        value |= 0x40;  
                    if (value & 0x10)
                        value |= 0x100; 
                    value &= ~0x30;
                }                   
                if (value != (copyVal & sbeE1AlarmsMask))
                {                   
                    copyVal |= 0x10;
                    if ((copyVal & sbeRedAlm) && !(value & sbeRedAlm))
                    {
                        copyVal &= ~sbeRedAlm;
                        pr_warning("%s: E1 Port %d LOF alarm ended.\n",
                                   ci->devname, portnum);
                    } else if (!(copyVal & sbeRedAlm) && (value & sbeRedAlm))
                    {
                        copyVal |= sbeRedAlm;
                        pr_warning("%s: E1 Warning: Port %d LOF alarm.\n",
                                   ci->devname, portnum);
                    } else if ((copyVal & sbeYelAlm) && !(value & sbeYelAlm))
                    {
                        copyVal &= ~sbeYelAlm;
                        pr_warning("%s: E1 Port %d RAI alarm ended.\n",
                                   ci->devname, portnum);
                    } else if (!(copyVal & sbeYelAlm) && (value & sbeYelAlm))
                    {
                        copyVal |= sbeYelAlm;
                        pr_warning("%s: E1 Warning: Port %d RAI alarm.\n",
                                   ci->devname, portnum);
                    } else if ((copyVal & sbeE1RMAI) && !(value & sbeE1RMAI))
                    {
                        copyVal &= ~sbeE1RMAI;
                        pr_warning("%s: E1 Port %d RMAI alarm ended.\n",
                                   ci->devname, portnum);
                    } else if (!(copyVal & sbeE1RMAI) && (value & sbeE1RMAI))
                    {
                        copyVal |= sbeE1RMAI;
                        pr_warning("%s: E1 Warning: Port %d RMAI alarm.\n",
                                   ci->devname, portnum);
                    } else if ((copyVal & sbeAISAlm) && !(value & sbeAISAlm))
                    {
                        copyVal &= ~sbeAISAlm;
                        pr_warning("%s: E1 Port %d AIS alarm ended.\n",
                                   ci->devname, portnum);
                    } else if (!(copyVal & sbeAISAlm) && (value & sbeAISAlm))
                    {
                        copyVal |= sbeAISAlm;
                        pr_warning("%s: E1 Warning: Port %d AIS alarm.\n",
                                   ci->devname, portnum);
                    }
                }
                
            } else
            {                       
                value = pci_read_32 ((u_int32_t *) &comet->t1_almi_ists);       
                value &= sbeAlarmsMask;
                if (value != (copyVal & sbeAlarmsMask))
                {                   
                    copyVal |= 0x10;
                    if ((copyVal & sbeRedAlm) && !(value & sbeRedAlm))
                    {
                        copyVal &= ~sbeRedAlm;
                        pr_warning("%s: Port %d red alarm ended.\n",
                                   ci->devname, portnum);
                    } else if (!(copyVal & sbeRedAlm) && (value & sbeRedAlm))
                    {
                        copyVal |= sbeRedAlm;
                        pr_warning("%s: Warning: Port %d red alarm.\n",
                                   ci->devname, portnum);
                    } else if ((copyVal & sbeYelAlm) && !(value & sbeYelAlm))
                    {
                        copyVal &= ~sbeYelAlm;
                        pr_warning("%s: Port %d yellow (RAI) alarm ended.\n",
                                   ci->devname, portnum);
                    } else if (!(copyVal & sbeYelAlm) && (value & sbeYelAlm))
                    {
                        copyVal |= sbeYelAlm;
                        pr_warning("%s: Warning: Port %d yellow (RAI) alarm.\n",
                                   ci->devname, portnum);
                    } else if ((copyVal & sbeAISAlm) && !(value & sbeAISAlm))
                    {
                        copyVal &= ~sbeAISAlm;
                        pr_warning("%s: Port %d blue (AIS) alarm ended.\n",
                                   ci->devname, portnum);
                    } else if (!(copyVal & sbeAISAlm) && (value & sbeAISAlm))
                    {
                        copyVal |= sbeAISAlm;
                        pr_warning("%s: Warning: Port %d blue (AIS) alarm.\n",
                                   ci->devname, portnum);
                    }
                }
            }                       
        }
        if (copyVal & sbeAlarmsMask)
            copyVal |= 0x80;        
        if (copyVal & 0x10)
            LEDval |= 0x100;        
        LEDval |= ((copyVal & 0xc0) >> (6 - (portnum * 2)));

        ci->alarmed[portnum] &= 0xfffff000;     
        ci->alarmed[portnum] |= (copyVal);      


        if (IS_FRAME_ANY_T1 (ci->port[portnum].p.port_mode))
        {                           
            value = pci_read_32 ((u_int32_t *) &comet->ibcd_ies);       
            value &= 0x3;           
            if (value & 0x2)
            {                       
                copyVal = c4_loop_port (ci, portnum, COMET_LBCMD_READ); 
                if (copyVal != COMET_MDIAG_LINELB)      
                    c4_loop_port (ci, portnum, COMET_MDIAG_LINELB);     
            }
            if (value & 0x1)
            {                       
                copyVal = c4_loop_port (ci, portnum, COMET_LBCMD_READ); 
                if (copyVal != COMET_MDIAG_LBOFF)       
                    c4_loop_port (ci, portnum, COMET_MDIAG_LBOFF);      
            }
        }
        if (IS_FRAME_ANY_T1ESF (ci->port[portnum].p.port_mode))
        {                           
            
            value = pci_read_32 ((u_int32_t *) &comet->t1_rboc_sts) & 0x3f;     
            if (value == 0x07)
                c4_loop_port (ci, portnum, COMET_MDIAG_LINELB); 
            if (value == 0x0a)
                c4_loop_port (ci, portnum, COMET_MDIAG_PAYLB);  
            if ((value == 0x1c) || (value == 0x19) || (value == 0x12))
                c4_loop_port (ci, portnum, COMET_MDIAG_LBOFF);  
            if (cxt1e1_log_level >= LOG_DEBUG)
                if (value != 0x3f)
                    pr_warning("%s: BOC value = %x on Port %d\n",
                               ci->devname, value, portnum);
            
        }
    }

    
    if (LEDval & 0x100)
        pci_write_32 ((u_int32_t *) &ci->cpldbase->leds, LEDval & 0xff);
#endif                              
}


STATIC void
c4_watchdog (ci_t * ci)
{
    if (drvr_state != SBE_DRVR_AVAILABLE)
    {
        if (cxt1e1_log_level >= LOG_MONITOR)
            pr_info("drvr not available (%x)\n", drvr_state);
        return;
    }
    ci->wdcount++;
    checkPorts (ci);
    ci->wd_notify = 0;
}


void
c4_cleanup (void)
{
    ci_t       *ci, *next;
    mpi_t      *pi;
    int         portnum, j;

    ci = c4_list;
    while (ci)
    {
        next = ci->next;            
        pci_write_32 ((u_int32_t *) &ci->cpldbase->leds, PMCC4_CPLD_LED_OFF);
        for (portnum = 0; portnum < ci->max_port; portnum++)
        {
            pi = &ci->port[portnum];
            c4_wq_port_cleanup (pi);
            for (j = 0; j < MUSYCC_NCHANS; j++)
            {
                if (pi->chan[j])
                    OS_kfree (pi->chan[j]);     
            }
            OS_kfree (pi->regram_saved);
        }
        OS_kfree (ci->iqd_p_saved);
        OS_kfree (ci);
        ci = next;                  
    }
}



int
c4_get_portcfg (ci_t * ci)
{
    comet_t    *comet;
    int         portnum, mask;
    u_int32_t   wdata, rdata;

    wdata = COMET_MDIAG_LBOFF;      

    mask = 0;
    for (portnum = 0; portnum < MUSYCC_NPORTS; portnum++)
    {
        comet = ci->port[portnum].cometbase;
        pci_write_32 ((u_int32_t *) &comet->mdiag, wdata);
        rdata = pci_read_32 ((u_int32_t *) &comet->mdiag) & COMET_MDIAG_LBMASK;
        if (wdata == rdata)
            mask |= 1 << portnum;
    }
    return mask;
}



status_t    __init
c4_init (ci_t * ci, u_char *func0, u_char *func1)
{
    mpi_t      *pi;
    mch_t      *ch;
    static u_int32_t count = 0;
    int         portnum, j;

    ci->state = C_INIT;
    ci->brdno = count++;
    ci->intlog.this_status_new = 0;
    atomic_set (&ci->bh_pending, 0);

    ci->reg = (struct musycc_globalr *) func0;
    ci->eeprombase = (u_int32_t *) (func1 + EEPROM_OFFSET);
    ci->cpldbase = (c4cpld_t *) ((u_int32_t *) (func1 + ISPLD_OFFSET));

    
#ifdef CONFIG_SBE_PMCC4_NCOMM
    
    pci_write_32 ((u_int32_t *) &ci->reg->glcd, GCD_MAGIC);
#else
    
    pci_write_32 ((u_int32_t *) &ci->reg->glcd, GCD_MAGIC | MUSYCC_GCD_INTB_DISABLE);
#endif

    {
        int         pmsk;

        
        for (portnum = 0; portnum < MUSYCC_NPORTS; portnum++)
        {
            pi = &ci->port[portnum];
            pi->cometbase = (comet_t *) ((u_int32_t *) (func1 + COMET_OFFSET (portnum)));
            pi->reg = (struct musycc_globalr *) ((u_char *) ci->reg + (portnum * 0x800));
            pi->portnum = portnum;
            pi->p.portnum = portnum;
            pi->openchans = 0;
#ifdef SBE_MAP_DEBUG
            pr_info("Comet-%d: addr = %p\n", portnum, pi->cometbase);
#endif
        }
        pmsk = c4_get_portcfg (ci);
        switch (pmsk)
        {
        case 0x1:
            ci->max_port = 1;
            break;
        case 0x3:
            ci->max_port = 2;
            break;
#if 0
        case 0x7:                   
            ci->max_port = 3;
            break;
#endif
        case 0xf:
            ci->max_port = 4;
            break;
        default:
            ci->max_port = 0;
            pr_warning("%s: illegal port configuration (%x)\n",
                       ci->devname, pmsk);
            return SBE_DRVR_FAIL;
        }
#ifdef SBE_MAP_DEBUG
        pr_info(">> %s: c4_get_build - pmsk %x max_port %x\n",
                ci->devname, pmsk, ci->max_port);
#endif
    }

    for (portnum = 0; portnum < ci->max_port; portnum++)
    {
        pi = &ci->port[portnum];
        pi->up = ci;
        pi->sr_last = 0xffffffff;
        pi->p.port_mode = CFG_FRAME_SF; 
        pi->p.portP = (CFG_CLK_PORT_EXTERNAL | CFG_LBO_LH0);    

        OS_sem_init (&pi->sr_sem_busy, SEM_AVAILABLE);
        OS_sem_init (&pi->sr_sem_wait, SEM_TAKEN);

        for (j = 0; j < 32; j++)
        {
            pi->fifomap[j] = -1;
            pi->tsm[j] = 0;         
        }

        
        for (j = 0; j < MUSYCC_NCHANS; j++)
        {
            ch = OS_kmalloc (sizeof (mch_t));
            if (ch)
            {
                pi->chan[j] = ch;
                ch->state = UNASSIGNED;
                ch->up = pi;
                ch->gchan = (-1);   
                ch->channum = (-1); 
                ch->p.card = ci->brdno;
                ch->p.port = portnum;
                ch->p.channum = (-1);   
                ch->p.mode_56k = 0; 
            } else
            {
                pr_warning("failed mch_t malloc, port %d channel %d size %u.\n",
                           portnum, j, (unsigned int) sizeof (mch_t));
                break;
            }
        }
    }


    {

        pci_write_32 ((u_int32_t *) &ci->cpldbase->leds,
                      PMCC4_CPLD_LED_GREEN | PMCC4_CPLD_LED_YELLOW);
        OS_uwait (750000, "leds");
        pci_write_32 ((u_int32_t *) &ci->cpldbase->leds, PMCC4_CPLD_LED_OFF);
    }

    OS_init_watchdog (&ci->wd, (void (*) (void *)) c4_watchdog, ci, WATCHDOG_TIMEOUT);
    return SBE_DRVR_SUCCESS;
}



status_t    __init
c4_init2 (ci_t * ci)
{
    status_t    ret;

    
    if ((ret = musycc_init (ci)) != SBE_DRVR_SUCCESS)
        return ret;

#if 0
    ci->p.framing_type = FRAMING_CBP;
    ci->p.h110enable = 1;
#if 0
    ci->p.hypersize = 0;
#else
    hyperdummy = 0;
#endif
    ci->p.clock = 0;                
    c4_card_set_params (ci, &ci->p);
#endif
    OS_start_watchdog (&ci->wd);
    return SBE_DRVR_SUCCESS;
}



int
c4_loop_port (ci_t * ci, int portnum, u_int8_t cmd)
{
    comet_t    *comet;
    volatile u_int32_t loopValue;

    comet = ci->port[portnum].cometbase;
    loopValue = pci_read_32 ((u_int32_t *) &comet->mdiag) & COMET_MDIAG_LBMASK;

    if (cmd & COMET_LBCMD_READ)
        return loopValue;           

    if (loopValue != cmd)
    {
        switch (cmd)
        {
        case COMET_MDIAG_LINELB:
            
            pci_write_32 ((u_int32_t *) &comet->ibcd_cfg, 0x05);
            break;
        case COMET_MDIAG_LBOFF:
            
            pci_write_32 ((u_int32_t *) &comet->ibcd_cfg, 0x00);
            break;
        }

        pci_write_32 ((u_int32_t *) &comet->mdiag, cmd);
        if (cxt1e1_log_level >= LOG_WARN)
            pr_info("%s: loopback mode changed to %2x from %2x on Port %d\n",
                    ci->devname, cmd, loopValue, portnum);
        loopValue = pci_read_32 ((u_int32_t *) &comet->mdiag) & COMET_MDIAG_LBMASK;
        if (loopValue != cmd)
        {
            if (cxt1e1_log_level >= LOG_ERROR)
                pr_info("%s: write to loop register failed, unknown state for Port %d\n",
                        ci->devname, portnum);
        }
    } else
    {
        if (cxt1e1_log_level >= LOG_WARN)
            pr_info("%s: loopback already in that mode (%2x)\n",
                    ci->devname, loopValue);
    }
    return 0;
}



status_t
c4_frame_rw (ci_t * ci, struct sbecom_port_param * pp)
{
    comet_t    *comet;
    volatile u_int32_t data;

    if (pp->portnum >= ci->max_port)
        return ENXIO;

    comet = ci->port[pp->portnum].cometbase;
    data = pci_read_32 ((u_int32_t *) comet + pp->port_mode) & 0xff;

    if (pp->portP)
    {                               
        if (pp->portStatus == data)
            pr_info("%s: Port %d already that value!  Writing again anyhow.\n",
                    ci->devname, pp->portnum);
        pp->portP = (u_int8_t) data;
        pci_write_32 ((u_int32_t *) comet + pp->port_mode,
                      pp->portStatus);
        data = pci_read_32 ((u_int32_t *) comet + pp->port_mode) & 0xff;
    }
    pp->portStatus = (u_int8_t) data;
    return 0;
}



status_t
c4_pld_rw (ci_t * ci, struct sbecom_port_param * pp)
{
    volatile u_int32_t *regaddr;
    volatile u_int32_t data;
    int         regnum = pp->port_mode;

    regaddr = (u_int32_t *) ci->cpldbase + regnum;
    data = pci_read_32 ((u_int32_t *) regaddr) & 0xff;

    if (pp->portP)
    {                               
        pp->portP = (u_int8_t) data;
        pci_write_32 ((u_int32_t *) regaddr, pp->portStatus);
        data = pci_read_32 ((u_int32_t *) regaddr) & 0xff;
    }
    pp->portStatus = (u_int8_t) data;
    return 0;
}



status_t
c4_musycc_rw (ci_t * ci, struct c4_musycc_param * mcp)
{
    mpi_t      *pi;
    volatile u_int32_t *dph;    
    u_int32_t  *dpr = 0;        
    int         offset = mcp->offset % 0x800;   
    int         portnum, ramread = 0;
    volatile u_int32_t data;

    portnum = (mcp->offset % 0x6000) / 0x800;
    if (portnum >= ci->max_port)
        return ENXIO;
    pi = &ci->port[portnum];
    if (mcp->offset >= 0x6000)
        offset += 0x6000;           
    dph = (u_int32_t *) ((u_long) pi->reg + offset);

    
    dpr = (u_int32_t *) ((u_long) pi->regram + offset);
    if (mcp->offset < 0x6000)       
    {
        if (offset >= 0x200 && offset < 0x380)
            ramread = 1;
        if (offset >= 0x10 && offset < 0x200)
            ramread = 1;
    }
    
    if (ramread)
    {
        data = *dpr;
        
    } else
    {
        data = pci_read_32 ((u_int32_t *) dph);
        
    }


    if (mcp->RWportnum & 0x80)
    {                               
        if (mcp->value == data)
            pr_info("%s: musycc grp%d already that value! writing again anyhow.\n",
                    ci->devname, (mcp->RWportnum & 0x7));
        
        if (ramread)
            *dpr = mcp->value;
        
        pci_write_32 ((u_int32_t *) dph, mcp->value);
    }
    mcp->value = data;              
    return 0;
}

status_t
c4_get_port (ci_t * ci, int portnum)
{
    if (portnum >= ci->max_port)    
        return ENXIO;

    SD_SEM_TAKE (&ci->sem_wdbusy, "_wd_");      
    checkPorts (ci);
    ci->port[portnum].p.portStatus = (u_int8_t) ci->alarmed[portnum];
    ci->alarmed[portnum] &= 0xdf;
    SD_SEM_GIVE (&ci->sem_wdbusy);  
    return 0;
}

status_t
c4_set_port (ci_t * ci, int portnum)
{
    mpi_t      *pi;
    struct sbecom_port_param *pp;
    int         e1mode;
    u_int8_t    clck;
    int         i;

    if (portnum >= ci->max_port)    
        return ENXIO;

    pi = &ci->port[portnum];
    pp = &ci->port[portnum].p;
    e1mode = IS_FRAME_ANY_E1 (pp->port_mode);
    if (cxt1e1_log_level >= LOG_MONITOR2)
    {
        pr_info("%s: c4_set_port[%d]:  entered, e1mode = %x, openchans %d.\n",
                ci->devname,
                portnum, e1mode, pi->openchans);
    }
    if (pi->openchans)
        return EBUSY;               

    {
        status_t    ret;

        if ((ret = c4_wq_port_init (pi)))       
            return (ret);
    }

    init_comet (ci, pi->cometbase, pp->port_mode, 1  , pp->portP);
    clck = pci_read_32 ((u_int32_t *) &ci->cpldbase->mclk) & PMCC4_CPLD_MCLK_MASK;
    if (e1mode)
        clck |= 1 << portnum;
    else
        clck &= 0xf ^ (1 << portnum);

    pci_write_32 ((u_int32_t *) &ci->cpldbase->mclk, clck);
    pci_write_32 ((u_int32_t *) &ci->cpldbase->mcsr, PMCC4_CPLD_MCSR_IND);
    pci_write_32 ((u_int32_t *) &pi->reg->gbp, OS_vtophys (pi->regram));

    
    
    

    pi->regram->grcd =
        __constant_cpu_to_le32 (MUSYCC_GRCD_RX_ENABLE |
                                MUSYCC_GRCD_TX_ENABLE |
                                MUSYCC_GRCD_OOFMP_DISABLE |
                                MUSYCC_GRCD_SF_ALIGN |  
                                MUSYCC_GRCD_COFAIRQ_DISABLE |
                                MUSYCC_GRCD_MC_ENABLE |
                       (MUSYCC_GRCD_POLLTH_32 << MUSYCC_GRCD_POLLTH_SHIFT));

    pi->regram->pcd =
        __constant_cpu_to_le32 ((e1mode ? 1 : 0) |
                                MUSYCC_PCD_TXSYNC_RISING |
                                MUSYCC_PCD_RXSYNC_RISING |
                                MUSYCC_PCD_RXDATA_RISING);

    
       pi->regram->mld = __constant_cpu_to_le32 (cxt1e1_max_mru | (cxt1e1_max_mru << 16));

    
    for (i = 0; i < 32; i++)
    {

        
        
        
        

        if (((i == 0) && e1mode) ||
            ((i == 16) && ((pp->port_mode == CFG_FRAME_E1CRC_CAS) || (pp->port_mode == CFG_FRAME_E1CRC_CAS_AMI)))
            || ((i > 23) && (!e1mode)))
        {
            pi->tsm[i] = 0xff;      
        } else
        {
            pi->tsm[i] = 0x00;      
        }
    }
    for (i = 0; i < MUSYCC_NCHANS; i++)
    {
        pi->regram->ttsm[i] = 0;
        pi->regram->rtsm[i] = 0;
    }
    FLUSH_MEM_WRITE ();
    musycc_serv_req (pi, SR_GROUP_INIT | SR_RX_DIRECTION);
    musycc_serv_req (pi, SR_GROUP_INIT | SR_TX_DIRECTION);

    musycc_init_mdt (pi);

    pi->group_is_set = 1;
    pi->p = *pp;
    return 0;
}


unsigned int max_int = 0;

status_t
c4_new_chan (ci_t * ci, int portnum, int channum, void *user)
{
    mpi_t      *pi;
    mch_t      *ch;
    int         gchan;

    if (c4_find_chan (channum))     
        return EEXIST;

    if (portnum >= ci->max_port)    
        return ENXIO;

    pi = &(ci->port[portnum]);
    
    for (gchan = 0; gchan < MUSYCC_NCHANS; gchan++)
    {
        ch = pi->chan[gchan];
        if (ch && ch->state == UNASSIGNED)      
            break;
    }
    if (gchan == MUSYCC_NCHANS)     
        return ENFILE;

    ch->up = pi;

    
    ch->state = DOWN;
    ch->user = user;
    ch->gchan = gchan;
    ch->channum = channum;          
    ch->p.channum = channum;
#if 1
    ch->p.card = ci->brdno;
    ch->p.port = portnum;
#endif
    ch->p.chan_mode = CFG_CH_PROTO_HDLC_FCS16;
    ch->p.idlecode = CFG_CH_FLAG_7E;
    ch->p.pad_fill_count = 2;
    spin_lock_init (&ch->ch_rxlock);
    spin_lock_init (&ch->ch_txlock);

    {
        status_t    ret;

        if ((ret = c4_wk_chan_init (pi, ch)))
            return ret;
    }

    
    if (ci->first_if == 0)          
    {
        ci->first_if = ci->last_if = user;
        ci->first_channum = ci->last_channum = channum;
    } else
    {
        ci->last_if = user;
        if (ci->last_channum < channum) 
            ci->last_channum = channum;
    }
    return 0;
}

status_t
c4_del_chan (int channum)
{
    mch_t      *ch;

    if (!(ch = c4_find_chan (channum)))
        return ENOENT;
    if (ch->state == UP)
        musycc_chan_down ((ci_t *) 0, channum);
    ch->state = UNASSIGNED;
    ch->gchan = (-1);
    ch->channum = (-1);
    ch->p.channum = (-1);
    return 0;
}

status_t
c4_del_chan_stats (int channum)
{
    mch_t      *ch;

    if (!(ch = c4_find_chan (channum)))
        return ENOENT;

    memset (&ch->s, 0, sizeof (struct sbecom_chan_stats));
    return 0;
}


status_t
c4_set_chan (int channum, struct sbecom_chan_param * p)
{
    mch_t      *ch;
    int         i, x = 0;

    if (!(ch = c4_find_chan (channum)))
        return ENOENT;

#if 1
    if (ch->p.card != p->card ||
        ch->p.port != p->port ||
        ch->p.channum != p->channum)
        return EINVAL;
#endif

    if (!(ch->up->group_is_set))
    {
        return EIO;                 
    }

    if (ch->p.status != p->status || ch->p.chan_mode != p->chan_mode ||
        ch->p.data_inv != p->data_inv || ch->p.intr_mask != p->intr_mask ||
        ch->txd_free < ch->txd_num) 
        x = 1;                      
    for (i = 0; i < 32; i++)        
        if (ch->p.bitmask[i] != p->bitmask[i])
            x = 1;                  
    ch->p = *p;
    if (x && (ch->state == UP))     
    {
        status_t    ret;

        if ((ret = musycc_chan_down ((ci_t *) 0, channum)))
            return ret;
        if ((ret = c4_chan_up (ch->up->up, channum)))
            return ret;
        sd_enable_xmit (ch->user);  
    }
    return 0;
}


status_t
c4_get_chan (int channum, struct sbecom_chan_param * p)
{
    mch_t      *ch;

    if (!(ch = c4_find_chan (channum)))
        return ENOENT;
    *p = ch->p;
    return 0;
}

status_t
c4_get_chan_stats (int channum, struct sbecom_chan_stats * p)
{
    mch_t      *ch;

    if (!(ch = c4_find_chan (channum)))
        return ENOENT;
    *p = ch->s;
    p->tx_pending = atomic_read (&ch->tx_pending);
    return 0;
}

STATIC int
c4_fifo_alloc (mpi_t * pi, int chan, int *len)
{
    int         i, l = 0, start = 0, max = 0, maxstart = 0;

    for (i = 0; i < 32; i++)
    {
        if (pi->fifomap[i] != -1)
        {
            l = 0;
            start = i + 1;
            continue;
        }
        ++l;
        if (l > max)
        {
            max = l;
            maxstart = start;
        }
        if (max == *len)
            break;
    }
    if (max != *len)
    {
        if (cxt1e1_log_level >= LOG_WARN)
            pr_info("%s: wanted to allocate %d fifo space, but got only %d\n",
                    pi->up->devname, *len, max);
        *len = max;
    }
    if (cxt1e1_log_level >= LOG_DEBUG)
        pr_info("%s: allocated %d fifo at %d for channel %d/%d\n",
                pi->up->devname, max, start, chan, pi->p.portnum);
    for (i = maxstart; i < (maxstart + max); i++)
        pi->fifomap[i] = chan;
    return start;
}

void
c4_fifo_free (mpi_t * pi, int chan)
{
    int         i;

    if (cxt1e1_log_level >= LOG_DEBUG)
        pr_info("%s: deallocated fifo for channel %d/%d\n",
                pi->up->devname, chan, pi->p.portnum);
    for (i = 0; i < 32; i++)
        if (pi->fifomap[i] == chan)
            pi->fifomap[i] = -1;
}


status_t
c4_chan_up (ci_t * ci, int channum)
{
    mpi_t      *pi;
    mch_t      *ch;
    struct mbuf *m;
    struct mdesc *md;
    int         nts, nbuf, txnum, rxnum;
    int         addr, i, j, gchan;
    u_int32_t   tmp;            

    if (!(ch = c4_find_chan (channum)))
        return ENOENT;
    if (ch->state == UP)
    {
        if (cxt1e1_log_level >= LOG_MONITOR)
            pr_info("%s: channel already UP, graceful early exit\n",
                    ci->devname);
        return 0;
    }
    pi = ch->up;
    gchan = ch->gchan;
    
    nts = 0;
    for (i = 0; i < 32; i++)
    {
        if (ch->p.bitmask[i] & pi->tsm[i])
        {
            if (1 || cxt1e1_log_level >= LOG_WARN)
            {
                pr_info("%s: c4_chan_up[%d] EINVAL (attempt to cfg in-use or unavailable TimeSlot[%d])\n",
                        ci->devname, channum, i);
                pr_info("+ ask4 %x, currently %x\n",
                        ch->p.bitmask[i], pi->tsm[i]);
            }
            return EINVAL;
        }
        for (j = 0; j < 8; j++)
            if (ch->p.bitmask[i] & (1 << j))
                nts++;
    }

    nbuf = nts / 8 ? nts / 8 : 1;
    if (!nbuf)
    {
        
        pr_info("%s: c4_chan_up[%d] ENOBUFS (no TimeSlots assigned)\n",
                ci->devname, channum);
        return ENOBUFS;             
    }
    addr = c4_fifo_alloc (pi, gchan, &nbuf);
    ch->state = UP;

    
    musycc_update_timeslots (pi);

    
    ch->s.tx_pending = 0;

    
    {
        u_int32_t   ccd;

        ccd = musycc_chan_proto (ch->p.chan_mode) << MUSYCC_CCD_PROTO_SHIFT;
        if ((ch->p.chan_mode == CFG_CH_PROTO_ISLP_MODE) ||
            (ch->p.chan_mode == CFG_CH_PROTO_TRANS))
        {
            ccd |= MUSYCC_CCD_FCS_XFER; 
        }
        ccd |= 2 << MUSYCC_CCD_MAX_LENGTH;      
        ccd |= ch->p.intr_mask;
        ccd |= addr << MUSYCC_CCD_BUFFER_LOC;
        if (ch->p.chan_mode == CFG_CH_PROTO_TRANS)
            ccd |= (nbuf) << MUSYCC_CCD_BUFFER_LENGTH;
        else
            ccd |= (nbuf - 1) << MUSYCC_CCD_BUFFER_LENGTH;

        if (ch->p.data_inv & CFG_CH_DINV_TX)
            ccd |= MUSYCC_CCD_INVERT_DATA;      
        pi->regram->tcct[gchan] = cpu_to_le32 (ccd);

        if (ch->p.data_inv & CFG_CH_DINV_RX)
            ccd |= MUSYCC_CCD_INVERT_DATA;      
        else
            ccd &= ~MUSYCC_CCD_INVERT_DATA;     
        pi->regram->rcct[gchan] = cpu_to_le32 (ccd);
        FLUSH_MEM_WRITE ();
    }

    
    musycc_serv_req (pi, SR_CHANNEL_CONFIG | SR_RX_DIRECTION | gchan);
    musycc_serv_req (pi, SR_CHANNEL_CONFIG | SR_TX_DIRECTION | gchan);

    if (ch->p.chan_mode == CFG_CH_PROTO_TRANS)
    {
        if (max_rxdesc_used == max_rxdesc_default)      
            max_rxdesc_used = MUSYCC_RXDESC_TRANS;
        if (max_txdesc_used == max_txdesc_default)      
            max_txdesc_used = MUSYCC_TXDESC_TRANS;
    }
    rxnum = max_rxdesc_used + (nts / 4);
    txnum = max_txdesc_used + (nts / 4);

#if 0
    
    if (cxt1e1_log_level >= LOG_MONITOR)
        pr_info("%s: mode %x rxnum %d (rxused %d def %d) txnum %d (txused %d def %d)\n",
                ci->devname, ch->p.chan_mode,
                rxnum, max_rxdesc_used, max_rxdesc_default,
                txnum, max_txdesc_used, max_txdesc_default);
#endif

    ch->rxd_num = rxnum;
    ch->txd_num = txnum;
    ch->rxix_irq_srv = 0;

    ch->mdr = OS_kmalloc (sizeof (struct mdesc) * rxnum);
    ch->mdt = OS_kmalloc (sizeof (struct mdesc) * txnum);
    if (ch->p.chan_mode == CFG_CH_PROTO_TRANS)
               tmp = __constant_cpu_to_le32 (cxt1e1_max_mru | EOBIRQ_ENABLE);
    else
               tmp = __constant_cpu_to_le32 (cxt1e1_max_mru);

    for (i = 0, md = ch->mdr; i < rxnum; i++, md++)
    {
        if (i == (rxnum - 1))
        {
            md->snext = &ch->mdr[0];
        } else
        {
            md->snext = &ch->mdr[i + 1];
        }
        md->next = cpu_to_le32 (OS_vtophys (md->snext));

               if (!(m = OS_mem_token_alloc (cxt1e1_max_mru)))
        {
            if (cxt1e1_log_level >= LOG_MONITOR)
                pr_info("%s: c4_chan_up[%d] - token alloc failure, size = %d.\n",
                                               ci->devname, channum, cxt1e1_max_mru);
            goto errfree;
        }
        md->mem_token = m;
        md->data = cpu_to_le32 (OS_vtophys (OS_mem_token_data (m)));
        md->status = tmp | MUSYCC_RX_OWNED;     
    }

    for (i = 0, md = ch->mdt; i < txnum; i++, md++)
    {
        md->status = HOST_TX_OWNED; 
        md->mem_token = 0;
        md->data = 0;
        if (i == (txnum - 1))
        {
            md->snext = &ch->mdt[0];
        } else
        {
            md->snext = &ch->mdt[i + 1];
        }
        md->next = cpu_to_le32 (OS_vtophys (md->snext));
    }
    ch->txd_irq_srv = ch->txd_usr_add = &ch->mdt[0];
    ch->txd_free = txnum;
    ch->tx_full = 0;
    ch->txd_required = 0;

    
    tmp = cpu_to_le32 (OS_vtophys (&ch->mdt[0]));
    pi->regram->thp[gchan] = tmp;
    pi->regram->tmp[gchan] = tmp;

    tmp = cpu_to_le32 (OS_vtophys (&ch->mdr[0]));
    pi->regram->rhp[gchan] = tmp;
    pi->regram->rmp[gchan] = tmp;

    
    FLUSH_MEM_WRITE ();
    if (ch->p.status & RX_ENABLED)
    {
#ifdef RLD_TRANS_DEBUG
        pr_info("++ c4_chan_up() CHAN RX ACTIVATE: chan %d\n", ch->channum);
#endif
        ch->ch_start_rx = 0;        
        musycc_serv_req (pi, SR_CHANNEL_ACTIVATE | SR_RX_DIRECTION | gchan);
    }
    if (ch->p.status & TX_ENABLED)
    {
#ifdef RLD_TRANS_DEBUG
        pr_info("++ c4_chan_up() CHAN TX ACTIVATE: chan %d <delayed>\n", ch->channum);
#endif
        ch->ch_start_tx = CH_START_TX_1ST;      
    }
    ch->status = ch->p.status;
    pi->openchans++;
    return 0;

errfree:
    while (i > 0)
    {
        
        i--;
        OS_mem_token_free (ch->mdr[i].mem_token);
    }
    OS_kfree (ch->mdt);
    ch->mdt = 0;
    ch->txd_num = 0;
    OS_kfree (ch->mdr);
    ch->mdr = 0;
    ch->rxd_num = 0;
    ch->state = DOWN;
    return ENOBUFS;
}


void
c4_stopwd (ci_t * ci)
{
    OS_stop_watchdog (&ci->wd);
    SD_SEM_TAKE (&ci->sem_wdbusy, "_stop_");    
    SD_SEM_GIVE (&ci->sem_wdbusy);
}


void
sbecom_get_brdinfo (ci_t * ci, struct sbe_brd_info * bip, u_int8_t *bsn)
{
    char       *np;
    u_int32_t   sn = 0;
    int         i;

    bip->brdno = ci->brdno;         
    bip->brd_id = ci->brd_id;
    bip->brd_hdw_id = ci->hdw_bid;
    bip->brd_chan_cnt = MUSYCC_NCHANS * ci->max_port;   
    bip->brd_port_cnt = ci->max_port;   
    bip->brd_pci_speed = BINFO_PCI_SPEED_unk;   

    if (ci->first_if)
    {
        {
            struct net_device *dev;

            dev = (struct net_device *) ci->first_if;
            np = (char *) dev->name;
        }
        strncpy (bip->first_iname, np, CHNM_STRLEN - 1);
    } else
        strcpy (bip->first_iname, "<NULL>");
    if (ci->last_if)
    {
        {
            struct net_device *dev;

            dev = (struct net_device *) ci->last_if;
            np = (char *) dev->name;
        }
        strncpy (bip->last_iname, np, CHNM_STRLEN - 1);
    } else
        strcpy (bip->last_iname, "<NULL>");

    if (bsn)
    {
        for (i = 0; i < 3; i++)
        {
            bip->brd_mac_addr[i] = *bsn++;
        }
        for (; i < 6; i++)
        {
            bip->brd_mac_addr[i] = *bsn;
            sn = (sn << 8) | *bsn++;
        }
    } else
    {
        for (i = 0; i < 6; i++)
            bip->brd_mac_addr[i] = 0;
    }
    bip->brd_sn = sn;
}


status_t
c4_get_iidinfo (ci_t * ci, struct sbe_iid_info * iip)
{
    struct net_device *dev;
    char       *np;

    if (!(dev = getuserbychan (iip->channum)))
        return ENOENT;

    np = dev->name;
    strncpy (iip->iname, np, CHNM_STRLEN - 1);
    return 0;
}


#ifdef CONFIG_SBE_PMCC4_NCOMM
void        (*nciInterrupt[MAX_BOARDS][4]) (void);
extern void wanpmcC4T1E1_hookInterrupt (int cardID, int deviceID, void *handler);

void
wanpmcC4T1E1_hookInterrupt (int cardID, int deviceID, void *handler)
{
    if (cardID < MAX_BOARDS)    
        nciInterrupt[cardID][deviceID] = handler;
}

irqreturn_t
c4_ebus_intr_th_handler (void *devp)
{
    ci_t       *ci = (ci_t *) devp;
    volatile u_int32_t ists;
    int         handled = 0;
    int         brdno;

    
    brdno = ci->brdno;
    ists = pci_read_32 ((u_int32_t *) &ci->cpldbase->intr);
    if (ists & PMCC4_CPLD_INTR_CMT_1)
    {
        handled = 0x1;
        if (nciInterrupt[brdno][0] != NULL)
            (*nciInterrupt[brdno][0]) ();
    }
    if (ists & PMCC4_CPLD_INTR_CMT_2)
    {
        handled |= 0x2;
        if (nciInterrupt[brdno][1] != NULL)
            (*nciInterrupt[brdno][1]) ();
    }
    if (ists & PMCC4_CPLD_INTR_CMT_3)
    {
        handled |= 0x4;
        if (nciInterrupt[brdno][2] != NULL)
            (*nciInterrupt[brdno][2]) ();
    }
    if (ists & PMCC4_CPLD_INTR_CMT_4)
    {
        handled |= 0x8;
        if (nciInterrupt[brdno][3] != NULL)
            (*nciInterrupt[brdno][3]) ();
    }
#if 0
    pci_write_32 ((u_int32_t *) &ci->reg->glcd, GCD_MAGIC | MUSYCC_GCD_INTB_DISABLE);
#endif

    return IRQ_RETVAL (handled);
}


unsigned long
wanpmcC4T1E1_getBaseAddress (int cardID, int deviceID)
{
    ci_t       *ci;
    unsigned long base = 0;

    ci = c4_list;
    while (ci)
    {
        if (ci->brdno == cardID)    
        {
            if (deviceID < ci->max_port)        
                base = ((unsigned long) ci->port[deviceID].cometbase);
            break;
        }
        ci = ci->next;              
    }
    return (base);
}

#endif                          


