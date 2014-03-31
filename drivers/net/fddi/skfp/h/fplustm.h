/******************************************************************************
 *
 *	(C)Copyright 1998,1999 SysKonnect,
 *	a business unit of Schneider & Koch & Co. Datensysteme GmbH.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	The information in this file is provided "AS IS" without warranty.
 *
 ******************************************************************************/


#ifndef	_FPLUS_
#define _FPLUS_

#ifndef	HW_PTR
#define	HW_PTR	void __iomem *
#endif

struct err_st {
	u_long err_valid ;		
	u_long err_abort ;		
	u_long err_e_indicator ;	
	u_long err_crc ;		
	u_long err_llc_frame ;		
	u_long err_mac_frame ;		
	u_long err_smt_frame ;		
	u_long err_imp_frame ;		
	u_long err_no_buf ;		
	u_long err_too_long ;		
	u_long err_bec_stat ;		
	u_long err_clm_stat ;		
	u_long err_sifg_det ;		
	u_long err_phinv ;		
	u_long err_tkiss ;		
	u_long err_tkerr ;		
} ;

struct s_smt_fp_txd {
	__le32 txd_tbctrl ;		
	__le32 txd_txdscr ;		
	__le32 txd_tbadr ;		
	__le32 txd_ntdadr ;		
#ifdef	ENA_64BIT_SUP
	__le32 txd_tbadr_hi ;		
#endif
	char far *txd_virt ;		
					
	struct s_smt_fp_txd volatile far *txd_next ;
	struct s_txd_os txd_os ;	
} ;

struct s_smt_fp_rxd {
	__le32 rxd_rbctrl ;		
	__le32 rxd_rfsw ;		
	__le32 rxd_rbadr ;		
	__le32 rxd_nrdadr ;		
#ifdef	ENA_64BIT_SUP
	__le32 rxd_rbadr_hi ;		
#endif
	char far *rxd_virt ;		
					
	struct s_smt_fp_rxd volatile far *rxd_next ;
	struct s_rxd_os rxd_os ;	
} ;

union s_fp_descr {
	struct	s_smt_fp_txd t ;		
	struct	s_smt_fp_rxd r ;		
} ;

struct s_smt_tx_queue {
	struct s_smt_fp_txd volatile *tx_curr_put ; 
	struct s_smt_fp_txd volatile *tx_prev_put ; 
	struct s_smt_fp_txd volatile *tx_curr_get ; 
	u_short tx_free ;			
	u_short tx_used ;			
	HW_PTR tx_bmu_ctl ;			
	HW_PTR tx_bmu_dsc ;			
} ;

struct s_smt_rx_queue {
	struct s_smt_fp_rxd volatile *rx_curr_put ; 
	struct s_smt_fp_rxd volatile *rx_prev_put ; 
	struct s_smt_fp_rxd volatile *rx_curr_get ; 
	u_short rx_free ;			
	u_short rx_used ;			
	HW_PTR rx_bmu_ctl ;			
	HW_PTR rx_bmu_dsc ;			
} ;

#define VOID_FRAME_OFF		0x00
#define CLAIM_FRAME_OFF		0x08
#define BEACON_FRAME_OFF	0x10
#define DBEACON_FRAME_OFF	0x18
#define RX_FIFO_OFF		0x21		
						

#define RBC_MEM_SIZE		0x8000
#define SEND_ASYNC_AS_SYNC	0x1
#define	SYNC_TRAFFIC_ON		0x2

#define	RX_FIFO_SPACE		0x4000 - RX_FIFO_OFF
#define	TX_FIFO_SPACE		0x4000

#define	TX_SMALL_FIFO		0x0900
#define	TX_MEDIUM_FIFO		TX_FIFO_SPACE / 2	
#define	TX_LARGE_FIFO		TX_FIFO_SPACE - TX_SMALL_FIFO	

#define	RX_SMALL_FIFO		0x0900
#define	RX_LARGE_FIFO		RX_FIFO_SPACE - RX_SMALL_FIFO	

struct s_smt_fifo_conf {
	u_short	rbc_ram_start ;		
	u_short	rbc_ram_end ;		
	u_short	rx1_fifo_start ;	
	u_short	rx1_fifo_size ;		
	u_short	rx2_fifo_start ;	
	u_short	rx2_fifo_size ;		
	u_short	tx_s_start ;		
	u_short	tx_s_size ;		
	u_short	tx_a0_start ;		
	u_short	tx_a0_size ;		
	u_short	fifo_config_mode ;	
} ;

#define FM_ADDRX	(FM_ADDET|FM_EXGPA0|FM_EXGPA1)

struct s_smt_fp {
	u_short	mdr2init ;		
	u_short	mdr3init ;		
	u_short frselreg_init ;		
	u_short	rx_mode ;		
	u_short	nsa_mode ;
	u_short rx_prom ;
	u_short	exgpa ;

	struct err_st err_stats ;	

	struct fddi_mac_sf {		
		u_char			mac_fc ;
		struct fddi_addr	mac_dest ;
		struct fddi_addr	mac_source ;
		u_char			mac_info[0x20] ;
	} mac_sfb ;


#define QUEUE_S			0
#define QUEUE_A0		1
#define QUEUE_R1		0
#define QUEUE_R2		1
#define USED_QUEUES		2

	struct s_smt_tx_queue *tx[USED_QUEUES] ;
	struct s_smt_rx_queue *rx[USED_QUEUES] ;

	struct s_smt_tx_queue tx_q[USED_QUEUES] ;
	struct s_smt_rx_queue rx_q[USED_QUEUES] ;

	struct	s_smt_fifo_conf	fifo ;

	
	u_short	 s2u ;
	u_short	 s2l ;

	
	HW_PTR	fm_st1u ;
	HW_PTR	fm_st1l ;
	HW_PTR	fm_st2u ;
	HW_PTR	fm_st2l ;
	HW_PTR	fm_st3u ;
	HW_PTR	fm_st3l ;


#define FPMAX_MULTICAST 32 
#define	SMT_MAX_MULTI	4
	struct {
		struct s_fpmc {
			struct fddi_addr	a ;	
			u_char			n ;	
			u_char			perm ;	
		} table[FPMAX_MULTICAST] ;
	} mc ;
	struct fddi_addr	group_addr ;
	u_long	func_addr ;		
	int	smt_slots_used ;	
	int	os_slots_used ;		 
					
} ;

#define RX_ENABLE_ALLMULTI	1	
#define RX_DISABLE_ALLMULTI	2	
#define RX_ENABLE_PROMISC	3	
#define RX_DISABLE_PROMISC	4	
#define RX_ENABLE_NSA		5	
#define RX_DISABLE_NSA		6	


#ifdef	AIX
#define MDR_REV
#define	AIX_REVERSE(x)		((((x)<<24L)&0xff000000L)	+	\
				 (((x)<< 8L)&0x00ff0000L)	+	\
				 (((x)>> 8L)&0x0000ff00L)	+	\
				 (((x)>>24L)&0x000000ffL))
#else
#ifndef AIX_REVERSE
#define	AIX_REVERSE(x)	(x)
#endif
#endif

#ifdef	MDR_REV	
#define	MDR_REVERSE(x)		((((x)<<24L)&0xff000000L)	+	\
				 (((x)<< 8L)&0x00ff0000L)	+	\
				 (((x)>> 8L)&0x0000ff00L)	+	\
				 (((x)>>24L)&0x000000ffL))
#else
#ifndef MDR_REVERSE
#define	MDR_REVERSE(x)	(x)
#endif
#endif

#endif
