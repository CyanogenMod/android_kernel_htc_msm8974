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

#include <asm/octeon/cvmx-helper.h>

#include <asm/octeon/cvmx-pko-defs.h>
#include <asm/octeon/cvmx-gmxx-defs.h>
#include <asm/octeon/cvmx-pcsxx-defs.h>

void __cvmx_interrupt_gmxx_enable(int interface);
void __cvmx_interrupt_pcsx_intx_en_reg_enable(int index, int block);
void __cvmx_interrupt_pcsxx_int_en_reg_enable(int index);

int __cvmx_helper_xaui_enumerate(int interface)
{
	union cvmx_gmxx_hg2_control gmx_hg2_control;

	
	gmx_hg2_control.u64 = cvmx_read_csr(CVMX_GMXX_HG2_CONTROL(interface));
	if (gmx_hg2_control.s.hg2tx_en)
		return 16;
	else
		return 1;
}

int __cvmx_helper_xaui_probe(int interface)
{
	int i;
	union cvmx_gmxx_inf_mode mode;

	mode.u64 = cvmx_read_csr(CVMX_GMXX_INF_MODE(interface));
	mode.s.en = 1;
	cvmx_write_csr(CVMX_GMXX_INF_MODE(interface), mode.u64);

	__cvmx_helper_setup_gmx(interface, 1);

	for (i = 0; i < 16; i++) {
		union cvmx_pko_mem_port_ptrs pko_mem_port_ptrs;
		pko_mem_port_ptrs.u64 = 0;
		pko_mem_port_ptrs.s.static_p = 0;
		pko_mem_port_ptrs.s.qos_mask = 0xff;
		
		pko_mem_port_ptrs.s.eid = interface * 4;
		pko_mem_port_ptrs.s.pid = interface * 16 + i;
		cvmx_write_csr(CVMX_PKO_MEM_PORT_PTRS, pko_mem_port_ptrs.u64);
	}
	return __cvmx_helper_xaui_enumerate(interface);
}

int __cvmx_helper_xaui_enable(int interface)
{
	union cvmx_gmxx_prtx_cfg gmx_cfg;
	union cvmx_pcsxx_control1_reg xauiCtl;
	union cvmx_pcsxx_misc_ctl_reg xauiMiscCtl;
	union cvmx_gmxx_tx_xaui_ctl gmxXauiTxCtl;
	union cvmx_gmxx_rxx_int_en gmx_rx_int_en;
	union cvmx_gmxx_tx_int_en gmx_tx_int_en;
	union cvmx_pcsxx_int_en_reg pcsx_int_en_reg;

	

	
	xauiMiscCtl.u64 = cvmx_read_csr(CVMX_PCSXX_MISC_CTL_REG(interface));
	xauiMiscCtl.s.gmxeno = 1;
	cvmx_write_csr(CVMX_PCSXX_MISC_CTL_REG(interface), xauiMiscCtl.u64);

	
	gmx_rx_int_en.u64 = cvmx_read_csr(CVMX_GMXX_RXX_INT_EN(0, interface));
	cvmx_write_csr(CVMX_GMXX_RXX_INT_EN(0, interface), 0x0);
	gmx_tx_int_en.u64 = cvmx_read_csr(CVMX_GMXX_TX_INT_EN(interface));
	cvmx_write_csr(CVMX_GMXX_TX_INT_EN(interface), 0x0);
	pcsx_int_en_reg.u64 = cvmx_read_csr(CVMX_PCSXX_INT_EN_REG(interface));
	cvmx_write_csr(CVMX_PCSXX_INT_EN_REG(interface), 0x0);

	
	
	
	gmxXauiTxCtl.u64 = cvmx_read_csr(CVMX_GMXX_TX_XAUI_CTL(interface));
	
	gmxXauiTxCtl.s.dic_en = 1;
	gmxXauiTxCtl.s.uni_en = 0;
	cvmx_write_csr(CVMX_GMXX_TX_XAUI_CTL(interface), gmxXauiTxCtl.u64);

	
	xauiCtl.u64 = cvmx_read_csr(CVMX_PCSXX_CONTROL1_REG(interface));
	xauiCtl.s.lo_pwr = 0;
	xauiCtl.s.reset = 1;
	cvmx_write_csr(CVMX_PCSXX_CONTROL1_REG(interface), xauiCtl.u64);

	
	if (CVMX_WAIT_FOR_FIELD64
	    (CVMX_PCSXX_CONTROL1_REG(interface), union cvmx_pcsxx_control1_reg,
	     reset, ==, 0, 10000))
		return -1;
	
	if (CVMX_WAIT_FOR_FIELD64
	    (CVMX_PCSXX_10GBX_STATUS_REG(interface),
	     union cvmx_pcsxx_10gbx_status_reg, alignd, ==, 1, 10000))
		return -1;
	
	if (CVMX_WAIT_FOR_FIELD64
	    (CVMX_GMXX_RX_XAUI_CTL(interface), union cvmx_gmxx_rx_xaui_ctl,
		    status, ==, 0, 10000))
		return -1;

	
	gmx_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(0, interface));
	gmx_cfg.s.en = 0;
	cvmx_write_csr(CVMX_GMXX_PRTX_CFG(0, interface), gmx_cfg.u64);

	
	if (CVMX_WAIT_FOR_FIELD64
	    (CVMX_GMXX_PRTX_CFG(0, interface), union cvmx_gmxx_prtx_cfg,
		    rx_idle, ==, 1, 10000))
		return -1;
	
	if (CVMX_WAIT_FOR_FIELD64
	    (CVMX_GMXX_PRTX_CFG(0, interface), union cvmx_gmxx_prtx_cfg,
		    tx_idle, ==, 1, 10000))
		return -1;

	
	gmx_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(0, interface));
	gmx_cfg.s.speed = 1;
	gmx_cfg.s.speed_msb = 0;
	gmx_cfg.s.slottime = 1;
	cvmx_write_csr(CVMX_GMXX_TX_PRTS(interface), 1);
	cvmx_write_csr(CVMX_GMXX_TXX_SLOT(0, interface), 512);
	cvmx_write_csr(CVMX_GMXX_TXX_BURST(0, interface), 8192);
	cvmx_write_csr(CVMX_GMXX_PRTX_CFG(0, interface), gmx_cfg.u64);

	
	cvmx_write_csr(CVMX_GMXX_RXX_INT_REG(0, interface),
		       cvmx_read_csr(CVMX_GMXX_RXX_INT_REG(0, interface)));
	cvmx_write_csr(CVMX_GMXX_TX_INT_REG(interface),
		       cvmx_read_csr(CVMX_GMXX_TX_INT_REG(interface)));
	cvmx_write_csr(CVMX_PCSXX_INT_REG(interface),
		       cvmx_read_csr(CVMX_PCSXX_INT_REG(interface)));

	
	if (CVMX_WAIT_FOR_FIELD64
	    (CVMX_PCSXX_STATUS1_REG(interface), union cvmx_pcsxx_status1_reg,
	     rcv_lnk, ==, 1, 10000))
		return -1;
	if (CVMX_WAIT_FOR_FIELD64
	    (CVMX_PCSXX_STATUS2_REG(interface), union cvmx_pcsxx_status2_reg,
	     xmtflt, ==, 0, 10000))
		return -1;
	if (CVMX_WAIT_FOR_FIELD64
	    (CVMX_PCSXX_STATUS2_REG(interface), union cvmx_pcsxx_status2_reg,
	     rcvflt, ==, 0, 10000))
		return -1;

	cvmx_write_csr(CVMX_GMXX_RXX_INT_EN(0, interface), gmx_rx_int_en.u64);
	cvmx_write_csr(CVMX_GMXX_TX_INT_EN(interface), gmx_tx_int_en.u64);
	cvmx_write_csr(CVMX_PCSXX_INT_EN_REG(interface), pcsx_int_en_reg.u64);

	cvmx_helper_link_autoconf(cvmx_helper_get_ipd_port(interface, 0));

	
	xauiMiscCtl.s.gmxeno = 0;
	cvmx_write_csr(CVMX_PCSXX_MISC_CTL_REG(interface), xauiMiscCtl.u64);

	gmx_cfg.u64 = cvmx_read_csr(CVMX_GMXX_PRTX_CFG(0, interface));
	gmx_cfg.s.en = 1;
	cvmx_write_csr(CVMX_GMXX_PRTX_CFG(0, interface), gmx_cfg.u64);

	__cvmx_interrupt_pcsx_intx_en_reg_enable(0, interface);
	__cvmx_interrupt_pcsx_intx_en_reg_enable(1, interface);
	__cvmx_interrupt_pcsx_intx_en_reg_enable(2, interface);
	__cvmx_interrupt_pcsx_intx_en_reg_enable(3, interface);
	__cvmx_interrupt_pcsxx_int_en_reg_enable(interface);
	__cvmx_interrupt_gmxx_enable(interface);

	return 0;
}

cvmx_helper_link_info_t __cvmx_helper_xaui_link_get(int ipd_port)
{
	int interface = cvmx_helper_get_interface_num(ipd_port);
	union cvmx_gmxx_tx_xaui_ctl gmxx_tx_xaui_ctl;
	union cvmx_gmxx_rx_xaui_ctl gmxx_rx_xaui_ctl;
	union cvmx_pcsxx_status1_reg pcsxx_status1_reg;
	cvmx_helper_link_info_t result;

	gmxx_tx_xaui_ctl.u64 = cvmx_read_csr(CVMX_GMXX_TX_XAUI_CTL(interface));
	gmxx_rx_xaui_ctl.u64 = cvmx_read_csr(CVMX_GMXX_RX_XAUI_CTL(interface));
	pcsxx_status1_reg.u64 =
	    cvmx_read_csr(CVMX_PCSXX_STATUS1_REG(interface));
	result.u64 = 0;

	
	if ((gmxx_tx_xaui_ctl.s.ls == 0) && (gmxx_rx_xaui_ctl.s.status == 0) &&
	    (pcsxx_status1_reg.s.rcv_lnk == 1)) {
		result.s.link_up = 1;
		result.s.full_duplex = 1;
		result.s.speed = 10000;
	} else {
		
		cvmx_write_csr(CVMX_GMXX_RXX_INT_EN(0, interface), 0x0);
		cvmx_write_csr(CVMX_GMXX_TX_INT_EN(interface), 0x0);
		cvmx_write_csr(CVMX_PCSXX_INT_EN_REG(interface), 0x0);
	}
	return result;
}

int __cvmx_helper_xaui_link_set(int ipd_port, cvmx_helper_link_info_t link_info)
{
	int interface = cvmx_helper_get_interface_num(ipd_port);
	union cvmx_gmxx_tx_xaui_ctl gmxx_tx_xaui_ctl;
	union cvmx_gmxx_rx_xaui_ctl gmxx_rx_xaui_ctl;

	gmxx_tx_xaui_ctl.u64 = cvmx_read_csr(CVMX_GMXX_TX_XAUI_CTL(interface));
	gmxx_rx_xaui_ctl.u64 = cvmx_read_csr(CVMX_GMXX_RX_XAUI_CTL(interface));

	
	if (!link_info.s.link_up)
		return 0;

	
	if ((gmxx_tx_xaui_ctl.s.ls == 0) && (gmxx_rx_xaui_ctl.s.status == 0))
		return 0;

	
	return __cvmx_helper_xaui_enable(interface);
}

extern int __cvmx_helper_xaui_configure_loopback(int ipd_port,
						 int enable_internal,
						 int enable_external)
{
	int interface = cvmx_helper_get_interface_num(ipd_port);
	union cvmx_pcsxx_control1_reg pcsxx_control1_reg;
	union cvmx_gmxx_xaui_ext_loopback gmxx_xaui_ext_loopback;

	
	pcsxx_control1_reg.u64 =
	    cvmx_read_csr(CVMX_PCSXX_CONTROL1_REG(interface));
	pcsxx_control1_reg.s.loopbck1 = enable_internal;
	cvmx_write_csr(CVMX_PCSXX_CONTROL1_REG(interface),
		       pcsxx_control1_reg.u64);

	
	gmxx_xaui_ext_loopback.u64 =
	    cvmx_read_csr(CVMX_GMXX_XAUI_EXT_LOOPBACK(interface));
	gmxx_xaui_ext_loopback.s.en = enable_external;
	cvmx_write_csr(CVMX_GMXX_XAUI_EXT_LOOPBACK(interface),
		       gmxx_xaui_ext_loopback.u64);

	
	return __cvmx_helper_xaui_enable(interface);
}
