/*
 *  'Standard' SDIO HOST CONTROLLER driver
 *
 * Copyright (C) 1999-2012, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: bcmsdstd.h 324797 2012-03-30 11:02:00Z $
 */
#ifndef	_BCM_SD_STD_H
#define	_BCM_SD_STD_H

#define sd_err(x)	do { if (sd_msglevel & SDH_ERROR_VAL) printf x; } while (0)
#define sd_trace(x)
#define sd_info(x)
#define sd_debug(x)
#define sd_data(x)
#define sd_ctrl(x)
#define sd_dma(x)

#define sd_sync_dma(sd, read, nbytes)
#define sd_init_dma(sd)
#define sd_ack_intr(sd)
#define sd_wakeup(sd);
extern int sdstd_osinit(sdioh_info_t *sd);
extern void sdstd_osfree(sdioh_info_t *sd);

#define sd_log(x)

#define SDIOH_ASSERT(exp) \
	do { if (!(exp)) \
		printf("!!!ASSERT fail: file %s lines %d", __FILE__, __LINE__); \
	} while (0)

#define BLOCK_SIZE_4318 64
#define BLOCK_SIZE_4328 512

#define SUCCESS	0
#define ERROR	1

#define SDIOH_MODE_SPI		0
#define SDIOH_MODE_SD1		1
#define SDIOH_MODE_SD4		2

#define MAX_SLOTS 6 	
#define SDIOH_REG_WINSZ	0x100 

#define SDIOH_TYPE_ARASAN_HDK	1
#define SDIOH_TYPE_BCM27XX	2
#define SDIOH_TYPE_TI_PCIXX21	4	
#define SDIOH_TYPE_RICOH_R5C822	5	
#define SDIOH_TYPE_JMICRON	6	

#define BCMSDYIELD

#define SDIOH_CMD7_EXP_STATUS   0x00001E00

#define RETRIES_LARGE 100000
#define sdstd_os_yield(sd)	do {} while (0)
#define RETRIES_SMALL 100


#define USE_BLOCKMODE		0x2	
#define USE_MULTIBLOCK		0x4

#define USE_FIFO		0x8	

#define CLIENT_INTR 		0x100	

#define HC_INTR_RETUNING	0x1000


struct sdioh_info {
	uint cfg_bar;                   	
	uint32 caps;                    	
	uint32 curr_caps;                    	

	osl_t 		*osh;			
	volatile char 	*mem_space;		
	uint		lockcount; 		
	bool		client_intr_enabled;	
	bool		intr_handler_valid;	
	sdioh_cb_fn_t	intr_handler;		
	void		*intr_handler_arg;	
	bool		initialized;		
	uint		target_dev;		
	uint16		intmask;		
	void		*sdos_info;		

	uint32		controller_type;	
	uint8		version;		
	uint 		irq;			
	int 		intrcount;		
	int 		local_intrcount;	
	bool 		host_init_done;		
	bool 		card_init_done;		
	bool 		polled_mode;		

	bool 		sd_blockmode;		
						
	bool 		use_client_ints;	
						
	int 		adapter_slot;		
	int 		sd_mode;		
	int 		client_block_size[SDIOD_MAX_IOFUNCS];		
	uint32 		data_xfer_count;	
	uint16 		card_rca;		
	int8		sd_dma_mode;		
	uint8 		num_funcs;		
	uint32 		com_cis_ptr;
	uint32 		func_cis_ptr[SDIOD_MAX_IOFUNCS];
	void		*dma_buf;		
	ulong		dma_phys;		
	void		*adma2_dscr_buf;	
	ulong		adma2_dscr_phys;	

	
	void		*dma_start_buf;
	ulong		dma_start_phys;
	uint		alloced_dma_size;
	void		*adma2_dscr_start_buf;
	ulong		adma2_dscr_start_phys;
	uint		alloced_adma2_dscr_size;

	int 		r_cnt;			
	int 		t_cnt;			
	bool		got_hcint;		
	uint16		last_intrstatus;	
	int 	host_UHSISupported;		
	int 	card_UHSI_voltage_Supported; 	
	int	global_UHSI_Supp;	
	volatile int	sd3_dat_state; 		
	volatile int	sd3_tun_state; 		
	bool	sd3_tuning_reqd; 	
	uint32	caps3;			
};

#define DMA_MODE_NONE	0
#define DMA_MODE_SDMA	1
#define DMA_MODE_ADMA1	2
#define DMA_MODE_ADMA2	3
#define DMA_MODE_ADMA2_64 4
#define DMA_MODE_AUTO	-1

#define USE_DMA(sd)		((bool)((sd->sd_dma_mode > 0) ? TRUE : FALSE))

#define TUNING_IDLE 			0
#define TUNING_START 			1
#define TUNING_START_AFTER_DAT 	2
#define TUNING_ONGOING 			3

#define DATA_TRANSFER_IDLE 		0
#define DATA_TRANSFER_ONGOING	1

#define CHECK_TUNING_PRE_DATA	1
#define CHECK_TUNING_POST_DATA	2


extern uint sd_msglevel;

extern bool check_client_intr(sdioh_info_t *sd);

extern void sdstd_devintr_on(sdioh_info_t *sd);
extern void sdstd_devintr_off(sdioh_info_t *sd);

extern void sdstd_intrs_on(sdioh_info_t *sd, uint16 norm, uint16 err);
extern void sdstd_intrs_off(sdioh_info_t *sd, uint16 norm, uint16 err);

extern void sdstd_spinbits(sdioh_info_t *sd, uint16 norm, uint16 err);



extern uint32 *sdstd_reg_map(osl_t *osh, int32 addr, int size);
extern void sdstd_reg_unmap(osl_t *osh, int32 addr, int size);

extern int sdstd_register_irq(sdioh_info_t *sd, uint irq);
extern void sdstd_free_irq(uint irq, sdioh_info_t *sd);

extern void sdstd_lock(sdioh_info_t *sd);
extern void sdstd_unlock(sdioh_info_t *sd);
extern void sdstd_waitlockfree(sdioh_info_t *sd);

extern int sdstd_waitbits(sdioh_info_t *sd, uint16 norm, uint16 err, bool yield, uint16 *bits);

extern void sdstd_3_enable_retuning_int(sdioh_info_t *sd);
extern void sdstd_3_disable_retuning_int(sdioh_info_t *sd);
extern bool sdstd_3_is_retuning_int_set(sdioh_info_t *sd);
extern void sdstd_3_check_and_do_tuning(sdioh_info_t *sd, int tuning_param);
extern bool sdstd_3_check_and_set_retuning(sdioh_info_t *sd);
extern int sdstd_3_get_tune_state(sdioh_info_t *sd);
extern int sdstd_3_get_data_state(sdioh_info_t *sd);
extern void sdstd_3_set_tune_state(sdioh_info_t *sd, int state);
extern void sdstd_3_set_data_state(sdioh_info_t *sd, int state);
extern uint8 sdstd_3_get_tuning_exp(sdioh_info_t *sd);
extern uint32 sdstd_3_get_uhsi_clkmode(sdioh_info_t *sd);
extern int sdstd_3_clk_tuning(sdioh_info_t *sd, uint32 sd3ClkMode);

extern void sdstd_3_start_tuning(sdioh_info_t *sd);
extern void sdstd_3_osinit_tuning(sdioh_info_t *sd);
extern void sdstd_3_osclean_tuning(sdioh_info_t *sd);

#endif 
