/*
 * Linux WiMax
 * API for user space
 *
 *
 * Copyright (C) 2007-2008 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Intel Corporation <linux-wimax@intel.com>
 * Inaky Perez-Gonzalez <inaky.perez-gonzalez@intel.com>
 *  - Initial implementation
 *
 *
 * This file declares the user/kernel protocol that is spoken over
 * Generic Netlink, as well as any type declaration that is to be used
 * by kernel and user space.
 *
 * It is intended for user space to clone it verbatim to use it as a
 * primary reference for definitions.
 *
 * Stuff intended for kernel usage as well as full protocol and stack
 * documentation is rooted in include/net/wimax.h.
 */

#ifndef __LINUX__WIMAX_H__
#define __LINUX__WIMAX_H__

#include <linux/types.h>

enum {
	WIMAX_GNL_VERSION = 01,
	
	WIMAX_GNL_ATTR_INVALID = 0x00,
	WIMAX_GNL_ATTR_MAX = 10,
};


enum {
	WIMAX_GNL_OP_MSG_FROM_USER,	
	WIMAX_GNL_OP_MSG_TO_USER,	
	WIMAX_GNL_OP_RFKILL,	
	WIMAX_GNL_OP_RESET,	
	WIMAX_GNL_RE_STATE_CHANGE,	
	WIMAX_GNL_OP_STATE_GET,		
};


enum {
	WIMAX_GNL_MSG_IFIDX = 1,
	WIMAX_GNL_MSG_PIPE_NAME,
	WIMAX_GNL_MSG_DATA,
};


enum wimax_rf_state {
	WIMAX_RF_OFF = 0,	
	WIMAX_RF_ON = 1,	
	WIMAX_RF_QUERY = 2,
};

enum {
	WIMAX_GNL_RFKILL_IFIDX = 1,
	WIMAX_GNL_RFKILL_STATE,
};


enum {
	WIMAX_GNL_RESET_IFIDX = 1,
};

enum {
	WIMAX_GNL_STGET_IFIDX = 1,
};

enum {
	WIMAX_GNL_STCH_IFIDX = 1,
	WIMAX_GNL_STCH_STATE_OLD,
	WIMAX_GNL_STCH_STATE_NEW,
};


 enum wimax_st {
	__WIMAX_ST_NULL = 0,
	WIMAX_ST_DOWN,
	__WIMAX_ST_QUIESCING,
	WIMAX_ST_UNINITIALIZED,
	WIMAX_ST_RADIO_OFF,
	WIMAX_ST_READY,
	WIMAX_ST_SCANNING,
	WIMAX_ST_CONNECTING,
	WIMAX_ST_CONNECTED,
	__WIMAX_ST_INVALID			
};


#endif 
