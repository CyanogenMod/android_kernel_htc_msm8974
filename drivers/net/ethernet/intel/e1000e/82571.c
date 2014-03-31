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

#define ID_LED_RESERVED_F746 0xF746
#define ID_LED_DEFAULT_82573 ((ID_LED_DEF1_DEF2 << 12) | \
			      (ID_LED_OFF1_ON2  <<  8) | \
			      (ID_LED_DEF1_DEF2 <<  4) | \
			      (ID_LED_DEF1_DEF2))

#define E1000_GCR_L1_ACT_WITHOUT_L0S_RX 0x08000000
#define AN_RETRY_COUNT          5 
#define E1000_BASE1000T_STATUS          10
#define E1000_IDLE_ERROR_COUNT_MASK     0xFF
#define E1000_RECEIVE_ERROR_COUNTER     21
#define E1000_RECEIVE_ERROR_MAX         0xFFFF

#define E1000_NVM_INIT_CTRL2_MNGM 0x6000 

static s32 e1000_get_phy_id_82571(struct e1000_hw *hw);
static s32 e1000_setup_copper_link_82571(struct e1000_hw *hw);
static s32 e1000_setup_fiber_serdes_link_82571(struct e1000_hw *hw);
static s32 e1000_check_for_serdes_link_82571(struct e1000_hw *hw);
static s32 e1000_write_nvm_eewr_82571(struct e1000_hw *hw, u16 offset,
				      u16 words, u16 *data);
static s32 e1000_fix_nvm_checksum_82571(struct e1000_hw *hw);
static void e1000_initialize_hw_bits_82571(struct e1000_hw *hw);
static s32 e1000_setup_link_82571(struct e1000_hw *hw);
static void e1000_clear_hw_cntrs_82571(struct e1000_hw *hw);
static void e1000_clear_vfta_82571(struct e1000_hw *hw);
static bool e1000_check_mng_mode_82574(struct e1000_hw *hw);
static s32 e1000_led_on_82574(struct e1000_hw *hw);
static void e1000_put_hw_semaphore_82571(struct e1000_hw *hw);
static void e1000_power_down_phy_copper_82571(struct e1000_hw *hw);
static void e1000_put_hw_semaphore_82573(struct e1000_hw *hw);
static s32 e1000_get_hw_semaphore_82574(struct e1000_hw *hw);
static void e1000_put_hw_semaphore_82574(struct e1000_hw *hw);
static s32 e1000_set_d0_lplu_state_82574(struct e1000_hw *hw, bool active);
static s32 e1000_set_d3_lplu_state_82574(struct e1000_hw *hw, bool active);

static s32 e1000_init_phy_params_82571(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;

	if (hw->phy.media_type != e1000_media_type_copper) {
		phy->type = e1000_phy_none;
		return 0;
	}

	phy->addr			 = 1;
	phy->autoneg_mask		 = AUTONEG_ADVERTISE_SPEED_DEFAULT;
	phy->reset_delay_us		 = 100;

	phy->ops.power_up		 = e1000_power_up_phy_copper;
	phy->ops.power_down		 = e1000_power_down_phy_copper_82571;

	switch (hw->mac.type) {
	case e1000_82571:
	case e1000_82572:
		phy->type		 = e1000_phy_igp_2;
		break;
	case e1000_82573:
		phy->type		 = e1000_phy_m88;
		break;
	case e1000_82574:
	case e1000_82583:
		phy->type		 = e1000_phy_bm;
		phy->ops.acquire = e1000_get_hw_semaphore_82574;
		phy->ops.release = e1000_put_hw_semaphore_82574;
		phy->ops.set_d0_lplu_state = e1000_set_d0_lplu_state_82574;
		phy->ops.set_d3_lplu_state = e1000_set_d3_lplu_state_82574;
		break;
	default:
		return -E1000_ERR_PHY;
		break;
	}

	
	ret_val = e1000_get_phy_id_82571(hw);
	if (ret_val) {
		e_dbg("Error getting PHY ID\n");
		return ret_val;
	}

	
	switch (hw->mac.type) {
	case e1000_82571:
	case e1000_82572:
		if (phy->id != IGP01E1000_I_PHY_ID)
			ret_val = -E1000_ERR_PHY;
		break;
	case e1000_82573:
		if (phy->id != M88E1111_I_PHY_ID)
			ret_val = -E1000_ERR_PHY;
		break;
	case e1000_82574:
	case e1000_82583:
		if (phy->id != BME1000_E_PHY_ID_R2)
			ret_val = -E1000_ERR_PHY;
		break;
	default:
		ret_val = -E1000_ERR_PHY;
		break;
	}

	if (ret_val)
		e_dbg("PHY ID unknown: type = 0x%08x\n", phy->id);

	return ret_val;
}

static s32 e1000_init_nvm_params_82571(struct e1000_hw *hw)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 eecd = er32(EECD);
	u16 size;

	nvm->opcode_bits = 8;
	nvm->delay_usec = 1;
	switch (nvm->override) {
	case e1000_nvm_override_spi_large:
		nvm->page_size = 32;
		nvm->address_bits = 16;
		break;
	case e1000_nvm_override_spi_small:
		nvm->page_size = 8;
		nvm->address_bits = 8;
		break;
	default:
		nvm->page_size = eecd & E1000_EECD_ADDR_BITS ? 32 : 8;
		nvm->address_bits = eecd & E1000_EECD_ADDR_BITS ? 16 : 8;
		break;
	}

	switch (hw->mac.type) {
	case e1000_82573:
	case e1000_82574:
	case e1000_82583:
		if (((eecd >> 15) & 0x3) == 0x3) {
			nvm->type = e1000_nvm_flash_hw;
			nvm->word_size = 2048;
			eecd &= ~E1000_EECD_AUPDEN;
			ew32(EECD, eecd);
			break;
		}
		
	default:
		nvm->type = e1000_nvm_eeprom_spi;
		size = (u16)((eecd & E1000_EECD_SIZE_EX_MASK) >>
				  E1000_EECD_SIZE_EX_SHIFT);
		size += NVM_WORD_SIZE_BASE_SHIFT;

		
		if (size > 14)
			size = 14;
		nvm->word_size	= 1 << size;
		break;
	}

	
	switch (hw->mac.type) {
	case e1000_82574:
	case e1000_82583:
		nvm->ops.acquire = e1000_get_hw_semaphore_82574;
		nvm->ops.release = e1000_put_hw_semaphore_82574;
		break;
	default:
		break;
	}

	return 0;
}

static s32 e1000_init_mac_params_82571(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 swsm = 0;
	u32 swsm2 = 0;
	bool force_clear_smbi = false;

	
	switch (hw->adapter->pdev->device) {
	case E1000_DEV_ID_82571EB_FIBER:
	case E1000_DEV_ID_82572EI_FIBER:
	case E1000_DEV_ID_82571EB_QUAD_FIBER:
		hw->phy.media_type = e1000_media_type_fiber;
		mac->ops.setup_physical_interface =
		    e1000_setup_fiber_serdes_link_82571;
		mac->ops.check_for_link = e1000e_check_for_fiber_link;
		mac->ops.get_link_up_info =
		    e1000e_get_speed_and_duplex_fiber_serdes;
		break;
	case E1000_DEV_ID_82571EB_SERDES:
	case E1000_DEV_ID_82571EB_SERDES_DUAL:
	case E1000_DEV_ID_82571EB_SERDES_QUAD:
	case E1000_DEV_ID_82572EI_SERDES:
		hw->phy.media_type = e1000_media_type_internal_serdes;
		mac->ops.setup_physical_interface =
		    e1000_setup_fiber_serdes_link_82571;
		mac->ops.check_for_link = e1000_check_for_serdes_link_82571;
		mac->ops.get_link_up_info =
		    e1000e_get_speed_and_duplex_fiber_serdes;
		break;
	default:
		hw->phy.media_type = e1000_media_type_copper;
		mac->ops.setup_physical_interface =
		    e1000_setup_copper_link_82571;
		mac->ops.check_for_link = e1000e_check_for_copper_link;
		mac->ops.get_link_up_info = e1000e_get_speed_and_duplex_copper;
		break;
	}

	
	mac->mta_reg_count = 128;
	
	mac->rar_entry_count = E1000_RAR_ENTRIES;
	
	mac->adaptive_ifs = true;

	
	switch (hw->mac.type) {
	case e1000_82573:
		mac->ops.set_lan_id = e1000_set_lan_id_single_port;
		mac->ops.check_mng_mode = e1000e_check_mng_mode_generic;
		mac->ops.led_on = e1000e_led_on_generic;
		mac->ops.blink_led = e1000e_blink_led_generic;

		
		mac->has_fwsm = true;
		mac->arc_subsystem_valid =
			(er32(FWSM) & E1000_FWSM_MODE_MASK)
			? true : false;
		break;
	case e1000_82574:
	case e1000_82583:
		mac->ops.set_lan_id = e1000_set_lan_id_single_port;
		mac->ops.check_mng_mode = e1000_check_mng_mode_82574;
		mac->ops.led_on = e1000_led_on_82574;
		break;
	default:
		mac->ops.check_mng_mode = e1000e_check_mng_mode_generic;
		mac->ops.led_on = e1000e_led_on_generic;
		mac->ops.blink_led = e1000e_blink_led_generic;

		
		mac->has_fwsm = true;
		break;
	}

	switch (hw->mac.type) {
	case e1000_82571:
	case e1000_82572:
		swsm2 = er32(SWSM2);

		if (!(swsm2 & E1000_SWSM2_LOCK)) {
			
			ew32(SWSM2, swsm2 | E1000_SWSM2_LOCK);
			force_clear_smbi = true;
		} else {
			force_clear_smbi = false;
		}
		break;
	default:
		force_clear_smbi = true;
		break;
	}

	if (force_clear_smbi) {
		
		swsm = er32(SWSM);
		if (swsm & E1000_SWSM_SMBI) {
			e_dbg("Please update your 82571 Bootagent\n");
		}
		ew32(SWSM, swsm & ~E1000_SWSM_SMBI);
	}

	 hw->dev_spec.e82571.smb_counter = 0;

	return 0;
}

static s32 e1000_get_variants_82571(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	static int global_quad_port_a; 
	struct pci_dev *pdev = adapter->pdev;
	int is_port_b = er32(STATUS) & E1000_STATUS_FUNC_1;
	s32 rc;

	rc = e1000_init_mac_params_82571(hw);
	if (rc)
		return rc;

	rc = e1000_init_nvm_params_82571(hw);
	if (rc)
		return rc;

	rc = e1000_init_phy_params_82571(hw);
	if (rc)
		return rc;

	
	switch (pdev->device) {
	case E1000_DEV_ID_82571EB_QUAD_COPPER:
	case E1000_DEV_ID_82571EB_QUAD_FIBER:
	case E1000_DEV_ID_82571EB_QUAD_COPPER_LP:
	case E1000_DEV_ID_82571PT_QUAD_COPPER:
		adapter->flags |= FLAG_IS_QUAD_PORT;
		
		if (global_quad_port_a == 0)
			adapter->flags |= FLAG_IS_QUAD_PORT_A;
		
		global_quad_port_a++;
		if (global_quad_port_a == 4)
			global_quad_port_a = 0;
		break;
	default:
		break;
	}

	switch (adapter->hw.mac.type) {
	case e1000_82571:
		
		if (((pdev->device == E1000_DEV_ID_82571EB_FIBER) ||
		     (pdev->device == E1000_DEV_ID_82571EB_SERDES) ||
		     (pdev->device == E1000_DEV_ID_82571EB_COPPER)) &&
		    (is_port_b))
			adapter->flags &= ~FLAG_HAS_WOL;
		
		if (adapter->flags & FLAG_IS_QUAD_PORT &&
		    (!(adapter->flags & FLAG_IS_QUAD_PORT_A)))
			adapter->flags &= ~FLAG_HAS_WOL;
		
		if (pdev->device == E1000_DEV_ID_82571EB_SERDES_QUAD)
			adapter->flags &= ~FLAG_HAS_WOL;
		break;
	case e1000_82573:
		if (pdev->device == E1000_DEV_ID_82573L) {
			adapter->flags |= FLAG_HAS_JUMBO_FRAMES;
			adapter->max_hw_frame_size = DEFAULT_JUMBO;
		}
		break;
	default:
		break;
	}

	return 0;
}

static s32 e1000_get_phy_id_82571(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 phy_id = 0;

	switch (hw->mac.type) {
	case e1000_82571:
	case e1000_82572:
		phy->id = IGP01E1000_I_PHY_ID;
		break;
	case e1000_82573:
		return e1000e_get_phy_id(hw);
		break;
	case e1000_82574:
	case e1000_82583:
		ret_val = e1e_rphy(hw, PHY_ID1, &phy_id);
		if (ret_val)
			return ret_val;

		phy->id = (u32)(phy_id << 16);
		udelay(20);
		ret_val = e1e_rphy(hw, PHY_ID2, &phy_id);
		if (ret_val)
			return ret_val;

		phy->id |= (u32)(phy_id);
		phy->revision = (u32)(phy_id & ~PHY_REVISION_MASK);
		break;
	default:
		return -E1000_ERR_PHY;
		break;
	}

	return 0;
}

static s32 e1000_get_hw_semaphore_82571(struct e1000_hw *hw)
{
	u32 swsm;
	s32 sw_timeout = hw->nvm.word_size + 1;
	s32 fw_timeout = hw->nvm.word_size + 1;
	s32 i = 0;

	if (hw->dev_spec.e82571.smb_counter > 2)
		sw_timeout = 1;

	
	while (i < sw_timeout) {
		swsm = er32(SWSM);
		if (!(swsm & E1000_SWSM_SMBI))
			break;

		udelay(50);
		i++;
	}

	if (i == sw_timeout) {
		e_dbg("Driver can't access device - SMBI bit is set.\n");
		hw->dev_spec.e82571.smb_counter++;
	}
	
	for (i = 0; i < fw_timeout; i++) {
		swsm = er32(SWSM);
		ew32(SWSM, swsm | E1000_SWSM_SWESMBI);

		
		if (er32(SWSM) & E1000_SWSM_SWESMBI)
			break;

		udelay(50);
	}

	if (i == fw_timeout) {
		
		e1000_put_hw_semaphore_82571(hw);
		e_dbg("Driver can't access the NVM\n");
		return -E1000_ERR_NVM;
	}

	return 0;
}

static void e1000_put_hw_semaphore_82571(struct e1000_hw *hw)
{
	u32 swsm;

	swsm = er32(SWSM);
	swsm &= ~(E1000_SWSM_SMBI | E1000_SWSM_SWESMBI);
	ew32(SWSM, swsm);
}
static s32 e1000_get_hw_semaphore_82573(struct e1000_hw *hw)
{
	u32 extcnf_ctrl;
	s32 i = 0;

	extcnf_ctrl = er32(EXTCNF_CTRL);
	extcnf_ctrl |= E1000_EXTCNF_CTRL_MDIO_SW_OWNERSHIP;
	do {
		ew32(EXTCNF_CTRL, extcnf_ctrl);
		extcnf_ctrl = er32(EXTCNF_CTRL);

		if (extcnf_ctrl & E1000_EXTCNF_CTRL_MDIO_SW_OWNERSHIP)
			break;

		extcnf_ctrl |= E1000_EXTCNF_CTRL_MDIO_SW_OWNERSHIP;

		usleep_range(2000, 4000);
		i++;
	} while (i < MDIO_OWNERSHIP_TIMEOUT);

	if (i == MDIO_OWNERSHIP_TIMEOUT) {
		
		e1000_put_hw_semaphore_82573(hw);
		e_dbg("Driver can't access the PHY\n");
		return -E1000_ERR_PHY;
	}

	return 0;
}

static void e1000_put_hw_semaphore_82573(struct e1000_hw *hw)
{
	u32 extcnf_ctrl;

	extcnf_ctrl = er32(EXTCNF_CTRL);
	extcnf_ctrl &= ~E1000_EXTCNF_CTRL_MDIO_SW_OWNERSHIP;
	ew32(EXTCNF_CTRL, extcnf_ctrl);
}

static DEFINE_MUTEX(swflag_mutex);

static s32 e1000_get_hw_semaphore_82574(struct e1000_hw *hw)
{
	s32 ret_val;

	mutex_lock(&swflag_mutex);
	ret_val = e1000_get_hw_semaphore_82573(hw);
	if (ret_val)
		mutex_unlock(&swflag_mutex);
	return ret_val;
}

static void e1000_put_hw_semaphore_82574(struct e1000_hw *hw)
{
	e1000_put_hw_semaphore_82573(hw);
	mutex_unlock(&swflag_mutex);
}

static s32 e1000_set_d0_lplu_state_82574(struct e1000_hw *hw, bool active)
{
	u16 data = er32(POEMB);

	if (active)
		data |= E1000_PHY_CTRL_D0A_LPLU;
	else
		data &= ~E1000_PHY_CTRL_D0A_LPLU;

	ew32(POEMB, data);
	return 0;
}

static s32 e1000_set_d3_lplu_state_82574(struct e1000_hw *hw, bool active)
{
	u16 data = er32(POEMB);

	if (!active) {
		data &= ~E1000_PHY_CTRL_NOND0A_LPLU;
	} else if ((hw->phy.autoneg_advertised == E1000_ALL_SPEED_DUPLEX) ||
		   (hw->phy.autoneg_advertised == E1000_ALL_NOT_GIG) ||
		   (hw->phy.autoneg_advertised == E1000_ALL_10_SPEED)) {
		data |= E1000_PHY_CTRL_NOND0A_LPLU;
	}

	ew32(POEMB, data);
	return 0;
}

static s32 e1000_acquire_nvm_82571(struct e1000_hw *hw)
{
	s32 ret_val;

	ret_val = e1000_get_hw_semaphore_82571(hw);
	if (ret_val)
		return ret_val;

	switch (hw->mac.type) {
	case e1000_82573:
		break;
	default:
		ret_val = e1000e_acquire_nvm(hw);
		break;
	}

	if (ret_val)
		e1000_put_hw_semaphore_82571(hw);

	return ret_val;
}

static void e1000_release_nvm_82571(struct e1000_hw *hw)
{
	e1000e_release_nvm(hw);
	e1000_put_hw_semaphore_82571(hw);
}

/**
 *  e1000_write_nvm_82571 - Write to EEPROM using appropriate interface
 *  @hw: pointer to the HW structure
 *  @offset: offset within the EEPROM to be written to
 *  @words: number of words to write
 *  @data: 16 bit word(s) to be written to the EEPROM
 *
 *  For non-82573 silicon, write data to EEPROM at offset using SPI interface.
 *
 *  If e1000e_update_nvm_checksum is not called after this function, the
 *  EEPROM will most likely contain an invalid checksum.
 **/
static s32 e1000_write_nvm_82571(struct e1000_hw *hw, u16 offset, u16 words,
				 u16 *data)
{
	s32 ret_val;

	switch (hw->mac.type) {
	case e1000_82573:
	case e1000_82574:
	case e1000_82583:
		ret_val = e1000_write_nvm_eewr_82571(hw, offset, words, data);
		break;
	case e1000_82571:
	case e1000_82572:
		ret_val = e1000e_write_nvm_spi(hw, offset, words, data);
		break;
	default:
		ret_val = -E1000_ERR_NVM;
		break;
	}

	return ret_val;
}

static s32 e1000_update_nvm_checksum_82571(struct e1000_hw *hw)
{
	u32 eecd;
	s32 ret_val;
	u16 i;

	ret_val = e1000e_update_nvm_checksum_generic(hw);
	if (ret_val)
		return ret_val;

	if (hw->nvm.type != e1000_nvm_flash_hw)
		return 0;

	
	for (i = 0; i < E1000_FLASH_UPDATES; i++) {
		usleep_range(1000, 2000);
		if ((er32(EECD) & E1000_EECD_FLUPD) == 0)
			break;
	}

	if (i == E1000_FLASH_UPDATES)
		return -E1000_ERR_NVM;

	
	if ((er32(FLOP) & 0xFF00) == E1000_STM_OPCODE) {
		ew32(HICR, E1000_HICR_FW_RESET_ENABLE);
		e1e_flush();
		ew32(HICR, E1000_HICR_FW_RESET);
	}

	
	eecd = er32(EECD) | E1000_EECD_FLUPD;
	ew32(EECD, eecd);

	for (i = 0; i < E1000_FLASH_UPDATES; i++) {
		usleep_range(1000, 2000);
		if ((er32(EECD) & E1000_EECD_FLUPD) == 0)
			break;
	}

	if (i == E1000_FLASH_UPDATES)
		return -E1000_ERR_NVM;

	return 0;
}

static s32 e1000_validate_nvm_checksum_82571(struct e1000_hw *hw)
{
	if (hw->nvm.type == e1000_nvm_flash_hw)
		e1000_fix_nvm_checksum_82571(hw);

	return e1000e_validate_nvm_checksum_generic(hw);
}

/**
 *  e1000_write_nvm_eewr_82571 - Write to EEPROM for 82573 silicon
 *  @hw: pointer to the HW structure
 *  @offset: offset within the EEPROM to be written to
 *  @words: number of words to write
 *  @data: 16 bit word(s) to be written to the EEPROM
 *
 *  After checking for invalid values, poll the EEPROM to ensure the previous
 *  command has completed before trying to write the next word.  After write
 *  poll for completion.
 *
 *  If e1000e_update_nvm_checksum is not called after this function, the
 *  EEPROM will most likely contain an invalid checksum.
 **/
static s32 e1000_write_nvm_eewr_82571(struct e1000_hw *hw, u16 offset,
				      u16 words, u16 *data)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 i, eewr = 0;
	s32 ret_val = 0;

	if ((offset >= nvm->word_size) || (words > (nvm->word_size - offset)) ||
	    (words == 0)) {
		e_dbg("nvm parameter(s) out of bounds\n");
		return -E1000_ERR_NVM;
	}

	for (i = 0; i < words; i++) {
		eewr = (data[i] << E1000_NVM_RW_REG_DATA) |
		       ((offset+i) << E1000_NVM_RW_ADDR_SHIFT) |
		       E1000_NVM_RW_REG_START;

		ret_val = e1000e_poll_eerd_eewr_done(hw, E1000_NVM_POLL_WRITE);
		if (ret_val)
			break;

		ew32(EEWR, eewr);

		ret_val = e1000e_poll_eerd_eewr_done(hw, E1000_NVM_POLL_WRITE);
		if (ret_val)
			break;
	}

	return ret_val;
}

static s32 e1000_get_cfg_done_82571(struct e1000_hw *hw)
{
	s32 timeout = PHY_CFG_TIMEOUT;

	while (timeout) {
		if (er32(EEMNGCTL) &
		    E1000_NVM_CFG_DONE_PORT_0)
			break;
		usleep_range(1000, 2000);
		timeout--;
	}
	if (!timeout) {
		e_dbg("MNG configuration cycle has not completed.\n");
		return -E1000_ERR_RESET;
	}

	return 0;
}

static s32 e1000_set_d0_lplu_state_82571(struct e1000_hw *hw, bool active)
{
	struct e1000_phy_info *phy = &hw->phy;
	s32 ret_val;
	u16 data;

	ret_val = e1e_rphy(hw, IGP02E1000_PHY_POWER_MGMT, &data);
	if (ret_val)
		return ret_val;

	if (active) {
		data |= IGP02E1000_PM_D0_LPLU;
		ret_val = e1e_wphy(hw, IGP02E1000_PHY_POWER_MGMT, data);
		if (ret_val)
			return ret_val;

		
		ret_val = e1e_rphy(hw, IGP01E1000_PHY_PORT_CONFIG, &data);
		data &= ~IGP01E1000_PSCFR_SMART_SPEED;
		ret_val = e1e_wphy(hw, IGP01E1000_PHY_PORT_CONFIG, data);
		if (ret_val)
			return ret_val;
	} else {
		data &= ~IGP02E1000_PM_D0_LPLU;
		ret_val = e1e_wphy(hw, IGP02E1000_PHY_POWER_MGMT, data);
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
	}

	return 0;
}

static s32 e1000_reset_hw_82571(struct e1000_hw *hw)
{
	u32 ctrl, ctrl_ext;
	s32 ret_val;

	ret_val = e1000e_disable_pcie_master(hw);
	if (ret_val)
		e_dbg("PCI-E Master disable polling has failed.\n");

	e_dbg("Masking off all interrupts\n");
	ew32(IMC, 0xffffffff);

	ew32(RCTL, 0);
	ew32(TCTL, E1000_TCTL_PSP);
	e1e_flush();

	usleep_range(10000, 20000);

	switch (hw->mac.type) {
	case e1000_82573:
		ret_val = e1000_get_hw_semaphore_82573(hw);
		break;
	case e1000_82574:
	case e1000_82583:
		ret_val = e1000_get_hw_semaphore_82574(hw);
		break;
	default:
		break;
	}
	if (ret_val)
		e_dbg("Cannot acquire MDIO ownership\n");

	ctrl = er32(CTRL);

	e_dbg("Issuing a global reset to MAC\n");
	ew32(CTRL, ctrl | E1000_CTRL_RST);

	
	switch (hw->mac.type) {
	case e1000_82574:
	case e1000_82583:
		e1000_put_hw_semaphore_82574(hw);
		break;
	default:
		break;
	}

	if (hw->nvm.type == e1000_nvm_flash_hw) {
		udelay(10);
		ctrl_ext = er32(CTRL_EXT);
		ctrl_ext |= E1000_CTRL_EXT_EE_RST;
		ew32(CTRL_EXT, ctrl_ext);
		e1e_flush();
	}

	ret_val = e1000e_get_auto_rd_done(hw);
	if (ret_val)
		
		return ret_val;


	switch (hw->mac.type) {
	case e1000_82573:
	case e1000_82574:
	case e1000_82583:
		msleep(25);
		break;
	default:
		break;
	}

	
	ew32(IMC, 0xffffffff);
	er32(ICR);

	if (hw->mac.type == e1000_82571) {
		
		ret_val = e1000_check_alt_mac_addr_generic(hw);
		if (ret_val)
			return ret_val;

		e1000e_set_laa_state_82571(hw, true);
	}

	
	if (hw->phy.media_type == e1000_media_type_internal_serdes)
		hw->mac.serdes_link_state = e1000_serdes_link_down;

	return 0;
}

static s32 e1000_init_hw_82571(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 reg_data;
	s32 ret_val;
	u16 i, rar_count = mac->rar_entry_count;

	e1000_initialize_hw_bits_82571(hw);

	
	ret_val = mac->ops.id_led_init(hw);
	if (ret_val)
		e_dbg("Error initializing identification LED\n");
		

	
	e_dbg("Initializing the IEEE VLAN\n");
	mac->ops.clear_vfta(hw);

	
	if (e1000e_get_laa_state_82571(hw))
		rar_count--;
	e1000e_init_rx_addrs(hw, rar_count);

	
	e_dbg("Zeroing the MTA\n");
	for (i = 0; i < mac->mta_reg_count; i++)
		E1000_WRITE_REG_ARRAY(hw, E1000_MTA, i, 0);

	
	ret_val = mac->ops.setup_link(hw);

	
	reg_data = er32(TXDCTL(0));
	reg_data = (reg_data & ~E1000_TXDCTL_WTHRESH) |
		   E1000_TXDCTL_FULL_TX_DESC_WB |
		   E1000_TXDCTL_COUNT_DESC;
	ew32(TXDCTL(0), reg_data);

	
	switch (mac->type) {
	case e1000_82573:
		e1000e_enable_tx_pkt_filtering(hw);
		
	case e1000_82574:
	case e1000_82583:
		reg_data = er32(GCR);
		reg_data |= E1000_GCR_L1_ACT_WITHOUT_L0S_RX;
		ew32(GCR, reg_data);
		break;
	default:
		reg_data = er32(TXDCTL(1));
		reg_data = (reg_data & ~E1000_TXDCTL_WTHRESH) |
			   E1000_TXDCTL_FULL_TX_DESC_WB |
			   E1000_TXDCTL_COUNT_DESC;
		ew32(TXDCTL(1), reg_data);
		break;
	}

	e1000_clear_hw_cntrs_82571(hw);

	return ret_val;
}

static void e1000_initialize_hw_bits_82571(struct e1000_hw *hw)
{
	u32 reg;

	
	reg = er32(TXDCTL(0));
	reg |= (1 << 22);
	ew32(TXDCTL(0), reg);

	
	reg = er32(TXDCTL(1));
	reg |= (1 << 22);
	ew32(TXDCTL(1), reg);

	
	reg = er32(TARC(0));
	reg &= ~(0xF << 27); 
	switch (hw->mac.type) {
	case e1000_82571:
	case e1000_82572:
		reg |= (1 << 23) | (1 << 24) | (1 << 25) | (1 << 26);
		break;
	case e1000_82574:
	case e1000_82583:
		reg |= (1 << 26);
		break;
	default:
		break;
	}
	ew32(TARC(0), reg);

	
	reg = er32(TARC(1));
	switch (hw->mac.type) {
	case e1000_82571:
	case e1000_82572:
		reg &= ~((1 << 29) | (1 << 30));
		reg |= (1 << 22) | (1 << 24) | (1 << 25) | (1 << 26);
		if (er32(TCTL) & E1000_TCTL_MULR)
			reg &= ~(1 << 28);
		else
			reg |= (1 << 28);
		ew32(TARC(1), reg);
		break;
	default:
		break;
	}

	
	switch (hw->mac.type) {
	case e1000_82573:
	case e1000_82574:
	case e1000_82583:
		reg = er32(CTRL);
		reg &= ~(1 << 29);
		ew32(CTRL, reg);
		break;
	default:
		break;
	}

	
	switch (hw->mac.type) {
	case e1000_82573:
	case e1000_82574:
	case e1000_82583:
		reg = er32(CTRL_EXT);
		reg &= ~(1 << 23);
		reg |= (1 << 22);
		ew32(CTRL_EXT, reg);
		break;
	default:
		break;
	}

	if (hw->mac.type == e1000_82571) {
		reg = er32(PBA_ECC);
		reg |= E1000_PBA_ECC_CORR_EN;
		ew32(PBA_ECC, reg);
	}

	if ((hw->mac.type == e1000_82571) || (hw->mac.type == e1000_82572)) {
		reg = er32(CTRL_EXT);
		reg &= ~E1000_CTRL_EXT_DMA_DYN_CLK_EN;
		ew32(CTRL_EXT, reg);
	}

	
	switch (hw->mac.type) {
	case e1000_82574:
	case e1000_82583:
		reg = er32(GCR);
		reg |= (1 << 22);
		ew32(GCR, reg);

		reg = er32(GCR2);
		reg |= 1;
		ew32(GCR2, reg);
		break;
	default:
		break;
	}
}

static void e1000_clear_vfta_82571(struct e1000_hw *hw)
{
	u32 offset;
	u32 vfta_value = 0;
	u32 vfta_offset = 0;
	u32 vfta_bit_in_reg = 0;

	switch (hw->mac.type) {
	case e1000_82573:
	case e1000_82574:
	case e1000_82583:
		if (hw->mng_cookie.vlan_id != 0) {
			vfta_offset = (hw->mng_cookie.vlan_id >>
				       E1000_VFTA_ENTRY_SHIFT) &
				      E1000_VFTA_ENTRY_MASK;
			vfta_bit_in_reg = 1 << (hw->mng_cookie.vlan_id &
					       E1000_VFTA_ENTRY_BIT_SHIFT_MASK);
		}
		break;
	default:
		break;
	}
	for (offset = 0; offset < E1000_VLAN_FILTER_TBL_SIZE; offset++) {
		vfta_value = (offset == vfta_offset) ? vfta_bit_in_reg : 0;
		E1000_WRITE_REG_ARRAY(hw, E1000_VFTA, offset, vfta_value);
		e1e_flush();
	}
}

static bool e1000_check_mng_mode_82574(struct e1000_hw *hw)
{
	u16 data;

	e1000_read_nvm(hw, NVM_INIT_CONTROL2_REG, 1, &data);
	return (data & E1000_NVM_INIT_CTRL2_MNGM) != 0;
}

static s32 e1000_led_on_82574(struct e1000_hw *hw)
{
	u32 ctrl;
	u32 i;

	ctrl = hw->mac.ledctl_mode2;
	if (!(E1000_STATUS_LU & er32(STATUS))) {
		for (i = 0; i < 4; i++)
			if (((hw->mac.ledctl_mode2 >> (i * 8)) & 0xFF) ==
			    E1000_LEDCTL_MODE_LED_ON)
				ctrl |= (E1000_LEDCTL_LED0_IVRT << (i * 8));
	}
	ew32(LEDCTL, ctrl);

	return 0;
}

bool e1000_check_phy_82574(struct e1000_hw *hw)
{
	u16 status_1kbt = 0;
	u16 receive_errors = 0;
	s32 ret_val = 0;

	ret_val = e1e_rphy(hw, E1000_RECEIVE_ERROR_COUNTER, &receive_errors);
	if (ret_val)
		return false;
	if (receive_errors == E1000_RECEIVE_ERROR_MAX)  {
		ret_val = e1e_rphy(hw, E1000_BASE1000T_STATUS, &status_1kbt);
		if (ret_val)
			return false;
		if ((status_1kbt & E1000_IDLE_ERROR_COUNT_MASK) ==
		    E1000_IDLE_ERROR_COUNT_MASK)
			return true;
	}

	return false;
}

static s32 e1000_setup_link_82571(struct e1000_hw *hw)
{
	switch (hw->mac.type) {
	case e1000_82573:
	case e1000_82574:
	case e1000_82583:
		if (hw->fc.requested_mode == e1000_fc_default)
			hw->fc.requested_mode = e1000_fc_full;
		break;
	default:
		break;
	}

	return e1000e_setup_link_generic(hw);
}

static s32 e1000_setup_copper_link_82571(struct e1000_hw *hw)
{
	u32 ctrl;
	s32 ret_val;

	ctrl = er32(CTRL);
	ctrl |= E1000_CTRL_SLU;
	ctrl &= ~(E1000_CTRL_FRCSPD | E1000_CTRL_FRCDPX);
	ew32(CTRL, ctrl);

	switch (hw->phy.type) {
	case e1000_phy_m88:
	case e1000_phy_bm:
		ret_val = e1000e_copper_link_setup_m88(hw);
		break;
	case e1000_phy_igp_2:
		ret_val = e1000e_copper_link_setup_igp(hw);
		break;
	default:
		return -E1000_ERR_PHY;
		break;
	}

	if (ret_val)
		return ret_val;

	return e1000e_setup_copper_link(hw);
}

static s32 e1000_setup_fiber_serdes_link_82571(struct e1000_hw *hw)
{
	switch (hw->mac.type) {
	case e1000_82571:
	case e1000_82572:
		ew32(SCTL, E1000_SCTL_DISABLE_SERDES_LOOPBACK);
		break;
	default:
		break;
	}

	return e1000e_setup_fiber_serdes_link(hw);
}

static s32 e1000_check_for_serdes_link_82571(struct e1000_hw *hw)
{
	struct e1000_mac_info *mac = &hw->mac;
	u32 rxcw;
	u32 ctrl;
	u32 status;
	u32 txcw;
	u32 i;
	s32 ret_val = 0;

	ctrl = er32(CTRL);
	status = er32(STATUS);
	rxcw = er32(RXCW);

	if ((rxcw & E1000_RXCW_SYNCH) && !(rxcw & E1000_RXCW_IV)) {

		
		switch (mac->serdes_link_state) {
		case e1000_serdes_link_autoneg_complete:
			if (!(status & E1000_STATUS_LU)) {
				mac->serdes_link_state =
				    e1000_serdes_link_autoneg_progress;
				mac->serdes_has_link = false;
				e_dbg("AN_UP     -> AN_PROG\n");
			} else {
				mac->serdes_has_link = true;
			}
			break;

		case e1000_serdes_link_forced_up:
			if ((rxcw & E1000_RXCW_C) || !(rxcw & E1000_RXCW_CW))  {
				
				ew32(TXCW, mac->txcw);
				ew32(CTRL, (ctrl & ~E1000_CTRL_SLU));
				mac->serdes_link_state =
				    e1000_serdes_link_autoneg_progress;
				mac->serdes_has_link = false;
				e_dbg("FORCED_UP -> AN_PROG\n");
			} else {
				mac->serdes_has_link = true;
			}
			break;

		case e1000_serdes_link_autoneg_progress:
			if (rxcw & E1000_RXCW_C) {
				if (status & E1000_STATUS_LU) {
					mac->serdes_link_state =
					    e1000_serdes_link_autoneg_complete;
					e_dbg("AN_PROG   -> AN_UP\n");
					mac->serdes_has_link = true;
				} else {
					
					mac->serdes_link_state =
					    e1000_serdes_link_down;
					e_dbg("AN_PROG   -> DOWN\n");
				}
			} else {
				ew32(TXCW, (mac->txcw & ~E1000_TXCW_ANE));
				ctrl |= (E1000_CTRL_SLU | E1000_CTRL_FD);
				ew32(CTRL, ctrl);

				
				ret_val = e1000e_config_fc_after_link_up(hw);
				if (ret_val) {
					e_dbg("Error config flow control\n");
					break;
				}
				mac->serdes_link_state =
				    e1000_serdes_link_forced_up;
				mac->serdes_has_link = true;
				e_dbg("AN_PROG   -> FORCED_UP\n");
			}
			break;

		case e1000_serdes_link_down:
		default:
			ew32(TXCW, mac->txcw);
			ew32(CTRL, (ctrl & ~E1000_CTRL_SLU));
			mac->serdes_link_state =
			    e1000_serdes_link_autoneg_progress;
			mac->serdes_has_link = false;
			e_dbg("DOWN      -> AN_PROG\n");
			break;
		}
	} else {
		if (!(rxcw & E1000_RXCW_SYNCH)) {
			mac->serdes_has_link = false;
			mac->serdes_link_state = e1000_serdes_link_down;
			e_dbg("ANYSTATE  -> DOWN\n");
		} else {
			for (i = 0; i < AN_RETRY_COUNT; i++) {
				udelay(10);
				rxcw = er32(RXCW);
				if ((rxcw & E1000_RXCW_IV) &&
				    !((rxcw & E1000_RXCW_SYNCH) &&
				      (rxcw & E1000_RXCW_C))) {
					mac->serdes_has_link = false;
					mac->serdes_link_state =
					    e1000_serdes_link_down;
					e_dbg("ANYSTATE  -> DOWN\n");
					break;
				}
			}

			if (i == AN_RETRY_COUNT) {
				txcw = er32(TXCW);
				txcw |= E1000_TXCW_ANE;
				ew32(TXCW, txcw);
				mac->serdes_link_state =
				    e1000_serdes_link_autoneg_progress;
				mac->serdes_has_link = false;
				e_dbg("ANYSTATE  -> AN_PROG\n");
			}
		}
	}

	return ret_val;
}

static s32 e1000_valid_led_default_82571(struct e1000_hw *hw, u16 *data)
{
	s32 ret_val;

	ret_val = e1000_read_nvm(hw, NVM_ID_LED_SETTINGS, 1, data);
	if (ret_val) {
		e_dbg("NVM Read Error\n");
		return ret_val;
	}

	switch (hw->mac.type) {
	case e1000_82573:
	case e1000_82574:
	case e1000_82583:
		if (*data == ID_LED_RESERVED_F746)
			*data = ID_LED_DEFAULT_82573;
		break;
	default:
		if (*data == ID_LED_RESERVED_0000 ||
		    *data == ID_LED_RESERVED_FFFF)
			*data = ID_LED_DEFAULT;
		break;
	}

	return 0;
}

bool e1000e_get_laa_state_82571(struct e1000_hw *hw)
{
	if (hw->mac.type != e1000_82571)
		return false;

	return hw->dev_spec.e82571.laa_is_present;
}

void e1000e_set_laa_state_82571(struct e1000_hw *hw, bool state)
{
	if (hw->mac.type != e1000_82571)
		return;

	hw->dev_spec.e82571.laa_is_present = state;

	
	if (state)
		e1000e_rar_set(hw, hw->mac.addr, hw->mac.rar_entry_count - 1);
}

static s32 e1000_fix_nvm_checksum_82571(struct e1000_hw *hw)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	s32 ret_val;
	u16 data;

	if (nvm->type != e1000_nvm_flash_hw)
		return 0;

	ret_val = e1000_read_nvm(hw, 0x10, 1, &data);
	if (ret_val)
		return ret_val;

	if (!(data & 0x10)) {
		ret_val = e1000_read_nvm(hw, 0x23, 1, &data);
		if (ret_val)
			return ret_val;

		if (!(data & 0x8000)) {
			data |= 0x8000;
			ret_val = e1000_write_nvm(hw, 0x23, 1, &data);
			if (ret_val)
				return ret_val;
			ret_val = e1000e_update_nvm_checksum(hw);
		}
	}

	return 0;
}

static s32 e1000_read_mac_addr_82571(struct e1000_hw *hw)
{
	if (hw->mac.type == e1000_82571) {
		s32 ret_val = 0;

		ret_val = e1000_check_alt_mac_addr_generic(hw);
		if (ret_val)
			return ret_val;
	}

	return e1000_read_mac_addr_generic(hw);
}

static void e1000_power_down_phy_copper_82571(struct e1000_hw *hw)
{
	struct e1000_phy_info *phy = &hw->phy;
	struct e1000_mac_info *mac = &hw->mac;

	if (!phy->ops.check_reset_block)
		return;

	
	if (!(mac->ops.check_mng_mode(hw) || phy->ops.check_reset_block(hw)))
		e1000_power_down_phy_copper(hw);
}

static void e1000_clear_hw_cntrs_82571(struct e1000_hw *hw)
{
	e1000e_clear_hw_cntrs_base(hw);

	er32(PRC64);
	er32(PRC127);
	er32(PRC255);
	er32(PRC511);
	er32(PRC1023);
	er32(PRC1522);
	er32(PTC64);
	er32(PTC127);
	er32(PTC255);
	er32(PTC511);
	er32(PTC1023);
	er32(PTC1522);

	er32(ALGNERRC);
	er32(RXERRC);
	er32(TNCRS);
	er32(CEXTERR);
	er32(TSCTC);
	er32(TSCTFC);

	er32(MGTPRC);
	er32(MGTPDC);
	er32(MGTPTC);

	er32(IAC);
	er32(ICRXOC);

	er32(ICRXPTC);
	er32(ICRXATC);
	er32(ICTXPTC);
	er32(ICTXATC);
	er32(ICTXQEC);
	er32(ICTXQMTC);
	er32(ICRXDMTC);
}

static const struct e1000_mac_operations e82571_mac_ops = {
	
	
	.id_led_init		= e1000e_id_led_init_generic,
	.cleanup_led		= e1000e_cleanup_led_generic,
	.clear_hw_cntrs		= e1000_clear_hw_cntrs_82571,
	.get_bus_info		= e1000e_get_bus_info_pcie,
	.set_lan_id		= e1000_set_lan_id_multi_port_pcie,
	
	
	.led_off		= e1000e_led_off_generic,
	.update_mc_addr_list	= e1000e_update_mc_addr_list_generic,
	.write_vfta		= e1000_write_vfta_generic,
	.clear_vfta		= e1000_clear_vfta_82571,
	.reset_hw		= e1000_reset_hw_82571,
	.init_hw		= e1000_init_hw_82571,
	.setup_link		= e1000_setup_link_82571,
	
	.setup_led		= e1000e_setup_led_generic,
	.config_collision_dist	= e1000e_config_collision_dist_generic,
	.read_mac_addr		= e1000_read_mac_addr_82571,
};

static const struct e1000_phy_operations e82_phy_ops_igp = {
	.acquire		= e1000_get_hw_semaphore_82571,
	.check_polarity		= e1000_check_polarity_igp,
	.check_reset_block	= e1000e_check_reset_block_generic,
	.commit			= NULL,
	.force_speed_duplex	= e1000e_phy_force_speed_duplex_igp,
	.get_cfg_done		= e1000_get_cfg_done_82571,
	.get_cable_length	= e1000e_get_cable_length_igp_2,
	.get_info		= e1000e_get_phy_info_igp,
	.read_reg		= e1000e_read_phy_reg_igp,
	.release		= e1000_put_hw_semaphore_82571,
	.reset			= e1000e_phy_hw_reset_generic,
	.set_d0_lplu_state	= e1000_set_d0_lplu_state_82571,
	.set_d3_lplu_state	= e1000e_set_d3_lplu_state,
	.write_reg		= e1000e_write_phy_reg_igp,
	.cfg_on_link_up      	= NULL,
};

static const struct e1000_phy_operations e82_phy_ops_m88 = {
	.acquire		= e1000_get_hw_semaphore_82571,
	.check_polarity		= e1000_check_polarity_m88,
	.check_reset_block	= e1000e_check_reset_block_generic,
	.commit			= e1000e_phy_sw_reset,
	.force_speed_duplex	= e1000e_phy_force_speed_duplex_m88,
	.get_cfg_done		= e1000e_get_cfg_done,
	.get_cable_length	= e1000e_get_cable_length_m88,
	.get_info		= e1000e_get_phy_info_m88,
	.read_reg		= e1000e_read_phy_reg_m88,
	.release		= e1000_put_hw_semaphore_82571,
	.reset			= e1000e_phy_hw_reset_generic,
	.set_d0_lplu_state	= e1000_set_d0_lplu_state_82571,
	.set_d3_lplu_state	= e1000e_set_d3_lplu_state,
	.write_reg		= e1000e_write_phy_reg_m88,
	.cfg_on_link_up      	= NULL,
};

static const struct e1000_phy_operations e82_phy_ops_bm = {
	.acquire		= e1000_get_hw_semaphore_82571,
	.check_polarity		= e1000_check_polarity_m88,
	.check_reset_block	= e1000e_check_reset_block_generic,
	.commit			= e1000e_phy_sw_reset,
	.force_speed_duplex	= e1000e_phy_force_speed_duplex_m88,
	.get_cfg_done		= e1000e_get_cfg_done,
	.get_cable_length	= e1000e_get_cable_length_m88,
	.get_info		= e1000e_get_phy_info_m88,
	.read_reg		= e1000e_read_phy_reg_bm2,
	.release		= e1000_put_hw_semaphore_82571,
	.reset			= e1000e_phy_hw_reset_generic,
	.set_d0_lplu_state	= e1000_set_d0_lplu_state_82571,
	.set_d3_lplu_state	= e1000e_set_d3_lplu_state,
	.write_reg		= e1000e_write_phy_reg_bm2,
	.cfg_on_link_up      	= NULL,
};

static const struct e1000_nvm_operations e82571_nvm_ops = {
	.acquire		= e1000_acquire_nvm_82571,
	.read			= e1000e_read_nvm_eerd,
	.release		= e1000_release_nvm_82571,
	.reload			= e1000e_reload_nvm_generic,
	.update			= e1000_update_nvm_checksum_82571,
	.valid_led_default	= e1000_valid_led_default_82571,
	.validate		= e1000_validate_nvm_checksum_82571,
	.write			= e1000_write_nvm_82571,
};

const struct e1000_info e1000_82571_info = {
	.mac			= e1000_82571,
	.flags			= FLAG_HAS_HW_VLAN_FILTER
				  | FLAG_HAS_JUMBO_FRAMES
				  | FLAG_HAS_WOL
				  | FLAG_APME_IN_CTRL3
				  | FLAG_HAS_CTRLEXT_ON_LOAD
				  | FLAG_HAS_SMART_POWER_DOWN
				  | FLAG_RESET_OVERWRITES_LAA 
				  | FLAG_TARC_SPEED_MODE_BIT 
				  | FLAG_APME_CHECK_PORT_B,
	.flags2			= FLAG2_DISABLE_ASPM_L1 
				  | FLAG2_DMA_BURST,
	.pba			= 38,
	.max_hw_frame_size	= DEFAULT_JUMBO,
	.get_variants		= e1000_get_variants_82571,
	.mac_ops		= &e82571_mac_ops,
	.phy_ops		= &e82_phy_ops_igp,
	.nvm_ops		= &e82571_nvm_ops,
};

const struct e1000_info e1000_82572_info = {
	.mac			= e1000_82572,
	.flags			= FLAG_HAS_HW_VLAN_FILTER
				  | FLAG_HAS_JUMBO_FRAMES
				  | FLAG_HAS_WOL
				  | FLAG_APME_IN_CTRL3
				  | FLAG_HAS_CTRLEXT_ON_LOAD
				  | FLAG_TARC_SPEED_MODE_BIT, 
	.flags2			= FLAG2_DISABLE_ASPM_L1 
				  | FLAG2_DMA_BURST,
	.pba			= 38,
	.max_hw_frame_size	= DEFAULT_JUMBO,
	.get_variants		= e1000_get_variants_82571,
	.mac_ops		= &e82571_mac_ops,
	.phy_ops		= &e82_phy_ops_igp,
	.nvm_ops		= &e82571_nvm_ops,
};

const struct e1000_info e1000_82573_info = {
	.mac			= e1000_82573,
	.flags			= FLAG_HAS_HW_VLAN_FILTER
				  | FLAG_HAS_WOL
				  | FLAG_APME_IN_CTRL3
				  | FLAG_HAS_SMART_POWER_DOWN
				  | FLAG_HAS_AMT
				  | FLAG_HAS_SWSM_ON_LOAD,
	.flags2			= FLAG2_DISABLE_ASPM_L1
				  | FLAG2_DISABLE_ASPM_L0S,
	.pba			= 20,
	.max_hw_frame_size	= ETH_FRAME_LEN + ETH_FCS_LEN,
	.get_variants		= e1000_get_variants_82571,
	.mac_ops		= &e82571_mac_ops,
	.phy_ops		= &e82_phy_ops_m88,
	.nvm_ops		= &e82571_nvm_ops,
};

const struct e1000_info e1000_82574_info = {
	.mac			= e1000_82574,
	.flags			= FLAG_HAS_HW_VLAN_FILTER
				  | FLAG_HAS_MSIX
				  | FLAG_HAS_JUMBO_FRAMES
				  | FLAG_HAS_WOL
				  | FLAG_APME_IN_CTRL3
				  | FLAG_HAS_SMART_POWER_DOWN
				  | FLAG_HAS_AMT
				  | FLAG_HAS_CTRLEXT_ON_LOAD,
	.flags2			  = FLAG2_CHECK_PHY_HANG
				  | FLAG2_DISABLE_ASPM_L0S
				  | FLAG2_NO_DISABLE_RX,
	.pba			= 32,
	.max_hw_frame_size	= DEFAULT_JUMBO,
	.get_variants		= e1000_get_variants_82571,
	.mac_ops		= &e82571_mac_ops,
	.phy_ops		= &e82_phy_ops_bm,
	.nvm_ops		= &e82571_nvm_ops,
};

const struct e1000_info e1000_82583_info = {
	.mac			= e1000_82583,
	.flags			= FLAG_HAS_HW_VLAN_FILTER
				  | FLAG_HAS_WOL
				  | FLAG_APME_IN_CTRL3
				  | FLAG_HAS_SMART_POWER_DOWN
				  | FLAG_HAS_AMT
				  | FLAG_HAS_JUMBO_FRAMES
				  | FLAG_HAS_CTRLEXT_ON_LOAD,
	.flags2			= FLAG2_DISABLE_ASPM_L0S
				  | FLAG2_NO_DISABLE_RX,
	.pba			= 32,
	.max_hw_frame_size	= DEFAULT_JUMBO,
	.get_variants		= e1000_get_variants_82571,
	.mac_ops		= &e82571_mac_ops,
	.phy_ops		= &e82_phy_ops_bm,
	.nvm_ops		= &e82571_nvm_ops,
};

