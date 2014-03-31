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

#ifndef	_TARGETHW_
#define _TARGETHW_

#ifdef	PCI
#define	RX_WATERMARK	24
#define TX_WATERMARK	24
#define SK_ML_ID_1	0x20
#define SK_ML_ID_2	0x30
#endif

#include	"skfbi.h"
#ifndef TAG_MODE	
#include	"fplus.h"
#else
#include	"fplustm.h"
#endif

#ifndef	HW_PTR
#define HW_PTR  void __iomem *
#endif

#ifdef MULT_OEM
#define	OI_STAT_LAST		0	
#define	OI_STAT_PRESENT		1	
#define	OI_STAT_VALID		2	 
#define	OI_STAT_ACTIVE		3	
					

struct	s_oem_ids {
	u_char	oi_status ;		
	u_char	oi_mark[5] ;		
	u_char 	oi_id[4] ;		
						
#ifdef PCI
	u_char 	oi_sub_id[4] ;		
					
#endif
} ;
#endif	


struct s_smt_hw {
	HW_PTR	iop ;			
	short	dma ;			
	short	irq ;			
	short	eprom ;			

#ifndef SYNC
	u_short	n_a_send ;		
#endif

#if	defined(PCI)
	short	slot ;			
	short   max_slots ;		
	short	wdog_used ;		
#endif

#ifdef	PCI
	u_short	pci_handle ;		
	u_long	is_imask ;		
	u_long	phys_mem_addr ;		
	u_short	mc_dummy ;			
	u_short hw_state ;		

#define	STARTED		1
#define	STOPPED		0

	int	hw_is_64bit ;		
#endif

#ifdef	TAG_MODE
	u_long	pci_fix_value ;		
#endif

	u_long	t_start ;		
	u_long	t_stop ;		
	u_short	timer_activ ;		

	u_char	pic_a1 ;
	u_char	pic_21 ;


	struct fddi_addr fddi_home_addr ;
	struct fddi_addr fddi_canon_addr ;
	struct fddi_addr fddi_phys_addr ;

	struct mac_parameter mac_pa ;	
	struct mac_counter mac_ct ;	
	u_short	mac_ring_is_up ;	

	struct s_smt_fp	fp ;		

#ifdef MULT_OEM
	struct s_oem_ids *oem_id ;	
	int oem_min_status ;		
#endif	

} ;
#endif
