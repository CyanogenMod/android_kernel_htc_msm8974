/*
 * ePAPR hcall interface
 *
 * Copyright 2008-2011 Freescale Semiconductor, Inc.
 *
 * Author: Timur Tabi <timur@freescale.com>
 *
 * This file is provided under a dual BSD/GPL license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef _EPAPR_HCALLS_H
#define _EPAPR_HCALLS_H

#include <linux/types.h>
#include <linux/errno.h>
#include <asm/byteorder.h>

#define EV_BYTE_CHANNEL_SEND		1
#define EV_BYTE_CHANNEL_RECEIVE		2
#define EV_BYTE_CHANNEL_POLL		3
#define EV_INT_SET_CONFIG		4
#define EV_INT_GET_CONFIG		5
#define EV_INT_SET_MASK			6
#define EV_INT_GET_MASK			7
#define EV_INT_IACK			9
#define EV_INT_EOI			10
#define EV_INT_SEND_IPI			11
#define EV_INT_SET_TASK_PRIORITY	12
#define EV_INT_GET_TASK_PRIORITY	13
#define EV_DOORBELL_SEND		14
#define EV_MSGSND			15
#define EV_IDLE				16

#define EV_LOCAL_VENDOR_ID		0	
#define EV_EPAPR_VENDOR_ID		1
#define EV_FSL_VENDOR_ID		2	
#define EV_IBM_VENDOR_ID		3	
#define EV_GHS_VENDOR_ID		4	
#define EV_ENEA_VENDOR_ID		5	
#define EV_WR_VENDOR_ID			6	
#define EV_AMCC_VENDOR_ID		7	
#define EV_KVM_VENDOR_ID		42	

#define EV_BYTE_CHANNEL_MAX_BYTES	16


#define _EV_HCALL_TOKEN(id, num) (((id) << 16) | (num))
#define EV_HCALL_TOKEN(hcall_num) _EV_HCALL_TOKEN(EV_EPAPR_VENDOR_ID, hcall_num)

#define EV_EPERM		1	
#define EV_ENOENT		2	
#define EV_EIO			3	
#define EV_EAGAIN		4	
#define EV_ENOMEM		5	
#define EV_EFAULT		6	
#define EV_ENODEV		7	
#define EV_EINVAL		8	
#define EV_INTERNAL		9	
#define EV_CONFIG		10	
#define EV_INVALID_STATE	11	
#define EV_UNIMPLEMENTED	12	
#define EV_BUFFER_OVERFLOW	13	


#define EV_HCALL_CLOBBERS "r0", "r12", "xer", "ctr", "lr", "cc", "memory"

#define EV_HCALL_CLOBBERS8 EV_HCALL_CLOBBERS
#define EV_HCALL_CLOBBERS7 EV_HCALL_CLOBBERS8, "r10"
#define EV_HCALL_CLOBBERS6 EV_HCALL_CLOBBERS7, "r9"
#define EV_HCALL_CLOBBERS5 EV_HCALL_CLOBBERS6, "r8"
#define EV_HCALL_CLOBBERS4 EV_HCALL_CLOBBERS5, "r7"
#define EV_HCALL_CLOBBERS3 EV_HCALL_CLOBBERS4, "r6"
#define EV_HCALL_CLOBBERS2 EV_HCALL_CLOBBERS3, "r5"
#define EV_HCALL_CLOBBERS1 EV_HCALL_CLOBBERS2, "r4"



static inline unsigned int ev_int_set_config(unsigned int interrupt,
	uint32_t config, unsigned int priority, uint32_t destination)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	register uintptr_t r5 __asm__("r5");
	register uintptr_t r6 __asm__("r6");

	r11 = EV_HCALL_TOKEN(EV_INT_SET_CONFIG);
	r3  = interrupt;
	r4  = config;
	r5  = priority;
	r6  = destination;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3), "+r" (r4), "+r" (r5), "+r" (r6)
		: : EV_HCALL_CLOBBERS4
	);

	return r3;
}

static inline unsigned int ev_int_get_config(unsigned int interrupt,
	uint32_t *config, unsigned int *priority, uint32_t *destination)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	register uintptr_t r5 __asm__("r5");
	register uintptr_t r6 __asm__("r6");

	r11 = EV_HCALL_TOKEN(EV_INT_GET_CONFIG);
	r3 = interrupt;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3), "=r" (r4), "=r" (r5), "=r" (r6)
		: : EV_HCALL_CLOBBERS4
	);

	*config = r4;
	*priority = r5;
	*destination = r6;

	return r3;
}

static inline unsigned int ev_int_set_mask(unsigned int interrupt,
	unsigned int mask)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");

	r11 = EV_HCALL_TOKEN(EV_INT_SET_MASK);
	r3 = interrupt;
	r4 = mask;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3), "+r" (r4)
		: : EV_HCALL_CLOBBERS2
	);

	return r3;
}

static inline unsigned int ev_int_get_mask(unsigned int interrupt,
	unsigned int *mask)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");

	r11 = EV_HCALL_TOKEN(EV_INT_GET_MASK);
	r3 = interrupt;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3), "=r" (r4)
		: : EV_HCALL_CLOBBERS2
	);

	*mask = r4;

	return r3;
}

static inline unsigned int ev_int_eoi(unsigned int interrupt)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");

	r11 = EV_HCALL_TOKEN(EV_INT_EOI);
	r3 = interrupt;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3)
		: : EV_HCALL_CLOBBERS1
	);

	return r3;
}

static inline unsigned int ev_byte_channel_send(unsigned int handle,
	unsigned int *count, const char buffer[EV_BYTE_CHANNEL_MAX_BYTES])
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	register uintptr_t r5 __asm__("r5");
	register uintptr_t r6 __asm__("r6");
	register uintptr_t r7 __asm__("r7");
	register uintptr_t r8 __asm__("r8");
	const uint32_t *p = (const uint32_t *) buffer;

	r11 = EV_HCALL_TOKEN(EV_BYTE_CHANNEL_SEND);
	r3 = handle;
	r4 = *count;
	r5 = be32_to_cpu(p[0]);
	r6 = be32_to_cpu(p[1]);
	r7 = be32_to_cpu(p[2]);
	r8 = be32_to_cpu(p[3]);

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3),
		  "+r" (r4), "+r" (r5), "+r" (r6), "+r" (r7), "+r" (r8)
		: : EV_HCALL_CLOBBERS6
	);

	*count = r4;

	return r3;
}

static inline unsigned int ev_byte_channel_receive(unsigned int handle,
	unsigned int *count, char buffer[EV_BYTE_CHANNEL_MAX_BYTES])
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	register uintptr_t r5 __asm__("r5");
	register uintptr_t r6 __asm__("r6");
	register uintptr_t r7 __asm__("r7");
	register uintptr_t r8 __asm__("r8");
	uint32_t *p = (uint32_t *) buffer;

	r11 = EV_HCALL_TOKEN(EV_BYTE_CHANNEL_RECEIVE);
	r3 = handle;
	r4 = *count;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3), "+r" (r4),
		  "=r" (r5), "=r" (r6), "=r" (r7), "=r" (r8)
		: : EV_HCALL_CLOBBERS6
	);

	*count = r4;
	p[0] = cpu_to_be32(r5);
	p[1] = cpu_to_be32(r6);
	p[2] = cpu_to_be32(r7);
	p[3] = cpu_to_be32(r8);

	return r3;
}

static inline unsigned int ev_byte_channel_poll(unsigned int handle,
	unsigned int *rx_count,	unsigned int *tx_count)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	register uintptr_t r5 __asm__("r5");

	r11 = EV_HCALL_TOKEN(EV_BYTE_CHANNEL_POLL);
	r3 = handle;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3), "=r" (r4), "=r" (r5)
		: : EV_HCALL_CLOBBERS3
	);

	*rx_count = r4;
	*tx_count = r5;

	return r3;
}

static inline unsigned int ev_int_iack(unsigned int handle,
	unsigned int *vector)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");

	r11 = EV_HCALL_TOKEN(EV_INT_IACK);
	r3 = handle;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3), "=r" (r4)
		: : EV_HCALL_CLOBBERS2
	);

	*vector = r4;

	return r3;
}

static inline unsigned int ev_doorbell_send(unsigned int handle)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");

	r11 = EV_HCALL_TOKEN(EV_DOORBELL_SEND);
	r3 = handle;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3)
		: : EV_HCALL_CLOBBERS1
	);

	return r3;
}

static inline unsigned int ev_idle(void)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");

	r11 = EV_HCALL_TOKEN(EV_IDLE);

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "=r" (r3)
		: : EV_HCALL_CLOBBERS1
	);

	return r3;
}

#endif
