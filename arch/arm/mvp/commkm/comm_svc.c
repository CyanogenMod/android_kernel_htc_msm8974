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


#include "comm_os.h"
#include "comm_os_mod_ver.h"
#include "comm_svc.h"



static int Init(void *args);
static void Exit(void);

COMM_OS_MOD_INIT(Init, Exit);

static int running;                 



static int
Init(void *argsIn)
{
	int rc = -1;
	unsigned int maxChannels = 8;

#if defined(COMM_BUILDING_SERVER)
	unsigned int pollingMillis = (unsigned int)-1;
#else
	unsigned int pollingMillis = 2000;
#endif
	unsigned int pollingCycles = 1;
	const char *args = argsIn;

	if (args && *args) {
		
		sscanf(args,
		       "max_channels:%u,poll_millis:%u,poll_cycles:%u",
		       &maxChannels, &pollingMillis, &pollingCycles);
		CommOS_Debug(("%s: arguments [%s].\n", __func__, args));
	}

	rc = Comm_Init(maxChannels);
	if (rc)
		goto out;

	rc = CommOS_StartIO(COMM_OS_MOD_SHORT_NAME_STRING "-disp",
			    Comm_DispatchAll, pollingMillis, pollingCycles,
			    COMM_OS_MOD_SHORT_NAME_STRING "-aio");
	if (rc) {
		unsigned long long timeout = 0;

		Comm_Finish(&timeout); 
		goto out;
	}

	running = 1;
	rc = 0;

out:
	return rc;
}



static int
Halt(void)
{
	unsigned int maxTries = 10;
	int rc = -1;

	if (!running) {
		rc = 0;
		goto out;
	}

	while (maxTries--) {
		unsigned long long timeout = 2000ULL;

		CommOS_Debug(("%s: Attempting to halt...\n", __func__));
		if (!Comm_Finish(&timeout)) {
			running = 0;
			rc = 0;
			break;
		}
	}

out:
	return rc;
}



static void
Exit(void)
{
	if (!Halt())
		CommOS_StopIO();
}



int
CommSvc_RegisterImpl(const CommImpl *impl)
{
	return Comm_RegisterImpl(impl);
}
#if defined(__linux__)
EXPORT_SYMBOL(CommSvc_RegisterImpl);
#endif 



void
CommSvc_UnregisterImpl(const CommImpl *impl)
{
	Comm_UnregisterImpl(impl);
}
#if defined(__linux__)
EXPORT_SYMBOL(CommSvc_UnregisterImpl);
#endif 



int
CommSvc_Alloc(const CommTranspInitArgs *transpArgs,
	      const CommImpl *impl,
	      int inBH,
	      CommChannel *newChannel)
{
	return Comm_Alloc(transpArgs, impl, inBH, newChannel);
}
#if defined(__linux__)
EXPORT_SYMBOL(CommSvc_Alloc);
#endif 



int
CommSvc_Zombify(CommChannel channel,
		int inBH)
{
	return Comm_Zombify(channel, inBH);
}
#if defined(__linux__)
EXPORT_SYMBOL(CommSvc_Zombify);
#endif 



int
CommSvc_IsActive(CommChannel channel)
{
	return Comm_IsActive(channel);
}
#if defined(__linux__)
EXPORT_SYMBOL(CommSvc_IsActive);
#endif 



CommTranspInitArgs
CommSvc_GetTranspInitArgs(CommChannel channel)
{
	return Comm_GetTranspInitArgs(channel);
}
#if defined(__linux__)
EXPORT_SYMBOL(CommSvc_GetTranspInitArgs);
#endif 



void *
CommSvc_GetState(CommChannel channel)
{
	return Comm_GetState(channel);
}
#if defined(__linux__)
EXPORT_SYMBOL(CommSvc_GetState);
#endif 


/**
 * @brief Writes a fully formatted packet (containing payload data, if
 *    applicable) to the specified channel.
 *    Note: This function requires the packet header and inlined payload,
 *    if any, to be in kernel memory.
 *    The operation may block until enough write space is available, but no
 *    more than the specified interval.  The operation either writes the full
 *    amount of bytes, or it fails.  Warning: callers must _not_ use the
 *    _Lock/_Unlock functions to bracket calls to this function.
 * @param[in,out] channel channel to write to.
 * @param packet packet to write.
 * @param[in,out] timeoutMillis interval in milliseconds to wait.
 * @return number of bytes written, 0 if it times out, -1 error.
 * @sideeffects Data may be written to the channel.
 */

int
CommSvc_Write(CommChannel channel,
	      const CommPacket *packet,
	      unsigned long long *timeoutMillis)
{
	return Comm_Write(channel, packet, timeoutMillis);
}
#if defined(__linux__)
EXPORT_SYMBOL(CommSvc_Write);
#endif 


/**
 * @brief Writes a packet and associated payload data to the specified channel.
 *     Note: This function requires the packet header to be in kernel memory;
 *     payloads may be in either kernel or user memory.
 *     The operation may block until enough write space is available, but not
 *     more than the specified interval.  The operation either writes the full
 *     amount of bytes, or it fails.  Users may call this function successively
 *     to write several packets from large {io|k}vecs. If that's the case, the
 *     packet header needs to be updated in between calls, for the different
 *     (total) lengths.  Warning: callers must _not_ use the _Lock/_Unlock
 *     functions to bracket calls to this function.
 * @param[in,out] channel the specified channel
 * @param packet packet to write
 * @param[in,out] vec kvec to write from
 * @param[in,out] vecLen length of kvec
 * @param[in,out] timeoutMillis interval in milliseconds to wait
 * @param[in,out] iovOffset must be set to 0 before first call (internal cookie)
 * @param kern != 0 if payloads are in kernel memory
 * @return number of bytes written, 0 if it timed out, < 0 error
 * @sideeffects data may be written to the channel
 */

int
CommSvc_WriteVec(CommChannel channel,
		 const CommPacket *packet,
		 struct kvec **vec,
		 unsigned int *vecLen,
		 unsigned long long *timeoutMillis,
		 unsigned int *iovOffset,
		 int kern)
{
	return Comm_WriteVec(channel, packet, vec, vecLen,
			     timeoutMillis, iovOffset, kern);
}
#if defined(__linux__)
EXPORT_SYMBOL(CommSvc_WriteVec);
#endif 



void
CommSvc_Put(CommChannel channel)
{
	Comm_Put(channel);
}
#if defined(__linux__)
EXPORT_SYMBOL(CommSvc_Put);
#endif 



void
CommSvc_DispatchUnlock(CommChannel channel)
{
	Comm_DispatchUnlock(channel);
}
#if defined(__linux__)
EXPORT_SYMBOL(CommSvc_DispatchUnlock);
#endif 



int
CommSvc_Lock(CommChannel channel)
{
	return Comm_Lock(channel);
}
#if defined(__linux__)
EXPORT_SYMBOL(CommSvc_Lock);
#endif 



void
CommSvc_Unlock(CommChannel channel)
{
	Comm_Unlock(channel);
}
#if defined(__linux__)
EXPORT_SYMBOL(CommSvc_Unlock);
#endif 



int
CommSvc_ScheduleAIOWork(CommOSWork *work)
{
	return CommOS_ScheduleAIOWork(work);
}
#if defined(__linux__)
EXPORT_SYMBOL(CommSvc_ScheduleAIOWork);
#endif 



unsigned int
CommSvc_RequestInlineEvents(CommChannel channel)
{
	return Comm_RequestInlineEvents(channel);
}
#if defined(__linux__)
EXPORT_SYMBOL(CommSvc_RequestInlineEvents);
#endif 



unsigned int
CommSvc_ReleaseInlineEvents(CommChannel channel)
{
	return Comm_ReleaseInlineEvents(channel);
}
#if defined(__linux__)
EXPORT_SYMBOL(CommSvc_ReleaseInlineEvents);
#endif 

