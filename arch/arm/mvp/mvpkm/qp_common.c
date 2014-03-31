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


#include <linux/uaccess.h>
#include <linux/module.h>

#include "mvp_types.h"
#include "comm_os.h"
#include "qp.h"


static inline int32
FreeSpace(uint32 head,
	  uint32 tail,
	  uint32 queueSize)
{

	return (tail >= head) ? (queueSize - (tail - head) - 1) :
				(head - tail - 1);
}


int32
QP_EnqueueSpace(QPHandle *qp)
{
	uint32 head;
	uint32 phantom;

	if (!QP_CheckHandle(qp))
		return QP_ERROR_INVALID_HANDLE;

	head    = qp->produceQ->head;
	phantom = qp->produceQ->phantom_tail;

	if (head    >= qp->queueSize ||
	    phantom >= qp->queueSize)
		return QP_ERROR_INVALID_HANDLE;

	return FreeSpace(head, phantom, qp->queueSize);
}
EXPORT_SYMBOL(QP_EnqueueSpace);


int32
QP_EnqueueSegment(QPHandle *qp,
		  const void *buf,
		  size_t bufSize,
		  int kern)
{
	int32 freeSpace;
	uint32 head;
	uint32 phantom;

	if (!QP_CheckHandle(qp))
		return QP_ERROR_INVALID_HANDLE;

	head = qp->produceQ->head;
	phantom = qp->produceQ->phantom_tail;

	if (head    >= qp->queueSize ||
	    phantom >= qp->queueSize)
		return QP_ERROR_INVALID_HANDLE;

	freeSpace = FreeSpace(head, phantom, qp->queueSize);

	if (bufSize <= freeSpace) {
		if (bufSize + phantom < qp->queueSize) {
			if (kern) {
				memcpy(qp->produceQ->data + phantom,
				       buf, bufSize);
			} else {
				if (copy_from_user(qp->produceQ->data + phantom,
						   buf, bufSize))
					return QP_ERROR_INVALID_ARGS;
			}
			phantom += bufSize;
		} else {
			uint32 written = qp->queueSize - phantom;

			if (kern) {
				memcpy(qp->produceQ->data + phantom,
				       buf, written);
				memcpy(qp->produceQ->data,
				       (uint8 *)buf + written,
				       bufSize - written);
			} else {
				if (copy_from_user(qp->produceQ->data + phantom,
						   buf, written))
					return QP_ERROR_INVALID_ARGS;
				if (copy_from_user(qp->produceQ->data,
						   (uint8 *)buf + written,
						   bufSize - written))
					return QP_ERROR_INVALID_ARGS;
			}
			phantom = bufSize - written;
		}
	} else {
		return QP_ERROR_NO_MEM;
	}

	qp->produceQ->phantom_tail = phantom;

	return bufSize;
}
EXPORT_SYMBOL(QP_EnqueueSegment);


int32
QP_EnqueueCommit(QPHandle *qp)
{
	uint32 phantom;

	if (!QP_CheckHandle(qp))
		return QP_ERROR_INVALID_HANDLE;

	phantom = qp->produceQ->phantom_tail;
	if (phantom >= qp->queueSize)
		return QP_ERROR_INVALID_HANDLE;

	qp->produceQ->tail = phantom;
	return QP_SUCCESS;
}
EXPORT_SYMBOL(QP_EnqueueCommit);


int32
QP_DequeueSpace(QPHandle *qp)
{
	uint32 tail;
	uint32 phantom;
	int32 bytesAvailable;

	if (!QP_CheckHandle(qp))
		return QP_ERROR_INVALID_HANDLE;

	tail    = qp->consumeQ->tail;
	phantom = qp->consumeQ->phantom_head;

	if (tail    >= qp->queueSize ||
	    phantom >= qp->queueSize)
		return QP_ERROR_INVALID_HANDLE;

	bytesAvailable = (tail - phantom);
	if ((int32)bytesAvailable < 0)
		bytesAvailable += qp->queueSize;

	return bytesAvailable;
}
EXPORT_SYMBOL(QP_DequeueSpace);


int32
QP_DequeueSegment(QPHandle *qp,
		  void *buf,
		  size_t bytesDesired,
		  int kern)
{
	uint32 tail;
	uint32 phantom;
	int32 bytesAvailable = 0;

	if (!QP_CheckHandle(qp))
		return QP_ERROR_INVALID_HANDLE;

	tail = qp->consumeQ->tail;
	phantom = qp->consumeQ->phantom_head;

	if (tail    >= qp->queueSize  ||
	    phantom >= qp->queueSize)
		return QP_ERROR_INVALID_HANDLE;

	bytesAvailable = (tail - phantom);
	if ((int32)bytesAvailable < 0)
		bytesAvailable += qp->queueSize;

	if (bytesDesired <= bytesAvailable) {
		if (bytesDesired + phantom < qp->queueSize) {
			if (kern) {
				memcpy(buf, qp->consumeQ->data + phantom,
				       bytesDesired);
			} else {
				if (copy_to_user(buf,
				    qp->consumeQ->data + phantom,
				    bytesDesired))
					return QP_ERROR_INVALID_ARGS;
			}
			phantom += bytesDesired;
		} else {
			uint32 written = qp->queueSize - phantom;

			if (kern) {
				memcpy(buf, qp->consumeQ->data + phantom,
				       written);
				memcpy((uint8 *)buf + written,
				       qp->consumeQ->data,
				       bytesDesired - written);
			} else {
				if (copy_to_user(buf,
						 qp->consumeQ->data + phantom,
						 written))
					return QP_ERROR_INVALID_ARGS;
				if (copy_to_user((uint8 *)buf + written,
						 qp->consumeQ->data,
						 bytesDesired - written))
					return QP_ERROR_INVALID_ARGS;
			}
			phantom = bytesDesired - written;
		}
	} else {
		return QP_ERROR_NO_MEM;
	}

	qp->consumeQ->phantom_head = phantom;

	return bytesDesired;
}
EXPORT_SYMBOL(QP_DequeueSegment);


int32
QP_DequeueCommit(QPHandle *qp)
{
	uint32 phantom;

	if (!QP_CheckHandle(qp))
		return QP_ERROR_INVALID_HANDLE;

	phantom = qp->consumeQ->phantom_head;
	if (phantom >= qp->queueSize)
		return QP_ERROR_INVALID_HANDLE;

	qp->consumeQ->head = phantom;
	return QP_SUCCESS;
}
EXPORT_SYMBOL(QP_DequeueCommit);


int32
QP_EnqueueReset(QPHandle *qp)
{
	uint32 tail;

	if (!QP_CheckHandle(qp))
		return QP_ERROR_INVALID_HANDLE;

	tail = qp->produceQ->tail;
	if (tail >= qp->queueSize)
		return QP_ERROR_INVALID_HANDLE;

	qp->produceQ->phantom_tail = tail;
	return QP_SUCCESS;
}
EXPORT_SYMBOL(QP_EnqueueReset);

int32
QP_DequeueReset(QPHandle *qp)
{
	uint32 head;

	if (!QP_CheckHandle(qp))
		return QP_ERROR_INVALID_HANDLE;

	head = qp->consumeQ->head;
	if (head >= qp->queueSize)
		return QP_ERROR_INVALID_HANDLE;

	qp->consumeQ->phantom_head = head;
	return QP_SUCCESS;
}
EXPORT_SYMBOL(QP_DequeueReset);

