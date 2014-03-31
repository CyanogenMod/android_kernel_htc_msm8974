/***********************license start***************
 * Author: Cavium Networks
 *
 * Contact: support@caviumnetworks.com
 * This file is part of the OCTEON SDK
 *
 * Copyright (c) 2003-2008 Cavium Networks
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, Version 2, as
 * published by the Free Software Foundation.
 *
 * This file is distributed in the hope that it will be useful, but
 * AS-IS and WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE, TITLE, or
 * NONINFRINGEMENT.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 * or visit http://www.gnu.org/licenses/.
 *
 * This file may also be available under a different license from Cavium.
 * Contact Cavium Networks for more information
 ***********************license end**************************************/

#ifndef __CVMX_H__
#define __CVMX_H__

#include <linux/kernel.h>
#include <linux/string.h>

enum cvmx_mips_space {
	CVMX_MIPS_SPACE_XKSEG = 3LL,
	CVMX_MIPS_SPACE_XKPHYS = 2LL,
	CVMX_MIPS_SPACE_XSSEG = 1LL,
	CVMX_MIPS_SPACE_XUSEG = 0LL
};

#define CVMX_MIPS32_SPACE_KSEG0 1l
#define CVMX_ADD_SEG32(segment, add) \
	(((int32_t)segment << 31) | (int32_t)(add))

#define CVMX_IO_SEG CVMX_MIPS_SPACE_XKPHYS

#define CVMX_ADD_SEG(segment, add) \
	((((uint64_t)segment) << 62) | (add))
#ifndef CVMX_ADD_IO_SEG
#define CVMX_ADD_IO_SEG(add) CVMX_ADD_SEG(CVMX_IO_SEG, (add))
#endif

#include "cvmx-asm.h"
#include "cvmx-packet.h"
#include "cvmx-sysinfo.h"

#include "cvmx-ciu-defs.h"
#include "cvmx-gpio-defs.h"
#include "cvmx-iob-defs.h"
#include "cvmx-ipd-defs.h"
#include "cvmx-l2c-defs.h"
#include "cvmx-l2d-defs.h"
#include "cvmx-l2t-defs.h"
#include "cvmx-led-defs.h"
#include "cvmx-mio-defs.h"
#include "cvmx-pow-defs.h"

#include "cvmx-bootinfo.h"
#include "cvmx-bootmem.h"
#include "cvmx-l2c.h"

#ifndef CVMX_ENABLE_DEBUG_PRINTS
#define CVMX_ENABLE_DEBUG_PRINTS 1
#endif

#if CVMX_ENABLE_DEBUG_PRINTS
#define cvmx_dprintf        printk
#else
#define cvmx_dprintf(...)   {}
#endif

#define CVMX_MAX_CORES          (16)
#define CVMX_CACHE_LINE_SIZE    (128)	
#define CVMX_CACHE_LINE_MASK    (CVMX_CACHE_LINE_SIZE - 1)	
#define CVMX_CACHE_LINE_ALIGNED __attribute__ ((aligned(CVMX_CACHE_LINE_SIZE)))
#define CAST64(v) ((long long)(long)(v))
#define CASTPTR(type, v) ((type *)(long)(v))

static inline uint32_t cvmx_get_proc_id(void) __attribute__ ((pure));
static inline uint32_t cvmx_get_proc_id(void)
{
	uint32_t id;
	asm("mfc0 %0, $15,0" : "=r"(id));
	return id;
}

#define CVMX_TMP_STR(x) CVMX_TMP_STR2(x)
#define CVMX_TMP_STR2(x) #x

 static inline uint64_t cvmx_build_mask(uint64_t bits)
{
	return ~((~0x0ull) << bits);
}

static inline uint64_t cvmx_build_io_address(uint64_t major_did,
					     uint64_t sub_did)
{
	return (0x1ull << 48) | (major_did << 43) | (sub_did << 40);
}

static inline uint64_t cvmx_build_bits(uint64_t high_bit,
				       uint64_t low_bit, uint64_t value)
{
	return (value & cvmx_build_mask(high_bit - low_bit + 1)) << low_bit;
}

static inline uint64_t cvmx_ptr_to_phys(void *ptr)
{
	if (sizeof(void *) == 8) {
		if ((CAST64(ptr) >> 62) == 3)
			return CAST64(ptr) & cvmx_build_mask(30);
		else
			return CAST64(ptr) & cvmx_build_mask(40);
	} else {
		return (long)(ptr) & 0x1fffffff;
	}
}

static inline void *cvmx_phys_to_ptr(uint64_t physical_address)
{
	if (sizeof(void *) == 8) {
		
		return CASTPTR(void,
			       CVMX_ADD_SEG(CVMX_MIPS_SPACE_XKPHYS,
					    physical_address));
	} else {
		return CASTPTR(void,
			       CVMX_ADD_SEG32(CVMX_MIPS32_SPACE_KSEG0,
					      physical_address));
	}
}


#define CVMX_BUILD_WRITE64(TYPE, ST)                                    \
static inline void cvmx_write64_##TYPE(uint64_t addr, TYPE##_t val)     \
{                                                                       \
    *CASTPTR(volatile TYPE##_t, addr) = val;                            \
}



#define CVMX_BUILD_READ64(TYPE, LT)                                     \
static inline TYPE##_t cvmx_read64_##TYPE(uint64_t addr)                \
{                                                                       \
	return *CASTPTR(volatile TYPE##_t, addr);			\
}


CVMX_BUILD_WRITE64(int64, "sd");
CVMX_BUILD_WRITE64(int32, "sw");
CVMX_BUILD_WRITE64(int16, "sh");
CVMX_BUILD_WRITE64(int8, "sb");
CVMX_BUILD_WRITE64(uint64, "sd");
CVMX_BUILD_WRITE64(uint32, "sw");
CVMX_BUILD_WRITE64(uint16, "sh");
CVMX_BUILD_WRITE64(uint8, "sb");
#define cvmx_write64 cvmx_write64_uint64

CVMX_BUILD_READ64(int64, "ld");
CVMX_BUILD_READ64(int32, "lw");
CVMX_BUILD_READ64(int16, "lh");
CVMX_BUILD_READ64(int8, "lb");
CVMX_BUILD_READ64(uint64, "ld");
CVMX_BUILD_READ64(uint32, "lw");
CVMX_BUILD_READ64(uint16, "lhu");
CVMX_BUILD_READ64(uint8, "lbu");
#define cvmx_read64 cvmx_read64_uint64


static inline void cvmx_write_csr(uint64_t csr_addr, uint64_t val)
{
	cvmx_write64(csr_addr, val);

	if (((csr_addr >> 40) & 0x7ffff) == (0x118))
		cvmx_read64(CVMX_MIO_BOOT_BIST_STAT);
}

static inline void cvmx_write_io(uint64_t io_addr, uint64_t val)
{
	cvmx_write64(io_addr, val);

}

static inline uint64_t cvmx_read_csr(uint64_t csr_addr)
{
	uint64_t val = cvmx_read64(csr_addr);
	return val;
}


static inline void cvmx_send_single(uint64_t data)
{
	const uint64_t CVMX_IOBDMA_SENDSINGLE = 0xffffffffffffa200ull;
	cvmx_write64(CVMX_IOBDMA_SENDSINGLE, data);
}

static inline void cvmx_read_csr_async(uint64_t scraddr, uint64_t csr_addr)
{
	union {
		uint64_t u64;
		struct {
			uint64_t scraddr:8;
			uint64_t len:8;
			uint64_t addr:48;
		} s;
	} addr;
	addr.u64 = csr_addr;
	addr.s.scraddr = scraddr >> 3;
	addr.s.len = 1;
	cvmx_send_single(addr.u64);
}

static inline int cvmx_octeon_is_pass1(void)
{
#if OCTEON_IS_COMMON_BINARY()
	return 0;	
#else
#if OCTEON_IS_MODEL(OCTEON_CN38XX)
	return cvmx_get_proc_id() == OCTEON_CN38XX_PASS1;
#else
	return 0;	
#endif
#endif
}

static inline unsigned int cvmx_get_core_num(void)
{
	unsigned int core_num;
	CVMX_RDHWRNV(core_num, 0);
	return core_num;
}

static inline uint32_t cvmx_pop(uint32_t val)
{
	uint32_t pop;
	CVMX_POP(pop, val);
	return pop;
}

static inline int cvmx_dpop(uint64_t val)
{
	int pop;
	CVMX_DPOP(pop, val);
	return pop;
}


static inline uint64_t cvmx_get_cycle(void)
{
	uint64_t cycle;
	CVMX_RDHWR(cycle, 31);
	return cycle;
}

static inline void cvmx_wait(uint64_t cycles)
{
	uint64_t done = cvmx_get_cycle() + cycles;

	while (cvmx_get_cycle() < done)
		; 
}

static inline uint64_t cvmx_get_cycle_global(void)
{
	if (cvmx_octeon_is_pass1())
		return 0;
	else
		return cvmx_read64(CVMX_IPD_CLK_COUNT);
}

#define CVMX_WAIT_FOR_FIELD64(address, type, field, op, value, timeout_usec)\
    (									\
{									\
	int result;							\
	do {								\
		uint64_t done = cvmx_get_cycle() + (uint64_t)timeout_usec * \
			cvmx_sysinfo_get()->cpu_clock_hz / 1000000;	\
		type c;							\
		while (1) {						\
			c.u64 = cvmx_read_csr(address);			\
			if ((c.s.field) op(value)) {			\
				result = 0;				\
				break;					\
			} else if (cvmx_get_cycle() > done) {		\
				result = -1;				\
				break;					\
			} else						\
				cvmx_wait(100);				\
		}							\
	} while (0);							\
	result;								\
})


static inline void cvmx_reset_octeon(void)
{
	union cvmx_ciu_soft_rst ciu_soft_rst;
	ciu_soft_rst.u64 = 0;
	ciu_soft_rst.s.soft_rst = 1;
	cvmx_write_csr(CVMX_CIU_SOFT_RST, ciu_soft_rst.u64);
}

static inline uint32_t cvmx_octeon_num_cores(void)
{
	uint32_t ciu_fuse = (uint32_t) cvmx_read_csr(CVMX_CIU_FUSE) & 0xffff;
	return cvmx_pop(ciu_fuse);
}

static uint8_t cvmx_fuse_read_byte(int byte_addr)
{
	union cvmx_mio_fus_rcmd read_cmd;

	read_cmd.u64 = 0;
	read_cmd.s.addr = byte_addr;
	read_cmd.s.pend = 1;
	cvmx_write_csr(CVMX_MIO_FUS_RCMD, read_cmd.u64);
	while ((read_cmd.u64 = cvmx_read_csr(CVMX_MIO_FUS_RCMD))
	       && read_cmd.s.pend)
		;
	return read_cmd.s.dat;
}

static inline int cvmx_fuse_read(int fuse)
{
	return (cvmx_fuse_read_byte(fuse >> 3) >> (fuse & 0x7)) & 1;
}

static inline int cvmx_octeon_model_CN36XX(void)
{
	return OCTEON_IS_MODEL(OCTEON_CN38XX)
		&& !cvmx_octeon_is_pass1()
		&& cvmx_fuse_read(264);
}

static inline int cvmx_octeon_zip_present(void)
{
	return octeon_has_feature(OCTEON_FEATURE_ZIP);
}

static inline int cvmx_octeon_dfa_present(void)
{
	if (!OCTEON_IS_MODEL(OCTEON_CN38XX)
	    && !OCTEON_IS_MODEL(OCTEON_CN31XX)
	    && !OCTEON_IS_MODEL(OCTEON_CN58XX))
		return 0;
	else if (OCTEON_IS_MODEL(OCTEON_CN3020))
		return 0;
	else if (cvmx_octeon_is_pass1())
		return 1;
	else
		return !cvmx_fuse_read(120);
}

static inline int cvmx_octeon_crypto_present(void)
{
	return octeon_has_feature(OCTEON_FEATURE_CRYPTO);
}

#endif 
