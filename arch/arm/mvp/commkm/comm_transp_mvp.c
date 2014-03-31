/*
 * Linux 2.6.32 and later Kernel module for VMware MVP Guest Communications
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

#include <linux/wait.h>

#include "comm_os.h"
#include "comm_transp_impl.h"

#include "mvp_types.h"
#include "qp.h"



struct CommTranspPriv {
	QPHandle *qp;
	CommTranspEvent event;
	unsigned int peerEvID;
	unsigned int writeSize;
	unsigned int readSize;
	uint32 backRef;
	CommOSWork work;
	CommOSAtomic raiseInline;
};


typedef struct TranspTableEntry {
	CommOSAtomic holds;
	CommTransp transp;
	CommOSWaitQueue wq;
} TranspTableEntry;

TranspTableEntry transpTable[QP_MAX_QUEUE_PAIRS];
static CommOSSpinlock_Define(transpTableLock);


static void
DestroyTransp(CommTransp transp)
{
	CommTranspID transpID;
	int32 rc;

	if (!transp) {
		CommOS_Debug(("Failed to close channel: Bad handle\n"));
		return;
	}

	CommOS_Log(("%s: Detaching channel [%u:%u]\n",
		    __func__,
		    transp->qp->id.context,
		    transp->qp->id.resource));

	transpID.d32[0] = transp->qp->id.context;
	transpID.d32[1] = transp->qp->id.resource;

#if !defined(COMM_BUILDING_SERVER)
	CommTranspEvent_Raise(transp->peerEvID,
			      &transpID,
			      COMM_TRANSP_IO_DETACH);
#endif

	rc = QP_Detach(transp->qp);

#if defined(COMM_BUILDING_SERVER)
	CommOS_WakeUp(&transpTable[transp->backRef].wq);
#endif

	CommOS_Kfree(transp);
	if (rc != QP_SUCCESS)
		CommOS_Log(("%s: Failed to detach. rc: %d\n", __func__, rc));
	else
		CommOS_Log(("%s: Channel detached.\n", __func__));
}



static void
TranspTableInit(void)
{
	uint32 i;

	CommOS_SpinLock(&transpTableLock);
	for (i = 0; i < QP_MAX_QUEUE_PAIRS; i++) {
		CommOS_WriteAtomic(&transpTable[i].holds, -1);
		transpTable[i].transp = NULL;
	}
	CommOS_SpinUnlock(&transpTableLock);
}



static inline int32
TranspTableAdd(CommTransp transp)
{
	uint32 i;

	if (!transp)
		return -1;

	CommOS_SpinLock(&transpTableLock);
	for (i = 0; i < QP_MAX_QUEUE_PAIRS; i++) {
		if ((transpTable[i].transp) == NULL) {
			transpTable[i].transp = transp;
			CommOS_WriteAtomic(&transpTable[i].holds, 1);
			CommOS_WaitQueueInit(&transpTable[i].wq);
			transp->backRef = i;
			break;
		}
	}
	CommOS_SpinUnlock(&transpTableLock);

	return 0;
}


static inline CommTransp
TranspTableGet(CommTranspID *id)
{
	CommTransp transp;
	uint32 i;

	if (!id)
		return NULL;

	for (i = 0; i < QP_MAX_QUEUE_PAIRS; i++) {
		transp = transpTable[i].transp;
		if (transp                                  &&
		    (transp->qp->id.context  == id->d32[0]) &&
		    (transp->qp->id.resource == id->d32[1])) {
			CommOS_AddReturnAtomic(&transpTable[i].holds, 1);
			return transp;
		}
	}

	CommOS_Debug(("%s: couldn't find transport object\n", __func__));
	return NULL;
}


static inline void
TranspTablePut(CommTransp transp)
{
	int32 holds;
	int32 backRef;

	if (!transp)
		return;

	backRef = transp->backRef;
	BUG_ON(backRef >= QP_MAX_QUEUE_PAIRS);

	holds = CommOS_SubReturnAtomic(&transpTable[backRef].holds, 1);
	if (holds > 0)
		return;

	BUG_ON(holds < 0);

	CommOS_SpinLock(&transpTableLock);
	CommOS_WriteAtomic(&transpTable[backRef].holds, -1);
	transpTable[backRef].transp = NULL;
	CommOS_SpinUnlock(&transpTableLock);
	DestroyTransp(transp);
}



static inline void
TranspTablePutNF(CommTransp transp)
{
	int32 holds;
	int32 backRef;

	if (!transp)
		return;

	backRef = transp->backRef;
	BUG_ON(backRef >= QP_MAX_QUEUE_PAIRS);

	holds = CommOS_SubReturnAtomic(&transpTable[backRef].holds, 1);
	BUG_ON(holds <= 0);
}



static void
RaiseEvent(CommOSWork *arg)
{
#if !defined(__linux__)
#error "RaiseEvent() is only supported on linux. Port 'container_of'!"
#endif
	CommTransp transp = container_of(arg, struct CommTranspPriv, work);
	CommTranspID transpID = {
		{
			.d32 = {
				[0] = transp->qp->id.context,
				[1] = transp->qp->id.resource
			}
		}
	};

	CommTranspEvent_Raise(transp->peerEvID,
			      &transpID,
			      COMM_TRANSP_IO_INOUT);
	TranspTablePut(transp);
}



unsigned int
CommTransp_RequestInlineEvents(CommTransp transp)
{
	unsigned int res = CommOS_AddReturnAtomic(&transp->raiseInline, 1);

	if (res == 1) {

		CommOS_AddReturnAtomic(&transpTable[transp->backRef].holds, 1);
		RaiseEvent(&transp->work);
	}
	return res;
}



unsigned int
CommTransp_ReleaseInlineEvents(CommTransp transp)
{
	return CommOS_SubReturnAtomic(&transp->raiseInline, 1);
}



#if defined(COMM_BUILDING_SERVER)

#define COMM_MAX_LISTENERS      QP_MAX_LISTENERS

static int32 NotifyCB(const QPInitArgs *args);
static void DetachCB(void *data);

static CommOSSpinlock_Define(listenersLock);
static CommTranspListener listeners[COMM_MAX_LISTENERS];
static uint32 numListeners;



static int32
NotifyCB(const QPInitArgs *args)
{
	CommTranspInitArgs transpArgs;
	uint32 i;
	int32 rc = -1;

	if (!args)
		return QP_ERROR_INVALID_ARGS;

	transpArgs.id.d32[0] = args->id.context;
	transpArgs.id.d32[1] = args->id.resource;
	transpArgs.capacity = args->capacity;
	transpArgs.type = args->type;

	CommOS_SpinLock(&listenersLock);
	for (i = 0; i < COMM_MAX_LISTENERS; i++) {
		if (listeners[i].probe &&
		    (listeners[i].probe(&transpArgs,
					listeners[i].probeData) == 0)) {
			CommOS_Debug(("%s: Delivered notify event to " \
				      "listener %u\n",
				      __func__, i));
			rc = 0;
			break;
		}
	}
	CommOS_SpinUnlock(&listenersLock);
	return rc;
}



static void
DetachCB(void *data)
{
	CommTransp transp = data;

	if (!transp || !(transp->event.ioEvent))
		return;

	CommOS_Debug(("%s: Guest detached from [%u:%u]\n",
		      __func__,
		      transp->qp->id.context,
		      transp->qp->id.resource));
	transp->event.ioEvent(transp, COMM_TRANSP_IO_DETACH,
			      transp->event.ioEventData);
}
#endif



int
CommTransp_Init(void)
{
	int32 rc;

	TranspTableInit();
	rc = CommTranspEvent_Init();

#if defined(COMM_BUILDING_SERVER)
	if (!rc)
		QP_RegisterListener(NotifyCB);
#endif
	return rc;
}



void
CommTransp_Exit(void)
{
	CommTranspEvent_Exit();
#if defined(COMM_BUILDING_SERVER)
	QP_UnregisterListener(NotifyCB);
#endif
}

#if defined(COMM_BUILDING_SERVER)


static int
DetachCondition(void *arg1, void *arg2)
{
	uint32 backRef = (uint32)arg1;

	return (CommOS_ReadAtomic(&transpTable[backRef].holds) == -1);
}
#endif



int
CommTranspEvent_Process(CommTranspID *id,
			CommTranspIOEvent event)
{
	int rc = 0;
	unsigned int delivered = 0;
	unsigned int backRef;
	int i = 0;

	CommTransp transp;
	uint32 raiseOnAllChannels = (id->d32[1] == COMM_TRANSP_ID_32_ANY);
	uint32 channels = raiseOnAllChannels ? QP_MAX_QUEUE_PAIRS : 1;

	while (channels--) {
		if (raiseOnAllChannels)
			id->d32[1] = i++;

		transp = TranspTableGet(id);
		if (transp) {
			if (transp->event.ioEvent)
				transp->event.ioEvent(transp, event,
						     transp->event.ioEventData);

			backRef = transp->backRef;
			TranspTablePut(transp);

#if defined(COMM_BUILDING_SERVER)
			if (event == COMM_TRANSP_IO_DETACH) {
				unsigned long long timeout = 30000;

				rc = CommOS_Wait(&transpTable[backRef].wq,
						 DetachCondition,
						 (void *)backRef,
						 NULL,
						 &timeout);
				switch (rc) {
				case 1:     
					rc = 0;
					break;
				default:    
					return -1;
				}
			}
#endif
			delivered++;
		}
	}

	rc = (delivered > 0) ? 0 : -1;
	return rc;
}



int
CommTransp_Register(const CommTranspListener *listener)
{
	int32 rc = -1;
#if defined(COMM_BUILDING_SERVER)
	uint32 i;

	if (!listener)
		return -1;

	CommOS_SpinLock(&listenersLock);
	for (i = 0; i < COMM_MAX_LISTENERS; i++) {
		if ((listeners[i].probe == NULL) &&
		    (listeners[i].probeData == NULL)) {
			listeners[i] = *listener;
			numListeners++;
			rc = 0;
			CommOS_Debug(("%s: Registered listener %u\n",
				      __func__, i));
			break;
		}
	}
	CommOS_SpinUnlock(&listenersLock);
#endif
	return rc;
}



void
CommTransp_Unregister(const CommTranspListener *listener)
{
#if defined(COMM_BUILDING_SERVER)
	uint32 i;

	if (!listener || !listener->probe)
		return;

	CommOS_SpinLock(&listenersLock);
	for (i = 0; i <  COMM_MAX_LISTENERS; i++) {
		if ((listeners[i].probe == listener->probe) &&
		    (listeners[i].probeData == listener->probeData)) {
			listeners[i].probe = NULL;
			listeners[i].probeData = NULL;
			numListeners--;
			CommOS_Debug(("%s: Unregistered listener %u\n",
				      __func__, i));
		}
	}
	CommOS_SpinUnlock(&listenersLock);
#endif
}



int
CommTransp_Open(CommTransp *transp,
		CommTranspInitArgs *transpArgs,
		CommTranspEvent *transpEvent)
{
	int32 rc = -1;
	QPHandle *qp = NULL;
	CommTransp transpOut = NULL;
	QPInitArgs qpInitArgs;

	if (!transp || !transpArgs)
		return -1;

	CommOS_Log(("%s: Attaching to [%u:%u]. Capacity: %u\n",
		    __func__, transpArgs->id.d32[1], transpArgs->id.d32[0],
		    transpArgs->capacity));

	qpInitArgs.id.context  = transpArgs->id.d32[0];
	qpInitArgs.id.resource = transpArgs->id.d32[1];
	qpInitArgs.capacity    = transpArgs->capacity;
	qpInitArgs.type        = transpArgs->type;

	transpOut = CommOS_Kmalloc(sizeof(*transpOut));
	if (!transpOut) {
		rc = -1;
		goto out;
	}

	rc = QP_Attach(&qpInitArgs, &qp);
	if (rc < 0) {
		rc = -1;
		goto out;
	}

	transpOut->qp = qp;

	transpArgs->id.d32[0] = qp->id.context;
	transpArgs->id.d32[1] = qp->id.resource;

	if (transpEvent) {
		transpOut->event = *transpEvent;
	} else {
		transpOut->event.ioEvent = NULL;
		transpOut->event.ioEventData = NULL;
	}

#if defined(COMM_BUILDING_SERVER)
	CommOS_Debug(("%s: Registering detach CB on id %u...\n",
		      __func__, transpArgs->id.d32[1]));
	QP_RegisterDetachCB(transpOut->qp, DetachCB, transpOut);
#endif

	transpOut->peerEvID = COMM_TRANSP_ID_32_ANY;
	transpOut->writeSize = 0;
	transpOut->readSize = 0;
	CommOS_InitWork(&transpOut->work, RaiseEvent);
	CommOS_WriteAtomic(&transpOut->raiseInline, 0);

	if (TranspTableAdd(transpOut)) {
		CommOS_Log(("%s: Exceeded max limit of transport objects!\n",
			    __func__));
		DestroyTransp(transpOut);
		rc = -1;
		goto out;
	}

	*transp = transpOut;
	rc = 0;

	CommOS_Log(("%s: Channel attached.\n", __func__));

out:
	if (rc && transpOut) {
		CommOS_Log(("%s: Failed to attach: %d\n", __func__, rc));
		CommOS_Kfree(transpOut);
	}

	return rc;
}



void
CommTransp_Close(CommTransp transp)
{
	if (!transp)
		return;

	CommOS_FlushAIOWork(&transp->work);
	TranspTablePut(transp);
}



int
CommTransp_EnqueueSpace(CommTransp transp)
{
	if (!transp)
		return -1;

	return QP_EnqueueSpace(transp->qp);
}



int
CommTransp_EnqueueReset(CommTransp transp)
{
	if (!transp)
		return -1;

	transp->writeSize = 0;
	return QP_EnqueueReset(transp->qp);
}



int
CommTransp_EnqueueSegment(CommTransp transp,
			  const void *buf,
			  unsigned int bufLen,
			  int kern)
{
	int rc;

	if (!transp)
		return -1;

	rc = QP_EnqueueSegment(transp->qp, (void *)buf, bufLen, kern);
	if (rc >= 0)
		transp->writeSize += (unsigned int)rc;
	else
		transp->writeSize = 0;

	return rc;
}



int
CommTransp_EnqueueCommit(CommTransp transp)
{
	int rc;

	if (!transp)
		return -1;

	rc = QP_EnqueueCommit(transp->qp);
	if (rc >= 0) {
		const unsigned int fudge = 4;
		int writable = CommTransp_EnqueueSpace(transp);

		if ((writable >= 0) &&
		    ((transp->writeSize + (unsigned int)writable + fudge) >=
		    transp->qp->queueSize)) {
			/*
			 * If bytes written since last commit + writable space
			 * 'almost' equal write queue size, then signal.
			 * The 'almost' fudge factor accounts for a possibly
			 * inaccurate CommTransp_EnqueueSpace() return value.
			 * Most of the time, this is inconsequential. In rare,
			 * borderline occasions, it results in a few extra
			 * signals.
			 * The scheme essentially means this: if this is the
			 * first packet to be write-committed, we signal.
			 * Otherwise, the remote end is supposed to keep
			 * going for as long as it can read.
			 */

			BUG_ON(transp->backRef >= QP_MAX_QUEUE_PAIRS);
			CommOS_AddReturnAtomic(
				&transpTable[transp->backRef].holds, 1);
			if (CommOS_ReadAtomic(&transp->raiseInline))
				RaiseEvent(&transp->work);
			else if (CommOS_ScheduleAIOWork(&transp->work))
				TranspTablePutNF(transp);
		}
		rc = 0;
	} else {
		rc = -1;
	}

	transp->writeSize = 0;
	return rc;
}



int
CommTransp_DequeueSpace(CommTransp transp)
{
	if (!transp)
		return -1;

	return QP_DequeueSpace(transp->qp);
}



int
CommTransp_DequeueReset(CommTransp transp)
{
	if (!transp)
		return -1;

	transp->readSize = 0;
	return QP_DequeueReset(transp->qp);
}



int
CommTransp_DequeueSegment(CommTransp transp,
			  void *buf,
			  unsigned bufLen,
			  int kern)
{
	int rc;

	if (!transp)
		return -1;

	rc = QP_DequeueSegment(transp->qp, buf, bufLen, kern);
	if (rc >= 0)
		transp->readSize += (unsigned int)rc;
	else
		transp->readSize = 0;

	return rc;
}



int
CommTransp_DequeueCommit(CommTransp transp)
{
	int rc;

	if (!transp)
		return -1;

	rc = QP_DequeueCommit(transp->qp);
	if (rc >= 0) {
		int readable = CommTransp_DequeueSpace(transp);
		const unsigned int limit = transp->qp->queueSize / 2;

		if ((readable >= 0) &&
		    (transp->readSize + (unsigned int)readable >= limit) &&
		    ((unsigned int)readable < limit)) {

			BUG_ON(transp->backRef >= QP_MAX_QUEUE_PAIRS);
			CommOS_AddReturnAtomic(
				&transpTable[transp->backRef].holds, 1);
			if (CommOS_ReadAtomic(&transp->raiseInline))
				RaiseEvent(&transp->work);
			else if (CommOS_ScheduleAIOWork(&transp->work))
				TranspTablePut(transp);
		}
		rc = 0;
	} else {
		rc = -1;
	}

	
	transp->readSize = 0;
	return rc;
}



int
CommTransp_Notify(const CommTranspID *notificationCenterID,
		  CommTranspInitArgs *transpArgs)
{
	QPInitArgs args;

	args.id.context = transpArgs->id.d32[0];
	args.id.resource = transpArgs->id.d32[1];
	args.capacity = transpArgs->capacity;
	args.type  = transpArgs->type;

	CommOS_Debug(("%s: d32[0]: %u d32[1]: %u\n",
		      __func__,
		      transpArgs->id.d32[0],
		      transpArgs->id.d32[1]));
	QP_Notify(&args);
	return 0;
}

