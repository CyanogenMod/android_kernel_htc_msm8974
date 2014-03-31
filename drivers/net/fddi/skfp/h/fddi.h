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

#ifndef	_FDDI_
#define _FDDI_

struct fddi_addr {
	u_char	a[6] ;
} ;

#define GROUP_ADDR	0x80		

struct fddi_mac {
	struct fddi_addr	mac_dest ;
	struct fddi_addr	mac_source ;
	u_char			mac_info[4478] ;
} ;

#define FDDI_MAC_SIZE	(12)
#define FDDI_RAW_MTU	(4500-5)	
#define FDDI_RAW	(4500)

#define FC_VOID		0x40		
#define FC_TOKEN	0x80		
#define FC_RES_TOKEN	0xc0		
#define FC_SMT_INFO	0x41		
#define FC_SMT_LAN_LOC	0x42		
#define FC_SMT_LOC	0x43		
#define FC_SMT_NSA	0x4f		
#define FC_MAC		0xc0		
#define FC_BEACON	0xc2		
#define FC_CLAIM	0xc3		
#define FC_SYNC_LLC	0xd0		
#define FC_ASYNC_LLC	0x50		
#define FC_SYNC_BIT	0x80		

#define FC_LLC_PRIOR	0x07		

#define BEACON_INFO	0		
#define DBEACON_INFO	1		


#define C_INDICATOR	(1<<0)
#define A_INDICATOR	(1<<1)
#define E_INDICATOR	(1<<2)
#define I_INDICATOR	(1<<6)		 
#define L_INDICATOR	(1<<7)		

#endif	
