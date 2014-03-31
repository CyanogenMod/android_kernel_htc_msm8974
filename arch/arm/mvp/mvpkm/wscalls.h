/*
 * Linux 2.6.32 and later Kernel module for VMware MVP Hypervisor Support
 *
 * Copyright (C) 2010-2013 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#line 5


#ifndef _WSCALLS_H
#define _WSCALLS_H

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

#define WSCALL_ACQUIRE_PAGE           1
#define WSCALL_FLUSH_ALL_DCACHES      2
#define WSCALL_IRQ                    3
#define WSCALL_ABORT                  4
#define WSCALL_LOG                    5
#define WSCALL_WAIT                   6
#define WSCALL_MUTEXLOCK              7
#define WSCALL_MUTEXUNLOCK            8
#define WSCALL_MUTEXUNLSLEEP          9
#define WSCALL_MUTEXUNLWAKE          10
#define WSCALL_GET_PAGE_FROM_VMID    11
#define WSCALL_REMOVE_PAGE_FROM_VMID 12
#define WSCALL_RELEASE_PAGE          13
#define WSCALL_READTOD               14
#define WSCALL_QP_GUEST_ATTACH       15
#define WSCALL_MONITOR_TIMER         16
#define WSCALL_COMM_SIGNAL           17
#define WSCALL_QP_NOTIFY             18

#define WSCALL_MAX_CALLNO            20

#define WSCALL_LOG_MAX 256

#define WSCALL_MAX_MPNS  16

#include "exitstatus.h"
#include "mutex.h"
#include "mksck_shared.h"
#include "qp.h"
#include "comm_transp.h"
#include "comm_transp_impl.h"

typedef struct WSParams {
	uint32 callno;
	union {
	struct {
		
		uint16 pages;

		uint16 order;

		
		PhysMem_RegionType forRegion;

		MPN mpns[WSCALL_MAX_MPNS];
	} pages;

	union {
		MPN mpn;          
		_Bool referenced; 
	} refCount;

	struct {
		ExitStatus   status; 
	} abort;

	struct {
		int level;
		char messg[WSCALL_LOG_MAX];
	} log;

	struct {
		HKVA mtxHKVA;           
		MutexMode mode;         
		uint32 cvi;             
		_Bool all;              
		_Bool ok;               
	} mutex;

	struct {
		Mksck_VmId  vmId;       

		_Bool found;

		
		MPN mpn[MKSCKPAGE_TOTAL];
	} pageMgmnt;

	struct {
		
		unsigned int now;
		
		unsigned int nowusec;
	} tod;

	struct {
		QPId id;             
		uint32 capacity;     
		uint32 type;         
		uint32 base;         
		uint32 nrPages;      
		int32 rc;            
	} qp;

	struct {
		CommTranspID transpID;
		CommTranspIOEvent event;
	} commEvent;

	struct {
		uint64 when64;       
	} timer;

	struct {
		_Bool suspendMode;   
	} wait;

	};                           
} WSParams;


static inline WSParams *UNUSED
WSP_Params(WorldSwitchPage *wsp_) {
	return (WSParams *)(wsp_->params_);
}

MY_ASSERTS(WSParFn,
	   ASSERT_ON_COMPILE(sizeof(WSParams) <= WSP_PARAMS_SIZE);
)
#endif
