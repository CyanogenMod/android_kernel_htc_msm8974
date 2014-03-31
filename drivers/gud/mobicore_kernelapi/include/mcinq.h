/*
 * Notifications inform the MobiCore runtime environment that information is
 * pending in a WSM buffer.
 *
 * The Trustlet Connector (TLC) and the corresponding Trustlet also utilize
 * this buffer to notify each other about new data within the
 * Trustlet Connector Interface (TCI).
 *
 * The buffer is set up as a queue, which means that more than one
 * notification can be written to the buffer before the switch to the other
 * world is performed. Each side therefore facilitates an incoming and an
 * outgoing queue for communication with the other side.
 *
 * Notifications hold the session ID, which is used to reference the
 * communication partner in the other world.
 * So if, e.g., the TLC in the normal world wants to notify his Trustlet
 * about new data in the TLC buffer
 *
 * Notification queue declarations.
 *
 * <-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *	products derived from this software without specific prior
 *	written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _MCINQ_H_
#define _MCINQ_H_

#define MIN_NQ_ELEM	1	
#define MAX_NQ_ELEM	64	

#define MIN_NQ_LEN	(MIN_NQ_ELEM * sizeof(notification))

#define MAX_NQ_LEN	(MAX_NQ_ELEM * sizeof(notification))

#define SID_MCP		0
#define SID_INVALID	0xffffffff

struct notification {
	uint32_t	session_id;	
	int32_t		payload;	
};

enum notification_payload {
	
	ERR_INVALID_EXIT_CODE	= -1,
	
	ERR_SESSION_CLOSE	= -2,
	
	ERR_INVALID_OPERATION	= -3,
	
	ERR_INVALID_SID		= -4,
	
	ERR_SID_NOT_ACTIVE	= -5
};

struct notification_queue_header {
	uint32_t	write_cnt;	
	uint32_t	read_cnt;	
	uint32_t	queue_size;	
};

struct notification_queue {
	
	struct notification_queue_header hdr;
	
	struct notification notification[MIN_NQ_ELEM];
};

#endif 
