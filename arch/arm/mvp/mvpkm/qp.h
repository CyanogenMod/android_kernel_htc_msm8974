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


#ifndef _QP_H
#define _QP_H

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_PV
#define INCLUDE_ALLOW_GPL
#include "include_check.h"


typedef enum QPState {
	QP_STATE_FREE = 0x1,	
	QP_STATE_CONNECTED,	
	
	QP_STATE_GUEST_ATTACHED,
	QP_STATE_MAX		
} QPState;

typedef struct QPId {
	uint32 context;
	uint32 resource;
} QPId;

typedef struct QPInitArgs {
	QPId id;                  
	uint32 capacity;          
	uint32 type;              
} QPInitArgs;

typedef struct QHandle {
	volatile uint32 head;		
	volatile uint32 tail;		
	volatile uint32 phantom_head;	
	volatile uint32 phantom_tail;	
	uint8 data[0];			
					
} QHandle;

typedef struct QPHandle {
	QPId id;			
	uint32 capacity;		
	QHandle *produceQ;		
	QHandle *consumeQ;		
	uint32 queueSize;		
	uint32 type;			

	QPState state;
	void (*peerDetachCB)(void *data);
	void *detachData;		
	struct page **pages;		
} QPHandle;

#define QP_SUCCESS                    0
#define QP_ERROR_NO_MEM             (-1)
#define QP_ERROR_INVALID_HANDLE     (-2)
#define QP_ERROR_INVALID_ARGS       (-3)
#define QP_ERROR_ALREADY_ATTACHED   (-4)

#define QP_MIN_CAPACITY            (PAGE_SIZE * 2)
#define QP_MAX_CAPACITY            (1024*1024)                 
#define QP_MAX_QUEUE_PAIRS         32
#define QP_MAX_ID                  QP_MAX_QUEUE_PAIRS
#define QP_MAX_LISTENERS           QP_MAX_QUEUE_PAIRS
#define QP_MAX_PAGES               (QP_MAX_CAPACITY/PAGE_SIZE) 

#define QP_INVALID_ID              0xFFFFFFFF
#define QP_INVALID_SIZE            0xFFFFFFFF
#define QP_INVALID_REGION          0xFFFFFFFF
#define QP_INVALID_TYPE            0xFFFFFFFF

#ifdef __KERNEL__
static inline
_Bool QP_CheckArgs(QPInitArgs *args)
{
	if (!args ||
	    !is_power_of_2(args->capacity) ||
	    (args->capacity < QP_MIN_CAPACITY) ||
	    (args->capacity > QP_MAX_CAPACITY) ||
	    !(args->id.resource < QP_MAX_ID ||
	      args->id.resource == QP_INVALID_ID) ||
	    (args->type == QP_INVALID_TYPE))
		return false;
	else
		return true;
}
#endif


static inline
_Bool QP_CheckHandle(QPHandle *qp)
{
#ifdef MVP_DEBUG
	if (!(qp)                                            ||
	    !(qp->produceQ)                                  ||
	    !(qp->consumeQ)                                  ||
	    (qp->state >= (uint32)QP_STATE_MAX)             ||
	    !(qp->queueSize < (QP_MAX_CAPACITY/2)))
		return false;
	else
		return true;
#else
	return true;
#endif
}


static inline void
QP_MakeInvalidQPHandle(QPHandle *qp)
{
	if (!qp)
		return;

	qp->id.context       = QP_INVALID_ID;
	qp->id.resource      = QP_INVALID_ID;
	qp->capacity         = QP_INVALID_SIZE;
	qp->produceQ         = NULL;
	qp->consumeQ         = NULL;
	qp->queueSize        = QP_INVALID_SIZE;
	qp->type             = QP_INVALID_TYPE;
	qp->state            = QP_STATE_FREE;
	qp->peerDetachCB     = NULL;
	qp->detachData       = NULL;
}

typedef int32 (*QPListener)(const QPInitArgs *);
int32 QP_RegisterListener(const QPListener);
int32 QP_UnregisterListener(const QPListener);
int32 QP_RegisterDetachCB(QPHandle *qp, void (*callback)(void *), void *data);


int32 QP_Attach(QPInitArgs *args, QPHandle **qp);
int32 QP_Detach(QPHandle *qp);
int32 QP_Notify(QPInitArgs *args);

int32 QP_EnqueueSpace(QPHandle *qp);
int32 QP_EnqueueSegment(QPHandle *qp, const void *buf, size_t length, int kern);
int32 QP_EnqueueCommit(QPHandle *qp);
int32 QP_EnqueueReset(QPHandle *qp);

int32 QP_DequeueSpace(QPHandle *qp);
int32 QP_DequeueSegment(QPHandle *qp, void *buf, size_t length, int kern);
int32 QP_DequeueReset(QPHandle *qp);
int32 QP_DequeueCommit(QPHandle *qp);

#define MVP_QP_SIGNATURE 0x53525051                   
#define MVP_QP_ATTACH \
	(MVP_OBJECT_CUSTOM_BASE + 0) 
#define MVP_QP_DETACH \
	(MVP_OBJECT_CUSTOM_BASE + 1) 
#define MVP_QP_NOTIFY \
	(MVP_OBJECT_CUSTOM_BASE + 2) 
#define MVP_QP_LAST \
	(MVP_OBJECT_CUSTOM_BASE + 3) 

#ifdef QP_DEBUG
   #ifdef IN_MONITOR
      #define QP_DBG(...) Log(__VA_ARGS__)
   #else
      #define QP_DBG(...) pr_info(__VA_ARGS__)
   #endif
#else
   #define QP_DBG(...)
#endif

#endif

