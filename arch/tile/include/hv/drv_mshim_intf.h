/*
 * Copyright 2011 Tilera Corporation. All Rights Reserved.
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


#ifndef _SYS_HV_INCLUDE_DRV_MSHIM_INTF_H
#define _SYS_HV_INCLUDE_DRV_MSHIM_INTF_H

#define TILE_MAX_MSHIMS 4

struct mshim_mem_info
{
  uint64_t mem_size;     
  uint8_t mem_type;      
  uint8_t mem_ecc;       
};

struct mshim_mem_error
{
  uint32_t sbe_count;     
};

#define MSHIM_MEM_INFO_OFF 0x100

#define MSHIM_MEM_ERROR_OFF 0x200

#endif 
