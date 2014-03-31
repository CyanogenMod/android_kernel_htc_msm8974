/*******************************************************************************

  Intel PRO/1000 Linux driver
  Copyright(c) 1999 - 2012 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  Linux NICS <linux.nics@intel.com>
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#include "e1000.h"

s32 e1000e_get_bus_info_pcie(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	struct e1000_bus_info *bus = &hw->bus;
	struct e1000_adapter *adapter = hw->adapter;
	u16 pcie_link_status, cap_offset;

	cap_offset = adapter->pdev->pcie_cap;
	if (!cap_offset) {
		bus->width = e1000_bus_width_unknown;
	} else {
		pci_read_config_word(adapter->pdev,
				     cap_offset + PCIE_LINK_STATUS,
				     &pcie_link_status);
		bus->width = (enum e1000_bus_width)((pcie_link_status &
						     PCIE_LINK_WIDTH_MASK) >>
						    PCIE_LINK_WIDTH_SHIFT);
	}

	mac->ops.set_lan_id(hw);

	return 0;
}

void e1000_set_lan_id_multi_port_pcie(struct e1000_hw *hw)
{
	struct e1000_bus_info *bus = &hw->bus;
	u32 reg;

	reg = er32(STATUS);
	bus->func = (reg & E1000_STATUS_FUNC_MASK) >> E1000_STATUS_FUNC_SHIFT;
}

void e1000_set_lan_id_single_port(struct e1000_hw *hw)
{
	struct e1000_bus_info *bus = &hw->bus;

	bus->func = 0;
}

void e1000_clear_vfta_generic(struct e1000_hw *hw)
{
	u32 offset;

	for (offset = 0; offset < E1000_VLAN_FILTER_TBL_SIZE; offset++) {
		E1000_WRITE_REG_ARRAY(hw, E1000_VFTA, offset, 0);
		e1e_flush();
	}
}

/**
 *  e1000_write_vfta_generic - Write value to VLAN filter table
 *  @hw: pointer to the HW structure
 *  @offset: register offset in VLAN filter table
 *  @value: register value written to VLAN filter table
 *
 *  Writes value at the given offset in the register array which stores
 *  the VLAN filter table.
 **/
void e1000_write_vfta_generic(struct e1000_hw *hw, u32 offset, u32 value)
{
	E1000_WRITE_REG_ARRAY(hw, E1000_VFTA, offset, value);
	e1e_flush();
}

void e1000e_init_rx_addrs(struct e1000_hw *hw, u16 rar_count)
{
	u32 i;
	u8 mac_addr[ETH_ALEN] = { 0 };

	
	e_dbg("Programming MAC Address into RAR[0]\n");

	e1000e_rar_set(hw, hw->mac.addr, 0);

	
	e_dbg("Clearing RAR[1-%u]\n", rar_count - 1);
	for (i = 1; i < rar_count; i++)
		e1000e_rar_set(hw, mac_addr, i);
}

s32 e1000_check_alt_mac_addr_generic(struct e1000_hw *hw)
{
	u32 i;
	s32 ret_val = 0;
	u16 offset, nvm_alt_mac_addr_offset, nvm_data;
	u8 alt_mac_addr[ETH_ALEN];

	ret_val = e1000_read_nvm(hw, NVM_COMPAT, 1, &nvm_data);
	if (ret_val)
		return ret_val;

	
	if (hw->mac.type == e1000_82573)
		return 0;

	ret_val = e1000_read_nvm(hw, NVM_ALT_MAC_ADDR_PTR, 1,
				 &nvm_alt_mac_addr_offset);
	if (ret_val) {
		e_dbg("NVM Read Error\n");
		return ret_val;
	}

	if ((nvm_alt_mac_addr_offset == 0xFFFF) ||
	    (nvm_alt_mac_addr_offset == 0x0000))
		
		return 0;

	if (hw->bus.func == E1000_FUNC_1)
		nvm_alt_mac_addr_offset += E1000_ALT_MAC_ADDRESS_OFFSET_LAN1;
	for (i = 0; i < ETH_ALEN; i += 2) {
		offset = nvm_alt_mac_addr_offset + (i >> 1);
		ret_val = e1000_read_nvm(hw, offset, 1, &nvm_data);
		if (ret_val) {
			e_dbg("NVM Read Error\n");
			return ret_val;
		}

		alt_mac_addr[i] = (u8)(nvm_data & 0xFF);
		alt_mac_addr[i + 1] = (u8)(nvm_data >> 8);
	}

	
	if (is_multicast_ether_addr(alt_mac_addr)) {
		e_dbg("Ignoring Alternate Mac Address with MC bit set\n");
		return 0;
	}

	e1000e_rar_set(hw, alt_mac_addr, 0);

	return 0;
}

void e1000e_rar_set(struct e1000_hw *hw, u8 *addr, u32 index)
{
	u32 rar_low, rar_high;

	rar_low = ((u32)addr[0] | ((u32)addr[1] << 8) |
		   ((u32)addr[2] << 16) | ((u32)addr[3] << 24));

	rar_high = ((u32)addr[4] | ((u32)addr[5] << 8));

	
	if (rar_low || rar_high)
		rar_high |= E1000_RAH_AV;

	ew32(RAL(index), rar_low);
	e1e_flush();
	ew32(RAH(index), rar_high);
	e1e_flush();
}

static u32 e1000_hash_mc_addr(struct e1000_hw *hw, u8 *mc_addr)
{
	u32 hash_value, hash_mask;
	u8 bit_shift = 0;

	
	hash_mask = (hw->mac.mta_reg_count * 32) - 1;

	while (hash_mask >> bit_shift != 0xFF)
		bit_shift++;

	switch (hw->mac.mc_filter_type) {
	default:
	case 0:
		break;
	case 1:
		bit_shift += 1;
		break;
	case 2:
		bit_shift += 2;
		break;
	case 3:
		bit_shift += 4;
		break;
	}

	hash_value = hash_mask & (((mc_addr[4] >> (8 - bit_shift)) |
				   (((u16)mc_addr[5]) << bit_shift)));

	return hash_value;
}

void e1000e_update_mc_addr_list_generic(struct e1000_hw *hw,
					u8 *mc_addr_list, u32 mc_addr_count)
{
	u32 hash_value, hash_bit, hash_reg;
	int i;

	
	memset(&hw->mac.mta_shadow, 0, sizeof(hw->mac.mta_shadow));

	
	for (i = 0; (u32)i < mc_addr_count; i++) {
		hash_value = e1000_hash_mc_addr(hw, mc_addr_list);

		hash_reg = (hash_value >> 5) & (hw->mac.mta_reg_count - 1);
		hash_bit = hash_value & 0x1F;

		hw->mac.mta_shadow[hash_reg] |= (1 << hash_bit);
		mc_addr_list += (ETH_ALEN);
	}

	
	for (i = hw->mac.mta_reg_count - 1; i >= 0; i--)
		E1000_WRITE_REG_ARRAY(hw, E1000_MTA, i, hw->mac.mta_shadow[i]);
	e1e_flush();
}

void e1000e_clear_hw_cntrs_base(struct e1000_hw *hw)
{
	er32(CRCERRS);
	er32(SYMERRS);
	er32(MPC);
	er32(SCC);
	er32(ECOL);
	er32(MCC);
	er32(LATECOL);
	er32(COLC);
	er32(DC);
	er32(SEC);
	er32(RLEC);
	er32(XONRXC);
	er32(XONTXC);
	er32(XOFFRXC);
	er32(XOFFTXC);
	er32(FCRUC);
	er32(GPRC);
	er32(BPRC);
	er32(MPRC);
	er32(GPTC);
	er32(GORCL);
	er32(GORCH);
	er32(GOTCL);
	er32(GOTCH);
	er32(RNBC);
	er32(RUC);
	er32(RFC);
	er32(ROC);
	er32(RJC);
	er32(TORL);
	er32(TORH);
	er32(TOTL);
	er32(TOTH);
	er32(TPR);
	er32(TPT);
	er32(MPTC);
	er32(BPTC);
}

s32 e1000e_check_for_copper_link(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	s32 ret_val;
	bool link;

	if (!mac->get_link_status)
		return 0;

	ret_val = e1000e_phy_has_link_generic(hw, 1, 0, &link);
	if (ret_val)
		return ret_val;

	if (!link)
		return 0;	

	mac->get_link_status = false;

	e1000e_check_downshift(hw);

	if (!mac->autoneg)
		return -E1000_ERR_CONFIG;

	mac->ops.config_collision_dist(hw);

	ret_val = e1000e_config_fc_after_link_up(hw);
	if (ret_val)
		e_dbg("Error configuring flow control\n");

	return ret_val;
}

s32 e1000e_check_for_fiber_link(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 rxcw;
	u32 ctrl;
	u32 status;
	s32 ret_val;

	ctrl = er32(CTRL);
	status = er32(STATUS);
	rxcw = er32(RXCW);

	
	if ((ctrl & E1000_CTRL_SWDPIN1) && !(status & E1000_STATUS_LU) &&
	    !(rxcw & E1000_RXCW_C)) {
		if (!mac->autoneg_failed) {
			mac->autoneg_failed = true;
			return 0;
		}
		e_dbg("NOT Rx'ing /C/, disable AutoNeg and force link.\n");

		
		ew32(TXCW, (mac->txcw & ~E1000_TXCW_ANE));

		
		ctrl = er32(CTRL);
		ctrl |= (E1000_CTRL_SLU | E1000_CTRL_FD);
		ew32(CTRL, ctrl);

		
		ret_val = e1000e_config_fc_after_link_up(hw);
		if (ret_val) {
			e_dbg("Error configuring flow control\n");
			return ret_val;
		}
	} else if ((ctrl & E1000_CTRL_SLU) && (rxcw & E1000_RXCW_C)) {
		e_dbg("Rx'ing /C/, enable AutoNeg and stop forcing link.\n");
		ew32(TXCW, mac->txcw);
		ew32(CTRL, (ctrl & ~E1000_CTRL_SLU));

		mac->serdes_has_link = true;
	}

	return 0;
}

s32 e1000e_check_for_serdes_link(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 rxcw;
	u32 ctrl;
	u32 status;
	s32 ret_val;

	ctrl = er32(CTRL);
	status = er32(STATUS);
	rxcw = er32(RXCW);

	
	if (!(status & E1000_STATUS_LU) && !(rxcw & E1000_RXCW_C)) {
		if (!mac->autoneg_failed) {
			mac->autoneg_failed = true;
			return 0;
		}
		e_dbg("NOT Rx'ing /C/, disable AutoNeg and force link.\n");

		
		ew32(TXCW, (mac->txcw & ~E1000_TXCW_ANE));

		
		ctrl = er32(CTRL);
		ctrl |= (E1000_CTRL_SLU | E1000_CTRL_FD);
		ew32(CTRL, ctrl);

		
		ret_val = e1000e_config_fc_after_link_up(hw);
		if (ret_val) {
			e_dbg("Error configuring flow control\n");
			return ret_val;
		}
	} else if ((ctrl & E1000_CTRL_SLU) && (rxcw & E1000_RXCW_C)) {
		e_dbg("Rx'ing /C/, enable AutoNeg and stop forcing link.\n");
		ew32(TXCW, mac->txcw);
		ew32(CTRL, (ctrl & ~E1000_CTRL_SLU));

		mac->serdes_has_link = true;
	} else if (!(E1000_TXCW_ANE & er32(TXCW))) {
		
		udelay(10);
		rxcw = er32(RXCW);
		if (rxcw & E1000_RXCW_SYNCH) {
			if (!(rxcw & E1000_RXCW_IV)) {
				mac->serdes_has_link = true;
				e_dbg("SERDES: Link up - forced.\n");
			}
		} else {
			mac->serdes_has_link = false;
			e_dbg("SERDES: Link down - force failed.\n");
		}
	}

	if (E1000_TXCW_ANE & er32(TXCW)) {
		status = er32(STATUS);
		if (status & E1000_STATUS_LU) {
			
			udelay(10);
			rxcw = er32(RXCW);
			if (rxcw & E1000_RXCW_SYNCH) {
				if (!(rxcw & E1000_RXCW_IV)) {
					mac->serdes_has_link = true;
					e_dbg("SERDES: Link up - autoneg completed successfully.\n");
				} else {
					mac->serdes_has_link = false;
					e_dbg("SERDES: Link down - invalid codewords detected in autoneg.\n");
				}
			} else {
				mac->serdes_has_link = false;
				e_dbg("SERDES: Link down - no sync.\n");
			}
		} else {
			mac->serdes_has_link = false;
			e_dbg("SERDES: Link down - autoneg failed\n");
		}
	}

	return 0;
}

static s32 e1000_set_default_fc_generic(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 nvm_data;

	ret_val = e1000_read_nvm(hw, NVM_INIT_CONTROL2_REG, 1, &nvm_data);

	if (ret_val) {
		e_dbg("NVM Read Error\n");
		return ret_val;
	}

	if ((nvm_data & NVM_WORD0F_PAUSE_MASK) == 0)
		hw->fc.requested_mode = e1000_fc_none;
	else if ((nvm_data & NVM_WORD0F_PAUSE_MASK) == NVM_WORD0F_ASM_DIR)
		hw->fc.requested_mode = e1000_fc_tx_pause;
	else
		hw->fc.requested_mode = e1000_fc_full;

	return 0;
}

s32 e1000e_setup_link_generic(struct e1000_hw *hw)
{
	s32 ret_val;

	if (hw->phy.ops.check_reset_block(hw))
		return 0;

	if (hw->fc.requested_mode == e1000_fc_default) {
		ret_val = e1000_set_default_fc_generic(hw);
		if (ret_val)
			return ret_val;
	}

	hw->fc.current_mode = hw->fc.requested_mode;

	e_dbg("After fix-ups FlowControl is now = %x\n", hw->fc.current_mode);

	
	ret_val = hw->mac.ops.setup_physical_interface(hw);
	if (ret_val)
		return ret_val;

	e_dbg("Initializing the Flow Control address, type and timer regs\n");
	ew32(FCT, FLOW_CONTROL_TYPE);
	ew32(FCAH, FLOW_CONTROL_ADDRESS_HIGH);
	ew32(FCAL, FLOW_CONTROL_ADDRESS_LOW);

	ew32(FCTTV, hw->fc.pause_time);

	return e1000e_set_fc_watermarks(hw);
}

static s32 e1000_commit_fc_settings_generic(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 txcw;

	switch (hw->fc.current_mode) {
	case e1000_fc_none:
		
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD);
		break;
	case e1000_fc_rx_pause:
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_PAUSE_MASK);
		break;
	case e1000_fc_tx_pause:
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_ASM_DIR);
		break;
	case e1000_fc_full:
		txcw = (E1000_TXCW_ANE | E1000_TXCW_FD | E1000_TXCW_PAUSE_MASK);
		break;
	default:
		e_dbg("Flow control param set incorrectly\n");
		return -E1000_ERR_CONFIG;
		break;
	}

	ew32(TXCW, txcw);
	mac->txcw = txcw;

	return 0;
}

static s32 e1000_poll_fiber_serdes_link_generic(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 i, status;
	s32 ret_val;

	for (i = 0; i < FIBER_LINK_UP_LIMIT; i++) {
		usleep_range(10000, 20000);
		status = er32(STATUS);
		if (status & E1000_STATUS_LU)
			break;
	}
	if (i == FIBER_LINK_UP_LIMIT) {
		e_dbg("Never got a valid link from auto-neg!!!\n");
		mac->autoneg_failed = true;
		ret_val = mac->ops.check_for_link(hw);
		if (ret_val) {
			e_dbg("Error while checking for link\n");
			return ret_val;
		}
		mac->autoneg_failed = false;
	} else {
		mac->autoneg_failed = false;
		e_dbg("Valid Link Found\n");
	}

	return 0;
}

s32 e1000e_setup_fiber_serdes_link(struct e1000_hw *hw)
{
	u32 ctrl;
	s32 ret_val;

	ctrl = er32(CTRL);

	
	ctrl &= ~E1000_CTRL_LRST;

	hw->mac.ops.config_collision_dist(hw);

	ret_val = e1000_commit_fc_settings_generic(hw);
	if (ret_val)
		return ret_val;

	e_dbg("Auto-negotiation enabled\n");

	ew32(CTRL, ctrl);
	e1e_flush();
	usleep_range(1000, 2000);

	if (hw->phy.media_type == e1000_media_type_internal_serdes ||
	    (er32(CTRL) & E1000_CTRL_SWDPIN1)) {
		ret_val = e1000_poll_fiber_serdes_link_generic(hw);
	} else {
		e_dbg("No signal detected\n");
	}

	return ret_val;
}

void e1000e_config_collision_dist_generic(struct e1000_hw *hw)
{
	u32 tctl;

	tctl = er32(TCTL);

	tctl &= ~E1000_TCTL_COLD;
	tctl |= E1000_COLLISION_DISTANCE << E1000_COLD_SHIFT;

	ew32(TCTL, tctl);
	e1e_flush();
}

s32 e1000e_set_fc_watermarks(struct e1000_hw *hw)
{
	u32 fcrtl = 0, fcrth = 0;

	if (hw->fc.current_mode & e1000_fc_tx_pause) {
		fcrtl = hw->fc.low_water;
		if (hw->fc.send_xon)
			fcrtl |= E1000_FCRTL_XONE;

		fcrth = hw->fc.high_water;
	}
	ew32(FCRTL, fcrtl);
	ew32(FCRTH, fcrth);

	return 0;
}

s32 e1000e_force_mac_fc(struct e1000_hw *hw)
{
	u32 ctrl;

	ctrl = er32(CTRL);

	e_dbg("hw->fc.current_mode = %u\n", hw->fc.current_mode);

	switch (hw->fc.current_mode) {
	case e1000_fc_none:
		ctrl &= (~(E1000_CTRL_TFCE | E1000_CTRL_RFCE));
		break;
	case e1000_fc_rx_pause:
		ctrl &= (~E1000_CTRL_TFCE);
		ctrl |= E1000_CTRL_RFCE;
		break;
	case e1000_fc_tx_pause:
		ctrl &= (~E1000_CTRL_RFCE);
		ctrl |= E1000_CTRL_TFCE;
		break;
	case e1000_fc_full:
		ctrl |= (E1000_CTRL_TFCE | E1000_CTRL_RFCE);
		break;
	default:
		e_dbg("Flow control param set incorrectly\n");
		return -E1000_ERR_CONFIG;
	}

	ew32(CTRL, ctrl);

	return 0;
}

s32 e1000e_config_fc_after_link_up(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	s32 ret_val = 0;
	u16 mii_status_reg, mii_nway_adv_reg, mii_nway_lp_ability_reg;
	u16 speed, duplex;

	if (mac->autoneg_failed) {
		if (hw->phy.media_type == e1000_media_type_fiber ||
		    hw->phy.media_type == e1000_media_type_internal_serdes)
			ret_val = e1000e_force_mac_fc(hw);
	} else {
		if (hw->phy.media_type == e1000_media_type_copper)
			ret_val = e1000e_force_mac_fc(hw);
	}

	if (ret_val) {
		e_dbg("Error forcing flow control settings\n");
		return ret_val;
	}

	if ((hw->phy.media_type == e1000_media_type_copper) && mac->autoneg) {
		ret_val = e1e_rphy(hw, PHY_STATUS, &mii_status_reg);
		if (ret_val)
			return ret_val;
		ret_val = e1e_rphy(hw, PHY_STATUS, &mii_status_reg);
		if (ret_val)
			return ret_val;

		if (!(mii_status_reg & MII_SR_AUTONEG_COMPLETE)) {
			e_dbg("Copper PHY and Auto Neg has not completed.\n");
			return ret_val;
		}

		ret_val = e1e_rphy(hw, PHY_AUTONEG_ADV, &mii_nway_adv_reg);
		if (ret_val)
			return ret_val;
		ret_val =
		    e1e_rphy(hw, PHY_LP_ABILITY, &mii_nway_lp_ability_reg);
		if (ret_val)
			return ret_val;

		if ((mii_nway_adv_reg & NWAY_AR_PAUSE) &&
		    (mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE)) {
			if (hw->fc.requested_mode == e1000_fc_full) {
				hw->fc.current_mode = e1000_fc_full;
				e_dbg("Flow Control = FULL.\n");
			} else {
				hw->fc.current_mode = e1000_fc_rx_pause;
				e_dbg("Flow Control = Rx PAUSE frames only.\n");
			}
		}
		else if (!(mii_nway_adv_reg & NWAY_AR_PAUSE) &&
			 (mii_nway_adv_reg & NWAY_AR_ASM_DIR) &&
			 (mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE) &&
			 (mii_nway_lp_ability_reg & NWAY_LPAR_ASM_DIR)) {
			hw->fc.current_mode = e1000_fc_tx_pause;
			e_dbg("Flow Control = Tx PAUSE frames only.\n");
		}
		else if ((mii_nway_adv_reg & NWAY_AR_PAUSE) &&
			 (mii_nway_adv_reg & NWAY_AR_ASM_DIR) &&
			 !(mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE) &&
			 (mii_nway_lp_ability_reg & NWAY_LPAR_ASM_DIR)) {
			hw->fc.current_mode = e1000_fc_rx_pause;
			e_dbg("Flow Control = Rx PAUSE frames only.\n");
		} else {
			hw->fc.current_mode = e1000_fc_none;
			e_dbg("Flow Control = NONE.\n");
		}

		ret_val = mac->ops.get_link_up_info(hw, &speed, &duplex);
		if (ret_val) {
			e_dbg("Error getting link speed and duplex\n");
			return ret_val;
		}

		if (duplex == HALF_DUPLEX)
			hw->fc.current_mode = e1000_fc_none;

		ret_val = e1000e_force_mac_fc(hw);
		if (ret_val) {
			e_dbg("Error forcing flow control settings\n");
			return ret_val;
		}
	}

	return 0;
}

s32 e1000e_get_speed_and_duplex_copper(struct e1000_hw *hw, u16 *speed,
				       u16 *duplex)
{
	u32 status;

	status = er32(STATUS);
	if (status & E1000_STATUS_SPEED_1000)
		*speed = SPEED_1000;
	else if (status & E1000_STATUS_SPEED_100)
		*speed = SPEED_100;
	else
		*speed = SPEED_10;

	if (status & E1000_STATUS_FD)
		*duplex = FULL_DUPLEX;
	else
		*duplex = HALF_DUPLEX;

	e_dbg("%u Mbps, %s Duplex\n",
	      *speed == SPEED_1000 ? 1000 : *speed == SPEED_100 ? 100 : 10,
	      *duplex == FULL_DUPLEX ? "Full" : "Half");

	return 0;
}

s32 e1000e_get_speed_and_duplex_fiber_serdes(struct e1000_hw *hw, u16 *speed,
					     u16 *duplex)
{
	*speed = SPEED_1000;
	*duplex = FULL_DUPLEX;

	return 0;
}

s32 e1000e_get_hw_semaphore(struct e1000_hw *hw)
{
	u32 swsm;
	s32 timeout = hw->nvm.word_size + 1;
	s32 i = 0;

	
	while (i < timeout) {
		swsm = er32(SWSM);
		if (!(swsm & E1000_SWSM_SMBI))
			break;

		udelay(50);
		i++;
	}

	if (i == timeout) {
		e_dbg("Driver can't access device - SMBI bit is set.\n");
		return -E1000_ERR_NVM;
	}

	
	for (i = 0; i < timeout; i++) {
		swsm = er32(SWSM);
		ew32(SWSM, swsm | E1000_SWSM_SWESMBI);

		
		if (er32(SWSM) & E1000_SWSM_SWESMBI)
			break;

		udelay(50);
	}

	if (i == timeout) {
		
		e1000e_put_hw_semaphore(hw);
		e_dbg("Driver can't access the NVM\n");
		return -E1000_ERR_NVM;
	}

	return 0;
}

void e1000e_put_hw_semaphore(struct e1000_hw *hw)
{
	u32 swsm;

	swsm = er32(SWSM);
	swsm &= ~(E1000_SWSM_SMBI | E1000_SWSM_SWESMBI);
	ew32(SWSM, swsm);
}

s32 e1000e_get_auto_rd_done(struct e1000_hw *hw)
{
	s32 i = 0;

	while (i < AUTO_READ_DONE_TIMEOUT) {
		if (er32(EECD) & E1000_EECD_AUTO_RD)
			break;
		usleep_range(1000, 2000);
		i++;
	}

	if (i == AUTO_READ_DONE_TIMEOUT) {
		e_dbg("Auto read by HW from NVM has not completed.\n");
		return -E1000_ERR_RESET;
	}

	return 0;
}

s32 e1000e_valid_led_default(struct e1000_hw *hw, u16 *data)
{
	s32 ret_val;

	ret_val = e1000_read_nvm(hw, NVM_ID_LED_SETTINGS, 1, data);
	if (ret_val) {
		e_dbg("NVM Read Error\n");
		return ret_val;
	}

	if (*data == ID_LED_RESERVED_0000 || *data == ID_LED_RESERVED_FFFF)
		*data = ID_LED_DEFAULT;

	return 0;
}

s32 e1000e_id_led_init_generic(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	s32 ret_val;
	const u32 ledctl_mask = 0x000000FF;
	const u32 ledctl_on = E1000_LEDCTL_MODE_LED_ON;
	const u32 ledctl_off = E1000_LEDCTL_MODE_LED_OFF;
	u16 data, i, temp;
	const u16 led_mask = 0x0F;

	ret_val = hw->nvm.ops.valid_led_default(hw, &data);
	if (ret_val)
		return ret_val;

	mac->ledctl_default = er32(LEDCTL);
	mac->ledctl_mode1 = mac->ledctl_default;
	mac->ledctl_mode2 = mac->ledctl_default;

	for (i = 0; i < 4; i++) {
		temp = (data >> (i << 2)) & led_mask;
		switch (temp) {
		case ID_LED_ON1_DEF2:
		case ID_LED_ON1_ON2:
		case ID_LED_ON1_OFF2:
			mac->ledctl_mode1 &= ~(ledctl_mask << (i << 3));
			mac->ledctl_mode1 |= ledctl_on << (i << 3);
			break;
		case ID_LED_OFF1_DEF2:
		case ID_LED_OFF1_ON2:
		case ID_LED_OFF1_OFF2:
			mac->ledctl_mode1 &= ~(ledctl_mask << (i << 3));
			mac->ledctl_mode1 |= ledctl_off << (i << 3);
			break;
		default:
			
			break;
		}
		switch (temp) {
		case ID_LED_DEF1_ON2:
		case ID_LED_ON1_ON2:
		case ID_LED_OFF1_ON2:
			mac->ledctl_mode2 &= ~(ledctl_mask << (i << 3));
			mac->ledctl_mode2 |= ledctl_on << (i << 3);
			break;
		case ID_LED_DEF1_OFF2:
		case ID_LED_ON1_OFF2:
		case ID_LED_OFF1_OFF2:
			mac->ledctl_mode2 &= ~(ledctl_mask << (i << 3));
			mac->ledctl_mode2 |= ledctl_off << (i << 3);
			break;
		default:
			
			break;
		}
	}

	return 0;
}

s32 e1000e_setup_led_generic(struct e1000_hw *hw)
{
	u32 ledctl;

	if (hw->mac.ops.setup_led != e1000e_setup_led_generic)
		return -E1000_ERR_CONFIG;

	if (hw->phy.media_type == e1000_media_type_fiber) {
		ledctl = er32(LEDCTL);
		hw->mac.ledctl_default = ledctl;
		
		ledctl &= ~(E1000_LEDCTL_LED0_IVRT | E1000_LEDCTL_LED0_BLINK |
			    E1000_LEDCTL_LED0_MODE_MASK);
		ledctl |= (E1000_LEDCTL_MODE_LED_OFF <<
			   E1000_LEDCTL_LED0_MODE_SHIFT);
		ew32(LEDCTL, ledctl);
	} else if (hw->phy.media_type == e1000_media_type_copper) {
		ew32(LEDCTL, hw->mac.ledctl_mode1);
	}

	return 0;
}

s32 e1000e_cleanup_led_generic(struct e1000_hw *hw)
{
	ew32(LEDCTL, hw->mac.ledctl_default);
	return 0;
}

s32 e1000e_blink_led_generic(struct e1000_hw *hw)
{
	u32 ledctl_blink = 0;
	u32 i;

	if (hw->phy.media_type == e1000_media_type_fiber) {
		
		ledctl_blink = E1000_LEDCTL_LED0_BLINK |
		    (E1000_LEDCTL_MODE_LED_ON << E1000_LEDCTL_LED0_MODE_SHIFT);
	} else {
		ledctl_blink = hw->mac.ledctl_mode2;
		for (i = 0; i < 4; i++)
			if (((hw->mac.ledctl_mode2 >> (i * 8)) & 0xFF) ==
			    E1000_LEDCTL_MODE_LED_ON)
				ledctl_blink |= (E1000_LEDCTL_LED0_BLINK <<
						 (i * 8));
	}

	ew32(LEDCTL, ledctl_blink);

	return 0;
}

s32 e1000e_led_on_generic(struct e1000_hw *hw)
{
	u32 ctrl;

	switch (hw->phy.media_type) {
	case e1000_media_type_fiber:
		ctrl = er32(CTRL);
		ctrl &= ~E1000_CTRL_SWDPIN0;
		ctrl |= E1000_CTRL_SWDPIO0;
		ew32(CTRL, ctrl);
		break;
	case e1000_media_type_copper:
		ew32(LEDCTL, hw->mac.ledctl_mode2);
		break;
	default:
		break;
	}

	return 0;
}

s32 e1000e_led_off_generic(struct e1000_hw *hw)
{
	u32 ctrl;

	switch (hw->phy.media_type) {
	case e1000_media_type_fiber:
		ctrl = er32(CTRL);
		ctrl |= E1000_CTRL_SWDPIN0;
		ctrl |= E1000_CTRL_SWDPIO0;
		ew32(CTRL, ctrl);
		break;
	case e1000_media_type_copper:
		ew32(LEDCTL, hw->mac.ledctl_mode1);
		break;
	default:
		break;
	}

	return 0;
}

void e1000e_set_pcie_no_snoop(struct e1000_hw *hw, u32 no_snoop)
{
	u32 gcr;

	if (no_snoop) {
		gcr = er32(GCR);
		gcr &= ~(PCIE_NO_SNOOP_ALL);
		gcr |= no_snoop;
		ew32(GCR, gcr);
	}
}

s32 e1000e_disable_pcie_master(struct e1000_hw *hw)
{
	u32 ctrl;
	s32 timeout = MASTER_DISABLE_TIMEOUT;

	ctrl = er32(CTRL);
	ctrl |= E1000_CTRL_GIO_MASTER_DISABLE;
	ew32(CTRL, ctrl);

	while (timeout) {
		if (!(er32(STATUS) & E1000_STATUS_GIO_MASTER_ENABLE))
			break;
		udelay(100);
		timeout--;
	}

	if (!timeout) {
		e_dbg("Master requests are pending.\n");
		return -E1000_ERR_MASTER_REQUESTS_PENDING;
	}

	return 0;
}

void e1000e_reset_adaptive(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;

	if (!mac->adaptive_ifs) {
		e_dbg("Not in Adaptive IFS mode!\n");
		return;
	}

	mac->current_ifs_val = 0;
	mac->ifs_min_val = IFS_MIN;
	mac->ifs_max_val = IFS_MAX;
	mac->ifs_step_size = IFS_STEP;
	mac->ifs_ratio = IFS_RATIO;

	mac->in_ifs_mode = false;
	ew32(AIT, 0);
}

void e1000e_update_adaptive(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;

	if (!mac->adaptive_ifs) {
		e_dbg("Not in Adaptive IFS mode!\n");
		return;
	}

	if ((mac->collision_delta * mac->ifs_ratio) > mac->tx_packet_delta) {
		if (mac->tx_packet_delta > MIN_NUM_XMITS) {
			mac->in_ifs_mode = true;
			if (mac->current_ifs_val < mac->ifs_max_val) {
				if (!mac->current_ifs_val)
					mac->current_ifs_val = mac->ifs_min_val;
				else
					mac->current_ifs_val +=
					    mac->ifs_step_size;
				ew32(AIT, mac->current_ifs_val);
			}
		}
	} else {
		if (mac->in_ifs_mode &&
		    (mac->tx_packet_delta <= MIN_NUM_XMITS)) {
			mac->current_ifs_val = 0;
			mac->in_ifs_mode = false;
			ew32(AIT, 0);
		}
	}
}
