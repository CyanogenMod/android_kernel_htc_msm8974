/*******************************************************************************

  Intel(R) Gigabit Ethernet Linux driver
  Copyright(c) 2007-2012 Intel Corporation.

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
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#include <linux/if_ether.h>
#include <linux/delay.h>

#include "e1000_mac.h"
#include "e1000_phy.h"

static s32  igb_phy_setup_autoneg(struct e1000_hw *hw);
static void igb_phy_force_speed_duplex_setup(struct e1000_hw *hw,
					       u16 *phy_ctrl);
static s32  igb_wait_autoneg(struct e1000_hw *hw);

static const u16 e1000_m88_cable_length_table[] =
	{ 0, 50, 80, 110, 140, 140, E1000_CABLE_LENGTH_UNDEFINED };
#define M88E1000_CABLE_LENGTH_TABLE_SIZE \
                (sizeof(e1000_m88_cable_length_table) / \
                 sizeof(e1000_m88_cable_length_table[0]))

static const u16 e1000_igp_2_cable_length_table[] =
    { 0, 0, 0, 0, 0, 0, 0, 0, 3, 5, 8, 11, 13, 16, 18, 21,
      0, 0, 0, 3, 6, 10, 13, 16, 19, 23, 26, 29, 32, 35, 38, 41,
      6, 10, 14, 18, 22, 26, 30, 33, 37, 41, 44, 48, 51, 54, 58, 61,
      21, 26, 31, 35, 40, 44, 49, 53, 57, 61, 65, 68, 72, 75, 79, 82,
      40, 45, 51, 56, 61, 66, 70, 75, 79, 83, 87, 91, 94, 98, 101, 104,
      60, 66, 72, 77, 82, 87, 92, 96, 100, 104, 108, 111, 114, 117, 119, 121,
      83, 89, 95, 100, 105, 109, 113, 116, 119, 122, 124,
      104, 109, 114, 118, 121, 124};
#define IGP02E1000_CABLE_LENGTH_TABLE_SIZE \
		(sizeof(e1000_igp_2_cable_length_table) / \
		 sizeof(e1000_igp_2_cable_length_table[0]))

s32 igb_check_reset_block(struct e1000_hw *hw)
{
	u32 manc;

	manc = rd32(E1000_MANC);

	return (manc & E1000_MANC_BLK_PHY_RST_ON_IDE) ?
	       E1000_BLK_PHY_RESET : 0;
}

s32 igb_get_phy_id(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val = 0;
	u16 phy_id;

	ret_val = phy->ops.read_reg(hw, PHY_ID1, &phy_id);
	if (ret_val)
		goto out;

	phy->id = (u32)(phy_id << 16);
	udelay(20);
	ret_val = phy->ops.read_reg(hw, PHY_ID2, &phy_id);
	if (ret_val)
		goto out;

	phy->id |= (u32)(phy_id & PHY_REVISION_MASK);
	phy->revision = (u32)(phy_id & ~PHY_REVISION_MASK);

out:
	return ret_val;
}

static s32 igb_phy_reset_dsp(struct e1000_hw *hw)
{
	s32 ret_val = 0;

	if (!(hw->phy.ops.write_reg))
		goto out;

	ret_val = hw->phy.ops.write_reg(hw, M88E1000_PHY_GEN_CONTROL, 0xC1);
	if (ret_val)
		goto out;

	ret_val = hw->phy.ops.write_reg(hw, M88E1000_PHY_GEN_CONTROL, 0);

out:
	return ret_val;
}

s32 igb_read_phy_reg_mdic(struct e1000_hw *hw, u32 offset, u16 *data)
{
	struct e1000_phy_info *phy = &hw->phy;
	u32 i, mdic = 0;
	s32 ret_val = 0;

	if (offset > MAX_PHY_REG_ADDRESS) {
		hw_dbg("PHY Address %d is out of range\n", offset);
		ret_val = -E1000_ERR_PARAM;
		goto out;
	}

	mdic = ((offset << E1000_MDIC_REG_SHIFT) |
		(phy->addr << E1000_MDIC_PHY_SHIFT) |
		(E1000_MDIC_OP_READ));

	wr32(E1000_MDIC, mdic);

	for (i = 0; i < (E1000_GEN_POLL_TIMEOUT * 3); i++) {
		udelay(50);
		mdic = rd32(E1000_MDIC);
		if (mdic & E1000_MDIC_READY)
			break;
	}
	if (!(mdic & E1000_MDIC_READY)) {
		hw_dbg("MDI Read did not complete\n");
		ret_val = -E1000_ERR_PHY;
		goto out;
	}
	if (mdic & E1000_MDIC_ERROR) {
		hw_dbg("MDI Error\n");
		ret_val = -E1000_ERR_PHY;
		goto out;
	}
	*data = (u16) mdic;

out:
	return ret_val;
}

s32 igb_write_phy_reg_mdic(struct e1000_hw *hw, u32 offset, u16 data)
{
	struct e1000_phy_info *phy = &hw->phy;
	u32 i, mdic = 0;
	s32 ret_val = 0;

	if (offset > MAX_PHY_REG_ADDRESS) {
		hw_dbg("PHY Address %d is out of range\n", offset);
		ret_val = -E1000_ERR_PARAM;
		goto out;
	}

	mdic = (((u32)data) |
		(offset << E1000_MDIC_REG_SHIFT) |
		(phy->addr << E1000_MDIC_PHY_SHIFT) |
		(E1000_MDIC_OP_WRITE));

	wr32(E1000_MDIC, mdic);

	for (i = 0; i < (E1000_GEN_POLL_TIMEOUT * 3); i++) {
		udelay(50);
		mdic = rd32(E1000_MDIC);
		if (mdic & E1000_MDIC_READY)
			break;
	}
	if (!(mdic & E1000_MDIC_READY)) {
		hw_dbg("MDI Write did not complete\n");
		ret_val = -E1000_ERR_PHY;
		goto out;
	}
	if (mdic & E1000_MDIC_ERROR) {
		hw_dbg("MDI Error\n");
		ret_val = -E1000_ERR_PHY;
		goto out;
	}

out:
	return ret_val;
}

s32 igb_read_phy_reg_i2c(struct e1000_hw *hw, u32 offset, u16 *data)
{
	struct e1000_phy_info *phy = &hw->phy;
	u32 i, i2ccmd = 0;


	i2ccmd = ((offset << E1000_I2CCMD_REG_ADDR_SHIFT) |
	          (phy->addr << E1000_I2CCMD_PHY_ADDR_SHIFT) |
	          (E1000_I2CCMD_OPCODE_READ));

	wr32(E1000_I2CCMD, i2ccmd);

	
	for (i = 0; i < E1000_I2CCMD_PHY_TIMEOUT; i++) {
		udelay(50);
		i2ccmd = rd32(E1000_I2CCMD);
		if (i2ccmd & E1000_I2CCMD_READY)
			break;
	}
	if (!(i2ccmd & E1000_I2CCMD_READY)) {
		hw_dbg("I2CCMD Read did not complete\n");
		return -E1000_ERR_PHY;
	}
	if (i2ccmd & E1000_I2CCMD_ERROR) {
		hw_dbg("I2CCMD Error bit set\n");
		return -E1000_ERR_PHY;
	}

	
	*data = ((i2ccmd >> 8) & 0x00FF) | ((i2ccmd << 8) & 0xFF00);

	return 0;
}

s32 igb_write_phy_reg_i2c(struct e1000_hw *hw, u32 offset, u16 data)
{
	struct e1000_phy_info *phy = &hw->phy;
	u32 i, i2ccmd = 0;
	u16 phy_data_swapped;

	
	if ((hw->phy.addr == 0) || (hw->phy.addr > 7)) {
		hw_dbg("PHY I2C Address %d is out of range.\n",
			  hw->phy.addr);
		return -E1000_ERR_CONFIG;
	}

	
	phy_data_swapped = ((data >> 8) & 0x00FF) | ((data << 8) & 0xFF00);

	i2ccmd = ((offset << E1000_I2CCMD_REG_ADDR_SHIFT) |
	          (phy->addr << E1000_I2CCMD_PHY_ADDR_SHIFT) |
	          E1000_I2CCMD_OPCODE_WRITE |
	          phy_data_swapped);

	wr32(E1000_I2CCMD, i2ccmd);

	
	for (i = 0; i < E1000_I2CCMD_PHY_TIMEOUT; i++) {
		udelay(50);
		i2ccmd = rd32(E1000_I2CCMD);
		if (i2ccmd & E1000_I2CCMD_READY)
			break;
	}
	if (!(i2ccmd & E1000_I2CCMD_READY)) {
		hw_dbg("I2CCMD Write did not complete\n");
		return -E1000_ERR_PHY;
	}
	if (i2ccmd & E1000_I2CCMD_ERROR) {
		hw_dbg("I2CCMD Error bit set\n");
		return -E1000_ERR_PHY;
	}

	return 0;
}

s32 igb_read_phy_reg_igp(struct e1000_hw *hw, u32 offset, u16 *data)
{
	s32 ret_val = 0;

	if (!(hw->phy.ops.acquire))
		goto out;

	ret_val = hw->phy.ops.acquire(hw);
	if (ret_val)
		goto out;

	if (offset > MAX_PHY_MULTI_PAGE_REG) {
		ret_val = igb_write_phy_reg_mdic(hw,
						   IGP01E1000_PHY_PAGE_SELECT,
						   (u16)offset);
		if (ret_val) {
			hw->phy.ops.release(hw);
			goto out;
		}
	}

	ret_val = igb_read_phy_reg_mdic(hw, MAX_PHY_REG_ADDRESS & offset,
					data);

	hw->phy.ops.release(hw);

out:
	return ret_val;
}

s32 igb_write_phy_reg_igp(struct e1000_hw *hw, u32 offset, u16 data)
{
	s32 ret_val = 0;

	if (!(hw->phy.ops.acquire))
		goto out;

	ret_val = hw->phy.ops.acquire(hw);
	if (ret_val)
		goto out;

	if (offset > MAX_PHY_MULTI_PAGE_REG) {
		ret_val = igb_write_phy_reg_mdic(hw,
						   IGP01E1000_PHY_PAGE_SELECT,
						   (u16)offset);
		if (ret_val) {
			hw->phy.ops.release(hw);
			goto out;
		}
	}

	ret_val = igb_write_phy_reg_mdic(hw, MAX_PHY_REG_ADDRESS & offset,
					   data);

	hw->phy.ops.release(hw);

out:
	return ret_val;
}

s32 igb_copper_link_setup_82580(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data;


	if (phy->reset_disable) {
		ret_val = 0;
		goto out;
	}

	if (phy->type == e1000_phy_82580) {
		ret_val = hw->phy.ops.reset(hw);
		if (ret_val) {
			hw_dbg("Error resetting the PHY.\n");
			goto out;
		}
	}

	
	ret_val = phy->ops.read_reg(hw, I82580_CFG_REG, &phy_data);
	if (ret_val)
		goto out;

	phy_data |= I82580_CFG_ASSERT_CRS_ON_TX;

	
	phy_data |= I82580_CFG_ENABLE_DOWNSHIFT;

	ret_val = phy->ops.write_reg(hw, I82580_CFG_REG, phy_data);

out:
	return ret_val;
}

s32 igb_copper_link_setup_m88(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data;

	if (phy->reset_disable) {
		ret_val = 0;
		goto out;
	}

	
	ret_val = phy->ops.read_reg(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		goto out;

	phy_data |= M88E1000_PSCR_ASSERT_CRS_ON_TX;

	phy_data &= ~M88E1000_PSCR_AUTO_X_MODE;

	switch (phy->mdix) {
	case 1:
		phy_data |= M88E1000_PSCR_MDI_MANUAL_MODE;
		break;
	case 2:
		phy_data |= M88E1000_PSCR_MDIX_MANUAL_MODE;
		break;
	case 3:
		phy_data |= M88E1000_PSCR_AUTO_X_1000T;
		break;
	case 0:
	default:
		phy_data |= M88E1000_PSCR_AUTO_X_MODE;
		break;
	}

	phy_data &= ~M88E1000_PSCR_POLARITY_REVERSAL;
	if (phy->disable_polarity_correction == 1)
		phy_data |= M88E1000_PSCR_POLARITY_REVERSAL;

	ret_val = phy->ops.write_reg(hw, M88E1000_PHY_SPEC_CTRL, phy_data);
	if (ret_val)
		goto out;

	if (phy->revision < E1000_REVISION_4) {
		ret_val = phy->ops.read_reg(hw, M88E1000_EXT_PHY_SPEC_CTRL,
					     &phy_data);
		if (ret_val)
			goto out;

		phy_data |= M88E1000_EPSCR_TX_CLK_25;

		if ((phy->revision == E1000_REVISION_2) &&
		    (phy->id == M88E1111_I_PHY_ID)) {
			
			phy_data &= ~M88EC018_EPSCR_DOWNSHIFT_COUNTER_MASK;
			phy_data |= M88EC018_EPSCR_DOWNSHIFT_COUNTER_5X;
		} else {
			
			phy_data &= ~(M88E1000_EPSCR_MASTER_DOWNSHIFT_MASK |
				      M88E1000_EPSCR_SLAVE_DOWNSHIFT_MASK);
			phy_data |= (M88E1000_EPSCR_MASTER_DOWNSHIFT_1X |
				     M88E1000_EPSCR_SLAVE_DOWNSHIFT_1X);
		}
		ret_val = phy->ops.write_reg(hw, M88E1000_EXT_PHY_SPEC_CTRL,
					     phy_data);
		if (ret_val)
			goto out;
	}

	
	ret_val = igb_phy_sw_reset(hw);
	if (ret_val) {
		hw_dbg("Error committing the PHY changes\n");
		goto out;
	}

out:
	return ret_val;
}

s32 igb_copper_link_setup_m88_gen2(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data;

	if (phy->reset_disable) {
		ret_val = 0;
		goto out;
	}

	
	ret_val = phy->ops.read_reg(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		goto out;

	phy_data &= ~M88E1000_PSCR_AUTO_X_MODE;

	switch (phy->mdix) {
	case 1:
		phy_data |= M88E1000_PSCR_MDI_MANUAL_MODE;
		break;
	case 2:
		phy_data |= M88E1000_PSCR_MDIX_MANUAL_MODE;
		break;
	case 3:
		
		if (phy->id != M88E1112_E_PHY_ID) {
			phy_data |= M88E1000_PSCR_AUTO_X_1000T;
			break;
		}
	case 0:
	default:
		phy_data |= M88E1000_PSCR_AUTO_X_MODE;
		break;
	}

	phy_data &= ~M88E1000_PSCR_POLARITY_REVERSAL;
	if (phy->disable_polarity_correction == 1)
		phy_data |= M88E1000_PSCR_POLARITY_REVERSAL;

	
	phy_data &= ~I347AT4_PSCR_DOWNSHIFT_MASK;
	phy_data |= I347AT4_PSCR_DOWNSHIFT_6X;
	phy_data |= I347AT4_PSCR_DOWNSHIFT_ENABLE;

	ret_val = phy->ops.write_reg(hw, M88E1000_PHY_SPEC_CTRL, phy_data);
	if (ret_val)
		goto out;

	
	ret_val = igb_phy_sw_reset(hw);
	if (ret_val) {
		hw_dbg("Error committing the PHY changes\n");
		goto out;
	}

out:
	return ret_val;
}

s32 igb_copper_link_setup_igp(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data;

	if (phy->reset_disable) {
		ret_val = 0;
		goto out;
	}

	ret_val = phy->ops.reset(hw);
	if (ret_val) {
		hw_dbg("Error resetting the PHY.\n");
		goto out;
	}

	msleep(100);

	if (phy->type == e1000_phy_igp) {
		
		if (phy->ops.set_d3_lplu_state)
			ret_val = phy->ops.set_d3_lplu_state(hw, false);
		if (ret_val) {
			hw_dbg("Error Disabling LPLU D3\n");
			goto out;
		}
	}

	
	ret_val = phy->ops.set_d0_lplu_state(hw, false);
	if (ret_val) {
		hw_dbg("Error Disabling LPLU D0\n");
		goto out;
	}
	
	ret_val = phy->ops.read_reg(hw, IGP01E1000_PHY_PORT_CTRL, &data);
	if (ret_val)
		goto out;

	data &= ~IGP01E1000_PSCR_AUTO_MDIX;

	switch (phy->mdix) {
	case 1:
		data &= ~IGP01E1000_PSCR_FORCE_MDI_MDIX;
		break;
	case 2:
		data |= IGP01E1000_PSCR_FORCE_MDI_MDIX;
		break;
	case 0:
	default:
		data |= IGP01E1000_PSCR_AUTO_MDIX;
		break;
	}
	ret_val = phy->ops.write_reg(hw, IGP01E1000_PHY_PORT_CTRL, data);
	if (ret_val)
		goto out;

	
	if (hw->mac.autoneg) {
		if (phy->autoneg_advertised == ADVERTISE_1000_FULL) {
			
			ret_val = phy->ops.read_reg(hw,
						    IGP01E1000_PHY_PORT_CONFIG,
						    &data);
			if (ret_val)
				goto out;

			data &= ~IGP01E1000_PSCFR_SMART_SPEED;
			ret_val = phy->ops.write_reg(hw,
						     IGP01E1000_PHY_PORT_CONFIG,
						     data);
			if (ret_val)
				goto out;

			
			ret_val = phy->ops.read_reg(hw, PHY_1000T_CTRL, &data);
			if (ret_val)
				goto out;

			data &= ~CR_1000T_MS_ENABLE;
			ret_val = phy->ops.write_reg(hw, PHY_1000T_CTRL, data);
			if (ret_val)
				goto out;
		}

		ret_val = phy->ops.read_reg(hw, PHY_1000T_CTRL, &data);
		if (ret_val)
			goto out;

		
		phy->original_ms_type = (data & CR_1000T_MS_ENABLE) ?
			((data & CR_1000T_MS_VALUE) ?
			e1000_ms_force_master :
			e1000_ms_force_slave) :
			e1000_ms_auto;

		switch (phy->ms_type) {
		case e1000_ms_force_master:
			data |= (CR_1000T_MS_ENABLE | CR_1000T_MS_VALUE);
			break;
		case e1000_ms_force_slave:
			data |= CR_1000T_MS_ENABLE;
			data &= ~(CR_1000T_MS_VALUE);
			break;
		case e1000_ms_auto:
			data &= ~CR_1000T_MS_ENABLE;
		default:
			break;
		}
		ret_val = phy->ops.write_reg(hw, PHY_1000T_CTRL, data);
		if (ret_val)
			goto out;
	}

out:
	return ret_val;
}

static s32 igb_copper_link_autoneg(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_ctrl;

	phy->autoneg_advertised &= phy->autoneg_mask;

	if (phy->autoneg_advertised == 0)
		phy->autoneg_advertised = phy->autoneg_mask;

	hw_dbg("Reconfiguring auto-neg advertisement params\n");
	ret_val = igb_phy_setup_autoneg(hw);
	if (ret_val) {
		hw_dbg("Error Setting up Auto-Negotiation\n");
		goto out;
	}
	hw_dbg("Restarting Auto-Neg\n");

	ret_val = phy->ops.read_reg(hw, PHY_CONTROL, &phy_ctrl);
	if (ret_val)
		goto out;

	phy_ctrl |= (MII_CR_AUTO_NEG_EN | MII_CR_RESTART_AUTO_NEG);
	ret_val = phy->ops.write_reg(hw, PHY_CONTROL, phy_ctrl);
	if (ret_val)
		goto out;

	if (phy->autoneg_wait_to_complete) {
		ret_val = igb_wait_autoneg(hw);
		if (ret_val) {
			hw_dbg("Error while waiting for "
			       "autoneg to complete\n");
			goto out;
		}
	}

	hw->mac.get_link_status = true;

out:
	return ret_val;
}

static s32 igb_phy_setup_autoneg(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 mii_autoneg_adv_reg;
	u16 mii_1000t_ctrl_reg = 0;

	phy->autoneg_advertised &= phy->autoneg_mask;

	
	ret_val = phy->ops.read_reg(hw, PHY_AUTONEG_ADV, &mii_autoneg_adv_reg);
	if (ret_val)
		goto out;

	if (phy->autoneg_mask & ADVERTISE_1000_FULL) {
		
		ret_val = phy->ops.read_reg(hw, PHY_1000T_CTRL,
					    &mii_1000t_ctrl_reg);
		if (ret_val)
			goto out;
	}


	mii_autoneg_adv_reg &= ~(NWAY_AR_100TX_FD_CAPS |
				 NWAY_AR_100TX_HD_CAPS |
				 NWAY_AR_10T_FD_CAPS   |
				 NWAY_AR_10T_HD_CAPS);
	mii_1000t_ctrl_reg &= ~(CR_1000T_HD_CAPS | CR_1000T_FD_CAPS);

	hw_dbg("autoneg_advertised %x\n", phy->autoneg_advertised);

	
	if (phy->autoneg_advertised & ADVERTISE_10_HALF) {
		hw_dbg("Advertise 10mb Half duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_10T_HD_CAPS;
	}

	
	if (phy->autoneg_advertised & ADVERTISE_10_FULL) {
		hw_dbg("Advertise 10mb Full duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_10T_FD_CAPS;
	}

	
	if (phy->autoneg_advertised & ADVERTISE_100_HALF) {
		hw_dbg("Advertise 100mb Half duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_100TX_HD_CAPS;
	}

	
	if (phy->autoneg_advertised & ADVERTISE_100_FULL) {
		hw_dbg("Advertise 100mb Full duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_100TX_FD_CAPS;
	}

	
	if (phy->autoneg_advertised & ADVERTISE_1000_HALF)
		hw_dbg("Advertise 1000mb Half duplex request denied!\n");

	
	if (phy->autoneg_advertised & ADVERTISE_1000_FULL) {
		hw_dbg("Advertise 1000mb Full duplex\n");
		mii_1000t_ctrl_reg |= CR_1000T_FD_CAPS;
	}

	switch (hw->fc.current_mode) {
	case e1000_fc_none:
		mii_autoneg_adv_reg &= ~(NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
		break;
	case e1000_fc_rx_pause:
		mii_autoneg_adv_reg |= (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
		break;
	case e1000_fc_tx_pause:
		mii_autoneg_adv_reg |= NWAY_AR_ASM_DIR;
		mii_autoneg_adv_reg &= ~NWAY_AR_PAUSE;
		break;
	case e1000_fc_full:
		mii_autoneg_adv_reg |= (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
		break;
	default:
		hw_dbg("Flow control param set incorrectly\n");
		ret_val = -E1000_ERR_CONFIG;
		goto out;
	}

	ret_val = phy->ops.write_reg(hw, PHY_AUTONEG_ADV, mii_autoneg_adv_reg);
	if (ret_val)
		goto out;

	hw_dbg("Auto-Neg Advertising %x\n", mii_autoneg_adv_reg);

	if (phy->autoneg_mask & ADVERTISE_1000_FULL) {
		ret_val = phy->ops.write_reg(hw,
					     PHY_1000T_CTRL,
					     mii_1000t_ctrl_reg);
		if (ret_val)
			goto out;
	}

out:
	return ret_val;
}

s32 igb_setup_copper_link(struct e1000_hw *hw)
{
	s32 ret_val;
	bool link;


	if (hw->mac.autoneg) {
		ret_val = igb_copper_link_autoneg(hw);
		if (ret_val)
			goto out;
	} else {
		hw_dbg("Forcing Speed and Duplex\n");
		ret_val = hw->phy.ops.force_speed_duplex(hw);
		if (ret_val) {
			hw_dbg("Error Forcing Speed and Duplex\n");
			goto out;
		}
	}

	ret_val = igb_phy_has_link(hw,
	                           COPPER_LINK_UP_LIMIT,
	                           10,
	                           &link);
	if (ret_val)
		goto out;

	if (link) {
		hw_dbg("Valid link established!!!\n");
		igb_config_collision_dist(hw);
		ret_val = igb_config_fc_after_link_up(hw);
	} else {
		hw_dbg("Unable to establish link!!!\n");
	}

out:
	return ret_val;
}

s32 igb_phy_force_speed_duplex_igp(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data;
	bool link;

	ret_val = phy->ops.read_reg(hw, PHY_CONTROL, &phy_data);
	if (ret_val)
		goto out;

	igb_phy_force_speed_duplex_setup(hw, &phy_data);

	ret_val = phy->ops.write_reg(hw, PHY_CONTROL, phy_data);
	if (ret_val)
		goto out;

	ret_val = phy->ops.read_reg(hw, IGP01E1000_PHY_PORT_CTRL, &phy_data);
	if (ret_val)
		goto out;

	phy_data &= ~IGP01E1000_PSCR_AUTO_MDIX;
	phy_data &= ~IGP01E1000_PSCR_FORCE_MDI_MDIX;

	ret_val = phy->ops.write_reg(hw, IGP01E1000_PHY_PORT_CTRL, phy_data);
	if (ret_val)
		goto out;

	hw_dbg("IGP PSCR: %X\n", phy_data);

	udelay(1);

	if (phy->autoneg_wait_to_complete) {
		hw_dbg("Waiting for forced speed/duplex link on IGP phy.\n");

		ret_val = igb_phy_has_link(hw,
						     PHY_FORCE_LIMIT,
						     100000,
						     &link);
		if (ret_val)
			goto out;

		if (!link)
			hw_dbg("Link taking longer than expected.\n");

		
		ret_val = igb_phy_has_link(hw,
						     PHY_FORCE_LIMIT,
						     100000,
						     &link);
		if (ret_val)
			goto out;
	}

out:
	return ret_val;
}

s32 igb_phy_force_speed_duplex_m88(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data;
	bool link;

	ret_val = phy->ops.read_reg(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		goto out;

	phy_data &= ~M88E1000_PSCR_AUTO_X_MODE;
	ret_val = phy->ops.write_reg(hw, M88E1000_PHY_SPEC_CTRL, phy_data);
	if (ret_val)
		goto out;

	hw_dbg("M88E1000 PSCR: %X\n", phy_data);

	ret_val = phy->ops.read_reg(hw, PHY_CONTROL, &phy_data);
	if (ret_val)
		goto out;

	igb_phy_force_speed_duplex_setup(hw, &phy_data);

	ret_val = phy->ops.write_reg(hw, PHY_CONTROL, phy_data);
	if (ret_val)
		goto out;

	
	ret_val = igb_phy_sw_reset(hw);
	if (ret_val)
		goto out;

	if (phy->autoneg_wait_to_complete) {
		hw_dbg("Waiting for forced speed/duplex link on M88 phy.\n");

		ret_val = igb_phy_has_link(hw, PHY_FORCE_LIMIT, 100000, &link);
		if (ret_val)
			goto out;

		if (!link) {
			if (hw->phy.type != e1000_phy_m88 ||
			    hw->phy.id == I347AT4_E_PHY_ID ||
			    hw->phy.id == M88E1112_E_PHY_ID) {
				hw_dbg("Link taking longer than expected.\n");
			} else {

				ret_val = phy->ops.write_reg(hw,
							     M88E1000_PHY_PAGE_SELECT,
							     0x001d);
				if (ret_val)
					goto out;
				ret_val = igb_phy_reset_dsp(hw);
				if (ret_val)
					goto out;
			}
		}

		
		ret_val = igb_phy_has_link(hw, PHY_FORCE_LIMIT,
					   100000, &link);
		if (ret_val)
			goto out;
	}

	if (hw->phy.type != e1000_phy_m88 ||
	    hw->phy.id == I347AT4_E_PHY_ID ||
	    hw->phy.id == M88E1112_E_PHY_ID)
		goto out;

	ret_val = phy->ops.read_reg(hw, M88E1000_EXT_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		goto out;

	phy_data |= M88E1000_EPSCR_TX_CLK_25;
	ret_val = phy->ops.write_reg(hw, M88E1000_EXT_PHY_SPEC_CTRL, phy_data);
	if (ret_val)
		goto out;

	ret_val = phy->ops.read_reg(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		goto out;

	phy_data |= M88E1000_PSCR_ASSERT_CRS_ON_TX;
	ret_val = phy->ops.write_reg(hw, M88E1000_PHY_SPEC_CTRL, phy_data);

out:
	return ret_val;
}

static void igb_phy_force_speed_duplex_setup(struct e1000_hw *hw,
					       u16 *phy_ctrl)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 ctrl;

	
	hw->fc.current_mode = e1000_fc_none;

	
	ctrl = rd32(E1000_CTRL);
	ctrl |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
	ctrl &= ~E1000_CTRL_SPD_SEL;

	
	ctrl &= ~E1000_CTRL_ASDE;

	
	*phy_ctrl &= ~MII_CR_AUTO_NEG_EN;

	
	if (mac->forced_speed_duplex & E1000_ALL_HALF_DUPLEX) {
		ctrl &= ~E1000_CTRL_FD;
		*phy_ctrl &= ~MII_CR_FULL_DUPLEX;
		hw_dbg("Half Duplex\n");
	} else {
		ctrl |= E1000_CTRL_FD;
		*phy_ctrl |= MII_CR_FULL_DUPLEX;
		hw_dbg("Full Duplex\n");
	}

	
	if (mac->forced_speed_duplex & E1000_ALL_100_SPEED) {
		ctrl |= E1000_CTRL_SPD_100;
		*phy_ctrl |= MII_CR_SPEED_100;
		*phy_ctrl &= ~(MII_CR_SPEED_1000 | MII_CR_SPEED_10);
		hw_dbg("Forcing 100mb\n");
	} else {
		ctrl &= ~(E1000_CTRL_SPD_1000 | E1000_CTRL_SPD_100);
		*phy_ctrl |= MII_CR_SPEED_10;
		*phy_ctrl &= ~(MII_CR_SPEED_1000 | MII_CR_SPEED_100);
		hw_dbg("Forcing 10mb\n");
	}

	igb_config_collision_dist(hw);

	wr32(E1000_CTRL, ctrl);
}

s32 igb_set_d3_lplu_state(struct e1000_hw *hw, bool active)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val = 0;
	u16 data;

	if (!(hw->phy.ops.read_reg))
		goto out;

	ret_val = phy->ops.read_reg(hw, IGP02E1000_PHY_POWER_MGMT, &data);
	if (ret_val)
		goto out;

	if (!active) {
		data &= ~IGP02E1000_PM_D3_LPLU;
		ret_val = phy->ops.write_reg(hw, IGP02E1000_PHY_POWER_MGMT,
					     data);
		if (ret_val)
			goto out;
		if (phy->smart_speed == e1000_smart_speed_on) {
			ret_val = phy->ops.read_reg(hw,
						    IGP01E1000_PHY_PORT_CONFIG,
						    &data);
			if (ret_val)
				goto out;

			data |= IGP01E1000_PSCFR_SMART_SPEED;
			ret_val = phy->ops.write_reg(hw,
						     IGP01E1000_PHY_PORT_CONFIG,
						     data);
			if (ret_val)
				goto out;
		} else if (phy->smart_speed == e1000_smart_speed_off) {
			ret_val = phy->ops.read_reg(hw,
						     IGP01E1000_PHY_PORT_CONFIG,
						     &data);
			if (ret_val)
				goto out;

			data &= ~IGP01E1000_PSCFR_SMART_SPEED;
			ret_val = phy->ops.write_reg(hw,
						     IGP01E1000_PHY_PORT_CONFIG,
						     data);
			if (ret_val)
				goto out;
		}
	} else if ((phy->autoneg_advertised == E1000_ALL_SPEED_DUPLEX) ||
		   (phy->autoneg_advertised == E1000_ALL_NOT_GIG) ||
		   (phy->autoneg_advertised == E1000_ALL_10_SPEED)) {
		data |= IGP02E1000_PM_D3_LPLU;
		ret_val = phy->ops.write_reg(hw, IGP02E1000_PHY_POWER_MGMT,
					      data);
		if (ret_val)
			goto out;

		
		ret_val = phy->ops.read_reg(hw, IGP01E1000_PHY_PORT_CONFIG,
					     &data);
		if (ret_val)
			goto out;

		data &= ~IGP01E1000_PSCFR_SMART_SPEED;
		ret_val = phy->ops.write_reg(hw, IGP01E1000_PHY_PORT_CONFIG,
					      data);
	}

out:
	return ret_val;
}

s32 igb_check_downshift(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data, offset, mask;

	switch (phy->type) {
	case e1000_phy_m88:
	case e1000_phy_gg82563:
		offset	= M88E1000_PHY_SPEC_STATUS;
		mask	= M88E1000_PSSR_DOWNSHIFT;
		break;
	case e1000_phy_igp_2:
	case e1000_phy_igp:
	case e1000_phy_igp_3:
		offset	= IGP01E1000_PHY_LINK_HEALTH;
		mask	= IGP01E1000_PLHR_SS_DOWNGRADE;
		break;
	default:
		
		phy->speed_downgraded = false;
		ret_val = 0;
		goto out;
	}

	ret_val = phy->ops.read_reg(hw, offset, &phy_data);

	if (!ret_val)
		phy->speed_downgraded = (phy_data & mask) ? true : false;

out:
	return ret_val;
}

static s32 igb_check_polarity_m88(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data;

	ret_val = phy->ops.read_reg(hw, M88E1000_PHY_SPEC_STATUS, &data);

	if (!ret_val)
		phy->cable_polarity = (data & M88E1000_PSSR_REV_POLARITY)
				      ? e1000_rev_polarity_reversed
				      : e1000_rev_polarity_normal;

	return ret_val;
}

static s32 igb_check_polarity_igp(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data, offset, mask;

	ret_val = phy->ops.read_reg(hw, IGP01E1000_PHY_PORT_STATUS, &data);
	if (ret_val)
		goto out;

	if ((data & IGP01E1000_PSSR_SPEED_MASK) ==
	    IGP01E1000_PSSR_SPEED_1000MBPS) {
		offset	= IGP01E1000_PHY_PCS_INIT_REG;
		mask	= IGP01E1000_PHY_POLARITY_MASK;
	} else {
		offset	= IGP01E1000_PHY_PORT_STATUS;
		mask	= IGP01E1000_PSSR_POLARITY_REVERSED;
	}

	ret_val = phy->ops.read_reg(hw, offset, &data);

	if (!ret_val)
		phy->cable_polarity = (data & mask)
				      ? e1000_rev_polarity_reversed
				      : e1000_rev_polarity_normal;

out:
	return ret_val;
}

static s32 igb_wait_autoneg(struct e1000_hw *hw)
{
	s32 ret_val = 0;
	u16 i, phy_status;

	
	for (i = PHY_AUTO_NEG_LIMIT; i > 0; i--) {
		ret_val = hw->phy.ops.read_reg(hw, PHY_STATUS, &phy_status);
		if (ret_val)
			break;
		ret_val = hw->phy.ops.read_reg(hw, PHY_STATUS, &phy_status);
		if (ret_val)
			break;
		if (phy_status & MII_SR_AUTONEG_COMPLETE)
			break;
		msleep(100);
	}

	return ret_val;
}

s32 igb_phy_has_link(struct e1000_hw *hw, u32 iterations,
			       u32 usec_interval, bool *success)
{
	s32 ret_val = 0;
	u16 i, phy_status;

	for (i = 0; i < iterations; i++) {
		ret_val = hw->phy.ops.read_reg(hw, PHY_STATUS, &phy_status);
		if (ret_val) {
			udelay(usec_interval);
		}
		ret_val = hw->phy.ops.read_reg(hw, PHY_STATUS, &phy_status);
		if (ret_val)
			break;
		if (phy_status & MII_SR_LINK_STATUS)
			break;
		if (usec_interval >= 1000)
			mdelay(usec_interval/1000);
		else
			udelay(usec_interval);
	}

	*success = (i < iterations) ? true : false;

	return ret_val;
}

s32 igb_get_cable_length_m88(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data, index;

	ret_val = phy->ops.read_reg(hw, M88E1000_PHY_SPEC_STATUS, &phy_data);
	if (ret_val)
		goto out;

	index = (phy_data & M88E1000_PSSR_CABLE_LENGTH) >>
		M88E1000_PSSR_CABLE_LENGTH_SHIFT;
	if (index >= M88E1000_CABLE_LENGTH_TABLE_SIZE - 1) {
		ret_val = -E1000_ERR_PHY;
		goto out;
	}

	phy->min_cable_length = e1000_m88_cable_length_table[index];
	phy->max_cable_length = e1000_m88_cable_length_table[index + 1];

	phy->cable_length = (phy->min_cable_length + phy->max_cable_length) / 2;

out:
	return ret_val;
}

s32 igb_get_cable_length_m88_gen2(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data, phy_data2, index, default_page, is_cm;

	switch (hw->phy.id) {
	case I347AT4_E_PHY_ID:
		
		ret_val = phy->ops.read_reg(hw, I347AT4_PAGE_SELECT,
					    &default_page);
		if (ret_val)
			goto out;

		ret_val = phy->ops.write_reg(hw, I347AT4_PAGE_SELECT, 0x07);
		if (ret_val)
			goto out;

		
		ret_val = phy->ops.read_reg(hw, (I347AT4_PCDL + phy->addr),
					    &phy_data);
		if (ret_val)
			goto out;

		
		ret_val = phy->ops.read_reg(hw, I347AT4_PCDC, &phy_data2);
		if (ret_val)
			goto out;

		is_cm = !(phy_data2 & I347AT4_PCDC_CABLE_LENGTH_UNIT);

		
		phy->min_cable_length = phy_data / (is_cm ? 100 : 1);
		phy->max_cable_length = phy_data / (is_cm ? 100 : 1);
		phy->cable_length = phy_data / (is_cm ? 100 : 1);

		
		ret_val = phy->ops.write_reg(hw, I347AT4_PAGE_SELECT,
					     default_page);
		if (ret_val)
			goto out;
		break;
	case M88E1112_E_PHY_ID:
		
		ret_val = phy->ops.read_reg(hw, I347AT4_PAGE_SELECT,
					    &default_page);
		if (ret_val)
			goto out;

		ret_val = phy->ops.write_reg(hw, I347AT4_PAGE_SELECT, 0x05);
		if (ret_val)
			goto out;

		ret_val = phy->ops.read_reg(hw, M88E1112_VCT_DSP_DISTANCE,
					    &phy_data);
		if (ret_val)
			goto out;

		index = (phy_data & M88E1000_PSSR_CABLE_LENGTH) >>
			M88E1000_PSSR_CABLE_LENGTH_SHIFT;
		if (index >= M88E1000_CABLE_LENGTH_TABLE_SIZE - 1) {
			ret_val = -E1000_ERR_PHY;
			goto out;
		}

		phy->min_cable_length = e1000_m88_cable_length_table[index];
		phy->max_cable_length = e1000_m88_cable_length_table[index + 1];

		phy->cable_length = (phy->min_cable_length +
				     phy->max_cable_length) / 2;

		
		ret_val = phy->ops.write_reg(hw, I347AT4_PAGE_SELECT,
					     default_page);
		if (ret_val)
			goto out;

		break;
	default:
		ret_val = -E1000_ERR_PHY;
		goto out;
	}

out:
	return ret_val;
}

s32 igb_get_cable_length_igp_2(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val = 0;
	u16 phy_data, i, agc_value = 0;
	u16 cur_agc_index, max_agc_index = 0;
	u16 min_agc_index = IGP02E1000_CABLE_LENGTH_TABLE_SIZE - 1;
	static const u16 agc_reg_array[IGP02E1000_PHY_CHANNEL_NUM] = {
	       IGP02E1000_PHY_AGC_A,
	       IGP02E1000_PHY_AGC_B,
	       IGP02E1000_PHY_AGC_C,
	       IGP02E1000_PHY_AGC_D
	};

	
	for (i = 0; i < IGP02E1000_PHY_CHANNEL_NUM; i++) {
		ret_val = phy->ops.read_reg(hw, agc_reg_array[i], &phy_data);
		if (ret_val)
			goto out;

		cur_agc_index = (phy_data >> IGP02E1000_AGC_LENGTH_SHIFT) &
				IGP02E1000_AGC_LENGTH_MASK;

		
		if ((cur_agc_index >= IGP02E1000_CABLE_LENGTH_TABLE_SIZE) ||
		    (cur_agc_index == 0)) {
			ret_val = -E1000_ERR_PHY;
			goto out;
		}

		
		if (e1000_igp_2_cable_length_table[min_agc_index] >
		    e1000_igp_2_cable_length_table[cur_agc_index])
			min_agc_index = cur_agc_index;
		if (e1000_igp_2_cable_length_table[max_agc_index] <
		    e1000_igp_2_cable_length_table[cur_agc_index])
			max_agc_index = cur_agc_index;

		agc_value += e1000_igp_2_cable_length_table[cur_agc_index];
	}

	agc_value -= (e1000_igp_2_cable_length_table[min_agc_index] +
		      e1000_igp_2_cable_length_table[max_agc_index]);
	agc_value /= (IGP02E1000_PHY_CHANNEL_NUM - 2);

	
	phy->min_cable_length = ((agc_value - IGP02E1000_AGC_RANGE) > 0) ?
				 (agc_value - IGP02E1000_AGC_RANGE) : 0;
	phy->max_cable_length = agc_value + IGP02E1000_AGC_RANGE;

	phy->cable_length = (phy->min_cable_length + phy->max_cable_length) / 2;

out:
	return ret_val;
}

s32 igb_get_phy_info_m88(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32  ret_val;
	u16 phy_data;
	bool link;

	if (phy->media_type != e1000_media_type_copper) {
		hw_dbg("Phy info is only valid for copper media\n");
		ret_val = -E1000_ERR_CONFIG;
		goto out;
	}

	ret_val = igb_phy_has_link(hw, 1, 0, &link);
	if (ret_val)
		goto out;

	if (!link) {
		hw_dbg("Phy info is only valid if link is up\n");
		ret_val = -E1000_ERR_CONFIG;
		goto out;
	}

	ret_val = phy->ops.read_reg(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		goto out;

	phy->polarity_correction = (phy_data & M88E1000_PSCR_POLARITY_REVERSAL)
				   ? true : false;

	ret_val = igb_check_polarity_m88(hw);
	if (ret_val)
		goto out;

	ret_val = phy->ops.read_reg(hw, M88E1000_PHY_SPEC_STATUS, &phy_data);
	if (ret_val)
		goto out;

	phy->is_mdix = (phy_data & M88E1000_PSSR_MDIX) ? true : false;

	if ((phy_data & M88E1000_PSSR_SPEED) == M88E1000_PSSR_1000MBS) {
		ret_val = phy->ops.get_cable_length(hw);
		if (ret_val)
			goto out;

		ret_val = phy->ops.read_reg(hw, PHY_1000T_STATUS, &phy_data);
		if (ret_val)
			goto out;

		phy->local_rx = (phy_data & SR_1000T_LOCAL_RX_STATUS)
				? e1000_1000t_rx_status_ok
				: e1000_1000t_rx_status_not_ok;

		phy->remote_rx = (phy_data & SR_1000T_REMOTE_RX_STATUS)
				 ? e1000_1000t_rx_status_ok
				 : e1000_1000t_rx_status_not_ok;
	} else {
		
		phy->cable_length = E1000_CABLE_LENGTH_UNDEFINED;
		phy->local_rx = e1000_1000t_rx_status_undefined;
		phy->remote_rx = e1000_1000t_rx_status_undefined;
	}

out:
	return ret_val;
}

s32 igb_get_phy_info_igp(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data;
	bool link;

	ret_val = igb_phy_has_link(hw, 1, 0, &link);
	if (ret_val)
		goto out;

	if (!link) {
		hw_dbg("Phy info is only valid if link is up\n");
		ret_val = -E1000_ERR_CONFIG;
		goto out;
	}

	phy->polarity_correction = true;

	ret_val = igb_check_polarity_igp(hw);
	if (ret_val)
		goto out;

	ret_val = phy->ops.read_reg(hw, IGP01E1000_PHY_PORT_STATUS, &data);
	if (ret_val)
		goto out;

	phy->is_mdix = (data & IGP01E1000_PSSR_MDIX) ? true : false;

	if ((data & IGP01E1000_PSSR_SPEED_MASK) ==
	    IGP01E1000_PSSR_SPEED_1000MBPS) {
		ret_val = phy->ops.get_cable_length(hw);
		if (ret_val)
			goto out;

		ret_val = phy->ops.read_reg(hw, PHY_1000T_STATUS, &data);
		if (ret_val)
			goto out;

		phy->local_rx = (data & SR_1000T_LOCAL_RX_STATUS)
				? e1000_1000t_rx_status_ok
				: e1000_1000t_rx_status_not_ok;

		phy->remote_rx = (data & SR_1000T_REMOTE_RX_STATUS)
				 ? e1000_1000t_rx_status_ok
				 : e1000_1000t_rx_status_not_ok;
	} else {
		phy->cable_length = E1000_CABLE_LENGTH_UNDEFINED;
		phy->local_rx = e1000_1000t_rx_status_undefined;
		phy->remote_rx = e1000_1000t_rx_status_undefined;
	}

out:
	return ret_val;
}

s32 igb_phy_sw_reset(struct e1000_hw *hw)
{
	s32 ret_val = 0;
	u16 phy_ctrl;

	if (!(hw->phy.ops.read_reg))
		goto out;

	ret_val = hw->phy.ops.read_reg(hw, PHY_CONTROL, &phy_ctrl);
	if (ret_val)
		goto out;

	phy_ctrl |= MII_CR_RESET;
	ret_val = hw->phy.ops.write_reg(hw, PHY_CONTROL, phy_ctrl);
	if (ret_val)
		goto out;

	udelay(1);

out:
	return ret_val;
}

s32 igb_phy_hw_reset(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32  ret_val;
	u32 ctrl;

	ret_val = igb_check_reset_block(hw);
	if (ret_val) {
		ret_val = 0;
		goto out;
	}

	ret_val = phy->ops.acquire(hw);
	if (ret_val)
		goto out;

	ctrl = rd32(E1000_CTRL);
	wr32(E1000_CTRL, ctrl | E1000_CTRL_PHY_RST);
	wrfl();

	udelay(phy->reset_delay_us);

	wr32(E1000_CTRL, ctrl);
	wrfl();

	udelay(150);

	phy->ops.release(hw);

	ret_val = phy->ops.get_cfg_done(hw);

out:
	return ret_val;
}

s32 igb_phy_init_script_igp3(struct e1000_hw *hw)
{
	hw_dbg("Running IGP 3 PHY init script\n");

	
	
	hw->phy.ops.write_reg(hw, 0x2F5B, 0x9018);
	
	hw->phy.ops.write_reg(hw, 0x2F52, 0x0000);
	
	hw->phy.ops.write_reg(hw, 0x2FB1, 0x8B24);
	
	hw->phy.ops.write_reg(hw, 0x2FB2, 0xF8F0);
	
	hw->phy.ops.write_reg(hw, 0x2010, 0x10B0);
	
	hw->phy.ops.write_reg(hw, 0x2011, 0x0000);
	
	hw->phy.ops.write_reg(hw, 0x20DD, 0x249A);
	
	hw->phy.ops.write_reg(hw, 0x20DE, 0x00D3);
	
	hw->phy.ops.write_reg(hw, 0x28B4, 0x04CE);
	
	hw->phy.ops.write_reg(hw, 0x2F70, 0x29E4);
	
	hw->phy.ops.write_reg(hw, 0x0000, 0x0140);
	
	hw->phy.ops.write_reg(hw, 0x1F30, 0x1606);
	
	hw->phy.ops.write_reg(hw, 0x1F31, 0xB814);
	
	hw->phy.ops.write_reg(hw, 0x1F35, 0x002A);
	
	hw->phy.ops.write_reg(hw, 0x1F3E, 0x0067);
	
	hw->phy.ops.write_reg(hw, 0x1F54, 0x0065);
	
	hw->phy.ops.write_reg(hw, 0x1F55, 0x002A);
	
	hw->phy.ops.write_reg(hw, 0x1F56, 0x002A);
	
	hw->phy.ops.write_reg(hw, 0x1F72, 0x3FB0);
	
	hw->phy.ops.write_reg(hw, 0x1F76, 0xC0FF);
	
	hw->phy.ops.write_reg(hw, 0x1F77, 0x1DEC);
	
	hw->phy.ops.write_reg(hw, 0x1F78, 0xF9EF);
	
	hw->phy.ops.write_reg(hw, 0x1F79, 0x0210);
	
	hw->phy.ops.write_reg(hw, 0x1895, 0x0003);
	
	hw->phy.ops.write_reg(hw, 0x1796, 0x0008);
	
	hw->phy.ops.write_reg(hw, 0x1798, 0xD008);
	hw->phy.ops.write_reg(hw, 0x1898, 0xD918);
	
	hw->phy.ops.write_reg(hw, 0x187A, 0x0800);
	hw->phy.ops.write_reg(hw, 0x0019, 0x008D);
	
	hw->phy.ops.write_reg(hw, 0x001B, 0x2080);
	
	hw->phy.ops.write_reg(hw, 0x0014, 0x0045);
	
	hw->phy.ops.write_reg(hw, 0x0000, 0x1340);

	return 0;
}

void igb_power_up_phy_copper(struct e1000_hw *hw)
{
	u16 mii_reg = 0;

	
	hw->phy.ops.read_reg(hw, PHY_CONTROL, &mii_reg);
	mii_reg &= ~MII_CR_POWER_DOWN;
	hw->phy.ops.write_reg(hw, PHY_CONTROL, mii_reg);
}

void igb_power_down_phy_copper(struct e1000_hw *hw)
{
	u16 mii_reg = 0;

	
	hw->phy.ops.read_reg(hw, PHY_CONTROL, &mii_reg);
	mii_reg |= MII_CR_POWER_DOWN;
	hw->phy.ops.write_reg(hw, PHY_CONTROL, mii_reg);
	msleep(1);
}

static s32 igb_check_polarity_82580(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data;


	ret_val = phy->ops.read_reg(hw, I82580_PHY_STATUS_2, &data);

	if (!ret_val)
		phy->cable_polarity = (data & I82580_PHY_STATUS2_REV_POLARITY)
		                      ? e1000_rev_polarity_reversed
		                      : e1000_rev_polarity_normal;

	return ret_val;
}

s32 igb_phy_force_speed_duplex_82580(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data;
	bool link;


	ret_val = phy->ops.read_reg(hw, PHY_CONTROL, &phy_data);
	if (ret_val)
		goto out;

	igb_phy_force_speed_duplex_setup(hw, &phy_data);

	ret_val = phy->ops.write_reg(hw, PHY_CONTROL, phy_data);
	if (ret_val)
		goto out;

	ret_val = phy->ops.read_reg(hw, I82580_PHY_CTRL_2, &phy_data);
	if (ret_val)
		goto out;

	phy_data &= ~I82580_PHY_CTRL2_AUTO_MDIX;
	phy_data &= ~I82580_PHY_CTRL2_FORCE_MDI_MDIX;

	ret_val = phy->ops.write_reg(hw, I82580_PHY_CTRL_2, phy_data);
	if (ret_val)
		goto out;

	hw_dbg("I82580_PHY_CTRL_2: %X\n", phy_data);

	udelay(1);

	if (phy->autoneg_wait_to_complete) {
		hw_dbg("Waiting for forced speed/duplex link on 82580 phy\n");

		ret_val = igb_phy_has_link(hw,
		                           PHY_FORCE_LIMIT,
		                           100000,
		                           &link);
		if (ret_val)
			goto out;

		if (!link)
			hw_dbg("Link taking longer than expected.\n");

		
		ret_val = igb_phy_has_link(hw,
		                           PHY_FORCE_LIMIT,
		                           100000,
		                           &link);
		if (ret_val)
			goto out;
	}

out:
	return ret_val;
}

s32 igb_get_phy_info_82580(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data;
	bool link;


	ret_val = igb_phy_has_link(hw, 1, 0, &link);
	if (ret_val)
		goto out;

	if (!link) {
		hw_dbg("Phy info is only valid if link is up\n");
		ret_val = -E1000_ERR_CONFIG;
		goto out;
	}

	phy->polarity_correction = true;

	ret_val = igb_check_polarity_82580(hw);
	if (ret_val)
		goto out;

	ret_val = phy->ops.read_reg(hw, I82580_PHY_STATUS_2, &data);
	if (ret_val)
		goto out;

	phy->is_mdix = (data & I82580_PHY_STATUS2_MDIX) ? true : false;

	if ((data & I82580_PHY_STATUS2_SPEED_MASK) ==
	    I82580_PHY_STATUS2_SPEED_1000MBPS) {
		ret_val = hw->phy.ops.get_cable_length(hw);
		if (ret_val)
			goto out;

		ret_val = phy->ops.read_reg(hw, PHY_1000T_STATUS, &data);
		if (ret_val)
			goto out;

		phy->local_rx = (data & SR_1000T_LOCAL_RX_STATUS)
		                ? e1000_1000t_rx_status_ok
		                : e1000_1000t_rx_status_not_ok;

		phy->remote_rx = (data & SR_1000T_REMOTE_RX_STATUS)
		                 ? e1000_1000t_rx_status_ok
		                 : e1000_1000t_rx_status_not_ok;
	} else {
		phy->cable_length = E1000_CABLE_LENGTH_UNDEFINED;
		phy->local_rx = e1000_1000t_rx_status_undefined;
		phy->remote_rx = e1000_1000t_rx_status_undefined;
	}

out:
	return ret_val;
}

s32 igb_get_cable_length_82580(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data, length;


	ret_val = phy->ops.read_reg(hw, I82580_PHY_DIAG_STATUS, &phy_data);
	if (ret_val)
		goto out;

	length = (phy_data & I82580_DSTATUS_CABLE_LENGTH) >>
	         I82580_DSTATUS_CABLE_LENGTH_SHIFT;

	if (length == E1000_CABLE_LENGTH_UNDEFINED)
		ret_val = -E1000_ERR_PHY;

	phy->cable_length = length;

out:
	return ret_val;
}
