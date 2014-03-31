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


#ifndef _SYS_HV_INCLUDE_SYSCALL_PUBLIC_H
#define _SYS_HV_INCLUDE_SYSCALL_PUBLIC_H

#define HV_SYS_FAST_SHIFT                 14

#define HV_SYS_FAST_MASK                  (1 << HV_SYS_FAST_SHIFT)

#define HV_SYS_FAST_PLO_SHIFT             13

#define HV_SYS_FAST_PL0_MASK              (1 << HV_SYS_FAST_PLO_SHIFT)

#define HV_SYS_fence_incoherent         (51 | HV_SYS_FAST_MASK \
                                       | HV_SYS_FAST_PL0_MASK)

#endif 
