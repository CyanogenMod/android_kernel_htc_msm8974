/*
 *  linux/drivers/acorn/scsi/msgqueue.c
 *
 *  Copyright (C) 1997-1998 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  message queue handling
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/init.h>

#include "msgqueue.h"

static struct msgqueue_entry *mqe_alloc(MsgQueue_t *msgq)
{
	struct msgqueue_entry *mq;

	if ((mq = msgq->free) != NULL)
		msgq->free = mq->next;

	return mq;
}

static void mqe_free(MsgQueue_t *msgq, struct msgqueue_entry *mq)
{
	if (mq) {
		mq->next = msgq->free;
		msgq->free = mq;
	}
}

void msgqueue_initialise(MsgQueue_t *msgq)
{
	int i;

	msgq->qe = NULL;
	msgq->free = &msgq->entries[0];

	for (i = 0; i < NR_MESSAGES; i++)
		msgq->entries[i].next = &msgq->entries[i + 1];

	msgq->entries[NR_MESSAGES - 1].next = NULL;
}


void msgqueue_free(MsgQueue_t *msgq)
{
}

int msgqueue_msglength(MsgQueue_t *msgq)
{
	struct msgqueue_entry *mq = msgq->qe;
	int length = 0;

	for (mq = msgq->qe; mq; mq = mq->next)
		length += mq->msg.length;

	return length;
}

struct message *msgqueue_getmsg(MsgQueue_t *msgq, int msgno)
{
	struct msgqueue_entry *mq;

	for (mq = msgq->qe; mq && msgno; mq = mq->next, msgno--);

	return mq ? &mq->msg : NULL;
}

int msgqueue_addmsg(MsgQueue_t *msgq, int length, ...)
{
	struct msgqueue_entry *mq = mqe_alloc(msgq);
	va_list ap;

	if (mq) {
		struct msgqueue_entry **mqp;
		int i;

		va_start(ap, length);
		for (i = 0; i < length; i++)
			mq->msg.msg[i] = va_arg(ap, unsigned int);
		va_end(ap);

		mq->msg.length = length;
		mq->msg.fifo = 0;
		mq->next = NULL;

		mqp = &msgq->qe;
		while (*mqp)
			mqp = &(*mqp)->next;

		*mqp = mq;
	}

	return mq != NULL;
}

void msgqueue_flush(MsgQueue_t *msgq)
{
	struct msgqueue_entry *mq, *mqnext;

	for (mq = msgq->qe; mq; mq = mqnext) {
		mqnext = mq->next;
		mqe_free(msgq, mq);
	}
	msgq->qe = NULL;
}

EXPORT_SYMBOL(msgqueue_initialise);
EXPORT_SYMBOL(msgqueue_free);
EXPORT_SYMBOL(msgqueue_msglength);
EXPORT_SYMBOL(msgqueue_getmsg);
EXPORT_SYMBOL(msgqueue_addmsg);
EXPORT_SYMBOL(msgqueue_flush);

MODULE_AUTHOR("Russell King");
MODULE_DESCRIPTION("SCSI message queue handling");
MODULE_LICENSE("GPL");
