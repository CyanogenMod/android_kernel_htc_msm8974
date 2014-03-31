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


#include <asm/octeon/octeon.h>

#include <asm/octeon/cvmx-config.h>

#include <asm/octeon/cvmx-mdio.h>
#include <asm/octeon/cvmx-helper.h>
#include <asm/octeon/cvmx-helper-board.h>

#include <asm/octeon/cvmx-gmxx-defs.h>
#include <asm/octeon/cvmx-pcsx-defs.h>

void __cvmx_interrupt_gmxx_enable(int interface);
void __cvmx_interrupt_pcsx_intx_en_reg_enable(int index, int block);
void __cvmx_interrupt_pcsxx_int_en_reg_enable(int index);

static int __cvmx_helper_sgmii_hardware_init_one_time(int interface, int index)
{
	const uint64_t clock_mhz = cvmx_sysinfo_get()->cpu_clock_hz / 1000000;
	union cvmx_pcsx_miscx_ctl_reg pcs_misc_ctl_reg;
	union cvmx_pcsx_linkx_timer_count_reg pcsx_linkx_timer_count_reg;
	union cvmx_gmxx_prtx_cfg gmxx_prtx_cfg;

	
	gmxx_prtx_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(index, interface));
	gmxx_prtx_cfg.s.en = 0;
	cvmx_write_csr(CVMX_GMXX_PRTX_CFG(index, interface), gmxx_prtx_cfg.u64);

	pcs_misc_ctl_reg.u64 =
	    cvmx_read_csr(CVMX_PCSX_MISCX_CTL_REG(index, interface));
	pcsx_linkx_timer_count_reg.u64 =
	    cvmx_read_csr(CVMX_PCSX_LINKX_TIMER_COUNT_REG(index, interface));
	if (pcs_misc_ctl_reg.s.mode) {
		
		pcsx_linkx_timer_count_reg.s.count =
		    (10000ull * clock_mhz) >> 10;
	} else {
		
		pcsx_linkx_timer_count_reg.s.count =
		    (1600ull * clock_mhz) >> 10;
	}
	cvmx_write_csr(CVMX_PCSX_LINKX_TIMER_COUNT_REG(index, interface),
		       pcsx_linkx_timer_count_reg.u64);

	if (pcs_misc_ctl_reg.s.mode) {
		
		union cvmx_pcsx_anx_adv_reg pcsx_anx_adv_reg;
		pcsx_anx_adv_reg.u64 =
		    cvmx_read_csr(CVMX_PCSX_ANX_ADV_REG(index, interface));
		pcsx_anx_adv_reg.s.rem_flt = 0;
		pcsx_anx_adv_reg.s.pause = 3;
		pcsx_anx_adv_reg.s.hfd = 1;
		pcsx_anx_adv_reg.s.fd = 1;
		cvmx_write_csr(CVMX_PCSX_ANX_ADV_REG(index, interface),
			       pcsx_anx_adv_reg.u64);
	} else {
		union cvmx_pcsx_miscx_ctl_reg pcsx_miscx_ctl_reg;
		pcsx_miscx_ctl_reg.u64 =
		    cvmx_read_csr(CVMX_PCSX_MISCX_CTL_REG(index, interface));
		if (pcsx_miscx_ctl_reg.s.mac_phy) {
			
			union cvmx_pcsx_sgmx_an_adv_reg pcsx_sgmx_an_adv_reg;
			pcsx_sgmx_an_adv_reg.u64 =
			    cvmx_read_csr(CVMX_PCSX_SGMX_AN_ADV_REG
					  (index, interface));
			pcsx_sgmx_an_adv_reg.s.link = 1;
			pcsx_sgmx_an_adv_reg.s.dup = 1;
			pcsx_sgmx_an_adv_reg.s.speed = 2;
			cvmx_write_csr(CVMX_PCSX_SGMX_AN_ADV_REG
				       (index, interface),
				       pcsx_sgmx_an_adv_reg.u64);
		} else {
			
		}
	}
	return 0;
}

static int __cvmx_helper_sgmii_hardware_init_link(int interface, int index)
{
	union cvmx_pcsx_mrx_control_reg control_reg;

	control_reg.u64 =
	    cvmx_read_csr(CVMX_PCSX_MRX_CONTROL_REG(index, interface));
	if (cvmx_sysinfo_get()->board_type != CVMX_BOARD_TYPE_SIM) {
		control_reg.s.reset = 1;
		cvmx_write_csr(CVMX_PCSX_MRX_CONTROL_REG(index, interface),
			       control_reg.u64);
		if (CVMX_WAIT_FOR_FIELD64
		    (CVMX_PCSX_MRX_CONTROL_REG(index, interface),
		     union cvmx_pcsx_mrx_control_reg, reset, ==, 0, 10000)) {
			cvmx_dprintf("SGMII%d: Timeout waiting for port %d "
				     "to finish reset\n",
			     interface, index);
			return -1;
		}
	}

	control_reg.s.rst_an = 1;
	control_reg.s.an_en = 1;
	control_reg.s.pwr_dn = 0;
	cvmx_write_csr(CVMX_PCSX_MRX_CONTROL_REG(index, interface),
		       control_reg.u64);

	if ((cvmx_sysinfo_get()->board_type != CVMX_BOARD_TYPE_SIM) &&
	    CVMX_WAIT_FOR_FIELD64(CVMX_PCSX_MRX_STATUS_REG(index, interface),
				  union cvmx_pcsx_mrx_status_reg, an_cpt, ==, 1,
				  10000)) {
		
		return -1;
	}
	return 0;
}

static int __cvmx_helper_sgmii_hardware_init_link_speed(int interface,
							int index,
							cvmx_helper_link_info_t
							link_info)
{
	int is_enabled;
	union cvmx_gmxx_prtx_cfg gmxx_prtx_cfg;
	union cvmx_pcsx_miscx_ctl_reg pcsx_miscx_ctl_reg;

	
	gmxx_prtx_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(index, interface));
	is_enabled = gmxx_prtx_cfg.s.en;
	gmxx_prtx_cfg.s.en = 0;
	cvmx_write_csr(CVMX_GMXX_PRTX_CFG(index, interface), gmxx_prtx_cfg.u64);

	
	if (CVMX_WAIT_FOR_FIELD64
	    (CVMX_GMXX_PRTX_CFG(index, interface), union cvmx_gmxx_prtx_cfg,
	     rx_idle, ==, 1, 10000)
	    || CVMX_WAIT_FOR_FIELD64(CVMX_GMXX_PRTX_CFG(index, interface),
				     union cvmx_gmxx_prtx_cfg, tx_idle, ==, 1,
				     10000)) {
		cvmx_dprintf
		    ("SGMII%d: Timeout waiting for port %d to be idle\n",
		     interface, index);
		return -1;
	}

	
	gmxx_prtx_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(index, interface));

	pcsx_miscx_ctl_reg.u64 =
	    cvmx_read_csr(CVMX_PCSX_MISCX_CTL_REG(index, interface));

	pcsx_miscx_ctl_reg.s.gmxeno = !link_info.s.link_up;

	
	if (link_info.s.link_up)
		gmxx_prtx_cfg.s.duplex = link_info.s.full_duplex;

	
	switch (link_info.s.speed) {
	case 10:
		gmxx_prtx_cfg.s.speed = 0;
		gmxx_prtx_cfg.s.speed_msb = 1;
		gmxx_prtx_cfg.s.slottime = 0;
		
		pcsx_miscx_ctl_reg.s.samp_pt = 25;
		cvmx_write_csr(CVMX_GMXX_TXX_SLOT(index, interface), 64);
		cvmx_write_csr(CVMX_GMXX_TXX_BURST(index, interface), 0);
		break;
	case 100:
		gmxx_prtx_cfg.s.speed = 0;
		gmxx_prtx_cfg.s.speed_msb = 0;
		gmxx_prtx_cfg.s.slottime = 0;
		pcsx_miscx_ctl_reg.s.samp_pt = 0x5;
		cvmx_write_csr(CVMX_GMXX_TXX_SLOT(index, interface), 64);
		cvmx_write_csr(CVMX_GMXX_TXX_BURST(index, interface), 0);
		break;
	case 1000:
		gmxx_prtx_cfg.s.speed = 1;
		gmxx_prtx_cfg.s.speed_msb = 0;
		gmxx_prtx_cfg.s.slottime = 1;
		pcsx_miscx_ctl_reg.s.samp_pt = 1;
		cvmx_write_csr(CVMX_GMXX_TXX_SLOT(index, interface), 512);
		cvmx_write_csr(CVMX_GMXX_TXX_BURST(index, interface), 8192);
		break;
	default:
		break;
	}

	
	cvmx_write_csr(CVMX_PCSX_MISCX_CTL_REG(index, interface),
		       pcsx_miscx_ctl_reg.u64);

	
	cvmx_write_csr(CVMX_GMXX_PRTX_CFG(index, interface), gmxx_prtx_cfg.u64);

	
	gmxx_prtx_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(index, interface));

	
	gmxx_prtx_cfg.s.en = is_enabled;
	cvmx_write_csr(CVMX_GMXX_PRTX_CFG(index, interface), gmxx_prtx_cfg.u64);

	return 0;
}

static int __cvmx_helper_sgmii_hardware_init(int interface, int num_ports)
{
	int index;

	__cvmx_helper_setup_gmx(interface, num_ports);

	for (index = 0; index < num_ports; index++) {
		int ipd_port = cvmx_helper_get_ipd_port(interface, index);
		__cvmx_helper_sgmii_hardware_init_one_time(interface, index);
		__cvmx_helper_sgmii_link_set(ipd_port,
					     __cvmx_helper_sgmii_link_get
					     (ipd_port));

	}

	return 0;
}

int __cvmx_helper_sgmii_enumerate(int interface)
{
	return 4;
}
int __cvmx_helper_sgmii_probe(int interface)
{
	union cvmx_gmxx_inf_mode mode;

	mode.u64 = cvmx_read_csr(CVMX_GMXX_INF_MODE(interface));
	mode.s.en = 1;
	cvmx_write_csr(CVMX_GMXX_INF_MODE(interface), mode.u64);
	return __cvmx_helper_sgmii_enumerate(interface);
}

int __cvmx_helper_sgmii_enable(int interface)
{
	int num_ports = cvmx_helper_ports_on_interface(interface);
	int index;

	__cvmx_helper_sgmii_hardware_init(interface, num_ports);

	for (index = 0; index < num_ports; index++) {
		union cvmx_gmxx_prtx_cfg gmxx_prtx_cfg;
		gmxx_prtx_cfg.u64 =
		    cvmx_read_csr(CVMX_GMXX_PRTX_CFG(index, interface));
		gmxx_prtx_cfg.s.en = 1;
		cvmx_write_csr(CVMX_GMXX_PRTX_CFG(index, interface),
			       gmxx_prtx_cfg.u64);
		__cvmx_interrupt_pcsx_intx_en_reg_enable(index, interface);
	}
	__cvmx_interrupt_pcsxx_int_en_reg_enable(interface);
	__cvmx_interrupt_gmxx_enable(interface);
	return 0;
}

cvmx_helper_link_info_t __cvmx_helper_sgmii_link_get(int ipd_port)
{
	cvmx_helper_link_info_t result;
	union cvmx_pcsx_miscx_ctl_reg pcs_misc_ctl_reg;
	int interface = cvmx_helper_get_interface_num(ipd_port);
	int index = cvmx_helper_get_interface_index_num(ipd_port);
	union cvmx_pcsx_mrx_control_reg pcsx_mrx_control_reg;

	result.u64 = 0;

	if (cvmx_sysinfo_get()->board_type == CVMX_BOARD_TYPE_SIM) {
		
		result.s.link_up = 1;
		result.s.full_duplex = 1;
		result.s.speed = 1000;
		return result;
	}

	pcsx_mrx_control_reg.u64 =
	    cvmx_read_csr(CVMX_PCSX_MRX_CONTROL_REG(index, interface));
	if (pcsx_mrx_control_reg.s.loopbck1) {
		
		result.s.link_up = 1;
		result.s.full_duplex = 1;
		result.s.speed = 1000;
		return result;
	}

	pcs_misc_ctl_reg.u64 =
	    cvmx_read_csr(CVMX_PCSX_MISCX_CTL_REG(index, interface));
	if (pcs_misc_ctl_reg.s.mode) {
		
		
	} else {
		union cvmx_pcsx_miscx_ctl_reg pcsx_miscx_ctl_reg;
		pcsx_miscx_ctl_reg.u64 =
		    cvmx_read_csr(CVMX_PCSX_MISCX_CTL_REG(index, interface));
		if (pcsx_miscx_ctl_reg.s.mac_phy) {
			
			union cvmx_pcsx_mrx_status_reg pcsx_mrx_status_reg;
			union cvmx_pcsx_anx_results_reg pcsx_anx_results_reg;

			pcsx_mrx_status_reg.u64 =
			    cvmx_read_csr(CVMX_PCSX_MRX_STATUS_REG
					  (index, interface));
			if (pcsx_mrx_status_reg.s.lnk_st == 0) {
				if (__cvmx_helper_sgmii_hardware_init_link
				    (interface, index) != 0)
					return result;
			}

			
			pcsx_anx_results_reg.u64 =
			    cvmx_read_csr(CVMX_PCSX_ANX_RESULTS_REG
					  (index, interface));
			if (pcsx_anx_results_reg.s.an_cpt) {
				result.s.full_duplex =
				    pcsx_anx_results_reg.s.dup;
				result.s.link_up =
				    pcsx_anx_results_reg.s.link_ok;
				switch (pcsx_anx_results_reg.s.spd) {
				case 0:
					result.s.speed = 10;
					break;
				case 1:
					result.s.speed = 100;
					break;
				case 2:
					result.s.speed = 1000;
					break;
				default:
					result.s.speed = 0;
					result.s.link_up = 0;
					break;
				}
			} else {
				result.s.speed = 0;
				result.s.link_up = 0;
			}
		} else {	

			result = __cvmx_helper_board_link_get(ipd_port);
		}
	}
	return result;
}

int __cvmx_helper_sgmii_link_set(int ipd_port,
				 cvmx_helper_link_info_t link_info)
{
	int interface = cvmx_helper_get_interface_num(ipd_port);
	int index = cvmx_helper_get_interface_index_num(ipd_port);
	__cvmx_helper_sgmii_hardware_init_link(interface, index);
	return __cvmx_helper_sgmii_hardware_init_link_speed(interface, index,
							    link_info);
}

int __cvmx_helper_sgmii_configure_loopback(int ipd_port, int enable_internal,
					   int enable_external)
{
	int interface = cvmx_helper_get_interface_num(ipd_port);
	int index = cvmx_helper_get_interface_index_num(ipd_port);
	union cvmx_pcsx_mrx_control_reg pcsx_mrx_control_reg;
	union cvmx_pcsx_miscx_ctl_reg pcsx_miscx_ctl_reg;

	pcsx_mrx_control_reg.u64 =
	    cvmx_read_csr(CVMX_PCSX_MRX_CONTROL_REG(index, interface));
	pcsx_mrx_control_reg.s.loopbck1 = enable_internal;
	cvmx_write_csr(CVMX_PCSX_MRX_CONTROL_REG(index, interface),
		       pcsx_mrx_control_reg.u64);

	pcsx_miscx_ctl_reg.u64 =
	    cvmx_read_csr(CVMX_PCSX_MISCX_CTL_REG(index, interface));
	pcsx_miscx_ctl_reg.s.loopbck2 = enable_external;
	cvmx_write_csr(CVMX_PCSX_MISCX_CTL_REG(index, interface),
		       pcsx_miscx_ctl_reg.u64);

	__cvmx_helper_sgmii_hardware_init_link(interface, index);
	return 0;
}
