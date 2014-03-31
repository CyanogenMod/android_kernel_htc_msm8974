/**********************************************************************
 * Author: Cavium Networks
 *
 * Contact: support@caviumnetworks.com
 * This file is part of the OCTEON SDK
 *
 * Copyright (c) 2003-2007 Cavium Networks
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
**********************************************************************/

#ifndef __ETHERNET_DEFINES_H__
#define __ETHERNET_DEFINES_H__

#include <asm/octeon/cvmx-config.h>


#define OCTEON_ETHERNET_VERSION "1.9"

#ifndef CONFIG_CAVIUM_RESERVE32
#define CONFIG_CAVIUM_RESERVE32 0
#endif

#define USE_SKBUFFS_IN_HW           1
#ifdef CONFIG_NETFILTER
#define REUSE_SKBUFFS_WITHOUT_FREE  0
#else
#define REUSE_SKBUFFS_WITHOUT_FREE  1
#endif

#define USE_HW_TCPUDP_CHECKSUM      1

#define USE_RED                     1
#define USE_ASYNC_IOBDMA            (CONFIG_CAVIUM_OCTEON_CVMSEG_SIZE > 0)

#define USE_10MBPS_PREAMBLE_WORKAROUND 1
#define DONT_WRITEBACK(x)           (x)

#define MAX_OUT_QUEUE_DEPTH 1000

#define FAU_TOTAL_TX_TO_CLEAN (CVMX_FAU_REG_END - sizeof(uint32_t))
#define FAU_NUM_PACKET_BUFFERS_TO_FREE (FAU_TOTAL_TX_TO_CLEAN - sizeof(uint32_t))

#define TOTAL_NUMBER_OF_PORTS       (CVMX_PIP_NUM_INPUT_PORTS+1)


#endif 
