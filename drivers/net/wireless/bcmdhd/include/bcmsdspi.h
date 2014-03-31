/*
 * SD-SPI Protocol Conversion - BCMSDH->SPI Translation Layer
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
 * $Id: bcmsdspi.h 294363 2011-11-06 23:02:20Z $
 */
#ifndef	_BCM_SD_SPI_H
#define	_BCM_SD_SPI_H


#define sd_err(x)
#define sd_trace(x)
#define sd_info(x)
#define sd_debug(x)
#define sd_data(x)
#define sd_ctrl(x)

#define sd_log(x)

#define SDIOH_ASSERT(exp) \
	do { if (!(exp)) \
		printf("!!!ASSERT fail: file %s lines %d", __FILE__, __LINE__); \
	} while (0)

#define BLOCK_SIZE_4318 64
#define BLOCK_SIZE_4328 512

#define SUCCESS	0
#undef ERROR
#define ERROR	1

#define SDIOH_MODE_SPI		0

#define USE_BLOCKMODE		0x2	
#define USE_MULTIBLOCK		0x4

struct sdioh_info {
	uint cfg_bar;                   	
	uint32 caps;                    	
	uint		bar0;			
	osl_t 		*osh;			
	void		*controller;	

	uint		lockcount; 		
	bool		client_intr_enabled;	
	bool		intr_handler_valid;	
	sdioh_cb_fn_t	intr_handler;		
	void		*intr_handler_arg;	
	bool		initialized;		
	uint32		target_dev;		
	uint32		intmask;		
	void		*sdos_info;		

	uint32		controller_type;	
	uint8		version;		
	uint 		irq;			
	uint32 		intrcount;		
	uint32 		local_intrcount;	
	bool 		host_init_done;		
	bool 		card_init_done;		
	bool 		polled_mode;		

	bool		sd_use_dma;		
	bool 		sd_blockmode;		
						
	bool 		use_client_ints;	
	bool		got_hcint;		
						
	int 		adapter_slot;		
	int 		sd_mode;		
	int 		client_block_size[SDIOD_MAX_IOFUNCS];		
	uint32 		data_xfer_count;	
	uint32		cmd53_wr_data;		
	uint32		card_response;		
	uint32		card_rsp_data;		
	uint16 		card_rca;		
	uint8 		num_funcs;		
	uint32 		com_cis_ptr;
	uint32 		func_cis_ptr[SDIOD_MAX_IOFUNCS];
	void		*dma_buf;
	ulong		dma_phys;
	int 		r_cnt;			
	int 		t_cnt;			
};


extern uint sd_msglevel;


extern uint32 *spi_reg_map(osl_t *osh, uintptr addr, int size);
extern void spi_reg_unmap(osl_t *osh, uintptr addr, int size);

extern int spi_register_irq(sdioh_info_t *sd, uint irq);
extern void spi_free_irq(uint irq, sdioh_info_t *sd);

extern void spi_lock(sdioh_info_t *sd);
extern void spi_unlock(sdioh_info_t *sd);

extern int spi_osinit(sdioh_info_t *sd);
extern void spi_osfree(sdioh_info_t *sd);

#endif 
