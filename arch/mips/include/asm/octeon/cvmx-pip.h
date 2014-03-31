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


#ifndef __CVMX_PIP_H__
#define __CVMX_PIP_H__

#include "cvmx-wqe.h"
#include "cvmx-fpa.h"
#include "cvmx-pip-defs.h"

#define CVMX_PIP_NUM_INPUT_PORTS                40
#define CVMX_PIP_NUM_WATCHERS                   4

typedef enum {
	CVMX_PIP_L4_NO_ERR = 0ull,
	CVMX_PIP_L4_MAL_ERR = 1ull,
	
	CVMX_PIP_CHK_ERR = 2ull,
	CVMX_PIP_L4_LENGTH_ERR = 3ull,
	
	CVMX_PIP_BAD_PRT_ERR = 4ull,
	
	CVMX_PIP_TCP_FLG8_ERR = 8ull,
	
	CVMX_PIP_TCP_FLG9_ERR = 9ull,
	
	CVMX_PIP_TCP_FLG10_ERR = 10ull,
	
	CVMX_PIP_TCP_FLG11_ERR = 11ull,
	
	CVMX_PIP_TCP_FLG12_ERR = 12ull,
	
	CVMX_PIP_TCP_FLG13_ERR = 13ull
} cvmx_pip_l4_err_t;

typedef enum {

	CVMX_PIP_IP_NO_ERR = 0ull,
	
	CVMX_PIP_NOT_IP = 1ull,
	
	CVMX_PIP_IPV4_HDR_CHK = 2ull,
	
	CVMX_PIP_IP_MAL_HDR = 3ull,
	
	CVMX_PIP_IP_MAL_PKT = 4ull,
	
	CVMX_PIP_TTL_HOP = 5ull,
	
	CVMX_PIP_OPTS = 6ull
} cvmx_pip_ip_exc_t;

typedef enum {
	
	CVMX_PIP_RX_NO_ERR = 0ull,
	CVMX_PIP_PARTIAL_ERR = 1ull,
	
	CVMX_PIP_JABBER_ERR = 2ull,
	CVMX_PIP_OVER_FCS_ERR = 3ull,
	
	CVMX_PIP_OVER_ERR = 4ull,
	CVMX_PIP_ALIGN_ERR = 5ull,
	CVMX_PIP_UNDER_FCS_ERR = 6ull,
	
	CVMX_PIP_GMX_FCS_ERR = 7ull,
	
	CVMX_PIP_UNDER_ERR = 8ull,
	
	CVMX_PIP_EXTEND_ERR = 9ull,
	CVMX_PIP_LENGTH_ERR = 10ull,
	
	CVMX_PIP_DAT_ERR = 11ull,
	
	CVMX_PIP_DIP_ERR = 11ull,
	CVMX_PIP_SKIP_ERR = 12ull,
	CVMX_PIP_NIBBLE_ERR = 13ull,
	
	CVMX_PIP_PIP_FCS = 16L,
	CVMX_PIP_PIP_SKIP_ERR = 17L,
	CVMX_PIP_PIP_L2_MAL_HDR = 18L
} cvmx_pip_rcv_err_t;

typedef union {
	cvmx_pip_l4_err_t l4_err;
	cvmx_pip_ip_exc_t ip_exc;
	cvmx_pip_rcv_err_t rcv_err;
} cvmx_pip_err_t;

typedef struct {
	
	uint32_t dropped_octets;
	
	uint32_t dropped_packets;
	
	uint32_t pci_raw_packets;
	
	uint32_t octets;
	
	uint32_t packets;
	uint32_t multicast_packets;
	uint32_t broadcast_packets;
	
	uint32_t len_64_packets;
	
	uint32_t len_65_127_packets;
	
	uint32_t len_128_255_packets;
	
	uint32_t len_256_511_packets;
	
	uint32_t len_512_1023_packets;
	
	uint32_t len_1024_1518_packets;
	
	uint32_t len_1519_max_packets;
	
	uint32_t fcs_align_err_packets;
	
	uint32_t runt_packets;
	
	uint32_t runt_crc_packets;
	
	uint32_t oversize_packets;
	
	uint32_t oversize_crc_packets;
	
	uint32_t inb_packets;
	uint64_t inb_octets;
	
	uint16_t inb_errors;
} cvmx_pip_port_status_t;

typedef union {
	uint64_t u64;
	struct {
		uint64_t rawfull:1;
		
		uint64_t reserved0:5;
		
		uint64_t parse_mode:2;
		
		uint64_t reserved1:1;
		uint64_t skip_len:7;
		
		uint64_t reserved2:6;
		
		uint64_t qos:3;
		
		uint64_t grp:4;
		uint64_t rs:1;
		
		uint64_t tag_type:2;
		
		uint64_t tag:32;
	} s;
} cvmx_pip_pkt_inst_hdr_t;


static inline void cvmx_pip_config_port(uint64_t port_num,
					union cvmx_pip_prt_cfgx port_cfg,
					union cvmx_pip_prt_tagx port_tag_cfg)
{
	cvmx_write_csr(CVMX_PIP_PRT_CFGX(port_num), port_cfg.u64);
	cvmx_write_csr(CVMX_PIP_PRT_TAGX(port_num), port_tag_cfg.u64);
}
#if 0
static inline void cvmx_pip_config_watcher(uint64_t watcher,
					   cvmx_pip_qos_watch_types match_type,
					   uint64_t match_value, uint64_t qos)
{
	cvmx_pip_port_watcher_cfg_t watcher_config;

	watcher_config.u64 = 0;
	watcher_config.s.match_type = match_type;
	watcher_config.s.match_value = match_value;
	watcher_config.s.qos = qos;

	cvmx_write_csr(CVMX_PIP_QOS_WATCHX(watcher), watcher_config.u64);
}
#endif
static inline void cvmx_pip_config_vlan_qos(uint64_t vlan_priority,
					    uint64_t qos)
{
	union cvmx_pip_qos_vlanx pip_qos_vlanx;
	pip_qos_vlanx.u64 = 0;
	pip_qos_vlanx.s.qos = qos;
	cvmx_write_csr(CVMX_PIP_QOS_VLANX(vlan_priority), pip_qos_vlanx.u64);
}

static inline void cvmx_pip_config_diffserv_qos(uint64_t diffserv, uint64_t qos)
{
	union cvmx_pip_qos_diffx pip_qos_diffx;
	pip_qos_diffx.u64 = 0;
	pip_qos_diffx.s.qos = qos;
	cvmx_write_csr(CVMX_PIP_QOS_DIFFX(diffserv), pip_qos_diffx.u64);
}

static inline void cvmx_pip_get_port_status(uint64_t port_num, uint64_t clear,
					    cvmx_pip_port_status_t *status)
{
	union cvmx_pip_stat_ctl pip_stat_ctl;
	union cvmx_pip_stat0_prtx stat0;
	union cvmx_pip_stat1_prtx stat1;
	union cvmx_pip_stat2_prtx stat2;
	union cvmx_pip_stat3_prtx stat3;
	union cvmx_pip_stat4_prtx stat4;
	union cvmx_pip_stat5_prtx stat5;
	union cvmx_pip_stat6_prtx stat6;
	union cvmx_pip_stat7_prtx stat7;
	union cvmx_pip_stat8_prtx stat8;
	union cvmx_pip_stat9_prtx stat9;
	union cvmx_pip_stat_inb_pktsx pip_stat_inb_pktsx;
	union cvmx_pip_stat_inb_octsx pip_stat_inb_octsx;
	union cvmx_pip_stat_inb_errsx pip_stat_inb_errsx;

	pip_stat_ctl.u64 = 0;
	pip_stat_ctl.s.rdclr = clear;
	cvmx_write_csr(CVMX_PIP_STAT_CTL, pip_stat_ctl.u64);

	stat0.u64 = cvmx_read_csr(CVMX_PIP_STAT0_PRTX(port_num));
	stat1.u64 = cvmx_read_csr(CVMX_PIP_STAT1_PRTX(port_num));
	stat2.u64 = cvmx_read_csr(CVMX_PIP_STAT2_PRTX(port_num));
	stat3.u64 = cvmx_read_csr(CVMX_PIP_STAT3_PRTX(port_num));
	stat4.u64 = cvmx_read_csr(CVMX_PIP_STAT4_PRTX(port_num));
	stat5.u64 = cvmx_read_csr(CVMX_PIP_STAT5_PRTX(port_num));
	stat6.u64 = cvmx_read_csr(CVMX_PIP_STAT6_PRTX(port_num));
	stat7.u64 = cvmx_read_csr(CVMX_PIP_STAT7_PRTX(port_num));
	stat8.u64 = cvmx_read_csr(CVMX_PIP_STAT8_PRTX(port_num));
	stat9.u64 = cvmx_read_csr(CVMX_PIP_STAT9_PRTX(port_num));
	pip_stat_inb_pktsx.u64 =
	    cvmx_read_csr(CVMX_PIP_STAT_INB_PKTSX(port_num));
	pip_stat_inb_octsx.u64 =
	    cvmx_read_csr(CVMX_PIP_STAT_INB_OCTSX(port_num));
	pip_stat_inb_errsx.u64 =
	    cvmx_read_csr(CVMX_PIP_STAT_INB_ERRSX(port_num));

	status->dropped_octets = stat0.s.drp_octs;
	status->dropped_packets = stat0.s.drp_pkts;
	status->octets = stat1.s.octs;
	status->pci_raw_packets = stat2.s.raw;
	status->packets = stat2.s.pkts;
	status->multicast_packets = stat3.s.mcst;
	status->broadcast_packets = stat3.s.bcst;
	status->len_64_packets = stat4.s.h64;
	status->len_65_127_packets = stat4.s.h65to127;
	status->len_128_255_packets = stat5.s.h128to255;
	status->len_256_511_packets = stat5.s.h256to511;
	status->len_512_1023_packets = stat6.s.h512to1023;
	status->len_1024_1518_packets = stat6.s.h1024to1518;
	status->len_1519_max_packets = stat7.s.h1519;
	status->fcs_align_err_packets = stat7.s.fcs;
	status->runt_packets = stat8.s.undersz;
	status->runt_crc_packets = stat8.s.frag;
	status->oversize_packets = stat9.s.oversz;
	status->oversize_crc_packets = stat9.s.jabber;
	status->inb_packets = pip_stat_inb_pktsx.s.pkts;
	status->inb_octets = pip_stat_inb_octsx.s.octs;
	status->inb_errors = pip_stat_inb_errsx.s.errs;

	if (cvmx_octeon_is_pass1()) {
		if (status->inb_packets > status->packets)
			status->dropped_packets =
			    status->inb_packets - status->packets;
		else
			status->dropped_packets = 0;
		if (status->inb_octets - status->inb_packets * 4 >
		    status->octets)
			status->dropped_octets =
			    status->inb_octets - status->inb_packets * 4 -
			    status->octets;
		else
			status->dropped_octets = 0;
	}
}

static inline void cvmx_pip_config_crc(uint64_t interface,
				       uint64_t invert_result, uint64_t reflect,
				       uint32_t initialization_vector)
{
	if (OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)) {
		union cvmx_pip_crc_ctlx config;
		union cvmx_pip_crc_ivx pip_crc_ivx;

		config.u64 = 0;
		config.s.invres = invert_result;
		config.s.reflect = reflect;
		cvmx_write_csr(CVMX_PIP_CRC_CTLX(interface), config.u64);

		pip_crc_ivx.u64 = 0;
		pip_crc_ivx.s.iv = initialization_vector;
		cvmx_write_csr(CVMX_PIP_CRC_IVX(interface), pip_crc_ivx.u64);
	}
}

static inline void cvmx_pip_tag_mask_clear(uint64_t mask_index)
{
	uint64_t index;
	union cvmx_pip_tag_incx pip_tag_incx;
	pip_tag_incx.u64 = 0;
	pip_tag_incx.s.en = 0;
	for (index = mask_index * 16; index < (mask_index + 1) * 16; index++)
		cvmx_write_csr(CVMX_PIP_TAG_INCX(index), pip_tag_incx.u64);
}

static inline void cvmx_pip_tag_mask_set(uint64_t mask_index, uint64_t offset,
					 uint64_t len)
{
	while (len--) {
		union cvmx_pip_tag_incx pip_tag_incx;
		uint64_t index = mask_index * 16 + offset / 8;
		pip_tag_incx.u64 = cvmx_read_csr(CVMX_PIP_TAG_INCX(index));
		pip_tag_incx.s.en |= 0x80 >> (offset & 0x7);
		cvmx_write_csr(CVMX_PIP_TAG_INCX(index), pip_tag_incx.u64);
		offset++;
	}
}

#endif 
