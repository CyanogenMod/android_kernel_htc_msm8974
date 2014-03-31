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


#ifndef _MKSCK_SHARED_H
#define _MKSCK_SHARED_H

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_GPL
#include "include_check.h"

#define MKSCK_MAX_SHARES_LOG2  4 
#define MKSCK_MAX_SHARES       (1U << MKSCK_MAX_SHARES_LOG2)
#define MKSCK_VMID2IDX(idx)    ((idx)%MKSCK_MAX_SHARES)
#define MKSCK_TGID2VMID(tgid)  (((((tgid)<<1)^((tgid)>>15))&0xfffe)|1)
#define MKSCKPAGE_TOTAL        8 
#define MKSCKPAGE_SIZE         (PAGE_SIZE * MKSCKPAGE_TOTAL)
#define MKSCK_SOCKETS_PER_PAGE \
	((MKSCKPAGE_SIZE-offsetof(MksckPage, sockets[0])) / sizeof(Mksck))

#define MKSCK_ALIGNMENT        8 
#define MKSCK_ALIGN(x)         MVP_ALIGN(x, MKSCK_ALIGNMENT)
#define MKSCK_DGSIZE(len)      offsetof(Mksck_Datagram, data[MKSCK_ALIGN(len)])
#define MKSCK_BUFSIZE          MKSCK_DGSIZE(MKSCK_XFER_MAX + 1)

#define MKSCK_CVAR_ROOM        0 
#define MKSCK_CVAR_FILL        1 

#define MKSCK_FINDSENDROOM_FULL 0xFFFFFFFFU

#define MKSCK_SHUT_WR          (1 << 0)  
#define MKSCK_SHUT_RD          (1 << 1)  

typedef struct Mksck Mksck;
typedef struct Mksck_Datagram Mksck_Datagram;
typedef struct MksckPage MksckPage;

#include "atomic.h"
#include "mksck.h"
#include "mmu_defs.h"
#include "mutex.h"
#include "arm_inline.h"

struct Mksck_Datagram {
	Mksck_Address fromAddr;   
	uint32        len:16;     
	uint32        pad:3;      
				  
	uint32        pages:13;   
	uint8         data[1]     
		__attribute__((aligned(MKSCK_ALIGNMENT)));
};

struct Mksck {
	AtmUInt32 refCount;	
				
				
	Mksck_Address addr;	
				
				
	Mksck_Address peerAddr;	
				
	struct Mksck *peer;	
				
				
				
	uint32 index;		

				
				
				
				

	uint32 write;		
				
	uint32 read;		
				
	uint32 wrap;		
				
	uint32 shutDown;	
	uint32 foundEmpty;	
	uint32 foundFull;	
	Mutex mutex;		
	MVA rcvCBEntryMVA;	
	MVA rcvCBParamMVA;	
	uint8 buff[MKSCK_BUFSIZE] 
		__attribute__((aligned(MKSCK_ALIGNMENT)));
};


struct MksckPage {
	_Bool isGuest;		
	uint32 tgid;		
				
	volatile HKVA vmHKVA;	
	AtmUInt32 refCount;	
				
				
				
				
	uint32 wakeHostRecv;	
				
	AtmUInt32 wakeVMMRecv;	
	Mutex mutex;		
	Mksck_VmId vmId;	
	Mksck_Port portStore;	
	uint32 numAllocSocks;	
	Mksck sockets[1];	
};

MksckPage *MksckPage_GetFromVmId(Mksck_VmId vmId);
Mksck_Port MksckPage_GetFreePort(MksckPage *mksckPage, Mksck_Port port);
Mksck     *MksckPage_GetFromAddr(MksckPage *mksckPage, Mksck_Address addr);
Mksck     *MksckPage_AllocSocket(MksckPage *mksckPage, Mksck_Address addr);
void       MksckPage_DecRefc(MksckPage *mksckPage);

void       Mksck_DecRefc(Mksck *mksck);
void       Mksck_CloseCommon(Mksck *mksck);
_Bool      Mksck_IncReadIndex(Mksck *mksck, uint32 read, Mksck_Datagram *dg);
uint32     Mksck_FindSendRoom(Mksck *mksck, uint32 needed);
void       Mksck_IncWriteIndex(Mksck *mksck, uint32 write, uint32 needed);
void       Mksck_DisconnectPeer(Mksck *mksck);


static inline MksckPage *
Mksck_ToSharedPage(Mksck *mksck)
{
	return (MksckPage *)((char *)(mksck - mksck->index) -
			     offsetof(MksckPage, sockets));
}
#endif
