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


#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/types.h>
#include <linux/if_ether.h>

#include "e1000_mac.h"
#include "e1000_82575.h"

static s32  igb_get_invariants_82575(struct e1000_hw *);
static s32  igb_acquire_phy_82575(struct e1000_hw *);
static void igb_release_phy_82575(struct e1000_hw *);
static s32  igb_acquire_nvm_82575(struct e1000_hw *);
static void igb_release_nvm_82575(struct e1000_hw *);
static s32  igb_check_for_link_82575(struct e1000_hw *);
static s32  igb_get_cfg_done_82575(struct e1000_hw *);
static s32  igb_init_hw_82575(struct e1000_hw *);
static s32  igb_phy_hw_reset_sgmii_82575(struct e1000_hw *);
static s32  igb_read_phy_reg_sgmii_82575(struct e1000_hw *, u32, u16 *);
static s32  igb_read_phy_reg_82580(struct e1000_hw *, u32, u16 *);
static s32  igb_write_phy_reg_82580(struct e1000_hw *, u32, u16);
static s32  igb_reset_hw_82575(struct e1000_hw *);
static s32  igb_reset_hw_82580(struct e1000_hw *);
static s32  igb_set_d0_lplu_state_82575(struct e1000_hw *, bool);
static s32  igb_setup_copper_link_82575(struct e1000_hw *);
static s32  igb_setup_serdes_link_82575(struct e1000_hw *);
static s32  igb_write_phy_reg_sgmii_82575(struct e1000_hw *, u32, u16);
static void igb_clear_hw_cntrs_82575(struct e1000_hw *);
static s32  igb_acquire_swfw_sync_82575(struct e1000_hw *, u16);
static s32  igb_get_pcs_speed_and_duplex_82575(struct e1000_hw *, u16 *,
						 u16 *);
static s32  igb_get_phy_id_82575(struct e1000_hw *);
static void igb_release_swfw_sync_82575(struct e1000_hw *, u16);
static bool igb_sgmii_active_82575(struct e1000_hw *);
static s32  igb_reset_init_script_82575(struct e1000_hw *);
static s32  igb_read_mac_addr_82575(struct e1000_hw *);
static s32  igb_set_pcie_completion_timeout(struct e1000_hw *hw);
static s32  igb_reset_mdicnfg_82580(struct e1000_hw *hw);
static s32  igb_validate_nvm_checksum_82580(struct e1000_hw *hw);
static s32  igb_update_nvm_checksum_82580(struct e1000_hw *hw);
static s32 igb_validate_nvm_checksum_i350(struct e1000_hw *hw);
static s32 igb_update_nvm_checksum_i350(struct e1000_hw *hw);
static const u16 e1000_82580_rxpbs_table[] =
	{ 36, 72, 144, 1, 2, 4, 8, 16,
	  35, 70, 140 };
#define E1000_82580_RXPBS_TABLE_SIZE \
	(sizeof(e1000_82580_rxpbs_table)/sizeof(u16))

static bool igb_sgmii_uses_mdio_82575(struct e1000_hw *hw)
{
	u32 reg = 0;
	bool ext_mdio = false;

	switch (hw->mac.type) {
	case e1000_82575:
	case e1000_82576:
		reg = rd32(E1000_MDIC);
		ext_mdio = !!(reg & E1000_MDIC_DEST);
		break;
	case e1000_82580:
	case e1000_i350:
		reg = rd32(E1000_MDICNFG);
		ext_mdio = !!(reg & E1000_MDICNFG_EXT_MDIO);
		break;
	default:
		break;
	}
	return ext_mdio;
}

static s32 igb_get_invariants_82575(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	struct e1000_nvm_info *nvm = &hw->nvm;
	struct e1000_mac_info *mac = &hw->mac;
	struct e1000_dev_spec_82575 * dev_spec = &hw->dev_spec._82575;
	u32 eecd;
	s32 ret_val;
	u16 size;
	u32 ctrl_ext = 0;

	switch (hw->device_id) {
	case E1000_DEV_ID_82575EB_COPPER:
	case E1000_DEV_ID_82575EB_FIBER_SERDES:
	case E1000_DEV_ID_82575GB_QUAD_COPPER:
		mac->type = e1000_82575;
		break;
	case E1000_DEV_ID_82576:
	case E1000_DEV_ID_82576_NS:
	case E1000_DEV_ID_82576_NS_SERDES:
	case E1000_DEV_ID_82576_FIBER:
	case E1000_DEV_ID_82576_SERDES:
	case E1000_DEV_ID_82576_QUAD_COPPER:
	case E1000_DEV_ID_82576_QUAD_COPPER_ET2:
	case E1000_DEV_ID_82576_SERDES_QUAD:
		mac->type = e1000_82576;
		break;
	case E1000_DEV_ID_82580_COPPER:
	case E1000_DEV_ID_82580_FIBER:
	case E1000_DEV_ID_82580_QUAD_FIBER:
	case E1000_DEV_ID_82580_SERDES:
	case E1000_DEV_ID_82580_SGMII:
	case E1000_DEV_ID_82580_COPPER_DUAL:
	case E1000_DEV_ID_DH89XXCC_SGMII:
	case E1000_DEV_ID_DH89XXCC_SERDES:
	case E1000_DEV_ID_DH89XXCC_BACKPLANE:
	case E1000_DEV_ID_DH89XXCC_SFP:
		mac->type = e1000_82580;
		break;
	case E1000_DEV_ID_I350_COPPER:
	case E1000_DEV_ID_I350_FIBER:
	case E1000_DEV_ID_I350_SERDES:
	case E1000_DEV_ID_I350_SGMII:
		mac->type = e1000_i350;
		break;
	default:
		return -E1000_ERR_MAC_INIT;
		break;
	}

	
	phy->media_type = e1000_media_type_copper;
	dev_spec->sgmii_active = false;

	ctrl_ext = rd32(E1000_CTRL_EXT);
	switch (ctrl_ext & E1000_CTRL_EXT_LINK_MODE_MASK) {
	case E1000_CTRL_EXT_LINK_MODE_SGMII:
		dev_spec->sgmii_active = true;
		break;
	case E1000_CTRL_EXT_LINK_MODE_1000BASE_KX:
	case E1000_CTRL_EXT_LINK_MODE_PCIE_SERDES:
		hw->phy.media_type = e1000_media_type_internal_serdes;
		break;
	default:
		break;
	}

	
	mac->mta_reg_count = 128;
	
	mac->rar_entry_count = E1000_RAR_ENTRIES_82575;
	if (mac->type == e1000_82576)
		mac->rar_entry_count = E1000_RAR_ENTRIES_82576;
	if (mac->type == e1000_82580)
		mac->rar_entry_count = E1000_RAR_ENTRIES_82580;
	if (mac->type == e1000_i350)
		mac->rar_entry_count = E1000_RAR_ENTRIES_I350;
	
	if (mac->type >= e1000_82580)
		mac->ops.reset_hw = igb_reset_hw_82580;
	else
		mac->ops.reset_hw = igb_reset_hw_82575;
	
	mac->asf_firmware_present = true;
	
	mac->arc_subsystem_valid =
		(rd32(E1000_FWSM) & E1000_FWSM_MODE_MASK)
			? true : false;
	
	if (mac->type == e1000_i350)
		dev_spec->eee_disable = false;
	else
		dev_spec->eee_disable = true;
	
	mac->ops.setup_physical_interface =
		(hw->phy.media_type == e1000_media_type_copper)
			? igb_setup_copper_link_82575
			: igb_setup_serdes_link_82575;

	
	eecd = rd32(E1000_EECD);

	nvm->opcode_bits        = 8;
	nvm->delay_usec         = 1;
	switch (nvm->override) {
	case e1000_nvm_override_spi_large:
		nvm->page_size    = 32;
		nvm->address_bits = 16;
		break;
	case e1000_nvm_override_spi_small:
		nvm->page_size    = 8;
		nvm->address_bits = 8;
		break;
	default:
		nvm->page_size    = eecd & E1000_EECD_ADDR_BITS ? 32 : 8;
		nvm->address_bits = eecd & E1000_EECD_ADDR_BITS ? 16 : 8;
		break;
	}

	nvm->type = e1000_nvm_eeprom_spi;

	size = (u16)((eecd & E1000_EECD_SIZE_EX_MASK) >>
		     E1000_EECD_SIZE_EX_SHIFT);

	size += NVM_WORD_SIZE_BASE_SHIFT;

	if ((hw->mac.type == e1000_82576) && (size > 15)) {
		pr_notice("The NVM size is not valid, defaulting to 32K\n");
		size = 15;
	}
	nvm->word_size = 1 << size;
	if (nvm->word_size == (1 << 15))
		nvm->page_size = 128;

	
	nvm->ops.acquire = igb_acquire_nvm_82575;
	if (nvm->word_size < (1 << 15))
		nvm->ops.read = igb_read_nvm_eerd;
	else
		nvm->ops.read = igb_read_nvm_spi;

	nvm->ops.release = igb_release_nvm_82575;
	switch (hw->mac.type) {
	case e1000_82580:
		nvm->ops.validate = igb_validate_nvm_checksum_82580;
		nvm->ops.update = igb_update_nvm_checksum_82580;
		break;
	case e1000_i350:
		nvm->ops.validate = igb_validate_nvm_checksum_i350;
		nvm->ops.update = igb_update_nvm_checksum_i350;
		break;
	default:
		nvm->ops.validate = igb_validate_nvm_checksum;
		nvm->ops.update = igb_update_nvm_checksum;
	}
	nvm->ops.write = igb_write_nvm_spi;

	
	switch (mac->type) {
	case e1000_82576:
	case e1000_i350:
		igb_init_mbx_params_pf(hw);
		break;
	default:
		break;
	}

	
	if (phy->media_type != e1000_media_type_copper) {
		phy->type = e1000_phy_none;
		return 0;
	}

	phy->autoneg_mask        = AUTONEG_ADVERTISE_SPEED_DEFAULT;
	phy->reset_delay_us      = 100;

	ctrl_ext = rd32(E1000_CTRL_EXT);

	
	if (igb_sgmii_active_82575(hw)) {
		phy->ops.reset      = igb_phy_hw_reset_sgmii_82575;
		ctrl_ext |= E1000_CTRL_I2C_ENA;
	} else {
		phy->ops.reset      = igb_phy_hw_reset;
		ctrl_ext &= ~E1000_CTRL_I2C_ENA;
	}

	wr32(E1000_CTRL_EXT, ctrl_ext);
	igb_reset_mdicnfg_82580(hw);

	if (igb_sgmii_active_82575(hw) && !igb_sgmii_uses_mdio_82575(hw)) {
		phy->ops.read_reg   = igb_read_phy_reg_sgmii_82575;
		phy->ops.write_reg  = igb_write_phy_reg_sgmii_82575;
	} else if (hw->mac.type >= e1000_82580) {
		phy->ops.read_reg   = igb_read_phy_reg_82580;
		phy->ops.write_reg  = igb_write_phy_reg_82580;
	} else {
		phy->ops.read_reg   = igb_read_phy_reg_igp;
		phy->ops.write_reg  = igb_write_phy_reg_igp;
	}

	
	hw->bus.func = (rd32(E1000_STATUS) & E1000_STATUS_FUNC_MASK) >>
	               E1000_STATUS_FUNC_SHIFT;

	
	ret_val = igb_get_phy_id_82575(hw);
	if (ret_val)
		return ret_val;

	
	switch (phy->id) {
	case I347AT4_E_PHY_ID:
	case M88E1112_E_PHY_ID:
	case M88E1111_I_PHY_ID:
		phy->type                   = e1000_phy_m88;
		phy->ops.get_phy_info       = igb_get_phy_info_m88;

		if (phy->id == I347AT4_E_PHY_ID ||
		    phy->id == M88E1112_E_PHY_ID)
			phy->ops.get_cable_length = igb_get_cable_length_m88_gen2;
		else
			phy->ops.get_cable_length = igb_get_cable_length_m88;

		phy->ops.force_speed_duplex = igb_phy_force_speed_duplex_m88;
		break;
	case IGP03E1000_E_PHY_ID:
		phy->type                   = e1000_phy_igp_3;
		phy->ops.get_phy_info       = igb_get_phy_info_igp;
		phy->ops.get_cable_length   = igb_get_cable_length_igp_2;
		phy->ops.force_speed_duplex = igb_phy_force_speed_duplex_igp;
		phy->ops.set_d0_lplu_state  = igb_set_d0_lplu_state_82575;
		phy->ops.set_d3_lplu_state  = igb_set_d3_lplu_state;
		break;
	case I82580_I_PHY_ID:
	case I350_I_PHY_ID:
		phy->type                   = e1000_phy_82580;
		phy->ops.force_speed_duplex = igb_phy_force_speed_duplex_82580;
		phy->ops.get_cable_length   = igb_get_cable_length_82580;
		phy->ops.get_phy_info       = igb_get_phy_info_82580;
		break;
	default:
		return -E1000_ERR_PHY;
	}

	return 0;
}

static s32 igb_acquire_phy_82575(struct e1000_hw *hw)
{
	u16 mask = E1000_SWFW_PHY0_SM;

	if (hw->bus.func == E1000_FUNC_1)
		mask = E1000_SWFW_PHY1_SM;
	else if (hw->bus.func == E1000_FUNC_2)
		mask = E1000_SWFW_PHY2_SM;
	else if (hw->bus.func == E1000_FUNC_3)
		mask = E1000_SWFW_PHY3_SM;

	return igb_acquire_swfw_sync_82575(hw, mask);
}

static void igb_release_phy_82575(struct e1000_hw *hw)
{
	u16 mask = E1000_SWFW_PHY0_SM;

	if (hw->bus.func == E1000_FUNC_1)
		mask = E1000_SWFW_PHY1_SM;
	else if (hw->bus.func == E1000_FUNC_2)
		mask = E1000_SWFW_PHY2_SM;
	else if (hw->bus.func == E1000_FUNC_3)
		mask = E1000_SWFW_PHY3_SM;

	igb_release_swfw_sync_82575(hw, mask);
}

static s32 igb_read_phy_reg_sgmii_82575(struct e1000_hw *hw, u32 offset,
					  u16 *data)
{
	s32 ret_val = -E1000_ERR_PARAM;

	if (offset > E1000_MAX_SGMII_PHY_REG_ADDR) {
		hw_dbg("PHY Address %u is out of range\n", offset);
		goto out;
	}

	ret_val = hw->phy.ops.acquire(hw);
	if (ret_val)
		goto out;

	ret_val = igb_read_phy_reg_i2c(hw, offset, data);

	hw->phy.ops.release(hw);

out:
	return ret_val;
}

static s32 igb_write_phy_reg_sgmii_82575(struct e1000_hw *hw, u32 offset,
					   u16 data)
{
	s32 ret_val = -E1000_ERR_PARAM;


	if (offset > E1000_MAX_SGMII_PHY_REG_ADDR) {
		hw_dbg("PHY Address %d is out of range\n", offset);
		goto out;
	}

	ret_val = hw->phy.ops.acquire(hw);
	if (ret_val)
		goto out;

	ret_val = igb_write_phy_reg_i2c(hw, offset, data);

	hw->phy.ops.release(hw);

out:
	return ret_val;
}

static s32 igb_get_phy_id_82575(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32  ret_val = 0;
	u16 phy_id;
	u32 ctrl_ext;
	u32 mdic;

	if (!(igb_sgmii_active_82575(hw))) {
		phy->addr = 1;
		ret_val = igb_get_phy_id(hw);
		goto out;
	}

	if (igb_sgmii_uses_mdio_82575(hw)) {
		switch (hw->mac.type) {
		case e1000_82575:
		case e1000_82576:
			mdic = rd32(E1000_MDIC);
			mdic &= E1000_MDIC_PHY_MASK;
			phy->addr = mdic >> E1000_MDIC_PHY_SHIFT;
			break;
		case e1000_82580:
		case e1000_i350:
			mdic = rd32(E1000_MDICNFG);
			mdic &= E1000_MDICNFG_PHY_MASK;
			phy->addr = mdic >> E1000_MDICNFG_PHY_SHIFT;
			break;
		default:
			ret_val = -E1000_ERR_PHY;
			goto out;
			break;
		}
		ret_val = igb_get_phy_id(hw);
		goto out;
	}

	
	ctrl_ext = rd32(E1000_CTRL_EXT);
	wr32(E1000_CTRL_EXT, ctrl_ext & ~E1000_CTRL_EXT_SDP3_DATA);
	wrfl();
	msleep(300);

	for (phy->addr = 1; phy->addr < 8; phy->addr++) {
		ret_val = igb_read_phy_reg_sgmii_82575(hw, PHY_ID1, &phy_id);
		if (ret_val == 0) {
			hw_dbg("Vendor ID 0x%08X read at address %u\n",
			       phy_id, phy->addr);
			if (phy_id == M88_VENDOR)
				break;
		} else {
			hw_dbg("PHY address %u was unreadable\n", phy->addr);
		}
	}

	
	if (phy->addr == 8) {
		phy->addr = 0;
		ret_val = -E1000_ERR_PHY;
		goto out;
	} else {
		ret_val = igb_get_phy_id(hw);
	}

	
	wr32(E1000_CTRL_EXT, ctrl_ext);

out:
	return ret_val;
}

static s32 igb_phy_hw_reset_sgmii_82575(struct e1000_hw *hw)
{
	s32 ret_val;


	hw_dbg("Soft resetting SGMII attached PHY...\n");

	ret_val = hw->phy.ops.write_reg(hw, 0x1B, 0x8084);
	if (ret_val)
		goto out;

	ret_val = igb_phy_sw_reset(hw);

out:
	return ret_val;
}

static s32 igb_set_d0_lplu_state_82575(struct e1000_hw *hw, bool active)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data;

	ret_val = phy->ops.read_reg(hw, IGP02E1000_PHY_POWER_MGMT, &data);
	if (ret_val)
		goto out;

	if (active) {
		data |= IGP02E1000_PM_D0_LPLU;
		ret_val = phy->ops.write_reg(hw, IGP02E1000_PHY_POWER_MGMT,
						 data);
		if (ret_val)
			goto out;

		
		ret_val = phy->ops.read_reg(hw, IGP01E1000_PHY_PORT_CONFIG,
						&data);
		data &= ~IGP01E1000_PSCFR_SMART_SPEED;
		ret_val = phy->ops.write_reg(hw, IGP01E1000_PHY_PORT_CONFIG,
						 data);
		if (ret_val)
			goto out;
	} else {
		data &= ~IGP02E1000_PM_D0_LPLU;
		ret_val = phy->ops.write_reg(hw, IGP02E1000_PHY_POWER_MGMT,
						 data);
		if (phy->smart_speed == e1000_smart_speed_on) {
			ret_val = phy->ops.read_reg(hw,
					IGP01E1000_PHY_PORT_CONFIG, &data);
			if (ret_val)
				goto out;

			data |= IGP01E1000_PSCFR_SMART_SPEED;
			ret_val = phy->ops.write_reg(hw,
					IGP01E1000_PHY_PORT_CONFIG, data);
			if (ret_val)
				goto out;
		} else if (phy->smart_speed == e1000_smart_speed_off) {
			ret_val = phy->ops.read_reg(hw,
					IGP01E1000_PHY_PORT_CONFIG, &data);
			if (ret_val)
				goto out;

			data &= ~IGP01E1000_PSCFR_SMART_SPEED;
			ret_val = phy->ops.write_reg(hw,
					IGP01E1000_PHY_PORT_CONFIG, data);
			if (ret_val)
				goto out;
		}
	}

out:
	return ret_val;
}

static s32 igb_acquire_nvm_82575(struct e1000_hw *hw)
{
	s32 ret_val;

	ret_val = igb_acquire_swfw_sync_82575(hw, E1000_SWFW_EEP_SM);
	if (ret_val)
		goto out;

	ret_val = igb_acquire_nvm(hw);

	if (ret_val)
		igb_release_swfw_sync_82575(hw, E1000_SWFW_EEP_SM);

out:
	return ret_val;
}

static void igb_release_nvm_82575(struct e1000_hw *hw)
{
	igb_release_nvm(hw);
	igb_release_swfw_sync_82575(hw, E1000_SWFW_EEP_SM);
}

static s32 igb_acquire_swfw_sync_82575(struct e1000_hw *hw, u16 mask)
{
	u32 swfw_sync;
	u32 swmask = mask;
	u32 fwmask = mask << 16;
	s32 ret_val = 0;
	s32 i = 0, timeout = 200; 

	while (i < timeout) {
		if (igb_get_hw_semaphore(hw)) {
			ret_val = -E1000_ERR_SWFW_SYNC;
			goto out;
		}

		swfw_sync = rd32(E1000_SW_FW_SYNC);
		if (!(swfw_sync & (fwmask | swmask)))
			break;

		igb_put_hw_semaphore(hw);
		mdelay(5);
		i++;
	}

	if (i == timeout) {
		hw_dbg("Driver can't access resource, SW_FW_SYNC timeout.\n");
		ret_val = -E1000_ERR_SWFW_SYNC;
		goto out;
	}

	swfw_sync |= swmask;
	wr32(E1000_SW_FW_SYNC, swfw_sync);

	igb_put_hw_semaphore(hw);

out:
	return ret_val;
}

static void igb_release_swfw_sync_82575(struct e1000_hw *hw, u16 mask)
{
	u32 swfw_sync;

	while (igb_get_hw_semaphore(hw) != 0);
	

	swfw_sync = rd32(E1000_SW_FW_SYNC);
	swfw_sync &= ~mask;
	wr32(E1000_SW_FW_SYNC, swfw_sync);

	igb_put_hw_semaphore(hw);
}

static s32 igb_get_cfg_done_82575(struct e1000_hw *hw)
{
	s32 timeout = PHY_CFG_TIMEOUT;
	s32 ret_val = 0;
	u32 mask = E1000_NVM_CFG_DONE_PORT_0;

	if (hw->bus.func == 1)
		mask = E1000_NVM_CFG_DONE_PORT_1;
	else if (hw->bus.func == E1000_FUNC_2)
		mask = E1000_NVM_CFG_DONE_PORT_2;
	else if (hw->bus.func == E1000_FUNC_3)
		mask = E1000_NVM_CFG_DONE_PORT_3;

	while (timeout) {
		if (rd32(E1000_EEMNGCTL) & mask)
			break;
		msleep(1);
		timeout--;
	}
	if (!timeout)
		hw_dbg("MNG configuration cycle has not completed.\n");

	
	if (((rd32(E1000_EECD) & E1000_EECD_PRES) == 0) &&
	    (hw->phy.type == e1000_phy_igp_3))
		igb_phy_init_script_igp3(hw);

	return ret_val;
}

static s32 igb_check_for_link_82575(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 speed, duplex;

	if (hw->phy.media_type != e1000_media_type_copper) {
		ret_val = igb_get_pcs_speed_and_duplex_82575(hw, &speed,
		                                             &duplex);
		hw->mac.get_link_status = !hw->mac.serdes_has_link;
	} else {
		ret_val = igb_check_for_copper_link(hw);
	}

	return ret_val;
}

void igb_power_up_serdes_link_82575(struct e1000_hw *hw)
{
	u32 reg;


	if ((hw->phy.media_type != e1000_media_type_internal_serdes) &&
	    !igb_sgmii_active_82575(hw))
		return;

	
	reg = rd32(E1000_PCS_CFG0);
	reg |= E1000_PCS_CFG_PCS_EN;
	wr32(E1000_PCS_CFG0, reg);

	
	reg = rd32(E1000_CTRL_EXT);
	reg &= ~E1000_CTRL_EXT_SDP3_DATA;
	wr32(E1000_CTRL_EXT, reg);

	
	wrfl();
	msleep(1);
}

static s32 igb_get_pcs_speed_and_duplex_82575(struct e1000_hw *hw, u16 *speed,
						u16 *duplex)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 pcs;

	
	mac->serdes_has_link = false;
	*speed = 0;
	*duplex = 0;

	pcs = rd32(E1000_PCS_LSTAT);

	if ((pcs & E1000_PCS_LSTS_LINK_OK) && (pcs & E1000_PCS_LSTS_SYNK_OK)) {
		mac->serdes_has_link = true;

		
		if (pcs & E1000_PCS_LSTS_SPEED_1000) {
			*speed = SPEED_1000;
		} else if (pcs & E1000_PCS_LSTS_SPEED_100) {
			*speed = SPEED_100;
		} else {
			*speed = SPEED_10;
		}

		
		if (pcs & E1000_PCS_LSTS_DUPLEX_FULL) {
			*duplex = FULL_DUPLEX;
		} else {
			*duplex = HALF_DUPLEX;
		}
	}

	return 0;
}

void igb_shutdown_serdes_link_82575(struct e1000_hw *hw)
{
	u32 reg;

	if (hw->phy.media_type != e1000_media_type_internal_serdes &&
	    igb_sgmii_active_82575(hw))
		return;

	if (!igb_enable_mng_pass_thru(hw)) {
		
		reg = rd32(E1000_PCS_CFG0);
		reg &= ~E1000_PCS_CFG_PCS_EN;
		wr32(E1000_PCS_CFG0, reg);

		
		reg = rd32(E1000_CTRL_EXT);
		reg |= E1000_CTRL_EXT_SDP3_DATA;
		wr32(E1000_CTRL_EXT, reg);

		
		wrfl();
		msleep(1);
	}
}

static s32 igb_reset_hw_82575(struct e1000_hw *hw)
{
	u32 ctrl, icr;
	s32 ret_val;

	ret_val = igb_disable_pcie_master(hw);
	if (ret_val)
		hw_dbg("PCI-E Master disable polling has failed.\n");

	
	ret_val = igb_set_pcie_completion_timeout(hw);
	if (ret_val) {
		hw_dbg("PCI-E Set completion timeout has failed.\n");
	}

	hw_dbg("Masking off all interrupts\n");
	wr32(E1000_IMC, 0xffffffff);

	wr32(E1000_RCTL, 0);
	wr32(E1000_TCTL, E1000_TCTL_PSP);
	wrfl();

	msleep(10);

	ctrl = rd32(E1000_CTRL);

	hw_dbg("Issuing a global reset to MAC\n");
	wr32(E1000_CTRL, ctrl | E1000_CTRL_RST);

	ret_val = igb_get_auto_rd_done(hw);
	if (ret_val) {
		hw_dbg("Auto Read Done did not complete\n");
	}

	
	if ((rd32(E1000_EECD) & E1000_EECD_PRES) == 0)
		igb_reset_init_script_82575(hw);

	
	wr32(E1000_IMC, 0xffffffff);
	icr = rd32(E1000_ICR);

	
	ret_val = igb_check_alt_mac_addr(hw);

	return ret_val;
}

static s32 igb_init_hw_82575(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	s32 ret_val;
	u16 i, rar_count = mac->rar_entry_count;

	
	ret_val = igb_id_led_init(hw);
	if (ret_val) {
		hw_dbg("Error initializing identification LED\n");
		
	}

	
	hw_dbg("Initializing the IEEE VLAN\n");
	if (hw->mac.type == e1000_i350)
		igb_clear_vfta_i350(hw);
	else
		igb_clear_vfta(hw);

	
	igb_init_rx_addrs(hw, rar_count);

	
	hw_dbg("Zeroing the MTA\n");
	for (i = 0; i < mac->mta_reg_count; i++)
		array_wr32(E1000_MTA, i, 0);

	
	hw_dbg("Zeroing the UTA\n");
	for (i = 0; i < mac->uta_reg_count; i++)
		array_wr32(E1000_UTA, i, 0);

	
	ret_val = igb_setup_link(hw);

	igb_clear_hw_cntrs_82575(hw);

	return ret_val;
}

static s32 igb_setup_copper_link_82575(struct e1000_hw *hw)
{
	u32 ctrl;
	s32  ret_val;

	ctrl = rd32(E1000_CTRL);
	ctrl |= E1000_CTRL_SLU;
	ctrl &= ~(E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
	wr32(E1000_CTRL, ctrl);

	ret_val = igb_setup_serdes_link_82575(hw);
	if (ret_val)
		goto out;

	if (igb_sgmii_active_82575(hw) && !hw->phy.reset_disable) {
		
		msleep(300);

		ret_val = hw->phy.ops.reset(hw);
		if (ret_val) {
			hw_dbg("Error resetting the PHY.\n");
			goto out;
		}
	}
	switch (hw->phy.type) {
	case e1000_phy_m88:
		if (hw->phy.id == I347AT4_E_PHY_ID ||
		    hw->phy.id == M88E1112_E_PHY_ID)
			ret_val = igb_copper_link_setup_m88_gen2(hw);
		else
			ret_val = igb_copper_link_setup_m88(hw);
		break;
	case e1000_phy_igp_3:
		ret_val = igb_copper_link_setup_igp(hw);
		break;
	case e1000_phy_82580:
		ret_val = igb_copper_link_setup_82580(hw);
		break;
	default:
		ret_val = -E1000_ERR_PHY;
		break;
	}

	if (ret_val)
		goto out;

	ret_val = igb_setup_copper_link(hw);
out:
	return ret_val;
}

static s32 igb_setup_serdes_link_82575(struct e1000_hw *hw)
{
	u32 ctrl_ext, ctrl_reg, reg;
	bool pcs_autoneg;
	s32 ret_val = E1000_SUCCESS;
	u16 data;

	if ((hw->phy.media_type != e1000_media_type_internal_serdes) &&
	    !igb_sgmii_active_82575(hw))
		return ret_val;


	wr32(E1000_SCTL, E1000_SCTL_DISABLE_SERDES_LOOPBACK);

	
	ctrl_ext = rd32(E1000_CTRL_EXT);
	ctrl_ext &= ~E1000_CTRL_EXT_SDP3_DATA;
	wr32(E1000_CTRL_EXT, ctrl_ext);

	ctrl_reg = rd32(E1000_CTRL);
	ctrl_reg |= E1000_CTRL_SLU;

	if (hw->mac.type == e1000_82575 || hw->mac.type == e1000_82576) {
		
		ctrl_reg |= E1000_CTRL_SWDPIN0 | E1000_CTRL_SWDPIN1;

		
		reg = rd32(E1000_CONNSW);
		reg |= E1000_CONNSW_ENRGSRC;
		wr32(E1000_CONNSW, reg);
	}

	reg = rd32(E1000_PCS_LCTL);

	
	pcs_autoneg = hw->mac.autoneg;

	switch (ctrl_ext & E1000_CTRL_EXT_LINK_MODE_MASK) {
	case E1000_CTRL_EXT_LINK_MODE_SGMII:
		
		pcs_autoneg = true;
		
		reg &= ~(E1000_PCS_LCTL_AN_TIMEOUT);
		break;
	case E1000_CTRL_EXT_LINK_MODE_1000BASE_KX:
		
		pcs_autoneg = false;
	default:
		if (hw->mac.type == e1000_82575 ||
		    hw->mac.type == e1000_82576) {
			ret_val = hw->nvm.ops.read(hw, NVM_COMPAT, 1, &data);
			if (ret_val) {
				printk(KERN_DEBUG "NVM Read Error\n\n");
				return ret_val;
			}

			if (data & E1000_EEPROM_PCS_AUTONEG_DISABLE_BIT)
				pcs_autoneg = false;
		}

		ctrl_reg |= E1000_CTRL_SPD_1000 | E1000_CTRL_FRCSPD |
		            E1000_CTRL_FD | E1000_CTRL_FRCDPX;

		
		reg |= E1000_PCS_LCTL_FSV_1000 | E1000_PCS_LCTL_FDV_FULL;
		break;
	}

	wr32(E1000_CTRL, ctrl_reg);

	reg &= ~(E1000_PCS_LCTL_AN_ENABLE | E1000_PCS_LCTL_FLV_LINK_UP |
		E1000_PCS_LCTL_FSD | E1000_PCS_LCTL_FORCE_LINK);

	/*
	 * We force flow control to prevent the CTRL register values from being
	 * overwritten by the autonegotiated flow control values
	 */
	reg |= E1000_PCS_LCTL_FORCE_FCTRL;

	if (pcs_autoneg) {
		
		reg |= E1000_PCS_LCTL_AN_ENABLE | 
		       E1000_PCS_LCTL_AN_RESTART; 
		hw_dbg("Configuring Autoneg:PCS_LCTL=0x%08X\n", reg);
	} else {
		
		reg |= E1000_PCS_LCTL_FSD;        

		hw_dbg("Configuring Forced Link:PCS_LCTL=0x%08X\n", reg);
	}

	wr32(E1000_PCS_LCTL, reg);

	if (!igb_sgmii_active_82575(hw))
		igb_force_mac_fc(hw);

	return ret_val;
}

static bool igb_sgmii_active_82575(struct e1000_hw *hw)
{
	struct e1000_dev_spec_82575 *dev_spec = &hw->dev_spec._82575;
	return dev_spec->sgmii_active;
}

static s32 igb_reset_init_script_82575(struct e1000_hw *hw)
{
	if (hw->mac.type == e1000_82575) {
		hw_dbg("Running reset init script for 82575\n");
		
		igb_write_8bit_ctrl_reg(hw, E1000_SCTL, 0x00, 0x0C);
		igb_write_8bit_ctrl_reg(hw, E1000_SCTL, 0x01, 0x78);
		igb_write_8bit_ctrl_reg(hw, E1000_SCTL, 0x1B, 0x23);
		igb_write_8bit_ctrl_reg(hw, E1000_SCTL, 0x23, 0x15);

		
		igb_write_8bit_ctrl_reg(hw, E1000_CCMCTL, 0x14, 0x00);
		igb_write_8bit_ctrl_reg(hw, E1000_CCMCTL, 0x10, 0x00);

		
		igb_write_8bit_ctrl_reg(hw, E1000_GIOCTL, 0x00, 0xEC);
		igb_write_8bit_ctrl_reg(hw, E1000_GIOCTL, 0x61, 0xDF);
		igb_write_8bit_ctrl_reg(hw, E1000_GIOCTL, 0x34, 0x05);
		igb_write_8bit_ctrl_reg(hw, E1000_GIOCTL, 0x2F, 0x81);

		
		igb_write_8bit_ctrl_reg(hw, E1000_SCCTL, 0x02, 0x47);
		igb_write_8bit_ctrl_reg(hw, E1000_SCCTL, 0x14, 0x00);
		igb_write_8bit_ctrl_reg(hw, E1000_SCCTL, 0x10, 0x00);
	}

	return 0;
}

static s32 igb_read_mac_addr_82575(struct e1000_hw *hw)
{
	s32 ret_val = 0;

	ret_val = igb_check_alt_mac_addr(hw);
	if (ret_val)
		goto out;

	ret_val = igb_read_mac_addr(hw);

out:
	return ret_val;
}

void igb_power_down_phy_copper_82575(struct e1000_hw *hw)
{
	
	if (!(igb_enable_mng_pass_thru(hw) || igb_check_reset_block(hw)))
		igb_power_down_phy_copper(hw);
}

static void igb_clear_hw_cntrs_82575(struct e1000_hw *hw)
{
	igb_clear_hw_cntrs_base(hw);

	rd32(E1000_PRC64);
	rd32(E1000_PRC127);
	rd32(E1000_PRC255);
	rd32(E1000_PRC511);
	rd32(E1000_PRC1023);
	rd32(E1000_PRC1522);
	rd32(E1000_PTC64);
	rd32(E1000_PTC127);
	rd32(E1000_PTC255);
	rd32(E1000_PTC511);
	rd32(E1000_PTC1023);
	rd32(E1000_PTC1522);

	rd32(E1000_ALGNERRC);
	rd32(E1000_RXERRC);
	rd32(E1000_TNCRS);
	rd32(E1000_CEXTERR);
	rd32(E1000_TSCTC);
	rd32(E1000_TSCTFC);

	rd32(E1000_MGTPRC);
	rd32(E1000_MGTPDC);
	rd32(E1000_MGTPTC);

	rd32(E1000_IAC);
	rd32(E1000_ICRXOC);

	rd32(E1000_ICRXPTC);
	rd32(E1000_ICRXATC);
	rd32(E1000_ICTXPTC);
	rd32(E1000_ICTXATC);
	rd32(E1000_ICTXQEC);
	rd32(E1000_ICTXQMTC);
	rd32(E1000_ICRXDMTC);

	rd32(E1000_CBTMPC);
	rd32(E1000_HTDPMC);
	rd32(E1000_CBRMPC);
	rd32(E1000_RPTHC);
	rd32(E1000_HGPTC);
	rd32(E1000_HTCBDPC);
	rd32(E1000_HGORCL);
	rd32(E1000_HGORCH);
	rd32(E1000_HGOTCL);
	rd32(E1000_HGOTCH);
	rd32(E1000_LENERRS);

	
	if (hw->phy.media_type == e1000_media_type_internal_serdes ||
	    igb_sgmii_active_82575(hw))
		rd32(E1000_SCVPC);
}

void igb_rx_fifo_flush_82575(struct e1000_hw *hw)
{
	u32 rctl, rlpml, rxdctl[4], rfctl, temp_rctl, rx_enabled;
	int i, ms_wait;

	if (hw->mac.type != e1000_82575 ||
	    !(rd32(E1000_MANC) & E1000_MANC_RCV_TCO_EN))
		return;

	
	for (i = 0; i < 4; i++) {
		rxdctl[i] = rd32(E1000_RXDCTL(i));
		wr32(E1000_RXDCTL(i),
		     rxdctl[i] & ~E1000_RXDCTL_QUEUE_ENABLE);
	}
	
	for (ms_wait = 0; ms_wait < 10; ms_wait++) {
		msleep(1);
		rx_enabled = 0;
		for (i = 0; i < 4; i++)
			rx_enabled |= rd32(E1000_RXDCTL(i));
		if (!(rx_enabled & E1000_RXDCTL_QUEUE_ENABLE))
			break;
	}

	if (ms_wait == 10)
		hw_dbg("Queue disable timed out after 10ms\n");

	rfctl = rd32(E1000_RFCTL);
	wr32(E1000_RFCTL, rfctl & ~E1000_RFCTL_LEF);

	rlpml = rd32(E1000_RLPML);
	wr32(E1000_RLPML, 0);

	rctl = rd32(E1000_RCTL);
	temp_rctl = rctl & ~(E1000_RCTL_EN | E1000_RCTL_SBP);
	temp_rctl |= E1000_RCTL_LPE;

	wr32(E1000_RCTL, temp_rctl);
	wr32(E1000_RCTL, temp_rctl | E1000_RCTL_EN);
	wrfl();
	msleep(2);

	for (i = 0; i < 4; i++)
		wr32(E1000_RXDCTL(i), rxdctl[i]);
	wr32(E1000_RCTL, rctl);
	wrfl();

	wr32(E1000_RLPML, rlpml);
	wr32(E1000_RFCTL, rfctl);

	
	rd32(E1000_ROC);
	rd32(E1000_RNBC);
	rd32(E1000_MPC);
}

static s32 igb_set_pcie_completion_timeout(struct e1000_hw *hw)
{
	u32 gcr = rd32(E1000_GCR);
	s32 ret_val = 0;
	u16 pcie_devctl2;

	
	if (gcr & E1000_GCR_CMPL_TMOUT_MASK)
		goto out;

	if (!(gcr & E1000_GCR_CAP_VER2)) {
		gcr |= E1000_GCR_CMPL_TMOUT_10ms;
		goto out;
	}

	ret_val = igb_read_pcie_cap_reg(hw, PCIE_DEVICE_CONTROL2,
	                                &pcie_devctl2);
	if (ret_val)
		goto out;

	pcie_devctl2 |= PCIE_DEVICE_CONTROL2_16ms;

	ret_val = igb_write_pcie_cap_reg(hw, PCIE_DEVICE_CONTROL2,
	                                 &pcie_devctl2);
out:
	
	gcr &= ~E1000_GCR_CMPL_TMOUT_RESEND;

	wr32(E1000_GCR, gcr);
	return ret_val;
}

void igb_vmdq_set_anti_spoofing_pf(struct e1000_hw *hw, bool enable, int pf)
{
	u32 dtxswc;

	switch (hw->mac.type) {
	case e1000_82576:
	case e1000_i350:
		dtxswc = rd32(E1000_DTXSWC);
		if (enable) {
			dtxswc |= (E1000_DTXSWC_MAC_SPOOF_MASK |
				   E1000_DTXSWC_VLAN_SPOOF_MASK);
			dtxswc ^= (1 << pf | 1 << (pf + MAX_NUM_VFS));
		} else {
			dtxswc &= ~(E1000_DTXSWC_MAC_SPOOF_MASK |
				    E1000_DTXSWC_VLAN_SPOOF_MASK);
		}
		wr32(E1000_DTXSWC, dtxswc);
		break;
	default:
		break;
	}
}

void igb_vmdq_set_loopback_pf(struct e1000_hw *hw, bool enable)
{
	u32 dtxswc;

	switch (hw->mac.type) {
	case e1000_82576:
		dtxswc = rd32(E1000_DTXSWC);
		if (enable)
			dtxswc |= E1000_DTXSWC_VMDQ_LOOPBACK_EN;
		else
			dtxswc &= ~E1000_DTXSWC_VMDQ_LOOPBACK_EN;
		wr32(E1000_DTXSWC, dtxswc);
		break;
	case e1000_i350:
		dtxswc = rd32(E1000_TXSWC);
		if (enable)
			dtxswc |= E1000_DTXSWC_VMDQ_LOOPBACK_EN;
		else
			dtxswc &= ~E1000_DTXSWC_VMDQ_LOOPBACK_EN;
		wr32(E1000_TXSWC, dtxswc);
		break;
	default:
		
		break;
	}


}

void igb_vmdq_set_replication_pf(struct e1000_hw *hw, bool enable)
{
	u32 vt_ctl = rd32(E1000_VT_CTL);

	if (enable)
		vt_ctl |= E1000_VT_CTL_VM_REPL_EN;
	else
		vt_ctl &= ~E1000_VT_CTL_VM_REPL_EN;

	wr32(E1000_VT_CTL, vt_ctl);
}

static s32 igb_read_phy_reg_82580(struct e1000_hw *hw, u32 offset, u16 *data)
{
	s32 ret_val;


	ret_val = hw->phy.ops.acquire(hw);
	if (ret_val)
		goto out;

	ret_val = igb_read_phy_reg_mdic(hw, offset, data);

	hw->phy.ops.release(hw);

out:
	return ret_val;
}

static s32 igb_write_phy_reg_82580(struct e1000_hw *hw, u32 offset, u16 data)
{
	s32 ret_val;


	ret_val = hw->phy.ops.acquire(hw);
	if (ret_val)
		goto out;

	ret_val = igb_write_phy_reg_mdic(hw, offset, data);

	hw->phy.ops.release(hw);

out:
	return ret_val;
}

static s32 igb_reset_mdicnfg_82580(struct e1000_hw *hw)
{
	s32 ret_val = 0;
	u32 mdicnfg;
	u16 nvm_data = 0;

	if (hw->mac.type != e1000_82580)
		goto out;
	if (!igb_sgmii_active_82575(hw))
		goto out;

	ret_val = hw->nvm.ops.read(hw, NVM_INIT_CONTROL3_PORT_A +
				   NVM_82580_LAN_FUNC_OFFSET(hw->bus.func), 1,
				   &nvm_data);
	if (ret_val) {
		hw_dbg("NVM Read Error\n");
		goto out;
	}

	mdicnfg = rd32(E1000_MDICNFG);
	if (nvm_data & NVM_WORD24_EXT_MDIO)
		mdicnfg |= E1000_MDICNFG_EXT_MDIO;
	if (nvm_data & NVM_WORD24_COM_MDIO)
		mdicnfg |= E1000_MDICNFG_COM_MDIO;
	wr32(E1000_MDICNFG, mdicnfg);
out:
	return ret_val;
}

static s32 igb_reset_hw_82580(struct e1000_hw *hw)
{
	s32 ret_val = 0;
	
	u16 swmbsw_mask = E1000_SW_SYNCH_MB;
	u32 ctrl, icr;
	bool global_device_reset = hw->dev_spec._82575.global_device_reset;


	hw->dev_spec._82575.global_device_reset = false;

	
	ctrl = rd32(E1000_CTRL);

	ret_val = igb_disable_pcie_master(hw);
	if (ret_val)
		hw_dbg("PCI-E Master disable polling has failed.\n");

	hw_dbg("Masking off all interrupts\n");
	wr32(E1000_IMC, 0xffffffff);
	wr32(E1000_RCTL, 0);
	wr32(E1000_TCTL, E1000_TCTL_PSP);
	wrfl();

	msleep(10);

	
	if (global_device_reset &&
		igb_acquire_swfw_sync_82575(hw, swmbsw_mask))
			global_device_reset = false;

	if (global_device_reset &&
		!(rd32(E1000_STATUS) & E1000_STAT_DEV_RST_SET))
		ctrl |= E1000_CTRL_DEV_RST;
	else
		ctrl |= E1000_CTRL_RST;

	wr32(E1000_CTRL, ctrl);
	wrfl();

	
	if (global_device_reset)
		msleep(5);

	ret_val = igb_get_auto_rd_done(hw);
	if (ret_val) {
		hw_dbg("Auto Read Done did not complete\n");
	}

	
	if ((rd32(E1000_EECD) & E1000_EECD_PRES) == 0)
		igb_reset_init_script_82575(hw);

	
	wr32(E1000_STATUS, E1000_STAT_DEV_RST_SET);

	
	wr32(E1000_IMC, 0xffffffff);
	icr = rd32(E1000_ICR);

	ret_val = igb_reset_mdicnfg_82580(hw);
	if (ret_val)
		hw_dbg("Could not reset MDICNFG based on EEPROM\n");

	
	ret_val = igb_check_alt_mac_addr(hw);

	
	if (global_device_reset)
		igb_release_swfw_sync_82575(hw, swmbsw_mask);

	return ret_val;
}

u16 igb_rxpbs_adjust_82580(u32 data)
{
	u16 ret_val = 0;

	if (data < E1000_82580_RXPBS_TABLE_SIZE)
		ret_val = e1000_82580_rxpbs_table[data];

	return ret_val;
}

static s32 igb_validate_nvm_checksum_with_offset(struct e1000_hw *hw,
						 u16 offset)
{
	s32 ret_val = 0;
	u16 checksum = 0;
	u16 i, nvm_data;

	for (i = offset; i < ((NVM_CHECKSUM_REG + offset) + 1); i++) {
		ret_val = hw->nvm.ops.read(hw, i, 1, &nvm_data);
		if (ret_val) {
			hw_dbg("NVM Read Error\n");
			goto out;
		}
		checksum += nvm_data;
	}

	if (checksum != (u16) NVM_SUM) {
		hw_dbg("NVM Checksum Invalid\n");
		ret_val = -E1000_ERR_NVM;
		goto out;
	}

out:
	return ret_val;
}

static s32 igb_update_nvm_checksum_with_offset(struct e1000_hw *hw, u16 offset)
{
	s32 ret_val;
	u16 checksum = 0;
	u16 i, nvm_data;

	for (i = offset; i < (NVM_CHECKSUM_REG + offset); i++) {
		ret_val = hw->nvm.ops.read(hw, i, 1, &nvm_data);
		if (ret_val) {
			hw_dbg("NVM Read Error while updating checksum.\n");
			goto out;
		}
		checksum += nvm_data;
	}
	checksum = (u16) NVM_SUM - checksum;
	ret_val = hw->nvm.ops.write(hw, (NVM_CHECKSUM_REG + offset), 1,
				&checksum);
	if (ret_val)
		hw_dbg("NVM Write Error while updating checksum.\n");

out:
	return ret_val;
}

static s32 igb_validate_nvm_checksum_82580(struct e1000_hw *hw)
{
	s32 ret_val = 0;
	u16 eeprom_regions_count = 1;
	u16 j, nvm_data;
	u16 nvm_offset;

	ret_val = hw->nvm.ops.read(hw, NVM_COMPATIBILITY_REG_3, 1, &nvm_data);
	if (ret_val) {
		hw_dbg("NVM Read Error\n");
		goto out;
	}

	if (nvm_data & NVM_COMPATIBILITY_BIT_MASK) {
		eeprom_regions_count = 4;
	}

	for (j = 0; j < eeprom_regions_count; j++) {
		nvm_offset = NVM_82580_LAN_FUNC_OFFSET(j);
		ret_val = igb_validate_nvm_checksum_with_offset(hw,
								nvm_offset);
		if (ret_val != 0)
			goto out;
	}

out:
	return ret_val;
}

static s32 igb_update_nvm_checksum_82580(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 j, nvm_data;
	u16 nvm_offset;

	ret_val = hw->nvm.ops.read(hw, NVM_COMPATIBILITY_REG_3, 1, &nvm_data);
	if (ret_val) {
		hw_dbg("NVM Read Error while updating checksum"
			" compatibility bit.\n");
		goto out;
	}

	if ((nvm_data & NVM_COMPATIBILITY_BIT_MASK) == 0) {
		
		nvm_data = nvm_data | NVM_COMPATIBILITY_BIT_MASK;
		ret_val = hw->nvm.ops.write(hw, NVM_COMPATIBILITY_REG_3, 1,
					&nvm_data);
		if (ret_val) {
			hw_dbg("NVM Write Error while updating checksum"
				" compatibility bit.\n");
			goto out;
		}
	}

	for (j = 0; j < 4; j++) {
		nvm_offset = NVM_82580_LAN_FUNC_OFFSET(j);
		ret_val = igb_update_nvm_checksum_with_offset(hw, nvm_offset);
		if (ret_val)
			goto out;
	}

out:
	return ret_val;
}

static s32 igb_validate_nvm_checksum_i350(struct e1000_hw *hw)
{
	s32 ret_val = 0;
	u16 j;
	u16 nvm_offset;

	for (j = 0; j < 4; j++) {
		nvm_offset = NVM_82580_LAN_FUNC_OFFSET(j);
		ret_val = igb_validate_nvm_checksum_with_offset(hw,
								nvm_offset);
		if (ret_val != 0)
			goto out;
	}

out:
	return ret_val;
}

static s32 igb_update_nvm_checksum_i350(struct e1000_hw *hw)
{
	s32 ret_val = 0;
	u16 j;
	u16 nvm_offset;

	for (j = 0; j < 4; j++) {
		nvm_offset = NVM_82580_LAN_FUNC_OFFSET(j);
		ret_val = igb_update_nvm_checksum_with_offset(hw, nvm_offset);
		if (ret_val != 0)
			goto out;
	}

out:
	return ret_val;
}

s32 igb_set_eee_i350(struct e1000_hw *hw)
{
	s32 ret_val = 0;
	u32 ipcnfg, eeer, ctrl_ext;

	ctrl_ext = rd32(E1000_CTRL_EXT);
	if ((hw->mac.type != e1000_i350) ||
	    (ctrl_ext & E1000_CTRL_EXT_LINK_MODE_MASK))
		goto out;
	ipcnfg = rd32(E1000_IPCNFG);
	eeer = rd32(E1000_EEER);

	
	if (!(hw->dev_spec._82575.eee_disable)) {
		ipcnfg |= (E1000_IPCNFG_EEE_1G_AN |
			E1000_IPCNFG_EEE_100M_AN);
		eeer |= (E1000_EEER_TX_LPI_EN |
			E1000_EEER_RX_LPI_EN |
			E1000_EEER_LPI_FC);

	} else {
		ipcnfg &= ~(E1000_IPCNFG_EEE_1G_AN |
			E1000_IPCNFG_EEE_100M_AN);
		eeer &= ~(E1000_EEER_TX_LPI_EN |
			E1000_EEER_RX_LPI_EN |
			E1000_EEER_LPI_FC);
	}
	wr32(E1000_IPCNFG, ipcnfg);
	wr32(E1000_EEER, eeer);
out:

	return ret_val;
}

static struct e1000_mac_operations e1000_mac_ops_82575 = {
	.init_hw              = igb_init_hw_82575,
	.check_for_link       = igb_check_for_link_82575,
	.rar_set              = igb_rar_set,
	.read_mac_addr        = igb_read_mac_addr_82575,
	.get_speed_and_duplex = igb_get_speed_and_duplex_copper,
};

static struct e1000_phy_operations e1000_phy_ops_82575 = {
	.acquire              = igb_acquire_phy_82575,
	.get_cfg_done         = igb_get_cfg_done_82575,
	.release              = igb_release_phy_82575,
};

static struct e1000_nvm_operations e1000_nvm_ops_82575 = {
	.acquire              = igb_acquire_nvm_82575,
	.read                 = igb_read_nvm_eerd,
	.release              = igb_release_nvm_82575,
	.write                = igb_write_nvm_spi,
};

const struct e1000_info e1000_82575_info = {
	.get_invariants = igb_get_invariants_82575,
	.mac_ops = &e1000_mac_ops_82575,
	.phy_ops = &e1000_phy_ops_82575,
	.nvm_ops = &e1000_nvm_ops_82575,
};

