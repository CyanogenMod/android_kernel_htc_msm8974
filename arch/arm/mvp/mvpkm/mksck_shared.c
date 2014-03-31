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

#include "mvp.h"
#include "mksck_shared.h"


Mksck *
MksckPage_GetFromAddr(MksckPage *mksckPage,
		      Mksck_Address addr)
{
	Mksck *mksck = mksckPage->sockets;
	uint32 ii;

	ASSERT(addr.vmId == mksckPage->vmId);

	for (ii = mksckPage->numAllocSocks; ii--; mksck++) {
		if ((ATOMIC_GETO(mksck->refCount) != 0) &&
		    (mksck->addr.addr == addr.addr))
			return mksck;
	}
	return NULL;
}

void
Mksck_CloseCommon(Mksck *mksck)
{
	Mksck_DisconnectPeer(mksck);

	while (Mutex_Lock(&mksck->mutex, MutexModeEX) < 0)
		;

	mksck->shutDown = MKSCK_SHUT_WR | MKSCK_SHUT_RD;
	Mutex_UnlWake(&mksck->mutex, MutexModeEX, MKSCK_CVAR_ROOM, true);

	Mksck_DecRefc(mksck);
}


void
Mksck_DecRefc(Mksck *mksck)
{
	uint32 oldRefc;

	DMB();
	do {
		while ((oldRefc = ATOMIC_GETO(mksck->refCount)) == 1) {
			MksckPage *mksckPage = Mksck_ToSharedPage(mksck);


			while (Mutex_Lock(&mksckPage->mutex, MutexModeEX) < 0)
				;

			if (ATOMIC_SETIF(mksck->refCount, 0, 1)) {
#if 0
				KNOWN_BUG(MVP-1349);
				PRINTK("Mksck_DecRefc: %08X " \
				       "shutDown %u, foundEmpty %u, " \
				       "foundFull %u, blocked %u\n",
				       mksck->addr.addr, mksck->shutDown,
				       mksck->foundEmpty, mksck->foundFull,
				       ATOMIC_GETO(mksck->mutex.blocked));
#endif

				ASSERT(mksck->peer == 0);

				Mutex_Unlock(&mksckPage->mutex, MutexModeEX);
				MksckPage_DecRefc(mksckPage);
				return;
			}

			Mutex_Unlock(&mksckPage->mutex, MutexModeEX);
		}

		 ASSERT(oldRefc != 0);
	} while (!ATOMIC_SETIF(mksck->refCount, oldRefc - 1, oldRefc));
}


Mksck_Port
MksckPage_GetFreePort(MksckPage *mksckPage,
		      Mksck_Port port)
{
	Mksck_Address addr = { .addr = Mksck_AddrInit(mksckPage->vmId, port) };
	uint32 ii;

	if (port == MKSCK_PORT_UNDEF)
		for (ii = 0; ii < MKSCK_SOCKETS_PER_PAGE; ii++) {

			addr.port = mksckPage->portStore--;
			if (!addr.port)
				mksckPage->portStore = MKSCK_PORT_HIGH;

			if (!MksckPage_GetFromAddr(mksckPage, addr))
				return addr.port;
		}
	else if (!MksckPage_GetFromAddr(mksckPage, addr))
		return addr.port;

	return MKSCK_PORT_UNDEF;
}

Mksck *
MksckPage_AllocSocket(MksckPage *mksckPage,
		      Mksck_Address addr)
{
	Mksck *mksck;
	uint32 i;

	for (i = 0;
	     (offsetof(MksckPage, sockets[i+1]) <= MKSCKPAGE_SIZE) &&
	     (i < 8 * sizeof(mksckPage->wakeHostRecv)) &&
	     (i < 8 * sizeof(mksckPage->wakeVMMRecv));
	     i++) {
		mksck = &mksckPage->sockets[i];
		if (ATOMIC_GETO(mksck->refCount) == 0) {
			ATOMIC_SETV(mksck->refCount, 1);
			mksck->addr          = addr;
			mksck->peerAddr.addr = MKSCK_ADDR_UNDEF;
			mksck->peer          = NULL;
			mksck->index         = i;
			mksck->write         = 0;
			mksck->read          = 0;
			mksck->shutDown      = 0;
			mksck->foundEmpty    = 0;
			mksck->foundFull     = 0;
			ATOMIC_SETV(mksck->mutex.blocked, 0);
			mksck->rcvCBEntryMVA = 0;
			mksck->rcvCBParamMVA = 0;

			if (mksckPage->numAllocSocks < ++i)
				mksckPage->numAllocSocks = i;

			return mksck;
		}
	}
	return NULL;
}


_Bool
Mksck_IncReadIndex(Mksck *mksck,
		   uint32 read,
		   Mksck_Datagram *dg)
{
	ASSERT(read == mksck->read);
	ASSERT((void *)dg == (void *)&mksck->buff[read]);

	read += MKSCK_DGSIZE(dg->len);
	if ((read > mksck->write) && (read >= mksck->wrap)) {
		ASSERT(read == mksck->wrap);
		read = 0;
	}
	mksck->read = read;

	return read == mksck->write;
}


uint32
Mksck_FindSendRoom(Mksck *mksck,
		   uint32 needed)
{
	uint32 read, write;

	read  = mksck->read;
	write = mksck->write;
	if (write == read) {
		if (needed < MKSCK_BUFSIZE) {
			mksck->read  = 0;
			mksck->write = 0;
			return 0;
		}
	} else if (write < read) {
		if (write + needed < read)
			return write;
	} else {
		if (write + needed < MKSCK_BUFSIZE)
			return write;

		if ((write + needed == MKSCK_BUFSIZE) && (read > 0))
			return write;

		if (needed < read) {
			mksck->wrap  = write;
			mksck->write = 0;
			return 0;
		}
	}

	return MKSCK_FINDSENDROOM_FULL;
}


/**
 * @brief increment read index over the packet just written
 * @param mksck socket packet was written to.
 *                Locked for exclusive access
 * @param write as returned by @ref Mksck_FindSendRoom
 * @param needed as passed to @ref Mksck_FindSendRoom
 * @return with mksck->write updated to next packet
 */
void
Mksck_IncWriteIndex(Mksck *mksck,
		    uint32 write,
		    uint32 needed)
{
	ASSERT(write == mksck->write);
	write += needed;
	if (write >= MKSCK_BUFSIZE) {
		ASSERT(write == MKSCK_BUFSIZE);
		mksck->wrap = MKSCK_BUFSIZE;
		write = 0;
	}
	ASSERT(write != mksck->read);
	mksck->write = write;
}
