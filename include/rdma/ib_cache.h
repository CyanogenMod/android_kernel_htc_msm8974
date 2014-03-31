/*
 * Copyright (c) 2004 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005 Intel Corporation. All rights reserved.
 * Copyright (c) 2005 Sun Microsystems, Inc. All rights reserved.
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

#ifndef _IB_CACHE_H
#define _IB_CACHE_H

#include <rdma/ib_verbs.h>

int ib_get_cached_gid(struct ib_device    *device,
		      u8                   port_num,
		      int                  index,
		      union ib_gid        *gid);

int ib_find_cached_gid(struct ib_device *device,
		       union ib_gid	*gid,
		       u8               *port_num,
		       u16              *index);

int ib_get_cached_pkey(struct ib_device    *device_handle,
		       u8                   port_num,
		       int                  index,
		       u16                 *pkey);

int ib_find_cached_pkey(struct ib_device    *device,
			u8                   port_num,
			u16                  pkey,
			u16                 *index);

int ib_get_cached_lmc(struct ib_device *device,
		      u8                port_num,
		      u8                *lmc);

#endif 
