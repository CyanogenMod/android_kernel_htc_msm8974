/******************************************************************************
*******************************************************************************
**
**  Copyright (C) Sistina Software, Inc.  1997-2003  All rights reserved.
**  Copyright (C) 2004-2008 Red Hat, Inc.  All rights reserved.
**
**  This copyrighted material is made available to anyone wishing to use,
**  modify, copy, or redistribute it subject to the terms and conditions
**  of the GNU General Public License v.2.
**
*******************************************************************************
******************************************************************************/


#include "dlm_internal.h"
#include "lowcomms.h"
#include "config.h"
#include "lock.h"
#include "midcomms.h"


static void copy_from_cb(void *dst, const void *base, unsigned offset,
			 unsigned len, unsigned limit)
{
	unsigned copy = len;

	if ((copy + offset) > limit)
		copy = limit - offset;
	memcpy(dst, base + offset, copy);
	len -= copy;
	if (len)
		memcpy(dst + copy, base, len);
}


int dlm_process_incoming_buffer(int nodeid, const void *base,
				unsigned offset, unsigned len, unsigned limit)
{
	union {
		unsigned char __buf[DLM_INBUF_LEN];
		
		union dlm_packet p;
	} __tmp;
	union dlm_packet *p = &__tmp.p;
	int ret = 0;
	int err = 0;
	uint16_t msglen;
	uint32_t lockspace;

	while (len > sizeof(struct dlm_header)) {


		copy_from_cb(p, base, offset, sizeof(struct dlm_header),
			     limit);

		msglen = le16_to_cpu(p->header.h_length);
		lockspace = p->header.h_lockspace;

		err = -EINVAL;
		if (msglen < sizeof(struct dlm_header))
			break;
		if (p->header.h_cmd == DLM_MSG) {
			if (msglen < sizeof(struct dlm_message))
				break;
		} else {
			if (msglen < sizeof(struct dlm_rcom))
				break;
		}
		err = -E2BIG;
		if (msglen > dlm_config.ci_buffer_size) {
			log_print("message size %d from %d too big, buf len %d",
				  msglen, nodeid, len);
			break;
		}
		err = 0;


		if (msglen > len)
			break;


		if (msglen > sizeof(__tmp) && p == &__tmp.p) {
			p = kmalloc(dlm_config.ci_buffer_size, GFP_NOFS);
			if (p == NULL)
				return ret;
		}

		copy_from_cb(p, base, offset, msglen, limit);

		BUG_ON(lockspace != p->header.h_lockspace);

		ret += msglen;
		offset += msglen;
		offset &= (limit - 1);
		len -= msglen;

		dlm_receive_buffer(p, nodeid);
	}

	if (p != &__tmp.p)
		kfree(p);

	return err ? err : ret;
}

