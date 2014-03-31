#ifndef _INC_PMCC4_PRIVATE_H_
#define _INC_PMCC4_PRIVATE_H_

/*-----------------------------------------------------------------------------
 * pmcc4_private.h -
 *
 * Copyright (C) 2005  SBE, Inc.
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


#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>    
#include <linux/timer.h>        
#include <linux/workqueue.h>
#include <linux/hdlc.h>

#include "libsbew.h"
#include "pmcc4_defs.h"
#include "pmcc4_cpld.h"
#include "musycc.h"
#include "sbe_promformat.h"
#include "comet.h"


#define SBE_DRVR_INIT        0x0
#define SBE_DRVR_AVAILABLE   0x69734F4E
#define SBE_DRVR_DOWN        0x1


struct mdesc
{
    volatile u_int32_t status;  
    u_int32_t   data;           
    u_int32_t   next;           
    void       *mem_token;      
    struct mdesc *snext;
};



struct c4_chan_info
{
    int         gchan;          
    int         channum;        
    u_int8_t    status;
#define TX_RECOVERY_MASK   0x0f
#define TX_ONR_RECOVERY    0x01
#define TX_BUFF_RECOVERY   0x02
#define RX_RECOVERY_MASK   0xf0
#define RX_ONR_RECOVERY    0x10

    unsigned char ch_start_rx;
#define CH_START_RX_NOW    1
#define CH_START_RX_ONR    2
#define CH_START_RX_BUF    3

    unsigned char ch_start_tx;
#define CH_START_TX_1ST    1
#define CH_START_TX_ONR    2
#define CH_START_TX_BUF    3

    char        tx_full;        
    short       txd_free;       
    short       txd_required;   
    unsigned short rxd_num;     
    unsigned short txd_num;     
    int         rxix_irq_srv;

    enum
    {
        UNASSIGNED,             
        DOWN,                   
        UP                      
    }           state;

    struct c4_port_info *up;
    void       *user;

    struct work_struct ch_work;
    struct mdesc *mdt;
    struct mdesc *mdr;
    struct mdesc *txd_irq_srv;
    struct mdesc *txd_usr_add;

#if 0

    u_int8_t    ts_bitmask[32];
#endif
    spinlock_t  ch_rxlock;
    spinlock_t  ch_txlock;
    atomic_t    tx_pending;

    struct sbecom_chan_stats s;
    struct sbecom_chan_param p;
};
typedef struct c4_chan_info mch_t;

struct c4_port_info
{

    struct musycc_globalr *reg;
    struct musycc_groupr *regram;
    void       *regram_saved;   
    comet_t    *cometbase;
    struct sbe_card_info *up;


    struct workqueue_struct *wq_port;   
    struct semaphore sr_sem_busy;       
    struct semaphore sr_sem_wait;       
    u_int32_t   sr_last;
    short       openchans;
    char        portnum;
    char        group_is_set;   

    mch_t      *chan[MUSYCC_NCHANS];
    struct sbecom_port_param p;

    u_int8_t    tsm[32];        
    int         fifomap[32];
};
typedef struct c4_port_info mpi_t;


#define COMET_OFFSET(x) (0x80000+(x)*0x10000)
#define EEPROM_OFFSET   0xC0000
#define ISPLD_OFFSET    0xD0000

#define ISPLD_MCSR  0x0
#define ISPLD_MCLK  0x1
#define ISPLD_LEDS  0x2
#define ISPLD_INTR  0x3
#define ISPLD_MAX   0x3

struct sbe_card_info
{
    struct musycc_globalr *reg;
    struct musycc_groupr *regram;
    u_int32_t  *iqd_p;          
    void       *iqd_p_saved;    
    unsigned int iqp_headx, iqp_tailx;

    struct semaphore sem_wdbusy;
    struct watchdog wd;         
    atomic_t    bh_pending;     
    u_int32_t   brd_id;         
    u_int16_t   hdw_bid;      
    unsigned short wdcount;
    unsigned char max_port;
    unsigned char brdno;        
    unsigned char wd_notify;
#define WD_NOTIFY_1TX       1
#define WD_NOTIFY_BUF       2
#define WD_NOTIFY_ONR       4
    enum                        
    {
        C_INIT,                 
        C_IDLE,                 
        C_RUNNING               
    }           state;

    struct sbe_card_info *next;
    u_int32_t  *eeprombase;     
    c4cpld_t   *cpldbase;       
    char       *release;        
    void       *hdw_info;
#ifdef CONFIG_PROC_FS
    struct proc_dir_entry *dir_dev;
#endif

    
    hdlc_device *first_if;
    hdlc_device *last_if;
    short       first_channum, last_channum;

    struct intlog
    {
        u_int32_t   this_status_new;
        u_int32_t   last_status_new;
        u_int32_t   drvr_intr_thcount;
        u_int32_t   drvr_intr_bhcount;
        u_int32_t   drvr_int_failure;
    }           intlog;

    mpi_t       port[MUSYCC_NPORTS];
    char        devname[SBE_IFACETMPL_SIZE + 1];
    atomic_t    tx_pending;
    u_int32_t   alarmed[4];     

#if defined(SBE_ISR_TASKLET)
    struct tasklet_struct ci_musycc_isr_tasklet;
#elif defined(SBE_ISR_IMMEDIATE)
    struct tq_struct ci_musycc_isr_tq;
#endif
};
typedef struct sbe_card_info ci_t;

struct s_hdw_info
{
    u_int8_t    pci_busno;
    u_int8_t    pci_slot;
    u_int8_t    pci_pin[2];
    u_int8_t    revid[2];
    u_int8_t    mfg_info_sts;
#define EEPROM_OK          0x00
#define EEPROM_CRCERR      0x01
    char        promfmt;        

    char        devname[SBE_IFACETMPL_SIZE];
    struct pci_bus *bus;
    struct net_device *ndev;
    struct pci_dev *pdev[2];

    unsigned long addr[2];
    unsigned long addr_mapped[2];
    unsigned long len[2];

    union
    {
        char        data[128];
        FLD_TYPE1   pft1;       
        FLD_TYPE2   pft2;       
    }           mfg_info;
};
typedef struct s_hdw_info hdw_info_t;


struct c4_priv
{
    int         channum;
    struct sbe_card_info *ci;
};



extern ci_t *c4_list;

mch_t      *c4_find_chan (int);
int         c4_set_chan (int channum, struct sbecom_chan_param *);
int         c4_get_chan (int channum, struct sbecom_chan_param *);
int         c4_get_chan_stats (int channum, struct sbecom_chan_stats *);

#endif                          
