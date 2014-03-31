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

#include <asm/io.h>
#include <linux/hdlc.h>
#include "pmcc4_sysdep.h"
#include "sbecom_inline_linux.h"
#include "libsbew.h"
#include "pmcc4.h"
#include "comet.h"
#include "comet_tables.h"

#ifdef SBE_INCLUDE_SYMBOLS
#define STATIC
#else
#define STATIC  static
#endif


extern int  cxt1e1_log_level;

#define COMET_NUM_SAMPLES   24  
#define COMET_NUM_UNITS     5   

STATIC void SetPwrLevel (comet_t * comet);
STATIC void WrtRcvEqualizerTbl (ci_t * ci, comet_t * comet, u_int32_t *table);
STATIC void WrtXmtWaveformTbl (ci_t * ci, comet_t * comet, u_int8_t table[COMET_NUM_SAMPLES][COMET_NUM_UNITS]);


void       *TWV_table[12] = {
    TWVLongHaul0DB, TWVLongHaul7_5DB, TWVLongHaul15DB, TWVLongHaul22_5DB,
    TWVShortHaul0, TWVShortHaul1, TWVShortHaul2, TWVShortHaul3, TWVShortHaul4,
    TWVShortHaul5,
    TWV_E1_75Ohm,    
    TWV_E1_120Ohm
};


static int
lbo_tbl_lkup (int t1, int lbo)
{
    if ((lbo < CFG_LBO_LH0) || (lbo > CFG_LBO_E120))    
    {
        if (t1)
            lbo = CFG_LBO_LH0;  
        else
            lbo = CFG_LBO_E120;     
    }
    return (lbo - 1);               
}


void
init_comet (void *ci, comet_t * comet, u_int32_t port_mode, int clockmaster,
            u_int8_t moreParams)
{
    u_int8_t isT1mode;
    u_int8_t    tix = CFG_LBO_LH0;      

    isT1mode = IS_FRAME_ANY_T1 (port_mode);
    
    if (isT1mode)
    {
        pci_write_32 ((u_int32_t *) &comet->gbl_cfg, 0xa0);     
        tix = lbo_tbl_lkup (isT1mode, CFG_LBO_LH0);     
    } else
    {
        pci_write_32 ((u_int32_t *) &comet->gbl_cfg, 0x81);     
        tix = lbo_tbl_lkup (isT1mode, CFG_LBO_E120);    
    }

    if (moreParams & CFG_LBO_MASK)
        tix = lbo_tbl_lkup (isT1mode, moreParams & CFG_LBO_MASK);       

    
    pci_write_32 ((u_int32_t *) &comet->tx_line_cfg, 0x00);     

    
    pci_write_32 ((u_int32_t *) &comet->mtest, 0x00);   

    
    pci_write_32 ((u_int32_t *) &comet->rjat_cfg, 0x10);        
    
    if (isT1mode)
    {
        pci_write_32 ((u_int32_t *) &comet->rjat_n1clk, 0x2F);  
        pci_write_32 ((u_int32_t *) &comet->rjat_n2clk, 0x2F);  
    } else
    {
        pci_write_32 ((u_int32_t *) &comet->rjat_n1clk, 0xFF);  
        pci_write_32 ((u_int32_t *) &comet->rjat_n2clk, 0xFF);  
    }

    
    pci_write_32 ((u_int32_t *) &comet->tjat_cfg, 0x10);        

    
    pci_write_32 ((u_int32_t *) &comet->rx_opt, 0x00);  

    
    
    if (isT1mode)
    {
        pci_write_32 ((u_int32_t *) &comet->tjat_n1clk, 0x2F);  
        pci_write_32 ((u_int32_t *) &comet->tjat_n2clk, 0x2F);  
    } else
    {
        pci_write_32 ((u_int32_t *) &comet->tjat_n1clk, 0xFF);  
        pci_write_32 ((u_int32_t *) &comet->tjat_n2clk, 0xFF);  
    }

    
    if (isT1mode)
    {                               
        pci_write_32 ((u_int32_t *) &comet->rx_elst_cfg, 0x00);
        pci_write_32 ((u_int32_t *) &comet->tx_elst_cfg, 0x00);
    } else
    {                               
        pci_write_32 ((u_int32_t *) &comet->rx_elst_cfg, 0x03);
        pci_write_32 ((u_int32_t *) &comet->tx_elst_cfg, 0x03);
        pci_write_32 ((u_int32_t *) &comet->rxce1_ctl, 0x00);   
        pci_write_32 ((u_int32_t *) &comet->txci1_ctl, 0x00);   
    }

    
    
    pci_write_32 ((u_int32_t *) &comet->t1_rboc_ena, 0x00);     
    if (isT1mode)
    {

        
        pci_write_32 ((u_int32_t *) &comet->ibcd_cfg, 0x04);    
        pci_write_32 ((u_int32_t *) &comet->ibcd_act, 0x08);    
        pci_write_32 ((u_int32_t *) &comet->ibcd_deact, 0x24);  
    }
    
    
    
    

    switch (port_mode)
    {
    case CFG_FRAME_SF:              
        pci_write_32 ((u_int32_t *) &comet->cdrc_cfg, 0);
        pci_write_32 ((u_int32_t *) &comet->t1_frmr_cfg, 0);
        pci_write_32 ((u_int32_t *) &comet->sigx_cfg, 0);
        pci_write_32 ((u_int32_t *) &comet->t1_xbas_cfg, 0x20); 
        pci_write_32 ((u_int32_t *) &comet->t1_almi_cfg, 0);
        break;
    case CFG_FRAME_ESF:     
        pci_write_32 ((u_int32_t *) &comet->cdrc_cfg, 0);
        pci_write_32 ((u_int32_t *) &comet->rxce1_ctl, 0x20);   
        pci_write_32 ((u_int32_t *) &comet->txci1_ctl, 0x20);   
        pci_write_32 ((u_int32_t *) &comet->t1_frmr_cfg, 0x30); 
        pci_write_32 ((u_int32_t *) &comet->sigx_cfg, 0x04);    
        pci_write_32 ((u_int32_t *) &comet->t1_xbas_cfg, 0x30); 
        pci_write_32 ((u_int32_t *) &comet->t1_almi_cfg, 0x10); 
        break;
    case CFG_FRAME_E1PLAIN:         
        pci_write_32 ((u_int32_t *) &comet->cdrc_cfg, 0);
        pci_write_32 ((u_int32_t *) &comet->sigx_cfg, 0);
        pci_write_32 ((u_int32_t *) &comet->e1_tran_cfg, 0);
        pci_write_32 ((u_int32_t *) &comet->e1_frmr_aopts, 0x40);
        break;
    case CFG_FRAME_E1CAS:           
        pci_write_32 ((u_int32_t *) &comet->cdrc_cfg, 0);
        pci_write_32 ((u_int32_t *) &comet->sigx_cfg, 0);
        pci_write_32 ((u_int32_t *) &comet->e1_tran_cfg, 0x60);
        pci_write_32 ((u_int32_t *) &comet->e1_frmr_aopts, 0);
        break;
    case CFG_FRAME_E1CRC:           
        pci_write_32 ((u_int32_t *) &comet->cdrc_cfg, 0);
        pci_write_32 ((u_int32_t *) &comet->sigx_cfg, 0);
        pci_write_32 ((u_int32_t *) &comet->e1_tran_cfg, 0x10);
        pci_write_32 ((u_int32_t *) &comet->e1_frmr_aopts, 0xc2);
        break;
    case CFG_FRAME_E1CRC_CAS:       
        pci_write_32 ((u_int32_t *) &comet->cdrc_cfg, 0);
        pci_write_32 ((u_int32_t *) &comet->sigx_cfg, 0);
        pci_write_32 ((u_int32_t *) &comet->e1_tran_cfg, 0x70);
        pci_write_32 ((u_int32_t *) &comet->e1_frmr_aopts, 0x82);
        break;
    case CFG_FRAME_SF_AMI:          
        pci_write_32 ((u_int32_t *) &comet->cdrc_cfg, 0x80);    
        pci_write_32 ((u_int32_t *) &comet->t1_frmr_cfg, 0);
        pci_write_32 ((u_int32_t *) &comet->t1_xbas_cfg, 0);
        pci_write_32 ((u_int32_t *) &comet->t1_almi_cfg, 0);
        pci_write_32 ((u_int32_t *) &comet->sigx_cfg, 0);
        break;
    case CFG_FRAME_ESF_AMI:         
        pci_write_32 ((u_int32_t *) &comet->cdrc_cfg, 0x80);    
        pci_write_32 ((u_int32_t *) &comet->rxce1_ctl, 0x20);   
        pci_write_32 ((u_int32_t *) &comet->txci1_ctl, 0x20);   
        pci_write_32 ((u_int32_t *) &comet->t1_frmr_cfg, 0x30); 
        pci_write_32 ((u_int32_t *) &comet->sigx_cfg, 0x04);    
        pci_write_32 ((u_int32_t *) &comet->t1_xbas_cfg, 0x10); 
        pci_write_32 ((u_int32_t *) &comet->t1_almi_cfg, 0x10); 
        break;
    case CFG_FRAME_E1PLAIN_AMI:       
        pci_write_32 ((u_int32_t *) &comet->cdrc_cfg, 0x80);    
        pci_write_32 ((u_int32_t *) &comet->sigx_cfg, 0);
        pci_write_32 ((u_int32_t *) &comet->e1_tran_cfg, 0x80);
        pci_write_32 ((u_int32_t *) &comet->e1_frmr_aopts, 0x40);
        break;
    case CFG_FRAME_E1CAS_AMI:       
        pci_write_32 ((u_int32_t *) &comet->cdrc_cfg, 0x80);    
        pci_write_32 ((u_int32_t *) &comet->sigx_cfg, 0);
        pci_write_32 ((u_int32_t *) &comet->e1_tran_cfg, 0xe0);
        pci_write_32 ((u_int32_t *) &comet->e1_frmr_aopts, 0);
        break;
    case CFG_FRAME_E1CRC_AMI:       
        pci_write_32 ((u_int32_t *) &comet->cdrc_cfg, 0x80);    
        pci_write_32 ((u_int32_t *) &comet->sigx_cfg, 0);
        pci_write_32 ((u_int32_t *) &comet->e1_tran_cfg, 0x90);
        pci_write_32 ((u_int32_t *) &comet->e1_frmr_aopts, 0xc2);
        break;
    case CFG_FRAME_E1CRC_CAS_AMI:   
        pci_write_32 ((u_int32_t *) &comet->cdrc_cfg, 0x80);    
        pci_write_32 ((u_int32_t *) &comet->sigx_cfg, 0);
        pci_write_32 ((u_int32_t *) &comet->e1_tran_cfg, 0xf0);
        pci_write_32 ((u_int32_t *) &comet->e1_frmr_aopts, 0x82);
        break;
    }                               



    
    
    if (clockmaster)
    {                               
        if (isT1mode)               
            pci_write_32 ((u_int32_t *) &comet->brif_cfg, 0x00);        
        else                        
            pci_write_32 ((u_int32_t *) &comet->brif_cfg, 0x01);        

        
        pci_write_32 ((u_int32_t *) &comet->brif_fpcfg, 0x00);  
        if ((moreParams & CFG_CLK_PORT_MASK) == CFG_CLK_PORT_INTERNAL)
        {
            if (cxt1e1_log_level >= LOG_SBEBUG12)
                pr_info(">> %s: clockmaster internal clock\n", __func__);
            pci_write_32 ((u_int32_t *) &comet->tx_time, 0x0d); 
        } else                      
        {
            if (cxt1e1_log_level >= LOG_SBEBUG12)
                pr_info(">> %s: clockmaster external clock\n", __func__);
            pci_write_32 ((u_int32_t *) &comet->tx_time, 0x09); 
        }

    } else                          
    {
        if (isT1mode)
            pci_write_32 ((u_int32_t *) &comet->brif_cfg, 0x20);        
        else
            pci_write_32 ((u_int32_t *) &comet->brif_cfg, 0x21);        
        pci_write_32 ((u_int32_t *) &comet->brif_fpcfg, 0x20);  
        if (cxt1e1_log_level >= LOG_SBEBUG12)
            pr_info(">> %s: clockslave internal clock\n", __func__);
        pci_write_32 ((u_int32_t *) &comet->tx_time, 0x0d);     
    }

    
    
    pci_write_32 ((u_int32_t *) &comet->brif_pfcfg, 0x01);      

    
    
    if (isT1mode)
        pci_write_32 ((u_int32_t *) &comet->rlps_eqvr, 0x2c);   
    else
        pci_write_32 ((u_int32_t *) &comet->rlps_eqvr, 0x34);   

    
    
    pci_write_32 ((u_int32_t *) &comet->rlps_cfgsts, 0x11);     
    if (isT1mode)
        pci_write_32 ((u_int32_t *) &comet->rlps_alos_thresh, 0x55);    
    else
        pci_write_32 ((u_int32_t *) &comet->rlps_alos_thresh, 0x22);    


    
    
    
    
    
    if (isT1mode)
        pci_write_32 ((u_int32_t *) &comet->btif_cfg, 0x38);    
    else
        pci_write_32 ((u_int32_t *) &comet->btif_cfg, 0x39);    

    pci_write_32 ((u_int32_t *) &comet->btif_fpcfg, 0x01);      

    
    

    
    pci_write_32 ((u_int32_t *) &comet->mdiag, 0x00);

    
    

    WrtXmtWaveformTbl (ci, comet, TWV_table[tix]);
    if (isT1mode)
        WrtRcvEqualizerTbl ((ci_t *) ci, comet, &T1_Equalizer[0]);
    else
        WrtRcvEqualizerTbl ((ci_t *) ci, comet, &E1_Equalizer[0]);
    SetPwrLevel (comet);
}

STATIC void
WrtXmtWaveform (ci_t * ci, comet_t * comet, u_int32_t sample, u_int32_t unit, u_int8_t data)
{
    u_int8_t    WaveformAddr;

    WaveformAddr = (sample << 3) + (unit & 7);
    pci_write_32 ((u_int32_t *) &comet->xlpg_pwave_addr, WaveformAddr);
    pci_flush_write (ci);           
    pci_write_32 ((u_int32_t *) &comet->xlpg_pwave_data, 0x7F & data);
}

STATIC void
WrtXmtWaveformTbl (ci_t * ci, comet_t * comet,
                   u_int8_t table[COMET_NUM_SAMPLES][COMET_NUM_UNITS])
{
    u_int32_t sample, unit;

    for (sample = 0; sample < COMET_NUM_SAMPLES; sample++)
    {
        for (unit = 0; unit < COMET_NUM_UNITS; unit++)
            WrtXmtWaveform (ci, comet, sample, unit, table[sample][unit]);
    }

    
    pci_write_32 ((u_int32_t *) &comet->xlpg_cfg, table[COMET_NUM_SAMPLES][0]);
}



STATIC void
WrtRcvEqualizerTbl (ci_t * ci, comet_t * comet, u_int32_t *table)
{
    u_int32_t   ramaddr;
    volatile u_int32_t value;

    for (ramaddr = 0; ramaddr < 256; ramaddr++)
    {
        
        {
            pci_write_32 ((u_int32_t *) &comet->rlps_eq_rwsel, 0x80);   
            pci_flush_write (ci);   
            pci_write_32 ((u_int32_t *) &comet->rlps_eq_iaddr, (u_int8_t) ramaddr); 
            pci_flush_write (ci);   
            OS_uwait (4, "wret");   
        }

        value = *table++;
        pci_write_32 ((u_int32_t *) &comet->rlps_idata3, (u_int8_t) (value >> 24));
        pci_write_32 ((u_int32_t *) &comet->rlps_idata2, (u_int8_t) (value >> 16));
        pci_write_32 ((u_int32_t *) &comet->rlps_idata1, (u_int8_t) (value >> 8));
        pci_write_32 ((u_int32_t *) &comet->rlps_idata0, (u_int8_t) value);
        pci_flush_write (ci);       

        

        pci_write_32 ((u_int32_t *) &comet->rlps_eq_rwsel, 0);  
        pci_flush_write (ci);       
        pci_write_32 ((u_int32_t *) &comet->rlps_eq_iaddr, (u_int8_t) ramaddr); 
        pci_flush_write (ci);       
        OS_uwait (4, "wret");       
    }

    pci_write_32 ((u_int32_t *) &comet->rlps_eq_cfg, 0xCB);     
}



STATIC void
SetPwrLevel (comet_t * comet)
{
    volatile u_int32_t temp;

    pci_write_32 ((u_int32_t *) &comet->xlpg_fdata_sel, 0x00);  

    pci_write_32 ((u_int32_t *) &comet->xlpg_atest_pctl, 0x01); 
    pci_write_32 ((u_int32_t *) &comet->xlpg_atest_pctl, 0x01);

    temp = pci_read_32 ((u_int32_t *) &comet->xlpg_atest_pctl) & 0xfe;
    pci_write_32 ((u_int32_t *) &comet->xlpg_atest_pctl, temp);

    pci_write_32 ((u_int32_t *) &comet->xlpg_atest_nctl, 0x01); 
    pci_write_32 ((u_int32_t *) &comet->xlpg_atest_nctl, 0x01);

    temp = pci_read_32 ((u_int32_t *) &comet->xlpg_atest_nctl) & 0xfe;
    pci_write_32 ((u_int32_t *) &comet->xlpg_atest_nctl, temp);
    pci_write_32 ((u_int32_t *) &comet->xlpg_fdata_sel, 0x01);  
}


#if 0
STATIC void
SetCometOps (comet_t * comet)
{
    volatile u_int8_t rd_value;

    if (comet == mConfig.C4Func1Base + (COMET0_OFFSET >> 2))
    {
        rd_value = (u_int8_t) pci_read_32 ((u_int32_t *) &comet->brif_cfg);     
        rd_value &= ~0x20;
        pci_write_32 ((u_int32_t *) &comet->brif_cfg, (u_int32_t) rd_value);

        rd_value = (u_int8_t) pci_read_32 ((u_int32_t *) &comet->brif_fpcfg);   
        rd_value &= ~0x20;
        pci_write_32 ((u_int32_t *) &comet->brif_fpcfg, (u_int8_t) rd_value);
    } else
    {
        rd_value = (u_int8_t) pci_read_32 ((u_int32_t *) &comet->brif_cfg);     
        rd_value |= 0x20;
        pci_write_32 ((u_int32_t *) &comet->brif_cfg, (u_int32_t) rd_value);

        rd_value = (u_int8_t) pci_read_32 ((u_int32_t *) &comet->brif_fpcfg);   
        rd_value |= 0x20;
        pci_write_32 ((u_int32_t *) &comet->brif_fpcfg, (u_int8_t) rd_value);
    }
}
#endif

