/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */


#ifndef _SYS_HV_DRV_PCIE_RC_INTF_H
#define _SYS_HV_DRV_PCIE_RC_INTF_H

#define PCIE_RC_CONFIG_MASK_OFF 0


typedef struct pcie_rc_config
{
  int intr;                     
  int plx_gen1;                 
} pcie_rc_config_t;

#endif  
