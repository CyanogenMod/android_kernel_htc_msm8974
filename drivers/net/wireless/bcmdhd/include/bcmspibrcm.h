/*
 * SD-SPI Protocol Conversion - BCMSDH->gSPI Translation Layer
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: bcmspibrcm.h 241182 2011-02-17 21:50:03Z $
 */
#ifndef	_BCM_SPI_BRCM_H
#define	_BCM_SPI_BRCM_H


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

#define BLOCK_SIZE_F1		64
#define BLOCK_SIZE_F2 		2048
#define BLOCK_SIZE_F3 		2048

#define SUCCESS	0
#undef ERROR
#define ERROR	1
#define ERROR_UF	2
#define ERROR_OF	3

#define SDIOH_MODE_SPI		0

#define USE_BLOCKMODE		0x2	
#define USE_MULTIBLOCK		0x4

struct sdioh_info {
	uint 		cfg_bar;		
	uint32		caps;			
	void		*bar0;			
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
						
	int 		adapter_slot;		
	int 		sd_mode;		
	int 		client_block_size[SPI_MAX_IOFUNCS];		
	uint32 		data_xfer_count;	
	uint16 		card_rca;		
	uint8 		num_funcs;		
	uint32 		card_dstatus;		
	uint32 		com_cis_ptr;
	uint32 		func_cis_ptr[SPI_MAX_IOFUNCS];
	void		*dma_buf;
	ulong		dma_phys;
	int 		r_cnt;			
	int 		t_cnt;			
	uint32		wordlen;			
	uint32		prev_fun;
	uint32		chip;
	uint32		chiprev;
	bool		resp_delay_all;
	bool		dwordmode;
	bool		resp_delay_new;

	struct spierrstats_t spierrstats;
};


extern uint sd_msglevel;


extern int spi_register_irq(sdioh_info_t *sd, uint irq);
extern void spi_free_irq(uint irq, sdioh_info_t *sd);

extern void spi_lock(sdioh_info_t *sd);
extern void spi_unlock(sdioh_info_t *sd);

extern int spi_osinit(sdioh_info_t *sd);
extern void spi_osfree(sdioh_info_t *sd);

#define SPI_RW_FLAG_M			BITFIELD_MASK(1)	
#define SPI_RW_FLAG_S			31
#define SPI_ACCESS_M			BITFIELD_MASK(1)	
#define SPI_ACCESS_S			30
#define SPI_FUNCTION_M			BITFIELD_MASK(2)	
#define SPI_FUNCTION_S			28
#define SPI_REG_ADDR_M			BITFIELD_MASK(17)	
#define SPI_REG_ADDR_S			11
#define SPI_LEN_M			BITFIELD_MASK(11)	
#define SPI_LEN_S			0

#endif 
