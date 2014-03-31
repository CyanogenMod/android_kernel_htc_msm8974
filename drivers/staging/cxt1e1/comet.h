#ifndef _INC_COMET_H_
#define _INC_COMET_H_

/*-----------------------------------------------------------------------------
 * comet.h -
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
 *
 * For further information, contact via email: support@sbei.com
 * SBE, Inc.  San Ramon, California  U.S.A.
 *-----------------------------------------------------------------------------
 */

#include <linux/types.h>

#define VINT32  volatile u_int32_t

struct s_comet_reg
{
    VINT32 gbl_cfg;      
    VINT32 clkmon;       
    VINT32 rx_opt;       
    VINT32 rx_line_cfg;  
    VINT32 tx_line_cfg;  
    VINT32 tx_frpass;    
    VINT32 tx_time;      
    VINT32 intr_1;       
    VINT32 intr_2;       
    VINT32 intr_3;       
    VINT32 mdiag;        
    VINT32 mtest;        
    VINT32 adiag;        
    VINT32 rev_id;       
#define pmon  rev_id
    VINT32 reset;        
    VINT32 prgd_phctl;   
    VINT32 cdrc_cfg;     
    VINT32 cdrc_ien;     
    VINT32 cdrc_ists;    
    VINT32 cdrc_alos;    

    VINT32 rjat_ists;    
    VINT32 rjat_n1clk;   
    VINT32 rjat_n2clk;   
    VINT32 rjat_cfg;     

    VINT32 tjat_ists;    
    VINT32 tjat_n1clk;   
    VINT32 tjat_n2clk;   
    VINT32 tjat_cfg;     

    VINT32 rx_elst_cfg;      
    VINT32 rx_elst_ists;     
    VINT32 rx_elst_idle;     
    VINT32 _rx_elst_res1f;   

    VINT32 tx_elst_cfg;      
    VINT32 tx_elst_ists;     
    VINT32 _tx_elst_res22;   
    VINT32 _tx_elst_res23;   
    VINT32 __res24;          
    VINT32 __res25;          
    VINT32 __res26;          
    VINT32 __res27;          

    VINT32 rxce1_ctl;        
    VINT32 rxce1_bits;       
    VINT32 rxce2_ctl;        
    VINT32 rxce2_bits;       
    VINT32 rxce3_ctl;        
    VINT32 rxce3_bits;       
    VINT32 _rxce_res2E;      
    VINT32 _rxce_res2F;      

    VINT32 brif_cfg;         
    VINT32 brif_fpcfg;       
    VINT32 brif_pfcfg;       
    VINT32 brif_tsoff;       
    VINT32 brif_boff;        
    VINT32 _brif_res35;      
    VINT32 _brif_res36;      
    VINT32 _brif_res37;      

    VINT32 txci1_ctl;        
    VINT32 txci1_bits;       
    VINT32 txci2_ctl;        
    VINT32 txci2_bits;       
    VINT32 txci3_ctl;        
    VINT32 txci3_bits;       
    VINT32 _txci_res3E;      
    VINT32 _txci_res3F;      

    VINT32 btif_cfg;         
    VINT32 btif_fpcfg;       
    VINT32 btif_pcfgsts;     
    VINT32 btif_tsoff;       
    VINT32 btif_boff;        
    VINT32 _btif_res45;      
    VINT32 _btif_res46;      
    VINT32 _btif_res47;      
    VINT32 t1_frmr_cfg;      
    VINT32 t1_frmr_ien;      
    VINT32 t1_frmr_ists;     
    VINT32 __res_4B;         
    VINT32 ibcd_cfg;         
    VINT32 ibcd_ies;         
    VINT32 ibcd_act;         
    VINT32 ibcd_deact;       

    VINT32 sigx_cfg;         
    VINT32 sigx_acc_cos;     
    VINT32 sigx_iac_cos;     
    VINT32 sigx_idb_cos;     

    VINT32 t1_xbas_cfg;      
    VINT32 t1_xbas_altx;     
    VINT32 t1_xibc_ctl;      
    VINT32 t1_xibc_lbcode;   

    VINT32 pmon_ies;         
    VINT32 pmon_fberr;       
    VINT32 pmon_feb_lsb;     
    VINT32 pmon_feb_msb;     
    VINT32 pmon_bed_lsb;     
    VINT32 pmon_bed_msb;     
    VINT32 pmon_lvc_lsb;     
    VINT32 pmon_lvc_msb;     

    VINT32 t1_almi_cfg;      
    VINT32 t1_almi_ien;      
    VINT32 t1_almi_ists;     
    VINT32 t1_almi_detsts;   

    VINT32 _t1_pdvd_res64;   
    VINT32 t1_pdvd_ies;      
    VINT32 _t1_xboc_res66;   
    VINT32 t1_xboc_code;     
    VINT32 _t1_xpde_res68;   
    VINT32 t1_xpde_ies;      

    VINT32 t1_rboc_ena;      
    VINT32 t1_rboc_sts;      

    VINT32 t1_tpsc_cfg;      
    VINT32 t1_tpsc_sts;      
    VINT32 t1_tpsc_ciaddr;   
    VINT32 t1_tpsc_cidata;   
    VINT32 t1_rpsc_cfg;      
    VINT32 t1_rpsc_sts;      
    VINT32 t1_rpsc_ciaddr;   
    VINT32 t1_rpsc_cidata;   
    VINT32 __res74;          
    VINT32 __res75;          
    VINT32 __res76;          
    VINT32 __res77;          

    VINT32 t1_aprm_cfg;      
    VINT32 t1_aprm_load;     
    VINT32 t1_aprm_ists;     
    VINT32 t1_aprm_1sec_2;   
    VINT32 t1_aprm_1sec_3;   
    VINT32 t1_aprm_1sec_4;   
    VINT32 t1_aprm_1sec_5;   
    VINT32 t1_aprm_1sec_6;   

    VINT32 e1_tran_cfg;      
    VINT32 e1_tran_txalarm;  
    VINT32 e1_tran_intctl;   
    VINT32 e1_tran_extrab;   
    VINT32 e1_tran_ien;      
    VINT32 e1_tran_ists;     
    VINT32 e1_tran_nats;     
    VINT32 e1_tran_nat;      
    VINT32 __res88;          
    VINT32 __res89;          
    VINT32 __res8A;          
    VINT32 __res8B;          

    VINT32 _t1_frmr_res8C;   
    VINT32 _t1_frmr_res8D;   
    VINT32 __res8E;          
    VINT32 __res8F;          

    VINT32 e1_frmr_aopts;    
    VINT32 e1_frmr_mopts;    
    VINT32 e1_frmr_ien;      
    VINT32 e1_frmr_mien;     
    VINT32 e1_frmr_ists;     
    VINT32 e1_frmr_mists;    
    VINT32 e1_frmr_sts;      
    VINT32 e1_frmr_masts;    
    VINT32 e1_frmr_nat_bits; 
    VINT32 e1_frmr_crc_lsb;  
    VINT32 e1_frmr_crc_msb;  
    VINT32 e1_frmr_nat_ien;  
    VINT32 e1_frmr_nat_ists; 
    VINT32 e1_frmr_nat;      
    VINT32 e1_frmr_fp_ien;   
    VINT32 e1_frmr_fp_ists;  

    VINT32 __resA0;          
    VINT32 __resA1;          
    VINT32 __resA2;          
    VINT32 __resA3;          
    VINT32 __resA4;          
    VINT32 __resA5;          
    VINT32 __resA6;          
    VINT32 __resA7;          

    VINT32 tdpr1_cfg;        
    VINT32 tdpr1_utl;        
    VINT32 tdpr1_ltl;        
    VINT32 tdpr1_ien;        
    VINT32 tdpr1_ists;       
    VINT32 tdpr1_data;       
    VINT32 __resAE;          
    VINT32 __resAF;          
    VINT32 tdpr2_cfg;        
    VINT32 tdpr2_utl;        
    VINT32 tdpr2_ltl;        
    VINT32 tdpr2_ien;        
    VINT32 tdpr2_ists;       
    VINT32 tdpr2_data;       
    VINT32 __resB6;          
    VINT32 __resB7;          
    VINT32 tdpr3_cfg;        
    VINT32 tdpr3_utl;        
    VINT32 tdpr3_ltl;        
    VINT32 tdpr3_ien;        
    VINT32 tdpr3_ists;       
    VINT32 tdpr3_data;       
    VINT32 __resBE;          
    VINT32 __resBF;          

    VINT32 rdlc1_cfg;        
    VINT32 rdlc1_intctl;     
    VINT32 rdlc1_sts;        
    VINT32 rdlc1_data;       
    VINT32 rdlc1_paddr;      
    VINT32 rdlc1_saddr;      
    VINT32 __resC6;          
    VINT32 __resC7;          
    VINT32 rdlc2_cfg;        
    VINT32 rdlc2_intctl;     
    VINT32 rdlc2_sts;        
    VINT32 rdlc2_data;       
    VINT32 rdlc2_paddr;      
    VINT32 rdlc2_saddr;      
    VINT32 __resCE;          
    VINT32 __resCF;          
    VINT32 rdlc3_cfg;        
    VINT32 rdlc3_intctl;     
    VINT32 rdlc3_sts;        
    VINT32 rdlc3_data;       
    VINT32 rdlc3_paddr;      
    VINT32 rdlc3_saddr;      

    VINT32 csu_cfg;          
    VINT32 _csu_resD7;       

    VINT32 rlps_idata3;      
    VINT32 rlps_idata2;      
    VINT32 rlps_idata1;      
    VINT32 rlps_idata0;      
    VINT32 rlps_eqvr;        
    VINT32 _rlps_resDD;      
    VINT32 _rlps_resDE;      
    VINT32 _rlps_resDF;      

    VINT32 prgd_ctl;         
    VINT32 prgd_ies;         
    VINT32 prgd_shift_len;   
    VINT32 prgd_tap;         
    VINT32 prgd_errin;       
    VINT32 _prgd_resE5;      
    VINT32 _prgd_resE6;      
    VINT32 _prgd_resE7;      
    VINT32 prgd_patin1;      
    VINT32 prgd_patin2;      
    VINT32 prgd_patin3;      
    VINT32 prgd_patin4;      
    VINT32 prgd_patdet1;     
    VINT32 prgd_patdet2;     
    VINT32 prgd_patdet3;     
    VINT32 prgd_patdet4;     

    VINT32 xlpg_cfg;         
    VINT32 xlpg_ctlsts;      
    VINT32 xlpg_pwave_addr;  
    VINT32 xlpg_pwave_data;  
    VINT32 xlpg_atest_pctl;  
    VINT32 xlpg_atest_nctl;  
    VINT32 xlpg_fdata_sel;   
    VINT32 _xlpg_resF7;      

    VINT32 rlps_cfgsts;      
    VINT32 rlps_alos_thresh; 
    VINT32 rlps_alos_dper;   
    VINT32 rlps_alos_cper;   
    VINT32 rlps_eq_iaddr;    
    VINT32 rlps_eq_rwsel;    
    VINT32 rlps_eq_ctlsts;   
    VINT32 rlps_eq_cfg;      
};

typedef struct s_comet_reg comet_t;

#define COMET_MDIAG_ID5        0x40
#define COMET_MDIAG_LBMASK     0x3F
#define COMET_MDIAG_PAYLB      0x20
#define COMET_MDIAG_LINELB     0x10
#define COMET_MDIAG_RAIS       0x08
#define COMET_MDIAG_DDLB       0x04
#define COMET_MDIAG_TXMFP      0x02
#define COMET_MDIAG_TXLOS      0x01
#define COMET_MDIAG_LBOFF      0x00

#undef VINT32

#ifdef __KERNEL__
extern void
init_comet (void *, comet_t *, u_int32_t, int, u_int8_t);
#endif

#endif                          
