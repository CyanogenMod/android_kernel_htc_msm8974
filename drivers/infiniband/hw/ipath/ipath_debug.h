/*
 * Copyright (c) 2006, 2007 QLogic Corporation. All rights reserved.
 * Copyright (c) 2003, 2004, 2005, 2006 PathScale, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _IPATH_DEBUG_H
#define _IPATH_DEBUG_H

#ifndef _IPATH_DEBUGGING	
#define _IPATH_DEBUGGING 1
#endif

#if _IPATH_DEBUGGING


#define __IPATH_INFO        0x1	
#define __IPATH_DBG         0x2	
#define __IPATH_TRSAMPLE    0x8	
#define __IPATH_VERBDBG     0x40	
#define __IPATH_PKTDBG      0x80	
#define __IPATH_PROCDBG     0x100
#define __IPATH_MMDBG       0x200
#define __IPATH_ERRPKTDBG   0x400
#define __IPATH_USER_SEND   0x1000	
#define __IPATH_KERNEL_SEND 0x2000	
#define __IPATH_EPKTDBG     0x4000	
#define __IPATH_IPATHDBG    0x10000	
#define __IPATH_IPATHWARN   0x20000	
#define __IPATH_IPATHERR    0x40000	
#define __IPATH_IPATHPD     0x80000	
#define __IPATH_IPATHTABLE  0x100000	
#define __IPATH_LINKVERBDBG 0x200000	

#else				


#define __IPATH_INFO      0x0	
#define __IPATH_DBG       0x0	
#define __IPATH_TRSAMPLE  0x0	
#define __IPATH_VERBDBG   0x0	
#define __IPATH_PKTDBG    0x0	
#define __IPATH_PROCDBG   0x0	
#define __IPATH_MMDBG     0x0
#define __IPATH_EPKTDBG   0x0	
#define __IPATH_IPATHDBG  0x0	
#define __IPATH_IPATHWARN 0x0	
#define __IPATH_IPATHERR  0x0	
#define __IPATH_IPATHPD   0x0	
#define __IPATH_IPATHTABLE 0x0	
#define __IPATH_LINKVERBDBG 0x0	

#endif				

#define __IPATH_VERBOSEDBG __IPATH_VERBDBG

#endif				
