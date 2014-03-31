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
#include <asm/octeon/cvmx-bootinfo.h>

#include <asm/octeon/cvmx-config.h>

#include <asm/octeon/cvmx-mdio.h>

#include <asm/octeon/cvmx-helper.h>
#include <asm/octeon/cvmx-helper-util.h>
#include <asm/octeon/cvmx-helper-board.h>

#include <asm/octeon/cvmx-gmxx-defs.h>
#include <asm/octeon/cvmx-asxx-defs.h>

cvmx_helper_link_info_t(*cvmx_override_board_link_get) (int ipd_port) =
    NULL;

int cvmx_helper_board_get_mii_address(int ipd_port)
{
	switch (cvmx_sysinfo_get()->board_type) {
	case CVMX_BOARD_TYPE_SIM:
		
		return -1;
	case CVMX_BOARD_TYPE_EBT3000:
	case CVMX_BOARD_TYPE_EBT5800:
	case CVMX_BOARD_TYPE_THUNDER:
	case CVMX_BOARD_TYPE_NICPRO2:
		
		if ((ipd_port >= 16) && (ipd_port < 20))
			return ipd_port - 16;
		else
			return -1;
	case CVMX_BOARD_TYPE_KODAMA:
	case CVMX_BOARD_TYPE_EBH3100:
	case CVMX_BOARD_TYPE_HIKARI:
	case CVMX_BOARD_TYPE_CN3010_EVB_HS5:
	case CVMX_BOARD_TYPE_CN3005_EVB_HS5:
	case CVMX_BOARD_TYPE_CN3020_EVB_HS5:
		if (ipd_port == 0)
			return 4;
		else if (ipd_port == 1)
			return 9;
		else
			return -1;
	case CVMX_BOARD_TYPE_NAC38:
		
		if ((ipd_port >= 0) && (ipd_port < 4))
			return ipd_port;
		else if ((ipd_port >= 16) && (ipd_port < 20))
			return ipd_port - 16 + 4;
		else
			return -1;
	case CVMX_BOARD_TYPE_EBH3000:
		
		return -1;
	case CVMX_BOARD_TYPE_EBH5200:
	case CVMX_BOARD_TYPE_EBH5201:
	case CVMX_BOARD_TYPE_EBT5200:
		
		if ((ipd_port >= CVMX_HELPER_BOARD_MGMT_IPD_PORT) &&
		    (ipd_port < (CVMX_HELPER_BOARD_MGMT_IPD_PORT + 2)))
			return ipd_port - CVMX_HELPER_BOARD_MGMT_IPD_PORT;
		if ((ipd_port >= 0) && (ipd_port < 4))
			return ipd_port + 2;
		else
			return -1;
	case CVMX_BOARD_TYPE_EBH5600:
	case CVMX_BOARD_TYPE_EBH5601:
	case CVMX_BOARD_TYPE_EBH5610:
		
		if (ipd_port == CVMX_HELPER_BOARD_MGMT_IPD_PORT)
			return 0;
		if ((ipd_port >= 0) && (ipd_port < 4))
			return ipd_port + 1;
		else
			return -1;
	case CVMX_BOARD_TYPE_CUST_NB5:
		if (ipd_port == 2)
			return 4;
		else
			return -1;
	case CVMX_BOARD_TYPE_NIC_XLE_4G:
		
		if ((ipd_port >= 16) && (ipd_port < 20))
			return ipd_port - 16 + 1;
		else
			return -1;
	case CVMX_BOARD_TYPE_NIC_XLE_10G:
	case CVMX_BOARD_TYPE_NIC10E:
		return -1;
	case CVMX_BOARD_TYPE_NIC4E:
		if (ipd_port >= 0 && ipd_port <= 3)
			return (ipd_port + 0x1f) & 0x1f;
		else
			return -1;
	case CVMX_BOARD_TYPE_NIC2E:
		if (ipd_port >= 0 && ipd_port <= 1)
			return ipd_port + 1;
		else
			return -1;
	case CVMX_BOARD_TYPE_BBGW_REF:
		return -1;

	case CVMX_BOARD_TYPE_CUST_WSX16:
		if (ipd_port >= 0 && ipd_port <= 3)
			return ipd_port;
		else if (ipd_port >= 16 && ipd_port <= 19)
			return ipd_port - 16 + 4;
		else
			return -1;
	}

	
	cvmx_dprintf
	    ("cvmx_helper_board_get_mii_address: Unknown board type %d\n",
	     cvmx_sysinfo_get()->board_type);
	return -1;
}

cvmx_helper_link_info_t __cvmx_helper_board_link_get(int ipd_port)
{
	cvmx_helper_link_info_t result;
	int phy_addr;
	int is_broadcom_phy = 0;

	
	if (cvmx_override_board_link_get)
		return cvmx_override_board_link_get(ipd_port);

	
	result.u64 = 0;

	switch (cvmx_sysinfo_get()->board_type) {
	case CVMX_BOARD_TYPE_SIM:
		
		result.s.link_up = 1;
		result.s.full_duplex = 1;
		result.s.speed = 1000;
		return result;
	case CVMX_BOARD_TYPE_EBH3100:
	case CVMX_BOARD_TYPE_CN3010_EVB_HS5:
	case CVMX_BOARD_TYPE_CN3005_EVB_HS5:
	case CVMX_BOARD_TYPE_CN3020_EVB_HS5:
		
		if (ipd_port == 1) {
			result.s.link_up = 1;
			result.s.full_duplex = 1;
			result.s.speed = 1000;
			return result;
		}
		
		break;
	case CVMX_BOARD_TYPE_CUST_NB5:
		
		if (ipd_port == 1) {
			result.s.link_up = 1;
			result.s.full_duplex = 1;
			result.s.speed = 1000;
			return result;
		} else		
			is_broadcom_phy = 1;
		break;
	case CVMX_BOARD_TYPE_BBGW_REF:
		
		if (ipd_port == 2) {
			
			result.u64 = 0;
			return result;
		} else {
			
			result.s.link_up = 1;
			result.s.full_duplex = 1;
			result.s.speed = 1000;
			return result;
		}
		break;
	}

	phy_addr = cvmx_helper_board_get_mii_address(ipd_port);
	if (phy_addr != -1) {
		if (is_broadcom_phy) {
			int phy_status =
			    cvmx_mdio_read(phy_addr >> 8, phy_addr & 0xff,
					   0x19);
			switch ((phy_status >> 8) & 0x7) {
			case 0:
				result.u64 = 0;
				break;
			case 1:
				result.s.link_up = 1;
				result.s.full_duplex = 0;
				result.s.speed = 10;
				break;
			case 2:
				result.s.link_up = 1;
				result.s.full_duplex = 1;
				result.s.speed = 10;
				break;
			case 3:
				result.s.link_up = 1;
				result.s.full_duplex = 0;
				result.s.speed = 100;
				break;
			case 4:
				result.s.link_up = 1;
				result.s.full_duplex = 1;
				result.s.speed = 100;
				break;
			case 5:
				result.s.link_up = 1;
				result.s.full_duplex = 1;
				result.s.speed = 100;
				break;
			case 6:
				result.s.link_up = 1;
				result.s.full_duplex = 0;
				result.s.speed = 1000;
				break;
			case 7:
				result.s.link_up = 1;
				result.s.full_duplex = 1;
				result.s.speed = 1000;
				break;
			}
		} else {
			int phy_status =
			    cvmx_mdio_read(phy_addr >> 8, phy_addr & 0xff, 17);

			if ((phy_status & (1 << 11)) == 0) {
				int auto_status =
				    cvmx_mdio_read(phy_addr >> 8,
						   phy_addr & 0xff, 0);
				if ((auto_status & (1 << 12)) == 0)
					phy_status |= 1 << 11;
			}

			if (phy_status & (1 << 11)) {
				result.s.link_up = 1;
				result.s.full_duplex = ((phy_status >> 13) & 1);
				switch ((phy_status >> 14) & 3) {
				case 0:	
					result.s.speed = 10;
					break;
				case 1:	
					result.s.speed = 100;
					break;
				case 2:	
					result.s.speed = 1000;
					break;
				case 3:	
					result.u64 = 0;
					break;
				}
			}
		}
	} else if (OCTEON_IS_MODEL(OCTEON_CN3XXX)
		   || OCTEON_IS_MODEL(OCTEON_CN58XX)
		   || OCTEON_IS_MODEL(OCTEON_CN50XX)) {
		union cvmx_gmxx_rxx_rx_inbnd inband_status;
		int interface = cvmx_helper_get_interface_num(ipd_port);
		int index = cvmx_helper_get_interface_index_num(ipd_port);
		inband_status.u64 =
		    cvmx_read_csr(CVMX_GMXX_RXX_RX_INBND(index, interface));

		result.s.link_up = inband_status.s.status;
		result.s.full_duplex = inband_status.s.duplex;
		switch (inband_status.s.speed) {
		case 0:	
			result.s.speed = 10;
			break;
		case 1:	
			result.s.speed = 100;
			break;
		case 2:	
			result.s.speed = 1000;
			break;
		case 3:	
			result.u64 = 0;
			break;
		}
	} else {
		result.u64 = 0;
	}

	
	if (!result.s.link_up)
		result.u64 = 0;

	return result;
}

int cvmx_helper_board_link_set_phy(int phy_addr,
				   cvmx_helper_board_set_phy_link_flags_types_t
				   link_flags,
				   cvmx_helper_link_info_t link_info)
{

	
	if ((link_flags & set_phy_link_flags_flow_control_mask) !=
	    set_phy_link_flags_flow_control_dont_touch) {
		cvmx_mdio_phy_reg_autoneg_adver_t reg_autoneg_adver;
		reg_autoneg_adver.u16 =
		    cvmx_mdio_read(phy_addr >> 8, phy_addr & 0xff,
				   CVMX_MDIO_PHY_REG_AUTONEG_ADVER);
		reg_autoneg_adver.s.asymmetric_pause =
		    (link_flags & set_phy_link_flags_flow_control_mask) ==
		    set_phy_link_flags_flow_control_enable;
		reg_autoneg_adver.s.pause =
		    (link_flags & set_phy_link_flags_flow_control_mask) ==
		    set_phy_link_flags_flow_control_enable;
		cvmx_mdio_write(phy_addr >> 8, phy_addr & 0xff,
				CVMX_MDIO_PHY_REG_AUTONEG_ADVER,
				reg_autoneg_adver.u16);
	}

	
	if ((link_flags & set_phy_link_flags_autoneg)
	    && (link_info.s.speed == 0)) {
		cvmx_mdio_phy_reg_control_t reg_control;
		cvmx_mdio_phy_reg_status_t reg_status;
		cvmx_mdio_phy_reg_autoneg_adver_t reg_autoneg_adver;
		cvmx_mdio_phy_reg_extended_status_t reg_extended_status;
		cvmx_mdio_phy_reg_control_1000_t reg_control_1000;

		reg_status.u16 =
		    cvmx_mdio_read(phy_addr >> 8, phy_addr & 0xff,
				   CVMX_MDIO_PHY_REG_STATUS);
		reg_autoneg_adver.u16 =
		    cvmx_mdio_read(phy_addr >> 8, phy_addr & 0xff,
				   CVMX_MDIO_PHY_REG_AUTONEG_ADVER);
		reg_autoneg_adver.s.advert_100base_t4 =
		    reg_status.s.capable_100base_t4;
		reg_autoneg_adver.s.advert_10base_tx_full =
		    reg_status.s.capable_10_full;
		reg_autoneg_adver.s.advert_10base_tx_half =
		    reg_status.s.capable_10_half;
		reg_autoneg_adver.s.advert_100base_tx_full =
		    reg_status.s.capable_100base_x_full;
		reg_autoneg_adver.s.advert_100base_tx_half =
		    reg_status.s.capable_100base_x_half;
		cvmx_mdio_write(phy_addr >> 8, phy_addr & 0xff,
				CVMX_MDIO_PHY_REG_AUTONEG_ADVER,
				reg_autoneg_adver.u16);
		if (reg_status.s.capable_extended_status) {
			reg_extended_status.u16 =
			    cvmx_mdio_read(phy_addr >> 8, phy_addr & 0xff,
					   CVMX_MDIO_PHY_REG_EXTENDED_STATUS);
			reg_control_1000.u16 =
			    cvmx_mdio_read(phy_addr >> 8, phy_addr & 0xff,
					   CVMX_MDIO_PHY_REG_CONTROL_1000);
			reg_control_1000.s.advert_1000base_t_full =
			    reg_extended_status.s.capable_1000base_t_full;
			reg_control_1000.s.advert_1000base_t_half =
			    reg_extended_status.s.capable_1000base_t_half;
			cvmx_mdio_write(phy_addr >> 8, phy_addr & 0xff,
					CVMX_MDIO_PHY_REG_CONTROL_1000,
					reg_control_1000.u16);
		}
		reg_control.u16 =
		    cvmx_mdio_read(phy_addr >> 8, phy_addr & 0xff,
				   CVMX_MDIO_PHY_REG_CONTROL);
		reg_control.s.autoneg_enable = 1;
		reg_control.s.restart_autoneg = 1;
		cvmx_mdio_write(phy_addr >> 8, phy_addr & 0xff,
				CVMX_MDIO_PHY_REG_CONTROL, reg_control.u16);
	} else if ((link_flags & set_phy_link_flags_autoneg)) {
		cvmx_mdio_phy_reg_control_t reg_control;
		cvmx_mdio_phy_reg_status_t reg_status;
		cvmx_mdio_phy_reg_autoneg_adver_t reg_autoneg_adver;
		cvmx_mdio_phy_reg_control_1000_t reg_control_1000;

		reg_status.u16 =
		    cvmx_mdio_read(phy_addr >> 8, phy_addr & 0xff,
				   CVMX_MDIO_PHY_REG_STATUS);
		reg_autoneg_adver.u16 =
		    cvmx_mdio_read(phy_addr >> 8, phy_addr & 0xff,
				   CVMX_MDIO_PHY_REG_AUTONEG_ADVER);
		reg_autoneg_adver.s.advert_100base_t4 = 0;
		reg_autoneg_adver.s.advert_10base_tx_full = 0;
		reg_autoneg_adver.s.advert_10base_tx_half = 0;
		reg_autoneg_adver.s.advert_100base_tx_full = 0;
		reg_autoneg_adver.s.advert_100base_tx_half = 0;
		if (reg_status.s.capable_extended_status) {
			reg_control_1000.u16 =
			    cvmx_mdio_read(phy_addr >> 8, phy_addr & 0xff,
					   CVMX_MDIO_PHY_REG_CONTROL_1000);
			reg_control_1000.s.advert_1000base_t_full = 0;
			reg_control_1000.s.advert_1000base_t_half = 0;
		}
		switch (link_info.s.speed) {
		case 10:
			reg_autoneg_adver.s.advert_10base_tx_full =
			    link_info.s.full_duplex;
			reg_autoneg_adver.s.advert_10base_tx_half =
			    !link_info.s.full_duplex;
			break;
		case 100:
			reg_autoneg_adver.s.advert_100base_tx_full =
			    link_info.s.full_duplex;
			reg_autoneg_adver.s.advert_100base_tx_half =
			    !link_info.s.full_duplex;
			break;
		case 1000:
			reg_control_1000.s.advert_1000base_t_full =
			    link_info.s.full_duplex;
			reg_control_1000.s.advert_1000base_t_half =
			    !link_info.s.full_duplex;
			break;
		}
		cvmx_mdio_write(phy_addr >> 8, phy_addr & 0xff,
				CVMX_MDIO_PHY_REG_AUTONEG_ADVER,
				reg_autoneg_adver.u16);
		if (reg_status.s.capable_extended_status)
			cvmx_mdio_write(phy_addr >> 8, phy_addr & 0xff,
					CVMX_MDIO_PHY_REG_CONTROL_1000,
					reg_control_1000.u16);
		reg_control.u16 =
		    cvmx_mdio_read(phy_addr >> 8, phy_addr & 0xff,
				   CVMX_MDIO_PHY_REG_CONTROL);
		reg_control.s.autoneg_enable = 1;
		reg_control.s.restart_autoneg = 1;
		cvmx_mdio_write(phy_addr >> 8, phy_addr & 0xff,
				CVMX_MDIO_PHY_REG_CONTROL, reg_control.u16);
	} else {
		cvmx_mdio_phy_reg_control_t reg_control;
		reg_control.u16 =
		    cvmx_mdio_read(phy_addr >> 8, phy_addr & 0xff,
				   CVMX_MDIO_PHY_REG_CONTROL);
		reg_control.s.autoneg_enable = 0;
		reg_control.s.restart_autoneg = 1;
		reg_control.s.duplex = link_info.s.full_duplex;
		if (link_info.s.speed == 1000) {
			reg_control.s.speed_msb = 1;
			reg_control.s.speed_lsb = 0;
		} else if (link_info.s.speed == 100) {
			reg_control.s.speed_msb = 0;
			reg_control.s.speed_lsb = 1;
		} else if (link_info.s.speed == 10) {
			reg_control.s.speed_msb = 0;
			reg_control.s.speed_lsb = 0;
		}
		cvmx_mdio_write(phy_addr >> 8, phy_addr & 0xff,
				CVMX_MDIO_PHY_REG_CONTROL, reg_control.u16);
	}
	return 0;
}

int __cvmx_helper_board_interface_probe(int interface, int supported_ports)
{
	switch (cvmx_sysinfo_get()->board_type) {
	case CVMX_BOARD_TYPE_CN3005_EVB_HS5:
		if (interface == 0)
			return 2;
		break;
	case CVMX_BOARD_TYPE_BBGW_REF:
		if (interface == 0)
			return 2;
		break;
	case CVMX_BOARD_TYPE_NIC_XLE_4G:
		if (interface == 0)
			return 0;
		break;
	case CVMX_BOARD_TYPE_EBH5600:
		if (interface == 1)
			return 0;
		break;
	}
	return supported_ports;
}

int __cvmx_helper_board_hardware_enable(int interface)
{
	if (cvmx_sysinfo_get()->board_type == CVMX_BOARD_TYPE_CN3005_EVB_HS5) {
		if (interface == 0) {
			
			cvmx_write_csr(CVMX_ASXX_TX_CLK_SETX(1, interface), 0);
			cvmx_write_csr(CVMX_ASXX_RX_CLK_SETX(1, interface), 0);
			cvmx_write_csr(CVMX_ASXX_TX_CLK_SETX(0, interface),
				       0xc);
			cvmx_write_csr(CVMX_ASXX_RX_CLK_SETX(0, interface),
				       0xc);
		}
	} else if (cvmx_sysinfo_get()->board_type ==
		   CVMX_BOARD_TYPE_CN3010_EVB_HS5) {
		if (interface == 0) {
			int phy_addr = cvmx_helper_board_get_mii_address(0);
			if (phy_addr != -1) {
				int phy_identifier =
				    cvmx_mdio_read(phy_addr >> 8,
						   phy_addr & 0xff, 0x2);
				
				if (phy_identifier == 0x0143) {
					cvmx_dprintf("\n");
					cvmx_dprintf("ERROR:\n");
					cvmx_dprintf
					    ("ERROR: Board type is CVMX_BOARD_TYPE_CN3010_EVB_HS5, but Broadcom PHY found.\n");
					cvmx_dprintf
					    ("ERROR: The board type is mis-configured, and software malfunctions are likely.\n");
					cvmx_dprintf
					    ("ERROR: All boards require a unique board type to identify them.\n");
					cvmx_dprintf("ERROR:\n");
					cvmx_dprintf("\n");
					cvmx_wait(1000000000);
					cvmx_write_csr(CVMX_ASXX_RX_CLK_SETX
						       (0, interface), 5);
					cvmx_write_csr(CVMX_ASXX_TX_CLK_SETX
						       (0, interface), 5);
				}
			}
		}
	}
	return 0;
}
