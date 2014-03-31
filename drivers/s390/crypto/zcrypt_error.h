/*
 *  linux/drivers/s390/crypto/zcrypt_error.h
 *
 *  zcrypt 2.1.0
 *
 *  Copyright (C)  2001, 2006 IBM Corporation
 *  Author(s): Robert Burroughs
 *	       Eric Rossman (edrossma@us.ibm.com)
 *
 *  Hotplug & misc device support: Jochen Roehrig (roehrig@de.ibm.com)
 *  Major cleanup & driver split: Martin Schwidefsky <schwidefsky@de.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _ZCRYPT_ERROR_H_
#define _ZCRYPT_ERROR_H_

#include "zcrypt_api.h"

struct error_hdr {
	unsigned char reserved1;	
	unsigned char type;		
	unsigned char reserved2[2];	
	unsigned char reply_code;	
	unsigned char reserved3[3];	
};

#define TYPE82_RSP_CODE 0x82
#define TYPE88_RSP_CODE 0x88

#define REP82_ERROR_MACHINE_FAILURE  0x10
#define REP82_ERROR_PREEMPT_FAILURE  0x12
#define REP82_ERROR_CHECKPT_FAILURE  0x14
#define REP82_ERROR_MESSAGE_TYPE     0x20
#define REP82_ERROR_INVALID_COMM_CD  0x21	
#define REP82_ERROR_INVALID_MSG_LEN  0x23
#define REP82_ERROR_RESERVD_FIELD    0x24	
#define REP82_ERROR_FORMAT_FIELD     0x29
#define REP82_ERROR_INVALID_COMMAND  0x30
#define REP82_ERROR_MALFORMED_MSG    0x40
#define REP82_ERROR_RESERVED_FIELDO  0x50	
#define REP82_ERROR_WORD_ALIGNMENT   0x60
#define REP82_ERROR_MESSAGE_LENGTH   0x80
#define REP82_ERROR_OPERAND_INVALID  0x82
#define REP82_ERROR_OPERAND_SIZE     0x84
#define REP82_ERROR_EVEN_MOD_IN_OPND 0x85
#define REP82_ERROR_RESERVED_FIELD   0x88
#define REP82_ERROR_TRANSPORT_FAIL   0x90
#define REP82_ERROR_PACKET_TRUNCATED 0xA0
#define REP82_ERROR_ZERO_BUFFER_LEN  0xB0

#define REP88_ERROR_MODULE_FAILURE   0x10

#define REP88_ERROR_MESSAGE_TYPE     0x20
#define REP88_ERROR_MESSAGE_MALFORMD 0x22
#define REP88_ERROR_MESSAGE_LENGTH   0x23
#define REP88_ERROR_RESERVED_FIELD   0x24
#define REP88_ERROR_KEY_TYPE	     0x34
#define REP88_ERROR_INVALID_KEY      0x82	
#define REP88_ERROR_OPERAND	     0x84	
#define REP88_ERROR_OPERAND_EVEN_MOD 0x85	

static inline int convert_error(struct zcrypt_device *zdev,
				struct ap_message *reply)
{
	struct error_hdr *ehdr = reply->message;

	switch (ehdr->reply_code) {
	case REP82_ERROR_OPERAND_INVALID:
	case REP82_ERROR_OPERAND_SIZE:
	case REP82_ERROR_EVEN_MOD_IN_OPND:
	case REP88_ERROR_MESSAGE_MALFORMD:
	
	
	
		
		return -EINVAL;
	case REP82_ERROR_MESSAGE_TYPE:
	
		WARN_ON(1);
		zdev->online = 0;
		return -EAGAIN;
	case REP82_ERROR_TRANSPORT_FAIL:
	case REP82_ERROR_MACHINE_FAILURE:
	
		
		zdev->online = 0;
		return -EAGAIN;
	default:
		zdev->online = 0;
		return -EAGAIN;	
	}
}

#endif 
