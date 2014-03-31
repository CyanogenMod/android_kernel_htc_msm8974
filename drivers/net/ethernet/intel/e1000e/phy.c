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

static s32 e1000_get_phy_cfg_done(struct e1000_hw *hw);
static s32 e1000_phy_force_speed_duplex(struct e1000_hw *hw);
static s32 e1000_set_d0_lplu_state(struct e1000_hw *hw, bool active);
static s32 e1000_wait_autoneg(struct e1000_hw *hw);
static u32 e1000_get_phy_addr_for_bm_page(u32 page, u32 reg);
static s32 e1000_access_phy_wakeup_reg_bm(struct e1000_hw *hw, u32 offset,
					  u16 *data, bool read, bool page_set);
static u32 e1000_get_phy_addr_for_hv_page(u32 page);
static s32 e1000_access_phy_debug_regs_hv(struct e1000_hw *hw, u32 offset,
                                          u16 *data, bool read);

static const u16 e1000_m88_cable_length_table[] = {
	0, 50, 80, 110, 140, 140, E1000_CABLE_LENGTH_UNDEFINED };
#define M88E1000_CABLE_LENGTH_TABLE_SIZE \
		ARRAY_SIZE(e1000_m88_cable_length_table)

static const u16 e1000_igp_2_cable_length_table[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 3, 5, 8, 11, 13, 16, 18, 21, 0, 0, 0, 3,
	6, 10, 13, 16, 19, 23, 26, 29, 32, 35, 38, 41, 6, 10, 14, 18, 22,
	26, 30, 33, 37, 41, 44, 48, 51, 54, 58, 61, 21, 26, 31, 35, 40,
	44, 49, 53, 57, 61, 65, 68, 72, 75, 79, 82, 40, 45, 51, 56, 61,
	66, 70, 75, 79, 83, 87, 91, 94, 98, 101, 104, 60, 66, 72, 77, 82,
	87, 92, 96, 100, 104, 108, 111, 114, 117, 119, 121, 83, 89, 95,
	100, 105, 109, 113, 116, 119, 122, 124, 104, 109, 114, 118, 121,
	124};
#define IGP02E1000_CABLE_LENGTH_TABLE_SIZE \
		ARRAY_SIZE(e1000_igp_2_cable_length_table)

#define BM_PHY_REG_PAGE(offset) \
	((u16)(((offset) >> PHY_PAGE_SHIFT) & 0xFFFF))
#define BM_PHY_REG_NUM(offset) \
	((u16)(((offset) & MAX_PHY_REG_ADDRESS) |\
	 (((offset) >> (PHY_UPPER_SHIFT - PHY_PAGE_SHIFT)) &\
		~MAX_PHY_REG_ADDRESS)))

#define HV_INTC_FC_PAGE_START             768
#define I82578_ADDR_REG                   29
#define I82577_ADDR_REG                   16
#define I82577_CFG_REG                    22
#define I82577_CFG_ASSERT_CRS_ON_TX       (1 << 15)
#define I82577_CFG_ENABLE_DOWNSHIFT       (3 << 10) 
#define I82577_CTRL_REG                   23

#define I82577_PHY_CTRL_2            18
#define I82577_PHY_STATUS_2          26
#define I82577_PHY_DIAG_STATUS       31

#define I82577_PHY_STATUS2_REV_POLARITY   0x0400
#define I82577_PHY_STATUS2_MDIX           0x0800
#define I82577_PHY_STATUS2_SPEED_MASK     0x0300
#define I82577_PHY_STATUS2_SPEED_1000MBPS 0x0200

#define I82577_PHY_CTRL2_AUTO_MDIX        0x0400
#define I82577_PHY_CTRL2_FORCE_MDI_MDIX   0x0200

#define I82577_DSTATUS_CABLE_LENGTH       0x03FC
#define I82577_DSTATUS_CABLE_LENGTH_SHIFT 2

#define BM_CS_CTRL1                       16

#define HV_MUX_DATA_CTRL               PHY_REG(776, 16)
#define HV_MUX_DATA_CTRL_GEN_TO_MAC    0x0400
#define HV_MUX_DATA_CTRL_FORCE_SPEED   0x0004

s32 e1000e_check_reset_block_generic(struct e1000_hw *hw)
{
	u32 manc;

	manc = er32(MANC);

	return (manc & E1000_MANC_BLK_PHY_RST_ON_IDE) ?
	       E1000_BLK_PHY_RESET : 0;
}

s32 e1000e_get_phy_id(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val = 0;
	u16 phy_id;
	u16 retry_count = 0;

	if (!phy->ops.read_reg)
		return 0;

	while (retry_count < 2) {
		ret_val = e1e_rphy(hw, PHY_ID1, &phy_id);
		if (ret_val)
			return ret_val;

		phy->id = (u32)(phy_id << 16);
		udelay(20);
		ret_val = e1e_rphy(hw, PHY_ID2, &phy_id);
		if (ret_val)
			return ret_val;

		phy->id |= (u32)(phy_id & PHY_REVISION_MASK);
		phy->revision = (u32)(phy_id & ~PHY_REVISION_MASK);

		if (phy->id != 0 && phy->id != PHY_REVISION_MASK)
			return 0;

		retry_count++;
	}

	return 0;
}

s32 e1000e_phy_reset_dsp(struct e1000_hw *hw)
{
	s32 ret_val;

	ret_val = e1e_wphy(hw, M88E1000_PHY_GEN_CONTROL, 0xC1);
	if (ret_val)
		return ret_val;

	return e1e_wphy(hw, M88E1000_PHY_GEN_CONTROL, 0);
}

s32 e1000e_read_phy_reg_mdic(struct e1000_hw *hw, u32 offset, u16 *data)
{
	struct e1000_phy_info *phy = &hw->phy;
	u32 i, mdic = 0;

	if (offset > MAX_PHY_REG_ADDRESS) {
		e_dbg("PHY Address %d is out of range\n", offset);
		return -E1000_ERR_PARAM;
	}

	mdic = ((offset << E1000_MDIC_REG_SHIFT) |
		(phy->addr << E1000_MDIC_PHY_SHIFT) |
		(E1000_MDIC_OP_READ));

	ew32(MDIC, mdic);

	for (i = 0; i < (E1000_GEN_POLL_TIMEOUT * 3); i++) {
		udelay(50);
		mdic = er32(MDIC);
		if (mdic & E1000_MDIC_READY)
			break;
	}
	if (!(mdic & E1000_MDIC_READY)) {
		e_dbg("MDI Read did not complete\n");
		return -E1000_ERR_PHY;
	}
	if (mdic & E1000_MDIC_ERROR) {
		e_dbg("MDI Error\n");
		return -E1000_ERR_PHY;
	}
	*data = (u16) mdic;

	if (hw->mac.type == e1000_pch2lan)
		udelay(100);

	return 0;
}

s32 e1000e_write_phy_reg_mdic(struct e1000_hw *hw, u32 offset, u16 data)
{
	struct e1000_phy_info *phy = &hw->phy;
	u32 i, mdic = 0;

	if (offset > MAX_PHY_REG_ADDRESS) {
		e_dbg("PHY Address %d is out of range\n", offset);
		return -E1000_ERR_PARAM;
	}

	mdic = (((u32)data) |
		(offset << E1000_MDIC_REG_SHIFT) |
		(phy->addr << E1000_MDIC_PHY_SHIFT) |
		(E1000_MDIC_OP_WRITE));

	ew32(MDIC, mdic);

	for (i = 0; i < (E1000_GEN_POLL_TIMEOUT * 3); i++) {
		udelay(50);
		mdic = er32(MDIC);
		if (mdic & E1000_MDIC_READY)
			break;
	}
	if (!(mdic & E1000_MDIC_READY)) {
		e_dbg("MDI Write did not complete\n");
		return -E1000_ERR_PHY;
	}
	if (mdic & E1000_MDIC_ERROR) {
		e_dbg("MDI Error\n");
		return -E1000_ERR_PHY;
	}

	if (hw->mac.type == e1000_pch2lan)
		udelay(100);

	return 0;
}

s32 e1000e_read_phy_reg_m88(struct e1000_hw *hw, u32 offset, u16 *data)
{
	s32 ret_val;

	ret_val = hw->phy.ops.acquire(hw);
	if (ret_val)
		return ret_val;

	ret_val = e1000e_read_phy_reg_mdic(hw, MAX_PHY_REG_ADDRESS & offset,
					   data);

	hw->phy.ops.release(hw);

	return ret_val;
}

s32 e1000e_write_phy_reg_m88(struct e1000_hw *hw, u32 offset, u16 data)
{
	s32 ret_val;

	ret_val = hw->phy.ops.acquire(hw);
	if (ret_val)
		return ret_val;

	ret_val = e1000e_write_phy_reg_mdic(hw, MAX_PHY_REG_ADDRESS & offset,
					    data);

	hw->phy.ops.release(hw);

	return ret_val;
}

s32 e1000_set_page_igp(struct e1000_hw *hw, u16 page)
{
	e_dbg("Setting page 0x%x\n", page);

	hw->phy.addr = 1;

	return e1000e_write_phy_reg_mdic(hw, IGP01E1000_PHY_PAGE_SELECT, page);
}

static s32 __e1000e_read_phy_reg_igp(struct e1000_hw *hw, u32 offset, u16 *data,
                                    bool locked)
{
	s32 ret_val = 0;

	if (!locked) {
		if (!hw->phy.ops.acquire)
			return 0;

		ret_val = hw->phy.ops.acquire(hw);
		if (ret_val)
			return ret_val;
	}

	if (offset > MAX_PHY_MULTI_PAGE_REG)
		ret_val = e1000e_write_phy_reg_mdic(hw,
						    IGP01E1000_PHY_PAGE_SELECT,
						    (u16)offset);
	if (!ret_val)
		ret_val = e1000e_read_phy_reg_mdic(hw,
						   MAX_PHY_REG_ADDRESS & offset,
						   data);
	if (!locked)
		hw->phy.ops.release(hw);

	return ret_val;
}

s32 e1000e_read_phy_reg_igp(struct e1000_hw *hw, u32 offset, u16 *data)
{
	return __e1000e_read_phy_reg_igp(hw, offset, data, false);
}

s32 e1000e_read_phy_reg_igp_locked(struct e1000_hw *hw, u32 offset, u16 *data)
{
	return __e1000e_read_phy_reg_igp(hw, offset, data, true);
}

static s32 __e1000e_write_phy_reg_igp(struct e1000_hw *hw, u32 offset, u16 data,
                                     bool locked)
{
	s32 ret_val = 0;

	if (!locked) {
		if (!hw->phy.ops.acquire)
			return 0;

		ret_val = hw->phy.ops.acquire(hw);
		if (ret_val)
			return ret_val;
	}

	if (offset > MAX_PHY_MULTI_PAGE_REG)
		ret_val = e1000e_write_phy_reg_mdic(hw,
						    IGP01E1000_PHY_PAGE_SELECT,
						    (u16)offset);
	if (!ret_val)
		ret_val = e1000e_write_phy_reg_mdic(hw, MAX_PHY_REG_ADDRESS &
							offset,
						    data);
	if (!locked)
		hw->phy.ops.release(hw);

	return ret_val;
}

s32 e1000e_write_phy_reg_igp(struct e1000_hw *hw, u32 offset, u16 data)
{
	return __e1000e_write_phy_reg_igp(hw, offset, data, false);
}

s32 e1000e_write_phy_reg_igp_locked(struct e1000_hw *hw, u32 offset, u16 data)
{
	return __e1000e_write_phy_reg_igp(hw, offset, data, true);
}

static s32 __e1000_read_kmrn_reg(struct e1000_hw *hw, u32 offset, u16 *data,
                                 bool locked)
{
	u32 kmrnctrlsta;

	if (!locked) {
		s32 ret_val = 0;

		if (!hw->phy.ops.acquire)
			return 0;

		ret_val = hw->phy.ops.acquire(hw);
		if (ret_val)
			return ret_val;
	}

	kmrnctrlsta = ((offset << E1000_KMRNCTRLSTA_OFFSET_SHIFT) &
		       E1000_KMRNCTRLSTA_OFFSET) | E1000_KMRNCTRLSTA_REN;
	ew32(KMRNCTRLSTA, kmrnctrlsta);
	e1e_flush();

	udelay(2);

	kmrnctrlsta = er32(KMRNCTRLSTA);
	*data = (u16)kmrnctrlsta;

	if (!locked)
		hw->phy.ops.release(hw);

	return 0;
}

s32 e1000e_read_kmrn_reg(struct e1000_hw *hw, u32 offset, u16 *data)
{
	return __e1000_read_kmrn_reg(hw, offset, data, false);
}

s32 e1000e_read_kmrn_reg_locked(struct e1000_hw *hw, u32 offset, u16 *data)
{
	return __e1000_read_kmrn_reg(hw, offset, data, true);
}

static s32 __e1000_write_kmrn_reg(struct e1000_hw *hw, u32 offset, u16 data,
                                  bool locked)
{
	u32 kmrnctrlsta;

	if (!locked) {
		s32 ret_val = 0;

		if (!hw->phy.ops.acquire)
			return 0;

		ret_val = hw->phy.ops.acquire(hw);
		if (ret_val)
			return ret_val;
	}

	kmrnctrlsta = ((offset << E1000_KMRNCTRLSTA_OFFSET_SHIFT) &
		       E1000_KMRNCTRLSTA_OFFSET) | data;
	ew32(KMRNCTRLSTA, kmrnctrlsta);
	e1e_flush();

	udelay(2);

	if (!locked)
		hw->phy.ops.release(hw);

	return 0;
}

s32 e1000e_write_kmrn_reg(struct e1000_hw *hw, u32 offset, u16 data)
{
	return __e1000_write_kmrn_reg(hw, offset, data, false);
}

s32 e1000e_write_kmrn_reg_locked(struct e1000_hw *hw, u32 offset, u16 data)
{
	return __e1000_write_kmrn_reg(hw, offset, data, true);
}

s32 e1000_copper_link_setup_82577(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 phy_data;

	
	ret_val = e1e_rphy(hw, I82577_CFG_REG, &phy_data);
	if (ret_val)
		return ret_val;

	phy_data |= I82577_CFG_ASSERT_CRS_ON_TX;

	
	phy_data |= I82577_CFG_ENABLE_DOWNSHIFT;

	return e1e_wphy(hw, I82577_CFG_REG, phy_data);
}

s32 e1000e_copper_link_setup_m88(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data;

	
	ret_val = e1e_rphy(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	
	if (phy->type != e1000_phy_bm)
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

	
	if (phy->type == e1000_phy_bm)
		phy_data |= BME1000_PSCR_ENABLE_DOWNSHIFT;

	ret_val = e1e_wphy(hw, M88E1000_PHY_SPEC_CTRL, phy_data);
	if (ret_val)
		return ret_val;

	if ((phy->type == e1000_phy_m88) &&
	    (phy->revision < E1000_REVISION_4) &&
	    (phy->id != BME1000_E_PHY_ID_R2)) {
		ret_val = e1e_rphy(hw, M88E1000_EXT_PHY_SPEC_CTRL, &phy_data);
		if (ret_val)
			return ret_val;

		phy_data |= M88E1000_EPSCR_TX_CLK_25;

		if ((phy->revision == 2) &&
		    (phy->id == M88E1111_I_PHY_ID)) {
			
			phy_data &= ~M88EC018_EPSCR_DOWNSHIFT_COUNTER_MASK;
			phy_data |= M88EC018_EPSCR_DOWNSHIFT_COUNTER_5X;
		} else {
			
			phy_data &= ~(M88E1000_EPSCR_MASTER_DOWNSHIFT_MASK |
				      M88E1000_EPSCR_SLAVE_DOWNSHIFT_MASK);
			phy_data |= (M88E1000_EPSCR_MASTER_DOWNSHIFT_1X |
				     M88E1000_EPSCR_SLAVE_DOWNSHIFT_1X);
		}
		ret_val = e1e_wphy(hw, M88E1000_EXT_PHY_SPEC_CTRL, phy_data);
		if (ret_val)
			return ret_val;
	}

	if ((phy->type == e1000_phy_bm) && (phy->id == BME1000_E_PHY_ID_R2)) {
		
		ret_val = e1e_wphy(hw, 29, 0x0003);
		if (ret_val)
			return ret_val;

		
		ret_val = e1e_wphy(hw, 30, 0x0000);
		if (ret_val)
			return ret_val;
	}

	
	ret_val = e1000e_commit_phy(hw);
	if (ret_val) {
		e_dbg("Error committing the PHY changes\n");
		return ret_val;
	}

	if (phy->type == e1000_phy_82578) {
		ret_val = e1e_rphy(hw, M88E1000_EXT_PHY_SPEC_CTRL, &phy_data);
		if (ret_val)
			return ret_val;

		
		phy_data |= I82578_EPSCR_DOWNSHIFT_ENABLE;
		phy_data &= ~I82578_EPSCR_DOWNSHIFT_COUNTER_MASK;
		ret_val = e1e_wphy(hw, M88E1000_EXT_PHY_SPEC_CTRL, phy_data);
		if (ret_val)
			return ret_val;
	}

	return 0;
}

s32 e1000e_copper_link_setup_igp(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data;

	ret_val = e1000_phy_hw_reset(hw);
	if (ret_val) {
		e_dbg("Error resetting the PHY.\n");
		return ret_val;
	}

	msleep(100);

	
	ret_val = e1000_set_d0_lplu_state(hw, false);
	if (ret_val) {
		e_dbg("Error Disabling LPLU D0\n");
		return ret_val;
	}
	
	ret_val = e1e_rphy(hw, IGP01E1000_PHY_PORT_CTRL, &data);
	if (ret_val)
		return ret_val;

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
	ret_val = e1e_wphy(hw, IGP01E1000_PHY_PORT_CTRL, data);
	if (ret_val)
		return ret_val;

	
	if (hw->mac.autoneg) {
		if (phy->autoneg_advertised == ADVERTISE_1000_FULL) {
			
			ret_val = e1e_rphy(hw, IGP01E1000_PHY_PORT_CONFIG,
					   &data);
			if (ret_val)
				return ret_val;

			data &= ~IGP01E1000_PSCFR_SMART_SPEED;
			ret_val = e1e_wphy(hw, IGP01E1000_PHY_PORT_CONFIG,
					   data);
			if (ret_val)
				return ret_val;

			
			ret_val = e1e_rphy(hw, PHY_1000T_CTRL, &data);
			if (ret_val)
				return ret_val;

			data &= ~CR_1000T_MS_ENABLE;
			ret_val = e1e_wphy(hw, PHY_1000T_CTRL, data);
			if (ret_val)
				return ret_val;
		}

		ret_val = e1e_rphy(hw, PHY_1000T_CTRL, &data);
		if (ret_val)
			return ret_val;

		
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
		ret_val = e1e_wphy(hw, PHY_1000T_CTRL, data);
	}

	return ret_val;
}

static s32 e1000_phy_setup_autoneg(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 mii_autoneg_adv_reg;
	u16 mii_1000t_ctrl_reg = 0;

	phy->autoneg_advertised &= phy->autoneg_mask;

	
	ret_val = e1e_rphy(hw, PHY_AUTONEG_ADV, &mii_autoneg_adv_reg);
	if (ret_val)
		return ret_val;

	if (phy->autoneg_mask & ADVERTISE_1000_FULL) {
		
		ret_val = e1e_rphy(hw, PHY_1000T_CTRL, &mii_1000t_ctrl_reg);
		if (ret_val)
			return ret_val;
	}


	mii_autoneg_adv_reg &= ~(NWAY_AR_100TX_FD_CAPS |
				 NWAY_AR_100TX_HD_CAPS |
				 NWAY_AR_10T_FD_CAPS   |
				 NWAY_AR_10T_HD_CAPS);
	mii_1000t_ctrl_reg &= ~(CR_1000T_HD_CAPS | CR_1000T_FD_CAPS);

	e_dbg("autoneg_advertised %x\n", phy->autoneg_advertised);

	
	if (phy->autoneg_advertised & ADVERTISE_10_HALF) {
		e_dbg("Advertise 10mb Half duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_10T_HD_CAPS;
	}

	
	if (phy->autoneg_advertised & ADVERTISE_10_FULL) {
		e_dbg("Advertise 10mb Full duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_10T_FD_CAPS;
	}

	
	if (phy->autoneg_advertised & ADVERTISE_100_HALF) {
		e_dbg("Advertise 100mb Half duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_100TX_HD_CAPS;
	}

	
	if (phy->autoneg_advertised & ADVERTISE_100_FULL) {
		e_dbg("Advertise 100mb Full duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_100TX_FD_CAPS;
	}

	
	if (phy->autoneg_advertised & ADVERTISE_1000_HALF)
		e_dbg("Advertise 1000mb Half duplex request denied!\n");

	
	if (phy->autoneg_advertised & ADVERTISE_1000_FULL) {
		e_dbg("Advertise 1000mb Full duplex\n");
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
		e_dbg("Flow control param set incorrectly\n");
		return -E1000_ERR_CONFIG;
	}

	ret_val = e1e_wphy(hw, PHY_AUTONEG_ADV, mii_autoneg_adv_reg);
	if (ret_val)
		return ret_val;

	e_dbg("Auto-Neg Advertising %x\n", mii_autoneg_adv_reg);

	if (phy->autoneg_mask & ADVERTISE_1000_FULL)
		ret_val = e1e_wphy(hw, PHY_1000T_CTRL, mii_1000t_ctrl_reg);

	return ret_val;
}

static s32 e1000_copper_link_autoneg(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_ctrl;

	phy->autoneg_advertised &= phy->autoneg_mask;

	if (phy->autoneg_advertised == 0)
		phy->autoneg_advertised = phy->autoneg_mask;

	e_dbg("Reconfiguring auto-neg advertisement params\n");
	ret_val = e1000_phy_setup_autoneg(hw);
	if (ret_val) {
		e_dbg("Error Setting up Auto-Negotiation\n");
		return ret_val;
	}
	e_dbg("Restarting Auto-Neg\n");

	ret_val = e1e_rphy(hw, PHY_CONTROL, &phy_ctrl);
	if (ret_val)
		return ret_val;

	phy_ctrl |= (MII_CR_AUTO_NEG_EN | MII_CR_RESTART_AUTO_NEG);
	ret_val = e1e_wphy(hw, PHY_CONTROL, phy_ctrl);
	if (ret_val)
		return ret_val;

	if (phy->autoneg_wait_to_complete) {
		ret_val = e1000_wait_autoneg(hw);
		if (ret_val) {
			e_dbg("Error while waiting for autoneg to complete\n");
			return ret_val;
		}
	}

	hw->mac.get_link_status = true;

	return ret_val;
}

s32 e1000e_setup_copper_link(struct e1000_hw *hw)
{
	s32 ret_val;
	bool link;

	if (hw->mac.autoneg) {
		ret_val = e1000_copper_link_autoneg(hw);
		if (ret_val)
			return ret_val;
	} else {
		e_dbg("Forcing Speed and Duplex\n");
		ret_val = e1000_phy_force_speed_duplex(hw);
		if (ret_val) {
			e_dbg("Error Forcing Speed and Duplex\n");
			return ret_val;
		}
	}

	ret_val = e1000e_phy_has_link_generic(hw, COPPER_LINK_UP_LIMIT, 10,
					      &link);
	if (ret_val)
		return ret_val;

	if (link) {
		e_dbg("Valid link established!!!\n");
		hw->mac.ops.config_collision_dist(hw);
		ret_val = e1000e_config_fc_after_link_up(hw);
	} else {
		e_dbg("Unable to establish link!!!\n");
	}

	return ret_val;
}

s32 e1000e_phy_force_speed_duplex_igp(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data;
	bool link;

	ret_val = e1e_rphy(hw, PHY_CONTROL, &phy_data);
	if (ret_val)
		return ret_val;

	e1000e_phy_force_speed_duplex_setup(hw, &phy_data);

	ret_val = e1e_wphy(hw, PHY_CONTROL, phy_data);
	if (ret_val)
		return ret_val;

	ret_val = e1e_rphy(hw, IGP01E1000_PHY_PORT_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	phy_data &= ~IGP01E1000_PSCR_AUTO_MDIX;
	phy_data &= ~IGP01E1000_PSCR_FORCE_MDI_MDIX;

	ret_val = e1e_wphy(hw, IGP01E1000_PHY_PORT_CTRL, phy_data);
	if (ret_val)
		return ret_val;

	e_dbg("IGP PSCR: %X\n", phy_data);

	udelay(1);

	if (phy->autoneg_wait_to_complete) {
		e_dbg("Waiting for forced speed/duplex link on IGP phy.\n");

		ret_val = e1000e_phy_has_link_generic(hw, PHY_FORCE_LIMIT,
						      100000, &link);
		if (ret_val)
			return ret_val;

		if (!link)
			e_dbg("Link taking longer than expected.\n");

		
		ret_val = e1000e_phy_has_link_generic(hw, PHY_FORCE_LIMIT,
						      100000, &link);
	}

	return ret_val;
}

s32 e1000e_phy_force_speed_duplex_m88(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data;
	bool link;

	ret_val = e1e_rphy(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	phy_data &= ~M88E1000_PSCR_AUTO_X_MODE;
	ret_val = e1e_wphy(hw, M88E1000_PHY_SPEC_CTRL, phy_data);
	if (ret_val)
		return ret_val;

	e_dbg("M88E1000 PSCR: %X\n", phy_data);

	ret_val = e1e_rphy(hw, PHY_CONTROL, &phy_data);
	if (ret_val)
		return ret_val;

	e1000e_phy_force_speed_duplex_setup(hw, &phy_data);

	ret_val = e1e_wphy(hw, PHY_CONTROL, phy_data);
	if (ret_val)
		return ret_val;

	
	ret_val = e1000e_commit_phy(hw);
	if (ret_val)
		return ret_val;

	if (phy->autoneg_wait_to_complete) {
		e_dbg("Waiting for forced speed/duplex link on M88 phy.\n");

		ret_val = e1000e_phy_has_link_generic(hw, PHY_FORCE_LIMIT,
						     100000, &link);
		if (ret_val)
			return ret_val;

		if (!link) {
			if (hw->phy.type != e1000_phy_m88) {
				e_dbg("Link taking longer than expected.\n");
			} else {
				ret_val = e1e_wphy(hw, M88E1000_PHY_PAGE_SELECT,
						   0x001d);
				if (ret_val)
					return ret_val;
				ret_val = e1000e_phy_reset_dsp(hw);
				if (ret_val)
					return ret_val;
			}
		}

		
		ret_val = e1000e_phy_has_link_generic(hw, PHY_FORCE_LIMIT,
						     100000, &link);
		if (ret_val)
			return ret_val;
	}

	if (hw->phy.type != e1000_phy_m88)
		return 0;

	ret_val = e1e_rphy(hw, M88E1000_EXT_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	phy_data |= M88E1000_EPSCR_TX_CLK_25;
	ret_val = e1e_wphy(hw, M88E1000_EXT_PHY_SPEC_CTRL, phy_data);
	if (ret_val)
		return ret_val;

	ret_val = e1e_rphy(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	phy_data |= M88E1000_PSCR_ASSERT_CRS_ON_TX;
	ret_val = e1e_wphy(hw, M88E1000_PHY_SPEC_CTRL, phy_data);

	return ret_val;
}

s32 e1000_phy_force_speed_duplex_ife(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data;
	bool link;

	ret_val = e1e_rphy(hw, PHY_CONTROL, &data);
	if (ret_val)
		return ret_val;

	e1000e_phy_force_speed_duplex_setup(hw, &data);

	ret_val = e1e_wphy(hw, PHY_CONTROL, data);
	if (ret_val)
		return ret_val;

	
	ret_val = e1e_rphy(hw, IFE_PHY_MDIX_CONTROL, &data);
	if (ret_val)
		return ret_val;

	data &= ~IFE_PMC_AUTO_MDIX;
	data &= ~IFE_PMC_FORCE_MDIX;

	ret_val = e1e_wphy(hw, IFE_PHY_MDIX_CONTROL, data);
	if (ret_val)
		return ret_val;

	e_dbg("IFE PMC: %X\n", data);

	udelay(1);

	if (phy->autoneg_wait_to_complete) {
		e_dbg("Waiting for forced speed/duplex link on IFE phy.\n");

		ret_val = e1000e_phy_has_link_generic(hw, PHY_FORCE_LIMIT,
						      100000, &link);
		if (ret_val)
			return ret_val;

		if (!link)
			e_dbg("Link taking longer than expected.\n");

		
		ret_val = e1000e_phy_has_link_generic(hw, PHY_FORCE_LIMIT,
						      100000, &link);
		if (ret_val)
			return ret_val;
	}

	return 0;
}

void e1000e_phy_force_speed_duplex_setup(struct e1000_hw *hw, u16 *phy_ctrl)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 ctrl;

	
	hw->fc.current_mode = e1000_fc_none;

	
	ctrl = er32(CTRL);
	ctrl |= (E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
	ctrl &= ~E1000_CTRL_SPD_SEL;

	
	ctrl &= ~E1000_CTRL_ASDE;

	
	*phy_ctrl &= ~MII_CR_AUTO_NEG_EN;

	
	if (mac->forced_speed_duplex & E1000_ALL_HALF_DUPLEX) {
		ctrl &= ~E1000_CTRL_FD;
		*phy_ctrl &= ~MII_CR_FULL_DUPLEX;
		e_dbg("Half Duplex\n");
	} else {
		ctrl |= E1000_CTRL_FD;
		*phy_ctrl |= MII_CR_FULL_DUPLEX;
		e_dbg("Full Duplex\n");
	}

	
	if (mac->forced_speed_duplex & E1000_ALL_100_SPEED) {
		ctrl |= E1000_CTRL_SPD_100;
		*phy_ctrl |= MII_CR_SPEED_100;
		*phy_ctrl &= ~(MII_CR_SPEED_1000 | MII_CR_SPEED_10);
		e_dbg("Forcing 100mb\n");
	} else {
		ctrl &= ~(E1000_CTRL_SPD_1000 | E1000_CTRL_SPD_100);
		*phy_ctrl |= MII_CR_SPEED_10;
		*phy_ctrl &= ~(MII_CR_SPEED_1000 | MII_CR_SPEED_100);
		e_dbg("Forcing 10mb\n");
	}

	hw->mac.ops.config_collision_dist(hw);

	ew32(CTRL, ctrl);
}

s32 e1000e_set_d3_lplu_state(struct e1000_hw *hw, bool active)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data;

	ret_val = e1e_rphy(hw, IGP02E1000_PHY_POWER_MGMT, &data);
	if (ret_val)
		return ret_val;

	if (!active) {
		data &= ~IGP02E1000_PM_D3_LPLU;
		ret_val = e1e_wphy(hw, IGP02E1000_PHY_POWER_MGMT, data);
		if (ret_val)
			return ret_val;
		if (phy->smart_speed == e1000_smart_speed_on) {
			ret_val = e1e_rphy(hw, IGP01E1000_PHY_PORT_CONFIG,
					   &data);
			if (ret_val)
				return ret_val;

			data |= IGP01E1000_PSCFR_SMART_SPEED;
			ret_val = e1e_wphy(hw, IGP01E1000_PHY_PORT_CONFIG,
					   data);
			if (ret_val)
				return ret_val;
		} else if (phy->smart_speed == e1000_smart_speed_off) {
			ret_val = e1e_rphy(hw, IGP01E1000_PHY_PORT_CONFIG,
					   &data);
			if (ret_val)
				return ret_val;

			data &= ~IGP01E1000_PSCFR_SMART_SPEED;
			ret_val = e1e_wphy(hw, IGP01E1000_PHY_PORT_CONFIG,
					   data);
			if (ret_val)
				return ret_val;
		}
	} else if ((phy->autoneg_advertised == E1000_ALL_SPEED_DUPLEX) ||
		   (phy->autoneg_advertised == E1000_ALL_NOT_GIG) ||
		   (phy->autoneg_advertised == E1000_ALL_10_SPEED)) {
		data |= IGP02E1000_PM_D3_LPLU;
		ret_val = e1e_wphy(hw, IGP02E1000_PHY_POWER_MGMT, data);
		if (ret_val)
			return ret_val;

		
		ret_val = e1e_rphy(hw, IGP01E1000_PHY_PORT_CONFIG, &data);
		if (ret_val)
			return ret_val;

		data &= ~IGP01E1000_PSCFR_SMART_SPEED;
		ret_val = e1e_wphy(hw, IGP01E1000_PHY_PORT_CONFIG, data);
	}

	return ret_val;
}

s32 e1000e_check_downshift(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data, offset, mask;

	switch (phy->type) {
	case e1000_phy_m88:
	case e1000_phy_gg82563:
	case e1000_phy_bm:
	case e1000_phy_82578:
		offset	= M88E1000_PHY_SPEC_STATUS;
		mask	= M88E1000_PSSR_DOWNSHIFT;
		break;
	case e1000_phy_igp_2:
	case e1000_phy_igp_3:
		offset	= IGP01E1000_PHY_LINK_HEALTH;
		mask	= IGP01E1000_PLHR_SS_DOWNGRADE;
		break;
	default:
		
		phy->speed_downgraded = false;
		return 0;
	}

	ret_val = e1e_rphy(hw, offset, &phy_data);

	if (!ret_val)
		phy->speed_downgraded = (phy_data & mask);

	return ret_val;
}

s32 e1000_check_polarity_m88(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data;

	ret_val = e1e_rphy(hw, M88E1000_PHY_SPEC_STATUS, &data);

	if (!ret_val)
		phy->cable_polarity = (data & M88E1000_PSSR_REV_POLARITY)
				      ? e1000_rev_polarity_reversed
				      : e1000_rev_polarity_normal;

	return ret_val;
}

s32 e1000_check_polarity_igp(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data, offset, mask;

	ret_val = e1e_rphy(hw, IGP01E1000_PHY_PORT_STATUS, &data);
	if (ret_val)
		return ret_val;

	if ((data & IGP01E1000_PSSR_SPEED_MASK) ==
	    IGP01E1000_PSSR_SPEED_1000MBPS) {
		offset	= IGP01E1000_PHY_PCS_INIT_REG;
		mask	= IGP01E1000_PHY_POLARITY_MASK;
	} else {
		offset	= IGP01E1000_PHY_PORT_STATUS;
		mask	= IGP01E1000_PSSR_POLARITY_REVERSED;
	}

	ret_val = e1e_rphy(hw, offset, &data);

	if (!ret_val)
		phy->cable_polarity = (data & mask)
				      ? e1000_rev_polarity_reversed
				      : e1000_rev_polarity_normal;

	return ret_val;
}

s32 e1000_check_polarity_ife(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data, offset, mask;

	if (phy->polarity_correction) {
		offset = IFE_PHY_EXTENDED_STATUS_CONTROL;
		mask = IFE_PESC_POLARITY_REVERSED;
	} else {
		offset = IFE_PHY_SPECIAL_CONTROL;
		mask = IFE_PSC_FORCE_POLARITY;
	}

	ret_val = e1e_rphy(hw, offset, &phy_data);

	if (!ret_val)
		phy->cable_polarity = (phy_data & mask)
		                       ? e1000_rev_polarity_reversed
		                       : e1000_rev_polarity_normal;

	return ret_val;
}

static s32 e1000_wait_autoneg(struct e1000_hw *hw)
{
	s32 ret_val = 0;
	u16 i, phy_status;

	
	for (i = PHY_AUTO_NEG_LIMIT; i > 0; i--) {
		ret_val = e1e_rphy(hw, PHY_STATUS, &phy_status);
		if (ret_val)
			break;
		ret_val = e1e_rphy(hw, PHY_STATUS, &phy_status);
		if (ret_val)
			break;
		if (phy_status & MII_SR_AUTONEG_COMPLETE)
			break;
		msleep(100);
	}

	return ret_val;
}

s32 e1000e_phy_has_link_generic(struct e1000_hw *hw, u32 iterations,
			       u32 usec_interval, bool *success)
{
	s32 ret_val = 0;
	u16 i, phy_status;

	for (i = 0; i < iterations; i++) {
		ret_val = e1e_rphy(hw, PHY_STATUS, &phy_status);
		if (ret_val)
			udelay(usec_interval);
		ret_val = e1e_rphy(hw, PHY_STATUS, &phy_status);
		if (ret_val)
			break;
		if (phy_status & MII_SR_LINK_STATUS)
			break;
		if (usec_interval >= 1000)
			mdelay(usec_interval/1000);
		else
			udelay(usec_interval);
	}

	*success = (i < iterations);

	return ret_val;
}

s32 e1000e_get_cable_length_m88(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data, index;

	ret_val = e1e_rphy(hw, M88E1000_PHY_SPEC_STATUS, &phy_data);
	if (ret_val)
		return ret_val;

	index = (phy_data & M88E1000_PSSR_CABLE_LENGTH) >>
	        M88E1000_PSSR_CABLE_LENGTH_SHIFT;

	if (index >= M88E1000_CABLE_LENGTH_TABLE_SIZE - 1)
		return -E1000_ERR_PHY;

	phy->min_cable_length = e1000_m88_cable_length_table[index];
	phy->max_cable_length = e1000_m88_cable_length_table[index + 1];

	phy->cable_length = (phy->min_cable_length + phy->max_cable_length) / 2;

	return 0;
}

s32 e1000e_get_cable_length_igp_2(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
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
		ret_val = e1e_rphy(hw, agc_reg_array[i], &phy_data);
		if (ret_val)
			return ret_val;

		cur_agc_index = (phy_data >> IGP02E1000_AGC_LENGTH_SHIFT) &
				IGP02E1000_AGC_LENGTH_MASK;

		
		if ((cur_agc_index >= IGP02E1000_CABLE_LENGTH_TABLE_SIZE) ||
		    (cur_agc_index == 0))
			return -E1000_ERR_PHY;

		
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

	return 0;
}

s32 e1000e_get_phy_info_m88(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32  ret_val;
	u16 phy_data;
	bool link;

	if (phy->media_type != e1000_media_type_copper) {
		e_dbg("Phy info is only valid for copper media\n");
		return -E1000_ERR_CONFIG;
	}

	ret_val = e1000e_phy_has_link_generic(hw, 1, 0, &link);
	if (ret_val)
		return ret_val;

	if (!link) {
		e_dbg("Phy info is only valid if link is up\n");
		return -E1000_ERR_CONFIG;
	}

	ret_val = e1e_rphy(hw, M88E1000_PHY_SPEC_CTRL, &phy_data);
	if (ret_val)
		return ret_val;

	phy->polarity_correction = (phy_data &
				    M88E1000_PSCR_POLARITY_REVERSAL);

	ret_val = e1000_check_polarity_m88(hw);
	if (ret_val)
		return ret_val;

	ret_val = e1e_rphy(hw, M88E1000_PHY_SPEC_STATUS, &phy_data);
	if (ret_val)
		return ret_val;

	phy->is_mdix = (phy_data & M88E1000_PSSR_MDIX);

	if ((phy_data & M88E1000_PSSR_SPEED) == M88E1000_PSSR_1000MBS) {
		ret_val = e1000_get_cable_length(hw);
		if (ret_val)
			return ret_val;

		ret_val = e1e_rphy(hw, PHY_1000T_STATUS, &phy_data);
		if (ret_val)
			return ret_val;

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

	return ret_val;
}

s32 e1000e_get_phy_info_igp(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data;
	bool link;

	ret_val = e1000e_phy_has_link_generic(hw, 1, 0, &link);
	if (ret_val)
		return ret_val;

	if (!link) {
		e_dbg("Phy info is only valid if link is up\n");
		return -E1000_ERR_CONFIG;
	}

	phy->polarity_correction = true;

	ret_val = e1000_check_polarity_igp(hw);
	if (ret_val)
		return ret_val;

	ret_val = e1e_rphy(hw, IGP01E1000_PHY_PORT_STATUS, &data);
	if (ret_val)
		return ret_val;

	phy->is_mdix = (data & IGP01E1000_PSSR_MDIX);

	if ((data & IGP01E1000_PSSR_SPEED_MASK) ==
	    IGP01E1000_PSSR_SPEED_1000MBPS) {
		ret_val = e1000_get_cable_length(hw);
		if (ret_val)
			return ret_val;

		ret_val = e1e_rphy(hw, PHY_1000T_STATUS, &data);
		if (ret_val)
			return ret_val;

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

	return ret_val;
}

s32 e1000_get_phy_info_ife(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data;
	bool link;

	ret_val = e1000e_phy_has_link_generic(hw, 1, 0, &link);
	if (ret_val)
		return ret_val;

	if (!link) {
		e_dbg("Phy info is only valid if link is up\n");
		return -E1000_ERR_CONFIG;
	}

	ret_val = e1e_rphy(hw, IFE_PHY_SPECIAL_CONTROL, &data);
	if (ret_val)
		return ret_val;
	phy->polarity_correction = (data & IFE_PSC_AUTO_POLARITY_DISABLE)
	                           ? false : true;

	if (phy->polarity_correction) {
		ret_val = e1000_check_polarity_ife(hw);
		if (ret_val)
			return ret_val;
	} else {
		
		phy->cable_polarity = (data & IFE_PSC_FORCE_POLARITY)
		                      ? e1000_rev_polarity_reversed
		                      : e1000_rev_polarity_normal;
	}

	ret_val = e1e_rphy(hw, IFE_PHY_MDIX_CONTROL, &data);
	if (ret_val)
		return ret_val;

	phy->is_mdix = (data & IFE_PMC_MDIX_STATUS) ? true : false;

	
	phy->cable_length = E1000_CABLE_LENGTH_UNDEFINED;
	phy->local_rx = e1000_1000t_rx_status_undefined;
	phy->remote_rx = e1000_1000t_rx_status_undefined;

	return 0;
}

s32 e1000e_phy_sw_reset(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 phy_ctrl;

	ret_val = e1e_rphy(hw, PHY_CONTROL, &phy_ctrl);
	if (ret_val)
		return ret_val;

	phy_ctrl |= MII_CR_RESET;
	ret_val = e1e_wphy(hw, PHY_CONTROL, phy_ctrl);
	if (ret_val)
		return ret_val;

	udelay(1);

	return ret_val;
}

s32 e1000e_phy_hw_reset_generic(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u32 ctrl;

	ret_val = phy->ops.check_reset_block(hw);
	if (ret_val)
		return 0;

	ret_val = phy->ops.acquire(hw);
	if (ret_val)
		return ret_val;

	ctrl = er32(CTRL);
	ew32(CTRL, ctrl | E1000_CTRL_PHY_RST);
	e1e_flush();

	udelay(phy->reset_delay_us);

	ew32(CTRL, ctrl);
	e1e_flush();

	udelay(150);

	phy->ops.release(hw);

	return e1000_get_phy_cfg_done(hw);
}

s32 e1000e_get_cfg_done(struct e1000_hw *hw)
{
	mdelay(10);

	return 0;
}

s32 e1000e_phy_init_script_igp3(struct e1000_hw *hw)
{
	e_dbg("Running IGP 3 PHY init script\n");

	
	
	e1e_wphy(hw, 0x2F5B, 0x9018);
	
	e1e_wphy(hw, 0x2F52, 0x0000);
	
	e1e_wphy(hw, 0x2FB1, 0x8B24);
	
	e1e_wphy(hw, 0x2FB2, 0xF8F0);
	
	e1e_wphy(hw, 0x2010, 0x10B0);
	
	e1e_wphy(hw, 0x2011, 0x0000);
	
	e1e_wphy(hw, 0x20DD, 0x249A);
	
	e1e_wphy(hw, 0x20DE, 0x00D3);
	
	e1e_wphy(hw, 0x28B4, 0x04CE);
	
	e1e_wphy(hw, 0x2F70, 0x29E4);
	
	e1e_wphy(hw, 0x0000, 0x0140);
	
	e1e_wphy(hw, 0x1F30, 0x1606);
	
	e1e_wphy(hw, 0x1F31, 0xB814);
	
	e1e_wphy(hw, 0x1F35, 0x002A);
	
	e1e_wphy(hw, 0x1F3E, 0x0067);
	
	e1e_wphy(hw, 0x1F54, 0x0065);
	
	e1e_wphy(hw, 0x1F55, 0x002A);
	
	e1e_wphy(hw, 0x1F56, 0x002A);
	
	e1e_wphy(hw, 0x1F72, 0x3FB0);
	
	e1e_wphy(hw, 0x1F76, 0xC0FF);
	
	e1e_wphy(hw, 0x1F77, 0x1DEC);
	
	e1e_wphy(hw, 0x1F78, 0xF9EF);
	
	e1e_wphy(hw, 0x1F79, 0x0210);
	
	e1e_wphy(hw, 0x1895, 0x0003);
	
	e1e_wphy(hw, 0x1796, 0x0008);
	
	e1e_wphy(hw, 0x1798, 0xD008);
	e1e_wphy(hw, 0x1898, 0xD918);
	
	e1e_wphy(hw, 0x187A, 0x0800);
	e1e_wphy(hw, 0x0019, 0x008D);
	
	e1e_wphy(hw, 0x001B, 0x2080);
	
	e1e_wphy(hw, 0x0014, 0x0045);
	
	e1e_wphy(hw, 0x0000, 0x1340);

	return 0;
}


static s32 e1000_get_phy_cfg_done(struct e1000_hw *hw)
{
	if (hw->phy.ops.get_cfg_done)
		return hw->phy.ops.get_cfg_done(hw);

	return 0;
}

static s32 e1000_phy_force_speed_duplex(struct e1000_hw *hw)
{
	if (hw->phy.ops.force_speed_duplex)
		return hw->phy.ops.force_speed_duplex(hw);

	return 0;
}

enum e1000_phy_type e1000e_get_phy_type_from_id(u32 phy_id)
{
	enum e1000_phy_type phy_type = e1000_phy_unknown;

	switch (phy_id) {
	case M88E1000_I_PHY_ID:
	case M88E1000_E_PHY_ID:
	case M88E1111_I_PHY_ID:
	case M88E1011_I_PHY_ID:
		phy_type = e1000_phy_m88;
		break;
	case IGP01E1000_I_PHY_ID: 
		phy_type = e1000_phy_igp_2;
		break;
	case GG82563_E_PHY_ID:
		phy_type = e1000_phy_gg82563;
		break;
	case IGP03E1000_E_PHY_ID:
		phy_type = e1000_phy_igp_3;
		break;
	case IFE_E_PHY_ID:
	case IFE_PLUS_E_PHY_ID:
	case IFE_C_E_PHY_ID:
		phy_type = e1000_phy_ife;
		break;
	case BME1000_E_PHY_ID:
	case BME1000_E_PHY_ID_R2:
		phy_type = e1000_phy_bm;
		break;
	case I82578_E_PHY_ID:
		phy_type = e1000_phy_82578;
		break;
	case I82577_E_PHY_ID:
		phy_type = e1000_phy_82577;
		break;
	case I82579_E_PHY_ID:
		phy_type = e1000_phy_82579;
		break;
	default:
		phy_type = e1000_phy_unknown;
		break;
	}
	return phy_type;
}

s32 e1000e_determine_phy_address(struct e1000_hw *hw)
{
	u32 phy_addr = 0;
	u32 i;
	enum e1000_phy_type phy_type = e1000_phy_unknown;

	hw->phy.id = phy_type;

	for (phy_addr = 0; phy_addr < E1000_MAX_PHY_ADDR; phy_addr++) {
		hw->phy.addr = phy_addr;
		i = 0;

		do {
			e1000e_get_phy_id(hw);
			phy_type = e1000e_get_phy_type_from_id(hw->phy.id);

			if (phy_type  != e1000_phy_unknown)
				return 0;

			usleep_range(1000, 2000);
			i++;
		} while (i < 10);
	}

	return -E1000_ERR_PHY_TYPE;
}

static u32 e1000_get_phy_addr_for_bm_page(u32 page, u32 reg)
{
	u32 phy_addr = 2;

	if ((page >= 768) || (page == 0 && reg == 25) || (reg == 31))
		phy_addr = 1;

	return phy_addr;
}

s32 e1000e_write_phy_reg_bm(struct e1000_hw *hw, u32 offset, u16 data)
{
	s32 ret_val;
	u32 page = offset >> IGP_PAGE_SHIFT;

	ret_val = hw->phy.ops.acquire(hw);
	if (ret_val)
		return ret_val;

	
	if (page == BM_WUC_PAGE) {
		ret_val = e1000_access_phy_wakeup_reg_bm(hw, offset, &data,
							 false, false);
		goto release;
	}

	hw->phy.addr = e1000_get_phy_addr_for_bm_page(page, offset);

	if (offset > MAX_PHY_MULTI_PAGE_REG) {
		u32 page_shift, page_select;

		if (hw->phy.addr == 1) {
			page_shift = IGP_PAGE_SHIFT;
			page_select = IGP01E1000_PHY_PAGE_SELECT;
		} else {
			page_shift = 0;
			page_select = BM_PHY_PAGE_SELECT;
		}

		
		ret_val = e1000e_write_phy_reg_mdic(hw, page_select,
		                                    (page << page_shift));
		if (ret_val)
			goto release;
	}

	ret_val = e1000e_write_phy_reg_mdic(hw, MAX_PHY_REG_ADDRESS & offset,
	                                    data);

release:
	hw->phy.ops.release(hw);
	return ret_val;
}

s32 e1000e_read_phy_reg_bm(struct e1000_hw *hw, u32 offset, u16 *data)
{
	s32 ret_val;
	u32 page = offset >> IGP_PAGE_SHIFT;

	ret_val = hw->phy.ops.acquire(hw);
	if (ret_val)
		return ret_val;

	
	if (page == BM_WUC_PAGE) {
		ret_val = e1000_access_phy_wakeup_reg_bm(hw, offset, data,
							 true, false);
		goto release;
	}

	hw->phy.addr = e1000_get_phy_addr_for_bm_page(page, offset);

	if (offset > MAX_PHY_MULTI_PAGE_REG) {
		u32 page_shift, page_select;

		if (hw->phy.addr == 1) {
			page_shift = IGP_PAGE_SHIFT;
			page_select = IGP01E1000_PHY_PAGE_SELECT;
		} else {
			page_shift = 0;
			page_select = BM_PHY_PAGE_SELECT;
		}

		
		ret_val = e1000e_write_phy_reg_mdic(hw, page_select,
		                                    (page << page_shift));
		if (ret_val)
			goto release;
	}

	ret_val = e1000e_read_phy_reg_mdic(hw, MAX_PHY_REG_ADDRESS & offset,
	                                   data);
release:
	hw->phy.ops.release(hw);
	return ret_val;
}

s32 e1000e_read_phy_reg_bm2(struct e1000_hw *hw, u32 offset, u16 *data)
{
	s32 ret_val;
	u16 page = (u16)(offset >> IGP_PAGE_SHIFT);

	ret_val = hw->phy.ops.acquire(hw);
	if (ret_val)
		return ret_val;

	
	if (page == BM_WUC_PAGE) {
		ret_val = e1000_access_phy_wakeup_reg_bm(hw, offset, data,
							 true, false);
		goto release;
	}

	hw->phy.addr = 1;

	if (offset > MAX_PHY_MULTI_PAGE_REG) {

		
		ret_val = e1000e_write_phy_reg_mdic(hw, BM_PHY_PAGE_SELECT,
						    page);

		if (ret_val)
			goto release;
	}

	ret_val = e1000e_read_phy_reg_mdic(hw, MAX_PHY_REG_ADDRESS & offset,
					   data);
release:
	hw->phy.ops.release(hw);
	return ret_val;
}

s32 e1000e_write_phy_reg_bm2(struct e1000_hw *hw, u32 offset, u16 data)
{
	s32 ret_val;
	u16 page = (u16)(offset >> IGP_PAGE_SHIFT);

	ret_val = hw->phy.ops.acquire(hw);
	if (ret_val)
		return ret_val;

	
	if (page == BM_WUC_PAGE) {
		ret_val = e1000_access_phy_wakeup_reg_bm(hw, offset, &data,
							 false, false);
		goto release;
	}

	hw->phy.addr = 1;

	if (offset > MAX_PHY_MULTI_PAGE_REG) {
		
		ret_val = e1000e_write_phy_reg_mdic(hw, BM_PHY_PAGE_SELECT,
						    page);

		if (ret_val)
			goto release;
	}

	ret_val = e1000e_write_phy_reg_mdic(hw, MAX_PHY_REG_ADDRESS & offset,
					    data);

release:
	hw->phy.ops.release(hw);
	return ret_val;
}

s32 e1000_enable_phy_wakeup_reg_access_bm(struct e1000_hw *hw, u16 *phy_reg)
{
	s32 ret_val;
	u16 temp;

	
	hw->phy.addr = 1;

	
	ret_val = e1000_set_page_igp(hw, (BM_PORT_CTRL_PAGE << IGP_PAGE_SHIFT));
	if (ret_val) {
		e_dbg("Could not set Port Control page\n");
		return ret_val;
	}

	ret_val = e1000e_read_phy_reg_mdic(hw, BM_WUC_ENABLE_REG, phy_reg);
	if (ret_val) {
		e_dbg("Could not read PHY register %d.%d\n",
		      BM_PORT_CTRL_PAGE, BM_WUC_ENABLE_REG);
		return ret_val;
	}

	temp = *phy_reg;
	temp |= BM_WUC_ENABLE_BIT;
	temp &= ~(BM_WUC_ME_WU_BIT | BM_WUC_HOST_WU_BIT);

	ret_val = e1000e_write_phy_reg_mdic(hw, BM_WUC_ENABLE_REG, temp);
	if (ret_val) {
		e_dbg("Could not write PHY register %d.%d\n",
		      BM_PORT_CTRL_PAGE, BM_WUC_ENABLE_REG);
		return ret_val;
	}

	return e1000_set_page_igp(hw, (BM_WUC_PAGE << IGP_PAGE_SHIFT));
}

s32 e1000_disable_phy_wakeup_reg_access_bm(struct e1000_hw *hw, u16 *phy_reg)
{
	s32 ret_val = 0;

	
	ret_val = e1000_set_page_igp(hw, (BM_PORT_CTRL_PAGE << IGP_PAGE_SHIFT));
	if (ret_val) {
		e_dbg("Could not set Port Control page\n");
		return ret_val;
	}

	
	ret_val = e1000e_write_phy_reg_mdic(hw, BM_WUC_ENABLE_REG, *phy_reg);
	if (ret_val)
		e_dbg("Could not restore PHY register %d.%d\n",
		      BM_PORT_CTRL_PAGE, BM_WUC_ENABLE_REG);

	return ret_val;
}

/**
 *  e1000_access_phy_wakeup_reg_bm - Read/write BM PHY wakeup register
 *  @hw: pointer to the HW structure
 *  @offset: register offset to be read or written
 *  @data: pointer to the data to read or write
 *  @read: determines if operation is read or write
 *  @page_set: BM_WUC_PAGE already set and access enabled
 *
 *  Read the PHY register at offset and store the retrieved information in
 *  data, or write data to PHY register at offset.  Note the procedure to
 *  access the PHY wakeup registers is different than reading the other PHY
 *  registers. It works as such:
 *  1) Set 769.17.2 (page 769, register 17, bit 2) = 1
 *  2) Set page to 800 for host (801 if we were manageability)
 *  3) Write the address using the address opcode (0x11)
 *  4) Read or write the data using the data opcode (0x12)
 *  5) Restore 769.17.2 to its original value
 *
 *  Steps 1 and 2 are done by e1000_enable_phy_wakeup_reg_access_bm() and
 *  step 5 is done by e1000_disable_phy_wakeup_reg_access_bm().
 *
 *  Assumes semaphore is already acquired.  When page_set==true, assumes
 *  the PHY page is set to BM_WUC_PAGE (i.e. a function in the call stack
 *  is responsible for calls to e1000_[enable|disable]_phy_wakeup_reg_bm()).
 **/
static s32 e1000_access_phy_wakeup_reg_bm(struct e1000_hw *hw, u32 offset,
					  u16 *data, bool read, bool page_set)
{
	s32 ret_val;
	u16 reg = BM_PHY_REG_NUM(offset);
	u16 page = BM_PHY_REG_PAGE(offset);
	u16 phy_reg = 0;

	
	if ((hw->mac.type == e1000_pchlan) &&
	    (!(er32(PHY_CTRL) & E1000_PHY_CTRL_GBE_DISABLE)))
		e_dbg("Attempting to access page %d while gig enabled.\n",
		      page);

	if (!page_set) {
		
		ret_val = e1000_enable_phy_wakeup_reg_access_bm(hw, &phy_reg);
		if (ret_val) {
			e_dbg("Could not enable PHY wakeup reg access\n");
			return ret_val;
		}
	}

	e_dbg("Accessing PHY page %d reg 0x%x\n", page, reg);

	
	ret_val = e1000e_write_phy_reg_mdic(hw, BM_WUC_ADDRESS_OPCODE, reg);
	if (ret_val) {
		e_dbg("Could not write address opcode to page %d\n", page);
		return ret_val;
	}

	if (read) {
		
		ret_val = e1000e_read_phy_reg_mdic(hw, BM_WUC_DATA_OPCODE,
		                                   data);
	} else {
		
		ret_val = e1000e_write_phy_reg_mdic(hw, BM_WUC_DATA_OPCODE,
						    *data);
	}

	if (ret_val) {
		e_dbg("Could not access PHY reg %d.%d\n", page, reg);
		return ret_val;
	}

	if (!page_set)
		ret_val = e1000_disable_phy_wakeup_reg_access_bm(hw, &phy_reg);

	return ret_val;
}

void e1000_power_up_phy_copper(struct e1000_hw *hw)
{
	u16 mii_reg = 0;

	
	e1e_rphy(hw, PHY_CONTROL, &mii_reg);
	mii_reg &= ~MII_CR_POWER_DOWN;
	e1e_wphy(hw, PHY_CONTROL, mii_reg);
}

void e1000_power_down_phy_copper(struct e1000_hw *hw)
{
	u16 mii_reg = 0;

	
	e1e_rphy(hw, PHY_CONTROL, &mii_reg);
	mii_reg |= MII_CR_POWER_DOWN;
	e1e_wphy(hw, PHY_CONTROL, mii_reg);
	usleep_range(1000, 2000);
}

s32 e1000e_commit_phy(struct e1000_hw *hw)
{
	if (hw->phy.ops.commit)
		return hw->phy.ops.commit(hw);

	return 0;
}

static s32 e1000_set_d0_lplu_state(struct e1000_hw *hw, bool active)
{
	if (hw->phy.ops.set_d0_lplu_state)
		return hw->phy.ops.set_d0_lplu_state(hw, active);

	return 0;
}

static s32 __e1000_read_phy_reg_hv(struct e1000_hw *hw, u32 offset, u16 *data,
				   bool locked, bool page_set)
{
	s32 ret_val;
	u16 page = BM_PHY_REG_PAGE(offset);
	u16 reg = BM_PHY_REG_NUM(offset);
	u32 phy_addr = hw->phy.addr = e1000_get_phy_addr_for_hv_page(page);

	if (!locked) {
		ret_val = hw->phy.ops.acquire(hw);
		if (ret_val)
			return ret_val;
	}

	
	if (page == BM_WUC_PAGE) {
		ret_val = e1000_access_phy_wakeup_reg_bm(hw, offset, data,
							 true, page_set);
		goto out;
	}

	if (page > 0 && page < HV_INTC_FC_PAGE_START) {
		ret_val = e1000_access_phy_debug_regs_hv(hw, offset,
		                                         data, true);
		goto out;
	}

	if (!page_set) {
		if (page == HV_INTC_FC_PAGE_START)
			page = 0;

		if (reg > MAX_PHY_MULTI_PAGE_REG) {
			
			ret_val = e1000_set_page_igp(hw,
						     (page << IGP_PAGE_SHIFT));

			hw->phy.addr = phy_addr;

			if (ret_val)
				goto out;
		}
	}

	e_dbg("reading PHY page %d (or 0x%x shifted) reg 0x%x\n", page,
	      page << IGP_PAGE_SHIFT, reg);

	ret_val = e1000e_read_phy_reg_mdic(hw, MAX_PHY_REG_ADDRESS & reg,
	                                  data);
out:
	if (!locked)
		hw->phy.ops.release(hw);

	return ret_val;
}

s32 e1000_read_phy_reg_hv(struct e1000_hw *hw, u32 offset, u16 *data)
{
	return __e1000_read_phy_reg_hv(hw, offset, data, false, false);
}

s32 e1000_read_phy_reg_hv_locked(struct e1000_hw *hw, u32 offset, u16 *data)
{
	return __e1000_read_phy_reg_hv(hw, offset, data, true, false);
}

s32 e1000_read_phy_reg_page_hv(struct e1000_hw *hw, u32 offset, u16 *data)
{
	return __e1000_read_phy_reg_hv(hw, offset, data, true, true);
}

static s32 __e1000_write_phy_reg_hv(struct e1000_hw *hw, u32 offset, u16 data,
				    bool locked, bool page_set)
{
	s32 ret_val;
	u16 page = BM_PHY_REG_PAGE(offset);
	u16 reg = BM_PHY_REG_NUM(offset);
	u32 phy_addr = hw->phy.addr = e1000_get_phy_addr_for_hv_page(page);

	if (!locked) {
		ret_val = hw->phy.ops.acquire(hw);
		if (ret_val)
			return ret_val;
	}

	
	if (page == BM_WUC_PAGE) {
		ret_val = e1000_access_phy_wakeup_reg_bm(hw, offset, &data,
							 false, page_set);
		goto out;
	}

	if (page > 0 && page < HV_INTC_FC_PAGE_START) {
		ret_val = e1000_access_phy_debug_regs_hv(hw, offset,
		                                         &data, false);
		goto out;
	}

	if (!page_set) {
		if (page == HV_INTC_FC_PAGE_START)
			page = 0;

		if ((hw->phy.type == e1000_phy_82578) &&
		    (hw->phy.revision >= 1) &&
		    (hw->phy.addr == 2) &&
		    ((MAX_PHY_REG_ADDRESS & reg) == 0) && (data & (1 << 11))) {
			u16 data2 = 0x7EFF;
			ret_val = e1000_access_phy_debug_regs_hv(hw,
								 (1 << 6) | 0x3,
								 &data2, false);
			if (ret_val)
				goto out;
		}

		if (reg > MAX_PHY_MULTI_PAGE_REG) {
			
			ret_val = e1000_set_page_igp(hw,
						     (page << IGP_PAGE_SHIFT));

			hw->phy.addr = phy_addr;

			if (ret_val)
				goto out;
		}
	}

	e_dbg("writing PHY page %d (or 0x%x shifted) reg 0x%x\n", page,
	      page << IGP_PAGE_SHIFT, reg);

	ret_val = e1000e_write_phy_reg_mdic(hw, MAX_PHY_REG_ADDRESS & reg,
	                                  data);

out:
	if (!locked)
		hw->phy.ops.release(hw);

	return ret_val;
}

s32 e1000_write_phy_reg_hv(struct e1000_hw *hw, u32 offset, u16 data)
{
	return __e1000_write_phy_reg_hv(hw, offset, data, false, false);
}

s32 e1000_write_phy_reg_hv_locked(struct e1000_hw *hw, u32 offset, u16 data)
{
	return __e1000_write_phy_reg_hv(hw, offset, data, true, false);
}

s32 e1000_write_phy_reg_page_hv(struct e1000_hw *hw, u32 offset, u16 data)
{
	return __e1000_write_phy_reg_hv(hw, offset, data, true, true);
}

static u32 e1000_get_phy_addr_for_hv_page(u32 page)
{
	u32 phy_addr = 2;

	if (page >= HV_INTC_FC_PAGE_START)
		phy_addr = 1;

	return phy_addr;
}

/**
 *  e1000_access_phy_debug_regs_hv - Read HV PHY vendor specific high registers
 *  @hw: pointer to the HW structure
 *  @offset: register offset to be read or written
 *  @data: pointer to the data to be read or written
 *  @read: determines if operation is read or write
 *
 *  Reads the PHY register at offset and stores the retreived information
 *  in data.  Assumes semaphore already acquired.  Note that the procedure
 *  to access these regs uses the address port and data port to read/write.
 *  These accesses done with PHY address 2 and without using pages.
 **/
static s32 e1000_access_phy_debug_regs_hv(struct e1000_hw *hw, u32 offset,
                                          u16 *data, bool read)
{
	s32 ret_val;
	u32 addr_reg = 0;
	u32 data_reg = 0;

	
	addr_reg = (hw->phy.type == e1000_phy_82578) ?
	           I82578_ADDR_REG : I82577_ADDR_REG;
	data_reg = addr_reg + 1;

	
	hw->phy.addr = 2;

	
	ret_val = e1000e_write_phy_reg_mdic(hw, addr_reg, (u16)offset & 0x3F);
	if (ret_val) {
		e_dbg("Could not write the Address Offset port register\n");
		return ret_val;
	}

	
	if (read)
		ret_val = e1000e_read_phy_reg_mdic(hw, data_reg, data);
	else
		ret_val = e1000e_write_phy_reg_mdic(hw, data_reg, *data);

	if (ret_val)
		e_dbg("Could not access the Data port register\n");

	return ret_val;
}

s32 e1000_link_stall_workaround_hv(struct e1000_hw *hw)
{
	s32 ret_val = 0;
	u16 data;

	if (hw->phy.type != e1000_phy_82578)
		return 0;

	
	e1e_rphy(hw, PHY_CONTROL, &data);
	if (data & PHY_CONTROL_LB)
		return 0;

	
	ret_val = e1e_rphy(hw, BM_CS_STATUS, &data);
	if (ret_val)
		return ret_val;

	data &= BM_CS_STATUS_LINK_UP | BM_CS_STATUS_RESOLVED |
		BM_CS_STATUS_SPEED_MASK;

	if (data != (BM_CS_STATUS_LINK_UP | BM_CS_STATUS_RESOLVED |
		     BM_CS_STATUS_SPEED_1000))
		return 0;

	msleep(200);

	
	ret_val = e1e_wphy(hw, HV_MUX_DATA_CTRL, HV_MUX_DATA_CTRL_GEN_TO_MAC |
			   HV_MUX_DATA_CTRL_FORCE_SPEED);
	if (ret_val)
		return ret_val;

	return e1e_wphy(hw, HV_MUX_DATA_CTRL, HV_MUX_DATA_CTRL_GEN_TO_MAC);
}

s32 e1000_check_polarity_82577(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data;

	ret_val = e1e_rphy(hw, I82577_PHY_STATUS_2, &data);

	if (!ret_val)
		phy->cable_polarity = (data & I82577_PHY_STATUS2_REV_POLARITY)
		                      ? e1000_rev_polarity_reversed
		                      : e1000_rev_polarity_normal;

	return ret_val;
}

s32 e1000_phy_force_speed_duplex_82577(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data;
	bool link;

	ret_val = e1e_rphy(hw, PHY_CONTROL, &phy_data);
	if (ret_val)
		return ret_val;

	e1000e_phy_force_speed_duplex_setup(hw, &phy_data);

	ret_val = e1e_wphy(hw, PHY_CONTROL, phy_data);
	if (ret_val)
		return ret_val;

	udelay(1);

	if (phy->autoneg_wait_to_complete) {
		e_dbg("Waiting for forced speed/duplex link on 82577 phy\n");

		ret_val = e1000e_phy_has_link_generic(hw, PHY_FORCE_LIMIT,
						      100000, &link);
		if (ret_val)
			return ret_val;

		if (!link)
			e_dbg("Link taking longer than expected.\n");

		
		ret_val = e1000e_phy_has_link_generic(hw, PHY_FORCE_LIMIT,
						      100000, &link);
	}

	return ret_val;
}

s32 e1000_get_phy_info_82577(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data;
	bool link;

	ret_val = e1000e_phy_has_link_generic(hw, 1, 0, &link);
	if (ret_val)
		return ret_val;

	if (!link) {
		e_dbg("Phy info is only valid if link is up\n");
		return -E1000_ERR_CONFIG;
	}

	phy->polarity_correction = true;

	ret_val = e1000_check_polarity_82577(hw);
	if (ret_val)
		return ret_val;

	ret_val = e1e_rphy(hw, I82577_PHY_STATUS_2, &data);
	if (ret_val)
		return ret_val;

	phy->is_mdix = (data & I82577_PHY_STATUS2_MDIX) ? true : false;

	if ((data & I82577_PHY_STATUS2_SPEED_MASK) ==
	    I82577_PHY_STATUS2_SPEED_1000MBPS) {
		ret_val = hw->phy.ops.get_cable_length(hw);
		if (ret_val)
			return ret_val;

		ret_val = e1e_rphy(hw, PHY_1000T_STATUS, &data);
		if (ret_val)
			return ret_val;

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

	return 0;
}

s32 e1000_get_cable_length_82577(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_data, length;

	ret_val = e1e_rphy(hw, I82577_PHY_DIAG_STATUS, &phy_data);
	if (ret_val)
		return ret_val;

	length = (phy_data & I82577_DSTATUS_CABLE_LENGTH) >>
	         I82577_DSTATUS_CABLE_LENGTH_SHIFT;

	if (length == E1000_CABLE_LENGTH_UNDEFINED)
		ret_val = -E1000_ERR_PHY;

	phy->cable_length = length;

	return 0;
}
