/*
 * The MobiCore Driver Kernel Module is a Linux device driver, which represents
 * the command proxy on the lowest layer to the secure world (Swd). Additional
 * services like memory allocation via mmap and generation of a L2 tables for
 * given virtual memory are also supported. IRQ functionality receives
 * information from the SWd in the non secure world (NWd).
 * As customary the driver is handled as linux device driver with "open",
 * "close" and "ioctl" commands. Access to the driver is possible after the
 * device "/dev/mobicore" has been opened.
 * The MobiCore Driver Kernel Module must be installed via
 * "insmod mcDrvModule.ko".
 *
 * <-- Copyright Giesecke & Devrient GmbH 2010-2012 -->
 * <-- Copyright Trustonic Limited 2013 -->
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

#ifndef _MC_LINUX_H_
#define _MC_LINUX_H_

#include "version.h"

#ifndef __KERNEL__
#include <stdint.h>
#endif

#define MC_ADMIN_DEVNODE	"mobicore"
#define MC_USER_DEVNODE		"mobicore-user"

struct mc_ioctl_init {
	
	uint32_t  nq_offset;
	
	uint32_t  nq_length;
	
	uint32_t  mcp_offset;
	
	uint32_t  mcp_length;
};

struct mc_ioctl_info {
	uint32_t  ext_info_id;	
	uint32_t  state;	
	uint32_t  ext_info;	
};

struct mc_ioctl_map {
	size_t	      len;	
	uint32_t      handle;	
	unsigned long addr;	
	unsigned long phys_addr;
	bool	      reused;	
};

struct mc_ioctl_reg_wsm {
	uint32_t buffer;	
	uint32_t len;		
	uint32_t pid;		
	uint32_t handle;	
	uint32_t table_phys;	
};


struct mc_ioctl_execute {
	
	uint32_t phys_start_addr;
	
	uint32_t length;
};

struct mc_ioctl_resolv_cont_wsm {
	
	uint32_t handle;
	
	uint32_t phys;
	
	uint32_t length;
	
	int32_t fd;
};

struct mc_ioctl_resolv_wsm {
	
	uint32_t handle;
	
	int32_t fd;
	
	uint32_t phys;
};


#define MC_IOC_MAGIC	'M'

#define MC_IO_INIT		_IOWR(MC_IOC_MAGIC, 0, struct mc_ioctl_init)
#define MC_IO_INFO		_IOWR(MC_IOC_MAGIC, 1, struct mc_ioctl_info)
#define MC_IO_VERSION		_IOR(MC_IOC_MAGIC, 2, uint32_t)
#define MC_IO_YIELD		_IO(MC_IOC_MAGIC, 3)
#define MC_IO_NSIQ		_IO(MC_IOC_MAGIC, 4)
#define MC_IO_FREE		_IO(MC_IOC_MAGIC, 5)
#define MC_IO_REG_WSM		_IOWR(MC_IOC_MAGIC, 6, struct mc_ioctl_reg_wsm)
#define MC_IO_UNREG_WSM		_IO(MC_IOC_MAGIC, 7)
#define MC_IO_LOCK_WSM		_IO(MC_IOC_MAGIC, 8)
#define MC_IO_UNLOCK_WSM	_IO(MC_IOC_MAGIC, 9)
#define MC_IO_EXECUTE		_IOWR(MC_IOC_MAGIC, 10, struct mc_ioctl_execute)

#define MC_IO_MAP_WSM		_IOWR(MC_IOC_MAGIC, 11, struct mc_ioctl_map)
#define MC_IO_MAP_MCI		_IOWR(MC_IOC_MAGIC, 12, struct mc_ioctl_map)
#define MC_IO_MAP_PWSM		_IOWR(MC_IOC_MAGIC, 13, struct mc_ioctl_map)

#define MC_IO_CLEAN_WSM		_IO(MC_IOC_MAGIC, 14)

#define MC_IO_RESOLVE_WSM	_IOWR(MC_IOC_MAGIC, 15, \
					struct mc_ioctl_resolv_wsm)

#define MC_IO_RESOLVE_CONT_WSM	_IOWR(MC_IOC_MAGIC, 16, \
					struct mc_ioctl_resolv_cont_wsm)

#define MC_IO_LOG_SETUP		_IO(MC_IOC_MAGIC, 17)

#endif 
