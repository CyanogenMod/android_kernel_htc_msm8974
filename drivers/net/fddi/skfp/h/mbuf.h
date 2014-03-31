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

#ifndef	_MBUF_
#define _MBUF_

#define M_SIZE	4504

#ifndef MAX_MBUF
#define MAX_MBUF	4
#endif

#ifndef NO_STD_MBUF
#define sm_next         m_next
#define sm_off          m_off
#define sm_len          m_len
#define sm_data         m_data
#define SMbuf           Mbuf
#define mtod		smtod
#define mtodoff		smtodoff
#endif

struct s_mbuf {
	struct s_mbuf	*sm_next ;		
	short		sm_off ;			
	u_int		sm_len ;			
#ifdef	PCI
	int		sm_use_count ;
#endif
	char		sm_data[M_SIZE] ;
} ;

typedef struct s_mbuf SMbuf ;

#define	smtod(x,t)	((t)((x)->sm_data + (x)->sm_off))
#define	smtodoff(x,t,o)	((t)((x)->sm_data + (o)))

#endif	
