/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2003-2005 Silicon Graphics, Inc. All rights reserved.
 */

#ifndef __ASM_IA64_SN_TIOCE_H__
#define __ASM_IA64_SN_TIOCE_H__

#define TIOCE_PART_NUM			0xCE00
#define TIOCE_SRC_ID			0x01
#define TIOCE_REV_A			0x1

#define CE_VIRT_PPB_VENDOR_ID		0x10a9
#define CE_VIRT_PPB_DEVICE_ID		0x4002

#define CE_HOST_BRIDGE_VENDOR_ID	0x10a9
#define CE_HOST_BRIDGE_DEVICE_ID	0x4001


#define TIOCE_NUM_M40_ATES		4096
#define TIOCE_NUM_M3240_ATES		2048
#define TIOCE_NUM_PORTS			2

typedef volatile struct tioce {
	u64	ce_adm_id;				
	u64	ce_pad_000008;				
	u64	ce_adm_dyn_credit_status;		
	u64	ce_adm_last_credit_status;		
	u64	ce_adm_credit_limit;			
	u64	ce_adm_force_credit;			
	u64	ce_adm_control;				
	u64	ce_adm_mmr_chn_timeout;			
	u64	ce_adm_ssp_ure_timeout;			
	u64	ce_adm_ssp_dre_timeout;			
	u64	ce_adm_ssp_debug_sel;			
	u64	ce_adm_int_status;			
	u64	ce_adm_int_status_alias;		
	u64	ce_adm_int_mask;			
	u64	ce_adm_int_pending;			
	u64	ce_adm_force_int;			
	u64	ce_adm_ure_ups_buf_barrier_flush;	
	u64	ce_adm_int_dest[15];	    
	u64	ce_adm_error_summary;			
	u64	ce_adm_error_summary_alias;		
	u64	ce_adm_error_mask;			
	u64	ce_adm_first_error;			
	u64	ce_adm_error_overflow;			
	u64	ce_adm_error_overflow_alias;		
	u64	ce_pad_000130[2];	    
	u64	ce_adm_tnum_error;			
	u64	ce_adm_mmr_err_detail;			
	u64	ce_adm_msg_sram_perr_detail;		
	u64	ce_adm_bap_sram_perr_detail;		
	u64	ce_adm_ce_sram_perr_detail;		
	u64	ce_adm_ce_credit_oflow_detail;		
	u64	ce_adm_tx_link_idle_max_timer;		
	u64	ce_adm_pcie_debug_sel;			
	u64	ce_pad_000180[16];	    

	u64	ce_adm_pcie_debug_sel_top;		
	u64	ce_adm_pcie_debug_lat_sel_lo_top;	
	u64	ce_adm_pcie_debug_lat_sel_hi_top;	
	u64	ce_adm_pcie_debug_trig_sel_top;		
	u64	ce_adm_pcie_debug_trig_lat_sel_lo_top;	
	u64	ce_adm_pcie_debug_trig_lat_sel_hi_top;	
	u64	ce_adm_pcie_trig_compare_top;		
	u64	ce_adm_pcie_trig_compare_en_top;	
	u64	ce_adm_ssp_debug_sel_top;		
	u64	ce_adm_ssp_debug_lat_sel_lo_top;	
	u64	ce_adm_ssp_debug_lat_sel_hi_top;	
	u64	ce_adm_ssp_debug_trig_sel_top;		
	u64	ce_adm_ssp_debug_trig_lat_sel_lo_top;	
	u64	ce_adm_ssp_debug_trig_lat_sel_hi_top;	
	u64	ce_adm_ssp_trig_compare_top;		
	u64	ce_adm_ssp_trig_compare_en_top;		
	u64	ce_pad_000280[48];	    

	u64	ce_adm_bap_ctrl;			
	u64	ce_pad_000408[127];	    

	u64	ce_msg_buf_data63_0[35];    
	u64	ce_pad_000920[29];	    

	u64	ce_msg_buf_data127_64[35];  
	u64	ce_pad_000B20[29];	    

	u64	ce_msg_buf_parity[35];	    
	u64	ce_pad_000D20[29];	    

	u64	ce_pad_000E00[576];	    

	#define ce_lsi(link_num)	ce_lsi[link_num-1]
	struct ce_lsi_reg {
		u64	ce_lsi_lpu_id;			
		u64	ce_lsi_rst;			
		u64	ce_lsi_dbg_stat;		
		u64	ce_lsi_dbg_cfg;			
		u64	ce_lsi_ltssm_ctrl;		
		u64	ce_lsi_lk_stat;			
		u64	ce_pad_00z030[2];   
		u64	ce_lsi_int_and_stat;		
		u64	ce_lsi_int_mask;		
		u64	ce_pad_00z050[22];  
		u64	ce_lsi_lk_perf_cnt_sel;		
		u64	ce_pad_00z108;			
		u64	ce_lsi_lk_perf_cnt_ctrl;	
		u64	ce_pad_00z118;			
		u64	ce_lsi_lk_perf_cnt1;		
		u64	ce_lsi_lk_perf_cnt1_test;	
		u64	ce_lsi_lk_perf_cnt2;		
		u64	ce_lsi_lk_perf_cnt2_test;	
		u64	ce_pad_00z140[24];  
		u64	ce_lsi_lk_lyr_cfg;		
		u64	ce_lsi_lk_lyr_status;		
		u64	ce_lsi_lk_lyr_int_stat;		
		u64	ce_lsi_lk_ly_int_stat_test;	
		u64	ce_lsi_lk_ly_int_stat_mask;	
		u64	ce_pad_00z228[3];   
		u64	ce_lsi_fc_upd_ctl;		
		u64	ce_pad_00z248[3];   
		u64	ce_lsi_flw_ctl_upd_to_timer;	
		u64	ce_lsi_flw_ctl_upd_timer0;	
		u64	ce_lsi_flw_ctl_upd_timer1;	
		u64	ce_pad_00z278[49];  
		u64	ce_lsi_freq_nak_lat_thrsh;	
		u64	ce_lsi_ack_nak_lat_tmr;		
		u64	ce_lsi_rply_tmr_thr;		
		u64	ce_lsi_rply_tmr;		
		u64	ce_lsi_rply_num_stat;		
		u64	ce_lsi_rty_buf_max_addr;	
		u64	ce_lsi_rty_fifo_ptr;		
		u64	ce_lsi_rty_fifo_rd_wr_ptr;	
		u64	ce_lsi_rty_fifo_cred;		
		u64	ce_lsi_seq_cnt;			
		u64	ce_lsi_ack_sent_seq_num;	
		u64	ce_lsi_seq_cnt_fifo_max_addr;	
		u64	ce_lsi_seq_cnt_fifo_ptr;	
		u64	ce_lsi_seq_cnt_rd_wr_ptr;	
		u64	ce_lsi_tx_lk_ts_ctl;		
		u64	ce_pad_00z478;			
		u64	ce_lsi_mem_addr_ctl;		
		u64	ce_lsi_mem_d_ld0;		
		u64	ce_lsi_mem_d_ld1;		
		u64	ce_lsi_mem_d_ld2;		
		u64	ce_lsi_mem_d_ld3;		
		u64	ce_lsi_mem_d_ld4;		
		u64	ce_pad_00z4B0[2];   
		u64	ce_lsi_rty_d_cnt;		
		u64	ce_lsi_seq_buf_cnt;		
		u64	ce_lsi_seq_buf_bt_d;		
		u64	ce_pad_00z4D8;			
		u64	ce_lsi_ack_lat_thr;		
		u64	ce_pad_00z4E8[3];   
		u64	ce_lsi_nxt_rcv_seq_1_cntr;	
		u64	ce_lsi_unsp_dllp_rcvd;		
		u64	ce_lsi_rcv_lk_ts_ctl;		
		u64	ce_pad_00z518[29];  
		u64	ce_lsi_phy_lyr_cfg;		
		u64	ce_pad_00z608;			
		u64	ce_lsi_phy_lyr_int_stat;	
		u64	ce_lsi_phy_lyr_int_stat_test;	
		u64	ce_lsi_phy_lyr_int_mask;	
		u64	ce_pad_00z628[11];  
		u64	ce_lsi_rcv_phy_cfg;		
		u64	ce_lsi_rcv_phy_stat1;		
		u64	ce_lsi_rcv_phy_stat2;		
		u64	ce_lsi_rcv_phy_stat3;		
		u64	ce_lsi_rcv_phy_int_stat;	
		u64	ce_lsi_rcv_phy_int_stat_test;	
		u64	ce_lsi_rcv_phy_int_mask;	
		u64	ce_pad_00z6B8[9];   
		u64	ce_lsi_tx_phy_cfg;		
		u64	ce_lsi_tx_phy_stat;		
		u64	ce_lsi_tx_phy_int_stat;		
		u64	ce_lsi_tx_phy_int_stat_test;	
		u64	ce_lsi_tx_phy_int_mask;		
		u64	ce_lsi_tx_phy_stat2;		
		u64	ce_pad_00z730[10];  
		u64	ce_lsi_ltssm_cfg1;		
		u64	ce_lsi_ltssm_cfg2;		
		u64	ce_lsi_ltssm_cfg3;		
		u64	ce_lsi_ltssm_cfg4;		
		u64	ce_lsi_ltssm_cfg5;		
		u64	ce_lsi_ltssm_stat1;		
		u64	ce_lsi_ltssm_stat2;		
		u64	ce_lsi_ltssm_int_stat;		
		u64	ce_lsi_ltssm_int_stat_test;	
		u64	ce_lsi_ltssm_int_mask;		
		u64	ce_lsi_ltssm_stat_wr_en;	
		u64	ce_pad_00z7D8[5];   
		u64	ce_lsi_gb_cfg1;			
		u64	ce_lsi_gb_cfg2;			
		u64	ce_lsi_gb_cfg3;			
		u64	ce_lsi_gb_cfg4;			
		u64	ce_lsi_gb_stat;			
		u64	ce_lsi_gb_int_stat;		
		u64	ce_lsi_gb_int_stat_test;	
		u64	ce_lsi_gb_int_mask;		
		u64	ce_lsi_gb_pwr_dn1;		
		u64	ce_lsi_gb_pwr_dn2;		
		u64	ce_pad_00z850[246]; 
	} ce_lsi[2];

	u64	ce_pad_004000[10];	    

	u64	ce_crm_debug_mux;			
	u64	ce_pad_004058;				
	u64	ce_crm_ssp_err_cmd_wrd;			
	u64	ce_crm_ssp_err_addr;			
	u64	ce_crm_ssp_err_syn;			

	u64	ce_pad_004078[499];	    

	u64	ce_cxm_dyn_credit_status;		
	u64	ce_cxm_last_credit_status;		
	u64	ce_cxm_credit_limit;			
	u64	ce_cxm_force_credit;			
	u64	ce_cxm_disable_bypass;			
	u64	ce_pad_005038[3];	    
	u64	ce_cxm_debug_mux;			

        u64        ce_pad_005058[501];         

	#define ce_dtl(link_num)	ce_dtl_utl[link_num-1]
	#define ce_utl(link_num)	ce_dtl_utl[link_num-1]
	struct ce_dtl_utl_reg {
		
		u64	ce_dtl_dtdr_credit_limit;	
		u64	ce_dtl_dtdr_credit_force;	
		u64	ce_dtl_dyn_credit_status;	
		u64	ce_dtl_dtl_last_credit_stat;	
		u64	ce_dtl_dtl_ctrl;		
		u64	ce_pad_00y028[5];   
		u64	ce_dtl_debug_sel;		
		u64	ce_pad_00y058[501]; 

		
		u64	ce_utl_utl_ctrl;		
		u64	ce_utl_debug_sel;		
		u64	ce_pad_00z010[510]; 
	} ce_dtl_utl[2];

	u64	ce_pad_00A000[514];	    

	u64	ce_ure_dyn_credit_status;		
	u64	ce_ure_last_credit_status;		
	u64	ce_ure_credit_limit;			
	u64	ce_pad_00B028;				
	u64	ce_ure_control;				
	u64	ce_ure_status;				
	u64	ce_pad_00B040[2];	    
	u64	ce_ure_debug_sel;			
	u64	ce_ure_pcie_debug_sel;			
	u64	ce_ure_ssp_err_cmd_wrd;			
	u64	ce_ure_ssp_err_addr;			
	u64	ce_ure_page_map;			
	u64	ce_ure_dir_map[TIOCE_NUM_PORTS];	
	u64	ce_ure_pipe_sel1;			
	u64	ce_ure_pipe_mask1;			
	u64	ce_ure_pipe_sel2;			
	u64	ce_ure_pipe_mask2;			
	u64	ce_ure_pcie1_credits_sent;		
	u64	ce_ure_pcie1_credits_used;		
	u64	ce_ure_pcie1_credit_limit;		
	u64	ce_ure_pcie2_credits_sent;		
	u64	ce_ure_pcie2_credits_used;		
	u64	ce_ure_pcie2_credit_limit;		
	u64	ce_ure_pcie_force_credit;		
	u64	ce_ure_rd_tnum_val;			
	u64	ce_ure_rd_tnum_rsp_rcvd;		
	u64	ce_ure_rd_tnum_esent_timer;		
	u64	ce_ure_rd_tnum_error;			
	u64	ce_ure_rd_tnum_first_cl;		
	u64	ce_ure_rd_tnum_link_buf;		
	u64	ce_ure_wr_tnum_val;			
	u64	ce_ure_sram_err_addr0;			
	u64	ce_ure_sram_err_addr1;			
	u64	ce_ure_sram_err_addr2;			
	u64	ce_ure_sram_rd_addr0;			
	u64	ce_ure_sram_rd_addr1;			
	u64	ce_ure_sram_rd_addr2;			
	u64	ce_ure_sram_wr_addr0;			
	u64	ce_ure_sram_wr_addr1;			
	u64	ce_ure_sram_wr_addr2;			
	u64	ce_ure_buf_flush10;			
	u64	ce_ure_buf_flush11;			
	u64	ce_ure_buf_flush12;			
	u64	ce_ure_buf_flush13;			
	u64	ce_ure_buf_flush20;			
	u64	ce_ure_buf_flush21;			
	u64	ce_ure_buf_flush22;			
	u64	ce_ure_buf_flush23;			
	u64	ce_ure_pcie_control1;			
	u64	ce_ure_pcie_control2;			

	u64	ce_pad_00B1B0[458];	    

	
	struct ce_ure_maint_ups_dat1_data {
		u64	data63_0[512];	    
		u64	data127_64[512];    
		u64	parity[512];	    
	} ce_ure_maint_ups_dat1;

	
	struct ce_ure_maint_ups_hdr1_data {
		u64	data63_0[512];	    
		u64	data127_64[512];    
		u64	parity[512];	    
	} ce_ure_maint_ups_hdr1;

	
	struct ce_ure_maint_ups_dat2_data {
		u64	data63_0[512];	    
		u64	data127_64[512];    
		u64	parity[512];	    
	} ce_ure_maint_ups_dat2;

	
	struct ce_ure_maint_ups_hdr2_data {
		u64	data63_0[512];	    
		u64	data127_64[512];    
		u64	parity[512];	    
	} ce_ure_maint_ups_hdr2;

	
	struct ce_ure_maint_dns_dat_data {
		u64	data63_0[512];	    
		u64	data127_64[512];    
		u64	parity[512];	    
	} ce_ure_maint_dns_dat;

	
	struct	ce_ure_maint_dns_hdr_data {
		u64	data31_0[64];	    
		u64	data95_32[64];	    
		u64	parity[64];	    
	} ce_ure_maint_dns_hdr;

	
	struct	ce_ure_maint_rci_data {
		u64	data41_0[64];	    
		u64	data69_42[64];	    
	} ce_ure_maint_rci;

	
	u64	ce_ure_maint_rspq[64];	    

	u64	ce_pad_01C000[4224];	    

	
	struct	ce_adm_maint_bap_buf_data {
		u64	data63_0[258];	    
		u64	data127_64[258];    
		u64	parity[258];	    
	} ce_adm_maint_bap_buf;

	u64	ce_pad_025830[5370];	    

			    
	u64	ce_ure_ate40[TIOCE_NUM_M40_ATES];

		    
	u64	ce_ure_ate3240[TIOCE_NUM_M3240_ATES];

	u64	ce_pad_03C000[2050];	    

	u64	ce_dre_dyn_credit_status1;		
	u64	ce_dre_dyn_credit_status2;		
	u64	ce_dre_last_credit_status1;		
	u64	ce_dre_last_credit_status2;		
	u64	ce_dre_credit_limit1;			
	u64	ce_dre_credit_limit2;			
	u64	ce_dre_force_credit1;			
	u64	ce_dre_force_credit2;			
	u64	ce_dre_debug_mux1;			
	u64	ce_dre_debug_mux2;			
	u64	ce_dre_ssp_err_cmd_wrd;			
	u64	ce_dre_ssp_err_addr;			
	u64	ce_dre_comp_err_cmd_wrd;		
	u64	ce_dre_comp_err_addr;			
	u64	ce_dre_req_status;			
	u64	ce_dre_config1;				
	u64	ce_dre_config2;				
	u64	ce_dre_config_req_status;		
	u64	ce_pad_0400A0[12];	    
	u64	ce_dre_dyn_fifo;			
	u64	ce_pad_040108[3];	    
	u64	ce_dre_last_fifo;			

	u64	ce_pad_040128[27];	    

	
	struct	ce_dre_maint_ds_head_queue {
		u64	data63_0[32];	    
		u64	data127_64[32];	    
		u64	parity[32];	    
	} ce_dre_maint_ds_head_q;

	u64	ce_pad_040500[352];	    

	
	struct	ce_dre_maint_ds_data_queue {
		u64	data63_0[256];	    
		u64	ce_pad_041800[256]; 
		u64	data127_64[256];    
		u64	ce_pad_042800[256]; 
		u64	parity[256];	    
		u64	ce_pad_043800[256]; 
	} ce_dre_maint_ds_data_q;

	
	struct	ce_dre_maint_ure_us_rsp_queue {
		u64	data63_0[8];	    
		u64	ce_pad_044040[24];  
		u64	data127_64[8];      
		u64	ce_pad_044140[24];  
		u64	parity[8];	    
		u64	ce_pad_044240[24];  
	} ce_dre_maint_ure_us_rsp_q;

	u64 	ce_dre_maint_us_wrt_rsp[32];

	u64	ce_end_of_struct;			
} tioce_t;

#define CE_LSI_GB_CFG1_RXL0S_THS_SHFT	0
#define CE_LSI_GB_CFG1_RXL0S_THS_MASK	(0xffULL << 0)
#define CE_LSI_GB_CFG1_RXL0S_SMP_SHFT	8
#define CE_LSI_GB_CFG1_RXL0S_SMP_MASK	(0xfULL << 8)
#define CE_LSI_GB_CFG1_RXL0S_ADJ_SHFT	12
#define CE_LSI_GB_CFG1_RXL0S_ADJ_MASK	(0x7ULL << 12)
#define CE_LSI_GB_CFG1_RXL0S_FLT_SHFT	15
#define CE_LSI_GB_CFG1_RXL0S_FLT_MASK	(0x1ULL << 15)
#define CE_LSI_GB_CFG1_LPBK_SEL_SHFT	16
#define CE_LSI_GB_CFG1_LPBK_SEL_MASK	(0x3ULL << 16)
#define CE_LSI_GB_CFG1_LPBK_EN_SHFT	18
#define CE_LSI_GB_CFG1_LPBK_EN_MASK	(0x1ULL << 18)
#define CE_LSI_GB_CFG1_RVRS_LB_SHFT	19
#define CE_LSI_GB_CFG1_RVRS_LB_MASK	(0x1ULL << 19)
#define CE_LSI_GB_CFG1_RVRS_CLK_SHFT	20
#define CE_LSI_GB_CFG1_RVRS_CLK_MASK	(0x3ULL << 20)
#define CE_LSI_GB_CFG1_SLF_TS_SHFT	24
#define CE_LSI_GB_CFG1_SLF_TS_MASK	(0xfULL << 24)

#define CE_ADM_INT_CE_ERROR_SHFT		0
#define CE_ADM_INT_LSI1_IP_ERROR_SHFT		1
#define CE_ADM_INT_LSI2_IP_ERROR_SHFT		2
#define CE_ADM_INT_PCIE_ERROR_SHFT		3
#define CE_ADM_INT_PORT1_HOTPLUG_EVENT_SHFT	4
#define CE_ADM_INT_PORT2_HOTPLUG_EVENT_SHFT	5
#define CE_ADM_INT_PCIE_PORT1_DEV_A_SHFT	6
#define CE_ADM_INT_PCIE_PORT1_DEV_B_SHFT	7
#define CE_ADM_INT_PCIE_PORT1_DEV_C_SHFT	8
#define CE_ADM_INT_PCIE_PORT1_DEV_D_SHFT	9
#define CE_ADM_INT_PCIE_PORT2_DEV_A_SHFT	10
#define CE_ADM_INT_PCIE_PORT2_DEV_B_SHFT	11
#define CE_ADM_INT_PCIE_PORT2_DEV_C_SHFT	12
#define CE_ADM_INT_PCIE_PORT2_DEV_D_SHFT	13
#define CE_ADM_INT_PCIE_MSG_SHFT		14 
#define CE_ADM_INT_PCIE_MSG_SLOT_0_SHFT		14
#define CE_ADM_INT_PCIE_MSG_SLOT_1_SHFT		15
#define CE_ADM_INT_PCIE_MSG_SLOT_2_SHFT		16
#define CE_ADM_INT_PCIE_MSG_SLOT_3_SHFT		17
#define CE_ADM_INT_PORT1_PM_PME_MSG_SHFT	22
#define CE_ADM_INT_PORT2_PM_PME_MSG_SHFT	23

#define CE_ADM_FORCE_INT_PCIE_PORT1_DEV_A_SHFT	0
#define CE_ADM_FORCE_INT_PCIE_PORT1_DEV_B_SHFT	1
#define CE_ADM_FORCE_INT_PCIE_PORT1_DEV_C_SHFT	2
#define CE_ADM_FORCE_INT_PCIE_PORT1_DEV_D_SHFT	3
#define CE_ADM_FORCE_INT_PCIE_PORT2_DEV_A_SHFT	4
#define CE_ADM_FORCE_INT_PCIE_PORT2_DEV_B_SHFT	5
#define CE_ADM_FORCE_INT_PCIE_PORT2_DEV_C_SHFT	6
#define CE_ADM_FORCE_INT_PCIE_PORT2_DEV_D_SHFT	7
#define CE_ADM_FORCE_INT_ALWAYS_SHFT		8

#define INTR_VECTOR_SHFT			56

#define CE_ADM_ERR_CRM_SSP_REQ_INVALID			(0x1ULL <<  0)
#define CE_ADM_ERR_SSP_REQ_HEADER			(0x1ULL <<  1)
#define CE_ADM_ERR_SSP_RSP_HEADER			(0x1ULL <<  2)
#define CE_ADM_ERR_SSP_PROTOCOL_ERROR			(0x1ULL <<  3)
#define CE_ADM_ERR_SSP_SBE				(0x1ULL <<  4)
#define CE_ADM_ERR_SSP_MBE				(0x1ULL <<  5)
#define CE_ADM_ERR_CXM_CREDIT_OFLOW			(0x1ULL <<  6)
#define CE_ADM_ERR_DRE_SSP_REQ_INVAL			(0x1ULL <<  7)
#define CE_ADM_ERR_SSP_REQ_LONG				(0x1ULL <<  8)
#define CE_ADM_ERR_SSP_REQ_OFLOW			(0x1ULL <<  9)
#define CE_ADM_ERR_SSP_REQ_SHORT			(0x1ULL << 10)
#define CE_ADM_ERR_SSP_REQ_SIDEBAND			(0x1ULL << 11)
#define CE_ADM_ERR_SSP_REQ_ADDR_ERR			(0x1ULL << 12)
#define CE_ADM_ERR_SSP_REQ_BAD_BE			(0x1ULL << 13)
#define CE_ADM_ERR_PCIE_COMPL_TIMEOUT			(0x1ULL << 14)
#define CE_ADM_ERR_PCIE_UNEXP_COMPL			(0x1ULL << 15)
#define CE_ADM_ERR_PCIE_ERR_COMPL			(0x1ULL << 16)
#define CE_ADM_ERR_DRE_CREDIT_OFLOW			(0x1ULL << 17)
#define CE_ADM_ERR_DRE_SRAM_PE				(0x1ULL << 18)
#define CE_ADM_ERR_SSP_RSP_INVALID			(0x1ULL << 19)
#define CE_ADM_ERR_SSP_RSP_LONG				(0x1ULL << 20)
#define CE_ADM_ERR_SSP_RSP_SHORT			(0x1ULL << 21)
#define CE_ADM_ERR_SSP_RSP_SIDEBAND			(0x1ULL << 22)
#define CE_ADM_ERR_URE_SSP_RSP_UNEXP			(0x1ULL << 23)
#define CE_ADM_ERR_URE_SSP_WR_REQ_TIMEOUT		(0x1ULL << 24)
#define CE_ADM_ERR_URE_SSP_RD_REQ_TIMEOUT		(0x1ULL << 25)
#define CE_ADM_ERR_URE_ATE3240_PAGE_FAULT		(0x1ULL << 26)
#define CE_ADM_ERR_URE_ATE40_PAGE_FAULT			(0x1ULL << 27)
#define CE_ADM_ERR_URE_CREDIT_OFLOW			(0x1ULL << 28)
#define CE_ADM_ERR_URE_SRAM_PE				(0x1ULL << 29)
#define CE_ADM_ERR_ADM_SSP_RSP_UNEXP			(0x1ULL << 30)
#define CE_ADM_ERR_ADM_SSP_REQ_TIMEOUT			(0x1ULL << 31)
#define CE_ADM_ERR_MMR_ACCESS_ERROR			(0x1ULL << 32)
#define CE_ADM_ERR_MMR_ADDR_ERROR			(0x1ULL << 33)
#define CE_ADM_ERR_ADM_CREDIT_OFLOW			(0x1ULL << 34)
#define CE_ADM_ERR_ADM_SRAM_PE				(0x1ULL << 35)
#define CE_ADM_ERR_DTL1_MIN_PDATA_CREDIT_ERR		(0x1ULL << 36)
#define CE_ADM_ERR_DTL1_INF_COMPL_CRED_UPDT_ERR		(0x1ULL << 37)
#define CE_ADM_ERR_DTL1_INF_POSTED_CRED_UPDT_ERR	(0x1ULL << 38)
#define CE_ADM_ERR_DTL1_INF_NPOSTED_CRED_UPDT_ERR	(0x1ULL << 39)
#define CE_ADM_ERR_DTL1_COMP_HD_CRED_MAX_ERR		(0x1ULL << 40)
#define CE_ADM_ERR_DTL1_COMP_D_CRED_MAX_ERR		(0x1ULL << 41)
#define CE_ADM_ERR_DTL1_NPOSTED_HD_CRED_MAX_ERR		(0x1ULL << 42)
#define CE_ADM_ERR_DTL1_NPOSTED_D_CRED_MAX_ERR		(0x1ULL << 43)
#define CE_ADM_ERR_DTL1_POSTED_HD_CRED_MAX_ERR		(0x1ULL << 44)
#define CE_ADM_ERR_DTL1_POSTED_D_CRED_MAX_ERR		(0x1ULL << 45)
#define CE_ADM_ERR_DTL2_MIN_PDATA_CREDIT_ERR		(0x1ULL << 46)
#define CE_ADM_ERR_DTL2_INF_COMPL_CRED_UPDT_ERR		(0x1ULL << 47)
#define CE_ADM_ERR_DTL2_INF_POSTED_CRED_UPDT_ERR	(0x1ULL << 48)
#define CE_ADM_ERR_DTL2_INF_NPOSTED_CRED_UPDT_ERR	(0x1ULL << 49)
#define CE_ADM_ERR_DTL2_COMP_HD_CRED_MAX_ERR		(0x1ULL << 50)
#define CE_ADM_ERR_DTL2_COMP_D_CRED_MAX_ERR		(0x1ULL << 51)
#define CE_ADM_ERR_DTL2_NPOSTED_HD_CRED_MAX_ERR		(0x1ULL << 52)
#define CE_ADM_ERR_DTL2_NPOSTED_D_CRED_MAX_ERR		(0x1ULL << 53)
#define CE_ADM_ERR_DTL2_POSTED_HD_CRED_MAX_ERR		(0x1ULL << 54)
#define CE_ADM_ERR_DTL2_POSTED_D_CRED_MAX_ERR		(0x1ULL << 55)
#define CE_ADM_ERR_PORT1_PCIE_COR_ERR			(0x1ULL << 56)
#define CE_ADM_ERR_PORT1_PCIE_NFAT_ERR			(0x1ULL << 57)
#define CE_ADM_ERR_PORT1_PCIE_FAT_ERR			(0x1ULL << 58)
#define CE_ADM_ERR_PORT2_PCIE_COR_ERR			(0x1ULL << 59)
#define CE_ADM_ERR_PORT2_PCIE_NFAT_ERR			(0x1ULL << 60)
#define CE_ADM_ERR_PORT2_PCIE_FAT_ERR			(0x1ULL << 61)

#define FLUSH_SEL_PORT1_PIPE0_SHFT	0
#define FLUSH_SEL_PORT1_PIPE1_SHFT	4
#define FLUSH_SEL_PORT1_PIPE2_SHFT	8
#define FLUSH_SEL_PORT1_PIPE3_SHFT	12
#define FLUSH_SEL_PORT2_PIPE0_SHFT	16
#define FLUSH_SEL_PORT2_PIPE1_SHFT	20
#define FLUSH_SEL_PORT2_PIPE2_SHFT	24
#define FLUSH_SEL_PORT2_PIPE3_SHFT	28

#define CE_DRE_RO_ENABLE		(0x1ULL << 0)
#define CE_DRE_DYN_RO_ENABLE		(0x1ULL << 1)
#define CE_DRE_SUP_CONFIG_COMP_ERROR	(0x1ULL << 2)
#define CE_DRE_SUP_IO_COMP_ERROR	(0x1ULL << 3)
#define CE_DRE_ADDR_MODE_SHFT		4

#define CE_DRE_LAST_CONFIG_COMPLETION	(0x7ULL << 0)
#define CE_DRE_DOWNSTREAM_CONFIG_ERROR	(0x1ULL << 3)
#define CE_DRE_CONFIG_COMPLETION_VALID	(0x1ULL << 4)
#define CE_DRE_CONFIG_REQUEST_ACTIVE	(0x1ULL << 5)

#define CE_URE_RD_MRG_ENABLE		(0x1ULL << 0)
#define CE_URE_WRT_MRG_ENABLE1		(0x1ULL << 4)
#define CE_URE_WRT_MRG_ENABLE2		(0x1ULL << 5)
#define CE_URE_WRT_MRG_TIMER_SHFT	12
#define CE_URE_WRT_MRG_TIMER_MASK	(0x7FFULL << CE_URE_WRT_MRG_TIMER_SHFT)
#define CE_URE_WRT_MRG_TIMER(x)		(((u64)(x) << \
					  CE_URE_WRT_MRG_TIMER_SHFT) & \
					 CE_URE_WRT_MRG_TIMER_MASK)
#define CE_URE_RSPQ_BYPASS_DISABLE	(0x1ULL << 24)
#define CE_URE_UPS_DAT1_PAR_DISABLE	(0x1ULL << 32)
#define CE_URE_UPS_HDR1_PAR_DISABLE	(0x1ULL << 33)
#define CE_URE_UPS_DAT2_PAR_DISABLE	(0x1ULL << 34)
#define CE_URE_UPS_HDR2_PAR_DISABLE	(0x1ULL << 35)
#define CE_URE_ATE_PAR_DISABLE		(0x1ULL << 36)
#define CE_URE_RCI_PAR_DISABLE		(0x1ULL << 37)
#define CE_URE_RSPQ_PAR_DISABLE		(0x1ULL << 38)
#define CE_URE_DNS_DAT_PAR_DISABLE	(0x1ULL << 39)
#define CE_URE_DNS_HDR_PAR_DISABLE	(0x1ULL << 40)
#define CE_URE_MALFORM_DISABLE		(0x1ULL << 44)
#define CE_URE_UNSUP_DISABLE		(0x1ULL << 45)

#define CE_URE_ATE3240_ENABLE		(0x1ULL << 0)
#define CE_URE_ATE40_ENABLE 		(0x1ULL << 1)
#define CE_URE_PAGESIZE_SHFT		4
#define CE_URE_PAGESIZE_MASK		(0x7ULL << CE_URE_PAGESIZE_SHFT)
#define CE_URE_4K_PAGESIZE		(0x0ULL << CE_URE_PAGESIZE_SHFT)
#define CE_URE_16K_PAGESIZE		(0x1ULL << CE_URE_PAGESIZE_SHFT)
#define CE_URE_64K_PAGESIZE		(0x2ULL << CE_URE_PAGESIZE_SHFT)
#define CE_URE_128K_PAGESIZE		(0x3ULL << CE_URE_PAGESIZE_SHFT)
#define CE_URE_256K_PAGESIZE		(0x4ULL << CE_URE_PAGESIZE_SHFT)

#define PKT_TRAFIC_SHRT			16
#define BUS_SRC_ID_SHFT			8
#define DEV_SRC_ID_SHFT			3
#define FNC_SRC_ID_SHFT			0
#define CE_URE_TC_MASK			(0x07ULL << PKT_TRAFIC_SHRT)
#define CE_URE_BUS_MASK			(0xFFULL << BUS_SRC_ID_SHFT)
#define CE_URE_DEV_MASK			(0x1FULL << DEV_SRC_ID_SHFT)
#define CE_URE_FNC_MASK			(0x07ULL << FNC_SRC_ID_SHFT)
#define CE_URE_PIPE_BUS(b)		(((u64)(b) << BUS_SRC_ID_SHFT) & \
					 CE_URE_BUS_MASK)
#define CE_URE_PIPE_DEV(d)		(((u64)(d) << DEV_SRC_ID_SHFT) & \
					 CE_URE_DEV_MASK)
#define CE_URE_PIPE_FNC(f)		(((u64)(f) << FNC_SRC_ID_SHFT) & \
					 CE_URE_FNC_MASK)

#define CE_URE_SEL1_SHFT		0
#define CE_URE_SEL2_SHFT		20
#define CE_URE_SEL3_SHFT		40
#define CE_URE_SEL1_MASK		(0x7FFFFULL << CE_URE_SEL1_SHFT)
#define CE_URE_SEL2_MASK		(0x7FFFFULL << CE_URE_SEL2_SHFT)
#define CE_URE_SEL3_MASK		(0x7FFFFULL << CE_URE_SEL3_SHFT)


#define CE_URE_MASK1_SHFT		0
#define CE_URE_MASK2_SHFT		20
#define CE_URE_MASK3_SHFT		40
#define CE_URE_MASK1_MASK		(0x7FFFFULL << CE_URE_MASK1_SHFT)
#define CE_URE_MASK2_MASK		(0x7FFFFULL << CE_URE_MASK2_SHFT)
#define CE_URE_MASK3_MASK		(0x7FFFFULL << CE_URE_MASK3_SHFT)


#define CE_URE_SI			(0x1ULL << 0)
#define CE_URE_ELAL_SHFT		4
#define CE_URE_ELAL_MASK		(0x7ULL << CE_URE_ELAL_SHFT)
#define CE_URE_ELAL_SET(n)		(((u64)(n) << CE_URE_ELAL_SHFT) & \
					 CE_URE_ELAL_MASK)
#define CE_URE_ELAL1_SHFT		8
#define CE_URE_ELAL1_MASK		(0x7ULL << CE_URE_ELAL1_SHFT)
#define CE_URE_ELAL1_SET(n)		(((u64)(n) << CE_URE_ELAL1_SHFT) & \
					 CE_URE_ELAL1_MASK)
#define CE_URE_SCC			(0x1ULL << 12)
#define CE_URE_PN1_SHFT			16
#define CE_URE_PN1_MASK			(0xFFULL << CE_URE_PN1_SHFT)
#define CE_URE_PN2_SHFT			24
#define CE_URE_PN2_MASK			(0xFFULL << CE_URE_PN2_SHFT)
#define CE_URE_PN1_SET(n)		(((u64)(n) << CE_URE_PN1_SHFT) & \
					 CE_URE_PN1_MASK)
#define CE_URE_PN2_SET(n)		(((u64)(n) << CE_URE_PN2_SHFT) & \
					 CE_URE_PN2_MASK)

#define CE_URE_ABP			(0x1ULL << 0)
#define CE_URE_PCP			(0x1ULL << 1)
#define CE_URE_MSP			(0x1ULL << 2)
#define CE_URE_AIP			(0x1ULL << 3)
#define CE_URE_PIP			(0x1ULL << 4)
#define CE_URE_HPS			(0x1ULL << 5)
#define CE_URE_HPC			(0x1ULL << 6)
#define CE_URE_SPLV_SHFT		7
#define CE_URE_SPLV_MASK		(0xFFULL << CE_URE_SPLV_SHFT)
#define CE_URE_SPLV_SET(n)		(((u64)(n) << CE_URE_SPLV_SHFT) & \
					 CE_URE_SPLV_MASK)
#define CE_URE_SPLS_SHFT		15
#define CE_URE_SPLS_MASK		(0x3ULL << CE_URE_SPLS_SHFT)
#define CE_URE_SPLS_SET(n)		(((u64)(n) << CE_URE_SPLS_SHFT) & \
					 CE_URE_SPLS_MASK)
#define CE_URE_PSN1_SHFT		19
#define CE_URE_PSN1_MASK		(0x1FFFULL << CE_URE_PSN1_SHFT)
#define CE_URE_PSN2_SHFT		32
#define CE_URE_PSN2_MASK		(0x1FFFULL << CE_URE_PSN2_SHFT)
#define CE_URE_PSN1_SET(n)		(((u64)(n) << CE_URE_PSN1_SHFT) & \
					 CE_URE_PSN1_MASK)
#define CE_URE_PSN2_SET(n)		(((u64)(n) << CE_URE_PSN2_SHFT) & \
					 CE_URE_PSN2_MASK)


#define CE_PIO_MMR			0x00000000
#define CE_PIO_MMR_LEN			0x04000000

#define CE_PIO_CONFIG_SPACE		0x04000000
#define CE_PIO_CONFIG_SPACE_LEN		0x04000000

#define CE_PIO_IO_SPACE_ALIAS		0x08000000
#define CE_PIO_IO_SPACE_ALIAS_LEN	0x08000000

#define CE_PIO_E_CONFIG_SPACE		0x10000000
#define CE_PIO_E_CONFIG_SPACE_LEN	0x10000000

#define CE_PIO_IO_SPACE			0x100000000
#define CE_PIO_IO_SPACE_LEN		0x100000000

#define CE_PIO_MEM_SPACE		0x200000000
#define CE_PIO_MEM_SPACE_LEN		TIO_HWIN_SIZE


#define CE_E_CONFIG_BUS_SHFT		20
#define CE_E_CONFIG_BUS_MASK		(0xFF << CE_E_CONFIG_BUS_SHFT)
#define CE_E_CONFIG_DEVICE_SHFT		15
#define CE_E_CONFIG_DEVICE_MASK		(0x1F << CE_E_CONFIG_DEVICE_SHFT)
#define CE_E_CONFIG_FUNC_SHFT		12
#define CE_E_CONFIG_FUNC_MASK		(0x7  << CE_E_CONFIG_FUNC_SHFT)

#endif 
