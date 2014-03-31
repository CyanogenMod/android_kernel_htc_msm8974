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


#ifndef __NETIO_ERRORS_H__
#define __NETIO_ERRORS_H__


typedef enum
{
  
  NETIO_NO_ERROR        = 0,

  
  NETIO_PKT             = 0,

  
  NETIO_ERR_MAX         = -701,

  
  NETIO_NOT_REGISTERED  = -701,

  
  NETIO_NOPKT           = -702,

  
  NETIO_NOT_IMPLEMENTED = -703,

  NETIO_QUEUE_FULL      = -704,

  
  NETIO_BAD_AFFINITY    = -705,

  
  NETIO_CANNOT_HOME     = -706,

  NETIO_BAD_CONFIG      = -707,

  
  NETIO_TOOMANY_XMIT    = -708,

  NETIO_UNREG_XMIT      = -709,

  
  NETIO_ALREADY_REGISTERED = -710,

  
  NETIO_LINK_DOWN       = -711,

  NETIO_FAULT           = -712,

  
  NETIO_BAD_CACHE_CONFIG = -713,

  
  NETIO_ERR_MIN         = -713,

#ifndef __DOXYGEN__
  NETIO_NO_RESPONSE     = 1
#endif
} netio_error_t;


#endif 
