/* Copyright (C) 2005  SBE, Inc.
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
 */

#include <linux/types.h>
#include "pmcc4_sysdep.h"
#include "sbecom_inline_linux.h"
#include "libsbew.h"
#include "pmcc4_private.h"
#include "pmcc4.h"
#include "sbe_bid.h"

#ifdef SBE_INCLUDE_SYMBOLS
#define STATIC
#else
#define STATIC  static
#endif


char       *
sbeid_get_bdname (ci_t * ci)
{
    char       *np = 0;

    switch (ci->brd_id)
    {
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPTMC_256T3_E1):
        np = "wanPTMC-256T3 <E1>";
        break;
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPTMC_256T3_T1):
        np = "wanPTMC-256T3 <T1>";
        break;
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C4T1E1):
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C4T1E1_L):
        np = "wanPMC-C4T1E1";
        break;
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C2T1E1):
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C2T1E1_L):
        np = "wanPMC-C2T1E1";
        break;
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C1T1E1):
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C1T1E1_L):
        np = "wanPMC-C1T1E1";
        break;
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPCI_C4T1E1):
        np = "wanPCI-C4T1E1";
        break;
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPCI_C2T1E1):
        np = "wanPCI-C2T1E1";
        break;
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPCI_C1T1E1):
        np = "wanPCI-C1T1E1";
        break;
    default:
        
        np = "wanPCI-CxT1E1";
        break;
    }

    return np;
}



void
sbeid_set_hdwbid (ci_t * ci)
{

    switch (ci->brd_id)
    {
        case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPTMC_256T3_E1):
        ci->hdw_bid = SBE_BID_256T3_E1; 
        break;
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPTMC_256T3_T1):
        ci->hdw_bid = SBE_BID_256T3_T1; 
        break;
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C4T1E1):
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C4T1E1_L):
        switch (ci->max_port)
        {
        default:                    
        case 4:
            ci->hdw_bid = SBE_BID_PMC_C4T1E1;   
            break;
        case 2:
            ci->hdw_bid = SBE_BID_PMC_C2T1E1;   
            ci->brd_id = SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C2T1E1);
            break;
        case 1:
            ci->hdw_bid = SBE_BID_PMC_C1T1E1;   
            ci->brd_id = SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C1T1E1);
            break;
        }
        break;
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C2T1E1):
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C2T1E1_L):
        ci->hdw_bid = SBE_BID_PMC_C2T1E1;       
        break;
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C1T1E1):
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C1T1E1_L):
        ci->hdw_bid = SBE_BID_PMC_C1T1E1;       
        break;
#ifdef SBE_PMCC4_ENABLE
    case 0:
        
        ci->brd_id = SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPCI_C4T1E1);
        
#endif
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPCI_C4T1E1):
        switch (ci->max_port)
        {
        default:                    
        case 4:
            ci->hdw_bid = SBE_BID_PCI_C4T1E1;   
            break;
        case 2:
            ci->hdw_bid = SBE_BID_PCI_C2T1E1;   
            ci->brd_id = SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPCI_C2T1E1);
            break;
        case 1:
            ci->hdw_bid = SBE_BID_PCI_C1T1E1;   
            ci->brd_id = SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPCI_C1T1E1);
            break;
        }
        break;
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPCI_C2T1E1):
        ci->hdw_bid = SBE_BID_PCI_C2T1E1;       
        break;
    case SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPCI_C1T1E1):
        ci->hdw_bid = SBE_BID_PCI_C1T1E1;       
        break;
    default:
        
        ci->hdw_bid = SBE_BID_PMC_C4T1E1;       
        break;
    }
}


void
sbeid_set_bdtype (ci_t * ci)
{
    
    switch (ci->hdw_bid)
    {
        case SBE_BID_C1T3:      
        ci->brd_id = SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C1T3);
        break;
    case SBE_BID_C24TE1:            
        ci->brd_id = SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPTMC_C24TE1);
        break;
    case SBE_BID_256T3_E1:          
        ci->brd_id = SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPTMC_256T3_E1);
        break;
    case SBE_BID_256T3_T1:          
        ci->brd_id = SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPTMC_256T3_T1);
        break;
    case SBE_BID_PMC_C4T1E1:        
        ci->brd_id = SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C4T1E1);
        break;
    case SBE_BID_PMC_C2T1E1:        
        ci->brd_id = SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C2T1E1);
        break;
    case SBE_BID_PMC_C1T1E1:        
        ci->brd_id = SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPMC_C1T1E1);
        break;
    case SBE_BID_PCI_C4T1E1:        
        ci->brd_id = SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPCI_C4T1E1);
        break;
    case SBE_BID_PCI_C2T1E1:        
        ci->brd_id = SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPCI_C2T1E1);
        break;
    case SBE_BID_PCI_C1T1E1:        
        ci->brd_id = SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPCI_C1T1E1);
        break;

    default:
        
        ci->brd_id = SBE_BOARD_ID (PCI_VENDOR_ID_SBE, PCI_DEVICE_ID_WANPCI_C4T1E1);
        break;
    }
}


