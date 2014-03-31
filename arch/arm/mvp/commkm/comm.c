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


#include "comm.h"
#include "comm_transp_impl.h"



#if defined(COMM_INSTRUMENT)
static CommOSAtomic commMaxCoalesceSize;
static CommOSAtomic commPacketsReceived;
static CommOSAtomic commCommittedPacketsReceived;
static CommOSAtomic commOpCalls;
#endif

#define COMM_DISPATCH_EXTRA_WRITER_WAKEUP 1

#define COMM_CHANNEL_MAX_CAPACITY 2048
#define COMM_CHANNEL_FREE        0x0
#define COMM_CHANNEL_INITIALIZED 0x1
#define COMM_CHANNEL_OPENED      0x2
#define COMM_CHANNEL_ACTIVE      0x4
#define COMM_CHANNEL_ZOMBIE      0x8

#define CommIsFree(chan)        ((chan)->lifecycleState == COMM_CHANNEL_FREE)
#define CommIsInitialized(chan) \
		((chan)->lifecycleState == COMM_CHANNEL_INITIALIZED)
#define CommIsOpened(chan)      ((chan)->lifecycleState == COMM_CHANNEL_OPENED)
#define CommIsActive(chan)      ((chan)->lifecycleState == COMM_CHANNEL_ACTIVE)
#define CommIsZombie(chan)      ((chan)->lifecycleState == COMM_CHANNEL_ZOMBIE)

#define CommSetFree(chan)        SetLifecycleState(chan, COMM_CHANNEL_FREE)
#define CommSetInitialized(chan) \
		SetLifecycleState(chan, COMM_CHANNEL_INITIALIZED)
#define CommSetOpened(chan)      SetLifecycleState(chan, COMM_CHANNEL_OPENED)
#define CommSetActive(chan)      SetLifecycleState(chan, COMM_CHANNEL_ACTIVE)
#define CommSetZombie(chan)      SetLifecycleState(chan, COMM_CHANNEL_ZOMBIE)

#define CommGlobalLock() CommOS_SpinLock(&commGlobalLock)
#define CommGlobalUnlock() CommOS_SpinUnlock(&commGlobalLock)
#define CommGlobalLockBH() CommOS_SpinLockBH(&commGlobalLock)
#define CommGlobalUnlockBH() CommOS_SpinUnlockBH(&commGlobalLock)

#define DispatchTrylock(chan) CommOS_MutexTrylock(&(chan)->dispatchMutex)
#define DispatchUnlock(chan) CommOS_MutexUnlock(&(chan)->dispatchMutex)

#define WriteLock(chan) CommOS_MutexLock(&(chan)->writeMutex)
#define WriteTrylock(chan) CommOS_MutexTrylock(&(chan)->writeMutex)
#define WriteUnlock(chan) CommOS_MutexUnlock(&(chan)->writeMutex)

#define StateLock(chan) CommOS_MutexLock(&(chan)->stateMutex)
#define StateTrylock(chan) CommOS_MutexTrylock(&(chan)->stateMutex)
#define StateUnlock(chan) CommOS_MutexUnlock(&(chan)->stateMutex)

#define CommHoldInit(chan) CommOS_WriteAtomic(&(chan)->holds, 0)
#define CommHold(chan) CommOS_AddReturnAtomic(&(chan)->holds, 1)
#define CommRelease(chan) CommOS_SubReturnAtomic(&(chan)->holds, 1)
#define CommIsHeld(chan) (CommOS_ReadAtomic(&(chan)->holds) > 0)

#define PacketLenOverLimit(chan, len) \
	(((len) - sizeof(CommPacket)) > ((chan)->transpArgs.capacity / 4))



struct CommChannelPriv {
	CommOSAtomic holds;             
	CommTranspInitArgs transpArgs;  
	CommTransp transp;              
	CommOSMutex dispatchMutex;      
	CommOSMutex writeMutex;         
	CommOSMutex stateMutex;         
	CommOSWaitQueue availableWaitQ; 
	unsigned int desiredWriteSpace; 
	const CommImpl *impl;           
	unsigned int implNmbOps;        
	unsigned int lifecycleState;    
	void *state;                    
};


static int running;                     
static CommOSWaitQueue exitWaitQ;       
static CommOSSpinlock commGlobalLock;   



static unsigned int commChannelCapacity;     
static unsigned int commChannelSize;         
static unsigned int commChannelAllocated;    
static struct CommChannelPriv *commChannels; 



static int
DefaultTranspListener(CommTranspInitArgs *transpArgs,
		      void *probeData)
{
	int rc = -1;
	const int inBH = 1;
	const CommImpl *impl;

	if (!transpArgs || !probeData) {
		CommOS_Debug(("%s: NULL args [0x%p, 0x%p].\n",
			      __func__, transpArgs, probeData));
		goto out;
	}

	impl = probeData;
	CommOS_Debug(("%s: Received attach info [%u,%u,%u:%u].\n",
		      __func__,
		      transpArgs->capacity, transpArgs->type,
		      transpArgs->id.d32[0], transpArgs->id.d32[1]));

	if (impl->checkArgs(transpArgs))
		goto out;

	transpArgs->mode = COMM_TRANSP_INIT_ATTACH; 


	rc = 0;
	if (Comm_Alloc(transpArgs, impl, inBH, NULL)) {
		impl->closeNtf(impl->closeNtfData, transpArgs, inBH);
		CommOS_Log(("%s: Can't allocate new channel!\n", __func__));
	}

out:
	return rc;
}



static inline void
SetLifecycleState(CommChannel channel,
		  unsigned int newState)
{

	channel->lifecycleState = newState;
}




static int
ExitCondition(void *arg1,
	      void *arg2)
{
	unsigned int i;
	int rc;

	(void)arg1;
	(void)arg2;
	CommOS_Debug(("%s: running [%d] commChannelAllocated [%u] " \
		      "commChannelSize [%u].\n",
		      __func__, running,
		      commChannelAllocated, commChannelSize));

	rc = !running && (commChannelAllocated == 0);
	if (!rc) {
		for (i = 0; i < commChannelCapacity; i++)
			CommOS_Debug(("%s: channel[%u] state [0x%x].\n",
				      __func__, i,
				      commChannels[i].lifecycleState));
	}
	return rc;
}



static int
WriteSpaceCondition(void *arg1,
		    void *arg2)
{
	CommChannel channel = arg1;

	if (!CommIsActive(channel))
		return -ENOMEM;

	return channel->desiredWriteSpace <
	       CommTransp_EnqueueSpace(channel->transp);
}



int
Comm_RegisterImpl(const CommImpl *impl)
{
	CommTranspListener listener = {
		.probe = DefaultTranspListener,
		.probeData = (void *)impl
	};

	return CommTransp_Register(&listener);
}



void
Comm_UnregisterImpl(const CommImpl *impl)
{
	CommTranspListener listener = {
		.probe = DefaultTranspListener,
		.probeData = (void *)impl
	};

	CommTransp_Unregister(&listener);
}



int
Comm_Init(unsigned int maxChannels)
{
	int rc = -1;
	unsigned int i;

	if (running || commChannels ||
	    (maxChannels == 0) || (maxChannels > COMM_CHANNEL_MAX_CAPACITY))
		goto out;

#if defined(COMM_INSTRUMENT)
	CommOS_WriteAtomic(&commMaxCoalesceSize, 0);
	CommOS_WriteAtomic(&commPacketsReceived, 0);
	CommOS_WriteAtomic(&commCommittedPacketsReceived, 0);
	CommOS_WriteAtomic(&commOpCalls, 0);
#endif

	CommOS_WaitQueueInit(&exitWaitQ);
	CommOS_SpinlockInit(&commGlobalLock);
	commChannelCapacity = maxChannels;
	commChannelAllocated = 0;
	commChannels =
		CommOS_Kmalloc((sizeof(*commChannels)) * commChannelCapacity);
	if (!commChannels)
		goto out;

	memset(commChannels, 0, (sizeof(*commChannels)) * commChannelCapacity);
	for (i = 0; i < commChannelCapacity; i++) {
		CommChannel channel;

		channel = &commChannels[i];
		CommHoldInit(channel);
		channel->transp = NULL;
		CommOS_MutexInit(&channel->dispatchMutex);
		CommOS_MutexInit(&channel->writeMutex);
		CommOS_MutexInit(&channel->stateMutex);
		CommOS_WaitQueueInit(&channel->availableWaitQ);
		channel->desiredWriteSpace = -1U;
		channel->state = NULL;
		CommSetFree(channel);
	}

	rc = CommTransp_Init();
	if (!rc) {
		commChannelSize = 0;
		running = 1;
		rc = 0;
	} else {
		CommOS_Kfree(commChannels);
	}

out:
	return rc;
}



int
Comm_Finish(unsigned long long *timeoutMillis)
{
	int rc;
	unsigned int i;
	unsigned long long timeout;

	for (i = 0; i < commChannelSize; i++)
		Comm_Zombify(&commChannels[i], 0);

	running = 0;
	timeout = timeoutMillis ? *timeoutMillis : 0;
	
	rc = CommOS_Wait(&exitWaitQ, ExitCondition, NULL, NULL, &timeout);
	if (rc == 1) {

		CommTransp_Exit();
		CommOS_Kfree(commChannels);
		commChannels = NULL;
		commChannelSize = 0;
#if defined(COMM_INSTRUMENT)
		CommOS_Log(("%s: commMaxCoalesceSize = %lu.\n",
			    __func__,
			    CommOS_ReadAtomic(&commMaxCoalesceSize)));
		CommOS_Log(("%s: commPacketsReceived = %lu.\n",
			    __func__,
			    CommOS_ReadAtomic(&commPacketsReceived)));
		CommOS_Log(("%s: commCommittedPacketsReceived = %lu.\n",
			    __func__,
			    CommOS_ReadAtomic(&commCommittedPacketsReceived)));
		CommOS_Log(("%s: commOpCalls = %lu.\n",
			    __func__,
			    CommOS_ReadAtomic)(&commOpCalls)));
#endif
		rc = 0;
	} else {
		rc = -1;
	}
	return rc;
}



int
Comm_Alloc(const CommTranspInitArgs *transpArgs,
	   const CommImpl *impl,
	   int inBH,
	   CommChannel *newChannel)
{
	unsigned int i;
	CommChannel channel = NULL;
	int restoreSize = 0;
	int modHeld = 0;
	int rc = -1;

	if (inBH)
		CommGlobalLock();
	else
		CommGlobalLockBH();

	if (!running || !transpArgs || !impl)
		goto out;

	if (CommOS_ModuleGet(impl->owner))
		goto out;

	modHeld = 1;

	for (i = 0; i < commChannelSize; i++) {

		if (!CommIsFree(&commChannels[i]) &&
		    (transpArgs->id.d64 != COMM_TRANSP_ID_64_ANY) &&
		    (transpArgs->id.d64 == commChannels[i].transpArgs.id.d64))
			goto out;

		if (!channel && CommIsFree(&commChannels[i]))
			channel = &commChannels[i];
	}

	if (!channel) {
		if (commChannelSize == commChannelCapacity)
			goto out;

		channel = &commChannels[commChannelSize];
		commChannelSize++;
		restoreSize = 1;
	}

	if (channel->transp) { 
		if (restoreSize)
			commChannelSize--;
		goto out;
	}

	channel->transpArgs = *transpArgs;
	channel->impl = impl;
	for (i = 0; impl->operations[i]; i++)
		;
	channel->implNmbOps = i;
	channel->desiredWriteSpace = -1U;
	commChannelAllocated++;
	CommSetInitialized(channel);

	if (newChannel)
		*newChannel = channel;

	rc = 0;
	CommOS_ScheduleDisp();

out:
	if (inBH)
		CommGlobalUnlock();
	else
		CommGlobalUnlockBH();

	if (rc && modHeld)
		CommOS_ModulePut(impl->owner);

	return rc;
}



int
Comm_Zombify(CommChannel channel,
	     int inBH)
{
	int rc = -1;

	if (!running)
		goto out;

	if (inBH)
		CommGlobalLock();
	else
		CommGlobalLockBH();

	if (CommIsActive(channel) || CommIsOpened(channel)) {
		CommSetZombie(channel);
		rc = 0;
	}

	if (inBH)
		CommGlobalUnlock();
	else
		CommGlobalUnlockBH();

out:
	if (!rc)
		CommOS_ScheduleDisp();
	return rc;
}



int
Comm_IsActive(CommChannel channel)
{
	return channel ? CommIsActive(channel) : 0;
}



static inline void
WakeUpWriter(CommChannel channel)
{
	if (WriteSpaceCondition(channel, NULL))
		CommOS_WakeUp(&channel->availableWaitQ);
}



static void
TranspEventHandler(CommTransp transp,
		   CommTranspIOEvent event,
		   void *data)
{
	CommChannel channel = (CommChannel)data;

	switch (event) {
	case COMM_TRANSP_IO_DETACH:
		CommOS_Debug(("%s: Detach event. Zombifying channel.\n",
			      __func__));
		Comm_Zombify(channel, 1);
		break;

	case COMM_TRANSP_IO_IN:
	case COMM_TRANSP_IO_INOUT:

		CommOS_ScheduleDisp();
		break;

	case COMM_TRANSP_IO_OUT:
		CommHold(channel);
		if (CommIsActive(channel))
			WakeUpWriter(channel);

		CommRelease(channel);

		if (CommIsZombie(channel))
			CommOS_ScheduleDisp();
		break;

	default:
		CommOS_Debug(("%s: Unhandled event [%u, %p, %p].\n",
			      __func__, event, transp, data));
	}
}



static void
CommClose(CommChannel channel)
{
	const CommImpl *impl = channel->impl;

	StateLock(channel);
	if (impl->stateDtor && channel->state)
		impl->stateDtor(channel->state);

	channel->state = NULL;
	StateUnlock(channel);

	CommOS_ModulePut(impl->owner);

	if (channel->transp) {
		CommTransp_Close(channel->transp);
		channel->transp = NULL;
	}

	CommGlobalLockBH();
	CommSetFree(channel);
	commChannelAllocated--;
	if (channel == &commChannels[commChannelSize - 1])
		commChannelSize--;

	CommGlobalUnlockBH();
	if (!running && (commChannelAllocated == 0))
		CommOS_WakeUp(&exitWaitQ);
}



static int
CommOpen(CommChannel channel)
{
	int rc = -1;
	CommTranspEvent transpEvent = {
		.ioEvent = TranspEventHandler,
		.ioEventData = channel
	};
	const CommImpl *impl;

	if (!channel || !CommIsInitialized(channel))
		return rc;

	if (!running) 
		goto out;

	impl = channel->impl;
	if (impl->stateCtor) {
		channel->state = impl->stateCtor(channel);
		if (!channel->state)
			goto out;
	}

	if (!CommTransp_Open(&channel->transp,
			     &channel->transpArgs, &transpEvent))
		rc = 0;
	else
		channel->transp = NULL;

out:
	if (!rc)
		CommSetOpened(channel);
	else
		CommClose(channel);

	return rc;
}



CommTranspInitArgs
Comm_GetTranspInitArgs(CommChannel channel)
{
	if (!channel) {
		CommTranspInitArgs res = { .capacity = 0 };

		return res;
	}
	return channel->transpArgs;
}



void *
Comm_GetState(CommChannel channel)
{
	if (!channel)
		return NULL;

	return channel->state;
}



static int
CopyFromChannel(CommChannel channel,
		void *dest,
		unsigned int size,
		int kern)
{
	return CommTransp_DequeueSegment(channel->transp, dest, size, kern);
}



int
Comm_Dispatch(CommChannel channel)
{
	int rc = 0;
	int zombify = 0;
	CommPacket packet;
	CommPacket firstPacket;
	unsigned int dataLen;
#define VEC_SIZE 32
	struct kvec vec[VEC_SIZE];
	unsigned int vecLen;


	if (DispatchTrylock(channel))
		return 0;

	

	if (CommIsActive(channel)) {

		

		WakeUpWriter(channel);

		
		CommTransp_DequeueReset(channel->transp);

		for (vecLen = 0; vecLen < VEC_SIZE; vecLen++) {
			if (!running)
				break;

			

			rc = CommTransp_DequeueSegment(channel->transp, &packet,
						       sizeof(packet), 1);
			if (rc <= 0) {
				

				rc = vecLen == 0 ? 0 : 1;
				break;
			}
#if defined(COMM_INSTRUMENT)
			CommOS_AddReturnAtomic(commPacketsReceived, 1);
#endif
			if ((rc != sizeof(packet)) ||
			    (packet.len < sizeof(packet))) {
				rc = -1; 
				break;
			}

			rc = 1;

			

			dataLen = packet.len - sizeof(packet);
			if (vecLen == 0) {
				

				firstPacket = packet;
				if (dataLen == 0) {

					CommTransp_DequeueCommit(
						channel->transp);
#if defined(COMM_INSTRUMENT)
					CommOS_AddReturnAtomic(
						&commCommittedPacketsReceived,
						1);
#endif
					break;
				}
			} else {

				if (memcmp(&packet.opCode, &firstPacket.opCode,
					   (sizeof(packet) -
					    offsetof(CommPacket, opCode))) ||
				    PacketLenOverLimit(
						channel,
						firstPacket.len + dataLen))
					break;
			}

			if (dataLen == 0) {

				vec[vecLen].iov_base = NULL;
				goto dequeueCommit;
			}


			vec[vecLen].iov_base =
				channel->impl->dataAlloc(dataLen, channel,
							 &packet,
							 CopyFromChannel);
			if (!vec[vecLen].iov_base) {
				CommOS_Log(("%s: BOOG -- COULD NOT " \
					    "DEQUEUE PAYLOAD! [%d != %u]",
					    __func__, rc, dataLen));
				rc = -1; 
				break;
			}
			rc = 1;

dequeueCommit:
			CommTransp_DequeueCommit(channel->transp);
#if defined(COMM_INSTRUMENT)
			CommOS_AddReturnAtomic(&commCommittedPacketsReceived,
					       1);
#endif
			vec[vecLen].iov_len = dataLen;
			if (vecLen > 0) {
				firstPacket.len += dataLen;

				if (packet.flags)
					firstPacket.flags = packet.flags;
			}
#if defined(COMM_INSTRUMENT)
			if (firstPacket.len >
			    CommOS_ReadAtomic(&commMaxCoalesceSize))
				CommOS_WriteAtomic(&commMaxCoalesceSize,
						   firstPacket.len);
#endif
			if (COMM_OPF_TEST_ERR(packet.flags)) {

				vecLen++;
				break;
			}
		}

		if (rc <= 0) {
			if (rc < 0) {
				zombify = 1;
				rc = 1;
			}
			goto outUnlockAndFreeIovec;
		}

#if defined(COMM_DISPATCH_EXTRA_WRITER_WAKEUP)
		

		WakeUpWriter(channel);
#endif

		if (firstPacket.opCode >= channel->implNmbOps) {
			CommOS_Debug(("%s: Ignoring illegal opCode [%u]!\n",
				      __func__,
				      (unsigned int)firstPacket.opCode));
			CommOS_Debug(("%s: Max opCode: %u\n",
				      __func__, channel->implNmbOps));
			goto outUnlockAndFreeIovec;
		}


#if defined(COMM_INSTRUMENT)
		CommOS_AddReturnAtomic(&commOpCalls, 1);
#endif

		CommHold(channel);
		channel->impl->operations[firstPacket.opCode](
			channel, channel->state, &firstPacket, vec, vecLen);
		CommRelease(channel);
		goto out; 
	}

	

	if (CommIsZombie(channel) && !CommIsHeld(channel)) {
		CommTranspInitArgs transpArgs = channel->transpArgs;
		void (*closeNtf)(void *,
				 const CommTranspInitArgs *,
				 int inBH) = channel->impl->closeNtf;
		void *closeNtfData = channel->impl->closeNtfData;

		while (WriteTrylock(channel)) {
			

			CommOS_Debug(("%s: Kicking writers out.\n", __func__));
			CommOS_WakeUp(&channel->availableWaitQ);
		}
		WriteUnlock(channel);

		CommOS_Debug(("%s: Nuking zombie channel.\n", __func__));
		CommClose(channel);
		if (closeNtf)
			closeNtf(closeNtfData, &transpArgs, 0);

		rc = -1;
	} else if (CommIsInitialized(channel) &&
		   (channel->impl->openAtMillis <= CommOS_GetCurrentMillis())) {
		if (!CommOpen(channel)) {
			if (channel->transpArgs.mode ==
			    COMM_TRANSP_INIT_CREATE) {

				CommTransp_Notify(&channel->impl->ntfCenterID,
						  &channel->transpArgs);
			} else { 
				packet.len = sizeof(packet);
				packet.opCode = 0xff;
				packet.flags = 0x00;


				CommTransp_EnqueueReset(channel->transp);
				rc = CommTransp_EnqueueSegment(
					channel->transp,
					&packet, sizeof(packet), 1);
				if ((rc == sizeof(packet)) &&
				   !CommTransp_EnqueueCommit(channel->transp)) {
					

					CommGlobalLockBH();
					if (CommIsOpened(channel)) {
						CommOS_Debug((
							"%s: Sent ack. "\
							"Activating channel.\n",
							__func__));
						CommSetActive(channel);
					}
					CommGlobalUnlockBH();
				}
			}
			rc = 1;
		}
	} else if (CommIsOpened(channel) &&
		   (channel->transpArgs.mode == COMM_TRANSP_INIT_CREATE)) {

		rc = CommTransp_DequeueSegment(channel->transp,
					       &packet, sizeof(packet), 1);
		if ((rc == sizeof(packet)) &&
		    !CommTransp_DequeueCommit(channel->transp)) {
			void (*activateNtf)(void *, CommChannel) = NULL;
			void *activateNtfData = NULL;

			

			CommGlobalLockBH();

			if (CommIsOpened(channel) &&
			    (packet.opCode == 0xff) && (packet.flags == 0x0)) {
				activateNtf = channel->impl->activateNtf;
				activateNtfData =
					channel->impl->activateNtfData;

				CommSetActive(channel);
				CommOS_Debug(("%s: Received attach ack. " \
					      "Activating channel.\n",
					      __func__));
			}

			CommHold(channel);
			CommGlobalUnlockBH();

			if (activateNtf)
				activateNtf(activateNtfData, channel);
			else
				CommRelease(channel);

		} else if ((channel->impl->openTimeoutAtMillis <=
			    CommOS_GetCurrentMillis()) ||
			   !running) {
			zombify = 1;
			CommOS_Debug(("%s: Zombifying expired open channel.\n",
				      __func__));
		}
		rc = 1;
	}

	DispatchUnlock(channel);

out:
	if (zombify)
		Comm_Zombify(channel, 0);

	return rc;

outUnlockAndFreeIovec:
	DispatchUnlock(channel);
	while (vecLen) {
		if (vec[--vecLen].iov_base) {
			channel->impl->dataFree(vec[vecLen].iov_base);
			vec[vecLen].iov_base = NULL;
		}
		vec[vecLen].iov_len = 0;
	}
	goto out;
#undef VEC_SIZE
}



unsigned int
Comm_DispatchAll(void)
{
	unsigned int i;
	unsigned int hits;

	for (hits = 0, i = 0; running && (i < commChannelSize); i++)
		hits += !!Comm_Dispatch(&commChannels[i]);

	return hits;
}


/**
 * @brief Writes a fully formatted packet (containing payload data, if
 *    applicable) to the specified channel.
 *    Note: This function requires the packet header and inlined payload,
 *    if any, to be in kernel memory.
 *    The operation may block until enough write space is available, but no
 *    more than the specified interval. The operation either writes the full
 *    amount of bytes, or it fails. Warning: callers must _not_ use the
 *    _Lock/_Unlock functions to bracket calls to this function.
 * @param[in,out] channel channel to write to.
 * @param packet packet to write.
 * @param[in,out] timeoutMillis interval in milliseconds to wait.
 * @return number of bytes written, 0 if it times out, -1 error.
 * @sideeffects Data may be written to the channel.
 */

int
Comm_Write(CommChannel channel,
	   const CommPacket *packet,
	   unsigned long long *timeoutMillis)
{
	int rc = -1;
	int zombify;

	if (!channel || !timeoutMillis ||
	    !packet || (packet->len < sizeof(*packet)))
		return rc;


	zombify = (*timeoutMillis >= COMM_MAX_TO);

	WriteLock(channel);
	if (!CommIsActive(channel))
		goto out;


	CommTransp_EnqueueReset(channel->transp);
	channel->desiredWriteSpace = packet->len;
	rc = CommOS_DoWait(&channel->availableWaitQ, WriteSpaceCondition,
			   channel, NULL, timeoutMillis,
			   (*timeoutMillis != COMM_MAX_TO_UNINT));
	channel->desiredWriteSpace = -1U;

	if (rc) 
		zombify = 0;

	if (rc == 1) { 
		rc = CommTransp_EnqueueSegment(channel->transp, packet,
					       packet->len, 1);
		if ((rc != packet->len) ||
		    CommTransp_EnqueueCommit(channel->transp)) {
			zombify = 1;
			rc = -1; 
		}
	}

out:
	WriteUnlock(channel);
	if (zombify)
		Comm_Zombify(channel, 0);

	return rc;
}


/**
 * @brief Writes a packet and associated payload data to the specified channel.
 *     Note: This function requires the packet header to be in kernel memory;
 *     payloads may be in either kernel or user memory.
 *     The operation may block until enough write space is available, but
 *     not more than the specified interval.
 *     The operation either writes the full amount of bytes, or it fails.
 *     If there is not enough data in the vector, padding will be added to
 *     reach the specified packet length, if the flags parameter requires it.
 *     Users may call this function successively to write several packets
 *     from large {io|k}vecs, when the flags parameter indicates it. If this
 *     is the case, the packet header needs to be updated accordingly in
 *     between calls, for the different (total) lengths.
 *     Warning: callers must _not_ use the _Lock/_Unlock functions to bracket
 *              calls to this function.
 * @param[in,out] channel the specified channel.
 * @param packet packet to write.
 * @param[in,out] vec kvec to write from.
 * @param[in,out] vecLen length of kvec.
 * @param[in,out] timeoutMillis interval in milliseconds to wait.
 * @param[in,out] iovOffset must be set to 0 before first call (internal cookie)
 * @param kern != 0 if payloads are in kernel memory
 * @return number of bytes written, 0 if it timed out, < 0 error
 * @sideeffects data may be written to the channel.
 */

int
Comm_WriteVec(CommChannel channel,
	      const CommPacket *packet,
	      struct kvec **vec,
	      unsigned int *vecLen,
	      unsigned long long *timeoutMillis,
	      unsigned int *iovOffset,
	      int kern)
{
	int rc;
	int zombify;
	unsigned int dataLen;
	unsigned int vecDataLen;
	unsigned int vecNdx;
	unsigned int iovLen;
	void *iovBase;

	if (!channel || !timeoutMillis || !iovOffset ||
	    !packet || (packet->len < sizeof(*packet)) ||
	    ((packet->len > sizeof(*packet)) &&
	    (!*vec || !*vecLen)))
		return -1;

	dataLen = packet->len - sizeof(*packet);

	zombify = (*timeoutMillis >= COMM_MAX_TO);

	WriteLock(channel);
	if (!CommIsActive(channel)) {
		rc = -1;
		goto out;
	}

	CommTransp_EnqueueReset(channel->transp);
	channel->desiredWriteSpace = packet->len;
	rc = CommOS_DoWait(&channel->availableWaitQ, WriteSpaceCondition,
			   channel, NULL, timeoutMillis,
			   (*timeoutMillis != COMM_MAX_TO_UNINT));
	channel->desiredWriteSpace = -1U;

	if (rc) 
		zombify = 0;

	if (rc == 1) { 
		iovLen = 0;
		rc = CommTransp_EnqueueSegment(channel->transp,
					       packet, sizeof(*packet), 1);
		if (rc != sizeof(*packet)) {
			zombify = 1;
			rc = -1; 
			goto out;
		}

		if (dataLen > 0) {
			int done = 0;

			for (vecDataLen = 0, vecNdx = 0;
			     vecNdx < *vecLen;
			     vecNdx++) {
				if (vecNdx)
					*iovOffset = 0;

				iovLen = (*vec)[vecNdx].iov_len - *iovOffset;
				iovBase = (*vec)[vecNdx].iov_base + *iovOffset;

				if (!iovLen)
					continue;

				vecDataLen += iovLen;
				if (vecDataLen >= dataLen) {
					iovLen -= (vecDataLen - dataLen);
					done = 1;
				}

				rc = CommTransp_EnqueueSegment(channel->transp,
							       iovBase, iovLen,
							       kern);
				if (rc != iovLen) {
					if (kern) {
						

						zombify = 1;
						rc = -1;
					} else {
						

						rc = -EFAULT;
					}
					goto out;
				}

				if (done) {
					CommTransp_EnqueueCommit(
						channel->transp);
					if (vecDataLen == dataLen) {
						vecNdx++;
						*iovOffset = 0;
					} else {
						*iovOffset += iovLen;
					}
					*vecLen -= vecNdx;
					*vec += vecNdx;
					break;
				}
			}

			if (!done) {

				static char pad[1024];
				unsigned int delta;
				unsigned int toSend;

				while (vecDataLen < dataLen) {
					delta = dataLen - vecDataLen;
					toSend = delta <= sizeof(pad) ?
							delta : sizeof(pad);
					if (toSend == delta)
						done = 1;

					vecDataLen += toSend;

					rc = CommTransp_EnqueueSegment(
						channel->transp, pad,
						toSend, 1);
					if (rc != toSend) {
						zombify = 1;
						rc = -1; 
						goto out;
					}

					if (done) {
						CommTransp_EnqueueCommit(
							channel->transp);
						*vec = NULL;
						*vecLen = 0;
						*iovOffset = 0;
						break;
					}
				}
			}
		} else {
			CommTransp_EnqueueCommit(channel->transp);
		}
		rc = (int)packet->len;
	} else {
		CommOS_Debug(("%s: timed out...\n", __func__));
	}

out:
	WriteUnlock(channel);
	if (zombify)
		Comm_Zombify(channel, 0);

	return rc;
}



void
Comm_Put(CommChannel channel)
{
	if (channel)
		CommRelease(channel);
}



void
Comm_DispatchUnlock(CommChannel channel)
{
	if (channel)
		DispatchUnlock(channel);
}



int
Comm_Lock(CommChannel channel)
{
	if (!channel)
		return -1;

	StateLock(channel);
	if (!CommIsActive(channel) && !CommIsZombie(channel)) {
		StateUnlock(channel);
		return -1;
	}
	return 0;
}



void
Comm_Unlock(CommChannel channel)
{
	if (channel)
		StateUnlock(channel);
}



unsigned int
Comm_RequestInlineEvents(CommChannel channel)
{
	if (channel->transp)
		return CommTransp_RequestInlineEvents(channel->transp);
	else
		return (unsigned int)-1;
}



unsigned int
Comm_ReleaseInlineEvents(CommChannel channel)
{
	if (channel->transp)
		return CommTransp_ReleaseInlineEvents(channel->transp);
	else
		return (unsigned int)-1;
}

