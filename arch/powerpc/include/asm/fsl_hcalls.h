/*
 * Freescale hypervisor call interface
 *
 * Copyright 2008-2010 Freescale Semiconductor, Inc.
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

#ifndef _FSL_HCALLS_H
#define _FSL_HCALLS_H

#include <linux/types.h>
#include <linux/errno.h>
#include <asm/byteorder.h>
#include <asm/epapr_hcalls.h>

#define FH_API_VERSION			1

#define FH_ERR_GET_INFO			1
#define FH_PARTITION_GET_DTPROP		2
#define FH_PARTITION_SET_DTPROP		3
#define FH_PARTITION_RESTART		4
#define FH_PARTITION_GET_STATUS		5
#define FH_PARTITION_START		6
#define FH_PARTITION_STOP		7
#define FH_PARTITION_MEMCPY		8
#define FH_DMA_ENABLE			9
#define FH_DMA_DISABLE			10
#define FH_SEND_NMI			11
#define FH_VMPIC_GET_MSIR		12
#define FH_SYSTEM_RESET			13
#define FH_GET_CORE_STATE		14
#define FH_ENTER_NAP			15
#define FH_EXIT_NAP			16
#define FH_CLAIM_DEVICE			17
#define FH_PARTITION_STOP_DMA		18

#define FH_HCALL_TOKEN(num)		_EV_HCALL_TOKEN(EV_FSL_VENDOR_ID, num)


static inline unsigned int fh_send_nmi(unsigned int vcpu_mask)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");

	r11 = FH_HCALL_TOKEN(FH_SEND_NMI);
	r3 = vcpu_mask;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3)
		: : EV_HCALL_CLOBBERS1
	);

	return r3;
}

#define FH_DTPROP_MAX_PATHLEN 4096
#define FH_DTPROP_MAX_PROPLEN 32768

static inline unsigned int fh_partition_get_dtprop(int handle,
						   uint64_t dtpath_addr,
						   uint64_t propname_addr,
						   uint64_t propvalue_addr,
						   uint32_t *propvalue_len)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	register uintptr_t r5 __asm__("r5");
	register uintptr_t r6 __asm__("r6");
	register uintptr_t r7 __asm__("r7");
	register uintptr_t r8 __asm__("r8");
	register uintptr_t r9 __asm__("r9");
	register uintptr_t r10 __asm__("r10");

	r11 = FH_HCALL_TOKEN(FH_PARTITION_GET_DTPROP);
	r3 = handle;

#ifdef CONFIG_PHYS_64BIT
	r4 = dtpath_addr >> 32;
	r6 = propname_addr >> 32;
	r8 = propvalue_addr >> 32;
#else
	r4 = 0;
	r6 = 0;
	r8 = 0;
#endif
	r5 = (uint32_t)dtpath_addr;
	r7 = (uint32_t)propname_addr;
	r9 = (uint32_t)propvalue_addr;
	r10 = *propvalue_len;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11),
		  "+r" (r3), "+r" (r4), "+r" (r5), "+r" (r6), "+r" (r7),
		  "+r" (r8), "+r" (r9), "+r" (r10)
		: : EV_HCALL_CLOBBERS8
	);

	*propvalue_len = r4;
	return r3;
}

static inline unsigned int fh_partition_set_dtprop(int handle,
						   uint64_t dtpath_addr,
						   uint64_t propname_addr,
						   uint64_t propvalue_addr,
						   uint32_t propvalue_len)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	register uintptr_t r6 __asm__("r6");
	register uintptr_t r8 __asm__("r8");
	register uintptr_t r5 __asm__("r5");
	register uintptr_t r7 __asm__("r7");
	register uintptr_t r9 __asm__("r9");
	register uintptr_t r10 __asm__("r10");

	r11 = FH_HCALL_TOKEN(FH_PARTITION_SET_DTPROP);
	r3 = handle;

#ifdef CONFIG_PHYS_64BIT
	r4 = dtpath_addr >> 32;
	r6 = propname_addr >> 32;
	r8 = propvalue_addr >> 32;
#else
	r4 = 0;
	r6 = 0;
	r8 = 0;
#endif
	r5 = (uint32_t)dtpath_addr;
	r7 = (uint32_t)propname_addr;
	r9 = (uint32_t)propvalue_addr;
	r10 = propvalue_len;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11),
		  "+r" (r3), "+r" (r4), "+r" (r5), "+r" (r6), "+r" (r7),
		  "+r" (r8), "+r" (r9), "+r" (r10)
		: : EV_HCALL_CLOBBERS8
	);

	return r3;
}

static inline unsigned int fh_partition_restart(unsigned int partition)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");

	r11 = FH_HCALL_TOKEN(FH_PARTITION_RESTART);
	r3 = partition;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3)
		: : EV_HCALL_CLOBBERS1
	);

	return r3;
}

#define FH_PARTITION_STOPPED	0
#define FH_PARTITION_RUNNING	1
#define FH_PARTITION_STARTING	2
#define FH_PARTITION_STOPPING	3
#define FH_PARTITION_PAUSING	4
#define FH_PARTITION_PAUSED	5
#define FH_PARTITION_RESUMING	6

static inline unsigned int fh_partition_get_status(unsigned int partition,
	unsigned int *status)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");

	r11 = FH_HCALL_TOKEN(FH_PARTITION_GET_STATUS);
	r3 = partition;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3), "=r" (r4)
		: : EV_HCALL_CLOBBERS2
	);

	*status = r4;

	return r3;
}

static inline unsigned int fh_partition_start(unsigned int partition,
	uint32_t entry_point, int load)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	register uintptr_t r5 __asm__("r5");

	r11 = FH_HCALL_TOKEN(FH_PARTITION_START);
	r3 = partition;
	r4 = entry_point;
	r5 = load;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3), "+r" (r4), "+r" (r5)
		: : EV_HCALL_CLOBBERS3
	);

	return r3;
}

static inline unsigned int fh_partition_stop(unsigned int partition)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");

	r11 = FH_HCALL_TOKEN(FH_PARTITION_STOP);
	r3 = partition;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3)
		: : EV_HCALL_CLOBBERS1
	);

	return r3;
}

struct fh_sg_list {
	uint64_t source;   
	uint64_t target;   
	uint64_t size;     
	uint64_t reserved; 
} __attribute__ ((aligned(32)));

static inline unsigned int fh_partition_memcpy(unsigned int source,
	unsigned int target, phys_addr_t sg_list, unsigned int count)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	register uintptr_t r5 __asm__("r5");
	register uintptr_t r6 __asm__("r6");
	register uintptr_t r7 __asm__("r7");

	r11 = FH_HCALL_TOKEN(FH_PARTITION_MEMCPY);
	r3 = source;
	r4 = target;
	r5 = (uint32_t) sg_list;

#ifdef CONFIG_PHYS_64BIT
	r6 = sg_list >> 32;
#else
	r6 = 0;
#endif
	r7 = count;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11),
		  "+r" (r3), "+r" (r4), "+r" (r5), "+r" (r6), "+r" (r7)
		: : EV_HCALL_CLOBBERS5
	);

	return r3;
}

static inline unsigned int fh_dma_enable(unsigned int liodn)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");

	r11 = FH_HCALL_TOKEN(FH_DMA_ENABLE);
	r3 = liodn;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3)
		: : EV_HCALL_CLOBBERS1
	);

	return r3;
}

static inline unsigned int fh_dma_disable(unsigned int liodn)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");

	r11 = FH_HCALL_TOKEN(FH_DMA_DISABLE);
	r3 = liodn;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3)
		: : EV_HCALL_CLOBBERS1
	);

	return r3;
}


static inline unsigned int fh_vmpic_get_msir(unsigned int interrupt,
	unsigned int *msir_val)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");

	r11 = FH_HCALL_TOKEN(FH_VMPIC_GET_MSIR);
	r3 = interrupt;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3), "=r" (r4)
		: : EV_HCALL_CLOBBERS2
	);

	*msir_val = r4;

	return r3;
}

static inline unsigned int fh_system_reset(void)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");

	r11 = FH_HCALL_TOKEN(FH_SYSTEM_RESET);

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "=r" (r3)
		: : EV_HCALL_CLOBBERS1
	);

	return r3;
}


static inline unsigned int fh_err_get_info(int queue, uint32_t *bufsize,
	uint32_t addr_hi, uint32_t addr_lo, int peek)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");
	register uintptr_t r5 __asm__("r5");
	register uintptr_t r6 __asm__("r6");
	register uintptr_t r7 __asm__("r7");

	r11 = FH_HCALL_TOKEN(FH_ERR_GET_INFO);
	r3 = queue;
	r4 = *bufsize;
	r5 = addr_hi;
	r6 = addr_lo;
	r7 = peek;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3), "+r" (r4), "+r" (r5), "+r" (r6),
		  "+r" (r7)
		: : EV_HCALL_CLOBBERS5
	);

	*bufsize = r4;

	return r3;
}


#define FH_VCPU_RUN	0
#define FH_VCPU_IDLE	1
#define FH_VCPU_NAP	2

static inline unsigned int fh_get_core_state(unsigned int handle,
	unsigned int vcpu, unsigned int *state)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");

	r11 = FH_HCALL_TOKEN(FH_GET_CORE_STATE);
	r3 = handle;
	r4 = vcpu;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3), "+r" (r4)
		: : EV_HCALL_CLOBBERS2
	);

	*state = r4;
	return r3;
}

static inline unsigned int fh_enter_nap(unsigned int handle, unsigned int vcpu)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");

	r11 = FH_HCALL_TOKEN(FH_ENTER_NAP);
	r3 = handle;
	r4 = vcpu;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3), "+r" (r4)
		: : EV_HCALL_CLOBBERS2
	);

	return r3;
}

static inline unsigned int fh_exit_nap(unsigned int handle, unsigned int vcpu)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");
	register uintptr_t r4 __asm__("r4");

	r11 = FH_HCALL_TOKEN(FH_EXIT_NAP);
	r3 = handle;
	r4 = vcpu;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3), "+r" (r4)
		: : EV_HCALL_CLOBBERS2
	);

	return r3;
}
static inline unsigned int fh_claim_device(unsigned int handle)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");

	r11 = FH_HCALL_TOKEN(FH_CLAIM_DEVICE);
	r3 = handle;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3)
		: : EV_HCALL_CLOBBERS1
	);

	return r3;
}

static inline unsigned int fh_partition_stop_dma(unsigned int handle)
{
	register uintptr_t r11 __asm__("r11");
	register uintptr_t r3 __asm__("r3");

	r11 = FH_HCALL_TOKEN(FH_PARTITION_STOP_DMA);
	r3 = handle;

	__asm__ __volatile__ ("sc 1"
		: "+r" (r11), "+r" (r3)
		: : EV_HCALL_CLOBBERS1
	);

	return r3;
}
#endif
