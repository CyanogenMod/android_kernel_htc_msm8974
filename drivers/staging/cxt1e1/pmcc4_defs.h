#ifndef _INC_PMCC4_DEFS_H_
#define _INC_PMCC4_DEFS_H_

/*-----------------------------------------------------------------------------
 * c4_defs.h -
 *
 *   Implementation elements of the wanPMC-C4T1E1 device driver
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


#define MAX_BOARDS          8
#define MAX_CHANS_USED      128

#ifdef  SBE_PMCC4_ENABLE
#define MUSYCC_NPORTS       4     
#endif
#ifdef SBE_WAN256T3_ENABLE
#define MUSYCC_NPORTS       8     
#endif
#define MUSYCC_NCHANS       32    

#define MUSYCC_NIQD         0x1000    
#define MUSYCC_MRU          2048  
#define MUSYCC_MTU          2048  
#define MUSYCC_TXDESC_MIN   10    
#define MUSYCC_RXDESC_MIN   18    
#define MUSYCC_TXDESC_TRANS 4     
#define MUSYCC_RXDESC_TRANS 12    

#define MAX_DEFAULT_IFQLEN  32    


#define SBE_IFACETMPL        "pmcc4-%d"
#ifdef IFNAMSIZ
#define SBE_IFACETMPL_SIZE    IFNAMSIZ
#else
#define SBE_IFACETMPL_SIZE    16
#endif

#define WATCHDOG_TIMEOUT      250000

#define WATCHDOG_UTIMEOUT     (WATCHDOG_TIMEOUT+300000)

#if !defined(SBE_ISR_TASKLET) && !defined(SBE_ISR_IMMEDIATE) && !defined(SBE_ISR_INLINE)
#define SBE_ISR_TASKLET
#endif

#endif   

