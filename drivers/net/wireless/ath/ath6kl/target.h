/*
 * Copyright (c) 2004-2010 Atheros Communications Inc.
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef TARGET_H
#define TARGET_H

#define AR6003_BOARD_DATA_SZ		1024
#define AR6003_BOARD_EXT_DATA_SZ	768
#define AR6003_BOARD_EXT_DATA_SZ_V2	1024

#define AR6004_BOARD_DATA_SZ     6144
#define AR6004_BOARD_EXT_DATA_SZ 0

#define RESET_CONTROL_ADDRESS		0x00000000
#define RESET_CONTROL_COLD_RST		0x00000100
#define RESET_CONTROL_MBOX_RST		0x00000004

#define CPU_CLOCK_STANDARD_S		0
#define CPU_CLOCK_STANDARD		0x00000003
#define CPU_CLOCK_ADDRESS		0x00000020

#define CLOCK_CONTROL_ADDRESS		0x00000028
#define CLOCK_CONTROL_LF_CLK32_S	2
#define CLOCK_CONTROL_LF_CLK32		0x00000004

#define SYSTEM_SLEEP_ADDRESS		0x000000c4
#define SYSTEM_SLEEP_DISABLE_S		0
#define SYSTEM_SLEEP_DISABLE		0x00000001

#define LPO_CAL_ADDRESS			0x000000e0
#define LPO_CAL_ENABLE_S		20
#define LPO_CAL_ENABLE			0x00100000

#define GPIO_PIN10_ADDRESS		0x00000050
#define GPIO_PIN11_ADDRESS		0x00000054
#define GPIO_PIN12_ADDRESS		0x00000058
#define GPIO_PIN13_ADDRESS		0x0000005c

#define HOST_INT_STATUS_ADDRESS		0x00000400
#define HOST_INT_STATUS_ERROR_S		7
#define HOST_INT_STATUS_ERROR		0x00000080

#define HOST_INT_STATUS_CPU_S		6
#define HOST_INT_STATUS_CPU		0x00000040

#define HOST_INT_STATUS_COUNTER_S	4
#define HOST_INT_STATUS_COUNTER		0x00000010

#define CPU_INT_STATUS_ADDRESS		0x00000401

#define ERROR_INT_STATUS_ADDRESS	0x00000402
#define ERROR_INT_STATUS_WAKEUP_S	2
#define ERROR_INT_STATUS_WAKEUP		0x00000004

#define ERROR_INT_STATUS_RX_UNDERFLOW_S	1
#define ERROR_INT_STATUS_RX_UNDERFLOW	0x00000002

#define ERROR_INT_STATUS_TX_OVERFLOW_S	0
#define ERROR_INT_STATUS_TX_OVERFLOW	0x00000001

#define COUNTER_INT_STATUS_ADDRESS	0x00000403
#define COUNTER_INT_STATUS_COUNTER_S	0
#define COUNTER_INT_STATUS_COUNTER	0x000000ff

#define RX_LOOKAHEAD_VALID_ADDRESS	0x00000405

#define INT_STATUS_ENABLE_ADDRESS	0x00000418
#define INT_STATUS_ENABLE_ERROR_S	7
#define INT_STATUS_ENABLE_ERROR		0x00000080

#define INT_STATUS_ENABLE_CPU_S		6
#define INT_STATUS_ENABLE_CPU		0x00000040

#define INT_STATUS_ENABLE_INT_S		5
#define INT_STATUS_ENABLE_INT		0x00000020
#define INT_STATUS_ENABLE_COUNTER_S	4
#define INT_STATUS_ENABLE_COUNTER	0x00000010

#define INT_STATUS_ENABLE_MBOX_DATA_S	0
#define INT_STATUS_ENABLE_MBOX_DATA	0x0000000f

#define CPU_INT_STATUS_ENABLE_ADDRESS	0x00000419
#define CPU_INT_STATUS_ENABLE_BIT_S	0
#define CPU_INT_STATUS_ENABLE_BIT	0x000000ff

#define ERROR_STATUS_ENABLE_ADDRESS		0x0000041a
#define ERROR_STATUS_ENABLE_RX_UNDERFLOW_S	1
#define ERROR_STATUS_ENABLE_RX_UNDERFLOW	0x00000002

#define ERROR_STATUS_ENABLE_TX_OVERFLOW_S	0
#define ERROR_STATUS_ENABLE_TX_OVERFLOW		0x00000001

#define COUNTER_INT_STATUS_ENABLE_ADDRESS	0x0000041b
#define COUNTER_INT_STATUS_ENABLE_BIT_S		0
#define COUNTER_INT_STATUS_ENABLE_BIT		0x000000ff

#define COUNT_ADDRESS			0x00000420

#define COUNT_DEC_ADDRESS		0x00000440

#define WINDOW_DATA_ADDRESS		0x00000474
#define WINDOW_WRITE_ADDR_ADDRESS	0x00000478
#define WINDOW_READ_ADDR_ADDRESS	0x0000047c
#define CPU_DBG_SEL_ADDRESS		0x00000483
#define CPU_DBG_ADDRESS			0x00000484

#define LOCAL_SCRATCH_ADDRESS		0x000000c0
#define ATH6KL_OPTION_SLEEP_DISABLE	0x08

#define RTC_BASE_ADDRESS		0x00004000
#define GPIO_BASE_ADDRESS		0x00014000
#define MBOX_BASE_ADDRESS		0x00018000
#define ANALOG_INTF_BASE_ADDRESS	0x0001c000

#define ATH6KL_ANALOG_PLL_REGISTER	(ANALOG_INTF_BASE_ADDRESS + 0x284)

#define SM(f, v)	(((v) << f##_S) & f)
#define MS(f, v)	(((v) & f) >> f##_S)

#define ATH6KL_AR6003_HI_START_ADDR           0x00540600
#define ATH6KL_AR6004_HI_START_ADDR           0x00400800

struct host_interest {
	u32 hi_app_host_interest;                      

	
	u32 hi_failure_state;                          

	
	u32 hi_dbglog_hdr;                             

	u32 hi_unused1;                       

	u32 hi_option_flag;                            

	u32 hi_serial_enable;                          

	
	u32 hi_dset_list_head;                         

	
	u32 hi_app_start;                              

	
	u32 hi_skip_clock_init;                        
	u32 hi_core_clock_setting;                     
	u32 hi_cpu_clock_setting;                      
	u32 hi_system_sleep_setting;                   
	u32 hi_xtal_control_setting;                   
	u32 hi_pll_ctrl_setting_24ghz;                 
	u32 hi_pll_ctrl_setting_5ghz;                  
	u32 hi_ref_voltage_trim_setting;               
	u32 hi_clock_info;                             

	u32 hi_bank0_addr_value;                       
	u32 hi_bank0_read_value;                       
	u32 hi_bank0_write_value;                      
	u32 hi_bank0_config_value;                     

	
	u32 hi_board_data;                             
	u32 hi_board_data_initialized;                 

	u32 hi_dset_ram_index_tbl;                     

	u32 hi_desired_baud_rate;                      
	u32 hi_dbglog_config;                          
	u32 hi_end_ram_reserve_sz;                     
	u32 hi_mbox_io_block_sz;                       

	u32 hi_num_bpatch_streams;                     
	u32 hi_mbox_isr_yield_limit;                   

	u32 hi_refclk_hz;                              
	u32 hi_ext_clk_detected;                       
	u32 hi_dbg_uart_txpin;                         
	u32 hi_dbg_uart_rxpin;                         
	u32 hi_hci_uart_baud;                          
	u32 hi_hci_uart_pin_assignments;               
	u32 hi_hci_uart_baud_scale_val;                
	u32 hi_hci_uart_baud_step_val;                 

	u32 hi_allocram_start;                         
	u32 hi_allocram_sz;                            
	u32 hi_hci_bridge_flags;                       
	u32 hi_hci_uart_support_pins;                  
	u32 hi_hci_uart_pwr_mgmt_params;               

	
	u32 hi_board_ext_data;                
	u32 hi_board_ext_data_config;         

	u32 hi_reset_flag;                            
	
	u32 hi_reset_flag_valid;                      
	u32 hi_hci_uart_pwr_mgmt_params_ext;           
	
	u32 hi_acs_flags;                              
	u32 hi_console_flags;                          
	u32 hi_nvram_state;                            
	u32 hi_option_flag2;                           

	
	u32 hi_sw_version_override;                    
	u32 hi_abi_version_override;                   

	u32 hi_hp_rx_traffic_ratio;                    

	
	u32 hi_test_apps_related    ;                  
	
	u32 hi_ota_testscript;                         
	
	u32 hi_cal_data;                               
	
	u32 hi_pktlog_num_buffers;                     

} __packed;

#define HI_ITEM(item)  offsetof(struct host_interest, item)

#define HI_OPTION_MAC_ADDR_METHOD_SHIFT	3

#define HI_OPTION_FW_MODE_IBSS    0x0
#define HI_OPTION_FW_MODE_BSS_STA 0x1
#define HI_OPTION_FW_MODE_AP      0x2

#define HI_OPTION_FW_SUBMODE_NONE      0x0
#define HI_OPTION_FW_SUBMODE_P2PDEV    0x1
#define HI_OPTION_FW_SUBMODE_P2PCLIENT 0x2
#define HI_OPTION_FW_SUBMODE_P2PGO     0x3

#define HI_OPTION_NUM_DEV_SHIFT   0x9

#define HI_OPTION_FW_BRIDGE_SHIFT 0x04

#define HI_OPTION_FW_MODE_BITS	       0x2
#define HI_OPTION_FW_MODE_SHIFT        0xC

#define HI_OPTION_FW_SUBMODE_BITS      0x2
#define HI_OPTION_FW_SUBMODE_SHIFT     0x14

#define AR6003_VTOP(vaddr) ((vaddr) & 0x001fffff)
#define AR6004_VTOP(vaddr) (vaddr)

#define TARG_VTOP(target_type, vaddr) \
	(((target_type) == TARGET_TYPE_AR6003) ? AR6003_VTOP(vaddr) : \
	(((target_type) == TARGET_TYPE_AR6004) ? AR6004_VTOP(vaddr) : 0))

#define ATH6KL_FWLOG_PAYLOAD_SIZE		1500

struct ath6kl_dbglog_buf {
	__le32 next;
	__le32 buffer_addr;
	__le32 bufsize;
	__le32 length;
	__le32 count;
	__le32 free;
} __packed;

struct ath6kl_dbglog_hdr {
	__le32 dbuf_addr;
	__le32 dropped;
} __packed;

#endif
